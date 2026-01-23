---
title: Cascade Optimizer(查询优化器)
---

# Cascade Optimizer(查询优化器)

优化器（Optimizer）是数据库中用于把逻辑计划转换成物理执行计划的核心组件，很大程度上决定了一个系统的性能。查询优化的本质就是对于给定的查询，找到该查询对应的正确物理执行计划并且是最低的"代价(cost)"。

在 MiniOB 中，查询优化器目前支持两种优化模式：基于规则的优化（RBO）和基于代价的优化（CBO）。两者都统一在 Cascade Optimizer 框架中实现，通过 `use_cascade` 配置项来选择使用哪种模式。本文主要介绍 Cascade Optimizer 的实现，其代码实现主要位于 `src/observer/sql/optimizer/cascade`。

## 基本概念

### Operator（算子）
描述某个特定的代数运算，例如join，aggregation 等，operator 分为 logical operator/physical operator。logical operator描述算子的逻辑定义，与具体实现无关。physical operator描述算子的具体实现算法。例如对 join 来说，logical operator 就是 join operator；physical operator 有hash join operator/nested loop join operator等。
在 MiniOB 中，operator 对应的抽象类为 `OperatorNode`。

### Expression（表达式）
这里的Expression（表达式）表示一个带有零或多个Input Expression（输入表达式）的Operator，同样可以分为Logical Expression（逻辑表达式）和 Physical Expression（物理表达式）。
例如对于 TableScan 算子来说，TableScan 算子本身就可以理解为是一个 Expression（不带输入表达式）；对于 Join 算子来说，Join 算子以及其子节点一起构成一个 Expression。

在 MiniOB 的 Cascade Optimizer 中，Expression 通过 `GroupExpr`（M_EXPR）来表示。`GroupExpr` 包含一个 `OperatorNode`（算子）和一组子 Group 的 ID（`child_group_ids`），而不是直接存储子 OperatorNode。这种设计使得多个等价的 Expression 可以共享相同的子 Group，从而避免重复计算。

### Group
Group 是一组逻辑等效的逻辑和物理表达式，产生相同的输出。
例如对于 `SELECT * FROM A JOIN B ON A.id = B.id JOIN C ON C.id = A.id;` 来说，其输出结果为 A，B，C 三个表根据 Join 条件进行 Join 的结果。逻辑表达式就可能包括 `(A Join B) Join C`，`A Join (B Join C)`；物理表达式就可能包括`(A Hash Join B) Nested Loop Join C`，`(A Hash Join B) Hash Join C`等。
在 MiniOB 中，Group 对应的实现位于 `src/observer/sql/optimizer/cascade/group.h`。

### M_EXPR (GroupExpr)
M_EXPR（在 MiniOB 中实现为 `GroupExpr`）是 Expression（表达式）的一种紧凑形式。M_EXPR 包含一个 Operator（算子）和一组子 Group 的 ID，而不是直接包含子 Expression。这种设计的优势在于：
- **共享子表达式**：多个等价的 Expression 可以共享相同的子 Group，避免重复计算
- **紧凑表示**：通过 Group ID 引用子节点，而不是直接存储子节点，节省内存
- **等价性检测**：通过 Group 机制，可以自动识别等价的表达式

在 Cascade 中，所有搜索都是通过 M_EXPR 完成的。在 MiniOB 中，M_EXPR 对应的实现位于 `src/observer/sql/optimizer/cascade/group_expr.h`。

### Rule（规则）
规则是将 Expression 转换为逻辑等价表达式。包含了Transformation Rule 和 Implementation Rule。其中，Transformation Rule 是将逻辑表达式转换为逻辑表达式；Implementation Rule 是将一个逻辑表达式转换为物理表达式。

**规则应用策略**：在 MiniOB 的 Cascade Optimizer 中，规则应用采用**全量应用**策略：
- **CBO 模式**：收集所有匹配的规则，为每个规则创建 `APPLY_RULE` task，所有匹配的规则都会被应用
- **RBO 模式**：遍历所有规则，对每个匹配的规则都进行应用，直到没有新的逻辑表达式生成

这种全量应用策略确保了所有可能的优化机会都被探索，从而生成更多的候选计划（CBO）或应用更多的优化规则（RBO）。

在 MiniOB 中，Rule 对应的实现位于 `src/observer/sql/optimizer/cascade/rules.h`。目前 MiniOB 已经实现了 Transformation Rule（如谓词下推、谓词重写、表达式简化）和 Implementation Rule（如逻辑算子到物理算子的转换）。

### Memo（在 Columbia 中也叫做 Search Space）
Memo 用于存放查询优化过程中所有枚举的计划的数据结构，利用 Memo 来避免生成重复的计划生成。
在 MiniOB 中，Memo 对应的实现位于 `src/observer/sql/optimizer/cascade/memo.h`。


## 搜索策略

在 Cascade 中，优化算法类似于DFS。我们有一个 task stack，每次循环都会从 task stack 中取一个 task 执行，每次执行可能导致新的任务被压入栈。这样不停循环直到栈为空，这样我们从根节点开始就能找到一个最优的物理计划。

其整体流程如下：
```
while task_list is not empty
  pop task
  perform task
```

在 MiniOB 的 Cascade Optimizer 中，task 共分为 6 种，根据优化模式（RBO 或 CBO）使用不同的 task：

**CBO 模式（Cost-Based Optimization）**：
使用异步任务调度，task 调度的流程图如下：
```
       optimize()                         
           │                              
           │                              
           ▼                              
    ┌─────────────┐        ┌─────────────┐
    │             │ ─────► │             │
    │   O_GROUP   │        │  O_INPUTS   │
    │             │ ◄────  │             │
    └──────┬──────┘        └─────────────┘
           │                      ▲       
           ▼                      │       
    ┌─────────────┐        ┌──────┴──────┐
    │             │ ─────► │             │
    │   O_EXPR    │        │ APPLY_RULE  │
    │             │ ◄───── │             │
    └─────┬───────┘        └─────────────┘
          │ ▲                             
          ▼ │                             
    ┌───────┴─────┐                       
    │             │                       
    │   E_GROUP   │                       
    │             │                       
    └─────────────┘    
```

**RBO 模式（Rule-Based Optimization）**：
使用同步递归，通过 `OPTIMIZE_RBO_GROUP` task 直接应用规则。

### Task 类型说明

* **O_GROUP**（CBO）：根据优化目标，对一个 Group 进行优化，即遍历 Group 中的每个 M_EXPR，为 logical M_EXPR 生成一个 O_EXPR task，为 physical M_EXPR 生成一个 O_INPUTS task。

* **O_EXPR**（CBO）：对一个 logical M_EXPR 进行优化，对于该 logical M_EXPR，如果存在可以应用的规则，则生成一个 APPLY_RULE task。

* **E_GROUP**（CBO）：为 Group 中的每一个 logical M_EXPR 生成一个 O_EXPR task。

* **APPLY_RULE**（CBO）：应用具体的优化规则，从逻辑表达式转换到逻辑表达式，或者从逻辑表达式转换为物理表达式。如果产生逻辑表达式，则生成一个 O_EXPR task；如果产生物理表达式，则生成一个 O_INPUTS task。

* **O_INPUTS**（CBO）：对物理表达式的代价进行计算，在此过程中需要递归地计算子节点的代价。

* **OPTIMIZE_RBO_GROUP**（RBO）：使用同步递归的方式，对 Group 应用 transformation rules 和 implementation rules。先应用 transformation rules 生成逻辑表达式，然后应用 implementation rules 生成物理表达式。不进行代价计算，只保留最后一个（最变换的）物理表达式。未来会跳过需要代价估算的规则（如 IndexScan、HashJoin 等）。

在 MiniOB 中，所有 task 类型位于 `src/observer/sql/optimizer/cascade/tasks` 目录下，Cascade Optimizer 的入口函数为 `src/observer/sql/optimizer/cascade/optimizer.h::Optimizer::optimize`。

### RBO 与 CBO 的区别

| 特性 | RBO | CBO |
|------|-----|-----|
| 任务调度 | 同步递归 | 异步任务调度 |
| 执行模式 | 先应用规则，再按需优化新节点 | 先应用规则，再按需优化新节点 |
| 候选探索 | 只保留最后一个（最变换的）逻辑/物理表达式 | 探索多个候选计划 |
| 代价计算 | 不计算代价 | 计算代价，选择最优 |
| 适用场景 | 简单查询，快速优化 | 复杂查询，需要精确代价估算 |

两者都使用相同的规则集（Transformation Rules 和 Implementation Rules）。目前 RBO 还没有区分哪些规则需要代价计算，但未来可能跳过依赖代价估算的规则（如 IndexScan、HashJoin 等），因为 RBO 不进行代价计算，无法判断这些规则生成的计划是否更优。

## 如何为 Cascade 添加新的算子转换规则

1. 添加逻辑算子和物理算子的定义，可参考`src/observer/sql/operator/table_get_logical_operator.h` 和 `src/observer/sql/operator/table_scan_physical_operator.h`
2. 添加逻辑算子到物理算子的转换规则，可参考`src/observer/sql/optimizer/implementation_rules.h::LogicalGetToPhysicalSeqScan`
3. 在 `src/observer/sql/optimizer/rules.h` 中的 `RuleSet` 中注册相应的转换规则。

## 优化模式选择

MiniOB 通过 `use_cascade` 配置项来选择优化模式：
- `use_cascade = true`：使用 CBO 模式，进行代价估算并选择最优计划
- `use_cascade = false`：使用 RBO 模式，按规则顺序应用，不进行代价估算

两种模式都统一在 Cascade Optimizer 框架中，使用相同的规则集和 Memo 结构，只是执行方式和候选选择策略不同。

## WIP
1. 实现 Apply Rule 中的 Expr binding。
2. 实现 property enforce。
3. 统计信息收集更多信息。
4. 优化 RBO 模式的规则应用顺序。
5. 在 RBO 模式中区分并跳过需要代价计算的规则（如 IndexScan、HashJoin 等）。

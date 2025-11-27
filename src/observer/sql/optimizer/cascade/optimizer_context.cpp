/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/optimizer_context.h"
#include "sql/optimizer/cascade/memo.h"
#include "sql/optimizer/cascade/rules.h"

OptimizerContext::OptimizerContext()
      : memo_(new Memo()), rule_set_(new RuleSet()), cost_model_(), task_pool_(nullptr),
        cost_upper_bound_(std::numeric_limits<double>::max()) {}

OptimizerContext::~OptimizerContext() {
    if (task_pool_ != nullptr) {
      delete task_pool_;
      task_pool_ = nullptr;
    }
    if (memo_ != nullptr) {
      delete memo_;
      memo_ = nullptr;
    }
    if (rule_set_ != nullptr) {
      delete rule_set_;
      rule_set_ = nullptr;
    }
  }

bool OptimizerContext::record_node_into_group(unique_ptr<OperatorNode> node, GroupExpr **gexpr,
                                  int target_group) {
  // 此方法仅用于没有子节点的叶子节点（如TableGet, Calc, Insert等）
  // 对于有子节点的情况，应该使用record_node_into_group(CandidateExpression &candidate, ...)
  // 注意：node的所有权会被转移到GroupExpr中
  // 如果节点已存在，memo_->insert_expression会删除new_gexpr，从而释放node
  std::vector<int> empty_child_groups;
  auto new_gexpr = new GroupExpr(std::move(node), std::move(empty_child_groups));
  auto ptr = memo_->insert_expression(new_gexpr, target_group);
  ASSERT(ptr, "Root of expr should not fail insertion");

  (*gexpr) = ptr;
  // 如果返回的ptr不是new_gexpr，说明节点已存在，new_gexpr会被删除，node也会被释放
  // 如果返回的ptr是new_gexpr，说明是新插入的，node的所有权已转移到GroupExpr
  return (ptr == new_gexpr);
}

bool OptimizerContext::record_node_into_group(CandidateExpression &candidate, GroupExpr **gexpr, int target_group) {
  auto new_gexpr = new GroupExpr(std::move(candidate.op), std::move(candidate.child_group_ids));
  auto ptr = memo_->insert_expression(new_gexpr, target_group);
  ASSERT(ptr, "Root of expr should not fail insertion");

  (*gexpr) = ptr;
  return (ptr == new_gexpr);
}

Memo &OptimizerContext::get_memo() { return *memo_; }

RuleSet &OptimizerContext::get_rule_set() { return *rule_set_; }

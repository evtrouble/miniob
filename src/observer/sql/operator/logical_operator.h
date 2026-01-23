/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/12/07.
//

#pragma once

#include "sql/expr/expression.h"
#include "sql/operator/operator_node.h"
#include "common/lang/unordered_set.h"

/**
 * @brief 逻辑算子
 * @defgroup LogicalOperator
 * @details 逻辑算子描述当前执行计划要做什么，比如从表中获取数据，过滤，投影，连接等等。
 * 物理算子会描述怎么做某件事情，这是与其不同的地方。
 */

/**
 * @brief 逻辑算子描述当前执行计划要做什么
 * @details 可以看OptimizeStage中相关的代码
 */
class LogicalOperator : public OperatorNode
{
public:
  LogicalOperator() = default;
  virtual ~LogicalOperator();

  bool is_physical() const override { return false; }
  bool is_logical() const override { return true; }

  void                                  add_expressions(unique_ptr<Expression> expr);
  auto                                  expressions() -> vector<unique_ptr<Expression>>                                  &{ return expressions_; }
  const vector<unique_ptr<Expression>> &expressions() const { return expressions_; }
  static bool                           can_generate_vectorized_operator(OpType type);

  /**
   * 这两个函数是为了打印时使用的，比如在explain中
   */
  virtual string name() const;
  virtual string param() const { return ""; }

  /**
   * @brief 克隆逻辑算子
   * @return 返回算子的一个深拷贝
   */
  virtual unique_ptr<LogicalOperator> clone() const = 0;

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<size_t>()(expressions_.size());
    for (const auto &expr : expressions_) {
      hash ^= std::hash<int>()(static_cast<int>(expr->type()));
    }
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_logi = static_cast<const LogicalOperator &>(other);
    if (expressions_.size() != other_logi.expressions_.size())
      return false;
    for (size_t i = 0; i < expressions_.size(); i++) {
      if (!expressions_[i]->equal(*(other_logi.expressions_[i])))
        return false;
    }
    return true;
  }

protected:
  ///< 表达式，比如select中的列，where中的谓词等等，都可以使用表达式来表示
  ///< 表达式能是一个常量，也可以是一个函数，也可以是一个列，也可以是一个子查询等等
  vector<unique_ptr<Expression>> expressions_;
};

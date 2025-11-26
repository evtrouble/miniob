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
// Created by WangYunlai on 2024/05/29.
//

#pragma once

#include "sql/operator/logical_operator.h"

class GroupByLogicalOperator : public LogicalOperator
{
public:
  GroupByLogicalOperator(vector<unique_ptr<Expression>> &&group_by_exprs, vector<Expression *> &&expressions);

  virtual ~GroupByLogicalOperator() = default;

  OpType              get_op_type() const override { return OpType::LOGICALGROUPBY; }

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<size_t>()(group_by_expressions_.size());
    hash ^= std::hash<size_t>()(aggregate_expressions_.size());
    for (const auto &expr : group_by_expressions_) {
      hash ^= std::hash<int>()(static_cast<int>(expr->type()));
    }
    for (const auto &expr : aggregate_expressions_) {
      hash ^= std::hash<int>()(static_cast<int>(expr->type()));
    }
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_gb = static_cast<const GroupByLogicalOperator &>(other);
    if (group_by_expressions_.size() != other_gb.group_by_expressions_.size())
      return false;
    if (aggregate_expressions_.size() != other_gb.aggregate_expressions_.size())
      return false;
    for (size_t i = 0; i < group_by_expressions_.size(); i++) {
      if (!group_by_expressions_[i]->equal(*(other_gb.group_by_expressions_[i])))
        return false;
    }
    for (size_t i = 0; i < aggregate_expressions_.size(); i++) {
      if (aggregate_expressions_[i] != other_gb.aggregate_expressions_[i]) {
        // 对于 Expression*，比较指针是否相同
        // 如果需要深度比较，可以使用 equal，但这里 Expression* 可能指向同一个对象
        return false;
      }
    }
    return true;
  }

  auto &group_by_expressions() { return group_by_expressions_; }
  auto &aggregate_expressions() { return aggregate_expressions_; }

private:
  vector<unique_ptr<Expression>> &group_by_expressions_ = expressions_;
  vector<Expression *>           aggregate_expressions_;  ///< 输出的表达式，可能包含聚合函数
};

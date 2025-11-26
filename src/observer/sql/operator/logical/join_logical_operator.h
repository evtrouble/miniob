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
// Created by Wangyunlai on 2022/12/07
//

#pragma once

#include "sql/operator/logical_operator.h"

/**
 * @brief 连接算子
 * @ingroup LogicalOperator
 * @details 连接算子，用于连接两个表。对应的物理算子或者实现，可能有NestedLoopJoin，HashJoin等等。
 */
class JoinLogicalOperator : public LogicalOperator
{
public:
  JoinLogicalOperator()          = default;
  virtual ~JoinLogicalOperator() = default;

  void                add_predicate_op(LogicalOperator *predicate_op) { predicate_op_ = predicate_op; }
  auto                predicates() -> Expression *
  {
    if (predicate_op_ != nullptr && predicate_op_->expressions().size() == 1) {
      return predicate_op_->expressions()[0].get();
    }
    return nullptr;
  }

  OpType get_op_type() const override { return OpType::LOGICALINNERJOIN; }

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<size_t>()(join_predicates_.size());
    for (const auto &pred : join_predicates_) {
      hash ^= std::hash<int>()(static_cast<int>(pred->type()));
    }
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_join = static_cast<const JoinLogicalOperator &>(other);
    if (join_predicates_.size() != other_join.join_predicates_.size())
      return false;
    for (size_t i = 0; i < join_predicates_.size(); i++) {
      if (!join_predicates_[i]->equal(*(other_join.join_predicates_[i])))
        return false;
    }
    return true;
  }

  vector<unique_ptr<Expression>> &get_join_predicates() { return join_predicates_; }

  void clear_join_predicates() { join_predicates_.clear(); }

  auto add_join_predicate(unique_ptr<Expression> &&predicate) { join_predicates_.push_back(std::move(predicate)); }

  unique_ptr<LogicalProperty> find_log_prop(const vector<LogicalProperty *> &log_props) override
  {
    if (log_props.size() != 2) {
      return nullptr;
    }

    LogicalProperty *left_log_prop  = log_props[0];
    LogicalProperty *right_log_prop = log_props[1];
    int              card           = left_log_prop->get_card() * right_log_prop->get_card();
    for (auto &predicate : join_predicates_) {
      if (predicate->type() != ExprType::COMPARISON) {
        continue;
      }
      auto  pred_expr = dynamic_cast<ComparisonExpr *>(predicate.get());
      auto &left      = pred_expr->left();
      auto &right     = pred_expr->right();
      if (pred_expr->comp() == CompOp::EQUAL_TO && left->type() == ExprType::FIELD &&
          right->type() == ExprType::FIELD) {
        card /= std::max(std::max(left_log_prop->get_card(), right_log_prop->get_card()), 1);
      }
    }
    return make_unique<LogicalProperty>(card);
  }

private:
  LogicalOperator                    *predicate_op_ = nullptr;
  std::vector<unique_ptr<Expression>> &join_predicates_ = expressions_;
};

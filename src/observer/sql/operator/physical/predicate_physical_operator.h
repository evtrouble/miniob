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
// Created by WangYunlai on 2022/6/27.
//

#pragma once

#include "sql/expr/expression.h"
#include "sql/operator/physical_operator.h"

class FilterStmt;

/**
 * @brief 过滤/谓词物理算子
 * @ingroup PhysicalOperator
 */
class PredicatePhysicalOperator : public PhysicalOperator
{
public:
  PredicatePhysicalOperator(unique_ptr<Expression> expr);

  virtual ~PredicatePhysicalOperator() = default;

  OpType               get_op_type() const override { return OpType::FILTER; }

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    if (expression_) {
      hash ^= std::hash<int>()(static_cast<int>(expression_->type()));
    }
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_pred = static_cast<const PredicatePhysicalOperator &>(other);
    if (expression_ == nullptr && other_pred.expression_ == nullptr)
      return true;
    if (expression_ == nullptr || other_pred.expression_ == nullptr)
      return false;
    return expression_->equal(*(other_pred.expression_));
  }

  double calculate_cost(LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm) override
  {
    // Predicate 需要处理所有输入行，代价与输入 cardinality 成正比
    // 这比在 TableScan 中直接过滤要昂贵，因为需要额外的 CPU 开销
    if (child_log_props.empty() || !child_log_props[0]) {
      return cm->cpu_op() * 1;  // Minimum cost even if no child
    }
    int card = child_log_props[0]->get_card();
    if (card == 0) {
      card = 1;  // Use minimum cardinality of 1 for cost calculation
    }
    // Calculate expression complexity to adjust cost
    double expr_complexity = cm->calculate_expression_complexity(expression_.get());
    if (expr_complexity < 1.0) {
      expr_complexity = 1.0;  // Minimum complexity multiplier
    }
    // Predicate 的代价：需要扫描所有输入行并评估表达式
    // 表达式复杂度越高，代价越高
    return cm->cpu_op() * card * expr_complexity;
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

  RC tuple_schema(TupleSchema &schema) const override;

  string param() const override;

private:
  unique_ptr<Expression> expression_;
};

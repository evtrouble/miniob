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
// Created by WangYunlai on 2022/07/01.
//

#pragma once

#include "sql/operator/physical_operator.h"
#include "sql/expr/expression_tuple.h"

/**
 * @brief 选择/投影物理算子
 * @ingroup PhysicalOperator
 */
class ProjectPhysicalOperator : public PhysicalOperator
{
public:
  ProjectPhysicalOperator(vector<unique_ptr<Expression>> &&expressions);

  virtual ~ProjectPhysicalOperator() = default;

  OpType               get_op_type() const override { return OpType::PROJECTION; }

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
    const auto &other_proj = static_cast<const ProjectPhysicalOperator &>(other);
    if (expressions_.size() != other_proj.expressions_.size())
      return false;
    for (size_t i = 0; i < expressions_.size(); i++) {
      if (!expressions_[i]->equal(*(other_proj.expressions_[i])))
        return false;
    }
    return true;
  }

  virtual double calculate_cost(
      LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm) override
  {
    return (cm->cpu_op()) * prop->get_card();
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  int cell_num() const { return tuple_.cell_num(); }

  Tuple *current_tuple() override;

  RC tuple_schema(TupleSchema &schema) const override;

private:
  vector<unique_ptr<Expression>>          expressions_;
  ExpressionTuple<unique_ptr<Expression>> tuple_;
};

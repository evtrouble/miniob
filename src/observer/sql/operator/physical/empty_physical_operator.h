/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "sql/operator/physical_operator.h"

/**
 * @brief Empty物理算子
 * @ingroup PhysicalOperator
 * @details 用于表示一个空的物理算子，不产生任何数据
 */
class EmptyPhysicalOperator : public PhysicalOperator
{
public:
  EmptyPhysicalOperator()          = default;
  virtual ~EmptyPhysicalOperator() = default;

  OpType get_op_type() const override { return OpType::EMPTY; }

  double calculate_cost(LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm) override
  {
    return 0.0;
  }

  RC open(Trx *trx) override { return RC::SUCCESS; }

  RC next() override { return RC::RECORD_EOF; }

  RC next(Chunk &chunk) override { return RC::RECORD_EOF; }

  RC close() override { return RC::SUCCESS; }

  Tuple *current_tuple() override { return nullptr; }

  RC tuple_schema(TupleSchema &schema) const override { return RC::SUCCESS; }
};

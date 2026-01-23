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
// Created by WangYunlai on 2021/6/7.
//

#pragma once

#include "sql/operator/physical_operator.h"
#include "sql/parser/parse.h"

class InsertStmt;

/**
 * @brief 插入物理算子
 * @ingroup PhysicalOperator
 */
class InsertPhysicalOperator : public PhysicalOperator
{
public:
  InsertPhysicalOperator(Table *table, vector<Value> &&values);

  virtual ~InsertPhysicalOperator() = default;

  OpType get_op_type() const override { return OpType::INSERT; }

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<int>()(table_->table_id());
    hash ^= std::hash<size_t>()(values_.size());
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_insert = static_cast<const InsertPhysicalOperator &>(other);
    if (table_->table_id() != other_insert.table()->table_id())
      return false;
    if (values_.size() != other_insert.values_.size())
      return false;
    for (size_t i = 0; i < values_.size(); i++) {
      if (values_[i].compare(other_insert.values_[i]) != 0)
        return false;
    }
    return true;
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Table *table() const { return table_; }

  Tuple *current_tuple() override { return nullptr; }

private:
  Table        *table_ = nullptr;
  vector<Value> values_;
};

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
// Created by WangYunlai on 2023/4/25.
//

#pragma once

#include "sql/operator/logical_operator.h"
#include "sql/parser/parse_defs.h"

/**
 * @brief 插入逻辑算子
 * @ingroup LogicalOperator
 */
class InsertLogicalOperator : public LogicalOperator
{
public:
  InsertLogicalOperator(Table *table, vector<Value> values);
  virtual ~InsertLogicalOperator() = default;

  OpType get_op_type() const override { return OpType::LOGICALINSERT; }

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
    const auto &other_insert = static_cast<const InsertLogicalOperator &>(other);
    if (table_->table_id() != other_insert.table()->table_id())
      return false;
    if (values_.size() != other_insert.values().size())
      return false;
    // 比较 values（Value 使用 compare 方法）
    for (size_t i = 0; i < values_.size(); i++) {
      if (values_[i].compare(other_insert.values()[i]) != 0)
        return false;
    }
    return true;
  }

  Table               *table() const { return table_; }
  const vector<Value> &values() const { return values_; }
  vector<Value>       &values() { return values_; }

  unique_ptr<LogicalOperator> clone() const override { return make_unique<InsertLogicalOperator>(table_, values_); }

private:
  Table        *table_ = nullptr;
  vector<Value> values_;
};

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
// Created by WangYunlai on 2022/6/7.
//

#pragma once

#include "common/sys/rc.h"
#include "sql/operator/physical_operator.h"
#include "storage/record/record_manager.h"
#include "storage/record/record_scanner.h"
#include "common/types.h"

class Table;

/**
 * @brief 表扫描物理算子
 * @ingroup PhysicalOperator
 */
class TableScanPhysicalOperator : public PhysicalOperator
{
public:
  TableScanPhysicalOperator(Table *table, ReadWriteMode mode) : table_(table), mode_(mode) {}

  virtual ~TableScanPhysicalOperator() = default;

  string param() const override;

  OpType           get_op_type() const override { return OpType::SEQSCAN; }
  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<int>()(table_->table_id());
    hash ^= std::hash<size_t>()(predicates_.size());
    for (const auto &pred : predicates_) {
      if (pred) {
        hash ^= std::hash<int>()(static_cast<int>(pred->type()));
      }
    }
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_get = static_cast<const TableScanPhysicalOperator &>(other);
    if (table_->table_id() != other_get.table_id())
      return false;
    if (predicates_.size() != other_get.predicates_.size())
      return false;
    for (size_t i = 0; i < predicates_.size(); i++) {
      if (!predicates_[i]->equal(*(other_get.predicates_[i])))
        return false;
    }
    return true;
  }

  double calculate_cost(LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm) override
  {
    // 表扫描的代价计算：
    // 1. IO 成本：需要扫描整个表（所有行），而不是只扫描匹配的行
    // 2. CPU 成本：处理所有行并过滤（过滤后的行数）
    // 表扫描需要扫描整个表，所以 IO 成本应该基于全表的行数
    // 但为了简化，我们使用 prop 中的基数（过滤后的行数）作为近似
    // TODO: 应该从 Catalog 获取全表的行数来计算 IO 成本
    int card = prop ? prop->get_card() : 0;
    if (card == 0) {
      card = 1;  // Use minimum cardinality of 1 for cost calculation
    }

    // 表扫描需要扫描整个表，IO 成本应该基于全表行数
    // 但当前 prop 中的 card 是过滤后的基数，我们假设全表行数 = card * 10（简单估计）
    // 实际上应该从 Catalog 获取准确的表统计信息
    int full_table_rows = card * 10;  // 简单估计：假设过滤后保留 10% 的数据
    if (full_table_rows < card) {
      full_table_rows = card;  // 至少等于过滤后的基数
    }

    // 表扫描的代价 = 扫描全表的 IO 成本 + 处理所有行的 CPU 成本
    // 注意：表扫描需要扫描所有行，所以 IO 成本基于全表行数
    double io_cost = cm->io() * full_table_rows;
    // CPU 成本：处理所有行（包括过滤）
    double cpu_cost = cm->cpu_op() * full_table_rows;

    return io_cost + cpu_cost;
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

  int table_id() const { return table_->table_id(); }

  void set_predicates(vector<unique_ptr<Expression>> &&exprs);

private:
  RC filter(RowTuple &tuple, bool &result);

private:
  Table                         *table_ = nullptr;
  Trx                           *trx_   = nullptr;
  ReadWriteMode                  mode_  = ReadWriteMode::READ_WRITE;
  RecordScanner                 *record_scanner_;
  Record                         current_record_;
  RowTuple                       tuple_;
  vector<unique_ptr<Expression>> predicates_;  // TODO chang predicate to table tuple filter
};

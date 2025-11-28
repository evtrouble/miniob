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
// Created by Wangyunlai on 2022/07/08.
//

#pragma once

#include "sql/expr/tuple.h"
#include "sql/operator/physical_operator.h"
#include "sql/optimizer/cascade/cost_model.h"
#include "sql/optimizer/cascade/property.h"
#include "storage/record/record_manager.h"

/**
 * @brief 索引扫描物理算子
 * @ingroup PhysicalOperator
 */
class IndexScanPhysicalOperator : public PhysicalOperator
{
public:
  IndexScanPhysicalOperator(Table *table, Index *index, ReadWriteMode mode, const Value *left_value,
      bool left_inclusive, const Value *right_value, bool right_inclusive);

  virtual ~IndexScanPhysicalOperator() = default;

  OpType get_op_type() const override { return OpType::INDEXSCAN; }

  virtual uint64_t hash() const override
  {
    uint64_t hash = std::hash<int>()(static_cast<int>(get_op_type()));
    hash ^= std::hash<int>()(table_->table_id());
    if (index_) {
      hash ^= std::hash<const void *>()(index_);
    }
    hash ^= std::hash<size_t>()(predicates_.size());
    hash ^= std::hash<bool>()(left_inclusive_);
    hash ^= std::hash<bool>()(right_inclusive_);
    return hash;
  }

  virtual bool operator==(const OperatorNode &other) const override
  {
    if (get_op_type() != other.get_op_type())
      return false;
    const auto &other_scan = static_cast<const IndexScanPhysicalOperator &>(other);
    if (table_->table_id() != other_scan.table_->table_id())
      return false;
    if (index_ != other_scan.index_)
      return false;
    if (left_inclusive_ != other_scan.left_inclusive_ || right_inclusive_ != other_scan.right_inclusive_)
      return false;
    if (predicates_.size() != other_scan.predicates_.size())
      return false;
    for (size_t i = 0; i < predicates_.size(); i++) {
      if (!predicates_[i]->equal(*(other_scan.predicates_[i])))
        return false;
    }
    // 比较 left_value_ 和 right_value_
    if (left_value_.compare(other_scan.left_value_) != 0)
      return false;
    if (right_value_.compare(other_scan.right_value_) != 0)
      return false;
    return true;
  }

  string param() const override;

  double calculate_cost(LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm) override
  {
    // 索引扫描的代价计算：
    // 1. 索引查找成本：index_probe（查找索引，固定成本，很小）
    // 2. IO 成本：访问匹配的行（只访问匹配的行，不需要扫描整个表）
    // 3. CPU 成本：处理匹配的行
    int card = prop ? prop->get_card() : 0;
    if (card == 0) {
      card = 1;  // Use minimum cardinality of 1 for cost calculation
    }

    // 索引扫描的代价 = 索引查找成本 + IO * 匹配行数 + CPU * 匹配行数
    // 索引扫描的优势：
    // - 基数更小（只访问匹配的行，而不是全表扫描）
    // - IO 成本更低（不需要扫描整个表，只需要访问匹配的行）
    //   索引扫描的 IO 成本应该比表扫描低，因为：
    //   - 表扫描需要扫描整个表（所有行）
    //   - 索引扫描只需要访问匹配的行（基数更小）
    double index_lookup_cost = cm->index_probe();  // 索引查找的固定成本（很小，0.00001）
    // 索引扫描的 IO 成本：只访问匹配的行，不需要扫描整个表
    // 假设每个匹配行需要一次 IO 访问（实际上可能更少，因为索引可以批量访问）
    double io_cost = cm->io() * card;
    // CPU 成本：处理匹配的行
    double cpu_cost = cm->cpu_op() * card;

    return index_lookup_cost + io_cost + cpu_cost;
  }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override;

  void set_predicates(vector<unique_ptr<Expression>> &&exprs);

private:
  // 与TableScanPhysicalOperator代码相同，可以优化
  RC filter(RowTuple &tuple, bool &result);

private:
  Trx          *trx_           = nullptr;
  Table        *table_         = nullptr;
  Index        *index_         = nullptr;
  ReadWriteMode mode_          = ReadWriteMode::READ_WRITE;
  IndexScanner *index_scanner_ = nullptr;

  Record   current_record_;
  RowTuple tuple_;

  Value left_value_;
  Value right_value_;
  bool  left_inclusive_  = false;
  bool  right_inclusive_ = false;

  vector<unique_ptr<Expression>> predicates_;
};

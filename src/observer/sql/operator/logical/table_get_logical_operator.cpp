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
// Created by Wangyunlai on 2022/12/15
//

#include "sql/operator/logical/table_get_logical_operator.h"
#include "sql/optimizer/cascade/property.h"
#include "catalog/catalog.h"

TableGetLogicalOperator::TableGetLogicalOperator(Table *table, ReadWriteMode mode)
    : LogicalOperator(), table_(table), mode_(mode)
{}

void TableGetLogicalOperator::set_predicates(vector<unique_ptr<Expression>> &&exprs)
{
  predicates_ = std::move(exprs);
}

unique_ptr<LogicalProperty> TableGetLogicalOperator::find_log_prop(const vector<LogicalProperty*> &log_props)
{
  int card = Catalog::get_instance().get_table_stats(table_->table_id()).row_nums;
  
  // 如果有 predicates，降低 cardinality
  // 简单估计：每个谓词会减少约 10% 的行数
  // TODO: 根据谓词类型和选择性来更准确地估计 cardinality
  if (!predicates_.empty()) {
    // 假设每个谓词过滤掉 90% 的数据，保留 10%
    for (size_t i = 0; i < predicates_.size(); i++) {
      card = card / 10;
      if (card < 1) {
        card = 1;  // 至少保留 1 行
      }
    }
  }
  
  return make_unique<LogicalProperty>(card);
}
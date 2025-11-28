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
// Created by Wangyunlai on 2022/12/13.
//

#include "sql/operator/logical/predicate_logical_operator.h"
#include "sql/optimizer/cascade/property.h"

PredicateLogicalOperator::PredicateLogicalOperator(unique_ptr<Expression> expression)
{
  expressions_.emplace_back(std::move(expression));
}

unique_ptr<LogicalProperty> PredicateLogicalOperator::find_log_prop(const vector<LogicalProperty *> &log_props)
{
  if (log_props.empty() || log_props[0] == nullptr) {
    // 如果没有输入属性，返回默认值
    return make_unique<LogicalProperty>(1000);
  }
  
  // 获取输入 cardinality
  int input_card = log_props[0]->get_card();
  
  // 简单估计：谓词过滤会减少约 10% 的行数（实际应该根据谓词类型和选择性来估计）
  // TODO: 根据谓词类型和选择性来更准确地估计 cardinality
  int output_card = input_card / 10;  // 假设过滤后保留 10% 的数据
  if (output_card < 1) {
    output_card = 1;  // 至少保留 1 行
  }
  
  return make_unique<LogicalProperty>(output_card);
}

unique_ptr<LogicalOperator> PredicateLogicalOperator::clone() const
{
  if (expressions_.empty()) {
    return nullptr;
  }
  return make_unique<PredicateLogicalOperator>(expressions_[0]->copy());
}

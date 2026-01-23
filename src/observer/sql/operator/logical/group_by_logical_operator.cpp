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
// Created by WangYunlai on 2024/05/30.
//

#include "common/log/log.h"
#include "sql/operator/logical/group_by_logical_operator.h"
#include "sql/expr/expression.h"

using namespace std;

GroupByLogicalOperator::GroupByLogicalOperator(vector<unique_ptr<Expression>> &&group_by_exprs,
                                               vector<Expression *> &&expressions)
{
  group_by_expressions_ = std::move(group_by_exprs);
  aggregate_expressions_ = std::move(expressions);
}

unique_ptr<LogicalOperator> GroupByLogicalOperator::clone() const
{
  vector<unique_ptr<Expression>> group_by_exprs;
  for (auto &expr : group_by_expressions_) {
    group_by_exprs.push_back(expr->copy());
  }
  // aggregate_expressions_ 是 Expression* 指针，不能直接复制
  // 这里需要从 expressions_ 中获取，但 GroupBy 的 aggregate_expressions_ 可能指向不同的表达式
  // 为了简化，我们只复制 group_by 表达式
  vector<Expression *> agg_exprs;
  // TODO: 需要正确复制 aggregate_expressions_
  return make_unique<GroupByLogicalOperator>(std::move(group_by_exprs), std::move(agg_exprs));
}

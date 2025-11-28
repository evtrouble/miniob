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

#include "sql/optimizer/cascade/rules.h"

class Expression;

/**
 * Rule transforms Filter(TableGet) -> TableGet with predicates
 * 谓词下推：将 Filter 中的谓词下推到 TableGet 算子中
 */
class PredicatePushdownRule : public Rule
{
public:
  PredicatePushdownRule();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Filter(Filter(...)) -> Filter(...) or removes Filter
 * 谓词重写：删除恒真/恒假的谓词
 */
class PredicateRewriteRule : public Rule
{
public:
  PredicateRewriteRule();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule simplifies expressions in logical operators
 * 表达式简化：简化比较表达式和联结表达式
 */
class ExpressionSimplifyRule : public Rule
{
public:
  ExpressionSimplifyRule();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;

private:
  // 简化表达式，返回是否发生变化
  bool simplify_expression(unique_ptr<Expression> &expr) const;
};

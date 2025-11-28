/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/transformation_rules.h"
#include "common/log/log.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/cascade/memo.h"
#include "sql/operator/logical/predicate_logical_operator.h"
#include "sql/operator/logical/table_get_logical_operator.h"
#include "sql/operator/logical/empty_logical_operator.h"
#include "sql/expr/expression.h"

// -------------------------------------------------------------------------------------------------
// PredicatePushdownRule
// -------------------------------------------------------------------------------------------------
PredicatePushdownRule::PredicatePushdownRule()
{
  type_          = RuleType::PREDICATE_PUSHDOWN;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALFILTER));
  auto child     = new Pattern(OpType::LOGICALGET);
  match_pattern_->add_child(child);
}

void PredicatePushdownRule::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  ASSERT(input->get_children_groups_size() == 1, "Filter should have 1 child");

  auto predicate_oper = static_cast<PredicateLogicalOperator *>(input->get_op());
  vector<unique_ptr<Expression>> &expressions = predicate_oper->expressions();
  if (expressions.size() != 1) {
    return;
  }

  // 获取子 GroupExpr
  Memo &memo = context->get_memo();
  Group *child_group = memo.get_group_by_id(input->get_child_group_ids()[0]);
  GroupExpr *child_gexpr = child_group->get_logical_expression();
  if (!child_gexpr || child_gexpr->get_op()->get_op_type() != OpType::LOGICALGET) {
    return;
  }

  auto table_get_oper = static_cast<TableGetLogicalOperator *>(child_gexpr->get_op());

  unique_ptr<Expression> &predicate_expr = expressions.front();
  vector<unique_ptr<Expression>> pushdown_exprs;

  // 提取可以下推的表达式
  if (predicate_expr->type() == ExprType::CONJUNCTION) {
    ConjunctionExpr *conjunction_expr = static_cast<ConjunctionExpr *>(predicate_expr.get());
    if (conjunction_expr->conjunction_type() == ConjunctionExpr::Type::OR) {
      // OR 操作太复杂，暂不支持
      return;
    }

    vector<unique_ptr<Expression>> &child_exprs = conjunction_expr->children();
    for (auto &child_expr : child_exprs) {
      pushdown_exprs.push_back(child_expr->copy());
    }
  } else if (predicate_expr->type() == ExprType::COMPARISON) {
    // 检查是否为恒真表达式
    pushdown_exprs.push_back(predicate_expr->copy());
  }

  if (pushdown_exprs.empty()) {
    return;
  }

  // 创建新的 TableGetLogicalOperator，包含下推的谓词
  // 合并原有的 predicates 和下推的 predicates
  for (auto &expr : table_get_oper->predicates()) {
    pushdown_exprs.push_back(expr->copy());
  }
  
  auto new_table_get = make_unique<TableGetLogicalOperator>(
      table_get_oper->table(), table_get_oper->read_write_mode());
  new_table_get->set_predicates(std::move(pushdown_exprs));

  // 返回新的 TableGet，直接替换 Filter（TableGet 是叶子节点，没有子节点）
  transformed->emplace_back(std::move(new_table_get));
}

// -------------------------------------------------------------------------------------------------
// PredicateRewriteRule
// -------------------------------------------------------------------------------------------------
PredicateRewriteRule::PredicateRewriteRule()
{
  type_          = RuleType::PREDICATE_REWRITE;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALFILTER));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void PredicateRewriteRule::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  ASSERT(input->get_children_groups_size() == 1, "Filter should have 1 child");

  auto predicate_oper = static_cast<PredicateLogicalOperator *>(input->get_op());
  vector<unique_ptr<Expression>> &expressions = predicate_oper->expressions();
  if (expressions.size() != 1) {
    return;
  }

  unique_ptr<Expression> &expr = expressions.front();
  if (expr->type() != ExprType::VALUE) {
    return;
  }

  // 检查是否为恒真或恒假
  auto value_expr = static_cast<ValueExpr *>(expr.get());
  bool bool_value = value_expr->get_value().get_boolean();

  if (bool_value == true) {
    // 恒真：删除 Filter，直接返回子节点
    Memo &memo = context->get_memo();
    memo.make_alias(input->get_group_id(), input->get_child_group_ids()[0]);
  } else {
    // 在 Cascade 中，这应该返回一个空算子
    transformed->emplace_back(std::unique_ptr<OperatorNode>(new EmptyLogicalOperator));
    return;
  }
}

// -------------------------------------------------------------------------------------------------
// ExpressionSimplifyRule
// -------------------------------------------------------------------------------------------------
ExpressionSimplifyRule::ExpressionSimplifyRule()
{
  type_          = RuleType::EXPRESSION_SIMPLIFY;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALFILTER));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void ExpressionSimplifyRule::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto predicate_oper = static_cast<PredicateLogicalOperator *>(input->get_op());
  vector<unique_ptr<Expression>> &expressions = predicate_oper->expressions();
  
  bool changed = false;
  vector<unique_ptr<Expression>> new_expressions;
  
  for (auto &expr : expressions) {
    unique_ptr<Expression> new_expr = expr->copy();
    if (simplify_expression(new_expr)) {
      changed = true;
    }
    new_expressions.push_back(std::move(new_expr));
  }

  if (changed && !new_expressions.empty()) {
    auto new_predicate = make_unique<PredicateLogicalOperator>(std::move(new_expressions.front()));
    transformed->emplace_back(std::move(new_predicate), input->get_child_group_ids());
  }
}

bool ExpressionSimplifyRule::simplify_expression(unique_ptr<Expression> &expr) const
{
  bool changed = false;

  // 简化比较表达式
  if (expr->type() == ExprType::COMPARISON) {
    Value value;
    ComparisonExpr *cmp_expr = static_cast<ComparisonExpr *>(expr.get());
    if (cmp_expr->try_get_value(value) == RC::SUCCESS) {
      expr = make_unique<ValueExpr>(value);
      changed = true;
    }
  }

  // 简化联结表达式
  if (expr->type() == ExprType::CONJUNCTION) {
    auto conjunction_expr = static_cast<ConjunctionExpr *>(expr.get());
    vector<unique_ptr<Expression>> &child_exprs = conjunction_expr->children();

    // 先简化子表达式
    for (auto &child_expr : child_exprs) {
      if (simplify_expression(child_expr)) {
        changed = true;
      }
    }

    // 检查是否有可以删除的常量表达式
    for (auto iter = child_exprs.begin(); iter != child_exprs.end();) {
      bool constant_value = false;
      if ((*iter)->type() == ExprType::VALUE && (*iter)->value_type() == AttrType::BOOLEANS) {
        auto value_expr = static_cast<ValueExpr *>(iter->get());
        constant_value = value_expr->get_value().get_boolean();

        if (conjunction_expr->conjunction_type() == ConjunctionExpr::Type::AND) {
          if (constant_value == true) {
            iter = child_exprs.erase(iter);
            changed = true;
            continue;
          } else {
            // always be false
            expr = make_unique<ValueExpr>(Value((bool)false));
            changed = true;
            return changed;
          }
        } else {
          // OR
          if (constant_value == true) {
            // always be true
            expr = make_unique<ValueExpr>(Value((bool)true));
            changed = true;
            return changed;
          } else {
            iter = child_exprs.erase(iter);
            changed = true;
            continue;
          }
        }
      }
      ++iter;
    }

    // 如果只有一个子表达式，直接替换
    if (child_exprs.size() == 1) {
      expr = std::move(child_exprs.front());
      changed = true;
    }
  }

  return changed;
}


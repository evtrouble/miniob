/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/cost_model.h"
#include "sql/optimizer/cascade/memo.h"
#include "catalog/catalog.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "common/log/log.h"
#include "sql/expr/expression.h"
#include "sql/operator/logical_operator.h"
#include "sql/operator/physical_operator.h"

double CostModel::calculate_cost(Memo *memo,
                               GroupExpr *gexpr)
{
  auto op = gexpr->get_op();
  // Group constructor ensures logical_prop_ is never nullptr
  auto log_prop = memo->get_group_by_id(gexpr->get_group_id())->get_logical_prop();
  
  int arity = gexpr->get_children_groups_size();
  vector<LogicalProperty*> child_log_props;
  for (int i = 0; i < arity; ++i) {
    auto child_group_id = gexpr->get_child_group_id(i);
    auto child_gexpr = memo->get_group_by_id(child_group_id);
    auto child_prop = child_gexpr->get_logical_prop();
    child_log_props.push_back(child_prop);
  }
  double cost = op->calculate_cost(log_prop, child_log_props, this);
  string op_name = "UNKNOWN";
  if (op->is_logical()) {
    op_name = static_cast<const LogicalOperator*>(op)->name();
  } else if (op->is_physical()) {
    op_name = static_cast<const PhysicalOperator*>(op)->name();
  }
  LOG_DEBUG("CostModel: op_name=%s, group_id=%d, prop_card=%d, arity=%d, cost=%.6f",
           op_name.c_str(),
           gexpr->get_group_id(),
           log_prop ? log_prop->get_card() : 0,
           arity,
           cost);
  if (arity > 0 && !child_log_props.empty() && child_log_props[0]) {
    LOG_DEBUG("CostModel: first_child_card=%d", child_log_props[0]->get_card());
  }
  return cost;

}

double CostModel::calculate_expression_complexity(const Expression *expr) const
{
  if (expr == nullptr) {
    return 1.0;  // Default complexity
  }

  double complexity = 0.0;
  ExprType expr_type = expr->type();

  switch (expr_type) {
    case ExprType::VALUE:
      // Constant value, no computation needed
      complexity = 0.0;
      break;

    case ExprType::FIELD:
      // Field access, very cheap
      complexity = 1.0;
      break;

    case ExprType::CAST: {
      // Type conversion, moderate cost
      const CastExpr *cast_expr = static_cast<const CastExpr *>(expr);
      complexity = 2.0 + calculate_expression_complexity(cast_expr->child().get());
      break;
    }

    case ExprType::COMPARISON: {
      // Comparison operation, moderate cost
      const ComparisonExpr *cmp_expr = static_cast<const ComparisonExpr *>(expr);
      complexity = 2.0 + calculate_expression_complexity(cmp_expr->left().get()) +
                   calculate_expression_complexity(cmp_expr->right().get());
      break;
    }

    case ExprType::ARITHMETIC: {
      // Arithmetic operation, higher cost
      const ArithmeticExpr *arith_expr = static_cast<const ArithmeticExpr *>(expr);
      complexity = 3.0 + calculate_expression_complexity(arith_expr->left().get()) +
                   calculate_expression_complexity(arith_expr->right().get());
      break;
    }

    case ExprType::CONJUNCTION: {
      // Conjunction (AND/OR), cost depends on number of children
      const ConjunctionExpr *conj_expr = static_cast<const ConjunctionExpr *>(expr);
      complexity = 1.0;  // Base cost for conjunction
      for (const auto &child : conj_expr->children()) {
        complexity += calculate_expression_complexity(child.get());
      }
      break;
    }

    case ExprType::AGGREGATION: {
      // Aggregation function, high cost
      const AggregateExpr *agg_expr = static_cast<const AggregateExpr *>(expr);
      complexity = 5.0 + calculate_expression_complexity(agg_expr->child().get());
      break;
    }

    case ExprType::STAR:
    case ExprType::UNBOUND_FIELD:
    case ExprType::UNBOUND_AGGREGATION:
    case ExprType::NONE:
    default:
      // Unknown or unsupported types, use default
      complexity = 1.0;
      break;
  }

  return complexity;
}
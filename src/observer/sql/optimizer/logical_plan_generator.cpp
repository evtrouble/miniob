/* Copyright (c) 2023 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2023/08/16.
//

#include "sql/optimizer/logical_plan_generator.h"

#include "common/log/log.h"

#include "sql/operator/logical/calc_logical_operator.h"
#include "sql/operator/logical/delete_logical_operator.h"
#include "sql/operator/logical/explain_logical_operator.h"
#include "sql/operator/logical/insert_logical_operator.h"
#include "sql/operator/logical/join_logical_operator.h"
#include "sql/operator/logical_operator.h"
#include "sql/operator/logical/predicate_logical_operator.h"
#include "sql/operator/logical/project_logical_operator.h"
#include "sql/operator/logical/table_get_logical_operator.h"
#include "sql/operator/logical/group_by_logical_operator.h"

#include "sql/stmt/calc_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/explain_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/stmt.h"

#include "sql/expr/expression_iterator.h"
#include "sql/optimizer/cascade/optimizer_context.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/cascade/memo.h"

using namespace std;
using namespace common;

RC LogicalPlanGenerator::create(Stmt *stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  RC rc = RC::SUCCESS;
  switch (stmt->type()) {
    case StmtType::CALC: {
      CalcStmt *calc_stmt = static_cast<CalcStmt *>(stmt);
      rc = create_plan(calc_stmt, root_gexpr, context);
    } break;

    case StmtType::SELECT: {
      SelectStmt *select_stmt = static_cast<SelectStmt *>(stmt);
      rc = create_plan(select_stmt, root_gexpr, context);
    } break;

    case StmtType::INSERT: {
      InsertStmt *insert_stmt = static_cast<InsertStmt *>(stmt);
      rc = create_plan(insert_stmt, root_gexpr, context);
    } break;

    case StmtType::DELETE: {
      DeleteStmt *delete_stmt = static_cast<DeleteStmt *>(stmt);
      rc = create_plan(delete_stmt, root_gexpr, context);
    } break;

    case StmtType::EXPLAIN: {
      ExplainStmt *explain_stmt = static_cast<ExplainStmt *>(stmt);
      rc = create_plan(explain_stmt, root_gexpr, context);
    } break;
    default: {
      rc = RC::UNIMPLEMENTED;
    }
  }
  return rc;
}

RC LogicalPlanGenerator::create_plan(CalcStmt *calc_stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  unique_ptr<OperatorNode> calc_op(new CalcLogicalOperator(std::move(calc_stmt->expressions())));
  context->record_node_into_group(std::move(calc_op), &root_gexpr);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(SelectStmt *select_stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  GroupExpr *last_gexpr = nullptr;
  // GroupExpr *group_by_gexpr = nullptr;

  // 1. 创建table get和join
  const vector<Table *> &tables = select_stmt->tables();
  for (Table *table : tables) {
    unique_ptr<OperatorNode> table_get_op(new TableGetLogicalOperator(table, ReadWriteMode::READ_ONLY));
    GroupExpr *table_get_gexpr = nullptr;
    context->record_node_into_group(std::move(table_get_op), &table_get_gexpr);

    if (last_gexpr == nullptr) {
      last_gexpr = table_get_gexpr;
    } else {
      // 创建join
      unique_ptr<OperatorNode> join_op(new JoinLogicalOperator);
      std::vector<int> child_groups = {last_gexpr->get_group_id(), table_get_gexpr->get_group_id()};
      CandidateExpression candidate(std::move(join_op), std::move(child_groups));
      context->record_node_into_group(candidate, &last_gexpr);
    }
  }

  // 2. 创建filter/predicate 0x606000001820
  if (select_stmt->filter_stmt()) {
    // 先创建predicate的expressions
    RC rc = create_plan(select_stmt->filter_stmt(), last_gexpr, context, last_gexpr->get_group_id());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to create predicate logical plan. rc=%s", strrc(rc));
      return rc;
    }
  }

  // // 3. 创建group by（检查是否有group by或聚合函数）
  // bool has_aggregation = false;
  // function<RC(unique_ptr<Expression>&)> check_aggregation = [&](unique_ptr<Expression> &expr) -> RC {
  //   if (expr->type() == ExprType::AGGREGATION) {
  //     has_aggregation = true;
  //   }
  //   return ExpressionIterator::iterate_child_expr(*expr, check_aggregation);
  // };
  // for (auto &expr : select_stmt->query_expressions()) {
  //   check_aggregation(expr);
  // }

  // if (select_stmt->group_by().size() > 0 || has_aggregation) {
  //   // 创建group by expressions和aggregate expressions
  //   vector<unique_ptr<Expression>> group_by_exprs;
  //   for (auto &expr : select_stmt->group_by()) {
  //     group_by_exprs.push_back(expr->copy());
  //   }

  //   vector<Expression *> aggregate_expressions;
  //   function<RC(unique_ptr<Expression>&)> collector = [&](unique_ptr<Expression> &expr) -> RC {
  //     RC rc = RC::SUCCESS;
  //     if (expr->type() == ExprType::AGGREGATION) {
  //       aggregate_expressions.push_back(expr.get());
  //     }
  //     rc = ExpressionIterator::iterate_child_expr(*expr, collector);
  //     return rc;
  //   };

  //   for (auto &expr : select_stmt->query_expressions()) {
  //     collector(expr);
  //   }

  //   if (last_gexpr) {
  //     unique_ptr<OperatorNode> group_by_op(new GroupByLogicalOperator(std::move(group_by_exprs), std::move(aggregate_expressions)));
  //     std::vector<int> child_groups = {last_gexpr->get_group_id()};
  //     CandidateExpression candidate(std::move(group_by_op), std::move(child_groups));
  //     bool inserted = context->record_node_into_group(candidate, &group_by_gexpr);
  //     if (!inserted) {
  //       Memo &memo = context->get_memo();
  //       auto group = memo.get_group_by_id(group_by_gexpr->get_group_id());
  //       group_by_gexpr = group->get_logical_expression();
  //     }
  //     last_gexpr = group_by_gexpr;
  //   }
  // }

  // 4. 创建projection
  unique_ptr<OperatorNode> project_op(new ProjectLogicalOperator(std::move(select_stmt->query_expressions())));
  CandidateExpression candidate(std::move(project_op), {last_gexpr->get_group_id()});
  context->record_node_into_group(candidate, &root_gexpr);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(FilterStmt *filter_stmt, GroupExpr *&root_gexpr, OptimizerContext *context, int gid)
{
  RC                                  rc = RC::SUCCESS;
  vector<unique_ptr<Expression>> cmp_exprs;
  const vector<FilterUnit *>    &filter_units = filter_stmt->filter_units();
  for (const FilterUnit *filter_unit : filter_units) {
    const FilterObj &filter_obj_left  = filter_unit->left();
    const FilterObj &filter_obj_right = filter_unit->right();

    unique_ptr<Expression> left(filter_obj_left.is_attr
                                    ? static_cast<Expression *>(new FieldExpr(filter_obj_left.field))
                                    : static_cast<Expression *>(new ValueExpr(filter_obj_left.value)));

    unique_ptr<Expression> right(filter_obj_right.is_attr
                                     ? static_cast<Expression *>(new FieldExpr(filter_obj_right.field))
                                     : static_cast<Expression *>(new ValueExpr(filter_obj_right.value)));

    if (left->value_type() != right->value_type()) {
      auto left_to_right_cost = implicit_cast_cost(left->value_type(), right->value_type());
      auto right_to_left_cost = implicit_cast_cost(right->value_type(), left->value_type());
      if (left_to_right_cost <= right_to_left_cost && left_to_right_cost != INT32_MAX) {
        ExprType left_type = left->type();
        auto cast_expr = make_unique<CastExpr>(std::move(left), right->value_type());
        if (left_type == ExprType::VALUE) {
          Value left_val;
          if (OB_FAIL(rc = cast_expr->try_get_value(left_val)))
          {
            LOG_WARN("failed to get value from left child", strrc(rc));
            return rc;
          }
          left = make_unique<ValueExpr>(left_val);
        } else {
          left = std::move(cast_expr);
        }
      } else if (right_to_left_cost < left_to_right_cost && right_to_left_cost != INT32_MAX) {
        ExprType right_type = right->type();
        auto cast_expr = make_unique<CastExpr>(std::move(right), left->value_type());
        if (right_type == ExprType::VALUE) {
          Value right_val;
          if (OB_FAIL(rc = cast_expr->try_get_value(right_val)))
          {
            LOG_WARN("failed to get value from right child", strrc(rc));
            return rc;
          }
          right = make_unique<ValueExpr>(right_val);
        } else {
          right = std::move(cast_expr);
        }

      } else {
        rc = RC::UNSUPPORTED;
        LOG_WARN("unsupported cast from %s to %s", attr_type_to_string(left->value_type()), attr_type_to_string(right->value_type()));
        return rc;
      }
    }

    ComparisonExpr *cmp_expr = new ComparisonExpr(filter_unit->comp(), std::move(left), std::move(right));
    cmp_exprs.emplace_back(cmp_expr);
  }

  unique_ptr<ConjunctionExpr> conjunction_expr(new ConjunctionExpr(ConjunctionExpr::Type::AND, cmp_exprs));
  unique_ptr<OperatorNode> predicate_op(new PredicateLogicalOperator(std::move(conjunction_expr)));
  CandidateExpression candidate(std::move(predicate_op), {gid});
  context->record_node_into_group(candidate, &root_gexpr);

  return rc;
}

int LogicalPlanGenerator::implicit_cast_cost(AttrType from, AttrType to)
{
  if (from == to) {
    return 0;
  }
  return DataType::type_instance(from)->cast_cost(to);
}

RC LogicalPlanGenerator::create_plan(InsertStmt *insert_stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  Table        *table = insert_stmt->table();
  vector<Value> values(insert_stmt->values(), insert_stmt->values() + insert_stmt->value_amount());
  unique_ptr<OperatorNode> insert_op(new InsertLogicalOperator(table, values));
  context->record_node_into_group(std::move(insert_op), &root_gexpr);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(DeleteStmt *delete_stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  Table *table = delete_stmt->table();
  FilterStmt *filter_stmt = delete_stmt->filter_stmt();

  // 1. 创建table get
  unique_ptr<OperatorNode> table_get_op(new TableGetLogicalOperator(table, ReadWriteMode::READ_WRITE));
  GroupExpr *table_get_gexpr = nullptr;
  context->record_node_into_group(std::move(table_get_op), &table_get_gexpr);

  // 2. 创建predicate（如果有）
  GroupExpr *predicate_gexpr = nullptr;
  GroupExpr *last_gexpr = table_get_gexpr;
  
  if (filter_stmt) {
    RC rc = create_plan(filter_stmt, predicate_gexpr, context, last_gexpr->get_group_id());
    if (OB_FAIL(rc)) {
      return rc;
    }
    last_gexpr = predicate_gexpr;
  }

  // 3. 创建delete
  unique_ptr<OperatorNode> delete_op(new DeleteLogicalOperator(table));
  std::vector<int> child_groups = {last_gexpr->get_group_id()};
  CandidateExpression candidate(std::move(delete_op), std::move(child_groups));
  context->record_node_into_group(candidate, &root_gexpr);

  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(ExplainStmt *explain_stmt, GroupExpr *&root_gexpr, OptimizerContext *context)
{
  Stmt *child_stmt = explain_stmt->child();
  GroupExpr *child_gexpr = nullptr;

  RC rc = create(child_stmt, child_gexpr, context);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create explain's child operator. rc=%s", strrc(rc));
    return rc;
  }

  unique_ptr<OperatorNode> explain_op(new ExplainLogicalOperator);
  if (child_gexpr) {
    std::vector<int> child_groups = {child_gexpr->get_group_id()};
    CandidateExpression candidate(std::move(explain_op), std::move(child_groups));
    bool inserted = context->record_node_into_group(candidate, &root_gexpr);
    if (!inserted) {
      Memo &memo = context->get_memo();
      auto group = memo.get_group_by_id(root_gexpr->get_group_id());
      root_gexpr = group->get_logical_expression();
    }
  } else {
    context->record_node_into_group(std::move(explain_op), &root_gexpr);
  }

  return rc;
}

/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/log/log.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/cascade/implementation_rules.h"
#include "sql/operator/logical/table_get_logical_operator.h"
#include "sql/operator/physical/table_scan_physical_operator.h"
#include "sql/operator/logical/project_logical_operator.h"
#include "sql/operator/physical/project_physical_operator.h"
#include "sql/operator/logical/insert_logical_operator.h"
#include "sql/operator/physical/insert_physical_operator.h"
#include "sql/operator/logical/explain_logical_operator.h"
#include "sql/operator/physical/explain_physical_operator.h"
#include "sql/operator/logical/calc_logical_operator.h"
#include "sql/operator/physical/calc_physical_operator.h"
#include "sql/operator/logical/delete_logical_operator.h"
#include "sql/operator/physical/delete_physical_operator.h"
#include "sql/operator/logical/predicate_logical_operator.h"
#include "sql/operator/physical/predicate_physical_operator.h"
#include "sql/operator/logical/group_by_logical_operator.h"
#include "sql/operator/physical/scalar_group_by_physical_operator.h"
#include "sql/operator/physical/hash_group_by_physical_operator.h"
#include "sql/operator/physical/index_scan_physical_operator.h"
#include "sql/operator/logical/join_logical_operator.h"
#include "sql/operator/physical/nested_loop_join_physical_operator.h"
#include "sql/operator/physical/hash_join_physical_operator.h"
#include "sql/operator/physical/empty_physical_operator.h"
#include "sql/operator/logical/empty_logical_operator.h"
#include "sql/expr/expression.h"
#include "sql/parser/parse_defs.h"

// -------------------------------------------------------------------------------------------------
// PhysicalSeqScan
// -------------------------------------------------------------------------------------------------
LogicalGetToPhysicalSeqScan::LogicalGetToPhysicalSeqScan()
{
  type_          = RuleType::GET_TO_SEQ_SCAN;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALGET));
}

void LogicalGetToPhysicalSeqScan::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  TableGetLogicalOperator *table_get_oper = static_cast<TableGetLogicalOperator *>(input->get_op());

  vector<unique_ptr<Expression>> &log_preds = table_get_oper->predicates();
  vector<unique_ptr<Expression>>  phys_preds;
  for (auto &pred : log_preds) {
    phys_preds.push_back(pred->copy());
  }

  Table *table           = table_get_oper->table();
  auto   table_scan_oper = new TableScanPhysicalOperator(table, table_get_oper->read_write_mode());
  table_scan_oper->set_predicates(std::move(phys_preds));
  auto oper = unique_ptr<OperatorNode>(table_scan_oper);

  transformed->emplace_back(std::move(oper));
}

// -------------------------------------------------------------------------------------------------
// PhysicalIndexScan
// -------------------------------------------------------------------------------------------------
LogicalGetToPhysicalIndexScan::LogicalGetToPhysicalIndexScan()
{
  type_          = RuleType::GET_TO_INDEX_SCAN;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALGET));
}

void LogicalGetToPhysicalIndexScan::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  TableGetLogicalOperator *table_get_oper = static_cast<TableGetLogicalOperator *>(input->get_op());

  vector<unique_ptr<Expression>> &predicates = table_get_oper->predicates();
  Table *table = table_get_oper->table();

  // Find expressions that can be used for index lookup
  Index     *index      = nullptr;
  ValueExpr *left_value_expr  = nullptr;
  ValueExpr *right_value_expr = nullptr;
  bool       left_inclusive    = false;
  bool       right_inclusive  = false;
  CompOp     comp_op          = NO_OP;
  
  for (auto &expr : predicates) {
    if (expr->type() == ExprType::COMPARISON) {
      auto comparison_expr = static_cast<ComparisonExpr *>(expr.get());
      comp_op = comparison_expr->comp();
      
      if (comp_op != EQUAL_TO) {
        continue;
      }

      unique_ptr<Expression> &left_expr  = comparison_expr->left();
      unique_ptr<Expression> &right_expr = comparison_expr->right();
      // At least one side must be a value
      if (left_expr->type() != ExprType::VALUE && right_expr->type() != ExprType::VALUE) {
        continue;
      }

      FieldExpr *field_expr = nullptr;
      ValueExpr *value_expr = nullptr;
      
      if (left_expr->type() == ExprType::FIELD && right_expr->type() == ExprType::VALUE) {
        field_expr = static_cast<FieldExpr *>(left_expr.get());
        value_expr = static_cast<ValueExpr *>(right_expr.get());
      } else if (right_expr->type() == ExprType::FIELD && left_expr->type() == ExprType::VALUE) {
        field_expr = static_cast<FieldExpr *>(right_expr.get());
        value_expr = static_cast<ValueExpr *>(left_expr.get());
      }

      if (field_expr == nullptr || value_expr == nullptr) {
        continue;
      }

      const Field &field = field_expr->field();
      // Check if field belongs to current table
      if (field.table() != table) {
        continue;
      }
      
      Index *found_index = table->find_index_by_field(field.field_name());
      if (nullptr == found_index) {
        continue;
      }
      
      // Found index, set range based on comparison operator
      index = found_index;

      left_value_expr = value_expr;
      right_value_expr = value_expr;
      left_inclusive = true;
      right_inclusive = true;
      
      break;  // Exit after finding first usable index query
    }
  }

  // Only generate IndexScan if index is found
  if (index != nullptr) {
    vector<unique_ptr<Expression>> phys_preds;
    for (auto &pred : predicates) {
      phys_preds.push_back(pred->copy());
    }

    const Value *left_value = left_value_expr ? &left_value_expr->get_value() : nullptr;
    const Value *right_value = right_value_expr ? &right_value_expr->get_value() : nullptr;
    
    auto index_scan_oper = new IndexScanPhysicalOperator(table, index, table_get_oper->read_write_mode(),
        left_value, left_inclusive, right_value, right_inclusive);
    index_scan_oper->set_predicates(std::move(phys_preds));
    auto oper = unique_ptr<OperatorNode>(index_scan_oper);

    transformed->emplace_back(std::move(oper));
  }
}

// -------------------------------------------------------------------------------------------------
//  LogicalProjectionToProjection
// -------------------------------------------------------------------------------------------------
LogicalProjectionToProjection::LogicalProjectionToProjection()
{
  type_          = RuleType::PROJECTION_TO_PHYSOCAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALPROJECTION));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalProjectionToProjection::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto project_oper = static_cast<ProjectLogicalOperator *>(input->get_op());
  ASSERT(input->get_children_groups_size() == 1, "only one child is supported for now");

  unique_ptr<PhysicalOperator> child_phy_oper;

  auto project_operator = make_unique<ProjectPhysicalOperator>(std::move(project_oper->expressions()));

  transformed->emplace_back(std::move(project_operator), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// PhysicalInsert
// -------------------------------------------------------------------------------------------------
LogicalInsertToInsert::LogicalInsertToInsert()
{
  type_          = RuleType::INSERT_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALINSERT));
}

void LogicalInsertToInsert::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  InsertLogicalOperator *insert_oper = static_cast<InsertLogicalOperator *>(input->get_op());

  Table         *table           = insert_oper->table();
  vector<Value> &values          = insert_oper->values();
  auto           insert_phy_oper = make_unique<InsertPhysicalOperator>(table, std::move(values));

  transformed->emplace_back(std::move(insert_phy_oper));
}

// -------------------------------------------------------------------------------------------------
// PhysicalExplain
// -------------------------------------------------------------------------------------------------
LogicalExplainToExplain::LogicalExplainToExplain()
{
  type_          = RuleType::EXPLAIN_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALEXPLAIN));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalExplainToExplain::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  unique_ptr<PhysicalOperator> explain_physical_oper(new ExplainPhysicalOperator());

  transformed->emplace_back(std::move(explain_physical_oper), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// PhysicalCalc
// -------------------------------------------------------------------------------------------------
LogicalCalcToCalc::LogicalCalcToCalc()
{
  type_          = RuleType::CALC_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALCALCULATE));
}

void LogicalCalcToCalc::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto                             calc_oper = static_cast<CalcLogicalOperator *>(input->get_op());
  unique_ptr<CalcPhysicalOperator> calc_phys_oper(new CalcPhysicalOperator(std::move(calc_oper->expressions())));

  transformed->emplace_back(std::move(calc_phys_oper));
}

// -------------------------------------------------------------------------------------------------
// PhysicalDelete
// -------------------------------------------------------------------------------------------------
LogicalDeleteToDelete::LogicalDeleteToDelete()
{
  type_          = RuleType::DELETE_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALDELETE));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalDeleteToDelete::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto delete_oper = static_cast<DeleteLogicalOperator*>(input->get_op());

  auto delete_phys_oper = unique_ptr<PhysicalOperator>(new DeletePhysicalOperator(delete_oper->table()));

  transformed->emplace_back(std::move(delete_phys_oper), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// Physical Predicate
// -------------------------------------------------------------------------------------------------
LogicalPredicateToPredicate::LogicalPredicateToPredicate()
{
  type_          = RuleType::PREDICATE_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALFILTER));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalPredicateToPredicate::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto predicate_oper = static_cast<PredicateLogicalOperator *>(input->get_op());

  vector<unique_ptr<Expression>> &expressions = predicate_oper->expressions();
  ASSERT(expressions.size() == 1, "predicate logical operator's children should be 1");

  unique_ptr<Expression>       expression = std::move(expressions.front());
  unique_ptr<PhysicalOperator> oper =
      unique_ptr<PhysicalOperator>(new PredicatePhysicalOperator(std::move(expression)));
  transformed->emplace_back(std::move(oper), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// Physical Nested Loop Join
// -------------------------------------------------------------------------------------------------
LogicalInnerJoinToNestedLoopJoin::LogicalInnerJoinToNestedLoopJoin()
{
  type_          = RuleType::INNER_JOIN_TO_NL_JOIN;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALINNERJOIN));
  auto left      = new Pattern(OpType::LEAF);
  auto right     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(left);
  match_pattern_->add_child(right);
}

void LogicalInnerJoinToNestedLoopJoin::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  ASSERT(input->get_children_groups_size() == 2, "join should have 2 children");

  auto nl_join_oper = make_unique<NestedLoopJoinPhysicalOperator>();
  transformed->emplace_back(std::move(nl_join_oper), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// Physical Hash Join
// -------------------------------------------------------------------------------------------------
LogicalInnerJoinToHashJoin::LogicalInnerJoinToHashJoin()
{
  type_          = RuleType::INNER_JOIN_TO_HASH_JOIN;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALINNERJOIN));
  auto left      = new Pattern(OpType::LEAF);
  auto right     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(left);
  match_pattern_->add_child(right);
}

void LogicalInnerJoinToHashJoin::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  ASSERT(input->get_children_groups_size() == 2, "join should have 2 children");

  // TODO: HashJoinPhysicalOperator is currently empty, needs implementation
  // Temporarily disabled, enable after implementation
  // auto hash_join_oper = make_unique<HashJoinPhysicalOperator>();
  // transformed->emplace_back(std::move(hash_join_oper), input->get_child_group_ids());
}

// -------------------------------------------------------------------------------------------------
// Physical Aggregation (Scalar GroupBy)
// -------------------------------------------------------------------------------------------------
LogicalGroupByToAggregation::LogicalGroupByToAggregation()
{
  type_          = RuleType::GROUP_BY_TO_PHYSICAL_AGGREGATION;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALGROUPBY));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalGroupByToAggregation::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto groupby_oper = static_cast<GroupByLogicalOperator *>(input->get_op());
  vector<unique_ptr<Expression>> &group_by_expressions = groupby_oper->group_by_expressions();
  
  // Only generate ScalarGroupBy when group_by_expressions is empty
  if (group_by_expressions.empty()) {
    vector<Expression *> aggregate_exprs;
    for (auto expr : groupby_oper->aggregate_expressions()) {
      aggregate_exprs.push_back(expr);
    }
    auto groupby_phys_oper = make_unique<ScalarGroupByPhysicalOperator>(std::move(aggregate_exprs));
    transformed->emplace_back(std::move(groupby_phys_oper), input->get_child_group_ids());
  }
}

// -------------------------------------------------------------------------------------------------
// Physical Hash Group By
// -------------------------------------------------------------------------------------------------
LogicalGroupByToHashGroupBy::LogicalGroupByToHashGroupBy()
{
  type_          = RuleType::GROUP_BY_TO_PHYSICL_HASH_GROUP_BY;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALGROUPBY));
  auto child     = new Pattern(OpType::LEAF);
  match_pattern_->add_child(child);
}

void LogicalGroupByToHashGroupBy::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  auto groupby_oper = static_cast<GroupByLogicalOperator *>(input->get_op());
  vector<unique_ptr<Expression>> &group_by_expressions = groupby_oper->group_by_expressions();
  
  // Only generate HashGroupBy when group_by_expressions is not empty
  if (!group_by_expressions.empty()) {
    vector<unique_ptr<Expression>> group_by_exprs;
    for (auto &expr : group_by_expressions) {
      group_by_exprs.push_back(expr->copy());
    }
    vector<Expression *> aggregate_exprs;
    for (auto expr : groupby_oper->aggregate_expressions()) {
      aggregate_exprs.push_back(expr);
    }
    auto groupby_phys_oper = make_unique<HashGroupByPhysicalOperator>(std::move(group_by_exprs), std::move(aggregate_exprs));
    transformed->emplace_back(std::move(groupby_phys_oper), input->get_child_group_ids());
  }
}

// -------------------------------------------------------------------------------------------------
// Physical Empty
// -------------------------------------------------------------------------------------------------
LogicalEmptyToEmpty::LogicalEmptyToEmpty()
{
  type_          = RuleType::EMPTY_TO_PHYSICAL;
  match_pattern_ = unique_ptr<Pattern>(new Pattern(OpType::LOGICALEMPTY));
}

void LogicalEmptyToEmpty::transform(
    GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const
{
  unique_ptr<PhysicalOperator> empty_phys_oper(new EmptyPhysicalOperator());
  transformed->emplace_back(std::move(empty_phys_oper));
}
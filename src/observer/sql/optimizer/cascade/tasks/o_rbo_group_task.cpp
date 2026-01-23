/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/tasks/o_rbo_group_task.h"
#include "sql/optimizer/cascade/optimizer_context.h"
#include "sql/optimizer/cascade/memo.h"
#include "sql/optimizer/cascade/rules.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/operator/logical_operator.h"
#include "common/log/log.h"

RC OptimizeRBOGroup::perform()
{
  LOG_TRACE("OptimizeRBOGroup::perform() group %d", group_->get_id());
  
  RC rc = logical_generate();
  if(OB_FAIL(rc)) {
    return rc;
  }
  return physical_generate();
}

RC OptimizeRBOGroup::logical_generate()
{
  if (group_->has_explored()) {
    return RC::SUCCESS;
  }

  // Apply transformation rules until no more rules can be applied
  // Use "apply rules first, then optimize new child groups" pattern (similar to CBO)
  auto &trans_rules = get_rule_set().get_rules_by_name(RuleSetName::LOGICAL_TRANSFORMATION);

  for (auto *rule : trans_rules) {
    // Get the current last logical operator (may have changed after applying rules)
    GroupExpr *logical_expr = group_->get_logical_expressions().back();
    if (logical_expr->rule_explored(rule) || 
        logical_expr->get_op()->get_op_type() != rule->get_match_pattern()->type() ||
        logical_expr->get_children_groups_size() != rule->get_match_pattern()->get_child_patterns_size()) {
      continue;
    }
    logical_expr->set_rule_explored(rule);

    // Apply transformation rule
    std::vector<CandidateExpression> after;
    rule->transform(logical_expr, &after, context_);
    
    for (auto &candidate : after) {
      GroupExpr *new_gexpr = nullptr;
      context_->record_node_into_group(candidate, &new_gexpr, group_->get_id());
    }
  }

  GroupExpr *logical_expr = group_->get_logical_expressions().back();
  for (int child_id : logical_expr->get_child_group_ids()) {
    Group *child_group = get_memo().get_group_by_id(child_id);
    if (child_group == nullptr) {
      return RC::OPTIMIZER_INVALID_GROUP_ID;
    }
    
    // If child group hasn't been explored, recursively optimize it
    if (!child_group->has_explored()) {
      OptimizeRBOGroup child_task(child_group, context_);
      RC rc = child_task.logical_generate();
      if (OB_FAIL(rc)) {
        return rc;
      }
    }
  }

  group_->set_explored();
  return RC::SUCCESS;
}

RC OptimizeRBOGroup::physical_generate()
{
  // Apply implementation rules to the final logical operator
  // TODO: Only apply basic rules (not CBO-specific rules like JoinReorder)
  if(!group_->get_physical_expressions().empty()) {
    return RC::SUCCESS;
  }

  GroupExpr *logical_expr = group_->get_logical_expressions().back();
  auto &impl_rules = get_rule_set().get_rules_by_name(RuleSetName::PHYSICAL_IMPLEMENTATION);

  for (auto *rule : impl_rules) {
    if (logical_expr->get_op()->get_op_type() != rule->get_match_pattern()->type() ||
        logical_expr->get_children_groups_size() != rule->get_match_pattern()->get_child_patterns_size()) {
      continue;
    }

    // Apply implementation rule
    std::vector<CandidateExpression> after;
    rule->transform(logical_expr, &after, context_);
    
    for (auto &candidate : after) {
      GroupExpr *new_gexpr = nullptr;
      context_->record_node_into_group(candidate, &new_gexpr, group_->get_id());
      for (int child_id : new_gexpr->get_child_group_ids()) {
        Group *child_group = get_memo().get_group_by_id(child_id);
        if (child_group == nullptr) {
          return RC::OPTIMIZER_INVALID_GROUP_ID;
        }
        OptimizeRBOGroup child_task(child_group, context_);
        RC rc = child_task.physical_generate();
        if (OB_FAIL(rc)) {
          return rc;
        }
      }
      return RC::SUCCESS;
    }
  }

  auto logical_op = static_cast<LogicalOperator *>(logical_expr->get_op());
  LOG_ERROR("Missing physical implementation rule for logical operator: %s (type: %d)", 
            logical_op->name().c_str(), static_cast<int>(logical_expr->get_op()->get_op_type()));
  return RC::OPTIMIZER_GROUP_EXPR_CREATE_FAILED;
}
/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/tasks/apply_rule_task.h"
#include "sql/optimizer/cascade/tasks/o_input_task.h"
#include "sql/optimizer/cascade/tasks/o_expr_task.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/cascade/rules.h"
#include "common/log/log.h"

RC ApplyRule::perform()
{
  LOG_TRACE("ApplyRule::perform() for rule: {%d}", rule_->get_rule_idx());
  if (group_expr_->rule_explored(rule_)) {
    return RC::SUCCESS;
  }
  // TODO: expr binding, currently group_expr_->get_op() is enough
  // TODO: check condition

  std::vector<CandidateExpression> after;
  rule_->transform(group_expr_, &after, context_);
  for (auto &candidate : after) {
    GroupExpr *new_gexpr = nullptr;
    auto g_id = group_expr_->get_group_id();
    if(context_->record_node_into_group(candidate, &new_gexpr, g_id)) {
      if (new_gexpr->get_op()->is_logical()) {
        // further optimize new expr
        push_task(new OptimizeExpression(new_gexpr, context_));
      } else {
        // calculate the cost of the new physical expr
        push_task(new OptimizeInputs(new_gexpr, context_));
      }
    } else {
      LOG_INFO("record_operator_node_into_group not insert new expr");
      new_gexpr->dump();
    }
  }

  group_expr_->set_rule_explored(rule_);
  return RC::SUCCESS;
}
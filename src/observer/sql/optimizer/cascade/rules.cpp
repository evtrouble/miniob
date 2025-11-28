/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/rules.h"
#include "sql/optimizer/cascade/implementation_rules.h"
#include "sql/optimizer/cascade/transformation_rules.h"
#include "sql/optimizer/cascade/group_expr.h"

RuleSet::RuleSet()
{
  // Transformation rules (logical -> logical)
  add_rule(RuleSetName::LOGICAL_TRANSFORMATION, new PredicatePushdownRule());
  add_rule(RuleSetName::LOGICAL_TRANSFORMATION, new PredicateRewriteRule());
  add_rule(RuleSetName::LOGICAL_TRANSFORMATION, new ExpressionSimplifyRule());

  // Implementation rules (logical -> physical)
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalProjectionToProjection());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalGetToPhysicalSeqScan());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalGetToPhysicalIndexScan());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalInsertToInsert());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalExplainToExplain());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalCalcToCalc());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalDeleteToDelete());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalPredicateToPredicate());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalInnerJoinToNestedLoopJoin());
  // TODO: 等 HashJoinPhysicalOperator 实现后再启用
  // add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalInnerJoinToHashJoin());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalGroupByToAggregation());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalGroupByToHashGroupBy());
  add_rule(RuleSetName::PHYSICAL_IMPLEMENTATION, new LogicalEmptyToEmpty());
}
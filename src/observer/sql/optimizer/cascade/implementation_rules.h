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

/**
 * Rule transforms Logical Scan -> Physical Scan
 */
class LogicalGetToPhysicalSeqScan : public Rule
{
public:
  LogicalGetToPhysicalSeqScan();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Get -> Physical Index Scan
 */
class LogicalGetToPhysicalIndexScan : public Rule
{
public:
  LogicalGetToPhysicalIndexScan();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Projection -> Physical Projection
 */
class LogicalProjectionToProjection : public Rule
{
public:
  LogicalProjectionToProjection();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Insert -> Physical Insert
 */
class LogicalInsertToInsert : public Rule
{
public:
  LogicalInsertToInsert();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical explain -> Physical explain
 */
class LogicalExplainToExplain : public Rule
{
public:
  LogicalExplainToExplain();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical calculate -> Physical calculate
 */
class LogicalCalcToCalc : public Rule
{
public:
  LogicalCalcToCalc();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical delete -> Physical delete
 */
class LogicalDeleteToDelete : public Rule
{
public:
  LogicalDeleteToDelete();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical predicate -> Physical predicate
 * TODO: In practice, this rule may not be used and can be removed
 */
class LogicalPredicateToPredicate : public Rule
{
public:
  LogicalPredicateToPredicate();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Inner Join -> Physical Nested Loop Join
 */
class LogicalInnerJoinToNestedLoopJoin : public Rule
{
public:
  LogicalInnerJoinToNestedLoopJoin();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Inner Join -> Physical Hash Join
 */
class LogicalInnerJoinToHashJoin : public Rule
{
public:
  LogicalInnerJoinToHashJoin();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Groupby -> Physical Aggregation(Scalar Groupby)
 */
class LogicalGroupByToAggregation : public Rule
{
public:
  LogicalGroupByToAggregation();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical GroupBy -> Physical GroupBy(Hash GroupBy)
 */
class LogicalGroupByToHashGroupBy : public Rule
{
public:
  LogicalGroupByToHashGroupBy();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};

/**
 * Rule transforms Logical Empty -> Physical Empty
 */
class LogicalEmptyToEmpty : public Rule
{
public:
  LogicalEmptyToEmpty();

  void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const override;
};
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

#pragma once

#include "common/lang/memory.h"
#include "common/sys/rc.h"
#include "common/type/attr_type.h"

class Stmt;
class CalcStmt;
class SelectStmt;
class FilterStmt;
class InsertStmt;
class DeleteStmt;
class ExplainStmt;
class GroupExpr;
class OptimizerContext;

class LogicalPlanGenerator
{
public:
  LogicalPlanGenerator()          = default;
  virtual ~LogicalPlanGenerator() = default;

  RC create(Stmt *stmt, GroupExpr *&root_gexpr, OptimizerContext *context);

private:
  RC create_plan(CalcStmt *calc_stmt, GroupExpr *&root_gexpr, OptimizerContext *context);
  RC create_plan(SelectStmt *select_stmt, GroupExpr *&root_gexpr, OptimizerContext *context);
  RC create_plan(FilterStmt *filter_stmt, GroupExpr *&root_gexpr, OptimizerContext *context, int gid);
  RC create_plan(InsertStmt *insert_stmt, GroupExpr *&root_gexpr, OptimizerContext *context);
  RC create_plan(DeleteStmt *delete_stmt, GroupExpr *&root_gexpr, OptimizerContext *context);
  RC create_plan(ExplainStmt *explain_stmt, GroupExpr *&root_gexpr, OptimizerContext *context);

  int implicit_cast_cost(AttrType from, AttrType to);
};

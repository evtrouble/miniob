/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Longda on 2021/4/13.
//

#include <string.h>

#include "optimize_stage.h"

#include "common/conf/ini.h"
#include "common/io/io.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "sql/stmt/stmt.h"
#include "sql/optimizer/cascade/optimizer.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/optimizer_utils.h"

using namespace std;
using namespace common;

RC OptimizeStage::handle_request(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  if (nullptr == stmt) {
    return RC::UNIMPLEMENTED;
  }

  Optimizer optimizer;
  GroupExpr *root_gexpr = nullptr;
  
  // 直接生成GroupExpr结构
  RC rc = logical_plan_generator_.create(stmt, root_gexpr, optimizer.context());
  if (rc != RC::SUCCESS) {
    if (rc != RC::UNIMPLEMENTED) {
      LOG_WARN("failed to create group expression. rc=%s", strrc(rc));
    }
    return rc;
  }

  if (!root_gexpr) {
    LOG_WARN("root group expression is null");
    return RC::INTERNAL;
  }

  // 使用Cascade优化器
  unique_ptr<PhysicalOperator> physical_operator;
  rc = optimizer.optimize(root_gexpr, physical_operator);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to optimize logical plan. rc=%s", strrc(rc));
    return rc;
  }
  string phys_plan_str = OptimizerUtils::dump_physical_plan(physical_operator);
  LOG_DEBUG("cascade physical plan:\n%s", phys_plan_str.c_str());
  sql_event->set_operator(std::move(physical_operator));
  return rc;
}

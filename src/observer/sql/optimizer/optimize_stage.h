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

#pragma once

#include "common/sys/rc.h"
#include "sql/optimizer/logical_plan_generator.h"

class SQLStageEvent;
class Stmt;

/**
 * @brief 将解析后的Statement转换成执行计划，并进行优化
 * @ingroup SQLStage
 * @details 使用Cascade优化器，直接从Statement生成GroupExpr结构，然后进行优化。
 * 不过并不是所有的语句都需要生成计划，有些可以直接执行，比如create table、create index等。
 * 这些语句可以参考 @class CommandExecutor。
 */
class OptimizeStage
{
public:
  RC handle_request(SQLStageEvent *event);

private:
  LogicalPlanGenerator logical_plan_generator_;   ///< 根据SQL生成GroupExpr结构
};

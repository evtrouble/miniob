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

#include "sql/optimizer/cascade/tasks/cascade_task.h"
#include "sql/optimizer/cascade/group.h"

/**
 * @brief: OptimizeRBOGroup, optimize a group using Rule-Based Optimization (RBO)
 *
 * This task uses synchronous recursion to apply transformation and implementation rules.
 * Unlike CBO which uses asynchronous task scheduling, RBO applies rules directly in a
 * simple recursive manner.
 */
class OptimizeRBOGroup : public CascadeTask
{
public:
  OptimizeRBOGroup(Group *group, OptimizerContext *context)
      : CascadeTask(context, CascadeTaskType::OPTIMIZE_RBO_GROUP), group_(group)
  {}

  RC perform() override;

  RC logical_generate();
  RC physical_generate();

private:
  Group *group_;
};

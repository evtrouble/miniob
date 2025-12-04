/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/group_expr.h"
#include "sql/operator/logical_operator.h"
#include "sql/operator/physical_operator.h"
#include "common/log/log.h"

uint64_t GroupExpr::hash() const
{
  auto hash = contents_->hash();
  for (const auto &child : child_groups_) {
    hash ^= std::hash<int>()(child) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  hash ^= std::hash<int>()(group_id_) + 0x9e3779b9 + (hash << 6) + (hash >> 2);;
  return hash;
}

void GroupExpr::dump() const
{
  stringstream ss;
  for (const auto &child : child_groups_) {
    ss << child << " ";
  }
  string op_name = "UNKNOWN";
  if (contents_->is_logical()) {
    op_name = static_cast<const LogicalOperator*>(contents_.get())->name();
  } else if (contents_->is_physical()) {
    op_name = static_cast<const PhysicalOperator*>(contents_.get())->name();
  }
  LOG_TRACE("GroupExpr contents: op_name=%s, child groups: %s", op_name.c_str(), ss.str().c_str());
}
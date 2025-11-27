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

#include "common/lang/unordered_set.h"
#include "common/lang/vector.h"
#include "common/lang/memory.h"
#include "common/log/log.h"
#include "sql/optimizer/cascade/group_expr.h"
#include "sql/optimizer/cascade/group.h"

const int UNDEFINED_GROUP = -1;

/**
 * @brief: memorization
 * @details: Memo class for tracking Groups and GroupExpressions and provides the
 * mechanisms by which we can do duplicate group detection.
 * TODO: rename to search space(ssp)? (in columbia)
 */
class Memo
{
public:
  Memo() = default;

  ~Memo() = default;

  GroupExpr *insert_expression(GroupExpr *gexpr) { return insert_expression(gexpr, -1); }

  GroupExpr *insert_expression(GroupExpr *gexpr, int target_group);

  Group *get_group_by_id(int id) const
  {
    auto idx = id;
    ASSERT(idx >= 0 && static_cast<size_t>(idx) < groups_.size(), "group_id out of bounds");
    return groups_[idx].get();
  }

  void dump() const;

private:
  int add_new_group(GroupExpr *gexpr);

  struct GExprPtrHash
  {
    std::size_t operator()(GroupExpr *const &s) const
    {
      if (s == nullptr)
        return 0;
      return s->hash();
    }
  };

  struct GExprPtrEq
  {
    bool operator()(GroupExpr *t1, GroupExpr *t2) const
    {
      if (t1 && t2) {  // 确保非空检查
        return (*t1 == *t2);
      }
      return false;  // 防止空指针解引用
    }
  };

  /**
   * Group owns GroupExpressions, not the memo
   */
  std::unordered_set<GroupExpr *, GExprPtrHash, GExprPtrEq> group_expressions_;

  vector<unique_ptr<Group>> groups_;
};
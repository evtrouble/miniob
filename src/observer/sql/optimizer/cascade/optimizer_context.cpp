/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/optimizer_context.h"
#include "sql/optimizer/cascade/memo.h"
#include "sql/optimizer/cascade/rules.h"

OptimizerContext::OptimizerContext()
      : memo_(new Memo()), rule_set_(new RuleSet()), cost_model_(), task_pool_(nullptr),
        cost_upper_bound_(std::numeric_limits<double>::max()) {}

OptimizerContext::~OptimizerContext() {
    if (task_pool_ != nullptr) {
      delete task_pool_;
      task_pool_ = nullptr;
    }
    if (memo_ != nullptr) {
      delete memo_;
      memo_ = nullptr;
    }
    if (rule_set_ != nullptr) {
      delete rule_set_;
      rule_set_ = nullptr;
    }
  }

bool OptimizerContext::record_node_into_group(unique_ptr<OperatorNode> node, GroupExpr **gexpr,
                                  int target_group) {
  // Note: ownership of node is transferred to GroupExpr
  // If node already exists, memo_->insert_expression will delete new_gexpr, releasing node
  std::vector<int> empty_child_groups;
  auto new_gexpr = new GroupExpr(std::move(node), std::move(empty_child_groups));
  auto ptr = memo_->insert_expression(new_gexpr, target_group);
  ASSERT(ptr, "Root of expr should not fail insertion");

  (*gexpr) = ptr;
  // If returned ptr is not new_gexpr, node already exists, new_gexpr will be deleted and node released
  // If returned ptr is new_gexpr, it's newly inserted, ownership of node has been transferred to GroupExpr
  return (ptr == new_gexpr);
}

bool OptimizerContext::record_node_into_group(CandidateExpression &candidate, GroupExpr **gexpr, int target_group) {
  auto new_gexpr = new GroupExpr(std::move(candidate.op), std::move(candidate.child_group_ids));
  auto ptr = memo_->insert_expression(new_gexpr, target_group);
  ASSERT(ptr, "Root of expr should not fail insertion");

  (*gexpr) = ptr;
  return (ptr == new_gexpr);
}

Memo &OptimizerContext::get_memo() { return *memo_; }

RuleSet &OptimizerContext::get_rule_set() { return *rule_set_; }

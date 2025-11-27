/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/cascade/optimizer.h"
#include "sql/optimizer/cascade/tasks/o_group_task.h"
#include "sql/optimizer/cascade/memo.h"

RC Optimizer::optimize(GroupExpr *root_gexpr, std::unique_ptr<PhysicalOperator> &physical_operator)
{
  ASSERT(root_gexpr != nullptr, "Root group expression should not be null");
  
  context_->get_memo().dump();

  int root_id = root_gexpr->get_group_id();

  RC rc = optimize_loop(root_id);
  if(OB_FAIL(rc)) {
    return rc;
  }
  LOG_TRACE("after optimize, memo dump:");
  context_->get_memo().dump();
  return choose_best_plan(root_id, physical_operator);
}

RC Optimizer::choose_best_plan(int root_group_id, std::unique_ptr<PhysicalOperator> &physical_operator)
{
  auto &memo = context_->get_memo();
  Group *root_group = memo.get_group_by_id(root_group_id);
  if(root_group == nullptr) {
    LOG_ERROR("Root group should not be null");
    return RC::OPTIMIZER_INVALID_GROUP_ID;
  }

  // Choose the best physical plan
  auto winner = root_group->get_winner();
  if (winner == nullptr) {
    LOG_WARN("No winner found in group %d", root_group_id);
    return RC::OPTIMIZER_MEMO_INSERT_FAILED;
  }
  auto winner_contents = winner->release_op();
  PhysicalOperator* winner_phys = static_cast<PhysicalOperator*>(winner_contents);
  LOG_TRACE("winner: %d", winner_phys->get_op_type());
  for (const auto& child : winner->get_child_group_ids()) {
    std::unique_ptr<PhysicalOperator> child_operator;
    RC rc = choose_best_plan(child, child_operator);
    if(OB_FAIL(rc)) {
      return rc;
    }
    winner_phys->add_child(std::move(child_operator));
  }
  physical_operator.reset(winner_phys);
  return RC::SUCCESS;
}

RC Optimizer::optimize_loop(int root_group_id)
{
  auto task_stack = new PendingTasks();
  context_->set_task_pool(task_stack);

  Memo &memo = context_->get_memo();
  task_stack->push(new OptimizeGroup(memo.get_group_by_id(root_group_id), context_.get()));

  return execute_task_stack(task_stack, root_group_id, context_.get());
}

RC Optimizer::execute_task_stack(PendingTasks *task_stack, int root_group_id, OptimizerContext *root_context)
{
  RC rc = RC::SUCCESS;
  while (!task_stack->empty()) {
    auto task = task_stack->pop();
    if(OB_SUCC(rc)) {
      rc = task->perform();
    }
    delete task;
  }
  return rc;
}

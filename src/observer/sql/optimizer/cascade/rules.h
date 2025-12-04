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

#include "common/lang/unordered_map.h"
#include "sql/optimizer/cascade/optimizer_context.h"
#include "sql/optimizer/cascade/pattern.h"

/**
 * @brief: Enum defining the types of rules
 */
enum class RuleType : uint32_t
{
  // Transformation rules (logical -> logical)
  PREDICATE_PUSHDOWN,   // 谓词下推
  PREDICATE_REWRITE,    // 谓词重写（删除恒真/恒假谓词）
  EXPRESSION_SIMPLIFY,  // 表达式简化

  // Don't move this one
  LogicalPhysicalDelimiter,

  // Implementation rules (logical -> physical)
  GET_TO_SEQ_SCAN,
  GET_TO_INDEX_SCAN,
  DELETE_TO_PHYSICAL,
  UPDATE_TO_PHYSICAL,
  INSERT_TO_PHYSICAL,
  AGGREGATE_TO_PHYSICAL,
  INNER_JOIN_TO_NL_JOIN,
  INNER_JOIN_TO_HASH_JOIN,
  IMPLEMENT_LIMIT,
  PROJECTION_TO_PHYSOCAL,
  ANALYZE_TO_PHYSICAL,
  EXPLAIN_TO_PHYSICAL,
  CALC_TO_PHYSICAL,
  PREDICATE_TO_PHYSICAL,
  GROUP_BY_TO_PHYSICAL_AGGREGATION,
  GROUP_BY_TO_PHYSICL_HASH_GROUP_BY,
  EMPTY_TO_PHYSICAL,

  NUM_RULES
};

/**
 * Enum defining set of rules
 */
enum class RuleSetName : uint32_t
{
  LOGICAL_TRANSFORMATION,  // logical -> logical
  PHYSICAL_IMPLEMENTATION  // logical -> physical
};

/**
 * Enum defining rule promises
 * Higher promise means the rule should be applied sooner.
 * Since we use a stack (LIFO), rules with higher promise are pushed later and executed first.
 * We want TRANSFORMATION rules (logical -> logical) to execute before IMPLEMENTATION rules (logical -> physical),
 * so TRANSFORMATION rules should have higher promise.
 */
enum class RulePromise : uint32_t
{

  /**
   * Physical rule (implementation rules)
   * Lower promise, will be pushed first, executed later
   */
  PHYSICAL_PROMISE = 0,

  /**
   * Logical rule (transformation rules)
   * Higher promise, will be pushed later, executed first
   */
  LOGICAL_PROMISE = 1
};

struct CandidateExpression
{
  std::unique_ptr<OperatorNode> op;
  std::vector<int>              child_group_ids;

  CandidateExpression(std::unique_ptr<OperatorNode> node, std::vector<int> children = std::vector<int>())
      : op(std::move(node)), child_group_ids(std::move(children))
  {}
};

/**
 * TODO: Distinguish from logical rules and unify logical rewrite rule
 */
class Rule
{
public:
  virtual ~Rule() {}
  /**
   * Gets the match pattern for the rule
   * @returns match pattern
   */
  Pattern *get_match_pattern() const { return match_pattern_.get(); }

  /**
   * @returns whether the rule is a physical transformation
   */
  bool is_physical() const { return type_ > RuleType::LogicalPhysicalDelimiter; }

  /**
   * @returns whether the rule is a logical rule
   */
  bool is_logical() const { return type_ < RuleType::LogicalPhysicalDelimiter; }

  /**
   * @returns the type of the rule
   */
  RuleType get_type() { return type_; }

  /**
   * @returns index of the rule for bitmask
   */
  uint32_t get_rule_idx() { return static_cast<uint32_t>(type_); }

  /**
   * Get the promise of the current rule for a expression in the current
   * context.
   *
   * @return The promise, the higher the promise, the rule should be applied sooner
   */
  virtual RulePromise promise(GroupExpr *group_expr) const
  {
    if (is_physical())
      return RulePromise::PHYSICAL_PROMISE;
    return RulePromise::LOGICAL_PROMISE;
  }

  /**
   * Convert a "before" operator tree to an "after" operator tree
   *
   * @param input The "before" operator tree
   * @param transformed Vector of "after" operator trees
   * @param context The current optimization context
   */
  virtual void transform(
      GroupExpr *input, std::vector<CandidateExpression> *transformed, OptimizerContext *context) const = 0;

protected:
  RuleType            type_;
  unique_ptr<Pattern> match_pattern_;
};

class RuleWithPromise
{
public:
  RuleWithPromise(Rule *rule, RulePromise promise) : rule_(rule), promise_(promise) {}

  Rule *get_rule() { return rule_; }

  /**
   * Gets the promise
   * @returns Promise
   */
  RulePromise get_promise() { return promise_; }

  bool operator<(const RuleWithPromise &r) const
  {
    // Compare promise first, higher promise executes first
    if (promise_ != r.promise_) {
      return promise_ < r.promise_;
    }
    // Using stack (LIFO), higher type values are pushed later and execute first
    // Execution order: EXPRESSION_SIMPLIFY (2) -> PREDICATE_REWRITE (1) -> PREDICATE_PUSHDOWN (0)
    return static_cast<uint32_t>(rule_->get_type()) < static_cast<uint32_t>(r.rule_->get_type());
  }

  bool operator>(const RuleWithPromise &r) const
  {
    // Compare promise first, higher promise executes first
    if (promise_ != r.promise_) {
      return promise_ > r.promise_;
    }
    // When promise is equal, compare rule type in descending order
    return static_cast<uint32_t>(rule_->get_type()) > static_cast<uint32_t>(r.rule_->get_type());
  }

private:
  /**
   * Rule
   */
  Rule *rule_;

  /**
   * Promise
   */
  RulePromise promise_;
};

class RuleSet
{
public:
  RuleSet();

  ~RuleSet()
  {
    for (auto &it : rules_map_) {
      for (auto rule : it.second) {
        delete rule;
      }
    }
  }

  /**
   * Adds a rule to the RuleSet
   */
  void add_rule(RuleSetName set, Rule *rule) { rules_map_[static_cast<uint32_t>(set)].push_back(rule); }

  /**
   * Gets all stored rules in a given RuleSet
   */
  std::vector<Rule *> &get_rules_by_name(RuleSetName set) { return rules_map_[static_cast<uint32_t>(set)]; }

private:
  /**
   * Map from RuleSetName (uint32_t) -> vector of rules
   * TODO: use unique_ptr
   */
  std::unordered_map<uint32_t, std::vector<Rule *>> rules_map_;
};
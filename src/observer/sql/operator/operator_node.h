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

#include <stdint.h>
#include "common/lang/vector.h"
#include "common/lang/memory.h"
#include "sql/optimizer/cascade/property.h"
#include "sql/optimizer/cascade/cost_model.h"
/**
 * @brief Operator type(including logical and physical)
 */
enum class OpType
{
  UNDEFINED = 0,

  // Special wildcard
  LEAF,

  // Logical Operators
  LOGICALGET,
  LOGICALCALCULATE,
  LOGICALGROUPBY,
  LOGICALPROJECTION,
  LOGICALFILTER,
  LOGICALINNERJOIN,
  LOGICALINSERT,
  LOGICALDELETE,
  LOGICALUPDATE,
  LOGICALLIMIT,
  LOGICALANALYZE,
  LOGICALEXPLAIN,
  LOGICALEMPTY,
  // Separation of logical and physical operators
  LOGICALPHYSICALDELIMITER,

  // Physical Operators
  EXPLAIN,
  CALCULATE,
  SEQSCAN,
  INDEXSCAN,
  ORDERBY,
  LIMIT,
  INNERINDEXJOIN,
  INNERNLJOIN,
  INNERHASHJOIN,
  PROJECTION,
  INSERT,
  DELETE,
  UPDATE,
  AGGREGATE,
  HASHGROUPBY,
  ANALYZE,
  FILTER,
  EMPTY,
  SCALARGROUPBY,
  STRINGLIST,
  AGGREGATE_VEC,
  EXPR_VEC,
  GROUPBY_VEC,
  PROJECTION_VEC,
  SEQSCAN_VEC
};

// OperatorNode is the abstract class of logical/physical operator
// In cascade optimizer, children are managed by GroupExpr through child_group_ids,
// not by OperatorNode itself.
class OperatorNode
{
public:
  virtual ~OperatorNode() = default;
  /**
   * TODO: add this function
   */
  // virtual std::string get_name() const = 0;

  virtual OpType get_op_type() const { return OpType::UNDEFINED; }

  /**
   * @return Whether node contents represent a physical operator / expression
   */
  virtual bool is_physical() const = 0;

  /**
   * @return Whether node represents a logical operator / expression
   */
  virtual bool is_logical() const = 0;

  /**
   * TODO: complete it if needed
   */
  virtual uint64_t hash() const { return std::hash<int>()(static_cast<int>(get_op_type())); }
  virtual bool     operator==(const OperatorNode &other) const
  {
    // 有额外成员的算子需要重写此方法来比较成员
    return get_op_type() == other.get_op_type();
  }
  /**
   * @brief Generate the logical property of the operator node using the input logical properties.
   * @param log_props Input logical properties of the operator node.
   * @return Logical property of the operator node.
   */
  virtual unique_ptr<LogicalProperty> find_log_prop(const vector<LogicalProperty *> &log_props) { return nullptr; }

  /**
   * @brief Calculates the cost of a logical operation.
   *
   * This function is intended to be overridden in derived classes. It calculates
   * the cost associated with a specific logical property, taking into account the
   * provided child logical properties and a cost model.
   *
   * @param prop A pointer to the logical property for which the cost is being calculated.
   * @param child_log_props A vector containing pointers to child logical properties.
   * @param cm A pointer to the cost model used for calculating the cost.
   * @return The calculated cost as a double.
   * 
   * @note Default implementation: uses CPU operation cost based on input cardinality.
   *       If there are child operators, uses the first child's cardinality.
   *       Otherwise, uses the current operator's cardinality.
   */
  virtual double calculate_cost(LogicalProperty *prop, const vector<LogicalProperty *> &child_log_props, CostModel *cm)
  {
    // Default cost: CPU operation cost based on input cardinality
    // For operators with children, use the first child's cardinality (input size)
    // For leaf operators, use the current operator's cardinality
    int card = 0;
    if (!child_log_props.empty() && child_log_props[0] != nullptr) {
      card = child_log_props[0]->get_card();
    } else if (prop != nullptr) {
      card = prop->get_card();
    }
    // If cardinality is 0, use a minimum cost to avoid zero cost
    // This ensures that even empty tables/operators have some cost
    if (card == 0) {
      card = 1;  // Use minimum cardinality of 1 for cost calculation
    }
    // Use CPU operation cost as default, similar to ProjectPhysicalOperator
    return cm->cpu_op() * card;
  }
};
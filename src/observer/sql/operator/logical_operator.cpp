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
// Created by Wangyunlai on 2022/12/08.
//

#include "sql/operator/logical_operator.h"

LogicalOperator::~LogicalOperator() {}

void LogicalOperator::add_expressions(unique_ptr<Expression> expr) { 
  expressions_.emplace_back(std::move(expr)); 
}

bool LogicalOperator::can_generate_vectorized_operator(OpType type)
{
  bool bool_ret = false;
  switch (type)
  {
  case OpType::LOGICALCALCULATE:
  case OpType::LOGICALDELETE:
  case OpType::LOGICALINSERT:
    bool_ret = false;
    break;
  
  default:
    bool_ret = true;
    break;
  }
  return bool_ret;
}

string logical_operator_type_name(OpType type)
{
  switch (type) {
    case OpType::LOGICALGET: return "LOGICAL_GET";
    case OpType::LOGICALCALCULATE: return "LOGICAL_CALCULATE";
    case OpType::LOGICALGROUPBY: return "LOGICAL_GROUP_BY";
    case OpType::LOGICALPROJECTION: return "LOGICAL_PROJECTION";
    case OpType::LOGICALFILTER: return "LOGICAL_FILTER";
    case OpType::LOGICALINNERJOIN: return "LOGICAL_INNER_JOIN";
    case OpType::LOGICALINSERT: return "LOGICAL_INSERT";
    case OpType::LOGICALDELETE: return "LOGICAL_DELETE";
    case OpType::LOGICALUPDATE: return "LOGICAL_UPDATE";
    case OpType::LOGICALLIMIT: return "LOGICAL_LIMIT";
    case OpType::LOGICALANALYZE: return "LOGICAL_ANALYZE";
    case OpType::LOGICALEXPLAIN: return "LOGICAL_EXPLAIN";
    case OpType::LOGICALEMPTY: return "LOGICAL_EMPTY";
    default: return "UNKNOWN_LOGICAL";
  }
}

string LogicalOperator::name() const { return logical_operator_type_name(get_op_type()); }

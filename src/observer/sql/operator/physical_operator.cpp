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
// Created by WangYunlai on 2022/11/18.
//

#include "sql/operator/physical_operator.h"

string physical_operator_type_name(OpType type)
{
  switch (type) {
    case OpType::SEQSCAN: return "TABLE_SCAN";
    case OpType::INDEXSCAN: return "INDEX_SCAN";
    case OpType::INNERNLJOIN: return "NESTED_LOOP_JOIN";
    case OpType::INNERHASHJOIN: return "HASH_JOIN";
    case OpType::EXPLAIN: return "EXPLAIN";
    case OpType::FILTER: return "PREDICATE";
    case OpType::INSERT: return "INSERT";
    case OpType::DELETE: return "DELETE";
    case OpType::PROJECTION: return "PROJECT";
    case OpType::STRINGLIST: return "STRING_LIST";
    case OpType::HASHGROUPBY: return "HASH_GROUP_BY";
    case OpType::SCALARGROUPBY: return "SCALAR_GROUP_BY";
    case OpType::AGGREGATE_VEC: return "AGGREGATE_VEC";
    case OpType::GROUPBY_VEC: return "GROUP_BY_VEC";
    case OpType::PROJECTION_VEC: return "PROJECT_VEC";
    case OpType::SEQSCAN_VEC: return "TABLE_SCAN_VEC";
    case OpType::EXPR_VEC: return "EXPR_VEC";
    default: return "UNKNOWN";
  }
}

string PhysicalOperator::name() const { return physical_operator_type_name(get_op_type()); }

string PhysicalOperator::param() const { return ""; }

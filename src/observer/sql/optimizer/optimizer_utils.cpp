/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/optimizer/optimizer_utils.h"
#include "sql/expr/expression.h"
#include "sql/parser/parse_defs.h"

using namespace std;

static string comp_op_to_string(CompOp op)
{
  switch (op) {
    case CompOp::EQUAL_TO: return "=";
    case CompOp::LESS_EQUAL: return "<=";
    case CompOp::NOT_EQUAL: return "<>";
    case CompOp::LESS_THAN: return "<";
    case CompOp::GREAT_EQUAL: return ">=";
    case CompOp::GREAT_THAN: return ">";
    case CompOp::NO_OP: return "";
    default: return "?";
  }
}

string OptimizerUtils::expression_to_string(const Expression *expr)
{
  if (expr == nullptr) {
    return "";
  }

  switch (expr->type()) {
    case ExprType::FIELD: {
      const auto *field_expr = static_cast<const FieldExpr *>(expr);
      string result;
      if (field_expr->table_name() && strlen(field_expr->table_name()) > 0) {
        result = string(field_expr->table_name()) + ".";
      }
      result += field_expr->field_name();
      return result;
    }
    case ExprType::VALUE: {
      const auto *value_expr = static_cast<const ValueExpr *>(expr);
      Value value = value_expr->get_value();
      return value.to_string();
    }
    case ExprType::COMPARISON: {
      const auto *comp_expr = static_cast<const ComparisonExpr *>(expr);
      string left_str = OptimizerUtils::expression_to_string(comp_expr->left().get());
      string right_str = OptimizerUtils::expression_to_string(comp_expr->right().get());
      string op_str = comp_op_to_string(comp_expr->comp());
      return left_str + " " + op_str + " " + right_str;
    }
    case ExprType::CONJUNCTION: {
      const auto *conj_expr = static_cast<const ConjunctionExpr *>(expr);
      string op_str = (conj_expr->conjunction_type() == ConjunctionExpr::Type::AND) ? "AND" : "OR";
      string result;
      const auto &children = conj_expr->children();
      for (size_t i = 0; i < children.size(); i++) {
        if (i > 0) {
          result += " " + op_str + " ";
        }
        result += "(" + OptimizerUtils::expression_to_string(children[i].get()) + ")";
      }
      return result;
    }
    case ExprType::ARITHMETIC: {
      const auto *arith_expr = static_cast<const ArithmeticExpr *>(expr);
      string left_str = OptimizerUtils::expression_to_string(arith_expr->left().get());
      string op_str;
      switch (arith_expr->arithmetic_type()) {
        case ArithmeticExpr::Type::ADD: op_str = "+"; break;
        case ArithmeticExpr::Type::SUB: op_str = "-"; break;
        case ArithmeticExpr::Type::MUL: op_str = "*"; break;
        case ArithmeticExpr::Type::DIV: op_str = "/"; break;
        case ArithmeticExpr::Type::NEGATIVE: return "-" + left_str;
        default: op_str = "?"; break;
      }
      if (arith_expr->right()) {
        string right_str = OptimizerUtils::expression_to_string(arith_expr->right().get());
        return "(" + left_str + " " + op_str + " " + right_str + ")";
      } else {
        return op_str + "(" + left_str + ")";
      }
    }
    case ExprType::CAST: {
      const auto *cast_expr = static_cast<const CastExpr *>(expr);
      string child_str = OptimizerUtils::expression_to_string(cast_expr->child().get());
      return "CAST(" + child_str + ")";
    }
    default: {
      const char *name = expr->name();
      return name ? string(name) : "?";
    }
  }
}

string OptimizerUtils::dump_physical_plan(const unique_ptr<PhysicalOperator>& children)
{
  std::function<void(ostream &, PhysicalOperator *, int, bool, vector<uint8_t> &)> to_string = [&](
    ostream &os, PhysicalOperator *oper, int level, bool last_child, vector<uint8_t> &ends)
  {
    for (int i = 0; i < level - 1; i++) {
      if (ends[i]) {
        os << "  ";
      } else {
        os << "│ ";
      }
    }
    if (level > 0) {
      if (last_child) {
        os << "└─";
        ends[level - 1] = 1;
      } else {
        os << "├─";
      }
    }

    os << oper->name();
    string param = oper->param();
    if (!param.empty()) {
      os << "(" << param << ")";
    }
    os << '\n';

    if (static_cast<int>(ends.size()) < level + 2) {
      ends.resize(level + 2);
    }
    ends[level + 1] = 0;

    vector<unique_ptr<PhysicalOperator>> &children = oper->children();
    const auto                                 size     = static_cast<int>(children.size());
    for (auto i = 0; i < size - 1; i++) {
      to_string(os, children[i].get(), level + 1, false /*last_child*/, ends);
    }
    if (size > 0) {
      to_string(os, children[size - 1].get(), level + 1, true /*last_child*/, ends);
    }
  };
  stringstream ss;
  ss << "OPERATOR(NAME)\n";

  int               level = 0;
  vector<uint8_t> ends;
  ends.push_back(true);
  to_string(ss, children.get(), level, true /*last_child*/, ends);

  return ss.str();
}
#include "executor/expr_eval.hpp"

#include <optional>
#include <variant>

namespace minidb::executor {

using minidb::common::IsNull;
using minidb::common::Value;
using minidb::parser::BinaryExpr;
using minidb::parser::BinaryOp;
using minidb::parser::ColumnRefExpr;
using minidb::parser::Expr;
using minidb::parser::LiteralExpr;

namespace {

std::optional<double> ToDouble(const Value& v) {
  if (std::holds_alternative<std::int32_t>(v)) return static_cast<double>(std::get<std::int32_t>(v));
  if (std::holds_alternative<std::int64_t>(v)) return static_cast<double>(std::get<std::int64_t>(v));
  if (std::holds_alternative<double>(v)) return std::get<double>(v);
  return std::nullopt;
}

util::Result<bool> ValuesEqual(const Value& a, const Value& b) {
  if (IsNull(a) || IsNull(b)) return util::Result<bool>(false);  

  auto da = ToDouble(a), db = ToDouble(b);
  if (da.has_value() && db.has_value()) return util::Result<bool>(*da == *db);

  if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
    return util::Result<bool>(std::get<std::string>(a) == std::get<std::string>(b));
  }
  if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
    return util::Result<bool>(std::get<bool>(a) == std::get<bool>(b));
  }
  return util::Status::InvalidArgument("cannot compare values of incompatible types");
}

util::Result<int> CompareOrdered(const Value& a, const Value& b) {
  auto da = ToDouble(a), db = ToDouble(b);
  if (da.has_value() && db.has_value()) {
    if (*da < *db) return util::Result<int>(-1);
    if (*da > *db) return util::Result<int>(1);
    return util::Result<int>(0);
  }
  if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
    const std::string& sa = std::get<std::string>(a);
    const std::string& sb = std::get<std::string>(b);
    if (sa < sb) return util::Result<int>(-1);
    if (sa > sb) return util::Result<int>(1);
    return util::Result<int>(0);
  }
  if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
    bool ba = std::get<bool>(a), bb = std::get<bool>(b);
    if (ba == bb) return util::Result<int>(0);
    return util::Result<int>(ba ? 1 : -1);
  }
  return util::Status::InvalidArgument("cannot order-compare values of incompatible types");
}

}  

util::Result<Value> Evaluate(const Expr& expr, const minidb::common::Schema& schema,
                              const std::vector<Value>& row) {
  if (const auto* col = std::get_if<ColumnRefExpr>(&expr.node)) {
    auto idx = schema.IndexOf(col->column);
    if (!idx.has_value()) {
      return util::Status::InvalidArgument("unknown column '" + col->column + "' in expression");
    }
    return util::Result<Value>(row[*idx]);
  }

  if (const auto* lit = std::get_if<LiteralExpr>(&expr.node)) {
    return util::Result<Value>(lit->value);
  }

  const auto& bin = std::get<BinaryExpr>(expr.node);

  if (bin.op == BinaryOp::kAnd || bin.op == BinaryOp::kOr) {
    auto left = Evaluate(*bin.left, schema, row);
    if (!left.ok()) return left.status();
    if (!std::holds_alternative<bool>(left.value())) {
      return util::Status::InvalidArgument("left side of AND/OR did not evaluate to a boolean");
    }
    bool lb = std::get<bool>(left.value());


    if (bin.op == BinaryOp::kOr && lb) return util::Result<Value>(Value(true));
    if (bin.op == BinaryOp::kAnd && !lb) return util::Result<Value>(Value(false));

    auto right = Evaluate(*bin.right, schema, row);
    if (!right.ok()) return right.status();
    if (!std::holds_alternative<bool>(right.value())) {
      return util::Status::InvalidArgument("right side of AND/OR did not evaluate to a boolean");
    }
    bool rb = std::get<bool>(right.value());
    return util::Result<Value>(Value(bin.op == BinaryOp::kAnd ? (lb && rb) : (lb || rb)));
  }

  auto left = Evaluate(*bin.left, schema, row);
  if (!left.ok()) return left.status();
  auto right = Evaluate(*bin.right, schema, row);
  if (!right.ok()) return right.status();

  if (bin.op == BinaryOp::kEq || bin.op == BinaryOp::kNeq) {
    auto eq = ValuesEqual(left.value(), right.value());
    if (!eq.ok()) return eq.status();
    return util::Result<Value>(Value(bin.op == BinaryOp::kEq ? eq.value() : !eq.value()));
  }

  if (IsNull(left.value()) || IsNull(right.value())) return util::Result<Value>(Value(false));

  auto cmp = CompareOrdered(left.value(), right.value());
  if (!cmp.ok()) return cmp.status();
  int c = cmp.value();
  bool result = false;
  switch (bin.op) {
    case BinaryOp::kLt: result = c < 0; break;
    case BinaryOp::kLte: result = c <= 0; break;
    case BinaryOp::kGt: result = c > 0; break;
    case BinaryOp::kGte: result = c >= 0; break;
    default: return util::Status::InvalidArgument("unsupported comparison operator");
  }
  return util::Result<Value>(Value(result));
}

util::Result<bool> EvaluatePredicate(const Expr* expr, const minidb::common::Schema& schema,
                                      const std::vector<Value>& row) {
  if (expr == nullptr) return util::Result<bool>(true);  
  auto result = Evaluate(*expr, schema, row);
  if (!result.ok()) return result.status();
  if (!std::holds_alternative<bool>(result.value())) {
    return util::Status::InvalidArgument("WHERE clause did not evaluate to a boolean");
  }
  return util::Result<bool>(std::get<bool>(result.value()));
}

}  

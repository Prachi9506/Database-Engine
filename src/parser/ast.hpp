#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "common/schema.hpp"
#include "common/value.hpp"

namespace minidb::parser {


enum class BinaryOp { kEq, kNeq, kLt, kLte, kGt, kGte, kAnd, kOr };

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct ColumnRefExpr {
  std::string column;
};

struct LiteralExpr {

  minidb::common::Value value;
};

struct BinaryExpr {
  ExprPtr left;
  BinaryOp op;
  ExprPtr right;
};

struct Expr {
  std::variant<ColumnRefExpr, LiteralExpr, BinaryExpr> node;
};

inline ExprPtr MakeColumnRef(std::string col) {
  return std::make_unique<Expr>(Expr{ColumnRefExpr{std::move(col)}});
}
inline ExprPtr MakeLiteral(minidb::common::Value v) {
  return std::make_unique<Expr>(Expr{LiteralExpr{std::move(v)}});
}
inline ExprPtr MakeBinary(ExprPtr left, BinaryOp op, ExprPtr right) {
  return std::make_unique<Expr>(Expr{BinaryExpr{std::move(left), op, std::move(right)}});
}


enum class AggregateFunc { kCount, kSum, kAvg, kMin, kMax };

struct SelectItem {
  bool is_star = false;                          
  std::optional<std::string> column;              
  std::optional<AggregateFunc> aggregate;          
  bool aggregate_star = false;
  std::optional<std::string> alias;                
};

struct OrderByClause {
  std::string column;
  bool descending = false;
};


struct CreateTableStmt {
  std::string table_name;
  std::vector<minidb::common::Column> columns;
};

struct DropTableStmt {
  std::string table_name;
};

struct CreateIndexStmt {
  std::string index_name;
  std::string table_name;
  std::string column_name;
};

struct DropIndexStmt {
  std::string index_name;
};

struct InsertStmt {
  std::string table_name;
  std::vector<std::string> columns;  
  std::vector<std::vector<minidb::common::Value>> rows;
};

struct UpdateStmt {
  std::string table_name;
  std::vector<std::pair<std::string, minidb::common::Value>> assignments;
  ExprPtr where;  
};

struct DeleteStmt {
  std::string table_name;
  ExprPtr where;  
};

struct SelectStmt {
  std::vector<SelectItem> items;
  std::string table_name;
  ExprPtr where;  
  std::optional<OrderByClause> order_by;
  std::optional<std::int64_t> limit;
};

using Statement = std::variant<CreateTableStmt, DropTableStmt, CreateIndexStmt, DropIndexStmt, InsertStmt,
                                UpdateStmt, DeleteStmt, SelectStmt>;

}  

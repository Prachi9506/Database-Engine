#include "parser/parser.hpp"

#include <stdexcept>

#include "parser/tokenizer.hpp"

namespace minidb::parser {

using minidb::common::Column;
using minidb::common::ColumnType;
using minidb::common::Value;

util::Status Parser::Expect(TokenType t, const std::string& context) {
  if (!Check(t)) {
    return util::Status::InvalidArgument("expected " + std::string(TokenTypeName(t)) + " in " + context +
                                          ", got " + TokenTypeName(Peek().type) +
                                          (Peek().lexeme.empty() ? "" : " ('" + Peek().lexeme + "')"));
  }
  Advance();
  return util::Status::OK();
}

util::Result<Statement> Parser::ParseStatement() {
  if (Check(TokenType::kEndOfInput)) {
    return util::Status::InvalidArgument("empty statement");
  }

  util::Result<Statement> result = util::Status::InvalidArgument("unreachable");
  switch (Peek().type) {
    case TokenType::kSelect: Advance(); result = ParseSelect(); break;
    case TokenType::kInsert: Advance(); result = ParseInsert(); break;
    case TokenType::kUpdate: Advance(); result = ParseUpdate(); break;
    case TokenType::kDelete: Advance(); result = ParseDelete(); break;
    case TokenType::kCreate: {
      Advance();
      if (Check(TokenType::kTable)) result = ParseCreateTable();
      else if (Check(TokenType::kIndex)) result = ParseCreateIndex();
      else return util::Status::InvalidArgument("expected TABLE or INDEX after CREATE, got " +
                                                 std::string(TokenTypeName(Peek().type)));
      break;
    }
    case TokenType::kDrop: {
      Advance();
      if (Check(TokenType::kTable)) result = ParseDropTable();
      else if (Check(TokenType::kIndex)) result = ParseDropIndex();
      else return util::Status::InvalidArgument("expected TABLE or INDEX after DROP, got " +
                                                 std::string(TokenTypeName(Peek().type)));
      break;
    }
    default:
      return util::Status::InvalidArgument(
          "expected a statement (SELECT/INSERT/UPDATE/DELETE/CREATE/DROP), got " +
          std::string(TokenTypeName(Peek().type)));
  }
  if (!result.ok()) return result.status();

  Match(TokenType::kSemicolon);
  if (!Check(TokenType::kEndOfInput)) {
    return util::Status::InvalidArgument("unexpected trailing tokens starting at '" + Peek().lexeme + "'");
  }
  return result;
}


util::Result<Column> Parser::ParseColumnDef() {
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected column name, got " + std::string(TokenTypeName(Peek().type)));
  }
  std::string name = Advance().lexeme;

  ColumnType type;
  std::uint16_t max_length = 255;
  switch (Peek().type) {
    case TokenType::kInt: Advance(); type = ColumnType::kInt; break;
    case TokenType::kBigInt: Advance(); type = ColumnType::kBigInt; break;
    case TokenType::kDouble: Advance(); type = ColumnType::kDouble; break;
    case TokenType::kBoolean: Advance(); type = ColumnType::kBool; break;
    case TokenType::kVarchar: {
      Advance();
      type = ColumnType::kVarchar;
      if (Match(TokenType::kLParen)) {
        if (!Check(TokenType::kIntLiteral)) {
          return util::Status::InvalidArgument("expected integer length in VARCHAR(n)");
        }
        try {
          max_length = static_cast<std::uint16_t>(std::stoul(Advance().lexeme));
        } catch (const std::exception&) {
          return util::Status::InvalidArgument("invalid VARCHAR length");
        }
        util::Status st = Expect(TokenType::kRParen, "VARCHAR length");
        if (!st.ok()) return st;
      }
      break;
    }
    default:
      return util::Status::InvalidArgument("expected a column type (INT/BIGINT/DOUBLE/BOOLEAN/VARCHAR), got " +
                                            std::string(TokenTypeName(Peek().type)));
  }

  bool nullable = true;
  if (Match(TokenType::kNot)) {
    util::Status st = Expect(TokenType::kNull, "NOT");
    if (!st.ok()) return st;
    nullable = false;
  }

  return util::Result<Column>(Column{name, type, nullable, max_length});
}

util::Result<Statement> Parser::ParseCreateTable() {
  util::Status st = Expect(TokenType::kTable, "CREATE TABLE");
  if (!st.ok()) return st;

  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after CREATE TABLE");
  }
  std::string table_name = Advance().lexeme;

  st = Expect(TokenType::kLParen, "CREATE TABLE");
  if (!st.ok()) return st;

  std::vector<Column> columns;
  do {
    auto col = ParseColumnDef();
    if (!col.ok()) return col.status();
    columns.push_back(col.value());
  } while (Match(TokenType::kComma));

  st = Expect(TokenType::kRParen, "CREATE TABLE column list");
  if (!st.ok()) return st;

  if (columns.empty()) {
    return util::Status::InvalidArgument("CREATE TABLE requires at least one column");
  }

  return util::Result<Statement>(Statement{CreateTableStmt{table_name, std::move(columns)}});
}


util::Result<Statement> Parser::ParseDropTable() {
  util::Status st = Expect(TokenType::kTable, "DROP TABLE");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after DROP TABLE");
  }
  std::string table_name = Advance().lexeme;
  return util::Result<Statement>(Statement{DropTableStmt{table_name}});
}


util::Result<Statement> Parser::ParseCreateIndex() {
  util::Status st = Expect(TokenType::kIndex, "CREATE INDEX");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected index name after CREATE INDEX");
  }
  std::string index_name = Advance().lexeme;

  st = Expect(TokenType::kOn, "CREATE INDEX");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after ON");
  }
  std::string table_name = Advance().lexeme;

  st = Expect(TokenType::kLParen, "CREATE INDEX");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected column name in CREATE INDEX");
  }
  std::string column_name = Advance().lexeme;
  st = Expect(TokenType::kRParen, "CREATE INDEX");
  if (!st.ok()) return st;

  return util::Result<Statement>(Statement{CreateIndexStmt{index_name, table_name, column_name}});
}

util::Result<Statement> Parser::ParseDropIndex() {
  util::Status st = Expect(TokenType::kIndex, "DROP INDEX");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected index name after DROP INDEX");
  }
  std::string index_name = Advance().lexeme;
  return util::Result<Statement>(Statement{DropIndexStmt{index_name}});
}


util::Result<Value> Parser::ParseLiteral() {
  if (Match(TokenType::kNull)) return util::Result<Value>(Value(std::monostate{}));
  if (Match(TokenType::kTrue)) return util::Result<Value>(Value(true));
  if (Match(TokenType::kFalse)) return util::Result<Value>(Value(false));

  if (Check(TokenType::kIntLiteral)) {
    std::string lexeme = Advance().lexeme;
    try {
      return util::Result<Value>(Value(static_cast<std::int64_t>(std::stoll(lexeme))));
    } catch (const std::exception&) {
      return util::Status::InvalidArgument("invalid integer literal '" + lexeme + "'");
    }
  }
  if (Check(TokenType::kFloatLiteral)) {
    std::string lexeme = Advance().lexeme;
    try {
      return util::Result<Value>(Value(std::stod(lexeme)));
    } catch (const std::exception&) {
      return util::Status::InvalidArgument("invalid float literal '" + lexeme + "'");
    }
  }
  if (Check(TokenType::kStringLiteral)) {
    return util::Result<Value>(Value(Advance().lexeme));
  }

  return util::Status::InvalidArgument("expected a literal value, got " + std::string(TokenTypeName(Peek().type)));
}


util::Result<Statement> Parser::ParseInsert() {
  util::Status st = Expect(TokenType::kInto, "INSERT");
  if (!st.ok()) return st;

  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after INSERT INTO");
  }
  std::string table_name = Advance().lexeme;

  std::vector<std::string> columns;
  if (Match(TokenType::kLParen)) {
    do {
      if (!Check(TokenType::kIdentifier)) {
        return util::Status::InvalidArgument("expected column name in INSERT column list");
      }
      columns.push_back(Advance().lexeme);
    } while (Match(TokenType::kComma));
    st = Expect(TokenType::kRParen, "INSERT column list");
    if (!st.ok()) return st;
  }

  st = Expect(TokenType::kValues, "INSERT");
  if (!st.ok()) return st;

  std::vector<std::vector<Value>> rows;
  do {
    st = Expect(TokenType::kLParen, "INSERT VALUES");
    if (!st.ok()) return st;
    std::vector<Value> row;
    do {
      auto lit = ParseLiteral();
      if (!lit.ok()) return lit.status();
      row.push_back(lit.value());
    } while (Match(TokenType::kComma));
    st = Expect(TokenType::kRParen, "INSERT VALUES tuple");
    if (!st.ok()) return st;
    rows.push_back(std::move(row));
  } while (Match(TokenType::kComma));

  return util::Result<Statement>(Statement{InsertStmt{table_name, std::move(columns), std::move(rows)}});
}


util::Result<ExprPtr> Parser::ParsePrimary() {
  if (Match(TokenType::kLParen)) {
    auto inner = ParseOrExpr();
    if (!inner.ok()) return inner.status();
    util::Status st = Expect(TokenType::kRParen, "parenthesized expression");
    if (!st.ok()) return st;
    return util::Result<ExprPtr>(std::move(inner.value()));
  }
  if (Check(TokenType::kIdentifier)) {
    return util::Result<ExprPtr>(MakeColumnRef(Advance().lexeme));
  }
  auto lit = ParseLiteral();
  if (!lit.ok()) return lit.status();
  return util::Result<ExprPtr>(MakeLiteral(lit.value()));
}

util::Result<ExprPtr> Parser::ParseComparison() {
  auto left = ParsePrimary();
  if (!left.ok()) return left.status();
  ExprPtr expr = std::move(left.value());

  std::optional<BinaryOp> op;
  switch (Peek().type) {
    case TokenType::kEq: op = BinaryOp::kEq; break;
    case TokenType::kNeq: op = BinaryOp::kNeq; break;
    case TokenType::kLt: op = BinaryOp::kLt; break;
    case TokenType::kLte: op = BinaryOp::kLte; break;
    case TokenType::kGt: op = BinaryOp::kGt; break;
    case TokenType::kGte: op = BinaryOp::kGte; break;
    default: break;
  }
  if (op.has_value()) {
    Advance();
    auto right = ParsePrimary();
    if (!right.ok()) return right.status();
    expr = MakeBinary(std::move(expr), *op, std::move(right.value()));
  }
  return util::Result<ExprPtr>(std::move(expr));
}

util::Result<ExprPtr> Parser::ParseAndExpr() {
  auto left = ParseComparison();
  if (!left.ok()) return left.status();
  ExprPtr expr = std::move(left.value());
  while (Match(TokenType::kAnd)) {
    auto right = ParseComparison();
    if (!right.ok()) return right.status();
    expr = MakeBinary(std::move(expr), BinaryOp::kAnd, std::move(right.value()));
  }
  return util::Result<ExprPtr>(std::move(expr));
}

util::Result<ExprPtr> Parser::ParseOrExpr() {
  auto left = ParseAndExpr();
  if (!left.ok()) return left.status();
  ExprPtr expr = std::move(left.value());
  while (Match(TokenType::kOr)) {
    auto right = ParseAndExpr();
    if (!right.ok()) return right.status();
    expr = MakeBinary(std::move(expr), BinaryOp::kOr, std::move(right.value()));
  }
  return util::Result<ExprPtr>(std::move(expr));
}

util::Result<ExprPtr> Parser::ParseWhereClause() {
  util::Status st = Expect(TokenType::kWhere, "WHERE");
  if (!st.ok()) return st;
  return ParseOrExpr();
}


util::Result<Statement> Parser::ParseUpdate() {
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after UPDATE");
  }
  std::string table_name = Advance().lexeme;

  util::Status st = Expect(TokenType::kSet, "UPDATE");
  if (!st.ok()) return st;

  std::vector<std::pair<std::string, Value>> assignments;
  do {
    if (!Check(TokenType::kIdentifier)) {
      return util::Status::InvalidArgument("expected column name in UPDATE SET clause");
    }
    std::string col = Advance().lexeme;
    st = Expect(TokenType::kEq, "UPDATE SET");
    if (!st.ok()) return st;
    auto lit = ParseLiteral();
    if (!lit.ok()) return lit.status();
    assignments.emplace_back(col, lit.value());
  } while (Match(TokenType::kComma));

  ExprPtr where = nullptr;
  if (Check(TokenType::kWhere)) {
    auto w = ParseWhereClause();
    if (!w.ok()) return w.status();
    where = std::move(w.value());
  }

  return util::Result<Statement>(Statement{UpdateStmt{table_name, std::move(assignments), std::move(where)}});
}


util::Result<Statement> Parser::ParseDelete() {
  util::Status st = Expect(TokenType::kFrom, "DELETE");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after DELETE FROM");
  }
  std::string table_name = Advance().lexeme;

  ExprPtr where = nullptr;
  if (Check(TokenType::kWhere)) {
    auto w = ParseWhereClause();
    if (!w.ok()) return w.status();
    where = std::move(w.value());
  }

  return util::Result<Statement>(Statement{DeleteStmt{table_name, std::move(where)}});
}


namespace {
std::optional<AggregateFunc> AggregateForToken(TokenType t) {
  switch (t) {
    case TokenType::kCount: return AggregateFunc::kCount;
    case TokenType::kSum: return AggregateFunc::kSum;
    case TokenType::kAvg: return AggregateFunc::kAvg;
    case TokenType::kMin: return AggregateFunc::kMin;
    case TokenType::kMax: return AggregateFunc::kMax;
    default: return std::nullopt;
  }
}
}  

util::Result<SelectItem> Parser::ParseSelectItem() {
  auto agg = AggregateForToken(Peek().type);
  if (agg.has_value()) {
    Advance();
    util::Status st = Expect(TokenType::kLParen, "aggregate function");
    if (!st.ok()) return st;

    SelectItem item;
    item.aggregate = agg;
    if (Match(TokenType::kStar)) {
      if (*agg != AggregateFunc::kCount) {
        return util::Status::InvalidArgument("'*' is only valid inside COUNT(...)");
      }
      item.aggregate_star = true;
    } else {
      if (!Check(TokenType::kIdentifier)) {
        return util::Status::InvalidArgument("expected column name or '*' inside aggregate function");
      }
      item.column = Advance().lexeme;
    }
    st = Expect(TokenType::kRParen, "aggregate function");
    if (!st.ok()) return st;

    if (Match(TokenType::kAs)) {
      if (!Check(TokenType::kIdentifier)) return util::Status::InvalidArgument("expected alias name after AS");
      item.alias = Advance().lexeme;
    }
    return util::Result<SelectItem>(item);
  }

  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected a column name or aggregate function in SELECT list, got " +
                                          std::string(TokenTypeName(Peek().type)));
  }
  SelectItem item;
  item.column = Advance().lexeme;
  if (Match(TokenType::kAs)) {
    if (!Check(TokenType::kIdentifier)) return util::Status::InvalidArgument("expected alias name after AS");
    item.alias = Advance().lexeme;
  }
  return util::Result<SelectItem>(item);
}

util::Result<Statement> Parser::ParseSelect() {
  std::vector<SelectItem> items;
  if (Match(TokenType::kStar)) {
    SelectItem star;
    star.is_star = true;
    items.push_back(star);
  } else {
    do {
      auto item = ParseSelectItem();
      if (!item.ok()) return item.status();
      items.push_back(item.value());
    } while (Match(TokenType::kComma));
  }

  util::Status st = Expect(TokenType::kFrom, "SELECT");
  if (!st.ok()) return st;
  if (!Check(TokenType::kIdentifier)) {
    return util::Status::InvalidArgument("expected table name after FROM");
  }
  std::string table_name = Advance().lexeme;

  ExprPtr where = nullptr;
  if (Check(TokenType::kWhere)) {
    auto w = ParseWhereClause();
    if (!w.ok()) return w.status();
    where = std::move(w.value());
  }

  std::optional<OrderByClause> order_by;
  if (Match(TokenType::kOrder)) {
    st = Expect(TokenType::kBy, "ORDER");
    if (!st.ok()) return st;
    if (!Check(TokenType::kIdentifier)) {
      return util::Status::InvalidArgument("expected column name after ORDER BY");
    }
    std::string col = Advance().lexeme;
    bool desc = false;
    if (Match(TokenType::kDesc)) {
      desc = true;
    } else {
      Match(TokenType::kAsc);
    }
    order_by = OrderByClause{col, desc};
  }

  std::optional<std::int64_t> limit;
  if (Match(TokenType::kLimit)) {
    if (!Check(TokenType::kIntLiteral)) {
      return util::Status::InvalidArgument("expected integer after LIMIT");
    }
    try {
      limit = std::stoll(Advance().lexeme);
    } catch (const std::exception&) {
      return util::Status::InvalidArgument("invalid LIMIT value");
    }
  }

  return util::Result<Statement>(
      Statement{SelectStmt{std::move(items), table_name, std::move(where), order_by, limit}});
}


util::Result<Statement> ParseSQL(const std::string& sql) {
  Tokenizer tokenizer(sql);
  auto tokens = tokenizer.TokenizeAll();
  if (!tokens.ok()) return tokens.status();
  Parser parser(std::move(tokens.value()));
  return parser.ParseStatement();
}

}  

#pragma once

#include <vector>

#include "parser/ast.hpp"
#include "parser/token.hpp"
#include "utilities/status.hpp"

namespace minidb::parser {

class Parser {
 public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}


  util::Result<Statement> ParseStatement();

 private:
  const Token& Peek() const { return tokens_[pos_]; }
  const Token& Advance() { return tokens_[pos_++]; }
  bool Check(TokenType t) const { return Peek().type == t; }
  bool Match(TokenType t) {
    if (Check(t)) { pos_++; return true; }
    return false;
  }
  util::Status Expect(TokenType t, const std::string& context);

  util::Result<Statement> ParseCreateTable();
  util::Result<Statement> ParseDropTable();
  util::Result<Statement> ParseCreateIndex();
  util::Result<Statement> ParseDropIndex();
  util::Result<Statement> ParseInsert();
  util::Result<Statement> ParseUpdate();
  util::Result<Statement> ParseDelete();
  util::Result<Statement> ParseSelect();

  util::Result<minidb::common::Column> ParseColumnDef();
  util::Result<minidb::common::Value> ParseLiteral();
  util::Result<ExprPtr> ParseWhereClause();  
  util::Result<ExprPtr> ParseOrExpr();
  util::Result<ExprPtr> ParseAndExpr();
  util::Result<ExprPtr> ParseComparison();
  util::Result<ExprPtr> ParsePrimary();
  util::Result<SelectItem> ParseSelectItem();

  std::vector<Token> tokens_;
  std::size_t pos_ = 0;
};

util::Result<Statement> ParseSQL(const std::string& sql);

}  

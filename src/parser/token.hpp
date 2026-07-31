#pragma once

#include <cstddef>
#include <string>

namespace minidb::parser {

enum class TokenType {
  kIdentifier,
  kIntLiteral,
  kFloatLiteral,
  kStringLiteral,

  kSelect, kFrom, kWhere, kInsert, kInto, kValues, kUpdate, kSet, kDelete,
  kCreate, kTable, kDrop, kOrder, kBy, kAsc, kDesc, kLimit, kAnd, kOr,
  kNot, kNull, kTrue, kFalse, kAs, kIndex, kOn,
  kInt, kBigInt, kDouble, kBoolean, kVarchar,
  kCount, kSum, kAvg, kMin, kMax,

  kStar, kComma, kLParen, kRParen, kSemicolon, kDot,
  kEq, kNeq, kLt, kLte, kGt, kGte,

  kEndOfInput,
  kInvalid,
};

struct Token {
  TokenType type = TokenType::kInvalid;
  std::string lexeme;      
  std::size_t position = 0; 
};

inline const char* TokenTypeName(TokenType t) {
  switch (t) {
    case TokenType::kIdentifier: return "identifier";
    case TokenType::kIntLiteral: return "integer literal";
    case TokenType::kFloatLiteral: return "float literal";
    case TokenType::kStringLiteral: return "string literal";
    case TokenType::kSelect: return "SELECT";
    case TokenType::kFrom: return "FROM";
    case TokenType::kWhere: return "WHERE";
    case TokenType::kInsert: return "INSERT";
    case TokenType::kInto: return "INTO";
    case TokenType::kValues: return "VALUES";
    case TokenType::kUpdate: return "UPDATE";
    case TokenType::kSet: return "SET";
    case TokenType::kDelete: return "DELETE";
    case TokenType::kCreate: return "CREATE";
    case TokenType::kTable: return "TABLE";
    case TokenType::kDrop: return "DROP";
    case TokenType::kOrder: return "ORDER";
    case TokenType::kBy: return "BY";
    case TokenType::kAsc: return "ASC";
    case TokenType::kDesc: return "DESC";
    case TokenType::kLimit: return "LIMIT";
    case TokenType::kAnd: return "AND";
    case TokenType::kOr: return "OR";
    case TokenType::kNot: return "NOT";
    case TokenType::kNull: return "NULL";
    case TokenType::kTrue: return "TRUE";
    case TokenType::kFalse: return "FALSE";
    case TokenType::kAs: return "AS";
    case TokenType::kIndex: return "INDEX";
    case TokenType::kOn: return "ON";
    case TokenType::kInt: return "INT";
    case TokenType::kBigInt: return "BIGINT";
    case TokenType::kDouble: return "DOUBLE";
    case TokenType::kBoolean: return "BOOLEAN";
    case TokenType::kVarchar: return "VARCHAR";
    case TokenType::kCount: return "COUNT";
    case TokenType::kSum: return "SUM";
    case TokenType::kAvg: return "AVG";
    case TokenType::kMin: return "MIN";
    case TokenType::kMax: return "MAX";
    case TokenType::kStar: return "*";
    case TokenType::kComma: return ",";
    case TokenType::kLParen: return "(";
    case TokenType::kRParen: return ")";
    case TokenType::kSemicolon: return ";";
    case TokenType::kDot: return ".";
    case TokenType::kEq: return "=";
    case TokenType::kNeq: return "!=";
    case TokenType::kLt: return "<";
    case TokenType::kLte: return "<=";
    case TokenType::kGt: return ">";
    case TokenType::kGte: return ">=";
    case TokenType::kEndOfInput: return "<end of input>";
    case TokenType::kInvalid: return "<invalid>";
  }
  return "<unknown>";
}

}  

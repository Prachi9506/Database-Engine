#include "parser/tokenizer.hpp"

#include <cctype>
#include <unordered_map>

namespace minidb::parser {

namespace {
const std::unordered_map<std::string, TokenType>& KeywordTable() {
  static const std::unordered_map<std::string, TokenType> table = {
      {"SELECT", TokenType::kSelect}, {"FROM", TokenType::kFrom}, {"WHERE", TokenType::kWhere},
      {"INSERT", TokenType::kInsert}, {"INTO", TokenType::kInto}, {"VALUES", TokenType::kValues},
      {"UPDATE", TokenType::kUpdate}, {"SET", TokenType::kSet}, {"DELETE", TokenType::kDelete},
      {"CREATE", TokenType::kCreate}, {"TABLE", TokenType::kTable}, {"DROP", TokenType::kDrop},
      {"ORDER", TokenType::kOrder}, {"BY", TokenType::kBy}, {"ASC", TokenType::kAsc},
      {"DESC", TokenType::kDesc}, {"LIMIT", TokenType::kLimit}, {"AND", TokenType::kAnd},
      {"OR", TokenType::kOr}, {"NOT", TokenType::kNot}, {"NULL", TokenType::kNull},
      {"TRUE", TokenType::kTrue}, {"FALSE", TokenType::kFalse}, {"AS", TokenType::kAs},
      {"INDEX", TokenType::kIndex}, {"ON", TokenType::kOn},
      {"INT", TokenType::kInt}, {"INTEGER", TokenType::kInt}, {"BIGINT", TokenType::kBigInt},
      {"DOUBLE", TokenType::kDouble}, {"BOOLEAN", TokenType::kBoolean}, {"BOOL", TokenType::kBoolean},
      {"VARCHAR", TokenType::kVarchar}, {"TEXT", TokenType::kVarchar},
      {"COUNT", TokenType::kCount}, {"SUM", TokenType::kSum}, {"AVG", TokenType::kAvg},
      {"MIN", TokenType::kMin}, {"MAX", TokenType::kMax},
  };
  return table;
}

std::string ToUpper(const std::string& s) {
  std::string out = s;
  for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}
}  

void Tokenizer::SkipWhitespaceAndComments() {
  while (!lexer_.AtEnd()) {
    char c = lexer_.Peek();
    if (Lexer::IsWhitespace(c)) {
      lexer_.Advance();
    } else if (c == '-' && lexer_.PeekNext() == '-') {
      while (!lexer_.AtEnd() && lexer_.Peek() != '\n') lexer_.Advance();
    } else {
      break;
    }
  }
}

Token Tokenizer::ScanNumber() {
  std::size_t start = lexer_.Position();
  std::string lexeme;
  while (!lexer_.AtEnd() && Lexer::IsDigit(lexer_.Peek())) lexeme.push_back(lexer_.Advance());

  bool is_float = false;
  if (!lexer_.AtEnd() && lexer_.Peek() == '.' && Lexer::IsDigit(lexer_.PeekNext())) {
    is_float = true;
    lexeme.push_back(lexer_.Advance());  
    while (!lexer_.AtEnd() && Lexer::IsDigit(lexer_.Peek())) lexeme.push_back(lexer_.Advance());
  }

  return Token{is_float ? TokenType::kFloatLiteral : TokenType::kIntLiteral, lexeme, start};
}

util::Result<Token> Tokenizer::ScanString() {
  std::size_t start = lexer_.Position();
  lexer_.Advance();  
  std::string value;
  while (true) {
    if (lexer_.AtEnd()) {
      return util::Status::InvalidArgument("unterminated string literal starting at position " +
                                            std::to_string(start));
    }
    char c = lexer_.Advance();
    if (c == '\'') {
      if (!lexer_.AtEnd() && lexer_.Peek() == '\'') {
        value.push_back('\'');
        lexer_.Advance();
        continue;
      }
      break;  
    }
    value.push_back(c);
  }
  return util::Result<Token>(Token{TokenType::kStringLiteral, value, start});
}

Token Tokenizer::ScanIdentifierOrKeyword() {
  std::size_t start = lexer_.Position();
  std::string lexeme;
  while (!lexer_.AtEnd() && Lexer::IsAlphaNumeric(lexer_.Peek())) lexeme.push_back(lexer_.Advance());

  auto& keywords = KeywordTable();
  auto it = keywords.find(ToUpper(lexeme));
  if (it != keywords.end()) {
    return Token{it->second, lexeme, start};
  }
  return Token{TokenType::kIdentifier, lexeme, start};
}

Token Tokenizer::ScanOperatorOrPunctuation() {
  std::size_t start = lexer_.Position();
  char c = lexer_.Advance();
  switch (c) {
    case '*': return Token{TokenType::kStar, "*", start};
    case ',': return Token{TokenType::kComma, ",", start};
    case '(': return Token{TokenType::kLParen, "(", start};
    case ')': return Token{TokenType::kRParen, ")", start};
    case ';': return Token{TokenType::kSemicolon, ";", start};
    case '.': return Token{TokenType::kDot, ".", start};
    case '=': return Token{TokenType::kEq, "=", start};
    case '!':
      if (!lexer_.AtEnd() && lexer_.Peek() == '=') {
        lexer_.Advance();
        return Token{TokenType::kNeq, "!=", start};
      }
      return Token{TokenType::kInvalid, "!", start};
    case '<':
      if (!lexer_.AtEnd() && lexer_.Peek() == '=') {
        lexer_.Advance();
        return Token{TokenType::kLte, "<=", start};
      }
      if (!lexer_.AtEnd() && lexer_.Peek() == '>') {
        lexer_.Advance();
        return Token{TokenType::kNeq, "<>", start};
      }
      return Token{TokenType::kLt, "<", start};
    case '>':
      if (!lexer_.AtEnd() && lexer_.Peek() == '=') {
        lexer_.Advance();
        return Token{TokenType::kGte, ">=", start};
      }
      return Token{TokenType::kGt, ">", start};
    default:
      return Token{TokenType::kInvalid, std::string(1, c), start};
  }
}

util::Result<Token> Tokenizer::NextToken() {
  SkipWhitespaceAndComments();
  if (lexer_.AtEnd()) {
    return util::Result<Token>(Token{TokenType::kEndOfInput, "", lexer_.Position()});
  }

  char c = lexer_.Peek();
  if (Lexer::IsDigit(c)) return util::Result<Token>(ScanNumber());
  if (Lexer::IsAlpha(c)) return util::Result<Token>(ScanIdentifierOrKeyword());
  if (c == '\'') return ScanString();

  Token tok = ScanOperatorOrPunctuation();
  if (tok.type == TokenType::kInvalid) {
    return util::Status::InvalidArgument("unexpected character '" + tok.lexeme + "' at position " +
                                          std::to_string(tok.position));
  }
  return util::Result<Token>(tok);
}

util::Result<std::vector<Token>> Tokenizer::TokenizeAll() {
  std::vector<Token> tokens;
  while (true) {
    auto tok = NextToken();
    if (!tok.ok()) return tok.status();
    tokens.push_back(tok.value());
    if (tok.value().type == TokenType::kEndOfInput) break;
  }
  return util::Result<std::vector<Token>>(std::move(tokens));
}

}  

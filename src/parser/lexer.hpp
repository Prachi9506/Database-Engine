#pragma once

#include <cstddef>
#include <string>

namespace minidb::parser {

class Lexer {
 public:
  explicit Lexer(const std::string& source) : source_(source) {}

  bool AtEnd() const { return pos_ >= source_.size(); }
  char Peek() const { return AtEnd() ? '\0' : source_[pos_]; }
  char PeekNext() const { return (pos_ + 1 < source_.size()) ? source_[pos_ + 1] : '\0'; }
  char Advance() { return source_[pos_++]; }
  std::size_t Position() const { return pos_; }

  static bool IsDigit(char c) { return c >= '0' && c <= '9'; }
  static bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
  static bool IsAlphaNumeric(char c) { return IsAlpha(c) || IsDigit(c); }
  static bool IsWhitespace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

 private:
  const std::string& source_;
  std::size_t pos_ = 0;
};

}  

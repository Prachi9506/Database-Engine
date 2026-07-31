#pragma once

#include <vector>

#include "parser/lexer.hpp"
#include "parser/token.hpp"
#include "utilities/status.hpp"

namespace minidb::parser {

class Tokenizer {
 public:
  explicit Tokenizer(std::string source) : source_(std::move(source)), lexer_(source_) {}


  util::Result<std::vector<Token>> TokenizeAll();

 private:
  util::Result<Token> NextToken();
  void SkipWhitespaceAndComments();
  Token ScanNumber();
  util::Result<Token> ScanString();
  Token ScanIdentifierOrKeyword();
  Token ScanOperatorOrPunctuation();


  std::string source_;
  Lexer lexer_;
};

}  

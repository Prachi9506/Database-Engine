#include "parser/tokenizer.hpp"

#include "test_framework.hpp"

using namespace minidb::parser;

TEST(Tokenizer, TokenizesSimpleSelect) {
  Tokenizer t("SELECT id, name FROM users WHERE age > 30;");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  auto& toks = result.value();

  std::vector<TokenType> expected = {
      TokenType::kSelect, TokenType::kIdentifier, TokenType::kComma, TokenType::kIdentifier,
      TokenType::kFrom, TokenType::kIdentifier, TokenType::kWhere, TokenType::kIdentifier,
      TokenType::kGt, TokenType::kIntLiteral, TokenType::kSemicolon, TokenType::kEndOfInput,
  };
  EXPECT_EQ(toks.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_TRUE(toks[i].type == expected[i]);
  }
}

TEST(Tokenizer, KeywordsAreCaseInsensitive) {
  Tokenizer t("select * from Users");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value()[0].type == TokenType::kSelect);
  EXPECT_TRUE(result.value()[1].type == TokenType::kStar);
  EXPECT_TRUE(result.value()[2].type == TokenType::kFrom);
  EXPECT_TRUE(result.value()[3].type == TokenType::kIdentifier);
  EXPECT_EQ(result.value()[3].lexeme, "Users");  
}

TEST(Tokenizer, IntegerAndFloatLiterals) {
  Tokenizer t("42 3.14 0 100.5");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  auto& toks = result.value();
  EXPECT_TRUE(toks[0].type == TokenType::kIntLiteral);
  EXPECT_EQ(toks[0].lexeme, "42");
  EXPECT_TRUE(toks[1].type == TokenType::kFloatLiteral);
  EXPECT_EQ(toks[1].lexeme, "3.14");
  EXPECT_TRUE(toks[2].type == TokenType::kIntLiteral);
  EXPECT_TRUE(toks[3].type == TokenType::kFloatLiteral);
}

TEST(Tokenizer, StringLiteralWithEscapedQuote) {
  Tokenizer t("'it''s a test'");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value()[0].type == TokenType::kStringLiteral);
  EXPECT_EQ(result.value()[0].lexeme, "it's a test");
}

TEST(Tokenizer, UnterminatedStringFails) {
  Tokenizer t("'unterminated");
  auto result = t.TokenizeAll();
  EXPECT_FALSE(result.ok());
}

TEST(Tokenizer, MultiCharOperators) {
  Tokenizer t("<= >= != <> < > =");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  auto& toks = result.value();
  EXPECT_TRUE(toks[0].type == TokenType::kLte);
  EXPECT_TRUE(toks[1].type == TokenType::kGte);
  EXPECT_TRUE(toks[2].type == TokenType::kNeq);
  EXPECT_TRUE(toks[3].type == TokenType::kNeq);  
  EXPECT_TRUE(toks[4].type == TokenType::kLt);
  EXPECT_TRUE(toks[5].type == TokenType::kGt);
  EXPECT_TRUE(toks[6].type == TokenType::kEq);
}

TEST(Tokenizer, LineCommentsAreSkipped) {
  Tokenizer t("SELECT * -- this is a comment\nFROM users");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  auto& toks = result.value();
  EXPECT_TRUE(toks[0].type == TokenType::kSelect);
  EXPECT_TRUE(toks[1].type == TokenType::kStar);
  EXPECT_TRUE(toks[2].type == TokenType::kFrom);
  EXPECT_TRUE(toks[3].type == TokenType::kIdentifier);
}

TEST(Tokenizer, UnexpectedCharacterFails) {
  Tokenizer t("SELECT # FROM users");
  auto result = t.TokenizeAll();
  EXPECT_FALSE(result.ok());
}

TEST(Tokenizer, VarcharWithLengthTokenizes) {
  Tokenizer t("VARCHAR(50)");
  auto result = t.TokenizeAll();
  EXPECT_TRUE(result.ok());
  auto& toks = result.value();
  EXPECT_TRUE(toks[0].type == TokenType::kVarchar);
  EXPECT_TRUE(toks[1].type == TokenType::kLParen);
  EXPECT_TRUE(toks[2].type == TokenType::kIntLiteral);
  EXPECT_EQ(toks[2].lexeme, "50");
  EXPECT_TRUE(toks[3].type == TokenType::kRParen);
}

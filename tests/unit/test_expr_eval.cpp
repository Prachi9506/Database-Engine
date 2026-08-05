#include "executor/expr_eval.hpp"

#include "test_framework.hpp"

using namespace minidb::common;
using namespace minidb::parser;
using namespace minidb::executor;

namespace {
Schema MakeSchema() {
  return Schema({
      Column{"id", ColumnType::kInt, false, 0},
      Column{"name", ColumnType::kVarchar, true, 50},
      Column{"age", ColumnType::kInt, true, 0},
  });
}
}  

TEST(ExprEval, ColumnRefResolvesValue) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto expr = MakeColumnRef("age");
  auto result = Evaluate(*expr, schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::get<std::int32_t>(result.value()), 30);
}

TEST(ExprEval, UnknownColumnFails) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto expr = MakeColumnRef("nonexistent");
  auto result = Evaluate(*expr, schema, row);
  EXPECT_FALSE(result.ok());
}

TEST(ExprEval, ComparisonAcrossNumericTypesWidens) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto expr = MakeBinary(MakeColumnRef("age"), BinaryOp::kEq, MakeLiteral(Value(std::int64_t(30))));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value());
}

TEST(ExprEval, GreaterThanComparison) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto expr = MakeBinary(MakeColumnRef("age"), BinaryOp::kGt, MakeLiteral(Value(std::int64_t(18))));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value());
}

TEST(ExprEval, AndCombinesCorrectly) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto left = MakeBinary(MakeColumnRef("age"), BinaryOp::kGt, MakeLiteral(Value(std::int64_t(18))));
  auto right = MakeBinary(MakeColumnRef("age"), BinaryOp::kLt, MakeLiteral(Value(std::int64_t(65))));
  auto expr = MakeBinary(std::move(left), BinaryOp::kAnd, std::move(right));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value());
}

TEST(ExprEval, OrShortCircuitsOnTrue) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto left = MakeBinary(MakeColumnRef("age"), BinaryOp::kEq, MakeLiteral(Value(std::int64_t(30))));

  auto right = MakeBinary(MakeColumnRef("does_not_exist"), BinaryOp::kEq, MakeLiteral(Value(std::int64_t(1))));
  auto expr = MakeBinary(std::move(left), BinaryOp::kOr, std::move(right));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value());
}

TEST(ExprEval, NullComparisonIsFalse) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), Value(std::monostate{}), std::int32_t(30)};
  auto expr = MakeBinary(MakeColumnRef("name"), BinaryOp::kEq, MakeLiteral(Value(std::string("alice"))));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_FALSE(result.value());
}

TEST(ExprEval, NoWhereClauseAlwaysMatches) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto result = EvaluatePredicate(nullptr, schema, row);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value());
}

TEST(ExprEval, IncompatibleTypeComparisonFails) {
  Schema schema = MakeSchema();
  std::vector<Value> row = {std::int32_t(1), std::string("alice"), std::int32_t(30)};
  auto expr = MakeBinary(MakeColumnRef("name"), BinaryOp::kGt, MakeLiteral(Value(std::int64_t(5))));
  auto result = EvaluatePredicate(expr.get(), schema, row);
  EXPECT_FALSE(result.ok());
}

#include "parser/parser.hpp"

#include "test_framework.hpp"

using namespace minidb::parser;
using namespace minidb::common;

TEST(Parser, ParsesCreateTable) {
  auto result = ParseSQL("CREATE TABLE users (id INT NOT NULL, name VARCHAR(50), age INT)");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<CreateTableStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->table_name, "users");
  EXPECT_EQ(stmt->columns.size(), std::size_t(3));
  EXPECT_EQ(stmt->columns[0].name, "id");
  EXPECT_TRUE(stmt->columns[0].type == ColumnType::kInt);
  EXPECT_FALSE(stmt->columns[0].nullable);
  EXPECT_EQ(stmt->columns[1].name, "name");
  EXPECT_TRUE(stmt->columns[1].type == ColumnType::kVarchar);
  EXPECT_EQ(stmt->columns[1].max_length, 50);
  EXPECT_TRUE(stmt->columns[2].nullable);
}

TEST(Parser, CreateTableRequiresAtLeastOneColumn) {
  auto result = ParseSQL("CREATE TABLE t ()");
  EXPECT_FALSE(result.ok());
}

TEST(Parser, ParsesDropTable) {
  auto result = ParseSQL("DROP TABLE users");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<DropTableStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->table_name, "users");
}

TEST(Parser, ParsesInsertWithExplicitColumns) {
  auto result = ParseSQL("INSERT INTO users (id, name, age) VALUES (1, 'alice', 30)");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<InsertStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->table_name, "users");
  EXPECT_EQ(stmt->columns.size(), std::size_t(3));
  EXPECT_EQ(stmt->rows.size(), std::size_t(1));
  EXPECT_EQ(std::get<std::int64_t>(stmt->rows[0][0]), 1);
  EXPECT_EQ(std::get<std::string>(stmt->rows[0][1]), "alice");
  EXPECT_EQ(std::get<std::int64_t>(stmt->rows[0][2]), 30);
}

TEST(Parser, ParsesInsertWithoutColumnListAndMultipleRows) {
  auto result = ParseSQL("INSERT INTO t VALUES (1, TRUE, NULL), (2, FALSE, 3.5)");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<InsertStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_TRUE(stmt->columns.empty());
  EXPECT_EQ(stmt->rows.size(), std::size_t(2));
  EXPECT_EQ(std::get<bool>(stmt->rows[0][1]), true);
  EXPECT_TRUE(IsNull(stmt->rows[0][2]));
  EXPECT_EQ(std::get<double>(stmt->rows[1][2]), 3.5);
}

TEST(Parser, ParsesUpdateWithWhere) {
  auto result = ParseSQL("UPDATE users SET name = 'bob', age = 31 WHERE id = 1");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<UpdateStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->table_name, "users");
  EXPECT_EQ(stmt->assignments.size(), std::size_t(2));
  EXPECT_EQ(stmt->assignments[0].first, "name");
  EXPECT_EQ(std::get<std::string>(stmt->assignments[0].second), "bob");
  EXPECT_TRUE(stmt->where != nullptr);
  auto* bin = std::get_if<BinaryExpr>(&stmt->where->node);
  EXPECT_TRUE(bin != nullptr);
  EXPECT_TRUE(bin->op == BinaryOp::kEq);
}

TEST(Parser, ParsesDeleteWithoutWhere) {
  auto result = ParseSQL("DELETE FROM users");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<DeleteStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_TRUE(stmt->where == nullptr);
}

TEST(Parser, ParsesSelectStar) {
  auto result = ParseSQL("SELECT * FROM users");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->items.size(), std::size_t(1));
  EXPECT_TRUE(stmt->items[0].is_star);
  EXPECT_EQ(stmt->table_name, "users");
}

TEST(Parser, ParsesSelectColumnListWithAlias) {
  auto result = ParseSQL("SELECT id, name AS full_name FROM users");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->items.size(), std::size_t(2));
  EXPECT_EQ(*stmt->items[0].column, "id");
  EXPECT_EQ(*stmt->items[1].column, "name");
  EXPECT_EQ(*stmt->items[1].alias, "full_name");
}

TEST(Parser, ParsesAggregateFunctions) {
  auto result = ParseSQL("SELECT COUNT(*), SUM(amount), AVG(amount) AS avg_amt FROM orders");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_EQ(stmt->items.size(), std::size_t(3));
  EXPECT_TRUE(stmt->items[0].aggregate == AggregateFunc::kCount);
  EXPECT_TRUE(stmt->items[0].aggregate_star);
  EXPECT_TRUE(stmt->items[1].aggregate == AggregateFunc::kSum);
  EXPECT_EQ(*stmt->items[1].column, "amount");
  EXPECT_TRUE(stmt->items[2].aggregate == AggregateFunc::kAvg);
  EXPECT_EQ(*stmt->items[2].alias, "avg_amt");
}

TEST(Parser, StarInsideNonCountAggregateRejected) {
  auto result = ParseSQL("SELECT SUM(*) FROM orders");
  EXPECT_FALSE(result.ok());
}

TEST(Parser, ParsesWhereWithAndOr) {
  auto result = ParseSQL("SELECT * FROM users WHERE age > 18 AND age < 65 OR name = 'admin'");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_TRUE(stmt->where != nullptr);
  auto* top = std::get_if<BinaryExpr>(&stmt->where->node);
  EXPECT_TRUE(top != nullptr);
  EXPECT_TRUE(top->op == BinaryOp::kOr);
  auto* left_and = std::get_if<BinaryExpr>(&top->left->node);
  EXPECT_TRUE(left_and != nullptr);
  EXPECT_TRUE(left_and->op == BinaryOp::kAnd);
}

TEST(Parser, ParsesParenthesizedWhereGrouping) {
  auto result = ParseSQL("SELECT * FROM users WHERE (age > 18 OR age < 5) AND active = TRUE");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  auto* top = std::get_if<BinaryExpr>(&stmt->where->node);
  EXPECT_TRUE(top != nullptr);
  EXPECT_TRUE(top->op == BinaryOp::kAnd);
  auto* left_or = std::get_if<BinaryExpr>(&top->left->node);
  EXPECT_TRUE(left_or != nullptr);
  EXPECT_TRUE(left_or->op == BinaryOp::kOr);
}

TEST(Parser, ParsesOrderByAndLimit) {
  auto result = ParseSQL("SELECT * FROM users ORDER BY age DESC LIMIT 10");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_TRUE(stmt->order_by.has_value());
  EXPECT_EQ(stmt->order_by->column, "age");
  EXPECT_TRUE(stmt->order_by->descending);
  EXPECT_TRUE(stmt->limit.has_value());
  EXPECT_EQ(*stmt->limit, 10);
}

TEST(Parser, OrderByDefaultsToAscending) {
  auto result = ParseSQL("SELECT * FROM users ORDER BY name");
  EXPECT_TRUE(result.ok());
  auto* stmt = std::get_if<SelectStmt>(&result.value());
  EXPECT_TRUE(stmt != nullptr);
  EXPECT_FALSE(stmt->order_by->descending);
}

TEST(Parser, TrailingSemicolonIsOptionalAndAccepted) {
  auto a = ParseSQL("SELECT * FROM users");
  auto b = ParseSQL("SELECT * FROM users;");
  EXPECT_TRUE(a.ok());
  EXPECT_TRUE(b.ok());
}

TEST(Parser, TrailingGarbageRejected) {
  auto result = ParseSQL("SELECT * FROM users EXTRA_JUNK");
  EXPECT_FALSE(result.ok());
}

TEST(Parser, EmptyInputRejected) {
  auto result = ParseSQL("");
  EXPECT_FALSE(result.ok());
}

TEST(Parser, MissingFromRejected) {
  auto result = ParseSQL("SELECT id users");
  EXPECT_FALSE(result.ok());
}

TEST(Parser, UnknownLeadingKeywordRejected) {
  auto result = ParseSQL("MERGE INTO users");
  EXPECT_FALSE(result.ok());
}

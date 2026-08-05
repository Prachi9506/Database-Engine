#include "executor/executor.hpp"

#include <filesystem>

#include "parser/parser.hpp"
#include "test_framework.hpp"

using namespace minidb;
using namespace minidb::common;
using namespace minidb::parser;
using namespace minidb::executor;

namespace {
std::string TempDir(const std::string& name) {
  std::string dir = TestTempRoot() + "minidb_test_exec_" + name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

util::Result<QueryResult> Run(Executor& exec, const std::string& sql) {
  auto stmt = ParseSQL(sql);
  if (!stmt.ok()) return stmt.status();
  return exec.Execute(stmt.value());
}
}  

TEST(Executor, CreateInsertSelect) {
  std::string dir = TempDir("basic");
  {
    Database db(dir);
    Executor exec(&db);

    auto create = Run(exec, "CREATE TABLE users (id INT NOT NULL, name VARCHAR(50), age INT)");
    EXPECT_TRUE(create.ok());

    auto insert = Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25), (3, 'carol', 40)");
    EXPECT_TRUE(insert.ok());
    EXPECT_EQ(insert.value().rows_affected, std::size_t(3));

    auto select = Run(exec, "SELECT * FROM users");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(3));
    EXPECT_EQ(select.value().column_names.size(), std::size_t(3));
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, SelectWithWhere) {
  std::string dir = TempDir("where");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25), (3, 'carol', 40)");

    auto select = Run(exec, "SELECT name FROM users WHERE age > 28");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(2));
    EXPECT_EQ(select.value().column_names[0], "name");
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, SelectWithOrderByAndLimit) {
  std::string dir = TempDir("order_limit");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25), (3, 'carol', 40)");

    auto select = Run(exec, "SELECT name, age FROM users ORDER BY age DESC LIMIT 2");
    EXPECT_TRUE(select.ok());
    auto& rows = select.value().rows;
    EXPECT_EQ(rows.size(), std::size_t(2));
    EXPECT_EQ(std::get<std::string>(rows[0][0]), "carol");  
    EXPECT_EQ(std::get<std::string>(rows[1][0]), "alice");  
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, OrderByColumnNotInSelectList) {
  std::string dir = TempDir("order_not_selected");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25)");

    auto select = Run(exec, "SELECT name FROM users ORDER BY age ASC");
    EXPECT_TRUE(select.ok());
    auto& rows = select.value().rows;
    EXPECT_EQ(std::get<std::string>(rows[0][0]), "bob");
    EXPECT_EQ(std::get<std::string>(rows[1][0]), "alice");
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, AggregateFunctions) {
  std::string dir = TempDir("aggregate");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE orders (id INT, amount DOUBLE)");
    Run(exec, "INSERT INTO orders VALUES (1, 10.0), (2, 20.0), (3, 30.0)");

    auto select = Run(exec, "SELECT COUNT(*), SUM(amount), AVG(amount), MIN(amount), MAX(amount) FROM orders");
    EXPECT_TRUE(select.ok());
    auto& rows = select.value().rows;
    EXPECT_EQ(rows.size(), std::size_t(1));
    EXPECT_EQ(std::get<std::int64_t>(rows[0][0]), 3);
    EXPECT_EQ(std::get<double>(rows[0][1]), 60.0);
    EXPECT_EQ(std::get<double>(rows[0][2]), 20.0);
    EXPECT_EQ(std::get<double>(rows[0][3]), 10.0);
    EXPECT_EQ(std::get<double>(rows[0][4]), 30.0);
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, AggregateOnEmptyTableYieldsNulls) {
  std::string dir = TempDir("aggregate_empty");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE orders (id INT, amount DOUBLE)");

    auto select = Run(exec, "SELECT COUNT(*), SUM(amount) FROM orders");
    EXPECT_TRUE(select.ok());
    auto& rows = select.value().rows;
    EXPECT_EQ(std::get<std::int64_t>(rows[0][0]), 0);
    EXPECT_TRUE(IsNull(rows[0][1]));  
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, MixingAggregateAndPlainColumnsRejected) {
  std::string dir = TempDir("mixed_agg");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE orders (id INT, amount DOUBLE)");
    auto select = Run(exec, "SELECT id, SUM(amount) FROM orders");
    EXPECT_FALSE(select.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, UpdateWithWhere) {
  std::string dir = TempDir("update_where");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25), (3, 'carol', 40)");

    auto update = Run(exec, "UPDATE users SET age = 99 WHERE name = 'bob'");
    EXPECT_TRUE(update.ok());
    EXPECT_EQ(update.value().rows_affected, std::size_t(1));

    auto select = Run(exec, "SELECT age FROM users WHERE name = 'bob'");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(std::get<std::int32_t>(select.value().rows[0][0]), 99);
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, UpdateGrowingRowsDoesNotDoubleUpdateHalloweenProblem) {

  std::string dir = TempDir("halloween");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, tag VARCHAR(200))");
    for (int i = 0; i < 20; ++i) {
      Run(exec, "INSERT INTO t VALUES (" + std::to_string(i) + ", 'x')");
    }

    std::string filler(150, 'y');
    auto update = Run(exec, "UPDATE t SET tag = '" + filler + "'");
    EXPECT_TRUE(update.ok());
    EXPECT_EQ(update.value().rows_affected, std::size_t(20));  

    auto select = Run(exec, "SELECT id FROM t");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(20));  
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, DeleteWithWhere) {
  std::string dir = TempDir("delete_where");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50), age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25), (3, 'carol', 40)");

    auto del = Run(exec, "DELETE FROM users WHERE age < 30");
    EXPECT_TRUE(del.ok());
    EXPECT_EQ(del.value().rows_affected, std::size_t(1));

    auto select = Run(exec, "SELECT * FROM users");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(2));
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, InsertWithExplicitColumnListLeavesOthersNull) {
  std::string dir = TempDir("insert_partial");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT NOT NULL, name VARCHAR(50), age INT)");
    auto insert = Run(exec, "INSERT INTO users (id, name) VALUES (1, 'alice')");
    EXPECT_TRUE(insert.ok());

    auto select = Run(exec, "SELECT age FROM users WHERE id = 1");
    EXPECT_TRUE(select.ok());
    EXPECT_TRUE(IsNull(select.value().rows[0][0]));
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, IntLiteralCoercedToInt32Column) {
  std::string dir = TempDir("coerce_int");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (a INT, b BIGINT)");
    auto insert = Run(exec, "INSERT INTO t VALUES (5, 5)");
    EXPECT_TRUE(insert.ok());

    auto select = Run(exec, "SELECT a, b FROM t");
    EXPECT_TRUE(select.ok());
    EXPECT_TRUE(std::holds_alternative<std::int32_t>(select.value().rows[0][0]));
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(select.value().rows[0][1]));
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, IntOutOfInt32RangeRejectedForIntColumn) {
  std::string dir = TempDir("coerce_overflow");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (a INT)");
    auto insert = Run(exec, "INSERT INTO t VALUES (99999999999)");
    EXPECT_FALSE(insert.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, NotNullViolationRejected) {
  std::string dir = TempDir("notnull");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT NOT NULL)");
    auto insert = Run(exec, "INSERT INTO t (id) VALUES (NULL)");
    EXPECT_FALSE(insert.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, DropTableThenQueryFails) {
  std::string dir = TempDir("drop_then_query");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT)");
    auto drop = Run(exec, "DROP TABLE t");
    EXPECT_TRUE(drop.ok());
    auto select = Run(exec, "SELECT * FROM t");
    EXPECT_FALSE(select.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Executor, EndToEndSurvivesRestart) {
  std::string dir = TempDir("e2e_restart");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, name VARCHAR(50))");
    Run(exec, "INSERT INTO users VALUES (1, 'alice'), (2, 'bob')");
    db.FlushAll();
  }
  {
    Database db(dir);
    Executor exec(&db);
    auto select = Run(exec, "SELECT name FROM users ORDER BY id");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(2));
    EXPECT_EQ(std::get<std::string>(select.value().rows[0][0]), "alice");
  }
  std::filesystem::remove_all(dir);
}

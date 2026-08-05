#include <algorithm>
#include <filesystem>

#include "executor/executor.hpp"
#include "parser/parser.hpp"
#include "test_framework.hpp"

using namespace minidb;
using namespace minidb::common;
using namespace minidb::parser;
using namespace minidb::executor;

namespace {
std::string TempDir(const std::string& name) {
  std::string dir = TestTempRoot() + "minidb_test_idx_" + name;
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

TEST(IndexIntegration, CreateIndexOnExistingDataIsUsable) {
  std::string dir = TempDir("create_on_existing");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, age INT, name VARCHAR(50))");
    for (int i = 0; i < 100; ++i) {
      Run(exec, "INSERT INTO users VALUES (" + std::to_string(i) + ", " + std::to_string(20 + i % 50) +
                    ", 'user')");
    }

    auto create_idx = Run(exec, "CREATE INDEX idx_id ON users (id)");
    EXPECT_TRUE(create_idx.ok());

    auto select = Run(exec, "SELECT id FROM users WHERE id = 42");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().rows.size(), std::size_t(1));
    EXPECT_EQ(std::get<std::int32_t>(select.value().rows[0][0]), 42);
    EXPECT_EQ(select.value().scan_strategy, "IndexScan(idx_id)");
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, QueryWithoutIndexUsesSeqScan) {
  std::string dir = TempDir("no_index");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE users (id INT, age INT)");
    Run(exec, "INSERT INTO users VALUES (1, 30)");

    auto select = Run(exec, "SELECT id FROM users WHERE id = 1");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().scan_strategy, "SeqScan");
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, IndexScanAndSeqScanAgreeOnResults) {

  std::string dir = TempDir("agree");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE indexed_t (id INT, val INT)");
    Run(exec, "CREATE TABLE plain_t (id INT, val INT)");
    for (int i = 0; i < 500; ++i) {
      std::string vals = "(" + std::to_string(i) + ", " + std::to_string(i * 7 % 97) + ")";
      Run(exec, "INSERT INTO indexed_t VALUES " + vals);
      Run(exec, "INSERT INTO plain_t VALUES " + vals);
    }
    Run(exec, "CREATE INDEX idx_val ON indexed_t (val)");

    auto indexed_result = Run(exec, "SELECT id FROM indexed_t WHERE val = 42");
    auto plain_result = Run(exec, "SELECT id FROM plain_t WHERE val = 42");
    EXPECT_TRUE(indexed_result.ok());
    EXPECT_TRUE(plain_result.ok());
    EXPECT_EQ(indexed_result.value().scan_strategy, "IndexScan(idx_val)");
    EXPECT_EQ(plain_result.value().scan_strategy, "SeqScan");

    EXPECT_EQ(indexed_result.value().rows.size(), plain_result.value().rows.size());
    std::vector<std::int32_t> indexed_ids, plain_ids;
    for (auto& row : indexed_result.value().rows) indexed_ids.push_back(std::get<std::int32_t>(row[0]));
    for (auto& row : plain_result.value().rows) plain_ids.push_back(std::get<std::int32_t>(row[0]));
    std::sort(indexed_ids.begin(), indexed_ids.end());
    std::sort(plain_ids.begin(), plain_ids.end());
    EXPECT_TRUE(indexed_ids == plain_ids);
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, RangeQueryUsesIndexScan) {
  std::string dir = TempDir("range");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, age INT)");
    for (int i = 0; i < 200; ++i) Run(exec, "INSERT INTO t VALUES (" + std::to_string(i) + ", " + std::to_string(i) + ")");
    Run(exec, "CREATE INDEX idx_age ON t (age)");

    auto select = Run(exec, "SELECT id FROM t WHERE age >= 50 AND age < 60");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().scan_strategy, "IndexScan(idx_age)");
    EXPECT_EQ(select.value().rows.size(), std::size_t(10));  
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, OrDisqualifiesIndexOptimization) {
  std::string dir = TempDir("or_disqualify");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, age INT)");
    Run(exec, "INSERT INTO t VALUES (1, 10), (2, 20)");
    Run(exec, "CREATE INDEX idx_age ON t (age)");


    auto select = Run(exec, "SELECT id FROM t WHERE age = 10 OR age = 20");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().scan_strategy, "SeqScan");
    EXPECT_EQ(select.value().rows.size(), std::size_t(2));
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, IndexStaysCorrectAfterUpdateAndDelete) {
  std::string dir = TempDir("maintenance");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, val INT)");
    for (int i = 0; i < 50; ++i) Run(exec, "INSERT INTO t VALUES (" + std::to_string(i) + ", " + std::to_string(i) + ")");
    Run(exec, "CREATE INDEX idx_val ON t (val)");

  
    auto update = Run(exec, "UPDATE t SET val = 999 WHERE id = 10");
    EXPECT_TRUE(update.ok());

    auto old_val = Run(exec, "SELECT id FROM t WHERE val = 10");
    EXPECT_TRUE(old_val.ok());
    EXPECT_EQ(old_val.value().rows.size(), std::size_t(0));
    EXPECT_EQ(old_val.value().scan_strategy, "IndexScan(idx_val)");

    auto new_val = Run(exec, "SELECT id FROM t WHERE val = 999");
    EXPECT_TRUE(new_val.ok());
    EXPECT_EQ(new_val.value().rows.size(), std::size_t(1));
    EXPECT_EQ(std::get<std::int32_t>(new_val.value().rows[0][0]), 10);

    auto del = Run(exec, "DELETE FROM t WHERE id = 20");
    EXPECT_TRUE(del.ok());
    auto deleted_val = Run(exec, "SELECT id FROM t WHERE val = 20");
    EXPECT_TRUE(deleted_val.ok());
    EXPECT_EQ(deleted_val.value().rows.size(), std::size_t(0));
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, NewlyInsertedRowsAreImmediatelyIndexed) {
  std::string dir = TempDir("insert_after_index");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, val INT)");
    Run(exec, "CREATE INDEX idx_val ON t (val)");  

    Run(exec, "INSERT INTO t VALUES (1, 500)");
    auto select = Run(exec, "SELECT id FROM t WHERE val = 500");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().scan_strategy, "IndexScan(idx_val)");
    EXPECT_EQ(select.value().rows.size(), std::size_t(1));
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, IndexOnNonIntegerColumnRejected) {
  std::string dir = TempDir("non_int_index");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, name VARCHAR(50))");
    auto create_idx = Run(exec, "CREATE INDEX idx_name ON t (name)");
    EXPECT_FALSE(create_idx.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, DropIndexFallsBackToSeqScan) {
  std::string dir = TempDir("drop_index");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT)");
    Run(exec, "INSERT INTO t VALUES (1)");
    Run(exec, "CREATE INDEX idx_id ON t (id)");

    auto before = Run(exec, "SELECT id FROM t WHERE id = 1");
    EXPECT_EQ(before.value().scan_strategy, "IndexScan(idx_id)");

    auto drop = Run(exec, "DROP INDEX idx_id");
    EXPECT_TRUE(drop.ok());

    auto after = Run(exec, "SELECT id FROM t WHERE id = 1");
    EXPECT_TRUE(after.ok());
    EXPECT_EQ(after.value().scan_strategy, "SeqScan");
    EXPECT_EQ(after.value().rows.size(), std::size_t(1));  
  }
  std::filesystem::remove_all(dir);
}

TEST(IndexIntegration, IndexAndDataSurviveFullRestart) {
  std::string dir = TempDir("restart");
  {
    Database db(dir);
    Executor exec(&db);
    Run(exec, "CREATE TABLE t (id INT, val INT)");
    for (int i = 0; i < 100; ++i) Run(exec, "INSERT INTO t VALUES (" + std::to_string(i) + ", " + std::to_string(i) + ")");
    Run(exec, "CREATE INDEX idx_val ON t (val)");
    db.FlushAll();
  }
  {
    Database db(dir);
    Executor exec(&db);
    auto select = Run(exec, "SELECT id FROM t WHERE val = 77");
    EXPECT_TRUE(select.ok());
    EXPECT_EQ(select.value().scan_strategy, "IndexScan(idx_val)");
    EXPECT_EQ(select.value().rows.size(), std::size_t(1));
    EXPECT_EQ(std::get<std::int32_t>(select.value().rows[0][0]), 77);

    Run(exec, "INSERT INTO t VALUES (999, 12345)");
    auto after_insert = Run(exec, "SELECT id FROM t WHERE val = 12345");
    EXPECT_TRUE(after_insert.ok());
    EXPECT_EQ(after_insert.value().scan_strategy, "IndexScan(idx_val)");
    EXPECT_EQ(after_insert.value().rows.size(), std::size_t(1));
  }
  std::filesystem::remove_all(dir);
}

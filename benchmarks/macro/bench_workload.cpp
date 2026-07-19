#include <cstdio>
#include <filesystem>
#include <random>

#include "bench_utils.hpp"
#include "database.hpp"
#include "executor/executor.hpp"
#include "parser/parser.hpp"

using namespace minidb;
using namespace minidb::bench;

namespace {
std::string TmpDir(const std::string& name) {
  std::string dir = BenchTempRoot() + "minidb_bench_dir_" + name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void Exec(executor::Executor& exec, const std::string& sql) {
  auto stmt = parser::ParseSQL(sql);
  if (!stmt.ok()) { std::printf("PARSE ERROR: %s -- %s\n", sql.c_str(), stmt.status().message().c_str()); return; }
  auto result = exec.Execute(stmt.value());
  if (!result.ok()) { std::printf("EXEC ERROR: %s -- %s\n", sql.c_str(), result.status().message().c_str()); return; }
}
}  

void BenchInsertThroughput() {
  std::string dir = TmpDir("insert");
  {
    Database db(dir);
    executor::Executor exec(&db);
    Exec(exec, "CREATE TABLE orders (id INT, customer_id INT, amount DOUBLE)");

    constexpr std::size_t kN = 10000;
    Run("INSERT (single-row statements)", kN, "no index maintenance", [&]() {
      for (std::size_t i = 0; i < kN; ++i) {
        Exec(exec, "INSERT INTO orders VALUES (" + std::to_string(i) + ", " + std::to_string(i % 500) + ", " +
                       std::to_string((i % 97) + 0.5) + ")");
      }
    });
  }
  std::filesystem::remove_all(dir);
}

void BenchSeqScanVsIndexScan() {
  std::string dir = TmpDir("scan_compare");
  {
    Database db(dir);
    executor::Executor exec(&db);
    Exec(exec, "CREATE TABLE orders (id INT, customer_id INT, amount DOUBLE)");

    constexpr std::size_t kRows = 20000;
    for (std::size_t i = 0; i < kRows; ++i) {
      Exec(exec, "INSERT INTO orders VALUES (" + std::to_string(i) + ", " + std::to_string(i % 500) + ", 1.0)");
    }

    constexpr std::size_t kQueries = 300;
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> pick_customer(0, 499);

    Run("SELECT ... WHERE customer_id = ?  (SeqScan, no index)", kQueries, "20k-row table, 300 point lookups", [&]() {
      for (std::size_t i = 0; i < kQueries; ++i) {
        Exec(exec, "SELECT id FROM orders WHERE customer_id = " + std::to_string(pick_customer(rng)));
      }
    });

    Exec(exec, "CREATE INDEX idx_customer ON orders (customer_id)");

    Run("SELECT ... WHERE customer_id = ?  (IndexScan, indexed)", kQueries, "same table & query shape, now indexed", [&]() {
      for (std::size_t i = 0; i < kQueries; ++i) {
        Exec(exec, "SELECT id FROM orders WHERE customer_id = " + std::to_string(pick_customer(rng)));
      }
    });

  }
  std::filesystem::remove_all(dir);
}

void BenchUpdateAndDeleteThroughput() {
  std::string dir = TmpDir("update_delete");
  {
    Database db(dir);
    executor::Executor exec(&db);
    Exec(exec, "CREATE TABLE t (id INT, val INT)");

    constexpr std::size_t kRows = 10000;
    for (std::size_t i = 0; i < kRows; ++i) Exec(exec, "INSERT INTO t VALUES (" + std::to_string(i) + ", 0)");

    constexpr std::size_t kUpdates = 2000;
    std::mt19937 rng(5);
    std::uniform_int_distribution<std::size_t> pick(0, kRows - 1);

    Run("UPDATE ... WHERE id = ?  (single-row match, SeqScan)", kUpdates, "10k-row table", [&]() {
      for (std::size_t i = 0; i < kUpdates; ++i) {
        Exec(exec, "UPDATE t SET val = " + std::to_string(i) + " WHERE id = " + std::to_string(pick(rng)));
      }
    });

    Run("DELETE ... WHERE id = ?  (single-row match, SeqScan)", kUpdates, "same table, shrinking", [&]() {
      for (std::size_t i = 0; i < kUpdates; ++i) {
        Exec(exec, "DELETE FROM t WHERE id = " + std::to_string(i));
      }
    });

  }
  std::filesystem::remove_all(dir);
}

void BenchAggregateOverFullTable() {
  std::string dir = TmpDir("aggregate");
  {
    Database db(dir);
    executor::Executor exec(&db);
    Exec(exec, "CREATE TABLE sales (id INT, amount DOUBLE)");

    constexpr std::size_t kRows = 50000;
    for (std::size_t i = 0; i < kRows; ++i) {
      Exec(exec, "INSERT INTO sales VALUES (" + std::to_string(i) + ", " + std::to_string((i % 1000) + 0.99) + ")");
    }

    Run("SELECT COUNT(*), SUM(amount), AVG(amount) FROM sales", 1, "single-pass aggregate over 50k rows", [&]() {
      Exec(exec, "SELECT COUNT(*), SUM(amount), AVG(amount) FROM sales");
    });

  }
  std::filesystem::remove_all(dir);
}

void RunMacroBenchmarks() {
  PrintHeader("Macro: End-to-End SQL Workload (Phases 2-8, full pipeline)");
  BenchInsertThroughput();
  BenchSeqScanVsIndexScan();
  BenchUpdateAndDeleteThroughput();
  BenchAggregateOverFullTable();
}

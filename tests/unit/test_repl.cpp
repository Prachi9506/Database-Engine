#include "cli/repl.hpp"

#include <filesystem>
#include <sstream>

#include "test_framework.hpp"

using namespace minidb;
using namespace minidb::cli;

namespace {
std::string TempDir(const std::string& name) {
  std::string dir = TestTempRoot() + "minidb_test_repl_" + name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}
}  

TEST(Repl, ProcessStatementRunsCreateAndInsert) {
  std::string dir = TempDir("process_basic");
  {
    Database db(dir);
    executor::Executor exec(&db);

    std::string create_out = ProcessStatement(&exec, "CREATE TABLE t (id INT)");
    EXPECT_EQ(create_out, "CREATE TABLE");

    std::string insert_out = ProcessStatement(&exec, "INSERT INTO t VALUES (1)");
    EXPECT_EQ(insert_out, "INSERT 0 1");
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, ProcessStatementFormatsSelectAsTable) {
  std::string dir = TempDir("process_select");
  {
    Database db(dir);
    executor::Executor exec(&db);
    ProcessStatement(&exec, "CREATE TABLE t (id INT, name VARCHAR(20))");
    ProcessStatement(&exec, "INSERT INTO t VALUES (1, 'alice')");

    std::string out = ProcessStatement(&exec, "SELECT * FROM t");
    EXPECT_TRUE(out.find("id") != std::string::npos);
    EXPECT_TRUE(out.find("name") != std::string::npos);
    EXPECT_TRUE(out.find("alice") != std::string::npos);
    EXPECT_TRUE(out.find("(1 row)") != std::string::npos);
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, ProcessStatementReportsErrorsWithoutCrashing) {
  std::string dir = TempDir("process_error");
  {
    Database db(dir);
    executor::Executor exec(&db);
    std::string out = ProcessStatement(&exec, "SELECT * FROM nonexistent_table");
    EXPECT_TRUE(out.rfind("ERROR:", 0) == 0);  
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, ProcessStatementReportsSyntaxErrors) {
  std::string dir = TempDir("process_syntax");
  {
    Database db(dir);
    executor::Executor exec(&db);
    std::string out = ProcessStatement(&exec, "NOT VALID SQL AT ALL");
    EXPECT_TRUE(out.rfind("ERROR:", 0) == 0);
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, RunHandlesMultiLineStatementsUntilSemicolon) {
  std::string dir = TempDir("multiline");
  {
    Database db(dir);
    Repl repl(&db);

    std::istringstream in(
        "CREATE TABLE t (\n"
        "  id INT,\n"
        "  name VARCHAR(20)\n"
        ");\n"
        "INSERT INTO t VALUES (1, 'alice');\n"
        "SELECT * FROM t;\n"
        ".exit\n");
    std::ostringstream out;
    repl.Run(in, out);

    std::string output = out.str();
    EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos);
    EXPECT_TRUE(output.find("INSERT 0 1") != std::string::npos);
    EXPECT_TRUE(output.find("alice") != std::string::npos);
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, RunIgnoresSemicolonInsideStringLiteral) {
  std::string dir = TempDir("semicolon_in_string");
  {
    Database db(dir);
    Repl repl(&db);

    std::istringstream in(
        "CREATE TABLE t (id INT, note VARCHAR(50));\n"
        "INSERT INTO t VALUES (1, 'a;b');\n"
        "SELECT note FROM t;\n"
        ".exit\n");
    std::ostringstream out;
    repl.Run(in, out);

    EXPECT_TRUE(out.str().find("a;b") != std::string::npos);
  }
  std::filesystem::remove_all(dir);
}

TEST(Repl, ExitCommandStopsTheLoop) {
  std::string dir = TempDir("exit_cmd");
  {
    Database db(dir);
    Repl repl(&db);
    std::istringstream in(".exit\nCREATE TABLE should_not_run (id INT);\n");
    std::ostringstream out;
    repl.Run(in, out);
    EXPECT_TRUE(out.str().find("CREATE TABLE") == std::string::npos);
  }
  std::filesystem::remove_all(dir);
}

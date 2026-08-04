#include "database.hpp"

#include <cstdio>
#include <filesystem>

#include "test_framework.hpp"

using namespace minidb;
using namespace minidb::common;
using namespace minidb::storage;

namespace {
std::string TempDir(const std::string& name) {
  std::string dir = TestTempRoot() + "minidb_test_dir_" + name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

Schema UsersSchema() {
  return Schema({
      Column{"id", ColumnType::kInt, false, 0},
      Column{"name", ColumnType::kVarchar, true, 100},
      Column{"age", ColumnType::kInt, true, 0},
  });
}
}  

TEST(Database, CreateTableInsertAndScan) {
  std::string dir = TempDir("basic");
  {
    Database db(dir);
    EXPECT_TRUE(db.Tables().CreateTable("users", UsersSchema()).ok());

    EXPECT_TRUE(db.Tables().InsertRow("users", {std::int32_t(1), std::string("alice"), std::int32_t(30)}).ok());
    EXPECT_TRUE(db.Tables().InsertRow("users", {std::int32_t(2), std::string("bob"), std::int32_t(25)}).ok());
    EXPECT_TRUE(db.Tables().InsertRow("users", {std::int32_t(3), std::string("carol"), std::int32_t(40)}).ok());

    int count = 0;
    db.Tables().Scan("users", [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 3);
  }
  std::filesystem::remove_all(dir);
}

TEST(Database, InsertingIntoUnknownTableFails) {
  std::string dir = TempDir("unknown_table");
  {
    Database db(dir);
    auto result = db.Tables().InsertRow("ghost", {std::int32_t(1)});
    EXPECT_FALSE(result.ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Database, UpdateAndDeleteRow) {
  std::string dir = TempDir("update_delete");
  {
    Database db(dir);
    db.Tables().CreateTable("users", UsersSchema());
    auto rid = db.Tables().InsertRow("users", {std::int32_t(1), std::string("alice"), std::int32_t(30)});
    EXPECT_TRUE(rid.ok());

    auto updated = db.Tables().UpdateRow("users", rid.value(),
                                          {std::int32_t(1), std::string("alice2"), std::int32_t(31)});
    EXPECT_TRUE(updated.ok());

    auto row = db.Tables().GetRow("users", updated.value());
    EXPECT_TRUE(row.ok());
    EXPECT_EQ(std::get<std::string>(row.value()[1]), "alice2");
    EXPECT_EQ(std::get<std::int32_t>(row.value()[2]), 31);

    EXPECT_TRUE(db.Tables().DeleteRow("users", updated.value()).ok());
    EXPECT_FALSE(db.Tables().GetRow("users", updated.value()).ok());
  }
  std::filesystem::remove_all(dir);
}

TEST(Database, DropTableThenRecreateWithDifferentSchema) {
  std::string dir = TempDir("drop_recreate");
  {
    Database db(dir);
    db.Tables().CreateTable("t", UsersSchema());
    db.Tables().InsertRow("t", {std::int32_t(1), std::string("x"), std::int32_t(1)});
    EXPECT_TRUE(db.Tables().DropTable("t").ok());

    Schema smaller({Column{"only_col", ColumnType::kBigInt, false, 0}});
    EXPECT_TRUE(db.Tables().CreateTable("t", smaller).ok());
    auto rid = db.Tables().InsertRow("t", {std::int64_t(999)});
    EXPECT_TRUE(rid.ok());
    auto row = db.Tables().GetRow("t", rid.value());
    EXPECT_TRUE(row.ok());
    EXPECT_EQ(std::get<std::int64_t>(row.value()[0]), 999LL);
  }
  std::filesystem::remove_all(dir);
}

TEST(Database, EverythingSurvivesFullRestart) {
  std::string dir = TempDir("full_restart");
  {
    Database db(dir);
    db.Tables().CreateTable("users", UsersSchema());
    db.Tables().InsertRow("users", {std::int32_t(1), std::string("alice"), std::int32_t(30)});
    db.Tables().InsertRow("users", {std::int32_t(2), std::string("bob"), std::int32_t(25)});
    db.FlushAll();
  }  
  {
    Database db(dir);  

    auto tables = db.Tables().ListTables();
    EXPECT_EQ(tables.size(), std::size_t(1));

    int count = 0;
    std::vector<std::string> names;
    db.Tables().Scan("users", [&](RecordId, const std::vector<Value>& row) {
      ++count;
      names.push_back(std::get<std::string>(row[1]));
    });
    EXPECT_EQ(count, 2);

    auto rid = db.Tables().InsertRow("users", {std::int32_t(3), std::string("carol"), std::int32_t(22)});
    EXPECT_TRUE(rid.ok());
    auto row = db.Tables().GetRow("users", rid.value());
    EXPECT_TRUE(row.ok());
    EXPECT_EQ(std::get<std::string>(row.value()[1]), "carol");
  }
  std::filesystem::remove_all(dir);
}

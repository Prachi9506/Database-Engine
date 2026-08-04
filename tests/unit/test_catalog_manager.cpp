#include "catalog/catalog_manager.hpp"

#include <cstdio>

#include "buffer/buffer_pool.hpp"
#include "storage/page_manager.hpp"
#include "test_framework.hpp"

using namespace minidb::common;
using namespace minidb::catalog;
using namespace minidb::storage;
using namespace minidb::buffer;

namespace {
std::string TempPath(const std::string& name) { return TestTempRoot() + "minidb_test_" + name + ".db"; }

Schema MakeSchema() {
  return Schema({
      Column{"id", ColumnType::kInt, false, 0},
      Column{"name", ColumnType::kVarchar, true, 100},
  });
}
}

TEST(CatalogManager, CreateAndGetTable) {
  std::string path = TempPath("cat_create");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, kInvalidPageId, kInvalidPageId);

    auto created = cat.CreateTable("users", MakeSchema(), /*data_first_page_id=*/42);
    EXPECT_TRUE(created.ok());
    EXPECT_EQ(created.value()->table_name, "users");
    EXPECT_EQ(created.value()->first_page_id, 42);
    EXPECT_EQ(created.value()->schema.ColumnCount(), std::size_t(2));

    TableMetadata* fetched = cat.GetTable("users");
    EXPECT_TRUE(fetched != nullptr);
    EXPECT_EQ(fetched->table_name, "users");
  }
  std::remove(path.c_str());
}

TEST(CatalogManager, DuplicateTableNameRejected) {
  std::string path = TempPath("cat_dup");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, kInvalidPageId, kInvalidPageId);
    EXPECT_TRUE(cat.CreateTable("t", MakeSchema(), 1).ok());
    auto second = cat.CreateTable("t", MakeSchema(), 2);
    EXPECT_FALSE(second.ok());
  }
  std::remove(path.c_str());
}

TEST(CatalogManager, DropTableRemovesIt) {
  std::string path = TempPath("cat_drop");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, kInvalidPageId, kInvalidPageId);
    cat.CreateTable("t", MakeSchema(), 1);
    EXPECT_TRUE(cat.DropTable("t").ok());
    EXPECT_TRUE(cat.GetTable("t") == nullptr);
    EXPECT_FALSE(cat.DropTable("t").ok());
  }
  std::remove(path.c_str());
}

TEST(CatalogManager, ListTablesReturnsAllCreated) {
  std::string path = TempPath("cat_list");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, kInvalidPageId, kInvalidPageId);
    cat.CreateTable("a", MakeSchema(), 1);
    cat.CreateTable("b", MakeSchema(), 2);
    cat.CreateTable("c", MakeSchema(), 3);
    auto names = cat.ListTables();
    EXPECT_EQ(names.size(), std::size_t(3));
  }
  std::remove(path.c_str());
}

TEST(CatalogManager, SchemaAndTablesSurviveRestart) {
  std::string path = TempPath("cat_restart");
  std::remove(path.c_str());
  page_id_t catalog_first_page;
  page_id_t index_first_page;
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, kInvalidPageId, kInvalidPageId);
    catalog_first_page = cat.CatalogFirstPageId();
    index_first_page = cat.IndexCatalogFirstPageId();

    cat.CreateTable("users", MakeSchema(), 7);
    cat.CreateTable("orders", Schema({Column{"order_id", ColumnType::kBigInt, false, 0}}), 15);
    bp.FlushAll();
  }
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    CatalogManager cat(&bp, catalog_first_page, index_first_page);  

    auto names = cat.ListTables();
    EXPECT_EQ(names.size(), std::size_t(2));

    TableMetadata* users = cat.GetTable("users");
    EXPECT_TRUE(users != nullptr);
    EXPECT_EQ(users->first_page_id, 7);
    EXPECT_EQ(users->schema.ColumnCount(), std::size_t(2));
    EXPECT_EQ(users->schema.At(0).name, "id");
    EXPECT_EQ(users->schema.At(1).name, "name");

    TableMetadata* orders = cat.GetTable("orders");
    EXPECT_TRUE(orders != nullptr);
    EXPECT_EQ(orders->first_page_id, 15);

    auto created = cat.CreateTable("products", MakeSchema(), 99);
    EXPECT_TRUE(created.ok());
    EXPECT_TRUE(created.value()->table_id > users->table_id);
    EXPECT_TRUE(created.value()->table_id > orders->table_id);
  }
  std::remove(path.c_str());
}

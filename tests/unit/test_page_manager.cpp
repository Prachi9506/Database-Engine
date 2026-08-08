#include "storage/page_manager.hpp"

#include <cstdio>
#include <cstring>

#include "storage/slotted_page.hpp"
#include "test_framework.hpp"

using namespace minidb::storage;

namespace {
std::string TempPath(const std::string& name) {
  return TestTempRoot() + "minidb_test_" + name + ".db";
}
}

TEST(PageManager, AllocateReadWriteRoundTrip) {
  std::string path = TempPath("pm_roundtrip");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    auto alloc = pm.AllocatePage();
    EXPECT_TRUE(alloc.ok());
    page_id_t pid = alloc.value();
    EXPECT_EQ(pid, minidb::config::kFirstDataPageId);

    Page page;
    std::memcpy(page.Data(), "round-trip-bytes", 16);
    EXPECT_TRUE(pm.WritePage(pid, page).ok());

    Page read_back;
    EXPECT_TRUE(pm.ReadPage(pid, &read_back).ok());
    EXPECT_TRUE(std::memcmp(read_back.Data(), "round-trip-bytes", 16) == 0);
  }
  std::remove(path.c_str());
}

TEST(PageManager, AllocatedPageIdsAreSequential) {
  std::string path = TempPath("pm_sequential");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    auto a = pm.AllocatePage();
    auto b = pm.AllocatePage();
    auto c = pm.AllocatePage();
    EXPECT_TRUE(a.ok() && b.ok() && c.ok());
    EXPECT_EQ(b.value(), a.value() + 1);
    EXPECT_EQ(c.value(), b.value() + 1);
  }
  std::remove(path.c_str());
}

TEST(PageManager, StatePersistsAcrossReopen) {
  std::string path = TempPath("pm_persist");
  std::remove(path.c_str());

  page_id_t written_id;
  {
    PageManager pm(path);
    auto alloc = pm.AllocatePage();
    written_id = alloc.value();
    Page page;
    std::memcpy(page.Data(), "persisted-across-restart", 25);
    pm.WritePage(written_id, page);
  }  

  {
    PageManager pm(path);  
    Page read_back;
    EXPECT_TRUE(pm.ReadPage(written_id, &read_back).ok());
    EXPECT_TRUE(std::memcmp(read_back.Data(), "persisted-across-restart", 25) == 0);

    auto next = pm.AllocatePage();
    EXPECT_TRUE(next.ok());
    EXPECT_TRUE(next.value() > written_id);
  }
  std::remove(path.c_str());
}

TEST(PageManager, ReadOutOfRangeFails) {
  std::string path = TempPath("pm_oob");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    Page page;
    auto status = pm.ReadPage(999, &page);
    EXPECT_FALSE(status.ok());
  }
  std::remove(path.c_str());
}

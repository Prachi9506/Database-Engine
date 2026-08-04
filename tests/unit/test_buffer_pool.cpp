#include "buffer/buffer_pool.hpp"

#include <cstdio>
#include <cstring>

#include "storage/page_manager.hpp"
#include "test_framework.hpp"

using namespace minidb::storage;
using namespace minidb::buffer;

namespace {
std::string TempPath(const std::string& name) {
  return TestTempRoot() + "minidb_test_" + name + ".db";
}
}  

TEST(BufferPool, NewPageThenFetchReturnsSameContent) {
  std::string path = TempPath("bp_basic");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, /*pool_size=*/4);

    page_id_t pid;
    Page* p = bp.NewPage(&pid);
    EXPECT_TRUE(p != nullptr);
    std::memcpy(p->Data(), "buffered", 8);
    bp.UnpinPage(pid, true);

    Page* fetched = bp.FetchPage(pid);
    EXPECT_TRUE(fetched != nullptr);
    EXPECT_TRUE(std::memcmp(fetched->Data(), "buffered", 8) == 0);
    bp.UnpinPage(pid, false);
  }
  std::remove(path.c_str());
}

TEST(BufferPool, EvictsLeastRecentlyUsedWhenFull) {
  std::string path = TempPath("bp_lru");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, /*pool_size=*/2);  

    page_id_t p1, p2, p3;
    Page* page1 = bp.NewPage(&p1);
    std::memcpy(page1->Data(), "page1", 5);
    bp.UnpinPage(p1, true);

    Page* page2 = bp.NewPage(&p2);
    std::memcpy(page2->Data(), "page2", 5);
    bp.UnpinPage(p2, true);


    Page* page3 = bp.NewPage(&p3);
    EXPECT_TRUE(page3 != nullptr);
    std::memcpy(page3->Data(), "page3", 5);
    bp.UnpinPage(p3, true);

    Page* refetched = bp.FetchPage(p1);
    EXPECT_TRUE(refetched != nullptr);
    EXPECT_TRUE(std::memcmp(refetched->Data(), "page1", 5) == 0);
    bp.UnpinPage(p1, false);
  }
  std::remove(path.c_str());
}

TEST(BufferPool, PinnedPagesAreNotEvicted) {
  std::string path = TempPath("bp_pin");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, /*pool_size=*/1);  

    page_id_t p1;
    Page* page1 = bp.NewPage(&p1);
    std::memcpy(page1->Data(), "pinned", 6);

    page_id_t p2;
    Page* page2 = bp.NewPage(&p2);  
    EXPECT_TRUE(page2 == nullptr);  

    bp.UnpinPage(p1, true);
  }
  std::remove(path.c_str());
}

TEST(BufferPool, FlushAllPersistsDirtyPages) {
  std::string path = TempPath("bp_flush");
  std::remove(path.c_str());
  page_id_t pid;
  {
    PageManager pm(path);
    BufferPool bp(&pm, 4);
    Page* p = bp.NewPage(&pid);
    std::memcpy(p->Data(), "flush-me", 8);
    bp.UnpinPage(pid, true);
    EXPECT_TRUE(bp.FlushAll().ok());
  }
  {

    PageManager pm(path);
    Page page;
    EXPECT_TRUE(pm.ReadPage(pid, &page).ok());
    EXPECT_TRUE(std::memcmp(page.Data(), "flush-me", 8) == 0);
  }
  std::remove(path.c_str());
}

TEST(BufferPool, TracksHitAndMissCounts) {
  std::string path = TempPath("bp_stats");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 4);

    page_id_t p1;
    bp.NewPage(&p1);
    bp.UnpinPage(p1, true);
    EXPECT_EQ(bp.MissCount(), std::size_t(0));  

    bp.FetchPage(p1);  
    bp.UnpinPage(p1, false);
    EXPECT_EQ(bp.HitCount(), std::size_t(1));

    bp.ResetStats();
    EXPECT_EQ(bp.HitCount(), std::size_t(0));
    EXPECT_EQ(bp.MissCount(), std::size_t(0));
  }
  std::remove(path.c_str());
}

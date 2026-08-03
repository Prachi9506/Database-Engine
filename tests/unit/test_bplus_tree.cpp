#include "index/bplus_tree.hpp"

#include <algorithm>
#include <cstdio>
#include <set>

#include "buffer/buffer_pool.hpp"
#include "storage/page_manager.hpp"
#include "test_framework.hpp"

using namespace minidb::index;
using namespace minidb::storage;
using namespace minidb::buffer;

namespace {
std::string TempPath(const std::string& name) { return TestTempRoot() + "minidb_test_bpt_" + name + ".db"; }
}  

TEST(BPlusTree, InsertAndSearchSingleKey) {
  std::string path = TempPath("single");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);

    EXPECT_TRUE(tree.Insert(42, RecordId{1, 5}).ok());
    auto result = tree.SearchEqual(42);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), std::size_t(1));
    EXPECT_EQ(result.value()[0].page_id, 1);
    EXPECT_EQ(result.value()[0].slot_id, 5);
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, SearchMissingKeyReturnsEmpty) {
  std::string path = TempPath("missing");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);
    tree.Insert(1, RecordId{1, 0});
    auto result = tree.SearchEqual(999);
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(result.value().empty());
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, ManyInsertsForceMultiLevelSplits) {

  std::string path = TempPath("many_inserts");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);

    const int kCount = 5000;
    for (int i = 0; i < kCount; ++i) {
      auto status = tree.Insert(i, RecordId{static_cast<page_id_t>(i / 100 + 1), static_cast<slot_id_t>(i % 100)});
      EXPECT_TRUE(status.ok());
    }

    for (int i = 0; i < kCount; i += 37) {  
      auto result = tree.SearchEqual(i);
      EXPECT_TRUE(result.ok());
      EXPECT_EQ(result.value().size(), std::size_t(1));
      EXPECT_EQ(result.value()[0].page_id, static_cast<page_id_t>(i / 100 + 1));
      EXPECT_EQ(result.value()[0].slot_id, static_cast<slot_id_t>(i % 100));
    }
    auto missing = tree.SearchEqual(kCount + 500);
    EXPECT_TRUE(missing.ok());
    EXPECT_TRUE(missing.value().empty());
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, DuplicateKeysAllReturnedBySearch) {
  std::string path = TempPath("duplicates");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);


    const int kDuplicates = 150;  
    for (int i = 0; i < 50; ++i) tree.Insert(-(i + 1), RecordId{1, 0});  
    for (int i = 0; i < kDuplicates; ++i) {
      tree.Insert(7, RecordId{static_cast<page_id_t>(i + 1), 0});
    }
    for (int i = 0; i < 50; ++i) tree.Insert(1000 + i, RecordId{1, 0});  

    auto result = tree.SearchEqual(7);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), std::size_t(kDuplicates));

    std::set<page_id_t> distinct_pages;
    for (auto& rid : result.value()) distinct_pages.insert(rid.page_id);
    EXPECT_EQ(distinct_pages.size(), std::size_t(kDuplicates));  

    auto neg = tree.SearchEqual(-25);
    EXPECT_TRUE(neg.ok());
    EXPECT_EQ(neg.value().size(), std::size_t(1));
    auto pos = tree.SearchEqual(1025);
    EXPECT_TRUE(pos.ok());
    EXPECT_EQ(pos.value().size(), std::size_t(1));
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, RangeScanInclusiveAndExclusiveBounds) {
  std::string path = TempPath("range");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);
    for (int i = 0; i < 1000; ++i) tree.Insert(i, RecordId{1, static_cast<slot_id_t>(i % 65535)});

    auto inclusive = tree.RangeScan(100, true, 200, true);
    EXPECT_TRUE(inclusive.ok());
    EXPECT_EQ(inclusive.value().size(), std::size_t(101));  
    EXPECT_EQ(inclusive.value().front().first, 100);
    EXPECT_EQ(inclusive.value().back().first, 200);

    auto exclusive = tree.RangeScan(100, false, 200, false);
    EXPECT_TRUE(exclusive.ok());
    EXPECT_EQ(exclusive.value().size(), std::size_t(99));  

    auto unbounded_low = tree.RangeScan(std::nullopt, true, 5, true);
    EXPECT_TRUE(unbounded_low.ok());
    EXPECT_EQ(unbounded_low.value().size(), std::size_t(6));  

    auto unbounded_high = tree.RangeScan(995, true, std::nullopt, true);
    EXPECT_TRUE(unbounded_high.ok());
    EXPECT_EQ(unbounded_high.value().size(), std::size_t(5));  
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, DeleteRemovesOnlySpecifiedEntry) {
  std::string path = TempPath("delete");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);

    tree.Insert(5, RecordId{1, 0});
    tree.Insert(5, RecordId{2, 0});  
    tree.Insert(5, RecordId{3, 0});

    EXPECT_TRUE(tree.Delete(5, RecordId{2, 0}).ok());

    auto result = tree.SearchEqual(5);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), std::size_t(2));
    for (auto& rid : result.value()) EXPECT_TRUE(rid.page_id != 2);

    EXPECT_FALSE(tree.Delete(5, RecordId{99, 0}).ok());
    EXPECT_FALSE(tree.Delete(12345, RecordId{1, 0}).ok());
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, DeleteAcrossThousandsOfKeysStillCorrect) {
  std::string path = TempPath("delete_many");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);

    const int kCount = 3000;
    for (int i = 0; i < kCount; ++i) tree.Insert(i, RecordId{1, static_cast<slot_id_t>(i % 65535)});

    int deleted = 0;
    for (int i = 0; i < kCount; i += 3) {
      auto status = tree.Delete(i, RecordId{1, static_cast<slot_id_t>(i % 65535)});
      EXPECT_TRUE(status.ok());
      ++deleted;
    }

    for (int i = 0; i < kCount; ++i) {
      auto result = tree.SearchEqual(i);
      EXPECT_TRUE(result.ok());
      if (i % 3 == 0) {
        EXPECT_TRUE(result.value().empty());
      } else {
        EXPECT_EQ(result.value().size(), std::size_t(1));
      }
    }
  }
  std::remove(path.c_str());
}

TEST(BPlusTree, TreeSurvivesRestartViaMetaPageId) {
  std::string path = TempPath("restart");
  std::remove(path.c_str());
  page_id_t meta_id;
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, kInvalidPageId);
    meta_id = tree.MetaPageId();
    for (int i = 0; i < 2000; ++i) tree.Insert(i, RecordId{1, static_cast<slot_id_t>(i % 65535)});
    bp.FlushAll();
  }
  {
    PageManager pm(path);
    BufferPool bp(&pm, 64);
    BPlusTree tree(&bp, meta_id);  

    auto result = tree.SearchEqual(1500);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value().size(), std::size_t(1));

    EXPECT_TRUE(tree.Insert(99999, RecordId{9, 9}).ok());
    auto check = tree.SearchEqual(99999);
    EXPECT_TRUE(check.ok());
    EXPECT_EQ(check.value().size(), std::size_t(1));
  }
  std::remove(path.c_str());
}

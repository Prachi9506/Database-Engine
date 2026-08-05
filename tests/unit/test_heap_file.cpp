#include "storage/heap_file.hpp"

#include <cstdio>
#include <set>

#include "buffer/buffer_pool.hpp"
#include "storage/page_manager.hpp"
#include "test_framework.hpp"

using namespace minidb::storage;
using namespace minidb::buffer;

namespace {
std::string TempPath(const std::string& name) {
  return TestTempRoot() + "minidb_test_" + name + ".db";
}
}

TEST(HeapFile, InsertAndGetSingleRecord) {
  std::string path = TempPath("hf_single");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);

    auto rid = hf.InsertRecord("row-one", 7);
    EXPECT_TRUE(rid.ok());

    auto bytes = hf.GetRecordBytes(rid.value());
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(bytes.value(), "row-one");
  }
  std::remove(path.c_str());
}

TEST(HeapFile, InsertManyRecordsSpansMultiplePages) {
  std::string path = TempPath("hf_multipage");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);


    std::vector<RecordId> rids;
    std::string filler(90, 'z');
    for (int i = 0; i < 150; ++i) {
      std::string row = "row" + std::to_string(i) + "_" + filler;
      auto rid = hf.InsertRecord(row.data(), static_cast<std::uint16_t>(row.size()));
      EXPECT_TRUE(rid.ok());
      rids.push_back(rid.value());
    }

    std::set<page_id_t> distinct_pages;
    for (auto& r : rids) distinct_pages.insert(r.page_id);
    EXPECT_TRUE(distinct_pages.size() > 1);

    for (int i = 0; i < 150; ++i) {
      std::string expected = "row" + std::to_string(i) + "_" + filler;
      auto bytes = hf.GetRecordBytes(rids[static_cast<std::size_t>(i)]);
      EXPECT_TRUE(bytes.ok());
      EXPECT_EQ(bytes.value(), expected);
    }
  }
  std::remove(path.c_str());
}

TEST(HeapFile, ScanVisitsAllLiveRecordsExactlyOnce) {
  std::string path = TempPath("hf_scan");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);

    std::string filler(90, 'w');
    for (int i = 0; i < 60; ++i) {
      std::string row = "r" + std::to_string(i) + filler;
      hf.InsertRecord(row.data(), static_cast<std::uint16_t>(row.size()));
    }

    int visited = 0;
    hf.Scan([&](RecordId, const char*, std::uint16_t) { ++visited; });
    EXPECT_EQ(visited, 60);
  }
  std::remove(path.c_str());
}

TEST(HeapFile, DeleteRemovesRecordFromScan) {
  std::string path = TempPath("hf_delete");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);

    auto r1 = hf.InsertRecord("keep-me", 7);
    auto r2 = hf.InsertRecord("delete-me", 9);
    EXPECT_TRUE(hf.DeleteRecord(r2.value()).ok());

    int visited = 0;
    hf.Scan([&](RecordId rid, const char* data, std::uint16_t len) {
      ++visited;
      EXPECT_EQ(rid.page_id, r1.value().page_id);
      EXPECT_EQ(std::string(data, len), "keep-me");
    });
    EXPECT_EQ(visited, 1);

    EXPECT_FALSE(hf.GetRecordBytes(r2.value()).ok());
  }
  std::remove(path.c_str());
}

TEST(HeapFile, UpdateShrinkInPlaceKeepsSameRecordId) {
  std::string path = TempPath("hf_update_shrink");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);

    auto rid = hf.InsertRecord("original-longer-value", 22);
    auto updated = hf.UpdateRecord(rid.value(), "shorter", 7);
    EXPECT_TRUE(updated.ok());
    EXPECT_EQ(updated.value(), rid.value());  

    auto bytes = hf.GetRecordBytes(updated.value());
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(bytes.value(), "shorter");
  }
  std::remove(path.c_str());
}

TEST(HeapFile, UpdateGrowReturnsNewRecordId) {
  std::string path = TempPath("hf_update_grow");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);

    auto rid = hf.InsertRecord("short", 5);
    std::string bigger(500, 'g');
    auto updated = hf.UpdateRecord(rid.value(), bigger.data(), static_cast<std::uint16_t>(bigger.size()));
    EXPECT_TRUE(updated.ok());

  
    auto bytes = hf.GetRecordBytes(updated.value());
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(bytes.value(), bigger);
  }
  std::remove(path.c_str());
}

TEST(HeapFile, UpdateGrowSpillsToNewPageWhenFirstPageIsFull) {
  std::string path = TempPath("hf_update_grow_spill");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);


    std::string filler(90, 'p');
    RecordId target;
    for (int i = 0; i < 40; ++i) {
      std::string row = "r" + std::to_string(i) + filler;
      auto r = hf.InsertRecord(row.data(), static_cast<std::uint16_t>(row.size()));
      if (!r.ok()) break;
      if (i == 5) target = r.value();  
    }

    std::string bigger(500, 'G');
    auto updated = hf.UpdateRecord(target, bigger.data(), static_cast<std::uint16_t>(bigger.size()));
    EXPECT_TRUE(updated.ok());
    EXPECT_TRUE(!(updated.value() == target));  

    auto bytes = hf.GetRecordBytes(updated.value());
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(bytes.value(), bigger);
  }
  std::remove(path.c_str());
}

TEST(HeapFile, DataSurvivesRestart) {
  std::string path = TempPath("hf_restart");
  std::remove(path.c_str());
  page_id_t first_page_id;
  RecordId rid;
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    first_page_id = hf.FirstPageId();
    auto r = hf.InsertRecord("durable-row", 11);
    rid = r.value();
    bp.FlushAll();
  }  
  {
    PageManager pm(path);            
    BufferPool bp(&pm, 8);           
    HeapFile hf(&bp, first_page_id); 

    auto bytes = hf.GetRecordBytes(rid);
    EXPECT_TRUE(bytes.ok());
    EXPECT_EQ(bytes.value(), "durable-row");
  }
  std::remove(path.c_str());
}

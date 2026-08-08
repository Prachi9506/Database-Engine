#include "storage/slotted_page.hpp"

#include <cstring>

#include "test_framework.hpp"

using namespace minidb::storage;

TEST(SlottedPage, InitStartsEmpty) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(/*page_id=*/7);
  EXPECT_EQ(sp.PageId(), 7);
  EXPECT_EQ(sp.SlotCount(), 0);
  EXPECT_EQ(sp.NextPageId(), kInvalidPageId);
}

TEST(SlottedPage, InsertAndGetSingleRecord) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);

  const char* data = "hello, minidb";
  auto slot = sp.InsertRecord(data, static_cast<std::uint16_t>(std::strlen(data)));
  EXPECT_TRUE(slot.has_value());
  EXPECT_EQ(*slot, 0);

  auto rec = sp.GetRecord(*slot);
  EXPECT_TRUE(rec.has_value());
  std::string got(rec->first, rec->second);
  EXPECT_EQ(got, std::string(data));
}

TEST(SlottedPage, InsertMultipleRecordsAndReadBack) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);

  std::vector<std::string> rows = {"alice", "bob", "charlotte", "dave"};
  std::vector<slot_id_t> slots;
  for (auto& r : rows) {
    auto s = sp.InsertRecord(r.data(), static_cast<std::uint16_t>(r.size()));
    EXPECT_TRUE(s.has_value());
    slots.push_back(*s);
  }
  EXPECT_EQ(sp.SlotCount(), 4);

  for (std::size_t i = 0; i < rows.size(); ++i) {
    auto rec = sp.GetRecord(slots[i]);
    EXPECT_TRUE(rec.has_value());
    EXPECT_EQ(std::string(rec->first, rec->second), rows[i]);
  }
}

TEST(SlottedPage, DeleteTombstonesRecord) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);
  auto slot = sp.InsertRecord("xyz", 3);
  EXPECT_TRUE(sp.DeleteRecord(*slot));
  EXPECT_FALSE(sp.GetRecord(*slot).has_value());
  EXPECT_TRUE(sp.IsTombstoned(*slot));
  EXPECT_FALSE(sp.DeleteRecord(*slot));
}

TEST(SlottedPage, InsertReusesTombstonedSlot) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);
  auto s0 = sp.InsertRecord("aaa", 3);
  auto s1 = sp.InsertRecord("bbb", 3);
  EXPECT_TRUE(sp.DeleteRecord(*s0));

  auto s2 = sp.InsertRecord("cc", 2);
  EXPECT_TRUE(s2.has_value());
  EXPECT_EQ(*s2, *s0);
  EXPECT_EQ(sp.SlotCount(), 2);  
  (void)s1;
}

TEST(SlottedPage, UpdateInPlaceShrinkSucceedsGrowFails) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);
  auto slot = sp.InsertRecord("0123456789", 10);

  EXPECT_TRUE(sp.UpdateRecordInPlace(*slot, "short", 5));
  auto rec = sp.GetRecord(*slot);
  EXPECT_EQ(std::string(rec->first, rec->second), "short");

  EXPECT_FALSE(sp.UpdateRecordInPlace(*slot, "this-is-way-too-long-for-the-slot", 34));
}

TEST(SlottedPage, InsertFailsWhenPageIsFull) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);

  std::string row(40, 'x');
  int inserted = 0;
  while (true) {
    auto s = sp.InsertRecord(row.data(), static_cast<std::uint16_t>(row.size()));
    if (!s.has_value()) break;
    ++inserted;
    EXPECT_TRUE(inserted < 1000);  
  }
  EXPECT_TRUE(inserted > 0);
  EXPECT_TRUE(sp.FreeSpace() < row.size());
}

TEST(SlottedPage, FreeSpaceShrinksAsRecordsAreAdded) {
  Page page;
  SlottedPage sp(&page);
  sp.Init(1);
  std::size_t before = sp.FreeSpace();
  sp.InsertRecord("some bytes", 10);
  std::size_t after = sp.FreeSpace();
  EXPECT_TRUE(after < before);
}

TEST(SlottedPage, DenselyPackedTinyRecordsNeverOverflowPageBoundary) {

  Page page;
  SlottedPage sp(&page);
  sp.Init(1);

  int inserted = 0;
  while (true) {
    auto slot = sp.InsertRecord("x", 1);
    if (!slot.has_value()) break;
    ++inserted;
    EXPECT_TRUE(inserted < 100000);  
  }

  EXPECT_TRUE(inserted > 0);
  std::size_t directory_end = SlottedPage::kHeaderSize + static_cast<std::size_t>(sp.SlotCount()) * SlottedPage::kSlotSize;
  EXPECT_TRUE(directory_end <= Page::Size());

  for (slot_id_t i = 0; i < sp.SlotCount(); ++i) {
    auto rec = sp.GetRecord(i);
    EXPECT_TRUE(rec.has_value());
    EXPECT_EQ(rec->second, std::uint16_t(1));
    EXPECT_EQ(rec->first[0], 'x');
  }
}

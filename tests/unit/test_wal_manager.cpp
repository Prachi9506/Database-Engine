#include "recovery/wal_manager.hpp"

#include <cstdio>
#include <cstring>

#include "test_framework.hpp"

using namespace minidb::recovery;
using namespace minidb::storage;

namespace {
std::string TempPath(const std::string& name) { return TestTempRoot() + "minidb_test_wal_" + name + ".log"; }

Page MakePage(char fill) {
  Page p;
  std::memset(p.Data(), fill, Page::Size());
  return p;
}
}  

TEST(WALManager, AppendAndReadBackUpdateRecord) {
  std::string path = TempPath("update");
  std::remove(path.c_str());
  {
    WALManager wal(path);
    Page before = MakePage('A');
    Page after = MakePage('B');
    lsn_t lsn = wal.AppendUpdate(/*txn_id=*/7, /*page_id=*/3, before, after);
    EXPECT_TRUE(lsn != kInvalidLsn);

    auto records = wal.ReadAll();
    EXPECT_TRUE(records.ok());
    EXPECT_EQ(records.value().size(), std::size_t(1));
    const WALRecord& r = records.value()[0];
    EXPECT_TRUE(r.type == WALRecordType::kUpdate);
    EXPECT_EQ(r.txn_id, std::uint64_t(7));
    EXPECT_EQ(r.page_id, 3);
    EXPECT_TRUE(std::memcmp(r.before_image.data(), before.Data(), Page::Size()) == 0);
    EXPECT_TRUE(std::memcmp(r.after_image.data(), after.Data(), Page::Size()) == 0);
  }
  std::remove(path.c_str());
}

TEST(WALManager, LsnsIncreaseMonotonically) {
  std::string path = TempPath("lsn_order");
  std::remove(path.c_str());
  {
    WALManager wal(path);
    lsn_t a = wal.AppendCommit(1);
    lsn_t b = wal.AppendCommit(2);
    lsn_t c = wal.AppendCommit(3);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(c > b);
  }
  std::remove(path.c_str());
}

TEST(WALManager, RecordsReadBackInAppendOrder) {
  std::string path = TempPath("order");
  std::remove(path.c_str());
  {
    WALManager wal(path);
    wal.AppendCommit(1);
    wal.AppendAbort(2);
    wal.AppendCheckpoint();
    wal.AppendCommit(3);

    auto records = wal.ReadAll();
    EXPECT_TRUE(records.ok());
    auto& recs = records.value();
    EXPECT_EQ(recs.size(), std::size_t(4));
    EXPECT_TRUE(recs[0].type == WALRecordType::kCommit);
    EXPECT_TRUE(recs[1].type == WALRecordType::kAbort);
    EXPECT_TRUE(recs[2].type == WALRecordType::kCheckpoint);
    EXPECT_TRUE(recs[3].type == WALRecordType::kCommit);
  }
  std::remove(path.c_str());
}

TEST(WALManager, LogPersistsAndContinuesLsnNumberingAcrossReopen) {
  std::string path = TempPath("persist");
  std::remove(path.c_str());
  lsn_t last_lsn_before;
  {
    WALManager wal(path);
    wal.AppendCommit(1);
    last_lsn_before = wal.AppendCommit(2);
  }
  {
    WALManager wal(path);  
    lsn_t new_lsn = wal.AppendCommit(3);
    EXPECT_TRUE(new_lsn > last_lsn_before);  

    auto records = wal.ReadAll();
    EXPECT_TRUE(records.ok());
    EXPECT_EQ(records.value().size(), std::size_t(3));  
  }
  std::remove(path.c_str());
}

TEST(WALManager, EmptyLogReadsBackEmpty) {
  std::string path = TempPath("empty");
  std::remove(path.c_str());
  {
    WALManager wal(path);
    auto records = wal.ReadAll();
    EXPECT_TRUE(records.ok());
    EXPECT_TRUE(records.value().empty());
  }
  std::remove(path.c_str());
}

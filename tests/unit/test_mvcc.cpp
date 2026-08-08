#include "transaction/mvcc.hpp"

#include <cstdio>

#include "buffer/buffer_pool.hpp"
#include "storage/page_manager.hpp"
#include "test_framework.hpp"
#include "transaction/transaction_manager.hpp"

using namespace minidb::common;
using namespace minidb::storage;
using namespace minidb::buffer;
using namespace minidb::transaction;

namespace {
std::string TempPath(const std::string& name) { return TestTempRoot() + "minidb_test_mvcc_" + name + ".db"; }

Schema MakeSchema() {
  return Schema({
      Column{"id", ColumnType::kInt, false, 0},
      Column{"name", ColumnType::kVarchar, true, 50},
  });
}
}  


TEST(MVCC, OwnUncommittedWriteIsVisibleToSelf) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  VersionMeta meta{t1->txn_id, kInvalidTxnId};
  EXPECT_TRUE(IsVisible(meta, *t1, &mgr));
}

TEST(MVCC, UncommittedWriteInvisibleToOtherTransaction) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  Transaction* t2 = mgr.Begin();
  VersionMeta meta{t1->txn_id, kInvalidTxnId};  
  EXPECT_FALSE(IsVisible(meta, *t2, &mgr));
}

TEST(MVCC, CommittedWriteVisibleToLaterTransaction) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  mgr.Commit(t1);
  Transaction* t2 = mgr.Begin();  
  VersionMeta meta{t1->txn_id, kInvalidTxnId};
  EXPECT_TRUE(IsVisible(meta, *t2, &mgr));
}

TEST(MVCC, SnapshotIsolationHidesLaterCommits) {

  TransactionManager mgr;
  Transaction* t1 = mgr.Begin(IsolationLevel::kSnapshotIsolation);
  Transaction* t2 = mgr.Begin(IsolationLevel::kSnapshotIsolation);
  VersionMeta meta{t1->txn_id, kInvalidTxnId};
  mgr.Commit(t1);
  EXPECT_FALSE(IsVisible(meta, *t2, &mgr));
}

TEST(MVCC, ReadCommittedSeesLaterCommitsWithinSameStatement) {
 
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin(IsolationLevel::kSnapshotIsolation);
  Transaction* t2 = mgr.Begin(IsolationLevel::kReadCommitted);
  VersionMeta meta{t1->txn_id, kInvalidTxnId};
  mgr.Commit(t1);
  EXPECT_TRUE(IsVisible(meta, *t2, &mgr));
}

TEST(MVCC, DeletedRowInvisibleOnceDeleterCommittedAndVisible) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  mgr.Commit(t1);
  Transaction* t2 = mgr.Begin();
  mgr.Commit(t2);  
  Transaction* t3 = mgr.Begin();  

  VersionMeta meta{t1->txn_id, t2->txn_id};
  EXPECT_FALSE(IsVisible(meta, *t3, &mgr));  
}

TEST(MVCC, DeletedRowStillVisibleToSnapshotBeforeDeleteCommitted) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  mgr.Commit(t1);
  Transaction* reader = mgr.Begin();  
  Transaction* deleter = mgr.Begin();
  VersionMeta meta{t1->txn_id, deleter->txn_id};
  mgr.Commit(deleter);  

  EXPECT_TRUE(IsVisible(meta, *reader, &mgr));  
}

TEST(MVCC, SelfDeletedRowInvisibleToSelf) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  mgr.Commit(t1);
  Transaction* t2 = mgr.Begin();
  VersionMeta meta{t1->txn_id, t2->txn_id};  
  EXPECT_FALSE(IsVisible(meta, *t2, &mgr));  
}


TEST(MVCC, InsertVisibleToSelfImmediately) {
  std::string path = TempPath("insert_self");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;

    Transaction* t1 = mgr.Begin();
    auto rid = store.InsertVersion(t1->txn_id, {std::int32_t(1), std::string("alice")});
    EXPECT_TRUE(rid.ok());

    int count = 0;
    store.ScanVisible(*t1, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 1);
  }
  std::remove(path.c_str());
}

TEST(MVCC, UncommittedInsertInvisibleToOtherTransaction) {
  std::string path = TempPath("insert_other");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;

    Transaction* t1 = mgr.Begin();
    Transaction* t2 = mgr.Begin();
    store.InsertVersion(t1->txn_id, {std::int32_t(1), std::string("alice")});

    int count = 0;
    store.ScanVisible(*t2, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 0);  

    mgr.Commit(t1);

    count = 0;
    store.ScanVisible(*t2, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 0);

    Transaction* t3 = mgr.Begin();
    count = 0;
    store.ScanVisible(*t3, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 1);
  }
  std::remove(path.c_str());
}

TEST(MVCC, UpdateCreatesNewVersionOldStillVisibleToConcurrentReader) {
  std::string path = TempPath("update_versions");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;

    Transaction* setup = mgr.Begin();
    auto rid = store.InsertVersion(setup->txn_id, {std::int32_t(1), std::string("alice")});
    mgr.Commit(setup);

    Transaction* reader = mgr.Begin();  
    Transaction* updater = mgr.Begin();
    auto new_rid = store.UpdateVersion(updater->txn_id, rid.value(), {std::int32_t(1), std::string("alice2")});
    EXPECT_TRUE(new_rid.ok());
    mgr.Commit(updater);

    std::vector<std::string> names_seen;
    store.ScanVisible(*reader, &mgr,
                       [&](RecordId, const std::vector<Value>& row) { names_seen.push_back(std::get<std::string>(row[1])); });
    EXPECT_EQ(names_seen.size(), std::size_t(1));
    EXPECT_EQ(names_seen[0], "alice");

    Transaction* fresh = mgr.Begin();
    names_seen.clear();
    store.ScanVisible(*fresh, &mgr,
                       [&](RecordId, const std::vector<Value>& row) { names_seen.push_back(std::get<std::string>(row[1])); });
    EXPECT_EQ(names_seen.size(), std::size_t(1));
    EXPECT_EQ(names_seen[0], "alice2");
  }
  std::remove(path.c_str());
}

TEST(MVCC, DeleteHidesRowFromLaterTransactionsOnly) {
  std::string path = TempPath("delete_versions");
  std::remove(path.c_str());
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;

    Transaction* setup = mgr.Begin();
    auto rid = store.InsertVersion(setup->txn_id, {std::int32_t(1), std::string("alice")});
    mgr.Commit(setup);

    Transaction* reader = mgr.Begin();  
    Transaction* deleter = mgr.Begin();
    EXPECT_TRUE(store.DeleteVersion(deleter->txn_id, rid.value()).ok());
    mgr.Commit(deleter);

    int count = 0;
    store.ScanVisible(*reader, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 1);  

    Transaction* fresh = mgr.Begin();
    count = 0;
    store.ScanVisible(*fresh, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 0);  
  }
  std::remove(path.c_str());
}

TEST(MVCC, TransactionIdsAreNotPersistedAcrossRestartKnownLimitation) {
  std::string path = TempPath("id_collision");
  std::remove(path.c_str());
  page_id_t first_page;
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, kInvalidPageId);
    first_page = hf.FirstPageId();
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;

    Transaction* t1 = mgr.Begin();  
    store.InsertVersion(t1->txn_id, {std::int32_t(1), std::string("durable")});
    mgr.Commit(t1);
    bp.FlushAll();
  }
  {
    PageManager pm(path);
    BufferPool bp(&pm, 8);
    HeapFile hf(&bp, first_page);
    Schema schema = MakeSchema();
    VersionedRecordStore store(&hf, &schema);
    TransactionManager mgr;  

    Transaction* reader = mgr.Begin();
    EXPECT_EQ(reader->txn_id, std::uint64_t(1));  

    int count = 0;
    store.ScanVisible(*reader, &mgr, [&](RecordId, const std::vector<Value>&) { ++count; });
    EXPECT_EQ(count, 1);
  }
  std::remove(path.c_str());
}

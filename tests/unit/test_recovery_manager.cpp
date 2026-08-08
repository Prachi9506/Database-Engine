#include "recovery/recovery_manager.hpp"

#include <cstdio>
#include <cstring>

#include "recovery/wal_manager.hpp"
#include "storage/page_manager.hpp"
#include "test_framework.hpp"

using namespace minidb::recovery;
using namespace minidb::storage;

namespace {
std::string DataPath(const std::string& name) { return TestTempRoot() + "minidb_test_recovery_" + name + "_data.db"; }
std::string WalPath(const std::string& name) { return TestTempRoot() + "minidb_test_recovery_" + name + "_wal.log"; }

Page MakePage(char fill) {
  Page p;
  std::memset(p.Data(), fill, Page::Size());
  return p;
}

bool PageContentIs(PageManager* pm, page_id_t pid, char expected_fill) {
  Page p;
  pm->ReadPage(pid, &p);
  Page expected = MakePage(expected_fill);
  return std::memcmp(p.Data(), expected.Data(), Page::Size()) == 0;
}
}  

TEST(RecoveryManager, CommittedTransactionIsRedoneAfterCrash) {

  std::string data_path = DataPath("redo_committed");
  std::string wal_path = WalPath("redo_committed");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t pid;
  {
    PageManager pm(data_path);
    auto alloc = pm.AllocatePage();
    pid = alloc.value();
    pm.WritePage(pid, MakePage('O'));  

    WALManager wal(wal_path);
    Page before = MakePage('O');
    Page after = MakePage('N');  
    wal.AppendUpdate(/*txn_id=*/1, pid, before, after);
    wal.AppendCommit(1);
    
  }  

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().redo_count, std::size_t(1));
    EXPECT_EQ(stats.value().undo_count, std::size_t(0));

    EXPECT_TRUE(PageContentIs(&pm, pid, 'N'));
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, UncommittedTransactionIsUndoneAfterCrash) {
  std::string data_path = DataPath("undo_uncommitted");
  std::string wal_path = WalPath("undo_uncommitted");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t pid;
  {
    PageManager pm(data_path);
    auto alloc = pm.AllocatePage();
    pid = alloc.value();
    pm.WritePage(pid, MakePage('O'));

    WALManager wal(wal_path);
    wal.AppendUpdate(/*txn_id=*/1, pid, MakePage('O'), MakePage('N'));
  }

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().redo_count, std::size_t(1));  
    EXPECT_EQ(stats.value().undo_count, std::size_t(1));  

    EXPECT_TRUE(PageContentIs(&pm, pid, 'O'));  
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, ExplicitlyAbortedTransactionIsAlsoUndone) {
  std::string data_path = DataPath("undo_aborted");
  std::string wal_path = WalPath("undo_aborted");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t pid;
  {
    PageManager pm(data_path);
    pid = pm.AllocatePage().value();
    pm.WritePage(pid, MakePage('O'));

    WALManager wal(wal_path);
    wal.AppendUpdate(1, pid, MakePage('O'), MakePage('N'));
    wal.AppendAbort(1);  
  }

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().undo_count, std::size_t(1));
    EXPECT_TRUE(PageContentIs(&pm, pid, 'O'));
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, MixedCommittedAndUncommittedTransactionsAcrossMultiplePages) {
  std::string data_path = DataPath("mixed");
  std::string wal_path = WalPath("mixed");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t p_committed, p_loser1, p_loser2;
  {
    PageManager pm(data_path);
    p_committed = pm.AllocatePage().value();
    p_loser1 = pm.AllocatePage().value();
    p_loser2 = pm.AllocatePage().value();
    pm.WritePage(p_committed, MakePage('O'));
    pm.WritePage(p_loser1, MakePage('O'));
    pm.WritePage(p_loser2, MakePage('O'));

    WALManager wal(wal_path);
    wal.AppendUpdate(1, p_committed, MakePage('O'), MakePage('N'));
    wal.AppendCommit(1);
    wal.AppendUpdate(2, p_loser1, MakePage('O'), MakePage('X'));
    wal.AppendUpdate(2, p_loser2, MakePage('O'), MakePage('Y'));
  }

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().redo_count, std::size_t(3));
    EXPECT_EQ(stats.value().undo_count, std::size_t(2));

    EXPECT_TRUE(PageContentIs(&pm, p_committed, 'N'));
    EXPECT_TRUE(PageContentIs(&pm, p_loser1, 'O'));
    EXPECT_TRUE(PageContentIs(&pm, p_loser2, 'O'));
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, CheckpointBoundsRedoButLoserBeforeCheckpointStillUndone) {

  std::string data_path = DataPath("checkpoint");
  std::string wal_path = WalPath("checkpoint");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t p_loser, p_after_checkpoint;
  {
    PageManager pm(data_path);
    p_loser = pm.AllocatePage().value();
    p_after_checkpoint = pm.AllocatePage().value();
    pm.WritePage(p_loser, MakePage('O'));
    pm.WritePage(p_after_checkpoint, MakePage('O'));

    WALManager wal(wal_path);
    wal.AppendUpdate(1, p_loser, MakePage('O'), MakePage('X'));  

    pm.WritePage(p_loser, MakePage('X'));
    lsn_t ckpt_lsn = wal.AppendCheckpoint();

    wal.AppendUpdate(2, p_after_checkpoint, MakePage('O'), MakePage('N'));
    wal.AppendCommit(2);

    EXPECT_TRUE(ckpt_lsn != kInvalidLsn);
  }

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_TRUE(stats.value().checkpoint_lsn_used != kInvalidLsn);
    EXPECT_EQ(stats.value().redo_count, std::size_t(1));
    EXPECT_EQ(stats.value().undo_count, std::size_t(1));

    EXPECT_TRUE(PageContentIs(&pm, p_loser, 'O'));            
    EXPECT_TRUE(PageContentIs(&pm, p_after_checkpoint, 'N'));  
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, RedoIsIdempotentWhenChangeWasAlreadyFlushed) {
 
  std::string data_path = DataPath("idempotent");
  std::string wal_path = WalPath("idempotent");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());

  page_id_t pid;
  {
    PageManager pm(data_path);
    pid = pm.AllocatePage().value();
    pm.WritePage(pid, MakePage('O'));

    WALManager wal(wal_path);
    wal.AppendUpdate(1, pid, MakePage('O'), MakePage('N'));
    wal.AppendCommit(1);
    pm.WritePage(pid, MakePage('N'));  
  }

  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().redo_count, std::size_t(1));  
    EXPECT_TRUE(PageContentIs(&pm, pid, 'N'));
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

TEST(RecoveryManager, EmptyLogRecoversTrivially) {
  std::string data_path = DataPath("empty_log");
  std::string wal_path = WalPath("empty_log");
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
  {
    PageManager pm(data_path);
    WALManager wal(wal_path);
    RecoveryManager recovery(&wal, &pm);
    auto stats = recovery.Recover();
    EXPECT_TRUE(stats.ok());
    EXPECT_EQ(stats.value().redo_count, std::size_t(0));
    EXPECT_EQ(stats.value().undo_count, std::size_t(0));
  }
  std::remove(data_path.c_str());
  std::remove(wal_path.c_str());
}

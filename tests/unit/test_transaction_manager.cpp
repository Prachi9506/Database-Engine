#include "transaction/transaction_manager.hpp"

#include "test_framework.hpp"

using namespace minidb::transaction;

TEST(TransactionManager, BeginAssignsIncreasingIds) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  Transaction* t2 = mgr.Begin();
  EXPECT_TRUE(t2->txn_id > t1->txn_id);
  EXPECT_TRUE(t1->state == TxnState::kActive);
}

TEST(TransactionManager, CommitChangesState) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  EXPECT_TRUE(mgr.Commit(t1).ok());
  EXPECT_TRUE(t1->state == TxnState::kCommitted);
  EXPECT_TRUE(mgr.IsCommitted(t1->txn_id));
  EXPECT_FALSE(mgr.IsActive(t1->txn_id));
}

TEST(TransactionManager, AbortChangesState) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  EXPECT_TRUE(mgr.Abort(t1).ok());
  EXPECT_TRUE(t1->state == TxnState::kAborted);
  EXPECT_FALSE(mgr.IsCommitted(t1->txn_id));
}

TEST(TransactionManager, DoubleCommitFails) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  EXPECT_TRUE(mgr.Commit(t1).ok());
  EXPECT_FALSE(mgr.Commit(t1).ok());
}

TEST(TransactionManager, SnapshotCapturesActiveTransactionsAtStart) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  Transaction* t2 = mgr.Begin();  
  EXPECT_TRUE(t2->active_at_start.count(t1->txn_id) == 1);

  mgr.Commit(t1);
  Transaction* t3 = mgr.Begin();  
  EXPECT_TRUE(t3->active_at_start.count(t1->txn_id) == 0);
  EXPECT_TRUE(t3->active_at_start.count(t2->txn_id) == 1);  
}

TEST(TransactionManager, GetTransactionAfterCommitStillWorks) {
  TransactionManager mgr;
  Transaction* t1 = mgr.Begin();
  txn_id_t id = t1->txn_id;
  mgr.Commit(t1);
  Transaction* looked_up = mgr.GetTransaction(id);
  EXPECT_TRUE(looked_up != nullptr);
  EXPECT_TRUE(looked_up->state == TxnState::kCommitted);
}

TEST(TransactionManager, UnknownTransactionIdReturnsNull) {
  TransactionManager mgr;
  EXPECT_TRUE(mgr.GetTransaction(99999) == nullptr);
}

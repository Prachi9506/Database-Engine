#include "transaction/lock_manager.hpp"

#include "test_framework.hpp"

using namespace minidb::transaction;
using minidb::storage::RecordId;

TEST(LockManager, SharedLocksFromDifferentTxnsAreCompatible) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockShared(1, r).ok());
  EXPECT_TRUE(lm.LockShared(2, r).ok());  
}

TEST(LockManager, ExclusiveConflictsWithExistingShared) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockShared(1, r).ok());
  auto result = lm.LockExclusive(2, r);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(result.code() == minidb::util::Code::kConflict);
}

TEST(LockManager, ExclusiveConflictsWithExistingExclusive) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockExclusive(1, r).ok());
  EXPECT_FALSE(lm.LockExclusive(2, r).ok());
  EXPECT_FALSE(lm.LockShared(2, r).ok());  
}

TEST(LockManager, SameTxnCanUpgradeSharedToExclusive) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockShared(1, r).ok());
  EXPECT_TRUE(lm.LockExclusive(1, r).ok());  
}

TEST(LockManager, UpgradeBlockedByOtherSharedHolders) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockShared(1, r).ok());
  EXPECT_TRUE(lm.LockShared(2, r).ok());
  EXPECT_FALSE(lm.LockExclusive(1, r).ok());  
}

TEST(LockManager, UnlockReleasesAndAllowsOthersToProceed) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockExclusive(1, r).ok());
  EXPECT_FALSE(lm.LockExclusive(2, r).ok());
  EXPECT_TRUE(lm.Unlock(1, r).ok());
  EXPECT_TRUE(lm.LockExclusive(2, r).ok());  
}

TEST(LockManager, ReleaseAllReleasesEveryLockForThatTxn) {
  LockManager lm;
  RecordId r1{1, 0}, r2{1, 1}, r3{1, 2};
  lm.LockExclusive(1, r1);
  lm.LockShared(1, r2);
  lm.LockExclusive(1, r3);

  lm.ReleaseAll(1);

  EXPECT_TRUE(lm.LockExclusive(2, r1).ok());
  EXPECT_TRUE(lm.LockExclusive(2, r2).ok());
  EXPECT_TRUE(lm.LockExclusive(2, r3).ok());
}

TEST(LockManager, UnlockWithoutHoldingFails) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_FALSE(lm.Unlock(1, r).ok());
}

TEST(LockManager, SameTxnLockingSameRowTwiceIsIdempotent) {
  LockManager lm;
  RecordId r{1, 0};
  EXPECT_TRUE(lm.LockShared(1, r).ok());
  EXPECT_TRUE(lm.LockShared(1, r).ok());  
}


TEST(LockManager, DetectsSimpleTwoCycle) {
  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph = {
      {1, {2}},
      {2, {1}},
  };
  std::vector<txn_id_t> cycle;
  EXPECT_TRUE(LockManager::DetectDeadlock(graph, &cycle));
  EXPECT_TRUE(cycle.size() >= 2);
}

TEST(LockManager, DetectsLongerCycle) {
  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph = {
      {1, {2}},
      {2, {3}},
      {3, {1}},
  };
  std::vector<txn_id_t> cycle;
  EXPECT_TRUE(LockManager::DetectDeadlock(graph, &cycle));
}

TEST(LockManager, NoCycleReturnsFalse) {

  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph = {
      {1, {2}},
      {2, {3}},
  };
  std::vector<txn_id_t> cycle;
  EXPECT_FALSE(LockManager::DetectDeadlock(graph, &cycle));
  EXPECT_TRUE(cycle.empty());
}

TEST(LockManager, EmptyGraphHasNoDeadlock) {
  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph;
  EXPECT_FALSE(LockManager::DetectDeadlock(graph, nullptr));
}

TEST(LockManager, DisjointComponentsOneWithCycleOneWithout) {

  std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>> graph = {
      {3, {4}},
      {1, {2}},
      {2, {1}},
  };
  EXPECT_TRUE(LockManager::DetectDeadlock(graph, nullptr));
}

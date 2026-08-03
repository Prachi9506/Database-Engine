#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "transaction/transaction.hpp"
#include "utilities/status.hpp"

namespace minidb::transaction {

class TransactionManager {
 public:
  Transaction* Begin(IsolationLevel level = IsolationLevel::kSnapshotIsolation);

  util::Status Commit(Transaction* txn);
  util::Status Abort(Transaction* txn);

  Transaction* GetTransaction(txn_id_t id);

  bool IsCommitted(txn_id_t id);
  bool IsActive(txn_id_t id);

 private:
  std::mutex mutex_;
  txn_id_t next_txn_id_ = 1;
  std::unordered_map<txn_id_t, std::unique_ptr<Transaction>> transactions_;
  std::unordered_set<txn_id_t> active_txn_ids_;
};

}  

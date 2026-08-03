#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "storage/page.hpp"
#include "transaction/transaction.hpp"
#include "utilities/status.hpp"

namespace minidb::transaction {

struct RecordIdHash {
  std::size_t operator()(const minidb::storage::RecordId& rid) const {
    return (static_cast<std::size_t>(rid.page_id) << 16) ^ static_cast<std::size_t>(rid.slot_id);
  }
};
struct RecordIdEq {
  bool operator()(const minidb::storage::RecordId& a, const minidb::storage::RecordId& b) const {
    return a.page_id == b.page_id && a.slot_id == b.slot_id;
  }
};

class LockManager {
 public:

  util::Status LockShared(txn_id_t txn_id, minidb::storage::RecordId rid);

  util::Status LockExclusive(txn_id_t txn_id, minidb::storage::RecordId rid);

  util::Status Unlock(txn_id_t txn_id, minidb::storage::RecordId rid);

  void ReleaseAll(txn_id_t txn_id);


  static bool DetectDeadlock(const std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>>& wait_for_graph,
                              std::vector<txn_id_t>* cycle_out);

 private:
  struct LockEntry {
    std::unordered_set<txn_id_t> shared_holders;
    txn_id_t exclusive_holder = kInvalidTxnId;
  };

  std::mutex mutex_;
  std::unordered_map<minidb::storage::RecordId, LockEntry, RecordIdHash, RecordIdEq> lock_table_;
  std::unordered_map<txn_id_t, std::unordered_set<minidb::storage::RecordId, RecordIdHash, RecordIdEq>> txn_locks_;
};

}

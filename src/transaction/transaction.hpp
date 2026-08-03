#pragma once

#include <cstdint>
#include <unordered_set>

namespace minidb::transaction {

using txn_id_t = std::uint64_t;
inline constexpr txn_id_t kInvalidTxnId = 0;

enum class TxnState { kActive, kCommitted, kAborted };
enum class IsolationLevel { kReadCommitted, kSnapshotIsolation };

struct Transaction {
  txn_id_t txn_id = kInvalidTxnId;
  TxnState state = TxnState::kActive;
  IsolationLevel isolation_level = IsolationLevel::kSnapshotIsolation;

  std::unordered_set<txn_id_t> active_at_start;
};

}  

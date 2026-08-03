#include "transaction/lock_manager.hpp"

#include <algorithm>
#include <functional>

namespace minidb::transaction {

using minidb::storage::RecordId;

util::Status LockManager::LockShared(txn_id_t txn_id, RecordId rid) {
  std::lock_guard<std::mutex> lock(mutex_);
  LockEntry& entry = lock_table_[rid];

  if (entry.exclusive_holder != kInvalidTxnId && entry.exclusive_holder != txn_id) {
    return util::Status::Conflict("LockShared: row is exclusively locked by another transaction");
  }
  entry.shared_holders.insert(txn_id);
  txn_locks_[txn_id].insert(rid);
  return util::Status::OK();
}

util::Status LockManager::LockExclusive(txn_id_t txn_id, RecordId rid) {
  std::lock_guard<std::mutex> lock(mutex_);
  LockEntry& entry = lock_table_[rid];

  if (entry.exclusive_holder != kInvalidTxnId && entry.exclusive_holder != txn_id) {
    return util::Status::Conflict("LockExclusive: row is exclusively locked by another transaction");
  }
  for (txn_id_t holder : entry.shared_holders) {
    if (holder != txn_id) {
      return util::Status::Conflict("LockExclusive: row is share-locked by another transaction");
    }
  }

  entry.shared_holders.clear();  
  entry.exclusive_holder = txn_id;
  txn_locks_[txn_id].insert(rid);
  return util::Status::OK();
}

util::Status LockManager::Unlock(txn_id_t txn_id, RecordId rid) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = lock_table_.find(rid);
  if (it == lock_table_.end()) return util::Status::NotFound("Unlock: no lock held on this row");

  LockEntry& entry = it->second;
  bool held = entry.shared_holders.erase(txn_id) > 0;
  if (entry.exclusive_holder == txn_id) {
    entry.exclusive_holder = kInvalidTxnId;
    held = true;
  }
  if (entry.shared_holders.empty() && entry.exclusive_holder == kInvalidTxnId) {
    lock_table_.erase(it);
  }

  auto txn_it = txn_locks_.find(txn_id);
  if (txn_it != txn_locks_.end()) txn_it->second.erase(rid);

  if (!held) return util::Status::NotFound("Unlock: this transaction does not hold a lock on this row");
  return util::Status::OK();
}

void LockManager::ReleaseAll(txn_id_t txn_id) {
  std::vector<RecordId> held;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = txn_locks_.find(txn_id);
    if (it == txn_locks_.end()) return;
    held.assign(it->second.begin(), it->second.end());
  }
  for (const RecordId& rid : held) Unlock(txn_id, rid);

  std::lock_guard<std::mutex> lock(mutex_);
  txn_locks_.erase(txn_id);
}

bool LockManager::DetectDeadlock(const std::unordered_map<txn_id_t, std::unordered_set<txn_id_t>>& wait_for_graph,
                                  std::vector<txn_id_t>* cycle_out) {
  enum class Color { kWhite, kGray, kBlack };
  std::unordered_map<txn_id_t, Color> color;
  std::vector<txn_id_t> stack;


  std::function<bool(txn_id_t)> visit = [&](txn_id_t node) -> bool {
    color[node] = Color::kGray;
    stack.push_back(node);

    auto it = wait_for_graph.find(node);
    if (it != wait_for_graph.end()) {
      for (txn_id_t neighbor : it->second) {
        Color c = color.count(neighbor) ? color[neighbor] : Color::kWhite;
        if (c == Color::kGray) {
          if (cycle_out != nullptr) {
            auto start = std::find(stack.begin(), stack.end(), neighbor);
            cycle_out->assign(start, stack.end());
          }
          return true;
        }
        if (c == Color::kWhite && visit(neighbor)) return true;
      }
    }

    stack.pop_back();
    color[node] = Color::kBlack;
    return false;
  };

  for (const auto& [node, neighbors] : wait_for_graph) {
    (void)neighbors;
    if (!color.count(node) || color[node] == Color::kWhite) {
      if (visit(node)) return true;
    }
  }
  return false;
}

}  

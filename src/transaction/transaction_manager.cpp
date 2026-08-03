#include "transaction/transaction_manager.hpp"

namespace minidb::transaction {

Transaction* TransactionManager::Begin(IsolationLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto txn = std::make_unique<Transaction>();
  txn->txn_id = next_txn_id_++;
  txn->state = TxnState::kActive;
  txn->isolation_level = level;
  txn->active_at_start = active_txn_ids_;  

  active_txn_ids_.insert(txn->txn_id);
  Transaction* raw = txn.get();
  transactions_[raw->txn_id] = std::move(txn);
  return raw;
}

util::Status TransactionManager::Commit(Transaction* txn) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (txn == nullptr || txn->state != TxnState::kActive) {
    return util::Status::InvalidArgument("Commit: transaction is not active");
  }
  txn->state = TxnState::kCommitted;
  active_txn_ids_.erase(txn->txn_id);
  return util::Status::OK();
}

util::Status TransactionManager::Abort(Transaction* txn) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (txn == nullptr || txn->state != TxnState::kActive) {
    return util::Status::InvalidArgument("Abort: transaction is not active");
  }
  txn->state = TxnState::kAborted;
  active_txn_ids_.erase(txn->txn_id);
  return util::Status::OK();
}

Transaction* TransactionManager::GetTransaction(txn_id_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = transactions_.find(id);
  if (it == transactions_.end()) return nullptr;
  return it->second.get();
}

bool TransactionManager::IsCommitted(txn_id_t id) {
  Transaction* txn = GetTransaction(id);
  return txn != nullptr && txn->state == TxnState::kCommitted;
}

bool TransactionManager::IsActive(txn_id_t id) {
  Transaction* txn = GetTransaction(id);
  return txn != nullptr && txn->state == TxnState::kActive;
}

}  

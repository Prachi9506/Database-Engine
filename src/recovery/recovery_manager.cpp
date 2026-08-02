#include "recovery/recovery_manager.hpp"

#include <cstring>
#include <unordered_set>

#include "storage/page.hpp"

namespace minidb::recovery {

using minidb::storage::Page;
using minidb::transaction::txn_id_t;

util::Result<RecoveryStats> RecoveryManager::Recover() {
  auto records_result = wal_->ReadAll();
  if (!records_result.ok()) return records_result.status();
  const std::vector<WALRecord>& records = records_result.value();

  RecoveryStats stats;

  std::unordered_set<txn_id_t> committed, aborted;
  std::size_t redo_start_index = 0;
  for (std::size_t i = 0; i < records.size(); ++i) {
    const WALRecord& r = records[i];
    if (r.type == WALRecordType::kCommit) committed.insert(r.txn_id);
    else if (r.type == WALRecordType::kAbort) aborted.insert(r.txn_id);
    else if (r.type == WALRecordType::kCheckpoint) {
      stats.checkpoint_lsn_used = r.lsn;
      redo_start_index = i + 1; 
    }
  }


  for (std::size_t i = redo_start_index; i < records.size(); ++i) {
    const WALRecord& r = records[i];
    if (r.type != WALRecordType::kUpdate) continue;

    Page after;
    std::memcpy(after.Data(), r.after_image.data(), Page::Size());
    util::Status status = page_manager_->WritePage(r.page_id, after);
    if (!status.ok()) return status;
    ++stats.redo_count;
  }

  for (auto it = records.rbegin(); it != records.rend(); ++it) {
    const WALRecord& r = *it;
    if (r.type != WALRecordType::kUpdate) continue;
    bool needs_undo = committed.count(r.txn_id) == 0;
    if (!needs_undo) continue;

    Page before;
    std::memcpy(before.Data(), r.before_image.data(), Page::Size());
    util::Status status = page_manager_->WritePage(r.page_id, before);
    if (!status.ok()) return status;
    ++stats.undo_count;
  }

  return util::Result<RecoveryStats>(stats);
}
   
}

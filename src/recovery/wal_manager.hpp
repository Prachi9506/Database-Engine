#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "recovery/wal_record.hpp"
#include "utilities/status.hpp"

namespace minidb::recovery {

class WALManager {
 public:
  explicit WALManager(const std::string& path);
  ~WALManager();

  lsn_t AppendUpdate(minidb::transaction::txn_id_t txn_id, minidb::storage::page_id_t page_id,
                      const minidb::storage::Page& before, const minidb::storage::Page& after);
  lsn_t AppendCommit(minidb::transaction::txn_id_t txn_id);
  lsn_t AppendAbort(minidb::transaction::txn_id_t txn_id);
  lsn_t AppendCheckpoint();


  void Flush();

  util::Result<std::vector<WALRecord>> ReadAll() const;

 private:
  lsn_t AppendRecord(const WALRecord& record);

  std::string path_;
  mutable std::fstream file_;
  mutable std::mutex mutex_;
  lsn_t next_lsn_ = 1;
};

}  

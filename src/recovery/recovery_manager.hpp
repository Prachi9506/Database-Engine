#pragma once

#include <cstddef>

#include "recovery/wal_manager.hpp"
#include "storage/page_manager.hpp"
#include "utilities/status.hpp"

namespace minidb::recovery {

struct RecoveryStats {
  std::size_t redo_count = 0;
  std::size_t undo_count = 0;
  lsn_t checkpoint_lsn_used = kInvalidLsn;  
};

class RecoveryManager {
 public:
  RecoveryManager(WALManager* wal, minidb::storage::PageManager* page_manager)
      : wal_(wal), page_manager_(page_manager) {}

  util::Result<RecoveryStats> Recover();

 private:
  WALManager* wal_;
  minidb::storage::PageManager* page_manager_;
};

}  

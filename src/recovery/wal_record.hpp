#pragma once

#include <cstdint>
#include <string>

#include "storage/page.hpp"
#include "transaction/transaction.hpp"

namespace minidb::recovery {

using lsn_t = std::uint64_t;
inline constexpr lsn_t kInvalidLsn = 0;

enum class WALRecordType : std::uint8_t {
  kUpdate = 1,
  kCommit = 2,
  kAbort = 3,
  kCheckpoint = 4,
};

struct WALRecord {
  lsn_t lsn = kInvalidLsn;
  WALRecordType type = WALRecordType::kUpdate;
  minidb::transaction::txn_id_t txn_id = minidb::transaction::kInvalidTxnId;

  minidb::storage::page_id_t page_id = minidb::storage::kInvalidPageId;
  std::string before_image;  
  std::string after_image;   
};

}  

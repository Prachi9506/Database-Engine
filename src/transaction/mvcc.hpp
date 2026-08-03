#pragma once

#include <functional>
#include <vector>

#include "common/schema.hpp"
#include "common/value.hpp"
#include "storage/heap_file.hpp"
#include "transaction/transaction.hpp"
#include "transaction/transaction_manager.hpp"
#include "utilities/status.hpp"

namespace minidb::transaction {

struct VersionMeta {
  txn_id_t xmin = kInvalidTxnId;
  txn_id_t xmax = kInvalidTxnId;  
};

bool IsVisible(const VersionMeta& meta, const Transaction& reader, TransactionManager* txn_mgr);

class VersionedRecordStore {
 public:
  VersionedRecordStore(minidb::storage::HeapFile* heap_file, const minidb::common::Schema* schema)
      : heap_file_(heap_file), schema_(schema) {}


  util::Result<minidb::storage::RecordId> InsertVersion(txn_id_t txn_id,
                                                         const std::vector<minidb::common::Value>& values);

  util::Status DeleteVersion(txn_id_t txn_id, minidb::storage::RecordId rid);


  util::Result<minidb::storage::RecordId> UpdateVersion(txn_id_t txn_id, minidb::storage::RecordId old_rid,
                                                         const std::vector<minidb::common::Value>& new_values);

 
  void ScanVisible(const Transaction& reader, TransactionManager* txn_mgr,
                    const std::function<void(minidb::storage::RecordId, const std::vector<minidb::common::Value>&)>&
                        visitor);

 private:
  minidb::storage::HeapFile* heap_file_;
  const minidb::common::Schema* schema_;
};

}  

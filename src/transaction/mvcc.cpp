#include "transaction/mvcc.hpp"

#include "storage/record.hpp"
#include "utilities/bytes.hpp"

namespace minidb::transaction {

namespace {
constexpr std::size_t kVersionHeaderSize = sizeof(txn_id_t) * 2;

std::string EncodeVersioned(VersionMeta meta, const std::string& encoded_row) {
  std::string out(kVersionHeaderSize, '\0');
  util::WriteFixed<txn_id_t>(out.data(), meta.xmin);
  util::WriteFixed<txn_id_t>(out.data() + sizeof(txn_id_t), meta.xmax);
  out.append(encoded_row);
  return out;
}

VersionMeta DecodeVersionMeta(const std::string& bytes) {
  VersionMeta meta;
  meta.xmin = util::ReadFixed<txn_id_t>(bytes.data());
  meta.xmax = util::ReadFixed<txn_id_t>(bytes.data() + sizeof(txn_id_t));
  return meta;
}

std::string RowPayload(const std::string& bytes) { return bytes.substr(kVersionHeaderSize); }

bool CommittedAndVisibleTo(txn_id_t id, const Transaction& reader, TransactionManager* txn_mgr) {
  if (id == kInvalidTxnId) return false;
  Transaction* actor = txn_mgr->GetTransaction(id);
  if (actor == nullptr || actor->state != TxnState::kCommitted) return false;

  if (reader.isolation_level == IsolationLevel::kReadCommitted) return true;


  return id < reader.txn_id && reader.active_at_start.count(id) == 0;
}
}  

bool IsVisible(const VersionMeta& meta, const Transaction& reader, TransactionManager* txn_mgr) {
  bool created_by_self = (meta.xmin == reader.txn_id);
  bool deleted_by_self = (meta.xmax == reader.txn_id);

  if (deleted_by_self) return false;  

  bool creator_visible = created_by_self || CommittedAndVisibleTo(meta.xmin, reader, txn_mgr);
  if (!creator_visible) return false;

  if (meta.xmax == kInvalidTxnId) return true;  


  bool deleter_visible = CommittedAndVisibleTo(meta.xmax, reader, txn_mgr);
  return !deleter_visible;
}

util::Result<minidb::storage::RecordId> VersionedRecordStore::InsertVersion(
    txn_id_t txn_id, const std::vector<minidb::common::Value>& values) {
  auto encoded = minidb::storage::RecordCodec::Encode(*schema_, values);
  if (!encoded.ok()) return encoded.status();

  VersionMeta meta{txn_id, kInvalidTxnId};
  std::string versioned = EncodeVersioned(meta, encoded.value());
  if (versioned.size() > 0xFFFF) {
    return util::Status::InvalidArgument("InsertVersion: encoded row exceeds codec limit");
  }
  return heap_file_->InsertRecord(versioned.data(), static_cast<std::uint16_t>(versioned.size()));
}

util::Status VersionedRecordStore::DeleteVersion(txn_id_t txn_id, minidb::storage::RecordId rid) {
  auto bytes = heap_file_->GetRecordBytes(rid);
  if (!bytes.ok()) return bytes.status();

  VersionMeta meta = DecodeVersionMeta(bytes.value());
  meta.xmax = txn_id;
  std::string payload = RowPayload(bytes.value());
  std::string versioned = EncodeVersioned(meta, payload);

  return heap_file_->UpdateRecord(rid, versioned.data(), static_cast<std::uint16_t>(versioned.size())).status();
}

util::Result<minidb::storage::RecordId> VersionedRecordStore::UpdateVersion(
    txn_id_t txn_id, minidb::storage::RecordId old_rid, const std::vector<minidb::common::Value>& new_values) {
  util::Status del_status = DeleteVersion(txn_id, old_rid);
  if (!del_status.ok()) return del_status;
  return InsertVersion(txn_id, new_values);
}

void VersionedRecordStore::ScanVisible(
    const Transaction& reader, TransactionManager* txn_mgr,
    const std::function<void(minidb::storage::RecordId, const std::vector<minidb::common::Value>&)>& visitor) {
  heap_file_->Scan([&](minidb::storage::RecordId rid, const char* data, std::uint16_t len) {
    std::string bytes(data, len);
    VersionMeta meta = DecodeVersionMeta(bytes);
    if (!IsVisible(meta, reader, txn_mgr)) return;

    auto decoded = minidb::storage::RecordCodec::Decode(*schema_, RowPayload(bytes));
    if (!decoded.ok()) return;  
    visitor(rid, decoded.value());
  });
}

}  

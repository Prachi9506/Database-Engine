#include "recovery/wal_manager.hpp"

#include "utilities/bytes.hpp"
#include "utilities/config.hpp"

namespace minidb::recovery {

using minidb::storage::Page;
using minidb::storage::page_id_t;
using minidb::transaction::txn_id_t;

WALManager::WALManager(const std::string& path) : path_(path) {
  file_.open(path_, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
  if (!file_.is_open()) {
    std::ofstream create(path_, std::ios::binary);
    create.close();
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
  }
  auto existing = ReadAll();
  if (existing.ok() && !existing.value().empty()) {
    next_lsn_ = existing.value().back().lsn + 1;
  }
  file_.seekp(0, std::ios::end);
}

WALManager::~WALManager() {
  if (file_.is_open()) {
    file_.flush();
    file_.close();
  }
}

lsn_t WALManager::AppendRecord(const WALRecord& record) {
  std::lock_guard<std::mutex> lock(mutex_);

  WALRecord r = record;
  r.lsn = next_lsn_++;

  std::string buf;
  char fixed[8 + 1 + 8 + 4];
  util::WriteFixed<lsn_t>(fixed + 0, r.lsn);
  fixed[8] = static_cast<char>(r.type);
  util::WriteFixed<txn_id_t>(fixed + 9, r.txn_id);
  util::WriteFixed<page_id_t>(fixed + 17, r.page_id);
  buf.append(fixed, sizeof(fixed));

  if (r.type == WALRecordType::kUpdate) {
    buf.append(r.before_image);
    buf.append(r.after_image);
  }

  char len_buf[4];
  util::WriteFixed<std::uint32_t>(len_buf, static_cast<std::uint32_t>(buf.size()));

  file_.seekp(0, std::ios::end);
  file_.write(len_buf, sizeof(len_buf));
  file_.write(buf.data(), static_cast<std::streamsize>(buf.size()));
  file_.flush();  

  return r.lsn;
}

lsn_t WALManager::AppendUpdate(txn_id_t txn_id, page_id_t page_id, const Page& before, const Page& after) {
  WALRecord r;
  r.type = WALRecordType::kUpdate;
  r.txn_id = txn_id;
  r.page_id = page_id;
  r.before_image.assign(before.Data(), Page::Size());
  r.after_image.assign(after.Data(), Page::Size());
  return AppendRecord(r);
}

lsn_t WALManager::AppendCommit(txn_id_t txn_id) {
  WALRecord r;
  r.type = WALRecordType::kCommit;
  r.txn_id = txn_id;
  return AppendRecord(r);
}

lsn_t WALManager::AppendAbort(txn_id_t txn_id) {
  WALRecord r;
  r.type = WALRecordType::kAbort;
  r.txn_id = txn_id;
  return AppendRecord(r);
}

lsn_t WALManager::AppendCheckpoint() {
  WALRecord r;
  r.type = WALRecordType::kCheckpoint;
  return AppendRecord(r);
}

void WALManager::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  file_.flush();
}

util::Result<std::vector<WALRecord>> WALManager::ReadAll() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<WALRecord> records;

  std::ifstream in(path_, std::ios::binary);
  if (!in.is_open()) return util::Result<std::vector<WALRecord>>(records);  

  while (true) {
    char len_buf[4];
    in.read(len_buf, sizeof(len_buf));
    if (in.gcount() < static_cast<std::streamsize>(sizeof(len_buf))) break;  
    std::uint32_t len = util::ReadFixed<std::uint32_t>(len_buf);

    std::string buf(len, '\0');
    in.read(buf.data(), static_cast<std::streamsize>(len));
    if (in.gcount() < static_cast<std::streamsize>(len)) {
      return util::Status::Corruption("WAL: truncated record (possible mid-write crash)");
    }

    WALRecord r;
    r.lsn = util::ReadFixed<lsn_t>(buf.data() + 0);
    r.type = static_cast<WALRecordType>(buf[8]);
    r.txn_id = util::ReadFixed<txn_id_t>(buf.data() + 9);
    r.page_id = util::ReadFixed<page_id_t>(buf.data() + 17);

    if (r.type == WALRecordType::kUpdate) {
      constexpr std::size_t kFixedSize = 8 + 1 + 8 + 4;
      if (buf.size() < kFixedSize + 2 * minidb::config::kPageSize) {
        return util::Status::Corruption("WAL: truncated update record images");
      }
      r.before_image = buf.substr(kFixedSize, minidb::config::kPageSize);
      r.after_image = buf.substr(kFixedSize + minidb::config::kPageSize, minidb::config::kPageSize);
    }

    records.push_back(std::move(r));
  }

  return util::Result<std::vector<WALRecord>>(std::move(records));
}

}  

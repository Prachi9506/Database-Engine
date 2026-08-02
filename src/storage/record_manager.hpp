#pragma once

#include <functional>
#include <vector>

#include "common/schema.hpp"
#include "common/value.hpp"
#include "storage/heap_file.hpp"
#include "storage/record.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

class RecordManager {
 public:
  RecordManager(HeapFile* heap_file, const minidb::common::Schema* schema)
      : heap_file_(heap_file), schema_(schema) {}

  util::Result<RecordId> InsertRow(const std::vector<minidb::common::Value>& values) {
    auto encoded = RecordCodec::Encode(*schema_, values);
    if (!encoded.ok()) return encoded.status();
    const std::string& bytes = encoded.value();
    if (bytes.size() > 0xFFFF) {
      return util::Status::InvalidArgument("InsertRow: encoded row exceeds 65535-byte codec limit");
    }
    return heap_file_->InsertRecord(bytes.data(), static_cast<std::uint16_t>(bytes.size()));
  }

  util::Result<std::vector<minidb::common::Value>> GetRow(RecordId rid) const {
    auto bytes = heap_file_->GetRecordBytes(rid);
    if (!bytes.ok()) return bytes.status();
    return RecordCodec::Decode(*schema_, bytes.value());
  }

  util::Result<RecordId> UpdateRow(RecordId rid, const std::vector<minidb::common::Value>& values) {
    auto encoded = RecordCodec::Encode(*schema_, values);
    if (!encoded.ok()) return encoded.status();
    const std::string& bytes = encoded.value();
    if (bytes.size() > 0xFFFF) {
      return util::Status::InvalidArgument("UpdateRow: encoded row exceeds 65535-byte codec limit");
    }
    return heap_file_->UpdateRecord(rid, bytes.data(), static_cast<std::uint16_t>(bytes.size()));
  }

  util::Status DeleteRow(RecordId rid) { return heap_file_->DeleteRecord(rid); }

  void Scan(const std::function<void(RecordId, const std::vector<minidb::common::Value>&)>& visitor) const {
    heap_file_->Scan([&](RecordId rid, const char* data, std::uint16_t len) {
      auto decoded = RecordCodec::Decode(*schema_, std::string(data, len));
      if (decoded.ok()) visitor(rid, decoded.value());
      
    });
  }

 private:
  HeapFile* heap_file_;
  const minidb::common::Schema* schema_;
};

}  

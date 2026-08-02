#pragma once

#include <functional>
#include <string>

#include "buffer/buffer_pool.hpp"
#include "storage/page.hpp"
#include "storage/slotted_page.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

class HeapFile {
 public:

  HeapFile(minidb::buffer::BufferPool* buffer_pool, page_id_t first_page_id);

  page_id_t FirstPageId() const { return first_page_id_; }

  util::Result<RecordId> InsertRecord(const char* data, std::uint16_t length);
  util::Result<std::string> GetRecordBytes(RecordId rid) const;
  util::Status DeleteRecord(RecordId rid);

  util::Result<RecordId> UpdateRecord(RecordId rid, const char* data, std::uint16_t length);

  void Scan(const std::function<void(RecordId, const char*, std::uint16_t)>& visitor) const;

 private:
  page_id_t AllocateNewPage(page_id_t link_from_page_id);

  minidb::buffer::BufferPool* buffer_pool_;
  page_id_t first_page_id_;
  mutable page_id_t last_insert_page_id_;  
};

}  

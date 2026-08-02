#include "storage/heap_file.hpp"

namespace minidb::storage {

HeapFile::HeapFile(minidb::buffer::BufferPool* buffer_pool, page_id_t first_page_id)
    : buffer_pool_(buffer_pool), first_page_id_(first_page_id), last_insert_page_id_(first_page_id) {
  if (first_page_id_ == kInvalidPageId) {
    page_id_t new_page_id;
    Page* page = buffer_pool_->NewPage(&new_page_id);
    SlottedPage sp(page);
    sp.Init(new_page_id, kInvalidPageId);
    buffer_pool_->UnpinPage(new_page_id, /*is_dirty=*/true);
    first_page_id_ = new_page_id;
    last_insert_page_id_ = new_page_id;
  }
}

page_id_t HeapFile::AllocateNewPage(page_id_t link_from_page_id) {
  page_id_t new_page_id;
  Page* new_page = buffer_pool_->NewPage(&new_page_id);
  SlottedPage new_sp(new_page);
  new_sp.Init(new_page_id, kInvalidPageId);
  buffer_pool_->UnpinPage(new_page_id, true);

  Page* link_page = buffer_pool_->FetchPage(link_from_page_id);
  SlottedPage link_sp(link_page);
  link_sp.SetNextPageId(new_page_id);
  buffer_pool_->UnpinPage(link_from_page_id, true);

  return new_page_id;
}

util::Result<RecordId> HeapFile::InsertRecord(const char* data, std::uint16_t length) {
  page_id_t current = last_insert_page_id_;
  page_id_t prev = kInvalidPageId;

  while (current != kInvalidPageId) {
    Page* page = buffer_pool_->FetchPage(current);
    if (page == nullptr) {
      return util::Status::IOError("InsertRecord: failed to fetch page");
    }
    SlottedPage sp(page);
    auto slot = sp.InsertRecord(data, length);
    if (slot.has_value()) {
      RecordId rid{current, *slot};
      buffer_pool_->UnpinPage(current, /*is_dirty=*/true);
      last_insert_page_id_ = current;
      return util::Result<RecordId>(rid);
    }
    page_id_t next = sp.NextPageId();
    buffer_pool_->UnpinPage(current, /*is_dirty=*/false);
    prev = current;
    current = next;
  }


  page_id_t link_source = (prev != kInvalidPageId) ? prev : last_insert_page_id_;
  page_id_t new_page_id = AllocateNewPage(link_source);

  Page* new_page = buffer_pool_->FetchPage(new_page_id);
  SlottedPage new_sp(new_page);
  auto slot = new_sp.InsertRecord(data, length);
  buffer_pool_->UnpinPage(new_page_id, true);

  if (!slot.has_value()) {
    return util::Status::OutOfSpace("InsertRecord: record too large for an empty page");
  }
  last_insert_page_id_ = new_page_id;
  return util::Result<RecordId>(RecordId{new_page_id, *slot});
}

util::Result<std::string> HeapFile::GetRecordBytes(RecordId rid) const {
  Page* page = buffer_pool_->FetchPage(rid.page_id);
  if (page == nullptr) {
    return util::Status::NotFound("GetRecordBytes: page not found");
  }
  SlottedPage sp(page);
  auto rec = sp.GetRecord(rid.slot_id);
  if (!rec.has_value()) {
    buffer_pool_->UnpinPage(rid.page_id, false);
    return util::Status::NotFound("GetRecordBytes: record deleted or slot invalid");
  }
  std::string bytes(rec->first, rec->second);
  buffer_pool_->UnpinPage(rid.page_id, false);
  return util::Result<std::string>(std::move(bytes));
}

util::Status HeapFile::DeleteRecord(RecordId rid) {
  Page* page = buffer_pool_->FetchPage(rid.page_id);
  if (page == nullptr) {
    return util::Status::NotFound("DeleteRecord: page not found");
  }
  SlottedPage sp(page);
  bool ok = sp.DeleteRecord(rid.slot_id);
  buffer_pool_->UnpinPage(rid.page_id, ok);
  if (!ok) return util::Status::NotFound("DeleteRecord: record already deleted or invalid slot");
  return util::Status::OK();
}

util::Result<RecordId> HeapFile::UpdateRecord(RecordId rid, const char* data, std::uint16_t length) {
  Page* page = buffer_pool_->FetchPage(rid.page_id);
  if (page == nullptr) {
    return util::Status::NotFound("UpdateRecord: page not found");
  }
  SlottedPage sp(page);
  bool in_place = sp.UpdateRecordInPlace(rid.slot_id, data, length);
  buffer_pool_->UnpinPage(rid.page_id, in_place);
  if (in_place) {
    return util::Result<RecordId>(rid);
  }


  util::Status del_status = DeleteRecord(rid);
  if (!del_status.ok()) return del_status;
  return InsertRecord(data, length);
}

void HeapFile::Scan(const std::function<void(RecordId, const char*, std::uint16_t)>& visitor) const {
  page_id_t current = first_page_id_;
  while (current != kInvalidPageId) {
    Page* page = buffer_pool_->FetchPage(current);
    if (page == nullptr) break;
    SlottedPage sp(page);
    std::uint16_t slot_count = sp.SlotCount();
    for (slot_id_t s = 0; s < slot_count; ++s) {
      auto rec = sp.GetRecord(s);
      if (rec.has_value()) {
        visitor(RecordId{current, s}, rec->first, rec->second);
      }
    }
    page_id_t next = sp.NextPageId();
    buffer_pool_->UnpinPage(current, false);
    current = next;
  }
}

}  

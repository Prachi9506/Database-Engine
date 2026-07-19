#include "buffer/buffer_pool.hpp"

namespace minidb::buffer {

BufferPool::BufferPool(minidb::storage::PageManager* page_manager, std::size_t pool_size)
    : page_manager_(page_manager), frames_(pool_size), replacer_(pool_size) {
  free_list_.reserve(pool_size);
  for (std::size_t i = 0; i < pool_size; ++i) {
    free_list_.push_back(static_cast<frame_id_t>(i));
  }
}

bool BufferPool::FindVictimFrame(frame_id_t* out_frame_id) {
  if (!free_list_.empty()) {
    *out_frame_id = free_list_.back();
    free_list_.pop_back();
    return true;
  }
  return replacer_.Victim(out_frame_id);
}

minidb::storage::Page* BufferPool::FetchPage(minidb::storage::page_id_t page_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    Frame& frame = frames_[it->second];
    if (frame.pin_count == 0) replacer_.Pin(it->second);
    frame.pin_count += 1;
    ++hit_count_;
    return &frame.page;
  }
  ++miss_count_;

  frame_id_t frame_id;
  if (!FindVictimFrame(&frame_id)) {
    return nullptr;  
  }

  Frame& frame = frames_[frame_id];
  if (frame.page_id != minidb::storage::kInvalidPageId) {
    if (frame.is_dirty) {
      page_manager_->WritePage(frame.page_id, frame.page);
    }
    page_table_.erase(frame.page_id);
  }

  frame.Reset();
  util::Status status = page_manager_->ReadPage(page_id, &frame.page);
  if (!status.ok()) {
    free_list_.push_back(frame_id);
    return nullptr;
  }

  frame.page_id = page_id;
  frame.pin_count = 1;
  frame.is_dirty = false;
  page_table_[page_id] = frame_id;
  return &frame.page;
}

minidb::storage::Page* BufferPool::NewPage(minidb::storage::page_id_t* out_page_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  util::Result<minidb::storage::page_id_t> alloc = page_manager_->AllocatePage();
  if (!alloc.ok()) return nullptr;
  minidb::storage::page_id_t page_id = alloc.value();

  frame_id_t frame_id;
  if (!FindVictimFrame(&frame_id)) {
    return nullptr;
  }

  Frame& frame = frames_[frame_id];
  if (frame.page_id != minidb::storage::kInvalidPageId) {
    if (frame.is_dirty) {
      page_manager_->WritePage(frame.page_id, frame.page);
    }
    page_table_.erase(frame.page_id);
  }

  frame.Reset();
  frame.page_id = page_id;
  frame.pin_count = 1;
  frame.is_dirty = true;  
  page_table_[page_id] = frame_id;

  *out_page_id = page_id;
  return &frame.page;
}

bool BufferPool::UnpinPage(minidb::storage::page_id_t page_id, bool is_dirty) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) return false;

  Frame& frame = frames_[it->second];
  if (frame.pin_count <= 0) return false;

  frame.is_dirty = frame.is_dirty || is_dirty;
  frame.pin_count -= 1;
  if (frame.pin_count == 0) {
    replacer_.Unpin(it->second);
  }
  return true;
}

util::Status BufferPool::FlushPage(minidb::storage::page_id_t page_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return util::Status::NotFound("FlushPage: page not resident");
  }
  Frame& frame = frames_[it->second];
  util::Status status = page_manager_->WritePage(page_id, frame.page);
  if (status.ok()) frame.is_dirty = false;
  return status;
}

util::Status BufferPool::FlushAll() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [page_id, frame_id] : page_table_) {
    Frame& frame = frames_[frame_id];
    if (frame.is_dirty) {
      util::Status status = page_manager_->WritePage(page_id, frame.page);
      if (!status.ok()) return status;
      frame.is_dirty = false;
    }
  }
  return util::Status::OK();
}

}  

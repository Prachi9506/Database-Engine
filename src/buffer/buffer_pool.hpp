#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include "buffer/frame.hpp"
#include "buffer/lru_replacer.hpp"
#include "storage/page_manager.hpp"
#include "utilities/config.hpp"
#include "utilities/status.hpp"

namespace minidb::buffer {

class BufferPool {
 public:
  BufferPool(minidb::storage::PageManager* page_manager,
             std::size_t pool_size = minidb::config::kDefaultBufferPoolSize);

  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  minidb::storage::Page* FetchPage(minidb::storage::page_id_t page_id);


  minidb::storage::Page* NewPage(minidb::storage::page_id_t* out_page_id);


  bool UnpinPage(minidb::storage::page_id_t page_id, bool is_dirty);


  util::Status FlushPage(minidb::storage::page_id_t page_id);

  util::Status FlushAll();

  std::size_t PoolSize() const { return frames_.size(); }


  std::size_t HitCount() const { return hit_count_; }
  std::size_t MissCount() const { return miss_count_; }
  double HitRate() const {
    std::size_t total = hit_count_ + miss_count_;
    return total > 0 ? static_cast<double>(hit_count_) / static_cast<double>(total) : 0.0;
  }
  void ResetStats() { hit_count_ = miss_count_ = 0; }

 private:
  bool FindVictimFrame(frame_id_t* out_frame_id);

  minidb::storage::PageManager* page_manager_;
  std::vector<Frame> frames_;
  std::unordered_map<minidb::storage::page_id_t, frame_id_t> page_table_;
  std::vector<frame_id_t> free_list_;
  LRUReplacer replacer_;
  mutable std::mutex mutex_;
  std::size_t hit_count_ = 0;
  std::size_t miss_count_ = 0;
};

}  

#pragma once

#include <list>
#include <mutex>
#include <unordered_map>

#include "buffer/frame.hpp"

namespace minidb::buffer {

class LRUReplacer {
 public:
  explicit LRUReplacer(std::size_t capacity) : capacity_(capacity) {}

  void Unpin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (position_.count(frame_id)) return;  
    lru_list_.push_front(frame_id);
    position_[frame_id] = lru_list_.begin();
  }

  void Pin(frame_id_t frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = position_.find(frame_id);
    if (it == position_.end()) return;
    lru_list_.erase(it->second);
    position_.erase(it);
  }

  bool Victim(frame_id_t* out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lru_list_.empty()) return false;
    *out = lru_list_.back();
    position_.erase(lru_list_.back());
    lru_list_.pop_back();
    return true;
  }

  std::size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_list_.size();
  }

 private:
  std::size_t capacity_;
  std::list<frame_id_t> lru_list_;  
  std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> position_;
  mutable std::mutex mutex_;
};

}  

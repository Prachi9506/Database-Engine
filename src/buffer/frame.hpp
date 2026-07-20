#pragma once

#include "storage/page.hpp"

namespace minidb::buffer {

using frame_id_t = std::int32_t;

struct Frame {
  minidb::storage::Page page;
  minidb::storage::page_id_t page_id = minidb::storage::kInvalidPageId;
  int pin_count = 0;
  bool is_dirty = false;

  void Reset() {
    page.Reset();
    page_id = minidb::storage::kInvalidPageId;
    pin_count = 0;
    is_dirty = false;
  }
};

}  

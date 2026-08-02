#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include "utilities/config.hpp"

namespace minidb::storage {

using page_id_t = std::int32_t;
using slot_id_t = std::uint16_t;

inline constexpr page_id_t kInvalidPageId = -1;

class Page {
 public:
  Page() { data_.fill(0); }

  char* Data() { return data_.data(); }
  const char* Data() const { return data_.data(); }

  static constexpr std::size_t Size() { return minidb::config::kPageSize; }

  void Reset() { data_.fill(0); }

 private:
  std::array<char, minidb::config::kPageSize> data_{};
};

struct RecordId {
  page_id_t page_id = kInvalidPageId;
  slot_id_t slot_id = 0;

  bool operator==(const RecordId& other) const {
    return page_id == other.page_id && slot_id == other.slot_id;
  }
  bool IsValid() const { return page_id != kInvalidPageId; }
};

}  

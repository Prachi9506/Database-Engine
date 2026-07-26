#pragma once

#include <cstdint>
#include <cstring>

#include "storage/page.hpp"
#include "utilities/bytes.hpp"
#include "utilities/config.hpp"

namespace minidb::index {

using Key = std::int64_t;

inline constexpr std::size_t kNodeHeaderSize = 1 + 2 + 4;  
inline constexpr std::size_t kKeySize = sizeof(Key);
inline constexpr std::size_t kRidSize = sizeof(minidb::storage::page_id_t) + sizeof(minidb::storage::slot_id_t);
inline constexpr std::size_t kLeafEntrySize = kKeySize + kRidSize;
inline constexpr std::size_t kChildSize = sizeof(minidb::storage::page_id_t);

inline constexpr std::size_t kLeafMaxKeys = (minidb::config::kPageSize - kNodeHeaderSize) / kLeafEntrySize;

inline constexpr std::size_t kInternalMaxKeys =
    (minidb::config::kPageSize - kNodeHeaderSize - kChildSize) / (kKeySize + kChildSize);

class BPlusNodeView {
 public:
  explicit BPlusNodeView(minidb::storage::Page* page) : page_(page) {}

  bool IsLeaf() const { return static_cast<unsigned char>(page_->Data()[0]) != 0; }

  void InitLeaf(minidb::storage::page_id_t next_leaf_id) {
    page_->Data()[0] = 1;
    SetKeyCount(0);
    SetNextLeafId(next_leaf_id);
  }
  void InitInternal() {
    page_->Data()[0] = 0;
    SetKeyCount(0);
    SetNextLeafId(minidb::storage::kInvalidPageId);
  }

  std::uint16_t KeyCount() const { return util::ReadFixed<std::uint16_t>(page_->Data() + 1); }
  void SetKeyCount(std::uint16_t n) { util::WriteFixed<std::uint16_t>(page_->Data() + 1, n); }

  minidb::storage::page_id_t NextLeafId() const {
    return util::ReadFixed<minidb::storage::page_id_t>(page_->Data() + 3);
  }
  void SetNextLeafId(minidb::storage::page_id_t id) {
    util::WriteFixed<minidb::storage::page_id_t>(page_->Data() + 3, id);
  }


  Key LeafKeyAt(std::size_t i) const { return util::ReadFixed<Key>(LeafEntryPtr(i)); }
  minidb::storage::RecordId LeafRidAt(std::size_t i) const {
    const char* p = LeafEntryPtr(i) + kKeySize;
    minidb::storage::RecordId rid;
    rid.page_id = util::ReadFixed<minidb::storage::page_id_t>(p);
    rid.slot_id = util::ReadFixed<minidb::storage::slot_id_t>(p + sizeof(minidb::storage::page_id_t));
    return rid;
  }
  void SetLeafEntry(std::size_t i, Key key, minidb::storage::RecordId rid) {
    char* p = LeafEntryPtr(i);
    util::WriteFixed<Key>(p, key);
    util::WriteFixed<minidb::storage::page_id_t>(p + kKeySize, rid.page_id);
    util::WriteFixed<minidb::storage::slot_id_t>(p + kKeySize + sizeof(minidb::storage::page_id_t), rid.slot_id);
  }


  void InsertLeafEntryAt(std::size_t i, Key key, minidb::storage::RecordId rid) {
    std::uint16_t n = KeyCount();
    for (std::size_t j = n; j > i; --j) {
      SetLeafEntry(j, LeafKeyAt(j - 1), LeafRidAt(j - 1));
    }
    SetLeafEntry(i, key, rid);
    SetKeyCount(static_cast<std::uint16_t>(n + 1));
  }

  void RemoveLeafEntryAt(std::size_t i) {
    std::uint16_t n = KeyCount();
    for (std::size_t j = i; j + 1 < n; ++j) {
      SetLeafEntry(j, LeafKeyAt(j + 1), LeafRidAt(j + 1));
    }
    SetKeyCount(static_cast<std::uint16_t>(n - 1));
  }


  Key InternalKeyAt(std::size_t i) const { return util::ReadFixed<Key>(InternalKeyPtr(i)); }
  void SetInternalKeyAt(std::size_t i, Key key) { util::WriteFixed<Key>(InternalKeyPtr(i), key); }

  minidb::storage::page_id_t InternalChildAt(std::size_t i) const {
    return util::ReadFixed<minidb::storage::page_id_t>(InternalChildPtr(i));
  }
  void SetInternalChildAt(std::size_t i, minidb::storage::page_id_t child) {
    util::WriteFixed<minidb::storage::page_id_t>(InternalChildPtr(i), child);
  }

  void InsertInternalKeyChildAt(std::size_t key_i, Key key, minidb::storage::page_id_t right_child) {
    std::uint16_t n = KeyCount();
    for (std::size_t j = n; j > key_i; --j) SetInternalKeyAt(j, InternalKeyAt(j - 1));
    SetInternalKeyAt(key_i, key);
    for (std::size_t j = n + 1; j > key_i + 1; --j) SetInternalChildAt(j, InternalChildAt(j - 1));
    SetInternalChildAt(key_i + 1, right_child);
    SetKeyCount(static_cast<std::uint16_t>(n + 1));
  }

 private:
  char* LeafEntryPtr(std::size_t i) { return page_->Data() + kNodeHeaderSize + i * kLeafEntrySize; }
  const char* LeafEntryPtr(std::size_t i) const { return page_->Data() + kNodeHeaderSize + i * kLeafEntrySize; }

  static constexpr std::size_t InternalKeysOffset() { return kNodeHeaderSize; }
  static constexpr std::size_t InternalChildrenOffset() { return kNodeHeaderSize + kInternalMaxKeys * kKeySize; }
  char* InternalKeyPtr(std::size_t i) { return page_->Data() + InternalKeysOffset() + i * kKeySize; }
  const char* InternalKeyPtr(std::size_t i) const { return page_->Data() + InternalKeysOffset() + i * kKeySize; }
  char* InternalChildPtr(std::size_t i) { return page_->Data() + InternalChildrenOffset() + i * kChildSize; }
  const char* InternalChildPtr(std::size_t i) const {
    return page_->Data() + InternalChildrenOffset() + i * kChildSize;
  }

  minidb::storage::Page* page_;
};

}  

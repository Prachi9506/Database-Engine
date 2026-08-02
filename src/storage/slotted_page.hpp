#pragma once

#include <cstring>
#include <optional>
#include <utility>

#include "storage/page.hpp"
#include "utilities/bytes.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

struct PageHeader {
  page_id_t page_id = kInvalidPageId;
  page_id_t next_page_id = kInvalidPageId;  
  std::uint16_t slot_count = 0;
  std::uint16_t free_space_offset = 0;  
};

struct Slot {
  std::uint16_t offset = 0;
  std::uint16_t length = 0;  
};

class SlottedPage {
 public:
  static constexpr std::size_t kHeaderSize = sizeof(page_id_t) * 2 + sizeof(std::uint16_t) * 2;
  static constexpr std::size_t kSlotSize = sizeof(std::uint16_t) * 2;

  explicit SlottedPage(Page* page) : page_(page) {}

  void Init(page_id_t page_id, page_id_t next_page_id = kInvalidPageId) {
    PageHeader header;
    header.page_id = page_id;
    header.next_page_id = next_page_id;
    header.slot_count = 0;
    header.free_space_offset = static_cast<std::uint16_t>(Page::Size());
    WriteHeader(header);
  }

  PageHeader ReadHeader() const {
    PageHeader h;
    const char* d = page_->Data();
    h.page_id = util::ReadFixed<page_id_t>(d + 0);
    h.next_page_id = util::ReadFixed<page_id_t>(d + 4);
    h.slot_count = util::ReadFixed<std::uint16_t>(d + 8);
    h.free_space_offset = util::ReadFixed<std::uint16_t>(d + 10);
    return h;
  }

  void WriteHeader(const PageHeader& h) {
    char* d = page_->Data();
    util::WriteFixed<page_id_t>(d + 0, h.page_id);
    util::WriteFixed<page_id_t>(d + 4, h.next_page_id);
    util::WriteFixed<std::uint16_t>(d + 8, h.slot_count);
    util::WriteFixed<std::uint16_t>(d + 10, h.free_space_offset);
  }

  page_id_t PageId() const { return ReadHeader().page_id; }
  page_id_t NextPageId() const { return ReadHeader().next_page_id; }
  void SetNextPageId(page_id_t next) {
    PageHeader h = ReadHeader();
    h.next_page_id = next;
    WriteHeader(h);
  }

  std::uint16_t SlotCount() const { return ReadHeader().slot_count; }

  std::size_t FreeSpace() const {
    PageHeader h = ReadHeader();
    std::size_t directory_end = kHeaderSize + static_cast<std::size_t>(h.slot_count) * kSlotSize;
    if (directory_end > h.free_space_offset) return 0;
    return h.free_space_offset - directory_end;
  }

  std::optional<slot_id_t> InsertRecord(const char* data, std::uint16_t length) {
    PageHeader h = ReadHeader();

    std::optional<slot_id_t> reuse_slot;
    for (std::uint16_t i = 0; i < h.slot_count; ++i) {
      Slot s = ReadSlot(i);
      if (s.length == 0) {
        reuse_slot = i;
        break;
      }
    }

    std::size_t needed_directory_growth = reuse_slot.has_value() ? 0 : kSlotSize;
    std::size_t directory_end = kHeaderSize + static_cast<std::size_t>(h.slot_count) * kSlotSize;
    std::size_t needed_before_record = directory_end + needed_directory_growth;

    if (needed_before_record > h.free_space_offset) return std::nullopt;
    std::size_t available = h.free_space_offset - needed_before_record;
    if (available < length) return std::nullopt;

    std::uint16_t new_offset = static_cast<std::uint16_t>(h.free_space_offset - length);
    std::memcpy(page_->Data() + new_offset, data, length);

    Slot slot{new_offset, length};
    slot_id_t slot_id;
    if (reuse_slot.has_value()) {
      slot_id = *reuse_slot;
      WriteSlot(slot_id, slot);
    } else {
      slot_id = h.slot_count;
      WriteSlot(slot_id, slot);
      h.slot_count += 1;
    }
    h.free_space_offset = new_offset;
    WriteHeader(h);
    return slot_id;
  }


  std::optional<std::pair<const char*, std::uint16_t>> GetRecord(slot_id_t slot_id) const {
    if (slot_id >= SlotCount()) return std::nullopt;
    Slot s = ReadSlot(slot_id);
    if (s.length == 0) return std::nullopt;  
    return std::make_pair(page_->Data() + s.offset, s.length);
  }

  bool DeleteRecord(slot_id_t slot_id) {
    if (slot_id >= SlotCount()) return false;
    Slot s = ReadSlot(slot_id);
    if (s.length == 0) return false;
    s.length = 0;
    WriteSlot(slot_id, s);
    return true;
  }

  bool UpdateRecordInPlace(slot_id_t slot_id, const char* data, std::uint16_t length) {
    if (slot_id >= SlotCount()) return false;
    Slot s = ReadSlot(slot_id);
    if (s.length == 0) return false;
    if (length > s.length) return false;
    std::memcpy(page_->Data() + s.offset, data, length);
    s.length = length;
    WriteSlot(slot_id, s);
    return true;
  }

  bool IsTombstoned(slot_id_t slot_id) const {
    if (slot_id >= SlotCount()) return true;
    return ReadSlot(slot_id).length == 0;
  }

 private:
  Slot ReadSlot(slot_id_t slot_id) const {
    const char* base = page_->Data() + kHeaderSize + static_cast<std::size_t>(slot_id) * kSlotSize;
    Slot s;
    s.offset = util::ReadFixed<std::uint16_t>(base + 0);
    s.length = util::ReadFixed<std::uint16_t>(base + 2);
    return s;
  }

  void WriteSlot(slot_id_t slot_id, const Slot& s) {
    char* base = page_->Data() + kHeaderSize + static_cast<std::size_t>(slot_id) * kSlotSize;
    util::WriteFixed<std::uint16_t>(base + 0, s.offset);
    util::WriteFixed<std::uint16_t>(base + 2, s.length);
  }

  Page* page_;
};

}  

#include "storage/page_manager.hpp"

#include <filesystem>

#include "utilities/bytes.hpp"

namespace minidb::storage {

namespace {

void EncodeHeaderPage(Page* page, page_id_t next_page_id, std::size_t page_count) {
  page->Reset();
  util::WriteFixed<page_id_t>(page->Data() + 0, next_page_id);
  util::WriteFixed<std::uint32_t>(page->Data() + 4, static_cast<std::uint32_t>(page_count));
}
void DecodeHeaderPage(const Page& page, page_id_t* next_page_id, std::size_t* page_count) {
  *next_page_id = util::ReadFixed<page_id_t>(page.Data() + 0);
  *page_count = util::ReadFixed<std::uint32_t>(page.Data() + 4);
}
}

PageManager::PageManager(const std::string& path) : path_(path) {
  bool existed = std::filesystem::exists(path);

  file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
  if (!file_.is_open()) {
    std::ofstream create(path_, std::ios::binary);
    create.close();
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
  }

  if (existed) {
    ReadFileHeader();
  } else {
    next_page_id_ = minidb::config::kFirstDataPageId;
    page_count_ = 1;  
    WriteFileHeader();
  }
}

PageManager::~PageManager() {
  if (file_.is_open()) {
    WriteFileHeader();
    file_.flush();
    file_.close();
  }
}

std::streamoff PageManager::OffsetOf(page_id_t page_id) const {
  return static_cast<std::streamoff>(page_id) * static_cast<std::streamoff>(Page::Size());
}

void PageManager::ReadFileHeader() {
  std::lock_guard<std::mutex> lock(mutex_);
  Page header_page;
  file_.seekg(OffsetOf(minidb::config::kHeaderPageId));
  file_.read(header_page.Data(), static_cast<std::streamsize>(Page::Size()));
  DecodeHeaderPage(header_page, &next_page_id_, &page_count_);
}

void PageManager::WriteFileHeader() {
  Page header_page;
  EncodeHeaderPage(&header_page, next_page_id_, page_count_);
  file_.seekp(OffsetOf(minidb::config::kHeaderPageId));
  file_.write(header_page.Data(), static_cast<std::streamsize>(Page::Size()));
  file_.flush();
}

util::Result<page_id_t> PageManager::AllocatePage() {
  std::lock_guard<std::mutex> lock(mutex_);
  page_id_t new_id = next_page_id_;
  next_page_id_ += 1;
  page_count_ += 1;


  Page blank;
  file_.seekp(OffsetOf(new_id));
  file_.write(blank.Data(), static_cast<std::streamsize>(Page::Size()));
  if (!file_.good()) {
    return util::Status::IOError("failed to extend data file for new page");
  }
  file_.flush();


  WriteFileHeader();

  return util::Result<page_id_t>(new_id);
}

util::Status PageManager::ReadPage(page_id_t page_id, Page* out) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (page_id < 0 || static_cast<std::size_t>(page_id) >= page_count_) {
    return util::Status::InvalidArgument("ReadPage: page_id out of range");
  }
  file_.seekg(OffsetOf(page_id));
  file_.read(out->Data(), static_cast<std::streamsize>(Page::Size()));
  if (!file_.good() && !file_.eof()) {
    return util::Status::IOError("ReadPage: I/O error");
  }
  file_.clear();  
  return util::Status::OK();
}

util::Status PageManager::WritePage(page_id_t page_id, const Page& page) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (page_id < 0 || static_cast<std::size_t>(page_id) >= page_count_) {
    return util::Status::InvalidArgument("WritePage: page_id out of range");
  }
  file_.seekp(OffsetOf(page_id));
  file_.write(page.Data(), static_cast<std::streamsize>(Page::Size()));
  if (!file_.good()) {
    return util::Status::IOError("WritePage: I/O error");
  }
  file_.flush();
  return util::Status::OK();
}

}  

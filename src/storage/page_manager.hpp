#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

#include "storage/page.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

class PageManager {
 public:
  explicit PageManager(const std::string& path);
  ~PageManager();

  PageManager(const PageManager&) = delete;
  PageManager& operator=(const PageManager&) = delete;


  util::Result<page_id_t> AllocatePage();

  util::Status ReadPage(page_id_t page_id, Page* out);
  util::Status WritePage(page_id_t page_id, const Page& page);

  std::size_t PageCount() const { return page_count_; }

 private:
  void ReadFileHeader();
  void WriteFileHeader();
  std::streamoff OffsetOf(page_id_t page_id) const;

  std::string path_;
  mutable std::fstream file_;
  mutable std::mutex mutex_;

  page_id_t next_page_id_ = minidb::config::kFirstDataPageId;
  std::size_t page_count_ = 0;  
};

}  

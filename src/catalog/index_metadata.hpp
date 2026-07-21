#pragma once

#include <cstdint>
#include <string>

#include "storage/page.hpp"

namespace minidb::catalog {

struct IndexMetadata {
  std::uint32_t index_id = 0;
  std::string index_name;
  std::string table_name;
  std::string column_name;

  minidb::storage::page_id_t meta_page_id = minidb::storage::kInvalidPageId;

  minidb::storage::RecordId catalog_record_id;
};

} 

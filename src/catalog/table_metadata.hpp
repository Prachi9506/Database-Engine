#pragma once

#include <cstdint>
#include <string>

#include "common/schema.hpp"
#include "storage/page.hpp"

namespace minidb::catalog {

struct TableMetadata {
  std::uint32_t table_id = 0;
  std::string table_name;
  minidb::common::Schema schema;
  minidb::storage::page_id_t first_page_id = minidb::storage::kInvalidPageId;

  minidb::storage::RecordId catalog_record_id;
};

}  

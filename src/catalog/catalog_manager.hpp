#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "buffer/buffer_pool.hpp"
#include "catalog/index_metadata.hpp"
#include "catalog/table_metadata.hpp"
#include "storage/heap_file.hpp"
#include "utilities/status.hpp"

namespace minidb::catalog {

class CatalogManager {
 public:

  CatalogManager(minidb::buffer::BufferPool* catalog_buffer_pool,
                 minidb::storage::page_id_t catalog_first_page_id,
                 minidb::storage::page_id_t index_catalog_first_page_id);

  minidb::storage::page_id_t CatalogFirstPageId() const { return catalog_heap_.FirstPageId(); }
  minidb::storage::page_id_t IndexCatalogFirstPageId() const { return index_heap_.FirstPageId(); }

  util::Result<TableMetadata*> CreateTable(const std::string& name, const minidb::common::Schema& schema,
                                            minidb::storage::page_id_t data_first_page_id);
  util::Status DropTable(const std::string& name);
  TableMetadata* GetTable(const std::string& name);
  std::vector<std::string> ListTables() const;

  util::Result<IndexMetadata*> CreateIndex(const std::string& index_name, const std::string& table_name,
                                            const std::string& column_name,
                                            minidb::storage::page_id_t meta_page_id);
  util::Status DropIndex(const std::string& index_name);
  IndexMetadata* GetIndex(const std::string& index_name);
  std::vector<IndexMetadata*> GetIndexesForTable(const std::string& table_name);
  std::vector<std::string> ListIndexes() const;

 private:
  void LoadTablesFromDisk();
  void LoadIndexesFromDisk();
  static std::string EncodeTableMetadata(const TableMetadata& meta);
  static util::Result<TableMetadata> DecodeTableMetadata(const char* data, std::uint16_t len);
  static std::string EncodeIndexMetadata(const IndexMetadata& meta);
  static util::Result<IndexMetadata> DecodeIndexMetadata(const char* data, std::uint16_t len);

  minidb::storage::HeapFile catalog_heap_;
  minidb::storage::HeapFile index_heap_;
  std::unordered_map<std::string, TableMetadata> tables_;
  std::unordered_map<std::string, IndexMetadata> indexes_;
  std::uint32_t next_table_id_ = 1;
  std::uint32_t next_index_id_ = 1;
};

}  

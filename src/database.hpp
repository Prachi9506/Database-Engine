#pragma once

#include <memory>
#include <string>

#include "buffer/buffer_pool.hpp"
#include "catalog/catalog_manager.hpp"
#include "storage/page_manager.hpp"
#include "storage/table_manager.hpp"
#include "utilities/config.hpp"

namespace minidb {

class Database {
 public:

  explicit Database(const std::string& directory)
      : catalog_page_manager_(directory + "/catalog.db"),
        catalog_buffer_pool_(&catalog_page_manager_, config::kDefaultBufferPoolSize),
        data_page_manager_(directory + "/data.db"),
        data_buffer_pool_(&data_page_manager_, config::kDefaultBufferPoolSize),
        index_page_manager_(directory + "/index.db"),
        index_buffer_pool_(&index_page_manager_, config::kDefaultBufferPoolSize),
        catalog_manager_(&catalog_buffer_pool_, DetectExistingTableCatalogFirstPage(),
                          DetectExistingIndexCatalogFirstPage()),
        table_manager_(&data_buffer_pool_, &catalog_manager_) {}

  storage::TableManager& Tables() { return table_manager_; }
  catalog::CatalogManager& Catalog() { return catalog_manager_; }
  buffer::BufferPool& IndexBufferPool() { return index_buffer_pool_; }


  void FlushAll() {
    catalog_buffer_pool_.FlushAll();
    data_buffer_pool_.FlushAll();
    index_buffer_pool_.FlushAll();
  }

 private:

  storage::page_id_t DetectExistingTableCatalogFirstPage() {
    if (catalog_page_manager_.PageCount() > 1) return config::kFirstDataPageId;
    return storage::kInvalidPageId;
  }
  storage::page_id_t DetectExistingIndexCatalogFirstPage() {
    if (catalog_page_manager_.PageCount() > 2) return config::kFirstDataPageId + 1;
    return storage::kInvalidPageId;
  }

  storage::PageManager catalog_page_manager_;
  buffer::BufferPool catalog_buffer_pool_;
  storage::PageManager data_page_manager_;
  buffer::BufferPool data_buffer_pool_;
  storage::PageManager index_page_manager_;
  buffer::BufferPool index_buffer_pool_;
  catalog::CatalogManager catalog_manager_;
  storage::TableManager table_manager_;
};

}  

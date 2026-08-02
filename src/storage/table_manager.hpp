#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffer/buffer_pool.hpp"
#include "catalog/catalog_manager.hpp"
#include "common/schema.hpp"
#include "common/value.hpp"
#include "storage/heap_file.hpp"
#include "storage/record_manager.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

class TableManager {
 public:
  TableManager(minidb::buffer::BufferPool* data_buffer_pool, minidb::catalog::CatalogManager* catalog_manager)
      : data_buffer_pool_(data_buffer_pool), catalog_manager_(catalog_manager) {}

  util::Status CreateTable(const std::string& name, const minidb::common::Schema& schema) {
    HeapFile heap(data_buffer_pool_, kInvalidPageId);
    page_id_t new_first_page = heap.FirstPageId();

    auto created = catalog_manager_->CreateTable(name, schema, new_first_page);
    if (!created.ok()) return created.status();

    OpenAndCache(name, *created.value());
    return util::Status::OK();
  }

  util::Status DropTable(const std::string& name) {
    util::Status status = catalog_manager_->DropTable(name);
    if (!status.ok()) return status;
    open_tables_.erase(name);
    return util::Status::OK();
  }

  std::vector<std::string> ListTables() const { return catalog_manager_->ListTables(); }

  util::Result<RecordId> InsertRow(const std::string& table, const std::vector<minidb::common::Value>& values) {
    auto* entry = GetOrOpen(table);
    if (entry == nullptr) return util::Status::NotFound("InsertRow: no such table '" + table + "'");
    return entry->record_manager->InsertRow(values);
  }

  util::Result<std::vector<minidb::common::Value>> GetRow(const std::string& table, RecordId rid) {
    auto* entry = GetOrOpen(table);
    if (entry == nullptr) return util::Status::NotFound("GetRow: no such table '" + table + "'");
    return entry->record_manager->GetRow(rid);
  }

  util::Result<RecordId> UpdateRow(const std::string& table, RecordId rid,
                                    const std::vector<minidb::common::Value>& values) {
    auto* entry = GetOrOpen(table);
    if (entry == nullptr) return util::Status::NotFound("UpdateRow: no such table '" + table + "'");
    return entry->record_manager->UpdateRow(rid, values);
  }

  util::Status DeleteRow(const std::string& table, RecordId rid) {
    auto* entry = GetOrOpen(table);
    if (entry == nullptr) return util::Status::NotFound("DeleteRow: no such table '" + table + "'");
    return entry->record_manager->DeleteRow(rid);
  }

  util::Status Scan(const std::string& table,
                     const std::function<void(RecordId, const std::vector<minidb::common::Value>&)>& visitor) {
    auto* entry = GetOrOpen(table);
    if (entry == nullptr) return util::Status::NotFound("Scan: no such table '" + table + "'");
    entry->record_manager->Scan(visitor);
    return util::Status::OK();
  }

  const minidb::common::Schema* GetSchema(const std::string& table) {
    auto* meta = catalog_manager_->GetTable(table);
    if (meta == nullptr) return nullptr;
    return &meta->schema;
  }

 private:
  struct OpenTable {
    std::unique_ptr<HeapFile> heap_file;
    std::unique_ptr<RecordManager> record_manager;
  };

  void OpenAndCache(const std::string& name, const minidb::catalog::TableMetadata& meta) {
    auto entry = std::make_unique<OpenTable>();
    entry->heap_file = std::make_unique<HeapFile>(data_buffer_pool_, meta.first_page_id);
    entry->record_manager = std::make_unique<RecordManager>(entry->heap_file.get(), &meta.schema);
    open_tables_[name] = std::move(entry);
  }

  OpenTable* GetOrOpen(const std::string& name) {
    auto it = open_tables_.find(name);
    if (it != open_tables_.end()) return it->second.get();

    auto* meta = catalog_manager_->GetTable(name);
    if (meta == nullptr) return nullptr;
    OpenAndCache(name, *meta);
    return open_tables_[name].get();
  }

  minidb::buffer::BufferPool* data_buffer_pool_;
  minidb::catalog::CatalogManager* catalog_manager_;
  std::unordered_map<std::string, std::unique_ptr<OpenTable>> open_tables_;
};

}  

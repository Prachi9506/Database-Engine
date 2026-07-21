#include "catalog/catalog_manager.hpp"

#include "utilities/bytes.hpp"

namespace minidb::catalog {

using minidb::common::Column;
using minidb::common::Schema;
using minidb::storage::RecordId;

CatalogManager::CatalogManager(minidb::buffer::BufferPool* catalog_buffer_pool,
                                minidb::storage::page_id_t catalog_first_page_id,
                                minidb::storage::page_id_t index_catalog_first_page_id)
    : catalog_heap_(catalog_buffer_pool, catalog_first_page_id),
      index_heap_(catalog_buffer_pool, index_catalog_first_page_id) {
  if (catalog_first_page_id != minidb::storage::kInvalidPageId) {
    LoadTablesFromDisk();
  }
  if (index_catalog_first_page_id != minidb::storage::kInvalidPageId) {
    LoadIndexesFromDisk();
  }
}

std::string CatalogManager::EncodeTableMetadata(const TableMetadata& meta) {
  std::string out;

  char id_buf[4];
  util::WriteFixed<std::uint32_t>(id_buf, meta.table_id);
  out.append(id_buf, 4);

  char name_len_buf[2];
  util::WriteFixed<std::uint16_t>(name_len_buf, static_cast<std::uint16_t>(meta.table_name.size()));
  out.append(name_len_buf, 2);
  out.append(meta.table_name);

  char first_page_buf[4];
  util::WriteFixed<minidb::storage::page_id_t>(first_page_buf, meta.first_page_id);
  out.append(first_page_buf, 4);

  char col_count_buf[2];
  util::WriteFixed<std::uint16_t>(col_count_buf, static_cast<std::uint16_t>(meta.schema.ColumnCount()));
  out.append(col_count_buf, 2);

  for (const Column& col : meta.schema.Columns()) {
    char cname_len_buf[2];
    util::WriteFixed<std::uint16_t>(cname_len_buf, static_cast<std::uint16_t>(col.name.size()));
    out.append(cname_len_buf, 2);
    out.append(col.name);

    char type_buf[1] = {static_cast<char>(col.type)};
    out.append(type_buf, 1);

    char nullable_buf[1] = {static_cast<char>(col.nullable ? 1 : 0)};
    out.append(nullable_buf, 1);

    char maxlen_buf[2];
    util::WriteFixed<std::uint16_t>(maxlen_buf, col.max_length);
    out.append(maxlen_buf, 2);
  }

  return out;
}

util::Result<TableMetadata> CatalogManager::DecodeTableMetadata(const char* data, std::uint16_t len) {
  std::size_t cursor = 0;
  auto need = [&](std::size_t n) -> bool { return cursor + n <= len; };

  if (!need(4)) return util::Status::Corruption("catalog: truncated table_id");
  TableMetadata meta;
  meta.table_id = util::ReadFixed<std::uint32_t>(data + cursor);
  cursor += 4;

  if (!need(2)) return util::Status::Corruption("catalog: truncated name length");
  std::uint16_t name_len = util::ReadFixed<std::uint16_t>(data + cursor);
  cursor += 2;
  if (!need(name_len)) return util::Status::Corruption("catalog: truncated name");
  meta.table_name.assign(data + cursor, name_len);
  cursor += name_len;

  if (!need(4)) return util::Status::Corruption("catalog: truncated first_page_id");
  meta.first_page_id = util::ReadFixed<minidb::storage::page_id_t>(data + cursor);
  cursor += 4;

  if (!need(2)) return util::Status::Corruption("catalog: truncated column count");
  std::uint16_t col_count = util::ReadFixed<std::uint16_t>(data + cursor);
  cursor += 2;

  std::vector<Column> columns;
  columns.reserve(col_count);
  for (std::uint16_t i = 0; i < col_count; ++i) {
    if (!need(2)) return util::Status::Corruption("catalog: truncated column name length");
    std::uint16_t cname_len = util::ReadFixed<std::uint16_t>(data + cursor);
    cursor += 2;
    if (!need(cname_len)) return util::Status::Corruption("catalog: truncated column name");
    std::string cname(data + cursor, cname_len);
    cursor += cname_len;

    if (!need(1)) return util::Status::Corruption("catalog: truncated column type");
    auto type = static_cast<minidb::common::ColumnType>(data[cursor]);
    cursor += 1;

    if (!need(1)) return util::Status::Corruption("catalog: truncated nullable flag");
    bool nullable = data[cursor] != 0;
    cursor += 1;

    if (!need(2)) return util::Status::Corruption("catalog: truncated max_length");
    std::uint16_t max_length = util::ReadFixed<std::uint16_t>(data + cursor);
    cursor += 2;

    columns.push_back(Column{cname, type, nullable, max_length});
  }

  meta.schema = Schema(std::move(columns));
  return util::Result<TableMetadata>(std::move(meta));
}

void CatalogManager::LoadTablesFromDisk() {
  std::uint32_t max_id = 0;
  catalog_heap_.Scan([&](RecordId rid, const char* data, std::uint16_t len) {
    auto decoded = DecodeTableMetadata(data, len);
    if (!decoded.ok()) return;  
    TableMetadata meta = decoded.value();
    meta.catalog_record_id = rid;
    if (meta.table_id > max_id) max_id = meta.table_id;
    tables_[meta.table_name] = std::move(meta);
  });
  next_table_id_ = max_id + 1;
}

util::Result<TableMetadata*> CatalogManager::CreateTable(const std::string& name, const Schema& schema,
                                                          minidb::storage::page_id_t data_first_page_id) {
  if (tables_.count(name)) {
    return util::Status::AlreadyExists("CreateTable: table '" + name + "' already exists");
  }

  TableMetadata meta;
  meta.table_id = next_table_id_++;
  meta.table_name = name;
  meta.schema = schema;
  meta.first_page_id = data_first_page_id;

  std::string encoded = EncodeTableMetadata(meta);
  if (encoded.size() > 0xFFFF) {
    return util::Status::InvalidArgument("CreateTable: encoded metadata too large (too many/long columns)");
  }
  auto rid = catalog_heap_.InsertRecord(encoded.data(), static_cast<std::uint16_t>(encoded.size()));
  if (!rid.ok()) return rid.status();
  meta.catalog_record_id = rid.value();

  auto [it, inserted] = tables_.emplace(name, std::move(meta));
  (void)inserted;
  return util::Result<TableMetadata*>(&it->second);
}

util::Status CatalogManager::DropTable(const std::string& name) {
  auto it = tables_.find(name);
  if (it == tables_.end()) {
    return util::Status::NotFound("DropTable: table '" + name + "' does not exist");
  }
  util::Status status = catalog_heap_.DeleteRecord(it->second.catalog_record_id);
  if (!status.ok()) return status;
  tables_.erase(it);

  return util::Status::OK();
}

TableMetadata* CatalogManager::GetTable(const std::string& name) {
  auto it = tables_.find(name);
  if (it == tables_.end()) return nullptr;
  return &it->second;
}

std::vector<std::string> CatalogManager::ListTables() const {
  std::vector<std::string> names;
  names.reserve(tables_.size());
  for (auto& [name, meta] : tables_) names.push_back(name);
  return names;
}


std::string CatalogManager::EncodeIndexMetadata(const IndexMetadata& meta) {
  std::string out;

  char id_buf[4];
  util::WriteFixed<std::uint32_t>(id_buf, meta.index_id);
  out.append(id_buf, 4);

  auto append_string = [&](const std::string& s) {
    char len_buf[2];
    util::WriteFixed<std::uint16_t>(len_buf, static_cast<std::uint16_t>(s.size()));
    out.append(len_buf, 2);
    out.append(s);
  };
  append_string(meta.index_name);
  append_string(meta.table_name);
  append_string(meta.column_name);

  char meta_page_buf[4];
  util::WriteFixed<minidb::storage::page_id_t>(meta_page_buf, meta.meta_page_id);
  out.append(meta_page_buf, 4);

  return out;
}

util::Result<IndexMetadata> CatalogManager::DecodeIndexMetadata(const char* data, std::uint16_t len) {
  std::size_t cursor = 0;
  auto need = [&](std::size_t n) -> bool { return cursor + n <= len; };

  if (!need(4)) return util::Status::Corruption("index catalog: truncated index_id");
  IndexMetadata meta;
  meta.index_id = util::ReadFixed<std::uint32_t>(data + cursor);
  cursor += 4;

  auto read_string = [&](std::string* out) -> bool {
    if (!need(2)) return false;
    std::uint16_t slen = util::ReadFixed<std::uint16_t>(data + cursor);
    cursor += 2;
    if (!need(slen)) return false;
    out->assign(data + cursor, slen);
    cursor += slen;
    return true;
  };
  if (!read_string(&meta.index_name)) return util::Status::Corruption("index catalog: truncated index_name");
  if (!read_string(&meta.table_name)) return util::Status::Corruption("index catalog: truncated table_name");
  if (!read_string(&meta.column_name)) return util::Status::Corruption("index catalog: truncated column_name");

  if (!need(4)) return util::Status::Corruption("index catalog: truncated meta_page_id");
  meta.meta_page_id = util::ReadFixed<minidb::storage::page_id_t>(data + cursor);
  cursor += 4;

  return util::Result<IndexMetadata>(std::move(meta));
}

void CatalogManager::LoadIndexesFromDisk() {
  std::uint32_t max_id = 0;
  index_heap_.Scan([&](RecordId rid, const char* data, std::uint16_t len) {
    auto decoded = DecodeIndexMetadata(data, len);
    if (!decoded.ok()) return;
    IndexMetadata meta = decoded.value();
    meta.catalog_record_id = rid;
    if (meta.index_id > max_id) max_id = meta.index_id;
    indexes_[meta.index_name] = std::move(meta);
  });
  next_index_id_ = max_id + 1;
}

util::Result<IndexMetadata*> CatalogManager::CreateIndex(const std::string& index_name,
                                                          const std::string& table_name,
                                                          const std::string& column_name,
                                                          minidb::storage::page_id_t meta_page_id) {
  if (indexes_.count(index_name)) {
    return util::Status::AlreadyExists("CreateIndex: index '" + index_name + "' already exists");
  }

  IndexMetadata meta;
  meta.index_id = next_index_id_++;
  meta.index_name = index_name;
  meta.table_name = table_name;
  meta.column_name = column_name;
  meta.meta_page_id = meta_page_id;

  std::string encoded = EncodeIndexMetadata(meta);
  if (encoded.size() > 0xFFFF) {
    return util::Status::InvalidArgument("CreateIndex: encoded metadata too large");
  }
  auto rid = index_heap_.InsertRecord(encoded.data(), static_cast<std::uint16_t>(encoded.size()));
  if (!rid.ok()) return rid.status();
  meta.catalog_record_id = rid.value();

  auto [it, inserted] = indexes_.emplace(index_name, std::move(meta));
  (void)inserted;
  return util::Result<IndexMetadata*>(&it->second);
}

util::Status CatalogManager::DropIndex(const std::string& index_name) {
  auto it = indexes_.find(index_name);
  if (it == indexes_.end()) {
    return util::Status::NotFound("DropIndex: index '" + index_name + "' does not exist");
  }
  util::Status status = index_heap_.DeleteRecord(it->second.catalog_record_id);
  if (!status.ok()) return status;
  indexes_.erase(it);

  return util::Status::OK();
}

IndexMetadata* CatalogManager::GetIndex(const std::string& index_name) {
  auto it = indexes_.find(index_name);
  if (it == indexes_.end()) return nullptr;
  return &it->second;
}

std::vector<IndexMetadata*> CatalogManager::GetIndexesForTable(const std::string& table_name) {
  std::vector<IndexMetadata*> result;
  for (auto& [name, meta] : indexes_) {
    if (meta.table_name == table_name) result.push_back(&meta);
  }
  return result;
}

std::vector<std::string> CatalogManager::ListIndexes() const {
  std::vector<std::string> names;
  names.reserve(indexes_.size());
  for (auto& [name, meta] : indexes_) names.push_back(name);
  return names;
}

}  

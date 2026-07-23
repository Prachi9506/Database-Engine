#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/value.hpp"

namespace minidb::common {

struct Column {
  std::string name;
  ColumnType type;
  bool nullable = true;
  std::uint16_t max_length = 255;
};

class Schema {
 public:
  Schema() = default;
  explicit Schema(std::vector<Column> columns) : columns_(std::move(columns)) {}

  const std::vector<Column>& Columns() const { return columns_; }
  std::size_t ColumnCount() const { return columns_.size(); }

  std::optional<std::size_t> IndexOf(const std::string& column_name) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
      if (columns_[i].name == column_name) return i;
    }
    return std::nullopt;
  }

  const Column& At(std::size_t index) const { return columns_[index]; }

 private:
  std::vector<Column> columns_;
};

}  

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace minidb::common {

enum class ColumnType : std::uint8_t {
  kInt = 0,     
  kBigInt = 1,  
  kDouble = 2,  
  kBool = 3,
  kVarchar = 4, 
};

inline const char* ColumnTypeName(ColumnType t) {
  switch (t) {
    case ColumnType::kInt: return "INT";
    case ColumnType::kBigInt: return "BIGINT";
    case ColumnType::kDouble: return "DOUBLE";
    case ColumnType::kBool: return "BOOLEAN";
    case ColumnType::kVarchar: return "VARCHAR";
  }
  return "UNKNOWN";
}

using Value = std::variant<std::monostate, std::int32_t, std::int64_t, double, bool, std::string>;

inline bool IsNull(const Value& v) { return std::holds_alternative<std::monostate>(v); }


inline bool ValueMatchesType(const Value& v, ColumnType type) {
  if (IsNull(v)) return true;
  switch (type) {
    case ColumnType::kInt: return std::holds_alternative<std::int32_t>(v);
    case ColumnType::kBigInt: return std::holds_alternative<std::int64_t>(v);
    case ColumnType::kDouble: return std::holds_alternative<double>(v);
    case ColumnType::kBool: return std::holds_alternative<bool>(v);
    case ColumnType::kVarchar: return std::holds_alternative<std::string>(v);
  }
  return false;
}

}  

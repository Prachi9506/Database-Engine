#pragma once

#include <optional>
#include <string>
#include <variant>

#include "common/value.hpp"

namespace minidb::executor {

inline std::optional<double> ValueToDoubleOrNull(const minidb::common::Value& v) {
  if (std::holds_alternative<std::int32_t>(v)) return static_cast<double>(std::get<std::int32_t>(v));
  if (std::holds_alternative<std::int64_t>(v)) return static_cast<double>(std::get<std::int64_t>(v));
  if (std::holds_alternative<double>(v)) return std::get<double>(v);
  return std::nullopt;
}

inline int CompareForSort(const minidb::common::Value& a, const minidb::common::Value& b) {
  auto da = ValueToDoubleOrNull(a), db = ValueToDoubleOrNull(b);
  if (da.has_value() && db.has_value()) return (*da < *db) ? -1 : (*da > *db ? 1 : 0);
  if (std::holds_alternative<std::string>(a) && std::holds_alternative<std::string>(b)) {
    const auto& sa = std::get<std::string>(a);
    const auto& sb = std::get<std::string>(b);
    return (sa < sb) ? -1 : (sa > sb ? 1 : 0);
  }
  if (std::holds_alternative<bool>(a) && std::holds_alternative<bool>(b)) {
    bool ba = std::get<bool>(a), bb = std::get<bool>(b);
    return ba == bb ? 0 : (ba ? 1 : -1);
  }
  return 0;
}

}  

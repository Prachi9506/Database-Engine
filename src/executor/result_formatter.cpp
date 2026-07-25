#include "executor/result_formatter.hpp"

#include <algorithm>
#include <sstream>
#include <variant>
#include <vector>

namespace minidb::executor {

using minidb::common::IsNull;
using minidb::common::Value;

std::string FormatValue(const Value& value) {
  if (IsNull(value)) return "NULL";
  if (std::holds_alternative<std::int32_t>(value)) return std::to_string(std::get<std::int32_t>(value));
  if (std::holds_alternative<std::int64_t>(value)) return std::to_string(std::get<std::int64_t>(value));
  if (std::holds_alternative<double>(value)) return std::to_string(std::get<double>(value));
  if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
  if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
  return "?";
}

std::string FormatQueryResult(const QueryResult& result) {
  if (result.column_names.empty()) {
    return result.message;
  }

  std::size_t ncols = result.column_names.size();
  std::vector<std::size_t> widths(ncols);
  for (std::size_t c = 0; c < ncols; ++c) widths[c] = result.column_names[c].size();

  std::vector<std::vector<std::string>> cells;
  cells.reserve(result.rows.size());
  for (const auto& row : result.rows) {
    std::vector<std::string> rendered;
    rendered.reserve(ncols);
    for (std::size_t c = 0; c < ncols && c < row.size(); ++c) {
      std::string text = FormatValue(row[c]);
      widths[c] = std::max(widths[c], text.size());
      rendered.push_back(std::move(text));
    }
    cells.push_back(std::move(rendered));
  }

  auto pad = [](const std::string& s, std::size_t width) { return s + std::string(width - s.size(), ' '); };

  std::ostringstream out;
  for (std::size_t c = 0; c < ncols; ++c) {
    out << pad(result.column_names[c], widths[c]);
    if (c + 1 < ncols) out << " | ";
  }
  out << "\n";
  for (std::size_t c = 0; c < ncols; ++c) {
    out << std::string(widths[c], '-');
    if (c + 1 < ncols) out << "-+-";
  }
  out << "\n";
  for (const auto& row : cells) {
    for (std::size_t c = 0; c < ncols; ++c) {
      out << pad(c < row.size() ? row[c] : "", widths[c]);
      if (c + 1 < ncols) out << " | ";
    }
    out << "\n";
  }
  out << "(" << result.rows.size() << (result.rows.size() == 1 ? " row)" : " rows)");
  if (!result.scan_strategy.empty()) out << "  [" << result.scan_strategy << "]";
  return out.str();
}

}  

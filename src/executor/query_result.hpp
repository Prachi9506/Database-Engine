#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "common/value.hpp"

namespace minidb::executor {

struct QueryResult {
  std::vector<std::string> column_names;
  std::vector<std::vector<minidb::common::Value>> rows;

  std::size_t rows_affected = 0;

  std::string message;


  std::string scan_strategy;
};

}  


#pragma once

#include <string>

#include "executor/query_result.hpp"

namespace minidb::executor {

std::string FormatValue(const minidb::common::Value& value);


std::string FormatQueryResult(const QueryResult& result);

}  

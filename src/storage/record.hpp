#pragma once

#include <string>
#include <vector>

#include "common/schema.hpp"
#include "common/value.hpp"
#include "utilities/status.hpp"

namespace minidb::storage {

class RecordCodec {
 public:
  static util::Result<std::string> Encode(const minidb::common::Schema& schema,
                                           const std::vector<minidb::common::Value>& values);

  static util::Result<std::vector<minidb::common::Value>> Decode(const minidb::common::Schema& schema,
                                                                   const std::string& bytes);

 private:
  static std::size_t NullBitmapBytes(std::size_t column_count) { return (column_count + 7) / 8; }
};

}  

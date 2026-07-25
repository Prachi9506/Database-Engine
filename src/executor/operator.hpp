#pragma once

#include <optional>
#include <vector>

#include "common/value.hpp"
#include "storage/page.hpp"
#include "utilities/status.hpp"

namespace minidb::executor {

struct Tuple {
  minidb::storage::RecordId rid;  
  std::vector<minidb::common::Value> values;
};

class Operator {
 public:
  virtual ~Operator() = default;
  virtual void Open() = 0;
  virtual std::optional<Tuple> Next() = 0;
  virtual void Close() = 0;


  virtual util::Status GetError() const { return util::Status::OK(); }
};

}  

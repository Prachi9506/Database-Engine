#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "database.hpp"
#include "executor/executor.hpp"

namespace minidb::cli {

std::string ProcessStatement(minidb::executor::Executor* executor, const std::string& sql);

class Repl {
 public:
  explicit Repl(minidb::Database* db) : db_(db), executor_(db) {}

  void Run(std::istream& in, std::ostream& out);

 private:
  minidb::Database* db_;
  minidb::executor::Executor executor_;
};

}

#include <filesystem>
#include <iostream>

#include "cli/repl.hpp"
#include "database.hpp"

int main(int argc, char** argv) {
  std::string dir = (argc > 1) ? argv[1] : "./minidb_data";
  std::filesystem::create_directories(dir);

  minidb::Database db(dir);
  minidb::cli::Repl repl(&db);
  repl.Run(std::cin, std::cout);
  return 0;
}

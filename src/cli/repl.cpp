#include "cli/repl.hpp"

#include "executor/result_formatter.hpp"
#include "parser/parser.hpp"

namespace minidb::cli {

std::string ProcessStatement(minidb::executor::Executor* executor, const std::string& sql) {
  bool blank = true;
  for (char c : sql) {
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ';') { blank = false; break; }
  }
  if (blank) return "";

  auto stmt = minidb::parser::ParseSQL(sql);
  if (!stmt.ok()) return "ERROR: " + stmt.status().message();

  auto result = executor->Execute(stmt.value());
  if (!result.ok()) return "ERROR: " + result.status().message();

  return minidb::executor::FormatQueryResult(result.value());
}

void Repl::Run(std::istream& in, std::ostream& out) {
  out << "MiniDB -- type SQL statements ending in ';', or .exit to quit.\n";

  std::string buffer;
  std::string line;
  bool first_line_of_statement = true;

  while (true) {
    out << (first_line_of_statement ? "minidb> " : "    ...> ");
    out.flush();
    if (!std::getline(in, line)) break;  

    if (first_line_of_statement && (line == ".exit" || line == ".quit")) break;

    buffer += line;
    buffer += '\n';

    bool in_string = false;
    bool found_terminator = false;
    for (char c : line) {
      if (c == '\'') in_string = !in_string;
      else if (c == ';' && !in_string) { found_terminator = true; break; }
    }

    if (found_terminator) {
      std::string output = ProcessStatement(&executor_, buffer);
      if (!output.empty()) out << output << "\n";
      buffer.clear();
      first_line_of_statement = true;
    } else {
      first_line_of_statement = false;
    }
  }


  db_->FlushAll();
}

}  // namespace minidb::cli

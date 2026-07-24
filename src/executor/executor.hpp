#pragma once

#include "database.hpp"
#include "executor/query_result.hpp"
#include "parser/ast.hpp"
#include "utilities/status.hpp"

namespace minidb::executor {

class Executor {
 public:
  explicit Executor(minidb::Database* db) : db_(db) {}

  util::Result<QueryResult> Execute(const minidb::parser::Statement& stmt);

 private:
  util::Result<QueryResult> ExecuteCreateTable(const minidb::parser::CreateTableStmt& stmt);
  util::Result<QueryResult> ExecuteDropTable(const minidb::parser::DropTableStmt& stmt);
  util::Result<QueryResult> ExecuteCreateIndex(const minidb::parser::CreateIndexStmt& stmt);
  util::Result<QueryResult> ExecuteDropIndex(const minidb::parser::DropIndexStmt& stmt);
  util::Result<QueryResult> ExecuteInsert(const minidb::parser::InsertStmt& stmt);
  util::Result<QueryResult> ExecuteUpdate(const minidb::parser::UpdateStmt& stmt);
  util::Result<QueryResult> ExecuteDelete(const minidb::parser::DeleteStmt& stmt);
  util::Result<QueryResult> ExecuteSelect(const minidb::parser::SelectStmt& stmt);

  minidb::Database* db_;
};

}  

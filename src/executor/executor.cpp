#include "executor/executor.hpp"

#include <variant>

#include "catalog/index_metadata.hpp"
#include "executor/aggregate.hpp"
#include "executor/expr_eval.hpp"
#include "executor/filter.hpp"
#include "executor/index_scan.hpp"
#include "executor/limit.hpp"
#include "executor/project.hpp"
#include "executor/seq_scan.hpp"
#include "executor/sort.hpp"
#include "index/bplus_tree.hpp"

namespace minidb::executor {

using minidb::common::ColumnType;
using minidb::common::Schema;
using minidb::common::Value;
using minidb::parser::AggregateFunc;

namespace {

util::Result<Value> CoerceLiteralToColumnType(const Value& literal, const minidb::common::Column& col) {
  if (minidb::common::IsNull(literal)) {
    if (!col.nullable) {
      return util::Status::InvalidArgument("NULL given for NOT NULL column '" + col.name + "'");
    }
    return util::Result<Value>(literal);
  }

  switch (col.type) {
    case ColumnType::kInt: {
      if (!std::holds_alternative<std::int64_t>(literal)) {
        return util::Status::InvalidArgument("expected an integer value for column '" + col.name + "'");
      }
      std::int64_t v = std::get<std::int64_t>(literal);
      if (v < INT32_MIN || v > INT32_MAX) {
        return util::Status::InvalidArgument("integer value out of range for INT column '" + col.name + "'");
      }
      return util::Result<Value>(Value(static_cast<std::int32_t>(v)));
    }
    case ColumnType::kBigInt: {
      if (!std::holds_alternative<std::int64_t>(literal)) {
        return util::Status::InvalidArgument("expected an integer value for column '" + col.name + "'");
      }
      return util::Result<Value>(literal);
    }
    case ColumnType::kDouble: {
      if (std::holds_alternative<double>(literal)) return util::Result<Value>(literal);
      if (std::holds_alternative<std::int64_t>(literal)) {
        return util::Result<Value>(Value(static_cast<double>(std::get<std::int64_t>(literal))));
      }
      return util::Status::InvalidArgument("expected a numeric value for column '" + col.name + "'");
    }
    case ColumnType::kBool: {
      if (!std::holds_alternative<bool>(literal)) {
        return util::Status::InvalidArgument("expected a boolean value for column '" + col.name + "'");
      }
      return util::Result<Value>(literal);
    }
    case ColumnType::kVarchar: {
      if (!std::holds_alternative<std::string>(literal)) {
        return util::Status::InvalidArgument("expected a string value for column '" + col.name + "'");
      }
      return util::Result<Value>(literal);
    }
  }
  return util::Status::InvalidArgument("unknown column type for '" + col.name + "'");
}

}  

namespace {


std::optional<minidb::index::Key> ExtractInt64Key(const Value& v) {
  if (std::holds_alternative<std::int32_t>(v)) return static_cast<std::int64_t>(std::get<std::int32_t>(v));
  if (std::holds_alternative<std::int64_t>(v)) return std::get<std::int64_t>(v);
  return std::nullopt;
}

void MaintainIndexesOnInsert(minidb::Database* db, const std::string& table_name, const Schema& schema,
                              const std::vector<Value>& row, storage::RecordId rid) {
  for (auto* idx : db->Catalog().GetIndexesForTable(table_name)) {
    auto col_idx = schema.IndexOf(idx->column_name);
    if (!col_idx.has_value()) continue;
    auto key = ExtractInt64Key(row[*col_idx]);
    if (!key.has_value()) continue; 
    minidb::index::BPlusTree tree(&db->IndexBufferPool(), idx->meta_page_id);
    tree.Insert(*key, rid);
  }
}

void MaintainIndexesOnDelete(minidb::Database* db, const std::string& table_name, const Schema& schema,
                              const std::vector<Value>& row, storage::RecordId rid) {
  for (auto* idx : db->Catalog().GetIndexesForTable(table_name)) {
    auto col_idx = schema.IndexOf(idx->column_name);
    if (!col_idx.has_value()) continue;
    auto key = ExtractInt64Key(row[*col_idx]);
    if (!key.has_value()) continue;
    minidb::index::BPlusTree tree(&db->IndexBufferPool(), idx->meta_page_id);
    tree.Delete(*key, rid);  
  }
}

}  

util::Result<QueryResult> Executor::Execute(const minidb::parser::Statement& stmt) {
  return std::visit(
      [this](const auto& s) -> util::Result<QueryResult> {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, minidb::parser::CreateTableStmt>) return ExecuteCreateTable(s);
        else if constexpr (std::is_same_v<T, minidb::parser::DropTableStmt>) return ExecuteDropTable(s);
        else if constexpr (std::is_same_v<T, minidb::parser::CreateIndexStmt>) return ExecuteCreateIndex(s);
        else if constexpr (std::is_same_v<T, minidb::parser::DropIndexStmt>) return ExecuteDropIndex(s);
        else if constexpr (std::is_same_v<T, minidb::parser::InsertStmt>) return ExecuteInsert(s);
        else if constexpr (std::is_same_v<T, minidb::parser::UpdateStmt>) return ExecuteUpdate(s);
        else if constexpr (std::is_same_v<T, minidb::parser::DeleteStmt>) return ExecuteDelete(s);
        else if constexpr (std::is_same_v<T, minidb::parser::SelectStmt>) return ExecuteSelect(s);
      },
      stmt);
}

util::Result<QueryResult> Executor::ExecuteCreateTable(const minidb::parser::CreateTableStmt& stmt) {
  Schema schema(stmt.columns);
  util::Status status = db_->Tables().CreateTable(stmt.table_name, schema);
  if (!status.ok()) return status;
  QueryResult result;
  result.message = "CREATE TABLE";
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteDropTable(const minidb::parser::DropTableStmt& stmt) {
  util::Status status = db_->Tables().DropTable(stmt.table_name);
  if (!status.ok()) return status;
  QueryResult result;
  result.message = "DROP TABLE";
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteCreateIndex(const minidb::parser::CreateIndexStmt& stmt) {
  const Schema* schema = db_->Tables().GetSchema(stmt.table_name);
  if (schema == nullptr) return util::Status::NotFound("no such table '" + stmt.table_name + "'");

  auto col_idx = schema->IndexOf(stmt.column_name);
  if (!col_idx.has_value()) {
    return util::Status::InvalidArgument("unknown column '" + stmt.column_name + "' in CREATE INDEX");
  }
  ColumnType col_type = schema->At(*col_idx).type;
  if (col_type != ColumnType::kInt && col_type != ColumnType::kBigInt) {
    return util::Status::InvalidArgument(
        "index only supported on INT/BIGINT columns in this implementation, column '" + stmt.column_name +
        "' is " + std::string(minidb::common::ColumnTypeName(col_type)));
  }

  minidb::index::BPlusTree tree(&db_->IndexBufferPool(), storage::kInvalidPageId);
  util::Status scan_status = db_->Tables().Scan(
      stmt.table_name, [&](storage::RecordId rid, const std::vector<Value>& values) {
        auto key = ExtractInt64Key(values[*col_idx]);
        if (key.has_value()) tree.Insert(*key, rid);
      });
  if (!scan_status.ok()) return scan_status;

  auto created = db_->Catalog().CreateIndex(stmt.index_name, stmt.table_name, stmt.column_name, tree.MetaPageId());
  if (!created.ok()) return created.status();

  QueryResult result;
  result.message = "CREATE INDEX";
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteDropIndex(const minidb::parser::DropIndexStmt& stmt) {
  util::Status status = db_->Catalog().DropIndex(stmt.index_name);
  if (!status.ok()) return status;
  QueryResult result;
  result.message = "DROP INDEX";
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteInsert(const minidb::parser::InsertStmt& stmt) {
  const Schema* schema = db_->Tables().GetSchema(stmt.table_name);
  if (schema == nullptr) return util::Status::NotFound("no such table '" + stmt.table_name + "'");

  std::vector<std::size_t> target_indices;
  if (stmt.columns.empty()) {
    for (std::size_t i = 0; i < schema->ColumnCount(); ++i) target_indices.push_back(i);
  } else {
    for (const auto& col_name : stmt.columns) {
      auto idx = schema->IndexOf(col_name);
      if (!idx.has_value()) return util::Status::InvalidArgument("unknown column '" + col_name + "' in INSERT");
      target_indices.push_back(*idx);
    }
  }

  std::size_t inserted = 0;
  for (const auto& row : stmt.rows) {
    if (row.size() != target_indices.size()) {
      return util::Status::InvalidArgument("INSERT value count does not match column count");
    }
    std::vector<Value> full_row(schema->ColumnCount(), Value(std::monostate{}));
    for (std::size_t i = 0; i < row.size(); ++i) {
      std::size_t col_idx = target_indices[i];
      auto coerced = CoerceLiteralToColumnType(row[i], schema->At(col_idx));
      if (!coerced.ok()) return coerced.status();
      full_row[col_idx] = coerced.value();
    }
    auto rid = db_->Tables().InsertRow(stmt.table_name, full_row);
    if (!rid.ok()) return rid.status();
    MaintainIndexesOnInsert(db_, stmt.table_name, *schema, full_row, rid.value());
    ++inserted;
  }

  QueryResult result;
  result.rows_affected = inserted;
  result.message = "INSERT 0 " + std::to_string(inserted);
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteUpdate(const minidb::parser::UpdateStmt& stmt) {
  const Schema* schema = db_->Tables().GetSchema(stmt.table_name);
  if (schema == nullptr) return util::Status::NotFound("no such table '" + stmt.table_name + "'");

  std::vector<std::pair<std::size_t, Value>> assignments;
  for (const auto& [col_name, literal] : stmt.assignments) {
    auto idx = schema->IndexOf(col_name);
    if (!idx.has_value()) return util::Status::InvalidArgument("unknown column '" + col_name + "' in UPDATE SET");
    auto coerced = CoerceLiteralToColumnType(literal, schema->At(*idx));
    if (!coerced.ok()) return coerced.status();
    assignments.emplace_back(*idx, coerced.value());
  }


  struct Match { storage::RecordId rid; std::vector<Value> values; };
  std::vector<Match> matches;
  util::Status scan_status = db_->Tables().Scan(
      stmt.table_name, [&](storage::RecordId rid, const std::vector<Value>& values) {
        auto matched = EvaluatePredicate(stmt.where.get(), *schema, values);
        if (matched.ok() && matched.value()) matches.push_back(Match{rid, values});
      });
  if (!scan_status.ok()) return scan_status;

  std::size_t updated = 0;
  for (auto& match : matches) {
    std::vector<Value> old_values = match.values;  // keep a copy for index removal
    storage::RecordId old_rid = match.rid;
    for (const auto& [idx, value] : assignments) match.values[idx] = value;
    auto result = db_->Tables().UpdateRow(stmt.table_name, match.rid, match.values);
    if (!result.ok()) return result.status();

    MaintainIndexesOnDelete(db_, stmt.table_name, *schema, old_values, old_rid);
    MaintainIndexesOnInsert(db_, stmt.table_name, *schema, match.values, result.value());
    ++updated;
  }

  QueryResult result;
  result.rows_affected = updated;
  result.message = "UPDATE " + std::to_string(updated);
  return util::Result<QueryResult>(std::move(result));
}

util::Result<QueryResult> Executor::ExecuteDelete(const minidb::parser::DeleteStmt& stmt) {
  const Schema* schema = db_->Tables().GetSchema(stmt.table_name);
  if (schema == nullptr) return util::Status::NotFound("no such table '" + stmt.table_name + "'");


  struct Match { storage::RecordId rid; std::vector<Value> values; };
  std::vector<Match> matches;
  util::Status scan_status = db_->Tables().Scan(
      stmt.table_name, [&](storage::RecordId rid, const std::vector<Value>& values) {
        auto matched = EvaluatePredicate(stmt.where.get(), *schema, values);
        if (matched.ok() && matched.value()) matches.push_back(Match{rid, values});
      });
  if (!scan_status.ok()) return scan_status;

  std::size_t deleted = 0;
  for (auto& match : matches) {
    util::Status status = db_->Tables().DeleteRow(stmt.table_name, match.rid);
    if (!status.ok()) return status;
    MaintainIndexesOnDelete(db_, stmt.table_name, *schema, match.values, match.rid);
    ++deleted;
  }

  QueryResult result;
  result.rows_affected = deleted;
  result.message = "DELETE " + std::to_string(deleted);
  return util::Result<QueryResult>(std::move(result));
}

namespace {

using minidb::parser::BinaryExpr;
using minidb::parser::BinaryOp;
using minidb::parser::ColumnRefExpr;
using minidb::parser::Expr;
using minidb::parser::LiteralExpr;


void FlattenAnd(const Expr* e, std::vector<const Expr*>* out) {
  if (const auto* bin = std::get_if<BinaryExpr>(&e->node); bin != nullptr && bin->op == BinaryOp::kAnd) {
    FlattenAnd(bin->left.get(), out);
    FlattenAnd(bin->right.get(), out);
  } else {
    out->push_back(e);
  }
}

BinaryOp FlipComparisonOp(BinaryOp op) {
  switch (op) {
    case BinaryOp::kLt: return BinaryOp::kGt;
    case BinaryOp::kGt: return BinaryOp::kLt;
    case BinaryOp::kLte: return BinaryOp::kGte;
    case BinaryOp::kGte: return BinaryOp::kLte;
    default: return op;  
  }
}

struct IndexableConjunct {
  std::string column;
  BinaryOp op;
  minidb::index::Key value;
};


std::optional<IndexableConjunct> ExtractIndexableConjunct(const Expr* e) {
  const auto* bin = std::get_if<BinaryExpr>(&e->node);
  if (bin == nullptr) return std::nullopt;
  if (bin->op == BinaryOp::kAnd || bin->op == BinaryOp::kOr) return std::nullopt;

  const auto* left_col = std::get_if<ColumnRefExpr>(&bin->left->node);
  const auto* right_lit = std::get_if<LiteralExpr>(&bin->right->node);
  if (left_col != nullptr && right_lit != nullptr) {
    if (!std::holds_alternative<std::int64_t>(right_lit->value)) return std::nullopt;
    return IndexableConjunct{left_col->column, bin->op, std::get<std::int64_t>(right_lit->value)};
  }

  const auto* right_col = std::get_if<ColumnRefExpr>(&bin->right->node);
  const auto* left_lit = std::get_if<LiteralExpr>(&bin->left->node);
  if (right_col != nullptr && left_lit != nullptr) {
    if (!std::holds_alternative<std::int64_t>(left_lit->value)) return std::nullopt;
    return IndexableConjunct{right_col->column, FlipComparisonOp(bin->op), std::get<std::int64_t>(left_lit->value)};
  }

  return std::nullopt;
}

}  

util::Result<QueryResult> Executor::ExecuteSelect(const minidb::parser::SelectStmt& stmt) {
  const Schema* schema = db_->Tables().GetSchema(stmt.table_name);
  if (schema == nullptr) return util::Status::NotFound("no such table '" + stmt.table_name + "'");

  bool any_aggregate = false, any_plain = false;
  for (const auto& item : stmt.items) {
    if (item.aggregate.has_value()) any_aggregate = true;
    else any_plain = true;
  }
  if (any_aggregate && any_plain) {
    return util::Status::InvalidArgument(
        "cannot mix aggregate and non-aggregate columns without GROUP BY (not supported)");
  }
  std::unique_ptr<Operator> chain;
  std::string scan_strategy = "SeqScan";

  if (stmt.where != nullptr) {
    bool top_is_or = false;
    if (const auto* bin = std::get_if<minidb::parser::BinaryExpr>(&stmt.where->node);
        bin != nullptr && bin->op == minidb::parser::BinaryOp::kOr) {
      top_is_or = true;
    }
    if (!top_is_or) {
      std::vector<const minidb::parser::Expr*> conjuncts;
      FlattenAnd(stmt.where.get(), &conjuncts);

      auto indexes = db_->Catalog().GetIndexesForTable(stmt.table_name);
      std::optional<IndexableConjunct> chosen;
      catalog::IndexMetadata* chosen_index = nullptr;

      for (const auto* c : conjuncts) {
        auto candidate = ExtractIndexableConjunct(c);
        if (!candidate.has_value()) continue;
        catalog::IndexMetadata* match = nullptr;
        for (auto* idx : indexes) {
          if (idx->column_name == candidate->column) { match = idx; break; }
        }
        if (match == nullptr) continue;

        if (candidate->op == minidb::parser::BinaryOp::kEq) {
          chosen = candidate;
          chosen_index = match;
          break;  
        }
        if (!chosen.has_value()) {
          chosen = candidate;
          chosen_index = match;
          
        }
      }

      if (chosen.has_value()) {
        minidb::index::BPlusTree tree(&db_->IndexBufferPool(), chosen_index->meta_page_id);
        if (chosen->op == minidb::parser::BinaryOp::kEq) {
          chain = std::make_unique<IndexScanOperator>(&db_->Tables(), stmt.table_name, tree, chosen->value);
        } else {
          std::optional<minidb::index::Key> low, high;
          bool low_inclusive = true, high_inclusive = true;
          switch (chosen->op) {
            case minidb::parser::BinaryOp::kLt: high = chosen->value; high_inclusive = false; break;
            case minidb::parser::BinaryOp::kLte: high = chosen->value; high_inclusive = true; break;
            case minidb::parser::BinaryOp::kGt: low = chosen->value; low_inclusive = false; break;
            case minidb::parser::BinaryOp::kGte: low = chosen->value; low_inclusive = true; break;
            default: break;
          }
          chain = std::make_unique<IndexScanOperator>(&db_->Tables(), stmt.table_name, tree, low, low_inclusive,
                                                        high, high_inclusive);
        }
        scan_strategy = "IndexScan(" + chosen_index->index_name + ")";
      }
    }
  }

  if (!chain) {
    chain = std::make_unique<SeqScanOperator>(&db_->Tables(), stmt.table_name);
  }
  if (stmt.where != nullptr) {
    chain = std::make_unique<FilterOperator>(std::move(chain), stmt.where.get(), schema);
  }

  QueryResult result;
  result.scan_strategy = scan_strategy;

  if (any_aggregate) {
    auto agg = std::make_unique<AggregateOperator>(std::move(chain), stmt.items, schema);
    agg->Open();
    result.column_names = agg->ColumnNames();
    while (auto tuple = agg->Next()) result.rows.push_back(std::move(tuple->values));
    util::Status err = agg->GetError();
    agg->Close();
    if (!err.ok()) return err;
    result.message = "SELECT " + std::to_string(result.rows.size());
    return util::Result<QueryResult>(std::move(result));
  }

 
  if (stmt.order_by.has_value()) {
    chain = std::make_unique<SortOperator>(std::move(chain), *stmt.order_by, schema);
  }

  auto project = std::make_unique<ProjectOperator>(std::move(chain), stmt.items, schema);
  result.column_names = project->ColumnNames();
  std::unique_ptr<Operator> final_chain = std::move(project);

  if (stmt.limit.has_value()) {
    final_chain = std::make_unique<LimitOperator>(std::move(final_chain), *stmt.limit);
  }

  final_chain->Open();
  while (auto tuple = final_chain->Next()) result.rows.push_back(std::move(tuple->values));
  util::Status err = final_chain->GetError();
  final_chain->Close();
  if (!err.ok()) return err;

  result.message = "SELECT " + std::to_string(result.rows.size());
  return util::Result<QueryResult>(std::move(result));
}

} 

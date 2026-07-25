#pragma once

#include <memory>
#include <string>
#include <vector>

#include "executor/operator.hpp"
#include "parser/ast.hpp"

namespace minidb::executor {

class ProjectOperator : public Operator {
 public:
  ProjectOperator(std::unique_ptr<Operator> child, std::vector<minidb::parser::SelectItem> items,
                   const minidb::common::Schema* schema)
      : child_(std::move(child)), items_(std::move(items)), schema_(schema) {
    BuildColumnPlan();
  }

  void Open() override {
    child_->Open();
    error_ = util::Status::OK();
  }

  std::optional<Tuple> Next() override {
    auto in = child_->Next();
    if (!in.has_value()) return std::nullopt;

    Tuple out;
    out.rid = in->rid;
    out.values.reserve(indices_.size());
    for (std::size_t idx : indices_) {
      if (idx >= in->values.size()) {
        error_ = util::Status::Corruption("Project: column index out of range");
        return std::nullopt;
      }
      out.values.push_back(in->values[idx]);
    }
    return out;
  }

  void Close() override { child_->Close(); }

  const std::vector<std::string>& ColumnNames() const { return column_names_; }
  util::Status GetError() const override { return error_.ok() ? child_->GetError() : error_; }

 private:
  void BuildColumnPlan() {
    if (items_.size() == 1 && items_[0].is_star) {
      for (const auto& col : schema_->Columns()) {
        indices_.push_back(schema_->IndexOf(col.name).value());
        column_names_.push_back(col.name);
      }
      return;
    }
    for (const auto& item : items_) {
      auto idx = schema_->IndexOf(*item.column);
      if (!idx.has_value()) {
        error_ = util::Status::InvalidArgument("unknown column '" + *item.column + "' in SELECT list");
        return;
      }
      indices_.push_back(*idx);
      column_names_.push_back(item.alias.value_or(*item.column));
    }
  }

  std::unique_ptr<Operator> child_;
  std::vector<minidb::parser::SelectItem> items_;
  const minidb::common::Schema* schema_;
  std::vector<std::size_t> indices_;
  std::vector<std::string> column_names_;
  util::Status error_;
};

}  

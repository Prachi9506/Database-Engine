#pragma once

#include <algorithm>
#include <memory>

#include "executor/expr_eval.hpp"
#include "executor/operator.hpp"
#include "executor/value_compare.hpp"
#include "parser/ast.hpp"

namespace minidb::executor {

class SortOperator : public Operator {
 public:
  SortOperator(std::unique_ptr<Operator> child, minidb::parser::OrderByClause order_by,
               const minidb::common::Schema* schema)
      : child_(std::move(child)), order_by_(std::move(order_by)), schema_(schema) {}

  void Open() override {
    buffered_.clear();
    cursor_ = 0;
    error_ = util::Status::OK();

    auto idx = schema_->IndexOf(order_by_.column);
    if (!idx.has_value()) {
      error_ = util::Status::InvalidArgument("ORDER BY: unknown column '" + order_by_.column + "'");
      return;
    }

    child_->Open();
    while (auto tuple = child_->Next()) buffered_.push_back(std::move(*tuple));
    child_error_ = child_->GetError();
    child_->Close();

    std::size_t col = *idx;
    bool descending = order_by_.descending;
    std::stable_sort(buffered_.begin(), buffered_.end(), [&](const Tuple& a, const Tuple& b) {

      bool a_null = minidb::common::IsNull(a.values[col]);
      bool b_null = minidb::common::IsNull(b.values[col]);
      if (a_null != b_null) return descending ? !a_null : a_null;
      if (a_null && b_null) return false;

      auto cmp = CompareForSort(a.values[col], b.values[col]);
      return descending ? cmp > 0 : cmp < 0;
    });
  }

  std::optional<Tuple> Next() override {
    if (!error_.ok()) return std::nullopt;
    if (cursor_ >= buffered_.size()) return std::nullopt;
    return buffered_[cursor_++];
  }

  void Close() override { buffered_.clear(); }

  util::Status GetError() const override { return error_.ok() ? child_error_ : error_; }

 private:
  std::unique_ptr<Operator> child_;
  minidb::parser::OrderByClause order_by_;
  const minidb::common::Schema* schema_;
  std::vector<Tuple> buffered_;
  std::size_t cursor_ = 0;
  util::Status error_;
  util::Status child_error_;
};

}  

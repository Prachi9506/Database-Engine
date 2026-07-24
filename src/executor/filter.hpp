#pragma once

#include <memory>

#include "executor/expr_eval.hpp"
#include "executor/operator.hpp"
#include "parser/ast.hpp"

namespace minidb::executor {

class FilterOperator : public Operator {
 public:
  FilterOperator(std::unique_ptr<Operator> child, const minidb::parser::Expr* predicate,
                  const minidb::common::Schema* schema)
      : child_(std::move(child)), predicate_(predicate), schema_(schema) {}

  void Open() override {
    child_->Open();
    error_ = util::Status::OK();
  }

  std::optional<Tuple> Next() override {
    while (true) {
      auto tuple = child_->Next();
      if (!tuple.has_value()) return std::nullopt;
      auto matched = EvaluatePredicate(predicate_, *schema_, tuple->values);
      if (!matched.ok()) {
        error_ = matched.status();
        return std::nullopt;
      }
      if (matched.value()) return tuple;
    }
  }

  void Close() override { child_->Close(); }

  util::Status GetError() const override { return error_.ok() ? child_->GetError() : error_; }

 private:
  std::unique_ptr<Operator> child_;
  const minidb::parser::Expr* predicate_;
  const minidb::common::Schema* schema_;
  util::Status error_;
};

}  

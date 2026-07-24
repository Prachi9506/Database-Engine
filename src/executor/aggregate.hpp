#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "executor/operator.hpp"
#include "executor/value_compare.hpp"
#include "parser/ast.hpp"

namespace minidb::executor {

class AggregateOperator : public Operator {
 public:
  AggregateOperator(std::unique_ptr<Operator> child, std::vector<minidb::parser::SelectItem> items,
                     const minidb::common::Schema* schema)
      : child_(std::move(child)), items_(std::move(items)), schema_(schema) {
    BuildColumnNames();
  }

  void Open() override {
    child_->Open();
    done_ = false;
    error_ = util::Status::OK();
  }

  std::optional<Tuple> Next() override {
    if (done_) return std::nullopt;
    done_ = true;
    Compute();
    if (!error_.ok()) return std::nullopt;
    Tuple out;
    out.values = result_values_;
    return out;
  }

  void Close() override { child_->Close(); }

  const std::vector<std::string>& ColumnNames() const { return column_names_; }
  util::Status GetError() const override { return error_.ok() ? child_->GetError() : error_; }

 private:
  struct Accumulator {
    std::int64_t count = 0;
    double sum = 0.0;
    bool has_value = false;
    std::optional<minidb::common::Value> min_v;
    std::optional<minidb::common::Value> max_v;
  };

  static const char* FuncName(minidb::parser::AggregateFunc f) {
    using minidb::parser::AggregateFunc;
    switch (f) {
      case AggregateFunc::kCount: return "COUNT";
      case AggregateFunc::kSum: return "SUM";
      case AggregateFunc::kAvg: return "AVG";
      case AggregateFunc::kMin: return "MIN";
      case AggregateFunc::kMax: return "MAX";
    }
    return "?";
  }

  void BuildColumnNames() {
    for (const auto& item : items_) {
      std::string arg = item.aggregate_star ? "*" : item.column.value_or("?");
      std::string default_name = std::string(FuncName(*item.aggregate)) + "(" + arg + ")";
      column_names_.push_back(item.alias.value_or(default_name));
    }
  }

  void Compute() {
    using minidb::common::IsNull;
    using minidb::common::Value;
    using minidb::parser::AggregateFunc;


    for (const auto& item : items_) {
      if ((*item.aggregate == AggregateFunc::kSum || *item.aggregate == AggregateFunc::kAvg) &&
          !item.aggregate_star) {
        auto idx = schema_->IndexOf(*item.column);
        if (!idx.has_value()) {
          error_ = util::Status::InvalidArgument("unknown column '" + *item.column + "' in aggregate");
          return;
        }
        auto type = schema_->At(*idx).type;
        using minidb::common::ColumnType;
        if (type != ColumnType::kInt && type != ColumnType::kBigInt && type != ColumnType::kDouble) {
          error_ = util::Status::InvalidArgument("SUM/AVG requires a numeric column, got " + *item.column);
          return;
        }
      }
    }
    if (!error_.ok()) return;

    std::vector<Accumulator> accs(items_.size());
    while (auto tuple = child_->Next()) {
      for (std::size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        Accumulator& acc = accs[i];

        if (*item.aggregate == AggregateFunc::kCount) {
          if (item.aggregate_star) {
            ++acc.count;
          } else {
            auto idx = schema_->IndexOf(*item.column);
            if (!idx.has_value()) {
              error_ = util::Status::InvalidArgument("unknown column '" + *item.column + "' in COUNT");
              return;
            }
            if (!IsNull(tuple->values[*idx])) ++acc.count;
          }
          continue;
        }

        auto idx = schema_->IndexOf(*item.column);
        if (!idx.has_value()) {
          error_ = util::Status::InvalidArgument("unknown column '" + *item.column + "' in aggregate");
          return;
        }
        const Value& v = tuple->values[*idx];
        if (IsNull(v)) continue;

        switch (*item.aggregate) {
          case AggregateFunc::kSum:
          case AggregateFunc::kAvg: {
            auto d = ValueToDoubleOrNull(v);
            if (!d.has_value()) {
              error_ = util::Status::InvalidArgument("non-numeric value encountered in SUM/AVG");
              return;
            }
            acc.sum += *d;
            ++acc.count;
            acc.has_value = true;
            break;
          }
          case AggregateFunc::kMin:
            if (!acc.min_v.has_value() || CompareForSort(v, *acc.min_v) < 0) acc.min_v = v;
            acc.has_value = true;
            break;
          case AggregateFunc::kMax:
            if (!acc.max_v.has_value() || CompareForSort(v, *acc.max_v) > 0) acc.max_v = v;
            acc.has_value = true;
            break;
          default:
            break;
        }
      }
    }

    for (std::size_t i = 0; i < items_.size(); ++i) {
      const auto& item = items_[i];
      const Accumulator& acc = accs[i];
      switch (*item.aggregate) {
        case AggregateFunc::kCount:
          result_values_.emplace_back(Value(acc.count));
          break;
        case AggregateFunc::kSum:
          result_values_.emplace_back(acc.has_value ? Value(acc.sum) : Value(std::monostate{}));
          break;
        case AggregateFunc::kAvg:
          result_values_.emplace_back(acc.has_value ? Value(acc.sum / static_cast<double>(acc.count))
                                                      : Value(std::monostate{}));
          break;
        case AggregateFunc::kMin:
          result_values_.emplace_back(acc.min_v.value_or(Value(std::monostate{})));
          break;
        case AggregateFunc::kMax:
          result_values_.emplace_back(acc.max_v.value_or(Value(std::monostate{})));
          break;
      }
    }
  }

  std::unique_ptr<Operator> child_;
  std::vector<minidb::parser::SelectItem> items_;
  const minidb::common::Schema* schema_;
  std::vector<std::string> column_names_;
  std::vector<minidb::common::Value> result_values_;
  bool done_ = false;
  util::Status error_;
};

}

#pragma once

#include <cstdint>
#include <memory>

#include "executor/operator.hpp"

namespace minidb::executor {

class LimitOperator : public Operator {
 public:
  LimitOperator(std::unique_ptr<Operator> child, std::int64_t limit) : child_(std::move(child)), limit_(limit) {}

  void Open() override {
    child_->Open();
    returned_ = 0;
  }

  std::optional<Tuple> Next() override {
    if (returned_ >= limit_) return std::nullopt;
    auto tuple = child_->Next();
    if (!tuple.has_value()) return std::nullopt;
    ++returned_;
    return tuple;
  }

  void Close() override { child_->Close(); }

  util::Status GetError() const override { return child_->GetError(); }

 private:
  std::unique_ptr<Operator> child_;
  std::int64_t limit_;
  std::int64_t returned_ = 0;
};

}  

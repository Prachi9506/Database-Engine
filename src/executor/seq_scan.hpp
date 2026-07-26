#pragma once

#include <string>

#include "executor/operator.hpp"
#include "storage/table_manager.hpp"

namespace minidb::executor {

class SeqScanOperator : public Operator {
 public:
  SeqScanOperator(minidb::storage::TableManager* table_manager, std::string table_name)
      : table_manager_(table_manager), table_name_(std::move(table_name)) {}

  void Open() override {
    buffered_.clear();
    cursor_ = 0;
    table_manager_->Scan(table_name_, [&](minidb::storage::RecordId rid,
                                           const std::vector<minidb::common::Value>& values) {
      buffered_.push_back(Tuple{rid, values});
    });
  }

  std::optional<Tuple> Next() override {
    if (cursor_ >= buffered_.size()) return std::nullopt;
    return buffered_[cursor_++];
  }

  void Close() override { buffered_.clear(); }

 private:
  minidb::storage::TableManager* table_manager_;
  std::string table_name_;
  std::vector<Tuple> buffered_;
  std::size_t cursor_ = 0;
};

}  

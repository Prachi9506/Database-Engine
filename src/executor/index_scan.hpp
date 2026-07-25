#pragma once

#include <memory>
#include <optional>
#include <string>

#include "executor/operator.hpp"
#include "index/bplus_tree.hpp"
#include "storage/table_manager.hpp"

namespace minidb::executor {

class IndexScanOperator : public Operator {
 public:
  IndexScanOperator(minidb::storage::TableManager* table_manager, std::string table_name,
                     minidb::index::BPlusTree tree, minidb::index::Key equal_key)
      : table_manager_(table_manager),
        table_name_(std::move(table_name)),
        tree_(tree),
        low_(equal_key),
        low_inclusive_(true),
        high_(equal_key),
        high_inclusive_(true),
        is_equality_(true) {}

  IndexScanOperator(minidb::storage::TableManager* table_manager, std::string table_name,
                     minidb::index::BPlusTree tree, std::optional<minidb::index::Key> low, bool low_inclusive,
                     std::optional<minidb::index::Key> high, bool high_inclusive)
      : table_manager_(table_manager),
        table_name_(std::move(table_name)),
        tree_(tree),
        low_(low),
        low_inclusive_(low_inclusive),
        high_(high),
        high_inclusive_(high_inclusive),
        is_equality_(false) {}

  void Open() override {
    cursor_ = 0;
    candidates_.clear();
    error_ = util::Status::OK();

    if (is_equality_) {
      auto result = tree_.SearchEqual(*low_);
      if (!result.ok()) { error_ = result.status(); return; }
      candidates_ = std::move(result.value());
    } else {
      auto result = tree_.RangeScan(low_, low_inclusive_, high_, high_inclusive_);
      if (!result.ok()) { error_ = result.status(); return; }
      candidates_.reserve(result.value().size());
      for (auto& [key, rid] : result.value()) candidates_.push_back(rid);
    }
  }

  std::optional<Tuple> Next() override {
    while (cursor_ < candidates_.size()) {
      minidb::storage::RecordId rid = candidates_[cursor_++];
      auto row = table_manager_->GetRow(table_name_, rid);
      if (row.ok()) return Tuple{rid, row.value()};
      
    }
    return std::nullopt;
  }

  void Close() override { candidates_.clear(); }

  util::Status GetError() const override { return error_; }

 private:
  minidb::storage::TableManager* table_manager_;
  std::string table_name_;
  minidb::index::BPlusTree tree_;
  std::optional<minidb::index::Key> low_;
  bool low_inclusive_;
  std::optional<minidb::index::Key> high_;
  bool high_inclusive_;
  bool is_equality_;

  std::vector<minidb::storage::RecordId> candidates_;
  std::size_t cursor_ = 0;
  util::Status error_;
};

}  

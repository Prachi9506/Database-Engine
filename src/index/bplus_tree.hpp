#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "buffer/buffer_pool.hpp"
#include "index/bplus_node.hpp"
#include "utilities/status.hpp"

namespace minidb::index {

class BPlusTree {
 public:

  BPlusTree(minidb::buffer::BufferPool* buffer_pool, minidb::storage::page_id_t meta_page_id);

  minidb::storage::page_id_t MetaPageId() const { return meta_page_id_; }

  util::Status Insert(Key key, minidb::storage::RecordId rid);


  util::Status Delete(Key key, minidb::storage::RecordId rid);

  util::Result<std::vector<minidb::storage::RecordId>> SearchEqual(Key key) const;

  util::Result<std::vector<std::pair<Key, minidb::storage::RecordId>>> RangeScan(
      std::optional<Key> low, bool low_inclusive, std::optional<Key> high, bool high_inclusive) const;

 private:
  minidb::storage::page_id_t GetRootPageId() const;
  void SetRootPageId(minidb::storage::page_id_t new_root);

  static std::size_t FindChildIndex(const BPlusNodeView& node, Key key);

  minidb::storage::page_id_t FindLeafForKey(Key key) const;


  minidb::storage::page_id_t SplitChild(minidb::storage::page_id_t parent_id, std::size_t child_index);

  void InsertNonFull(minidb::storage::page_id_t node_id, Key key, minidb::storage::RecordId rid);

  minidb::buffer::BufferPool* buffer_pool_;
  minidb::storage::page_id_t meta_page_id_;
};

}  

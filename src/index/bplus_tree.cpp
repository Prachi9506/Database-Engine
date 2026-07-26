#include "index/bplus_tree.hpp"

#include "utilities/bytes.hpp"

namespace minidb::index {

using minidb::storage::kInvalidPageId;
using minidb::storage::Page;
using minidb::storage::page_id_t;
using minidb::storage::RecordId;

BPlusTree::BPlusTree(minidb::buffer::BufferPool* buffer_pool, page_id_t meta_page_id)
    : buffer_pool_(buffer_pool), meta_page_id_(meta_page_id) {
  if (meta_page_id_ == kInvalidPageId) {
    page_id_t new_meta_id;
    Page* meta_page = buffer_pool_->NewPage(&new_meta_id);

    page_id_t new_root_id;
    Page* root_page = buffer_pool_->NewPage(&new_root_id);
    BPlusNodeView root_view(root_page);
    root_view.InitLeaf(kInvalidPageId);
    buffer_pool_->UnpinPage(new_root_id, true);

    util::WriteFixed<page_id_t>(meta_page->Data(), new_root_id);
    buffer_pool_->UnpinPage(new_meta_id, true);

    meta_page_id_ = new_meta_id;
  }
}

page_id_t BPlusTree::GetRootPageId() const {
  Page* meta = buffer_pool_->FetchPage(meta_page_id_);
  page_id_t root = util::ReadFixed<page_id_t>(meta->Data());
  buffer_pool_->UnpinPage(meta_page_id_, false);
  return root;
}

void BPlusTree::SetRootPageId(page_id_t new_root) {
  Page* meta = buffer_pool_->FetchPage(meta_page_id_);
  util::WriteFixed<page_id_t>(meta->Data(), new_root);
  buffer_pool_->UnpinPage(meta_page_id_, true);
}

std::size_t BPlusTree::FindChildIndex(const BPlusNodeView& node, Key key) {
  std::size_t n = node.KeyCount();
  std::size_t i = 0;
  while (i < n && node.InternalKeyAt(i) <= key) ++i;
  return i;
}

page_id_t BPlusTree::FindLeafForKey(Key key) const {
  page_id_t current = GetRootPageId();
  while (true) {
    Page* page = buffer_pool_->FetchPage(current);
    BPlusNodeView view(page);
    if (view.IsLeaf()) {
      buffer_pool_->UnpinPage(current, false);
      return current;
    }
    std::size_t idx = FindChildIndex(view, key);
    page_id_t child = view.InternalChildAt(idx);
    buffer_pool_->UnpinPage(current, false);
    current = child;
  }
}

page_id_t BPlusTree::SplitChild(page_id_t parent_id, std::size_t child_index) {
  Page* parent_page = buffer_pool_->FetchPage(parent_id);
  BPlusNodeView parent(parent_page);
  page_id_t child_id = parent.InternalChildAt(child_index);

  Page* child_page = buffer_pool_->FetchPage(child_id);
  BPlusNodeView child(child_page);

  page_id_t new_right_id;
  Page* new_right_page = buffer_pool_->NewPage(&new_right_id);
  BPlusNodeView new_right(new_right_page);

  Key separator;
  if (child.IsLeaf()) {
    new_right.InitLeaf(child.NextLeafId());
    child.SetNextLeafId(new_right_id);

    std::uint16_t n = child.KeyCount();
    std::size_t mid = n / 2;

    std::size_t left = mid, right = mid;
    while (left > 1 || right < n) {
      if (right < n && child.LeafKeyAt(right) != child.LeafKeyAt(right - 1)) {
        mid = right;
        break;
      }
      if (left > 1 && child.LeafKeyAt(left - 1) != child.LeafKeyAt(left - 2)) {
        mid = left - 1;
        break;
      }
      if (right < n) ++right;
      if (left > 1) --left;
    }
    std::size_t right_count = n - mid;
    for (std::size_t j = 0; j < right_count; ++j) {
      new_right.SetLeafEntry(j, child.LeafKeyAt(mid + j), child.LeafRidAt(mid + j));
    }
    new_right.SetKeyCount(static_cast<std::uint16_t>(right_count));
    child.SetKeyCount(static_cast<std::uint16_t>(mid));
    separator = new_right.LeafKeyAt(0);  
  } else {
    new_right.InitInternal();
    std::uint16_t n = child.KeyCount();
    std::size_t mid = n / 2;
    separator = child.InternalKeyAt(mid);  

    std::size_t right_count = n - mid - 1;
    for (std::size_t j = 0; j < right_count; ++j) new_right.SetInternalKeyAt(j, child.InternalKeyAt(mid + 1 + j));
    for (std::size_t j = 0; j <= right_count; ++j) new_right.SetInternalChildAt(j, child.InternalChildAt(mid + 1 + j));
    new_right.SetKeyCount(static_cast<std::uint16_t>(right_count));
    child.SetKeyCount(static_cast<std::uint16_t>(mid));
  }

  parent.InsertInternalKeyChildAt(child_index, separator, new_right_id);

  buffer_pool_->UnpinPage(child_id, true);
  buffer_pool_->UnpinPage(new_right_id, true);
  buffer_pool_->UnpinPage(parent_id, true);
  return new_right_id;
}

void BPlusTree::InsertNonFull(page_id_t node_id, Key key, RecordId rid) {
  Page* page = buffer_pool_->FetchPage(node_id);
  BPlusNodeView node(page);

  if (node.IsLeaf()) {
    std::uint16_t n = node.KeyCount();
    std::size_t i = 0;
    while (i < n && node.LeafKeyAt(i) < key) ++i;
    node.InsertLeafEntryAt(i, key, rid);
    buffer_pool_->UnpinPage(node_id, true);
    return;
  }

  std::size_t idx = FindChildIndex(node, key);
  page_id_t child_id = node.InternalChildAt(idx);
  buffer_pool_->UnpinPage(node_id, false);

  Page* child_page = buffer_pool_->FetchPage(child_id);
  BPlusNodeView child(child_page);
  bool child_full = child.IsLeaf() ? (child.KeyCount() >= kLeafMaxKeys) : (child.KeyCount() >= kInternalMaxKeys);
  buffer_pool_->UnpinPage(child_id, false);

  if (child_full) {
    SplitChild(node_id, idx);
    Page* parent_page2 = buffer_pool_->FetchPage(node_id);
    BPlusNodeView parent2(parent_page2);
    std::size_t idx2 = FindChildIndex(parent2, key);
    child_id = parent2.InternalChildAt(idx2);
    buffer_pool_->UnpinPage(node_id, false);
  }

  InsertNonFull(child_id, key, rid);
}

util::Status BPlusTree::Insert(Key key, RecordId rid) {
  page_id_t root_id = GetRootPageId();
  Page* root_page = buffer_pool_->FetchPage(root_id);
  BPlusNodeView root(root_page);
  bool root_full = root.IsLeaf() ? (root.KeyCount() >= kLeafMaxKeys) : (root.KeyCount() >= kInternalMaxKeys);
  buffer_pool_->UnpinPage(root_id, false);

  if (root_full) {
    page_id_t new_root_id;
    Page* new_root_page = buffer_pool_->NewPage(&new_root_id);
    BPlusNodeView new_root(new_root_page);
    new_root.InitInternal();
    new_root.SetInternalChildAt(0, root_id);
    buffer_pool_->UnpinPage(new_root_id, true);

    SplitChild(new_root_id, 0);
    SetRootPageId(new_root_id);
    root_id = new_root_id;
  }

  InsertNonFull(root_id, key, rid);
  return util::Status::OK();
}

util::Status BPlusTree::Delete(Key key, RecordId rid) {
  page_id_t leaf_id = FindLeafForKey(key);
  while (leaf_id != kInvalidPageId) {
    Page* page = buffer_pool_->FetchPage(leaf_id);
    BPlusNodeView leaf(page);
    std::uint16_t n = leaf.KeyCount();
    bool exceeded = false;
    for (std::size_t i = 0; i < n; ++i) {
      Key k = leaf.LeafKeyAt(i);
      if (k > key) {
        exceeded = true;
        break;
      }
      if (k == key) {
        RecordId r = leaf.LeafRidAt(i);
        if (r.page_id == rid.page_id && r.slot_id == rid.slot_id) {
          leaf.RemoveLeafEntryAt(i);
          buffer_pool_->UnpinPage(leaf_id, true);
          return util::Status::OK();
        }
      }
    }
    page_id_t next = leaf.NextLeafId();
    buffer_pool_->UnpinPage(leaf_id, false);
    if (exceeded) break;
    leaf_id = next;
  }
  return util::Status::NotFound("index entry not found for given key/RecordId");
}

util::Result<std::vector<RecordId>> BPlusTree::SearchEqual(Key key) const {
  std::vector<RecordId> results;
  page_id_t leaf_id = FindLeafForKey(key);
  bool first = true;
  while (leaf_id != kInvalidPageId) {
    Page* page = buffer_pool_->FetchPage(leaf_id);
    BPlusNodeView leaf(page);
    std::uint16_t n = leaf.KeyCount();
    std::size_t i = 0;
    if (first) {
      while (i < n && leaf.LeafKeyAt(i) < key) ++i;
      first = false;
    }
    bool exceeded = false;
    for (; i < n; ++i) {
      Key k = leaf.LeafKeyAt(i);
      if (k == key) {
        results.push_back(leaf.LeafRidAt(i));
      } else if (k > key) {
        exceeded = true;
        break;
      }
    }
    page_id_t next = leaf.NextLeafId();
    buffer_pool_->UnpinPage(leaf_id, false);
    if (exceeded) break;
    leaf_id = next;
  }
  return util::Result<std::vector<RecordId>>(std::move(results));
}

util::Result<std::vector<std::pair<Key, RecordId>>> BPlusTree::RangeScan(std::optional<Key> low,
                                                                          bool low_inclusive,
                                                                          std::optional<Key> high,
                                                                          bool high_inclusive) const {
  std::vector<std::pair<Key, RecordId>> results;

  page_id_t leaf_id;
  if (low.has_value()) {
    leaf_id = FindLeafForKey(*low);
  } else {
    page_id_t current = GetRootPageId();
    while (true) {
      Page* page = buffer_pool_->FetchPage(current);
      BPlusNodeView view(page);
      if (view.IsLeaf()) {
        buffer_pool_->UnpinPage(current, false);
        break;
      }
      page_id_t child = view.InternalChildAt(0);
      buffer_pool_->UnpinPage(current, false);
      current = child;
    }
    leaf_id = current;
  }

  while (leaf_id != kInvalidPageId) {
    Page* page = buffer_pool_->FetchPage(leaf_id);
    BPlusNodeView leaf(page);
    std::uint16_t n = leaf.KeyCount();
    bool stop = false;
    for (std::size_t i = 0; i < n; ++i) {
      Key k = leaf.LeafKeyAt(i);
      if (low.has_value() && (k < *low || (!low_inclusive && k == *low))) continue;
      if (high.has_value() && (k > *high || (!high_inclusive && k == *high))) {
        stop = true;
        break;
      }
      results.emplace_back(k, leaf.LeafRidAt(i));
    }
    page_id_t next = leaf.NextLeafId();
    buffer_pool_->UnpinPage(leaf_id, false);
    if (stop) break;
    leaf_id = next;
  }

  return util::Result<std::vector<std::pair<Key, RecordId>>>(std::move(results));
}

}  

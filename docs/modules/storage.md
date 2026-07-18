## Design

```
HeapFile            (table = chain of pages, linked via next_page_id)
   │ uses
   ▼
BufferPool          (page_id -> in-RAM frame, LRU eviction, pin/unpin)
   │ uses (on cache miss)
   ▼
PageManager         (page_id -> byte offset in data.db, raw read()/write())
   │ operates on
   ▼
Page                (dumb fixed 4096-byte buffer)
SlottedPage          (interprets a Page's bytes as: header + slot directory + records)
```


## Algorithms

- **Slot allocation**: bump-pointer from the high end of the page,
  directory grows from the low end, tombstoned slots reused before growing
  the directory. O(1) insert/get/delete.
- **Heap file insert**: last-successful page first, walk forward
  through the chain on failure, allocate+link a new page only as a last
  resort. O(1) amortized.
- **Buffer pool eviction**: LRU via intrusive doubly-linked list + hashmap.
  O(1) touch/evict.

## Complexity

| Operation | Time | Notes |
|---|---|---|
| SlottedPage insert/get/delete | O(1) | bounded by fixed page size |
| HeapFile insert | O(1) amortized | occasional new-page allocation |
| HeapFile get-by-RecordId | O(1) | one page fetch |
| HeapFile full scan | O(n) | n = total live+dead slots across all pages |
| BufferPool fetch (hit) | O(1) | hashmap lookup |
| BufferPool fetch (miss) | O(1) + 1 disk I/O | unavoidable |
| PageManager read/write | O(1) | direct offset seek |


## Flow: inserting a row

```
HeapFile::InsertRecord(bytes)
  -> BufferPool::FetchPage(last_insert_page_id_)
       -> [cache hit] return frame.page
       -> [cache miss] PageManager::ReadPage() -> populate a frame -> return
  -> SlottedPage(page).InsertRecord(bytes)
       -> [fits] write into free region, add/reuse slot, return slot_id
       -> [doesn't fit] BufferPool::UnpinPage(); try next page in chain;
          if chain exhausted: BufferPool::NewPage() -> PageManager::AllocatePage()
  -> BufferPool::UnpinPage(page_id, dirty=true)
  -> return RecordId{page_id, slot_id}
```


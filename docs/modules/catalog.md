## Design

```
Database
  ├── catalog.db  → PageManager → BufferPool → CatalogManager
  │                                                 │ (heap file of encoded TableMetadata rows)
  └── data.db     → PageManager → BufferPool → TableManager
                                                    │ (one HeapFile + RecordManager per open table)
                                                    ▼
                                              RecordManager ──uses──▶ RecordCodec ──uses──▶ Schema/Value (common/)
```

## Algorithms

- **Record encoding**: positional, schema-order encoding with a leading
  null-bitmap (`ceil(ncols/8)` bytes) so NULL columns cost zero payload
  bytes. O(#columns + variable payload size).
- **Catalog load-at-startup**: a single linear scan of the catalog heap
  file via `HeapFile::Scan`, decoding each row into a `TableMetadata` and
  rebuilding the in-memory `unordered_map<name, TableMetadata>`. O(#tables).
- **Table lookup**: O(1) via the in-memory map once loaded; `TableManager`
  additionally caches an open `HeapFile`+`RecordManager` per table name so
  repeated operations reuse the heap file's "last insert page" cache from
  Phase 1 instead of re-deriving it every call.

## Complexity

| Operation | Time | Notes |
|---|---|---|
| RecordCodec::Encode/Decode | O(#columns + payload) | no search, purely positional |
| CatalogManager::CreateTable | O(1) map insert + O(1) heap insert | plus one encode pass over columns |
| CatalogManager::GetTable | O(1) | hashmap lookup |
| CatalogManager startup load | O(#tables) | one-time linear scan |
| TableManager::InsertRow (cold) | O(1) catalog lookup + O(1) heap insert | "cold" = table not yet opened this session |
| TableManager::InsertRow (warm) | O(1) heap insert | cached HeapFile/RecordManager |

Memory: `O(#tables)` for the catalog's in-memory map (one `TableMetadata`,
which itself is `O(#columns)`, per table) plus `O(#currently-open tables)`
for `TableManager`'s cache.

## Flow: `CREATE TABLE users (...)` then `INSERT`

```
TableManager::CreateTable("users", schema)
  -> HeapFile(data_buffer_pool_, kInvalidPageId)      # allocates page 1 of data.db
  -> CatalogManager::CreateTable("users", schema, first_page_id=1)
       -> encode TableMetadata -> catalog_heap_.InsertRecord(bytes)
       -> tables_["users"] = metadata                  # in-memory map updated
  -> TableManager caches HeapFile + RecordManager for "users"

TableManager::InsertRow("users", {1, "alice", 30})
  -> GetOrOpen("users")                                 # cache hit after CreateTable
  -> RecordCodec::Encode(schema, values) -> bytes
  -> HeapFile::InsertRecord(bytes) -> RecordId
```



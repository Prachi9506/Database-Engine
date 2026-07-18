## Design

```
CREATE INDEX idx_age ON users (age)
  -> validate column type (INT/BIGINT only, see below)
  -> build a fresh BPlusTree, scan the table once, insert (age, RecordId)
     for every row
  -> persist IndexMetadata { index_name, table_name, column_name,
                              meta_page_id } via CatalogManager

SELECT ... FROM users WHERE age = 30
  -> optimizer inspects the WHERE clause (see "Optimizer rule" below)
  -> finds an indexed equality conjunct on `age`
  -> builds IndexScanOperator instead of SeqScanOperator
  -> still wraps the WHOLE original WHERE clause in a Filter on top

INSERT/UPDATE/DELETE on users
  -> after the row-level storage operation succeeds, MaintainIndexesOn*
     walks every index on that table and updates it to match
```

### B+Tree storage architecture

```
   index.db  (a THIRD file, alongside catalog.db and data.db)
      │
      ▼
   PageManager + BufferPool  
      │
      ▼
   BPlusTree
      ├── meta page (permanent, allocated once)  --  holds current root page id
      └── root page  --  changes across the tree's lifetime as it splits
                │
        ┌───────┴────────┐
     internal          internal        (routing only, no data)
        │                  │
      leaf ←──────────→  leaf ←──────────→ leaf   (linked for range scans)
```


## Algorithms

- **Insertion**: preemptive top-down splitting (the CLRS B-Tree
  algorithm) — see `bplus_tree.hpp` for the full write-up.
- **Search/range scan**: root-to-leaf routing descent, then linear
  in-leaf scan, following leaf links for ranges or duplicate keys that
  span more than one leaf.
- **Optimizer (index selection)**: a simple, fully documented rule —
  flatten the WHERE clause's top-level AND-chain (an OR anywhere at the
  top disqualifies the optimization entirely), look for a conjunct of the
  shape `indexed_column OP integer_literal`, and prefer an equality
  conjunct over a range one. First usable match wins — **no cost-based
  comparison among multiple candidate indexes.**

## Complexity

| Operation | Time |
|---|---|
| B+Tree search / insert / delete | O(log_B n) page I/Os, B ≈ 290–340 |
| B+Tree range scan of k results | O(log_B n + k/B) |
| CREATE INDEX on an n-row table | O(n log_B n) — one scan, one insert per row |
| Index maintenance per INSERT/UPDATE/DELETE | O(log_B n) per index on the table |
| Optimizer's conjunct search | O(WHERE clause size) — negligible vs. the query itself |


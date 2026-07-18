## Design

```
ExecuteSelect(SelectStmt)
  chain = SeqScan(table)
  if WHERE:        chain = Filter(chain, predicate)
  if aggregate:     chain = Aggregate(chain, items)          --> done, 1 row
  else:
    if ORDER BY:    chain = Sort(chain, order_by)             (operates on FULL schema row)
    chain = Project(chain, items)                              (reshape to SELECT list)
    if LIMIT:       chain = Limit(chain, n)
  chain->Open(); while (chain->Next()) collect; chain->Close()
```

## Algorithms

- **Expression evaluation** (`expr_eval.cpp`): recursive tree-walking
  interpreter over the `Expr` AST, the same technique as PostgreSQL's
  `ExecQual` (before its optional JIT compilation).
- **Sort**: `std::stable_sort`, O(n log n), in-memory only.
- **Aggregate**: single-pass streaming accumulation (no GROUP BY, so
  there's always exactly one implicit group — no hashing or sorting
  needed to group rows).
- **Filter**: linear predicate evaluation per tuple, O(1) per tuple.

## Complexity

| Operator | Time | Memory |
|---|---|---|
| SeqScan | O(n) to materialize on Open() | O(n) |
| Filter | O(1) amortized per Next() | O(1) |
| Project | O(#selected columns) per Next() | O(1) |
| Sort | O(n log n) | O(n) |
| Limit | O(1) per Next() | O(1) |
| Aggregate | O(n) single pass | O(1) (fixed accumulators) |





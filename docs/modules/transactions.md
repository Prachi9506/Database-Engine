## Design

```
Transaction            { txn_id, state, isolation_level, active_at_start }
TransactionManager      Begin() / Commit() / Abort() / GetTransaction()
                         -- hands out ids, tracks active + all-time history

LockManager             LockShared() / LockExclusive() / Unlock() / ReleaseAll()
                         -- row-level 2PL, non-blocking try-acquire 
                         DetectDeadlock() -- wait-for-graph cycle detection

VersionMeta             { xmin, xmax }  -- who created / superseded a row version
IsVisible(meta, reader, txn_mgr)  -- the MVCC visibility predicate
VersionedRecordStore    wraps HeapFile + RecordCodec with a 16-byte
                         [xmin][xmax] prefix per stored row, giving
                         Insert/Update/Delete "Version" + ScanVisible
```

### MVCC storage layout

```
Physical bytes of one heap-file record, under VersionedRecordStore:

  [xmin: 8 bytes][xmax: 8 bytes][RecordCodec-encoded row bytes...]
   ^ txn that created this        ^ ordinary Phase 2 encoding,
     version                        completely untouched
                  ^ txn that superseded it, or kInvalidTxnId (0) if still current
```


## Algorithms

- **2PL (Two-Phase Locking)**: acquire locks during the "growing phase,"
  release them all at once (`ReleaseAll`) at commit/abort -- the
  "shrinking phase" -- never acquiring a new lock after releasing any.
  This ordering discipline is what gives 2PL its serializability
  guarantee.
- **Deadlock detection**: build a wait-for graph (edge txn A -> txn B
  means "A is waiting on a lock B holds") and run DFS-based cycle
  detection with a three-color (white/gray/black) traversal, reporting
  the actual cycle found.
- **MVCC visibility**: see `mvcc.hpp`/`mvcc.cpp` for the full rule
  breakdown -- in short, a version is visible if its creator is committed
  and (for Snapshot Isolation) committed strictly before the reader's
  snapshot was taken, and is NOT hidden by a visible-to-the-reader
  deletion.

## Complexity

| Operation | Time |
|---|---|
| Begin/Commit/Abort | O(1) amortized (plus O(active txns) to copy the snapshot set) |
| LockShared/LockExclusive/Unlock | O(1) amortized |
| ReleaseAll | O(locks held by that txn) |
| DetectDeadlock | O(V + E) over the supplied wait-for graph |
| IsVisible | O(1) |
| VersionedRecordStore insert/update/delete | same as the underlying HeapFile operation, O(1) amortized |
| ScanVisible | O(n) -- visits every physical version, same as any full scan |


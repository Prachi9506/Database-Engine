## Design

```
Every durable change, BEFORE it's safe to consider permanent:

  WALManager::AppendUpdate(txn_id, page_id, before_image, after_image)
     -> serialize -> append to wal.log -> fsync (Flush(), called every append)
     -> returns an LSN (Log Sequence Number), strictly increasing

  WALManager::AppendCommit(txn_id) / AppendAbort(txn_id) / AppendCheckpoint()
     -> same append+fsync discipline

On restart:

  RecoveryManager::Recover()
     ANALYSIS  -- scan the whole log once: which txns committed? which
                  didn't? where's the last checkpoint?
     REDO      -- replay every update's AFTER-image, from the checkpoint
                  forward, unconditionally (committed or not)
     UNDO      -- replay every update's BEFORE-image, walking the WHOLE
                  log backward, for every txn that never committed
                  (covers both crash-abandoned "losers" AND explicitly
                  aborted transactions -- see the bug story below)
```


## Algorithms

- **WAL append**: sequential file write + `fsync`, O(1) per record.
- **Recovery ANALYSIS**: single linear scan of the log, O(n).
- **REDO**: linear scan from the checkpoint LSN forward, O(records since
  checkpoint).
- **UNDO**: linear scan of the WHOLE log backward, O(records belonging to
  non-committed transactions).

## Complexity

| Operation | Time |
|---|---|
| WAL append (any record type) | O(1) amortized -- one sequential write + fsync |
| WAL full read | O(log size) |
| Recovery analysis | O(log size) |
| Recovery REDO | O(records since last checkpoint) |
| Recovery UNDO | O(records belonging to non-committed transactions), whole log |


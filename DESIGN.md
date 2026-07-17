# MiniDB — Design Document

A miniature relational database engine in modern C++17/20, built the way a real
database is built: storage first, then query processing, then transactions,
then networking.
---


## 1. High-Level Architecture

```
                         ┌─────────────────────────┐
                         │      Interactive CLI      │
                         │   (or TCP client socket)  │
                         └────────────┬───────────────┘
                                      │  SQL text
                         ┌────────────▼───────────────┐
                         │      Network / Session       │
                         │  (TCP server, protocol,      │
                         │   connection + session state)│
                         └────────────┬───────────────┘
                                      │
                         ┌────────────▼───────────────┐
                         │           PARSER              │
                         │  Lexer → Tokenizer → Parser   │
                         │        → AST                  │
                         └────────────┬───────────────┘
                                      │ AST
                         ┌────────────▼───────────────┐
                         │        QUERY PLANNER          │
                         │  AST → Logical Plan           │
                         │  Logical Plan → Optimizer      │
                         │  → Physical Plan               │
                         └────────────┬───────────────┘
                                      │ Physical Plan (operator tree)
                         ┌────────────▼───────────────┐
                         │      EXECUTION ENGINE         │
                         │  Iterator-model operators:    │
                         │  SeqScan, IndexScan, Filter,   │
                         │  Project, Sort, Limit, Agg,     │
                         │  Insert/Update/Delete           │
                         └───────┬─────────────┬───────┘
                                  │             │
                     ┌────────────▼───┐   ┌─────▼────────────┐
                     │   CATALOG        │   │   TRANSACTION      │
                     │  (schemas,       │   │   MANAGER           │
                     │   table defs,    │   │  (locks, MVCC,       │
                     │   indexes)       │   │   WAL, commit/abort) │
                     └────────┬────────┘   └─────────┬───────────┘
                              │                        │
                         ┌────▼────────────────────────▼────┐
                         │            STORAGE ENGINE           │
                         │  Table/Record Manager               │
                         │  Index Manager (B+Tree, Hash)       │
                         │  Page Manager (heap files, slotted   │
                         │   pages, binary serialization)       │
                         └────────────────┬─────────────────┘
                                          │
                         ┌────────────────▼─────────────────┐
                         │           BUFFER MANAGER            │
                         │   Page cache + LRU eviction          │
                         │   Pin/unpin, dirty tracking           │
                         └────────────────┬─────────────────┘
                                          │
                         ┌────────────────▼─────────────────┐
                         │          DISK / FILE I/O            │
                         │  data.db  |  wal.log  |  catalog.db  │
                         └───────────────────────────────────┘
```

This mirrors the classic layered database architecture used by every serious
RDBMS: **client-facing layer → SQL processing layer → execution layer →
storage layer → disk**, with transaction/recovery services cutting across the
lower three layers. PostgreSQL calls these the same names almost exactly
(parser, planner/optimizer, executor, storage manager, buffer manager, WAL).

---

## 2. Module Dependency Diagram


```
cli/            → network/, parser/, executor/
network/        → parser/, executor/, transaction/
parser/         → ast/, utilities/
optimizer/      → ast/, catalog/, statistics
executor/       → optimizer/, storage/, catalog/, transaction/, index/
transaction/    → storage/ (WAL, locks reference pages/records), recovery/
recovery/       → storage/ (WAL replay writes pages)
index/          → storage/ (pages), buffer/
catalog/        → storage/ (catalog itself is stored as a special table)
storage/        → buffer/, utilities/
buffer/         → storage/page.hpp (POD page type only), utilities/
utilities/      → (nothing — leaf module)
```

## 3. Folder Structure

```
minidb/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── architecture/           
│   ├── modules/                
│   └── decisions/               
│
├── src/
│   ├── cli/
│   │   ├── repl.hpp/.cpp          
│   │   └── main.cpp
│   │
│   ├── utilities/
│   │   ├── status.hpp             
│   │   ├── logger.hpp/.cpp
│   │   ├── bytes.hpp            
│   │   └── config.hpp            
│   │
│   ├── parser/
│   │   ├── lexer.hpp/.cpp          
│   │   ├── tokenizer.hpp/.cpp      
│   │   ├── token.hpp               
│   │   ├── parser.hpp/.cpp         
│   │   └── ast.hpp                 
│   │
│   ├── catalog/
│   │   ├── schema.hpp              
│   │   ├── table_metadata.hpp     
│   │   └── catalog_manager.hpp/.cpp 
│   │
│   ├── storage/
│   │   ├── page.hpp                
│   │   ├── slotted_page.hpp/.cpp   
│   │   ├── page_manager.hpp/.cpp  
│   │   ├── heap_file.hpp/.cpp      
│   │   ├── record.hpp              
│   │   ├── record_manager.hpp/.cpp 
│   │   └── table_manager.hpp/.cpp  
│   │
│   ├── buffer/
│   │   ├── buffer_pool.hpp/.cpp    
│   │   ├── lru_replacer.hpp/.cpp   
│   │   └── frame.hpp               
│   │
│   ├── index/
│   │   ├── index.hpp               
│   │   ├── bplus_tree.hpp/.cpp     
│   │   ├── bplus_node.hpp          
│   │   └── hash_index.hpp/.cpp     
│   │
│   ├── planner/
│   │   ├── logical_plan.hpp        
│   │   ├── plan_builder.hpp/.cpp   
│   │   ├── optimizer.hpp/.cpp     
│   │   ├── physical_plan.hpp      
│   │   └── statistics.hpp/.cpp     
│   │
│   ├── executor/
│   │   ├── executor.hpp            
│   │   ├── seq_scan.hpp/.cpp
│   │   ├── index_scan.hpp/.cpp
│   │   ├── filter.hpp/.cpp
│   │   ├── project.hpp/.cpp
│   │   ├── sort.hpp/.cpp
│   │   ├── limit.hpp/.cpp
│   │   ├── aggregate.hpp/.cpp
│   │   ├── insert.hpp/.cpp
│   │   ├── update.hpp/.cpp
│   │   ├── delete.hpp/.cpp
│   │   └── executor_context.hpp    
│   │
│   ├── transaction/
│   │   ├── transaction.hpp        
│   │   ├── transaction_manager.hpp/.cpp
│   │   ├── lock_manager.hpp/.cpp   
│   │   └── mvcc.hpp/.cpp           
│   │
│   ├── recovery/
│   │   ├── wal_record.hpp          
│   │   ├── wal_manager.hpp/.cpp   
│   │   ├── checkpoint.hpp/.cpp
│   │   └── recovery_manager.hpp/.cpp 
│   │
│   └── network/
│       ├── tcp_server.hpp/.cpp
│       ├── connection.hpp/.cpp
│       ├── protocol.hpp            
│       └── session.hpp/.cpp       
│
├── tests/
│   ├── unit/                      
│   ├── integration/                
│   └── crash_recovery/             
│
├── benchmarks/
│   ├── micro/                   
│   └── macro/              
│
└── data/                          
    ├── catalog.db
    ├── data.db
    └── wal.log
```


---

## 4. Data Flow Diagram (one query, end to end)

```
"SELECT name FROM users WHERE age > 30 ORDER BY name LIMIT 10;"

  1. network/session.cpp     receives bytes, decodes protocol frame -> SQL string
  2. parser/lexer.cpp        SQL string -> char stream, strips whitespace/comments
  3. parser/tokenizer.cpp    char stream -> [SELECT][name][FROM][users][WHERE]...
  4. parser/parser.cpp       tokens -> AST (SelectStmt node with projection/where/order/limit)
  5. planner/plan_builder    AST -> Logical Plan:
                                Limit(10)
                                 -> Sort(name)
                                  -> Project(name)
                                   -> Filter(age > 30)
                                    -> Scan(users)
  6. planner/optimizer       consult catalog/statistics: does users.age have an index?
                                if yes -> rewrite Scan+Filter into IndexScan(users, age>30)
  7. planner/*  -> Physical Plan (concrete operators, chosen algorithms)
  8. executor/*               Volcano iterator model: Limit.Next() pulls from Sort.Next()
                                pulls from Project.Next() pulls from Filter.Next() pulls
                                from IndexScan.Next() -> record_manager -> buffer_pool
                                -> page_manager -> disk (on cache miss)
  9. transaction/mvcc         each tuple checked for visibility against the calling txn's snapshot
  10. network/session          result rows serialized back over the wire
```



## Storage Engine Architecture

```
 Table "users"  (logical)
        │
        ▼
 Heap File  = linked list of fixed-size Pages on disk
   Page 0 → Page 3 → Page 7 → Page 12 → NULL
        │
        ▼
 Each Page (4096 bytes) = Slotted Page layout:

  ┌────────────────────────────────────────────────────────┐
  │ Page Header (page_id, next_page_id, slot_count, free_ptr)│
  ├────────────────────────────────────────────────────────┤
  │ Slot Directory: [offset0,len0][offset1,len1]...          │→ grows right
  ├────────────────────────────────────────────────────────┤
  │                      free space                           │
  ├────────────────────────────────────────────────────────┤
  │ ...record2 bytes...           │
  │ ...record1 bytes...           │
  │ ...record0 bytes...            │← grows left (records packed from the end)
  └────────────────────────────────────────────────────────┘
```


## Transaction Architecture

```
   TransactionManager
        │
        ├── Begin() -> Transaction{ id, snapshot, isolation_level, state=Active }
        │
        ├── every read/write in the txn:
        │      LockManager.AcquireLock(txn, resource, mode)   [2PL, Strict 2PL]
        │      MVCC.CheckVisibility(tuple, txn.snapshot)
        │      WALManager.AppendRecord(txn, op)                [before the page is modified]
        │
        ├── Commit():
        │      WALManager.AppendRecord(COMMIT)
        │      WALManager.Flush()          <- WAL rule: log record hits disk before commit returns
        │      LockManager.ReleaseAll(txn)
        │
        └── Abort():
               undo using WAL records written so far (Phase-5 detail)
               LockManager.ReleaseAll(txn)
```

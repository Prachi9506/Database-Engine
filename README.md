# MiniDB

A miniature relational database engine built from scratch in modern
C++17/20 — storage engine, SQL parser, query executor, B+Tree indexes,
a rule-based optimizer, transactions with MVCC, write-ahead logging and
crash recovery, and a TCP server with an interactive CLI.

```
minidb> CREATE TABLE users (id INT NOT NULL, name VARCHAR(50), age INT);
CREATE TABLE
minidb> INSERT INTO users VALUES (1, 'alice', 30), (2, 'bob', 25);
INSERT 0 2
minidb> CREATE INDEX idx_age ON users (age);
CREATE INDEX
minidb> SELECT name FROM users WHERE age > 26 ORDER BY name;
 name
------
 alice
(1 row)  [IndexScan(idx_age)]
```



## Quick start

```sh
mkdir build 
cd build
cmake ..
make -j
```

This builds:
- `storage_tests` — the full test suite (179 tests)
- `minidb_cli` — the interactive REPL
- `minidb_server` — the TCP server
- `minidb_bench` — the benchmark suite

```sh
./storage_tests                        # run all tests
./minidb_cli ./my_database              # interactive SQL, data persisted under ./my_database
./minidb_server 5433 ./my_database &    # start a server on port 5433
./minidb_bench                          # real, measured performance numbers
```


## Architecture

```
CLI / TCP client
       │  SQL text
       ▼
Parser (lexer → tokenizer → recursive-descent parser → AST)
       ▼
Executor (optimizer: SeqScan vs IndexScan → Volcano operator pipeline)
       │
   ┌───┴────────────────┐
   ▼                    ▼
Catalog            Storage Engine
(tables/indexes,    (heap files, slotted pages,
 persisted as        buffer pool, B+Tree)
 heap-file rows)
       │
       ▼
   Disk (data.db, catalog.db, index.db)

[Transaction/Lock/MVCC and WAL/Recovery: real, tested, standalone --
 see the scope note above]
```


## Project structure

```
minidb/
├── DESIGN.md             
├── README.md               
├── CMakeLists.txt
├── src/
│   ├── utilities/          # Status/Result error handling, byte helpers, config
│   ├── common/              # Value/Schema -- shared beneath storage AND catalog
│   ├── storage/              # Page, SlottedPage, PageManager, HeapFile, RecordCodec
│   ├── buffer/                 # BufferPool, LRUReplacer
│   ├── catalog/                 # CatalogManager (tables + indexes, persisted as heap-file rows)
│   ├── parser/                    # Lexer, Tokenizer, AST, recursive-descent Parser
│   ├── index/                      # B+Tree (bplus_node, bplus_tree)
│   ├── executor/                     # Volcano operators, expression eval, the Executor itself
│   ├── transaction/                    # Transaction, TransactionManager, LockManager, MVCC
│   ├── recovery/                         # WAL, RecoveryManager (ARIES-lite)
│   ├── network/                            # wire protocol, TCPServer
│   ├── cli/                                  # interactive REPL
│   └── database.hpp                            # top-level Database instance, wires it all together
├── tests/unit/               # 179 tests, one file per module, gtest-shaped macros
├── benchmarks/
│   ├── micro/                # storage/index component benchmarks
│   ├── macro/                 # end-to-end SQL workload benchmarks
│   └── bench_main.cpp
└── docs/modules/              # one deep-dive .md per subsystem (see below)
```

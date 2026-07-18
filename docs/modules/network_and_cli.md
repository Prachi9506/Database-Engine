## Design

```
                     ┌─────────────────────┐
  human at terminal → │   cli::Repl          │ → Database + Executor (in-process)
                     └─────────────────────┘

                     ┌─────────────────────┐
  remote client     → │  network::TCPServer   │ → Database + Executor (shared, one process)
  (any language,       │  (thread-per-connection)│
   speaks the wire      └─────────────────────┘
   protocol below)
```

### Wire protocol

```
Request:   [4-byte length, network byte order][SQL text, UTF-8]
Response:  [1-byte status: 0=OK, 1=ERROR][4-byte length][payload text]
```

One message type each direction -- enough to prove a remote client can
drive the database, without building out PostgreSQL's full state machine
(startup packet, auth, extended query protocol with prepared statements,
row description vs. data row messages, ...).

### Concurrency model

Thread-per-connection: one accept thread blocks on `accept()`, and every
accepted connection gets its own `std::thread` running a simple
request/response loop until the client disconnects. Multiple connections
read and write their own sockets fully concurrently, but every call into
`Executor::Execute()` is serialized behind one mutex -- a deliberate,
documented simplification (see Trade-offs).

## Algorithms

- **Frame I/O**: `WriteAll`/`ReadAll` loops handling partial
  `read()`/`write()` (POSIX doesn't guarantee a single call transfers the
  full requested length) -- the standard technique every raw-socket
  protocol implementation needs.
- **REPL statement assembly**: a lightweight single-quote-aware scan (not
  a full tokenizer) tracks whether a `;` is inside a string literal, so
  `INSERT INTO t VALUES ('a;b')` isn't mistaken for two statements.

## Complexity

| Operation | Time |
|---|---|
| WriteFrame/ReadFrame | O(payload size) -- one or more `read`/`write` syscalls |
| Accepting a connection | O(1) |
| REPL multi-line assembly | O(input length) |


cmake_minimum_required(VERSION 3.16)
project(minidb CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(minidb_storage
  src/storage/page_manager.cpp
  src/storage/heap_file.cpp
  src/storage/record.cpp
  src/buffer/buffer_pool.cpp
  src/catalog/catalog_manager.cpp
  src/parser/tokenizer.cpp
  src/parser/parser.cpp
  src/index/bplus_tree.cpp
  src/executor/expr_eval.cpp
  src/executor/executor.cpp
  src/executor/result_formatter.cpp
  src/transaction/transaction_manager.cpp
  src/transaction/lock_manager.cpp
  src/transaction/mvcc.cpp
  src/recovery/wal_manager.cpp
  src/recovery/recovery_manager.cpp
  src/network/socket_compat.cpp
  src/network/protocol.cpp
  src/network/tcp_server.cpp
  src/cli/repl.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(minidb_storage Threads::Threads)

if(WIN32)
  target_link_libraries(minidb_storage ws2_32)
endif()

add_executable(minidb_cli src/cli/main.cpp)
target_link_libraries(minidb_cli minidb_storage)

add_executable(minidb_server src/network/server_main.cpp)
target_link_libraries(minidb_server minidb_storage)

add_executable(minidb_bench
  benchmarks/bench_main.cpp
  benchmarks/micro/bench_storage.cpp
  benchmarks/macro/bench_workload.cpp
)
target_include_directories(minidb_bench PRIVATE benchmarks)
target_link_libraries(minidb_bench minidb_storage)

enable_testing()
add_executable(storage_tests
  tests/unit/test_framework.hpp
  tests/unit/test_main.cpp
  tests/unit/test_slotted_page.cpp
  tests/unit/test_page_manager.cpp
  tests/unit/test_buffer_pool.cpp
  tests/unit/test_heap_file.cpp
  tests/unit/test_record_codec.cpp
  tests/unit/test_catalog_manager.cpp
  tests/unit/test_database.cpp
  tests/unit/test_tokenizer.cpp
  tests/unit/test_parser.cpp
  tests/unit/test_expr_eval.cpp
  tests/unit/test_executor.cpp
  tests/unit/test_bplus_tree.cpp
  tests/unit/test_index_integration.cpp
  tests/unit/test_transaction_manager.cpp
  tests/unit/test_lock_manager.cpp
  tests/unit/test_mvcc.cpp
  tests/unit/test_wal_manager.cpp
  tests/unit/test_recovery_manager.cpp
  tests/unit/test_protocol.cpp
  tests/unit/test_tcp_server.cpp
  tests/unit/test_repl.cpp
)
target_link_libraries(storage_tests minidb_storage)
add_test(NAME storage_tests COMMAND storage_tests)

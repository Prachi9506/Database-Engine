#include <csignal>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "database.hpp"
#include "network/tcp_server.hpp"

namespace {
volatile std::sig_atomic_t g_stop_requested = 0;
void HandleSignal(int) { g_stop_requested = 1; }
}  

int main(int argc, char** argv) {
  int port = (argc > 1) ? std::atoi(argv[1]) : 5433;
  std::string dir = (argc > 2) ? argv[2] : "./minidb_data";
  std::filesystem::create_directories(dir);

  minidb::Database db(dir);
  minidb::executor::Executor executor(&db);
  minidb::network::TCPServer server(&executor);

  auto status = server.Start(port);
  if (!status.ok()) {
    std::cerr << "Failed to start server: " << status.message() << "\n";
    return 1;
  }
  std::cout << "MiniDB server listening on port " << server.Port() << "\n";

  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);
  while (!g_stop_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::cout << "Shutting down...\n";
  server.Stop();
  db.FlushAll();
  return 0;
}

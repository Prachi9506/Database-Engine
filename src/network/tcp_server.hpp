#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "executor/executor.hpp"
#include "network/socket_compat.hpp"
#include "utilities/status.hpp"

namespace minidb::network {

class TCPServer {
 public:
  explicit TCPServer(minidb::executor::Executor* executor) : executor_(executor) {}
  ~TCPServer();

  TCPServer(const TCPServer&) = delete;
  TCPServer& operator=(const TCPServer&) = delete;


  util::Status Start(int port);


  void Stop();

  int Port() const { return port_; }

  bool IsRunning() const { return running_.load(); }

 private:
  void AcceptLoop();
  void HandleConnection(socket_t client_fd);

  minidb::executor::Executor* executor_;
  std::mutex exec_mutex_;  

  socket_t listen_fd_ = kInvalidSocket;
  int port_ = 0;
  std::atomic<bool> running_{false};
  std::thread accept_thread_;

  std::mutex threads_mutex_;
  std::vector<std::thread> connection_threads_;
};

}  

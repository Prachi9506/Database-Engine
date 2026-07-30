#include "network/tcp_server.hpp"

#include <cstring>

#include "executor/result_formatter.hpp"
#include "network/protocol.hpp"
#include "parser/parser.hpp"

namespace minidb::network {

namespace {

constexpr int kPollIntervalMs = 200;
}  

TCPServer::~TCPServer() { Stop(); }

util::Status TCPServer::Start(int port) {
  InitSocketLibrary();  

  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ == kInvalidSocket) return util::Status::IOError("TCPServer::Start: socket() failed");

  int opt = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    CloseSocketHandle(listen_fd_);
    listen_fd_ = kInvalidSocket;
    return util::Status::IOError("TCPServer::Start: bind() failed");
  }

  socklen_compat_t addr_len = sizeof(addr);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
    port_ = ntohs(addr.sin_port);
  } else {
    port_ = port;
  }

  if (::listen(listen_fd_, /*backlog=*/16) < 0) {
    CloseSocketHandle(listen_fd_);
    listen_fd_ = kInvalidSocket;
    return util::Status::IOError("TCPServer::Start: listen() failed");
  }

  running_ = true;
  accept_thread_ = std::thread(&TCPServer::AcceptLoop, this);
  return util::Status::OK();
}

void TCPServer::AcceptLoop() {
  while (running_.load()) {

    if (!WaitReadable(listen_fd_, kPollIntervalMs)) continue;

    sockaddr_in client_addr{};
    socklen_compat_t client_len = sizeof(client_addr);
    socket_t client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd == kInvalidSocket) continue;

    std::lock_guard<std::mutex> lock(threads_mutex_);
    connection_threads_.emplace_back(&TCPServer::HandleConnection, this, client_fd);
  }
}

void TCPServer::HandleConnection(socket_t client_fd) {
  while (running_.load()) {
    if (!WaitReadable(client_fd, kPollIntervalMs)) continue;

    auto request = ReadFrame(client_fd);
    if (!request.ok()) break;  

    auto stmt = minidb::parser::ParseSQL(request.value());
    if (!stmt.ok()) {
      WriteResponse(client_fd, /*ok=*/false, stmt.status().message());
      continue;
    }

    std::string response_text;
    {
      std::lock_guard<std::mutex> lock(exec_mutex_);  
      auto result = executor_->Execute(stmt.value());
      if (!result.ok()) {
        WriteResponse(client_fd, false, result.status().message());
        continue;
      }
      response_text = minidb::executor::FormatQueryResult(result.value());
    }
    WriteResponse(client_fd, true, response_text);
  }

  CloseSocketHandle(client_fd);
}

void TCPServer::Stop() {
  if (!running_.exchange(false)) return;


  if (accept_thread_.joinable()) accept_thread_.join();

  if (listen_fd_ != kInvalidSocket) {
    CloseSocketHandle(listen_fd_);
    listen_fd_ = kInvalidSocket;
  }

  std::lock_guard<std::mutex> lock(threads_mutex_);
  for (auto& t : connection_threads_) {
    if (t.joinable()) t.join();
  }
  connection_threads_.clear();
}

}  

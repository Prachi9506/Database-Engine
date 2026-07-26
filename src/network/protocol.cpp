#include "network/protocol.hpp"

#include <cstdint>
#include <cstring>

namespace minidb::network {

namespace {

bool WriteAll(socket_t sock, const char* data, std::size_t len) {
  std::size_t sent = 0;
  while (sent < len) {
    long n = SocketSend(sock, data + sent, len - sent);
    if (n <= 0) {
      if (WasInterrupted()) continue;
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool ReadAll(socket_t sock, char* data, std::size_t len) {
  std::size_t received = 0;
  while (received < len) {
    long n = SocketRecv(sock, data + received, len - received);
    if (n == 0) return false;  
    if (n < 0) {
      if (WasInterrupted()) continue;
      return false;
    }
    received += static_cast<std::size_t>(n);
  }
  return true;
}
}  

util::Status WriteFrame(socket_t sock, const std::string& payload) {
  std::uint32_t len_net = htonl(static_cast<std::uint32_t>(payload.size()));
  if (!WriteAll(sock, reinterpret_cast<const char*>(&len_net), sizeof(len_net))) {
    return util::Status::IOError("WriteFrame: failed to write length prefix");
  }
  if (!WriteAll(sock, payload.data(), payload.size())) {
    return util::Status::IOError("WriteFrame: failed to write payload");
  }
  return util::Status::OK();
}

util::Result<std::string> ReadFrame(socket_t sock) {
  std::uint32_t len_net = 0;
  if (!ReadAll(sock, reinterpret_cast<char*>(&len_net), sizeof(len_net))) {
    return util::Status::IOError("ReadFrame: connection closed or read failed");
  }
  std::uint32_t len = ntohl(len_net);
  if (len > kMaxFrameSize) {
    return util::Status::InvalidArgument("ReadFrame: advertised frame size exceeds maximum");
  }
  std::string payload(len, '\0');
  if (len > 0 && !ReadAll(sock, payload.data(), len)) {
    return util::Status::IOError("ReadFrame: connection closed mid-payload");
  }
  return util::Result<std::string>(std::move(payload));
}

util::Status WriteResponse(socket_t sock, bool ok, const std::string& payload) {
  char status_byte = ok ? 0 : 1;
  if (!WriteAll(sock, &status_byte, 1)) {
    return util::Status::IOError("WriteResponse: failed to write status byte");
  }
  return WriteFrame(sock, payload);
}

util::Result<std::pair<bool, std::string>> ReadResponse(socket_t sock) {
  char status_byte = 0;
  if (!ReadAll(sock, &status_byte, 1)) {
    return util::Status::IOError("ReadResponse: connection closed or read failed");
  }
  auto frame = ReadFrame(sock);
  if (!frame.ok()) return frame.status();
  return util::Result<std::pair<bool, std::string>>(std::make_pair(status_byte == 0, frame.value()));
}

}  

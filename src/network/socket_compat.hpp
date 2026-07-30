#pragma once

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7+
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstddef>

namespace minidb::network {

#ifdef _WIN32
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
using socklen_compat_t = int;
#else
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
using socklen_compat_t = socklen_t;
#endif

void InitSocketLibrary();

inline void CloseSocketHandle(socket_t s) {
#ifdef _WIN32
  ::closesocket(s);
#else
  ::close(s);
#endif
}

inline void ShutdownSocketHandle(socket_t s) {
#ifdef _WIN32
  ::shutdown(s, SD_BOTH);
#else
  ::shutdown(s, SHUT_RDWR);
#endif
}

inline long SocketSend(socket_t s, const char* buf, std::size_t len) {
#ifdef _WIN32
  return ::send(s, buf, static_cast<int>(len), 0);
#else
  return ::send(s, buf, len, 0);
#endif
}
inline long SocketRecv(socket_t s, char* buf, std::size_t len) {
#ifdef _WIN32
  return ::recv(s, buf, static_cast<int>(len), 0);
#else
  return ::recv(s, buf, len, 0);
#endif
}


inline bool WasInterrupted() {
#ifdef _WIN32
  return false;
#else
  return errno == EINTR;
#endif
}

inline bool WaitReadable(socket_t s, int timeout_ms) {
  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(s, &read_set);
  timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
#ifdef _WIN32
  int nfds = 0;  
#else
  int nfds = static_cast<int>(s) + 1;
#endif
  int rc = ::select(nfds, &read_set, nullptr, nullptr, &tv);
  return rc > 0;
}

}  

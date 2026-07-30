#include "network/socket_compat.hpp"

namespace minidb::network {

#ifdef _WIN32
namespace {

struct WinsockGuard {
  WinsockGuard() {
    WSADATA wsa_data;
    ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
  }
  ~WinsockGuard() { ::WSACleanup(); }
};
}  

void InitSocketLibrary() {
  static WinsockGuard guard;
  (void)guard;
}
#else
void InitSocketLibrary() {

}
#endif

}  

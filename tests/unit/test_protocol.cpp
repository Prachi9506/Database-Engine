#include "network/protocol.hpp"

#include <thread>

#include "network/socket_compat.hpp"
#include "test_framework.hpp"

using namespace minidb::network;

namespace {

struct SocketPair {
  socket_t a = kInvalidSocket, b = kInvalidSocket;

  SocketPair() {
    InitSocketLibrary();

    socket_t listener = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  
    ::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(listener, 1);

    socklen_compat_t addr_len = sizeof(addr);
    ::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &addr_len);

    a = ::socket(AF_INET, SOCK_STREAM, 0);
    ::connect(a, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    sockaddr_in peer_addr{};
    socklen_compat_t peer_len = sizeof(peer_addr);
    b = ::accept(listener, reinterpret_cast<sockaddr*>(&peer_addr), &peer_len);

    CloseSocketHandle(listener);
  }
  ~SocketPair() {
    CloseSocketHandle(a);
    CloseSocketHandle(b);
  }
};
}

TEST(Protocol, WriteFrameThenReadFrameRoundTrips) {
  SocketPair sp;
  EXPECT_TRUE(WriteFrame(sp.a, "SELECT * FROM users").ok());
  auto result = ReadFrame(sp.b);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "SELECT * FROM users");
}

TEST(Protocol, EmptyPayloadRoundTrips) {
  SocketPair sp;
  EXPECT_TRUE(WriteFrame(sp.a, "").ok());
  auto result = ReadFrame(sp.b);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST(Protocol, LargePayloadRoundTrips) {
  SocketPair sp;
  std::string big(500000, 'x');
  std::thread writer([&]() { WriteFrame(sp.a, big); });

  auto result = ReadFrame(sp.b);
  writer.join();

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value().size(), big.size());
  EXPECT_TRUE(result.value() == big);
}

TEST(Protocol, ResponseRoundTripsOkAndPayload) {
  SocketPair sp;
  EXPECT_TRUE(WriteResponse(sp.a, true, "some result text").ok());
  auto result = ReadResponse(sp.b);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().first);
  EXPECT_EQ(result.value().second, "some result text");
}

TEST(Protocol, ResponseRoundTripsErrorFlag) {
  SocketPair sp;
  EXPECT_TRUE(WriteResponse(sp.a, false, "syntax error near FOO").ok());
  auto result = ReadResponse(sp.b);
  EXPECT_TRUE(result.ok());
  EXPECT_FALSE(result.value().first);
  EXPECT_EQ(result.value().second, "syntax error near FOO");
}

TEST(Protocol, ReadFrameFailsOnClosedConnection) {
  SocketPair sp;
  CloseSocketHandle(sp.a);
  sp.a = kInvalidSocket;  
  auto result = ReadFrame(sp.b);
  EXPECT_FALSE(result.ok());
}

TEST(Protocol, MultipleFramesInSequence) {
  SocketPair sp;
  WriteFrame(sp.a, "first");
  WriteFrame(sp.a, "second");
  WriteFrame(sp.a, "third");

  auto r1 = ReadFrame(sp.b);
  auto r2 = ReadFrame(sp.b);
  auto r3 = ReadFrame(sp.b);
  EXPECT_TRUE(r1.ok() && r2.ok() && r3.ok());
  EXPECT_EQ(r1.value(), "first");
  EXPECT_EQ(r2.value(), "second");
  EXPECT_EQ(r3.value(), "third");
}

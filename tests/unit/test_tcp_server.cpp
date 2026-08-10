#include "network/tcp_server.hpp"

#include <filesystem>

#include "network/protocol.hpp"
#include "network/socket_compat.hpp"
#include "test_framework.hpp"

using namespace minidb;
using namespace minidb::network;

namespace {
std::string TempDir(const std::string& name) {
  std::string dir = TestTempRoot() + "minidb_test_tcp_" + name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

class TestClient {
 public:
  bool Connect(int port) {
    InitSocketLibrary();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    return ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
  }
  ~TestClient() {
    if (fd_ != kInvalidSocket) CloseSocketHandle(fd_);
  }

  util::Result<std::pair<bool, std::string>> Query(const std::string& sql) {
    auto write_status = WriteFrame(fd_, sql);
    if (!write_status.ok()) return write_status;
    return ReadResponse(fd_);
  }

  socket_t fd_ = kInvalidSocket;
};
}

TEST(TCPServer, StartAndStopCleanly) {
  std::string dir = TempDir("start_stop");
  {
    Database db(dir);
    executor::Executor exec(&db);
    TCPServer server(&exec);
    EXPECT_TRUE(server.Start(0).ok());  
    EXPECT_TRUE(server.Port() > 0);
    EXPECT_TRUE(server.IsRunning());
    server.Stop();
    EXPECT_FALSE(server.IsRunning());
  }
  std::filesystem::remove_all(dir);
}

TEST(TCPServer, ClientCanConnectAndRunDDLAndDML) {
  std::string dir = TempDir("basic");
  {
    Database db(dir);
    executor::Executor exec(&db);
    TCPServer server(&exec);
    EXPECT_TRUE(server.Start(0).ok());

    TestClient client;
    EXPECT_TRUE(client.Connect(server.Port()));

    auto create = client.Query("CREATE TABLE users (id INT, name VARCHAR(50))");
    EXPECT_TRUE(create.ok());
    EXPECT_TRUE(create.value().first);  

    auto insert = client.Query("INSERT INTO users VALUES (1, 'alice'), (2, 'bob')");
    EXPECT_TRUE(insert.ok());
    EXPECT_TRUE(insert.value().first);

    auto select = client.Query("SELECT name FROM users ORDER BY id");
    EXPECT_TRUE(select.ok());
    EXPECT_TRUE(select.value().first);
    EXPECT_TRUE(select.value().second.find("alice") != std::string::npos);
    EXPECT_TRUE(select.value().second.find("bob") != std::string::npos);

    server.Stop();
  }
  std::filesystem::remove_all(dir);
}

TEST(TCPServer, SyntaxErrorReturnsErrorResponseNotDisconnect) {
  std::string dir = TempDir("syntax_error");
  {
    Database db(dir);
    executor::Executor exec(&db);
    TCPServer server(&exec);
    server.Start(0);

    TestClient client;
    EXPECT_TRUE(client.Connect(server.Port()));

    auto bad = client.Query("SELEKT * FROM nowhere");
    EXPECT_TRUE(bad.ok());          
    EXPECT_FALSE(bad.value().first);  

    auto create = client.Query("CREATE TABLE t (id INT)");
    EXPECT_TRUE(create.ok());
    EXPECT_TRUE(create.value().first);

    server.Stop();
  }
  std::filesystem::remove_all(dir);
}

TEST(TCPServer, MultipleSequentialClientsShareTheSameDatabase) {
  std::string dir = TempDir("multi_client");
  {
    Database db(dir);
    executor::Executor exec(&db);
    TCPServer server(&exec);
    server.Start(0);

    {
      TestClient client1;
      EXPECT_TRUE(client1.Connect(server.Port()));
      client1.Query("CREATE TABLE shared (id INT)");
      client1.Query("INSERT INTO shared VALUES (1)");
    }  

    {
      TestClient client2;
      EXPECT_TRUE(client2.Connect(server.Port()));
      auto select = client2.Query("SELECT id FROM shared");
      EXPECT_TRUE(select.ok());
      EXPECT_TRUE(select.value().first);
      EXPECT_TRUE(select.value().second.find("1") != std::string::npos);
    }

    server.Stop();
  }
  std::filesystem::remove_all(dir);
}

TEST(TCPServer, ConcurrentClientsDoNotCorruptSharedState) {
  std::string dir = TempDir("concurrent");
  {
    Database db(dir);
    executor::Executor exec(&db);
    TCPServer server(&exec);
    server.Start(0);
    int port = server.Port();

    {
      TestClient setup;
      setup.Connect(port);
      setup.Query("CREATE TABLE counter (id INT)");
    }

    constexpr int kClients = 8;
    constexpr int kInsertsPerClient = 20;
    std::vector<std::thread> threads;
    for (int t = 0; t < kClients; ++t) {
      threads.emplace_back([port, t]() {
        TestClient client;
        client.Connect(port);
        for (int i = 0; i < kInsertsPerClient; ++i) {
          client.Query("INSERT INTO counter VALUES (" + std::to_string(t * 1000 + i) + ")");
        }
      });
    }
    for (auto& th : threads) th.join();

    TestClient verifier;
    verifier.Connect(port);
    auto select = verifier.Query("SELECT COUNT(*) FROM counter");
    EXPECT_TRUE(select.ok());
    EXPECT_TRUE(select.value().first);
    EXPECT_TRUE(select.value().second.find(std::to_string(kClients * kInsertsPerClient)) != std::string::npos);

    server.Stop();
  }
  std::filesystem::remove_all(dir);
}

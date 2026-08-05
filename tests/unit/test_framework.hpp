#pragma once

#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>


inline std::string TestTempRoot() {

  static const std::string root = std::filesystem::temp_directory_path().string() + "/minidb_tests/";
  std::filesystem::create_directories(root);
  return root;
}

namespace minidb::test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
  static std::vector<TestCase> registry;
  return registry;
}

inline int& FailureCount() {
  static int failures = 0;
  return failures;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    Registry().push_back({name, std::move(fn)});
  }
};

inline int RunAll() {
  int ran = 0;
  for (auto& tc : Registry()) {
    std::cout << "[RUNNING] " << tc.name << "\n" << std::flush;
    int before = FailureCount();
    tc.fn();
    bool passed = (FailureCount() == before);
    std::cout << (passed ? "[  OK  ] " : "[ FAIL ] ") << tc.name << "\n" << std::flush;
    ++ran;
  }
  std::cout << "----\n" << ran << " tests ran, " << FailureCount() << " assertion failures\n";
  return FailureCount() == 0 ? 0 : 1;
}

}  

#define MINIDB_CONCAT_INNER(a, b) a##b
#define MINIDB_CONCAT(a, b) MINIDB_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                            \
  void MINIDB_CONCAT(suite##_##name##_, __LINE__)();                                 \
  static minidb::test::Registrar MINIDB_CONCAT(registrar_, __LINE__)(                \
      #suite "." #name, MINIDB_CONCAT(suite##_##name##_, __LINE__));                 \
  void MINIDB_CONCAT(suite##_##name##_, __LINE__)()

#define EXPECT_TRUE(cond)                                                            \
  do {                                                                               \
    if (!(cond)) {                                                                   \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " EXPECT_TRUE(" #cond   \
                << ")\n";                                                            \
      ++minidb::test::FailureCount();                                                \
    }                                                                                \
  } while (0)

#define EXPECT_FALSE(cond) EXPECT_TRUE(!(cond))

#define EXPECT_EQ(a, b)                                                              \
  do {                                                                               \
    if (!((a) == (b))) {                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " EXPECT_EQ(" #a ", " #b \
                << ")\n";                                                            \
      ++minidb::test::FailureCount();                                                \
    }                                                                                \
  } while (0)

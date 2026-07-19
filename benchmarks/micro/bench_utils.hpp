#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace minidb::bench {


inline std::string BenchTempRoot() {
  static const std::string root = std::filesystem::temp_directory_path().string() + "/minidb_bench/";
  std::filesystem::create_directories(root);
  return root;
}

class Timer {
 public:
  Timer() : start_(std::chrono::steady_clock::now()) {}
  double ElapsedMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - start_).count();
  }
  void Reset() { start_ = std::chrono::steady_clock::now(); }

 private:
  std::chrono::steady_clock::time_point start_;
};

struct Result {
  std::string name;
  std::size_t operations;
  double elapsed_ms;
  std::string note;

  double OpsPerSec() const { return elapsed_ms > 0 ? (operations / elapsed_ms) * 1000.0 : 0.0; }
  double UsPerOp() const { return operations > 0 ? (elapsed_ms * 1000.0) / operations : 0.0; }
};

inline std::vector<Result>& AllResults() {
  static std::vector<Result> results;
  return results;
}


inline void Run(const std::string& name, std::size_t operations, const std::string& note,
                 const std::function<void()>& fn) {
  Timer t;
  fn();
  double elapsed = t.ElapsedMs();
  Result r{name, operations, elapsed, note};
  AllResults().push_back(r);
  std::printf("  %-38s %10zu ops   %9.2f ms   %12.1f ops/sec   %8.2f us/op   %s\n", name.c_str(), operations,
              elapsed, r.OpsPerSec(), r.UsPerOp(), note.c_str());
  std::fflush(stdout);
}

inline void PrintHeader(const std::string& section) {
  std::printf("\n=== %s ===\n", section.c_str());
  std::printf("  %-38s %14s   %12s   %16s   %11s   %s\n", "benchmark", "operations", "elapsed", "throughput",
              "latency", "note");
  std::fflush(stdout);
}

}  

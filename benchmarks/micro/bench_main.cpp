#include <cstdio>

void RunStorageMicroBenchmarks();
void RunMacroBenchmarks();

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("MiniDB Benchmarks\n");
  std::printf("(all numbers measured on this run, in this environment -- not representative of\n");
  std::printf(" any particular production hardware; useful for relative comparison, e.g.\n");
  std::printf(" IndexScan vs. SeqScan on identical data, not as absolute performance claims)\n");

  RunStorageMicroBenchmarks();
  RunMacroBenchmarks();

  std::printf("\nDone.\n");
  return 0;
}

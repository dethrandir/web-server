#pragma once

#include <cstddef>

namespace bench {

struct MicroOptions {
  std::size_t iterations = 1'000'000;
};

struct LoadOptions {
  std::size_t clients = 16;      // concurrent client connections
  std::size_t requests = 5000;   // requests per client
  std::size_t file_size = 1024;  // served file size, in bytes
  bool keep_alive = true;
  bool cache = true;
};

void run_micro_benchmarks(const MicroOptions& options);

void run_load_benchmark(const LoadOptions& options);

}  // namespace bench

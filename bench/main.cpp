#include "bench.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

void print_usage() {
  std::printf("usage: web_server_bench [options]\n"
              "\n"
              "  --clients N     concurrent client connections (default 16)\n"
              "  --requests N    requests per client (default 5000)\n"
              "  --file-size N   bytes of the served file (default 1024)\n"
              "  --close         use Connection: close instead of keep-alive\n"
              "  --no-cache      disable the response cache\n"
              "  --micro-only    run only the micro benchmarks\n"
              "  --load-only     run only the load benchmark\n"
              "  -h, --help      show this help\n");
}

}  // namespace

int main(int argc, char** argv) {
  bench::MicroOptions micro;
  bench::LoadOptions load;
  bool run_micro = true;
  bool run_load = true;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    const auto take_number = [&](std::size_t& target) {
      if (i + 1 < argc) {
        target = std::strtoull(argv[++i], nullptr, 10);
      }
    };

    if (argument == "--clients") {
      take_number(load.clients);
    } else if (argument == "--requests") {
      take_number(load.requests);
    } else if (argument == "--file-size") {
      take_number(load.file_size);
    } else if (argument == "--iterations") {
      take_number(micro.iterations);
    } else if (argument == "--close") {
      load.keep_alive = false;
    } else if (argument == "--no-cache") {
      load.cache = false;
    } else if (argument == "--micro-only") {
      run_load = false;
    } else if (argument == "--load-only") {
      run_micro = false;
    } else if (argument == "-h" || argument == "--help") {
      print_usage();
      return 0;
    } else {
      std::fprintf(stderr, "unknown option: %s\n", argument.c_str());
      return 1;
    }
  }

  if (run_micro) {
    bench::run_micro_benchmarks(micro);
  }
  if (run_load) {
    if (run_micro) {
      std::printf("\n");
    }
    bench::run_load_benchmark(load);
  }
  return 0;
}

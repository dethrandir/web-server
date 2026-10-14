#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace bench {

inline double seconds_since(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// Prevents the optimizer from discarding a computation.
template <typename T> inline void do_not_optimize(const T& value) {
#if defined(__GNUC__) || defined(__clang__)
  asm volatile("" : : "r,m"(value) : "memory");
#else
  volatile const T* sink = &value;
  static_cast<void>(sink);
#endif
}

struct Statistics {
  double min = 0.0;
  double max = 0.0;
  double mean = 0.0;
  double p50 = 0.0;
  double p90 = 0.0;
  double p99 = 0.0;
};

inline Statistics summarize(std::vector<double> samples) {
  Statistics stats;
  if (samples.empty()) {
    return stats;
  }
  std::sort(samples.begin(), samples.end());

  double sum = 0.0;
  for (double value : samples) {
    sum += value;
  }

  const auto percentile = [&samples](double quantile) {
    const auto index = static_cast<std::size_t>(quantile * static_cast<double>(samples.size()));
    return samples[std::min(index, samples.size() - 1)];
  };

  stats.min = samples.front();
  stats.max = samples.back();
  stats.mean = sum / static_cast<double>(samples.size());
  stats.p50 = percentile(0.50);
  stats.p90 = percentile(0.90);
  stats.p99 = percentile(0.99);
  return stats;
}

template <typename F> inline void run_micro(const char* name, std::size_t iterations, F&& body) {
  for (int i = 0; i < 3; ++i) {
    body();
  }
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    body();
  }
  const double elapsed = seconds_since(start);
  const double ns_per_op = elapsed / static_cast<double>(iterations) * 1e9;
  std::printf("  %-26s %10.1f ns/op  (%zu iters)\n", name, ns_per_op, iterations);
}

}  // namespace bench

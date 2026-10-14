#pragma once

#include "stats/stats.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace web_server::stats {

// Periodically samples a snapshot from a provider, computes CPU usage between
// samples, and (optionally) appends each sample as a JSON line to a file. The
// latest snapshot is kept as pretty JSON for the statistics endpoint.
class Reporter {
public:
  using Provider = std::function<Snapshot()>;

  Reporter(Provider provider, std::filesystem::path file, std::chrono::seconds interval);
  ~Reporter();

  Reporter(const Reporter&) = delete;
  Reporter& operator=(const Reporter&) = delete;

  void start();
  void stop();

  // The most recent sample (before the first sample, all fields are zero).
  [[nodiscard]] Snapshot latest() const;

private:
  void loop();
  void sample();

  Provider provider_;
  std::filesystem::path file_;
  std::chrono::seconds interval_;
  std::chrono::steady_clock::time_point started_at_{};
  std::chrono::steady_clock::time_point last_sample_at_{};
  double last_cpu_seconds_ = 0.0;
  bool has_previous_ = false;

  std::atomic<bool> running_{false};
  std::thread thread_;
  mutable std::mutex mutex_;
  Snapshot latest_;
};

}  // namespace web_server::stats

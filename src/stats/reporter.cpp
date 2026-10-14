#include "stats/reporter.hpp"

#include <fstream>
#include <utility>

namespace web_server::stats {

Reporter::Reporter(Provider provider, std::filesystem::path file, std::chrono::seconds interval)
    : provider_(std::move(provider)), file_(std::move(file)), interval_(interval) {}

Reporter::~Reporter() {
  stop();
}

void Reporter::start() {
  if (running_.exchange(true)) {
    return;
  }
  started_at_ = std::chrono::steady_clock::now();
  has_previous_ = false;
  sample();
  thread_ = std::thread([this] { loop(); });
}

void Reporter::stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

Snapshot Reporter::latest() const {
  std::lock_guard lock{mutex_};
  return latest_;
}

void Reporter::loop() {
  const std::chrono::seconds interval = interval_.count() > 0 ? interval_ : std::chrono::seconds{1};
  while (running_.load()) {
    const auto deadline = std::chrono::steady_clock::now() + interval;
    while (running_.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
    if (!running_.load()) {
      break;
    }
    sample();
  }
}

void Reporter::sample() {
  const auto now = std::chrono::steady_clock::now();

  Snapshot snapshot = provider_();
  snapshot.timestamp = utc_timestamp();
  snapshot.uptime_seconds = std::chrono::duration<double>(now - started_at_).count();
  snapshot.rss_bytes = read_rss_bytes();
  snapshot.cpu_seconds = read_process_cpu_seconds();

  if (has_previous_) {
    const double elapsed = std::chrono::duration<double>(now - last_sample_at_).count();
    if (elapsed > 0.0) {
      snapshot.cpu_percent = (snapshot.cpu_seconds - last_cpu_seconds_) / elapsed * 100.0;
    }
  }
  last_cpu_seconds_ = snapshot.cpu_seconds;
  last_sample_at_ = now;
  has_previous_ = true;

  const std::uint64_t lookups = snapshot.cache_hits + snapshot.cache_misses;
  if (lookups > 0) {
    snapshot.cache_hit_ratio =
      static_cast<double>(snapshot.cache_hits) / static_cast<double>(lookups);
  }

  {
    std::lock_guard lock{mutex_};
    latest_ = snapshot;
  }

  if (!file_.empty()) {
    std::ofstream out(file_, std::ios::binary | std::ios::app);
    out << to_json(snapshot, /*pretty=*/false) << '\n';
  }
}

}  // namespace web_server::stats

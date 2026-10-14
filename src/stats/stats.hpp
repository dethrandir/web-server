#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace web_server::stats {

// Live counters updated on the hot path. All fields are atomic so they can be
// bumped from any worker thread without locking.
struct Counters {
  std::atomic<std::uint64_t> requests{0};
  std::atomic<std::uint64_t> responses_2xx{0};
  std::atomic<std::uint64_t> responses_3xx{0};
  std::atomic<std::uint64_t> responses_4xx{0};
  std::atomic<std::uint64_t> responses_5xx{0};
  std::atomic<std::uint64_t> connections_accepted{0};
  std::atomic<std::uint64_t> connections_active{0};
  std::atomic<std::uint64_t> bytes_received{0};
  std::atomic<std::uint64_t> bytes_sent{0};
  std::atomic<std::uint64_t> workers_total{0};
  std::atomic<std::uint64_t> workers_busy{0};
  std::atomic<std::uint64_t> queue_length{0};

  Counters() = default;
  Counters(const Counters&) = delete;
  Counters& operator=(const Counters&) = delete;
};

// A point-in-time view of everything the stats module can report.
struct Snapshot {
  std::string timestamp;
  double uptime_seconds = 0.0;

  std::uint64_t requests = 0;
  std::uint64_t responses_2xx = 0;
  std::uint64_t responses_3xx = 0;
  std::uint64_t responses_4xx = 0;
  std::uint64_t responses_5xx = 0;
  std::uint64_t connections_accepted = 0;
  std::uint64_t connections_active = 0;
  std::uint64_t bytes_received = 0;
  std::uint64_t bytes_sent = 0;

  std::uint64_t cache_entries = 0;
  std::uint64_t cache_bytes = 0;
  std::uint64_t cache_hits = 0;
  std::uint64_t cache_misses = 0;
  std::uint64_t cache_evictions = 0;
  double cache_hit_ratio = 0.0;

  std::uint64_t workers_total = 0;
  std::uint64_t workers_busy = 0;
  std::uint64_t queue_length = 0;

  std::uint64_t rss_bytes = 0;
  double cpu_seconds = 0.0;
  double cpu_percent = 0.0;
};

// Serializes a snapshot; pretty-printed for the endpoint, compact for JSONL files.
[[nodiscard]] std::string to_json(const Snapshot& snapshot, bool pretty = true);

// ISO-8601 UTC timestamp, e.g. "2026-10-14T15:04:05Z".
[[nodiscard]] std::string utc_timestamp();

// Resident set size of the current process, in bytes (Linux).
[[nodiscard]] std::uint64_t read_rss_bytes();

// Cumulative CPU time consumed by the process, in seconds.
[[nodiscard]] double read_process_cpu_seconds();

}  // namespace web_server::stats

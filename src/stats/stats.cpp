#include "stats/stats.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/resource.h>
#include <unistd.h>

namespace web_server::stats {

namespace {

double to_seconds(const timeval& value) {
  return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1'000'000.0;
}

}  // namespace

std::string to_json(const Snapshot& snapshot, bool pretty) {
  nlohmann::json json;
  json["timestamp"] = snapshot.timestamp;
  json["uptime_seconds"] = snapshot.uptime_seconds;

  json["requests"] = {
    {"total", snapshot.requests},    {"2xx", snapshot.responses_2xx},
    {"3xx", snapshot.responses_3xx}, {"4xx", snapshot.responses_4xx},
    {"5xx", snapshot.responses_5xx},
  };

  json["connections"] = {
    {"accepted", snapshot.connections_accepted},
    {"active", snapshot.connections_active},
  };

  json["bytes"] = {
    {"received", snapshot.bytes_received},
    {"sent", snapshot.bytes_sent},
  };

  json["cache"] = {
    {"entries", snapshot.cache_entries},     {"bytes", snapshot.cache_bytes},
    {"hits", snapshot.cache_hits},           {"misses", snapshot.cache_misses},
    {"evictions", snapshot.cache_evictions}, {"hit_ratio", snapshot.cache_hit_ratio},
  };

  json["workers"] = {
    {"total", snapshot.workers_total},
    {"busy", snapshot.workers_busy},
    {"queue", snapshot.queue_length},
  };

  json["process"] = {
    {"rss_bytes", snapshot.rss_bytes},
    {"cpu_seconds", snapshot.cpu_seconds},
    {"cpu_percent", snapshot.cpu_percent},
  };

  return pretty ? json.dump(2) : json.dump();
}

std::string utc_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
  ::gmtime_r(&now, &utc);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return buffer;
}

std::uint64_t read_rss_bytes() {
  std::ifstream statm("/proc/self/statm");
  std::uint64_t total_pages = 0;
  std::uint64_t resident_pages = 0;
  statm >> total_pages >> resident_pages;

  const long page_size = ::sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return 0;
  }
  return resident_pages * static_cast<std::uint64_t>(page_size);
}

double read_process_cpu_seconds() {
  rusage usage{};
  if (::getrusage(RUSAGE_SELF, &usage) != 0) {
    return 0.0;
  }
  return to_seconds(usage.ru_utime) + to_seconds(usage.ru_stime);
}

}  // namespace web_server::stats

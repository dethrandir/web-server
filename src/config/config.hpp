#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace web_server {

namespace config {

// Maps a request path to a file or directory on disk.
struct Route {
  std::string path;
  std::string target;
};

struct Config {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  // Number of worker threads. 0 means "use the hardware concurrency".
  std::uint32_t threads = 0;
  // Seconds an idle keep-alive connection is kept open before being closed.
  std::uint32_t keep_alive_timeout_seconds = 5;
  // Seconds allowed to receive a complete request before the connection is dropped.
  std::uint32_t read_timeout_seconds = 15;
  // Maximum size of the request header block, in bytes.
  std::size_t max_header_bytes = 16384;
  // Maximum size of the request body, in bytes.
  std::size_t max_body_bytes = 1024ull * 1024;
  std::string log_path;
  std::vector<Route> routes;
  // How long a cached response stays valid, in seconds. 0 disables caching.
  std::uint32_t cache_ttl_seconds = 0;
  // Maximum total memory the cache may use, in bytes.
  std::size_t cache_max_bytes = 1024ull * 1024 * 1024;
  // Files larger than this are served directly, without entering the cache.
  std::size_t cache_max_file_bytes = 64ull * 1024 * 1024;

  // Built-in statistics endpoint/dashboard (off by default).
  bool stats_enabled = false;
  std::string stats_path = "/statistics";                  // JSON endpoint
  std::string stats_page = "/statistics.html";             // dashboard page
  std::string stats_page_file = "static/statistics.html";  // page file on disk
  std::string stats_file;                                  // optional JSON Lines output
  std::uint32_t stats_interval_seconds = 5;                // sampling interval

  // Built-in route documentation endpoint/dashboard (off by default).
  bool docs_enabled = false;
  std::string docs_path = "/docs";                  // JSON endpoint
  std::string docs_page = "/docs.html";             // dashboard page
  std::string docs_page_file = "static/docs.html";  // page file on disk

  // Live watching of files (inotify).
  bool watch_enabled = false;
  bool watch_config_reload = false;    // reload config on change
  bool watch_invalidate_cache = true;  // evict cached files when they change
};

Config load(const std::filesystem::path& path);

std::vector<std::string> validate(const Config& config);

void write_example(const std::filesystem::path& path);

// Returns the route whose path is the longest prefix of request_path, or nullptr.
const Route* find_route(const std::vector<Route>& routes, std::string_view request_path);

}  // namespace config

}  // namespace web_server

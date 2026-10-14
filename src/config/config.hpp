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
  std::string log_path;
  std::vector<Route> routes;
  // How long a cached response stays valid, in seconds. 0 disables caching.
  std::uint32_t cache_ttl_seconds = 0;
  // Maximum total memory the cache may use, in bytes.
  std::size_t cache_max_bytes = 1024ull * 1024 * 1024;
  // Files larger than this are served directly, without entering the cache.
  std::size_t cache_max_file_bytes = 64ull * 1024 * 1024;
};

Config load(const std::filesystem::path& path);

std::vector<std::string> validate(const Config& config);

void write_example(const std::filesystem::path& path);

// Returns the route whose path is the longest prefix of request_path, or nullptr.
const Route* find_route(const std::vector<Route>& routes, std::string_view request_path);

}  // namespace config

}  // namespace web_server

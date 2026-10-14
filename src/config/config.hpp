#pragma once

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
};

Config load(const std::filesystem::path& path);

std::vector<std::string> validate(const Config& config);

void write_example(const std::filesystem::path& path);

// Returns the route whose path is the longest prefix of request_path, or nullptr.
const Route* find_route(const std::vector<Route>& routes, std::string_view request_path);

}  // namespace config

}  // namespace web_server

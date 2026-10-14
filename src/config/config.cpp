#include "config.hpp"

#include <toml.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace web_server {

namespace config {

namespace {

constexpr std::string_view kExampleConfig = R"(# web-server configuration file

[server]
host = "0.0.0.0"
port = 8080

[routes]
# Map a request path to a file or directory to serve.
"/merhaba" = "./public/merhaba.html"
"/assets" = "./public/assets"

[logging]
# Path of the log file. Use "-" to log to the console instead.
path = "web-server.log"
)";

}  // namespace

Config load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("configuration file not found: " + path.string());
  }

  const toml::table tbl = toml::parse_file(path.string());

  Config config;

  if (const auto* server = tbl["server"].as_table()) {
    config.host = (*server)["host"].value_or(config.host);

    const std::int64_t port = (*server)["port"].value_or(static_cast<std::int64_t>(config.port));
    if (port < 1 || port > 65535) {
      throw std::runtime_error("server.port must be in range 1-65535");
    }
    config.port = static_cast<std::uint16_t>(port);
  }

  if (const auto* routes = tbl["routes"].as_table()) {
    for (const auto& [key, value] : *routes) {
      Route route;
      route.path = std::string{key.str()};
      route.target = value.value_or(std::string{});
      config.routes.push_back(std::move(route));
    }
  }

  if (const auto* logging = tbl["logging"].as_table()) {
    config.log_path = (*logging)["path"].value_or(config.log_path);
  }

  return config;
}

std::vector<std::string> validate(const Config& config) {
  std::vector<std::string> errors;

  if (config.host.empty()) {
    errors.emplace_back("server.host must not be empty");
  }

  for (const Route& route : config.routes) {
    if (route.path.empty() || route.path.front() != '/') {
      errors.emplace_back("route path must start with '/': " + route.path);
    }
    if (route.target.empty()) {
      errors.emplace_back("route target must not be empty: " + route.path);
    } else if (!std::filesystem::exists(route.target)) {
      errors.emplace_back("route target does not exist: " + route.target);
    }
  }

  if (!config.log_path.empty() && config.log_path != "-") {
    if (std::filesystem::is_directory(config.log_path)) {
      errors.emplace_back("logging.path points to a directory: " + config.log_path);
    }
  }

  return errors;
}

void write_example(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("could not open file for writing: " + path.string());
  }

  out << kExampleConfig;
  if (!out) {
    throw std::runtime_error("could not write configuration file: " + path.string());
  }
}

const Route* find_route(const std::vector<Route>& routes, std::string_view request_path) {
  const Route* best = nullptr;
  for (const Route& route : routes) {
    if (route.path.empty() || !request_path.starts_with(route.path)) {
      continue;
    }
    if (request_path.size() != route.path.size() && route.path.back() != '/' &&
        request_path[route.path.size()] != '/') {
      continue;
    }
    if (best == nullptr || route.path.size() > best->path.size()) {
      best = &route;
    }
  }
  return best;
}

}  // namespace config

}  // namespace web_server

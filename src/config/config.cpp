#include "config.hpp"

#include "util/string.hpp"

#include <toml.hpp>

#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace web_server {

namespace config {

namespace {

constexpr std::string_view kExampleConfig = R"(# web-server configuration file

[server]
host = "0.0.0.0"
port = 8080
# Worker threads (0 = one per CPU core).
threads = 0
# Seconds an idle keep-alive connection stays open.
keep_alive_timeout = 5
# Seconds allowed to receive a complete request.
read_timeout = 15
# Request header/body size limits, in bytes.
max_header_bytes = 16384
max_body_bytes = 1048576

[routes]
# Map a request path to a file or directory to serve.
"/merhaba" = "./public/merhaba.html"
"/assets" = "./public/assets"

[cache]
# Time-to-live for cached files, in seconds (0 disables caching).
ttl = 60
# Maximum total memory the cache may use (accepts B/K/M/G suffixes).
max_size = "1G"
# Files larger than this are served directly, without caching.
max_file_size = "64M"

[stats]
# Built-in statistics endpoint and dashboard (off by default).
enabled = false
path = "/statistics"
page = "/statistics.html"
page_file = "static/statistics.html"
# Optional JSON Lines output; a timestamped sample is appended every interval.
file = ""
interval = 5

[docs]
# Built-in route documentation endpoint and dashboard (off by default).
enabled = false
path = "/docs"
page = "/docs.html"
page_file = "static/docs.html"

[watch]
# Watch files with inotify (off by default).
enabled = false
# Reload the configuration when its file changes.
config_reload = false
# Evict a cached file as soon as it changes on disk.
invalidate_cache = true

[logging]
# Path of the log file. Use "-" to log to the console instead.
path = "web-server.log"
)";

std::size_t parse_size(std::string_view text) {
  const std::string_view trimmed = util::trim(text);
  if (trimmed.empty()) {
    throw std::runtime_error("expected a size value, got an empty string");
  }

  std::size_t digits = 0;
  while (digits < trimmed.size() &&
         std::isdigit(static_cast<unsigned char>(trimmed[digits])) != 0) {
    ++digits;
  }
  if (digits == 0) {
    throw std::runtime_error("invalid size value: " + std::string{trimmed});
  }

  std::uint64_t number = 0;
  const auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + digits, number);
  if (ec != std::errc{}) {
    throw std::runtime_error("invalid size value: " + std::string{trimmed});
  }

  const std::string_view suffix = util::trim(trimmed.substr(digits));
  std::uint64_t multiplier = 1;
  if (suffix.empty() || suffix == "B" || suffix == "b") {
    multiplier = 1;
  } else if (suffix == "K" || suffix == "KB" || suffix == "k" || suffix == "kb") {
    multiplier = 1024ull;
  } else if (suffix == "M" || suffix == "MB" || suffix == "m" || suffix == "mb") {
    multiplier = 1024ull * 1024;
  } else if (suffix == "G" || suffix == "GB" || suffix == "g" || suffix == "gb") {
    multiplier = 1024ull * 1024 * 1024;
  } else if (suffix == "T" || suffix == "TB" || suffix == "t" || suffix == "tb") {
    multiplier = 1024ull * 1024 * 1024 * 1024;
  } else {
    throw std::runtime_error("unknown size suffix: " + std::string{suffix});
  }

  return static_cast<std::size_t>(number * multiplier);
}

std::size_t read_size(const toml::table& table, std::string_view key, std::size_t fallback) {
  const toml::node* value = table.get(key);
  if (value == nullptr) {
    return fallback;
  }
  if (const auto* as_string = value->as_string()) {
    return parse_size(as_string->get());
  }
  if (const auto* as_integer = value->as_integer()) {
    if (as_integer->get() < 0) {
      throw std::runtime_error(std::string{key} + " must not be negative");
    }
    return static_cast<std::size_t>(as_integer->get());
  }
  throw std::runtime_error(std::string{key} + " must be a number or size string");
}

std::uint32_t read_uint(const toml::table& table, std::string_view key, std::uint32_t fallback) {
  const std::int64_t value = table[key].value_or(static_cast<std::int64_t>(fallback));
  if (value < 0) {
    throw std::runtime_error(std::string{key} + " must not be negative");
  }
  return static_cast<std::uint32_t>(value);
}

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

    config.threads = read_uint(*server, "threads", config.threads);
    config.keep_alive_timeout_seconds =
      read_uint(*server, "keep_alive_timeout", config.keep_alive_timeout_seconds);
    config.read_timeout_seconds = read_uint(*server, "read_timeout", config.read_timeout_seconds);
    config.max_header_bytes = read_size(*server, "max_header_bytes", config.max_header_bytes);
    config.max_body_bytes = read_size(*server, "max_body_bytes", config.max_body_bytes);
  }

  if (const auto* routes = tbl["routes"].as_table()) {
    for (const auto& [key, value] : *routes) {
      Route route;
      route.path = std::string{key.str()};
      route.target = value.value_or(std::string{});
      config.routes.push_back(std::move(route));
    }
  }

  if (const auto* cache = tbl["cache"].as_table()) {
    const std::int64_t ttl =
      (*cache)["ttl"].value_or(static_cast<std::int64_t>(config.cache_ttl_seconds));
    if (ttl < 0) {
      throw std::runtime_error("cache.ttl must not be negative");
    }
    config.cache_ttl_seconds = static_cast<std::uint32_t>(ttl);
    config.cache_max_bytes = read_size(*cache, "max_size", config.cache_max_bytes);
    config.cache_max_file_bytes = read_size(*cache, "max_file_size", config.cache_max_file_bytes);
  }

  if (const auto* stats = tbl["stats"].as_table()) {
    config.stats_enabled = (*stats)["enabled"].value_or(config.stats_enabled);
    config.stats_path = (*stats)["path"].value_or(config.stats_path);
    config.stats_page = (*stats)["page"].value_or(config.stats_page);
    config.stats_page_file = (*stats)["page_file"].value_or(config.stats_page_file);
    config.stats_file = (*stats)["file"].value_or(config.stats_file);
    config.stats_interval_seconds = read_uint(*stats, "interval", config.stats_interval_seconds);
  }

  if (const auto* docs = tbl["docs"].as_table()) {
    config.docs_enabled = (*docs)["enabled"].value_or(config.docs_enabled);
    config.docs_path = (*docs)["path"].value_or(config.docs_path);
    config.docs_page = (*docs)["page"].value_or(config.docs_page);
    config.docs_page_file = (*docs)["page_file"].value_or(config.docs_page_file);
  }

  if (const auto* watch = tbl["watch"].as_table()) {
    config.watch_enabled = (*watch)["enabled"].value_or(config.watch_enabled);
    config.watch_config_reload = (*watch)["config_reload"].value_or(config.watch_config_reload);
    config.watch_invalidate_cache =
      (*watch)["invalidate_cache"].value_or(config.watch_invalidate_cache);
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

#include "config.hpp"

#include <toml.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace web_server {

namespace config {

namespace {

constexpr std::string_view kExampleConfig = R"(# web-server configuration file

[server]
host = "0.0.0.0"
port = 8080

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

}  // namespace config

}  // namespace web_server

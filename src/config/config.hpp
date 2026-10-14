#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace web_server {

namespace config {

struct Config {
  std::string host = "0.0.0.0";
  std::uint16_t port = 8080;
  std::string log_path;
};

Config load(const std::filesystem::path& path);

std::vector<std::string> validate(const Config& config);

void write_example(const std::filesystem::path& path);

}  // namespace config

}  // namespace web_server

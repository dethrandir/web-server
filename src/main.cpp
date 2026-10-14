#include "config.hpp"

#include <cxxopts.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace web_server {

namespace {

constexpr std::string_view kProgramName = "web-server";
constexpr std::string_view kVersion = WEB_SERVER_VERSION;

void setup_logging(const config::Config& config, spdlog::level::level_enum level) {
  if (!config.log_path.empty() && config.log_path != "-") {
    const std::filesystem::path path = config.log_path;
    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path());
    }
    spdlog::set_default_logger(spdlog::basic_logger_mt("file", path.string()));
  }

  spdlog::set_level(level);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::flush_on(spdlog::level::info);
}

int run_validate(const std::filesystem::path& path) {
  try {
    const config::Config cfg = config::load(path);
    const std::vector<std::string> errors = config::validate(cfg);

    if (!errors.empty()) {
      std::cerr << "configuration '" << path.string() << "' is invalid:\n";
      for (const std::string& error : errors) {
        std::cerr << "  - " << error << '\n';
      }
      return 1;
    }

    std::cout << "configuration '" << path.string() << "' is valid\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "configuration '" << path.string() << "' is invalid: " << e.what() << '\n';
    return 1;
  }
}

}  // namespace

int run(int argc, char** argv) {
  cxxopts::Options options{std::string{kProgramName}, "A simple web server"};

  // clang-format off
  options.add_options()
    ("h,help", "Print this help message")
    ("v,version", "Print version information")
    ("c,config", "Path to the configuration file", cxxopts::value<std::string>())
    ("e,example", "Write an example configuration file to PATH", cxxopts::value<std::string>())
    ("t,validate", "Validate the configuration file at PATH and exit", cxxopts::value<std::string>())
    ("verbose", "Enable verbose (debug) logging");
  // clang-format on

  try {
    const cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help") > 0) {
      std::cout << options.help() << '\n';
      return 0;
    }

    if (result.count("version") > 0) {
      std::cout << kProgramName << ' ' << kVersion << '\n';
      return 0;
    }

    if (result.count("validate") > 0) {
      return run_validate(result["validate"].as<std::string>());
    }

    if (result.count("example") > 0) {
      const std::filesystem::path path = result["example"].as<std::string>();
      config::write_example(path);
      std::cout << "example configuration written to '" << path.string() << "'\n";
      return 0;
    }

    const bool has_config = result.count("config") > 0;
    const config::Config config =
      has_config ? config::load(result["config"].as<std::string>()) : config::Config{};

    setup_logging(config, result.count("verbose") > 0 ? spdlog::level::debug : spdlog::level::info);

    if (has_config) {
      spdlog::info("loaded configuration: host={} port={} log_path={}", config.host, config.port,
                   config.log_path.empty() ? "(console)" : config.log_path);
    } else {
      spdlog::warn("no configuration file provided (use --config PATH)");
    }

    spdlog::info("{} {} starting", kProgramName, kVersion);
    return 0;
  } catch (const cxxopts::exceptions::exception& e) {
    spdlog::error("invalid arguments: {}", e.what());
    std::cerr << options.help() << '\n';
    return 1;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return 1;
  }
}

}  // namespace web_server

int main(int argc, char** argv) {
  return web_server::run(argc, argv);
}

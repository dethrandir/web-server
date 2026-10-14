#include "config/config.hpp"
#include "log/log.hpp"
#include "server/server.hpp"

#include <cxxopts.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace web_server {

namespace {

constexpr std::string_view kProgramName = "web-server";
constexpr std::string_view kVersion = WEB_SERVER_VERSION;

std::atomic<bool> g_stop_requested{false};

extern "C" void handle_signal(int) {
  g_stop_requested.store(true);
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

    const bool to_console = config.log_path.empty() || config.log_path == "-";
    log::init(to_console ? std::filesystem::path{} : std::filesystem::path{config.log_path},
              result.count("verbose") > 0 ? log::Level::Debug : log::Level::Info);

    if (has_config) {
      log::info("loaded configuration: host={} port={} threads={} routes={} cache_ttl={}s "
                "cache_max={}B log_path={}",
                config.host, config.port, config.threads, config.routes.size(),
                config.cache_ttl_seconds, config.cache_max_bytes,
                to_console ? "(console)" : config.log_path);
    } else {
      log::warn("no configuration file provided (use --config PATH)");
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    server::Server server{config};
    server.start();
    log::info("{} {} started (press Ctrl+C to stop)", kProgramName, kVersion);

    while (!g_stop_requested.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{200});
    }

    log::info("shutting down");
    server.stop();
    log::info("stopped");
    return 0;
  } catch (const cxxopts::exceptions::exception& e) {
    log::error("invalid arguments: {}", e.what());
    std::cerr << options.help() << '\n';
    return 1;
  } catch (const std::exception& e) {
    log::error("{}", e.what());
    return 1;
  }
}

}  // namespace web_server

int main(int argc, char** argv) {
  return web_server::run(argc, argv);
}

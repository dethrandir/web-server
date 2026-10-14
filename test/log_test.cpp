#include "log/log.hpp"

#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace logging = web_server::log;

namespace {

struct TempDir {
  std::filesystem::path path;
  explicit TempDir(std::string_view name) : path(std::filesystem::temp_directory_path() / name) {
    std::filesystem::remove_all(path);
  }
  ~TempDir() {
    logging::shutdown();
    std::filesystem::remove_all(path);
  }
};

std::string read_all(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("log: writes messages to the configured file", "[log]") {
  const TempDir dir{"web_server_log_test"};
  const std::filesystem::path file = dir.path / "app.log";

  logging::init(file, logging::Level::Debug);
  logging::info("hello {}", 42);
  logging::debug("debug line");
  logging::shutdown();

  REQUIRE(std::filesystem::exists(file));
  const std::string content = read_all(file);
  REQUIRE(content.find("hello 42") != std::string::npos);
  REQUIRE(content.find("debug line") != std::string::npos);
}

TEST_CASE("log: respects the configured level", "[log]") {
  const TempDir dir{"web_server_log_level"};
  const std::filesystem::path file = dir.path / "level.log";

  logging::init(file, logging::Level::Error);
  logging::info("suppressed");
  logging::error("shown");
  logging::shutdown();

  const std::string content = read_all(file);
  REQUIRE(content.find("suppressed") == std::string::npos);
  REQUIRE(content.find("shown") != std::string::npos);
}

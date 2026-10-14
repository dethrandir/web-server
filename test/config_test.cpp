#include "config/config.hpp"

#include <catch_amalgamated.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace config = web_server::config;

namespace {

std::filesystem::path write_fixture(std::string_view name, std::string_view content) {
  const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << content;
  return path;
}

struct TempFile {
  std::filesystem::path path;
  explicit TempFile(std::filesystem::path p) : path(std::move(p)) {}
  ~TempFile() { std::filesystem::remove(path); }
};

}  // namespace

TEST_CASE("config: loads server, routes and logging", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_ok.toml",
                                    "[server]\nhost = \"127.0.0.1\"\nport = 9000\n"
                                    "[routes]\n\"/merhaba\" = \"./public/merhaba.html\"\n"
                                    "\"/assets\" = \"./public/assets\"\n"
                                    "[logging]\npath = \"app.log\"\n")};

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.host == "127.0.0.1");
  REQUIRE(cfg.port == 9000);
  REQUIRE(cfg.log_path == "app.log");
  REQUIRE(cfg.routes.size() == 2);

  const config::Route* route = config::find_route(cfg.routes, "/merhaba");
  REQUIRE(route != nullptr);
  REQUIRE(route->target == "./public/merhaba.html");
}

TEST_CASE("config: falls back to defaults for missing settings", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_empty.toml", "")};

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.host == "0.0.0.0");
  REQUIRE(cfg.port == 8080);
  REQUIRE(cfg.log_path.empty());
  REQUIRE(cfg.routes.empty());
  REQUIRE(cfg.cache_ttl_seconds == 0);
  REQUIRE(cfg.cache_max_bytes == 1024ull * 1024 * 1024);
  REQUIRE(cfg.cache_max_file_bytes == 64ull * 1024 * 1024);
}

TEST_CASE("config: parses cache memory limits", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_cache_size.toml",
                                    "[cache]\nttl = 1\nmax_size = \"1G\"\n"
                                    "max_file_size = \"64M\"\n")};

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.cache_max_bytes == 1024ull * 1024 * 1024);
  REQUIRE(cfg.cache_max_file_bytes == 64ull * 1024 * 1024);
}

TEST_CASE("config: accepts a plain integer cache size", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_cache_int.toml", "[cache]\nmax_size = 2048\n")};

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.cache_max_bytes == 2048);
}

TEST_CASE("config: rejects an invalid cache size", "[config]") {
  const TempFile file{
    write_fixture("web_server_cfg_cache_bad_size.toml", "[cache]\nmax_size = \"1X\"\n")};
  REQUIRE_THROWS_AS(config::load(file.path), std::runtime_error);
}

TEST_CASE("config: loads the cache ttl", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_cache.toml", "[cache]\nttl = 120\n")};

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.cache_ttl_seconds == 120);
}

TEST_CASE("config: rejects a negative cache ttl", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_cache_bad.toml", "[cache]\nttl = -1\n")};
  REQUIRE_THROWS_AS(config::load(file.path), std::runtime_error);
}

TEST_CASE("config: rejects an out-of-range port", "[config]") {
  const TempFile file{write_fixture("web_server_cfg_bad_port.toml", "[server]\nport = 70000\n")};
  REQUIRE_THROWS_AS(config::load(file.path), std::runtime_error);
}

TEST_CASE("config: missing file throws", "[config]") {
  REQUIRE_THROWS(config::load("web_server_definitely_missing.toml"));
}

TEST_CASE("config: validate flags an empty host", "[config]") {
  config::Config cfg;
  cfg.host.clear();
  REQUIRE(config::validate(cfg).size() == 1);

  cfg.host = "0.0.0.0";
  REQUIRE(config::validate(cfg).empty());
}

TEST_CASE("config: validate checks routes", "[config]") {
  config::Config cfg;
  cfg.routes = {{"merhaba", "."}};
  REQUIRE(config::validate(cfg).size() == 1);

  cfg.routes = {{"/merhaba", "web_server_missing_target_xyz"}};
  REQUIRE(config::validate(cfg).size() == 1);

  cfg.routes = {{"/merhaba", "."}};
  REQUIRE(config::validate(cfg).empty());
}

TEST_CASE("config: find_route picks the longest matching prefix", "[config]") {
  const std::vector<config::Route> routes = {
    {"/", "root"}, {"/assets", "assets"}, {"/assets/img", "images"}, {"/merhaba", "merhaba"}};

  REQUIRE(config::find_route(routes, "/merhaba")->target == "merhaba");
  REQUIRE(config::find_route(routes, "/assets/img/logo.png")->target == "images");
  REQUIRE(config::find_route(routes, "/assets/app.js")->target == "assets");
  REQUIRE(config::find_route(routes, "/anything/else")->target == "root");

  const std::vector<config::Route> nested = {{"/assets", "assets"}, {"/merhaba", "merhaba"}};
  REQUIRE(config::find_route(nested, "/merhabaX") == nullptr);
  REQUIRE(config::find_route(nested, "/other") == nullptr);
}

TEST_CASE("config: write_example produces a loadable config", "[config]") {
  const TempFile file{std::filesystem::temp_directory_path() / "web_server_cfg_example.toml"};

  config::write_example(file.path);
  REQUIRE(std::filesystem::exists(file.path));

  const config::Config cfg = config::load(file.path);
  REQUIRE(cfg.port == 8080);
  REQUIRE(cfg.cache_ttl_seconds == 60);
  REQUIRE(cfg.cache_max_bytes == 1024ull * 1024 * 1024);
  REQUIRE(cfg.cache_max_file_bytes == 64ull * 1024 * 1024);
  REQUIRE(cfg.log_path == "web-server.log");
  REQUIRE(config::find_route(cfg.routes, "/merhaba")->target == "./public/merhaba.html");
}

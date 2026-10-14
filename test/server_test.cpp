#include "config/config.hpp"
#include "net/socket.hpp"
#include "server/server.hpp"

#include <array>
#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace server = web_server::server;
namespace config = web_server::config;
namespace net = web_server::net;

namespace {

struct Fixture {
  std::filesystem::path dir;
  std::filesystem::path file;

  Fixture() {
    dir = std::filesystem::temp_directory_path() / "web_server_server_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    file = dir / "hello.txt";
    std::ofstream out(file, std::ios::binary);
    out << "hello world";
  }

  ~Fixture() { std::filesystem::remove_all(dir); }
};

config::Config make_config(const std::filesystem::path& dir) {
  config::Config cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 0;
  cfg.threads = 2;
  cfg.read_timeout_seconds = 2;
  cfg.keep_alive_timeout_seconds = 2;
  cfg.routes = {{"/hello", (dir / "hello.txt").string()}, {"/static", dir.string()}};
  return cfg;
}

std::string round_trip(std::uint16_t port, std::string_view raw) {
  net::Socket connection = net::connect_tcp("127.0.0.1", port);
  net::send(connection, std::as_bytes(std::span{raw}));

  std::string response;
  std::array<std::byte, 4096> buffer{};
  while (true) {
    const std::size_t count = net::receive(connection, buffer);
    if (count == 0) {
      break;
    }
    response.append(reinterpret_cast<const char*>(buffer.data()), count);
  }
  return response;
}

}  // namespace

TEST_CASE("server: starts in a stopped state", "[server]") {
  server::Server instance{config::Config{}};
  REQUIRE_FALSE(instance.running());
}

TEST_CASE("server: serves a routed file", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();
  REQUIRE(instance.running());
  REQUIRE(instance.port() != 0);

  const std::string response =
    round_trip(instance.port(), "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 200"));
  REQUIRE(response.find("Content-Type: text/plain") != std::string::npos);
  REQUIRE(response.find("hello world") != std::string::npos);

  instance.stop();
  REQUIRE_FALSE(instance.running());
}

TEST_CASE("server: returns 404 for unknown paths", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();

  const std::string response =
    round_trip(instance.port(), "GET /nope HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 404"));

  instance.stop();
}

TEST_CASE("server: rejects unsupported methods", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();

  const std::string response =
    round_trip(instance.port(),
               "POST /hello HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 405"));
  REQUIRE(response.find("Allow: GET, HEAD") != std::string::npos);

  instance.stop();
}

TEST_CASE("server: HEAD omits the body", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();

  const std::string response =
    round_trip(instance.port(), "HEAD /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 200"));
  REQUIRE(response.find("Content-Length: 11") != std::string::npos);
  REQUIRE(response.find("hello world") == std::string::npos);

  instance.stop();
}

TEST_CASE("server: serves files from a directory route", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();

  const std::string response = round_trip(
    instance.port(), "GET /static/hello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 200"));
  REQUIRE(response.find("hello world") != std::string::npos);

  instance.stop();
}

TEST_CASE("server: blocks path traversal", "[server]") {
  const Fixture fixture;
  server::Server instance{make_config(fixture.dir)};
  instance.start();

  const std::string response = round_trip(
    instance.port(), "GET /static/..%2fhello.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(response.starts_with("HTTP/1.1 403"));

  instance.stop();
}

TEST_CASE("server: invalidates the cache when a file changes", "[server]") {
  const Fixture fixture;
  config::Config cfg = make_config(fixture.dir);
  cfg.cache_ttl_seconds = 60;
  cfg.cache_max_bytes = 1024 * 1024;
  cfg.watch_enabled = true;
  cfg.watch_invalidate_cache = true;

  server::Server instance{cfg};
  instance.start();

  const std::string first =
    round_trip(instance.port(), "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(first.find("hello world") != std::string::npos);

  {
    std::ofstream out(fixture.file, std::ios::binary | std::ios::trunc);
    out << "changed body";
  }

  std::string second;
  for (int attempt = 0; attempt < 40; ++attempt) {
    second =
      round_trip(instance.port(), "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    if (second.find("changed body") != std::string::npos) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
  }
  REQUIRE(second.find("changed body") != std::string::npos);

  instance.stop();
}

TEST_CASE("server: exposes the statistics endpoint and page", "[server]") {
  const Fixture fixture;
  const std::filesystem::path page = fixture.dir / "stats.html";
  {
    std::ofstream out(page, std::ios::binary);
    out << "<html>stats page</html>";
  }

  config::Config cfg = make_config(fixture.dir);
  cfg.stats_enabled = true;
  cfg.stats_page_file = page.string();

  server::Server instance{cfg};
  instance.start();

  const std::string json =
    round_trip(instance.port(), "GET /statistics HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(json.starts_with("HTTP/1.1 200"));
  REQUIRE(json.find("application/json") != std::string::npos);
  REQUIRE(json.find("\"cache\"") != std::string::npos);
  REQUIRE(json.find("\"hit_ratio\"") != std::string::npos);

  const std::string html = round_trip(
    instance.port(), "GET /statistics.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(html.starts_with("HTTP/1.1 200"));
  REQUIRE(html.find("stats page") != std::string::npos);

  instance.stop();
}

TEST_CASE("server: serves a cached file after it is removed", "[server]") {
  const Fixture fixture;
  config::Config cfg = make_config(fixture.dir);
  cfg.cache_ttl_seconds = 60;
  cfg.cache_max_bytes = 1024 * 1024;

  server::Server instance{cfg};
  instance.start();

  const std::string first =
    round_trip(instance.port(), "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(first.find("hello world") != std::string::npos);

  std::filesystem::remove(fixture.file);

  const std::string second =
    round_trip(instance.port(), "GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
  REQUIRE(second.starts_with("HTTP/1.1 200"));
  REQUIRE(second.find("hello world") != std::string::npos);

  instance.stop();
}

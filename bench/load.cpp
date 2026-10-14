#include "bench.hpp"
#include "config/config.hpp"
#include "http/http.hpp"
#include "io/file.hpp"
#include "net/socket.hpp"
#include "server/server.hpp"
#include "support.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace config = web_server::config;
namespace http = web_server::http;
namespace io = web_server::io;
namespace net = web_server::net;
namespace server = web_server::server;

namespace bench {

namespace {

struct ClientResult {
  std::size_t requests = 0;
  std::size_t errors = 0;
  std::size_t bytes = 0;
  std::vector<double> latencies;
};

std::string make_request(bool keep_alive) {
  std::string request = "GET / HTTP/1.1\r\nHost: localhost\r\n";
  request += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
  request += "\r\n";
  return request;
}

bool send_all(net::Socket& connection, std::string_view data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const std::size_t count = net::send(connection, std::as_bytes(std::span{data}.subspan(sent)));
    if (count == 0) {
      return false;
    }
    sent += count;
  }
  return true;
}

// Reads exactly one HTTP/1.1 response; returns bytes read (0 on error).
std::size_t read_response(net::Socket& connection) {
  std::array<std::byte, 8192> chunk{};
  std::string buffer;

  std::size_t header_end = std::string::npos;
  while (header_end == std::string::npos) {
    const std::size_t count = net::receive(connection, chunk);
    if (count == 0) {
      return 0;
    }
    buffer.append(reinterpret_cast<const char*>(chunk.data()), count);
    header_end = buffer.find("\r\n\r\n");
  }

  const std::vector<http::Header> headers = http::parse_headers(buffer.substr(0, header_end));
  const std::size_t content_length = http::content_length(headers).value_or(0);
  std::size_t have_body = buffer.size() - (header_end + 4);
  while (have_body < content_length) {
    const std::size_t count = net::receive(connection, chunk);
    if (count == 0) {
      break;
    }
    have_body += count;
  }
  return buffer.size() - (header_end + 4 - have_body);
}

void client_worker(std::uint16_t port, const LoadOptions& options, ClientResult& result) {
  const std::string request = make_request(options.keep_alive);

  if (options.keep_alive) {
    net::Socket connection = net::connect_tcp("127.0.0.1", port);
    for (std::size_t i = 0; i < options.requests; ++i) {
      const auto start = std::chrono::steady_clock::now();
      if (!send_all(connection, request)) {
        ++result.errors;
        break;
      }
      const std::size_t bytes = read_response(connection);
      const double latency = seconds_since(start);
      if (bytes == 0) {
        ++result.errors;
        break;
      }
      ++result.requests;
      result.bytes += bytes;
      result.latencies.push_back(latency);
    }
    return;
  }

  for (std::size_t i = 0; i < options.requests; ++i) {
    const auto start = std::chrono::steady_clock::now();
    net::Socket connection = net::connect_tcp("127.0.0.1", port);
    if (!send_all(connection, request)) {
      ++result.errors;
      continue;
    }
    const std::size_t bytes = read_response(connection);
    const double latency = seconds_since(start);
    if (bytes == 0) {
      ++result.errors;
      continue;
    }
    ++result.requests;
    result.bytes += bytes;
    result.latencies.push_back(latency);
  }
}

}  // namespace

void run_load_benchmark(const LoadOptions& options) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "web_server_bench";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  io::write_text(dir / "index.html", std::string(options.file_size, 'a'));

  config::Config config;
  config.host = "127.0.0.1";
  config.port = 0;
  config.threads = 4;
  config.read_timeout_seconds = 30;
  config.keep_alive_timeout_seconds = 30;
  config.routes = {{"/", dir.string()}};
  config.cache_ttl_seconds = options.cache ? 60 : 0;
  config.cache_max_bytes = 64ull * 1024 * 1024;

  server::Server server{config};
  server.start();
  const std::uint16_t port = server.port();

  std::vector<ClientResult> results(options.clients);
  std::vector<std::thread> threads;
  threads.reserve(options.clients);

  const auto start = std::chrono::steady_clock::now();
  for (std::size_t i = 0; i < options.clients; ++i) {
    threads.emplace_back([&, i] { client_worker(port, options, results[i]); });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  const double elapsed = seconds_since(start);

  server.stop();

  std::size_t requests = 0;
  std::size_t errors = 0;
  std::size_t bytes = 0;
  std::vector<double> latencies;
  for (ClientResult& result : results) {
    requests += result.requests;
    errors += result.errors;
    bytes += result.bytes;
    latencies.insert(latencies.end(), result.latencies.begin(), result.latencies.end());
  }

  const Statistics stats = summarize(latencies);
  const double throughput = elapsed > 0 ? static_cast<double>(requests) / elapsed : 0.0;
  const double mbps = elapsed > 0 ? static_cast<double>(bytes) / elapsed / (1024.0 * 1024.0) : 0.0;

  std::printf("load benchmark:\n");
  std::printf("  clients=%zu requests/client=%zu keep-alive=%s cache=%s file=%zuB\n",
              options.clients, options.requests, options.keep_alive ? "yes" : "no",
              options.cache ? "yes" : "no", options.file_size);
  std::printf("  requests   : %zu (%zu errors)\n", requests, errors);
  std::printf("  elapsed    : %.3f s\n", elapsed);
  std::printf("  throughput : %.0f req/s   %.1f MB/s\n", throughput, mbps);
  std::printf("  latency    : p50=%.3f ms  p90=%.3f ms  p99=%.3f ms  min=%.3f ms  max=%.3f ms\n",
              stats.p50 * 1e3, stats.p90 * 1e3, stats.p99 * 1e3, stats.min * 1e3, stats.max * 1e3);

  std::filesystem::remove_all(dir);
}

}  // namespace bench

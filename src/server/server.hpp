#pragma once

#include "cache/manager.hpp"
#include "config/config.hpp"
#include "http/http.hpp"
#include "net/socket.hpp"
#include "stats/reporter.hpp"
#include "stats/stats.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace web_server::server {

// Accepts connections and answers them based on the configured routes.
class Server {
public:
  explicit Server(config::Config config);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  // Binds the listening socket and serves in the background.
  void start();

  // Requests shutdown and waits for the background worker to finish.
  void stop();

  // Serves on the calling thread until stop() is requested.
  void run();

  [[nodiscard]] bool running() const noexcept;

  // The port the server is actually bound to (useful when configured with 0).
  [[nodiscard]] std::uint16_t port() const noexcept;

private:
  void accept_loop();
  void worker_loop();
  void enqueue(net::Socket connection);
  void handle_connection(net::Socket connection);
  int respond(net::Socket& connection, const http::Request& request, bool keep_alive);
  [[nodiscard]] stats::Snapshot snapshot() const;
  void count_response(int status) noexcept;

  config::Config config_;
  cache::CacheManager cache_manager_;
  stats::Counters counters_;
  std::unique_ptr<stats::Reporter> reporter_;

  net::Socket listener_;
  std::atomic<bool> running_{false};
  std::uint16_t port_ = 0;

  std::thread acceptor_;
  std::vector<std::thread> workers_;

  std::mutex mutex_;
  std::condition_variable queue_cv_;
  std::condition_variable stop_cv_;
  std::deque<net::Socket> queue_;
};

}  // namespace web_server::server

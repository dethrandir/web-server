#pragma once

#include "config/config.hpp"
#include "net/socket.hpp"

#include <atomic>
#include <thread>

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

private:
  void serve_forever();
  void handle_connection(net::Socket connection);

  config::Config config_;
  net::Socket listener_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

}  // namespace web_server::server

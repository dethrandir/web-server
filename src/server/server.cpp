#include "server/server.hpp"

#include <stdexcept>
#include <utility>

namespace web_server::server {

Server::Server(config::Config config) : config_(std::move(config)) {}

Server::~Server() {
  // TODO: ensure the worker thread is stopped and joined before destruction.
}

// TODO: implement
void Server::start() {
  throw std::logic_error("web_server::server::Server::start is not implemented yet");
}

// TODO: implement
void Server::stop() {
  throw std::logic_error("web_server::server::Server::stop is not implemented yet");
}

// TODO: implement
void Server::run() {
  throw std::logic_error("web_server::server::Server::run is not implemented yet");
}

bool Server::running() const noexcept {
  return running_.load();
}

// TODO: implement
void Server::serve_forever() {
  throw std::logic_error("web_server::server::Server::serve_forever is not implemented yet");
}

// TODO: implement
void Server::handle_connection(net::Socket) {
  throw std::logic_error("web_server::server::Server::handle_connection is not implemented yet");
}

}  // namespace web_server::server

#include "socket.hpp"

#include <stdexcept>
#include <unistd.h>
#include <utility>

namespace web_server::net {

Socket::Socket(int handle) noexcept : handle_(handle) {}

Socket::~Socket() {
  close();
}

Socket::Socket(Socket&& other) noexcept : handle_(std::exchange(other.handle_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, -1);
  }
  return *this;
}

bool Socket::valid() const noexcept {
  return handle_ >= 0;
}

int Socket::handle() const noexcept {
  return handle_;
}

void Socket::close() noexcept {
  if (handle_ >= 0) {
    ::close(handle_);
    handle_ = -1;
  }
}

// TODO: implement
Socket listen_tcp(std::string_view, std::uint16_t, int) {
  throw std::logic_error("web_server::net::listen_tcp is not implemented yet");
}

// TODO: implement
void connect(Socket&, std::string_view, std::uint16_t) {
  throw std::logic_error("web_server::net::connect is not implemented yet");
}

// TODO: implement
Socket accept(const Socket&) {
  throw std::logic_error("web_server::net::accept is not implemented yet");
}

// TODO: implement
std::size_t send(Socket&, std::span<const std::byte>) {
  throw std::logic_error("web_server::net::send is not implemented yet");
}

// TODO: implement
std::size_t receive(Socket&, std::span<std::byte>) {
  throw std::logic_error("web_server::net::receive is not implemented yet");
}

// TODO: implement
void set_reuse_address(Socket&, bool) {
  throw std::logic_error("web_server::net::set_reuse_address is not implemented yet");
}

}  // namespace web_server::net

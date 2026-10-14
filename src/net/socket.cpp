#include "socket.hpp"

#include <cerrno>
#include <chrono>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace web_server::net {

namespace {

struct AddrInfoDeleter {
  void operator()(addrinfo* info) const noexcept { ::freeaddrinfo(info); }
};

using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

AddrInfoPtr resolve(std::string_view host, std::uint16_t port, bool passive) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = passive ? AI_PASSIVE : 0;

  const std::string node{host};
  const std::string service = std::to_string(port);

  addrinfo* result = nullptr;
  const int rc =
    ::getaddrinfo(host.empty() ? nullptr : node.c_str(), service.c_str(), &hints, &result);
  if (rc != 0) {
    throw std::runtime_error("failed to resolve " + node + ":" + service + ": " +
                             ::gai_strerror(rc));
  }
  return AddrInfoPtr{result};
}

std::string endpoint(std::string_view host, std::uint16_t port) {
  return std::string{host} + ":" + std::to_string(port);
}

void set_timeout(int handle, int option, std::chrono::seconds timeout) {
  timeval value{};
  value.tv_sec = static_cast<time_t>(timeout.count());
  value.tv_usec = 0;
  if (::setsockopt(handle, SOL_SOCKET, option, &value, sizeof(value)) != 0) {
    throw std::system_error(errno, std::generic_category(), "setsockopt(timeout)");
  }
}

}  // namespace

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

Socket listen_tcp(std::string_view host, std::uint16_t port, int backlog) {
  const AddrInfoPtr addresses = resolve(host, port, /*passive=*/true);
  std::string last_error = "no usable address found";

  for (const addrinfo* ai = addresses.get(); ai != nullptr; ai = ai->ai_next) {
    try {
      Socket socket{::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)};
      if (!socket.valid()) {
        throw std::system_error(errno, std::generic_category(), "socket");
      }
      set_reuse_address(socket, true);
      if (::bind(socket.handle(), ai->ai_addr, ai->ai_addrlen) != 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
      }
      if (::listen(socket.handle(), backlog) != 0) {
        throw std::system_error(errno, std::generic_category(), "listen");
      }
      return socket;
    } catch (const std::exception& e) {
      last_error = e.what();
    }
  }

  throw std::runtime_error("failed to listen on " + endpoint(host, port) + ": " + last_error);
}

Socket connect_tcp(std::string_view host, std::uint16_t port) {
  const AddrInfoPtr addresses = resolve(host, port, /*passive=*/false);
  std::string last_error = "no usable address found";

  for (const addrinfo* ai = addresses.get(); ai != nullptr; ai = ai->ai_next) {
    Socket socket{::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)};
    if (!socket.valid()) {
      last_error = std::system_error(errno, std::generic_category(), "socket").what();
      continue;
    }
    if (::connect(socket.handle(), ai->ai_addr, ai->ai_addrlen) == 0) {
      return socket;
    }
    last_error = std::system_error(errno, std::generic_category(), "connect").what();
  }

  throw std::runtime_error("failed to connect to " + endpoint(host, port) + ": " + last_error);
}

Socket accept(const Socket& server) {
  int fd = -1;
  do {
    fd = ::accept(server.handle(), nullptr, nullptr);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    throw std::system_error(errno, std::generic_category(), "accept");
  }
  return Socket{fd};
}

std::size_t send(Socket& socket, std::span<const std::byte> data) {
  std::ptrdiff_t written = -1;
  do {
    written = ::send(socket.handle(), data.data(), data.size(), MSG_NOSIGNAL);
  } while (written < 0 && errno == EINTR);

  if (written < 0) {
    throw std::system_error(errno, std::generic_category(), "send");
  }
  return static_cast<std::size_t>(written);
}

std::size_t receive(Socket& socket, std::span<std::byte> buffer) {
  std::ptrdiff_t read = -1;
  do {
    read = ::recv(socket.handle(), buffer.data(), buffer.size(), 0);
  } while (read < 0 && errno == EINTR);

  if (read < 0) {
    throw std::system_error(errno, std::generic_category(), "recv");
  }
  return static_cast<std::size_t>(read);
}

void set_reuse_address(Socket& socket, bool enabled) {
  const int value = enabled ? 1 : 0;
  if (::setsockopt(socket.handle(), SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) != 0) {
    throw std::system_error(errno, std::generic_category(), "setsockopt(SO_REUSEADDR)");
  }
}

void set_receive_timeout(Socket& socket, std::chrono::seconds timeout) {
  set_timeout(socket.handle(), SO_RCVTIMEO, timeout);
}

void set_send_timeout(Socket& socket, std::chrono::seconds timeout) {
  set_timeout(socket.handle(), SO_SNDTIMEO, timeout);
}

std::uint16_t local_port(const Socket& socket) {
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (::getsockname(socket.handle(), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    throw std::system_error(errno, std::generic_category(), "getsockname");
  }

  if (address.ss_family == AF_INET) {
    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&address);
    return ntohs(ipv4->sin_port);
  }
  if (address.ss_family == AF_INET6) {
    const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&address);
    return ntohs(ipv6->sin6_port);
  }
  throw std::runtime_error("unsupported address family for local_port");
}

}  // namespace web_server::net

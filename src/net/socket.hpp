#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace web_server::net {

// RAII wrapper around a native socket descriptor.
class Socket {
public:
  Socket() noexcept = default;
  explicit Socket(int handle) noexcept;
  ~Socket();

  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] int handle() const noexcept;
  void close() noexcept;

private:
  int handle_ = -1;
};

// Creates a TCP socket bound to host:port and put into the listening state.
[[nodiscard]] Socket listen_tcp(std::string_view host, std::uint16_t port, int backlog = 128);

// Creates a TCP socket connected to host:port.
[[nodiscard]] Socket connect_tcp(std::string_view host, std::uint16_t port);

// Accepts a pending connection from a listening socket.
[[nodiscard]] Socket accept(const Socket& server);

// Sends data, returning the number of bytes written.
std::size_t send(Socket& socket, std::span<const std::byte> data);

// Receives data into buffer, returning the number of bytes read (0 on EOF).
std::size_t receive(Socket& socket, std::span<std::byte> buffer);

// Enables or disables SO_REUSEADDR on the socket.
void set_reuse_address(Socket& socket, bool enabled);

// Sets the receive/send timeout (SO_RCVTIMEO/SO_SNDTIMEO). Zero clears it.
void set_receive_timeout(Socket& socket, std::chrono::seconds timeout);

void set_send_timeout(Socket& socket, std::chrono::seconds timeout);

// Returns the local port the socket is bound to.
[[nodiscard]] std::uint16_t local_port(const Socket& socket);

}  // namespace web_server::net

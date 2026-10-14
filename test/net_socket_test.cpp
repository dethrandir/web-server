#include "net/socket.hpp"

#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace net = web_server::net;

TEST_CASE("net: loopback send/receive round trip", "[net]") {
  net::Socket server = net::listen_tcp("127.0.0.1", 0);
  REQUIRE(server.valid());

  const std::uint16_t port = net::local_port(server);
  REQUIRE(port != 0);

  std::string received;
  std::thread worker([&] {
    net::Socket client = net::accept(server);
    std::vector<std::byte> buffer(64);
    const std::size_t count = net::receive(client, buffer);
    received.assign(reinterpret_cast<const char*>(buffer.data()), count);
    const std::string reply = "pong";
    net::send(client, std::as_bytes(std::span{reply}));
  });

  net::Socket conn = net::connect_tcp("127.0.0.1", port);
  const std::string message = "ping";
  net::send(conn, std::as_bytes(std::span{message}));

  std::vector<std::byte> buffer(64);
  const std::size_t count = net::receive(conn, buffer);
  const std::string reply(reinterpret_cast<const char*>(buffer.data()), count);
  worker.join();

  REQUIRE(received == "ping");
  REQUIRE(reply == "pong");
}

TEST_CASE("net: socket close and move semantics", "[net]") {
  net::Socket server = net::listen_tcp("127.0.0.1", 0);
  REQUIRE(server.valid());
  const int handle = server.handle();

  net::Socket moved = std::move(server);
  REQUIRE(moved.valid());
  REQUIRE(moved.handle() == handle);
  REQUIRE_FALSE(server.valid());

  moved.close();
  REQUIRE_FALSE(moved.valid());
}

TEST_CASE("net: sets socket timeouts", "[net]") {
  net::Socket server = net::listen_tcp("127.0.0.1", 0);
  REQUIRE_NOTHROW(net::set_receive_timeout(server, std::chrono::seconds{1}));
  REQUIRE_NOTHROW(net::set_send_timeout(server, std::chrono::seconds{1}));
}

TEST_CASE("net: connecting to a refused port throws", "[net]") {
  REQUIRE_THROWS(net::connect_tcp("127.0.0.1", 1));
}

#include "server/server.hpp"

#include <catch_amalgamated.hpp>

namespace server = web_server::server;

TEST_CASE("server: starts in a stopped state", "[server]") {
  server::Server instance{web_server::config::Config{}};
  REQUIRE_FALSE(instance.running());
}

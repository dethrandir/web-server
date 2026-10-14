#include "cache/cache.hpp"

#include <catch_amalgamated.hpp>
#include <chrono>

namespace cache = web_server::cache;

TEST_CASE("cache: starts empty with the configured budget", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{60}};
  REQUIRE(instance.size() == 0);
  REQUIRE(instance.bytes() == 0);
  REQUIRE(instance.max_bytes() == 1024);
}

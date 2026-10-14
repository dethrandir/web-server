#include "cache/cache.hpp"

#include <catch_amalgamated.hpp>
#include <chrono>
#include <cstddef>
#include <string>

namespace cache = web_server::cache;

namespace {

cache::CacheEntry make_entry(const std::string& path, std::size_t size) {
  cache::CacheEntry entry;
  entry.path = path;
  entry.content_type = "text/plain";
  entry.data.resize(size);
  return entry;
}

}  // namespace

TEST_CASE("cache: starts empty with the configured budget", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{60}};
  REQUIRE(instance.size() == 0);
  REQUIRE(instance.bytes() == 0);
  REQUIRE(instance.max_bytes() == 1024);
}

TEST_CASE("cache: stores and returns entries", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{60}};
  instance.put(make_entry("/a.txt", 10));

  REQUIRE(instance.size() == 1);
  REQUIRE(instance.bytes() == 10);

  const cache::CacheEntry* entry = instance.get("/a.txt");
  REQUIRE(entry != nullptr);
  REQUIRE(entry->data.size() == 10);
  REQUIRE(entry->content_type == "text/plain");
  REQUIRE(instance.get("/missing.txt") == nullptr);
}

TEST_CASE("cache: replacing an entry does not double count", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{60}};
  instance.put(make_entry("/a.txt", 10));
  instance.put(make_entry("/a.txt", 30));

  REQUIRE(instance.size() == 1);
  REQUIRE(instance.bytes() == 30);
}

TEST_CASE("cache: evicts the least recently used entry", "[cache]") {
  cache::Cache instance{25, std::chrono::seconds{60}};
  instance.put(make_entry("/a", 10));
  instance.put(make_entry("/b", 10));
  instance.get("/a");
  instance.put(make_entry("/c", 10));

  REQUIRE(instance.get("/a") != nullptr);
  REQUIRE(instance.get("/b") == nullptr);
  REQUIRE(instance.get("/c") != nullptr);
}

TEST_CASE("cache: refuses entries larger than the budget", "[cache]") {
  cache::Cache instance{100, std::chrono::seconds{60}};
  instance.put(make_entry("/big", 200));

  REQUIRE(instance.size() == 0);
  REQUIRE(instance.bytes() == 0);
}

TEST_CASE("cache: expires entries when the ttl elapses", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{0}};
  instance.put(make_entry("/a", 10));

  REQUIRE(instance.get("/a") == nullptr);
  REQUIRE(instance.size() == 0);
  REQUIRE(instance.bytes() == 0);
}

TEST_CASE("cache: erase and clear update the byte count", "[cache]") {
  cache::Cache instance{1024, std::chrono::seconds{60}};
  instance.put(make_entry("/a", 10));
  instance.put(make_entry("/b", 10));

  REQUIRE(instance.erase("/a"));
  REQUIRE_FALSE(instance.erase("/a"));
  REQUIRE(instance.bytes() == 10);

  instance.clear();
  REQUIRE(instance.size() == 0);
  REQUIRE(instance.bytes() == 0);
}

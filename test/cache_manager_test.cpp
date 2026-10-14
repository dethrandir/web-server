#include "cache/manager.hpp"

#include <catch_amalgamated.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace cache = web_server::cache;

namespace {

struct TempFile {
  std::filesystem::path path;

  TempFile() {
    path = std::filesystem::temp_directory_path() / "web_server_cache_manager.txt";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << "payload";
  }

  ~TempFile() { std::filesystem::remove(path); }
};

std::string as_string(const std::shared_ptr<const cache::CacheEntry>& entry) {
  return std::string{reinterpret_cast<const char*>(entry->data.data()), entry->data.size()};
}

}  // namespace

TEST_CASE("cache manager: reads files from disk", "[cache]") {
  const TempFile file;
  cache::CacheManager manager{1024 * 1024, std::chrono::seconds{60}, 1024 * 1024};

  const std::shared_ptr<const cache::CacheEntry> entry = manager.get(file.path);
  REQUIRE(entry != nullptr);
  REQUIRE(as_string(entry) == "payload");
}

TEST_CASE("cache manager: serves a cached file after it is removed", "[cache]") {
  const TempFile file;
  cache::CacheManager manager{1024 * 1024, std::chrono::seconds{60}, 1024 * 1024};

  REQUIRE(manager.get(file.path) != nullptr);
  std::filesystem::remove(file.path);

  const std::shared_ptr<const cache::CacheEntry> cached = manager.get(file.path);
  REQUIRE(cached != nullptr);
  REQUIRE(as_string(cached) == "payload");
}

TEST_CASE("cache manager: with ttl zero everything is read from disk", "[cache]") {
  const TempFile file;
  cache::CacheManager manager{1024 * 1024, std::chrono::seconds{0}, 1024 * 1024};

  REQUIRE(manager.get(file.path) != nullptr);
  std::filesystem::remove(file.path);

  REQUIRE(manager.get(file.path) == nullptr);
}

TEST_CASE("cache manager: files larger than the limit are not cached", "[cache]") {
  const TempFile file;
  cache::CacheManager manager{1024 * 1024, std::chrono::seconds{60}, 3};  // 3-byte limit

  REQUIRE(manager.get(file.path) != nullptr);
  std::filesystem::remove(file.path);

  REQUIRE(manager.get(file.path) == nullptr);  // was never cached
}

TEST_CASE("cache manager: missing files return nullptr", "[cache]") {
  cache::CacheManager manager{1024, std::chrono::seconds{60}, 1024};
  REQUIRE(manager.get("web_server_cache_manager_missing.txt") == nullptr);
}

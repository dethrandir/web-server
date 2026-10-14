#pragma once

#include "cache/cache.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>

namespace web_server::cache {

// Serves file contents from the cache when possible and from disk otherwise.
// Callers do not need to know whether a given file was cached.
class CacheManager {
public:
  CacheManager(std::size_t max_bytes, std::chrono::seconds ttl, std::size_t max_file_bytes);

  // Returns the file contents (loading and caching them on a miss). Returns
  // nullptr if the file cannot be read. When the TTL is zero, caching is disabled
  // and every call reads from disk.
  [[nodiscard]] std::shared_ptr<const CacheEntry> get(const std::filesystem::path& file);

  [[nodiscard]] CacheStats stats() const;

private:
  [[nodiscard]] std::shared_ptr<CacheEntry> load(const std::filesystem::path& file) const;

  Cache cache_;
  std::size_t max_file_bytes_;
  bool enabled_;
};

}  // namespace web_server::cache

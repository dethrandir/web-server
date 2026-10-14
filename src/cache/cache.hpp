#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace web_server::cache {

// A single file held in memory, plus the metadata needed to serve it.
struct CacheEntry {
  std::filesystem::path path;
  std::string content_type;
  std::vector<std::byte> data;
  std::filesystem::file_time_type last_write_time{};
  std::chrono::steady_clock::time_point loaded_at{};
  std::chrono::steady_clock::time_point last_used{};
};

// In-memory store of hot files, bounded by a total byte budget and a TTL.
class Cache {
public:
  Cache(std::size_t max_bytes, std::chrono::seconds ttl);

  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

  // Returns the entry if present and still fresh, otherwise nullptr.
  const CacheEntry* get(const std::filesystem::path& path);

  // Inserts or replaces an entry, evicting others to respect the byte budget.
  void put(CacheEntry entry);

  bool erase(const std::filesystem::path& path);

  void clear();

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t bytes() const noexcept;
  [[nodiscard]] std::size_t max_bytes() const noexcept;

private:
  void evict_expired();
  void evict_until_fits(std::size_t incoming_bytes);

  std::unordered_map<std::filesystem::path, CacheEntry> entries_;
  std::size_t max_bytes_;
  std::chrono::seconds ttl_;
  std::size_t bytes_ = 0;
};

}  // namespace web_server::cache

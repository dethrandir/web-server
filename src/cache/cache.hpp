#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace web_server::cache {

// A point-in-time view of the cache's activity and occupancy.
struct CacheStats {
  std::uint64_t entries = 0;
  std::uint64_t bytes = 0;
  std::uint64_t hits = 0;
  std::uint64_t misses = 0;
  std::uint64_t evictions = 0;
};

// A single file held in memory, plus the metadata needed to serve it.
struct CacheEntry {
  std::filesystem::path path;
  std::vector<std::byte> data;
  std::filesystem::file_time_type last_write_time{};
  std::chrono::steady_clock::time_point loaded_at{};
  std::chrono::steady_clock::time_point last_used{};
};

// Thread-safe, in-memory store of hot files, bounded by a byte budget and a TTL.
//
// Entries are handed out as shared_ptr<const CacheEntry>, so a reader keeps the
// data alive even if another thread evicts the entry in the meantime.
class Cache {
public:
  Cache(std::size_t max_bytes, std::chrono::seconds ttl);

  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

  // Returns the entry if present and still fresh, otherwise nullptr.
  [[nodiscard]] std::shared_ptr<const CacheEntry> get(const std::filesystem::path& path);

  // Inserts or replaces an entry, evicting others to respect the byte budget.
  // Returns the stored entry, or nullptr if it can never fit.
  std::shared_ptr<const CacheEntry> put(std::shared_ptr<CacheEntry> entry);

  bool erase(const std::filesystem::path& path);

  // Drops a cached file (same as erase, named for the watch/invalidation path).
  bool invalidate(const std::filesystem::path& path);

  // Drops every cached entry located under `directory`. Returns how many.
  std::size_t invalidate_prefix(const std::filesystem::path& directory);

  void clear();

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::size_t bytes() const;
  [[nodiscard]] std::size_t max_bytes() const noexcept;

  // Snapshot of occupancy and hit/miss/eviction counters.
  [[nodiscard]] CacheStats stats() const;

private:
  void evict_expired_locked();
  void evict_until_fits_locked(std::size_t incoming_bytes);

  mutable std::mutex mutex_;
  std::unordered_map<std::filesystem::path, std::shared_ptr<CacheEntry>> entries_;
  std::size_t max_bytes_;
  std::chrono::seconds ttl_;
  std::size_t bytes_ = 0;

  std::atomic<std::uint64_t> hits_{0};
  std::atomic<std::uint64_t> misses_{0};
  std::atomic<std::uint64_t> evictions_{0};
};

}  // namespace web_server::cache

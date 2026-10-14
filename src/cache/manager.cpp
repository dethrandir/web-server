#include "cache/manager.hpp"

#include "io/file.hpp"

#include <exception>
#include <system_error>

namespace web_server::cache {

CacheManager::CacheManager(std::size_t max_bytes, std::chrono::seconds ttl,
                           std::size_t max_file_bytes)
    : cache_(max_bytes, ttl), max_file_bytes_(max_file_bytes), enabled_(ttl.count() > 0) {}

std::shared_ptr<const CacheEntry> CacheManager::get(const std::filesystem::path& file) {
  if (enabled_) {
    if (std::shared_ptr<const CacheEntry> cached = cache_.get(file); cached != nullptr) {
      return cached;
    }
  }

  std::shared_ptr<CacheEntry> entry = load(file);
  if (entry == nullptr) {
    return nullptr;
  }

  if (enabled_ && entry->data.size() <= max_file_bytes_) {
    return cache_.put(std::move(entry));
  }
  return entry;
}

CacheStats CacheManager::stats() const {
  return cache_.stats();
}

std::shared_ptr<CacheEntry> CacheManager::load(const std::filesystem::path& file) const {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file, ec)) {
    return nullptr;
  }

  try {
    auto entry = std::make_shared<CacheEntry>();
    entry->path = file;
    entry->data = io::read_bytes(file);
    entry->last_write_time = std::filesystem::last_write_time(file, ec);
    return entry;
  } catch (const std::exception&) {
    return nullptr;
  }
}

}  // namespace web_server::cache

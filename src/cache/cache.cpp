#include "cache/cache.hpp"

#include <utility>

namespace web_server::cache {

namespace {

std::chrono::steady_clock::time_point now() {
  return std::chrono::steady_clock::now();
}

bool expired(const CacheEntry& entry, std::chrono::seconds ttl) {
  if (ttl.count() == 0) {
    return true;
  }
  return now() - entry.loaded_at >= ttl;
}

}  // namespace

Cache::Cache(std::size_t max_bytes, std::chrono::seconds ttl) : max_bytes_(max_bytes), ttl_(ttl) {}

const CacheEntry* Cache::get(const std::filesystem::path& path) {
  const auto it = entries_.find(path);
  if (it == entries_.end()) {
    return nullptr;
  }
  if (expired(it->second, ttl_)) {
    bytes_ -= it->second.data.size();
    entries_.erase(it);
    return nullptr;
  }
  it->second.last_used = now();
  return &it->second;
}

void Cache::put(CacheEntry entry) {
  const std::size_t incoming = entry.data.size();
  if (max_bytes_ == 0 || incoming > max_bytes_) {
    return;
  }

  evict_expired();

  const auto existing = entries_.find(entry.path);
  if (existing != entries_.end()) {
    bytes_ -= existing->second.data.size();
    entries_.erase(existing);
  }

  evict_until_fits(incoming);

  entry.loaded_at = now();
  entry.last_used = entry.loaded_at;
  bytes_ += incoming;

  const std::filesystem::path key = entry.path;
  entries_.insert_or_assign(key, std::move(entry));
}

bool Cache::erase(const std::filesystem::path& path) {
  const auto it = entries_.find(path);
  if (it == entries_.end()) {
    return false;
  }
  bytes_ -= it->second.data.size();
  entries_.erase(it);
  return true;
}

void Cache::clear() {
  entries_.clear();
  bytes_ = 0;
}

std::size_t Cache::size() const noexcept {
  return entries_.size();
}

std::size_t Cache::bytes() const noexcept {
  return bytes_;
}

std::size_t Cache::max_bytes() const noexcept {
  return max_bytes_;
}

void Cache::evict_expired() {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (expired(it->second, ttl_)) {
      bytes_ -= it->second.data.size();
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

void Cache::evict_until_fits(std::size_t incoming) {
  while (bytes_ + incoming > max_bytes_ && !entries_.empty()) {
    auto victim = entries_.begin();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->second.last_used < victim->second.last_used) {
        victim = it;
      }
    }
    bytes_ -= victim->second.data.size();
    entries_.erase(victim);
  }
}

}  // namespace web_server::cache

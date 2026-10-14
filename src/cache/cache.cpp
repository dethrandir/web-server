#include "cache/cache.hpp"

#include <stdexcept>

namespace web_server::cache {

Cache::Cache(std::size_t max_bytes, std::chrono::seconds ttl) : max_bytes_(max_bytes), ttl_(ttl) {}

// TODO: implement (look up, refresh last_used, drop if expired)
const CacheEntry* Cache::get(const std::filesystem::path&) {
  throw std::logic_error("web_server::cache::Cache::get is not implemented yet");
}

// TODO: implement (insert/replace, update bytes_, evict to fit)
void Cache::put(CacheEntry) {
  throw std::logic_error("web_server::cache::Cache::put is not implemented yet");
}

// TODO: implement
bool Cache::erase(const std::filesystem::path&) {
  throw std::logic_error("web_server::cache::Cache::erase is not implemented yet");
}

// TODO: implement
void Cache::clear() {
  throw std::logic_error("web_server::cache::Cache::clear is not implemented yet");
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

// TODO: implement
void Cache::evict_expired() {
  throw std::logic_error("web_server::cache::Cache::evict_expired is not implemented yet");
}

// TODO: implement
void Cache::evict_until_fits(std::size_t) {
  throw std::logic_error("web_server::cache::Cache::evict_until_fits is not implemented yet");
}

}  // namespace web_server::cache

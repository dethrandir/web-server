#include "bench.hpp"
#include "cache/cache.hpp"
#include "http/http.hpp"
#include "support.hpp"
#include "util/string.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>

namespace http = web_server::http;
namespace cache = web_server::cache;
namespace util = web_server::util;

namespace bench {

void run_micro_benchmarks(const MicroOptions& options) {
  std::printf("micro benchmarks:\n");

  const std::string request =
    "GET /hello HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello";
  run_micro("http::parse_request", options.iterations, [&] {
    const http::Request parsed = http::parse_request(request);
    do_not_optimize(parsed.method);
  });

  const std::string headers = "Host: example.com\r\nAccept: */*\r\nContent-Length: 5\r\n";
  run_micro("http::parse_headers", options.iterations, [&] {
    const std::vector<http::Header> parsed = http::parse_headers(headers);
    do_not_optimize(parsed.size());
  });

  cache::Cache cache{1 << 20, std::chrono::seconds{60}};
  auto entry = std::make_shared<cache::CacheEntry>();
  entry->path = "/a";
  entry->data.resize(64);
  cache.put(entry);

  run_micro("cache::get (hit)", options.iterations, [&] {
    const std::shared_ptr<const cache::CacheEntry> found = cache.get("/a");
    do_not_optimize(found != nullptr);
  });

  run_micro("cache::put", options.iterations, [&] {
    auto fresh = std::make_shared<cache::CacheEntry>();
    fresh->path = "/b";
    fresh->data.resize(64);
    do_not_optimize(cache.put(std::move(fresh)) != nullptr);
  });

  run_micro("util::split", options.iterations, [&] {
    const std::vector<std::string> parts = util::split("a,b,c,d,e", ',');
    do_not_optimize(parts.size());
  });

  run_micro("util::trim", options.iterations, [&] {
    const std::string_view trimmed = util::trim("   hello world   ");
    do_not_optimize(trimmed.size());
  });
}

}  // namespace bench

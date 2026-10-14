#include "server/server.hpp"

#include "io/file.hpp"
#include "log/log.hpp"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace web_server::server {

namespace {

bool iequals(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i]))) {
      return false;
    }
  }
  return true;
}

std::string strip_query(std::string_view target) {
  const std::size_t pos = target.find('?');
  return std::string{pos == std::string_view::npos ? target : target.substr(0, pos)};
}

std::string percent_decode(std::string_view text) {
  const auto hex_value = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
      return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
      return ch - 'A' + 10;
    }
    return -1;
  };

  std::string result;
  result.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 2 < text.size()) {
      const int high = hex_value(text[i + 1]);
      const int low = hex_value(text[i + 2]);
      if (high >= 0 && low >= 0) {
        result.push_back(static_cast<char>((high << 4) | low));
        i += 2;
        continue;
      }
    }
    result.push_back(text[i]);
  }
  return result;
}

std::string http_date() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
  ::gmtime_r(&now, &utc);
  char buffer[64];
  std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &utc);
  return buffer;
}

std::string_view content_type_for(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  for (char& ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  if (extension == ".html" || extension == ".htm")
    return "text/html; charset=utf-8";
  if (extension == ".css")
    return "text/css; charset=utf-8";
  if (extension == ".js")
    return "text/javascript; charset=utf-8";
  if (extension == ".json")
    return "application/json";
  if (extension == ".txt")
    return "text/plain; charset=utf-8";
  if (extension == ".xml")
    return "application/xml";
  if (extension == ".png")
    return "image/png";
  if (extension == ".jpg" || extension == ".jpeg")
    return "image/jpeg";
  if (extension == ".gif")
    return "image/gif";
  if (extension == ".svg")
    return "image/svg+xml";
  if (extension == ".ico")
    return "image/x-icon";
  if (extension == ".pdf")
    return "application/pdf";
  if (extension == ".wasm")
    return "application/wasm";
  if (extension == ".woff")
    return "font/woff";
  if (extension == ".woff2")
    return "font/woff2";
  return "application/octet-stream";
}

std::string docs_json(const config::Config& cfg, double uptime_seconds) {
  std::error_code ec;
  nlohmann::json routes = nlohmann::json::array();
  for (const config::Route& route : cfg.routes) {
    routes.push_back({
      {"path", route.path},
      {"target", route.target},
      {"type", std::filesystem::is_directory(route.target, ec) ? "directory" : "file"},
    });
  }

  nlohmann::json endpoints = nlohmann::json::array();
  endpoints.push_back(cfg.docs_path);
  if (!cfg.docs_page.empty()) {
    endpoints.push_back(cfg.docs_page);
  }
  if (cfg.stats_enabled) {
    endpoints.push_back(cfg.stats_path);
    endpoints.push_back(cfg.stats_page);
  }

  nlohmann::json json;
  json["program"] = "web-server";
  json["version"] = WEB_SERVER_VERSION;
  json["uptime_seconds"] = uptime_seconds;
  json["routes"] = std::move(routes);
  json["endpoints"] = std::move(endpoints);
  return json.dump(2);
}

std::string error_body(int status) {
  const std::string phrase{http::reason_phrase(status)};
  const std::string code = std::to_string(status);
  return "<!doctype html><html><head><title>" + code + " " + phrase + "</title></head><body><h1>" +
         code + " " + phrase + "</h1></body></html>";
}

std::string build_response(int status, std::size_t content_length, std::string_view content_type,
                           std::string_view body, bool include_body, bool keep_alive,
                           std::span<const http::Header> extra = {}) {
  std::string response;
  response.reserve(body.size() + 256);
  response += "HTTP/1.1 ";
  response += std::to_string(status);
  response += ' ';
  response += http::reason_phrase(status);
  response += "\r\nDate: ";
  response += http_date();
  response += "\r\nServer: web-server\r\n";
  if (!content_type.empty()) {
    response += "Content-Type: ";
    response += content_type;
    response += "\r\n";
  }
  response += "Content-Length: ";
  response += std::to_string(content_length);
  response += "\r\nConnection: ";
  response += keep_alive ? "keep-alive" : "close";
  response += "\r\n";
  for (const http::Header& header : extra) {
    response += header.name;
    response += ": ";
    response += header.value;
    response += "\r\n";
  }
  response += "\r\n";
  if (include_body) {
    response.append(body);
  }
  return response;
}

bool send_all(net::Socket& connection, std::string_view data, stats::Counters& counters) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    try {
      const std::size_t count = net::send(connection, std::as_bytes(std::span{data}.subspan(sent)));
      if (count == 0) {
        return false;
      }
      sent += count;
      counters.bytes_sent.fetch_add(count, std::memory_order_relaxed);
    } catch (const std::exception&) {
      return false;
    }
  }
  return true;
}

bool recv_into(net::Socket& connection, std::string& buffer, stats::Counters& counters) {
  std::byte chunk[4096];
  try {
    const std::size_t count = net::receive(connection, chunk);
    if (count == 0) {
      return false;
    }
    buffer.append(reinterpret_cast<const char*>(chunk), count);
    counters.bytes_received.fetch_add(count, std::memory_order_relaxed);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

struct ReadOutcome {
  enum class Kind { Complete, Closed, Error };
  Kind kind = Kind::Closed;
  int status = 0;
  http::Request request;
};

ReadOutcome read_one_request(net::Socket& connection, std::string& buffer, std::size_t max_header,
                             std::size_t max_body, stats::Counters& counters) {
  while (true) {
    const std::size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      if (buffer.size() > max_header) {
        return {ReadOutcome::Kind::Error, 431, {}};
      }
      if (!recv_into(connection, buffer, counters)) {
        return {ReadOutcome::Kind::Closed, 0, {}};
      }
      continue;
    }
    if (header_end > max_header) {
      return {ReadOutcome::Kind::Error, 431, {}};
    }

    const std::string_view head{buffer.data(), header_end};
    const std::size_t line_end = head.find("\r\n");
    const std::string_view header_block =
      (line_end == std::string_view::npos) ? std::string_view{} : head.substr(line_end + 2);
    const std::size_t body_length =
      http::content_length(http::parse_headers(header_block)).value_or(0);
    if (body_length > max_body) {
      return {ReadOutcome::Kind::Error, 413, {}};
    }

    const std::size_t total = header_end + 4 + body_length;
    while (buffer.size() < total) {
      if (!recv_into(connection, buffer, counters)) {
        return {ReadOutcome::Kind::Closed, 0, {}};
      }
    }

    const std::string raw = buffer.substr(0, total);
    buffer.erase(0, total);
    try {
      return {ReadOutcome::Kind::Complete, 0, http::parse_request(raw)};
    } catch (const std::exception&) {
      return {ReadOutcome::Kind::Error, 400, {}};
    }
  }
}

bool wants_keep_alive(const http::Request& request) {
  const std::optional<std::string_view> connection =
    http::header_value(request.headers, "Connection");
  if (connection.has_value()) {
    if (iequals(*connection, "close")) {
      return false;
    }
    if (iequals(*connection, "keep-alive")) {
      return true;
    }
  }
  return request.version == "HTTP/1.1";
}

// RAII guard around the active-connection gauge.
struct ActiveConnection {
  std::atomic<std::uint64_t>& counter;
  explicit ActiveConnection(std::atomic<std::uint64_t>& target) : counter(target) {
    counter.fetch_add(1, std::memory_order_relaxed);
  }
  ~ActiveConnection() { counter.fetch_sub(1, std::memory_order_relaxed); }
};

}  // namespace

Server::Server(config::Config config)
    : config_(std::make_shared<const config::Config>(std::move(config))),
      cache_manager_(config_.load()->cache_max_bytes,
                     std::chrono::seconds{config_.load()->cache_ttl_seconds},
                     config_.load()->cache_max_file_bytes) {}

Server::~Server() {
  stop();
}

std::shared_ptr<const config::Config> Server::config() const {
  return config_.load();
}

void Server::start() {
  if (running_.load()) {
    return;
  }

  const std::shared_ptr<const config::Config> cfg = config();

  listener_ = net::listen_tcp(cfg->host, cfg->port);
  port_ = net::local_port(listener_);
  // Wake accept() periodically so stop() can interrupt it without closing the
  // socket from another thread (closing does not reliably wake a blocked accept).
  net::set_receive_timeout(listener_, std::chrono::seconds{1});
  started_at_ = std::chrono::steady_clock::now();
  running_.store(true);

  std::size_t worker_count = cfg->threads;
  if (worker_count == 0) {
    worker_count = std::thread::hardware_concurrency();
    if (worker_count == 0) {
      worker_count = 4;
    }
  }
  counters_.workers_total.store(worker_count, std::memory_order_relaxed);

  workers_.reserve(worker_count);
  for (std::size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
  acceptor_ = std::thread([this] { accept_loop(); });

  if (cfg->stats_enabled) {
    reporter_ = std::make_unique<stats::Reporter>(
      [this] { return snapshot(); }, std::filesystem::path{cfg->stats_file},
      std::chrono::seconds{cfg->stats_interval_seconds});
    reporter_->start();
  }

  setup_watchers(*cfg);

  log::info("listening on {}:{} with {} worker(s)", cfg->host, port_, worker_count);
}

void Server::stop() {
  if (!running_.exchange(false)) {
    return;
  }

  if (watcher_ != nullptr) {
    watcher_->stop();
  }
  if (reporter_ != nullptr) {
    reporter_->stop();
  }

  queue_cv_.notify_all();

  if (acceptor_.joinable()) {
    acceptor_.join();
  }
  for (std::thread& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  workers_.clear();

  listener_.close();

  {
    std::lock_guard lock{mutex_};
    queue_.clear();
  }
  stop_cv_.notify_all();
}

void Server::run() {
  start();
  std::unique_lock lock{mutex_};
  stop_cv_.wait(lock, [this] { return !running_.load(); });
}

bool Server::running() const noexcept {
  return running_.load();
}

std::uint16_t Server::port() const noexcept {
  return port_;
}

void Server::reload(const config::Config& updated) {
  const std::shared_ptr<const config::Config> current = config();

  config::Config applied = updated;
  // Restart-only fields keep their running values.
  applied.host = current->host;
  applied.port = current->port;
  applied.threads = current->threads;
  applied.log_path = current->log_path;
  applied.cache_ttl_seconds = current->cache_ttl_seconds;
  applied.cache_max_bytes = current->cache_max_bytes;
  applied.cache_max_file_bytes = current->cache_max_file_bytes;
  applied.stats_enabled = current->stats_enabled;
  applied.stats_path = current->stats_path;
  applied.stats_page = current->stats_page;
  applied.stats_page_file = current->stats_page_file;
  applied.stats_file = current->stats_file;
  applied.stats_interval_seconds = current->stats_interval_seconds;
  applied.docs_enabled = current->docs_enabled;
  applied.docs_path = current->docs_path;
  applied.docs_page = current->docs_page;
  applied.docs_page_file = current->docs_page_file;
  applied.watch_enabled = current->watch_enabled;
  applied.watch_config_reload = current->watch_config_reload;
  applied.watch_invalidate_cache = current->watch_invalidate_cache;

  if (updated.host != current->host || updated.port != current->port ||
      updated.threads != current->threads) {
    log::warn("config reload: host/port/threads changed; restart required");
  }
  if (updated.cache_ttl_seconds != current->cache_ttl_seconds ||
      updated.cache_max_bytes != current->cache_max_bytes ||
      updated.cache_max_file_bytes != current->cache_max_file_bytes) {
    log::warn("config reload: cache settings changed; restart required");
  }
  if (updated.stats_enabled != current->stats_enabled ||
      updated.stats_path != current->stats_path || updated.stats_page != current->stats_page ||
      updated.stats_file != current->stats_file ||
      updated.stats_interval_seconds != current->stats_interval_seconds) {
    log::warn("config reload: stats settings changed; restart required");
  }

  const std::size_t route_count = applied.routes.size();
  const std::uint32_t read_timeout = applied.read_timeout_seconds;
  const std::uint32_t keep_alive_timeout = applied.keep_alive_timeout_seconds;
  config_.store(std::make_shared<const config::Config>(std::move(applied)));
  log::info("configuration reloaded: routes={} read_timeout={}s keep_alive={}s", route_count,
            read_timeout, keep_alive_timeout);
}

void Server::setup_watchers(const config::Config& cfg) {
  if (!cfg.watch_enabled || !cfg.watch_invalidate_cache || cfg.routes.empty()) {
    return;
  }

  watcher_ = std::make_unique<watch::Watcher>();
  std::error_code ec;
  for (const config::Route& route : cfg.routes) {
    if (std::filesystem::is_directory(route.target, ec)) {
      const std::filesystem::path directory = route.target;
      watcher_->watch(directory, [this, directory](const watch::Event&) {
        const std::size_t removed = cache_manager_.invalidate_prefix(directory);
        if (removed > 0) {
          log::debug("cache: invalidated {} file(s) under {}", removed, directory.string());
        }
      });
    } else {
      const std::filesystem::path file = route.target;
      watcher_->watch(file, [this, file](const watch::Event&) {
        if (cache_manager_.invalidate(file)) {
          log::debug("cache: invalidated {}", file.string());
        }
      });
    }
  }
  watcher_->start();
  log::info("watching {} route target(s) for changes", cfg.routes.size());
}

void Server::accept_loop() {
  while (running_.load()) {
    try {
      net::Socket connection = net::accept(listener_);
      if (!running_.load()) {
        break;
      }
      counters_.connections_accepted.fetch_add(1, std::memory_order_relaxed);
      enqueue(std::move(connection));
    } catch (const std::exception&) {
      if (!running_.load()) {
        break;
      }
    }
  }
}

void Server::enqueue(net::Socket connection) {
  {
    std::lock_guard lock{mutex_};
    queue_.push_back(std::move(connection));
    counters_.queue_length.store(queue_.size(), std::memory_order_relaxed);
  }
  queue_cv_.notify_one();
}

void Server::worker_loop() {
  while (true) {
    net::Socket connection;
    {
      std::unique_lock lock{mutex_};
      queue_cv_.wait(lock, [this] { return !running_.load() || !queue_.empty(); });
      if (queue_.empty()) {
        return;
      }
      connection = std::move(queue_.front());
      queue_.pop_front();
      counters_.queue_length.store(queue_.size(), std::memory_order_relaxed);
    }

    counters_.workers_busy.fetch_add(1, std::memory_order_relaxed);
    try {
      handle_connection(std::move(connection));
    } catch (const std::exception& e) {
      log::error("connection handling failed: {}", e.what());
    }
    counters_.workers_busy.fetch_sub(1, std::memory_order_relaxed);
  }
}

void Server::handle_connection(net::Socket connection) {
  const ActiveConnection active{counters_.connections_active};

  const std::shared_ptr<const config::Config> cfg = config();

  net::set_receive_timeout(connection, std::chrono::seconds{cfg->read_timeout_seconds});
  net::set_send_timeout(connection, std::chrono::seconds{cfg->read_timeout_seconds});

  std::string buffer;
  bool first_request = true;
  while (running_.load()) {
    if (!first_request) {
      net::set_receive_timeout(connection, std::chrono::seconds{cfg->keep_alive_timeout_seconds});
    }
    first_request = false;

    const ReadOutcome outcome =
      read_one_request(connection, buffer, cfg->max_header_bytes, cfg->max_body_bytes, counters_);
    if (outcome.kind == ReadOutcome::Kind::Closed) {
      break;
    }
    if (outcome.kind == ReadOutcome::Kind::Error) {
      const std::string body = error_body(outcome.status);
      send_all(
        connection,
        build_response(outcome.status, body.size(), "text/html; charset=utf-8", body, true, false),
        counters_);
      count_response(outcome.status);
      break;
    }

    counters_.requests.fetch_add(1, std::memory_order_relaxed);
    const bool keep_alive = wants_keep_alive(outcome.request);
    const int status = respond(connection, *cfg, outcome.request, keep_alive);
    if (status < 0) {
      break;
    }
    count_response(status);
    if (!keep_alive) {
      break;
    }
  }
}

int Server::respond(net::Socket& connection, const config::Config& cfg,
                    const http::Request& request, bool keep_alive) {
  const bool include_body = request.method != "HEAD";

  const auto send_status = [&](int status, std::string_view content_type, std::string_view body,
                               std::span<const http::Header> extra = {}) -> int {
    const std::string response =
      build_response(status, body.size(), content_type, body, include_body, keep_alive, extra);
    return send_all(connection, response, counters_) ? status : -1;
  };

  if (request.method != "GET" && request.method != "HEAD") {
    const http::Header allow{"Allow", "GET, HEAD"};
    return send_status(405, "text/html; charset=utf-8", error_body(405), std::span{&allow, 1});
  }

  const std::string request_path = percent_decode(strip_query(request.target));

  if (cfg.docs_enabled) {
    if (request_path == cfg.docs_path) {
      const double uptime =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at_).count();
      return send_status(200, "application/json", docs_json(cfg, uptime));
    }
    if (request_path == cfg.docs_page) {
      try {
        const std::string html = io::read_text(cfg.docs_page_file);
        return send_status(200, "text/html; charset=utf-8", html);
      } catch (const std::exception&) {
        return send_status(500, "text/html; charset=utf-8", error_body(500));
      }
    }
  }

  if (cfg.stats_enabled) {
    if (request_path == cfg.stats_path) {
      stats::Snapshot current = snapshot();
      if (reporter_ != nullptr) {
        const stats::Snapshot sampled = reporter_->latest();
        current.rss_bytes = sampled.rss_bytes;
        current.cpu_seconds = sampled.cpu_seconds;
        current.cpu_percent = sampled.cpu_percent;
        current.uptime_seconds = sampled.uptime_seconds;
      } else {
        current.rss_bytes = stats::read_rss_bytes();
        current.cpu_seconds = stats::read_process_cpu_seconds();
      }
      current.timestamp = stats::utc_timestamp();
      return send_status(200, "application/json", stats::to_json(current));
    }
    if (request_path == cfg.stats_page) {
      try {
        const std::string html = io::read_text(cfg.stats_page_file);
        return send_status(200, "text/html; charset=utf-8", html);
      } catch (const std::exception&) {
        return send_status(500, "text/html; charset=utf-8", error_body(500));
      }
    }
  }

  const config::Route* route = config::find_route(cfg.routes, request_path);
  if (route == nullptr) {
    return send_status(404, "text/html; charset=utf-8", error_body(404));
  }

  std::string_view rest = std::string_view{request_path}.substr(route->path.size());
  while (!rest.empty() && rest.front() == '/') {
    rest.remove_prefix(1);
  }

  std::error_code ec;
  std::filesystem::path file;
  if (std::filesystem::is_directory(route->target, ec)) {
    const std::filesystem::path relative{std::string{rest}};
    for (const std::filesystem::path& part : relative) {
      if (part == "..") {
        return send_status(403, "text/html; charset=utf-8", error_body(403));
      }
    }
    file = std::filesystem::path{route->target} / relative;
    if (std::filesystem::is_directory(file, ec)) {
      file /= "index.html";
    }
  } else {
    if (!rest.empty()) {
      return send_status(404, "text/html; charset=utf-8", error_body(404));
    }
    file = route->target;
  }

  const std::shared_ptr<const cache::CacheEntry> entry = cache_manager_.get(file);
  if (entry == nullptr) {
    return send_status(404, "text/html; charset=utf-8", error_body(404));
  }

  const std::string_view body{reinterpret_cast<const char*>(entry->data.data()),
                              entry->data.size()};
  return send_status(200, content_type_for(file), body);
}

stats::Snapshot Server::snapshot() const {
  stats::Snapshot snapshot;
  snapshot.requests = counters_.requests.load(std::memory_order_relaxed);
  snapshot.responses_2xx = counters_.responses_2xx.load(std::memory_order_relaxed);
  snapshot.responses_3xx = counters_.responses_3xx.load(std::memory_order_relaxed);
  snapshot.responses_4xx = counters_.responses_4xx.load(std::memory_order_relaxed);
  snapshot.responses_5xx = counters_.responses_5xx.load(std::memory_order_relaxed);
  snapshot.connections_accepted = counters_.connections_accepted.load(std::memory_order_relaxed);
  snapshot.connections_active = counters_.connections_active.load(std::memory_order_relaxed);
  snapshot.bytes_received = counters_.bytes_received.load(std::memory_order_relaxed);
  snapshot.bytes_sent = counters_.bytes_sent.load(std::memory_order_relaxed);
  snapshot.workers_total = counters_.workers_total.load(std::memory_order_relaxed);
  snapshot.workers_busy = counters_.workers_busy.load(std::memory_order_relaxed);
  snapshot.queue_length = counters_.queue_length.load(std::memory_order_relaxed);

  const cache::CacheStats cache = cache_manager_.stats();
  snapshot.cache_entries = cache.entries;
  snapshot.cache_bytes = cache.bytes;
  snapshot.cache_hits = cache.hits;
  snapshot.cache_misses = cache.misses;
  snapshot.cache_evictions = cache.evictions;

  const std::uint64_t lookups = snapshot.cache_hits + snapshot.cache_misses;
  if (lookups > 0) {
    snapshot.cache_hit_ratio =
      static_cast<double>(snapshot.cache_hits) / static_cast<double>(lookups);
  }
  return snapshot;
}

void Server::count_response(int status) noexcept {
  if (status >= 200 && status < 300) {
    counters_.responses_2xx.fetch_add(1, std::memory_order_relaxed);
  } else if (status >= 300 && status < 400) {
    counters_.responses_3xx.fetch_add(1, std::memory_order_relaxed);
  } else if (status >= 400 && status < 500) {
    counters_.responses_4xx.fetch_add(1, std::memory_order_relaxed);
  } else if (status >= 500) {
    counters_.responses_5xx.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace web_server::server

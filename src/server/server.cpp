#include "server/server.hpp"

#include "log/log.hpp"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
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

bool send_all(net::Socket& connection, std::string_view data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    try {
      const std::size_t count = net::send(connection, std::as_bytes(std::span{data}.subspan(sent)));
      if (count == 0) {
        return false;
      }
      sent += count;
    } catch (const std::exception&) {
      return false;
    }
  }
  return true;
}

bool recv_into(net::Socket& connection, std::string& buffer) {
  std::byte chunk[4096];
  try {
    const std::size_t count = net::receive(connection, chunk);
    if (count == 0) {
      return false;
    }
    buffer.append(reinterpret_cast<const char*>(chunk), count);
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
                             std::size_t max_body) {
  while (true) {
    const std::size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      if (buffer.size() > max_header) {
        return {ReadOutcome::Kind::Error, 431, {}};
      }
      if (!recv_into(connection, buffer)) {
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
      if (!recv_into(connection, buffer)) {
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

}  // namespace

Server::Server(config::Config config)
    : config_(std::move(config)),
      cache_manager_(config_.cache_max_bytes, std::chrono::seconds{config_.cache_ttl_seconds},
                     config_.cache_max_file_bytes) {}

Server::~Server() {
  stop();
}

void Server::start() {
  if (running_.load()) {
    return;
  }

  listener_ = net::listen_tcp(config_.host, config_.port);
  port_ = net::local_port(listener_);
  // Wake accept() periodically so stop() can interrupt it without closing the
  // socket from another thread (closing does not reliably wake a blocked accept).
  net::set_receive_timeout(listener_, std::chrono::seconds{1});
  running_.store(true);

  std::size_t worker_count = config_.threads;
  if (worker_count == 0) {
    worker_count = std::thread::hardware_concurrency();
    if (worker_count == 0) {
      worker_count = 4;
    }
  }

  workers_.reserve(worker_count);
  for (std::size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this] { worker_loop(); });
  }
  acceptor_ = std::thread([this] { accept_loop(); });

  log::info("listening on {}:{} with {} worker(s)", config_.host, port_, worker_count);
}

void Server::stop() {
  if (!running_.exchange(false)) {
    return;
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

void Server::accept_loop() {
  while (running_.load()) {
    try {
      net::Socket connection = net::accept(listener_);
      if (!running_.load()) {
        break;
      }
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
    }

    try {
      handle_connection(std::move(connection));
    } catch (const std::exception& e) {
      log::error("connection handling failed: {}", e.what());
    }
  }
}

void Server::handle_connection(net::Socket connection) {
  net::set_receive_timeout(connection, std::chrono::seconds{config_.read_timeout_seconds});
  net::set_send_timeout(connection, std::chrono::seconds{config_.read_timeout_seconds});

  std::string buffer;
  bool first_request = true;
  while (running_.load()) {
    if (!first_request) {
      net::set_receive_timeout(connection,
                               std::chrono::seconds{config_.keep_alive_timeout_seconds});
    }
    first_request = false;

    const ReadOutcome outcome =
      read_one_request(connection, buffer, config_.max_header_bytes, config_.max_body_bytes);
    if (outcome.kind == ReadOutcome::Kind::Closed) {
      break;
    }
    if (outcome.kind == ReadOutcome::Kind::Error) {
      const std::string body = error_body(outcome.status);
      send_all(connection, build_response(outcome.status, body.size(), "text/html; charset=utf-8",
                                          body, true, false));
      break;
    }

    const bool keep_alive = wants_keep_alive(outcome.request);
    if (!respond(connection, outcome.request, keep_alive)) {
      break;
    }
    if (!keep_alive) {
      break;
    }
  }
}

bool Server::respond(net::Socket& connection, const http::Request& request, bool keep_alive) {
  const bool include_body = request.method != "HEAD";

  const auto send_simple = [&](int status, std::string_view content_type, std::string_view body,
                               std::span<const http::Header> extra = {}) {
    return send_all(connection, build_response(status, body.size(), content_type, body,
                                               include_body, keep_alive, extra));
  };

  if (request.method != "GET" && request.method != "HEAD") {
    const http::Header allow{"Allow", "GET, HEAD"};
    return send_simple(405, "text/html; charset=utf-8", error_body(405), std::span{&allow, 1});
  }

  const std::string request_path = percent_decode(strip_query(request.target));
  const config::Route* route = config::find_route(config_.routes, request_path);
  if (route == nullptr) {
    return send_simple(404, "text/html; charset=utf-8", error_body(404));
  }

  std::string_view rest = std::string_view{request_path}.substr(route->path.size());
  while (!rest.empty() && rest.front() == '/') {
    rest.remove_prefix(1);
  }

  // Resolve the on-disk path for the request. Existence is not checked here:
  // the cache manager may still be able to serve a cached copy.
  std::error_code ec;
  std::filesystem::path file;
  if (std::filesystem::is_directory(route->target, ec)) {
    const std::filesystem::path relative{std::string{rest}};
    for (const std::filesystem::path& part : relative) {
      if (part == "..") {
        return send_simple(403, "text/html; charset=utf-8", error_body(403));
      }
    }
    file = std::filesystem::path{route->target} / relative;
    if (std::filesystem::is_directory(file, ec)) {
      file /= "index.html";
    }
  } else {
    if (!rest.empty()) {
      return send_simple(404, "text/html; charset=utf-8", error_body(404));
    }
    file = route->target;
  }

  const std::shared_ptr<const cache::CacheEntry> entry = cache_manager_.get(file);
  if (entry == nullptr) {
    return send_simple(404, "text/html; charset=utf-8", error_body(404));
  }

  const std::string_view body{reinterpret_cast<const char*>(entry->data.data()),
                              entry->data.size()};
  return send_all(connection, build_response(200, entry->data.size(), content_type_for(file), body,
                                             include_body, keep_alive));
}

}  // namespace web_server::server

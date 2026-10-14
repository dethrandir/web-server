#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace web_server::http {

struct Header {
  std::string name;
  std::string value;
};

struct Request {
  std::string method;
  std::string target;
  std::string version;
  std::vector<Header> headers;
  std::string body;
};

struct Response {
  std::string version;
  int status = 0;
  std::string reason;
  std::vector<Header> headers;
  std::string body;
};

// Returns true if the line matches the RFC 9110 field-line shape (name: value).
[[nodiscard]] bool is_header_line(std::string_view line);

// Splits a raw HTTP message into its head and body at the first blank line.
[[nodiscard]] std::pair<std::string_view, std::string_view> split_head_body(std::string_view raw);

// Collects the regex-recognized header lines into a list of name/value pairs.
[[nodiscard]] std::vector<Header> parse_headers(std::string_view head);

[[nodiscard]] bool has_header(const std::vector<Header>& headers, std::string_view name);

// Case-insensitive lookup of the first header with the given name.
[[nodiscard]] std::optional<std::string_view> header_value(const std::vector<Header>& headers,
                                                           std::string_view name);

// Case-insensitive lookup of every header value with the given name.
[[nodiscard]] std::vector<std::string_view> header_values(const std::vector<Header>& headers,
                                                          std::string_view name);

// Parses the Content-Length header, if present and a valid non-negative integer.
[[nodiscard]] std::optional<std::size_t> content_length(const std::vector<Header>& headers);

// Renders headers back into a CRLF-terminated block.
[[nodiscard]] std::string to_string(const std::vector<Header>& headers);

[[nodiscard]] Request parse_request(std::string_view raw);

[[nodiscard]] Response parse_response(std::string_view raw);

// Standard HTTP/1.1 reason phrase for a status code ("OK" for 200, ...).
[[nodiscard]] std::string_view reason_phrase(int status);

}  // namespace web_server::http

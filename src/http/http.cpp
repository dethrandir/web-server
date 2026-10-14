#include "http/http.hpp"

#include "util/string.hpp"

#include <cctype>
#include <charconv>
#include <regex>
#include <stdexcept>
#include <system_error>

namespace web_server::http {

namespace {

// HTTP/1.1 token per RFC 9110 (tchar).
const std::regex& header_line_regex() {
  static const std::regex pattern{R"(^([!#$%&'*+\-.^_`|~0-9A-Za-z]+):(.*)$)"};
  return pattern;
}

bool iequals(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    const auto left = static_cast<unsigned char>(lhs[i]);
    const auto right = static_cast<unsigned char>(rhs[i]);
    if (std::tolower(left) != std::tolower(right)) {
      return false;
    }
  }
  return true;
}

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (true) {
    const std::size_t end = text.find('\n', start);
    std::string_view line =
      (end == std::string_view::npos) ? text.substr(start) : text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

std::vector<std::string_view> split_whitespace(std::string_view line) {
  std::vector<std::string_view> tokens;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
      ++i;
    }
    if (i >= line.size()) {
      break;
    }
    const std::size_t start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
      ++i;
    }
    tokens.push_back(line.substr(start, i - start));
  }
  return tokens;
}

std::string_view first_line(std::string_view head) {
  const std::size_t end = head.find('\n');
  std::string_view line = (end == std::string_view::npos) ? head : head.substr(0, end);
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return line;
}

std::optional<int> parse_int(std::string_view text) {
  const std::string_view value = util::trim(text);
  if (value.empty()) {
    return std::nullopt;
  }
  int result = 0;
  const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
  if (ec != std::errc{} || ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return result;
}

}  // namespace

bool is_header_line(std::string_view line) {
  const std::string trimmed{util::trim(line)};
  return std::regex_match(trimmed, header_line_regex());
}

std::pair<std::string_view, std::string_view> split_head_body(std::string_view raw) {
  std::size_t pos = raw.find("\r\n\r\n");
  std::size_t separator = 4;
  if (pos == std::string_view::npos) {
    pos = raw.find("\n\n");
    separator = 2;
  }
  if (pos == std::string_view::npos) {
    return {raw, {}};
  }
  return {raw.substr(0, pos), raw.substr(pos + separator)};
}

std::vector<Header> parse_headers(std::string_view head) {
  std::vector<Header> headers;
  for (std::string_view raw_line : split_lines(head)) {
    const std::string_view line = util::trim(raw_line);
    if (line.empty()) {
      continue;
    }

    std::smatch match;
    const std::string candidate{line};
    if (!std::regex_match(candidate, match, header_line_regex())) {
      continue;
    }

    Header header;
    header.name = match[1].str();
    header.value = std::string{util::trim(match[2].str())};
    headers.push_back(std::move(header));
  }
  return headers;
}

bool has_header(const std::vector<Header>& headers, std::string_view name) {
  return header_value(headers, name).has_value();
}

std::optional<std::string_view> header_value(const std::vector<Header>& headers,
                                             std::string_view name) {
  for (const Header& header : headers) {
    if (iequals(header.name, name)) {
      return std::string_view{header.value};
    }
  }
  return std::nullopt;
}

std::vector<std::string_view> header_values(const std::vector<Header>& headers,
                                            std::string_view name) {
  std::vector<std::string_view> values;
  for (const Header& header : headers) {
    if (iequals(header.name, name)) {
      values.push_back(std::string_view{header.value});
    }
  }
  return values;
}

std::optional<std::size_t> content_length(const std::vector<Header>& headers) {
  const std::optional<std::string_view> value = header_value(headers, "Content-Length");
  if (!value.has_value()) {
    return std::nullopt;
  }

  const std::string_view text = util::trim(*value);
  if (text.empty()) {
    return std::nullopt;
  }

  std::size_t result = 0;
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), result);
  if (ec != std::errc{} || ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  return result;
}

std::string to_string(const std::vector<Header>& headers) {
  std::string result;
  for (const Header& header : headers) {
    result.append(header.name);
    result.append(": ");
    result.append(header.value);
    result.append("\r\n");
  }
  return result;
}

Request parse_request(std::string_view raw) {
  const auto [head, body] = split_head_body(raw);

  const std::vector<std::string_view> tokens = split_whitespace(first_line(head));
  if (tokens.size() < 3) {
    throw std::runtime_error("malformed HTTP/1.1 request line");
  }

  Request request;
  request.method = std::string{tokens[0]};
  request.target = std::string{tokens[1]};
  request.version = std::string{tokens[2]};

  const std::size_t line_end = head.find('\n');
  if (line_end != std::string_view::npos) {
    request.headers = parse_headers(head.substr(line_end + 1));
  }
  request.body = std::string{body};
  return request;
}

Response parse_response(std::string_view raw) {
  const auto [head, body] = split_head_body(raw);

  const std::vector<std::string_view> tokens = split_whitespace(first_line(head));
  if (tokens.size() < 2) {
    throw std::runtime_error("malformed HTTP/1.1 status line");
  }

  Response response;
  response.version = std::string{tokens[0]};

  const std::optional<int> status = parse_int(tokens[1]);
  if (!status.has_value()) {
    throw std::runtime_error("malformed HTTP/1.1 status code");
  }
  response.status = *status;

  if (tokens.size() > 2) {
    response.reason = std::string{tokens[2]};
    for (std::size_t i = 3; i < tokens.size(); ++i) {
      response.reason.push_back(' ');
      response.reason.append(tokens[i]);
    }
  }

  const std::size_t line_end = head.find('\n');
  if (line_end != std::string_view::npos) {
    response.headers = parse_headers(head.substr(line_end + 1));
  }
  response.body = std::string{body};
  return response;
}

std::string_view reason_phrase(int status) {
  switch (status) {
  case 100:
    return "Continue";
  case 101:
    return "Switching Protocols";
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 202:
    return "Accepted";
  case 204:
    return "No Content";
  case 206:
    return "Partial Content";
  case 301:
    return "Moved Permanently";
  case 302:
    return "Found";
  case 304:
    return "Not Modified";
  case 307:
    return "Temporary Redirect";
  case 308:
    return "Permanent Redirect";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 408:
    return "Request Timeout";
  case 409:
    return "Conflict";
  case 411:
    return "Length Required";
  case 413:
    return "Content Too Large";
  case 414:
    return "URI Too Long";
  case 415:
    return "Unsupported Media Type";
  case 418:
    return "I'm a teapot";
  case 429:
    return "Too Many Requests";
  case 431:
    return "Request Header Fields Too Large";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  case 502:
    return "Bad Gateway";
  case 503:
    return "Service Unavailable";
  case 504:
    return "Gateway Timeout";
  case 505:
    return "HTTP Version Not Supported";
  default:
    return "Unknown";
  }
}

}  // namespace web_server::http

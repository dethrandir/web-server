#include "http/http.hpp"

#include <catch_amalgamated.hpp>
#include <string>
#include <vector>

namespace http = web_server::http;

TEST_CASE("http: detects header lines", "[http]") {
  REQUIRE(http::is_header_line("Content-Length: 5"));
  REQUIRE(http::is_header_line("Host:example.com"));
  REQUIRE(http::is_header_line("X-Empty:"));
  REQUIRE_FALSE(http::is_header_line("GET / HTTP/1.1"));
  REQUIRE_FALSE(http::is_header_line(": missing-name"));
  REQUIRE_FALSE(http::is_header_line("no colon here"));
}

TEST_CASE("http: splits head and body", "[http]") {
  const std::string crlf = "GET / HTTP/1.1\r\nHost: a\r\n\r\nbody";
  const auto [head_crlf, body_crlf] = http::split_head_body(crlf);
  REQUIRE(head_crlf == "GET / HTTP/1.1\r\nHost: a");
  REQUIRE(body_crlf == "body");

  const std::string lf = "GET / HTTP/1.1\nHost: a\n\nbody";
  const auto [head_lf, body_lf] = http::split_head_body(lf);
  REQUIRE(head_lf == "GET / HTTP/1.1\nHost: a");
  REQUIRE(body_lf == "body");

  const std::string headless = "GET / HTTP/1.1\r\nHost: a\r\n";
  const auto [head_only, body_none] = http::split_head_body(headless);
  REQUIRE(head_only == headless);
  REQUIRE(body_none.empty());
}

TEST_CASE("http: parses headers into an array", "[http]") {
  const std::string block = "Host: example.com\r\nAccept: text/html\r\nNot a header\r\n\r\n";
  const std::vector<http::Header> headers = http::parse_headers(block);

  REQUIRE(headers.size() == 2);
  REQUIRE(headers[0].name == "Host");
  REQUIRE(headers[0].value == "example.com");
  REQUIRE(headers[1].name == "Accept");
  REQUIRE(headers[1].value == "text/html");
}

TEST_CASE("http: looks up headers case-insensitively", "[http]") {
  const std::vector<http::Header> headers = {
    {"Content-Type", "text/plain"}, {"X-Trace", "a"}, {"X-Trace", "b"}};

  REQUIRE(http::has_header(headers, "content-type"));
  REQUIRE_FALSE(http::has_header(headers, "missing"));
  REQUIRE(http::header_value(headers, "CONTENT-TYPE") ==
          std::optional<std::string_view>{"text/plain"});
  REQUIRE(http::header_values(headers, "x-trace") == std::vector<std::string_view>{"a", "b"});
}

TEST_CASE("http: parses content length", "[http]") {
  REQUIRE(http::content_length({{"Content-Length", "42"}}) == std::optional<std::size_t>{42});
  REQUIRE(http::content_length({{"Content-Length", " 42 "}}) == std::optional<std::size_t>{42});
  REQUIRE_FALSE(http::content_length({{"Content-Length", "abc"}}).has_value());
  REQUIRE_FALSE(http::content_length({{"Content-Length", "-1"}}).has_value());
  REQUIRE_FALSE(http::content_length({}).has_value());
}

TEST_CASE("http: parses an HTTP/1.1 request", "[http]") {
  const std::string raw =
    "POST /submit HTTP/1.1\r\nHost: example.com\r\nContent-Length: 5\r\n\r\nhello";

  const http::Request request = http::parse_request(raw);
  REQUIRE(request.method == "POST");
  REQUIRE(request.target == "/submit");
  REQUIRE(request.version == "HTTP/1.1");
  REQUIRE(request.headers.size() == 2);
  REQUIRE(request.body == "hello");
  REQUIRE(http::content_length(request.headers) == std::optional<std::size_t>{5});
}

TEST_CASE("http: parses an HTTP/1.1 response", "[http]") {
  const std::string raw =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nhi";

  const http::Response response = http::parse_response(raw);
  REQUIRE(response.version == "HTTP/1.1");
  REQUIRE(response.status == 200);
  REQUIRE(response.reason == "OK");
  REQUIRE(response.headers.size() == 2);
  REQUIRE(response.body == "hi");
}

TEST_CASE("http: rejects malformed start lines", "[http]") {
  REQUIRE_THROWS_AS(http::parse_request("nope\r\n\r\n"), std::runtime_error);
  REQUIRE_THROWS_AS(http::parse_response("nope\r\n\r\n"), std::runtime_error);
}

TEST_CASE("http: maps status codes to reason phrases", "[http]") {
  REQUIRE(http::reason_phrase(200) == "OK");
  REQUIRE(http::reason_phrase(404) == "Not Found");
  REQUIRE(http::reason_phrase(500) == "Internal Server Error");
}

TEST_CASE("http: serializes headers", "[http]") {
  REQUIRE(http::to_string({{"Host", "example.com"}, {"Accept", "*/*"}}) ==
          "Host: example.com\r\nAccept: */*\r\n");
}

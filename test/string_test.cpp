#include "util/string.hpp"

#include <catch_amalgamated.hpp>
#include <string>
#include <vector>

namespace util = web_server::util;

TEST_CASE("string: trims surrounding whitespace", "[string]") {
  REQUIRE(util::trim("  hi  ") == "hi");
  REQUIRE(util::trim("\t\n hi \r\n") == "hi");
  REQUIRE(util::trim("") == "");
  REQUIRE(util::trim("   ") == "");
  REQUIRE(util::trim("untouched") == "untouched");
}

TEST_CASE("string: changes case", "[string]") {
  REQUIRE(util::to_lower("HeLLo") == "hello");
  REQUIRE(util::to_upper("HeLLo") == "HELLO");
}

TEST_CASE("string: splits on a delimiter", "[string]") {
  REQUIRE(util::split("", ',') == std::vector<std::string>{});
  REQUIRE(util::split("a,b,c", ',') == std::vector<std::string>{"a", "b", "c"});
  REQUIRE(util::split("a,,b", ',') == std::vector<std::string>{"a", "", "b"});
  REQUIRE(util::split("a,", ',') == std::vector<std::string>{"a", ""});
  REQUIRE(util::split("a:b", ':') == std::vector<std::string>{"a", "b"});
}

TEST_CASE("string: joins parts", "[string]") {
  REQUIRE(util::join({"a", "b", "c"}, "-") == "a-b-c");
  REQUIRE(util::join({}, "-") == "");
  REQUIRE(util::join({"solo"}, "-") == "solo");
}

TEST_CASE("string: checks prefixes and suffixes", "[string]") {
  REQUIRE(util::starts_with("hello world", "hello"));
  REQUIRE_FALSE(util::starts_with("hello", "world"));
  REQUIRE(util::ends_with("hello world", "world"));
  REQUIRE_FALSE(util::ends_with("hello", "world"));
  REQUIRE(util::starts_with("anything", ""));
}

TEST_CASE("string: replaces all occurrences", "[string]") {
  REQUIRE(util::replace_all("a.b.c", ".", "/") == "a/b/c");
  REQUIRE(util::replace_all("aaa", "aa", "b") == "ba");
  REQUIRE(util::replace_all("abc", "", "x") == "abc");
}

#include "io/file.hpp"

#include <catch_amalgamated.hpp>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace io = web_server::io;

namespace {

std::filesystem::path temp_path(std::string_view name) {
  return std::filesystem::temp_directory_path() / name;
}

struct TempFile {
  std::filesystem::path path;
  explicit TempFile(std::string_view name) : path(temp_path(name)) {
    std::filesystem::remove(path);
  }
  ~TempFile() { std::filesystem::remove(path); }
};

}  // namespace

TEST_CASE("io: writes and reads text", "[io]") {
  const TempFile file{"web_server_io_text.txt"};
  REQUIRE_FALSE(io::exists(file.path));

  io::write_text(file.path, "hello world");
  REQUIRE(io::exists(file.path));
  REQUIRE(io::read_text(file.path) == "hello world");
}

TEST_CASE("io: appends text", "[io]") {
  const TempFile file{"web_server_io_append.txt"};
  io::write_text(file.path, "a");
  io::append_text(file.path, "b");
  io::append_text(file.path, "c");
  REQUIRE(io::read_text(file.path) == "abc");
}

TEST_CASE("io: truncating write discards previous content", "[io]") {
  const TempFile file{"web_server_io_truncate.txt"};
  io::write_text(file.path, "first");
  io::write_text(file.path, "second");
  REQUIRE(io::read_text(file.path) == "second");
}

TEST_CASE("io: reads lines and strips CR", "[io]") {
  const TempFile file{"web_server_io_lines.txt"};
  io::write_text(file.path, "one\r\ntwo\nthree");

  const std::vector<std::string> lines = io::read_lines(file.path);
  REQUIRE(lines.size() == 3);
  REQUIRE(lines[0] == "one");
  REQUIRE(lines[1] == "two");
  REQUIRE(lines[2] == "three");
}

TEST_CASE("io: reads and writes raw bytes", "[io]") {
  const TempFile file{"web_server_io_bytes.bin"};
  const std::vector<std::byte> data{std::byte{0x00}, std::byte{0x7F}, std::byte{0xFF}};

  io::write_bytes(file.path, data);
  REQUIRE(io::read_bytes(file.path) == data);
}

TEST_CASE("io: reading a missing file throws", "[io]") {
  REQUIRE_THROWS(io::read_text("web_server_missing_file.txt"));
  REQUIRE_THROWS(io::read_bytes("web_server_missing_file.bin"));
}

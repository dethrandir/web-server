#include "file.hpp"

#include <stdexcept>

namespace web_server::io {

// TODO: implement
bool exists(const std::filesystem::path&) {
  throw std::logic_error("web_server::io::exists is not implemented yet");
}

// TODO: implement
std::string read_text(const std::filesystem::path&) {
  throw std::logic_error("web_server::io::read_text is not implemented yet");
}

// TODO: implement
std::vector<std::byte> read_bytes(const std::filesystem::path&) {
  throw std::logic_error("web_server::io::read_bytes is not implemented yet");
}

// TODO: implement
std::vector<std::string> read_lines(const std::filesystem::path&) {
  throw std::logic_error("web_server::io::read_lines is not implemented yet");
}

// TODO: implement
void write_text(const std::filesystem::path&, std::string_view, bool) {
  throw std::logic_error("web_server::io::write_text is not implemented yet");
}

// TODO: implement
void append_text(const std::filesystem::path&, std::string_view) {
  throw std::logic_error("web_server::io::append_text is not implemented yet");
}

// TODO: implement
void write_bytes(const std::filesystem::path&, std::span<const std::byte>, bool) {
  throw std::logic_error("web_server::io::write_bytes is not implemented yet");
}

}  // namespace web_server::io

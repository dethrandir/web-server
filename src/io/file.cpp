#include "file.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace web_server::io {

namespace {

std::ios::openmode write_mode(bool truncate) {
  return std::ios::binary | (truncate ? std::ios::trunc : std::ios::app);
}

}  // namespace

bool exists(const std::filesystem::path& path) {
  return std::filesystem::exists(path);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open file for reading: " + path.string());
  }

  std::string content{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (in.bad()) {
    throw std::runtime_error("failed to read file: " + path.string());
  }
  return content;
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open file for reading: " + path.string());
  }

  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0) {
    throw std::runtime_error("failed to determine size of file: " + path.string());
  }
  in.seekg(0, std::ios::beg);

  std::vector<std::byte> data(static_cast<std::size_t>(size));
  if (size > 0) {
    in.read(reinterpret_cast<char*>(data.data()), size);
    if (!in) {
      throw std::runtime_error("failed to read file: " + path.string());
    }
  }
  return data;
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open file for reading: " + path.string());
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
  }
  if (in.bad()) {
    throw std::runtime_error("failed to read file: " + path.string());
  }
  return lines;
}

void write_text(const std::filesystem::path& path, std::string_view content, bool truncate) {
  std::ofstream out(path, write_mode(truncate));
  if (!out) {
    throw std::runtime_error("failed to open file for writing: " + path.string());
  }

  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!out) {
    throw std::runtime_error("failed to write file: " + path.string());
  }
}

void append_text(const std::filesystem::path& path, std::string_view content) {
  write_text(path, content, false);
}

void write_bytes(const std::filesystem::path& path, std::span<const std::byte> data,
                 bool truncate) {
  std::ofstream out(path, write_mode(truncate));
  if (!out) {
    throw std::runtime_error("failed to open file for writing: " + path.string());
  }

  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!out) {
    throw std::runtime_error("failed to write file: " + path.string());
  }
}

}  // namespace web_server::io

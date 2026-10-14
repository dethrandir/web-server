#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace web_server::io {

[[nodiscard]] bool exists(const std::filesystem::path& path);

// Reads the whole file into a string.
[[nodiscard]] std::string read_text(const std::filesystem::path& path);

// Reads the whole file into a byte buffer.
[[nodiscard]] std::vector<std::byte> read_bytes(const std::filesystem::path& path);

// Reads the file as a list of lines (without line terminators).
[[nodiscard]] std::vector<std::string> read_lines(const std::filesystem::path& path);

// Writes content to the file, optionally truncating an existing file first.
void write_text(const std::filesystem::path& path, std::string_view content, bool truncate = true);

void append_text(const std::filesystem::path& path, std::string_view content);

void write_bytes(const std::filesystem::path& path, std::span<const std::byte> data,
                 bool truncate = true);

}  // namespace web_server::io

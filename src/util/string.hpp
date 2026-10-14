#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace web_server::util {

// Removes leading and trailing whitespace.
[[nodiscard]] std::string_view trim(std::string_view text);

[[nodiscard]] std::string to_lower(std::string_view text);

[[nodiscard]] std::string to_upper(std::string_view text);

[[nodiscard]] std::vector<std::string> split(std::string_view text, char delimiter);

[[nodiscard]] std::string join(const std::vector<std::string>& parts, std::string_view delimiter);

[[nodiscard]] bool starts_with(std::string_view text, std::string_view prefix);

[[nodiscard]] bool ends_with(std::string_view text, std::string_view suffix);

[[nodiscard]] std::string replace_all(std::string_view text, std::string_view from,
                                      std::string_view to);

}  // namespace web_server::util

#include "string.hpp"

#include <algorithm>
#include <cctype>

namespace web_server::util {

std::string_view trim(std::string_view text) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

  const auto begin = std::find_if_not(text.begin(), text.end(), is_space);
  const auto end = std::find_if_not(text.rbegin(), text.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return text.substr(static_cast<std::size_t>(begin - text.begin()),
                     static_cast<std::size_t>(end - begin));
}

std::string to_lower(std::string_view text) {
  std::string result{text};
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return result;
}

std::string to_upper(std::string_view text) {
  std::string result{text};
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return result;
}

std::vector<std::string> split(std::string_view text, char delimiter) {
  std::vector<std::string> parts;
  if (text.empty()) {
    return parts;
  }

  std::size_t start = 0;
  while (true) {
    const std::size_t pos = text.find(delimiter, start);
    if (pos == std::string_view::npos) {
      parts.emplace_back(text.substr(start));
      break;
    }
    parts.emplace_back(text.substr(start, pos - start));
    start = pos + 1;
  }
  return parts;
}

std::string join(const std::vector<std::string>& parts, std::string_view delimiter) {
  std::string result;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      result.append(delimiter);
    }
    result.append(parts[i]);
  }
  return result;
}

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.starts_with(prefix);
}

bool ends_with(std::string_view text, std::string_view suffix) {
  return text.ends_with(suffix);
}

std::string replace_all(std::string_view text, std::string_view from, std::string_view to) {
  if (from.empty()) {
    return std::string{text};
  }

  std::string result;
  std::size_t start = 0;
  while (true) {
    const std::size_t pos = text.find(from, start);
    if (pos == std::string_view::npos) {
      result.append(text.substr(start));
      break;
    }
    result.append(text.substr(start, pos - start));
    result.append(to);
    start = pos + from.size();
  }
  return result;
}

}  // namespace web_server::util

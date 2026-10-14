#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <utility>

namespace web_server::log {

enum class Level { Trace, Debug, Info, Warn, Error, Critical };

// Initializes the global logger. If log_file is empty, logs go to the console.
void init(const std::filesystem::path& log_file = {}, Level level = Level::Info);

void set_level(Level level);

void shutdown();

template <typename... Args> void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::error(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
  spdlog::critical(fmt, std::forward<Args>(args)...);
}

}  // namespace web_server::log

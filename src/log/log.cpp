#include "log.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace web_server::log {

namespace {

constexpr std::string_view kLoggerName = "web_server";
constexpr std::string_view kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";

spdlog::level::level_enum to_spdlog_level(Level level) {
  switch (level) {
  case Level::Trace:
    return spdlog::level::trace;
  case Level::Debug:
    return spdlog::level::debug;
  case Level::Info:
    return spdlog::level::info;
  case Level::Warn:
    return spdlog::level::warn;
  case Level::Error:
    return spdlog::level::err;
  case Level::Critical:
    return spdlog::level::critical;
  }
  return spdlog::level::info;
}

}  // namespace

void init(const std::filesystem::path& log_file, Level level) {
  const std::string name{kLoggerName};
  spdlog::drop(name);

  if (!log_file.empty()) {
    if (log_file.has_parent_path()) {
      std::filesystem::create_directories(log_file.parent_path());
    }
    spdlog::set_default_logger(spdlog::basic_logger_mt(name, log_file.string()));
  } else {
    spdlog::set_default_logger(spdlog::stderr_color_mt(name));
  }

  spdlog::set_level(to_spdlog_level(level));
  spdlog::set_pattern(std::string{kPattern});
  spdlog::flush_on(spdlog::level::info);
}

void set_level(Level level) {
  spdlog::set_level(to_spdlog_level(level));
}

void shutdown() {
  spdlog::shutdown();
}

}  // namespace web_server::log

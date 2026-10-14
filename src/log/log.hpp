#pragma once

#include <filesystem>

namespace web_server::log {

enum class Level { Trace, Debug, Info, Warn, Error, Critical };

// Initializes the global logger. If log_file is empty, logs go to the console.
void init(const std::filesystem::path& log_file = {}, Level level = Level::Info);

void set_level(Level level);

void shutdown();

}  // namespace web_server::log

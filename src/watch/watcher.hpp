#pragma once

#include <filesystem>
#include <functional>
#include <memory>

namespace web_server::watch {

enum class EventType { Modified, Created, Deleted, Other };

struct Event {
  std::filesystem::path path;
  EventType type = EventType::Other;
};

using Callback = std::function<void(const Event&)>;

// Watches files and directories through inotify on a background thread.
// Files are watched via their parent directory, so atomic saves (write + rename)
// and deletions are detected as well.
class Watcher {
public:
  Watcher();
  ~Watcher();

  Watcher(const Watcher&) = delete;
  Watcher& operator=(const Watcher&) = delete;

  // Registers a callback for changes affecting `path` (a file or a directory).
  void watch(const std::filesystem::path& path, Callback callback);

  void start();
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace web_server::watch

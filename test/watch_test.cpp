#include "watch/watcher.hpp"

#include <catch_amalgamated.hpp>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace watch = web_server::watch;

namespace {

struct TempDir {
  std::filesystem::path path;

  explicit TempDir(const std::string& name) {
    path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }

  ~TempDir() { std::filesystem::remove_all(path); }
};

}  // namespace

TEST_CASE("watch: fires when a watched file changes", "[watch]") {
  const TempDir dir{"web_server_watch_file"};
  const std::filesystem::path file = dir.path / "data.txt";
  {
    std::ofstream out(file, std::ios::binary);
    out << "first";
  }

  watch::Watcher watcher;
  std::mutex mutex;
  std::condition_variable cv;
  bool fired = false;
  std::filesystem::path seen;

  watcher.watch(file, [&](const watch::Event& event) {
    std::lock_guard lock{mutex};
    fired = true;
    seen = event.path;
    cv.notify_all();
  });
  watcher.start();

  {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << "second";
  }

  {
    std::unique_lock lock{mutex};
    cv.wait_for(lock, std::chrono::seconds{3}, [&] { return fired; });
  }

  CHECK(fired);
  CHECK(seen == file);
  watcher.stop();
}

TEST_CASE("watch: fires when a watched directory entry changes", "[watch]") {
  const TempDir dir{"web_server_watch_dir"};

  watch::Watcher watcher;
  std::mutex mutex;
  std::condition_variable cv;
  bool fired = false;

  watcher.watch(dir.path, [&](const watch::Event&) {
    std::lock_guard lock{mutex};
    fired = true;
    cv.notify_all();
  });
  watcher.start();

  {
    std::ofstream out(dir.path / "created.txt", std::ios::binary);
    out << "hi";
  }

  {
    std::unique_lock lock{mutex};
    cv.wait_for(lock, std::chrono::seconds{3}, [&] { return fired; });
  }

  CHECK(fired);
  watcher.stop();
}

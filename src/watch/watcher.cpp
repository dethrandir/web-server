#include "watch/watcher.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <mutex>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace web_server::watch {

namespace {

constexpr std::uint32_t kMask = IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO | IN_DELETE |
                                IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF;

EventType classify(std::uint32_t mask) {
  if ((mask & (IN_DELETE | IN_DELETE_SELF | IN_MOVED_FROM)) != 0) {
    return EventType::Deleted;
  }
  if ((mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
    return EventType::Created;
  }
  if ((mask & (IN_CLOSE_WRITE | IN_MODIFY)) != 0) {
    return EventType::Modified;
  }
  return EventType::Other;
}

}  // namespace

struct Watcher::Impl {
  struct Registration {
    std::filesystem::path directory;
    std::string target_filename;  // empty when a whole directory is watched
    Callback callback;
  };

  int inotify_fd = -1;
  int wake_fd = -1;
  std::atomic<bool> running{false};
  std::thread thread;
  std::mutex mutex;
  // Several registrations can share one inotify watch descriptor (same directory).
  std::unordered_map<int, std::vector<Registration>> watches;

  Impl() {
    inotify_fd = ::inotify_init1(IN_CLOEXEC);
    if (inotify_fd < 0) {
      throw std::system_error(errno, std::generic_category(), "inotify_init1");
    }
    wake_fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd < 0) {
      ::close(inotify_fd);
      throw std::system_error(errno, std::generic_category(), "eventfd");
    }
  }

  ~Impl() {
    if (inotify_fd >= 0) {
      ::close(inotify_fd);
    }
    if (wake_fd >= 0) {
      ::close(wake_fd);
    }
  }

  void add_directory_watch(const std::filesystem::path& directory, std::string target,
                           Callback callback) {
    const int wd = ::inotify_add_watch(inotify_fd, directory.c_str(), kMask);
    if (wd < 0) {
      throw std::system_error(errno, std::generic_category(), "inotify_add_watch");
    }
    std::lock_guard lock{mutex};
    watches[wd].push_back(Registration{directory, std::move(target), std::move(callback)});
  }

  void handle_event(const inotify_event& event) {
    std::vector<Registration> registrations;
    {
      std::lock_guard lock{mutex};
      const auto it = watches.find(event.wd);
      if (it != watches.end()) {
        registrations = it->second;
      }
      if ((event.mask & IN_IGNORED) != 0) {
        watches.erase(event.wd);
      }
    }

    const std::string name = event.len > 0 ? std::string{event.name} : std::string{};
    const EventType type = classify(event.mask);
    for (const Registration& registration : registrations) {
      if (!registration.target_filename.empty() && name != registration.target_filename) {
        continue;
      }
      const std::filesystem::path affected =
        name.empty() ? registration.directory : registration.directory / name;
      registration.callback(Event{affected, type});
    }
  }

  void run() {
    std::vector<char> buffer(64 * 1024);
    pollfd descriptors[2];
    descriptors[0] = {inotify_fd, POLLIN, 0};
    descriptors[1] = {wake_fd, POLLIN, 0};

    while (running.load()) {
      const int ready = ::poll(descriptors, 2, -1);
      if (ready < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if ((descriptors[1].revents & POLLIN) != 0) {
        break;
      }
      if ((descriptors[0].revents & POLLIN) == 0) {
        continue;
      }

      const ssize_t length = ::read(inotify_fd, buffer.data(), buffer.size());
      if (length <= 0) {
        continue;
      }
      ssize_t offset = 0;
      while (offset < length) {
        const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);
        handle_event(*event);
        offset += static_cast<ssize_t>(sizeof(inotify_event) + event->len);
      }
    }
  }
};

Watcher::Watcher() : impl_(std::make_unique<Impl>()) {}

Watcher::~Watcher() {
  stop();
}

void Watcher::watch(const std::filesystem::path& path, Callback callback) {
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec)) {
    impl_->add_directory_watch(path, {}, std::move(callback));
    return;
  }

  std::filesystem::path directory = path.parent_path();
  if (directory.empty()) {
    directory = ".";
  }
  impl_->add_directory_watch(directory, path.filename().string(), std::move(callback));
}

void Watcher::start() {
  if (impl_->running.exchange(true)) {
    return;
  }
  impl_->thread = std::thread([this] { impl_->run(); });
}

void Watcher::stop() {
  if (!impl_->running.exchange(false)) {
    return;
  }
  const std::uint64_t wake = 1;
  const ssize_t ignored = ::write(impl_->wake_fd, &wake, sizeof(wake));
  static_cast<void>(ignored);
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
}

}  // namespace web_server::watch

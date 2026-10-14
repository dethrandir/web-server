#include "log.hpp"

#include <stdexcept>

namespace web_server::log {

// TODO: implement
void init(const std::filesystem::path&, Level) {
  throw std::logic_error("web_server::log::init is not implemented yet");
}

// TODO: implement
void set_level(Level) {
  throw std::logic_error("web_server::log::set_level is not implemented yet");
}

// TODO: implement
void shutdown() {
  throw std::logic_error("web_server::log::shutdown is not implemented yet");
}

}  // namespace web_server::log

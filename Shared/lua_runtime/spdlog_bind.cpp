#include "spdlog_bind.h"

#include <fmt/args.h>
#include <spdlog/spdlog.h>

#include "shared/event.h"

namespace lua {
namespace bindings {

template <void logFunc(const std::string&)>
void log(std::string text, sol::variadic_args args) {
  auto store = fmt::dynamic_format_arg_store<fmt::format_context>();
  for (const auto& arg : args) {
    if (arg.get_type() == sol::type::string) {
      store.push_back(arg.as<std::string>());
    } else if (arg.get_type() == sol::type::number) {
      store.push_back(arg.as<int>());
    } else if (arg.get_type() == sol::type::boolean) {
      store.push_back(arg.as<bool>());
    } else if (arg.get_type() == sol::type::userdata) {
      store.push_back(arg.as<std::string>());
    } else {
      store.push_back(arg.as<std::string>());
    }
  }
  logFunc(fmt::vformat(text, store));
}

void Bind_spdlog(sol::state& lua) {
  lua["LOG_ERROR"] = log<spdlog::error>;
  lua["LOG_INFO"] = log<spdlog::info>;
  lua["LOG_DEBUG"] = log<spdlog::debug>;
  lua["LOG_CRITICAL"] = log<spdlog::critical>;
  lua["LOG_WARN"] = log<spdlog::warn>;
  lua["LOG_TRACE"] = log<spdlog::trace>;
}

}  // namespace bindings
}  // namespace lua

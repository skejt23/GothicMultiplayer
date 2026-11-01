#include "spdlog_bind.h"

#include <fmt/args.h>
#include <fmt/format.h>
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
  try {
    logFunc(fmt::vformat(text, store));
  } catch (const fmt::format_error& error) {
    SPDLOG_ERROR("Lua log format error in '{}': {}", text, error.what());
  }
}

/* luagmp (func)
 *
 * Logs a message with ERROR severity.
 *
 * Error messages indicate serious problems that prevent normal operation
 * or cause a feature to fail.
 *
 * @version  0.3.0
 * @name     LOG_ERROR
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_ERROR(std::string text, sol::variadic_args args) {
  log<spdlog::error>(std::move(text), args);
}

/* luagmp (func)
 *
 * Logs a message with INFO severity.
 *
 * Informational messages describe normal application behavior and
 * important runtime events.
 *
 * @version  0.3.0
 * @name     LOG_INFO
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_INFO(std::string text, sol::variadic_args args) {
  log<spdlog::info>(std::move(text), args);
}

/* luagmp (func)
 *
 * Logs a message with DEBUG severity.
 *
 * Debug messages provide diagnostic information useful during development
 * and troubleshooting.
 *
 * @version  0.3.0
 * @name     LOG_DEBUG
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_DEBUG(std::string text, sol::variadic_args args) {
  log<spdlog::debug>(std::move(text), args);
}

/* luagmp (func)
 *
 * Logs a message with CRITICAL severity.
 *
 * Critical messages report very severe errors that may require immediate
 * attention or application shutdown.
 *
 * @version  0.3.0
 * @name     LOG_CRITICAL
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_CRITICAL(std::string text, sol::variadic_args args) {
  log<spdlog::critical>(std::move(text), args);
}

/* luagmp (func)
 *
 * Logs a message with WARN severity.
 *
 * Warning messages indicate potential problems or unusual situations that
 * do not immediately stop execution.
 *
 * @version  0.3.0
 * @name     LOG_WARN
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_WARN(std::string text, sol::variadic_args args) {
  log<spdlog::warn>(std::move(text), args);
}

/* luagmp (func)
 *
 * Logs a message with TRACE severity.
 *
 * Trace messages provide very detailed output, typically used for
 * low-level debugging and deep diagnostics.
 *
 * @version  0.3.0
 * @name     LOG_TRACE
 * @side     shared
 * @category Log
 * @param  (string) text      Message text, may contain format specifiers.
 *
 */
void Function_LOG_TRACE(std::string text, sol::variadic_args args) {
  log<spdlog::trace>(std::move(text), args);
}

void Bind_spdlog(sol::state& lua) {
  lua["LOG_ERROR"] = Function_LOG_ERROR;
  lua["LOG_INFO"] = Function_LOG_INFO;
  lua["LOG_DEBUG"] = Function_LOG_DEBUG;
  lua["LOG_CRITICAL"] = Function_LOG_CRITICAL;
  lua["LOG_WARN"] = Function_LOG_WARN;
  lua["LOG_TRACE"] = Function_LOG_TRACE;
}

}  // namespace bindings
}  // namespace lua

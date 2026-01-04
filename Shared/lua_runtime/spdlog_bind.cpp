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


/* luadoc (func)
*
* Logs a message with ERROR severity.
*
* Error messages indicate serious problems that prevent normal operation
* or cause a feature to fail.
*
* @name     LOG_ERROR
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

/* luadoc (func)
*
* Logs a message with INFO severity.
*
* Informational messages describe normal application behavior and
* important runtime events.
*
* @name     LOG_INFO
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

/* luadoc (func)
*
* Logs a message with DEBUG severity.
*
* Debug messages provide diagnostic information useful during development
* and troubleshooting.
*
* @name     LOG_DEBUG
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

/* luadoc (func)
*
* Logs a message with CRITICAL severity.
*
* Critical messages report very severe errors that may require immediate
* attention or application shutdown.
*
* @name     LOG_CRITICAL
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

/* luadoc (func)
*
* Logs a message with WARN severity.
*
* Warning messages indicate potential problems or unusual situations that
* do not immediately stop execution.
*
* @name     LOG_WARN
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

/* luadoc (func)
*
* Logs a message with TRACE severity.
*
* Trace messages provide very detailed output, typically used for
* low-level debugging and deep diagnostics.
*
* @name     LOG_TRACE
* @side     shared
* @category Log
* @param  (string) text      The message text, may contain format specifiers.
*
*/

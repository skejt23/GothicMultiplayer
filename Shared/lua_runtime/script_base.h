
/*
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <spdlog/spdlog.h>

#include "sol/sol.hpp"
#include "spdlog_bind.h"
#include "timer_manager.h"
#include "utility_bind.h"

namespace lua {

// Security policies - compile-time selection
// The unused policy's code is completely eliminated from the binary

struct SandboxedPolicy {
  static constexpr const char* name = "Sandboxed";

  static void OpenLibraries(sol::state& lua) {
    // SAFE for untrusted/client scripts - NO file/system access
    lua.open_libraries(sol::lib::base,      // Basic functions (print, type, assert, error, etc.)
                       sol::lib::string,    // String manipulation
                       sol::lib::math,      // Math functions
                       sol::lib::table,     // Table manipulation
                       sol::lib::coroutine  // Coroutines for async operations
    );
    // Explicitly disable dangerous functions that might slip through
    lua["dofile"] = sol::lua_nil;
    lua["loadfile"] = sol::lua_nil;
    lua["load"] = sol::lua_nil;
  }
};

struct TrustedPolicy {
  static constexpr const char* name = "Trusted";

  static void OpenLibraries(sol::state& lua) {
    // For trusted server-side scripts only
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::table, sol::lib::coroutine,
                       sol::lib::os  // Allow os.time(), os.date() for server scripts
                                     // Still DO NOT open: io, package, debug
    );

    // Whitelist only safe os functions
    sol::table os_table = lua["os"];
    sol::table safe_os = lua.create_table();
    safe_os["time"] = os_table["time"];
    safe_os["date"] = os_table["date"];
    safe_os["clock"] = os_table["clock"];
    safe_os["difftime"] = os_table["difftime"];
    lua["os"] = safe_os;

    // Still block dangerous functions
    lua["dofile"] = sol::lua_nil;
    lua["loadfile"] = sol::lua_nil;
  }
};

// Exception/panic handlers (shared)
namespace detail {
inline int script_exception_handler(lua_State* L, sol::optional<const std::exception&> maybe_exception, sol::string_view description) {
  SPDLOG_ERROR("An exception occurred in a function, here's what it says ");
  if (maybe_exception) {
    SPDLOG_ERROR("(straight from the exception): ");
    const std::exception& ex = *maybe_exception;
    SPDLOG_ERROR(ex.what());
  } else {
    SPDLOG_ERROR("(from the description parameter): ");
    SPDLOG_ERROR("{}", description.data());
  }
  return sol::stack::push(L, description);
}

inline void script_panic(sol::optional<std::string> maybe_msg) {
  SPDLOG_CRITICAL("Lua is in a panic state and will now abort() the application");
  if (maybe_msg) {
    const std::string& msg = maybe_msg.value();
    SPDLOG_CRITICAL("error message: {}", msg);
  }
}
}  // namespace detail

// Template-based script base - SecurityPolicy is compile-time parameter
// Client binaries use SandboxedPolicy, server uses TrustedPolicy
template <typename SecurityPolicy>
class ScriptBase {
public:
  ScriptBase() {
    InitializeCommon();
  }

  virtual ~ScriptBase() {
    timer_manager_.Clear();
  }

  // Process timer callbacks
  void ProcessTimers() {
    timer_manager_.ProcessTimers();
  }

  // Access the underlying Lua state
  sol::state& GetLuaState() {
    return lua_;
  }
  const sol::state& GetLuaState() const {
    return lua_;
  }

  // Access the timer manager
  TimerManager& GetTimerManager() {
    return timer_manager_;
  }

protected:
  // Initialize Lua state with common bindings
  virtual void InitializeCommon() {
    // Apply security policy (compile-time dispatch)
    SecurityPolicy::OpenLibraries(lua_);

    lua_.set_exception_handler(&detail::script_exception_handler);
    lua_.set_panic(sol::c_call<decltype(&detail::script_panic), &detail::script_panic>);

    // Bind shared functionality
    bindings::Bind_spdlog(lua_);
    bindings::BindUtilities(lua_);
    bindings::BindTimers(lua_, timer_manager_);
  }

  // Override to add domain-specific bindings
  virtual void BindDomainSpecific() = 0;

  sol::state lua_;
  TimerManager timer_manager_;
};

// Type aliases for convenience
using SandboxedScriptBase = ScriptBase<SandboxedPolicy>;
using TrustedScriptBase = ScriptBase<TrustedPolicy>;

}  // namespace lua

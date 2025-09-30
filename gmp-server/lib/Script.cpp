#include "Script.h"

#include <spdlog/spdlog.h>

#include "sol/sol.hpp"
// Binds
#include "Lua/event_bind.h"
#include "Lua/function_bind.h"
#include "Lua/spdlog_bind.h"

using namespace std;
const string directory = "scripts";

int script_exception_handler(lua_State* L, sol::optional<const std::exception&> maybe_exception,
                             sol::string_view description) {
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

Script::Script(vector<string> scripts) {
  Init();
  LoadScripts(scripts);
}

Script::~Script() {
  timer_manager_.Clear();
}

void Script::Init() {
  lua.open_libraries();
  lua.set_exception_handler(&script_exception_handler);
  lua.set_panic(sol::c_call<decltype(&script_panic), &script_panic>);
  BindFunctionsAndVariables();
}

void Script::BindFunctionsAndVariables() {
  lua::bindings::Bind_spdlog(lua);
  lua::bindings::BindEvents(lua);
  lua::bindings::BindFunctions(lua, timer_manager_);
}

void Script::LoadScripts(vector<string> scripts) {
  for_each(scripts.begin(), scripts.end(), bind(&Script::LoadScript, this, placeholders::_1));
}

void Script::LoadScript(string script) {
  try {
    auto result = lua.safe_script_file(directory + "/" + script);
    SPDLOG_INFO("{} has been loaded", script);
  } catch (const sol::error& e) {
    SPDLOG_ERROR("{} cannot be loaded: {}", script, e.what());
  }
}

void Script::ProcessTimers() {
  timer_manager_.ProcessTimers();
}

TimerManager& Script::GetTimerManager() {
  return timer_manager_;
}
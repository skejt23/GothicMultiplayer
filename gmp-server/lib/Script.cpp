#include "Script.h"

#include <spdlog/spdlog.h>

#include "sol/sol.hpp"
// Binds
#include "Lua/event_bind.h"
#include "Lua/function_bind.h"

using namespace std;
const string directory = "scripts";

Script::Script(vector<string> scripts) {
  BindDomainSpecific();
  LoadScripts(scripts);
}

void Script::BindDomainSpecific() {
  lua::bindings::BindEvents(lua_);
  lua::bindings::BindFunctions(lua_, timer_manager_);
}

void Script::LoadScripts(vector<string> scripts) {
  for_each(scripts.begin(), scripts.end(), bind(&Script::LoadScript, this, placeholders::_1));
}

void Script::LoadScript(const string& script) {
  try {
    auto result = lua_.safe_script_file(directory + "/" + script);
    SPDLOG_INFO("{} has been loaded", script);
  } catch (const sol::error& e) {
    SPDLOG_ERROR("{} cannot be loaded: {}", script, e.what());
  }
}
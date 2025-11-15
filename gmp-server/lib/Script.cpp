#include "Script.h"

#include <spdlog/spdlog.h>

#include "sol/sol.hpp"
// Binds
#include "Lua/event_bind.h"
#include "Lua/function_bind.h"

using namespace std;

LuaScript::LuaScript() {
  BindDomainSpecific();
}

void LuaScript::BindDomainSpecific() {
  lua::bindings::BindEvents(lua_);
  lua::bindings::BindFunctions(lua_, timer_manager_);
}
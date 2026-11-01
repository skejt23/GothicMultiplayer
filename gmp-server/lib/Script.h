#pragma once
#include <vector>
#include <string>

#include "Shared/lua_runtime/script_base.h"

// Server Lua VM - trusted (loaded from local disk by admin)
// Uses TrustedScriptBase which includes TrustedPolicy at compile-time
// Responsible solely for Lua state and engine bindings
class LuaScript : public lua::TrustedScriptBase {
public:
  LuaScript();
  ~LuaScript();

protected:
  void BindDomainSpecific() override;
};

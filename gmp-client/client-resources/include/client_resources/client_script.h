#pragma once

#include "shared/lua_runtime/script_base.h"

class ClientScript : public lua::SandboxedScriptBase {
public:
  ClientScript();
  ~ClientScript() = default;

protected:
  void BindDomainSpecific() override;
};

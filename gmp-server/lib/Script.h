#pragma once
#include <vector>
#include <string>

#include "shared/lua_runtime/script_base.h"

// Server scripts are trusted (loaded from local disk by admin)
// Uses TrustedScriptBase which includes TrustedPolicy at compile-time
class Script : public lua::TrustedScriptBase {
public:
  Script(std::vector<std::string> scripts);
  ~Script() = default;

protected:
  void BindDomainSpecific() override;

private:
  void LoadScripts(std::vector<std::string> scripts);
  void LoadScript(const std::string& script);
};
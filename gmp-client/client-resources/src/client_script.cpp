#include "client_resources/client_script.h"

#include <spdlog/spdlog.h>

ClientScript::ClientScript() {
  BindDomainSpecific();
}

void ClientScript::BindDomainSpecific() {
  SPDLOG_DEBUG("ClientScript domain bindings initialized");
  // Domain-specific bindings (e.g., Gothic events) should be added
  // by the consuming code after construction via GetLuaState()
}

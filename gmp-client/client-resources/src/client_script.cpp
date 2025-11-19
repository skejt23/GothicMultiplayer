#include "client_resources/client_script.h"
#include "client_resources/event_bind.h"

#include <spdlog/spdlog.h>

ClientScript::ClientScript() {
  BindDomainSpecific();
}

void ClientScript::BindDomainSpecific() {
  SPDLOG_DEBUG("ClientScript domain bindings initialized");
  gmp::client::lua::bindings::BindEvents(lua_);
}

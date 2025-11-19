#pragma once

#include "sol/sol.hpp"

namespace gmp::client::lua::bindings {

void BindEvents(sol::state& lua);
void ResetEvents();

} // namespace gmp::client::lua::bindings

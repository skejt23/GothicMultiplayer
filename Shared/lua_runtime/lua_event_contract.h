#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "sol/sol.hpp"

namespace gmp::lua {

// Metadata is supplied by the dispatcher, never decoded from the payload.
struct EventMetadata {
  std::optional<std::uint32_t> source_element;
  std::optional<std::uint32_t> sender_player_id;
  bool remote = false;
};

inline std::uint32_t ReadEventId(const sol::object& value) {
  if (value.get_type() != sol::type::number) {
    throw sol::error("Event identifiers must be positive 32-bit integers");
  }
  const double number = value.as<double>();
  if (!std::isfinite(number) || std::trunc(number) != number || number < 1 || number > std::numeric_limits<std::uint32_t>::max()) {
    throw sol::error("Event identifiers must be positive 32-bit integers");
  }
  return static_cast<std::uint32_t>(number);
}

inline std::optional<std::uint32_t> ReadEventSource(const sol::object& options) {
  if (!options.valid() || options.get_type() == sol::type::nil)
    return std::nullopt;
  if (options.get_type() != sol::type::table)
    throw sol::error("Event options must be a table or nil; pass payload arguments after this slot");
  sol::object source = options.as<sol::table>()["sourceId"];
  if (source.get_type() == sol::type::nil)
    return std::nullopt;
  return ReadEventId(source);
}

inline sol::table MakeEventContext(sol::state_view lua, const EventMetadata& metadata) {
  sol::table context = lua.create_table();
  context["remote"] = metadata.remote;
  if (metadata.source_element)
    context["sourceId"] = *metadata.source_element;
  if (metadata.sender_player_id)
    context["senderPlayerId"] = *metadata.sender_player_id;
  return context;
}

}  // namespace gmp::lua

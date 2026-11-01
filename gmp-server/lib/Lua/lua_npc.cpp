/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "lua_npc.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>

#include "../game_server.h"
#include "../npc_manager.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "npc_packets.h"

namespace lua::bindings {
namespace {

std::optional<std::uint32_t> ParseId(std::int64_t id) {
  if (id <= 0 || static_cast<std::uint64_t>(id) > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(id);
}

const NpcManager::Npc* FindNpc(std::int64_t id) {
  const auto parsed = ParseId(id);
  if (!g_server || !parsed) {
    return nullptr;
  }
  const auto npc = g_server->GetNpcManager().GetNpc(*parsed);
  return npc ? &npc->get() : nullptr;
}

sol::table MakeActionTable(sol::state_view lua, const NpcManager::NpcAction& action) {
  sol::table result = lua.create_table();
  result["id"] = action.id;
  result["type"] = kNpcActionPlayAnimation;
  result["status"] = action.started_at.has_value() ? "running" : "queued";
  result["animation"] = action.animation;
  result["timeoutMs"] = action.timeout_ms;
  return result;
}

}  // namespace

/* luagmp (func)
 *
 * Creates an unspawned, server-owned NPC.
 *
 * @version  0.3.0
 * @name     createNpc
 * @side     server
 * @category NPC
 * @note     By default, the NPC won't be added to the world. It's necessary to call [spawnPlayer](../player/spawnPlayer.md) afterwards.
 * @param    (string) name      NPC name.
 * @param    (string|nil) instance Gothic NPC instance; defaults to PC_HERO.
 * @return   (number|nil, table|nil)           NPC id and nil on success; nil and {message = string} on failure.
 *
 */
::lua::bind_helpers::LuaResult Function_CreateNpc(const std::string& name, sol::optional<std::string> instance, sol::this_state state) {
  sol::state_view lua(state);
  const auto id = g_server ? g_server->CreateNpc(name, instance.value_or("PC_HERO")) : 0;
  if (id == 0)
    return ::lua::bind_helpers::MakeFailure(lua, "NPC creation failed");
  return ::lua::bind_helpers::MakeSuccess(lua, sol::make_object(lua, id));
}

/* luagmp (func)
 *
 * Destroys an NPC and cancels its queued actions.
 *
 * @version  0.3.0
 * @name     destroyNpc
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (boolean)         True if the NPC was destroyed.
 *
 */
bool Function_DestroyNpc(std::int64_t npc_id) {
  const auto id = ParseId(npc_id);
  return g_server && id && g_server->DestroyNpc(*id);
}

/* luagmp (func)
 *
 * Checks whether an id identifies an existing server-owned NPC.
 *
 * @version  0.3.0
 * @name     isNpc
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Player or NPC id to test.
 * @return   (boolean)         True for an existing server NPC, including unspawned NPCs.
 *
 */
bool Function_IsNpc(std::int64_t npc_id) {
  return FindNpc(npc_id) != nullptr;
}

/* luagmp (func)
 *
 * Sets an NPC's persistent animation, including looping animations. All current
 * and future viewers receive this state. Finite queued actions take precedence.
 *
 * @version  0.3.0
 * @name     setNpcAnimation
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @param    (string) animation Gothic animation name; an empty string clears it.
 * @return   (boolean)         True if the state was accepted; does not validate client assets.
 *
 */
bool Function_SetNpcAnimation(std::int64_t npc_id, const std::string& animation) {
  const auto id = ParseId(npc_id);
  return g_server && id && g_server->SetNpcAnimation(*id, animation);
}

/* luagmp (func)
 *
 * Returns the stored persistent animation, independently of the finite queue.
 *
 * @version  0.3.0
 * @name     getNpcAnimation
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (string|nil)      Animation name, empty when cleared; nil for a missing NPC.
 *
 */
sol::object Function_GetNpcAnimation(std::int64_t npc_id, sol::this_state state) {
  const auto* npc = FindNpc(npc_id);
  if (!npc) {
    return sol::nil;
  }
  return sol::make_object(sol::state_view(state), npc->animation);
}

/* luagmp (func)
 *
 * Queues a finite animation. Completion is reported by [onNpcActionFinished](../../server-events/npc/onNpcActionFinished.md).
 *
 * @version  0.3.0
 * @name     queueNpcAnimation
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @param    (string) animation Gothic animation name.
 * @param    (number|nil) timeout_ms Execution timeout, 1-30000 ms; defaults to 10000.
 * @return   (number|nil, table|nil)          Action id and nil on success; nil and {message = string} when rejected.
 *
 */
::lua::bind_helpers::LuaResult Function_QueueNpcAnimation(std::int64_t npc_id, const std::string& animation, sol::optional<std::int64_t> timeout_ms,
                                                          sol::this_state state) {
  sol::state_view lua(state);
  const auto id = ParseId(npc_id);
  const auto timeout = timeout_ms.value_or(NpcManager::kDefaultActionTimeoutMs);
  if (!g_server || !id || timeout <= 0 || timeout > NpcManager::kMaxActionTimeoutMs) {
    return ::lua::bind_helpers::MakeFailure(lua, "Invalid NPC or animation timeout");
  }
  const auto action = g_server->QueueNpcAnimation(*id, animation, static_cast<std::uint32_t>(timeout));
  if (action == 0)
    return ::lua::bind_helpers::MakeFailure(lua, "NPC animation was rejected");
  return ::lua::bind_helpers::MakeSuccess(lua, sol::make_object(lua, action));
}

/* luagmp (func)
 *
 * Cancels the current action and all pending actions.
 *
 * @version  0.3.0
 * @name     clearNpcActions
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (boolean)         True if cleared, including an empty queue; false for an invalid NPC or failed operation.
 *
 */
bool Function_ClearNpcActions(std::int64_t npc_id) {
  const auto id = ParseId(npc_id);
  return g_server && id && g_server->ClearNpcActions(*id);
}

/* luagmp (func)
 *
 * Returns the number of running and pending actions.
 *
 * @version  0.3.0
 * @name     getNpcActionsCount
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (number|nil)          Queue size, or nil for a missing NPC.
 *
 */
sol::optional<std::int64_t> Function_GetNpcActionsCount(std::int64_t npc_id) {
  const auto* npc = FindNpc(npc_id);
  if (!npc)
    return sol::nullopt;
  return static_cast<std::int64_t>(npc->actions.size());
}

/* luagmp (func)
 *
 * Returns a copy of an action's metadata. Index one is the queue head.
 *
 * @version  0.3.0
 * @name     getNpcAction
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @param    (number) index     One-based queue index.
 * @return   ({id, type, status, animation, timeoutMs}|nil) Action snapshot, or nil for an invalid NPC or index.
 *
 */
sol::object Function_GetNpcAction(std::int64_t npc_id, std::int64_t index, sol::this_state state) {
  const auto* npc = FindNpc(npc_id);
  if (!npc || index < 1 || static_cast<std::uint64_t>(index) > npc->actions.size()) {
    return sol::nil;
  }
  sol::state_view lua(state);
  return sol::make_object(lua, MakeActionTable(lua, npc->actions[static_cast<std::size_t>(index - 1)]));
}

/* luagmp (func)
 *
 * Returns copies of all running and pending actions in queue order.
 *
 * @version  0.3.0
 * @name     getNpcActions
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   ({...}|nil)       One-based Lua array of action snapshots, or nil for a missing NPC.
 *
 */
sol::object Function_GetNpcActions(std::int64_t npc_id, sol::this_state state) {
  const auto* npc = FindNpc(npc_id);
  if (!npc) {
    return sol::nil;
  }
  sol::state_view lua(state);
  sol::table result = lua.create_table();
  std::size_t index = 1;
  for (const auto& action : npc->actions) {
    result[index++] = MakeActionTable(lua, action);
  }
  return sol::make_object(lua, result);
}

/* luagmp (func)
 *
 * Returns the most recently allocated action id, including completed actions.
 *
 * @version  0.3.0
 * @name     getNpcLastActionId
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (number|nil)          Action id, zero before any actions, or nil for a missing NPC.
 *
 */
sol::optional<std::int64_t> Function_GetNpcLastActionId(std::int64_t npc_id) {
  const auto* npc = FindNpc(npc_id);
  if (!npc)
    return sol::nullopt;
  return static_cast<std::int64_t>(npc->next_action_id) - 1;
}

/* luagmp (func)
 *
 * Checks whether an allocated action has left the queue, including failure or cancellation.
 * A true result does not imply success. Use [onNpcActionFinished](../../server-events/npc/onNpcActionFinished.md)'s result argument to distinguish successful completion from failure, timeout or cancellation.
 *
 * @version  0.3.0
 * @name     isNpcActionFinished
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @param    (number) action_id Action id returned by [queueNpcAnimation](queueNpcAnimation.md).
 * @return   (boolean)         True once the action is no longer queued; false for unknown ids.
 *
 */
bool Function_IsNpcActionFinished(std::int64_t npc_id, std::int64_t action_id) {
  const auto npc = ParseId(npc_id);
  const auto action = ParseId(action_id);
  return g_server && npc && action && g_server->GetNpcManager().IsActionFinished(*npc, *action);
}

/* luagmp (func)
 *
 * Checks whether the queue contains a running or pending action of the requested type.
 *
 * @version  0.3.0
 * @name     isNpcActionTypeQueued
 * @side     server
 * @category NPC
 * @param    (number) npc_id     Server NPC id.
 * @param    (number) action_type ACTION_PLAY_ANI is the only supported type.
 * @return   (boolean)          True if the type is present in the queue.
 *
 */
bool Function_IsNpcActionTypeQueued(std::int64_t npc_id, std::int64_t action_type) {
  const auto* npc = FindNpc(npc_id);
  return npc && action_type == kNpcActionPlayAnimation && !npc->actions.empty();
}

/* luagmp (func)
 *
 * Returns the player currently responsible for executing this NPC's actions.
 *
 * @version  0.3.0
 * @name     getNpcHostPlayer
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @return   (number|nil)          Player id, or nil when unhosted or missing.
 *
 */
sol::optional<std::int64_t> Function_GetNpcHostPlayer(std::int64_t npc_id) {
  const auto* npc = FindNpc(npc_id);
  if (!npc || npc->host_player_id == 0)
    return sol::nullopt;
  return static_cast<std::int64_t>(npc->host_player_id);
}

/* luagmp (func)
 *
 * Assigns an eligible streaming player as host, or releases the host for automatic selection.
 *
 * @version  0.3.0
 * @name     setNpcHostPlayer
 * @side     server
 * @category NPC
 * @param    (number) npc_id    Server NPC id.
 * @param    (number|nil) host_id   Eligible player id; nil releases the current host.
 * @return   (boolean)         True if the assignment was accepted.
 *
 */
bool Function_SetNpcHostPlayer(std::int64_t npc_id, sol::optional<std::int64_t> host_id) {
  const auto npc = ParseId(npc_id);
  if (!g_server || !npc) {
    return false;
  }
  if (!host_id) {
    return g_server->SetNpcHostPlayer(*npc, 0);
  }
  const auto host = ParseId(*host_id);
  return host && g_server->SetNpcHostPlayer(*npc, *host);
}

/* luagmp (const)
 *
 * Represents action play ani type.
 *
 * @name     ACTION_PLAY_ANI
 * @side     server
 * @category NPC
 *
 */

void BindNpc(sol::state& lua) {
  lua["createNpc"] = Function_CreateNpc;
  lua["destroyNpc"] = Function_DestroyNpc;
  lua["isNpc"] = Function_IsNpc;
  lua["setNpcAnimation"] = Function_SetNpcAnimation;
  lua["getNpcAnimation"] = Function_GetNpcAnimation;
  lua["queueNpcAnimation"] = Function_QueueNpcAnimation;
  lua["clearNpcActions"] = Function_ClearNpcActions;
  lua["getNpcActionsCount"] = Function_GetNpcActionsCount;
  lua["getNpcAction"] = Function_GetNpcAction;
  lua["getNpcActions"] = Function_GetNpcActions;
  lua["getNpcLastActionId"] = Function_GetNpcLastActionId;
  lua["isNpcActionFinished"] = Function_IsNpcActionFinished;
  lua["isNpcActionTypeQueued"] = Function_IsNpcActionTypeQueued;
  lua["getNpcHostPlayer"] = Function_GetNpcHostPlayer;
  lua["setNpcHostPlayer"] = Function_SetNpcHostPlayer;

  lua["ACTION_PLAY_ANI"] = kNpcActionPlayAnimation;
}

}  // namespace lua::bindings

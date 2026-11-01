
/*
MIT License

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

#include "event_bind.h"

#include <spdlog/spdlog.h>

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../resource_manager.h"
#include "../server_events.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_events.h"
#include "Shared/lua_runtime/lua_value_codec.h"
#include "game_server.h"
#include "lua.h"
#include "lua_item_ground.h"
#include "shared/event.h"

namespace lua {
namespace bindings {

namespace {

using LuaProxyArgs = ::gmp::lua::LuaEventProxyArgs;
::gmp::lua::LuaEventProxyMap kLuaEventProxies;

std::optional<std::string> CurrentEventOwner() {
  auto* owner = ResourceManager::GetCurrentResource();
  if (!ResourceManager::GetActiveInstance() || !owner)
    return std::nullopt;
  return owner->GetName();
}

void ExecuteResourceEvent(const std::string& owner, std::function<void()> callback) {
  auto* manager = ResourceManager::GetActiveInstance();
  const auto resource = manager ? manager->GetResource(owner) : std::nullopt;
  if (!resource || !resource->get().IsLoaded())
    return;
  ResourceManager::ScopedResourceContext context(resource->get());
  callback();
}

::gmp::lua::LuaEventBindings kEventBindings{kLuaEventProxies, {CurrentEventOwner, ExecuteResourceEvent}};

sol::object MakeInstanceOrNil(sol::state_view lua, const std::optional<std::string>& instance) {
  return instance && !instance->empty() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::nil);
}

/* luagmp (event)
 *
 * Reports NPC creation at the end of the server tick. The NPC may already be spawned or destroyed.
 *
 * @version  0.3.0
 * @name     onNpcCreated
 * @side     server
 * @category NPC
 * @param    (number) npc_id  Created NPC id.
 *
 */
void Proxy_OnNpcCreated(LuaProxyArgs args) {
  const auto event = std::any_cast<OnNpcCreatedEvent>(args.event);
  args.callback(event.npc_id);
}

/* luagmp (event)
 *
 * Triggered after a server-owned NPC is destroyed. The id is no longer valid.
 *
 * @version  0.3.0
 * @name     onNpcDestroyed
 * @side     server
 * @category NPC
 * @param    (number) npc_id  Destroyed NPC id.
 *
 */
void Proxy_OnNpcDestroyed(LuaProxyArgs args) {
  const auto event = std::any_cast<OnNpcDestroyedEvent>(args.event);
  args.callback(event.npc_id);
}

/* luagmp (event)
 *
 * Triggered when the player responsible for executing an NPC's actions changes.
 *
 * @version  0.3.0
 * @name     onNpcChangeHostPlayer
 * @side     server
 * @category NPC
 * @param    (number) npc_id       NPC id.
 * @param    (number|nil) previous_id Previous host id, or nil when unhosted.
 * @param    (number|nil) current_id New host id, or nil when unhosted.
 *
 */
void Proxy_OnNpcChangeHostPlayer(LuaProxyArgs args) {
  const auto event = std::any_cast<OnNpcChangeHostPlayerEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  args.callback(event.npc_id, event.previous_id == 0 ? sol::make_object(lua, sol::nil) : sol::make_object(lua, event.previous_id),
                event.current_id == 0 ? sol::make_object(lua, sol::nil) : sol::make_object(lua, event.current_id));
}

/* luagmp (event)
 *
 * Triggered when the current action is dispatched to its host. A replay after host migration may emit it again.
 *
 * @version  0.3.0
 * @name     onNpcActionSent
 * @side     server
 * @category NPC
 * @param    (number) npc_id       NPC id.
 * @param    (number) action_type  ACTION_PLAY_ANI.
 * @param    (number) action_id    Dispatched action id.
 *
 */
void Proxy_OnNpcActionSent(LuaProxyArgs args) {
  const auto event = std::any_cast<OnNpcActionSentEvent>(args.event);
  args.callback(event.npc_id, event.action_type, event.action_id);
}

/* luagmp (event)
 *
 * Triggered when an action leaves the queue through completion, failure, cancellation or timeout.
 *
 * @version  0.3.0
 * @name     onNpcActionFinished
 * @side     server
 * @category NPC
 * @param    (number) npc_id       NPC id.
 * @param    (number) action_type  ACTION_PLAY_ANI.
 * @param    (number) action_id    Finished action id.
 * @param    (boolean) result     True only for successful completion reported by the current host.
 *
 */
void Proxy_OnNpcActionFinished(LuaProxyArgs args) {
  const auto event = std::any_cast<OnNpcActionFinishedEvent>(args.event);
  args.callback(event.npc_id, event.action_type, event.action_id, event.result);
}

/* luagmp (event)
 *
 * This event is triggered in every server main loop iteration.
 *
 * @version  0.3.0
 * @name     onTick
 * @side     server
 * @category Game
 *
 */
void Proxy_OnTick(LuaProxyArgs args) {
  std::any_cast<OnTickEvent>(args.event);
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered every time the server clock updates.
 *
 * @version  0.3.0
 * @name     onClockUpdate
 * @side     server
 * @category Game
 * @param    (number) day           The current ingame day.
 * @param    (number) hour          The current ingame hour.
 * @param    (number) min           The current ingame minute.
 *
 */
void Proxy_OnClockUpdate(LuaProxyArgs args) {
  OnClockUpdateEvent gametime_event = std::any_cast<OnClockUpdateEvent>(args.event);
  args.callback(gametime_event.day, gametime_event.hour, gametime_event.min);
}

/* luagmp (event)
 *
 * This event is triggered when a player connects to the server.
 *
 * @version  0.3.0
 * @name     onPlayerConnect
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player that connected.
 *
 */
void Proxy_OnPlayerConnect(LuaProxyArgs args) {
  std::uint32_t player_id = std::any_cast<std::uint32_t>(args.event);
  args.callback(player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player disconnects from the server.
 *
 * @version  0.3.0
 * @name     onPlayerDisconnect
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player that disconnected.
 * @param    (number) reason       The reason why player got disconnected. For more information, see [Network
 * Constants](../../server-constants/Network.md).
 *
 */
void Proxy_OnPlayerDisconnect(LuaProxyArgs args) {
  OnPlayerDisconnectEvent player_disconnect_event = std::any_cast<OnPlayerDisconnectEvent>(args.event);
  args.callback(player_disconnect_event.player_id, player_disconnect_event.reason);
}

/* luagmp (event)
 *
 * This event is triggered when a player sends a chat message.
 *
 * @version  0.3.0
 * @name     onPlayerMessage
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player who sent the message.
 * @param    (string) text         The message text.
 *
 */
void Proxy_OnPlayerMessage(LuaProxyArgs args) {
  OnPlayerMessageEvent player_message_event = std::any_cast<OnPlayerMessageEvent>(args.event);
  args.callback(player_message_event.pid, player_message_event.text);
}

/* luagmp (event)
 *
 * This event is triggered when a player issues a command.
 *
 * @version  0.3.0
 * @name     onPlayerCommand
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player issuing the command.
 * @param    (string) command      The command name.
 * @param    ({...}) params        Command parameters.
 *
 */
void Proxy_OnPlayerCommand(LuaProxyArgs args) {
  OnPlayerCommandEvent player_command_event = std::any_cast<OnPlayerCommandEvent>(args.event);
  args.callback(player_command_event.pid, player_command_event.command, player_command_event.params);
}

/* luagmp (event)
 *
 * This event is triggered when a player dies.
 *
 * @version  0.3.0
 * @name     onPlayerDeath
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player who died.
 * @param    (number|nil) killer_id Optional id of the killer (nil if none).
 *
 */
void Proxy_OnPlayerDeath(LuaProxyArgs args) {
  OnPlayerDeathEvent player_death_event = std::any_cast<OnPlayerDeathEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object killer = player_death_event.killer_id.has_value() ? sol::make_object(lua, player_death_event.killer_id.value()) : sol::lua_nil;
  args.callback(player_death_event.player_id, killer);
}

/* luagmp (event)
 *
 * This event is triggered when a player stands up after being unconscious.
 *
 * @version  0.3.0
 * @name     onPlayerStandUp
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player who stood up.
 *
 */
void Proxy_OnPlayerStandUp(LuaProxyArgs args) {
  OnPlayerStandUpEvent player_standup_event = std::any_cast<OnPlayerStandUpEvent>(args.event);
  args.callback(player_standup_event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player becomes unconscious.
 *
 * @version  0.3.0
 * @name     onPlayerUnconscious
 * @side     server
 * @category Player
 * @param    (number|nil) attacker_id Optional attacker id (nil if none).
 * @param    (number) victim_id      Victim player or NPC id.
 *
 */
void Proxy_OnPlayerUnconscious(LuaProxyArgs args) {
  OnPlayerUnconsciousEvent player_unconscious_event = std::any_cast<OnPlayerUnconsciousEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object attacker =
      player_unconscious_event.attacker_id.has_value() ? sol::make_object(lua, player_unconscious_event.attacker_id.value()) : sol::lua_nil;
  args.callback(attacker, player_unconscious_event.victim_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player drops an item.
 *
 * @version  0.3.0
 * @name     onPlayerDropItem
 * @side     server
 * @category Player
 * @param    (number) player_id        Player id who dropped the item.
 * @param    (ItemGround) item_ground  Ground item object.
 *
 */
void Proxy_OnPlayerDropItem(LuaProxyArgs args) {
  OnPlayerDropItemEvent drop_item_event = std::any_cast<OnPlayerDropItemEvent>(args.event);
  args.callback(drop_item_event.pid, LuaItemGround(drop_item_event.item_ground_id));
}

/* luagmp (event)
 *
 * This event is triggered when a player picks up an item.
 *
 * @version  0.3.0
 * @name     onPlayerTakeItem
 * @side     server
 * @category Player
 * @param    (number) player_id        Player id who took the item.
 * @param    (ItemGround) item_ground  Ground item object.
 *
 */
void Proxy_OnPlayerTakeItem(LuaProxyArgs args) {
  OnPlayerTakeItemEvent take_item_event = std::any_cast<OnPlayerTakeItemEvent>(args.event);
  args.callback(take_item_event.pid, LuaItemGround(take_item_event.item_ground_id));
}

/* luagmp (event)
 *
 * This event is triggered when the player/npc attempts to use an item.
 *
 * @version  0.3.0
 * @name     onPlayerUseItem
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player/npc using the item.
 * @param    (string|nil) item          Item instance name, or nil for an empty slot.
 * @param    (number) quantity       Amount consumed; zero for a reusable item.
 * @param    (string) scheme         Native use scheme reported by the controlling client.
 *
 */
void Proxy_OnPlayerUseItem(LuaProxyArgs args) {
  const auto event = std::any_cast<OnPlayerUseItemEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  args.callback(event.player_id, MakeInstanceOrNil(lua, event.item_instance), event.quantity, event.scheme);
}

/* luagmp (event)
 *
 * This event is triggered when the player/npc attempts to cast a spell.
 *
 * @version  0.3.0
 * @name     onPlayerCastSpell
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) caster_id    Caster player or server NPC id.
 * @param    (number) spell_id     Spell identifier.
 * @param    (number|nil) target_id Optional target player or server NPC id (nil if none).
 *
 */
void Proxy_OnPlayerCastSpell(LuaProxyArgs args) {
  OnPlayerCastSpellEvent cast_spell_event = std::any_cast<OnPlayerCastSpellEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object target = cast_spell_event.target_id.has_value() ? sol::make_object(lua, cast_spell_event.target_id.value()) : sol::lua_nil;
  args.callback(cast_spell_event.caster_id, cast_spell_event.spell_id, target);
}

/* luagmp (event)
 *
 * This event is triggered when a player summons an NPC using a game's spell.
 *
 * @version  0.3.0
 * @name     onPlayerSummon
 * @side     server
 * @category Player
 * @param    (number) playerId       The id of the player who summoned the NPC.
 * @param    (number) spellId        The id of the spell used to summon the NPC.
 * @param    (string) summonInstance The instance name of the summoned NPC.
 * @param    (number) npcId          The id of the summoned NPC.
 *
 */
void Proxy_OnPlayerSummon(LuaProxyArgs args) {
  const auto event = std::any_cast<OnPlayerSummonEvent>(args.event);
  args.callback(event.player_id, event.spell_id, event.summon_instance, event.npc_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player teleports using a game's spell or teleport trigger.
 *
 * @version  0.3.0
 * @name     onPlayerTeleport
 * @side     server
 * @category Player
 * @note     This event is triggered after the teleport is completed and cannot be cancelled. Calling [setPlayerPosition](../../server-functions/player/setPlayerPosition.md) does not trigger this event.
 * @param    (number) playerId The id of the player who teleported.
 * @param    (string) vobName  The name of the destination waypoint, freepoint or vob.
 *
 */
void Proxy_OnPlayerTeleport(LuaProxyArgs args) {
  const auto event = std::any_cast<OnPlayerTeleportEvent>(args.event);
  args.callback(event.player_id, event.destination);
}

/* luagmp (event)
 *
 * This event is triggered when player health changes.
 *
 * @version  0.3.0
 * @name     onPlayerChangeHealth
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player whose health points got changed.
 * @param    (number) oldHP        The previous health points of the player.
 * @param    (number) newHP        The new health points of the player.
 *
 */
void Proxy_OnPlayerChangeHealth(LuaProxyArgs args) {
  OnPlayerChangeHealthEvent health_event = std::any_cast<OnPlayerChangeHealthEvent>(args.event);
  args.callback(health_event.player_id, health_event.old_hp, health_event.new_hp);
}

/* luagmp (event)
 *
 * This event is triggered when player mana changes.
 *
 * @version  0.3.0
 * @name     onPlayerChangeMana
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player whose mana points got changed.
 * @param    (number) previous     The previous mana points of the player.
 * @param    (number) current      The current mana points of the player.
 *
 */
void Proxy_OnPlayerChangeMana(LuaProxyArgs args) {
  OnPlayerChangeManaEvent mana_event = std::any_cast<OnPlayerChangeManaEvent>(args.event);
  args.callback(mana_event.player_id, mana_event.previous, mana_event.current);
}

/* luagmp (event)
 *
 * This event is triggered when player tries to change the played world (ZEN).
 *
 * @version  0.3.0
 * @name     onPlayerWorldChange
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player who tries to change the played world.
 * @param    (string) world        The filename of the world.
 * @param    (string) waypoint     The name of the waypoint that the player will be teleported to.
 *
 */
void Proxy_OnPlayerWorldChange(LuaProxyArgs args) {
  OnPlayerChangeWorldEvent world_event = std::any_cast<OnPlayerChangeWorldEvent>(args.event);
  args.callback(world_event.player_id, world_event.world, world_event.waypoint);
}

/* luagmp (event)
 *
 * This event is triggered when player entered the world and was spawned in it.
 *
 * @version  0.3.0
 * @name     onPlayerWorldEnter
 * @side     server
 * @category Player
 * @param    (number) player_id    The id of the player who entered the world.
 * @param    (string) world        The filename of the world.
 *
 */
void Proxy_OnPlayerWorldEnter(LuaProxyArgs args) {
  OnPlayerWorldEnterEvent world_event = std::any_cast<OnPlayerWorldEnterEvent>(args.event);
  args.callback(world_event.player_id, world_event.world);
}

/* luagmp (event)
 *
 * This event is triggered when a player's weapon mode changes.
 *
 * @version  0.3.0
 * @name     onPlayerWeaponModeChange
 * @side     server
 * @category Player
 * @param    (number) player_id  Player id.
 * @param    (number) old_mode   Previous weapon mode. For more information, see [Weapon Mode Constants](../../shared-constants/WeaponMode.md).
 * @param    (number) new_mode   New weapon mode from the same table.
 *
 */
void Proxy_OnPlayerWeaponModeChange(LuaProxyArgs args) {
  OnPlayerWeaponModeChangeEvent weapon_mode_event = std::any_cast<OnPlayerWeaponModeChangeEvent>(args.event);
  args.callback(weapon_mode_event.player_id, weapon_mode_event.old_mode, weapon_mode_event.new_mode);
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped amulet changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipAmulet
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped amulet item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipAmulet(LuaProxyArgs args) {
  OnPlayerEquipAmuletEvent amulet_event = std::any_cast<OnPlayerEquipAmuletEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, amulet_event.item_instance);
  args.callback(amulet_event.player_id, item, MakeInstanceOrNil(lua, amulet_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped armor changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipArmor
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped armor item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipArmor(LuaProxyArgs args) {
  OnPlayerEquipArmorEvent armor_event = std::any_cast<OnPlayerEquipArmorEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, armor_event.item_instance);
  args.callback(armor_event.player_id, item, MakeInstanceOrNil(lua, armor_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped belt changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipBelt
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped belt item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipBelt(LuaProxyArgs args) {
  OnPlayerEquipBeltEvent belt_event = std::any_cast<OnPlayerEquipBeltEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, belt_event.item_instance);
  args.callback(belt_event.player_id, item, MakeInstanceOrNil(lua, belt_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped hand item changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipHandItem
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (number) hand           Hand id (0 = left, 1 = right).
 * @param    (string|nil) item         Equipped hand item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipHandItem(LuaProxyArgs args) {
  OnPlayerEquipHandItemEvent hand_item_event = std::any_cast<OnPlayerEquipHandItemEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, hand_item_event.item_instance);
  args.callback(hand_item_event.player_id, hand_item_event.hand, item, MakeInstanceOrNil(lua, hand_item_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped helmet changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipHelmet
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped helmet item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipHelmet(LuaProxyArgs args) {
  OnPlayerEquipHelmetEvent helmet_event = std::any_cast<OnPlayerEquipHelmetEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, helmet_event.item_instance);
  args.callback(helmet_event.player_id, item, MakeInstanceOrNil(lua, helmet_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped melee weapon changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipMeleeWeapon
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped melee weapon item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipMeleeWeapon(LuaProxyArgs args) {
  OnPlayerEquipMeleeWeaponEvent melee_weapon_event = std::any_cast<OnPlayerEquipMeleeWeaponEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, melee_weapon_event.item_instance);
  args.callback(melee_weapon_event.player_id, item, MakeInstanceOrNil(lua, melee_weapon_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped ranged weapon changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipRangedWeapon
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped ranged weapon item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipRangedWeapon(LuaProxyArgs args) {
  OnPlayerEquipRangedWeaponEvent ranged_weapon_event = std::any_cast<OnPlayerEquipRangedWeaponEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, ranged_weapon_event.item_instance);
  args.callback(ranged_weapon_event.player_id, item, MakeInstanceOrNil(lua, ranged_weapon_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped ring changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipRing
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (number) ring_slot      Logical ring slot (0 or 1).
 * @param    (string|nil) item         Equipped ring item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipRing(LuaProxyArgs args) {
  OnPlayerEquipRingEvent ring_event = std::any_cast<OnPlayerEquipRingEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, ring_event.item_instance);
  args.callback(ring_event.player_id, ring_event.ring_slot, item, MakeInstanceOrNil(lua, ring_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's equipped shield changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipShield
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (string|nil) item         Equipped shield item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipShield(LuaProxyArgs args) {
  OnPlayerEquipShieldEvent shield_event = std::any_cast<OnPlayerEquipShieldEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, shield_event.item_instance);
  args.callback(shield_event.player_id, item, MakeInstanceOrNil(lua, shield_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player's active spell slot changes.
 *
 * @version  0.3.0
 * @name     onPlayerEquipSpellSlot
 * @side     server
 * @category Player
 * @note     This event can be cancelled.
 * @param    (number) player_id      Player id.
 * @param    (number) slot_id        Active spell slot id (0 if none).
 * @param    (string|nil) item         Active spell item (nil if none).
 * @param    (string|nil) previous_item Previously equipped item.
 *
 */
void Proxy_OnPlayerEquipSpellSlot(LuaProxyArgs args) {
  OnPlayerEquipSpellSlotEvent spell_event = std::any_cast<OnPlayerEquipSpellSlotEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item = MakeInstanceOrNil(lua, spell_event.item_instance);
  args.callback(spell_event.player_id, spell_event.slot_id, item, MakeInstanceOrNil(lua, spell_event.previous_item_instance));
}

/* luagmp (event)
 *
 * This event is triggered when a player spawns (initial spawn).
 *
 * @version  0.3.0
 * @name     onPlayerSpawn
 * @side     server
 * @category Player
 * @param    (number) player_id    Player id spawned.
 * @param    (number) x         X coordinate of spawn.
 * @param    (number) y         Y coordinate of spawn.
 * @param    (number) z         Z coordinate of spawn.
 *
 */
void Proxy_OnPlayerSpawn(LuaProxyArgs args) {
  OnPlayerSpawnEvent player_spawn_event = std::any_cast<OnPlayerSpawnEvent>(args.event);
  args.callback(player_spawn_event.player_id, player_spawn_event.position.x, player_spawn_event.position.y, player_spawn_event.position.z);
}

/* luagmp (event)
 *
 * This event is triggered when a player respawns.
 *
 * @version  0.3.0
 * @name     onPlayerRespawn
 * @side     server
 * @category Player
 * @param    (number) player_id    Player id respawned.
 * @param    (number) x         X coordinate of respawn.
 * @param    (number) y         Y coordinate of respawn.
 * @param    (number) z         Z coordinate of respawn.
 *
 */
void Proxy_OnPlayerRespawn(LuaProxyArgs args) {
  OnPlayerRespawnEvent player_respawn_event = std::any_cast<OnPlayerRespawnEvent>(args.event);
  args.callback(player_respawn_event.player_id, player_respawn_event.position.x, player_respawn_event.position.y, player_respawn_event.position.z);
}

/* luagmp (event)
 *
 * This event is triggered when a player is spawned for another player (streaming in).
 *
 * @version  0.3.0
 * @name     onPlayerSpawnFor
 * @side     server
 * @category Player
 * @param    (number) player_id    Player id receiving the spawn.
 * @param    (number) spawn_id     Player id spawned for the receiver.
 *
 */
void Proxy_OnPlayerSpawnFor(LuaProxyArgs args) {
  OnPlayerSpawnForEvent player_spawn_for_event = std::any_cast<OnPlayerSpawnForEvent>(args.event);
  args.callback(player_spawn_for_event.player_id, player_spawn_for_event.spawn_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player is unspawned for another player (streaming out).
 *
 * @version  0.3.0
 * @name     onPlayerUnspawnFor
 * @side     server
 * @category Player
 * @param    (number) player_id    Player id losing the spawn.
 * @param    (number) spawn_id     Player or NPC id removed for the viewer.
 *
 */
void Proxy_OnPlayerUnspawnFor(LuaProxyArgs args) {
  OnPlayerUnspawnForEvent player_unspawn_for_event = std::any_cast<OnPlayerUnspawnForEvent>(args.event);
  args.callback(player_unspawn_for_event.player_id, player_unspawn_for_event.spawn_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player is hit.
 *
 * @version  0.3.0
 * @name     onPlayerHit
 * @side     server
 * @category Player
 * @param    (number|nil) attacker_id  Optional attacker id (nil if none).
 * @param    (number) victim_id     Victim player or NPC id.
 * @param    (number) damage        Damage to deal. Use [setEventValue](../../shared-functions/event/setEventValue.md)(newDamage) to change it, or
 * [cancelEvent](../../shared-functions/event/cancelEvent.md)() to prevent it.
 * @param    (number) damageType    Gothic damage type/mode flags. For more information, see [Damage Constants](../../shared-constants/Damage.md).
 *
 */
void Proxy_OnPlayerHit(LuaProxyArgs args) {
  OnPlayerHitEvent player_hit_event = std::any_cast<OnPlayerHitEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object attacker = player_hit_event.attacker_id.has_value() ? sol::make_object(lua, player_hit_event.attacker_id.value()) : sol::lua_nil;
  args.callback(attacker, player_hit_event.victim_id, player_hit_event.damage, player_hit_event.damage_type);
}

/* luagmp (event)
 *
 * This event is triggered when validated voice packets start arriving from a player.
 *
 * @version  0.3.0
 * @name     onPlayerVoiceStart
 * @side     server
 * @category Voice
 * @param    (number) player_id  Speaking player id.
 *
 */
void Proxy_OnPlayerVoiceStart(LuaProxyArgs args) {
  OnPlayerVoiceEvent event = std::any_cast<OnPlayerVoiceEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered after a player's validated voice traffic stops.
 *
 * @version  0.3.0
 * @name     onPlayerVoiceStop
 * @side     server
 * @category Voice
 * @param    (number) player_id  Player id.
 *
 */
void Proxy_OnPlayerVoiceStop(LuaProxyArgs args) {
  OnPlayerVoiceEvent event = std::any_cast<OnPlayerVoiceEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player changes voice channels.
 *
 * @version  0.3.0
 * @name     onPlayerVoiceChannelChange
 * @side     server
 * @category Voice
 * @param    (number) player_id    Player id.
 * @param    (string) old_channel  Previous channel name.
 * @param    (string) new_channel  New channel name.
 *
 */
void Proxy_OnPlayerVoiceChannelChange(LuaProxyArgs args) {
  OnPlayerVoiceChannelChangeEvent event = std::any_cast<OnPlayerVoiceChannelChangeEvent>(args.event);
  args.callback(event.player_id, event.old_channel, event.new_channel);
}

void RegisterProxies() {
  kLuaEventProxies[kEventOnNpcCreatedName] = Proxy_OnNpcCreated;

  kLuaEventProxies[kEventOnNpcDestroyedName] = Proxy_OnNpcDestroyed;

  kLuaEventProxies[kEventOnNpcChangeHostPlayerName] = Proxy_OnNpcChangeHostPlayer;

  kLuaEventProxies[kEventOnNpcActionSentName] = Proxy_OnNpcActionSent;

  kLuaEventProxies[kEventOnNpcActionFinishedName] = Proxy_OnNpcActionFinished;

  kLuaEventProxies[kEventOnTickName] = Proxy_OnTick;

  kLuaEventProxies[kEventOnClockUpdateName] = Proxy_OnClockUpdate;

  kLuaEventProxies[kEventOnPlayerConnectName] = Proxy_OnPlayerConnect;

  kLuaEventProxies[kEventOnPlayerDisconnectName] = Proxy_OnPlayerDisconnect;

  kLuaEventProxies[kEventOnPlayerMessageName] = Proxy_OnPlayerMessage;

  kLuaEventProxies[kEventOnPlayerCommandName] = Proxy_OnPlayerCommand;

  kLuaEventProxies[kEventOnPlayerDeathName] = Proxy_OnPlayerDeath;

  kLuaEventProxies[kEventOnPlayerStandUpName] = Proxy_OnPlayerStandUp;

  kLuaEventProxies[kEventOnPlayerUnconsciousName] = Proxy_OnPlayerUnconscious;

  kLuaEventProxies[kEventOnPlayerDropItemName] = Proxy_OnPlayerDropItem;

  kLuaEventProxies[kEventOnPlayerTakeItemName] = Proxy_OnPlayerTakeItem;

  kLuaEventProxies[kEventOnPlayerUseItemName] = Proxy_OnPlayerUseItem;

  kLuaEventProxies[kEventOnPlayerCastSpellName] = Proxy_OnPlayerCastSpell;

  kLuaEventProxies[kEventOnPlayerSummonName] = Proxy_OnPlayerSummon;

  kLuaEventProxies[kEventOnPlayerTeleportName] = Proxy_OnPlayerTeleport;

  kLuaEventProxies[kEventOnPlayerChangeHealthName] = Proxy_OnPlayerChangeHealth;

  kLuaEventProxies[kEventOnPlayerChangeManaName] = Proxy_OnPlayerChangeMana;

  kLuaEventProxies[kEventOnPlayerWorldChangeName] = Proxy_OnPlayerWorldChange;

  kLuaEventProxies[kEventOnPlayerWorldEnterName] = Proxy_OnPlayerWorldEnter;

  kLuaEventProxies[kEventOnPlayerWeaponModeChangeName] = Proxy_OnPlayerWeaponModeChange;

  kLuaEventProxies[kEventOnPlayerEquipAmuletName] = Proxy_OnPlayerEquipAmulet;

  kLuaEventProxies[kEventOnPlayerEquipArmorName] = Proxy_OnPlayerEquipArmor;

  kLuaEventProxies[kEventOnPlayerEquipBeltName] = Proxy_OnPlayerEquipBelt;

  kLuaEventProxies[kEventOnPlayerEquipHandItemName] = Proxy_OnPlayerEquipHandItem;

  kLuaEventProxies[kEventOnPlayerEquipHelmetName] = Proxy_OnPlayerEquipHelmet;

  kLuaEventProxies[kEventOnPlayerEquipMeleeWeaponName] = Proxy_OnPlayerEquipMeleeWeapon;

  kLuaEventProxies[kEventOnPlayerEquipRangedWeaponName] = Proxy_OnPlayerEquipRangedWeapon;

  kLuaEventProxies[kEventOnPlayerEquipRingName] = Proxy_OnPlayerEquipRing;

  kLuaEventProxies[kEventOnPlayerEquipShieldName] = Proxy_OnPlayerEquipShield;

  kLuaEventProxies[kEventOnPlayerEquipSpellSlotName] = Proxy_OnPlayerEquipSpellSlot;

  kLuaEventProxies[kEventOnPlayerSpawnName] = Proxy_OnPlayerSpawn;

  kLuaEventProxies[kEventOnPlayerRespawnName] = Proxy_OnPlayerRespawn;

  kLuaEventProxies[kEventOnPlayerSpawnForName] = Proxy_OnPlayerSpawnFor;

  kLuaEventProxies[kEventOnPlayerUnspawnForName] = Proxy_OnPlayerUnspawnFor;

  kLuaEventProxies[kEventOnPlayerHitName] = Proxy_OnPlayerHit;

  kLuaEventProxies[kEventOnPlayerVoiceStartName] = Proxy_OnPlayerVoiceStart;

  kLuaEventProxies[kEventOnPlayerVoiceStopName] = Proxy_OnPlayerVoiceStop;

  kLuaEventProxies[kEventOnPlayerVoiceChannelChangeName] = Proxy_OnPlayerVoiceChannelChange;
}

}  // namespace

/* luagmp (func)
 * Sends payload arguments to a client custom event. The server is the sender; options.sourceId is script metadata.
 * @name triggerClientEvent
 * @side server
 * @category Event
 * @param (number|table|nil) recipients Player id, array of player ids, or nil for all players.
 * @param (string) name Remote-enabled client event name.
 * @param (table|nil) options Metadata {sourceId = positive integer}; pass nil for no metadata.
 * @param (any...) ... Serializable payload arguments, including nil.
 * @return (boolean) Whether the event was sent.
 */
auto Function_Events_TriggerClientEvent(sol::object recipients, const std::string& name, sol::object options, sol::variadic_args args,
                                        sol::this_state ts) {
  sol::state_view lua(ts);
  if (!g_server || name.empty())
    return false;
  const auto source = gmp::lua::ReadEventSource(options);
  std::vector<GameServer::PlayerId> targets;
  if (recipients.get_type() == sol::type::nil) {
    for (const auto& entry : g_server->GetPlayerManager().GetAllPlayers()) targets.push_back(entry.first);
  } else if (recipients.get_type() == sol::type::table) {
    for (const auto& entry : recipients.as<sol::table>()) targets.push_back(gmp::lua::ReadEventId(entry.second));
  } else {
    targets.push_back(gmp::lua::ReadEventId(recipients));
  }
  if (targets.empty())
    return false;
  std::string encoded;
  std::string error;
  if (!gmp::lua::EncodeLuaArgs(lua, args, encoded, error)) {
    SPDLOG_ERROR("triggerClientEvent '{}': {}", name, error);
    return false;
  }
  return g_server->TriggerClientEvent(targets, name, source.value_or(0), encoded);
}

void RemoveHandlersForResource(const std::string& owner_name) {
  kEventBindings.RemoveHandlersForOwner(owner_name);
}

bool TriggerRemoteEvent(sol::state_view lua, const std::string& event_name, std::uint32_t sender_player_id, std::uint32_t source_element,
                        const std::vector<sol::object>& args) {
  (void)lua;
  ::gmp::lua::EventMetadata metadata;
  metadata.sender_player_id = sender_player_id;
  if (source_element != 0)
    metadata.source_element = source_element;
  return kEventBindings.TriggerRemoteEvent(event_name, metadata, args);
}

void BindEvents(sol::state& lua) {
  RegisterProxies();
  ::gmp::lua::BindCommonEvents(lua, kEventBindings);
  lua["triggerClientEvent"] = Function_Events_TriggerClientEvent;
}

}  // namespace bindings
}  // namespace lua

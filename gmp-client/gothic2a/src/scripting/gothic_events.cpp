/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "gothic_events.h"

#include <spdlog/spdlog.h>

#include <any>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_diagnostics.h"
#include "Shared/lua_runtime/lua_events.h"
#include "Shared/lua_runtime/lua_value_codec.h"
#include "item_ground.h"
#include "net_game.h"
#include "shared/event.h"

namespace gmp::gothic {

namespace {

using LuaProxyArgs = ::gmp::lua::LuaEventProxyArgs;
::gmp::lua::LuaEventProxyMap g_gothic_event_proxies;
::gmp::lua::LuaEventBindings g_event_bindings{g_gothic_event_proxies};

/* luagmp (event)
 *
 * This event is triggered once when the player connects to the server.
 *
 * @version  0.3.0
 * @name     onInit
 * @side     client
 * @category Game
 *
 */
void Proxy_OnInit(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the player disconnects from the server.
 *
 * @version  0.3.0
 * @name     onExit
 * @side     client
 * @category Game
 *
 */
void Proxy_OnExit(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered each frame.
 *
 * @version  0.3.0
 * @name     onRender
 * @side     client
 * @category Game
 *
 */
void Proxy_OnRender(LuaProxyArgs args) {
  // onRender has no arguments for now
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered each time the game time minute passes.
 *
 * @version  0.3.0
 * @name     onTime
 * @side     client
 * @category Game
 * @param    (number) day   The current in-game day.
 * @param    (number) hour  The current in-game hour.
 * @param    (number) min   The current in-game minute.
 *
 */
void Proxy_OnTime(LuaProxyArgs args) {
  OnTimeEvent event = std::any_cast<OnTimeEvent>(args.event);
  args.callback(event.day, event.hour, event.min);
}

/* luagmp (event)
 *
 * This event is triggered when weather changes.
 *
 * @version  0.3.0
 * @name     onWeatherChange
 * @side     client
 * @category Weather
 * @param    (number) old_weather_type  Previous weather type. For more information, see [Weather Constants](../../shared-constants/Weather.md).
 * @param    (number) new_weather_type  New weather type from the same table.
 *
 */
void Proxy_OnWeatherChange(LuaProxyArgs args) {
  OnWeatherChangeEvent event = std::any_cast<OnWeatherChangeEvent>(args.event);
  args.callback(event.old_weather_type, event.new_weather_type);
}

/* luagmp (event)
 *
 * This event is triggered when a key is pressed.
 *
 * @version  0.3.0
 * @name     onKeyDown
 * @side     client
 * @category Input
 * @param    (number) key    The key code pressed. For more information, see [Key Constants](../../client-constants/Key.md).
 *
 */
void Proxy_OnKeyDown(LuaProxyArgs args) {
  OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
  args.callback(event.key);
}

/* luagmp (event)
 *
 * This event is triggered when a key is released.
 *
 * @version  0.3.0
 * @name     onKeyUp
 * @side     client
 * @category Input
 * @param    (number) key    The key code released. For more information, see [Key Constants](../../client-constants/Key.md).
 *
 */
void Proxy_OnKeyUp(LuaProxyArgs args) {
  OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
  args.callback(event.key);
}

/* luagmp (event)
 *
 * This event is triggered when the user presses Ctrl+V.
 *
 * @version  0.3.0
 * @name     onPaste
 * @side     client
 * @category Input
 * @param    (string) text  Current clipboard text.
 *
 */
void Proxy_OnPaste(LuaProxyArgs args) {
  OnPasteEvent event = std::any_cast<OnPasteEvent>(args.event);
  args.callback(event.text);
}

/* luagmp (event)
 *
 * This event is triggered when a slash command is submitted through chat input.
 *
 * @version  0.3.0
 * @name     onCommand
 * @side     client
 * @category Input
 * @param    (string) command  Command name without the leading slash.
 * @param    (string) params   Raw command parameters after the command name.
 *
 */
void Proxy_OnCommand(LuaProxyArgs args) {
  OnCommandEvent event = std::any_cast<OnCommandEvent>(args.event);
  args.callback(event.command, event.params);
}

/* luagmp (event)
 *
 * This event is triggered when a mouse button is pressed.
 *
 * @version  0.3.0
 * @name     onMouseDown
 * @side     client
 * @category Mouse
 * @param    (number) button The mouse button pressed. For more information, see [Mouse Constants](../../client-constants/Mouse.md).
 *
 */
void Proxy_OnMouseDown(LuaProxyArgs args) {
  OnMouseButtonEvent event = std::any_cast<OnMouseButtonEvent>(args.event);
  args.callback(event.button);
}

/* luagmp (event)
 *
 * This event is triggered when a mouse button is released.
 *
 * @version  0.3.0
 * @name     onMouseUp
 * @side     client
 * @category Mouse
 * @param    (number) button The mouse button released. For more information, see [Mouse Constants](../../client-constants/Mouse.md).
 *
 */
void Proxy_OnMouseUp(LuaProxyArgs args) {
  OnMouseButtonEvent event = std::any_cast<OnMouseButtonEvent>(args.event);
  args.callback(event.button);
}

/* luagmp (event)
 *
 * This event is triggered when the mouse cursor is moved.
 *
 * @version  0.3.0
 * @name     onMouseMove
 * @side     client
 * @category Mouse
 * @param    (number) x cursor X position.
 * @param    (number) y cursor Y position.
 *
 */
void Proxy_OnMouseMove(LuaProxyArgs args) {
  OnMouseMoveEvent event = std::any_cast<OnMouseMoveEvent>(args.event);
  args.callback(event.x, event.y);
}

/* luagmp (event)
 *
 * This event is triggered when the mouse wheel is scrolled.
 *
 * @version  0.3.0
 * @name     onMouseWheel
 * @side     client
 * @category Mouse
 * @param    (number) z Mouse wheel delta.
 *
 */
void Proxy_OnMouseWheel(LuaProxyArgs args) {
  OnMouseWheelEvent event = std::any_cast<OnMouseWheelEvent>(args.event);
  args.callback(event.z);
}

/* luagmp (event)
 *
 * This event is triggered when the user opens the inventory.
 *
 * @version  0.3.0
 * @name     onOpenInventory
 * @side     client
 * @category Inventory
 *
 */
void Proxy_OnOpenInventory(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the user closes the inventory.
 *
 * @version  0.3.0
 * @name     onCloseInventory
 * @side     client
 * @category Inventory
 *
 */
void Proxy_OnCloseInventory(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the selected inventory slot changes.
 *
 * @version  0.3.0
 * @name     onInventorySlotChange
 * @side     client
 * @category Inventory
 * @param    (number) from  Previous slot number.
 * @param    (number) to    Current slot number.
 *
 */
void Proxy_OnInventorySlotChange(LuaProxyArgs args) {
  OnInventorySlotChangeEvent event = std::any_cast<OnInventorySlotChangeEvent>(args.event);
  args.callback(event.from, event.to);
}

/* luagmp (event)
 *
 * This event is triggered when the client requests a world change via oCTriggerChangeLevel vob.
 *
 * @version  0.3.0
 * @name     onWorldChange
 * @side     client
 * @category World
 * @param    (string) world     New world filename.
 * @param    (string) waypoint  Waypoint name the player will be teleported to.
 *
 */
void Proxy_OnWorldChange(LuaProxyArgs args) {
  OnWorldChangeEvent event = std::any_cast<OnWorldChangeEvent>(args.event);
  args.callback(event.world, event.waypoint);
}

/* luagmp (event)
 *
 * This event is triggered when the player enters a world.
 *
 * @version  0.3.0
 * @name     onWorldEnter
 * @side     client
 * @category World
 * @param    (string) world  World filename.
 *
 */
void Proxy_OnWorldEnter(LuaProxyArgs args) {
  OnWorldEnterEvent event = std::any_cast<OnWorldEnterEvent>(args.event);
  args.callback(event.world);
}

/* luagmp (event)
 *
 * This event is triggered when the hero equips an item.
 *
 * @version  0.3.0
 * @name     onEquip
 * @side     client
 * @category Hero
 * @param    (string) item  Item instance name.
 *
 */
void Proxy_OnEquip(LuaProxyArgs args) {
  OnItemEvent event = std::any_cast<OnItemEvent>(args.event);
  args.callback(event.item);
}

/* luagmp (event)
 *
 * This event is triggered when the hero drops an item.
 *
 * @version  0.3.0
 * @name     onDropItem
 * @side     client
 * @category Hero
 * @param    (string) item    Item instance name.
 * @param    (number) amount  Item quantity.
 *
 */
void Proxy_OnDropItem(LuaProxyArgs args) {
  OnDropItemEvent event = std::any_cast<OnDropItemEvent>(args.event);
  args.callback(event.item, event.amount);
}

/* luagmp (event)
 *
 * This event is triggered when the hero takes an item from the ground.
 *
 * @version  0.3.0
 * @name     onTakeItem
 * @side     client
 * @category Hero
 * @param    (string) item           Item instance name.
 * @param    (boolean) synchronized  True when pickup is synchronized with the server.
 * @param    (number) amount         Item quantity.
 * @param    (number|nil) itemGroundId Ground item id, or nil for non-server items.
 *
 */
void Proxy_OnTakeItem(LuaProxyArgs args) {
  OnTakeItemEvent event = std::any_cast<OnTakeItemEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object item_ground_id = event.item_ground_id.has_value() ? sol::make_object(lua, *event.item_ground_id) : sol::make_object(lua, sol::lua_nil);
  args.callback(event.item, event.synchronized, event.amount, item_ground_id);
}

/* luagmp (event)
 *
 * This event is triggered when the hero uses, interacts with, opens, or consumes an item.
 *
 * @version  0.3.0
 * @name     onUseItem
 * @side     client
 * @category Hero
 * @param    (string) item     Item instance name.
 * @param    (string) scheme   Item scheme name, if available.
 * @param    (number) from     Previous interact state.
 * @param    (number) to       Current interact state.
 *
 */
void Proxy_OnUseItem(LuaProxyArgs args) {
  OnUseItemEvent event = std::any_cast<OnUseItemEvent>(args.event);
  args.callback(event.item, event.scheme, event.from, event.to);
}

/* luagmp (event)
 *
 * This event is triggered when a server-side ground item is created for the client.
 *
 * @version  0.3.0
 * @name     onItemGroundCreate
 * @side     client
 * @category ItemGround
 * @param    (ItemGround) itemGround  Ground item object.
 *
 */
void Proxy_OnItemGroundCreate(LuaProxyArgs args) {
  LuaItemGround event = std::any_cast<LuaItemGround>(args.event);
  args.callback(event);
}

/* luagmp (event)
 *
 * This event is triggered when a server-side ground item is destroyed for the client.
 *
 * @version  0.3.0
 * @name     onItemGroundDestroy
 * @side     client
 * @category ItemGround
 * @param    (ItemGround) itemGround  Ground item object.
 *
 */
void Proxy_OnItemGroundDestroy(LuaProxyArgs args) {
  LuaItemGround event = std::any_cast<LuaItemGround>(args.event);
  args.callback(event);
}

/* luagmp (event)
 *
 * This event is triggered when all server-side ground items are destroyed during world or virtual-world changes.
 *
 * @version  0.3.0
 * @name     onItemsGroundDestroy
 * @side     client
 * @category ItemGround
 *
 */
void Proxy_OnItemsGroundDestroy(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the hero unequips an item.
 *
 * @version  0.3.0
 * @name     onUnequip
 * @side     client
 * @category Hero
 * @param    (string) item  Item instance name.
 *
 */
void Proxy_OnUnequip(LuaProxyArgs args) {
  OnItemEvent event = std::any_cast<OnItemEvent>(args.event);
  args.callback(event.item);
}

/* luagmp (event)
 *
 * This event is triggered when a player object is created locally.
 *
 * @version  0.3.0
 * @name     onPlayerCreate
 * @side     client
 * @category Player
 * @param    (number) player_id    The local player id.
 *
 */
void Proxy_OnPlayerCreate(LuaProxyArgs args) {
  PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a local player object is destroyed.
 *
 * @version  0.3.0
 * @name     onPlayerDestroy
 * @side     client
 * @category Player
 * @param    (number) player_id    The local player id.
 *
 */
void Proxy_OnPlayerDestroy(LuaProxyArgs args) {
  PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a chat message is received locally.
 *
 * @version  0.3.0
 * @name     onPlayerMessage
 * @side     client
 * @category Player
 * @param    (number|nil) sender_id Optional sender id (nil for system).
 * @param    (number) r            The red color component in RGB model.
 * @param    (number) g            The green color component in RGB model.
 * @param    (number) b            The blue color component in RGB model.
 * @param    (string) message      Message text.
 *
 */
void Proxy_OnPlayerMessage(LuaProxyArgs args) {
  OnPlayerMessageEvent event = std::any_cast<OnPlayerMessageEvent>(args.event);
  sol::state_view lua(args.callback.lua_state());
  sol::object sender = event.sender_id.has_value() ? sol::make_object(lua, event.sender_id.value()) : sol::lua_nil;
  args.callback(sender, event.r, event.g, event.b, event.message);
}

/* luagmp (event)
 *
 * This event is triggered when a player respawns after death.
 *
 * @version  0.3.0
 * @name     onPlayerRespawn
 * @side     client
 * @category Player
 * @param    (number) id  The id of the respawned player.
 *
 */
void Proxy_OnPlayerRespawn(LuaProxyArgs args) {
  PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player is spawned into the world.
 *
 * @version  0.3.0
 * @name     onPlayerSpawn
 * @side     client
 * @category Player
 * @param    (number) id  The id of the spawned player.
 *
 */
void Proxy_OnPlayerSpawn(LuaProxyArgs args) {
  PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when a player summons an NPC using a game's spell.
 *
 * @version  0.3.0
 * @name     onPlayerSummon
 * @side     client
 * @category Player
 * @note     The summoned NPC exists when this event is triggered. This event is not triggered when an existing summon streams in.
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
 * @side     client
 * @category Player
 * @note     This event is triggered after the server accepts the teleport. Calling [setPlayerPosition](../../client-functions/player/setPlayerPosition.md) does not trigger this event.
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
 * This event is triggered when a player or NPC dies.
 *
 * @version  0.3.0
 * @name     onPlayerDeath
 * @side     client
 * @category Player
 * @param    (number) id  The id of the player who died.
 *
 */
void Proxy_OnPlayerDeath(LuaProxyArgs args) {
  PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
  args.callback(event.player_id);
}

/* luagmp (event)
 *
 * This event is triggered when the client receives a ping update for a player.
 *
 * @version  0.3.0
 * @name     onPlayerChangePing
 * @side     client
 * @category Player
 * @param    (number) id    The id of the player.
 * @param    (number) ping  The player's ping.
 *
 */
void Proxy_OnPlayerChangePing(LuaProxyArgs args) {
  OnPlayerPingEvent event = std::any_cast<OnPlayerPingEvent>(args.event);
  args.callback(event.player_id, event.ping);
}

/* luagmp (event)
 *
 * This event is triggered when the effective voice chat state changes.
 *
 * @version  0.3.0
 * @name     onVoiceChatStateChange
 * @side     client
 * @category Voice
 * @param    (boolean) enabled  True when voice chat is available and enabled locally.
 * @param    (number) range     Server-configured proximity range in game units.
 *
 */
void Proxy_OnVoiceChatStateChange(LuaProxyArgs args) {
  OnVoiceChatStateEvent event = std::any_cast<OnVoiceChatStateEvent>(args.event);
  args.callback(event.enabled, event.range);
}

/* luagmp (event)
 *
 * This event is triggered when the local player starts transmitting voice.
 *
 * @version  0.3.0
 * @name     onVoiceTransmitStart
 * @side     client
 * @category Voice
 *
 */
void Proxy_OnVoiceTransmitStart(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the local player stops transmitting voice.
 *
 * @version  0.3.0
 * @name     onVoiceTransmitStop
 * @side     client
 * @category Voice
 *
 */
void Proxy_OnVoiceTransmitStop(LuaProxyArgs args) {
  args.callback();
}

/* luagmp (event)
 *
 * This event is triggered when the local player changes voice channels.
 *
 * @version  0.3.0
 * @name     onVoiceChannelChange
 * @side     client
 * @category Voice
 * @param    (string) old_channel  Previous channel name.
 * @param    (string) new_channel  New channel name.
 *
 */
void Proxy_OnVoiceChannelChange(LuaProxyArgs args) {
  OnVoiceChannelChangeEvent event = std::any_cast<OnVoiceChannelChangeEvent>(args.event);
  args.callback(event.old_channel, event.new_channel);
}

/* luagmp (event)
 *
 * This event is triggered when a remote player starts speaking.
 *
 * @version  0.3.0
 * @name     onPlayerVoiceStart
 * @side     client
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
 * This event is triggered after a remote player's voice activity ends.
 *
 * @version  0.3.0
 * @name     onPlayerVoiceStop
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 *
 */
void Proxy_OnPlayerVoiceStop(LuaProxyArgs args) {
  OnPlayerVoiceEvent event = std::any_cast<OnPlayerVoiceEvent>(args.event);
  args.callback(event.player_id);
}

void RegisterGothicEventProxies() {
  g_gothic_event_proxies[kEventOnInitName] = Proxy_OnInit;

  g_gothic_event_proxies[kEventOnExitName] = Proxy_OnExit;

  g_gothic_event_proxies[kEventOnRenderName] = Proxy_OnRender;

  g_gothic_event_proxies[kEventOnTimeName] = Proxy_OnTime;

  g_gothic_event_proxies[kEventOnWeatherChangeName] = Proxy_OnWeatherChange;

  g_gothic_event_proxies[kEventOnKeyDownName] = Proxy_OnKeyDown;

  g_gothic_event_proxies[kEventOnKeyUpName] = Proxy_OnKeyUp;

  g_gothic_event_proxies[kEventOnPasteName] = Proxy_OnPaste;

  g_gothic_event_proxies[kEventOnCommandName] = Proxy_OnCommand;

  g_gothic_event_proxies[kEventOnMouseDownName] = Proxy_OnMouseDown;

  g_gothic_event_proxies[kEventOnMouseUpName] = Proxy_OnMouseUp;

  g_gothic_event_proxies[kEventOnMouseMoveName] = Proxy_OnMouseMove;

  g_gothic_event_proxies[kEventOnMouseWheelName] = Proxy_OnMouseWheel;

  g_gothic_event_proxies[kEventOnOpenInventoryName] = Proxy_OnOpenInventory;

  g_gothic_event_proxies[kEventOnCloseInventoryName] = Proxy_OnCloseInventory;

  g_gothic_event_proxies[kEventOnInventorySlotChangeName] = Proxy_OnInventorySlotChange;

  g_gothic_event_proxies[kEventOnWorldChangeName] = Proxy_OnWorldChange;

  g_gothic_event_proxies[kEventOnWorldEnterName] = Proxy_OnWorldEnter;

  g_gothic_event_proxies[kEventOnEquipName] = Proxy_OnEquip;

  g_gothic_event_proxies[kEventOnDropItemName] = Proxy_OnDropItem;

  g_gothic_event_proxies[kEventOnTakeItemName] = Proxy_OnTakeItem;

  g_gothic_event_proxies[kEventOnUseItemName] = Proxy_OnUseItem;

  g_gothic_event_proxies[kEventOnItemGroundCreateName] = Proxy_OnItemGroundCreate;

  g_gothic_event_proxies[kEventOnItemGroundDestroyName] = Proxy_OnItemGroundDestroy;

  g_gothic_event_proxies[kEventOnItemsGroundDestroyName] = Proxy_OnItemsGroundDestroy;

  g_gothic_event_proxies[kEventOnUnequipName] = Proxy_OnUnequip;

  g_gothic_event_proxies[kEventOnPlayerCreateName] = Proxy_OnPlayerCreate;

  g_gothic_event_proxies[kEventOnPlayerDestroyName] = Proxy_OnPlayerDestroy;

  g_gothic_event_proxies[kEventOnPlayerMessageName] = Proxy_OnPlayerMessage;

  g_gothic_event_proxies[kEventOnPlayerRespawnName] = Proxy_OnPlayerRespawn;

  g_gothic_event_proxies[kEventOnPlayerSpawnName] = Proxy_OnPlayerSpawn;

  g_gothic_event_proxies[kEventOnPlayerSummonName] = Proxy_OnPlayerSummon;

  g_gothic_event_proxies[kEventOnPlayerTeleportName] = Proxy_OnPlayerTeleport;

  g_gothic_event_proxies[kEventOnPlayerDeathName] = Proxy_OnPlayerDeath;

  g_gothic_event_proxies[kEventOnPlayerChangePingName] = Proxy_OnPlayerChangePing;

  g_gothic_event_proxies[kEventOnVoiceChatStateChangeName] = Proxy_OnVoiceChatStateChange;

  g_gothic_event_proxies[kEventOnVoiceTransmitStartName] = Proxy_OnVoiceTransmitStart;

  g_gothic_event_proxies[kEventOnVoiceTransmitStopName] = Proxy_OnVoiceTransmitStop;

  g_gothic_event_proxies[kEventOnVoiceChannelChangeName] = Proxy_OnVoiceChannelChange;

  g_gothic_event_proxies[kEventOnPlayerVoiceStartName] = Proxy_OnPlayerVoiceStart;

  g_gothic_event_proxies[kEventOnPlayerVoiceStopName] = Proxy_OnPlayerVoiceStop;
}

void RegisterGothicEventsInManager() {
  EventManager::Instance().RegisterEvent(kEventOnInitName);
  EventManager::Instance().RegisterEvent(kEventOnExitName);
  EventManager::Instance().RegisterEvent(kEventOnRenderName);
  EventManager::Instance().RegisterEvent(kEventOnTimeName);
  EventManager::Instance().RegisterEvent(kEventOnWeatherChangeName);
  EventManager::Instance().RegisterEvent(kEventOnKeyDownName);
  EventManager::Instance().RegisterEvent(kEventOnKeyUpName);
  EventManager::Instance().RegisterEvent(kEventOnPasteName);
  EventManager::Instance().RegisterEvent(kEventOnCommandName);
  EventManager::Instance().RegisterEvent(kEventOnMouseDownName);
  EventManager::Instance().RegisterEvent(kEventOnMouseUpName);
  EventManager::Instance().RegisterEvent(kEventOnMouseMoveName);
  EventManager::Instance().RegisterEvent(kEventOnMouseWheelName);
  EventManager::Instance().RegisterEvent(kEventOnOpenInventoryName);
  EventManager::Instance().RegisterEvent(kEventOnCloseInventoryName);
  EventManager::Instance().RegisterEvent(kEventOnInventorySlotChangeName);
  EventManager::Instance().RegisterEvent(kEventOnWorldChangeName);
  EventManager::Instance().RegisterEvent(kEventOnWorldEnterName);
  EventManager::Instance().RegisterEvent(kEventOnEquipName);
  EventManager::Instance().RegisterEvent(kEventOnDropItemName);
  EventManager::Instance().RegisterEvent(kEventOnTakeItemName);
  EventManager::Instance().RegisterEvent(kEventOnUseItemName);
  EventManager::Instance().RegisterEvent(kEventOnItemGroundCreateName);
  EventManager::Instance().RegisterEvent(kEventOnItemGroundDestroyName);
  EventManager::Instance().RegisterEvent(kEventOnItemsGroundDestroyName);
  EventManager::Instance().RegisterEvent(kEventOnUnequipName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCreateName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDestroyName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerMessageName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerRespawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSpawnName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerSummonName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerTeleportName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDeathName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerChangePingName);
  EventManager::Instance().RegisterEvent(kEventOnVoiceChatStateChangeName);
  EventManager::Instance().RegisterEvent(kEventOnVoiceTransmitStartName);
  EventManager::Instance().RegisterEvent(kEventOnVoiceTransmitStopName);
  EventManager::Instance().RegisterEvent(kEventOnVoiceChannelChangeName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerVoiceStartName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerVoiceStopName);
}

}  // namespace

/* luagmp (func)
 * Sends payload arguments to a remote-enabled server event. The server supplies context.senderPlayerId from the authenticated connection,
 * independently of payload and sourceId.
 * @name triggerServerEvent
 * @side client
 * @category Event
 * @param (string) name Remote-enabled server event name.
 * @param (table|nil) options Metadata {sourceId = positive integer}; pass nil for no metadata.
 * @param (any...) ... Serializable payload arguments, including nil.
 * @return (boolean) Whether the event was sent.
 */
auto Function_GothicEvents_TriggerServerEvent(const std::string& name, sol::object options, sol::variadic_args args, sol::this_state ts) {
  sol::state_view lua(ts);
  if (name.empty())
    return false;
  const auto source = gmp::lua::ReadEventSource(options);
  auto* client = NetGame::Instance().game_client.get();
  if (!client)
    return false;
  std::string encoded;
  std::string error;
  if (!gmp::lua::EncodeLuaArgs(lua, args, encoded, error)) {
    SPDLOG_ERROR("triggerServerEvent '{}': {}", name, error);
    return false;
  }
  return client->SendLuaEventToServer(name, source.value_or(0), encoded);
}

bool TriggerRemoteEvent(const std::string& event_name, std::uint32_t source_element, const std::vector<sol::object>& args) {
  ::gmp::lua::EventMetadata metadata;
  if (source_element != 0)
    metadata.source_element = source_element;
  return g_event_bindings.TriggerRemoteEvent(event_name, metadata, args);
}

void ResetGothicEvents() {
  EventManager::Instance().Reset();
  g_event_bindings.Reset();
  RegisterGothicEventsInManager();
}

void BindGothicEvents(sol::state& lua) {
  RegisterGothicEventProxies();
  RegisterGothicEventsInManager();
  ::gmp::lua::BindCommonEvents(lua, g_event_bindings);
  lua["triggerServerEvent"] = Function_GothicEvents_TriggerServerEvent;
}

}  // namespace gmp::gothic

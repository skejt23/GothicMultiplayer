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

#include "gothic_bindings.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>

#include "Interface.h"
#include "Mod.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "ZenGin/zGothicAPI.h"
#include "audio/lua_music.h"
#include "gmp_core.h"
#include "item_ground.h"
#include "language.h"
#include "lua_camera.h"
#include "lua_chat_input.h"
#include "lua_cursor.h"
#include "lua_discord.h"
#include "lua_gothic_music.h"
#include "lua_interface.h"
#include "lua_label.h"
#include "lua_route.h"
#include "lua_sky.h"
#include "lua_sound.h"
#include "lua_sound3d.h"
#include "lua_sprite.h"
#include "lua_vob.h"
#include "lua_voice.h"
#include "lua_world_label.h"
#include "net_game.h"
#include "npc_animation_utils.h"
#include "npc_weapon_utils.h"
#include "patch.h"
#include "process_input.h"
#include "script_instance_scope.h"
#include "weapon_skills.h"
#include "world_utils.hpp"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {
struct ClientNpc {
  oCNpc* npc{nullptr};
  std::string name;
  bool spawned{false};
  std::uint64_t instance_change_request{0};
};

std::unordered_map<int, ClientNpc> g_client_npcs;
int g_next_npc_id = -1;

auto FindClientNpc(std::int64_t id) {
  if (id >= 0 || id < std::numeric_limits<int>::min())
    return g_client_npcs.end();
  return g_client_npcs.find(static_cast<int>(id));
}

oCSpawnManager* GetSpawnManager() {
  return ogame ? ogame->GetSpawnManager() : nullptr;
}

bool HasFactoryAndParser() {
  // Gameplay symbols must remain available after native effects use cur_parser.
  return zfactory && Gothic_II_Addon::parser;
}

bool HasSpawnPrerequisites() {
  return HasFactoryAndParser() && ogame && GetSpawnManager();
}

std::optional<int> ResolveScriptInstance(const std::string& instance) {
  auto* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return std::nullopt;
  }

  zSTRING instance_name(instance.c_str());
  const int instance_id = parser->GetIndex(instance_name);
  if (instance_id <= 0) {
    return std::nullopt;
  }

  return instance_id;
}

Gothic2APlayer* GetPlayerById(std::uint64_t id) {
  auto& players = NetGame::Instance().players;
  auto it = std::find_if(players.begin(), players.end(), [id](Gothic2APlayer* player) { return player && player->base_player().id() == id; });

  if (it == players.end()) {
    return nullptr;
  }

  return *it;
}

Gothic2APlayer* GetPlayerByIdSigned(std::int64_t id) {
  if (id < 0) {
    return nullptr;
  }

  return GetPlayerById(static_cast<std::uint64_t>(id));
}

oCNpc* GetNpcById(std::int64_t id) {
  if (id < 0) {
    auto it = FindClientNpc(id);
    if (it != g_client_npcs.end()) {
      return it->second.npc;
    }
    return nullptr;
  }

  if (auto* player = GetPlayerById(static_cast<std::uint64_t>(id))) {
    return player->GetNpc();
  }

  return nullptr;
}

const PlayerPresenceEntry* GetOnlinePlayerById(std::int64_t id) {
  auto& net = NetGame::Instance();
  return id >= 0 && net.game_client && net.IsConnected() ? net.game_client->GetOnlinePlayer(static_cast<std::uint64_t>(id)) : nullptr;
}

enum class EquipmentSlot {
  Armor,
  MeleeWeapon,
  RangedWeapon,
  Helmet,
  Shield,
};

std::optional<std::string> ItemInstanceNameByIndex(int index) {
  if (index <= 0) {
    return std::nullopt;
  }

  auto* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return std::nullopt;
  }

  auto* symbol = parser->GetSymbol(index);
  if (!symbol || symbol->name.IsEmpty()) {
    return std::nullopt;
  }

  return std::string(symbol->name.ToChar());
}

std::optional<std::string> ItemInstanceName(oCItem* item) {
  if (!item) {
    return std::nullopt;
  }

  auto instance = ItemInstanceNameByIndex(item->GetInstance());
  if (instance.has_value()) {
    return instance;
  }

  zSTRING instance_name = item->GetInstanceName();
  if (instance_name.IsEmpty()) {
    return std::nullopt;
  }

  return std::string(instance_name.ToChar());
}

oCItem* GetNpcSlotItem(oCNpc* npc, const char* slot_name) {
  if (!npc) {
    return nullptr;
  }

  zSTRING slot(slot_name);
  return npc->GetSlotItem(slot);
}

bool IsValidEquipmentSlotItem(oCItem* item, EquipmentSlot slot) {
  switch (slot) {
    case EquipmentSlot::Armor:
      return item && (item->wear & ITM_WEAR_TORSO) != 0;
    case EquipmentSlot::MeleeWeapon:
      return item && (item->HasFlag(ITM_FLAG_SWD) || item->HasFlag(ITM_FLAG_DAG) || item->HasFlag(ITM_FLAG_AXE) || item->HasFlag(ITM_FLAG_2HD_SWD) ||
                      item->HasFlag(ITM_FLAG_2HD_AXE));
    case EquipmentSlot::RangedWeapon:
      return item && (item->HasFlag(ITM_FLAG_BOW) || item->HasFlag(ITM_FLAG_CROSSBOW));
    case EquipmentSlot::Helmet:
      return item && (item->wear & ITM_WEAR_HEAD) != 0;
    case EquipmentSlot::Shield:
      return item && item->HasFlag(ITM_FLAG_SHIELD);
  }

  return false;
}

oCItem* GetCurrentMeleeWeapon(oCNpc* npc) {
  if (!npc) {
    return nullptr;
  }

  switch (npc->GetWeaponMode()) {
    case NPC_WEAPON_1HS:
    case NPC_WEAPON_2HS:
      return dynamic_cast<oCItem*>(npc->GetRightHand());
    default:
      return npc->GetEquippedMeleeWeapon();
  }
}

oCItem* GetCurrentRangedWeapon(oCNpc* npc) {
  if (!npc) {
    return nullptr;
  }

  switch (npc->GetWeaponMode()) {
    case NPC_WEAPON_BOW:
      return dynamic_cast<oCItem*>(npc->GetLeftHand());
    case NPC_WEAPON_CBOW:
      return dynamic_cast<oCItem*>(npc->GetRightHand());
    default:
      return npc->GetEquippedRangedWeapon();
  }
}

oCItem* GetEquippedSlotItem(oCNpc* npc, EquipmentSlot slot) {
  if (!npc) {
    return nullptr;
  }

  switch (slot) {
    case EquipmentSlot::Armor:
      return npc->GetEquippedArmor();
    case EquipmentSlot::MeleeWeapon:
      return GetCurrentMeleeWeapon(npc);
    case EquipmentSlot::RangedWeapon:
      return GetCurrentRangedWeapon(npc);
    case EquipmentSlot::Helmet:
      return GetNpcSlotItem(npc, NPC_NODE_HELMET);
    case EquipmentSlot::Shield:
      return GetNpcSlotItem(npc, NPC_NODE_SHIELD);
  }

  return nullptr;
}

std::optional<std::string> CachedEquipmentSlotInstance(Gothic2APlayer* player, EquipmentSlot slot) {
  if (!player) {
    return std::nullopt;
  }

  auto& base = player->base_player();
  switch (slot) {
    case EquipmentSlot::Armor:
      return base.equipped_armor().empty() ? std::nullopt : std::optional<std::string>{base.equipped_armor()};
    case EquipmentSlot::MeleeWeapon:
      return base.melee_weapon().empty() ? std::nullopt : std::optional<std::string>{base.melee_weapon()};
    case EquipmentSlot::RangedWeapon:
      return base.ranged_weapon().empty() ? std::nullopt : std::optional<std::string>{base.ranged_weapon()};
    case EquipmentSlot::Helmet:
      return base.equipped_helmet().empty() ? std::nullopt : std::optional<std::string>{base.equipped_helmet()};
    case EquipmentSlot::Shield:
      return base.equipped_shield().empty() ? std::nullopt : std::optional<std::string>{base.equipped_shield()};
  }

  return std::nullopt;
}

std::optional<std::string> GetEquipmentSlotInstance(std::int64_t id, EquipmentSlot slot) {
  if (auto* player = GetPlayerByIdSigned(id)) {
    if (auto* npc = player->GetNpc()) {
      return ItemInstanceName(GetEquippedSlotItem(npc, slot));
    }

    return CachedEquipmentSlotInstance(player, slot);
  }

  return ItemInstanceName(GetEquippedSlotItem(GetNpcById(id), slot));
}

struct InventoryItemLookup {
  oCItem* item{};
  bool created{};
};

InventoryItemLookup FindInventoryItem(oCNpc* npc, const std::string& instance, bool create_one) {
  if (!npc) {
    return {};
  }

  auto instance_id = ResolveScriptInstance(instance);
  if (!instance_id) {
    return {};
  }

  if (auto* item = npc->inventory2.IsIn(*instance_id, 1)) {
    return {item, false};
  }

  if (create_one) {
    oCItem* item = zfactory ? zfactory->CreateItem(*instance_id) : nullptr;
    if (!item) {
      return {};
    }

    item->amount = 1;
    npc->inventory2.Insert(item);
    return {item, true};
  }

  return {};
}

void RemoveItemFromHandSlots(oCNpc* npc, oCItem* item) {
  if (!npc || !item) {
    return;
  }

  if (npc->GetRightHand() == item) {
    zSTRING slot(NPC_NODE_RIGHTHAND);
    npc->RemoveFromSlot(slot, 0, 1);
  }

  if (npc->GetLeftHand() == item) {
    zSTRING slot(NPC_NODE_LEFTHAND);
    npc->RemoveFromSlot(slot, 0, 1);
  }
}

bool UnequipSlot(oCNpc* npc, EquipmentSlot slot) {
  if (!npc) {
    return false;
  }

  oCItem* item = GetEquippedSlotItem(npc, slot);
  if (!item) {
    return false;
  }

  npc->UnequipItem(item);
  RemoveItemFromHandSlots(npc, item);
  return GetEquippedSlotItem(npc, slot) != item;
}

bool EquipInventoryInstance(oCNpc* npc, const std::string& instance) {
  if (instance == "NULL") {
    return false;
  }

  auto lookup = FindInventoryItem(npc, instance, true);
  if (!lookup.item) {
    return false;
  }

  if (lookup.item->HasFlag(ITM_FLAG_ACTIVE)) {
    return true;
  }

  npc->Equip(lookup.item);
  return lookup.item->HasFlag(ITM_FLAG_ACTIVE);
}

bool EquipSlotInstance(oCNpc* npc, EquipmentSlot slot, const std::string& instance) {
  if (!npc) {
    return false;
  }

  if (instance == "NULL") {
    return UnequipSlot(npc, slot);
  }

  auto lookup = FindInventoryItem(npc, instance, true);
  if (!lookup.item) {
    return false;
  }

  if (!IsValidEquipmentSlotItem(lookup.item, slot)) {
    if (lookup.created) {
      const int amount = std::max(1, lookup.item->amount);
      oCItem* removed = npc->RemoveFromInv(lookup.item, amount);
      if (!removed) {
        removed = lookup.item;
      }
      removed->RemoveVobFromWorld();
    }
    return false;
  }

  if (GetEquippedSlotItem(npc, slot) == lookup.item) {
    return true;
  }

  switch (slot) {
    case EquipmentSlot::MeleeWeapon:
      switch (npc->GetWeaponMode()) {
        case NPC_WEAPON_1HS:
        case NPC_WEAPON_2HS:
          npc->SetRightHand(lookup.item);
          return GetEquippedSlotItem(npc, slot) == lookup.item;
        default:
          break;
      }
      break;
    case EquipmentSlot::RangedWeapon:
      switch (npc->GetWeaponMode()) {
        case NPC_WEAPON_BOW:
          npc->SetLeftHand(lookup.item);
          return GetEquippedSlotItem(npc, slot) == lookup.item;
        case NPC_WEAPON_CBOW:
          npc->SetRightHand(lookup.item);
          return GetEquippedSlotItem(npc, slot) == lookup.item;
        default:
          break;
      }
      break;
    default:
      break;
  }

  if (auto* current = GetEquippedSlotItem(npc, slot)) {
    npc->UnequipItem(current);
    RemoveItemFromHandSlots(npc, current);
  }

  npc->Equip(lookup.item);
  return GetEquippedSlotItem(npc, slot) == lookup.item;
}

bool UnequipSlot(std::int64_t id, EquipmentSlot slot) {
  return UnequipSlot(GetNpcById(id), slot);
}

struct VideoModeSelection {
  int index{};
  zTRnd_VidModeInfo info{};
};

std::optional<VideoModeSelection> FindVideoMode(int width, int height, std::optional<int> bpp) {
  if (!zrenderer || width <= 0 || height <= 0) {
    return std::nullopt;
  }

  const int mode_count = zrenderer->Vid_GetNumModes();
  for (int mode_index = 0; mode_index < mode_count; ++mode_index) {
    zTRnd_VidModeInfo mode_info;
    if (!zrenderer->Vid_GetModeInfo(mode_info, mode_index)) {
      continue;
    }

    if (mode_info.xdim == width && mode_info.ydim == height && (!bpp.has_value() || mode_info.bpp == *bpp)) {
      return VideoModeSelection{mode_index, mode_info};
    }
  }

  return std::nullopt;
}

zCSkyControler* GetActiveSkyController() {
  if (!ogame || !ogame->GetGameWorld()) {
    return nullptr;
  }

  return ogame->GetGameWorld()->GetActiveSkyControler();
}

bool ReplaceStandaloneNpcInstance(ClientNpc& entry, int instance_id) {
  if (!entry.npc || !zfactory || instance_id <= 0) {
    return false;
  }

  oCNpc* old_npc = entry.npc;
  const bool was_spawned = old_npc->GetHomeWorld() != nullptr;
  if (was_spawned && (!ogame || !ogame->GetGameWorld())) {
    return false;
  }

  oCNpc* new_npc = nullptr;
  {
    auto* parser = Gothic_II_Addon::parser;
    if (!IsNpcInstance(parser, instance_id))
      return false;
    ScriptInstanceScope scope(*parser, instance_id);
    new_npc = zfactory->CreateNpc(instance_id);
  }
  if (!new_npc) {
    return false;
  }
  new_npc->startAIState = 0;
  new_npc->state.ClearAIState();

  ResetWorldRenderState();
  old_npc->ClearEM();
  old_npc->SetInteractItem(nullptr);
  ClearNpcFocusReferences(old_npc);

  zSTRING name = old_npc->GetName();
  new_npc->name[0] = name;

  if (was_spawned) {
    zVEC3 position = old_npc->GetPositionWorld();
    zVEC3 heading = old_npc->GetAtVectorWorld();
    entry.npc = new_npc;
    new_npc->Enable(position);
    new_npc->SetHeadingYWorld(position + heading);
    new_npc->SetPositionWorld(position);
    ogame->GetGameWorld()->RemoveVob(old_npc);
  } else {
    entry.npc = new_npc;
  }
  old_npc->Release();  // Release the ClientNpc entry's factory reference.

  entry.spawned = new_npc->GetHomeWorld() != nullptr;
  ResetWorldRenderState();
  return true;
}

oCMenu_Status* GetStatusMenu() {
  zSTRING status_menu_name("MENU_STATUS");
  if (auto* menu = dynamic_cast<oCMenu_Status*>(zCMenu::GetByName(status_menu_name))) {
    return menu;
  }

  zSTRING fallback_name("STATUS");
  return dynamic_cast<oCMenu_Status*>(zCMenu::GetByName(fallback_name));
}
}  // namespace

/* luagmp (global)
 *
 * Represents the client player id.
 *
 * Use it, to manipulate the hero (local player) with player functions, e.g: [setPlayerPosition](../client-functions/player/setPlayerPosition.md).
 *
 * @side		client
 * @name		heroId
 * @return (number)
 *
 */

/* luagmp (func)
 *
 * This function will change the player/NPC in-game instance.
 *
 * @version  0.3.0
 * @name     setPlayerInstance
 * @side     client
 * @category Player
 * @param    (number) player_id      Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance       Instance name.
 * @return   (boolean)               True if the change was queued for the next frame, false otherwise.
 *
 */
bool Function_SetPlayerInstance(std::int64_t id, const std::string& instance) {
  const auto instance_id = ResolveScriptInstance(instance);
  if (!instance_id.has_value() || !IsNpcInstance(Gothic_II_Addon::parser, *instance_id)) {
    return false;
  }

  if (auto* player = GetPlayerByIdSigned(id)) {
    if (!player->GetNpc())
      return false;
    NetGame::Instance().ChangePlayerInstance(player->base_player().id(), instance, true);
    return true;
  }

  if (id < 0) {
    auto it = FindClientNpc(id);
    if (it == g_client_npcs.end()) {
      return false;
    }
    auto* previous = it->second.npc;
    const auto request = ++it->second.instance_change_request;
    auto* world = ogame ? ogame->GetGameWorld() : nullptr;
    GMPCore::Instance().DeferToNextFrame([id, previous, request, world, index = *instance_id] {
      const auto current = FindClientNpc(id);
      if (current != g_client_npcs.end() && current->second.npc == previous && current->second.instance_change_request == request && ogame &&
          ogame->GetGameWorld() == world)
        ReplaceStandaloneNpcInstance(current->second, index);
    });
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current instance, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerInstance
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)          Instance name or nil.
 *
 */
sol::object Function_GetPlayerInstance(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    std::string name = npc->GetInstanceName().ToChar();
    return sol::make_object(lua, std::move(name));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will change the player/NPC name.
 *
 * @version  0.3.0
 * @name     setPlayerName
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) name       New player name.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerName(std::int64_t id, const std::string& name) {
  if (auto* player = GetPlayerByIdSigned(id)) {
    zSTRING new_name(name.c_str());

    for (auto* other_player : NetGame::Instance().players) {
      if (!other_player || !other_player->GetNpc()) {
        continue;
      }

      if (other_player->base_player().id() != static_cast<std::uint64_t>(id) && other_player->GetNpc()->GetName() == new_name) {
        return false;
      }
    }

    player->SetName(new_name);
    player->base_player().set_name(new_name.ToChar());
    return true;
  }

  if (auto* npc = GetNpcById(id)) {
    npc->name[0] = name.c_str();
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current name, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerName
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Player name or nil.
 *
 */
sol::object Function_GetPlayerName(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    std::string name = npc->GetName().ToChar();
    return sol::make_object(lua, std::move(name));
  }

  if (const auto* online = GetOnlinePlayerById(id))
    return sol::make_object(lua, online->name);

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will change the player/NPC name color.
 *
 * @version  0.3.0
 * @name     setPlayerNameColor
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or server NPC id; client-local NPC ids are not supported.
 * @param    (number) r          The red color component in RGB model.
 * @param    (number) g          The green color component in RGB model.
 * @param    (number) b          The blue color component in RGB model.
 * @return   (boolean)           True on success.
 *
 * @note Accepts player and server NPC ids; negative client-local NPC ids are not supported.
 */
bool Function_SetPlayerNameColor(std::int64_t id, int r, int g, int b) {
  if (auto* player = GetPlayerByIdSigned(id)) {
    player->SetNameColor(zCOLOR(::lua::bind_helpers::ClampByte(r), ::lua::bind_helpers::ClampByte(g), ::lua::bind_helpers::ClampByte(b), 255));
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current name color, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerNameColor
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or server NPC id; client-local NPC ids are not supported.
 * @return   ({r, g, b}|nil)     Table containing color in RGB model or nil.
 *
 * @note Accepts player and server NPC ids; negative client-local NPC ids are not supported.
 */
sol::object Function_GetPlayerNameColor(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* player = GetPlayerByIdSigned(id)) {
    const auto& color = player->GetNameColor();

    sol::table tbl = lua.create_table();
    tbl["r"] = static_cast<int>(color.r);
    tbl["g"] = static_cast<int>(color.g);
    tbl["b"] = static_cast<int>(color.b);

    return sol::make_object(lua, tbl);
  }

  if (const auto* online = GetOnlinePlayerById(id)) {
    return sol::make_object(
        lua, lua.create_table_with("r", static_cast<int>(online->r), "g", static_cast<int>(online->g), "b", static_cast<int>(online->b)));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC health, not exceeding his current max health.
 *
 * @version  0.3.0
 * @name     setPlayerHealth
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) health     New health value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerHealth(std::int64_t id, int health) {
  if (health < 0) {
    health = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    const int max_health = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
    const int clamped_health = std::min(health, max_health);
    npc->SetAttribute(NPC_ATR_HITPOINTS, clamped_health);

    if (auto* player = GetPlayerByIdSigned(id)) {
      player->base_player().set_health(clamped_health);
    }

    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current health, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerHealth
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Current health or nil.
 *
 */
sol::object Function_GetPlayerHealth(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_HITPOINTS));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC maximum health. If the current health exceeds the new maximum, it will be clamped down to the new value.
 *
 * @version  0.3.0
 * @name     setPlayerMaxHealth
 * @side     client
 * @category Player
 * @param    (number) player_id   Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) max_health  New maximum health.
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerMaxHealth(std::int64_t id, int max_health) {
  if (max_health < 0) {
    max_health = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    npc->SetAttribute(NPC_ATR_HITPOINTSMAX, max_health);
    const int current_health = npc->GetAttribute(NPC_ATR_HITPOINTS);
    if (current_health > max_health) {
      npc->SetAttribute(NPC_ATR_HITPOINTS, max_health);
    }
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current maximum health, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMaxHealth
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Max health or nil.
 *
 */
sol::object Function_GetPlayerMaxHealth(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_HITPOINTSMAX));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC mana, not exceeding his current max mana.
 *
 * @version  0.3.0
 * @name     setPlayerMana
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) mana       Mana value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerMana(std::int64_t id, int mana) {
  if (mana < 0) {
    mana = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    const int max_mana = npc->GetAttribute(NPC_ATR_MANAMAX);
    const int clamped_mana = std::min(mana, max_mana);
    npc->SetAttribute(NPC_ATR_MANA, clamped_mana);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current mana, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMana
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Current mana or nil.
 *
 */
sol::object Function_GetPlayerMana(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_MANA));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC maximum mana. If the current mana exceeds the new maximum, it will be clamped down to the new value.
 *
 * @version  0.3.0
 * @name     setPlayerMaxMana
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) max_mana   New maximum mana.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerMaxMana(std::int64_t id, int max_mana) {
  if (max_mana < 0) {
    max_mana = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    npc->SetAttribute(NPC_ATR_MANAMAX, max_mana);
    const int current_mana = npc->GetAttribute(NPC_ATR_MANA);
    if (current_mana > max_mana) {
      npc->SetAttribute(NPC_ATR_MANA, max_mana);
    }
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current max mana, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerMaxMana
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Max mana or nil.
 *
 */
sol::object Function_GetPlayerMaxMana(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_MANAMAX));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC strength attribute.
 *
 * @version  0.3.0
 * @name     setPlayerStrength
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) strength   Strength value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerStrength(std::int64_t id, int strength) {
  if (strength < 0) {
    strength = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    npc->SetAttribute(NPC_ATR_STRENGTH, strength);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC strength attribute, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerStrength
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Strength value or nil.
 *
 */
sol::object Function_GetPlayerStrength(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_STRENGTH));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC dexterity attribute.
 *
 * @version  0.3.0
 * @name     setPlayerDexterity
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) dexterity  Dexterity value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerDexterity(std::int64_t id, int dexterity) {
  if (dexterity < 0) {
    dexterity = 0;
  }

  if (auto* npc = GetNpcById(id)) {
    npc->SetAttribute(NPC_ATR_DEXTERITY, dexterity);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC dexterity attribute, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerDexterity
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Dexterity value or nil.
 *
 */
sol::object Function_GetPlayerDexterity(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_DEXTERITY));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC weapon skill hit chance.
 *
 * @version  0.3.0
 * @name     setPlayerSkillWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) skill_id   Skill identifier. For more information, see [Weapon Constants](../../shared-constants/Weapon.md).
 * @param    (number) percentage Hit chance (0-100).
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerSkillWeapon(std::int64_t id, int skill_id, int percentage) {
  percentage = std::clamp(percentage, 0, 100);

  if (auto* npc = GetNpcById(id)) {
    if (!ApplyNpcWeaponSkill(npc, skill_id, percentage))
      return false;
    if (auto* actor = GetPlayerByIdSigned(id)) {
      actor->base_player().set_weapon_skill(skill_id, percentage);
      actor->base_player().set_talent(skill_id, gmp::WeaponSkillTier(percentage));
    }
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC weapon skill hit chance, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerSkillWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) skill_id   Skill identifier. For more information, see [Weapon Constants](../../shared-constants/Weapon.md).
 * @return   (number|nil)        Hit chance (0-100) or nil.
 *
 */
sol::object Function_GetPlayerSkillWeapon(std::int64_t id, int skill_id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetHitChance(skill_id));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC talent value.
 *
 * @version  0.3.0
 * @name     setPlayerTalent
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) talent_id    Talent identifier. For more information, see [Talent Constants](../../shared-constants/Talent.md).
 * @param    (number) talent_value Talent value.
 * @return   (boolean)             True on success.
 *
 */
bool Function_SetPlayerTalent(std::int64_t id, int talent_id, int talent_value) {
  if (talent_id < 1 || talent_id >= oCNpcTalent::NPC_TAL_MAX)
    return false;
  talent_value = gmp::IsWeaponSkill(talent_id) ? std::clamp(talent_value, 0, 2) : std::max(0, talent_value);
  if (auto* npc = GetNpcById(id)) {
    ApplyNpcTalent(npc, talent_id, talent_value);
    if (auto* actor = GetPlayerByIdSigned(id))
      actor->base_player().set_talent(talent_id, talent_value);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC talent value, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerTalent
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) talent_id  Talent identifier. For more information, see [Talent Constants](../../shared-constants/Talent.md).
 * @return   (number|nil)        Talent value or nil.
 *
 */
sol::object Function_GetPlayerTalent(std::int64_t id, int talent_id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetTalentSkill(talent_id));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC level.
 *
 * @version  0.3.0
 * @name     setPlayerLevel
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) level      New level.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerLevel(std::int64_t id, int level) {
  if (auto* npc = GetNpcById(id)) {
    npc->level = static_cast<int>(std::max(level, 0));
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return player/NPC current level, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerLevel
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Level or nil.
 *
 */
sol::object Function_GetPlayerLevel(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, static_cast<int>(npc->level));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the hero's experience points.
 *
 * @version  0.3.0
 * @name     setExp
 * @side     client
 * @category Hero
 * @param    (number) exp       New exp value.
 * @return   (boolean)          True on success.
 *
 */
bool Function_SetExp(int exp) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_exp = static_cast<unsigned long>(std::max(exp, 0));
      npc->experience_points = clamped_exp;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetExperience(clamped_exp, 0, npc->experience_points_next_level);
      }
      return true;
    }
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the hero's current experience point amount, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getExp
 * @side     client
 * @category Hero
 * @return   (number|nil)        Exp value or nil.
 *
 */
sol::object Function_GetExp(sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      return sol::make_object(lua, static_cast<int>(npc->experience_points));
    }
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the experience required for the hero's next level.
 *
 * @version  0.3.0
 * @name     setNextLevelExp
 * @side     client
 * @category Hero
 * @param    (number) next_level_exp   Required exp for next level.
 * @return   (boolean)                 True on success.
 *
 */
bool Function_SetNextLevelExp(int next_level_exp) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_next_level = static_cast<unsigned long>(std::max(next_level_exp, 0));
      npc->experience_points_next_level = clamped_next_level;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetExperience(npc->experience_points, 0, clamped_next_level);
      }
      return true;
    }
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the current experience required for the hero's next level, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getNextLevelExp
 * @side     client
 * @category Hero
 * @return   (number|nil)        Next level exp or nil.
 *
 */
sol::object Function_GetNextLevelExp(sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      return sol::make_object(lua, static_cast<int>(npc->experience_points_next_level));
    }
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the hero's learn points.
 *
 * @version  0.3.0
 * @name     setLearnPoints
 * @side     client
 * @category Hero
 * @param    (number) learn_points   New learn points value.
 * @return   (boolean)               True on success.
 *
 */
bool Function_SetLearnPoints(int learn_points) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_points = static_cast<unsigned long>(std::max(learn_points, 0));
      npc->learn_points = clamped_points;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetLearnPoints(clamped_points);
      }
      return true;
    }
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the hero's current learn points, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getLearnPoints
 * @side     client
 * @category Hero
 * @return   (number|nil)        Learn points or nil.
 *
 */
sol::object Function_GetLearnPoints(sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      return sol::make_object(lua, static_cast<int>(npc->learn_points));
    }
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC visual model and textures.
 *
 * @version  0.3.0
 * @name     setPlayerVisual
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) bodyModel   Body model name.
 * @param    (number) bodyTexture    Body texture index.
 * @param    (string) headModel   Head model name.
 * @param    (number) headTexture    Head texture index.
 * @param    (number|nil) teethTexture  Optional teeth texture file numeric id. Defaults to 0 if omitted.
 * @param    (number|nil) skinColor     Optional color variant of head & body texture files. Defaults to 0 if omitted.
 * @return   (boolean)               True on success.
 *
 */
bool Function_SetPlayerVisual(std::int64_t id, const std::string& body_model, int body_texture, const std::string& head_model, int head_texture,
                              sol::optional<int> teeth_texture, sol::optional<int> skin_color) {
  if (auto* npc = GetNpcById(id); npc) {
    zSTRING body(body_model.c_str());
    zSTRING head(head_model.c_str());
    const int color_variant = skin_color.value_or(0);
    const int teeth_variant = teeth_texture.value_or(0);
    npc->SetAdditionalVisuals(body, body_texture, color_variant, head, head_texture, teeth_variant, -1);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current visual model and textures, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerVisual
 * @side     client
 * @category Player
 * @param    (number) player_id                                           Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   ({bodyModel, bodyTexture, headModel, headTexture, teethTexture, skinColor}|nil)   Player visual or nil.
 *
 */
sol::object Function_GetPlayerVisual(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id); npc) {
    sol::table tbl = lua.create_table();

    tbl["bodyModel"] = std::string(npc->body_visualName.ToChar());
    tbl["bodyTexture"] = static_cast<int>(npc->body_TexVarNr);
    tbl["headModel"] = std::string(npc->head_visualName.ToChar());
    tbl["headTexture"] = static_cast<int>(npc->head_TexVarNr);
    tbl["teethTexture"] = static_cast<int>(npc->teeth_TexVarNr);
    tbl["skinColor"] = static_cast<int>(npc->body_TexColorNr);
    return sol::make_object(lua, tbl);
  }
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC model fatness.
 *
 * @version  0.3.0
 * @name     setPlayerFatness
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) fatness    Fatness value.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerFatness(std::int64_t id, float fatness) {
  if (auto* npc = GetNpcById(id); npc) {
    npc->SetFatness(fatness);
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current model fatness, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerFatness
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)          Player fatness or nil.
 *
 */
sol::object Function_GetPlayerFatness(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->model_fatness);
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC model scale.
 *
 * @version  0.3.0
 * @name     setPlayerScale
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) x            Scale factor on x axis.
 * @param    (number) y            Scale factor on y axis.
 * @param    (number) z            Scale factor on z axis.
 * @return   (boolean)             True on success.
 *
 */
bool Function_SetPlayerScale(std::int64_t id, float x, float y, float z) {
  if (auto* npc = GetNpcById(id); npc) {
    npc->SetModelScale(zVEC3{x, y, z});
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current model scale, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerScale
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   ({x, y, z}|nil)       Player scale or nil.
 *
 */
sol::object Function_GetPlayerScale(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id); npc) {
    const zVEC3 scale = npc->model_scale;
    return sol::make_object(lua, ::lua::bind_helpers::MakeVec3Table(lua, scale[VX], scale[VY], scale[VZ]));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC weapon mode for all players.
 *
 * @version  0.3.0
 * @name     setPlayerWeaponMode
 * @side     client
 * @category Player
 * @param    (number) player_id   Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) weapon_mode Weapon mode constant. For more information, see [Weapon Mode Constants](../../shared-constants/WeaponMode.md).
 * @return   (boolean)            True on success.
 *
 */
bool Function_SetPlayerWeaponMode(std::int64_t id, int weapon_mode) {
  if (weapon_mode < NPC_WEAPON_NONE || weapon_mode > NPC_WEAPON_MAG)
    return false;
  if (auto* npc = GetNpcById(id); npc) {
    return SetNpcWeaponMode(npc, weapon_mode);
  }

  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current weapon mode, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerWeaponMode
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Weapon mode from [Weapon Mode Constants](../../shared-constants/WeaponMode.md), or nil.
 *
 */
sol::object Function_GetPlayerWeaponMode(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id); npc) {
    return sol::make_object(lua, npc->GetWeaponMode());
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will return the active animation name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerAni
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Active animation name, or nil if unavailable.
 *
 */
sol::object Function_GetPlayerAni(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    if (auto* model = npc->GetModel(); model && model->numActiveAnis > 0) {
      if (auto* active = model->GetActiveAni(0); active && active->protoAni) {
        return sol::make_object(lua, std::string(active->protoAni->GetAniName().ToChar()));
      }
    }
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will return the active animation id for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerAniId
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number)            Active animation id, or -1 if unavailable.
 *
 */
std::int32_t Function_GetPlayerAniId(std::int64_t id) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* model = npc->GetModel(); model && model->numActiveAnis > 0) {
      if (auto* active = model->GetActiveAni(0); active && active->protoAni) {
        return active->protoAni->GetAniID();
      }
    }
  }

  return -1;
}

/* luagmp (func)
 *
 * This function will return the equipped armor instance name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerArmor
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Equipped armor instance, or nil if no armor is equipped.
 *
 */
sol::object Function_GetPlayerArmor(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  auto instance = GetEquipmentSlotInstance(id, EquipmentSlot::Armor);
  return instance.has_value() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::lua_nil);
}

/* luagmp (func)
 *
 * This function will return the equipped melee weapon instance name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerMeleeWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Equipped melee weapon instance, or nil if none is equipped.
 *
 */
sol::object Function_GetPlayerMeleeWeapon(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  auto instance = GetEquipmentSlotInstance(id, EquipmentSlot::MeleeWeapon);
  return instance.has_value() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::lua_nil);
}

/* luagmp (func)
 *
 * This function will return the equipped ranged weapon instance name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerRangedWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Equipped ranged weapon instance, or nil if none is equipped.
 *
 */
sol::object Function_GetPlayerRangedWeapon(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  auto instance = GetEquipmentSlotInstance(id, EquipmentSlot::RangedWeapon);
  return instance.has_value() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::lua_nil);
}

/* luagmp (func)
 *
 * This function will return the equipped helmet instance name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerHelmet
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Equipped helmet instance, or nil if no helmet is equipped.
 *
 */
sol::object Function_GetPlayerHelmet(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  auto instance = GetEquipmentSlotInstance(id, EquipmentSlot::Helmet);
  return instance.has_value() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::lua_nil);
}

/* luagmp (func)
 *
 * This function will return the equipped shield instance name for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerShield
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (string|nil)        Equipped shield instance, or nil if no shield is equipped.
 *
 */
sol::object Function_GetPlayerShield(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  auto instance = GetEquipmentSlotInstance(id, EquipmentSlot::Shield);
  return instance.has_value() ? sol::make_object(lua, *instance) : sol::make_object(lua, sol::lua_nil);
}

/* luagmp (func)
 *
 * This function will apply an animation overlay on player (eg. "HUMANS_MILITIA.MDS").
 *
 * @version  0.3.0
 * @name     applyPlayerOverlay
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) overlay      The name of overlay.
 * @return   (boolean)             True on success.
 *
 */
bool Function_ApplyPlayerOverlay(std::int64_t id, const std::string& overlay) {
  if (auto* npc = GetNpcById(id); npc) {
    zSTRING overlay_name(overlay.c_str());
    return npc->ApplyOverlay(overlay_name) != 0;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC active animation overlays, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerOverlays
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   ({...}|nil)           Array of overlay names or nil.
 *
 */
sol::object Function_GetPlayerOverlays(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zCArray<zSTRING>& overlays = npc->activeOverlays;

    if (overlays.GetNum() == 0) {
      return sol::nil;  // or return empty table if you prefer
    }

    sol::table tbl = lua.create_table(overlays.GetNum(), 0);

    for (int i = 0; i < overlays.GetNum(); ++i) {
      tbl[i + 1] = overlays[i].ToChar();
    }

    return tbl;
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will remove specified animation overlay from the player.
 *
 * @version  0.3.0
 * @name     removePlayerOverlay
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) overlay      The name of overlay.
 * @return   (boolean)             True on success.
 *
 */
bool Function_RemovePlayerOverlay(std::int64_t id, const std::string& overlay) {
  if (auto* npc = GetNpcById(id); npc) {
    zSTRING overlay_name(overlay.c_str());
    overlay_name.Upper();
    const bool has_overlay = npc->activeOverlays.IsInList(overlay_name) != 0;
    if (has_overlay) {
      npc->RemoveOverlay(overlay_name);
    }
    return has_overlay;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will play an animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     playAni
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) aniName    Animation name (e.g. "T_STAND_2_SIT").
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayAni(std::int64_t id, const std::string& ani_name) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* model = npc->GetModel()) {
      model->StartAnimation(zSTRING(ani_name.c_str()));
      return true;
    }
  }

  return false;
}

/* luagmp (func)
 *
 * This function will stop a played animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     stopAni
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string|nil) aniName  Animation name to stop. Defaults to "" for first active animation.
 * @return   (boolean)             True on success.
 *
 */
bool Function_StopAni(std::int64_t id, sol::optional<std::string> ani_name) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* model = npc->GetModel()) {
      const std::string name = ani_name.value_or("");
      if (!name.empty()) {
        model->StopAnimation(zSTRING(name.c_str()));
        return true;
      }
      if (model->numActiveAnis > 0) {
        if (auto* active = model->GetActiveAni(0)) {
          model->StopAni(active);
          return true;
        }
      }
    }
  }

  return false;
}

/* luagmp (func)
 *
 * This function will play a face animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     playFaceAni
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) aniName    Face animation name (e.g. "S_FRIENDLY").
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayFaceAni(std::int64_t id, const std::string& ani_name) {
  if (auto* npc = GetNpcById(id)) {
    zSTRING face_name(ani_name.c_str());
    npc->StartFaceAni(face_name, 1.0f, 1.0f);
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will stop a played face animation on the player/NPC.
 *
 * @version  0.3.0
 * @name     stopFaceAni
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string|nil) aniName  Face animation name to stop. Defaults to "" for first active animation.
 * @return   (boolean)             True on success.
 *
 */
bool Function_StopFaceAni(std::int64_t id, sol::optional<std::string> ani_name) {
  if (auto* npc = GetNpcById(id)) {
    zSTRING face_name(ani_name.value_or("").c_str());
    npc->StopFaceAni(face_name);
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will play a gesticulation animation on the player/NPC.
 * Calls for the same id are throttled to once every 3500 ms.
 *
 * @version  0.3.0
 * @name     playGesticulation
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player id.
 * @return   (boolean)           True on success.
 *
 */
bool Function_PlayGesticulation(std::int64_t id) {
  static std::unordered_map<std::int64_t, std::chrono::steady_clock::time_point> next_allowed_by_player;

  if (auto* npc = GetNpcById(id); npc && !npc->IsDead() && !npc->IsUnconscious()) {
    const auto now = std::chrono::steady_clock::now();
    auto& next_allowed = next_allowed_by_player[id];
    if (now < next_allowed) {
      return false;
    }

    npc->StartDialogAni();
    next_allowed = now + std::chrono::milliseconds(3500);
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will set the player/NPC world position.
 *
 * @version  0.3.0
 * @name     setPlayerPosition
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) x          X coordinate.
 * @param    (number) y          Y coordinate.
 * @param    (number) z          Z coordinate.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerPosition(std::int64_t id, float x, float y, float z) {
  if (auto* npc = GetNpcById(id)) {
    zVEC3 position{x, y, z};
    npc->SetPositionWorld(position);

    if (auto* player = GetPlayerByIdSigned(id)) {
      player->SetPosition(position);
      player->base_player().set_position(x, y, z);
    }

    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC current world position.
 *
 * @version  0.3.0
 * @name     getPlayerPosition
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   ({x,y,z}|nil)       Table with keys `x`,`y`,`z` or nil.
 *
 */
sol::object Function_GetPlayerPosition(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zVEC3 position = npc->GetPositionWorld();
    return sol::make_object(lua, ::lua::bind_helpers::MakeVec3Table(lua, position[VX], position[VY], position[VZ]));
  }

  if (const auto* online = GetOnlinePlayerById(id)) {
    const auto& p = online->position;
    return sol::make_object(lua, ::lua::bind_helpers::MakeVec3Table(lua, p.x, p.y, p.z));
  }

  return sol::nil;
}

/* luagmp (func)
 * Returns the online player's world, independently of native streaming.
 * @version  0.3.0
 * @name     getPlayerWorld
 * @side     client
 * @category Player
 * @param    (number) player_id Target player id; NPC ids are not supported.
 * @return   (string|nil) World filename or nil.
 */
sol::object Function_GetPlayerWorld(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  if (const auto* online = GetOnlinePlayerById(id))
    return sol::make_object(lua, online->world);
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the player/NPC facing angle in world space.
 *
 * @version  0.3.0
 * @name     setPlayerAngle
 * @side     client
 * @category Player
 * @param    (number) player_id        Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (number) angle            Angle in degrees.
 * @return   (boolean)                 True on success.
 *
 */
bool Function_SetPlayerAngle(std::int64_t id, float angle) {
  if (auto* npc = GetNpcById(id)) {
    const float radians = glm::radians(angle);
    const zVEC3 heading_vector(std::sin(radians), 0.0F, std::cos(radians));
    npc->SetHeadingYWorld(npc->GetPositionWorld() + heading_vector);

    if (auto* player = GetPlayerByIdSigned(id)) {
      player->base_player().set_rotation(glm::vec3(heading_vector[VX], heading_vector[VY], heading_vector[VZ]));
    }
    return true;
  }
  return false;
}

/* luagmp (func)
 *
 * This function will return the player/NPC facing angle in world space, or nil if unavailable.
 *
 * @version  0.3.0
 * @name     getPlayerAngle
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (number|nil)        Angle in degrees or nil.
 *
 */
sol::object Function_GetPlayerAngle(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zVEC3 forward = npc->GetAtVectorWorld();
    return sol::make_object(lua, ::lua::bind_helpers::NormalizeDegrees(glm::degrees(std::atan2(forward[VX], forward[VZ]))));
  }

  return sol::nil;
}

/* luagmp (func)
 *
 * This function will give an item to the player or NPC on the client.
 *
 * @version  0.3.0
 * @name     giveItem
 * @side     client
 * @category Inventory
 * @param    (number) player_id    Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance     Item instance name.
 * @param    (number) amount       Amount to give.
 * @return   (boolean)             True on success.
 *
 */
bool Function_GiveItem(std::int64_t id, const std::string& instance, std::int32_t amount) {
  if (amount <= 0) {
    return false;
  }

  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = Gothic_II_Addon::parser) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        if (auto* existing = npc->inventory2.IsIn(instance_id, 1)) {
          existing->amount += amount;
          return true;
        }

        oCItem* item = zfactory ? zfactory->CreateItem(instance_id) : nullptr;
        if (!item) {
          return false;
        }

        item->amount = amount;
        npc->inventory2.Insert(item);
        return true;
      }
    }
  }

  return false;
}

/* luagmp (func)
 *
 * This function will equip an item for the player/NPC.
 *
 * @version  0.3.0
 * @name     equipItem
 * @side     client
 * @category Player
 * @param    (number) player_id      Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance       Item instance name.
 * @return   (boolean)               True on success.
 *
 */
bool Function_EquipItem(std::int64_t id, const std::string& instance) {
  return EquipInventoryInstance(GetNpcById(id), instance);
}

/* luagmp (func)
 *
 * This function will unequip an item from the player/NPC.
 *
 * @version  0.3.0
 * @name     unequipItem
 * @side     client
 * @category Player
 * @param    (number) player_id      Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance       Item instance name.
 * @return   (boolean)               True on success.
 *
 */
bool Function_UnequipItem(std::int64_t id, const std::string& instance) {
  auto* npc = GetNpcById(id);
  auto lookup = FindInventoryItem(npc, instance, false);
  if (!lookup.item) {
    return false;
  }

  const bool was_equipped = lookup.item->HasFlag(ITM_FLAG_ACTIVE) || npc->GetRightHand() == lookup.item || npc->GetLeftHand() == lookup.item;
  if (!was_equipped) {
    return false;
  }

  npc->UnequipItem(lookup.item);
  RemoveItemFromHandSlots(npc, lookup.item);
  return !lookup.item->HasFlag(ITM_FLAG_ACTIVE) && npc->GetRightHand() != lookup.item && npc->GetLeftHand() != lookup.item;
}

/* luagmp (func)
 *
 * This function will equip an armor item for a player or NPC.
 *
 * @version  0.3.0
 * @name     equipArmor
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @param    (string) instance     Armor item instance name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipArmor(std::int64_t id, const std::string& instance) {
  return EquipSlotInstance(GetNpcById(id), EquipmentSlot::Armor, instance);
}

/* luagmp (func)
 *
 * This function will unequip the current armor from a player or NPC.
 *
 * @version  0.3.0
 * @name     unequipArmor
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @return   (boolean)             True when an armor item was equipped and unequipped.
 *
 */
bool Function_UnequipArmor(std::int64_t id) {
  return UnequipSlot(id, EquipmentSlot::Armor);
}

/* luagmp (func)
 *
 * This function will equip a melee weapon for a player or NPC.
 *
 * @version  0.3.0
 * @name     equipMeleeWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @param    (string) instance     Melee weapon item instance name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipMeleeWeapon(std::int64_t id, const std::string& instance) {
  return EquipSlotInstance(GetNpcById(id), EquipmentSlot::MeleeWeapon, instance);
}

/* luagmp (func)
 *
 * This function will unequip the current melee weapon from a player or NPC.
 *
 * @version  0.3.0
 * @name     unequipMeleeWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @return   (boolean)             True when a melee weapon was equipped and unequipped.
 *
 */
bool Function_UnequipMeleeWeapon(std::int64_t id) {
  return UnequipSlot(id, EquipmentSlot::MeleeWeapon);
}

/* luagmp (func)
 *
 * This function will equip a ranged weapon for a player or NPC.
 *
 * @version  0.3.0
 * @name     equipRangedWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @param    (string) instance     Ranged weapon item instance name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipRangedWeapon(std::int64_t id, const std::string& instance) {
  return EquipSlotInstance(GetNpcById(id), EquipmentSlot::RangedWeapon, instance);
}

/* luagmp (func)
 *
 * This function will unequip the current ranged weapon from a player or NPC.
 *
 * @version  0.3.0
 * @name     unequipRangedWeapon
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @return   (boolean)             True when a ranged weapon was equipped and unequipped.
 *
 */
bool Function_UnequipRangedWeapon(std::int64_t id) {
  return UnequipSlot(id, EquipmentSlot::RangedWeapon);
}

/* luagmp (func)
 *
 * This function will equip a helmet item for a player or NPC.
 *
 * @version  0.3.0
 * @name     equipHelmet
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @param    (string) instance     Helmet item instance name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipHelmet(std::int64_t id, const std::string& instance) {
  return EquipSlotInstance(GetNpcById(id), EquipmentSlot::Helmet, instance);
}

/* luagmp (func)
 *
 * This function will unequip the current helmet from a player or NPC.
 *
 * @version  0.3.0
 * @name     unequipHelmet
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @return   (boolean)             True when a helmet item was equipped and unequipped.
 *
 */
bool Function_UnequipHelmet(std::int64_t id) {
  return UnequipSlot(id, EquipmentSlot::Helmet);
}

/* luagmp (func)
 *
 * This function will equip a shield item for a player or NPC.
 *
 * @version  0.3.0
 * @name     equipShield
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @param    (string) instance     Shield item instance name.
 * @return   (boolean)             True on success.
 *
 */
bool Function_EquipShield(std::int64_t id, const std::string& instance) {
  return EquipSlotInstance(GetNpcById(id), EquipmentSlot::Shield, instance);
}

/* luagmp (func)
 *
 * This function will unequip the current shield from a player or NPC.
 *
 * @version  0.3.0
 * @name     unequipShield
 * @side     client
 * @category Player
 * @param    (number) player_id    Target player or client NPC id.
 * @return   (boolean)             True when a shield item was equipped and unequipped.
 *
 */
bool Function_UnequipShield(std::int64_t id) {
  return UnequipSlot(id, EquipmentSlot::Shield);
}

/* luagmp (func)
 *
 * This function will return the amount of a specific item in the player/NPC inventory on the client.
 *
 * @version  0.3.0
 * @name     hasItem
 * @side     client
 * @category Inventory
 * @param    (number) player_id      Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance       Item instance name.
 * @return   (number)                Item amount or 0 if missing.
 *
 */
int Function_HasItem(std::int64_t id, const std::string& instance) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = Gothic_II_Addon::parser) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        if (auto* item = npc->inventory2.IsIn(instance_id, 1)) {
          return std::max(0, item->amount);
        }
      }
    }
  }

  return 0;
}

/* luagmp (func)
 *
 * This function will return the local player's currently selected inventory slot.
 *
 * @version  0.3.0
 * @name     getCurrentInventorySlot
 * @side     client
 * @category Inventory
 * @return   (number|nil)  Current inventory slot, starting from 0, or nil if unavailable.
 *
 */
sol::object Function_GetCurrentInventorySlot(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!player) {
    return sol::nil;
  }

  return sol::make_object(lua, player->inventory2.selectedItem);
}

/* luagmp (func)
 *
 * This function will return the local player's inventory as item instance keys with item amounts as values.
 *
 * @version  0.3.0
 * @name     getInventory
 * @side     client
 * @category Inventory
 * @return   ({[string]: number})  Inventory table indexed by item instance.
 *
 */
sol::object Function_GetInventory(sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table inventory = lua.create_table();
  if (!player) {
    return sol::make_object(lua, inventory);
  }

  int slot = 0;
  while (auto* item = player->GetItem(0, slot++)) {
    auto instance = ItemInstanceName(item);
    if (!instance.has_value()) {
      continue;
    }

    inventory[*instance] = std::max(0, item->amount);
  }

  return sol::make_object(lua, inventory);
}

/* luagmp (func)
 *
 * This function will return the item instance from the local player's inventory slot.
 *
 * @version  0.3.0
 * @name     getItemBySlot
 * @side     client
 * @category Inventory
 * @param    (number) slot       Inventory slot, starting from 0.
 * @return   (string|nil)        Item instance, or nil if the slot is empty.
 *
 */
sol::object Function_GetItemBySlot(std::int32_t slot, sol::this_state ts) {
  sol::state_view lua(ts);
  if (!player || slot < 0) {
    return sol::nil;
  }

  auto instance = ItemInstanceName(player->GetItem(0, slot));
  if (!instance.has_value()) {
    return sol::nil;
  }

  return sol::make_object(lua, *instance);
}

/* luagmp (func)
 *
 * This function will return the item instance currently held in the local player's left hand.
 *
 * @version  0.3.0
 * @name     getLeftHand
 * @side     client
 * @category Inventory
 * @return   (string|nil)  Item instance, or nil if the hand is empty.
 *
 */
sol::object Function_GetLeftHand(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!player) {
    return sol::nil;
  }

  auto instance = ItemInstanceName(dynamic_cast<oCItem*>(player->GetLeftHand()));
  if (!instance.has_value()) {
    return sol::nil;
  }

  return sol::make_object(lua, *instance);
}

/* luagmp (func)
 *
 * This function will return the item instance currently held in the local player's right hand.
 *
 * @version  0.3.0
 * @name     getRightHand
 * @side     client
 * @category Inventory
 * @return   (string|nil)  Item instance, or nil if the hand is empty.
 *
 */
sol::object Function_GetRightHand(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!player) {
    return sol::nil;
  }

  auto instance = ItemInstanceName(dynamic_cast<oCItem*>(player->GetRightHand()));
  if (!instance.has_value()) {
    return sol::nil;
  }

  return sol::make_object(lua, *instance);
}

/* luagmp (func)
 *
 * This function will drop an item from the local player's inventory.
 *
 * @version  0.3.0
 * @name     dropItem
 * @side     client
 * @category Inventory
 * @param    (string) instance      Item instance name.
 * @param    (number|nil) amount    Optional amount to drop. Defaults to the whole stack.
 * @return   (boolean)              True on success.
 *
 */
bool Function_DropItem(const std::string& instance, sol::optional<std::int32_t> amount) {
  if (!player || !zfactory) {
    return false;
  }

  auto* parser = Gothic_II_Addon::parser;
  if (!parser) {
    return false;
  }

  zSTRING instance_name(instance.c_str());
  const int instance_id = parser->GetIndex(instance_name);
  if (instance_id < 0) {
    return false;
  }

  auto* item = player->inventory2.IsIn(instance_id, 1);
  if (!item || item->amount <= 0) {
    return false;
  }

  std::int32_t drop_amount = amount ? *amount : item->amount;
  if (drop_amount <= 0) {
    return false;
  }
  drop_amount = std::min(drop_amount, item->amount);

  if (item->amount > drop_amount) {
    item->amount -= drop_amount;

    auto* dropped_item = zfactory->CreateItem(instance_id);
    if (!dropped_item) {
      item->amount += drop_amount;
      return false;
    }

    dropped_item->amount = drop_amount;
    player->DoPutInInventory(dropped_item);
    return player->DoDropVob(dropped_item) != 0;
  }

  return player->DoDropVob(item) != 0;
}

/* luagmp (func)
 *
 * This function will drop the item from the local player's inventory slot.
 *
 * @version  0.3.0
 * @name     dropItemBySlot
 * @side     client
 * @category Inventory
 * @param    (number) slot   Inventory slot, starting from 0.
 * @return   (boolean)       True on success.
 *
 */
bool Function_DropItemBySlot(std::int32_t slot) {
  if (!player || slot < 0) {
    return false;
  }

  auto* item = player->GetItem(0, slot);
  if (!item) {
    return false;
  }

  return player->DoDropVob(item) != 0;
}

/* luagmp (func)
 *
 * This function will remove an item from the player/NPC inventory on the client.
 *
 * @version  0.3.0
 * @name     removeItem
 * @side     client
 * @category Inventory
 * @param    (number) player_id      Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (string) instance       Item instance name.
 * @param    (number) amount         Amount to remove.
 * @return   (boolean)               True on success.
 *
 */
bool Function_RemoveItem(std::int64_t id, const std::string& instance, std::int32_t amount) {
  if (amount <= 0) {
    return 0;
  }

  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = Gothic_II_Addon::parser) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        if (auto* item = npc->inventory2.IsIn(instance_id, 1)) {
          const int remove_amount = std::min(amount, item->amount);
          if (remove_amount > 0) {
            return npc->inventory2.Remove(instance_id, remove_amount);
          }
        }
      }
    }
  }

  return false;
}

/* luagmp (func)
 *
 * This function will remove all items from the hero's inventory.
 *
 * @version  0.3.0
 * @name     clearInventory
 * @side     client
 * @category Inventory
 *
 */
void Function_ClearInventory() {
  if (!player) {
    return;
  }

  UnequipSlot(player, EquipmentSlot::Armor);
  UnequipSlot(player, EquipmentSlot::RangedWeapon);
  UnequipSlot(player, EquipmentSlot::MeleeWeapon);
  UnequipSlot(player, EquipmentSlot::Helmet);
  UnequipSlot(player, EquipmentSlot::Shield);

  player->inventory2.ClearInventory();
}

/* luagmp (func)
 *
 * This function will open the local player's inventory.
 *
 * @version  0.3.0
 * @name     openInventory
 * @side     client
 * @category Inventory
 * @return   (boolean)  True on success.
 *
 */
bool Function_OpenInventory() {
  if (!player) {
    return false;
  }

  player->OpenInventory(1);
  return true;
}

/* luagmp (func)
 *
 * This function will close the local player's inventory.
 *
 * @version  0.3.0
 * @name     closeInventory
 * @side     client
 * @category Inventory
 * @return   (boolean)  True on success.
 *
 */
bool Function_CloseInventory() {
  if (!player) {
    return false;
  }

  player->CloseInventory();
  return true;
}

/* luagmp (func)
 *
 * This function will check whether the local player's inventory is open.
 *
 * @version  0.3.0
 * @name     isInventoryOpen
 * @side     client
 * @category Inventory
 * @return   (boolean)  True when the inventory is open.
 *
 */
bool Function_IsInventoryOpen() {
  return player && player->inventory2.IsOpen() != 0;
}

/* luagmp (func)
 *
 * This function will change the player's current game world.
 *
 * @version  0.3.0
 * @name     changeWorld
 * @side     client
 * @category World
 * @param    (string) world             World filename.
 * @param    (string|nil) start_point   Optional start point name.
 *
 */
void Function_ChangeWorld(const std::string& world, sol::optional<std::string> start_point) {
  if (!ogame || world.empty()) {
    SPDLOG_WARN("changeWorld called without an active game or world name");
    return;
  }

  const std::string point = start_point.value_or("");
  GMPCore::Instance().DeferToNextFrame([world, point]() {
    if (!ogame)
      return;
    PrepareWorldChange();
    zSTRING z_world(world.c_str());
    zSTRING z_start_point(point.c_str());
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel(z_world, z_start_point);
    Patch::ChangeLevelEnabled(false);
    ResetWorldRenderState();
  });
}

/* luagmp (func)
 *
 * This function will return the current game world filename.
 *
 * @version  0.3.0
 * @name     getWorld
 * @side     client
 * @category World
 * @return   (string)           World filename.
 *
 */
sol::object Function_GetWorld(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!ogame || !ogame->GetGameWorld()) {
    return sol::nil;
  }

  return sol::make_object(lua, std::string(ogame->GetGameWorld()->GetWorldFilename().ToChar()));
}

/* luagmp (func)
 *
 * This function will create a client-side NPC entry and return an internal npc id (<0).
 *
 * @version  0.3.0
 * @name     createNpc
 * @side     client
 * @category NPC
 * @note     By default, the NPC won't be added to the world. It's necessary to call [`spawnNpc`](spawnNpc.md) afterwards.
 * @param    (string) name       Name for the created NPC.
 * @return   (number|nil, table|nil)            Negative local NPC id and nil on success; nil and {message = string} on failure.
 *
 */
::lua::bind_helpers::LuaResult Function_CreateNpc(const std::string& name, sol::this_state state) {
  sol::state_view lua(state);
  if (!HasFactoryAndParser()) {
    SPDLOG_WARN("createNpc: missing game engine components");
    return ::lua::bind_helpers::MakeFailure(lua, "Game engine is not ready to create a local NPC");
  }

  auto* parser = Gothic_II_Addon::parser;
  const int default_instance = parser->GetIndex("PC_HERO");
  if (default_instance < 0) {
    SPDLOG_WARN("createNpc: failed to resolve default instance 'PC_HERO'");
    return ::lua::bind_helpers::MakeFailure(lua, "Default NPC instance PC_HERO was not found");
  }

  oCNpc* npc = zfactory->CreateNpc(default_instance);
  if (!npc) {
    SPDLOG_WARN("createNpc: failed to allocate npc");
    return ::lua::bind_helpers::MakeFailure(lua, "Local NPC allocation failed");
  }

  npc->name[0] = name.c_str();

  const int npc_id = g_next_npc_id--;
  g_client_npcs.emplace(npc_id, ClientNpc{npc, name});
  return ::lua::bind_helpers::MakeSuccess(lua, sol::make_object(lua, npc_id));
}

/* luagmp (func)
 *
 * This function will destroy a client-side NPC.
 *
 * @version  0.3.0
 * @name     destroyNpc
 * @side     client
 * @category NPC
 * @param    (number) npc_id     NPC id.
 * @return   (boolean)           True on success.
 *
 */
bool Function_DestroyNpc(std::int64_t npc_id) {
  auto it = FindClientNpc(npc_id);
  if (it == g_client_npcs.end()) {
    return false;
  }

  oCNpc* npc = it->second.npc;
  if (npc && npc->GetHomeWorld()) {
    npc->Disable();
    npc->RemoveVobFromWorld();
  }

  if (auto* spawn_manager = GetSpawnManager()) {
    spawn_manager->DeleteNpc(npc);
  }

  g_client_npcs.erase(it);
  return true;
}

/* luagmp (func)
 *
 * This function will spawn a previously created NPC into the world.
 *
 * @version  0.3.0
 * @name     spawnNpc
 * @side     client
 * @category NPC
 * @param    (number) npc_id             NPC id.
 * @param    (string|nil) instance_name  Optional instance name (defaults to "PC_HERO").
 * @return   (boolean)                   True if spawn attached to world.
 *
 */
bool Function_SpawnNpc(std::int64_t npc_id, sol::optional<std::string> instance_name) {
  auto it = FindClientNpc(npc_id);
  if (it == g_client_npcs.end()) {
    return false;
  }

  ClientNpc& entry = it->second;
  if (entry.spawned || entry.npc->GetHomeWorld() != nullptr) {
    SPDLOG_WARN("spawnNpc: npc {} is already spawned", npc_id);
    return false;
  }

  if (!HasSpawnPrerequisites()) {
    SPDLOG_WARN("spawnNpc: missing game engine components");
    return false;
  }

  const std::string instance = instance_name.value_or("PC_HERO");
  auto* parser = Gothic_II_Addon::parser;
  const int instance_index = parser->GetIndex(instance.c_str());
  if (instance_index < 0) {
    SPDLOG_WARN("spawnNpc: instance '{}' not found", instance);
    return false;
  }

  entry.npc->InitByScript(instance_index, 0);
  entry.npc->startAIState = 0;
  entry.npc->name[0] = entry.name.c_str();

  zVEC3 spawn_position{0.0f, 0.0f, 0.0f};
  GetSpawnManager()->SpawnNpc(entry.npc, spawn_position, 0.0f);

  if (entry.npc->GetHomeWorld() == nullptr) {
    entry.npc->Enable(spawn_position);
  }

  const bool attached = entry.npc->GetHomeWorld() != nullptr;
  if (!attached) {
    SPDLOG_WARN("spawnNpc: failed to attach npc to world");
  }

  entry.spawned = attached;

  return attached;
}

/* luagmp (func)
 *
 * This function will unspawn a client NPC from the world.
 *
 * @version  0.3.0
 * @name     unspawnNpc
 * @side     client
 * @category NPC
 * @param    (number) npc_id   Internal npc id.
 * @return   (boolean)         True on success.
 *
 */
bool Function_UnspawnNpc(std::int64_t npc_id) {
  auto it = FindClientNpc(npc_id);
  if (it == g_client_npcs.end()) {
    return false;
  }

  oCNpc* npc = it->second.npc;
  if (!it->second.spawned || npc == nullptr) {
    SPDLOG_WARN("unspawnNpc: npc {} is not spawned", npc_id);
    return false;
  }
  if (npc && npc->GetHomeWorld()) {
    npc->Disable();
    npc->RemoveVobFromWorld();
  }

  it->second.spawned = false;

  return true;
}

/* luagmp (func)
 *
 * This function will check whether a player or client NPC object exists.
 *
 * @version  0.3.0
 * @name     isPlayerCreated
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean)           True when the object exists.
 *
 */
bool Function_IsPlayerCreated(std::int64_t id) {
  if (id < 0) {
    return FindClientNpc(id) != g_client_npcs.end();
  }

  return GetPlayerByIdSigned(id) != nullptr;
}

/* luagmp (func)
 *
 * This function will check whether a player or client NPC is spawned into the world.
 *
 * @version  0.3.0
 * @name     isPlayerStreamed
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean|nil)       True when spawned, false when known but not spawned, nil if unknown.
 *
 */
sol::object Function_IsPlayerStreamed(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (id < 0) {
    auto it = FindClientNpc(id);
    if (it == g_client_npcs.end()) {
      return sol::nil;
    }
    return sol::make_object(lua, it->second.spawned && it->second.npc != nullptr);
  }

  auto* player = GetPlayerByIdSigned(id);
  if (!player) {
    return sol::nil;
  }

  return sol::make_object(lua, player->base_player().has_spawned() && player->GetNpc() != nullptr);
}

/* luagmp (func)
 *
 * This function will check whether a player or NPC is dead.
 *
 * @version  0.3.0
 * @name     isPlayerDead
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean|nil)       Dead state, or nil if unavailable.
 *
 */
sol::object Function_IsPlayerDead(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->IsDead() != 0);
  }
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will check whether a player or NPC is unconscious.
 *
 * @version  0.3.0
 * @name     isPlayerUnconscious
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean|nil)       Unconscious state, or nil if unavailable.
 *
 */
sol::object Function_IsPlayerUnconscious(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->IsUnconscious() != 0);
  }
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will return the local player's current network ping.
 *
 * @version  0.3.0
 * @name     getPlayerPing
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player id; NPC ids are not supported.
 * @return   (number)            Player ping replicated by the server, or -1 if unavailable.
 *
 */
std::int32_t Function_GetPlayerPing(std::int64_t id) {
  auto& net_game = NetGame::Instance();
  if (!net_game.game_client || !net_game.IsConnected() || id < 0) {
    return -1;
  }

  return net_game.game_client->GetPlayerPing(static_cast<std::uint64_t>(id));
}

/* luagmp (func)
 *
 * This function will return the focused player or NPC id.
 *
 * @version  0.3.0
 * @name     getPlayerFocus
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return (number|nil) Focused player or NPC id, including negative client-local NPC ids; nil when the focus has no known id.
 *
 */
sol::optional<std::int64_t> Function_GetPlayerFocus(std::int64_t id) {
  auto* npc = GetNpcById(id);
  if (!npc)
    return sol::nullopt;
  oCNpc* focus = npc->GetFocusNpc();
  if (!focus)
    return sol::nullopt;
  for (const auto& [local_id, entry] : g_client_npcs) {
    if (entry.npc == focus)
      return local_id;
  }
  const auto network_id = NetGame::Instance().GetPlayerIdByNpc(focus);
  if (!network_id)
    return sol::nullopt;
  return static_cast<std::int64_t>(*network_id);
}

/* luagmp (func)
 *
 * This function will return the multiplayer player type.
 *
 * @version  0.3.0
 * @name     getPlayerType
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or server NPC id; client-local NPC ids are not supported.
 * @return   (number|nil)        Player type, or nil if unavailable.
 *
 * @note Accepts player and server NPC ids; negative client-local NPC ids are not supported.
 */
sol::object Function_GetPlayerType(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  if (auto* player = GetPlayerByIdSigned(id)) {
    return sol::make_object(lua, static_cast<int>(player->Type));
  }
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will place a player or NPC on the floor at its current position.
 *
 * @version  0.3.0
 * @name     setPlayerOnFloor
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerOnFloor(std::int64_t id) {
  if (auto* npc = GetNpcById(id)) {
    zVEC3 position = npc->GetPositionWorld();
    npc->SetOnFloor(position);
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will enable or disable collision and physics for a player or NPC.
 *
 * @version  0.3.0
 * @name     setPlayerCollision
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @param    (boolean) enabled   Collision state.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetPlayerCollision(std::int64_t id, bool enabled) {
  if (auto* npc = GetNpcById(id)) {
    npc->SetCollDet(enabled ? 1 : 0);
    if (auto* anictrl = npc->GetAnictrl()) {
      anictrl->SetPhysicsEnabled(enabled ? 1 : 0);
    }
    return true;
  }

  return false;
}

/* luagmp (func)
 *
 * This function will return whether collision is enabled for a player or NPC.
 *
 * @version  0.3.0
 * @name     getPlayerCollision
 * @side     client
 * @category Player
 * @param    (number) player_id  Target player or NPC id; negative ids refer to client-local NPCs.
 * @return   (boolean|nil)       Collision state, or nil if unavailable.
 *
 */
sol::object Function_GetPlayerCollision(std::int64_t id, sol::this_state ts) {
  sol::state_view lua(ts);
  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->collDetectionStatic != 0 || npc->collDetectionDynamic != 0);
  }
  return sol::nil;
}

/* luagmp (func)
 *
 * This function will set the in-game world time (hour, minute).
 *
 * @version  0.3.0
 * @name     setTime
 * @side     client
 * @category Game
 * @param    (number) hour    Hour component.
 * @param    (number) minute  Minute component.
 * @return   (boolean)        True on success.
 *
 */
bool Function_SetTime(int hour, int minute) {
  if (!ogame || !ogame->GetWorldTimer()) {
    return false;
  }

  ogame->GetWorldTimer()->SetTime(hour, minute);
  return true;
}

/* luagmp (func)
 *
 * This function will return the current in-game world time.
 *
 * @version  0.3.0
 * @name     getTime
 * @side     client
 * @category Game
 * @return   ({hour, minute})  Table containing hour and minute.
 *
 */
sol::object Function_GetTime(sol::this_state ts) {
  sol::state_view lua(ts);

  int hour = 0;
  int minute = 0;

  if (ogame && ogame->GetWorldTimer()) {
    ogame->GetWorldTimer()->GetTime(hour, minute);
  }

  sol::table tbl = lua.create_table();
  tbl["hour"] = hour;
  tbl["minute"] = minute;
  return sol::make_object(lua, tbl);
}

/* luagmp (func)
 *
 * This function will set the duration of an in-game day in milliseconds.
 *
 * @version  0.3.0
 * @name     setDayLength
 * @side     client
 * @category Game
 * @param    (number) miliseconds  Day length in milliseconds (min 10000 ms).
 * @return   (boolean)             True on success.
 *
 */
bool Function_SetDayLength(float day_length_ms) {
  if (day_length_ms <= 0.0f) {
    return false;
  }

  constexpr float kMinDayLengthMs = 10000.0f;
  if (day_length_ms < kMinDayLengthMs) {
    SPDLOG_WARN("setDayLength called with invalid value {} ms, clamping to {} ms", day_length_ms, kMinDayLengthMs);
    day_length_ms = kMinDayLengthMs;
  }

  NetGame::Instance().SetDayLengthMs(day_length_ms);
  return true;
}

/* luagmp (func)
 *
 * This function will return the current configured in-game day length in milliseconds.
 *
 * @version  0.3.0
 * @name     getDayLength
 * @side     client
 * @category Game
 * @return   (number)  Day length in milliseconds.
 *
 */
sol::object Function_GetDayLength(sol::this_state ts) {
  sol::state_view lua(ts);
  return sol::make_object(lua, NetGame::Instance().GetDayLengthMs());
}

/* luagmp (func)
 *
 * This function will enable or disable Gothic damage animations for non-local NPCs.
 *
 * @version  0.3.0
 * @name     enable_DamageAnims
 * @side     client
 * @category Game
 * @param    (boolean) enabled  True to allow damage animations.
 *
 */
void Function_Enable_DamageAnims(bool enabled) {
  SetDamageAnimationsEnabled(enabled);
}

/* luagmp (func)
 *
 * This function will enable or disable arrow and bolt trails.
 *
 * @version  0.3.0
 * @name     enable_MunitionTrail
 * @side     client
 * @category Game
 * @param    (boolean) enabled  True to show munition trails.
 *
 */
void Function_Enable_MunitionTrail(bool enabled) {
  SetMunitionTrailEnabled(enabled);
}

/* luagmp (func)
 *
 * This function will enable or disable melee weapon trails.
 *
 * @version  0.3.0
 * @name     enable_WeaponTrail
 * @side     client
 * @category Game
 * @param    (boolean) enabled  True to show weapon trails.
 *
 */
void Function_Enable_WeaponTrail(bool enabled) {
  zCAIPlayer::s_bShowWeaponTrails = enabled ? 1 : 0;
}

/* luagmp (func)
 *
 * This function will set the Gothic blood rendering mode.
 *
 * Blood modes: 0 none, 1 particles, 2 decals, 3 trails, 4 amplification.
 *
 * @version  0.3.0
 * @name     setBloodMode
 * @side     client
 * @category Game
 * @param    (number) mode  Blood mode.
 * @return   (boolean)      True on success.
 *
 */
bool Function_SetBloodMode(std::int32_t mode) {
  if (mode < oCNpc::oEBloodMode_None || mode > oCNpc::oEBloodMode_Amplification) {
    return false;
  }

  oCNpc::modeBlood = static_cast<oCNpc::oEBloodMode>(mode);
  return true;
}

/* luagmp (func)
 *
 * This function will return the current Gothic blood rendering mode.
 *
 * Blood modes: 0 none, 1 particles, 2 decals, 3 trails, 4 amplification.
 *
 * @version  0.3.0
 * @name     getBloodMode
 * @side     client
 * @category Game
 * @return   (number)  Blood mode.
 *
 */
std::int32_t Function_GetBloodMode() {
  return static_cast<std::int32_t>(oCNpc::modeBlood);
}

/* luagmp (func)
 *
 * This function will set the Gothic view distance scalability factor.
 *
 * @version  0.3.0
 * @name     setSightFactor
 * @side     client
 * @category Game
 * @param    (number) factor  Sight factor, clamped to 0.02 - 5.0.
 * @return   (boolean)        True on success.
 *
 */
bool Function_SetSightFactor(float factor) {
  auto* sky_controller = GetActiveSkyController();
  if (!sky_controller || !std::isfinite(factor)) {
    return false;
  }

  factor = std::clamp(factor, 0.02f, 5.0f);
  if (zoptions) {
    const int game_setting = std::max(0, static_cast<int>(factor * 100.0f) / 20 - 1);
    zoptions->WriteInt(zSTRING("PERFORMANCE"), "sightValue", game_setting, 1);
  }

  sky_controller->SetFarZScalability(factor);
  return true;
}

/* luagmp (func)
 *
 * This function will return the current Gothic view distance scalability factor.
 *
 * @version  0.3.0
 * @name     getSightFactor
 * @side     client
 * @category Game
 * @return   (number)  Sight factor.
 *
 */
float Function_GetSightFactor() {
  auto* sky_controller = GetActiveSkyController();
  if (!sky_controller) {
    return 1.0f;
  }

  return sky_controller->GetFarZScalability();
}

/* luagmp (func)
 *
 * This function will set the global progressive mesh LOD strength modifier.
 *
 * @version  0.3.0
 * @name     setLODStrengthModifier
 * @side     client
 * @category Game
 * @param    (number) modifier  LOD strength modifier.
 *
 */
void Function_SetLODStrengthModifier(float modifier) {
  zCProgMeshProto::SetLODParamStrengthModifier(modifier);
}

/* luagmp (func)
 *
 * This function will return the global progressive mesh LOD strength modifier.
 *
 * @version  0.3.0
 * @name     getLODStrengthModifier
 * @side     client
 * @category Game
 * @return   (number)  LOD strength modifier.
 *
 */
float Function_GetLODStrengthModifier() {
  return zCProgMeshProto::GetLODParamStrengthModifier();
}

/* luagmp (func)
 *
 * This function will set the global progressive mesh LOD strength override.
 *
 * Use -1 to use mesh values. Use 0 to disable LOD.
 *
 * @version  0.3.0
 * @name     setLODStrengthOverride
 * @side     client
 * @category Game
 * @param    (number) strength  LOD strength override.
 *
 */
void Function_SetLODStrengthOverride(float strength) {
  zCProgMeshProto::SetLODParamStrength(strength);
}

/* luagmp (func)
 *
 * This function will return the global progressive mesh LOD strength override.
 *
 * @version  0.3.0
 * @name     getLODStrengthOverride
 * @side     client
 * @category Game
 * @return   (number)  LOD strength override.
 *
 */
float Function_GetLODStrengthOverride() {
  return zCProgMeshProto::s_lodParamStrengthOverride;
}

/* luagmp (func)
 *
 * This function will play a BIK video from Gothic's _Work\Data\Video path.
 *
 * @version  0.3.0
 * @name     playVideo
 * @side     client
 * @category Game
 * @param    (string) file_name  BIK file name, e.g. "Intro.bik".
 * @return   (boolean)           True when Gothic accepted the video.
 *
 */
bool Function_PlayVideo(const std::string& file_name) {
  if (!gameMan || file_name.empty()) {
    return false;
  }

  return gameMan->PlayVideo(zSTRING(file_name.c_str())) != 0;
}

/* luagmp (func)
 *
 * This function will check whether a physical file exists.
 *
 * @version  0.3.0
 * @name     fileExists
 * @side     client
 * @category Game
 * @param    (string) file_path  File path, e.g. "System/Gothic.ini".
 * @return   (boolean)           True when the file exists.
 *
 */
bool Function_FileExists(const std::string& file_path) {
  if (file_path.empty()) {
    return false;
  }

  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::path(file_path), error);
}

/* luagmp (func)
 *
 * This function will return the current game resolution.
 *
 * @version  0.3.0
 * @name     getResolution
 * @side     client
 * @category Interface
 * @return   ({x, y})  Table containing width and height.
 *
 */
sol::object Function_GetResolution(sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table resolution = lua.create_table();

  int width = 800;
  int height = 600;
  int bpp = 32;
  if (zrenderer) {
    width = zrenderer->vid_xdim;
    height = zrenderer->vid_ydim;
    bpp = zrenderer->vid_bpp;
  }

  resolution["x"] = width;
  resolution["y"] = height;
  resolution["bpp"] = bpp;
  return sol::make_object(lua, resolution);
}

/* luagmp (func)
 *
 * This function will set the current game resolution.
 *
 * @version  0.3.0
 * @name     setResolution
 * @side     client
 * @category Interface
 * @param    (number) width      Resolution width.
 * @param    (number) height     Resolution height.
 * @param    (number|nil) bpp    Optional bits per pixel. Defaults to the current renderer bpp.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetResolution(std::int32_t width, std::int32_t height, sol::optional<std::int32_t> bpp) {
  if (!zrenderer || width <= 0 || height <= 0) {
    return false;
  }

  std::optional<int> requested_bpp;
  if (bpp.has_value()) {
    requested_bpp = *bpp;
  } else if (zrenderer->vid_bpp > 0) {
    requested_bpp = zrenderer->vid_bpp;
  }

  auto selection = FindVideoMode(width, height, requested_bpp);
  if (!selection.has_value() && requested_bpp.has_value()) {
    selection = FindVideoMode(width, height, std::nullopt);
  }
  if (!selection.has_value()) {
    return false;
  }

  HWND window = hWndApp;
  zCView::SetMode(selection->info.xdim, selection->info.ydim, selection->info.bpp, &window);

  zrenderer->SetViewport(0, 0, selection->info.xdim, selection->info.ydim);

  if (zoptions) {
    zoptions->WriteInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", selection->info.xdim);
    zoptions->WriteInt(zOPT_SEC_VIDEO, "zVidResFullscreenY", selection->info.ydim);
    zoptions->WriteInt(zOPT_SEC_VIDEO, "zVidResFullscreenBPP", selection->info.bpp);
  }

  return true;
}

/* luagmp (func)
 *
 * This function will return the available game resolutions.
 *
 * @version  0.3.0
 * @name     getAvailableResolutions
 * @side     client
 * @category Interface
 * @return   (table)  Array of resolution tables containing x, y, and bpp.
 *
 */
sol::object Function_GetAvailableResolutions(sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table resolutions = lua.create_table();
  if (!zrenderer) {
    return sol::make_object(lua, resolutions);
  }

  int result_index = 1;
  const int mode_count = zrenderer->Vid_GetNumModes();
  for (int mode_index = 0; mode_index < mode_count; ++mode_index) {
    zTRnd_VidModeInfo mode_info;
    if (!zrenderer->Vid_GetModeInfo(mode_info, mode_index)) {
      continue;
    }

    sol::table resolution = lua.create_table();
    resolution["x"] = mode_info.xdim;
    resolution["y"] = mode_info.ydim;
    resolution["bpp"] = mode_info.bpp;
    resolutions[result_index++] = resolution;
  }

  return sol::make_object(lua, resolutions);
}

/* luagmp (func)
 *
 * This function will return the current frame rate estimate.
 *
 * @version  0.3.0
 * @name     getFpsRate
 * @side     client
 * @category Game
 * @return   (number)  Frames per second, or 0 if unavailable.
 *
 */
std::int32_t Function_GetFpsRate() {
  if (!ztimer || ztimer->frameTimeFloatSecs <= 0.0f) {
    return 0;
  }

  return static_cast<std::int32_t>(std::lround(1.0f / ztimer->frameTimeFloatSecs));
}

/* luagmp (func)
 *
 * This function will return client network statistics.
 *
 * @version  0.3.0
 * @name     getNetworkStats
 * @side     client
 * @category Multiplayer
 * @note     GNS connection quality is a recent fraction from 0.0 to 1.0, or -1 when unavailable; it is not cumulative packet loss.
 * @note     Traffic rates are recent transport estimates. Pending and unacknowledged values are bytes, queue time is microseconds.
 * @return   ({available, pingMs, localQuality, remoteQuality, outgoingPacketsPerSecond, outgoingBytesPerSecond, incomingPacketsPerSecond,
 * incomingBytesPerSecond, sendRateBytesPerSecond, pendingUnreliableBytes, pendingReliableBytes, unacknowledgedReliableBytes, controlQueueTimeUsec,
 * realtimeQueueTimeUsec})  Table containing current network statistics.
 *
 */
sol::object Function_GetNetworkStats(sol::this_state ts) {
  sol::state_view lua(ts);

  Net::NetworkStats stats;
  auto& net_game = NetGame::Instance();
  if (net_game.game_client) {
    stats = net_game.game_client->GetNetworkStats();
  }

  sol::table tbl = lua.create_table();
  tbl["available"] = stats.available;
  tbl["pingMs"] = stats.ping_ms;
  tbl["localQuality"] = stats.local_quality;
  tbl["remoteQuality"] = stats.remote_quality;
  tbl["outgoingPacketsPerSecond"] = stats.outgoing_packets_per_second;
  tbl["outgoingBytesPerSecond"] = stats.outgoing_bytes_per_second;
  tbl["incomingPacketsPerSecond"] = stats.incoming_packets_per_second;
  tbl["incomingBytesPerSecond"] = stats.incoming_bytes_per_second;
  tbl["sendRateBytesPerSecond"] = stats.send_rate_bytes_per_second;
  tbl["pendingUnreliableBytes"] = stats.pending_unreliable_bytes;
  tbl["pendingReliableBytes"] = stats.pending_reliable_bytes;
  tbl["unacknowledgedReliableBytes"] = stats.unacknowledged_reliable_bytes;
  tbl["controlQueueTimeUsec"] = stats.control_queue_time_usec;
  tbl["realtimeQueueTimeUsec"] = stats.realtime_queue_time_usec;

  return sol::make_object(lua, tbl);
}

/* luagmp (func)
 *
 * This function will return the language currently used by the client.
 *
 * @version  0.3.0
 * @name     getLanguageIndex
 * @side     client
 * @category Multiplayer
 * @return   (number)  Language index selected in the client settings.
 *
 */
int Function_GetLanguageIndex() {
  return LanguageManager::Instance().GetActiveLanguageIndex();
}

/* luagmp (func)
 *
 * This function will close the game immediately.
 *
 * @version  0.3.0
 * @name     exitGame
 * @side     client
 * @category Game
 *
 */
void Function_ExitGame() {
  GMPCore::ExitGame(0);
}

/* luagmp (func)
 *
 * This function will enable/disable opening GMP menu with ESC.
 *
 * @version  0.3.0
 * @name     enableGMPMenu
 * @side     client
 * @category Multiplayer
 * @param    (boolean) enable  True to allow ESC to open/close GMP menu, false to ignore ESC for GMP menu.
 *
 */
void Function_EnableGMPMenu(bool enable) {
  EnableGMPMenu(enable);
}

/* luagmp (func)
 *
 * This function will force open GMP menu.
 *
 * @version  0.3.0
 * @name     openGMPMenu
 * @side     client
 * @category Multiplayer
 *
 */
void Function_OpenGMPMenu() {
  OpenGMPMenu();
}

/* luagmp (func)
 *
 * This function will force close GMP menu.
 *
 * @version  0.3.0
 * @name     closeGMPMenu
 * @side     client
 * @category Multiplayer
 *
 */
void Function_CloseGMPMenu() {
  CloseGMPMenu();
}

void CleanupGothicViews() {
  LuaLabel::CleanupViews();
  LuaWorldLabel::CleanupViews();
  LuaSprite::CleanupViews();
  LuaCursor::CleanupViews();
}

/* luagmp (const)
 *
 * Represents planet sun type.
 *
 * @category Sky
 * @side     client
 * @name     PLANET_SUN
 *
 */

/* luagmp (const)
 *
 * Represents planet moon type.
 *
 * @category Sky
 * @side     client
 * @name     PLANET_MOON
 *
 */

/* luagmp (const)
 *
 * English language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_EN
 *
 */

/* luagmp (const)
 *
 * Polish language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_PL
 *
 */

/* luagmp (const)
 *
 * German language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_DE
 *
 */

/* luagmp (const)
 *
 * Russian language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_RU
 *
 */

/* luagmp (const)
 *
 * Czech language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_CZ
 *
 */

/* luagmp (const)
 *
 * Slovak language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_SK
 *
 */

/* luagmp (const)
 *
 * Interslavic language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_ISV
 *
 */

/* luagmp (const)
 *
 * Ukrainian language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_UA
 *
 */

/* luagmp (const)
 *
 * Italian language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_IT
 *
 */

/* luagmp (const)
 *
 * Hungarian language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_HU
 *
 */

/* luagmp (const)
 *
 * French language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_FR
 *
 */

/* luagmp (const)
 *
 * Danish language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_DA
 *
 */

/* luagmp (const)
 *
 * Spanish language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_ES
 *
 */

/* luagmp (const)
 *
 * Silesian language index.
 *
 * @category Language
 * @side     client
 * @name     LANG_SZL
 *
 */

void BindGothicSpecific(sol::state& lua) {
  SPDLOG_TRACE("Initializing Gothic 2 Addon 2.6 specific bindings...");

  lua["setPlayerInstance"] = Function_SetPlayerInstance;
  lua["getPlayerInstance"] = Function_GetPlayerInstance;
  lua["setPlayerName"] = Function_SetPlayerName;
  lua["getPlayerName"] = Function_GetPlayerName;
  lua["setPlayerNameColor"] = Function_SetPlayerNameColor;
  lua["getPlayerNameColor"] = Function_GetPlayerNameColor;
  lua["setPlayerHealth"] = Function_SetPlayerHealth;
  lua["getPlayerHealth"] = Function_GetPlayerHealth;
  lua["setPlayerMaxHealth"] = Function_SetPlayerMaxHealth;
  lua["getPlayerMaxHealth"] = Function_GetPlayerMaxHealth;
  lua["setPlayerMana"] = Function_SetPlayerMana;
  lua["getPlayerMana"] = Function_GetPlayerMana;
  lua["setPlayerMaxMana"] = Function_SetPlayerMaxMana;
  lua["getPlayerMaxMana"] = Function_GetPlayerMaxMana;
  lua["setPlayerStrength"] = Function_SetPlayerStrength;
  lua["getPlayerStrength"] = Function_GetPlayerStrength;
  lua["setPlayerDexterity"] = Function_SetPlayerDexterity;
  lua["getPlayerDexterity"] = Function_GetPlayerDexterity;
  lua["setPlayerSkillWeapon"] = Function_SetPlayerSkillWeapon;
  lua["getPlayerSkillWeapon"] = Function_GetPlayerSkillWeapon;
  lua["setPlayerTalent"] = Function_SetPlayerTalent;
  lua["getPlayerTalent"] = Function_GetPlayerTalent;
  lua["setPlayerLevel"] = Function_SetPlayerLevel;
  lua["getPlayerLevel"] = Function_GetPlayerLevel;
  lua["setLearnPoints"] = Function_SetLearnPoints;
  lua["getLearnPoints"] = Function_GetLearnPoints;
  lua["setExp"] = Function_SetExp;
  lua["getExp"] = Function_GetExp;
  lua["setPlayerFatness"] = Function_SetPlayerFatness;
  lua["getPlayerFatness"] = Function_GetPlayerFatness;
  lua["setPlayerVisual"] = Function_SetPlayerVisual;
  lua["getPlayerVisual"] = Function_GetPlayerVisual;
  lua["setPlayerScale"] = Function_SetPlayerScale;
  lua["getPlayerScale"] = Function_GetPlayerScale;
  lua["setPlayerWeaponMode"] = Function_SetPlayerWeaponMode;
  lua["getPlayerWeaponMode"] = Function_GetPlayerWeaponMode;
  lua["getPlayerAni"] = Function_GetPlayerAni;
  lua["getPlayerAniId"] = Function_GetPlayerAniId;
  lua["getPlayerArmor"] = Function_GetPlayerArmor;
  lua["getPlayerMeleeWeapon"] = Function_GetPlayerMeleeWeapon;
  lua["getPlayerRangedWeapon"] = Function_GetPlayerRangedWeapon;
  lua["getPlayerHelmet"] = Function_GetPlayerHelmet;
  lua["getPlayerShield"] = Function_GetPlayerShield;
  lua["applyPlayerOverlay"] = Function_ApplyPlayerOverlay;
  lua["getPlayerOverlays"] = Function_GetPlayerOverlays;
  lua["removePlayerOverlay"] = Function_RemovePlayerOverlay;
  lua["playAni"] = Function_PlayAni;
  lua["stopAni"] = Function_StopAni;
  lua["playFaceAni"] = Function_PlayFaceAni;
  lua["stopFaceAni"] = Function_StopFaceAni;
  lua["playGesticulation"] = Function_PlayGesticulation;
  lua["setPlayerPosition"] = Function_SetPlayerPosition;
  lua["getPlayerPosition"] = Function_GetPlayerPosition;
  lua["getPlayerWorld"] = Function_GetPlayerWorld;
  lua["setPlayerAngle"] = Function_SetPlayerAngle;
  lua["getPlayerAngle"] = Function_GetPlayerAngle;

  lua["giveItem"] = Function_GiveItem;
  lua["equipItem"] = Function_EquipItem;
  lua["unequipItem"] = Function_UnequipItem;
  lua["equipArmor"] = Function_EquipArmor;
  lua["unequipArmor"] = Function_UnequipArmor;
  lua["equipMeleeWeapon"] = Function_EquipMeleeWeapon;
  lua["unequipMeleeWeapon"] = Function_UnequipMeleeWeapon;
  lua["equipRangedWeapon"] = Function_EquipRangedWeapon;
  lua["unequipRangedWeapon"] = Function_UnequipRangedWeapon;
  lua["equipHelmet"] = Function_EquipHelmet;
  lua["unequipHelmet"] = Function_UnequipHelmet;
  lua["equipShield"] = Function_EquipShield;
  lua["unequipShield"] = Function_UnequipShield;
  lua["hasItem"] = Function_HasItem;
  lua["getCurrentInventorySlot"] = Function_GetCurrentInventorySlot;
  lua["getInventory"] = Function_GetInventory;
  lua["getItemBySlot"] = Function_GetItemBySlot;
  lua["getLeftHand"] = Function_GetLeftHand;
  lua["getRightHand"] = Function_GetRightHand;
  lua["dropItem"] = Function_DropItem;
  lua["dropItemBySlot"] = Function_DropItemBySlot;
  lua["removeItem"] = Function_RemoveItem;
  lua["clearInventory"] = Function_ClearInventory;
  lua["openInventory"] = Function_OpenInventory;
  lua["closeInventory"] = Function_CloseInventory;
  lua["isInventoryOpen"] = Function_IsInventoryOpen;

  lua["changeWorld"] = Function_ChangeWorld;
  lua["getWorld"] = Function_GetWorld;

  lua["createNpc"] = Function_CreateNpc;
  lua["destroyNpc"] = Function_DestroyNpc;
  lua["spawnNpc"] = Function_SpawnNpc;
  lua["unspawnNpc"] = Function_UnspawnNpc;

  lua["isPlayerCreated"] = Function_IsPlayerCreated;
  lua["isPlayerStreamed"] = Function_IsPlayerStreamed;
  lua["isPlayerDead"] = Function_IsPlayerDead;
  lua["isPlayerUnconscious"] = Function_IsPlayerUnconscious;
  lua["getPlayerPing"] = Function_GetPlayerPing;
  lua["getPlayerFocus"] = Function_GetPlayerFocus;
  lua["getPlayerType"] = Function_GetPlayerType;
  lua["setPlayerOnFloor"] = Function_SetPlayerOnFloor;
  lua["setPlayerCollision"] = Function_SetPlayerCollision;
  lua["getPlayerCollision"] = Function_GetPlayerCollision;

  lua["setTime"] = Function_SetTime;
  lua["getTime"] = Function_GetTime;
  lua["setDayLength"] = Function_SetDayLength;
  lua["getDayLength"] = Function_GetDayLength;

  lua["enable_DamageAnims"] = Function_Enable_DamageAnims;
  lua["enable_MunitionTrail"] = Function_Enable_MunitionTrail;
  lua["enable_WeaponTrail"] = Function_Enable_WeaponTrail;
  lua["setBloodMode"] = Function_SetBloodMode;
  lua["getBloodMode"] = Function_GetBloodMode;
  lua["setSightFactor"] = Function_SetSightFactor;
  lua["getSightFactor"] = Function_GetSightFactor;
  lua["setLODStrengthModifier"] = Function_SetLODStrengthModifier;
  lua["getLODStrengthModifier"] = Function_GetLODStrengthModifier;
  lua["setLODStrengthOverride"] = Function_SetLODStrengthOverride;
  lua["getLODStrengthOverride"] = Function_GetLODStrengthOverride;
  lua["playVideo"] = Function_PlayVideo;
  lua["fileExists"] = Function_FileExists;

  lua["getResolution"] = Function_GetResolution;
  lua["setResolution"] = Function_SetResolution;
  lua["getAvailableResolutions"] = Function_GetAvailableResolutions;
  lua["getFpsRate"] = Function_GetFpsRate;
  lua["getNetworkStats"] = Function_GetNetworkStats;
  lua["getLanguageIndex"] = Function_GetLanguageIndex;

  lua["exitGame"] = Function_ExitGame;
  lua["enableGMPMenu"] = Function_EnableGMPMenu;
  lua["openGMPMenu"] = Function_OpenGMPMenu;
  lua["closeGMPMenu"] = Function_CloseGMPMenu;

  BindInputConstants(lua);
  BindInput(lua);
  BindCursor(lua);
  BindChatInput(lua);
  BindDiscord(lua);
  BindLabel(lua);
  BindWorldLabel(lua);
  BindSprite(lua);
  BindSound(lua);
  BindSound3d(lua);
  gmp::lua::BindMusic(lua);
  BindGothicMusic(lua);
  BindVob(lua);
  BindCamera(lua);
  BindRoute(lua);
  BindSky(lua);
  BindInterface(lua);
  BindItemGround(lua);
  BindVoice(lua);

  // Constants
  lua["PLANET_SUN"] = 0;
  lua["PLANET_MOON"] = 1;

  const auto& languageManager = LanguageManager::Instance();
  lua["LANG_EN"] = languageManager.GetLanguageIndex("EN");
  lua["LANG_PL"] = languageManager.GetLanguageIndex("PL");
  lua["LANG_DE"] = languageManager.GetLanguageIndex("DE");
  lua["LANG_RU"] = languageManager.GetLanguageIndex("RU");
  lua["LANG_CZ"] = languageManager.GetLanguageIndex("CZ");
  lua["LANG_SK"] = languageManager.GetLanguageIndex("SK");
  lua["LANG_ISV"] = languageManager.GetLanguageIndex("ISV");
  lua["LANG_UA"] = languageManager.GetLanguageIndex("UA");
  lua["LANG_IT"] = languageManager.GetLanguageIndex("IT");
  lua["LANG_HU"] = languageManager.GetLanguageIndex("HU");
  lua["LANG_FR"] = languageManager.GetLanguageIndex("FR");
  lua["LANG_DA"] = languageManager.GetLanguageIndex("DA");
  lua["LANG_ES"] = languageManager.GetLanguageIndex("ES");
  lua["LANG_SZL"] = languageManager.GetLanguageIndex("SZL");
  lua["LANG_TR"] = languageManager.GetLanguageIndex("TR");
}

}  // namespace gmp::gothic

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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <spdlog/spdlog.h>

#include "ZenGin/zGothicAPI.h"
#include "discord_presence.h"
#include "process_input.h"
#include "net_game.h"
#include "patch.h"

#include "lua_draw.h"
#include "lua_texture.h"
#include "lua_sound.h"
#include "lua_cursor.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {
struct ClientNpc {
  oCNpc* npc{nullptr};
  std::string name;
  bool spawned{false};
};

std::unordered_map<int, ClientNpc> g_client_npcs;
int g_next_npc_id = -1;

oCSpawnManager* GetSpawnManager() {
  return ogame ? ogame->GetSpawnManager() : nullptr;
}

bool HasFactoryAndParser() { return zfactory && zCParser::GetParser(); }

bool HasSpawnPrerequisites() {
  return HasFactoryAndParser() && ogame && GetSpawnManager();
}

sol::optional<std::string> GetOptionalString(const sol::table& table, const char* lowerKey, const char* upperKey) {
  if (auto value = table.get<sol::optional<std::string>>(lowerKey); value) {
    return value;
  }
  return table.get<sol::optional<std::string>>(upperKey);
}

Gothic2APlayer* GetPlayerById(std::uint64_t id) {
  auto& players = NetGame::Instance().players;
  auto it = std::find_if(players.begin(), players.end(), [id](Gothic2APlayer* player) {
    return player && player->base_player().id() == id;
  });

  if (it == players.end()) {
    return nullptr;
  }

  return *it;
}

oCNpc* GetNpcById(std::uint64_t id) {
  if (auto* player = GetPlayerById(id)) {
    return player->GetNpc();
  }

  return nullptr;
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

/* luadoc (func)
*
* Set a player's instance.
*
* @name     setPlayerInstance
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (string) instance       Instance name.
* @return   (boolean)               True on success, false otherwise.
*
*/
bool Function_SetPlayerInstance(std::uint64_t id, const std::string& instance) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = zCParser::GetParser()) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);

      if (instance_id >= 0) {
        npc->InitByScript(instance_id, 0);
        return true;
      }
    }
  }
  return false;
}

/* luadoc (func)
*
* Get a player's instance name.
*
* @name     getPlayerInstance
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (string|nil)         Instance name or nil.
*
*/
sol::object Function_GetPlayerInstance(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    std::string name = npc->GetInstanceName().ToChar();
    return sol::make_object(lua, std::move(name));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's display name.
*
* @name     setPlayerName
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (string) name       New player name.
* @return   (boolean)           True on success.
*
*/
bool Function_SetPlayerName(std::uint64_t id, const std::string& name) {
    if (auto* player = GetPlayerById(id)) {
      zSTRING new_name(name.c_str());

      for (auto* other_player : NetGame::Instance().players) {
        if (!other_player || !other_player->GetNpc()) {
          continue;
        }

        if (other_player->base_player().id() != id && other_player->GetNpc()->GetName() == new_name) {
          return false;
        }
      }

      player->SetName(new_name);
      player->base_player().set_name(new_name.ToChar());
      return true;
    }

    return false;
  }

/* luadoc (func)
*
* Get a player's name or nil if unavailable.
*
* @name     getPlayerName
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (string|nil)        Player name or nil.
*
*/
sol::object Function_GetPlayerName(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    std::string name = npc->GetName().ToChar();
    return sol::make_object(lua, std::move(name));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's name color (RGB 0-255).
*
* @name     setPlayerColor
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) r          Red (0-255).
* @param    (int) g          Green (0-255).
* @param    (int) b          Blue (0-255).
*
*/
bool Function_SetPlayerColor(std::uint64_t id, int r, int g, int b) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);

    if (auto* player = GetPlayerById(id)) {
      player->SetNameColor(zCOLOR(static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                                  static_cast<unsigned char>(b), 255));
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's name color as a table { r, g, b } or nil if unavailable.
*
* @name     getPlayerColor
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   ({r, g, b}|nil)  RGB color (0-255) or nil.
*
*/
sol::object Function_GetPlayerColor(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* player = GetPlayerById(id)) {
    const auto& color = player->GetNameColor();

    sol::table tbl = lua.create_table();
    tbl["r"] = static_cast<int>(color.r);
    tbl["g"] = static_cast<int>(color.g);
    tbl["b"] = static_cast<int>(color.b);

    return sol::make_object(lua, tbl);
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's current health (clamped to [0,max]).
*
* @name     setPlayerHealth
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) health     New health value.
*
*/
bool Function_SetPlayerHealth(std::uint64_t id, int health) {
    if (health < 0) {
      health = 0;
    }

    if (auto* player = GetPlayerById(id)) {
      if (auto* npc = player->GetNpc()) {
        const int max_health = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
        const int clamped_health = std::min(health, max_health);
        npc->SetAttribute(NPC_ATR_HITPOINTS, clamped_health);
        player->base_player().set_health(static_cast<short>(clamped_health));
        return true;
      }
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's current health.
*
* @name     getPlayerHealth
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Current health or nil.
*
*/
sol::object Function_GetPlayerHealth(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_HITPOINTS));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's maximum health and clamp current health if needed.
*
* @name     setPlayerMaxHealth
* @side     client
* @category Player
* @param    (int) player_id   Target player id.
* @param    (int) max_health  New maximum health.
*
*/
bool Function_SetPlayerMaxHealth(std::uint64_t id, int max_health) {
    if (max_health < 0) {
      max_health = 0;
    }

    if (auto* player = GetPlayerById(id)) {
      if (auto* npc = player->GetNpc()) {
        npc->SetAttribute(NPC_ATR_HITPOINTSMAX, max_health);
        const int current_health = npc->GetAttribute(NPC_ATR_HITPOINTS);
        if (current_health > max_health) {
          npc->SetAttribute(NPC_ATR_HITPOINTS, max_health);
        }
        return true;
      }
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's maximum health.
*
* @name     getPlayerMaxHealth
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Max health or nil.
*
*/
sol::object Function_GetPlayerMaxHealth(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_HITPOINTSMAX));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's current mana (clamped to [0,max]).
*
* @name     setPlayerMana
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) mana       Mana value.
*
*/
bool Function_SetPlayerMana(std::uint64_t id, int mana) {
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

/* luadoc (func)
*
* Get a player's current mana.
*
* @name     getPlayerMana
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Current mana or nil.
*
*/
sol::object Function_GetPlayerMana(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_MANA));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's maximum mana and clamp current mana if needed.
*
* @name     setPlayerMaxMana
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) max_mana   New maximum mana.
*
*/
bool Function_SetPlayerMaxMana(std::uint64_t id, int max_mana) {
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

/* luadoc (func)
*
* Get a player's maximum mana.
*
* @name     getPlayerMaxMana
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Max mana or nil.
*
*/
sol::object Function_GetPlayerMaxMana(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_MANAMAX));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's strength attribute.
*
* @name     setPlayerStrength
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) strength   Strength value.
*
*/
bool Function_SetPlayerStrength(std::uint64_t id, int strength) {
    if (strength < 0) {
      strength = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      npc->SetAttribute(NPC_ATR_STRENGTH, strength);
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's strength attribute.
*
* @name     getPlayerStrength
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Strength value or nil.
*
*/
sol::object Function_GetPlayerStrength(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_STRENGTH));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's dexterity attribute.
*
* @name     setPlayerDexterity
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) dexterity  Dexterity value.
*
*/
bool Function_SetPlayerDexterity(std::uint64_t id, int dexterity) {
    if (dexterity < 0) {
      dexterity = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      npc->SetAttribute(NPC_ATR_DEXTERITY, dexterity);
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's dexterity attribute.
*
* @name     getPlayerDexterity
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)              Dexterity value or nil.
*
*/
sol::object Function_GetPlayerDexterity(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetAttribute(NPC_ATR_DEXTERITY));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's weapon skill hit chance (0-100).
*
* @name     setPlayerSkillWeapon
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) skill_id   Skill identifier.
* @param    (int) percentage Hit chance (0-100).
*
*/
bool Function_SetPlayerSkillWeapon(std::uint64_t id, int skill_id, int percentage) {
    percentage = std::clamp(percentage, 0, 100);

    if (auto* npc = GetNpcById(id)) {
      npc->SetHitChance(skill_id, percentage);
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's weapon skill hit chance.
*
* @name     getPlayerSkillWeapon
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) skill_id   Skill identifier.
* @return   (int|nil)              Hit chance (0-100) or nil.
*
*/
sol::object Function_GetPlayerSkillWeapon(std::uint64_t id, int skill_id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetHitChance(skill_id));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's talent value.
*
* @name     setPlayerTalent
* @side     client
* @category Player
* @param    (int) player_id   Target player id.
* @param    (int) talent_id   Talent identifier.
* @param    (int) talent_value Talent value.
*
*/
bool Function_SetPlayerTalent(std::uint64_t id, int talent_id, int talent_value) {
    if (auto* npc = GetNpcById(id)) {
      npc->SetTalentSkill(talent_id, talent_value);
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's talent value.
*
* @name     getPlayerTalent
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) talent_id  Talent identifier.
* @return   (int|nil)              Talent value or nil.
*
*/
sol::object Function_GetPlayerTalent(std::uint64_t id, int talent_id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->GetTalentSkill(talent_id));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's experience level.
*
* @name     setPlayerLevel
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (int) level         New level.
*
*/
bool Function_SetPlayerLevel(std::uint64_t id, int level) {
    if (auto* npc = GetNpcById(id)) {
      npc->level = static_cast<int>(std::max(level, 0));
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's level or 0 if unavailable.
*
* @name     getPlayerLevel
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (int|nil)        Level or nil.
*
*/
sol::object Function_GetPlayerLevel(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, static_cast<int>(npc->level));
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set the player's experience points.
*
* @name     setExp
* @side     client
* @category Hero
* @param    (int) exp           New exp value.
*
*/
void Function_SetExp(int exp) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_exp = static_cast<unsigned long>(std::max(exp, 0));
      npc->experience_points = clamped_exp;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetExperience(clamped_exp, 0, npc->experience_points_next_level);
      }
    }
  }
}

/* luadoc (func)
*
* Get the player's experience points.
*
* @name     getExp
* @side     client
* @category Hero
* @return   (int|nil)        Exp value or nil.
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

/* luadoc (func)
*
* Set the experience required for the player's next level.
*
* @name     setNextLevelExp
* @side     client
* @category Hero
* @param    (int) next_level_exp    Required exp for next level.
*
*/
void Function_SetNextLevelExp(int next_level_exp) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_next_level = static_cast<unsigned long>(std::max(next_level_exp, 0));
      npc->experience_points_next_level = clamped_next_level;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetExperience(npc->experience_points, 0, clamped_next_level);
      }
    }
  }
}
/* luadoc (func)
*
* Get the experience required for the player's next level.
*
* @name     getNextLevelExp
* @side     client
* @category Hero
* @return   (int|nil)        Next level exp or nil.
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

/* luadoc (func)
*
* Set the player's learn points.
*
* @name     setLearnPoints
* @side     client
* @category Hero
* @param    (int) learn_points   New learn points value.
*
*/
void Function_SetLearnPoints(int learn_points) {
  if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
    if (auto* npc = player->GetNpc()) {
      const unsigned long clamped_points = static_cast<unsigned long>(std::max(learn_points, 0));
      npc->learn_points = clamped_points;

      if (auto* status_menu = GetStatusMenu()) {
        status_menu->SetLearnPoints(clamped_points);
      }
    }
  }
}

/* luadoc (func)
*
* Get the player's learn points.
*
* @name     getLearnPoints
* @side     client
* @category Hero
* @return   (int|nil)        Learn points or nil.
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

/* luadoc (func)
*
* Set a player's visual model and textures.
*
* @name     setPlayerVisual
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) body_model   Body model name.
* @param    (int) body_texture    Body texture index.
* @param    (string) head_model   Head model name.
* @param    (int) head_texture    Head texture index.
*
*/
bool Function_SetPlayerVisual(std::uint64_t id, const std::string& body_model, int body_texture, const std::string& head_model, int head_texture,
         sol::optional<int> teeth_texture, sol::optional<int> skin_color) {
        if (auto* npc = GetNpcById(id); npc) {
          zSTRING body(body_model.c_str());
          zSTRING head(head_model.c_str());
          const int color_variant = skin_color.value_or(0);
          const int teeth_variant = teeth_texture.value_or(0);
          npc->SetAdditionalVisuals(body, body_texture, color_variant, head, head_texture, teeth_variant, color_variant);
          return true;
        }
        return false;
      }

sol::object Function_GetPlayerVisual(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id); npc) {
    sol::table tbl = lua.create_table();

    tbl["body_model"] = std::string(npc->body_visualName.ToChar());
    tbl["body_texture"] = static_cast<int>(npc->body_TexVarNr);
    tbl["head_model"] = std::string(npc->head_visualName.ToChar());
    tbl["head_texture"] = static_cast<int>(npc->head_TexVarNr);
    return sol::make_object(lua, tbl);
  }
  return sol::nil;
}

/* luadoc (func)
*
* Set a player's model fatness.
*
* @name     setPlayerFatness
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @param    (float) fatness   Fatness value.
*
*/
bool Function_SetPlayerFatness(std::uint64_t id, float fatness) {
    if (auto* npc = GetNpcById(id); npc) {
      npc->SetFatness(fatness);
      return true;
    }
    return false;
  }

sol::object Function_GetPlayerFatness(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    return sol::make_object(lua, npc->model_fatness);
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's model scale.
*
* @name     setPlayerScale
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @param    (float) x   Scale factor on x axis.
* @param    (float) y   Scale factor on y axis.
* @param    (float) z   Scale factor on z axis.
*
*/
bool Function_SetPlayerScale(std::uint64_t id, float x, float y, float z) {
    if (auto* npc = GetNpcById(id); npc) {
      npc->SetModelScale(zVEC3{x, y, z});
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's model scale.
*
* @name     getPlayerScale
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @return    ({x, y, z}|nil)   Player scale or nil.
*
*/
sol::object Function_GetPlayerScale(std::uint64_t id, sol::this_state ts){
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id); npc) {
    const zVEC3 scale = npc->model_scale;
    sol::table tbl = lua.create_table();
    tbl["x"] = scale[VX];
    tbl["y"] = scale[VY];
    tbl["z"] = scale[VZ];
    return sol::make_object(lua, tbl);
  }

  return sol::nil;
}

/* luadoc (func)
*
* Apply animation overlay on player.
*
* @name     applyPlayerOverlay
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) overlay   The name of overlay.
*
*/
bool Function_ApplyPlayerOverlay(std::uint64_t id, const std::string& overlay) {
    if (auto* npc = GetNpcById(id); npc) {
      zSTRING overlay_name(overlay.c_str());
      return npc->ApplyOverlay(overlay_name) != 0;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's active animation overlays.
*
* @name     getPlayerOverlays
* @side     client
* @category Player
* @param    (int) player_id
* @return   ({...}|nil)   Array of overlay names or nil.
*
*/
sol::object Function_GetPlayerOverlays(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zCArray<zSTRING>& overlays = npc->activeOverlays;

    if (overlays.GetNum() == 0) {
      return sol::nil; // or return empty table if you prefer
    }

    sol::table tbl = lua.create_table(overlays.GetNum(), 0);

    for (int i = 0; i < overlays.GetNum(); ++i) {
      tbl[i + 1] = overlays[i].ToChar();
    }

    return tbl;
  }

  return sol::nil;
}

/* luadoc (func)
*
* Remove animation overlay on player.
*
* @name     removePlayerOverlay
* @side     client
* @category Player
* @param    (int) player_id    Target player id.
* @param    (string) overlay   The name of overlay.
*
*/
bool Function_RemovePlayerOverlay(std::uint64_t id, const std::string& overlay) {
    if (auto* npc = GetNpcById(id); npc) {
      zSTRING overlay_name(overlay.c_str());
      const bool has_overlay = npc->GetOverlay(overlay_name) != 0;
      if (has_overlay) {
        npc->RemoveOverlay(overlay_name);
      }
      return has_overlay;
    }
    return false;
  }

/* luadoc (func)
*
* Set a player's world position.
*
* @name     setPlayerPosition
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (float) x       X coordinate.
* @param    (float) y       Y coordinate.
* @param    (float) z       Z coordinate.
*
*/
bool Function_SetPlayerPosition(std::uint64_t id, float x, float y, float z) {
    if (auto* player = GetPlayerById(id)) {
      if (auto* npc = player->GetNpc()) {
        zVEC3 position{x, y, z};
        npc->SetPositionWorld(position);
        player->SetPosition(position);
        player->base_player().set_position(x, y, z);
        return true;
      }
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's world position as tuple (x,y,z).
*
* @name     getPlayerPosition
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   ({x,y,z}|nil)  Table with keys `x`,`y`,`z` or nil.
*
*/
sol::object Function_GetPlayerPosition(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zVEC3 position = npc->GetPositionWorld();
    sol::table tbl = lua.create_table();
    tbl["x"] = position[VX];
    tbl["y"] = position[VY];
    tbl["z"] = position[VZ];
    return sol::make_object(lua, tbl);
  }

  return sol::nil;
}

/* luadoc (func)
*
* Set a player's facing angle (radians) in world space.
*
* @name     setPlayerAngle
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @param    (float) angle    Angle in radians.
* @param    (boolean|nil) interpolate Optional interpolation flag.
*
*/
bool Function_SetPlayerAngle(std::uint64_t id, float angle, sol::optional<bool> /*interpolate*/) {
    if (auto* npc = GetNpcById(id)) {
      const float radians = angle;
      const zVEC3 heading_vector(std::sin(radians), 0.0F, std::cos(radians));
      npc->SetHeadingYWorld(heading_vector);
      return true;
    }
    return false;
  }

/* luadoc (func)
*
* Get a player's facing angle (radians) in world space.
*
* @name     getPlayerAngle
* @side     client
* @category Player
* @param    (int) player_id  Target player id.
* @return   (number|nil)           Angle in radians or nil.
*
*/
sol::object Function_GetPlayerAngle(std::uint64_t id, sol::this_state ts) {
  sol::state_view lua(ts);

  if (auto* npc = GetNpcById(id)) {
    const zVEC3 forward = npc->GetAtVectorWorld();
    return sol::make_object(lua, std::atan2(forward[VX], forward[VZ]));
  }

  return sol::nil;
}
/* luadoc (func)
*
* Give an item to a player or NPC on the client.
*
* @name     giveItem
* @side     client
* @category Inventory
* @param    (int) player_id  Target player id.
* @param    (string) instance       Item instance name.
* @param    (int) amount            Amount to give.
* @return   (boolean)               True on success.
*
*/
bool Function_GiveItem(std::uint64_t id, const std::string& instance, std::int32_t amount) {
  if (amount <= 0) {
    return false;
  }

  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = zCParser::GetParser()) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        npc->CreateItems(instance_id, amount);
        return true;
      }
    }
  }

  return false;
}

/* luadoc (func)
*
* Equip an item for a player or NPC on the client.
*
* @name     equipItem
* @side     client
* @category Inventory
* @param    (int) player_id  Target player id.
* @param    (string) instance       Item instance name.
* @param    (int) slot_id           Optional slot id, ignored on client.
* @return   (boolean)               True on success.
*
*/
bool Function_EquipItem(std::uint64_t id, const std::string& instance, sol::optional<int> slot_id) {
  (void)slot_id;

  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = zCParser::GetParser()) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        npc->CreateItems(instance_id, 1);
        if (auto* item = npc->inventory2.IsIn(instance_id, 1)) {
          npc->EquipItem(item);
          return true;
        }
      }
    }
  }

  return false;
}

/* luadoc (func)
*
* Unequip an item from a player or NPC on the client.
*
* @name     unequipItem
* @side     client
* @category Inventory
* @param    (int) player_id  Target player id.
* @param    (string) instance       Item instance name.
* @return   (boolean)               True on success.
*
*/
bool Function_UnequipItem(std::uint64_t id, const std::string& instance) {
  if (auto* npc = GetNpcById(id)) {
    if (auto* parser = zCParser::GetParser()) {
      zSTRING instance_name(instance.c_str());
      const int instance_id = parser->GetIndex(instance_name);
      if (instance_id >= 0) {
        if (auto* item = npc->inventory2.IsIn(instance_id, 1)) {
          npc->UnequipItem(item);
          return true;
        }
      }
    }
  }

  return false;
}

/* luadoc (func)
*
* Change the current game world.
*
* @name     changeWorld
* @side     client
* @category World
* @param    (string) world             World filename.
* @param    (string|nil) start_point   Optional start point name.
*
*/
bool Function_ChangeWorld(const std::string& world, sol::optional<std::string> start_point) {
  zSTRING z_world(world.c_str());
  zSTRING z_start_point = start_point ? zSTRING(start_point->c_str()) : zSTRING("");
    Patch::ChangeLevelEnabled(true);
    ogame->ChangeLevel(z_world, z_start_point);
    Patch::ChangeLevelEnabled(false);
	return 0;
}

/* luadoc (func)
*
* Get the current game world filename.
*
* @name     getWorld
* @side     client
* @category World
* @return   (string)           World filename.
*
*/
sol::object Function_GetWorld(sol::this_state ts) {
  sol::state_view lua(ts);
  return sol::make_object(lua, std::string(ogame->GetGameWorld()->GetWorldFilename().ToChar()));
}

/* luadoc (func)
*
* Create a client-side NPC entry and return an internal npc id (<0).
*
* @name     createNpc
* @side     client
* @category NPC
* @param    (string) name  Name for the created NPC.
* @return   (int)            Internal npc id (negative) or 0 on failure.
*
*/
int Function_CreateNpc(const std::string& name) {
    if (!HasFactoryAndParser()) {
      SPDLOG_WARN("createNpc: missing game engine components");
      return 0;
    }

    auto* parser = zCParser::GetParser();
    const int default_instance = parser->GetIndex("PC_HERO");
    if (default_instance < 0) {
      SPDLOG_WARN("createNpc: failed to resolve default instance 'PC_HERO'");
      return 0;
    }

    oCNpc* npc = zfactory->CreateNpc(default_instance);
    if (!npc) {
      SPDLOG_WARN("createNpc: failed to allocate npc");
      return 0;
    }

    npc->name[0] = name.c_str();

    const int npc_id = g_next_npc_id--;
    g_client_npcs.emplace(npc_id, ClientNpc{npc, name});
    return npc_id;
  }

/* luadoc (func)
*
* Destroy a client-side NPC previously created with `createNpc`.
*
* @name     destroyNpc
* @side     client
* @category NPC
* @param    (int) npc_id  Internal npc id returned by `createNpc`.
* @return   (boolean)       True on success.
*
*/
bool Function_DestroyNpc(int npc_id) {
    auto it = g_client_npcs.find(npc_id);
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

/* luadoc (func)
*
* Spawn a previously created NPC into the world using an optional instance.
*
* @name     spawnNpc
* @side     client
* @category NPC
* @param    (int) npc_id             Internal npc id.
* @param    (string|nil) instance_name Optional instance name (e.g., "PC_HERO").
* @return   (boolean)                   True if spawn attached to world.
*
*/
bool Function_SpawnNpc(int npc_id, sol::optional<std::string> instance_name) {
    auto it = g_client_npcs.find(npc_id);
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
    auto* parser = zCParser::GetParser();
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

/* luadoc (func)
*
* Unspawn (remove) a client NPC from the world without destroying it.
*
* @name     unspawnNpc
* @side     client
* @category NPC
* @param    (int) npc_id  Internal npc id.
* @return   (boolean)       True on success.
*
*/
bool Function_UnspawnNpc(int npc_id) {
    auto it = g_client_npcs.find(npc_id);
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

void BindDiscord(sol::state& lua) {
/* luadoc (class)
*
* This class exposes static methods for updating the user's Discord activity from the game client.
*
* @name     Discord
* @side     client
* @category Discord
*
*/
  auto discord = lua.create_table("Discord");

/* luadoc (method)
*
* Updates the Discord Rich Presence activity. 
* 
* All parameters are optional and should be passed via a table. Missing fields default to empty values.
*
* @name     SetActivity
* @static
* @param    (table) params Activity configuration table.
* @param    (string) params.state Text shown as the activity state.
* @param    (string) params.details Text shown as activity details.
* @param    (string) params.largeImageKey Key of the large image asset.
* @param    (string) params.largeImageText Tooltip text for the large image.
* @param    (string) params.smallImageKey Key of the small image asset.
* @param    (string) params.smallImageText Tooltip text for the small image.
*
*/
  discord.set_function("SetActivity", [](const sol::table& params) {
    auto state = GetOptionalString(params, "state", "State").value_or("");
    auto details = GetOptionalString(params, "details", "Details").value_or("");
    auto large_image_key = GetOptionalString(params, "largeImageKey", "LargeImageKey").value_or("");
    auto large_image_text = GetOptionalString(params, "largeImageText", "LargeImageText").value_or("");
    auto small_image_key = GetOptionalString(params, "smallImageKey", "SmallImageKey").value_or("");
    auto small_image_text = GetOptionalString(params, "smallImageText", "SmallImageText").value_or("");

    DiscordRichPresence::Instance().UpdateActivity(state, details, 0, 0, large_image_key, large_image_text, 
                                                    small_image_key, small_image_text);
  });
}

void BindDraw(sol::state& lua) {
  sol::usertype<LuaDraw> draw_type = lua.new_usertype<LuaDraw>(
      "Draw",
      sol::constructors<LuaDraw(), LuaDraw(int, int, const std::string&)>());

  draw_type[sol::meta_function::call] = sol::overload(
      []() { return LuaDraw(); },
      [](int x, int y, const std::string& text) { return LuaDraw(x, y, text); });

  draw_type["setPosition"] = &LuaDraw::setPosition;
  draw_type["getPosition"] = &LuaDraw::getPosition;
  draw_type["setPositionPx"] = &LuaDraw::setPositionPx;
  draw_type["getPositionPx"] = &LuaDraw::getPositionPx;

  draw_type["setText"] = &LuaDraw::setText;
  draw_type["getText"] = &LuaDraw::getText;

  draw_type["setFont"] = &LuaDraw::setFont;
  draw_type["getFont"] = &LuaDraw::getFont;

  draw_type["setColor"] = &LuaDraw::setColor;
  draw_type["getColor"] = &LuaDraw::getColor;

  draw_type["setAlpha"] = &LuaDraw::setAlpha;
  draw_type["getAlpha"] = &LuaDraw::getAlpha;

  draw_type["setVisible"] = &LuaDraw::setVisible;
  draw_type["getVisible"] = &LuaDraw::getVisible;

  draw_type["render"] = &LuaDraw::render;

  // Properties (Lua table access)
  draw_type["position"] = sol::property(&LuaDraw::getPosition, &LuaDraw::setPosition);
  draw_type["positionPx"] = sol::property(&LuaDraw::getPositionPx, &LuaDraw::setPositionPx);
  draw_type["text"] = sol::property(&LuaDraw::getText, &LuaDraw::setText);
  draw_type["font"] = sol::property(&LuaDraw::getFont, &LuaDraw::setFont);
  draw_type["color"] = sol::property(&LuaDraw::getColor);
  draw_type["alpha"] = sol::property(&LuaDraw::getAlpha, &LuaDraw::setAlpha);
  draw_type["visible"] = sol::property(&LuaDraw::getVisible, &LuaDraw::setVisible);
}

void BindTexture(sol::state& lua) {
  sol::usertype<LuaTexture> texture_type = lua.new_usertype<LuaTexture>(
      "Texture",
      sol::constructors<LuaTexture(int, int, int, int, const std::string&)>());

  texture_type[sol::meta_function::call] =
      [](int x, int y, int width, int height, const std::string& file) {
        return LuaTexture(x, y, width, height, file);
      };

  texture_type["setPosition"] = &LuaTexture::setPosition;
  texture_type["getPosition"] = &LuaTexture::getPosition;
  texture_type["setPositionPx"] = &LuaTexture::setPositionPx;
  texture_type["getPositionPx"] = &LuaTexture::getPositionPx;

  texture_type["setSize"] = &LuaTexture::setSize;
  texture_type["getSize"] = &LuaTexture::getSize;
  texture_type["setSizePx"] = &LuaTexture::setSizePx;
  texture_type["getSizePx"] = &LuaTexture::getSizePx;

  texture_type["setRect"] = &LuaTexture::setRect;
  texture_type["getRect"] = &LuaTexture::getRect;
  texture_type["setRectPx"] = &LuaTexture::setRectPx;
  texture_type["getRectPx"] = &LuaTexture::getRectPx;

  texture_type["setColor"] = &LuaTexture::setColor;
  texture_type["getColor"] = &LuaTexture::getColor;
  texture_type["setAlpha"] = &LuaTexture::setAlpha;
  texture_type["getAlpha"] = &LuaTexture::getAlpha;

  texture_type["setVisible"] = &LuaTexture::setVisible;
  texture_type["getVisible"] = &LuaTexture::getVisible;
  texture_type["setFile"] = &LuaTexture::setFile;
  texture_type["getFile"] = &LuaTexture::getFile;
  
  texture_type["top"] = &LuaTexture::top;

  texture_type["render"] = &LuaTexture::render;

  // Properties (Lua table access)
  texture_type["position"] = sol::property(&LuaTexture::getPosition, &LuaTexture::setPosition);
  texture_type["positionPx"] = sol::property(&LuaTexture::getPositionPx, &LuaTexture::setPositionPx);
  texture_type["size"] = sol::property(&LuaTexture::getSize, &LuaTexture::setSize);
  texture_type["sizePx"] = sol::property(&LuaTexture::getSizePx, &LuaTexture::setSizePx);
  texture_type["rect"] = sol::property(&LuaTexture::getRect, &LuaTexture::setRect);
  texture_type["rectPx"] = sol::property(&LuaTexture::getRectPx, &LuaTexture::setRectPx);
  texture_type["color"] = sol::property(&LuaTexture::getColor, &LuaTexture::setColor);
  texture_type["alpha"] = sol::property(&LuaTexture::getAlpha, &LuaTexture::setAlpha);
  texture_type["visible"] = sol::property(&LuaTexture::getVisible, &LuaTexture::setVisible);
  texture_type["file"] = sol::property(&LuaTexture::getFile, &LuaTexture::setFile);
}

void BindSound(sol::state& lua) {
  sol::usertype<LuaSound> sound_type = lua.new_usertype<LuaSound>(
      "Sound",
      sol::constructors<LuaSound(const std::string&)>());

  sound_type["play"] = &LuaSound::play;
  sound_type["stop"] = &LuaSound::stop;
  sound_type["isPlaying"] = &LuaSound::isPlaying;

  sound_type["file"] = sol::property(&LuaSound::getFile, &LuaSound::setFile);
  sound_type["playingTime"] = sol::property(&LuaSound::getPlayingTime);
  sound_type["volume"] = sol::property(&LuaSound::getVolume, &LuaSound::setVolume);
  sound_type["looping"] = sol::property(&LuaSound::getLooping, &LuaSound::setLooping);
  sound_type["balance"] = sol::property(&LuaSound::getBalance, &LuaSound::setBalance);
}


/* luadoc (func)
*
* Set the in-game world time (hour, minute).
*
* @name     setTime
* @side     client
* @category Time
* @param    (int) hour    Hour component.
* @param    (int) minute  Minute component.
*
*/
void Function_SetTime(int hour, int minute) {
  if (!ogame || !ogame->GetWorldTimer()) {
    return;
  }

  ogame->GetWorldTimer()->SetTime(hour, minute);
}

/* luadoc (func)
*
* Get the in-game world time.
*
* @name     getTime
* @side     client
* @category Time
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
  tbl["hour"]   = hour;
  tbl["minute"] = minute;
  return sol::make_object(lua, tbl);
}

/* luadoc (func)
*
* Set the duration of an in-game day in seconds.
*
* @name     setDayLength
* @side     client
* @category Time
* @param    (float) day_length_seconds  Length of day in seconds.
*
*/
void Function_SetDayLength(float day_length_seconds) {
  //
}

/* luadoc (func)
*
* Get the configured in-game day length in seconds.
*
* @name     getDayLength
* @side     client
* @category Time
* @return   (float)  Day length in seconds.
*
*/
sol::object Function_GetDayLength() {
  return sol::nil;
}

void BindGothicSpecific(sol::state& lua) {
  SPDLOG_TRACE("Initializing Gothic 2 Addon 2.6 specific bindings...");

  lua["setPlayerInstance"] = Function_SetPlayerInstance;
  lua["getPlayerInstance"] = Function_GetPlayerInstance;
  lua["setPlayerName"] = Function_SetPlayerName;
  lua["getPlayerName"] = Function_GetPlayerName;
  lua["setPlayerColor"] = Function_SetPlayerColor;
  lua["getPlayerColor"] = Function_GetPlayerColor;
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
  lua["setPlayerFatness"] = Function_SetNextLevelExp;
  lua["getNextLevelExp"] = Function_GetNextLevelExp;
  lua["setPlayerVisual"] = Function_SetPlayerVisual;
  lua["getPlayerVisual"] = Function_SetPlayerVisual;
  lua["setPlayerScale"] = Function_SetPlayerScale;
  lua["getPlayerScale"] = Function_GetPlayerScale;
  lua["applyPlayerOverlay"] = Function_ApplyPlayerOverlay;
  lua["getPlayerOverlays"] = Function_GetPlayerOverlays;
  lua["removePlayerOverlay"] = Function_RemovePlayerOverlay;
  lua["setPlayerPosition"] = Function_SetPlayerPosition;
  lua["getPlayerPosition"] = Function_GetPlayerPosition;
  lua["setPlayerAngle"] = Function_SetPlayerAngle;
  lua["getPlayerAngle"] = Function_GetPlayerAngle;

  lua["giveItem"] = Function_GiveItem;
  lua["equipItem"] = Function_EquipItem;
  lua["unequipItem"] = Function_UnequipItem;
  
  lua["changeWorld"] = Function_ChangeWorld;
  lua["getWorld"] = Function_GetWorld;

  lua["createNpc"] = Function_CreateNpc;
  lua["destroyNpc"] = Function_DestroyNpc;
  lua["spawnNpc"] = Function_SpawnNpc;
  lua["unspawnNpc"] = Function_UnspawnNpc;

  BindInputConstants(lua);
  BindCursor(lua);
  BindDiscord(lua);
  BindDraw(lua);
  BindTexture(lua);
  BindSound(lua);

  lua["setTime"] = Function_SetTime;
  lua["getTime"] = Function_GetTime;
  lua["setDayLength"] = Function_SetDayLength;
  lua["getDayLength"] = Function_GetDayLength;
}

void CleanupGothicViews() {
  LuaDraw::CleanupViews();
  LuaTexture::CleanupViews();
  LuaCursor::CleanupViews();
}

}  // namespace gmp::gothic

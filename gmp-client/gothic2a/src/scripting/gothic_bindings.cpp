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
#include "client_resources/process_input.h"
#include "net_game.h"
#include "patch.h"

#include "lua_draw.h"
#include "lua_texture.h"
#include "lua_sound.h"

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

void BindPlayers(sol::state& lua) {
  lua.set_function("setPlayerInstance", [](std::uint64_t id, const std::string& instance) {
    if (auto* npc = GetNpcById(id)) {
      if (auto* parser = zCParser::GetParser()) {
        zSTRING instance_name(instance.c_str());
        const int instance_id = parser->GetIndex(instance_name);

        if (instance_id >= 0) {
          npc->InitByScript(instance_id, 0);
        }
      }
    }
  });

  lua.set_function("getPlayerInstance", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return std::string(npc->GetInstanceName().ToChar());
    }

    return std::string();
  });

  lua.set_function("setPlayerName", [](std::uint64_t id, const std::string& name) {
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
  });

  lua.set_function("getPlayerName", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return std::string(npc->GetName().ToChar());
    }

    return std::string();
  });

  lua.set_function("setPlayerColor", [](std::uint64_t id, int r, int g, int b) {
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);

    if (auto* player = GetPlayerById(id)) {
      player->SetNameColor(zCOLOR(static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                                  static_cast<unsigned char>(b), 255));
    }
  });

  lua.set_function("getPlayerColor", [](std::uint64_t id) {
    if (auto* player = GetPlayerById(id)) {
      const auto& color = player->GetNameColor();
      return std::make_tuple(static_cast<int>(color.r), static_cast<int>(color.g), static_cast<int>(color.b));
    }

    return std::make_tuple(0, 0, 0);
  });

  lua.set_function("setPlayerHealth", [](std::uint64_t id, int health) {
    if (health < 0) {
      health = 0;
    }

    if (auto* player = GetPlayerById(id)) {
      if (auto* npc = player->GetNpc()) {
        const int max_health = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
        const int clamped_health = std::min(health, max_health);
        npc->SetAttribute(NPC_ATR_HITPOINTS, clamped_health);
        player->base_player().set_hp(static_cast<short>(clamped_health));
      }
    }
  });

  lua.set_function("getPlayerHealth", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_HITPOINTS);
    }

    return 0;
  });

  lua.set_function("setPlayerMaxHealth", [](std::uint64_t id, int max_health) {
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
      }
    }
  });

  lua.set_function("getPlayerMaxHealth", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
    }

    return 0;
  });

  lua.set_function("setPlayerMana", [](std::uint64_t id, int mana) {
    if (mana < 0) {
      mana = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      const int max_mana = npc->GetAttribute(NPC_ATR_MANAMAX);
      const int clamped_mana = std::min(mana, max_mana);
      npc->SetAttribute(NPC_ATR_MANA, clamped_mana);
    }
  });

  lua.set_function("getPlayerMana", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_MANA);
    }

    return 0;
  });

  lua.set_function("setPlayerMaxMana", [](std::uint64_t id, int max_mana) {
    if (max_mana < 0) {
      max_mana = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      npc->SetAttribute(NPC_ATR_MANAMAX, max_mana);
      const int current_mana = npc->GetAttribute(NPC_ATR_MANA);
      if (current_mana > max_mana) {
        npc->SetAttribute(NPC_ATR_MANA, max_mana);
      }
    }
  });

  lua.set_function("getPlayerMaxMana", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_MANAMAX);
    }

    return 0;
  });

  lua.set_function("setPlayerStrength", [](std::uint64_t id, int strength) {
    if (strength < 0) {
      strength = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      npc->SetAttribute(NPC_ATR_STRENGTH, strength);
    }
  });

  lua.set_function("getPlayerStrength", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_STRENGTH);
    }

    return 0;
  });

  lua.set_function("setPlayerDexterity", [](std::uint64_t id, int dexterity) {
    if (dexterity < 0) {
      dexterity = 0;
    }

    if (auto* npc = GetNpcById(id)) {
      npc->SetAttribute(NPC_ATR_DEXTERITY, dexterity);
    }
  });

  lua.set_function("getPlayerDexterity", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetAttribute(NPC_ATR_DEXTERITY);
    }

    return 0;
  });

  lua.set_function("setPlayerSkillWeapon", [](std::uint64_t id, int skill_id, int percentage) {
    percentage = std::clamp(percentage, 0, 100);

    if (auto* npc = GetNpcById(id)) {
      npc->SetHitChance(skill_id, percentage);
    }
  });

  lua.set_function("getPlayerSkillWeapon", [](std::uint64_t id, int skill_id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetHitChance(skill_id);
    }

    return 0;
  });

  lua.set_function("setPlayerTalent", [](std::uint64_t id, int talent_id, int talent_value) {
    if (auto* npc = GetNpcById(id)) {
      npc->SetTalentSkill(talent_id, talent_value);
    }
  });

  lua.set_function("getPlayerTalent", [](std::uint64_t id, int talent_id) {
    if (auto* npc = GetNpcById(id)) {
      return npc->GetTalentSkill(talent_id);
    }

    return 0;
  });

  lua.set_function("setPlayerWorld", [](std::uint64_t id, const std::string& world, sol::optional<std::string> start_point) {
    if (auto* player = GetPlayerById(id)) {
      if (!player->IsLocalPlayer() || !ogame) {
        return;
      }

      zSTRING z_world(world.c_str());
      zSTRING z_start_point = start_point ? zSTRING(start_point->c_str()) : zSTRING("");
      Patch::ChangeLevelEnabled(true);
      ogame->ChangeLevel(z_world, z_start_point);
      Patch::ChangeLevelEnabled(false);
    }
  });

  lua.set_function("getPlayerWorld", [](std::uint64_t id) {
    if (auto* player = GetPlayerById(id)) {
      if (player->IsLocalPlayer() && ogame && ogame->GetGameWorld()) {
        return std::string(ogame->GetGameWorld()->GetWorldFilename().ToChar());
      }
    }

    return std::string();
  });

  lua.set_function("setPlayerPosition", [](std::uint64_t id, float x, float y, float z) {
    if (auto* player = GetPlayerById(id)) {
      if (auto* npc = player->GetNpc()) {
        zVEC3 position{x, y, z};
        npc->SetPositionWorld(position);
        player->SetPosition(position);
        player->base_player().set_position(x, y, z);
      }
    }
  });

  lua.set_function("getPlayerPosition", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      const zVEC3 position = npc->GetPositionWorld();
      return std::make_tuple(position[VX], position[VY], position[VZ]);
    }

    return std::make_tuple(0.0F, 0.0F, 0.0F);
  });

  lua.set_function("setPlayerAngle", [](std::uint64_t id, float angle, sol::optional<bool> /*interpolate*/) {
    if (auto* npc = GetNpcById(id)) {
      const float radians = angle;
      const zVEC3 heading_vector(std::sin(radians), 0.0F, std::cos(radians));
      npc->SetHeadingYWorld(heading_vector);
    }
  });

  lua.set_function("getPlayerAngle", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      const zVEC3 forward = npc->GetAtVectorWorld();
      return std::atan2(forward[VX], forward[VZ]);
    }

    return 0.0F;
  });

  lua.set_function(
      "setPlayerVisual",
      [](std::uint64_t id, const std::string& body_model, int body_texture, const std::string& head_model, int head_texture,
         sol::optional<int> teeth_texture, sol::optional<int> skin_color) {
        if (auto* npc = GetNpcById(id); npc) {
          zSTRING body(body_model.c_str());
          zSTRING head(head_model.c_str());
          const int color_variant = skin_color.value_or(0);
          const int teeth_variant = teeth_texture.value_or(0);
          npc->SetAdditionalVisuals(body, body_texture, color_variant, head, head_texture, teeth_variant, color_variant);
        }
      });

  lua.set_function("setPlayerFatness", [](std::uint64_t id, float fatness) {
    if (auto* npc = GetNpcById(id); npc) {
      npc->SetFatness(fatness);
    }
  });

  lua.set_function("setPlayerScale", [](std::uint64_t id, float x, float y, float z) {
    if (auto* npc = GetNpcById(id); npc) {
      npc->SetModelScale(zVEC3{x, y, z});
    }
  });

  lua.set_function("applyPlayerOverlay", [](std::uint64_t id, const std::string& overlay) {
    if (auto* npc = GetNpcById(id); npc) {
      zSTRING overlay_name(overlay.c_str());
      return npc->ApplyOverlay(overlay_name) != 0;
    }
    return false;
  });

  lua.set_function("removePlayerOverlay", [](std::uint64_t id, const std::string& overlay) {
    if (auto* npc = GetNpcById(id); npc) {
      zSTRING overlay_name(overlay.c_str());
      const bool has_overlay = npc->GetOverlay(overlay_name) != 0;
      if (has_overlay) {
        npc->RemoveOverlay(overlay_name);
      }
      return has_overlay;
    }
    return false;
  });

  lua.set_function("setPlayerLevel", [](std::uint64_t id, int level) {
    if (auto* npc = GetNpcById(id)) {
      npc->level = static_cast<int>(std::max(level, 0));
    }
  });

  lua.set_function("getPlayerLevel", [](std::uint64_t id) {
    if (auto* npc = GetNpcById(id)) {
      return static_cast<int>(npc->level);
    }

    return 0;
  });

  lua.set_function("setLearnPoints", [](int learn_points) {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        const unsigned long clamped_points = static_cast<unsigned long>(std::max(learn_points, 0));
        npc->learn_points = clamped_points;

        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetLearnPoints(clamped_points);
        }
      }
    }
  });

  lua.set_function("getLearnPoints", []() {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        return static_cast<int>(npc->learn_points);
      }
    }

    return 0;
  });

  lua.set_function("setExp", [](int exp) {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        const unsigned long clamped_exp = static_cast<unsigned long>(std::max(exp, 0));
        npc->experience_points = clamped_exp;

        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetExperience(clamped_exp, 0, npc->experience_points_next_level);
        }
      }
    }
  });

  lua.set_function("getExp", []() {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        return static_cast<int>(npc->experience_points);
      }
    }

    return 0;
  });

  lua.set_function("setNextLevelExp", [](int next_level_exp) {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        const unsigned long clamped_next_level = static_cast<unsigned long>(std::max(next_level_exp, 0));
        npc->experience_points_next_level = clamped_next_level;

        if (auto* status_menu = GetStatusMenu()) {
          status_menu->SetExperience(npc->experience_points, 0, clamped_next_level);
        }
      }
    }
  });

  lua.set_function("getNextLevelExp", []() {
    if (auto* player = Gothic2APlayer::GetLocalPlayer()) {
      if (auto* npc = player->GetNpc()) {
        return static_cast<int>(npc->experience_points_next_level);
      }
    }

    return 0;
  });
}

void BindNpc(sol::state& lua) {
  lua.set_function("createNpc", [](const std::string& name) {
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
  });

  lua.set_function("destroyNpc", [](int npc_id) {
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
  });

  lua.set_function("spawnNpc", [](int npc_id, sol::optional<std::string> instance_name) {
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
  });

  lua.set_function("unspawnNpc", [](int npc_id) {
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
  });
}

void BindInput(sol::state& lua) {
    // KEY_* constants
#define BIND_KEY(name) lua[#name] = name;
    G2_KEY_LIST(BIND_KEY)
#undef BIND_KEY

    // MOUSE_* constants
#define BIND_MOUSE(name) lua[#name] = name;
    G2_MOUSE_LIST(BIND_MOUSE)
#undef BIND_MOUSE

    // GAME_* constants (logical actions, separate set)
#define BIND_GAME(name) lua[#name] = name;
    G2_GAME_LIST(BIND_GAME)
#undef BIND_GAME


  lua.set_function("KeyPressed", [](int key) -> bool {
      if (key < 0 || key > MAX_KEYS_AND_CODES) return false;
      return s_pressedThisFrame[key];
  });

  lua.set_function("KeyToggled", [](int key) -> bool {
      if (key < 0 || key > MAX_KEYS_AND_CODES) return false;
      return s_toggledThisFrame[key];
  });
}

void BindDiscord(sol::state& lua) {
  auto discord = lua.create_table("Discord");
  
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

void BindTime(sol::state& lua) {
  lua.set_function("setTime", [](int hour, int minute) {
    if (!ogame || !ogame->GetWorldTimer()) {
      return;
    }

    ogame->GetWorldTimer()->SetTime(hour, minute);
  });

  lua.set_function("getTime", []() {
    int hour = 0;
    int minute = 0;

    if (ogame && ogame->GetWorldTimer()) {
      ogame->GetWorldTimer()->GetTime(hour, minute);
    }

    return std::make_tuple(hour, minute);
  });

  lua.set_function("setDayLength", [](float day_length_seconds) {
    //
  });

  lua.set_function("getDayLength", []() {
    //
  });
}

void BindGothicSpecific(sol::state& lua) {
  SPDLOG_TRACE("Initializing Gothic 2 Addon 2.6 specific bindings...");

  BindPlayers(lua);
  BindNpc(lua);
  BindInput(lua);
  BindDiscord(lua);
  BindDraw(lua);
  BindTexture(lua);
  BindSound(lua);
  BindTime(lua);
}

void CleanupGothicViews() {
  LuaDraw::CleanupViews();
  LuaTexture::CleanupViews();
}

}  // namespace gmp::gothic

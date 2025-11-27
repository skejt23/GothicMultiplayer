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

#include "shared_bind.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <spdlog/spdlog.h>

#include "shared/lua_constants.h"

namespace lua {
namespace bindings {

namespace {

std::optional<ServerInfoProvider> g_server_info_provider;

std::string Function_GetHostname() {
  if (!g_server_info_provider || !g_server_info_provider->get_hostname) {
    SPDLOG_WARN("Cannot get hostname before the server info provider is set");
    return {};
  }

  return g_server_info_provider->get_hostname();
}

int Function_GetMaxSlots() {
  if (!g_server_info_provider || !g_server_info_provider->get_max_slots) {
    SPDLOG_WARN("Cannot get max slots before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_max_slots();
}

sol::object Function_GetOnlinePlayers(sol::this_state ts) {
  sol::state_view lua(ts);
  if (!g_server_info_provider || !g_server_info_provider->get_online_players) {
    SPDLOG_WARN("Cannot get online players before the server info provider is set");
    return sol::make_object(lua, sol::lua_nil);
  }

  const auto players = g_server_info_provider->get_online_players();
  sol::table players_table = lua.create_table();
  std::uint32_t index = 1;
  for (int player_id : players) {
    players_table[index++] = player_id;
  }

  return players_table;
}

int Function_GetPlayersCount() {
  if (!g_server_info_provider || !g_server_info_provider->get_players_count) {
    SPDLOG_WARN("Cannot get players count before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_players_count();
}

float Function_GetDistance2d(float x1, float y1, float x2, float y2) {
  const float dx = x1 - x2;
  const float dy = y1 - y2;
  return std::sqrt(dx * dx + dy * dy);
}

float Function_GetDistance3d(float x1, float y1, float z1, float x2, float y2, float z2) {
  const float dx = x1 - x2;
  const float dy = y1 - y2;
  const float dz = z1 - z2;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float Function_GetVectorAngle(float x1, float y1, float x2, float y2) {
  return std::atan2(y2 - y1, x2 - x1);
}

}  // namespace

void BindSharedConstants(sol::state& lua) {
  lua["TALENT_UNKNOWN"] = TALENT_UNKNOWN;
  lua["TALENT_1H"] = TALENT_1H;
  lua["TALENT_2H"] = TALENT_2H;
  lua["TALENT_BOW"] = TALENT_BOW;
  lua["TALENT_CROSSBOW"] = TALENT_CROSSBOW;
  lua["TALENT_PICK_LOCKS"] = TALENT_PICK_LOCKS;
  lua["TALENT_PICKPOCKET"] = TALENT_PICKPOCKET;
  lua["TALENT_MAGE"] = TALENT_MAGE;
  lua["TALENT_SNEAK"] = TALENT_SNEAK;
  lua["TALENT_REGENERATE"] = TALENT_REGENERATE;
  lua["TALENT_FIREMASTER"] = TALENT_FIREMASTER;
  lua["TALENT_ACROBATIC"] = TALENT_ACROBATIC;
  lua["TALENT_PICKPOCKET_UNUSED"] = TALENT_PICKPOCKET_UNUSED;
  lua["TALENT_SMITH"] = TALENT_SMITH;
  lua["TALENT_RUNES"] = TALENT_RUNES;
  lua["TALENT_ALCHEMY"] = TALENT_ALCHEMY;
  lua["TALENT_THROPHY"] = TALENT_THROPHY;
  lua["TALENT_A"] = TALENT_A;
  lua["TALENT_B"] = TALENT_B;
  lua["TALENT_C"] = TALENT_C;
  lua["TALENT_D"] = TALENT_D;
  lua["TALENT_E"] = TALENT_E;
  lua["TALENT_MAX"] = TALENT_MAX;

  lua["WEAPON_1H"] = WEAPON_1H;
  lua["WEAPON_2H"] = WEAPON_2H;
  lua["WEAPON_BOW"] = WEAPON_BOW;
  lua["WEAPON_CBOW"] = WEAPON_CBOW;

  lua["DAMAGE_INVALID"] = DAMAGE_INVALID;
  lua["DAMAGE_BARRIER"] = DAMAGE_BARRIER;
  lua["DAMAGE_BLUNT"] = DAMAGE_BLUNT;
  lua["DAMAGE_EDGE"] = DAMAGE_EDGE;
  lua["DAMAGE_FIRE"] = DAMAGE_FIRE;
  lua["DAMAGE_FLY"] = DAMAGE_FLY;
  lua["DAMAGE_MAGIC"] = DAMAGE_MAGIC;
  lua["DAMAGE_POINT"] = DAMAGE_POINT;
  lua["DAMAGE_FALL"] = DAMAGE_FALL;

  lua["ITEM_CAT_NONE"] = ITEM_CAT_NONE;
  lua["ITEM_CAT_NF"] = ITEM_CAT_NF;
  lua["ITEM_CAT_FF"] = ITEM_CAT_FF;
  lua["ITEM_CAT_MUN"] = ITEM_CAT_MUN;
  lua["ITEM_CAT_ARMOR"] = ITEM_CAT_ARMOR;
  lua["ITEM_CAT_FOOD"] = ITEM_CAT_FOOD;
  lua["ITEM_CAT_DOCS"] = ITEM_CAT_DOCS;
  lua["ITEM_CAT_POTION"] = ITEM_CAT_POTION;
  lua["ITEM_CAT_LIGHT"] = ITEM_CAT_LIGHT;
  lua["ITEM_CAT_RUNE"] = ITEM_CAT_RUNE;
  lua["ITEM_CAT_MAGIC"] = ITEM_CAT_MAGIC;
  lua["ITEM_FLAG_DAG"] = ITEM_FLAG_DAG;
  lua["ITEM_FLAG_SWD"] = ITEM_FLAG_SWD;
  lua["ITEM_FLAG_AXE"] = ITEM_FLAG_AXE;
  lua["ITEM_FLAG_2HD_SWD"] = ITEM_FLAG_2HD_SWD;
  lua["ITEM_FLAG_2HD_AXE"] = ITEM_FLAG_2HD_AXE;
  lua["ITEM_FLAG_SHIELD"] = ITEM_FLAG_SHIELD;
  lua["ITEM_FLAG_BOW"] = ITEM_FLAG_BOW;
  lua["ITEM_FLAG_CROSSBOW"] = ITEM_FLAG_CROSSBOW;
  lua["ITEM_FLAG_RING"] = ITEM_FLAG_RING;
  lua["ITEM_FLAG_AMULET"] = ITEM_FLAG_AMULET;
  lua["ITEM_FLAG_BELT"] = ITEM_FLAG_BELT;
  lua["ITEM_FLAG_DROPPED"] = ITEM_FLAG_DROPPED;
  lua["ITEM_FLAG_MI"] = ITEM_FLAG_MI;
  lua["ITEM_FLAG_MULTI"] = ITEM_FLAG_MULTI;
  lua["ITEM_FLAG_NFOCUS"] = ITEM_FLAG_NFOCUS;
  lua["ITEM_FLAG_CREATEAMMO"] = ITEM_FLAG_CREATEAMMO;
  lua["ITEM_FLAG_NSPLIT"] = ITEM_FLAG_NSPLIT;
  lua["ITEM_FLAG_DRINK"] = ITEM_FLAG_DRINK;
  lua["ITEM_FLAG_TORCH"] = ITEM_FLAG_TORCH;
  lua["ITEM_FLAG_THROW"] = ITEM_FLAG_THROW;
  lua["ITEM_FLAG_ACTIVE"] = ITEM_FLAG_ACTIVE;
  lua["ITEM_WEAR_NO"] = ITEM_WEAR_NO;
  lua["ITEM_WEAR_TORSO"] = ITEM_WEAR_TORSO;
  lua["ITEM_WEAR_HEAD"] = ITEM_WEAR_HEAD;
  lua["ITEM_WEAR_LIGHT"] = ITEM_WEAR_LIGHT;

  lua["WEAPONMODE_NONE"] = WEAPONMODE_NONE;
  lua["WEAPONMODE_FIST"] = WEAPONMODE_FIST;
  lua["WEAPONMODE_DAG"] = WEAPONMODE_DAG;
  lua["WEAPONMODE_1HS"] = WEAPONMODE_1HS;
  lua["WEAPONMODE_2HS"] = WEAPONMODE_2HS;
  lua["WEAPONMODE_BOW"] = WEAPONMODE_BOW;
  lua["WEAPONMODE_CBOW"] = WEAPONMODE_CBOW;
  lua["WEAPONMODE_MAG"] = WEAPONMODE_MAG;
  lua["WEAPONMODE_MAX"] = WEAPONMODE_MAX;
}

void BindSharedFunctions(sol::state& lua) {
  lua["getHostname"] = Function_GetHostname;
  lua["getMaxSlots"] = Function_GetMaxSlots;
  lua["getOnlinePlayers"] = Function_GetOnlinePlayers;
  lua["getPlayersCount"] = Function_GetPlayersCount;
  lua["getDistance2d"] = Function_GetDistance2d;
  lua["getDistance3d"] = Function_GetDistance3d;
  lua["getVectorAngle"] = Function_GetVectorAngle;
}

void SetServerInfoProvider(ServerInfoProvider provider) {
  g_server_info_provider = std::move(provider);
}

}  // namespace bindings
}  // namespace lua

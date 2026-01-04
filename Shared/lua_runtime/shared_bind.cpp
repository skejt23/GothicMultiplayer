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

#include "lua_constants.h"

namespace lua {
namespace bindings {

namespace {

std::optional<ServerInfoProvider> g_server_info_provider;

/* luadoc (func)
*
* Get the hostname of the server.
*
* @name     getHostname
* @side     shared
* @category Game
* @return   (string)       Server hostname.
*
*/
std::string Function_GetHostname() {
  if (!g_server_info_provider || !g_server_info_provider->get_hostname) {
    SPDLOG_WARN("Cannot get hostname before the server info provider is set");
    return {};
  }

  return g_server_info_provider->get_hostname();
}

/* luadoc (func)
*
* Get the max number of slots available on the server.
*
* @name     getMaxSlots
* @side     shared
* @category Game
* @return   (int)       Max slots number on the server.
*
*/
int Function_GetMaxSlots() {
  if (!g_server_info_provider || !g_server_info_provider->get_max_slots) {
    SPDLOG_WARN("Cannot get max slots before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_max_slots();
}

/* luadoc (func)
*
* Get the array containing player ids that are currently online.
*
* @name     getOnlinePlayers
* @side     shared
* @category Game
* @return   ({...})       Table containing player ids.
*
*/
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

/* luadoc (func)
*
* Get the number of online players on the server.
*
* @name     getPlayersCount
* @side     shared
* @category Game
* @return   (int)       Number of players on the server.
*
*/
int Function_GetPlayersCount() {
  if (!g_server_info_provider || !g_server_info_provider->get_players_count) {
    SPDLOG_WARN("Cannot get players count before the server info provider is set");
    return 0;
  }

  return g_server_info_provider->get_players_count();
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
}

void SetServerInfoProvider(ServerInfoProvider provider) {
  g_server_info_provider = std::move(provider);
}

}  // namespace bindings
}  // namespace lua

/* luadoc (const)
*
* Unknown talent.
*
* @category Talent
* @side     shared
* @name     TALENT_UNKNOWN
*
*/

/* luadoc (const)
*
* One-handed weapon talent.
*
* @category Talent
* @side     shared
* @name     TALENT_1H
*
*/

/* luadoc (const)
*
* Two-handed weapon talent.
*
* @category Talent
* @side     shared
* @name     TALENT_2H
*
*/

/* luadoc (const)
*
* Bow talent.
*
* @category Talent
* @side     shared
* @name     TALENT_BOW
*
*/

/* luadoc (const)
*
* Crossbow talent.
*
* @category Talent
* @side     shared
* @name     TALENT_CROSSBOW
*
*/

/* luadoc (const)
*
* Lockpicking talent.
*
* @category Talent
* @side     shared
* @name     TALENT_PICK_LOCKS
*
*/

/* luadoc (const)
*
* Pickpocket talent.
*
* @category Talent
* @side     shared
* @name     TALENT_PICKPOCKET
*
*/

/* luadoc (const)
*
* Mage talent.
*
* @category Talent
* @side     shared
* @name     TALENT_MAGE
*
*/

/* luadoc (const)
*
* Sneak talent.
*
* @category Talent
* @side     shared
* @name     TALENT_SNEAK
*
*/

/* luadoc (const)
*
* Regeneration talent.
*
* @category Talent
* @side     shared
* @name     TALENT_REGENERATE
*
*/

/* luadoc (const)
*
* Fire master talent.
*
* @category Talent
* @side     shared
* @name     TALENT_FIREMASTER
*
*/

/* luadoc (const)
*
* Acrobatics talent.
*
* @category Talent
* @side     shared
* @name     TALENT_ACROBATIC
*
*/

/* luadoc (const)
*
* Pickpocket (unused) talent.
*
* @category Talent
* @side     shared
* @name     TALENT_PICKPOCKET_UNUSED
*
*/

/* luadoc (const)
*
* Smithing talent.
*
* @category Talent
* @side     shared
* @name     TALENT_SMITH
*
*/

/* luadoc (const)
*
* Rune usage talent.
*
* @category Talent
* @side     shared
* @name     TALENT_RUNES
*
*/

/* luadoc (const)
*
* Alchemy talent.
*
* @category Talent
* @side     shared
* @name     TALENT_ALCHEMY
*
*/

/* luadoc (const)
*
* Trophy hunting talent.
*
* @category Talent
* @side     shared
* @name     TALENT_THROPHY
*
*/

/* luadoc (const)
*
* Talent A (reserved).
*
* @category Talent
* @side     shared
* @name     TALENT_A
*
*/

/* luadoc (const)
*
* Talent B (reserved).
*
* @category Talent
* @side     shared
* @name     TALENT_B
*
*/

/* luadoc (const)
*
* Talent C (reserved).
*
* @category Talent
* @side     shared
* @name     TALENT_C
*
*/

/* luadoc (const)
*
* Talent D (reserved).
*
* @category Talent
* @side     shared
* @name     TALENT_D
*
*/

/* luadoc (const)
*
* Talent E (reserved).
*
* @category Talent
* @side     shared
* @name     TALENT_E
*
*/

/* luadoc (const)
*
* Maximum talent value / sentinel.
*
* @category Talent
* @side     shared
* @name     TALENT_MAX
*
*/


/* luadoc (const)
*
* One-handed weapon type.
*
* @category Weapon
* @side     shared
* @name     WEAPON_1H
*
*/

/* luadoc (const)
*
* Two-handed weapon type.
*
* @category Weapon
* @side     shared
* @name     WEAPON_2H
*
*/

/* luadoc (const)
*
* Bow weapon type.
*
* @category Weapon
* @side     shared
* @name     WEAPON_BOW
*
*/

/* luadoc (const)
*
* Crossbow weapon type.
*
* @category Weapon
* @side     shared
* @name     WEAPON_CBOW
*
*/


/* luadoc (const)
*
* Invalid damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_INVALID
*
*/

/* luadoc (const)
*
* Barrier damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_BARRIER
*
*/

/* luadoc (const)
*
* Blunt damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_BLUNT
*
*/

/* luadoc (const)
*
* Edge (slashing) damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_EDGE
*
*/

/* luadoc (const)
*
* Fire damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_FIRE
*
*/

/* luadoc (const)
*
* Fly / impact damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_FLY
*
*/

/* luadoc (const)
*
* Magic damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_MAGIC
*
*/

/* luadoc (const)
*
* Point (piercing) damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_POINT
*
*/

/* luadoc (const)
*
* Fall damage type.
*
* @category Damage
* @side     shared
* @name     DAMAGE_FALL
*
*/


/* luadoc (const)
*
* No item category.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_NONE
*
*/

/* luadoc (const)
*
* Item category: NF.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_NF
*
*/

/* luadoc (const)
*
* Item category: FF.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_FF
*
*/

/* luadoc (const)
*
* Item category: ammunition.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_MUN
*
*/

/* luadoc (const)
*
* Item category: armor.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_ARMOR
*
*/

/* luadoc (const)
*
* Item category: food.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_FOOD
*
*/

/* luadoc (const)
*
* Item category: documents.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_DOCS
*
*/

/* luadoc (const)
*
* Item category: potion.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_POTION
*
*/

/* luadoc (const)
*
* Item category: light.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_LIGHT
*
*/

/* luadoc (const)
*
* Item category: rune.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_RUNE
*
*/

/* luadoc (const)
*
* Item category: magic.
*
* @category Item
* @side     shared
* @name     ITEM_CAT_MAGIC
*
*/

/* luadoc (const)
*
* Item flag: dagger.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_DAG
*
*/

/* luadoc (const)
*
* Item flag: sword.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_SWD
*
*/

/* luadoc (const)
*
* Item flag: axe.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_AXE
*
*/

/* luadoc (const)
*
* Item flag: two-handed sword.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_2HD_SWD
*
*/

/* luadoc (const)
*
* Item flag: two-handed axe.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_2HD_AXE
*
*/

/* luadoc (const)
*
* Item flag: shield.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_SHIELD
*
*/

/* luadoc (const)
*
* Item flag: bow.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_BOW
*
*/

/* luadoc (const)
*
* Item flag: crossbow.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_CROSSBOW
*
*/

/* luadoc (const)
*
* Item flag: ring.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_RING
*
*/

/* luadoc (const)
*
* Item flag: amulet.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_AMULET
*
*/

/* luadoc (const)
*
* Item flag: belt.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_BELT
*
*/

/* luadoc (const)
*
* Item flag: dropped item.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_DROPPED
*
*/

/* luadoc (const)
*
* Item flag: mission item.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_MI
*
*/

/* luadoc (const)
*
* Item flag: multi-item.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_MULTI
*
*/

/* luadoc (const)
*
* Item flag: no focus.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_NFOCUS
*
*/

/* luadoc (const)
*
* Item flag: creates ammo.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_CREATEAMMO
*
*/

/* luadoc (const)
*
* Item flag: no split.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_NSPLIT
*
*/

/* luadoc (const)
*
* Item flag: drinkable.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_DRINK
*
*/

/* luadoc (const)
*
* Item flag: torch.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_TORCH
*
*/

/* luadoc (const)
*
* Item flag: throwable.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_THROW
*
*/

/* luadoc (const)
*
* Item flag: active item.
*
* @category Item
* @side     shared
* @name     ITEM_FLAG_ACTIVE
*
*/

/* luadoc (const)
*
* Item wear slot: none.
*
* @category Item
* @side     shared
* @name     ITEM_WEAR_NO
*
*/

/* luadoc (const)
*
* Item wear slot: torso.
*
* @category Item
* @side     shared
* @name     ITEM_WEAR_TORSO
*
*/

/* luadoc (const)
*
* Item wear slot: head.
*
* @category Item
* @side     shared
* @name     ITEM_WEAR_HEAD
*
*/

/* luadoc (const)
*
* Item wear slot: light.
*
* @category Item
* @side     shared
* @name     ITEM_WEAR_LIGHT
*
*/


/* luadoc (const)
*
* No weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_NONE
*
*/

/* luadoc (const)
*
* Fist weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_FIST
*
*/

/* luadoc (const)
*
* Dagger weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_DAG
*
*/

/* luadoc (const)
*
* One-handed sword weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_1HS
*
*/

/* luadoc (const)
*
* Two-handed sword weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_2HS
*
*/

/* luadoc (const)
*
* Bow weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_BOW
*
*/

/* luadoc (const)
*
* Crossbow weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_CBOW
*
*/

/* luadoc (const)
*
* Magic weapon mode.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_MAG
*
*/

/* luadoc (const)
*
* Maximum weapon mode value / sentinel.
*
* @category WeaponMode
* @side     shared
* @name     WEAPONMODE_MAX
*
*/

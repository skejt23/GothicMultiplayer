/*
MIT License

Copyright (c) 2024 Gothic Multiplayer Team.

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

#include "lua_constants.h"

#include "net_enums.h"
#include "server_constants.h"

namespace lua {
namespace bindings {

/* luagmp (const)
 *
 * Selects the equipment category of the item; rings use an available ring slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_GENERIC
 *
 */

/* luagmp (const)
 *
 * Selects the armor slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_ARMOR
 *
 */

/* luagmp (const)
 *
 * Selects the melee weapon slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_MELEE_WEAPON
 *
 */

/* luagmp (const)
 *
 * Selects the ranged weapon slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_RANGED_WEAPON
 *
 */

/* luagmp (const)
 *
 * Selects the helmet slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_HELMET
 *
 */

/* luagmp (const)
 *
 * Selects the shield slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_SHIELD
 *
 */

/* luagmp (const)
 *
 * Selects the amulet slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_AMULET
 *
 */

/* luagmp (const)
 *
 * Selects the belt slot.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_BELT
 *
 */

/* luagmp (const)
 *
 * Selects the first ring slot, independently of the other ring's item instance.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_RING_LEFT
 *
 */

/* luagmp (const)
 *
 * Selects the second ring slot, independently of the other ring's item instance.
 *
 * @category Inventory
 * @side     server
 * @name     EQUIP_SLOT_RING_RIGHT
 *
 */

/* luagmp (const)
 *
 * Represents disconnected player state.
 *
 * @category Network
 * @side     server
 * @name     DISCONNECTED
 *
 */

/* luagmp (const)
 *
 * Represents lost connection player state.
 *
 * @category Network
 * @side     server
 * @name     LOST_CONNECTION
 *
 */

/* luagmp (const)
 *
 * Represents crash player state.
 *
 * @category Network
 * @side     server
 * @name     HAS_CRASHED
 *
 */

void BindServerConstants(sol::state& lua) {
  lua["DISCONNECTED"] = gmp::server::DISCONNECTED;
  lua["LOST_CONNECTION"] = gmp::server::LOST_CONNECTION;
  lua["HAS_CRASHED"] = gmp::server::HAS_CRASHED;
  lua["EQUIP_SLOT_GENERIC"] = Net::EQUIP_SLOT_GENERIC;
  lua["EQUIP_SLOT_ARMOR"] = Net::EQUIP_SLOT_ARMOR;
  lua["EQUIP_SLOT_MELEE_WEAPON"] = Net::EQUIP_SLOT_MELEE_WEAPON;
  lua["EQUIP_SLOT_RANGED_WEAPON"] = Net::EQUIP_SLOT_RANGED_WEAPON;
  lua["EQUIP_SLOT_HELMET"] = Net::EQUIP_SLOT_HELMET;
  lua["EQUIP_SLOT_SHIELD"] = Net::EQUIP_SLOT_SHIELD;
  lua["EQUIP_SLOT_AMULET"] = Net::EQUIP_SLOT_AMULET;
  lua["EQUIP_SLOT_BELT"] = Net::EQUIP_SLOT_BELT;
  lua["EQUIP_SLOT_RING_LEFT"] = Net::EQUIP_SLOT_RING_LEFT;
  lua["EQUIP_SLOT_RING_RIGHT"] = Net::EQUIP_SLOT_RING_RIGHT;
}

}  // namespace bindings
}  // namespace lua

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

#include "lua_interface.h"

#include "ZenGin/zGothicAPI.h"
#include "gothic-patches/hud_control.h"

namespace gmp::gothic {

/* luagmp (func)
 *
 * This function will convert pixels to virtuals screen X dimension and return it as a result.
 * Virtuals are special type of unit used by the game to position UI elements independent from game resolution.
 *
 * @version  0.3.0
 * @name     pixelsToVirtualX
 * @side     client
 * @category Interface
 * @note     Use this function only when you want to convert x position or width on the screen.
 * @note     Virtual screen coordinates are similar to percentage values, where 0 is 0% and 8192 is 100%.
 * @param    (number) pixels  The pixels to convert.
 * @return   (number)         The virtuals after conversion.
 *
 */
int Function_PixelsToVirtualX(int pixels) {
  if (!screen) {
    return 0;
  }
  return screen->anx(pixels);
};

/* luagmp (func)
 *
 * This function will convert pixels to virtuals on screen Y dimension and return it as a result.
 * Virtuals are special type of unit used by the game to position UI elements independent from game resolution.
 *
 * @version  0.3.0
 * @name     pixelsToVirtualY
 * @side     client
 * @category Interface
 * @note     Use this function only when you want to convert y position or width on the screen.
 * @note     Virtual screen coordinates are similar to percentage values, where 0 is 0% and 8192 is 100%.
 * @param    (number) pixels  The pixels to convert.
 * @return   (number)         The virtuals after conversion.
 *
 */
int Function_PixelsToVirtualY(int pixels) {
  if (!screen) {
    return 0;
  }
  return screen->any(pixels);
};

/* luagmp (func)
 *
 * This function will convert virtuals to pixels on screen X dimension and return it as a result.
 *
 * @version  0.3.0
 * @name     virtualToPixelsX
 * @side     client
 * @category Interface
 * @note     Use this function only when you want to convert x position or width on the screen.
 * @param    (number) virtuals  The virtuals to convert.
 * @return   (number)           The pixels after conversion.
 *
 */
int Function_VirtualToPixelsX(int virtuals) {
  if (!screen) {
    return 0;
  }
  return screen->nax(virtuals);
};

/* luagmp (func)
 *
 * This function will convert virtuals to pixels on screen Y dimension and return it as a result.
 *
 * @version  0.3.0
 * @name     virtualToPixelsY
 * @side     client
 * @category Interface
 * @note     Use this function only when you want to convert y position or width on the screen.
 * @param    (number) virtuals  The virtuals to convert.
 * @return   (number)           The pixels after conversion.
 *
 */
int Function_VirtualToPixelsY(int virtuals) {
  if (!screen) {
    return 0;
  }
  return screen->nay(virtuals);
};

/* luagmp (func)
 *
 * This function will enable or disable the Gothic HUD.
 *
 * @version  0.3.0
 * @name     enableHud
 * @side     client
 * @category Interface
 * @param    (number) hud_type  HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @param    (boolean) enabled  New HUD state.
 * @return   (boolean)          True on success.
 *
 */
bool Function_EnableHud(std::int32_t hud_type, bool enabled) {
  return SetHudEnabled(hud_type, enabled);
}

/* luagmp (func)
 *
 * This function will return whether the Gothic HUD is enabled.
 *
 * @version  0.3.0
 * @name     isHudEnabled
 * @side     client
 * @category Interface
 * @param    (number) hud_type  HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @return   (boolean|nil)      HUD state, or nil for unsupported HUD types.
 *
 */
sol::object Function_IsHudEnabled(std::int32_t hud_type, sol::this_state ts) {
  sol::state_view lua(ts);
  auto enabled = GetHudEnabled(hud_type);
  if (!enabled.has_value()) {
    return sol::nil;
  }

  return sol::make_object(lua, *enabled);
}

/* luagmp (func)
 *
 * This function will set the Gothic HUD display mode.
 *
 * @version  0.3.0
 * @name     setHudMode
 * @side     client
 * @category Interface
 * @param    (number) hud_type  HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @param    (number) mode      HUD mode constant from the same table.
 * @return   (boolean)          True on success.
 *
 */
bool Function_SetHudMode(std::int32_t hud_type, std::int32_t mode) {
  return SetHudMode(hud_type, mode);
}

/* luagmp (func)
 *
 * This function will return the Gothic HUD display mode.
 *
 * @version  0.3.0
 * @name     getHudMode
 * @side     client
 * @category Interface
 * @param    (number) hud_type  HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @return   (number|nil)       HUD mode constant from the same table, or nil for unsupported HUD types.
 *
 */
sol::object Function_GetHudMode(std::int32_t hud_type, sol::this_state ts) {
  sol::state_view lua(ts);
  auto mode = GetHudMode(hud_type);
  if (!mode.has_value()) {
    return sol::nil;
  }

  return sol::make_object(lua, *mode);
}

/* luagmp (func)
 *
 * This function will set a Gothic HUD status bar position in virtual coordinates.
 *
 * @version  0.3.0
 * @name     setBarPosition
 * @side     client
 * @category Interface
 * @param    (number) hud_type  Status bar HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @param    (number) x         X position.
 * @param    (number) y         Y position.
 * @return   (boolean)          True on success.
 *
 */
bool Function_SetBarPosition(std::int32_t hud_type, std::int32_t x, std::int32_t y) {
  return SetHudBarPosition(hud_type, x, y);
}

/* luagmp (func)
 *
 * This function will return a Gothic HUD status bar position in virtual coordinates.
 *
 * @version  0.3.0
 * @name     getBarPosition
 * @side     client
 * @category Interface
 * @param    (number) hud_type  Status bar HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @return   ({x, y}|nil)       Status bar position, or nil for unsupported HUD types.
 *
 */
sol::object Function_GetBarPosition(std::int32_t hud_type, sol::this_state ts) {
  sol::state_view lua(ts);
  auto position = GetHudBarPosition(hud_type);
  if (!position.has_value()) {
    return sol::nil;
  }

  sol::table result = lua.create_table();
  result["x"] = position->x;
  result["y"] = position->y;
  return sol::make_object(lua, result);
}

/* luagmp (func)
 *
 * This function will set a Gothic HUD status bar size in virtual coordinates.
 *
 * @version  0.3.0
 * @name     setBarSize
 * @side     client
 * @category Interface
 * @param    (number) hud_type  Status bar HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @param    (number) width     Width.
 * @param    (number) height    Height.
 * @return   (boolean)          True on success.
 *
 */
bool Function_SetBarSize(std::int32_t hud_type, std::int32_t width, std::int32_t height) {
  return SetHudBarSize(hud_type, width, height);
}

/* luagmp (func)
 *
 * This function will return a Gothic HUD status bar size in virtual coordinates.
 *
 * @version  0.3.0
 * @name     getBarSize
 * @side     client
 * @category Interface
 * @param    (number) hud_type       Status bar HUD type constant. For more information, see [HUD Constants](../../client-constants/HUD.md).
 * @return   ({width, height}|nil)   Status bar size, or nil for unsupported HUD types.
 *
 */
sol::object Function_GetBarSize(std::int32_t hud_type, sol::this_state ts) {
  sol::state_view lua(ts);
  auto size = GetHudBarSize(hud_type);
  if (!size.has_value()) {
    return sol::nil;
  }

  sol::table result = lua.create_table();
  result["width"] = size->x;
  result["height"] = size->y;
  return sol::make_object(lua, result);
}

/* luagmp (func)
 *
 * This function will check whether the Gothic console is open.
 *
 * @version  0.3.0
 * @name     isConsoleOpen
 * @side     client
 * @category Interface
 * @return   (boolean)  True when the console is visible.
 *
 */
bool Function_IsConsoleOpen() {
  return zcon && zcon->IsVisible() != 0;
}

/* luagmp (const)
 *
 * Represents the whole Gothic player status HUD.
 *
 * @category HUD
 * @side     client
 * @name     HUD_ALL
 *
 */

/* luagmp (const)
 *
 * Represents the health status bar.
 *
 * @category HUD
 * @side     client
 * @name     HUD_HEALTH_BAR
 *
 */

/* luagmp (const)
 *
 * Represents the mana status bar.
 *
 * @category HUD
 * @side     client
 * @name     HUD_MANA_BAR
 *
 */

/* luagmp (const)
 *
 * Represents the swim and dive breath status bar.
 *
 * @category HUD
 * @side     client
 * @name     HUD_SWIM_BAR
 *
 */

/* luagmp (const)
 *
 * Represents the focused NPC health status bar.
 *
 * @category HUD
 * @side     client
 * @name     HUD_FOCUS_BAR
 *
 */

/* luagmp (const)
 *
 * Represents the focused object name text.
 *
 * @category HUD
 * @side     client
 * @name     HUD_FOCUS_NAME
 *
 */

/* luagmp (const)
 *
 * Represents hidden HUD mode.
 *
 * @category HUD
 * @side     client
 * @name     HUD_MODE_HIDDEN
 *
 */

/* luagmp (const)
 *
 * Represents default HUD mode.
 *
 * @category HUD
 * @side     client
 * @name     HUD_MODE_DEFAULT
 *
 */

/* luagmp (const)
 *
 * Represents always-visible HUD mode.
 *
 * @category HUD
 * @side     client
 * @name     HUD_MODE_ALWAYS_VISIBLE
 *
 */

void BindInterface(sol::state& lua) {
  lua["pixelsToVirtualX"] = Function_PixelsToVirtualX;
  lua["pixelsToVirtualY"] = Function_PixelsToVirtualY;
  lua["virtualToPixelsX"] = Function_VirtualToPixelsX;
  lua["virtualToPixelsY"] = Function_VirtualToPixelsY;

  lua["enableHud"] = Function_EnableHud;
  lua["isHudEnabled"] = Function_IsHudEnabled;
  lua["setHudMode"] = Function_SetHudMode;
  lua["getHudMode"] = Function_GetHudMode;
  lua["setBarPosition"] = Function_SetBarPosition;
  lua["getBarPosition"] = Function_GetBarPosition;
  lua["setBarSize"] = Function_SetBarSize;
  lua["getBarSize"] = Function_GetBarSize;
  lua["isConsoleOpen"] = Function_IsConsoleOpen;

  // Constants
  lua["HUD_ALL"] = kHudAll;
  lua["HUD_HEALTH_BAR"] = kHudHealthBar;
  lua["HUD_MANA_BAR"] = kHudManaBar;
  lua["HUD_SWIM_BAR"] = kHudSwimBar;
  lua["HUD_FOCUS_BAR"] = kHudFocusBar;
  lua["HUD_FOCUS_NAME"] = kHudFocusName;
  lua["HUD_MODE_HIDDEN"] = kHudModeHidden;
  lua["HUD_MODE_DEFAULT"] = kHudModeDefault;
  lua["HUD_MODE_ALWAYS_VISIBLE"] = kHudModeAlwaysVisible;
}

}  // namespace gmp::gothic

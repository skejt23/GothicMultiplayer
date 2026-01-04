#include "process_input.h"

#include <array>
#include <cstring>

#include "gothic_events.h"
#include "shared/event.h"

#include "lua_cursor.h"

namespace gmp::gothic {

bool s_prevPressed[kMaxTrackedCode + 1] = {};
bool s_pressedThisFrame[kMaxTrackedCode + 1] = {};
bool s_toggledThisFrame[kMaxTrackedCode + 1] = {};
std::array<bool, kMaxTrackedCode + 1> s_disabledKeys = {};

namespace {
constexpr std::array<int, 8> kMouseButtonCodes = {
    MOUSE_BUTTONLEFT, MOUSE_BUTTONRIGHT, MOUSE_BUTTONMID, MOUSE_XBUTTON1,
    MOUSE_XBUTTON2,  MOUSE_XBUTTON3,    MOUSE_XBUTTON4,  MOUSE_XBUTTON5};
}  // namespace

void ProcessInput(zCInput* zinput) {
  if (!zinput) {
    return;
  }

  std::memset(s_pressedThisFrame, 0, sizeof(s_pressedThisFrame));
  std::memset(s_toggledThisFrame, 0, sizeof(s_toggledThisFrame));

  // Process keyboard keys
  for (const auto& key : kKeyboardKeys) {
    const int code = key.code;
    const bool is_disabled = code >= 0 && code <= kMaxTrackedCode && s_disabledKeys[code];
    const bool is_pressed = !is_disabled && zinput->KeyPressed(code) != 0;
    const bool was_pressed = s_prevPressed[code];

    if (is_pressed != was_pressed) {
      s_toggledThisFrame[code] = true;

      if (is_pressed) {
        EventManager::Instance().TriggerEvent(kEventOnKeyDownName, OnKeyEvent{code});
      } else {
        EventManager::Instance().TriggerEvent(kEventOnKeyUpName, OnKeyEvent{code});
      }
    }

    s_pressedThisFrame[code] = is_pressed;
  }

  // Process mouse movement and buttons
  for (std::size_t i = 0; i < kMouseButtonCodes.size(); ++i) {
    const int code = kMouseButtonCodes[i];
    const bool is_pressed = zinput->KeyPressed(code) != 0;
    const bool was_pressed = s_prevPressed[code];

    if (is_pressed != was_pressed) {
      s_toggledThisFrame[code] = true;

      if (is_pressed) {
        EventManager::Instance().TriggerEvent(kEventOnMouseDownName, OnMouseButtonEvent{code});
      } else {
        EventManager::Instance().TriggerEvent(kEventOnMouseUpName, OnMouseButtonEvent{code});
      }
    }

    s_pressedThisFrame[code] = is_pressed;
  }

  float dx = 0.0f;
  float dy = 0.0f;
  float wheel = 0.0f;
  zinput->GetMousePos(dx, dy, wheel);

  if (dx != 0.0f || dy != 0.0f) {
    EventManager::Instance().TriggerEvent(kEventOnMouseMoveName, OnMouseMoveEvent{dx, dy});
  }

  if (wheel != 0.0f) {
    EventManager::Instance().TriggerEvent(kEventOnMouseWheelName, OnMouseWheelEvent{wheel});
  }

  std::memcpy(s_prevPressed, s_pressedThisFrame, sizeof(s_prevPressed));

  LuaCursor::Instance().UpdateFromInput(zinput);
}

void BindInputConstants(sol::state& lua) {
  // Bind keyboard keys
  for (const auto& key : kKeyboardKeys) {
    lua[key.name] = key.code;
  }

  // Bind mouse keys
  for (const auto& key : kMouseKeys) {
    lua[key.name] = key.code;
  }

  // Bind game/logical action keys
  for (const auto& key : kGameKeys) {
    lua[key.name] = key.code;
  }

  // Bind input query functions
/* luadoc (func)
*
* The function is used to check whether the specified keyboard key is pressed.
*
* @name     KeyPressed
* @side     client
* @category Input
* @param    (int) key      The key code to check. For more information about key codes, see [Key Constants](../../client-constants/key.md).
* @return   (bool)             True if the key is currently pressed, false otherwise.
*
*/
  lua.set_function("KeyPressed", [](int key) -> bool {
    if (key < 0 || key > MAX_KEYS_AND_CODES) {
      return false;
    }
    return s_pressedThisFrame[key];
  });

/* luadoc (func)
*
* The function is used to check whether the specified keyboard key was toggled from unpressed to pressed state.
*
* @name     KeyToggled
* @side     client
* @category Input
* @param    (int) key      The key code to check. For more information about key codes, see [Key Constants](../../client-constants/key.md).
* @return   (bool)             True if the key was toggled, false otherwise.
*
*/
  lua.set_function("KeyToggled", [](int key) -> bool {
    if (key < 0 || key > MAX_KEYS_AND_CODES) {
      return false;
    }
    return s_toggledThisFrame[key];
  });

/* luadoc (func)
*
* This function will disable/enable default game actions that are bound to keys.
*
* @name     disableControls
* @side     client
* @category Input
* @param   (bool)             true when you want to disable game keys, otherwise false.
*
*/
  lua.set_function("disableControls", [](bool toggle) {
    Gothic_II_Addon::player->SetNpcAIDisabled(!toggle);
  });

/* luadoc (func)
*
* The function is used to check whether default game actions are disabled.
*
* @name     isControlsDisabled
* @side     client
* @category Input
* @return   (bool)             true when disabled, otherwise false.
*
*/
  lua.set_function("isControlsDisabled", []() {
    return Gothic_II_Addon::player->ai_disabled;
  });

/* luadoc (func)
*
* This function will disable/enable specified keyboard key, like: ESCAPE, TAB, etc.
*
* @name     disableKey
* @side     client
* @category Input
* @param   (int) keyId          The key code to disable. For more information about key codes, see [Key Constants](../../client-constants/key.md).
* @param   (bool) toggle          true when you want to disable specified keyboard key, otherwise false
*
*/
  lua.set_function("disableKey", [](int key, bool toggle) {
    if (key < 0 || key > kMaxTrackedCode) {
      return;
    }

    s_disabledKeys[key] = toggle;
  });

/* luadoc (func)
*
* The function is used to check whether the specified keyboard key is disabled.
*
* @name     isKeyDisabled
* @side     client
* @category Input
* @param   (int) keyId          The key code to check. For more information about key codes, see [Key Constants](../../client-constants/key.md).
* @return   (bool)             true when disabled, otherwise false.
*
*/
  lua.set_function("isKeyDisabled", [](int key) {
    if (key < 0 || key > kMaxTrackedCode) {
      return false;
    }

    return s_disabledKeys[key];
  });
}

void BindCursor(sol::state& lua) {
/* luadoc (func)
*
* Sets the cursor position in virtual (screen-scaled) coordinates.
*
* @name     setCursorPosition
* @side     client
* @category Cursor
* @param    (int) x X position.
* @param    (int) y Y position.
*
*/
  lua.set_function("setCursorPosition", [](int x, int y) {
    LuaCursor::Instance().setPosition(x, y); 
  });
  
/* luadoc (func)
*
* Returns the cursor position in virtual (screen-scaled) coordinates.
*
* @name     getCursorPosition
* @side     client
* @category Cursor
* @return   (int, int) X and Y position.
*
*/
  lua.set_function("getCursorPosition", [](sol::this_state s) {
    return LuaCursor::Instance().getPosition(s); 
  });

/* luadoc (func)
*
* Sets the cursor position in pixel coordinates.
*
* @name     setCursorPositionPx
* @side     client
* @category Cursor
* @param    (int) x X position in pixels.
* @param    (int) y Y position in pixels.
*
*/
  lua.set_function("setCursorPositionPx", [](int x, int y) {
    LuaCursor::Instance().setPositionPx(x, y);
  });
  
/* luadoc (func)
*
* Returns the cursor position in pixel coordinates.
*
* @name     getCursorPositionPx
* @side     client
* @category Cursor
* @return   (int, int) X and Y position in pixels.
*
*/
  lua.set_function("getCursorPositionPx", [](sol::this_state s) {
    return LuaCursor::Instance().getPositionPx(s);
  });

/* luadoc (func)
*
* Sets the cursor size in virtual (screen-scaled) units.
*
* @name     setCursorSize
* @side     client
* @category Cursor
* @param    (int) width Cursor width.
* @param    (int) height Cursor height.
*
*/
  lua.set_function("setCursorSize", [](int width, int height) {
    LuaCursor::Instance().setSize(width, height);
  });
  
/* luadoc (func)
*
* Returns the cursor size in virtual (screen-scaled) units.
*
* @name     getCursorSize
* @side     client
* @category Cursor
* @return   (int, int) Cursor width and height.
*
*/
  lua.set_function("getCursorSize", [](sol::this_state s) {
    return LuaCursor::Instance().getSize(s);
  });

/* luadoc (func)
*
* Sets the cursor size in pixel units.
*
* @name     setCursorSizePx
* @side     client
* @category Cursor
* @param    (int) width Cursor width in pixels.
* @param    (int) height Cursor height in pixels.
*
*/
  lua.set_function("setCursorSizePx", [](int width, int height) {
    LuaCursor::Instance().setSizePx(width, height);
  });
  
/* luadoc (func)
*
* Returns the cursor size in pixel units.
*
* @name     getCursorSizePx
* @side     client
* @category Cursor
* @return   (int, int) Cursor width and height in pixels.
*
*/
  lua.set_function("getCursorSizePx", [](sol::this_state s) {
    return LuaCursor::Instance().getSizePx(s);
  });

 /* luadoc (func)
*
* Sets the cursor texture.
*
* @name     setCursorTxt
* @side     client
* @category Cursor
* @param    (string) file Texture file name.
*
*/
  lua.set_function("setCursorTxt", [](const std::string& file) {
    LuaCursor::Instance().setTexture(file);
  });
  
/* luadoc (func)
*
* Returns the current cursor texture.
*
* @name     getCursorTxt
* @side     client
* @category Cursor
* @return   (string) Texture file name.
*
*/
  lua.set_function("getCursorTxt", []() {
    return LuaCursor::Instance().getTexture();
  });

/* luadoc (func)
*
* Sets whether the cursor is visible.
*
* @name     setCursorVisible
* @side     client
* @category Cursor
* @param    (bool) toggle True to show the cursor, false to hide it.
*
*/
  lua.set_function("setCursorVisible", [](bool toggle) {
    LuaCursor::Instance().setVisible(toggle);
  });
  
/* luadoc (func)
*
* Returns whether the cursor is currently visible.
*
* @name     isCursorVisible
* @side     client
* @category Cursor
* @return   (bool) True if the cursor is visible.
*
*/
  lua.set_function("isCursorVisible", []() {
    return LuaCursor::Instance().isVisible();
  });

/* luadoc (func)
*
* Sets the cursor movement sensitivity.
*
* @name     setCursorSensitivity
* @side     client
* @category Cursor
* @param    (number) sensitivity Cursor sensitivity multiplier.
*
*/
  lua.set_function("setCursorSensitivity", [](float sensitivity) {
    LuaCursor::Instance().setSensitivity(sensitivity);
  });
  
/* luadoc (func)
*
* Returns the cursor movement sensitivity.
*
* @name     getCursorSensitivity
* @side     client
* @category Cursor
* @return   (number) Cursor sensitivity multiplier.
*
*/
  lua.set_function("getCursorSensitivity", []() {
    return LuaCursor::Instance().getSensitivity();
  });

/* luadoc (func)
*
* Returns whether a mouse button is currently pressed.
*
* @name     isMouseBtnPressed
* @side     client
* @category Cursor
* @param    (int) button Mouse button identifier.
* @return   (bool) True if the button is pressed.
*
*/
  lua.set_function("isMouseBtnPressed", [](int button) {
    return LuaCursor::Instance().isButtonPressed(button);
  });
}

}  // namespace gmp::gothic
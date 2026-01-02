#include "process_input.h"

#include <cstring>

#include "gothic_events.h"
#include "shared/event.h"

namespace gmp::gothic {

bool s_prevPressed[MAX_KEYS_AND_CODES + 1] = {};
bool s_pressedThisFrame[MAX_KEYS_AND_CODES + 1] = {};
bool s_toggledThisFrame[MAX_KEYS_AND_CODES + 1] = {};

void ProcessInput(zCInput* zinput) {
  if (!zinput) {
    return;
  }

  std::memset(s_pressedThisFrame, 0, sizeof(s_pressedThisFrame));
  std::memset(s_toggledThisFrame, 0, sizeof(s_toggledThisFrame));

  // Process keyboard and mouse keys
  for (const auto& key : kKeyboardKeys) {
    const int code = key.code;
    const bool is_pressed = zinput->KeyPressed(code) != 0;
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

  for (const auto& key : kMouseKeys) {
    const int code = key.code;
    const bool is_pressed = zinput->KeyPressed(code) != 0;
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

  std::memcpy(s_prevPressed, s_pressedThisFrame, sizeof(s_prevPressed));
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
}

}  // namespace gmp::gothic
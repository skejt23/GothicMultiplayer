#include "process_input.h"

#include <spdlog/spdlog.h>
#include <windows.h>

#include <array>
#include <cstring>
#include <string>

#include "CChat.h"
#include "gothic_events.h"
#include "hooking/MemoryPatch.h"
#include "lua_cursor.h"
#include "shared/event.h"

namespace gmp::gothic {

bool s_prevPressed[kMaxTrackedCode + 1] = {};
bool s_pressedThisFrame[kMaxTrackedCode + 1] = {};
bool s_toggledThisFrame[kMaxTrackedCode + 1] = {};
std::array<bool, kMaxTrackedCode + 1> s_disabledKeys = {};

namespace {
constexpr DWORD kNativeInputGetStateAddress = 0x004D4D30;
constexpr DWORD kNativeInputGetToggledAddress = 0x004D5020;
constexpr DWORD kNativeGameHandleEventAddress = 0x006FC170;

using InputGetStateFn = float(__thiscall*)(zCInput_Win32*, unsigned short);
using InputGetToggledFn = int(__thiscall*)(zCInput_Win32*, unsigned short);
using GameHandleEventFn = int(__thiscall*)(oCGame*, int);

InputGetStateFn s_originalInputGetState = nullptr;
InputGetToggledFn s_originalInputGetToggled = nullptr;
GameHandleEventFn s_originalGameHandleEvent = nullptr;

float __fastcall ChatInputGetState(zCInput_Win32* input, void*, unsigned short logical_key) {
  if (CChat::GetInstance()->IsInputActive())
    return 0.0f;
  return s_originalInputGetState ? s_originalInputGetState(input, logical_key) : 0.0f;
}

int __fastcall ChatInputGetToggled(zCInput_Win32* input, void*, unsigned short logical_key) {
  if (CChat::GetInstance()->IsInputActive())
    return FALSE;
  return s_originalInputGetToggled ? s_originalInputGetToggled(input, logical_key) : FALSE;
}

int __fastcall ChatGameHandleEvent(oCGame* game, void*, int key) {
  // Native screen shortcuts are dispatched separately from logical polling.
  if (CChat::GetInstance()->IsInputActive())
    return TRUE;
  return s_originalGameHandleEvent ? s_originalGameHandleEvent(game, key) : FALSE;
}

constexpr int kMaxLogicalInputCode = GAME_LAME_HEAL;
constexpr std::array<int, 8> kMouseButtonCodes = {MOUSE_BUTTONLEFT, MOUSE_BUTTONRIGHT, MOUSE_BUTTONMID, MOUSE_XBUTTON1,
                                                  MOUSE_XBUTTON2,   MOUSE_XBUTTON3,    MOUSE_XBUTTON4,  MOUSE_XBUTTON5};

std::array<bool, kMaxTrackedCode + 1> s_prevRawPressed = {};
std::array<bool, kMaxTrackedCode + 1> s_rawPressedThisFrame = {};
std::array<bool, kMaxLogicalInputCode + 1> s_prevLogicalPressed = {};
std::array<bool, kMaxLogicalInputCode + 1> s_logicalPressedThisFrame = {};
std::array<bool, kMaxLogicalInputCode + 1> s_logicalToggledThisFrame = {};
std::array<bool, kMaxLogicalInputCode + 1> s_disabledLogicalKeys = {};

bool IsLogicalInputCode(int code) {
  for (const auto& key : kGameKeys) {
    if (key.code == code) {
      return true;
    }
  }
  return false;
}

bool ReadLogicalInputPressed(zCInput* zinput, int code, bool chat_input_active) {
  if (!zinput || chat_input_active) {
    return false;
  }

  return zinput->GetState(static_cast<unsigned short>(code)) != 0.0f;
}

std::string WideClipboardTextToAnsi(const wchar_t* text) {
  if (!text) {
    return {};
  }

  const int required_size = WideCharToMultiByte(CP_ACP, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (required_size <= 1) {
    return {};
  }

  std::string result(static_cast<std::size_t>(required_size - 1), '\0');
  WideCharToMultiByte(CP_ACP, 0, text, -1, result.data(), required_size, nullptr, nullptr);
  return result;
}

std::string ReadClipboardText() {
  if (!OpenClipboard(nullptr)) {
    return {};
  }

  std::string result;
  if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
    if (HANDLE handle = GetClipboardData(CF_UNICODETEXT)) {
      if (auto* text = static_cast<const wchar_t*>(GlobalLock(handle))) {
        result = WideClipboardTextToAnsi(text);
        GlobalUnlock(handle);
      }
    }
  } else if (IsClipboardFormatAvailable(CF_TEXT)) {
    if (HANDLE handle = GetClipboardData(CF_TEXT)) {
      if (auto* text = static_cast<const char*>(GlobalLock(handle))) {
        result = text;
        GlobalUnlock(handle);
      }
    }
  }

  CloseClipboard();
  return result;
}

bool ShouldEmitWhileChatInputActive(int code) {
  return code == KEY_UP || code == KEY_UPARROW || code == KEY_DOWN || code == KEY_DOWNARROW || code == KEY_PRIOR || code == KEY_PGUP ||
         code == KEY_NEXT || code == KEY_PGDN || code == KEY_HOME || code == KEY_END || code == KEY_RETURN || code == KEY_NUMPADENTER ||
         code == KEY_ESCAPE;
}

void NormalizeCursorDelta(zCInput* zinput, float& dx, float& dy) {
  float sensitivity_x = 1.0f;
  float sensitivity_y = 1.0f;
  zinput->GetMouseSensitivity(sensitivity_x, sensitivity_y);
  if (sensitivity_x != 0.0f) {
    dx /= sensitivity_x;
  }
  if (sensitivity_y != 0.0f) {
    dy /= sensitivity_y;
  }

  int flip_x = 0;
  int flip_y = 0;
  zinput->GetMouseFlipXY(flip_x, flip_y);
  if (flip_x) {
    dx = -dx;
  }
  if (flip_y) {
    dy = -dy;
  }
}
}  // namespace

void InstallChatInputHooks() {
  // Keep KeyPressed/GetKey unchanged: chat still needs text and modifiers.
  if (!s_originalInputGetState) {
    if (auto original = CreateHook(kNativeInputGetStateAddress, reinterpret_cast<DWORD>(&ChatInputGetState)))
      s_originalInputGetState = reinterpret_cast<InputGetStateFn>(*original);
    else
      SPDLOG_ERROR("Failed to install chat guard for native logical input state");
  }
  if (!s_originalInputGetToggled) {
    if (auto original = CreateHook(kNativeInputGetToggledAddress, reinterpret_cast<DWORD>(&ChatInputGetToggled)))
      s_originalInputGetToggled = reinterpret_cast<InputGetToggledFn>(*original);
    else
      SPDLOG_ERROR("Failed to install chat guard for native logical input toggles");
  }
  if (!s_originalGameHandleEvent) {
    if (auto original = CreateHook(kNativeGameHandleEventAddress, reinterpret_cast<DWORD>(&ChatGameHandleEvent)))
      s_originalGameHandleEvent = reinterpret_cast<GameHandleEventFn>(*original);
    else
      SPDLOG_ERROR("Failed to install chat guard for native game key events");
  }
}

std::optional<int> FindKeyboardKeyCode(std::string_view name) {
  for (const auto& key : kKeyboardKeys) {
    if (key.name == name) {
      return key.code;
    }
  }
  return std::nullopt;
}

std::string_view FindKeyboardKeyName(int code) {
  for (const auto& key : kKeyboardKeys) {
    if (key.code == code) {
      return key.name;
    }
  }
  return {};
}

void ProcessInput(zCInput* zinput) {
  if (!zinput) {
    return;
  }

  std::memset(s_pressedThisFrame, 0, sizeof(s_pressedThisFrame));
  std::memset(s_toggledThisFrame, 0, sizeof(s_toggledThisFrame));
  s_rawPressedThisFrame.fill(false);
  s_logicalPressedThisFrame.fill(false);
  s_logicalToggledThisFrame.fill(false);
  std::array<bool, kMaxTrackedCode + 1> processed_codes = {};
  processed_codes.fill(false);
  const bool chat_input_active = CChat::GetInstance()->IsInputActive();

  for (const auto& key : kGameKeys) {
    const int code = key.code;
    if (s_disabledLogicalKeys[code]) {
      continue;
    }

    const bool is_pressed = ReadLogicalInputPressed(zinput, code, chat_input_active);
    s_logicalPressedThisFrame[code] = is_pressed;
    s_logicalToggledThisFrame[code] = is_pressed && !s_prevLogicalPressed[code];
  }

  // Process keyboard keys
  for (const auto& key : kKeyboardKeys) {
    const int code = key.code;
    if (code < 0 || code > kMaxTrackedCode || processed_codes[code]) {
      continue;
    }
    processed_codes[code] = true;
    const bool is_disabled = code >= 0 && code <= kMaxTrackedCode && s_disabledKeys[code];
    const bool raw_pressed = zinput->KeyPressed(code) != 0;
    const bool is_pressed = (!chat_input_active || ShouldEmitWhileChatInputActive(code)) && !is_disabled && raw_pressed;
    const bool was_pressed = s_prevPressed[code];
    s_rawPressedThisFrame[code] = raw_pressed;

    if (is_pressed && !was_pressed) {
      s_toggledThisFrame[code] = true;
    }

    if (is_pressed != was_pressed) {
      if (is_pressed) {
        EventManager::Instance().TriggerEvent(kEventOnKeyDownName, OnKeyEvent{code});
      } else {
        EventManager::Instance().TriggerEvent(kEventOnKeyUpName, OnKeyEvent{code});
      }
    }

    s_pressedThisFrame[code] = is_pressed;
  }

  const bool ctrl_pressed = (s_rawPressedThisFrame[KEY_LCONTROL] && !s_disabledKeys[KEY_LCONTROL]) ||
                            (s_rawPressedThisFrame[KEY_RCONTROL] && !s_disabledKeys[KEY_RCONTROL]);
  const bool paste_pressed = s_rawPressedThisFrame[KEY_V] && !s_prevRawPressed[KEY_V] && !s_disabledKeys[KEY_V];
  if (ctrl_pressed && paste_pressed) {
    EventManager::Instance().TriggerEvent(kEventOnPasteName, OnPasteEvent{ReadClipboardText()});
  }

  // Process mouse movement and buttons
  for (std::size_t i = 0; i < kMouseButtonCodes.size(); ++i) {
    const int code = kMouseButtonCodes[i];
    const bool raw_pressed = zinput->KeyPressed(code) != 0;
    const bool is_pressed = !chat_input_active && raw_pressed;
    const bool was_pressed = s_prevPressed[code];
    s_rawPressedThisFrame[code] = raw_pressed;

    if (is_pressed && !was_pressed) {
      s_toggledThisFrame[code] = true;
    }

    if (is_pressed != was_pressed) {
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
  float cursor_dx = dx;
  float cursor_dy = dy;
  NormalizeCursorDelta(zinput, cursor_dx, cursor_dy);

  if (!chat_input_active && (dx != 0.0f || dy != 0.0f)) {
    EventManager::Instance().TriggerEvent(kEventOnMouseMoveName, OnMouseMoveEvent{dx, dy});
  }

  if (wheel != 0.0f) {
    EventManager::Instance().TriggerEvent(kEventOnMouseWheelName, OnMouseWheelEvent{wheel});
  }

  std::memcpy(s_prevPressed, s_pressedThisFrame, sizeof(s_prevPressed));
  s_prevRawPressed = s_rawPressedThisFrame;
  s_prevLogicalPressed = s_logicalPressedThisFrame;

  LuaCursor::Instance().UpdateFromInput(cursor_dx, cursor_dy);
}

/* luagmp (func)
 *
 * This function will check whether the specified keyboard key is pressed.
 *
 * @version  0.3.0
 * @name     isKeyPressed
 * @side     client
 * @category Input
 * @param    (number) key      The key code to check. For more information about key codes, see [Key Constants](../../client-constants/Key.md).
 * @return   (boolean)         True if the key is currently pressed, false otherwise.
 *
 */
bool Function_IsKeyPressed(int key) {
  if (key < 0 || key > kMaxTrackedCode) {
    return false;
  }
  return s_pressedThisFrame[key];
}

/* luagmp (func)
 *
 * This function will check whether the specified Gothic logical game action is pressed.
 *
 * @version  0.3.0
 * @name     isLogicalKeyPressed
 * @side     client
 * @category Input
 * @param    (number) key      The logical game action code to check. For more information, see [Logical Key
 * Constants](../../client-constants/LogicalKey.md).
 * @return   (boolean)         True if the logical action is currently active, false otherwise.
 *
 */
bool Function_IsLogicalKeyPressed(int key) {
  if (key < 0 || key > kMaxLogicalInputCode || !IsLogicalInputCode(key)) {
    return false;
  }
  if (s_disabledLogicalKeys[key]) {
    return false;
  }
  return s_logicalPressedThisFrame[key];
}

/* luagmp (func)
 *
 * This function will check whether the specified keyboard key was toggled from unpressed to pressed state.
 *
 * @version  0.3.0
 * @name     isKeyToggled
 * @side     client
 * @category Input
 * @param    (number) key      The key code to check. For more information about key codes, see [Key Constants](../../client-constants/Key.md).
 * @return   (boolean)         True if the key became pressed this frame, false otherwise.
 *
 */
bool Function_IsKeyToggled(int key) {
  if (key < 0 || key > kMaxTrackedCode) {
    return false;
  }
  return s_toggledThisFrame[key];
}

/* luagmp (func)
 *
 * This function will check whether the specified Gothic logical game action was toggled from unpressed to pressed state.
 *
 * @version  0.3.0
 * @name     isLogicalKeyToggled
 * @side     client
 * @category Input
 * @param    (number) key      The logical game action code to check. For more information, see [Logical Key
 * Constants](../../client-constants/LogicalKey.md).
 * @return   (boolean)         True if the logical action became pressed this frame, false otherwise.
 *
 */
bool Function_IsLogicalKeyToggled(int key) {
  if (key < 0 || key > kMaxLogicalInputCode || !IsLogicalInputCode(key)) {
    return false;
  }
  if (s_disabledLogicalKeys[key]) {
    return false;
  }
  return s_logicalToggledThisFrame[key];
}

/* luagmp (func)
 *
 * This function will enable or disable default game actions that are bound to keys.
 *
 * @version  0.3.0
 * @name     disableControls
 * @side     client
 * @category Input
 * @param    (boolean) disabled True to disable default game controls; false to enable.
 * @return   (boolean)          True on success.
 *
 */
bool Function_DisableControls(bool disabled) {
  if (!Gothic_II_Addon::player) {
    return false;
  }

  Gothic_II_Addon::player->SetNpcAIDisabled(disabled);
  return true;
}

/* luagmp (func)
 *
 * This function will check whether default game actions are disabled.
 *
 * @version  0.3.0
 * @name     isControlsDisabled
 * @side     client
 * @category Input
 * @return   (boolean)           True while the hero exists and default controls are disabled.
 *
 */
bool Function_IsControlsDisabled() {
  return Gothic_II_Addon::player && Gothic_II_Addon::player->ai_disabled;
}

/* luagmp (func)
 *
 * This function will enable or disable Lua polling and key events for a keyboard key. This does not change native game bindings.
 *
 * @version  0.3.0
 * @name     setScriptKeyEnabled
 * @side     client
 * @category Input
 * @param    (number) keyId        The key code to control. For more information about key codes, see [Key Constants](../../client-constants/Key.md).
 * @param (boolean) enabled True to include this input in script polling; false to suppress it.
 * @return   (boolean)             True on success.
 *
 */
bool Function_SetScriptKeyEnabled(int key, bool enabled) {
  if (key < 0 || key > kMaxTrackedCode) {
    return false;
  }

  s_disabledKeys[key] = !enabled;
  return true;
}

/* luagmp (func)
 *
 * This function will enable or disable a Gothic logical game action for Lua input polling. This does not change native game bindings.
 *
 * @version  0.3.0
 * @name     setScriptLogicalKeyEnabled
 * @side     client
 * @category Input
 * @param    (number) key        The logical game action code to control. For more information about key codes, see [Logical Key
 * Constants](../../client-constants/LogicalKey.md).
 * @param (boolean) enabled True to include this input in script polling; false to suppress it.
 * @return   (boolean)           True on success.
 *
 */
bool Function_SetScriptLogicalKeyEnabled(int key, bool enabled) {
  if (key < 0 || key > kMaxLogicalInputCode || !IsLogicalInputCode(key)) {
    return false;
  }

  s_disabledLogicalKeys[key] = !enabled;
  return true;
}

/* luagmp (func)
 *
 * This function will check whether the specified keyboard key is enabled for script polling.
 *
 * @version  0.3.0
 * @name     isScriptKeyEnabled
 * @side     client
 * @category Input
 * @param    (number) keyId        The key code to check. For more information about key codes, see [Key Constants](../../client-constants/Key.md).
 * @return   (boolean)             True for a valid input enabled for script polling.
 *
 */
bool Function_IsScriptKeyEnabled(int key) {
  if (key < 0 || key > kMaxTrackedCode) {
    return false;
  }

  return !s_disabledKeys[key];
}

/* luagmp (func)
 *
 * This function will check whether a Gothic logical game action is enabled for Lua input polling.
 *
 * @version  0.3.0
 * @name     isScriptLogicalKeyEnabled
 * @side     client
 * @category Input
 * @param    (number) key        The logical game action code to check. For more information about key codes, see [Logical Key
 * Constants](../../client-constants/LogicalKey.md).
 * @return   (boolean)           True for a valid input enabled for script polling.
 *
 */
bool Function_IsScriptLogicalKeyEnabled(int key) {
  if (key < 0 || key > kMaxLogicalInputCode || !IsLogicalInputCode(key)) {
    return false;
  }

  return !s_disabledLogicalKeys[key];
}

/* luagmp (func)
 *
 * This function will set the cursor position in virtual (screen-scaled) coordinates.
 *
 * @version  0.3.0
 * @name     setCursorPosition
 * @side     client
 * @category Cursor
 * @param    (number) x      X position.
 * @param    (number) y      Y position.
 *
 */
void Function_SetCursorPosition(int x, int y) {
  LuaCursor::Instance().setPosition(x, y);
}

/* luagmp (func)
 *
 * This function will return the current cursor position in virtual (screen-scaled) coordinates.
 *
 * @version  0.3.0
 * @name     getCursorPosition
 * @side     client
 * @category Cursor
 * @return   ({x, y})    X and Y position.
 *
 */
sol::table Function_GetCursorPosition(sol::this_state s) {
  return LuaCursor::Instance().getPosition(s);
}

/* luagmp (func)
 *
 * This function will set the cursor position in pixel coordinates.
 *
 * @version  0.3.0
 * @name     setCursorPositionPx
 * @side     client
 * @category Cursor
 * @param    (number) x      X position in pixels.
 * @param    (number) y      Y position in pixels.
 *
 */
void Function_SetCursorPositionPx(int x, int y) {
  LuaCursor::Instance().setPositionPx(x, y);
}

/* luagmp (func)
 *
 * This function will return the current cursor position in pixel coordinates.
 *
 * @version  0.3.0
 * @name     getCursorPositionPx
 * @side     client
 * @category Cursor
 * @return   ({x, y})    X and Y position in pixels.
 *
 */
sol::table Function_GetCursorPositionPx(sol::this_state s) {
  return LuaCursor::Instance().getPositionPx(s);
}

/* luagmp (func)
 *
 * This function will set the cursor size in virtual units.
 *
 * @version  0.3.0
 * @name     setCursorSize
 * @side     client
 * @category Cursor
 * @param    (number) width    cursor width.
 * @param    (number) height   cursor height.
 *
 */
void Function_SetCursorSize(int width, int height) {
  LuaCursor::Instance().setSize(width, height);
}

/* luagmp (func)
 *
 * This function will return the cursor size in virtual (screen-scaled) units.
 *
 * @version  0.3.0
 * @name     getCursorSize
 * @side     client
 * @category Cursor
 * @return   ({x, y})     cursor width and height.
 *
 */
sol::table Function_GetCursorSize(sol::this_state s) {
  return LuaCursor::Instance().getSize(s);
}

/* luagmp (func)
 *
 * This function will set the cursor size in pixel units.
 *
 * @version  0.3.0
 * @name     setCursorSizePx
 * @side     client
 * @category Cursor
 * @param    (number) width      cursor width in pixels.
 * @param    (number) height     cursor height in pixels.
 *
 */
void Function_SetCursorSizePx(int width, int height) {
  LuaCursor::Instance().setSizePx(width, height);
}

/* luagmp (func)
 *
 * This function will return the cursor size in pixel units.
 *
 * @version  0.3.0
 * @name     getCursorSizePx
 * @side     client
 * @category Cursor
 * @return   ({x, y})     cursor width and height in pixels.
 *
 */
sol::table Function_GetCursorSizePx(sol::this_state s) {
  return LuaCursor::Instance().getSizePx(s);
}

/* luagmp (func)
 *
 * This function will set the cursor texture.
 *
 * @version  0.3.0
 * @name     setCursorTextureFile
 * @side     client
 * @category Cursor
 * @param    (string) file    Texture file name.
 *
 */
void Function_SetCursorTextureFile(const std::string& file) {
  LuaCursor::Instance().setTextureFile(file);
}

/* luagmp (func)
 *
 * This function will return the current cursor texture.
 *
 * @version  0.3.0
 * @name     getCursorTextureFile
 * @side     client
 * @category Cursor
 * @return   (string)        Texture file name.
 *
 */
std::string Function_GetCursorTextureFile() {
  return LuaCursor::Instance().getTextureFile();
}

/* luagmp (func)
 *
 * This function will set whether the cursor is visible.
 *
 * @version  0.3.0
 * @name     setCursorVisible
 * @side     client
 * @category Cursor
 * @param    (boolean) toggle    True to show the cursor, false to hide it.
 *
 */
void Function_SetCursorVisible(bool toggle) {
  LuaCursor::Instance().setVisible(toggle);
}

/* luagmp (func)
 *
 * This function will return whether the cursor is currently visible.
 *
 * @version  0.3.0
 * @name     isCursorVisible
 * @side     client
 * @category Cursor
 * @return   (boolean)           True if the cursor is visible.
 *
 */
bool Function_IsCursorVisible() {
  return LuaCursor::Instance().isVisible();
}

/* luagmp (func)
 *
 * This function will set the cursor movement sensitivity.
 *
 * @version  0.3.0
 * @name     setCursorSensitivity
 * @side     client
 * @category Cursor
 * @param    (number) sensitivity    cursor sensitivity multiplier.
 *
 */
void Function_SetCursorSensitivity(float sensitivity) {
  LuaCursor::Instance().setSensitivity(sensitivity);
}

/* luagmp (func)
 *
 * This function will return the cursor movement sensitivity.
 *
 * @version  0.3.0
 * @name     getCursorSensitivity
 * @side     client
 * @category Cursor
 * @return   (number)        cursor sensitivity multiplier.
 *
 */
float Function_GetCursorSensitivity() {
  return LuaCursor::Instance().getSensitivity();
}

/* luagmp (func)
 *
 * This function will return whether a mouse button is currently pressed.
 *
 * @version  0.3.0
 * @name     isMouseBtnPressed
 * @side     client
 * @category Cursor
 * @param    (number) button     Mouse button identifier. For more information, see [Mouse Constants](../../client-constants/Mouse.md).
 * @return   (boolean)           True if the button is currently pressed, otherwise false.
 *
 */
bool Function_IsMouseBtnPressed(int button) {
  return LuaCursor::Instance().isButtonDown(button);
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
}

void BindInput(sol::state& lua) {
  lua["isKeyPressed"] = Function_IsKeyPressed;
  lua["isLogicalKeyPressed"] = Function_IsLogicalKeyPressed;
  lua["isKeyToggled"] = Function_IsKeyToggled;
  lua["isLogicalKeyToggled"] = Function_IsLogicalKeyToggled;
  lua["disableControls"] = Function_DisableControls;
  lua["isControlsDisabled"] = Function_IsControlsDisabled;
  lua["setScriptKeyEnabled"] = Function_SetScriptKeyEnabled;
  lua["setScriptLogicalKeyEnabled"] = Function_SetScriptLogicalKeyEnabled;
  lua["isScriptKeyEnabled"] = Function_IsScriptKeyEnabled;
  lua["isScriptLogicalKeyEnabled"] = Function_IsScriptLogicalKeyEnabled;
}

void BindCursor(sol::state& lua) {
  lua["setCursorPosition"] = Function_SetCursorPosition;
  lua["getCursorPosition"] = Function_GetCursorPosition;
  lua["setCursorPositionPx"] = Function_SetCursorPositionPx;
  lua["getCursorPositionPx"] = Function_GetCursorPositionPx;
  lua["setCursorSize"] = Function_SetCursorSize;
  lua["getCursorSize"] = Function_GetCursorSize;
  lua["setCursorSizePx"] = Function_SetCursorSizePx;
  lua["getCursorSizePx"] = Function_GetCursorSizePx;
  lua["setCursorTextureFile"] = Function_SetCursorTextureFile;
  lua["getCursorTextureFile"] = Function_GetCursorTextureFile;
  lua["setCursorVisible"] = Function_SetCursorVisible;
  lua["isCursorVisible"] = Function_IsCursorVisible;
  lua["setCursorSensitivity"] = Function_SetCursorSensitivity;
  lua["getCursorSensitivity"] = Function_GetCursorSensitivity;
  lua["isMouseBtnPressed"] = Function_IsMouseBtnPressed;
}

}  // namespace gmp::gothic

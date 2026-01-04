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

#pragma once

#include <array>
#include <string_view>

#include "ZenGin/zGothicAPI.h"
#include "sol/sol.hpp"

namespace gmp::gothic {

constexpr int kMaxTrackedCode = MAX_MOUSE_BTNS_AND_CODES;

// Input state arrays
extern bool s_prevPressed[kMaxTrackedCode + 1];
extern bool s_pressedThisFrame[kMaxTrackedCode + 1];
extern bool s_toggledThisFrame[kMaxTrackedCode + 1];

// Process input and trigger key events
void ProcessInput(zCInput* zinput);

// Key code with name for Lua binding
struct KeyBinding {
  int code;
  std::string_view name;
};

// clang-format off

// Keyboard keys
inline constexpr std::array kKeyboardKeys = {
    KeyBinding{KEY_ESCAPE, "KEY_ESCAPE"},
    KeyBinding{KEY_1, "KEY_1"}, KeyBinding{KEY_2, "KEY_2"}, KeyBinding{KEY_3, "KEY_3"},
    KeyBinding{KEY_4, "KEY_4"}, KeyBinding{KEY_5, "KEY_5"}, KeyBinding{KEY_6, "KEY_6"},
    KeyBinding{KEY_7, "KEY_7"}, KeyBinding{KEY_8, "KEY_8"}, KeyBinding{KEY_9, "KEY_9"},
    KeyBinding{KEY_0, "KEY_0"},
    KeyBinding{KEY_MINUS, "KEY_MINUS"}, KeyBinding{KEY_EQUALS, "KEY_EQUALS"},
    KeyBinding{KEY_BACK, "KEY_BACK"}, KeyBinding{KEY_TAB, "KEY_TAB"},
    KeyBinding{KEY_Q, "KEY_Q"}, KeyBinding{KEY_W, "KEY_W"}, KeyBinding{KEY_E, "KEY_E"},
    KeyBinding{KEY_R, "KEY_R"}, KeyBinding{KEY_T, "KEY_T"}, KeyBinding{KEY_Y, "KEY_Y"},
    KeyBinding{KEY_U, "KEY_U"}, KeyBinding{KEY_I, "KEY_I"}, KeyBinding{KEY_O, "KEY_O"},
    KeyBinding{KEY_P, "KEY_P"},
    KeyBinding{KEY_LBRACKET, "KEY_LBRACKET"}, KeyBinding{KEY_RBRACKET, "KEY_RBRACKET"},
    KeyBinding{KEY_RETURN, "KEY_RETURN"}, KeyBinding{KEY_LCONTROL, "KEY_LCONTROL"},
    KeyBinding{KEY_A, "KEY_A"}, KeyBinding{KEY_S, "KEY_S"}, KeyBinding{KEY_D, "KEY_D"},
    KeyBinding{KEY_F, "KEY_F"}, KeyBinding{KEY_G, "KEY_G"}, KeyBinding{KEY_H, "KEY_H"},
    KeyBinding{KEY_J, "KEY_J"}, KeyBinding{KEY_K, "KEY_K"}, KeyBinding{KEY_L, "KEY_L"},
    KeyBinding{KEY_SEMICOLON, "KEY_SEMICOLON"}, KeyBinding{KEY_APOSTROPHE, "KEY_APOSTROPHE"},
    KeyBinding{KEY_GRAVE, "KEY_GRAVE"}, KeyBinding{KEY_LSHIFT, "KEY_LSHIFT"},
    KeyBinding{KEY_BACKSLASH, "KEY_BACKSLASH"},
    KeyBinding{KEY_Z, "KEY_Z"}, KeyBinding{KEY_X, "KEY_X"}, KeyBinding{KEY_C, "KEY_C"},
    KeyBinding{KEY_V, "KEY_V"}, KeyBinding{KEY_B, "KEY_B"}, KeyBinding{KEY_N, "KEY_N"},
    KeyBinding{KEY_M, "KEY_M"},
    KeyBinding{KEY_COMMA, "KEY_COMMA"}, KeyBinding{KEY_PERIOD, "KEY_PERIOD"},
    KeyBinding{KEY_SLASH, "KEY_SLASH"}, KeyBinding{KEY_RSHIFT, "KEY_RSHIFT"},
    KeyBinding{KEY_MULTIPLY, "KEY_MULTIPLY"}, KeyBinding{KEY_LMENU, "KEY_LMENU"},
    KeyBinding{KEY_SPACE, "KEY_SPACE"}, KeyBinding{KEY_CAPITAL, "KEY_CAPITAL"},
    KeyBinding{KEY_F1, "KEY_F1"}, KeyBinding{KEY_F2, "KEY_F2"}, KeyBinding{KEY_F3, "KEY_F3"},
    KeyBinding{KEY_F4, "KEY_F4"}, KeyBinding{KEY_F5, "KEY_F5"}, KeyBinding{KEY_F6, "KEY_F6"},
    KeyBinding{KEY_F7, "KEY_F7"}, KeyBinding{KEY_F8, "KEY_F8"}, KeyBinding{KEY_F9, "KEY_F9"},
    KeyBinding{KEY_F10, "KEY_F10"}, KeyBinding{KEY_F11, "KEY_F11"}, KeyBinding{KEY_F12, "KEY_F12"},
    KeyBinding{KEY_F13, "KEY_F13"}, KeyBinding{KEY_F14, "KEY_F14"}, KeyBinding{KEY_F15, "KEY_F15"},
    KeyBinding{KEY_NUMLOCK, "KEY_NUMLOCK"}, KeyBinding{KEY_SCROLL, "KEY_SCROLL"},
    KeyBinding{KEY_NUMPAD0, "KEY_NUMPAD0"}, KeyBinding{KEY_NUMPAD1, "KEY_NUMPAD1"},
    KeyBinding{KEY_NUMPAD2, "KEY_NUMPAD2"}, KeyBinding{KEY_NUMPAD3, "KEY_NUMPAD3"},
    KeyBinding{KEY_NUMPAD4, "KEY_NUMPAD4"}, KeyBinding{KEY_NUMPAD5, "KEY_NUMPAD5"},
    KeyBinding{KEY_NUMPAD6, "KEY_NUMPAD6"}, KeyBinding{KEY_NUMPAD7, "KEY_NUMPAD7"},
    KeyBinding{KEY_NUMPAD8, "KEY_NUMPAD8"}, KeyBinding{KEY_NUMPAD9, "KEY_NUMPAD9"},
    KeyBinding{KEY_SUBTRACT, "KEY_SUBTRACT"}, KeyBinding{KEY_ADD, "KEY_ADD"},
    KeyBinding{KEY_DECIMAL, "KEY_DECIMAL"}, KeyBinding{KEY_OEM_102, "KEY_OEM_102"},
    KeyBinding{KEY_KANA, "KEY_KANA"}, KeyBinding{KEY_ABNT_C1, "KEY_ABNT_C1"},
    KeyBinding{KEY_CONVERT, "KEY_CONVERT"}, KeyBinding{KEY_NOCONVERT, "KEY_NOCONVERT"},
    KeyBinding{KEY_YEN, "KEY_YEN"}, KeyBinding{KEY_ABNT_C2, "KEY_ABNT_C2"},
    KeyBinding{KEY_NUMPADEQUALS, "KEY_NUMPADEQUALS"}, KeyBinding{KEY_PREVTRACK, "KEY_PREVTRACK"},
    KeyBinding{KEY_AT, "KEY_AT"}, KeyBinding{KEY_COLON, "KEY_COLON"},
    KeyBinding{KEY_UNDERLINE, "KEY_UNDERLINE"}, KeyBinding{KEY_KANJI, "KEY_KANJI"},
    KeyBinding{KEY_STOP, "KEY_STOP"}, KeyBinding{KEY_AX, "KEY_AX"},
    KeyBinding{KEY_UNLABELED, "KEY_UNLABELED"}, KeyBinding{KEY_NEXTTRACK, "KEY_NEXTTRACK"},
    KeyBinding{KEY_NUMPADENTER, "KEY_NUMPADENTER"}, KeyBinding{KEY_RCONTROL, "KEY_RCONTROL"},
    KeyBinding{KEY_MUTE, "KEY_MUTE"}, KeyBinding{KEY_CALCULATOR, "KEY_CALCULATOR"},
    KeyBinding{KEY_PLAYPAUSE, "KEY_PLAYPAUSE"}, KeyBinding{KEY_MEDIASTOP, "KEY_MEDIASTOP"},
    KeyBinding{KEY_VOLUMEDOWN, "KEY_VOLUMEDOWN"}, KeyBinding{KEY_VOLUMEUP, "KEY_VOLUMEUP"},
    KeyBinding{KEY_WEBHOME, "KEY_WEBHOME"}, KeyBinding{KEY_NUMPADCOMMA, "KEY_NUMPADCOMMA"},
    KeyBinding{KEY_DIVIDE, "KEY_DIVIDE"}, KeyBinding{KEY_SYSRQ, "KEY_SYSRQ"},
    KeyBinding{KEY_RMENU, "KEY_RMENU"}, KeyBinding{KEY_PAUSE, "KEY_PAUSE"},
    KeyBinding{KEY_HOME, "KEY_HOME"}, KeyBinding{KEY_UP, "KEY_UP"},
    KeyBinding{KEY_PRIOR, "KEY_PRIOR"}, KeyBinding{KEY_LEFT, "KEY_LEFT"},
    KeyBinding{KEY_RIGHT, "KEY_RIGHT"}, KeyBinding{KEY_END, "KEY_END"},
    KeyBinding{KEY_DOWN, "KEY_DOWN"}, KeyBinding{KEY_NEXT, "KEY_NEXT"},
    KeyBinding{KEY_INSERT, "KEY_INSERT"}, KeyBinding{KEY_DELETE, "KEY_DELETE"},
    KeyBinding{KEY_LWIN, "KEY_LWIN"}, KeyBinding{KEY_RWIN, "KEY_RWIN"},
    KeyBinding{KEY_APPS, "KEY_APPS"}, KeyBinding{KEY_POWER, "KEY_POWER"},
    KeyBinding{KEY_SLEEP, "KEY_SLEEP"}, KeyBinding{KEY_WAKE, "KEY_WAKE"},
    KeyBinding{KEY_WEBSEARCH, "KEY_WEBSEARCH"}, KeyBinding{KEY_WEBFAVORITES, "KEY_WEBFAVORITES"},
    KeyBinding{KEY_WEBREFRESH, "KEY_WEBREFRESH"}, KeyBinding{KEY_WEBSTOP, "KEY_WEBSTOP"},
    KeyBinding{KEY_WEBFORWARD, "KEY_WEBFORWARD"}, KeyBinding{KEY_WEBBACK, "KEY_WEBBACK"},
    KeyBinding{KEY_MYCOMPUTER, "KEY_MYCOMPUTER"}, KeyBinding{KEY_MAIL, "KEY_MAIL"},
    KeyBinding{KEY_MEDIASELECT, "KEY_MEDIASELECT"},
    // Aliases
    KeyBinding{KEY_BACKSPACE, "KEY_BACKSPACE"}, KeyBinding{KEY_NUMPADSTAR, "KEY_NUMPADSTAR"},
    KeyBinding{KEY_LALT, "KEY_LALT"}, KeyBinding{KEY_CAPSLOCK, "KEY_CAPSLOCK"},
    KeyBinding{KEY_NUMPADMINUS, "KEY_NUMPADMINUS"}, KeyBinding{KEY_NUMPADPLUS, "KEY_NUMPADPLUS"},
    KeyBinding{KEY_NUMPADPERIOD, "KEY_NUMPADPERIOD"}, KeyBinding{KEY_NUMPADSLASH, "KEY_NUMPADSLASH"},
    KeyBinding{KEY_RALT, "KEY_RALT"}, KeyBinding{KEY_UPARROW, "KEY_UPARROW"},
    KeyBinding{KEY_PGUP, "KEY_PGUP"}, KeyBinding{KEY_LEFTARROW, "KEY_LEFTARROW"},
    KeyBinding{KEY_RIGHTARROW, "KEY_RIGHTARROW"}, KeyBinding{KEY_DOWNARROW, "KEY_DOWNARROW"},
    KeyBinding{KEY_PGDN, "KEY_PGDN"}, KeyBinding{KEY_CIRCUMFLEX, "KEY_CIRCUMFLEX"},
};

// Mouse codes
inline constexpr std::array kMouseKeys = {
    KeyBinding{MOUSE_DX, "MOUSE_DX"}, KeyBinding{MOUSE_DY, "MOUSE_DY"},
    KeyBinding{MOUSE_UP, "MOUSE_UP"}, KeyBinding{MOUSE_DOWN, "MOUSE_DOWN"},
    KeyBinding{MOUSE_LEFT, "MOUSE_LEFT"}, KeyBinding{MOUSE_RIGHT, "MOUSE_RIGHT"},
    KeyBinding{MOUSE_WHEELUP, "MOUSE_WHEELUP"}, KeyBinding{MOUSE_WHEELDOWN, "MOUSE_WHEELDOWN"},
    KeyBinding{MOUSE_BUTTONLEFT, "MOUSE_BUTTONLEFT"}, KeyBinding{MOUSE_BUTTONRIGHT, "MOUSE_BUTTONRIGHT"},
    KeyBinding{MOUSE_BUTTONMID, "MOUSE_BUTTONMID"},
    KeyBinding{MOUSE_XBUTTON1, "MOUSE_XBUTTON1"}, KeyBinding{MOUSE_XBUTTON2, "MOUSE_XBUTTON2"},
    KeyBinding{MOUSE_XBUTTON3, "MOUSE_XBUTTON3"}, KeyBinding{MOUSE_XBUTTON4, "MOUSE_XBUTTON4"},
    KeyBinding{MOUSE_XBUTTON5, "MOUSE_XBUTTON5"},
};

// Game/logical action codes
inline constexpr std::array kGameKeys = {
    KeyBinding{GAME_LEFT, "GAME_LEFT"}, KeyBinding{GAME_RIGHT, "GAME_RIGHT"},
    KeyBinding{GAME_UP, "GAME_UP"}, KeyBinding{GAME_DOWN, "GAME_DOWN"},
    KeyBinding{GAME_ACTION, "GAME_ACTION"}, KeyBinding{GAME_SLOW, "GAME_SLOW"},
    KeyBinding{GAME_ACTION2, "GAME_ACTION2"}, KeyBinding{GAME_WEAPON, "GAME_WEAPON"},
    KeyBinding{GAME_SMOVE, "GAME_SMOVE"}, KeyBinding{GAME_SMOVE2, "GAME_SMOVE2"},
    KeyBinding{GAME_SHIFT, "GAME_SHIFT"}, KeyBinding{GAME_END, "GAME_END"},
    KeyBinding{GAME_INVENTORY, "GAME_INVENTORY"}, KeyBinding{GAME_LOOK, "GAME_LOOK"},
    KeyBinding{GAME_SNEAK, "GAME_SNEAK"}, KeyBinding{GAME_STRAFELEFT, "GAME_STRAFELEFT"},
    KeyBinding{GAME_STRAFERIGHT, "GAME_STRAFERIGHT"}, KeyBinding{GAME_SCREEN_STATUS, "GAME_SCREEN_STATUS"},
    KeyBinding{GAME_SCREEN_LOG, "GAME_SCREEN_LOG"}, KeyBinding{GAME_SCREEN_MAP, "GAME_SCREEN_MAP"},
    KeyBinding{GAME_LOOK_FP, "GAME_LOOK_FP"}, KeyBinding{GAME_LOCK_TARGET, "GAME_LOCK_TARGET"},
    KeyBinding{GAME_PARADE, "GAME_PARADE"}, KeyBinding{GAME_ACTIONLEFT, "GAME_ACTIONLEFT"},
    KeyBinding{GAME_ACTIONRIGHT, "GAME_ACTIONRIGHT"}, KeyBinding{GAME_LAME_POTION, "GAME_LAME_POTION"},
    KeyBinding{GAME_LAME_HEAL, "GAME_LAME_HEAL"},
};

// clang-format on

// Bind all key constants to Lua state
void BindInputConstants(sol::state& lua);

// Bind Cursor related functions to Lua state
void BindCursor(sol::state& lua);

}  // namespace gmp::gothic

/* luadoc (const)
 *
 * Escape key.
 *
 * @category Key
 * @side     client
 * @name     KEY_ESCAPE
 *
 */

/* luadoc (const)
 *
 * Number key 1.
 *
 * @category Key
 * @side     client
 * @name     KEY_1
 *
 */

/* luadoc (const)
 *
 * Number key 2.
 *
 * @category Key
 * @side     client
 * @name     KEY_2
 *
 */

/* luadoc (const)
 *
 * Number key 3.
 *
 * @category Key
 * @side     client
 * @name     KEY_3
 *
 */

/* luadoc (const)
 *
 * Number key 4.
 *
 * @category Key
 * @side     client
 * @name     KEY_4
 *
 */

/* luadoc (const)
 *
 * Number key 5.
 *
 * @category Key
 * @side     client
 * @name     KEY_5
 *
 */

/* luadoc (const)
 *
 * Number key 6.
 *
 * @category Key
 * @side     client
 * @name     KEY_6
 *
 */

/* luadoc (const)
 *
 * Number key 7.
 *
 * @category Key
 * @side     client
 * @name     KEY_7
 *
 */

/* luadoc (const)
 *
 * Number key 8.
 *
 * @category Key
 * @side     client
 * @name     KEY_8
 *
 */

/* luadoc (const)
 *
 * Number key 9.
 *
 * @category Key
 * @side     client
 * @name     KEY_9
 *
 */

/* luadoc (const)
 *
 * Number key 0.
 *
 * @category Key
 * @side     client
 * @name     KEY_0
 *
 */

/* luadoc (const)
 *
 * Minus key.
 *
 * @category Key
 * @side     client
 * @name     KEY_MINUS
 *
 */

/* luadoc (const)
 *
 * Equals key.
 *
 * @category Key
 * @side     client
 * @name     KEY_EQUALS
 *
 */

/* luadoc (const)
 *
 * Backspace key.
 *
 * @category Key
 * @side     client
 * @name     KEY_BACK
 *
 */

/* luadoc (const)
 *
 * Tab key.
 *
 * @category Key
 * @side     client
 * @name     KEY_TAB
 *
 */

/* luadoc (const)
 *
 * Letter key Q.
 *
 * @category Key
 * @side     client
 * @name     KEY_Q
 *
 */

/* luadoc (const)
 *
 * Letter key W.
 *
 * @category Key
 * @side     client
 * @name     KEY_W
 *
 */

/* luadoc (const)
 *
 * Letter key E.
 *
 * @category Key
 * @side     client
 * @name     KEY_E
 *
 */

/* luadoc (const)
 *
 * Letter key R.
 *
 * @category Key
 * @side     client
 * @name     KEY_R
 *
 */

/* luadoc (const)
 *
 * Letter key T.
 *
 * @category Key
 * @side     client
 * @name     KEY_T
 *
 */

/* luadoc (const)
 *
 * Letter key Y.
 *
 * @category Key
 * @side     client
 * @name     KEY_Y
 *
 */

/* luadoc (const)
 *
 * Letter key U.
 *
 * @category Key
 * @side     client
 * @name     KEY_U
 *
 */

/* luadoc (const)
 *
 * Letter key I.
 *
 * @category Key
 * @side     client
 * @name     KEY_I
 *
 */

/* luadoc (const)
 *
 * Letter key O.
 *
 * @category Key
 * @side     client
 * @name     KEY_O
 *
 */

/* luadoc (const)
 *
 * Letter key P.
 *
 * @category Key
 * @side     client
 * @name     KEY_P
 *
 */

/* luadoc (const)
 *
 * Left bracket key.
 *
 * @category Key
 * @side     client
 * @name     KEY_LBRACKET
 *
 */

/* luadoc (const)
 *
 * Right bracket key.
 *
 * @category Key
 * @side     client
 * @name     KEY_RBRACKET
 *
 */

/* luadoc (const)
 *
 * Enter / Return key.
 *
 * @category Key
 * @side     client
 * @name     KEY_RETURN
 *
 */

/* luadoc (const)
 *
 * Left Control key.
 *
 * @category Key
 * @side     client
 * @name     KEY_LCONTROL
 *
 */

/* luadoc (const)
 *
 * Letter key A.
 *
 * @category Key
 * @side     client
 * @name     KEY_A
 *
 */

/* luadoc (const)
 *
 * Letter key S.
 *
 * @category Key
 * @side     client
 * @name     KEY_S
 *
 */

/* luadoc (const)
 *
 * Letter key D.
 *
 * @category Key
 * @side     client
 * @name     KEY_D
 *
 */

/* luadoc (const)
 *
 * Letter key F.
 *
 * @category Key
 * @side     client
 * @name     KEY_F
 *
 */

/* luadoc (const)
 *
 * Letter key G.
 *
 * @category Key
 * @side     client
 * @name     KEY_G
 *
 */

/* luadoc (const)
 *
 * Letter key H.
 *
 * @category Key
 * @side     client
 * @name     KEY_H
 *
 */

/* luadoc (const)
 *
 * Letter key J.
 *
 * @category Key
 * @side     client
 * @name     KEY_J
 *
 */

/* luadoc (const)
 *
 * Letter key K.
 *
 * @category Key
 * @side     client
 * @name     KEY_K
 *
 */

/* luadoc (const)
 *
 * Letter key L.
 *
 * @category Key
 * @side     client
 * @name     KEY_L
 *
 */

/* luadoc (const)
 *
 * Semicolon key.
 *
 * @category Key
 * @side     client
 * @name     KEY_SEMICOLON
 *
 */

/* luadoc (const)
 *
 * Apostrophe key.
 *
 * @category Key
 * @side     client
 * @name     KEY_APOSTROPHE
 *
 */

/* luadoc (const)
 *
 * Grave accent key.
 *
 * @category Key
 * @side     client
 * @name     KEY_GRAVE
 *
 */

/* luadoc (const)
 *
 * Left Shift key.
 *
 * @category Key
 * @side     client
 * @name     KEY_LSHIFT
 *
 */

/* luadoc (const)
 *
 * Backslash key.
 *
 * @category Key
 * @side     client
 * @name     KEY_BACKSLASH
 *
 */

/* luadoc (const)
 *
 * Letter key Z.
 *
 * @category Key
 * @side     client
 * @name     KEY_Z
 *
 */

/* luadoc (const)
 *
 * Letter key X.
 *
 * @category Key
 * @side     client
 * @name     KEY_X
 *
 */

/* luadoc (const)
 *
 * Letter key C.
 *
 * @category Key
 * @side     client
 * @name     KEY_C
 *
 */

/* luadoc (const)
 *
 * Letter key V.
 *
 * @category Key
 * @side     client
 * @name     KEY_V
 *
 */

/* luadoc (const)
 *
 * Letter key B.
 *
 * @category Key
 * @side     client
 * @name     KEY_B
 *
 */

/* luadoc (const)
 *
 * Letter key N.
 *
 * @category Key
 * @side     client
 * @name     KEY_N
 *
 */

/* luadoc (const)
 *
 * Letter key M.
 *
 * @category Key
 * @side     client
 * @name     KEY_M
 *
 */

/* luadoc (const)
 *
 * Comma key.
 *
 * @category Key
 * @side     client
 * @name     KEY_COMMA
 *
 */

/* luadoc (const)
 *
 * Period key.
 *
 * @category Key
 * @side     client
 * @name     KEY_PERIOD
 *
 */

/* luadoc (const)
 *
 * Slash key.
 *
 * @category Key
 * @side     client
 * @name     KEY_SLASH
 *
 */

/* luadoc (const)
 *
 * Right Shift key.
 *
 * @category Key
 * @side     client
 * @name     KEY_RSHIFT
 *
 */

/* luadoc (const)
 *
 * Left Alt key.
 *
 * @category Key
 * @side     client
 * @name     KEY_LMENU
 *
 */

/* luadoc (const)
 *
 * Spacebar key.
 *
 * @category Key
 * @side     client
 * @name     KEY_SPACE
 *
 */

/* luadoc (const)
 *
 * Caps Lock key.
 *
 * @category Key
 * @side     client
 * @name     KEY_CAPITAL
 *
 */

/* luadoc (const)
 *
 * Function key F1.
 *
 * @category Key
 * @side     client
 * @name     KEY_F1
 *
 */

/* luadoc (const)
 *
 * Function key F2.
 *
 * @category Key
 * @side     client
 * @name     KEY_F2
 *
 */

/* luadoc (const)
 *
 * Function key F3.
 *
 * @category Key
 * @side     client
 * @name     KEY_F3
 *
 */

/* luadoc (const)
 *
 * Function key F4.
 *
 * @category Key
 * @side     client
 * @name     KEY_F4
 *
 */

/* luadoc (const)
 *
 * Function key F5.
 *
 * @category Key
 * @side     client
 * @name     KEY_F5
 *
 */

/* luadoc (const)
 *
 * Function key F6.
 *
 * @category Key
 * @side     client
 * @name     KEY_F6
 *
 */

/* luadoc (const)
 *
 * Function key F7.
 *
 * @category Key
 * @side     client
 * @name     KEY_F7
 *
 */

/* luadoc (const)
 *
 * Function key F8.
 *
 * @category Key
 * @side     client
 * @name     KEY_F8
 *
 */

/* luadoc (const)
 *
 * Function key F9.
 *
 * @category Key
 * @side     client
 * @name     KEY_F9
 *
 */

/* luadoc (const)
 *
 * Function key F10.
 *
 * @category Key
 * @side     client
 * @name     KEY_F10
 *
 */

/* luadoc (const)
 *
 * Function key F11.
 *
 * @category Key
 * @side     client
 * @name     KEY_F11
 *
 */

/* luadoc (const)
 *
 * Function key F12.
 *
 * @category Key
 * @side     client
 * @name     KEY_F12
 *
 */
/* luadoc (const)
 *
 * Mouse delta X movement.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_DX
 *
 */

/* luadoc (const)
 *
 * Mouse delta Y movement.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_DY
 *
 */

/* luadoc (const)
 *
 * Mouse movement up.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_UP
 *
 */

/* luadoc (const)
 *
 * Mouse movement down.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_DOWN
 *
 */

/* luadoc (const)
 *
 * Mouse left direction.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_LEFT
 *
 */

/* luadoc (const)
 *
 * Mouse right direction.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_RIGHT
 *
 */

/* luadoc (const)
 *
 * Mouse wheel scroll up.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_WHEELUP
 *
 */

/* luadoc (const)
 *
 * Mouse wheel scroll down.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_WHEELDOWN
 *
 */

/* luadoc (const)
 *
 * Mouse left button.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_BUTTONLEFT
 *
 */

/* luadoc (const)
 *
 * Mouse right button.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_BUTTONRIGHT
 *
 */

/* luadoc (const)
 *
 * Mouse middle button.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_BUTTONMID
 *
 */

/* luadoc (const)
 *
 * Mouse extra button 1.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_XBUTTON1
 *
 */

/* luadoc (const)
 *
 * Mouse extra button 2.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_XBUTTON2
 *
 */

/* luadoc (const)
 *
 * Mouse extra button 3.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_XBUTTON3
 *
 */

/* luadoc (const)
 *
 * Mouse extra button 4.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_XBUTTON4
 *
 */

/* luadoc (const)
 *
 * Mouse extra button 5.
 *
 * @category Mouse
 * @side     client
 * @name     MOUSE_XBUTTON5
 *
 */
/* luadoc (const)
 *
 * Move left action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LEFT
 *
 */

/* luadoc (const)
 *
 * Move right action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_RIGHT
 *
 */

/* luadoc (const)
 *
 * Move up action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_UP
 *
 */

/* luadoc (const)
 *
 * Move down action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_DOWN
 *
 */

/* luadoc (const)
 *
 * Primary action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_ACTION
 *
 */

/* luadoc (const)
 *
 * Slow movement / walk modifier.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SLOW
 *
 */

/* luadoc (const)
 *
 * Secondary action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_ACTION2
 *
 */

/* luadoc (const)
 *
 * Weapon action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_WEAPON
 *
 */

/* luadoc (const)
 *
 * Strafe movement.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SMOVE
 *
 */

/* luadoc (const)
 *
 * Alternate strafe movement.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SMOVE2
 *
 */

/* luadoc (const)
 *
 * Shift modifier.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SHIFT
 *
 */

/* luadoc (const)
 *
 * End action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_END
 *
 */

/* luadoc (const)
 *
 * Open inventory.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_INVENTORY
 *
 */

/* luadoc (const)
 *
 * Look mode.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LOOK
 *
 */

/* luadoc (const)
 *
 * Sneak mode.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SNEAK
 *
 */

/* luadoc (const)
 *
 * Strafe left.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_STRAFELEFT
 *
 */

/* luadoc (const)
 *
 * Strafe right.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_STRAFERIGHT
 *
 */

/* luadoc (const)
 *
 * Show status screen.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SCREEN_STATUS
 *
 */

/* luadoc (const)
 *
 * Show log screen.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SCREEN_LOG
 *
 */

/* luadoc (const)
 *
 * Show map screen.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_SCREEN_MAP
 *
 */

/* luadoc (const)
 *
 * First-person look.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LOOK_FP
 *
 */

/* luadoc (const)
 *
 * Lock target.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LOCK_TARGET
 *
 */

/* luadoc (const)
 *
 * Parry / block action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_PARADE
 *
 */

/* luadoc (const)
 *
 * Left-hand action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_ACTIONLEFT
 *
 */

/* luadoc (const)
 *
 * Right-hand action.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_ACTIONRIGHT
 *
 */

/* luadoc (const)
 *
 * Use potion.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LAME_POTION
 *
 */

/* luadoc (const)
 *
 * Use healing item.
 *
 * @category LogicalKey
 * @side     client
 * @name     GAME_LAME_HEAL
 *
 */

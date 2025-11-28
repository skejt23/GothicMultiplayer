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

#include "ZenGin/zGothicAPI.h"
#include <unordered_set>

static std::unordered_set<int> s_keysToggledThisFrame;
static std::unordered_set<int> s_keysPressedThisFrame;

namespace gmp::gothic {

extern bool s_prevPressed[MAX_KEYS_AND_CODES + 1];
extern bool s_pressedThisFrame[MAX_KEYS_AND_CODES + 1];
extern bool s_toggledThisFrame[MAX_KEYS_AND_CODES + 1];

    void ProcessInput(zCInput* zinput);

// All physical Keys
#define G2_KEY_LIST(X)                          \
    X(KEY_ESCAPE)                               \
    X(KEY_1)                                    \
    X(KEY_2)                                    \
    X(KEY_3)                                    \
    X(KEY_4)                                    \
    X(KEY_5)                                    \
    X(KEY_6)                                    \
    X(KEY_7)                                    \
    X(KEY_8)                                    \
    X(KEY_9)                                    \
    X(KEY_0)                                    \
    X(KEY_MINUS)                                \
    X(KEY_EQUALS)                               \
    X(KEY_BACK)                                 \
    X(KEY_TAB)                                  \
    X(KEY_Q)                                    \
    X(KEY_W)                                    \
    X(KEY_E)                                    \
    X(KEY_R)                                    \
    X(KEY_T)                                    \
    X(KEY_Y)                                    \
    X(KEY_U)                                    \
    X(KEY_I)                                    \
    X(KEY_O)                                    \
    X(KEY_P)                                    \
    X(KEY_LBRACKET)                             \
    X(KEY_RBRACKET)                             \
    X(KEY_RETURN)                               \
    X(KEY_LCONTROL)                             \
    X(KEY_A)                                    \
    X(KEY_S)                                    \
    X(KEY_D)                                    \
    X(KEY_F)                                    \
    X(KEY_G)                                    \
    X(KEY_H)                                    \
    X(KEY_J)                                    \
    X(KEY_K)                                    \
    X(KEY_L)                                    \
    X(KEY_SEMICOLON)                            \
    X(KEY_APOSTROPHE)                           \
    X(KEY_GRAVE)                                \
    X(KEY_LSHIFT)                               \
    X(KEY_BACKSLASH)                            \
    X(KEY_Z)                                    \
    X(KEY_X)                                    \
    X(KEY_C)                                    \
    X(KEY_V)                                    \
    X(KEY_B)                                    \
    X(KEY_N)                                    \
    X(KEY_M)                                    \
    X(KEY_COMMA)                                \
    X(KEY_PERIOD)                               \
    X(KEY_SLASH)                                \
    X(KEY_RSHIFT)                               \
    X(KEY_MULTIPLY)                             \
    X(KEY_LMENU)                                \
    X(KEY_SPACE)                                \
    X(KEY_CAPITAL)                              \
    X(KEY_F1)                                   \
    X(KEY_F2)                                   \
    X(KEY_F3)                                   \
    X(KEY_F4)                                   \
    X(KEY_F5)                                   \
    X(KEY_F6)                                   \
    X(KEY_F7)                                   \
    X(KEY_F8)                                   \
    X(KEY_F9)                                   \
    X(KEY_F10)                                  \
    X(KEY_NUMLOCK)                              \
    X(KEY_SCROLL)                               \
    X(KEY_NUMPAD7)                              \
    X(KEY_NUMPAD8)                              \
    X(KEY_NUMPAD9)                              \
    X(KEY_SUBTRACT)                             \
    X(KEY_NUMPAD4)                              \
    X(KEY_NUMPAD5)                              \
    X(KEY_NUMPAD6)                              \
    X(KEY_ADD)                                  \
    X(KEY_NUMPAD1)                              \
    X(KEY_NUMPAD2)                              \
    X(KEY_NUMPAD3)                              \
    X(KEY_NUMPAD0)                              \
    X(KEY_DECIMAL)                              \
    X(KEY_OEM_102)                              \
    X(KEY_F11)                                  \
    X(KEY_F12)                                  \
    X(KEY_F13)                                  \
    X(KEY_F14)                                  \
    X(KEY_F15)                                  \
    X(KEY_KANA)                                 \
    X(KEY_ABNT_C1)                              \
    X(KEY_CONVERT)                              \
    X(KEY_NOCONVERT)                            \
    X(KEY_YEN)                                  \
    X(KEY_ABNT_C2)                              \
    X(KEY_NUMPADEQUALS)                         \
    X(KEY_PREVTRACK)                            \
    X(KEY_AT)                                   \
    X(KEY_COLON)                                \
    X(KEY_UNDERLINE)                            \
    X(KEY_KANJI)                                \
    X(KEY_STOP)                                 \
    X(KEY_AX)                                   \
    X(KEY_UNLABELED)                            \
    X(KEY_NEXTTRACK)                            \
    X(KEY_NUMPADENTER)                          \
    X(KEY_RCONTROL)                             \
    X(KEY_MUTE)                                 \
    X(KEY_CALCULATOR)                           \
    X(KEY_PLAYPAUSE)                            \
    X(KEY_MEDIASTOP)                            \
    X(KEY_VOLUMEDOWN)                           \
    X(KEY_VOLUMEUP)                             \
    X(KEY_WEBHOME)                              \
    X(KEY_NUMPADCOMMA)                          \
    X(KEY_DIVIDE)                               \
    X(KEY_SYSRQ)                                \
    X(KEY_RMENU)                                \
    X(KEY_PAUSE)                                \
    X(KEY_HOME)                                 \
    X(KEY_UP)                                   \
    X(KEY_PRIOR)                                \
    X(KEY_LEFT)                                 \
    X(KEY_RIGHT)                                \
    X(KEY_END)                                  \
    X(KEY_DOWN)                                 \
    X(KEY_NEXT)                                 \
    X(KEY_INSERT)                               \
    X(KEY_DELETE)                               \
    X(KEY_LWIN)                                 \
    X(KEY_RWIN)                                 \
    X(KEY_APPS)                                 \
    X(KEY_POWER)                                \
    X(KEY_SLEEP)                                \
    X(KEY_WAKE)                                 \
    X(KEY_WEBSEARCH)                            \
    X(KEY_WEBFAVORITES)                         \
    X(KEY_WEBREFRESH)                           \
    X(KEY_WEBSTOP)                              \
    X(KEY_WEBFORWARD)                           \
    X(KEY_WEBBACK)                              \
    X(KEY_MYCOMPUTER)                           \
    X(KEY_MAIL)                                 \
    X(KEY_MEDIASELECT)                          \
    /* aliases */                               \
    X(KEY_BACKSPACE)                            \
    X(KEY_NUMPADSTAR)                           \
    X(KEY_LALT)                                 \
    X(KEY_CAPSLOCK)                             \
    X(KEY_NUMPADMINUS)                          \
    X(KEY_NUMPADPLUS)                           \
    X(KEY_NUMPADPERIOD)                         \
    X(KEY_NUMPADSLASH)                          \
    X(KEY_RALT)                                 \
    X(KEY_UPARROW)                              \
    X(KEY_PGUP)                                 \
    X(KEY_LEFTARROW)                            \
    X(KEY_RIGHTARROW)                           \
    X(KEY_DOWNARROW)                            \
    X(KEY_PGDN)                                 \
    X(KEY_CIRCUMFLEX)

// Mouse
#define G2_MOUSE_LIST(X)                        \
    X(MOUSE_DX)                                 \
    X(MOUSE_DY)                                 \
    X(MOUSE_UP)                                 \
    X(MOUSE_DOWN)                               \
    X(MOUSE_LEFT)                               \
    X(MOUSE_RIGHT)                              \
    X(MOUSE_WHEELUP)                            \
    X(MOUSE_WHEELDOWN)                          \
    X(MOUSE_BUTTONLEFT)                         \
    X(MOUSE_BUTTONRIGHT)                        \
    X(MOUSE_BUTTONMID)                          \
    X(MOUSE_XBUTTON1)                           \
    X(MOUSE_XBUTTON2)                           \
    X(MOUSE_XBUTTON3)                           \
    X(MOUSE_XBUTTON4)                           \
    X(MOUSE_XBUTTON5)

// Game/LogicalKey
#define G2_GAME_LIST(X)                         \
    X(GAME_LEFT)                                \
    X(GAME_RIGHT)                               \
    X(GAME_UP)                                  \
    X(GAME_DOWN)                                \
    X(GAME_ACTION)                              \
    X(GAME_SLOW)                                \
    X(GAME_ACTION2)                             \
    X(GAME_WEAPON)                              \
    X(GAME_SMOVE)                               \
    X(GAME_SMOVE2)                              \
    X(GAME_SHIFT)                               \
    X(GAME_END)                                 \
    X(GAME_INVENTORY)                           \
    X(GAME_LOOK)                                \
    X(GAME_SNEAK)                               \
    X(GAME_STRAFELEFT)                          \
    X(GAME_STRAFERIGHT)                         \
    X(GAME_SCREEN_STATUS)                       \
    X(GAME_SCREEN_LOG)                          \
    X(GAME_SCREEN_MAP)                          \
    X(GAME_LOOK_FP)                             \
    X(GAME_LOCK_TARGET)                         \
    X(GAME_PARADE)                              \
    X(GAME_ACTIONLEFT)                          \
    X(GAME_ACTIONRIGHT)                         \
    X(GAME_LAME_POTION)                         \
    X(GAME_LAME_HEAL)

// Combine all physical codes
#define G2_ALL_PHYSICAL_CODES(X) \
        G2_KEY_LIST(X)               \
        G2_MOUSE_LIST(X)

static constexpr int kAllPhysicalCodes[] = {
#define CODE(name) name,
        G2_ALL_PHYSICAL_CODES(CODE)
#undef CODE
};

static constexpr size_t kAllPhysicalCodeCount = sizeof(kAllPhysicalCodes) / sizeof(int);

} // namespace gmp::gothic
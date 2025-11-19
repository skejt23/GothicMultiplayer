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

#include "scripting/gothic_bindings.h"

#include <spdlog/spdlog.h>

#include "ZenGin/zGothicAPI.h"

namespace gmp::gothic {

void BindInput(sol::state& lua) {
  lua.set_function("KeyToggled", [](int key) -> bool { return zinput && zinput->KeyToggled(key); });

  sol::table key = lua.create_named_table("Key");
  key["ESCAPE"] = KEY_ESCAPE;
  key["1"] = KEY_1;
  key["2"] = KEY_2;
  key["3"] = KEY_3;
  key["4"] = KEY_4;
  key["5"] = KEY_5;
  key["6"] = KEY_6;
  key["7"] = KEY_7;
  key["8"] = KEY_8;
  key["9"] = KEY_9;
  key["0"] = KEY_0;
  key["MINUS"] = KEY_MINUS;
  key["EQUALS"] = KEY_EQUALS;
  key["BACK"] = KEY_BACK;
  key["TAB"] = KEY_TAB;
  key["Q"] = KEY_Q;
  key["W"] = KEY_W;
  key["E"] = KEY_E;
  key["R"] = KEY_R;
  key["T"] = KEY_T;
  key["Y"] = KEY_Y;
  key["U"] = KEY_U;
  key["I"] = KEY_I;
  key["O"] = KEY_O;
  key["P"] = KEY_P;
  key["LBRACKET"] = KEY_LBRACKET;
  key["RBRACKET"] = KEY_RBRACKET;
  key["RETURN"] = KEY_RETURN;
  key["LCONTROL"] = KEY_LCONTROL;
  key["A"] = KEY_A;
  key["S"] = KEY_S;
  key["D"] = KEY_D;
  key["F"] = KEY_F;
  key["G"] = KEY_G;
  key["H"] = KEY_H;
  key["J"] = KEY_J;
  key["K"] = KEY_K;
  key["L"] = KEY_L;
  key["SEMICOLON"] = KEY_SEMICOLON;
  key["APOSTROPHE"] = KEY_APOSTROPHE;
  key["GRAVE"] = KEY_GRAVE;
  key["LSHIFT"] = KEY_LSHIFT;
  key["BACKSLASH"] = KEY_BACKSLASH;
  key["Z"] = KEY_Z;
  key["X"] = KEY_X;
  key["C"] = KEY_C;
  key["V"] = KEY_V;
  key["B"] = KEY_B;
  key["N"] = KEY_N;
  key["M"] = KEY_M;
  key["COMMA"] = KEY_COMMA;
  key["PERIOD"] = KEY_PERIOD;
  key["SLASH"] = KEY_SLASH;
  key["RSHIFT"] = KEY_RSHIFT;
  key["MULTIPLY"] = KEY_MULTIPLY;
  key["LMENU"] = KEY_LMENU;
  key["SPACE"] = KEY_SPACE;
  key["CAPITAL"] = KEY_CAPITAL;
  key["F1"] = KEY_F1;
  key["F2"] = KEY_F2;
  key["F3"] = KEY_F3;
  key["F4"] = KEY_F4;
  key["F5"] = KEY_F5;
  key["F6"] = KEY_F6;
  key["F7"] = KEY_F7;
  key["F8"] = KEY_F8;
  key["F9"] = KEY_F9;
  key["F10"] = KEY_F10;
  key["NUMLOCK"] = KEY_NUMLOCK;
  key["SCROLL"] = KEY_SCROLL;
  key["NUMPAD7"] = KEY_NUMPAD7;
  key["NUMPAD8"] = KEY_NUMPAD8;
  key["NUMPAD9"] = KEY_NUMPAD9;
  key["SUBTRACT"] = KEY_SUBTRACT;
  key["NUMPAD4"] = KEY_NUMPAD4;
  key["NUMPAD5"] = KEY_NUMPAD5;
  key["NUMPAD6"] = KEY_NUMPAD6;
  key["ADD"] = KEY_ADD;
  key["NUMPAD1"] = KEY_NUMPAD1;
  key["NUMPAD2"] = KEY_NUMPAD2;
  key["NUMPAD3"] = KEY_NUMPAD3;
  key["NUMPAD0"] = KEY_NUMPAD0;
  key["DECIMAL"] = KEY_DECIMAL;
  key["F11"] = KEY_F11;
  key["F12"] = KEY_F12;
}

void BindGothicSpecific(sol::state& lua) {
  SPDLOG_TRACE("Initializing Gothic 2 Addon 2.6 specific bindings...");

  BindInput(lua);
}

}  // namespace gmp::gothic

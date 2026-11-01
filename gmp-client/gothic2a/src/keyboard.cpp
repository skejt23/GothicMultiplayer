
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include "keyboard.h"

#include "language.h"

#include <array>

bool UseCyrillicLayout(bool ignorekeyboardlayout) {
  if (ignorekeyboardlayout)
    return false;

  return Language::Instance().GetEncoding() == localization::LanguageEncoding::kCp1251;
}

namespace {
char TranslateSystemKey(int key, UINT code_page) {
  if (key == KEY_RETURN || key == KEY_NUMPADENTER)
    return 0x0D;
  if (key == KEY_TAB)
    return ' ';
  if (key <= 0 || key > 0xFF)
    return 0;
  const auto layout = GetKeyboardLayout(0);
  const UINT scan_code = (key & 0x80) ? (0xE000u | (key & 0x7F)) : static_cast<UINT>(key);
  const UINT virtual_key = MapVirtualKeyExW(scan_code, MAPVK_VSC_TO_VK_EX, layout);
  if (!virtual_key || virtual_key > 0xFF)
    return 0;
  std::array<BYTE, 256> state{};
  auto modifier = [&](int left, int right, int combined, int left_key, int right_key) {
    state[left] = zinput->KeyPressed(left_key) ? 0x80 : 0;
    state[right] = zinput->KeyPressed(right_key) ? 0x80 : 0;
    state[combined] = state[left] | state[right];
  };
  modifier(VK_LSHIFT, VK_RSHIFT, VK_SHIFT, KEY_LSHIFT, KEY_RSHIFT);
  modifier(VK_LCONTROL, VK_RCONTROL, VK_CONTROL, KEY_LCONTROL, KEY_RCONTROL);
  modifier(VK_LMENU, VK_RMENU, VK_MENU, KEY_LALT, KEY_RALT);
  // DirectInput may expose only Right Alt, while Windows layouts expect the
  // synthetic Ctrl+Alt state used by AltGr (e.g. Polish Programmer's layout).
  if (state[VK_RMENU])
    state[VK_LCONTROL] = state[VK_CONTROL] = 0x80;
  state[VK_CAPITAL] = GetKeyState(VK_CAPITAL) & 1;
  state[VK_NUMLOCK] = GetKeyState(VK_NUMLOCK) & 1;
  state[virtual_key] |= 0x80;
  std::array<WCHAR, 8> wide{};
  // Flag 4 keeps the translator from changing Windows' shared dead-key state.
  const int count = ToUnicodeEx(virtual_key, scan_code, state.data(), wide.data(), static_cast<int>(wide.size()), 4, layout);
  if (count != 1)
    return 0;
  if (!code_page) {
    code_page = 1250;
    switch (Language::Instance().GetEncoding()) {
      case localization::LanguageEncoding::kCp1251:
        code_page = 1251;
        break;
      case localization::LanguageEncoding::kCp1252:
        code_page = 1252;
        break;
      case localization::LanguageEncoding::kCp1254:
        code_page = 1254;
        break;
      default:
        break;
    }
  }
  char character = 0;
  BOOL substituted = FALSE;
  if (WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, wide.data(), 1, &character, 1, nullptr, &substituted) != 1 || substituted)
    return 0;
  return character;
}

unsigned char TranslateKey(int key, bool with_shift, bool use_cyrillic) {
  if (key == KEY_RETURN || key == KEY_NUMPADENTER)
    return 0x0D;
  if (!use_cyrillic) {
    switch (key) {
      case KEY_0:
        return (with_shift) ? ')' : '0';
      case KEY_1:
        return (with_shift) ? '!' : '1';
      case KEY_2:
        return (with_shift) ? '@' : '2';
      case KEY_3:
        return (with_shift) ? '#' : '3';
      case KEY_4:
        return (with_shift) ? '$' : '4';
      case KEY_5:
        return (with_shift) ? '%' : '5';
      case KEY_6:
        return (with_shift) ? '^' : '6';
      case KEY_7:
        return (with_shift) ? '&' : '7';
      case KEY_8:
        return (with_shift) ? '*' : '8';
      case KEY_9:
        return (with_shift) ? '(' : '9';
      case KEY_MINUS:
        return (with_shift) ? '_' : '-';
      case KEY_EQUALS:
        return (with_shift) ? '+' : '=';
      case KEY_BACK:
        return 0x08;
      case KEY_TAB:
        return ' ';
      case KEY_Q:
        return (with_shift) ? 'Q' : 'q';
      case KEY_W:
        return (with_shift) ? 'W' : 'w';
      case KEY_E:
        return (with_shift) ? 'E' : 'e';
      case KEY_R:
        return (with_shift) ? 'R' : 'r';
      case KEY_T:
        return (with_shift) ? 'T' : 't';
      case KEY_Y:
        return (with_shift) ? 'Y' : 'y';
      case KEY_U:
        return (with_shift) ? 'U' : 'u';
      case KEY_I:
        return (with_shift) ? 'I' : 'i';
      case KEY_O:
        return (with_shift) ? 'O' : 'o';
      case KEY_P:
        return (with_shift) ? 'P' : 'p';
      case KEY_LBRACKET:
        return (with_shift) ? '{' : '[';
      case KEY_RBRACKET:
        return (with_shift) ? '}' : ']';
      case KEY_A:
        return (with_shift) ? 'A' : 'a';
      case KEY_S:
        return (with_shift) ? 'S' : 's';
      case KEY_D:
        return (with_shift) ? 'D' : 'd';
      case KEY_F:
        return (with_shift) ? 'F' : 'f';
      case KEY_G:
        return (with_shift) ? 'G' : 'g';
      case KEY_H:
        return (with_shift) ? 'H' : 'h';
      case KEY_J:
        return (with_shift) ? 'J' : 'j';
      case KEY_K:
        return (with_shift) ? 'K' : 'k';
      case KEY_L:
        return (with_shift) ? 'L' : 'l';
      case KEY_SEMICOLON:
        return (with_shift) ? ':' : ';';
      case KEY_APOSTROPHE:
        return (with_shift) ? 0x22 : 0x27;
      case KEY_GRAVE:
        return (with_shift) ? '~' : '`';
      case KEY_BACKSLASH:
        return (with_shift) ? '|' : '\\';
      case KEY_Z:
        return (with_shift) ? 'Z' : 'z';
      case KEY_X:
        return (with_shift) ? 'X' : 'x';
      case KEY_C:
        return (with_shift) ? 'C' : 'c';
      case KEY_V:
        return (with_shift) ? 'V' : 'v';
      case KEY_B:
        return (with_shift) ? 'B' : 'b';
      case KEY_N:
        return (with_shift) ? 'N' : 'n';
      case KEY_M:
        return (with_shift) ? 'M' : 'm';
      case KEY_COMMA:
        return (with_shift) ? '<' : ',';
      case KEY_PERIOD:
        return (with_shift) ? '>' : '.';
      case KEY_SLASH:
        return (with_shift) ? '?' : '/';
      case KEY_SPACE:
        return ' ';
      case KEY_NUMPAD1:
        return '1';
      case KEY_NUMPAD2:
        return '2';
      case KEY_NUMPAD3:
        return '3';
      case KEY_NUMPAD4:
        return '4';
      case KEY_NUMPAD5:
        return '5';
      case KEY_NUMPAD6:
        return '6';
      case KEY_NUMPAD7:
        return '7';
      case KEY_NUMPAD8:
        return '8';
      case KEY_NUMPAD9:
        return '9';
      case KEY_NUMPAD0:
        return '0';
      case KEY_DECIMAL:
        return '.';
      default:
        return 0;
    }
  }
  switch (key) {
    case KEY_0:
      return (with_shift) ? ')' : '0';
    case KEY_1:
      return (with_shift) ? '!' : '1';
    case KEY_2:
      return (with_shift) ? 0x22 : '2';
    case KEY_3:
      return (with_shift) ? 0xB9 : '3';
    case KEY_4:
      return (with_shift) ? 0x3B : '4';
    case KEY_5:
      return (with_shift) ? 0x25 : '5';
    case KEY_6:
      return (with_shift) ? 0x3A : '6';
    case KEY_7:
      return (with_shift) ? 0x3F : '7';
    case KEY_8:
      return (with_shift) ? '*' : '8';
    case KEY_9:
      return (with_shift) ? '(' : '9';
    case KEY_MINUS:
      return (with_shift) ? '_' : '-';
    case KEY_EQUALS:
      return (with_shift) ? '+' : '=';
    case KEY_BACK:
      return 0x08;
    case KEY_TAB:
      return ' ';
    case KEY_Q:
      return (with_shift) ? '\xC9' : '\xE9';
    case KEY_W:
      return (with_shift) ? 0xD6 : 0xF6;
    case KEY_E:
      return (with_shift) ? 0xD3 : 0xF3;
    case KEY_R:
      return (with_shift) ? 0xCA : 0xEA;
    case KEY_T:
      return (with_shift) ? 0xC5 : 0xE5;
    case KEY_Y:
      return (with_shift) ? 0xCD : 0xED;
    case KEY_U:
      return (with_shift) ? 0xC3 : 0xE3;
    case KEY_I:
      return (with_shift) ? 0xD8 : 0xF8;
    case KEY_O:
      return (with_shift) ? 0xD9 : 0xF9;
    case KEY_P:
      return (with_shift) ? 0xC7 : 0xE7;
    case KEY_LBRACKET:
      return (with_shift) ? 0xD5 : 0xF5;
    case KEY_RBRACKET:
      return (with_shift) ? 0xDA : 0xFA;
    case KEY_A:
      return (with_shift) ? 0xD4 : 0xF4;
    case KEY_S:
      return (with_shift) ? 0xDB : 0xFB;
    case KEY_D:
      return (with_shift) ? 0xC2 : 0xE2;
    case KEY_F:
      return (with_shift) ? 0xC0 : 0xE0;
    case KEY_G:
      return (with_shift) ? 0xCF : 0xEF;
    case KEY_H:
      return (with_shift) ? 0xD0 : 0xF0;
    case KEY_J:
      return (with_shift) ? 0xCE : 0xEE;
    case KEY_K:
      return (with_shift) ? 0xCB : 0xEB;
    case KEY_L:
      return (with_shift) ? 0xC4 : 0xE4;
    case KEY_SEMICOLON:
      return (with_shift) ? 0xC6 : 0xE6;
    case KEY_APOSTROPHE:
      return (with_shift) ? 0xDD : 0xFD;
    case KEY_GRAVE:
      return (with_shift) ? 0xA8 : 0xB8;
    case KEY_BACKSLASH:
      return (with_shift) ? 0x2F : 0x5C;
    case KEY_Z:
      return (with_shift) ? 0xDF : 0xFF;
    case KEY_X:
      return (with_shift) ? 0xD7 : 0xF7;
    case KEY_C:
      return (with_shift) ? 0xD1 : 0xF1;
    case KEY_V:
      return (with_shift) ? 0xCC : 0xEC;
    case KEY_B:
      return (with_shift) ? 0xC8 : 0xE8;
    case KEY_N:
      return (with_shift) ? 0xD2 : 0xF2;
    case KEY_M:
      return (with_shift) ? 0xDC : 0xFC;
    case KEY_COMMA:
      return (with_shift) ? 0xC1 : 0xE1;
    case KEY_PERIOD:
      return (with_shift) ? 0xDE : 0xFE;
    case KEY_SLASH:
      return (with_shift) ? 0x2C : 0x2E;
    case KEY_SPACE:
      return ' ';
    case KEY_NUMPAD1:
      return '1';
    case KEY_NUMPAD2:
      return '2';
    case KEY_NUMPAD3:
      return '3';
    case KEY_NUMPAD4:
      return '4';
    case KEY_NUMPAD5:
      return '5';
    case KEY_NUMPAD6:
      return '6';
    case KEY_NUMPAD7:
      return '7';
    case KEY_NUMPAD8:
      return '8';
    case KEY_NUMPAD9:
      return '9';
    case KEY_NUMPAD0:
      return '0';
    case KEY_DECIMAL:
      return '.';
    default:
      return 0;
  }
}
}  // namespace

char GInput::TranslateKeyFromKeyboard(int key, bool ignorekeyboardlayout, unsigned int code_page) {
  if (!zinput)
    return 0;
  // Keep the explicit Latin-only fields and the legacy Cyrillic fallback.
  // Other text follows the actual Windows keyboard layout, including AltGr.
  if (!ignorekeyboardlayout && !UseCyrillicLayout(false))
    return TranslateSystemKey(key, code_page);
  const bool shift = zinput->KeyPressed(KEY_LSHIFT) || zinput->KeyPressed(KEY_RSHIFT);
  const bool cyrillic = UseCyrillicLayout(ignorekeyboardlayout);
  unsigned char ch = TranslateKey(key, shift, cyrillic);
  // Caps Lock affects letters only; Shift reverses it for both supported alphabets.
  if (GetKeyState(VK_CAPITAL) & 1) {
    if (ch >= 'a' && ch <= 'z')
      ch -= 'a' - 'A';
    else if (ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';
    else if (cyrillic && ch >= 0xC0)
      ch ^= 0x20;
    else if (cyrillic && (ch == 0xA8 || ch == 0xB8))
      ch ^= 0x10;
  }
  return static_cast<char>(ch);
}

bool GInput::IsControlShortcutPressed() {
  return zinput && (zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL)) && !zinput->KeyPressed(KEY_RALT) &&
         !zinput->KeyPressed(KEY_LALT);
}

char GInput::GetCharacterFormKeyboard(bool ignorekeyboardlayout) {
  if (!zinput)
    return 0;
  for (int key = KEY_1; key <= KEY_DECIMAL; ++key) {
    if (zinput->KeyToggled(key)) {
      if (const char ch = TranslateKeyFromKeyboard(key, ignorekeyboardlayout))
        return ch;
    }
  }
  return 0;
}

char GInput::GetNumberCharacterFromKeyboard() {
  zINT i_keyboard = KEY_1;
  do {
    if (zinput->KeyToggled(i_keyboard)) {
      switch (i_keyboard) {
        case KEY_0:
          return '0';
          break;
        case KEY_1:
          return '1';
          break;
        case KEY_2:
          return '2';
          break;
        case KEY_3:
          return '3';
          break;
        case KEY_4:
          return '4';
          break;
        case KEY_5:
          return '5';
          break;
        case KEY_6:
          return '6';
          break;
        case KEY_7:
          return '7';
          break;
        case KEY_8:
          return '8';
          break;
        case KEY_9:
          return '9';
          break;
        case KEY_NUMPAD1:
          return '1';
          break;
        case KEY_NUMPAD2:
          return '2';
          break;
        case KEY_NUMPAD3:
          return '3';
          break;
        case KEY_NUMPAD4:
          return '4';
          break;
        case KEY_NUMPAD5:
          return '5';
          break;
        case KEY_NUMPAD6:
          return '6';
          break;
        case KEY_NUMPAD7:
          return '7';
          break;
        case KEY_NUMPAD8:
          return '8';
          break;
        case KEY_NUMPAD9:
          return '9';
          break;
        case KEY_NUMPAD0:
          return '0';
          break;
        case KEY_BACK:
          return 0x08;
          break;
        default:
          return 0;
      }
    }
  } while ((++i_keyboard) < 0x56);
  return 0;
};


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

#include "CConfig.h"

enum KeyboardLayout { KEYBOARD_LATIN, KEYBOARD_CYRYLLIC };

char GInput::GetCharacterFormKeyboard(bool ignorekeyboardlayout) {
  bool with_shift = false;
  if ((zinput->KeyPressed(KEY_RSHIFT)) || (zinput->KeyPressed(KEY_LSHIFT)))
    with_shift = true;
  if (zinput->KeyPressed(KEY_RETURN)) {
    zinput->ClearKeyBuffer();
    return 0x0D;
  }
  if (zinput->KeyPressed(KEY_SPACE)) {
    zinput->ClearKeyBuffer();
    return ' ';
  }
  if (zinput->KeyPressed(KEY_X)) {
    zinput->ClearKeyBuffer();
    if (CConfig::GetInstance()->keyboardlayout == KEYBOARD_LATIN || ignorekeyboardlayout)
      return (with_shift) ? 'X' : 'x';
    return (with_shift) ? 0xD7 : 0xF7;
  } else if (zinput->KeyPressed(KEY_F)) {
    zinput->ClearKeyBuffer();
    if (CConfig::GetInstance()->keyboardlayout == KEYBOARD_LATIN || ignorekeyboardlayout)
      return (with_shift) ? 'F' : 'f';
    return (with_shift) ? 0xC0 : 0xE0;
  }
  zINT i_keyboard = KEY_1;
  if (ignorekeyboardlayout)
    goto IGNORETHISCRAP;
  switch (CConfig::GetInstance()->keyboardlayout) {
    case KEYBOARD_LATIN:
    IGNORETHISCRAP:
      do {
        if (zinput->KeyToggled(i_keyboard)) {
          switch (i_keyboard) {
            case KEY_0:
              return (with_shift) ? ')' : '0';
              break;
            case KEY_1:
              return (with_shift) ? '!' : '1';
              break;
            case KEY_2:
              return (with_shift) ? '@' : '2';
              break;
            case KEY_3:
              return (with_shift) ? '#' : '3';
              break;
            case KEY_4:
              return (with_shift) ? '$' : '4';
              break;
            case KEY_5:
              return (with_shift) ? '%' : '5';
              break;
            case KEY_6:
              return (with_shift) ? '^' : '6';
              break;
            case KEY_7:
              return (with_shift) ? '&' : '7';
              break;
            case KEY_8:
              return (with_shift) ? '*' : '8';
              break;
            case KEY_9:
              return (with_shift) ? '(' : '9';
              break;
            case KEY_MINUS:
              return (with_shift) ? '_' : '-';
              break;
            case KEY_EQUALS:
              return (with_shift) ? '+' : '=';
              break;
            case KEY_BACK:
              return 0x08;
              break;
            case KEY_TAB:
              return ' ';
              break;
            case KEY_Q:
              return (with_shift) ? 'Q' : 'q';
              break;
            case KEY_W:
              return (with_shift) ? 'W' : 'w';
              break;
            case KEY_E:
              return (with_shift) ? 'E' : 'e';
              break;
            case KEY_R:
              return (with_shift) ? 'R' : 'r';
              break;
            case KEY_T:
              return (with_shift) ? 'T' : 't';
              break;
            case KEY_Y:
              return (with_shift) ? 'Y' : 'y';
              break;
            case KEY_U:
              return (with_shift) ? 'U' : 'u';
              break;
            case KEY_I:
              return (with_shift) ? 'I' : 'i';
              break;
            case KEY_O:
              return (with_shift) ? 'O' : 'o';
              break;
            case KEY_P:
              return (with_shift) ? 'P' : 'p';
              break;
            case KEY_LBRACKET:
              return (with_shift) ? '{' : '[';
              break;
            case KEY_RBRACKET:
              return (with_shift) ? '}' : ']';
              break;
            // case KEY_: return (with_shift)?'':'';break;
            // case KEY_: return (with_shift)?'':'';break;
            case KEY_A:
              return (with_shift) ? 'A' : 'a';
              break;
            case KEY_S:
              return (with_shift) ? 'S' : 's';
              break;
            case KEY_D:
              return (with_shift) ? 'D' : 'd';
              break;
            case KEY_F:
              return (with_shift) ? 'F' : 'f';
              break;
            case KEY_G:
              return (with_shift) ? 'G' : 'g';
              break;
            case KEY_H:
              return (with_shift) ? 'H' : 'h';
              break;
            case KEY_J:
              return (with_shift) ? 'J' : 'j';
              break;
            case KEY_K:
              return (with_shift) ? 'K' : 'k';
              break;
            case KEY_L:
              return (with_shift) ? 'L' : 'l';
              break;
            case KEY_SEMICOLON:
              return (with_shift) ? ':' : ';';
              break;
            case KEY_APOSTROPHE:
              return (with_shift) ? '\x22' : '\x27';
              break;
            case KEY_GRAVE:
              return (with_shift) ? '~' : '`';
              break;
            // case KEY_: return (with_shift)?'':'';break;
            case KEY_BACKSLASH:
              return (with_shift) ? '|' : '\\';
              break;
            case KEY_Z:
              return (with_shift) ? 'Z' : 'z';
              break;
            case KEY_X:
              return (with_shift) ? 'X' : 'x';
              break;
            case KEY_C:
              return (with_shift) ? 'C' : 'c';
              break;
            case KEY_V:
              return (with_shift) ? 'V' : 'v';
              break;
            case KEY_B:
              return (with_shift) ? 'B' : 'b';
              break;
            case KEY_N:
              return (with_shift) ? 'N' : 'n';
              break;
            case KEY_M:
              return (with_shift) ? 'M' : 'm';
              break;
            case KEY_COMMA:
              return (with_shift) ? '<' : ',';
              break;
            case KEY_PERIOD:
              return (with_shift) ? '>' : '.';
              break;
            case KEY_SLASH:
              return (with_shift) ? '?' : '/';
              break;
            case KEY_SPACE:
              return ' ';
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
            case KEY_DECIMAL:
              return '.';
              break;
            default:
              return 0;
          }
        }
      } while ((++i_keyboard) < 0x56);
      break;
    case KEYBOARD_CYRYLLIC:
      do {
        if (zinput->KeyToggled(i_keyboard)) {
          switch (i_keyboard) {
            case KEY_0:
              return (with_shift) ? ')' : '0';
              break;
            case KEY_1:
              return (with_shift) ? '!' : '1';
              break;
            case KEY_2:
              return (with_shift) ? 0x22 : '2';
              break;
            case KEY_3:
              return (with_shift) ? 0xB9 : '3';
              break;
            case KEY_4:
              return (with_shift) ? 0x3B : '4';
              break;
            case KEY_5:
              return (with_shift) ? 0x25 : '5';
              break;
            case KEY_6:
              return (with_shift) ? 0x3A : '6';
              break;
            case KEY_7:
              return (with_shift) ? 0x3F : '7';
              break;
            case KEY_8:
              return (with_shift) ? '*' : '8';
              break;
            case KEY_9:
              return (with_shift) ? '(' : '9';
              break;
            case KEY_MINUS:
              return (with_shift) ? '_' : '-';
              break;
            case KEY_EQUALS:
              return (with_shift) ? '+' : '=';
              break;
            case KEY_BACK:
              return 0x08;
              break;
            case KEY_TAB:
              return ' ';
              break;
            case KEY_Q:
              return (with_shift) ? '\xC9' : '\xE9';
              break;
            case KEY_W:
              return (with_shift) ? 0xD6 : 0xF6;
              break;
            case KEY_E:
              return (with_shift) ? 0xD3 : 0xF3;
              break;
            case KEY_R:
              return (with_shift) ? 0xCA : 0xEA;
              break;
            case KEY_T:
              return (with_shift) ? 0xC5 : 0xE5;
              break;
            case KEY_Y:
              return (with_shift) ? 0xCD : 0xED;
              break;
            case KEY_U:
              return (with_shift) ? 0xC3 : 0xE3;
              break;
            case KEY_I:
              return (with_shift) ? 0xD8 : 0xF8;
              break;
            case KEY_O:
              return (with_shift) ? 0xD9 : 0xF9;
              break;
            case KEY_P:
              return (with_shift) ? 0xC7 : 0xE7;
              break;
            case KEY_LBRACKET:
              return (with_shift) ? 0xD5 : 0xF5;
              break;
            case KEY_RBRACKET:
              return (with_shift) ? 0xDA : 0xFA;
              break;
            // case KEY_: return (with_shift)?'':'';break;
            // case KEY_: return (with_shift)?'':'';break;
            case KEY_A:
              return (with_shift) ? 0xD4 : 0xF4;
              break;
            case KEY_S:
              return (with_shift) ? 0xDB : 0xFB;
              break;
            case KEY_D:
              return (with_shift) ? 0xC2 : 0xE2;
              break;
            case KEY_F:
              return (with_shift) ? 0xC0 : 0xE0;
              break;
            case KEY_G:
              return (with_shift) ? 0xCF : 0xEF;
              break;
            case KEY_H:
              return (with_shift) ? 0xD0 : 0xF0;
              break;
            case KEY_J:
              return (with_shift) ? 0xCE : 0xEE;
              break;
            case KEY_K:
              return (with_shift) ? 0xCB : 0xEB;
              break;
            case KEY_L:
              return (with_shift) ? 0xC4 : 0xE4;
              break;
            case KEY_SEMICOLON:
              return (with_shift) ? 0xC6 : 0xE6;
              break;
            case KEY_APOSTROPHE:
              return (with_shift) ? 0xDD : 0xFD;
              break;
            case KEY_GRAVE:
              return (with_shift) ? 0xA8 : 0xB8;
              break;
            // case KEY_: return (with_shift)?'':'';break;
            case KEY_BACKSLASH:
              return (with_shift) ? 0x2F : 0x5C;
              break;
            case KEY_Z:
              return (with_shift) ? 0xDF : 0xFF;
              break;
            case KEY_X:
              return (with_shift) ? 0xD7 : 0xF7;
              break;
            case KEY_C:
              return (with_shift) ? 0xD1 : 0xF1;
              break;
            case KEY_V:
              return (with_shift) ? 0xCC : 0xEC;
              break;
            case KEY_B:
              return (with_shift) ? 0xC8 : 0xE8;
              break;
            case KEY_N:
              return (with_shift) ? 0xD2 : 0xF2;
              break;
            case KEY_M:
              return (with_shift) ? 0xDC : 0xFC;
              break;
            case KEY_COMMA:
              return (with_shift) ? 0xC1 : 0xE1;
              break;
            case KEY_PERIOD:
              return (with_shift) ? 0xDE : 0xFE;
              break;
            case KEY_SLASH:
              return (with_shift) ? 0x2C : 0x2E;
              break;
            case KEY_SPACE:
              return ' ';
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
            case KEY_DECIMAL:
              return '.';
              break;
            default:
              return 0;
          }
        }
      } while ((++i_keyboard) < 0x56);
      break;
  }
  return 0;  // should rarely happen
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
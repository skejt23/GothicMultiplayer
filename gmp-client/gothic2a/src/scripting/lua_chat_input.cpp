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

#include "lua_chat_input.h"

#include <string>

#include "CChat.h"
#include "Shared/lua_runtime/lua_math.h"

namespace gmp::gothic {

namespace {

CChat& Chat() {
  return *CChat::GetInstance();
}

}  // namespace

/* luagmp (func)
 *
 * This function clears the current chat input text.
 *
 * @version  0.3.0
 * @name     chatInputClear
 * @side     client
 * @category Chat
 *
 */
void Function_ChatInputClear() {
  Chat().ClearInput();
}

/* luagmp (func)
 *
 * This function closes the chat input.
 *
 * @version  0.3.0
 * @name     chatInputClose
 * @side     client
 * @category Chat
 *
 */
void Function_ChatInputClose() {
  Chat().CloseInput();
}

/* luagmp (func)
 *
 * This function returns the current chat input caret position.
 *
 * @version  0.3.0
 * @name     chatInputGetCaretPosition
 * @side     client
 * @category Chat
 * @return   (number) Zero-based caret position.
 *
 */
int Function_ChatInputGetCaretPosition() {
  return Chat().GetInputCaretPosition();
}

/* luagmp (func)
 *
 * This function returns the font assigned to the chat input.
 *
 * @version  0.3.0
 * @name     chatInputGetFont
 * @side     client
 * @category Chat
 * @return   (string) Font name.
 *
 */
std::string Function_ChatInputGetFont() {
  return Chat().GetInputFont();
}

/* luagmp (func)
 *
 * This function returns the maximum chat input length in bytes.
 *
 * @version  0.3.0
 * @name     chatInputGetMaxLength
 * @side     client
 * @category Chat
 * @return   (number) Maximum input length.
 *
 */
int Function_ChatInputGetMaxLength() {
  return Chat().GetInputMaxLength();
}

/* luagmp (func)
 *
 * This function returns the chat input position in virtual screen units.
 *
 * @version  0.3.0
 * @name     chatInputGetPosition
 * @side     client
 * @category Chat
 * @return   (Vec2) Position with x and y fields.
 *
 */
::lua::types::Vec2 Function_ChatInputGetPosition() {
  return ::lua::types::Vec2(static_cast<float>(Chat().GetInputX()), static_cast<float>(Chat().GetInputY()));
}

/* luagmp (func)
 *
 * This function returns the current chat input text.
 *
 * @version  0.3.0
 * @name     chatInputGetText
 * @side     client
 * @category Chat
 * @return   (string) Input text.
 *
 */
std::string Function_ChatInputGetText() {
  return Chat().GetInputText();
}

/* luagmp (func)
 *
 * This function checks whether the chat input is open.
 *
 * @version  0.3.0
 * @name     chatInputIsOpen
 * @side     client
 * @category Chat
 * @return   (boolean) True when the input is open.
 *
 */
bool Function_ChatInputIsOpen() {
  return Chat().IsInputActive();
}

/* luagmp (func)
 *
 * This function opens the chat input.
 *
 * @version  0.3.0
 * @name     chatInputOpen
 * @side     client
 * @category Chat
 *
 */
void Function_ChatInputOpen() {
  Chat().OpenInput();
}

/* luagmp (func)
 *
 * This function submits the current chat input text.
 *
 * @version  0.3.0
 * @name     chatInputSend
 * @side     client
 * @category Chat
 *
 */
void Function_ChatInputSend() {
  Chat().SubmitInput();
}

/* luagmp (func)
 *
 * This function changes the chat input caret position.
 *
 * @version  0.3.0
 * @name     chatInputSetCaretPosition
 * @side     client
 * @category Chat
 * @param    (number) position Zero-based caret position.
 *
 */
void Function_ChatInputSetCaretPosition(int position) {
  Chat().SetInputCaretPosition(position);
}

/* luagmp (func)
 *
 * This function changes the font assigned to the chat input.
 *
 * @version  0.3.0
 * @name     chatInputSetFont
 * @side     client
 * @category Chat
 * @param    (string) font Font name.
 *
 */
void Function_ChatInputSetFont(const std::string& font) {
  Chat().SetInputFont(font);
}

/* luagmp (func)
 *
 * This function changes the maximum chat input length in bytes.
 * The native default is 84 bytes.
 * The length is clamped to 1-1024, matching the message packet limit.
 * Existing text is truncated and the caret is clamped when necessary.
 *
 * @version  0.3.0
 * @name     chatInputSetMaxLength
 * @side     client
 * @category Chat
 * @param    (number) length Maximum input length.
 *
 */
void Function_ChatInputSetMaxLength(int length) {
  Chat().SetInputMaxLength(length);
}

/* luagmp (func)
 *
 * This function changes the chat input position in virtual screen units.
 *
 * @version  0.3.0
 * @name     chatInputSetPosition
 * @side     client
 * @category Chat
 * @param    (number) x X position.
 * @param    (number) y Y position.
 *
 */
void Function_ChatInputSetPosition(int x, int y) {
  Chat().SetInputPosition(x, y);
}

/* luagmp (func)
 *
 * This function changes the current chat input text.
 *
 * @version  0.3.0
 * @name     chatInputSetText
 * @side     client
 * @category Chat
 * @param    (string) text New input text.
 *
 */
void Function_ChatInputSetText(const std::string& text) {
  Chat().SetInputText(text);
}

void BindChatInput(sol::state& lua) {
  lua["chatInputClear"] = Function_ChatInputClear;
  lua["chatInputClose"] = Function_ChatInputClose;
  lua["chatInputGetCaretPosition"] = Function_ChatInputGetCaretPosition;
  lua["chatInputGetFont"] = Function_ChatInputGetFont;
  lua["chatInputGetMaxLength"] = Function_ChatInputGetMaxLength;
  lua["chatInputGetPosition"] = Function_ChatInputGetPosition;
  lua["chatInputGetText"] = Function_ChatInputGetText;
  lua["chatInputIsOpen"] = Function_ChatInputIsOpen;
  lua["chatInputOpen"] = Function_ChatInputOpen;
  lua["chatInputSend"] = Function_ChatInputSend;
  lua["chatInputSetCaretPosition"] = Function_ChatInputSetCaretPosition;
  lua["chatInputSetFont"] = Function_ChatInputSetFont;
  lua["chatInputSetMaxLength"] = Function_ChatInputSetMaxLength;
  lua["chatInputSetPosition"] = Function_ChatInputSetPosition;
  lua["chatInputSetText"] = Function_ChatInputSetText;
}

}  // namespace gmp::gothic

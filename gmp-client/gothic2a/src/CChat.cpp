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

/*****************************************************************************
**                                                                          **
**  File name:      Interface/CChat.cpp                                     **
**                                                                          **
**  Created by:     29/06/11 - skejt23                                     **
**                                                                          **
**  Description:    Multiplayer chat input and message bridge                **
**                                                                          **
*****************************************************************************/

#pragma warning(disable : 4018)

#include "CChat.h"

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "keyboard.h"
#include "net_game.h"
#include "patch.h"
#include "protocol/common_structs.h"
#include "scripting/gothic_events.h"
#include "shared/event.h"

using namespace Gothic_II_Addon;

namespace {
UINT ChatInputCodePage(std::string_view font) {
  constexpr std::pair<const char*, UINT> encodings[] = {{"CP1250_", 1250}, {"CP1251_", 1251}, {"CP1252_", 1252}, {"CP1254_", 1254}};
  for (const auto& [prefix, code_page] : encodings)
    if (font.size() >= 7 && _strnicmp(font.data(), prefix, 7) == 0)
      return code_page;
  // Script chat fonts use Gothic's native code page. The menu language can be
  // English while the game and its unprefixed fonts still use Polish CP1250.
  const auto native_code_page = GetACP();
  return native_code_page == CP_UTF8 ? 1250 : native_code_page;
}

std::size_t ClampCaretPosition(int position, std::size_t text_size) {
  if (position <= 0) {
    return 0;
  }

  return std::min(static_cast<std::size_t>(position), text_size);
}

std::optional<std::pair<std::string, std::string>> ParseCommand(std::string_view text) {
  if (text.empty() || text.front() != '/') {
    return std::nullopt;
  }

  text.remove_prefix(1);
  const auto command_start = text.find_first_not_of(' ');
  if (command_start == std::string_view::npos) {
    return std::nullopt;
  }

  text.remove_prefix(command_start);
  const auto space_pos = text.find(' ');
  std::string command(text.substr(0, space_pos));
  if (command.empty()) {
    return std::nullopt;
  }

  std::string params;
  if (space_pos != std::string_view::npos) {
    const auto params_start = text.find_first_not_of(' ', space_pos);
    if (params_start != std::string_view::npos) {
      params.assign(text.substr(params_start));
    }
  }

  return std::make_pair(std::move(command), std::move(params));
}

bool IsSensitiveCommand(std::string_view command) {
  constexpr std::string_view kRconCommand = "rcon";
  return command.size() == kRconCommand.size() &&
         std::equal(command.begin(), command.end(), kRconCommand.begin(), kRconCommand.end(), [](char a, char b) {
           return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
         });
}

std::string FormatMessage(const char* format, va_list args) {
  if (!format) {
    return {};
  }

  char text[512] = {};
  const int written = std::vsnprintf(text, sizeof(text), format, args);
  if (written < 0) {
    return {};
  }

  return std::string(text, std::min<std::size_t>(static_cast<std::size_t>(written), sizeof(text) - 1));
}

void DispatchSystemMessage(const zCOLOR& color, const std::string& text) {
  EventManager::Instance().TriggerEvent(
      gmp::gothic::kEventOnPlayerMessageName,
      gmp::gothic::OnPlayerMessageEvent{std::nullopt,
                                        static_cast<std::uint8_t>(color.r),
                                        static_cast<std::uint8_t>(color.g),
                                        static_cast<std::uint8_t>(color.b),
                                        text});
}
}  // namespace

extern zCOLOR Normal;

CChat::CChat() = default;

CChat::~CChat() = default;

bool CChat::IsInputActive() const {
  return input_active_;
}

void CChat::OpenInput() {
  if (input_active_) {
    return;
  }

  if (zinput) {
    // ClearKeyBuffer also erases held modifier states. Preserve Shift/Ctrl
    // and discard only the default opening key when it is still queued.
    discard_opening_t_ = zinput->KeyPressed(KEY_T) != 0;
    zinput->ResetRepeatKey(TRUE);
  }
  input_active_ = true;
  SetHandleEventTop();
  caret_position_ = std::min(caret_position_, current_text_.size());
  PrepareForInput();
}

void CChat::CloseInput(bool clear_text) {
  if (!input_active_) {
    if (clear_text) {
      ClearInput();
    }
    return;
  }

  if (clear_text) {
    ClearInput();
  }

  input_active_ = false;
  pending_input_.clear();
  SetEnableHandleEvent(false);
  discard_opening_t_ = false;
  if (zinput) {
    for (int count = 0; count < 256 && zinput->GetKey(FALSE, FALSE) != 0; ++count) {
    }
    zinput->ResetRepeatKey(TRUE);
  }
  ClearAfterInput();
}

void CChat::ClearInput() {
  current_text_.clear();
  caret_position_ = 0;
}

void CChat::SubmitInput() {
  if (!current_text_.empty()) {
    SendCurrentMessage();
  }

  CloseInput(true);
  if (!input_active_) {
    ClearInput();
  }
}

int CChat::GetInputCaretPosition() const {
  return static_cast<int>(caret_position_);
}

void CChat::SetInputCaretPosition(int position) {
  caret_position_ = ClampCaretPosition(position, current_text_.size());
}

const std::string& CChat::GetInputFont() const {
  return input_font_;
}

void CChat::SetInputFont(const std::string& font) {
  if (!font.empty()) {
    input_font_ = font;
  }
}

int CChat::GetInputMaxLength() const {
  return input_max_length_;
}

void CChat::SetInputMaxLength(int length) {
  input_max_length_ = std::clamp(length, 1, static_cast<int>(kMaxChatMessageLength));
  if (current_text_.size() > static_cast<std::size_t>(input_max_length_)) {
    current_text_.resize(static_cast<std::size_t>(input_max_length_));
  }
  caret_position_ = std::min(caret_position_, current_text_.size());
}

int CChat::GetInputX() const {
  return input_x_;
}

int CChat::GetInputY() const {
  return input_y_;
}

void CChat::SetInputPosition(int x, int y) {
  input_x_ = x;
  input_y_ = y;
}

const std::string& CChat::GetInputText() const {
  return current_text_;
}

void CChat::SetInputText(const std::string& text) {
  current_text_ = text.substr(0, static_cast<std::size_t>(input_max_length_));
  caret_position_ = current_text_.size();
}

void CChat::SendCurrentMessage() {
  if (current_text_.empty()) {
    return;
  }

  const std::string message = current_text_;
  if (auto command = ParseCommand(message)) {
    if (!IsSensitiveCommand(command->first)) {
      EventManager::Instance().TriggerEvent(gmp::gothic::kEventOnCommandName,
                                            gmp::gothic::OnCommandEvent{command->first, command->second});
    }
  }

  NetGame::Instance().SendMessage(message.c_str());
}

void CChat::InsertInputCharacter(char ch) {
  if (current_text_.size() >= static_cast<std::size_t>(input_max_length_)) {
    return;
  }

  caret_position_ = std::min(caret_position_, current_text_.size());
  current_text_.insert(current_text_.begin() + static_cast<std::ptrdiff_t>(caret_position_), ch);
  ++caret_position_;
}

void CChat::DeleteInputCharacterBeforeCaret() {
  if (current_text_.empty() || caret_position_ == 0) {
    return;
  }

  caret_position_ = std::min(caret_position_, current_text_.size());
  current_text_.erase(current_text_.begin() + static_cast<std::ptrdiff_t>(caret_position_ - 1));
  --caret_position_;
}

void CChat::PrepareForInput() {
  camera_mode_change_enabled_ = zCAICamera::bCamChanges != 0;
  zCAICamera::bCamChanges = 0;

  if (player && player->inventory2.IsOpen()) {
    player->inventory2.Close();
  }

  Patch::PlayerInterfaceEnabled(false);
  oCNpc::SetNpcAIDisabled(1);
  KeepInputLocked();
}

void CChat::ClearAfterInput() {
  if (player && player->IsMovLock()) {
    player->SetMovLock(0);
  }

  oCNpc::SetNpcAIDisabled(0);
  Patch::PlayerInterfaceEnabled(true);
  zCAICamera::bCamChanges = camera_mode_change_enabled_ ? 1 : 0;
}

void CChat::KeepInputLocked() {
  if (!player) {
    return;
  }

  if (!player->IsMovLock()) {
    player->SetMovLock(1);
  }

  if (auto* anictrl = player->GetAnictrl()) {
    anictrl->StopTurnAnis();
  }
}

void CChat::HandleInput() {
  if (!input_active_) {
    return;
  }

  KeepInputLocked();

  if (!zinput)
    return;

  // Lua observes its editing/history keys before submission closes input.
  // Modifiers were captured at native event delivery; apply before onRender.
  auto pending = std::move(pending_input_);
  pending_input_.clear();
  for (const auto& input : pending) {
    ApplyInputKey(input.key, input.character, input.control_pressed);
    if (!input_active_)
      break;
  }

  // The engine callback handles the first key and its repeat. Drain additional
  // presses without advancing repeat twice.
  for (int count = 0; input_active_ && count < 256; ++count) {
    const auto key = zinput->GetKey(FALSE, TRUE);
    if (key == 0)
      break;
    if (key & KEY_RELEASE)
      continue;
    ApplyInputKey(key, static_cast<unsigned char>(GInput::TranslateKeyFromKeyboard(key, false, ChatInputCodePage(input_font_))),
                  GInput::IsControlShortcutPressed());
  }
  discard_opening_t_ = false;
}

int CChat::HandleEvent(int key) {
  if (!input_active_ || !zinput)
    return false;
  if (key & KEY_RELEASE)
    return true;
  if (pending_input_.size() < 256) {
    pending_input_.push_back({key, static_cast<unsigned char>(GInput::TranslateKeyFromKeyboard(key, false, ChatInputCodePage(input_font_))),
                              GInput::IsControlShortcutPressed()});
  }
  return true;
}

void CChat::ApplyInputKey(int key, unsigned char character, bool control_pressed) {
  if (discard_opening_t_) {
    discard_opening_t_ = false;
    if (key == KEY_T) {
      zinput->ResetRepeatKey(TRUE);
      return;
    }
  }
  switch (key) {
    case KEY_RETURN:
    case KEY_NUMPADENTER:
      SubmitInput();
      return;
    case KEY_ESCAPE:
      CloseInput(true);
      return;
    case KEY_BACKSPACE:
      DeleteInputCharacterBeforeCaret();
      return;
    case KEY_LEFTARROW:
      if (caret_position_ > 0)
        --caret_position_;
      return;
    case KEY_RIGHTARROW:
      if (caret_position_ < current_text_.size())
        ++caret_position_;
      return;
    case KEY_DELETE:
      if (caret_position_ < current_text_.size())
        current_text_.erase(caret_position_, 1);
      return;
    case KEY_HOME:
      caret_position_ = 0;
      return;
    case KEY_END:
      caret_position_ = current_text_.size();
      return;
    default:
      break;
  }
  if (!control_pressed && character >= 0x20 && character != 0x7F)
    InsertInputCharacter(static_cast<char>(character));
}

void CChat::WriteMessage(MsgType type, bool PrintTimed, const zCOLOR& rgb, const char* format, ...) {
  (void)type;
  (void)PrintTimed;

  va_list args;
  va_start(args, format);
  const std::string text = FormatMessage(format, args);
  va_end(args);

  DispatchSystemMessage(rgb, text);
}

void CChat::WriteMessage(MsgType type, bool PrintTimed, const char* format, ...) {
  (void)type;
  (void)PrintTimed;

  va_list args;
  va_start(args, format);
  const std::string text = FormatMessage(format, args);
  va_end(args);

  DispatchSystemMessage(Normal, text);
}

void CChat::ClearChat() {
}

void CChat::PrintChat() {
}

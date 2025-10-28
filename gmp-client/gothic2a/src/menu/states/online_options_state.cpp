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

#include "online_options_state.hpp"

#include <spdlog/spdlog.h>

#include <format>

#include "keyboard.h"
#include "language.h"
#include "main_menu.h"
#include "menu/states/main_menu_loop_state.hpp"
#include "menu/states/set_watch_position_state.hpp"

// External declarations
extern zCOLOR Normal;
extern zCOLOR Highlighted;
extern zCOLOR Red;

namespace menu {
namespace states {

OnlineOptionsState::OnlineOptionsState(MenuContext& context)
    : context_(context), selectedOption_(OptionItem::NICKNAME), shouldReturnToMainMenu_(false), shouldEnterWatchPositioning_(false) {
}

void OnlineOptionsState::OnEnter() {
  SPDLOG_INFO("Entering online options state");
}

void OnlineOptionsState::OnExit() {
  SPDLOG_INFO("Exiting online options state");
}

StateResult OnlineOptionsState::Update() {
  UpdateTitleWeapon();
  RenderOptionsMenu();
  HandleInput();
  return StateResult::Continue;
}

MenuState* OnlineOptionsState::CheckTransition() {
  if (shouldReturnToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  if (shouldEnterWatchPositioning_) {
    return new SetWatchPositionState(context_);
  }
  return nullptr;
}

void OnlineOptionsState::RenderOptionsMenu() {
  context_.screen->SetFont("FONT_OLD_20_WHITE.TGA");
  zCOLOR fcolor;

  // Nickname
  fcolor = (selectedOption_ == OptionItem::NICKNAME) ? Highlighted : Normal;
  if (context_.writingNickname)
    fcolor = Red;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 3200, Language::Instance()[Language::MMENU_NICKNAME]);
  context_.screen->SetFontColor(Normal);
  int screenResX = zoptions->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
  int nicknameX = (screenResX < 1024) ? 1800 : 1500;
  context_.screen->Print(nicknameX, 3200, context_.config.Nickname);

  // Log Chat
  fcolor = (selectedOption_ == OptionItem::LOG_CHAT) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(
      200, 3600, (context_.config.logchat) ? Language::Instance()[Language::MMENU_LOGCHATYES] : Language::Instance()[Language::MMENU_LOGCHATNO]);

  // Watch
  fcolor = (selectedOption_ == OptionItem::WATCH_TOGGLE) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 4000,
                         (context_.config.watch) ? Language::Instance()[Language::MMENU_WATCHON] : Language::Instance()[Language::MMENU_WATCHOFF]);

  // Set Watch Position
  fcolor = (selectedOption_ == OptionItem::WATCH_POSITION) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 4400, Language::Instance()[Language::MMENU_SETWATCHPOS]);

  // Anti-aliasing
  fcolor = (selectedOption_ == OptionItem::ANTIALIASING) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 4800,
                         (zoptions->ReadInt("ENGINE", "zVidEnableAntiAliasing", 0) == 1) ? Language::Instance()[Language::MMENU_ANTIALIASINGYES]
                                                                                         : Language::Instance()[Language::MMENU_ANTIAlIASINGNO]);

  // Joystick
  fcolor = (selectedOption_ == OptionItem::JOYSTICK) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 5200,
                         (zoptions->ReadBool(zOPT_SEC_GAME, "joystick", 0) == 1) ? Language::Instance()[Language::MMENU_JOYSTICKYES]
                                                                                 : Language::Instance()[Language::MMENU_JOYSTICKNO]);

  // Chat Lines
  fcolor = (selectedOption_ == OptionItem::CHAT_LINES) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  const auto chatlines_str = std::format("{} {}", Language::Instance()[Language::MMENU_CHATLINES].ToChar(), context_.config.ChatLines);
  context_.screen->Print(200, 5600, chatlines_str.c_str());

  // Keyboard Layout
  fcolor = (selectedOption_ == OptionItem::KEYBOARD_LAYOUT) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  switch (context_.config.keyboardlayout) {
    case Config::KEYBOARD_POLISH:
      context_.screen->Print(200, 6000, Language::Instance()[Language::KEYBOARD_POLISH]);
      break;
    case Config::KEYBOARD_GERMAN:
      context_.screen->Print(200, 6000, Language::Instance()[Language::KEYBOARD_GERMAN]);
      break;
    case Config::KEYBOARD_CYRYLLIC:
      context_.screen->Print(200, 6000, Language::Instance()[Language::KEYBOARD_RUSSIAN]);
      break;
  }

  // Language
  fcolor = (selectedOption_ == OptionItem::LANGUAGE) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  zSTRING languageText = "Language: ";
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();
  if (context_.config.lang >= 0 && context_.config.lang < static_cast<int>(languages.size())) {
    languageText += languages[context_.config.lang].displayName;
  } else {
    languageText += Language::Instance()[Language::LANGUAGE];
  }
  context_.screen->Print(200, 6400, languageText);

  // Intros
  fcolor = (selectedOption_ == OptionItem::INTRO_VIDEOS) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 6800,
                         (zoptions->ReadBool(zOPT_SEC_GAME, "playLogoVideos", 1) == 1) ? Language::Instance()[Language::INTRO_YES]
                                                                                       : Language::Instance()[Language::INTRO_NO]);

  // Back
  fcolor = (selectedOption_ == OptionItem::BACK) ? Highlighted : Normal;
  context_.screen->SetFontColor(fcolor);
  context_.screen->Print(200, 7200, Language::Instance()[Language::MMENU_BACK]);
}

void OnlineOptionsState::HandleInput() {
  if (context_.writingNickname) {
    char x[2] = {0, 0};
    x[0] = GInput::GetCharacterFormKeyboard();

    // Backspace
    if ((x[0] == 8) && (context_.config.Nickname.Length() > 0)) {
      context_.config.Nickname.DeleteRight(1);
    }

    // Add character (printable ASCII)
    if ((x[0] >= 0x20) && (context_.config.Nickname.Length() < 24)) {
      context_.config.Nickname += x;
    }

    // Enter - confirm
    if ((x[0] == 0x0D) && (!context_.config.Nickname.IsEmpty())) {
      context_.config.SaveConfigToFile();
      context_.writingNickname = false;
    }
    return;
  }

  // Navigation
  if (context_.input->KeyToggled(KEY_UP)) {
    if (selectedOption_ == OptionItem::NICKNAME) {
      selectedOption_ = OptionItem::BACK;
    } else {
      selectedOption_ = static_cast<OptionItem>(static_cast<int>(selectedOption_) - 1);
    }
  }

  if (context_.input->KeyToggled(KEY_DOWN)) {
    if (selectedOption_ == OptionItem::BACK) {
      selectedOption_ = OptionItem::NICKNAME;
    } else {
      selectedOption_ = static_cast<OptionItem>(static_cast<int>(selectedOption_) + 1);
    }
  }

  // Left/Right for adjustable options
  if (context_.input->KeyToggled(KEY_LEFT)) {
    AdjustOption(selectedOption_, -1);
  }

  if (context_.input->KeyToggled(KEY_RIGHT)) {
    AdjustOption(selectedOption_, 1);
  }

  // Execute option on Enter
  if (context_.input->KeyPressed(KEY_RETURN)) {
    context_.input->ClearKeyBuffer();
    ExecuteOption(selectedOption_);
  }
}

void OnlineOptionsState::ExecuteOption(OptionItem option) {
  switch (option) {
    case OptionItem::NICKNAME:
      context_.writingNickname = true;
      break;

    case OptionItem::LOG_CHAT:
      context_.config.logchat = !context_.config.logchat;
      context_.config.SaveConfigToFile();
      break;

    case OptionItem::WATCH_TOGGLE:
      context_.config.watch = !context_.config.watch;
      context_.config.SaveConfigToFile();
      break;

    case OptionItem::WATCH_POSITION:
      context_.config.watch = true;
      shouldEnterWatchPositioning_ = true;
      break;

    case OptionItem::ANTIALIASING: {
      int current = zoptions->ReadInt("ENGINE", "zVidEnableAntiAliasing", 0);
      zoptions->WriteInt("ENGINE", "zVidEnableAntiAliasing", (current == 0) ? 1 : 0);
      gameMan->ApplySomeSettings();
      break;
    }

    case OptionItem::JOYSTICK: {
      int current = zoptions->ReadBool(zOPT_SEC_GAME, "enableJoystick", 0);
      zoptions->WriteBool(zOPT_SEC_GAME, "enableJoystick", !current);
      gameMan->ApplySomeSettings();
      break;
    }

    case OptionItem::INTRO_VIDEOS: {
      int current = zoptions->ReadBool(zOPT_SEC_GAME, "playLogoVideos", 1);
      zoptions->WriteBool(zOPT_SEC_GAME, "playLogoVideos", !current);
      gameMan->ApplySomeSettings();
      break;
    }

    case OptionItem::BACK:
      shouldReturnToMainMenu_ = true;
      break;

    default:
      // Other options handled by left/right
      break;
  }
}

void OnlineOptionsState::AdjustOption(OptionItem option, int direction) {
  switch (option) {
    case OptionItem::CHAT_LINES:
      if (direction < 0) {  // Left
        if (context_.config.ChatLines > 0) {
          if (context_.config.ChatLines <= 5)
            context_.config.ChatLines = 0;
          else
            context_.config.ChatLines--;
          context_.config.SaveConfigToFile();
        }
      } else {  // Right
        if (context_.config.ChatLines < 30) {
          if (context_.config.ChatLines < 5)
            context_.config.ChatLines = 5;
          else
            context_.config.ChatLines++;
          context_.config.SaveConfigToFile();
        }
      }
      break;

    case OptionItem::KEYBOARD_LAYOUT:
      if (direction < 0) {  // Left
        if (context_.config.keyboardlayout > Config::KEYBOARD_POLISH) {
          context_.config.keyboardlayout--;
          context_.config.SaveConfigToFile();
        }
      } else {  // Right
        if (context_.config.keyboardlayout < Config::KEYBOARD_CYRYLLIC) {
          context_.config.keyboardlayout++;
          context_.config.SaveConfigToFile();
        }
      }
      break;

    case OptionItem::LANGUAGE: {
      const auto& languages = LanguageManager::Instance().GetAvailableLanguages();
      if (languages.empty())
        return;

      int newIndex = context_.config.lang + direction;
      if (newIndex < 0)
        newIndex = static_cast<int>(languages.size()) - 1;
      if (newIndex >= static_cast<int>(languages.size()))
        newIndex = 0;

      if (newIndex == context_.config.lang)
        return;

      // Load new language
      LanguageManager::Instance().LoadLanguages(LanguageManager::Instance().GetLanguageDir().c_str(), newIndex);

      context_.config.lang = newIndex;
      context_.config.SaveConfigToFile();

      // Rebuild server list UI so translated labels refresh immediately
      delete context_.extendedServerList;
      context_.extendedServerList = new ExtendedServerList(context_.serverList);
      context_.extendedServerList->RefreshList();
      break;
    }

    default:
      // Other options don't support left/right
      break;
  }
}

void OnlineOptionsState::UpdateTitleWeapon() {
  if (context_.titleWeapon) {
    context_.titleWeapon->RotateWorldX(0.6f);
  }
}

}  // namespace states
}  // namespace menu

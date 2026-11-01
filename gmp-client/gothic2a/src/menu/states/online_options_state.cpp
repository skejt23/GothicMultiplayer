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

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "keyboard.h"
#include "language.h"
#include "menu/states/main_menu_loop_state.hpp"
#include "scripting/process_input.h"

extern zCOLOR Normal;
extern zCOLOR Highlighted;

namespace {
using namespace Gothic_II_Addon;

constexpr char kMenuBackground[] = "MENU_INGAME.TGA";
constexpr char kNormalFont[] = "FONT_OLD_20_WHITE.TGA";
constexpr char kSmallFont[] = "FONT_OLD_10_WHITE.TGA";

constexpr int kPanelLeft = 1350;
constexpr int kPanelTop = 1000;
constexpr int kPanelRight = 6842;
constexpr int kPanelBottom = 7192;

constexpr int kTitleY = 1200;
constexpr int kRowY = 1700;
constexpr int kRowDy = 650;
constexpr int kLabelX = 1000;
constexpr int kValueX = 4300;
constexpr int kValueWidth = 2900;
constexpr int kChoiceYOffset = 120;
constexpr int kBackY = 6500;
constexpr int kWideItemWidth = 6192;
constexpr int kInfoX = 300;
constexpr int kInfoY = 7780;

constexpr int kCategoryItemCount = 3;
constexpr int kMultiplayerItemCount = 7;
constexpr int kDisplayItemCount = 4;
constexpr int kVolumeSteps = 20;

zSTRING BoolText(bool value) {
  return Language::Instance()[value ? Language::MMENU_YES : Language::MMENU_NO];
}

zSTRING RendererText(Config::RendererType renderer) {
  switch (renderer) {
    case Config::RendererType::D3D7:
      return "D3D7";
    case Config::RendererType::D3D9:
      return "D3D9";
    case Config::RendererType::D3D11:
      return "D3D11";
  }
  return "D3D9";
}

zSTRING PushToTalkText(int key) {
  zSTRING name = zCInput::GetNameByControlValue(static_cast<unsigned short>(key));
  if (!name.IsEmpty()) {
    return name;
  }

  const auto stable_name = gmp::gothic::FindKeyboardKeyName(key);
  if (stable_name.empty()) {
    return "KEY_K";
  }
  const std::string stable_name_text(stable_name);
  return stable_name_text.c_str();
}

zSTRING VolumeText(float volume) {
  const int filled = std::clamp(static_cast<int>(std::lround(volume * kVolumeSteps)), 0, kVolumeSteps);
  std::string text = "[";
  text.append(filled, '|');
  text.append(kVolumeSteps - filled, '-');
  text.append("] ");
  text.append(std::to_string(filled * (100 / kVolumeSteps)));
  text.push_back('%');
  return text.c_str();
}
}  // namespace

namespace menu::states {

OnlineOptionsState::OnlineOptionsState(MenuContext& context)
    : context_(context),
      page_(Page::Categories),
      selectedItem_(0),
      shouldReturnToMainMenu_(false),
      editingNickname_(false),
      capturingPushToTalk_(false),
      menuView_(nullptr) {
}

OnlineOptionsState::~OnlineOptionsState() {
  delete menuView_;
}

void OnlineOptionsState::OnEnter() {
  SPDLOG_INFO("Entering Multiplayer options state");
  if (!menuView_) {
    menuView_ = new zCView(kPanelLeft, kPanelTop, kPanelRight, kPanelBottom);
    menuView_->InsertBack(kMenuBackground);
  }
  context_.input->ClearKeyBuffer();
}

void OnlineOptionsState::OnExit() {
  if (editingNickname_) {
    context_.config.Nickname = nicknameBeforeEdit_;
  }
  editingNickname_ = false;
  capturingPushToTalk_ = false;
  SPDLOG_INFO("Exiting Multiplayer options state");
}

StateResult OnlineOptionsState::Update() {
  RenderMenu();
  HandleInput();
  return StateResult::Continue;
}

MenuState* OnlineOptionsState::CheckTransition() {
  if (shouldReturnToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  return nullptr;
}

void OnlineOptionsState::SetFont(bool small_font, bool highlighted) {
  const auto font = Language::Instance().ApplyFontPrefix(small_font ? kSmallFont : kNormalFont);
  menuView_->SetFont(font.c_str());
  menuView_->SetFontColor(highlighted ? Highlighted : Normal);
}

void OnlineOptionsState::PrintCentered(int x, int width, int y, const zSTRING& text) {
  menuView_->Print(x + (width - menuView_->FontSize(text)) / 2, y, text);
}

void OnlineOptionsState::PrintHeading(const zSTRING& text) {
  SetFont(false, false);
  menuView_->PrintCX(kTitleY, text);
}

void OnlineOptionsState::PrintWideItem(int index, int y, const zSTRING& text) {
  SetFont(false, selectedItem_ == index);
  PrintCentered(kLabelX, kWideItemWidth, y, text);
}

void OnlineOptionsState::PrintOption(int index, int row, const zSTRING& label, const zSTRING& value, bool value_active) {
  SetFont(false, selectedItem_ == index);
  menuView_->Print(kLabelX, kRowY + kRowDy * row, label);

  SetFont(true, value_active);
  PrintCentered(kValueX, kValueWidth, kRowY + kRowDy * row + kChoiceYOffset, value);
}

void OnlineOptionsState::RenderMenu() {
  if (!menuView_) {
    return;
  }

  menuView_->ClrPrintwin();
  switch (page_) {
    case Page::Categories:
      RenderCategories();
      break;
    case Page::Multiplayer:
      RenderMultiplayerOptions();
      break;
    case Page::Display:
      RenderDisplayOptions();
      break;
  }
  menuView_->Render();
  menuView_->ClrPrintwin();
}

void OnlineOptionsState::RenderCategories() {
  const auto& language = Language::Instance();
  PrintHeading(language[Language::MMENU_GMP_OPTIONS]);
  PrintWideItem(0, 2800, language[Language::MMENU_GMP_CATEGORY]);
  PrintWideItem(1, 3550, language[Language::MMENU_DISPLAY_CATEGORY]);
  PrintWideItem(2, kBackY, language[Language::MMENU_BACK]);
}

void OnlineOptionsState::RenderMultiplayerOptions() {
  const auto& language = Language::Instance();
  PrintHeading(language[Language::MMENU_GMP_CATEGORY]);

  zSTRING nickname = context_.config.Nickname;
  if (editingNickname_) {
    nickname += "_";
  }
  PrintOption(0, 0, language[Language::MMENU_NICKNAME], nickname, editingNickname_);

  zSTRING language_name = language[Language::LANGUAGE];
  if (const auto* info = LanguageManager::Instance().GetLanguage(LanguageManager::Instance().GetActiveLanguageIndex())) {
    language_name = info->displayName;
  }
  PrintOption(1, 1, language[Language::MMENU_LANGUAGE], language_name, false);
  PrintOption(2, 2, language[Language::MMENU_VOICE_ENABLED], BoolText(context_.config.IsVoiceChatEnabled()), false);
  PrintOption(3,
              3,
              language[Language::MMENU_VOICE_PTT_KEY],
              PushToTalkText(context_.config.GetVoicePushToTalkKey()),
              capturingPushToTalk_);
  PrintOption(4, 4, language[Language::MMENU_VOICE_VOLUME], VolumeText(context_.config.GetVoiceOutputVolume()), false);
  PrintOption(5, 5, language[Language::MMENU_EXTENDED_MENU_SCENES], BoolText(context_.config.extended_menu_scenes), false);
  PrintWideItem(6, kBackY, language[Language::MMENU_BACK]);
}

void OnlineOptionsState::RenderDisplayOptions() {
  const auto& language = Language::Instance();
  PrintHeading(language[Language::MMENU_DISPLAY_CATEGORY]);
  PrintOption(0, 0, language[Language::MMENU_WINDOW_ALWAYS_ON_TOP], BoolText(context_.config.IsWindowAlwaysOnTop()), false);
  PrintOption(1, 1, language[Language::MMENU_VSYNC], BoolText(context_.config.vsync_enabled), false);
  PrintOption(2, 2, language[Language::MMENU_RENDERER], RendererText(context_.config.GetRendererType()), false);
  PrintWideItem(3, kBackY, language[Language::MMENU_BACK]);

  if (selectedItem_ < 3) {
    SetFont(true, false);
    menuView_->Print(kInfoX, kInfoY, language[Language::MMENU_RESTART_REQUIRED]);
  }
}

void OnlineOptionsState::HandleInput() {
  if (editingNickname_) {
    HandleNicknameInput();
    return;
  }
  if (capturingPushToTalk_) {
    HandlePushToTalkInput();
    return;
  }

  if (context_.input->KeyToggled(KEY_ESCAPE)) {
    GoBack();
    context_.input->ClearKeyBuffer();
    return;
  }

  if (context_.input->KeyToggled(KEY_UP)) {
    MoveSelection(-1);
  } else if (context_.input->KeyToggled(KEY_DOWN)) {
    MoveSelection(1);
  }

  if (context_.input->KeyToggled(KEY_LEFT)) {
    AdjustSelectedItem(-1);
  } else if (context_.input->KeyToggled(KEY_RIGHT)) {
    AdjustSelectedItem(1);
  }

  if (context_.input->KeyPressed(KEY_RETURN)) {
    context_.input->ClearKeyBuffer();
    ActivateSelectedItem();
  }
}

void OnlineOptionsState::HandleNicknameInput() {
  if (context_.input->KeyToggled(KEY_ESCAPE)) {
    context_.config.Nickname = nicknameBeforeEdit_;
    editingNickname_ = false;
    context_.input->ClearKeyBuffer();
    return;
  }

  const char character = GInput::GetCharacterFormKeyboard();
  if (character == 0x08 && context_.config.Nickname.Length() > 0) {
    context_.config.Nickname.DeleteRight(1);
  } else if (character == 0x0D) {
    if (!context_.config.Nickname.IsEmpty()) {
      context_.config.SaveConfigToFile();
      editingNickname_ = false;
    }
  } else if (character >= 0x20 && context_.config.Nickname.Length() < 24) {
    char text[2] = {character, 0};
    context_.config.Nickname += text;
  }
}

void OnlineOptionsState::HandlePushToTalkInput() {
  const unsigned short raw_key = context_.input->GetKey(FALSE, FALSE);
  if (raw_key == 0 || (raw_key & KEY_RELEASE)) {
    return;
  }

  const int key = raw_key & ~KEY_RELEASE;
  if (key != KEY_ESCAPE && !gmp::gothic::FindKeyboardKeyName(key).empty()) {
    context_.config.SetVoicePushToTalkKey(key);
    context_.config.SaveConfigToFile();
  }
  capturingPushToTalk_ = false;
  context_.input->ClearKeyBuffer();
}

void OnlineOptionsState::MoveSelection(int direction) {
  const int item_count = GetItemCount();
  selectedItem_ = (selectedItem_ + direction + item_count) % item_count;
}

int OnlineOptionsState::GetItemCount() const {
  switch (page_) {
    case Page::Categories:
      return kCategoryItemCount;
    case Page::Multiplayer:
      return kMultiplayerItemCount;
    case Page::Display:
      return kDisplayItemCount;
  }
  return 0;
}

void OnlineOptionsState::ActivateSelectedItem() {
  if (page_ == Page::Categories) {
    if (selectedItem_ == 0) {
      page_ = Page::Multiplayer;
      selectedItem_ = 0;
    } else if (selectedItem_ == 1) {
      page_ = Page::Display;
      selectedItem_ = 0;
    } else {
      shouldReturnToMainMenu_ = true;
    }
    return;
  }

  if (page_ == Page::Multiplayer) {
    if (selectedItem_ == 0) {
      nicknameBeforeEdit_ = context_.config.Nickname;
      editingNickname_ = true;
      context_.input->ClearKeyBuffer();
    } else if (selectedItem_ == 3) {
      capturingPushToTalk_ = true;
      context_.input->ClearKeyBuffer();
    } else if (selectedItem_ == kMultiplayerItemCount - 1) {
      GoBack();
    }
    return;
  }

  if (selectedItem_ == kDisplayItemCount - 1) {
    GoBack();
  }
}

void OnlineOptionsState::AdjustSelectedItem(int direction) {
  if (page_ == Page::Multiplayer) {
    switch (selectedItem_) {
      case 1:
        ChangeLanguage(direction);
        return;
      case 2:
        context_.config.SetVoiceChatEnabled(!context_.config.IsVoiceChatEnabled());
        break;
      case 4: {
        const float volume = context_.config.GetVoiceOutputVolume() + static_cast<float>(direction) / kVolumeSteps;
        context_.config.SetVoiceOutputVolume(std::clamp(volume, 0.0f, 1.0f));
        break;
      }
      case 5:
        context_.config.extended_menu_scenes = !context_.config.extended_menu_scenes;
        context_.sceneManager.Configure(context_.config.extended_menu_scenes);
        break;
      default:
        return;
    }
    context_.config.SaveConfigToFile();
    return;
  }

  if (page_ != Page::Display) {
    return;
  }

  switch (selectedItem_) {
    case 0:
      context_.config.SetWindowAlwaysOnTop(!context_.config.IsWindowAlwaysOnTop());
      break;
    case 1:
      context_.config.vsync_enabled = !context_.config.vsync_enabled;
      break;
    case 2: {
      constexpr int renderer_count = 3;
      const int current = static_cast<int>(context_.config.GetRendererType());
      const int next = (current + direction + renderer_count) % renderer_count;
      context_.config.SetRendererType(static_cast<Config::RendererType>(next));
      break;
    }
    default:
      return;
  }
  context_.config.SaveConfigToFile();
}

void OnlineOptionsState::ChangeLanguage(int direction) {
  auto& manager = LanguageManager::Instance();
  const int language_count = static_cast<int>(manager.GetLanguageCount());
  if (language_count == 0) {
    return;
  }

  int current = manager.GetActiveLanguageIndex();
  if (current < 0) {
    current = std::max(manager.GetLanguageIndex(context_.config.language), 0);
  }
  const int next = (current + direction + language_count) % language_count;
  if (next == current || !manager.LoadLanguage(next)) {
    return;
  }

  const auto* info = manager.GetLanguage(next);
  if (!info) {
    return;
  }
  context_.config.language = std::filesystem::path(info->filename).stem().string();
  context_.config.SaveConfigToFile();

  delete context_.extendedServerList;
  context_.extendedServerList = new ExtendedServerList(context_.serverList);
  context_.extendedServerList->RefreshList();
}

void OnlineOptionsState::GoBack() {
  if (page_ == Page::Categories) {
    shouldReturnToMainMenu_ = true;
    return;
  }

  selectedItem_ = page_ == Page::Multiplayer ? 0 : 1;
  page_ = Page::Categories;
}

}  // namespace menu::states

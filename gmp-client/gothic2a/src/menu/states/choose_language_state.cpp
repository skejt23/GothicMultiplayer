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

#include "choose_language_state.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

#include "StandardFonts.h"
#include "keyboard.h"
#include "language.h"
#include "menu/states/main_menu_loop_state.hpp"

namespace {
const G2W::Font kFontRed20(255, 0, 0, "FONT_OLD_20_WHITE.TGA");
constexpr int kSetupTableX = 2200;
constexpr int kSetupTableY = 3200;
constexpr int kSetupTableWidth = 3800;
constexpr int kSetupTableHeight = 1800;
constexpr int kSetupTableInterline = 700;
constexpr int kSetupTableVisibleRows = 2;
constexpr int kSetupTablePaddingWidth = 400;
constexpr int kSetupTableLabelWidth = 4000;
constexpr int kSetupTableValueWidth = 3200;
constexpr int kAcceptButtonX = 3000;
constexpr int kAcceptButtonY = 4000;
constexpr int kAcceptButtonWidth = 2200;
constexpr int kAcceptButtonHeight = 400;
constexpr char kOptionsBackgroundTexture[] = "MENU_INGAME.TGA";
constexpr char kOptionsHighlightTexture[] = "MENU_INGAME.TGA";
constexpr char kAcceptText[] = "Accept";
}  // namespace

namespace menu {
namespace states {

ChooseLanguageState::ChooseLanguageState(MenuContext& context)
    : context_(context),
      selectedLanguage_(0),
      selectedItem_(SetupItem::NICKNAME),
      isEditingNickname_(false),
      shouldTransitionToMainMenu_(false),
      setupTable_(nullptr),
      acceptButton_(nullptr) {
  currentNickname_ = context_.config.Nickname;
}

ChooseLanguageState::~ChooseLanguageState() {
  delete setupTable_;
  delete acceptButton_;
}

void ChooseLanguageState::OnEnter() {
  SPDLOG_INFO("Entering first-launch setup state");

  // Set initial language from config if available
  const int configuredLanguage = LanguageManager::Instance().GetLanguageIndex(context_.config.language);
  if (configuredLanguage >= 0) {
    selectedLanguage_ = configuredLanguage;
  }

  InitializeControls();
}

void ChooseLanguageState::OnExit() {
  SPDLOG_INFO("Exiting first-launch setup state, selected language: {}, nickname: {}", selectedLanguage_, currentNickname_.ToChar());
}

StateResult ChooseLanguageState::Update() {
  RenderSetup();
  HandleInput();
  return StateResult::Continue;
}

MenuState* ChooseLanguageState::CheckTransition() {
  if (shouldTransitionToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  return nullptr;
}

void ChooseLanguageState::InitializeControls() {
  if (!setupTable_) {
    setupTable_ = new G2W::Table(kSetupTableX, kSetupTableY, kSetupTableWidth, kSetupTableHeight, kSetupTableInterline, kSetupTableVisibleRows);
    setupTable_->addColumn("", kSetupTablePaddingWidth);
    setupTable_->addColumn("", kSetupTableLabelWidth);
    setupTable_->addColumn("", kSetupTableValueWidth);
    setupTable_->setBackground(kOptionsBackgroundTexture);
    setupTable_->setFont(FNT_WHITE_20);
    setupTable_->setHighlightFont(FNT_GREEN_20);
  }

  if (!acceptButton_) {
    acceptButton_ = new G2W::Button(kAcceptButtonX, kAcceptButtonY, kAcceptButtonWidth, kAcceptButtonHeight);
    acceptButton_->setTexture(kOptionsBackgroundTexture);
    acceptButton_->setHighlightTexture(kOptionsHighlightTexture);
    acceptButton_->setFont(FNT_WHITE_20);
    acceptButton_->setHighlightFont(FNT_GREEN_20);
  }
}

void ChooseLanguageState::ApplySelectedLanguage() {
  const auto* langInfo = LanguageManager::Instance().GetLanguage(selectedLanguage_);
  if (!langInfo) {
    SPDLOG_ERROR("Invalid language selection: {}", selectedLanguage_);
    return;
  }

  SPDLOG_INFO("Applying language: {}", langInfo->filename);
  if (!LanguageManager::Instance().LoadLanguage(selectedLanguage_)) {
    return;
  }

  // Update config
  context_.config.language = std::filesystem::path(langInfo->filename).stem().string();
  context_.config.SaveConfigToFile();
}

void ChooseLanguageState::RenderSetup() {
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();

  if (languages.empty()) {
    context_.screen->Print(200, 200, "Error: No languages available");
    return;
  }

  InitializeControls();
  setupTable_->clear();
  setupTable_->setHighlightFont(isEditingNickname_ ? kFontRed20 : FNT_GREEN_20);

  std::string languageValue;
  if (selectedLanguage_ >= 0 && selectedLanguage_ < static_cast<int>(languages.size())) {
    languageValue = languages[selectedLanguage_].displayName;
  }

  auto addRow = [&](SetupItem item, std::string label, std::string value = {}) {
    setupTable_->addRow(G2W::TableRow{{"", std::move(label), std::move(value)}, item == selectedItem_});
  };

  addRow(SetupItem::NICKNAME, Language::Instance()[Language::MMENU_NICKNAME].ToChar(), currentNickname_.ToChar());
  addRow(SetupItem::LANGUAGE, Language::Instance()[Language::MMENU_LANGUAGE].ToChar(), languageValue);

  setupTable_->render();

  acceptButton_->setText(kAcceptText);
  acceptButton_->highlight = (selectedItem_ == SetupItem::ACCEPT);
  acceptButton_->render();
}

void ChooseLanguageState::HandleInput() {
  const auto& languages = LanguageManager::Instance().GetAvailableLanguages();
  if (languages.empty()) {
    return;
  }

  if (isEditingNickname_) {
    if (context_.input->KeyPressed(KEY_RETURN)) {
      isEditingNickname_ = false;
      context_.input->ClearKeyBuffer();
      return;
    }

    char inputChar[2] = {0, 0};
    inputChar[0] = GInput::GetCharacterFormKeyboard();

    // Backspace - delete last character
    if (inputChar[0] == 8 && currentNickname_.Length() > 0) {
      currentNickname_.DeleteRight(1);
    }

    // Regular character - add to nickname (max 24 chars)
    if (inputChar[0] >= 0x20 && currentNickname_.Length() < 24) {
      currentNickname_ += inputChar;
    }

    return;
  }

  // Navigation
  if (context_.input->KeyToggled(KEY_UP)) {
    if (selectedItem_ == SetupItem::NICKNAME) {
      selectedItem_ = SetupItem::ACCEPT;
    } else {
      selectedItem_ = static_cast<SetupItem>(static_cast<int>(selectedItem_) - 1);
    }
  }

  if (context_.input->KeyToggled(KEY_DOWN)) {
    if (selectedItem_ == SetupItem::ACCEPT) {
      selectedItem_ = SetupItem::NICKNAME;
    } else {
      selectedItem_ = static_cast<SetupItem>(static_cast<int>(selectedItem_) + 1);
    }
  }

  // Language selection
  if (selectedItem_ == SetupItem::LANGUAGE) {
    int direction = 0;
    if (context_.input->KeyToggled(KEY_LEFT)) {
      direction = -1;
    }
    if (context_.input->KeyToggled(KEY_RIGHT)) {
      direction = 1;
    }

    if (direction != 0) {
      selectedLanguage_ += direction;
      if (selectedLanguage_ < 0) {
        selectedLanguage_ = static_cast<int>(languages.size()) - 1;
      }
      if (selectedLanguage_ >= static_cast<int>(languages.size())) {
        selectedLanguage_ = 0;
      }
      ApplySelectedLanguage();
    }
  }

  if (context_.input->KeyPressed(KEY_RETURN)) {
    switch (selectedItem_) {
      case SetupItem::NICKNAME:
        isEditingNickname_ = true;
        context_.input->ClearKeyBuffer();
        break;
      case SetupItem::LANGUAGE:
        // No-op on enter for language row; use left/right.
        break;
      case SetupItem::ACCEPT:
        if (currentNickname_.IsEmpty()) {
          SPDLOG_WARN("Cannot accept first-launch settings: nickname is empty");
          return;
        }

        context_.config.Nickname = currentNickname_;
        if (const auto* language = LanguageManager::Instance().GetLanguage(selectedLanguage_)) {
          context_.config.language = std::filesystem::path(language->filename).stem().string();
        }
        context_.config.SaveConfigToFile();
        context_.input->ClearKeyBuffer();
        shouldTransitionToMainMenu_ = true;
        break;
    }
  }
}

}  // namespace states
}  // namespace menu

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

#include "menu/menu_context.hpp"
#include "menu/states/menu_state.hpp"

namespace menu::states {

class OnlineOptionsState : public MenuState {
public:
  explicit OnlineOptionsState(MenuContext& context);
  ~OnlineOptionsState() override;

  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override {
    return "OnlineOptions";
  }

private:
  enum class Page {
    Categories,
    Multiplayer,
    Display,
  };

  void RenderMenu();
  void RenderCategories();
  void RenderMultiplayerOptions();
  void RenderDisplayOptions();
  void SetFont(bool small_font, bool highlighted);
  void PrintCentered(int x, int width, int y, const zSTRING& text);
  void PrintHeading(const zSTRING& text);
  void PrintWideItem(int index, int y, const zSTRING& text);
  void PrintOption(int index, int row, const zSTRING& label, const zSTRING& value, bool value_active);

  void HandleInput();
  void HandleNicknameInput();
  void HandlePushToTalkInput();
  void MoveSelection(int direction);
  int GetItemCount() const;
  void ActivateSelectedItem();
  void AdjustSelectedItem(int direction);
  void ChangeLanguage(int direction);
  void GoBack();

  MenuContext& context_;
  Page page_;
  int selectedItem_;
  bool shouldReturnToMainMenu_;
  bool editingNickname_;
  bool capturingPushToTalk_;
  zSTRING nicknameBeforeEdit_;
  zCView* menuView_;
};

}  // namespace menu::states

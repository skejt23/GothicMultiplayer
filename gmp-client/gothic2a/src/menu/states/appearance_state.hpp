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

namespace menu {
namespace states {

/**
 * @brief State for customizing player appearance (face, body, walkstyle)
 *
 * This state allows players to customize:
 * - Head model
 * - Face texture
 * - Skin texture
 * - Walk style
 */
class AppearanceState : public MenuState {
private:
  MenuContext& context_;

  enum class AppearancePart { HEAD = 0, FACE = 1, SKIN = 2, WALKSTYLE = 3 };

  AppearancePart currentPart_;
  AppearancePart lastPart_;
  bool shouldReturnToMainMenu_;

public:
  explicit AppearanceState(MenuContext& context);
  ~AppearanceState() override = default;

  // MenuState interface
  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override {
    return "Appearance";
  }

private:
  void SetupAppearanceCamera();
  void CleanupAppearanceCamera();
  void RenderAppearanceUI();
  void HandleInput();
  void NavigateParts(int direction);
  void AdjustCurrentPart(int direction);
  void UpdateCameraPosition();
  void ApplyAppearanceChanges();
};

}  // namespace states
}  // namespace menu

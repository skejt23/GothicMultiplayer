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

#pragma once

#include "menu/menu_context.hpp"
#include "menu/scene/menu_camera.h"
#include "menu/states/menu_state.hpp"

namespace menu::states {

class CharacterCreationState final : public MenuState {
public:
  explicit CharacterCreationState(MenuContext& context);
  void OnEnter() override;
  void OnExit() override;
  StateResult Update() override;
  void UpdateScene(float delta_time) override;
  void StopScene() override;
  MenuState* CheckTransition() override;
  const char* GetStateName() const override { return "CharacterCreation"; }

private:
  enum Field { Body, Head, Face, Skin, Walking, FieldCount };
  void Render();
  void ChangeSelection(int change);
  void StopPreviewAnimation();

  MenuContext& context_;
  MenuCamera camera_;
  gmp::CharacterAppearance appearance_;
  oCNpc* preview_player_ = nullptr;
  zCAIBase* saved_player_ai_ = nullptr;
  zMAT4 saved_transform_;
  zVEC3 preview_position_;
  zTVobSleepingMode saved_sleeping_mode_ = zVOB_SLEEPING;
  bool saved_physics_enabled_ = false;
  bool saved_static_collision_ = false;
  bool saved_dynamic_collision_ = false;
  bool saved_movement_lock_ = false;
  bool saved_random_animations_ = true;
  int saved_hour_ = 12;
  int saved_minute_ = 0;
  int selected_field_ = Face;
  bool grounded_ = false;
  bool appearance_changed_ = true;
  bool animation_changed_ = true;
  bool finished_ = false;
};

}  // namespace menu::states

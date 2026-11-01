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

#include "character_creation_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "character_appearance.h"
#include "menu/states/main_menu_loop_state.hpp"

extern zCOLOR Normal;
extern zCOLOR Highlighted;

namespace menu::states {
namespace {

template <std::size_t Size>
int ChoiceIndex(const std::array<std::string_view, Size>& choices, const std::string& value) {
  const auto found = std::find(choices.begin(), choices.end(), value);
  return found == choices.end() ? 0 : static_cast<int>(found - choices.begin());
}

int Cycle(int value, int change, int count) {
  return (value + change + count) % count;
}

constexpr std::array kBodyLabels{Language::APP_MALE, Language::APP_FEMALE};
constexpr std::array kHeadLabels{Language::APP_FIGHTER, Language::APP_BALD, Language::APP_FAT_BALD,
                                 Language::APP_PONY, Language::APP_PSIONIC, Language::APP_THIEF};
constexpr std::array kWalkingLabels{Language::APP_DEFAULT, Language::APP_GUARD, Language::APP_MAGE, Language::APP_RELAXED,
                                    Language::APP_ARROGANT, Language::APP_TIRED, Language::APP_WOMAN};

struct PreviewCamera {
  float angle;
  float distance;
  float height;
};
// Body, head profile, face close-up, skin, walking: the classic Lua framing.
constexpr std::array<PreviewCamera, 5> kCameras{{{0, 260, 30}, {90, 105, 80}, {0, 140, 70}, {0, 260, 30}, {45, 300, 25}}};
constexpr float kPreviewAngle = -34.0f;
constexpr float kRadiansPerDegree = 0.01745329252f;

}  // namespace

CharacterCreationState::CharacterCreationState(MenuContext& context)
    : context_(context), camera_(context.game), appearance_(context.config.GetCharacterAppearance()) {
}

void CharacterCreationState::OnEnter() {
  context_.HideTitleWeapon();
  context_.sceneManager.StopScene();
  if (context_.logoView) {
    context_.screen->RemoveItem(context_.logoView);
  }
  context_.input->ClearKeyBuffer();
  preview_player_ = Gothic_II_Addon::player;
  if (!preview_player_) {
    return;
  }
  saved_transform_ = preview_player_->trafoObjToWorld;
  saved_sleeping_mode_ = static_cast<zTVobSleepingMode>(preview_player_->sleepingMode);
  saved_physics_enabled_ = preview_player_->physicsEnabled;
  saved_static_collision_ = preview_player_->collDetectionStatic;
  saved_dynamic_collision_ = preview_player_->collDetectionDynamic;
  saved_movement_lock_ = preview_player_->IsMovLock() != 0;
  if (auto* model = preview_player_->GetModel()) {
    saved_random_animations_ = model->randAnisEnabled;
  }
  // The normal hero controller still reacts to menu keys with movement locked.
  // As with MenuNpc, suspend native frame activity and advance only the model.
  preview_player_->GetEM()->Clear();
  saved_player_ai_ = preview_player_->callback_ai;
  if (saved_player_ai_) {
    saved_player_ai_->AddRef();
    preview_player_->SetAI(nullptr);
  }
  preview_player_->SetMovLock(true);
  preview_player_->SetPhysicsEnabled(false);
  preview_player_->SetCollDetStat(false);
  preview_player_->SetCollDetDyn(false);
  preview_player_->SetSleeping(true);
  preview_position_ = zVEC3(29912.9f, 5185.68f, -15710.0f);
  context_.game->GetWorldTimer()->GetTime(saved_hour_, saved_minute_);
}

void CharacterCreationState::StopPreviewAnimation() {
  if (preview_player_ && Gothic_II_Addon::player == preview_player_) {
    if (auto* model = preview_player_->GetModel()) {
      model->StopAnisLayerRange(0, 100);
    }
  }
}

void CharacterCreationState::OnExit() {
  StopScene();
}

void CharacterCreationState::StopScene() {
  StopPreviewAnimation();
  camera_.Reset();
  if (preview_player_ && Gothic_II_Addon::player == preview_player_) {
    preview_player_->GetEM()->Clear();
    gmp::gothic::ApplyCharacterAppearance(*preview_player_, context_.config.GetCharacterAppearance());
    if (auto* model = preview_player_->GetModel()) {
      model->SetRandAnisEnabled(saved_random_animations_);
    }
    preview_player_->SetTrafoObjToWorld(saved_transform_);
    preview_player_->SetCollDetStat(saved_static_collision_);
    preview_player_->SetCollDetDyn(saved_dynamic_collision_);
    preview_player_->SetPhysicsEnabled(saved_physics_enabled_);
    preview_player_->SetMovLock(saved_movement_lock_);
    preview_player_->SetAI(saved_player_ai_);
    if (preview_player_->human_ai) {
      preview_player_->human_ai->StartStandAni();
    }
    preview_player_->SetSleepingMode(saved_sleeping_mode_);
    context_.game->GetWorldTimer()->SetTime(saved_hour_, saved_minute_);
  }
  if (saved_player_ai_) {
    saved_player_ai_->Release();
    saved_player_ai_ = nullptr;
  }
  preview_player_ = nullptr;
}

void CharacterCreationState::UpdateScene(float delta_time) {
  if (!preview_player_ || Gothic_II_Addon::player != preview_player_ || !camera_.IsReady()) {
    return;
  }
  context_.game->GetWorldTimer()->SetTime(12, 0);
  if (appearance_changed_) {
    StopPreviewAnimation();
    gmp::gothic::ApplyCharacterAppearance(*preview_player_, appearance_);
    appearance_changed_ = false;
    animation_changed_ = true;
  }
  if (!grounded_) {
    preview_player_->SetPositionWorld(preview_position_);
    auto position = preview_position_;
    preview_player_->SetOnFloor(position);
    preview_position_ = preview_player_->GetPositionWorld();
    const float heading = kPreviewAngle * kRadiansPerDegree;
    preview_player_->SetHeadingYWorld(preview_position_ + zVEC3(std::sin(heading), 0.0f, std::cos(heading)));
    grounded_ = true;
  }
  if (auto* model = preview_player_->GetModel()) {
    model->SetRandAnisEnabled(false);
    const zSTRING animation(selected_field_ == Walking ? "S_WALKL" : "S_RUN");
    if (animation_changed_ || !model->IsAnimationActive(animation)) {
      StopPreviewAnimation();
      model->StartAnimation(animation);
      animation_changed_ = false;
    }
    if (ztimer && ztimer->frameTimeFloatSecs > 0.0f && std::isfinite(delta_time) && delta_time > 0.0f) {
      const float time_scale = model->timeScale;
      model->timeScale = time_scale * std::min(delta_time, 0.1f) / ztimer->frameTimeFloatSecs;
      model->AdvanceAnis();
      model->timeScale = time_scale;
      // Ignore the animation's root motion; the preview actor never moves.
      preview_player_->SetBBox3DLocal(model->GetBBox3D());
    }
  }
  preview_player_->SetSleeping(true);
  const auto& camera = kCameras[selected_field_];
  const float angle = kPreviewAngle + camera.angle;
  const float radians = angle * kRadiansPerDegree;
  camera_.Apply(preview_position_ + zVEC3(camera.distance * std::sin(radians), camera.height, camera.distance * std::cos(radians)),
                0.0f, angle + 180.0f);
}

void CharacterCreationState::Render() {
  const auto font = context_.language.ApplyFontPrefix("FONT_DEFAULT.TGA");
  context_.screen->SetFont(font.c_str());
  context_.screen->SetFontColor(Normal);
  context_.screen->Print(100, 200, context_.language[Language::MMENU_APPEARANCE]);
  context_.screen->Print(100, 7600, context_.language[Language::APP_CONTROLS]);
  context_.screen->Print(100, 7850, context_.language[Language::APP_SAVE_HINT]);
  const std::array<Language::STRING_ID, FieldCount> labels{
      Language::APP_BODY, Language::HEAD_MODEL, Language::FACE_APPEARANCE, Language::SKIN_TEXTURE, Language::WALK_STYLE};
  const std::array<std::string, FieldCount> values{
      context_.language[kBodyLabels[ChoiceIndex(gmp::kCharacterBodies, appearance_.body_model)]].ToChar(),
      context_.language[kHeadLabels[ChoiceIndex(gmp::kCharacterHeads, appearance_.head_model)]].ToChar(),
      std::to_string(appearance_.head_texture), std::to_string(appearance_.body_texture),
      context_.language[kWalkingLabels[ChoiceIndex(gmp::kCharacterWalkingOverlays, appearance_.walking_overlay)]].ToChar()};
  for (int i = 0; i < FieldCount; ++i) {
    context_.screen->SetFontColor(i == selected_field_ ? Highlighted : Normal);
    const auto text = std::string(context_.language[labels[i]].ToChar()) + ": " + values[i];
    context_.screen->Print(200, 2200 + i * 400, zSTRING(text.c_str()));
  }
}

void CharacterCreationState::ChangeSelection(int change) {
  switch (selected_field_) {
    case Body:
      appearance_.body_model = gmp::kCharacterBodies[Cycle(ChoiceIndex(gmp::kCharacterBodies, appearance_.body_model), change,
                                                         static_cast<int>(gmp::kCharacterBodies.size()))];
      break;
    case Head:
      appearance_.head_model = gmp::kCharacterHeads[Cycle(ChoiceIndex(gmp::kCharacterHeads, appearance_.head_model), change,
                                                        static_cast<int>(gmp::kCharacterHeads.size()))];
      break;
    case Face:
      appearance_.head_texture = Cycle(appearance_.head_texture, change, gmp::kMaxCharacterHeadTexture + 1);
      break;
    case Skin:
      appearance_.body_texture = Cycle(appearance_.body_texture, change, gmp::kMaxCharacterBodyTexture + 1);
      break;
    case Walking:
      appearance_.walking_overlay = gmp::kCharacterWalkingOverlays[Cycle(
          ChoiceIndex(gmp::kCharacterWalkingOverlays, appearance_.walking_overlay), change, static_cast<int>(gmp::kCharacterWalkingOverlays.size()))];
      break;
  }
  appearance_changed_ = true;
}

StateResult CharacterCreationState::Update() {
  Render();
  if (context_.input->KeyToggled(KEY_RETURN) || context_.input->KeyToggled(KEY_ESCAPE)) {
    // Escape also saves, matching the original repeatable appearance menu.
    context_.config.SetCharacterAppearance(appearance_);
    context_.config.SaveConfigToFile();
    context_.input->ClearKeyBuffer();
    finished_ = true;
    return StateResult::Continue;
  }
  const bool was_walking = selected_field_ == Walking;
  if (context_.input->KeyToggled(KEY_UP)) {
    selected_field_ = Cycle(selected_field_, -1, FieldCount);
  } else if (context_.input->KeyToggled(KEY_DOWN)) {
    selected_field_ = Cycle(selected_field_, 1, FieldCount);
  }
  if (was_walking != (selected_field_ == Walking)) {
    animation_changed_ = true;
  }
  if (context_.input->KeyToggled(KEY_LEFT) || context_.input->KeyToggled(KEY_A)) {
    ChangeSelection(-1);
  } else if (context_.input->KeyToggled(KEY_RIGHT) || context_.input->KeyToggled(KEY_D)) {
    ChangeSelection(1);
  }
  return StateResult::Continue;
}

MenuState* CharacterCreationState::CheckTransition() {
  return finished_ ? new MainMenuLoopState(context_) : nullptr;
}

}  // namespace menu::states

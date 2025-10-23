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

#include "appearance_state.hpp"

#include <spdlog/spdlog.h>

#include "gothic2a_player.hpp"
#include "keyboard.h"
#include "language.h"
#include "menu/states/main_menu_loop_state.hpp"

namespace menu {
namespace states {

constexpr const char* WalkAnim = "S_WALKL";

AppearanceState::AppearanceState(MenuContext& context)
    : context_(context), currentPart_(AppearancePart::FACE), lastPart_(AppearancePart::FACE), shouldReturnToMainMenu_(false) {
}

void AppearanceState::OnEnter() {
  SPDLOG_INFO("Entering appearance state");
  SetupAppearanceCamera();
}

void AppearanceState::OnExit() {
  SPDLOG_INFO("Exiting appearance state");
  CleanupAppearanceCamera();

  // Save appearance settings
  context_.config.SaveConfigToFile();
}

StateResult AppearanceState::Update() {
  RenderAppearanceUI();
  HandleInput();
  return StateResult::Continue;
}

MenuState* AppearanceState::CheckTransition() {
  if (shouldReturnToMainMenu_) {
    return new MainMenuLoopState(context_);
  }
  return nullptr;
}

void AppearanceState::SetupAppearanceCamera() {
  if (!context_.appearanceCameraCreated) {
    // Create weapon for camera positioning
    context_.appearanceWeapon = zfactory->CreateItem(zCParser::GetParser()->GetIndex("ItMw_1h_Mil_Sword"));
    context_.appearanceWeapon->SetPositionWorld(zVEC3(context_.player->GetPositionWorld()[VX] - 78, context_.player->GetPositionWorld()[VY] + 50,
                                                      context_.player->GetPositionWorld()[VZ] - 119));
    context_.appearanceWeapon->RotateWorldY(30);
    context_.appearanceWeapon->name.Clear();

    // Apply current appearance settings to player
    auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(context_.config.headmodel);
    context_.player->SetAdditionalVisuals("HUM_BODY_NAKED0", context_.config.skintexture, 0, head_model, context_.config.facetexture, 0, -1);
    context_.player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = context_.config.skintexture;

    // Initialize to face view
    currentPart_ = AppearancePart::FACE;
    lastPart_ = AppearancePart::FACE;

    // Init camera to focus on appearance weapon
    ogame->CamInit(context_.appearanceWeapon, zCCamera::activeCam);
    context_.appearanceCameraCreated = true;
  } else {
    // Reset to face view if re-entering
    currentPart_ = AppearancePart::FACE;
    lastPart_ = AppearancePart::FACE;
    context_.appearanceWeapon->ResetRotationsWorld();
    context_.appearanceWeapon->SetPositionWorld(zVEC3(context_.player->GetPositionWorld()[VX] - 78, context_.player->GetPositionWorld()[VY] + 50,
                                                      context_.player->GetPositionWorld()[VZ] - 119));
    context_.appearanceWeapon->RotateWorldY(30);
    ogame->CamInit(context_.appearanceWeapon, zCCamera::activeCam);
  }
}

void AppearanceState::CleanupAppearanceCamera() {
  // Return camera to title weapon
  ogame->CamInit(context_.cameraWeapon, zCCamera::activeCam);
}

void AppearanceState::RenderAppearanceUI() {
  // Display info text
  context_.screen->SetFont("FONT_DEFAULT.TGA");
  context_.screen->SetFontColor({255, 255, 255});
  context_.screen->Print(100, 200, Language::Instance()[Language::APP_INFO1]);

  // Display current part being edited
  switch (currentPart_) {
    case AppearancePart::HEAD:
      context_.screen->Print(500, 2000, Language::Instance()[Language::HEAD_MODEL]);
      break;
    case AppearancePart::FACE:
      context_.screen->Print(500, 2000, Language::Instance()[Language::FACE_APPERANCE]);
      break;
    case AppearancePart::SKIN:
      context_.screen->Print(500, 2000, Language::Instance()[Language::SKIN_TEXTURE]);
      break;
    case AppearancePart::WALKSTYLE:
      context_.screen->Print(500, 2000, Language::Instance()[Language::WALK_STYLE]);
      break;
  }
}

void AppearanceState::HandleInput() {
  // Navigate between appearance parts
  if (context_.input->KeyToggled(KEY_UP) && currentPart_ > AppearancePart::HEAD) {
    currentPart_ = static_cast<AppearancePart>(static_cast<int>(currentPart_) - 1);
  }

  if (context_.input->KeyToggled(KEY_DOWN) && currentPart_ < AppearancePart::WALKSTYLE) {
    currentPart_ = static_cast<AppearancePart>(static_cast<int>(currentPart_) + 1);
  }

  // Exit on ESC
  if (context_.input->KeyPressed(KEY_ESCAPE)) {
    context_.input->ClearKeyBuffer();
    shouldReturnToMainMenu_ = true;
    return;
  }

  // Adjust current part
  AdjustCurrentPart(0);
}

void AppearanceState::NavigateParts(int direction) {
  // Not needed - handled in HandleInput
}

void AppearanceState::AdjustCurrentPart(int direction) {
  switch (currentPart_) {
    case AppearancePart::HEAD:
      // Left/Right to change head model
      if (context_.input->KeyToggled(KEY_LEFT)) {
        if (context_.config.headmodel > 0) {
          context_.config.headmodel--;
          ApplyAppearanceChanges();
        }
      }
      if (context_.input->KeyToggled(KEY_RIGHT)) {
        if (context_.config.headmodel < 5) {
          context_.config.headmodel++;
          ApplyAppearanceChanges();
        }
      }
      // Update camera position if part changed
      if (lastPart_ != currentPart_) {
        context_.appearanceWeapon->SetPositionWorld(zVEC3(context_.player->GetPositionWorld()[VX] + 55, context_.player->GetPositionWorld()[VY] + 70,
                                                          context_.player->GetPositionWorld()[VZ] - 37));
        context_.appearanceWeapon->RotateWorldY(-90);
        lastPart_ = currentPart_;
      }
      break;

    case AppearancePart::FACE:
      // Left/Right to change face texture
      if (context_.input->KeyToggled(KEY_LEFT)) {
        if (context_.config.facetexture > 0) {
          context_.config.facetexture--;
          ApplyAppearanceChanges();
        }
      }
      if (context_.input->KeyToggled(KEY_RIGHT)) {
        if (context_.config.facetexture < 162) {
          context_.config.facetexture++;
          ApplyAppearanceChanges();
        }
      }
      // Update camera position if part changed
      if (lastPart_ != currentPart_) {
        context_.appearanceWeapon->SetPositionWorld(zVEC3(context_.player->GetPositionWorld()[VX] - 78, context_.player->GetPositionWorld()[VY] + 50,
                                                          context_.player->GetPositionWorld()[VZ] - 119));
        context_.appearanceWeapon->RotateWorldY(90);
        lastPart_ = currentPart_;
      }
      break;

    case AppearancePart::SKIN:
      // Stop walk animation if active
      if (context_.player->GetModel()->IsAnimationActive(WalkAnim)) {
        context_.player->GetModel()->StopAnimation(WalkAnim);
      }

      // Left/Right to change skin texture
      if (context_.input->KeyToggled(KEY_LEFT)) {
        if (context_.config.skintexture > 0) {
          context_.config.skintexture--;
          ApplyAppearanceChanges();
          context_.player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = context_.config.skintexture;
        }
      }
      if (context_.input->KeyToggled(KEY_RIGHT)) {
        if (context_.config.skintexture < 12) {
          context_.config.skintexture++;
          ApplyAppearanceChanges();
          context_.player->GetModel()->meshLibList[0]->texAniState.actAniFrames[0][0] = context_.config.skintexture;
        }
      }
      break;

    case AppearancePart::WALKSTYLE:
      // Left/Right to change walk style
      if (context_.input->KeyPressed(KEY_LEFT)) {
        context_.input->ClearKeyBuffer();
        if (context_.config.walkstyle > 0) {
          if (context_.player->GetModel()->IsAnimationActive(WalkAnim)) {
            context_.player->GetModel()->StopAnimation(WalkAnim);
          }
          auto walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(context_.config.walkstyle);
          context_.player->RemoveOverlay(walkstyle_tmp);
          context_.config.walkstyle--;
          walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(context_.config.walkstyle);
          context_.player->ApplyOverlay(walkstyle_tmp);
        }
      }
      if (context_.input->KeyPressed(KEY_RIGHT)) {
        context_.input->ClearKeyBuffer();
        if (context_.config.walkstyle < 6) {
          if (context_.player->GetModel()->IsAnimationActive(WalkAnim)) {
            context_.player->GetModel()->StopAnimation(WalkAnim);
          }
          auto walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(context_.config.walkstyle);
          context_.player->RemoveOverlay(walkstyle_tmp);
          context_.config.walkstyle++;
          walkstyle_tmp = Gothic2APlayer::GetWalkStyleFromByte(context_.config.walkstyle);
          context_.player->ApplyOverlay(walkstyle_tmp);
        }
      }
      // Play walk animation
      if (!context_.player->GetModel()->IsAnimationActive(WalkAnim)) {
        context_.player->GetModel()->StartAnimation(WalkAnim);
      }
      // Keep player in position
      context_.player->trafoObjToWorld.SetTranslation(context_.savedPlayerPosition);
      break;
  }
}

void AppearanceState::UpdateCameraPosition() {
  // Not needed - handled in AdjustCurrentPart
}

void AppearanceState::ApplyAppearanceChanges() {
  auto head_model = Gothic2APlayer::GetHeadModelNameFromByte(context_.config.headmodel);
  context_.player->SetAdditionalVisuals("HUM_BODY_NAKED0", context_.config.skintexture, 0, head_model, context_.config.facetexture, 0, -1);
}

}  // namespace states
}  // namespace menu

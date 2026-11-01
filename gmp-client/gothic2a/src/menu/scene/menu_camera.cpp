/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include "menu/scene/menu_camera.h"

#include <cmath>

namespace menu {
namespace {
bool IsFinite(const zVEC3& position) {
  return std::isfinite(position[VX]) && std::isfinite(position[VY]) && std::isfinite(position[VZ]);
}
}  // namespace

MenuCamera::~MenuCamera() {
  Reset();
}

bool MenuCamera::IsReady() const {
  return game_ && game_->GetWorld() && zCCamera::activeCam && game_->GetCameraVob() && (!anchor_ || anchor_ == game_->GetCameraVob()) &&
         game_->GetCameraVob()->GetHomeWorld() == game_->GetWorld();
}

bool MenuCamera::Apply(const zVEC3& position, float pitch, float yaw, float roll) {
  if (!IsReady() || !IsFinite(position) || !std::isfinite(pitch) || !std::isfinite(yaw) || !std::isfinite(roll)) {
    return false;
  }
  if (!anchor_) {
    // CamInit(vob, camera) overwrites the session's owning camVob pointer.
    // Reusing that vob avoids transferring/releasing the session's reference
    // and preserves its AI camera target and all other gameplay camera state.
    anchor_ = game_->GetCameraVob();
    anchor_->AddRef();
    saved_transform_ = anchor_->trafoObjToWorld;
    saved_sleeping_mode_ = static_cast<zTVobSleepingMode>(anchor_->sleepingMode);
    saved_ai_ = anchor_->callback_ai;
    if (saved_ai_) {
      saved_ai_->AddRef();
    }
    anchor_->SetAI(nullptr);
  }
  if (anchor_ != game_->GetCameraVob() || anchor_->GetHomeWorld() != game_->GetWorld()) {
    return false;
  }
  SetPosition(position);
  SetRotation(pitch, yaw, roll);
  anchor_->SetSleeping(true);
  zCCamera::activeCam->Activate();
  return true;
}

void MenuCamera::SetPosition(const zVEC3& position) {
  if (anchor_ && IsFinite(position)) {
    anchor_->SetPositionWorld(position);
  }
}

void MenuCamera::SetRotation(float pitch, float yaw, float roll) {
  if (anchor_ && std::isfinite(pitch) && std::isfinite(yaw) && std::isfinite(roll)) {
    anchor_->ResetRotationsWorld();
    anchor_->RotateWorldZ(std::remainder(roll, 360.0f));
    anchor_->RotateWorldX(std::remainder(pitch, 360.0f));
    anchor_->RotateWorldY(std::remainder(yaw, 360.0f));
  }
}

void MenuCamera::LookAt(const zVEC3& target) {
  if (anchor_ && IsFinite(target)) {
    const auto direction = target - anchor_->GetPositionWorld();
    if (direction.Length_Sqr() > 0.001f) {
      anchor_->SetHeadingAtWorld(direction / direction.Length());
    }
  }
}

bool MenuCamera::IsBehindCamera(const zVEC3& position, float margin) const {
  auto* camera = zCCamera::activeCam;
  if (!IsReady() || !anchor_ || camera->connectedVob != anchor_ || !IsFinite(position)) {
    return false;
  }
  camera->Activate();
  return (camera->camMatrix * position)[VZ] < -margin;
}

bool MenuCamera::Project(const zVEC3& position, float& x, float& y) const {
  auto* camera = zCCamera::activeCam;
  if (!IsReady() || !anchor_ || camera->connectedVob != anchor_ || !IsFinite(position)) {
    return false;
  }
  camera->Activate();
  const zVEC3 local = camera->camMatrix * position;
  if (!IsFinite(local) || local[VZ] <= 0.0f || local[VZ] <= camera->nearClipZ || local[VZ] >= camera->farClipZ) {
    return false;
  }
  camera->Project(&local, x, y);
  const auto& viewport = camera->vpData;
  return std::isfinite(x) && std::isfinite(y) && viewport.xdim > 0 && viewport.ydim > 0 && x >= viewport.xmin && x < viewport.xmin + viewport.xdim &&
         y >= viewport.ymin && y < viewport.ymin + viewport.ydim;
}

void MenuCamera::Reset() {
  if (!anchor_) {
    return;
  }
  // Only restore the camera we acquired, never a replacement from a world load.
  if (game_ && game_->GetCameraVob() == anchor_ && anchor_->GetHomeWorld() == game_->GetWorld()) {
    anchor_->SetTrafoObjToWorld(saved_transform_);
    anchor_->SetAI(saved_ai_);
    anchor_->SetSleepingMode(saved_sleeping_mode_);
  }
  if (saved_ai_) {
    saved_ai_->Release();
    saved_ai_ = nullptr;
  }
  anchor_->Release();
  anchor_ = nullptr;
}

}  // namespace menu

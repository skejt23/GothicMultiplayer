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

#include "menu_scene_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <random>

#include "menu/scenes/scene_registry.h"

namespace menu {

SceneManager::SceneManager(oCGame* game) : game_(game), camera_(game) {
}
SceneManager::~SceneManager() {
  Cleanup();
}

void SceneManager::Configure(bool extended) {
  const bool show_weapon = weapon_requested_;
  Cleanup();
  scenes::RegisterBasicMenuScenes(*this, !extended);
  if (extended) {
    scenes::RegisterExtendedMenuScenes(*this);
  }
  weapon_requested_ = show_weapon;
  pending_start_ = true;
}

void SceneManager::RegisterScene(std::string name, SceneFactory factory, bool include_in_cycle) {
  if (!factory || std::any_of(scenes_.begin(), scenes_.end(), [&](const auto& scene) { return scene.name == name; })) {
    SPDLOG_WARN("Ignoring invalid/duplicate menu scene registration: {}", name);
    return;
  }
  scenes_.push_back({std::move(name), std::move(factory), include_in_cycle});
}

bool SceneManager::IsReady() const {
  return camera_.IsReady() && Gothic_II_Addon::player && Gothic_II_Addon::player->GetHomeWorld() == game_->GetWorld();
}

bool SceneManager::TryActivate(size_t index) {
  if (index >= scenes_.size() || scenes_[index].failed || !IsReady()) {
    return false;
  }
  StopScene();
  auto& entry = scenes_[index];
  if (!remove_range_saved_) {
    saved_remove_range_ = oCSpawnManager::GetRemoveRange();
    remove_range_saved_ = true;
    oCSpawnManager::SetRemoveRange(2097152.0f);
  }
  try {
    active_scene_ = entry.create();
    active_scene_name_ = entry.name;
    if (active_scene_) {
      const auto settings = active_scene_->GetSettings();
      if (camera_.Apply(settings.camera_position, settings.camera_pitch, settings.camera_yaw, settings.camera_roll) && active_scene_->Start()) {
        pending_start_ = false;
        if (weapon_requested_) {
          ShowWeapon();
        }
        SPDLOG_INFO("Main menu scene started: {}", active_scene_name_);
        return true;
      }
    }
  } catch (const std::exception& error) {
    SPDLOG_ERROR("Main menu scene '{}': {}", entry.name, error.what());
  }
  SPDLOG_WARN("Main menu scene initialization failed: {}", entry.name);
  entry.failed = true;
  StopScene();  // Also handles partial initialization; the scene destructor is a second RAII guard.
  return false;
}

bool SceneManager::ActivateScene(const std::string& name) {
  // Copy the index before StopScene can clear active_scene_name_ (used by restart).
  for (size_t i = 0; i < scenes_.size(); ++i) {
    if (scenes_[i].name == name) {
      return TryActivate(i);
    }
  }
  return false;
}

bool SceneManager::ActivateNextScene() {
  size_t first = 0;
  for (size_t i = 0; i < scenes_.size(); ++i) {
    if (scenes_[i].name == active_scene_name_) {
      first = i + 1;
      break;
    }
  }
  for (size_t count = 0; count < scenes_.size(); ++count) {
    const size_t i = (first + count) % scenes_.size();
    if (scenes_[i].include_in_cycle && TryActivate(i)) {
      return true;
    }
  }
  return ActivateScene(kDefaultSceneName);
}

bool SceneManager::ActivateRandomScene() {
  std::vector<size_t> candidates;
  for (size_t i = 0; i < scenes_.size(); ++i) {
    if (scenes_[i].include_in_cycle && !scenes_[i].failed) {
      candidates.push_back(i);
    }
  }
  static std::mt19937 random(std::random_device{}());
  std::shuffle(candidates.begin(), candidates.end(), random);
  for (const auto i : candidates) {
    SPDLOG_DEBUG("Main menu scene selected: {}", scenes_[i].name);
    if (TryActivate(i)) {
      return true;
    }
  }
  return ActivateScene(kDefaultSceneName);
}

void SceneManager::MarkActiveSceneFailed() {
  SPDLOG_WARN("Stopping unhealthy main menu scene: {}", active_scene_name_);
  for (auto& entry : scenes_) {
    if (entry.name == active_scene_name_) {
      entry.failed = true;
    }
  }
  StopScene();
  pending_start_ = true;
}

void SceneManager::Update(float delta_time) {
  if (pending_start_ && IsReady()) {
    // Wait for bootstrap prerequisites once; never retry broken assets each frame.
    pending_start_ = false;
    ActivateRandomScene();
  }
  if (!active_scene_) {
    return;
  }
  if (!IsReady()) {
    StopScene();
    return;
  }
  try {
    // A suspended window must not cause actors to jump across the entire stage.
    active_scene_->Update(std::isfinite(delta_time) ? std::clamp(delta_time, 0.0f, 0.1f) : 0.0f);
    if (!active_scene_->IsHealthy()) {
      MarkActiveSceneFailed();
    }
  } catch (const std::exception& error) {
    SPDLOG_ERROR("Main menu scene update failed: {}", error.what());
    MarkActiveSceneFailed();
  }
}

void SceneManager::Render() {
  if (active_scene_ && IsReady()) {
    try {
      active_scene_->Render();
    } catch (const std::exception& error) {
      SPDLOG_ERROR("Main menu scene rendering failed: {}", error.what());
      MarkActiveSceneFailed();
    }
  }
}

void SceneManager::ResetActiveScene() {
  const std::string name = active_scene_name_;
  if (!name.empty()) {
    SPDLOG_DEBUG("Main menu scene restart: {}", name);
    if (!ActivateScene(name)) {
      ActivateRandomScene();
    }
  }
}

void SceneManager::StopScene() {
  pending_start_ = false;
  // Keep UI visibility intent when switching scenes, but release the old weapon.
  const bool requested = weapon_requested_;
  HideWeapon();
  weapon_requested_ = requested;
  if (active_scene_) {
    active_scene_->Stop();
    SPDLOG_INFO("Main menu scene stopped: {}", active_scene_name_);
    active_scene_.reset();
  }
  active_scene_name_.clear();
  camera_.Reset();
  if (remove_range_saved_) {
    oCSpawnManager::SetRemoveRange(saved_remove_range_);
    remove_range_saved_ = false;
  }
}

void SceneManager::ShowWeapon() {
  weapon_requested_ = true;
  if (active_weapon_ || !active_scene_ || !IsReady()) {
    return;
  }
  const auto settings = active_scene_->GetSettings();
  if (!settings.show_weapon || !settings.weapon_visual_name || !settings.weapon_baseline) {
    return;
  }
  auto* visual = zCVisual::LoadVisual(zSTRING(settings.weapon_visual_name));
  if (!visual) {
    SPDLOG_WARN("Menu weapon visual failed: {}", settings.weapon_visual_name);
    return;
  }
  active_weapon_ = new zCVob();
  active_weapon_->SetVisual(visual);
  visual->Release();  // SetVisual retains its own reference.
  const auto& baseline = *settings.weapon_baseline;
  camera_.SetPosition(baseline.camera_position);
  camera_.SetRotation(baseline.camera_pitch, baseline.camera_yaw);
  active_weapon_->SetPositionWorld(baseline.weapon_position);
  game_->GetWorld()->AddVobAsChild_novt(active_weapon_, camera_.GetAnchor());
  camera_.Apply(settings.camera_position, settings.camera_pitch, settings.camera_yaw, settings.camera_roll);
  active_weapon_->ResetRotationsWorld();
  active_weapon_->RotateWorldY(std::remainder(settings.camera_yaw - baseline.camera_yaw, 360.0f));
  active_scene_->SetWeapon(active_weapon_);
}

void SceneManager::HideWeapon() {
  weapon_requested_ = false;
  if (active_scene_) {
    active_scene_->SetWeapon(nullptr);
  }
  if (active_weapon_) {
    if (active_weapon_->GetHomeWorld()) {
      active_weapon_->RemoveVobFromWorld();
    }
    active_weapon_->Release();
    active_weapon_ = nullptr;
  }
}

void SceneManager::Cleanup() {
  StopScene();
  weapon_requested_ = false;
  scenes_.clear();
}

}  // namespace menu

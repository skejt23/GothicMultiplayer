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

#include "menu/scenes/extended/credits_walk_scene.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>

namespace menu::scenes {
namespace {
// Scene settings. Positions use ZEN world units (Y is up), rotations use
// degrees, and camera coordinates are applied without a terrain offset.
constexpr const char* kWorld = "NEWWORLD\\NEWWORLD.ZEN";

const MenuSceneSettings kCreditsCamera = [] {
  MenuSceneSettings camera;
  camera.camera_position = zVEC3(12067.4f, 892.671f, 5887.35f);
  camera.camera_pitch = 25.6f;
  camera.camera_yaw = -58.24f;
  camera.camera_roll = 0.0f;
  camera.show_weapon = false;
  return camera;
}();

// Each actor gets its own spawn, destination and visuals. FindRoute chooses
// connected waynet edges between the two names; no camera-relative movement.
// Each actor vanishes at its end_wp; the group resets once everyone arrives.
// Names credit the original client authors.
const auto kCreditsWalkers = std::to_array<MenuNpcDefinition>({
    {
        .name = "skejt23",
        .name_color = {255, 0, 0},
        .name_font = "CP1250_FONT_DEFAULT.TGA",
        .name_show = true,
        .name_fade_start = 1000.0f,
        .name_fade_end = 2000.0f,
        .visual = {"HUM_BODY_NAKED0", 8, "HUM_HEAD_FATBALD", 18},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_2H_PAL_SWORD",
        .equip_ranged = "",
        .equip_armor = "ITAR_PAL_H",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
        .freeze_on_wp = true,
    },
    {
        .name = "mecio",
        .name_color = {255, 255, 255},
        .name_show = true,
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_PAL_SWORD",
        .equip_armor = "ITAR_PAL_M",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "pampi",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 1, "HUM_HEAD_PONY", 58},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_PAL_SWORD",
        .equip_armor = "ITAR_PAL_M",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "Macrentofeth",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 1, "HUM_HEAD_PONY", 61},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_PAL_SWORD",
        .equip_armor = "ITAR_PAL_H",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "Sative",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 1, "HUM_HEAD_BALD", 7},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_MIL_M",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "Nubzior",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 1, "HUM_HEAD_BALD", 32},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_MIL_L",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "DamianQ",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 8, "HUM_HEAD_PONY", 26},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_MIL_M",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
    {
        .name = "DeathLock",
        .name_color = {255, 255, 255},
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 8, "HUM_HEAD_THIEF", 85},
        .overlay = "HUMANS_MILITIA.MDS",
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_MIL_L",
        .spawn_wp = "NW_CITY_MERCHANT_PATH_29_B",
        .end_wp = "NW_CITY_TO_FOREST_02",
    },
});
constexpr float kCreditsWalkSpeed = 120.0f;
constexpr float kCreditsWalkerSpacing = 180.0f;
constexpr float kCreditsReleaseInterval = kCreditsWalkerSpacing / kCreditsWalkSpeed;
// Pause after the last arrival before the group reappears at its spawn points.
constexpr float kCreditsResetDelay = 1.0f;
// Delay between releasing each walker from its spawn point.
size_t next_walker_to_release_ = 0;
float release_timer_ = 0.0f;
}  // namespace

MenuSceneSettings CreditsWalkScene::GetSettings() const {
  return kCreditsCamera;
}

bool CreditsWalkScene::Start() {
  Stop();
  auto* world = game_ ? game_->GetGameWorld() : nullptr;
  if (!world || !MenuNpc::CanCreate() || !world->wayNet || world->GetWorldFilename() != zSTRING(kWorld)) {
    return false;
  }
  for (size_t i = 0; i < kCreditsWalkers.size(); ++i) {
    const auto& actor = kCreditsWalkers[i];
    auto npc = MenuNpc::Create(*world, actor);
    if (!npc) {
      SPDLOG_WARN("Main menu scene CreditsWalkScene: failed to create actor {}", i);
      return false;
    }
    walkers_.push_back(std::move(npc));
  }
  healthy_ = ResetWalkers();
  return healthy_;
}

bool CreditsWalkScene::ResetWalkers() {
  reset_delay_ = 0.0f;
  release_timer_ = 0.0f;
  next_walker_to_release_ = 0;

  for (auto& walker : walkers_) {
    walker->SetVisible(false);

    if (!walker->ResetToSpawn()) {
      return false;
    }
  }

  // Start the first walker immediately.
  if (!walkers_.empty()) {
    if (!walkers_[0]->WalkToEndWaypoint(kCreditsWalkSpeed)) {
      return false;
    }

    walkers_[0]->SetVisible(true);
    next_walker_to_release_ = 1;
  }

  SPDLOG_DEBUG("Menu scene reset: CreditsWalkScene");
  return true;
}

void CreditsWalkScene::Update(float delta_time) {
  // Release queued walkers one by one.
  if (next_walker_to_release_ < walkers_.size()) {
    release_timer_ += delta_time;

    while (release_timer_ >= kCreditsReleaseInterval &&
           next_walker_to_release_ < walkers_.size()) {
      release_timer_ -= kCreditsReleaseInterval;

      auto& walker = walkers_[next_walker_to_release_];

      if (!walker->WalkToEndWaypoint(kCreditsWalkSpeed)) {
        healthy_ = false;
        return;
      }

      walker->SetVisible(true);
      ++next_walker_to_release_;
    }
  }

  for (size_t i = 0; i < walkers_.size(); ++i) {
    auto& walker = walkers_[i];

    // Don't update walkers that haven't been released yet.
    if (i >= next_walker_to_release_) {
      continue;
    }

    if (!walker->HasReachedDestination()) {
      walker->Update(delta_time);
    }

    if (walker->GetMovement() == MenuNpc::Movement::Failed) {
      healthy_ = false;
      return;
    }

    if (walker->HasReachedDestination()) {
      walker->SetVisible(false);
    }
  }

  const bool all_released =
      next_walker_to_release_ >= walkers_.size();

  const bool all_arrived =
      all_released &&
      !walkers_.empty() &&
      std::all_of(
          walkers_.begin(),
          walkers_.end(),
          [](const auto& walker) {
            return walker->HasReachedDestination();
          });

  if (all_arrived) {
    reset_delay_ += delta_time;

    if (reset_delay_ >= kCreditsResetDelay) {
      healthy_ = ResetWalkers();
    }
  } else {
    reset_delay_ = 0.0f;
  }
}

void CreditsWalkScene::Render() {
  for (const auto& walker : walkers_) {
    nameplates_.Render(*walker, camera_);
  }
}

bool CreditsWalkScene::IsHealthy() const {
  return healthy_ && !walkers_.empty() && std::all_of(walkers_.begin(), walkers_.end(), [](const auto& walker) { return walker->IsValid(); });
}

void CreditsWalkScene::Stop() {
  healthy_ = false;
  reset_delay_ = 0.0f;
  walkers_.clear();
}

}  // namespace menu::scenes

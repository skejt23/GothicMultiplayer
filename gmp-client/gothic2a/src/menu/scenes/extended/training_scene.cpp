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

#include "menu/scenes/extended/training_scene.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>

namespace menu::scenes {
namespace {
// Scene settings. Positions use ZEN world units (Y is up), rotations use
// degrees, and camera coordinates are applied without a terrain offset.
constexpr const char* kWorld = "NEWWORLD\\NEWWORLD.ZEN";

const MenuSceneSettings kTrainingCamera = [] {
  MenuSceneSettings camera;
  camera.camera_position = zVEC3(4850.34f, 1247.35f, 6502.66f);
  camera.camera_pitch = 28.0f;
  camera.camera_yaw = -12.8f;
  camera.camera_roll = 0.0f;
  camera.show_weapon = false;
  return camera;
}();

// Each entry controls its own animation and weapon state. The array size is
// inferred, so adding/removing an actor only requires editing this list.
const auto kTrainees = std::to_array<MenuNpcDefinition>({
    {
        .name = "Patrix",
        .name_color = {255, 255, 255},
        .name_font = "CP1250_FONT_DEFAULT.TGA",
        .name_show = true,
        .name_fade_start = 1000.0f,
        .name_fade_end = 2000.0f,
        .instance = "PC_HERO",
        .visual = {"HUM_BODY_NAKED0", 1, "HUM_HEAD_BALD", 5},
        .overlay = "",
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_ranged = "",
        .equip_armor = "ITAR_BLOODWYN_ADDON",
        .spawn_wp = "NW_CITY_HABOUR_KASERN_CENTRE_02",
        .end_wp = "",
        .freeze_on_wp = false,
        .animation = "T_1HSFREE",
        .animation_loop = true,
        .draw_melee = true,
    },
    {
        .name = "Bimbol",
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 0, "HUM_HEAD_PONY", 44},
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_BLOODWYN_ADDON",
        .spawn_wp = "NW_CITY_HABOUR_KASERN_CENTRE_03",
        .freeze_on_wp = false,
        .animation = "T_1HSFREE",
        .animation_loop = true,
        .draw_melee = true,
    },
    {
        .name = "Martis",
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 0, "HUM_HEAD_PONY", 44},
        .equip_melee = "ITMW_1H_MIL_SWORD",
        .equip_armor = "ITAR_BLOODWYN_ADDON",
        .spawn_wp = "NW_CITY_HABOUR_KASERN_CENTRE_04",
        .freeze_on_wp = false,
        .animation = "T_1HSFREE",
        .animation_loop = true,
        .draw_melee = true,
    },
    {
        .name = "Profesores",
        .name_show = true,
        .visual = {"HUM_BODY_NAKED0", 0, "HUM_HEAD_FATBALD", 97},
        .equip_melee = "ITMW_1H_PAL_SWORD",
        .equip_armor = "ITAR_THORUS_ADDON",
        .spawn_wp = "NW_CITY_HABOUR_KASERN_CENTRE_01",
        .freeze_on_wp = true,
        .animation = "S_LGUARD",  // Standing with arms crossed.
        .animation_loop = true,
        .draw_melee = false,  // Keep the sword sheathed for the idle pose.
    },
});
constexpr float kTrainingYaw = 30.0f;
}  // namespace

MenuSceneSettings TrainingScene::GetSettings() const {
  return kTrainingCamera;
}

bool TrainingScene::Start() {
  Stop();
  auto* world = game_ ? game_->GetGameWorld() : nullptr;
  if (!world || !MenuNpc::CanCreate() || !world->wayNet || world->GetWorldFilename() != zSTRING(kWorld)) {
    return false;
  }
  for (size_t i = 0; i < kTrainees.size(); ++i) {
    const auto& actor = kTrainees[i];
    auto npc = MenuNpc::Create(*world, actor);
    if (!npc) {
      SPDLOG_WARN("Main menu scene TrainingScene: failed to create actor {} ({})", i, actor.name);
      return false;
    }
    npc->SetRotation(kTrainingYaw);
    trainees_.push_back(std::move(npc));
  }
  return !trainees_.empty();
}

void TrainingScene::Update(float delta_time) {
  for (auto& trainee : trainees_) {
    trainee->Update(delta_time);
  }
}

bool TrainingScene::IsHealthy() const {
  return !trainees_.empty() && std::all_of(trainees_.begin(), trainees_.end(), [](const auto& trainee) { return trainee->IsValid(); });
}

void TrainingScene::Render() {
  for (const auto& trainee : trainees_) {
    nameplates_.Render(*trainee, camera_);
  }
}
void TrainingScene::Stop() {
  trainees_.clear();
}

}  // namespace menu::scenes

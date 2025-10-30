
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include <optional>

#include "hooking/MemoryPatch.h"
#include "ZenGin/zGothicAPI.h"
#include "patch_install.hpp"

#define gLogStatistics *(int*)(0x008C2B50)
#define gStatusReadGameOptions *(int*)(0x008B21D4)
#define zOPT_SEC_OPTIONS *(zSTRING*)(0x008CD6E0)
#define show_Focus *(int*)(0x008B21D8)
#define show_FocusItm *(int*)(0x008B21DC)
#define show_FocusMob *(int*)(0x008B21E0)
#define show_FocusNpc *(int*)(0x008B21E4)
#define show_FocusBar *(int*)(0x008B21E8)
#define s_bTargetLocked *(int*)(0x00AB2680)

void* DisMember(size_t size, ...) {
  if (size != sizeof(void*))
    return nullptr;
  va_list args;
  va_start(args, size);
  void* res = va_arg(args, void*);
  va_end(args);
  return res;
}

void InstalloCGamePatches() {
  CallPatch(0x006C89D8, (DWORD)DisMember(sizeof(void (oCGame::*)()), &oCGame::UpdatePlayerStatus), 0);
};

namespace Gothic_II_Addon {

namespace {

// Constants for focus positioning
constexpr float MODEL_HEIGHT_MULTIPLIER = 1.6f;
constexpr float BBOX_HEIGHT_OFFSET_MULTIPLIER = 0.82f;
constexpr float FOCUS_HEIGHT_LIMIT_OFFSET = 100.0f;
constexpr int MAX_SCREEN_WIDTH = 8192;
constexpr int MAX_SCREEN_HEIGHT = 8191;
constexpr float DIVE_TIME_INVALID = -1000000.0f;

struct FocusInfo {
  zSTRING name;
  int current_health{};
  int health_max{};
};

std::optional<FocusInfo> GetFocusInfo(zCVob* vob) {
  FocusInfo info{};
  if (oCMOB* mob = zDYNAMIC_CAST<oCMOB>(vob)) {
    if (!show_FocusMob) {
      return std::nullopt;
    }
    info.name = mob->GetName();
  } else if (oCNpc* npc = zDYNAMIC_CAST<oCNpc>(vob)) {
    if ((!show_FocusNpc && !show_FocusBar) || npc->noFocus) {
      return std::nullopt;
    }
    if (show_FocusNpc) {
      info.name = npc->GetName();
    }
    if (show_FocusBar) {
      info.current_health = npc->GetAttribute(NPC_ATR_HITPOINTS);
      info.health_max = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
    }
  } else if (oCItem* item = zDYNAMIC_CAST<oCItem>(vob)) {
    if (!show_FocusItm) {
      return std::nullopt;
    }
    info.name = item->GetName(1);
  } else {
    return std::nullopt;
  }
  return info;
}

zVEC3 CalculateFocusPosition(zCVob* focus_vob) {
  zVEC3 focus_pos;
  if (zCModel* model = zDYNAMIC_CAST<zCModel>(focus_vob->visual)) {
    focus_pos = focus_vob->GetPositionWorld();
    focus_pos[VY] += model->bbox3DLocalFixed.maxs[VY] * MODEL_HEIGHT_MULTIPLIER;
    float max_height = focus_vob->bbox3D.maxs[VY] + FOCUS_HEIGHT_LIMIT_OFFSET;
    if (focus_pos[VY] > max_height)
      focus_pos[VY] = max_height;
  } else {
    const auto& bbox = focus_vob->bbox3D;
    focus_pos = bbox.GetCenter();

    // Offset Y position upward by 82% of the bounding box height
    float bbox_height = bbox.maxs[VY] - bbox.mins[VY];
    focus_pos[VY] += bbox_height * BBOX_HEIGHT_OFFSET_MULTIPLIER;
  }
  return focus_pos;
}

void RenderFocusName(const zVEC3& world_pos, const zSTRING& name) {
  // Project 3D world position to screen coordinates
  zCCamera* camera = zCCamera::activeCam;
  camera->Activate();
  
  // Transform world position to camera space
  zVEC3 camera_pos_vec = camera->camMatrix * world_pos;

  // Skip if behind camera
  if (camera_pos_vec[VZ] <= 0.0f) {
    return;
  }
  
  // Project to 2D screen coordinates
  float inv_z = 1.0f / camera_pos_vec[VZ];
  float screen_x = camera_pos_vec[VX] * camera->viewDistanceX * inv_z + camera->vpData.xcenter;
  float screen_y = camera->vpData.ycenter - (camera_pos_vec[VY] * camera->viewDistanceY * inv_z);
  
  // Add newline to focus name and calculate text dimensions
  zSTRING display_name = name + "\n";
  int font_height = screen->FontSize(display_name);
  int half_text_width = screen->nax(font_height / 2);
  
  // Adjust screen position to center text horizontally
  screen_x -= half_text_width;
  
  // Convert to screen pixel coordinates with clamping
  int final_x = screen->anx(screen_x);
  int final_y = screen->any(screen_y);
  
  // Clamp X position to screen bounds
  int min_x = screen->FontY();
  int max_x = MAX_SCREEN_WIDTH - screen->FontY();
  if (final_x < min_x) final_x = min_x;
  if (final_x > max_x) final_x = max_x;
  
  // Clamp Y position to screen bounds
  font_height = screen->FontSize(display_name);
  if (final_y < 0) {
    final_y = 0;
  } else if (final_y > MAX_SCREEN_HEIGHT - font_height) {
    final_y = MAX_SCREEN_HEIGHT - font_height;
  }
  
  // Render the focus name at calculated screen position
  screen->Print(final_x, final_y, display_name);
};

}  // namespace

/// @brief Custom replacement for oCGame::UpdatePlayerStatus that handles UI elements
/// and focus display for multiplayer functionality.
/// @note This doesn't fully mimic the original functionality.
void oCGame::UpdatePlayerStatus() {
  if (!player) {
    return;
  }
  screen->RemoveItem(hpBar);
  screen->RemoveItem(swimBar);
  screen->RemoveItem(manaBar);
  screen->RemoveItem(focusBar);
  if (!showPlayerStatus) {
    return;
  }

  zCWorld* world = GetWorld();
  // Skip if a global cutscene is running
  if (world != nullptr && world->csPlayer->GetPlayingGlobalCutscene()) {
    return;
  }

  screen->InsertItem(hpBar);
  hpBar->SetMaxRange(0, player->GetAttribute(NPC_ATR_HITPOINTSMAX));
  hpBar->SetRange(0, player->GetAttribute(NPC_ATR_HITPOINTSMAX));
  hpBar->SetPreview(player->GetAttribute(NPC_ATR_HITPOINTS));
  hpBar->SetValue(player->GetAttribute(NPC_ATR_HITPOINTS));

  if (player->GetAnictrl() != nullptr) {
    if (player->GetAnictrl()->wmode_last == ANI_WALKMODE_DIVE) {
      float swim_time_a, swim_time_b, swim_time_c;
      player->GetSwimDiveTime(swim_time_a, swim_time_b, swim_time_c);
      screen->InsertItem(swimBar);
      float max_swim_time = swim_time_b;
      if (swim_time_b == DIVE_TIME_INVALID)
        swim_time_c = max_swim_time;
      swimBar->SetMaxRange(0, max_swim_time);
      swimBar->SetRange(0, max_swim_time);
      swimBar->SetValue(swim_time_c);
    }
  }

  if (player->inventory2.IsOpen() || player->GetWeaponMode() == NPC_WEAPON_MAG) {
    if (player->GetAttribute(NPC_ATR_MANAMAX) > 0) {
      screen->InsertItem(manaBar);
      manaBar->SetMaxRange(0, player->GetAttribute(NPC_ATR_MANAMAX));
      manaBar->SetRange(0, player->GetAttribute(NPC_ATR_MANAMAX));
      manaBar->SetPreview(player->GetAttribute(NPC_ATR_MANA));
      manaBar->SetValue(player->GetAttribute(NPC_ATR_MANA));
    }
  }

  if (gStatusReadGameOptions) {
    gStatusReadGameOptions = 0;
    if (zoptions) {
      show_Focus = zoptions->ReadBool(zOPT_SEC_OPTIONS, "show_Focus", show_Focus);
      show_FocusItm = zoptions->ReadBool(zOPT_SEC_OPTIONS, "show_FocusItm", show_FocusItm);
      show_FocusMob = zoptions->ReadBool(zOPT_SEC_OPTIONS, "show_FocusMob", show_FocusMob);
      show_FocusNpc = zoptions->ReadBool(zOPT_SEC_OPTIONS, "show_FocusNpc", show_FocusNpc);
      show_FocusBar = zoptions->ReadBool(zOPT_SEC_OPTIONS, "show_FocusBar", show_FocusBar);
    }
  }

  if (show_Focus) {
    zCVob* focus_vob = player->GetFocusVob();
    if (!focus_vob) {
      return;
    }

    auto focus_info = GetFocusInfo(focus_vob);
    if (!focus_info.has_value()) {
      return;
    }

    if (focus_info->current_health > 0) {
      screen->InsertItem(focusBar);
      focusBar->SetMaxRange(0, focus_info->health_max);
      focusBar->SetRange(0, focus_info->health_max);
      focusBar->SetValue(focus_info->current_health);
    }

    zVEC3 focus_pos = CalculateFocusPosition(focus_vob);
    RenderFocusName(focus_pos, focus_info->name);
  }
}

}  // namespace Gothic_II_Addon
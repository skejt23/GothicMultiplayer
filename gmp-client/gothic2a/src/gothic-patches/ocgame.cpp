
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

#include <algorithm>
#include <optional>

#include "hooking/MemoryPatch.h"
#include "ZenGin/zGothicAPI.h"
#include "hud_control.h"
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

namespace gmp::gothic {
namespace {
std::int32_t g_hud_all_mode = kHudModeDefault;
std::int32_t g_hud_health_bar_mode = kHudModeDefault;
std::int32_t g_hud_mana_bar_mode = kHudModeDefault;
std::int32_t g_hud_swim_bar_mode = kHudModeDefault;
std::int32_t g_hud_focus_bar_mode = kHudModeDefault;
std::int32_t g_hud_focus_name_mode = kHudModeDefault;

bool IsValidHudMode(std::int32_t mode) {
  return mode == kHudModeHidden || mode == kHudModeDefault || mode == kHudModeAlwaysVisible;
}

std::int32_t* GetHudModeStorage(std::int32_t hud_type) {
  switch (hud_type) {
    case kHudAll:
      return &g_hud_all_mode;
    case kHudHealthBar:
      return &g_hud_health_bar_mode;
    case kHudManaBar:
      return &g_hud_mana_bar_mode;
    case kHudSwimBar:
      return &g_hud_swim_bar_mode;
    case kHudFocusBar:
      return &g_hud_focus_bar_mode;
    case kHudFocusName:
      return &g_hud_focus_name_mode;
    default:
      return nullptr;
  }
}

oCViewStatusBar* GetHudBar(std::int32_t hud_type) {
  if (!Gothic_II_Addon::ogame) {
    return nullptr;
  }

  switch (hud_type) {
    case kHudHealthBar:
      return Gothic_II_Addon::ogame->hpBar;
    case kHudManaBar:
      return Gothic_II_Addon::ogame->manaBar;
    case kHudSwimBar:
      return Gothic_II_Addon::ogame->swimBar;
    case kHudFocusBar:
      return Gothic_II_Addon::ogame->focusBar;
    default:
      return nullptr;
  }
}
}  // namespace

bool SetHudEnabled(std::int32_t hud_type, bool enabled) {
  return SetHudMode(hud_type, enabled ? kHudModeDefault : kHudModeHidden);
}

std::optional<bool> GetHudEnabled(std::int32_t hud_type) {
  auto mode = GetHudMode(hud_type);
  if (!mode.has_value()) {
    return std::nullopt;
  }

  return *mode != kHudModeHidden;
}

bool IsHudTypeEnabled(std::int32_t hud_type) {
  return GetHudEnabled(hud_type).value_or(false);
}

bool SetHudMode(std::int32_t hud_type, std::int32_t mode) {
  if (!IsValidHudMode(mode)) {
    return false;
  }

  auto* storage = GetHudModeStorage(hud_type);
  if (!storage) {
    return false;
  }

  *storage = mode;
  return true;
}

std::optional<std::int32_t> GetHudMode(std::int32_t hud_type) {
  auto* storage = GetHudModeStorage(hud_type);
  if (!storage) {
    return std::nullopt;
  }

  return *storage;
}

bool IsHudVisible(std::int32_t hud_type, bool default_visible) {
  // The engine's global gate belongs to menus/dialogs. Script preferences do
  // not change it, and an always-visible bar must still respect that gate.
  if (!Gothic_II_Addon::ogame || !Gothic_II_Addon::ogame->GetShowPlayerStatus()) {
    return false;
  }
  auto all_mode = GetHudMode(kHudAll);
  if (!all_mode.has_value() || *all_mode == kHudModeHidden) {
    return false;
  }

  auto mode = GetHudMode(hud_type);
  if (!mode.has_value() || *mode == kHudModeHidden) {
    return false;
  }

  if (*mode == kHudModeAlwaysVisible || *all_mode == kHudModeAlwaysVisible) {
    return true;
  }

  return default_visible;
}

bool SetHudBarPosition(std::int32_t hud_type, std::int32_t x, std::int32_t y) {
  auto* bar = GetHudBar(hud_type);
  if (!bar) {
    return false;
  }

  bar->SetPos(x, y);
  return true;
}

std::optional<HudVector> GetHudBarPosition(std::int32_t hud_type) {
  auto* bar = GetHudBar(hud_type);
  if (!bar) {
    return std::nullopt;
  }

  int x = 0;
  int y = 0;
  bar->GetPos(x, y);
  return HudVector{x, y};
}

bool SetHudBarSize(std::int32_t hud_type, std::int32_t width, std::int32_t height) {
  auto* bar = GetHudBar(hud_type);
  if (!bar || width <= 0 || height <= 0) {
    return false;
  }

  bar->SetSize(width, height);
  return true;
}

std::optional<HudVector> GetHudBarSize(std::int32_t hud_type) {
  auto* bar = GetHudBar(hud_type);
  if (!bar) {
    return std::nullopt;
  }

  int width = 0;
  int height = 0;
  bar->GetSize(width, height);
  return HudVector{width, height};
}

}  // namespace gmp::gothic

namespace Gothic_II_Addon {

namespace {

// Constants for focus positioning
constexpr float MODEL_HEIGHT_MULTIPLIER = 1.6f;
constexpr float BBOX_HEIGHT_OFFSET_MULTIPLIER = 0.82f;
constexpr float FOCUS_HEIGHT_LIMIT_OFFSET = 100.0f;
constexpr int MAX_SCREEN_WIDTH = 8192;
constexpr int MAX_SCREEN_HEIGHT = 8191;
constexpr float DIVE_TIME_INVALID = -1000000.0f;

void UpdateStatusBar(oCViewStatusBar* bar, int maximum, int current) {
  if (!bar)
    return;
  // Empty stats affect the fill, never the frame or the visibility policy.
  // Native status bars require a nonzero range even when the stat has no max.
  const int range = std::max(1, maximum);
  const int value = std::clamp(current, 0, std::max(0, maximum));
  screen->InsertItem(bar);
  bar->SetMaxRange(0, range);
  bar->SetRange(0, range);
  bar->SetPreview(value);
  bar->SetValue(value);
}

struct FocusInfo {
  zSTRING name;
  int current_health{};
  int health_max{};
  bool default_name_visible{};
  bool default_bar_visible{};
};

std::optional<FocusInfo> GetFocusInfo(zCVob* vob) {
  if (!vob) {
    return std::nullopt;
  }

  FocusInfo info{};
  if (oCMOB* mob = zDYNAMIC_CAST<oCMOB>(vob)) {
    info.name = mob->GetName();
    info.default_name_visible = show_Focus && show_FocusMob;
  } else if (oCNpc* npc = zDYNAMIC_CAST<oCNpc>(vob)) {
    if (npc->noFocus) {
      return std::nullopt;
    }
    info.name = npc->GetName();
    info.current_health = npc->GetAttribute(NPC_ATR_HITPOINTS);
    info.health_max = npc->GetAttribute(NPC_ATR_HITPOINTSMAX);
    info.default_name_visible = show_Focus && show_FocusNpc;
    info.default_bar_visible = show_Focus && show_FocusBar;
  } else if (oCItem* item = zDYNAMIC_CAST<oCItem>(vob)) {
    info.name = item->GetName(1);
    info.default_name_visible = show_Focus && show_FocusItm;
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
  if (!screen) {
    return;
  }
  if (hpBar)
    screen->RemoveItem(hpBar);
  if (swimBar)
    screen->RemoveItem(swimBar);
  if (manaBar)
    screen->RemoveItem(manaBar);
  if (focusBar)
    screen->RemoveItem(focusBar);
  if (!player || !GetShowPlayerStatus() ||
      gmp::gothic::GetHudMode(gmp::gothic::kHudAll) == gmp::gothic::kHudModeHidden) {
    return;
  }

  zCWorld* world = GetWorld();
  // Skip if a global cutscene is running
  if (world && world->csPlayer && world->csPlayer->GetPlayingGlobalCutscene()) {
    return;
  }

  if (gmp::gothic::IsHudVisible(gmp::gothic::kHudHealthBar, true)) {
    UpdateStatusBar(hpBar, player->GetAttribute(NPC_ATR_HITPOINTSMAX), player->GetAttribute(NPC_ATR_HITPOINTS));
  }

  if (player->GetAnictrl() != nullptr) {
    const bool is_diving = player->GetAnictrl()->wmode_last == ANI_WALKMODE_DIVE;
    if (gmp::gothic::IsHudVisible(gmp::gothic::kHudSwimBar, is_diving)) {
      float swim_time = 0.0f;
      float dive_time = 0.0f;
      float dive_value = 0.0f;
      player->GetSwimDiveTime(swim_time, dive_time, dive_value);

      float max_swim_time = dive_time;
      if (dive_time == DIVE_TIME_INVALID || max_swim_time <= 0.0f) {
        max_swim_time = 1.0f;
        dive_value = max_swim_time;
      }

      screen->InsertItem(swimBar);
      swimBar->SetMaxRange(0, max_swim_time);
      swimBar->SetRange(0, max_swim_time);
      swimBar->SetValue(dive_value);
    }
  }

  const int mana_max = player->GetAttribute(NPC_ATR_MANAMAX);
  const bool mana_default_visible =
      mana_max > 0 && (player->inventory2.IsOpen() || player->GetWeaponMode() == NPC_WEAPON_MAG);
  if (gmp::gothic::IsHudVisible(gmp::gothic::kHudManaBar, mana_default_visible)) {
    UpdateStatusBar(manaBar, mana_max, player->GetAttribute(NPC_ATR_MANA));
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

  zCVob* focus_vob = player->GetFocusVob();
  if (!focus_vob) {
    return;
  }

  auto focus_info = GetFocusInfo(focus_vob);
  if (!focus_info.has_value()) {
    return;
  }

  if (focus_info->current_health > 0 &&
      gmp::gothic::IsHudVisible(gmp::gothic::kHudFocusBar, focus_info->default_bar_visible)) {
    UpdateStatusBar(focusBar, focus_info->health_max, focus_info->current_health);
  }

  if (focus_vob->GetVisual() &&
      gmp::gothic::IsHudVisible(gmp::gothic::kHudFocusName, focus_info->default_name_visible)) {
    zVEC3 focus_pos = CalculateFocusPosition(focus_vob);
    RenderFocusName(focus_pos, focus_info->name);
  }
}

}  // namespace Gothic_II_Addon

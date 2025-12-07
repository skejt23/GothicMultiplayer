/*
MIT License

Copyright (c) 2025 skejt23

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

#include "dev/dev_tools.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "hooking/MemoryPatch.h"
#include "keyboard.h"

namespace debug {
namespace {
constexpr float kMinSpeed = 100.0f;
constexpr float kMaxSpeed = 7000.0f;
constexpr float kSpeedStep = 100.0f;

// ProcessRainFX hook
// Ghidra: zCSkyControler_Outdoor::ProcessRainFX(void) @ 005EAF30
// Original Gothic 2 method that handles time-based weather transitions and
// calls SetEffectWeight/UpdateParticles/CreateParticles/RenderParticles in sequence
constexpr DWORD kProcessRainFXAddress = 0x005EAF30;
using ProcessRainFXFn = void(__thiscall*)(zCSkyControler_Outdoor*);
ProcessRainFXFn g_processRainFXOriginal = nullptr;

// SetRainFXWeight hook
// Ghidra: zCSkyControler_Outdoor::SetRainFXWeight(float weight, float duration) @ 005EB230
// Parameters:
//   weight:   Target rain intensity (0.0-1.0)
//   duration: Time window for fade-in/fade-out effect
// Sets timeStartRain/timeStopRain offsets (+0x6a8, +0x6ac) based on current game time
constexpr DWORD kSetRainFXWeightAddress = 0x005EB230;
using SetRainFXWeightFn = void(__thiscall*)(zCSkyControler_Outdoor*, float, float);
SetRainFXWeightFn g_setRainFXWeightOriginal = nullptr;

// ═══════════════════════════════════════════════════════════════════════════════
// Weather Override System State
// ═══════════════════════════════════════════════════════════════════════════════
// These globals control the dev tools weather override feature, which allows
// forcing specific weather conditions (rain/snow) at fixed intensities.
//
// g_weatherOverrideActive: Master enable flag for weather override
// g_overrideWeather:       Type of weather to display (zTWEATHER_RAIN/SNOW)
// g_overrideRainWeight:    Intensity (0.0-1.0), controls particle density
// g_overrideInitialized:   Tracks whether particle system was initialized
//                          for current override session (reset on deactivation)
//
// When active, Hook_ProcessRainFX bypasses Gothic's time-based weather system
// and manually drives the particle system with fixed parameters. This requires
// special initialization (see detailed comment in Hook_ProcessRainFX) to ensure
// particles have staggered lifetimes, matching how Gothic normally manages them
// during gameplay.
// ═══════════════════════════════════════════════════════════════════════════════
bool g_weatherOverrideActive = false;
zTWeather g_overrideWeather = zTWEATHER_RAIN;
float g_overrideRainWeight = 0.0f;
bool g_overrideInitialized = false;

void __fastcall Hook_SetRainFXWeight(zCSkyControler_Outdoor* sky, void* /*edx*/, float weight, float duration) {
  if (!sky) {
    if (g_setRainFXWeightOriginal) {
      g_setRainFXWeightOriginal(sky, weight, duration);
    }
    return;
  }

  if (!g_weatherOverrideActive) {
    if (g_setRainFXWeightOriginal) {
      g_setRainFXWeightOriginal(sky, weight, duration);
    }
    return;
  }

  // Override active: pin the window and weight, ignore requested duration/weight
  // Ghidra offsets: timeStartRain @ +0x6a8, timeStopRain @ +0x6ac
  // By setting start < current time < stop with weight pinned, we force constant intensity
  sky->rainFX.timeStartRain = -1000.0f;
  sky->rainFX.timeStopRain = 1000.0f;
  sky->rainFX.outdoorRainFXWeight = g_overrideRainWeight;
  sky->m_enuWeather = g_overrideWeather;
  if (sky->rainFX.outdoorRainFX) {
    sky->rainFX.outdoorRainFX->SetWeatherType(g_overrideWeather);
    sky->rainFX.outdoorRainFX->SetEffectWeight(g_overrideRainWeight, g_overrideRainWeight);
  }
}

void __fastcall Hook_ProcessRainFX(zCSkyControler_Outdoor* sky, void* /*edx*/) {
  if (!sky) {
    if (g_processRainFXOriginal) {
      g_processRainFXOriginal(sky);
    }
    return;
  }

  // If override is NOT active, just call original and return
  if (!g_weatherOverrideActive) {
    g_overrideInitialized = false;  // Reset so next activation reinitializes
    if (g_processRainFXOriginal) {
      g_processRainFXOriginal(sky);
    }
    return;
  }

  // Override path: bypass the original time-window based fade and push a fixed weight
  zCSkyControler::s_skyEffectsEnabled = TRUE;
  sky->m_bDontRain = FALSE;
  sky->rainFX.camLocationHint = zCSkyControler::zCAM_OUTSIDE_SECTOR;

  // Ensure we have the rain FX instance; if not, let the original create it once
  if (!sky->rainFX.outdoorRainFX && g_processRainFXOriginal) {
    g_processRainFXOriginal(sky);
  }

  if (!sky->rainFX.outdoorRainFX) {
    return;  // Nothing to drive
  }

  sky->m_enuWeather = g_overrideWeather;
  sky->rainFX.outdoorRainFX->SetWeatherType(g_overrideWeather);

  // Stabilize camera-related state so CreateParticles never drops bursts due to big deltas
  // Ghidra: m_camPosLastFrame @ zCOutdoorRainFX+0xe01c (used by CheckCameraBeam @ 005e1a70)
  if (zCCamera::activeCam && zCCamera::activeCam->connectedVob) {
    sky->rainFX.outdoorRainFX->m_camPosLastFrame = zCCamera::activeCam->connectedVob->GetPositionWorld();
  }

  const float targetWeight = std::clamp(g_overrideRainWeight, 0.0f, 1.0f);

  // Use the target weight directly so the particle system renders with no oscillation
  sky->rainFX.outdoorRainFXWeight = targetWeight;
  sky->rainFX.renderLightning = FALSE;  // keep forced weather stable and silent

  sky->rainFX.soundVolume = targetWeight;

  if (targetWeight <= 0.0f) {
    sky->rainFX.m_bRaining = FALSE;
    sky->rainFX.outdoorRainFX->SetEffectWeight(0.0f, sky->rainFX.soundVolume);
    g_overrideInitialized = false;  // Reset so next activation reinitializes
    return;
  }

  if (!sky->rainFX.m_bRaining) {
    sky->rainFX.m_iRainCtr++;
  }
  sky->rainFX.m_bRaining = TRUE;

  zTRenderContext renderContext{};
  renderContext.cam = zCCamera::activeCam;
  renderContext.vob = zCCamera::activeCam ? zCCamera::activeCam->connectedVob : nullptr;
  renderContext.world = renderContext.vob ? renderContext.vob->homeWorld : nullptr;
  if (renderContext.cam) {
    renderContext.cam->Activate();
  }

  // ═══════════════════════════════════════════════════════════════════════════════
  // Weather Override Particle Initialization
  // ═══════════════════════════════════════════════════════════════════════════════
  // Gothic 2's rain/snow particle system (zCOutdoorRainFX) manages particle lifetimes
  // to create natural weather patterns. Understanding this behavior is essential to
  // reliably control weather.
  //
  // HOW GOTHIC'S PARTICLE SYSTEM WORKS:
  // ------------------------------------
  // 1. SetEffectWeight() @ 005e18e0 allocates particles and rebuilds m_freeFlyParticleList array
  //    - Calculates m_numDestFlyParticle (param1 * 1024.0) and stores @ +0xe018
  //    - Updates m_effectWeight field @ +0xe014
  //    - Populates m_freeFlyParticleList (zCArray @ +0xe008) with available particles
  // 2. CreateParticles() @ 005e1c70 pops zSParticle entries from m_freeFlyParticleList
  //    - Each zSParticle is 0x1c bytes (28 bytes)
  //    - Assigns m_destPosition and m_destNormal for each particle
  //    - Sets m_killTotalTime = 1.0 for each new particle (full lifetime)
  // 3. UpdateParticles() @ 005e2400 decrements each particle's m_killTotalTime each frame
  //    - Decrement amount: (frameTime / m_timeLen)
  // 4. When m_killTotalTime < 0, particle "dies" and returns to m_freeFlyParticleList
  // 5. Dead particles respawn via CreateParticles() with m_killTotalTime = 1.0
  //
  // CAMERA BEAM DETECTION:
  // ----------------------
  // zCOutdoorRainFX::CheckCameraBeam() @ 005e1a70 detects camera teleportation by
  // measuring distance between current camera position and m_camPosLastFrame (+0xe01c).
  // When the distance exceeds threshold (3750.0 units, hex 0x456a6000), CreateParticles()
  // randomizes m_killTotalTime for new particles instead of using 1.0. This prevents
  // synchronized particle death after teleports. CheckCameraBeam returns 1 if beam
  // detected, 0 otherwise, and resets affected particles to m_killTotalTime = 100.0.
  //
  // REQUIREMENTS FOR RELIABLE WEATHER OVERRIDE:
  // --------------------------------------------
  // 1. Ideally call SetEffectWeight() only ONCE during initialization (not every frame)
  //    - Calling repeatedly rebuilds m_freeFlyParticleList, resetting particle count
  //    - ApplyWeatherOverride currently re-calls SetEffectWeight as a hard clamp; keep
  //      that in mind if you refactor to a one-shot initialization path.
  // 2. Set m_camPosLastFrame far from current position before first CreateParticles()
  //    - Triggers CheckCameraBeam() detection
  //    - Causes CreateParticles() to randomize m_killTotalTime values
  //    - Without this, all particles start with m_killTotalTime=1.0 and die together
  // 3. On subsequent frames, directly update m_effectWeight field
  //    - Avoids rebuilding particle system while maintaining desired intensity
  //
  // This approach ensures particles have staggered lifetimes, producing smooth,
  // non-pulsing rain/snow that matches Gothic's natural behavior.
  // ═══════════════════════════════════════════════════════════════════════════════

  if (!g_overrideInitialized) {
    // Initialize particle system (allocates particles, sets up free list)
    sky->rainFX.outdoorRainFX->SetEffectWeight(targetWeight, targetWeight);

    // Trigger camera beam detection by simulating large camera movement
    // This causes CreateParticles() to randomize particle lifetimes
    if (zCCamera::activeCam && zCCamera::activeCam->connectedVob) {
      zVEC3 farAwayPos = zCCamera::activeCam->connectedVob->GetPositionWorld();
      farAwayPos[0] += 100000.0f;  // Delta >> 3750.0 threshold triggers CheckCameraBeam() @ 005e1a70
      sky->rainFX.outdoorRainFX->m_camPosLastFrame = farAwayPos;
    }

    g_overrideInitialized = true;
  } else {
    // Subsequent frames: directly update weight without rebuilding particle system
    sky->rainFX.outdoorRainFX->m_effectWeight = targetWeight;
  }
  sky->rainFX.outdoorRainFX->UpdateParticles();
  sky->rainFX.outdoorRainFX->CreateParticles(renderContext);

  if (sky->rainFX.camLocationHint != zCSkyControler::zCAM_INSIDE_SECTOR_CANT_SEE_OUTSIDE) {
    zCOLOR col = sky->GetDaylightColorFromIntensity(255);
    col.alpha = 255;
    sky->rainFX.outdoorRainFX->RenderParticles(renderContext, col);
  }

  // Keep struct weight pinned even if internal code modified it
  sky->rainFX.outdoorRainFXWeight = targetWeight;

  static int logCounter = 0;
  if ((++logCounter % 120) == 0) {
    SPDLOG_INFO("Weather override tick: w={} sound={} rainStruct={} camOutside={}", targetWeight, sky->rainFX.soundVolume,
                sky->rainFX.outdoorRainFXWeight, sky->rainFX.camLocationHint == zCSkyControler::zCAM_OUTSIDE_SECTOR);
  }
}

bool HasModifierForToggle() {
  return zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL) || zinput->KeyPressed(KEY_LALT) || zinput->KeyPressed(KEY_RALT);
}

void EnablePhysics() {
  player->SetCollDet(1);
  player->SetPhysicsEnabled(1);
  player->GetAnictrl()->SetPhysicsEnabled(1);
}

void DisablePhysics() {
  player->SetCollDet(0);
  player->SetPhysicsEnabled(0);
  player->GetAnictrl()->SetPhysicsEnabled(0);
}

void ZeroVelocity() {
  auto* anictrl = player->GetAnictrl();
  anictrl->velocity[VX] = 0.0f;
  anictrl->velocity[VY] = 0.0f;
  anictrl->velocity[VZ] = 0.0f;
  anictrl->state = zCAIPlayer::zMV_STATE_FLY;
}

}  // namespace

DevTools::DevTools()
    : noclip_enabled_(false),
      noclip_speed_(1000.0f),
      last_noclip_update_(0),
      weather_menu_open_(false),
      weather_selection_(0),
      weather_override_active_(false),
      override_weather_(zTWEATHER_SNOW),
      override_rain_weight_(0.0f),
      hooks_initialized_(false) {
}

void DevTools::InitHooks() {
  if (hooks_initialized_) {
    return;
  }

  // Hook ProcessRainFX to allow weather override
  if (auto original = CreateHook(kProcessRainFXAddress, (DWORD)Hook_ProcessRainFX)) {
    g_processRainFXOriginal = reinterpret_cast<ProcessRainFXFn>(*original);
    SPDLOG_INFO("DevTools: Hooked ProcessRainFX at 0x{:08X}", kProcessRainFXAddress);
  } else {
    SPDLOG_ERROR("DevTools: Failed to hook ProcessRainFX at 0x{:08X}", kProcessRainFXAddress);
  }

  // Hook SetRainFXWeight to block external fades when override is active
  if (auto original = CreateHook(kSetRainFXWeightAddress, (DWORD)Hook_SetRainFXWeight)) {
    g_setRainFXWeightOriginal = reinterpret_cast<SetRainFXWeightFn>(*original);
    SPDLOG_INFO("DevTools: Hooked SetRainFXWeight at 0x{:08X}", kSetRainFXWeightAddress);
  } else {
    SPDLOG_ERROR("DevTools: Failed to hook SetRainFXWeight at 0x{:08X}", kSetRainFXWeightAddress);
  }

  hooks_initialized_ = true;
}

DevTools& DevTools::Instance() {
  static DevTools instance;
  return instance;
}

void DevTools::HandleInput(bool writingOnChat) {
  HandleNoclip(writingOnChat);
  HandleWeatherMenu(writingOnChat);
}

void DevTools::Render() {
#ifndef NDEBUG
  // Show player position in debug builds
  if (player) {
    screen->SetFont("FONT_OLD_10_WHITE.TGA");
    screen->SetFontColor(zCOLOR(200, 200, 200));

    zVEC3 pos = player->GetPositionWorld();
    std::string posText = "Pos: (" + std::to_string(static_cast<int>(pos[VX])) + ", " + std::to_string(static_cast<int>(pos[VY])) + ", " +
                          std::to_string(static_cast<int>(pos[VZ])) + ")";

    constexpr int marginX = 100;
    constexpr int marginY = 100;
    const int textW = screen->FontSize(posText.c_str());
    screen->Print(8192 - marginX - textW, marginY, posText.c_str());
  }
#endif

  RenderNoclipOverlay();
  RenderWeatherMenu();

  // Apply weather override after sky rendering has processed
  // This ensures our override happens after ProcessRainFX
  ApplyWeatherOverride();
}

void DevTools::HandleNoclip(bool writingOnChat) {
  if (zinput->KeyToggled(KEY_F8) && !writingOnChat) {
    if (!HasModifierForToggle()) {
      noclip_enabled_ = !noclip_enabled_;
      last_noclip_update_ = clock();

      if (!noclip_enabled_) {
        EnablePhysics();
      }
    }
  }

  if (!noclip_enabled_ || writingOnChat) {
    return;
  }

  DisablePhysics();
  ZeroVelocity();

  const clock_t now = clock();
  const float deltaTime = (now - last_noclip_update_) / 1000.0f;
  if (deltaTime <= 0.0f) {
    return;
  }

  last_noclip_update_ = now;

  // Speed adjustment
  if (zinput->KeyPressed(KEY_ADD)) {
    noclip_speed_ = std::min(noclip_speed_ + kSpeedStep, kMaxSpeed);
  }
  if (zinput->KeyPressed(KEY_SUBTRACT)) {
    noclip_speed_ = std::max(noclip_speed_ - kSpeedStep, kMinSpeed);
  }

  // Apply movement
  zVEC3 movement(0.0f, 0.0f, 0.0f);
  const float moveDistance = noclip_speed_ * deltaTime;

  zVEC3 forward = player->GetAtVectorWorld();
  zVEC3 right = player->trafoObjToWorld.GetRightVector();
  forward.Normalize();
  right.Normalize();

  if (zinput->KeyPressed(KEY_W) || zinput->KeyPressed(KEY_UP)) {
    movement[VX] += forward[VX] * moveDistance;
    movement[VZ] += forward[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_S) || zinput->KeyPressed(KEY_DOWN)) {
    movement[VX] -= forward[VX] * moveDistance;
    movement[VZ] -= forward[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_A) || zinput->KeyPressed(KEY_LEFT)) {
    movement[VX] -= right[VX] * moveDistance;
    movement[VZ] -= right[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_D) || zinput->KeyPressed(KEY_RIGHT)) {
    movement[VX] += right[VX] * moveDistance;
    movement[VZ] += right[VZ] * moveDistance;
  }
  if (zinput->KeyPressed(KEY_SPACE)) {
    movement[VY] += moveDistance;
  }
  if (zinput->KeyPressed(KEY_LCONTROL) || zinput->KeyPressed(KEY_RCONTROL)) {
    movement[VY] -= moveDistance;
  }

  zVEC3 newPos = player->GetPositionWorld();
  newPos[VX] += movement[VX];
  newPos[VY] += movement[VY];
  newPos[VZ] += movement[VZ];
  player->SetPositionWorld(newPos);
}

void DevTools::RenderNoclipOverlay() {
  if (!noclip_enabled_) {
    return;
  }

  screen->SetFont("FONT_OLD_10_WHITE.TGA");
  screen->SetFontColor(zCOLOR(0, 255, 0));

  constexpr int marginX = 100;
  constexpr int marginY = 100;

  std::vector<std::string> lines;
  lines.push_back("=== NOCLIP MODE ACTIVE ===");
  lines.push_back("Speed: " + std::to_string(static_cast<int>(noclip_speed_)) + " units/s");
  lines.push_back("WASD/Arrows: Move horizontally");
  lines.push_back("Space: Move up");
  lines.push_back("Ctrl: Move down");
  lines.push_back("+/-: Adjust speed");
  lines.push_back("F8: Toggle noclip off");

  const int fontH = screen->FontY();
  const int spacing = fontH;
  const int totalHeight = spacing * static_cast<int>(lines.size());
  const int yStart = 8192 - marginY - totalHeight;

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    const int textW = screen->FontSize(line.c_str());
    const int x = 8192 - marginX - textW;
    const int y = yStart + static_cast<int>(i) * spacing;

    if (i == 0 || i == 1) {
      screen->SetFontColor(zCOLOR(0, 255, 0));
      screen->Print(x, y, line.c_str());
      screen->SetFontColor(zCOLOR(200, 200, 200));
    } else {
      screen->Print(x, y, line.c_str());
    }
  }
}

void DevTools::HandleWeatherMenu(bool writingOnChat) {
  // F9: Toggle weather menu
  if (zinput->KeyToggled(KEY_F9) && !writingOnChat) {
    weather_menu_open_ = !weather_menu_open_;
  }

  if (!weather_menu_open_ || writingOnChat) {
    return;
  }

  // Navigate with arrow keys
  if (zinput->KeyToggled(KEY_UP)) {
    weather_selection_ = (weather_selection_ - 1 + 3) % 3;
  }
  if (zinput->KeyToggled(KEY_DOWN)) {
    weather_selection_ = (weather_selection_ + 1) % 3;
  }

  // Apply weather with Space
  if (zinput->KeyToggled(KEY_SPACE)) {
    switch (weather_selection_) {
      case 0:  // Clear (type is ignored when weight is 0)
        weather_override_active_ = true;
        override_weather_ = zTWEATHER_RAIN;  // Doesn't matter for clear
        override_rain_weight_ = 0.0f;
        SPDLOG_INFO("Weather override: Clear");
        break;
      case 1:  // Rain
        weather_override_active_ = true;
        override_weather_ = zTWEATHER_RAIN;
        override_rain_weight_ = 1.0f;
        SPDLOG_INFO("Weather override: Rain");
        break;
      case 2:  // Snow
        weather_override_active_ = true;
        override_weather_ = zTWEATHER_SNOW;
        override_rain_weight_ = 1.0f;
        SPDLOG_INFO("Weather override: Snow");
        break;
    }
  }

  // Close with Escape
  if (zinput->KeyToggled(KEY_ESCAPE)) {
    weather_menu_open_ = false;
  }
}

void DevTools::ApplyWeatherOverride() {
  // Sync class state to global variables used by the ProcessRainFX hook
  g_weatherOverrideActive = weather_override_active_;
  g_overrideWeather = override_weather_;
  g_overrideRainWeight = override_rain_weight_;

  if (!weather_override_active_) {
    return;
  }

  // Also ensure sky effects are enabled
  zCSkyControler* skyBase = zCSkyControler::s_activeSkyControler;
  if (!skyBase) {
    return;
  }

  zCSkyControler_Outdoor* sky = zDYNAMIC_CAST<zCSkyControler_Outdoor>(skyBase);
  if (!sky) {
    return;
  }

  // Ensure sky effects are enabled and rain is allowed
  zCSkyControler::s_skyEffectsEnabled = TRUE;
  sky->m_bDontRain = FALSE;

  // Hard-clamp the current state so other engine code cannot fade it out between hooks
  sky->m_enuWeather = override_weather_;
  sky->rainFX.timeStartRain = -1000.0f;
  sky->rainFX.timeStopRain = 1000.0f;
  sky->rainFX.outdoorRainFXWeight = override_rain_weight_;

  if (sky->rainFX.outdoorRainFX) {
    sky->rainFX.outdoorRainFX->SetWeatherType(override_weather_);
    sky->rainFX.outdoorRainFX->SetEffectWeight(override_rain_weight_, override_rain_weight_);
  }
}

void DevTools::RenderWeatherMenu() {
  if (!weather_menu_open_) {
    return;
  }

  screen->SetFont("FONT_OLD_10_WHITE.TGA");

  constexpr int menuX = 4096;  // Center horizontally
  constexpr int menuY = 2048;  // Center vertically
  constexpr int lineSpacing = 200;

  // Title
  screen->SetFontColor(zCOLOR(255, 255, 0));
  const char* title = "=== WEATHER MENU ===";
  const int titleWidth = screen->FontSize(title);
  screen->Print(menuX - titleWidth / 2, menuY - lineSpacing * 2, title);

  // Weather options
  const char* options[] = {"Clear", "Rain", "Snow"};

  for (int i = 0; i < 3; ++i) {
    const int yPos = menuY + i * lineSpacing;

    if (i == weather_selection_) {
      screen->SetFontColor(zCOLOR(0, 255, 0));
      std::string selected = "> " + std::string(options[i]) + " <";
      const int width = screen->FontSize(selected.c_str());
      screen->Print(menuX - width / 2, yPos, selected.c_str());
    } else {
      screen->SetFontColor(zCOLOR(200, 200, 200));
      const int width = screen->FontSize(options[i]);
      screen->Print(menuX - width / 2, yPos, options[i]);
    }
  }

  // Controls help
  screen->SetFontColor(zCOLOR(150, 150, 150));
  const char* help = "Up/Down: Navigate | Space: Apply | F9/Esc: Close";
  const int helpWidth = screen->FontSize(help);
  screen->Print(menuX - helpWidth / 2, menuY + lineSpacing * 4, help);

  // Show active override status
  if (weather_override_active_) {
    screen->SetFontColor(zCOLOR(0, 255, 255));
    const char* activeWeather = override_rain_weight_ <= 0.0f ? "Clear" : (override_weather_ == zTWEATHER_RAIN ? "Rain" : "Snow");
    std::string status = "[Active: " + std::string(activeWeather) + "]";
    const int statusWidth = screen->FontSize(status.c_str());
    screen->Print(menuX - statusWidth / 2, menuY + lineSpacing * 5, status.c_str());
  }
}

}  // namespace debug

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

#include "sky_utils.h"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

#include "hooking/MemoryPatch.h"
#include "ZenGin/zGothicAPI.h"
#include "shared/event.h"
#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_constants.h"
#include "scripting/gothic_events.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {

zVEC3 MakeColorVec3(int r, int g, int b) {
  return zVEC3(static_cast<float>(::lua::bind_helpers::ClampByte(r)),
               static_cast<float>(::lua::bind_helpers::ClampByte(g)),
               static_cast<float>(::lua::bind_helpers::ClampByte(b)));
}

zVEC4 MakeColorVec4(int r, int g, int b, int a) {
  return zVEC4(static_cast<float>(::lua::bind_helpers::ClampByte(r)),
               static_cast<float>(::lua::bind_helpers::ClampByte(g)),
               static_cast<float>(::lua::bind_helpers::ClampByte(b)),
               static_cast<float>(::lua::bind_helpers::ClampByte(a)));
}

zCSkyControler_Outdoor* GetOutdoorSky() {
  zCSkyControler* sky_base = zCSkyControler::s_activeSkyControler;
  if (!sky_base && ogame && ogame->GetGameWorld()) {
    sky_base = ogame->GetGameWorld()->GetActiveSkyControler();
  }
  return zDYNAMIC_CAST<zCSkyControler_Outdoor>(sky_base);
}

zCMaterial* GetPlanetMaterial(zCSkyPlanet& planet) {
  if (!planet.mesh) {
    return nullptr;
  }
  if (planet.mesh->numPoly > 0) {
    if (planet.mesh->polyArray) {
      if (auto* material = planet.mesh->polyArray[0].material) {
        return material;
      }
    }
    if (planet.mesh->polyList && planet.mesh->polyList[0]) {
      return planet.mesh->polyList[0]->material;
    }
  }
  return nullptr;
}

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

// ???????????????????????????????????????????????????????????????????????????????
// Weather Override System State
// ???????????????????????????????????????????????????????????????????????????????
// These globals control the sky utils weather override feature, which allows
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
// ???????????????????????????????????????????????????????????????????????????????
bool g_weatherOverrideActive = false;
zTWeather g_overrideWeather = zTWEATHER_RAIN;
float g_overrideRainWeight = 0.0f;
bool g_overrideInitialized = false;
bool g_overrideLightning = false;
bool g_hooksInitialized = false;
std::optional<RainStartTime> g_rainStartTime;
std::optional<RainStartTime> g_rainStopTime;

float ToSkyTime(int hour, int min) {
  const auto day_minutes = static_cast<float>(hour * 60 + min);
  auto sky_time = day_minutes / (24.0f * 60.0f) + 0.5f;
  while (sky_time > 1.0f) {
    sky_time -= 1.0f;
  }
  return sky_time;
}

std::optional<RainStartTime> SkyTimeToClockTime(float sky_time) {
  if (!std::isfinite(sky_time)) {
    return std::nullopt;
  }

  sky_time = std::clamp(sky_time, 0.0f, 1.0f);
  auto world_fraction = sky_time - 0.5f;
  if (world_fraction < 0.0f) {
    world_fraction += 1.0f;
  }

  auto total_minutes = static_cast<int>(std::round(world_fraction * 24.0f * 60.0f)) % (24 * 60);
  return RainStartTime{total_minutes / 60, total_minutes % 60};
}

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
  sky->rainFX.renderLightning = g_overrideLightning ? TRUE : FALSE;

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

  // ???????????????????????????????????????????????????????????????????????????????
  // Weather Override Particle Initialization
  // ???????????????????????????????????????????????????????????????????????????????
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
  // ???????????????????????????????????????????????????????????????????????????????
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

void InitWeatherHooks() {
  if (g_hooksInitialized) {
    return;
  }

  // Hook ProcessRainFX to allow weather override
  if (auto original = CreateHook(kProcessRainFXAddress, (DWORD)Hook_ProcessRainFX)) {
    g_processRainFXOriginal = reinterpret_cast<ProcessRainFXFn>(*original);
    SPDLOG_INFO("SkyUtils: Hooked ProcessRainFX at 0x{:08X}", kProcessRainFXAddress);
  } else {
    SPDLOG_ERROR("SkyUtils: Failed to hook ProcessRainFX at 0x{:08X}", kProcessRainFXAddress);
  }

  // Hook SetRainFXWeight to block external fades when override is active
  if (auto original = CreateHook(kSetRainFXWeightAddress, (DWORD)Hook_SetRainFXWeight)) {
    g_setRainFXWeightOriginal = reinterpret_cast<SetRainFXWeightFn>(*original);
    SPDLOG_INFO("SkyUtils: Hooked SetRainFXWeight at 0x{:08X}", kSetRainFXWeightAddress);
  } else {
    SPDLOG_ERROR("SkyUtils: Failed to hook SetRainFXWeight at 0x{:08X}", kSetRainFXWeightAddress);
  }

  g_hooksInitialized = true;
}

void ApplyWeatherOverride() {
  if (!g_weatherOverrideActive) {
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
  sky->m_enuWeather = g_overrideWeather;
  sky->rainFX.renderLightning = g_overrideLightning ? TRUE : FALSE;
  sky->rainFX.timeStartRain = -1000.0f;
  sky->rainFX.timeStopRain = 1000.0f;
  sky->rainFX.outdoorRainFXWeight = g_overrideRainWeight;

  if (sky->rainFX.outdoorRainFX) {
    sky->rainFX.outdoorRainFX->SetWeatherType(g_overrideWeather);
    sky->rainFX.outdoorRainFX->SetEffectWeight(g_overrideRainWeight, g_overrideRainWeight);
  }
}

}  // namespace

bool ApplyWeatherType(int weather_type) {
  const int old_weather_type = GetWeatherType();
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  InitWeatherHooks();

  g_weatherOverrideActive = true;
  g_overrideInitialized = false;
  g_overrideLightning = false;
  if (weather_type <= 0) {
    g_overrideWeather = zTWEATHER_RAIN;
    g_overrideRainWeight = 0.0f;
  } else if (weather_type == WEATHER_SNOW) {
    g_overrideWeather = zTWEATHER_SNOW;
    g_overrideRainWeight = 1.0f;
  } else {
    g_overrideWeather = zTWEATHER_RAIN;
    g_overrideRainWeight = 1.0f;
  }

  ApplyWeatherOverride();
  const int new_weather_type = GetWeatherType();
  if (new_weather_type != old_weather_type) {
    EventManager::Instance().TriggerEvent(kEventOnWeatherChangeName,
                                          OnWeatherChangeEvent{old_weather_type, new_weather_type});
  }
  return true;
}

int GetWeatherType() {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return 0;
  }

  if (sky->rainFX.outdoorRainFXWeight <= 0.0f) {
    return 0;
  }

  if (sky->m_enuWeather == zTWEATHER_SNOW) {
    return WEATHER_SNOW;
  }

  if (sky->m_enuWeather == zTWEATHER_RAIN) {
    return WEATHER_RAIN;
  }

  return 0;
}

bool SetRainWeight(float weight, int weather_type, bool render_lightning) {
  auto* sky = GetOutdoorSky();
  if (!sky || !std::isfinite(weight)) {
    return false;
  }

  InitWeatherHooks();

  g_weatherOverrideActive = true;
  g_overrideInitialized = false;
  g_overrideRainWeight = std::clamp(weight, 0.0f, 1.0f);
  g_overrideLightning = render_lightning && g_overrideRainWeight > 0.0f;
  g_overrideWeather = weather_type == WEATHER_SNOW ? zTWEATHER_SNOW : zTWEATHER_RAIN;

  ApplyWeatherOverride();
  if (g_overrideRainWeight <= 0.0f && sky->rainFX.outdoorRainFX) {
    sky->rainFX.outdoorRainFX->SetEffectWeight(0.0f, 0.0f);
    sky->rainFX.outdoorRainFXWeight = 0.0f;
  }

  return true;
}

bool SetRainStartTime(int hour, int min) {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  if (hour < 0 || hour > 23 || min < 0 || min > 59) {
    return false;
  }

  g_rainStartTime = RainStartTime{hour, min};
  sky->rainFX.timeStartRain = ToSkyTime(hour, min);
  return true;
}

std::optional<RainStartTime> GetRainStartTime() {
  if (g_rainStartTime.has_value()) {
    return g_rainStartTime;
  }

  auto* sky = GetOutdoorSky();
  if (!sky) {
    return std::nullopt;
  }

  return SkyTimeToClockTime(sky->rainFX.timeStartRain);
}

bool SetRainStopTime(int hour, int min) {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  if (hour < 0 || hour > 23 || min < 0 || min > 59) {
    return false;
  }

  const auto stop_sky_time = ToSkyTime(hour, min);
  if (g_rainStartTime.has_value() && stop_sky_time <= ToSkyTime(g_rainStartTime->hour, g_rainStartTime->min)) {
    return false;
  }

  g_rainStopTime = RainStartTime{hour, min};
  sky->rainFX.timeStopRain = stop_sky_time;
  return true;
}

std::optional<RainStartTime> GetRainStopTime() {
  if (g_rainStopTime.has_value()) {
    return g_rainStopTime;
  }

  auto* sky = GetOutdoorSky();
  if (!sky) {
    return std::nullopt;
  }

  return SkyTimeToClockTime(sky->rainFX.timeStopRain);
}

bool SetWindScale(float wind_scale) {
  auto* sky = GetOutdoorSky();
  if (!sky || !std::isfinite(wind_scale)) {
    return false;
  }

  zVEC3 wind_vec = sky->m_bWindVec;
  float length = wind_vec.Length();
  if (length <= 0.0001f) {
    wind_vec = zVEC3(1.0f, 0.0f, 0.0f);
  } else {
    wind_vec.Normalize();
  }

  sky->m_bWindVec = zVEC3(wind_vec[VX] * wind_scale, wind_vec[VY] * wind_scale, wind_vec[VZ] * wind_scale);
  return true;
}

float GetWindScale() {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return 0.0f;
  }

  return sky->m_bWindVec.Length();
}

bool SetDontRain(bool toggle) {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  if (toggle) {
    g_weatherOverrideActive = false;
    g_overrideInitialized = false;
    g_overrideRainWeight = 0.0f;
  }

  sky->m_bDontRain = toggle ? TRUE : FALSE;
  if (toggle && sky->rainFX.outdoorRainFX) {
    sky->rainFX.outdoorRainFX->SetEffectWeight(0.0f, 0.0f);
    sky->rainFX.outdoorRainFXWeight = 0.0f;
  }
  return true;
}

bool GetDontRain() {
  auto* sky = GetOutdoorSky();
  return sky && sky->m_bDontRain != FALSE;
}

bool SetFogColor(int id, int r, int g, int b) {
  auto* sky = GetOutdoorSky();
  if (!sky || id < 0) {
    return false;
  }

  const auto color = MakeColorVec3(r, g, b);
  if (id >= sky->fogColorDayVariations.numInArray) {
    return false;
  }

  sky->fogColorDayVariations.parray[id] = color;
  if (id < sky->fogColorDayVariations2.numInArray) {
    sky->fogColorDayVariations2.parray[id] = color;
  }

  sky->SetLightDirty();
  return true;
}

bool SetCloudsColor(int r, int g, int b) {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  const auto color = MakeColorVec3(r, g, b);
  sky->masterState.domeColor0 = color;
  sky->masterState.domeColor1 = color;
  sky->SetLightDirty();
  return true;
}

bool SetPlanetSize(int planet_id, float size) {
  auto* sky = GetOutdoorSky();
  if (!sky || planet_id < 0 || planet_id >= NUM_PLANETS) {
    return false;
  }

  sky->planets[planet_id].size = size;
  return true;
}

bool SetPlanetColor(int planet_id, int r, int g, int b, int a) {
  auto* sky = GetOutdoorSky();
  if (!sky || planet_id < 0 || planet_id >= NUM_PLANETS) {
    return false;
  }

  const auto color = MakeColorVec4(r, g, b, a);
  sky->planets[planet_id].color0 = color;
  sky->planets[planet_id].color1 = color;
  return true;
}

bool SetPlanetTexture(int planet_id, const std::string& texture) {
  auto* sky = GetOutdoorSky();
  if (!sky || texture.empty() || planet_id < 0 || planet_id >= NUM_PLANETS) {
    return false;
  }

  auto* material = GetPlanetMaterial(sky->planets[planet_id]);
  if (!material) {
    return false;
  }

  zSTRING tex_name = texture.c_str();
  material->SetTexture(tex_name);
  return true;
}

bool SetLightingColor(int r, int g, int b) {
  auto* sky = GetOutdoorSky();
  if (!sky) {
    return false;
  }

  sky->SetOverrideColor(MakeColorVec3(r, g, b));
  sky->SetOverrideColorFlag(TRUE);
  sky->SetLightDirty();
  return true;
}

}  // namespace gmp::gothic

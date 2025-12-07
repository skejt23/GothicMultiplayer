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

#include "HooksManager.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>

#include "ZenGin/zGothicAPI.h"
#include "config.h"
#include "renderer/d3d9/D3D9Renderer.h"

namespace {

constexpr DWORD kRenderHookAddress = 0x006C86A0;
constexpr DWORD kAiMovingHookAddress = 0x0050E750;
constexpr DWORD kVidIsResolutionValidAddress = 0x0042C140;  // VidIsResolutionValid
constexpr DWORD kSetFOV2Address = 0x0054A960;               // zCCamera::SetFOV(float, float)

using RenderOriginalFn = void(__thiscall*)(oCGame*);
using AiMovingOriginalFn = void(__thiscall*)(zCAIPlayer*, zCVob*);

RenderOriginalFn g_renderOriginal = nullptr;
AiMovingOriginalFn g_aiMovingOriginal = nullptr;

// Original VidIsResolutionValid function pointer
using VidIsResolutionValidFn = int(__cdecl*)(int x, int y, int bpp);
VidIsResolutionValidFn g_vidIsResolutionValidOriginal = nullptr;

// Original zCCamera::SetFOV(float, float) function pointer
using SetFOV2Fn = void(__thiscall*)(zCCamera*, float, float);
SetFOV2Fn g_setFOV2Original = nullptr;

// Hook for VidIsResolutionValid - bypasses hardcoded 1600x1200 limit
// The original function rejects resolutions above 1600x1200 and enforces aspect ratio limits.
// We allow any reasonable resolution that the D3D9 adapter can handle.
static int s_resolutionCheckCount = 0;
int __cdecl Hook_VidIsResolutionValid(int x, int y, int bpp) {
  s_resolutionCheckCount++;

  // Minimum resolution (Gothic requires at least 640x480)
  if (x < 640 || y < 480) {
    SPDLOG_INFO("VidIsResolutionValid[{}] REJECTED {}x{} @ {}bpp (too small)", s_resolutionCheckCount, x, y, bpp);
    return 0;
  }
  // Only 32-bit color is supported with D3D9 renderer
  if (bpp != 32) {
    SPDLOG_INFO("VidIsResolutionValid[{}] REJECTED {}x{} @ {}bpp (not 32bpp)", s_resolutionCheckCount, x, y, bpp);
    return 0;
  }
  SPDLOG_INFO("VidIsResolutionValid[{}] ACCEPTED {}x{} @ {}bpp", s_resolutionCheckCount, x, y, bpp);
  // Accept the resolution - D3D9 mode enumeration already filters valid modes
  return 1;
}

// =============================================================================
// FOV 180° BUG FIX - Hook for zCCamera::SetFOV(float, float)
// =============================================================================
//
// NOTE: This issue only occurs when users have the vdfs32g.dll wrapper
// installed for modern Windows compatibility. This is NOT the original
// vdfs32g.dll that ships with Gothic II, but a replacement wrapper (e.g.,
// from GothicFix/SystemPack) that adds widescreen and compatibility fixes.
// Users running vanilla Gothic II without such wrappers are unaffected.
//
// SYMPTOM:
//   When using the D3D9 renderer, the camera receives fovH=180° instead of
//   the expected 90°, causing extreme fisheye distortion.
//
// ROOT CAUSE:
//   The replacement vdfs32g.dll patches the CALL at 0x0054A214 in zCCamera's
//   constructor to redirect to a widescreen FOV correction thunk. This
//   patching happens AFTER our DLL loads (observed at vdfs32g.dll+1F3B9).
//
//   The thunk performs widescreen FOV correction using FPU math, reading
//   aspect ratio from memory populated by hooks into the D3D7 renderer.
//   With our D3D9 renderer, those D3D7 hooks are never triggered, so the
//   aspect ratio memory stays 0.0, causing:
//     tan(half_fov) / 0.0 = infinity → atan(inf) = π/2 → fovH = 180°
//
// SOLUTION:
//   Hook SetFOV(float, float) at the function level to intercept the broken
//   180° value and correct it to 90°. This works regardless of what vdfs32g
//   does to the call site.
// =============================================================================
static int s_setFOV2Count = 0;
void __fastcall Hook_SetFOV2(zCCamera* camera, void* /*edx*/, float fovH_deg, float fovV_deg) {
  s_setFOV2Count++;

  // WORKAROUND: Detect and fix the 180° FOV bug.
  // The broken case has fovH ≈ 180° and fovV ≈ 67.5° (the correct vertical FOV).
  // This happens when the widescreen thunk's aspect ratio memory is uninitialized (0.0),
  // causing: tan(half_fov) / 0.0 = infinity → atan(inf) = π/2 → fovH = 180°.
  float correctedFovH = fovH_deg;
  if (fovH_deg >= 179.9f && fovH_deg <= 180.1f && fovV_deg >= 67.0f && fovV_deg <= 68.0f) {
    correctedFovH = 90.0f;  // Replace with the intended default FOV
    static bool s_loggedFix = false;
    if (!s_loggedFix) {
      SPDLOG_WARN(
          "SetFOV2: Detected 180° FOV bug, correcting to 90°. "
          "This indicates uninitialized widescreen thunk memory.");
      s_loggedFix = true;
    }
  }

  // Call original function with corrected value
  if (g_setFOV2Original) {
    g_setFOV2Original(camera, correctedFovH, fovV_deg);
  }
}

}  // namespace

HooksManager::HooksManager(void) {
  this->InitAllPatches();
  InitializeCriticalSection(&this->DoneCs);
  InitializeCriticalSection(&this->RenderCs);
  InitializeCriticalSection(&this->CloseLoadScreenCs);
  InitializeCriticalSection(&this->AiMovingCs);
}

HooksManager::~HooksManager(void) {
}

void HooksManager::ClearAllHooks() {
  EnterCriticalSection(&this->RenderCs);
  this->OnRenderHooks.clear();
  this->RenderCallbacksToDelete.clear();
  LeaveCriticalSection(&this->RenderCs);

  EnterCriticalSection(&this->CloseLoadScreenCs);
  this->OnCloseLoadScreenHooks.clear();
  LeaveCriticalSection(&this->CloseLoadScreenCs);

  EnterCriticalSection(&this->DoneCs);
  this->OnDoneHooks.clear();
  LeaveCriticalSection(&this->DoneCs);

  EnterCriticalSection(&this->AiMovingCs);
  this->OnAiMovingHooks.clear();
  this->AiMovingCallbacksToDelete.clear();
  LeaveCriticalSection(&this->AiMovingCs);

  SPDLOG_DEBUG("HooksManager: All hooks cleared for shutdown");
}

void HooksManager::AddHook(HOOK_TYPE type, DWORD callback) {
  switch (type) {
    case HT_RENDER:
      EnterCriticalSection(&this->RenderCs);
      this->OnRenderHooks.insert(callback);
      LeaveCriticalSection(&this->RenderCs);
      break;
    case HT_CLOSELOADSCREEN:
      EnterCriticalSection(&this->CloseLoadScreenCs);
      this->OnCloseLoadScreenHooks.insert(callback);
      LeaveCriticalSection(&this->CloseLoadScreenCs);
      break;
    case HT_DONE:
      EnterCriticalSection(&this->DoneCs);
      this->OnDoneHooks.insert(callback);
      LeaveCriticalSection(&this->DoneCs);
      break;
    case HT_AIMOVING:
      EnterCriticalSection(&this->AiMovingCs);
      this->OnAiMovingHooks.insert(callback);
      LeaveCriticalSection(&this->AiMovingCs);
      break;
  }
}

void HooksManager::RemoveHook(HOOK_TYPE type, DWORD callback) {
  switch (type) {
    case HT_RENDER:
      EnterCriticalSection(&this->RenderCs);
      this->RenderCallbacksToDelete.push_back(callback);
      LeaveCriticalSection(&this->RenderCs);
      break;
    case HT_CLOSELOADSCREEN:
      EnterCriticalSection(&this->CloseLoadScreenCs);
      this->OnCloseLoadScreenHooks.erase(this->OnRenderHooks.find(callback));
      LeaveCriticalSection(&this->CloseLoadScreenCs);
      break;
    case HT_DONE:
      EnterCriticalSection(&this->DoneCs);
      this->OnDoneHooks.erase(this->OnRenderHooks.find(callback));
      LeaveCriticalSection(&this->DoneCs);
      break;
    case HT_AIMOVING:
      EnterCriticalSection(&this->AiMovingCs);
      this->AiMovingCallbacksToDelete.push_back(callback);
      LeaveCriticalSection(&this->AiMovingCs);
      break;
  }
}

// Check if D3D9 renderer is enabled via config (default: true)
static bool UseDx9Renderer() {
  return Config::Instance().GetRendererType() == Config::RendererType::D3D9;
}

// Address of the CALL instruction in zCCamera constructor that calls SetFOV(float)
// Original: E8 07 07 00 00 -> CALL 0x0054A920 (SetFOV(float))
// Patched by widescreen mods: varies -> CALL to dynamically generated thunk
constexpr uintptr_t kCameraCtorSetFOVCallAddress = 0x0054A214;
constexpr uintptr_t kOriginalSetFOVAddress = 0x0054A920;

void HooksManager::InitAllPatches() {
  // Check if the SetFOV CALL in the camera constructor has been patched by something.
  // This is for diagnostic purposes - we log if it's different from the original.
  {
    BYTE* pCall = reinterpret_cast<BYTE*>(kCameraCtorSetFOVCallAddress);
    if (pCall[0] == 0xE8) {
      int32_t currentOffset = *reinterpret_cast<int32_t*>(pCall + 1);
      uintptr_t currentTarget = kCameraCtorSetFOVCallAddress + 5 + currentOffset;

      if (currentTarget != kOriginalSetFOVAddress) {
        SPDLOG_WARN("Camera constructor SetFOV CALL is patched! Target=0x{:08X} (expected 0x{:08X})", currentTarget, kOriginalSetFOVAddress);
      } else {
        SPDLOG_DEBUG("Camera constructor SetFOV CALL is original (0x{:08X})", currentTarget);
      }
    }
  }

  if (UseDx9Renderer()) {
    // Inject DX9 Renderer
    // First, patch the allocation size to match our renderer class size
    DWORD newSize = sizeof(zCRnd_D3D_DX9);
    MemoryPatch::WriteMemory(0x00630803, reinterpret_cast<PBYTE>(&newSize), sizeof(DWORD));
    SPDLOG_DEBUG("Patched renderer allocation size from 0x82E7C to 0x{:X} ({} bytes)", newSize, newSize);

    // Replaces the call to zCRnd_D3D constructor in zDieter_StartUp
    MemoryPatch::CallPatch(0x00630824, (DWORD)ConstructDX9Renderer, 0);

    // Hook VidIsResolutionValid to bypass 1600x1200 resolution limit
    // This allows the D3D9 renderer to use any resolution the adapter supports
    if (auto original = CreateHook(kVidIsResolutionValidAddress, (DWORD)Hook_VidIsResolutionValid)) {
      g_vidIsResolutionValidOriginal = reinterpret_cast<VidIsResolutionValidFn>(*original);
      SPDLOG_DEBUG("Hooked VidIsResolutionValid to allow higher resolutions");
    } else {
      SPDLOG_ERROR("Failed to hook VidIsResolutionValid at 0x{:08X}", kVidIsResolutionValidAddress);
    }

    // Patch inlined VidIsResolutionValid calls to bypass 1600x1200 limit
    {
      BYTE jmpShort = 0xEB;  // JMP SHORT opcode (unconditional)

      // Patch in Update_ChoiceBox (builds resolution list for menu)
      MemoryPatch::WriteMemory(0x0042E14F, &jmpShort, 1);
      SPDLOG_DEBUG("Patched inlined VidIsResolutionValid in Update_ChoiceBox at 0x0042E14F");

      // Patch in Apply_Options_Video (loop that finds selected resolution)
      // Original: test eax, eax / jnz short loc_42C5FB (75 36) at 0x42C5C3
      MemoryPatch::WriteMemory(0x0042C5C3, &jmpShort, 1);
      SPDLOG_DEBUG("Patched inlined VidIsResolutionValid in Apply_Options_Video at 0x0042C5C3");

      // Patch in Apply_Options_Video (validation check after loop)
      // Original: test eax, eax / jnz loc_42CA4B (0F 85 CA 03 00 00) at 0x42C67B
      // This is a NEAR jump (6 bytes), change 0F 85 to 90 E9 (NOP + JMP near)
      BYTE jmpNear[2] = {0x90, 0xE9};  // NOP + JMP near (offset stays the same)
      MemoryPatch::WriteMemory(0x0042C67B, jmpNear, 2);
      SPDLOG_DEBUG("Patched inlined VidIsResolutionValid in Apply_Options_Video at 0x0042C67B");
    }
  }

  // =============================================================================
  // Hook zCCamera::SetFOV(float, float) to work around the 180° FOV bug.
  //
  // ROOT CAUSE: vdfs32g.dll patches the CALL at 0x0054A214 in zCCamera's
  // constructor to redirect to a widescreen FOV correction thunk. This thunk
  // reads aspect ratio from memory that gets populated by D3D7 renderer hooks.
  // With our D3D9 renderer, those hooks are bypassed, so the memory stays 0.0,
  // causing division by zero → atan(inf) = π/2 → fovH = 180°.
  //
  // Since vdfs32g patches AFTER our DLL loads, we can't restore the original
  // CALL. Instead, we hook the SetFOV function itself to catch and fix the
  // broken 180° value.
  // =============================================================================
  if (auto original = CreateHook(kSetFOV2Address, (DWORD)Hook_SetFOV2)) {
    g_setFOV2Original = reinterpret_cast<SetFOV2Fn>(*original);
    SPDLOG_DEBUG("Hooked zCCamera::SetFOV at 0x{:08X} to fix vdfs32g widescreen bug", kSetFOV2Address);
  } else {
    SPDLOG_ERROR("Failed to hook zCCamera::SetFOV at 0x{:08X}", kSetFOV2Address);
  }

  if (auto original = CreateHook(kRenderHookAddress, (DWORD)&HooksManager::OnRender)) {
    g_renderOriginal = reinterpret_cast<RenderOriginalFn>(*original);
  } else {
    SPDLOG_ERROR("HooksManager: failed to hook render at 0x{0:08X}", kRenderHookAddress);
  }

  if (auto original = CreateHook(kAiMovingHookAddress, (DWORD)&HooksManager::OnAiMoving)) {
    g_aiMovingOriginal = reinterpret_cast<AiMovingOriginalFn>(*original);
  } else {
    SPDLOG_ERROR("HooksManager: failed to hook AI moving at 0x{0:08X}", kAiMovingHookAddress);
  }
}

void __fastcall HooksManager::OnRender(oCGame* gameInstance) {
  HooksManager* hm = HooksManager::GetInstance();
  if (g_renderOriginal) {
    g_renderOriginal(gameInstance);
  }

  static bool logged = false;
  if (!logged) {
    SPDLOG_INFO("HooksManager::OnRender invoked");
    logged = true;
  }

  EnterCriticalSection(&hm->RenderCs);
  HooksSet::iterator end = hm->OnRenderHooks.end();
  for (HooksSet::iterator it = hm->OnRenderHooks.begin(); it != end; ++it) {
    typedef void (*fptr)(void);
    fptr p = (fptr)(*it);
    p();
  }
  if (!hm->RenderCallbacksToDelete.empty()) {
    for (size_t idx = 0; idx < hm->RenderCallbacksToDelete.size(); ++idx) {
      hm->OnRenderHooks.erase(hm->RenderCallbacksToDelete[idx]);
    }
    hm->RenderCallbacksToDelete.clear();
  }
  LeaveCriticalSection(&hm->RenderCs);
}

void __fastcall HooksManager::OnAiMoving(zCAIPlayer* aiPlayer, void* /*unusedEdx*/, zCVob* targetVob) {
  HooksManager* hm = HooksManager::GetInstance();
  if (g_aiMovingOriginal) {
    g_aiMovingOriginal(aiPlayer, targetVob);
  }

  EnterCriticalSection(&hm->AiMovingCs);
  HooksSet::iterator end = hm->OnAiMovingHooks.end();
  for (HooksSet::iterator it = hm->OnAiMovingHooks.begin(); it != end; ++it) {
    typedef void (*fptr)(void);
    fptr p = (fptr)(*it);
    p();
  }
  if (!hm->AiMovingCallbacksToDelete.empty()) {
    for (size_t idx = 0; idx < hm->AiMovingCallbacksToDelete.size(); ++idx) {
      hm->OnAiMovingHooks.erase(hm->AiMovingCallbacksToDelete[idx]);
    }
    hm->AiMovingCallbacksToDelete.clear();
  }
  LeaveCriticalSection(&hm->AiMovingCs);
}

void __stdcall HooksManager::OnCloseLoadScreen() {
  HooksManager* hm = HooksManager::GetInstance();
  EnterCriticalSection(&hm->CloseLoadScreenCs);
  HooksSet::iterator end = hm->OnCloseLoadScreenHooks.end();
  for (HooksSet::iterator it = hm->OnCloseLoadScreenHooks.begin(); it != end; ++it) {
    typedef void (*fptr)(void);
    fptr p = (fptr)(*it);
    p();
  }
  LeaveCriticalSection(&hm->CloseLoadScreenCs);
}

void __stdcall HooksManager::OnDone() {
  HooksManager* hm = HooksManager::GetInstance();
  EnterCriticalSection(&hm->DoneCs);
  HooksSet::iterator end = hm->OnDoneHooks.end();
  for (HooksSet::iterator it = hm->OnDoneHooks.begin(); it != end; ++it) {
    typedef void (*fptr)(void);
    fptr p = (fptr)(*it);
    p();
  }
  LeaveCriticalSection(&hm->DoneCs);
}

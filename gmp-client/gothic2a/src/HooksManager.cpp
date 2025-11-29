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

#include <cstdio>

#include "ZenGin/zGothicAPI.h"
#include "renderer/D3D9Renderer.h"

namespace {

constexpr DWORD kRenderHookAddress = 0x006C86A0;
constexpr DWORD kAiMovingHookAddress = 0x0050E750;
constexpr DWORD kVidIsResolutionValidAddress = 0x0042C140;  // VidIsResolutionValid

using RenderOriginalFn = void(__thiscall*)(oCGame*);
using AiMovingOriginalFn = void(__thiscall*)(zCAIPlayer*, zCVob*);

RenderOriginalFn g_renderOriginal = nullptr;
AiMovingOriginalFn g_aiMovingOriginal = nullptr;

// Original VidIsResolutionValid function pointer
using VidIsResolutionValidFn = int(__cdecl*)(int x, int y, int bpp);
VidIsResolutionValidFn g_vidIsResolutionValidOriginal = nullptr;

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

constexpr bool kUseDx9Renderer = true;

void HooksManager::InitAllPatches() {
  if (kUseDx9Renderer) {
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

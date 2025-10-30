
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

namespace {

constexpr DWORD kRenderHookAddress = 0x006C86A0;
constexpr DWORD kAiMovingHookAddress = 0x0050E750;

using Gothic_II_Addon::oCGame;
using Gothic_II_Addon::zCAIPlayer;
using Gothic_II_Addon::zCVob;

using RenderOriginalFn = void(__thiscall*)(oCGame*);
using AiMovingOriginalFn = void(__thiscall*)(zCAIPlayer*, zCVob*);

RenderOriginalFn g_renderOriginal = nullptr;
AiMovingOriginalFn g_aiMovingOriginal = nullptr;

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
void HooksManager::InitAllPatches() {
  if (auto original = CreateHook(kRenderHookAddress, (DWORD)&HooksManager::OnRender)) {
    g_renderOriginal = reinterpret_cast<RenderOriginalFn>(*original);
  } else {
    SPDLOG_ERROR("HooksManager: failed to hook render at 0x{0:08X}", kRenderHookAddress);
  }

  // CreateHook(0x6C2C26, (DWORD)&HooksManager::OnCloseLoadScreen); Nie potrzebne for now
  // CreateHook(0x4254E0, (DWORD)&HooksManager::OnDone); Nie potrzebne for now

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

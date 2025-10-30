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

#include "MemoryPatch.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <map>
#include <memory>
#include <polyhook2/Detour/x86Detour.hpp>

namespace MemoryPatch {

struct HookInfo {
  std::unique_ptr<PLH::x86Detour> detour;
  uint64_t original;
  DWORD callback;
};

static std::map<DWORD, HookInfo> g_hookInfo;

void WriteMemory(DWORD dwAddress, PBYTE pbBytes, DWORD dwLength) {
  if (!dwAddress || !pbBytes || !dwLength)
    return;

  void* pAddress = reinterpret_cast<void*>(dwAddress);
  DWORD oldProtect;

  if (VirtualProtect(pAddress, dwLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    memcpy(pAddress, pbBytes, dwLength);
    VirtualProtect(pAddress, dwLength, oldProtect, &oldProtect);
  }
}

void EraseMemory(DWORD dwAddress, BYTE bValue, DWORD dwLength) {
  if (!dwAddress || !dwLength)
    return;

  void* pAddress = reinterpret_cast<void*>(dwAddress);
  DWORD oldProtect;

  if (VirtualProtect(pAddress, dwLength, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    memset(pAddress, bValue, dwLength);
    VirtualProtect(pAddress, dwLength, oldProtect, &oldProtect);
  }
}

void JmpPatch(DWORD dwAddress, DWORD dwDest) {
  if (!dwAddress || !dwDest)
    return;

  BYTE* pAddress = reinterpret_cast<BYTE*>(dwAddress);
  DWORD oldProtect;

  if (VirtualProtect(pAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    // E9 = JMP rel32
    pAddress[0] = 0xE9;
    *reinterpret_cast<DWORD*>(pAddress + 1) = dwDest - dwAddress - 5;
    VirtualProtect(pAddress, 5, oldProtect, &oldProtect);
  }
}

void CallPatch(DWORD dwAddress, DWORD dwDest, int nNops) {
  if (!dwAddress || !dwDest)
    return;

  BYTE* pAddress = reinterpret_cast<BYTE*>(dwAddress);
  size_t patchSize = 5 + nNops;
  DWORD oldProtect;

  if (VirtualProtect(pAddress, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    // E8 = CALL rel32
    pAddress[0] = 0xE8;
    *reinterpret_cast<DWORD*>(pAddress + 1) = dwDest - dwAddress - 5;

    // Fill remaining bytes with NOPs
    for (int i = 0; i < nNops; ++i) {
      pAddress[5 + i] = 0x90;
    }

    VirtualProtect(pAddress, patchSize, oldProtect, &oldProtect);
  }
}

std::optional<void*> CreateHook(DWORD destination, DWORD callback) {
  if (!destination || !callback)
    return std::nullopt;

  uint64_t original = 0;
  auto detour = std::make_unique<PLH::x86Detour>(static_cast<uint64_t>(destination), static_cast<uint64_t>(callback), &original);

  if (!detour->hook()) {
    SPDLOG_ERROR("CreateHook: detour->hook() failed at 0x{0:08X}", destination);
    return std::nullopt;
  }

  HookInfo info;
  info.detour = std::move(detour);
  info.original = original;
  info.callback = callback;
  g_hookInfo[destination] = std::move(info);
  SPDLOG_INFO("CreateHook: installed hook at 0x{0:08X} -> 0x{1:08X} (original at 0x{2:08X})", destination, callback, static_cast<DWORD>(original));
  return reinterpret_cast<void*>(original);
}

}  // namespace MemoryPatch

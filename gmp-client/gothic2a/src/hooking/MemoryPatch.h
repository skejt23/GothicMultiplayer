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

#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>

// Basic Win32-compatible type aliases retained for legacy call sites
typedef unsigned char BYTE;
typedef BYTE* PBYTE;
typedef unsigned long DWORD;

// Hooking helpers built on top of PolyHook2
namespace MemoryPatch {

// Write arbitrary bytes to memory address
void WriteMemory(DWORD dwAddress, PBYTE pbBytes, DWORD dwLength);

// Fill memory region with a specific byte value (typically 0x90 for NOPs)
void EraseMemory(DWORD dwAddress, BYTE bValue, DWORD dwLength);

// Create a jump patch from dwAddress to dwDest
void JmpPatch(DWORD dwAddress, DWORD dwDest);

// Create a call patch from dwAddress to dwDest with optional NOPs
void CallPatch(DWORD dwAddress, DWORD dwDest, int nNops);

// Create a detour hook at destination that calls callback.
// Returns the original function pointer (PolyHook trampoline) on success, std::nullopt on failure.
std::optional<void*> CreateHook(DWORD destination, DWORD callback);

}  // namespace MemoryPatch

// Legacy global wrappers retained for existing call sites
inline void WriteMemory(DWORD dwAddress, PBYTE pbBytes, DWORD dwLength) {
  MemoryPatch::WriteMemory(dwAddress, pbBytes, dwLength);
}

inline void EraseMemory(DWORD dwAddress, BYTE bValue, DWORD dwLength) {
  MemoryPatch::EraseMemory(dwAddress, bValue, dwLength);
}

inline void JmpPatch(DWORD dwAddress, DWORD dwDest) {
  MemoryPatch::JmpPatch(dwAddress, dwDest);
}

inline void CallPatch(DWORD dwAddress, DWORD dwDest, int nNops) {
  MemoryPatch::CallPatch(dwAddress, dwDest, nNops);
}

inline std::optional<void*> CreateHook(DWORD destination, DWORD callback) {
  return MemoryPatch::CreateHook(destination, callback);
}

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

#include <asmjit/asmjit.h>
#include <windows.h>
#include <cstdint>
#include <functional>

namespace AsmPatch {

// AsmJit runtime for generating executable code
asmjit::JitRuntime& GetRuntime();

// Helper type for patch building functions
// The function receives an x86::Assembler to emit code
using PatchBuilder = std::function<void(asmjit::x86::Assembler&)>;

// Build a patch using AsmJit and return the executable code pointer
// Returns nullptr on failure
void* BuildPatch(const PatchBuilder& builder);

// Install a mid-function patch at the given address
// patchSize: number of bytes to overwrite (will be filled with JMP + NOPs)
// builder: function that emits the patch code using AsmJit
// Returns the address of the generated code, or nullptr on failure
void* InstallMidFunctionPatch(DWORD patchAddress, size_t patchSize, const PatchBuilder& builder);

}  // namespace AsmPatch

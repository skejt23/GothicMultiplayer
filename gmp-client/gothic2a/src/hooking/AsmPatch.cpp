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

#include "AsmPatch.h"
#include "MemoryPatch.h"
#include <spdlog/spdlog.h>

namespace AsmPatch {

asmjit::JitRuntime& GetRuntime() {
    static asmjit::JitRuntime runtime;
    return runtime;
}

void* BuildPatch(const PatchBuilder& builder) {
    asmjit::CodeHolder code;
    code.init(GetRuntime().environment(), GetRuntime().cpuFeatures());
    
    asmjit::x86::Assembler assembler(&code);
    
    // Let the caller emit the patch code
    builder(assembler);
    
    // Finalize and add to runtime
    void* result = nullptr;
    asmjit::Error err = GetRuntime().add(&result, &code);
    if (err) {
        SPDLOG_ERROR("AsmJit error: {}", asmjit::DebugUtils::errorAsString(err));
        return nullptr;
    }
    
    return result;
}

void* InstallMidFunctionPatch(DWORD patchAddress, size_t patchSize, const PatchBuilder& builder) {
    if (patchSize < 5) {
        SPDLOG_ERROR("Patch size must be at least 5 bytes for a JMP instruction");
        return nullptr;
    }
    
    void* patchCode = BuildPatch(builder);
    if (!patchCode) {
        return nullptr;
    }
    
    // Install the jump to our patch
    MemoryPatch::JmpPatch(patchAddress, reinterpret_cast<DWORD>(patchCode));
    
    // NOP out any remaining bytes
    if (patchSize > 5) {
        MemoryPatch::EraseMemory(patchAddress + 5, 0x90, patchSize - 5);
    }
    
    SPDLOG_DEBUG("Installed mid-function patch at 0x{:08X} -> 0x{:08X}", 
                 patchAddress, reinterpret_cast<DWORD>(patchCode));
    
    return patchCode;
}

}  // namespace AsmPatch

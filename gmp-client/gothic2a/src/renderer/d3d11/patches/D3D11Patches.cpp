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

#include "renderer/d3d11/patches/D3D11Patches.h"

#include <spdlog/spdlog.h>

#include "ZenGin/zGothicAPI.h"
#include "hooking/MemoryPatch.h"
#include "renderer/d3d11/ShaderSemanticBridge.h"

namespace gmp::renderer::d3d11 {
namespace {

// zCRenderManager::DrawVertexBuffer(zCVertexBuffer*, int, int, unsigned short*, unsigned long, zCShader*)
// Captures authoritative stage semantics (base vs lightmap) built by the engine.
constexpr DWORD kRenderManagerDrawVBWithShader = 0x005D9390;

// IMPORTANT: This is a real __fastcall member in the original engine:
//   ECX = this, EDX = first explicit argument (zCVertexBuffer*)
using RenderManagerDrawVBWithShaderFn = void(__fastcall*)(Gothic_II_Addon::zCRenderManager*, Gothic_II_Addon::zCVertexBuffer*, int, int,
                                                          unsigned short*, unsigned long, Gothic_II_Addon::zCShader*);
RenderManagerDrawVBWithShaderFn g_rmDrawVBWithShaderOriginal = nullptr;

void __fastcall Hook_RenderManager_DrawVBWithShader(Gothic_II_Addon::zCRenderManager* self, Gothic_II_Addon::zCVertexBuffer* vb, int first_vert,
                                                    int num_vert, unsigned short* index_list, unsigned long num_index,
                                                    Gothic_II_Addon::zCShader* shader) {
  // Bridge shader-stage semantics (stage0 lightmap, stage1 base) into our D3D11 renderer.
  // This avoids fragile heuristics based on texture names / stale stage state.
  ShaderSemanticBridge::ScopedOverride scoped(shader);

  if (g_rmDrawVBWithShaderOriginal) {
    g_rmDrawVBWithShaderOriginal(self, vb, first_vert, num_vert, index_list, num_index, shader);
  }
}

}  // namespace

void InitializeD3D11Patches() {
  // =============================================================================
  // Hook zCRenderManager::DrawVertexBuffer(..., zCShader*) to capture authoritative
  // base/lightmap stage ordering (ZenGin BuildShader sets stage0=LIGHTMAP, stage1=BASE).
  // This provides the D3D11 renderer with accurate shader semantics.
  // =============================================================================
  if (auto original = CreateHook(kRenderManagerDrawVBWithShader, (DWORD)Hook_RenderManager_DrawVBWithShader)) {
    g_rmDrawVBWithShaderOriginal = reinterpret_cast<RenderManagerDrawVBWithShaderFn>(*original);
  } else {
    SPDLOG_ERROR("Failed to hook zCRenderManager::DrawVertexBuffer(zCShader*) at 0x{:08X}", kRenderManagerDrawVBWithShader);
  }
}

}  // namespace gmp::renderer::d3d11

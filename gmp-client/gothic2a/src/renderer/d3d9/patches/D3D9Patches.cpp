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

#include "renderer/d3d9/patches/D3D9Patches.h"

#include <spdlog/spdlog.h>

#include <cstdint>

#include "ZenGin/zGothicAPI.h"
#include "hooking/MemoryPatch.h"

using namespace Gothic_II_Addon;

namespace gmp::renderer::d3d9 {
namespace {

// =============================================================================
// INVENTORY ITEM RENDER FIX (oCItem::Render hook)
// =============================================================================
//
// PROBLEM:
// Glass/metal items (potions, etc.) appear with incorrect metallic/corrupted
// textures when rendered in the D3D9 inventory view.
//
// WORKAROUND:
// Hook oCItem::Render and, for inventory world rendering, skip the env map
// setup by calling oCVob::Render directly instead of letting oCItem::Render
// enable environment mapping.
//
// TODO: Root cause not fully understood. Gothic's oCItem::Render enables
// environment mapping for glass/metal materials before rendering.
// This works correctly in D3D7 but produces incorrect results in D3D9.
// We don't know exactly WHY the env map calculation differs - possibilities include:
//   - Different texture stage state defaults between D3D7 and D3D9
//   - Different env map texture coordinate generation
//   - Our D3D9 renderer not replicating some implicit D3D7 behavior
//   - The separate inventory world context affecting env map calculations
//
// For now, bypassing env map setup entirely is a working workaround that
// produces visually correct results.
// =============================================================================

// oCItem::Render(zTRenderContext&) at 0x00714020
// Calling convention: __fastcall (this in ECX, renderContext in EDX)
constexpr DWORD kOCItemRenderAddress = 0x00714020;

// oCVob::Render(zTRenderContext&) at 0x0077D3B0
// This is the parent class render - calling it directly skips oCItem's env map setup
constexpr DWORD kOCVobRenderAddress = 0x0077D3B0;

// Original function pointer
using OCItemRenderFn = int(__fastcall*)(oCItem*, zTRenderContext&);
OCItemRenderFn g_originalOCItemRender = nullptr;

// Direct call to oCVob::Render (parent class)
using OCVobRenderFn = int(__fastcall*)(oCVob*, zTRenderContext&);

// Hook for oCItem::Render
// Intercepts item rendering to fix the inventory metallic/corrupted appearance.
int __fastcall Hook_OCItemRender(oCItem* item, zTRenderContext& renderContext) {
  // Check if this is an inventory world render
  bool is_inventory = false;

  if (renderContext.world) {
    is_inventory = renderContext.world->m_bIsInventoryWorld != 0;
  }

  if (is_inventory) {
    // INVENTORY RENDER FIX:
    // Skip oCItem::Render entirely and call oCVob::Render directly.
    // This bypasses the env map enable/disable logic that causes the metallic look.

    // Force fullbright lighting for clean, consistent appearance
    renderContext.hintLightingFullbright = 1;
    renderContext.hintLightingSwell = 0;

    // Call oCVob::Render directly, bypassing env map setup
    auto callOCVobRender = reinterpret_cast<OCVobRenderFn>(kOCVobRenderAddress);
    return callOCVobRender(item, renderContext);
  }

  // For normal world rendering, use original oCItem::Render (preserves env map effects)
  if (g_originalOCItemRender) {
    return g_originalOCItemRender(item, renderContext);
  }
  return 0;
}

}  // namespace

void InitializeD3D9Patches() {
  // =============================================================================
  // Hook oCItem::Render to fix inventory rendering of glass/metal items.
  // See detailed explanation above.
  // =============================================================================
  if (auto original = CreateHook(kOCItemRenderAddress, (DWORD)Hook_OCItemRender)) {
    g_originalOCItemRender = reinterpret_cast<OCItemRenderFn>(*original);
  } else {
    SPDLOG_ERROR("Failed to hook oCItem::Render at 0x{:08X}", kOCItemRenderAddress);
  }
}

}  // namespace gmp::renderer::d3d9

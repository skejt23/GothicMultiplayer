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

#include <cstdint>
#include <optional>

#include "ZenGin/zGothicAPI.h"

namespace gmp::renderer::d3d11 {

struct ShaderSemantic {
  bool valid = false;
  bool has_lightmap = false;

  // Indices into Gothic's shader stages (zCShader::stageList[]).
  // Typically: base=1, lightmap=0 for world multitexture lightmaps.
  int base_stage_index = -1;
  int lightmap_stage_index = -1;

  Gothic_II_Addon::zCTexture* base_texture = nullptr;
  Gothic_II_Addon::zCTexture* lightmap_texture = nullptr;

  // Base-stage RGB generation mode from zCShaderStage::rgbGen.
  // We use this to decide whether vertex colors should be applied in the base pass.
  int base_rgb_gen = -1;

  // Base-stage colorFactor (ARGB packed in zCOLOR) used by rgbGen=FACTOR.
  std::uint32_t base_color_factor_argb = 0xFFFFFFFFu;

  // Derived expectations for VB formats with 2 UV sets.
  // base_uv_index usually 0, lightmap_uv_index usually 1.
  int base_uv_index = 0;
  int lightmap_uv_index = 1;
};

// Thread-local bridge from ZenGin's zCShader semantics to our renderer.
// Populated by a hook on zCRenderManager::DrawVertexBuffer(..., zCShader*).
namespace ShaderSemanticBridge {

std::optional<ShaderSemantic> TryGetCurrent();

// RAII helper to override the current semantic for the calling thread.
class ScopedOverride {
public:
  explicit ScopedOverride(const Gothic_II_Addon::zCShader* shader);
  ~ScopedOverride();

  ScopedOverride(const ScopedOverride&) = delete;
  ScopedOverride& operator=(const ScopedOverride&) = delete;

private:
  std::optional<ShaderSemantic> previous_;
  bool had_previous_ = false;
};

}  // namespace ShaderSemanticBridge

}  // namespace gmp::renderer::d3d11

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

#include "renderer/d3d11/ShaderSemanticBridge.h"

#include <spdlog/spdlog.h>

#include <algorithm>

#include "ZenGin/Gothic_II_Addon/API/zRenderManager.h"

namespace gmp::renderer::d3d11 {
namespace {

thread_local bool g_has_current = false;
thread_local ShaderSemantic g_current;

ShaderSemantic CaptureFromShader(const Gothic_II_Addon::zCShader* shader) {
  ShaderSemantic out;
  if (shader == nullptr) {
    return out;
  }

  out.valid = true;
  out.has_lightmap = (shader->hasLightmap != 0);

  const int num_stages = std::clamp(shader->numStages, 0, static_cast<int>(Gothic_II_Addon::MAX_STAGES));

  int base_idx = -1;
  int lm_idx = -1;

  for (int i = 0; i < num_stages; ++i) {
    const Gothic_II_Addon::zCShaderStage* stage = shader->stageList[i];
    if (stage == nullptr) {
      continue;
    }

    const bool is_lm = (stage->shaderFXMode == Gothic_II_Addon::zSHD_FX_LIGHTMAP || stage->shaderFXMode == Gothic_II_Addon::zSHD_FX_LIGHTMAP_DYN ||
                        stage->tcGen == Gothic_II_Addon::zSHD_TCGEN_LIGHTMAP);

    const bool is_base = (stage->shaderFXMode == Gothic_II_Addon::zSHD_FX_BASETEX || stage->tcGen == Gothic_II_Addon::zSHD_TCGEN_BASE);

    if (lm_idx < 0 && is_lm) {
      lm_idx = i;
      out.lightmap_texture = stage->texture;
    }
    if (base_idx < 0 && is_base) {
      base_idx = i;
      out.base_texture = stage->texture;
      out.base_rgb_gen = static_cast<int>(stage->rgbGen);
      out.base_color_factor_argb = static_cast<std::uint32_t>(stage->colorFactor.dword);
    }
  }

  // If we didn't find explicit markers, apply conservative defaults.
  // World multi-texture lightmaps in G2 often use stage0=lightmap, stage1=base.
  if (out.has_lightmap) {
    if (lm_idx < 0 && num_stages >= 1) {
      lm_idx = 0;
      out.lightmap_texture = shader->stageList[0] ? shader->stageList[0]->texture : nullptr;
    }
    if (base_idx < 0 && num_stages >= 2) {
      base_idx = 1;
      out.base_texture = shader->stageList[1] ? shader->stageList[1]->texture : nullptr;
      out.base_rgb_gen = shader->stageList[1] ? static_cast<int>(shader->stageList[1]->rgbGen) : -1;
      out.base_color_factor_argb = shader->stageList[1] ? static_cast<std::uint32_t>(shader->stageList[1]->colorFactor.dword) : 0xFFFFFFFFu;
    }
  } else {
    if (base_idx < 0 && num_stages >= 1) {
      base_idx = 0;
      out.base_texture = shader->stageList[0] ? shader->stageList[0]->texture : nullptr;
      out.base_rgb_gen = shader->stageList[0] ? static_cast<int>(shader->stageList[0]->rgbGen) : -1;
      out.base_color_factor_argb = shader->stageList[0] ? static_cast<std::uint32_t>(shader->stageList[0]->colorFactor.dword) : 0xFFFFFFFFu;
    }
  }

  out.base_stage_index = base_idx;
  out.lightmap_stage_index = lm_idx;

  // UV expectations (engine's PackVB uses UV0 for base, UV1 for lightmap).
  out.base_uv_index = 0;
  out.lightmap_uv_index = 1;

  return out;
}

void SetCurrentFromShader(const Gothic_II_Addon::zCShader* shader) {
  g_current = CaptureFromShader(shader);
  g_has_current = g_current.valid;
}

void ClearCurrent() {
  g_has_current = false;
  g_current = ShaderSemantic{};
}

}  // namespace

namespace ShaderSemanticBridge {

std::optional<ShaderSemantic> TryGetCurrent() {
  if (!g_has_current) {
    return std::nullopt;
  }
  return g_current;
}

ScopedOverride::ScopedOverride(const Gothic_II_Addon::zCShader* shader) {
  if (g_has_current) {
    previous_ = g_current;
    had_previous_ = true;
  }
  SetCurrentFromShader(shader);
}

ScopedOverride::~ScopedOverride() {
  if (had_previous_) {
    g_current = *previous_;
    g_has_current = true;
  } else {
    ClearCurrent();
  }
}

}  // namespace ShaderSemanticBridge

}  // namespace gmp::renderer::d3d11

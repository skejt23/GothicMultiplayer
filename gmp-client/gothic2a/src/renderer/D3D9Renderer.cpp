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

#include "D3D9Renderer.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "D3D9DisplayModes.h"
#include "D3D9RendererImpl.h"
#include "D3D9Texture.h"
#include "D3D9VertexBuffer.h"
#include "ZenGin/Gothic_II_Addon/API/z3d.h"
#include "ZenGin/Gothic_II_Addon/API/zRenderer.h"
#include "ZenGin/Gothic_II_Addon/API/zTexConvert.h"
#include "ZenGin/Gothic_II_Addon/API/zVertexTransform.h"

namespace {

constexpr unsigned long kTrue32 = 1;
constexpr unsigned long kFalse32 = 0;
constexpr int kMaxBuckets = 512;

// Mask to extract RGB components from a 32-bit ARGB color (discards alpha).
constexpr unsigned long kRgbMask = 0x00FFFFFF;

constexpr std::array<unsigned long, zRND_TOP_COUNT> kTextureOpMap = {D3DTOP_DISABLE,
                                                                     D3DTOP_SELECTARG1,
                                                                     D3DTOP_SELECTARG2,
                                                                     D3DTOP_MODULATE,
                                                                     D3DTOP_MODULATE2X,
                                                                     D3DTOP_MODULATE4X,
                                                                     D3DTOP_ADD,
                                                                     D3DTOP_ADDSIGNED,
                                                                     D3DTOP_ADDSIGNED2X,
                                                                     D3DTOP_SUBTRACT,
                                                                     D3DTOP_ADDSMOOTH,
                                                                     D3DTOP_BLENDDIFFUSEALPHA,
                                                                     D3DTOP_BLENDTEXTUREALPHA,
                                                                     D3DTOP_BLENDFACTORALPHA,
                                                                     D3DTOP_BLENDTEXTUREALPHAPM,
                                                                     D3DTOP_BLENDCURRENTALPHA,
                                                                     D3DTOP_PREMODULATE,
                                                                     D3DTOP_MODULATEALPHA_ADDCOLOR,
                                                                     D3DTOP_MODULATECOLOR_ADDALPHA,
                                                                     D3DTOP_MODULATEINVALPHA_ADDCOLOR,
                                                                     D3DTOP_MODULATEINVCOLOR_ADDALPHA,
                                                                     D3DTOP_BUMPENVMAP,
                                                                     D3DTOP_BUMPENVMAPLUMINANCE,
                                                                     D3DTOP_DOTPRODUCT3};

constexpr std::array<unsigned long, 6> kTextureArgMap = {D3DTA_CURRENT, D3DTA_DIFFUSE, D3DTA_SELECTMASK,
                                                         D3DTA_TEXTURE, D3DTA_TFACTOR, D3DTA_SPECULAR};

enum SamplerSlot : size_t {
  kSamplerAddressU = 0,
  kSamplerAddressV,
  kSamplerBorderColor,
  kSamplerMagFilter,
  kSamplerMinFilter,
  kSamplerMipFilter,
  kSamplerMipMapLodBias,
  kSamplerMaxMipLevel,
  kSamplerMaxAnisotropy
};

// Maps Gothic texture operation enum to D3D9 texture operation.
unsigned long MapTextureOp(unsigned long op) {
  return (op < kTextureOpMap.size()) ? kTextureOpMap[op] : D3DTOP_DISABLE;
}

// Maps Gothic texture argument enum to D3D9 texture argument.
unsigned long MapTextureArg(unsigned long arg) {
  return (arg < kTextureArgMap.size()) ? kTextureArgMap[arg] : D3DTA_DIFFUSE;
}

// Maps Gothic texture transform flags to D3D9 flags.
unsigned long MapTextureTransformFlags(unsigned long value) {
  switch (value) {
    case zRND_TTF_COUNT1:
      return D3DTTFF_COUNT1;
    case zRND_TTF_COUNT2:
      return D3DTTFF_COUNT2;
    case zRND_TTF_COUNT3:
      return D3DTTFF_COUNT3;
    case zRND_TTF_COUNT4:
      return D3DTTFF_COUNT4;
    default:
      return D3DTTFF_DISABLE;
  }
}

bool ConvertPrimitiveType(zTVBufferPrimitiveType inType, D3DPRIMITIVETYPE& outType) {
  switch (inType) {
    case zVBUFFER_PT_POINTLIST:
      outType = D3DPT_POINTLIST;
      return true;
    case zVBUFFER_PT_LINELIST:
      outType = D3DPT_LINELIST;
      return true;
    case zVBUFFER_PT_LINESTRIP:
      outType = D3DPT_LINESTRIP;
      return true;
    case zVBUFFER_PT_TRIANGLELIST:
      outType = D3DPT_TRIANGLELIST;
      return true;
    case zVBUFFER_PT_TRIANGLESTRIP:
      outType = D3DPT_TRIANGLESTRIP;
      return true;
    case zVBUFFER_PT_TRIANGLEFAN:
      outType = D3DPT_TRIANGLEFAN;
      return true;
    default:
      break;
  }

  return false;
}

}  // namespace

namespace {

// Sentinel value for state cache invalidation.
// Uses std::numeric_limits to clearly express intent: an invalid/uninitialized state.
constexpr unsigned long kCacheInvalidSentinel = std::numeric_limits<unsigned long>::max();

// Scale factor for converting integer zbias values to D3D depth bias.
// Gothic uses integer bias values (0-15), D3D expects small float offsets.
constexpr float kZBiasScale = 0.0001f;

// Linear attenuation coefficient for point lights.
// Controls how quickly light intensity falls off with distance.
constexpr float kPointLightLinearAttenuation = 0.009f;

void SetIdentityMatrix(zMAT4& m) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      m[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }
}

// RAII guard for temporarily disabling radial fog during rendering.
// Radial fog must be disabled for certain geometry (particles, alpha polys)
// and restored afterward. This guard handles the save/restore automatically.
class ScopedRadialFogDisable {
public:
  ScopedRadialFogDisable(zCRnd_D3D_DX9* renderer, gmp::renderer::D3D9FogManager& fog_manager)
      : renderer_(renderer),
        fog_manager_(fog_manager),
        fog_was_enabled_(renderer->GetFog() != 0),
        radial_was_enabled_(fog_manager.IsRadialEnabled() && fog_was_enabled_) {
    if (radial_was_enabled_) {
      renderer_->SetFog(false);
      fog_manager_.SetRadialEnabled(false);
      renderer_->SetFog(true);
    }
  }

  ~ScopedRadialFogDisable() {
    if (radial_was_enabled_) {
      renderer_->SetFog(false);
      fog_manager_.SetRadialEnabled(true);
      renderer_->SetFog(true);
    }
    renderer_->SetFog(fog_was_enabled_);
  }

  // Non-copyable
  ScopedRadialFogDisable(const ScopedRadialFogDisable&) = delete;
  ScopedRadialFogDisable& operator=(const ScopedRadialFogDisable&) = delete;

private:
  zCRnd_D3D_DX9* renderer_;
  gmp::renderer::D3D9FogManager& fog_manager_;
  bool fog_was_enabled_;
  bool radial_was_enabled_;
};

// RAII guard for alpha rendering pass state management.
// On construction: Prepares state for alpha polygon rendering.
// On destruction: Restores state for subsequent opaque geometry rendering.
class ScopedAlphaRenderPass {
public:
  explicit ScopedAlphaRenderPass(zCRnd_D3D_DX9* renderer) : renderer_(renderer) {
    // No setup needed - individual DrawQueuedAlphaPoly calls configure per-poly state
  }

  ~ScopedAlphaRenderPass() {
    renderer_->RestoreOpaqueRenderState();
  }

  // Non-copyable
  ScopedAlphaRenderPass(const ScopedAlphaRenderPass&) = delete;
  ScopedAlphaRenderPass& operator=(const ScopedAlphaRenderPass&) = delete;

private:
  zCRnd_D3D_DX9* renderer_;
};

}  // namespace

zCRnd_D3D_DX9::zCRnd_D3D_DX9() {
  SPDLOG_TRACE("zCRnd_D3D_DX9::zCRnd_D3D_DX9() - Initializing DX9 Renderer");
  impl_ = std::make_unique<D3D9RendererImpl>();

  // Initialize matrices to identity
  SetIdentityMatrix(view_matrix_);
  SetIdentityMatrix(proj_matrix_);

  // Invalidate state caches with sentinel value so any first SetRenderState call will update D3D
  std::ranges::fill(render_state_cache_, kCacheInvalidSentinel);
  for (auto& stage : tex_stage_state_cache_) {
    std::ranges::fill(stage, kCacheInvalidSentinel);
  }
  for (auto& sampler : sampler_state_cache_) {
    std::ranges::fill(sampler, kCacheInvalidSentinel);
  }

  // Initialize state tracking members (most already have default values from class definition)
  active_texture_.fill(nullptr);
  alpha_sort_bucket_.fill(nullptr);
}

zCRnd_D3D_DX9::~zCRnd_D3D_DX9() {
  SPDLOG_TRACE("zCRnd_D3D_DX9::~zCRnd_D3D_DX9()");
}

void zCRnd_D3D_DX9::BeginFrame() {
  SPDLOG_TRACE("BeginFrame");

  alpha_sort_bucket_.fill(nullptr);

  auto* activeCam = zCCamera::activeCam;
  float farClipZ = 65535.0f;
  float nearClipZ = 0.25f;
  if (activeCam) {
    farClipZ = activeCam->farClipZ;
    nearClipZ = activeCam->nearClipZ;
  }
  if (farClipZ < 500.0f) {
    farClipZ = 500.0f;
  }
  if (nearClipZ <= 0.0f) {
    nearClipZ = 0.25f;
  }

  z_max_from_engine_ = farClipZ;
  z_min_from_engine_ = nearClipZ;
  bucket_size_ = static_cast<float>(kMaxBuckets) / z_max_from_engine_;

  // Set bucket size on our alpha poly queue (same calculation as above)
  alpha_poly_queue_.SetFarClipZ(farClipZ);

  // ----------------------------------------------------------------------------
  // Reciprocal Z-Buffer Scaling (circa 2000-2002 technique)
  // ----------------------------------------------------------------------------
  // Gothic II uses a hyperbolic depth mapping: z_ndc = A + B / eye_z
  //
  // This "reciprocal Z" approach was common in early hardware (D3D7/D3D8 era)
  // to combat the notorious depth buffer precision problem:
  //   - Linear Z-buffers waste most precision on near objects
  //   - Distant objects get very little precision, causing Z-fighting
  //
  // The hyperbolic curve redistributes precision more evenly across the view
  // frustum. Modern engines (2015+) typically use "reversed-Z" with floating-
  // point depth buffers instead, but we must preserve the original formula
  // exactly to maintain compatibility with Gothic's world geometry, culling
  // systems, and visibility calculations.
  //
  // Formula derivation:
  //   A = zFar / (zFar - zNear)
  //   B = -zFar * zNear / (zFar - zNear)
  // ----------------------------------------------------------------------------
  const float denom = z_max_from_engine_ - z_min_from_engine_;
  if (denom <= std::numeric_limits<float>::epsilon()) {
    SPDLOG_WARN("BeginFrame: Invalid clip range near={} far={} (forcing identity scaling)", z_min_from_engine_, z_max_from_engine_);
    z_proj_offset_ = 1.0f;
    z_proj_scale_ = 0.0f;
  } else {
    z_proj_offset_ = z_max_from_engine_ / denom;
    z_proj_scale_ = -z_max_from_engine_ * z_min_from_engine_ / denom;
  }

  if (fog_manager_.IsEnabled()) {
    SetFog(1);
  }

  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_CURRENT);
  SetTextureStageState(1, zRND_TSS_COLOROP, zRND_TOP_DISABLE);

  // Level polys use zbias 0 so vobs can have negative bias for layering
  SetZBias(0);  // DEFAULT_LEVEL_ZBIAS = 0
}

void zCRnd_D3D_DX9::EndFrame() {
  SPDLOG_TRACE("EndFrame");

  // Call backend EndFrame to draw test triangle on top of everything
  if (impl_) {
    impl_->EndFrame();
  }

  // EndFrame just cleans up frame state, does NOT call EndScene or Present
}

void zCRnd_D3D_DX9::FlushPolys() {
  // Skip rendering during device reset
  if (GetSurfaceLost()) {
    poly_batch_.clear();
    alpha_poly_queue_.Reset();
    immediate_alpha_poly_queue_.Reset();
    return;
  }

  // 1. Flush the batched immediate polygons (e.g. Sky, Particles, UI)
  if (!poly_batch_.empty()) {
    EnsureSceneBegun();

    // Use a static vector for vertices to avoid reallocations
    static std::vector<VertexRHW> verts;

    for (zCPolygon* poly : poly_batch_) {
      if (!poly || poly->numClipVert < 3)
        continue;

      // Set texture from material
      zCTexture* tex = nullptr;
      void* d3dTex = nullptr;
      if (poly->material) {
        tex = poly->material->GetAniTexture();
        if (tex) {
          tex->CacheIn(-1);
          // Get the D3D9 texture pointer for batch state tracking
          auto* tex9 = static_cast<zCTex_D3D9*>(tex);
          if (tex9) {
            d3dTex = tex9->GetDX9Texture();
          }
        }
      }
      SetTexture(0, tex);

      int numVerts = poly->numClipVert;
      verts.clear();
      verts.reserve(numVerts);

      for (int i = 0; i < numVerts; ++i) {
        int transIdx = poly->clipVert[i]->transformedIndex;
        auto* vertTrans = zCVertexTransform::GetVert(transIdx);
        auto* feat = poly->clipFeat[i];

        VertexRHW v;
        v.x = vertTrans->vertScrX;
        v.y = vertTrans->vertScrY;
        v.rhw = vertTrans->vertCamSpaceZInv;
        v.z = z_proj_offset_ + z_proj_scale_ * v.rhw;
        v.u = feat->texu;
        v.v = feat->texv;
        v.color = feat->lightDyn.dword;

        verts.push_back(v);
      }

      if (impl_ && !verts.empty()) {
        impl_->BatchTriangleFan(verts.data(), static_cast<int>(verts.size()), d3dTex);
      }
    }

    // Flush remaining batched geometry
    if (impl_) {
      impl_->FlushBatch();
    }

    poly_batch_.clear();
  }

  // 2. Render the alpha-blended polygon sort list
  // This is critical for transparent objects, particles, effects, etc.
  RenderAlphaSortList();
}

void zCRnd_D3D_DX9::ResetMultiTexturing() {
  SetTextureStageState(1, zRND_TSS_COLOROP, zRND_TOP_DISABLE);
  SetTextureStageState(1, zRND_TSS_ALPHAOP, zRND_TOP_DISABLE);
  SetTextureStageState(1, zRND_TSS_TEXCOORDINDEX, 0);
}

void zCRnd_D3D_DX9::InvalidateStateCache() {
  // After device recreation, the D3D device has lost all state.
  // We must invalidate our state caches so the next render calls will re-apply everything.
  // Use the sentinel value that means "not set" - any value comparison will fail and force a D3D call.
  std::ranges::fill(render_state_cache_, kCacheInvalidSentinel);
  for (auto& stage : tex_stage_state_cache_) {
    std::ranges::fill(stage, kCacheInvalidSentinel);
  }
  for (auto& sampler : sampler_state_cache_) {
    std::ranges::fill(sampler, kCacheInvalidSentinel);
  }

  // Also clear active texture tracking
  active_texture_.fill(nullptr);
  active_material_ = nullptr;
}

bool zCRnd_D3D_DX9::ActivateMaterial(zCMaterial* material) {
  if (material == active_material_ && material->texture == active_texture_[0]) {
    return true;
  }

  active_material_ = material;
  zTRnd_AlphaBlendFunc alpha_func = material->rndAlphaBlendFunc;
  zCTexture* tex = material->GetAniTexture();
  active_texture_[0] = tex;

  // No texture case
  if (tex == nullptr) {
    SetTexture(0, nullptr);
    ApplyOpaqueRenderStates();
    return true;
  }

  // Try to cache the texture
  if (tex->CacheIn(-1) != zRES_CACHED_IN) {
    if (alpha_func != zRND_ALPHA_FUNC_NONE) {
      return false;  // Defer alpha poly with uncached texture
    }
    active_texture_[0] = nullptr;
    SetTexture(0, nullptr);
    ApplyOpaqueRenderStates();
    return true;
  }

  // Alpha blending requested - defer to alpha system
  if (alpha_func != zRND_ALPHA_FUNC_NONE) {
    return true;
  }

  // Texture with alpha channel - use alpha testing for punch-through
  if (tex->HasAlpha() && alpha_test_enabled_) {
    ApplyAlphaTestStates();
    SetTexture(0, tex);
    ApplyTextureAddressMode();
    return true;
  }

  // Opaque texture without alpha
  ApplyOpaqueRenderStates();
  SetTexture(0, tex);
  ApplyTextureAddressMode();
  return true;
}

// ApplyTextureAddressMode - Sets texture wrapping/clamping based on current state.
void zCRnd_D3D_DX9::ApplyTextureAddressMode() {
  DWORD mode = texture_wrap_enabled_ ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSU, mode);
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSV, mode);
}

// ApplyOpaqueRenderStates - Configures render state for opaque geometry.
void zCRnd_D3D_DX9::ApplyOpaqueRenderStates() {
  ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
  ApplyRenderState(D3DRS_ALPHATESTENABLE, kFalse32);
  ApplyRenderState(D3DRS_DITHERENABLE, dither_enabled_ ? kTrue32 : kFalse32);
  ApplyRenderState(D3DRS_ZWRITEENABLE, z_buffer_write_enabled_ ? kTrue32 : kFalse32);
  SetZBufferCompare(z_buffer_cmp_);
}

// ApplyAlphaTestStates - Configures render state for alpha-tested geometry.
// Used for textures with alpha channel to achieve punch-through transparency.
void zCRnd_D3D_DX9::ApplyAlphaTestStates() {
  // Configure texture stage for modulated color with texture alpha
  SetTextureStageState(0, zRND_TSS_TEXTURETRANSFORMFLAGS, zRND_TTF_DISABLE);
  SetTextureStageState(0, zRND_TSS_TEXCOORDINDEX, 0);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_DIFFUSE);
  SetTextureStageState(0, zRND_TSS_ALPHAOP, zRND_TOP_SELECTARG1);
  SetTextureStageState(0, zRND_TSS_ALPHAARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_ALPHAARG2, zRND_TA_DIFFUSE);

  // Enable alpha test with reference threshold
  ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
  ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
  ApplyRenderState(D3DRS_ALPHATESTENABLE, kTrue32);
  ApplyRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
  ApplyRenderState(D3DRS_ALPHAREF, alpha_reference_);
  ApplyRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  ApplyRenderState(D3DRS_ZWRITEENABLE, kTrue32);
  ApplyRenderState(D3DRS_DITHERENABLE, kFalse32);
}

// BuildAlphaPolyVertices - Populates vertex data for an alpha polygon.
//
// This helper extracts common logic used by both QueueAlphaPoly (deferred rendering)
// and AddAlphaPoly (immediate rendering). It handles:
//
// 1. Fixed-point alpha multiplication: `(a * b) >> 8` computes `a * b / 256`
//    using integer math. This was standard practice in software renderers and
//    remains efficient for 8-bit color component blending.
//
// 2. D3DCOLOR packing: `(a << 24) | (r << 16) | (g << 8) | b` packs ARGB
//    components into a single 32-bit DWORD in Direct3D's native color format.
//
// 3. Centroid Z for depth sorting: Averaging vertex Z values gives the polygon's
//    approximate center depth, used for back-to-front alpha sorting (painter's
//    algorithm).
//
void zCRnd_D3D_DX9::BuildAlphaPolyVertices(gmp::renderer::QueuedAlphaPoly* ap, const zCPolygon* poly, const zCMaterial* mat) {
  using namespace gmp::renderer;

  // Configure render state.
  ap->texture = active_texture_[0];
  ap->texture_wrap = texture_wrap_enabled_ != 0;
  ap->texture_has_alpha = active_texture_[0] && active_texture_[0]->HasAlpha();
  ap->z_func = static_cast<ZBufferCmp>(z_buffer_cmp_);
  ap->z_bias = active_status_.zbias;

  // Determine blend function.
  const auto alpha_source = ap->texture_has_alpha ? zRND_ALPHA_SOURCE_MATERIAL : alpha_blend_source_;
  if (alpha_source == zRND_ALPHA_SOURCE_CONSTANT && mat->rndAlphaBlendFunc == zRND_ALPHA_FUNC_NONE) {
    ap->blend_func = static_cast<AlphaBlendFunc>(alpha_blend_func_);
  } else {
    ap->blend_func = static_cast<AlphaBlendFunc>(mat->rndAlphaBlendFunc);
  }

  ap->vert_count = std::min(poly->numClipVert, kAlphaPolyMaxVerts);

  const auto alpha_factor_int = static_cast<unsigned long>(alpha_blend_factor_ * 256.0f);
  const auto mat_alpha = static_cast<unsigned long>(mat->color.alpha);
  const bool has_texture = (ap->texture != nullptr);

  float z_sum = 0.0f;
  for (int i = 0; i < ap->vert_count; ++i) {
    const auto* feat = poly->clipFeat[i];
    const auto* vert = zCVertexTransform::GetVert(poly->clipVert[i]->transformedIndex);
    const auto feat_alpha = static_cast<unsigned long>(feat->lightDyn.alpha);

    // Compute alpha based on source mode.
    unsigned long alpha;
    if (alpha_source == zRND_ALPHA_SOURCE_CONSTANT) {
      alpha = (alpha_factor_int * feat_alpha) >> 8;
    } else if (alpha_blend_source_ == zRND_ALPHA_SOURCE_CONSTANT) {
      alpha = (alpha_factor_int * feat_alpha * mat_alpha) >> 16;
    } else {
      alpha = (feat_alpha * mat_alpha) >> 8;
    }

    const float rhw = vert->vertCamSpaceZInv;
    z_sum += vert->vertCamSpace.n[2];

    auto& v = ap->verts[i];
    v.x = vert->vertScrX;
    v.y = vert->vertScrY;
    v.z = z_proj_offset_ + z_proj_scale_ * rhw;
    v.rhw = rhw;
    v.u = feat->texu;
    v.v = feat->texv;

    if (has_texture) {
      v.color = (alpha << 24) | (feat->lightDyn.dword & kRgbMask);
    } else {
      const auto r = (static_cast<unsigned long>(feat->lightDyn.r) * static_cast<unsigned long>(mat->color.r)) >> 8;
      const auto g = (static_cast<unsigned long>(feat->lightDyn.g) * static_cast<unsigned long>(mat->color.g)) >> 8;
      const auto b = (static_cast<unsigned long>(feat->lightDyn.b) * static_cast<unsigned long>(mat->color.b)) >> 8;
      v.color = (alpha << 24) | (r << 16) | (g << 8) | b;
    }
  }

  ap->z_value = z_sum / static_cast<float>(ap->vert_count);
}

// QueueAlphaPoly - Queues a polygon for deferred alpha-blended rendering.
//
// Submits to the sorted alpha queue for proper back-to-front rendering
// during RenderAlphaSortList.
//
void zCRnd_D3D_DX9::QueueAlphaPoly(zCPolygon* poly, zCMaterial* mat) {
  if (!poly || poly->numClipVert < 3) {
    return;
  }

  auto* ap = alpha_poly_queue_.Allocate();
  if (!ap) {
    alpha_limit_reached_ = TRUE;
    return;
  }
  alpha_limit_reached_ = alpha_poly_queue_.IsLimitReached() ? TRUE : FALSE;

  BuildAlphaPolyVertices(ap, poly, mat);
  alpha_poly_queue_.Submit(ap);
}

void zCRnd_D3D_DX9::DrawPolyVertexLit(zCPolygon* poly) {
  if (poly->numClipVert < 3)
    return;

  zCMaterial* mat = poly->material;
  if (!mat)
    return;

  if (!ActivateMaterial(mat))
    return;

  const bool isAlphaBlend = !(alpha_blend_source_ == zRND_ALPHA_SOURCE_MATERIAL && mat->rndAlphaBlendFunc == zRND_ALPHA_FUNC_NONE);

  if (isAlphaBlend) {
    QueueAlphaPoly(poly, mat);
    return;
  }

  const bool hasTexture = (active_texture_[0] != nullptr);
  const bool hasTextureAlpha = hasTexture && active_texture_[0]->HasAlpha();

  // Build vertices - consolidate all paths into single loop
  static std::vector<VertexRHW> verts;
  verts.clear();
  verts.reserve(poly->numClipVert);

  for (int i = 0; i < poly->numClipVert; i++) {
    auto* vertTrans = zCVertexTransform::GetVert(poly->clipVert[i]->transformedIndex);
    auto* feat = poly->clipFeat[i];

    DWORD color;
    float u, v;
    if (hasTexture) {
      color = feat->lightDyn.dword;
      u = feat->texu;
      v = feat->texv;
    } else {
      // Mix dynamic light with material color
      unsigned long lr = (feat->lightDyn.r * mat->color.r) >> 8;
      unsigned long lg = (feat->lightDyn.g * mat->color.g) >> 8;
      unsigned long lb = (feat->lightDyn.b * mat->color.b) >> 8;
      color = (lr << 16) | (lg << 8) | lb;
      u = 0.0f;
      v = 0.0f;
    }

    verts.push_back({
        .x = vertTrans->vertScrX,
        .y = vertTrans->vertScrY,
        .z = z_proj_offset_ + z_proj_scale_ * vertTrans->vertCamSpaceZInv,
        .rhw = vertTrans->vertCamSpaceZInv,
        .color = color,
        .u = u,
        .v = v,
    });
  }

  // Only disable alpha blend for non-alpha textures
  if (!hasTextureAlpha) {
    ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
    SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  }
  ResetMultiTexturing();

  if (impl_ && !verts.empty()) {
    impl_->DrawTriangleFan(verts.data(), static_cast<int>(verts.size()));
  }
}

// DrawPoly - Immediate mode polygon rendering
//
// With T&L hardware enforced (which we require), world geometry (indoor and outdoor)
// uses zCRenderManager::PackVB() -> DrawVertexBuffer() for batched T&L rendering.
// DrawPoly is only called for dynamic/special geometry that bypasses zCRenderManager:
// - Animated models, particles, poly strips
// - Sky screen blend effects
// - Special effects and overlays
//
// Since these dynamic polygons don't have lightmaps, we use vertex-lit rendering
// for all cases. Alpha-blended polygons are automatically queued for sorted rendering.
void zCRnd_D3D_DX9::DrawPoly(zCPolygon* poly) {
  if (!poly || poly->polyNumVert < 3)
    return;

  // Block rendering during device reset
  if (GetSurfaceLost())
    return;

  // Temporarily disable radial fog for dynamic geometry rendering
  ScopedRadialFogDisable fog_guard(this, fog_manager_);

  ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  // Disable clipping (better performance)
  ApplyRenderState(D3DRS_CLIPPING, kFalse32);
  ApplyRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

  // With T&L enforced, world geometry goes through DrawVertexBuffer.
  // DrawPoly is used for dynamic geometry (models, particles, sky effects).
  // These don't have lightmaps, so we always use vertex-lit rendering.
  // Alpha-blended polys are automatically queued for sorted rendering.
  DrawPolyVertexLit(poly);
}

void zCRnd_D3D_DX9::DrawLightmapList(zCPolygon** polyList, int numPolys) {
  for (int i = 0; i < numPolys; ++i) {
    DrawPoly(polyList[i]);
  }
}

void zCRnd_D3D_DX9::DrawLine(float x1, float y1, float x2, float y2, zCOLOR color) {
  EnsureSceneBegun();
  if (impl_) {
    impl_->DrawLine(x1, y1, x2, y2, color.dword);
  }
}

void zCRnd_D3D_DX9::DrawLineZ(float x1, float y1, float z1, float x2, float y2, float z2, zCOLOR color) {
  // TODO: Implement 3D line drawing
}

void zCRnd_D3D_DX9::SetPixel(float x, float y, zCOLOR color) {
  // TODO
}

// DrawPolySimple - Renders 2D/UI polygons with pre-transformed vertices.
//
// This function uses several standard Direct3D rendering patterns:
//
// 1. Pre-transformed (RHW) vertices: The VertexRHW format (x, y, z, rhw, color, u, v)
//    is Direct3D's "transformed and lit" vertex format. Setting rhw=1.0 bypasses the
//    transformation pipeline for screen-space 2D rendering.
//
// 2. Triangle fan primitive: Vertex 0 is shared by all triangles, making it efficient
//    for convex polygons like quads. Standard for UI rendering.
//
// 3. Virtual screen coordinates: The 0-8192 coordinate system was a common pattern in
//    90s/2000s games for resolution-independent UI. We detect and scale as needed.
//
// 4. Alpha blending equations: The SRC_BLEND/DEST_BLEND pairs implement standard
//    compositing operations:
//    - BLEND: src*srcAlpha + dest*(1-srcAlpha) - standard transparency
//    - ADD:   src*srcAlpha + dest*1 - additive glow/fire effects
//    - MUL:   src*destColor + dest*0 - multiplicative darkening
//    - MUL2:  src*destColor + dest*srcColor - 2x multiply for contrast
//
// 5. Fog save/restore: 2D elements are rendered with fog disabled to prevent scene
//    fog from affecting UI overlays.
//
void zCRnd_D3D_DX9::DrawPolySimple(zCTexture* texture, zTRndSimpleVertex* vertices, int num_vertices) {
  using enum zTRnd_AlphaBlendFunc;

  if (!vertices || num_vertices < 3 || !impl_ || GetSurfaceLost()) {
    return;
  }

  EnsureSceneBegun();

  // Save and disable fog for 2D rendering.
  const int saved_fog = GetFog();
  SetFog(0);

  SetTexture(0, texture);

  // Configure render state for 2D/UI.
  impl_->SetRenderState(D3DRS_CLIPPING, FALSE);
  impl_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  impl_->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

  // Disable multitexturing.
  impl_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  impl_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

  // Configure texture stage 0.
  impl_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  impl_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
  impl_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  impl_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

  SetZBufferCompare(active_status_.zfunc);

  // Configure alpha blending based on current mode.
  bool uses_alpha_blend = false;
  switch (active_status_.alphafunc) {
    case zRND_ALPHA_FUNC_BLEND:
      ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
      impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      uses_alpha_blend = true;
      break;

    case zRND_ALPHA_FUNC_ADD:
      ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
      impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
      uses_alpha_blend = true;
      break;

    case zRND_ALPHA_FUNC_MUL:
      ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
      impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
      break;

    case zRND_ALPHA_FUNC_MUL2:
      ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
      impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
      break;

    case zRND_ALPHA_FUNC_SUB:
      // SUB not directly supported; fall through to BLEND.
      SPDLOG_WARN("DrawPolySimple: Unsupported alpha function SUB, using BLEND");
      [[fallthrough]];

    default:
      // NONE, TEST, MAT_DEFAULT - check texture alpha.
      if (texture && texture->HasAlpha()) {
        ApplyRenderState(D3DRS_ALPHABLENDENABLE, kTrue32);
        impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        impl_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        uses_alpha_blend = true;
      } else {
        ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
        impl_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
      }
      break;
  }

  impl_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

  // Detect coordinate space and compute scaling factors.
  // Gothic may pass pixel coordinates or legacy 0-8192 view-space coordinates.
  constexpr float kLegacyViewSpace = 8192.0f;
  const float screen_width = (vid_xdim > 0) ? static_cast<float>(vid_xdim) : kLegacyViewSpace;
  const float screen_height = (vid_ydim > 0) ? static_cast<float>(vid_ydim) : kLegacyViewSpace;

  float max_x = 0.0f;
  float max_y = 0.0f;
  for (int i = 0; i < num_vertices; ++i) {
    max_x = std::max(max_x, vertices[i].pos.n[0]);
    max_y = std::max(max_y, vertices[i].pos.n[1]);
  }

  auto compute_scale = [kLegacyViewSpace](float max_val, float screen_val) {
    if (screen_val <= 0.0f || max_val <= 0.0f) {
      return 1.0f;
    }
    if (max_val <= screen_val + 0.5f) {
      return 1.0f;  // Already in pixel space.
    }
    if (max_val <= kLegacyViewSpace + 0.5f) {
      return screen_val / kLegacyViewSpace;  // Legacy 0-8192 coordinates.
    }
    return screen_val / max_val;  // Fallback clamp.
  };

  const float scale_x = compute_scale(max_x, screen_width);
  const float scale_y = compute_scale(max_y, screen_height);

  // Build pre-transformed vertices.
  // UI polygons are typically quads (4 verts), but allow up to 16 for safety.
  constexpr int kMaxSimpleVerts = 16;
  VertexRHW verts[kMaxSimpleVerts];
  const int vert_count = std::min(num_vertices, kMaxSimpleVerts);

  constexpr float kMidDepth = 0.5f;  // Safe depth value, mid-range.
  constexpr float kRhw = 1.0f;       // Orthographic projection.

  for (int i = 0; i < vert_count; ++i) {
    verts[i].x = vertices[i].pos.n[0] * scale_x;
    verts[i].y = vertices[i].pos.n[1] * scale_y;
    verts[i].z = kMidDepth;
    verts[i].rhw = kRhw;
    verts[i].u = vertices[i].uv.n[0];
    verts[i].v = vertices[i].uv.n[1];

    // Apply alpha factor for blend modes.
    if (uses_alpha_blend && active_status_.alphasrc == zRND_ALPHA_SOURCE_CONSTANT) {
      const auto src_alpha = (vertices[i].color.dword >> 24) & 0xff;
      const auto alpha = static_cast<unsigned long>(active_status_.alphafactor * static_cast<float>(src_alpha));
      verts[i].color = (alpha << 24) | (vertices[i].color.dword & kRgbMask);
    } else {
      verts[i].color = vertices[i].color.dword;
    }
  }

  impl_->DrawTriangleFan(verts, vert_count);

  SetFog(saved_fog);
}

// Properly enables/disables fog with all associated state.
// Delegates to fog_manager_ for actual D3D9 state management.
void zCRnd_D3D_DX9::SetFog(int enable) {
  // Early return if state hasn't changed.
  if (active_status_.fog == enable)
    return;

  active_status_.fog = enable;
  fog_manager_.SetEnabled(enable != 0);
}

int zCRnd_D3D_DX9::GetFog() const {
  return fog_manager_.IsEnabled() ? 1 : 0;
}

void zCRnd_D3D_DX9::SetRadialFog(int enable) {
  fog_manager_.SetRadialEnabled(enable != 0);
}

int zCRnd_D3D_DX9::GetRadialFog() const {
  return fog_manager_.IsRadialEnabled() ? 1 : 0;
}

void zCRnd_D3D_DX9::SetFogColor(const zCOLOR& color) {
  fog_manager_.SetColor(color);
}

zCOLOR zCRnd_D3D_DX9::GetFogColor() const {
  return fog_manager_.GetColor();
}

void zCRnd_D3D_DX9::SetFogRange(float nearZ, float farZ, int mode) {
  fog_manager_.SetRange(nearZ, farZ, mode);
}

void zCRnd_D3D_DX9::GetFogRange(float& nearZ, float& farZ, int& mode) {
  fog_manager_.GetRange(nearZ, farZ, mode);
}

zTRnd_PolyDrawMode zCRnd_D3D_DX9::GetPolyDrawMode() const {
  return poly_draw_mode_;
}

void zCRnd_D3D_DX9::SetPolyDrawMode(const zTRnd_PolyDrawMode& mode) {
  poly_draw_mode_ = mode;
  if (impl_) {
    switch (mode) {
      case zRND_DRAW_WIRE:
        impl_->SetFillMode(D3DFILL_WIREFRAME);
        break;
      case zRND_DRAW_FLAT:
      case zRND_DRAW_MATERIAL:
      default:
        impl_->SetFillMode(D3DFILL_SOLID);
        break;
    }
  }
}

int zCRnd_D3D_DX9::GetSurfaceLost() const {
  return surface_lost_;
}

void zCRnd_D3D_DX9::SetSurfaceLost(int lost) {
  surface_lost_ = lost;
}

int zCRnd_D3D_DX9::GetSyncOnAmbientCol() const {
  // Not used.
  return 0;
}

void zCRnd_D3D_DX9::SetSyncOnAmbientCol(int sync) {
  // Not used.
}

void zCRnd_D3D_DX9::SetTextureWrapEnabled(int enable) {
  texture_wrap_enabled_ = enable;
  active_status_.texwrap = enable;  // Update for alpha poly system
  if (impl_) {
    impl_->SetTextureWrap(0, enable);
    impl_->SetTextureWrap(1, enable);
  }
}

int zCRnd_D3D_DX9::GetTextureWrapEnabled() const {
  return texture_wrap_enabled_;
}

// Only applies to stage 0, uses LINEAR for bilinear or POINT for nearest
void zCRnd_D3D_DX9::SetBilerpFilterEnabled(int enable) {
  bilerp_filter_enabled_ = enable;
  active_status_.filter = enable;
  if (impl_) {
    // filter 0 = point, 1 = bilinear, 2 = anisotropic
    int filter = enable ? 1 : 0;
    impl_->SetTextureFilter(0, filter);
  }
}

int zCRnd_D3D_DX9::GetBilerpFilterEnabled() const {
  return bilerp_filter_enabled_;
}

void zCRnd_D3D_DX9::SetDitherEnabled(int enable) {
  dither_enabled_ = enable;
  ApplyRenderState(D3DRS_DITHERENABLE, enable ? kTrue32 : kFalse32);
}

int zCRnd_D3D_DX9::GetDitherEnabled() const {
  return dither_enabled_;
}

zTRnd_PolySortMode zCRnd_D3D_DX9::GetPolySortMode() const {
  return poly_sort_mode_;
}

void zCRnd_D3D_DX9::SetPolySortMode(const zTRnd_PolySortMode& mode) {
  poly_sort_mode_ = mode;

  // Enable or disable Z-buffer based on sort mode
  if (mode == zRND_SORT_ZBUFFER) {
    ApplyRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
  } else {
    ApplyRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
  }
}

int zCRnd_D3D_DX9::GetZBufferWriteEnabled() const {
  return z_buffer_write_enabled_;
}

void zCRnd_D3D_DX9::SetZBufferWriteEnabled(int enable) {
  z_buffer_write_enabled_ = enable;
  ApplyRenderState(D3DRS_ZWRITEENABLE, enable ? kTrue32 : kFalse32);
}

void zCRnd_D3D_DX9::SetZBias(int bias) {
  z_bias_ = bias;
  active_status_.zbias = bias;  // Update for alpha poly system
  float fBias = static_cast<float>(bias) * kZBiasScale;
  if (impl_)
    impl_->SetZBias(fBias);
}

int zCRnd_D3D_DX9::GetZBias() const {
  return z_bias_;
}

zTRnd_ZBufferCmp zCRnd_D3D_DX9::GetZBufferCompare() {
  return z_buffer_cmp_;
}

void zCRnd_D3D_DX9::SetZBufferCompare(const zTRnd_ZBufferCmp& cmp) {
  z_buffer_cmp_ = cmp;
  active_status_.zfunc = cmp;  // Update for alpha poly system
  unsigned long func = D3DCMP_LESSEQUAL;
  switch (cmp) {
    case zRND_ZBUFFER_CMP_NEVER:
      func = D3DCMP_NEVER;
      break;
    case zRND_ZBUFFER_CMP_LESS:
      func = D3DCMP_LESS;
      break;
    case zRND_ZBUFFER_CMP_ALWAYS:
      func = D3DCMP_ALWAYS;
      break;
    case zRND_ZBUFFER_CMP_LESS_EQUAL:
    default:
      func = D3DCMP_LESSEQUAL;
      break;
  }
  ApplyRenderState(D3DRS_ZFUNC, func);
}

int zCRnd_D3D_DX9::GetPixelWriteEnabled() const {
  return pixel_write_enabled_;
}

void zCRnd_D3D_DX9::SetPixelWriteEnabled(int enable) {
  pixel_write_enabled_ = enable;
  DWORD mask = enable ? (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA) : 0;
  if (impl_)
    impl_->SetColorWrite(mask);
}

void zCRnd_D3D_DX9::SetAlphaBlendSource(const zTRnd_AlphaBlendSource& src) {
  alpha_blend_source_ = src;
  active_status_.alphasrc = src;  // Update for alpha poly system
}

zTRnd_AlphaBlendSource zCRnd_D3D_DX9::GetAlphaBlendSource() const {
  return alpha_blend_source_;
}

// The actual D3D state application happens in SetAlphaBlendFuncImmed or
// during rendering (e.g., DrawPolySimple switches on active_status_.alphafunc).
void zCRnd_D3D_DX9::SetAlphaBlendFunc(const zTRnd_AlphaBlendFunc& mode) {
  alpha_blend_func_ = mode;
  active_status_.alphafunc = mode;
}

zTRnd_AlphaBlendFunc zCRnd_D3D_DX9::GetAlphaBlendFunc() const {
  return alpha_blend_func_;
}

float zCRnd_D3D_DX9::GetAlphaBlendFactor() const {
  return alpha_blend_factor_;
}

void zCRnd_D3D_DX9::SetAlphaBlendFactor(const float& factor) {
  alpha_blend_factor_ = factor;
  active_status_.alphafactor = factor;
}

void zCRnd_D3D_DX9::SetAlphaReference(unsigned long ref) {
  alpha_reference_ = ref;
}

unsigned long zCRnd_D3D_DX9::GetAlphaReference() const {
  return alpha_reference_;
}

int zCRnd_D3D_DX9::GetCacheAlphaPolys() const {
  return 1;
}

void zCRnd_D3D_DX9::SetCacheAlphaPolys(int cache) {
  // TODO
}

int zCRnd_D3D_DX9::GetAlphaLimitReached() const {
  return alpha_limit_reached_;
}

// AddAlphaPoly - Queues a polygon for immediate alpha-blended rendering.
//
// Called by Gothic's particle system and effects for transparent geometry
// that should be rendered in the current frame's immediate alpha pass.
//
void zCRnd_D3D_DX9::AddAlphaPoly(const zCPolygon* poly) {
  if (!poly || poly->numClipVert < 3) {
    return;
  }

  auto* mat = poly->material;
  if (!mat || !ActivateMaterial(mat)) {
    return;
  }

  auto* ap = immediate_alpha_poly_queue_.Allocate();
  if (!ap) {
    return;
  }

  BuildAlphaPolyVertices(ap, poly, mat);
}

// FlushAlphaPolys - Renders and clears the immediate alpha poly queue.
void zCRnd_D3D_DX9::FlushAlphaPolys() {
  using namespace gmp::renderer;

  if (immediate_alpha_poly_queue_.IsEmpty()) {
    return;
  }

  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  ApplyRenderState(D3DRS_CLIPPING, kFalse32);
  ApplyRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

  immediate_alpha_poly_queue_.RenderInOrder([this](const QueuedAlphaPoly& ap) { DrawQueuedAlphaPoly(&ap); });

  immediate_alpha_poly_queue_.Reset();
}

void zCRnd_D3D_DX9::SetRenderMode(zTRnd_RenderMode mode) {
  if (!impl_)
    return;

  render_mode_ = static_cast<int>(mode);
  SPDLOG_TRACE("SetRenderMode: {}", render_mode_);

  // Set up standard render states for geometry rendering.
  // With T&L hardware enforced, we don't need special 2-pass lightmap mode handling.
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
  SetZBufferWriteEnabled(TRUE);
  SetZBufferCompare(zRND_ZBUFFER_CMP_LESS);
  ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_DIFFUSE);
}

zTRnd_RenderMode zCRnd_D3D_DX9::GetRenderMode() const {
  return static_cast<zTRnd_RenderMode>(render_mode_);
}

int zCRnd_D3D_DX9::HasCapability(zTRnd_Capability cap) const {
  // Modern D3D9 hardware supports everything Gothic II needs
  switch (cap) {
    case zRND_CAP_GUARD_BAND:
      return TRUE;  // All modern cards support guard band clipping
    case zRND_CAP_ALPHATEST:
      return TRUE;  // Alpha testing universally supported
    case zRND_CAP_MAX_BLEND_STAGES:
      return 8;  // D3D9 supports at least 8 texture stages
    case zRND_CAP_MAX_BLEND_STAGE_TEXTURES:
      // D3D9 supports at least 8 textures in multitexture mode
      // critical for staged multitexturing (e.g., lightmaps)
      return 8;
    case zRND_CAP_DIFFUSE_LAST_BLEND_STAGE_ONLY:
      return FALSE;  // Modern cards can use diffuse in any stage
    case zRND_CAP_TNL:
      return TRUE;  // Transform & Lighting supported
    case zRND_CAP_TNL_HARDWARE:
      return TRUE;  // Hardware T&L available
    case zRND_CAP_TNL_MAXLIGHTS:
      return 8;  // D3D9 spec minimum is 8, but modern cards support more
    case zRND_CAP_DEPTH_BUFFER_PREC:
      return 32;  // Modern hardware supports 32-bit depth (D24S8 or D32)
    case zRND_CAP_BLENDDIFFUSEALPHA:
      return TRUE;  // Blend diffuse alpha supported
    default:
      return FALSE;
  }
}

void zCRnd_D3D_DX9::GetGuardBandBorders(float& left, float& right, float& top, float& bottom) {
  // not used by Gothic II, so we can leave it unimplemented
}

void zCRnd_D3D_DX9::ResetZTest() {
  // TODO
}

int zCRnd_D3D_DX9::HasPassedZTest() {
  return 1;
}

zCTexture* zCRnd_D3D_DX9::CreateTexture() {
  return new zCTex_D3D9();
}

zCTextureConvert* zCRnd_D3D_DX9::CreateTextureConvert() {
  // Use the original CPU-side converter; it already handles all Gothic texture formats.
  return new zCTexConGeneric();
}

int zCRnd_D3D_DX9::GetTotalTextureMem() {
  if (impl_) {
    size_t bytes = impl_->GetAvailableTextureMem();
    if (bytes > 0) {
      return static_cast<int>(std::min<size_t>(bytes, std::numeric_limits<int>::max()));
    }
  }
  return 256 * 1024 * 1024;
}

int zCRnd_D3D_DX9::SupportsTextureFormat(zTRnd_TextureFormat fmt) {
  switch (fmt) {
    case zRND_TEX_FORMAT_ARGB_8888:
    case zRND_TEX_FORMAT_ABGR_8888:
    case zRND_TEX_FORMAT_RGBA_8888:
    case zRND_TEX_FORMAT_BGRA_8888:
    case zRND_TEX_FORMAT_RGB_888:
    case zRND_TEX_FORMAT_BGR_888:
    case zRND_TEX_FORMAT_ARGB_4444:
    case zRND_TEX_FORMAT_ARGB_1555:
    case zRND_TEX_FORMAT_RGB_565:
    case zRND_TEX_FORMAT_PAL_8:
    case zRND_TEX_FORMAT_DXT1:
    case zRND_TEX_FORMAT_DXT3:
    case zRND_TEX_FORMAT_DXT5:
      return 1;
    default:
      return 0;
  }
}

int zCRnd_D3D_DX9::SupportsTextureFormatHardware(zTRnd_TextureFormat fmt) {
  switch (fmt) {
    case zRND_TEX_FORMAT_DXT1:
    case zRND_TEX_FORMAT_DXT3:
    case zRND_TEX_FORMAT_DXT5:
      return 1;
    default:
      return SupportsTextureFormat(fmt);
  }
}

int zCRnd_D3D_DX9::GetMaxTextureSize() {
  return impl_->GetCapabilities().max_texture_size;
}

void zCRnd_D3D_DX9::GetStatistics(zTRnd_Stats& stats) {
  // Not implemented - return zeroed stats.
  stats = {};
}

void zCRnd_D3D_DX9::ResetStatistics() {
}

void zCRnd_D3D_DX9::Vid_Blit(int complete, tagRECT* src, tagRECT* dst) {
  SPDLOG_TRACE("Vid_Blit called");
  // This is called every frame by GameManager::Render() after game->Render()

  if (impl_) {
    impl_->Vid_Blit();  // EndScene → Present → BeginScene
  }
}

void zCRnd_D3D_DX9::Vid_Clear(zCOLOR& color, int flags) {
  // Convert zCOLOR to D3DCOLOR (ARGB)
  unsigned long d3dColor = (color.alpha << 24) | (color.r << 16) | (color.g << 8) | color.b;
  SPDLOG_TRACE("Vid_Clear called with flags={}, color=R:{} G:{} B:{} A:{} -> 0x{:08X}", flags, color.r, color.g, color.b, color.alpha, d3dColor);
  if (impl_) {
    unsigned long clearFlags = 0;
    switch (flags) {
      case zRND_CLEAR_FRAMEBUFFER:
        clearFlags = 0x00000001;  // D3DCLEAR_TARGET
        break;
      case zRND_CLEAR_ZBUFFER:
        clearFlags = 0x00000002;  // D3DCLEAR_ZBUFFER
        break;
      default:                    // zRND_CLEAR_ALL or any other value
        clearFlags = 0x00000003;  // D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER
        break;
    }

    // For now, always clear both to match current backend behavior
    // TODO: Update backend Clear to accept flags parameter
    impl_->Clear(d3dColor);
  }
}

int zCRnd_D3D_DX9::Vid_Lock(zTRndSurfaceDesc& desc) {
  SPDLOG_TRACE("Vid_Lock called");
  return 0;
}

int zCRnd_D3D_DX9::Vid_Unlock() {
  SPDLOG_TRACE("Vid_Unlock called");
  return 0;
}

int zCRnd_D3D_DX9::Vid_IsLocked() {
  return 0;
}

int zCRnd_D3D_DX9::Vid_GetFrontBufferCopy(zCTextureConvert& texConv) {
  return 0;
}

int zCRnd_D3D_DX9::Vid_GetNumDevices() {
  SPDLOG_TRACE("Vid_GetNumDevices called");
  return 1;
}

int zCRnd_D3D_DX9::Vid_GetActiveDeviceNr() {
  return 0;
}

int zCRnd_D3D_DX9::Vid_SetDevice(int deviceNr) {
  SPDLOG_TRACE("Vid_SetDevice called with {}", deviceNr);
  return 1;
}

int zCRnd_D3D_DX9::Vid_GetDeviceInfo(zTRnd_DeviceInfo& info, int deviceNr) {
  SPDLOG_TRACE("Vid_GetDeviceInfo called for device {}", deviceNr);
  return 1;
}

int zCRnd_D3D_DX9::Vid_GetNumModes() {
  int count = display_modes_.GetNumModes();
  SPDLOG_INFO("Vid_GetNumModes called, returning {}", count);
  return count;
}

int zCRnd_D3D_DX9::Vid_GetModeInfo(zTRnd_VidModeInfo& info, int modeNr) {
  const auto* mode = display_modes_.GetMode(modeNr);
  if (!mode) {
    SPDLOG_WARN("Vid_GetModeInfo called for invalid mode {}", modeNr);
    return 0;
  }

  info.xdim = mode->width;
  info.ydim = mode->height;
  info.bpp = mode->bpp;
  info.fullscreenOnly = 0;

  SPDLOG_INFO("Vid_GetModeInfo[{}]: {}x{} {}bpp", modeNr, info.xdim, info.ydim, info.bpp);
  return 1;
}

int zCRnd_D3D_DX9::Vid_GetActiveModeNr() {
  return display_modes_.GetActiveModeNr();
}

int zCRnd_D3D_DX9::Vid_SetMode(int modeNr, HWND__** hwnd) {
  SPDLOG_INFO("Vid_SetMode: mode={}", modeNr);
  if (!impl_)
    return 0;

  const auto* mode = display_modes_.GetMode(modeNr);
  if (!mode) {
    SPDLOG_ERROR("Invalid mode number: {}", modeNr);
    return 0;
  }

  // Block all rendering during device reset
  surface_lost_ = 1;

  // Clear all texture stage bindings.
  for (int i = 0; i < 8; i++) {
    SetTexture(i, nullptr);
  }

  // Destroy ALL vertex buffers before device Reset.
  // D3DPOOL_DEFAULT resources must be released before Reset().
  // They will be recreated when Gothic needs them again.
  zCVertexBuffer_D3D9::DestroyAllBuffers();

  // Clear any queued alpha polygons before device reset - they reference stale state
  alpha_poly_queue_.Reset();
  immediate_alpha_poly_queue_.Reset();
  num_alpha_polys_ = 0;

  // Update base class members - CRITICAL for clipping!
  vid_xdim = mode->width;
  vid_ydim = mode->height;
  vid_bpp = mode->bpp;

  SPDLOG_INFO("Setting video mode: {}x{} {}bpp", vid_xdim, vid_ydim, vid_bpp);

  HWND hWindow = (hwnd && *hwnd) ? *hwnd : GetActiveWindow();
  SPDLOG_INFO("Vid_SetMode using HWND: {}", (void*)hWindow);

  if (hWindow) {
    // Ensure window is visible, updated, and sized correctly
    ShowWindow(hWindow, SW_SHOW);
    UpdateWindow(hWindow);

    // Resize window to match resolution (client area)
    RECT rc = {0, 0, vid_xdim, vid_ydim};
    AdjustWindowRect(&rc, GetWindowLong(hWindow, GWL_STYLE), FALSE);
    SetWindowPos(hWindow, HWND_TOP, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_SHOWWINDOW);
  }

  // On first call, device doesn't exist yet - use Init().
  // On subsequent calls (resolution change), use Reset() to preserve D3DPOOL_MANAGED textures.
  bool success = false;
  if (!impl_->GetDevice()) {
    // First initialization - create the device with stored screen mode
    bool fullscreen = (screen_mode_ == zRND_SCRMODE_FULLSCREEN);
    success = impl_->Init(hWindow, vid_xdim, vid_ydim, fullscreen);
  } else {
    // Resolution change - use Reset() to preserve managed textures (including lightmaps)
    success = impl_->Reset(vid_xdim, vid_ydim);
  }

  if (success) {
    surface_lost_ = 0;  // Allow rendering again

    // Initialize fog manager with the new D3D9 device.
    fog_manager_.Init(impl_->GetDevice());

    // Invalidate all render state caches - the new device has default state
    // This ensures the next render calls will re-apply all necessary states
    InvalidateStateCache();

    // This calls Create() to allocate new D3D9 buffers and callbacks to refill data
    // Needed to restore all vertex buffers lost during device Reset() (e.g., resolution change)
    auto& allBuffers = zCVertexBuffer_D3D9::GetAllBuffers();
    for (zCVertexBuffer_D3D9* vb : allBuffers) {
      if (vb) {
        if (!vb->CallRecreateLostVBCallbacks()) {
          SPDLOG_WARN("Vid_SetMode: Failed to recreate vertex buffer");
        }
      }
    }

    impl_->BeginFrame();
    display_modes_.SetActiveModeNr(modeNr);  // Track the active mode for Vid_GetActiveModeNr
    return 1;
  }
  SPDLOG_ERROR("Vid_SetMode: Backend Init failed");
  surface_lost_ = 0;  // Clear flag even on failure
  return 0;
}

void zCRnd_D3D_DX9::Vid_SetScreenMode(zTRnd_ScreenMode mode) {
  SPDLOG_INFO("Vid_SetScreenMode called: {}", (int)mode);

  // HIDE mode is treated as WINDOWED
  if (mode == zRND_SCRMODE_HIDE) {
    mode = zRND_SCRMODE_WINDOWED;
  }

  // Store the requested mode - will be used when Init() creates the device
  screen_mode_ = mode;

  if (!impl_ || !impl_->GetDevice()) {
    // Device not initialized yet - just store the mode for later use in Init()
    SPDLOG_DEBUG("Vid_SetScreenMode: Device not ready, mode {} stored for Init()", (int)mode);
    return;
  }

  // Check if device was already created in the correct mode
  // The device mode is determined at Init() time based on screen_mode_
  // Runtime switching is complex and not currently supported
  SPDLOG_DEBUG("Vid_SetScreenMode: Device already created, mode set to {}", (int)mode);
}

zTRnd_ScreenMode zCRnd_D3D_DX9::Vid_GetScreenMode() {
  return screen_mode_;
}

void zCRnd_D3D_DX9::Vid_SetGammaCorrection(float gamma, float contrast, float brightness) {
  SPDLOG_INFO("Vid_SetGammaCorrection: gamma={:.2f}, contrast={:.2f}, brightness={:.2f}", gamma, contrast, brightness);
  gamma_ = gamma;
  if (impl_) {
    impl_->SetGammaCorrection(gamma, contrast, brightness);
  }
}

float zCRnd_D3D_DX9::Vid_GetGammaCorrection() {
  return gamma_;
}

void zCRnd_D3D_DX9::Vid_BeginLfbAccess() {
  // TODO
}

void zCRnd_D3D_DX9::Vid_EndLfbAccess() {
  // TODO
}

void zCRnd_D3D_DX9::Vid_SetLfbAlpha(int alpha) {
  // TODO
}

void zCRnd_D3D_DX9::Vid_SetLfbAlphaFunc(const zTRnd_AlphaBlendFunc& func) {
  // TODO
}

void zCRnd_D3D_DX9::EnsureSceneBegun() {
  if (!impl_ || GetSurfaceLost()) {
    return;
  }
  impl_->BeginFrame();
}

int zCRnd_D3D_DX9::SetTransform(zTRnd_TrafoType type, const zMAT4& matrix) {
  if (!impl_) {
    return 0;
  }

  switch (type) {
    case zRND_TRAFO_VIEW:
      // Gothic's VIEW matrix is actually a combined model-view (WORLD) matrix.
      // It's already transposed to row-major format by Gothic.
      // We set D3DTS_WORLD and leave D3DTS_VIEW at identity.
      view_matrix_ = matrix;
      impl_->SetTransform(1, (const float*)&view_matrix_);  // D3DTS_WORLD
      break;

    case zRND_TRAFO_PROJECTION:
      // Gothic's projection matrix is already in D3D's row-major format.
      proj_matrix_ = matrix;
      impl_->SetTransform(3, (const float*)&proj_matrix_);  // D3DTS_PROJECTION
      break;

    case zRND_TRAFO_TEXTURE0:
      impl_->SetTransform(4, (const float*)&matrix);  // D3DTS_TEXTURE0
      break;

    case zRND_TRAFO_TEXTURE1:
      impl_->SetTransform(5, (const float*)&matrix);  // D3DTS_TEXTURE1
      break;

    case zRND_TRAFO_TEXTURE2:
      impl_->SetTransform(6, (const float*)&matrix);  // D3DTS_TEXTURE2
      break;

    case zRND_TRAFO_TEXTURE3:
      impl_->SetTransform(7, (const float*)&matrix);  // D3DTS_TEXTURE3
      break;

    default:
      return 0;
  }

  return 1;
}

int zCRnd_D3D_DX9::SetViewport(int x, int y, int w, int h) {
  if (!impl_)
    return 0;
  // Gothic passes pixel coordinates directly - no conversion needed
  impl_->SetViewport(x, y, w, h);
  return 1;
}

namespace {

// Converts degrees to radians.
constexpr float DegToRad(float degrees) {
  constexpr float kDegToRadFactor = 3.14159265358979323846f / 180.0f;
  return degrees * kDegToRadFactor;
}

// Packs RGB floats (0-255 range) into a D3DCOLOR (0x00RRGGBB).
unsigned long PackAmbientColor(const zVEC3& rgb) {
  return ((static_cast<int>(rgb[0]) & 0xff) << 16) | ((static_cast<int>(rgb[1]) & 0xff) << 8) | ((static_cast<int>(rgb[2]) & 0xff) << 0);
}

// Configures D3DLIGHT9 for a point light.
void SetupPointLight(D3DLIGHT9& dlight, const zCRenderLight* light) {
  dlight.Type = D3DLIGHT_POINT;
  dlight.Range = light->range;
  dlight.Attenuation0 = 0.0f;
  dlight.Attenuation1 = kPointLightLinearAttenuation;
  dlight.Attenuation2 = 0.0f;
  std::memcpy(&dlight.Position, &light->positionLS, sizeof(dlight.Position));
}

// Configures D3DLIGHT9 for a spot light.
void SetupSpotLight(D3DLIGHT9& dlight, const zCRenderLight* light) {
  dlight.Type = D3DLIGHT_SPOT;
  dlight.Range = light->range;
  dlight.Attenuation0 = 0.0f;
  dlight.Attenuation1 = 1.0f;
  dlight.Attenuation2 = 0.0f;
  dlight.Falloff = 1.0f;
  dlight.Theta = 0.0f;
  dlight.Phi = DegToRad(40.0f);  // Fixed cone angle; light->spotConeAngle unused.
  std::memcpy(&dlight.Position, &light->positionLS, sizeof(dlight.Position));
}

// Configures D3DLIGHT9 for a directional light.
void SetupDirectionalLight(D3DLIGHT9& dlight, zCRenderLight* light) {
  dlight.Type = D3DLIGHT_DIRECTIONAL;
  // Guard against zero-length direction vector.
  auto& dir = light->directionLS;
  if (dir[VX] == 0.0f && dir[VY] == 0.0f && dir[VZ] == 0.0f) {
    dir = zVEC3(1, 0, 0);
  }
  std::memcpy(&dlight.Direction, &dir, sizeof(dlight.Direction));
}

}  // namespace

int zCRnd_D3D_DX9::SetLight(unsigned long index, zCRenderLight* light) {
  // Track which slot holds the ambient light (if any).
  static std::optional<unsigned long> s_ambient_light_index;

  if (!light) {
    // Disable the light at this index.
    if (s_ambient_light_index == index) {
      s_ambient_light_index.reset();
      impl_->SetRenderState(D3DRS_AMBIENT, 0);
    }
    impl_->LightEnable(index, FALSE);
    return TRUE;
  }

  // Handle ambient light specially - it uses D3DRS_AMBIENT, not a light slot.
  if (light->lightType == zLIGHT_TYPE_AMBIENT) {
    s_ambient_light_index = index;
    impl_->SetRenderState(D3DRS_AMBIENT, PackAmbientColor(light->colorDiffuse));
    impl_->LightEnable(index, FALSE);
    return TRUE;
  }

  // Build D3D light structure.
  D3DLIGHT9 dlight{};

  // Gothic's colorDiffuse is in 0-255 range; D3D9 expects 0.0-1.0.
  constexpr float kColorScale = 1.0f / 255.0f;
  dlight.Diffuse.r = light->colorDiffuse[0] * kColorScale;
  dlight.Diffuse.g = light->colorDiffuse[1] * kColorScale;
  dlight.Diffuse.b = light->colorDiffuse[2] * kColorScale;
  dlight.Diffuse.a = 1.0f;

  switch (light->lightType) {
    case zLIGHT_TYPE_POINT:
      SetupPointLight(dlight, light);
      break;
    case zLIGHT_TYPE_SPOT:
      SetupSpotLight(dlight, light);
      break;
    case zLIGHT_TYPE_DIR:
      SetupDirectionalLight(dlight, light);
      break;
    default:
      return TRUE;
  }

  // NOTE: Do NOT set WORLD to identity here!
  // Gothic transforms light->directionLS by the same transform it uses for the object's
  // vertices. When Gothic calls SetLight, the current WORLD matrix is already correct.
  impl_->SetLight(index, &dlight);
  impl_->LightEnable(index, TRUE);

  // Clear ambient if this slot was previously ambient.
  if (s_ambient_light_index == index) {
    s_ambient_light_index.reset();
    impl_->SetRenderState(D3DRS_AMBIENT, 0);
  }

  return TRUE;
}

int zCRnd_D3D_DX9::GetMaterial(zCRenderer::zTMaterial& mat) {
  mat.diffuseRGBA = current_material_.diffuseRGBA;
  mat.ambientRGBA = current_material_.ambientRGBA;
  return 1;
}

int zCRnd_D3D_DX9::SetMaterial(const zCRenderer::zTMaterial& mat) {
  if (!impl_)
    return 0;

  // Store current material for GetMaterial
  current_material_.diffuseRGBA = mat.diffuseRGBA;
  current_material_.ambientRGBA = mat.ambientRGBA;

  D3DMATERIAL9 d3dMat{};

  // Gothic passes normalized 0-1 values in zTMaterial (unlike zCRenderLight which is 0-255)
  d3dMat.Diffuse.r = mat.diffuseRGBA[0];
  d3dMat.Diffuse.g = mat.diffuseRGBA[1];
  d3dMat.Diffuse.b = mat.diffuseRGBA[2];
  d3dMat.Diffuse.a = mat.diffuseRGBA[3];

  d3dMat.Ambient.r = mat.ambientRGBA[0];
  d3dMat.Ambient.g = mat.ambientRGBA[1];
  d3dMat.Ambient.b = mat.ambientRGBA[2];
  d3dMat.Ambient.a = mat.ambientRGBA[3];

  d3dMat.Specular.r = 0.0f;
  d3dMat.Specular.g = 0.0f;
  d3dMat.Specular.b = 0.0f;
  d3dMat.Specular.a = 0.0f;
  d3dMat.Emissive.r = 0.0f;
  d3dMat.Emissive.g = 0.0f;
  d3dMat.Emissive.b = 0.0f;
  d3dMat.Emissive.a = 0.0f;
  d3dMat.Power = 0.0f;

  impl_->SetMaterial(&d3dMat);

  return 1;
}

int zCRnd_D3D_DX9::SetTexture(unsigned long stage, zCTexture* texture) {
  if (!impl_ || GetSurfaceLost())
    return 0;

  // Update active_texture_ array for debug capture (this array is used by ActivateMaterial
  // for the DrawPoly path, but SetTexture is also called directly by zCRenderManager
  // for the vertex buffer path - we need to track it here too)
  if (stage < 4) {
    active_texture_[stage] = texture;
  }

  void* d3dTex = nullptr;
  bool hasAlpha = false;

  if (texture) {
    // We assume all textures are created by us and are zCTex_D3D9
    zCTex_D3D9* tex9 = static_cast<zCTex_D3D9*>(texture);
    if (tex9) {
      d3dTex = tex9->GetDX9Texture();
      hasAlpha = tex9->HasAlpha();
      if (!d3dTex) {
        zSTRING name = tex9->GetObjectName();
        SPDLOG_WARN("SetTexture: Texture '{}' at stage {} has no D3D9 texture", name.ToChar(), stage);
      }
    } else {
      SPDLOG_WARN("SetTexture: Texture at stage {} is not zCTex_D3D9", stage);
    }
  }

  impl_->SetTexture(stage, d3dTex);

  // Handle Alpha Testing for Stage 0 (Primary Texture)
  // This is critical for trees and other punch-through alpha objects
  if (stage == 0) {
    bool enableAlphaTest = false;

    // If texture has alpha channel AND we are not using alpha blending (opaque material),
    // then we must enable alpha testing to discard transparent pixels (leaves).
    // Also check for MAT_DEFAULT which implies no blending.
    if (alpha_blend_func_ == zRND_ALPHA_FUNC_TEST) {
      enableAlphaTest = true;
    } else if (hasAlpha && (alpha_blend_func_ == zRND_ALPHA_FUNC_NONE || alpha_blend_func_ == zRND_ALPHA_FUNC_MAT_DEFAULT)) {
      enableAlphaTest = true;
    }

    ApplyRenderState(D3DRS_ALPHATESTENABLE, enableAlphaTest ? kTrue32 : kFalse32);

    if (enableAlphaTest) {
      // Standard alpha test configuration for Gothic
      ApplyRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

      if (alpha_reference_ == 0) {
        SetAlphaReference(128);
      }
    }
  } else {
    // For other stages, we generally don't want alpha testing unless explicitly requested
    // But if we are in a multi-pass scenario, we might need to be careful.
    // For now, only Stage 0 controls the global alpha test state in this auto-logic.
  }

  return 1;
}

int zCRnd_D3D_DX9::SetTextureStageState(unsigned long stage, zTRnd_TextureStageState state, unsigned long value) {
  if (stage >= kMaxTextureStages || state >= zRND_TSS_COUNT) {
    SPDLOG_WARN("SetTextureStageState: invalid stage {} or state {}", stage, static_cast<unsigned long>(state));
    return 0;
  }

  // Texture stage state (D3DTSS_*)
  auto applyStageState = [&](unsigned long d3d_state, unsigned long new_value) -> int {
    if (tex_stage_state_cache_[stage][state] == new_value) {
      return 1;
    }
    tex_stage_state_cache_[stage][state] = new_value;
    if (impl_) {
      impl_->SetTextureStageState(stage, d3d_state, new_value);
    }
    return 1;
  };

  // Sampler state (D3DSAMP_*)
  auto applySamplerState = [&](SamplerSlot slot, unsigned long d3d_state, unsigned long new_value) -> int {
    const auto slot_index = static_cast<size_t>(slot);
    if (slot_index >= kSamplerStateCount || sampler_state_cache_[stage][slot_index] == new_value) {
      return (slot_index < kSamplerStateCount) ? 1 : 0;
    }
    sampler_state_cache_[stage][slot_index] = new_value;
    if (impl_) {
      impl_->SetSamplerState(stage, d3d_state, new_value);
    }
    return 1;
  };

  switch (state) {
    // Color operations
    case zRND_TSS_COLOROP:
      return applyStageState(D3DTSS_COLOROP, MapTextureOp(value));
    case zRND_TSS_COLORARG1:
      return applyStageState(D3DTSS_COLORARG1, MapTextureArg(value));
    case zRND_TSS_COLORARG2:
      return applyStageState(D3DTSS_COLORARG2, MapTextureArg(value));

    // Alpha operations
    case zRND_TSS_ALPHAOP:
      return applyStageState(D3DTSS_ALPHAOP, MapTextureOp(value));
    case zRND_TSS_ALPHAARG1:
      return applyStageState(D3DTSS_ALPHAARG1, MapTextureArg(value));
    case zRND_TSS_ALPHAARG2:
      return applyStageState(D3DTSS_ALPHAARG2, MapTextureArg(value));

    // Bump mapping
    case zRND_TSS_BUMPENVMAT00:
      return applyStageState(D3DTSS_BUMPENVMAT00, value);
    case zRND_TSS_BUMPENVMAT01:
      return applyStageState(D3DTSS_BUMPENVMAT01, value);
    case zRND_TSS_BUMPENVMAT10:
      return applyStageState(D3DTSS_BUMPENVMAT10, value);
    case zRND_TSS_BUMPENVMAT11:
      return applyStageState(D3DTSS_BUMPENVMAT11, value);
    case zRND_TSS_BUMPENVLSCALE:
      return applyStageState(D3DTSS_BUMPENVLSCALE, value);
    case zRND_TSS_BUMPENVLOFFSET:
      return applyStageState(D3DTSS_BUMPENVLOFFSET, value);

    // Texture coordinate and transform
    case zRND_TSS_TEXCOORDINDEX:
      return applyStageState(D3DTSS_TEXCOORDINDEX, value);
    case zRND_TSS_TEXTURETRANSFORMFLAGS:
      return applyStageState(D3DTSS_TEXTURETRANSFORMFLAGS, MapTextureTransformFlags(value));

    // Sampler states - address modes
    case zRND_TSS_ADDRESS:
      applySamplerState(kSamplerAddressU, D3DSAMP_ADDRESSU, value);
      return applySamplerState(kSamplerAddressV, D3DSAMP_ADDRESSV, value);
    case zRND_TSS_ADDRESSU:
      return applySamplerState(kSamplerAddressU, D3DSAMP_ADDRESSU, value);
    case zRND_TSS_ADDRESSV:
      return applySamplerState(kSamplerAddressV, D3DSAMP_ADDRESSV, value);
    case zRND_TSS_BORDERCOLOR:
      return applySamplerState(kSamplerBorderColor, D3DSAMP_BORDERCOLOR, value);

    // Sampler states - filtering
    case zRND_TSS_MAGFILTER:
      return applySamplerState(kSamplerMagFilter, D3DSAMP_MAGFILTER, value);
    case zRND_TSS_MINFILTER:
      return applySamplerState(kSamplerMinFilter, D3DSAMP_MINFILTER, value);
    case zRND_TSS_MIPFILTER:
      return applySamplerState(kSamplerMipFilter, D3DSAMP_MIPFILTER, value);
    case zRND_TSS_MIPMAPLODBIAS:
      return applySamplerState(kSamplerMipMapLodBias, D3DSAMP_MIPMAPLODBIAS, value);
    case zRND_TSS_MAXMIPLEVEL:
      return applySamplerState(kSamplerMaxMipLevel, D3DSAMP_MAXMIPLEVEL, value);
    case zRND_TSS_MAXANISOTROPY:
      return applySamplerState(kSamplerMaxAnisotropy, D3DSAMP_MAXANISOTROPY, value);

    default:
      SPDLOG_WARN("SetTextureStageState: unsupported state {}", static_cast<unsigned long>(state));
      return 0;
  }
}

// SetAlphaBlendFuncImmed - Configures alpha blending state for rendering.
// Called from DrawPolySimple, alpha poly Draw, etc.
int zCRnd_D3D_DX9::SetAlphaBlendFuncImmed(zTRnd_AlphaBlendFunc func) {
  const bool enable_alpha_test = (func == zRND_ALPHA_FUNC_TEST || func == zRND_ALPHA_FUNC_BLEND_TEST);
  const bool disable_blending = (func == zRND_ALPHA_FUNC_NONE);

  // Set blend enable state.
  ApplyRenderState(D3DRS_ALPHABLENDENABLE, disable_blending ? kFalse32 : kTrue32);

  // Set blend factors.
  switch (func) {
    case zRND_ALPHA_FUNC_TEST:
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
      break;
    case zRND_ALPHA_FUNC_BLEND:
    case zRND_ALPHA_FUNC_BLEND_TEST:
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      break;
    case zRND_ALPHA_FUNC_ADD:
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
      break;
    case zRND_ALPHA_FUNC_MUL:
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
      break;
    case zRND_ALPHA_FUNC_MUL2:
      ApplyRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
      ApplyRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
      break;
    case zRND_ALPHA_FUNC_NONE:
    default:
      break;  // No blend factors needed.
  }

  // Configure alpha test and related states.
  if (enable_alpha_test) {
    ApplyRenderState(D3DRS_DITHERENABLE, kFalse32);
    ApplyRenderState(D3DRS_ZWRITEENABLE, kTrue32);
    ApplyRenderState(D3DRS_ALPHATESTENABLE, kTrue32);
    ApplyRenderState(D3DRS_ALPHAREF, alpha_reference_);
  } else {
    ApplyRenderState(D3DRS_ALPHATESTENABLE, kFalse32);
    ApplyRenderState(D3DRS_DITHERENABLE, disable_blending && active_status_.dither ? kTrue32 : kFalse32);
    ApplyRenderState(D3DRS_ZWRITEENABLE, disable_blending && active_status_.zwrite ? kTrue32 : kFalse32);
    SetZBufferCompare(active_status_.zfunc);
  }

  alpha_blend_immed_ = 1;
  return 1;
}

int zCRnd_D3D_DX9::SetRenderState(zTRnd_RenderStateType state, unsigned long value) {
  unsigned long d3dState = 0;
  switch (state) {
    case zRND_RENDERSTATE_CLIPPING:
      d3dState = D3DRS_CLIPPING;
      break;
    case zRND_RENDERSTATE_TEXTUREFACTOR:
      d3dState = D3DRS_TEXTUREFACTOR;
      texture_factor_ = value;
      break;
    default:
      SPDLOG_WARN("SetRenderState: unsupported state {}", static_cast<unsigned long>(state));
      return 0;
  }

  return ApplyRenderState(d3dState, value) ? 1 : 0;
}

unsigned long zCRnd_D3D_DX9::GetRenderState(zTRnd_RenderStateType state) {
  switch (state) {
    case zRND_RENDERSTATE_CLIPPING:
      return render_state_cache_[D3DRS_CLIPPING];
    case zRND_RENDERSTATE_TEXTUREFACTOR:
      return render_state_cache_[D3DRS_TEXTUREFACTOR];
  }
  return 0;
}

bool zCRnd_D3D_DX9::ApplyRenderState(unsigned long state, unsigned long value) {
  const size_t maxStates = sizeof(render_state_cache_) / sizeof(render_state_cache_[0]);
  if (state >= maxStates) {
    return false;
  }

  if (render_state_cache_[state] == value) {
    return true;
  }

  render_state_cache_[state] = value;

  if (impl_) {
    impl_->SetRenderState(state, value);

    if (state == D3DRS_ALPHATESTENABLE && value == kTrue32) {
      if (render_state_cache_[D3DRS_ALPHAREF] != alpha_reference_) {
        render_state_cache_[D3DRS_ALPHAREF] = alpha_reference_;
        impl_->SetRenderState(D3DRS_ALPHAREF, alpha_reference_);
      }

      constexpr unsigned long kAlphaFunc = D3DCMP_GREATEREQUAL;
      if (render_state_cache_[D3DRS_ALPHAFUNC] != kAlphaFunc) {
        render_state_cache_[D3DRS_ALPHAFUNC] = kAlphaFunc;
        impl_->SetRenderState(D3DRS_ALPHAFUNC, kAlphaFunc);
      }
    }

    if (state == D3DRS_FOGENABLE && value == kFalse32) {
      auto invalidate = [this, maxStates](unsigned long target) {
        if (target < maxStates) {
          render_state_cache_[target] = kCacheInvalidSentinel;
        }
      };

      if (fog_manager_.IsRadialEnabled()) {
        invalidate(D3DRS_FOGVERTEXMODE);
        invalidate(D3DRS_RANGEFOGENABLE);
      } else {
        invalidate(D3DRS_FOGTABLEMODE);
      }

      invalidate(D3DRS_FOGCOLOR);
      invalidate(D3DRS_FOGSTART);
      invalidate(D3DRS_FOGEND);
    }
  }

  return true;
}

void zCRnd_D3D_DX9::AddAlphaSortObject(zCRndAlphaSortObject* obj) {
  if (!obj)
    return;

  // Get the Z-value for depth sorting (distance from camera)
  float zvalue = obj->zvalue;  // Direct member access

  if (zvalue == 0.0f) {
    SPDLOG_WARN("AddAlphaSortObject: Skipping object with zvalue=0");
    return;
  }

  // Map Z-value to bucket index using bucketSize (calculated in BeginFrame)
  int bucket = static_cast<int>(std::floor(bucket_size_ * zvalue));
  if (bucket >= kMaxBuckets)
    bucket = kMaxBuckets - 1;
  if (bucket < 0)
    bucket = 0;

  SPDLOG_TRACE("AddAlphaSortObject: z={:.2f} bucketSize={:.4f} bucket={} type={}", zvalue, bucket_size_, bucket, (void*)obj);

  // If bucket is empty, just insert
  if (alpha_sort_bucket_[bucket] == nullptr) {
    obj->nextSortObject = nullptr;  // Direct member access
    alpha_sort_bucket_[bucket] = obj;
    SPDLOG_TRACE("  -> Inserted as first entry in empty bucket {}", bucket);
    return;
  }

  // Larger Z = farther from camera. For painter's algorithm (back-to-front),
  // we render larger Z first. Within each bucket, sort so larger Z is at head.

  // If this object is farther (larger Z) than the first entry, insert at head
  if (alpha_sort_bucket_[bucket]->zvalue <= zvalue) {
    obj->nextSortObject = alpha_sort_bucket_[bucket];  // Direct member access
    alpha_sort_bucket_[bucket] = obj;
    SPDLOG_TRACE("  -> Inserted at head of bucket {} (farther than {:.2f})", bucket, alpha_sort_bucket_[bucket]->nextSortObject->zvalue);
    return;
  }

  // Otherwise, traverse the bucket to find the correct position
  // Sort order within bucket: far to near (descending Z)
  zCRndAlphaSortObject* entry = alpha_sort_bucket_[bucket];
  zCRndAlphaSortObject* nextEntry = entry->nextSortObject;  // Direct member access
  int traverseCount = 1;

  while (true) {
    if (nextEntry == nullptr)
      break;  // End of list
    if (nextEntry->zvalue <= zvalue)
      break;  // Found insertion point

    entry = nextEntry;
    nextEntry = entry->nextSortObject;  // Direct member access
    traverseCount++;
  }

  // Insert between entry and nextEntry
  entry->nextSortObject = obj;      // Direct member access
  obj->nextSortObject = nextEntry;  // Direct member access
  SPDLOG_TRACE("  -> Inserted in bucket {} after {} traversals (between {:.2f} and {:.2f})", bucket, traverseCount, entry->zvalue,
               nextEntry ? nextEntry->zvalue : -1.0f);
}

// ApplyAlphaBlendState - Configures texture stage and blend state for alpha rendering.
//
// Consolidates the blend function setup logic for both textured and non-textured
// alpha polygons. Each blend mode requires specific texture stage operations:
//
// - BLEND: Standard alpha blending (src*srcAlpha + dest*(1-srcAlpha))
// - ADD:   Additive blending for glow/fire effects (src*srcAlpha + dest)
// - MUL:   Multiplicative for shadows/darkening (src*destColor)
// - MUL2:  2x multiply for enhanced contrast (src*destColor + dest*srcColor)
//
// Returns true if fog should be disabled for this blend mode (ADD, MUL).
//
bool zCRnd_D3D_DX9::ApplyAlphaBlendState(gmp::renderer::AlphaBlendFunc blend_func, bool has_texture, bool texture_has_alpha) {
  using namespace gmp::renderer;

  bool disable_fog = false;

  // For non-textured polys, always use diffuse color only
  if (!has_texture) {
    SetTextureStageState(0, zRND_TSS_ALPHAOP, zRND_TOP_SELECTARG2);
    SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_SELECTARG2);
  }

  switch (blend_func) {
    case AlphaBlendFunc::kAdd:
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_ALPHAOP, texture_has_alpha ? zRND_TOP_MODULATE : zRND_TOP_SELECTARG2);
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_ADD);
      disable_fog = true;
      break;

    case AlphaBlendFunc::kMul:
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_SELECTARG1);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_MUL);
      disable_fog = true;
      break;

    case AlphaBlendFunc::kMul2:
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_SELECTARG1);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_MUL2);
      break;

    case AlphaBlendFunc::kBlend:
    default:
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_ALPHAOP, texture_has_alpha ? zRND_TOP_MODULATE : zRND_TOP_SELECTARG2);
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_BLEND);
      break;
  }

  return disable_fog;
}

// ----------------------------------------------------------------------------
// Batched Alpha Polygon Rendering
// ----------------------------------------------------------------------------
// These methods implement batched rendering for alpha polygons to reduce
// draw call overhead. Polygons with the same render state (texture, blend mode)
// are accumulated and rendered in a single draw call.
// ----------------------------------------------------------------------------

// SetupAlphaRenderState - Configures D3D9 state for a batch of alpha polygons.
//
// This sets up all the render state needed for the current batch, including:
// - Texture and sampler state
// - Blend function
// - Z buffer configuration
// - Texture stage operations
//
void zCRnd_D3D_DX9::SetupAlphaRenderState(const gmp::renderer::AlphaRenderStateKey& state) {
  using namespace gmp::renderer;

  // Setup common alpha state
  ResetMultiTexturing();
  ApplyRenderState(D3DRS_ALPHATESTENABLE, kFalse32);
  ApplyRenderState(D3DRS_ZWRITEENABLE, kFalse32);

  SetTextureStageState(0, zRND_TSS_ALPHAARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_ALPHAARG2, zRND_TA_DIFFUSE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_DIFFUSE);
  SetTextureStageState(0, zRND_TSS_TEXTURETRANSFORMFLAGS, zRND_TTF_DISABLE);
  SetTextureStageState(0, zRND_TSS_TEXCOORDINDEX, 0);

  // Set Z function and bias
  SetZBufferCompare(static_cast<zTRnd_ZBufferCmp>(state.z_func));
  SetZBias(state.z_bias);

  // Configure texture and sampler state
  const bool has_texture = (state.texture != nullptr);
  if (has_texture) {
    const DWORD address_mode = state.texture_wrap ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
    impl_->SetSamplerState(0, D3DSAMP_ADDRESSU, address_mode);
    impl_->SetSamplerState(0, D3DSAMP_ADDRESSV, address_mode);
    SetTexture(0, static_cast<zCTexture*>(state.texture));
  } else {
    SetTexture(0, nullptr);
  }

  // Apply blend-specific texture stage and blend state
  // Return value (disable_fog) not needed - fog is disabled at pass level
  (void)ApplyAlphaBlendState(state.blend_func, has_texture, state.texture_has_alpha);
}

// FlushAlphaBatch - Renders the current batch of alpha polygons.
//
// Gets batch data from the batcher, sets up render state, and issues
// a single batched draw call for all polygons in the batch.
//
void zCRnd_D3D_DX9::FlushAlphaBatch() {
  using namespace gmp::renderer;

  if (!alpha_batcher_.HasPendingGeometry()) {
    return;
  }

  const AlphaVertex* vertices = nullptr;
  const uint16_t* indices = nullptr;
  AlphaRenderStateKey state;

  const size_t index_count = alpha_batcher_.GetBatchData(vertices, indices, state);
  if (index_count == 0 || vertices == nullptr || indices == nullptr) {
    alpha_batcher_.MarkBatchRendered();
    return;
  }

  // Setup render state for this batch
  SetupAlphaRenderState(state);

  // Count vertices (we need this for the draw call)
  // The batcher tracks this internally but doesn't expose it directly
  // We can calculate it from the max index
  size_t vertex_count = 0;
  for (size_t i = 0; i < index_count; ++i) {
    if (indices[i] >= vertex_count) {
      vertex_count = indices[i] + 1;
    }
  }

  // Draw the batch
  static_assert(sizeof(AlphaVertex) == sizeof(VertexRHW), "AlphaVertex must match VertexRHW size");
  impl_->DrawAlphaBatch(reinterpret_cast<const VertexRHW*>(vertices), vertex_count, indices, index_count);

  alpha_batcher_.MarkBatchRendered();
}

// RenderAlphaPolyBatched - Submits a polygon to the batcher.
//
// If the polygon can be added to the current batch (same render state),
// it's accumulated. If not, the current batch is flushed first.
//
void zCRnd_D3D_DX9::RenderAlphaPolyBatched(const gmp::renderer::QueuedAlphaPoly& poly) {
  // Try to add to current batch
  if (!alpha_batcher_.Submit(poly)) {
    // Batch is full or state changed - flush current batch
    FlushAlphaBatch();
    // Now submit the poly (starts a new batch)
    alpha_batcher_.Submit(poly);
  }
}

// DrawQueuedAlphaPoly - renders a QueuedAlphaPoly using D3D9
// Note: This function is still used for immediate alpha polys (FlushAlphaPolys).
// For the sorted alpha pass, we use the batched rendering path instead.
void zCRnd_D3D_DX9::DrawQueuedAlphaPoly(const gmp::renderer::QueuedAlphaPoly* ap) {
  using namespace gmp::renderer;

  SPDLOG_DEBUG("DrawQueuedAlphaPoly: ENTRY ap={} impl_={} surface_lost_={}", static_cast<const void*>(ap), static_cast<void*>(impl_.get()),
               GetSurfaceLost());

  if (!ap || ap->vert_count < 3 || !impl_ || GetSurfaceLost()) {
    SPDLOG_DEBUG("DrawQueuedAlphaPoly: EARLY EXIT ap={} vert_count={} impl_={} surface_lost_={}", static_cast<const void*>(ap),
                 ap ? ap->vert_count : -1, static_cast<void*>(impl_.get()), GetSurfaceLost());
    return;
  }

  // Setup render state from the poly's state key
  AlphaRenderStateKey state = AlphaRenderStateKey::FromPoly(*ap);
  SetupAlphaRenderState(state);

  // Draw the polygon
  static_assert(sizeof(AlphaVertex) == sizeof(VertexRHW), "AlphaVertex must match VertexRHW size");
  impl_->DrawTriangleFan(reinterpret_cast<const VertexRHW*>(ap->verts.data()), ap->vert_count);
}

void zCRnd_D3D_DX9::RenderAlphaSortList() {
  using namespace gmp::renderer;

  // Skip rendering during device reset.
  if (GetSurfaceLost()) {
    alpha_poly_queue_.Reset();
    immediate_alpha_poly_queue_.Reset();
    num_alpha_polys_ = 0;
    for (auto& bucket : alpha_sort_bucket_) {
      bucket = nullptr;
    }
    return;
  }

  // Count engine objects (VOBs etc.) in buckets.
  int total_vob_objects = 0;
  for (const auto* bucket : alpha_sort_bucket_) {
    for (auto* obj = bucket; obj != nullptr; obj = obj->nextSortObject) {
      ++total_vob_objects;
    }
  }

  const int queued_poly_count = alpha_poly_queue_.GetQueuedCount();
  if (total_vob_objects == 0 && queued_poly_count == 0) {
    return;
  }

  // RAII guard ensures opaque render state is restored even on early exit
  ScopedAlphaRenderPass alpha_pass_guard(this);

  // Temporarily disable radial fog for entire alpha pass (more efficient than per-poly)
  ScopedRadialFogDisable fog_guard(this, fog_manager_);

  SetTexture(0, nullptr);

  int draw_index = 0;

  // Track whether we just rendered an alpha sort object (need state reset before alpha poly)
  bool last_was_alpha_sort_object = false;

  // Start batched alpha rendering
  alpha_batcher_.Begin();

  // Interleave alpha sort objects and alpha polys by bucket for correct depth ordering.
  // Both systems use the same bucket count and Z-to-bucket mapping, so we can iterate
  // through buckets from far to near (high to low index) and render both types in each bucket.
  // This ensures proper back-to-front ordering even when fog (alpha sort object) is far
  // and particle effects (alpha poly) are near.
  for (int bucket_idx = kMaxBuckets - 1; bucket_idx >= 0; --bucket_idx) {
    // First render alpha sort objects in this bucket (farther objects first within bucket)
    while (alpha_sort_bucket_[bucket_idx] != nullptr) {
      // Flush any pending alpha poly batch before rendering alpha sort object
      FlushAlphaBatch();

      auto* alpha_object = alpha_sort_bucket_[bucket_idx];
      alpha_sort_bucket_[bucket_idx] = alpha_object->nextSortObject;

      alpha_object->Draw(draw_index);
      ++draw_index;
      last_was_alpha_sort_object = true;
    }

    // Then render alpha polys in this bucket using batching
    QueuedAlphaPoly* poly = alpha_poly_queue_.GetBucketHead(bucket_idx);
    if (poly != nullptr) {
      // Reset state after alpha sort object rendering before switching to alpha polys
      if (last_was_alpha_sort_object) {
        ResetStateAfterAlphaSortObjects();
        last_was_alpha_sort_object = false;
      }

      // Process all polys in this bucket through the batcher
      while (poly != nullptr) {
        RenderAlphaPolyBatched(*poly);
        ++draw_index;
        poly = poly->next;
      }
    }
  }

  // Flush any remaining batched geometry
  FlushAlphaBatch();

  // Finalize batching and log stats (End returns bool for final batch, which we already flushed)
  (void)alpha_batcher_.End();
  const auto& stats = alpha_batcher_.GetStats();
  if (stats.polys_submitted > 0) {
    // Log every 60 frames to avoid spam (roughly once per second at 60fps)
    static int frame_counter = 0;
    if (++frame_counter >= 60) {
      SPDLOG_DEBUG("Alpha batch: {} polys -> {} batches ({} draw calls saved, {:.1f}% reduction)", stats.polys_submitted, stats.batches_flushed,
                   stats.draw_calls_saved, stats.GetBatchEfficiency() * 100.0f);
      frame_counter = 0;
    }
  }
  alpha_batcher_.ResetStats();

  // Clear the alpha poly queue (buckets are already cleared in the loop above)
  alpha_poly_queue_.Reset();

  num_alpha_polys_ = 0;
}

// RestoreOpaqueRenderState - Restores render state for opaque geometry after alpha pass.
//
// Called by ScopedAlphaRenderPass destructor to reset render state to defaults
// suitable for subsequent opaque geometry rendering.
void zCRnd_D3D_DX9::RestoreOpaqueRenderState() {
  // Restore depth buffer state
  ApplyRenderState(D3DRS_ZWRITEENABLE, z_buffer_write_enabled_ ? kTrue32 : kFalse32);
  SetZBufferCompare(z_buffer_cmp_);

  // Restore blend and dither state
  ApplyRenderState(D3DRS_DITHERENABLE, dither_enabled_ ? kTrue32 : kFalse32);
  ApplyRenderState(D3DRS_ALPHABLENDENABLE, kFalse32);
  ApplyRenderState(D3DRS_ALPHATESTENABLE, kFalse32);

  // Restore texture stage states
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_CURRENT);

  // Restore texture address mode
  const auto address_mode = texture_wrap_enabled_ ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSU, address_mode);
  impl_->SetSamplerState(0, D3DSAMP_ADDRESSV, address_mode);

  // Clear texture and material state
  SetTexture(0, nullptr);
  active_material_ = nullptr;
}

// ResetStateAfterAlphaSortObjects - Resets render state after alpha sort object rendering.
//
// Called after rendering alpha sort objects (like water) to prepare for
// subsequent alpha poly rendering (like fire particles). The Draw() calls may have
// changed texture, blend state, or other render state that needs to be reset.
void zCRnd_D3D_DX9::ResetStateAfterAlphaSortObjects() {
  SetTexture(0, nullptr);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  ApplyRenderState(D3DRS_CLIPPING, kFalse32);
  ApplyRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
}

// DrawVertexBuffer - Primary T&L rendering path for batched world geometry.
//
// This is the main rendering function for world geometry when T&L hardware is available.
// Gothic's zCRenderManager batches polygons via PackVB() and renders them through this
// function. Since we enforce T&L hardware support, this is the dominant path for static
// world rendering.
//
// Vertex types handled:
// - zVBUFFER_VERTTYPE_UT_UL: Untransformed, Unlit - uses hardware T&L for lighting
// - zVBUFFER_VERTTYPE_UT_L: Untransformed, Lit - pre-lit vertex colors, T&L for transform
// - zVBUFFER_VERTTYPE_T_L: Transformed, Lit - already in screen space (2D elements)
int zCRnd_D3D_DX9::DrawVertexBuffer(zCVertexBuffer* vertex_buffer, int first_vert, int num_vert, unsigned short* index_list,
                                    unsigned long num_index) {
  if (vertex_buffer == nullptr || impl_ == nullptr) {
    return 0;
  }

  // Block rendering during device reset.
  if (GetSurfaceLost()) {
    SPDLOG_DEBUG("DrawVertexBuffer: Skipping due to surface lost");
    return 0;
  }

  EnsureSceneBegun();

  // If no vertices specified, nothing to draw.
  if (num_vert == 0) {
    return 1;
  }

  // Negative num_vert means draw entire buffer.
  if (num_vert < 0) {
    num_vert = vertex_buffer->numVertex;
  }

  auto* vb_d3d = static_cast<zCVertexBuffer_D3D9*>(vertex_buffer);
  if (vb_d3d == nullptr) {
    return 0;
  }

  IDirect3DVertexBuffer9* handle = vb_d3d->GetHandle();
  if (handle == nullptr) {
    SPDLOG_WARN("DrawVertexBuffer: Vertex buffer has no D3D handle");
    return 0;
  }

  D3DPRIMITIVETYPE prim_type_d3d;
  if (!ConvertPrimitiveType(vb_d3d->GetPrimitiveType(), prim_type_d3d)) {
    SPDLOG_WARN("DrawVertexBuffer: Unsupported primitive type {}", static_cast<int>(vb_d3d->GetPrimitiveType()));
    return 0;
  }

  // Index list selection:
  // - zCRenderManager always provides indices (modern path, 99% of calls)
  const unsigned long index_list_size = (num_index > 0) ? num_index : vb_d3d->GetIndexListSize();
  unsigned short* const index_list_ptr = (num_index > 0) ? index_list : vb_d3d->GetIndexListPtr();

  // Configure render state based on vertex type.
  int lighting_on = 0;
  int clipping_on = 0;
  unsigned long culling = D3DCULL_NONE;

  switch (vb_d3d->GetVertexType()) {
    case zVBUFFER_VERTTYPE_UT_UL:
      // Untransformed, Unlit - needs hardware lighting.
      lighting_on = 1;
      clipping_on = 1;
      culling = D3DCULL_CW;
      break;
    case zVBUFFER_VERTTYPE_UT_L:
      // Untransformed, Lit - pre-lit vertex colors.
      lighting_on = 0;
      clipping_on = 1;
      culling = D3DCULL_CW;
      break;
    case zVBUFFER_VERTTYPE_T_L:
    default:
      // Transformed, Lit - already in screen space.
      lighting_on = 0;
      clipping_on = 0;
      culling = D3DCULL_NONE;
      break;
  }

  ApplyRenderState(D3DRS_LIGHTING, lighting_on);
  ApplyRenderState(D3DRS_CLIPPING, clipping_on);
  ApplyRenderState(D3DRS_CULLMODE, culling);

  if (active_texture_[1] != nullptr) {
    const auto stage1_color_op = tex_stage_state_cache_[1][zRND_TSS_COLOROP];
    if (stage1_color_op == D3DTOP_SELECTARG1) {
      SetTextureStageState(1, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
    }
  }

  // Execute draw call based on poly draw mode.
  int result = 1;
  const auto fvf = vb_d3d->GetD3DFVF();
  const auto stride = vb_d3d->GetVertexStride();

  switch (poly_draw_mode_) {
    case zRND_DRAW_WIRE:
    case zRND_DRAW_FLAT:
      // Skip wire/flat modes - not supported.
      break;

    case zRND_DRAW_MATERIAL_WIRE:
      // Draw material, skip wire overlay.
      if (!impl_->DrawVertexBuffer(handle, fvf, stride, prim_type_d3d, first_vert, num_vert, index_list_ptr, index_list_size)) {
        result = 0;
      }
      ApplyRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
      break;

    default:
      if (!impl_->DrawVertexBuffer(handle, fvf, stride, prim_type_d3d, first_vert, num_vert, index_list_ptr, index_list_size)) {
        SPDLOG_WARN("DrawVertexBuffer: Draw call failed");
        result = 0;
      }
  }

  ApplyRenderState(D3DRS_LIGHTING, 0);
  return result;
}

zCVertexBuffer* zCRnd_D3D_DX9::CreateVertexBuffer() {
  return new zCVertexBuffer_D3D9();
}

zCRenderer* __stdcall CreateDX9Renderer() {
  SPDLOG_TRACE("CreateDX9Renderer called - Injecting DX9 Renderer");
  return new zCRnd_D3D_DX9();
}

void __fastcall ConstructDX9Renderer(void* mem) {
  SPDLOG_TRACE("ConstructDX9Renderer called - Constructing DX9 Renderer in place at {}", mem);
  if (mem) {
    ::new (mem) zCRnd_D3D_DX9();
  }
}

IDirect3DDevice9* zCRnd_D3D_DX9::GetDevice() const {
  return impl_ ? impl_->GetDevice() : nullptr;
}

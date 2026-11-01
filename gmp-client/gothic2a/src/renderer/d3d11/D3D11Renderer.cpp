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

#include "D3D11Renderer.h"

#include <d3d11.h>
#include <dxgi.h>
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

#include "D3D11DisplayModes.h"
#include "D3D11RendererImpl.h"
#include "D3D11Texture.h"
#include "D3D11VertexBuffer.h"
#include "NativeRenderState.h"
#include "config.h"
#include "renderer/d3d11/ShaderSemanticBridge.h"

using gmp::renderer::d3d11::MaterialCB;
using gmp::renderer::d3d11::VertexRHW;

#include "ZenGin/Gothic_II_Addon/API/z3d.h"
#include "ZenGin/Gothic_II_Addon/API/zRenderer.h"
#include "ZenGin/Gothic_II_Addon/API/zTexConvert.h"
#include "ZenGin/Gothic_II_Addon/API/zVertexTransform.h"

namespace {

using gmp::renderer::d3d11::Light;
using gmp::renderer::d3d11::LightType;

// Gothic's renderer interface is D3D9-shaped (state IDs + numeric values).
// Keep those numeric values, but avoid D3D9-named macros in this translation unit.
enum class LegacyRenderState : unsigned long {
  kZEnable = 7,
  kFillMode = 8,
  kZWriteEnable = 14,
  kAlphaTestEnable = 15,
  kSrcBlend = 19,
  kDestBlend = 20,
  kCullMode = 22,
  kZFunc = 23,
  kAlphaRef = 24,
  kAlphaFunc = 25,
  kDitherEnable = 26,
  kAlphaBlendEnable = 27,
  kFogEnable = 28,
  kTextureFactor = 60,
  kClipping = 136,
  kLighting = 137,
  kAmbient = 139,
  kFogVertexMode = 140,
  kFogColor = 34,
  kFogTableMode = 35,
  kFogStart = 36,
  kFogEnd = 37,
  kRangeFogEnable = 48,
};

enum class LegacyBlend : unsigned long {
  kZero = 1,
  kOne = 2,
  kSrcColor = 3,
  kInvSrcColor = 4,
  kSrcAlpha = 5,
  kInvSrcAlpha = 6,
  kDestAlpha = 7,
  kInvDestAlpha = 8,
  kDestColor = 9,
  kInvDestColor = 10,
};

enum class LegacyCompareFunc : unsigned long {
  kNever = 1,
  kLess = 2,
  kEqual = 3,
  kLessEqual = 4,
  kGreater = 5,
  kNotEqual = 6,
  kGreaterEqual = 7,
  kAlways = 8,
};

enum class LegacyFillMode : unsigned long {
  kPoint = 1,
  kWireframe = 2,
  kSolid = 3,
};

enum class LegacyCullMode : unsigned long {
  kNone = 1,
  kCW = 2,
  kCCW = 3,
};

enum class LegacyZEnable : unsigned long {
  kFalse = 0,
  kTrue = 1,
};

constexpr unsigned long kColorWriteEnableRed = (1UL << 0);
constexpr unsigned long kColorWriteEnableGreen = (1UL << 1);
constexpr unsigned long kColorWriteEnableBlue = (1UL << 2);
constexpr unsigned long kColorWriteEnableAlpha = (1UL << 3);

constexpr unsigned long ToUL(LegacyRenderState v) {
  return static_cast<unsigned long>(v);
}
constexpr unsigned long ToUL(LegacyBlend v) {
  return static_cast<unsigned long>(v);
}
constexpr unsigned long ToUL(LegacyCompareFunc v) {
  return static_cast<unsigned long>(v);
}
constexpr unsigned long ToUL(LegacyFillMode v) {
  return static_cast<unsigned long>(v);
}
constexpr unsigned long ToUL(LegacyCullMode v) {
  return static_cast<unsigned long>(v);
}
constexpr unsigned long ToUL(LegacyZEnable v) {
  return static_cast<unsigned long>(v);
}

constexpr unsigned long kTrue32 = 1;
constexpr unsigned long kFalse32 = 0;
constexpr int kMaxBuckets = 512;

// Mask to extract RGB components from a 32-bit ARGB color (discards alpha).
constexpr unsigned long kRgbMask = 0x00FFFFFF;

// D3D11 doesn't use fixed-function texture operations - these are handled in shaders
// Keep the enums for compatibility but they won't be directly mapped to D3D state

// Convert Gothic primitive type to D3D11 topology
bool ConvertPrimitiveType(zTVBufferPrimitiveType inType, D3D11_PRIMITIVE_TOPOLOGY& outTopology) {
  switch (inType) {
    case zVBUFFER_PT_POINTLIST:
      outTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
      return true;
    case zVBUFFER_PT_LINELIST:
      outTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
      return true;
    case zVBUFFER_PT_LINESTRIP:
      outTopology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
      return true;
    case zVBUFFER_PT_TRIANGLELIST:
      outTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      return true;
    case zVBUFFER_PT_TRIANGLESTRIP:
      outTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
      return true;
    case zVBUFFER_PT_TRIANGLEFAN:
      // D3D11 doesn't support triangle fans - would need to convert to triangle list
      SPDLOG_WARN("Triangle fan topology not supported in D3D11");
      return false;
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

// Scale factors for converting Gothic's integer zbias (0-15) to D3D11 depth bias.
// D3D11 uses two separate bias values:
// - DEPTHBIAS: constant offset in depth buffer units (needs to be small)
// - SLOPESCALEDEPTHBIAS: scales with polygon slope (needs to be larger for angled surfaces)
// These values control how zbias works at all distances.
constexpr float kDepthBiasScale = -0.000005f;   // Constant depth offset (negative = closer to camera)
constexpr float kSlopeScaledBiasScale = -1.0f;  // Slope-dependent offset (negative = closer to camera)

// Linear attenuation coefficient for point lights.
// Controls how quickly light intensity falls off with distance.
constexpr float kPointLightLinearAttenuation = 0.009f;

constexpr gmp::renderer::d3d11::AddressMode ToAddressMode(unsigned long address) {
  // Gothic uses D3D9-style numeric values for addressing.
  // D3DTADDRESS_WRAP=1, MIRROR=2, CLAMP=3, BORDER=4 (fall back to clamp).
  switch (address) {
    case 1:
      return gmp::renderer::d3d11::AddressMode::kWrap;
    case 2:
      return gmp::renderer::d3d11::AddressMode::kMirror;
    default:
      return gmp::renderer::d3d11::AddressMode::kClamp;
  }
}

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
  ScopedRadialFogDisable(zCRnd_D3D_DX11* renderer, gmp::renderer::d3d11::D3D11FogManager& fog_manager)
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
  zCRnd_D3D_DX11* renderer_;
  gmp::renderer::d3d11::D3D11FogManager& fog_manager_;
  bool fog_was_enabled_;
  bool radial_was_enabled_;
};

// RAII guard for alpha rendering pass state management.
// On construction: Prepares state for alpha polygon rendering.
// On destruction: Restores state for subsequent opaque geometry rendering.
class ScopedAlphaRenderPass {
public:
  explicit ScopedAlphaRenderPass(zCRnd_D3D_DX11* renderer)
      : renderer_(renderer),
        saved_zwrite_(renderer ? renderer->GetZBufferWriteEnabled() : 1),
        saved_zcmp_(renderer ? renderer->GetZBufferCompare() : zRND_ZBUFFER_CMP_LESS_EQUAL) {
    // No setup needed - individual DrawQueuedAlphaPoly calls configure per-poly state
  }

  ~ScopedAlphaRenderPass() {
    // Alpha sort object Draw() code can freely change depth state. That state must not
    // leak out of the alpha pass because later immediate effects (lens flares/decals)
    // snapshot the current Z compare into their alpha poly state.
    if (renderer_) {
      renderer_->SetZBufferWriteEnabled(saved_zwrite_);
      renderer_->SetZBufferCompare(saved_zcmp_);
    }
    renderer_->RestoreOpaqueRenderState();
  }

  // Non-copyable
  ScopedAlphaRenderPass(const ScopedAlphaRenderPass&) = delete;
  ScopedAlphaRenderPass& operator=(const ScopedAlphaRenderPass&) = delete;

private:
  zCRnd_D3D_DX11* renderer_;
  int saved_zwrite_;
  zTRnd_ZBufferCmp saved_zcmp_;
};

}  // namespace

zCRnd_D3D_DX11::zCRnd_D3D_DX11() {
  impl_ = std::make_unique<gmp::renderer::d3d11::D3D11RendererImpl>();

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

zCRnd_D3D_DX11::~zCRnd_D3D_DX11() {
  if (impl_) {
    impl_->Cleanup();
    impl_.reset();
  }
}

void zCRnd_D3D_DX11::BeginFrame() {
  FlushBatch();

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
  // RHW Depth Mapping (computed once per frame)
  // ----------------------------------------------------------------------------
  // Gothic can change the projection matrix mid-frame (notably the near plane).
  // RHW vertices (XYZRHW-like) were already transformed using the original
  // projection state, so these depth constants must stay consistent with that.
  //
  // Invariant: compute RHW depth constants from BeginFrame near/far and do not
  // update them from SetTransform(PROJECTION).
  //
  // Formula: z_ndc = offset + scale * rhw, where rhw = 1/z_eye
  // With: offset = far/(far-near), scale = -far*near/(far-near)
  // This produces the standard D3D perspective depth distribution.
  //
  // Implementation: z_ndc = rhw_z_proj_offset_ + rhw_z_proj_scale_ * rhw
  // Where rhw = 1/z_eye (from zCVertexTransform::vertCamSpaceZInv).
  const float denom = z_max_from_engine_ - z_min_from_engine_;
  if (denom <= std::numeric_limits<float>::epsilon()) {
    SPDLOG_WARN("BeginFrame: Invalid clip range near={} far={} (forcing identity scaling)", z_min_from_engine_, z_max_from_engine_);
    rhw_z_proj_offset_ = 1.0f;
    rhw_z_proj_scale_ = 0.0f;
  } else {
    rhw_z_proj_offset_ = z_max_from_engine_ / denom;
    rhw_z_proj_scale_ = -z_max_from_engine_ * z_min_from_engine_ / denom;
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

void zCRnd_D3D_DX11::EndFrame() {
  FlushBatch();

  // Call backend EndFrame to draw test triangle on top of everything
  if (impl_) {
    impl_->EndFrame();
  }

  // EndFrame just cleans up frame state, does NOT call EndScene or Present
}

void zCRnd_D3D_DX11::FlushPolys() {
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
          // Get the D3D11 texture pointer for batch state tracking
          auto* tex9 = static_cast<zCTex_D3D11*>(tex);
          if (tex9) {
            d3dTex = tex9->GetSRV();
          }
        }
      }

      // Apply stage0 addressing explicitly for this RHW draw.
      // Ensures the sampler state matches Gothic's intent (from SetTextureStageState)
      // rather than using stale state left by previous draws.
      if (impl_) {
        unsigned long addr_u = tex_stage_state_cache_[0][zRND_TSS_ADDRESSU];
        unsigned long addr_v = tex_stage_state_cache_[0][zRND_TSS_ADDRESSV];
        const unsigned long addr_both = tex_stage_state_cache_[0][zRND_TSS_ADDRESS];
        if (addr_u == kCacheInvalidSentinel) {
          addr_u = (addr_both != kCacheInvalidSentinel) ? addr_both : (texture_wrap_enabled_ ? 1UL : 3UL);
        }
        if (addr_v == kCacheInvalidSentinel) {
          addr_v = (addr_both != kCacheInvalidSentinel) ? addr_both : (texture_wrap_enabled_ ? 1UL : 3UL);
        }

        impl_->SetSamplerAddressing(0, ToAddressMode(addr_u), ToAddressMode(addr_v));
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
        // Use RHW-based depth with stable BeginFrame constants.
        // ALWAYS clamp to [0,1]: Issue 13 fix - out-of-range depth causes D3D11 clip rejection.
        v.z = std::clamp(rhw_z_proj_offset_ + rhw_z_proj_scale_ * v.rhw, 0.0f, 1.0f);
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

void zCRnd_D3D_DX11::ResetMultiTexturing() {
  SetTextureStageState(1, zRND_TSS_COLOROP, zRND_TOP_DISABLE);
  SetTextureStageState(1, zRND_TSS_ALPHAOP, zRND_TOP_DISABLE);
  SetTextureStageState(1, zRND_TSS_TEXCOORDINDEX, 0);
}

void zCRnd_D3D_DX11::InvalidateStateCache() {
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

bool zCRnd_D3D_DX11::ActivateMaterial(zCMaterial* material) {
  // Get the animated texture first - we need this for the cache check
  zCTexture* tex = material->GetAniTexture();

  // Match D3D9 behavior: cache check uses material pointer and checks if texture matches
  // We must compare against the animated texture we're about to use, not material->texture
  if (material == active_material_ && tex == active_texture_[0]) {
    return true;
  }

  FlushBatch();

  active_material_ = material;
  zTRnd_AlphaBlendFunc alpha_func = material->rndAlphaBlendFunc;
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

  // Alpha blending requested - defer to alpha system (matches D3D9)
  // Just return true; the polygon will be queued via QueueAlphaPoly
  if (alpha_func != zRND_ALPHA_FUNC_NONE) {
    return true;
  }

  // Texture with alpha channel - use alpha testing for punch-through
  const unsigned long alpha_test_state = render_state_cache_[ToUL(LegacyRenderState::kAlphaTestEnable)];
  const bool alpha_test_enabled = (alpha_test_state != kFalse32 && alpha_test_state != kCacheInvalidSentinel);
  if (tex->HasAlpha() && alpha_test_enabled) {
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
void zCRnd_D3D_DX11::ApplyTextureAddressMode() {
  if (!impl_) {
    return;
  }

  unsigned long addr_u = tex_stage_state_cache_[0][zRND_TSS_ADDRESSU];
  unsigned long addr_v = tex_stage_state_cache_[0][zRND_TSS_ADDRESSV];
  const unsigned long addr_both = tex_stage_state_cache_[0][zRND_TSS_ADDRESS];
  if (addr_u == kCacheInvalidSentinel) {
    addr_u = (addr_both != kCacheInvalidSentinel) ? addr_both : (texture_wrap_enabled_ ? 1UL : 3UL);
  }
  if (addr_v == kCacheInvalidSentinel) {
    addr_v = (addr_both != kCacheInvalidSentinel) ? addr_both : (texture_wrap_enabled_ ? 1UL : 3UL);
  }

  impl_->SetSamplerAddressing(0, ToAddressMode(addr_u), ToAddressMode(addr_v));
}

// ApplyOpaqueRenderStates - Configures render state for opaque geometry.
void zCRnd_D3D_DX11::ApplyOpaqueRenderStates() {
  // Set alpha_blend_func_ so DrawVertexBuffer uses correct state
  alpha_blend_func_ = zRND_ALPHA_FUNC_NONE;
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaBlendEnable), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaTestEnable), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kDitherEnable), dither_enabled_ ? kTrue32 : kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kZWriteEnable), z_buffer_write_enabled_ ? kTrue32 : kFalse32);
  SetZBufferCompare(z_buffer_cmp_);
}

// ApplyAlphaTestStates - Configures render state for alpha-tested geometry.
// Used for textures with alpha channel to achieve punch-through transparency.
void zCRnd_D3D_DX11::ApplyAlphaTestStates() {
  // Set alpha_blend_func_ so DrawVertexBuffer uses correct state
  alpha_blend_func_ = zRND_ALPHA_FUNC_TEST;

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
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaBlendEnable), kTrue32);
  ApplyRenderState(ToUL(LegacyRenderState::kSrcBlend), ToUL(LegacyBlend::kOne));
  ApplyRenderState(ToUL(LegacyRenderState::kDestBlend), ToUL(LegacyBlend::kZero));
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaTestEnable), kTrue32);
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaFunc), ToUL(LegacyCompareFunc::kGreaterEqual));
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaRef), alpha_reference_);
  ApplyRenderState(ToUL(LegacyRenderState::kZFunc), ToUL(LegacyCompareFunc::kLessEqual));
  ApplyRenderState(ToUL(LegacyRenderState::kZWriteEnable), kTrue32);
  ApplyRenderState(ToUL(LegacyRenderState::kDitherEnable), kFalse32);
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
void zCRnd_D3D_DX11::BuildAlphaPolyVertices(gmp::renderer::d3d11::QueuedAlphaPoly* ap, const zCPolygon* poly, const zCMaterial* mat) {
  using namespace gmp::renderer::d3d11;

  // Configure render state.
  ap->texture = active_texture_[0];
  ap->texture_has_alpha = active_texture_[0] && active_texture_[0]->HasAlpha();
  ap->z_func = static_cast<ZBufferCmp>(z_buffer_cmp_);
  ap->z_bias = active_status_.zbias;

  // Snapshot the texture wrap state for deferred rendering.
  ap->texture_wrap = texture_wrap_enabled_ != 0;

  const bool has_texture = (ap->texture != nullptr);

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
    const float z_cam = vert->vertCamSpace.n[2];
    z_sum += z_cam;

    auto& v = ap->verts[i];
    v.x = vert->vertScrX;
    v.y = vert->vertScrY;
    v.rhw = rhw;

    // Use RHW-based depth with stable BeginFrame constants.
    // ALWAYS clamp to [0,1]: Issue 13 fix - out-of-range depth causes D3D11 clip rejection
    // resulting in sky dome horizontal seam artifacts.
    v.z = std::clamp(rhw_z_proj_offset_ + rhw_z_proj_scale_ * rhw, 0.0f, 1.0f);

    // Untextured ADD geometry handling:
    // - Screen blend overlays (sun glow) use extremely high RHW (~10000) for near-plane rendering
    //   These should keep their natural near-plane depth to render on top of the scene.
    // - Sky dome mesh uses normal camera-space RHW values and should render at far-plane
    //   to appear behind all scene geometry and avoid visible dome edges.
    // Threshold: RHW > 100 indicates a near-plane overlay (normal scene RHW is << 1 for distant objects)
    const bool is_untextured_add = (!has_texture && ap->blend_func == gmp::renderer::d3d11::AlphaBlendFunc::kAdd);
    const bool is_near_plane_overlay = (rhw > 100.0f);  // Screen blend uses rhw ~= 10000
    if (is_untextured_add && !is_near_plane_overlay) {
      // Sky dome or similar background geometry - force to far plane
      v.z = 0.9999f;
    }

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
void zCRnd_D3D_DX11::QueueAlphaPoly(zCPolygon* poly, zCMaterial* mat) {
  if (!poly || poly->numClipVert < 3) {
    return;
  }

  auto* ap = alpha_poly_queue_.Allocate();
  if (!ap) {
    alpha_limit_reached_ = TRUE;
    return;
  }

  // Populate the queued poly (captures texture, blend func, z, etc.)
  BuildAlphaPolyVertices(ap, poly, mat);

  // Submit to depth bucket list for proper sorted rendering
  alpha_poly_queue_.Submit(ap);
  ++num_alpha_polys_;
}

void zCRnd_D3D_DX11::FlushBatch() {
  if (batch_verts_.empty() || batch_indices_.empty())
    return;

  if (impl_) {
    impl_->DrawBatchRHW(batch_verts_.data(), static_cast<int>(batch_verts_.size()), batch_indices_.data(), static_cast<int>(batch_indices_.size()),
                        static_cast<int>(z_buffer_cmp_));
  }

  batch_verts_.clear();
  batch_indices_.clear();
}

void zCRnd_D3D_DX11::AddToBatch(const std::vector<VertexRHW>& verts) {
  if (verts.size() < 3)
    return;

  // Check limits (16-bit indices)
  // Max vertices 65536.
  if (batch_verts_.size() + verts.size() > 60000) {
    FlushBatch();
  }

  uint16_t base_idx = static_cast<uint16_t>(batch_verts_.size());
  int tri_count = static_cast<int>(verts.size()) - 2;

  // Append vertices
  batch_verts_.insert(batch_verts_.end(), verts.begin(), verts.end());

  // Append indices (Fan -> List)
  for (int i = 0; i < tri_count; ++i) {
    batch_indices_.push_back(base_idx + 0);
    batch_indices_.push_back(base_idx + static_cast<uint16_t>(i + 1));
    batch_indices_.push_back(base_idx + static_cast<uint16_t>(i + 2));
  }
}

// DrawPolyVertexLit - Renders a polygon using vertex colors for lighting.
//
// This is the main rendering path for dynamic geometry (models, particles, sky effects).
// It handles both opaque and alpha-blended polygons, with alpha polys being queued
// for sorted rendering.
void zCRnd_D3D_DX11::DrawPolyVertexLit(zCPolygon* poly) {
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
        // Use RHW-based depth with stable BeginFrame constants.
        // ALWAYS clamp to [0,1]: Issue 13 fix - out-of-range depth causes D3D11 clip rejection.
        .z = std::clamp(rhw_z_proj_offset_ + rhw_z_proj_scale_ * vertTrans->vertCamSpaceZInv, 0.0f, 1.0f),
        .rhw = vertTrans->vertCamSpaceZInv,
        .color = color,
        .u = u,
        .v = v,
    });
  }

  // Only disable alpha blend for non-alpha textures
  if (!hasTextureAlpha) {
    ApplyRenderState(ToUL(LegacyRenderState::kAlphaBlendEnable), kFalse32);
    SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  }
  ResetMultiTexturing();

  if (impl_ && !verts.empty()) {
    AddToBatch(verts);
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
void zCRnd_D3D_DX11::DrawPoly(zCPolygon* poly) {
  if (!poly || poly->polyNumVert < 3)
    return;

  // Block rendering during device reset
  if (GetSurfaceLost())
    return;

  // Temporarily disable radial fog for dynamic geometry rendering
  ScopedRadialFogDisable fog_guard(this, fog_manager_);

  ApplyRenderState(ToUL(LegacyRenderState::kAlphaBlendEnable), kFalse32);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  // Disable clipping (better performance)
  ApplyRenderState(ToUL(LegacyRenderState::kClipping), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kCullMode), ToUL(LegacyCullMode::kNone));

  // With T&L enforced, world geometry goes through DrawVertexBuffer.
  // DrawPoly is used for dynamic geometry (models, particles, sky effects).
  // These don't have lightmaps, so we always use vertex-lit rendering.
  // Alpha-blended polys are automatically queued for sorted rendering.
  DrawPolyVertexLit(poly);
}

void zCRnd_D3D_DX11::DrawLightmapList(zCPolygon** polyList, int numPolys) {
  // Gothic can still emit DrawLightmapList for BSP/static-world surfaces.
  // If we never see this, then missing indoor lightmaps must be explained by VB path state.
  // If we do see it and polys have a lightmap texture, then our renderer is currently ignoring it.
  static bool logged_once = false;
  if (!logged_once) {
    logged_once = true;
    zCPolygon* first_poly = nullptr;
    for (int i = 0; i < numPolys; ++i) {
      if (polyList && polyList[i] != nullptr) {
        first_poly = polyList[i];
        break;
      }
    }

    const char* lm_name = "<null>";
    const char* mat_name = "<null>";
    const void* lm_ptr = nullptr;
    const void* lm_tex_ptr = nullptr;

    if (first_poly != nullptr) {
      if (first_poly->lightmap != nullptr) {
        lm_ptr = static_cast<void*>(first_poly->lightmap);
        if (first_poly->lightmap->tex != nullptr) {
          lm_tex_ptr = static_cast<void*>(first_poly->lightmap->tex);
          const char* n = first_poly->lightmap->tex->GetObjectName().ToChar();
          if (n && n[0]) {
            lm_name = n;
          }
        }
      }

      if (first_poly->material != nullptr) {
        const zSTRING mn = first_poly->material->GetName();
        const char* mnc = mn.ToChar();
        if (mnc && mnc[0]) {
          mat_name = mnc;
        }
      }
    }

    SPDLOG_WARN("DrawLightmapList called: numPolys={} first_poly={} lightmap={} lightmap_tex={} lightmap_tex_name='{}' material='{}'", numPolys,
                static_cast<void*>(first_poly), lm_ptr, lm_tex_ptr, lm_name, mat_name);
  }

  for (int i = 0; i < numPolys; ++i) {
    DrawPoly(polyList[i]);
  }
}

void zCRnd_D3D_DX11::DrawLine(float x1, float y1, float x2, float y2, zCOLOR color) {
  FlushBatch();
  EnsureSceneBegun();
  if (impl_) {
    impl_->DrawLine(x1, y1, x2, y2, color.dword);
  }
}

void zCRnd_D3D_DX11::DrawLineZ(float x1, float y1, float z1, float x2, float y2, float z2, zCOLOR color) {
  // TODO: Implement 3D line drawing
}

void zCRnd_D3D_DX11::SetPixel(float x, float y, zCOLOR color) {
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
void zCRnd_D3D_DX11::DrawPolySimple(zCTexture* texture, zTRndSimpleVertex* vertices, int num_vertices) {
  using enum zTRnd_AlphaBlendFunc;

  FlushBatch();

  if (!vertices || num_vertices < 3 || !impl_ || GetSurfaceLost()) {
    return;
  }

  EnsureSceneBegun();

  // Save and disable fog for 2D rendering.
  const int saved_fog = GetFog();
  SetFog(0);

  SetTexture(0, texture);

  // Configure render state for 2D/UI using native D3D11 API.
  using namespace gmp::renderer::d3d11;

  impl_->SetCullMode(CullMode::kNone);
  impl_->SetAlphaTest(false, 0.0f);

  // Disable multitexturing.
  impl_->SetTextureStageColorOp(1, CombineOp::kDisable);
  impl_->SetTextureStageAlphaOp(1, CombineOp::kDisable);

  // Configure texture stage 0.
  impl_->SetTextureStageColorArg(0, 1, CombineArg::kTexture);
  impl_->SetTextureStageColorArg(0, 2, CombineArg::kDiffuse);
  impl_->SetTextureStageAlphaArg(0, 1, CombineArg::kTexture);
  impl_->SetTextureStageAlphaArg(0, 2, CombineArg::kDiffuse);

  SetZBufferCompare(active_status_.zfunc);

  // Configure alpha blending based on current mode using native D3D11 API.
  bool uses_alpha_blend = false;
  switch (active_status_.alphafunc) {
    case zRND_ALPHA_FUNC_BLEND:
      impl_->SetAlphaBlend(true, BlendFactor::kSrcAlpha, BlendFactor::kInvSrcAlpha);
      impl_->SetTextureStageColorOp(0, CombineOp::kModulate);
      uses_alpha_blend = true;
      break;

    case zRND_ALPHA_FUNC_ADD:
      impl_->SetAlphaBlend(true, BlendFactor::kSrcAlpha, BlendFactor::kOne);
      impl_->SetTextureStageColorOp(0, CombineOp::kModulate);
      uses_alpha_blend = true;
      break;

    case zRND_ALPHA_FUNC_MUL:
      impl_->SetAlphaBlend(true, BlendFactor::kDestColor, BlendFactor::kZero);
      impl_->SetTextureStageColorOp(0, CombineOp::kSelectArg1);
      break;

    case zRND_ALPHA_FUNC_MUL2:
      impl_->SetAlphaBlend(true, BlendFactor::kDestColor, BlendFactor::kSrcColor);
      impl_->SetTextureStageColorOp(0, CombineOp::kSelectArg1);
      break;

    case zRND_ALPHA_FUNC_SUB:
      // SUB not directly supported; fall through to BLEND.
      SPDLOG_WARN("DrawPolySimple: Unsupported alpha function SUB, using BLEND");
      [[fallthrough]];

    default:
      // NONE, TEST, MAT_DEFAULT - check texture alpha.
      if (texture && texture->HasAlpha()) {
        impl_->SetAlphaBlend(true, BlendFactor::kSrcAlpha, BlendFactor::kInvSrcAlpha);
        impl_->SetTextureStageColorOp(0, CombineOp::kModulate);
        impl_->SetTextureStageAlphaOp(0, CombineOp::kModulate);
        uses_alpha_blend = true;
      } else {
        impl_->SetAlphaBlend(false, BlendFactor::kOne, BlendFactor::kZero);
        impl_->SetTextureStageColorOp(0, CombineOp::kModulate);
      }
      break;
  }

  impl_->SetTextureStageAlphaOp(0, CombineOp::kModulate);

  // Detect coordinate space and compute scaling factors.
  // Gothic may pass pixel coordinates or 0-8192 view-space coordinates.
  constexpr float kViewSpace8192 = 8192.0f;
  const float screen_width = (vid_xdim > 0) ? static_cast<float>(vid_xdim) : kViewSpace8192;
  const float screen_height = (vid_ydim > 0) ? static_cast<float>(vid_ydim) : kViewSpace8192;

  float max_x = 0.0f;
  float max_y = 0.0f;
  for (int i = 0; i < num_vertices; ++i) {
    max_x = std::max(max_x, vertices[i].pos.n[0]);
    max_y = std::max(max_y, vertices[i].pos.n[1]);
  }

  auto compute_scale = [kViewSpace8192](float max_val, float screen_val) {
    if (screen_val <= 0.0f || max_val <= 0.0f) {
      return 1.0f;
    }
    if (max_val <= screen_val + 0.5f) {
      return 1.0f;  // Already in pixel space.
    }
    if (max_val <= kViewSpace8192 + 0.5f) {
      return screen_val / kViewSpace8192;  // 0-8192 view-space coordinates.
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

  impl_->DrawTriangleFan(verts, vert_count, 0);

  SetFog(saved_fog);
}

// Properly enables/disables fog with all associated state.
// Delegates to fog_manager_ for actual D3D11 state management.
void zCRnd_D3D_DX11::SetFog(int enable) {
  // Early return if state hasn't changed.
  if (active_status_.fog == enable)
    return;

  active_status_.fog = enable;
  fog_manager_.SetEnabled(enable != 0);

  // Sync fog state to impl layer for shader constant buffer
  SyncFogToImpl();
}

int zCRnd_D3D_DX11::GetFog() const {
  return fog_manager_.IsEnabled() ? 1 : 0;
}

void zCRnd_D3D_DX11::SetRadialFog(int enable) {
  fog_manager_.SetRadialEnabled(enable != 0);

  // Sync fog state to impl layer for shader constant buffer (radial fog affects VS fog distance calc)
  SyncFogToImpl();
}

int zCRnd_D3D_DX11::GetRadialFog() const {
  return fog_manager_.IsRadialEnabled() ? 1 : 0;
}

void zCRnd_D3D_DX11::SetFogColor(const zCOLOR& color) {
  fog_manager_.SetColor(color);

  // Sync fog state to impl layer for shader constant buffer
  SyncFogToImpl();
}

zCOLOR zCRnd_D3D_DX11::GetFogColor() const {
  return fog_manager_.GetColor();
}

void zCRnd_D3D_DX11::SetFogRange(float nearZ, float farZ, int mode) {
  fog_manager_.SetRange(nearZ, farZ, mode);

  // Sync fog state to impl layer for shader constant buffer
  SyncFogToImpl();
}

void zCRnd_D3D_DX11::GetFogRange(float& nearZ, float& farZ, int& mode) {
  fog_manager_.GetRange(nearZ, farZ, mode);
}

void zCRnd_D3D_DX11::SyncFogToImpl() {
  if (!impl_) {
    return;
  }

  // Get fog parameters from the fog manager
  const zCOLOR fog_color = fog_manager_.GetColor();
  const bool fog_enabled = fog_manager_.IsEnabled();
  const bool radial_enabled = fog_manager_.IsRadialEnabled();
  float start = 0.0f, end = 0.0f;
  int mode = 0;
  fog_manager_.GetRange(start, end, mode);

  // Pack color into ARGB format expected by impl
  const unsigned long color_argb = (fog_color.alpha << 24) | (fog_color.r << 16) | (fog_color.g << 8) | fog_color.b;

  // Update impl layer fog state including range fog
  impl_->SetFog(fog_enabled, color_argb, start, end, radial_enabled);
}

zTRnd_PolyDrawMode zCRnd_D3D_DX11::GetPolyDrawMode() const {
  return poly_draw_mode_;
}

void zCRnd_D3D_DX11::SetPolyDrawMode(const zTRnd_PolyDrawMode& mode) {
  poly_draw_mode_ = mode;
  if (impl_) {
    switch (mode) {
      case zRND_DRAW_WIRE:
        impl_->SetFillMode(ToUL(LegacyFillMode::kWireframe));
        break;
      case zRND_DRAW_FLAT:
      case zRND_DRAW_MATERIAL:
      default:
        impl_->SetFillMode(ToUL(LegacyFillMode::kSolid));
        break;
    }
  }
}

int zCRnd_D3D_DX11::GetSurfaceLost() const {
  return surface_lost_;
}

void zCRnd_D3D_DX11::SetSurfaceLost(int lost) {
  surface_lost_ = lost;
}

int zCRnd_D3D_DX11::GetSyncOnAmbientCol() const {
  // Not used.
  return 0;
}

void zCRnd_D3D_DX11::SetSyncOnAmbientCol(int sync) {
  // Not used.
}

void zCRnd_D3D_DX11::SetTextureWrapEnabled(int enable) {
  texture_wrap_enabled_ = enable;
  active_status_.texwrap = enable;  // Update for alpha poly system
  if (impl_) {
    impl_->SetTextureWrap(0, enable);
    impl_->SetTextureWrap(1, enable);
  }
}

int zCRnd_D3D_DX11::GetTextureWrapEnabled() const {
  return texture_wrap_enabled_;
}

// Only applies to stage 0, uses LINEAR for bilinear or POINT for nearest
void zCRnd_D3D_DX11::SetBilerpFilterEnabled(int enable) {
  bilerp_filter_enabled_ = enable;
  active_status_.filter = enable;
  if (impl_) {
    // filter 0 = point, 1 = bilinear, 2 = anisotropic
    int filter = enable ? 1 : 0;
    impl_->SetTextureFilter(0, filter);
  }
}

int zCRnd_D3D_DX11::GetBilerpFilterEnabled() const {
  return bilerp_filter_enabled_;
}

void zCRnd_D3D_DX11::SetDitherEnabled(int enable) {
  dither_enabled_ = enable;
  ApplyRenderState(ToUL(LegacyRenderState::kDitherEnable), enable ? kTrue32 : kFalse32);
}

int zCRnd_D3D_DX11::GetDitherEnabled() const {
  return dither_enabled_;
}

zTRnd_PolySortMode zCRnd_D3D_DX11::GetPolySortMode() const {
  return poly_sort_mode_;
}

void zCRnd_D3D_DX11::SetPolySortMode(const zTRnd_PolySortMode& mode) {
  poly_sort_mode_ = mode;

  // Enable or disable Z-buffer based on sort mode
  if (mode == zRND_SORT_ZBUFFER) {
    ApplyRenderState(ToUL(LegacyRenderState::kZEnable), ToUL(LegacyZEnable::kTrue));
  } else {
    ApplyRenderState(ToUL(LegacyRenderState::kZEnable), ToUL(LegacyZEnable::kFalse));
  }
}

int zCRnd_D3D_DX11::GetZBufferWriteEnabled() const {
  return z_buffer_write_enabled_;
}

void zCRnd_D3D_DX11::SetZBufferWriteEnabled(int enable) {
  z_buffer_write_enabled_ = enable;
  ApplyRenderState(ToUL(LegacyRenderState::kZWriteEnable), enable ? kTrue32 : kFalse32);
}

void zCRnd_D3D_DX11::SetZBias(int bias) {
  z_bias_ = bias;
  active_status_.zbias = bias;  // Update for alpha poly system
  // Convert Gothic's zbias to D3D11's two-component depth bias.
  // Negative values push geometry toward the camera (in front of coplanar surfaces).
  float depth_bias = static_cast<float>(bias) * kDepthBiasScale;
  float slope_bias = static_cast<float>(bias) * kSlopeScaledBiasScale;
  if (impl_)
    impl_->SetZBias(depth_bias, slope_bias);
}

int zCRnd_D3D_DX11::GetZBias() const {
  return z_bias_;
}

zTRnd_ZBufferCmp zCRnd_D3D_DX11::GetZBufferCompare() {
  return z_buffer_cmp_;
}

void zCRnd_D3D_DX11::SetZBufferCompare(const zTRnd_ZBufferCmp& cmp) {
  z_buffer_cmp_ = cmp;
  active_status_.zfunc = cmp;  // Update for alpha poly system
  unsigned long func = ToUL(LegacyCompareFunc::kLessEqual);
  switch (cmp) {
    case zRND_ZBUFFER_CMP_NEVER:
      func = ToUL(LegacyCompareFunc::kNever);
      break;
    case zRND_ZBUFFER_CMP_LESS:
      func = ToUL(LegacyCompareFunc::kLess);
      break;
    case zRND_ZBUFFER_CMP_ALWAYS:
      func = ToUL(LegacyCompareFunc::kAlways);
      break;
    case zRND_ZBUFFER_CMP_LESS_EQUAL:
    default:
      func = ToUL(LegacyCompareFunc::kLessEqual);
      break;
  }
  ApplyRenderState(ToUL(LegacyRenderState::kZFunc), func);
}

int zCRnd_D3D_DX11::GetPixelWriteEnabled() const {
  return pixel_write_enabled_;
}

void zCRnd_D3D_DX11::SetPixelWriteEnabled(int enable) {
  pixel_write_enabled_ = enable;
  DWORD mask = enable ? (kColorWriteEnableRed | kColorWriteEnableGreen | kColorWriteEnableBlue | kColorWriteEnableAlpha) : 0;
  (void)mask;
  if (impl_)
    impl_->SetColorWrite(enable != 0, enable != 0, enable != 0, enable != 0);
}

void zCRnd_D3D_DX11::SetAlphaBlendSource(const zTRnd_AlphaBlendSource& src) {
  alpha_blend_source_ = src;
  active_status_.alphasrc = src;  // Update for alpha poly system
}

zTRnd_AlphaBlendSource zCRnd_D3D_DX11::GetAlphaBlendSource() const {
  return alpha_blend_source_;
}

// The actual D3D state application happens in SetAlphaBlendFuncImmed or
// during rendering (e.g., DrawPolySimple switches on active_status_.alphafunc).
void zCRnd_D3D_DX11::SetAlphaBlendFunc(const zTRnd_AlphaBlendFunc& mode) {
  alpha_blend_func_ = mode;
  active_status_.alphafunc = mode;
}

zTRnd_AlphaBlendFunc zCRnd_D3D_DX11::GetAlphaBlendFunc() const {
  return alpha_blend_func_;
}

float zCRnd_D3D_DX11::GetAlphaBlendFactor() const {
  return alpha_blend_factor_;
}

void zCRnd_D3D_DX11::SetAlphaBlendFactor(const float& factor) {
  alpha_blend_factor_ = factor;
  active_status_.alphafactor = factor;
}

void zCRnd_D3D_DX11::SetAlphaReference(unsigned long ref) {
  alpha_reference_ = ref;
}

unsigned long zCRnd_D3D_DX11::GetAlphaReference() const {
  return alpha_reference_;
}

int zCRnd_D3D_DX11::GetCacheAlphaPolys() const {
  return 1;
}

void zCRnd_D3D_DX11::SetCacheAlphaPolys(int cache) {
  // TODO
}

int zCRnd_D3D_DX11::GetAlphaLimitReached() const {
  return alpha_limit_reached_;
}

// AddAlphaPoly - Queues a polygon for immediate alpha-blended rendering.
//
// Called by Gothic's particle system and effects for transparent geometry
// that should be rendered in the current frame's immediate alpha pass.
//
void zCRnd_D3D_DX11::AddAlphaPoly(const zCPolygon* poly) {
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
void zCRnd_D3D_DX11::FlushAlphaPolys() {
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  ApplyRenderState(ToUL(LegacyRenderState::kClipping), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kCullMode), ToUL(LegacyCullMode::kNone));

  immediate_alpha_poly_queue_.RenderInOrder([this](const gmp::renderer::d3d11::QueuedAlphaPoly& ap) { DrawQueuedAlphaPoly(&ap); });

  immediate_alpha_poly_queue_.Reset();
}

void zCRnd_D3D_DX11::SetRenderMode(zTRnd_RenderMode mode) {
  if (!impl_)
    return;

  render_mode_ = static_cast<int>(mode);

  // Set up standard render states for geometry rendering using native D3D11 API.
  using namespace gmp::renderer::d3d11;

  impl_->SetSamplerAddressing(0, AddressMode::kWrap, AddressMode::kWrap);
  SetZBufferWriteEnabled(TRUE);
  SetZBufferCompare(zRND_ZBUFFER_CMP_LESS);
  impl_->SetAlphaBlend(false, BlendFactor::kOne, BlendFactor::kZero);
  impl_->SetTextureStageColorOp(0, CombineOp::kModulate);
  impl_->SetTextureStageColorArg(0, 1, CombineArg::kTexture);
  impl_->SetTextureStageColorArg(0, 2, CombineArg::kDiffuse);
}

zTRnd_RenderMode zCRnd_D3D_DX11::GetRenderMode() const {
  return static_cast<zTRnd_RenderMode>(render_mode_);
}

int zCRnd_D3D_DX11::HasCapability(zTRnd_Capability cap) const {
  // Modern D3D11 hardware supports everything Gothic II needs
  switch (cap) {
    case zRND_CAP_GUARD_BAND:
      return TRUE;  // All modern cards support guard band clipping
    case zRND_CAP_ALPHATEST:
      return TRUE;  // Alpha testing universally supported
    case zRND_CAP_MAX_BLEND_STAGES:
      return 8;  // D3D11 supports at least 8 texture stages
    case zRND_CAP_MAX_BLEND_STAGE_TEXTURES:
      // D3D11 supports at least 8 textures in multitexture mode
      // critical for staged multitexturing (e.g., lightmaps)
      return 8;
    case zRND_CAP_DIFFUSE_LAST_BLEND_STAGE_ONLY:
      return FALSE;  // Modern cards can use diffuse in any stage
    case zRND_CAP_TNL:
      return TRUE;  // Transform & Lighting supported
    case zRND_CAP_TNL_HARDWARE:
      return TRUE;  // Hardware T&L available
    case zRND_CAP_TNL_MAXLIGHTS:
      return 8;  // D3D11 spec minimum is 8, but modern cards support more
    case zRND_CAP_DEPTH_BUFFER_PREC:
      return 32;  // Modern hardware supports 32-bit depth (D24S8 or D32)
    case zRND_CAP_BLENDDIFFUSEALPHA:
      return TRUE;  // Blend diffuse alpha supported
    default:
      return FALSE;
  }
}

void zCRnd_D3D_DX11::GetGuardBandBorders(float& left, float& right, float& top, float& bottom) {
  // not used by Gothic II, so we can leave it unimplemented
}

void zCRnd_D3D_DX11::ResetZTest() {
  // TODO
}

int zCRnd_D3D_DX11::HasPassedZTest() {
  return 1;
}

zCTexture* zCRnd_D3D_DX11::CreateTexture() {
  return new zCTex_D3D11();
}

zCTextureConvert* zCRnd_D3D_DX11::CreateTextureConvert() {
  return new zCTexConGeneric();
}

int zCRnd_D3D_DX11::GetTotalTextureMem() {
  if (impl_) {
    size_t bytes = impl_->GetAvailableTextureMem();
    if (bytes > 0) {
      return static_cast<int>(std::min<size_t>(bytes, std::numeric_limits<int>::max()));
    }
  }
  return 256 * 1024 * 1024;
}

int zCRnd_D3D_DX11::SupportsTextureFormat(zTRnd_TextureFormat fmt) {
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

int zCRnd_D3D_DX11::SupportsTextureFormatHardware(zTRnd_TextureFormat fmt) {
  switch (fmt) {
    case zRND_TEX_FORMAT_DXT1:
    case zRND_TEX_FORMAT_DXT3:
    case zRND_TEX_FORMAT_DXT5:
      return 1;
    default:
      return SupportsTextureFormat(fmt);
  }
}

int zCRnd_D3D_DX11::GetMaxTextureSize() {
  return impl_->GetCapabilities().max_texture_size;
}

void zCRnd_D3D_DX11::GetStatistics(zTRnd_Stats& stats) {
  // Not implemented - return zeroed stats.
  stats = {};
}

void zCRnd_D3D_DX11::ResetStatistics() {
}

void zCRnd_D3D_DX11::Vid_Blit(int complete, tagRECT* src, tagRECT* dst) {
  // This is called every frame by GameManager::Render() after game->Render()

  if (impl_) {
    impl_->Present();
  }
}

void zCRnd_D3D_DX11::Vid_Clear(zCOLOR& color, int flags) {
  // Convert zCOLOR to D3DCOLOR (ARGB)
  unsigned long d3dColor = (color.alpha << 24) | (color.r << 16) | (color.g << 8) | color.b;
  if (impl_) {
    const bool clear_color = (flags != zRND_CLEAR_ZBUFFER);
    const bool clear_depth = (flags != zRND_CLEAR_FRAMEBUFFER);
    impl_->Clear(d3dColor, clear_color, clear_depth);
  }
}

int zCRnd_D3D_DX11::Vid_Lock(zTRndSurfaceDesc& desc) {
  return 0;
}

int zCRnd_D3D_DX11::Vid_Unlock() {
  return 0;
}

int zCRnd_D3D_DX11::Vid_IsLocked() {
  return 0;
}

int zCRnd_D3D_DX11::Vid_GetFrontBufferCopy(zCTextureConvert& texConv) {
  return 0;
}

int zCRnd_D3D_DX11::Vid_GetNumDevices() {
  return 1;
}

int zCRnd_D3D_DX11::Vid_GetActiveDeviceNr() {
  return 0;
}

int zCRnd_D3D_DX11::Vid_SetDevice(int deviceNr) {
  return 1;
}

int zCRnd_D3D_DX11::Vid_GetDeviceInfo(zTRnd_DeviceInfo& info, int deviceNr) {
  return 1;
}

int zCRnd_D3D_DX11::Vid_GetNumModes() {
  int count = display_modes_.GetNumModes();
  return count;
}

int zCRnd_D3D_DX11::Vid_GetModeInfo(zTRnd_VidModeInfo& info, int modeNr) {
  const auto* mode = display_modes_.GetMode(modeNr);
  if (!mode) {
    SPDLOG_WARN("Vid_GetModeInfo called for invalid mode {}", modeNr);
    return 0;
  }

  info.xdim = mode->width;
  info.ydim = mode->height;
  info.bpp = mode->bpp;
  info.fullscreenOnly = 0;

  return 1;
}

int zCRnd_D3D_DX11::Vid_GetActiveModeNr() {
  return display_modes_.GetActiveModeNr();
}

int zCRnd_D3D_DX11::Vid_SetMode(int modeNr, HWND__** hwnd) {
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

  // Clear any queued alpha polygons before device reset - they reference stale state
  alpha_poly_queue_.Reset();
  immediate_alpha_poly_queue_.Reset();
  num_alpha_polys_ = 0;

  // Update base class members - CRITICAL for clipping!
  vid_xdim = mode->width;
  vid_ydim = mode->height;
  vid_bpp = mode->bpp;

  HWND hWindow = (hwnd && *hwnd) ? *hwnd : GetActiveWindow();

  if (hWindow) {
    const bool want_fullscreen = (screen_mode_ == zRND_SCRMODE_FULLSCREEN);
    ApplyWindowMode(hWindow, want_fullscreen, vid_xdim, vid_ydim);
  }

  // On first call, device doesn't exist yet - use Init().
  // On subsequent calls (resolution change), use Resize() to preserve textures.
  bool fullscreen = (screen_mode_ == zRND_SCRMODE_FULLSCREEN);
  bool success = false;
  if (!impl_->GetDevice()) {
    // First initialization - create the device
    success = impl_->Init(hWindow, vid_xdim, vid_ydim, fullscreen);
  } else {
    // Resolution change - use Resize() to preserve textures (like D3D9's Reset)
    success = impl_->Resize(vid_xdim, vid_ydim);
  }

  if (success) {
    surface_lost_ = 0;  // Allow rendering again
    display_modes_.SetActiveModeNr(modeNr);
    return 1;
  }

  return 0;
}

void zCRnd_D3D_DX11::ApplyWindowMode(HWND hWindow, bool fullscreen, int width, int height) {
  // For D3D11 FLIP model we do borderless fullscreen and must explicitly change window styles/sizing.
  static bool s_saved_window_style = false;
  static LONG s_window_style = 0;
  static LONG s_window_ex_style = 0;

  if (!s_saved_window_style) {
    s_window_style = GetWindowLong(hWindow, GWL_STYLE);
    s_window_ex_style = GetWindowLong(hWindow, GWL_EXSTYLE);
    s_saved_window_style = true;
  }

  if (fullscreen) {
    // Borderless fullscreen: remove decoration and size to the current monitor.
    HMONITOR mon = MonitorFromWindow(hWindow, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(mon, &mi)) {
      LONG style = s_window_style;
      style &= ~WS_OVERLAPPEDWINDOW;
      style |= WS_POPUP;
      SetWindowLong(hWindow, GWL_STYLE, style);

      LONG ex_style = s_window_ex_style;
      ex_style &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
      SetWindowLong(hWindow, GWL_EXSTYLE, ex_style);

      const int w = mi.rcMonitor.right - mi.rcMonitor.left;
      const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
      SetWindowPos(hWindow, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
      // Fallback: just move to (0,0) and size to requested resolution.
      SetWindowPos(hWindow, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
  } else {
    // Windowed: restore original styles and size client area to requested resolution.
    if (s_saved_window_style) {
      SetWindowLong(hWindow, GWL_STYLE, s_window_style);
      SetWindowLong(hWindow, GWL_EXSTYLE, s_window_ex_style);
    }

    RECT rc = {0, 0, width, height};
    AdjustWindowRect(&rc, GetWindowLong(hWindow, GWL_STYLE), FALSE);
    SetWindowPos(hWindow, HWND_TOP, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }

  ShowWindow(hWindow, SW_SHOW);
  UpdateWindow(hWindow);
  SetForegroundWindow(hWindow);
  SetFocus(hWindow);
}

void zCRnd_D3D_DX11::Vid_SetScreenMode(zTRnd_ScreenMode mode) {
  // HIDE mode is treated as WINDOWED
  if (mode == zRND_SCRMODE_HIDE) {
    mode = zRND_SCRMODE_WINDOWED;
  }

  // Store the requested mode - will be used when Init() creates the device
  zTRnd_ScreenMode previous_mode = screen_mode_;
  screen_mode_ = mode;

  if (!impl_ || !impl_->GetDevice()) {
    // Device not initialized yet - just store the mode for later use in Init()
    return;
  }

  // If the mode has actually changed, switch fullscreen state
  if (mode != previous_mode) {
    bool want_fullscreen = (mode == zRND_SCRMODE_FULLSCREEN);
    if (!impl_->SetFullscreenState(want_fullscreen)) {
      SPDLOG_WARN("Vid_SetScreenMode: Failed to switch fullscreen state, reverting to previous mode");
      screen_mode_ = previous_mode;
    }
  }
}

zTRnd_ScreenMode zCRnd_D3D_DX11::Vid_GetScreenMode() {
  // For D3D11 FLIP model we implement borderless fullscreen, so we must NOT report FULLSCREEN
  // to the engine (otherwise it will set surface_lost and force mode switches).
  zTRnd_ScreenMode result = screen_mode_;
  const bool flip = (impl_ && impl_->IsUsingFlipModel());
  if (flip && screen_mode_ == zRND_SCRMODE_FULLSCREEN) {
    result = zRND_SCRMODE_WINDOWED;
  }

  return result;
}

void zCRnd_D3D_DX11::Vid_SetGammaCorrection(float gamma, float contrast, float brightness) {
  gamma_ = gamma;
  contrast_ = contrast;
  brightness_ = brightness;
  if (impl_) {
    impl_->SetGammaCorrection(gamma, contrast, brightness);
  }
}

float zCRnd_D3D_DX11::Vid_GetGammaCorrection() {
  return gamma_;
}

void zCRnd_D3D_DX11::Vid_BeginLfbAccess() {
  // TODO
}

void zCRnd_D3D_DX11::Vid_EndLfbAccess() {
  // TODO
}

void zCRnd_D3D_DX11::Vid_SetLfbAlpha(int alpha) {
  // TODO
}

void zCRnd_D3D_DX11::Vid_SetLfbAlphaFunc(const zTRnd_AlphaBlendFunc& func) {
  // TODO
}

void zCRnd_D3D_DX11::EnsureSceneBegun() {
  if (!impl_ || GetSurfaceLost()) {
    return;
  }
  impl_->BeginFrame();
}

int zCRnd_D3D_DX11::SetTransform(zTRnd_TrafoType type, const zMAT4& matrix) {
  if (!impl_) {
    return 0;
  }

  switch (type) {
    case zRND_TRAFO_VIEW:
      // Gothic's VIEW matrix is actually a combined model-view (WORLD) matrix.
      // It's already transposed to row-major format by Gothic.
      // We set WORLD and leave VIEW at identity (same as D3D9 path).
      view_matrix_ = matrix;
      impl_->SetWorldMatrix(reinterpret_cast<const float*>(&view_matrix_));
      return 1;
    case zRND_TRAFO_PROJECTION:
      // Gothic's projection matrix is already in D3D's row-major format.
      proj_matrix_ = matrix;
      impl_->SetProjectionMatrix(reinterpret_cast<const float*>(&proj_matrix_));
      // Do NOT update RHW depth constants from projection matrix.
      // RHW vertices were transformed using the camera state at BeginFrame, so we
      // must use depth constants computed from BeginFrame near/far (z_min/max_from_engine_).
      // Updating from the projection matrix here would cause depth mismatches when
      // Gothic changes projection mid-frame for overlays.
      return 1;
    case zRND_TRAFO_TEXTURE0:
      impl_->SetTextureTransformMatrix(0, reinterpret_cast<const float*>(&matrix));
      return 1;
    case zRND_TRAFO_TEXTURE1:
      impl_->SetTextureTransformMatrix(1, reinterpret_cast<const float*>(&matrix));
      return 1;
    default:
      // Texture transforms and other types are ignored in the D3D11 path for now.
      return 0;
  }
}

int zCRnd_D3D_DX11::SetViewport(int x, int y, int w, int h) {
  if (!impl_)
    return 0;
  // Flush any pending batched geometry before changing the viewport.
  // Gothic inventory rendering sets a small viewport for each item slot,
  // renders the item, then restores the full viewport. If we don't flush,
  // geometry batched under the old viewport will be drawn with the new one.
  FlushBatch();

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

// Configures a renderer-owned Light for a point light.
void SetupPointLight(Light& out, const zCRenderLight* light) {
  out.type = LightType::kPoint;
  out.range = light->range;
  out.attenuation0 = 0.0f;
  out.attenuation1 = kPointLightLinearAttenuation;
  out.attenuation2 = 0.0f;
  out.position_x = light->positionLS[VX];
  out.position_y = light->positionLS[VY];
  out.position_z = light->positionLS[VZ];
}

// Configures a renderer-owned Light for a spot light.
void SetupSpotLight(Light& out, const zCRenderLight* light) {
  out.type = LightType::kSpot;
  out.range = light->range;
  out.attenuation0 = 0.0f;
  out.attenuation1 = 1.0f;
  out.attenuation2 = 0.0f;
  out.spot_outer_angle_rad = DegToRad(40.0f);  // Fixed cone angle; light->spotConeAngle unused.
  out.position_x = light->positionLS[VX];
  out.position_y = light->positionLS[VY];
  out.position_z = light->positionLS[VZ];

  // Guard against zero-length direction vector.
  zVEC3 dir = light->directionLS;
  if (dir[VX] == 0.0f && dir[VY] == 0.0f && dir[VZ] == 0.0f) {
    dir = zVEC3(1, 0, 0);
  }
  out.direction_x = dir[VX];
  out.direction_y = dir[VY];
  out.direction_z = dir[VZ];
}

// Configures a renderer-owned Light for a directional light.
void SetupDirectionalLight(Light& out, zCRenderLight* light) {
  out.type = LightType::kDirectional;
  // Guard against zero-length direction vector.
  auto& dir = light->directionLS;
  if (dir[VX] == 0.0f && dir[VY] == 0.0f && dir[VZ] == 0.0f) {
    dir = zVEC3(1, 0, 0);
  }
  out.direction_x = dir[VX];
  out.direction_y = dir[VY];
  out.direction_z = dir[VZ];
}

}  // namespace

int zCRnd_D3D_DX11::SetLight(unsigned long index, zCRenderLight* light) {
  // Track which slot holds the ambient light (if any).
  static std::optional<unsigned long> s_ambient_light_index;

  if (!light) {
    // Disable the light at this index.
    if (s_ambient_light_index == index) {
      s_ambient_light_index.reset();
      impl_->SetAmbientLight(0);
    }
    impl_->LightEnable(index, FALSE);
    return TRUE;
  }

  // Handle ambient light specially - it uses the ambient color in the light buffer.
  if (light->lightType == zLIGHT_TYPE_AMBIENT) {
    s_ambient_light_index = index;
    impl_->SetAmbientLight(PackAmbientColor(light->colorDiffuse));
    impl_->LightEnable(index, FALSE);
    return TRUE;
  }

  // Build renderer-owned light structure.
  Light out{};

  // Gothic's colorDiffuse is in 0-255 range; normalize to 0.0-1.0.
  constexpr float kColorScale = 1.0f / 255.0f;
  out.diffuse_r = light->colorDiffuse[0] * kColorScale;
  out.diffuse_g = light->colorDiffuse[1] * kColorScale;
  out.diffuse_b = light->colorDiffuse[2] * kColorScale;

  switch (light->lightType) {
    case zLIGHT_TYPE_POINT:
      SetupPointLight(out, light);
      break;
    case zLIGHT_TYPE_SPOT:
      SetupSpotLight(out, light);
      break;
    case zLIGHT_TYPE_DIR:
      SetupDirectionalLight(out, light);
      break;
    default:
      return TRUE;
  }

  // Pass the light to the impl for shader-based lighting
  impl_->SetLight(index, out);
  impl_->LightEnable(index, TRUE);

  // Clear ambient if this slot was previously ambient.
  if (s_ambient_light_index == index) {
    s_ambient_light_index.reset();
    impl_->SetAmbientLight(0);
  }

  return TRUE;
}

int zCRnd_D3D_DX11::GetMaterial(zCRenderer::zTMaterial& mat) {
  mat.diffuseRGBA = current_material_.diffuseRGBA;
  mat.ambientRGBA = current_material_.ambientRGBA;
  return 1;
}

int zCRnd_D3D_DX11::SetMaterial(const zCRenderer::zTMaterial& mat) {
  if (!impl_)
    return 0;

  // Store current material for GetMaterial
  current_material_.diffuseRGBA = mat.diffuseRGBA;
  current_material_.ambientRGBA = mat.ambientRGBA;

  MaterialCB material{};
  material.diffuse = {mat.diffuseRGBA[0], mat.diffuseRGBA[1], mat.diffuseRGBA[2], mat.diffuseRGBA[3]};
  material.ambient = {mat.ambientRGBA[0], mat.ambientRGBA[1], mat.ambientRGBA[2], mat.ambientRGBA[3]};
  material.specular = {0.0f, 0.0f, 0.0f, 0.0f};
  material.emissive = {0.0f, 0.0f, 0.0f, 0.0f};
  material.power = 0.0f;
  material.alpha_ref = 0.0f;
  material.alpha_test_enabled = 0.0f;

  impl_->SetMaterial(material);

  return 1;
}

int zCRnd_D3D_DX11::SetTexture(unsigned long stage, zCTexture* texture) {
  FlushBatch();
  if (!impl_ || GetSurfaceLost())
    return 0;

  // Resolve animated textures to the actual current frame.
  // Gothic frequently passes a wrapper texture here (animated, tiled, etc.).
  // The D3D11 SRV lives on the underlying texture instance.
  zCTexture* resolved_texture = texture;
  if (resolved_texture != nullptr) {
    // IMPORTANT: Some wrapper textures can return null from GetAniTexture().
    // If null, use the wrapper itself; otherwise we incorrectly drop the binding.
    zCTexture* ani = resolved_texture->GetAniTexture();
    if (ani != nullptr) {
      resolved_texture = ani;
    }
  }

  // Update active_texture_ array for debug capture.
  if (stage < 4) {
    active_texture_[stage] = resolved_texture;
  }

  ID3D11ShaderResourceView* srv = nullptr;
  bool hasAlpha = false;
  DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

  if (resolved_texture) {
    zCTex_D3D11* tex11 = dynamic_cast<zCTex_D3D11*>(resolved_texture);
    if (tex11 != nullptr) {
      // Ensure SRV is created before binding; some Gothic textures may load without SRVs yet.
      if (!tex11->EnsureSRV(gmp::renderer::d3d11::g_D3D11Device)) {
        zSTRING name = tex11->GetObjectName();
        SPDLOG_WARN("SetTexture: Texture '{}' at stage {} failed to create D3D11 SRV", name.ToChar(), stage);
      }

      srv = tex11->GetSRV();
      hasAlpha = tex11->HasAlpha();
      dxgiFormat = tex11->GetDXGIFormat();
      if (!srv) {
        zSTRING name = tex11->GetObjectName();
        SPDLOG_WARN("SetTexture: Texture '{}' at stage {} has no D3D11 SRV", name.ToChar(), stage);
      }
    } else {
      SPDLOG_WARN("SetTexture: Texture at stage {} is not zCTex_D3D11 (ptr={} resolved={})", stage, (void*)texture, (void*)resolved_texture);
    }
  }

  impl_->SetTexture(static_cast<int>(stage), srv, dxgiFormat);

  // Handle Alpha Testing for Stage 0 (Primary Texture)
  // Match D3D9's SetTexture behavior exactly:
  // - If alpha_blend_func_ is already TEST, enable alpha test
  // - If texture has alpha AND blend func is NONE/MAT_DEFAULT, enable alpha test
  // - For smooth-alpha textures (DXT3/DXT5), upgrade TEST to BLEND_TEST
  if (stage == 0) {
    bool enableAlphaTest = false;

    // Check for smooth alpha once - used for potential TEST->BLEND_TEST upgrade
    bool smooth_alpha = false;
    if (hasAlpha && resolved_texture != nullptr) {
      auto* tex11 = dynamic_cast<zCTex_D3D11*>(resolved_texture);
      if (tex11 != nullptr) {
        smooth_alpha = tex11->HasSmoothAlpha();
      }
    }

    // Match D3D9 logic: check alpha_blend_func_ directly, not render-state cache
    if (alpha_blend_func_ == zRND_ALPHA_FUNC_TEST) {
      enableAlphaTest = true;
      // Upgrade TEST to BLEND_TEST for smooth-alpha textures (nets, etc.)
      if (smooth_alpha) {
        alpha_blend_func_ = zRND_ALPHA_FUNC_BLEND_TEST;
      }
    } else if (hasAlpha && (alpha_blend_func_ == zRND_ALPHA_FUNC_NONE || alpha_blend_func_ == zRND_ALPHA_FUNC_MAT_DEFAULT)) {
      enableAlphaTest = true;
      // For smooth-alpha textures, use BLEND_TEST; otherwise plain TEST
      alpha_blend_func_ = smooth_alpha ? zRND_ALPHA_FUNC_BLEND_TEST : zRND_ALPHA_FUNC_TEST;
    }

    ApplyRenderState(ToUL(LegacyRenderState::kAlphaTestEnable), enableAlphaTest ? kTrue32 : kFalse32);

    if (enableAlphaTest) {
      // Standard alpha test configuration for Gothic
      ApplyRenderState(ToUL(LegacyRenderState::kAlphaFunc), ToUL(LegacyCompareFunc::kGreaterEqual));

      // Ensure a sane default if the engine hasn't set ALPHAREF yet.
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

int zCRnd_D3D_DX11::SetTextureStageState(unsigned long stage, zTRnd_TextureStageState state, unsigned long value) {
  if (stage >= kMaxTextureStages || state >= zRND_TSS_COUNT) {
    SPDLOG_WARN("SetTextureStageState: invalid stage {} or state {}", stage, static_cast<unsigned long>(state));
    return 0;
  }

  // Update cache
  if (tex_stage_state_cache_[stage][state] != value) {
    tex_stage_state_cache_[stage][state] = value;
  }

  if (!impl_) {
    return 1;
  }

  // Translate Gothic texture stage states to native D3D11 API calls.
  using namespace gmp::renderer::d3d11;

  // Helper to convert Gothic zRND_TOP_* to native CombineOp.
  auto to_combine_op = [](unsigned long op) -> CombineOp {
    switch (op) {
      case zRND_TOP_DISABLE:
        return CombineOp::kDisable;
      case zRND_TOP_SELECTARG1:
        return CombineOp::kSelectArg1;
      case zRND_TOP_SELECTARG2:
        return CombineOp::kSelectArg2;
      case zRND_TOP_MODULATE:
        return CombineOp::kModulate;
      case zRND_TOP_MODULATE2X:
        return CombineOp::kModulate2X;
      case zRND_TOP_MODULATE4X:
        return CombineOp::kModulate4X;
      case zRND_TOP_ADD:
        return CombineOp::kAdd;
      case zRND_TOP_ADDSMOOTH:
        return CombineOp::kAddSmooth;
      case zRND_TOP_BLENDDIFFUSEALPHA:
        return CombineOp::kBlendDiffuseAlpha;
      default:
        return CombineOp::kModulate;
    }
  };

  // Helper to convert Gothic zRND_TA_* to native CombineArg.
  auto to_combine_arg = [](unsigned long arg) -> CombineArg {
    switch (arg) {
      case zRND_TA_CURRENT:
        return CombineArg::kCurrent;
      case zRND_TA_DIFFUSE:
        return CombineArg::kDiffuse;
      case zRND_TA_TEXTURE:
        return CombineArg::kTexture;
      case zRND_TA_TFACTOR:
        return CombineArg::kTFactor;
      case zRND_TA_SPECULAR:
        return CombineArg::kSpecular;
      default:
        return CombineArg::kCurrent;
    }
  };

  // Helper to convert Gothic TEXCOORDINDEX to native types.
  auto to_uv_source = [](unsigned long tci) -> TexCoordSource {
    const unsigned long index = tci & 0xFFFF;  // Low 16 bits = UV set index
    if (index == 0)
      return TexCoordSource::kUV0;
    if (index == 1)
      return TexCoordSource::kUV1;
    return TexCoordSource::kGenerated;
  };

  auto to_texgen_mode = [](unsigned long tci) -> TexGenMode {
    const unsigned long flags = tci & 0xFFFF0000;  // High 16 bits = texgen flags
    if (flags == zRND_TSS_TCI_CAMERASPACENORMAL)
      return TexGenMode::kCameraSpaceNormal;
    if (flags == zRND_TSS_TCI_CAMERASPACEPOSITION)
      return TexGenMode::kCameraSpacePosition;
    if (flags == zRND_TSS_TCI_CAMERASPACEREFLECTIONVECTOR)
      return TexGenMode::kCameraSpaceReflection;
    return TexGenMode::kNone;
  };

  // Helper to convert legacy D3D texture filter (D3D7-D3D9) to native FilterMode.
  auto to_filter_mode = [](unsigned long filter) -> FilterMode {
    switch (filter) {
      case 1:  // D3DFILTER_NEAREST / D3DTEXF_POINT
        return FilterMode::kPoint;
      case 2:  // D3DFILTER_LINEAR / D3DTEXF_LINEAR
      case 3:  // D3DFILTER_MIPNEAREST
      case 4:  // D3DFILTER_MIPLINEAR
      case 5:  // D3DFILTER_LINEARMIPNEAREST
      case 6:  // D3DFILTER_LINEARMIPLINEAR
      default:
        return FilterMode::kLinear;
    }
  };

  const int s = static_cast<int>(stage);

  switch (state) {
    case zRND_TSS_COLOROP:
      impl_->SetTextureStageColorOp(s, to_combine_op(value));
      break;
    case zRND_TSS_COLORARG1:
      impl_->SetTextureStageColorArg(s, 1, to_combine_arg(value));
      break;
    case zRND_TSS_COLORARG2:
      impl_->SetTextureStageColorArg(s, 2, to_combine_arg(value));
      break;
    case zRND_TSS_ALPHAOP:
      impl_->SetTextureStageAlphaOp(s, to_combine_op(value));
      break;
    case zRND_TSS_ALPHAARG1:
      impl_->SetTextureStageAlphaArg(s, 1, to_combine_arg(value));
      break;
    case zRND_TSS_ALPHAARG2:
      impl_->SetTextureStageAlphaArg(s, 2, to_combine_arg(value));
      break;
    case zRND_TSS_TEXCOORDINDEX:
      impl_->SetTextureStageUVSource(s, to_uv_source(value), to_texgen_mode(value));
      break;
    case zRND_TSS_TEXTURETRANSFORMFLAGS:
      impl_->SetTextureStageTransform(s, value != zRND_TTF_DISABLE);
      break;
    case zRND_TSS_ADDRESS:
      // Both U and V.
      impl_->SetSamplerAddressing(s, ToAddressMode(value), ToAddressMode(value));

      // Keep per-axis cache in sync so later ADDRESSU/ADDRESSV calls can preserve
      // the other axis correctly.
      tex_stage_state_cache_[stage][zRND_TSS_ADDRESSU] = value;
      tex_stage_state_cache_[stage][zRND_TSS_ADDRESSV] = value;
      break;
    case zRND_TSS_ADDRESSU:
    case zRND_TSS_ADDRESSV: {
      // Preserve the axis that wasn't updated.
      // Gothic can set U and V independently for some materials/effects.
      // Each axis must be preserved separately when only one is updated.
      unsigned long u_value = tex_stage_state_cache_[stage][zRND_TSS_ADDRESSU];
      unsigned long v_value = tex_stage_state_cache_[stage][zRND_TSS_ADDRESSV];

      // Fall back to combined ADDRESS if the per-axis state wasn't set.
      const unsigned long both_value = tex_stage_state_cache_[stage][zRND_TSS_ADDRESS];
      if (u_value == kCacheInvalidSentinel) {
        u_value = (both_value != kCacheInvalidSentinel) ? both_value : value;
      }
      if (v_value == kCacheInvalidSentinel) {
        v_value = (both_value != kCacheInvalidSentinel) ? both_value : value;
      }

      if (state == zRND_TSS_ADDRESSU) {
        u_value = value;
      } else {
        v_value = value;
      }

      impl_->SetSamplerAddressing(s, ToAddressMode(u_value), ToAddressMode(v_value));
      break;
    }
    case zRND_TSS_MAGFILTER:
    case zRND_TSS_MINFILTER:
      // Use the filter mode for all filter types.
      impl_->SetSamplerFilter(s, to_filter_mode(value));
      break;
    case zRND_TSS_MIPFILTER: {
      // Mip filtering is separate from min/mag in D3D9.
      // Treat anything that isn't explicitly LINEAR as POINT/NONE.
      const bool mip_linear = (value == 2 || value == 4 || value == 6);
      impl_->SetSamplerMipFilter(s, mip_linear);
      break;
    }
    default:
      // Other states (bump mapping, border color, etc.) are ignored.
      break;
  }

  return 1;
}

// SetAlphaBlendFuncImmed - Configures alpha blending state for rendering.
// Called from DrawPolySimple, alpha poly Draw, etc.
int zCRnd_D3D_DX11::SetAlphaBlendFuncImmed(zTRnd_AlphaBlendFunc func) {
  FlushBatch();
  const bool enable_alpha_test = (func == zRND_ALPHA_FUNC_TEST || func == zRND_ALPHA_FUNC_BLEND_TEST);
  const bool disable_blending = (func == zRND_ALPHA_FUNC_NONE);

  // IMPORTANT: Track the blend function so DrawVertexBuffer can use it.
  // Gothic's water and other alpha-sorted VOBs call SetAlphaBlendFuncImmed before
  // rendering, and DrawVertexBuffer needs to know the blend mode.
  alpha_blend_func_ = func;
  active_status_.alphafunc = func;

  if (!impl_) {
    return 1;
  }

  using namespace gmp::renderer::d3d11;

  // Configure blend state using native D3D11 API.
  BlendFactor src_blend = BlendFactor::kOne;
  BlendFactor dst_blend = BlendFactor::kZero;

  switch (func) {
    case zRND_ALPHA_FUNC_TEST:
      src_blend = BlendFactor::kOne;
      dst_blend = BlendFactor::kZero;
      break;
    case zRND_ALPHA_FUNC_BLEND:
    case zRND_ALPHA_FUNC_BLEND_TEST:
      src_blend = BlendFactor::kSrcAlpha;
      dst_blend = BlendFactor::kInvSrcAlpha;
      break;
    case zRND_ALPHA_FUNC_ADD:
      src_blend = BlendFactor::kSrcAlpha;
      dst_blend = BlendFactor::kOne;
      break;
    case zRND_ALPHA_FUNC_MUL:
      src_blend = BlendFactor::kDestColor;
      dst_blend = BlendFactor::kZero;
      break;
    case zRND_ALPHA_FUNC_MUL2:
      src_blend = BlendFactor::kDestColor;
      dst_blend = BlendFactor::kSrcColor;
      break;
    case zRND_ALPHA_FUNC_NONE:
    default:
      break;  // Defaults are fine.
  }

  impl_->SetAlphaBlend(!disable_blending, src_blend, dst_blend);

  // Configure alpha test and depth write states.
  if (enable_alpha_test) {
    impl_->SetAlphaTest(true, static_cast<float>(alpha_reference_) / 255.0f);
    impl_->SetDepthTest(true, true, CompareFunc::kLessEqual);
  } else {
    impl_->SetAlphaTest(false, 0.0f);
    const bool z_write = disable_blending && active_status_.zwrite;
    impl_->SetDepthTest(true, z_write, CompareFunc::kLessEqual);
    SetZBufferCompare(active_status_.zfunc);
  }

  // Keep legacy cache in sync for code that still reads it.
  render_state_cache_[ToUL(LegacyRenderState::kAlphaBlendEnable)] = disable_blending ? kFalse32 : kTrue32;
  render_state_cache_[ToUL(LegacyRenderState::kAlphaTestEnable)] = enable_alpha_test ? kTrue32 : kFalse32;
  render_state_cache_[ToUL(LegacyRenderState::kZWriteEnable)] =
      (enable_alpha_test || (disable_blending && active_status_.zwrite)) ? kTrue32 : kFalse32;

  alpha_blend_immed_ = 1;
  return 1;
}

int zCRnd_D3D_DX11::SetRenderState(zTRnd_RenderStateType state, unsigned long value) {
  FlushBatch();
  switch (state) {
    case zRND_RENDERSTATE_CLIPPING:
      // D3D11 always clips - this is a no-op but we keep the cache in sync.
      render_state_cache_[ToUL(LegacyRenderState::kClipping)] = value;
      return 1;

    case zRND_RENDERSTATE_TEXTUREFACTOR:
      texture_factor_ = value;
      render_state_cache_[ToUL(LegacyRenderState::kTextureFactor)] = value;
      if (impl_) {
        impl_->SetTextureFactor(value);
      }
      return 1;

    default:
      SPDLOG_WARN("SetRenderState: unsupported state {}", static_cast<unsigned long>(state));
      return 0;
  }
}

unsigned long zCRnd_D3D_DX11::GetRenderState(zTRnd_RenderStateType state) {
  switch (state) {
    case zRND_RENDERSTATE_CLIPPING:
      return render_state_cache_[ToUL(LegacyRenderState::kClipping)];
    case zRND_RENDERSTATE_TEXTUREFACTOR:
      return render_state_cache_[ToUL(LegacyRenderState::kTextureFactor)];
  }
  return 0;
}

bool zCRnd_D3D_DX11::ApplyRenderState(unsigned long state, unsigned long value) {
  const size_t maxStates = sizeof(render_state_cache_) / sizeof(render_state_cache_[0]);
  if (state >= maxStates) {
    return false;
  }

  if (render_state_cache_[state] == value) {
    return true;
  }

  render_state_cache_[state] = value;

  // Keep renderer-side cached state in sync when legacy render states are
  // applied directly (i.e., without going through SetZBufferCompare).
  if (state == ToUL(LegacyRenderState::kZFunc)) {
    switch (value) {
      case ToUL(LegacyCompareFunc::kNever):
        z_buffer_cmp_ = zRND_ZBUFFER_CMP_NEVER;
        break;
      case ToUL(LegacyCompareFunc::kLess):
        z_buffer_cmp_ = zRND_ZBUFFER_CMP_LESS;
        break;
      case ToUL(LegacyCompareFunc::kAlways):
        z_buffer_cmp_ = zRND_ZBUFFER_CMP_ALWAYS;
        break;
      case ToUL(LegacyCompareFunc::kLessEqual):
      default:
        z_buffer_cmp_ = zRND_ZBUFFER_CMP_LESS_EQUAL;
        break;
    }
    active_status_.zfunc = z_buffer_cmp_;
  }

  if (impl_) {
    // For a small, high-traffic subset of D3D9-style states, translate here into
    // typed descriptors and apply via the D3D11-first APIs on D3D11RendererImpl.
    // Anything not covered falls back to the existing compat layer.
    auto map_blend_factor = [](unsigned long d3d_blend) -> gmp::renderer::d3d11::BlendFactor {
      using gmp::renderer::d3d11::BlendFactor;
      switch (d3d_blend) {
        case ToUL(LegacyBlend::kZero):
          return BlendFactor::kZero;
        case ToUL(LegacyBlend::kOne):
          return BlendFactor::kOne;
        case ToUL(LegacyBlend::kSrcColor):
          return BlendFactor::kSrcColor;
        case ToUL(LegacyBlend::kInvSrcColor):
          return BlendFactor::kInvSrcColor;
        case ToUL(LegacyBlend::kSrcAlpha):
          return BlendFactor::kSrcAlpha;
        case ToUL(LegacyBlend::kInvSrcAlpha):
          return BlendFactor::kInvSrcAlpha;
        case ToUL(LegacyBlend::kDestAlpha):
          return BlendFactor::kDestAlpha;
        case ToUL(LegacyBlend::kInvDestAlpha):
          return BlendFactor::kInvDestAlpha;
        case ToUL(LegacyBlend::kDestColor):
          return BlendFactor::kDestColor;
        case ToUL(LegacyBlend::kInvDestColor):
          return BlendFactor::kInvDestColor;
        default:
          return BlendFactor::kOne;
      }
    };

    auto map_compare = [](unsigned long d3d_cmp) -> gmp::renderer::d3d11::CompareFunc {
      using gmp::renderer::d3d11::CompareFunc;
      switch (d3d_cmp) {
        case ToUL(LegacyCompareFunc::kNever):
          return CompareFunc::kNever;
        case ToUL(LegacyCompareFunc::kLess):
          return CompareFunc::kLess;
        case ToUL(LegacyCompareFunc::kEqual):
          return CompareFunc::kEqual;
        case ToUL(LegacyCompareFunc::kLessEqual):
          return CompareFunc::kLessEqual;
        case ToUL(LegacyCompareFunc::kGreater):
          return CompareFunc::kGreater;
        case ToUL(LegacyCompareFunc::kNotEqual):
          return CompareFunc::kNotEqual;
        case ToUL(LegacyCompareFunc::kGreaterEqual):
          return CompareFunc::kGreaterEqual;
        case ToUL(LegacyCompareFunc::kAlways):
        default:
          return CompareFunc::kAlways;
      }
    };

    auto try_apply_native = [this, maxStates, &map_blend_factor, &map_compare](unsigned long changed_state) -> bool {
      using namespace gmp::renderer::d3d11;

      auto safe_get = [this, maxStates](unsigned long s, unsigned long default_value) -> unsigned long {
        if (s >= maxStates) {
          return default_value;
        }
        const unsigned long v = render_state_cache_[s];
        return (v == kCacheInvalidSentinel) ? default_value : v;
      };

      // Blend state depends on 3 states.
      if (changed_state == ToUL(LegacyRenderState::kAlphaBlendEnable) || changed_state == ToUL(LegacyRenderState::kSrcBlend) ||
          changed_state == ToUL(LegacyRenderState::kDestBlend)) {
        BlendDesc blend = {};
        blend.enable = safe_get(ToUL(LegacyRenderState::kAlphaBlendEnable), kFalse32) != kFalse32;
        blend.src_color = map_blend_factor(safe_get(ToUL(LegacyRenderState::kSrcBlend), ToUL(LegacyBlend::kSrcAlpha)));
        blend.dst_color = map_blend_factor(safe_get(ToUL(LegacyRenderState::kDestBlend), ToUL(LegacyBlend::kInvSrcAlpha)));
        blend.color_op = BlendOp::kAdd;

        // Set alpha blend factors to match the prebuilt blend states.
        // MUL mode (DestColor, Zero) uses DestAlpha, Zero for alpha blending.
        // This ensures shadows and other MUL-blended geometry render correctly.
        const bool is_mul_blend = (blend.src_color == BlendFactor::kDestColor && blend.dst_color == BlendFactor::kZero);
        const bool is_mul2_blend = (blend.src_color == BlendFactor::kDestColor && blend.dst_color == BlendFactor::kSrcColor);
        if (is_mul_blend) {
          blend.src_alpha = BlendFactor::kDestAlpha;
          blend.dst_alpha = BlendFactor::kZero;
        } else if (is_mul2_blend) {
          blend.src_alpha = BlendFactor::kDestAlpha;
          blend.dst_alpha = BlendFactor::kSrcAlpha;
        }
        // For other modes (BLEND, ADD), keep defaults (kOne, kZero) which matches
        // the prebuilt bs_alpha_blend and bs_alpha_add states.

        impl_->SetBlendDesc(blend);
        return true;
      }

      // Depth state depends on 3 states.
      if (changed_state == ToUL(LegacyRenderState::kZEnable) || changed_state == ToUL(LegacyRenderState::kZWriteEnable) ||
          changed_state == ToUL(LegacyRenderState::kZFunc)) {
        DepthDesc depth = {};
        depth.enable = safe_get(ToUL(LegacyRenderState::kZEnable), ToUL(LegacyZEnable::kTrue)) != ToUL(LegacyZEnable::kFalse);
        depth.write_enable = safe_get(ToUL(LegacyRenderState::kZWriteEnable), kTrue32) != kFalse32;
        depth.func = map_compare(safe_get(ToUL(LegacyRenderState::kZFunc), ToUL(LegacyCompareFunc::kLessEqual)));
        impl_->SetDepthDesc(depth);
        return true;
      }

      // Raster state depends on cull + fill.
      if (changed_state == ToUL(LegacyRenderState::kCullMode) || changed_state == ToUL(LegacyRenderState::kFillMode)) {
        RasterDesc raster = {};
        const unsigned long fill = safe_get(ToUL(LegacyRenderState::kFillMode), ToUL(LegacyFillMode::kSolid));
        raster.fill = (fill == ToUL(LegacyFillMode::kWireframe)) ? FillMode::kWireframe : FillMode::kSolid;

        const unsigned long cull = safe_get(ToUL(LegacyRenderState::kCullMode), ToUL(LegacyCullMode::kCW));
        raster.cull = (cull == ToUL(LegacyCullMode::kNone)) ? CullMode::kNone : CullMode::kBack;
        raster.front_ccw = true;
        impl_->SetRasterDesc(raster);
        return true;
      }

      // Alpha test state depends on enable, func, and ref.
      if (changed_state == ToUL(LegacyRenderState::kAlphaTestEnable) || changed_state == ToUL(LegacyRenderState::kAlphaFunc) ||
          changed_state == ToUL(LegacyRenderState::kAlphaRef)) {
        const bool enable = safe_get(ToUL(LegacyRenderState::kAlphaTestEnable), kFalse32) != kFalse32;
        const float ref = static_cast<float>(safe_get(ToUL(LegacyRenderState::kAlphaRef), 128)) / 255.0f;
        // Alpha func is baked into the shader - we always use GREATEREQUAL.
        impl_->SetAlphaTest(enable, ref);
        return true;
      }

      if (changed_state == ToUL(LegacyRenderState::kLighting)) {
        // D3D9 fixed-function: LIGHTING toggles material/light evaluation.
        // D3D11 emulation: select unlit UT_UL shaders when disabled.
        const bool enable = safe_get(ToUL(LegacyRenderState::kLighting), kFalse32) != kFalse32;
        impl_->SetLightingEnabled(enable);
        return true;
      }

      // States that are no-ops in D3D11 (handled by shaders or not applicable).
      if (changed_state == ToUL(LegacyRenderState::kDitherEnable) || changed_state == ToUL(LegacyRenderState::kClipping)) {
        // These are tracked in the cache but don't affect D3D11 rendering.
        return true;
      }

      // Fog states are handled via FogManager which sets shader constants.
      if (changed_state == ToUL(LegacyRenderState::kFogEnable) || changed_state == ToUL(LegacyRenderState::kFogColor) ||
          changed_state == ToUL(LegacyRenderState::kFogTableMode) || changed_state == ToUL(LegacyRenderState::kFogStart) ||
          changed_state == ToUL(LegacyRenderState::kFogEnd) || changed_state == ToUL(LegacyRenderState::kFogVertexMode) ||
          changed_state == ToUL(LegacyRenderState::kRangeFogEnable)) {
        return true;
      }

      return false;
    };

    const bool handled_by_native = try_apply_native(state);
    if (!handled_by_native) {
      // Log unhandled states for debugging - these should not occur in normal operation.
      SPDLOG_WARN("ApplyRenderState: Unhandled legacy render state {} = {}", state, value);
    }

    if (state == ToUL(LegacyRenderState::kFogEnable) && value == kFalse32) {
      auto invalidate = [this, maxStates](unsigned long target) {
        if (target < maxStates) {
          render_state_cache_[target] = kCacheInvalidSentinel;
        }
      };

      if (fog_manager_.IsRadialEnabled()) {
        invalidate(ToUL(LegacyRenderState::kFogVertexMode));
        invalidate(ToUL(LegacyRenderState::kRangeFogEnable));
      } else {
        invalidate(ToUL(LegacyRenderState::kFogTableMode));
      }

      invalidate(ToUL(LegacyRenderState::kFogColor));
      invalidate(ToUL(LegacyRenderState::kFogStart));
      invalidate(ToUL(LegacyRenderState::kFogEnd));
    }
  }

  return true;
}

void zCRnd_D3D_DX11::AddAlphaSortObject(zCRndAlphaSortObject* obj) {
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
  if (bucket >= kMaxBuckets) {
    bucket = kMaxBuckets - 1;
  }
  if (bucket < 0) {
    bucket = 0;
  }

  // If bucket is empty, just insert
  if (alpha_sort_bucket_[bucket] == nullptr) {
    obj->nextSortObject = nullptr;  // Direct member access
    alpha_sort_bucket_[bucket] = obj;
    return;
  }

  // Larger Z = farther from camera. For painter's algorithm (back-to-front),
  // we render larger Z first. Within each bucket, sort so larger Z is at head.

  // If this object is farther (larger Z) than the first entry, insert at head
  if (alpha_sort_bucket_[bucket]->zvalue <= zvalue) {
    obj->nextSortObject = alpha_sort_bucket_[bucket];  // Direct member access
    alpha_sort_bucket_[bucket] = obj;
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
bool zCRnd_D3D_DX11::ApplyAlphaBlendState(gmp::renderer::d3d11::AlphaBlendFunc blend_func, bool has_texture, bool texture_has_alpha) {
  using namespace gmp::renderer::d3d11;

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

    case AlphaBlendFunc::kTest:
      // Alpha test only: punch-through cutouts (nets/leaves). Still writes Z.
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_ALPHAOP, zRND_TOP_SELECTARG1);
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_TEST);
      break;

    case AlphaBlendFunc::kBlendTest:
      // Alpha test + alpha blend. Used by some cutout materials that should still
      // blend their edges while discarding fully transparent pixels.
      if (has_texture) {
        SetTextureStageState(0, zRND_TSS_ALPHAOP, texture_has_alpha ? zRND_TOP_MODULATE : zRND_TOP_SELECTARG2);
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
      }
      SetAlphaBlendFuncImmed(zRND_ALPHA_FUNC_BLEND_TEST);
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

// SetupAlphaRenderState - Configures D3D11 state for a batch of alpha polygons.
//
// This sets up all the render state needed for the current batch, including:
// - Texture and sampler state
// - Blend function
// - Z buffer configuration
// - Texture stage operations
//
void zCRnd_D3D_DX11::SetupAlphaRenderState(const gmp::renderer::d3d11::AlphaRenderStateKey& state) {
  using namespace gmp::renderer::d3d11;

  // Setup common alpha state
  ResetMultiTexturing();
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaTestEnable), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kZWriteEnable), kFalse32);

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
    const auto address_mode = state.texture_wrap ? AddressMode::kWrap : AddressMode::kClamp;
    impl_->SetSamplerAddressing(0, address_mode, address_mode);
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
void zCRnd_D3D_DX11::FlushAlphaBatch() {
  using namespace gmp::renderer::d3d11;

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
  const bool has_texture = (state.texture != nullptr);
  // Pass the z_func directly so the impl can select the correct depth stencil state
  impl_->DrawAlphaBatch(reinterpret_cast<const VertexRHW*>(vertices), vertex_count, indices, index_count, has_texture,
                        static_cast<int>(state.z_func));

  alpha_batcher_.MarkBatchRendered();
}

// RenderAlphaPolyBatched - Submits a polygon to the batcher.
//
// If the polygon can be added to the current batch (same render state),
// it's accumulated. If not, the current batch is flushed first.
//
void zCRnd_D3D_DX11::RenderAlphaPolyBatched(const gmp::renderer::d3d11::QueuedAlphaPoly& poly) {
  // Try to add to current batch
  if (!alpha_batcher_.Submit(poly)) {
    // Batch is full or state changed - flush current batch
    FlushAlphaBatch();
    // Now submit the poly (starts a new batch)
    alpha_batcher_.Submit(poly);
  }
}

// DrawQueuedAlphaPoly - renders a QueuedAlphaPoly using D3D11
// Note: This function is still used for immediate alpha polys (FlushAlphaPolys).
// For the sorted alpha pass, we use the batched rendering path instead.
void zCRnd_D3D_DX11::DrawQueuedAlphaPoly(const gmp::renderer::d3d11::QueuedAlphaPoly* ap) {
  using namespace gmp::renderer::d3d11;

  if (!ap || ap->vert_count < 3 || !impl_ || GetSurfaceLost()) {
    return;
  }

  // Setup render state from the poly's state key
  AlphaRenderStateKey state = AlphaRenderStateKey::FromPoly(*ap);
  SetupAlphaRenderState(state);

  // Draw the polygon - pass z_func directly so impl can select correct depth stencil state
  static_assert(sizeof(AlphaVertex) == sizeof(VertexRHW), "AlphaVertex must match VertexRHW size");
  impl_->DrawTriangleFan(reinterpret_cast<const VertexRHW*>(ap->verts.data()), ap->vert_count, static_cast<int>(ap->z_func));
}

// Renders all translucent (alpha-blended) geometry back-to-front.
// This renderer keeps translucent drawables in per-depth buckets ("alpha sort" list)
// so blending composes correctly: objects farther from the camera must draw before
// nearer ones because the depth buffer cannot resolve semi-transparent layers.
// This pass walks buckets from far to near and interleaves two sources of alpha
// content per bucket: engine-managed alpha sort objects (e.g., water surfaces)
// and queued alpha polys (e.g., particles). Alpha polys are batched to reduce
// draw calls, while alpha sort objects are rendered immediately. The ordering
// preserves correct visual blending while keeping state changes minimal.
void zCRnd_D3D_DX11::RenderAlphaSortList() {
  using namespace gmp::renderer::d3d11;

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

  // Finalize batching (End returns bool for final batch, which we already flushed)
  (void)alpha_batcher_.End();
  alpha_batcher_.ResetStats();

  // Clear the alpha poly queue (buckets are already cleared in the loop above)
  alpha_poly_queue_.Reset();

  num_alpha_polys_ = 0;
}

// RestoreOpaqueRenderState - Restores render state for opaque geometry after alpha pass.
//
// Called by ScopedAlphaRenderPass destructor to reset render state to defaults
// suitable for subsequent opaque geometry rendering.
void zCRnd_D3D_DX11::RestoreOpaqueRenderState() {
  // Restore depth buffer state
  ApplyRenderState(ToUL(LegacyRenderState::kZWriteEnable), z_buffer_write_enabled_ ? kTrue32 : kFalse32);
  SetZBufferCompare(z_buffer_cmp_);

  // Restore blend and dither state
  ApplyRenderState(ToUL(LegacyRenderState::kDitherEnable), dither_enabled_ ? kTrue32 : kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaBlendEnable), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kAlphaTestEnable), kFalse32);

  // Restore texture stage states
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  SetTextureStageState(0, zRND_TSS_COLORARG1, zRND_TA_TEXTURE);
  SetTextureStageState(0, zRND_TSS_COLORARG2, zRND_TA_CURRENT);

  // Restore texture address mode from the cached stage-state.
  // Some textures (notably sky domes) require wrap U + clamp V.
  ApplyTextureAddressMode();

  // Clear texture and material state
  SetTexture(0, nullptr);
  active_material_ = nullptr;
}

// ResetStateAfterAlphaSortObjects - Resets render state after alpha sort object rendering.
//
// Called after rendering alpha sort objects (like water) to prepare for
// subsequent alpha poly rendering (like fire particles). The Draw() calls may have
// changed texture, blend state, or other render state that needs to be reset.
void zCRnd_D3D_DX11::ResetStateAfterAlphaSortObjects() {
  SetTexture(0, nullptr);
  SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
  ApplyRenderState(ToUL(LegacyRenderState::kClipping), kFalse32);
  ApplyRenderState(ToUL(LegacyRenderState::kCullMode), ToUL(LegacyCullMode::kNone));
}

// DrawVertexBuffer - Primary rendering path for batched world geometry.
//
// Gothic's zCRenderManager batches polygons via PackVB() and renders them through this
// function. In D3D11, we emulate Gothic's T&L-style rendering using vertex shaders that
// perform transformation and lighting (replacing the old fixed-function hardware T&L).
// This is the dominant path for static world rendering.
//
// Vertex types handled:
// - zVBUFFER_VERTTYPE_UT_UL: Untransformed, Unlit - vertex shaders apply transform & lighting
// - zVBUFFER_VERTTYPE_UT_L: Untransformed, Lit - pre-lit vertex colors, shaders only transform
// - zVBUFFER_VERTTYPE_T_L: Transformed, Lit - already in screen space (2D elements, no transform)
int zCRnd_D3D_DX11::DrawVertexBuffer(zCVertexBuffer* vertex_buffer, int first_vert, int num_vert, unsigned short* index_list,
                                     unsigned long num_index) {
  if (vertex_buffer == nullptr || impl_ == nullptr) {
    return 0;
  }

  // Block rendering during device reset.
  if (GetSurfaceLost()) {
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

  auto* vb_d3d = static_cast<zCVertexBuffer_D3D11*>(vertex_buffer);
  if (vb_d3d == nullptr) {
    return 0;
  }

  ID3D11Buffer* buffer = vb_d3d->GetBuffer();
  if (buffer == nullptr) {
    SPDLOG_WARN("DrawVertexBuffer: Vertex buffer has no D3D11 buffer");
    return 0;
  }

  D3D11_PRIMITIVE_TOPOLOGY topology;
  if (!ConvertPrimitiveType(vb_d3d->GetPrimitiveType(), topology)) {
    SPDLOG_WARN("DrawVertexBuffer: Unsupported primitive type {}", static_cast<int>(vb_d3d->GetPrimitiveType()));
    return 0;
  }

  using namespace gmp::renderer::d3d11;  // For native render state types.

  // Index list selection:
  // - zCRenderManager always provides indices (modern path, 99% of calls)
  const unsigned long index_list_size = (num_index > 0) ? num_index : vb_d3d->GetIndexListSize();
  unsigned short* const index_list_ptr = (num_index > 0) ? index_list : vb_d3d->GetIndexListPtr();

  // Configure render state based on vertex type.
  int lighting_on = 0;
  int clipping_on = 0;
  unsigned long culling = ToUL(LegacyCullMode::kNone);

  switch (vb_d3d->GetVertexType()) {
    case zVBUFFER_VERTTYPE_UT_UL:
      // Untransformed, Unlit - needs hardware lighting.
      lighting_on = 1;
      clipping_on = 1;
      culling = ToUL(LegacyCullMode::kCW);
      break;
    case zVBUFFER_VERTTYPE_UT_L:
      // Untransformed, Lit - pre-lit vertex colors.
      lighting_on = 0;
      clipping_on = 1;
      culling = ToUL(LegacyCullMode::kCW);
      break;
    case zVBUFFER_VERTTYPE_T_L:
    default:
      // Transformed, Lit - already in screen space.
      lighting_on = 0;
      clipping_on = 0;
      culling = ToUL(LegacyCullMode::kNone);
      break;
  }

  ApplyRenderState(ToUL(LegacyRenderState::kLighting), lighting_on);
  ApplyRenderState(ToUL(LegacyRenderState::kClipping), clipping_on);
  ApplyRenderState(ToUL(LegacyRenderState::kCullMode), culling);

  // Stage 1 activity and lightmap detection.
  // Gothic uses stage 1 for multiple things (true lightmaps, env maps, and sometimes even base textures).
  // We compute a conservative heuristic later (after base binding) to avoid misclassifying cases where
  // stage1 is actually the base texture.
  // NOTE: tex_stage_state_cache_ stores Gothic zRND_TOP_* values (zRenderer.h), not D3DTOP_*.
  auto get_stage_tex_name = [](zCTexture* tex) -> const char* {
    if (!tex)
      return "null";
    const zSTRING n = tex->GetObjectName();
    const char* c = n.ToChar();
    return (c && c[0]) ? c : "";
  };

  // Prefer checking the resolved ani texture, since the engine often stores a wrapper on the stage.
  zCTexture* t1_raw = active_texture_[1];
  zCTexture* t1_ani = t1_raw ? t1_raw->GetAniTexture() : nullptr;
  const bool stage1_is_lightmap = ((t1_raw && t1_raw->IsLightmap() != 0) || (t1_ani && t1_ani->IsLightmap() != 0));

  // Stage 2 can be used on some world batches (e.g. when stage1 is repurposed for base texture).
  zCTexture* t2_raw = active_texture_[2];
  zCTexture* t2_ani = t2_raw ? t2_raw->GetAniTexture() : nullptr;
  const bool stage2_is_lightmap = ((t2_raw && t2_raw->IsLightmap() != 0) || (t2_ani && t2_ani->IsLightmap() != 0));

  // Debug log stage 2 state (some world batches use stage2 for lightmaps when stage1 is repurposed).

  if (active_texture_[1] != nullptr) {
    const auto stage1_color_op = tex_stage_state_cache_[1][zRND_TSS_COLOROP];
    // Some paths temporarily set stage1 to SELECTARG1; normalize to MODULATE for our dual-texture handling.
    if (stage1_color_op == zRND_TOP_SELECTARG1) {
      SetTextureStageState(1, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
    }
  }

  // IMPORTANT: The VB path can be multipass (base pass + lightmap/detail passes).
  // For multiplicative/additive passes, stage0 is the *effect* texture and must not be overridden by
  // our "find a base texture" fallback logic. Otherwise we end up re-binding the base texture again
  // and the intended lightmap/darkening pass never happens.
  const bool is_multiply_pass = (alpha_blend_func_ == zRND_ALPHA_FUNC_MUL || alpha_blend_func_ == zRND_ALPHA_FUNC_MUL2);
  const bool is_add_pass = (alpha_blend_func_ == zRND_ALPHA_FUNC_ADD);
  const bool is_effect_pass = is_multiply_pass || is_add_pass;

  // If available, prefer the engine's own semantic shader info over texture-name heuristics.
  // This is populated by a hook on zCRenderManager::DrawVertexBuffer(..., zCShader*).
  const auto shader_semantic = (!is_effect_pass) ? gmp::renderer::d3d11::ShaderSemanticBridge::TryGetCurrent() : std::nullopt;
  const bool has_shader_semantic = shader_semantic.has_value() && shader_semantic->valid;
  const int sem_base_stage = has_shader_semantic ? shader_semantic->base_stage_index : -1;
  const int sem_lightmap_stage = has_shader_semantic ? shader_semantic->lightmap_stage_index : -1;

  // DIAGNOSTIC: detect invalid semantic configuration where base and lightmap point to the same stage.
  // This would cause both slots to bind the same texture, resulting in broken lightmap rendering.
  if (has_shader_semantic && shader_semantic->has_lightmap && sem_base_stage == sem_lightmap_stage && sem_base_stage >= 0) {
    static int warned_same_stage = 0;
    if (warned_same_stage < 3) {
      ++warned_same_stage;
      SPDLOG_ERROR("[SEMANTIC BUG] sem_base_stage == sem_lightmap_stage == {} (both point to same texture stage!)", sem_base_stage);
    }
  }

  // Bind stage 0 SRV for the VB path.
  // Some batches (notably fmt=0x29 stride=32 indoors) appear to carry the base texture on stage 1.
  // Also, stage0 can be non-null but not a D3D11 texture (dynamic_cast fails), which would produce a white fallback.
  // Strategy: try binding stage0 as base; if that fails, use the active material's texture; finally fall back
  // to stage1 when it's NOT a lightmap.
  auto has_non_empty_name = [](zCTexture* tex) -> bool {
    if (tex == nullptr) {
      return false;
    }
    const zSTRING n = tex->GetObjectName();
    const char* c = n.ToChar();
    return (c != nullptr && c[0] != '\0');
  };

  auto is_valid_base_candidate = [&](zCTexture* tex) -> bool {
    if (tex == nullptr) {
      return false;
    }
    if (has_non_empty_name(tex)) {
      return true;
    }
    zCTexture* ani = tex->GetAniTexture();
    return has_non_empty_name(ani);
  };

  // Tracks which D3D11 texture we actually bound as the base texture for this VB draw.
  zCTex_D3D11* bound_base_d3d11 = nullptr;

  auto try_bind_base_to_stage0 = [&](zCTexture* tex) -> bool {
    if (tex == nullptr) {
      return false;
    }
    zCTexture* resolved = tex->GetAniTexture();
    if (resolved == nullptr) {
      resolved = tex;
    }
    zCTex_D3D11* d3d11_tex = dynamic_cast<zCTex_D3D11*>(resolved);
    if (d3d11_tex == nullptr) {
      return false;
    }
    if (!d3d11_tex->EnsureSRV(gmp::renderer::d3d11::g_D3D11Device)) {
      return false;
    }
    ID3D11ShaderResourceView* srv = d3d11_tex->GetSRV();
    if (srv == nullptr) {
      return false;
    }

    // Debug: Check if the texture content is actually valid
    if (!d3d11_tex->IsReadyForRendering()) {
      static int warned_base_not_ready = 0;
      if (warned_base_not_ready < 20) {
        ++warned_base_not_ready;
        const char* name = resolved->GetObjectName();
        SPDLOG_WARN(
            "try_bind_base_to_stage0: Texture '{}' has SRV but IsReadyForRendering()=false! "
            "This will cause black textures.",
            name ? name : "<unnamed>");
      }
    }

    impl_->SetTexture(0, srv);
    bound_base_d3d11 = d3d11_tex;
    return true;
  };

  // For effect passes (MUL/MUL2/ADD), honor stage0 exactly as the engine set it.
  if (is_effect_pass) {
    const bool bound = try_bind_base_to_stage0(active_texture_[0]);
    if (!bound) {
      impl_->SetTexture(0, nullptr);
    }
  }
  const bool stage0_name_ok = is_valid_base_candidate(active_texture_[0]);
  const bool stage1_name_ok = is_valid_base_candidate(active_texture_[1]);
  bool bound_base = false;
  bool base_from_stage0 = false;
  bool base_from_stage1 = false;
  bool base_from_material = false;
  bool base_from_semantic = false;
  int semantic_base_stage_bound = -1;

  // Authoritative base binding: if the semantic bridge is active and tells us which stage is the BASE,
  // bind that stage's texture into our shader slot 0.
  // Only fall back to heuristics if the semantic stage can't be bound (e.g. non-D3D11 texture, missing SRV).
  if (!is_effect_pass && has_shader_semantic && shader_semantic->has_lightmap && sem_base_stage >= 0 && sem_base_stage < 8) {
    if (active_texture_[sem_base_stage] != nullptr) {
      bound_base = try_bind_base_to_stage0(active_texture_[sem_base_stage]);
      base_from_semantic = bound_base;
      semantic_base_stage_bound = bound_base ? sem_base_stage : -1;
      if (bound_base && sem_base_stage == 0) {
        base_from_stage0 = true;
      }
      if (bound_base && sem_base_stage == 1) {
        base_from_stage1 = true;
      }
    }

    static bool warned_semantic_base_bind_fail = false;
    if (!bound_base && !warned_semantic_base_bind_fail) {
      warned_semantic_base_bind_fail = true;
      SPDLOG_WARN("Semantic lightmap draw: failed to bind BASE from semantic stage {}; falling back to heuristic base selection. t_sem='{}' ani='{}'",
                  sem_base_stage, get_stage_tex_name(active_texture_[sem_base_stage]),
                  get_stage_tex_name(active_texture_[sem_base_stage] ? active_texture_[sem_base_stage]->GetAniTexture() : nullptr));
    }
  }

  // Heuristic base binding: only used when we couldn't bind via the semantic stage.
  // (We keep this as a safety net for non-semantic draws or missing/unbindable semantic textures.)
  bool allow_stage0_as_lightmap_source = false;
  if (!is_effect_pass && !bound_base) {
    // Some indoor/world batches appear to carry an unnamed texture on stage0 (often NOT the real diffuse), while
    // stage1 carries a named texture but is configured to use UV1 + MODULATE (lightmap-like state).
    // In this specific case, treating stage1 as the lightmap (our heuristic) breaks textures completely.
    // Instead, prefer stage1 as the base texture and (optionally) treat stage0 as the lightmap source.
    bool prefer_stage1_as_base_over_unnamed_stage0 = false;
    {
      const auto stride_dbg = vb_d3d->GetVertexStride();
      const auto format_dbg = vb_d3d->GetVertexFormat();
      const int vertex_type_dbg = static_cast<int>(vb_d3d->GetVertexType());
      const bool geom_dbg_ok = (vertex_type_dbg == zVBUFFER_VERTTYPE_UT_L && stride_dbg == 32);
      const auto op1_dbg = tex_stage_state_cache_[1][zRND_TSS_COLOROP];
      const bool stage1_active_dbg = (op1_dbg != zRND_TOP_DISABLE);
      const bool stage1_op_is_multiply_dbg = (op1_dbg == zRND_TOP_MODULATE || op1_dbg == zRND_TOP_MODULATE2X || op1_dbg == zRND_TOP_MODULATE4X);
      const unsigned long tci1_dbg = (tex_stage_state_cache_[1][zRND_TSS_TEXCOORDINDEX] & 0x7);

      if (geom_dbg_ok && format_dbg == 0x29 && stage1_active_dbg && stage1_op_is_multiply_dbg && tci1_dbg == 1 && !stage0_name_ok && stage1_name_ok &&
          active_texture_[1] != nullptr && !stage1_is_lightmap) {
        prefer_stage1_as_base_over_unnamed_stage0 = true;
        allow_stage0_as_lightmap_source = (active_texture_[0] != nullptr);
      }
    }

    if (prefer_stage1_as_base_over_unnamed_stage0) {
      bound_base = try_bind_base_to_stage0(active_texture_[1]);
      base_from_stage1 = bound_base;
      if (bound_base) {
        static bool warned_stage_swap = false;
        if (!warned_stage_swap) {
          warned_stage_swap = true;
          SPDLOG_WARN(
              "DrawVB fmt=0x29 stride=32: stage0 is unnamed but stage1 looks lightmap-like; using stage1 as BASE and considering stage0 as LIGHTMAP "
              "source. t0='{}' t1='{}'",
              get_stage_tex_name(active_texture_[0]), get_stage_tex_name(active_texture_[1]));
        }
        // Ensure the shader samples the base from UV0.
        SetTextureStageState(0, zRND_TSS_TEXCOORDINDEX, 0);
      }
    }

    if (!bound_base) {
      bound_base = try_bind_base_to_stage0(active_texture_[0]);
      base_from_stage0 = bound_base;
    }
  }

  // If stage0 couldn't be bound, try the current material's texture.
  zCTexture* material_tex = nullptr;
  if (!is_effect_pass && !bound_base && active_material_ != nullptr) {
    material_tex = active_material_->GetAniTexture();
    if (is_valid_base_candidate(material_tex)) {
      bound_base = try_bind_base_to_stage0(material_tex);
      base_from_material = bound_base;
    }
  }

  if (!is_effect_pass && !bound_base && active_texture_[1] != nullptr && !stage1_is_lightmap) {
    const unsigned long stage1_tci_for_base = (tex_stage_state_cache_[1][zRND_TSS_TEXCOORDINDEX] & 0x7);
    // Historically we avoided using stage1 as base when it referenced UV1 because that is often a lightmap.
    // However, in practice some indoor/world batches arrive with stale TEXCOORDINDEX while stage0 is missing
    // or unbindable. In that case, refusing stage1-as-base makes the whole surface sample "null" tex0.
    // We only allow this fallback when stage1 is NOT flagged as a lightmap.

    bound_base = try_bind_base_to_stage0(active_texture_[1]);
    base_from_stage1 = bound_base;
  }
  if (!is_effect_pass && !bound_base) {
    impl_->SetTexture(0, nullptr);
    bound_base_d3d11 = nullptr;
  }

  // If we had to synthesize the base texture (not from stage0), force stage0 to use UV0.
  // In D3D9, world diffuse almost always uses TEXCOORD0, while TEXCOORD1 is typically lightmap.
  // When the engine bypasses stage-state updates, TEXCOORDINDEX can be stale and cause "blobby" indoor artifacts.
  if (base_from_stage1 || base_from_material || (base_from_semantic && semantic_base_stage_bound != 0) || (!base_from_stage0 && !stage0_name_ok)) {
    SetTextureStageState(0, zRND_TSS_TEXCOORDINDEX, 0);
  }

  // Indoor/world shading relies heavily on vertex colors (pre-lit) and/or lightmaps.
  // If stage0 COLOROP is left at SELECTARG1 (texture only) due to stale state, vertex colors are ignored,
  // which removes most "shadows"/baked lighting. This shows up most on fmt=0x29 stride=32.
  if (!is_effect_pass) {
    // IMPORTANT: When the semantic bridge says this batch uses a lightmap shader, engine stage0 is
    // often the lightmap stage (SELECTARG1) and stage1 is the base stage. Forcing stage0 into
    // MODULATE in that case incorrectly multiplies vertex colors into the base pass and can make
    // indoor lightmaps significantly darker than reference.
    const bool semantic_lightmap_active = (has_shader_semantic && shader_semantic->has_lightmap);

    const auto stride_dbg = vb_d3d->GetVertexStride();
    const auto format_dbg = vb_d3d->GetVertexFormat();
    if (format_dbg == 0x29 && stride_dbg == 32) {
      const auto op0 = tex_stage_state_cache_[0][zRND_TSS_COLOROP];
      // Only force MODULATE when we actually have a base texture bound. If stage0 is null/unbound,
      // forcing MODULATE can turn intended prelit (vertex-color-only) draws into black.
      if (!semantic_lightmap_active && bound_base && op0 == zRND_TOP_SELECTARG1) {
        SetTextureStageState(0, zRND_TSS_COLOROP, zRND_TOP_MODULATE);
      }
    }
  }

  // Decide whether stage1 should be treated as a lightmap.
  // Primary signal: texture reports IsLightmap() or shader semantics indicate lightmap.
  const auto stage1_color_op_effective = tex_stage_state_cache_[1][zRND_TSS_COLOROP];
  const bool stage1_active = (stage1_color_op_effective != zRND_TOP_DISABLE);
  const bool stage1_op_is_multiply = (stage1_color_op_effective == zRND_TOP_MODULATE || stage1_color_op_effective == zRND_TOP_MODULATE2X ||
                                      stage1_color_op_effective == zRND_TOP_MODULATE4X);

  const auto stage2_color_op_effective = tex_stage_state_cache_[2][zRND_TSS_COLOROP];
  const bool stage2_active = (stage2_color_op_effective != zRND_TOP_DISABLE);

  bool has_lightmap = false;
  bool lightmap_from_stage0 = false;
  bool lightmap_from_stage2 = false;

  bool semantic_ffp_enabled = false;
  unsigned long semantic_s0_colorop = 0;
  unsigned long semantic_s0_colorarg1 = 0;
  unsigned long semantic_s0_colorarg2 = 0;
  unsigned long semantic_s1_colorop = 0;
  unsigned long semantic_s1_colorarg1 = 0;
  unsigned long semantic_s1_colorarg2 = 0;

  // Authoritative path: use ZenGin shader semantics when available.
  if (has_shader_semantic && shader_semantic->has_lightmap) {
    has_lightmap = true;
    lightmap_from_stage0 = (sem_lightmap_stage == 0);
    lightmap_from_stage2 = (sem_lightmap_stage == 2);

    // Capture engine stage state for accurate 2-stage fixed-function emulation in the lightmap PS.
    // Engine commonly uses stage0=LIGHTMAP and stage1=BASE.
    // NOTE: We only enable this path for the subset of ZenGin zRND_TOP/zRND_TA values we currently emulate.
    if (sem_lightmap_stage >= 0 && sem_base_stage >= 0) {
      const auto normalize_arg = [](unsigned long arg, bool is_arg1) {
        // Some paths don't touch COLORARG and cache entries remain invalid.
        // Use the fixed-function defaults:
        // - COLORARG1 = TEXTURE
        // - COLORARG2 = CURRENT
        if (arg == 0xFFFFFFFFu) {
          return is_arg1 ? static_cast<unsigned long>(zRND_TA_TEXTURE) : static_cast<unsigned long>(zRND_TA_CURRENT);
        }
        return arg;
      };
      const auto normalize_op = [](unsigned long op) {
        // If unset, assume MODULATE.
        if (op == 0xFFFFFFFFu) {
          return static_cast<unsigned long>(zRND_TOP_MODULATE);
        }
        return op;
      };

      const auto is_supported_op = [](unsigned long op) {
        // ZenGin zRND_TOP subset implemented in HLSL.
        return (op == static_cast<unsigned long>(zRND_TOP_SELECTARG1)) || (op == static_cast<unsigned long>(zRND_TOP_SELECTARG2)) ||
               (op == static_cast<unsigned long>(zRND_TOP_MODULATE)) || (op == static_cast<unsigned long>(zRND_TOP_MODULATE2X)) ||
               (op == static_cast<unsigned long>(zRND_TOP_MODULATE4X)) || (op == static_cast<unsigned long>(zRND_TOP_ADD));
      };
      const auto is_supported_arg = [](unsigned long arg) {
        // ZenGin zRND_TA subset implemented in HLSL.
        return (arg == static_cast<unsigned long>(zRND_TA_CURRENT)) || (arg == static_cast<unsigned long>(zRND_TA_TEXTURE)) ||
               (arg == static_cast<unsigned long>(zRND_TA_DIFFUSE)) || (arg == static_cast<unsigned long>(zRND_TA_TFACTOR));
      };

      semantic_s0_colorop = normalize_op(tex_stage_state_cache_[sem_lightmap_stage][zRND_TSS_COLOROP]);
      semantic_s0_colorarg1 = normalize_arg(tex_stage_state_cache_[sem_lightmap_stage][zRND_TSS_COLORARG1], true);
      semantic_s0_colorarg2 = normalize_arg(tex_stage_state_cache_[sem_lightmap_stage][zRND_TSS_COLORARG2], false);
      semantic_s1_colorop = normalize_op(tex_stage_state_cache_[sem_base_stage][zRND_TSS_COLOROP]);
      semantic_s1_colorarg1 = normalize_arg(tex_stage_state_cache_[sem_base_stage][zRND_TSS_COLORARG1], true);
      semantic_s1_colorarg2 = normalize_arg(tex_stage_state_cache_[sem_base_stage][zRND_TSS_COLORARG2], false);

      semantic_ffp_enabled = is_supported_op(semantic_s0_colorop) && is_supported_op(semantic_s1_colorop) &&
                             is_supported_arg(semantic_s0_colorarg1) && is_supported_arg(semantic_s0_colorarg2) &&
                             is_supported_arg(semantic_s1_colorarg1) && is_supported_arg(semantic_s1_colorarg2);

      static bool warned_semantic_ffp_disabled = false;
      if (!semantic_ffp_enabled && !warned_semantic_ffp_disabled) {
        warned_semantic_ffp_disabled = true;
        SPDLOG_WARN(
            "Semantic FFP lightmap emulation disabled (unsupported COLORARG/COLOROP); falling back to heuristic combine. s0(op={},a1={},a2={}) "
            "s1(op={},a1={},a2={})",
            semantic_s0_colorop, semantic_s0_colorarg1, semantic_s0_colorarg2, semantic_s1_colorop, semantic_s1_colorarg1, semantic_s1_colorarg2);
      }
    }

    // Our dual-texture shader expects:
    // - slot/stage 0: BASE (diffuse)
    // - slot/stage 1: LIGHTMAP
    //
    // ZenGin's common ordering for world lightmaps is:
    // - engine stage 0: LIGHTMAP
    // - engine stage 1: BASE, and stage1 COLOROP often carries MODULATE2X/4X brightening.
    //
    // IMPORTANT: Do NOT override stage0 COLOROP here.
    // Engine stage COLOROP semantics are about combining with CURRENT, and reusing them for our
    // "base-vs-vertex" op can double-apply darkening/brightening and skew lighting.
    //
    // Instead, only:
    // - remap UV indices for our shader
    // - preserve the engine's *combine* COLOROP (usually on base_stage) for the post-multiply scaling.
    const unsigned long combine_color_op = (sem_base_stage >= 0) ? tex_stage_state_cache_[sem_base_stage][zRND_TSS_COLOROP] : zRND_TOP_MODULATE;

    // Our lightmap PS interprets "ColorOp0" as how to combine the BASE texture with vertex color.
    // When using semantic remapping (engine stage1=BASE, stage0=LIGHTMAP), copying engine stage0
    // state into shader-stage0 is wrong and can make indoor/world batches too dark.
    // Use rgbGen to decide whether vertex colors should be applied at all.
    const bool base_wants_vertex = (shader_semantic->base_rgb_gen == Gothic_II_Addon::zSHD_RGBGEN_VERTEX);
    SetTextureStageState(0, zRND_TSS_COLOROP, base_wants_vertex ? zRND_TOP_MODULATE : zRND_TOP_SELECTARG1);

    SetTextureStageState(0, zRND_TSS_TEXCOORDINDEX, static_cast<unsigned long>(shader_semantic->base_uv_index));
    SetTextureStageState(1, zRND_TSS_TEXCOORDINDEX, static_cast<unsigned long>(shader_semantic->lightmap_uv_index));
    SetTextureStageState(1, zRND_TSS_COLOROP, combine_color_op);

    static bool logged_semantic_lightmap = false;
    if (!logged_semantic_lightmap) {
      logged_semantic_lightmap = true;
      SPDLOG_WARN("Using shader semantics for lightmaps: base_stage={} lightmap_stage={} (fmt=0x{:X} stride={})", sem_base_stage, sem_lightmap_stage,
                  vb_d3d->GetVertexFormat(), vb_d3d->GetVertexStride());
    }
  } else {
    // Non-semantic path: rely solely on IsLightmap() texture flags.
    if (stage1_active && active_texture_[1] != nullptr) {
      has_lightmap = stage1_is_lightmap;
      if (has_lightmap && allow_stage0_as_lightmap_source && base_from_stage1) {
        // Stage swap case: stage1 was selected as base because stage0 looked suspicious.
        // If stage0 exists, treat it as the lightmap input.
        lightmap_from_stage0 = true;
      }
    }

    // Fallback: some batches put the base texture on stage1 and the lightmap on stage2.
    if (!has_lightmap && base_from_stage1 && stage2_active && active_texture_[2] != nullptr && stage2_is_lightmap) {
      has_lightmap = true;
      lightmap_from_stage2 = true;

      // Remap stage2's UV index + op into our shader's stage1 slots using native API.
      const unsigned long tci2_raw = (tex_stage_state_cache_[2][zRND_TSS_TEXCOORDINDEX] & 0x7);
      const auto tci2_source = (tci2_raw == 1) ? TexCoordSource::kUV1 : TexCoordSource::kUV0;
      impl_->SetTextureStageUVSource(1, tci2_source);

      // Convert stage2 color op to native CombineOp
      const auto native_op = [](unsigned long op) -> CombineOp {
        switch (op) {
          case zRND_TOP_DISABLE:
            return CombineOp::kDisable;
          case zRND_TOP_SELECTARG1:
            return CombineOp::kSelectArg1;
          case zRND_TOP_SELECTARG2:
            return CombineOp::kSelectArg2;
          case zRND_TOP_MODULATE:
            return CombineOp::kModulate;
          case zRND_TOP_MODULATE2X:
            return CombineOp::kModulate2X;
          case zRND_TOP_MODULATE4X:
            return CombineOp::kModulate4X;
          case zRND_TOP_ADD:
            return CombineOp::kAdd;
          default:
            return CombineOp::kModulate;
        }
      }(stage2_color_op_effective);
      impl_->SetTextureStageColorOp(1, native_op);

      static bool warned_stage2_lightmap = false;
      if (!warned_stage2_lightmap) {
        warned_stage2_lightmap = true;
        SPDLOG_WARN("Using stage2 as lightmap (remapped to shader stage1): t2='{}' ani='{}' op2={} tci2={}", get_stage_tex_name(t2_raw),
                    get_stage_tex_name(t2_ani), stage2_color_op_effective, tci2_raw);
      }
    }
  }

  // Pass lightmap decision to impl right before binding stage1 SRV.
  impl_->SetHasLightmap(has_lightmap);

  // Forward semantic base-stage parameters for the lightmap shader.
  if (has_shader_semantic && shader_semantic->has_lightmap) {
    impl_->SetSemanticBaseRgbGen(shader_semantic->base_rgb_gen);
    impl_->SetSemanticBaseColorFactor(shader_semantic->base_color_factor_argb);

    // Forward engine-stage combine parameters for accurate emulation.
    impl_->SetSemanticLightmapFfp(semantic_ffp_enabled, semantic_s0_colorop, semantic_s0_colorarg1, semantic_s0_colorarg2, semantic_s1_colorop,
                                  semantic_s1_colorarg1, semantic_s1_colorarg2);
  } else {
    // Disable semantic override for non-semantic / heuristic paths.
    impl_->SetSemanticBaseRgbGen(-1);
    impl_->SetSemanticBaseColorFactor(0xFFFFFFFFu);
    impl_->SetSemanticLightmapFfp(false, 0, 0, 0, 0, 0, 0);
  }

  // Bind stage 1 SRV only for true lightmaps.
  // NOTE: If we detected the lightmap on stage2, we still bind it into slot/stage 1 for the shader.
  zCTexture* lightmap_src = nullptr;
  int lightmap_src_stage = -1;
  bool lightmap_bound_ok = false;  // Track if lightmap successfully bound (when has_lightmap=true)

  if (has_lightmap) {
    // Authoritative path: when shader semantics are present, use the engine's declared lightmap stage.
    if (has_shader_semantic && shader_semantic->has_lightmap && sem_lightmap_stage >= 0 && sem_lightmap_stage < 8) {
      lightmap_src = active_texture_[sem_lightmap_stage];
      lightmap_src_stage = sem_lightmap_stage;

      // If active_texture_ is null but the shader semantic captured a lightmap texture directly, use that.
      // This can happen when the engine doesn't call SetTextureStage for the lightmap slot but the shader
      // stage has a texture pointer set internally.
      if (lightmap_src == nullptr && shader_semantic->lightmap_texture != nullptr) {
        lightmap_src = shader_semantic->lightmap_texture;
        lightmap_src_stage = sem_lightmap_stage;
      }

      static bool warned_semantic_lightmap_missing = false;
      if (lightmap_src == nullptr && !warned_semantic_lightmap_missing) {
        warned_semantic_lightmap_missing = true;
        SPDLOG_WARN("Semantic lightmap draw: semantic lightmap stage {} has no texture; falling back to heuristic lightmap selection.",
                    sem_lightmap_stage);
      }
    }

    // Fallback heuristic selection.
    if (lightmap_src == nullptr) {
      if (lightmap_from_stage0) {
        lightmap_src = active_texture_[0];
        lightmap_src_stage = 0;
      } else {
        lightmap_src = lightmap_from_stage2 ? active_texture_[2] : active_texture_[1];
        lightmap_src_stage = lightmap_from_stage2 ? 2 : 1;
      }
    }
  }

  if (has_lightmap && lightmap_src != nullptr) {
    // Stage bindings often store a wrapper texture; the actual GPU texture lives on GetAniTexture().
    zCTexture* t1_resolved = lightmap_src->GetAniTexture();
    if (t1_resolved == nullptr) {
      t1_resolved = lightmap_src;
    }
    zCTex_D3D11* tex11 = dynamic_cast<zCTex_D3D11*>(t1_resolved);
    if (tex11) {
      if (tex11->EnsureSRV(gmp::renderer::d3d11::g_D3D11Device)) {
        ID3D11ShaderResourceView* srv = tex11->GetSRV();
        if (srv) {
          // GUARD: Detect if lightmap SRV is the same as what we bound as base (bound_base_d3d11).
          // If they're the same, we'd multiply the texture by itself, producing darker output.
          if (bound_base_d3d11 && bound_base_d3d11->GetSRV() == srv) {
            static int warned_same_srv = 0;
            if (warned_same_srv < 5) {
              ++warned_same_srv;
              SPDLOG_ERROR(
                  "[LIGHTMAP BUG] Lightmap SRV == Base SRV! srv={} lightmap_src_stage={} "
                  "This will cause self-multiplication (darker/black textures)!",
                  (void*)srv, lightmap_src_stage);
            }
            // Don't bind - use white fallback to avoid darkening
            impl_->SetTexture(1, nullptr);
            lightmap_bound_ok = false;
          } else {
            impl_->SetTexture(1, srv);
            lightmap_bound_ok = true;  // Successfully bound a valid, distinct lightmap
          }
        } else {
          SPDLOG_WARN("Lightmap texture has no SRV");
          impl_->SetTexture(1, nullptr);
          lightmap_bound_ok = false;
        }
      } else {
        SPDLOG_WARN("Failed to ensure lightmap SRV");
        impl_->SetTexture(1, nullptr);
        lightmap_bound_ok = false;
      }
    } else {
      SPDLOG_WARN("Lightmap bind failed: src={} resolved={} name='{}'", (void*)lightmap_src, (void*)t1_resolved, get_stage_tex_name(t1_resolved));
      impl_->SetTexture(1, nullptr);
      lightmap_bound_ok = false;
    }
  } else if (has_lightmap && lightmap_src == nullptr) {
    // has_lightmap=true but no source texture found - this is a failure
    impl_->SetTexture(1, nullptr);
    lightmap_bound_ok = false;
  } else {
    impl_->SetTexture(1, nullptr);
    // No lightmap needed - this is fine (lightmap_bound_ok stays false but has_lightmap is also false)
  }

  // Execute draw call based on poly draw mode.
  int result = 1;
  const auto stride = vb_d3d->GetVertexStride();
  const auto format = vb_d3d->GetVertexFormat();

  // Convert Gothic vertex type to int for impl
  // zVBUFFER_VERTTYPE_UT_UL = 0, zVBUFFER_VERTTYPE_UT_L = 1, zVBUFFER_VERTTYPE_T_L = 2
  const int vertex_type = static_cast<int>(vb_d3d->GetVertexType());

  // Compute the alpha blend func used for the actual D3D11 draw call.
  // The VB path may bind textures via impl_->SetTexture(0, srv) directly, bypassing SetTexture() logic.
  // Upgrade TEST -> BLEND_TEST for smooth-alpha cutouts to match legacy behavior.
  int alpha_blend_func_for_draw = static_cast<int>(alpha_blend_func_);
  if (!is_effect_pass && bound_base_d3d11 != nullptr) {
    const unsigned long alpha_test_state = render_state_cache_[ToUL(LegacyRenderState::kAlphaTestEnable)];
    const bool alpha_test_enabled = (alpha_test_state != kFalse32 && alpha_test_state != kCacheInvalidSentinel);
    if (alpha_test_enabled && alpha_blend_func_ == zRND_ALPHA_FUNC_TEST && bound_base_d3d11->HasSmoothAlpha()) {
      alpha_blend_func_for_draw = static_cast<int>(zRND_ALPHA_FUNC_BLEND_TEST);
    }
  }

  // Skip draw if the base texture isn't properly bound for indoor/world geometry.
  // When the base texture is still loading (gpu_content_valid_ = false), the white fallback SRV
  // combined with dark prelit vertex colors produces nearly-black output.
  // It's better to skip the draw entirely (geometry will appear next frame when texture is ready).
  const bool is_indoor_world_geom = (format == 0x29 && stride == 32);
  if (is_indoor_world_geom && !is_effect_pass) {
    // Check 1: Do we have a bound base texture at all?
    if (bound_base_d3d11 == nullptr) {
      static int skipped_draws_no_base = 0;
      if (skipped_draws_no_base < 10) {
        ++skipped_draws_no_base;
        SPDLOG_WARN("[SKIP DRAW] Indoor geometry (fmt=0x29 stride=32) has no valid base texture - skipping to avoid black fallback");
      }
      return 1;  // Return success but don't draw
    }
    // Check 2: Is the base texture actually ready for rendering (not just SRV exists)?
    if (!bound_base_d3d11->IsReadyForRendering()) {
      static int skipped_draws_base_not_ready = 0;
      if (skipped_draws_base_not_ready < 10) {
        ++skipped_draws_base_not_ready;
        SPDLOG_WARN("[SKIP DRAW] Indoor geometry (fmt=0x29 stride=32) base texture not ready for rendering - skipping");
      }
      return 1;  // Return success but don't draw
    }
    // Check 3: If we need a lightmap but couldn't bind one, render without it rather than skipping.
    // Skipping causes completely black geometry which is worse than slightly incorrect lighting.
    // The lightmap will typically become available on the next frame as textures cache in.
    if (has_lightmap && !lightmap_bound_ok) {
      static int warned_no_lightmap = 0;
      if (warned_no_lightmap < 10) {
        ++warned_no_lightmap;
        SPDLOG_WARN("[NO LIGHTMAP] Indoor geometry (fmt=0x29 stride=32) needs lightmap but couldn't bind one - rendering without lightmap");
      }
      // Disable lightmap mode so shader doesn't try to multiply by non-existent texture
      has_lightmap = false;
      impl_->SetHasLightmap(false);
    }
  }

  switch (poly_draw_mode_) {
    case zRND_DRAW_WIRE:
    case zRND_DRAW_FLAT:
      // Skip wire/flat modes - not supported.
      break;

    case zRND_DRAW_MATERIAL_WIRE:
      // Draw material, skip wire overlay.
      if (!impl_->DrawVertexBuffer(buffer, stride, topology, first_vert, num_vert, index_list_ptr, index_list_size, vertex_type,
                                   alpha_blend_func_for_draw)) {
        result = 0;
      }
      ApplyRenderState(ToUL(LegacyRenderState::kZFunc), ToUL(LegacyCompareFunc::kLessEqual));
      break;

    default:
      if (!impl_->DrawVertexBuffer(buffer, stride, topology, first_vert, num_vert, index_list_ptr, index_list_size, vertex_type,
                                   alpha_blend_func_for_draw)) {
        SPDLOG_WARN("DrawVertexBuffer: Draw call failed");
        result = 0;
      }
  }

  ApplyRenderState(ToUL(LegacyRenderState::kLighting), 0);
  return result;
}

zCVertexBuffer* zCRnd_D3D_DX11::CreateVertexBuffer() {
  return new zCVertexBuffer_D3D11();
}

zCRenderer* __stdcall CreateDX11Renderer() {
  return new zCRnd_D3D_DX11();
}

void __fastcall ConstructDX11Renderer(void* mem) {
  if (mem) {
    ::new (mem) zCRnd_D3D_DX11();
  }
}

ID3D11Device* zCRnd_D3D_DX11::GetDevice() const {
  return impl_ ? impl_->device : nullptr;
}

ID3D11DeviceContext* zCRnd_D3D_DX11::GetContext() const {
  return impl_ ? impl_->context : nullptr;
}

void zCRnd_D3D_DX11::SetVSync(bool enable) {
  if (impl_) {
    impl_->vsync = enable;
  }
}

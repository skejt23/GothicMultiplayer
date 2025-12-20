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

#include <bit>
#include <cstddef>
#include <cstdint>

namespace gmp::renderer::d3d11 {

// D3D11-native state descriptors used by the renderer.
//
// These types intentionally avoid D3D9 constants and naming.
// They are small POD-ish structs suitable for hashing/caching into actual
// ID3D11* state objects.

enum class BlendFactor : std::uint8_t {
  kZero = 0,
  kOne,
  kSrcColor,
  kInvSrcColor,
  kSrcAlpha,
  kInvSrcAlpha,
  kDestColor,
  kInvDestColor,
  kDestAlpha,
  kInvDestAlpha,
};

enum class BlendOp : std::uint8_t {
  kAdd = 0,
  kSub,
  kRevSub,
  kMin,
  kMax,
};

// Renderer-owned light representation.
// This replaces D3DLIGHT9-shaped blobs for the D3D11 path.
//
// Matches how lights are consumed by GpuLight/LightCB:
// - type is encoded separately
// - position/range and direction are provided explicitly
// - attenuation is classic (const/linear/quadratic)
// - spot outer cone angle is stored (currently not used by default shaders)
enum class LightType : std::uint8_t {
  kOff = 0,
  kPoint = 1,
  kSpot = 2,
  kDirectional = 3,
};

struct Light {
  LightType type = LightType::kOff;

  float position_x = 0.0f;
  float position_y = 0.0f;
  float position_z = 0.0f;

  float direction_x = 0.0f;
  float direction_y = 0.0f;
  float direction_z = 0.0f;

  float range = 0.0f;

  // Diffuse color in linear space, normalized to [0,1].
  float diffuse_r = 0.0f;
  float diffuse_g = 0.0f;
  float diffuse_b = 0.0f;

  // 1 / (att0 + att1*d + att2*d^2)
  float attenuation0 = 0.0f;
  float attenuation1 = 0.0f;
  float attenuation2 = 0.0f;

  // Spot light outer cone angle in radians.
  float spot_outer_angle_rad = 0.0f;
};

struct BlendDesc {
  bool enable = false;
  bool alpha_to_coverage = false;

  BlendFactor src_color = BlendFactor::kOne;
  BlendFactor dst_color = BlendFactor::kZero;
  BlendOp color_op = BlendOp::kAdd;

  BlendFactor src_alpha = BlendFactor::kOne;
  BlendFactor dst_alpha = BlendFactor::kZero;
  BlendOp alpha_op = BlendOp::kAdd;

  // Bitmask of enabled channels for RT0; default RGBA.
  std::uint8_t write_mask = 0x0F;

  friend bool operator==(const BlendDesc& a, const BlendDesc& b) = default;
};

enum class CompareFunc : std::uint8_t {
  kNever = 0,
  kLess,
  kEqual,
  kLessEqual,
  kGreater,
  kNotEqual,
  kGreaterEqual,
  kAlways,
};

struct DepthDesc {
  bool enable = true;
  bool write_enable = true;
  CompareFunc func = CompareFunc::kLessEqual;

  friend bool operator==(const DepthDesc& a, const DepthDesc& b) = default;
};

enum class CullMode : std::uint8_t {
  kNone = 0,
  kFront,
  kBack,
};

enum class FillMode : std::uint8_t {
  kSolid = 0,
  kWireframe,
};

struct RasterDesc {
  FillMode fill = FillMode::kSolid;
  CullMode cull = CullMode::kBack;

  // D3D11 uses `FrontCounterClockwise`; keep this explicit and renderer-owned.
  bool front_ccw = false;

  // Bias is kept since the engine historically uses it for decals/overlays.
  std::int32_t depth_bias = 0;
  float slope_scaled_depth_bias = 0.0f;
  float depth_bias_clamp = 0.0f;

  bool scissor_enable = false;
  bool multisample_enable = false;

  friend bool operator==(const RasterDesc& a, const RasterDesc& b) = default;
};

enum class AddressMode : std::uint8_t {
  kWrap = 0,
  kMirror,
  kClamp,
};

enum class FilterMode : std::uint8_t {
  kPoint = 0,
  kLinear,
  kAnisotropic,
};

struct SamplerDesc {
  AddressMode address_u = AddressMode::kWrap;
  AddressMode address_v = AddressMode::kWrap;
  AddressMode address_w = AddressMode::kWrap;

  FilterMode filter = FilterMode::kLinear;
  // D3D9 has separate MIN/MAG and MIP filter controls. We model MIP filtering
  // as a simple point/linear toggle to build the correct D3D11 filter.
  bool mip_linear = true;
  std::uint8_t max_anisotropy = 16;

  friend bool operator==(const SamplerDesc& a, const SamplerDesc& b) = default;
};

// ---------------------------------------------------------------------------
// FFP Emulation Types - Renderer-owned semantic values for texture stage state.
// These replace D3D9-era numeric codes with explicit, meaningful enums.
// ---------------------------------------------------------------------------

// Texture coordinate source for a texture stage.
// Low bits of D3DTSS_TEXCOORDINDEX select which UV set to use.
enum class TexCoordSource : std::uint8_t {
  kUV0 = 0,        // Use UV set 0 (TEXCOORD0)
  kUV1 = 1,        // Use UV set 1 (TEXCOORD1)
  kGenerated = 2,  // Use texture coordinate generation (see TexGenMode)
};

// Texture coordinate generation mode.
// High bits of D3DTSS_TEXCOORDINDEX encode D3DTSS_TCI_* flags.
enum class TexGenMode : std::uint8_t {
  kNone = 0,               // No generation, use UV directly
  kCameraSpaceNormal,      // D3DTSS_TCI_CAMERASPACENORMAL - camera-space normals
  kCameraSpacePosition,    // D3DTSS_TCI_CAMERASPACEPOSITION - camera-space position
  kCameraSpaceReflection,  // D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR - reflection vector
  kSphereMap,              // Sphere-map style environment mapping fallback
};

// Texture stage combine operation.
// Maps to D3D9 D3DTOP_* and Gothic's zRND_TOP_* constants.
enum class CombineOp : std::uint8_t {
  kDisable = 0,             // D3DTOP_DISABLE / zRND_TOP_DISABLE
  kSelectArg1,              // D3DTOP_SELECTARG1 / zRND_TOP_SELECTARG1 (texture)
  kSelectArg2,              // D3DTOP_SELECTARG2 / zRND_TOP_SELECTARG2 (diffuse)
  kModulate,                // D3DTOP_MODULATE / zRND_TOP_MODULATE (tex * vtx)
  kModulate2X,              // D3DTOP_MODULATE2X / zRND_TOP_MODULATE2X (tex * vtx * 2)
  kModulate4X,              // D3DTOP_MODULATE4X / zRND_TOP_MODULATE4X (tex * vtx * 4)
  kAdd,                     // D3DTOP_ADD / zRND_TOP_ADD (tex + vtx)
  kAddSmooth = 10,          // D3DTOP_ADDSMOOTH / zRND_TOP_ADDSMOOTH
  kBlendDiffuseAlpha = 11,  // D3DTOP_BLENDDIFFUSEALPHA / zRND_TOP_BLENDDIFFUSEALPHA
};

// Texture stage combine argument source.
// Maps to D3D9 D3DTA_* and Gothic's zRND_TA_* constants.
enum class CombineArg : std::uint8_t {
  kCurrent = 0,  // D3DTA_CURRENT / zRND_TA_CURRENT - result of previous stage
  kDiffuse,      // D3DTA_DIFFUSE / zRND_TA_DIFFUSE - vertex color
  kTexture = 3,  // D3DTA_TEXTURE / zRND_TA_TEXTURE - sampled texture
  kTFactor,      // D3DTA_TFACTOR / zRND_TA_TFACTOR - D3DRS_TEXTUREFACTOR
  kSpecular,     // D3DTA_SPECULAR / zRND_TA_SPECULAR - vertex specular
};

// Per-stage texture state for FFP emulation.
struct TextureStageState {
  TexCoordSource uv_source = TexCoordSource::kUV0;
  TexGenMode texgen_mode = TexGenMode::kNone;

  CombineOp color_op = CombineOp::kModulate;
  CombineArg color_arg1 = CombineArg::kTexture;
  CombineArg color_arg2 = CombineArg::kDiffuse;

  CombineOp alpha_op = CombineOp::kModulate;
  CombineArg alpha_arg1 = CombineArg::kTexture;
  CombineArg alpha_arg2 = CombineArg::kDiffuse;

  bool transform_enabled = false;
  // Matrix stored separately (too large for POD equality).

  friend bool operator==(const TextureStageState& a, const TextureStageState& b) = default;
};

// Minimal hash helpers for state caches.
// Note: kept header-only so caches can use unordered_map without additional deps.
struct NativeRenderStateHash {
  static constexpr std::size_t HashCombine(std::size_t h, std::size_t v) {
    // Boost-style hash combine.
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
  }

  std::size_t operator()(const BlendDesc& d) const {
    std::size_t h = 0;
    h = HashCombine(h, static_cast<std::size_t>(d.enable));
    h = HashCombine(h, static_cast<std::size_t>(d.alpha_to_coverage));
    h = HashCombine(h, static_cast<std::size_t>(d.src_color));
    h = HashCombine(h, static_cast<std::size_t>(d.dst_color));
    h = HashCombine(h, static_cast<std::size_t>(d.color_op));
    h = HashCombine(h, static_cast<std::size_t>(d.src_alpha));
    h = HashCombine(h, static_cast<std::size_t>(d.dst_alpha));
    h = HashCombine(h, static_cast<std::size_t>(d.alpha_op));
    h = HashCombine(h, static_cast<std::size_t>(d.write_mask));
    return h;
  }

  std::size_t operator()(const DepthDesc& d) const {
    std::size_t h = 0;
    h = HashCombine(h, static_cast<std::size_t>(d.enable));
    h = HashCombine(h, static_cast<std::size_t>(d.write_enable));
    h = HashCombine(h, static_cast<std::size_t>(d.func));
    return h;
  }

  std::size_t operator()(const RasterDesc& d) const {
    std::size_t h = 0;
    h = HashCombine(h, static_cast<std::size_t>(d.fill));
    h = HashCombine(h, static_cast<std::size_t>(d.cull));
    h = HashCombine(h, static_cast<std::size_t>(d.front_ccw));
    h = HashCombine(h, static_cast<std::size_t>(static_cast<std::uint32_t>(d.depth_bias)));
    // Hash floats by bit-pattern. Using std::bit_cast avoids strict-aliasing UB.
    const auto fb1 = std::bit_cast<std::uint32_t>(d.slope_scaled_depth_bias);
    const auto fb2 = std::bit_cast<std::uint32_t>(d.depth_bias_clamp);
    h = HashCombine(h, static_cast<std::size_t>(fb1));
    h = HashCombine(h, static_cast<std::size_t>(fb2));
    h = HashCombine(h, static_cast<std::size_t>(d.scissor_enable));
    h = HashCombine(h, static_cast<std::size_t>(d.multisample_enable));
    return h;
  }

  std::size_t operator()(const SamplerDesc& d) const {
    std::size_t h = 0;
    h = HashCombine(h, static_cast<std::size_t>(d.address_u));
    h = HashCombine(h, static_cast<std::size_t>(d.address_v));
    h = HashCombine(h, static_cast<std::size_t>(d.address_w));
    h = HashCombine(h, static_cast<std::size_t>(d.filter));
    h = HashCombine(h, static_cast<std::size_t>(d.mip_linear));
    h = HashCombine(h, static_cast<std::size_t>(d.max_anisotropy));
    return h;
  }
};

}  // namespace gmp::renderer::d3d11

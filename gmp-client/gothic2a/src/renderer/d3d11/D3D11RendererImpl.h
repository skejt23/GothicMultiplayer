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

// ----------------------------------------------------------------------------
// D3D11RendererImpl - Low-level Direct3D 11 device wrapper
// ----------------------------------------------------------------------------
// This struct encapsulates D3D11 initialization, state management, and drawing
// primitives. It serves as the implementation detail for zCRnd_D3D_DX11 (pImpl).
//
// Key Differences from D3D9:
//
// 1. No Fixed Function Pipeline:
//    D3D11 requires shaders for all rendering. We use a minimal set of HLSL
//    shaders to emulate the FFP behavior Gothic expects.
//
// 2. Immutable State Objects:
//    Instead of setting individual render states, D3D11 uses pre-created
//    state objects (BlendState, DepthStencilState, RasterizerState, SamplerState).
//
// 3. Constant Buffers:
//    Transform matrices, material properties, and lighting are passed via
//    constant buffers instead of SetTransform/SetMaterial/SetLight calls.
//
// 4. No Device Lost:
//    D3D11 handles device recovery automatically. We only need to handle
//    swap chain resize on window size changes.
//
// 5. Deferred Context (optional):
//    D3D11 supports command list recording for multi-threaded rendering.
//    Currently we use immediate context only.
// ----------------------------------------------------------------------------

#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>

#include "D3D11Vertex.h"
#include "NativeRenderState.h"

namespace gmp::renderer::d3d11 {

// --- Renderer Capabilities ---
// Hardware capabilities queried from D3D11.

struct RendererCapabilities {
  unsigned int max_texture_size = 0;
  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
};

// --- Dynamic Vertex Batching ---
// Accumulates vertices to reduce draw calls by batching multiple polygons.

static constexpr size_t kBatchMaxVertices = 8192;
static constexpr size_t kBatchMaxIndices = kBatchMaxVertices * 3;

// Dynamic buffer sizes for ring-buffer allocation
// These need to be large enough to hold a single frame's worth of data.
// With triple buffering, we have 3 sets of these buffers.
static constexpr size_t kDynamicVBSize = 2 * 1024 * 1024;  // 2 MB vertex buffer per frame
static constexpr size_t kDynamicIBSize = 512 * 1024;       // 512 KB index buffer per frame

// Number of frames in flight (triple buffering)
static constexpr int kFramesInFlight = 3;

struct VertexBatch {
  VertexRHW vertices[kBatchMaxVertices];
  std::uint16_t indices[kBatchMaxIndices];
  size_t vertex_count = 0;
  size_t index_count = 0;

  void* current_texture = nullptr;
  bool is_active = false;

  void Reset() {
    vertex_count = 0;
    index_count = 0;
    current_texture = nullptr;
    is_active = false;
  }

  [[nodiscard]] bool HasRoom(size_t verts, size_t inds) const {
    return (vertex_count + verts <= kBatchMaxVertices) && (index_count + inds <= kBatchMaxIndices);
  }
};

// --- Constant Buffer Structures ---
// These are uploaded to the GPU for shader access.

struct alignas(16) TransformCB {
  DirectX::XMMATRIX world;
  DirectX::XMMATRIX view;
  DirectX::XMMATRIX projection;
  DirectX::XMMATRIX world_view_proj;
};

struct alignas(16) MaterialCB {
  DirectX::XMFLOAT4 diffuse;
  DirectX::XMFLOAT4 ambient;
  DirectX::XMFLOAT4 specular;
  DirectX::XMFLOAT4 emissive;
  float power;
  float alpha_ref;
  float alpha_test_enabled;
  float _pad[1];
};

struct alignas(16) FogCB {
  DirectX::XMFLOAT4 color;
  float start;
  float end;
  float density;
  float enabled;
  float range_enabled;
  float _pad[3];
};

struct alignas(16) ScreenCB {
  DirectX::XMFLOAT2 screen_size;
  DirectX::XMFLOAT2 inv_screen_size;
};

// FFP emulation constant buffer for pixel shaders.
// Field values map to enums in NativeRenderState.h for semantic clarity:
// - CombineOp: color_op0/1, alpha_op (kDisable=0, kSelectArg1=1, kSelectArg2=2, kModulate=3, kModulate2X=4, kModulate4X=5, kAdd=6)
// - CombineArg: alpha_arg1/2, sem_s*_colorarg* (kCurrent=0, kDiffuse=1, kTexture=3, kTFactor=4, kSpecular=5)
// - TexCoordSource+TexGenMode: uv_source0/1 is a packed integer stored as float:
//   packed = (uint8)TexCoordSource | ((uint8)TexGenMode << 8)
struct alignas(16) AlphaTestCB {
  // Alpha test configuration.
  float alpha_ref;           // Alpha reference value [0,1].
  float alpha_test_enabled;  // 1.0 if alpha test is on, else 0.0.
  float alpha_blend_func;    // For fog behavior: 3=ADD needs black fog.

  // Texture coordinate source for stages 0/1.
  // Packed as float: (uint8)TexCoordSource | ((uint8)TexGenMode << 8)
  float uv_source0;
  float uv_source1;

  // Stage 0 combine operations (CombineOp enum values as floats).
  float alpha_op;   // How to combine alpha: 0=MODULATE, 1=SELECTARG1, 2=SELECTARG2.
  float color_op0;  // How to combine color stage 0.
  float color_op1;  // How to combine color stage 1 (for dual-texture).

  // Optional semantic data (set only when the engine semantic bridge is active).
  // base_rgb_gen: -1 = not set, else Gothic_II_Addon::zTShaderRGBGen (0=IDENTITY,1=VERTEX,2=FACTOR,3=WAVE).
  float base_rgb_gen;
  float base_factor_r;
  float base_factor_g;
  float base_factor_b;

  // Full FFP emulation for 2-stage paths (lightmaps, water).
  // When sem_ffp_enabled > 0.5, shader uses these CombineOp/CombineArg values.
  float sem_ffp_enabled;
  float sem_s0_colorop;    // Stage 0 CombineOp.
  float sem_s0_colorarg1;  // Stage 0 CombineArg for arg1.
  float sem_s0_colorarg2;  // Stage 0 CombineArg for arg2.
  float sem_s1_colorop;    // Stage 1 CombineOp.
  float sem_s1_colorarg1;  // Stage 1 CombineArg for arg1.
  float sem_s1_colorarg2;  // Stage 1 CombineArg for arg2.

  // D3DRS_TEXTUREFACTOR (expanded to normalized RGBA).
  float tex_factor_r;
  float tex_factor_g;
  float tex_factor_b;
  float tex_factor_a;

  // Stage 0 alpha combine arguments (CombineArg values).
  float alpha_arg1;
  float alpha_arg2;
  float _pad_alpha_args[2];

  // Texture transform enable flags (1.0 = transform enabled for that stage).
  float tex_transform_enabled0;
  float tex_transform_enabled1;
  float _pad_tex_transform[2];

  // Texture transform matrices (only used when corresponding enabled flag is set).
  DirectX::XMMATRIX tex_transform0;
  DirectX::XMMATRIX tex_transform1;
};

// Modern gamma/color correction parameters.
// Uses standard color grading pipeline: Brightness → Contrast → Gamma.
// At neutral (0.5 for all sliders), all parameters produce identity transform.
// This is a modernized replacement for Gothic's D3D7/D3D9 hardware gamma ramp.
struct alignas(16) GammaCB {
  float brightness_offset;  // Additive offset: (brightness - 0.5) * 0.8
  float contrast_scale;     // Scale around 0.5: contrast * 2.0
  float gamma_exp;          // Power curve: 1.0 / (gamma * 2.0)
  float gamma_enabled;      // 1.0 when active, 0.0 for fast path (optional optimization).
  float _pad[4];
};

// Maximum number of lights supported (matches Gothic's typical limit)
static constexpr int kMaxLights = 8;

// Individual light structure for GPU
struct alignas(16) GpuLight {
  DirectX::XMFLOAT4 position;     // xyz = position, w = range
  DirectX::XMFLOAT4 direction;    // xyz = direction, w = type (0=off, 1=point, 2=spot, 3=dir)
  DirectX::XMFLOAT4 diffuse;      // rgb = color, a = enabled
  DirectX::XMFLOAT4 attenuation;  // x=const, y=linear, z=quadratic, w=spotCutoff
};

// Light constant buffer - holds all lights and ambient
struct alignas(16) LightCB {
  GpuLight lights[kMaxLights];
  DirectX::XMFLOAT4 ambient;  // Global ambient color (RGBA)
  int num_active_lights;      // Number of lights currently enabled
  float padding[3];
};

// --- D3D11RendererImpl ---
// Low-level D3D11 device wrapper with state management.

static constexpr size_t kMaxTextureStages = 8;

struct D3D11RendererImpl {
  ~D3D11RendererImpl() {
    Cleanup();
  }

  // --- Core D3D11 Objects ---
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* context = nullptr;
  IDXGISwapChain* swap_chain = nullptr;

  // DXGI output for fullscreen mode transitions.
  IDXGIOutput* dxgi_output = nullptr;

  // RenderDoc/PIX markers (optional). Queried from the device context.
  ID3DUserDefinedAnnotation* user_annotation = nullptr;

  // --- Render Targets ---
  ID3D11RenderTargetView* rtv = nullptr;
  ID3D11DepthStencilView* dsv = nullptr;
  ID3D11Texture2D* depth_stencil_texture = nullptr;

  // FLIP model backbuffer tracking:
  // With DXGI flip-model swap effects (FLIP_DISCARD/FLIP_SEQUENTIAL), the swap chain
  // rotates underlying backbuffers after each Present(). An RTV created from GetBuffer(0)
  // becomes stale after Present because it points to a buffer that may now be displayed.
  // We track when a refresh is needed and do it at BeginFrame() to keep rendering correct.
  // Note: With FLIP models, only GetBuffer(0) is valid - other indices return DXGI_ERROR_INVALID_CALL.
  bool flip_rtv_needs_refresh_ = false;  // Set after Present(), cleared after RTV refresh

  // --- Shaders ---
  ID3D11VertexShader* vs_basic = nullptr;                // 3D transformed geometry with normals
  ID3D11VertexShader* vs_basic_color = nullptr;          // 3D transformed geometry with normals + vertex color (stride=36)
  ID3D11VertexShader* vs_basic_unlit = nullptr;          // UT_UL with lighting disabled (stride=32)
  ID3D11VertexShader* vs_basic_color_unlit = nullptr;    // UT_UL w/ vertex color and lighting disabled (stride=36)
  ID3D11VertexShader* vs_lit = nullptr;                  // 3D pre-lit geometry (no normals)
  ID3D11VertexShader* vs_lit_2uv = nullptr;              // 3D pre-lit geometry (pos+color+2xuv, stride=32)
  ID3D11VertexShader* vs_lit_normal_singleuv = nullptr;  // 3D pre-lit geometry with normals + single UV (stride=36)
  ID3D11VertexShader* vs_lit_normal = nullptr;           // 3D pre-lit geometry with normals (water)
  ID3D11VertexShader* vs_rhw = nullptr;                  // Pre-transformed (screen-space) geometry
  ID3D11PixelShader* ps_basic = nullptr;                 // Textured with fog
  ID3D11PixelShader* ps_rhw = nullptr;                   // Textured for RHW (no fog)
  ID3D11PixelShader* ps_rhw_alpha = nullptr;             // RHW alpha polys w/ stage0 ops
  ID3D11PixelShader* ps_lightmap = nullptr;              // Dual-texture lightmapping
  ID3D11PixelShader* ps_dual_add = nullptr;              // Dual-texture additive (env/water)
  ID3D11PixelShader* ps_vertex_color = nullptr;          // Vertex color only (no texture)

  // --- Input Layouts ---
  ID3D11InputLayout* layout_3d = nullptr;                   // For Vertex3D (pos+normal+uv, stride=32)
  ID3D11InputLayout* layout_3d_color = nullptr;             // For Vertex3D (pos+normal+color+uv, stride=36)
  ID3D11InputLayout* layout_lit = nullptr;                  // For pre-lit vertices (pos+color+uv, stride=24)
  ID3D11InputLayout* layout_lit_2uv = nullptr;              // For pre-lit vertices (pos+color+2xuv, stride=32)
  ID3D11InputLayout* layout_lit_normal_singleuv = nullptr;  // For lit vertices with normal (pos+normal+color+uv, stride=36)
  ID3D11InputLayout* layout_lit_normal = nullptr;           // For lit vertices with normal (pos+normal+color+2xuv, stride=44)
  ID3D11InputLayout* layout_rhw = nullptr;                  // For VertexRHW
  ID3D11InputLayout* layout_rhw2 = nullptr;                 // For VertexRHW2

  // --- Constant Buffers ---
  ID3D11Buffer* cb_transform = nullptr;
  ID3D11Buffer* cb_material = nullptr;
  ID3D11Buffer* cb_fog = nullptr;
  ID3D11Buffer* cb_screen = nullptr;
  ID3D11Buffer* cb_alpha_test = nullptr;
  ID3D11Buffer* cb_light = nullptr;
  ID3D11Buffer* cb_gamma = nullptr;

  // --- State Objects ---
  // Blend states
  ID3D11BlendState* bs_opaque = nullptr;
  ID3D11BlendState* bs_alpha_blend = nullptr;
  ID3D11BlendState* bs_alpha_add = nullptr;
  ID3D11BlendState* bs_alpha_mul = nullptr;
  ID3D11BlendState* bs_alpha_mul2 = nullptr;  // MUL2: DestColor * Src + SrcColor * Dest

  // Depth stencil states
  ID3D11DepthStencilState* dss_default = nullptr;     // Z-test and Z-write enabled (LESS_EQUAL)
  ID3D11DepthStencilState* dss_alpha = nullptr;       // Z-test enabled (LESS_EQUAL), Z-write disabled
  ID3D11DepthStencilState* dss_alpha_less = nullptr;  // Z-test enabled (LESS), Z-write disabled
  ID3D11DepthStencilState* dss_no_depth = nullptr;    // Z-test and Z-write disabled

  // Rasterizer states
  ID3D11RasterizerState* rs_default = nullptr;    // Back-face culling
  ID3D11RasterizerState* rs_no_cull = nullptr;    // No culling
  ID3D11RasterizerState* rs_wireframe = nullptr;  // Wireframe mode
  ID3D11RasterizerState* rs_biased = nullptr;     // With depth bias

  // Sampler states
  ID3D11SamplerState* ss_linear_wrap = nullptr;
  ID3D11SamplerState* ss_linear_clamp = nullptr;
  ID3D11SamplerState* ss_linear_wrap_clamp = nullptr;  // wrap U, clamp V
  ID3D11SamplerState* ss_linear_clamp_wrap = nullptr;  // clamp U, wrap V
  ID3D11SamplerState* ss_linear_mip_point_wrap = nullptr;
  ID3D11SamplerState* ss_linear_mip_point_clamp = nullptr;
  ID3D11SamplerState* ss_linear_mip_point_wrap_clamp = nullptr;  // wrap U, clamp V
  ID3D11SamplerState* ss_linear_mip_point_clamp_wrap = nullptr;  // clamp U, wrap V
  ID3D11SamplerState* ss_point_wrap = nullptr;
  ID3D11SamplerState* ss_point_clamp = nullptr;
  ID3D11SamplerState* ss_point_wrap_clamp = nullptr;  // wrap U, clamp V
  ID3D11SamplerState* ss_point_clamp_wrap = nullptr;  // clamp U, wrap V

  ID3D11SamplerState* ss_aniso_wrap = nullptr;
  ID3D11SamplerState* ss_aniso_clamp = nullptr;
  ID3D11SamplerState* ss_aniso_wrap_clamp = nullptr;  // wrap U, clamp V
  ID3D11SamplerState* ss_aniso_clamp_wrap = nullptr;  // clamp U, wrap V

  // Default fallback textures (1x1) for when no texture is bound
  // White for multiplicative blends (identity), Black for additive blends (no-op)
  ID3D11Texture2D* white_texture = nullptr;
  ID3D11ShaderResourceView* white_srv = nullptr;
  ID3D11Texture2D* black_texture = nullptr;
  ID3D11ShaderResourceView* black_srv = nullptr;

  // --- Dynamic Buffers (Triple Buffered) ---
  // Each frame uses its own set of buffers to avoid CPU-GPU synchronization.
  // The GPU can be up to 2 frames behind the CPU without stalling.
  struct FrameResources {
    ID3D11Buffer* dynamic_vb = nullptr;
    ID3D11Buffer* dynamic_ib = nullptr;
    ID3D11Query* fence = nullptr;  // Signals when GPU finishes this frame
    size_t vb_offset = 0;          // Current write position in VB
    size_t ib_offset = 0;          // Current write position in IB
    bool fence_pending = false;    // True if fence has been issued
  };
  std::array<FrameResources, kFramesInFlight> frame_resources_;
  int current_frame_index_ = 0;
  size_t dynamic_vb_capacity = 0;
  size_t dynamic_ib_capacity = 0;
  bool frame_begun_ = false;  // Guards against multiple BeginFrame calls per frame

  bool RefreshBackBufferRTV();

  // Convenience pointers to current frame's resources
  ID3D11Buffer* dynamic_vb = nullptr;
  ID3D11Buffer* dynamic_ib = nullptr;
  size_t dynamic_vb_offset = 0;
  size_t dynamic_ib_offset = 0;

  // --- Batching ---
  VertexBatch batch_;

  // --- Current State Tracking ---
  std::array<ID3D11ShaderResourceView*, kMaxTextureStages> bound_srvs = {};
  std::array<DXGI_FORMAT, kMaxTextureStages> bound_srv_formats_ = {};
  std::array<ID3D11SamplerState*, kMaxTextureStages> bound_samplers = {};

  struct BoundVertexBuffer {
    ID3D11Buffer* buffer = nullptr;
    UINT stride = 0;
    UINT offset = 0;
  };
  std::array<BoundVertexBuffer, 2> bound_vbs = {};
  ID3D11Buffer* bound_ib = nullptr;
  DXGI_FORMAT bound_ib_format = DXGI_FORMAT_UNKNOWN;
  UINT bound_ib_offset = 0;
  D3D11_PRIMITIVE_TOPOLOGY current_topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

  int current_viewport_x = (std::numeric_limits<int>::min)();
  int current_viewport_y = (std::numeric_limits<int>::min)();
  int current_viewport_w = (std::numeric_limits<int>::min)();
  int current_viewport_h = (std::numeric_limits<int>::min)();

  // Cache constant buffer bindings to avoid redundant API calls in hot paths.
  // Slots used today: VS {0..5}, PS {1..6}.
  static constexpr std::size_t kMaxConstantBufferSlots = 8;
  std::array<ID3D11Buffer*, kMaxConstantBufferSlots> bound_vs_cbs = {};
  std::array<ID3D11Buffer*, kMaxConstantBufferSlots> bound_ps_cbs = {};
  ID3D11BlendState* current_blend_state = nullptr;
  ID3D11DepthStencilState* current_depth_state = nullptr;
  UINT current_stencil_ref = 0;
  ID3D11RasterizerState* current_rasterizer_state = nullptr;
  // The last rasterizer state requested by higher-level code (before applying Z-bias overrides).
  ID3D11RasterizerState* requested_rasterizer_state_ = nullptr;
  // The last typed raster descriptor set by higher-level code via SetRasterDesc().
  // This is our best "engine intent" signal for culling (e.g. CullMode::kNone for two-sided foliage).
  RasterDesc engine_raster_desc_ = {};
  bool has_engine_raster_desc_ = false;
  // Z-bias parameters (in normalized depth units) requested by the renderer.
  float raster_depth_bias_ = 0.0f;
  float raster_slope_scaled_depth_bias_ = 0.0f;
  float raster_depth_bias_clamp_ = 0.0f;
  ID3D11VertexShader* current_vs = nullptr;
  ID3D11PixelShader* current_ps = nullptr;
  ID3D11InputLayout* current_layout = nullptr;

  // Tracked blend descriptor for alpha-poly shader hints.
  // (Used to detect additive blending for alpha-poly shader paths.)
  BlendDesc current_blend_desc_ = {};

  // --- Texture Stage State Tracking (using renderer-owned enums) ---
  // Per-stage state using explicit types from NativeRenderState.h.
  std::array<TexCoordSource, 8> stage_uv_source_ = {};  // Which UV set (UV0, UV1, Generated)
  std::array<TexGenMode, 8> stage_texgen_mode_ = {};    // Texture coordinate generation mode
  std::array<CombineOp, 8> stage_color_op_ = {CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate,
                                              CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate};
  std::array<CombineOp, 8> stage_alpha_op_ = {CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate,
                                              CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate, CombineOp::kModulate};
  std::array<CombineArg, 8> stage_color_arg1_ = {CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture,
                                                 CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture};
  std::array<CombineArg, 8> stage_color_arg2_ = {CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse,
                                                 CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse};
  std::array<CombineArg, 8> stage_alpha_arg1_ = {CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture,
                                                 CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture, CombineArg::kTexture};
  std::array<CombineArg, 8> stage_alpha_arg2_ = {CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse,
                                                 CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse, CombineArg::kDiffuse};
  std::array<bool, 8> stage_tex_transform_enabled_ = {};  // Whether texture transform is active

  // Native sampler state tracking (per stage).
  std::array<SamplerDesc, 8> stage_sampler_desc_ = {};

  // Raw matrices as provided by the engine (needed for texgen paths).
  DirectX::XMMATRIX tex_transform_matrix_raw_[8] = {};
  DirectX::XMMATRIX tex_transform_matrix_[8] = {};

  bool has_lightmap_ = false;  // Whether lightmap is active (from tex stage state)

  // Optional semantic shader hints (set by zCRnd_D3D_DX11 when ShaderSemanticBridge is active).
  // When not set, uses default heuristic behavior.
  int semantic_base_rgb_gen_ = -1;
  DirectX::XMFLOAT3 semantic_base_factor_rgb_ = {1.0f, 1.0f, 1.0f};

  // Optional semantic 2-stage fixed-function emulation parameters for lightmap draws.
  bool semantic_ffp_enabled_ = false;
  unsigned long semantic_s0_colorop_ = 0;
  unsigned long semantic_s0_colorarg1_ = 0;
  unsigned long semantic_s0_colorarg2_ = 0;
  unsigned long semantic_s1_colorop_ = 0;
  unsigned long semantic_s1_colorarg1_ = 0;
  unsigned long semantic_s1_colorarg2_ = 0;

  // D3DRS_TEXTUREFACTOR (ARGB) expanded to normalized RGBA.
  DirectX::XMFLOAT4 texture_factor_rgba_ = {1.0f, 1.0f, 1.0f, 1.0f};

  // --- Cached Constant Buffer Data ---
  TransformCB transform_data = {};
  MaterialCB material_data = {};
  FogCB fog_data = {};
  ScreenCB screen_data = {};
  AlphaTestCB alpha_test_data = {};
  LightCB light_data = {};
  GammaCB gamma_data = {};

  // Legacy render state bits that materially affect shader selection.
  bool lighting_enabled_ = true;

  // Last uploaded CB snapshots (to skip redundant Map/Unmap when data repeats).
  TransformCB last_transform_data_ = {};
  MaterialCB last_material_data_ = {};
  FogCB last_fog_data_ = {};
  AlphaTestCB last_alpha_test_data_ = {};
  LightCB last_light_data_ = {};
  bool has_last_transform_data_ = false;
  bool has_last_material_data_ = false;
  bool has_last_fog_data_ = false;
  bool has_last_alpha_test_data_ = false;
  bool has_last_light_data_ = false;

  bool transform_dirty = true;
  bool material_dirty = true;
  bool fog_dirty = true;
  bool light_dirty = true;
  std::array<bool, kMaxLights> light_enabled = {};

  // --- Hardware Capabilities ---
  RendererCapabilities capabilities_;
  UINT max_anisotropy = 16;

  // --- Window Info ---
  HWND hwnd = nullptr;
  int screen_width = 0;
  int screen_height = 0;
  bool fullscreen = false;
  bool vsync = true;

  // --- FLIP Model / Tearing Support ---
  // True if the system supports DXGI_PRESENT_ALLOW_TEARING (Windows 10+)
  bool tearing_supported = false;
  // True if we're using FLIP presentation model (modern, allows tearing)
  bool using_flip_model = false;

  [[nodiscard]] bool IsUsingFlipModel() const {
    return using_flip_model;
  }

  // --- Lifecycle ---
  bool Init(void* hwnd, int width, int height, bool fullscreen);
  bool Resize(int width, int height);
  bool SetFullscreenState(bool fullscreen);
  void Cleanup();

  // --- Frame Management ---
  void BeginFrame();
  void EndFrame();
  void Present();
  void Clear(unsigned long color);
  void Clear(unsigned long color, bool clear_color, bool clear_depth);

  // --- Drawing Primitives ---
  void DrawTriangles(const Vertex3D* vertices, int count);
  void DrawTrianglesRHW(const VertexRHW* vertices, int count);
  void DrawTriangleFan(const VertexRHW* vertices, int count);
  void DrawTriangleFan2(const VertexRHW2* vertices, int count);
  void DrawLine(float x1, float y1, float x2, float y2, unsigned long color);

  // Vertex types for DrawVertexBuffer:
  // 0 = UT_UL (Untransformed, Unlit) - needs 3D pipeline with normals (stride=36)
  // 1 = UT_L (Untransformed, Lit) - 3D pipeline, pre-lit (stride=24)
  // 2 = T_L (Transformed, Lit) - screen-space RHW pipeline (stride=28 or similar)
  // alpha_blend_func: 0=NONE, 1=BLEND, 2=ADD, 3=SUB, 4=MUL, 5=MUL2, 6=TEST, 7=MAT_DEFAULT
  bool DrawVertexBuffer(ID3D11Buffer* vertex_buffer, UINT stride, D3D11_PRIMITIVE_TOPOLOGY topology, UINT start_vertex, UINT vertex_count,
                        const unsigned short* indices, UINT index_count, int vertex_type = 1, int alpha_blend_func = 0);

  // --- Batched Drawing ---
  void BatchTriangleFan(const VertexRHW* vertices, int count, void* texture);
  void FlushBatch();

  // --- Alpha Polygon Batched Drawing ---
  // z_func: 0=ALWAYS, 1=NEVER, 2=LESS, 3=LESS_EQUAL
  bool DrawAlphaBatch(const VertexRHW* vertices, size_t vertex_count, const uint16_t* indices, size_t index_count, bool has_texture, int z_func);
  void DrawTriangleFan(const VertexRHW* vertices, int count, int z_func);
  void DrawBatchRHW(const VertexRHW* vertices, int vertex_count, const uint16_t* indices, int index_count, int z_func);

  // --- Transforms & Viewport ---
  void SetViewport(int x, int y, int width, int height);
  void SetWorldMatrix(const float* matrix);
  void SetViewMatrix(const float* matrix);
  void SetProjectionMatrix(const float* matrix);
  void SetTextureTransformMatrix(unsigned long stage, const float* matrix);
  void UpdateTransformCB();

  // --- Texture Management ---
  void SetTexture(int stage, ID3D11ShaderResourceView* srv, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
  void SetTextureWrap(int stage, bool enable);
  void SetTextureFilter(int stage, int filter);
  void SetHasLightmap(bool has_lightmap);

  // Semantic hints (populated by ShaderSemanticBridge in the higher-level renderer).
  void SetSemanticBaseRgbGen(int rgb_gen);
  void SetSemanticBaseColorFactor(unsigned long argb);
  void SetSemanticLightmapFfp(bool enabled, unsigned long s0_colorop, unsigned long s0_colorarg1, unsigned long s0_colorarg2,
                              unsigned long s1_colorop, unsigned long s1_colorarg1, unsigned long s1_colorarg2);

  // --- State Management ---
  void SetBlendState(ID3D11BlendState* state);
  void SetDepthStencilState(ID3D11DepthStencilState* state, UINT stencil_ref = 0);
  void SetRasterizerState(ID3D11RasterizerState* state);
  void SetSamplerState(int stage, ID3D11SamplerState* state);
  void SetShaders(ID3D11VertexShader* vs, ID3D11PixelShader* ps);
  void SetInputLayout(ID3D11InputLayout* layout);
  void BindVertexBuffer(UINT slot, ID3D11Buffer* buffer, UINT stride, UINT offset);
  void BindIndexBuffer(ID3D11Buffer* buffer, DXGI_FORMAT format, UINT offset);
  void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology);
  void BindVSConstantBuffer(UINT slot, ID3D11Buffer* buffer);
  void BindPSConstantBuffer(UINT slot, ID3D11Buffer* buffer);

  // --- Material & Fog ---
  void SetMaterial(const MaterialCB& material);
  void SetFog(bool enable, unsigned long color, float start, float end, bool range_enabled);
  void SetGammaCorrection(float gamma, float contrast, float brightness);
  void UpdateMaterialCB();
  void UpdateFogCB();
  void UpdateAlphaTestCB();

  // --- Convenience State Setters ---
  void ApplyOpaqueState();
  void ApplyAlphaBlendState();
  void ApplyAlphaAddState();
  void ApplyAlphaTestState();

  // --- Native State Descriptors (D3D11-first) ---
  // These accept renderer-owned typed descriptors (see NativeRenderState.h) and
  // select the closest prebuilt D3D11 state objects.
  //
  // NOTE: Today this does not create arbitrary D3D11 state objects; it maps to
  // the existing fixed set (bs_*, dss_*, rs_*, ss_*). State-object caching and
  // full descriptor->D3D11_DESC conversion is handled in a later migration step.
  void SetBlendDesc(const BlendDesc& desc);
  void SetDepthDesc(const DepthDesc& desc);
  void SetRasterDesc(const RasterDesc& desc);
  void SetSamplerDesc(int stage, const SamplerDesc& desc);

  // --- Native Texture Stage State API ---
  // These replace D3D9-style SetTextureStageState calls with typed, D3D11-native methods.
  // They update internal tracking arrays used by shaders for FFP emulation.
  void SetTextureStageColorOp(int stage, CombineOp op);
  void SetTextureStageAlphaOp(int stage, CombineOp op);
  void SetTextureStageColorArg(int stage, int arg_index, CombineArg arg);
  void SetTextureStageAlphaArg(int stage, int arg_index, CombineArg arg);
  void SetTextureStageUVSource(int stage, TexCoordSource source, TexGenMode gen = TexGenMode::kNone);
  void SetTextureStageTransform(int stage, bool enable);

  // --- Native Render State API ---
  // These replace D3D9-style SetRenderState calls with typed, D3D11-native methods.
  void SetAlphaTest(bool enable, float ref = 0.5f);
  void SetAlphaBlend(bool enable, BlendFactor src = BlendFactor::kSrcAlpha, BlendFactor dst = BlendFactor::kInvSrcAlpha);
  void SetCullMode(CullMode mode);
  void SetDepthTest(bool enable, bool write_enable = true, CompareFunc func = CompareFunc::kLessEqual);
  void SetTextureFactor(unsigned long argb);

  // --- Native Sampler State API ---
  // These replace D3D9-style SetSamplerState calls with typed methods.
  void SetSamplerAddressing(int stage, AddressMode u, AddressMode v);
  void SetSamplerFilter(int stage, FilterMode filter);
  void SetSamplerMipFilter(int stage, bool mip_linear);

  // --- Accessors ---
  [[nodiscard]] ID3D11Device* GetDevice() const {
    return device;
  }
  [[nodiscard]] ID3D11DeviceContext* GetContext() const {
    return context;
  }
  [[nodiscard]] const RendererCapabilities& GetCapabilities() const {
    return capabilities_;
  }
  [[nodiscard]] bool IsFullscreen() const {
    return fullscreen;
  }

  // D3D11-native helpers
  void SetFillMode(unsigned long mode);
  void SetZBias(float depth_bias, float slope_scaled_depth_bias);
  void SetColorWrite(bool r, bool g, bool b, bool a);

  void SetLightingEnabled(bool enable);
  size_t GetAvailableTextureMem() const;
  void SetLight(unsigned long index, const Light& light);
  void LightEnable(unsigned long index, bool enable);
  void SetAmbientLight(unsigned long color);
  void UpdateLightBuffer();

private:
  [[nodiscard]] float ComputeEffectiveAlphaRef(float raw_ref);

  // --- Initialization Helpers ---
  bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height, bool fullscreen);
  bool CreateRenderTargetView();
  bool CreateDepthStencilView(int width, int height);
  bool CreateShaders();
  bool CreateConstantBuffers();
  bool CreateStateObjects();
  bool CreateDynamicBuffers();

  // --- Dynamic Buffer Helpers ---
  void* AllocateDynamicVB(size_t bytes, UINT& out_offset);
  uint16_t* AllocateDynamicIB(size_t count, UINT& out_offset);

  // Rotate to next frame's buffers. Waits for that frame's GPU work to complete.
  void RotateFrameResources();

  // Update convenience pointers to current frame's buffers
  void UpdateCurrentFramePointers();

  // --- Sampler State Helpers ---
  // Selects and applies appropriate prebuilt sampler based on stage_sampler_desc_.
  void ApplyPrebuiltSamplerState(int stage);

  [[nodiscard]] bool IsAdditiveBlendForAlphaPoly() const;

  // --- D3D11 State Object Caches (typed descriptors) ---
  // These caches own the created D3D11 state objects and are released in Cleanup().
  std::unordered_map<BlendDesc, ID3D11BlendState*, NativeRenderStateHash> blend_state_cache_;
  std::unordered_map<DepthDesc, ID3D11DepthStencilState*, NativeRenderStateHash> depth_state_cache_;
  std::unordered_map<RasterDesc, ID3D11RasterizerState*, NativeRenderStateHash> raster_state_cache_;
  std::unordered_map<SamplerDesc, ID3D11SamplerState*, NativeRenderStateHash> sampler_state_cache_;

  ID3D11BlendState* GetOrCreateBlendState(const BlendDesc& desc);
  ID3D11DepthStencilState* GetOrCreateDepthStencilState(const DepthDesc& desc);
  ID3D11RasterizerState* GetOrCreateRasterizerState(const RasterDesc& desc);
  ID3D11SamplerState* GetOrCreateSamplerState(const SamplerDesc& desc);
};

// --- Global D3D11 State ---
// Exposed for D3D11VertexBuffer and D3D11Texture resource management.
extern ID3D11Device* g_D3D11Device;
extern ID3D11DeviceContext* g_D3D11Context;

}  // namespace gmp::renderer::d3d11

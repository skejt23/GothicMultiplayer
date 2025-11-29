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
// D3D9RendererImpl - Low-level Direct3D 9 device wrapper
// ----------------------------------------------------------------------------
// This struct encapsulates D3D9 initialization, state management, and drawing
// primitives. It serves as the implementation detail for zCRnd_D3D_DX9 (pImpl).
//
// Key Optimizations:
//
// 1. State Caching:
//    All render states, sampler states, and texture stage states are cached to
//    avoid redundant D3D9 API calls. The cache is invalidated on device reset.
//    Typical cache hit rate: ~80-85%.
//
// 2. State Blocks:
//    Common render state configurations (opaque, alpha blend, alpha add, alpha
//    test) are stored in D3D9 state blocks for efficient mode switching.
//
// 3. Dynamic Buffer Ring Buffer Pattern:
//    Dynamic vertex/index buffers use the ring buffer pattern for efficient
//    CPU→GPU data transfer without stalls:
//    - D3DLOCK_NOOVERWRITE: Append to unused buffer region (no stall)
//    - D3DLOCK_DISCARD: Orphan buffer when full (minimal stall)
//    This enables continuous streaming of batched geometry without GPU stalls.
//
// 4. Batched Rendering:
//    Multiple polygons with identical render state are accumulated and drawn
//    in single draw calls, reducing driver overhead by ~90%.
// ----------------------------------------------------------------------------

#include <d3d9.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "DynamicVertexBuffer.h"

// --- Vertex Formats ---
// These vertex structures match Gothic's internal formats and use Fixed Function Pipeline (FFP).

// 3D vertex with position, normal, diffuse color, and single UV coordinate.
// Used for world geometry that needs lighting calculations.
struct Vertex3D {
  float x, y, z;        // Position in world/view space
  float nx, ny, nz;     // Normal for lighting
  unsigned long color;  // Pre-lit vertex color (ARGB)
  float u, v;           // Texture coordinates
};

// Pre-transformed (RHW) vertex with position, color, and single UV.
// Used for 2D UI elements and post-projection geometry.
// RHW = Reciprocal of Homogeneous W (already screen-space transformed).
struct VertexRHW {
  float x, y, z, rhw;   // Screen-space position (rhw = 1/w)
  unsigned long color;  // Vertex color (ARGB)
  float u, v;           // Texture coordinates
};

// Pre-transformed vertex with two UV sets for multi-texturing.
// Used for lightmapped surfaces (diffuse + lightmap).
struct VertexRHW2 {
  float x, y, z, rhw;   // Screen-space position
  unsigned long color;  // Vertex color (ARGB)
  float u1, v1;         // Primary texture coordinates (diffuse)
  float u2, v2;         // Secondary texture coordinates (lightmap)
};

// --- FVF (Flexible Vertex Format) Constants ---
// These define the vertex format for D3D9's Fixed Function Pipeline.

// Vertex3D: Position + Normal + Diffuse + 1 texture coordinate set
constexpr unsigned long kFvfVertex3D = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

// VertexRHW: Pre-transformed position + Diffuse + 1 texture coordinate set
constexpr unsigned long kFvfVertexRhw = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

// VertexRHW2: Pre-transformed position + Diffuse + 2 texture coordinate sets
constexpr unsigned long kFvfVertexRhw2 = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2;

// --- Renderer Capabilities ---
// Hardware capabilities queried from D3D9.

struct RendererCapabilities {
  unsigned int max_texture_size = 0;
};

// --- Dynamic Vertex Batching ---
// Accumulates vertices to reduce draw calls by batching multiple polygons.
// Instead of calling DrawPrimitiveUP per polygon, we collect vertices and flush once.

// Maximum vertices per batch. Modern GPUs benefit from larger batches.
// Each VertexRHW is 28 bytes, so 8192 vertices = ~230KB (well within modern VRAM budgets).
static constexpr size_t kBatchMaxVertices = 8192;
static constexpr size_t kBatchMaxIndices = kBatchMaxVertices * 3;

// Alpha poly batch buffer sizes (separate from general batching)
static constexpr size_t kAlphaBatchMaxVertices = 4096;
static constexpr size_t kAlphaBatchMaxIndices = kAlphaBatchMaxVertices * 3;

struct VertexBatch {
  // Vertex data accumulated this batch
  VertexRHW vertices[kBatchMaxVertices];
  std::uint16_t indices[kBatchMaxIndices];
  size_t vertex_count = 0;
  size_t index_count = 0;

  // Current batch state (for state-change detection)
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

// --- D3D9RendererImpl ---
// Low-level D3D9 device wrapper with state management.

// State cache constants
static constexpr size_t kMaxRenderStates = 210;       // D3DRS_BLENDOPALPHA + 1
static constexpr size_t kMaxSamplerStates = 14;       // D3DSAMP_DMAPOFFSET + 1
static constexpr size_t kMaxTextureStageStates = 33;  // D3DTSS_CONSTANT + 1
static constexpr size_t kMaxTextureStages = 8;
static constexpr DWORD kStateCacheInvalid = 0xDEADBEEF;  // Sentinel for invalid cache entry

// State change statistics for performance monitoring
struct StateChangeStats {
  size_t render_state_changes = 0;  // D3D API calls made
  size_t render_state_cached = 0;   // Calls avoided due to cache hit
  size_t sampler_state_changes = 0;
  size_t sampler_state_cached = 0;
  size_t texture_stage_changes = 0;
  size_t texture_stage_cached = 0;
  size_t texture_changes = 0;
  size_t texture_cached = 0;

  void Reset() {
    render_state_changes = 0;
    render_state_cached = 0;
    sampler_state_changes = 0;
    sampler_state_cached = 0;
    texture_stage_changes = 0;
    texture_stage_cached = 0;
    texture_changes = 0;
    texture_cached = 0;
  }

  [[nodiscard]] float GetRenderStateCacheHitRate() const {
    const size_t total = render_state_changes + render_state_cached;
    return total > 0 ? static_cast<float>(render_state_cached) / static_cast<float>(total) : 0.0f;
  }

  [[nodiscard]] size_t GetTotalApiCalls() const {
    return render_state_changes + sampler_state_changes + texture_stage_changes + texture_changes;
  }

  [[nodiscard]] size_t GetTotalCacheHits() const {
    return render_state_cached + sampler_state_cached + texture_stage_cached + texture_cached;
  }
};

struct D3D9RendererImpl {
  // D3D9 objects
  IDirect3D9* d3d = nullptr;
  IDirect3DDevice9* device = nullptr;
  D3DPRESENT_PARAMETERS present_params = {};
  DWORD max_anisotropy = 1;
  DWORD max_simultaneous_textures = 1;

  // Dynamic index buffer for DrawVertexBuffer (general purpose)
  IDirect3DIndexBuffer9* dynamic_index_buffer = nullptr;
  UINT dynamic_index_capacity = 0;

  // --- Ring Buffer Pattern ---
  // Dynamic vertex/index buffers using ring buffer pattern for efficient streaming.
  // See DynamicVertexBuffer.h for detailed documentation on the pattern.
  gmp::renderer::DynamicVertexBuffer batch_ring_vb_;  // General batching
  gmp::renderer::DynamicIndexBuffer batch_ring_ib_;   // General batching
  gmp::renderer::DynamicVertexBuffer alpha_ring_vb_;  // Alpha polygon batching
  gmp::renderer::DynamicIndexBuffer alpha_ring_ib_;   // Alpha polygon batching

  // Current batch accumulation (in CPU memory before upload)
  VertexBatch batch_;

  // Statistics for debugging/profiling
  struct BatchStats {
    size_t draw_calls_saved = 0;
    size_t batches_flushed = 0;
    size_t polygons_batched = 0;
    void Reset() {
      draw_calls_saved = 0;
      batches_flushed = 0;
      polygons_batched = 0;
    }
  } batch_stats_;

  // Texture binding tracking (for unbind optimization)
  std::array<void*, 8> bound_textures = {};

  // --- State Caching ---
  // All D3D9 state is cached to avoid redundant API calls.
  // Cache is invalidated (filled with sentinel) on device reset.
  std::array<DWORD, kMaxRenderStates> render_state_cache_{};
  std::array<std::array<DWORD, kMaxSamplerStates>, kMaxTextureStages> sampler_state_cache_{};
  std::array<std::array<DWORD, kMaxTextureStageStates>, kMaxTextureStages> texture_stage_cache_{};
  std::array<IDirect3DBaseTexture9*, kMaxTextureStages> texture_cache_{};

  // --- State Blocks ---
  // Pre-recorded state configurations for common render modes.
  // These reduce state switching overhead by applying multiple states atomically.
  IDirect3DStateBlock9* state_block_opaque_ = nullptr;       // Opaque geometry (z-write, no blend)
  IDirect3DStateBlock9* state_block_alpha_blend_ = nullptr;  // Alpha blended (z-test, no z-write, src*alpha + dst*(1-alpha))
  IDirect3DStateBlock9* state_block_alpha_add_ = nullptr;    // Additive blend (z-test, no z-write, src + dst)
  IDirect3DStateBlock9* state_block_alpha_test_ = nullptr;   // Alpha tested (z-write, alpha test enabled)

  // State change statistics
  StateChangeStats state_stats_;

  // Scene state
  bool in_scene = false;

  // Hardware capabilities
  RendererCapabilities capabilities_;

  // --- Lifecycle ---
  bool Init(void* hwnd, int width, int height, bool fullscreen);  // Create device with specified mode
  bool Reset(int width, int height);  // Reset device for resolution change (preserves D3DPOOL_MANAGED resources)
  void Cleanup();

  // --- Frame Management ---
  void BeginFrame();
  void EndFrame();
  void Vid_Blit();
  void Present();
  void Clear(unsigned long color);

  // --- Drawing Primitives ---
  void DrawTriangles(const Vertex3D* vertices, int count);
  void DrawTrianglesRHW(const VertexRHW* vertices, int count);
  void DrawTriangleFan(const VertexRHW* vertices, int count);
  void DrawTriangleFan2(const VertexRHW2* vertices, int count);
  void DrawLine(float x1, float y1, float x2, float y2, unsigned long color);
  bool DrawVertexBuffer(void* vertexBuffer, unsigned long fvf, unsigned int stride, D3DPRIMITIVETYPE primitiveType, unsigned int startVertex,
                        unsigned int vertexCount, const unsigned short* indices, unsigned int indexCount);

  // --- Batched Drawing ---
  // These methods accumulate geometry and flush when state changes or buffer is full.
  void BatchTriangleFan(const VertexRHW* vertices, int count, void* texture);
  void FlushBatch();
  [[nodiscard]] const BatchStats& GetBatchStats() const {
    return batch_stats_;
  }

  // --- Alpha Polygon Batched Drawing ---
  // Specialized batched drawing for alpha polygons.
  // Uses separate vertex/index buffers to avoid flushing general batch.
  bool DrawAlphaBatch(const VertexRHW* vertices, size_t vertex_count, const uint16_t* indices, size_t index_count);

  // --- Transforms & Viewport ---
  void SetViewport(int x, int y, int width, int height);
  void SetTransform(int state, const float* matrix);

  // --- Texture Management ---
  void SetTexture(int stage, void* texture);
  void SetTextureWrap(int stage, bool enable);
  void SetTextureFilter(int stage, int filter);

  // --- Render States (Cached) ---
  // These methods check the cache before calling D3D9.
  // Returns true if state was changed, false if cached.
  bool SetRenderStateCached(D3DRENDERSTATETYPE state, DWORD value);
  bool SetSamplerStateCached(DWORD stage, D3DSAMPLERSTATETYPE state, DWORD value);
  bool SetTextureStageStateCached(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value);
  bool SetTextureCached(DWORD stage, IDirect3DBaseTexture9* texture);

  // Direct state setters (bypass cache - use sparingly)
  void SetDither(bool enable);
  void SetGammaCorrection(float gamma, float contrast, float brightness);
  void SetAlphaRef(unsigned long ref);
  void SetColorWrite(DWORD mask);
  void SetRenderState(unsigned long state, unsigned long value);                   // Legacy, bypasses cache
  void SetTextureStageState(int stage, unsigned long state, unsigned long value);  // Legacy
  void SetSamplerState(int stage, unsigned long state, unsigned long value);       // Legacy

  // --- State Block Operations ---
  void CreateStateBlocks();
  void ReleaseStateBlocks();
  void ApplyOpaqueStateBlock();
  void ApplyAlphaBlendStateBlock();
  void ApplyAlphaAddStateBlock();
  void ApplyAlphaTestStateBlock();

  // --- State Cache Management ---
  void InvalidateStateCache();  // Call after device reset
  [[nodiscard]] const StateChangeStats& GetStateStats() const {
    return state_stats_;
  }
  void ResetStateStats() {
    state_stats_.Reset();
  }

  // --- Lighting ---
  void SetLight(int index, const D3DLIGHT9* light);
  void LightEnable(int index, bool enable);
  void SetMaterial(const D3DMATERIAL9* material);

  // --- Accessors ---
  [[nodiscard]] IDirect3DDevice9* GetDevice() const {
    return device;
  }
  [[nodiscard]] size_t GetAvailableTextureMem() const;
  [[nodiscard]] const RendererCapabilities& GetCapabilities() const {
    return capabilities_;
  }

  // --- Alpha Blending ---
  void SetAlphaBlend(bool enable);
  void SetAlphaBlendFunc(D3DBLEND src, D3DBLEND dest);

  // --- Z Buffer ---
  void SetZBuffer(bool enable);
  void SetZWrite(bool enable);
  void SetZFunc(D3DCMPFUNC func);

  // --- Culling ---
  void SetCullMode(D3DCULL mode);

  // --- Fog ---
  void SetFog(bool enable);
  void SetFogColor(unsigned long color);
  void SetFogStart(float start);
  void SetFogEnd(float end);
  void SetFogDensity(float density);
  void SetFogMode(int mode);

  // --- Fill Mode ---
  void SetFillMode(D3DFILLMODE mode);

  // --- Depth Bias ---
  void SetZBias(float bias);
};

// --- Global D3D9 State ---
// Exposed for D3D9VertexBufferImpl and D3D9TextureImpl resource management.

extern IDirect3DDevice9* g_D3D9Device;
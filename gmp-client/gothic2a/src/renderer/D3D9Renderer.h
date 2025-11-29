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

#include <array>
#include <memory>
#include <vector>

#include "AlphaPoly.h"
#include "D3D9DisplayModes.h"
#include "D3D9FogManager.h"
#include "ZenGin/zGothicAPI.h"

// Forward declarations.
struct D3D9RendererImpl;
struct IDirect3DDevice9;

namespace Gothic_II_Addon {
class zCMaterial;
class zCPolygon;
class zCVertexBuffer;
class zCTexture;
class zCLight;
struct zTRndSimpleVertex;
}  // namespace Gothic_II_Addon

// ----------------------------------------------------------------------------
// D3D9 Renderer Implementation for Gothic II
// Author: skejt23 | Created: 2025-11-29
// ----------------------------------------------------------------------------
// This class implements Gothic II's zCRenderer interface using Direct3D 9,
// replacing the original Direct3D 7 renderer. The implementation must maintain
// interface and behavioral compatibility with the original renderer because
// Gothic's engine has hardcoded expectations throughout its subsystems.
//
// Why compatibility matters:
// - Gothic's world rendering (zCWorld::Render), VOB system (zCVob::Render),
//   sky controller (zCSkyControler), particle effects (zCParticleFX), and many
//   other subsystems call renderer methods expecting specific behavior.
// - The alpha polygon sorting order, Z-buffer depth mapping, material activation
//   logic, and fog state management must produce equivalent visual results.
// - Deviating from expected behavior causes visual artifacts, Z-fighting,
//   incorrect transparency ordering, or crashes.
//
// Compatibility behaviors (required by Gothic's engine):
// - Standard perspective Z-buffer mapping: z_ndc = A + B/eye_z
// - Back-to-front alpha polygon sorting (painter's algorithm)
// - Two-pass lightmap rendering for world geometry
// - Radial (range-based) fog for outdoor scenes
//
// New features and optimizations:
// - Native Direct3D 9 (no legacy D3D7 compatibility layer overhead)
// - Widescreen and modern resolution support
// - Alpha polygon batching (~90% draw call reduction in typical scenes)
// - 512 depth buckets for alpha sorting (2x original for finer depth precision)
// - D3D9 render state caching (~50% cache hit rate, reduces API overhead)
// - Material and texture stage state caching
// ----------------------------------------------------------------------------
class zCRnd_D3D_DX9 : public zCRenderer {
public:
  zCRnd_D3D_DX9();
  ~zCRnd_D3D_DX9() override;

  // zCRenderer interface overrides (vtable must match exactly).
  void BeginFrame() override;
  void EndFrame() override;
  void FlushPolys() override;
  void DrawPoly(zCPolygon* poly) override;
  void DrawLightmapList(zCPolygon** polyList, int numPolys) override;
  void DrawLine(float x1, float y1, float x2, float y2, zCOLOR color) override;
  void DrawLineZ(float x1, float y1, float z1, float x2, float y2, float z2, zCOLOR color) override;
  void SetPixel(float x, float y, zCOLOR color) override;
  void DrawPolySimple(zCTexture* texture, zTRndSimpleVertex* vertices, int numVertices) override;
  void SetFog(int enable) override;
  int GetFog() const override;
  void SetRadialFog(int enable) override;
  int GetRadialFog() const override;
  void SetFogColor(const zCOLOR& color) override;
  zCOLOR GetFogColor() const override;
  void SetFogRange(float nearZ, float farZ, int mode) override;
  void GetFogRange(float& nearZ, float& farZ, int& mode) override;
  zTRnd_PolyDrawMode GetPolyDrawMode() const override;
  void SetPolyDrawMode(const zTRnd_PolyDrawMode& mode) override;
  int GetSurfaceLost() const override;
  void SetSurfaceLost(int lost) override;
  int GetSyncOnAmbientCol() const override;
  void SetSyncOnAmbientCol(int sync) override;
  void SetTextureWrapEnabled(int enable) override;
  int GetTextureWrapEnabled() const override;
  void SetBilerpFilterEnabled(int enable) override;
  int GetBilerpFilterEnabled() const override;
  void SetDitherEnabled(int enable) override;
  int GetDitherEnabled() const override;
  zTRnd_PolySortMode GetPolySortMode() const override;
  void SetPolySortMode(const zTRnd_PolySortMode& mode) override;
  int GetZBufferWriteEnabled() const override;
  void SetZBufferWriteEnabled(int enable) override;
  void SetZBias(int bias) override;
  int GetZBias() const override;
  zTRnd_ZBufferCmp GetZBufferCompare() override;
  void SetZBufferCompare(const zTRnd_ZBufferCmp& cmp) override;
  int GetPixelWriteEnabled() const override;
  void SetPixelWriteEnabled(int enable) override;
  void SetAlphaBlendSource(const zTRnd_AlphaBlendSource& src) override;
  zTRnd_AlphaBlendSource GetAlphaBlendSource() const override;
  void SetAlphaBlendFunc(const zTRnd_AlphaBlendFunc& func) override;
  zTRnd_AlphaBlendFunc GetAlphaBlendFunc() const override;
  float GetAlphaBlendFactor() const override;
  void SetAlphaBlendFactor(const float& factor) override;
  void SetAlphaReference(unsigned long ref) override;
  unsigned long GetAlphaReference() const override;
  int GetCacheAlphaPolys() const override;
  void SetCacheAlphaPolys(int cache) override;
  int GetAlphaLimitReached() const override;
  void AddAlphaPoly(const zCPolygon* poly) override;
  void FlushAlphaPolys() override;
  void SetRenderMode(zTRnd_RenderMode mode) override;
  zTRnd_RenderMode GetRenderMode() const override;
  int HasCapability(zTRnd_Capability cap) const override;
  void GetGuardBandBorders(float& left, float& right, float& top, float& bottom) override;
  void ResetZTest() override;
  int HasPassedZTest() override;
  zCTexture* CreateTexture() override;
  zCTextureConvert* CreateTextureConvert() override;
  int GetTotalTextureMem() override;
  int SupportsTextureFormat(zTRnd_TextureFormat fmt) override;
  int SupportsTextureFormatHardware(zTRnd_TextureFormat fmt) override;
  int GetMaxTextureSize() override;
  void GetStatistics(zTRnd_Stats& stats) override;
  void ResetStatistics() override;
  void Vid_Blit(int complete, tagRECT* src, tagRECT* dst) override;
  void Vid_Clear(zCOLOR& color, int flags) override;
  int Vid_Lock(zTRndSurfaceDesc& desc) override;
  int Vid_Unlock() override;
  int Vid_IsLocked() override;
  int Vid_GetFrontBufferCopy(zCTextureConvert& texConv) override;
  int Vid_GetNumDevices() override;
  int Vid_GetActiveDeviceNr() override;
  int Vid_SetDevice(int deviceNr) override;
  int Vid_GetDeviceInfo(zTRnd_DeviceInfo& info, int deviceNr) override;
  int Vid_GetNumModes() override;
  int Vid_GetModeInfo(zTRnd_VidModeInfo& info, int modeNr) override;
  int Vid_GetActiveModeNr() override;
  int Vid_SetMode(int modeNr, HWND__** hwnd) override;
  void Vid_SetScreenMode(zTRnd_ScreenMode mode) override;
  zTRnd_ScreenMode Vid_GetScreenMode() override;
  void Vid_SetGammaCorrection(float r, float g, float b) override;
  float Vid_GetGammaCorrection() override;
  void Vid_BeginLfbAccess() override;
  void Vid_EndLfbAccess() override;
  void Vid_SetLfbAlpha(int alpha) override;
  void Vid_SetLfbAlphaFunc(const zTRnd_AlphaBlendFunc& func) override;
  int SetTransform(zTRnd_TrafoType type, const zMAT4& matrix) override;
  int SetViewport(int x, int y, int w, int h) override;
  int SetLight(unsigned long index, zCRenderLight* light) override;
  int GetMaterial(zCRenderer::zTMaterial& mat) override;
  int SetMaterial(const zCRenderer::zTMaterial& mat) override;
  int SetTexture(unsigned long stage, zCTexture* texture) override;
  int SetTextureStageState(unsigned long stage, zTRnd_TextureStageState state, unsigned long value) override;
  int SetAlphaBlendFuncImmed(zTRnd_AlphaBlendFunc func) override;
  int SetRenderState(zTRnd_RenderStateType state, unsigned long value) override;
  unsigned long GetRenderState(zTRnd_RenderStateType state) override;
  void AddAlphaSortObject(zCRndAlphaSortObject* obj) override;
  void RenderAlphaSortList() override;
  int DrawVertexBuffer(zCVertexBuffer* buffer, int startVert, int numVert, unsigned short* indexList, unsigned long numIndices) override;
  zCVertexBuffer* CreateVertexBuffer() override;

  // Public accessors.
  [[nodiscard]] IDirect3DDevice9* GetDevice() const;

  // Renders a QueuedAlphaPoly from our D3D9-native alpha poly queue.
  void DrawQueuedAlphaPoly(const gmp::renderer::QueuedAlphaPoly* ap);

  // Restores render state for opaque geometry after alpha pass.
  void RestoreOpaqueRenderState();

  // Resets render state after alpha sort object rendering for subsequent alpha poly rendering.
  void ResetStateAfterAlphaSortObjects();

private:
  // Constants.
  static constexpr size_t kMaxTextureStages = 4;
  static constexpr size_t kMaxRenderStates = 160;
  static constexpr size_t kMaxTexStageStates = 24;
  static constexpr size_t kSamplerStateCount = 9;
  static constexpr size_t kMaxAlphaBuckets = 512;

  // Internal rendering helpers.
  void EnsureSceneBegun();
  bool ApplyRenderState(unsigned long state, unsigned long value);
  void ResetMultiTexturing();
  void InvalidateStateCache();  // Called after device recreation to force re-apply of render states
  [[nodiscard]] bool ActivateMaterial(zCMaterial* material);
  void ApplyTextureAddressMode();  // Sets texture wrapping/clamping
  void ApplyOpaqueRenderStates();  // Configures state for opaque geometry
  void ApplyAlphaTestStates();     // Configures state for alpha-tested geometry
  void DrawPolyVertexLit(zCPolygon* poly);
  void QueueAlphaPoly(zCPolygon* poly, zCMaterial* mat);
  void BuildAlphaPolyVertices(gmp::renderer::QueuedAlphaPoly* ap, const zCPolygon* poly, const zCMaterial* mat);
  [[nodiscard]] bool ApplyAlphaBlendState(gmp::renderer::AlphaBlendFunc blend_func, bool has_texture, bool texture_has_alpha);

  // Batched alpha polygon rendering helpers.
  void SetupAlphaRenderState(const gmp::renderer::AlphaRenderStateKey& state);
  void FlushAlphaBatch();
  void RenderAlphaPolyBatched(const gmp::renderer::QueuedAlphaPoly& poly);

  // D3D9 implementation details (pImpl pattern with RAII).
  std::unique_ptr<D3D9RendererImpl> impl_;

  // Display mode enumeration (initialized from impl_->d3d in Init).
  gmp::renderer::D3D9DisplayModes display_modes_;

  // Alpha polygon batcher for reducing draw calls.
  gmp::renderer::AlphaPolyBatcher alpha_batcher_;

  // Render state tracking (cached to avoid redundant D3D calls).
  std::array<unsigned long, kMaxRenderStates> render_state_cache_{};
  std::array<std::array<unsigned long, kMaxTexStageStates>, kMaxTextureStages> tex_stage_state_cache_{};
  std::array<std::array<unsigned long, kSamplerStateCount>, kMaxTextureStages> sampler_state_cache_{};

  // Active status flags (for alpha poly state restoration).
  struct {
    int texwrap = 1;
    int filter = 1;
    int dither = 1;
    int fog = 0;
    float nearZ = 0.0f;
    float farZ = 0.0f;
    zTRnd_AlphaBlendFunc alphafunc = zRND_ALPHA_FUNC_NONE;
    zTRnd_AlphaBlendSource alphasrc = zRND_ALPHA_SOURCE_MATERIAL;
    float alphafactor = 1.0f;
    int zenable = 1;
    zTRnd_ZBufferCmp zfunc = zRND_ZBUFFER_CMP_LESS;
    int zwrite = 1;
    int zbias = 0;
    unsigned long zd3dfunc = 0;
    zTRnd_RenderMode renderMode = zRND_MODE_1_PASS_VERT_LIGHT;
  } active_status_;

  // Screen/window state.
  int screen_width_ = 0;
  int screen_height_ = 0;
  zTRnd_ScreenMode screen_mode_ = zRND_SCRMODE_WINDOWED;
  HWND window_handle_ = nullptr;

  // Render state.
  int render_mode_ = 0;
  int vb_clipping_ = 1;

  // Transform matrices (stored for persistence across frames).
  zMAT4 view_matrix_{};
  zMAT4 proj_matrix_{};

  // Z buffer scaling (from clip plane setup).
  // Perspective depth mapping: z_ndc = z_proj_offset_ + z_proj_scale_ / eye_z
  // Where: z_proj_offset_ = zFar / (zFar - zNear)
  //        z_proj_scale_ = -zFar * zNear / (zFar - zNear)
  float z_max_from_engine_ = 2.0f;
  float z_min_from_engine_ = 0.25f;
  float z_proj_offset_ = 1.0f;
  float z_proj_scale_ = 0.0f;

  // Fog management (see D3D9FogManager.h for documentation).
  gmp::renderer::D3D9FogManager fog_manager_;

  // Depth buffer state.
  int z_bias_ = 0;
  int z_buffer_write_enabled_ = 1;
  zTRnd_ZBufferCmp z_buffer_cmp_ = zRND_ZBUFFER_CMP_LESS;

  // Alpha blending state.
  zTRnd_AlphaBlendSource alpha_blend_source_ = zRND_ALPHA_SOURCE_MATERIAL;
  zTRnd_AlphaBlendFunc alpha_blend_func_ = zRND_ALPHA_FUNC_NONE;
  float alpha_blend_factor_ = 1.0f;
  unsigned long alpha_reference_ = 128;
  int alpha_limit_reached_ = 0;
  int cache_alpha_polys_ = 1;
  int alpha_test_enabled_ = 1;
  int alpha_blend_immed_ = 0;
  unsigned long texture_factor_ = 0;

  // Alpha polygon sorting.
  int num_alpha_polys_ = 0;
  std::array<zCRndAlphaSortObject*, kMaxAlphaBuckets> alpha_sort_bucket_{};
  float bucket_size_ = 0.0f;
  gmp::renderer::AlphaPolyQueue alpha_poly_queue_;
  gmp::renderer::AlphaPolyQueue immediate_alpha_poly_queue_;

  // Gamma correction state (0.5 = neutral, range 0.1-0.9).
  float gamma_ = 0.5f;

  // Texture state.
  std::array<zCTexture*, kMaxTextureStages> active_texture_{};
  int texture_wrap_enabled_ = 1;
  int bilerp_filter_enabled_ = 1;
  int dither_enabled_ = 1;

  // Material state.
  zCMaterial* active_material_ = nullptr;
  zCRenderer::zTMaterial current_material_ = {};

  // Render state.
  int pixel_write_enabled_ = 1;
  int surface_lost_ = 0;
  int must_flush_on_ambient_color_ = 0;
  zTRnd_PolySortMode poly_sort_mode_ = zRND_SORT_NONE;
  zTRnd_PolyDrawMode poly_draw_mode_ = zRND_DRAW_MATERIAL;

  // Polygon batching.
  std::vector<zCPolygon*> poly_batch_;
};

// Factory functions for the hook.
zCRenderer* __stdcall CreateDX9Renderer();
void __fastcall ConstructDX9Renderer(void* mem);

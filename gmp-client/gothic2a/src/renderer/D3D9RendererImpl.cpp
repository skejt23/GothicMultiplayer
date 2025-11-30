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

#include "D3D9RendererImpl.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <ranges>

#include "DynamicVertexBuffer.h"

// --- Global D3D9 State ---

IDirect3DDevice9* g_D3D9Device = nullptr;

// --- Anonymous Namespace Helpers ---

namespace {

D3DFORMAT SelectDepthStencilFormat(IDirect3D9* d3d, D3DFORMAT backBufferFormat) {
  // Try D3DFMT_D24S8 first (24-bit depth + 8-bit stencil) - most common and well-supported
  if (SUCCEEDED(d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, backBufferFormat, backBufferFormat, D3DFMT_D24S8))) {
    if (SUCCEEDED(
            d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, backBufferFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8))) {
      SPDLOG_INFO("Selected D24S8 depth-stencil format (24-bit depth, 8-bit stencil)");
      return D3DFMT_D24S8;
    }
  }

  // Fallback to D3DFMT_D16 (16-bit depth, no stencil) for older hardware
  if (SUCCEEDED(d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, backBufferFormat, backBufferFormat, D3DFMT_D16))) {
    if (SUCCEEDED(
            d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, backBufferFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16))) {
      SPDLOG_INFO("Selected D16 depth-stencil format (16-bit depth, no stencil)");
      return D3DFMT_D16;
    }
  }
  SPDLOG_INFO("Selected D32 depth-stencil format (32-bit depth, no stencil)");
  return D3DFMT_D32;
}

std::uint32_t ComputePrimitiveCount(D3DPRIMITIVETYPE primType, std::uint32_t vertexCount, std::uint32_t indexCount) {
  const bool use_index_data = indexCount > 0;
  const std::uint32_t span = use_index_data ? indexCount : vertexCount;
  switch (primType) {
    case D3DPT_POINTLIST:
      return span;
    case D3DPT_LINELIST:
      return span / 2u;
    case D3DPT_LINESTRIP:
      return span >= 2u ? span - 1u : 0u;
    case D3DPT_TRIANGLELIST:
      return span / 3u;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:
      return span >= 3u ? span - 2u : 0u;
    default:
      return 0u;
  }
}

bool EnsureIndexBufferCapacity(D3D9RendererImpl& data, std::uint32_t indexCount) {
  if (indexCount == 0) {
    return true;
  }

  if (data.dynamic_index_buffer && data.dynamic_index_capacity >= indexCount) {
    return true;
  }

  if (data.dynamic_index_buffer) {
    data.dynamic_index_buffer->Release();
    data.dynamic_index_buffer = nullptr;
    data.dynamic_index_capacity = 0;
  }

  std::uint32_t new_capacity = indexCount;
  // Start with a larger minimum capacity to avoid frequent reallocations.
  // Modern systems have plenty of RAM; 4096 indices = 8KB.
  constexpr std::uint32_t kMinCapacity = 4096u;
  if (new_capacity < kMinCapacity) {
    new_capacity = kMinCapacity;
  }

  HRESULT hr = data.device->CreateIndexBuffer(new_capacity * sizeof(unsigned short), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                              D3DPOOL_DEFAULT, &data.dynamic_index_buffer, nullptr);

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create dynamic index buffer. Error: {:x}", static_cast<unsigned long>(hr));
    data.dynamic_index_capacity = 0;
    return false;
  }

  data.dynamic_index_capacity = new_capacity;
  return true;
}

}  // namespace

// --- D3D9RendererImpl Implementation ---

bool D3D9RendererImpl::Init(void* hwnd, int width, int height, bool fullscreen) {
  SPDLOG_TRACE("Initializing D3D9RendererImpl...");

  if (d3d || device) {
    SPDLOG_TRACE("D3D9RendererImpl already initialized, cleaning up first...");
    Cleanup();
  }

  d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d) {
    SPDLOG_ERROR("Direct3DCreate9 failed!");
    return false;
  }

  // Verify hardware T&L support - reject pre-2000 era cards without it
  D3DCAPS9 caps;
  if (FAILED(d3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps))) {
    SPDLOG_ERROR("GetDeviceCaps failed - cannot determine hardware capabilities");
    d3d->Release();
    d3d = nullptr;
    return false;
  }

  if (!(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)) {
    SPDLOG_ERROR("Hardware Transform & Lighting (T&L) not supported. This renderer requires a GPU from 2000 or later.");
    d3d->Release();
    d3d = nullptr;
    return false;
  }

  // Use provided dimensions
  std::uint32_t back_buffer_width = static_cast<std::uint32_t>(width);
  std::uint32_t back_buffer_height = static_cast<std::uint32_t>(height);

  if (back_buffer_width == 0 || back_buffer_height == 0) {
    back_buffer_width = 800;
    back_buffer_height = 600;
    SPDLOG_WARN("Requested size is 0x0, using fallback size: {}x{}", back_buffer_width, back_buffer_height);
  }

  present_params = {};
  present_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  present_params.hDeviceWindow = static_cast<HWND>(hwnd);
  present_params.BackBufferFormat = D3DFMT_X8R8G8B8;
  present_params.BackBufferWidth = back_buffer_width;
  present_params.BackBufferHeight = back_buffer_height;
  // Triple buffering: 2 back buffers + 1 front buffer = smoother frame pacing
  // and reduced input lag compared to double buffering (1 back buffer).
  present_params.BackBufferCount = 2;
  present_params.EnableAutoDepthStencil = TRUE;
  present_params.AutoDepthStencilFormat = SelectDepthStencilFormat(d3d, present_params.BackBufferFormat);
  present_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

  if (fullscreen) {
    // Find matching fullscreen display mode
    D3DDISPLAYMODE display_mode{};
    bool found_mode = false;
    UINT best_refresh_rate = 0;

    UINT mode_count = d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
    for (UINT i = 0; i < mode_count; ++i) {
      D3DDISPLAYMODE mode{};
      if (SUCCEEDED(d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode))) {
        if (mode.Width == back_buffer_width && mode.Height == back_buffer_height) {
          if (mode.RefreshRate > best_refresh_rate) {
            display_mode = mode;
            best_refresh_rate = mode.RefreshRate;
            found_mode = true;
          }
        }
      }
    }

    if (found_mode) {
      SPDLOG_INFO("Init: Creating fullscreen device: {}x{}@{}Hz", display_mode.Width, display_mode.Height, display_mode.RefreshRate);
      present_params.Windowed = FALSE;
      present_params.BackBufferFormat = display_mode.Format;
      present_params.FullScreen_RefreshRateInHz = display_mode.RefreshRate;
    } else {
      SPDLOG_WARN("Init: No fullscreen mode found for {}x{}, falling back to windowed", back_buffer_width, back_buffer_height);
      present_params.Windowed = TRUE;
      present_params.FullScreen_RefreshRateInHz = 0;
    }
  } else {
    SPDLOG_INFO("Init: Creating windowed device: {}x{}", back_buffer_width, back_buffer_height);
    present_params.Windowed = TRUE;
    present_params.FullScreen_RefreshRateInHz = 0;
  }

  HRESULT hr_create =
      d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, static_cast<HWND>(hwnd),
                        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &present_params, &device);
  if (FAILED(hr_create)) {
    SPDLOG_ERROR("CreateDevice failed with HRESULT: 0x{:08X}", static_cast<unsigned long>(hr_create));
    return false;
  }

  g_D3D9Device = device;

  // Create ring buffers for dynamic batching
  // General batch buffer: 256KB vertex, 48KB index (enough for large batches)
  constexpr size_t kBatchVBSize = 256 * 1024;  // 256 KB
  constexpr size_t kBatchIBCount = 24 * 1024;  // 24K indices (48 KB)
  if (!batch_ring_vb_.Create(device, kBatchVBSize, kFvfVertexRhw)) {
    SPDLOG_WARN("Failed to create batch ring vertex buffer, falling back to DrawPrimitiveUP");
  }
  if (!batch_ring_ib_.Create(device, kBatchIBCount)) {
    SPDLOG_WARN("Failed to create batch ring index buffer, falling back to DrawPrimitiveUP");
  }

  // Alpha batch buffer: 128KB vertex, 24KB index (alpha polys are typically smaller)
  constexpr size_t kAlphaVBSize = 128 * 1024;  // 128 KB
  constexpr size_t kAlphaIBCount = 12 * 1024;  // 12K indices (24 KB)
  if (!alpha_ring_vb_.Create(device, kAlphaVBSize, kFvfVertexRhw)) {
    SPDLOG_WARN("Failed to create alpha ring vertex buffer");
  }
  if (!alpha_ring_ib_.Create(device, kAlphaIBCount)) {
    SPDLOG_WARN("Failed to create alpha ring index buffer");
  }

  batch_.Reset();
  batch_stats_.Reset();
  state_stats_.Reset();

  // Initialize state caches with invalid sentinel values
  InvalidateStateCache();

  // Use caps already retrieved during T&L check
  max_anisotropy = std::clamp<DWORD>(caps.MaxAnisotropy, 1u, 16u);
  max_simultaneous_textures = std::clamp<DWORD>(caps.MaxSimultaneousTextures, 1u, 8u);

  SPDLOG_DEBUG("D3D9 Device Caps: MaxAnisotropy={}, MaxSimultaneousTextures={}", max_anisotropy, max_simultaneous_textures);

  capabilities_.max_texture_size = (std::min)(caps.MaxTextureWidth, caps.MaxTextureHeight);

  D3DMATRIX identity{};
  identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
  device->SetTransform(D3DTS_WORLD, &identity);
  device->SetTransform(D3DTS_VIEW, &identity);
  device->SetTransform(D3DTS_PROJECTION, &identity);

  device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
  device->SetRenderState(D3DRS_DITHERENABLE, TRUE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->SetRenderState(D3DRS_CLIPPING, FALSE);
  device->SetRenderState(D3DRS_LIGHTING, FALSE);
  device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
  device->SetRenderState(D3DRS_DEPTHBIAS, 0);
  device->SetRenderState(D3DRS_AMBIENT, 0);
  device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
  device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

  for (DWORD t = 0; t < 8; ++t) {
    device->SetTexture(t, nullptr);
    bound_textures[t] = nullptr;
  }

  D3DMATERIAL9 default_material{};
  default_material.Diffuse.r = 1.0f;
  default_material.Diffuse.g = 1.0f;
  default_material.Diffuse.b = 1.0f;
  default_material.Diffuse.a = 1.0f;
  default_material.Ambient.r = 1.0f;
  default_material.Ambient.g = 1.0f;
  default_material.Ambient.b = 1.0f;
  default_material.Ambient.a = 1.0f;
  device->SetMaterial(&default_material);

  // Loop through all texture stages the hardware supports
  DWORD num_tex_stages = (std::min<DWORD>)(max_simultaneous_textures, 8);
  for (DWORD stage = 0; stage < num_tex_stages; ++stage) {
    // Sampler states (D3D9 separates these from texture stage states)
    device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
    device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    device->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, max_anisotropy);
    device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    // Slight negative LOD bias makes distant textures sharper by selecting
    // higher-resolution mip levels. -0.5 is a conservative value that improves
    // clarity without excessive texture shimmer/aliasing.
    device->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, std::bit_cast<DWORD>(-0.5f));

    device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

    // Set color/alpha ops - stage 0 modulates, others disabled
    if (stage == 0) {
      device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
      device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    } else {
      device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
      device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
  }

  // Create state blocks for common render modes (after all default state is set)
  CreateStateBlocks();

  HRESULT hr_begin = device->BeginScene();
  if (SUCCEEDED(hr_begin)) {
    in_scene = true;
  }

  return true;
}

bool D3D9RendererImpl::Reset(int width, int height) {
  if (!device) {
    SPDLOG_ERROR("Reset called with null device");
    return false;
  }

  // Flush any pending batched geometry
  FlushBatch();

  // End the current scene if active
  if (in_scene) {
    device->EndScene();
    in_scene = false;
  }

  // Release D3DPOOL_DEFAULT resources before Reset
  // (D3DPOOL_MANAGED resources survive Reset automatically)
  batch_ring_vb_.Release();
  batch_ring_ib_.Release();
  alpha_ring_vb_.Release();
  alpha_ring_ib_.Release();
  if (dynamic_index_buffer) {
    dynamic_index_buffer->Release();
    dynamic_index_buffer = nullptr;
  }
  dynamic_index_capacity = 0;

  // Release state blocks before device reset
  ReleaseStateBlocks();

  // Update present parameters for new resolution
  present_params.BackBufferWidth = width;
  present_params.BackBufferHeight = height;

  // Reset the device
  HRESULT hr = device->Reset(&present_params);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Device Reset failed with HRESULT: 0x{:08X}", static_cast<unsigned long>(hr));
    return false;
  }

  SPDLOG_INFO("Device Reset successful: {}x{}", width, height);

  // Recreate ring buffers
  constexpr size_t kBatchVBSize = 256 * 1024;
  constexpr size_t kBatchIBCount = 24 * 1024;
  if (!batch_ring_vb_.Recreate(device)) {
    SPDLOG_WARN("Failed to recreate batch ring vertex buffer after Reset");
  }
  if (!batch_ring_ib_.Recreate(device)) {
    SPDLOG_WARN("Failed to recreate batch ring index buffer after Reset");
  }

  constexpr size_t kAlphaVBSize = 128 * 1024;
  constexpr size_t kAlphaIBCount = 12 * 1024;
  if (!alpha_ring_vb_.Recreate(device)) {
    SPDLOG_WARN("Failed to recreate alpha ring vertex buffer after Reset");
  }
  if (!alpha_ring_ib_.Recreate(device)) {
    SPDLOG_WARN("Failed to recreate alpha ring index buffer after Reset");
  }

  batch_.Reset();
  batch_stats_.Reset();

  // Invalidate state caches (all states are lost after device Reset)
  InvalidateStateCache();

  // Restore default render states (they are lost after Reset)
  device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
  device->SetRenderState(D3DRS_DITHERENABLE, TRUE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->SetRenderState(D3DRS_CLIPPING, FALSE);
  device->SetRenderState(D3DRS_LIGHTING, FALSE);
  device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
  device->SetRenderState(D3DRS_DEPTHBIAS, 0);
  device->SetRenderState(D3DRS_AMBIENT, 0);
  device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
  device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

  // Clear texture bindings
  for (DWORD t = 0; t < 8; ++t) {
    device->SetTexture(t, nullptr);
    bound_textures[t] = nullptr;
  }

  // Restore default material
  D3DMATERIAL9 default_material{};
  default_material.Diffuse.r = 1.0f;
  default_material.Diffuse.g = 1.0f;
  default_material.Diffuse.b = 1.0f;
  default_material.Diffuse.a = 1.0f;
  default_material.Ambient.r = 1.0f;
  default_material.Ambient.g = 1.0f;
  default_material.Ambient.b = 1.0f;
  default_material.Ambient.a = 1.0f;
  device->SetMaterial(&default_material);

  // Restore texture stage states
  DWORD num_tex_stages = (std::min<DWORD>)(max_simultaneous_textures, 8);
  for (DWORD stage = 0; stage < num_tex_stages; ++stage) {
    device->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
    device->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    device->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, max_anisotropy);
    device->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(stage, D3DSAMP_MIPMAPLODBIAS, std::bit_cast<DWORD>(-0.5f));

    device->SetTextureStageState(stage, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(stage, D3DTSS_COLORARG2, D3DTA_CURRENT);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(stage, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

    if (stage == 0) {
      device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_MODULATE);
      device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    } else {
      device->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
      device->SetTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
  }

  // Recreate state blocks after device reset
  CreateStateBlocks();

  // Begin new scene
  HRESULT hr_begin = device->BeginScene();
  if (SUCCEEDED(hr_begin)) {
    in_scene = true;
  }

  return true;
}

void D3D9RendererImpl::Cleanup() {
  // Flush any pending batched geometry before cleanup
  FlushBatch();

  in_scene = false;

  // Release state blocks
  ReleaseStateBlocks();

  // Release ring buffers
  batch_ring_vb_.Release();
  batch_ring_ib_.Release();
  alpha_ring_vb_.Release();
  alpha_ring_ib_.Release();

  if (dynamic_index_buffer) {
    dynamic_index_buffer->Release();
    dynamic_index_buffer = nullptr;
  }
  dynamic_index_capacity = 0;

  if (device) {
    device->Release();
    device = nullptr;
  }
  g_D3D9Device = nullptr;

  if (d3d) {
    d3d->Release();
    d3d = nullptr;
  }
}

void D3D9RendererImpl::BeginFrame() {
  if (device && in_scene) {
    D3DVIEWPORT9 vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = present_params.BackBufferWidth;
    vp.Height = present_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    device->SetViewport(&vp);
  }

  // Reset batch stats for new frame
  batch_stats_.Reset();

  // Begin new frame for ring buffers (may trigger DISCARD if near capacity)
  batch_ring_vb_.BeginFrame();
  batch_ring_ib_.BeginFrame();
  alpha_ring_vb_.BeginFrame();
  alpha_ring_ib_.BeginFrame();
}

void D3D9RendererImpl::EndFrame() {
  // Flush any remaining batched geometry before frame end
  FlushBatch();

  SPDLOG_TRACE("Frame batch stats: {} polys batched, {} batches flushed, {} draw calls saved", batch_stats_.polygons_batched,
               batch_stats_.batches_flushed, batch_stats_.draw_calls_saved);

  // Log state cache and ring buffer stats periodically (every 60 frames)
  static int frame_counter = 0;
  if (++frame_counter >= 60) {
    const auto& stats = state_stats_;
    const size_t total_calls = stats.GetTotalApiCalls();
    const size_t total_hits = stats.GetTotalCacheHits();
    if (total_calls + total_hits > 0) {
      const float hit_rate = static_cast<float>(total_hits) / static_cast<float>(total_calls + total_hits) * 100.0f;
      SPDLOG_DEBUG("State cache: {} API calls, {} cache hits ({:.1f}% hit rate)", total_calls, total_hits, hit_rate);
    }

    // Log ring buffer efficiency
    const size_t vb_discards = batch_ring_vb_.GetDiscardCount() + alpha_ring_vb_.GetDiscardCount();
    const size_t vb_nooverwrite = batch_ring_vb_.GetNoOverwriteCount() + alpha_ring_vb_.GetNoOverwriteCount();
    if (vb_discards + vb_nooverwrite > 0) {
      const float nooverwrite_rate = static_cast<float>(vb_nooverwrite) / static_cast<float>(vb_discards + vb_nooverwrite) * 100.0f;
      SPDLOG_DEBUG("Ring buffer: {} discards, {} nooverwrite ({:.1f}% stall-free)", vb_discards, vb_nooverwrite, nooverwrite_rate);
    }

    batch_ring_vb_.ResetStats();
    batch_ring_ib_.ResetStats();
    alpha_ring_vb_.ResetStats();
    alpha_ring_ib_.ResetStats();
    frame_counter = 0;
  }
  state_stats_.Reset();
}

void D3D9RendererImpl::Vid_Blit() {
  if (device) {
    // Flush any pending geometry before presenting
    FlushBatch();

    if (in_scene) {
      device->EndScene();
      in_scene = false;
    }

    device->Present(nullptr, nullptr, nullptr, nullptr);

    D3DVIEWPORT9 vp;
    vp.X = 0;
    vp.Y = 0;
    vp.Width = present_params.BackBufferWidth;
    vp.Height = present_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    device->SetViewport(&vp);

    if (SUCCEEDED(device->BeginScene())) {
      in_scene = true;
    }
  }
}

void D3D9RendererImpl::Present() {
  if (device) {
    device->Present(nullptr, nullptr, nullptr, nullptr);
  }
}

void D3D9RendererImpl::Clear(unsigned long color) {
  if (device) {
    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, color, 1.0f, 0);
  }
}

void D3D9RendererImpl::DrawTriangles(const Vertex3D* vertices, int count) {
  if (device && count > 0) {
    // DrawTriangles uses pre-lit vertex colors (lightDyn), so disable hardware lighting
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetFVF(kFvfVertex3D);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count, vertices, sizeof(Vertex3D));
  }
}

void D3D9RendererImpl::DrawTrianglesRHW(const VertexRHW* vertices, int count) {
  if (device && count > 0) {
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetFVF(kFvfVertexRhw);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, vertices, sizeof(VertexRHW));
  }
}

void D3D9RendererImpl::DrawTriangleFan(const VertexRHW* vertices, int count) {
  if (device && count >= 3) {
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, FALSE);
    device->SetFVF(kFvfVertexRhw);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count - 2, vertices, sizeof(VertexRHW));
  }
}

// BatchTriangleFan - Accumulates triangle fan vertices for batched rendering.
//
// Instead of issuing a draw call per polygon, this method:
// 1. Converts the triangle fan to indexed triangles
// 2. Accumulates vertices and indices in a batch buffer
// 3. Flushes when texture changes or buffer is full
//
// A triangle fan with N vertices produces N-2 triangles.
// Each triangle is stored as 3 indices pointing to the shared vertex 0.
//
void D3D9RendererImpl::BatchTriangleFan(const VertexRHW* vertices, int count, void* texture) {
  if (!device || count < 3) {
    return;
  }

  // If ring buffers are not available, fall back to immediate drawing
  if (!batch_ring_vb_.IsValid() || !batch_ring_ib_.IsValid()) {
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, FALSE);
    device->SetFVF(kFvfVertexRhw);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count - 2, vertices, sizeof(VertexRHW));
    return;
  }

  // Calculate required space
  const size_t num_triangles = count - 2;
  const size_t required_indices = num_triangles * 3;

  // Check if we need to flush due to texture change or space exhaustion
  if (batch_.is_active && batch_.current_texture != texture) {
    FlushBatch();
  }

  if (!batch_.HasRoom(count, required_indices)) {
    FlushBatch();
  }

  // Start new batch if needed
  if (!batch_.is_active) {
    batch_.is_active = true;
    batch_.current_texture = texture;
  }

  // Record base vertex index for this polygon
  const unsigned short base_vertex = static_cast<unsigned short>(batch_.vertex_count);

  // Copy vertices to batch
  std::memcpy(&batch_.vertices[batch_.vertex_count], vertices, count * sizeof(VertexRHW));

  // Convert triangle fan to indexed triangles
  // Fan: v0 is shared, triangles are (0,1,2), (0,2,3), (0,3,4), etc.
  for (size_t i = 0; i < num_triangles; ++i) {
    batch_.indices[batch_.index_count++] = base_vertex;
    batch_.indices[batch_.index_count++] = base_vertex + static_cast<unsigned short>(i + 1);
    batch_.indices[batch_.index_count++] = base_vertex + static_cast<unsigned short>(i + 2);
  }

  batch_.vertex_count += count;
  ++batch_stats_.polygons_batched;
  ++batch_stats_.draw_calls_saved;  // We saved one draw call by batching
}

// FlushBatch - Renders all accumulated batch geometry in a single draw call.
//
// This is where the batching payoff happens:
// - Multiple polygons that were added via BatchTriangleFan are drawn together
// - Uses ring buffer pattern with NOOVERWRITE/DISCARD for efficient streaming
// - Reduces driver overhead from many small draw calls
//
void D3D9RendererImpl::FlushBatch() {
  if (!batch_.is_active || batch_.vertex_count == 0 || batch_.index_count == 0) {
    batch_.Reset();
    return;
  }

  if (!device || !batch_ring_vb_.IsValid() || !batch_ring_ib_.IsValid()) {
    batch_.Reset();
    return;
  }

  // Allocate space in ring buffers
  size_t vb_offset = 0;
  void* vb_data = batch_ring_vb_.Allocate(batch_.vertex_count, sizeof(VertexRHW), vb_offset);
  if (!vb_data) {
    SPDLOG_ERROR("FlushBatch: Failed to allocate vertex buffer space");
    batch_.Reset();
    return;
  }

  // Copy vertex data
  std::memcpy(vb_data, batch_.vertices, batch_.vertex_count * sizeof(VertexRHW));
  batch_ring_vb_.Unlock();

  size_t ib_start_index = 0;
  uint16_t* ib_data = batch_ring_ib_.Allocate(batch_.index_count, ib_start_index);
  if (!ib_data) {
    SPDLOG_ERROR("FlushBatch: Failed to allocate index buffer space");
    batch_.Reset();
    return;
  }

  // Copy index data
  std::memcpy(ib_data, batch_.indices, batch_.index_count * sizeof(uint16_t));
  batch_ring_ib_.Unlock();

  // Set up render state
  device->SetRenderState(D3DRS_LIGHTING, FALSE);
  device->SetRenderState(D3DRS_CLIPPING, FALSE);
  device->SetFVF(kFvfVertexRhw);
  device->SetStreamSource(0, batch_ring_vb_.GetBuffer(), static_cast<UINT>(vb_offset), sizeof(VertexRHW));
  device->SetIndices(batch_ring_ib_.GetBuffer());

  // Draw all batched triangles in one call
  const std::uint32_t num_triangles = static_cast<std::uint32_t>(batch_.index_count / 3);
  device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                               0,                                                // BaseVertexIndex
                               0,                                                // MinVertexIndex
                               static_cast<std::uint32_t>(batch_.vertex_count),  // NumVertices
                               static_cast<UINT>(ib_start_index),                // StartIndex
                               num_triangles                                     // PrimitiveCount
  );

  // Clear stream source to avoid issues with other draw calls
  device->SetStreamSource(0, nullptr, 0, 0);
  device->SetIndices(nullptr);

  ++batch_stats_.batches_flushed;

  // Adjust saved draw calls: we issued 1 draw call instead of N
  // So actual savings = polygons_batched_this_batch - 1
  // But we already counted each BatchTriangleFan as a saved call,
  // so we need to subtract 1 for the flush we just did
  if (batch_stats_.draw_calls_saved > 0) {
    --batch_stats_.draw_calls_saved;
  }

  batch_.Reset();
}

// DrawAlphaBatch - Renders a batch of alpha polygons in a single draw call.
//
// This function is optimized for alpha polygon rendering:
// - Uses dedicated alpha ring buffers with NOOVERWRITE for streaming
// - Caller is responsible for setting all render state beforehand
// - Returns true on success, false if buffers not available or draw failed.
//
bool D3D9RendererImpl::DrawAlphaBatch(const VertexRHW* vertices, size_t vertex_count, const uint16_t* indices, size_t index_count) {
  if (!device || !vertices || !indices || vertex_count == 0 || index_count == 0) {
    return false;
  }

  // Check if alpha ring buffers are available
  if (!alpha_ring_vb_.IsValid() || !alpha_ring_ib_.IsValid()) {
    // Fall back to DrawIndexedPrimitiveUP (slower but works)
    device->SetFVF(kFvfVertexRhw);
    device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(vertex_count), static_cast<UINT>(index_count / 3), indices,
                                   D3DFMT_INDEX16, vertices, sizeof(VertexRHW));
    return true;
  }

  // Allocate space in ring buffers
  size_t vb_offset = 0;
  void* vb_data = alpha_ring_vb_.Allocate(vertex_count, sizeof(VertexRHW), vb_offset);
  if (!vb_data) {
    SPDLOG_ERROR("DrawAlphaBatch: Failed to allocate vertex buffer space");
    return false;
  }

  std::memcpy(vb_data, vertices, vertex_count * sizeof(VertexRHW));
  alpha_ring_vb_.Unlock();

  size_t ib_start_index = 0;
  uint16_t* ib_data = alpha_ring_ib_.Allocate(index_count, ib_start_index);
  if (!ib_data) {
    SPDLOG_ERROR("DrawAlphaBatch: Failed to allocate index buffer space");
    return false;
  }

  std::memcpy(ib_data, indices, index_count * sizeof(uint16_t));
  alpha_ring_ib_.Unlock();

  // Set up render state
  device->SetRenderState(D3DRS_LIGHTING, FALSE);
  device->SetRenderState(D3DRS_CLIPPING, FALSE);
  device->SetFVF(kFvfVertexRhw);
  device->SetStreamSource(0, alpha_ring_vb_.GetBuffer(), static_cast<UINT>(vb_offset), sizeof(VertexRHW));
  device->SetIndices(alpha_ring_ib_.GetBuffer());

  // Draw the batch
  const UINT prim_count = static_cast<UINT>(index_count / 3);
  HRESULT hr_draw =
      device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, static_cast<UINT>(vertex_count), static_cast<UINT>(ib_start_index), prim_count);

  // Clear stream source to avoid issues
  device->SetStreamSource(0, nullptr, 0, 0);
  device->SetIndices(nullptr);

  return SUCCEEDED(hr_draw);
}

void D3D9RendererImpl::DrawTriangleFan2(const VertexRHW2* vertices, int count) {
  if (device && count >= 3) {
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_CLIPPING, FALSE);
    device->SetFVF(kFvfVertexRhw2);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, count - 2, vertices, sizeof(VertexRHW2));
  }
}

void D3D9RendererImpl::DrawLine(float x1, float y1, float x2, float y2, unsigned long color) {
  if (device) {
    VertexRHW verts[2];
    verts[0] = {x1, y1, 0.0f, 1.0f, color, 0.0f, 0.0f};
    verts[1] = {x2, y2, 0.0f, 1.0f, color, 0.0f, 0.0f};
    device->SetFVF(kFvfVertexRhw);
    device->DrawPrimitiveUP(D3DPT_LINELIST, 1, verts, sizeof(VertexRHW));
  }
}

void D3D9RendererImpl::SetViewport(int x, int y, int width, int height) {
  if (!device)
    return;
  if (width <= 0 || height <= 0)
    return;

  D3DVIEWPORT9 vp;
  vp.X = x;
  vp.Y = y;
  vp.Width = width;
  vp.Height = height;
  vp.MinZ = 0.0f;
  vp.MaxZ = 1.0f;
  device->SetViewport(&vp);
}

void D3D9RendererImpl::SetTransform(int state, const float* matrix) {
  if (device) {
    D3DTRANSFORMSTATETYPE type;
    switch (state) {
      case 1:
        type = D3DTS_WORLD;
        break;
      case 2:
        type = D3DTS_VIEW;
        break;
      case 3:
        type = D3DTS_PROJECTION;
        break;
      case 4:
        type = D3DTS_TEXTURE0;
        break;
      case 5:
        type = D3DTS_TEXTURE1;
        break;
      case 6:
        type = D3DTS_TEXTURE2;
        break;
      case 7:
        type = D3DTS_TEXTURE3;
        break;
      default:
        return;
    }
    device->SetTransform(type, reinterpret_cast<const D3DMATRIX*>(matrix));
  }
}

void D3D9RendererImpl::SetTexture(int stage, void* texture) {
  if (device) {
    if (stage >= 0 && stage < 8) {
      bound_textures[stage] = texture;
    }
    // Use cached version to avoid redundant D3D9 calls
    SetTextureCached(static_cast<DWORD>(stage), static_cast<IDirect3DBaseTexture9*>(texture));
  }
}

void D3D9RendererImpl::SetTextureWrap(int stage, bool enable) {
  if (device) {
    D3DTEXTUREADDRESS mode = enable ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
    device->SetSamplerState(stage, D3DSAMP_ADDRESSU, mode);
    device->SetSamplerState(stage, D3DSAMP_ADDRESSV, mode);
  }
}

void D3D9RendererImpl::SetTextureFilter(int stage, int filter) {
  if (!device)
    return;
  int sampler_stage = stage;
  if (stage < 0 || stage >= static_cast<int>(max_simultaneous_textures)) {
    sampler_stage = std::clamp(stage, 0, static_cast<int>(max_simultaneous_textures) - 1);
  }

  D3DTEXTUREFILTERTYPE min_mag = D3DTEXF_POINT;
  D3DTEXTUREFILTERTYPE mip = D3DTEXF_POINT;
  DWORD aniso = 1;

  switch (filter) {
    case 2:
      min_mag = D3DTEXF_ANISOTROPIC;
      mip = D3DTEXF_LINEAR;
      aniso = 16;
      break;
    case 1:
      // Bilinear filtering
      min_mag = D3DTEXF_LINEAR;
      mip = D3DTEXF_LINEAR;
      aniso = 1;
      break;
    default:
      // Point/nearest filtering
      min_mag = D3DTEXF_POINT;
      mip = D3DTEXF_POINT;
      aniso = 1;
      break;
  }

  device->SetSamplerState(sampler_stage, D3DSAMP_MINFILTER, min_mag);
  device->SetSamplerState(sampler_stage, D3DSAMP_MAGFILTER, min_mag);
  device->SetSamplerState(sampler_stage, D3DSAMP_MIPFILTER, mip);
  device->SetSamplerState(sampler_stage, D3DSAMP_MAXANISOTROPY, aniso);
}

void D3D9RendererImpl::SetDither(bool enable) {
  if (device)
    device->SetRenderState(D3DRS_DITHERENABLE, enable ? TRUE : FALSE);
}

// Apply gamma/contrast/brightness color correction via hardware gamma ramp.
// Uses standard image processing pipeline: gamma → contrast → brightness.
// Input parameters are expected in range [0.1, 0.9] where 0.5 is neutral.
// NOTE: Hardware gamma ramps only work in fullscreen exclusive mode on D3D9.
// In windowed mode, SetGammaRamp calls are ignored by the driver.
void D3D9RendererImpl::SetGammaCorrection(float gamma, float contrast, float brightness) {
  if (!device) {
    return;
  }

  // Hardware gamma ramps don't work in windowed mode - log once and skip
  if (present_params.Windowed) {
    static bool warned = false;
    if (!warned) {
      SPDLOG_WARN("SetGammaCorrection: Hardware gamma ramp not supported in windowed mode");
      warned = false;
    }
    // Still compute and set the ramp - it will take effect if mode changes to fullscreen
  }

  // Clamp input parameters to valid range (0.5 = neutral for all)
  constexpr float kMinInput = 0.1f;
  constexpr float kMaxInput = 0.9f;
  gamma = std::clamp(gamma, kMinInput, kMaxInput);
  contrast = std::clamp(contrast, kMinInput, kMaxInput);
  brightness = std::clamp(brightness, kMinInput, kMaxInput);

  // Convert to standard working values:
  // - gamma_value: 0.1 → 5.0 (darker), 0.5 → 1.0 (neutral), 0.9 → 0.56 (brighter)
  // - contrast_value: 0.1 → 0.2 (low), 0.5 → 1.0 (neutral), 0.9 → 1.8 (high)
  // - brightness_offset: 0.1 → -0.32, 0.5 → 0.0 (neutral), 0.9 → +0.32
  const float gamma_value = 1.0f / (gamma * 2.0f);
  const float contrast_value = contrast * 2.0f;
  const float brightness_offset = (brightness - 0.5f) * 0.8f;

  D3DGAMMARAMP ramp;
  for (int i = 0; i < 256; ++i) {
    float color = i / 255.0f;

    // Step 1: Apply gamma correction (power curve)
    color = std::pow(color, gamma_value);

    // Step 2: Apply contrast (linear scale around midpoint)
    color = (color - 0.5f) * contrast_value + 0.5f;

    // Step 3: Apply brightness (additive offset)
    color += brightness_offset;

    color = std::clamp(color, 0.0f, 1.0f);

    // Convert to 16-bit and apply uniformly to all channels
    const WORD value = static_cast<WORD>(color * 65535.0f);
    ramp.red[i] = value;
    ramp.green[i] = value;
    ramp.blue[i] = value;
  }

  device->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &ramp);
}

void D3D9RendererImpl::SetAlphaRef(unsigned long ref) {
  if (device)
    device->SetRenderState(D3DRS_ALPHAREF, ref);
}

void D3D9RendererImpl::SetColorWrite(DWORD mask) {
  if (device) {
    device->SetRenderState(D3DRS_COLORWRITEENABLE, mask);
  }
}

void D3D9RendererImpl::SetRenderState(unsigned long state, unsigned long value) {
  // Use cached version to avoid redundant D3D9 calls
  SetRenderStateCached(static_cast<D3DRENDERSTATETYPE>(state), value);
}

void D3D9RendererImpl::SetTextureStageState(int stage, unsigned long state, unsigned long value) {
  // Use cached version to avoid redundant D3D9 calls
  SetTextureStageStateCached(static_cast<DWORD>(stage), static_cast<D3DTEXTURESTAGESTATETYPE>(state), value);
}

void D3D9RendererImpl::SetSamplerState(int stage, unsigned long state, unsigned long value) {
  // Use cached version to avoid redundant D3D9 calls
  SetSamplerStateCached(static_cast<DWORD>(stage), static_cast<D3DSAMPLERSTATETYPE>(state), value);
}

void D3D9RendererImpl::SetLight(int index, const D3DLIGHT9* light) {
  if (!device || !light)
    return;
  device->SetLight(index, light);
}

void D3D9RendererImpl::LightEnable(int index, bool enable) {
  if (device)
    device->LightEnable(index, enable);
}

void D3D9RendererImpl::SetMaterial(const D3DMATERIAL9* material) {
  if (!device || !material)
    return;
  device->SetMaterial(material);
}

bool D3D9RendererImpl::DrawVertexBuffer(void* vertex_buffer, unsigned long fvf, unsigned int stride, D3DPRIMITIVETYPE primitive_type,
                                        unsigned int start_vertex, unsigned int vertex_count, const unsigned short* indices,
                                        unsigned int index_count) {
  if (!device || !vertex_buffer || vertex_count == 0 || stride == 0) {
    return false;
  }

  if (primitive_type == D3DPT_FORCE_DWORD)
    return false;

  IDirect3DVertexBuffer9* vb = static_cast<IDirect3DVertexBuffer9*>(vertex_buffer);
  device->SetStreamSource(0, vb, 0, stride);
  device->SetFVF(fvf);

  const bool has_indices = (indices != nullptr) && (index_count > 0);
  const std::uint32_t prim_count = ComputePrimitiveCount(primitive_type, vertex_count, has_indices ? index_count : vertex_count);
  if (prim_count == 0u)
    return false;

  if (has_indices) {
    if (!EnsureIndexBufferCapacity(*this, index_count))
      return false;
    void* dst = nullptr;
    if (FAILED(dynamic_index_buffer->Lock(0, index_count * sizeof(unsigned short), &dst, D3DLOCK_DISCARD)))
      return false;
    std::memcpy(dst, indices, index_count * sizeof(unsigned short));
    dynamic_index_buffer->Unlock();
    device->SetIndices(dynamic_index_buffer);
    device->DrawIndexedPrimitive(primitive_type, start_vertex, 0, vertex_count, 0, prim_count);
    device->SetIndices(nullptr);
  } else {
    device->DrawPrimitive(primitive_type, start_vertex, prim_count);
  }
  return true;
}

size_t D3D9RendererImpl::GetAvailableTextureMem() const {
  return (device) ? static_cast<size_t>(device->GetAvailableTextureMem()) : 0;
}

void D3D9RendererImpl::SetAlphaBlend(bool enable) {
  if (device)
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

void D3D9RendererImpl::SetAlphaBlendFunc(D3DBLEND src, D3DBLEND dest) {
  if (device) {
    device->SetRenderState(D3DRS_SRCBLEND, src);
    device->SetRenderState(D3DRS_DESTBLEND, dest);
  }
}

void D3D9RendererImpl::SetZBuffer(bool enable) {
  if (device)
    device->SetRenderState(D3DRS_ZENABLE, enable);
}

void D3D9RendererImpl::SetZWrite(bool enable) {
  if (device)
    device->SetRenderState(D3DRS_ZWRITEENABLE, enable);
}

void D3D9RendererImpl::SetZFunc(D3DCMPFUNC func) {
  if (device)
    device->SetRenderState(D3DRS_ZFUNC, func);
}

void D3D9RendererImpl::SetCullMode(D3DCULL mode) {
  if (device)
    device->SetRenderState(D3DRS_CULLMODE, mode);
}

void D3D9RendererImpl::SetFog(bool enable) {
  if (device)
    device->SetRenderState(D3DRS_FOGENABLE, enable);
}

void D3D9RendererImpl::SetFogColor(unsigned long color) {
  if (device)
    device->SetRenderState(D3DRS_FOGCOLOR, color);
}

void D3D9RendererImpl::SetFogStart(float start) {
  if (device)
    device->SetRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(start));
}

void D3D9RendererImpl::SetFogEnd(float end) {
  if (device)
    device->SetRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(end));
}

void D3D9RendererImpl::SetFogDensity(float density) {
  if (device)
    device->SetRenderState(D3DRS_FOGDENSITY, std::bit_cast<DWORD>(density));
}

void D3D9RendererImpl::SetFogMode(int mode) {
  if (device) {
    DWORD d3d_mode = D3DFOG_NONE;
    switch (mode) {
      case 0:
      case 3:
        d3d_mode = D3DFOG_LINEAR;
        break;
      case 1:
        d3d_mode = D3DFOG_EXP;
        break;
      case 2:
        d3d_mode = D3DFOG_EXP2;
        break;
    }
    device->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    device->SetRenderState(D3DRS_FOGTABLEMODE, d3d_mode);
  }
}

void D3D9RendererImpl::SetFillMode(D3DFILLMODE mode) {
  if (device)
    device->SetRenderState(D3DRS_FILLMODE, mode);
}

void D3D9RendererImpl::SetZBias(float bias) {
  if (device)
    device->SetRenderState(D3DRS_DEPTHBIAS, std::bit_cast<DWORD>(bias));
}

// ----------------------------------------------------------------------------
// State Caching Implementation
// ----------------------------------------------------------------------------
// These methods check a local cache before calling into D3D9.
// This eliminates redundant API calls which have significant driver overhead.
// Statistics are tracked for performance monitoring.
// ----------------------------------------------------------------------------

bool D3D9RendererImpl::SetRenderStateCached(D3DRENDERSTATETYPE state, DWORD value) {
  if (!device)
    return false;

  const auto idx = static_cast<size_t>(state);
  if (idx >= kMaxRenderStates) {
    // State index out of range - call directly
    device->SetRenderState(state, value);
    ++state_stats_.render_state_changes;
    return true;
  }

  if (render_state_cache_[idx] == value) {
    ++state_stats_.render_state_cached;
    return false;  // Already set to this value
  }

  render_state_cache_[idx] = value;
  device->SetRenderState(state, value);
  ++state_stats_.render_state_changes;
  return true;
}

bool D3D9RendererImpl::SetSamplerStateCached(DWORD stage, D3DSAMPLERSTATETYPE state, DWORD value) {
  if (!device)
    return false;

  const auto stage_idx = static_cast<size_t>(stage);
  const auto state_idx = static_cast<size_t>(state);

  if (stage_idx >= kMaxTextureStages || state_idx >= kMaxSamplerStates) {
    // Out of range - call directly
    device->SetSamplerState(stage, state, value);
    ++state_stats_.sampler_state_changes;
    return true;
  }

  if (sampler_state_cache_[stage_idx][state_idx] == value) {
    ++state_stats_.sampler_state_cached;
    return false;
  }

  sampler_state_cache_[stage_idx][state_idx] = value;
  device->SetSamplerState(stage, state, value);
  ++state_stats_.sampler_state_changes;
  return true;
}

bool D3D9RendererImpl::SetTextureStageStateCached(DWORD stage, D3DTEXTURESTAGESTATETYPE state, DWORD value) {
  if (!device)
    return false;

  const auto stage_idx = static_cast<size_t>(stage);
  const auto state_idx = static_cast<size_t>(state);

  if (stage_idx >= kMaxTextureStages || state_idx >= kMaxTextureStageStates) {
    device->SetTextureStageState(stage, state, value);
    ++state_stats_.texture_stage_changes;
    return true;
  }

  if (texture_stage_cache_[stage_idx][state_idx] == value) {
    ++state_stats_.texture_stage_cached;
    return false;
  }

  texture_stage_cache_[stage_idx][state_idx] = value;
  device->SetTextureStageState(stage, state, value);
  ++state_stats_.texture_stage_changes;
  return true;
}

bool D3D9RendererImpl::SetTextureCached(DWORD stage, IDirect3DBaseTexture9* texture) {
  if (!device)
    return false;

  const auto stage_idx = static_cast<size_t>(stage);
  if (stage_idx >= kMaxTextureStages) {
    device->SetTexture(stage, texture);
    ++state_stats_.texture_changes;
    return true;
  }

  if (texture_cache_[stage_idx] == texture) {
    ++state_stats_.texture_cached;
    return false;
  }

  texture_cache_[stage_idx] = texture;
  device->SetTexture(stage, texture);
  ++state_stats_.texture_changes;
  return true;
}

void D3D9RendererImpl::InvalidateStateCache() {
  // Fill all caches with sentinel value to force re-application
  std::ranges::fill(render_state_cache_, kStateCacheInvalid);

  for (auto& stage : sampler_state_cache_) {
    std::ranges::fill(stage, kStateCacheInvalid);
  }

  for (auto& stage : texture_stage_cache_) {
    std::ranges::fill(stage, kStateCacheInvalid);
  }

  std::ranges::fill(texture_cache_, nullptr);
  std::ranges::fill(bound_textures, nullptr);
}

// ----------------------------------------------------------------------------
// State Block Implementation
// ----------------------------------------------------------------------------
// State blocks capture a set of render states and can apply them atomically.
// This is more efficient than setting individual states when switching between
// common rendering modes (opaque, alpha blend, alpha test).
// ----------------------------------------------------------------------------

void D3D9RendererImpl::CreateStateBlocks() {
  if (!device)
    return;

  // Release any existing state blocks
  ReleaseStateBlocks();

  // --- Opaque State Block ---
  // Z-write enabled, no blending, no alpha test
  device->BeginStateBlock();
  device->SetRenderState(D3DRS_ZENABLE, TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
  device->EndStateBlock(&state_block_opaque_);

  // --- Alpha Blend State Block ---
  // Z-test enabled, z-write disabled, standard alpha blending
  device->BeginStateBlock();
  device->SetRenderState(D3DRS_ZENABLE, TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->EndStateBlock(&state_block_alpha_blend_);

  // --- Alpha Add State Block ---
  // Z-test enabled, z-write disabled, additive blending (for fire, glow effects)
  device->BeginStateBlock();
  device->SetRenderState(D3DRS_ZENABLE, TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->EndStateBlock(&state_block_alpha_add_);

  // --- Alpha Test State Block ---
  // Z-write enabled, alpha test enabled (for vegetation, fences)
  device->BeginStateBlock();
  device->SetRenderState(D3DRS_ZENABLE, TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
  device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
  device->SetRenderState(D3DRS_ALPHAREF, 128);
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->EndStateBlock(&state_block_alpha_test_);
}

void D3D9RendererImpl::ReleaseStateBlocks() {
  if (state_block_opaque_) {
    state_block_opaque_->Release();
    state_block_opaque_ = nullptr;
  }
  if (state_block_alpha_blend_) {
    state_block_alpha_blend_->Release();
    state_block_alpha_blend_ = nullptr;
  }
  if (state_block_alpha_add_) {
    state_block_alpha_add_->Release();
    state_block_alpha_add_ = nullptr;
  }
  if (state_block_alpha_test_) {
    state_block_alpha_test_->Release();
    state_block_alpha_test_ = nullptr;
  }
}

void D3D9RendererImpl::ApplyOpaqueStateBlock() {
  if (state_block_opaque_) {
    state_block_opaque_->Apply();
    // Invalidate cached states that the block modified
    render_state_cache_[D3DRS_ZENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZWRITEENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZFUNC] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHABLENDENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHATESTENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_CULLMODE] = kStateCacheInvalid;
  }
}

void D3D9RendererImpl::ApplyAlphaBlendStateBlock() {
  if (state_block_alpha_blend_) {
    state_block_alpha_blend_->Apply();
    render_state_cache_[D3DRS_ZENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZWRITEENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZFUNC] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHABLENDENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_SRCBLEND] = kStateCacheInvalid;
    render_state_cache_[D3DRS_DESTBLEND] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHATESTENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_CULLMODE] = kStateCacheInvalid;
  }
}

void D3D9RendererImpl::ApplyAlphaAddStateBlock() {
  if (state_block_alpha_add_) {
    state_block_alpha_add_->Apply();
    render_state_cache_[D3DRS_ZENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZWRITEENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZFUNC] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHABLENDENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_SRCBLEND] = kStateCacheInvalid;
    render_state_cache_[D3DRS_DESTBLEND] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHATESTENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_CULLMODE] = kStateCacheInvalid;
  }
}

void D3D9RendererImpl::ApplyAlphaTestStateBlock() {
  if (state_block_alpha_test_) {
    state_block_alpha_test_->Apply();
    render_state_cache_[D3DRS_ZENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZWRITEENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ZFUNC] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHABLENDENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHATESTENABLE] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHAFUNC] = kStateCacheInvalid;
    render_state_cache_[D3DRS_ALPHAREF] = kStateCacheInvalid;
    render_state_cache_[D3DRS_CULLMODE] = kStateCacheInvalid;
  }
}

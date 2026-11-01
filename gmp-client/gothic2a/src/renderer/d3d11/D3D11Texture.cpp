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

#include "D3D11Texture.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

#include "D3D11MapTracker.h"
#include "D3D11Renderer.h"
#include "D3D11RendererImpl.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gothic_II_Addon;

namespace gmp::renderer::d3d11 {

namespace {

void SetDebugObjectName(ID3D11DeviceChild* obj, const std::string& name) {
  if (!obj || name.empty()) {
    return;
  }
  // Same GUID as WKPDID_D3DDebugObjectName, but avoids linking dxguid.lib.
  static const GUID kD3DDebugObjectName = {0x429b8c22, 0x9188, 0x4b0c, {0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00}};
  obj->SetPrivateData(kD3DDebugObjectName, static_cast<UINT>(name.size()), name.c_str());
}

}  // namespace

DXGI_FORMAT GothicFormatToDXGI(zTRnd_TextureFormat format, bool* hasAlpha) {
  if (hasAlpha)
    *hasAlpha = false;

  switch (format) {
    case zRND_TEX_FORMAT_ARGB_4444:
      if (hasAlpha)
        *hasAlpha = true;
      return DXGI_FORMAT_B4G4R4A4_UNORM;

    case zRND_TEX_FORMAT_ARGB_1555:
      if (hasAlpha)
        *hasAlpha = true;
      return DXGI_FORMAT_B5G5R5A1_UNORM;

    case zRND_TEX_FORMAT_RGB_565:
      return DXGI_FORMAT_B5G6R5_UNORM;

    case zRND_TEX_FORMAT_ARGB_8888:
      if (hasAlpha)
        *hasAlpha = true;
      return DXGI_FORMAT_B8G8R8A8_UNORM;

    case zRND_TEX_FORMAT_DXT1:
      return DXGI_FORMAT_BC1_UNORM;

    case zRND_TEX_FORMAT_DXT2:
    case zRND_TEX_FORMAT_DXT3:
      if (hasAlpha)
        *hasAlpha = true;
      return DXGI_FORMAT_BC2_UNORM;

    case zRND_TEX_FORMAT_DXT4:
    case zRND_TEX_FORMAT_DXT5:
      if (hasAlpha)
        *hasAlpha = true;
      return DXGI_FORMAT_BC3_UNORM;

    case zRND_TEX_FORMAT_PAL_8:
      // Palettized textures need to be converted to RGBA
      return DXGI_FORMAT_B8G8R8A8_UNORM;

    default:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
  }
}

uint32_t GetBytesPerPixel(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      return 4;
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
      return 2;
    default:
      return 4;
  }
}

bool IsCompressedFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return true;
    default:
      return false;
  }
}

uint32_t GetBlockSize(DXGI_FORMAT format) {
  return IsCompressedFormat(format) ? 4 : 1;
}

uint32_t GetBytesPerBlock(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
      return 8;  // 8 bytes per 4x4 block
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      return 16;  // 16 bytes per 4x4 block
    default:
      return 0;
  }
}

}  // namespace gmp::renderer::d3d11

using namespace gmp::renderer::d3d11;

//------------------------------------------------------------------------------
// zCTex_D3D11 Implementation
//------------------------------------------------------------------------------

std::string zCTex_D3D11::DebugName() const {
  const zSTRING name = GetObjectName();
  const char* raw = name.ToChar();
  if (raw == nullptr || *raw == '\0') {
    return "<unnamed>";
  }
  return std::string(raw);
}

zCTex_D3D11::zCTex_D3D11() = default;

zCTex_D3D11::~zCTex_D3D11() {
  ReleaseResourceData();
}

int zCTex_D3D11::ReleaseResourceData() {
  UnlockAllMipLevels();
  srv_.Reset();
  texture_.Reset();
  stagingTexture_.Reset();
  gpu_content_valid_ = false;  // GPU texture released - no valid content
  cacheState = zRES_CACHED_OUT;
  return 1;
}

int zCTex_D3D11::Lock(int /*mode*/) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);

  if (cacheState == zRES_CACHED_OUT && !is_loading_) {
    CacheIn(-1);
  }
  is_locked_ = true;
  locked_mip_mask_.reset();
  std::fill(locked_rects_.begin(), locked_rects_.end(), D3D11LockedRect{});
  std::fill(exposed_pitches_.begin(), exposed_pitches_.end(), 0);
  return 1;
}

int zCTex_D3D11::Unlock() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);

  // Save which mips were actually written before UnlockAllMipLevels clears the mask.
  // We need this to know which mips to copy to GPU.
  std::bitset<kMaxTrackedMips> mips_written = locked_mip_mask_;

  // Debug: Log which mips were written
  if (mips_written.none()) {
    static int warned_no_mips = 0;
    if (warned_no_mips < 20) {
      ++warned_no_mips;
      SPDLOG_WARN("Unlock [{}]: No mips were written during this Lock/Unlock cycle! staging_dirty_={}", DebugName(), staging_dirty_);
    }
  }

  UnlockAllMipLevels();

  // Copy from staging to GPU texture if dirty
  if (staging_dirty_ && stagingTexture_ && texture_) {
    if (g_D3D11Context) {
      CopyFromStagingToGPU(g_D3D11Context, mips_written);
    }
    staging_dirty_ = false;
  }

  is_locked_ = false;
  cacheState = zRES_CACHED_IN;
  return 1;
}

int zCTex_D3D11::SetTextureInfo(const zCTextureInfo& texInfo) {
  if (texInfo.sizeX == 0 || texInfo.sizeY == 0) {
    SPDLOG_WARN("SetTextureInfo [{}]: texture has no size, aborting", DebugName());
    return 0;
  }
  tex_info_ = texInfo;
  dxgiFormat_ = GothicFormatToDXGI(texInfo.texFormat, &has_alpha_);

  // Format-based smooth alpha hint (may be refined later by scanning mip0).
  // DXT5 and 32-bit ARGB frequently contain fractional alpha; DXT1/1555 are typically binary.
  // NOTE: DXT3/BC2 has 4-bit explicit alpha (16 discrete levels), which is NOT smooth alpha.
  // Treating DXT3 as smooth alpha causes incorrect TEST->BLEND_TEST upgrades for punch-through
  // cutout textures (fishnets, leaves), making them appear too opaque/dark.
  smooth_alpha_hint_ = false;
  if (has_alpha_) {
    switch (texInfo.texFormat) {
      case zRND_TEX_FORMAT_DXT5:
      case zRND_TEX_FORMAT_ARGB_4444:
      case zRND_TEX_FORMAT_ARGB_8888:
        smooth_alpha_hint_ = true;
        break;
      default:
        smooth_alpha_hint_ = false;
        break;
    }
  }
  smooth_alpha_known_ = false;
  smooth_alpha_detected_ = false;
  return 1;
}

bool zCTex_D3D11::HasSmoothAlpha() const {
  if (!has_alpha_) {
    return false;
  }
  if (smooth_alpha_known_) {
    return smooth_alpha_detected_;
  }
  return smooth_alpha_hint_;
}

void* zCTex_D3D11::GetPaletteBuffer() {
  // D3D11 doesn't use palette buffers
  return nullptr;
}

int zCTex_D3D11::GetTextureBuffer(int mipMapNr, void*& buffer, int& pitchXBytes) {
  buffer = nullptr;
  pitchXBytes = 0;

  // ZenGin is *supposed* to call Lock() before GetTextureBuffer(), but we have evidence
  // that some code paths can call GetTextureBuffer() directly.
  // If we refuse here, the engine's LoadResourceData() fails and we end up with missing
  // textures/lightmaps (manifesting as intermittent black surfaces).
  // Be robust: auto-enter a locked state on first use.
  if (!is_locked_) {
    static int warned_not_locked = 0;
    if (warned_not_locked < 20) {
      ++warned_not_locked;
#ifdef _WIN32
      const DWORD tid = GetCurrentThreadId();
#else
      const unsigned long tid = 0;
#endif
      SPDLOG_WARN("GetTextureBuffer [{}]: called while not locked (auto-locking). tid={} cache={} loading={} mip={}", DebugName(),
                  static_cast<unsigned long>(tid), static_cast<int>(cacheState), is_loading_ ? 1 : 0, mipMapNr);
    }
    is_locked_ = true;
    locked_mip_mask_.reset();
    std::fill(locked_rects_.begin(), locked_rects_.end(), D3D11LockedRect{});
    std::fill(exposed_pitches_.begin(), exposed_pitches_.end(), 0);
    for (auto& buf : linear_buffers_) {
      buf.clear();
    }
  }

  if (mipMapNr < 0 || mipMapNr >= kMaxTrackedMips) {
    SPDLOG_ERROR("GetTextureBuffer [{}]: mip {} outside tracked range", DebugName(), mipMapNr);
    return 0;
  }

  // NOTE: For BC/DXT formats, mip levels smaller than 4x4 are still valid.
  // They are stored as a single 4x4 block. Do NOT reject them here.

  // If already locked, return cached values
  if (locked_mip_mask_.test(mipMapNr)) {
    // If we're using a linear buffer, return that; otherwise return staging pointer
    if (!linear_buffers_[mipMapNr].empty()) {
      buffer = linear_buffers_[mipMapNr].data();
    } else {
      buffer = locked_rects_[mipMapNr].pData;
    }
    pitchXBytes = exposed_pitches_[mipMapNr];
    return (buffer != nullptr && pitchXBytes != 0) ? 1 : 0;
  }

  // Get device
  if (!g_D3D11Device || !g_D3D11Context) {
    SPDLOG_CRITICAL("GetTextureBuffer [{}]: Device or context is null", DebugName());
    return 0;
  }

  // Create textures if they don't exist
  if (!texture_ || !stagingTexture_) {
    // Create GPU texture (DEFAULT usage)
    D3D11_TEXTURE2D_DESC gpuDesc = {};
    gpuDesc.Width = tex_info_.sizeX;
    gpuDesc.Height = tex_info_.sizeY;
    gpuDesc.MipLevels = tex_info_.numMipMap;
    gpuDesc.ArraySize = 1;
    gpuDesc.Format = dxgiFormat_;
    gpuDesc.SampleDesc.Count = 1;
    gpuDesc.SampleDesc.Quality = 0;
    gpuDesc.Usage = D3D11_USAGE_DEFAULT;
    gpuDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    gpuDesc.CPUAccessFlags = 0;
    gpuDesc.MiscFlags = 0;

    HRESULT hr = g_D3D11Device->CreateTexture2D(&gpuDesc, nullptr, texture_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
      SPDLOG_ERROR("GetTextureBuffer [{}]: Failed to create GPU texture: 0x{:08X}", DebugName(), static_cast<uint32_t>(hr));
      return 0;
    }

    // Name early, even if SRV isn't created yet.
    {
      char tag[512];
      std::snprintf(tag, sizeof(tag), "tex:%s@%p", DebugName().c_str(), static_cast<void*>(this));
      SetDebugObjectName(texture_.Get(), tag);
    }

    // Create staging texture for CPU access
    if (!CreateStagingTexture(g_D3D11Device)) {
      texture_.Reset();
      return 0;
    }

    {
      char tag[512];
      std::snprintf(tag, sizeof(tag), "staging:%s@%p", DebugName().c_str(), static_cast<void*>(this));
      SetDebugObjectName(stagingTexture_.Get(), tag);
    }
  }

  // Map the staging texture
  D3D11_MAPPED_SUBRESOURCE mapped = {};
  UINT subresource = D3D11CalcSubresource(mipMapNr, 0, tex_info_.numMipMap);

  HRESULT hr = TrackedMap(g_D3D11Context, static_cast<ID3D11Resource*>(stagingTexture_.Get()), subresource, D3D11_MAP_WRITE, 0, &mapped,
                          "zCTex_D3D11::GetTextureBuffer");
  if (FAILED(hr)) {
    SPDLOG_ERROR("GetTextureBuffer [{}]: Failed to map staging texture mip {}: 0x{:08X}", DebugName(), mipMapNr, static_cast<uint32_t>(hr));
    return 0;
  }

  locked_rects_[mipMapNr].pData = mapped.pData;
  locked_rects_[mipMapNr].RowPitch = mapped.RowPitch;
  locked_mip_mask_.set(mipMapNr);
  staging_dirty_ = true;

  // Calculate the pitch Gothic expects.
  // Align behavior with the D3D9 implementation:
  //   - DXT1: fake pitch = width/2 (clamped to >= 1)
  //   - DXT3: fake pitch = width (clamped to >= 1)
  //   - DXT5: use the actual pitch (historically matches Gothic's writer behavior)
  // For other formats, use the actual mapped pitch.
  const int mip_width = std::max(1, tex_info_.sizeX >> mipMapNr);
  int gothic_pitch = 0;
  switch (tex_info_.texFormat) {
    case zRND_TEX_FORMAT_DXT1: {
      gothic_pitch = std::max(1, mip_width >> 1);
      break;
    }
    case zRND_TEX_FORMAT_DXT3: {
      gothic_pitch = std::max(1, mip_width);
      break;
    }
    case zRND_TEX_FORMAT_DXT5: {
      gothic_pitch = static_cast<int>(mapped.RowPitch);
      break;
    }
    default: {
      gothic_pitch = static_cast<int>(mapped.RowPitch);
      break;
    }
  }

  // For compressed formats where D3D11 pitch differs from Gothic's expected pitch,
  // use a linear buffer that Gothic writes to. We'll copy to staging in Unlock().
  if (IsCompressedFormat(dxgiFormat_) && static_cast<int>(mapped.RowPitch) != gothic_pitch) {
    const int mip_width = std::max(1, tex_info_.sizeX >> mipMapNr);
    const int mip_height = std::max(1, tex_info_.sizeY >> mipMapNr);
    const int block_rows = (mip_height + 3) / 4;
    const int bytes_per_block_row = ((mip_width + 3) / 4) * GetBytesPerBlock(dxgiFormat_);
    const size_t total_size = static_cast<size_t>(block_rows) * bytes_per_block_row;

    linear_buffers_[mipMapNr].resize(total_size);
    buffer = linear_buffers_[mipMapNr].data();
    // Return Gothic's expected pitch, not the actual byte pitch
    pitchXBytes = gothic_pitch;
  } else {
    buffer = mapped.pData;
    pitchXBytes = gothic_pitch;
  }

  exposed_pitches_[mipMapNr] = pitchXBytes;
  return 1;
}

zCTextureInfo zCTex_D3D11::GetTextureInfo() {
  if (cacheState == zRES_CACHED_OUT) {
    CacheIn(-1);
  }
  return tex_info_;
}

int zCTex_D3D11::CopyPaletteDataTo(void* /*destBuffer*/) {
  return 0;
}

int zCTex_D3D11::CopyTextureDataTo(int mipMapNr, void* destBuffer, int destPitchXBytes) {
  if (destBuffer == nullptr || !is_locked_ || !stagingTexture_) {
    return 0;
  }

  UINT subresource = D3D11CalcSubresource(mipMapNr, 0, tex_info_.numMipMap);
  D3D11_MAPPED_SUBRESOURCE mapped = {};

  HRESULT hr = TrackedMap(g_D3D11Context, static_cast<ID3D11Resource*>(stagingTexture_.Get()), subresource, D3D11_MAP_READ, 0, &mapped,
                          "zCTex_D3D11::CopyTextureDataTo");
  if (FAILED(hr)) {
    SPDLOG_ERROR("CopyTextureDataTo [{}]: Failed to map mip {}: 0x{:08X}", DebugName(), mipMapNr, static_cast<uint32_t>(hr));
    return 0;
  }

  const int mip_width = std::max(1, tex_info_.sizeX >> mipMapNr);
  const int mip_height = std::max(1, tex_info_.sizeY >> mipMapNr);

  auto* src = static_cast<const uint8_t*>(mapped.pData);
  auto* dest = static_cast<uint8_t*>(destBuffer);

  if (IsCompressedFormat(dxgiFormat_)) {
    // For compressed formats, copy entire linear buffer
    const int block_height = (mip_height + 3) / 4;
    std::memcpy(dest, src, block_height * mapped.RowPitch);
  } else {
    // Copy row by row
    const uint32_t bpp = GetBytesPerPixel(dxgiFormat_);
    const uint32_t rowBytes = mip_width * bpp;
    for (int h = 0; h < mip_height; ++h) {
      std::memcpy(dest + h * destPitchXBytes, src + h * mapped.RowPitch, rowBytes);
    }
  }

  TrackedUnmap(g_D3D11Context, static_cast<ID3D11Resource*>(stagingTexture_.Get()), subresource, "zCTex_D3D11::CopyTextureDataTo");
  return 1;
}

int zCTex_D3D11::HasAlpha() {
  return has_alpha_ ? 1 : 0;
}

void zCTex_D3D11::ReleaseData() {
  ReleaseResourceData();
}

int zCTex_D3D11::LoadResourceData() {
  is_loading_ = true;
  const int result = zCTexture::LoadResourceData();
  is_loading_ = false;

  if (result != 0) {
    cacheState = zRES_CACHED_IN;
  } else {
    SPDLOG_WARN("LoadResourceData FAILED [{}]", DebugName());
  }
  return result;
}

bool zCTex_D3D11::EnsureSRV(ID3D11Device* device) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);

  // CRITICAL: If the texture is currently locked (being written to by Gothic's loader),
  // we must NOT try to use it - this is the core race condition we're preventing.
  // The texture loading sequence is: Lock() → GetTextureBuffer() → [write data] → Unlock()
  // If we try to render during this window, we get black/corrupt textures.
  if (is_locked_) {
    static bool warned_texture_locked = false;
    if (!warned_texture_locked) {
      warned_texture_locked = true;
      SPDLOG_WARN("EnsureSRV [{}]: Texture is currently locked (being loaded); cannot use for rendering yet.", DebugName());
    }
    return false;
  }

  // Check if SRV already exists AND the content is valid.
  // Note: srv_ can exist from a previous successful load, but if content was invalidated
  // (e.g., texture re-loaded), we need gpu_content_valid_ to also be true.
  if (srv_ && gpu_content_valid_) {
    return true;
  }

  if (!device) {
    SPDLOG_WARN("EnsureSRV [{}]: Device is null", DebugName());
    return false;
  }

  if (!texture_) {
    // Texture doesn't exist - try to force cache-in which triggers LoadResourceData.
    // This can happen for textures that are referenced before being properly loaded.
    if (cacheState == zRES_CACHED_OUT) {
      CacheIn(-1);  // Force load
    }

    // Some code paths can end up with cacheState not indicating cached-out while the GPU texture
    // is still missing. Try forcing the normal resource load path.
    if (!texture_ && !is_loading_) {
      LoadResourceData();
    }

    // If still no texture after cache-in/load attempts, do NOT create an "empty" GPU texture.
    // Empty resources typically sample as black and can fully black-out surfaces (especially
    // when used as the BASE in the lightmap shader).
    // Returning false lets the renderer fall back to its safe placeholder SRV.
    if (!texture_) {
      static bool warned_missing_gpu_texture = false;
      if (!warned_missing_gpu_texture) {
        warned_missing_gpu_texture = true;
        SPDLOG_WARN("EnsureSRV [{}]: No GPU texture available after load attempts; using renderer fallback SRV.", DebugName());
      }
      return false;
    }
  }

  // CRITICAL: The GPU texture object may exist but not have valid content yet.
  // This happens when GetTextureBuffer() creates the empty GPU texture, but
  // Unlock() -> CopyFromStagingToGPU() hasn't been called yet to upload the data.
  // Binding such a texture would sample zeros (black), causing the black texture bug.
  if (!gpu_content_valid_) {
    static bool warned_content_not_ready = false;
    if (!warned_content_not_ready) {
      warned_content_not_ready = true;
      SPDLOG_WARN(
          "EnsureSRV [{}]: GPU texture exists but content not uploaded yet (staging->GPU copy pending); "
          "using renderer fallback SRV.",
          DebugName());
    }
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = dxgiFormat_;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.MipLevels = std::max(1, tex_info_.numMipMap);

  HRESULT hr = device->CreateShaderResourceView(texture_.Get(), &srvDesc, srv_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("EnsureSRV [{}]: Failed to create SRV: 0x{:08X}", DebugName(), static_cast<uint32_t>(hr));
    return false;
  }

  // Helpful for RenderDoc: label both the resource and SRV with the Gothic texture name.
  // This makes it obvious if tex0/tex1 bindings are swapped (diffuse vs lightmap).
  const std::string base = DebugName();
  {
    char tex_tag[512];
    std::snprintf(tex_tag, sizeof(tex_tag), "tex:%s@%p", base.c_str(), static_cast<void*>(this));
    SetDebugObjectName(texture_.Get(), tex_tag);
  }
  {
    char srv_tag[512];
    std::snprintf(srv_tag, sizeof(srv_tag), "srv:%s@%p", base.c_str(), static_cast<void*>(this));
    SetDebugObjectName(srv_.Get(), srv_tag);
  }
  return true;
}

bool zCTex_D3D11::CreateStagingTexture(ID3D11Device* device) {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = tex_info_.sizeX;
  desc.Height = tex_info_.sizeY;
  desc.MipLevels = tex_info_.numMipMap;
  desc.ArraySize = 1;
  desc.Format = dxgiFormat_;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;

  HRESULT hr = device->CreateTexture2D(&desc, nullptr, stagingTexture_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("CreateStagingTexture [{}]: Failed: 0x{:08X}", DebugName(), static_cast<uint32_t>(hr));
    return false;
  }

  return true;
}

void zCTex_D3D11::CopyFromStagingToGPU(ID3D11DeviceContext* context, const std::bitset<kMaxTrackedMips>& mipsToCopy) {
  if (!stagingTexture_ || !texture_ || !context) {
    SPDLOG_WARN("CopyFromStagingToGPU [{}]: Missing staging={} texture={} context={}", DebugName(), (bool)stagingTexture_, (bool)texture_,
                (bool)context);
    return;
  }

  // Only copy mip levels that were actually written during the Lock/Unlock cycle.
  // The staging texture memory for untouched mips is UNINITIALIZED (zeros/garbage),
  // so copying the entire resource would overwrite valid GPU mips with black data.
  // This fixes the "black far-away geometry" bug where mips 1+ appear black because
  // Gothic only wrote mip0 in one Lock/Unlock pass.
  bool any_copied = false;
  int mips_copied_count = 0;
  for (int mip = 0; mip < kMaxTrackedMips && mip < tex_info_.numMipMap; ++mip) {
    if (mipsToCopy.test(mip)) {
      UINT subresource = D3D11CalcSubresource(mip, 0, tex_info_.numMipMap);
      context->CopySubresourceRegion(texture_.Get(), subresource, 0, 0, 0, stagingTexture_.Get(), subresource, nullptr);
      any_copied = true;
      ++mips_copied_count;
    }
  }

  // Mark GPU content as valid only if we copied at least mip0 (the base level).
  // If we only copied higher mips (rare), the texture may still be incomplete.
  // NOTE: CopySubresourceRegion is asynchronous on the GPU, but D3D11 guarantees
  // command serialization on the same context. Any subsequent draw calls using this
  // texture will execute AFTER the copy completes.
  if (any_copied && mipsToCopy.test(0)) {
    gpu_content_valid_ = true;
  } else if (any_copied && !mipsToCopy.test(0)) {
    // Copied some mips but NOT mip0 - this is unusual and might indicate a problem
    static int warned_no_mip0 = 0;
    if (warned_no_mip0 < 10) {
      ++warned_no_mip0;
      SPDLOG_WARN("CopyFromStagingToGPU [{}]: Copied {} mips but NOT mip0! gpu_content_valid_ stays {}", DebugName(), mips_copied_count,
                  gpu_content_valid_);
    }
  } else if (!any_copied) {
    static int warned_nothing_copied = 0;
    if (warned_nothing_copied < 10) {
      ++warned_nothing_copied;
      SPDLOG_WARN("CopyFromStagingToGPU [{}]: Nothing was copied! mipsToCopy is empty", DebugName());
    }
  }
}

void zCTex_D3D11::CopyLinearBufferToStaging(int mipMapNr) {
  if (linear_buffers_[mipMapNr].empty()) {
    return;  // No linear buffer used for this mip
  }

  if (!locked_rects_[mipMapNr].pData) {
    return;  // Staging not mapped
  }

  const int mip_width = std::max(1, tex_info_.sizeX >> mipMapNr);
  const int mip_height = std::max(1, tex_info_.sizeY >> mipMapNr);
  const int block_cols = (mip_width + 3) / 4;
  const int block_rows = (mip_height + 3) / 4;
  const int bytes_per_block = GetBytesPerBlock(dxgiFormat_);
  const int src_row_bytes = block_cols * bytes_per_block;  // Actual bytes per block row
  const int dst_pitch = locked_rects_[mipMapNr].RowPitch;

  const uint8_t* src = linear_buffers_[mipMapNr].data();
  uint8_t* dst = static_cast<uint8_t*>(locked_rects_[mipMapNr].pData);

  // Gothic writes compressed data linearly - copy block row by block row
  // The linear buffer contains the compressed blocks laid out sequentially
  for (int row = 0; row < block_rows; ++row) {
    std::memcpy(dst + row * dst_pitch, src + row * src_row_bytes, src_row_bytes);
  }

  // Clear the linear buffer but DON'T shrink_to_fit() here.
  // shrink_to_fit() causes heap deallocation which can deadlock with D3D11's
  // multithread protection lock (see ntdll_RtlEnterCriticalSection in stack traces).
  // The memory will be reused on next texture load or freed when the texture is destroyed.
  linear_buffers_[mipMapNr].clear();
}

void zCTex_D3D11::UnlockAllMipLevels() {
  if (!stagingTexture_ || !g_D3D11Context) {
    locked_mip_mask_.reset();
    std::fill(locked_rects_.begin(), locked_rects_.end(), D3D11LockedRect{});
    std::fill(exposed_pitches_.begin(), exposed_pitches_.end(), 0);
    for (auto& buf : linear_buffers_) {
      buf.clear();
    }
    return;
  }

  auto detect_smooth_alpha_from_mip0 = [&]() {
    if (smooth_alpha_known_ || !has_alpha_) {
      return;
    }
    if (!locked_mip_mask_.test(0)) {
      return;
    }
    if (!locked_rects_[0].pData) {
      return;
    }

    // Only scan common 32-bit uncompressed formats (fast path).
    if (dxgiFormat_ != DXGI_FORMAT_B8G8R8A8_UNORM && dxgiFormat_ != DXGI_FORMAT_R8G8B8A8_UNORM) {
      return;
    }

    const int width = std::max(1, tex_info_.sizeX);
    const int height = std::max(1, tex_info_.sizeY);
    const int row_pitch = static_cast<int>(locked_rects_[0].RowPitch);
    const auto* data = static_cast<const uint8_t*>(locked_rects_[0].pData);
    if (!data || row_pitch <= 0) {
      return;
    }

    // If we see any intermediate alpha value, treat this as smooth alpha.
    // Keep a small deadzone to ignore potential padding/quantization noise.
    bool has_intermediate = false;
    for (int y = 0; y < height && !has_intermediate; ++y) {
      const uint8_t* row = data + y * row_pitch;
      for (int x = 0; x < width; ++x) {
        const uint8_t a = row[x * 4 + 3];
        if (a > 8 && a < 247) {
          has_intermediate = true;
          break;
        }
      }
    }

    smooth_alpha_detected_ = has_intermediate;
    smooth_alpha_known_ = true;
  };

  // Try to learn smooth-alpha from mip0 before unmapping it.
  detect_smooth_alpha_from_mip0();

  for (int mip = 0; mip < kMaxTrackedMips; ++mip) {
    if (locked_mip_mask_.test(mip)) {
      // Copy from linear buffer to staging if used
      CopyLinearBufferToStaging(mip);

      UINT subresource = D3D11CalcSubresource(mip, 0, tex_info_.numMipMap);
      TrackedUnmap(g_D3D11Context, static_cast<ID3D11Resource*>(stagingTexture_.Get()), subresource, "zCTex_D3D11::UnlockAllMipLevels");
      locked_mip_mask_.reset(mip);
      locked_rects_[mip] = {};
      exposed_pitches_[mip] = 0;
    }
  }
}

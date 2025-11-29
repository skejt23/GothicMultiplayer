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

#include "D3D9Texture.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <ranges>
#include <span>

#include "D3D9Renderer.h"

using namespace Gothic_II_Addon;

namespace {

constexpr int kD3DPoolManaged = 1;

// Helper to check if a texture format is DXT compressed.
constexpr bool IsDxtFormat(zTRnd_TextureFormat format) {
  return format == zRND_TEX_FORMAT_DXT1 || format == zRND_TEX_FORMAT_DXT2 || format == zRND_TEX_FORMAT_DXT3 || format == zRND_TEX_FORMAT_DXT4 ||
         format == zRND_TEX_FORMAT_DXT5;
}

// Minimum dimension for DXT block compression.
constexpr int kDxtBlockSize = 4;

}  // namespace

auto CreateTexture_D3D9(IDirect3DDevice9* device, int width, int height, int levels, uint32_t usage, D3D9Format format, int pool)
    -> IDirect3DTexture9* {
  IDirect3DTexture9* texture = nullptr;
  const auto d3d_format = static_cast<D3DFORMAT>(format);
  const HRESULT hr = device->CreateTexture(width, height, levels, usage, d3d_format, static_cast<D3DPOOL>(pool), &texture, nullptr);
  return SUCCEEDED(hr) ? texture : nullptr;
}

void ReleaseTexture_D3D9(IDirect3DTexture9* texture) {
  if (texture != nullptr) {
    texture->Release();
  }
}

auto LockTexture_D3D9(IDirect3DTexture9* texture, int level, D3D9LockedRect* rect_out) -> bool {
  if (texture == nullptr || rect_out == nullptr) {
    return false;
  }

  D3DLOCKED_RECT locked_rect{};
  const HRESULT hr = texture->LockRect(level, &locked_rect, nullptr, 0);

  if (FAILED(hr)) {
    SPDLOG_ERROR("LockTexture_D3D9: Failed to lock level {} hr={:#x}", level, static_cast<unsigned>(hr));
    return false;
  }

  rect_out->Pitch = locked_rect.Pitch;
  rect_out->pBits = locked_rect.pBits;
  return true;
}

auto UnlockTexture_D3D9(IDirect3DTexture9* texture, int level) -> bool {
  if (texture == nullptr) {
    return false;
  }
  const HRESULT hr = texture->UnlockRect(level);
  if (FAILED(hr)) {
    SPDLOG_ERROR("UnlockTexture_D3D9: Failed to unlock mip {} hr=0x{:x}", level, static_cast<unsigned>(hr));
  }
  return SUCCEEDED(hr);
}

auto zCTex_D3D9::DebugName() const -> std::string {
  const zSTRING name = GetObjectName();
  const char* raw = name.ToChar();
  if (raw == nullptr || *raw == '\0') {
    return "<unnamed>";
  }
  return std::string(raw);
}

zCTex_D3D9::zCTex_D3D9() = default;

zCTex_D3D9::~zCTex_D3D9() {
  ReleaseResourceData();
}

auto zCTex_D3D9::ReleaseResourceData() -> int {
  UnlockAllMipLevels();
  if (texture_ != nullptr) {
    ReleaseTexture_D3D9(texture_);
    texture_ = nullptr;
  }
  cacheState = zRES_CACHED_OUT;
  return 1;
}

auto zCTex_D3D9::Lock(int /*mode*/) -> int {
  if (cacheState == zRES_CACHED_OUT && !is_loading_) {
    CacheIn(-1);
  }
  is_locked_ = true;
  locked_mip_mask_.reset();
  std::ranges::fill(locked_rects_, D3D9LockedRect{});
  std::ranges::fill(exposed_pitches_, 0);
  return 1;
}

auto zCTex_D3D9::Unlock() -> int {
  UnlockAllMipLevels();
  is_locked_ = false;
  cacheState = zRES_CACHED_IN;
  return 1;
}

auto zCTex_D3D9::SetTextureInfo(const zCTextureInfo& texInfo) -> int {
  // Validate texture dimensions.
  if (texInfo.sizeX == 0 || texInfo.sizeY == 0) {
    SPDLOG_WARN("SetTextureInfo [{}]: texture has no size, aborting", DebugName());
    return 0;
  }

  tex_info_ = texInfo;
  return 1;
}

auto zCTex_D3D9::GetPaletteBuffer() -> void* {
  // D3D9 doesn't use palette buffers - texture binding handled by renderer.
  return nullptr;
}

auto zCTex_D3D9::GetTextureBuffer(int mipMapNr, void*& buffer, int& pitchXBytes) -> int {
  // This function returns a pointer to the texture buffer and the pitch.
  // For DXT formats, it returns a FAKE pitch that Gothic expects, not the real
  // D3D9 pitch.

  buffer = nullptr;
  pitchXBytes = 0;

  if (!is_locked_) {
    SPDLOG_WARN("GetTextureBuffer [{}]: Texture is not locked!", DebugName());
    return 0;
  }

  // For DXT1/DXT3, width and height must be at least 4 pixels.
  if (tex_info_.texFormat == zRND_TEX_FORMAT_DXT1 || tex_info_.texFormat == zRND_TEX_FORMAT_DXT3) {
    const int mip_width = tex_info_.sizeX >> mipMapNr;
    const int mip_height = tex_info_.sizeY >> mipMapNr;
    if (mip_width < kDxtBlockSize || mip_height < kDxtBlockSize) {
      SPDLOG_WARN("GetTextureBuffer [{}]: DXT mipmap {} has dimensions {}x{} (< 4x4)", DebugName(), mipMapNr, mip_width, mip_height);
      return 0;
    }
  }

  if (mipMapNr < 0 || mipMapNr >= kMaxTrackedMips) {
    SPDLOG_ERROR("GetTextureBuffer [{}]: mip {} outside tracked range", DebugName(), mipMapNr);
    return 0;
  }

  // If already locked, return cached values.
  if (locked_mip_mask_.test(mipMapNr)) {
    buffer = locked_rects_[mipMapNr].pBits;
    pitchXBytes = exposed_pitches_[mipMapNr];
    return (buffer != nullptr && pitchXBytes != 0) ? 1 : 0;
  }

  // Create texture if it doesn't exist.
  if (texture_ == nullptr) {
    // Always get the current device when creating a texture
    // (device_ may be stale after device recreation).
    IDirect3DDevice9* device = nullptr;
    if (auto* rnd = static_cast<zCRnd_D3D_DX9*>(zrenderer); rnd != nullptr) {
      device = rnd->GetDevice();
    }
    if (device == nullptr) {
      SPDLOG_CRITICAL("GetTextureBuffer [{}]: Device is null", DebugName());
      return 0;
    }

    // Determine D3D format.
    D3D9Format d3d_format = D3D9Format::R5G6B5;
    switch (tex_info_.texFormat) {
      case zRND_TEX_FORMAT_ARGB_4444:
        d3d_format = D3D9Format::A4R4G4B4;
        has_alpha_ = true;
        break;
      case zRND_TEX_FORMAT_ARGB_1555:
        d3d_format = D3D9Format::A1R5G5B5;
        has_alpha_ = true;
        break;
      case zRND_TEX_FORMAT_DXT1:
        d3d_format = D3D9Format::DXT1;
        break;
      case zRND_TEX_FORMAT_DXT3:
        d3d_format = D3D9Format::DXT3;
        has_alpha_ = true;
        break;
      case zRND_TEX_FORMAT_DXT5:
        d3d_format = D3D9Format::DXT5;
        has_alpha_ = true;
        break;
      case zRND_TEX_FORMAT_ARGB_8888:
        d3d_format = D3D9Format::A8R8G8B8;
        has_alpha_ = true;
        break;
      case zRND_TEX_FORMAT_RGB_565:
      default:
        d3d_format = D3D9Format::R5G6B5;
        break;
    }

    texture_ = CreateTexture_D3D9(device, tex_info_.sizeX, tex_info_.sizeY, tex_info_.numMipMap, 0, d3d_format, kD3DPoolManaged);
    if (texture_ == nullptr) {
      SPDLOG_ERROR("GetTextureBuffer [{}]: Failed to create texture {}x{} fmt:{}", DebugName(), tex_info_.sizeX, tex_info_.sizeY,
                   static_cast<int>(d3d_format));
      return 0;
    }
  }

  // Lock the texture.
  D3D9LockedRect locked_rect{};
  if (!LockTexture_D3D9(texture_, mipMapNr, &locked_rect)) {
    SPDLOG_ERROR("GetTextureBuffer [{}]: Failed to lock mip {}", DebugName(), mipMapNr);
    return 0;
  }

  locked_rects_[mipMapNr] = locked_rect;
  locked_mip_mask_.set(mipMapNr);

  // Return the actual D3D9 surface pointer.
  buffer = locked_rect.pBits;

  // For DXT formats, return FAKE pitch that Gothic expects.
  // Handle DXT1 and DXT3, everything else (including DXT5) uses actual pitch.
  switch (tex_info_.texFormat) {
    case zRND_TEX_FORMAT_DXT1:
      pitchXBytes = tex_info_.sizeX >> (mipMapNr + 1);
      break;
    case zRND_TEX_FORMAT_DXT3:
      pitchXBytes = tex_info_.sizeX >> mipMapNr;
      break;
    default:
      pitchXBytes = locked_rect.Pitch;
      break;
  }

  exposed_pitches_[mipMapNr] = pitchXBytes;
  return 1;
}

auto zCTex_D3D9::GetTextureInfo() -> zCTextureInfo {
  if (cacheState == zRES_CACHED_OUT) {
    CacheIn(-1);
  }
  return tex_info_;
}

auto zCTex_D3D9::CopyPaletteDataTo(void* /*destBuffer*/) -> int {
  return 0;
}

auto zCTex_D3D9::CopyTextureDataTo(int mipMapNr, void* destBuffer, int destPitchXBytes) -> int {
  if (destBuffer == nullptr || !is_locked_ || texture_ == nullptr) {
    return 0;
  }

  D3D9LockedRect locked_rect{};
  if (!LockTexture_D3D9(texture_, mipMapNr, &locked_rect)) {
    SPDLOG_ERROR("CopyTextureDataTo [{}]: Failed to lock mip {}", DebugName(), mipMapNr);
    return 0;
  }

  const auto* src = static_cast<std::byte*>(locked_rect.pBits);
  auto* dest = static_cast<std::byte*>(destBuffer);

  // Get surface dimensions.
  const int mip_width = std::max(1, tex_info_.sizeX >> mipMapNr);
  const int mip_height = std::max(1, tex_info_.sizeY >> mipMapNr);

  // Handle DXT compressed formats and PAL_8 only.
  switch (tex_info_.texFormat) {
    case zRND_TEX_FORMAT_DXT1:
    case zRND_TEX_FORMAT_DXT3:
    case zRND_TEX_FORMAT_DXT5: {
      // For DXT, copy entire linear buffer (height is in blocks).
      const int block_height = (mip_height + 3) / 4;
      std::memcpy(dest, src, block_height * locked_rect.Pitch);
      break;
    }
    case zRND_TEX_FORMAT_PAL_8: {
      // 8-bit paletted - copy row by row.
      for (int h = 0; h < mip_height; ++h) {
        std::memcpy(dest + h * destPitchXBytes, src + h * locked_rect.Pitch, mip_width);
      }
      break;
    }
    default:
      UnlockTexture_D3D9(texture_, mipMapNr);
      return 0;
  }

  UnlockTexture_D3D9(texture_, mipMapNr);
  return 1;
}

auto zCTex_D3D9::HasAlpha() -> int {
  return has_alpha_ ? 1 : 0;
}

void zCTex_D3D9::ReleaseData() {
  ReleaseResourceData();
}

auto zCTex_D3D9::LoadResourceData() -> int {
  is_loading_ = true;

  // With D3DPOOL_MANAGED textures and proper device Reset(),
  // textures survive resolution changes - just use the base class
  // implementation.
  const int result = zCTexture::LoadResourceData();

  is_loading_ = false;

  if (result != 0) {
    cacheState = zRES_CACHED_IN;
  } else {
    SPDLOG_WARN("LoadResourceData FAILED [{}]", DebugName());
  }
  return result;
}

auto zCTex_D3D9::GetDX9Texture() -> IDirect3DTexture9* {
  if (cacheState == zRES_CACHED_OUT) {
    CacheIn(-1);
  }
  return texture_;
}

auto zCTex_D3D9::ComputeExposedPitch(int mip_level, int actual_pitch) const -> int {
  // Gothic's texture converter writes data using a "logical pitch" based on
  // bytesPerPixel from GetTexFormatInfo:
  //   pitch = (width >> mip_level) * bytesPerPixel
  //
  // For non-compressed formats, this matches the D3D9 surface pitch.
  // For DXT compressed formats, Gothic writes a linear buffer:
  //   DXT1: bytesPerPixel = 0.5 -> pitch = width/2
  //   DXT3/5: bytesPerPixel = 1.0 -> pitch = width
  //
  // But D3D9 DXT surfaces use block pitch:
  //   DXT1: (width/4) * 8 = width * 2
  //   DXT3/5: (width/4) * 16 = width * 4
  //
  // So for DXT, we return the "Gothic logical pitch" and will need to
  // handle the pitch mismatch when unlocking the texture.

  int width = tex_info_.sizeX >> mip_level;
  if (width < 1) {
    width = 1;
  }

  switch (tex_info_.texFormat) {
    case zRND_TEX_FORMAT_DXT1: {
      // Gothic expects width * 0.5 = width / 2.
      const int pitch = width >> 1;
      return pitch < 1 ? 1 : pitch;
    }
    case zRND_TEX_FORMAT_DXT2:
    case zRND_TEX_FORMAT_DXT3:
    case zRND_TEX_FORMAT_DXT4:
    case zRND_TEX_FORMAT_DXT5:
      // Gothic expects width * 1.0 = width.
      return width;
    default:
      // For non-compressed formats, D3D9 pitch matches Gothic's expectation.
      return actual_pitch;
  }
}

void zCTex_D3D9::UnlockAllMipLevels() {
  if (texture_ == nullptr) {
    locked_mip_mask_.reset();
    std::ranges::fill(locked_rects_, D3D9LockedRect{});
    std::ranges::fill(exposed_pitches_, 0);
    return;
  }

  for (int mip = 0; mip < kMaxTrackedMips; ++mip) {
    if (locked_mip_mask_.test(mip)) {
      UnlockTexture_D3D9(texture_, mip);
      locked_mip_mask_.reset(mip);
      locked_rects_[mip] = {};
      exposed_pitches_[mip] = 0;
    }
  }
}

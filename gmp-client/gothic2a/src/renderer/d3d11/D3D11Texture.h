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

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <bitset>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "../common.h"
#include "ZenGin/zGothicAPI.h"

using Microsoft::WRL::ComPtr;

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace gmp::renderer::d3d11 {

// D3D11 locked rect replacement for staging textures
struct D3D11LockedRect {
  uint32_t RowPitch = 0;
  void* pData = nullptr;
};

// Convert Gothic texture format to DXGI format
[[nodiscard]] DXGI_FORMAT GothicFormatToDXGI(zTRnd_TextureFormat format, bool* hasAlpha = nullptr);

// Calculate bytes per pixel for a given DXGI format
[[nodiscard]] uint32_t GetBytesPerPixel(DXGI_FORMAT format);

// Check if format is DXT/BC compressed
[[nodiscard]] bool IsCompressedFormat(DXGI_FORMAT format);

// Get block size for compressed formats (4 for BC, 1 for uncompressed)
[[nodiscard]] uint32_t GetBlockSize(DXGI_FORMAT format);

// Get bytes per block for compressed formats
[[nodiscard]] uint32_t GetBytesPerBlock(DXGI_FORMAT format);

}  // namespace gmp::renderer::d3d11

// D3D11 texture class replacing zCTexture
class zCTex_D3D11 : public zCTexture {
public:
  zCTex_D3D11();
  ~zCTex_D3D11() override;

  // Non-copyable, non-movable
  zCTex_D3D11(const zCTex_D3D11&) = delete;
  zCTex_D3D11& operator=(const zCTex_D3D11&) = delete;
  zCTex_D3D11(zCTex_D3D11&&) = delete;
  zCTex_D3D11& operator=(zCTex_D3D11&&) = delete;

  // zCTextureExchange interface
  int Lock(int mode) override;
  int Unlock() override;
  int SetTextureInfo(const zCTextureInfo& texInfo) override;
  void* GetPaletteBuffer() override;
  int GetTextureBuffer(int mipMapNr, void*& buffer, int& pitchXBytes) override;
  zCTextureInfo GetTextureInfo() override;
  int CopyPaletteDataTo(void* destBuffer) override;
  int CopyTextureDataTo(int mipMapNr, void* destBuffer, int destPitchXBytes) override;
  int HasAlpha() override;

  // zCTexture interface
  void ReleaseData() override;

  // zCResource interface overrides
  int ReleaseResourceData() override;
  int LoadResourceData() override;

  // D3D11 specific accessors
  [[nodiscard]] ID3D11Texture2D* GetTexture() const {
    return texture_.Get();
  }
  [[nodiscard]] ID3D11ShaderResourceView* GetSRV() const {
    return srv_.Get();
  }

  // Get the DXGI format of this texture (cached, avoids COM GetDesc call).
  [[nodiscard]] DXGI_FORMAT GetDXGIFormat() const {
    return dxgiFormat_;
  }

  // Heuristic used by the D3D11 renderer front-end to pick between TEST and BLEND_TEST
  // for world geometry. Returns true if this texture is likely to contain non-binary alpha.
  [[nodiscard]] bool HasSmoothAlpha() const;

  // Check if the texture is ready for rendering:
  // - Not currently locked (being loaded)
  // - GPU texture exists with valid uploaded content
  [[nodiscard]] bool IsReadyForRendering() const {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return !is_locked_ && texture_ && srv_ && gpu_content_valid_;
  }

  // Create SRV for binding to shaders (if not already created)
  bool EnsureSRV(ID3D11Device* device);

private:
  static constexpr int kMaxTrackedMips = 16;

  [[nodiscard]] std::string DebugName() const;
  void UnlockAllMipLevels();
  bool CreateStagingTexture(ID3D11Device* device);
  void CopyFromStagingToGPU(ID3D11DeviceContext* context, const std::bitset<kMaxTrackedMips>& mipsToCopy);
  void CopyLinearBufferToStaging(int mipMapNr);

  ComPtr<ID3D11Texture2D> texture_;         // GPU texture (DEFAULT usage)
  ComPtr<ID3D11Texture2D> stagingTexture_;  // Staging texture for CPU access
  ComPtr<ID3D11ShaderResourceView> srv_;    // Shader resource view

  zCTextureInfo tex_info_{};
  DXGI_FORMAT dxgiFormat_ = DXGI_FORMAT_UNKNOWN;

  bool is_locked_ = false;
  bool is_loading_ = false;
  bool has_alpha_ = false;
  bool staging_dirty_ = false;      // True if staging texture has been written to
  bool gpu_content_valid_ = false;  // True after staging->GPU copy completes (texture safe to sample)

  // Smooth alpha detection (used for auto alpha mode selection in the world geometry path).
  bool smooth_alpha_hint_ = false;
  bool smooth_alpha_known_ = false;
  bool smooth_alpha_detected_ = false;

  std::bitset<kMaxTrackedMips> locked_mip_mask_{};
  std::array<gmp::renderer::d3d11::D3D11LockedRect, kMaxTrackedMips> locked_rects_{};
  std::array<int, kMaxTrackedMips> exposed_pitches_{};

  // Linear buffers for DXT formats where D3D11 pitch differs from Gothic's expected pitch
  std::array<std::vector<uint8_t>, kMaxTrackedMips> linear_buffers_{};

  // Thread safety: protect state that can be accessed from both render and loader threads
  // Must be recursive_mutex because Gothic's CacheIn() can trigger callbacks that re-enter Lock()
  mutable std::recursive_mutex state_mutex_;
};

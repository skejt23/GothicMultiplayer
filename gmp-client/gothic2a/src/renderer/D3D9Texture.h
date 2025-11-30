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
#include <bitset>
#include <cstdint>
#include <string>

#include "../common.h"
#include "ZenGin/zGothicAPI.h"

struct IDirect3DDevice9;
struct IDirect3DTexture9;

// D3D9 texture wrapper types and functions
struct D3D9LockedRect {
  int Pitch = 0;
  void* pBits = nullptr;
};

// Format constants (mapped from D3DFORMAT)
enum class D3D9Format : uint32_t {
  Unknown = 0,
  R8G8B8 = 20,
  A8R8G8B8 = 21,
  X8R8G8B8 = 22,
  R5G6B5 = 23,
  X1R5G5B5 = 24,
  A1R5G5B5 = 25,
  A4R4G4B4 = 26,
  R3G3B2 = 27,
  A8 = 28,
  A8R3G3B2 = 29,
  X4R4G4B4 = 30,
  A2B10G10R10 = 31,
  A8B8G8R8 = 32,
  X8B8G8R8 = 33,
  G16R16 = 34,
  A2R10G10B10 = 35,
  A16B16G16R16 = 36,
  DXT1 = 0x31545844,
  DXT2 = 0x32545844,
  DXT3 = 0x33545844,
  DXT4 = 0x34545844,
  DXT5 = 0x35545844,
};

[[nodiscard]] IDirect3DTexture9* CreateTexture_D3D9(IDirect3DDevice9* device, int width, int height, int levels, uint32_t usage, D3D9Format format,
                                                    int pool);
void ReleaseTexture_D3D9(IDirect3DTexture9* texture);
[[nodiscard]] bool LockTexture_D3D9(IDirect3DTexture9* texture, int level, D3D9LockedRect* rect_out);
bool UnlockTexture_D3D9(IDirect3DTexture9* texture, int level);

class zCTex_D3D9 : public zCTexture {
public:
  zCTex_D3D9();
  ~zCTex_D3D9() override;

  // Non-copyable, non-movable
  zCTex_D3D9(const zCTex_D3D9&) = delete;
  zCTex_D3D9& operator=(const zCTex_D3D9&) = delete;
  zCTex_D3D9(zCTex_D3D9&&) = delete;
  zCTex_D3D9& operator=(zCTex_D3D9&&) = delete;

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

  // Helper to get the D3D9 texture
  // With D3DPOOL_MANAGED textures and proper device Reset(), textures survive resolution changes.
  [[nodiscard]] IDirect3DTexture9* GetDX9Texture();

private:
  [[nodiscard]] std::string DebugName() const;
  void UnlockAllMipLevels();
  [[nodiscard]] int ComputeExposedPitch(int mipLevel, int actualPitch) const;

  static constexpr int kMaxTrackedMips = 16;

  IDirect3DTexture9* texture_ = nullptr;
  zCTextureInfo tex_info_{};
  bool is_locked_ = false;
  bool is_loading_ = false;
  bool has_alpha_ = false;

  std::bitset<kMaxTrackedMips> locked_mip_mask_{};
  std::array<D3D9LockedRect, kMaxTrackedMips> locked_rects_{};
  std::array<int, kMaxTrackedMips> exposed_pitches_{};
};

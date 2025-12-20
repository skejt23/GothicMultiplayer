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
// D3D11 Vertex Buffer Implementation for Gothic II
// ----------------------------------------------------------------------------
// This file implements Gothic's zCVertexBuffer interface using Direct3D 11.
//
// Vertex Buffer Types in the Renderer:
//
// 1. Static Vertex Buffers (this file):
//    - Created via zCRenderer::CreateVertexBuffer()
//    - Used for world geometry, static meshes, skeletal animation
//    - D3D11_USAGE_DEFAULT with staging buffer for uploads
//    - Locked once for initial upload, then optimized (marked GPU-only)
//
// 2. Dynamic Ring Buffers (DynamicVertexBuffer.h):
//    - Used internally by D3D11RendererImpl for batched rendering
//    - Ring buffer pattern with NOOVERWRITE/DISCARD for streaming
//    - Optimal for per-frame dynamic geometry (particles, UI, alpha polys)
//
// Lock Flags (Gothic to D3D11 mapping):
// - kLockReadOnly (1)     → Map with READ flag (requires staging copy)
// - kLockDiscard (16)     → D3D11_MAP_WRITE_DISCARD
// - kLockNoOverwrite (32) → D3D11_MAP_WRITE_NO_OVERWRITE
// - kLockNoSysLock (64)   → No equivalent in D3D11
//
// Device Reset Handling:
// D3D11 resources don't need explicit reset handling like D3D9, but we still
// track all buffers for coordinated cleanup during shutdown.
// ----------------------------------------------------------------------------

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <vector>

#include "ZenGin/zGothicAPI.h"

using Microsoft::WRL::ComPtr;

// D3D11 implementation of Gothic's vertex buffer interface.
// Manages GPU vertex data and provides lock/unlock semantics for CPU access.
class zCVertexBuffer_D3D11 : public zCVertexBuffer {
public:
  zCVertexBuffer_D3D11();
  ~zCVertexBuffer_D3D11() override;

  // zCVertexBuffer interface.
  int Create(unsigned long fvf, unsigned long num_verts, unsigned long flags) override;
  int Destroy() override;
  int Lock(unsigned long flags) override;
  int Unlock() override;
  int IsLocked() override;
  int Optimize() override;
  int SetPrimitiveType(zTVBufferPrimitiveType type) override;
  zTVBufferPrimitiveType GetPrimitiveType() override;
  unsigned long GetVertexFormat() override;
  zTVBufferVertexType GetVertexType() override;
  int SetIndexList(const zCArray<unsigned short>& list) override;
  int SetIndexListSize(unsigned long size) override;
  unsigned long GetIndexListSize() override;
  unsigned short* GetIndexListPtr() override;

  // D3D11-specific accessors.
  [[nodiscard]] ID3D11Buffer* GetBuffer() const {
    return buffer_.Get();
  }
  [[nodiscard]] std::uint32_t GetVertexStride() const {
    return static_cast<std::uint32_t>(arrayStride);
  }
  [[nodiscard]] std::uint32_t GetVertexFormat_D3D11() const {
    return vertex_format_;
  }

  // Bind this buffer for rendering
  void Bind(ID3D11DeviceContext* context, uint32_t slot = 0) const;

  // Static methods for device reset/shutdown handling.
  static void DestroyAllBuffers();
  static std::vector<zCVertexBuffer_D3D11*>& GetAllBuffers() {
    return all_buffers_;
  }

private:
  ComPtr<ID3D11Buffer> buffer_;         // GPU vertex buffer (DYNAMIC usage)
  ComPtr<ID3D11Buffer> stagingBuffer_;  // Staging buffer for CPU access (unused currently)

  zTVBufferPrimitiveType primitive_type_ = zVBUFFER_PT_TRIANGLELIST;
  std::uint32_t vertex_format_ = 0;
  std::uint32_t num_verts_ = 0;
  bool is_locked_ = false;
  bool optimized_ = false;
  bool is_dynamic_ = false;     // True if created with dynamic flag
  bool staging_dirty_ = false;  // True if staging buffer has new data



  std::vector<std::uint16_t> index_list_;

  // Global registry of all vertex buffers for cleanup.
  static std::vector<zCVertexBuffer_D3D11*> all_buffers_;

  [[nodiscard]] static constexpr std::uint32_t CalculateVertexSize(std::uint32_t z_fvf);
  void SetupComponentPointers(unsigned char* base);
};

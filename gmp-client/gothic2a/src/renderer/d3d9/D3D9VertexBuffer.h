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
// D3D9 Vertex Buffer Implementation for Gothic II
// ----------------------------------------------------------------------------
// This file implements Gothic's zCVertexBuffer interface using Direct3D 9.
//
// Vertex Buffer Types in the Renderer:
//
// 1. Static Vertex Buffers (this file):
//    - Created via zCRenderer::CreateVertexBuffer()
//    - Used for world geometry, static meshes, skeletal animation
//    - D3DPOOL_DEFAULT with optional D3DUSAGE_DYNAMIC
//    - Locked once for initial upload, then optimized (marked GPU-only)
//
// 2. Dynamic Ring Buffers (DynamicVertexBuffer.h):
//    - Used internally by D3D9RendererImpl for batched rendering
//    - Ring buffer pattern with NOOVERWRITE/DISCARD for streaming
//    - Optimal for per-frame dynamic geometry (particles, UI, alpha polys)
//
// Lock Flags:
// Gothic uses custom lock flags that map to D3D9:
// - kLockReadOnly (1)     → D3DLOCK_READONLY
// - kLockDiscard (16)     → D3DLOCK_DISCARD (orphan buffer, get fresh memory)
// - kLockNoOverwrite (32) → D3DLOCK_NOOVERWRITE (promise not to touch used data)
// - kLockNoSysLock (64)   → D3DLOCK_NOSYSLOCK (no critical section, faster)
//
// Device Reset Handling:
// D3DPOOL_DEFAULT resources are lost on device reset (resolution change, Alt+Tab).
// All vertex buffers are tracked in a global list for coordinated release/recreate.
// ----------------------------------------------------------------------------

#include <cstdint>
#include <vector>

#include "ZenGin/zGothicAPI.h"

struct IDirect3DVertexBuffer9;

// D3D9 implementation of Gothic's vertex buffer interface.
// Manages GPU vertex data and provides lock/unlock semantics for CPU access.
class zCVertexBuffer_D3D9 : public zCVertexBuffer {
public:
  zCVertexBuffer_D3D9();
  ~zCVertexBuffer_D3D9() override;

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

  // D3D9-specific accessors.
  [[nodiscard]] IDirect3DVertexBuffer9* GetHandle() const {
    return handle_;
  }
  [[nodiscard]] std::uint32_t GetD3DFVF() const {
    return d3d_fvf_;
  }
  [[nodiscard]] std::uint32_t GetVertexStride() const {
    return static_cast<std::uint32_t>(arrayStride);
  }

  // Static methods for device reset handling.
  static void DestroyAllBuffers();
  static std::vector<zCVertexBuffer_D3D9*>& GetAllBuffers() {
    return all_buffers_;
  }

private:
  IDirect3DVertexBuffer9* handle_ = nullptr;
  zTVBufferPrimitiveType primitive_type_ = zVBUFFER_PT_TRIANGLELIST;
  std::uint32_t vertex_format_ = 0;
  std::uint32_t num_verts_ = 0;
  std::uint32_t d3d_fvf_ = 0;
  bool is_locked_ = false;
  bool optimized_ = false;

  std::vector<std::uint16_t> index_list_;

  // Global registry of all vertex buffers for device reset handling.
  static std::vector<zCVertexBuffer_D3D9*> all_buffers_;

  [[nodiscard]] std::uint32_t ConvertVertexFormat(std::uint32_t z_fvf) const;
  [[nodiscard]] static constexpr std::uint32_t CalculateVertexSize(std::uint32_t z_fvf);
};

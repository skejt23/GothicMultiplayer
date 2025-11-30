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

#include "D3D9VertexBuffer.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

extern IDirect3DDevice9* g_D3D9Device;

std::vector<zCVertexBuffer_D3D9*> zCVertexBuffer_D3D9::all_buffers_;

zCVertexBuffer_D3D9::zCVertexBuffer_D3D9() {
  // Initialize base class members.
  numVertex = 0;
  arrayStride = 0;
  array.basePtr = nullptr;
  array.XYZPtr = nullptr;
  array.XYZRHWPtr = nullptr;
  array.normalPtr = nullptr;
  array.colorPtr = nullptr;
  array.texUV0Ptr = nullptr;
  array.texUV1Ptr = nullptr;
  array.texUV2Ptr = nullptr;
  array.texUV3Ptr = nullptr;
  numVertsFilled = 0;
  createFlags = 0;
  vertexBufferID = 0;

  // Register in global list for device reset handling.
  all_buffers_.push_back(this);
}

zCVertexBuffer_D3D9::~zCVertexBuffer_D3D9() {
  Destroy();

  // Remove from global list.
  auto it = std::find(all_buffers_.begin(), all_buffers_.end(), this);
  if (it != all_buffers_.end()) {
    all_buffers_.erase(it);
  }
}

void zCVertexBuffer_D3D9::DestroyAllBuffers() {
  for (zCVertexBuffer_D3D9* vb : all_buffers_) {
    if (vb != nullptr && vb->handle_ != nullptr) {
      vb->handle_->Release();
      vb->handle_ = nullptr;
    }
  }
}

namespace {

// Vertex component sizes in bytes.
constexpr std::uint32_t kXYZSize = 12;      // 3 floats
constexpr std::uint32_t kXYZRHWSize = 16;   // 4 floats
constexpr std::uint32_t kNormalSize = 12;   // 3 floats
constexpr std::uint32_t kColorSize = 4;     // 1 DWORD
constexpr std::uint32_t kTexCoordSize = 8;  // 2 floats per UV

constexpr std::uint32_t GetTexCoordCount(std::uint32_t format) {
  if (format & zVBUFFER_VERTFORMAT_TEXUV4)
    return 4;
  if (format & zVBUFFER_VERTFORMAT_TEXUV3)
    return 3;
  if (format & zVBUFFER_VERTFORMAT_TEXUV2)
    return 2;
  if (format & zVBUFFER_VERTFORMAT_TEXUV1)
    return 1;
  return 0;
}

}  // namespace

std::uint32_t zCVertexBuffer_D3D9::ConvertVertexFormat(std::uint32_t z_fvf) const {
  std::uint32_t d3d_fvf = 0;

  if (z_fvf & zVBUFFER_VERTFORMAT_XYZ) {
    d3d_fvf |= D3DFVF_XYZ;
  }
  if (z_fvf & zVBUFFER_VERTFORMAT_XYZRHW) {
    d3d_fvf |= D3DFVF_XYZRHW;
  }
  if (z_fvf & zVBUFFER_VERTFORMAT_NORMAL) {
    d3d_fvf |= D3DFVF_NORMAL;
  }
  if (z_fvf & zVBUFFER_VERTFORMAT_COLOR) {
    d3d_fvf |= D3DFVF_DIFFUSE;
  }

  switch (GetTexCoordCount(z_fvf)) {
    case 1:
      d3d_fvf |= D3DFVF_TEX1;
      break;
    case 2:
      d3d_fvf |= D3DFVF_TEX2;
      break;
    case 3:
      d3d_fvf |= D3DFVF_TEX3;
      break;
    case 4:
      d3d_fvf |= D3DFVF_TEX4;
      break;
    default:
      break;
  }

  return d3d_fvf;
}

constexpr std::uint32_t zCVertexBuffer_D3D9::CalculateVertexSize(std::uint32_t z_fvf) {
  std::uint32_t size = 0;

  if (z_fvf & zVBUFFER_VERTFORMAT_XYZ) {
    size += kXYZSize;
  } else if (z_fvf & zVBUFFER_VERTFORMAT_XYZRHW) {
    size += kXYZRHWSize;
  }
  if (z_fvf & zVBUFFER_VERTFORMAT_NORMAL) {
    size += kNormalSize;
  }
  if (z_fvf & zVBUFFER_VERTFORMAT_COLOR) {
    size += kColorSize;
  }
  size += GetTexCoordCount(z_fvf) * kTexCoordSize;

  return size;
}

int zCVertexBuffer_D3D9::Create(unsigned long fvf, unsigned long num_verts, unsigned long flags) {
  // Gothic buffer creation flags.
  constexpr unsigned long kFlagDoNotClip = 1;
  constexpr unsigned long kFlagSystemMemory = 4;
  constexpr unsigned long kFlagWriteOnly = 8;

  Destroy();

  vertex_format_ = fvf;
  num_verts_ = num_verts;
  numVertex = num_verts;
  createFlags = flags;
  d3d_fvf_ = ConvertVertexFormat(fvf);

  const auto vert_size = CalculateVertexSize(fvf);
  arrayStride = vert_size;

  const auto total_size = num_verts * vert_size;

  IDirect3DDevice9* device = g_D3D9Device;
  if (device == nullptr) {
    SPDLOG_ERROR("VB Create called with no D3D device - check hook timing");
    return 0;
  }

  DWORD d3d_usage = 0;
  D3DPOOL pool = D3DPOOL_DEFAULT;

  if (flags & kFlagSystemMemory) {
    pool = D3DPOOL_SYSTEMMEM;
  }
  if (flags & kFlagDoNotClip) {
    d3d_usage |= D3DUSAGE_DONOTCLIP;
  }
  if (flags & kFlagWriteOnly) {
    d3d_usage |= D3DUSAGE_WRITEONLY;
    // D3DUSAGE_DYNAMIC is required for D3DLOCK_DISCARD in D3DPOOL_DEFAULT.
    if (pool == D3DPOOL_DEFAULT) {
      d3d_usage |= D3DUSAGE_DYNAMIC;
    }
  }

  IDirect3DVertexBuffer9* vb = nullptr;
  HRESULT hr = device->CreateVertexBuffer(total_size, d3d_usage, d3d_fvf_, pool, &vb, nullptr);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create vertex buffer. Error: {:#x}", static_cast<unsigned long>(hr));
    return 0;
  }

  handle_ = vb;
  return 1;
}

int zCVertexBuffer_D3D9::Destroy() {
  if (handle_ != nullptr) {
    handle_->Release();
    handle_ = nullptr;
  }
  is_locked_ = false;
  optimized_ = false;
  d3d_fvf_ = 0;
  num_verts_ = 0;
  return 1;
}

int zCVertexBuffer_D3D9::Lock(unsigned long flags) {
  // Gothic lock flags.
  constexpr unsigned long kLockReadOnly = 1;
  constexpr unsigned long kLockDiscard = 16;
  constexpr unsigned long kLockNoOverwrite = 32;
  constexpr unsigned long kLockNoSysLock = 64;

  if (handle_ == nullptr) {
    return 0;
  }

  // Optimized buffers are GPU-only and cannot be locked.
  if (optimized_) {
    SPDLOG_WARN("Attempted to lock an optimized vertex buffer");
    return 0;
  }

  // Prevent double locking which can crash D3D9 or cause driver stalls.
  // If already locked, assume the pointer is still valid.
  if (is_locked_) {
    return 1;
  }

  DWORD lock_flags = 0;
  if (flags & kLockReadOnly) {
    lock_flags |= D3DLOCK_READONLY;
  }
  if (flags & kLockDiscard) {
    lock_flags |= D3DLOCK_DISCARD;
  }
  if (flags & kLockNoOverwrite) {
    lock_flags |= D3DLOCK_NOOVERWRITE;
  }
  if (flags & kLockNoSysLock) {
    lock_flags |= D3DLOCK_NOSYSLOCK;
  }

  void* ptr = nullptr;
  const auto size = num_verts_ * arrayStride;
  HRESULT hr = handle_->Lock(0, size, &ptr, lock_flags);
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to lock vertex buffer. Error: {:#x}", static_cast<unsigned long>(hr));
    return 0;
  }

  if (ptr == nullptr) {
    return 0;
  }

  is_locked_ = true;
  array.basePtr = static_cast<unsigned char*>(ptr);

  // Setup component pointers based on vertex format.
  unsigned char* curr = array.basePtr;

  if (vertex_format_ & zVBUFFER_VERTFORMAT_XYZ) {
    array.XYZPtr = reinterpret_cast<zVEC3*>(curr);
    curr += kXYZSize;
  } else if (vertex_format_ & zVBUFFER_VERTFORMAT_XYZRHW) {
    array.XYZRHWPtr = reinterpret_cast<zVEC4*>(curr);
    curr += kXYZRHWSize;
  }

  if (vertex_format_ & zVBUFFER_VERTFORMAT_NORMAL) {
    array.normalPtr = reinterpret_cast<zVEC3*>(curr);
    curr += kNormalSize;
  }

  if (vertex_format_ & zVBUFFER_VERTFORMAT_COLOR) {
    array.colorPtr = reinterpret_cast<zCOLOR*>(curr);
    curr += kColorSize;
  }

  const auto tex_coord_count = GetTexCoordCount(vertex_format_);
  for (std::uint32_t i = 0; i < tex_coord_count; ++i) {
    array.texUVPtr[i] = reinterpret_cast<zVEC2*>(curr);
    curr += kTexCoordSize;
  }
  for (std::uint32_t i = tex_coord_count; i < 4; ++i) {
    array.texUVPtr[i] = nullptr;
  }

  return 1;
}

int zCVertexBuffer_D3D9::Unlock() {
  if (!is_locked_) {
    return 1;
  }

  if (handle_ == nullptr) {
    return 0;
  }

  handle_->Unlock();
  is_locked_ = false;
  std::memset(&array, 0, sizeof(array));
  return 1;
}

int zCVertexBuffer_D3D9::IsLocked() {
  return is_locked_ ? 1 : 0;
}

int zCVertexBuffer_D3D9::Optimize() {
  // In D3D9, this is a semantic marker that prevents further locking,
  // enforcing that this is static geometry that won't be modified.
  if (handle_ == nullptr) {
    return 0;
  }
  if (optimized_) {
    return 1;
  }

  optimized_ = true;
  numVertsFilled = numVertex;
  return 1;
}

int zCVertexBuffer_D3D9::SetPrimitiveType(zTVBufferPrimitiveType type) {
  primitive_type_ = type;
  return 1;
}

zTVBufferPrimitiveType zCVertexBuffer_D3D9::GetPrimitiveType() {
  return primitive_type_;
}

unsigned long zCVertexBuffer_D3D9::GetVertexFormat() {
  return vertex_format_;
}

zTVBufferVertexType zCVertexBuffer_D3D9::GetVertexType() {
  // Transformed vertices (already in screen space, RHW present).
  if (vertex_format_ & zVBUFFER_VERTFORMAT_XYZRHW) {
    return zVBUFFER_VERTTYPE_T_L;
  }

  // Untransformed with normals but no color = Unlit (needs D3D lighting).
  const bool has_normal = (vertex_format_ & zVBUFFER_VERTFORMAT_NORMAL) != 0;
  const bool has_color = (vertex_format_ & zVBUFFER_VERTFORMAT_COLOR) != 0;
  if (has_normal && !has_color) {
    return zVBUFFER_VERTTYPE_UT_UL;
  }

  // Untransformed with color or without normals = Lit (pre-lit).
  return zVBUFFER_VERTTYPE_UT_L;
}

int zCVertexBuffer_D3D9::SetIndexList(const zCArray<unsigned short>& list) {
  const int count = list.GetNumInList();
  index_list_.resize(count);
  if (count > 0) {
    std::memcpy(index_list_.data(), list.GetArray(), count * sizeof(unsigned short));
  }
  return 1;
}

int zCVertexBuffer_D3D9::SetIndexListSize(unsigned long size) {
  index_list_.resize(size);
  return 1;
}

unsigned long zCVertexBuffer_D3D9::GetIndexListSize() {
  return static_cast<unsigned long>(index_list_.size());
}

unsigned short* zCVertexBuffer_D3D9::GetIndexListPtr() {
  return index_list_.data();
}

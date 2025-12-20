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

#include "D3D11VertexBuffer.h"

#include "D3D11MapTracker.h"
#include "D3D11RendererImpl.h"
using namespace gmp::renderer::d3d11;

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

std::vector<zCVertexBuffer_D3D11*> zCVertexBuffer_D3D11::all_buffers_;

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

zCVertexBuffer_D3D11::zCVertexBuffer_D3D11() {
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

  // Register in global list for cleanup.
  all_buffers_.push_back(this);
}

zCVertexBuffer_D3D11::~zCVertexBuffer_D3D11() {
  Destroy();

  // Remove from global list.
  auto it = std::find(all_buffers_.begin(), all_buffers_.end(), this);
  if (it != all_buffers_.end()) {
    all_buffers_.erase(it);
  }
}

void zCVertexBuffer_D3D11::DestroyAllBuffers() {
  for (zCVertexBuffer_D3D11* vb : all_buffers_) {
    if (vb != nullptr) {
      vb->buffer_.Reset();
      vb->stagingBuffer_.Reset();
    }
  }
}

constexpr std::uint32_t zCVertexBuffer_D3D11::CalculateVertexSize(std::uint32_t z_fvf) {
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

void zCVertexBuffer_D3D11::SetupComponentPointers(unsigned char* base) {
  array.basePtr = base;
  unsigned char* curr = base;

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
}

int zCVertexBuffer_D3D11::Create(unsigned long fvf, unsigned long num_verts, unsigned long flags) {
  // Gothic buffer creation flags.
  constexpr unsigned long kFlagDoNotClip = 1;
  constexpr unsigned long kFlagSystemMemory = 4;
  constexpr unsigned long kFlagWriteOnly = 8;

  Destroy();

  vertex_format_ = fvf;
  num_verts_ = num_verts;
  numVertex = num_verts;
  createFlags = flags;

  const auto vert_size = CalculateVertexSize(fvf);
  arrayStride = vert_size;

  const auto total_size = num_verts * vert_size;

  if (!g_D3D11Device) {
    SPDLOG_ERROR("VB Create called with no D3D11 device - check hook timing");
    return 0;
  }

  // Use DYNAMIC buffer for Gothic vertex buffers (allows Map/Unmap).
  // With multithread protection enabled, Map operations are safe.
  is_dynamic_ = true;

  D3D11_BUFFER_DESC bufferDesc = {};
  bufferDesc.ByteWidth = total_size;
  bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
  bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bufferDesc.MiscFlags = 0;
  bufferDesc.StructureByteStride = 0;

  HRESULT hr = g_D3D11Device->CreateBuffer(&bufferDesc, nullptr, buffer_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create D3D11 vertex buffer. Error: 0x{:08X}", static_cast<uint32_t>(hr));
    return 0;
  }

  return 1;
}

int zCVertexBuffer_D3D11::Destroy() {
  buffer_.Reset();
  stagingBuffer_.Reset();
  is_locked_ = false;
  optimized_ = false;
  is_dynamic_ = false;
  staging_dirty_ = false;
  num_verts_ = 0;
  return 1;
}

int zCVertexBuffer_D3D11::Lock(unsigned long flags) {
  if (!buffer_) {
    return 0;
  }

  // Optimized buffers are GPU-only and cannot be locked.
  if (optimized_) {
    SPDLOG_WARN("Attempted to lock an optimized vertex buffer");
    return 0;
  }

  // Prevent double locking
  if (is_locked_) {
    return 1;
  }

  if (!g_D3D11Context) {
    return 0;
  }

  // Map the buffer with WRITE_DISCARD (replaces contents, no sync needed)
  D3D11_MAPPED_SUBRESOURCE mapped;
  D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD;

  HRESULT hr = TrackedMap(g_D3D11Context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, mapType, 0, &mapped, "zCVertexBuffer_D3D11::Lock");
  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to map vertex buffer: 0x{:08X}", static_cast<uint32_t>(hr));
    return 0;
  }

  is_locked_ = true;
  SetupComponentPointers(static_cast<unsigned char*>(mapped.pData));

  return 1;
}

int zCVertexBuffer_D3D11::Unlock() {
  if (!is_locked_) {
    return 1;
  }

  // If buffer or context is null, the buffer cannot remain in a valid mapped state.
  // Reset is_locked_ to prevent a subsequent Unlock from trying to Unmap an unmapped buffer.
  if (!buffer_) {
    is_locked_ = false;
    std::memset(&array, 0, sizeof(array));
    SPDLOG_WARN("Unlock called with null buffer - resetting lock state");
    return 0;
  }

  if (!g_D3D11Context) {
    is_locked_ = false;
    std::memset(&array, 0, sizeof(array));
    SPDLOG_WARN("Unlock called with null context - resetting lock state");
    return 0;
  }

  // Unmap the buffer
  TrackedUnmap(g_D3D11Context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, "zCVertexBuffer_D3D11::Unlock");

  is_locked_ = false;
  std::memset(&array, 0, sizeof(array));
  return 1;
}

int zCVertexBuffer_D3D11::IsLocked() {
  return is_locked_ ? 1 : 0;
}

int zCVertexBuffer_D3D11::Optimize() {
  // Mark as optimized - prevents further locking
  if (!buffer_) {
    return 0;
  }
  if (optimized_) {
    return 1;
  }

  // Make sure any pending data is uploaded
  if (is_locked_) {
    Unlock();
  }

  optimized_ = true;
  numVertsFilled = numVertex;

  return 1;
}

int zCVertexBuffer_D3D11::SetPrimitiveType(zTVBufferPrimitiveType type) {
  primitive_type_ = type;
  return 1;
}

zTVBufferPrimitiveType zCVertexBuffer_D3D11::GetPrimitiveType() {
  return primitive_type_;
}

unsigned long zCVertexBuffer_D3D11::GetVertexFormat() {
  return vertex_format_;
}

zTVBufferVertexType zCVertexBuffer_D3D11::GetVertexType() {
  // Transformed vertices (already in screen space, RHW present).
  if (vertex_format_ & zVBUFFER_VERTFORMAT_XYZRHW) {
    return zVBUFFER_VERTTYPE_T_L;
  }

  // Untransformed with normals but no color = Unlit (needs lighting).
  const bool has_normal = (vertex_format_ & zVBUFFER_VERTFORMAT_NORMAL) != 0;
  const bool has_color = (vertex_format_ & zVBUFFER_VERTFORMAT_COLOR) != 0;
  if (has_normal && !has_color) {
    return zVBUFFER_VERTTYPE_UT_UL;
  }

  // Untransformed with color or without normals = Lit (pre-lit).
  return zVBUFFER_VERTTYPE_UT_L;
}

int zCVertexBuffer_D3D11::SetIndexList(const zCArray<unsigned short>& list) {
  const int count = list.GetNumInList();
  index_list_.resize(count);
  if (count > 0) {
    std::memcpy(index_list_.data(), list.GetArray(), count * sizeof(unsigned short));
  }
  return 1;
}

int zCVertexBuffer_D3D11::SetIndexListSize(unsigned long size) {
  index_list_.resize(size);
  return 1;
}

unsigned long zCVertexBuffer_D3D11::GetIndexListSize() {
  return static_cast<unsigned long>(index_list_.size());
}

unsigned short* zCVertexBuffer_D3D11::GetIndexListPtr() {
  return index_list_.data();
}

void zCVertexBuffer_D3D11::Bind(ID3D11DeviceContext* context, uint32_t slot) const {
  if (!context || !buffer_) {
    return;
  }

  UINT stride = static_cast<UINT>(arrayStride);
  UINT offset = 0;
  ID3D11Buffer* buffers[] = {buffer_.Get()};
  context->IASetVertexBuffers(slot, 1, buffers, &stride, &offset);
}

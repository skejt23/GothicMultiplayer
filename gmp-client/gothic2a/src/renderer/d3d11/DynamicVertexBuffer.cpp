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

#include "DynamicVertexBuffer.h"

#include <spdlog/spdlog.h>

#include <cassert>

#include "D3D11MapTracker.h"

namespace gmp::renderer::d3d11 {

//------------------------------------------------------------------------------
// DynamicVertexBuffer Implementation
//------------------------------------------------------------------------------

bool DynamicVertexBuffer::Init(ID3D11Device* device, uint32_t sizeInBytes, uint32_t vertexStride) {
  if (!device || sizeInBytes == 0 || vertexStride == 0) {
    SPDLOG_ERROR("DynamicVertexBuffer::Init - Invalid parameters");
    return false;
  }

  Release();

  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = sizeInBytes;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;
  desc.StructureByteStride = 0;

  HRESULT hr = device->CreateBuffer(&desc, nullptr, buffer_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("DynamicVertexBuffer::Init - CreateBuffer failed: 0x{:08X}", static_cast<uint32_t>(hr));
    return false;
  }

  bufferSize_ = sizeInBytes;
  stride_ = vertexStride;
  writeOffset_ = 0;

  return true;
}

void DynamicVertexBuffer::Release() {
  if (isMapped_) {
    SPDLOG_WARN("DynamicVertexBuffer::Release called while buffer is mapped!");
  }
  buffer_.Reset();
  bufferSize_ = 0;
  stride_ = 0;
  writeOffset_ = 0;
  mappedData_ = nullptr;
  isMapped_ = false;
}

void* DynamicVertexBuffer::Lock(ID3D11DeviceContext* context, uint32_t vertexCount, uint32_t& outStartVertex, bool discard) {
  if (!context || !buffer_ || vertexCount == 0) {
    return nullptr;
  }

  if (isMapped_) {
    SPDLOG_WARN("DynamicVertexBuffer::Lock called while already mapped!");
    return nullptr;
  }

  uint32_t requiredBytes = vertexCount * stride_;

  // Check if we need to wrap around or discard
  bool needDiscard = discard || (writeOffset_ + requiredBytes > bufferSize_);

  if (needDiscard) {
    writeOffset_ = 0;
  }

  // Double-check we have space
  if (writeOffset_ + requiredBytes > bufferSize_) {
    SPDLOG_ERROR("DynamicVertexBuffer::Lock - Buffer overflow: need {} bytes at offset {}, buffer size {}", requiredBytes, writeOffset_, bufferSize_);
    return nullptr;
  }

  D3D11_MAP mapType = needDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAPPED_SUBRESOURCE mapped = {};

  HRESULT hr = TrackedMap(context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, mapType, 0, &mapped, "DynamicVertexBuffer::Lock");
  if (FAILED(hr)) {
    SPDLOG_ERROR("DynamicVertexBuffer::Lock - Map failed: 0x{:08X}", static_cast<uint32_t>(hr));
    return nullptr;
  }

  mappedData_ = mapped.pData;
  isMapped_ = true;

  // Calculate output values
  outStartVertex = writeOffset_ / stride_;
  void* writePtr = static_cast<uint8_t*>(mappedData_) + writeOffset_;

  // Advance write position
  writeOffset_ += requiredBytes;

  return writePtr;
}

void DynamicVertexBuffer::Unlock(ID3D11DeviceContext* context) {
  if (!isMapped_) {
    return;
  }

  // If context or buffer is null, we cannot properly unmap, but we must reset state
  // to prevent a subsequent Unlock from trying to unmap an unmapped buffer.
  if (!context || !buffer_) {
    mappedData_ = nullptr;
    isMapped_ = false;
    return;
  }

  TrackedUnmap(context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, "DynamicVertexBuffer::Unlock");
  mappedData_ = nullptr;
  isMapped_ = false;
}

void DynamicVertexBuffer::Bind(ID3D11DeviceContext* context, uint32_t slot) const {
  if (!context || !buffer_) {
    return;
  }

  UINT offset = 0;
  ID3D11Buffer* buffers[] = {buffer_.Get()};
  context->IASetVertexBuffers(slot, 1, buffers, &stride_, &offset);
}

//------------------------------------------------------------------------------
// DynamicIndexBuffer Implementation
//------------------------------------------------------------------------------

bool DynamicIndexBuffer::Init(ID3D11Device* device, uint32_t sizeInBytes, bool use16Bit) {
  if (!device || sizeInBytes == 0) {
    SPDLOG_ERROR("DynamicIndexBuffer::Init - Invalid parameters");
    return false;
  }

  Release();

  D3D11_BUFFER_DESC desc = {};
  desc.ByteWidth = sizeInBytes;
  desc.Usage = D3D11_USAGE_DYNAMIC;
  desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  desc.MiscFlags = 0;
  desc.StructureByteStride = 0;

  HRESULT hr = device->CreateBuffer(&desc, nullptr, buffer_.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("DynamicIndexBuffer::Init - CreateBuffer failed: 0x{:08X}", static_cast<uint32_t>(hr));
    return false;
  }

  bufferSize_ = sizeInBytes;
  indexSize_ = use16Bit ? 2 : 4;
  format_ = use16Bit ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
  writeOffset_ = 0;

  return true;
}

void DynamicIndexBuffer::Release() {
  if (isMapped_) {
    SPDLOG_WARN("DynamicIndexBuffer::Release called while buffer is mapped!");
  }
  buffer_.Reset();
  bufferSize_ = 0;
  indexSize_ = 2;
  writeOffset_ = 0;
  mappedData_ = nullptr;
  isMapped_ = false;
  format_ = DXGI_FORMAT_R16_UINT;
}

void* DynamicIndexBuffer::Lock(ID3D11DeviceContext* context, uint32_t indexCount, uint32_t& outStartIndex, bool discard) {
  if (!context || !buffer_ || indexCount == 0) {
    return nullptr;
  }

  if (isMapped_) {
    SPDLOG_WARN("DynamicIndexBuffer::Lock called while already mapped!");
    return nullptr;
  }

  uint32_t requiredBytes = indexCount * indexSize_;

  // Check if we need to wrap around or discard
  bool needDiscard = discard || (writeOffset_ + requiredBytes > bufferSize_);

  if (needDiscard) {
    writeOffset_ = 0;
  }

  // Double-check we have space
  if (writeOffset_ + requiredBytes > bufferSize_) {
    SPDLOG_ERROR("DynamicIndexBuffer::Lock - Buffer overflow: need {} bytes at offset {}, buffer size {}", requiredBytes, writeOffset_, bufferSize_);
    return nullptr;
  }

  D3D11_MAP mapType = needDiscard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
  D3D11_MAPPED_SUBRESOURCE mapped = {};

  HRESULT hr = TrackedMap(context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, mapType, 0, &mapped, "DynamicIndexBuffer::Lock");
  if (FAILED(hr)) {
    SPDLOG_ERROR("DynamicIndexBuffer::Lock - Map failed: 0x{:08X}", static_cast<uint32_t>(hr));
    return nullptr;
  }

  mappedData_ = mapped.pData;
  isMapped_ = true;

  // Calculate output values
  outStartIndex = writeOffset_ / indexSize_;
  void* writePtr = static_cast<uint8_t*>(mappedData_) + writeOffset_;

  // Advance write position
  writeOffset_ += requiredBytes;

  return writePtr;
}

void DynamicIndexBuffer::Unlock(ID3D11DeviceContext* context) {
  if (!isMapped_) {
    return;
  }

  // If context or buffer is null, we cannot properly unmap, but we must reset state
  // to prevent a subsequent Unlock from trying to unmap an unmapped buffer.
  if (!context || !buffer_) {
    mappedData_ = nullptr;
    isMapped_ = false;
    return;
  }

  TrackedUnmap(context, static_cast<ID3D11Resource*>(buffer_.Get()), 0, "DynamicIndexBuffer::Unlock");
  mappedData_ = nullptr;
  isMapped_ = false;
}

void DynamicIndexBuffer::Bind(ID3D11DeviceContext* context) const {
  if (!context || !buffer_) {
    return;
  }

  context->IASetIndexBuffer(buffer_.Get(), format_, 0);
}

}  // namespace gmp::renderer::d3d11

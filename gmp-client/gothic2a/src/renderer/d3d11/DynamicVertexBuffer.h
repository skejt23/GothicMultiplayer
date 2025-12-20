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

// Ring-buffer style dynamic vertex/index buffers for efficient streaming geometry

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace gmp::renderer::d3d11 {

// Forward declarations
struct D3D11RendererImpl;

//------------------------------------------------------------------------------
// DynamicVertexBuffer - Ring-buffer for streaming vertex data
//------------------------------------------------------------------------------
class DynamicVertexBuffer {
public:
  DynamicVertexBuffer() = default;
  ~DynamicVertexBuffer() {
    Release();
  }

  // Non-copyable
  DynamicVertexBuffer(const DynamicVertexBuffer&) = delete;
  DynamicVertexBuffer& operator=(const DynamicVertexBuffer&) = delete;

  // Move semantics
  DynamicVertexBuffer(DynamicVertexBuffer&& other) noexcept
      : bufferSize_(other.bufferSize_),
        stride_(other.stride_),
        writeOffset_(other.writeOffset_),
        mappedData_(other.mappedData_),
        isMapped_(other.isMapped_) {
    buffer_.Swap(other.buffer_);
    other.bufferSize_ = 0;
    other.stride_ = 0;
    other.writeOffset_ = 0;
    other.mappedData_ = nullptr;
    other.isMapped_ = false;
  }

  DynamicVertexBuffer& operator=(DynamicVertexBuffer&& other) noexcept {
    if (this != &other) {
      Release();
      buffer_.Swap(other.buffer_);
      bufferSize_ = other.bufferSize_;
      stride_ = other.stride_;
      writeOffset_ = other.writeOffset_;
      mappedData_ = other.mappedData_;
      isMapped_ = other.isMapped_;
      other.bufferSize_ = 0;
      other.stride_ = 0;
      other.writeOffset_ = 0;
      other.mappedData_ = nullptr;
      other.isMapped_ = false;
    }
    return *this;
  }

  // Initialize the buffer
  bool Init(ID3D11Device* device, uint32_t sizeInBytes, uint32_t vertexStride);

  // Release resources
  void Release();

  // Lock a portion of the buffer for writing
  // Returns pointer to write vertices, or nullptr on failure
  // If discard is true, the entire buffer is discarded (use when starting a new frame)
  // vertexCount: number of vertices to allocate
  // outStartVertex: receives the starting vertex index for the draw call
  void* Lock(ID3D11DeviceContext* context, uint32_t vertexCount, uint32_t& outStartVertex, bool discard = false);

  // Unlock the buffer after writing
  void Unlock(ID3D11DeviceContext* context);

  // Bind this buffer for rendering
  void Bind(ID3D11DeviceContext* context, uint32_t slot = 0) const;

  // Accessors
  ID3D11Buffer* GetBuffer() const {
    return buffer_.Get();
  }
  uint32_t GetBufferSize() const {
    return bufferSize_;
  }
  uint32_t GetStride() const {
    return stride_;
  }
  uint32_t GetCurrentOffset() const {
    return writeOffset_;
  }
  bool IsValid() const {
    return buffer_ != nullptr;
  }
  bool IsMapped() const {
    return isMapped_;
  }

  // Reset write position (call at frame start with discard)
  void Reset() {
    writeOffset_ = 0;
  }

private:
  ComPtr<ID3D11Buffer> buffer_;
  uint32_t bufferSize_ = 0;
  uint32_t stride_ = 0;
  uint32_t writeOffset_ = 0;    // Current write position in bytes
  void* mappedData_ = nullptr;  // Pointer to mapped memory
  bool isMapped_ = false;
};

//------------------------------------------------------------------------------
// DynamicIndexBuffer - Ring-buffer for streaming index data
//------------------------------------------------------------------------------
class DynamicIndexBuffer {
public:
  DynamicIndexBuffer() = default;
  ~DynamicIndexBuffer() {
    Release();
  }

  // Non-copyable
  DynamicIndexBuffer(const DynamicIndexBuffer&) = delete;
  DynamicIndexBuffer& operator=(const DynamicIndexBuffer&) = delete;

  // Move semantics
  DynamicIndexBuffer(DynamicIndexBuffer&& other) noexcept
      : bufferSize_(other.bufferSize_),
        indexSize_(other.indexSize_),
        writeOffset_(other.writeOffset_),
        mappedData_(other.mappedData_),
        isMapped_(other.isMapped_),
        format_(other.format_) {
    buffer_.Swap(other.buffer_);
    other.bufferSize_ = 0;
    other.indexSize_ = 0;
    other.writeOffset_ = 0;
    other.mappedData_ = nullptr;
    other.isMapped_ = false;
  }

  DynamicIndexBuffer& operator=(DynamicIndexBuffer&& other) noexcept {
    if (this != &other) {
      Release();
      buffer_.Swap(other.buffer_);
      bufferSize_ = other.bufferSize_;
      indexSize_ = other.indexSize_;
      writeOffset_ = other.writeOffset_;
      mappedData_ = other.mappedData_;
      isMapped_ = other.isMapped_;
      format_ = other.format_;
      other.bufferSize_ = 0;
      other.indexSize_ = 0;
      other.writeOffset_ = 0;
      other.mappedData_ = nullptr;
      other.isMapped_ = false;
    }
    return *this;
  }

  // Initialize the buffer (use16Bit: true for 16-bit indices, false for 32-bit)
  bool Init(ID3D11Device* device, uint32_t sizeInBytes, bool use16Bit = true);

  // Release resources
  void Release();

  // Lock a portion of the buffer for writing
  // indexCount: number of indices to allocate
  // outStartIndex: receives the starting index for the draw call
  void* Lock(ID3D11DeviceContext* context, uint32_t indexCount, uint32_t& outStartIndex, bool discard = false);

  // Unlock the buffer after writing
  void Unlock(ID3D11DeviceContext* context);

  // Bind this buffer for rendering
  void Bind(ID3D11DeviceContext* context) const;

  // Accessors
  ID3D11Buffer* GetBuffer() const {
    return buffer_.Get();
  }
  uint32_t GetBufferSize() const {
    return bufferSize_;
  }
  uint32_t GetIndexSize() const {
    return indexSize_;
  }
  DXGI_FORMAT GetFormat() const {
    return format_;
  }
  uint32_t GetCurrentOffset() const {
    return writeOffset_;
  }
  bool IsValid() const {
    return buffer_ != nullptr;
  }
  bool IsMapped() const {
    return isMapped_;
  }

  // Reset write position
  void Reset() {
    writeOffset_ = 0;
  }

private:
  ComPtr<ID3D11Buffer> buffer_;
  uint32_t bufferSize_ = 0;
  uint32_t indexSize_ = 2;    // 2 for 16-bit, 4 for 32-bit
  uint32_t writeOffset_ = 0;  // Current write position in bytes
  void* mappedData_ = nullptr;
  bool isMapped_ = false;
  DXGI_FORMAT format_ = DXGI_FORMAT_R16_UINT;
};

//------------------------------------------------------------------------------
// Convenience constants
//------------------------------------------------------------------------------
constexpr uint32_t DEFAULT_DYNAMIC_VB_SIZE = 4 * 1024 * 1024;  // 4 MB
constexpr uint32_t DEFAULT_DYNAMIC_IB_SIZE = 2 * 1024 * 1024;  // 2 MB

}  // namespace gmp::renderer::d3d11

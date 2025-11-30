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

#include <d3d9.h>

#include <cstddef>
#include <cstdint>

namespace gmp::renderer {

// ----------------------------------------------------------------------------
// Dynamic Vertex Buffer - Ring Buffer Pattern
// ----------------------------------------------------------------------------
// This class implements efficient dynamic vertex buffer management using the
// classic "ring buffer" or "circular buffer" pattern for D3D9.
//
// The Problem:
// Dynamic vertex data (particles, UI, batched geometry) changes every frame.
// Naive approaches cause GPU stalls:
// - DrawPrimitiveUP: Driver allocates temporary memory per call (slow)
// - Lock with D3DLOCK_DISCARD every call: GPU must finish all pending draws
//   before buffer can be orphaned and reallocated
//
// The Solution - Ring Buffer:
// Allocate a large buffer and write to different regions each frame:
//
// Frame 1: [====DATA====|------------------|------------------|]
//           ^write                          GPU reading here
// Frame 2: [------------|====DATA====|-----|------------------|]
//                        ^write             GPU reading Frame 1
// Frame 3: [------------|------------|====DATA====|-----------|]
//                                     ^write
//
// When the buffer is full (or close to wrapping), we use D3DLOCK_DISCARD to
// orphan the old buffer and start fresh. Otherwise, D3DLOCK_NOOVERWRITE tells
// the driver we won't touch regions that may still be in use by the GPU.
//
// Lock Flags:
// - D3DLOCK_NOOVERWRITE: "I won't modify data the GPU is using" - no stall
// - D3DLOCK_DISCARD: "Orphan this buffer, give me a fresh one" - minimal stall
//
// Performance:
// This pattern achieves near-optimal GPU/CPU parallelism by never forcing
// the GPU to wait for the CPU or vice versa.
// ----------------------------------------------------------------------------

class DynamicVertexBuffer {
public:
  DynamicVertexBuffer() = default;
  ~DynamicVertexBuffer();

  // Non-copyable, movable
  DynamicVertexBuffer(const DynamicVertexBuffer&) = delete;
  DynamicVertexBuffer& operator=(const DynamicVertexBuffer&) = delete;
  DynamicVertexBuffer(DynamicVertexBuffer&& other) noexcept;
  DynamicVertexBuffer& operator=(DynamicVertexBuffer&& other) noexcept;

  // Initialize the buffer with given capacity and vertex stride.
  // Returns true on success.
  bool Create(IDirect3DDevice9* device, size_t capacity_bytes, DWORD fvf);

  // Release the D3D9 buffer. Call before device reset.
  void Release();

  // Recreate after device reset.
  bool Recreate(IDirect3DDevice9* device);

  // Allocate space for vertex data and return a write pointer.
  // vertex_count: Number of vertices to allocate
  // stride: Bytes per vertex
  // out_offset: Receives the byte offset in the buffer for SetStreamSource
  // Returns nullptr if allocation fails.
  void* Allocate(size_t vertex_count, size_t stride, size_t& out_offset);

  // Must be called after writing data obtained from Allocate().
  void Unlock();

  // Reset write position to beginning (call at frame start if you want full reset).
  // This triggers a DISCARD on the next Allocate().
  void Reset();

  // Begin a new frame - resets if we've wrapped around
  void BeginFrame();

  // Accessors
  [[nodiscard]] IDirect3DVertexBuffer9* GetBuffer() const {
    return buffer_;
  }
  [[nodiscard]] size_t GetCapacity() const {
    return capacity_;
  }
  [[nodiscard]] size_t GetUsedBytes() const {
    return write_offset_;
  }
  [[nodiscard]] DWORD GetFVF() const {
    return fvf_;
  }
  [[nodiscard]] bool IsValid() const {
    return buffer_ != nullptr;
  }

  // Statistics
  [[nodiscard]] size_t GetDiscardCount() const {
    return discard_count_;
  }
  [[nodiscard]] size_t GetNoOverwriteCount() const {
    return nooverwrite_count_;
  }
  void ResetStats() {
    discard_count_ = 0;
    nooverwrite_count_ = 0;
  }

private:
  IDirect3DDevice9* device_ = nullptr;
  IDirect3DVertexBuffer9* buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t write_offset_ = 0;
  DWORD fvf_ = 0;
  bool locked_ = false;
  bool need_discard_ = true;  // First allocation always discards

  // Statistics
  size_t discard_count_ = 0;
  size_t nooverwrite_count_ = 0;
};

// ----------------------------------------------------------------------------
// Dynamic Index Buffer - Same Ring Buffer Pattern
// ----------------------------------------------------------------------------
// Identical pattern for index data. Kept as separate class for type safety
// and because index buffers have different D3D9 creation parameters.
// ----------------------------------------------------------------------------

class DynamicIndexBuffer {
public:
  DynamicIndexBuffer() = default;
  ~DynamicIndexBuffer();

  // Non-copyable, movable
  DynamicIndexBuffer(const DynamicIndexBuffer&) = delete;
  DynamicIndexBuffer& operator=(const DynamicIndexBuffer&) = delete;
  DynamicIndexBuffer(DynamicIndexBuffer&& other) noexcept;
  DynamicIndexBuffer& operator=(DynamicIndexBuffer&& other) noexcept;

  // Initialize the buffer with given capacity (in indices, not bytes).
  bool Create(IDirect3DDevice9* device, size_t capacity_indices);

  // Release the D3D9 buffer.
  void Release();

  // Recreate after device reset.
  bool Recreate(IDirect3DDevice9* device);

  // Allocate space for index data.
  // index_count: Number of indices to allocate
  // out_start_index: Receives the starting index for DrawIndexedPrimitive
  // Returns nullptr if allocation fails.
  uint16_t* Allocate(size_t index_count, size_t& out_start_index);

  // Must be called after writing.
  void Unlock();

  // Reset for new frame.
  void Reset();
  void BeginFrame();

  // Accessors
  [[nodiscard]] IDirect3DIndexBuffer9* GetBuffer() const {
    return buffer_;
  }
  [[nodiscard]] size_t GetCapacity() const {
    return capacity_;
  }
  [[nodiscard]] size_t GetUsedIndices() const {
    return write_offset_;
  }
  [[nodiscard]] bool IsValid() const {
    return buffer_ != nullptr;
  }

  // Statistics
  [[nodiscard]] size_t GetDiscardCount() const {
    return discard_count_;
  }
  [[nodiscard]] size_t GetNoOverwriteCount() const {
    return nooverwrite_count_;
  }
  void ResetStats() {
    discard_count_ = 0;
    nooverwrite_count_ = 0;
  }

private:
  IDirect3DDevice9* device_ = nullptr;
  IDirect3DIndexBuffer9* buffer_ = nullptr;
  size_t capacity_ = 0;      // In indices
  size_t write_offset_ = 0;  // In indices
  bool locked_ = false;
  bool need_discard_ = true;

  size_t discard_count_ = 0;
  size_t nooverwrite_count_ = 0;
};

}  // namespace gmp::renderer

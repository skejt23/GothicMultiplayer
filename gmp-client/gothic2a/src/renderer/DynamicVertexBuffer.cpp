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

#include <cstring>
#include <utility>

namespace gmp::renderer {

// ----------------------------------------------------------------------------
// DynamicVertexBuffer Implementation
// ----------------------------------------------------------------------------

DynamicVertexBuffer::~DynamicVertexBuffer() {
  Release();
}

DynamicVertexBuffer::DynamicVertexBuffer(DynamicVertexBuffer&& other) noexcept
    : device_(other.device_),
      buffer_(other.buffer_),
      capacity_(other.capacity_),
      write_offset_(other.write_offset_),
      fvf_(other.fvf_),
      locked_(other.locked_),
      need_discard_(other.need_discard_),
      discard_count_(other.discard_count_),
      nooverwrite_count_(other.nooverwrite_count_) {
  other.buffer_ = nullptr;
  other.device_ = nullptr;
}

DynamicVertexBuffer& DynamicVertexBuffer::operator=(DynamicVertexBuffer&& other) noexcept {
  if (this != &other) {
    Release();
    device_ = other.device_;
    buffer_ = other.buffer_;
    capacity_ = other.capacity_;
    write_offset_ = other.write_offset_;
    fvf_ = other.fvf_;
    locked_ = other.locked_;
    need_discard_ = other.need_discard_;
    discard_count_ = other.discard_count_;
    nooverwrite_count_ = other.nooverwrite_count_;
    other.buffer_ = nullptr;
    other.device_ = nullptr;
  }
  return *this;
}

bool DynamicVertexBuffer::Create(IDirect3DDevice9* device, size_t capacity_bytes, DWORD fvf) {
  if (!device || capacity_bytes == 0) {
    return false;
  }

  Release();

  device_ = device;
  capacity_ = capacity_bytes;
  fvf_ = fvf;

  HRESULT hr =
      device_->CreateVertexBuffer(static_cast<UINT>(capacity_bytes), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, fvf, D3DPOOL_DEFAULT, &buffer_, nullptr);

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create dynamic vertex buffer ({} bytes): {:#x}", capacity_bytes, static_cast<unsigned long>(hr));
    return false;
  }

  SPDLOG_DEBUG("Created dynamic vertex buffer: {} KB", capacity_bytes / 1024);
  return true;
}

void DynamicVertexBuffer::Release() {
  if (locked_ && buffer_) {
    buffer_->Unlock();
    locked_ = false;
  }
  if (buffer_) {
    buffer_->Release();
    buffer_ = nullptr;
  }
  capacity_ = 0;
  write_offset_ = 0;
  need_discard_ = true;
}

bool DynamicVertexBuffer::Recreate(IDirect3DDevice9* device) {
  if (!device || capacity_ == 0) {
    return false;
  }

  // Store old values
  size_t old_capacity = capacity_;
  DWORD old_fvf = fvf_;

  // Release old buffer (D3DPOOL_DEFAULT is lost on reset)
  if (buffer_) {
    buffer_->Release();
    buffer_ = nullptr;
  }

  device_ = device;
  write_offset_ = 0;
  need_discard_ = true;
  locked_ = false;

  HRESULT hr = device_->CreateVertexBuffer(static_cast<UINT>(old_capacity), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, old_fvf, D3DPOOL_DEFAULT, &buffer_,
                                           nullptr);

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to recreate dynamic vertex buffer: {:#x}", static_cast<unsigned long>(hr));
    return false;
  }

  return true;
}

void* DynamicVertexBuffer::Allocate(size_t vertex_count, size_t stride, size_t& out_offset) {
  if (!buffer_ || vertex_count == 0 || stride == 0) {
    return nullptr;
  }

  if (locked_) {
    SPDLOG_WARN("DynamicVertexBuffer::Allocate called while still locked");
    Unlock();
  }

  const size_t required_bytes = vertex_count * stride;

  // Check if we have room, or need to wrap around (discard)
  if (write_offset_ + required_bytes > capacity_) {
    need_discard_ = true;
    write_offset_ = 0;
  }

  // Choose lock flags based on whether we're discarding
  DWORD lock_flags = D3DLOCK_NOSYSLOCK;
  if (need_discard_) {
    lock_flags |= D3DLOCK_DISCARD;
    ++discard_count_;
    need_discard_ = false;
  } else {
    lock_flags |= D3DLOCK_NOOVERWRITE;
    ++nooverwrite_count_;
  }

  void* data = nullptr;
  HRESULT hr = buffer_->Lock(static_cast<UINT>(write_offset_), static_cast<UINT>(required_bytes), &data, lock_flags);

  if (FAILED(hr) || !data) {
    SPDLOG_ERROR("Failed to lock dynamic vertex buffer: {:#x}", static_cast<unsigned long>(hr));
    return nullptr;
  }

  out_offset = write_offset_;
  write_offset_ += required_bytes;
  locked_ = true;

  return data;
}

void DynamicVertexBuffer::Unlock() {
  if (locked_ && buffer_) {
    buffer_->Unlock();
    locked_ = false;
  }
}

void DynamicVertexBuffer::Reset() {
  if (locked_) {
    Unlock();
  }
  write_offset_ = 0;
  need_discard_ = true;
}

void DynamicVertexBuffer::BeginFrame() {
  // At frame start, if we've used a significant portion of the buffer,
  // reset to allow DISCARD. Otherwise, continue with NOOVERWRITE.
  // Threshold: 75% usage triggers reset
  if (write_offset_ > (capacity_ * 3) / 4) {
    Reset();
  }
}

// ----------------------------------------------------------------------------
// DynamicIndexBuffer Implementation
// ----------------------------------------------------------------------------

DynamicIndexBuffer::~DynamicIndexBuffer() {
  Release();
}

DynamicIndexBuffer::DynamicIndexBuffer(DynamicIndexBuffer&& other) noexcept
    : device_(other.device_),
      buffer_(other.buffer_),
      capacity_(other.capacity_),
      write_offset_(other.write_offset_),
      locked_(other.locked_),
      need_discard_(other.need_discard_),
      discard_count_(other.discard_count_),
      nooverwrite_count_(other.nooverwrite_count_) {
  other.buffer_ = nullptr;
  other.device_ = nullptr;
}

DynamicIndexBuffer& DynamicIndexBuffer::operator=(DynamicIndexBuffer&& other) noexcept {
  if (this != &other) {
    Release();
    device_ = other.device_;
    buffer_ = other.buffer_;
    capacity_ = other.capacity_;
    write_offset_ = other.write_offset_;
    locked_ = other.locked_;
    need_discard_ = other.need_discard_;
    discard_count_ = other.discard_count_;
    nooverwrite_count_ = other.nooverwrite_count_;
    other.buffer_ = nullptr;
    other.device_ = nullptr;
  }
  return *this;
}

bool DynamicIndexBuffer::Create(IDirect3DDevice9* device, size_t capacity_indices) {
  if (!device || capacity_indices == 0) {
    return false;
  }

  Release();

  device_ = device;
  capacity_ = capacity_indices;

  const size_t size_bytes = capacity_indices * sizeof(uint16_t);

  HRESULT hr = device_->CreateIndexBuffer(static_cast<UINT>(size_bytes), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          &buffer_, nullptr);

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to create dynamic index buffer ({} indices): {:#x}", capacity_indices, static_cast<unsigned long>(hr));
    return false;
  }

  SPDLOG_DEBUG("Created dynamic index buffer: {} indices ({} KB)", capacity_indices, size_bytes / 1024);
  return true;
}

void DynamicIndexBuffer::Release() {
  if (locked_ && buffer_) {
    buffer_->Unlock();
    locked_ = false;
  }
  if (buffer_) {
    buffer_->Release();
    buffer_ = nullptr;
  }
  capacity_ = 0;
  write_offset_ = 0;
  need_discard_ = true;
}

bool DynamicIndexBuffer::Recreate(IDirect3DDevice9* device) {
  if (!device || capacity_ == 0) {
    return false;
  }

  size_t old_capacity = capacity_;

  if (buffer_) {
    buffer_->Release();
    buffer_ = nullptr;
  }

  device_ = device;
  write_offset_ = 0;
  need_discard_ = true;
  locked_ = false;

  const size_t size_bytes = old_capacity * sizeof(uint16_t);

  HRESULT hr = device_->CreateIndexBuffer(static_cast<UINT>(size_bytes), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          &buffer_, nullptr);

  if (FAILED(hr)) {
    SPDLOG_ERROR("Failed to recreate dynamic index buffer: {:#x}", static_cast<unsigned long>(hr));
    return false;
  }

  return true;
}

uint16_t* DynamicIndexBuffer::Allocate(size_t index_count, size_t& out_start_index) {
  if (!buffer_ || index_count == 0) {
    return nullptr;
  }

  if (locked_) {
    SPDLOG_WARN("DynamicIndexBuffer::Allocate called while still locked");
    Unlock();
  }

  // Check if we have room, or need to wrap around
  if (write_offset_ + index_count > capacity_) {
    need_discard_ = true;
    write_offset_ = 0;
  }

  DWORD lock_flags = D3DLOCK_NOSYSLOCK;
  if (need_discard_) {
    lock_flags |= D3DLOCK_DISCARD;
    ++discard_count_;
    need_discard_ = false;
  } else {
    lock_flags |= D3DLOCK_NOOVERWRITE;
    ++nooverwrite_count_;
  }

  const size_t offset_bytes = write_offset_ * sizeof(uint16_t);
  const size_t size_bytes = index_count * sizeof(uint16_t);

  void* data = nullptr;
  HRESULT hr = buffer_->Lock(static_cast<UINT>(offset_bytes), static_cast<UINT>(size_bytes), &data, lock_flags);

  if (FAILED(hr) || !data) {
    SPDLOG_ERROR("Failed to lock dynamic index buffer: {:#x}", static_cast<unsigned long>(hr));
    return nullptr;
  }

  out_start_index = write_offset_;
  write_offset_ += index_count;
  locked_ = true;

  return static_cast<uint16_t*>(data);
}

void DynamicIndexBuffer::Unlock() {
  if (locked_ && buffer_) {
    buffer_->Unlock();
    locked_ = false;
  }
}

void DynamicIndexBuffer::Reset() {
  if (locked_) {
    Unlock();
  }
  write_offset_ = 0;
  need_discard_ = true;
}

void DynamicIndexBuffer::BeginFrame() {
  if (write_offset_ > (capacity_ * 3) / 4) {
    Reset();
  }
}

}  // namespace gmp::renderer

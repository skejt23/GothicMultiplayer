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

#include "AlphaPoly.h"

#include <cmath>

namespace gmp::renderer::d3d11 {

AlphaPolyQueue::AlphaPolyQueue() : alloc_count_(0), bucket_size_(1.0f) {
  pool_.resize(kAlphaPolyPoolSize);
  Reset();
}

void AlphaPolyQueue::Reset() {
  alloc_count_ = 0;
  std::fill(buckets_.begin(), buckets_.end(), nullptr);
}

QueuedAlphaPoly* AlphaPolyQueue::Allocate() {
  if (alloc_count_ >= kAlphaPolyPoolSize) {
    return nullptr;
  }

  QueuedAlphaPoly* poly = &pool_[alloc_count_++];
  poly->vert_count = 0;
  poly->texture = nullptr;
  poly->blend_func = AlphaBlendFunc::kBlend;
  poly->z_func = ZBufferCmp::kLess;
  poly->z_bias = 0;
  poly->texture_wrap = true;
  poly->texture_has_alpha = false;
  poly->z_value = 0.0f;
  poly->next = nullptr;

  return poly;
}

void AlphaPolyQueue::Submit(QueuedAlphaPoly* poly) {
  if (!poly) {
    return;
  }

  // Calculate average Z for sorting
  float z_sum = 0.0f;
  for (int i = 0; i < poly->vert_count; ++i) {
    z_sum += poly->verts[i].z;
  }
  poly->z_value = z_sum / static_cast<float>(poly->vert_count);
  if (!std::isfinite(poly->z_value)) {
    poly->z_value = 0.0f;
  }

  // Add to bucket list with sorted insertion (back-to-front within bucket).
  // Larger z_value means farther away in the RHW/clip-space we use for alpha polys.
  const int bucket_idx = ComputeBucket(poly->z_value);
  QueuedAlphaPoly*& head = buckets_[bucket_idx];

  if (head == nullptr || head->z_value <= poly->z_value) {
    poly->next = head;
    head = poly;
    return;
  }

  QueuedAlphaPoly* entry = head;
  while (entry->next != nullptr && entry->next->z_value > poly->z_value) {
    entry = entry->next;
  }
  poly->next = entry->next;
  entry->next = poly;
}

void AlphaPolyQueue::SetFarClipZ(float far_clip_z) {
  bucket_size_ = far_clip_z / static_cast<float>(kAlphaSortBuckets);
}

int AlphaPolyQueue::ComputeBucket(float z_value) const {
  if (bucket_size_ <= 0.0f) {
    return 0;
  }

  const int bucket = static_cast<int>(z_value / bucket_size_);
  if (bucket < 0) {
    return 0;
  }
  if (bucket >= kAlphaSortBuckets) {
    return kAlphaSortBuckets - 1;
  }
  return bucket;
}

// ----------------------------------------------------------------------------
// AlphaPolyBatcher
// ----------------------------------------------------------------------------

AlphaPolyBatcher::AlphaPolyBatcher() {
  vertices_.reserve(kAlphaBatchMaxVertices);
  indices_.reserve(kAlphaBatchMaxIndices);
}

void AlphaPolyBatcher::Begin() {
  vertex_count_ = 0;
  index_count_ = 0;
  batch_active_ = true;
  current_state_ = {};
  stats_.Reset();
}

bool AlphaPolyBatcher::Submit(const QueuedAlphaPoly& poly) {
  stats_.polys_submitted++;

  // Check if we can batch this polygon
  if (!CanBatch(poly)) {
    return false;  // Caller should flush and retry
  }

  // Add vertices and indices
  AddTriangleFan(poly.verts.data(), poly.vert_count);

  stats_.polys_batched++;
  return true;
}

size_t AlphaPolyBatcher::GetBatchData(const AlphaVertex*& verts, const uint16_t*& inds, AlphaRenderStateKey& state) const {
  if (index_count_ == 0) {
    verts = nullptr;
    inds = nullptr;
    state = {};
    return 0;
  }

  verts = vertices_.data();
  inds = indices_.data();
  state = current_state_;
  return index_count_;
}

void AlphaPolyBatcher::MarkBatchRendered() {
  stats_.batches_flushed++;
  stats_.vertices_rendered += vertex_count_;
  stats_.indices_rendered += index_count_;
  vertex_count_ = 0;
  index_count_ = 0;
}

bool AlphaPolyBatcher::End() {
  batch_active_ = false;

  if (index_count_ > 0) {
    stats_.draw_calls_saved = stats_.polys_batched - stats_.batches_flushed;
    return true;  // Caller should flush final batch
  }

  return false;
}

void AlphaPolyBatcher::Flush() {
  if (index_count_ > 0) {
    MarkBatchRendered();
  }
}

void AlphaPolyBatcher::AddTriangleFan(const AlphaVertex* verts, int vert_count) {
  if (vert_count < 3) {
    return;
  }

  // Convert triangle fan to indexed triangles
  const size_t num_tris = static_cast<size_t>(vert_count - 2);
  const size_t num_indices = num_tris * 3;

  // Add vertices
  const size_t base_vertex = vertex_count_;
  vertices_.resize(vertex_count_ + vert_count);
  for (int i = 0; i < vert_count; ++i) {
    vertices_[vertex_count_++] = verts[i];
  }

  // Add indices (triangle fan: 0-1-2, 0-2-3, 0-3-4, ...)
  indices_.resize(index_count_ + num_indices);
  for (size_t tri = 0; tri < num_tris; ++tri) {
    indices_[index_count_++] = static_cast<uint16_t>(base_vertex);
    indices_[index_count_++] = static_cast<uint16_t>(base_vertex + tri + 1);
    indices_[index_count_++] = static_cast<uint16_t>(base_vertex + tri + 2);
  }
}

bool AlphaPolyBatcher::CanBatch(const QueuedAlphaPoly& poly) const {
  // Check if we have room
  const size_t num_tris = static_cast<size_t>(poly.vert_count - 2);
  const size_t num_indices = num_tris * 3;

  if (vertex_count_ + poly.vert_count > kAlphaBatchMaxVertices) {
    return false;
  }
  if (index_count_ + num_indices > kAlphaBatchMaxIndices) {
    return false;
  }

  // If batch is empty, accept any polygon
  if (index_count_ == 0) {
    // Initialize state from this polygon
    const_cast<AlphaPolyBatcher*>(this)->current_state_ = AlphaRenderStateKey::FromPoly(poly);
    return true;
  }

  // Check if state matches current batch
  const AlphaRenderStateKey poly_state = AlphaRenderStateKey::FromPoly(poly);
  if (poly_state != current_state_) {
    const_cast<AlphaPolyBatcher*>(this)->stats_.state_changes++;
    return false;
  }

  return true;
}

}  // namespace gmp::renderer::d3d11

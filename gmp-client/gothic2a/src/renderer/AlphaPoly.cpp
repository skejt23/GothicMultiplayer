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

namespace gmp::renderer {

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

void AlphaPolyQueue::SetFarClipZ(float far_clip_z) {
  if (far_clip_z < 500.0f) {
    far_clip_z = 500.0f;
  }
  bucket_size_ = static_cast<float>(kAlphaSortBuckets) / far_clip_z;
}

int AlphaPolyQueue::ComputeBucket(float z_value) const {
  int bucket = static_cast<int>(bucket_size_ * z_value);
  if (bucket >= kAlphaSortBuckets) {
    bucket = kAlphaSortBuckets - 1;
  }
  if (bucket < 0) {
    bucket = 0;
  }
  return bucket;
}

void AlphaPolyQueue::Submit(QueuedAlphaPoly* poly) {
  if (!poly || poly->vert_count < 3) {
    return;
  }

  float z_value = poly->z_value;
  if (z_value == 0.0f) {
    return;
  }

  int index = ComputeBucket(z_value);

  // If bucket is empty, insert directly
  if (buckets_[index] == nullptr) {
    poly->next = nullptr;
    buckets_[index] = poly;
    return;
  }

  // Larger Z = farther from camera. For painter's algorithm, render farther first.
  // Sort so larger Z is at head of bucket list.
  // If first bucket entry has smaller Z (closer), insert new poly at head
  if (buckets_[index]->z_value <= z_value) {
    poly->next = buckets_[index];
    buckets_[index] = poly;
    return;
  }

  // Otherwise traverse the bucket to find correct sorted position (far to near)
  QueuedAlphaPoly* entry = buckets_[index];
  QueuedAlphaPoly* next_entry = entry->next;
  while (true) {
    if (next_entry == nullptr) {
      break;
    }
    if (next_entry->z_value <= z_value) {
      break;
    }
    entry = next_entry;
    next_entry = entry->next;
  }

  // Insert into the linked list at this position
  entry->next = poly;
  poly->next = next_entry;
}

// ----------------------------------------------------------------------------
// AlphaPolyBatcher Implementation
// ----------------------------------------------------------------------------

AlphaPolyBatcher::AlphaPolyBatcher() {
  vertices_.resize(kAlphaBatchMaxVertices);
  indices_.resize(kAlphaBatchMaxIndices);
  vertex_count_ = 0;
  index_count_ = 0;
  batch_active_ = false;
  current_state_ = {};
}

void AlphaPolyBatcher::Begin() {
  vertex_count_ = 0;
  index_count_ = 0;
  batch_active_ = false;
  current_state_ = {};
}

bool AlphaPolyBatcher::CanBatch(const QueuedAlphaPoly& poly) const {
  if (!batch_active_) {
    return true;  // First poly starts a new batch
  }

  // Check if render state matches current batch
  AlphaRenderStateKey poly_state = AlphaRenderStateKey::FromPoly(poly);
  if (poly_state != current_state_) {
    return false;  // State change - must flush
  }

  // Check if we have room for this polygon
  // Triangle fan with N verts = N-2 triangles = (N-2)*3 indices
  const size_t required_indices = (poly.vert_count - 2) * 3;
  if (vertex_count_ + poly.vert_count > kAlphaBatchMaxVertices || index_count_ + required_indices > kAlphaBatchMaxIndices) {
    return false;  // Buffer full - must flush
  }

  return true;
}

void AlphaPolyBatcher::AddTriangleFan(const AlphaVertex* verts, int vert_count) {
  if (vert_count < 3)
    return;

  // Record base vertex for indexing
  const auto base_vertex = static_cast<uint16_t>(vertex_count_);

  // Copy vertices
  for (int i = 0; i < vert_count; ++i) {
    vertices_[vertex_count_++] = verts[i];
  }

  // Convert triangle fan to indexed triangles
  // Fan vertex 0 is shared; triangles are (0,1,2), (0,2,3), (0,3,4), etc.
  const int num_triangles = vert_count - 2;
  for (int i = 0; i < num_triangles; ++i) {
    indices_[index_count_++] = base_vertex;
    indices_[index_count_++] = base_vertex + static_cast<uint16_t>(i + 1);
    indices_[index_count_++] = base_vertex + static_cast<uint16_t>(i + 2);
  }

  stats_.vertices_rendered += vert_count;
  stats_.indices_rendered += num_triangles * 3;
}

bool AlphaPolyBatcher::Submit(const QueuedAlphaPoly& poly) {
  if (poly.vert_count < 3) {
    return true;  // Nothing to do
  }

  stats_.polys_submitted++;

  // Check if this poly can be added to current batch
  if (!CanBatch(poly)) {
    // State or buffer change - caller needs to flush current batch first
    return false;
  }

  // Start new batch if needed
  if (!batch_active_) {
    current_state_ = AlphaRenderStateKey::FromPoly(poly);
    batch_active_ = true;
  }

  // Add polygon to batch
  AddTriangleFan(poly.verts.data(), poly.vert_count);
  stats_.polys_batched++;

  return true;
}

size_t AlphaPolyBatcher::GetBatchData(const AlphaVertex*& vertices_out, const uint16_t*& indices_out, AlphaRenderStateKey& state_out) const {
  if (index_count_ == 0) {
    vertices_out = nullptr;
    indices_out = nullptr;
    return 0;
  }

  vertices_out = vertices_.data();
  indices_out = indices_.data();
  state_out = current_state_;
  return index_count_;
}

void AlphaPolyBatcher::MarkBatchRendered() {
  if (index_count_ > 0) {
    stats_.batches_flushed++;
  }

  // Reset for next batch
  vertex_count_ = 0;
  index_count_ = 0;
  batch_active_ = false;
  current_state_ = {};
}

void AlphaPolyBatcher::Flush() {
  // This just marks the batch as needing to be rendered
  // Caller should call GetBatchData() then MarkBatchRendered()
}

bool AlphaPolyBatcher::End() {
  // Calculate saved draw calls
  if (stats_.polys_batched > stats_.batches_flushed) {
    stats_.draw_calls_saved = stats_.polys_batched - stats_.batches_flushed;
  }

  // Return true if there's a final batch to render
  return index_count_ > 0;
}

}  // namespace gmp::renderer

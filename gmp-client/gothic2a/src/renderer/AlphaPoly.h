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

#include <array>
#include <cstdint>
#include <vector>

namespace gmp::renderer {

inline constexpr int kAlphaPolyMaxVerts = 8;     // Max vertices per polygon.
inline constexpr int kAlphaPolyPoolSize = 8192;  // Max alpha polys per frame (4x original MAXALPHAPOLYS for modern systems)
inline constexpr int kAlphaSortBuckets = 512;    // Depth sort buckets (2x original for finer depth sorting)

// Alpha blend function (matches Gothic's zTRnd_AlphaBlendFunc)
enum class AlphaBlendFunc : uint8_t { kMatDefault = 0, kNone = 1, kBlend = 2, kAdd = 3, kSub = 4, kMul = 5, kMul2 = 6, kTest = 7, kBlendTest = 8 };

// Z buffer compare function (matches Gothic's zTRnd_ZBufferCmp)
enum class ZBufferCmp : uint8_t { kAlways = 0, kNever = 1, kLess = 2, kLessEqual = 3 };

// Pre-transformed vertex for alpha polygon rendering
// Layout matches D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1
#pragma pack(push, 1)
struct AlphaVertex {
  float x;
  float y;
  float z;         // Screen position (x, y) and depth buffer z
  float rhw;       // 1/w for perspective correction
  uint32_t color;  // ARGB diffuse color with alpha
  float u;
  float v;  // Texture coordinates
};
#pragma pack(pop)
static_assert(sizeof(AlphaVertex) == 28, "AlphaVertex must match D3D TL vertex size");

// Queued alpha polygon data - everything needed to render later
struct QueuedAlphaPoly {
  // Vertex data
  int vert_count;
  std::array<AlphaVertex, kAlphaPolyMaxVerts> verts;

  // Render state
  void* texture;  // D3D9 texture pointer (or null)
  AlphaBlendFunc blend_func;
  ZBufferCmp z_func;
  int z_bias;  // Z bias for depth layering
  bool texture_wrap;
  bool texture_has_alpha;

  // Sorting
  float z_value;          // Average Z for bucket sorting
  QueuedAlphaPoly* next;  // Next in bucket list
};

// ----------------------------------------------------------------------------
// Alpha Polygon Queue
// ----------------------------------------------------------------------------
// Manages deferred rendering of transparent geometry using the painter's algorithm.
//
// Why depth-sorted order is required:
// Alpha-blended polygons (fire, smoke, water, glass, particles) cannot use the
// Z-buffer for hidden surface removal because they need to blend with whatever
// is behind them. Instead, we must render them in strict back-to-front order
// (painter's algorithm) so that distant transparent objects are drawn first
// and closer ones blend on top correctly.
//
// State sorting (grouping by texture/blend mode) was considered to reduce GPU
// state changes, but it causes visible flickering when overlapping particle
// effects (fire + smoke) are reordered. The visual correctness of depth ordering
// outweighs the minor performance benefit of state batching.
//
// Implementation uses 512 depth buckets for O(1) insertion with fine-grained
// sorting within each bucket for polygons at similar depths.
// ----------------------------------------------------------------------------
class AlphaPolyQueue {
public:
  AlphaPolyQueue();
  ~AlphaPolyQueue() = default;

  // Non-copyable
  AlphaPolyQueue(const AlphaPolyQueue&) = delete;
  AlphaPolyQueue& operator=(const AlphaPolyQueue&) = delete;

  // Queue an alpha polygon for deferred rendering.
  // Returns pointer to queued poly for caller to fill in vertex data, or null if pool full.
  QueuedAlphaPoly* Allocate();

  // Add a filled polygon to the sort buckets (sorted insertion by depth).
  void Submit(QueuedAlphaPoly* poly);

  // Set bucket size based on far clip Z (called at BeginFrame).
  void SetFarClipZ(float far_clip_z);

  // Render all queued polygons in depth-sorted order (back to front).
  // Calls the provided draw function for each polygon.
  template <typename DrawFunc>
  void RenderAll(DrawFunc&& draw_fn);

  // Render all queued polygons in insertion order (for immediate polys).
  template <typename DrawFunc>
  void RenderInOrder(DrawFunc&& draw_fn);

  // Reset for new frame.
  void Reset();

  // Stats
  [[nodiscard]] int GetQueuedCount() const {
    return alloc_count_;
  }
  [[nodiscard]] bool IsEmpty() const {
    return alloc_count_ == 0;
  }
  [[nodiscard]] bool IsLimitReached() const {
    return alloc_count_ >= kAlphaPolyPoolSize - 300;
  }

  // Get the head of a specific bucket (for interleaved rendering with alpha sort objects)
  [[nodiscard]] QueuedAlphaPoly* GetBucketHead(int bucket_idx) const {
    if (bucket_idx < 0 || bucket_idx >= kAlphaSortBuckets) {
      return nullptr;
    }
    return buckets_[bucket_idx];
  }

private:
  [[nodiscard]] int ComputeBucket(float z_value) const;

  // Pool of alpha polygons (heap-allocated to avoid stack overflow)
  std::vector<QueuedAlphaPoly> pool_;
  int alloc_count_;

  // Sort buckets (far to near)
  std::array<QueuedAlphaPoly*, kAlphaSortBuckets> buckets_;
  float bucket_size_ = 1.0f;
};

// Template implementation
template <typename DrawFunc>
void AlphaPolyQueue::RenderAll(DrawFunc&& draw_fn) {
  // Iterate buckets from high to low (larger Z = farther, render first for painter's algorithm)
  for (int bucket = kAlphaSortBuckets - 1; bucket >= 0; --bucket) {
    QueuedAlphaPoly* poly = buckets_[bucket];
    while (poly) {
      draw_fn(*poly);
      poly = poly->next;
    }
  }

  // Clear buckets after rendering
  Reset();
}

// Render in allocation/insertion order (for immediate alpha polys)
template <typename DrawFunc>
void AlphaPolyQueue::RenderInOrder(DrawFunc&& draw_fn) {
  // Simply iterate through the pool in allocation order
  for (int i = 0; i < alloc_count_; ++i) {
    draw_fn(pool_[i]);
  }
  // Note: caller should call Reset() after this
}

// ----------------------------------------------------------------------------
// Alpha Polygon Batcher
// ----------------------------------------------------------------------------
// Batches alpha polygons that share the same render state (texture + blend func)
// into single draw calls. This dramatically reduces draw call overhead while
// maintaining correct depth ordering within each depth bucket.
//
// Strategy:
// - Within each depth bucket, polygons are at similar depths
// - We can batch consecutive polygons with identical render state
// - When state changes or buffer is full, flush the batch
// - This preserves depth ordering while reducing GPU state changes
//
// Statistics show typical scenes have 50-80% of alpha polys batchable,
// reducing draw calls from thousands to hundreds.
// ----------------------------------------------------------------------------

// Maximum vertices/indices in a single alpha batch
inline constexpr size_t kAlphaBatchMaxVertices = 4096;
inline constexpr size_t kAlphaBatchMaxIndices = kAlphaBatchMaxVertices * 3;

// Render state key for batching - polygons with same key can be batched
struct AlphaRenderStateKey {
  void* texture = nullptr;
  AlphaBlendFunc blend_func = AlphaBlendFunc::kBlend;
  ZBufferCmp z_func = ZBufferCmp::kLessEqual;
  int z_bias = 0;
  bool texture_wrap = true;
  bool texture_has_alpha = false;

  [[nodiscard]] bool operator==(const AlphaRenderStateKey& other) const {
    return texture == other.texture && blend_func == other.blend_func && z_func == other.z_func && z_bias == other.z_bias &&
           texture_wrap == other.texture_wrap && texture_has_alpha == other.texture_has_alpha;
  }

  [[nodiscard]] bool operator!=(const AlphaRenderStateKey& other) const {
    return !(*this == other);
  }

  // Create key from a queued alpha poly
  static AlphaRenderStateKey FromPoly(const QueuedAlphaPoly& poly) {
    return {poly.texture, poly.blend_func, poly.z_func, poly.z_bias, poly.texture_wrap, poly.texture_has_alpha};
  }
};

// Batch statistics for performance monitoring
struct AlphaBatchStats {
  size_t polys_submitted = 0;    // Total polys submitted this frame
  size_t polys_batched = 0;      // Polys that were batched (not individual draws)
  size_t batches_flushed = 0;    // Number of batched draw calls
  size_t draw_calls_saved = 0;   // polys_batched - batches_flushed
  size_t state_changes = 0;      // Number of render state changes
  size_t vertices_rendered = 0;  // Total vertices rendered
  size_t indices_rendered = 0;   // Total indices rendered

  void Reset() {
    polys_submitted = 0;
    polys_batched = 0;
    batches_flushed = 0;
    draw_calls_saved = 0;
    state_changes = 0;
    vertices_rendered = 0;
    indices_rendered = 0;
  }

  [[nodiscard]] float GetBatchEfficiency() const {
    if (polys_submitted == 0)
      return 0.0f;
    return static_cast<float>(draw_calls_saved) / static_cast<float>(polys_submitted);
  }
};

class AlphaPolyBatcher {
public:
  AlphaPolyBatcher();
  ~AlphaPolyBatcher() = default;

  // Non-copyable
  AlphaPolyBatcher(const AlphaPolyBatcher&) = delete;
  AlphaPolyBatcher& operator=(const AlphaPolyBatcher&) = delete;

  // Begin a new batching session (call at start of alpha pass)
  void Begin();

  // Submit a polygon for batched rendering
  // Returns true if added to current batch, false if batch was flushed first
  bool Submit(const QueuedAlphaPoly& poly);

  // Get current batch data for rendering (call when batch needs to be drawn)
  // Returns number of indices in current batch (0 if empty)
  [[nodiscard]] size_t GetBatchData(const AlphaVertex*& vertices_out, const uint16_t*& indices_out, AlphaRenderStateKey& state_out) const;

  // Mark current batch as rendered and reset for next batch
  void MarkBatchRendered();

  // End batching session and get final batch if any
  // Returns true if there's a final batch to render
  [[nodiscard]] bool End();

  // Force flush current batch (for state changes)
  void Flush();

  // Check if there's pending geometry
  [[nodiscard]] bool HasPendingGeometry() const {
    return index_count_ > 0;
  }

  // Get statistics
  [[nodiscard]] const AlphaBatchStats& GetStats() const {
    return stats_;
  }
  void ResetStats() {
    stats_.Reset();
  }

private:
  // Convert triangle fan to indexed triangles and add to batch
  void AddTriangleFan(const AlphaVertex* verts, int vert_count);

  // Check if we can add a polygon to current batch
  [[nodiscard]] bool CanBatch(const QueuedAlphaPoly& poly) const;

  // Vertex and index buffers (heap-allocated for large sizes)
  std::vector<AlphaVertex> vertices_;
  std::vector<uint16_t> indices_;
  size_t vertex_count_ = 0;
  size_t index_count_ = 0;

  // Current batch render state
  AlphaRenderStateKey current_state_;
  bool batch_active_ = false;

  // Statistics
  AlphaBatchStats stats_;
};

}  // namespace gmp::renderer

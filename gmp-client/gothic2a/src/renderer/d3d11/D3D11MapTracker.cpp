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

#include "D3D11MapTracker.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace gmp::renderer::d3d11 {

namespace {

struct Key {
  ID3D11Resource* resource = nullptr;
  UINT subresource = 0;

  bool operator==(const Key& other) const {
    return resource == other.resource && subresource == other.subresource;
  }
};

struct KeyHash {
  std::size_t operator()(const Key& k) const noexcept {
    const auto p = reinterpret_cast<std::uintptr_t>(k.resource);
    // Mix pointer bits with subresource.
    return (static_cast<std::size_t>(p >> 4) ^ (static_cast<std::size_t>(k.subresource) * 0x9E3779B97F4A7C15ull));
  }
};

struct Entry {
  int map_count = 0;
  std::string last_map_tag;
  std::string last_unmap_tag;
#ifdef _WIN32
  DWORD last_map_tid = 0;
  DWORD last_unmap_tid = 0;
#endif
};

struct Event {
  enum class Type { kMapOk, kMapFail, kUnmap } type;
  ID3D11Resource* resource;
  UINT subresource;
  HRESULT hr;
  const char* tag;
#ifdef _WIN32
  DWORD tid;
#endif
};

constexpr int kRingSize = 64;
std::mutex g_mu;
std::unordered_map<Key, Entry, KeyHash> g_entries;
Event g_ring[kRingSize] = {};
std::atomic<int> g_ring_pos{0};

#ifdef _WIN32
DWORD CurTid() {
  return GetCurrentThreadId();
}
#else
unsigned long CurTid() {
  return 0;
}
#endif

void PushEvent(Event ev) {
  const int pos = g_ring_pos.fetch_add(1, std::memory_order_relaxed);
  g_ring[pos % kRingSize] = ev;
}

void DumpRecentLocked_NoLock() {
  // Dump the last few events in reverse order.
  SPDLOG_ERROR("Recent Map/Unmap events (newest last):");
  const int end = g_ring_pos.load(std::memory_order_relaxed);
  const int start = (end > kRingSize) ? (end - kRingSize) : 0;
  for (int i = start; i < end; ++i) {
    const Event& e = g_ring[i % kRingSize];
    const char* type = "?";
    if (e.type == Event::Type::kMapOk)
      type = "MapOK";
    if (e.type == Event::Type::kMapFail)
      type = "MapFAIL";
    if (e.type == Event::Type::kUnmap)
      type = "Unmap";
#ifdef _WIN32
    SPDLOG_ERROR("  {} res={} sub={} tag='{}' hr=0x{:08X} tid={}", type, static_cast<void*>(e.resource), e.subresource, (e.tag ? e.tag : ""),
                 static_cast<std::uint32_t>(e.hr), static_cast<unsigned long>(e.tid));
#else
    SPDLOG_ERROR("  {} res={} sub={} tag='{}' hr=0x{:08X}", type, static_cast<void*>(e.resource), e.subresource, (e.tag ? e.tag : ""),
                 static_cast<std::uint32_t>(e.hr));
#endif
  }
}

}  // namespace

HRESULT TrackedMap(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, D3D11_MAP map_type, UINT map_flags,
                   D3D11_MAPPED_SUBRESOURCE* mapped, const char* tag) {
  if (!context || !resource || !mapped) {
    return E_INVALIDARG;
  }

  const HRESULT hr = context->Map(resource, subresource, map_type, map_flags, mapped);

#ifdef _DEBUG
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (SUCCEEDED(hr)) {
      Entry& e = g_entries[Key{resource, subresource}];
      e.map_count += 1;
      e.last_map_tag = (tag ? tag : "");
#ifdef _WIN32
      e.last_map_tid = CurTid();
#endif
      PushEvent(Event{Event::Type::kMapOk, resource, subresource, hr, tag,
#ifdef _WIN32
                      CurTid()
#else
                      0
#endif
      });
    } else {
      PushEvent(Event{Event::Type::kMapFail, resource, subresource, hr, tag,
#ifdef _WIN32
                      CurTid()
#else
                      0
#endif
      });
    }
  }
#else
  (void)tag;
#endif

  return hr;
}

void TrackedUnmap(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, const char* tag) {
  if (!context || !resource) {
    return;
  }

#ifdef _DEBUG
  {
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_entries.find(Key{resource, subresource});
    if (it == g_entries.end() || it->second.map_count <= 0) {
      SPDLOG_ERROR("Unmap without Map detected: res={} sub={} tag='{}' (no tracked map)", static_cast<void*>(resource), subresource,
                   (tag ? tag : ""));
      DumpRecentLocked_NoLock();
    } else {
      it->second.map_count -= 1;
      it->second.last_unmap_tag = (tag ? tag : "");
#ifdef _WIN32
      it->second.last_unmap_tid = CurTid();
#endif
      if (it->second.map_count == 0) {
        // Keep the entry around (useful for later diagnostics), but it could be erased.
      }
    }

    PushEvent(Event{Event::Type::kUnmap, resource, subresource, S_OK, tag,
#ifdef _WIN32
                    CurTid()
#else
                    0
#endif
    });
  }
#else
  (void)tag;
#endif

  context->Unmap(resource, subresource);
}

}  // namespace gmp::renderer::d3d11

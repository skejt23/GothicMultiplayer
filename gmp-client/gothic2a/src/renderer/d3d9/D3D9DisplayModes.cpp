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

#include "D3D9DisplayModes.h"

#include <d3d9.h>
#include <spdlog/spdlog.h>

#include <set>
#include <utility>
#include <vector>

namespace gmp::renderer {

D3D9DisplayModes::D3D9DisplayModes() {
  Enumerate();
}

void D3D9DisplayModes::Enumerate() {
  if (enumerated_) {
    return;
  }

  // Create temporary D3D9 instance for enumeration
  IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d9) {
    SPDLOG_ERROR("D3D9DisplayModes: Failed to create D3D9 for enumeration");
    enumerated_ = true;
    return;
  }

  enumerated_ = true;

  // Common/standard resolutions we want to support (sorted by pixel count)
  // We filter to these to keep the menu manageable and ensure important modes are included
  static const std::vector<std::pair<int, int>> kPreferredResolutions = {
      {800, 600},    // 4:3
      {1024, 768},   // 4:3
      {1280, 720},   // 16:9 HD
      {1280, 800},   // 16:10
      {1280, 1024},  // 5:4
      {1366, 768},   // 16:9 (common laptop)
      {1440, 900},   // 16:10
      {1600, 900},   // 16:9
      {1600, 1200},  // 4:3
      {1680, 1050},  // 16:10
      {1920, 1080},  // 16:9 Full HD
      {1920, 1200},  // 16:10
      {2560, 1440},  // 16:9 QHD
      {2560, 1600},  // 16:10
      {3840, 2160},  // 16:9 4K
  };

  // Enumerate 32-bit modes (X8R8G8B8) for the default adapter
  UINT modeCount = d3d9->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
  SPDLOG_INFO("D3D9DisplayModes: Found {} display modes from adapter", modeCount);

  // Build a set of available resolutions from the adapter
  std::set<std::pair<int, int>> availableModes;
  for (UINT i = 0; i < modeCount; i++) {
    D3DDISPLAYMODE mode;
    if (SUCCEEDED(d3d9->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &mode))) {
      availableModes.insert({static_cast<int>(mode.Width), static_cast<int>(mode.Height)});
    }
  }

  // Add preferred resolutions that are available on this adapter
  for (const auto& pref : kPreferredResolutions) {
    if (availableModes.count(pref) > 0) {
      modes_.push_back({pref.first, pref.second, 32});
      SPDLOG_DEBUG("D3D9DisplayModes: Added mode {}x{}", pref.first, pref.second);
    }
  }

  SPDLOG_INFO("D3D9DisplayModes: {} modes available", modes_.size());

  // Release temporary D3D9 instance
  d3d9->Release();
}

int D3D9DisplayModes::GetNumModes() const {
  return static_cast<int>(modes_.size());
}

const VideoMode* D3D9DisplayModes::GetMode(int index) const {
  if (index < 0 || index >= static_cast<int>(modes_.size())) {
    return nullptr;
  }
  return &modes_[index];
}

int D3D9DisplayModes::FindModeIndex(int width, int height, int bpp) const {
  for (int i = 0; i < static_cast<int>(modes_.size()); ++i) {
    if (modes_[i].width == width && modes_[i].height == height && modes_[i].bpp == bpp) {
      return i;
    }
  }
  return -1;
}

int D3D9DisplayModes::GetActiveModeNr() const {
  return active_mode_nr_;
}

void D3D9DisplayModes::SetActiveModeNr(int modeNr) {
  active_mode_nr_ = modeNr;
}

}  // namespace gmp::renderer

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

#include "D3D11DisplayModes.h"

#include <dxgi.h>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

#include <set>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace gmp::renderer::d3d11 {

D3D11DisplayModes::D3D11DisplayModes() {
  Enumerate();
}

void D3D11DisplayModes::Enumerate() {
  if (enumerated_) {
    return;
  }
  enumerated_ = true;

  // Create DXGI Factory for enumeration
  ComPtr<IDXGIFactory1> factory;
  HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
  if (FAILED(hr)) {
    SPDLOG_ERROR("D3D11DisplayModes: Failed to create DXGI factory: 0x{:08X}", static_cast<uint32_t>(hr));
    return;
  }

  // Get primary adapter
  ComPtr<IDXGIAdapter1> adapter;
  hr = factory->EnumAdapters1(0, adapter.GetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("D3D11DisplayModes: Failed to get primary adapter: 0x{:08X}", static_cast<uint32_t>(hr));
    return;
  }

  // Get primary output (monitor)
  ComPtr<IDXGIOutput> output;
  hr = adapter->EnumOutputs(0, output.GetAddressOf());
  if (FAILED(hr)) {
    SPDLOG_ERROR("D3D11DisplayModes: Failed to get primary output: 0x{:08X}", static_cast<uint32_t>(hr));
    return;
  }

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

  // Get number of modes for B8G8R8A8_UNORM format (equivalent to X8R8G8B8)
  UINT modeCount = 0;
  hr = output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &modeCount, nullptr);
  if (FAILED(hr) || modeCount == 0) {
    SPDLOG_ERROR("D3D11DisplayModes: Failed to get display mode count: 0x{:08X}", static_cast<uint32_t>(hr));
    return;
  }

  // Get all modes
  std::vector<DXGI_MODE_DESC> allModes(modeCount);
  hr = output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &modeCount, allModes.data());
  if (FAILED(hr)) {
    SPDLOG_ERROR("D3D11DisplayModes: Failed to enumerate display modes: 0x{:08X}", static_cast<uint32_t>(hr));
    return;
  }

  // Build a set of available resolutions
  std::set<std::pair<int, int>> availableModes;
  for (const auto& mode : allModes) {
    availableModes.insert({static_cast<int>(mode.Width), static_cast<int>(mode.Height)});
  }

  // Add preferred resolutions that are available on this adapter
  for (const auto& pref : kPreferredResolutions) {
    if (availableModes.count(pref) > 0) {
      modes_.push_back({pref.first, pref.second, 32});
    }
  }
}

int D3D11DisplayModes::GetNumModes() const {
  return static_cast<int>(modes_.size());
}

const VideoMode* D3D11DisplayModes::GetMode(int index) const {
  if (index < 0 || index >= static_cast<int>(modes_.size())) {
    return nullptr;
  }
  return &modes_[index];
}

int D3D11DisplayModes::FindModeIndex(int width, int height, int bpp) const {
  for (int i = 0; i < static_cast<int>(modes_.size()); ++i) {
    if (modes_[i].width == width && modes_[i].height == height && modes_[i].bpp == bpp) {
      return i;
    }
  }
  return -1;
}

int D3D11DisplayModes::GetActiveModeNr() const {
  return active_mode_nr_;
}

void D3D11DisplayModes::SetActiveModeNr(int modeNr) {
  active_mode_nr_ = modeNr;
}

}  // namespace gmp::renderer::d3d11

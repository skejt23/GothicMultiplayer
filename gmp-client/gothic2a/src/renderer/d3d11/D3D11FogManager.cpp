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

#include "D3D11FogManager.h"

namespace gmp::renderer::d3d11 {

void D3D11FogManager::SetEnabled(bool enable) {
  if (disabled_ == !enable) {
    return;  // No state change
  }

  disabled_ = !enable;
  dirty_ = true;
}

void D3D11FogManager::SetRadialEnabled(bool enable) {
  if (radial_enabled_ == enable) {
    return;
  }

  radial_enabled_ = enable;
  dirty_ = true;
}

void D3D11FogManager::SetColor(const zCOLOR& color) {
  if (color_.dword == color.dword) {
    return;
  }

  color_ = color;
  dirty_ = true;
}

void D3D11FogManager::SetRange(float near_z, float far_z, int mode) {
  if (start_ == near_z && end_ == far_z && mode_ == mode) {
    return;
  }

  start_ = near_z;
  end_ = far_z;
  mode_ = mode;
  dirty_ = true;
}

void D3D11FogManager::GetRange(float& near_z, float& far_z, int& mode) const {
  near_z = start_;
  far_z = end_;
  mode = mode_;
}

D3D11FogManager::FogState D3D11FogManager::SaveState() const {
  return FogState{.enabled = !disabled_, .radial = radial_enabled_, .color = color_, .start = start_, .end = end_, .mode = mode_};
}

void D3D11FogManager::RestoreState(const FogState& state) {
  radial_enabled_ = state.radial;
  color_ = state.color;
  start_ = state.start;
  end_ = state.end;
  mode_ = state.mode;
  SetEnabled(state.enabled);
}

D3D11FogManager::FogCBData D3D11FogManager::GetConstantBufferData() const {
  FogCBData data = {};

  // Convert zCOLOR (BGRA) to RGBA float
  data.FogColor[0] = static_cast<float>(color_.r) / 255.0f;
  data.FogColor[1] = static_cast<float>(color_.g) / 255.0f;
  data.FogColor[2] = static_cast<float>(color_.b) / 255.0f;
  data.FogColor[3] = static_cast<float>(color_.alpha) / 255.0f;

  data.FogStart = start_;
  data.FogEnd = end_;
  data.FogDensity = 0.01f;  // Default density for exponential modes
  data.FogEnabled = disabled_ ? 0 : 1;
  data.FogMode = radial_enabled_ ? static_cast<int>(FogMode::Radial) : static_cast<int>(FogMode::Linear);

  return data;
}

}  // namespace gmp::renderer::d3d11

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

#include "D3D9FogManager.h"

#include <spdlog/spdlog.h>

namespace gmp::renderer {

namespace {
// Constants for D3D9 boolean render states.
constexpr DWORD kTrue32 = 1;
constexpr DWORD kFalse32 = 0;
}  // namespace

void D3D9FogManager::Init(IDirect3DDevice9* device) {
  device_ = device;
  SPDLOG_TRACE("D3D9FogManager initialized");
}

void D3D9FogManager::SetEnabled(bool enable) {
  if (disabled_ == !enable) {
    return;  // No state change
  }

  disabled_ = !enable;

  if (!device_) {
    return;
  }

  if (enable) {
    ApplyRenderState(D3DRS_FOGENABLE, kTrue32);
    ApplyFogModes(true);

    // Apply fog color and range.
    ApplyRenderState(D3DRS_FOGCOLOR, color_.dword);
    ApplyRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(start_));
    ApplyRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(end_));

    // Set range fog enable based on radial setting.
    ApplyRenderState(D3DRS_RANGEFOGENABLE, radial_enabled_ ? kTrue32 : kFalse32);
  } else {
    // Disable fog - must also disable fog modes to prevent artifacts.
    ApplyFogModes(false);
    ApplyRenderState(D3DRS_FOGENABLE, kFalse32);
  }
}

void D3D9FogManager::SetRadialEnabled(bool enable) {
  radial_enabled_ = enable;
}

void D3D9FogManager::SetColor(const zCOLOR& color) {
  color_ = color;
  if (device_) {
    ApplyRenderState(D3DRS_FOGCOLOR, color.dword);
  }
}

void D3D9FogManager::SetRange(float near_z, float far_z, int mode) {
  start_ = near_z;
  end_ = far_z;
  mode_ = mode;

  if (!device_) {
    return;
  }

  ApplyRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(near_z));
  ApplyRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(far_z));

  if (radial_enabled_) {
    ApplyRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);  // Disable table fog
    ApplyRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
    ApplyRenderState(D3DRS_RANGEFOGENABLE, kTrue32);
  } else {
    ApplyRenderState(D3DRS_RANGEFOGENABLE, kFalse32);    // Disable range fog
    ApplyRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);  // Disable vertex fog
    ApplyRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
  }
}

void D3D9FogManager::GetRange(float& near_z, float& far_z, int& mode) const {
  near_z = start_;
  far_z = end_;
  mode = mode_;
}

D3D9FogManager::FogState D3D9FogManager::SaveState() const {
  return FogState{.enabled = !disabled_, .radial = radial_enabled_, .color = color_, .start = start_, .end = end_, .mode = mode_};
}

void D3D9FogManager::RestoreState(const FogState& state) {
  radial_enabled_ = state.radial;
  color_ = state.color;
  start_ = state.start;
  end_ = state.end;
  mode_ = state.mode;
  SetEnabled(state.enabled);
}

void D3D9FogManager::ApplyToDevice() {
  if (!device_ || disabled_) {
    return;
  }

  ApplyRenderState(D3DRS_FOGENABLE, kTrue32);
  ApplyFogModes(true);
  ApplyRenderState(D3DRS_FOGCOLOR, color_.dword);
  ApplyRenderState(D3DRS_FOGSTART, std::bit_cast<DWORD>(start_));
  ApplyRenderState(D3DRS_FOGEND, std::bit_cast<DWORD>(end_));
  ApplyRenderState(D3DRS_RANGEFOGENABLE, radial_enabled_ ? kTrue32 : kFalse32);
}

void D3D9FogManager::ApplyRenderState(D3DRENDERSTATETYPE state, DWORD value) {
  if (device_) {
    device_->SetRenderState(state, value);
  }
}

void D3D9FogManager::ApplyFogModes(bool enable) {
  if (!device_) {
    return;
  }

  if (enable) {
    // Set fog mode based on radial fog setting.
    if (radial_enabled_) {
      ApplyRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
    } else {
      ApplyRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    }
  } else {
    // Disable fog modes.
    if (radial_enabled_) {
      ApplyRenderState(D3DRS_RANGEFOGENABLE, kFalse32);
      ApplyRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    } else {
      ApplyRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    }
  }
}

}  // namespace gmp::renderer

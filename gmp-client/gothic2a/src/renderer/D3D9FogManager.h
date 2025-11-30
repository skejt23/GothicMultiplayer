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

#include <bit>

#include "ZenGin/zGothicAPI.h"

namespace gmp::renderer {

// ----------------------------------------------------------------------------
// D3D9 Fog Management System
// ----------------------------------------------------------------------------
// This class encapsulates Gothic II's fog rendering state management for the
// D3D9 renderer. Gothic's fog system supports two modes:
//
// 1. Table Fog (Pixel Fog): Applied per-pixel in the rasterizer using lookup
//    tables. Uses FOGTABLEMODE.
//    This is the classic fog mode used in indoor scenes and smaller areas.
//
// 2. Vertex Fog (Range-Based): Computed per-vertex in the T&L pipeline.
//    When combined with RANGEFOGENABLE, it calculates fog based on actual
//    distance from camera rather than just Z-depth, preventing "banding"
//    artifacts at screen edges. This is Gothic's "radial fog" mode.
//
// Note: Fog must be disabled during 2D rendering (UI, menus) to prevent scene
// fog from affecting overlay elements.
// ----------------------------------------------------------------------------

class D3D9FogManager {
public:
  // Fog state structure for save/restore operations.
  struct FogState {
    bool enabled = false;
    bool radial = true;
    zCOLOR color{};
    float start = 0.0f;
    float end = 10000.0f;
    int mode = 0;
  };

  D3D9FogManager() = default;

  // Initialize with D3D9 device reference.
  void Init(IDirect3DDevice9* device);

  // Enable or disable fog rendering.
  // When enabled, applies all fog parameters to D3D9 render states.
  // When disabled, clears fog modes to prevent rendering artifacts.
  void SetEnabled(bool enable);
  bool IsEnabled() const {
    return !disabled_;
  }

  // Enable or disable radial (range-based) fog.
  // Radial fog uses vertex fog with range-based distance calculation,
  // preventing fog "banding" at screen edges in large outdoor scenes.
  void SetRadialEnabled(bool enable);
  bool IsRadialEnabled() const {
    return radial_enabled_;
  }

  // Set fog color (ARGB format matching Gothic's zCOLOR).
  void SetColor(const zCOLOR& color);
  zCOLOR GetColor() const {
    return color_;
  }

  // Set fog distance range.
  // near_z: Distance where fog begins (0% fog)
  // far_z: Distance where fog is complete (100% fog)
  // mode: Fog density mode (reserved for future use)
  void SetRange(float near_z, float far_z, int mode);
  void GetRange(float& near_z, float& far_z, int& mode) const;

  // Save and restore fog state (used during 2D rendering).
  FogState SaveState() const;
  void RestoreState(const FogState& state);

  // Apply current fog settings to D3D9 device.
  // Call this when re-enabling fog or after device reset.
  void ApplyToDevice();

private:
  // Apply a D3D9 render state, with optional state caching.
  void ApplyRenderState(D3DRENDERSTATETYPE state, DWORD value);

  // Apply fog mode settings based on radial flag.
  void ApplyFogModes(bool enable);

  IDirect3DDevice9* device_ = nullptr;

  // Fog state.
  bool disabled_ = false;       // Master enable/disable flag
  bool radial_enabled_ = true;  // Use range-based vertex fog vs table fog
  zCOLOR color_{};
  float start_ = 0.0f;
  float end_ = 10000.0f;
  int mode_ = 0;
};

}  // namespace gmp::renderer

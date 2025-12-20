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

#include <d3d11.h>

#include "ZenGin/zGothicAPI.h"

namespace gmp::renderer::d3d11 {

// ----------------------------------------------------------------------------
// D3D11 Fog Management System (Shader-Based)
// ----------------------------------------------------------------------------
// This class encapsulates Gothic II's fog rendering state management for the
// D3D11 renderer. Unlike D3D9's fixed-function fog, D3D11 implements fog
// entirely in shaders via the FogCB constant buffer.
//
// Gothic's fog system supports two modes:
//
// 1. Linear Fog: Classic fog where density increases linearly between
//    start and end distances.
//
// 2. Radial Fog (Range-Based): Fog calculated based on actual distance
//    from camera rather than just Z-depth, preventing "banding"
//    artifacts at screen edges. This is Gothic's default outdoor mode.
//
// The fog parameters are passed to shaders via a constant buffer:
// - FogEnabled: Master enable flag (0 or 1)
// - FogColor: RGBA fog color
// - FogStart/FogEnd: Distance range for linear fog
// - FogDensity: Fog density for exponential modes
// - FogMode: 0=Linear, 1=Exp, 2=Exp2, 3=Radial
//
// Note: Fog must be disabled during 2D rendering (UI, menus).
// ----------------------------------------------------------------------------

class D3D11FogManager {
public:
  // Fog mode constants (for shader)
  enum class FogMode : int { Linear = 0, Exponential = 1, ExponentialSquared = 2, Radial = 3 };

  // Fog state structure for save/restore operations.
  struct FogState {
    bool enabled = false;
    bool radial = true;
    zCOLOR color{};
    float start = 0.0f;
    float end = 10000.0f;
    int mode = 0;
  };

  // Fog constant buffer data (matches HLSL layout)
  struct FogCBData {
    float FogColor[4];  // RGBA
    float FogStart;
    float FogEnd;
    float FogDensity;
    int FogEnabled;
    int FogMode;       // 0=Linear, 1=Exp, 2=Exp2, 3=Radial
    float Padding[3];  // Pad to 16-byte boundary
  };

  D3D11FogManager() = default;

  // Enable or disable fog rendering.
  void SetEnabled(bool enable);
  bool IsEnabled() const {
    return !disabled_;
  }

  // Enable or disable radial (range-based) fog.
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
  void SetRange(float near_z, float far_z, int mode);
  void GetRange(float& near_z, float& far_z, int& mode) const;

  // Save and restore fog state (used during 2D rendering).
  FogState SaveState() const;
  void RestoreState(const FogState& state);

  // Get constant buffer data for shader upload
  FogCBData GetConstantBufferData() const;

  // Check if fog state has changed since last upload
  bool IsDirty() const {
    return dirty_;
  }
  void ClearDirty() {
    dirty_ = false;
  }

private:
  // Fog state
  bool disabled_ = false;       // Master enable/disable flag
  bool radial_enabled_ = true;  // Use range-based fog vs depth-based
  zCOLOR color_{};
  float start_ = 0.0f;
  float end_ = 10000.0f;
  int mode_ = 0;
  bool dirty_ = true;  // True if CB needs update
};

}  // namespace gmp::renderer::d3d11

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

namespace gmp::renderer::d3d11 {

// --- Vertex Formats ---
// These vertex structures match Gothic's internal formats.
// In D3D11, we use Input Layouts instead of FVF.

// 3D vertex with position, normal, diffuse color, and single UV coordinate.
// Used for world geometry that needs lighting calculations.
struct Vertex3D {
  float x, y, z;        // Position in world/view space
  float nx, ny, nz;     // Normal for lighting
  unsigned long color;  // Pre-lit vertex color (ARGB)
  float u, v;           // Texture coordinates
};

// Pre-transformed (RHW) vertex with position, color, and single UV.
// Used for 2D UI elements and post-projection geometry.
// In D3D11, we transform these in the vertex shader to NDC.
struct VertexRHW {
  float x, y, z, rhw;   // Screen-space position (rhw = 1/w)
  unsigned long color;  // Vertex color (ARGB)
  float u, v;           // Texture coordinates
};

// Pre-transformed vertex with two UV sets for multi-texturing.
// Used for lightmapped surfaces (diffuse + lightmap).
struct VertexRHW2 {
  float x, y, z, rhw;   // Screen-space position
  unsigned long color;  // Vertex color (ARGB)
  float u1, v1;         // Primary texture coordinates (diffuse)
  float u2, v2;         // Secondary texture coordinates (lightmap)
};

}  // namespace gmp::renderer::d3d11

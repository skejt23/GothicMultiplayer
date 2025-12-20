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

namespace gmp::renderer::d3d11 {

// Debug-only Map/Unmap tracker to pinpoint "Unmap not mapped" sources.
// In release builds, these are thin passthrough wrappers.
HRESULT TrackedMap(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, D3D11_MAP map_type, UINT map_flags,
                   D3D11_MAPPED_SUBRESOURCE* mapped, const char* tag);

void TrackedUnmap(ID3D11DeviceContext* context, ID3D11Resource* resource, UINT subresource, const char* tag);

}  // namespace gmp::renderer::d3d11

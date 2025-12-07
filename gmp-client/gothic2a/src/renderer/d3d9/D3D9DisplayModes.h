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

#include <vector>

namespace gmp::renderer {

struct VideoMode {
  int width;
  int height;
  int bpp;
};

class D3D9DisplayModes {
public:
  D3D9DisplayModes();
  ~D3D9DisplayModes() = default;

  D3D9DisplayModes(const D3D9DisplayModes&) = delete;
  D3D9DisplayModes& operator=(const D3D9DisplayModes&) = delete;
  D3D9DisplayModes(D3D9DisplayModes&&) = delete;
  D3D9DisplayModes& operator=(D3D9DisplayModes&&) = delete;

  // Get the number of available display modes.
  [[nodiscard]] int GetNumModes() const;

  // Get mode info by index. Returns nullptr if index is invalid.
  [[nodiscard]] const VideoMode* GetMode(int index) const;

  // Find the index of a mode matching the given parameters.
  // Returns -1 if not found.
  [[nodiscard]] int FindModeIndex(int width, int height, int bpp) const;

  // Get/set the currently active mode index.
  [[nodiscard]] int GetActiveModeNr() const;
  void SetActiveModeNr(int modeNr);

private:
  void Enumerate();

  std::vector<VideoMode> modes_;
  int active_mode_nr_ = 0;
  bool enumerated_ = false;
};

}  // namespace gmp::renderer

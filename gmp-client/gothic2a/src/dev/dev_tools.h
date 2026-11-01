/*
MIT License

Copyright (c) 2025 skejt23

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

#include <ctime>

#include "ZenGin/zGothicAPI.h"

namespace debug {

class DevTools {
public:
  static DevTools& Instance();

  void HandleInput(bool writingOnChat);
  void Render();
  void SetNoclipAllowed(bool allowed);

private:
  DevTools();
  ~DevTools() = default;

  DevTools(const DevTools&) = delete;
  DevTools& operator=(const DevTools&) = delete;

  void HandleNoclip(bool writingOnChat);
  void RenderNoclipOverlay();
  void DisableNoclip();

  // Noclip state
  bool noclip_allowed_;
  bool noclip_enabled_;
  float noclip_speed_;
  clock_t last_noclip_update_;
};

}  // namespace debug

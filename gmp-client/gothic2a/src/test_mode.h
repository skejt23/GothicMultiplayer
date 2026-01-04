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

#include "benchmark.h"

class GMPCore;

/**
 * @brief Handles the specialized "Test Mode" initialization logic.
 *
 * This class encapsulates the logic for bypassing the main menu and
 * jumping directly into a specific level with a specific spawn point,
 * primarily for development and debugging purposes.
 */
class TestMode {
public:
  /**
   * @brief Construct a new Test Mode object
   *
   * @param core Reference to the GMPCore instance.
   */
  explicit TestMode(GMPCore& core);

  ~TestMode() = default;

  /**
   * @brief Initialize test mode: load level, setup player, etc.
   */
  void Initialize();

  /**
   * @brief Frame update for test mode (e.g., benchmark tick).
   */
  void OnFrame();

private:
  GMPCore& core_;
  Benchmark benchmark_;
};

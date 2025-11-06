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

#include <chrono>
#include <cstdint>

/**
 * @brief Frame-rate independent timer for consistent animation speeds
 *
 * This class ensures that animations and updates run at a consistent rate
 * regardless of actual FPS. It mimics a target frame rate (default 60 FPS)
 * by accumulating time and returning true when enough time has elapsed for
 * the next "virtual frame".
 */
class FrameRateLimiter {
public:
  /**
   * @brief Construct a new Frame Rate Limiter
   *
   * @param targetFps Target frame rate to simulate (default: 60 FPS)
   */
  explicit FrameRateLimiter(int targetFps = 60)
      : target_frame_duration_(std::chrono::milliseconds(1000 / targetFps)), last_update_(std::chrono::steady_clock::now()), accumulated_time_(0) {
  }

  /**
   * @brief Check if enough time has elapsed for the next virtual frame
   *
   * Call this every frame. Returns true when enough time has accumulated
   * for the next virtual frame at the target FPS rate.
   *
   * @return true if a virtual frame should be processed
   */
  bool ShouldUpdate() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_update_;
    last_update_ = now;

    accumulated_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    if (accumulated_time_ >= target_frame_duration_) {
      accumulated_time_ -= target_frame_duration_;
      return true;
    }

    return false;
  }

  /**
   * @brief Reset the accumulated time
   *
   * Useful when entering/exiting states to avoid sudden jumps
   */
  void Reset() {
    accumulated_time_ = std::chrono::milliseconds(0);
    last_update_ = std::chrono::steady_clock::now();
  }

  /**
   * @brief Get elapsed time since construction or last reset
   *
   * @return Elapsed time in milliseconds
   */
  std::int64_t GetElapsedMilliseconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_).count();
  }

private:
  std::chrono::milliseconds target_frame_duration_;
  std::chrono::steady_clock::time_point last_update_;
  std::chrono::milliseconds accumulated_time_;
};

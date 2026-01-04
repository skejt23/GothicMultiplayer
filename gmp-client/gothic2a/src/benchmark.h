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
#include <string>
#include <vector>

class GMPCore;

/**
 * @brief Coordinates benchmark measurements across different renderers.
 *
 * This class provides FPS measurement and logging functionality for comparing
 * renderer performance (D3D7/D3D9/D3D11) with vsync disabled.
 *
 * Usage:
 * 1. Create Benchmark instance when test mode starts with benchmark enabled
 * 2. Call OnFrame() every frame to accumulate timing data
 * 3. Stop() is called automatically after configured duration, or manually
 */
class Benchmark {
public:
  explicit Benchmark(GMPCore& core);
  ~Benchmark() = default;

  /**
   * @brief Initialize benchmark configuration.
   */
  void Initialize();

  /**
   * @brief Start the benchmark explicitly.
   */
  void Start();

  /**
   * @brief Toggle benchmark running state.
   */
  void Toggle();

  /**
   * @brief Called each frame to update FPS measurements.
   * Automatically stops benchmark after configured duration.
   */
  void OnFrame();

  /**
   * @brief Stop benchmark and log results.
   */
  void Stop();

  /**
   * @brief Check if benchmark is currently running.
   */
  [[nodiscard]] bool IsRunning() const {
    return running_;
  }

  /**
   * @brief Get current instantaneous FPS (smoothed over recent frames).
   */
  [[nodiscard]] float GetCurrentFPS() const;

  /**
   * @brief Get the 1% low FPS.
   */
  [[nodiscard]] double Get1PercentLowFPS() const;

  /**
   * @brief Get the 0.1% low FPS.
   */
  [[nodiscard]] double Get01PercentLowFPS() const;

  /**
   * @brief Get average FPS since benchmark started.
   */
  [[nodiscard]] float GetAverageFPS() const;

  /**
   * @brief Get the name of the current renderer being benchmarked.
   */
  [[nodiscard]] const std::string& GetRendererName() const {
    return renderer_name_;
  }

private:
  void DrawOverlay();
  void DisableVSync();
  void LogResults();
  float ComputePercentileFPS(float percentile) const;

  GMPCore& core_;
  bool running_ = false;
  bool vsync_disabled_ = false;

  // Timing data
  std::chrono::high_resolution_clock::time_point benchmark_start_time_;
  std::chrono::high_resolution_clock::time_point last_frame_time_;
  std::vector<float> frame_times_ms_;  // All frame times for percentile calculations
  int frame_count_ = 0;

  // Configuration
  float duration_seconds_ = 10.0f;
  std::string renderer_name_;

  // Smoothed FPS for display (rolling average)
  static constexpr int kFPSSmoothingWindow = 30;
  float fps_history_[kFPSSmoothingWindow] = {};
  int fps_history_index_ = 0;
};

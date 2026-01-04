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

#include "benchmark.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>

#include "ZenGin/zGothicAPI.h"
#include "config.h"
#include "gmp_core.h"
#include "renderer/d3d11/D3D11Renderer.h"
#include "renderer/d3d9/D3D9Renderer.h"

Benchmark::Benchmark(GMPCore& core) : core_(core) {
}

void Benchmark::Initialize() {
  duration_seconds_ = 10.0f;

  // Determine renderer name for logging
  switch (Config::Instance().GetRendererType()) {
    case Config::RendererType::D3D7:
      renderer_name_ = "D3D7";
      break;
    case Config::RendererType::D3D9:
      renderer_name_ = "D3D9";
      break;
    case Config::RendererType::D3D11:
      renderer_name_ = "D3D11";
      break;
  }

  SPDLOG_INFO("[Benchmark] Initializing benchmark for {} renderer", renderer_name_);
  SPDLOG_INFO("[Benchmark] Duration: {}s", duration_seconds_);
}

void Benchmark::Start() {
  if (running_)
    return;

  // Always disable VSync for benchmark
  DisableVSync();

  // Reserve space for frame times (estimate: 60fps * duration + 50% headroom)
  size_t estimated_frames = static_cast<size_t>(std::ceil(duration_seconds_ * 60 * 1.5f));
  frame_times_ms_.clear();
  frame_times_ms_.reserve(estimated_frames);

  // Initialize timing
  frame_count_ = 0;
  fps_history_index_ = 0;
  std::fill(std::begin(fps_history_), std::end(fps_history_), 0.0f);

  benchmark_start_time_ = std::chrono::high_resolution_clock::now();
  last_frame_time_ = benchmark_start_time_;
  running_ = true;

  SPDLOG_INFO("[Benchmark] Started!");
}

void Benchmark::Toggle() {
  if (running_) {
    Stop();
  } else {
    Start();
  }
}

void Benchmark::DisableVSync() {
  // Try D3D11
  if (auto* d3d11 = dynamic_cast<zCRnd_D3D_DX11*>(Gothic_II_Addon::zrenderer)) {
    d3d11->SetVSync(false);
    vsync_disabled_ = true;
    SPDLOG_INFO("[Benchmark] VSync disabled for D3D11");
    return;
  }

  // Try D3D9
  if (auto* d3d9 = dynamic_cast<zCRnd_D3D_DX9*>(Gothic_II_Addon::zrenderer)) {
    d3d9->SetVSync(false);
    vsync_disabled_ = true;
    SPDLOG_INFO("[Benchmark] VSync disabled for D3D9");
    return;
  }

  // D3D7 or unknown - no GMP-side vsync control
  SPDLOG_WARN("[Benchmark] VSync control not available for this renderer");
  vsync_disabled_ = false;
}

void Benchmark::OnFrame() {
  if (!running_) {
    return;
  }

  auto now = std::chrono::high_resolution_clock::now();

  // Calculate frame time
  auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame_time_);
  float frame_time_ms = static_cast<float>(frame_duration.count()) / 1000.0f;
  last_frame_time_ = now;

  // Skip first frame (usually has initialization overhead)
  if (frame_count_ > 0) {
    frame_times_ms_.push_back(frame_time_ms);

    // Update rolling FPS average
    float instant_fps = (frame_time_ms > 0.0f) ? (1000.0f / frame_time_ms) : 0.0f;
    fps_history_[fps_history_index_] = instant_fps;
    fps_history_index_ = (fps_history_index_ + 1) % kFPSSmoothingWindow;
  }
  ++frame_count_;

  // Check if benchmark duration has elapsed
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - benchmark_start_time_);
  if (duration_seconds_ > 0.0f && elapsed.count() >= static_cast<long long>(duration_seconds_)) {
    Stop();
  }

  // Log periodic stats (every 5 seconds)
  if (frame_count_ % 300 == 0 && frame_count_ > 0) {  // ~5 seconds at 60fps
    SPDLOG_INFO("[Benchmark] {} - Current FPS: {:.1f}, Avg FPS: {:.1f}, Frames: {}", renderer_name_, GetCurrentFPS(), GetAverageFPS(), frame_count_);
  }

  DrawOverlay();
}

void Benchmark::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  SPDLOG_INFO("[Benchmark] Stopping benchmark...");
  LogResults();

  // Restore vsync to user preference
  if (vsync_disabled_) {
    bool vsync = Config::Instance().vsync_enabled;

    // Direct renderer access to restore
    if (Gothic_II_Addon::zrenderer) {
      if (auto* d3d11 = dynamic_cast<zCRnd_D3D_DX11*>(Gothic_II_Addon::zrenderer)) {
        d3d11->SetVSync(vsync);
      } else if (auto* d3d9 = dynamic_cast<zCRnd_D3D_DX9*>(Gothic_II_Addon::zrenderer)) {
        d3d9->SetVSync(vsync);
      }
    }
    SPDLOG_INFO("[Benchmark] VSync restored to {}", vsync);
  }
}

float Benchmark::GetCurrentFPS() const {
  // Calculate rolling average from fps_history_
  float sum = 0.0f;
  int count = 0;
  for (int i = 0; i < kFPSSmoothingWindow; ++i) {
    if (fps_history_[i] > 0.0f) {
      sum += fps_history_[i];
      ++count;
    }
  }
  return (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
}

float Benchmark::GetAverageFPS() const {
  if (frame_times_ms_.empty()) {
    return 0.0f;
  }

  // Calculate average frame time, then convert to FPS
  float total_time_ms = 0.0f;
  for (float ft : frame_times_ms_) {
    total_time_ms += ft;
  }
  float avg_frame_time_ms = total_time_ms / static_cast<float>(frame_times_ms_.size());
  return (avg_frame_time_ms > 0.0f) ? (1000.0f / avg_frame_time_ms) : 0.0f;
}

double Benchmark::Get1PercentLowFPS() const {
  return ComputePercentileFPS(1.0f);
}

double Benchmark::Get01PercentLowFPS() const {
  return ComputePercentileFPS(0.1f);
}

void Benchmark::DrawOverlay() {
  using namespace Gothic_II_Addon;
  if (!running_ || !screen)
    return;

  std::stringstream ss;
  ss << "Benchmark: " << renderer_name_ << "\n";
  ss << "FPS: " << std::fixed << std::setprecision(1) << GetCurrentFPS() << "\n";
  ss << "Avg: " << GetAverageFPS() << "\n";
  ss << "1% Low: " << Get1PercentLowFPS();

  zSTRING text = ss.str().c_str();
  screen->Print(100, 1000, text);
}

float Benchmark::ComputePercentileFPS(float percentile) const {
  if (frame_times_ms_.empty()) {
    return 0.0f;
  }

  // For FPS percentiles, we use the slowest frames (highest frame times)
  // 1% low = bottom 1% of FPS = top 1% of frame times
  std::vector<float> sorted_times = frame_times_ms_;
  std::sort(sorted_times.begin(), sorted_times.end(), std::greater<float>());

  // Get the top percentile of frame times (slowest frames)
  size_t index = static_cast<size_t>(std::ceil(sorted_times.size() * (percentile / 100.0f))) - 1;
  index = std::min(index, sorted_times.size() - 1);

  // Average the slowest frames up to the percentile
  float sum = 0.0f;
  for (size_t i = 0; i <= index; ++i) {
    sum += sorted_times[i];
  }
  float avg_slow_frame_time = sum / static_cast<float>(index + 1);

  return (avg_slow_frame_time > 0.0f) ? (1000.0f / avg_slow_frame_time) : 0.0f;
}

void Benchmark::LogResults() {
  if (frame_times_ms_.empty()) {
    SPDLOG_WARN("[Benchmark] No frame data collected!");
    return;
  }

  // Calculate statistics
  float avg_fps = GetAverageFPS();
  float one_percent_low = ComputePercentileFPS(1.0f);
  float point_one_percent_low = ComputePercentileFPS(0.1f);

  // Calculate frame time stats
  float min_frame_time = *std::min_element(frame_times_ms_.begin(), frame_times_ms_.end());
  float max_frame_time = *std::max_element(frame_times_ms_.begin(), frame_times_ms_.end());
  float total_time_ms = std::accumulate(frame_times_ms_.begin(), frame_times_ms_.end(), 0.0f);
  float avg_frame_time = total_time_ms / static_cast<float>(frame_times_ms_.size());

  // Log to console
  SPDLOG_INFO("==================================================");
  SPDLOG_INFO("             BENCHMARK RESULTS");
  SPDLOG_INFO("==================================================");
  SPDLOG_INFO("Renderer:       {}", renderer_name_);
  SPDLOG_INFO("VSync Disabled: {}", vsync_disabled_ ? "Yes" : "No (D3D7)");
  SPDLOG_INFO("Total Frames:   {}", frame_times_ms_.size());
  SPDLOG_INFO("Duration:       {:.2f}s", total_time_ms / 1000.0f);
  SPDLOG_INFO("--------------------------------------------------");
  SPDLOG_INFO("Average FPS:    {:.2f}", avg_fps);
  SPDLOG_INFO("1% Low FPS:     {:.2f}", one_percent_low);
  SPDLOG_INFO("0.1% Low FPS:   {:.2f}", point_one_percent_low);
  SPDLOG_INFO("--------------------------------------------------");
  SPDLOG_INFO("Avg Frame Time: {:.3f} ms", avg_frame_time);
  SPDLOG_INFO("Min Frame Time: {:.3f} ms ({:.1f} FPS)", min_frame_time, 1000.0f / min_frame_time);
  SPDLOG_INFO("Max Frame Time: {:.3f} ms ({:.1f} FPS)", max_frame_time, 1000.0f / max_frame_time);
  SPDLOG_INFO("==================================================");

  SPDLOG_INFO("Max Frame Time: {:.3f} ms ({:.1f} FPS)", max_frame_time, 1000.0f / max_frame_time);
  SPDLOG_INFO("==================================================");
}

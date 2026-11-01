/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace gmp {

// The source clock need not have the same epoch as the receiver's steady clock.
// Its spacing, rather than packet arrival spacing, defines the presentation path.
class PoseInterpolator {
 public:
  using Vector = std::array<float, 3>;
  struct Pose {
    Vector position{};
    Vector direction{0.0f, 0.0f, 1.0f};
  };
  struct Settings {
    double delay_ms = 120.0;
    double max_extrapolation_ms = 120.0;
    float max_speed = 1500.0f;  // Gothic world units (centimetres) per second.
    float max_extrapolation_distance = 200.0f;
    float teleport_distance = 400.0f;
    std::size_t max_samples = 32;
  };

  PoseInterpolator() = default;
  explicit PoseInterpolator(Settings settings) : settings_(settings) {
    settings_.max_samples = std::clamp<std::size_t>(settings_.max_samples, 2, 128);
    if (!std::isfinite(settings_.delay_ms)) settings_.delay_ms = 120.0;
    settings_.delay_ms = std::clamp(settings_.delay_ms, 0.0, 500.0);
    if (!std::isfinite(settings_.max_extrapolation_ms)) settings_.max_extrapolation_ms = 120.0;
    settings_.max_extrapolation_ms = std::clamp(settings_.max_extrapolation_ms, 0.0, 500.0);
    if (!std::isfinite(settings_.max_speed) || settings_.max_speed < 0.0f) settings_.max_speed = 1500.0f;
    if (!std::isfinite(settings_.max_extrapolation_distance) || settings_.max_extrapolation_distance < 0.0f) settings_.max_extrapolation_distance = 200.0f;
    if (!std::isfinite(settings_.teleport_distance) || settings_.teleport_distance < 0.0f) settings_.teleport_distance = 400.0f;
  }

  void Reset() {
    samples_.clear();
    offset_samples_.clear();
    offset_ms_.reset();
    velocity_ = {};
  }

  bool empty() const { return samples_.empty(); }
  double delay_ms() const { return settings_.delay_ms; }

  bool Push(Pose pose, std::uint64_t source_time_ms, double received_time_ms, bool teleport = false) {
    if (!Finite(pose.position) || !Finite(pose.direction) || !std::isfinite(received_time_ms)) {
      return false;
    }
    const double source = static_cast<double>(source_time_ms);
    if (teleport) Reset();
    if (!samples_.empty() && source < samples_.back().time_ms) {
      return false;
    }
    pose.direction = NormalizeHeading(pose.direction);
    if (!samples_.empty() && source == samples_.back().time_ms) {
      // Multiple newer revisioned server commands can share one clock ms.
      samples_.back().pose = pose;
      return true;
    }
    if (!samples_.empty() && Distance(samples_.back().pose.position, pose.position) > settings_.teleport_distance) {
      Reset();
    }

    // A rolling minimum estimates the baseline clock offset without incorporating
    // temporary queueing delay. Rising offsets are approached gradually.
    offset_samples_.push_back(received_time_ms - source);
    while (offset_samples_.size() > settings_.max_samples) offset_samples_.pop_front();
    const double observed_offset = *std::min_element(offset_samples_.begin(), offset_samples_.end());
    if (!offset_ms_ || observed_offset < *offset_ms_) {
      offset_ms_ = observed_offset;
    } else {
      *offset_ms_ += std::min(1.0, observed_offset - *offset_ms_);
    }

    velocity_ = {};
    if (!samples_.empty()) {
      const double span_ms = source - samples_.back().time_ms;
      if (span_ms >= 1.0 && span_ms <= 1000.0) {
        const float seconds = static_cast<float>(span_ms / 1000.0);
        for (std::size_t axis = 0; axis < 3; ++axis) {
          velocity_[axis] = (pose.position[axis] - samples_.back().pose.position[axis]) / seconds;
        }
        const float speed = Length(velocity_);
        if (speed > settings_.max_speed && speed > 0.0f) {
          for (float& component : velocity_) component *= settings_.max_speed / speed;
        }
      }
    }
    samples_.push_back({pose, source});
    while (samples_.size() > std::max<std::size_t>(2, settings_.max_samples)) samples_.pop_front();
    return true;
  }

  double RenderTime(double local_time_ms) const {
    return offset_ms_ ? local_time_ms - *offset_ms_ - settings_.delay_ms : local_time_ms;
  }

  std::optional<Pose> Evaluate(double local_time_ms) {
    if (samples_.empty() || !std::isfinite(local_time_ms)) return std::nullopt;
    const double render_time = RenderTime(local_time_ms);
    while (samples_.size() >= 2 && samples_[1].time_ms <= render_time) samples_.pop_front();
    Pose result = samples_.front().pose;
    if (samples_.size() >= 2) {
      const Sample& from = samples_[0];
      const Sample& to = samples_[1];
      const float alpha = static_cast<float>(std::clamp((render_time - from.time_ms) / (to.time_ms - from.time_ms), 0.0, 1.0));
      for (std::size_t axis = 0; axis < 3; ++axis) {
        result.position[axis] += (to.pose.position[axis] - from.pose.position[axis]) * alpha;
      }
      const float from_yaw = std::atan2(from.pose.direction[0], from.pose.direction[2]);
      const float to_yaw = std::atan2(to.pose.direction[0], to.pose.direction[2]);
      constexpr float kPi = 3.14159265358979323846f;
      const float arc = std::remainder(to_yaw - from_yaw, 2.0f * kPi);
      const float yaw = from_yaw + arc * alpha;
      result.direction = {std::sin(yaw), 0.0f, std::cos(yaw)};
    } else {
      const double elapsed_ms = render_time - samples_.front().time_ms;
      if (elapsed_ms > 0.0) {
        const float speed = Length(velocity_);
        const float seconds = static_cast<float>(std::min(elapsed_ms, settings_.max_extrapolation_ms) / 1000.0);
        const float distance = std::min(speed * seconds, settings_.max_extrapolation_distance);
        if (speed > 0.0f) {
          for (std::size_t axis = 0; axis < 3; ++axis) result.position[axis] += velocity_[axis] * distance / speed;
        }
      }
      // Once extrapolation expires, hold its capped endpoint until a new source
      // sample arrives. Snapping back at the horizon would reverse motion during
      // every ordinary packet gap. The logical actor cache remains authoritative.
    }
    return result;
  }

 private:
  struct Sample { Pose pose; double time_ms; };
  static bool Finite(const Vector& value) {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
  }
  static float Length(const Vector& value) {
    return std::hypot(value[0], value[1], value[2]);
  }
  static float Distance(const Vector& from, const Vector& to) {
    return std::hypot(to[0] - from[0], to[1] - from[1], to[2] - from[2]);
  }
  static Vector NormalizeHeading(Vector value) {
    const float length = std::hypot(value[0], value[2]);
    return length > 0.0001f ? Vector{value[0] / length, 0.0f, value[2] / length} : Vector{0.0f, 0.0f, 1.0f};
  }
  Settings settings_{};
  std::deque<Sample> samples_;
  std::deque<double> offset_samples_;
  std::optional<double> offset_ms_;
  Vector velocity_{};
};

}  // namespace gmp

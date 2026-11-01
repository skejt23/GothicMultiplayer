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

#include "gothic_weather.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

#include "Shared/lua_runtime/lua_constants.h"

namespace {
constexpr float kInitialRainStartSkyTime = 0.187f;
constexpr float kInitialRainStopSkyTime = 0.25f;
constexpr float kRainMinLength = 0.042f;
constexpr float kRainRandomLength = 0.06f;
constexpr float kRainFadeFraction = 0.2f;
constexpr float kRainFadeEndFraction = 1.0f - kRainFadeFraction;
}  // namespace

GothicWeather::GothicWeather() : rng_(std::random_device{}()) {
}

void GothicWeather::Initialize(const GothicClock::Time& current_time) {
  initialized_ = true;
  rain_start_sky_time_ = kInitialRainStartSkyTime;
  rain_stop_sky_time_ = kInitialRainStopSkyTime;

  const auto [rain_start_hour, rain_start_min] = SkyTimeToClockTime(rain_start_sky_time_);
  state_.rain_start_hour = rain_start_hour;
  state_.rain_start_min = rain_start_min;
  const auto [rain_stop_hour, rain_stop_min] = SkyTimeToClockTime(rain_stop_sky_time_);
  state_.rain_stop_hour = rain_stop_hour;
  state_.rain_stop_min = rain_stop_min;

  ResetLastSkyTime(current_time);
  Update(current_time);

  SPDLOG_INFO("Automatic Gothic weather enabled");
}

bool GothicWeather::Update(const GothicClock::Time& current_time) {
  if (!initialized_) {
    Initialize(current_time);
    return true;
  }

  if (state_.disabled) {
    return false;
  }

  const auto old_state = state_;
  const auto sky_time = ToSkyTime(current_time);
  bool weather_window_changed = false;

  // Gothic rolls a new rain window when sky master time wraps at noon.
  if (last_sky_time_ - sky_time > 0.95f) {
    RollRainWindow();
    weather_window_changed = true;
  }
  last_sky_time_ = sky_time;

  state_.rain_weight = 0.0f;
  if (!state_.dont_rain && sky_time >= rain_start_sky_time_ && sky_time <= rain_stop_sky_time_) {
    const auto duration = rain_stop_sky_time_ - rain_start_sky_time_;
    if (duration > 0.0f) {
      const auto fraction = (sky_time - rain_start_sky_time_) / duration;
      if (fraction < kRainFadeFraction) {
        state_.rain_weight = fraction / kRainFadeFraction;
      } else if (fraction > kRainFadeEndFraction) {
        state_.rain_weight = (1.0f - fraction) / kRainFadeFraction;
      } else {
        state_.rain_weight = 1.0f;
      }
      state_.rain_weight = std::clamp(state_.rain_weight, 0.0f, 1.0f);
    }
  }

  if (state_.rain_weight > 0.0f) {
    state_.weather_type = WEATHER_RAIN;
    if (!raining_) {
      ++rain_counter_;
      raining_ = true;
    }
  } else {
    state_.weather_type = 0;
    raining_ = false;
  }

  return weather_window_changed || old_state.weather_type != state_.weather_type ||
         old_state.rain_start_hour != state_.rain_start_hour || old_state.rain_start_min != state_.rain_start_min ||
         old_state.rain_stop_hour != state_.rain_stop_hour || old_state.rain_stop_min != state_.rain_stop_min ||
         std::fabs(old_state.rain_weight - state_.rain_weight) > 0.01f || old_state.render_lightning != state_.render_lightning ||
         old_state.dont_rain != state_.dont_rain || old_state.disabled != state_.disabled ||
         std::fabs(old_state.wind_scale - state_.wind_scale) > 0.01f;
}

void GothicWeather::ResetLastSkyTime(const GothicClock::Time& current_time) {
  last_sky_time_ = ToSkyTime(current_time);
}

const GothicWeather::State& GothicWeather::GetState() const {
  return state_;
}

bool GothicWeather::SetWeatherType(std::int32_t weather_type) {
  if (weather_type < 0 || weather_type > WEATHER_RAIN) {
    return false;
  }

  state_.weather_type = weather_type;
  state_.rain_weight = weather_type > 0 ? 1.0f : 0.0f;
  state_.render_lightning = false;
  raining_ = state_.rain_weight > 0.0f;
  return true;
}

std::int32_t GothicWeather::GetWeatherType() const {
  return state_.weather_type;
}

bool GothicWeather::SetRainStartTime(std::int32_t hour, std::int32_t min) {
  if (hour < 0 || hour > 23 || min < 0 || min > 59) {
    return false;
  }

  state_.rain_start_hour = hour;
  state_.rain_start_min = min;
  const auto old_duration = std::max(kRainMinLength, rain_stop_sky_time_ - rain_start_sky_time_);
  rain_start_sky_time_ = ToSkyTime(hour, min);
  rain_stop_sky_time_ = std::min(1.0f, rain_start_sky_time_ + old_duration);
  const auto [rain_stop_hour, rain_stop_min] = SkyTimeToClockTime(rain_stop_sky_time_);
  state_.rain_stop_hour = rain_stop_hour;
  state_.rain_stop_min = rain_stop_min;
  return true;
}

std::pair<std::int32_t, std::int32_t> GothicWeather::GetRainStartTime() const {
  return {state_.rain_start_hour, state_.rain_start_min};
}

bool GothicWeather::SetRainStopTime(std::int32_t hour, std::int32_t min) {
  if (hour < 0 || hour > 23 || min < 0 || min > 59) {
    return false;
  }

  const auto rain_stop_sky_time = ToSkyTime(hour, min);
  if (rain_stop_sky_time <= rain_start_sky_time_) {
    return false;
  }

  rain_stop_sky_time_ = rain_stop_sky_time;
  state_.rain_stop_hour = hour;
  state_.rain_stop_min = min;
  return true;
}

std::pair<std::int32_t, std::int32_t> GothicWeather::GetRainStopTime() const {
  return {state_.rain_stop_hour, state_.rain_stop_min};
}

bool GothicWeather::SetWindScale(float wind_scale) {
  if (!std::isfinite(wind_scale)) {
    return false;
  }

  state_.wind_scale = wind_scale;
  return true;
}

float GothicWeather::GetWindScale() const {
  return state_.wind_scale;
}

void GothicWeather::SetDontRain(bool toggle) {
  state_.dont_rain = toggle;
  if (toggle) {
    state_.rain_weight = 0.0f;
    state_.weather_type = 0;
    raining_ = false;
  }
}

bool GothicWeather::GetDontRain() const {
  return state_.dont_rain;
}

void GothicWeather::SetDisabled(bool toggle) {
  state_.disabled = toggle;
}

bool GothicWeather::GetDisabled() const {
  return state_.disabled;
}

std::int64_t GothicWeather::ToTotalGameMinutes(const GothicClock::Time& time) {
  return static_cast<std::int64_t>(time.day_) * 24 * 60 + static_cast<std::int64_t>(time.hour_) * 60 +
         static_cast<std::int64_t>(time.min_);
}

float GothicWeather::ToSkyTime(const GothicClock::Time& time) {
  const auto day_minutes = static_cast<float>(static_cast<std::int32_t>(time.hour_) * 60 + static_cast<std::int32_t>(time.min_));
  auto sky_time = day_minutes / (24.0f * 60.0f) + 0.5f;
  while (sky_time > 1.0f) {
    sky_time -= 1.0f;
  }
  return sky_time;
}

float GothicWeather::ToSkyTime(std::int32_t hour, std::int32_t minute) {
  const auto day_minutes = static_cast<float>(hour * 60 + minute);
  auto sky_time = day_minutes / (24.0f * 60.0f) + 0.5f;
  while (sky_time > 1.0f) {
    sky_time -= 1.0f;
  }
  return sky_time;
}

std::pair<std::int32_t, std::int32_t> GothicWeather::SkyTimeToClockTime(float sky_time) {
  if (!std::isfinite(sky_time)) {
    sky_time = 0.0f;
  }
  sky_time = std::clamp(sky_time, 0.0f, 1.0f);

  auto world_fraction = sky_time - 0.5f;
  if (world_fraction < 0.0f) {
    world_fraction += 1.0f;
  }

  auto total_minutes = static_cast<std::int32_t>(std::round(world_fraction * 24.0f * 60.0f)) % (24 * 60);
  return {total_minutes / 60, total_minutes % 60};
}

void GothicWeather::RollRainWindow() {
  std::uniform_real_distribution<float> random_fraction(0.0f, 1.0f);

  rain_start_sky_time_ = random_fraction(rng_);
  if (rain_start_sky_time_ > 1.0f - kRainMinLength) {
    rain_start_sky_time_ = 1.0f - kRainMinLength;
  }

  rain_stop_sky_time_ = rain_start_sky_time_ + kRainMinLength + random_fraction(rng_) * kRainRandomLength;
  if (rain_stop_sky_time_ > 1.0f) {
    rain_stop_sky_time_ = 1.0f;
  }

  state_.render_lightning = rain_counter_ > 3 && random_fraction(rng_) > 0.6f;

  const auto [rain_start_hour, rain_start_min] = SkyTimeToClockTime(rain_start_sky_time_);
  state_.rain_start_hour = rain_start_hour;
  state_.rain_start_min = rain_start_min;
  const auto [rain_stop_hour, rain_stop_min] = SkyTimeToClockTime(rain_stop_sky_time_);
  state_.rain_stop_hour = rain_stop_hour;
  state_.rain_stop_min = rain_stop_min;

  SPDLOG_DEBUG("Automatic weather window rolled: start {:02}:{:02}, stop {:02}:{:02}, lightning {}", state_.rain_start_hour,
               state_.rain_start_min, state_.rain_stop_hour, state_.rain_stop_min, state_.render_lightning);
}

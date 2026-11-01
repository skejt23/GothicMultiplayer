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

#include <cstdint>
#include <random>
#include <utility>

#include "gothic_clock.h"

class GothicWeather {
public:
  struct State {
    std::int32_t weather_type{0};
    std::int32_t rain_start_hour{0};
    std::int32_t rain_start_min{0};
    std::int32_t rain_stop_hour{0};
    std::int32_t rain_stop_min{0};
    float wind_scale{0.0f};
    bool dont_rain{false};
    bool disabled{false};
    float rain_weight{0.0f};
    bool render_lightning{false};
  };

  GothicWeather();

  void Initialize(const GothicClock::Time& current_time);
  bool Update(const GothicClock::Time& current_time);
  void ResetLastSkyTime(const GothicClock::Time& current_time);

  const State& GetState() const;
  bool SetWeatherType(std::int32_t weather_type);
  std::int32_t GetWeatherType() const;
  bool SetRainStartTime(std::int32_t hour, std::int32_t min);
  std::pair<std::int32_t, std::int32_t> GetRainStartTime() const;
  bool SetRainStopTime(std::int32_t hour, std::int32_t min);
  std::pair<std::int32_t, std::int32_t> GetRainStopTime() const;
  bool SetWindScale(float wind_scale);
  float GetWindScale() const;
  void SetDontRain(bool toggle);
  bool GetDontRain() const;
  void SetDisabled(bool toggle);
  bool GetDisabled() const;

  static std::int64_t ToTotalGameMinutes(const GothicClock::Time& time);

private:
  static float ToSkyTime(const GothicClock::Time& time);
  static float ToSkyTime(std::int32_t hour, std::int32_t minute);
  static std::pair<std::int32_t, std::int32_t> SkyTimeToClockTime(float sky_time);

  void RollRainWindow();

  State state_;
  bool initialized_{false};
  float rain_start_sky_time_{0.187f};
  float rain_stop_sky_time_{0.25f};
  float last_sky_time_{0.0f};
  std::int32_t rain_counter_{0};
  bool raining_{false};
  std::mt19937 rng_;
};

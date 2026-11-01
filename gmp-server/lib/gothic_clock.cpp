
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include "gothic_clock.h"

#include <spdlog/spdlog.h>

#include <chrono>

#include "server_events.h"
#include "shared/event.h"

GothicClock::GothicClock(Time initial_time, double seconds_per_game_minute)
    : time_(initial_time), seconds_per_game_minute_(seconds_per_game_minute) {
  last_update_time_ = std::chrono::steady_clock::now();
  if (seconds_per_game_minute_ > 0.0) {
    day_length_ms_ = seconds_per_game_minute_ * 24.0 * 60.0 * 1000.0;
  }
  EventManager::Instance().RegisterEvent(kEventOnClockUpdateName);
  if (seconds_per_game_minute_ == 0.0) {
    SPDLOG_INFO("Gothic clock is frozen (seconds_per_game_minute = 0)");
  } else {
    SPDLOG_INFO("Gothic clock: 1 game minute every {} real-world second(s)", seconds_per_game_minute_);
  }
}

std::vector<GothicClock::Time> GothicClock::RunClock() {
  std::vector<Time> advanced_times;

  // If seconds_per_game_minute is 0, time is frozen - do nothing
  if (seconds_per_game_minute_ == 0.0) {
    return advanced_times;
  }

  auto now = std::chrono::steady_clock::now();
  auto interval = std::chrono::duration<double>(seconds_per_game_minute_);
  auto elapsed = now - last_update_time_;
  if (elapsed >= interval) {
    const auto minutes_to_advance = static_cast<int>(elapsed / interval);
    auto advance_minute = [this]() {
      if (++time_.min_ > 59) {
        time_.min_ = 0;
        if (++time_.hour_ > 23) {
          time_.hour_ = 0;
          time_.day_++;
        }
      }
    };

    for (int i = 0; i < minutes_to_advance; ++i) {
      advance_minute();
      advanced_times.push_back(time_);
      EventManager::Instance().TriggerEvent(kEventOnClockUpdateName, OnClockUpdateEvent{time_.day_, time_.hour_, time_.min_});
    }

    last_update_time_ += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval * minutes_to_advance);
  }

  return advanced_times;
}

void GothicClock::UpdateTime(GothicClock::Time new_time) {
  time_ = new_time;
  SPDLOG_DEBUG("Gothic clock updated to {}", time_);
}

GothicClock::Time GothicClock::GetTime() const {
  Time current_time;
  current_time = time_;
  return current_time;
}

bool GothicClock::SetDayLengthMs(double day_length_ms) {
  day_length_ms_ = day_length_ms;
  if (day_length_ms_ <= 0.0) {
    seconds_per_game_minute_ = 0.0;
    SPDLOG_INFO("Gothic clock is frozen (day length = 0)");
  } else {
    seconds_per_game_minute_ = day_length_ms_ / (24.0 * 60.0 * 1000.0);
    SPDLOG_INFO("Gothic clock: day length {} ms ({} s per game minute)", day_length_ms_, seconds_per_game_minute_);
  }

  last_update_time_ = std::chrono::steady_clock::now();
  return true;
}

double GothicClock::GetDayLengthMs() const {
  return day_length_ms_;
}

std::ostream& operator<<(std::ostream& os, const GothicClock::Time& d) {
  return os << d.day_ << '-' << static_cast<std::int32_t>(d.hour_) << ':' << static_cast<std::int32_t>(d.min_);
}

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

#include "lua_sky.h"

#include "Shared/lua_runtime/bind_helpers.h"
#include "game_server.h"

namespace lua::bindings {

/* luagmp (class)
 *
 * Static sky controller for authoritative server weather.
 *
 * @version  0.3.0
 * @name     Sky
 * @side     server
 * @category World
 *
 */
class LuaSky {
public:
  /// Assigns the weatherType property using the same value format as its getter.
  void SetWeatherType(int weather_type) const {
    g_server->SetWeatherType(weather_type);
  }
  /* luagmp (property)
   *
   * Current server weather type. When automatic weather is enabled, this can be overwritten on the next server game minute. For more information, see
   * [Weather Constants](../../shared-constants/Weather.md).
   *
   * @name     weatherType
   * @return   (number)
   */
  int GetWeatherType() const {
    return g_server->GetWeatherType();
  }

  /// Assigns the rainStartTime property using the same value format as its getter.
  void SetRainStartTime(const sol::object& value) const {
    if (auto time = ::lua::bind_helpers::ReadTimeTable(value)) {
      g_server->SetRainStartTime(time->first, time->second);
    }
  }

  /* luagmp (property)
   *
   * Rain start time: `{hour = number, minute = number}`
   *
   * @name     rainStartTime
   * @return   (table)
   */
  sol::object GetRainStartTime(sol::this_state ts) const {
    const auto time = g_server->GetRainStartTime();
    return ::lua::bind_helpers::MakeTimeTable(ts, time.first, time.second);
  }

  /// Assigns the rainStopTime property using the same value format as its getter.
  void SetRainStopTime(const sol::object& value) const {
    if (auto time = ::lua::bind_helpers::ReadTimeTable(value)) {
      g_server->SetRainStopTime(time->first, time->second);
    }
  }

  /* luagmp (property)
   *
   * Rain stop time: `{hour = number, minute = number}`
   *
   * @note     The stop time must be after `rainStartTime` in Gothic sky time.
   * @name     rainStopTime
   * @return   (table)
   */
  sol::object GetRainStopTime(sol::this_state ts) const {
    const auto time = g_server->GetRainStopTime();
    return ::lua::bind_helpers::MakeTimeTable(ts, time.first, time.second);
  }

  /// Assigns the windScale property using the same value format as its getter.
  void SetWindScale(float wind_scale) const {
    g_server->SetWindScale(wind_scale);
  }
  /* luagmp (property)
   *
   * Wind scale used during precipitation.
   *
   * @name     windScale
   * @return   (number)
   */
  float GetWindScale() const {
    return g_server->GetWindScale();
  }

  /// Assigns the precipitationEnabled property using the same value format as its getter.
  void SetPrecipitationEnabled(bool enabled) const {
    g_server->SetDontRain(!enabled);
  }
  /* luagmp (property)
   *
   * Disables rain and snow rendering while leaving automatic weather timing active.
   *
   * @name     precipitationEnabled
   * @return   (boolean)
   */
  bool GetPrecipitationEnabled() const {
    return !g_server->GetDontRain();
  }

  /// Assigns the automaticWeatherEnabled property using the same value format as its getter.
  void SetAutomaticWeatherEnabled(bool enabled) const {
    g_server->SetWeatherDisabled(!enabled);
  }
  /* luagmp (property)
   *
   * Toggles the server automatic weather state machine. Set this to false before manually controlling weather.
   *
   * @name     automaticWeatherEnabled
   * @return   (boolean)
   */
  bool GetAutomaticWeatherEnabled() const {
    return !g_server->GetWeatherDisabled();
  }
};

void BindSky(sol::state& lua) {
  auto sky_type = lua.new_usertype<LuaSky>("Sky", sol::no_constructor);

  sky_type["weatherType"] = sol::property(&LuaSky::GetWeatherType, &LuaSky::SetWeatherType);
  sky_type["rainStartTime"] = sol::property(&LuaSky::GetRainStartTime, &LuaSky::SetRainStartTime);
  sky_type["rainStopTime"] = sol::property(&LuaSky::GetRainStopTime, &LuaSky::SetRainStopTime);
  sky_type["windScale"] = sol::property(&LuaSky::GetWindScale, &LuaSky::SetWindScale);
  sky_type["precipitationEnabled"] = sol::property(&LuaSky::GetPrecipitationEnabled, &LuaSky::SetPrecipitationEnabled);
  sky_type["automaticWeatherEnabled"] = sol::property(&LuaSky::GetAutomaticWeatherEnabled, &LuaSky::SetAutomaticWeatherEnabled);

  lua["Sky"] = LuaSky{};
}

}  // namespace lua::bindings

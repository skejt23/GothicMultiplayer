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

#include <optional>
#include <string>

#include "Shared/lua_runtime/bind_helpers.h"
#include "net_game.h"
#include "sky_utils.h"

namespace gmp::gothic {
namespace {

struct Color3 {
  int r;
  int g;
  int b;
};

struct Color4 {
  int r;
  int g;
  int b;
  int a;
};

std::optional<Color3> ReadColor3Table(const sol::table& table) {
  sol::object r = table["r"];
  sol::object g = table["g"];
  sol::object b = table["b"];
  if (!r.is<int>() || !g.is<int>() || !b.is<int>()) {
    return std::nullopt;
  }

  return Color3{r.as<int>(), g.as<int>(), b.as<int>()};
}

std::optional<Color4> ReadColor4Table(const sol::table& table) {
  auto color = ReadColor3Table(table);
  sol::object a = table["a"];
  if (!color.has_value() || !a.is<int>()) {
    return std::nullopt;
  }

  return Color4{color->r, color->g, color->b, a.as<int>()};
}

}  // namespace

/* luagmp (class)
 *
 * Static sky controller for local and synchronized sky settings.
 *
 * @version  0.3.0
 * @name     Sky
 * @side     client
 * @category World
 *
 */
class LuaSky {
public:
  /// Assigns the weatherType property using the same value format as its getter.
  void SetWeatherType(int weather_type) const {
    ApplyWeatherType(weather_type);
  }
  /* luagmp (property)
   *
   * Current local weather type. Use `requestTimeAndSkySync` to pull the authoritative value from the server. For more
   * information, see [Weather Constants](../../shared-constants/Weather.md).
   *
   * @name     weatherType
   * @return   (number)
   */
  int GetWeatherType() const {
    return gmp::gothic::GetWeatherType();
  }

  /// Assigns the rainStartTime property using the same value format as its getter.
  void SetRainStartTime(const sol::object& value) const {
    if (auto time = ::lua::bind_helpers::ReadTimeTable(value)) {
      gmp::gothic::SetRainStartTime(time->first, time->second);
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
    const auto time = gmp::gothic::GetRainStartTime();
    return ::lua::bind_helpers::MakeTimeTable(ts, time ? time->hour : 0, time ? time->min : 0);
  }

  /// Assigns the rainStopTime property using the same value format as its getter.
  void SetRainStopTime(const sol::object& value) const {
    if (auto time = ::lua::bind_helpers::ReadTimeTable(value)) {
      gmp::gothic::SetRainStopTime(time->first, time->second);
    }
  }

  /* luagmp (property)
   *
   * Rain stop time: `{hour = number, minute = number}`
   *
   * @name     rainStopTime
   * @return   (table)
   */
  sol::object GetRainStopTime(sol::this_state ts) const {
    const auto time = gmp::gothic::GetRainStopTime();
    return ::lua::bind_helpers::MakeTimeTable(ts, time ? time->hour : 0, time ? time->min : 0);
  }

  /// Assigns the windScale property using the same value format as its getter.
  void SetWindScale(float wind_scale) const {
    gmp::gothic::SetWindScale(wind_scale);
  }
  /* luagmp (property)
   *
   * Wind scale used during precipitation.
   *
   * @name     windScale
   * @return   (number)
   */
  float GetWindScale() const {
    return gmp::gothic::GetWindScale();
  }

  /// Assigns the precipitationEnabled property using the same value format as its getter.
  void SetPrecipitationEnabled(bool enabled) const {
    gmp::gothic::SetDontRain(!enabled);
  }
  /* luagmp (property)
   *
   * Toggles local rain and snow rendering.
   *
   * @name     precipitationEnabled
   * @return   (boolean)
   */
  bool GetPrecipitationEnabled() const {
    return !gmp::gothic::GetDontRain();
  }

  /* luagmp (method)
   * Sets one local fog color entry to the supplied RGB color.
   * @name setFogColor
   * @param (number) id Fog entry or sky planet id.
   * @param (table) value RGB table {r, g, b}.
   */
  void SetFogColor(int id, sol::table value) const {
    const auto color = ReadColor3Table(value);
    if (!color)
      throw sol::error("Expected an RGB color table");
    gmp::gothic::SetFogColor(id, color->r, color->g, color->b);
  }

  /* luagmp (method)
   * Sets the local cloud RGB color.
   * @name setCloudsColor
   * @param (table) value RGB table {r, g, b}.
   */
  void SetCloudsColor(sol::table value) const {
    const auto color = ReadColor3Table(value);
    if (!color)
      throw sol::error("Expected an RGB color table");
    gmp::gothic::SetCloudsColor(color->r, color->g, color->b);
  }

  /* luagmp (method)
   * Sets the size of one sky planet.
   * @name setPlanetSize
   * @param (number) id Fog entry or sky planet id.
   * @param (number) size Planet size.
   */
  void SetPlanetSize(int id, float size) const {
    gmp::gothic::SetPlanetSize(id, size);
  }

  /* luagmp (method)
   * Sets one sky planet RGBA color.
   * @name setPlanetColor
   * @param (number) id Fog entry or sky planet id.
   * @param (table) value RGBA table {r, g, b, a}.
   */
  void SetPlanetColor(int id, sol::table value) const {
    const auto color = ReadColor4Table(value);
    if (!color)
      throw sol::error("Expected an RGBA color table");
    gmp::gothic::SetPlanetColor(id, color->r, color->g, color->b, color->a);
  }

  /* luagmp (method)
   * Sets the texture file of one sky planet.
   * @name setPlanetTexture
   * @param (number) id Fog entry or sky planet id.
   * @param (string) file Texture file name.
   */
  void SetPlanetTexture(int id, const std::string& file) const {
    gmp::gothic::SetPlanetTexture(id, file);
  }

  /* luagmp (method)
   * Sets the local sky lighting RGB color.
   * @name setLightingColor
   * @param (table) value RGB table {r, g, b}.
   */
  void SetLightingColor(sol::table value) const {
    const auto color = ReadColor3Table(value);
    if (!color)
      throw sol::error("Expected an RGB color table");
    gmp::gothic::SetLightingColor(color->r, color->g, color->b);
  }

  /* luagmp (method)
   * Requests authoritative time and sky settings; they are applied asynchronously when the reply arrives.
   * @name requestTimeAndSkySync
   * @return (boolean) Whether the request was sent; completion is asynchronous.
   */
  bool RequestTimeAndSkySync() const {
    auto& net_game = NetGame::Instance();
    if (!net_game.IsConnected())
      return false;
    net_game.SyncGameTime();
    return true;
  }
};

void BindSky(sol::state& lua) {
  auto sky_type = lua.new_usertype<LuaSky>("Sky", sol::no_constructor);

  sky_type["weatherType"] = sol::property(&LuaSky::GetWeatherType, &LuaSky::SetWeatherType);
  sky_type["rainStartTime"] = sol::property(&LuaSky::GetRainStartTime, &LuaSky::SetRainStartTime);
  sky_type["rainStopTime"] = sol::property(&LuaSky::GetRainStopTime, &LuaSky::SetRainStopTime);
  sky_type["windScale"] = sol::property(&LuaSky::GetWindScale, &LuaSky::SetWindScale);
  sky_type["precipitationEnabled"] = sol::property(&LuaSky::GetPrecipitationEnabled, &LuaSky::SetPrecipitationEnabled);
  sky_type["setFogColor"] = &LuaSky::SetFogColor;
  sky_type["setCloudsColor"] = &LuaSky::SetCloudsColor;
  sky_type["setPlanetSize"] = &LuaSky::SetPlanetSize;
  sky_type["setPlanetColor"] = &LuaSky::SetPlanetColor;
  sky_type["setPlanetTexture"] = &LuaSky::SetPlanetTexture;
  sky_type["setLightingColor"] = &LuaSky::SetLightingColor;
  sky_type["requestTimeAndSkySync"] = &LuaSky::RequestTimeAndSkySync;

  lua["Sky"] = LuaSky{};
}

}  // namespace gmp::gothic

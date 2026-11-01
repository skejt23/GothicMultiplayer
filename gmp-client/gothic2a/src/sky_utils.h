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

#include <optional>
#include <string>

namespace gmp::gothic {

struct RainStartTime {
  int hour;
  int min;
};

bool ApplyWeatherType(int weather_type);
int GetWeatherType();
bool SetRainWeight(float weight, int weather_type, bool render_lightning);
bool SetRainStartTime(int hour, int min);
std::optional<RainStartTime> GetRainStartTime();
bool SetRainStopTime(int hour, int min);
std::optional<RainStartTime> GetRainStopTime();
bool SetWindScale(float wind_scale);
float GetWindScale();
bool SetDontRain(bool toggle);
bool GetDontRain();
bool SetFogColor(int id, int r, int g, int b);
bool SetCloudsColor(int r, int g, int b);
bool SetPlanetSize(int planet_id, float size);
bool SetPlanetColor(int planet_id, int r, int g, int b, int a);
bool SetPlanetTexture(int planet_id, const std::string& texture);
bool SetLightingColor(int r, int g, int b);

}  // namespace gmp::gothic

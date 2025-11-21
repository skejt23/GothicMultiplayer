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

#include "gothic_bindings.h"

#include <optional>
#include <tuple>
#include <spdlog/spdlog.h>

#include "ZenGin/zGothicAPI.h"
#include "discord_presence.h"
#include "client_resources/process_input.h"

#include "lua_draw.h"
#include "lua_texture.h"
#include "lua_sound.h"

namespace gmp::gothic {
namespace {
sol::optional<std::string> GetOptionalString(const sol::table& table, const char* lowerKey, const char* upperKey) {
  if (auto value = table.get<sol::optional<std::string>>(lowerKey); value) {
    return value;
  }
  return table.get<sol::optional<std::string>>(upperKey);
}

}  // namespace

void BindInput(sol::state& lua) {
    // KEY_* constants
#define BIND_KEY(name) lua[#name] = name;
    G2_KEY_LIST(BIND_KEY)
#undef BIND_KEY

    // MOUSE_* constants
#define BIND_MOUSE(name) lua[#name] = name;
    G2_MOUSE_LIST(BIND_MOUSE)
#undef BIND_MOUSE

    // GAME_* constants (logical actions, separate set)
#define BIND_GAME(name) lua[#name] = name;
    G2_GAME_LIST(BIND_GAME)
#undef BIND_GAME


  lua.set_function("KeyPressed", [](int key) -> bool {
      if (key < 0 || key > MAX_KEYS_AND_CODES) return false;
      return s_pressedThisFrame[key];
  });

  lua.set_function("KeyToggled", [](int key) -> bool {
      if (key < 0 || key > MAX_KEYS_AND_CODES) return false;
      return s_toggledThisFrame[key];
  });
}

void BindDiscord(sol::state& lua) {
  auto discord = lua.create_table("Discord");
  
  discord.set_function("SetActivity", [](const sol::table& params) {
    auto state = GetOptionalString(params, "state", "State").value_or("");
    auto details = GetOptionalString(params, "details", "Details").value_or("");
    auto large_image_key = GetOptionalString(params, "largeImageKey", "LargeImageKey").value_or("");
    auto large_image_text = GetOptionalString(params, "largeImageText", "LargeImageText").value_or("");
    auto small_image_key = GetOptionalString(params, "smallImageKey", "SmallImageKey").value_or("");
    auto small_image_text = GetOptionalString(params, "smallImageText", "SmallImageText").value_or("");

    DiscordRichPresence::Instance().UpdateActivity(state, details, 0, 0, large_image_key, large_image_text, 
                                                    small_image_key, small_image_text);
  });
}

void BindDraw(sol::state& lua) {
  sol::usertype<LuaDraw> draw_type = lua.new_usertype<LuaDraw>(
      "Draw",
      sol::constructors<LuaDraw(), LuaDraw(int, int, const std::string&)>());

  draw_type[sol::meta_function::call] = sol::overload(
      []() { return LuaDraw(); },
      [](int x, int y, const std::string& text) { return LuaDraw(x, y, text); });

  draw_type["setPosition"] = &LuaDraw::setPosition;
  draw_type["getPosition"] = &LuaDraw::getPosition;
  draw_type["setPositionPx"] = &LuaDraw::setPositionPx;
  draw_type["getPositionPx"] = &LuaDraw::getPositionPx;

  draw_type["setText"] = &LuaDraw::setText;
  draw_type["getText"] = &LuaDraw::getText;

  draw_type["setFont"] = &LuaDraw::setFont;
  draw_type["getFont"] = &LuaDraw::getFont;

  draw_type["setColor"] = &LuaDraw::setColor;
  draw_type["getColor"] = &LuaDraw::getColor;

  draw_type["setAlpha"] = &LuaDraw::setAlpha;
  draw_type["getAlpha"] = &LuaDraw::getAlpha;

  draw_type["setVisible"] = &LuaDraw::setVisible;
  draw_type["getVisible"] = &LuaDraw::getVisible;

  draw_type["render"] = &LuaDraw::render;

  // Properties (Lua table access)
  draw_type["position"] = sol::property(&LuaDraw::getPosition, &LuaDraw::setPosition);
  draw_type["positionPx"] = sol::property(&LuaDraw::getPositionPx, &LuaDraw::setPositionPx);
  draw_type["text"] = sol::property(&LuaDraw::getText, &LuaDraw::setText);
  draw_type["font"] = sol::property(&LuaDraw::getFont, &LuaDraw::setFont);
  draw_type["color"] = sol::property(&LuaDraw::getColor);
  draw_type["alpha"] = sol::property(&LuaDraw::getAlpha, &LuaDraw::setAlpha);
  draw_type["visible"] = sol::property(&LuaDraw::getVisible, &LuaDraw::setVisible);

}

void BindTexture(sol::state& lua) {
  sol::usertype<LuaTexture> texture_type = lua.new_usertype<LuaTexture>(
      "Texture",
      sol::constructors<LuaTexture(int, int, int, int, const std::string&)>());

  texture_type[sol::meta_function::call] =
      [](int x, int y, int width, int height, const std::string& file) {
        return LuaTexture(x, y, width, height, file);
      };

  texture_type["setPosition"] = &LuaTexture::setPosition;
  texture_type["getPosition"] = &LuaTexture::getPosition;
  texture_type["setPositionPx"] = &LuaTexture::setPositionPx;
  texture_type["getPositionPx"] = &LuaTexture::getPositionPx;

  texture_type["setSize"] = &LuaTexture::setSize;
  texture_type["getSize"] = &LuaTexture::getSize;
  texture_type["setSizePx"] = &LuaTexture::setSizePx;
  texture_type["getSizePx"] = &LuaTexture::getSizePx;

  texture_type["setRect"] = &LuaTexture::setRect;
  texture_type["getRect"] = &LuaTexture::getRect;
  texture_type["setRectPx"] = &LuaTexture::setRectPx;
  texture_type["getRectPx"] = &LuaTexture::getRectPx;

  texture_type["setColor"] = &LuaTexture::setColor;
  texture_type["getColor"] = &LuaTexture::getColor;
  texture_type["setAlpha"] = &LuaTexture::setAlpha;
  texture_type["getAlpha"] = &LuaTexture::getAlpha;

  texture_type["setVisible"] = &LuaTexture::setVisible;
  texture_type["getVisible"] = &LuaTexture::getVisible;
  texture_type["setFile"] = &LuaTexture::setFile;
  texture_type["getFile"] = &LuaTexture::getFile;
  
  texture_type["top"] = &LuaTexture::top;

  texture_type["render"] = &LuaTexture::render;

  // Properties (Lua table access)
  texture_type["position"] = sol::property(&LuaTexture::getPosition, &LuaTexture::setPosition);
  texture_type["positionPx"] = sol::property(&LuaTexture::getPositionPx, &LuaTexture::setPositionPx);
  texture_type["size"] = sol::property(&LuaTexture::getSize, &LuaTexture::setSize);
  texture_type["sizePx"] = sol::property(&LuaTexture::getSizePx, &LuaTexture::setSizePx);
  texture_type["rect"] = sol::property(&LuaTexture::getRect, &LuaTexture::setRect);
  texture_type["rectPx"] = sol::property(&LuaTexture::getRectPx, &LuaTexture::setRectPx);
  texture_type["color"] = sol::property(&LuaTexture::getColor, &LuaTexture::setColor);
  texture_type["alpha"] = sol::property(&LuaTexture::getAlpha, &LuaTexture::setAlpha);
  texture_type["visible"] = sol::property(&LuaTexture::getVisible, &LuaTexture::setVisible);
  texture_type["file"] = sol::property(&LuaTexture::getFile, &LuaTexture::setFile);
}

void BindSound(sol::state& lua) {
  sol::usertype<LuaSound> sound_type = lua.new_usertype<LuaSound>(
      "Sound",
      sol::constructors<LuaSound(const std::string&)>());

  sound_type["play"] = &LuaSound::play;
  sound_type["stop"] = &LuaSound::stop;
  sound_type["isPlaying"] = &LuaSound::isPlaying;

  sound_type["file"] = sol::property(&LuaSound::getFile, &LuaSound::setFile);
  sound_type["playingTime"] = sol::property(&LuaSound::getPlayingTime);
  sound_type["volume"] = sol::property(&LuaSound::getVolume, &LuaSound::setVolume);
  sound_type["looping"] = sol::property(&LuaSound::getLooping, &LuaSound::setLooping);
  sound_type["balance"] = sol::property(&LuaSound::getBalance, &LuaSound::setBalance);
}

void BindTime(sol::state& lua) {
  lua.set_function("setTime", [](int hour, int minute) {
    if (!ogame || !ogame->GetWorldTimer()) {
      return;
    }

    ogame->GetWorldTimer()->SetTime(hour, minute);
  });

  lua.set_function("getTime", []() {
    int hour = 0;
    int minute = 0;

    if (ogame && ogame->GetWorldTimer()) {
      ogame->GetWorldTimer()->GetTime(hour, minute);
    }

    return std::make_tuple(hour, minute);
  });

  lua.set_function("setDayLength", [](float day_length_seconds) {
    //
  });

  lua.set_function("getDayLength", []() {
    //
  });
}

void BindGothicSpecific(sol::state& lua) {
  SPDLOG_TRACE("Initializing Gothic 2 Addon 2.6 specific bindings...");

  BindInput(lua);
  BindDiscord(lua);
  BindDraw(lua);
  BindTexture(lua);
  BindSound(lua);
  BindTime(lua);
}

void CleanupGothicViews() {
  LuaDraw::CleanupViews();
  LuaTexture::CleanupViews();
}

}  // namespace gmp::gothic

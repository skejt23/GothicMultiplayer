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

#include "lua_discord.h"

#include "Shared/lua_runtime/bind_helpers.h"
#include "lua_helpers.h"

namespace gmp::gothic {

/* luagmp (class)
 *
 * This class exposes static methods for updating the user's Discord activity from the game client.
 *
 * @version  0.3.0
 * @name     Discord
 * @side     client
 * @category Game
 *
 */

/* luagmp (method)
 *
 * This function updates the Discord Rich Presence activity. Missing fields keep their last-set values.
 *
 * @version  0.3.0
 * @name     setActivity
 * @static
 * @param    ({...}) Activity configuration table.
 *
 */
void Function_Discord_SetActivity(const sol::table& params) {
  auto& activity = lua_helpers::GetDiscordActivityState();

  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "state", "State"); value) {
    activity.state = *value;
  }
  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "details", "Details"); value) {
    activity.details = *value;
  }
  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "largeImageKey", "LargeImageKey"); value) {
    activity.large_image_key = *value;
  }
  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "largeImageText", "LargeImageText"); value) {
    activity.large_image_text = *value;
  }
  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "smallImageKey", "SmallImageKey"); value) {
    activity.small_image_key = *value;
  }
  if (auto value = ::lua::bind_helpers::GetOptionalString(params, "smallImageText", "SmallImageText"); value) {
    activity.small_image_text = *value;
  }

  lua_helpers::ApplyDiscordActivityState(activity);
}

/* luagmp (method)
 *
 * This function will update the activity state text.
 *
 * @version  0.3.0
 * @name     setState
 * @static
 * @param    (string) state
 *
 */
void Function_Discord_SetState(const std::string& state) {
  auto& activity = lua_helpers::GetDiscordActivityState();
  activity.state = state;
  lua_helpers::ApplyDiscordActivityState(activity);
}

/* luagmp (method)
 *
 * This function will update the activity details text.
 *
 * @version  0.3.0
 * @name     setDetails
 * @static
 * @param    (string) details
 *
 */
void Function_Discord_SetDetails(const std::string& details) {
  auto& activity = lua_helpers::GetDiscordActivityState();
  activity.details = details;
  lua_helpers::ApplyDiscordActivityState(activity);
}

/* luagmp (method)
 *
 * This function will update the large image entry for the activity.
 *
 * @version  0.3.0
 * @name     setLargeImage
 * @static
 * @param    (string) key    Asset key for the large image.
 * @param    (string) text   Optional tooltip text for the large image.
 *
 */
void Function_Discord_SetLargeImage(const std::string& key, const sol::optional<std::string>& text) {
  auto& activity = lua_helpers::GetDiscordActivityState();
  activity.large_image_key = key;
  if (text) {
    activity.large_image_text = *text;
  }
  lua_helpers::ApplyDiscordActivityState(activity);
}

/* luagmp (method)
 *
 * This function will update the small image entry for the activity.
 *
 * @version  0.3.0
 * @name     setSmallImage
 * @static
 * @param    (string) key    Asset key for the small image.
 * @param    (string) text   Optional tooltip text for the small image.
 *
 */
void Function_Discord_SetSmallImage(const std::string& key, const sol::optional<std::string>& text) {
  auto& activity = lua_helpers::GetDiscordActivityState();
  activity.small_image_key = key;
  if (text) {
    activity.small_image_text = *text;
  }
  lua_helpers::ApplyDiscordActivityState(activity);
}

/* luagmp (method)
 *
 * This function will clear the current activity and stored values.
 *
 * @version  0.3.0
 * @name     clearActivity
 * @static
 *
 */
void Function_Discord_ClearActivity() {
  lua_helpers::ClearDiscordActivityState();
}

void BindDiscord(sol::state& lua) {
  auto discord = lua.create_table("Discord");
  discord.set_function("setActivity", Function_Discord_SetActivity);
  discord.set_function("setState", Function_Discord_SetState);
  discord.set_function("setDetails", Function_Discord_SetDetails);
  discord.set_function("setLargeImage", Function_Discord_SetLargeImage);
  discord.set_function("setSmallImage", Function_Discord_SetSmallImage);
  discord.set_function("clearActivity", Function_Discord_ClearActivity);
}

}  // namespace gmp::gothic

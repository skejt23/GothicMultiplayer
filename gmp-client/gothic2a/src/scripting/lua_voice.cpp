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

#include "lua_voice.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"
#include "net_game.h"

namespace gmp::gothic {
namespace {

bool IsValidVoicePlayerId(std::int64_t player_id) {
  return player_id > 0 && static_cast<std::uint64_t>(player_id) <= std::numeric_limits<std::uint32_t>::max();
}

}  // namespace

/* luagmp (class)
 *
 * This singleton class controls native proximity voice chat for the local client.
 *
 * @version  0.3.0
 * @name     Voice
 * @side     client
 * @category Game
 *
 */

/* luagmp (method)
 *
 * This function checks whether the server offers voice chat.
 *
 * @name     isAvailable
 * @return   (boolean) True when the server has proximity voice chat enabled.
 *
 */
bool LuaVoice::isAvailable() {
  return NetGame::Instance().IsVoiceChatAvailable();
}

/* luagmp (method)
 *
 * This function checks whether voice chat is effectively enabled for this client.
 *
 * @name     isEnabled
 * @return   (boolean) True when server, user, and script settings all allow voice chat.
 *
 */
bool LuaVoice::isEnabled() {
  return NetGame::Instance().IsVoiceChatEnabled();
}

/* luagmp (method)
 *
 * This function changes the client-wide script voice chat gate. Enabling it does not override a disabled user setting.
 *
 * @name     setScriptEnabled
 * @param    (boolean) enabled  Whether scripts permit voice chat; all resources share this gate.
 * @return   (boolean) The resulting effective voice chat state.
 *
 */
bool LuaVoice::setScriptEnabled(bool enabled) {
  NetGame& net_game = NetGame::Instance();
  net_game.SetVoiceChatScriptEnabled(enabled);
  return net_game.IsVoiceChatEnabled();
}

/* luagmp (method)
 *
 * This function returns the effective local proximity voice range.
 *
 * @name     getRange
 * @return   (number) Voice range in game units, or zero when unavailable.
 *
 */
float LuaVoice::getRange() {
  return NetGame::Instance().GetVoiceChatRange();
}

/* luagmp (method)
 *
 * This function limits the local client's proximity voice range. The server's
 * range remains authoritative, so this setting can only reduce the range.
 *
 * @name     setLocalRangeLimit
 * @param    (number) range  Maximum local voice range in game units.
 * @return   (boolean)       True when the range was accepted.
 *
 */
bool LuaVoice::setLocalRangeLimit(float range) {
  return NetGame::Instance().SetVoiceChatRange(range);
}

/* luagmp (method)
 *
 * This function returns the script output-volume multiplier.
 *
 * @name     getOutputVolume
 * @return   (number) Multiplier from 0.0 to 1.0, applied after the user's volume setting.
 *
 */
float LuaVoice::getOutputVolume() {
  return NetGame::Instance().GetVoiceChatOutputVolume();
}

/* luagmp (method)
 *
 * This function changes the script output-volume multiplier.
 *
 * @name     setOutputVolume
 * @param    (number) volume  Multiplier from 0.0 to 1.0, clamped to that range.
 *
 */
void LuaVoice::setOutputVolume(float volume) {
  NetGame::Instance().SetVoiceChatOutputVolume(volume);
}

/* luagmp (method)
 *
 * This function returns the active push-to-talk key code.
 *
 * @name     getPushToTalkKey
 * @return   (number) Key code used for push-to-talk.
 *
 */
int LuaVoice::getPushToTalkKey() {
  return NetGame::Instance().GetVoiceChatPushToTalkKey();
}

/* luagmp (method)
 *
 * This function sets the local client's push-to-talk key for this resource.
 *
 * @name     setPushToTalkKey
 * @param    (number) key  Keyboard key code in the range 1-255.
 * @return   (boolean)      True when the key was accepted.
 *
 */
bool LuaVoice::setPushToTalkKey(int key) {
  return NetGame::Instance().SetVoiceChatPushToTalkKey(key);
}

/* luagmp (method)
 *
 * This function returns the available microphone device names.
 *
 * @name     getInputDevices
 * @return   ({string...}) Available microphone names.
 *
 */
sol::as_table_t<std::vector<std::string>> LuaVoice::getInputDevices() {
  return sol::as_table(NetGame::Instance().GetVoiceChatInputDevices());
}

/* luagmp (method)
 *
 * This function returns the selected microphone name. An empty string means the system default.
 *
 * @name     getInputDevice
 * @return   (string) Selected microphone name.
 *
 */
std::string LuaVoice::getInputDevice() {
  return NetGame::Instance().GetVoiceChatInputDevice();
}

/* luagmp (method)
 *
 * This function selects a microphone by name. Pass an empty string to restore the system default.
 *
 * @name     setInputDevice
 * @param    (string) device_name  Microphone name returned by `getInputDevices`, or an empty string.
 * @return   (boolean)              True when the device was selected.
 *
 */
bool LuaVoice::setInputDevice(const std::string& device_name) {
  return NetGame::Instance().SetVoiceChatInputDevice(device_name);
}

/* luagmp (method)
 *
 * This function returns the available playback device names.
 *
 * @name     getOutputDevices
 * @return   ({string...}) Available playback device names.
 *
 */
sol::as_table_t<std::vector<std::string>> LuaVoice::getOutputDevices() {
  return sol::as_table(NetGame::Instance().GetVoiceChatOutputDevices());
}

/* luagmp (method)
 *
 * This function returns the selected playback device name. An empty string means the system default.
 *
 * @name     getOutputDevice
 * @return   (string) Selected playback device name.
 *
 */
std::string LuaVoice::getOutputDevice() {
  return NetGame::Instance().GetVoiceChatOutputDevice();
}

/* luagmp (method)
 *
 * This function selects a playback device by name. Pass an empty string to restore the system default.
 *
 * @name     setOutputDevice
 * @param    (string) device_name  Device name returned by `getOutputDevices`, or an empty string.
 * @return   (boolean)              True when the device was selected.
 *
 */
bool LuaVoice::setOutputDevice(const std::string& device_name) {
  return NetGame::Instance().SetVoiceChatOutputDevice(device_name);
}

/* luagmp (method)
 *
 * This function returns the local player's current voice channel.
 *
 * @name     getChannel
 * @return   (string) Current channel name.
 *
 */
std::string LuaVoice::getChannel() {
  return NetGame::Instance().GetVoiceChatChannel();
}

/* luagmp (method)
 *
 * This function changes the local player's voice channel.
 *
 * @name     setChannel
 * @param    (string) channel  Printable channel name, up to 32 bytes.
 * @return   (boolean)         True when the channel change was accepted locally and sent when connected.
 *
 */
bool LuaVoice::setChannel(const std::string& channel) {
  return NetGame::Instance().SetVoiceChatChannel(channel);
}

/* luagmp (method)
 *
 * This function checks whether the local player is currently transmitting voice.
 *
 * @name     isTransmitting
 * @return   (boolean) True while voice frames are being captured and sent.
 *
 */
bool LuaVoice::isTransmitting() {
  return NetGame::Instance().IsVoiceChatTransmitting();
}

/* luagmp (func)
 *
 * This function checks whether a remote player is currently speaking.
 *
 * @version  0.3.0
 * @name     isPlayerVoiceTalking
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 * @return   (boolean) True while recent valid voice frames are being received.
 *
 */
bool Function_IsPlayerVoiceTalking(std::int64_t player_id) {
  return IsValidVoicePlayerId(player_id) && NetGame::Instance().IsPlayerVoiceTalking(static_cast<std::uint64_t>(player_id));
}

/* luagmp (func)
 *
 * This function checks whether a remote player's voice is muted locally.
 *
 * @version  0.3.0
 * @name     isPlayerVoiceMuted
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 * @return   (boolean) True when the player is muted.
 *
 */
bool Function_IsPlayerVoiceMuted(std::int64_t player_id) {
  return IsValidVoicePlayerId(player_id) && NetGame::Instance().IsPlayerVoiceMuted(static_cast<std::uint64_t>(player_id));
}

/* luagmp (func)
 *
 * This function mutes or unmutes a remote player locally.
 *
 * @version  0.3.0
 * @name     setPlayerVoiceMuted
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 * @param    (boolean) muted     New mute state.
 * @return   (boolean) True when the player id was valid.
 *
 */
bool Function_SetPlayerVoiceMuted(std::int64_t player_id, bool muted) {
  return IsValidVoicePlayerId(player_id) && NetGame::Instance().SetPlayerVoiceMuted(static_cast<std::uint64_t>(player_id), muted);
}

/* luagmp (func)
 *
 * This function returns a remote player's local volume multiplier.
 *
 * @version  0.3.0
 * @name     getPlayerVoiceVolume
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 * @return   (number) Multiplier from 0.0 to 1.0.
 *
 */
float Function_GetPlayerVoiceVolume(std::int64_t player_id) {
  if (!IsValidVoicePlayerId(player_id)) {
    return 1.0f;
  }
  return NetGame::Instance().GetPlayerVoiceVolume(static_cast<std::uint64_t>(player_id));
}

/* luagmp (func)
 *
 * This function changes a remote player's local volume multiplier.
 *
 * @version  0.3.0
 * @name     setPlayerVoiceVolume
 * @side     client
 * @category Voice
 * @param    (number) player_id  Player id.
 * @param    (number) volume     Multiplier from 0.0 to 1.0, clamped to that range.
 * @return   (boolean) True when the player id was valid.
 *
 */
bool Function_SetPlayerVoiceVolume(std::int64_t player_id, float volume) {
  return IsValidVoicePlayerId(player_id) && NetGame::Instance().SetPlayerVoiceVolume(static_cast<std::uint64_t>(player_id), volume);
}

void BindVoice(sol::state& lua) {
  sol::usertype<LuaVoice> voice_type = lua.new_usertype<LuaVoice>("Voice", sol::no_constructor);

  voice_type["isAvailable"] = &LuaVoice::isAvailable;
  voice_type["isEnabled"] = &LuaVoice::isEnabled;
  voice_type["setScriptEnabled"] = &LuaVoice::setScriptEnabled;
  voice_type["getRange"] = &LuaVoice::getRange;
  voice_type["setLocalRangeLimit"] = &LuaVoice::setLocalRangeLimit;
  voice_type["getOutputVolume"] = &LuaVoice::getOutputVolume;
  voice_type["setOutputVolume"] = &LuaVoice::setOutputVolume;
  voice_type["getPushToTalkKey"] = &LuaVoice::getPushToTalkKey;
  voice_type["setPushToTalkKey"] = &LuaVoice::setPushToTalkKey;
  voice_type["getInputDevices"] = &LuaVoice::getInputDevices;
  voice_type["getInputDevice"] = &LuaVoice::getInputDevice;
  voice_type["setInputDevice"] = &LuaVoice::setInputDevice;
  voice_type["getOutputDevices"] = &LuaVoice::getOutputDevices;
  voice_type["getOutputDevice"] = &LuaVoice::getOutputDevice;
  voice_type["setOutputDevice"] = &LuaVoice::setOutputDevice;
  voice_type["getChannel"] = &LuaVoice::getChannel;
  voice_type["setChannel"] = &LuaVoice::setChannel;
  voice_type["isTransmitting"] = &LuaVoice::isTransmitting;
  lua["isPlayerVoiceTalking"] = Function_IsPlayerVoiceTalking;
  lua["isPlayerVoiceMuted"] = Function_IsPlayerVoiceMuted;
  lua["setPlayerVoiceMuted"] = Function_SetPlayerVoiceMuted;
  lua["getPlayerVoiceVolume"] = Function_GetPlayerVoiceVolume;
  lua["setPlayerVoiceVolume"] = Function_SetPlayerVoiceVolume;

  lua["Voice"] = LuaVoice{};
}

}  // namespace gmp::gothic

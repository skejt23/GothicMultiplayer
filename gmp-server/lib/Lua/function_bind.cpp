
/*
MIT License

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

#include "function_bind.h"
#include "function_helpers.cpp"

#include "game_server.h"
#include "shared/lua_runtime/timer_manager.h"

using namespace std;


// Functions
int Function_Log(std::string name, std::string text) {
  std::ofstream logfile;
  logfile.open(name, std::ios_base::app);
  if (logfile.is_open()) {
    logfile << text << "\n";
    logfile.close();
  }
  return 0;
}

void Function_SetDiscordActivity(const sol::table& params) {
  if (!g_server) {
    SPDLOG_WARN("Cannot update Discord activity before the server is initialized");
    return;
  }

  auto activity = g_server->GetDiscordActivity();

  if (auto state = GetOptionalString(params, "state", "State")) {
    activity.state = *state;
  }
  if (auto details = GetOptionalString(params, "details", "Details")) {
    activity.details = *details;
  }
  if (auto largeImageKey = GetOptionalString(params, "largeImageKey", "LargeImageKey")) {
    activity.large_image_key = *largeImageKey;
  }
  if (auto largeImageText = GetOptionalString(params, "largeImageText", "LargeImageText")) {
    activity.large_image_text = *largeImageText;
  }
  if (auto smallImageKey = GetOptionalString(params, "smallImageKey", "SmallImageKey")) {
    activity.small_image_key = *smallImageKey;
  }
  if (auto smallImageText = GetOptionalString(params, "smallImageText", "SmallImageText")) {
    activity.small_image_text = *smallImageText;
  }

  g_server->UpdateDiscordActivity(activity);
}

void Function_SendServerMessage(const std::string& message) {
  if (!g_server) {
    SPDLOG_WARN("Cannot send server message before the server is initialized");
    return;
  }

  g_server->SendServerMessage(message);
}

std::string Function_HashMd5(const std::string& input) {
  unsigned char digest[MD5_DIGEST_LENGTH];
  MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  return BytesToHex(digest, MD5_DIGEST_LENGTH);
}

std::string Function_HashSha1(const std::string& input) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  return BytesToHex(digest, SHA_DIGEST_LENGTH);
}

std::string Function_HashSha256(const std::string& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  return BytesToHex(digest, SHA256_DIGEST_LENGTH);
}

std::string Function_HashSha384(const std::string& input) {
  unsigned char digest[SHA384_DIGEST_LENGTH];
  SHA384(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  return BytesToHex(digest, SHA384_DIGEST_LENGTH);
}

std::string Function_HashSha512(const std::string& input) {
  unsigned char digest[SHA512_DIGEST_LENGTH];
  SHA512(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  return BytesToHex(digest, SHA512_DIGEST_LENGTH);
}


// Register Functions
void lua::bindings::BindFunctions(sol::state& lua, TimerManager& timer_manager) {
  lua["Log"] = Function_Log;

  lua["SetDiscordActivity"] = Function_SetDiscordActivity;
  lua["SendServerMessage"] = Function_SendServerMessage;

  lua["md5"] = Function_HashMd5;
  lua["sha1"] = Function_HashSha1;
  lua["sha256"] = Function_HashSha256;
  lua["sha384"] = Function_HashSha384;
  lua["sha512"] = Function_HashSha512;
}

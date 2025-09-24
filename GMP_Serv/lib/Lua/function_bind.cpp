
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

std::int64_t Function_GetTickCount() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - kStartTime).count();
}

sol::object Function_HexToRgb(const std::string& hex, sol::this_state ts) {
  sol::state_view lua(ts);
  auto color = ParseHexColor(hex);
  if (!color) {
    return sol::make_object(lua, sol::lua_nil);
  }

  sol::table table = lua.create_table(3, 0);
  table[1] = (*color)[0];
  table[2] = (*color)[1];
  table[3] = (*color)[2];
  table["r"] = (*color)[0];
  table["g"] = (*color)[1];
  table["b"] = (*color)[2];

  return table;
}

std::string Function_RgbToHex(int r, int g, int b) {
  auto clamp_component = [](int value) { return std::clamp(value, 0, 255); };

  std::ostringstream stream;
  stream << std::hex << std::nouppercase << std::setfill('0');
  stream << std::setw(2) << clamp_component(r);
  stream << std::setw(2) << clamp_component(g);
  stream << std::setw(2) << clamp_component(b);
  return stream.str();
}

sol::object Function_Sscanf(const std::string& format, const std::string& text, sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table result = lua.create_table();
  std::istringstream stream(text);
  int index = 1;

  for (char specifier : format) {
    if (std::isspace(static_cast<unsigned char>(specifier))) {
      continue;
    }
    switch (specifier) {
      case 'd': {
        long long value;
        if (!(stream >> value)) {
          return sol::make_object(lua, sol::lua_nil);
        }
        result[index++] = value;
        break;
      }
      case 'f': {
        double value;
        if (!(stream >> value)) {
          return sol::make_object(lua, sol::lua_nil);
        }
        result[index++] = value;
        break;
      }
      case 's': {
        std::string value;
        if (!(stream >> value)) {
          return sol::make_object(lua, sol::lua_nil);
        }
        result[index++] = value;
        break;
      }
      default:
        return sol::make_object(lua, sol::lua_nil);
    }
  }

  return result;
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

  lua["getTickCount"] = Function_GetTickCount;
  lua["hexToRgb"] = Function_HexToRgb;
  lua["rgbToHex"] = Function_RgbToHex;
  lua["sscanf"] = Function_Sscanf;

  lua["md5"] = Function_HashMd5;
  lua["sha1"] = Function_HashSha1;
  lua["sha256"] = Function_HashSha256;
  lua["sha384"] = Function_HashSha384;
  lua["sha512"] = Function_HashSha512;

  lua.set_function("setTimer",
                   [&timer_manager](sol::protected_function func, int interval, int execute_times, sol::variadic_args args, sol::this_state ts) {
                     sol::state_view lua(ts);
                     auto copied_arguments = CopyArguments(lua, args);
                     auto timer_interval = std::chrono::milliseconds(interval);
                     std::uint32_t times = execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u;
                     return static_cast<int>(timer_manager.CreateTimer(std::move(func), timer_interval, times, std::move(copied_arguments)));
                   });

  lua.set_function("killTimer", [&timer_manager](int timer_id) { timer_manager.KillTimer(static_cast<TimerManager::TimerId>(timer_id)); });

  lua.set_function("getTimerInterval", [&timer_manager](int timer_id, sol::this_state ts) -> sol::object {
    sol::state_view lua(ts);
    auto interval = timer_manager.GetInterval(static_cast<TimerManager::TimerId>(timer_id));
    if (!interval) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return sol::make_object(lua, static_cast<int>(interval->count()));
  });

  lua.set_function("setTimerInterval", [&timer_manager](int timer_id, int interval) {
    timer_manager.SetInterval(static_cast<TimerManager::TimerId>(timer_id), std::chrono::milliseconds(interval));
  });

  lua.set_function("getTimerExecuteTimes", [&timer_manager](int timer_id, sol::this_state ts) -> sol::object {
    sol::state_view lua(ts);
    auto times = timer_manager.GetExecuteTimes(static_cast<TimerManager::TimerId>(timer_id));
    if (!times) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return sol::make_object(lua, static_cast<int>(*times));
  });

  lua.set_function("setTimerExecuteTimes", [&timer_manager](int timer_id, int execute_times) {
    std::uint32_t times = execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u;
    timer_manager.SetExecuteTimes(static_cast<TimerManager::TimerId>(timer_id), times);
  });
}
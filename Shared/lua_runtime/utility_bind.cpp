
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

#include "utility_bind.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/md5.h>
#include <openssl/sha.h>

namespace lua {
namespace bindings {

namespace {

const auto kStartTime = std::chrono::steady_clock::now();

std::optional<std::array<int, 3>> ParseHexColor(const std::string& hex) {
  std::string sanitized = hex;

  if (!sanitized.empty() && sanitized[0] == '#') {
    sanitized.erase(0, 1);
  }

  if (sanitized.size() >= 2 && sanitized[0] == '0' && (sanitized[1] == 'x' || sanitized[1] == 'X')) {
    sanitized.erase(0, 2);
  }

  if (sanitized.empty()) {
    return std::nullopt;
  }

  for (char& ch : sanitized) {
    if (!std::isxdigit(static_cast<unsigned char>(ch))) {
      return std::nullopt;
    }
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  std::array<int, 3> components{};

  if (sanitized.size() == 6) {
    for (std::size_t i = 0; i < 3; ++i) {
      const auto start = sanitized.substr(i * 2, 2);
      components[i] = std::stoi(start, nullptr, 16);
    }
    return components;
  }

  if (sanitized.size() == 3) {
    for (std::size_t i = 0; i < 3; ++i) {
      const std::string doubled(2, sanitized[i]);
      components[i] = std::stoi(doubled, nullptr, 16);
    }
    return components;
  }

  return std::nullopt;
}

std::vector<sol::object> CopyArguments(sol::state_view lua, const sol::variadic_args& args) {
  std::vector<sol::object> values;
  values.reserve(args.size());

  for (auto arg : args) {
    values.emplace_back(sol::make_object(lua, arg));
  }

  return values;
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

  for (std::size_t i = 0; i < format.size(); ++i) {
    char specifier = format[i];

    if (std::isspace(static_cast<unsigned char>(specifier))) {
      continue;
    }

    bool is_last = true;
    for (std::size_t j = i + 1; j < format.size(); ++j) {
      if (!std::isspace(static_cast<unsigned char>(format[j]))) {
        is_last = false;
        break;
      }
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

        if (is_last) {
          if (!std::getline(stream >> std::ws, value)) {
            return sol::make_object(lua, sol::lua_nil);
          }
        } else {
          if (!(stream >> value)) {
            return sol::make_object(lua, sol::lua_nil);
          }
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


std::int64_t Function_GetTickCount() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - kStartTime).count();
}

std::string BytesToHex(const unsigned char* data, std::size_t length) {
  std::ostringstream stream;
  stream << std::hex << std::nouppercase << std::setfill('0');

  for (std::size_t i = 0; i < length; ++i) {
    stream << std::setw(2) << static_cast<int>(data[i]);
  }

  return stream.str();
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

}  // namespace

void BindUtilities(sol::state& lua) {
  lua["getTickCount"] = Function_GetTickCount;
  lua["hexToRgb"] = Function_HexToRgb;
  lua["rgbToHex"] = Function_RgbToHex;
  lua["sscanf"] = Function_Sscanf;
  lua["md5"] = Function_HashMd5;
  lua["sha1"] = Function_HashSha1;
  lua["sha256"] = Function_HashSha256;
  lua["sha384"] = Function_HashSha384;
  lua["sha512"] = Function_HashSha512;
}

void BindTimers(sol::state& lua, TimerManager& timer_manager) {
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

}  // namespace bindings
}  // namespace lua

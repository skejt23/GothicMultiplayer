
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

#include "lua_utility.h"

#include <sodium.h>
#include <xxhash.h>

#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "bind_helpers.h"
#include "shared/crypto_utils.h"

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

/* luagmp (func)
 *
 * This function will convert a hex color string to an RGB table.
 *
 * @version  0.3.0
 * @name     hexToRgb
 * @side     shared
 * @category Utility
 * @param    (string) hex   Hex color string (e.g. "#RRGGBB", "0xRRGGBB", or "RGB").
 * @return   ({r, g, b}|nil)   Table containing r, g, b components or nil on failure.
 *
 */
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

/* luagmp (func)
 *
 * This function will convert RGB components to a lowercase hex color string.
 *
 * @version  0.3.0
 * @name     rgbToHex
 * @side     shared
 * @category Utility
 * @param    (number) r   Red component (0-255).
 * @param    (number) g   Green component (0-255).
 * @param    (number) b   Blue component (0-255).
 * @return   (string) Lowercase hexadecimal representation.
 *
 */
std::string Function_RgbToHex(int r, int g, int b) {
  std::ostringstream stream;
  stream << std::hex << std::nouppercase << std::setfill('0');
  stream << std::setw(2) << static_cast<int>(::lua::bind_helpers::ClampByte(r));
  stream << std::setw(2) << static_cast<int>(::lua::bind_helpers::ClampByte(g));
  stream << std::setw(2) << static_cast<int>(::lua::bind_helpers::ClampByte(b));
  return stream.str();
}

/* luagmp (func)
 *
 * This function will split text according to a format string and return the parsed values.
 *
 * @version  0.3.0
 * @name     sscanf
 * @side     shared
 * @category Utility
 * @param    (string) format  Format string where each specifier maps to a value. Supported specifiers: `d` (integer), `f` (number), `s` (string).
 * @param    (string) text    Input text to parse.
 * @return   (table|nil)      Array of parsed values, or nil on parse failure.
 *
 */
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

/* luagmp (func)
 *
 * This function will return the number of milliseconds since the scripting runtime started.
 *
 * @version  0.3.0
 * @name     getTickCount
 * @side     shared
 * @category Utility
 * @return   (number)       Milliseconds since startup.
 *
 */
std::int64_t Function_GetTickCount() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - kStartTime).count();
}

/* luagmp (func)
 *
 * This function will return the current local wall-clock time.
 *
 * @version  0.3.0
 * @name     getRealTime
 * @side     shared
 * @category Utility
 * @return   ({hour, minute, second})  Table containing local time components.
 *
 */
sol::table Function_GetRealTime(sol::this_state ts) {
  sol::state_view lua(ts);
  sol::table result = lua.create_table();

  std::time_t now = std::time(nullptr);
  std::tm local_time{};

#if defined(_WIN32)
  localtime_s(&local_time, &now);
#else
  localtime_r(&now, &local_time);
#endif

  result["hour"] = local_time.tm_hour;
  result["minute"] = local_time.tm_min;
  result["second"] = local_time.tm_sec;
  return result;
}

/* luagmp (func)
 *
 * This function will calculate the SHA-256 hash of a string and return it as hex.
 *
 * @version  0.3.0
 * @name     sha256
 * @side     shared
 * @category Hash
 * @param    (string) input  Input text to hash.
 * @return   (string)        Hexadecimal hash string.
 *
 */
std::string Function_Sha256(const std::string& input) {
  unsigned char digest[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(digest, reinterpret_cast<const unsigned char*>(input.data()), input.size());
  return gmp::crypto::BytesToHex(digest, crypto_hash_sha256_BYTES);
}

/* luagmp (func)
 *
 * This function will calculate the SHA-512 hash of a string and return it as hex.
 *
 * @version  0.3.0
 * @name     sha512
 * @side     shared
 * @category Hash
 * @param    (string) input  Input text to hash.
 * @return   (string)        Hexadecimal hash string.
 *
 */
std::string Function_Sha512(const std::string& input) {
  unsigned char digest[crypto_hash_sha512_BYTES];
  crypto_hash_sha512(digest, reinterpret_cast<const unsigned char*>(input.data()), input.size());
  return gmp::crypto::BytesToHex(digest, crypto_hash_sha512_BYTES);
}

lua_Integer ReadXxHashSeed(const sol::optional<sol::stack_object>& seed) {
  if (!seed) {
    return 0;
  }
  if (lua_isinteger(seed->lua_state(), seed->stack_index()) == 0) {
    throw sol::error("hash seed must be a Lua integer");
  }
  const lua_Integer seed_value = seed->as<lua_Integer>();
  if (seed_value < 0 || seed_value > 4294967295LL) {
    throw sol::error("hash seed must be between 0 and 4294967295");
  }
  return seed_value;
}

/* luagmp (func)
 *
 * Return the XXH32 hash of the input string as lowercase hexadecimal text.
 *
 * @version  0.3.0
 * @name     xxh32
 * @side     shared
 * @category Hash
 * @param    (string) input  Input bytes to hash.
 * @param    (number) seed   Integer seed from 0 to 4294967295. (Optional, defaults to 0.)
 * @return   (string)        Lowercase hexadecimal hash string (8 characters, including leading zeros).
 *
 */
std::string Function_Xxh32(const std::string& input, sol::optional<sol::stack_object> seed) {
  const lua_Integer seed_value = ReadXxHashSeed(seed);
  XXH32_canonical_t digest;
  XXH32_canonicalFromHash(&digest, XXH32(input.data(), input.size(), static_cast<XXH32_hash_t>(seed_value)));
  return gmp::crypto::BytesToHex(digest.digest, sizeof(digest.digest));
}

/* luagmp (func)
 *
 * Return the XXH64 hash of the input string as lowercase hexadecimal text.
 *
 * @version  0.3.0
 * @name     xxh64
 * @side     shared
 * @category Hash
 * @param    (string) input  Input bytes to hash.
 * @param    (number) seed   Integer seed from 0 to 4294967295. (Optional, defaults to 0.)
 * @return   (string)        Lowercase hexadecimal hash string (16 characters, including leading zeros).
 *
 */
std::string Function_Xxh64(const std::string& input, sol::optional<sol::stack_object> seed) {
  const auto seed_value = static_cast<XXH64_hash_t>(ReadXxHashSeed(seed));
  XXH64_canonical_t digest;
  XXH64_canonicalFromHash(&digest, XXH64(input.data(), input.size(), seed_value));
  return gmp::crypto::BytesToHex(digest.digest, sizeof(digest.digest));
}

/* luagmp (func)
 *
 * Return the XXH128 hash of the input string as lowercase hexadecimal text.
 *
 * @version  0.3.0
 * @name     xxh128
 * @side     shared
 * @category Hash
 * @param    (string) input  Input bytes to hash.
 * @param    (number) seed   Integer seed from 0 to 4294967295. (Optional, defaults to 0.)
 * @return   (string)        Lowercase hexadecimal hash string (32 characters, including leading zeros).
 *
 */
std::string Function_Xxh128(const std::string& input, sol::optional<sol::stack_object> seed) {
  const auto seed_value = static_cast<XXH64_hash_t>(ReadXxHashSeed(seed));
  XXH128_canonical_t digest;
  XXH128_canonicalFromHash(&digest, XXH3_128bits_withSeed(input.data(), input.size(), seed_value));
  return gmp::crypto::BytesToHex(digest.digest, sizeof(digest.digest));
}

}  // namespace

struct SetTimerBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will create a new timer that calls the given function at a fixed interval.
   *
   * The timer passes any additional arguments to the callback when it executes.
   * If execute_times is 0 or negative, the timer repeats indefinitely.
   *
   * @version  0.3.0
   * @name     setTimer
   * @side     shared
   * @category Timer
   * @param    (function) func         Callback function executed by the timer.
   * @param    (number) interval       Interval in milliseconds.
   * @param    (number) execute_times  How many times to execute the callback (<= 0 means infinite).
   * @param    (...) ...               Additional arguments forwarded to the callback.
   * @return   (number)                Timer ID.
   *
   */

  auto operator()(sol::protected_function func, int interval, int execute_times, sol::variadic_args args, sol::this_state ts) const {
    sol::state_view lua(ts);
    auto copied_arguments = ::lua::bind_helpers::CopyArguments(lua, args);
    auto timer_interval = std::chrono::milliseconds(interval);
    std::uint32_t times = execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u;
    return static_cast<int>(timer_manager.CreateTimer(std::move(func), timer_interval, times, std::move(copied_arguments)));
  }
};

struct KillTimerBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will stop and remove an existing timer.
   *
   * @version  0.3.0
   * @name     killTimer
   * @side     shared
   * @category Timer
   * @param    (number) timer_id   Timer ID returned by [setTimer](setTimer.md).
   *
   */

  auto operator()(int timer_id) const {
    timer_manager.KillTimer(static_cast<TimerManager::TimerId>(timer_id));
  }
};

struct SetTimerIntervalBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will set the interval (in milliseconds) of an existing timer.
   *
   * @version  0.3.0
   * @name     setTimerInterval
   * @side     shared
   * @category Timer
   * @param    (number) timer_id     Timer ID returned by [setTimer](setTimer.md).
   * @param    (number) interval     New interval in milliseconds.
   *
   */

  auto operator()(int timer_id, int interval) const {
    timer_manager.SetInterval(static_cast<TimerManager::TimerId>(timer_id), std::chrono::milliseconds(interval));
  }
};

struct GetTimerIntervalBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will return the interval (in milliseconds) of a timer, or nil if the timer does not exist.
   *
   * @version  0.3.0
   * @name     getTimerInterval
   * @side     shared
   * @category Timer
   * @param    (number) timer_id      Timer ID returned by [setTimer](setTimer.md).
   * @return   (number|nil)           Interval in milliseconds, or nil if not found.
   *
   */

  auto operator()(int timer_id, sol::this_state ts) const -> sol::object {
    sol::state_view lua(ts);
    auto interval = timer_manager.GetInterval(static_cast<TimerManager::TimerId>(timer_id));
    if (!interval) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return sol::make_object(lua, static_cast<int>(interval->count()));
  }
};

struct SetTimerRemainingExecutionsBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will set how many times the timer should execute.
   * If execute_times is 0 or negative, the timer repeats indefinitely.
   *
   * @version  0.3.0
   * @name     setTimerRemainingExecutions
   * @side     shared
   * @category Timer
   * @param    (number) timer_id       Timer ID returned by [setTimer](setTimer.md).
   * @param    (number) execute_times  How many times to execute (<= 0 means infinite).
   *
   */

  auto operator()(int timer_id, int execute_times) const {
    std::uint32_t times = execute_times > 0 ? static_cast<std::uint32_t>(execute_times) : 0u;
    timer_manager.SetRemainingExecutions(static_cast<TimerManager::TimerId>(timer_id), times);
  }
};

struct GetTimerRemainingExecutionsBinding {
  TimerManager& timer_manager;
  /* luagmp (func)
   *
   * This function will return how many times the timer will execute, or nil if the timer does not exist.
   * A value of 0 means the timer repeats indefinitely.
   *
   * @version  0.3.0
   * @name     getTimerRemainingExecutions
   * @side     shared
   * @category Timer
   * @param    (number) timer_id     Timer ID returned by [setTimer](setTimer.md).
   * @return   (number|nil)          Execute count (0 = infinite), or nil if not found.
   *
   */

  auto operator()(int timer_id, sol::this_state ts) const -> sol::object {
    sol::state_view lua(ts);
    auto times = timer_manager.GetRemainingExecutions(static_cast<TimerManager::TimerId>(timer_id));
    if (!times) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return sol::make_object(lua, static_cast<int>(*times));
  }
};

void BindUtilities(sol::state& lua) {
  lua["getTickCount"] = Function_GetTickCount;
  lua["getRealTime"] = Function_GetRealTime;
  lua["hexToRgb"] = Function_HexToRgb;
  lua["rgbToHex"] = Function_RgbToHex;
  lua["sscanf"] = Function_Sscanf;
  lua["sha256"] = Function_Sha256;
  lua["sha512"] = Function_Sha512;
  lua["xxh32"] = Function_Xxh32;
  lua["xxh64"] = Function_Xxh64;
  lua["xxh128"] = Function_Xxh128;
}

void BindTimers(sol::state& lua, TimerManager& timer_manager) {
  lua.set_function("setTimer", SetTimerBinding{timer_manager});

  lua.set_function("killTimer", KillTimerBinding{timer_manager});

  lua.set_function("setTimerInterval", SetTimerIntervalBinding{timer_manager});

  lua.set_function("getTimerInterval", GetTimerIntervalBinding{timer_manager});

  lua.set_function("setTimerRemainingExecutions", SetTimerRemainingExecutionsBinding{timer_manager});

  lua.set_function("getTimerRemainingExecutions", GetTimerRemainingExecutionsBinding{timer_manager});
}

}  // namespace bindings
}  // namespace lua

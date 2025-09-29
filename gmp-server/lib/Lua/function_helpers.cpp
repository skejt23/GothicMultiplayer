
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


#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/md5.h>
#include <openssl/sha.h>
#include <spdlog/spdlog.h>

#include "timer_manager.h"

using namespace std;

namespace {

const auto kStartTime = std::chrono::steady_clock::now();

sol::optional<std::string> GetOptionalString(const sol::table& table, const char* lowerKey, const char* upperKey) {
  if (auto value = table.get<sol::optional<std::string>>(lowerKey); value) {
    return value;
  }
  return table.get<sol::optional<std::string>>(upperKey);
}

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

std::string BytesToHex(const unsigned char* data, std::size_t length) {
  std::ostringstream stream;
  stream << std::hex << std::nouppercase << std::setfill('0');

  for (std::size_t i = 0; i < length; ++i) {
    stream << std::setw(2) << static_cast<int>(data[i]);
  }

  return stream.str();
}

std::vector<sol::object> CopyArguments(sol::state_view lua, const sol::variadic_args& args) {
  std::vector<sol::object> values;
  values.reserve(args.size());

  for (auto arg : args) {
    values.emplace_back(sol::make_object(lua, arg));
  }

  return values;
}

}  // namespace
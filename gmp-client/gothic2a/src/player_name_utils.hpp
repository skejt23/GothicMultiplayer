
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

#include <string>

constexpr auto kMaxPlayerNameLength = 24;

inline std::string SanitizePlayerName(const std::string& player_name) {
  std::string sanitized = player_name;
  // Remove any unwanted characters (e.g., non-printable characters)
  sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [](unsigned char c) { return !std::isprint(c); }), sanitized.end());
  // Trim whitespace from both ends
  sanitized.erase(sanitized.begin(), std::find_if(sanitized.begin(), sanitized.end(), [](unsigned char c) { return !std::isspace(c); }));
  sanitized.erase(std::find_if(sanitized.rbegin(), sanitized.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), sanitized.end());
  // Enforce maximum length
  if (sanitized.length() > kMaxPlayerNameLength) {
    sanitized = sanitized.substr(0, kMaxPlayerNameLength);
  }
  return sanitized;
}

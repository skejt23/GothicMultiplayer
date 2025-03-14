
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <toml.hpp>

// Class for reading from TOML files.
class TomlWrapper {
public:
  void Serialize(std::string_view file_path) const;

  // Tries to get the value (T) from the specified keys.
  // Example usage:
  // GetValue<uint8_t>("gameTimeStart", "minute");
  // GetValue<std::string>("playerName");
  template <typename T, typename... Keys>
  std::optional<T> GetValue(const std::string& key, Keys... keys) {
    try {
      return toml::find<T>(data_, key, std::forward<Keys>(keys)...);
    } catch (std::exception&) {
      return std::nullopt;
    }
  }

  // The same as GetValue from above, but allows to set a default value in case
  // no value is found in the config.
  template <typename T, typename... Keys>
  T GetValue(const std::string& key, T defaultValue, Keys... keys) {
    try {
      return toml::find<T>(data_, key, std::forward<Keys>(keys)...);
    } catch (std::exception&) {
      return defaultValue;
    }
  }

  // Value by key accessor.
  // Will create an empty table value if the value with the given key didn't exist.
  toml::value& operator[](const std::string& key) {
    return data_[key];
  }
  toml::value& operator[](const char* key) {
    return data_[key];
  }

  /**
   * Creates the TomlWrapper and loads the conent from the given TOML file.
   *
   * @param filePath Path to the TOML file.
   * @throws toml::syntax_error if the file is not in line with TOML syntax.
   * @throws std::runtime_error if the file is not found.
   *
   */
  static TomlWrapper CreateFromFile(std::string_view file_path);

private:
  toml::value data_;
};
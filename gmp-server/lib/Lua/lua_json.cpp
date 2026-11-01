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

#include "Lua/lua_json.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Shared/lua_runtime/bind_helpers.h"

namespace lua::bindings {

namespace {

constexpr std::size_t kMaxJsonBytes = 1024 * 1024;
constexpr std::size_t kMaxEntries = 4096;
constexpr int kMaxDepth = 32;

using Json = NLOHMANN_JSON_NAMESPACE::json;

bool EncodeLuaValue(sol::state_view lua, const sol::object& value, Json& out, std::string& error, int depth,
                    std::unordered_set<const void*>& visited) {
  (void)lua;
  if (depth > kMaxDepth) {
    error = "Lua value exceeded maximum serialization depth";
    return false;
  }

  switch (value.get_type()) {
    case sol::type::nil:
      out = nullptr;
      return true;
    case sol::type::boolean:
      out = value.as<bool>();
      return true;
    case sol::type::number: {
      const double number = value.as<double>();
      double integral = 0.0;
      const double int64_min = -std::ldexp(1.0, 63);
      const double int64_max_exclusive = std::ldexp(1.0, 63);
      if (std::isfinite(number) && std::modf(number, &integral) == 0.0 && integral >= int64_min && integral < int64_max_exclusive) {
        out = static_cast<std::int64_t>(integral);
        return true;
      }

      out = number;
      return true;
    }
    case sol::type::string:
      out = value.as<std::string>();
      return true;
    case sol::type::table: {
      const void* identity = ::lua::bind_helpers::GetLuaIdentity(value);
      if (identity != nullptr && !visited.insert(identity).second) {
        error = "Lua table contains a cyclic reference";
        return false;
      }

      sol::table table = value.as<sol::table>();
      bool has_string_keys = false;
      bool has_number_keys = false;
      std::size_t max_index = 0;
      std::size_t count = 0;

      for (const auto& kv : table) {
        sol::object key = kv.first;
        if (key.get_type() == sol::type::string) {
          has_string_keys = true;
        } else if (key.get_type() == sol::type::number) {
          has_number_keys = true;
          const double raw = key.as<double>();
          if (raw < 1.0 || std::floor(raw) != raw) {
            error = "Lua table numeric key is not a positive integer";
            if (identity != nullptr) {
              visited.erase(identity);
            }
            return false;
          }
          max_index = std::max(max_index, static_cast<std::size_t>(raw));
        } else {
          error = "Lua table contains unsupported key type";
          if (identity != nullptr) {
            visited.erase(identity);
          }
          return false;
        }
        ++count;
      }

      if (has_string_keys && has_number_keys) {
        error = "Lua table mixes string and numeric keys";
        if (identity != nullptr) {
          visited.erase(identity);
        }
        return false;
      }

      if (has_number_keys) {
        if (count != max_index) {
          error = "Lua array has gaps";
          if (identity != nullptr) {
            visited.erase(identity);
          }
          return false;
        }
        std::vector<Json> array_items(max_index);
        for (const auto& kv : table) {
          const auto index = static_cast<std::size_t>(kv.first.as<double>());
          Json entry;
          if (!EncodeLuaValue(lua, kv.second, entry, error, depth + 1, visited)) {
            if (identity != nullptr) {
              visited.erase(identity);
            }
            return false;
          }
          array_items[index - 1] = std::move(entry);
        }
        Json array = Json::array();
        for (auto& item : array_items) {
          array.push_back(std::move(item));
        }
        out = std::move(array);
      } else {
        Json object = Json::object();
        for (const auto& kv : table) {
          const auto key = kv.first.as<std::string>();
          Json entry;
          if (!EncodeLuaValue(lua, kv.second, entry, error, depth + 1, visited)) {
            if (identity != nullptr) {
              visited.erase(identity);
            }
            return false;
          }
          object[key] = std::move(entry);
        }
        out = std::move(object);
      }

      if (identity != nullptr) {
        visited.erase(identity);
      }
      return true;
    }
    default:
      error = "Unsupported Lua value type";
      return false;
  }
}

bool DecodeJsonValue(sol::state_view lua, const Json& value, sol::object& out, std::string& error, int depth) {
  if (depth > kMaxDepth) {
    error = "JSON value exceeded maximum deserialization depth";
    return false;
  }

  if (value.is_null()) {
    out = sol::make_object(lua, sol::lua_nil);
    return true;
  }
  if (value.is_boolean()) {
    out = sol::make_object(lua, value.get<bool>());
    return true;
  }
  if (value.is_number_integer()) {
    out = sol::make_object(lua, value.get<Json::number_integer_t>());
    return true;
  }
  if (value.is_number_unsigned()) {
    const auto number = value.get<Json::number_unsigned_t>();
    if (number <= static_cast<Json::number_unsigned_t>(std::numeric_limits<Json::number_integer_t>::max())) {
      out = sol::make_object(lua, static_cast<Json::number_integer_t>(number));
    } else {
      out = sol::make_object(lua, static_cast<double>(number));
    }
    return true;
  }
  if (value.is_number_float()) {
    out = sol::make_object(lua, value.get<double>());
    return true;
  }
  if (value.is_string()) {
    out = sol::make_object(lua, value.get<std::string>());
    return true;
  }
  if (value.is_array()) {
    sol::table table = lua.create_table(static_cast<int>(value.size()), 0);
    for (std::size_t i = 0; i < value.size(); ++i) {
      sol::object entry;
      if (!DecodeJsonValue(lua, value[i], entry, error, depth + 1)) {
        return false;
      }
      table[i + 1] = entry;
    }
    out = sol::make_object(lua, table);
    return true;
  }
  if (value.is_object()) {
    sol::table table = lua.create_table();
    for (auto it = value.begin(); it != value.end(); ++it) {
      sol::object entry;
      if (!DecodeJsonValue(lua, it.value(), entry, error, depth + 1)) {
        return false;
      }
      table[it.key()] = entry;
    }
    out = sol::make_object(lua, table);
    return true;
  }

  error = "Unsupported JSON value type";
  return false;
}

bool EnsureParentDirectory(const std::filesystem::path& path, std::string& error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "Failed to create parent directory: " + ec.message();
    return false;
  }
  return true;
}

/* luagmp (class)
 *
 * JSON-backed key/value store returned by `JSON.open()`.
 *
 * @version  0.3.0
 * @name     JSON
 * @side     server
 * @category File
 *
 */
class JSON {
public:
  explicit JSON(std::filesystem::path path) : path_(std::move(path)) {
  }

  static std::tuple<sol::object, sol::object> Open(const std::string& relative_path, sol::this_state ts);

  bool Load(std::string& error) {
    entries_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
      return true;
    }
    if (ec) {
      error = "Failed to stat JSON file: " + ec.message();
      return false;
    }

    auto size = std::filesystem::file_size(path_, ec);
    if (ec) {
      error = "Failed to get JSON file size: " + ec.message();
      return false;
    }
    if (size > kMaxJsonBytes) {
      error = "JSON file exceeds size limit";
      return false;
    }

    std::ifstream file(path_, std::ios::binary);
    if (!file.good()) {
      error = "Unable to open JSON file";
      return false;
    }

    Json json;
    try {
      file >> json;
    } catch (const nlohmann::json::parse_error& ex) {
      error = std::string("Failed to parse JSON: ") + ex.what();
      return false;
    }

    if (!json.is_object()) {
      error = "JSON root must be an object";
      return false;
    }

    for (auto it = json.begin(); it != json.end(); ++it) {
      if (entries_.size() >= kMaxEntries) {
        error = "JSON entry limit exceeded";
        return false;
      }
      entries_.emplace_back(it.key(), it.value());
    }

    return true;
  }

  bool Save(std::string& error) const {
    if (!EnsureParentDirectory(path_, error)) {
      return false;
    }

    if (entries_.size() > kMaxEntries) {
      error = "JSON entry limit exceeded";
      return false;
    }

    Json json = Json::object();
    for (const auto& entry : entries_) {
      json[entry.key] = entry.value;
    }

    std::string payload = json.dump(2);
    if (payload.size() > kMaxJsonBytes) {
      error = "JSON payload exceeds size limit";
      return false;
    }

    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file.good()) {
      error = "Unable to open JSON file for writing";
      return false;
    }
    file << payload << '\n';
    if (!file.good()) {
      error = "Failed to write JSON file";
      return false;
    }
    return true;
  }

  std::size_t EntryCount() const {
    return entries_.size();
  }

  sol::object KeyAtIndex(sol::state_view lua, std::size_t index) const {
    if (index >= entries_.size()) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return sol::make_object(lua, entries_[index].key);
  }

  sol::object GetValue(sol::state_view lua, const std::string& key, std::string& error) const {
    const Entry* entry = FindKey(key);
    if (!entry) {
      return sol::make_object(lua, sol::lua_nil);
    }
    sol::object value;
    if (!DecodeJsonValue(lua, entry->value, value, error, 0)) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return value;
  }

  bool SetValue(sol::state_view lua, const std::string& key, const sol::object& value, std::string& error) {
    Json encoded;
    std::unordered_set<const void*> visited;
    if (!EncodeLuaValue(lua, value, encoded, error, 0, visited)) {
      return false;
    }

    if (Entry* entry = FindKey(key)) {
      entry->value = std::move(encoded);
      return true;
    }

    if (entries_.size() >= kMaxEntries) {
      error = "JSON entry limit exceeded";
      return false;
    }
    entries_.push_back({key, std::move(encoded)});
    return true;
  }

  bool RemoveKey(const std::string& key) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->key == key) {
        entries_.erase(it);
        return true;
      }
    }
    return false;
  }

  /* luagmp (method)
   *
   * Return the key at the given one-based index or nil if out of range.
   *
   * @name     keyAt
   * @param    (number) index  One-based entry index.
   * @return   (string|nil)    Key at index or nil.
   *
   */
  sol::object KeyAt(int index, sol::this_state ts) const {
    sol::state_view lua(ts);
    if (index < 1) {
      return sol::make_object(lua, sol::lua_nil);
    }
    return KeyAtIndex(lua, static_cast<std::size_t>(index - 1));
  }

  /* luagmp (method)
   *
   * Return the number of entries in the file.
   *
   * @name     count
   * @return   (number) Entry count.
   *
   */
  int Count() const {
    return static_cast<int>(EntryCount());
  }

  /* luagmp (method)
   *
   * Get a value by key.
   *
   * @name     get
   * @param    (string) key  Entry key.
   * @return   (any|nil)     Value or nil if missing or invalid.
   *
   */
  sol::object Get(const std::string& key, sol::this_state ts) const {
    sol::state_view lua(ts);
    std::string error;
    sol::object value = GetValue(lua, key, error);
    if (!error.empty()) {
      SPDLOG_WARN("JSON get failed for key '{}': {}", key, error);
    }
    return value;
  }

  /* luagmp (method)
   *
   * Set a value by key and save the file.
   *
   * @name     set
   * @param    (string) key  Entry key.
   * @param    (any) value   Value to store.
   *
   * @return (boolean, table|nil) True and nil on success; false and {message = string} on encoding or persistence failure.
   */
  std::tuple<bool, sol::object> Set(const std::string& key, const sol::object& value, sol::this_state ts) {
    sol::state_view lua(ts);
    std::string error;
    const bool saved = SetValue(lua, key, value, error) && Save(error);
    return WriteResult(lua, saved, error);
  }

  // Mutations preserve automatic persistence and now report save failures to Lua.
  /* luagmp (method)
   * Removes a key if present and persists the store.
   * @name remove
   * @param (string) key Entry key.
   * @return (boolean, table|nil) True and nil on success; false and {message = string} on persistence failure. In-memory changes are retained on
   * failure.
   */
  std::tuple<bool, sol::object> Remove(const std::string& key, sol::this_state ts) {
    RemoveKey(key);
    std::string error;
    const bool saved = Save(error);
    return WriteResult(sol::state_view(ts), saved, error);
  }

  /* luagmp (method)
   * Removes all keys and persists the store.
   * @name clear
   * @return (boolean, table|nil) True and nil on success; false and {message = string} on persistence failure. In-memory changes are retained on
   * failure.
   */
  std::tuple<bool, sol::object> Clear(sol::this_state ts) {
    entries_.clear();
    std::string error;
    const bool saved = Save(error);
    return WriteResult(sol::state_view(ts), saved, error);
  }

  static std::tuple<bool, sol::object> WriteResult(sol::state_view lua, bool saved, const std::string& error) {
    if (saved)
      return {true, sol::make_object(lua, sol::nil)};
    SPDLOG_WARN("JSON write failed: {}", error);
    return {false, sol::make_object(lua, lua.create_table_with("message", error))};
  }

private:
  struct Entry {
    std::string key;
    Json value;
  };

  Entry* FindKey(const std::string& key) {
    for (auto& entry : entries_) {
      if (entry.key == key) {
        return &entry;
      }
    }
    return nullptr;
  }

  const Entry* FindKey(const std::string& key) const {
    for (const auto& entry : entries_) {
      if (entry.key == key) {
        return &entry;
      }
    }
    return nullptr;
  }

  std::filesystem::path path_;
  std::vector<Entry> entries_;
};

std::optional<JSON> LoadJSON(const std::string& relative_path, std::string& error) {
  auto resolved = ::lua::bind_helpers::ResolveDataPath(relative_path);
  if (!resolved) {
    error = "Invalid JSON path";
    return std::nullopt;
  }
  JSON file(*resolved);
  if (!file.Load(error)) {
    return std::nullopt;
  }
  return file;
}

/* luagmp (method)
 *
 * Open a JSON file relative to the server data/internal directory.
 *
 * @name     open
 * @param    (string) relative_path  Path under the data/internal directory.
 *
 * @return (JSON|nil, table|nil) Store and nil on success; nil and {message = string} on failure.
 * @static
 */
std::tuple<sol::object, sol::object> JSON::Open(const std::string& relative_path, sol::this_state ts) {
  sol::state_view lua(ts);
  std::string error;
  auto file = LoadJSON(relative_path, error);
  if (!file) {
    if (!error.empty() && error != "Invalid JSON path") {
      SPDLOG_WARN("JSON open failed for '{}': {}", relative_path, error);
    }
    return {sol::make_object(lua, sol::nil), sol::make_object(lua, lua.create_table_with("message", error))};
  }
  return {sol::make_object(lua, std::move(*file)), sol::make_object(lua, sol::nil)};
}

}  // namespace

void BindJSON(sol::state& lua) {
  sol::usertype<JSON> store_type = lua.new_usertype<JSON>("JSON", sol::no_constructor);
  store_type.set_function("open", &JSON::Open);
  store_type["count"] = &JSON::Count;
  store_type["clear"] = &JSON::Clear;
  store_type["get"] = &JSON::Get;
  store_type["set"] = &JSON::Set;
  store_type["remove"] = &JSON::Remove;
  store_type["keyAt"] = &JSON::KeyAt;
}

}  // namespace lua::bindings

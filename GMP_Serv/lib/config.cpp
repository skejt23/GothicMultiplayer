 
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

#include "config.h"

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "shared/toml_wrapper.h"

namespace {
constexpr std::uint32_t kMaxNameLength = 100;

const std::unordered_map<std::string, std::variant<std::string, std::vector<std::string>, std::int32_t, bool, GothicClock::Time>>
    kDefault_Config_Values = {{"name", std::string("Gothic Multiplayer Server")},
                              {"port", 57005},
                              {"admin_passwd", std::string("")},
                              {"slots", 12},
                              {"public", false},
                              {"log_file", std::string("log.txt")},
                              {"map", std::string("NEWWORLD\\NEWWORLD.ZEN")},
                              {"respawn_time_seconds", 5},
                              {"allow_modification", true},
                              {"game_mode", 0},
                              {"allow_dropitems", true},
                              {"hide_map", false},
                              {"be_unconcious_before_dead", false},
                              {"observation", false},
                              {"quick_pots", false},
                              {"map_md5", std::string("")},
                              {"log_to_stdout", true},
                              {"log_level", std::string("info")},
                              {"game_time", GothicClock::Time{1u, 8u, 0u}},  // 8:00
                              {"scripts", std::vector<std::string>{std::string("main.lua")}},
                              {"character_definitions_file", std::string("")},
                              {"tick_rate_ms", 100},
#ifndef WIN32
                              {"daemon", true}
#else
                              {"daemon", false}
#endif
};

}  // namespace

Config::Config() {
  Load();
}

void Config::Load() {
  values_ = kDefault_Config_Values;

  TomlWrapper config;
  try {
    config = TomlWrapper::CreateFromFile("config.toml");
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Couldn't load config file: {}", ex.what());
    return;
  }

  for (auto& value : values_) {
    std::visit(
        [&config, &value](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          auto value_opt = config.GetValue<T>(value.first);
          if (value_opt) {
            value.second = *value_opt;
          }
        },
        value.second);
  }
  ValidateAndFixValues();
}

void Config::ValidateAndFixValues() {
  auto& name = std::get<std::string>(values_.at("name"));
  if (name.size() > kMaxNameLength) {
    name.resize(kMaxNameLength);
    SPDLOG_WARN("Truncated server name to {} since it exceeded the maximum length limit({})", name, kMaxNameLength);
  }

  auto& log_level = std::get<std::string>(values_.at("log_level"));
  const std::set<spdlog::string_view_t> valid_log_levels = SPDLOG_LEVEL_NAMES;
  auto it_level = valid_log_levels.find(spdlog::string_view_t(log_level));

  if (it_level == valid_log_levels.end()) {
    auto& default_log_level = std::get<std::string>(kDefault_Config_Values.at("log_level"));
    SPDLOG_WARN("Invalid log level in config: {}. Setting to default \"{}\"", log_level, default_log_level);
    values_["log_level"] = default_log_level;
  }
}

void Config::LogConfigValues() const {
  const std::set<std::string> keys_to_ignore = {"admin_passwd"};

  for (auto& value : values_) {
    std::visit(
        [&value, &keys_to_ignore](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if (keys_to_ignore.find(value.first) == keys_to_ignore.end()) {
            if constexpr (std::is_same_v<T, std::string>) {
              if (arg.empty()) {
                return;
              }
            }
            SPDLOG_INFO("{}: {}", value.first, arg);
          }
        },
        value.second);
  }
}
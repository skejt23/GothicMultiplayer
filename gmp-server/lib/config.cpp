 
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
#include <spdlog/fmt/fmt.h>
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
constexpr std::uint32_t kMaxAuthKeyLength = 32;

const std::unordered_map<std::string, std::variant<std::string, std::vector<std::string>, std::int32_t, bool>>
    kDefault_Config_Values = {{"name", std::string("Gothic Multiplayer Server")},
                              {"port", 57005},
                              {"public", false},
                              {"slots", 12},
                              {"admin_passwd", std::string("")},
                              {"auth_key", std::string("")},
                              {"map", std::string("NEWWORLD\\NEWWORLD.ZEN")},
                              {"map_md5", std::string("")},
                              {"allow_modification", true},
                              {"hide_map", false},
                              {"respawn_time_seconds", 5},
                              {"log_file", std::string("log.txt")},
                              {"log_to_stdout", true},
                              {"log_level", std::string("trace")},
                              {"scripts", std::vector<std::string>{std::string("main.lua")}},
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

  auto& auth_key = std::get<std::string>(values_.at("auth_key"));
  if (auth_key.size() > kMaxAuthKeyLength) {
    auth_key.resize(kMaxAuthKeyLength);
    SPDLOG_WARN("Truncated auth_key since it exceeded the maximum length limit({})", kMaxAuthKeyLength);
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
  constexpr std::string_view kFrame = "-========================================-";
  const auto bool_to_string = [](bool value) { return value ? "true" : "false"; };

  SPDLOG_INFO(kFrame);

  SPDLOG_INFO("-= Basic server information =-");
  SPDLOG_INFO("* {:<18}: {}", "Hostname", Get<std::string>("name"));
  SPDLOG_INFO("* {:<18}: {}", "Port", Get<std::int32_t>("port"));
  SPDLOG_INFO("* {:<18}: {}", "Public", bool_to_string(Get<bool>("public")));
  SPDLOG_INFO("* {:<18}: {}", "Max slots", Get<std::int32_t>("slots"));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Gameplay settings =-");
  SPDLOG_INFO("* {:<18}: {}", "Map", Get<std::string>("map"));
  const auto& map_md5 = Get<std::string>("map_md5");
  SPDLOG_INFO("* {:<18}: {}", "Map MD5", map_md5.empty() ? "<not set>" : map_md5);
  SPDLOG_INFO("* {:<18}: {}", "Allow modification", bool_to_string(Get<bool>("allow_modification")));
  SPDLOG_INFO("* {:<18}: {}", "Hide map", bool_to_string(Get<bool>("hide_map")));
  SPDLOG_INFO("* {:<18}: {}", "Respawn time", fmt::format("{}s", Get<std::int32_t>("respawn_time_seconds")));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Logging =-");
  SPDLOG_INFO("* {:<18}: {}", "Log file", Get<std::string>("log_file"));
  SPDLOG_INFO("* {:<18}: {}", "Log to stdout", bool_to_string(Get<bool>("log_to_stdout")));
  SPDLOG_INFO("* {:<18}: {}", "Log level", Get<std::string>("log_level"));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Scripts =-");
  const auto& scripts = Get<std::vector<std::string>>("scripts");
  SPDLOG_INFO("* {:<18}: {}", "Script count", scripts.size());

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Performance =-");
  SPDLOG_INFO("* {:<18}: {} ms", "Tick rate", Get<std::int32_t>("tick_rate_ms"));

#ifndef WIN32
  const bool daemon = Get<bool>("daemon");
  SPDLOG_INFO("");
  SPDLOG_INFO("-= Process management =-");
  SPDLOG_INFO("* {:<18}: {}", "Daemonize", bool_to_string(daemon));
#endif

  SPDLOG_INFO(kFrame);
}
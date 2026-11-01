
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

#include <sodium.h>
#include <spdlog/common.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "shared/toml_wrapper.h"

namespace {
constexpr std::uint32_t kMaxNameLength = 100;
constexpr std::uint32_t kMaxAuthKeyLength = 32;
constexpr std::size_t kServerIdentitySeedBytes = 32;

const std::unordered_map<std::string, std::variant<std::string, std::vector<std::string>, std::int32_t, bool>> kDefault_Config_Values = {
    {"name", std::string("Gothic Multiplayer Server")},
    {"port", 57005},
    {"public", false},
    {"slots", 12},
    {"admin_passwd", std::string("")},
    {"auth_key", std::string("")},
    {"server_identity_seed", std::string("")},
    {"map", std::string("NEWWORLD\\NEWWORLD.ZEN")},
    {"map_md5", std::string("")},
    {"allow_modification", true},
    {"hide_map", false},
    {"respawn_time_seconds", 5},
    {"voice_enabled", true},
    {"voice_range", 3500},
    {"resources", std::vector<std::string>{std::string("default"), std::string("prototype")}},
    {"addon_vdfs", std::vector<std::string>{}},
    {"downloader_group", std::string("")},
    {"downloader_file_max_chunk", 4 * 1024 * 1024},
    {"downloader_rate_limit", 30},
    {"downloader_download_timeout_seconds", 300},
    {"log_file", std::string("log.txt")},
    {"log_to_stdout", true},
    {"log_level", std::string("trace")},
    {"tick_rate_ms", 100},
    {"seconds_per_game_minute", 4},
    {"stream_radius", 5000},
    {"stream_height", 0},
#ifndef WIN32
    {"daemon", true}
#else
    {"daemon", false}
#endif
};

// Helper function to encode binary data to base64
std::string base64_encode(const unsigned char* data, size_t len) {
  const size_t encoded_max_len = sodium_base64_ENCODED_LEN(len, sodium_base64_VARIANT_ORIGINAL);
  std::string result(encoded_max_len, '\0');

  sodium_bin2base64(result.data(), encoded_max_len, data, len, sodium_base64_VARIANT_ORIGINAL);

  // Remove null terminator
  result.resize(std::strlen(result.c_str()));
  return result;
}

// Helper function to decode base64 to binary data
std::vector<unsigned char> base64_decode(const std::string& encoded) {
  std::vector<unsigned char> decoded(encoded.size());
  size_t decoded_len;

  if (sodium_base642bin(decoded.data(), decoded.size(), encoded.data(), encoded.size(), nullptr, &decoded_len, nullptr,
                        sodium_base64_VARIANT_ORIGINAL) != 0) {
    return {};
  }

  decoded.resize(decoded_len);
  return decoded;
}

}  // namespace

Config::Config() : Config("config.toml") {
}

Config::Config(std::filesystem::path config_path) : config_path_(std::move(config_path)) {
  if (sodium_init() < 0) {
    SPDLOG_ERROR("Failed to initialize libsodium");
    throw std::runtime_error("Failed to initialize libsodium");
  }
  Load();
}

void Config::Load() {
  values_ = kDefault_Config_Values;

  TomlWrapper config;
  try {
    config = TomlWrapper::CreateFromFile(config_path_.string());
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
  EnsureServerIdentitySeed();
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

  auto& resources = std::get<std::vector<std::string>>(values_.at("resources"));
  std::vector<std::string> normalized_resources;
  normalized_resources.reserve(resources.size());
  for (const auto& resource : resources) {
    if (resource.empty()) {
      SPDLOG_WARN("Ignoring empty resource name in config");
      continue;
    }

    if (std::find(normalized_resources.begin(), normalized_resources.end(), resource) != normalized_resources.end()) {
      SPDLOG_WARN("Ignoring duplicate resource '{}' in config", resource);
      continue;
    }

    normalized_resources.push_back(resource);
  }
  resources = std::move(normalized_resources);

  auto& addon_vdfs = std::get<std::vector<std::string>>(values_.at("addon_vdfs"));
  std::vector<std::string> normalized_addons;
  normalized_addons.reserve(addon_vdfs.size());
  for (const auto& addon : addon_vdfs) {
    if (addon.empty()) {
      SPDLOG_WARN("Ignoring empty addon VDF path in config");
      continue;
    }
    if (std::find(normalized_addons.begin(), normalized_addons.end(), addon) != normalized_addons.end()) {
      SPDLOG_WARN("Ignoring duplicate addon VDF '{}' in config", addon);
      continue;
    }
    normalized_addons.push_back(addon);
  }
  addon_vdfs = std::move(normalized_addons);

  auto& downloader_group = std::get<std::string>(values_.at("downloader_group"));
  if (downloader_group.size() > 64) {
    downloader_group.resize(64);
    SPDLOG_WARN("Truncated downloader_group to 64 characters");
  }
  auto& file_max_chunk = std::get<std::int32_t>(values_.at("downloader_file_max_chunk"));
  constexpr std::int32_t kMinimumChunk = 16 * 1024;
  constexpr std::int32_t kMaximumChunk = 16 * 1024 * 1024;
  if (file_max_chunk < kMinimumChunk || file_max_chunk > kMaximumChunk) {
    SPDLOG_WARN("Invalid downloader_file_max_chunk {}; using 4194304", file_max_chunk);
    file_max_chunk = 4 * 1024 * 1024;
  }
  auto& rate_limit = std::get<std::int32_t>(values_.at("downloader_rate_limit"));
  if (rate_limit < 0 || rate_limit > 10000) {
    SPDLOG_WARN("Invalid downloader_rate_limit {}; using 30", rate_limit);
    rate_limit = 30;
  }
  auto& download_timeout = std::get<std::int32_t>(values_.at("downloader_download_timeout_seconds"));
  if (download_timeout < 0 || download_timeout > 86400) {
    SPDLOG_WARN("Invalid downloader_download_timeout_seconds {}; using 300", download_timeout);
    download_timeout = 300;
  }

  auto& log_level = std::get<std::string>(values_.at("log_level"));
  const std::set<spdlog::string_view_t> valid_log_levels = SPDLOG_LEVEL_NAMES;
  auto it_level = valid_log_levels.find(spdlog::string_view_t(log_level));

  if (it_level == valid_log_levels.end()) {
    auto& default_log_level = std::get<std::string>(kDefault_Config_Values.at("log_level"));
    SPDLOG_WARN("Invalid log level in config: {}. Setting to default \"{}\"", log_level, default_log_level);
    values_["log_level"] = default_log_level;
  }

  for (const auto* key : {"seconds_per_game_minute", "stream_radius", "stream_height", "voice_range"}) {
    auto& value = std::get<std::int32_t>(values_.at(key));
    if (value < 0) {
      auto default_value = std::get<std::int32_t>(kDefault_Config_Values.at(key));
      SPDLOG_WARN("Invalid {} in config: {}. Setting to default {}", key, value, default_value);
      value = default_value;
    }
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
  SPDLOG_INFO("* {:<18}: {}", "Voice chat", bool_to_string(Get<bool>("voice_enabled")));
  SPDLOG_INFO("* {:<18}: {}", "Voice range", Get<std::int32_t>("voice_range"));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Resourcepacks =-");
  const auto& resources = Get<std::vector<std::string>>("resources");
  std::ostringstream resources_list;
  for (std::size_t i = 0; i < resources.size(); ++i) {
    if (i > 0) {
      resources_list << ", ";
    }
    resources_list << resources[i];
  }
  SPDLOG_INFO("* {:<18}: {}", "Resources", resources.empty() ? std::string("<none>") : resources_list.str());
  const auto& addon_vdfs = Get<std::vector<std::string>>("addon_vdfs");
  std::ostringstream addon_list;
  for (std::size_t i = 0; i < addon_vdfs.size(); ++i) {
    if (i > 0)
      addon_list << ", ";
    addon_list << addon_vdfs[i];
  }
  SPDLOG_INFO("* {:<18}: {}", "VDFs", addon_vdfs.empty() ? std::string("<none>") : addon_list.str());
  SPDLOG_INFO("* {:<18}: {}", "Download group",
              Get<std::string>("downloader_group").empty() ? std::string("<client ip_port>") : Get<std::string>("downloader_group"));
  SPDLOG_INFO("* {:<18}: {} bytes", "Download chunk", Get<std::int32_t>("downloader_file_max_chunk"));
  SPDLOG_INFO("* {:<18}: {}/min", "Download rate", Get<std::int32_t>("downloader_rate_limit"));
  SPDLOG_INFO("* {:<18}: {} s", "Download timeout", Get<std::int32_t>("downloader_download_timeout_seconds"));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Logging =-");
  SPDLOG_INFO("* {:<18}: {}", "Log file", Get<std::string>("log_file"));
  SPDLOG_INFO("* {:<18}: {}", "Log to stdout", bool_to_string(Get<bool>("log_to_stdout")));
  SPDLOG_INFO("* {:<18}: {}", "Log level", Get<std::string>("log_level"));

  SPDLOG_INFO("");
  SPDLOG_INFO("-= Performance =-");
  SPDLOG_INFO("* {:<18}: {} ms", "Tick rate", Get<std::int32_t>("tick_rate_ms"));
  SPDLOG_INFO("* {:<18}: {} s", "Secs per game min", Get<std::int32_t>("seconds_per_game_minute"));
  SPDLOG_INFO("* {:<18}: {}", "Stream radius", Get<std::int32_t>("stream_radius"));
  const auto stream_height = Get<std::int32_t>("stream_height");
  SPDLOG_INFO("* {:<18}: {}", "Stream height", stream_height == 0 ? std::string("unlimited") : std::to_string(stream_height));

#ifndef WIN32
  const bool daemon = Get<bool>("daemon");
  SPDLOG_INFO("");
  SPDLOG_INFO("-= Process management =-");
  SPDLOG_INFO("* {:<18}: {}", "Daemonize", bool_to_string(daemon));
#endif

  SPDLOG_INFO(kFrame);
}

void Config::EnsureServerIdentitySeed() {
  auto& seed = std::get<std::string>(values_.at("server_identity_seed"));

  // Preserve the existing website token and server-list identity.
  if (!seed.empty()) {
    auto decoded_seed = base64_decode(seed);

    if (decoded_seed.size() == kServerIdentitySeedBytes) {
      SPDLOG_INFO("Server website token loaded from config");
      return;
    } else {
      SPDLOG_WARN("Existing server seed is invalid (expected {} bytes, got {}), generating new one", kServerIdentitySeedBytes, decoded_seed.size());
    }
  }

  // Generate new random seed
  std::array<unsigned char, kServerIdentitySeedBytes> new_seed;
  randombytes_buf(new_seed.data(), new_seed.size());

  // Encode to base64 and store
  seed = base64_encode(new_seed.data(), new_seed.size());

  SPDLOG_INFO("Generated new server website token");

  // Save to config file
  SaveConfigToFile();
}

void Config::SaveConfigToFile() {
  try {
    auto config = TomlWrapper::CreateFromFile(config_path_.string());

    // Update the server seed in the TOML data
    config["server_identity_seed"] = Get<std::string>("server_identity_seed");

    config.Serialize(config_path_.string());

    SPDLOG_INFO("Saved server website token to {}", config_path_.string());
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to save config file: {}", ex.what());
  }
}

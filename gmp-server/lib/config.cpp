
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
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "shared/toml_wrapper.h"

namespace {
constexpr std::uint32_t kMaxNameLength = 100;
constexpr std::uint32_t kMaxAuthKeyLength = 32;

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

Config::Config() {
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
  EnsureServerKeys();
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

void Config::EnsureServerKeys() {
  auto& seed = std::get<std::string>(values_.at("server_identity_seed"));

  // Check if seed already exists and is valid
  if (!seed.empty()) {
    auto decoded_seed = base64_decode(seed);

    if (decoded_seed.size() == crypto_sign_SEEDBYTES) {
      SPDLOG_INFO("Server identity seed loaded from config");
      DeriveKeysFromSeed();
      return;
    } else {
      SPDLOG_WARN("Existing server seed is invalid (expected {} bytes, got {}), generating new one", crypto_sign_SEEDBYTES, decoded_seed.size());
    }
  }

  // Generate new random seed
  std::array<unsigned char, crypto_sign_SEEDBYTES> new_seed;
  randombytes_buf(new_seed.data(), new_seed.size());

  // Encode to base64 and store
  seed = base64_encode(new_seed.data(), new_seed.size());

  SPDLOG_INFO("Generated new server identity seed");

  // Derive and cache the keys
  DeriveKeysFromSeed();

  // Save to config file
  SaveConfigToFile();
}

void Config::DeriveKeysFromSeed() {
  const auto& seed_str = Get<std::string>("server_identity_seed");
  if (seed_str.empty()) {
    SPDLOG_ERROR("Cannot derive keys: seed is empty");
    cached_public_key_.clear();
    cached_private_key_.clear();
    return;
  }

  // Decode and cache the seed
  auto binary_seed = base64_decode(seed_str);
  if (binary_seed.size() != crypto_sign_SEEDBYTES) {
    SPDLOG_ERROR("Invalid seed size for key derivation: expected {} bytes, got {}", crypto_sign_SEEDBYTES, binary_seed.size());
    cached_public_key_.clear();
    cached_private_key_.clear();
    return;
  }

  // Derive keypair from seed
  std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> pk;
  std::array<unsigned char, crypto_sign_SECRETKEYBYTES> sk;

  if (crypto_sign_seed_keypair(pk.data(), sk.data(), binary_seed.data()) != 0) {
    SPDLOG_ERROR("Failed to derive keypair from seed");
    cached_public_key_.clear();
    cached_private_key_.clear();
    return;
  }

  // Cache the derived keys
  cached_public_key_.assign(pk.begin(), pk.end());
  cached_private_key_.assign(sk.begin(), sk.end());

  SPDLOG_DEBUG("Server identity keys derived and cached in memory");
}

void Config::SaveConfigToFile() {
  try {
    auto config = TomlWrapper::CreateFromFile("config.toml");

    // Update the server seed in the TOML data
    config["server_identity_seed"] = Get<std::string>("server_identity_seed");

    config.Serialize("config.toml");

    SPDLOG_INFO("Saved server identity seed to config.toml");
  } catch (const std::exception& ex) {
    SPDLOG_ERROR("Failed to save config file: {}", ex.what());
  }
}

// ============================================================================
// Binary accessors - for network transmission and crypto operations
// ============================================================================

std::vector<unsigned char> Config::GetServerPublicKeyBinary() const {
  return cached_public_key_;
}

std::vector<unsigned char> Config::GetServerPrivateKeyBinary() const {
  return cached_private_key_;
}

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

#pragma warning(disable : 4101)

#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <system_error>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "CServerList.h"

namespace {

#ifdef MASTER_SERVER_ENDPOINT
constexpr std::string_view kMasterServerEndpoint = MASTER_SERVER_ENDPOINT;
#else
constexpr std::string_view kMasterServerEndpoint{};
#endif

struct MasterServerEndpointInfo {
  std::string host;
  std::string path{"/"};
  int port{80};
  bool use_https{false};
};

std::optional<MasterServerEndpointInfo> ParseMasterServerEndpoint(std::string_view endpoint) {
  if (endpoint.empty()) {
    return std::nullopt;
  }

  MasterServerEndpointInfo info;
  std::string_view remainder = endpoint;

  auto scheme_pos = remainder.find("://");
  if (scheme_pos != std::string_view::npos) {
    auto scheme = remainder.substr(0, scheme_pos);
    if (scheme == "http") {
      info.use_https = false;
    } else if (scheme == "https") {
      info.use_https = true;
      info.port = 443;
    } else {
      return std::nullopt;
    }
    remainder.remove_prefix(scheme_pos + 3);
  }

  if (remainder.empty()) {
    return std::nullopt;
  }

  auto path_pos = remainder.find('/');
  std::string_view host_port = remainder;
  if (path_pos != std::string_view::npos) {
    host_port = remainder.substr(0, path_pos);
    auto path = remainder.substr(path_pos);
    info.path.assign(path.begin(), path.end());
    if (info.path.empty()) {
      info.path = "/";
    }
  }

  if (host_port.empty()) {
    return std::nullopt;
  }

  auto port_pos = host_port.rfind(':');
  if (port_pos != std::string_view::npos) {
    auto port_view = host_port.substr(port_pos + 1);
    int port_value = 0;
    auto result = std::from_chars(port_view.data(), port_view.data() + port_view.size(), port_value);
    if (result.ec != std::errc{}) {
      return std::nullopt;
    }
    info.port = port_value;
    host_port.remove_suffix(host_port.size() - port_pos);
  } else if (!info.use_https) {
    info.port = 80;
  }

  info.host.assign(host_port.begin(), host_port.end());

  if (info.host.empty()) {
    return std::nullopt;
  }

  return info;
}

std::unique_ptr<httplib::Client> CreateMasterServerClient(const MasterServerEndpointInfo& info) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (info.use_https) {
    auto client = std::make_unique<httplib::SSLClient>(info.host, info.port);
    client->enable_server_certificate_verification(true);
    return client;
  }
#else
  if (info.use_https) {
    return nullptr;
  }
#endif

  return std::make_unique<httplib::Client>(info.host, info.port);
}

unsigned short ClampToUint16(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > std::numeric_limits<unsigned short>::max()) {
    return std::numeric_limits<unsigned short>::max();
  }
  return static_cast<unsigned short>(value);
}

int ParseIntegerField(const nlohmann::json& object, const char* key, int default_value = 0) {
  auto it = object.find(key);
  if (it == object.end() || it->is_null()) {
    return default_value;
  }

  if (it->is_number_integer()) {
    return static_cast<int>(it->get<std::int64_t>());
  }

  if (it->is_number_unsigned()) {
    auto value = it->get<std::uint64_t>();
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
      return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
  }

  if (it->is_number_float()) {
    return static_cast<int>(it->get<double>());
  }

  if (it->is_string()) {
    const auto& str = it->get_ref<const std::string&>();
    int parsed_value = default_value;
    auto result = std::from_chars(str.data(), str.data() + str.size(), parsed_value);
    if (result.ec == std::errc{}) {
      return parsed_value;
    }
  }

  return default_value;
}
std::string SanitizeDisplayText(std::string text) {
  for (auto& ch : text) {
    unsigned char value = static_cast<unsigned char>(ch);
    if (value < 0x20 && value != 0x07) {
      ch = ' ';
    }
  }
  return text;
}

std::string ParseStringField(const nlohmann::json& object, const char* key) {
  auto it = object.find(key);
  if (it == object.end() || it->is_null()) {
    return {};
  }

  if (it->is_string()) {
    return it->get<std::string>();
  }

  if (it->is_number_integer()) {
    return std::to_string(it->get<std::int64_t>());
  }

  if (it->is_number_unsigned()) {
    return std::to_string(it->get<std::uint64_t>());
  }

  if (it->is_number_float()) {
    return std::to_string(it->get<double>());
  }

  return {};
}

}  // namespace

CServerList::~CServerList(void) {
  if (!this->server_vector.empty())
    this->server_vector.clear();
}

size_t CServerList::GetListSize() {
  return this->server_vector.size();
}

SServerInfo* CServerList::At(size_t pos) {
  if (pos >= this->server_vector.size())
    return NULL;
  return (SServerInfo*)&this->server_vector[pos];
}

void CServerList::Parse() {
  if (data.empty())
    return;
  if (!server_vector.empty())
    server_vector.clear();
  switch (list_type) {
    case HTTP_LIST: {
      nlohmann::json json_data;
      try {
        json_data = nlohmann::json::parse(data);
      } catch (const std::exception& ex) {
        SetError(ErrorCode::kParseError, std::string("Failed to parse master server response: ") + ex.what());
        data.clear();
        return;
      }

      const nlohmann::json* servers_json = nullptr;
      if (json_data.is_array()) {
        servers_json = &json_data;
      } else if (json_data.is_object()) {
        if (auto it = json_data.find("servers"); it != json_data.end() && it->is_array()) {
          servers_json = &(*it);
        }
      }

      if (servers_json == nullptr) {
        SetError(ErrorCode::kParseError, "Master server response did not contain a server list.");
        data.clear();
        return;
      }

      for (const auto& entry : *servers_json) {
        if (!entry.is_object()) {
          continue;
        }

        auto ip = ParseStringField(entry, "ip");
        if (ip.empty()) {
          ip = ParseStringField(entry, "address");
        }

        const auto port_value = ParseIntegerField(entry, "port", 0);
        if (ip.empty() || port_value <= 0) {
          continue;
        }

        SServerInfo tmp_sv_inf{};
        tmp_sv_inf.Clear();

        tmp_sv_inf.port = ClampToUint16(port_value);

        auto name = ParseStringField(entry, "servername");
        if (name.empty()) {
          name = ParseStringField(entry, "name");
        }

        auto gamemode = ParseStringField(entry, "gamemode");
        if (gamemode.empty()) {
          gamemode = ParseStringField(entry, "map");
        }

        auto description = ParseStringField(entry, "description");

        name = SanitizeDisplayText(std::move(name));
        gamemode = SanitizeDisplayText(std::move(gamemode));
        description = SanitizeDisplayText(std::move(description));

        tmp_sv_inf.ip = ip.c_str();
        tmp_sv_inf.name = name.c_str();
        tmp_sv_inf.map = gamemode.c_str();
        tmp_sv_inf.server_website = description.c_str();

        tmp_sv_inf.num_of_players = ClampToUint16(ParseIntegerField(entry, "players", 0));
        tmp_sv_inf.max_players = ClampToUint16(ParseIntegerField(entry, "maxslots", 0));
        tmp_sv_inf.ping = ClampToUint16(ParseIntegerField(entry, "ping", 0));

        server_vector.push_back(tmp_sv_inf);
      }

      SetError(ErrorCode::kNone, "");
    } break;
    case UDP_LIST:
      break;
    default:
      printf("SNE exception executed!\n");
      // should never execute
      break;
  }
  data.clear();
}

bool CServerList::ReceiveListHttp() {
  server_vector.clear();
  data.clear();
  SetError(ErrorCode::kNone, "");

  if (kMasterServerEndpoint.empty()) {
    SetError(ErrorCode::kNotConfigured, "Master server endpoint is not configured.");
    return false;
  }

  auto endpoint_info_opt = ParseMasterServerEndpoint(kMasterServerEndpoint);
  if (!endpoint_info_opt) {
    SetError(ErrorCode::kInvalidEndpoint, "Master server endpoint has an invalid format.");
    return false;
  }

  const auto endpoint_info = *endpoint_info_opt;

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
  if (endpoint_info.use_https) {
    SetError(ErrorCode::kConnectionFailed, "Master server endpoint requires HTTPS support, but this build lacks OpenSSL.");
    return false;
  }
#endif

  auto client = CreateMasterServerClient(endpoint_info);
  if (!client) {
    SetError(ErrorCode::kConnectionFailed, "Unable to initialize HTTP client for the master server endpoint.");
    return false;
  }

  client->set_connection_timeout(5, 0);
  client->set_read_timeout(5, 0);
  client->set_write_timeout(5, 0);

  auto response = client->Get(endpoint_info.path.c_str());
  if (!response) {
    SetError(ErrorCode::kConnectionFailed, "Failed to connect to the master server endpoint.");
    return false;
  }

  if (response->status != 200) {
    SetError(ErrorCode::kBadResponse, "Master server responded with HTTP status " + std::to_string(response->status) + ".");
    return false;
  }

  data = response->body;
  list_type = HTTP_LIST;
  Parse();
  return error_code == ErrorCode::kNone;
}

bool CServerList::ReceiveListUDP() {
  SetError(ErrorCode::kNone, "");
  return true;
}

const char* CServerList::GetLastError() {
  if (error_message_.empty()) {
    return nullptr;
  }
  return error_message_.c_str();
}

void CServerList::SetError(ErrorCode code, std::string message) {
  error_code = code;
  error_message_ = std::move(message);
}

void SServerInfo::Clear() {
  if (!this->ip.IsEmpty())
    this->ip.Clear();
  if (!this->name.IsEmpty())
    this->name.Clear();
  if (!this->map.IsEmpty())
    this->map.Clear();
  if (!this->server_website.IsEmpty())
    this->server_website.Clear();
  this->num_of_players = 0;
  this->max_players = 0;
  this->port = 0;
  this->ping = 0;
}
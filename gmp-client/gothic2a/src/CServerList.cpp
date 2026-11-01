
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

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <system_error>
#include <utility>

#include <httplib.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <nlohmann/json.hpp>

#include "CServerList.h"
#include "master_server_client.h"

namespace {

#ifdef MASTER_SERVER_ENDPOINT
constexpr std::string_view kMasterServerEndpoint = MASTER_SERVER_ENDPOINT;
#else
constexpr std::string_view kMasterServerEndpoint{};
#endif

unsigned short ClampToUint16(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > std::numeric_limits<unsigned short>::max()) {
    return std::numeric_limits<unsigned short>::max();
  }
  return static_cast<unsigned short>(value);
}

int ClampToInt(std::int64_t value) {
  if (value < std::numeric_limits<int>::min()) {
    return std::numeric_limits<int>::min();
  }
  if (value > std::numeric_limits<int>::max()) {
    return std::numeric_limits<int>::max();
  }
  return static_cast<int>(value);
}

int ClampToInt(std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
    return std::numeric_limits<int>::max();
  }
  return static_cast<int>(value);
}

std::optional<int> TryParseIntegerField(const nlohmann::json& object, const char* key) {
  auto it = object.find(key);
  if (it == object.end() || it->is_null()) {
    return std::nullopt;
  }

  if (it->is_number_unsigned()) {
    return ClampToInt(it->get<std::uint64_t>());
  }

  if (it->is_number_integer()) {
    return ClampToInt(it->get<std::int64_t>());
  }

  if (it->is_number_float()) {
    const auto value = it->get<double>();
    if (!std::isfinite(value)) {
      return std::nullopt;
    }
    if (value < static_cast<double>(std::numeric_limits<int>::min())) {
      return std::numeric_limits<int>::min();
    }
    if (value > static_cast<double>(std::numeric_limits<int>::max())) {
      return std::numeric_limits<int>::max();
    }
    return static_cast<int>(value);
  }

  if (it->is_string()) {
    const auto& str = it->get_ref<const std::string&>();
    int parsed_value = 0;
    auto result = std::from_chars(str.data(), str.data() + str.size(), parsed_value);
    if (result.ec == std::errc{} && result.ptr == str.data() + str.size()) {
      return parsed_value;
    }
  }

  return std::nullopt;
}

int ParseIntegerField(const nlohmann::json& object, const char* key, int default_value = 0) {
  return TryParseIntegerField(object, key).value_or(default_value);
}

bool IsValidPort(int value) {
  return value > 0 && value <= std::numeric_limits<unsigned short>::max();
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

  if (it->is_number_unsigned()) {
    return std::to_string(it->get<std::uint64_t>());
  }

  if (it->is_number_integer()) {
    return std::to_string(it->get<std::int64_t>());
  }

  if (it->is_number_float()) {
    return std::to_string(it->get<double>());
  }

  return {};
}

int PingHost(const std::string& host) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* address = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &address) != 0) {
    return -1;
  }
  const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resolved(address, freeaddrinfo);
  if (!address || !address->ai_addr) {
    return -1;
  }
  const auto destination = reinterpret_cast<const sockaddr_in*>(address->ai_addr)->sin_addr.s_addr;
  const HANDLE handle = IcmpCreateFile();
  if (handle == INVALID_HANDLE_VALUE) {
    return -1;
  }
  alignas(ICMP_ECHO_REPLY) std::array<unsigned char, sizeof(ICMP_ECHO_REPLY) + 8> buffer{};
  const auto count = IcmpSendEcho(handle, destination, nullptr, 0, nullptr, buffer.data(), static_cast<DWORD>(buffer.size()), 1000);
  IcmpCloseHandle(handle);
  const auto* reply = reinterpret_cast<const ICMP_ECHO_REPLY*>(buffer.data());
  if (count == 0 || reply->Status != IP_SUCCESS) {
    return -1;
  }
  return ClampToInt(static_cast<std::uint64_t>(reply->RoundTripTime));
}

}  // namespace

CServerList::~CServerList(void) {
  stop_pinging_->store(true);
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

        auto ip = ParseStringField(entry, "ip_address");

        const auto port_value = ParseIntegerField(entry, "port", 0);
        if (ip.empty() || !IsValidPort(port_value)) {
          continue;
        }

        SServerInfo tmp_sv_inf{};
        tmp_sv_inf.Clear();

        tmp_sv_inf.port = static_cast<unsigned short>(port_value);

        auto name = ParseStringField(entry, "name");

        auto gamemode = ParseStringField(entry, "map");

        auto description = ParseStringField(entry, "description");

        name = SanitizeDisplayText(std::move(name));
        gamemode = SanitizeDisplayText(std::move(gamemode));
        description = SanitizeDisplayText(std::move(description));

        tmp_sv_inf.ip = ip.c_str();
        tmp_sv_inf.name = name.c_str();
        tmp_sv_inf.map = gamemode.c_str();
        tmp_sv_inf.server_website = description.c_str();

        tmp_sv_inf.num_of_players = ClampToUint16(ParseIntegerField(entry, "current_players", 0));
        tmp_sv_inf.max_players = ClampToUint16(ParseIntegerField(entry, "max_slots", 0));
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

CServerList::FetchResult CServerList::FetchBody() {
  if (kMasterServerEndpoint.empty()) {
    return {ErrorCode::kNotConfigured, {}, "Master server endpoint is not configured."};
  }

  auto endpoint_info_opt = master_server::ParseEndpoint(kMasterServerEndpoint);
  if (!endpoint_info_opt) {
    return {ErrorCode::kInvalidEndpoint, {}, "Master server endpoint must be a valid HTTPS URL."};
  }

  const auto endpoint_info = *endpoint_info_opt;

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
  if (endpoint_info.use_https) {
    return {ErrorCode::kConnectionFailed, {}, "Master server endpoint requires HTTPS support, but this build lacks OpenSSL."};
  }
#endif

  auto client = master_server::CreateClient(endpoint_info);
  if (!client) {
    return {ErrorCode::kConnectionFailed, {}, "Unable to initialize HTTPS client for the master server endpoint."};
  }

  client->set_connection_timeout(5, 0);
  client->set_read_timeout(5, 0);
  client->set_write_timeout(5, 0);

  auto response = client->Get(endpoint_info.path.c_str());
  if (!response) {
    return {ErrorCode::kConnectionFailed, {}, "Failed to connect to the master server endpoint."};
  }

  if (response->status != 200) {
    return {ErrorCode::kBadResponse, {}, "Master server responded with HTTP status " + std::to_string(response->status) + "."};
  }

  return {ErrorCode::kNone, std::move(response->body), {}};
}

bool CServerList::ApplyFetchResult(FetchResult result) {
  SetError(result.error, std::move(result.message));
  if (result.error != ErrorCode::kNone) {
    return false;
  }
  server_vector.clear();
  data = std::move(result.body);
  list_type = HTTP_LIST;
  Parse();
  return error_code == ErrorCode::kNone;
}

bool CServerList::ReceiveListHttp() {
  return ApplyFetchResult(FetchBody());
}

bool CServerList::BeginRefresh() {
  if (refresh_result_.valid()) {
    return true;
  }
  try {
    refresh_result_ = std::async(std::launch::async, &CServerList::FetchBody);
    return true;
  } catch (const std::exception& error) {
    SetError(ErrorCode::kConnectionFailed, error.what());
    return false;
  }
}

std::optional<bool> CServerList::PollRefresh() {
  if (!refresh_result_.valid() || refresh_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return std::nullopt;
  }
  try {
    return ApplyFetchResult(refresh_result_.get());
  } catch (const std::exception& error) {
    SetError(ErrorCode::kConnectionFailed, error.what());
    return false;
  }
}

void CServerList::BeginPings(std::vector<std::string> hosts) {
  if (ping_result_.valid() || hosts.empty()) {
    return;
  }
  std::sort(hosts.begin(), hosts.end());
  hosts.erase(std::unique(hosts.begin(), hosts.end()), hosts.end());
  ping_result_ = std::async(std::launch::async, [hosts = std::move(hosts), stop = stop_pinging_]() {
    PingResults results;
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
      return results;
    }
    struct Cleanup {
      ~Cleanup() { WSACleanup(); }
    } cleanup;
    for (const auto& host : hosts) {
      if (stop->load()) {
        break;
      }
      results.emplace_back(host, PingHost(host));
    }
    return results;
  });
}

std::optional<CServerList::PingResults> CServerList::PollPings() {
  if (!ping_result_.valid() || ping_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return std::nullopt;
  }
  return ping_result_.get();
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

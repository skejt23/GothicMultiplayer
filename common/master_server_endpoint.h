/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace master_server {

struct EndpointInfo {
  std::string host;
  std::string path{"/"};
  int port{443};
  bool use_https{true};
};

namespace detail {

inline bool IsAsciiWhitespace(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

inline std::string_view TrimAsciiWhitespace(std::string_view value) {
  while (!value.empty() && IsAsciiWhitespace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && IsAsciiWhitespace(value.back())) {
    value.remove_suffix(1);
  }
  return value;
}

inline bool ParsePort(std::string_view port_view, int& port) {
  if (port_view.empty()) {
    return false;
  }

  int parsed_port = 0;
  const auto* first = port_view.data();
  const auto* last = first + port_view.size();
  const auto result = std::from_chars(first, last, parsed_port);
  if (result.ec != std::errc{} || result.ptr != last || parsed_port <= 0 || parsed_port > 65535) {
    return false;
  }

  port = parsed_port;
  return true;
}

inline void AssignPath(EndpointInfo& info, std::string_view path) {
  info.path.clear();
  if (path.empty()) {
    info.path = "/";
    return;
  }

  if (path.front() == '?') {
    info.path = "/";
  }
  info.path.append(path.begin(), path.end());
}

}  // namespace detail

inline std::optional<EndpointInfo> ParseEndpoint(std::string_view endpoint) {
  endpoint = detail::TrimAsciiWhitespace(endpoint);
  if (!endpoint.starts_with("https://")) {
    return std::nullopt;
  }

  EndpointInfo info;
  std::string_view remainder = endpoint.substr(std::string_view("https://").size());

  if (remainder.empty()) {
    return std::nullopt;
  }

  const auto path_pos = remainder.find_first_of("/?");
  std::string_view host_port = remainder;
  if (path_pos != std::string_view::npos) {
    host_port = remainder.substr(0, path_pos);
    detail::AssignPath(info, remainder.substr(path_pos));
  }

  if (host_port.empty()) {
    return std::nullopt;
  }

  std::string_view host = host_port;
  if (host_port.front() == '[') {
    const auto close_bracket_pos = host_port.find(']');
    if (close_bracket_pos == std::string_view::npos) {
      return std::nullopt;
    }

    host = host_port.substr(1, close_bracket_pos - 1);
    const auto port_suffix = host_port.substr(close_bracket_pos + 1);
    if (!port_suffix.empty()) {
      if (port_suffix.front() != ':') {
        return std::nullopt;
      }
      if (!detail::ParsePort(port_suffix.substr(1), info.port)) {
        return std::nullopt;
      }
    }
  } else {
    const auto port_pos = host_port.rfind(':');
    if (port_pos != std::string_view::npos) {
      if (!detail::ParsePort(host_port.substr(port_pos + 1), info.port)) {
        return std::nullopt;
      }
      host = host_port.substr(0, port_pos);
    }

    if (host.find(':') != std::string_view::npos) {
      return std::nullopt;
    }
  }

  host = detail::TrimAsciiWhitespace(host);
  if (host.empty() || host.find_first_of(" \t\r\n\f\v@#\\[]") != std::string_view::npos) {
    return std::nullopt;
  }

  info.host.assign(host.begin(), host.end());
  return info;
}

}  // namespace master_server

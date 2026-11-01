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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "common_structs.h"
#include "transport/transport.h"

namespace httplib {
class Server;
struct Request;
struct Response;
}  // namespace httplib

class ResourceServer {
public:
  ResourceServer(std::uint16_t port, std::filesystem::path public_dir, std::size_t file_max_chunk,
                 std::uint32_t rate_limit_per_minute, std::uint32_t download_timeout_seconds);
  ~ResourceServer();

  bool Start();
  void Stop();

  std::string IssueToken(Net::ConnectionHandle connection);
  void RevokeToken(Net::ConnectionHandle connection);
  bool RegisterAllowedAsset(std::string logical_path, std::filesystem::path source_path);

private:
  void HandleDownloadRequest(const httplib::Request& req, httplib::Response& res);
  bool IsTokenValid(const std::string& token) const;
  bool AcceptRequestFrom(std::string_view address);
  std::optional<std::filesystem::path> ResolvePublicAssetPath(std::string_view requested_path) const;

  std::uint16_t port_;
  std::filesystem::path public_dir_;
  std::unique_ptr<httplib::Server> http_server_;
  std::thread http_thread_;
  std::atomic<bool> running_{false};
  std::size_t file_max_chunk_;
  std::uint32_t rate_limit_per_minute_;
  std::uint32_t download_timeout_seconds_;

  struct RequestWindow {
    std::chrono::steady_clock::time_point started;
    std::uint32_t count{0};
  };
  std::mutex rate_limit_mutex_;
  std::unordered_map<std::string, RequestWindow> request_windows_;
  std::uint64_t request_counter_{0};

  mutable std::mutex token_mutex_;
  std::unordered_map<std::string, Net::ConnectionHandle> token_to_connection_;
  std::unordered_map<Net::ConnectionHandle, std::string> connection_to_token_;
  mutable std::mutex asset_mutex_;
  std::unordered_map<std::string, std::filesystem::path> allowed_assets_;
};

#include "resource_server.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <random>
#include <sstream>
#include <vector>

namespace {
std::string GenerateResourceToken() {
  constexpr std::size_t kTokenBytes = 16;
  std::array<std::uint8_t, kTokenBytes> buffer{};
  static thread_local std::mt19937_64 rng(std::random_device{}());
  for (auto& byte : buffer) {
    byte = static_cast<std::uint8_t>(rng() & 0xFF);
  }

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (auto byte : buffer) {
    oss << std::setw(2) << static_cast<int>(byte);
  }
  return oss.str();
}
}  // namespace

bool ResourceServer::RegisterAllowedAsset(std::string logical_path, std::filesystem::path source_path) {
  namespace fs = std::filesystem;
  fs::path logical(logical_path);
  logical = logical.lexically_normal();
  if (logical.empty() || logical.is_absolute()) {
    return false;
  }
  for (const auto& part : logical) {
    if (part == "..") {
      return false;
    }
  }
  std::error_code ec;
  source_path = fs::weakly_canonical(source_path, ec);
  if (ec || !fs::is_regular_file(source_path, ec) || ec) {
    return false;
  }
  std::lock_guard<std::mutex> lock(asset_mutex_);
  allowed_assets_[logical.generic_string()] = std::move(source_path);
  return true;
}

ResourceServer::ResourceServer(std::uint16_t port, std::filesystem::path public_dir, std::size_t file_max_chunk,
                               std::uint32_t rate_limit_per_minute, std::uint32_t download_timeout_seconds)
    : port_(port),
      public_dir_(std::move(public_dir)),
      file_max_chunk_(file_max_chunk),
      rate_limit_per_minute_(rate_limit_per_minute),
      download_timeout_seconds_(download_timeout_seconds) {}

ResourceServer::~ResourceServer() {
  Stop();
}

bool ResourceServer::Start() {
  http_server_ = std::make_unique<httplib::Server>();
  if (download_timeout_seconds_ > 0) {
    http_server_->set_read_timeout(download_timeout_seconds_, 0);
    http_server_->set_write_timeout(download_timeout_seconds_, 0);
  }
  http_server_->Get(R"(/public/(.+))", [this](const httplib::Request& req, httplib::Response& res) { HandleDownloadRequest(req, res); });

  SPDLOG_INFO("Serving client resources from '{}'", public_dir_.string());
  if (!std::filesystem::exists(public_dir_)) {
    SPDLOG_WARN("Resource public directory '{}' does not exist yet; downloads will fail until packs are produced.", public_dir_.string());
  }

  const char* bind_address = "0.0.0.0";
  auto bound_port = http_server_->bind_to_port(bind_address, port_);
  if (!bound_port) {
    SPDLOG_ERROR("Failed to bind resource HTTP server on {}:{}", bind_address, port_);
    http_server_.reset();
    return false;
  }

  running_.store(true, std::memory_order_release);
  http_thread_ = std::thread([this]() { http_server_->listen_after_bind(); });

  SPDLOG_INFO("Resource HTTP server listening on tcp:{}", port_);
  return true;
}

void ResourceServer::Stop() {
  if (http_server_) {
    http_server_->stop();
  }
  if (http_thread_.joinable()) {
    http_thread_.join();
  }
  running_.store(false, std::memory_order_release);
  http_server_.reset();
}

std::string ResourceServer::IssueToken(Net::ConnectionHandle connection) {
  auto token = GenerateResourceToken();
  std::lock_guard<std::mutex> lock(token_mutex_);
  token_to_connection_[token] = connection;
  connection_to_token_[connection] = token;
  return token;
}

void ResourceServer::RevokeToken(Net::ConnectionHandle connection) {
  std::lock_guard<std::mutex> lock(token_mutex_);
  auto it = connection_to_token_.find(connection);
  if (it == connection_to_token_.end()) {
    return;
  }
  token_to_connection_.erase(it->second);
  connection_to_token_.erase(it);
}

bool ResourceServer::IsTokenValid(const std::string& token) const {
  std::lock_guard<std::mutex> lock(token_mutex_);
  return token_to_connection_.find(token) != token_to_connection_.end();
}

bool ResourceServer::AcceptRequestFrom(std::string_view address) {
  if (rate_limit_per_minute_ == 0) {
    return true;
  }
  const auto now = std::chrono::steady_clock::now();
  constexpr auto kWindow = std::chrono::minutes(1);
  std::lock_guard<std::mutex> lock(rate_limit_mutex_);
  auto& window = request_windows_[std::string(address)];
  if (window.started.time_since_epoch().count() == 0 || now - window.started >= kWindow) {
    window = RequestWindow{now, 1};
  } else if (window.count >= rate_limit_per_minute_) {
    return false;
  } else {
    ++window.count;
  }

  if (++request_counter_ % 256 == 0) {
    std::erase_if(request_windows_, [now](const auto& entry) { return now - entry.second.started >= std::chrono::minutes(2); });
  }
  return true;
}

std::optional<std::filesystem::path> ResourceServer::ResolvePublicAssetPath(std::string_view requested_path) const {
  namespace fs = std::filesystem;
  fs::path rel_path(requested_path.begin(), requested_path.end());
  rel_path = rel_path.lexically_normal();
  if (rel_path.empty() || rel_path.is_absolute()) {
    return std::nullopt;
  }

  for (const auto& part : rel_path) {
    if (part == "..") {
      return std::nullopt;
    }
  }

  {
    std::lock_guard<std::mutex> lock(asset_mutex_);
    const auto allowed = allowed_assets_.find(rel_path.generic_string());
    if (allowed != allowed_assets_.end()) {
      return allowed->second;
    }
  }

  std::error_code ec;
  fs::path candidate = public_dir_ / rel_path;
  if (fs::is_regular_file(candidate, ec)) {
    return candidate;
  }

  fs::path fallback = public_dir_ / rel_path.filename();
  if (fs::is_regular_file(fallback, ec)) {
    return fallback;
  }

  return std::nullopt;
}

void ResourceServer::HandleDownloadRequest(const httplib::Request& req, httplib::Response& res) {
  if (!AcceptRequestFrom(req.remote_addr)) {
    res.status = 429;
    res.set_header("Retry-After", "60");
    res.set_content("rate limit exceeded", "text/plain");
    return;
  }

  auto token_it = req.params.find("token");
  if (token_it == req.params.end()) {
    res.status = 401;
    res.set_content("missing token", "text/plain");
    return;
  }

  if (!IsTokenValid(token_it->second)) {
    res.status = 403;
    res.set_content("invalid token", "text/plain");
    return;
  }

  if (req.matches.size() < 2) {
    res.status = 404;
    res.set_content("not found", "text/plain");
    return;
  }

  const std::string requested_path = req.matches[1];
  auto resolved_path = ResolvePublicAssetPath(requested_path);
  if (!resolved_path.has_value()) {
    res.status = 404;
    res.set_content("not found", "text/plain");
    return;
  }

  std::error_code ec;
  const auto size = std::filesystem::file_size(*resolved_path, ec);
  if (ec) {
    res.status = 404;
    res.set_content("not found", "text/plain");
    return;
  }

  auto file = std::make_shared<std::ifstream>(*resolved_path, std::ios::binary);
  auto file_mutex = std::make_shared<std::mutex>();
  auto buffer = std::make_shared<std::vector<char>>(file_max_chunk_);
  if (!file->is_open()) {
    res.status = 404;
    res.set_content("not found", "text/plain");
    return;
  }
  const auto request_deadline = download_timeout_seconds_ == 0
                                    ? std::chrono::steady_clock::time_point::max()
                                    : std::chrono::steady_clock::now() + std::chrono::seconds(download_timeout_seconds_);
  res.set_header("Accept-Ranges", "bytes");
  res.set_content_provider(
      static_cast<std::size_t>(size), "application/octet-stream",
      [file, file_mutex, buffer, request_deadline](std::size_t offset, std::size_t length, httplib::DataSink& sink) {
        if (std::chrono::steady_clock::now() >= request_deadline) {
          return false;
        }
        std::lock_guard<std::mutex> lock(*file_mutex);
        file->clear();
        file->seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!*file) return false;
        std::size_t remaining = length;
        while (remaining > 0) {
          if (std::chrono::steady_clock::now() >= request_deadline) {
            return false;
          }
          const auto wanted = std::min(remaining, buffer->size());
          file->read(buffer->data(), static_cast<std::streamsize>(wanted));
          const auto count = file->gcount();
          if (count <= 0 || !sink.write(buffer->data(), static_cast<std::size_t>(count))) return false;
          remaining -= static_cast<std::size_t>(count);
        }
        return true;
      });
}

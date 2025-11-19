#include "resource_server.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

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

ResourceServer::ResourceServer(std::uint16_t port, std::filesystem::path public_dir) : port_(port), public_dir_(std::move(public_dir)) {
}

ResourceServer::~ResourceServer() {
  Stop();
}

bool ResourceServer::Start() {
  http_server_ = std::make_unique<httplib::Server>();
  http_server_->Get(R"(/public/(.+))", [this](const httplib::Request& req, httplib::Response& res) { HandleDownloadRequest(req, res); });

  SPDLOG_INFO("Serving client resources from '{}'", public_dir_.string());
  if (!std::filesystem::exists(public_dir_)) {
    SPDLOG_WARN("Resource public directory '{}' does not exist yet; downloads will fail until packs are produced.", public_dir_.string());
  }

  const char* bind_address = "0.0.0.0";
  auto bound_port = http_server_->bind_to_port(bind_address, port_);
  if (bound_port <= 0) {
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

  std::ifstream file(resolved_path->string(), std::ios::binary);
  if (!file) {
    res.status = 404;
    res.set_content("not found", "text/plain");
    return;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  auto payload = buffer.str();
  res.set_content(std::move(payload), "application/octet-stream");
}

#include "resource_downloader.h"

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "shared/crypto_utils.h"

namespace gmp::client {

ResourceDownloader::ResourceDownloader(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler)
    : event_observer_(eventObserver), task_scheduler_(taskScheduler) {}

ResourceDownloader::~ResourceDownloader() {
  StopDownload();
}

void ResourceDownloader::SetServerEndpoint(const std::string& ip, std::uint32_t port) {
  server_ip_ = ip;
  server_port_ = port;
}

void ResourceDownloader::SetDownloadToken(const std::string& token) {
  resource_download_token_ = token;
}

void ResourceDownloader::SetBasePath(const std::string& path) {
  resource_base_path_ = path.empty() ? "/public" : path;
}

void ResourceDownloader::AnnounceResources(std::vector<ClientResourceInfoEntry> resources) {
  std::lock_guard<std::mutex> lock(resource_mutex_);
  announced_resources_ = std::move(resources);
}

void ResourceDownloader::StopDownload() {
  resource_download_cancelled_.store(true, std::memory_order_release);
  if (resource_download_thread_.joinable()) {
    resource_download_thread_.join();
  }
  resource_download_cancelled_.store(false, std::memory_order_release);
}

void ResourceDownloader::Reset() {
  StopDownload();
  resources_ready_.store(false, std::memory_order_release);
  resource_download_failed_.store(false, std::memory_order_release);
  resource_download_error_.clear();
  resource_download_token_.clear();
  resource_base_path_ = "/public";
  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    announced_resources_.clear();
    downloaded_resources_.clear();
  }
}

std::vector<ResourceDownloader::ResourcePayload> ResourceDownloader::ConsumeDownloadedResources() {
  std::lock_guard<std::mutex> lock(resource_mutex_);
  std::vector<ResourcePayload> payloads = std::move(downloaded_resources_);
  downloaded_resources_.clear();
  return payloads;
}

void ResourceDownloader::BeginDownload() {
  StopDownload();

  std::vector<ClientResourceInfoEntry> resources;
  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    resources = announced_resources_;
  }

  if (resources.empty()) {
    SPDLOG_INFO("No client resources announced; skipping download phase");
    NotifyResourcesReady();
    return;
  }

  if (resource_download_token_.empty()) {
    HandleResourceDownloadFailure("Server did not provide a resource download token");
    return;
  }

  if (server_ip_.empty() || server_port_ == 0) {
    HandleResourceDownloadFailure("Missing server endpoint information for resource download");
    return;
  }

  std::uint64_t total_bytes = 0;
  for (const auto& entry : resources) {
    total_bytes += entry.archive_size;
  }

  SPDLOG_INFO("Requesting consent to download {} resource pack(s) totaling {} bytes", resources.size(), total_bytes);

  if (!event_observer_.RequestResourceDownloadConsent(resources.size(), total_bytes)) {
    HandleResourceDownloadFailure("Client declined required resource download");
    return;
  }

  SPDLOG_INFO("Consent granted; launching download thread");

  resource_download_cancelled_.store(false, std::memory_order_release);
  resource_download_thread_ = std::thread([this, total_bytes, resources = std::move(resources)]() mutable {
    try {
      ResourceDownloadWorker(std::move(resources), total_bytes);
    } catch (const std::exception& ex) {
      HandleResourceDownloadFailure(ex.what());
    }
  });
}

void ResourceDownloader::ResourceDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::uint64_t total_bytes) {
  SPDLOG_INFO("Starting download of {} client resource pack(s) ({} bytes)", resources.size(), total_bytes);

  httplib::Client client(server_ip_.c_str(), static_cast<int>(server_port_));
  client.set_read_timeout(300, 0);
  client.set_write_timeout(30, 0);
  client.set_follow_location(true);

  std::vector<ResourcePayload> downloaded;
  downloaded.reserve(resources.size());

  std::uint64_t downloaded_bytes = 0;
  for (const auto& resource : resources) {
    if (resource_download_cancelled_.load(std::memory_order_acquire)) {
      SPDLOG_INFO("Resource download cancelled before completion");
      return;
    }

    SPDLOG_INFO("Fetching manifest '{}'", resource.manifest_path);
    const auto manifest_path = BuildDownloadPath(resource.manifest_path);
    const auto manifest_result = client.Get(manifest_path.c_str());
    if (!manifest_result) {
      HandleResourceDownloadFailure("Failed to download manifest '" + resource.manifest_path + "'");
      return;
    }
    if (manifest_result->status != 200) {
      HandleResourceDownloadFailure("HTTP " + std::to_string(manifest_result->status) + " while downloading manifest '" + resource.manifest_path + "'");
      return;
    }

    const auto* manifest_bytes = reinterpret_cast<const std::uint8_t*>(manifest_result->body.data());
    if (!VerifyDigest(resource.manifest_sha256, manifest_bytes, manifest_result->body.size())) {
      HandleResourceDownloadFailure("Manifest digest mismatch for '" + resource.manifest_path + "'");
      return;
    }

    if (resource_download_cancelled_.load(std::memory_order_acquire)) {
      SPDLOG_INFO("Resource download cancelled before completion");
      return;
    }

    SPDLOG_INFO("Fetching archive '{}'", resource.archive_path);
    const auto archive_path = BuildDownloadPath(resource.archive_path);
    const auto archive_result = client.Get(archive_path.c_str());
    if (!archive_result) {
      HandleResourceDownloadFailure("Failed to download archive '" + resource.archive_path + "'");
      return;
    }
    if (archive_result->status != 200) {
      HandleResourceDownloadFailure("HTTP " + std::to_string(archive_result->status) + " while downloading archive '" + resource.archive_path + "'");
      return;
    }

    const auto* archive_bytes = reinterpret_cast<const std::uint8_t*>(archive_result->body.data());
    if (archive_result->body.size() != resource.archive_size) {
      HandleResourceDownloadFailure("Archive size mismatch for '" + resource.archive_path + "'");
      return;
    }

    if (!VerifyDigest(resource.archive_sha256, archive_bytes, archive_result->body.size())) {
      HandleResourceDownloadFailure("Archive digest mismatch for '" + resource.archive_path + "'");
      return;
    }

    ResourcePayload downloaded_resource;
    downloaded_resource.descriptor = resource;
    downloaded_resource.manifest_json = manifest_result->body;
    downloaded_resource.archive_bytes.assign(archive_bytes, archive_bytes + archive_result->body.size());
    downloaded.emplace_back(std::move(downloaded_resource));

    downloaded_bytes += resource.archive_size;
    const auto resource_name = resource.name;
    task_scheduler_.ScheduleOnMainThread([this, resource_name, downloaded_bytes, total_bytes]() {
      event_observer_.OnResourceDownloadProgress(resource_name, downloaded_bytes, total_bytes);
    });
  }

  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    downloaded_resources_ = std::move(downloaded);
  }

  SPDLOG_INFO("Successfully downloaded {} client resource pack(s); notifying runtime", resources.size());
  NotifyResourcesReady();
}

void ResourceDownloader::NotifyResourcesReady() {
  SPDLOG_INFO("Dispatching OnResourcesReady callback to main thread");
  resources_ready_.store(true, std::memory_order_release);
  task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnResourcesReady(); });
}

void ResourceDownloader::HandleResourceDownloadFailure(const std::string& reason) {
  if (resource_download_cancelled_.load(std::memory_order_acquire)) {
    return;
  }

  resource_download_failed_.store(true, std::memory_order_release);
  resource_download_error_ = reason;
  SPDLOG_ERROR("Resource download failed: {}", reason);

  task_scheduler_.ScheduleOnMainThread([this, reason]() { event_observer_.OnResourceDownloadFailed(reason); });
}

std::string ResourceDownloader::BuildDownloadPath(const std::string& relative_path) const {
  std::string sanitized = relative_path;
  std::replace(sanitized.begin(), sanitized.end(), '\\', '/');
  const auto first_non_slash = sanitized.find_first_not_of('/');
  if (first_non_slash != std::string::npos) {
    sanitized.erase(0, first_non_slash);
  } else {
    sanitized.clear();
  }

  std::string base = resource_base_path_;
  if (base.empty()) {
    base = "/public";
  }
  if (base.front() != '/') {
    base.insert(base.begin(), '/');
  }
  while (base.size() > 1 && base.back() == '/') {
    base.pop_back();
  }

  std::string path = base;
  if (!sanitized.empty()) {
    path.push_back('/');
    path += sanitized;
  }

  if (!resource_download_token_.empty()) {
    path.push_back(path.find('?') == std::string::npos ? '?' : '&');
    path += "token=";
    path += resource_download_token_;
  }

  return path;
}

bool ResourceDownloader::VerifyDigest(const std::string& expected_hex, const std::uint8_t* data, std::size_t size) const {
  if (expected_hex.empty() || data == nullptr) {
    return false;
  }

  const std::string computed = gmp::crypto::NormalizeHex(gmp::crypto::ComputeSHA256(data, size));
  const std::string expected = gmp::crypto::NormalizeHex(expected_hex);
  return computed == expected;
}

}  // namespace gmp::client

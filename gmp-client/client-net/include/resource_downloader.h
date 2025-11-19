#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include "packets.h"
#include "event_observer.hpp"
#include "task_scheduler.h"

namespace gmp::client {

class ResourceDownloader {
public:
  struct ResourcePayload {
    ClientResourceInfoEntry descriptor;
    std::string manifest_json;
    std::vector<std::uint8_t> archive_bytes;
  };

  ResourceDownloader(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler);
  ~ResourceDownloader();

  void SetServerEndpoint(const std::string& ip, std::uint32_t port);
  void SetDownloadToken(const std::string& token);
  void SetBasePath(const std::string& path);
  void AnnounceResources(std::vector<ClientResourceInfoEntry> resources);
  
  void BeginDownload();
  void StopDownload();
  void Reset();

  std::vector<ResourcePayload> ConsumeDownloadedResources();

private:
  void ResourceDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::uint64_t total_bytes);
  void NotifyResourcesReady();
  void HandleResourceDownloadFailure(const std::string& reason);
  std::string BuildDownloadPath(const std::string& relative_path) const;
  bool VerifyDigest(const std::string& expected_hex, const std::uint8_t* data, std::size_t size) const;

  EventObserver& event_observer_;
  gmp::TaskScheduler& task_scheduler_;

  std::string server_ip_;
  std::uint32_t server_port_{0};
  std::string resource_download_token_;
  std::string resource_base_path_{"/public"};

  std::thread resource_download_thread_;
  std::atomic<bool> resource_download_cancelled_{false};
  std::atomic<bool> resources_ready_{false};
  std::atomic<bool> resource_download_failed_{false};
  std::string resource_download_error_;
  
  mutable std::mutex resource_mutex_;
  std::vector<ClientResourceInfoEntry> announced_resources_;
  std::vector<ResourcePayload> downloaded_resources_;
};

}  // namespace gmp::client

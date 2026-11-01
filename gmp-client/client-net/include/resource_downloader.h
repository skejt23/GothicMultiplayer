#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <utility>

#include "packets.h"
#include "event_observer.hpp"
#include "task_scheduler.h"

namespace httplib {
class Client;
}

namespace gmp::client {

class ResourceDownloader {
public:
  struct ResourcePayload {
    ClientResourceInfoEntry descriptor;
    std::string manifest_json;
    std::vector<std::uint8_t> archive_bytes;
  };

  struct AddonPayload {
    AddonVdfInfoEntry descriptor;
    std::filesystem::path cached_path;
  };

  ResourceDownloader(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler);
  ~ResourceDownloader();

  void SetServerEndpoint(const std::string& ip, std::uint32_t port);
  void SetDownloadToken(const std::string& token);
  void SetBasePath(const std::string& path);
  void SetStoreGroup(const std::string& group);
  void AnnounceResources(std::vector<ClientResourceInfoEntry> resources);
  void AnnounceAddons(std::vector<AddonVdfInfoEntry> addons);
  void AnnounceAddonBundle(AddonBundleInfoEntry bundle);
  
  void BeginDownload();
  void StopDownload();
  void Reset();

  std::vector<ResourcePayload> ConsumeDownloadedResources();
  std::vector<AddonPayload> ConsumeDownloadedAddons();

private:
  void StartDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::vector<AddonVdfInfoEntry> addons,
                           AddonBundleInfoEntry addon_bundle, std::uint64_t total_bytes);
  void ResourceDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::vector<AddonVdfInfoEntry> addons,
                              AddonBundleInfoEntry addon_bundle, std::uint64_t total_bytes);
  bool DownloadAddonBundle(httplib::Client& client, const std::vector<AddonVdfInfoEntry>& addons,
                           const AddonBundleInfoEntry& bundle, const std::filesystem::path& output_directory,
                           std::uint64_t& downloaded_bytes, std::uint64_t total_bytes, std::string& error);
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
  std::string store_group_;

  std::thread resource_download_thread_;
  std::atomic<bool> resource_download_cancelled_{false};
  std::atomic<bool> resources_ready_{false};
  std::atomic<bool> resource_download_failed_{false};
  std::string resource_download_error_;
  
  mutable std::mutex resource_mutex_;
  std::vector<ClientResourceInfoEntry> announced_resources_;
  std::vector<AddonVdfInfoEntry> announced_addons_;
  AddonBundleInfoEntry announced_addon_bundle_;
  std::vector<ResourcePayload> downloaded_resources_;
  std::vector<AddonPayload> downloaded_addons_;
};

}  // namespace gmp::client

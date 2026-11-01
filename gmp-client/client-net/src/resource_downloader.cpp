#include "resource_downloader.h"

#include <addon/addon_transport.h>
#include <httplib.h>
#include <sodium.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <unordered_set>

#include "shared/crypto_utils.h"

namespace gmp::client {

namespace {

constexpr std::size_t kMaximumAddonCount = 32;
constexpr std::uint64_t kMaximumAddonSize = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaximumTotalDownloadSize = 4ULL * 1024ULL * 1024ULL * 1024ULL;

bool IsSha256Hex(std::string_view value) {
  return value.size() == crypto_hash_sha256_BYTES * 2 &&
         std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

std::string SanitizeServerStoreName(std::string_view name) {
  std::string safe;
  safe.reserve(std::min<std::size_t>(name.size(), 64));
  bool previous_separator = false;
  for (unsigned char ch : name) {
    if (safe.size() >= 64) {
      break;
    }
    if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.') {
      safe.push_back(static_cast<char>(ch));
      previous_separator = false;
    } else if (!previous_separator && !safe.empty()) {
      safe.push_back('_');
      previous_separator = true;
    }
  }
  while (!safe.empty() && (safe.back() == '_' || safe.back() == '.' || safe.back() == ' ')) {
    safe.pop_back();
  }
  if (safe.empty()) {
    safe = "unnamed-server";
  }

  const auto upper = [&]() {
    std::string value = safe;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
  }();
  const auto dot = upper.find('.');
  const auto device_name = upper.substr(0, dot);
  const bool reserved = device_name == "CON" || device_name == "PRN" || device_name == "AUX" || device_name == "NUL" ||
                        (device_name.size() == 4 && (device_name.starts_with("COM") || device_name.starts_with("LPT")) && device_name[3] >= '1' &&
                         device_name[3] <= '9');
  if (reserved) {
    safe.insert(safe.begin(), '_');
  }
  return safe;
}

std::string CaseInsensitiveFileKey(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

bool IsEmptyAddonBundle(const AddonBundleInfoEntry& bundle) {
  return bundle.archive_path.empty() && bundle.archive_sha256.empty() && bundle.archive_size == 0 &&
         bundle.manifest_path.empty() && bundle.manifest_sha256.empty() && bundle.manifest_size == 0;
}

std::vector<gmp::addon::AddonTransportEntry> MakeTransportEntries(const std::vector<AddonVdfInfoEntry>& addons) {
  std::vector<gmp::addon::AddonTransportEntry> entries;
  entries.reserve(addons.size());
  for (const auto& addon : addons) {
    entries.push_back(gmp::addon::AddonTransportEntry{
        addon.logical_name, addon.size, addon.sha256, addon.contains_gothic_dat});
  }
  return entries;
}

class ScopedDirectoryCleanup {
public:
  explicit ScopedDirectoryCleanup(std::filesystem::path path) : path_(std::move(path)) {}
  ~ScopedDirectoryCleanup() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

private:
  std::filesystem::path path_;
};

}  // namespace

ResourceDownloader::ResourceDownloader(EventObserver& eventObserver, gmp::TaskScheduler& taskScheduler)
    : event_observer_(eventObserver), task_scheduler_(taskScheduler) {
}

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

void ResourceDownloader::SetStoreGroup(const std::string& group) {
  const std::string requested = group.empty() ? server_ip_ + "_" + std::to_string(server_port_) : group;
  store_group_ = SanitizeServerStoreName(requested);
}

void ResourceDownloader::AnnounceAddons(std::vector<AddonVdfInfoEntry> addons) {
  std::lock_guard<std::mutex> lock(resource_mutex_);
  announced_addons_ = std::move(addons);
}

void ResourceDownloader::AnnounceAddonBundle(AddonBundleInfoEntry bundle) {
  std::lock_guard<std::mutex> lock(resource_mutex_);
  announced_addon_bundle_ = std::move(bundle);
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
  store_group_.clear();
  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    announced_resources_.clear();
    announced_addons_.clear();
    announced_addon_bundle_ = {};
    downloaded_resources_.clear();
    downloaded_addons_.clear();
  }
}

std::vector<ResourceDownloader::AddonPayload> ResourceDownloader::ConsumeDownloadedAddons() {
  std::lock_guard<std::mutex> lock(resource_mutex_);
  std::vector<AddonPayload> payloads = std::move(downloaded_addons_);
  downloaded_addons_.clear();
  return payloads;
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
  std::vector<AddonVdfInfoEntry> addons;
  AddonBundleInfoEntry addon_bundle;
  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    resources = announced_resources_;
    addons = announced_addons_;
    addon_bundle = announced_addon_bundle_;
  }

  if (addons.size() > kMaximumAddonCount) {
    HandleResourceDownloadFailure("Server announced too many addon VDFs");
    return;
  }

  if ((!resources.empty() || !addons.empty()) && resource_download_token_.empty()) {
    HandleResourceDownloadFailure("Server did not provide a resource download token");
    return;
  }

  if (server_ip_.empty() || server_port_ == 0) {
    HandleResourceDownloadFailure("Missing server endpoint information for resource download");
    return;
  }

  std::uint64_t total_bytes = 0;
  std::size_t gothic_dat_count = 0;
  std::unordered_set<std::string> addon_filenames;
  for (const auto& entry : resources) {
    if (entry.archive_size == 0 || entry.archive_size > std::numeric_limits<std::size_t>::max() ||
        !IsSha256Hex(entry.manifest_sha256) || !IsSha256Hex(entry.archive_sha256)) {
      HandleResourceDownloadFailure("Server announced an invalid client resource descriptor");
      return;
    }
    if (entry.manifest_size == 0) {
      HandleResourceDownloadFailure("Server announced an invalid client resource descriptor");
      return;
    }
    if (entry.archive_size > std::numeric_limits<std::uint64_t>::max() - total_bytes ||
        entry.manifest_size > std::numeric_limits<std::uint64_t>::max() - total_bytes - entry.archive_size) {
      HandleResourceDownloadFailure("Server resource download size overflow");
      return;
    }
    total_bytes += entry.archive_size + entry.manifest_size;
  }
  for (const auto& addon : addons) {
    std::string filename_error;
    if (addon.logical_name.empty() || addon.size == 0 || addon.size > kMaximumAddonSize ||
        !IsSha256Hex(addon.sha256) ||
        !gmp::addon::IsPortableAddonFilename(addon.logical_name, filename_error)) {
      HandleResourceDownloadFailure("Server announced an invalid addon VDF descriptor");
      return;
    }
    if (!addon_filenames.insert(CaseInsensitiveFileKey(addon.logical_name)).second) {
      HandleResourceDownloadFailure("Server announced duplicate addon VDF filename '" + addon.logical_name + "'");
      return;
    }
    gothic_dat_count += addon.contains_gothic_dat ? 1U : 0U;
  }
  if (gothic_dat_count > 1) {
    HandleResourceDownloadFailure("Server announced more than one addon GOTHIC.DAT");
    return;
  }

  if (addons.empty()) {
    if (!IsEmptyAddonBundle(addon_bundle)) {
      HandleResourceDownloadFailure("Server announced an addon bundle without addon VDF entries");
      return;
    }
  } else {
    if (addon_bundle.archive_path.empty() || addon_bundle.manifest_path.empty() || addon_bundle.archive_size == 0 ||
        addon_bundle.manifest_size == 0 || !IsSha256Hex(addon_bundle.archive_sha256) ||
        !IsSha256Hex(addon_bundle.manifest_sha256) ||
        addon_bundle.archive_size > std::numeric_limits<std::uint64_t>::max() - total_bytes ||
        addon_bundle.manifest_size > std::numeric_limits<std::uint64_t>::max() - total_bytes - addon_bundle.archive_size) {
      HandleResourceDownloadFailure("Server announced an invalid addon bundle descriptor");
      return;
    }
    total_bytes += addon_bundle.archive_size + addon_bundle.manifest_size;
    try {
      const auto expected_manifest =
          gmp::addon::BuildAddonBundleManifest(MakeTransportEntries(addons), addon_bundle.archive_size,
                                               addon_bundle.archive_sha256);
      if (expected_manifest.size() != addon_bundle.manifest_size ||
          !VerifyDigest(addon_bundle.manifest_sha256, expected_manifest.data(), expected_manifest.size())) {
        HandleResourceDownloadFailure("Server addon bundle manifest identity is inconsistent");
        return;
      }
    } catch (const std::exception& ex) {
      HandleResourceDownloadFailure(std::string("Invalid addon bundle manifest: ") + ex.what());
      return;
    }
  }

  if (total_bytes > kMaximumTotalDownloadSize) {
    HandleResourceDownloadFailure("Server announced content exceeding the hardcoded 4 GiB download limit");
    return;
  }

  if (resources.empty() && addons.empty()) {
    SPDLOG_INFO("No client resources or addon VDFs announced; synchronizing an empty addon Store group");
    StartDownloadWorker({}, {}, {}, 0);
    return;
  }

  SPDLOG_INFO("Starting resource download of {} content package(s) totaling {} bytes", resources.size() + (addons.empty() ? 0U : 1U), total_bytes);
  StartDownloadWorker(std::move(resources), std::move(addons), std::move(addon_bundle), total_bytes);
}

void ResourceDownloader::StartDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::vector<AddonVdfInfoEntry> addons,
                                             AddonBundleInfoEntry addon_bundle, std::uint64_t total_bytes) {
  resource_download_cancelled_.store(false, std::memory_order_release);
  resource_download_thread_ = std::thread([this, total_bytes, resources = std::move(resources), addons = std::move(addons),
                                            addon_bundle = std::move(addon_bundle)]() mutable {
    try {
      ResourceDownloadWorker(std::move(resources), std::move(addons), std::move(addon_bundle), total_bytes);
    } catch (const std::exception& ex) {
      HandleResourceDownloadFailure(ex.what());
    }
  });
}

void ResourceDownloader::ResourceDownloadWorker(std::vector<ClientResourceInfoEntry> resources, std::vector<AddonVdfInfoEntry> addons,
                                                AddonBundleInfoEntry addon_bundle, std::uint64_t total_bytes) {
  SPDLOG_INFO("Starting download of {} client resource pack(s) and one bundle containing {} addon VDF(s) ({} bytes)",
              resources.size(), addons.size(), total_bytes);

  httplib::Client client(server_ip_.c_str(), static_cast<int>(server_port_));
  client.set_read_timeout(300, 0);
  client.set_write_timeout(30, 0);
  client.set_follow_location(true);

  std::vector<ResourcePayload> downloaded;
  downloaded.reserve(resources.size());
  std::vector<AddonPayload> downloaded_addons;
  downloaded_addons.reserve(addons.size());

  std::uint64_t downloaded_bytes = 0;
  task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnResourceDownloadPhase("Downloading resource packs..."); });
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
      HandleResourceDownloadFailure("HTTP " + std::to_string(manifest_result->status) + " while downloading manifest '" + resource.manifest_path +
                                    "'");
      return;
    }

    const auto* manifest_bytes = reinterpret_cast<const std::uint8_t*>(manifest_result->body.data());
    if (!VerifyDigest(resource.manifest_sha256, manifest_bytes, manifest_result->body.size())) {
      HandleResourceDownloadFailure("Manifest digest mismatch for '" + resource.manifest_path + "'");
      return;
    }
    if (manifest_result->body.size() != resource.manifest_size) {
      HandleResourceDownloadFailure("Manifest size mismatch for '" + resource.manifest_path + "'");
      return;
    }
    downloaded_bytes += resource.manifest_size;
    task_scheduler_.ScheduleOnMainThread([this, resource_name = resource.name, downloaded_bytes, total_bytes]() {
      event_observer_.OnResourceDownloadProgress(resource_name, downloaded_bytes, total_bytes);
    });

    if (resource_download_cancelled_.load(std::memory_order_acquire)) {
      SPDLOG_INFO("Resource download cancelled before completion");
      return;
    }

    SPDLOG_INFO("Fetching archive '{}'", resource.archive_path);
    const auto archive_path = BuildDownloadPath(resource.archive_path);
    std::vector<std::uint8_t> archive_payload;
    archive_payload.reserve(static_cast<std::size_t>(resource.archive_size));
    std::uint64_t archive_received = 0;
    auto last_progress_update = std::chrono::steady_clock::now();
    const auto archive_result = client.Get(archive_path.c_str(), [&](const char* data, std::size_t length) {
      if (resource_download_cancelled_.load(std::memory_order_acquire) ||
          length > resource.archive_size - std::min(archive_received, resource.archive_size)) {
        return false;
      }
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
      archive_payload.insert(archive_payload.end(), bytes, bytes + length);
      archive_received += length;
      const auto now = std::chrono::steady_clock::now();
      constexpr auto kProgressTimeInterval = std::chrono::milliseconds(100);
      if (archive_received == resource.archive_size || now - last_progress_update >= kProgressTimeInterval) {
        last_progress_update = now;
        task_scheduler_.ScheduleOnMainThread([this, resource_name = resource.name, current = downloaded_bytes + archive_received, total_bytes]() {
          event_observer_.OnResourceDownloadProgress(resource_name, current, total_bytes);
        });
      }
      return true;
    });
    if (resource_download_cancelled_.load(std::memory_order_acquire)) {
      SPDLOG_INFO("Resource download cancelled before completion");
      return;
    }
    if (!archive_result) {
      HandleResourceDownloadFailure("Failed to download archive '" + resource.archive_path + "'");
      return;
    }
    if (archive_result->status != 200) {
      HandleResourceDownloadFailure("HTTP " + std::to_string(archive_result->status) + " while downloading archive '" + resource.archive_path + "'");
      return;
    }

    if (archive_received != resource.archive_size) {
      HandleResourceDownloadFailure("Archive size mismatch for '" + resource.archive_path + "'");
      return;
    }

    if (!VerifyDigest(resource.archive_sha256, archive_payload.data(), archive_payload.size())) {
      HandleResourceDownloadFailure("Archive digest mismatch for '" + resource.archive_path + "'");
      return;
    }

    ResourcePayload downloaded_resource;
    downloaded_resource.descriptor = resource;
    downloaded_resource.manifest_json = manifest_result->body;
    downloaded_resource.archive_bytes = std::move(archive_payload);
    downloaded.emplace_back(std::move(downloaded_resource));

    downloaded_bytes += resource.archive_size;
    const auto resource_name = resource.name;
    task_scheduler_.ScheduleOnMainThread([this, resource_name, downloaded_bytes, total_bytes]() {
      event_observer_.OnResourceDownloadProgress(resource_name, downloaded_bytes, total_bytes);
    });
  }

  namespace fs = std::filesystem;
  if (!addons.empty()) {
    task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnResourceDownloadPhase("Checking local addon cache..."); });
  }
  const std::string store_group =
      store_group_.empty() ? SanitizeServerStoreName(server_ip_ + "_" + std::to_string(server_port_)) : store_group_;
  const auto multiplayer_root = fs::current_path() / "Multiplayer";
  const auto store_root = multiplayer_root / "Store";
  const auto cache_root = store_root / store_group;
  std::error_code filesystem_error;
  fs::create_directories(store_root, filesystem_error);
  if (filesystem_error) {
    HandleResourceDownloadFailure("Failed to create addon Store directory: " + filesystem_error.message());
    return;
  }

  std::unordered_set<std::string> expected_filenames;
  for (const auto& addon : addons) {
    expected_filenames.insert(addon.logical_name);
  }

  bool exact_mirror = false;
  std::string mirror_rejection;
  if (!fs::exists(cache_root, filesystem_error) && !filesystem_error) {
    exact_mirror = addons.empty();
    if (!exact_mirror) {
      mirror_rejection = "Store group does not exist";
    }
  } else if (!filesystem_error && fs::is_directory(cache_root, filesystem_error) && !filesystem_error) {
    exact_mirror = true;
    std::size_t entry_count = 0;
    for (fs::directory_iterator it(cache_root, filesystem_error), end; !filesystem_error && it != end; it.increment(filesystem_error)) {
      ++entry_count;
      if (!it->is_regular_file(filesystem_error) || filesystem_error ||
          !expected_filenames.contains(it->path().filename().string())) {
        exact_mirror = false;
        mirror_rejection = "Store group contains an unexpected entry";
        break;
      }
    }
    if (filesystem_error) {
      HandleResourceDownloadFailure("Failed to inspect addon Store group: " + filesystem_error.message());
      return;
    }
    if (exact_mirror && entry_count != addons.size()) {
      exact_mirror = false;
      mirror_rejection = "Store group file count differs from the server addon list";
    }
    if (exact_mirror) {
      for (const auto& addon : addons) {
        const auto path = cache_root / addon.logical_name;
        if (fs::file_size(path, filesystem_error) != addon.size || filesystem_error) {
          exact_mirror = false;
          mirror_rejection = addon.logical_name + ": size differs from the server manifest";
          filesystem_error.clear();
          break;
        }
        try {
          if (gmp::crypto::NormalizeHex(gmp::crypto::ComputeFileSHA256(path)) != gmp::crypto::NormalizeHex(addon.sha256)) {
            exact_mirror = false;
            mirror_rejection = addon.logical_name + ": SHA-256 differs from the server manifest";
            break;
          }
        } catch (const std::exception& ex) {
          exact_mirror = false;
          mirror_rejection = addon.logical_name + ": cannot verify SHA-256 (" + ex.what() + ")";
          break;
        }
      }
    }
  } else if (filesystem_error) {
    HandleResourceDownloadFailure("Failed to inspect addon Store group: " + filesystem_error.message());
    return;
  } else {
    mirror_rejection = "Store group path is not a directory";
  }

  if (exact_mirror) {
    SPDLOG_INFO("Addon Store group '{}' exactly matches the server cache", store_group);
    for (const auto& addon : addons) {
      const auto cache_path = cache_root / addon.logical_name;
      downloaded_addons.push_back(AddonPayload{addon, cache_path});
    }
    if (!addons.empty()) {
      downloaded_bytes += addon_bundle.archive_size + addon_bundle.manifest_size;
      task_scheduler_.ScheduleOnMainThread([this, downloaded_bytes, total_bytes]() {
        event_observer_.OnResourceDownloadProgress("resource packs", downloaded_bytes, total_bytes);
      });
    }
  } else {
    SPDLOG_INFO("Rebuilding addon Store group '{}': {}", store_group, mirror_rejection);
    const auto transaction_root = multiplayer_root / "StoreTransactions";
    const auto pending_root = transaction_root / (store_group + ".pending");
    const auto previous_root = transaction_root / (store_group + ".previous");
    fs::remove_all(pending_root, filesystem_error);
    filesystem_error.clear();
    fs::remove_all(previous_root, filesystem_error);
    filesystem_error.clear();
    fs::create_directories(pending_root, filesystem_error);
    if (filesystem_error) {
      HandleResourceDownloadFailure("Failed to create addon Store transaction: " + filesystem_error.message());
      return;
    }
    ScopedDirectoryCleanup pending_cleanup(pending_root);

    if (!addons.empty()) {
      std::string download_error;
      if (!DownloadAddonBundle(client, addons, addon_bundle, pending_root, downloaded_bytes, total_bytes, download_error)) {
        if (!resource_download_cancelled_.load(std::memory_order_acquire)) {
          HandleResourceDownloadFailure(download_error.empty() ? "Addon bundle download failed" : download_error);
        }
        return;
      }
    }

    const bool had_previous = fs::exists(cache_root, filesystem_error) && !filesystem_error;
    if (filesystem_error) {
      HandleResourceDownloadFailure("Failed to inspect existing addon Store group before commit: " + filesystem_error.message());
      return;
    }
    if (had_previous) {
      fs::rename(cache_root, previous_root, filesystem_error);
      if (filesystem_error) {
        HandleResourceDownloadFailure("Failed to move the previous addon Store group aside: " + filesystem_error.message());
        return;
      }
    }
    fs::rename(pending_root, cache_root, filesystem_error);
    if (filesystem_error) {
      const std::string commit_error = filesystem_error.message();
      if (had_previous) {
        std::error_code restore_error;
        fs::rename(previous_root, cache_root, restore_error);
      }
      HandleResourceDownloadFailure("Failed to commit addon Store group: " + commit_error);
      return;
    }
    fs::remove_all(previous_root, filesystem_error);
    if (filesystem_error) {
      SPDLOG_WARN("Failed to remove previous addon Store transaction '{}': {}", previous_root.string(), filesystem_error.message());
    }
    filesystem_error.clear();
    fs::remove(transaction_root, filesystem_error);

    for (const auto& addon : addons) {
      downloaded_addons.push_back(AddonPayload{addon, cache_root / addon.logical_name});
    }
    SPDLOG_INFO("Addon Store group '{}' now exactly mirrors {} server VDF(s)", store_group, addons.size());
  }

  {
    std::lock_guard<std::mutex> lock(resource_mutex_);
    downloaded_resources_ = std::move(downloaded);
    downloaded_addons_ = std::move(downloaded_addons);
  }

  task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnResourceDownloadPhase("Validating downloaded content..."); });
  SPDLOG_INFO("All client content is verified and ready; notifying runtime");
  NotifyResourcesReady();
}

bool ResourceDownloader::DownloadAddonBundle(httplib::Client& client, const std::vector<AddonVdfInfoEntry>& addons,
                                             const AddonBundleInfoEntry& bundle,
                                             const std::filesystem::path& output_directory,
                                             std::uint64_t& downloaded_bytes, std::uint64_t total_bytes,
                                             std::string& error) {
  namespace fs = std::filesystem;
  const auto expected_manifest =
      gmp::addon::BuildAddonBundleManifest(MakeTransportEntries(addons), bundle.archive_size, bundle.archive_sha256);

  const auto manifest_result = client.Get(BuildDownloadPath(bundle.manifest_path).c_str());
  if (!manifest_result) {
    error = "Failed to download addons.bin";
    return false;
  }
  if (manifest_result->status != 200) {
    error = "HTTP " + std::to_string(manifest_result->status) + " while downloading addons.bin";
    return false;
  }
  const auto* manifest_bytes = reinterpret_cast<const std::uint8_t*>(manifest_result->body.data());
  if (manifest_result->body.size() != bundle.manifest_size ||
      !VerifyDigest(bundle.manifest_sha256, manifest_bytes, manifest_result->body.size()) ||
      manifest_result->body.size() != expected_manifest.size() ||
      !std::equal(expected_manifest.begin(), expected_manifest.end(), manifest_bytes)) {
    error = "addons.bin differs from the announced bundle manifest";
    return false;
  }
  downloaded_bytes += bundle.manifest_size;
  task_scheduler_.ScheduleOnMainThread([this, downloaded_bytes, total_bytes]() {
    event_observer_.OnResourceDownloadProgress("resource packs", downloaded_bytes, total_bytes);
  });

  fs::path transport_path = output_directory;
  transport_path += ".zip.part";
  std::error_code ec;
  fs::remove(transport_path, ec);

  std::ofstream output(transport_path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    error = "Failed to create temporary addons.zip file '" + transport_path.string() + "'";
    return false;
  }

  gmp::crypto::EnsureSodiumInitialized();
  crypto_hash_sha256_state hash_state;
  crypto_hash_sha256_init(&hash_state);
  std::uint64_t received = 0;
  auto last_progress_update = std::chrono::steady_clock::now();
  const auto remove_temporary = [&]() {
    output.close();
    std::error_code ignored;
    fs::remove(transport_path, ignored);
  };

  SPDLOG_INFO("Streaming addon resource bundle containing {} VDF(s)", addons.size());
  const auto request_path = BuildDownloadPath(bundle.archive_path);
  const auto result = client.Get(request_path.c_str(), [&](const char* data, std::size_t length) {
    if (resource_download_cancelled_.load(std::memory_order_acquire) ||
        length > bundle.archive_size - std::min(received, bundle.archive_size)) {
      return false;
    }
    output.write(data, static_cast<std::streamsize>(length));
    if (!output) {
      return false;
    }
    crypto_hash_sha256_update(&hash_state, reinterpret_cast<const unsigned char*>(data), length);
    received += length;
    const auto now = std::chrono::steady_clock::now();
    constexpr auto kProgressTimeInterval = std::chrono::milliseconds(100);
    if (received == bundle.archive_size || now - last_progress_update >= kProgressTimeInterval) {
      last_progress_update = now;
      task_scheduler_.ScheduleOnMainThread([this, current = downloaded_bytes + received, total_bytes]() {
        event_observer_.OnResourceDownloadProgress("resource packs", current, total_bytes);
      });
    }
    return true;
  });

  output.close();
  if (resource_download_cancelled_.load(std::memory_order_acquire)) {
    remove_temporary();
    return false;
  }
  if (!result) {
    remove_temporary();
    error = "Failed to download addons.zip";
    return false;
  }
  if (result->status != 200) {
    remove_temporary();
    error = "HTTP " + std::to_string(result->status) + " while downloading addons.zip";
    return false;
  }
  if (received != bundle.archive_size) {
    remove_temporary();
    error = "addons.zip size mismatch";
    return false;
  }

  std::array<unsigned char, crypto_hash_sha256_BYTES> hash{};
  crypto_hash_sha256_final(&hash_state, hash.data());
  if (gmp::crypto::BytesToHex(hash.data(), hash.size()) != gmp::crypto::NormalizeHex(bundle.archive_sha256)) {
    remove_temporary();
    error = "addons.zip SHA-256 mismatch";
    return false;
  }
  task_scheduler_.ScheduleOnMainThread([this]() { event_observer_.OnResourceDownloadPhase("Extracting addon VDFs..."); });
  if (!gmp::addon::ExtractAddonTransportBundle(transport_path, output_directory, MakeTransportEntries(addons), error)) {
    remove_temporary();
    error = "Invalid addons.zip: " + error;
    return false;
  }
  fs::remove(transport_path, ec);
  if (ec) {
    remove_temporary();
    error = "Failed to remove temporary addons.zip: " + ec.message();
    return false;
  }
  downloaded_bytes += received;
  SPDLOG_INFO("Extracted {} addon VDF(s) from addons.zip", addons.size());
  error.clear();
  return true;
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

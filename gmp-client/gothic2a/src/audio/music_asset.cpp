#include "music_asset.h"

#include <addon/addon_vfs.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "ZenGin/zGothicAPI.h"
#include "content/gothic_vfs_overlay.h"

namespace gmp::audio {
namespace {
// Decoders currently materialize compressed audio and the decoded PCM in
// memory. Bound input sizes before allocating or narrowing to Vorbis's int.
constexpr std::size_t kMaximumMusicBytes = 256ULL * 1024ULL * 1024ULL;

std::optional<std::vector<std::uint8_t>> ReadLooseMusic(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    if (error) throw std::runtime_error("Cannot inspect music file: " + path.string());
    return std::nullopt;
  }
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) throw std::runtime_error("Cannot open music file: " + path.string());
  const auto length = input.tellg();
  if (length <= 0 || length > static_cast<std::streamoff>(kMaximumMusicBytes))
    throw std::runtime_error("Invalid music file size: " + path.string());
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
  input.seekg(0, std::ios::beg);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
    throw std::runtime_error("Incomplete music file read: " + path.string());
  return bytes;
}
}  // namespace

std::vector<std::uint8_t> ReadMusicAsset(const std::string& filepath) {
  const auto paths = MusicAssetPaths(filepath);
  if (!paths) {
    throw std::runtime_error("Invalid relative music path: " + filepath);
  }
  auto& overlay = gmp::gothic::GothicVfsOverlay::Instance();
  // Active server assets override local copies in either tree.
  // Gothic archives may store paths relative to _Work/Data, so MUSIC/... is
  // the archive representation of the second root, not a third loose root.
  const std::array<std::string, 3> archive_paths{
      (*paths)[0], (*paths)[1], (*paths)[1].substr(std::string_view("_Work/Data/").size())};
  for (const auto& path : archive_paths) {
    if (auto file = overlay.OpenExact(path)) {
      const auto length = file->Size();
      if (!length || length > kMaximumMusicBytes) throw std::runtime_error("Invalid addon music size: " + path);
      std::vector<std::uint8_t> bytes(length);
      if (file->Read(bytes.data(), bytes.size()) != bytes.size()) throw std::runtime_error("Incomplete addon music read: " + path);
      return bytes;
    }
  }
  using namespace Gothic_II_Addon;
  std::filesystem::path game_root;
  if (zoptions) {
    const auto* root = zoptions->GetDirString(DIR_ROOT).ToChar();
    if (root && *root) game_root = std::filesystem::path(root);
  }
  if (game_root.empty()) game_root = std::filesystem::current_path();
  for (const auto& path : *paths) {
    if (auto bytes = ReadLooseMusic(game_root / path)) return std::move(*bytes);
  }
  if (zFILE_VDFS::s_vdfsInitialized) {
    for (const auto& path : *paths) {
      if (auto bytes = overlay.ReadNativeFile((game_root / path).string(), kMaximumMusicBytes)) return std::move(*bytes);
    }
  }
  throw std::runtime_error("Music asset not found under Multiplayer/Music or _Work/Data/Music: " + filepath);
}
}  // namespace gmp::audio

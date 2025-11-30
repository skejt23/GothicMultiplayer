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

#include "gothic_music_bridge.h"

#include <spdlog/spdlog.h>

#include <algorithm>

// Gothic API access
#include "ZenGin/zGothicAPI.h"

// For CreateHook
#include "hooking/MemoryPatch.h"

namespace gmp::audio {

// Address of Gothic's zOpt_Sound_ChangeMusicVol function.
constexpr DWORD kChangeMusicVolAddress = 0x005093E0;

// Original function type: int __cdecl zOpt_Sound_ChangeMusicVol(zCOptionEntry*)
using ChangeMusicVolOriginalFn = int(__cdecl*)(zCOptionEntry*);
static ChangeMusicVolOriginalFn g_original_change_music_vol = nullptr;

// Static member initialization.
float GothicMusicBridge::backup_volume_ = 1.0f;
bool GothicMusicBridge::is_muted_ = false;
float GothicMusicBridge::options_volume_ = 1.0f;
bool GothicMusicBridge::hook_installed_ = false;

// Internal storage for volume change callbacks.
static std::vector<VolumeChangeCallback> g_volume_callbacks;

// Our hook function that intercepts zOpt_Sound_ChangeMusicVol.
static int __cdecl HookedChangeMusicVol(zCOptionEntry* entry) {
  // Call the original Gothic function first.
  int result = 0;
  if (g_original_change_music_vol) {
    result = g_original_change_music_vol(entry);
  }

  // Now handle our own volume tracking.
  if (entry) {
    float new_volume = entry->varValue.ToFloat();

    // Clamp to valid range.
    new_volume = std::max(0.0f, std::min(1.0f, new_volume));

    SPDLOG_DEBUG("zOpt_Sound_ChangeMusicVol hooked: volume changed to {}", new_volume);

    // Update cached value via the setter.
    GothicMusicBridge::SetOptionsVolume(new_volume);

    // Notify all registered callbacks.
    for (auto callback : g_volume_callbacks) {
      if (callback) {
        callback(new_volume);
      }
    }
  }

  return result;
}

void GothicMusicBridge::Initialize() {
  // Read initial volume from zCOptions.
  if (zoptions) {
    options_volume_ = zoptions->ReadReal("SOUND", "musicVolume", 1.0f);
    SPDLOG_DEBUG("Initial music volume from zCOptions: {}", options_volume_);
  } else {
    SPDLOG_WARN("zoptions not available, using default volume 1.0");
    options_volume_ = 1.0f;
  }

  // Install hook on zOpt_Sound_ChangeMusicVol.
  if (!hook_installed_) {
    if (auto original = CreateHook(kChangeMusicVolAddress, (DWORD)HookedChangeMusicVol)) {
      g_original_change_music_vol = reinterpret_cast<ChangeMusicVolOriginalFn>(*original);
      hook_installed_ = true;
      SPDLOG_DEBUG("Installed hook on zOpt_Sound_ChangeMusicVol at 0x{:08X}", kChangeMusicVolAddress);
    } else {
      SPDLOG_ERROR("Failed to install hook on zOpt_Sound_ChangeMusicVol");
    }
  }

  SPDLOG_DEBUG("GothicMusicBridge initialized");
}

void GothicMusicBridge::Shutdown() {
  // Restore music if we had it muted
  if (is_muted_) {
    UnmuteGothicMusic();
  }
  SPDLOG_DEBUG("GothicMusicBridge shutdown");
}

void GothicMusicBridge::MuteGothicMusic() {
  if (is_muted_) {
    return;  // Already muted.
  }

  if (zmusic) {
    backup_volume_ = zmusic->GetVolume();
    zmusic->SetVolume(0.0f);
    is_muted_ = true;
    SPDLOG_DEBUG("Gothic music muted (backup volume: {})", backup_volume_);
  }
}

void GothicMusicBridge::UnmuteGothicMusic() {
  if (!is_muted_) {
    return;  // Not muted.
  }

  if (zmusic) {
    zmusic->SetVolume(backup_volume_);
    is_muted_ = false;
    SPDLOG_DEBUG("Gothic music restored (volume: {})", backup_volume_);
  }
}

float GothicMusicBridge::GetGothicMusicVolume() {
  if (zmusic) {
    return zmusic->GetVolume();
  }
  return 0.0f;
}

void GothicMusicBridge::SetGothicMusicVolume(float volume) {
  if (zmusic) {
    zmusic->SetVolume(volume);
    if (!is_muted_) {
      backup_volume_ = volume;  // Update backup if not muted.
    }
  }
}

bool GothicMusicBridge::IsGothicMusicMuted() {
  return is_muted_;
}

void GothicMusicBridge::StopGothicTheme() {
  if (zmusic) {
    zmusic->Stop();
    SPDLOG_DEBUG("Gothic music theme stopped");
  }
}

float GothicMusicBridge::GetOptionsVolume() {
  // Try to refresh from zCOptions if available.
  if (zoptions) {
    options_volume_ = zoptions->ReadReal("SOUND", "musicVolume", options_volume_);
  }
  return options_volume_;
}

void GothicMusicBridge::SetOptionsVolume(float volume) {
  options_volume_ = std::max(0.0f, std::min(1.0f, volume));
}

float GothicMusicBridge::RegisterVolumeChangeCallback(VolumeChangeCallback callback) {
  if (callback) {
    // Avoid duplicates.
    auto it = std::find(g_volume_callbacks.begin(), g_volume_callbacks.end(), callback);
    if (it == g_volume_callbacks.end()) {
      g_volume_callbacks.push_back(callback);
      SPDLOG_DEBUG("Registered volume change callback");
    }
  }
  return options_volume_;
}

void GothicMusicBridge::UnregisterVolumeChangeCallback(VolumeChangeCallback callback) {
  auto it = std::find(g_volume_callbacks.begin(), g_volume_callbacks.end(), callback);
  if (it != g_volume_callbacks.end()) {
    g_volume_callbacks.erase(it);
    SPDLOG_DEBUG("Unregistered volume change callback");
  }
}

}  // namespace gmp::audio

// Re-implement the gothic namespace functions used by MusicPlayer.
namespace gmp::audio::gothic {

void MuteGothicMusicSystem() {
  GothicMusicBridge::MuteGothicMusic();
}

void UnmuteGothicMusicSystem() {
  GothicMusicBridge::UnmuteGothicMusic();
}

float GetGothicMusicVolume() {
  return GothicMusicBridge::GetGothicMusicVolume();
}

void SetGothicMusicVolume(float volume) {
  GothicMusicBridge::SetGothicMusicVolume(volume);
}

float GetOptionsVolume() {
  return GothicMusicBridge::GetOptionsVolume();
}

float RegisterVolumeChangeCallback(VolumeChangeCallback callback) {
  return GothicMusicBridge::RegisterVolumeChangeCallback(callback);
}

void UnregisterVolumeChangeCallback(VolumeChangeCallback callback) {
  GothicMusicBridge::UnregisterVolumeChangeCallback(callback);
}

}  // namespace gmp::audio::gothic

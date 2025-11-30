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

#include <vector>

namespace gmp::audio {

/// Callback signature for volume change notifications.
using VolumeChangeCallback = void (*)(float new_volume);

/**
 * @brief Manages integration between GMP's music player and Gothic's native music system.
 *
 * When GMP music is playing, Gothic's DirectMusic-based music system should be
 * silenced to avoid overlapping audio. This class provides hooks and utilities
 * for controlling Gothic's music.
 *
 * Also manages integration with zCOptions to respect user's music volume settings.
 */
class GothicMusicBridge {
public:
  /// Initialize hooks into Gothic's music system and register zCOptions handlers.
  static void Initialize();

  /// Remove hooks (call during shutdown).
  static void Shutdown();

  /// Mute Gothic's music system.
  static void MuteGothicMusic();

  /// Unmute Gothic's music system and restore previous volume.
  static void UnmuteGothicMusic();

  /// Get Gothic's current music volume from zmusic.
  static float GetGothicMusicVolume();

  /// Set Gothic's music volume directly.
  static void SetGothicMusicVolume(float volume);

  /// Check if Gothic's music is currently muted by us.
  static bool IsGothicMusicMuted();

  /// Stop Gothic's currently playing theme.
  static void StopGothicTheme();

  /// Get the user's music volume setting from zCOptions (0.0 to 1.0).
  static float GetOptionsVolume();

  /// Register a callback to be notified when the user changes music volume in options.
  /// Returns the current volume immediately.
  static float RegisterVolumeChangeCallback(VolumeChangeCallback callback);

  /// Unregister a previously registered volume change callback.
  static void UnregisterVolumeChangeCallback(VolumeChangeCallback callback);

  /// Update the cached options volume (called by zCOptions change handler).
  static void SetOptionsVolume(float volume);

private:
  static float backup_volume_;
  static bool is_muted_;
  static float options_volume_;
  static bool hook_installed_;
};

}  // namespace gmp::audio
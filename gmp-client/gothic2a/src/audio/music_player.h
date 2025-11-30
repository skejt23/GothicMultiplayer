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

#ifndef GMP_CLIENT_GOTHIC2A_LIB_AUDIO_MUSIC_PLAYER_H_
#define GMP_CLIENT_GOTHIC2A_LIB_AUDIO_MUSIC_PLAYER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

namespace gmp::audio {

/// Supported audio formats for the music player.
enum class AudioFormat {
  kUnknown,
  kWav,
  kMp3,
  kOgg,
  kFlac
};

/// Decodes audio from various formats to raw PCM.
class AudioDecoder {
 public:
  virtual ~AudioDecoder() = default;

  /// Load audio file and decode to PCM.
  virtual bool Load(const std::string& filepath) = 0;

  /// Load audio from memory buffer.
  virtual bool LoadFromMemory(const uint8_t* data, size_t size) = 0;

  /// Get decoded PCM data.
  virtual const std::vector<uint8_t>& GetPCMData() const = 0;

  /// Get sample rate (e.g., 44100).
  virtual int GetSampleRate() const = 0;

  /// Get number of channels (1 = mono, 2 = stereo).
  virtual int GetChannels() const = 0;

  /// Get bits per sample (typically 16).
  virtual int GetBitsPerSample() const = 0;

  /// Get duration in seconds.
  virtual float GetDuration() const = 0;

  /// Factory method to create decoder for a given format.
  static std::unique_ptr<AudioDecoder> CreateForFormat(AudioFormat format);

  /// Detect format from file extension.
  static AudioFormat DetectFormat(const std::string& filepath);
};

/// WAV decoder implementation.
class WavDecoder : public AudioDecoder {
 public:
  bool Load(const std::string& filepath) override;
  bool LoadFromMemory(const uint8_t* data, size_t size) override;
  const std::vector<uint8_t>& GetPCMData() const override { return pcm_data_; }
  int GetSampleRate() const override { return sample_rate_; }
  int GetChannels() const override { return channels_; }
  int GetBitsPerSample() const override { return bits_per_sample_; }
  float GetDuration() const override;

 private:
  std::vector<uint8_t> pcm_data_;
  int sample_rate_ = 0;
  int channels_ = 0;
  int bits_per_sample_ = 0;
};

/// MP3 decoder implementation (using minimp3).
class Mp3Decoder : public AudioDecoder {
 public:
  bool Load(const std::string& filepath) override;
  bool LoadFromMemory(const uint8_t* data, size_t size) override;
  const std::vector<uint8_t>& GetPCMData() const override { return pcm_data_; }
  int GetSampleRate() const override { return sample_rate_; }
  int GetChannels() const override { return channels_; }
  int GetBitsPerSample() const override { return 16; }  // minimp3 outputs 16-bit.
  float GetDuration() const override;

 private:
  std::vector<uint8_t> pcm_data_;
  int sample_rate_ = 0;
  int channels_ = 0;
};

/// OGG Vorbis decoder implementation (using stb_vorbis).
class OggDecoder : public AudioDecoder {
 public:
  bool Load(const std::string& filepath) override;
  bool LoadFromMemory(const uint8_t* data, size_t size) override;
  const std::vector<uint8_t>& GetPCMData() const override { return pcm_data_; }
  int GetSampleRate() const override { return sample_rate_; }
  int GetChannels() const override { return channels_; }
  int GetBitsPerSample() const override { return 16; }  // stb_vorbis outputs 16-bit.
  float GetDuration() const override;

 private:
  std::vector<uint8_t> pcm_data_;
  int sample_rate_ = 0;
  int channels_ = 0;
};

/// Callback type for music events.
using MusicEventCallback = std::function<void()>;

/**
 * @brief General-purpose music player supporting multiple audio formats.
 *
 * Uses SDL3 for audio output and supports WAV, MP3, OGG, and FLAC formats.
 * When active, it can automatically mute Gothic's native music system.
 *
 * Usage:
 *   MusicPlayer player;
 *   player.Load("music.mp3");
 *   player.SetVolume(0.8f);
 *   player.Play();
 */
class MusicPlayer {
 public:
  MusicPlayer();
  ~MusicPlayer();

  // Non-copyable
  MusicPlayer(const MusicPlayer&) = delete;
  MusicPlayer& operator=(const MusicPlayer&) = delete;

  /// Load audio file (auto-detects format from extension).
  bool Load(const std::string& filepath);

  /// Load audio from memory with explicit format.
  bool LoadFromMemory(const uint8_t* data, size_t size, AudioFormat format);

  /// Start playback.
  void Play(bool loop = false);

  /// Pause playback (can be resumed).
  void Pause();

  /// Resume paused playback.
  void Resume();

  /// Stop playback and reset position.
  void Stop();

  /// Check if currently playing.
  bool IsPlaying() const;

  /// Check if paused.
  bool IsPaused() const;

  /// Set volume (0.0 to 1.0).
  void SetVolume(float volume);

  /// Get current volume.
  float GetVolume() const;

  /// Get current playback position in seconds.
  float GetPosition() const;

  /// Seek to position in seconds.
  void Seek(float position);

  /// Get total duration in seconds.
  float GetDuration() const;

  /// Get the currently loaded file path.
  const std::string& GetFilePath() const { return filepath_; }

  /// Set callback for when music finishes playing.
  void SetOnFinishedCallback(MusicEventCallback callback);

  /// Enable/disable automatic muting of Gothic's music when playing.
  void SetMuteGothicMusic(bool mute) { mute_gothic_music_ = mute; }

  /// Check if Gothic music muting is enabled.
  bool GetMuteGothicMusic() const { return mute_gothic_music_; }

  /// Get the user's music volume setting from Gothic's options (0.0 to 1.0).
  /// The final playback volume is player_volume * options_volume.
  float GetOptionsVolume() const;

  /// Check if using Gothic's options volume for scaling.
  bool GetUseOptionsVolume() const { return use_options_volume_; }

  /// Enable/disable using Gothic's options volume for scaling.
  void SetUseOptionsVolume(bool use) { use_options_volume_ = use; }

  /// Get singleton instance for global music control.
  static MusicPlayer& Instance();

 private:
  bool InitAudioDevice();
  void ShutdownAudioDevice();
  void FeedAudioStream();

  // Gothic music system integration.
  void MuteGothicMusic();
  void UnmuteGothicMusic();

  std::string filepath_;
  std::unique_ptr<AudioDecoder> decoder_;

  SDL_AudioStream* stream_ = nullptr;
  SDL_AudioSpec audio_spec_{};

  std::atomic<bool> playing_{false};
  std::atomic<bool> paused_{false};
  std::atomic<bool> looping_{false};
  std::atomic<float> volume_{1.0f};
  std::atomic<float> options_volume_{1.0f};
  std::atomic<size_t> playback_position_{0};

  std::thread playback_thread_;
  std::atomic<bool> stop_thread_{false};
  std::mutex mutex_;

  MusicEventCallback on_finished_callback_;
  bool mute_gothic_music_ = true;
  bool use_options_volume_ = true;
  float gothic_music_volume_backup_ = 1.0f;

  void PlaybackLoop();
};

}  // namespace gmp::audio

#endif  // GMP_CLIENT_GOTHIC2A_LIB_AUDIO_MUSIC_PLAYER_H_

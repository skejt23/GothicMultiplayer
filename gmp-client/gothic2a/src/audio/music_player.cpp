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

#include "music_player.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>

// Use header-only decoders from xmake packages
#define MINIMP3_IMPLEMENTATION
#include <minimp3.h>
#include <minimp3_ex.h>

#include <stb/stb_vorbis.c>

namespace gmp::audio {

/// Callback type for volume change notifications from zCOptions.
using VolumeChangeCallback = void (*)(float new_volume);

// Forward declaration for Gothic music system access.
namespace gothic {
void MuteGothicMusicSystem();
void UnmuteGothicMusicSystem();
float GetGothicMusicVolume();
void SetGothicMusicVolume(float volume);
float GetOptionsVolume();
float RegisterVolumeChangeCallback(VolumeChangeCallback callback);
void UnregisterVolumeChangeCallback(VolumeChangeCallback callback);
}  // namespace gothic

// ============================================================================
// AudioDecoder Factory
// ============================================================================

AudioFormat AudioDecoder::DetectFormat(const std::string& filepath) {
  // Find last dot
  size_t dot_pos = filepath.rfind('.');
  if (dot_pos == std::string::npos) {
    return AudioFormat::kUnknown;
  }

  std::string ext = filepath.substr(dot_pos + 1);
  // Convert to lowercase
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (ext == "wav" || ext == "wave") {
    return AudioFormat::kWav;
  } else if (ext == "mp3") {
    return AudioFormat::kMp3;
  } else if (ext == "ogg" || ext == "oga") {
    return AudioFormat::kOgg;
  } else if (ext == "flac") {
    return AudioFormat::kFlac;
  }

  return AudioFormat::kUnknown;
}

std::unique_ptr<AudioDecoder> AudioDecoder::CreateForFormat(AudioFormat format) {
  switch (format) {
    case AudioFormat::kWav:
      return std::make_unique<WavDecoder>();
    case AudioFormat::kMp3:
      return std::make_unique<Mp3Decoder>();
    case AudioFormat::kOgg:
      return std::make_unique<OggDecoder>();
    case AudioFormat::kFlac:
      SPDLOG_WARN("FLAC format not yet implemented, falling back to WAV decoder");
      return std::make_unique<WavDecoder>();
    default:
      return nullptr;
  }
}

// ============================================================================
// WavDecoder Implementation
// ============================================================================

bool WavDecoder::Load(const std::string& filepath) {
  SDL_AudioSpec spec;
  Uint8* audio_buf = nullptr;
  Uint32 audio_len = 0;

  if (!SDL_LoadWAV(filepath.c_str(), &spec, &audio_buf, &audio_len)) {
    SPDLOG_ERROR("Failed to load WAV file '{}': {}", filepath, SDL_GetError());
    return false;
  }

  sample_rate_ = spec.freq;
  channels_ = spec.channels;

  // Determine bits per sample from format
  switch (spec.format) {
    case SDL_AUDIO_S8:
    case SDL_AUDIO_U8:
      bits_per_sample_ = 8;
      break;
    case SDL_AUDIO_S16:
      bits_per_sample_ = 16;
      break;
    case SDL_AUDIO_S32:
    case SDL_AUDIO_F32:
      bits_per_sample_ = 32;
      break;
    default:
      bits_per_sample_ = 16;
  }

  pcm_data_.assign(audio_buf, audio_buf + audio_len);
  SDL_free(audio_buf);

  SPDLOG_DEBUG("Loaded WAV: {}Hz, {} channels, {}-bit, {} bytes",
               sample_rate_, channels_, bits_per_sample_, pcm_data_.size());
  return true;
}

bool WavDecoder::LoadFromMemory(const uint8_t* data, size_t size) {
  SDL_IOStream* io = SDL_IOFromConstMem(data, size);
  if (!io) {
    SPDLOG_ERROR("Failed to create IO stream for WAV data");
    return false;
  }

  SDL_AudioSpec spec;
  Uint8* audio_buf = nullptr;
  Uint32 audio_len = 0;

  if (!SDL_LoadWAV_IO(io, true, &spec, &audio_buf, &audio_len)) {
    SPDLOG_ERROR("Failed to load WAV from memory: {}", SDL_GetError());
    return false;
  }

  sample_rate_ = spec.freq;
  channels_ = spec.channels;
  bits_per_sample_ = 16;  // Assume 16-bit for simplicity.

  pcm_data_.assign(audio_buf, audio_buf + audio_len);
  SDL_free(audio_buf);

  return true;
}

float WavDecoder::GetDuration() const {
  if (sample_rate_ == 0 || channels_ == 0 || bits_per_sample_ == 0) {
    return 0.0f;
  }
  size_t bytes_per_sample = (bits_per_sample_ / 8) * channels_;
  size_t total_samples = pcm_data_.size() / bytes_per_sample;
  return static_cast<float>(total_samples) / static_cast<float>(sample_rate_);
}

// ============================================================================
// Mp3Decoder Implementation
// ============================================================================

bool Mp3Decoder::Load(const std::string& filepath) {
  // Read file into memory
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    SPDLOG_ERROR("Failed to open MP3 file: {}", filepath);
    return false;
  }

  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> file_data(file_size);
  if (!file.read(reinterpret_cast<char*>(file_data.data()), file_size)) {
    SPDLOG_ERROR("Failed to read MP3 file: {}", filepath);
    return false;
  }

  return LoadFromMemory(file_data.data(), file_data.size());
}

bool Mp3Decoder::LoadFromMemory(const uint8_t* data, size_t size) {
  mp3dec_t mp3d;
  mp3dec_file_info_t info;

  mp3dec_init(&mp3d);

  // Decode entire MP3 to PCM
  if (mp3dec_load_buf(&mp3d, data, size, &info, nullptr, nullptr) != 0) {
    SPDLOG_ERROR("Failed to decode MP3 data");
    return false;
  }

  if (info.samples == 0) {
    SPDLOG_ERROR("MP3 file contains no audio samples");
    return false;
  }

  sample_rate_ = info.hz;
  channels_ = info.channels;

  // Convert mp3d_sample_t (int16_t) to bytes
  size_t pcm_size = info.samples * sizeof(mp3d_sample_t);
  pcm_data_.resize(pcm_size);
  std::memcpy(pcm_data_.data(), info.buffer, pcm_size);

  // Free the buffer allocated by minimp3
  free(info.buffer);

  SPDLOG_DEBUG("Decoded MP3: {}Hz, {} channels, {} samples",
               sample_rate_, channels_, info.samples);
  return true;
}

float Mp3Decoder::GetDuration() const {
  if (sample_rate_ == 0 || channels_ == 0) {
    return 0.0f;
  }
  size_t total_samples = pcm_data_.size() / (2 * channels_);  // 16-bit = 2 bytes.
  return static_cast<float>(total_samples) / static_cast<float>(sample_rate_);
}

// ============================================================================
// OggDecoder Implementation
// ============================================================================

bool OggDecoder::Load(const std::string& filepath) {
  int channels, sample_rate;
  short* output;

  int samples = stb_vorbis_decode_filename(filepath.c_str(), &channels, &sample_rate, &output);
  if (samples < 0) {
    SPDLOG_ERROR("Failed to decode OGG file: {}", filepath);
    return false;
  }

  channels_ = channels;
  sample_rate_ = sample_rate;

  // Convert to bytes
  size_t pcm_size = samples * channels * sizeof(short);
  pcm_data_.resize(pcm_size);
  std::memcpy(pcm_data_.data(), output, pcm_size);

  free(output);

  SPDLOG_DEBUG("Decoded OGG: {}Hz, {} channels, {} samples",
               sample_rate_, channels_, samples);
  return true;
}

bool OggDecoder::LoadFromMemory(const uint8_t* data, size_t size) {
  int channels, sample_rate;
  short* output;

  int samples = stb_vorbis_decode_memory(data, static_cast<int>(size),
                                          &channels, &sample_rate, &output);
  if (samples < 0) {
    SPDLOG_ERROR("Failed to decode OGG from memory");
    return false;
  }

  channels_ = channels;
  sample_rate_ = sample_rate;

  size_t pcm_size = samples * channels * sizeof(short);
  pcm_data_.resize(pcm_size);
  std::memcpy(pcm_data_.data(), output, pcm_size);

  free(output);

  return true;
}

float OggDecoder::GetDuration() const {
  if (sample_rate_ == 0 || channels_ == 0) {
    return 0.0f;
  }
  size_t total_samples = pcm_data_.size() / (2 * channels_);
  return static_cast<float>(total_samples) / static_cast<float>(sample_rate_);
}

// ============================================================================
// MusicPlayer Implementation
// ============================================================================

MusicPlayer& MusicPlayer::Instance() {
  static MusicPlayer instance;
  return instance;
}

MusicPlayer::MusicPlayer() {
  // Get initial options volume.
  options_volume_ = gothic::GetOptionsVolume();
  SPDLOG_DEBUG("MusicPlayer created (options volume: {})", options_volume_.load());
}

MusicPlayer::~MusicPlayer() {
  Stop();
  ShutdownAudioDevice();
  SPDLOG_DEBUG("MusicPlayer destroyed");
}

bool MusicPlayer::InitAudioDevice() {
  if (stream_) {
    return true;  // Already initialized.
  }

  if (!decoder_) {
    SPDLOG_ERROR("Cannot init audio device: no decoder loaded");
    return false;
  }

  // Set up audio spec based on decoded audio
  audio_spec_.freq = decoder_->GetSampleRate();
  audio_spec_.channels = decoder_->GetChannels();
  audio_spec_.format = SDL_AUDIO_S16;  // All our decoders output 16-bit

  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                       &audio_spec_, nullptr, nullptr);
  if (!stream_) {
    SPDLOG_ERROR("Failed to open audio device: {}", SDL_GetError());
    return false;
  }

  SPDLOG_DEBUG("Audio device initialized: {}Hz, {} channels",
               audio_spec_.freq, audio_spec_.channels);
  return true;
}

void MusicPlayer::ShutdownAudioDevice() {
  if (stream_) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
}

bool MusicPlayer::Load(const std::string& filepath) {
  Stop();

  AudioFormat format = AudioDecoder::DetectFormat(filepath);
  if (format == AudioFormat::kUnknown) {
    SPDLOG_ERROR("Unknown audio format for file: {}", filepath);
    return false;
  }

  decoder_ = AudioDecoder::CreateForFormat(format);
  if (!decoder_) {
    SPDLOG_ERROR("Failed to create decoder for format");
    return false;
  }

  if (!decoder_->Load(filepath)) {
    decoder_.reset();
    return false;
  }

  filepath_ = filepath;

  // Reinitialize audio device for new format
  ShutdownAudioDevice();
  if (!InitAudioDevice()) {
    decoder_.reset();
    return false;
  }

  SPDLOG_INFO("Loaded music: {} (duration: {:.1f}s)", filepath, GetDuration());
  return true;
}

bool MusicPlayer::LoadFromMemory(const uint8_t* data, size_t size, AudioFormat format) {
  Stop();

  if (format == AudioFormat::kUnknown) {
    SPDLOG_ERROR("Must specify audio format when loading from memory");
    return false;
  }

  decoder_ = AudioDecoder::CreateForFormat(format);
  if (!decoder_) {
    SPDLOG_ERROR("Failed to create decoder for format");
    return false;
  }

  if (!decoder_->LoadFromMemory(data, size)) {
    decoder_.reset();
    return false;
  }

  filepath_ = "<memory>";

  ShutdownAudioDevice();
  if (!InitAudioDevice()) {
    decoder_.reset();
    return false;
  }

  return true;
}

void MusicPlayer::Play(bool loop) {
  if (!decoder_ || !stream_) {
    SPDLOG_WARN("Cannot play: no music loaded");
    return;
  }

  if (playing_) {
    Stop();
  }

  looping_ = loop;
  playing_ = true;
  paused_ = false;
  playback_position_ = 0;

  // Mute Gothic's music if configured
  if (mute_gothic_music_) {
    MuteGothicMusic();
  }

  // Start playback
  if (!SDL_ResumeAudioStreamDevice(stream_)) {
    SPDLOG_ERROR("Failed to resume audio stream: {}", SDL_GetError());
    playing_ = false;
    return;
  }

  // Start playback thread
  stop_thread_ = false;
  playback_thread_ = std::thread(&MusicPlayer::PlaybackLoop, this);

  SPDLOG_DEBUG("Music playback started (loop={})", loop);
}

void MusicPlayer::Pause() {
  if (!playing_ || paused_) {
    return;
  }

  paused_ = true;
  SDL_PauseAudioStreamDevice(stream_);
  SPDLOG_DEBUG("Music paused");
}

void MusicPlayer::Resume() {
  if (!playing_ || !paused_) {
    return;
  }

  paused_ = false;
  SDL_ResumeAudioStreamDevice(stream_);
  SPDLOG_DEBUG("Music resumed");
}

void MusicPlayer::Stop() {
  if (!playing_) {
    return;
  }

  playing_ = false;
  paused_ = false;
  stop_thread_ = true;

  if (playback_thread_.joinable()) {
    playback_thread_.join();
  }

  if (stream_) {
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
  }

  playback_position_ = 0;

  // Restore Gothic's music
  if (mute_gothic_music_) {
    UnmuteGothicMusic();
  }

  SPDLOG_DEBUG("Music stopped");
}

bool MusicPlayer::IsPlaying() const {
  return playing_ && !paused_;
}

bool MusicPlayer::IsPaused() const {
  return playing_ && paused_;
}

void MusicPlayer::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float MusicPlayer::GetVolume() const {
  return volume_;
}

float MusicPlayer::GetPosition() const {
  if (!decoder_ || decoder_->GetSampleRate() == 0) {
    return 0.0f;
  }

  size_t bytes_per_sample = 2 * decoder_->GetChannels();  // 16-bit.
  size_t samples_played = playback_position_ / bytes_per_sample;
  return static_cast<float>(samples_played) / static_cast<float>(decoder_->GetSampleRate());
}

void MusicPlayer::Seek(float position) {
  if (!decoder_) {
    return;
  }

  float duration = GetDuration();
  position = std::clamp(position, 0.0f, duration);

  size_t bytes_per_sample = 2 * decoder_->GetChannels();
  size_t target_sample = static_cast<size_t>(position * decoder_->GetSampleRate());
  playback_position_ = target_sample * bytes_per_sample;

  // Clear buffered audio for immediate seek.
  if (stream_) {
    SDL_ClearAudioStream(stream_);
  }
}

float MusicPlayer::GetDuration() const {
  return decoder_ ? decoder_->GetDuration() : 0.0f;
}

float MusicPlayer::GetOptionsVolume() const {
  // Poll directly from Gothic's options for real-time value.
  return gothic::GetOptionsVolume();
}

void MusicPlayer::SetOnFinishedCallback(MusicEventCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  on_finished_callback_ = std::move(callback);
}

void MusicPlayer::PlaybackLoop() {
  while (!stop_thread_ && playing_) {
    if (paused_) {
      SDL_Delay(10);
      continue;
    }

    FeedAudioStream();
    SDL_Delay(5);  // Small delay to prevent spinning.
  }
}

void MusicPlayer::FeedAudioStream() {
  if (!decoder_ || !stream_) {
    return;
  }

  const auto& pcm_data = decoder_->GetPCMData();
  size_t data_size = pcm_data.size();

  // Check if we need more data in the stream.
  constexpr int kLowWaterMark = 8192;
  int queued = SDL_GetAudioStreamQueued(stream_);

  if (queued > kLowWaterMark) {
    return;  // Enough data buffered.
  }

  size_t pos = playback_position_.load();

  if (pos >= data_size) {
    // End of track
    if (looping_) {
      playback_position_ = 0;
      pos = 0;
    } else {
      playing_ = false;

      // Fire callback
      std::lock_guard<std::mutex> lock(mutex_);
      if (on_finished_callback_) {
        on_finished_callback_();
      }
      return;
    }
  }

  // Calculate how much to feed.
  constexpr size_t kChunkSize = 16384;
  size_t remaining = data_size - pos;
  size_t to_feed = std::min(kChunkSize, remaining);

  // Apply volume by scaling samples.
  // Final volume = player_volume * options_volume (if enabled).
  std::vector<int16_t> volume_adjusted(to_feed / 2);
  const int16_t* src = reinterpret_cast<const int16_t*>(pcm_data.data() + pos);
  float vol = volume_.load();
  if (use_options_volume_) {
    // Poll current options volume directly for real-time updates.
    vol *= gothic::GetOptionsVolume();
  }

  for (size_t i = 0; i < volume_adjusted.size(); ++i) {
    volume_adjusted[i] = static_cast<int16_t>(src[i] * vol);
  }

  if (!SDL_PutAudioStreamData(stream_, volume_adjusted.data(), to_feed)) {
    SPDLOG_ERROR("Failed to put audio stream data: {}", SDL_GetError());
    return;
  }

  playback_position_ = pos + to_feed;
}

void MusicPlayer::MuteGothicMusic() {
  // Store current volume and mute
  gothic_music_volume_backup_ = gothic::GetGothicMusicVolume();
  gothic::SetGothicMusicVolume(0.0f);
  SPDLOG_DEBUG("Gothic music muted (was {})", gothic_music_volume_backup_);
}

void MusicPlayer::UnmuteGothicMusic() {
  // Restore previous volume
  gothic::SetGothicMusicVolume(gothic_music_volume_backup_);
  SPDLOG_DEBUG("Gothic music restored to {}", gothic_music_volume_backup_);
}

}  // namespace gmp::audio

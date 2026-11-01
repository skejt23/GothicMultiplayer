/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

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

#include "VoicePlayback.h"

#include "VoiceCapture.h"

#include <SDL3/SDL.h>
#include <opus/opus.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
#include <utility>

namespace gmp::voice {
namespace {

constexpr int kOutputChannels = 2;
constexpr std::size_t kJitterFrames = 3;
constexpr std::size_t kMaxQueuedFrames = 12;
constexpr std::uint32_t kMaxRecoverableGap = 5;

std::optional<SDL_AudioDeviceID> FindPlaybackDevice(const std::string& device_name) {
  if (device_name.empty()) {
    return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  }

  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
  if (!devices) {
    return std::nullopt;
  }

  std::optional<SDL_AudioDeviceID> result;
  for (int i = 0; i < count; ++i) {
    const char* name = SDL_GetAudioDeviceName(devices[i]);
    if (name && device_name == name) {
      result = devices[i];
      break;
    }
  }
  SDL_free(devices);
  return result;
}

bool IsNewerSequence(std::uint32_t value, std::uint32_t previous) {
  return static_cast<std::int32_t>(value - previous) > 0;
}

std::int16_t ClampSample(float sample) {
  if (!std::isfinite(sample)) {
    return 0;
  }
  return static_cast<std::int16_t>(std::clamp(sample,
                                              static_cast<float>(std::numeric_limits<std::int16_t>::min()),
                                              static_cast<float>(std::numeric_limits<std::int16_t>::max())));
}

}  // namespace

struct VoicePlayback::SpeakerState {
  ~SpeakerState() {
    if (decoder) {
      opus_decoder_destroy(decoder);
    }
  }

  OpusDecoder* decoder{nullptr};
  bool has_talkspurt{false};
  std::uint32_t talkspurt_id{0};
  std::uint32_t expected_sequence{0};
  std::deque<std::int16_t> samples;
  bool buffering{true};
  float gain{1.0f};
};

VoicePlayback::VoicePlayback() = default;

VoicePlayback::~VoicePlayback() {
  StopPlayback();
}

bool VoicePlayback::StartPlayback() {
  if (output_stream_) {
    return true;
  }

  SDL_AudioSpec desired{};
  desired.freq = kSampleRate;
  desired.format = SDL_AUDIO_S16;
  desired.channels = kOutputChannels;

  const auto output_device = FindPlaybackDevice(output_device_name_);
  if (!output_device.has_value()) {
    SPDLOG_ERROR("Voice playback device '{}' is unavailable", output_device_name_);
    return false;
  }

  output_stream_ = SDL_OpenAudioDeviceStream(*output_device, &desired,
                                              &VoicePlayback::AudioStreamCallback, this);
  if (!output_stream_) {
    SPDLOG_ERROR("Failed to open voice playback device: {}", SDL_GetError());
    return false;
  }

  if (!SDL_ResumeAudioStreamDevice(output_stream_)) {
    SPDLOG_ERROR("Failed to start voice playback device: {}", SDL_GetError());
    SDL_DestroyAudioStream(output_stream_);
    output_stream_ = nullptr;
    return false;
  }

  SPDLOG_INFO("Voice playback initialized ({} Hz, stereo output)", kSampleRate);
  return true;
}

void VoicePlayback::StopPlayback() {
  if (output_stream_) {
    SDL_PauseAudioStreamDevice(output_stream_);
    SDL_DestroyAudioStream(output_stream_);
    output_stream_ = nullptr;
  }
  ClearPlayers();
}

bool VoicePlayback::IsPlaying() const {
  return output_stream_ != nullptr;
}

VoicePlayback::SpeakerState* VoicePlayback::GetOrCreateSpeaker(std::uint32_t player_id) {
  auto existing = speakers_.find(player_id);
  if (existing != speakers_.end()) {
    return existing->second.get();
  }

  int error = OPUS_OK;
  OpusDecoder* decoder = opus_decoder_create(kSampleRate, kChannels, &error);
  if (!decoder || error != OPUS_OK) {
    SPDLOG_WARN("Failed to create Opus decoder for player {}: {}", player_id, opus_strerror(error));
    if (decoder) {
      opus_decoder_destroy(decoder);
    }
    return nullptr;
  }

  auto speaker = std::make_unique<SpeakerState>();
  speaker->decoder = decoder;
  SpeakerState* result = speaker.get();
  speakers_.emplace(player_id, std::move(speaker));
  return result;
}

bool VoicePlayback::PlayVoice(std::uint32_t player_id, std::uint32_t talkspurt_id, std::uint32_t sequence,
                              const std::vector<std::uint8_t>& encoded_data, float gain) {
  if (!output_stream_ || player_id == 0 || talkspurt_id == 0 || encoded_data.empty() ||
      encoded_data.size() > kMaxEncodedFrameSize || !std::isfinite(gain)) {
    return false;
  }

  const int packet_samples = opus_packet_get_nb_samples(encoded_data.data(), static_cast<opus_int32>(encoded_data.size()), kSampleRate);
  if (packet_samples != kFrameSamples) {
    SPDLOG_WARN("Rejected invalid Opus voice frame from player {} ({} samples)", player_id, packet_samples);
    return false;
  }

  std::lock_guard<std::mutex> lock(playback_mutex_);
  SpeakerState* speaker = GetOrCreateSpeaker(player_id);
  if (!speaker) {
    return false;
  }

  if (!speaker->has_talkspurt || speaker->talkspurt_id != talkspurt_id) {
    if (speaker->has_talkspurt && !IsNewerSequence(talkspurt_id, speaker->talkspurt_id)) {
      return false;
    }

    const int reset_error = opus_decoder_ctl(speaker->decoder, OPUS_RESET_STATE);
    if (reset_error != OPUS_OK) {
      SPDLOG_WARN("Failed to reset Opus decoder for player {}: {}", player_id, opus_strerror(reset_error));
      return false;
    }
    speaker->has_talkspurt = true;
    speaker->talkspurt_id = talkspurt_id;
    speaker->expected_sequence = sequence;
    speaker->samples.clear();
    speaker->buffering = true;
  }

  if (sequence != speaker->expected_sequence) {
    if (!IsNewerSequence(sequence, speaker->expected_sequence)) {
      return false;
    }

    const std::uint32_t gap = sequence - speaker->expected_sequence;
    if (gap > kMaxRecoverableGap) {
      opus_decoder_ctl(speaker->decoder, OPUS_RESET_STATE);
      speaker->samples.clear();
      speaker->buffering = true;
      speaker->expected_sequence = sequence;
    } else {
      std::array<std::int16_t, kFrameSamples> recovered{};
      if (gap == 1) {
        const int decoded = opus_decode(speaker->decoder, encoded_data.data(), static_cast<opus_int32>(encoded_data.size()),
                                        recovered.data(), kFrameSamples, 1);
        if (decoded > 0) {
          speaker->samples.insert(speaker->samples.end(), recovered.begin(), recovered.begin() + decoded);
        }
      } else {
        for (std::uint32_t missing = 0; missing < gap; ++missing) {
          const int decoded = opus_decode(speaker->decoder, nullptr, 0, recovered.data(), kFrameSamples, 0);
          if (decoded <= 0) {
            break;
          }
          speaker->samples.insert(speaker->samples.end(), recovered.begin(), recovered.begin() + decoded);
        }
      }
      speaker->expected_sequence = sequence;
    }
  }

  std::array<std::int16_t, kFrameSamples> decoded_pcm{};
  const int decoded_samples = opus_decode(speaker->decoder, encoded_data.data(), static_cast<opus_int32>(encoded_data.size()),
                                          decoded_pcm.data(), kFrameSamples, 0);
  if (decoded_samples <= 0) {
    SPDLOG_WARN("Failed to decode voice frame from player {}: {}", player_id, opus_strerror(decoded_samples));
    return false;
  }

  speaker->samples.insert(speaker->samples.end(), decoded_pcm.begin(), decoded_pcm.begin() + decoded_samples);
  speaker->expected_sequence = sequence + 1;
  speaker->gain = std::clamp(gain, 0.0f, 1.0f);

  const std::size_t max_samples = kMaxQueuedFrames * static_cast<std::size_t>(kFrameSamples);
  const std::size_t target_samples = kJitterFrames * static_cast<std::size_t>(kFrameSamples);
  if (speaker->samples.size() > max_samples) {
    const std::size_t to_drop = speaker->samples.size() - target_samples;
    for (std::size_t i = 0; i < to_drop; ++i) {
      speaker->samples.pop_front();
    }
    speaker->buffering = true;
  }
  return true;
}

void VoicePlayback::RemovePlayer(std::uint32_t player_id) {
  std::lock_guard<std::mutex> lock(playback_mutex_);
  speakers_.erase(player_id);
}

void VoicePlayback::ClearPlayers() {
  std::lock_guard<std::mutex> lock(playback_mutex_);
  speakers_.clear();
}

void VoicePlayback::SetMasterVolume(float volume) {
  if (!std::isfinite(volume)) {
    return;
  }
  std::lock_guard<std::mutex> lock(playback_mutex_);
  master_volume_ = std::clamp(volume, 0.0f, 1.0f);
}

float VoicePlayback::GetMasterVolume() const {
  std::lock_guard<std::mutex> lock(playback_mutex_);
  return master_volume_;
}

std::vector<std::string> VoicePlayback::GetOutputDevices() const {
  std::vector<std::string> result;
  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
  if (!devices) {
    SPDLOG_WARN("Failed to enumerate voice playback devices: {}", SDL_GetError());
    return result;
  }

  result.reserve(static_cast<std::size_t>(std::max(count, 0)));
  for (int i = 0; i < count; ++i) {
    const char* name = SDL_GetAudioDeviceName(devices[i]);
    if (name) {
      result.emplace_back(name);
    }
  }
  SDL_free(devices);
  return result;
}

const std::string& VoicePlayback::GetOutputDevice() const {
  return output_device_name_;
}

bool VoicePlayback::SetOutputDevice(const std::string& device_name) {
  if (!FindPlaybackDevice(device_name).has_value()) {
    return false;
  }
  output_device_name_ = device_name;
  return true;
}

void SDLCALL VoicePlayback::AudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int) {
  auto* playback = static_cast<VoicePlayback*>(userdata);
  if (playback) {
    playback->MixAudio(stream, additional_amount);
  }
}

void VoicePlayback::MixAudio(SDL_AudioStream* stream, int additional_amount) {
  if (additional_amount <= 0) {
    return;
  }

  const std::size_t output_frame_bytes = sizeof(std::int16_t) * kOutputChannels;
  const std::size_t frame_count = (static_cast<std::size_t>(additional_amount) + output_frame_bytes - 1) / output_frame_bytes;
  mix_buffer_.assign(frame_count, 0.0f);

  {
    std::lock_guard<std::mutex> lock(playback_mutex_);
    const std::size_t jitter_samples = kJitterFrames * static_cast<std::size_t>(kFrameSamples);
    for (auto& [player_id, speaker_ptr] : speakers_) {
      (void)player_id;
      SpeakerState& speaker = *speaker_ptr;
      if (speaker.buffering) {
        if (speaker.samples.size() < jitter_samples) {
          continue;
        }
        speaker.buffering = false;
      }

      const std::size_t samples_to_mix = std::min(frame_count, speaker.samples.size());
      const float gain = speaker.gain * master_volume_;
      for (std::size_t i = 0; i < samples_to_mix; ++i) {
        mix_buffer_[i] += static_cast<float>(speaker.samples.front()) * gain;
        speaker.samples.pop_front();
      }
      if (speaker.samples.empty()) {
        speaker.buffering = true;
      }
    }
  }

  output_buffer_.assign(frame_count * kOutputChannels, 0);
  for (std::size_t i = 0; i < frame_count; ++i) {
    const std::int16_t sample = ClampSample(mix_buffer_[i]);
    output_buffer_[i * kOutputChannels] = sample;
    output_buffer_[i * kOutputChannels + 1] = sample;
  }

  if (!SDL_PutAudioStreamData(stream, output_buffer_.data(),
                              static_cast<int>(output_buffer_.size() * sizeof(std::int16_t)))) {
    SPDLOG_ERROR("Failed to queue mixed voice playback data: {}", SDL_GetError());
  }
}

}  // namespace gmp::voice

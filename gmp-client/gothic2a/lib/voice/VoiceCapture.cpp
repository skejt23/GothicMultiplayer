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

#include "VoiceCapture.h"

#include <SDL3/SDL.h>
#include <opus/opus.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

namespace gmp::voice {
namespace {

constexpr int kVoiceBitrate = 24000;
constexpr std::size_t kMaxPendingFrames = 10;

std::optional<SDL_AudioDeviceID> FindRecordingDevice(const std::string& device_name) {
  if (device_name.empty()) {
    return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
  }

  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
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

}  // namespace

VoiceCapture::VoiceCapture() = default;

VoiceCapture::~VoiceCapture() {
  StopCapture();
  if (encoder_) {
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
  }
}

bool VoiceCapture::EnsureEncoder() {
  if (encoder_) {
    return true;
  }

  int error = OPUS_OK;
  encoder_ = opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_VOIP, &error);
  if (!encoder_ || error != OPUS_OK) {
    SPDLOG_ERROR("Failed to create Opus voice encoder: {}", opus_strerror(error));
    encoder_ = nullptr;
    return false;
  }

  if (opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(kVoiceBitrate)) != OPUS_OK ||
      opus_encoder_ctl(encoder_, OPUS_SET_VBR(1)) != OPUS_OK ||
      opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(5)) != OPUS_OK ||
      opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1)) != OPUS_OK ||
      opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(10)) != OPUS_OK ||
      opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)) != OPUS_OK) {
    SPDLOG_ERROR("Failed to configure Opus voice encoder");
    opus_encoder_destroy(encoder_);
    encoder_ = nullptr;
    return false;
  }

  return true;
}

bool VoiceCapture::StartCapture() {
  if (input_stream_) {
    return true;
  }
  if (!EnsureEncoder()) {
    return false;
  }

  SDL_AudioSpec desired{};
  desired.freq = kSampleRate;
  desired.format = SDL_AUDIO_S16;
  desired.channels = kChannels;

  const auto input_device = FindRecordingDevice(input_device_name_);
  if (!input_device.has_value()) {
    SPDLOG_ERROR("Voice capture device '{}' is unavailable", input_device_name_);
    return false;
  }

  input_stream_ = SDL_OpenAudioDeviceStream(*input_device, &desired, &VoiceCapture::AudioStreamCallback, this);
  if (!input_stream_) {
    SPDLOG_ERROR("Failed to open voice capture device: {}", SDL_GetError());
    return false;
  }

  if (!SDL_ResumeAudioStreamDevice(input_stream_)) {
    SPDLOG_ERROR("Failed to start voice capture device: {}", SDL_GetError());
    SDL_DestroyAudioStream(input_stream_);
    input_stream_ = nullptr;
    return false;
  }

  capturing_.store(true, std::memory_order_release);
  SPDLOG_INFO("Voice capture initialized ({} Hz, mono, {} ms frames)", kSampleRate, kFrameDurationMs);
  return true;
}

void VoiceCapture::StopCapture() {
  transmitting_.store(false, std::memory_order_release);
  capturing_.store(false, std::memory_order_release);

  if (input_stream_) {
    SDL_PauseAudioStreamDevice(input_stream_);
    SDL_DestroyAudioStream(input_stream_);
    input_stream_ = nullptr;
  }

  std::lock_guard<std::mutex> lock(capture_mutex_);
  pending_frames_.clear();
  partial_frame_.fill(0);
  partial_frame_size_ = 0;
  next_sequence_ = 0;
  dropped_frames_.store(0, std::memory_order_relaxed);
}

bool VoiceCapture::IsCapturing() const {
  return capturing_.load(std::memory_order_acquire);
}

void VoiceCapture::SetTransmitting(bool transmitting) {
  const bool was_transmitting = transmitting_.load(std::memory_order_acquire);
  if (was_transmitting == transmitting) {
    return;
  }

  std::lock_guard<std::mutex> lock(capture_mutex_);
  pending_frames_.clear();
  partial_frame_.fill(0);
  partial_frame_size_ = 0;
  dropped_frames_.store(0, std::memory_order_relaxed);

  if (transmitting) {
    if (!EnsureEncoder()) {
      return;
    }
    const int error = opus_encoder_ctl(encoder_, OPUS_RESET_STATE);
    if (error != OPUS_OK) {
      SPDLOG_ERROR("Failed to reset Opus voice encoder: {}", opus_strerror(error));
      return;
    }
    ++talkspurt_id_;
    if (talkspurt_id_ == 0) {
      ++talkspurt_id_;
    }
    next_sequence_ = 0;
  }

  transmitting_.store(transmitting, std::memory_order_release);
}

bool VoiceCapture::IsTransmitting() const {
  return transmitting_.load(std::memory_order_acquire);
}

void SDLCALL VoiceCapture::AudioStreamCallback(void* userdata, SDL_AudioStream* stream, int, int) {
  auto* capture = static_cast<VoiceCapture*>(userdata);
  if (capture) {
    capture->DrainAudioStream(stream);
  }
}

void VoiceCapture::DrainAudioStream(SDL_AudioStream* stream) {
  std::array<std::int16_t, kFrameSamples * 2> samples{};
  for (;;) {
    const int available = SDL_GetAudioStreamAvailable(stream);
    if (available <= 0) {
      return;
    }

    const int bytes_to_read = std::min(available, static_cast<int>(samples.size() * sizeof(std::int16_t)));
    const int bytes_read = SDL_GetAudioStreamData(stream, samples.data(), bytes_to_read);
    if (bytes_read <= 0) {
      if (bytes_read < 0) {
        SPDLOG_ERROR("Failed to read voice capture data: {}", SDL_GetError());
      }
      return;
    }

    if (transmitting_.load(std::memory_order_acquire)) {
      AppendSamples(samples.data(), static_cast<std::size_t>(bytes_read) / sizeof(std::int16_t));
    }
  }
}

void VoiceCapture::AppendSamples(const std::int16_t* samples, std::size_t count) {
  std::lock_guard<std::mutex> lock(capture_mutex_);
  if (!transmitting_.load(std::memory_order_relaxed)) {
    return;
  }

  while (count > 0) {
    const std::size_t copy_count = std::min(count, static_cast<std::size_t>(kFrameSamples) - partial_frame_size_);
    std::memcpy(partial_frame_.data() + partial_frame_size_, samples, copy_count * sizeof(std::int16_t));
    partial_frame_size_ += copy_count;
    samples += copy_count;
    count -= copy_count;

    if (partial_frame_size_ != kFrameSamples) {
      continue;
    }

    if (pending_frames_.size() >= kMaxPendingFrames) {
      pending_frames_.pop_front();
      ++next_sequence_;
      dropped_frames_.fetch_add(1, std::memory_order_relaxed);
    }
    pending_frames_.push_back(partial_frame_);
    partial_frame_.fill(0);
    partial_frame_size_ = 0;
  }
}

std::vector<VoiceCapture::EncodedFrame> VoiceCapture::ConsumeEncodedFrames() {
  std::deque<std::array<std::int16_t, kFrameSamples>> frames;
  std::uint32_t talkspurt_id = 0;
  std::uint32_t first_sequence = 0;
  {
    std::lock_guard<std::mutex> lock(capture_mutex_);
    frames.swap(pending_frames_);
    talkspurt_id = talkspurt_id_;
    first_sequence = next_sequence_;
    next_sequence_ += static_cast<std::uint32_t>(frames.size());
  }

  const std::uint32_t dropped = dropped_frames_.exchange(0, std::memory_order_relaxed);
  if (dropped > 0) {
    SPDLOG_WARN("Dropped {} queued voice capture frame(s)", dropped);
  }

  std::vector<EncodedFrame> encoded_frames;
  encoded_frames.reserve(frames.size());
  if (!encoder_) {
    return encoded_frames;
  }

  std::array<unsigned char, kMaxEncodedFrameSize> encoded{};
  std::uint32_t sequence = first_sequence;
  for (const auto& frame : frames) {
    const int encoded_size = opus_encode(encoder_, frame.data(), kFrameSamples, encoded.data(), static_cast<opus_int32>(encoded.size()));
    if (encoded_size <= 0) {
      SPDLOG_WARN("Failed to encode voice frame: {}", opus_strerror(encoded_size));
      ++sequence;
      continue;
    }

    EncodedFrame result;
    result.talkspurt_id = talkspurt_id;
    result.sequence = sequence++;
    result.data.assign(encoded.begin(), encoded.begin() + encoded_size);
    encoded_frames.push_back(std::move(result));
  }
  return encoded_frames;
}

std::vector<std::string> VoiceCapture::GetInputDevices() const {
  std::vector<std::string> result;
  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
  if (!devices) {
    SPDLOG_WARN("Failed to enumerate voice capture devices: {}", SDL_GetError());
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

const std::string& VoiceCapture::GetInputDevice() const {
  return input_device_name_;
}

bool VoiceCapture::SetInputDevice(const std::string& device_name) {
  if (!FindRecordingDevice(device_name).has_value()) {
    return false;
  }
  input_device_name_ = device_name;
  return true;
}

}  // namespace gmp::voice

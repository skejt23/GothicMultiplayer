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

#pragma once

#include <SDL3/SDL_audio.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct OpusEncoder;

namespace gmp::voice {

inline constexpr int kSampleRate = 48000;
inline constexpr int kChannels = 1;
inline constexpr int kFrameSamples = 960;
inline constexpr int kFrameDurationMs = 20;
inline constexpr std::size_t kMaxEncodedFrameSize = 512;

class VoiceCapture {
public:
  struct EncodedFrame {
    std::uint32_t talkspurt_id{0};
    std::uint32_t sequence{0};
    std::vector<std::uint8_t> data;
  };

  VoiceCapture();
  ~VoiceCapture();

  VoiceCapture(const VoiceCapture&) = delete;
  VoiceCapture& operator=(const VoiceCapture&) = delete;

  bool StartCapture();
  void StopCapture();
  bool IsCapturing() const;

  void SetTransmitting(bool transmitting);
  bool IsTransmitting() const;

  std::vector<EncodedFrame> ConsumeEncodedFrames();
  std::vector<std::string> GetInputDevices() const;
  const std::string& GetInputDevice() const;
  bool SetInputDevice(const std::string& device_name);

private:
  static void SDLCALL AudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
  void DrainAudioStream(SDL_AudioStream* stream);
  void AppendSamples(const std::int16_t* samples, std::size_t count);
  bool EnsureEncoder();

  SDL_AudioStream* input_stream_{nullptr};
  OpusEncoder* encoder_{nullptr};
  std::string input_device_name_;

  mutable std::mutex capture_mutex_;
  std::array<std::int16_t, kFrameSamples> partial_frame_{};
  std::size_t partial_frame_size_{0};
  std::deque<std::array<std::int16_t, kFrameSamples>> pending_frames_;

  std::atomic<bool> capturing_{false};
  std::atomic<bool> transmitting_{false};
  std::atomic<std::uint32_t> dropped_frames_{0};
  std::uint32_t talkspurt_id_{0};
  std::uint32_t next_sequence_{0};
};

}  // namespace gmp::voice

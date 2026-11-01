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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gmp::voice {

class VoicePlayback {
public:
  VoicePlayback();
  ~VoicePlayback();

  VoicePlayback(const VoicePlayback&) = delete;
  VoicePlayback& operator=(const VoicePlayback&) = delete;

  bool StartPlayback();
  void StopPlayback();
  bool IsPlaying() const;

  bool PlayVoice(std::uint32_t player_id, std::uint32_t talkspurt_id, std::uint32_t sequence,
                 const std::vector<std::uint8_t>& encoded_data, float gain);
  void RemovePlayer(std::uint32_t player_id);
  void ClearPlayers();

  void SetMasterVolume(float volume);
  float GetMasterVolume() const;
  std::vector<std::string> GetOutputDevices() const;
  const std::string& GetOutputDevice() const;
  bool SetOutputDevice(const std::string& device_name);

private:
  struct SpeakerState;

  static void SDLCALL AudioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
  void MixAudio(SDL_AudioStream* stream, int additional_amount);
  SpeakerState* GetOrCreateSpeaker(std::uint32_t player_id);

  SDL_AudioStream* output_stream_{nullptr};
  std::string output_device_name_;
  mutable std::mutex playback_mutex_;
  std::unordered_map<std::uint32_t, std::unique_ptr<SpeakerState>> speakers_;
  std::vector<float> mix_buffer_;
  std::vector<std::int16_t> output_buffer_;
  float master_volume_{1.0f};
};

}  // namespace gmp::voice

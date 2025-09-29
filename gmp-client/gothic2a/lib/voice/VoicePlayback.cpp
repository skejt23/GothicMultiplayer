#include "VoicePlayback.h"

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <zlib.h>

#include <mutex>

// TODO: Check if works with more than one talking player
// It may be necessary to create one VoicePlayback object per player
std::mutex voicePlaybackBufferMutex;
std::atomic_bool stopPlaybackThread = false;

using namespace std;

VoicePlayback::VoicePlayback() {
  out = nullptr;
}

VoicePlayback::~VoicePlayback() {
  stopPlaybackThread = true;
  if (out) {
    SDL_PauseAudioStreamDevice(out);  // Pause the playback stream.
    SDL_DestroyAudioStream(out);      // This also closes the underlying device.
  }
  ClearBuffers();
}

bool VoicePlayback::StartPlayback() {
  out = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr, nullptr, nullptr);
  if (!out) {
    SPDLOG_ERROR("Failed to open playback device stream, error: {}", SDL_GetError());
    return false;
  }
  // Resume the stream to start playback.
  if (!SDL_ResumeAudioStreamDevice(out)) {
    SPDLOG_ERROR("Failed to resume playback device stream, error: {}", SDL_GetError());
    SDL_DestroyAudioStream(out);
    out = nullptr;
    return false;
  }
  stopPlaybackThread = false;
  loopThread = thread(&VoicePlayback::Loop, this);
  loopThread.detach();
  return true;
}

void VoicePlayback::PlayVoice(char* buffer, size_t size) {
  const lock_guard<mutex> lock(voicePlaybackBufferMutex);
  voiceBuffers.push_back(VoiceBuffer{buffer, size});
}

void VoicePlayback::Loop() {
  do {
    {
      // Use a lock_guard to check if there is any voice data.
      lock_guard<mutex> lock(voicePlaybackBufferMutex);
      if (voiceBuffers.empty()) {
        SDL_Delay(16);
        continue;
      }
    }
    // Pop the first voice buffer from the queue.
    voicePlaybackBufferMutex.lock();
    VoiceBuffer voiceBuffer = voiceBuffers.front();
    voiceBuffers.pop_front();
    voicePlaybackBufferMutex.unlock();

    // Prepare a temporary buffer for decompression.
    int maxSize = 480 * sizeof(float) * 2 * 4096;
    char* rawData = new char[maxSize];
    memset(rawData, 0, maxSize);
    // 'decompressedSize' will be updated with the actual number of bytes produced.
    uLongf decompressedSize = maxSize;
    int result = uncompress((Bytef*)rawData, &decompressedSize, (Bytef*)voiceBuffer.data, voiceBuffer.size);
    if (result != Z_OK) {
      SPDLOG_ERROR("Failed to uncompress voice data, error code: {}", result);
      delete[] rawData;
      delete[] voiceBuffer.data;
      continue;
    }
    // Feed the decompressed audio data into the playback stream.
    if (!SDL_PutAudioStreamData(out, rawData, decompressedSize)) {
      SPDLOG_ERROR("Failed to put audio stream data, error: {}", SDL_GetError());
    }
    delete[] rawData;
    delete[] voiceBuffer.data;
  } while (!stopPlaybackThread);
}

void VoicePlayback::ClearBuffers() {
  const lock_guard<mutex> lock(voicePlaybackBufferMutex);
  while (!voiceBuffers.empty()) {
    auto voiceBuffer = voiceBuffers.front();
    voiceBuffers.pop_front();
    delete[] voiceBuffer.data;
  }
}

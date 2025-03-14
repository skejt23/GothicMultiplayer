#include "VoiceCapture.h"

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>
#include <zlib.h>

#include <mutex>

const int AUDIO_CHANNELS = 2;
std::mutex voiceCaptureBufferMutex;
std::atomic_bool stopCaptureThread = false;

using namespace std;

VoiceCapture::VoiceCapture() {
  // 'in' is now an SDL_AudioStream pointer.
  in = nullptr;
  voiceBufferSize = 0;
  // Allocate a buffer to hold captured audio data.
  voiceBuffer = new char[480 * sizeof(float) * AUDIO_CHANNELS * 4096];
  memset(voiceBuffer, 0, 480 * sizeof(float) * AUDIO_CHANNELS * 4096);
}

VoiceCapture::~VoiceCapture() {
  stopCaptureThread = true;
  // Optionally pause the stream before destruction.
  if (in) {
    SDL_PauseAudioStreamDevice(in);  // Pauses the stream's device.
    SDL_DestroyAudioStream(in);
  }
  delete[] voiceBuffer;
}

vector<string> VoiceCapture::GetInputDevices() {
  vector<string> result;
  int num_devices = 0;
  auto* devices = SDL_GetAudioRecordingDevices(&num_devices);
  if (devices == nullptr) {
    SPDLOG_ERROR("Failed to get input devices, error: {}", SDL_GetError());
    return result;
  }

  for (int i = 0; i < num_devices; i++) {
    result.push_back(SDL_GetAudioDeviceName(devices[i]));
  }
  return result;
}

bool VoiceCapture::StartCapture() {
  // Set up the desired audio format.
  SDL_AudioSpec desired;
  desired.freq = 48000;            // Sample rate (Hz) – adjust as needed.
  desired.format = SDL_AUDIO_F32;  // 32-bit floating point samples.
  desired.channels = AUDIO_CHANNELS;

  in = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &desired, nullptr, nullptr);
  if (!in) {
    SPDLOG_ERROR("Failed to open capture device stream, error: {}", SDL_GetError());
    return false;
  }

  // The device starts paused – resume to begin capturing.
  if (!SDL_ResumeAudioStreamDevice(in)) {
    SPDLOG_ERROR("Failed to resume capture device stream, error: {}", SDL_GetError());
    SDL_DestroyAudioStream(in);
    in = nullptr;
    return false;
  }

  stopCaptureThread = false;
  // Launch the capture loop in a separate thread.
  loopThread = thread(&VoiceCapture::Loop, this);
  loopThread.detach();

  return true;
}

void VoiceCapture::Loop() {
  const int blockSize = 480 * sizeof(float) * AUDIO_CHANNELS;
  Uint8 buf[blockSize];

  while (!stopCaptureThread) {
    int available = SDL_GetAudioStreamAvailable(in);
    if (available > 0) {
      // Read either a full block or whatever is available.
      int toRead = (available < blockSize) ? available : blockSize;
      int retrieved = SDL_GetAudioStreamData(in, buf, toRead);
      if (retrieved < 0) {
        SPDLOG_ERROR("SDL_GetAudioStreamData error: {}", SDL_GetError());
      } else if (retrieved > 0) {
        // Lock the buffer mutex, append the new data.
        lock_guard<mutex> lock(voiceCaptureBufferMutex);
        memcpy(voiceBuffer + voiceBufferSize, buf, retrieved);
        voiceBufferSize += retrieved;
      }
    } else {
      SDL_Delay(16);
    }
  }
}

bool VoiceCapture::GetAndFlushVoiceBuffer(char* buffer, int& size) {
  const lock_guard<mutex> lock(voiceCaptureBufferMutex);
  if (voiceBufferSize == 0) {
    return false;
  }
  uLongf dstSize = 480 * sizeof(float) * AUDIO_CHANNELS * 4096;
  // Compress the captured audio using zlib.
  compress((Bytef*)buffer, &dstSize, (Bytef*)voiceBuffer, voiceBufferSize);
  size = voiceBufferSize;
  memset(voiceBuffer, 0, 480 * sizeof(float) * AUDIO_CHANNELS * 4096);
  voiceBufferSize = 0;
  return true;
}

int VoiceCapture::GetNumberOfChannels() const {
  return AUDIO_CHANNELS;
}

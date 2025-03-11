#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <vector>
#include <string>
#include <thread>

class VoiceCapture
{
public:
  VoiceCapture();
  ~VoiceCapture();

  bool StartCapture();
  bool GetAndFlushVoiceBuffer(char*, int&);
  int GetNumberOfChannels() const;

  std::vector<std::string> GetInputDevices();

private:
  SDL_AudioStream* in;
  char* voiceBuffer;
  size_t voiceBufferSize;
  void Loop();

  std::thread loopThread;
};
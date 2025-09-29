#pragma once

#include <SDL3/SDL.h>
#include <deque>
#include <thread>

class VoicePlayback
{
public:
  VoicePlayback();
  ~VoicePlayback();

  void PlayVoice(char*, size_t);
  bool StartPlayback();

private:
  struct VoiceBuffer
  {
    char* data;
    size_t size;
  };

  std::deque<VoiceBuffer> voiceBuffers;
  std::thread loopThread;

  SDL_AudioStream* out;

  void ClearBuffers();
  void Loop();
};
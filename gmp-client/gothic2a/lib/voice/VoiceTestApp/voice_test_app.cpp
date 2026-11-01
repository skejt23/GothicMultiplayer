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

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <cstdio>
#include <iostream>
#include <memory>

#include "VoiceCapture.h"
#include "VoicePlayback.h"

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::cerr << "Couldn't initialize SDL: " << SDL_GetError() << "\n";
  }
  gmp::voice::VoiceCapture capture;
  std::cout << "Press any key to start recording...\n";
  std::getchar();
  if (!capture.StartCapture()) {
    std::cerr << "Failed to start recording...\n";
    return 1;
  }
  capture.SetTransmitting(true);
  std::cout << "Press any key to stop recording...\n";
  std::getchar();
  auto frames = capture.ConsumeEncodedFrames();
  capture.SetTransmitting(false);
  capture.StopCapture();
  std::cout << "Press any key to play the recording...\n";
  std::getchar();

  gmp::voice::VoicePlayback playback;
  if (!playback.StartPlayback()) {
    std::cerr << "Failed to start playback...\n";
    return 1;
  }
  for (const auto& frame : frames) {
    playback.PlayVoice(1, frame.talkspurt_id, frame.sequence, frame.data, 1.0f);
  }

  std::cout << "Press any key to exit...\n";
  std::getchar();
  return 0;
}

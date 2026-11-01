
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

#ifndef CACTIVEANIID_H
#define CACTIVEANIID_H

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

class CActiveAniID {
private:
  struct ObservedAnimation {
    std::string name;
    float frame = 0.0f;
    int direction = 1;
    bool finite = false;
  };
  std::unordered_map<int, ObservedAnimation> observed_animations;
  const void* observed_hero = nullptr;
  const void* observed_model = nullptr;
  int current_animation = -1;

public:
  struct Transition {
    std::string name;
    bool started;
    std::uint64_t source_time_ms;
  };
  CActiveAniID(void);
  ~CActiveAniID(void);
  static CActiveAniID *GetInstance();
  void AddAni(int id);
  void ValidateStack(void);
  int GetAniID(void);
  void Observe();
  void Reset();
  std::deque<Transition> TakeTransitions();

 private:
  std::deque<Transition> transitions;
};
#endif

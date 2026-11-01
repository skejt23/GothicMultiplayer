
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

/*****************************************************************************
**																			**
**	File name:		CGmpClient/CInterpolatePos.h		   					**
**																			**
**	Created by:		17/12/11	-	skejt23									**
**																			**
**	Description:	Position interpolation	(at least pretends to be :-)) 	**
**																			**
*****************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>

#include "ZenGin/zGothicAPI.h"
#include "gothic2a_player.hpp"
#include "pose_interpolator.h"

class CInterpolatePos {
private:
  using Clock = std::chrono::steady_clock;
  struct Presentation {
    std::uint64_t source_time_ms;
    std::function<void()> apply;
  };
  Gothic2APlayer* InterpolatingPlayer;
  gmp::PoseInterpolator PoseBuffer;
  std::deque<Presentation> Presentations;

public:
  bool IsInterpolating;

public:
  CInterpolatePos(Gothic2APlayer* Player);
  ~CInterpolatePos();
  void DoInterpolate();
  bool IsDistanceSmallerThanRadius(float radius, float bX, float bY, float bZ, float rX, float rY, float rZ);
  bool IsDistanceSmallerThanRadius(float radius, const zVEC3& Pos, const zVEC3& Pos1);
  void StopInterpolation();
  void UpdateInterpolation(float x, float y, float z);
  void UpdatePoseInterpolation(float x, float y, float z, float nx, float ny, float nz,
                               std::uint64_t source_time_ms, bool teleport = false);
  void QueuePresentation(std::uint64_t source_time_ms, std::function<void()> apply);
  double PresentationTime(double local_time_ms) const { return PoseBuffer.RenderTime(local_time_ms); }
};

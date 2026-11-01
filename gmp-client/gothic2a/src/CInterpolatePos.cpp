
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
** ** *	File name:		CGmpClient/CInterpolatePos.cpp
*** *	Created by:		17/12/11	-	skejt23
*** *	Description:	Position interpolation	(at least pretends to be :-))
***
*****************************************************************************/

#include "CInterpolatePos.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "CIngame.h"
#include "config.h"

extern CIngame* global_ingame;

namespace {
double LocalTimeMs() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}  // namespace

CInterpolatePos::CInterpolatePos(Gothic2APlayer* actor) : InterpolatingPlayer(actor), IsInterpolating(false) {
  gmp::PoseInterpolator::Settings settings;
  settings.delay_ms = Config::Instance().GetInterpolationDelayMs();
  PoseBuffer = gmp::PoseInterpolator(settings);
  if (global_ingame) global_ingame->Interpolation.push_back(this);
}

CInterpolatePos::~CInterpolatePos() {
  StopInterpolation();
  if (global_ingame) {
    auto& interpolators = global_ingame->Interpolation;
    interpolators.erase(std::remove(interpolators.begin(), interpolators.end(), this), interpolators.end());
  }
}

void CInterpolatePos::DoInterpolate() {
  if (!IsInterpolating) return;
  if (!InterpolatingPlayer || !InterpolatingPlayer->npc) {
    StopInterpolation();
    return;
  }
  const double now = LocalTimeMs();
  const auto pose = PoseBuffer.Evaluate(now);
  if (pose) {
    InterpolatingPlayer->SetPosition(zVEC3(pose->position[0], pose->position[1], pose->position[2]));
    const zVEC3 position = InterpolatingPlayer->npc->GetPositionWorld();
    InterpolatingPlayer->npc->SetHeadingYWorld(position + zVEC3(pose->direction[0], 0.0f, pose->direction[2]));
  }
  const double render_time = PoseBuffer.RenderTime(now);
  while (!Presentations.empty() && static_cast<double>(Presentations.front().source_time_ms) <= render_time) {
    auto apply = std::move(Presentations.front().apply);
    Presentations.pop_front();
    apply();
    if (!IsInterpolating || !InterpolatingPlayer || !InterpolatingPlayer->npc) return;
  }
}

bool CInterpolatePos::IsDistanceSmallerThanRadius(float radius, float bx, float by, float bz, float rx, float ry, float rz) {
  return std::hypot(rx - bx, ry - by, rz - bz) < radius;
}

bool CInterpolatePos::IsDistanceSmallerThanRadius(float radius, const zVEC3& from, const zVEC3& to) {
  return IsDistanceSmallerThanRadius(radius, from[VX], from[VY], from[VZ], to[VX], to[VY], to[VZ]);
}

void CInterpolatePos::StopInterpolation() {
  IsInterpolating = false;
  PoseBuffer.Reset();
  Presentations.clear();
}

void CInterpolatePos::UpdateInterpolation(float x, float y, float z) {
  const zVEC3 heading = InterpolatingPlayer && InterpolatingPlayer->npc
      ? InterpolatingPlayer->npc->GetAtVectorWorld() : zVEC3(0.0f, 0.0f, 1.0f);
  UpdatePoseInterpolation(x, y, z, heading[VX], heading[VY], heading[VZ], static_cast<std::uint64_t>(LocalTimeMs()));
}

void CInterpolatePos::UpdatePoseInterpolation(float x, float y, float z, float nx, float ny, float nz,
                                             std::uint64_t source_time_ms, bool teleport) {
  if (!InterpolatingPlayer || !InterpolatingPlayer->npc) {
    StopInterpolation();
    return;
  }
  if (teleport) {
    StopInterpolation();
    InterpolatingPlayer->SetPosition(zVEC3(x, y, z));
    const zVEC3 position = InterpolatingPlayer->npc->GetPositionWorld();
    InterpolatingPlayer->npc->SetHeadingYWorld(position + zVEC3(nx, 0.0f, nz));
  }
  if (PoseBuffer.Push({{x, y, z}, {nx, ny, nz}}, source_time_ms, LocalTimeMs(), teleport)) {
    IsInterpolating = true;
  }
}

void CInterpolatePos::QueuePresentation(std::uint64_t source_time_ms, std::function<void()> apply) {
  if (!apply) return;
  // The action stream is reliable and ordered, while poses may overtake it.
  // Sort by source time so a delayed older action cannot block a newer one.
  auto insertion = std::upper_bound(Presentations.begin(), Presentations.end(), source_time_ms,
      [](std::uint64_t time, const Presentation& presentation) { return time < presentation.source_time_ms; });
  Presentations.insert(insertion, Presentation{source_time_ms, std::move(apply)});
  // Bounded presentation lifetime; never replay stale effects accumulated during
  // a paused world. Transport action IDs provide deduplication before this queue.
  while (Presentations.size() > 128) Presentations.pop_front();
  IsInterpolating = true;
}

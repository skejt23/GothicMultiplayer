/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "set_watch_position_state.hpp"

#include <spdlog/spdlog.h>

#include "CWatch.h"
#include "keyboard.h"
#include "menu/states/online_options_state.hpp"

namespace menu {
namespace states {

SetWatchPositionState::SetWatchPositionState(MenuContext& context) : context_(context), shouldReturnToOptions_(false) {
}

void SetWatchPositionState::OnEnter() {
  SPDLOG_INFO("Entering watch position state");
  // Hide logo when positioning watch
  if (context_.logoView) {
    context_.screen->RemoveItem(context_.logoView);
  }
}

void SetWatchPositionState::OnExit() {
  SPDLOG_INFO("Exiting watch position state");
  SaveWatchPosition();
  // Restore logo
  if (context_.logoView) {
    context_.screen->InsertItem(context_.logoView, false);
  }
}

StateResult SetWatchPositionState::Update() {
  RenderWatchAndInstructions();
  HandleInput();
  return StateResult::Continue;
}

MenuState* SetWatchPositionState::CheckTransition() {
  if (shouldReturnToOptions_) {
    return new OnlineOptionsState(context_);
  }
  return nullptr;
}

void SetWatchPositionState::RenderWatchAndInstructions() {
  // Display the watch at current position
  if (context_.config.watch) {
    CWatch::GetInstance()->PrintWatch();
  }

  // Display instructions (use arrow keys to position, ESC to return)
  context_.screen->SetFont("FONT_OLD_20_WHITE.TGA");
  context_.screen->SetFontColor({255, 255, 255});
  context_.screen->Print(200, 200, "Use arrow keys to position the watch");
  context_.screen->Print(200, 400, "Press ESC to return");
}

void SetWatchPositionState::HandleInput() {
  // ESC to return to options
  if (context_.input->KeyPressed(KEY_ESCAPE)) {
    context_.input->ClearKeyBuffer();
    shouldReturnToOptions_ = true;
    return;
  }

  // Arrow keys to adjust position (16 pixel increments)
  if (context_.input->KeyPressed(KEY_UP)) {
    context_.config.WatchPosY -= 16;
  }
  if (context_.input->KeyPressed(KEY_DOWN)) {
    context_.config.WatchPosY += 16;
  }
  if (context_.input->KeyPressed(KEY_LEFT)) {
    context_.config.WatchPosX -= 16;
  }
  if (context_.input->KeyPressed(KEY_RIGHT)) {
    context_.config.WatchPosX += 16;
  }
}

void SetWatchPositionState::SaveWatchPosition() {
  context_.config.SaveConfigToFile();
}

}  // namespace states
}  // namespace menu

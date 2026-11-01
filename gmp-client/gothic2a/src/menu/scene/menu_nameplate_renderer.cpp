/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

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

#include "menu/scene/menu_nameplate_renderer.h"

#include "menu/scene/menu_camera.h"
#include "menu/scene/menu_npc.h"

namespace menu {

void MenuNameplateRenderer::Render(const MenuNpc& npc, const MenuCamera& camera) {
  if (!screen || !zrenderer || !npc.HasVisibleNameplate()) {
    return;
  }
  float x = 0.0f;
  float y = 0.0f;
  if (!camera.Project(npc.GetNameplatePosition(), x, y)) {
    return;
  }
  auto color = npc.GetNameplateColor();
  color.alpha = npc.GetNameplateAlpha(camera.GetAnchor()->GetPositionWorld());
  if (color.alpha == 0) {
    return;
  }
  const zSTRING text(npc.GetNameplate().c_str());
  const zSTRING font_name(npc.GetNameplateFont().c_str());
  auto* previous_font = screen->GetFont();
  const zCOLOR previous_color = screen->fontColor;
  screen->SetFont(font_name);
  screen->SetFontColor(color);
  if (auto* font = screen->GetFont()) {
    // Project and PrintChars both use pixels. A detached
    // VIEW_ITEM queues Print text, while Render only draws its child views.
    // Draw immediately so neither view state nor queued text outlives a frame.
    const int width = font->GetFontX(text);
    const int height = font->GetFontY();
    const int left = static_cast<int>(x) - width / 2;
    const int top = static_cast<int>(y) - height;
    if (left >= screen->pposx && top >= screen->pposy && left + width <= screen->pposx + screen->psizex &&
        top + height <= screen->pposy + screen->psizey) {
      // PrintChars preserves an incoming additive blend mode. Use ordinary
      // alpha blending for the fade, then restore it for the following menu UI.
      const auto previous_blend = zrenderer->GetAlphaBlendFunc();
      zrenderer->SetAlphaBlendFunc(zRND_ALPHA_FUNC_BLEND);
      screen->PrintChars(left, top, text);
      zrenderer->SetAlphaBlendFunc(previous_blend);
    }
  }
  screen->SetFont(previous_font);
  screen->SetFontColor(previous_color);
}

}  // namespace menu

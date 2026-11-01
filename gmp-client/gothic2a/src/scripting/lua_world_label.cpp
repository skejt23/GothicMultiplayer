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

#include "lua_world_label.h"

#include <algorithm>
#include <cmath>

#include "Shared/lua_runtime/bind_helpers.h"
#include "lua_helpers.h"

using namespace Gothic_II_Addon;

namespace gmp::gothic {
namespace {
constexpr float kDefaultWorldLabelDistance = 1000.0f;

float DistanceSquared(const zVEC3& left, const zVEC3& right) {
  const float dx = left[VX] - right[VX];
  const float dy = left[VY] - right[VY];
  const float dz = left[VZ] - right[VZ];
  return dx * dx + dy * dy + dz * dz;
}
}  // namespace

class LuaWorldLabelView : public zCView {
public:
  explicit LuaWorldLabelView(LuaWorldLabel& owner) : zCView(0, 0, 8192, 8192), owner_(owner) {
  }

  void Blit() override {
    owner_.Blit();
  }

private:
  LuaWorldLabel& owner_;
};

std::unordered_set<LuaWorldLabel*> LuaWorldLabel::active_labels_;

/* luagmp (class)
 *
 * 3D text drawing helper for rendering world-space text on screen.
 *
 * @version  0.3.0
 * @name     WorldLabel
 * @side     client
 * @category UI
 *
 */

/* luagmp (constructor)
 *
 * Creates a new WorldLabel object with default settings.
 *
 */
LuaWorldLabel::LuaWorldLabel()
    : view_(nullptr),
      font_name_("FONT_DEFAULT.TGA"),
      position_(0.0f, 0.0f, 0.0f),
      color_(255, 255, 255, 255),
      visible_(true),
      attached_to_screen_(false),
      max_render_distance_(kDefaultWorldLabelDistance) {
  Initialize();
}

/* luagmp (constructor)
 *
 * Creates a new WorldLabel object at the provided world position.
 *
 * @param    (number) x    Initial X world position.
 * @param    (number) y    Initial Y world position.
 * @param    (number) z    Initial Z world position.
 *
 */
LuaWorldLabel::LuaWorldLabel(float x, float y, float z)
    : view_(nullptr),
      font_name_("FONT_DEFAULT.TGA"),
      position_(x, y, z),
      color_(255, 255, 255, 255),
      visible_(true),
      attached_to_screen_(false),
      max_render_distance_(kDefaultWorldLabelDistance) {
  Initialize();
}

/* luagmp (constructor)
 *
 * Creates a new WorldLabel object at the provided world position with initial text.
 *
 * @param    (number) x       Initial X world position.
 * @param    (number) y       Initial Y world position.
 * @param    (number) z       Initial Z world position.
 * @param    (string) text    Initial text line.
 *
 */
LuaWorldLabel::LuaWorldLabel(float x, float y, float z, const std::string& text)
    : view_(nullptr),
      lines_{},
      font_name_("FONT_DEFAULT.TGA"),
      position_(x, y, z),
      color_(255, 255, 255, 255),
      visible_(true),
      attached_to_screen_(false),
      max_render_distance_(kDefaultWorldLabelDistance) {
  setText(text);
  Initialize();
}

LuaWorldLabel::~LuaWorldLabel() {
  if (screen && view_ && attached_to_screen_) {
    screen->RemoveItem(view_);
    attached_to_screen_ = false;
  }

  delete view_;
  view_ = nullptr;
  active_labels_.erase(this);
}

void LuaWorldLabel::Initialize() {
  active_labels_.insert(this);
  view_ = new LuaWorldLabelView(*this);
  if (view_) {
    view_->SetFont(font_name_.c_str());
    view_->SetFontColor(color_);
  }
}

void LuaWorldLabel::CleanupViews() {
  if (!screen) {
    return;
  }

  for (auto* label : active_labels_) {
    if (label && label->view_ && label->attached_to_screen_) {
      screen->RemoveItem(label->view_);
      label->attached_to_screen_ = false;
    }
  }
}

void LuaWorldLabel::RenderActiveLabels() {
  auto labels = active_labels_;
  for (auto* label : labels) {
    if (label) {
      label->Blit();
    }
  }
}

/* luagmp (method)
 *
 * Sets the world position used to project this text to screen space.
 *
 * @name     setPosition
 * @param    (number) x    X world position.
 * @param    (number) y    Y world position.
 * @param    (number) z    Z world position.
 *
 */
void LuaWorldLabel::setPosition(float x, float y, float z) {
  position_ = zVEC3(x, y, z);
}

/// Assigns the position property using the same value format as its getter.
void LuaWorldLabel::setPositionValue(sol::object value) {
  float x = position_[VX];
  float y = position_[VY];
  float z = position_[VZ];
  if (lua_helpers::ReadVec3(value, x, y, z)) {
    setPosition(x, y, z);
  }
}

/* luagmp (property)
 *
 * World position {x, y, z}, projected onto the screen while rendering.
 *
 * @name     position
 * @return   ({x, y, z})
 *
 */
/* luagmp (method)
 *
 * Returns the current world position.
 *
 * @name     getPosition
 * @return   ({x, y, z})   Table containing x,y,z world position.
 *
 */
sol::table LuaWorldLabel::getPosition(sol::this_state s) const {
  return ::lua::bind_helpers::MakeVec3Table(sol::state_view(s), position_[VX], position_[VY], position_[VZ]);
}

/* luagmp (method)
 *
 * Appends a text line to the WorldLabel object.
 *
 * @name     appendLine
 * @param    (string) text    Text line to append.
 *
 */
void LuaWorldLabel::appendLine(const std::string& text) {
  lines_.push_back(text);
}

/* luagmp (method)
 *
 * Removes all text lines.
 *
 * @name     clearLines
 *
 */
void LuaWorldLabel::clearLines() {
  lines_.clear();
}

/* luagmp (property)
 * Dense one-based array of text lines. See `getLines` and `setLines`.
 * @name lines
 * @return (table)
 */
/* luagmp (method)
 *
 * Returns all text lines.
 *
 * @name     getLines
 * @return   (table)    Array-like table of text lines.
 *
 */
sol::table LuaWorldLabel::getLines(sol::this_state s) const {
  sol::state_view lua(s);
  sol::table result = lua.create_table();
  int index = 1;
  for (const auto& line : lines_) {
    result[index++] = line;
  }
  return result;
}

/* luagmp (method)
 *
 * Sets the font used for rendering.
 *
 * @name     setFont
 * @param    (string) font    Font file name.
 *
 */
void LuaWorldLabel::setFont(const std::string& font_name) {
  font_name_ = font_name;
  if (view_) {
    view_->SetFont(font_name_.c_str());
  }
}

/* luagmp (property)
 *
 * Represents the font identifier used for rendering.
 *
 * @name     font
 * @return   (string)
 *
 */
/* luagmp (method)
 *
 * Returns the current font file name.
 *
 * @name     getFont
 * @return   (string)    Font file name.
 *
 */
std::string LuaWorldLabel::getFont() const {
  return font_name_;
}

/* luagmp (method)
 *
 * Sets the text color.
 *
 * @name     setColor
 * @param    (number) r    Red color component.
 * @param    (number) g    Green color component.
 * @param    (number) b    Blue color component.
 *
 */
void LuaWorldLabel::setColor(int r, int g, int b) {
  color_.SetRGB(r, g, b);
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/// Assigns the color property using the same value format as its getter.
void LuaWorldLabel::setColorValue(sol::object value) {
  int r = color_.r;
  int g = color_.g;
  int b = color_.b;
  int a = color_.alpha;
  if (lua_helpers::ReadColor(value, r, g, b, a)) {
    setColor(r, g, b);
    setAlpha(a);
  }
}

/* luagmp (property)
 *
 * Represents the WorldLabel color.
 *
 * @name     color
 * @return   ({r, g, b, a})
 *
 */
/* luagmp (method)
 *
 * Returns the text color.
 *
 * @name     getColor
 * @return   ({r, g, b, a})    Table containing color in RGBA model.
 *
 */
sol::table LuaWorldLabel::getColor(sol::this_state s) const {
  sol::state_view lua(s);
  sol::table color = lua.create_table();
  color["r"] = color_.r;
  color["g"] = color_.g;
  color["b"] = color_.b;
  color["a"] = color_.alpha;
  return color;
}

/* luagmp (method)
 *
 * Sets the text alpha.
 *
 * @name     setAlpha
 * @param    (number) alpha    Opacity value (0-255).
 *
 */
void LuaWorldLabel::setAlpha(int alpha) {
  color_.alpha = ::lua::bind_helpers::ClampByte(alpha);
  if (view_) {
    view_->SetFontColor(color_);
  }
}

/* luagmp (property)
 * Opacity from 0 (transparent) to 255 (opaque).
 *
 * @name     alpha
 * @return   (number)
 *
 */
/* luagmp (method)
 *
 * Returns the current alpha.
 *
 * @name     getAlpha
 * @return   (number)    Opacity value (0-255).
 *
 */
int LuaWorldLabel::getAlpha() const {
  return color_.alpha;
}

/* luagmp (method)
 *
 * Sets whether the WorldLabel object should `render`.
 *
 * @name     setVisible
 * @param    (boolean) visible    True to render, false to hide.
 *
 */
void LuaWorldLabel::setVisible(bool visible) {
  visible_ = visible;
}

/* luagmp (property)
 *
 * Represents whether the WorldLabel object is rendered.
 *
 * @name     visible
 * @return   (boolean)
 *
 */
/* luagmp (method)
 *
 * Returns whether this WorldLabel object is visible.
 *
 * @name     getVisible
 * @return   (boolean)    True if visible.
 *
 */
bool LuaWorldLabel::getVisible() const {
  return visible_;
}

/* luagmp (method)
 *
 * Sets the max render distance from the local player.
 *
 * @name     setMaxRenderDistance
 * @param    (number) distance    Max distance in world units. Zero disables distance culling.
 *
 */
void LuaWorldLabel::setMaxRenderDistance(float distance) {
  max_render_distance_ = std::max(0.0f, distance);
}

/* luagmp (property)
 * Maximum distance from the local hero in world units; this is independent of the camera.
 * @name maxRenderDistance
 * @return (number)
 */
/* luagmp (method)
 *
 * Returns the max render distance.
 *
 * @name     getMaxRenderDistance
 * @return   (number)    Max distance in world units.
 *
 */
float LuaWorldLabel::getMaxRenderDistance() const {
  return max_render_distance_;
}

/* luagmp (method)
 *
 * Moves the WorldLabel view to the front of the screen view stack.
 *
 * @name     bringToFront
 *
 */
void LuaWorldLabel::bringToFront() {
  if (view_ && attached_to_screen_) {
    view_->Top();
  }
}

/* luagmp (method)
 *
 * Forces immediate rendering of this WorldLabel object.
 *
 * @name     render
 *
 */
void LuaWorldLabel::render() {
  Blit();
}

void LuaWorldLabel::Blit() {
  if (!visible_ || lines_.empty() || !screen || !zCCamera::activeCam) {
    return;
  }

  if (max_render_distance_ > 0.0f && player) {
    const float max_distance_sq = max_render_distance_ * max_render_distance_;
    if (DistanceSquared(position_, player->GetPositionWorld()) > max_distance_sq) {
      return;
    }
  }

  zCCamera* camera = zCCamera::activeCam;
  camera->Activate();
  zVEC3 projected = camera->camMatrix * position_;
  if (projected[VZ] <= 0.0f) {
    return;
  }

  float center_x = 0.0f;
  float center_y = 0.0f;
  camera->Project(&projected, center_x, center_y);

  zCView* screen_view = screen;
  zCFont* previous_font = screen_view->GetFont();
  const zCOLOR previous_font_color = screen_view->fontColor;

  screen_view->SetFont(font_name_.c_str());
  screen_view->SetFontColor(color_);

  const int center_x_virtual = screen_view->anx(static_cast<int>(center_x));
  const int center_y_virtual = screen_view->any(static_cast<int>(center_y));
  const int line_height = screen_view->FontY();

  for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
    zSTRING line(lines_[i].c_str());
    const int line_width = screen_view->FontSize(line);
    screen_view->Print(center_x_virtual - line_width / 2, center_y_virtual + i * line_height, line);
  }

  screen_view->SetFont(previous_font);
  screen_view->SetFontColor(previous_font_color);
}

/* luagmp (method)
 * Atomically replaces the label lines. The input must be a dense one-based string array.
 * @name setLines
 * @param (table) lines Array of text lines.
 */
void LuaWorldLabel::setLines(sol::table lines) {
  std::vector<std::string> replacement;
  const auto count = lines.size();
  replacement.reserve(count);
  for (std::size_t index = 1; index <= count; ++index) {
    sol::object line = lines[index];
    if (line.get_type() != sol::type::string)
      throw sol::error("lines must be a dense array of strings starting at 1");
    replacement.push_back(line.as<std::string>());
  }
  std::size_t entries = 0;
  for (const auto& entry : lines) {
    (void)entry;
    ++entries;
  }
  if (entries != count)
    throw sol::error("lines must be a dense array of strings starting at 1");
  lines_ = std::move(replacement);
}

/* luagmp (property)
 * Complete text as a newline-separated string. Reading and writing use the same representation.
 * @name text
 * @return (string)
 */
/* luagmp (method)
 * Returns all lines joined by newline characters.
 * @name getText
 * @return (string) Complete label text.
 */
std::string LuaWorldLabel::getText() const {
  std::string text;
  for (std::size_t index = 0; index < lines_.size(); ++index) {
    if (index != 0)
      text += '\n';
    text += lines_[index];
  }
  return text;
}

/* luagmp (method)
 * Replaces all lines by splitting text on newline characters.
 * @name setText
 * @param (string) text Complete label text.
 */
void LuaWorldLabel::setText(const std::string& text) {
  std::vector<std::string> replacement;
  std::size_t first = 0;
  for (;;) {
    const auto next = text.find('\n', first);
    replacement.push_back(text.substr(first, next == std::string::npos ? next : next - first));
    if (next == std::string::npos)
      break;
    first = next + 1;
  }
  lines_ = std::move(replacement);
}

/* luagmp (method)
 * Removes one line by its one-based index.
 * @name removeLine
 * @param (number) index Line index starting at 1.
 * @return (boolean) False if out of range.
 */
bool LuaWorldLabel::removeLine(int index) {
  if (index < 1 || static_cast<std::size_t>(index) > lines_.size())
    return false;
  lines_.erase(lines_.begin() + index - 1);
  return true;
}

/* luagmp (method)
 * Replaces one existing line by its one-based index.
 * @name setLine
 * @param (number) index Line index starting at 1.
 * @param (string) text Replacement text.
 * @return (boolean) False if out of range.
 */
bool LuaWorldLabel::setLine(int index, const std::string& text) {
  if (index < 1 || static_cast<std::size_t>(index) > lines_.size())
    return false;
  lines_[index - 1] = text;
  return true;
}

/* luagmp (method)
 * Returns one line by its one-based index.
 * @name getLine
 * @param (number) index Line index starting at 1.
 * @return (string|nil) Text or nil if out of range.
 */
sol::object LuaWorldLabel::getLine(int index, sol::this_state s) const {
  sol::state_view lua(s);
  if (index < 1 || static_cast<std::size_t>(index) > lines_.size())
    return sol::nil;
  return sol::make_object(lua, lines_[index - 1]);
}

void BindWorldLabel(sol::state& lua) {
  sol::usertype<LuaWorldLabel> label_type = lua.new_usertype<LuaWorldLabel>(
      "WorldLabel", sol::constructors<LuaWorldLabel(), LuaWorldLabel(float, float, float), LuaWorldLabel(float, float, float, const std::string&)>());

  label_type["setPosition"] = &LuaWorldLabel::setPosition;
  label_type["getPosition"] = &LuaWorldLabel::getPosition;

  label_type["getText"] = &LuaWorldLabel::getText;
  label_type["setText"] = &LuaWorldLabel::setText;

  label_type["setFont"] = &LuaWorldLabel::setFont;
  label_type["getFont"] = &LuaWorldLabel::getFont;
  label_type["setColor"] = &LuaWorldLabel::setColor;
  label_type["getColor"] = &LuaWorldLabel::getColor;
  label_type["setAlpha"] = &LuaWorldLabel::setAlpha;
  label_type["getAlpha"] = &LuaWorldLabel::getAlpha;
  label_type["setVisible"] = &LuaWorldLabel::setVisible;
  label_type["getVisible"] = &LuaWorldLabel::getVisible;

  label_type["render"] = &LuaWorldLabel::render;

  label_type["position"] = sol::property(&LuaWorldLabel::getPosition, &LuaWorldLabel::setPositionValue);
  label_type["text"] = sol::property(&LuaWorldLabel::getText, &LuaWorldLabel::setText);
  label_type["font"] = sol::property(&LuaWorldLabel::getFont, &LuaWorldLabel::setFont);
  label_type["color"] = sol::property(&LuaWorldLabel::getColor, &LuaWorldLabel::setColorValue);
  label_type["alpha"] = sol::property(&LuaWorldLabel::getAlpha, &LuaWorldLabel::setAlpha);
  label_type["visible"] = sol::property(&LuaWorldLabel::getVisible, &LuaWorldLabel::setVisible);
  label_type["lines"] = sol::property(&LuaWorldLabel::getLines, &LuaWorldLabel::setLines);
  label_type["getLines"] = &LuaWorldLabel::getLines;
  label_type["setLines"] = &LuaWorldLabel::setLines;
  label_type["appendLine"] = &LuaWorldLabel::appendLine;
  label_type["removeLine"] = &LuaWorldLabel::removeLine;
  label_type["setLine"] = &LuaWorldLabel::setLine;
  label_type["getLine"] = &LuaWorldLabel::getLine;
  label_type["clearLines"] = &LuaWorldLabel::clearLines;
  label_type["getMaxRenderDistance"] = &LuaWorldLabel::getMaxRenderDistance;
  label_type["setMaxRenderDistance"] = &LuaWorldLabel::setMaxRenderDistance;
  label_type["maxRenderDistance"] = sol::property(&LuaWorldLabel::getMaxRenderDistance, &LuaWorldLabel::setMaxRenderDistance);
  label_type["bringToFront"] = &LuaWorldLabel::bringToFront;
}

}  // namespace gmp::gothic

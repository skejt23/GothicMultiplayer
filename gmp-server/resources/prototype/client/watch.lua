local Watch = {
  x = 7000,
  y = 2500,
  lineHeight = 200,
  font = 'FONT_DEFAULT.TGA',
  visible = true,
  lines = {}
}

local function makeLine(index, text)
  local draw = Label.new(Watch.x, Watch.y + (index - 1) * Watch.lineHeight, text)
  draw:setFont(Watch.font)
  draw:setColor(255, 255, 255)
  draw:setVisible(Watch.visible)
  return draw
end

local function formatRealTime()
  local time = getRealTime()
  return string.format('%02d:%02d:%02d', time.hour, time.minute, time.second)
end

local function formatGameTime()
  local time = getTime()
  return string.format('%02d:%02d', time.hour, time.minute)
end

Watch.lines = {
  makeLine(1, 'Real Time:'),
  makeLine(2, ''),
  makeLine(3, 'Game Time:'),
  makeLine(4, '')
}

function Watch:setVisible(visible)
  self.visible = visible and true or false
  for _, line in ipairs(self.lines) do
    line:setVisible(self.visible)
  end
end

function Watch:update()
  if not self.visible then
    return
  end

  self.lines[2]:setText(formatRealTime())
  self.lines[4]:setText(formatGameTime())
end

addEventHandler('onRender', function()
  Watch:update()
end)

addEventHandler('onExit', function()
  Watch:setVisible(false)
end)

function onResourceStop()
  Watch:setVisible(false)
end

Watch:update()

return Watch

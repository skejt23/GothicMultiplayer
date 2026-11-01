local Net = {
  x = 3000,
  y = 1000,
  lineHeight = 200,
  font = 'Font_Old_10_White_Hi.TGA',
  visible = false,
  lines = {}
}

local function formatInteger(value)
  return string.format('%.0f', tonumber(value) or 0)
end

local function formatPercent(value)
  value = tonumber(value)
  if not value or value < 0 then return 'unavailable' end
  return string.format('%.2f%%', value * 100)
end

local function makeLine(index, text, r, g, b)
  local draw = Label.new(Net.x, Net.y + (index - 1) * Net.lineHeight, text)
  draw:setFont(Net.font)
  draw:setColor(r, g, b)
  draw:setVisible(false)
  return draw
end

Net.lines = {
  makeLine(1, 'Network', 243, 8, 188),
  makeLine(2, '', 255, 255, 255),
  makeLine(3, '', 255, 255, 255),
  makeLine(4, '', 255, 255, 255),
  makeLine(5, '', 255, 255, 255),
  makeLine(6, '', 255, 255, 255),
  makeLine(7, '', 255, 255, 255),
  makeLine(8, '', 255, 255, 255),
  makeLine(9, '', 255, 255, 255),
  makeLine(10, '', 255, 255, 255),
  makeLine(11, '', 255, 255, 255),
  makeLine(12, '', 255, 255, 255),
  makeLine(13, '', 255, 255, 255),
  makeLine(14, '', 255, 255, 255)
}

function Net:setVisible(visible)
  self.visible = visible and true or false
  for _, line in ipairs(self.lines) do
    line:setVisible(self.visible)
  end
end

function Net:update()
  local stats = getNetworkStats()

  self.lines[2]:setText('FPS: ' .. formatInteger(getFpsRate()) .. ' | Ping: ' .. (stats.available and formatInteger(stats.pingMs) .. ' ms' or 'unavailable'))
  self.lines[3]:setText('Local quality: ' .. formatPercent(stats.localQuality))
  self.lines[4]:setText('Remote quality: ' .. formatPercent(stats.remoteQuality))
  self.lines[5]:setText('Queued reliable bytes: ' .. formatInteger(stats.pendingReliableBytes))
  self.lines[6]:setText('Queued unreliable bytes: ' .. formatInteger(stats.pendingUnreliableBytes))
  self.lines[7]:setText('Unacknowledged reliable bytes: ' .. formatInteger(stats.unacknowledgedReliableBytes))
  self.lines[8]:setText('Send rate bytes: ' .. formatInteger(stats.sendRateBytesPerSecond) .. '/s')
  self.lines[9]:setText('Control queue time: ' .. formatInteger(stats.controlQueueTimeUsec) .. 'us')
  self.lines[10]:setText('Realtime queue time: ' .. formatInteger(stats.realtimeQueueTimeUsec) .. 'us')
  self.lines[11]:setText('Incoming bytes: ' .. formatInteger(stats.incomingBytesPerSecond) .. '/s')
  self.lines[12]:setText('Outgoing bytes: ' .. formatInteger(stats.outgoingBytesPerSecond) .. '/s')
  self.lines[13]:setText('Incoming packets: ' .. formatInteger(stats.incomingPacketsPerSecond) .. '/s')
  self.lines[14]:setText('Outgoing packets: ' .. formatInteger(stats.outgoingPacketsPerSecond) .. '/s')
end

addEventHandler('onRender', function()
  if Net.visible then
    Net:update()
  end
end)

addEventHandler('onKeyDown', function(key)
  if key == KEY_F6 and not isConsoleOpen() then
    Net:update()
    Net:setVisible(true)
  end
end)

addEventHandler('onKeyUp', function(key)
  if key == KEY_F6 then
    Net:setVisible(false)
  end
end)

addEventHandler('onExit', function()
  Net:setVisible(false)
end)

return Net

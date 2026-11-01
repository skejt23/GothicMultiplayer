local language = getLanguageIndex()
local fontPrefix = 'CP1252_'
if language == LANG_PL or language == LANG_CZ or language == LANG_SK or language == LANG_ISV or language == LANG_HU or language == LANG_SZL then
  fontPrefix = 'CP1250_'
elseif language == LANG_RU or language == LANG_UA then
  fontPrefix = 'CP1251_'
elseif language == LANG_TR then
  fontPrefix = 'CP1254_'
end

local PlayerList = {
  x = 2700,
  y = 800,
  backgroundX = 2500,
  backgroundY = 600,
  backgroundWidth = 3500,
  backgroundHeight = 6000,
  lineHeight = 200,
  maxVisibleRows = 30,
  font = fontPrefix .. 'Font_Old_10_White_Hi.TGA',
  visible = false,
  beginIndex = 0,
  rows = {}
}

local background = Sprite.new(
  PlayerList.backgroundX,
  PlayerList.backgroundY,
  PlayerList.backgroundWidth,
  PlayerList.backgroundHeight,
  'MENU_INGAME.TGA'
)

local function makeDraw(x, y, text, r, g, b)
  local draw = Label.new(x, y, text or '')
  draw:setFont(PlayerList.font)
  draw:setColor(r, g, b)
  draw:setVisible(false)
  return draw
end

local headers = {
  makeDraw(PlayerList.x, PlayerList.y, 'ID', 243, 8, 188),
  makeDraw(PlayerList.x + 300, PlayerList.y, 'Players', 243, 8, 188),
  makeDraw(PlayerList.x + 2800, PlayerList.y, 'Ping', 243, 8, 188)
}

local noPlayers = makeDraw(PlayerList.x, PlayerList.y + PlayerList.lineHeight, 'No Players.', 255, 250, 200)

local function clampByte(value)
  value = tonumber(value) or 255
  if value < 0 then
    return 0
  end
  if value > 255 then
    return 255
  end
  return math.floor(value)
end

local function setDrawsVisible(draws, visible)
  for _, draw in ipairs(draws) do
    draw:setVisible(visible)
  end
end

local function hideRows()
  for _, row in ipairs(PlayerList.rows) do
    row.arrow:setVisible(false)
    row.id:setVisible(false)
    row.name:setVisible(false)
    row.ping:setVisible(false)
  end
end

local function getColor(playerId)
  local color = getPlayerNameColor(playerId)
  if color then
    return clampByte(color.r), clampByte(color.g), clampByte(color.b)
  end
  return 255, 255, 255
end

local function getName(playerId)
  local name = getPlayerName(playerId)
  if name == nil or name == '' then
    return tostring(playerId)
  end
  return string.sub(name, 1, 20)
end

local function getPlayers()
  local players = {}
  local onlinePlayers = getOnlinePlayers()

  if onlinePlayers == nil then
    return players
  end

  for _, playerId in ipairs(onlinePlayers) do
    local name = getName(playerId)
    if name ~= nil then
      table.insert(players, {
        id = playerId,
        name = name
      })
    end
  end

  table.sort(players, function(left, right)
    return left.id < right.id
  end)

  return players
end

local function isLocalPlayer(playerId)
  return heroId ~= nil and tonumber(playerId) == tonumber(heroId)
end

local function getPingText(playerId)
  local ping = tonumber(getPlayerPing(playerId))
  if ping == nil or ping < 0 then
    return '-'
  end

  return tostring(math.floor(ping))
end

for index = 1, PlayerList.maxVisibleRows do
  local rowY = PlayerList.y + index * PlayerList.lineHeight
  PlayerList.rows[index] = {
    arrow = makeDraw(PlayerList.x - 250, rowY, '->', 255, 255, 255),
    id = makeDraw(PlayerList.x, rowY, '', 255, 255, 255),
    name = makeDraw(PlayerList.x + 300, rowY, '', 255, 255, 255),
    ping = makeDraw(PlayerList.x + 2800, rowY, '', 255, 255, 255)
  }
end

background:setVisible(false)

function PlayerList:setVisible(visible)
  visible = visible and true or false
  if self.visible == visible then
    return
  end

  self.visible = visible
  background:setVisible(visible)
  setDrawsVisible(headers, visible)
  noPlayers:setVisible(false)

  if visible then
    self.beginIndex = 0
    disableControls(true)
    self:update()
  else
    hideRows()
    disableControls(false)
  end
end

function PlayerList:scroll(delta)
  local players = getPlayers()
  local maxScrollIndex = math.max(#players - self.maxVisibleRows, 0)

  self.beginIndex = math.max(0, math.min(maxScrollIndex, self.beginIndex + delta))
  self:update(players)
end

function PlayerList:update(players)
  if not self.visible then
    return
  end

  disableControls(true)

  players = players or getPlayers()
  local maxScrollIndex = math.max(#players - self.maxVisibleRows, 0)
  self.beginIndex = math.max(0, math.min(maxScrollIndex, self.beginIndex))

  hideRows()
  noPlayers:setVisible(#players == 0)

  if #players == 0 then
    return
  end

  local visibleRows = math.min(#players - self.beginIndex, self.maxVisibleRows)
  for rowIndex = 1, visibleRows do
    local playerData = players[self.beginIndex + rowIndex]
    local row = self.rows[rowIndex]
    local r, g, b = getColor(playerData.id)

    row.id:setText(tostring(playerData.id))
    row.name:setText(playerData.name)
    row.ping:setText(getPingText(playerData.id))

    row.id:setColor(r, g, b)
    row.name:setColor(r, g, b)
    row.ping:setColor(r, g, b)
    row.arrow:setColor(r, g, b)

    row.arrow:setVisible(isLocalPlayer(playerData.id))
    row.id:setVisible(true)
    row.name:setVisible(true)
    row.ping:setVisible(true)
  end
end

addEventHandler('onRender', function()
  PlayerList:update()
end)

addEventHandler('onKeyDown', function(key)
  if key == KEY_F1 and not isConsoleOpen() then
    if not PlayerList.visible and chatInputIsOpen() then
      return
    end

    PlayerList:setVisible(not PlayerList.visible)
    return
  end

  if not PlayerList.visible then
    return
  end

  if key == KEY_ESCAPE then
    PlayerList:setVisible(false)
  elseif key == KEY_UP then
    PlayerList:scroll(-1)
  elseif key == KEY_DOWN then
    PlayerList:scroll(1)
  elseif key == KEY_PRIOR then
    PlayerList.beginIndex = 0
    PlayerList:update()
  elseif key == KEY_NEXT then
    local players = getPlayers()
    PlayerList.beginIndex = math.max(#players - PlayerList.maxVisibleRows, 0)
    PlayerList:update(players)
  end
end)

addEventHandler('onExit', function()
  PlayerList:setVisible(false)
end)

return PlayerList

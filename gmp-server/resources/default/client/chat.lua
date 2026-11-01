local language = getLanguageIndex()
local fontPrefix = 'CP1252_'
if language == LANG_PL or language == LANG_CZ or language == LANG_SK or language == LANG_ISV or language == LANG_HU or language == LANG_SZL then
  fontPrefix = 'CP1250_'
elseif language == LANG_RU or language == LANG_UA then
  fontPrefix = 'CP1251_'
elseif language == LANG_TR then
  fontPrefix = 'CP1254_'
end

local Chat = {
  x = 0,
  y = 0,
  maxLines = 15,
  historySize = 50,
  lineHeight = 200,
  inputTextOffset = 100,
  inputLimit = 128,
  font = fontPrefix .. 'FONT_DEFAULT.TGA',
  visible = true,
  fadeMs = 400,
  location = 0,
  inputHistory = {},
  inputHistorySize = 10,
  inputHistoryLocation = 0,
  lines = {}
}

local inputPrompt = Label.new(Chat.x, Chat.y + Chat.maxLines * Chat.lineHeight, '->')
local inputText = Label.new(Chat.x + Chat.inputTextOffset, Chat.y + Chat.maxLines * Chat.lineHeight, '')
local lastInputText = ''

inputPrompt:setFont(Chat.font)
inputText:setFont(Chat.font)
inputPrompt:setColor(190, 100, 0, 255)
inputText:setColor(255, 255, 255)
inputPrompt:setVisible(false)
inputText:setVisible(false)
chatInputSetFont(Chat.font)
chatInputSetMaxLength(Chat.inputLimit)

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

local function splitLines(text)
  text = tostring(text or '')
  text = string.gsub(text, '\r\n', '\n')
  text = string.gsub(text, '\r', '\n')

  local lines = {}
  local startIndex = 1

  while true do
    local newline = string.find(text, '\n', startIndex, true)
    if not newline then
      table.insert(lines, string.sub(text, startIndex))
      return lines
    end

    table.insert(lines, string.sub(text, startIndex, newline - 1))
    startIndex = newline + 1
  end
end

local function makeDraw(text, r, g, b)
  local draw = Label.new(0, 0, text or '')
  draw:setFont(Chat.font)
  draw:setColor(clampByte(r), clampByte(g), clampByte(b))
  draw:setAlpha(0)
  draw:setVisible(false)
  return draw
end

local function hideLine(line)
  if line.nameDraw then
    line.nameDraw:setVisible(false)
  end
  if line.messageDraw then
    line.messageDraw:setVisible(false)
  end
end

local function setLineVisible(line, visible)
  if line.nameDraw then
    line.nameDraw:setVisible(visible)
  end
  line.messageDraw:setVisible(visible)
end

function Chat:layout()
  chatInputSetPosition(self.x, self.y + self.maxLines * self.lineHeight)

  local firstVisible = self:firstVisibleLine()
  local lastVisible = self:lastVisibleLine()

  for index, line in ipairs(self.lines) do
    local visible = self.visible and index >= firstVisible and index <= lastVisible
    setLineVisible(line, visible)

    if visible then
      local y = self.y + (index - firstVisible) * self.lineHeight
      if line.nameDraw then
        line.nameDraw:setPosition(self.x, y)
        line.messageDraw:setPosition(self.x + line.nameDraw:getWidth(), y)
      else
        line.messageDraw:setPosition(self.x, y)
      end
    end
  end
end

function Chat:firstVisibleLine()
  if #self.lines == 0 then
    return 1
  end

  local first = #self.lines - self.maxLines + 1 + self.location
  if first < 1 then
    return 1
  end

  return first
end

function Chat:lastVisibleLine()
  if #self.lines == 0 then
    return 0
  end

  return math.min(self:firstVisibleLine() + self.maxLines - 1, #self.lines)
end

function Chat:setLocation(location)
  local maxBack = math.max(#self.lines - self.maxLines, 0)
  self.location = math.max(-maxBack, math.min(location, 0))
  self:layout()
end

function Chat:scroll(delta)
  self:setLocation(self.location + delta)
end

function Chat:setMaxLines(maxLines)
  maxLines = math.floor(tonumber(maxLines) or self.maxLines)
  if maxLines <= 0 or maxLines > 30 then
    return
  end

  self.maxLines = maxLines
  if self.historySize < self.maxLines then
    self.historySize = self.maxLines
  end

  self:setLocation(self.location)
end

function Chat:setHistorySize(historySize)
  historySize = math.floor(tonumber(historySize) or self.historySize)
  if historySize < self.maxLines then
    return
  end

  self.historySize = historySize
  while #self.lines > self.historySize do
    local old = table.remove(self.lines, 1)
    hideLine(old)
  end

  self:setLocation(self.location)
end

function Chat:setInputHistorySize(size)
  size = math.floor(tonumber(size) or self.inputHistorySize)
  if size < 0 then
    return
  end

  self.inputHistorySize = size
  while #self.inputHistory > self.inputHistorySize do
    table.remove(self.inputHistory, 1)
  end

  self:loadInputHistoryMessage(0)
end

function Chat:pushInputHistoryMessage(message)
  message = tostring(message or '')
  if self.inputHistorySize <= 0 or message == '' then
    return
  end

  if self.inputHistory[#self.inputHistory] == message then
    self.inputHistoryLocation = 0
    return
  end

  table.insert(self.inputHistory, message)
  while #self.inputHistory > self.inputHistorySize do
    table.remove(self.inputHistory, 1)
  end

  self.inputHistoryLocation = 0
end

function Chat:loadInputHistoryMessage(location)
  if #self.inputHistory == 0 then
    return
  end

  location = math.max(-#self.inputHistory, math.min(location, 0))
  if self.inputHistoryLocation == location then
    return
  end

  self.inputHistoryLocation = location

  if location == 0 then
    chatInputSetText('')
  else
    chatInputSetText(self.inputHistory[#self.inputHistory + location + 1])
  end
end

function Chat:showNewest()
  self:setLocation(0)
end

function Chat:push(line)
  local firstVisible = self:firstVisibleLine()
  local wasAtNewest = self.location == 0
  local removed = 0

  table.insert(self.lines, line)
  while #self.lines > self.historySize do
    local old = table.remove(self.lines, 1)
    hideLine(old)
    removed = removed + 1
  end

  if wasAtNewest then
    self.location = 0
  else
    local desiredFirst = math.max(firstVisible - removed, 1)
    local newestFirst = math.max(#self.lines - self.maxLines + 1, 1)
    self.location = desiredFirst - newestFirst
  end

  self:setLocation(self.location)
end

function Chat:pushMessage(message, r, g, b)
  for _, text in ipairs(splitLines(message)) do
    self:push({
      messageDraw = makeDraw(text, r, g, b),
      createdAt = getTickCount()
    })
  end
end

function Chat:clear()
  for _, line in ipairs(self.lines) do
    hideLine(line)
  end
  self.lines = {}
  self.location = 0
  self:layout()
end

function Chat:setVisible(visible)
  self.visible = visible and true or false
  self:layout()

  if not self.visible and chatInputIsOpen() then
    chatInputClear()
    chatInputClose()
  end
end

function Chat:print(message, r, g, b)
  self:pushMessage(message, r, g, b)
end

function Chat:printPlayer(playerId, r, g, b, message)
  self:pushMessage(message, r, g, b)

  if playerId == heroId then
    self:pushInputHistoryMessage(message)
  end
end

function Chat:updateLines()
  local now = getTickCount()
  for _, line in ipairs(self.lines) do
    local alpha = 255
    if self.fadeMs > 0 then
      alpha = math.min(255, math.floor(((now - line.createdAt) / self.fadeMs) * 255))
    end
    if line.nameDraw then
      line.nameDraw:setAlpha(alpha)
    end
    line.messageDraw:setAlpha(alpha)
  end
end

function Chat:updateInput()
  local open = chatInputIsOpen()
  local pos = chatInputGetPosition()
  local font = chatInputGetFont()
  local text = chatInputGetText()
  local caret = chatInputGetCaretPosition()
  local shownText = text

  -- Ordinary typing is consumed natively and does not emit onKeyDown.
  if open and text ~= lastInputText and heroId ~= nil then
    playGesticulation(heroId)
  end
  lastInputText = text

  if open and math.floor(getTickCount() / 750) % 2 == 0 then
    shownText = string.sub(text, 1, caret) .. '_' .. string.sub(text, caret + 1)
  end

  inputPrompt:setFont(font)
  inputText:setFont(font)
  inputPrompt:setPosition(pos.x, pos.y)
  inputText:setPosition(pos.x + self.inputTextOffset, pos.y)
  inputText:setText(shownText)
  inputPrompt:setVisible(open)
  inputText:setVisible(open)
end

local function insertPasteText(text)
  if not chatInputIsOpen() then
    return
  end

  text = tostring(text or '')
  text = string.gsub(text, '[\r\n]+', ' ')
  if text == '' then
    return
  end

  local current = chatInputGetText()
  local caret = chatInputGetCaretPosition()
  local nextText = string.sub(current, 1, caret) .. text .. string.sub(current, caret + 1)

  chatInputSetText(nextText)
  chatInputSetCaretPosition(math.min(caret + #text, #nextText))
end

local function commandText(command, params)
  params = tostring(params or '')
  if params == '' then
    return '/' .. command
  end

  return '/' .. command .. ' ' .. params
end

addEventHandler('onRender', function()
  Chat:updateLines()
  Chat:updateInput()
end)

addEventHandler('onPlayerMessage', function(playerId, r, g, b, message)
    if playerId ~= nil then
        Chat:printPlayer(playerId, r, g, b, message)
    else
        Chat:print(message, r, g, b)
    end
end)

addEventHandler('onPaste', function(text)
  insertPasteText(text)
end)

addEventHandler('onCommand', function(command, params)
  local rawCommand = tostring(command or '')
  command = string.lower(rawCommand)
  params = tostring(params or '')

  Chat:pushInputHistoryMessage(commandText(rawCommand, params))

  if command == 'chatclear' then
    Chat:clear()
    cancelEvent()
  elseif command == 'chatlines' then
    local args = sscanf('d', params)
    if args then
      Chat:setMaxLines(args[1])
    end
    cancelEvent()
  elseif command == 'chatlimit' then
    local args = sscanf('d', params)
    if args then
      Chat:setHistorySize(args[1])
    end
    cancelEvent()
  elseif command == 'chatinputlimit' then
    local args = sscanf('d', params)
    if args then
      Chat:setInputHistorySize(args[1])
    end
    cancelEvent()
  end
end)

addEventHandler('onKeyDown', function(key)
  if chatInputIsOpen() then
    if key == KEY_UP then
      Chat:scroll(-1)
      cancelEvent()
    elseif key == KEY_DOWN then
      Chat:scroll(1)
      cancelEvent()
    elseif key == KEY_PRIOR or key == KEY_PGUP then
      Chat:loadInputHistoryMessage(Chat.inputHistoryLocation - 1)
      cancelEvent()
    elseif key == KEY_NEXT or key == KEY_PGDN then
      Chat:loadInputHistoryMessage(Chat.inputHistoryLocation + 1)
      cancelEvent()
    elseif key == KEY_RETURN then
      Chat:showNewest()
      Chat.inputHistoryLocation = 0
    elseif key == KEY_ESCAPE then
      Chat.inputHistoryLocation = 0
    elseif key == KEY_HOME then
      chatInputSetCaretPosition(0)
      cancelEvent()
    elseif key == KEY_END then
      chatInputSetCaretPosition(#chatInputGetText())
      cancelEvent()
    end
    return
  end

  if key == KEY_T and Chat.visible and not isConsoleOpen() then
    chatInputOpen()
    Chat:showNewest()
    Chat.inputHistoryLocation = 0
    return
  end

  if key == KEY_F7 then
    Chat:setVisible(not Chat.visible)
  end
end)

addEventHandler('onMouseWheel', function(direction)
  if Chat.visible and chatInputIsOpen() then
    Chat:scroll(-direction)
  end
end)

addEventHandler('onExit', function()
  Chat:clear()
  chatInputClear()
  chatInputClose()
end)

function onResourceStop()
  Chat:clear()
  inputPrompt:setVisible(false)
  inputText:setVisible(false)
  chatInputClear()
  chatInputClose()
end

Chat:layout()

return Chat

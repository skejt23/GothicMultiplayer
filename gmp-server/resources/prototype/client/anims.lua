local Anims = {
  x = 6500,
  y = 3200,
  lineHeight = 200,
  maxVisibleRows = 10,
  font = 'FONT_DEFAULT.TGA',
  visible = false,
  selectedIndex = 1,
  firstIndex = 1,
  rows = {}
}

local animationDefinitions = {
  { name = 'Sit', loop = 'S_SIT', start = 'T_STAND_2_SIT', finish = 'T_SIT_2_STAND' },
  { name = 'Piss', loop = 'S_PEE', start = 'T_STAND_2_PEE', finish = 'T_PEE_2_STAND' },
  { name = 'Guard', loop = 'S_HGUARD', start = 'T_STAND_2_HGUARD', finish = 'T_HGUARD_2_STAND' },
  { name = 'Wash', loop = 'S_WASH', start = 'T_STAND_2_WASH', finish = 'T_WASH_2_STAND' },
  { name = 'Pray', loop = 'S_PRAY', start = 'T_STAND_2_PRAY', finish = 'T_PRAY_2_STAND' },
  { name = 'Magic', loop = 'T_PRACTICEMAGIC3' },
  { name = 'Magic2', loop = 'T_PRACTICEMAGIC4' },
  { name = 'Magic3', loop = 'T_PRACTICEMAGIC2' },
  { name = 'Dance', loop = 'T_DANCE_05' },
  { name = 'WatchFight', loop = 'S_WATCHFIGHT', start = 'T_STAND_2_WATCHFIGHT', finish = 'T_WATCHFIGHT_2_STAND' },
  { name = 'Cello', loop = 'S_CELLO_S0', start = 'T_CELLO_STAND_2_S0', finish = 'T_CELLO_S0_2_STAND' },
  { name = 'Drumscheit', loop = 'S_DRUMSCHEIT_S0', start = 'T_DRUMSCHEIT_STAND_2_S0', finish = 'T_DRUMSCHEIT_S0_2_STAND' },
  { name = 'Dudel', loop = 'S_DUDEL_S0', start = 'T_DUDEL_STAND_2_S0', finish = 'T_DUDEL_S0_2_STAND' },
  { name = 'Harfe', loop = 'S_HARFE_S0', start = 'T_HARFE_STAND_2_S0', finish = 'T_HARFE_S0_2_STAND' },
  { name = 'Ielaute', loop = 'S_IELAUTE_S0', start = 'T_IELAUTE_STAND_2_S0', finish = 'T_IELAUTE_S0_2_STAND' },
  { name = 'Drums', loop = 'S_PAUKE_S0', start = 'T_PAUKE_STAND_2_S0', finish = 'T_PAUKE_S0_2_STAND' },
  { name = 'Plunder', loop = 'T_PLUNDER' },
  { name = 'Kick', loop = 'T_BORINGKICK' }
}

local blockedFragments = {
  'DIVE',
  'TOUCHPLATE',
  'VWHEEL',
  'RELOAD',
  'LADDER'
}

local suppressedKeys = {
  KEY_T,
  KEY_F1,
  KEY_F6,
  KEY_F7
}

local animations = {}
local previousControlsDisabled = false
local previousKeyState = {}
local activeMenuLoop = nil

local title = Label.new(Anims.x, Anims.y, 'Anims Menu')
title:setFont(Anims.font)
title:setColor(255, 255, 255)
title:setVisible(false)

local function hasBlockedFragment(value)
  if value == nil then
    return false
  end

  for _, fragment in ipairs(blockedFragments) do
    if string.find(value, fragment, 1, true) ~= nil then
      return true
    end
  end

  return false
end

local function shouldIncludeAnimation(animation)
  return not hasBlockedFragment(animation.loop)
    and not hasBlockedFragment(animation.start)
    and not hasBlockedFragment(animation.finish)
end

for _, animation in ipairs(animationDefinitions) do
  if shouldIncludeAnimation(animation) then
    table.insert(animations, animation)
  end
end

local function clamp(value, minimum, maximum)
  if value < minimum then
    return minimum
  end

  if value > maximum then
    return maximum
  end

  return value
end

local function callBoolean(fn, ...)
  local ok, value = pcall(fn, ...)
  return ok and value == true
end

local function isHeroUnavailable()
  if heroId == nil then
    return true
  end

  return callBoolean(isPlayerDead, heroId) or callBoolean(isPlayerUnconscious, heroId)
end

local function getHeroAnimation()
  if heroId == nil then
    return nil
  end

  local ok, animation = pcall(getPlayerAni, heroId)
  if ok then
    return animation
  end

  return nil
end

local function getControlsDisabled()
  local ok, disabled = pcall(function() return isControlsDisabled() end)
  return ok and disabled == true
end

local function hideRows()
  for _, row in ipairs(Anims.rows) do
    row:setVisible(false)
  end
end

local function setMenuInputDisabled(disabled)
  if disabled then
    previousControlsDisabled = getControlsDisabled()
    disableControls(true)

    for _, key in ipairs(suppressedKeys) do
      if previousKeyState[key] == nil then
        previousKeyState[key] = not isScriptKeyEnabled(key)
      end
      setScriptKeyEnabled(key, false)
    end

    return
  end

  disableControls(previousControlsDisabled)
  previousControlsDisabled = false

  for _, key in ipairs(suppressedKeys) do
    local previous = previousKeyState[key]
    if previous ~= nil then
      setScriptKeyEnabled(key, not previous)
      previousKeyState[key] = nil
    end
  end
end

local function isSelectedLoopActive(animation)
  return activeMenuLoop == animation.loop or getHeroAnimation() == animation.loop
end

for index = 1, Anims.maxVisibleRows do
  local row = Label.new(Anims.x, Anims.y + index * Anims.lineHeight, '')
  row:setFont(Anims.font)
  row:setColor(255, 255, 255)
  row:setVisible(false)
  Anims.rows[index] = row
end

function Anims:update()
  if not self.visible then
    return
  end

  self.selectedIndex = clamp(self.selectedIndex, 1, #animations)
  self.firstIndex = clamp(self.firstIndex, 1, math.max(#animations - self.maxVisibleRows + 1, 1))

  title:setVisible(true)
  hideRows()

  local visibleRows = math.min(self.maxVisibleRows, #animations - self.firstIndex + 1)
  for rowIndex = 1, visibleRows do
    local animationIndex = self.firstIndex + rowIndex - 1
    local row = self.rows[rowIndex]

    row:setText(animations[animationIndex].name)
    if animationIndex == self.selectedIndex then
      row:setColor(128, 180, 128)
    else
      row:setColor(255, 255, 255)
    end
    row:setVisible(true)
  end
end

function Anims:open()
  if self.visible or #animations == 0 or isHeroUnavailable() then
    return
  end

  local animation = animations[self.selectedIndex]
  if animation.finish ~= nil and isSelectedLoopActive(animation) then
    playAni(heroId, animation.finish)
    activeMenuLoop = nil
    return
  end

  if animation.finish == nil and isSelectedLoopActive(animation) then
    stopAni(heroId, animation.loop)
    activeMenuLoop = nil
    return
  end

  self.visible = true
  setMenuInputDisabled(true)
  self:update()
end

function Anims:close()
  if not self.visible then
    return
  end

  self.visible = false
  title:setVisible(false)
  hideRows()
  setMenuInputDisabled(false)
end

function Anims:toggle()
  if self.visible then
    self:close()
  else
    self:open()
  end
end

function Anims:runSelected()
  if #animations == 0 or isHeroUnavailable() then
    self:close()
    return
  end

  local animation = animations[self.selectedIndex]
  if playAni(heroId, animation.start or animation.loop) then
    activeMenuLoop = animation.loop
  else
    activeMenuLoop = nil
  end
  self:close()
end

function Anims:scroll(delta)
  if delta < 0 then
    if self.selectedIndex > 1 then
      self.selectedIndex = self.selectedIndex - 1
      if self.firstIndex > 1 and self.selectedIndex > self.firstIndex then
        self.firstIndex = self.firstIndex - 1
      end
    end
  elseif delta > 0 and self.selectedIndex < #animations then
    self.selectedIndex = self.selectedIndex + 1
    if self.selectedIndex > self.maxVisibleRows then
      self.firstIndex = self.firstIndex + 1
    end
  end

  self:update()
end

addEventHandler('onRender', function()
  if not Anims.visible then
    return
  end

  if isHeroUnavailable() then
    activeMenuLoop = nil
    Anims:close()
    return
  end

  disableControls(true)
  Anims:update()
end)

addEventHandler('onKeyDown', function(key)
  if key == KEY_F3 and not isConsoleOpen() and not chatInputIsOpen() then
    Anims:toggle()
    return
  end

  if not Anims.visible then
    return
  end

  if key == KEY_ESCAPE then
    Anims:close()
  elseif key == KEY_UP then
    Anims:scroll(-1)
  elseif key == KEY_DOWN then
    Anims:scroll(1)
  elseif key == KEY_RETURN then
    Anims:runSelected()
  end
end, 9000)

addEventHandler('onPlayerDeath', function(playerId)
  if heroId ~= nil and tonumber(playerId) == tonumber(heroId) then
    activeMenuLoop = nil
    Anims:close()
  end
end)

addEventHandler('onWorldChange', function()
  activeMenuLoop = nil
  Anims:close()
end)

addEventHandler('onWorldEnter', function()
  activeMenuLoop = nil
  Anims:close()
end)

addEventHandler('onExit', function()
  activeMenuLoop = nil
  Anims:close()
end)

return Anims

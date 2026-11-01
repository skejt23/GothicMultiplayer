local Map = {
  visible = false,
  textureSize = 8200,
  markerFont = 'FONT_DEFAULT.TGA',
  markerDraws = {},
  currentConfig = nil
}

local mapTexture = Sprite.new(0, 0, Map.textureSize, Map.textureSize, 'MAP_NEWWORLD.TGA')
mapTexture:setVisible(false)

local mapConfigs = {
  {
    key = 'newworld',
    texture = 'MAP_NEWWORLD.TGA',
    matches = function(world)
      return string.sub(world, 1, #'NEWWORLD\\NEWWORLD.ZEN') == 'NEWWORLD\\NEWWORLD.ZEN'
    end,
    markerPosition = function(position)
      return math.floor((position.x / 15) + 1850), 4415 - math.floor(position.z / 12)
    end
  },
  {
    key = 'oldworld',
    texture = 'MAP_OLDWORLD.TGA',
    matches = function(world)
      return string.sub(world, 1, #'OLDWORLD\\OLDWORLD.ZEN') == 'OLDWORLD\\OLDWORLD.ZEN'
    end,
    markerPosition = function(position)
      return math.floor((position.x / 16) + 4830), 3855 - math.floor(position.z / 12)
    end
  },
  {
    key = 'jarkendar',
    texture = 'MAP_ADDONWORLD.TGA',
    matches = function(world)
      return string.find(world, 'ADDONWORLD', 1, true) ~= nil
    end,
    markerPosition = function(position)
      return math.floor((position.x / 11) + 4290), 4420 - math.floor(position.z / 10)
    end
  },
  {
    key = 'oldvalley',
    texture = 'MAP_WORLD_ORC.TGA',
    matches = function(world)
      return string.sub(world, 1, #'OLDVALLEY.ZEN') == 'OLDVALLEY.ZEN'
    end,
    markerPosition = function(position)
      return math.floor((position.x / 24) + 4045), 4105 - math.floor(position.z / 16)
    end
  },
  {
    key = 'colony',
    texture = 'MAP_WORLD_ORC.TGA',
    matches = function(world)
      return string.find(world, 'COLONY', 1, true) ~= nil
    end,
    markerPosition = function(position)
      return math.floor((position.x / 24) + 4045), 4105 - math.floor(position.z / 16)
    end
  }
}

local function normalizeWorldName(world)
  world = tostring(world or '')
  world = string.gsub(world, '/', '\\')
  return string.upper(world)
end

local function getCurrentMapConfig()
  local world = normalizeWorldName(getWorld())

  for _, config in ipairs(mapConfigs) do
    if config.matches(world) then
      return config
    end
  end

  return nil
end

local function getOnlinePlayerIds()
  local ok, players = pcall(getOnlinePlayers)
  if ok and players ~= nil then
    return players
  end

  return {}
end

local function getPlayerLabel(playerId)
  local name = getPlayerName(playerId)
  if name == nil or name == '' then
    name = tostring(playerId)
  end

  return '+ ' .. string.sub(name, 1, 96)
end

local function isPlayerDrawable(playerId)
  local world = getPlayerWorld(playerId)
  return world ~= nil and normalizeWorldName(world) == normalizeWorldName(getWorld())
end

local function hideMarkers(fromIndex)
  for index = fromIndex or 1, #Map.markerDraws do
    Map.markerDraws[index]:setVisible(false)
  end
end

local function getMarkerDraw(index)
  local marker = Map.markerDraws[index]
  if marker == nil then
    marker = Label.new(0, 0, '')
    marker:setFont(Map.markerFont)
    marker:setColor(255, 255, 255)
    marker:setVisible(false)
    Map.markerDraws[index] = marker
  end

  return marker
end

function Map:refreshConfig()
  local config = getCurrentMapConfig()
  self.currentConfig = config

  if config ~= nil then
    mapTexture:setFile(config.texture)
  end

  return config
end

function Map:setVisible(visible)
  visible = visible and true or false
  if self.visible == visible then
    return
  end

  self.visible = visible
  mapTexture:setVisible(visible)

  if not visible then
    hideMarkers()
  else
    self:update()
  end
end

function Map:open()
  if self.visible then
    return
  end

  if self:refreshConfig() == nil then
    return
  end

  self:setVisible(true)
end

function Map:close()
  self:setVisible(false)
end

function Map:toggle()
  if self.visible then
    self:close()
  else
    self:open()
  end
end

function Map:update()
  if not self.visible then
    return
  end

  local config = self.currentConfig or self:refreshConfig()
  if config == nil then
    self:close()
    return
  end

  local shownMarkers = 0
  for _, playerId in ipairs(getOnlinePlayerIds()) do
    if isPlayerDrawable(playerId) then
      local position = getPlayerPosition(playerId)
      if position ~= nil then
        shownMarkers = shownMarkers + 1
        local x, y = config.markerPosition(position)
        local marker = getMarkerDraw(shownMarkers)

        marker:setText(getPlayerLabel(playerId))
        marker:setPosition(x, y)
        marker:setVisible(true)
      end
    end
  end

  hideMarkers(shownMarkers + 1)
end

addEventHandler('onRender', function()
  Map:update()
end)

addEventHandler('onKeyDown', function(key)
  if key == KEY_F2 and not isConsoleOpen() and not chatInputIsOpen() then
    Map:toggle()
    return
  end

  if Map.visible and key == KEY_ESCAPE then
    Map:close()
  end
end)

addEventHandler('onWorldChange', function()
  Map:close()
  Map.currentConfig = nil
end)

addEventHandler('onWorldEnter', function()
  Map:close()
  Map.currentConfig = nil
end)

addEventHandler('onExit', function()
  Map:close()
end)

return Map

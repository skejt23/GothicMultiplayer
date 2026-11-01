local function playerName(playerId)
  return getPlayerName(playerId) or ('#' .. tostring(playerId))
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

local positionStorePath = 'prototype/positions.json'
local statAliases = {
  str = 'str',
  strength = 'str',
  dex = 'dex',
  dexterity = 'dex',
  hp = 'hp',
  health = 'hp',
  mana = 'mana'
}
local statLabels = {
  str = 'strength',
  dex = 'dexterity',
  hp = 'health',
  mana = 'mana'
}

local function cmdAcp(playerId)
  sendMessageToPlayer(playerId, 0, 255, 0, '-=========== ACP ===========-')
  sendMessageToPlayer(playerId, 0, 255, 0, '/color id r g b - Change player color')
  sendMessageToPlayer(playerId, 0, 255, 0, '/name id nickname - Change player nickname')
  sendMessageToPlayer(playerId, 0, 255, 0, '/kick id reason - Kick player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/ban id reason - Ban player permanently')
  sendMessageToPlayer(playerId, 0, 255, 0, '/tp from_id to_id - Teleport player to other player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/tpall to_id - Teleport players to other player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/giveitem id instance amount - Give item to player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/stats id str|dex|hp|mana value - Set player stat')
  sendMessageToPlayer(playerId, 0, 255, 0, '/visual id body_model body_texture head_model head_texture - Change player visuals')
  sendMessageToPlayer(playerId, 0, 255, 0, '/pos name - Save your XYZ + angle coordinates')
  sendMessageToPlayer(playerId, 0, 255, 0, '/world id path - Change player world')
  sendMessageToPlayer(playerId, 0, 255, 0, '/instance id name - Change player instance')
  sendMessageToPlayer(playerId, 0, 255, 0, '/scale id x y z - Change player scale')
  sendMessageToPlayer(playerId, 0, 255, 0, '/heal id - Heal player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/kill id - Kill player')
  sendMessageToPlayer(playerId, 0, 255, 0, '/time hour minute - Set server time')
end

local function cmdColor(playerId, params)
  local args = sscanf('dddd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /color id r g b')
    return
  end

  local id, r, g, b = args[1], args[2], args[3], args[4]
  if not isPlayerConnected(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change color of unconnected player!')
    return
  end

  setPlayerNameColor(id, r, g, b)

  sendMessageToPlayer(playerId, r, g, b, string.format('ACP: You changed color of %s to %d, %d, %d', playerName(id), r, g, b))
  sendMessageToPlayer(id, r, g, b, string.format('Your color was changed to %d, %d, %d by %s', r, g, b, playerName(playerId)))
end

local function cmdName(playerId, params)
  local args = sscanf('ds', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /name id nickname')
    return
  end

  local id, name = args[1], args[2]
  if not isPlayerConnected(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change nickname of unconnected player!')
    return
  end

  local oldName = playerName(id)
  setPlayerName(id, name)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed nickname of %s to %s', oldName, name))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Your nickname was changed to %s by %s', name, playerName(playerId)))
end

local function cmdKick(playerId, params)
  local args = sscanf('ds', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /kick id reason')
    return
  end

  local id, reason = args[1], args[2]
  if not isPlayerConnected(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot kick unconnected player!')
    return
  end

  local kickedName = playerName(id)
  local adminName = playerName(playerId)
  kick(id, reason)

  sendMessageToAll(255, 80, 0, string.format('ACP: %s has been kicked by %s', kickedName, adminName))
  sendMessageToAll(255, 80, 0, string.format('Reason: %s', reason))
end

local function cmdBan(playerId, params)
  local args = sscanf('ds', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /ban id reason')
    return
  end

  local id, reason = args[1], args[2]
  if not isPlayerConnected(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot ban unconnected player!')
    return
  end

  local bannedName = playerName(id)
  local adminName = playerName(playerId)
  ban(id, reason)

  sendMessageToAll(255, 0, 0, string.format('ACP: %s has been permanently banned by %s', bannedName, adminName))
  sendMessageToAll(255, 0, 0, string.format('Reason: %s', reason))
end

local function cmdTp(playerId, params)
  local args = sscanf('dd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /tp from_id to_id')
    return
  end

  local fromId, toId = args[1], args[2]
  if not isPlayerSpawned(fromId) or not isPlayerSpawned(toId) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot teleport unconnected or unspawned players!')
    return
  end

  if fromId == toId then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot teleport the same player!')
    return
  end

  local world = getPlayerWorld(toId)
  if world ~= getPlayerWorld(fromId) then
    setPlayerWorld(fromId, world)
  end

  local position = getPlayerPosition(toId)
  if not position then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Target player position is unavailable!')
    return
  end

  setPlayerPosition(fromId, position.x, position.y, position.z)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: Teleported %s to %s', playerName(fromId), playerName(toId)))
  sendMessageToPlayer(fromId, 0, 255, 0, string.format('You were teleported to %s by %s', playerName(toId), playerName(playerId)))
  sendMessageToPlayer(toId, 0, 255, 0, string.format('To you has been teleported %s by %s', playerName(fromId), playerName(playerId)))
end

local function cmdTpAll(playerId, params)
  local args = sscanf('d', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /tpall to_id')
    return
  end

  local toId = args[1]
  if not isPlayerSpawned(toId) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot teleport to unconnected or unspawned player!')
    return
  end

  local world = getPlayerWorld(toId)
  local position = getPlayerPosition(toId)
  if not position then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Target player position is unavailable!')
    return
  end

  local message = string.format('You were teleported to %s by %s', playerName(toId), playerName(playerId))
  for _, id in ipairs(getOnlinePlayers() or {}) do
    if isPlayerSpawned(id) then
      if world ~= getPlayerWorld(id) then
        setPlayerWorld(id, world)
      end

      sendMessageToPlayer(id, 0, 255, 0, message)
      setPlayerPosition(id, position.x, position.y, position.z)
    end
  end

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: Teleported players to %s', playerName(toId)))
end

local function cmdGiveItem(playerId, params)
  local args = sscanf('dsd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /giveitem id instance amount')
    return
  end

  local id, instance, amount = args[1], string.upper(args[2]), args[3]
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot give item to unconnected or unspawned player!')
    return
  end

  if amount < 1 then
    amount = 1
  end

  giveItem(id, instance, amount)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You gave item %s amount: %d to %s', instance, amount, playerName(id)))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Received item %s amount: %d from %s', instance, amount, playerName(playerId)))
end

local function cmdStats(playerId, params)
  local args = sscanf('dsd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /stats id str|dex|hp|mana value')
    return
  end

  local id, stat, value = args[1], statAliases[string.lower(args[2] or '')], args[3]
  if not stat then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Unknown stat. Use str, dex, hp or mana')
    return
  end

  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change stats of unconnected or unspawned player!')
    return
  end

  if stat == 'str' then
    value = math.max(value, 0)
    setPlayerStrength(id, value)
  elseif stat == 'dex' then
    value = math.max(value, 0)
    setPlayerDexterity(id, value)
  elseif stat == 'hp' then
    value = math.max(value, 1)
    if (getPlayerMaxHealth(id) or 0) < value then
      setPlayerMaxHealth(id, value)
    end
    setPlayerHealth(id, value)
  elseif stat == 'mana' then
    value = math.max(value, 1)
    if (getPlayerMaxMana(id) or 0) < value then
      setPlayerMaxMana(id, value)
    end
    setPlayerMana(id, value)
  end

  local label = statLabels[stat]
  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed %s %s to %d', playerName(id), label, value))
  sendMessageToPlayer(id, 0, 255, 0, string.format('%s was changed to %d by %s', label, value, playerName(playerId)))
end

local function cmdVisual(playerId, params)
  local args = sscanf('dsdsddd', params or '') or sscanf('dsdsd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /visual id body_model body_texture head_model head_texture [teeth_texture] [skin_color]')
    return
  end

  local id, bodyModel, bodyTexture, headModel, headTexture = args[1], args[2], args[3], args[4], args[5]
  local teethTexture, skinColor = args[6] or 0, args[7] or 0
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change visuals of unconnected or unspawned player!')
    return
  end

  if not setPlayerVisual(id, bodyModel, bodyTexture, headModel, headTexture, teethTexture, skinColor) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Failed to change player visuals')
    return
  end

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed %s visuals to %s/%d %s/%d teeth:%d skin:%d',
      playerName(id), bodyModel, bodyTexture, headModel, headTexture, teethTexture, skinColor))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Your visuals were changed by %s', playerName(playerId)))
end

local function cmdPos(playerId, params)
  local args = sscanf('s', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /pos name')
    return
  end

  if not isPlayerSpawned(playerId) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You must be spawned to save your position!')
    return
  end

  local name = args[1]
  local position = getPlayerPosition(playerId)
  local angle = getPlayerAngle(playerId)
  if not position or angle == nil then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Your position is unavailable!')
    return
  end

  local file = JSON.open(positionStorePath)
  if not file then
    sendMessageToPlayer(playerId, 255, 0, 0, string.format('ACP: Cannot open %s', positionStorePath))
    return
  end

  local saved, saveError = file:set(name, {
    x = position.x,
    y = position.y,
    z = position.z,
    angle = angle,
    world = getPlayerWorld(playerId) or ''
  })

  if not saved then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Cannot save position: ' .. saveError.message)
    return
  end

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: Saved position %s (%.2f, %.2f, %.2f, %.2f)',
      name, position.x, position.y, position.z, angle))
end

local function cmdWorld(playerId, params)
  local args = sscanf('ds', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /world id path')
    return
  end

  local id, world = args[1], args[2]
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change world for unconnected or unspawned player!')
    return
  end

  setPlayerWorld(id, world)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed %s world to %s', playerName(id), world))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Changing world to %s by %s', world, playerName(playerId)))
end

local function cmdInstance(playerId, params)
  local args = sscanf('ds', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /instance id name')
    return
  end

  local id, instance = args[1], string.upper(args[2])
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change instance of unconnected or unspawned player!')
    return
  end

  setPlayerInstance(id, instance)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed %s instance to %s', playerName(id), instance))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Changing instance to %s by %s', instance, playerName(playerId)))
end

local function cmdScale(playerId, params)
  local args = sscanf('dfff', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /scale id x y z')
    return
  end

  local id, x, y, z = args[1], args[2], args[3], args[4]
  if not isPlayerSpawned(id) then
      sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot change scale of unconnected or unspawned player!')
      return
  end
  
  setPlayerScale(id, x, y, z)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You changed %s scale to %.2f, %.2f, %.2f', playerName(id), x, y, z))
  sendMessageToPlayer(id, 0, 255, 0, string.format('Changing scale to %.2f, %.2f, %.2f by %s', x, y, z, playerName(playerId)))
end

local function cmdHeal(playerId, params)
  local args = sscanf('d', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /heal id')
    return
  end

  local id = args[1]
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot heal unconnected or unspawned player!')
    return
  end

  setPlayerHealth(id, getPlayerMaxHealth(id) or 1)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You healed %s', playerName(id)))
  sendMessageToPlayer(id, 0, 255, 0, string.format('You were healed by %s', playerName(playerId)))
end

local function cmdKill(playerId, params)
  local args = sscanf('d', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /kill id')
    return
  end

  local id = args[1]
  if not isPlayerSpawned(id) then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: You cannot kill unconnected or unspawned player!')
    return
  end

  setPlayerHealth(id, 0)

  sendMessageToPlayer(playerId, 0, 255, 0, string.format('ACP: You killed %s', playerName(id)))
  sendMessageToPlayer(id, 0, 255, 0, string.format('You were killed by %s', playerName(playerId)))
end

local function cmdTime(playerId, params)
  local args = sscanf('dd', params or '')
  if not args then
    sendMessageToPlayer(playerId, 255, 0, 0, 'ACP: Type /time hour min')
    return
  end

  local hour = clamp(args[1], 0, 23)
  local minute = clamp(args[2], 0, 59)

  setTime(hour, minute)
  sendMessageToAll(0, 255, 0, string.format('ACP: %s changed time to %02d:%02d', playerName(playerId), hour, minute))
end

local commandHandlers = {
  acp = cmdAcp,
  color = cmdColor,
  name = cmdName,
  kick = cmdKick,
  ban = cmdBan,
  tp = cmdTp,
  tpall = cmdTpAll,
  giveitem = cmdGiveItem,
  stats = cmdStats,
  visual = cmdVisual,
  pos = cmdPos,
  world = cmdWorld,
  instance = cmdInstance,
  scale = cmdScale,
  heal = cmdHeal,
  kill = cmdKill,
  time = cmdTime,
}

addEventHandler('onPlayerCommand', function(playerId, command, params)
  local handler = commandHandlers[string.lower(command or '')]
  if handler then
    if not isPlayerAdmin(playerId) then
      sendMessageToPlayer(player_id, 255, 0, 0, "ACP: You are not an admin!")
      return
    end
    
    handler(playerId, params or '')
  end
end)

local talkingPlayers = {}
local playerChannels = {}

addEventHandler('onPlayerVoiceStart', function(playerId)
  talkingPlayers[playerId] = true
end)

addEventHandler('onPlayerVoiceStop', function(playerId)
  talkingPlayers[playerId] = nil
end)

addEventHandler('onPlayerVoiceChannelChange', function(playerId, _, newChannel)
  playerChannels[playerId] = newChannel
  sendMessageToPlayer(playerId, 80, 220, 120, 'Voice channel: ' .. newChannel)
end)

addEventHandler('onPlayerDisconnect', function(playerId)
  talkingPlayers[playerId] = nil
  playerChannels[playerId] = nil
end)

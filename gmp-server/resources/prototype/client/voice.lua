local VoiceChat = {
  enabled = false,
  available = false,
  range = 0,
  channel = 'default',
  transmitting = false,
  talkingPlayers = {},
  defaultKey = KEY_K,
  defaultChannel = 'default'
}

local indicator = Label.new(6500, 7600, '')
indicator:setFont('FONT_DEFAULT.TGA')
indicator:setColor(80, 220, 120)
indicator:setVisible(false)

function VoiceChat:isAvailable()
  return Voice:isAvailable()
end

function VoiceChat:isPlayerTalking(playerId)
  return self.talkingPlayers[playerId] == true
end

function VoiceChat:updateIndicator()
  indicator:setText(string.format('VOICE [%s]', self.channel))
  indicator:setVisible(self.enabled and self.transmitting)
end

function VoiceChat:initialize()
  local keySet = Voice:setPushToTalkKey(self.defaultKey)
  local channelSet = Voice:setChannel(self.defaultChannel)

  self.available = self:isAvailable()
  self.enabled = self.available and Voice:setScriptEnabled(true)
  self.range = Voice:getRange()
  self.channel = Voice:getChannel()
  self:updateIndicator()

  return keySet and channelSet and self.enabled
end

addEventHandler('onVoiceChatStateChange', function(enabled, range)
  VoiceChat.available = Voice:isAvailable()
  VoiceChat.enabled = enabled
  VoiceChat.range = range
  VoiceChat:updateIndicator()
end)

addEventHandler('onVoiceTransmitStart', function()
  VoiceChat.transmitting = true
  VoiceChat:updateIndicator()
end)

addEventHandler('onVoiceTransmitStop', function()
  VoiceChat.transmitting = false
  VoiceChat:updateIndicator()
end)

addEventHandler('onVoiceChannelChange', function(_, newChannel)
  VoiceChat.channel = newChannel
  VoiceChat:updateIndicator()
end)

addEventHandler('onPlayerVoiceStart', function(playerId)
  VoiceChat.talkingPlayers[playerId] = true
end)

addEventHandler('onPlayerVoiceStop', function(playerId)
  VoiceChat.talkingPlayers[playerId] = nil
end)

addEventHandler('onExit', function()
  VoiceChat.transmitting = false
  VoiceChat.talkingPlayers = {}
  VoiceChat:updateIndicator()
end)

function onResourceStop()
  indicator:setVisible(false)
end

VoiceChat:initialize()

return VoiceChat

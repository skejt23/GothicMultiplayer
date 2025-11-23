-- Example server-side packet echo handler for script packets.
-- The client sends a key code as an unsigned byte; the server responds with a string.

local RELIABLE_TYPE = RELIABLE or 1

addEventHandler('onPacket', function(playerId, packet)
    local key_code = packet:readUInt8()
    local response = Packet.new()
	local messageResponse = string.format('Server received key: %d from %d', key_code, playerId)
	print(messageResponse)
    response:writeString(messageResponse)
    -- Reliability values follow Net::PacketReliability in common/znet/net_enums.h
    response:send(playerId, RELIABLE_TYPE)
	response:reset()
end)
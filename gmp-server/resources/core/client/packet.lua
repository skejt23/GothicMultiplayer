-- Example client-side usage of script packets.
-- Sends the pressed key code to the server and prints the server's response.

local RELIABLE_TYPE = RELIABLE or 1

addEventHandler('onKeyDown', function(key)
    local packet = Packet.new()
    packet:writeUInt8(key)
    packet:send(RELIABLE_TYPE)
	packet:reset()
end)

addEventHandler('onPacket', function(packet)
    local message = packet:readString()
    LOG_INFO('[Packet Example] {}', message)
end)
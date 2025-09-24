local kServerMessageIntervalSeconds = 30
local lastServerMessageTimestamp = 0

addEventHandler('onClockUpdate', function(day, hour, minute)
    local now = os.time()
    if now - lastServerMessageTimestamp >= kServerMessageIntervalSeconds then
        SendServerMessage("Remember to drink some water and have fun!")
        lastServerMessageTimestamp = now
    end
end)
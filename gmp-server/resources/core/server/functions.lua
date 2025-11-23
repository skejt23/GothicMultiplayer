local kServerMessageIntervalSeconds = 30
local lastServerMessageTimestamp = 0

addEventHandler('onGameTime', function(day, hour, minute)
    local now = os.time()
    if now - lastServerMessageTimestamp >= kServerMessageIntervalSeconds then
        SendServerMessage("Remember to drink some water and have fun!")
        lastServerMessageTimestamp = now

        local pos = getPlayerPosition(1)
        print(string.format("Player 1 pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z))
        --setPlayerPosition(id, 0, 0, 0)
    end
end)

addEventHandler('onPlayerCommand', function(id, cmd)
    if cmd == "spawn" then
        print("Matched spawn, teleporting")
        setPlayerPosition(id, 0, 0, 0)
    end
end)
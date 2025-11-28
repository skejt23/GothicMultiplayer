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

addEventHandler("onPlayerCommand", function(id, cmd, params)
    if cmd == "spawn" then
        setPlayerPosition(id, 0, 0, 0)
        return
    end

    if cmd == "time" then
        local parsed = sscanf("dd", params)
        setTime(parsed[1], parsed[2])
        print(getTime().hour, getTime().min)
        return
    end

    if cmd == "map" then
        print(getServerWorld())
        setServerWorld(params)
        print(getServerWorld())
        return
    end

    if cmd == "msg" then
        local parsed = sscanf("ddds", params)

        if not parsed then
            sendMessageToPlayer(id, 255, 0, 0, "Usage: /msg r g b text")
            return
        end

        local r = parsed[1]
        local g = parsed[2]
        local b = parsed[3]
        local text = parsed[4]

        sendMessageToPlayer(id, r, g, b, text)
        return
    end

    if cmd == "post" then
        local parsed = sscanf("ddds", params)
        if not parsed then
            sendMessageToPlayer(id, 255, 0, 0, "Usage: /post r g b text")
            return
        end

        local r = parsed[1]
        local g = parsed[2]
        local b = parsed[3]
        local text = parsed[4]

        sendMessageToAll(r, g, b, text)
        return
    end

    if cmd == "all" then
        local parsed = sscanf("ddds", params)
        if not parsed then
            sendMessageToPlayer(id, 255, 0, 0, "Usage: /all r g b text")
            return
        end

        local r = parsed[1]
        local g = parsed[2]
        local b = parsed[3]
        local text = parsed[4]

        sendPlayerMessageToAll(id, r, g, b, text)
        return
    end

    if cmd == "pm" then
        local parsed = sscanf("dddds", params)

        if not parsed then
            sendMessageToPlayer(id, 255, 0, 0, "Usage: /pm toId r g b text")
            return
        end

        local toId = parsed[1]
        local r    = parsed[2]
        local g    = parsed[3]
        local b    = parsed[4]
        local text = parsed[5]

        if not toId then
            sendMessageToPlayer(id, 255, 0, 0, "Invalid player id")
            return
        end

        sendPlayerMessageToPlayer(id, toId, r, g, b, text)
        return
    end
end)

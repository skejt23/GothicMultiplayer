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

    if cmd == "name" then
        print(getPlayerName(id))
        setPlayerName(id, params)
        print(getPlayerName(id))
        return
    end

    if cmd == "item" then
        local parsed = sscanf("sd", params)
        giveItem(id, parsed[1], parsed[2])
        return
    end

    if cmd == "itemu" then
        unequipItem(id, params)
        return
    end

    if cmd == "iteme" then
        equipItem(id, params)
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

    if cmd == "visual" then
        setPlayerVisual(id, "HUM_BODY_NAKED0", 8, "HUM_HEAD_FIGHTER", 26)
        local visual = getPlayerVisual(id)
        print("visual: " .. visual.bodyModel .. " | " .. visual.bodyTexture .. " | " .. visual.headModel .. " | " .. visual.headTexture)
		setPlayerFatness(id, 5.0)
        local fatness = getPlayerFatness(id)
        print("Fatness: " .. fatness)
		setPlayerScale(id, 2.5, 0.75, 2.5)
        local scale = getPlayerScale(id)
        print("Scale: " .. scale.x .. "," .. scale.y .. "," .. scale.z)
		applyPlayerOverlay(id, "HUMANS_RELAXED.MDS")
        local overlays = getPlayerOverlays(id)
        print("Overlays: " .. #overlays)
    elseif cmd == "hp" then
        setPlayerMaxHealth(id, 150)
        setPlayerHealth(id, 120)
        print("Health: " .. getPlayerHealth(id) .. "/" .. getPlayerMaxHealth(id))
    elseif cmd == "mp" then
        setPlayerMaxMana(id, 200)
        setPlayerMana(id, 75)
        print("Mana: " .. getPlayerMana(id) .. "/" .. getPlayerMaxMana(id))
    elseif cmd == "stats" then
        setPlayerStrength(id, 40)
		print("Strength: " .. getPlayerStrength(id))
        setPlayerDexterity(id, 25)
		print("Dexterity: " .. getPlayerDexterity(id))
        setPlayerSkillWeapon(id, WEAPON_1H, 80)
		print("Skill 1H: " .. getPlayerSkillWeapon(id, WEAPON_1H) .. " | " .. WEAPON_1H)
        setPlayerTalent(id, TALENT_BOW, 2)
		print("Talent Bow: " .. getPlayerTalent(id, TALENT_BOW) .. " | " .. TALENT_BOW)
        setPlayerTalent(id, TALENT_MAGE, 3)
		print("Magic Circle: " .. getPlayerTalent(id, TALENT_MAGE) .. " | " .. TALENT_MAGE)
        setPlayerTalent(id, TALENT_PICKPOCKET, 1)
		print("Pickpocket: " .. getPlayerTalent(id, TALENT_PICKPOCKET) .. " | " .. TALENT_PICKPOCKET)
    elseif cmd == "pos" then
		setPlayerWorld(id, "COLONY.ZEN")
        local world = getPlayerWorld(id)
        print("Worldpath: " .. world)
        setPlayerPosition(id, -4577.95752, 720, 738.122925)
		setPlayerAngle(id, 180)
        local pos = getPlayerPosition(id)
        print("Position: " .. pos.x .. "," .. pos.y .. "," .. pos.z .. "," .. getPlayerAngle(id))
    elseif cmd == "color" then
        setPlayerColor(id, 0, 200, 255)
        local color = getPlayerColor(id)
        print("Nickname color: " .. color.r .. "," .. color.g .. "," .. color.b)
    elseif cmd == "exp" then
        setPlayerExp(id, 1250)
        setPlayerNextLevelExp(id, 2000)
        print("Experience: " .. getPlayerExp(id) .. "/" .. getPlayerNextLevelExp(id))
        setPlayerLearnPoints(id, 20)
        print("Learn points: " .. getPlayerLearnPoints(id))
        setPlayerLevel(id, 30)
        print("Level: " .. getPlayerLevel(id))
    end
end)

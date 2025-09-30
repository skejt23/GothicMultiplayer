local function vectorToString(x, y, z)
    return string.format("(%.2f, %.2f, %.2f)", x, y, z)
end

local function optionalIdToString(id)
    if id == nil then
        return "nil"
    end

    return tostring(id)
end

addEventHandler('onInit', function()
    print('Hello GMP')
end)

addEventHandler('onGameTime', function(day, hour, minute)
    LOG_INFO("Clock update: Day {} Time {:02d}:{:02d}", day, hour, minute)
end)

addEventHandler('onPlayerConnect', function(id)
    LOG_INFO("Player {} connected", id)
end)

addEventHandler('onPlayerDisconnect', function(id)
    LOG_INFO("Player {} disconnected", id)
end)

addEventHandler('onPlayerMessage', function(id, text)
    LOG_INFO("{} says: {}", id, text)
end)

addEventHandler('onPlayerCommand', function(id, command)
    LOG_INFO("Command from {}: /{}", id, command)
end)

addEventHandler('onPlayerWhisper', function(fromId, toId, text)
    LOG_INFO("{} whispers to {}: {}", fromId, toId, text)
end)

addEventHandler('onPlayerKill', function(killerId, victimId)
    LOG_INFO("Player {} killed {}", killerId, victimId)
end)

addEventHandler('onPlayerDeath', function(playerId, killerId)
    LOG_INFO("Player {} died (killer: {})", playerId, optionalIdToString(killerId))
end)

addEventHandler('onPlayerDropItem', function(playerId, itemInstance, amount)
    LOG_INFO("Player {} dropped item {} x{}", playerId, itemInstance, amount)
end)

addEventHandler('onPlayerTakeItem', function(playerId, itemInstance)
    LOG_INFO("Player {} picked up item {}", playerId, itemInstance)
end)

addEventHandler('onPlayerCastSpell', function(casterId, spellId, targetId)
    LOG_INFO("Player {} cast spell {} on {}", casterId, spellId, optionalIdToString(targetId))
end)

addEventHandler('onPlayerSpawn', function(playerId, posX, posY, posZ)
    LOG_INFO("Player {} spawned at {}", playerId, vectorToString(posX, posY, posZ))
end)

addEventHandler('onPlayerRespawn', function(playerId, posX, posY, posZ)
    LOG_INFO("Player {} respawned at {}", playerId, vectorToString(posX, posY, posZ))
end)

addEventHandler('onPlayerHit', function(attackerId, victimId, damage)
    LOG_INFO("{} hit {} for {} HP", optionalIdToString(attackerId), victimId, damage)
end)
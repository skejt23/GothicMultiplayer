local function vectorToString(x, y, z)
    return string.format("(%.2f, %.2f, %.2f)", x, y, z)
end

local function optionalIdToString(id)
    if id == nil then
        return "nil"
    end

    return tostring(id)
end

addEventHandler('onGameTime', function(day, hour, minute)
    LOG_INFO("Clock update: Day {} Time {:02d}:{:02d}", day, hour, minute)
end)

addEventHandler('onPlayerConnect', function(id)
    LOG_INFO("Player {} connected", id)
    spawnPlayer(id, 0, 0, 0) -- Khorinis

    giveItem(id, "ITAR_PAL_H", 1)
    giveItem(id, "ITMW_1H_VLK_DAGGER", 1)
    equipItem(id, "ITMW_2H_AXE_L_01")

    giveItem(id, "ITRW_ARROW", 100)
    equipItem(id, "ITRW_BOW_L_01")

    giveItem(id, "ITSC_LIGHT", 5)

    giveItem(id, "ITRI_STR_02", 1)
    giveItem(id, "ITRI_DEX_02", 1)

    giveItem(id, "ITFO_APPLE", 5)
    giveItem(id, "ITFO_STEW", 5)
end)

addEventHandler('onPlayerDisconnect', function(id)
    LOG_INFO("Player {} disconnected", id)
end)

addEventHandler('onPlayerMessage', function(id, text)
    LOG_INFO("{} says: {}", id, text)
end)

addEventHandler('onPlayerCommand', function(id, command, params)
    LOG_INFO("Command from {}: /{} {}", id, command, params)
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

addEventHandler('onPlayerWeaponModeChange', function(playerId, weaponModeOld, WeaponModeNew)
    LOG_INFO("Player {} changed weapon mode from {} to {}", playerId, weaponModeOld, WeaponModeNew)
end)

addEventHandler('onPlayerHandItemChange', function(playerId, handSlot, itemInstance)
    LOG_INFO("Player {} changed equip state for Hand Item on slot {} for {}", playerId, handSlot, itemInstance)
end)

addEventHandler('onPlayerRingChange', function(playerId, handSlot, itemInstance)
    LOG_INFO("Player {} changed equip state for Ring on slot {} for {}", playerId, handSlot, itemInstance)
end)

addEventHandler('onPlayerShieldChange', function(playerId, itemInstance)
    LOG_INFO("Player {} changed equip state for Shield for {}", playerId, itemInstance)
end)

addEventHandler('onPlayerArmorChange', function(playerId, itemInstance)
    LOG_INFO("Player {} changed equip state for Armor for {}", playerId, itemInstance)
end)

addEventHandler('onPlayerMeleeWeaponChange', function(playerId, itemInstance)
    LOG_INFO("Player {} changed equip state for Melee Weapon for {}", playerId, itemInstance)
end)

addEventHandler('onPlayerRangedWeaponChange', function(playerId, itemInstance)
    LOG_INFO("Player {} changed equip state for Ranged Weapon for {}", playerId, itemInstance)
end)

addEventHandler('onPlayerSpellSlotChange', function(playerId, handSlot, itemInstance)
    LOG_INFO("Player {} changed equip state for a Spell Slot on slot {} for {}", playerId, handSlot, itemInstance)
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

addEventHandler('onPlayerSpawnFor', function(playerId, spawnedId)
    LOG_INFO("Player {} respawned for {}", spawnedId, playerId)
end)

addEventHandler('onPlayerUnpawnFor', function(playerId, spawnedId)
    LOG_INFO("Player {} despawned for {}", spawnedId, playerId)
end)

addEventHandler('onPlayerHit', function(attackerId, victimId, damage)
    LOG_INFO("{} hit {} for {} HP", optionalIdToString(attackerId), victimId, damage)
end)
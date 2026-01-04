    local npc = nil
    local npc2 = nil

addEventHandler('onKeyDown', function(key)
	if(key == KEY_Z) then
        print("keyZ -> spawning npcs")
            npc = createNpc("TestNpc")
            npc2 = createNpc("TestNpc2")
            npc3 = createNpc("TestNpc3")
        spawnNpc(npc, "PC_HERO")
        spawnNpc(npc2, "SCAVENGER")
	elseif(key == KEY_X) then
        print("keyX -> destroying npcs")
        destroyNpc(npc)
        destroyNpc(npc2)
	end
	if key == KEY_M then
		giveItem(heroId, "ITAR_LEATHER_L", 1)
	end
end)
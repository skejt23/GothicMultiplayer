addEventHandler('onInit', function()
    LOG_INFO('JOINING SERVER')
end)

addEventHandler('onExit', function()
    LOG_INFO('LEAVING SERVER')
end)

addEventHandler('onKeyDown', function(key)
    LOG_INFO('[Core][Client] Pressed via onKeyDown (key={})', key)
	if(key == KEY_F4) then
        print("F4 down!")
		setTime(20, 0);
	elseif(KeyPressed(KEY_U)) then
        print("U down!")
		setTime(6, 0);
	end
end)

addEventHandler('onKeyUp', function(key)
    LOG_INFO('[Core][Client] Unpressed via onKeyUp (key={})', key)
    if(key == KEY_F4) then
        print("F4 released!")
	elseif(KeyToggled(KEY_U)) then
        print("U released!")
	end
end)

addEventHandler('onPlayerCreate', function(playerId)
    LOG_INFO('[Core][Client] Player created: id={}', playerId)
end)

addEventHandler('onPlayerDestroy', function(playerId)
    LOG_INFO('[Core][Client] Player destroyed: id={}', playerId)
end)

addEventHandler("onRender", function()
    -- Called every frame; place lightweight UI logic here if needed.
end)
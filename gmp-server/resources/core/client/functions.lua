addEventHandler('onKeyDown', function(key)
    if key == KEY_V then
        setPlayerVisual(heroId, "HUM_BODY_NAKED0", 8, "HUM_HEAD_FIGHTER", 26)
		setPlayerFatness(heroId, 5.0)
		setPlayerScale(heroId, 2.5, 0.75, 2.5)
		applyPlayerOverlay(heroId, "HUMANS_RELAXED.MDS")
    end
end)
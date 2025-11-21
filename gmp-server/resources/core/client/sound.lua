------------------------------------------------------------
-- Sound example (minimal, uses all functions + properties)
------------------------------------------------------------

-- Create sound from file
local sfx = Sound.new("GAMESTART.WAV")

-- Set properties (all floats except file)
sfx.volume  = 1.0        -- 0.0 ? 1.0
sfx.balance = 0.0        -- -1.0 ? 1.0  (center)
sfx.looping = true

-- Read back properties
local file        = sfx.file
local vol         = sfx.volume
local bal         = sfx.balance
local loopState   = sfx.looping
local timePlayed  = sfx.playingTime    -- read-only

--[[ addEventHandler('onInit', function()
	-- Start playback
	sfx:play()

    print("Playing sound " .. file)
	print(vol)
	print(bal)
	print(loopState)
	print(timePlayed)
	print(sfx:isPlaying())
end) ]]
--[[ It will not work on init, there's some timing issue at play unfortunately. ]]

addEventHandler('onKeyDown', function(key)
    if key == KEY_K then
        sfx:play()
        print("Pressed K, playing sound")
			print("Playing sound " .. file)
			print(vol)
			print(bal)
			print(loopState)
			print(timePlayed)
			print(sfx:isPlaying())
    end
    if key == KEY_I then
        if(sfx:isPlaying()) then
			sfx:stop()
			print("Pressed I, stopping sound")
		end
    end
end)
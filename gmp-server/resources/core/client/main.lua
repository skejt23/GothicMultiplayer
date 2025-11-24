LOG_INFO('[Core][Client] Core client-side resources initialized.')

print(getHostname())
print(getMaxSlots())
print(getOnlinePlayers())
print(getPlayersCount())
print(getDistance2d(0, 0, 10, 10))
print(getDistance3d(0, 0, 0, 10, 10, 10))
print(getVectorAngle(0, 0, 10, 103))

addEventHandler('onKeyDown', function(key)
	if key == KEY_N then
		print(getHostname())
		print(getMaxSlots())
		print(getOnlinePlayers())
		print(getPlayersCount())
		print(getDistance2d(0, 0, 10, 10))
		print(getDistance3d(0, 0, 0, 10, 10, 10))
		print(getVectorAngle(0, 0, 10, 103))
	end
	if key == KEY_M then
		local playercount = getPlayersCount()
		for i = 1, playercount do
			print(i)
		end
	end
end)
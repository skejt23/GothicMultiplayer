print('Hello GMP')

-- Test logging functions
LOG_WARN("Easy padding in numbers like {:08d}", 12);
LOG_CRITICAL("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
LOG_INFO("Positional args are {1} {0}..", "too", "supported");
LOG_INFO("{:>8} aligned, {:<8} aligned", "right", "left");

SetDiscordActivity({
    State = "In the Colony",
    Details = "Gothic Multiplayer adventures",
    --LargeImageKey = "gmp_logo",
    --LargeImageText = "Gothic Multiplayer 2011",
    --SmallImageKey = "campfire",
    --SmallImageText = "Campfire meetup"
})

-- Demonstrates how to dynamically update a single field while keeping the others intact.
addEventHandler('onPlayerConnect', function(playerId)
    SetDiscordActivity({
        Details = string.format("Player %d just joined the server", playerId)
    })
end)
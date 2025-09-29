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
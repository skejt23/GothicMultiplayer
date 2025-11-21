LOG_WARN('[timer.lua] Demonstrating timer helpers')

local tick_counter = 0
local repeating_timer

repeating_timer = setTimer(function(message)
    tick_counter = tick_counter + 1
    LOG_INFO('[timer.lua] Tick {} -> {}', tick_counter, message)

    if tick_counter == 3 then
        if setTimerInterval(repeating_timer, 2000) then
            LOG_INFO('[timer.lua] Timer interval updated to 2000 ms')
        end
    elseif tick_counter == 5 then
        if setTimerExecuteTimes(repeating_timer, 2) then
            LOG_INFO('[timer.lua] Timer will execute two more times')
        end
    elseif tick_counter >= 7 then
        if killTimer(repeating_timer) then
            LOG_INFO('[timer.lua] Repeating timer stopped')
        end
    end
end, 1000, 0, 'Repeating timer example')

if repeating_timer then
    LOG_INFO('[timer.lua] Created repeating timer with id {}', repeating_timer)
else
    LOG_ERROR('[timer.lua] Failed to create repeating timer')
end

setTimer(function()
    LOG_INFO('[timer.lua] One-shot timer fired after 3 seconds')
end, 3000, 1)
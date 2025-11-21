LOG_WARN('[utility.lua] Demonstrating utility helpers')

local start_tick = getTickCount()
LOG_INFO('[utility.lua] Server tick at load: {} ms', start_tick)

local color = hexToRgb('#7fc4ff')
if color then
    local hex_without_prefix = rgbToHex(color.r, color.g, color.b)
    local hex_with_alpha = rgbToHex(color.r, color.g, color.b, color.a or 255, true, { uppercase = true })

    LOG_INFO('[utility.lua] hexToRgb -> r={} g={} b={} a={}', color.r, color.g, color.b, color.a or 'n/a')
    LOG_INFO('[utility.lua] rgbToHex (no prefix) -> {}', hex_without_prefix)
    LOG_INFO('[utility.lua] rgbToHex (with prefix + alpha) -> {}', hex_with_alpha)
else
    LOG_WARN('[utility.lua] Failed to decode color string')
end

local parsed = sscanf('7 1.25 "quoted value" true', '%d %f %q %b')
if parsed then
    LOG_INFO('[utility.lua] sscanf -> integer={}, float={}, text={}, bool={}', parsed[1], parsed[2], parsed[3], tostring(parsed[4]))
else
    LOG_WARN('[utility.lua] sscanf example failed to parse input')
end

local function log_elapsed()
    local elapsed = getTickCount() - start_tick
    LOG_INFO('[utility.lua] {} ms elapsed since script load', elapsed)
end

setTimer(log_elapsed, 2500, 1)
local cursorVisible = false
local currentTexture = 'LO.TGA'
local sizeVirtual = { w = 96, h = 96 }

addEventHandler('onKeyDown', function(key)
    if key == KEY_NUMPAD0 then
        if cursorVisible then 
            setCursorTxt(currentTexture)
            setCursorSize(sizeVirtual.w, sizeVirtual.h)
            setCursorSensitivity(5.0)
            setCursorPosition(4096, 4096)
            LOG_INFO('[Example] Cursor defaults.')
        else
            cursorVisible = not cursorVisible
            setCursorVisible(cursorVisible)
            LOG_INFO(string.format('[Example] Cursor visibility set to %s', tostring(cursorVisible)))
        end
    end
end)

addEventHandler('onMouseMove', function(dx, dy)
    -- Nudge the cursor position manually using relative delta
    --[[ local x, y = getCursorPosition()
    if x and y then
        setCursorPosition(x + dx, y + dy)
    end ]]
    LOG_INFO(string.format('[Example] Mouse moved: %f %f', dx, dy))
end)

addEventHandler('onMouseDown', function(button)
    if not cursorVisible then return end

    if(button == MOUSE_BUTTONLEFT) then
        sizeVirtual.w = math.max(96, sizeVirtual.w - 2)
        sizeVirtual.h = math.max(96, sizeVirtual.h - 2)
        setCursorSize(sizeVirtual.w, sizeVirtual.h)
        LOG_INFO(string.format('[Example] Cursor size set to %dx%d (virtual)', sizeVirtual.w, sizeVirtual.h))
    end
    if(button == MOUSE_BUTTONRIGHT) then
        sizeVirtual.w = math.min(256, sizeVirtual.w + 2)
        sizeVirtual.h = math.min(256, sizeVirtual.h + 2)
        setCursorSize(sizeVirtual.w, sizeVirtual.h)
        LOG_INFO(string.format('[Example] Cursor size set to %dx%d (virtual)', sizeVirtual.w, sizeVirtual.h))
    end
    if(isMouseBtnPressed(MOUSE_BUTTONMID)) then
        currentTexture = (currentTexture == 'LO.TGA') and 'DEFAULT.TGA' or 'LO.TGA'
        setCursorTxt(currentTexture)
        LOG_INFO(string.format('[Example] Cursor texture changed to %s', currentTexture))
    end
    LOG_INFO(string.format('[Example] Mouse button %d pressed', button))
end)

addEventHandler('onMouseUp', function(button)
    LOG_INFO(string.format('[Example] Mouse button %d released', button))
end)

addEventHandler('onMouseWheel', function(direction)
    local sens = getCursorSensitivity()
    if(direction > 0) then
		setCursorSensitivity(sens + 1.0)
    LOG_INFO(string.format('[Example] Mouse sensitivity: %f', getCursorSensitivity()))
    else 
        setCursorSensitivity(sens - 1.0)
    LOG_INFO(string.format('[Example] Mouse sensitivity: %f', getCursorSensitivity()))
    end
    LOG_INFO(string.format('[Example] Mouse wheel moved: %d', direction))
end)
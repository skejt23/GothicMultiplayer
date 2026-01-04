-- Example showcase for control and input helpers

local function toggleControls()
    local controlsDisabled = not isControlsDisabled()
    disableControls(controlsDisabled)

    if controlsDisabled then
        LOG_INFO('[Example] Controls disabled; hero AI paused and player input cleared.')
    else
        LOG_INFO('[Example] Controls restored; hero AI re-enabled.')
    end
end

local function toggleTabKey()
    local tabDisabled = not isKeyDisabled(KEY_TAB)
    disableKey(KEY_TAB, tabDisabled)
    LOG_INFO(string.format('[Example] TAB key %s', tabDisabled and 'disabled' or 'enabled'))
end

local function reportStates()
    LOG_INFO(string.format('[Example] Controls disabled: %s', tostring(isControlsDisabled())))
    LOG_INFO(string.format('[Example] TAB disabled: %s', tostring(isKeyDisabled(KEY_TAB))))
end

addEventHandler('onKeyUp', function(key)
    LOG_INFO('[Core][Client] Unpressed via onKeyUp (key={})', key)
end)

addEventHandler('onKeyDown', function(key)
    LOG_INFO('[Core][Client] Pressed via onKeyDown (key={})', key)
    if key == KEY_J then
        toggleControls()
    elseif key == KEY_U then
        toggleTabKey()
    elseif key == KEY_N then
        reportStates()
    end
end)
LOG_INFO('[Core][Client] Core client-side resources initialized.')

addEventHandler("onRender", function()
    if KeyToggled(Key.F4) then
        LOG_INFO("F4 key toggled!")
    end
end)

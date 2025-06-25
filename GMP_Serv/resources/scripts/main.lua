print('Hello GMP')

-- Test logging functions
LOG_WARN("Easy padding in numbers like {:08d}", 12);
LOG_CRITICAL("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
LOG_INFO("Positional args are {1} {0}..", "too", "supported");
LOG_INFO("{:>8} aligned, {:<8} aligned", "right", "left");

addEventHandler('onClockUpdate', function(day, hour, minute)
    print("Server clock has been updated. Day " .. day .. " Hour " .. hour .. " Minute " .. minute .. ".")
end)

addEventHandler('onPlayerConnect', function(id)
    print("Player " .. id .. " has connected.")
end)
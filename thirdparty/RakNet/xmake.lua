target("RakNet")
    set_kind("static")
    
    -- Add all source files from the CMakeLists.txt
    add_files("Source/*.cpp")
    
    -- Set include directories
    add_includedirs("Source", {public = true})
    
    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("wsock32", "ws2_32", "Iphlpapi")
        add_cxflags("/w")  -- Disable all warnings for MSVC
    else
        add_cxflags("-fPIC")
    end
    
    -- Add threading support
    add_packages("threads") 
    set_languages("c++20")
    set_default(false) -- So it's not installed by default
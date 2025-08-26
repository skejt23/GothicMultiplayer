-- MIT License

-- Copyright (c) 2025 Gothic Multiplayer Team.

-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:

-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.

-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

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
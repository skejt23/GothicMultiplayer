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

target("LuaRuntime")
    set_kind("static")
    
    -- Source files
    -- Note: script_base.h is header-only template (policy-based design)
    add_files("spdlog_bind.cpp", "utility_bind.cpp", "timer_manager.cpp")
    
    -- Headers
    add_headerfiles("*.h")
    
    -- Include directories
    -- Public include allows: #include "shared/lua_runtime/script_base.h"
    add_includedirs("$(projectdir)", {public = true})
    
    -- Dependencies
    add_deps("SharedLib")
    add_packages("sol2", "spdlog", "fmt", {public = true})
    
    -- Not built by default (only when needed by client/server)
    set_default(false)

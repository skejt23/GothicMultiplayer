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

target("GMPLauncher")
    set_kind("binary")
    add_files("main.cpp")
    add_includedirs("../../../common")
    add_defines("_WIN32_WINNT=0x0601", "WINVER=0x0601")
    
    if is_plat("windows") then
        add_files("resource.rc")
    end

    add_syslinks("kernel32", "user32", "advapi32", "winhttp", "psapi")
    add_packages("spdlog")

    local update_source_url = get_config("gmp_update_source_url")
    if update_source_url and #update_source_url > 0 then
        update_source_url = update_source_url:gsub("\\", "\\\\"):gsub("\"", "\\\"")
        add_defines(string.format("GMP_UPDATE_SOURCE_URL=\"%s\"", update_source_url))
    end
    
    -- Set output name
    set_basename("GMPLauncher")
    
    -- Set working directory for debugging
    set_rundir("$(projectdir)")

    on_install(function (target)
        import("install_to_multiplayer_dir")(target, {launcher = true})
    end)

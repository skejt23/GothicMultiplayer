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

set_project("GothicMultiplayer")
set_version("0.2", {build = "%Y%m%d"})

set_languages("c++20")
set_defaultarchs("linux|x64", "windows|x86")
set_runtimes("MT")

-- Set output directory for binaries
set_rundir("$(builddir)/bin")
set_configdir("$(builddir)/config")
add_configfiles("version.h.in")
add_moduledirs("xmake/modules")
set_prefixdir("/", { bindir = "." })

option("discord_app_id")
    set_showmenu(true)
    set_description("Discord application id used for rich presence")
    set_default("")
option_end()

option("master_server_endpoint")
    set_showmenu(true)
    set_description("HTTP endpoint used to register this server in the master server list")
    set_default("")
option_end()


add_rules("mode.debug", "mode.release", "mode.releasedbg")

add_requires("spdlog 1.15.1", {configs = {fmt_external = true}})
-- Pulling in gtest_main doesn't work, due to a runtime conflict MT (static) vs MD (dynamic), so we still have to define it ourselves
add_requires("gtest 1.16.*", {configs = {main = true}})
add_requires("fmt 11.0.2",
             "toml11 4.3.*", 
             "lua 5.4.7",
             "dylib 2.2.*",
             "nlohmann_json 3.11.*",
             "bitsery 5.2.*",
             "glm 1.0.*",
             "sol2 3.3.*",
             "cpp-httplib 0.21.0",
             "zlib 1.3.1",
             "openssl 1.1.1-w")

includes("common", "shared", "gmp-server", "thirdparty", "tests")

if is_plat("windows") then
    add_requires("discord")
    includes("gmp-client", "InjectMage")
end
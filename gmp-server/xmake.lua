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

includes("lib/znet", "lib/znet_rak")

target("Server")
    set_kind("static")
    add_files("lib/*.cpp")
    add_files("lib/Lua/*.cpp")
    add_includedirs("$(builddir)/config")
    add_includedirs("lib", {public = true})
    add_deps("common", "SharedLib", "znet_server")
    add_defines("SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")
    add_packages("spdlog", "fmt", "toml11", "nlohmann_json", "bitsery", "glm", "sol2", "cpp-httplib", "dylib", "openssl", {public = true})
    local master_endpoint = get_config("master_server_endpoint")
    if master_endpoint and #master_endpoint > 0 then
        add_defines(string.format("MASTER_SERVER_ENDPOINT=\"%s\"", master_endpoint))
    end
    set_default(false) -- So it's not installed by default

target("ServerApp")
    set_basename("gmp-server")
    set_kind("binary")
    add_files("app/main.cpp")
    add_deps("Server")
    add_packages("spdlog")
    set_prefixdir("gmp-server", { bindir = "." })
    add_installfiles("resources/*")
    add_installfiles("resources/scripts/*", {prefixdir = "scripts"})

includes("test")
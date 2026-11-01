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
set_defaultarchs("linux|x86_64", "windows|x86")
set_runtimes("MT")

-- sol2 otherwise converts missing or invalid Lua arguments with unchecked
-- lua_to* calls (for example, a missing integer becomes 0). Enable the one
-- safety feature needed by bindings without changing sol2's reference/object
-- ABI. The local compatibility header also avoids sol2 3.3's heap-allocating
-- function-signature formatter on argument failures.
add_defines("SOL_SAFE_FUNCTION_CALLS=1")
add_includedirs("Shared/lua_runtime/sol2_compat")

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
    set_description("HTTPS endpoint used for master server registration and server lists")
    set_default("")
option_end()

option("gmp_update_source_url")
    set_showmenu(true)
    set_description("Base HTTP(S) URL where GMPLauncher downloads GMP.dll and its .sha256 file")
    set_default("")
option_end()


add_rules("mode.debug", "mode.release", "mode.releasedbg")

includes("xmake/packages/gamenetworkingsockets.lua")
local mariadb_package = "mariadb-connector-c"
if is_plat("linux") then
    includes("xmake/packages/mariadb-connector-c.lua")
    mariadb_package = "gmp-mariadb-connector-c"
end
add_requires("gamenetworkingsockets v1.6.0", {configs = {shared = false}})
-- Share the already required crypto package instead of linking another copy.
add_requireconfs("gamenetworkingsockets.protobuf-cpp.abseil", {override = true, version = "20250814.0"})
add_requireconfs("gamenetworkingsockets.openssl3", {override = true, version = "3.6.3"})
add_requireconfs("cpp-httplib.openssl3", {override = true, version = "3.6.3"})
add_requireconfs(mariadb_package .. ".openssl3", {override = true, version = "3.6.3"})

add_requires("spdlog 1.15.1", {configs = {fmt_external = true}})
add_requires("cpp-httplib 0.22.0", {configs = {ssl = true}})
add_requires("xxhash 0.8.3", {configs = {shared = false, cmake = false}})
-- Pulling in gtest_main doesn't work, due to a runtime conflict MT (static) vs MD (dynamic), so we still have to define it ourselves
add_requires("gtest 1.16.*", {configs = {main = true}})
add_requires("sqlite3 3.53.0+400", {configs = {
    explain_comments = false,
    dbpage_vtab = false,
    stmt_vtab = false,
    dbstat_vtab = false,
    math_functions = false,
    omit_deprecated = true
}})
add_requires(mariadb_package .. " 3.4.9", {alias = "mariadb-connector-c", configs = {
    ssl = is_plat("windows") and "schannel" or "openssl3",
    curl = false,
    dyncol = false,
    external_zlib = true
}})
add_requires("fmt 11.0.2",
             "toml11 4.4.*", 
             "lua 5.4.7",
             "nlohmann_json 3.12.*",
             "bitsery 5.2.*",
             "glm 1.0.*",
             "sol2 3.3.*",
             "zlib 1.3.1",
             "miniz 3.1.1",
             "libopus 1.5.*",
             "minizip 1.2.13",
             "openssl3 3.6.3",
             "libsodium 1.0.*")

includes("common", "Shared", "gmp-server", "thirdparty", "gmp-client/client-net", "tests")

if is_plat("windows") then
    add_requires("polyhook2", "asmjit", "stb", "minimp3", "discord")
    includes("gmp-client")
end

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

target("BanListTest")
    set_kind("binary")
    add_files("ban_list_test.cpp")
    add_deps("Server")
    add_packages("spdlog", "bitsery")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    -- disable the build by default
    set_default(false)

target("ResourceManagerTest")
    set_kind("binary")
    add_files("resource_manager_test.cpp")
    add_deps("Server")
    add_packages("spdlog", "sol2", "fmt", "toml11")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    -- disable the build by default
    set_default(false)

target("DatabaseBindingTest")
    set_kind("binary")
    set_encodings("utf-8")
    add_files("database_binding_test.cpp")
    add_deps("Server")
    add_packages("gtest", "sol2", "sqlite3", "mariadb-connector-c")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("ClientResourcePackagerTest")
    set_kind("binary")
    add_files("client_resource_packager_test.cpp")
    add_deps("Server")
    add_packages("spdlog", "fmt", "toml11")
    add_packages("nlohmann_json")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("AddonConfigTest")
    set_kind("binary")
    add_files("addon_config_test.cpp")
    add_deps("Server")
    add_packages("gtest", "spdlog", "toml11", "libsodium")
    add_tests("default")
    set_rundir(os.projectdir())
    if is_plat("windows") then
        add_cxflags("/utf-8")
    end
    set_default(false)

target("NpcManagerTest")
    set_kind("binary")
    set_encodings("utf-8")
    add_files("npc_manager_test.cpp")
    add_deps("Server")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("PlayerManagerTest")
    set_kind("binary")
    set_encodings("utf-8")
    add_files("player_manager_test.cpp")
    add_deps("Server")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("ActorInventoryTest")
    set_kind("binary")
    set_encodings("utf-8")
    add_files("actor_inventory_test.cpp")
    add_deps("Server")
    add_packages("gtest", "nlohmann_json", "spdlog")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

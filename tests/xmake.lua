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

target("MasterServerEndpointTest")
    set_kind("binary")
    add_files("master_server_endpoint_test.cpp")
    add_deps("common")
    add_packages("gtest", "cpp-httplib")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("ConnectionTest")
    set_kind("binary")
    set_encodings("utf-8")
    add_files("fake_client.cpp", "fake_client_connection_test.cpp", "real_client_connection_test.cpp")
    add_deps("Server", "Client.Net")
    add_packages("spdlog", "bitsery")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    -- disable the build by default
    set_default(false)

target("TransportTest")
    set_kind("binary")
    add_files("transport_test.cpp")
    add_deps("Transport")
    add_packages("gtest", "gamenetworkingsockets")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("AnimationSyncTest")
    set_kind("binary")
    set_languages("c++20")
    add_files("animation_sync_test.cpp")
    add_deps("common")
    add_packages("gtest", "glm")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("NativeDamageObservationTest")
    set_kind("binary")
    set_languages("c++20")
    add_files("native_damage_observation_test.cpp")
    add_deps("common")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

target("MusicAssetPathTest")
    set_kind("binary")
    set_languages("c++20")
    add_files("music_asset_path_test.cpp")
    add_packages("gtest")
    add_tests("default")
    set_rundir(os.projectdir())
    set_default(false)

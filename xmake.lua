set_project("GothicMultiplayer")
set_version("0.2", {build = "%Y%m%d"})

set_languages("c++20")
set_defaultarchs("linux|x64", "windows|x86")
set_runtimes("MT")

-- Set output directory for binaries
set_rundir("$(builddir)/bin")
set_configdir("$(builddir)/config")
add_configfiles("version.h.in")
set_prefixdir("/", { bindir = "." })

add_rules("mode.debug", "mode.release", "mode.releasedbg")

add_requires("spdlog 1.15.1", {configs = {fmt_external = true}})
add_requires("fmt 11.0.2",
             "toml11 4.3.*", 
             "lua 5.4.7",
             "dylib 2.2.*",
             "gtest 1.16.*",
             "nlohmann_json 3.11.*",
             "bitsery 5.2.*",
             "glm 1.0.*",
             "sol2 3.3.*",
             "pugixml 1.15",
             "cpp-httplib 0.21.0",
             "zlib 1.3.1")

includes("xmake", "common", "Shared", "GMP_Serv", "thirdparty")

if is_plat("windows") then
    includes("GMP_Client", "InjectMage")
end

target("ZenKit")
    set_kind("static")
    set_languages("c++20")
    add_includedirs("ZenKit/include", {public = true})
    add_defines("_ZKEXPORT=1", "ZKNO_REM=1")
    add_defines("_ZK_WITH_ZIPPED_VDF=1", {public = true})
    add_packages("miniz", {public = true})
    add_files(
        "ZenKit/src/Error.cc",
        "ZenKit/src/Logger.cc",
        "ZenKit/src/Misc.cc",
        "ZenKit/src/Date.cc",
        "ZenKit/src/Stream.cc",
        "ZenKit/src/Vfs.cc",
        "ZenKit/src/DaedalusScript.cc",
        "ZenKit/src/DaedalusVm.cc",
        "ZenKit/src/addon/daedalus.cc"
    )
    if is_plat("windows") then
        add_files("ZenKit/src/MmapWin32.cc")
        add_defines("_ZK_WITH_MMAP=1", {public = true})
    elseif is_plat("linux") or is_plat("macosx") then
        add_files("ZenKit/src/MmapPosix.cc")
        add_defines("_ZK_WITH_MMAP=1", {public = true})
    end
    if is_plat("windows") then
        add_defines("NOMINMAX")
    end
    set_default(false)

if is_plat("windows") then
    includes("SDL3", "BugTrap")
end
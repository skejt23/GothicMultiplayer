-- g2api static library
target("g2api")
    set_kind("static")
    add_files("g2api/ocgame.cpp")
    add_includedirs("g2api", {public = true})
    add_includedirs("dx7sdk/inc", {public = true})
    add_deps("InjectMage")
    set_default(false) -- So it's not installed by default

-- zNetInterface header-only library
target("zNetInterface")
    set_kind("headeronly")
    add_includedirs("zNet", {public = true})
    add_deps("common")

-- znet shared library
target("ClientNet")
    set_kind("shared")
    add_files("znet_rak/client.cpp")
    add_deps("zNetInterface", "RakNet")
    add_packages("spdlog")
    set_basename("znet")

    on_install("install_to_system_dir")
    
-- Main GMP shared library (DLL)
target("ClientMain")
    set_kind("shared")
    add_includedirs("$(builddir)/config")
    add_files("Button.cpp",
              "CActiveAniID.cpp",
              "CAnimMenu.cpp", 
              "CChat.cpp",
              "CConfig.cpp",
              "game_client.cpp",
              "CHeroClass.cpp",
              "CIngame.cpp",
              "CInterpolatePos.cpp",
              "CInventory.cpp",
              "CLanguage.cpp",
              "CLocalPlayer.cpp",
              "CMainMenu.cpp",
              "CMap.cpp",
              "CMenu.cpp",
              "CObjectMenu.cpp",
              "CObservation.cpp",
              "CPlayer.cpp",
              "CPlayerList.cpp",
              "CSelectClass.cpp",
              "CServerList.cpp",
              "CShrinker.cpp",
              "CSpawnPoint.cpp",
              "CSyncFuncs.cpp",
              "CWatch.cpp",
              "ExceptionHandler.cpp",
              "ExtendedServerList.cpp",
              "Font.cpp",
              "GMPSplash.cpp",
              "GMP_Client.cpp",
              "HooksManager.cpp",
              "Interface.cpp",
              "keyboard.cpp",
              "Mod.cpp",
              "Network.cpp",
              "HTTPDownloader.cpp",
              "patch.cpp",
              "Table.cpp",
              "config.cpp",
              "WorldBuilder/CBuilder.cpp",
              "WorldBuilder/load.cpp",
              "WorldBuilder/save.cpp")
    
    add_deps("common", "g2api", "SharedLib", "zNetInterface", "Client.Voice", "SDL3", "InjectMage", "BugTrap")    
    add_packages("spdlog", "cpp-httplib", "dylib", "pugixml", "glm", "bitsery")
    add_syslinks("wsock32", "ws2_32", "Iphlpapi", "user32", "gdi32", "kernel32")
    
    add_defines("SPDLOG_FMT_EXTERNAL")
    add_defines("DIRECTINPUT_VERSION=0x0800")
    add_defines("_WIN32_WINNT=0x0601", "WINVER=0x0601")
    
    -- Resource file handling (Windows specific)
    if is_plat("windows") then
        add_files("resource.rc")
    end
    
    -- Set output name
    set_basename("GMP")

    on_install(function (target)
        local install_to_system_dir = import("install_to_system_dir")
        -- Install the main GMP shared library
        install_to_system_dir(target)

        -- Install resources
        local installdir = target:installdir()
        local scriptdir = target:scriptdir()
        os.mkdir(path.join(installdir, "Data"))
        os.mkdir(path.join(installdir, "Multiplayer"))
        os.cp(path.join(scriptdir, "resources/Data/*"), path.join(installdir, "Data"))
        os.cp(path.join(scriptdir, "resources/Multiplayer/*"), path.join(installdir, "Multiplayer"))
        print("Installed resources to " .. installdir)
    end)

-- Include subdirectories
includes("Voice", "Launcher")
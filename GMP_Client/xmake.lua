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
              "external_console_window.cpp",
              "patch.cpp",
              "Table.cpp",
              "config.cpp",
              "DiscordPresence.cpp",
              "WorldBuilder/CBuilder.cpp",
              "WorldBuilder/load.cpp",
              "WorldBuilder/save.cpp")

    add_files("DiscordGameSDK/src/*.cpp")
    add_includedirs("DiscordGameSDK/src")
    
    add_files("gothic-patches/*.cpp")
    add_includedirs("gothic-patches")
    
    add_deps("common", "SharedLib", "zNetInterface", "Client.Voice", "SDL3", "InjectMage", "BugTrap", "gothic_api")    
    add_packages("spdlog", "cpp-httplib", "dylib", "pugixml", "glm", "bitsery", "nlohmann_json")
    add_syslinks("wsock32", "ws2_32", "Iphlpapi", "user32", "gdi32", "kernel32")

    add_linkdirs("DiscordGameSDK/lib")
    add_links("discord_game_sdk")
    
    add_defines("SPDLOG_FMT_EXTERNAL")
    add_defines("DIRECTINPUT_VERSION=0x0800")
    add_defines("_WIN32_WINNT=0x0601", "WINVER=0x0601")
    add_defines("SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")
    
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

        local dllSource = path.join(scriptdir, "DiscordGameSDK/lib/discord_game_sdk.dll")
        if os.isfile(dllSource) then
            local systemLower = path.join(installdir, "system")
            local systemUpper = path.join(installdir, "System")
            local systemDir = nil

            if os.isdir(systemLower) then
                systemDir = systemLower
            elseif os.isdir(systemUpper) then
                systemDir = systemUpper
            else
                systemDir = systemLower
                os.mkdir(systemDir)
            end

            os.cp(dllSource, systemDir)
            print("Installed discord_game_sdk.dll to " .. systemDir)
        end
    end)

-- Include subdirectories
includes("Voice", "Launcher", "gothic-api")
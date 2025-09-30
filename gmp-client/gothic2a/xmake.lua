-- Main GMP shared library (DLL)
target("ClientMain")
    set_kind("shared")
    add_includedirs("$(builddir)/config")
    add_files("src/**.cpp")
    add_includedirs("src/gothic-patches")
    
    add_deps("common", "SharedLib", "zNetInterface", "Client.Voice", "SDL3", "InjectMage", "BugTrap", "gothic_api")
    add_packages("spdlog", "fmt", "cpp-httplib", "dylib", "glm", "bitsery", "nlohmann_json")
    add_syslinks("wsock32", "ws2_32", "Iphlpapi", "user32", "gdi32", "kernel32")

    add_defines("SPDLOG_FMT_EXTERNAL")
    add_defines("DIRECTINPUT_VERSION=0x0800")
    add_defines("_WIN32_WINNT=0x0601", "WINVER=0x0601")
    add_defines("SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")
    
    local discord_app_id = get_config("discord_app_id")
    if discord_app_id and #discord_app_id > 0 then
        if not discord_app_id:match("^%d+$") then
            raise("discord_app_id must contain only digits")
        end
        add_packages("discord")
        add_defines(string.format("DISCORD_APPLICATION_ID=%sLL", discord_app_id))
    end

    local master_endpoint = get_config("master_server_endpoint")
    if master_endpoint and #master_endpoint > 0 then
        add_defines(string.format("MASTER_SERVER_ENDPOINT=\"%s\"", master_endpoint))
    end


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

        local discord_pkg = target:pkg("discord")
        if discord_pkg then
            local dllCandidates = {}
            local pkgdir = discord_pkg:installdir()
            if pkgdir then
                dllCandidates = os.files(path.join(pkgdir, "**/discord_game_sdk.dll")) or {}
            end
            if #dllCandidates == 0 then
                local fetchinfo = discord_pkg:fetch()
                if fetchinfo then
                    local libfiles = fetchinfo.libfiles or {}
                    for _, libfile in ipairs(libfiles) do
                        if libfile:lower():match("discord_game_sdk%.dll$") then
                            table.insert(dllCandidates, libfile)
                            break
                        end
                    end
                end
            end
        if #dllCandidates > 0 then
           local dllSource = dllCandidates[1]
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
        end
    end)

-- Include subdirectories
includes("launcher", "lib")
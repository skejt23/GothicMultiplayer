target("SDL3")
    set_kind("phony")

    add_includedirs("include", {public = true})

    if is_plat("windows") then
        if is_arch("x86") then
            add_linkdirs("lib", {public = true})
            add_links("SDL3", {public = true})
        end
    end
    
    set_group("thirdparty")

    on_install(function (target)
        if is_plat("windows") then
            local dll_path = path.join(os.scriptdir(), "lib", "SDL3.dll")
            if os.isfile(dll_path) then
                local install_dir = target:installdir()
                if install_dir then
                    -- Check for both "system" and "System", given that Gothic might use either
                    local system_dir_lower = path.join(install_dir, "system")
                    local system_dir_upper = path.join(install_dir, "System")
                    
                    local target_dir = nil
                    if os.isdir(system_dir_lower) then
                        target_dir = system_dir_lower
                    elseif os.isdir(system_dir_upper) then
                        target_dir = system_dir_upper
                    else
                        target_dir = system_dir_lower
                        os.mkdir(target_dir)
                    end
                    
                    os.cp(dll_path, target_dir)
                    print("Installed SDL3.dll to " .. target_dir)
                end
            end
        end
    end)
-- Helper function for installing DLLs to Gothic system directory
function install_to_system_dir(target)
    if is_plat("windows") then
        local dll_path = target:targetfile()
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
                print("Installed " .. path.filename(target:targetfile()) .. " to " .. target_dir)
            end
        end
    end
end
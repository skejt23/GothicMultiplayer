target("GMPLauncher")
    set_kind("binary")
    add_files("main.cpp")
    add_syslinks("kernel32", "user32", "advapi32")
    add_packages("spdlog")
    
    -- Set output name
    set_basename("GMPLauncher")
    
    -- Set working directory for debugging
    set_rundir("$(projectdir)")

    on_install(install_to_system_dir)
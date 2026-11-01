target("BugTrap")
    set_kind("phony")
    
    add_includedirs(".", {public = true})
    
    if is_plat("windows") then
        add_linkdirs(".", {public = true})
        add_links("BugTrap", {public = true})
    end
    
    set_group("thirdparty")

    on_install(function (target)
        import("install_to_multiplayer_dir")(target, {file = path.join(target:scriptdir(), "BugTrap.dll")})
    end)

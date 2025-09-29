-- zNetInterface header-only library
target("zNetInterface")
    set_kind("headeronly")
    add_includedirs("zNet", {public = true})
    add_deps("common")

-- znet shared library
target("ClientNet")
    set_kind("shared")
    add_files("znet-rak/client.cpp")
    add_deps("zNetInterface", "RakNet")
    add_packages("spdlog")
    set_basename("znet")

    on_install("install_to_system_dir")
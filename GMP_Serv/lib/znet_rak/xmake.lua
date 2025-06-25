target("znet_server")
    set_kind("shared")
    add_files("server.cpp")
    add_deps("common", "RakNet", "zNetServerInterface")
    add_packages("spdlog")
    add_includedirs(".", {public = true})
    set_default(false) -- So it's not installed by default

target("Client.Resources")
    set_kind("static")
    add_files("src/**.cpp")
    add_headerfiles("include/(**.h)")
    add_includedirs("include", {public = true})
    add_includedirs("../client-net/include", {public = true})
    add_includedirs("../client-net/lib/znet", {public = true})

    add_deps("LuaRuntime", "ResourceLoader", "gothic_api", "Client.Net")
    add_packages("spdlog", "fmt", "sol2", "bitsery", {public = true})
    set_default(false)

includes("test")

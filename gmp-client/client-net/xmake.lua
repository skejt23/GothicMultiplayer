target("Client.Net")
    set_kind("static")
    add_files("src/**.cpp")
    add_deps("zNetServerInterface")
    add_packages("spdlog")
    add_includedirs("include", {public = true})

includes("lib")
target("Client.Net")
    set_kind("static")
    add_files("src/**.cpp")
    add_deps("zNetInterface")
    add_packages("spdlog", "fmt", "cpp-httplib", "dylib", "glm", "bitsery", "nlohmann_json")
    add_includedirs("include", {public = true})

includes("lib")
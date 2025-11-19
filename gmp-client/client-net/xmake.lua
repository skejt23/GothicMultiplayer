target("Client.Net")
    set_kind("static")
    add_files("src/**.cpp")
    add_deps("zNetInterface", "SharedLib")
    add_packages("spdlog", "fmt", "cpp-httplib", "dylib", "glm", "bitsery", "nlohmann_json", "libsodium", {public = true})
    add_includedirs("include", {public = true})

includes("lib")
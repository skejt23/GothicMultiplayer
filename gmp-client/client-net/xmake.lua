target("Client.Net")
    set_kind("static")
    add_files("src/**.cpp")
    add_deps("Transport", "SharedLib", "AddonSupport")
    add_packages("spdlog", "fmt", "cpp-httplib", "glm", "bitsery", "nlohmann_json", "libsodium", {public = true})
    add_includedirs("include", {public = true})

includes("test")

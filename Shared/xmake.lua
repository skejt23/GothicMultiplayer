target("SharedLib")
    set_kind("static")
    add_files("toml_wrapper.cpp", "event.cpp", "math.cpp")
    add_includedirs("include", {public = true})
    add_deps("common")
    add_packages("toml11", "glm", {public = true})
    set_default(false) -- So it's not installed by default
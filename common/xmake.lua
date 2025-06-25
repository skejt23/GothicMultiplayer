target("common")
    set_kind("headeronly")
    add_includedirs(".", "znet", {public = true})
    add_packages("glm") 
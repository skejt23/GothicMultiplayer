target("Client.Voice")
    set_kind("static")
    add_files("VoiceCapture.cpp", "VoicePlayback.cpp")
    add_includedirs(".", {public = true})
    add_packages("spdlog", "zlib")
    add_deps("SDL3")
    set_default(false) -- So it's not installed by default

target("Client.VoiceTestApp")
    set_kind("binary")
    add_files("VoiceTestApp/voice_test_app.cpp")
    add_deps("Client.Voice")
    set_prefixdir("/", { bindir = "tools" })
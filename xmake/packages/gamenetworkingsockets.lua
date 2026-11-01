-- The direct-IP transport is statically linked into each executable/module.
-- Use one Protobuf runtime and the project's existing OpenSSL package; neither
-- Steamworks nor the optional ICE/WebRTC dependencies are built.
package("gamenetworkingsockets")
    set_homepage("https://github.com/ValveSoftware/GameNetworkingSockets")
    set_description("Valve's open-source reliable/unreliable UDP transport")
    set_license("BSD-3-Clause")
    set_urls("https://github.com/ValveSoftware/GameNetworkingSockets/archive/refs/tags/$(version).tar.gz")
    add_versions("v1.6.0", "bddfe735d29ff2bbf186013a945ed57caaf4b79893eb7918c06c0f64955016f3")
    add_deps("cmake", "protobuf-cpp 32.1", "openssl3 3.6.3")
    add_defines("STEAMNETWORKINGSOCKETS_STATIC_LINK")
    add_links("GameNetworkingSockets_s")
    add_includedirs("include/GameNetworkingSockets")
    on_load(function (package)
        if package:is_plat("windows") then
            package:add("syslinks", "ws2_32", "crypt32", "winmm", "iphlpapi")
        else
            package:add("syslinks", "pthread")
        end
    end)
    on_install("windows|x86", "windows|x64", "linux|x86_64", "linux|x64", function (package)
        local protobuf = package:dep("protobuf-cpp")
        local openssl = package:dep("openssl3")
        local configs = {
            "-DBUILD_STATIC_LIB=ON", "-DBUILD_SHARED_LIB=OFF",
            "-DBUILD_EXAMPLES=OFF", "-DBUILD_TESTS=OFF", "-DBUILD_TOOLS=OFF",
            "-DENABLE_ICE=OFF", "-DUSE_STEAMWEBRTC=OFF",
            "-DUSE_CRYPTO=OpenSSL", "-DProtobuf_USE_STATIC_LIBS=ON",
            "-DOPENSSL_USE_STATIC_LIBS=ON",
            "-DOPENSSL_ROOT_DIR=" .. openssl:installdir(),
            "-DProtobuf_ROOT=" .. protobuf:installdir(),
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
            "-DCMAKE_CXX_STANDARD=17"
        }
        import("package.tools.cmake").install(package, configs, {packagedeps = {"protobuf-cpp", "openssl3"}})
    end)
    on_test(function (package)
        assert(package:has_cxxfuncs("GameNetworkingSockets_Kill()", {includes = "steam/steamnetworkingsockets.h", configs = {languages = "c++17"}}))
    end)
package_end()

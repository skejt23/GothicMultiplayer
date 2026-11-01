-- Reuse upstream metadata and dependencies, but pass the CMake backend name.
package("gmp-mariadb-connector-c")
    set_base("mariadb-connector-c")
    on_install("linux", function (package)
        local ssl = package:config("ssl")
        local configs = {
            CMAKE_C_STANDARD = "99",
            CMAKE_BUILD_TYPE = package:debug() and "Debug" or "Release",
            CMAKE_COMPILE_WARNING_AS_ERROR = "OFF",
            -- openssl3 is the dependency name; MariaDB calls the backend OPENSSL.
            WITH_SSL = ssl == "openssl3" and "OPENSSL" or ssl:upper()
        }
        local openssl = package:dep("openssl3") or package:dep("openssl")
        if openssl and not openssl:is_system() then
            configs.OPENSSL_ROOT_DIR = openssl:installdir()
        end
        for name, value in pairs(package:configs()) do
            if name ~= "ssl" and not package:extraconf("configs", name, "builtin") then
                configs["WITH_" .. name:upper()] = value and "ON" or "OFF"
            end
        end
        import("package.tools.cmake").install(package, configs)
        for _, lib in ipairs(os.files(path.join(package:installdir("lib"), "mariadb", "*.a"))) do
            os.trycp(lib, path.join(package:installdir("lib"), path.filename(lib)))
        end
        if package:config("shared") then
            os.trycp(path.join(package:installdir("lib"), "mariadb", "*.so"), package:installdir("bin"))
            os.cp(path.join(package:installdir("lib"), "mariadb", "plugin"), package:installdir("bin"))
        end
    end)
package_end()

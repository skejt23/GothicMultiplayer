-- Run with: xmake lua tests/install_layout_test.lua
-- Exercises the installer with text fixtures; does not build or launch the client.

function main()
    local projectdir = os.workingdir()
    local install = import("install_to_multiplayer_dir", {
        rootdir = path.join(projectdir, "xmake", "modules"), anonymous = true
    })
    local fixture_id = hash.uuid():gsub("[{}]", "")
    local root = path.join(projectdir, "build", "install-layout-test", fixture_id)
    local source = path.join(root, "source")
    local destination = path.join(root, "Gothic II")
    os.mkdir(source)

    local function target(filename, platform)
        return {
            targetfile = function () return path.join(source, filename) end,
            installdir = function () return destination end,
            is_plat = function (_, value) return value == (platform or "windows") end,
            name = function () return filename end
        }
    end

    local filenames = {"GMP.dll", "SDL3.dll", "BugTrap.dll", "discord_game_sdk.dll"}
    for _, filename in ipairs(filenames) do
        io.writefile(path.join(source, filename), "fixture: " .. filename)
        install(target(filename))
        assert(io.readfile(path.join(destination, "Multiplayer", "Runtime", filename)) == "fixture: " .. filename)
    end

    io.writefile(path.join(source, "GMPLauncher.exe"), "launcher fixture")
    io.writefile(path.join(source, "GMPLauncher.pdb"), "symbols fixture")
    install(target("GMPLauncher.exe"), {launcher = true})
    assert(io.readfile(path.join(destination, "Multiplayer", "GMPLauncher.exe")) == "launcher fixture")
    assert(io.readfile(path.join(destination, "Multiplayer", "GMPLauncher.pdb")) == "symbols fixture")
    assert(not os.exists(path.join(destination, "Multiplayer", "Runtime", "GMPLauncher.exe")))
    assert(not os.exists(path.join(destination, "System")))

    -- Phony/package targets supply their DLL explicitly rather than a targetfile.
    install(target("unused"), {file = path.join(source, "discord_game_sdk.dll")})
    install(target("not-built.dll", "linux"))
    local rejected = false
    try {
        function () install(target("missing.dll")) end,
        catch {function (errors)
            assert(tostring(errors):find("Required install file is missing", 1, true))
            rejected = true
        end}
    }
    assert(rejected, "Missing runtime files must fail installation")

    print("Install layout checks passed (runtime, launcher, symbols, package DLL, platform, missing file).")
    print("Fixtures: " .. root)
end

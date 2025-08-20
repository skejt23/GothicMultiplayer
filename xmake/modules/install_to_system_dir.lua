
local function _copy_file(target)
    local file = target:targetfile()
    if not file or not os.isfile(file) then
        return  -- nothing to copy
    end

    local root = target:installdir()
    if not root then
        return
    end

    -- Gothic sometimes uses "system", sometimes "System"
    local lower = path.join(root, "system")
    local upper = path.join(root, "System")

    local dst = os.isdir(lower) and lower
            or os.isdir(upper) and upper
            or lower            -- default if neither exists

    if not os.isdir(dst) then
        os.mkdir(dst)
    end

    os.cp(file, dst)
    print("Installed " .. path.filename(file) .. " → " .. dst)
end

function main(target)
    if is_plat("windows") then
        _copy_file(target)
    end
end
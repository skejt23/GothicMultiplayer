-- MIT License

-- Copyright (c) 2025 Gothic Multiplayer Team.

-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:

-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.

-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

local function _copy_file(target, options)
    local file = options.file or target:targetfile()
    if not file or not os.isfile(file) then
        raise("Required install file is missing: " .. (file or target:name()))
    end

    local root = target:installdir()
    if not root then
        return
    end

    local dst = path.join(root, "Multiplayer")
    if not options.launcher then
        dst = path.join(dst, "Runtime")
    end

    if not os.isdir(dst) then
        os.mkdir(dst)
    end

    os.cp(file, dst)
    print("Installed " .. path.filename(file) .. " → " .. dst)

    -- Also copy PDB file if it exists (for debugging)
    local pdb = path.join(path.directory(file), path.basename(file) .. ".pdb")
    if os.isfile(pdb) then
        os.cp(pdb, dst)
        print("Installed " .. path.filename(pdb) .. " → " .. dst)
    end
end

function main(target, options)
    if target:is_plat("windows") then
        _copy_file(target, options or {})
    end
end

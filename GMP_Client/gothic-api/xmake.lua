target("gothic_api")
	set_kind("static")

	-- Source that applies the runtime dynamic cast patch when loaded
	add_files("ZenGin/zGothicAPI.cpp")

	-- Public include dirs so dependents can include Gothic_UserAPI headers
	add_includedirs(".", "ZenGin/Gothic_UserAPI", {public = true})

	-- Add library directory and link Shw32.lib
	add_linkdirs("ZenGin", {public = true})
	add_links("Shw32", {public = true})

	-- Build only for Gothic II Addon
	add_defines("__G2A", {public = true})

	-- This library is linked by ClientMain; keep it off the default install
	set_default(false)


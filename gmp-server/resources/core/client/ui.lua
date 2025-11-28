------------------------------------------------------------
-- Draw: one instance, methods + properties
------------------------------------------------------------
local title = Draw.new(0, 0, "Hello UI")

-- Configure via methods
title:setPosition(0.1, 0.1)
title:setPositionPx(100, 60)

title:setText("Hello UI (methods)")
title:setFont("FONT_OLD_20_WHITE_HI.TGA")
title:setColor(255, 200, 120)
title:setAlpha(255)
title:setVisible(true)

-- Configure / tweak via simple scalar properties
title.text    = "Hello UI (properties)"
title.font    = "FONT_OLD_20_WHITE_HI.TGA"
title.alpha   = 255
title.visible = true

-- Read some values back using properties/getters
local posX, posY       = title.position       -- property (getter)
local pxX, pxY         = title.positionPx     -- property (getter)
local r, g, b          = title:getColor()     -- getter
local currentText      = title.text           -- property
local currentFont      = title.font           -- property
local currentAlpha     = title.alpha          -- property
local currentVisible   = title.visible        -- property


------------------------------------------------------------
-- Texture: one instance, methods + properties
------------------------------------------------------------
local logo = Texture.new(40, 80, 256, 256, "MENU_INGAME.TGA")

-- Configure via methods
logo:setPosition(0.05, 0.2)
logo:setPositionPx(40, 100)

logo:setSize(256, 256)
logo:setSizePx(256, 256)

logo:setRect(0, 0, 1, 1)
logo:setRectPx(0, 0, 256, 256)

logo:setColor(255, 255, 255)
logo:setAlpha(230)
logo:setVisible(true)
logo:setFile("MENU_INGAME.TGA")

logo:top()  -- bring to front


-- Read back via properties/getters
local tx, ty           = logo.position        -- property
local tpx, tpy         = logo.positionPx      -- property
local tw, th           = logo.size            -- property
local tpw, tph         = logo.sizePx          -- property
local rx, ry, rw, rh   = logo.rect            -- property
local prx, pry, prw, prh = logo.rectPx        -- property

local lr, lg, lb       = logo:getColor()
local la               = logo.alpha           -- property
local lVisible         = logo.visible         -- property
local currentFile      = logo.file            -- property
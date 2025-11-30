LOG_WARN('[hash.lua] Demonstrating hash helpers')

local string = "Hash Me"
local payloads = { string }

for _, text in ipairs(payloads) do
    LOG_INFO('[hash.lua] Input: {}', text)
    LOG_INFO('[hash.lua]  sha256-> {}', sha256(text))
    LOG_INFO('[hash.lua]  sha512-> {}', sha512(text))
end
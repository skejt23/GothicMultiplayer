LOG_WARN('[hash.lua] Demonstrating hash helpers')

local string = "Hash Me"

for _, text in ipairs(payloads) do
    LOG_INFO('[hash.lua] Input: {}', text)
    LOG_INFO('[hash.lua]  md5   -> {}', md5(text))
    LOG_INFO('[hash.lua]  sha1  -> {}', sha1(text))
    LOG_INFO('[hash.lua]  sha256-> {}', sha256(text))
    LOG_INFO('[hash.lua]  sha384-> {}', sha384(text))
    LOG_INFO('[hash.lua]  sha512-> {}', sha512(text))
end
CoreShared = CoreShared or {}

local welcome = "Welcome adventurer"
if CoreShared.GetWelcomeMessage then
  welcome = CoreShared.GetWelcomeMessage()
end

if LOG_INFO then
  LOG_INFO('[Core][Client] {}', welcome)
end

function CoreClientNotify(message)
  if LOG_INFO then
    LOG_INFO('[Core][Client] {}', message)
  end
end

CoreClientNotify("Client-side helpers initialized.")

CoreShared = CoreShared or {}

CoreShared.metadata = {
  resource = "core",
  version = "0.1.0",
  welcome_message = "Welcome to Gothic Multiplayer!"
}

function CoreShared.GetWelcomeMessage()
  return CoreShared.metadata.welcome_message
end

if LOG_INFO then
  LOG_INFO('[Core][Shared] Initialized shared helpers for resource {} (v{})', CoreShared.metadata.resource, CoreShared.metadata.version)
end

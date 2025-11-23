#include "client_resources/event_bind.h"

#include <spdlog/spdlog.h>
#include <map>
#include <string>
#include <any>

#include "client_resources/client_events.h"
#include "shared/event.h"

namespace gmp::client::lua::bindings {

namespace {

struct LuaProxyArgs {
  std::any event;
  sol::protected_function callback;
};

static std::map<std::string, std::function<void(LuaProxyArgs)>> kLuaEventProxies;

void RegisterProxies() {
  kLuaEventProxies[kEventOnInitName] = {[](LuaProxyArgs args) {
    args.callback();
  }};
  kLuaEventProxies[kEventOnExitName] = {[](LuaProxyArgs args) {
    args.callback();
  }};
  kLuaEventProxies[kEventOnPacketName] = {[](LuaProxyArgs args) {
    OnPacketEvent event = std::any_cast<OnPacketEvent>(args.event);
    args.callback(event.packet);
  }};
  kLuaEventProxies[kEventOnRenderName] = {[](LuaProxyArgs args) {
    // onRender has no arguments for now
    args.callback();
  }};
  kLuaEventProxies[kEventOnKeyDownName] = {[](LuaProxyArgs args) {
    OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
    args.callback(event.key);
  }};
  kLuaEventProxies[kEventOnKeyUpName] = {[](LuaProxyArgs args) {
    OnKeyEvent event = std::any_cast<OnKeyEvent>(args.event);
    args.callback(event.key);
  }};
  kLuaEventProxies[kEventOnPlayerCreateName] = {[](LuaProxyArgs args) {
    PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
    args.callback(event.player_id);
  }};
  kLuaEventProxies[kEventOnPlayerDestroyName] = {[](LuaProxyArgs args) {
    PlayerLifecycleEvent event = std::any_cast<PlayerLifecycleEvent>(args.event);
    args.callback(event.player_id);
  }};
}

std::optional<std::function<void(LuaProxyArgs)>> GetProxy(std::string event_name) {
  return kLuaEventProxies[event_name];
}

} // namespace

void BindEvents(sol::state& lua) {
  RegisterProxies();

  // Ensure events are registered in EventManager
  EventManager::Instance().RegisterEvent(kEventOnInitName);
  EventManager::Instance().RegisterEvent(kEventOnExitName);
  EventManager::Instance().RegisterEvent(kEventOnPacketName);
  EventManager::Instance().RegisterEvent(kEventOnRenderName);
  EventManager::Instance().RegisterEvent(kEventOnKeyDownName);
  EventManager::Instance().RegisterEvent(kEventOnKeyUpName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCreateName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDestroyName);

  lua.new_usertype<gmp::client::Packet>("Packet", sol::constructors<gmp::client::Packet()>(), 
                                        "reset", &gmp::client::Packet::reset, "send", &gmp::client::Packet::send,
                                        "writeBool", &gmp::client::Packet::writeBool, "writeInt8", &gmp::client::Packet::writeInt8,
                                        "writeUInt8", &gmp::client::Packet::writeUInt8, "writeInt16", &gmp::client::Packet::writeInt16,
                                        "writeUInt16", &gmp::client::Packet::writeUInt16, "writeInt32", &gmp::client::Packet::writeInt32,
                                        "writeUInt32", &gmp::client::Packet::writeUInt32, "writeFloat", &gmp::client::Packet::writeFloat,
                                        "writeString", &gmp::client::Packet::writeString, "writeBlob", &gmp::client::Packet::writeBlob,
                                        "readBool", &gmp::client::Packet::readBool, "readInt8", &gmp::client::Packet::readInt8,
                                        "readUInt8", &gmp::client::Packet::readUInt8, "readInt16", &gmp::client::Packet::readInt16,
                                        "readUInt16", &gmp::client::Packet::readUInt16, "readInt32", &gmp::client::Packet::readInt32,
                                        "readUInt32", &gmp::client::Packet::readUInt32, "readFloat", &gmp::client::Packet::readFloat,
                                        "readString", &gmp::client::Packet::readString, "readBlob", &gmp::client::Packet::readBlob);

  lua.set_function("addEventHandler", [](std::string event_name, sol::protected_function lua_callback) -> bool {
    SPDLOG_TRACE("addEventHandler({})", event_name);

    auto proxy = GetProxy(event_name);
    if (!proxy) {
      SPDLOG_ERROR("addEventHandler: event with name {} doesn't exist!", event_name);
      return false;
    }

    auto callback = [proxy, lua_callback](std::any event) {
      LuaProxyArgs args;
      args.event = event;
      args.callback = lua_callback;
      (*proxy)(args);
    };

    return EventManager::Instance().SubscribeToEvent(event_name, callback);
  });
}

void ResetEvents() {
  EventManager::Instance().Reset();
  EventManager::Instance().RegisterEvent(kEventOnInitName);
  EventManager::Instance().RegisterEvent(kEventOnExitName);
  EventManager::Instance().RegisterEvent(kEventOnPacketName);
  EventManager::Instance().RegisterEvent(kEventOnRenderName);
  EventManager::Instance().RegisterEvent(kEventOnKeyDownName);
  EventManager::Instance().RegisterEvent(kEventOnKeyUpName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerCreateName);
  EventManager::Instance().RegisterEvent(kEventOnPlayerDestroyName);
}

} // namespace gmp::client::lua::bindings

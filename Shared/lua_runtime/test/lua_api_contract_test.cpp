#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <thread>

#include "Shared/lua_runtime/bind_helpers.h"
#include "Shared/lua_runtime/lua_event_contract.h"
#include "Shared/lua_runtime/lua_events.h"
#include "Shared/lua_runtime/lua_math.h"
#include "Shared/lua_runtime/lua_utility.h"

namespace {

struct EventCleanup {
  gmp::lua::LuaEventBindings& bindings;
  ~EventCleanup() {
    bindings.Reset();
    EventManager::Instance().Reset();
  }
};

TEST(LuaApiContract, SharedEventsWorkWithoutServerResourceHooks) {
  sol::state lua;
  lua.open_libraries(sol::lib::base);
  gmp::lua::LuaEventProxyMap proxies;
  gmp::lua::LuaEventBindings events(proxies);
  EventCleanup cleanup{events};
  EventManager::Instance().Reset();
  EventManager::Instance().RegisterEvent("builtin_contract");
  proxies["builtin_contract"] = [](gmp::lua::LuaEventProxyArgs args) { args.callback(std::any_cast<int>(args.event)); };
  gmp::lua::BindCommonEvents(lua, events);
  const auto result = lua.safe_script(R"(
    assert(addEvent('shared_contract'))
    local received = 0
    local function callback(context, ...)
      assert(not context.remote and context.sourceId == nil and context.senderPlayerId == nil)
      assert(select('#', ...) == 3)
      local a, b, c = ...
      assert(a == 42 and b == nil and c == nil)
      received = received + 1
    end
    assert(addEventHandler('shared_contract', callback))
    assert(callEvent('shared_contract', nil, 42, nil, nil) and received == 1)
    setEventEnabled('shared_contract', false)
    assert(not callEvent('shared_contract', nil, 42, nil, nil) and received == 1)
    setEventEnabled('shared_contract', true)
    assert(removeEventHandler('shared_contract', callback))
    assert(not removeEventHandler('shared_contract', callback))
    removeEvent('shared_contract')
    assert(not callEvent('shared_contract', nil))

    assert(addEventHandler('builtin_contract', function(value)
      assert(value == 7 and not isEventCancelled())
      setEventValue(123)
      cancelEvent()
      assert(isEventCancelled())
    end))
    removeEvent('builtin_contract')
  )",
                                      sol::script_pass_on_error);
  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
  const auto dispatched = EventManager::Instance().DispatchEvent("builtin_contract", 7);
  EXPECT_TRUE(dispatched.dispatched);
  EXPECT_TRUE(dispatched.cancelled);
  EXPECT_EQ(dispatched.value, 123);
}

TEST(LuaApiContract, AngleUnitsAndHorizontalPlaneAreExplicit) {
  sol::state lua;
  lua.open_libraries(sol::lib::base, sol::lib::math);
  ::lua::bindings::BindMath(lua);
  const auto result = lua.safe_script(R"(
    local degrees = Quat.new()
    degrees:fromEulerDegrees(Vec3.new(20, 35, -10))
    local radians = Quat.new()
    radians:fromEulerRadians(Vec3.new(math.rad(20), math.rad(35), math.rad(-10)))
    assert(math.abs(math.abs(Quat.dot(degrees, radians)) - 1) < 0.00001)
    local roundTrip = Quat.new()
    roundTrip:fromEulerDegrees(degrees:toEulerDegrees())
    assert(math.abs(math.abs(Quat.dot(degrees, roundTrip)) - 1) < 0.00001)

    local axis = Quat.new()
    axis:fromAxisAngleDegrees(Vec3.new(0, 1, 0), 90)
    assert(math.abs(axis:toAxisAngleRadians().w - math.pi / 2) < 0.00001)
    assert(math.abs(axis:toAxisAngleDegrees().w - 90) < 0.00001)

    local a, b = Mat4.new(), Mat4.new()
    a:postRotateYDegrees(90)
    b:postRotateYRadians(math.pi / 2)
    local v = Vec3.new(1, 0, 0)
    assert(a:rotate(v):distance(b:rotate(v)) < 0.00001)
    local origin, point = Vec3.new(), Vec3.new(3, 100, 4)
    assert(origin:distanceHorizontal(point) == 5)
    assert(math.abs(origin:distance2d(point) - math.sqrt(10009)) < 0.0001)
  )",
                                      sol::script_pass_on_error);
  if (!result.valid()) {
    const sol::error error = result;
    FAIL() << error.what();
  }
}

TEST(LuaApiContract, EventMetadataIsValidatedAndSnapshotsAreIndependent) {
  sol::state lua;
  gmp::lua::EventMetadata metadata;
  metadata.source_element = 17;
  metadata.sender_player_id = 23;
  metadata.remote = true;
  sol::table first = gmp::lua::MakeEventContext(lua, metadata);
  first["senderPlayerId"] = 999;
  sol::table second = gmp::lua::MakeEventContext(lua, metadata);
  EXPECT_EQ(second["senderPlayerId"].get<int>(), 23);
  EXPECT_EQ(second["sourceId"].get<int>(), 17);
  EXPECT_TRUE(second["remote"].get<bool>());

  for (double invalid : {0.0, -1.0, 1.5, 4294967296.0, std::numeric_limits<double>::infinity()}) {
    EXPECT_THROW(gmp::lua::ReadEventId(sol::make_object(lua, invalid)), sol::error);
  }
  EXPECT_EQ(gmp::lua::ReadEventId(sol::make_object(lua, 4294967295.0)), 4294967295u);
  EXPECT_THROW(gmp::lua::ReadEventId(sol::make_object(lua, "23")), sol::error);
  EXPECT_FALSE(gmp::lua::ReadEventSource(sol::make_object(lua, sol::nil)).has_value());
  EXPECT_THROW(gmp::lua::ReadEventSource(sol::make_object(lua, 42)), sol::error);
  sol::table options = lua.create_table_with("sourceId", 17);
  EXPECT_EQ(gmp::lua::ReadEventSource(sol::make_object(lua, options)), 17u);
}

TEST(LuaApiContract, TimerCountTracksRemainingExecutions) {
  sol::state lua;
  TimerManager timers;
  lua.open_libraries(sol::lib::base);
  ::lua::bindings::BindTimers(lua, timers);
  ASSERT_TRUE(lua.safe_script(R"(
    calls = 0
    timer = setTimer(function() calls = calls + 1 end, 0, 2)
    assert(getTimerRemainingExecutions(timer) == 2)
  )",
                              sol::script_pass_on_error)
                  .valid());
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  timers.ProcessTimers();
  ASSERT_TRUE(lua.safe_script("assert(calls == 1 and getTimerRemainingExecutions(timer) == 1)", sol::script_pass_on_error).valid());
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  timers.ProcessTimers();
  ASSERT_TRUE(lua.safe_script("assert(calls == 2 and getTimerRemainingExecutions(timer) == nil)", sol::script_pass_on_error).valid());
}

TEST(LuaApiContract, TimeSnapshotsUseMinuteConsistently) {
  sol::state lua;
  const auto snapshot = ::lua::bind_helpers::MakeTimeTable(sol::this_state{lua.lua_state()}, 12, 34);
  const auto parsed = ::lua::bind_helpers::ReadTimeTable(snapshot);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->first, 12);
  EXPECT_EQ(parsed->second, 34);
  EXPECT_EQ(snapshot.as<sol::table>()["min"].get<sol::object>().get_type(), sol::type::nil);
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

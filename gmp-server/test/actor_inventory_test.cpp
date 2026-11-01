#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>

#include "Script.h"
#include "character.h"
#include "game_server.h"
#include "inventory_sync.h"
#include "item_ground_manager.h"
#include "native_event_packets.h"
#include "packets.h"
#include "server_events.h"
#include "shared/event.h"
#include "summon_registry.h"
#include "summon_spells.h"
#include "transformation_spells.h"
#include "weapon_skills.h"

TEST(WeaponSkills, TrainingTiersUseGothicThresholds) {
  for (const int percentage : {-1, 0, 29}) EXPECT_EQ(gmp::WeaponSkillTier(percentage), 0);
  for (const int percentage : {30, 59}) EXPECT_EQ(gmp::WeaponSkillTier(percentage), 1);
  for (const int percentage : {60, 100, 101}) EXPECT_EQ(gmp::WeaponSkillTier(percentage), 2);
}

TEST(WeaponSkills, ServerPersistsMatchingTiersAcrossUpgradesAndDowngrades) {
  GameServer server;
  const auto id = server.GetPlayerManager().AddPlayer(70001, "Training", 2);
  ASSERT_NE(id, 0u);
  auto& actor = server.GetCharacter(id)->get();
  for (int skill = 1; skill <= 4; ++skill) {
    for (const int percentage : {0, 29, 30, 59, 60, 100, 59, 29, -10, 150}) {
      ASSERT_TRUE(server.SetCharacterSkillWeapon(id, skill, percentage));
      const auto expected = std::clamp(percentage, 0, 100);
      EXPECT_EQ(actor.weapon_skills.at(skill), expected);
      EXPECT_EQ(actor.talents.at(skill), gmp::WeaponSkillTier(expected));
    }
  }
  const auto skills = actor.weapon_skills;
  const auto talents = actor.talents;
  EXPECT_FALSE(server.SetCharacterSkillWeapon(id, 0, 60));
  EXPECT_FALSE(server.SetCharacterSkillWeapon(id, 5, 60));
  EXPECT_EQ(actor.weapon_skills, skills);
  EXPECT_EQ(actor.talents, talents);
  ASSERT_TRUE(server.SetCharacterTalent(id, 1, 0));
  EXPECT_EQ(actor.talents.at(1), 0);
  ASSERT_TRUE(server.SetCharacterSkillWeapon(id, 1, 60));
  EXPECT_EQ(actor.talents.at(1), 2);
  ASSERT_TRUE(server.SetCharacterTalent(id, 1, 99));
  EXPECT_EQ(actor.talents.at(1), 2);
}

TEST(ActorInventory, InsufficientDropLeavesOwnershipUnchanged) {
  Character actor;
  ASSERT_TRUE(actor.AddInventory("ITRW_ARROW", 3));
  EXPECT_FALSE(actor.RemoveInventory("ITRW_ARROW", 4));
  EXPECT_EQ(actor.InventoryCount("ITRW_ARROW"), 3);
  EXPECT_FALSE(actor.RemoveInventory("UNKNOWN", 1));
  EXPECT_FALSE(actor.RemoveInventory("ITRW_ARROW", -1));
  EXPECT_TRUE(actor.RemoveInventory("ITRW_ARROW", 3));
  EXPECT_FALSE(actor.inventory.contains("ITRW_ARROW"));
}

TEST(ActorInventory, StackOverflowCannotCommitPartialTake) {
  Character actor;
  ASSERT_TRUE(actor.AddInventory("ITMI_GOLD", std::numeric_limits<std::int32_t>::max() - 1));
  EXPECT_FALSE(actor.AddInventory("ITMI_GOLD", 2));
  EXPECT_EQ(actor.InventoryCount("ITMI_GOLD"), std::numeric_limits<std::int32_t>::max() - 1);
  EXPECT_TRUE(actor.AddInventory("ITMI_GOLD", 1));
}

TEST(ActorInventory, NativeDeclarationsValidateNamesAndQuantitiesWithoutCatalogs) {
  EXPECT_TRUE(ValidateNativeInventoryDeclaration({{"CUSTOM_MOD_ITEM", 2}, {"ITMI_GOLD", 100}}));
  EXPECT_TRUE(ValidateNativeInventoryDeclaration({}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{"", 1}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{"INVALID NAME", 1}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{std::string(256, 'A'), 1}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{"CUSTOM_MOD_ITEM", 0}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{"CUSTOM_MOD_ITEM", -1}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration({{"CUSTOM_MOD_ITEM", 1}, {"CUSTOM_MOD_ITEM", 2}}));
  EXPECT_FALSE(ValidateNativeInventoryDeclaration(std::vector<InventoryEntry>(kMaxInventoryEntries + 1, {"ITEM", 1})));
  std::vector<InventoryEntry> oversized_names;
  for (int index = 0; index < 2000; ++index) {
    const auto suffix = std::to_string(index);
    oversized_names.push_back({std::string(255 - suffix.size(), 'A') + suffix, 1});
  }
  EXPECT_FALSE(ValidateNativeInventoryDeclaration(oversized_names));
}

TEST(GroundTransactions, CallbackDestructionDoesNotTransferReservationToReplacement) {
  ItemGroundManager ground;
  ItemGroundManager::CreateOptions options;
  options.instance = "ITMI_GOLD";
  const auto first_id = ground.Create(options).id;
  ASSERT_TRUE(ground.Reserve(first_id, 1));
  EXPECT_FALSE(ground.Reserve(first_id, 2));
  ground.ReleaseReservation(first_id, 2);
  EXPECT_FALSE(ground.Reserve(first_id, 2));
  ASSERT_TRUE(ground.Destroy(first_id));
  const auto replacement_id = ground.Create(options).id;
  EXPECT_NE(first_id, replacement_id);
  EXPECT_EQ(ground.Get(first_id), nullptr);
  EXPECT_TRUE(ground.Reserve(replacement_id, 2));
  ground.Clear();
  EXPECT_GT(ground.Create(options).id, replacement_id);
}

class ServerTestData : public ::testing::Test {
protected:
  void SetUp() override {
    const auto directory = std::filesystem::current_path() / "build" / "actor-inventory-tests";
    std::filesystem::create_directories(directory);
    path = directory / ("gmp-server-test-data-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
  }
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
  void Write(const std::string& source) {
    std::ofstream output(path, std::ios::binary);
    output << source;
  }
  std::filesystem::path path;
};

class ServerActorValidation : public ServerTestData {
protected:
  void SetUp() override {
    ServerTestData::SetUp();
    // Initialize actor state without starting network listeners or resources.
    first = server.GetPlayerManager().AddPlayer(70001, "HostOne", 2);
    second = server.GetPlayerManager().AddPlayer(70002, "HostTwo", 2);
    ASSERT_NE(first, 0u);
    ASSERT_NE(second, 0u);
    npc_id = server.GetNpcManager().Create("Mage", "PC_HERO", "NEWWORLD.ZEN", std::max(2u, server.GetMaxSlots()));
    ASSERT_NE(npc_id, 0u);
    for (const auto id : {first, second, npc_id}) {
      auto& actor = server.GetCharacter(id)->get();
      actor.is_ingame = 1;
      actor.world = "NEWWORLD.ZEN";
      actor.health = actor.max_health = actor.mana = actor.max_mana = 100;
      actor.strength = 10;
      actor.state.health_points = actor.state.mana_points = 100;
      actor.state.life_state = PLAYER_LIFE_ALIVE;
      actor.state.weapon_mode = 1;
    }
    for (const auto id : {first, second}) {
      auto& player = server.GetPlayerManager().GetPlayer(id)->get();
      player.npc_world_ready = true;
      player.spawned_players.insert(npc_id);
      player.spawned_players.insert(first == id ? second : first);
    }
    auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
    npc.streamed_by_players = {first, second};
    ASSERT_TRUE(server.SetNpcHostPlayer(npc_id, first));
    ASSERT_TRUE(npc.AddInventory("ITPO_TEST", 3));
    ASSERT_TRUE(npc.AddInventory("ITRU_TEST", 1));
  }
  template <typename T>
  void Send(Net::ConnectionHandle connection, T packet) {
    std::vector<std::uint8_t> bytes;
    using Adapter = bitsery::OutputBufferAdapter<std::vector<std::uint8_t>>;
    bytes.resize(bitsery::quickSerialization<Adapter>(bytes, packet));
    EXPECT_TRUE(server.HandlePacket(connection, bytes.data(), static_cast<std::uint32_t>(bytes.size())));
  }
  GameServer server;
  std::uint32_t first{0}, second{0}, npc_id{0};
  PlayerHitReportPacket Hit(Character& attacker, Character& victim, std::uint32_t identity, std::uint32_t damage_type = 2, bool nonlethal = false) {
    PlayerHitReportPacket hit{};
    hit.packet_type = Net::PT_PLAYER_HIT;
    hit.attacker_id = attacker.player_id;
    hit.actor_generation = attacker.actor_generation;
    hit.victim_id = victim.player_id;
    hit.victim_generation = victim.actor_generation;
    hit.attack_id = identity;
    hit.command_revision = attacker.command_revision;
    hit.damage = 10;
    hit.damage_type = damage_type;
    hit.dont_kill = nonlethal;
    return hit;
  }
  void ApproveProjectile(Character& attacker, std::uint32_t identity) {
    attacker.approved_projectiles.push_back(
        {identity, 10, 32, 3000, std::chrono::steady_clock::now() + std::chrono::seconds(5), {}, 1, attacker.state.position});
  }
  PlayerHitReportPacket FinishingMove(Character& attacker, Character& victim, std::uint32_t identity = 1) {
    EXPECT_TRUE(attacker.AddInventory("ITMW_TEST", 1));
    attacker.state.melee_weapon_instance = "ITMW_TEST";
    attacker.state.weapon_mode = 3;
    victim.health = victim.state.health_points = 1;
    victim.flags |= PlayerManager::PL_UNCONCIOUS;
    victim.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
    auto hit = Hit(attacker, victim, identity, 4, true);
    hit.finishing_move = true;
    return hit;
  }
  PlayerTeleportPacket Teleport(Character& actor, std::uint32_t identity = 1) {
    PlayerTeleportPacket packet;
    packet.player_id = actor.player_id;
    packet.actor_generation = actor.actor_generation;
    packet.command_revision = actor.command_revision;
    if (auto npc = server.GetNpcManager().GetNpc(actor.player_id))
      packet.control_epoch = npc->get().control_epoch;
    packet.action_id = identity;
    packet.position = {100.0f, 200.0f, 300.0f};
    packet.normal = {1.0f, 0.0f, 0.0f};
    packet.destination = "NW_TELEPORT_TEST";
    return packet;
  }
};

class ServerInstanceValidation : public ServerActorValidation {
protected:
  PlayerInstanceReadyPacket Defaults(std::uint32_t id) {
    const auto& actor = server.GetCharacter(id)->get();
    PlayerInstanceReadyPacket packet;
    packet.request_revision = actor.instance_change->request_revision;
    auto& out = packet.defaults;
    out.packet_type = Net::PT_PLAYER_SPAWN;
    out.player_id = id;
    out.actor_generation = actor.actor_generation;
    out.instance = actor.instance;
    out.health = out.max_health = 750;
    out.mana = out.max_mana = 250;
    out.strength = 90;
    out.dexterity = 80;
    out.level = 30;
    out.body_model = "HUM_BODY_NAKED0";
    out.head_model = "HUM_HEAD_PONY";
    out.body_texture = 4;
    out.inventory = {{"ITAR_TEST", 1}, {"ITPO_TEST", 2}};
    out.equipped_armor_instance = "ITAR_TEST";
    out.weapon_skills = {{1, 60}};
    out.talents = {{1, 2}};
    return packet;
  }
};

TEST_F(ServerInstanceValidation, ConstructorReplacesOldStatsAppearanceInventoryAndEquipmentOnce) {
  auto& actor = server.GetCharacter(first)->get();
  actor.instance = "PC_HERO";
  actor.body_model = "OLD_BODY";
  actor.head_model = "OLD_HEAD";
  ASSERT_TRUE(actor.AddInventory("ITRU_TEST", 3));
  const auto position = actor.state.position;
  ASSERT_TRUE(server.SetCharacterInstance(first, "PC_MAGE_OW"));
  auto ready = Defaults(first);
  ready.defaults.position = {9999.0f, 9999.0f, 9999.0f};
  Send(70002, ready);
  ASSERT_TRUE(actor.instance_change);
  EXPECT_EQ(actor.max_health, 100);
  Send(70001, ready);
  EXPECT_FALSE(actor.instance_change);
  EXPECT_EQ(actor.max_health, 750);
  EXPECT_EQ(actor.health, 750);
  EXPECT_EQ(actor.max_mana, 250);
  EXPECT_EQ(actor.mana, 250);
  EXPECT_EQ(actor.strength, 90);
  EXPECT_EQ(actor.body_model, "HUM_BODY_NAKED0");
  EXPECT_EQ(actor.head_model, "HUM_HEAD_PONY");
  EXPECT_EQ(actor.InventoryCount("ITRU_TEST"), 0);
  EXPECT_EQ(actor.InventoryCount("ITPO_TEST"), 2);
  EXPECT_EQ(actor.state.equipped_armor_instance, "ITAR_TEST");
  EXPECT_EQ(actor.state.position, position);
  const auto sequence = actor.state_sequence;
  ready.defaults.health = ready.defaults.max_health = 50000;
  Send(70001, ready);
  EXPECT_EQ(actor.max_health, 750);
  EXPECT_EQ(actor.state_sequence, sequence);
}

TEST_F(ServerInstanceValidation, ScriptedChangesAfterRequestOverrideConstructorDefaults) {
  auto& actor = server.GetCharacter(first)->get();
  ASSERT_TRUE(server.SetCharacterInstance(first, "CH"));
  auto ready = Defaults(first);
  ASSERT_TRUE(server.SetCharacterMaxHealth(first, 1200));
  ASSERT_TRUE(server.SetCharacterHealth(first, 900));
  ASSERT_TRUE(server.SetCharacterMaxMana(first, 0));
  ASSERT_TRUE(server.SetCharacterSkillWeapon(first, 1, 30));
  ASSERT_TRUE(server.SetCharacterInvulnerable(first, true));
  ASSERT_TRUE(server.GiveItem(first, "ITPO_TEST", 1));
  Send(70001, ready);
  EXPECT_EQ(actor.max_health, 1200);
  EXPECT_EQ(actor.health, 900);
  EXPECT_EQ(actor.max_mana, 0);
  EXPECT_EQ(actor.mana, 0);
  EXPECT_EQ(actor.weapon_skills.at(1), 30);
  EXPECT_EQ(actor.talents.at(1), 1);
  EXPECT_TRUE(actor.invulnerable);
  EXPECT_EQ(actor.InventoryCount("ITPO_TEST"), 3);
}

TEST_F(ServerInstanceValidation, SupersededCompletionCannotReplaceNewerRequestAndFailureRestoresPreviousInstance) {
  auto& actor = server.GetCharacter(first)->get();
  actor.instance = "PC_HERO";
  ASSERT_TRUE(server.SetCharacterInstance(first, "WOLF"));
  auto stale = Defaults(first);
  ASSERT_TRUE(server.SetCharacterInstance(first, "SCAVENGER"));
  const auto revision = actor.instance_change->request_revision;
  Send(70001, stale);
  ASSERT_TRUE(actor.instance_change);
  EXPECT_EQ(actor.instance_change->request_revision, revision);
  auto failed = Defaults(first);
  failed.success = false;
  Send(70001, failed);
  EXPECT_FALSE(actor.instance_change);
  EXPECT_EQ(actor.instance, "PC_HERO");
  EXPECT_EQ(actor.max_health, 100);
}

TEST_F(ServerInstanceValidation, InvalidInventoryCannotGrantItemsOrLeaveInitializationStuck) {
  auto& actor = server.GetCharacter(first)->get();
  actor.instance = "PC_HERO";
  ASSERT_TRUE(server.SetCharacterInstance(first, "CH"));
  auto ready = Defaults(first);
  ready.defaults.inventory.push_back({"INVALID NAME", 1});
  Send(70001, ready);
  EXPECT_FALSE(actor.instance_change);
  EXPECT_EQ(actor.instance, "PC_HERO");
  EXPECT_EQ(actor.max_health, 100);
  EXPECT_TRUE(actor.inventory.empty());
}

TEST_F(ServerInstanceValidation, OnlyTheCurrentNpcHostCanCompleteTheIssuedConstructor) {
  ASSERT_TRUE(server.SetCharacterInstance(npc_id, "CH"));
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  ASSERT_TRUE(npc.instance_change);
  ASSERT_TRUE(server.SetNpcHostPlayer(npc_id, first));
  auto ready = Defaults(npc_id);
  ready.control_epoch = npc.control_epoch;
  ASSERT_TRUE(server.SetNpcHostPlayer(npc_id, second));
  Send(70001, ready);
  ASSERT_TRUE(npc.instance_change);
  EXPECT_EQ(npc.max_health, 100);
  ready.control_epoch = npc.control_epoch;
  Send(70002, ready);
  EXPECT_FALSE(npc.instance_change);
  EXPECT_EQ(npc.max_health, 750);
}

TEST_F(ServerActorValidation, NativeTeleportCommitsOnceAndBlocksEarlierMovementWithoutChangingResources) {
  auto& actor = server.GetCharacter(first)->get();
  const auto teleport = Teleport(actor);
  const auto old_revision = actor.command_revision;
  const auto inventory = actor.inventory;
  Send(70001, teleport);
  EXPECT_EQ(actor.state.position, teleport.position);
  EXPECT_EQ(actor.state.nrot, teleport.normal);
  EXPECT_NE(actor.command_revision, old_revision);
  EXPECT_EQ(actor.health, 100);
  EXPECT_EQ(actor.mana, 100);
  EXPECT_EQ(actor.inventory, inventory);
  const auto sequence = actor.state_sequence;
  const auto revision = actor.command_revision;
  Send(70001, teleport);
  EXPECT_EQ(actor.command_revision, revision);
  EXPECT_EQ(actor.state_sequence, sequence);

  PlayerStateUpdatePacket old_movement{};
  old_movement.packet_type = Net::PT_ACTUAL_STATISTICS;
  old_movement.actor_generation = actor.actor_generation;
  old_movement.command_revision = old_revision;
  old_movement.state_sequence = 200;
  old_movement.state = actor.state;
  old_movement.state.position = glm::vec3(0.0f);
  Send(70001, old_movement);
  EXPECT_EQ(actor.state.position, teleport.position);
}

TEST_F(ServerActorValidation, TeleportingBothPlayersRestoresMutualStreamingRegardlessOfAppearanceOrCallOrder) {
  auto& picked = server.GetCharacter(first)->get();
  auto& defaults = server.GetCharacter(second)->get();
  picked.body_model = "HUM_BODY_BABE0";
  picked.body_texture = 8;
  picked.head_model = "HUM_HEAD_FIGHTER";
  picked.head_texture = 26;
  picked.overlays = {"HUMANS_MILITIA.MDS"};
  defaults.body_model = "HUM_BODY_NAKED0";
  defaults.body_texture = 9;
  defaults.head_model = "HUM_HEAD_PONY";
  defaults.head_texture = 18;
  defaults.overlays.clear();
  ASSERT_TRUE(server.SetStreamerRadius(1000));
  ASSERT_TRUE(server.SetCharacterPosition(first, {0.0f, 0.0f, 0.0f}));
  ASSERT_TRUE(picked.streamed_by_players.count(second));
  ASSERT_TRUE(defaults.streamed_by_players.count(first));
  float destination = 5000.0f;
  for (const auto leader : {first, second}) {
    const auto follower = leader == first ? second : first;
    ASSERT_TRUE(server.SetCharacterPosition(leader, {destination, 0.0f, 0.0f}));
    EXPECT_FALSE(picked.streamed_by_players.count(second));
    EXPECT_FALSE(defaults.streamed_by_players.count(first));
    EXPECT_FALSE(server.GetPlayerManager().GetPlayer(first)->get().spawned_players.count(second));
    EXPECT_FALSE(server.GetPlayerManager().GetPlayer(second)->get().spawned_players.count(first));
    ASSERT_TRUE(server.SetCharacterPosition(follower, {destination + 100.0f, 0.0f, 0.0f}));
    EXPECT_TRUE(picked.streamed_by_players.count(second));
    EXPECT_TRUE(defaults.streamed_by_players.count(first));
    EXPECT_TRUE(server.GetPlayerManager().GetPlayer(first)->get().spawned_players.count(second));
    EXPECT_TRUE(server.GetPlayerManager().GetPlayer(second)->get().spawned_players.count(first));
    EXPECT_EQ(picked.body_model, "HUM_BODY_BABE0");
    EXPECT_EQ(picked.body_texture, 8);
    EXPECT_EQ(picked.head_model, "HUM_HEAD_FIGHTER");
    EXPECT_EQ(picked.head_texture, 26);
    EXPECT_EQ(picked.overlays, std::vector<std::string>{"HUMANS_MILITIA.MDS"});
    EXPECT_EQ(defaults.body_model, "HUM_BODY_NAKED0");
    EXPECT_EQ(defaults.body_texture, 9);
    EXPECT_EQ(defaults.head_model, "HUM_HEAD_PONY");
    EXPECT_EQ(defaults.head_texture, 18);
    EXPECT_TRUE(defaults.overlays.empty());
    destination += 5000.0f;
  }
}

TEST_F(ServerActorValidation, DelayedNativeTeleportCannotReplaceANewerScriptedTransform) {
  auto& actor = server.GetCharacter(first)->get();
  const auto teleport = Teleport(actor);
  const glm::vec3 scripted_position{500.0f, 200.0f, 300.0f};
  ASSERT_TRUE(server.SetCharacterPosition(first, scripted_position));
  Send(70001, teleport);
  EXPECT_EQ(actor.state.position, scripted_position);
  EXPECT_FALSE(actor.last_teleport_action_id);
  auto current = Teleport(actor);
  ASSERT_TRUE(server.SetCharacterAngle(first, 0.5f));
  const auto heading = actor.state.nrot;
  Send(70001, current);
  EXPECT_EQ(actor.state.position, scripted_position);
  EXPECT_EQ(actor.state.nrot, heading);
  EXPECT_FALSE(actor.last_teleport_action_id);
  current = Teleport(actor);
  Send(70001, current);
  EXPECT_EQ(actor.state.position, current.position);
}

TEST_F(ServerActorValidation, NativeTeleportSurvivesAnUnacknowledgedManaUpdate) {
  auto& actor = server.GetCharacter(first)->get();
  const auto teleport = Teleport(actor);
  ASSERT_TRUE(server.SetCharacterMana(first, 80));
  EXPECT_NE(actor.command_revision, teleport.command_revision);
  Send(70001, teleport);
  EXPECT_EQ(actor.state.position, teleport.position);
  EXPECT_EQ(actor.mana, 80);
}

TEST_F(ServerActorValidation, NativeTeleportRejectsOtherPlayersStaleGenerationsAndInvalidDestinations) {
  auto& actor = server.GetCharacter(first)->get();
  const auto position = actor.state.position;
  const auto revision = actor.command_revision;
  Send(70002, Teleport(actor));
  auto invalid = Teleport(actor);
  invalid.actor_generation++;
  Send(70001, invalid);
  invalid = Teleport(actor);
  invalid.position.x = std::numeric_limits<float>::quiet_NaN();
  Send(70001, invalid);
  invalid = Teleport(actor);
  invalid.destination.clear();
  Send(70001, invalid);
  invalid = Teleport(actor);
  invalid.normal = glm::vec3(0.0f);
  Send(70001, invalid);
  EXPECT_EQ(actor.state.position, position);
  EXPECT_EQ(actor.command_revision, revision);
  EXPECT_FALSE(actor.last_teleport_action_id);
  Send(70001, Teleport(actor));
  EXPECT_NE(actor.command_revision, revision);
}

TEST_F(ServerActorValidation, NativeNpcTeleportRequiresCurrentHostAndHostChangeResetsDuplicateTracking) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  const auto original = npc.state.position;
  auto teleport = Teleport(npc, 100);
  Send(70002, teleport);
  EXPECT_EQ(npc.state.position, original);
  Send(70001, teleport);
  EXPECT_EQ(npc.state.position, teleport.position);
  ASSERT_TRUE(server.SetNpcHostPlayer(npc_id, second));
  const auto revision = npc.command_revision;
  teleport.action_id = 101;
  Send(70001, teleport);
  Send(70002, teleport);  // Previous host's control epoch is invalid for either sender.
  EXPECT_EQ(npc.command_revision, revision);
  teleport = Teleport(npc, 1);
  teleport.position.x += 50.0f;
  Send(70002, teleport);
  EXPECT_EQ(npc.state.position, teleport.position);
  EXPECT_NE(npc.command_revision, revision);
}

TEST_F(ServerActorValidation, NativeTeleportTriggerCanMoveAnUnconsciousPlayerWithoutRecoveringThem) {
  auto& actor = server.GetCharacter(first)->get();
  actor.health = actor.state.health_points = 1;
  actor.flags |= PlayerManager::PL_UNCONCIOUS;
  actor.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  const auto teleport = Teleport(actor);
  Send(70001, teleport);
  EXPECT_EQ(actor.state.position, teleport.position);
  EXPECT_EQ(actor.health, 1);
  EXPECT_EQ(actor.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
}

class ServerTransformationValidation : public ServerActorValidation {
protected:
  void SetUp() override {
    ServerActorValidation::SetUp();
    auto& actor = server.GetCharacter(first)->get();
    actor.instance = "PC_HERO";
    actor.health = actor.state.health_points = 625;
    actor.max_health = 1000;
    actor.mana = actor.state.mana_points = 200;
    actor.max_mana = 300;
    actor.strength = 77;
    actor.dexterity = 66;
    actor.level = 7;
    actor.exp = 1234;
    actor.learn_points = 12;
    actor.state.nrot = {0.0f, 0.0f, 1.0f};
    actor.state.equipped_armor_instance = "ITAR_TEST";
    ASSERT_TRUE(actor.AddInventory("ITAR_TEST", 1));
    ASSERT_TRUE(actor.AddInventory("ITSC_TRF51", 2));
    for (int spell = 47; spell <= 58; ++spell) ASSERT_TRUE(actor.AddInventory("ITRU_TRF" + std::to_string(spell), 1));
  }
  CastSpellPacket Transform(std::uint16_t spell = 51, std::uint32_t identity = 1, bool scroll = false) {
    const auto& actor = server.GetCharacter(first)->get();
    CastSpellPacket packet{};
    packet.packet_type = Net::PT_CASTSPELL;
    packet.caster_id = first;
    packet.actor_generation = actor.actor_generation;
    packet.command_revision = actor.command_revision;
    packet.action_id = identity;
    packet.item_instance = scroll ? "ITSC_TRF51" : "ITRU_TRF" + std::to_string(spell);
    packet.scroll = scroll;
    packet.spell_id = spell;
    packet.transformation_succeeded = true;
    return packet;
  }
  PlayerTransformationPacket Return(std::uint32_t identity = 1) {
    const auto& actor = server.GetCharacter(first)->get();
    PlayerTransformationPacket packet;
    packet.player_id = first;
    packet.actor_generation = actor.actor_generation;
    packet.command_revision = actor.command_revision;
    packet.cast_id = identity;
    return packet;
  }
};

TEST_F(ServerTransformationValidation, AllNativeFormsRoundTripWithoutChangingHumanAttributesOrRoundingHealth) {
  for (std::uint16_t spell = 47; spell <= 58; ++spell) {
    const auto cast = Transform(spell, spell);
    Send(70001, cast);
    auto& actor = server.GetCharacter(first)->get();
    ASSERT_TRUE(actor.transformation) << spell;
    const auto* form = gmp::GetTransformationForm(spell);
    EXPECT_EQ(actor.instance, form->instance);
    EXPECT_EQ(actor.max_health, form->max_health);
    EXPECT_EQ(actor.strength, form->strength);
    EXPECT_EQ(actor.state.equipped_armor_instance, "");
    EXPECT_EQ(actor.state.weapon_mode, 1);  // Creature MDS files provide FIST locomotion.
    EXPECT_TRUE(actor.approved_projectiles.empty());
    Send(70001, Return(spell));
    EXPECT_FALSE(actor.transformation);
    EXPECT_EQ(actor.instance, "PC_HERO");
    EXPECT_EQ(actor.health, 625);
    EXPECT_EQ(actor.max_health, 1000);
    EXPECT_EQ(actor.max_mana, 300);
    EXPECT_EQ(actor.strength, 77);
    EXPECT_EQ(actor.dexterity, 66);
    EXPECT_EQ(actor.state.equipped_armor_instance, "ITAR_TEST");
    EXPECT_EQ(actor.level, 7);
    EXPECT_EQ(actor.exp, 1234);
    EXPECT_EQ(actor.learn_points, 12);
    EXPECT_EQ(actor.mana, 200 - (spell - 46) * 10);
  }
}

TEST_F(ServerTransformationValidation, CreaturePoseKeepsFistLocomotionWithoutHumanEquipment) {
  Send(70001, Transform());
  auto& actor = server.GetCharacter(first)->get();
  ASSERT_TRUE(actor.transformation);
  for (std::uint32_t sequence = 1; sequence <= 2; ++sequence) {
    PlayerStateUpdatePacket update{};
    update.packet_type = Net::PT_ACTUAL_STATISTICS;
    update.player_id = first;
    update.actor_generation = actor.actor_generation;
    update.command_revision = actor.command_revision;
    update.state_sequence = sequence;
    update.state = actor.state;
    update.state.source_time_ms = ActorClockMilliseconds();
    update.state.position.x = 100.0f * sequence;
    update.state.weapon_mode = sequence == 1 ? 0 : 7;
    update.state.equipped_armor_instance = "ITAR_TEST";
    update.state.active_spell_instance = "ITRU_TRF51";
    update.state.spell_investing = true;
    Send(70001, update);
    EXPECT_EQ(actor.state.position, update.state.position);
    EXPECT_EQ(actor.state.weapon_mode, 1);
    EXPECT_EQ(actor.state.equipped_armor_instance, "");
    EXPECT_EQ(actor.state.active_spell_instance, "");
    EXPECT_FALSE(actor.state.spell_investing);
  }
}

TEST_F(ServerTransformationValidation, DefaultHumanInstanceAndCurrentPositionSurviveReturn) {
  auto& actor = server.GetCharacter(first)->get();
  actor.instance.clear();
  Send(70001, Transform());
  ASSERT_TRUE(actor.transformation);
  actor.state.position = {700.0f, 200.0f, 400.0f};
  Send(70001, Return());
  EXPECT_FALSE(actor.transformation);
  EXPECT_TRUE(actor.instance.empty());
  EXPECT_EQ(actor.state.position, glm::vec3(700.0f, 200.0f, 400.0f));
  EXPECT_EQ(actor.state.weapon_mode, 0);
  EXPECT_EQ(actor.health, 625);
  EXPECT_EQ(actor.mana, 190);
}

TEST_F(ServerTransformationValidation, DamageInAnimalFormPersistsAndClientCannotChooseRestoredStats) {
  Send(70001, Transform());
  auto& actor = server.GetCharacter(first)->get();
  actor.health = actor.state.health_points = 25;  // Wolf entered with 37/60 HP.
  actor.exp = 1500;
  actor.learn_points = 17;
  auto back = Return();
  back.max_health = 1000000;
  back.strength = 1000000;
  back.instance = "DRAGON";
  back.state.health_points = 1000000;
  Send(70001, back);
  EXPECT_EQ(actor.health, 425);  // Twelve wolf HP are 200 human HP.
  EXPECT_EQ(actor.max_health, 1000);
  EXPECT_EQ(actor.strength, 77);
  EXPECT_EQ(actor.mana, 190);
  EXPECT_EQ(actor.exp, 1500);
  EXPECT_EQ(actor.learn_points, 17);
  const auto sequence = actor.state_sequence;
  Send(70001, back);
  EXPECT_EQ(actor.state_sequence, sequence);
  EXPECT_EQ(actor.health, 425);
}

TEST_F(ServerTransformationValidation, LowHealthSmallFormCannotDieOrHealThroughIntegerConversion) {
  auto& actor = server.GetCharacter(first)->get();
  actor.health = actor.state.health_points = 1;
  for (std::uint32_t identity = 1; identity <= 3; ++identity) {
    Send(70001, Transform(47, identity));
    ASSERT_TRUE(actor.transformation);
    EXPECT_EQ(actor.health, 1);
    Send(70001, Return(identity));
    EXPECT_EQ(actor.health, 1);
    EXPECT_EQ(actor.max_health, 1000);
  }
}

TEST_F(ServerTransformationValidation, FailedPlacementCancelledCastAndMissingManaDoNotChangeFormOrConsumeScroll) {
  auto failed = Transform(51, 1, true);
  failed.transformation_succeeded = false;
  Send(70001, failed);
  auto& actor = server.GetCharacter(first)->get();
  EXPECT_FALSE(actor.transformation);
  EXPECT_EQ(actor.InventoryCount("ITSC_TRF51"), 2);
  EXPECT_EQ(actor.mana, 200);
  const auto handler = EventManager::Instance().SubscribeToEventWithPriority(
      kEventOnPlayerCastSpellName, [](const std::any&) { EventManager::Instance().CancelCurrentEvent(); }, -100);
  ASSERT_TRUE(handler);
  Send(70001, Transform(51, 2, true));
  EventManager::Instance().UnsubscribeFromEvent(kEventOnPlayerCastSpellName, *handler);
  EXPECT_FALSE(actor.transformation);
  EXPECT_EQ(actor.InventoryCount("ITSC_TRF51"), 2);
  actor.mana = actor.state.mana_points = 4;
  Send(70001, Transform(51, 3, true));
  EXPECT_FALSE(actor.transformation);
  EXPECT_EQ(actor.InventoryCount("ITSC_TRF51"), 2);
}

TEST_F(ServerTransformationValidation, DuplicateAndNestedCastsCannotChargeTwiceOrReplaceSavedHuman) {
  const auto cast = Transform(51, 1, true);
  Send(70001, cast);
  Send(70001, cast);
  Send(70001, Transform(51, 2, true));
  auto& actor = server.GetCharacter(first)->get();
  ASSERT_TRUE(actor.transformation);
  EXPECT_EQ(actor.transformation->instance, "PC_HERO");
  EXPECT_EQ(actor.transformation->mana, 195);
  EXPECT_EQ(actor.InventoryCount("ITSC_TRF51"), 1);
  Send(70001, Return());
  EXPECT_EQ(actor.instance, "PC_HERO");
  EXPECT_EQ(actor.mana, 195);
}

TEST_F(ServerTransformationValidation, ReturnRequiresOwnerGenerationAndMatchingCast) {
  Send(70001, Transform());
  Send(70002, Return());
  auto stale = Return();
  stale.actor_generation++;
  Send(70001, stale);
  Send(70001, Return(2));
  ASSERT_TRUE(server.GetCharacter(first)->get().transformation);
  Send(70001, Return());
  EXPECT_FALSE(server.GetCharacter(first)->get().transformation);
}

TEST_F(ServerTransformationValidation, HostedNpcCannotTakeThePlayerTransformationPath) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  ASSERT_TRUE(npc.AddInventory("ITRU_TRF51", 1));
  auto cast = Transform();
  cast.caster_id = npc_id;
  cast.actor_generation = npc.actor_generation;
  cast.command_revision = npc.command_revision;
  cast.control_epoch = npc.control_epoch;
  Send(70001, cast);
  EXPECT_FALSE(npc.transformation);
  EXPECT_EQ(npc.instance, "PC_HERO");
  EXPECT_EQ(npc.mana, 100);
}

TEST_F(ServerTransformationValidation, FireWaranNaturalAttackUsesFireDamage) {
  Send(70001, Transform(55));
  auto& attacker = server.GetCharacter(first)->get();
  auto& victim = server.GetCharacter(second)->get();
  victim.health = victim.max_health = victim.state.health_points = 1000;
  auto hit = Hit(attacker, victim, 1, 1u << 2);
  hit.damage = 100;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 1000);
  hit.damage_type = 1u << 3;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 900);
}

TEST_F(ServerTransformationValidation, DeadAnimalReturnDoesNotRevivePlayerAndRespawnRestoresHuman) {
  Send(70001, Transform());
  auto& actor = server.GetCharacter(first)->get();
  actor.health = actor.state.health_points = 0;
  actor.state.life_state = PLAYER_LIFE_DEAD;
  Send(70001, Return());
  EXPECT_EQ(actor.health, 0);
  EXPECT_EQ(actor.state.life_state, PLAYER_LIFE_DEAD);
  EXPECT_EQ(actor.instance, "PC_HERO");
  ASSERT_TRUE(server.UnspawnCharacter(first));
  ASSERT_TRUE(server.SpawnCharacter(first));
  EXPECT_EQ(actor.max_health, 1000);
  EXPECT_EQ(actor.health, 1000);
}

TEST_F(ServerTransformationValidation, VirtualWorldTransferRestoresHumanBeforeNewGeneration) {
  Send(70001, Transform());
  const auto old = Return();
  ASSERT_TRUE(server.SetCharacterVirtualWorld(first, 42));
  auto& actor = server.GetCharacter(first)->get();
  EXPECT_FALSE(actor.transformation);
  EXPECT_EQ(actor.instance, "PC_HERO");
  EXPECT_EQ(actor.health, 625);
  EXPECT_EQ(actor.max_health, 1000);
  Send(70001, old);
  EXPECT_EQ(actor.mana, 190);
}

class ServerSummonValidation : public ServerActorValidation {
protected:
  void SetUp() override {
    ServerActorValidation::SetUp();
    auto& caster = server.GetCharacter(first)->get();
    caster.mana = caster.max_mana = caster.state.mana_points = 1000;
    ASSERT_TRUE(caster.AddInventory("ITRU_WOLF", 1));
    ASSERT_TRUE(caster.AddInventory("ITSC_ARMY", 3));
  }
  CastSpellPacket Cast(std::uint32_t action = 1, bool army = false) {
    const auto& caster = server.GetCharacter(first)->get();
    CastSpellPacket packet{};
    packet.packet_type = Net::PT_CASTSPELL;
    packet.caster_id = first;
    packet.actor_generation = caster.actor_generation;
    packet.command_revision = caster.command_revision;
    packet.action_id = action;
    packet.item_instance = army ? "ITSC_ARMY" : "ITRU_WOLF";
    packet.scroll = army;
    packet.spell_id = army ? 45 : 25;
    packet.mana_delta = 1000000;  // A client cannot choose a summon's cost.
    return packet;
  }
  std::vector<std::uint32_t> Summons() const {
    std::vector<std::uint32_t> result;
    for (const auto id : server.GetNpcManager().GetNpcIds())
      if (server.GetNpcManager().GetNpc(id)->get().summon)
        result.push_back(id);
    return result;
  }
};

TEST_F(ServerSummonValidation, CreatesOneServerNpcInCasterScopeWithTrustedHealthAndManaCost) {
  auto& caster = server.GetCharacter(first)->get();
  caster.virtual_world = 7;
  caster.state.position = {100.0f, 200.0f, 300.0f};
  caster.state.nrot = {0.0f, 0.0f, 1.0f};
  Send(70001, Cast());
  ASSERT_EQ(Summons().size(), 1u);
  const auto& npc = server.GetNpcManager().GetNpc(Summons().front())->get();
  EXPECT_EQ(npc.instance, "SUMMONED_WOLF");
  EXPECT_EQ(npc.health, 100);
  EXPECT_EQ(npc.max_health, 100);
  EXPECT_EQ(npc.world, caster.world);
  EXPECT_EQ(npc.virtual_world, 7);
  EXPECT_EQ(npc.state.position, glm::vec3(100.0f, 200.0f, 500.0f));
  EXPECT_TRUE(npc.is_ingame);
  EXPECT_EQ(npc.summon->caster_id, first);
  EXPECT_EQ(npc.summon->caster_generation, caster.actor_generation);
  EXPECT_EQ(npc.summon->cast_id, 1u);
  EXPECT_EQ(caster.mana, 960);
  EXPECT_TRUE(caster.approved_projectiles.empty());
}

TEST_F(ServerSummonValidation, ArmyCreatesSixIdentitiesConsumesOneScrollAndDuplicateCannotRecast) {
  auto cast = Cast(1, true);
  Send(70001, cast);
  auto& caster = server.GetCharacter(first)->get();
  ASSERT_EQ(Summons().size(), 6u);
  EXPECT_EQ(caster.InventoryCount("ITSC_ARMY"), 2);
  EXPECT_EQ(caster.mana, 995);
  EXPECT_TRUE(caster.approved_projectiles.empty());
  cast.command_revision = caster.command_revision;
  Send(70001, cast);
  EXPECT_EQ(Summons().size(), 6u);
  EXPECT_EQ(caster.InventoryCount("ITSC_ARMY"), 2);
  EXPECT_EQ(caster.mana, 995);
}

TEST_F(ServerSummonValidation, CancelledSummonDoesNotCreateNpcsOrConsumeResources) {
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerCastSpellName, [](std::any) { EventManager::Instance().CancelCurrentEvent(); }));
  Send(70001, Cast(1, true));
  const auto& caster = server.GetCharacter(first)->get();
  EXPECT_TRUE(Summons().empty());
  EXPECT_EQ(caster.InventoryCount("ITSC_ARMY"), 3);
  EXPECT_EQ(caster.mana, 1000);
}

TEST_F(ServerSummonValidation, InsufficientManaAndUnownedRuneCannotSummon) {
  auto& caster = server.GetCharacter(first)->get();
  caster.mana = 39;
  Send(70001, Cast());
  EXPECT_TRUE(Summons().empty());
  EXPECT_EQ(caster.mana, 39);
  caster.mana = 1000;
  ASSERT_TRUE(caster.RemoveInventory("ITRU_WOLF", 1));
  Send(70001, Cast(2));
  EXPECT_TRUE(Summons().empty());
  EXPECT_EQ(caster.mana, 1000);
}

TEST_F(ServerSummonValidation, LimitRejectsWholeArmyWithoutChargingAnotherScroll) {
  Send(70001, Cast(1, true));
  Send(70001, Cast(2, true));
  ASSERT_EQ(Summons().size(), 12u);
  Send(70001, Cast(3, true));
  const auto& caster = server.GetCharacter(first)->get();
  EXPECT_EQ(Summons().size(), 12u);
  EXPECT_EQ(caster.InventoryCount("ITSC_ARMY"), 1);
  EXPECT_EQ(caster.mana, 990);
}

TEST_F(ServerSummonValidation, FailedFormationCannotLeavePartialNpcsOrConsumeMana) {
  server.GetCharacter(first)->get().state.position.z = 10000000.0f;
  Send(70001, Cast(1, true));
  EXPECT_TRUE(Summons().empty());
  EXPECT_EQ(server.GetCharacter(first)->get().mana, 1000);
  EXPECT_EQ(server.GetCharacter(first)->get().InventoryCount("ITSC_ARMY"), 3);
}

TEST_F(ServerSummonValidation, ChangingOwnerVirtualWorldRemovesItsSummons) {
  Send(70001, Cast());
  ASSERT_EQ(Summons().size(), 1u);
  ASSERT_TRUE(server.SetCharacterVirtualWorld(first, 42));
  EXPECT_TRUE(Summons().empty());
}

TEST_F(ServerSummonValidation, SummonEventLuaArgumentsIncludeControllableServerNpcId) {
  Send(70001, Cast());
  ASSERT_EQ(Summons().size(), 1u);
  const auto summoned = Summons().front();
  // Event subscriptions require a loaded resource and its execution scope.
  struct ResourceDirectory {
    std::filesystem::path previous{std::filesystem::current_path()};
    std::filesystem::path root;
    explicit ResourceDirectory(const std::filesystem::path& root) : root(root) {
      std::filesystem::create_directories(root / "resources" / "summon_test" / "server");
      std::filesystem::current_path(root);
    }
    ~ResourceDirectory() {
      std::filesystem::current_path(previous);
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } directory(path.parent_path() / path.stem());
  LuaScript script;
  ResourceManager resources;
  auto& lua = script.GetLuaState();
  lua["test_caster"] = first;
  lua["test_summon"] = summoned;
  {
    std::ofstream metadata(directory.root / "resources" / "summon_test" / "resource.toml");
    metadata << "version = \"1.0.0\"\nscripts = [\"server/main.lua\"]\n";
    std::ofstream source(directory.root / "resources" / "summon_test" / "server" / "main.lua");
    source << R"(
    summon_notifications = 0
    teleport_notifications = 0
    assert(addEventHandler('onPlayerTeleport', function(...)
      assert(select('#', ...) == 2)
      local playerId, vobName = ...
      assert(playerId == test_caster and vobName == 'NW_TELEPORT_TEST')
      teleport_notifications = teleport_notifications + 1
    end))
    assert(addEventHandler('onPlayerSummon', function(playerId, spellId, instance, npcId)
      assert(playerId == test_caster and spellId == 25)
      assert(instance == 'SUMMONED_WOLF' and npcId == test_summon)
      assert(isNpc(npcId))
      assert(destroyNpc(npcId))
      summon_notifications = summon_notifications + 1
    end))
  )";
  }
  resources.DiscoverResources();
  ASSERT_TRUE(resources.LoadResource("summon_test", script));
  EventManager::Instance().TriggerEvent(kEventOnPlayerSummonName, OnPlayerSummonEvent{first, 25, "SUMMONED_WOLF", summoned});
  EXPECT_EQ(resources.GetResource("summon_test")->get().GetEnvironment()["summon_notifications"].get<int>(), 1);
  EXPECT_FALSE(server.GetNpcManager().GetNpc(summoned));
  EventManager::Instance().TriggerEvent(kEventOnPlayerTeleportName, OnPlayerTeleportEvent{first, "NW_TELEPORT_TEST"});
  EXPECT_EQ(resources.GetResource("summon_test")->get().GetEnvironment()["teleport_notifications"].get<int>(), 1);
  resources.UnloadResource("summon_test", script);
}

TEST_F(ServerTestData, SummonDefinitionsAreTrustedAndFailedReloadIsAtomic) {
  SummonRegistry summons;
  EXPECT_EQ(summons.Find(45)->count, 6);
  EXPECT_FALSE(gmp::IsNpcSummonSpell(81));
  EXPECT_EQ(summons.Find(81), nullptr);
  Write(R"({"summons":[{"spell_id":25,"instance":"CUSTOM_WOLF","count":2,"mana_cost":17,"max_health":500}]})");
  ASSERT_TRUE(summons.Load(path));
  EXPECT_EQ(summons.Find(25)->instance, "CUSTOM_WOLF");
  EXPECT_EQ(summons.Find(25)->max_health, 500);
  Write(R"({"summons":[{"spell_id":25,"instance":"BAD","count":999}]})");
  EXPECT_FALSE(summons.Load(path));
  EXPECT_EQ(summons.Find(25)->instance, "CUSTOM_WOLF");
  EXPECT_EQ(summons.Find(25)->count, 2);
  Write(R"({"summons":[{"spell_id":19,"instance":"DEMON"}]})");
  EXPECT_FALSE(summons.Load(path));
  EXPECT_EQ(summons.Find(19), nullptr);
  Write(R"({"summons":[{"spell_id":25,"count":1.5}]})");
  EXPECT_FALSE(summons.Load(path));
  Write(R"({"summons":[{"spell_id":25,"count":4294967297}]})");
  EXPECT_FALSE(summons.Load(path));
  EXPECT_EQ(summons.Find(25)->count, 2);
}

class ServerHealingValidation : public ServerActorValidation {
protected:
  void SetUp() override {
    ServerActorValidation::SetUp();
    auto& caster = server.GetCharacter(first)->get();
    caster.health = caster.state.health_points = 100;
    caster.max_health = 2000;
    caster.mana = caster.max_mana = caster.state.mana_points = 1000;
    for (const auto& instance : {"HEAL_1", "HEAL_3", "HEAL_5", "HEAL_21", "HEAL_28", "HEAL_41", "HEAL_SCROLL", "ITRU_PALLIGHT"})
      ASSERT_TRUE(caster.AddInventory(instance, 1));
  }
  CastSpellPacket Cast(std::uint16_t spell, const std::string& item, std::uint32_t action = 1) {
    const auto& caster = server.GetCharacter(first)->get();
    CastSpellPacket packet{};
    packet.packet_type = Net::PT_CASTSPELLONTARGET;
    packet.caster_id = packet.target_id = first;
    packet.actor_generation = packet.target_generation = caster.actor_generation;
    packet.command_revision = caster.command_revision;
    packet.action_id = action;
    packet.spell_id = spell;
    packet.item_instance = item;
    packet.scroll = item == "HEAL_SCROLL";
    packet.mana_delta = 1000000;
    return packet;
  }
};

TEST_F(ServerHealingValidation, AllSixHealsCommitTrustedHealthAndManaWithoutApprovingDamage) {
  const std::uint16_t spells[] = {1, 3, 5, 21, 28, 41};
  const int heals[] = {200, 400, 800, 200, 400, 800};
  const int costs[] = {10, 25, 50, 10, 25, 50};
  auto& caster = server.GetCharacter(first)->get();
  for (int index = 0; index < 6; ++index) {
    caster.health = caster.state.health_points = 100;
    caster.mana = 1000;
    Send(70001, Cast(spells[index], "HEAL_" + std::to_string(spells[index]), index + 1));
    EXPECT_EQ(caster.health, 100 + heals[index]);
    EXPECT_EQ(caster.state.health_points, caster.health);
    EXPECT_EQ(caster.mana, 1000 - costs[index]);
    EXPECT_TRUE(caster.approved_projectiles.empty());
  }
}

TEST_F(ServerHealingValidation, HealScrollClampsToMaximumAndDuplicateCannotHealOrConsumeAgain) {
  auto& caster = server.GetCharacter(first)->get();
  caster.health = caster.state.health_points = 1950;
  auto cast = Cast(21, "HEAL_SCROLL");
  Send(70001, cast);
  EXPECT_EQ(caster.health, 2000);
  EXPECT_EQ(caster.mana, 995);
  EXPECT_EQ(caster.InventoryCount("HEAL_SCROLL"), 0);
  caster.health = caster.state.health_points = 1800;
  cast.command_revision = caster.command_revision;
  Send(70001, cast);
  EXPECT_EQ(caster.health, 1800);
  EXPECT_EQ(caster.mana, 995);
}

TEST_F(ServerHealingValidation, CancelledOrUnaffordableHealingCannotChangeHealthOrConsumeScroll) {
  auto& caster = server.GetCharacter(first)->get();
  caster.mana = 4;
  Send(70001, Cast(21, "HEAL_SCROLL"));
  EXPECT_EQ(caster.health, 100);
  EXPECT_EQ(caster.mana, 4);
  EXPECT_EQ(caster.InventoryCount("HEAL_SCROLL"), 1);
  caster.mana = 1000;
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerCastSpellName, [](std::any) { EventManager::Instance().CancelCurrentEvent(); }));
  Send(70001, Cast(21, "HEAL_SCROLL", 2));
  EXPECT_EQ(caster.health, 100);
  EXPECT_EQ(caster.mana, 1000);
  EXPECT_EQ(caster.InventoryCount("HEAL_SCROLL"), 1);
}

TEST_F(ServerHealingValidation, ResourceHealthOverrideIsNotHealedTwice) {
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerCastSpellName, [&](std::any) { EXPECT_TRUE(server.SetCharacterHealth(first, 777)); }));
  Send(70001, Cast(21, "HEAL_21"));
  EXPECT_EQ(server.GetCharacter(first)->get().health, 777);
  EXPECT_EQ(server.GetCharacter(first)->get().mana, 990);
}

TEST_F(ServerHealingValidation, HostedNpcCanHealButAnotherViewerCannotCastForIt) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  npc.health = npc.state.health_points = 10;
  npc.max_health = 1000;
  ASSERT_TRUE(npc.AddInventory("HEAL_21", 1));
  auto cast = Cast(21, "HEAL_21");
  cast.caster_id = cast.target_id = npc_id;
  cast.actor_generation = cast.target_generation = npc.actor_generation;
  cast.command_revision = npc.command_revision;
  cast.control_epoch = npc.control_epoch;
  Send(70002, cast);
  EXPECT_EQ(npc.health, 10);
  EXPECT_EQ(npc.mana, 100);
  Send(70001, cast);
  EXPECT_EQ(npc.health, 210);
  EXPECT_EQ(npc.mana, 90);
  EXPECT_TRUE(npc.approved_projectiles.empty());
}

TEST_F(ServerHealingValidation, UnconsciousCasterCannotReviveItselfWithAHealPacket) {
  auto& caster = server.GetCharacter(first)->get();
  caster.health = caster.state.health_points = 1;
  caster.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  caster.flags |= PlayerManager::PL_UNCONCIOUS;
  Send(70001, Cast(21, "HEAL_SCROLL"));
  EXPECT_EQ(caster.health, 1);
  EXPECT_EQ(caster.mana, 1000);
  EXPECT_EQ(caster.InventoryCount("HEAL_SCROLL"), 1);
}

TEST_F(ServerHealingValidation, ZeroIdPaladinLightIsAcceptedWithoutDamageApproval) {
  auto cast = Cast(0, "ITRU_PALLIGHT");
  cast.mana_delta = -10;
  Send(70001, cast);
  const auto& caster = server.GetCharacter(first)->get();
  EXPECT_EQ(caster.mana, 990);
  EXPECT_EQ(caster.health, 100);
  EXPECT_EQ(caster.InventoryCount("ITRU_PALLIGHT"), 1);
  EXPECT_TRUE(caster.approved_projectiles.empty());
}

class ServerTorchValidation : public ServerActorValidation {
protected:
  void SetUp() override {
    ServerActorValidation::SetUp();
  }
  void Hold(const std::string& left, const std::string& right = std::string{}) {
    auto& actor = server.GetCharacter(first)->get();
    PlayerStateUpdatePacket update{};
    update.packet_type = Net::PT_ACTUAL_STATISTICS;
    update.player_id = first;
    update.actor_generation = actor.actor_generation;
    update.command_revision = actor.command_revision;
    update.state_sequence = ++sequence;
    update.state = actor.state;
    update.state.nrot = {0, 0, 1};
    update.state.source_time_ms = 1000 + sequence;
    update.state.left_hand_item_instance = left;
    update.state.right_hand_item_instance = right;
    Send(70001, update);
  }
  std::uint32_t sequence{0};
};

TEST_F(ServerTorchValidation, BurningPresentationReservesOwnedUnlitTorchWithoutGrantingItems) {
  ASSERT_TRUE(server.GiveItem(first, "ITLSTORCH", 1));
  auto& actor = server.GetCharacter(first)->get();
  Hold("ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.left_hand_item_instance, "ITLSTORCHBURNING");
  EXPECT_EQ(actor.InventoryCount("ITLSTORCH"), 1);
  EXPECT_EQ(actor.InventoryCount("ITLSTORCHBURNING"), 0);
  Hold("");
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  Hold("ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.left_hand_item_instance, "ITLSTORCHBURNING");
  EXPECT_EQ(actor.InventoryCount("ITLSTORCH"), 1);
}

TEST_F(ServerTorchValidation, UnownedTorchAndUnrelatedTorchInstancesRemainRejected) {
  auto& actor = server.GetCharacter(first)->get();
  Hold("ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  ASSERT_TRUE(server.GiveItem(first, "ITLSTORCH", 1));
  Hold("OTHER_TORCH");
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  EXPECT_EQ(actor.InventoryCount("OTHER_TORCH"), 0);
}

TEST_F(ServerTorchValidation, OneOwnedTorchCannotProduceTwoBurningHandObjects) {
  ASSERT_TRUE(server.GiveItem(first, "ITLSTORCH", 1));
  Hold("ITLSTORCHBURNING", "ITLSTORCHBURNING");
  const auto& actor = server.GetCharacter(first)->get();
  EXPECT_EQ(actor.state.left_hand_item_instance, "ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
  Hold("ITLSTORCHBURNING", "ITLSTORCH");
  EXPECT_EQ(actor.state.left_hand_item_instance, "ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
  Hold("ITLSTORCH", "ITLSTORCHBURNING");
  EXPECT_EQ(actor.state.left_hand_item_instance, "ITLSTORCH");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
}

TEST_F(ServerTorchValidation, RemovingLastBackingTorchImmediatelyClearsItsBurningPresentation) {
  ASSERT_TRUE(server.GiveItem(first, "ITLSTORCH", 1));
  Hold("ITLSTORCHBURNING");
  ASSERT_EQ(server.GetCharacter(first)->get().state.left_hand_item_instance, "ITLSTORCHBURNING");
  ASSERT_TRUE(server.RemoveItem(first, "ITLSTORCH", 1));
  const auto& actor = server.GetCharacter(first)->get();
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  EXPECT_EQ(actor.InventoryCount("ITLSTORCH"), 0);
}

class ServerWeaponModeValidation : public ServerActorValidation {
protected:
  void SetUp() override {
    ServerActorValidation::SetUp();
    for (const auto id : {first, npc_id}) {
      auto& actor = server.GetCharacter(id)->get();
      for (const auto* instance : {"BOW", "CROSSBOW", "SWORD", "TWOHANDER", "TORCH"}) ASSERT_TRUE(actor.AddInventory(instance, 1));
    }
  }
};

TEST_F(ServerWeaponModeValidation, DrawSheathAndDirectSwitchesKeepModesAndHandsTogetherForPlayersAndNpcs) {
  for (const auto id : {first, npc_id}) {
    auto& actor = server.GetCharacter(id)->get();
    actor.state.melee_weapon_instance = "SWORD";
    actor.state.ranged_weapon_instance = "BOW";
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 5));
    EXPECT_EQ(actor.state.weapon_mode, 5);
    EXPECT_EQ(actor.state.left_hand_item_instance, "BOW");
    EXPECT_EQ(actor.state.right_hand_item_instance, "");
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 0));
    EXPECT_EQ(actor.state.weapon_mode, 0);
    EXPECT_EQ(actor.state.left_hand_item_instance, "");
    EXPECT_EQ(actor.state.ranged_weapon_instance, "BOW");
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 5));
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 3));
    EXPECT_EQ(actor.state.left_hand_item_instance, "");
    EXPECT_EQ(actor.state.right_hand_item_instance, "SWORD");
    actor.state.ranged_weapon_instance = "CROSSBOW";
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 6));
    EXPECT_EQ(actor.state.left_hand_item_instance, "");
    EXPECT_EQ(actor.state.right_hand_item_instance, "CROSSBOW");
    actor.state.melee_weapon_instance = "TWOHANDER";
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 4));
    EXPECT_EQ(actor.state.right_hand_item_instance, "TWOHANDER");
    ASSERT_TRUE(server.SetCharacterWeaponMode(id, 1));
    EXPECT_EQ(actor.state.left_hand_item_instance, "");
    EXPECT_EQ(actor.state.right_hand_item_instance, "");
    for (const auto* instance : {"BOW", "CROSSBOW", "SWORD", "TWOHANDER"}) EXPECT_EQ(actor.InventoryCount(instance), 1);
  }
}

TEST_F(ServerWeaponModeValidation, RepeatedNoneRepairsStaleWeaponsWithoutRemovingOtherHeldItems) {
  auto& actor = server.GetCharacter(first)->get();
  actor.state.weapon_mode = 0;
  actor.state.ranged_weapon_instance = "BOW";
  actor.state.melee_weapon_instance = "SWORD";
  actor.state.left_hand_item_instance = "BOW";
  actor.state.right_hand_item_instance = "SWORD";
  ASSERT_TRUE(server.SetCharacterWeaponMode(first, 0));
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
  actor.state.left_hand_item_instance = "TORCH";
  actor.state.right_hand_item_instance = "SWORD";
  actor.state.active_spell_instance = "ITRU_TEST";
  actor.state.active_spell_level = 2;
  actor.state.spell_investing = true;
  ASSERT_TRUE(server.SetCharacterWeaponMode(first, 0));
  EXPECT_EQ(actor.state.left_hand_item_instance, "TORCH");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
  EXPECT_EQ(actor.state.active_spell_instance, "");
  EXPECT_EQ(actor.state.active_spell_level, 0);
  EXPECT_FALSE(actor.state.spell_investing);
}

TEST_F(ServerWeaponModeValidation, CounterstrikeStyleModeOverrideKeepsTheNestedCommandState) {
  auto& actor = server.GetCharacter(first)->get();
  actor.state.ranged_weapon_instance = "BOW";
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerWeaponModeChangeName, [&](std::any event) {
    const auto change = std::any_cast<OnPlayerWeaponModeChangeEvent>(event);
    if (change.player_id == first && change.new_mode == 5)
      EXPECT_TRUE(server.SetCharacterWeaponMode(first, 0));
  }));
  ASSERT_TRUE(server.SetCharacterWeaponMode(first, 5));
  EXPECT_EQ(actor.state.weapon_mode, 0);
  EXPECT_EQ(actor.state.left_hand_item_instance, "");
  EXPECT_EQ(actor.state.right_hand_item_instance, "");
}

TEST_F(ServerWeaponModeValidation, WeaponModeCallbackMayDestroyTheNpc) {
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerWeaponModeChangeName, [&](std::any event) {
    const auto change = std::any_cast<OnPlayerWeaponModeChangeEvent>(event);
    if (change.player_id == npc_id)
      EXPECT_TRUE(server.DestroyNpc(npc_id));
  }));
  EXPECT_FALSE(server.SetCharacterWeaponMode(npc_id, 0));
  EXPECT_FALSE(server.GetCharacter(npc_id));
}

TEST_F(ServerInstanceValidation, PendingLoadoutDrawCommandSurvivesConstructorCompletion) {
  auto& actor = server.GetCharacter(first)->get();
  ASSERT_TRUE(server.SetCharacterInstance(first, "PC_HERO"));
  ASSERT_TRUE(server.EquipMeleeWeapon(first, "ITMW_TEST"));
  ASSERT_TRUE(server.SetCharacterWeaponMode(first, 3));
  auto ready = Defaults(first);
  ready.defaults.weapon_mode = 0;
  Send(70001, ready);
  EXPECT_FALSE(actor.instance_change);
  EXPECT_EQ(actor.state.weapon_mode, 3);
  EXPECT_EQ(actor.state.melee_weapon_instance, "ITMW_TEST");
  EXPECT_EQ(actor.state.right_hand_item_instance, "ITMW_TEST");
  EXPECT_EQ(actor.InventoryCount("ITMW_TEST"), 1);
}

TEST_F(ServerActorValidation, WorldEntryNeverImportsNativeInventoryAndPreservesServerGrants) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.npc_world_ready = false;
  ASSERT_TRUE(server.GiveItem(first, "ITPO_TEST", 3));
  PlayerWorldEnterPacket ready;
  ready.packet_type = Net::PT_PLAYER_WORLD_ENTER;
  ready.actor_generation = player.actor_generation;
  ready.world_name = player.world;
  ready.inventory = {{"ITPO_TEST", 100000}, {"ITRU_TEST", 50}};
  Send(70001, ready);
  EXPECT_TRUE(player.npc_world_ready);
  EXPECT_TRUE(player.inventory_initialized);
  EXPECT_EQ(player.InventoryCount("ITPO_TEST"), 3);
  EXPECT_EQ(player.InventoryCount("ITRU_TEST"), 0);
  player.npc_world_ready = false;
  ready.inventory = {{"ITPO_TEST", 1}};
  Send(70001, ready);
  EXPECT_EQ(player.InventoryCount("ITPO_TEST"), 3);
  EXPECT_EQ(player.InventoryCount("ITRU_TEST"), 0);
}

TEST_F(ServerActorValidation, UnownedItemUseCannotAssignNativeEffects) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.health = player.state.health_points = 10;
  player.mana = player.state.mana_points = 20;
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerUseItemName, [&](std::any) { ++requests; }));
  ActorActionPacket use;
  use.actor_id = first;
  use.actor_generation = player.actor_generation;
  use.command_revision = player.command_revision;
  use.action_id = 1;
  use.kind = ActorActionKind::ItemUse;
  use.item_instance = "ITPO_TEST";
  use.quantity = 1;
  use.health_delta = use.mana_delta = 50;
  Send(70001, use);
  EXPECT_EQ(player.health, 10);
  EXPECT_EQ(player.mana, 20);
  EXPECT_EQ(player.InventoryCount("ITPO_TEST"), 0);
  EXPECT_EQ(requests, 1);
}

TEST_F(ServerActorValidation, PoseSnapshotsCannotAssignHealthOrManaIncreases) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.health = player.state.health_points = 10;
  player.mana = player.state.mana_points = 20;
  PlayerStateUpdatePacket update;
  update.packet_type = Net::PT_ACTUAL_STATISTICS;
  update.actor_generation = player.actor_generation;
  update.command_revision = player.command_revision;
  update.state_sequence = 1;
  update.state = player.state;
  update.state.nrot = {0, 0, 1};
  update.state.position.x = 123;
  update.state.source_time_ms = ActorClockMilliseconds();
  update.state.health_points = update.state.mana_points = 100;
  Send(70001, update);
  EXPECT_EQ(player.health, 10);
  EXPECT_EQ(player.mana, 20);
  EXPECT_EQ(player.state.health_points, 10);
  EXPECT_EQ(player.state.mana_points, 20);
  EXPECT_EQ(player.state.position.x, 123);
}

TEST_F(ServerActorValidation, NaturalAttackDamageTypesDoNotDependOnInstanceNamesOrWeaponDrawState) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  for (const auto* instance : {"PC_HERO", "CH", "ZOMBIE01", "ZOMBIE02", "ZOMBIE03", "ZOMBIE04", "Zombie01", "ZOMBIE05", "WOLF", "FIREWARAN",
                               "STONEGOLEM", "TROLL", "DRAGON_FIRE", "CUSTOM_ADDON_CREATURE"}) {
    attacker.instance = instance;
    for (const auto mode : {0, 1}) {
      attacker.state.weapon_mode = mode;
      for (const auto type : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 6u, 24u, 255u}) {
        SCOPED_TRACE(::testing::Message() << instance << " weapon mode " << mode << " damage type " << type);
        attacker.last_hit_action_id.reset();
        attacker.hit_victims.clear();
        victim.health = victim.state.health_points = 100;
        Send(70001, Hit(attacker, victim, 1, type));
        EXPECT_EQ(victim.health, 90);
        EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_ALIVE);
      }
    }
  }
}

TEST_F(ServerActorValidation, NaturalAttacksWorkBetweenPlayersAndHostedNpcs) {
  const auto other_npc_id = server.GetNpcManager().Create("Other", "CUSTOM_ADDON_CREATURE", "NEWWORLD.ZEN", std::max(2u, server.GetMaxSlots()));
  ASSERT_NE(other_npc_id, 0u);
  auto& other_npc = server.GetNpcManager().GetNpc(other_npc_id)->get();
  other_npc.is_ingame = 1;
  other_npc.health = other_npc.max_health = other_npc.state.health_points = 100;
  other_npc.strength = 10;
  other_npc.state.life_state = PLAYER_LIFE_ALIVE;
  other_npc.state.weapon_mode = 1;
  other_npc.streamed_by_players = {first, second};
  for (const auto id : {first, second}) server.GetPlayerManager().GetPlayer(id)->get().spawned_players.insert(other_npc_id);
  ASSERT_TRUE(server.SetNpcHostPlayer(other_npc_id, second));
  for (const auto attacker_id : {first, second, npc_id, other_npc_id}) {
    auto& attacker = server.GetCharacter(attacker_id)->get();
    const auto connection = attacker_id == second || attacker_id == other_npc_id ? 70002 : 70001;
    for (const auto victim_id : {first, second, npc_id, other_npc_id}) {
      if (victim_id == attacker_id)
        continue;
      auto& victim = server.GetCharacter(victim_id)->get();
      for (const auto type : {2u, 4u, 8u, 16u, 24u}) {
        SCOPED_TRACE(::testing::Message() << attacker_id << " -> " << victim_id << " damage type " << type);
        attacker.last_hit_action_id.reset();
        attacker.hit_victims.clear();
        victim.health = victim.state.health_points = 100;
        auto hit = Hit(attacker, victim, 1, type);
        if (const auto npc = server.GetNpcManager().GetNpc(attacker_id))
          hit.control_epoch = npc->get().control_epoch;
        Send(connection, hit);
        EXPECT_EQ(victim.health, 90);
        EXPECT_EQ(victim.last_validated_attacker, attacker_id);
      }
    }
  }
}

TEST_F(ServerActorValidation, NaturalAttacksKeepValidMaskReachStrengthAndReplayBounds) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  attacker.instance = "CUSTOM_ADDON_CREATURE";
  Send(70001, Hit(attacker, victim, 1, 0));
  Send(70001, Hit(attacker, victim, 1, 256));
  Send(70001, Hit(attacker, victim, 1, 260));
  EXPECT_EQ(victim.health, 100);
  EXPECT_FALSE(attacker.last_hit_action_id.has_value());
  victim.state.position.x = 301.0f;
  Send(70001, Hit(attacker, victim, 1, 4));
  EXPECT_EQ(victim.health, 100);
  EXPECT_FALSE(attacker.last_hit_action_id.has_value());
  victim.state.position.x = 0.0f;
  auto hit = Hit(attacker, victim, 1, 4);
  hit.damage = 1000000;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 90);  // Server-owned strength still caps damage to 10.
  Send(70001, hit);
  EXPECT_EQ(victim.health, 90);
  hit.attack_id = 2;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 90);  // A fresh action cannot bypass the cooldown.
  attacker.last_attack_at -= std::chrono::seconds(1);
  Send(70001, hit);
  EXPECT_EQ(victim.health, 80);
}

TEST_F(ServerActorValidation, NaturalAttacksKeepHostAuthorityWithoutCatalogProtectionCaps) {
  auto& attacker = server.GetNpcManager().GetNpc(npc_id)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  attacker.instance = "CUSTOM_ADDON_CREATURE";
  auto hit = Hit(attacker, victim, 1, 4);
  hit.control_epoch = attacker.control_epoch;
  Send(70002, hit);  // Only the controlling client may report this NPC's hit.
  EXPECT_EQ(victim.health, 100);
  EXPECT_FALSE(attacker.last_hit_action_id.has_value());
  ++hit.control_epoch;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 100);
  EXPECT_FALSE(attacker.last_hit_action_id.has_value());
  hit.control_epoch = attacker.control_epoch;
  ASSERT_TRUE(victim.AddInventory("ITAR_TEST", 1));
  victim.state.equipped_armor_instance = "ITAR_TEST";
  Send(70001, hit);
  EXPECT_EQ(victim.health, 90);  // Native damage already includes local armor effects.
}

TEST_F(ServerActorValidation, WeakPhysicalWeaponsCannotClipTheNativeMinimumBelowFive) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  ASSERT_TRUE(attacker.AddInventory("ITMW_TEST", 1));
  ASSERT_TRUE(victim.AddInventory("ITAR_TEST", 1));
  attacker.state.melee_weapon_instance = "ITMW_TEST";
  attacker.state.weapon_mode = 3;
  victim.state.equipped_armor_instance = "ITAR_TEST";
  for (const int strength : {0, 1, 3}) {
    SCOPED_TRACE(strength);
    attacker.strength = strength;
    attacker.last_hit_action_id.reset();
    attacker.hit_victims.clear();
    victim.health = victim.state.health_points = 100;
    auto hit = Hit(attacker, victim, 1, 4);
    hit.damage = 5;
    Send(70001, hit);
    EXPECT_EQ(victim.health, 95);
    Send(70001, hit);
    EXPECT_EQ(victim.health, 95);  // The minimum does not bypass replay checks.
  }
  attacker.last_hit_action_id.reset();
  attacker.hit_victims.clear();
  auto custom_minimum = Hit(attacker, victim, 1, 4);
  custom_minimum.damage = 2;
  Send(70001, custom_minimum);
  EXPECT_EQ(victim.health, 93);  // A validation cap must not inflate native damage.
}

TEST_F(ServerActorValidation, NaturalAttackMaskDoesNotReplaceDrawnWeaponOrPaidProjectileValidation) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  attacker.instance = "CUSTOM_ADDON_CREATURE";
  attacker.state.weapon_mode = 3;
  Send(70001, Hit(attacker, victim, 1, 4));  // No owned melee weapon.
  EXPECT_EQ(victim.health, 100);
  ASSERT_TRUE(attacker.AddInventory("ITMW_TEST", 1));
  attacker.state.melee_weapon_instance = "ITMW_TEST";
  Send(70001, Hit(attacker, victim, 1, 8));  // Custom native weapon channels are accepted.
  EXPECT_EQ(victim.health, 90);
  Send(70001, Hit(attacker, victim, 1, 4));
  EXPECT_EQ(victim.health, 90);
  attacker.last_hit_action_id.reset();
  attacker.hit_victims.clear();
  attacker.state.weapon_mode = 7;
  Send(70001, Hit(attacker, victim, 2, 32));  // No server-approved spell cast.
  EXPECT_EQ(victim.health, 90);
  EXPECT_FALSE(attacker.last_hit_action_id.has_value());
}

TEST_F(ServerActorValidation, PvpMeleeKnocksOutThenAllowsExecution) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  victim.health = victim.state.health_points = 5;
  Send(70001, Hit(attacker, victim, 1));
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
  attacker.last_attack_at -= std::chrono::seconds(1);
  Send(70001, Hit(attacker, victim, 2));
  EXPECT_EQ(victim.health, 0);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_DEAD);
}

TEST_F(ServerActorValidation, NativeFinishingMoveKillsWithoutNormalDamageOrArmorCalculation) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto hit = FinishingMove(attacker, victim);
  ASSERT_TRUE(victim.AddInventory("ITAR_TEST", 1));
  victim.state.equipped_armor_instance = "ITAR_TEST";
  hit.damage = 0;
  hit.damage_type = 0;
  int hits = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any event) {
    const auto request = std::any_cast<OnPlayerHitEvent>(event);
    ++hits;
    EXPECT_EQ(request.damage, 1);
  }));
  Send(70001, hit);
  EXPECT_EQ(hits, 1);
  EXPECT_EQ(victim.health, 0);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_DEAD);
  EXPECT_NE(victim.tod, 0);
  EXPECT_EQ(victim.flags & PlayerManager::PL_UNCONCIOUS, 0);
}

TEST_F(ServerActorValidation, FinishingMoveRequiresOwnedMeleeWeaponAndCanonicalUnconsciousVictim) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto hit = FinishingMove(attacker, victim);
  const auto reject = [&] {
    Send(70001, hit);
    EXPECT_EQ(victim.health, 1);
    EXPECT_EQ(victim.tod, 0);
    EXPECT_FALSE(attacker.last_hit_action_id.has_value());
  };
  victim.state.life_state = PLAYER_LIFE_ALIVE;
  reject();
  victim.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  victim.flags &= ~PlayerManager::PL_UNCONCIOUS;
  reject();
  victim.flags |= PlayerManager::PL_UNCONCIOUS;
  attacker.state.weapon_mode = 1;
  reject();  // Fists cannot perform Gothic's finishing move.
  attacker.state.weapon_mode = 5;
  reject();
  attacker.state.weapon_mode = 3;
  attacker.state.melee_weapon_instance = "UNOWNED_ITEM";
  reject();
  attacker.state.melee_weapon_instance = "ITMW_TEST";
  ASSERT_TRUE(attacker.RemoveInventory("ITMW_TEST", 1));
  reject();
  ASSERT_TRUE(attacker.AddInventory("ITMW_TEST", 1));
  victim.state.position.x = 501.0f;
  reject();
  victim.state.position.x = 0.0f;
  victim.invulnerable = true;
  reject();
  victim.invulnerable = false;
  victim.virtual_world = 1;
  reject();
  victim.virtual_world = 0;
  ++hit.victim_generation;
  reject();
  hit.victim_generation = victim.actor_generation;
  ++hit.actor_generation;
  reject();
  hit.actor_generation = attacker.actor_generation;
  ++hit.command_revision;
  reject();
}

TEST_F(ServerActorValidation, FinishingMoveHonorsHitCancellationAndCannotBeReplayed) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  const auto hit = FinishingMove(attacker, victim);
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any) {
    ++requests;
    EventManager::Instance().CancelCurrentEvent();
  }));
  Send(70001, hit);
  Send(70001, hit);
  EXPECT_EQ(requests, 1);
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
}

TEST_F(ServerActorValidation, FinishingMoveCannotExecuteMultipleVictimsOrBypassMeleeCooldown) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  const auto first_hit = FinishingMove(attacker, victim);
  auto second_hit = FinishingMove(attacker, npc);
  Send(70001, first_hit);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_DEAD);
  Send(70001, second_hit);
  EXPECT_EQ(npc.health, 1);
  second_hit.attack_id = 2;
  Send(70001, second_hit);
  EXPECT_EQ(npc.health, 1);
  attacker.last_attack_at -= std::chrono::seconds(1);
  Send(70001, second_hit);
  EXPECT_EQ(npc.state.life_state, PLAYER_LIFE_DEAD);
}

TEST_F(ServerActorValidation, FinishingMoveRechecksRecoveryDuringHitCallback) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  const auto hit = FinishingMove(attacker, victim);
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any) { EXPECT_TRUE(server.SetCharacterHealth(second, 20)); }));
  Send(70001, hit);
  EXPECT_EQ(victim.health, 20);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_ALIVE);
}

TEST_F(ServerActorValidation, FinishingMoveRechecksWeaponAndRangeDuringHitCallback) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto hit = FinishingMove(attacker, victim);
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any) {
    if (++requests == 1)
      attacker.state.weapon_mode = 1;
    else
      attacker.state.position.x = 501.0f;
  }));
  Send(70001, hit);
  EXPECT_EQ(victim.health, 1);
  attacker.state.weapon_mode = 3;
  attacker.last_attack_at -= std::chrono::seconds(1);
  hit.attack_id = 2;
  Send(70001, hit);
  EXPECT_EQ(requests, 2);
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
}

TEST_F(ServerActorValidation, HostedNpcFinishingMoveRequiresCurrentHostAndRechecksResourceCallback) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto hit = FinishingMove(npc, victim);
  hit.control_epoch = npc.control_epoch;
  Send(70002, hit);  // Another client cannot report this NPC's execution.
  EXPECT_EQ(victim.health, 1);
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any) { EXPECT_TRUE(server.SetNpcHostPlayer(npc_id, second)); }));
  Send(70001, hit);
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
}

TEST_F(ServerActorValidation, StandUpRetriesCurrentRevisionAfterMinimumRecoveryDelay) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.health = player.state.health_points = 1;
  player.flags |= PlayerManager::PL_UNCONCIOUS;
  player.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  player.state.body_state = 22 | 256;
  player.state.animation_name = "S_WOUNDED";
  player.command_revision = 10;
  PlayerStandUpPacket recovery{};
  recovery.packet_type = Net::PT_PLAYER_STANDUP;
  recovery.actor_generation = player.actor_generation;
  recovery.command_revision = 9;
  player.last_damage_at = std::chrono::steady_clock::now() - std::chrono::seconds(2);
  Send(70001, recovery);
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
  recovery.command_revision = player.command_revision;
  player.last_damage_at = std::chrono::steady_clock::now();
  Send(70001, recovery);
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
  player.last_damage_at -= std::chrono::seconds(2);
  Send(70001, recovery);
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_ALIVE);
  EXPECT_EQ(player.health, 1);
  EXPECT_EQ(player.flags & PlayerManager::PL_UNCONCIOUS, 0);
  EXPECT_EQ(player.state.body_state, 256u);
  EXPECT_TRUE(player.state.animation_name.empty());
  EXPECT_FALSE(player.CombatRevisionIsCurrent(recovery.command_revision));
}

TEST_F(ServerActorValidation, StandUpCannotResurrectDeadPlayerOrUsePreviousGeneration) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.health = player.state.health_points = 1;
  player.flags |= PlayerManager::PL_UNCONCIOUS;
  player.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  player.last_damage_at = std::chrono::steady_clock::now() - std::chrono::seconds(2);
  PlayerStandUpPacket recovery{};
  recovery.packet_type = Net::PT_PLAYER_STANDUP;
  recovery.actor_generation = player.actor_generation + 1;
  recovery.command_revision = player.command_revision;
  Send(70001, recovery);
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
  EXPECT_TRUE(server.SetCharacterHealth(first, 0));
  recovery.actor_generation = player.actor_generation;
  recovery.command_revision = player.command_revision;
  Send(70001, recovery);
  EXPECT_EQ(player.health, 0);
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_DEAD);
  EXPECT_NE(player.tod, 0);
}

TEST_F(ServerActorValidation, ScriptedRecoveryClearsUnconsciousPoseAndRejectsOldActions) {
  auto& player = server.GetPlayerManager().GetPlayer(first)->get();
  player.health = player.state.health_points = 1;
  player.flags |= PlayerManager::PL_UNCONCIOUS;
  player.state.life_state = PLAYER_LIFE_UNCONSCIOUS;
  player.state.body_state = 22;
  player.state.animation_name = "S_WOUNDEDB";
  const auto old_revision = player.command_revision;
  EXPECT_TRUE(server.SetCharacterHealth(first, 20));
  EXPECT_EQ(player.state.life_state, PLAYER_LIFE_ALIVE);
  EXPECT_EQ(player.state.body_state, 0u);
  EXPECT_TRUE(player.state.animation_name.empty());
  EXPECT_FALSE(player.CombatRevisionIsCurrent(old_revision));
}

TEST_F(ServerActorValidation, ConsecutivePaidProjectilesSurviveWeaponSwitchAndCanKill) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  victim.health = victim.state.health_points = npc.health = npc.state.health_points = 5;
  ApproveProjectile(attacker, 100);
  ApproveProjectile(attacker, 101);
  attacker.state.weapon_mode = 0;
  Send(70001, Hit(attacker, victim, 1, 32));
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_DEAD);
  Send(70001, Hit(attacker, npc, 2, 32));
  EXPECT_EQ(npc.state.life_state, PLAYER_LIFE_DEAD);
  EXPECT_TRUE(attacker.approved_projectiles.front().victims.contains(second));
  EXPECT_TRUE(attacker.approved_projectiles.back().victims.contains(npc_id));
}

TEST_F(ServerActorValidation, NativeSpellDamageWorksWithoutRuneOrArmorMetadata) {
  auto& caster = server.GetNpcManager().GetNpc(npc_id)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  victim.health = victim.max_health = victim.state.health_points = 1000;
  ASSERT_TRUE(victim.AddInventory("ITAR_TEST", 1));
  victim.state.equipped_armor_instance = "ITAR_TEST";
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELL;
  cast.caster_id = npc_id;
  cast.actor_generation = caster.actor_generation;
  cast.control_epoch = caster.control_epoch;
  cast.action_id = 1;
  cast.item_instance = "ITRU_TEST";
  cast.spell_id = 19;
  Send(70001, cast);
  ASSERT_EQ(caster.approved_projectiles.size(), 1u);
  auto hit = Hit(caster, victim, 2, 32);
  hit.control_epoch = caster.control_epoch;
  hit.damage = 30;  // Native spell damage 80 minus protection 50.
  Send(70001, hit);
  EXPECT_EQ(victim.health, 970);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_ALIVE);
  Send(70001, hit);
  EXPECT_EQ(victim.health, 970);
}

TEST_F(ServerActorValidation, SpellDamageDoesNotSubtractArmorTwiceFromNativeDamage) {
  auto& caster = server.GetNpcManager().GetNpc(npc_id)->get();
  auto& victim = server.GetCharacter(second)->get();
  victim.health = victim.max_health = victim.state.health_points = 1000;
  ASSERT_TRUE(victim.AddInventory("ITAR_TEST", 1));
  victim.state.equipped_armor_instance = "ITAR_TEST";
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELL;
  cast.caster_id = npc_id;
  cast.actor_generation = caster.actor_generation;
  cast.control_epoch = caster.control_epoch;
  cast.command_revision = caster.command_revision;
  cast.action_id = 1;
  cast.item_instance = "ITRU_TEST";
  cast.spell_id = 19;
  Send(70001, cast);
  auto hit = Hit(caster, victim, 2, 32);
  hit.control_epoch = caster.control_epoch;
  hit.damage = 30;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 970);  // 80 - 50 once, not (80 - 50) - 50.
  Send(70001, hit);
  EXPECT_EQ(victim.health, 970);
}

TEST_F(ServerActorValidation, ExplicitNonlethalProjectileProtectsUnconsciousPlayers) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  victim.health = victim.state.health_points = 5;
  ApproveProjectile(attacker, 100);
  ApproveProjectile(attacker, 101);
  Send(70001, Hit(attacker, victim, 1, 32, true));
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
  Send(70001, Hit(attacker, victim, 2, 32, true));
  EXPECT_EQ(victim.health, 1);
  EXPECT_EQ(victim.state.life_state, PLAYER_LIFE_UNCONSCIOUS);
}

TEST_F(ServerActorValidation, OldCombatReportsCannotCrossARespawnCommandBarrier) {
  auto& attacker = server.GetPlayerManager().GetPlayer(first)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  attacker.command_revision = attacker.combat_barrier_revision = 10;
  auto old_hit = Hit(attacker, victim, 1);
  old_hit.command_revision = 9;
  Send(70001, old_hit);
  EXPECT_EQ(victim.health, 100);
  Send(70001, Hit(attacker, victim, 1));
  EXPECT_EQ(victim.health, 90);
  auto future_hit = Hit(attacker, victim, 2);
  future_hit.command_revision = 11;
  Send(70001, future_hit);
  EXPECT_EQ(victim.health, 90);
}

TEST_F(ServerActorValidation, LifeResetClearsOutstandingProjectileApprovals) {
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  ApproveProjectile(victim, 100);
  ASSERT_FALSE(victim.approved_projectiles.empty());
  ASSERT_TRUE(server.SetCharacterHealth(second, 0));
  EXPECT_TRUE(victim.approved_projectiles.empty());
  EXPECT_EQ(victim.combat_barrier_revision, victim.command_revision);
}

TEST_F(ServerActorValidation, NpcUseRechecksHostAfterResourceCallback) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  npc.health = npc.state.health_points = 10;
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerUseItemName, [&](std::any) { EXPECT_TRUE(server.SetNpcHostPlayer(npc_id, second)); }));
  ActorActionPacket use;
  use.actor_id = npc_id;
  use.actor_generation = npc.actor_generation;
  use.control_epoch = npc.control_epoch;
  use.action_id = 1;
  use.kind = ActorActionKind::ItemUse;
  use.item_instance = "ITPO_TEST";
  use.quantity = 1;
  use.health_delta = 50;
  Send(70001, use);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 3);
  EXPECT_EQ(npc.health, 10);
  use.control_epoch = npc.control_epoch;
  use.action_id = 2;
  Send(70001, use);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 3);
  Send(70002, use);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 2);
  EXPECT_EQ(npc.health, 60);
}

TEST_F(ServerActorValidation, NpcCastRechecksHostAfterResourceCallback) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerCastSpellName,
                                                        [&](std::any) { EXPECT_TRUE(server.SetNpcHostPlayer(npc_id, second)); }));
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELL;
  cast.caster_id = npc_id;
  cast.actor_generation = npc.actor_generation;
  cast.control_epoch = npc.control_epoch;
  cast.action_id = 1;
  cast.item_instance = "ITRU_TEST";
  cast.spell_id = 19;
  cast.mana_delta = -5;
  Send(70001, cast);
  EXPECT_EQ(npc.mana, 100);
  EXPECT_TRUE(npc.approved_projectiles.empty());
  cast.control_epoch = npc.control_epoch;
  cast.action_id = 2;
  Send(70002, cast);
  EXPECT_EQ(npc.mana, 95);
  EXPECT_FALSE(npc.approved_projectiles.empty());
}

TEST_F(ServerActorValidation, InvalidSpellLevelCannotApproveAnAttackOrConsumeMana) {
  auto& caster = server.GetNpcManager().GetNpc(npc_id)->get();
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELL;
  cast.caster_id = npc_id;
  cast.actor_generation = caster.actor_generation;
  cast.control_epoch = caster.control_epoch;
  cast.action_id = 1;
  cast.item_instance = "ITRU_TEST";
  cast.spell_id = 19;
  cast.spell_level = kMaxSpellInvestLevel + 1;
  cast.mana_delta = -5;
  Send(70001, cast);
  EXPECT_EQ(caster.mana, 100);
  EXPECT_TRUE(caster.approved_projectiles.empty());
  cast.spell_level = kMaxSpellInvestLevel;
  cast.command_revision = caster.command_revision;
  Send(70001, cast);
  EXPECT_EQ(caster.mana, 95);
  EXPECT_EQ(caster.approved_projectiles.size(), 1u);
}

TEST_F(ServerActorValidation, RejectedScrollTargetsSendFreshInventoryAndManaCorrections) {
  auto& caster = server.GetNpcManager().GetNpc(npc_id)->get();
  ASSERT_TRUE(caster.AddInventory("ITSC_TEST", 1));
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELLONTARGET;
  cast.caster_id = npc_id;
  cast.actor_generation = caster.actor_generation;
  cast.control_epoch = caster.control_epoch;
  cast.item_instance = "ITSC_TEST";
  cast.scroll = true;
  cast.spell_id = 19;
  const auto reject = [&] {
    const auto revision = caster.command_revision;
    const auto sequence = caster.state_sequence;
    ++cast.action_id;
    Send(70001, cast);
    EXPECT_TRUE(IsNewActorSequence(caster.command_revision, revision));
    EXPECT_TRUE(IsNewActorSequence(caster.state_sequence, sequence));
    EXPECT_EQ(caster.mana, 100);
    EXPECT_EQ(caster.InventoryCount("ITSC_TEST"), 1);
    EXPECT_TRUE(caster.approved_projectiles.empty());
  };
  reject();  // Target packet without a target.
  cast.target_id = 99999;
  reject();  // Missing target.
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  cast.target_id = second;
  cast.target_generation = victim.actor_generation + 1;
  reject();  // Reused or stale target incarnation.
  cast.target_generation = victim.actor_generation;
  victim.virtual_world = 1;
  reject();  // Different visibility scope.
  victim.virtual_world = 0;
  victim.state.position.x = 4000.0f;
  reject();  // Beyond the trusted spell reach.
  victim.state.position.x = 0.0f;
  const auto target = server.GetNpcManager().Create("Target", "PC_HERO", "NEWWORLD.ZEN", std::max(2u, server.GetMaxSlots()));
  ASSERT_NE(target, 0u);
  auto& npc_target = server.GetNpcManager().GetNpc(target)->get();
  npc_target.is_ingame = 1;
  npc_target.health = npc_target.state.health_points = 100;
  npc_target.state.life_state = PLAYER_LIFE_ALIVE;
  server.GetPlayerManager().GetPlayer(first)->get().spawned_players.insert(target);
  cast.target_id = target;
  cast.target_generation = npc_target.actor_generation;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerCastSpellName, [&](std::any) { EXPECT_TRUE(server.DestroyNpc(target)); }));
  reject();  // A resource destroys the target during validation.
}

TEST_F(ServerActorValidation, NpcHitRechecksHostAfterResourceCallback) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  auto& victim = server.GetPlayerManager().GetPlayer(second)->get();
  ASSERT_TRUE(
      EventManager::Instance().SubscribeToEvent(kEventOnPlayerHitName, [&](std::any) { EXPECT_TRUE(server.SetNpcHostPlayer(npc_id, second)); }));
  PlayerHitReportPacket hit{};
  hit.packet_type = Net::PT_PLAYER_HIT;
  hit.attacker_id = npc_id;
  hit.actor_generation = npc.actor_generation;
  hit.control_epoch = npc.control_epoch;
  hit.attack_id = 1;
  hit.victim_id = second;
  hit.victim_generation = victim.actor_generation;
  hit.damage = 10;
  hit.damage_type = 2;
  Send(70001, hit);
  EXPECT_EQ(victim.health, 100);
}

TEST_F(ServerActorValidation, UnprofiledUseDispatchesBeforeOwnershipCheck) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerUseItemName, [&](std::any event) {
    const auto request = std::any_cast<OnPlayerUseItemEvent>(event);
    EXPECT_EQ(request.player_id, npc_id);
    ++requests;
  }));
  ActorActionPacket use;
  use.actor_id = npc_id;
  use.actor_generation = npc.actor_generation;
  use.control_epoch = npc.control_epoch;
  use.action_id = 1;
  use.kind = ActorActionKind::ItemUse;
  use.item_instance = "ITPO_TEST";
  use.quantity = 4;  // The request is observable even though the actor owns only three.
  use.health_delta = 50;
  npc.health = npc.state.health_points = 10;
  Send(70001, use);
  EXPECT_EQ(requests, 1);
  EXPECT_EQ(npc.health, 10);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 3);
  use.command_revision = npc.command_revision;
  use.action_id = 2;
  use.quantity = 1;
  Send(70001, use);
  Send(70001, use);  // Duplicate cannot dispatch or consume twice.
  EXPECT_EQ(requests, 2);
  EXPECT_EQ(npc.health, 60);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 2);
}

TEST_F(ServerActorValidation, ReusableItemUseDoesNotConsumeOrRequireAProfile) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  ActorActionPacket use;
  use.actor_id = npc_id;
  use.actor_generation = npc.actor_generation;
  use.control_epoch = npc.control_epoch;
  use.action_id = 1;
  use.kind = ActorActionKind::ItemUse;
  use.item_instance = "ITPO_TEST";
  use.quantity = 0;
  use.mana_delta = 1000000;
  npc.mana = npc.state.mana_points = 20;
  Send(70001, use);
  EXPECT_EQ(npc.mana, npc.max_mana);
  EXPECT_EQ(npc.InventoryCount("ITPO_TEST"), 3);
}

TEST_F(ServerActorValidation, ScriptInventoryAndEquipmentAcceptNamesAbsentFromAnyCatalog) {
  ASSERT_TRUE(server.GiveItem(first, "custom_potion", 3));
  EXPECT_EQ(server.HasItem(first, "CUSTOM_POTION"), 3);
  EXPECT_FALSE(server.GiveItem(first, "INVALID NAME", 1));
  EXPECT_FALSE(server.GiveItem(first, std::string(256, 'A'), 1));
  ASSERT_TRUE(server.EquipArmor(first, "custom_armor"));
  EXPECT_EQ(server.GetCharacter(first)->get().state.equipped_armor_instance, "CUSTOM_ARMOR");
  EXPECT_EQ(server.HasItem(first, "custom_armor"), 1);
  ASSERT_TRUE(server.RemoveItem(first, "custom_armor", 1));
  EXPECT_TRUE(server.GetCharacter(first)->get().state.equipped_armor_instance.empty());
  EXPECT_TRUE(server.PlayAnimation(first, "CUSTOM_ANIMATION"));
  EXPECT_TRUE(server.StopAnimation(first, "CUSTOM_ANIMATION"));
}

TEST_F(ServerActorValidation, GenericEquipmentCommitsOnlyTheCurrentControllersNamedAcknowledgement) {
  ASSERT_TRUE(server.EquipItem(first, "CUSTOM_RING"));
  auto& actor = server.GetCharacter(first)->get();
  ASSERT_TRUE(actor.pending_equipment_commands.contains(Net::EQUIP_SLOT_GENERIC));
  const auto command = actor.pending_equipment_commands.at(Net::EQUIP_SLOT_GENERIC);
  EquipmentResultPacket result;
  result.actor_id = first;
  result.actor_generation = actor.actor_generation;
  result.command_revision = command.command_revision;
  result.item_instance = result.observed_instance = "CUSTOM_RING";
  result.observed_slot = Net::EQUIP_SLOT_RING_RIGHT;
  result.success = true;
  Send(70002, result);
  EXPECT_TRUE(actor.state.equipped_ring_right_instance.empty());
  EXPECT_TRUE(actor.pending_equipment_commands.contains(Net::EQUIP_SLOT_GENERIC));
  Send(70001, result);
  EXPECT_EQ(actor.state.equipped_ring_right_instance, "CUSTOM_RING");
  EXPECT_FALSE(actor.pending_equipment_commands.contains(Net::EQUIP_SLOT_GENERIC));
  EXPECT_TRUE(actor.generic_equipment_requests.empty());
  ASSERT_TRUE(server.UnequipItem(first, "CUSTOM_RING"));
  Send(70001, result);
  EXPECT_TRUE(actor.state.equipped_ring_right_instance.empty());
}

TEST_F(ServerActorValidation, GenericNpcEquipmentWaitsForAHostWithoutGuessingItsSlot) {
  ASSERT_TRUE(server.SetNpcHostPlayer(npc_id, 0));
  ASSERT_TRUE(server.EquipItem(npc_id, "CUSTOM_ARMOR"));
  ASSERT_TRUE(server.EquipItem(npc_id, "CUSTOM_WEAPON"));
  const auto& npc = server.GetCharacter(npc_id)->get();
  EXPECT_EQ(npc.generic_equipment_requests, (std::vector<std::string>{"CUSTOM_ARMOR", "CUSTOM_WEAPON"}));
  EXPECT_TRUE(npc.pending_equipment_commands.empty());
  EXPECT_TRUE(npc.state.equipped_armor_instance.empty());
  EXPECT_TRUE(npc.state.melee_weapon_instance.empty());
}

TEST_F(ServerActorValidation, EquipmentEventsCanReplaceCancelledRequestsWithoutReenteringTheHandler) {
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerEquipRingName, [&](std::any) {
    ++requests;
    EXPECT_TRUE(server.EquipItem(npc_id, "CUSTOM_RING", Net::EQUIP_SLOT_RING_RIGHT));
    EventManager::Instance().CancelCurrentEvent();
  }));
  EXPECT_FALSE(server.EquipItem(npc_id, "CUSTOM_RING", Net::EQUIP_SLOT_RING_LEFT));
  const auto& npc = server.GetCharacter(npc_id)->get();
  EXPECT_EQ(requests, 1);
  EXPECT_TRUE(npc.state.equipped_ring_left_instance.empty());
  EXPECT_EQ(npc.state.equipped_ring_right_instance, "CUSTOM_RING");
}

TEST_F(ServerActorValidation, LuaItemEventsReceiveNamesAndCanCancelUseWithoutDefinitionClasses) {
  LuaScript script;
  auto& lua = script.GetLuaState();
  ASSERT_TRUE(lua.safe_script(R"(
    assert(ItemDefinition == nil and ItemCallbacks == nil)
    addEventHandler('onPlayerUseItem', function(id, instance, quantity, scheme)
      assert(instance == 'CUSTOM_POTION' and quantity == 1 and scheme == 'POTION')
      received_instance = instance
      cancelEvent()
    end)
  )", sol::script_pass_on_error).valid());
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  ASSERT_TRUE(npc.AddInventory("CUSTOM_POTION", 2));
  ActorActionPacket use;
  use.actor_id = npc_id;
  use.actor_generation = npc.actor_generation;
  use.control_epoch = npc.control_epoch;
  use.command_revision = npc.command_revision;
  use.action_id = 1;
  use.kind = ActorActionKind::ItemUse;
  use.item_instance = "custom_potion";
  use.scheme = "POTION";
  use.quantity = 1;
  Send(70001, use);
  EXPECT_EQ(lua["received_instance"].get<std::string>(), "CUSTOM_POTION");
  EXPECT_EQ(npc.InventoryCount("CUSTOM_POTION"), 2);
}

TEST_F(ServerActorValidation, NativeCastDeltaPreservesNewerManaCommandsAndConsecutiveCasts) {
  auto& npc = server.GetNpcManager().GetNpc(npc_id)->get();
  CastSpellPacket cast{};
  cast.packet_type = Net::PT_CASTSPELL;
  cast.caster_id = npc_id;
  cast.actor_generation = npc.actor_generation;
  cast.control_epoch = npc.control_epoch;
  cast.command_revision = npc.command_revision;
  cast.item_instance = "ITRU_TEST";
  cast.spell_id = 19;
  cast.mana_delta = -5;
  ASSERT_TRUE(server.SetCharacterMana(npc_id, 80));
  cast.action_id = 1;
  Send(70001, cast);
  EXPECT_EQ(npc.mana, 75);
  cast.action_id = 2;  // Same captured command revision, next genuine native cast.
  Send(70001, cast);
  EXPECT_EQ(npc.mana, 70);
  EXPECT_EQ(npc.approved_projectiles.size(), 2u);
}

TEST_F(ServerActorValidation, CancelledNativeEquipmentRequestKeepsPreviousEquipment) {
  auto& actor = server.GetPlayerManager().GetPlayer(first)->get();
  ASSERT_TRUE(actor.AddInventory("ITRI_TEST", 1));
  int requests = 0;
  ASSERT_TRUE(EventManager::Instance().SubscribeToEvent(kEventOnPlayerEquipRingName, [&](std::any event) {
    const auto request = std::any_cast<OnPlayerEquipRingEvent>(event);
    EXPECT_EQ(request.item_instance, "ITRI_TEST");
    EXPECT_FALSE(request.previous_item_instance);
    ++requests;
    EventManager::Instance().CancelCurrentEvent();
  }));
  PlayerStateUpdatePacket packet{};
  packet.packet_type = Net::PT_ACTUAL_STATISTICS;
  packet.player_id = first;
  packet.actor_generation = actor.actor_generation;
  packet.command_revision = actor.command_revision;
  packet.state_sequence = 1;
  packet.state = actor.state;
  packet.state.nrot = {0, 0, 1};
  packet.state.source_time_ms = 1000;
  packet.state.equipped_ring_left_instance = "ITRI_TEST";
  Send(70001, packet);
  EXPECT_EQ(requests, 1);
  EXPECT_EQ(actor.state.equipped_ring_left_instance, "");
  EXPECT_EQ(actor.InventoryCount("ITRI_TEST"), 1);
}

TEST(ServerActorLuaBindings, AnimationGetterReturnsReportedNamesForPlayersAndNpcs) {
  GameServer server;
  LuaScript script;
  auto& lua = script.GetLuaState();
  const auto player_id = server.GetPlayerManager().AddPlayer(70001, "Animated player", 1);
  const auto npc_id = server.CreateNpc("Animated NPC");
  ASSERT_NE(player_id, 0u);
  ASSERT_NE(npc_id, 0u);
  lua["test_player_id"] = player_id;
  lua["test_npc_id"] = npc_id;
  ASSERT_TRUE(lua.safe_script(R"(
    assert(type(getPlayerAni) == 'function' and getPlayerAniId == nil)
    assert(getPlayerAni(test_player_id) == nil and getPlayerAni(test_npc_id) == nil)
    assert(getPlayerAni(0) == nil)
  )", sol::script_pass_on_error).valid());
  server.GetCharacter(player_id)->get().state.animation_name = "S_CUSTOM_PLAYER_POSE";
  server.GetCharacter(npc_id)->get().state.animation_name = "S_CUSTOM_NPC_POSE";
  ASSERT_TRUE(lua.safe_script(R"(
    assert(getPlayerAni(test_player_id) == 'S_CUSTOM_PLAYER_POSE')
    assert(getPlayerAni(test_npc_id) == 'S_CUSTOM_NPC_POSE')
  )", sol::script_pass_on_error).valid());
  server.GetCharacter(player_id)->get().state.animation_name.clear();
  EXPECT_TRUE(lua.safe_script("assert(getPlayerAni(test_player_id) == nil)", sol::script_pass_on_error).valid());
}

TEST(ServerActorLuaBindings, FunctionsAndConstantsUseProductionBindings) {
  GameServer server;
  LuaScript script;
  auto& lua = script.GetLuaState();
  const auto id = server.GetPlayerManager().AddPlayer(70001, "LuaActor", 1);
  lua["test_actor_id"] = id;
  const auto definitions = lua.safe_script(R"(
    for _, name in ipairs({'setPlayerPosition', 'setPlayerInvulnerable', 'isPlayerInvulnerable'}) do
      assert(type(_G[name]) == 'function', name)
    end
    assert(setPlayerPosition(test_actor_id, 1, 2, 3))
    assert(setPlayerPosition(test_actor_id, 4, 5, 6, true))
    assert(setPlayerPosition(test_actor_id, 7, 8, 9, false))
    assert(EQUIP_SLOT_GENERIC == -1 and EQUIP_SLOT_MELEE_WEAPON == -3)
    assert(EQUIP_SLOT_RING_LEFT == -9 and EQUIP_SLOT_RING_RIGHT == -10)
    assert(setPlayerInvulnerable(test_actor_id, true))
    assert(isPlayerInvulnerable(test_actor_id))
    assert(setPlayerInvulnerable(test_actor_id, false))
    assert(not isPlayerInvulnerable(test_actor_id))
    assert(setPlayerRespawnTime(test_actor_id, 1))
    assert(getPlayerRespawnTime(test_actor_id) == 1)
    assert(not setPlayerRespawnTime(test_actor_id, -1))
    assert(type(getPlayersStreamedToPlayer(test_actor_id)) == 'table')
    assert(type(getPlayersStreamingPlayer(test_actor_id)) == 'table')
    assert(type(setVoiceEnabled) == 'function' and type(getVoiceRange) == 'function')
    assert(type(setVoiceEnabledForPlayer) == 'function')
    assert(Voice == nil and setCharacterPosition == nil and getCharactersStreamedToPlayer == nil)
    assert(type(callEvent) == 'function' and type(triggerClientEvent) == 'function')
    assert(emitEvent == nil and sendClientEvent == nil)
    assert(setPlayerHealth(test_actor_id, 75) and getPlayerHealth(test_actor_id) == 75)
    assert(setPlayerExp(test_actor_id, 123) and getPlayerExp(test_actor_id) == 123)
    local npc, createError = createNpc('Contract NPC')
    assert(npc ~= nil and createError == nil and isNpc(npc) and not isNpc(test_actor_id))
    assert(not isNpc(-1) and not isNpc(npc + 4294967296))
    assert(setPlayerPosition(npc, 10, 20, 30) and getPlayerPosition(npc).x == 10)
    assert(setPlayerHealth(npc, 75) and getPlayerHealth(npc) == 75)
    assert(getNpcHostPlayer(npc) == nil and getNpcActionsCount(npc) == 0)
    local action, queueError = queueNpcAnimation(npc, 'T_STAND_2_SIT', 1500)
    assert(action ~= nil and queueError == nil and getNpcActionsCount(npc) == 1)
    assert(getNpcAction(npc, 0) == nil and getNpcAction(npc, 1).id == action)
    assert(getNpcAction(npc, 1).timeoutMs == 1500 and getNpcActions(npc)[1].id == action)
    assert(destroyNpc(npc) and getPlayerHealth(npc) == nil and getNpcActions(npc) == nil)
    local missingAction, missingError = queueNpcAnimation(npc, 'T_STAND_2_SIT')
    assert(missingAction == nil and type(missingError.message) == 'string')
    assert(setItemUseProfile == nil and setItemSpellProfile == nil)
    assert(ItemCallbacks == nil and ItemDefinition == nil)
  )",
                                           sol::script_pass_on_error);
  ASSERT_TRUE(definitions.valid());
  ASSERT_TRUE(server.GetCharacterPosition(id));
  EXPECT_EQ(*server.GetCharacterPosition(id), glm::vec3(7, 8, 9));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  const auto project = std::filesystem::current_path();
  const auto scratch = project / "build" / "actor-unit-tests";
  std::filesystem::create_directories(scratch);
  std::filesystem::current_path(scratch);
  const auto result = RUN_ALL_TESTS();
  std::filesystem::current_path(project);
  return result;
}

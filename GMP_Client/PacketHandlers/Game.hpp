
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "../CGmpClient.h"
#include "../CChat.h"

extern zCOLOR RED;
namespace Game {
	void OnMapName(CGmpClient* client, Packet packet) {
		const char* currentMap = oCGame::GetGame()->GetGameWorld()->GetWorldName().ToChar();
		//TODO: refactor sekretnych algorytmow GMP
		if (memcmp(packet.data + 1, currentMap, 
			(packet.length - 1 > strlen(currentMap) + 1) ? strlen(currentMap) + 1 : packet.length - 1)) {
			client->map = ((char*)packet.data + 1);
		}
		else if (!client->map.IsEmpty()){
			client->map.Clear(); 
		}
		zCOption::GetOption()->ChangeDir(WORK_DATA_WORLDS);
		zFILE_VDFS* vdf = oCObjectFactory::GetFactory()->CreateZFile((char*)packet.data + 1);
		if (vdf->Exists()) {
			vdf->Open(false);
			BYTE* mapBuffer = new BYTE[vdf->Size()];
			vdf->Read(mapBuffer, vdf->Size());
			MD5Sum* md5 = client->GetMD5(mapBuffer, vdf->Size());
			vdf->Close();
			delete[] mapBuffer;
			BYTE buffer[18];
			ZeroMemory(buffer, 18);
			buffer[0] = Net::PT_CHECKSUM;
			buffer[1] = client->AreDefaultItems();
			if (md5) {
				memcpy(buffer + 2, md5, 16);
			}
			client->network->Send((const char*)buffer, 18, Net::IMMEDIATE_PRIORITY, Net::RELIABLE);
		}
	}

	void OnInGame(CGmpClient* client, Packet packet) {
		client->IsInGame = true;
	}

	//TODO: To nie moze byc takie duze
	void OnActualStatistics(CGmpClient* client, Packet packet) {
		//Defines z CGmpClient.cpp
		static const zSTRING Door = "DOOR";
		static const zSTRING BowSound = "BOWSHOOT";
		static const zSTRING CrossbowSound = "CROSSBOWSHOOT";
		static const zSTRING Lever = "LEVER";
		static const zSTRING Touchplate = "TOUCHPLATE";
		static const zSTRING VWheel = "VWHEEL";
		static const zSTRING STUMBLE = "T_STUMBLE";
		static const zSTRING GOTHIT = "T_GOTHIT";
		static const zSTRING HitSound = "SVM_15_AARGH_4";
		static const zSTRING Arrows = "ITRW_ARROW";
		static const zSTRING Bolt = "ITRW_BOLT";

		if (client->game_mode == 1) {
			for (size_t i = 1; i < client->player.size(); i++) {
				if (!memcmp((*client->classmgr)[client->player[i]->char_class]->team_name.ToChar(),
					(*client->classmgr)[client->player[0]->char_class]->team_name.ToChar(),
					(*client->classmgr)[client->player[0]->char_class]->team_name.Length())) {
					client->player[i]->npc->variousFlags = 1 << 1;
					client->player[i]->SameTeamAsLocalPlayer = true;
				}
			}
		}
		bool found;
		unsigned int pIt = 1;
		while (pIt < packet.length) {
			CPlayer* player = nullptr;
			size_t i;
			found = false;
			for (i = 0; i < client->player.size(); i++) {
				player = nullptr;
				if (!memcmp(packet.data + pIt, &client->player[i]->id, sizeof(uint64_t))) {
					found = true;
					player = client->player[i];
					break; 
				};
			}
			if (!found) {
				pIt += sizeof(uint64_t) + 51;
				continue;
			}
			if (i) {
				pIt += sizeof(uint64_t);
				pIt++;	//miejsce na przetwarzanie flag
				zVEC3 pos;
				memcpy(&pos, packet.data + pIt, 12); pIt += 12;
				if (!player->enable) {
					player->npc->Enable(pos);
					player->enable = TRUE;
				}
				else player->AnalyzePosition(pos);
				float nrot[3];
				memcpy(nrot, packet.data + pIt, 12); pIt += 12;
				if (!player->npc->IsDead()) {
					player->npc->SetAngle(-nrot[0], nrot[1], 0.0f, nrot[0], nrot[1], nrot[2]);
				}
				short word;
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word == 0) {
					if (player->npc->GetLeftHand()) {
						oCItem* lptr = player->npc->GetLeftHand();
						oCItem* ptr = player->npc->GetRightHand();
						player->npc->DropAllInHand();
						lptr->RemoveVobFromWorld();
						if (ptr) player->npc->SetRightHand(ptr);
					}
				}
				else if (word > 5892 && word < 7850) {
					if (!player->npc->GetLeftHand()) {
						oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
						player->npc->SetLeftHand(New);
						client->CheckForSpecialEffects(player->npc->GetLeftHand(), player->npc);
					}
					else {
						if (player->npc->GetLeftHand()->GetInstance() != static_cast<int>(word)) {
							oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
							oCItem* Old = player->npc->GetLeftHand();
							player->npc->SetLeftHand(New);
							Old->RemoveVobFromWorld();
							client->CheckForSpecialEffects(player->npc->GetLeftHand(), player->npc);
						}
					}
				}
				//sprawdzanie lewej reki
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word == 0) {
					if (player->npc->GetRightHand()) {
						oCItem* rptr = player->npc->GetRightHand();
						oCItem* ptr = player->npc->GetLeftHand();
						player->npc->DropAllInHand();
						if (ptr) player->npc->SetLeftHand(ptr);
						client->PlayDrawSound(rptr, player->npc, false);
						rptr->RemoveVobFromWorld();
					}
				}
				else if (word > 5892 && word < 7850) {
					if (!player->npc->GetRightHand()) {
						oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
						player->npc->SetRightHand(New);
						client->PlayDrawSound(player->npc->GetRightHand(), player->npc, true);
					}
					else {
						if (player->npc->GetRightHand()->GetInstance() != static_cast<int>(word)) {
							oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
							oCItem* Old = player->npc->GetRightHand();
							player->npc->SetRightHand(New);
							Old->RemoveVobFromWorld();
							client->PlayDrawSound(player->npc->GetRightHand(), player->npc, true);
						}
					}
				}
				//sprawdzanie prawej reki
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word == 0) {
					if (player->npc->GetEquippedArmor()) {
						player->npc->UnequipItem(player->npc->GetEquippedArmor());
					}
				}
				else if (word > 5892 && word < 7850) {
					if (!player->npc->GetEquippedArmor()) {
						if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
							oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
							player->npc->GetInventory()->Insert(New);
							player->npc->Equip(New);
						}
						else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
					}
					else {
						if (player->npc->GetEquippedArmor()->GetInstance() != static_cast<int>(word)) {
							if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
								oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
								player->npc->GetInventory()->Insert(New);
								player->npc->Equip(New);
							}
							else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
						}
					}
				}
				//sprawdzanie zbroi
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (player->npc && word > 0 && word < 1400) {
					zCModelAni* Ani = player->npc->GetModel()->GetAniFromAniID(static_cast<int>(word));
					if (Ani && !player->npc->IsDead()) {
						if (!IsBadCodePtr((FARPROC)&Ani->GetAniName())) {
							if (!Ani->GetAniName().IsEmpty()) {
								if (Ani->GetAniName().Search(Door) == 2) {
									if (!player->npc->GetInteractMob()) {
										oCMobLockable* ALocked = static_cast<oCMobLockable*>(player->npc->FindMobInter(Door));
										if (ALocked) {
											ALocked->SetMobBodyState(player->npc);
											ALocked->AI_UseMobToState(player->npc, !ALocked->GetState());
										}
									}
									else {
										player->npc->GetInteractMob()->SetMobBodyState(player->npc);
										player->npc->GetInteractMob()->AI_UseMobToState(player->npc, !player->npc->GetInteractMob()->GetState());
									}
								}
								else if (player->npc->GetInteractMob()) {
									if (player->npc->GetInteractMob()->GetState() == 0)
										player->npc->GetInteractMob()->SendEndInteraction(player->npc, 0, 1);
									else player->npc->GetInteractMob()->SendEndInteraction(player->npc, 1, 0);
								}
								if (!player->npc->GetModel()->IsAnimationActive(Ani->GetAniName())) {
									player->npc->GetModel()->StartAnimation(Ani->GetAniName());
									if (!memcmp("T_BOWRELOAD", Ani->GetAniName().ToChar(), 11)) {
										oCItem* Bullet = oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex(Arrows));
										zCSoundSystem::GetSoundSystem()->PlaySound3D(BowSound, client->player[0]->npc, 2);
										player->npc->SetRightHand(Bullet);
										oCItem* Arrowe = player->npc->GetInventory()->IsIn(7083, 1);
										if (Arrowe) player->npc->GetInventory()->Remove(7083, 1);
										player->npc->DoShootArrow(1);
									}
									if (!memcmp("T_CBOWRELOAD", Ani->GetAniName().ToChar(), 12)) {
										oCItem* Bullet = oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex(Bolt));
										zCSoundSystem::GetSoundSystem()->PlaySound3D(CrossbowSound, client->player[0]->npc, 2);
										player->npc->SetLeftHand(Bullet);
										oCItem* Bolte = player->npc->GetInventory()->IsIn(7084, 1);
										if (Bolte) player->npc->GetInventory()->Remove(7084, 1);
										player->npc->DoShootArrow(1);
									}
									if (!memcmp("T_LEVER_S0_2_S1", Ani->GetAniName().ToChar(), 15)) {
										oCMobInter* LeverSwitch = player->npc->FindMobInter(Lever);
										if (LeverSwitch) {
											if (!LeverSwitch->GetTriggerName().IsEmpty()) {
												zCMover* Mover = static_cast<zCMover*>(oCGame::GetGame()->GetGameWorld()->SearchVobByName(LeverSwitch->GetTriggerName()));
												if (Mover)Mover->TriggerMover(Mover);
											}
										}
									}
									if (!memcmp("T_TOUCHPLATE_S0_2_S1", Ani->GetAniName().ToChar(), 20) || !memcmp("T_TOUCHPLATE_S1_2_S0", Ani->GetAniName().ToChar(), 20)) {
										oCMobInter* LeverSwitch = player->npc->FindMobInter(Touchplate);
										if (LeverSwitch) {
											if (!LeverSwitch->GetTriggerName().IsEmpty()) {
												zCMover* Mover = static_cast<zCMover*>(oCGame::GetGame()->GetGameWorld()->SearchVobByName(LeverSwitch->GetTriggerName()));
												if (Mover)Mover->TriggerMover(Mover);
											}
										}
									}
									if (!memcmp("T_VWHEEL_S0_2_S1", Ani->GetAniName().ToChar(), 16)) {
										oCMobInter* LeverSwitch = player->npc->FindMobInter(VWheel);
										if (LeverSwitch) {
											if (!LeverSwitch->GetTriggerName().IsEmpty()) {
												zCMover* Mover = static_cast<zCMover*>(oCGame::GetGame()->GetGameWorld()->SearchVobByName(LeverSwitch->GetTriggerName()));
												if (Mover)Mover->TriggerMover(Mover);
											}
										}
									}
								}
							}
						}
					}
				}
				//sprawdzanie animacji
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word > player->npc->GetHealth()) {
					if (player->npc->GetHealth() == 1) {
						player->npc->RefreshNpc();
						player->npc->SetHealth(1);
					}
				}
				if ((!player->hp) && (word == player->npc->GetMaxHealth())) {
					player->hp = word;
					auto current_pos = player->npc->GetPosition();
					player->npc->ResetPos(current_pos);
				}
				else if ((player->npc->GetHealth() > 0) && (word == 0)) {
					player->hp = 0;
				}
				else {
					if (player->update_hp_packet >= 5) {
						player->hp = word;
						player->npc->SetHealth(static_cast<int>(word));
						player->update_hp_packet = 0;
					}
					else player->update_hp_packet++;
				}
				//update hp
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				player->npc->SetMana(static_cast<int>(word));
				// update many
				memcpy(&word, packet.data + pIt, 1); pIt++;
				BYTE SpellNr = static_cast<BYTE>(word);
				if (SpellNr != player->npc->GetActiveSpellNr() && SpellNr > 0 && SpellNr < 100)
				{
					for (int s = 0; s < player->npc->GetSpellBook()->GetNoOfSpells(); s++) {
						player->npc->Equip(player->npc->GetSpellBook()->GetSpellItem(s));
					}
					oCItem* SpellItem = player->npc->GetSpellItem((int)SpellNr);
					if (SpellItem) {
						player->npc->Equip(SpellItem);
						player->npc->GetSpellBook()->Open(0);
					}
				}
				else if (SpellNr == 0 && player->npc->GetActiveSpellNr() > 0) {
					player->npc->GetSpellBook()->Close(1);
				}
				else if (player->npc->IsDead()) {
					player->npc->GetSpellBook()->Close(1);
				}
				// update spella w rekach
				if ((BYTE)player->npc->GetWeaponMode() != packet.data[pIt]) {
					player->npc->SetWeaponMode((oCNpc_WeaponMode)packet.data[pIt]);
					pIt++;
				}
				//sprawdzenie pozycji bojowej
				switch ((CPlayer::HeadState)packet.data[pIt])
				{
				case CPlayer::HEAD_NONE:
					player->npc->GetAnictrl()->SetLookAtTarget(0.5f, 0.5f);
					break;
				case CPlayer::HEAD_LEFT:
					player->npc->GetAnictrl()->SetLookAtTarget(0.0f, 0.5f);
					break;
				case CPlayer::HEAD_RIGHT:
					player->npc->GetAnictrl()->SetLookAtTarget(1.0f, 0.5f);
					break;
				case CPlayer::HEAD_UP:
					player->npc->GetAnictrl()->SetLookAtTarget(0.5f, 0.0f);
					break;
				case CPlayer::HEAD_DOWN:
					player->npc->GetAnictrl()->SetLookAtTarget(0.5f, 1.0f);
					break;
				}
				pIt++;
				// synchro obrotu glowy
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word == 0) {
					if (player->npc->GetEquippedRangedWeapon()) {
						player->npc->UnequipItem(player->npc->GetEquippedRangedWeapon());
					}
				}
				else if (word > 5892 && word < 7850) {
					if (!player->npc->GetEquippedRangedWeapon()) {
						if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
							oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
							player->npc->GetInventory()->Insert(New);
							player->npc->Equip(New);
						}
						else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
					}
					else {
						if (player->npc->GetEquippedRangedWeapon()->GetInstance() != static_cast<int>(word)) {
							if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
								oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
								player->npc->GetInventory()->Insert(New);
								player->npc->Equip(New);
							}
							else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
						}
					}
				}
				//sprawdzanie broni dystansowej
				memcpy(&word, packet.data + pIt, 2); pIt += 2;
				if (word == 0) {
					if (player->npc->GetEquippedMeleeWeapon()) {
						player->npc->UnequipItem(player->npc->GetEquippedMeleeWeapon());
					}
				}
				else if (word > 5892 && word < 7850) {
					if (!player->npc->GetEquippedMeleeWeapon()) {
						if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
							oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
							player->npc->GetInventory()->Insert(New);
							player->npc->Equip(New);
						}
						else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
					}
					else {
						if (player->npc->GetEquippedMeleeWeapon()->GetInstance() != static_cast<int>(word)) {
							if (!player->npc->GetInventory()->IsIn(static_cast<int>(word), 1)) {
								oCItem* New = oCObjectFactory::GetFactory()->CreateItem(static_cast<int>(word));
								player->npc->GetInventory()->Insert(New);
								player->npc->Equip(New);
							}
							else player->npc->Equip(player->npc->GetInventory()->IsIn(static_cast<int>(word), 1));
						}
					}
				}
				//sprawdzanie broni do walki wrecz
			}
			else {
				pIt += sizeof(uint64_t);
				if (client->player[0]->update_hp_packet >= 5) {
					short last_hp = client->player[0]->hp;
					memcpy(&client->player[0]->hp, packet.data + pIt, 2); pIt += 2;
					if ((!last_hp) && (client->player[0]->hp == client->player[0]->npc->GetMaxHealth())) {
						client->player[0]->npc->RefreshNpc();
						client->player[0]->npc->SetMovLock(0);
						if (client->game_mode == 0) {
							client->player[0]->npc->ResetPos(*(*client->spawnpoint)[rand() % client->spawnpoint->GetSize()]);
						}
						if (client->game_mode == 1) {
							std::vector<const char*> team_list;
							team_list.push_back((*client->classmgr)[0]->team_name.ToChar());
							for (size_t x = 1; x < client->classmgr->GetSize(); x++) {
								bool match = false;
								for (size_t y = 0; y < team_list.size(); y++) {
									if (!memcmp((*client->classmgr)[x]->team_name.ToChar(), team_list[y],
										strlen(team_list[y]) + 1)) {
										match = true; break;
									}
								}
								if (!match) {
									team_list.push_back((*client->classmgr)[x]->team_name.ToChar());
								}
							}
							size_t z;
							for (z = 0; z < team_list.size(); z++) {
								if (!memcmp((*client->classmgr)[client->player[0]->char_class]->team_name.ToChar(), team_list[z], strlen(team_list[z]) + 1)) {
									break;
								}
							}
							client->player[0]->npc->ResetPos(*(*client->spawnpoint)[(rand() % (client->spawnpoint->GetSize() / team_list.size())) * team_list.size() + z]);
							team_list.clear();
						}
					}
					else client->player[0]->SetHealth(static_cast<int>(client->player[0]->hp));
					client->player[0]->update_hp_packet = 0;
					if (static_cast<int>(client->player[0]->hp) < client->HeroLastHp) {
						if (!client->player[0]->npc->IsDead() && client->player[0]->npc->GetBodyState() != 10 && client->player[0]->npc->GetBodyState() != 12 && client->player[0]->npc->GetBodyState() != 22 && !client->player[0]->npc->GetAnictrl()->IsInWater()) {
							if (client->player[0]->npc->GetAnictrl()->IsRunning()) {
								client->player[0]->npc->GetModel()->StartAnimation(GOTHIT);
							}
							else {
								client->player[0]->npc->GetModel()->StartAnimation(STUMBLE);
							}
							int RandomSound = rand() % 2 + 1;
							if (RandomSound == 2) {
								zCSoundSystem::GetSoundSystem()->PlaySound3D(HitSound, client->player[0]->GetNpc(), 2);
							}
						}
						client->HeroLastHp = static_cast<int>(client->player[0]->hp);
					}
					else if (static_cast<int>(client->player[0]->hp) != client->HeroLastHp) client->HeroLastHp = static_cast<int>(client->player[0]->hp);
				}
				else {
					client->player[0]->update_hp_packet++;
					pIt += 2;
				}
			}
		}
	}

	void OnMapOnly(CGmpClient* client, Packet packet)
	{
		bool found;
		unsigned int pIt = 1;
		while (pIt < packet.length) {
			size_t i;
			found = false;
			for (i = 0; i < client->player.size(); i++) {
				if (!memcmp(packet.data + pIt, &client->player[i]->id, sizeof(uint64_t))) {
					found = true;
					break;
				}
			}
			if (found) {
				pIt += sizeof(uint64_t);
				float pos[2];
				memcpy(pos, packet.data + pIt, 8);
				client->player[i]->npc->SetPosition(pos[0], client->player[i]->npc->GetPosition().y, pos[1]);
				client->player[i]->DisablePlayer();
				pIt += 8;
			}
			else pIt += 8 + sizeof(uint64_t);
		}
	}

	void OnDoDie(CGmpClient* client, Packet packet)
	{
		for (size_t i = 0; i < client->player.size(); i++) {
			if (!memcmp(&client->player[i]->id, packet.data + 1, sizeof(uint64_t))) {
				client->player[i]->hp = 0;
				client->player[i]->update_hp_packet = 0;
				client->player[i]->SetHealth(0);
			}
		}
	}

	void OnRespawn(CGmpClient* client, Packet packet)
	{
		for (size_t i = 0; i < client->player.size(); i++) {
			if (!memcmp(&client->player[i]->id, packet.data + 1, sizeof(uint64_t))) {
				client->player[i]->RespawnPlayer();
			}
		}
	}

	void OnCastSpell(CGmpClient* client, Packet packet)
	{
		short SpellId = 0;
		memcpy(&SpellId, packet.data + 1 + sizeof(uint64_t), 1);
		if (SpellId >= 0 && SpellId < 100) {
			for (size_t i = 0; i < client->player.size(); i++) {
				if (!memcmp(&client->player[i]->id, packet.data + 1, sizeof(uint64_t))) {
					oCSpell* Spell = oCSpell::_CreateNewInstance();
					Spell->InitValues(SpellId);
					Spell->Setup(client->player[i]->GetNpc(), 0, 0);
					client->RunSpellLogic(SpellId, client->player[i]->GetNpc(), 0);
					client->RunSpellScript(Spell->GetSpellInstanceName(SpellId).ToChar(), client->player[i]->GetNpc());
					Spell->Cast();
				}
			}
		}
	}

	void OnCastSpellOnTarget(CGmpClient* client, Packet packet)
	{
		uint64_t TargetId;
		short SpellId = 0;
		memcpy(&TargetId, packet.data + 1 + sizeof(uint64_t), 8);
		memcpy(&SpellId, packet.data + 9 + sizeof(uint64_t), 1);
		if (SpellId >= 0 && SpellId < 100) {
			for (size_t i = 0; i < client->player.size(); i++) {
				if (!memcmp(&client->player[i]->id, packet.data + 1, sizeof(uint64_t))) {
					for (size_t s = 0; s < client->player.size(); s++) {
						if (!memcmp(&client->player[s]->id, &TargetId, sizeof(uint64_t))) {
							oCSpell* Spell = oCSpell::_CreateNewInstance();
							Spell->InitValues(SpellId);
							Spell->Setup(client->player[i]->GetNpc(), client->player[s]->GetNpc(), 0);
							client->RunSpellLogic(SpellId, client->player[i]->GetNpc(), client->player[s]->GetNpc());
							client->RunSpellScript(Spell->GetSpellInstanceName(SpellId).ToChar(), client->player[i]->GetNpc(), client->player[s]->GetNpc());
							Spell->Cast();
						}
					}
				}
			}
		}
	}

	void OnDropItem(CGmpClient* client, Packet packet)
	{
		short instance;
		short amount;
		memcpy(&instance, packet.data + 1 + sizeof(uint64_t), 2);
		memcpy(&amount, packet.data + 3 + sizeof(uint64_t), 2);
		if (instance > 5892 && instance < 7850) {
			for (size_t i = 0; i < client->player.size(); i++) {
				if (!memcmp(&client->player[i]->id, packet.data + 1, sizeof(uint64_t))) {
					oCWorld* world = oCGame::GetGame()->GetGameWorld();
					oCItem* NpcDrop = oCObjectFactory::GetFactory()->CreateItem(instance);
					NpcDrop->SetAmount(amount);
					zVEC3 startPos = client->player[i]->npc->GetTrafoModelNodeToWorld("ZS_RIGHTHAND").GetTranslation();
					NpcDrop->SetPosition(startPos);
					world->AddVob(NpcDrop);
					NpcDrop->SetSleeping(false);
					NpcDrop->SetStaticVob(false);
					NpcDrop->SetPhysicsEnabled(true);
				}
			}
		}
	}

	void OnTakeItem(CGmpClient* client, Packet packet)
	{
		short number;
		memcpy(&number, packet.data + 1 + sizeof(uint64_t), 2);
		if (number > 5892 && number < 7850) {
			zCListSort<oCItem>* ItemList = oCGame::GetGame()->GetWorld()->GetItemList();
			int size = ItemList->GetSize();
			for (size_t x = 0; x < client->player.size(); x++) {
				if (!memcmp(&client->player[x]->id, packet.data + 1, sizeof(uint64_t))) {
					for (int i = 0; i < size; i++) {
						ItemList = ItemList->GetNext();
						oCItem* ItemInList = ItemList->GetData();
						if (ItemInList->GetInstance() == number) {
							if (client->player[x]->npc->GetDistanceToVob(ItemInList) < 250.0f) {
								client->player[x]->npc->DoTakeVob(ItemInList);
								break;
							}
						}
					}
				}
			}
		}
	}

	void OnWhisper(CGmpClient* client, Packet packet)
	{
		size_t i;
		bool found = false;
		for (i = 0; i < client->player.size(); i++) {
			if (!memcmp(packet.data + 1, &client->player[i]->id, sizeof(uint64_t))) { 
				found = true;
				break;
			}
		}
		if (found) {
			static zCOLOR whisper_color(0, 255, 255, 255);
			CChat::GetInstance()->WriteMessage(WHISPER, true, whisper_color, "%s-> %s", client->player[i]->npc->GetName().ToChar(), (const char*)packet.data + 1 + sizeof(uint64_t) * 2);
		}
	}

	void OnMessage(CGmpClient* client, Packet packet)
	{
		size_t i = 0;
		bool found = false;
		for (i = 0; i < client->player.size(); i++) {
			if (!memcmp(packet.data + 1, &client->player[i]->id, sizeof(uint64_t))) {
				found = true;
				break;
			}
		}
		if (found) {
			CChat::GetInstance()->WriteMessage(NORMAL, false, "%s: %s", client->player[i]->npc->GetName().ToChar(), (const char*)packet.data + 1 + sizeof(uint64_t));
		}
	}

	void OnServerMessage(CGmpClient* client, Packet packet)
	{
		static zCOLOR server_message_color(255, 128, 0, 255);
		CChat::GetInstance()->WriteMessage(NORMAL, false, server_message_color, "(SERVER): %s", (const char*)packet.data + 1);
	}

	void OnRcon(CGmpClient* client, Packet packet)
	{
		if (packet.data[1] == 0x41) {
			client->IsAdminOrModerator = true;
		}
		CChat::GetInstance()->WriteMessage(ADMIN, false, RED, "%s", (char*)packet.data + 1);
	}

	void OnAllOthers(CGmpClient* client, Packet packet)
	{
		unsigned int pIt = 1;
		while (pIt < packet.length) {
			CPlayer* newhero = new CPlayer();
			newhero->enable = FALSE;
			memcpy(&newhero->id, packet.data + pIt, sizeof(uint64_t)); pIt += sizeof(uint64_t);
			oCNpc* npc = oCObjectFactory::GetFactory()->CreateNpc(oCNpc::GetHero()->GetInstance());
			newhero->SetNpc(npc);
			newhero->char_class = packet.data[pIt];
			client->classmgr->EquipNPC(packet.data[pIt], newhero, true); pIt++;
			newhero->npc->SetGuild(9);
			newhero->hp = static_cast<short>(newhero->GetHealth());
			zVEC3 pos;
			memcpy(&pos, packet.data + pIt, 12);
			newhero->SetPosition(pos); pIt += 12;
			pIt += 6;	//pomijam l/p zbroje
			if (newhero->Type == CPlayer::NPC_HUMAN) newhero->SetAppearance(packet.data[pIt], packet.data[pIt + 1], packet.data[pIt + 2]); pIt += 3;
			if (newhero->Type > CPlayer::NPC_DRACONIAN || newhero->Type == CPlayer::NPC_HUMAN) newhero->npc->ApplyOverlay(CPlayer::GetWalkStyleFromByte(packet.data[pIt])); pIt++;
			const char* heroname = (const char*)packet.data + pIt; pIt += strlen(heroname) + 1;
			newhero->SetName(heroname);
			newhero->update_hp_packet = 0;
			client->player.push_back(newhero);
		}
	}

	void OnJoinGame(CGmpClient* client, Packet packet)
	{
		CPlayer* newhero = new CPlayer();
		memcpy(&newhero->id, packet.data + 1, sizeof(uint64_t));
		zVEC3 pos;
		memcpy(&pos, packet.data + (2 + sizeof(uint64_t)), 12);
		oCNpc* npc = oCObjectFactory::GetFactory()->CreateNpc(oCNpc::GetHero()->GetInstance());
		newhero->SetNpc(npc);
		client->classmgr->EquipNPC(packet.data[1 + sizeof(uint64_t)], newhero, true);
		newhero->char_class = packet.data[1 + sizeof(uint64_t)];
		newhero->npc->SetGuild(9);
		newhero->hp = static_cast<short>(newhero->GetHealth());
		newhero->SetPosition(pos);
		const char* heroname = (const char*)packet.data + (38 + sizeof(uint64_t));
		newhero->SetName(heroname);
		if (newhero->Type == CPlayer::NPC_HUMAN) newhero->SetAppearance(packet.data[sizeof(uint64_t) + 34], packet.data[sizeof(uint64_t) + 35], packet.data[sizeof(uint64_t) + 36]);
		if (newhero->Type > CPlayer::NPC_DRACONIAN || newhero->Type == CPlayer::NPC_HUMAN) newhero->npc->ApplyOverlay(CPlayer::GetWalkStyleFromByte(packet.data[sizeof(uint64_t) + 37]));
		static zCOLOR color(0, 255, 0, 255);
		CChat::GetInstance()->WriteMessage(NORMAL, false, color, "%s%s", heroname, (*client->lang)[CLanguage::SOMEONE_JOIN_GAME].ToChar());
		newhero->enable = FALSE;
		newhero->update_hp_packet = 0;
		//kod
		client->player.push_back(newhero);
	}

	void OnYourName(CGmpClient* client, Packet packet)
	{
		zSTRING tmpStr = (const char*)(packet.data + 1);
		oCNpc::GetHero()->SetName(tmpStr);
		tmpStr.Clear();
	}

	void OnGameInfo(CGmpClient* client, Packet packet)
	{
		STime t;
		memcpy(&t, packet.data + 1, 4);
		oCGame::GetGame()->SetTime(static_cast<int>(t.day), static_cast<int>(t.hour), static_cast<int>(t.min));
		if (!client->IgnoreFirstTimeMessage) CChat::GetInstance()->WriteMessage(NORMAL, false, "Time set to: %d:%.2d", t.hour, t.min);
		client->IgnoreFirstTimeMessage = false;
		client->game_mode = packet.data[5];
		UsePotionKeys = packet.data[6] & 0x01;
		client->DropItemsAllowed = packet.data[6] & 0x02;
		client->ForceHideMap = packet.data[6] & 0x04;
		if (packet.data[6] & 0x08) memcpy(&client->mp_restore, packet.data + 7, 2);
		else client->mp_restore = 0;
	}

	void OnLeftGame(CGmpClient* client, Packet packet)
	{
		for (size_t i = 1; i < client->player.size(); i++) {
			if (!memcmp(packet.data + 1, &client->player[i]->id, sizeof(uint64_t))) {
				static zCOLOR color(255, 0, 0, 255);
				CChat::GetInstance()->WriteMessage(NORMAL, false, color, "%s%s", client->player[i]->GetName(), (*client->lang)[CLanguage::SOMEONEDISCONNECT_FROM_SERVER].ToChar());
				client->player[i]->LeaveGame();
				delete client->player[i];
				client->player.erase(client->player.begin() + i);
				break;
			}
		}
	}

	void OnVoice(CGmpClient* client, Packet packet)
    {
        int size;
		memcpy(&size, packet.data + 1, 4);
        char* buffer = new char[size];
        memcpy(buffer, packet.data + 5, size);
		client->voicePlayback->PlayVoice(buffer, size);
    }
}
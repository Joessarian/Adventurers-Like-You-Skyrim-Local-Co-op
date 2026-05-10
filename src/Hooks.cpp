#include "Hooks.h"
#include <Compatibility.h>
#include <DebugAPI.h>
#include <GlobalCoopData.h>
#include <Raycast.h>
#include <Settings.h>
#include <Util.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	namespace Hooks
	{
		using EventResult = RE::BSEventNotifyControl;
		void Install()
		{
			MainHook::InstallHook();
			ActivateHandlerHooks::InstallHooks();
			ActorEquipManagerHooks::InstallHooks();
			ActorMagicCasterHooks::InstallHooks();
			AIProcessHooks::InstallHooks();
			AnimationGraphManagerHooks::InstallHooks();
			AttackBlockHandlerHooks::InstallHooks();
			BarterMenuHooks::InstallHooks();
			BookMenuHooks::InstallHooks();
			BSMultiBoundHooks::InstallHooks();
			CharacterHooks::InstallHooks();
			ContainerMenuHooks::InstallHooks();
			CraftingMenuHooks::InstallHooks();
			DialogueMenuHooks::InstallHooks();
			FavoritesMenuHooks::InstallHooks();
			GiftMenuHooks::InstallHooks();
			JumpHandlerHooks::InstallHooks();
			InputEventHooks::InstallHooks();
			InventoryMenuHooks::InstallHooks();
			LegendarySkillResetConfirmCallbackHooks::InstallHooks();
			LoadingMenuHooks::InstallHooks();
			LookHandlerHooks::InstallHooks();
			MagicMenuHooks::InstallHooks();
			MagicStaggerHooks::InstallHooks();
			MeleeHitHooks::InstallHooks();
			MenuControlsHooks::InstallHooks();
			MovementHandlerHooks::InstallHooks();
			NiNodeHooks::InstallHooks();
			PlayerCameraTransitionStateHooks::InstallHooks();
			PlayerCharacterHooks::InstallHooks();
			ProjectileHooks::InstallHooks();
			RaceSexMenuHooks::InstallHooks();
			ReadyWeaponHandlerHooks::InstallHooks();
			ShoutHandlerHooks::InstallHooks();
			SleepWaitMenuHooks::InstallHooks();
			SneakHandlerHooks::InstallHooks();
			SpellItemHooks::InstallHooks();
			SprintHandlerHooks::InstallHooks();
			StatsMenuHooks::InstallHooks();
			TESCameraHooks::InstallHooks();
			TESObjectBOOKHooks::InstallHooks();
			TESObjectREFRHooks::InstallHooks();
			ThirdPersonCameraStatesHooks::InstallHooks();
			TogglePOVHandlerHooks::InstallHooks();
			TrainingMenuHooks::InstallHooks();
			ValueModifierEffectHooks::InstallHooks();
			VampireLordEffectHooks::InstallHooks();
			WerewolfEffectHooks::InstallHooks();
			INF("Installed all hooks");
		}

//=============
// [MAIN HOOK]:
//=============

		void MainHook::Update(RE::Main* a_this, float a_a2)
		{
#ifdef ALYSLC_PROFILING
			float modUpdateMS = 0.0f;
			float modUpdateMS2 = 0.0f;
			float funcMS = 0.0f;
			SteadyClock::time_point tp = SteadyClock::now();
			// Update allow-saving flag before running the main update.
			GlobalCoopData::UpdateAllowSavingFlag();
			// Run the game's update next.
			_Update(a_this, a_a2);
			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			DBG("Game update took {}ms.", funcMS);
			tp = SteadyClock::now();
			SteadyClock::time_point tpStart = SteadyClock::now();

			// Skip if global data isn't set yet.
			if (!glob.globalDataInit)
			{
				return;
			}

			if (glob.loadingASave)
			{
				return;
			}

			// Handle any changes to Enderal progression.
			// Eg. P1 level ups, or changes to crafting, memory, or learning points.
			if (ALYSLC::EnderalCompat::g_installed)
			{
				GlobalCoopData::HandleEnderalProgressionChanges();
			}

			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			modUpdateMS2 += funcMS;
			DBG("Enderal progression update took {}ms.", funcMS);
			tp = SteadyClock::now();

			// Update all connected controllers' button and analog states, in or out of co-op,
			// since we still need controller state data when in the co-op summoning menu.
			glob.cdh->UpdatePlayerControllerStates();
			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			modUpdateMS2 += funcMS;
			DBG("Controller state update took {}ms.", funcMS);
			tp = SteadyClock::now();

			// Cam/menu input managers run their update funcs next.
			glob.cam->Update();
			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			modUpdateMS2 += funcMS;
			DBG("Camera update took {}ms.", funcMS);
			tp = SteadyClock::now();

			glob.mim->Update();
			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			modUpdateMS2 += funcMS;
			DBG("MIM update took {}ms.", funcMS);
			tp = SteadyClock::now();

			// Draw the menu control overlay if a player is controlling menus 
			// while in co-op or the co-op summoning menu.
			glob.mim->DrawPlayerMenuControlOverlay();
			funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
			modUpdateMS2 += funcMS;
			DBG("PMC update took {}ms.", funcMS);
			tp = SteadyClock::now();

			if (glob.allPlayersInit)
			{
				// Update combat state first.
				GlobalCoopData::UpdatePlayerCoopCombatState();
				funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
				modUpdateMS2 += funcMS;
				DBG("Combat state update took {}ms.", funcMS);
				tp = SteadyClock::now();

				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive)
					{
						// NOTE: 
						// Update funcs must be run in this order.
						p->Update();
						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s P Update took {}ms.", 
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();

						p->em->Update();
						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s EM Update took {}ms.",
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();

						p->pam->Update();
						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s PAM Update took {}ms.",
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();

						p->mm->Update();
						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s M Update took {}ms.", 
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();

						p->tm->Update();
						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s TM Update took {}ms.", 
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();

						if (p->isDowned)
						{
							p->UpdateWhenDowned();
						}

						funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
						modUpdateMS2 += funcMS;
						DBG("{}'s downed update took {}ms.", 
							p->coopActor->GetName(), funcMS);
						tp = SteadyClock::now();
					}
				}
			}
			
			// Update crosshair text and check for arm collisions
			// after the players' managers have run their updates.
			tp = SteadyClock::now();
			if (glob.coopSessionActive)
			{
				GlobalCoopData::HandlePlayerArmCollisions();
				funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
				modUpdateMS2 += funcMS;
				DBG("Player arm collisions took {}ms.", funcMS);
				tp = SteadyClock::now();

				// Clear if a fullscreen menu is open.
				auto ui = RE::UI::GetSingleton();
				GlobalCoopData::SetCrosshairText
				(
					ui->GameIsPaused() || 
					ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) 
				);
				funcMS = Util::GetElapsedSeconds(tp, true) * 1000.0f;
				modUpdateMS2 += funcMS;
				DBG("Crosshair text update took {}ms.", funcMS);
			}

			modUpdateMS = Util::GetElapsedSeconds(tpStart, true) * 1000.0f;
			DBG
			(
				"Game global time delta: {}ms, ALYSLC update time delta: {}ms, {}ms.", 
				*g_deltaTimeRealTime * 1000.0f,
				modUpdateMS,
				modUpdateMS2
			);
#else 
			// Update allow-saving flag before running the main update.
			GlobalCoopData::UpdateAllowSavingFlag();

			// Skip if global data isn't set yet.
			if (!glob.globalDataInit)
			{
				return _Update(a_this, a_a2);;
			}

			if (glob.loadingASave)
			{
				return _Update(a_this, a_a2);;
			}

			// Run the game's update next.
			_Update(a_this, a_a2);

			// Handle any changes to Enderal progression.
			// Eg. P1 level ups, or changes to crafting, memory, or learning points.
			if (ALYSLC::EnderalCompat::g_installed)
			{
				GlobalCoopData::HandleEnderalProgressionChanges();
			}

			// Update all connected controllers' button and analog states, in or out of co-op,
			// since we still need controller state data when in the co-op summoning menu.
			glob.cdh->UpdatePlayerControllerStates();
			// Cam/menu input managers run their update funcs next.
			glob.cam->Update();
			glob.mim->Update();
			// Draw the menu control overlay if a player is controlling menus 
			// while in co-op or the co-op summoning menu.
			glob.mim->DrawPlayerMenuControlOverlay();
			if (glob.allPlayersInit)
			{
				// Update combat state first.
				GlobalCoopData::UpdatePlayerCoopCombatState();
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive)
					{
						p->Update();
					}
				}
			}
			
			// Update crosshair text and check for arm collisions
			// after the players' managers have run their updates.
			if (glob.coopSessionActive)
			{
				GlobalCoopData::HandlePlayerArmCollisions();
				// Clear if a fullscreen menu is open.
				auto ui = RE::UI::GetSingleton();
				GlobalCoopData::SetCrosshairText
				(
					ui->GameIsPaused() || 
					ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) 
				);
			}
#endif
		}

//=================
// [GENERAL HOOKS]:
//=================

// [AI PROCESS HOOKS]:
		void AIProcessHooks::AIProcess_SetRotationSpeedZ1
		(
			RE::AIProcess* a_this, float a_rotationSpeed
		)
		{
			// Players' rotation speeds are set elsewhere, so we'll skip the updates here.
			if (!glob.coopSessionActive)
			{
				return _AIProcess_SetRotationSpeedZ1(a_this, a_rotationSpeed);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetUserData());
			if ((playerIndex == -1) || (playerIndex == 0 && !glob.cam->IsRunning()))
			{
				return _AIProcess_SetRotationSpeedZ1(a_this, a_rotationSpeed);
			}
		}

		void AIProcessHooks::AIProcess_SetRotationSpeedZ2
		(
			RE::AIProcess* a_this, float a_rotationSpeed
		)
		{
			// Players' rotation speeds are set elsewhere, so we'll skip the updates here.	
			if (!glob.coopSessionActive)
			{
				return _AIProcess_SetRotationSpeedZ2(a_this, a_rotationSpeed);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetUserData()); 
			if ((playerIndex == -1) || (playerIndex == 0 && !glob.cam->IsRunning()))
			{
				return _AIProcess_SetRotationSpeedZ2(a_this, a_rotationSpeed);
			}
		}

		void AIProcessHooks::AIProcess_SetRotationSpeedZ3
		(
			RE::AIProcess* a_this, float a_rotationSpeed
		)
		{
			// Players' rotation speeds are set elsewhere, so we'll skip the updates here.	
			if (!glob.coopSessionActive)
			{
				return _AIProcess_SetRotationSpeedZ3(a_this, a_rotationSpeed);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetUserData()); 
			if ((playerIndex == -1) || (playerIndex == 0 && !glob.cam->IsRunning()))
			{
				return _AIProcess_SetRotationSpeedZ3(a_this, a_rotationSpeed);
			}
		}

// [ACTOR EQUIP MANAGER HOOKS]:

		// IMPORTANT:
		// This hook will not fire if the original EquipObject call's passed in extra data list
		// has 'ExtraWorn/ExtraWornLeft' data to match the slot the item is being equipped to.
		// This is why the item must be added to the inventory chest first.
		void ActorEquipManagerHooks::EquipObject
		(
			RE::ActorEquipManager* a_this, 
			RE::Actor* a_actor, 
			RE::TESBoundObject* a_object, 
			const RE::ObjectEquipParams& a_objectEquipParams
		)
		{
			if (!a_actor || !a_object || !glob.globalDataInit || !glob.coopSessionActive)
			{
				return _EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_actor); 
			if (playerIndex == -1)
			{
				return _EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}
			
			DBG
			(
				"{}: {} (0x{:X}, type: 0x{:X}, exList: {:p}). Force equip: {}, Unks: {}, {}.", 
				a_actor->GetName(),
				a_object->GetName(),
				a_object->formID,
				*a_object->formType,
				fmt::ptr(a_objectEquipParams.extraDataList),
				a_objectEquipParams.forceEquip,
				a_objectEquipParams.unk23,
				a_objectEquipParams.unk24
			);
			// Ignore if P1, transform(ing/ed), or skipping equip processing.
			const auto& p = glob.coopPlayers[playerIndex];
			if (p->isPlayer1 || p->isTransforming || p->isTransformed || p->em->skipEquipProcessing)
			{
				// Still can auto-equip ammo with ALYSLC's system if this actor is P1.
				if (p->isPlayer1)
				{
					// Do not equip anything onto P1 if another player's inventory 
					// is copied over to them.
					if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
					{
						DBG
						(
							"Cannot equip {} when another player's inventory is copied over to P1.",
							a_object->GetName()
						);
						return;
					}
					else
					{
						p->em->AutoEquipAmmo(a_object);
					}
				}

				return _EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}
				
			bool inInventory = p->coopActor->GetInventory().contains(a_object);
			DBG("In inventory: {}.", inInventory);

			RE::BGSEquipSlot* equipSlotToRestore = nullptr;
			if (auto reqEquipSlot = a_objectEquipParams.equipSlot; reqEquipSlot)
			{
				// Ensure 1H weapon gets equipped in the correct hand.
				// Certain races, like the Manakin race, 
				// do not equip weapons in the LH 
				// unless the weapon's equip slot is set to the LH slot.
				// Save the original equip slot to restore after the equip call.
				// NOTE: 
				// Has not been tested thoroughly for long-term side-effects, 
				// but nothing to report so far.
				if (!p->coopActor->race || !p->coopActor->race->GetPlayable()) 
				{
					if (auto itemEquipType = a_object->As<RE::BGSEquipType>(); 
						itemEquipType && itemEquipType->equipSlot)
					{
						if (a_object->As<RE::TESObjectWEAP>())
						{
							// Right equip slot to left. Set to either.
							if (reqEquipSlot == glob.leftHandEquipSlot && 
								itemEquipType->equipSlot == glob.rightHandEquipSlot)
							{
								equipSlotToRestore = glob.rightHandEquipSlot;
								itemEquipType->SetEquipSlot(glob.eitherHandEquipSlot);
							}
							// Left equip slot to right. Set to right.
							else if (reqEquipSlot == glob.rightHandEquipSlot && 
									 itemEquipType->equipSlot == glob.leftHandEquipSlot)
							{
								equipSlotToRestore = glob.leftHandEquipSlot;
								itemEquipType->SetEquipSlot(glob.rightHandEquipSlot);
							}
						}
					}
				}
			}

			// Do not want to unequip auto-equipped bound objects, 
			// since these objects are typically equipped after a spell is cast
			// and take the place of the cast spell in its hand slot.
			// Also ignore instances where the game equips the "fists" weapon to clear hand slots.
			bool isBound = 
			(
				(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
				(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
			);
			if (!isBound && a_object != glob.fists && a_object != glob.dummy1H)
			{
				// Ignore calls to force equip the item, as this can lead 
				// to the weapon's model remaining equipped while not tagged as equipped 
				// in the inventory.
				if (a_objectEquipParams.forceEquip)
				{
					return;
				}
			}
			else 
			{
				// Special bound weapon handling for co-op companions.
				if (isBound)
				{
					// Have to check if this bound weapon was equipped
					// following a request from the player.
					// The game will automatically try to equip the bound bow 
					// even if we've unequipped it on weapon sheathe 
					// or after its duration has elapsed.
					if (a_object->IsWeapon())
					{
						auto weap = a_object->As<RE::TESObjectWEAP>();
						auto equipSlotToUse = weap->equipSlot;
						// Ugh. Sometimes the object has the same name and equip slot 
						// but a different form ID and is a different pointer 
						// when compared to the cached requested weapon.
						// Ex. Cast the normal 'Bound Battleaxe' spell, 
						// but the Mystic version's weapon is equipped.
						// Also check if the names are the same to determine equivalence
						// if this occurs.
						bool reqToEquip = 
						(
							(
								(
									p->pam->boundWeapReqLH &&
									equipSlotToUse == glob.leftHandEquipSlot
								) &&
								(
									(a_object == p->em->lastReqBoundWeapLH) ||
									(
										p->em->lastReqBoundWeapLH &&
										strncmp
										(
											a_object->GetName(),
											p->em->lastReqBoundWeapLH->GetName(),
											strlen(a_object->GetName())
										) == 0
									)
								)
							) ||
							(
								(
									p->pam->boundWeapReqRH &&
									equipSlotToUse == glob.rightHandEquipSlot
								) &&
								(
									(a_object == p->em->lastReqBoundWeapRH) ||
									(
										p->em->lastReqBoundWeapRH &&
										strncmp
										(
											a_object->GetName(),
											p->em->lastReqBoundWeapRH->GetName(),
											strlen(a_object->GetName())
										) == 0
									)
								)
							) ||
							(
								(
									p->pam->boundWeapReq2H &&
									equipSlotToUse == glob.bothHandsEquipSlot
								) &&
								(
									(a_object == p->em->lastReqBoundWeapRH) ||
									(
										p->em->lastReqBoundWeapRH &&
										strncmp
										(
											a_object->GetName(),
											p->em->lastReqBoundWeapRH->GetName(),
											strlen(a_object->GetName())
										) == 0
									)
								)
							)
						);

						// CHANGE TO DEBUG
						DBG
						(
							"{}: trying to equip bound weapon {} (0x{:X}) with equip slot {}. "
							"{}. Reqs: {}, {}, {}. Equipped objects: {}, {}, "
							"Already equipped: {}. Ammo: {}, {}. "
							"Last requested bound weapons: {} (0x{:X}), {} (0x{:X}), "
							"comps: {}, {}. Form names match: {}, {}.", 
							a_actor->GetName(),
							a_object->GetName(),
							a_object->formID,
							equipSlotToUse ? Util::GetEditorID(equipSlotToUse) : "NONE",
							reqToEquip ? "ALLOWING" : "IGNORING",
							p->pam->boundWeapReq2H,
							p->pam->boundWeapReqLH,
							p->pam->boundWeapReqRH,
							p->coopActor->GetEquippedObject(true) ? 
							p->coopActor->GetEquippedObject(true)->GetName() :
							"NONE",
							p->coopActor->GetEquippedObject(false) ? 
							p->coopActor->GetEquippedObject(false)->GetName() :
							"NONE",
							equipSlotToUse == glob.leftHandEquipSlot ?
							p->coopActor->GetEquippedObject(true) == a_object :
							p->coopActor->GetEquippedObject(false) == a_object,
							p->coopActor->GetCurrentAmmo() ? 
							p->coopActor->GetCurrentAmmo()->GetName() :
							"NONE",
							p->em->equippedForms[!EquipIndex::kAmmo] ? 
							p->em->equippedForms[!EquipIndex::kAmmo]->GetName() :
							"NONE",
							p->em->lastReqBoundWeapLH ? 
							p->em->lastReqBoundWeapLH->GetName() : 
							"NONE",
							p->em->lastReqBoundWeapLH ? 
							p->em->lastReqBoundWeapLH->formID : 
							0xDEAD,
							p->em->lastReqBoundWeapRH ? 
							p->em->lastReqBoundWeapRH->GetName() : 
							"NONE",
							p->em->lastReqBoundWeapRH ? 
							p->em->lastReqBoundWeapRH->formID : 
							0xDEAD,
							a_object == p->em->lastReqBoundWeapLH,
							a_object == p->em->lastReqBoundWeapRH,
							p->em->lastReqBoundWeapLH &&
							strncmp
							(
								a_object->GetName(),
								p->em->lastReqBoundWeapLH->GetName(),
								strlen(a_object->GetName())
							) == 0,
							p->em->lastReqBoundWeapRH &&
							strncmp
							(
								a_object->GetName(),
								p->em->lastReqBoundWeapRH->GetName(),
								strlen(a_object->GetName())
							) == 0
						);

						auto aem = RE::ActorEquipManager::GetSingleton();
						if (reqToEquip)
						{
							// Player did request to equip this bound weapon,
							// so also equip previously equipped bound ammo, if any.
							// Ammo might have been unequipped silently, so re-equip here.
							auto cachedAmmo = p->em->equippedForms[!EquipIndex::kAmmo];
							if ((cachedAmmo) && 
								(cachedAmmo->HasKeywordByEditorID("WeapTypeBoundArrow")) && 
								(weap->IsBow() || weap->IsCrossbow()))
							{
								Util::EquipObject
								(
									p->coopActor.get(), cachedAmmo->As<RE::TESAmmo>()
								);
							}
						}
						else
						{
							// Player did not request to equip a bound weapon
							// or the weapon is already equipped, so ignore.
							return;
						}
					}
				}
				else if (a_object == glob.fists || a_object == glob.dummy1H)
				{
					// Unlike for P1, fists do not automatically get unequipped 
					// for companion players, so do it here.
					_EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
					UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
					return;
				}
			}

			_EquipObject(a_this, a_actor, a_object, a_objectEquipParams);

			// Restore original equip slot if modified.
			if (equipSlotToRestore)
			{
				auto itemEquipType = a_object->As<RE::BGSEquipType>();
				if (itemEquipType)
				{
					itemEquipType->SetEquipSlot(equipSlotToRestore);
				}
			}
		}

		void ActorEquipManagerHooks::UnequipObject
		(
			RE::ActorEquipManager* a_this,
			RE::Actor* a_actor,
			RE::TESBoundObject* a_object,
			const RE::ObjectEquipParams& a_objectEquipParams
		)
		{
			if (!a_actor || !a_object || !glob.globalDataInit || !glob.coopSessionActive)
			{
				return _UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_actor); 
			if (playerIndex == -1)
			{
				return _UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}
			
			DBG
			(
				"{}: {} (0x{:X}, type: 0x{:X}, exList: {:p}). Force equip: {}, Unks: {}, {}.", 
				a_actor->GetName(), 
				a_object->GetName(),
				a_object->formID, 
				*a_object->formType,
				fmt::ptr(a_objectEquipParams.extraDataList),
				a_objectEquipParams.forceEquip,
				a_objectEquipParams.unk23,
				a_objectEquipParams.unk24
			);

			// Ignore if P1, transform(ing/ed), or skipping equip processing.
			const auto& p = glob.coopPlayers[playerIndex];
			if (p->isPlayer1 || p->isTransforming || p->isTransformed || p->em->skipEquipProcessing)
			{
				// Do not unequip anything from P1 if another player's inventory 
				// is copied over to them.
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					DBG
					(
						"Cannot unequip {} when another player's inventory is copied over to P1.",
						a_object->GetName()
					);
					return;
				}
				else
				{
					return _UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
				}
			}

			bool isBound = 
			(
				(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
				(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
			);
			if (isBound && p->coopActor->IsWeaponDrawn())
			{
				DBG
				(
					"{}: trying to unequip bound weapon {}. Equip slot: {}.", 
					a_actor->GetName(),
					a_object->GetName(), 
					Util::GetEditorID(a_objectEquipParams.equipSlot)
				);
				// TODO: 
				// Prevent the game from unequipping bound weapons if their duration 
				// has not expired yet and the player has drawn their weapons.
				auto aem = RE::ActorEquipManager::GetSingleton();
				auto weap = a_object->As<RE::TESObjectWEAP>();
				if (p->pam->boundWeapReq2H)
				{
					float remainingLifetime = 
					(
						p->pam->secsBoundWeapon2HDuration - p->pam->secsSinceBoundWeap2HReq
					);
					if ((remainingLifetime > 0.0f) && 
						(a_object == p->em->lastReqBoundWeapRH || a_object->IsAmmo()))
					{
						DBG
						(
							"{}: trying to unequip bound 2H weapon/ammo {}. "
							"Time left: {}. Ignoring.", 
							a_actor->GetName(),
							a_object->GetName(), 
							p->pam->secsBoundWeapon2HDuration - p->pam->secsSinceBoundWeap2HReq
						);
						return;
					}
				} 
				
				if (p->pam->boundWeapReqLH)
				{
					float remainingLifetime = 
					(
						p->pam->secsBoundWeaponLHDuration - p->pam->secsSinceBoundWeapLHReq
					);
					if (remainingLifetime > 0.0f && 
						a_object == p->em->lastReqBoundWeapLH &&
						a_objectEquipParams.equipSlot == glob.leftHandEquipSlot)
					{
						DBG
						(
							"{}: trying to unequip bound LH weapon {}. Time left: {}. Ignoring.", 
							a_actor->GetName(),
							a_object->GetName(), 
							p->pam->secsBoundWeaponLHDuration - p->pam->secsSinceBoundWeapLHReq
						);
						return;
					}
				}
				
				if (p->pam->boundWeapReqRH)
				{
					float remainingLifetime = 
					(
						p->pam->secsBoundWeaponRHDuration - p->pam->secsSinceBoundWeapRHReq
					);
					if (remainingLifetime > 0.0f && 
						a_object == p->em->lastReqBoundWeapRH &&
						a_objectEquipParams.equipSlot == glob.rightHandEquipSlot)
					{
						DBG
						(
							"{}: trying to unequip bound RH weapon {}. Time left: {}. Ignoring.", 
							a_actor->GetName(),
							a_object->GetName(), 
							p->pam->secsBoundWeaponRHDuration - p->pam->secsSinceBoundWeapRHReq
						);
						return;
					}
				}
			}
			else if (a_object != glob.fists && a_object != glob.dummy1H)
			{
				bool inInventory = p->coopActor->GetInventory().contains(a_object);
				DBG("In inventory: {}.", inInventory);
				if (!inInventory)
				{
					DBG("Skip unequipping {} because it is not in {}'s inventory.", 
						a_object->GetName(), p->coopActor->GetName());
					return;
				}
			}

			return _UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
		}

// [ACTOR MAGIC CASTER HOOKS]:
		void ActorMagicCasterHooks::ClearMagicNode(RE::ActorMagicCaster * a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _ClearMagicNode(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _ClearMagicNode(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _ClearMagicNode(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}
			else if (pIndex == 0)
			{
				auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);
			}

			_ClearMagicNode(a_this);
		}

		void ActorMagicCasterHooks::DeselectSpellImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _DeselectSpellImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _DeselectSpellImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _DeselectSpellImpl(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}
			else if (pIndex == 0)
			{
				auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);
			}

			_DeselectSpellImpl(a_this);
		}

		void ActorMagicCasterHooks::FinishCastImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _FinishCastImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _FinishCastImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _FinishCastImpl(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}

			_FinishCastImpl(a_this);
		}
		
		void ActorMagicCasterHooks::InterruptCastImpl
		(
			RE::ActorMagicCaster* a_this, bool a_depleteEnergy
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _InterruptCastImpl(a_this, a_depleteEnergy);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _InterruptCastImpl(a_this, a_depleteEnergy);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _InterruptCastImpl(a_this, a_depleteEnergy);
				}

				auto source = a_this->GetCastingSource();
				if (source != RE::MagicSystem::CastingSource::kLeftHand &&
					source != RE::MagicSystem::CastingSource::kRightHand)
				{
					return _InterruptCastImpl(a_this, a_depleteEnergy);
				}

				// Can restore magicka when the spell is fully charged,
				// even when the call is trying to deplete magicka.
				// Pre-cast state only.
				if (*a_this->state <= RE::MagicCaster::State::kUnk04)
				{
					// Think the 'deplete magicka' arg might actually be 'refund magicka'.
					_InterruptCastImpl(a_this, true);
					// Magicka gets depleted/refunded in another function run after this call,
					// so if we set the magicka charged to 0, 
					// it will be refunded to the pre-cast level.
					a_this->costCharged = 0.0f;
				}
				else
				{
					_InterruptCastImpl(a_this, a_depleteEnergy);
				}

				/*DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}, cost charged: {}. State: {}, deplete energy: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					a_this->costCharged,
					*a_this->state,
					a_depleteEnergy
				);*/
				return;
			}

			return _InterruptCastImpl(a_this, a_depleteEnergy);
		}

		void ActorMagicCasterHooks::RequestCastImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _RequestCastImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _RequestCastImpl(a_this);
			}

			if (a_this->currentSpell)
			{
				// Apply player-specific magicka multiplier,
				// check if the new cost is less than the player's current magicka level,
				// and if it is, force-allow the cast by disabling the cast check 
				// before running the original request func,
				// which would normally reject the cast due to the unmodified cost 
				// being larger than the player's current magicka level.
				// Restore the original cast checks afterward.
				const auto& p = glob.coopPlayers[pIndex];
				if (!p->IsRunning())
				{
					return _RequestCastImpl(a_this);
				}

				auto source = a_this->GetCastingSource();
				/*DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
				// Ignore requests to cast when the companion player 
				// has not started performing the corresponding cast input action.
				if (!p->isPlayer1)
				{
					bool notRequested = 
					(
						(
							source == RE::MagicSystem::CastingSource::kLeftHand &&
							p->pam->castingGlobVars[!CastingGlobIndex::kLH]->value == 0.0f &&
							p->pam->castingGlobVars[!CastingGlobIndex::k2H]->value == 0.0f
						) ||
						(
							source == RE::MagicSystem::CastingSource::kRightHand &&
							p->pam->castingGlobVars[!CastingGlobIndex::kRH]->value == 0.0f&&
							p->pam->castingGlobVars[!CastingGlobIndex::k2H]->value == 0.0f
						)
					);
					if (notRequested)
					{
						DBG
						(
							"{}: {} cast of {} not requested.",
							p->coopActor->GetName(),
							source == RE::MagicSystem::CastingSource::kLeftHand ?
							"LH" :
							"RH",
							a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE"
						);
						a_this->currentSpell = nullptr;
						a_this->state = RE::MagicCaster::State::kNone;
						return;
					}
				}

				// No cost while in god mode.
				a_this->currentSpellCost *= 
				(
					p->isInGodMode ? 
					0.0f :
					Settings::vfMagickaCostMult[p->playerID]
				);
				float cost = a_this->currentSpellCost;
				if (a_this->currentSpell->GetCastingType() == 
					RE::MagicSystem::CastingType::kConcentration)
				{
					cost *= *g_deltaTimeRealTime;
				}

				if (p->isInGodMode || 
					cost <= p->coopActor->GetActorValue(RE::ActorValue::kMagicka))
				{
					a_this->flags.set(RE::ActorMagicCaster::Flags::kSkipCheckCast);
					_RequestCastImpl(a_this);
					a_this->flags.reset(RE::ActorMagicCaster::Flags::kSkipCheckCast);
				}

				return;
			}

			_RequestCastImpl(a_this);
		}

		void ActorMagicCasterHooks::SelectSpellImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _SelectSpellImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _SelectSpellImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _SelectSpellImpl(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}

			_SelectSpellImpl(a_this);
		}

		void ActorMagicCasterHooks::SetCurrentSpellImpl
		(
			RE::ActorMagicCaster* a_this, RE::MagicItem* a_spell
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _SetCurrentSpellImpl(a_this, a_spell);

			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _SetCurrentSpellImpl(a_this, a_spell);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _SetCurrentSpellImpl(a_this, a_spell);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}

			_SetCurrentSpellImpl(a_this, a_spell);
		}

		void ActorMagicCasterHooks::SpellCast
		(
			RE::ActorMagicCaster* a_this,
			bool a_doCast,
			uint32_t a_arg2, 
			RE::MagicItem* a_spell
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _SpellCast(a_this, a_doCast, a_arg2, a_spell);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _SpellCast(a_this, a_doCast, a_arg2, a_spell);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _SpellCast(a_this, a_doCast, a_arg2, a_spell);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}. "
					"Casting globs: LH: {}, RH: {}, 2H: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state,
					p->pam->castingGlobVars[!CastingGlobIndex::kLH]->value,
					p->pam->castingGlobVars[!CastingGlobIndex::kRH]->value,
					p->pam->castingGlobVars[!CastingGlobIndex::k2H]->value
				);*/
			}

			_SpellCast(a_this, a_doCast, a_arg2, a_spell);
		}

		void ActorMagicCasterHooks::StartCastImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _StartCastImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _StartCastImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _StartCastImpl(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}

			_StartCastImpl(a_this);
		}

		bool ActorMagicCasterHooks::StartChargeImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _StartChargeImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _StartChargeImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _StartChargeImpl(a_this);
				}

				/*DBG
				(
					"{}, caster {}, spell {}, cost {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
					
				auto source = a_this->GetCastingSource();
				// Set cast start TPs.
				if (source == RE::MagicSystem::CastingSource::kLeftHand)
				{
					p->lastLHCastChargeStartTP = SteadyClock::now();
				}
				else if (source == RE::MagicSystem::CastingSource::kRightHand)
				{
					p->lastRHCastChargeStartTP = SteadyClock::now();
				}
			}

			return _StartChargeImpl(a_this);
		}

		void ActorMagicCasterHooks::StartReadyImpl(RE::ActorMagicCaster* a_this)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _StartReadyImpl(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _StartReadyImpl(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _StartReadyImpl(a_this);
				}

				/*auto source = a_this->GetCastingSource();
				DBG
				(
					"{}, caster {} (performing: {}, just started: {}, type: {}), "
					"spell {}, cost: {}. State: {}.",
					a_this->actor->GetName(),
					!a_this->castingSource,
					(
						source == RE::MagicSystem::CastingSource::kLeftHand && 
						p->pam->IsPerforming(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand && 
						p->pam->IsPerforming(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->JustStarted(InputAction::kCastLH)
					) ||
					(
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->JustStarted(InputAction::kCastRH)
					),
					(
						source == RE::MagicSystem::CastingSource::kLeftHand &&
						p->pam->IsPerforming(InputAction::kCastLH) &&
						p->em->GetLHSpell() ?
						!p->em->GetLHSpell()->GetCastingType() :
						source == RE::MagicSystem::CastingSource::kRightHand &&
						p->pam->IsPerforming(InputAction::kCastRH) && 
						p->em->GetRHSpell() ?
						!p->em->GetRHSpell()->GetCastingType() :
						!RE::MagicSystem::CastingType::kConstantEffect
					),
					a_this->currentSpell ? a_this->currentSpell->GetName() : "NONE",
					a_this->currentSpellCost,
					*a_this->state
				);*/
			}

			_StartReadyImpl(a_this);
		}

		void ActorMagicCasterHooks::Update(RE::ActorMagicCaster* a_this, float a_delta)
		{
			// Stall companion players' hand cast if casting a fire and forget (FNF) spell.

			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _Update(a_this, a_delta);
			}

			auto source = a_this->GetCastingSource();
			auto state = a_this->state;
			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->actor);
			if (pIndex == -1)
			{
				return _Update(a_this, a_delta);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Is a companion player.
			if (pIndex > 0)
			{
				if (!p->IsRunning())
				{
					return _Update(a_this, a_delta);
				}

				auto lhSpell = p->em->GetLHSpell();
				auto rhSpell = p->em->GetRHSpell();
				bool isCasting = 
				(
					(p->pam->IsPerforming(InputAction::kCastLH)) ||
					(p->pam->IsPerforming(InputAction::kCastRH)) ||
					(  
						(
							p->pam->IsPerforming(InputAction::kSpecialAction)
						) &&
						(
							p->pam->reqSpecialAction == SpecialActionType::kCastBothHands ||
							p->pam->reqSpecialAction == SpecialActionType::kDualCast
						)
					)
				);
				bool is2HCast = 
				(
					isCasting &&
					lhSpell &&
					rhSpell &&
					lhSpell == rhSpell &&
					rhSpell->equipSlot == glob.bothHandsEquipSlot
				);
				bool stallCastUntilBindReleased = false;
				// NOTE:
				// 
				// 2H spellcasts seem to use only one hand caster (usually the RH caster).
				// We do not want to link the player's pressed cast bind 
				// to the act of casting itself, since the 2H cast can be triggered 
				// through the opposite hand's cast bind, 
				// but that caster isn't guaranteed to have the 2H spell equipped and active.
				// For example, we can cast 'Blizzard' with the 'CastLH' bind,
				// but the player's LH caster will remain inactive,
				// while its RH caster equips and charges the spell.
				if (is2HCast)
				{
					stallCastUntilBindReleased = 
					(
						a_this->currentSpell == rhSpell &&
						rhSpell->GetCastingType() == 
						RE::MagicSystem::CastingType::kFireAndForget &&
						a_this->state == RE::MagicCaster::State::kReady 
					);
				}
				else
				{
					if (source == RE::MagicSystem::CastingSource::kLeftHand)
					{
						stallCastUntilBindReleased = 
						(
							(
								lhSpell &&
								lhSpell->GetCastingType() == 
								RE::MagicSystem::CastingType::kFireAndForget &&
								a_this->state == RE::MagicCaster::State::kReady 
							) &&
							(
								(p->pam->AllButtonsPressedForAction(InputAction::kCastLH)) ||
								(
									p->pam->AllButtonsPressedForAction
									(
										InputAction::kSpecialAction
									) &&
									p->pam->reqSpecialAction ==
									SpecialActionType::kCastBothHands
								)
							)
						);
					}
					else if (source == RE::MagicSystem::CastingSource::kRightHand)
					{
						stallCastUntilBindReleased = 
						(
							(
								rhSpell &&
								rhSpell->GetCastingType() == 
								RE::MagicSystem::CastingType::kFireAndForget &&
								a_this->state == RE::MagicCaster::State::kReady 
							) &&
							(
								(p->pam->AllButtonsPressedForAction(InputAction::kCastRH)) ||
								(
									p->pam->AllButtonsPressedForAction
									(
										InputAction::kSpecialAction
									) &&
									p->pam->reqSpecialAction == 
									SpecialActionType::kCastBothHands
								)
							)
						);
					}
				}
					
				// Modify caster state to loop the charge animation 
				// just when the caster reaches the ready state.
				// Returning directly afterward without performing the update 
				// seems to do the trick.
				// NOTE:
				// Does not work for spell animations that do not reach the ready state (3).
				if (stallCastUntilBindReleased)
				{
					// Looping ready animation.
					a_this->state = RE::ActorMagicCaster::State::kUnk04;
					// Skip this update.
					return;
				}

				if (a_this->currentSpell)
				{
					const float baseCost = a_this->currentSpell->CalculateMagickaCost
					(
						p->coopActor.get()
					);
					float cost = baseCost;
					if (a_this->currentSpell->GetCastingType() == 
						RE::MagicSystem::CastingType::kConcentration)
					{
						cost *= *g_deltaTimeRealTime;
					}
				}

				// Setting the 'skip check' flag only during the 'start' state
				// suddenly re-enables the game's caster magicka expenditure?
				// It just works.
				if (*a_this->state == RE::MagicCaster::State::kUnk01)
				{	
					a_this->flags.set(RE::ActorMagicCaster::Flags::kSkipCheckCast);
				}
				else
				{
					a_this->flags.reset(RE::ActorMagicCaster::Flags::kSkipCheckCast);
				}
			}
			// Commented out until making sure the method is thread-safe.
			// Insertion of the input event is currently only done 
			// when the PAM main task runs on the main thread.
			/*else if (pIndex == 0 && p->IsRunning())
			{
				auto source = a_this->GetCastingSource();
				auto ui = RE::UI::GetSingleton();
				bool isLH = source == RE::MagicSystem::CastingSource::kLeftHand;
				bool isRH = source == RE::MagicSystem::CastingSource::kRightHand;
				bool isStillCasting =
				(
					(
						(isLH || isRH) &&
						(
							glob.menuPID > 0 &&
							ui && 
							!ui->GameIsPaused() && 
							*a_this->state == RE::MagicCaster::State::kNone
						)
					) &&
					(
						(
							isLH && p->pam->IsPerforming(InputAction::kCastLH)
						) ||
						(
							isRH && p->pam->IsPerforming(InputAction::kCastRH)
						) ||
						(
							(
								p->pam->IsPerforming(InputAction::kSpecialAction)
							) &&
							(
								p->pam->reqSpecialAction == SpecialActionType::kCastBothHands ||
								p->pam->reqSpecialAction == SpecialActionType::kDualCast
							)
						)
					)
				);
				if (isStillCasting)
				{
					DBG("Restart cast for source {} on menu opening.", 
						!a_this->castingSource);
					p->pam->QueueP1ButtonEvent
					(
						source == RE::MagicSystem::CastingSource::kLeftHand ?
						InputAction::kCastLH :
						InputAction::kCastRH,
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kRelease,
						0.0f, 
						false
					);
					p->pam->QueueP1ButtonEvent
					(
						source == RE::MagicSystem::CastingSource::kLeftHand ?
						InputAction::kCastLH :
						InputAction::kCastRH,
						RE::INPUT_DEVICE::kGamepad, 
						ButtonEventPressType::kInstantTrigger,
						0.0f, 
						false
					);
					return;
				}
			}*/

			// Make sure the magic caster's target matches the player's selected target.
			// Otherwise, any first-frame damage spells, like beam projectiles, 
			// will hit the target set when first starting the cast, 
			// instead of the current target, which may be different.
			const auto chosenTarget = p->tm->GetRangedTargetActor();
			a_this->desiredTarget = chosenTarget;
			_Update(a_this, a_delta);
			a_this->desiredTarget = chosenTarget;
		}

// [ANIMATION GRAPH MANAGER HOOKS]:
		EventResult AnimationGraphManagerHooks::ProcessEvent
		(
			RE::BSAnimationGraphManager* a_this, 
			const RE::BSAnimationGraphEvent* a_event, 
			RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource
		)
		{
			if (!glob.globalDataInit || 
				!glob.allPlayersInit || 
				!glob.coopSessionActive ||
				!a_event || 
				!a_event->holder)
			{
				return _ProcessEvent(a_this, a_event, a_eventSource);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_event->holder->formID); 
			if (playerIndex == -1)
			{
				return _ProcessEvent(a_this, a_event, a_eventSource);
			}

			const auto& p = glob.coopPlayers[playerIndex];
			if (!p->pam->IsRunning())
			{
				return _ProcessEvent(a_this, a_event, a_eventSource);
			}

			const auto& pam = p->pam;
			// When transformed into the Vampire Lord:
			// (un)equip spells and start/stop glow FX.
			if (!p->isPlayer1 && p->isTransformed && Util::IsVampireLord(p->coopActor.get())) 
			{
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						auto vampireBodyFloatFX = 
						(
							dataHandler->LookupForm<RE::BGSArtObject>(0x15FC5, "Dawnguard.esm")
						);
						// Play/stop hit effect art.
						if (a_event->tag == "GroundStart")
						{
							Util::StopHitArt(p->coopActor.get(), vampireBodyFloatFX);
							aem->EquipObject(p->coopActor.get(), glob.fists);
						}
						else if (a_event->tag == "LevitateStart")
						{
							Util::StartHitArt
							(
								p->coopActor.get(), vampireBodyFloatFX, p->coopActor.get()
							);
							RE::SpellItem* leveledDrainSpell = nullptr;
							RE::SpellItem* leveledRaiseDeadSpell = nullptr;
							float playerLevel = p->coopActor->GetLevel();
							if (playerLevel <= 10.0f)
							{
								leveledDrainSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x19324, "Dawnguard.esm"
									)
								);
								leveledRaiseDeadSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0xBA54, "Dawnguard.esm"
									)
								);
							}
							else if (playerLevel <= 20.0f)
							{
								leveledDrainSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x19326, "Dawnguard.esm"
									)
								);
								leveledRaiseDeadSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x13EC8, "Dawnguard.esm"
									)
								);
							}
							else if (playerLevel <= 30.0f)
							{
								leveledDrainSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x19AD5, "Dawnguard.esm"
									)
								);
								leveledRaiseDeadSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x13EC9, "Dawnguard.esm"
									)
								);
							}
							else if (playerLevel <= 40.0f)
							{
								leveledDrainSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x19AD6, "Dawnguard.esm"
									)
								);
								leveledRaiseDeadSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x13ECA, "Dawnguard.esm"
									)
								);
							}
							else
							{
								leveledDrainSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x19AD7, "Dawnguard.esm"
									)
								);
								leveledRaiseDeadSpell = 
								(
									dataHandler->LookupForm<RE::SpellItem>
									(
										0x13ECB, "Dawnguard.esm"
									)
								);
							}

							if (leveledDrainSpell)
							{
								leveledDrainSpell = 
								(
									p->em->CopyToPlaceholderSpell
									(
										leveledDrainSpell, PlaceholderMagicIndex::kRH
									)
								);
								aem->EquipSpell
								(
									p->coopActor.get(), 
									leveledDrainSpell->As<RE::SpellItem>(), 
									glob.rightHandEquipSlot
								);
							}

							if (leveledRaiseDeadSpell)
							{
								leveledRaiseDeadSpell = 
								(
									p->em->CopyToPlaceholderSpell
									(
										leveledRaiseDeadSpell, PlaceholderMagicIndex::kLH
									)
								);
								aem->EquipSpell
								(
									p->coopActor.get(), 
									leveledRaiseDeadSpell->As<RE::SpellItem>(), 
									glob.leftHandEquipSlot
								);
							}
						}
					}
				}
			}

			// Match supported animation event tags with the event's tag.
			const auto tagHash = Hash(a_event->tag);
			// Update camera shake state.
			if (p->isPlayer1)
			{
				// DBG("{}: {}", p->coopActor->GetName(), a_event->tag);
				if (tagHash == "StartAnimatedCameraDelta"_h)
				{
					glob.isCameraShakeActive = true;
				}
				else if (tagHash == "EndAnimatedCamera"_h)
				{
					glob.isCameraShakeActive = false;
				}
			}

			// Set performed action anim event tag so that the player action manager
			// can handle AV modification later when it updates, which minimizes processing done
			// by this game thread.
			auto perfAVAnimEvent = 
			(
				std::pair<PerfAnimEventTag, uint16_t>(PerfAnimEventTag::kNone, 0)
			);
			switch (tagHash)
			{
			// Starting to cast a spell.
			case ("BeginCastLeft"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kBeginCastLeft, pam->lastAnimEventID };
				break;
			}
			case ("BeginCastRight"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kBeginCastRight, pam->lastAnimEventID };
				break;
			}
			// Ranged weapon projectile release.
			case ("BowRelease"_h):
			case ("BowReleaseFast"_h):
			case ("arrowRelease"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kBowRelease, pam->lastAnimEventID };
				break;
			}
			// Releasing charged spell.
			case ("MLh_SpellFire_Event"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kCastLeft, pam->lastAnimEventID };
				break;
			}
			case ("MRh_SpellFire_Event"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kCastRight, pam->lastAnimEventID };
				break;
			}
			// Shield Charge/Sprint/Sneak Roll started.
			case ("StartAnimatedCameraDelta"_h):
			case ("StartAnimatedCamera"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kSprintStart, pam->lastAnimEventID };
				break;
			}
			// TK Dodge start.
			case ("TKDR_DodgeStart"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kDodgeStart, pam->lastAnimEventID };
				break;
			}
			// Melee weapon attack is about to collide.
			// Set weapon damage mult, as needed.
			case ("preHitFrame"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kPreHitFrame, pam->lastAnimEventID };
				break;
			}
			// Attack animation hit frame.
			case ("HitFrame"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kHitFrame, pam->lastAnimEventID };
				break;
			}
			// Attack complete. 
			// Reset weapon damage mult once the attack stop animation event fires.
			case ("attackStop"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kAttackStop, pam->lastAnimEventID };
				break;
			}
			// Stop casting.
			case ("CastStop"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kCastStop, pam->lastAnimEventID };
				break;
			}
			// Sprint/sneak roll stopped.
			case ("EndAnimatedCameraDelta"_h):
			case ("EndAnimatedCamera"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kSprintStop, pam->lastAnimEventID };
				break;
			}
			// TK Dodge end.
			case ("TKDodgeStop"_h):
			case ("TKDR_DodgeEnd"_h):
			{
				perfAVAnimEvent = { PerfAnimEventTag::kDodgeStop, pam->lastAnimEventID };
				break;
			}
			case ("Collision_AttackStart"_h):
			{
				break;
			}
			case ("Collision_Add"_h):
			{
				break;
			}
			case ("Collision_Remove"_h):
			{
				break;
			}
			default:
				// No need to handle.
				return EventResult::kContinue;
			}

			// Increment with wrap-around.
			pam->lastAnimEventID = 
			(
				pam->lastAnimEventID == UINT16_MAX ?
				1 : 
				pam->lastAnimEventID + 1
			);

			// Improves recovery speed for transition from dodge end to movement start.
			if (Hash(a_event->tag) == "TKDodgeStop"_h || Hash(a_event->tag) == "TKDR_DodgeEnd"_h)
			{
				p->mm->SetDontMove(true);
				p->coopActor->NotifyAnimationGraph("moveStop");
				if (p->lsMoved)
				{
					p->mm->SetDontMove(false);
					p->coopActor->NotifyAnimationGraph("moveStart");
				}
			}
			
			DBG("{}: {}", p->coopActor->GetName(), a_event->tag);
			p->lastAnimEventTag = a_event->tag;

			DBG
			(
				"{}: Getting Lock. (0x{:X})",
				p->coopActor->GetName(),
				std::hash<std::jthread::id>()(std::this_thread::get_id())
			);
			{
				std::unique_lock<std::mutex> perfAnimQueueLock(p->pam->avcam->perfAnimQueueMutex);
				DBG
				(
					"{}: Lock obtained. (0x{:X})", 
					p->coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				if (perfAVAnimEvent.first != PerfAnimEventTag::kNone)
				{
					p->pam->avcam->perfAnimEventsQueue.emplace(std::move(perfAVAnimEvent));
				}
			}

			return _ProcessEvent(a_this, a_event, a_eventSource);
		}

// [BS MULTI BOUND HOOKS]:
		bool BSMultiBoundHooks::QWithinPoint(RE::BSMultiBound* a_this, const RE::NiPoint3& a_pos)
		{
			// Ensure players are not occluded by any occlusion volumes 
			// in the current cell.
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if (!p1 || !p1->parentCell)
			{
				return _QWithinPoint(a_this, a_pos);
			}

			auto cell = p1->parentCell;
			auto sky = RE::TES::GetSingleton() ? RE::TES::GetSingleton()->sky : nullptr;
			if (!cell || !sky)
			{
				return _QWithinPoint(a_this, a_pos);
			}

			bool checkForMultiboundRefrs = 
			(
				(
					(Settings::bRemoveInteriorOcclusion) && 
					(
						(cell->IsInteriorCell()) || 
						(sky->mode == RE::Sky::Mode::kInterior)
					)
				) ||
				(Settings::bRemoveExteriorOcclusion && cell->IsExteriorCell())	
			);
			if (!checkForMultiboundRefrs)
			{
				return _QWithinPoint(a_this, a_pos);
			}

			if (!cell->loadedData || cell->loadedData->multiboundRefMap.empty())
			{
				return _QWithinPoint(a_this, a_pos);
			}

			for (const auto& refrMB : cell->loadedData->multiboundRefMap)
			{
				// Treat as within multibound if this multibound is within the current cell.
				// Prevents occlusion of refrs inside the multibound.
				if (refrMB.second && 
					refrMB.second->multiBound && 
					refrMB.second->multiBound.get() == a_this)
				{
					return true;
				}
			}

			return _QWithinPoint(a_this, a_pos);
		}

// [CHARACTER HOOKS]:
		void CharacterHooks::AddObjectToContainer
		(
			RE::Character* a_this, 
			RE::TESBoundObject* a_object, 
			RE::ExtraDataList* a_extraList, 
			std::int32_t a_count, 
			RE::TESObjectREFR* a_fromRefr
		)
		{
			if (!a_object || !glob.globalDataInit || !glob.allPlayersInit)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			if (playerIndex == -1)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}
			
			// Moving an object back to self in this way has led to a ton of crashes 
			// and weird bugs from my experience.
			// Change as sent/received from none.
			if (a_fromRefr == a_this)
			{
				DBG("{}: Move {} to/from none, not self.", 
					a_this->GetName(), a_object->GetName());
				a_fromRefr = nullptr;
			}
			
			// Allow addition of bound objects.
			bool isBound = 
			(
				(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
				(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
			);
			if (isBound)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}

			const auto& p = glob.coopPlayers[playerIndex];
			// Remove any items that are not equipped first.
			p->em->RemoveUndesiredItems();

			auto p1 = RE::PlayerCharacter::GetSingleton();
			const auto ui = RE::UI::GetSingleton(); 
			if (!ui || !p1)
			{
				p->em->inventoryChest->AddObjectToContainer
				(
					a_object, a_extraList, a_count, a_fromRefr
				);
				return;
			}

			/* NOTE: 
			 * Unused alternative to moving items from ContainerChangedEvent handler.
			// Move party-wide items to P1 if a companion player received the item.
			// Also move Enderal skillbooks to P1 so that P1's AddObjectToContainer() hook 
			// can give all players another skillbook of the same tier.
			bool shouldSendToP1 = 
			(
				(Util::IsPartyWideItem(a_object)) || 
				(
					ALYSLC::EnderalCompat::g_installed &&
					Settings::bEveryoneGetsALootedEnderalSkillbook &&
					GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.contains
					(
						a_object->formID
					)
				)
			);
			if (!shouldSendToP1)
			{
				auto inventory = p->coopActor->GetInventory();
				const auto iter = inventory.find(a_object); 
				if (iter != inventory.end())
				{
					const auto& invEntryData = iter->second.second;
					if (invEntryData && invEntryData->IsQuestObject())
					{
						shouldSendToP1 = true;
					}
				}
			}
						
			// Skip transfer unless it is a party wide/quest item.
			if (shouldSendToP1)
			{
				DBG
				(
					"NOT adding party-wide/quest item/Enderal skillbook {} (x{}) to {}. "
					"Giving to P1.",
					a_object->GetName(),
					a_count,
					p->coopActor->GetName()
				);
				// Not from co-op entity, so can distribute skillbooks/give additional gold
				// if these items are added to P1.
				p1->AddObjectToContainer(a_object, a_extraList, a_count, nullptr);
				// Nothing more to do here since the item should not reach this player.
				return;
			}
			*/

			if (a_fromRefr != p->em->inventoryChest.get())
			{
				DBG
				(
					"{}: Add {} of {} to inventory chest instead. From {}.",
					p->coopActor->GetName(),
					a_count, 
					a_object->GetName(),
					a_fromRefr ? a_fromRefr->GetName() : "NONE"
				);
				p->em->inventoryChest->AddObjectToContainer
				(
					a_object, a_extraList, a_count, a_fromRefr
				);
			}
			else
			{
				_AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}
		}

		float CharacterHooks::CheckClampDamageModifier
		(
			RE::Character* a_this, RE::ActorValue a_av, float a_delta
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _CheckClampDamageModifier(a_this, a_av, a_delta);
			}
			
			// REMOVE when done debugging.
			/*if (a_av == RE::ActorValue::kHealth)
			{
				DBG("{} is about to have their health modified by {}.",
					a_this->GetName(), a_delta);
			}*/

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			// Ignore attempts to kill this character if they are in the process of getting up.
			// Dying while getting up locks the character in place once they fully stand up.
			// Forever running on an invisible treadmill while stuck between 
			// this plane of existence and the afterlife.
			if (a_av == RE::ActorValue::kHealth &&
				a_this->GetActorValue(RE::ActorValue::kHealth) + a_delta <= 0.0f)
			{
				if (a_this->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued)
				{
					return 0.0f;
				}
				else 
				{
					// Do not go below 0 health.
					//a_delta = -a_this->GetActorValue(RE::ActorValue::kHealth);
				}

				// Ensure the actor does not die while paralyzed, 
				// which can cause some issues with reanimation.
				if (playerIndex == -1)
				{
					a_this->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}

			// Do not allow any AVs to change when downed 
			// and do not allow health, magicka, or stamina
			// to change while in god mode.
			bool hmsActorValue = 
			(
				a_av == RE::ActorValue::kHealth ||
				a_av == RE::ActorValue::kMagicka ||
				a_av == RE::ActorValue::kStamina
			);

			// Is a player.
			if (playerIndex != -1)
			{
				const auto& p = glob.coopPlayers[playerIndex];
				// Flash the H/M/S bar as needed.
				if (auto trueHUD = ALYSLC::TrueHUDCompat::g_trueHUDAPI3; trueHUD && hmsActorValue)
				{
					const auto handle = a_this->GetHandle();
					float currentValue = a_this->GetActorValue(a_av);
					if (currentValue > 0.0f && currentValue + a_delta <= 0.0f)
					{
						trueHUD->FlashActorValue(handle, a_av, true);
					}
				}

				// Do not modify AVs when no players are being dismissed and
				// the co-op actor is not revived or if an HMS AV is being decreased in god mode.
				bool notDismissingPlayers = 
				(
					!Settings::bUseReviveSystem || glob.livingPlayers == glob.activePlayers
				);
				if ((notDismissingPlayers) && 
					(!p->isRevived || (hmsActorValue && p->isInGodMode && a_delta < 0.0f)))
				{
					return 0.0f;
				}
				else
				{
					// NOTE:
					// For stamina, the delta amount is scaled by cost multiplier here
					// instead of in the cost functions in the player action function holder
					// because we want a consistent solution for both types of players.
					// Drawback would be not being able to link an action
					// with a specific stamina reduction, 
					// since this function does not provide any context
					// for the source of the AV change.
					// However, any source of stamina damage can be scaled here,
					// including absorption from enemy spells.

					// Apply damage received mult if the player was damaged.
					// Do not care about the source of the damage in this case,
					// as the damage received mult should apply to all sources of damage.
					if (a_av ==  RE::ActorValue::kHealth && a_delta < 0.0f)
					{
						// Max negative delta (-FLT_MAX) means that this player 
						// should have <= 0 health even if their damage received multiplier is 0, 
						// so don't apply the mult in that case.
						if (a_delta != -FLT_MAX)
						{
							a_delta *= Settings::vfDamageReceivedMult[p->playerID];
							// Also apply health cost mult if reviving another player.
							if (p->isRevivingPlayer)
							{
								// Ensure the player does not lose all their health.
								a_delta = max
								(
									-a_this->GetActorValue(RE::ActorValue::kHealth) + 
									Settings::fMinHealthWhileReviving,
									a_delta * Settings::vfReviveHealthCostMult[p->playerID]
								);
							}
						}
					}
					else if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
					{
						// Check if the player is self-healing, 
						// and if so, add skill XP to their Restoration skill.
						// Clamp first.
						float currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
						float currentMaxHealth = Util::GetFullAVAmount
						(
							a_this, RE::ActorValue::kHealth
						);
						float baseXP = std::clamp(a_delta, 0.0f, currentMaxHealth - currentHealth);
						// HACKY ALERT:
						// Prevent full-heal on combat exit for co-op companions.
						// Check if heal delta is larger than 
						// the total health regen from healing effects,
						// which would indicate that an external source of health regen
						// restored the player's health to full.
						if (currentHealth < currentMaxHealth && 
							a_delta >= currentMaxHealth - currentHealth)
						{
							float realDelta = currentMaxHealth - currentHealth;
							// REMOVE when done debugging.
							/*DBG
							(
								"Delta health: {}, real delta: {}, diff: {}. "
								"In combat: {}, in co-op combat: {}, "
								"combat controller: {}, active: {}. Discard: {}",
								a_delta, 
								realDelta,
								fabsf(a_delta - realDelta),
								p->coopActor->IsInCombat(),
								glob.isInCoopCombat,
								(bool)p->coopActor->combatController,
								p->coopActor->combatController ? 
								!p->coopActor->combatController->inactive :
								false,
								fabsf(1.0f - (a_delta - realDelta)) < 0.0001f
							);*/
							// Seems as if the delta that heals the player to full 
							// is always very close to (current max health - current health) + 1.
							// Good enough for filtering out this health change.
							if (fabsf(1.0f - (a_delta - realDelta)) < 0.0001f)
							{
								return 0.0f;
							}
						}

						if (baseXP > 0.0f && 
							p->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellRH
							))
						{
							// Targets self.
							const auto rhSpell = p->em->GetRHSpell(); 
							if (rhSpell && 
								rhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									p->playerID, RE::ActorValue::kRestoration, a_delta
								);
							}
						}

						if (baseXP > 0.0f && 
							p->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellLH
							))
						{
							// Targets self.
							const auto lhSpell = p->em->GetLHSpell(); 
							if (lhSpell && 
								lhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									p->playerID, RE::ActorValue::kRestoration, a_delta
								);
							}
						}

						if (baseXP > 0.0f && 
							p->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellQS
							))
						{
							// Targets self.
							if (p->em->quickSlotSpell && 
								p->em->quickSlotSpell->GetDelivery() == 
								RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									p->playerID, RE::ActorValue::kRestoration, a_delta
								);
							}
						}
					}
					else if (a_delta < 0.0f && a_av == RE::ActorValue::kStamina)
					{
						// If Elden Sprint is installed, 
						// do not expend stamina while outside of combat.
						if (ALYSLC::EldenSprintCompat::g_installed && 
							!glob.isInCoopCombat)
						{
							return 0.0f;
						}

						// NOTE:
						// This applies to all sources of stamina damage,
						// whether the cost of a stamina-consuming action,
						// or stamina absorption from an outside source.
						//
						// However, the same cannot be done with magicka,
						// since we are already scaling the magicka costs for casting spells
						// in the ActorMagicCaster::RequestCastImpl() hook.
						// Scaling the delta here would double the application of the multiplier.
						// We also have no way of linking each call of this function 
						// to its originating action, so there is no way to scale
						// magicka absorption and other sources of magicka damage.
						a_delta *= Settings::vfStaminaCostMult[p->playerID];
					}
				}
			}
			else
			{
				// Not a health change or a decrease in health.
				if (a_av != RE::ActorValue::kHealth || a_delta <= 0.0f)
				{
					return _CheckClampDamageModifier(a_this, a_av, a_delta);
				}

				// Check if this actor is being healed by a co-op player.
				for (const auto& p : glob.coopPlayers)
				{
					if (!p->isActive || a_this->GetHandle() != p->tm->GetRangedTargetActor())
					{
						continue;
					}

					float currentHealth = a_this->GetActorValue(RE::ActorValue::kHealth);
					float currentMaxHealth = Util::GetFullAVAmount
					(
						a_this, RE::ActorValue::kHealth
					);
					float healthDelta = std::clamp
					(
						a_delta, 0.0f, currentMaxHealth - currentHealth
					);
					if (healthDelta > 0.0f && 
						p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kRestorationSpellRH
						))
					{
						// RH Restoration spell that does not target the caster.
						const auto rhSpell = p->em->GetRHSpell(); 
						if (rhSpell && rhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
						{
							GlobalCoopData::AddSkillXP
							(
								p->playerID, RE::ActorValue::kRestoration, healthDelta
							);
						}
					}

					if (healthDelta > 0.0f && 
						p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kRestorationSpellLH
						))
					{
						// LH Restoration spell that does not target the caster.
						const auto lhSpell = p->em->GetLHSpell(); 
						if (lhSpell && lhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
						{
							GlobalCoopData::AddSkillXP
							(
								p->playerID, RE::ActorValue::kRestoration, healthDelta
							);
						}
					}

					if (healthDelta > 0.0f && 
						p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kRestorationSpellQS
						))
					{
						// QS Restoration spell that does not target the caster.
						if (p->em->quickSlotSpell && 
							p->em->quickSlotSpell->GetDelivery() != 
							RE::MagicSystem::Delivery::kSelf)
						{
							GlobalCoopData::AddSkillXP
							(
								p->playerID, RE::ActorValue::kRestoration, healthDelta
							);
						}
					}
				}
			}

			return _CheckClampDamageModifier(a_this, a_av, a_delta);
		}

		void CharacterHooks::DrawWeaponMagicHands(RE::Character* a_this, bool a_draw)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _DrawWeaponMagicHands(a_this, a_draw);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); 
			if (playerIndex == -1)
			{
				return _DrawWeaponMagicHands(a_this, a_draw);
			}
			
			// Do not allow the game to automatically 
			// sheathe/unsheathe the player actor's weapons/magic on a whim.
			const auto& p = glob.coopPlayers[playerIndex];
			// Blocking weapon/magic drawing while transforming crashes the game at times,
			// so allow it here.
			if (a_draw == p->pam->weapMagReadied || p->isTransforming)
			{
				return _DrawWeaponMagicHands(a_this, a_draw);;
			}
		}
		
		void CharacterHooks::HandleHealthDamage
		(
			RE::Character* a_this, RE::Actor* a_attacker, float a_damage
		)
		{
			// NOTE: 
			// The given damage is negative.
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _HandleHealthDamage(a_this, a_attacker, a_damage);;
			}

			// REMOVE when done debugging.
			/*DBG("{} is about to take {} damage from {}.",
				a_this->GetName(), a_damage, a_attacker ? a_attacker->GetName() : "NONE");*/

			// Check for damage dealt by a player.
			auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(a_attacker);
			// Check for damage dealt to a player.
			auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			bool playerVictim = playerVictimIndex != -1;
			bool playerAttacker = playerAttackerIndex != -1;
			// Multiplier to apply to the damage argument.
			float damageMult = 1.0f;
			if (playerAttacker)
			{
				// The attacking player.
				const auto& p = glob.coopPlayers[playerAttackerIndex];
				// Check for friendly fire (not from self) and negate damage.
				if (!Settings::vbFriendlyFire[p->playerID] && 
					Util::IsPartyFriendlyActor(a_this) && 
					a_this != p->coopActor.get())
				{
					a_this->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -a_damage
					);
					return;
				}

				// Apply damage dealt mult for the attacking player.
				damageMult *= Settings::vfDamageDealtMult[p->playerID];
				// TEMP until I find a direct way of applying the sneak damage multiplier 
				// to all forms of damage.
				// Apply sneak/additional damage mult if not attacking self.
				if (a_this != p->coopActor.get() && 
					p->pam->attackDamageMultSet &&
					p->pam->reqDamageMult != 1.0f)
				{
					// Apply sneak attack mult.
					damageMult *= p->pam->reqDamageMult;
					// Reset damage multiplier if performing a ranged sneak attack.
					// Melee sneak attacks reset the damage multiplier on attack stop,
					// but I have yet to find a way to check 
					// if the player no longer has any active projectiles, 
					// so reset the damage multiplier on a damaging hit.
					p->pam->ResetAttackDamageMult();
				}
			}

			// Adjust damage based off new damage mult.
			// Done before death (< 0 HP) checks below.
			// Ignore direct modifications of HP, which occur with direct changes to HP, 
			// such as RestoreActorValue() below.
			// Don't want to get caught in a recursive loop.
			// NOTE: 
			// As a result, certain types of damage without an attributable attacker, 
			// such as explosion damage,
			// will not be affected by the player's damage dealt multiplier.
			// TODO:
			// Find a way to do health damage without this function triggering,
			// since we currently have to adjust the damage dealt
			// via direct modification of the health actor value.
			// Or will have to figure out how to determine 
			// if the damage source has been scaled already.
			float deltaHealth = a_damage * (damageMult - 1.0f); 
			if (deltaHealth != 0.0f && a_attacker)
			{
				// Apply the inverse of the damage received mult for friendly fire, 
				// since the RestoreActorValue() call below will run through
				// our CheckClampDamageModifier() hook
				// and will apply the damage received mult again to any negative health delta.
				// We can cancel out the second application in this way.
				if (playerVictim)
				{
					const auto& victimP = glob.coopPlayers[playerVictimIndex];
					if (Settings::vfDamageReceivedMult[victimP->playerID] > 0.0f)
					{
						// If additional damage is required,
						// damage to apply for this second call is not modified.
						// Otherwise, this hook will only fire once 
						// and we can set the damage applied to the original damage 
						// (received damage mult already applied) times the attacker damage mult.
						if (deltaHealth < 0.0f)
						{
							// Not modifying the damage arg itself, 
							// since after multiplying it with the computed damage mult, 
							// we'll have one application each of the damage dealt 
							// and received mults, as required.
							deltaHealth *= 
							(
								1.0f / Settings::vfDamageReceivedMult[victimP->playerID]
							);
						}
						else
						{
							a_damage *= damageMult;
						}
					}
					else
					{
						a_damage = 0.0f;
					}
				}
				else
				{
					a_damage *= damageMult;
				}

				// This hook will run again with no attacker given 
				// and then execution will return here.
				a_this->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth
				);
			}

			// Check if the player must be set as downed when at or below 0 health.
			if (playerVictim)
			{
				const auto& p = glob.coopPlayers[playerVictimIndex];
				// Ignore duplicate calls if the player is already downed 
				// or if there are no living players.
				if (a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f && 
					glob.livingPlayers > 0 &&
					!p->isDowned)
				{
					if (Settings::bUseReviveSystem)
					{
						// Set this player as downed.
						p->SetAsDowned();
						bool playerStillStanding = std::any_of
						(
							glob.coopPlayers.begin(), glob.coopPlayers.end(),
							[](const auto& a_p) 
							{
								return a_p->isActive && !a_p->isDowned;
							}
						);
						if (!playerStillStanding)
						{
							// All players downed, end co-op session.
							glob.taskRunner->AddTask([](){ GlobalCoopData::YouDiedTask(); });
						} 
						else if (a_this->GetActorValue(RE::ActorValue::kHealth) < 0.0f)
						{
							// Stop! Stop! They're already dead!
							return;
						}
					}
					else
					{
						// If not using the revive system, once one player dies,
						// all other players die and the co-op session ends.
						auto handle = p->coopActor->GetHandle();
						glob.taskRunner->AddTask
						(
							[handle](){ GlobalCoopData::YouDiedTask(handle); }
						);
					}
				}
			}

			// Player inflicted health damage on this character.
			// Add XP, if the attacker is not P1, and set killer if this character is now dead.
			if (playerAttacker)
			{
				// NOTE: 
				// Handled friendly fire check above already, 
				// so guaranteed to either have friendly fire enabled 
				// or attacking a target that is not party-friendly.
				const auto& p = glob.coopPlayers[playerAttackerIndex];
				// Do not give attacking player XP if attacking another player that is in god mode.
				bool victimPlayerInGodMode = 
				(
					playerVictimIndex != -1 && glob.coopPlayers[playerVictimIndex]->isInGodMode
				);
				if (!p->isPlayer1 && !victimPlayerInGodMode)
				{
					// Check spell attack source and increment skill XP if needed.
					const auto lhForm = p->em->equippedForms[!EquipIndex::kLeftHand];
					const auto rhForm = p->em->equippedForms[!EquipIndex::kRightHand];
					const auto qsSpellForm = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
					auto addDestructionXP =
					[&p, &a_damage, a_this](RE::TESForm* a_potentialSourceForm) 
					{
						if (!a_potentialSourceForm)
						{
							return;
						}

						// Is not a destruction spell, so exit.
						const auto spell = a_potentialSourceForm->As<RE::SpellItem>(); 
						if (!spell ||
							!spell->avEffectSetting || 
							spell->avEffectSetting->data.associatedSkill != 
							RE::ActorValue::kDestruction)
						{
							return;
						}

						GlobalCoopData::AddSkillXP
						(
							p->playerID, RE::ActorValue::kDestruction, -a_damage
						);
					};

					// Check for destruction spell cast from LH/RH/Quick Slot.
					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellLH
						))
					{
						addDestructionXP(lhForm);
					}

					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellRH
						))
					{
						addDestructionXP(rhForm);
					}

					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellQS
						))
					{
						addDestructionXP(qsSpellForm);
					}
				}

				// Killed by co-op player.
				auto p1 = RE::PlayerCharacter::GetSingleton(); 
				if (p1 &&
					!a_this->IsEssential() &&
					a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)
				{
					// NOTE: 
					// Enderal treats dead actors without an associated killer
					// as killed by P1, so clear out the handle here 
					// to get XP from killing this actor.
					// Setting directly to P1 does not properly grant XP for some reason.
					a_this->boolBits.set(RE::Actor::BOOL_BITS::kMurderAlarm);
					if (ALYSLC::EnderalCompat::g_installed) 
					{
						a_this->KillImpl(p->coopActor.get(), FLT_MAX, false, false);
						a_this->myKiller = p->coopActor.get();
					}
					else
					{
						a_this->KillImpl(p1, FLT_MAX, false, false);
						a_this->myKiller = p1;
					}

					// Have to store info to give the killer player first rights 
					// to loot the corpse with the QuickLoot menu.
					if (ALYSLC::QuickLootCompat::g_installed)
					{
						// Use extra data to store the real killer, 
						// since P1 takes the blame for any co-op companion kills.
						// Use owner exData to keep track of killer.
						a_this->extraList.SetOwner(p->coopActor.get());
					}
				}
			}

			_HandleHealthDamage(a_this, a_attacker, a_damage);
		}

		void CharacterHooks::ModifyAnimationUpdateData
		(
			RE::Character* a_this, RE::BSAnimationUpdateData& a_data
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _ModifyAnimationUpdateData(a_this, a_data);
			}
			
			// Only speed up the getup animation when any of the three extra mechanics 
			// are enabled while in co-op.
			bool speedupGetup = 
			(
				(
					Settings::bEnableArmsRotation ||
					Settings::bEnableFlopping ||
					Settings::bEnableObjectManipulation
				) &&
				(
					a_this->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued ||
					a_this->GetKnockState() == RE::KNOCK_STATE_ENUM::kGetUp
				)
			);
			if (speedupGetup)
			{
				a_data.deltaTime *= 3.0f;
			}

			const auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); 
			if (playerIndex == -1)
			{
				return _ModifyAnimationUpdateData(a_this, a_data);
			}

			const auto& p = glob.coopPlayers[playerIndex];
			// Dodging/equip animations only available if the player's race is humanoid.
			if (p->coopActor->HasKeyword(glob.npcKeyword))
			{
				// Speed up (un)equip/dodging anims.
				// TODO: 
				// Support for more dodge mods.
				bool isEquipping = false;
				bool isUnequipping = false;
				bool isTDMDodging = false;
				bool isTKDodging = false;
				a_this->GetGraphVariableBool("IsEquipping", isEquipping);
				a_this->GetGraphVariableBool("IsUnequipping", isUnequipping);
				a_this->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
				a_this->GetGraphVariableBool("bIsDodging", isTKDodging);

				if ((Settings::bSpeedUpEquipAnimations) && (isEquipping || isUnequipping))
				{
					a_data.deltaTime *= Settings::fEquipAnimSpeedFactor;
				}
				else if ((Settings::bSpeedUpDodgeAnimations) && (isTDMDodging || isTKDodging))
				{
					a_data.deltaTime *= Settings::fDodgeAnimSpeedFactor;
				}

				// Increase sprint animation playback speed relative to the default
				// base speed of 85 and base sprint movement mult of 1.5.
				// Feels less floaty at higher sprint speed multipliers, 
				// since more steps are taken per second with the increased animation speed.
				if (p->pam->isSprinting) 
				{
					a_data.deltaTime *= max
					(
						0.1f,
						(Settings::fBaseSpeed / 85.0f) * (Settings::fSprintingMovMult / 1.5f)
					);
				}
			}
			else 
			{
				// Get up faster when not a humanoid.
				if (p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued ||
					p->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kGetUp)
				{
					a_data.deltaTime *= 5.0f;
				}
			}

			if (p->mm->isDashDodging)
			{
				// Slow down dash dodge animation if the player's equip weight is high.
				const float weightAdjAnimSpeedFactor = Util::InterpolateEaseIn
				(
					1.0f, 
					0.5f, 
					std::clamp
					(
						p->mm->dashDodgeEquippedWeight / 75.0f, 
						0.0f, 
						1.0f
					), 
					2.0f
				) * Settings::fDashDodgeAnimSpeedFactor * p->mm->dashDodgeLSDisplacement;
				a_data.deltaTime *= weightAdjAnimSpeedFactor;
			}
			else if (a_this->IsSwimming() && p->pam->IsPerforming(InputAction::kSprint))
			{
				// Speed up swimming animation to match the increased speedmult
				// while 'sprinting' in the water.
				a_data.deltaTime *= max(0.1f, Settings::fSprintingMovMult);
			}

			_ModifyAnimationUpdateData(a_this, a_data);
		}

		bool CharacterHooks::NotifyAnimationGraph
		(
			RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _NotifyAnimationGraph(a_this, a_eventName);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex
			(
				skyrim_cast<RE::Character*>(a_this)
			); 
			if (playerIndex == -1)
			{
				return _NotifyAnimationGraph(a_this, a_eventName);
			}

			const auto& p = glob.coopPlayers[playerIndex];
			auto hash = Hash(a_eventName);
			if (p->isTransformed) 
			{
				// Do not allow the game to toggle the levitation state again
				// after the player's toggle request is being fulfilled.
				// Each time an equip/unequip event fires, 
				// the game tries to toggle levitation 2 times.
				if ((p->isTogglingLevitationState) && 
					(hash == "LevitationToggle"_h || hash == "LevitationToggleMoving"_h)) 
				{
					return false;
				}
			}
			
			// Stop any scene package idle from playing.
			if (p->coopActor->currentProcess && 
				p->coopActor->currentProcess->middleHigh && 
				p->coopActor->currentProcess->middleHigh->unk210)
			{
				if (p->coopActor->IsWeaponDrawn())
				{
					_NotifyAnimationGraph(a_this, "IdleStopInstant");
				}
				else
				{
					_NotifyAnimationGraph(a_this, "IdleForceDefaultState");
				}

				return true;
			}

			// Prevent the game from forcing the co-op companion player 
			// into/out of sneaking against their wishes.
			// Dash dodges trigger the sneak animation briefly, 
			// so ignore such animation event requests.
			bool sneakStateChangeAttempt = 
			(
				(!p->mm->isRequestingDashDodge && !p->mm->isDashDodging) && 
				(hash == "SneakStart"_h || hash == "SneakStop"_h)
			);
			if (sneakStateChangeAttempt)
			{
				// If trying to exit the sneak state while the player wants to remain sneaking,
				// or if trying to enter the sneak state while the player wants to stop sneaking,
				// return false.
				if ((p->pam->wantsToSneak && hash == "SneakStop"_h) ||
					(!p->pam->wantsToSneak && hash == "SneakStart"_h))
				{
					return false;
				}
			}
			else if ((hash == "staggerStart"_h) &&
					 (
						 p->isRevivingPlayer || 
						 p->coopActor->IsOnMount() || 
						 Util::HandleIsValid(p->coopActor->GetOccupiedFurniture())
					 ))
			{
				// Prevent stagger when reviving, mounted, or using furniture,
				// which will make the companion player exit the animation or dismount prematurely 
				// and potentially glitch their equip state.
				return _NotifyAnimationGraph(a_this, "staggerStop");
			}
			else if (Settings::bUseReviveSystem && hash == "BleedoutStart"_h)
			{
				// Skip bleedout animations when using the co-op revive system.
				// Players will ragdoll and become unresponsive when reaching 0 health instead.
				return _NotifyAnimationGraph(a_this, "bleedOutStop");
			}
			else if (((p->isDowned && !p->isRevived) ||
					 (p->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && 
					  hash == "GetUpBegin"_h &&
					  p->selfValid)
			{
				// Ignore requests to get up when the player is downed and not revived.
				return false;
			}
			else if ((p->coopActor->IsInKillMove()) && 
					 (hash == "PairEnd"_h || hash == "pairedStop"_h))
			{
				// Sometimes, when a killmove fails, the player will remain locked in place
				// because the game still considers them to be in a killmove,
				// so unset the killmove flag here to signal the player's PAM 
				// to stop handling the previously triggered killmove and reset the player's data.
				p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
			}
			
			return _NotifyAnimationGraph(a_this, a_eventName);
		}

		void CharacterHooks::PickUpObject
		(
			RE::Character* a_this, 
			RE::TESObjectREFR* a_object, 
			std::int32_t a_count, 
			bool a_arg3, 
			bool a_playSound
		)
		{
			if (!a_object || !glob.globalDataInit || !glob.allPlayersInit)
			{
				return _PickUpObject(a_this, a_object, a_count, a_arg3, a_playSound);
			}
			
			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return _PickUpObject(a_this, a_object, a_count, a_arg3, a_playSound);
			}

			DBG
			(
				"{}: {} of {}. List: {:p}, Arg3: {}",
				a_this->GetName(),
				a_count, 
				a_object ? a_object->GetName() : "NONE",
				fmt::ptr(a_object ? std::addressof(a_object->extraList) : nullptr),
				a_arg3
			);

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			if (playerIndex == -1)
			{
				return _PickUpObject(a_this, a_object, a_count, a_arg3, a_playSound);
			}
			
			const auto& p = glob.coopPlayers[playerIndex];
			DBG
			(
				"{}: Add {} of {} to inventory chest instead via P1.",
				p->coopActor->GetName(),
				a_count, 
				a_object->GetName()
			);

			auto owner = a_object->extraList.GetByType<RE::ExtraOwnership>(); 
			RE::TESForm* oldOwner = a_object->extraList.GetOwner();
			a_object->extraList.SetOwner(p->coopActor.get());
			DBG
			(
				"Adding ownerhip exData to list {:p}: {}. ExCount: {}.",
				fmt::ptr(std::addressof(a_object->extraList)),
				a_object->extraList.GetOwner() == p->coopActor.get() ?
				"SUCC" :
				"FAIL",
				a_object->extraList.GetCount()
			);

			p1->PickUpObject(a_object, a_count, false, true);

			const auto inventory = p1->GetInventory();
			const auto iter = inventory.find(a_object->GetBaseObject());
			// Not in P1's inventory after pickup.
			if (iter == inventory.end() || !iter->second.second)
			{
				DBG
				(
					"ERR: {}: Failed to find {} of {} in P1's inventory after pickup.",
					p->coopActor->GetName(),
					a_count, 
					a_object->GetName()
				);
				return;
			}

			// Can add a copy to the inventory chest if not a quest, party-wide, 
			// or added Enderal skillbook item.
			// Otherwise, the item will remain in P1's inventory after pickup above.
			bool shouldAddToChest = 
			(
				!iter->second.second->IsQuestObject() &&
				!Util::IsPartyWideItem(a_object)
				/*(!iter->second.second->IsQuestObject()) &&
				(!Util::IsPartyWideItem(a_object)) &&
				(
					!ALYSLC::EnderalCompat::g_installed ||
					!Settings::bEveryoneGetsALootedEnderalSkillbook ||
					!GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.contains
					(
						a_object->GetBaseObject()->formID
					)
				)*/
			);
			if (!shouldAddToChest)
			{
				DBG
				(
					"Item {} is a quest/party-wide/added Enderal skillbook object. "
					"Keeping in P1's inventory.", 
					a_object->GetName(),
					a_this->GetName()
				);
				return;
			}
			
			// No extra lists, so the item was not added on pickup.
			if (!iter->second.second->extraLists || iter->second.second->extraLists->empty())
			{
				DBG
				(
					"ERR: {}: No extra data lists for {} in P1's inventory after pickup.",
					p->coopActor->GetName(),
					a_object->GetName()
				);
				return;
			}

			uint32_t i = 0;
			for (auto extraDataList : *iter->second.second->extraLists)
			{
				DBG
				(
					"Item {} (list #{}) is extra list {:p}.",
					a_object->GetName(),
					i,
					fmt::ptr(extraDataList)
				);
				++i;
				if (!extraDataList)
				{
					continue;
				}

				for (auto type = RE::ExtraDataType::kNone; 
					type <= RE::ExtraDataType::kUnkBF; 
					type = static_cast<RE::ExtraDataType>(!type + 1))
				{
					if (auto data = a_object->extraList.GetByType(type); data)
					{
						DBG
						(
							"Item {} in {}'s inventory has exData list {:p} "
							"with data {:p} of type 0x{:X}.",
							a_object->GetName(),
							p1->GetName(),
							fmt::ptr(extraDataList),
							fmt::ptr(data),
							type
						);
					}
				}

				auto owner = extraDataList->GetByType<RE::ExtraOwnership>();
				// Found our added owner exData from earlier, so this is the picked-up object.
				if (owner && owner->owner == p->coopActor.get())
				{
					// Restore old owner before transferring, if not a quest item.
					extraDataList->SetOwner(oldOwner);
					// Crashes when the newly transferred item 
					// is dropped from the companion player's inventory chest
					// if directly transfered over with the current extra data list.
					p->em->inventoryChest->AddObjectToContainer
					(
						a_object->GetBaseObject(),
						Util::CopyExtraDataList(extraDataList),
						a_count,
						nullptr
					);
					p1->RemoveItem
					(
						a_object->GetBaseObject(),
						a_count, 
						RE::ITEM_REMOVE_REASON::kRemove, 
						extraDataList,
						nullptr
					);
					/*p1->RemoveItem
					(
						a_object->GetBaseObject(),
						a_count, 
						RE::ITEM_REMOVE_REASON::kRemove, 
						extraDataList,
						p->em->inventoryChest.get()
					);*/
				}

				// Continue looping just in case there are other picked up items from before
				// that should be transferred to the player's chest.
			}
		}

		void CharacterHooks::PutCreatedPackage
		(
			RE::Character* a_this,
			RE::TESPackage* a_package,
			bool a_tempPackage, 
			bool a_createdPackage
		)
		{
			// Prevent the game force-running packages that can play idles 
			// or equip gear on the companion player's character.

			if (!glob.globalDataInit || !glob.allPlayersInit)
			{
				return _PutCreatedPackage(a_this, a_package, a_tempPackage, a_createdPackage);
			}

			if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); pIndex != -1)
			{
				const auto& p = glob.coopPlayers[pIndex];
				if (!a_package)
				{
					DBG
					(
						"{}: RUNNING NONE (temp: {}, created: {}).",
						a_this->GetName(), 
						a_tempPackage,
						a_createdPackage
					);
					return _PutCreatedPackage
					(
						a_this, a_package, a_tempPackage, a_createdPackage
					);
				}
				// Only allow getting up from furniture 
				// while the interaction package is NOT running,
				// and allow attempts to dismount only if the player 
				// is no longer requesting to mount.
				// Also allow a choice selection of procedures 
				// if the current package is the interaction package.
				// Choice. Needs testing. Boo.
				bool allow = 
				(
					(
						!p->mm->interactionPackageRunning &&
						*a_package->procedureType == 
						RE::PACKAGE_PROCEDURE_TYPE::kGetUpFromChairBed
					) ||
					(
						!p->mm->wantsToMount &&
						*a_package->procedureType == 
						RE::PACKAGE_PROCEDURE_TYPE::kDismountActor
					) ||
					(
						p->mm->wantsToMount &&
						*a_package->procedureType == 
						RE::PACKAGE_PROCEDURE_TYPE::kMountActor
					) ||
					(
						*a_package->procedureType == 
						RE::PACKAGE_PROCEDURE_TYPE::kDoNothing ||
						*a_package->procedureType == 
						RE::PACKAGE_PROCEDURE_TYPE::kPackage
					)
					/*||
					(
						(p->mm->interactionPackageRunning) &&
						(
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kActivate ||
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kCannibal ||
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kEat ||
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kSleep ||
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kUseItemAt ||
							*a_package->procedureType ==
							RE::PACKAGE_PROCEDURE_TYPE::kVampireFeed
						)
					)*/
				);
				if (!allow)
				{
					auto currentCoopPackage = p->pam->GetCurrentPackage();
					DBG
					(
						"{}: IGNORE {} (0x{:X}, temp: {}, created: {}, procedure type: {}. "
						"Run {} (0x{:X}, type {}) instead.",
						a_this->GetName(), 
						a_package->GetName(),
						a_package->formID,
						a_tempPackage,
						a_createdPackage,
						*a_package->procedureType,
						currentCoopPackage ? Util::GetEditorID(currentCoopPackage) : "NONE",
						currentCoopPackage ? currentCoopPackage->formID : 0xDEAD,
						currentCoopPackage ? 
						*currentCoopPackage->procedureType :
						RE::PACKAGE_PROCEDURE_TYPE::kNone
					);
					return;
				}

				DBG
				(
					"{}: RUNNING {} (0x{:X}, temp: {}, created: {}, procedure type: {}.",
					a_this->GetName(), 
					a_package->GetName(),
					a_package->formID,
					a_tempPackage,
					a_createdPackage,
					*a_package->procedureType
				);
			}

			_PutCreatedPackage(a_this, a_package, a_tempPackage, a_createdPackage);
		}

		RE::ObjectRefHandle* CharacterHooks::RemoveItem
		(
			RE::Character* a_this,
			RE::ObjectRefHandle* a_handleOut, 
			RE::TESBoundObject* a_item,
			std::int32_t a_count, 
			RE::ITEM_REMOVE_REASON a_reason, 
			RE::ExtraDataList* a_extraList,
			RE::TESObjectREFR* a_moveToRef, 
			const RE::NiPoint3* a_dropLoc,
			const RE::NiPoint3* a_rotate
		)
		{
			if (!a_item || !glob.globalDataInit || !glob.allPlayersInit)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason, 
					a_extraList, 
					a_moveToRef,
					a_dropLoc, 
					a_rotate
				);
			}

			if (const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); pIndex > 0)
			{
				// Moving an object back to self in this way has led to a ton of crashes 
				// and weird bugs from my experience.
				// Change as sent/received from none.
				if (a_moveToRef == a_this)
				{
					DBG("{}: Move {} to/from none, not self.", 
						a_this->GetName(), a_item->GetName());
					a_moveToRef = nullptr;
				}

				const auto& p = glob.coopPlayers[pIndex];
				DBG
				(
					"{} is removing {} of {} from their inventory. "
					"Reason: {}. To refr: {}. Drop loc: ({}, {}, {}).",
					p->coopActor->GetName(),
					a_count,
					a_item->GetName(),
					a_reason,
					a_moveToRef ? a_moveToRef->GetName() : "NONE",
					a_dropLoc ? a_dropLoc->x : 0.0f,
					a_dropLoc ? a_dropLoc->y : 0.0f,
					a_dropLoc ? a_dropLoc->z : 0.0f
				);

				// Do not remove to P1 or the inventory chest 
				// when this player's inventory is copied over to P1.
				bool preventRemoval = 
				(
					(
						glob.menuPID == pIndex &&
						glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory)
					) &&
					(
						(a_moveToRef) && 
						(a_moveToRef->IsPlayerRef() || a_moveToRef == p->em->inventoryChest.get())
					)
				);
				if (preventRemoval)
				{
					DBG("ERR: Should not be moving item from {} to {}.",
						p->coopActor->GetName(), a_moveToRef ? a_moveToRef->GetName() : "NONE");
					return nullptr;
				}

				// Remove from the player's inventory as usual if moving to the inventory chest,
				// removing completely, or moving to another player.
				if ((a_moveToRef == p->em->inventoryChest.get()) ||
					(a_reason == RE::ITEM_REMOVE_REASON::kRemove && !a_moveToRef) ||
					(
						(
							a_reason == RE::ITEM_REMOVE_REASON::kStoreInTeammate || 
							a_reason == RE::ITEM_REMOVE_REASON::kStoreInContainer
						) &&
						(GlobalCoopData::IsCoopPlayer(a_moveToRef))
					))
				{
					return _RemoveItem
					(
						a_this, 
						a_handleOut,
						a_item, 
						a_count,
						a_reason, 
						a_extraList, 
						a_moveToRef,
						a_dropLoc, 
						a_rotate
					);
				}
				else if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
				{
					// Otherwise, drop or remove the corresponding item from the inventory chest.
					auto chestExDataList = Util::FindMatchingExtraDataList
					(
						p->em->inventoryChest.get(),
						a_item,
						a_extraList
					);
					// Drop by moving the matching item to P1 from the inventory chest 
					// since dropping directly from the chest fails.
					if (a_reason == RE::ITEM_REMOVE_REASON::kDropping)
					{
						DBG
						(
							"Moving {} (x{}, list {:p}) from {}'s inventory chest to P1 "
							"and dropping at ({}, {}, {})",
							a_item->GetName(),
							a_count,
							fmt::ptr(chestExDataList),
							p->coopActor->GetName(),
							a_dropLoc ? a_dropLoc->x : 0.0f,
							a_dropLoc ? a_dropLoc->y : 0.0f,
							a_dropLoc ? a_dropLoc->z : 0.0f
						);
						p->em->inventoryChest->RemoveItem
						(
							a_item, 
							a_count, 
							RE::ITEM_REMOVE_REASON::kRemove,
							chestExDataList,
							p1
						);
						p1->DropObject
						(
							a_item,
							chestExDataList,
							a_count,
							a_dropLoc
						);
					}
					else
					{
						DBG
						(
							"Removing {} (x{}, list {:p}) from {}'s inventory chest.",
							a_item->GetName(),
							a_count,
							fmt::ptr(chestExDataList),
							p->coopActor->GetName()
						);
						// Remove matching item directly from chest.
						p->em->inventoryChest->RemoveItem
						(
							a_item, 
							a_count, 
							a_reason,
							chestExDataList,
							a_moveToRef
						);
					}

					// Remove the item from the player's inventory too.
					DBG
					(
						"Removing {} (x{}, list {:p}) from {}'s inventory.",
						a_item->GetName(),
						a_count,
						fmt::ptr(a_extraList),
						p->coopActor->GetName()
					);
					_RemoveItem
					(
						a_this, 
						a_handleOut,
						a_item, 
						a_count,
						RE::ITEM_REMOVE_REASON::kRemove, 
						a_extraList, 
						nullptr,
						a_dropLoc, 
						a_rotate
					);
					
					a_handleOut = nullptr;
					return nullptr;
				}
			}

			return _RemoveItem
			(
				a_this, 
				a_handleOut,
				a_item, 
				a_count,
				a_reason, 
				a_extraList, 
				a_moveToRef,
				a_dropLoc, 
				a_rotate
			);
		}

		void CharacterHooks::RemoveWeapon(RE::Character* a_this, RE::BIPED_OBJECT a_equipIndex)
		{
			// Sometimes, the game removes bound weapons before or without an equip event 
			// or our unequip hook firing.
			// Catch such instances before they occur and prevent removal of the bound weapon/ammo
			// if their duration has not expired.

			if (!glob.globalDataInit ||
				!glob.allPlayersInit ||
				!glob.coopSessionActive ||
				glob.loadingASave)
			{
				return _RemoveWeapon(a_this, a_equipIndex);	
			}

			// Not a player.
			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); 
			if (pIndex == -1)
			{
				return _RemoveWeapon(a_this, a_equipIndex);	
			}

			const auto& p = glob.coopPlayers[pIndex];
			const auto biped = p->coopActor->GetBiped();
			if (!biped)
			{
				return _RemoveWeapon(a_this, a_equipIndex);
			}

			const auto& bipedObj = biped->objects[a_equipIndex];
			DBG
			(
				"{}: {} at equip index {}.",
				p->coopActor->GetName(), 
				bipedObj.item ? bipedObj.item->GetName() : "NONE", 
				!a_equipIndex
			);
			bool shouldPreventRemoval = false;
			if (bipedObj.item)
			{
				auto weap = bipedObj.item->As<RE::TESObjectWEAP>();
				bool is1HEquipType = weap->equipSlot != glob.bothHandsEquipSlot;
				bool isBoundWeap = weap && weap->IsBound();
				bool isBoundAmmo = 
				(
					bipedObj.item->As<RE::TESAmmo>() && 
					bipedObj.item->As<RE::TESAmmo>()->HasKeywordByEditorID
					(
						"WeapTypeBoundArrow"
					)
				);
				if (isBoundWeap)
				{
					uint8_t active1HBoundWeapReqs = 0;
					uint8_t active2HBoundWeapReqs = 0;
					if (p->em->lastReqBoundWeapRH && 
						p->em->lastReqBoundWeapRH->As<RE::TESObjectWEAP>())
					{
						auto rhBoundWeap = 
						(
							p->em->lastReqBoundWeapRH->As<RE::TESObjectWEAP>()
						);
						if (rhBoundWeap->equipSlot == glob.bothHandsEquipSlot &&
							p->pam->boundWeapReq2H &&
							p->pam->secsBoundWeapon2HDuration - 
							p->pam->secsSinceBoundWeap2HReq > 0.0f)
						{
							active2HBoundWeapReqs++;
						}
						else if (rhBoundWeap->equipSlot != glob.bothHandsEquipSlot &&
									p->pam->boundWeapReqRH &&
									p->pam->secsBoundWeaponRHDuration - 
									p->pam->secsSinceBoundWeapRHReq > 0.0f)
						{
							active1HBoundWeapReqs++;
						}
					}
							
					if (p->em->lastReqBoundWeapLH && 
						p->em->lastReqBoundWeapLH->As<RE::TESObjectWEAP>())
					{
						auto lhBoundWeap = 
						(
							p->em->lastReqBoundWeapLH->As<RE::TESObjectWEAP>()
						);
						if (lhBoundWeap->equipSlot != glob.bothHandsEquipSlot &&
							p->pam->boundWeapReqLH &&
							p->pam->secsBoundWeaponLHDuration - 
							p->pam->secsSinceBoundWeapLHReq > 0.0f)
						{
							active1HBoundWeapReqs++;
						}
					}

					uint8_t equipped1HBoundWeaps = 0;
					auto lhObj = p->coopActor->GetEquippedObject(true);
					auto lhWeap = lhObj ? lhObj->As<RE::TESObjectWEAP>() : nullptr; 
					auto rhObj = p->coopActor->GetEquippedObject(false);
					auto rhWeap = rhObj ? rhObj->As<RE::TESObjectWEAP>() : nullptr; 
					if (lhWeap && 
						lhWeap->IsBound() && 
						lhWeap->equipSlot != glob.bothHandsEquipSlot)
					{
						equipped1HBoundWeaps++;
					}

					if (rhWeap && 
						rhWeap->IsBound() &&
						rhWeap->equipSlot != glob.bothHandsEquipSlot)
					{
						equipped1HBoundWeaps++;
					}

					bool reqRemovalOf2HWeap = weap->equipSlot == glob.bothHandsEquipSlot;
					shouldPreventRemoval = 
					(
						(
							reqRemovalOf2HWeap &&
							active2HBoundWeapReqs != 0
						) ||
						(
							!reqRemovalOf2HWeap &&
							active1HBoundWeapReqs > 0 &&
							equipped1HBoundWeaps == active1HBoundWeapReqs
						)
					);

					DBG
					(
						"{}: Active 1H/2H requests: {}, {}, equipped 1H bound weaps: {}, "
						"Removal request is for {} ({}, index: {}). {}.",
						p->coopActor->GetName(), 
						active1HBoundWeapReqs,
						active2HBoundWeapReqs,
						equipped1HBoundWeaps,
						weap->GetName(),
						weap->equipSlot == glob.bothHandsEquipSlot ? "2H" : "1H",
						a_equipIndex,
						shouldPreventRemoval ? "IGNORE" : "ALLOW"
					);
				}
				else if (isBoundAmmo)
				{
					if (p->pam->boundWeapReq2H &&
						p->em->lastReqBoundWeapRH &&
						p->em->lastReqBoundWeapRH->As<RE::TESObjectWEAP>() &&
						p->em->lastReqBoundWeapRH->As<RE::TESObjectWEAP>()->IsRanged() &&
						p->pam->secsBoundWeapon2HDuration - 
						p->pam->secsSinceBoundWeap2HReq > 0.0f)
					{
						DBG
						(
							"{}: Ignore removal of bound ammo {}.",
							p->coopActor->GetName(), 
							bipedObj.item->GetName()
						);
						shouldPreventRemoval = true;
					}
				}
			}	

			DBG
			(
				"{}: {} at equip index {}. {}.",
				p->coopActor->GetName(), 
				bipedObj.item ? bipedObj.item->GetName() : "NONE",
				!a_equipIndex,
				shouldPreventRemoval ? "IGNORE" : "ALLOW"
			);
			if (shouldPreventRemoval)
			{
				return;
			}

			return _RemoveWeapon(a_this, a_equipIndex);	
		}

		void CharacterHooks::ResetInventory(RE::Character* a_this, bool a_leveledOnly)
		{
			// Prevent players from resetting their inventory when disabled/resurrected,
			// since a full inventory reset removes items added during the co-op session.

			// Allow inventory resets if no players are initialized
			// or if this character is not a player.
			if (!glob.globalDataInit || !glob.allPlayersInit)
			{
				return _ResetInventory(a_this, a_leveledOnly);	
			}

			// Reset if not a player or if there is no request to reset.
			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			if (pIndex == -1 || glob.coopPlayers[pIndex]->em->skipEquipProcessing)
			{
				if (pIndex != -1)
				{
					DBG("ALLOW: {}, leveled only: {}", a_this->GetName(), a_leveledOnly);
				}

				return _ResetInventory(a_this, a_leveledOnly);
			}

			DBG("SKIP: {}, leveled only: {}", a_this->GetName(), a_leveledOnly);
		}

		void CharacterHooks::SetCurrentScene(RE::Character* a_this, RE::BGSScene* a_scene)
		{
			// Prevent the game from roping companion players into scenes,
			// which can force-play idles on the companion player's character
			// and lock them in place, despite player attempts to move the character.

			if (glob.globalDataInit && glob.allPlayersInit && glob.coopSessionActive && a_scene)
			{
				if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); pIndex != -1)
				{
					return _SetCurrentScene(a_this, nullptr);;
				}
			}

			_SetCurrentScene(a_this, a_scene);
		}

		void CharacterHooks::Update(RE::Character* a_this, float a_delta)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _Update(a_this, a_delta);
			}

			if (GlobalCoopData::IsCoopPlayer(a_this))
			{
				const auto& p = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(a_this)];

				// IMPORTANT NOTE:
				// If instead using a package flag to ignore combat,
				// the player will still enter combat and any external checks querying the combat
				// state for the player will return the right value, but the player cannot damage 
				// neutral NPCs with weapons, may have their chosen gear (un)equipped 
				// based on what the game thinks is preferrable,
				// or even have their character rotate automatically
				// towards a targeted actor when spellcasting.
				// 
				// Doing the following will stop players' characters
				// from aggro-ing NPCs or other players
				// and is proven to prevent the game from auto-equipping gear
				// or rotating the player to face their combat target as it sees fit.
				// However, by disabling combat altogether for companion players,
				// mods that check if a character is in combat will not function properly;
				// for example, TrueHUD will not display actor info bars for companion players
				// even if other NPCs are attacking them.
				
				if (a_this->combatController)
				{
					a_this->combatController->inactive = true;
					a_this->combatController->ignoringCombat = true;
					a_this->combatController->stoppedCombat = true;
					a_this->combatController->startedCombat = false;
					a_this->combatController->targetHandle = 
					a_this->combatController->previousTargetHandle = RE::ActorHandle();
					a_this->combatController->cachedTarget = nullptr;
				}

				// Stop idles and any executing scene packages, but not run-once packages, 
				// which are generated at runtime and have the 'FF' mod index.
				// Unfortunately, the any already-triggered idle still briefly plays 
				// until the previously-running co-op package is evaluated.
				// Prevents companion players from losing control of their character
				// and getting locked into animations.
				if (p->coopActor->currentProcess && p->coopActor->currentProcess->middleHigh)
				{
					auto runningPackage = p->coopActor->GetCurrentPackage();
					// Different package than current and not a run-once package 
					// or a package from this mod.
					if ((runningPackage && runningPackage != p->pam->GetCurrentPackage()) &&
						((runningPackage->formID >> 24) != 0xFF) && 
						((runningPackage->formID >> 12) != (p->coopActor->formID >> 12)))
					{
						DBG
						(
							"{}: Stop current package: idle: {}, running: {}, scene: {}.", 
							p->coopActor->GetName(),
							p->coopActor->currentProcess->middleHigh->unk210 ?
							Util::GetEditorID(p->coopActor->currentProcess->middleHigh->unk210) :
							"NONE",
							runningPackage ?
							Util::GetEditorID(runningPackage) :
							"NONE",
							p->coopActor->GetCurrentScene() ? 
							Util::GetEditorID(p->coopActor->GetCurrentScene()) :
							"NONE"
						);
					
						p->coopActor->SetCurrentScene(nullptr);
						// Package idle playing currently.
						if (p->coopActor->currentProcess->middleHigh->unk210)
						{
							// Only way to stop package idles from playing
							// and from continuing to evaluate the package originating the idle.
							// The game will continue to set the radiant package 
							// as the current package in subsequent frames otherwise.
							p->coopActor->StopInteractingQuick(true);
							p->pam->StopCurrentIdle();
						}

						// Execute the previously-run co-op package following the interruption.
						p->pam->SetAndEveluatePackage(p->pam->GetCurrentPackage());
					}
				}

				// Stop combat between companion players (P2, P3, P4).
				a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
				if (auto combatGroup = a_this->GetCombatGroup(); combatGroup)
				{
					combatGroup->lock.LockForWrite();
							
					// Stop attacking and combat.
					for (auto iter = combatGroup->targets.begin();
						iter >= combatGroup->targets.begin() &&
						iter < combatGroup->targets.end();
						++iter)
					{
						// Already a target, so we can exit.
						auto pIndex = GlobalCoopData::GetCoopPlayerIndex
						(
							iter->targetHandle
						);
						if (pIndex != -1)
						{
							iter = combatGroup->targets.erase(iter);
							if (iter > combatGroup->targets.begin())
							{
								--iter;
							}
						}
					}

					combatGroup->lock.UnlockForWrite();
				}

				// Let the game update the player first after we've stopped combat.
				_Update(a_this, a_delta);

				//===================
				// Node Orientations.
				//===================
				// NOTE:
				// All downward passes for the player's nodes have been performed at this point, 
				// so restore all saved default local transforms for the next frame.
				// Reasoning: Sometimes, such as when a havok impulse is applied to the player,
				// the game won't restore the animation-derived local transforms 
				// for all the player's nodes, since the havok impulse applied its own
				// overriding local transform to the node(s).
				// Thus, any of our local transform modifications from the last frame 
				// will carry over and stack with this frame's,
				// which leads to setting incorrect local transforms (lots of spinning nodes) 
				// unless the defaults are restored first.
				p->mm->nom->RestoreOriginalNodeLocalTransforms(p);

				//===========================
				// Movement and Player State.
				//===========================
				
				// Make sure the player's life state reports them as alive once no longer downed.
				bool inDownedLifeState = 
				{
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
				};
				if (glob.livingPlayers > 0 && !p->isDowned && inDownedLifeState)
				{
					a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
				}

				auto currentProc = a_this->currentProcess; 
				if (!currentProc)
				{
					return;
				}
				
				auto ui = RE::UI::GetSingleton();
				auto high = currentProc->high; 
				bool gamePaused = ui->GameIsPaused();
				if (high && !gamePaused && p->mm->IsRunning())
				{
					auto paraMT = glob.paraglidingMT;
					auto& speeds = 
					(
						high->currentMovementType.defaultData.speeds
					);
					auto& rotateWhileMovingRun = 
					(
						high->currentMovementType.defaultData.rotateWhileMovingRun	
					);

					// NOTE: 
					// Base movement type data values seem to only reset 
					// to their defaults each frame 
					// if the player's speedmult is modified.
					// Otherwise, the movement speed changes each frame will accumulate, 
					// reaching infinity and preventing the player from moving.
					float speedMultToSet = p->mm->speedMult;
					if (speedMultToSet < 0.0f || isnan(speedMultToSet) || isinf(speedMultToSet))
					{
						speedMultToSet = p->mm->baseSpeedMult;
					}

					p->coopActor->SetBaseActorValue(RE::ActorValue::kSpeedMult, speedMultToSet);
					// Applies the new speedmult right away,
					p->coopActor->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f
					);
					p->coopActor->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f
					);

					// NOTE:
					// Another annoying issue to work around:
					// Since movement speed does not update 
					// while the player is ragdolled or getting up,
					// if the player was moving fast before ragdolling, 
					// they'll shoot out in their facing direction
					// once they fully get up and until their movement speed normalizes.
					// Do not allow movement until the player's movement speed zeroes out 
					// if the player has just fully gotten up.
					// Obviously a better solution would involve 
					// finding a way to set movement speed directly to 0
					// when ragdolled or getting up, but for now, this'll have to do.

					// Set movement speed to an obscenely high value to quickly
					// arrest built up momentum while also keeping the player in place
					// with the 'don't move' flag.
					if (p->mm->shouldCurtailMomentum)
					{
						// Ensure the player is set to not move 
						// and any lingering movement offset is cleared.
						// Otherwise, sanic mode.
						p->mm->ClearKeepOffsetFromActor();
						Util::NativeFunctions::SetDontMove(p->coopActor.get(), true);

						// Affects how quickly the player slows down.
						// The higher, the faster the reported movement speed becomes zero.
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kLeft]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kLeft]
						[RE::Movement::MaxSpeeds::kRun]				= 
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRight]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRight]
						[RE::Movement::MaxSpeeds::kRun]				= 
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kForward]
						[RE::Movement::MaxSpeeds::kWalk]			= 
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kForward]
						[RE::Movement::MaxSpeeds::kRun]				=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kBack]
						[RE::Movement::MaxSpeeds::kWalk]			= 
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kBack]
						[RE::Movement::MaxSpeeds::kRun]				= 100000.0f;
					}
					else if (auto charController = p->coopActor->GetCharController(); 
							 charController)
					{
						//================
						// Rotation speed.
						//================
						if (p->mm->isDashDodging)
						{
							// No rotation when dodging.
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kWalk]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kRun]					=
							rotateWhileMovingRun							= 0.0f;
						}
						else if (p->mm->isParagliding)
						{
							// Scale up default rotation rates.
							if (paraMT)
							{
								const auto& paraglidingSpeeds = 
								(
									paraMT->movementTypeData.defaultData.speeds
								);
								const auto& paraglidingRotateWhileMovingRun = 
								(
									paraMT->movementTypeData.defaultData.rotateWhileMovingRun	
								);

								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kWalk] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kRotations]
									[RE::Movement::MaxSpeeds::kWalk] * 
									Settings::fBaseRotationMult * 
									Settings::fBaseMTRotationMult
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kRun] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kRotations]
									[RE::Movement::MaxSpeeds::kRun] * 
									Settings::fBaseRotationMult * 
									Settings::fBaseMTRotationMult
								);
								rotateWhileMovingRun = 
								(
									paraglidingRotateWhileMovingRun * 
									Settings::fBaseRotationMult * 
									Settings::fBaseMTRotationMult
								);
							}
							else
							{
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kWalk]				= 
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kRun]					=
								(
									70.0f * TO_RADIANS *
									Settings::fBaseRotationMult * 
									Settings::fBaseMTRotationMult
								);

								rotateWhileMovingRun = 
								(
									120.0f * TO_RADIANS * 
									Settings::fBaseRotationMult * 
									Settings::fBaseMTRotationMult
								);
							}
						}
						else
						{
							// Increase rotation speed 
							// since all the movement types' default speeds
							// are too slow when used with KeepOffsetFromActor()
							// and produce sluggish changes in movement direction.
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kWalk]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kRun]					=
							rotateWhileMovingRun							= 
							(
								Settings::fBaseRotationMult * Settings::fBaseMTRotationMult * PI
							);
						}

						//=================
						// Movement speeds.
						//=================
						// NOTE:
						// Paraglide dodge velocity changes are char controller velocity-based 
						// and are not handled here.
						// Simply set the movement type data to the paraglide MT equivalent.
						if (p->mm->isParagliding)
						{
							if (paraMT)
							{
								const auto& paraglidingSpeeds = 
								(
									paraMT->movementTypeData.defaultData.speeds
								);

								// Movement speeds.
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kWalk] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kLeft]
									[RE::Movement::MaxSpeeds::kWalk]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kRun] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kLeft]
									[RE::Movement::MaxSpeeds::kRun]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kWalk] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kRight]
									[RE::Movement::MaxSpeeds::kWalk]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kRun] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kRight]
									[RE::Movement::MaxSpeeds::kRun]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kWalk] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kForward]
									[RE::Movement::MaxSpeeds::kWalk]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kRun] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kForward]
									[RE::Movement::MaxSpeeds::kRun]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kWalk] = 
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kBack]
									[RE::Movement::MaxSpeeds::kWalk]
								);
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kRun] =
								(
									paraglidingSpeeds
									[RE::Movement::SPEED_DIRECTIONS::kBack]
									[RE::Movement::MaxSpeeds::kRun]
								);
							}
							else
							{
								// Same movement speeds across the board when paragliding.
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kWalk]			=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kRun]				=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kWalk]			=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kRun]				=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kWalk]			=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kRun]				=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kWalk]			=
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kRun]				= 700.0f;
							}
						}
						else if (p->mm->isDashDodging)
						{
							// Interpolate between the starting and ending speedmult values.
							float dodgeSpeed = Util::InterpolateEaseInEaseOut
							(
								Settings::fMaxDashDodgeSpeedmult,
								Settings::fMinDashDodgeSpeedmult,
								p->mm->dashDodgeCompletionRatio,
								2.0f
							);

							// Same speed across the board when dodging.
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kRun]				= dodgeSpeed;
						}
						else
						{
							// Out velocity seems to be the intended velocity 
							// before collisions are accounted for.
							// Do not need the Z component.
							RE::NiPoint3 linVelXY = RE::NiPoint3
							(
								charController->outVelocity.quad.m128_f32[0], 
								charController->outVelocity.quad.m128_f32[1], 
								0.0f
							);
							auto linVelYaw = 
							(
								linVelXY.Length() == 0.0f ? 
								p->analogStickParams[!AnalogStickParams::kLSCamRelAng] : 
								Util::DirectionToGameAngYaw(linVelXY)
							);
							// Yaw difference between the XY velocity direction 
							// and the direction in which the player wishes to head.
							float movementToHeadingAngDiff = 
							(
								p->lsMoved ? 
								Util::NormalizeAngToPi
								(
									p->analogStickParams[!AnalogStickParams::kLSCamRelAng] - 
									linVelYaw
								) : 
								0.0f
							);
							// Sets the bounds for the diff factor applied to movement speed below. 
							// Dependent on rotation speeds -- rotate faster, pivot faster.
							float range = max
							(
								1.0f, 
								(Settings::fBaseMTRotationMult * Settings::fBaseRotationMult) / 
								3.0f
							);
							// Max speed factor. Maxes out at 90 degrees.
							float diffFactor =
							(
								1.0f + 
								(
									range * 
									powf
									(
										std::clamp
										(
											fabsf(movementToHeadingAngDiff) / (PI / 2.0f), 
											0.0f, 
											1.0f
										), 
										6.0f
									)
								)
							);

							// Player must not be sprinting, mounted, downed, animation driven, 
							// or running their interaction package.
							if (!p->pam->isSprinting && 
								!p->coopActor->IsOnMount() &&
								!p->mm->isAnimDriven && 
								!p->mm->interactionPackageRunning &&
								!p->isDowned)
							{
								// The core movement problem when using KeepOffsetFromActor() 
								// with the player themselves as the offset target
								// is slow deceleration/acceleration 
								// when changing directions rapidly.
								// First noticed that playing the 'SprintStart' animation event
								// right as the player starts pivoting causes them to turn
								// and face the new movement direction almost instantly.
								// Increasing the movement type's directional max speed values, 
								// depending on how rapidly the player is turning,
								// has the same effect as forcing the player to briefly sprint 
								// each time they change directions
								// and removes most of the sluggishness.
								// Can still cause rapid bursts of movement at times.

								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
								speeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
							}
						}
					}

					// Prevent automatic armor re-equip.
					high->reEquipArmorTimer = FLT_MAX;
				}

				if (auto midHigh = currentProc->middleHigh; midHigh)
				{
					// Prevent the game from automatically equipping 
					// a torch on co-op companions while in dim environments.
					// Seems to attempt torch equip when the timer hits 0 or below.
					midHigh->torchEvaluationTimer = FLT_MAX;

					// If using the revive system and killed by another player, 
					// prevent the game from forcing the player into bleedout.
					if (Settings::bUseReviveSystem && Settings::bCanKillmoveOtherPlayers)
					{
						midHigh->deferredKillTimer = FLT_MAX;
					}
				}

				// [TEMP WORKAROUND 1]:
				// Temporary solution to players becoming "hostile" towards one another.
				// Remove targeted players from this player's combat group.
				a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
				Util::RemovePlayerCombatTargets(a_this);
				
				// Already performed the player update, so return.
				return;
			}
			// Not a co-op entity.
			else
			{
				bool isMountedByPlayer = false;
				RE::ActorPtr riderPtr{ nullptr };
				if (a_this->IsAMount())
				{
					a_this->GetMountedBy(riderPtr);
					if (riderPtr)
					{
						isMountedByPlayer = GlobalCoopData::IsCoopPlayer(riderPtr.get());
					}
				}

				if (isMountedByPlayer)
				{
					// Modify mount speed mult when sprinting to maintain speed consistency 
					// between P1 and other players' mounts.
					const auto& p =
					(
						glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(riderPtr.get())]
					);
					if (p->pam->IsPerforming(InputAction::kSprint))
					{
						a_this->SetActorValue
						(
							RE::ActorValue::kSpeedMult, 120.0f * Settings::fSprintingMovMult
						);
						a_this->RestoreActorValue
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, 
							RE::ActorValue::kCarryWeight, 
							-0.001f
						);
						a_this->RestoreActorValue
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, 
							RE::ActorValue::kCarryWeight, 
							0.001f
						);
					}
					else
					{
						a_this->SetActorValue(RE::ActorValue::kSpeedMult, 120.0f);
						a_this->RestoreActorValue
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, 
							RE::ActorValue::kCarryWeight,
							-0.001f
						);
						a_this->RestoreActorValue
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, 
							RE::ActorValue::kCarryWeight,
							0.001f
						);
					}
				}
				else
				{
					auto currentProc = a_this->currentProcess; 
					if (!currentProc || !currentProc->middleHigh)
					{
						return _Update(a_this, a_delta);
					}
					
					// [TEMP WORKAROUND 1]:
					// Temporary solution to allies/teammates attacking co-op players,
					// even on accidental hits.
					// Stop combat right away if the current combat target is a co-op player.
					if (Util::IsPartyFriendlyActor(a_this))
					{
						a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
						Util::RemovePlayerCombatTargets(a_this);
					}

					// Let the game update this character first.
					_Update(a_this, a_delta);

					// Ensure NPC in dialogue with the player is not faded out if ACC is installed.
					if (ALYSLC::AlternateConversationCameraCompat::g_installed)
					{
						const auto handle = a_this->GetHandle();
						auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
						if ((menuTopicManager) && 
							(
								menuTopicManager->speaker == handle ||
								menuTopicManager->lastSpeaker == handle
							))
						{
							if (a_this->GetAlpha() != 1.0f)
							{
								a_this->SetAlpha(1.0f);
							}

							auto currentProc = a_this->currentProcess; 
							if (currentProc && currentProc->high) 
							{
								if (currentProc->high->fadeAlpha != 1.0f)
								{
									currentProc->high->fadeAlpha = 1.0f;
								}

								if (currentProc->high->fadeState.any
									(
										RE::HighProcessData::FADE_STATE::kIn,
										RE::HighProcessData::FADE_STATE::kOut,
										RE::HighProcessData::FADE_STATE::kOutDelete,
										RE::HighProcessData::FADE_STATE::kOutDisable,
										RE::HighProcessData::FADE_STATE::kTeleportIn,
										RE::HighProcessData::FADE_STATE::kTeleportOut
									))
								{
									currentProc->high->fadeState.reset
									(
										RE::HighProcessData::FADE_STATE::kIn,
										RE::HighProcessData::FADE_STATE::kOut,
										RE::HighProcessData::FADE_STATE::kOutDelete,
										RE::HighProcessData::FADE_STATE::kOutDisable,
										RE::HighProcessData::FADE_STATE::kTeleportIn,
										RE::HighProcessData::FADE_STATE::kTeleportOut
									);
								}
							}

							auto speaker3DPtr = Util::GetRefr3D(a_this); 	
							if (speaker3DPtr)
							{
								bool update3D = false;
								if (speaker3DPtr->fadeAmount != 1.0f)
								{
									speaker3DPtr->fadeAmount = 1.0f;
									update3D = true;
								}

								if (speaker3DPtr->flags.all(RE::NiAVObject::Flag::kHidden))
								{
									speaker3DPtr->flags.reset(RE::NiAVObject::Flag::kHidden);
									update3D = true;
								}

								if (update3D)
								{
									RE::NiUpdateData updateData{ };
									speaker3DPtr->UpdateDownwardPass(updateData, 0);
								}
							}
						}
					}
							
					// [TEMP WORKAROUND 2]:
					// Disable Precision on this actor when ragdolled 
					// to avoid a ragdoll reset position glitch on knock explosion
					// where the hit actor is teleported to their last ragdoll position 
					// instead of staying at their current position.
					// Precision is re-enabled on the actor after they get up.
					if (Settings::bApplyTemporaryRagdollWarpWorkaround)
					{
						if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI4; api)
						{
							const auto handle = a_this->GetHandle();
							if (a_this->IsInRagdollState() && api->IsActorActive(handle))
							{
								api->ToggleDisableActor(handle, true);
							}
							else if (!a_this->IsInRagdollState() && 
									 a_this->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && 
									 !api->IsActorActive(handle))
							{
								api->ToggleDisableActor(handle, false);
							}
						}
					}

					// Already updated.
					return;
				}
			}

			_Update(a_this, a_delta);
		}

		std::uint32_t CharacterHooks::UseAmmo(RE::Character* a_this, std::uint32_t a_shotCount)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _UseAmmo(a_this, a_shotCount);
			}
			
			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			if (playerIndex == -1)
			{
				return _UseAmmo(a_this, a_shotCount);
			}

			const auto& p = glob.coopPlayers[playerIndex];

			// Do not remove ammo when in god mode.
			if (p->isInGodMode)
			{
				return _UseAmmo(a_this, 0);
			}

			auto currentAmmo = p->coopActor->GetCurrentAmmo();
			if (!currentAmmo)
			{
				return _UseAmmo(a_this, a_shotCount);
			}

			const auto oldCount = Util::GetInventoryItemCount
			(
				p->em->inventoryChest.get(), currentAmmo
			);
			// Remove worn extra data from the list if we're removing the last unit of ammo.
			// Will crash on removal otherwise.
			if (oldCount == 1)
			{
				Util::RemoveWornRankExtraData
				(
					currentAmmo,
					Util::GetWornRankExtraDataList
					(
						p->em->inventoryChest.get(), currentAmmo, false
					), 
					false
				);
			}
				
			DBG
			(
				"{}: About to remove {} of {}. Has {} currently.",
				p->coopActor->GetName(),
				a_shotCount,
				currentAmmo->GetName(),
				oldCount
			);
			
			// Have to re-favorite and re-hotkey once re-added. Ree.
			int8_t oldHotkey = -1;
			auto hotkeyData = Util::GetHotkeyExData
			(
				p->em->inventoryChest.get(), currentAmmo, nullptr
			);
			bool wasFavorited = (bool)hotkeyData;
			if (hotkeyData)
			{
				oldHotkey = !*hotkeyData->hotkey;
			}
				
			// 'Use' the ammo from the chest.
			p->em->inventoryChest->RemoveItem
			(
				currentAmmo, 
				a_shotCount,
				RE::ITEM_REMOVE_REASON::kRemove,
				nullptr, 
				nullptr
			);

			auto newCount = Util::GetInventoryItemCount
			(
				p->em->inventoryChest.get(), currentAmmo
			);
			DBG
			(
				"{}: Removing {} of {} from their inventory chest. {} -> {}",
				p->coopActor->GetName(),
				a_shotCount,
				currentAmmo->GetName(),
				oldCount,
				newCount
			);
			// Only use remaining ammo if there is none left in the inventory chest.
			if (newCount <= 0)
			{
				auto playerAmmoCount = Util::GetInventoryItemCount
				(
					p->coopActor.get(), currentAmmo
				);
				if (playerAmmoCount > 0)
				{
					return _UseAmmo(a_this, playerAmmoCount);
				}
				else
				{
					return _UseAmmo(a_this, a_shotCount);
				}
			}
			else
			{
				if (wasFavorited)
				{
					Util::ChangeFormFavoritesStatus
					(
						p->em->inventoryChest.get(), currentAmmo, true, nullptr
					);
					if (oldHotkey != -1)
					{
						Util::ChangeFormHotkeyStatus
						(
							p->em->inventoryChest.get(), currentAmmo, oldHotkey, nullptr
						);
					}
				}
			}

			return 0;
		}

		float CharacterHooks::GetActorValue(RE::ActorValueOwner* a_this, RE::ActorValue a_akValue)
		{
			if (!glob.globalDataInit)
			{
				return _GetActorValue(a_this, a_akValue);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					const float value = _GetActorValue(a_this, a_akValue);
					DBG
					(
						"{}: {} is {}.", 
						actorPtr->GetName(), 
						Util::GetActorValueName(a_akValue),
						value
					);

					return value;
				}
			}

			return _GetActorValue(a_this, a_akValue);
		}

		float CharacterHooks::GetBaseActorValue
		(
			RE::ActorValueOwner* a_this, RE::ActorValue a_akValue
		)
		{
			if (!glob.globalDataInit)
			{
				return _GetBaseActorValue(a_this, a_akValue);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					const float value = _GetBaseActorValue(a_this, a_akValue);
					DBG
					(
						"{}: {} is {}.", 
						actorPtr->GetName(), 
						Util::GetActorValueName(a_akValue),
						value
					);

					return value;
				}
			}

			return _GetBaseActorValue(a_this, a_akValue);
		}

		float CharacterHooks::GetPermanentActorValue
		(
			RE::ActorValueOwner* a_this, RE::ActorValue a_akValue
		)
		{
			if (!glob.globalDataInit)
			{
				return _GetPermanentActorValue(a_this, a_akValue);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					const float value = _GetPermanentActorValue(a_this, a_akValue);
					DBG
					(
						"{}: {} is {}.", 
						actorPtr->GetName(), 
						Util::GetActorValueName(a_akValue),
						value
					);

					return value;
				}
			}

			return _GetPermanentActorValue(a_this, a_akValue);
		}

		void CharacterHooks::ModActorValue
		(
			RE::ActorValueOwner* a_this, RE::ActorValue a_akValue, float a_value
		)
		{
			if (!glob.globalDataInit)
			{
				return _ModActorValue(a_this, a_akValue, a_value);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					DBG
					(
						"{}: {} by {}.", 
						actorPtr->GetName(), 
						Util::GetActorValueName(a_akValue),
						a_value
					);
					break;
				}
			}

			return _ModActorValue(a_this, a_akValue, a_value);
		}

		void CharacterHooks::RestoreActorValue
		(
			RE::ActorValueOwner* a_this,
			RE::ACTOR_VALUE_MODIFIER a_modifier,
			RE::ActorValue a_akValue,
			float a_value
		)
		{
			if (!glob.globalDataInit)
			{
				return _RestoreActorValue(a_this, a_modifier, a_akValue, a_value);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					// Check if HMS permanent actor value will drop.
					// Prevent this from happening for now until the source of the bug is found.
					if ((a_modifier == RE::ACTOR_VALUE_MODIFIER::kPermanent) &&
						(a_akValue == RE::ActorValue::kHealth || 
						a_akValue == RE::ActorValue::kMagicka ||
						a_akValue == RE::ActorValue::kStamina))
					{
						float serializedBaseValue = 0.0f;
						const auto iter = glob.serializablePlayerData.find(actorPtr->formID);
						if (iter != glob.serializablePlayerData.end())
						{
							serializedBaseValue = 
							(
								iter->second->hmsBasePointsList
								[
									a_akValue == RE::ActorValue::kHealth ?
									0 :
									a_akValue == RE::ActorValue::kMagicka ? 
									1 :
									2
								]
							);
						}

						const float newPermMod = 
						(
							actorPtr->GetActorValueModifier(a_modifier, a_akValue) + a_value
						);
						const float currentPermValue = a_this->GetPermanentActorValue(a_akValue);
						// CHANGE TO DEBUG
						DBG
						(
							"{}: Adjusting {}'s {} modifier by {} to {}. "
							"New permanent value: {} ({} + {}), should be {} as serialized. "
							"Restore by {} instead.", 
							actorPtr->GetName(), 
							newPermMod > 0.0f ? 
							"ALLOW" :
							"IGNORE",
							Util::GetActorValueName(a_akValue),
							a_modifier == RE::ACTOR_VALUE_MODIFIER::kDamage ? 
							"DAMAGE" :
							a_modifier == RE::ACTOR_VALUE_MODIFIER::kPermanent ? 
							"PERMANENT" :
							a_modifier == RE::ACTOR_VALUE_MODIFIER::kTemporary ? 
							"TEMPORARY" :
							"INVALID",
							a_value,
							newPermMod,
							currentPermValue + a_value,
							currentPermValue,
							a_value,
							serializedBaseValue,
							serializedBaseValue - currentPermValue
						);
						if (currentPermValue + a_value <= 0.0f)
						{
							// Set to serialized value or ignore this call
							// if there is no serialized value.
							if (serializedBaseValue != 0.0f)
							{
								_RestoreActorValue
								(
									a_this,
									a_modifier,
									a_akValue, 
									serializedBaseValue - currentPermValue
								);
								a_this->SetBaseActorValue(a_akValue, serializedBaseValue);
								ERR
								(
									"ERR: {}: Permanent {} was almost set to <= 0 value. "
									"Set to {}, modifier to {}, base to {}.",
									actorPtr->GetName(),
									Util::GetActorValueName(a_akValue),
									a_this->GetPermanentActorValue(a_akValue),
									actorPtr->GetActorValueModifier
									(
										RE::ACTOR_VALUE_MODIFIER::kPermanent, a_akValue
									),
									a_this->GetBaseActorValue(a_akValue)
								);
							}
							else
							{
								ERR
								(
									"ERR: {}: Permanent {} was almost set to <= 0 value. "
									"SKIP because no serialized value is present.",
									actorPtr->GetName(),
									Util::GetActorValueName(a_akValue)
								);	
							}

							return;
						}
					}
					
					break;
				}
			}

			return _RestoreActorValue(a_this, a_modifier, a_akValue, a_value);
		}

		void CharacterHooks::SetActorValue
		(
			RE::ActorValueOwner* a_this, RE::ActorValue a_akValue, float a_value
		)
		{
			if (!glob.globalDataInit)
			{
				return _SetActorValue(a_this, a_akValue, a_value);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				if (actorPtr && a_this == actorPtr.get())
				{
					DBG
					(
						"{}: {} to {}.", 
						actorPtr->GetName(), 
						Util::GetActorValueName(a_akValue),
						a_value
					);
					break;
				}
			}

			return _SetActorValue(a_this, a_akValue, a_value);
		}

		void CharacterHooks::SetBaseActorValue
		(
			RE::ActorValueOwner* a_this, RE::ActorValue a_akValue, float a_value
		)
		{
			if (!glob.globalDataInit)
			{
				return _SetBaseActorValue(a_this, a_akValue, a_value);
			}

			for (const auto actorPtr : glob.coopEntityBlacklist)
			{
				// Not a player.
				if (!actorPtr || a_this != actorPtr.get())
				{
					continue;
				}
				
				bool isHMS = 
				(
					a_akValue == RE::ActorValue::kHealth || 
					a_akValue == RE::ActorValue::kMagicka ||
					a_akValue == RE::ActorValue::kStamina
				);
				const auto skillIter = GlobalCoopData::AV_TO_SKILL_MAP.find(a_akValue);
				bool isSkill = skillIter != GlobalCoopData::AV_TO_SKILL_MAP.end();
				// Skip if not an HMS or skill AV.
				if (!isHMS && !isSkill)
				{
					break;
				}
				
				// Do not change the value if there is no serialized data for the player.
				const auto iter = glob.serializablePlayerData.find(actorPtr->formID);
				if (iter == glob.serializablePlayerData.end())
				{
					break;
				}

				const auto& data = iter->second;
				float serializedBaseValue = a_value;
				if (isHMS)
				{
					const auto index =
					(
						a_akValue == RE::ActorValue::kHealth ? 
						0 :
						a_akValue == RE::ActorValue::kMagicka ? 
						1 : 
						2
					);
					serializedBaseValue = 
					(
						data->hmsBasePointsList[index] + data->hmsPointIncreasesList[index]
					);
					if (a_value != serializedBaseValue)
					{
						// CHANGE TO DEBUG
						DBG
						(
							"{}: Trying to set attribute {}'s base value to {}. Set to {} instead.",
							actorPtr->GetName(), 
							Util::GetActorValueName(a_akValue),
							a_value, 
							serializedBaseValue
						);
					}
				}
				// May be unnecessary to commented out for now.
				//else
				//{
				//	const auto index = skillIter->second;
				//	serializedBaseValue = 
				//	(
				//		data->skillBaseLevelsList[index] + data->skillLevelIncreasesList[index]
				//	);
				//	if (a_value != serializedBaseValue)
				//	{
				//		// CHANGE TO DEBUG
				//		DBG
				//		(
				//			"{}: Trying to set skill {}'s base value to {}. Set to {} instead.",
				//			actorPtr->GetName(), 
				//			Util::GetActorValueName(a_akValue),
				//			a_value, 
				//			serializedBaseValue
				//		);
				//	}
				//}

				// Break and set.
				a_value = serializedBaseValue;
				break;
			}

			return _SetBaseActorValue(a_this, a_akValue, a_value);
		}

// [INPUT EVENT HOOKS]:
		void InputEventHooks::DispatchInputEvents
		(
			RE::BSTEventSource<RE::InputEvent*>* a_this,
			RE::InputEvent** a_inputEvents
		)
		{
			if (!a_inputEvents)
			{
				return _DispatchInputEvents(a_this, a_inputEvents);
			}

			// No input events this frame, so reset all menu controls-related data.
			if (!(*a_inputEvents))
			{
				MenuControlsHooks::summoningMenuBindPressed = 
				MenuControlsHooks::pauseAndWaitWerePressed =
				MenuControlsHooks::debugMenuBindPressed = false;
				MenuControlsHooks::pauseBindHeldTime =
				MenuControlsHooks::waitBindHeldTime = -1.0f;
			}
			
			uint32_t i = 0;
			RE::InputEvent* event{ *a_inputEvents };
			while (event)
			{
				// Clear padding on internally-sent input events.
				if (auto idEvent = event->AsIDEvent(); idEvent)
				{
					if (idEvent->pad24 != 0)
					{
						DBG("Event {} does not have a pad of 0: 0x{:X}.",
							idEvent->userEvent, idEvent->pad24);
					}

					idEvent->pad24 = 0;
				}
				
				++i;
				event = event->next;
			}
			
			// Sending the companion player's input events separately 
			// only allows for processing of one input device's events at a time.
			// For example, if P2 is moving the left stick while in their inventory
			// and P1 is pressing WASD, P2 will change the selected menu entry, 
			// while P1 stays still instead of moving.
			// We rectify this issue by chaining the companion player's inputs 
			// to the beginning of the game's input event chain.
			if (glob.globalDataInit && glob.mim->IsRunning())
			{
				if (!glob.mim->queuedInputEvents.empty())
				{
					/*DBG("{} queued input events to tack onto list of {} events.",
						glob.mim->queuedInputEvents.size(), i);*/
					const auto& lastQueuedEvent = 
					(
						*glob.mim->queuedInputEvents[glob.mim->queuedInputEvents.size() - 1]
					);
					if (*a_inputEvents)
					{
						lastQueuedEvent->next = *a_inputEvents;	
					}
						
					*a_inputEvents = *glob.mim->queuedInputEvents[0];
				}
				
				glob.mim->queuedInputEvents.clear();
			}

			return _DispatchInputEvents(a_this, a_inputEvents);
		}

// [LEGENDARY SKILL CALLBACK HOOKS]:
	void LegendarySkillResetConfirmCallbackHooks::Run
	(
		RE::LegendarySkillResetConfirmCallback* a_this, RE::IMessageBoxCallback::Message a_msg
	)
	{
		// NOTE:
		// Unsatisfied with the hacky approach below.
		// Need to figure out how to register a callback instead of hooking here,
		// since multiple callbacks can run each time the player confirms 
		// a Legendary leveling skill reset and we only want to refund perk points once.
		// Also, if a UI mod removes the confirmation message box, well,
		// we can't refund the correct number of perks.
		// Also also, may remove eventually if shared perks are done away with.

		if (!glob.globalDataInit)
		{
			return _Run(a_this, a_msg);
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return _Run(a_this, a_msg);
		}

		// 'kUnk1' indicates that the 'Yes' button was pressed to confirm the skill reset.
		// Skip any other messages.
		if (a_msg != RE::IMessageBoxCallback::Message::kUnk1)
		{
			return _Run(a_this, a_msg);
		}

		int8_t perkCountBefore = p1->perkCount;
		// Get the set of perks to refund.
		// Avoid inserting duplicates.
		// Overriding the game's attempt at refunding which seems to not refund 
		// the proper number of perks sometimes.
		std::set<RE::BGSPerk*> unlockedPerks{ };
		bool isShared = glob.SHARED_SKILL_AVS_SET.contains(a_this->skill);
		const auto skillName = Util::GetActorValueName(a_this->skill);
		auto lookForUnlockedPerks = 
		[p1, &unlockedPerks, &skillName]
		(RE::BGSSkillPerkTreeNode* a_node, RE::Actor* a_actor) 
		{
			if (!a_node || 
				!a_node->associatedSkill || 
				a_node->associatedSkill->enumName != skillName)
			{
				return;
			}
				
			auto perk = a_node->perk;
			while (perk)
			{
				if (Util::Player1PerkListHasPerk(perk))
				{
					unlockedPerks.emplace(perk);
				}

				perk = perk->nextPerk;
			}
		};

		Util::TraverseAllPerks(p1, lookForUnlockedPerks);
		DBG
		(
			"{} perks unlocked in {}'s skill tree. Skill level is {}.", 
			unlockedPerks.size(),
			Util::GetActorValueName(a_this->skill),
			p1->GetActorValue(a_this->skill)
		);

		// Let the game clear out the tree and set the perk count, which we will override after.
		_Run(a_this, a_msg);

		if (!unlockedPerks.empty() && isShared)
		{
			const auto fid = 
			(
				glob.coopSessionActive && 
				glob.menuPID != -1 && 
				glob.coopPlayers[glob.menuPID]->coopActor ? 
				glob.coopPlayers[glob.menuPID]->coopActor->formID : 
				p1->formID
			);
			const auto iter = glob.serializablePlayerData.find(fid);
			if (iter == glob.serializablePlayerData.end())
			{
				return;
			}
			
			auto& data = iter->second;
			const auto iter2 = glob.AV_TO_SKILL_MAP.find(a_this->skill);
			if (iter2 == glob.AV_TO_SKILL_MAP.end())
			{
				return;
			}
			
			// With just SKYUI and no Stats Menu modifications, two callbacks appear to fire.
			// P1's legendary leveling count for the skill is incremented between the first 
			// and the second callback, so we skip refunding perk points 
			// when the first callback fires.
			auto sharedSkill = iter2->second;
			DBG
			(
				"Player with FID 0x{:X} has already leveled {} {} times. "
				"P1 has Legendary leveling count {}.", 
				fid,
				Util::GetActorValueName(a_this->skill),
				data->skillLegendaryList[sharedSkill], 
				p1->skills->data->legendaryLevels[sharedSkill]
			);
			if (data->skillLegendaryList[sharedSkill] ==
				p1->skills->data->legendaryLevels[sharedSkill])
			{
				return;
			}

			// Number of perk points to give back to this player [0, number of perks in the tree].
			uint32_t refundCount = 0;
			auto& takenSharedPerksSet = data->GetTakenSharedPerksSet();
			for (const auto perk : unlockedPerks)
			{
				// Already saved as taken by the menu-controlling player.
				if (takenSharedPerksSet.contains(perk))
				{
					DBG
					(
						"{}'s perk {} (0x{:X} was saved as taken by {}. "
						"Add to refund count.",
						skillName,
						perk->GetName(),
						perk->formID,
						glob.coopSessionActive && glob.menuPID != -1 ? 
						glob.coopPlayers[glob.menuPID]->coopActor->GetName() :
						p1->GetName()
					);
					++refundCount;
				}
				else
				{
					// Check to see if no other players have unlocked the perk,
					// meaning it hasn't been saved as unlocked yet and this player has taken it
					// while the Stats Menu is open.
					bool savedAsUnlocked = false;
					for (const auto& [_, data2] : glob.serializablePlayerData)
					{
						if (!data2)
						{
							continue;
						}

						if (data2->GetTakenSharedPerksSet().contains(perk))
						{
							savedAsUnlocked = true;
							break;
						}
					}

					if (!savedAsUnlocked)
					{
						DBG
						(
							"{}'s perk {} (0x{:X} was just taken by {} while in the Stats Menu. "
							"Add to refund count.",
							skillName,
							perk->GetName(),
							perk->formID,
							glob.coopSessionActive && glob.menuPID != -1 ? 
							glob.coopPlayers[glob.menuPID]->coopActor->GetName() :
							p1->GetName()
						);
						++refundCount;
					}
				}
			}
			
			DBG
			(
				"Perk count for {} (msg: {}, unks: 0x{:X}, 0x{:X}, 0x{:X}), went from {} to {}. "
				"Shared perk points to refund: {}. Perk count is now {}.", 
				Util::GetActorValueName(a_this->skill), 
				a_msg,
				a_this->unk0C,
				a_this->unk10,
				a_this->unk18,
				perkCountBefore, 
				p1->perkCount,
				refundCount,
				perkCountBefore + refundCount
			);
			p1->perkCount = perkCountBefore + refundCount;
		}
		else if (isShared)
		{
			p1->perkCount = 0;
			DBG
			(
				"No shared perks unlocked for skill {} (msg: {}). Set perk count to 0.", 
				Util::GetActorValueName(a_this->skill), 
				a_msg
			);
		}
		else
		{
			// Game sometimes removes perk points instead of refunding them (? lol).
			// Use our own calculated unlocked perks count instead if this happens.
			if (perkCountBefore > p1->perkCount)
			{
				DBG
				(
					"CALLBACK: {} ({}). Perk count decreased from {} to {}, "
					"setting to {} instead.",
					Util::GetActorValueName(a_this->skill), 
					a_msg,
					perkCountBefore,
					p1->perkCount,
					perkCountBefore + unlockedPerks.size()
					//perkCountBefore + (perkCountBefore - p1->perkCount)
				);
				p1->perkCount = perkCountBefore + unlockedPerks.size(); 
				//perkCountBefore + (perkCountBefore - p1->perkCount);
				// Also, notify the player that the perk count may still be incorrect
				// and that the issue can be remedied by closing and re-opening the perk tree.
				RE::DebugMessageBox
				(
					"[ALYSLC]\nIf the number of refunded perk points is incorrect, "
					"please re-open the perk tree to set the proper amount."
				);
			}
			else
			{
				DBG
				(
					"Nothing to adjust for skill {} (msg: {}). Retain perk count of {}.", 
					Util::GetActorValueName(a_this->skill), 
					a_msg,
					p1->perkCount
				);
			}
		}
	}

// [MAGIC STAGGER HOOKS]:
	
		void MagicStaggerHooks::ProcessStagger
		(
			RE::Actor* a_target, float a_staggerMult, RE::Actor* a_aggressor
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _ProcessStagger(a_target, a_staggerMult, a_aggressor);
			}

			if (const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_target); pIndex != -1)
			{
				const auto& p = glob.coopPlayers[pIndex];
				// Remove stagger if reviving or playing an animation.
				if (p->isRevivingPlayer || p->coopActor->IsAnimationDriven())
				{
					DBG
					(
						"Removing stagger from magic hit while {} is reviving another player.",
						p->coopActor->GetName()
					);
					return;
				}
			}

			return _ProcessStagger(a_target, a_staggerMult, a_aggressor);
		}

// [MELEE HIT HOOKS]:
		void MeleeHitHooks::ProcessHit(RE::Actor* a_victim, RE::HitData& a_hitData)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _ProcessHit(a_victim, a_hitData);
			}
			
			const auto attackerPtr = Util::GetActorPtrFromHandle(a_hitData.aggressor); 
			// If a companion player takes damage while mounted, they immediately dismount.
			// Since I can't find the source of that dismount call and block it from running,
			// and since the function(s) that force the player to dismount run 
			// when this hook executes, we'll just apply the intended damage 
			// and bail instead to prevent the dismount.
			if (GlobalCoopData::IsCoopPlayer(a_victim) && a_victim->IsOnMount())
			{
				DBG
				(
					"NOPE, NO MOUNT. Damage: {} by {} to {}. Gimme.",
					a_hitData.totalDamage,
					attackerPtr ? attackerPtr->GetName() : "NONE",
					a_victim ? a_victim->GetName() : "NONE"
				);
				a_victim->DoDamage
				(
					a_hitData.totalDamage,
					attackerPtr ? attackerPtr.get() : nullptr,
					true				
				);
				return;
			}

			DBG
			(
				"{} was hit by {}. Flags: 0b{:B}.", 
				a_victim ? a_victim->GetName() : "NONE",
				attackerPtr ? attackerPtr->GetName() : "NONE",
				*a_hitData.flags
			);
			if (!attackerPtr)
			{
				return _ProcessHit(a_victim, a_hitData);;
			}

			auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_victim);
			auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(attackerPtr.get());
			// Handle cached melee UseSkill() calls for P1 
			// (always fires before this hook does for melee skills).
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if ((p1) && 
				(glob.lastP1MeleeUseSkillCallArgs) && 
				(attackerPtr.get() == p1 || a_victim == p1))
			{
				// Conditions to execute delayed UseSkill call:
				// 1. P1 is the attacker:
				//	- Either must have friendly fire enabled 
				// or must not be attacking party-friendly actor.
				// 2. P1 is the victim:
				//	- The attacker is not another player 
				// or is a player that has friendly fire enabled.
				bool p1AttackerCanUseSkill = 
				(
					(attackerPtr.get() == p1) && 
					(Settings::vbFriendlyFire[0] || !Util::IsPartyFriendlyActor(a_victim))
				);
				bool p1VictimCanUseSkill = 
				(
					(a_victim == p1) && 
					(
						playerAttackerIndex == -1 || 
						Settings::vbFriendlyFire[glob.coopPlayers[playerAttackerIndex]->playerID]
					)
				);
				if (p1AttackerCanUseSkill || p1VictimCanUseSkill)
				{
					p1->UseSkill
					(
						glob.lastP1MeleeUseSkillCallArgs->skill,
						glob.lastP1MeleeUseSkillCallArgs->points, 
						glob.lastP1MeleeUseSkillCallArgs->assocForm
					);
				}

				glob.lastP1MeleeUseSkillCallArgs.reset();
			}

			// Player was hit; add armor/block XP as needed.
			if (playerVictimIndex != -1)
			{
				const auto& p = glob.coopPlayers[playerVictimIndex];
				// Remove stagger if reviving or playing an animation.
				if (p->isRevivingPlayer || p->coopActor->IsAnimationDriven())
				{
					DBG
					(
						"Removing stagger from melee hit while {} is reviving another player.",
						p->coopActor->GetName()
					);
					a_hitData.stagger = 0.0f;
					if (a_hitData.attackData)
					{
						a_hitData.attackData->data.knockDown = 0.0f;
						a_hitData.attackData->data.staggerOffset = 0.0f;
					}
				}

				// Not P1 and not attacked by another player 
				// or not in god mode and friendly fire is enabled.
				const bool canAddXP = 
				(
					(!p->isPlayer1) &&
					(
						(playerAttackerIndex == -1) || 
						(
							!p->isInGodMode && 
							Settings::vbFriendlyFire
							[glob.coopPlayers[playerAttackerIndex]->playerID]
						)
					)
				);
				if (canAddXP)
				{
					const auto& armorRatings = p->em->armorRatings;
					float rawDamage = a_hitData.physicalDamage;
					// Block skill.
					if (a_hitData.percentBlocked > 0.0f)
					{
						GlobalCoopData::AddSkillXP
						(
							p->playerID,
							RE::ActorValue::kBlock, 
							rawDamage * a_hitData.percentBlocked
						);
					}

					// Armor skills.
					float lightArmorBaseXP = 
					(
						(rawDamage * armorRatings.first) / 
						(
							armorRatings.first + armorRatings.second == 0.0f ? 
							1.0f : 
							armorRatings.first + armorRatings.second
						)
					);
					float heavyArmorBaseXP = 
					(
						(rawDamage * armorRatings.second) / 
						(
							armorRatings.first + armorRatings.second == 0.0f ? 
							1.0f : 
							armorRatings.first + armorRatings.second
						)
					);
					if (lightArmorBaseXP > 0.0f)
					{
						GlobalCoopData::AddSkillXP
						(
							p->playerID, RE::ActorValue::kLightArmor, lightArmorBaseXP
						);
					}

					if (heavyArmorBaseXP > 0.0f)
					{
						GlobalCoopData::AddSkillXP
						(
							p->playerID, RE::ActorValue::kHeavyArmor, heavyArmorBaseXP
						);
					}
				}

				if (a_hitData.flags.any
				(
					RE::HitData::Flag::kDisableWeapon, RE::HitData::Flag::kDisarm
				))
				{
					DBG
					(
						"{} was disarmed by {}. Unequip hand forms.", 
						p->coopActor->GetName(), attackerPtr->GetName()
					);
					p->em->UnequipHandForms(glob.bothHandsEquipSlot);
				}
			}

			_ProcessHit(a_victim, a_hitData);
		}

// [MENU CONTROLS HOOKS]:
		EventResult MenuControlsHooks::ProcessEvent
		(
			RE::MenuControls* a_this, 
			RE::InputEvent** a_inputEvents, 
			RE::BSTEventSource<RE::InputEvent*>* a_eventSource
		)
		{
			auto ui = RE::UI::GetSingleton();
			if (!glob.globalDataInit || !ui || !a_inputEvents || !(*a_inputEvents))
			{
				return _ProcessEvent(a_this, a_inputEvents, a_eventSource);
			}

			// Do not process any events if an input device is (dis)connecting.
			auto event = *a_inputEvents;
			std::unordered_map
			<RE::InputEvent*, std::unique_ptr<GlobalCoopData::CachedInputEventData>>
			originalEventDataMap { };
			while (event)
			{
				if (*event->eventType == RE::INPUT_EVENT_TYPE::kDeviceConnect)
				{
					return _ProcessEvent(a_this, a_inputEvents, a_eventSource);
				}
				
				// Make sure the event type is valid, 
				// instead of the invalid flag we may have set earlier 
				// to block the event from being processed.
				RestoreInputEventType(event);
				if (auto buttonEvent = event->AsButtonEvent(); buttonEvent)
				{
					originalEventDataMap.insert
					(
						{
							event, 
							std::make_unique<GlobalCoopData::CachedInputEventData>
							(
								event->GetDevice(),
								*event->eventType,
								buttonEvent->userEvent,
								buttonEvent->idCode,
								buttonEvent->heldDownSecs,
								buttonEvent->value
							)
						}
					);
				}
				else
				{
					originalEventDataMap.insert
					(
						{
							event, 
							std::make_unique<GlobalCoopData::CachedInputEventData>
							(
								event->GetDevice(),
								*event->eventType,
								event->QUserEvent()
							)
						}
					);
				}

				event = event->next;
			}
			
			// Reset special co-op menu triggered states and bind hold times.
			bool allReleased = 
			(
				((*a_inputEvents) && !((*a_inputEvents)->next)) &&
				(
					!((*a_inputEvents)->AsButtonEvent()) ||
					!((*a_inputEvents)->AsButtonEvent()->IsPressed())
				)
			);
			// All buttons released, so reset state after processing.
			bool shouldReset = 
			(
				(allReleased) && 
				(
					summoningMenuBindPressed ||
					debugMenuBindPressed ||
					pauseBindHeldTime != -1.0f ||
					waitBindHeldTime != -1.0f
				)
			);

			// REMOVE when done debugging.
			// Troubleshooting an inconsistent 'stuck key' issue
			// that produces a lingering 'Tab' keyboard input
			// which heads every input chain after alt-tabbing into another window
			// and tabbing back into Skyrim.
			// As a result, we have to skip over this keyboard device input event each time.
			/*uint32_t numEvents = 0;
			event = *a_inputEvents;
			while (event)
			{
				DBG
				(
					"Event #{}: {} ({:p}, 0x{:X}, type: {}, device: {}, pad: 0x{:X}).",
					numEvents + 1,
					event->AsIDEvent() ? event->AsIDEvent()->QUserEvent() : "NONE",
					fmt::ptr(event),
					event->AsButtonEvent() ? event->AsButtonEvent()->idCode : 0xFF,
					event->GetEventType(),
					*event->device,
					event->AsIDEvent() ? event->AsIDEvent()->pad24 : 0x0
				);
				event = event->next;
				++numEvents;
			}*/

			// Filter out P1 inputs, gamepad or otherwise, that should be ignored 
			// by this menu event handler while in co-op.
			// Restore any input events that should still be handled by subsequent handlers.
			auto eventsToRestore = FilterInputEvents(a_inputEvents);

			// Process the all the events and save the result to return 
			// after restoring the original event data for any events
			// that still should be propagated in their original form.
			auto result = _ProcessEvent(a_this, a_inputEvents, a_eventSource);
			if (!eventsToRestore.empty())
			{
				for (const auto& event : eventsToRestore)
				{
					if (!event)
					{
						continue;
					}

					const auto iter = originalEventDataMap.find(event); 
					if (iter != originalEventDataMap.end())
					{
						RestoreInputEventType(event);
						const auto& data = iter->second;
						auto buttonEvent = event->AsButtonEvent();
						if (buttonEvent)
						{
							buttonEvent->device = data->device;
							buttonEvent->eventType = data->eventType;
							buttonEvent->userEvent = data->userEvent;
							buttonEvent->idCode = data->idCode;
							buttonEvent->heldDownSecs = data->heldDownSecs;
							buttonEvent->value = data->value;
						}
						else
						{
							auto idEvent = event->AsIDEvent();
							if (idEvent)
							{
								idEvent->userEvent = data->userEvent;
							}

							event->device = data->device;
							event->eventType = data->eventType;
						}
					}
				}
			}
			
			if (shouldReset)
			{
				// REMOVE when done debugging.
				DBG
				(
					"Buttons released. Reset flags to false. "
					"Summoning menu triggered: {}, debug menu triggered: {}. "
					"Hold times: {}, {}. Single event: {}, is button event: {}, is pressed: {}.",
					summoningMenuBindPressed, 
					debugMenuBindPressed, 
					pauseBindHeldTime,
					waitBindHeldTime,
					(*a_inputEvents)->QUserEvent(),
					(bool)((*a_inputEvents)->AsButtonEvent()),
					((*a_inputEvents)->AsButtonEvent()) ? 
					((*a_inputEvents)->AsButtonEvent()->IsPressed()) :
					false
				);
				summoningMenuBindPressed = 
				pauseAndWaitWerePressed =
				debugMenuBindPressed = false;
				pauseBindHeldTime =
				waitBindHeldTime = -1.0f;
			}

			return result;
		}

		void MenuControlsHooks::BlockInputEvent(RE::InputEvent* a_event)
		{
			// Block the given input event from being processed.
			// Get chained event's event sub-types.

			if (!a_event)
			{
				return;
			}
			
			// Reset event flag (used to block an analog stick event) 
			// which carries over once set.
			if (*a_event->eventType > RE::INPUT_EVENT_TYPE::kKinect)
			{
				a_event->eventType = static_cast<RE::INPUT_EVENT_TYPE>
				(
					!(*a_event->eventType) - !RE::INPUT_EVENT_TYPE::kKinect + 1
				);
			}

			auto idEvent = a_event->AsIDEvent();
			if (!idEvent)
			{
				return;
			}

			auto buttonEvent = a_event->AsButtonEvent();
			if (buttonEvent)
			{
				// DBG("Block button event {}.", buttonEvent->userEvent);
				buttonEvent->idCode = 0xFF;
				buttonEvent->heldDownSecs = 0.0f;
				buttonEvent->value = 0.0f;
				idEvent->userEvent = "ALYSLC_BLOCKED";
			}
			else
			{
				/*DBG
				(
					"Block input event of type {}: {}.", 
					*a_event->eventType, a_event->QUserEvent()
				);*/
				// JANK ALERT:
				// Must also set an invalid event type flag
				// to stop analog stick events from being processed by action handlers.
				a_event->eventType.set
				(
					static_cast<RE::INPUT_EVENT_TYPE>(!RE::INPUT_EVENT_TYPE::kKinect + 1)
				);
				idEvent->userEvent = "ALYSLC_BLOCKED";
			}
		}

		bool MenuControlsHooks::CheckForMenuTriggeringInput
		(
			RE::InputEvent* a_inputEvent,
			bool& a_newEventChainedOut
		)
		{
			// Check if P1 is trying to open the Summoning/Debug menus 
			// when the co-op camera is not active.
			// Store whether or not an additional input event was chained to trigger a menu
			// in the outparam.
			// Return true if the event should be blocked.

			// NOTE:
			// Delay Pause and Wait binds to trigger their actions on release, 
			// rather than on press, when P1's managers are inactive.
			// Pause + Wait -> Co-op Debug Menu
			// Wait + Pause -> Co-op Summoning Menu binds.

			// Must have a valid event; do not invalidate otherwise.
			if (!a_inputEvent)
			{
				return false;
			}

			auto ui = RE::UI::GetSingleton();
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			const auto& device = a_inputEvent->GetDevice();
			const bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
			// Ignore any events that are not button events,
			// or button events not from the gamepad/keyboard
			// or sent as emulated input by a co-op player.
			if ((!buttonEvent) || 
				(
					device != RE::INPUT_DEVICES::kGamepad && 
					device != RE::INPUT_DEVICES::kKeyboard
				))
			{
				return false;
			}

			auto ue = RE::UserEvents::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			if (!ue || !controlMap)
			{
				return false;
			}
			
			//===============================
			// [Pause/Wait Bind Event Check]:
			// ==============================

			bool isGamepadEvent = device == RE::INPUT_DEVICES::kGamepad;
			// For controllers:
			// Debug: always Pause + Wait when not in co-op.
			// Summon: always Wait + Pause when not in co-op.
			uint32_t pauseMask = 0xFF;
			(
				isGamepadEvent ? GAMEPAD_MASK_START : RE::BSKeyboardDevice::Keys::kEscape
			);
			uint32_t waitMask = 0xFF;
			if (ue && controlMap)
			{
				pauseMask = controlMap->GetMappedKey
				(
					ue->pause, device
				);
				waitMask = controlMap->GetMappedKey
				(
					ue->wait, device
				);
			}

			// Sometimes the associated user event is 'Journal' instead of 'Pause'.
			if (pauseMask == 0xFF) 
			{
				pauseMask = controlMap->GetMappedKey
				(
					ue->journal, device
				);
			}

			// Ensure both masks are valid, despite failing to get mapped ID code.
			// Both masks are sometimes the same here, 
			// so ensure they aren't by falling back to the default masks.
			if (pauseMask == 0xFF || pauseMask == waitMask) 
			{
				pauseMask = 
				(
					isGamepadEvent ? GAMEPAD_MASK_START : RE::BSKeyboardDevice::Keys::kEscape
				);
			}

			if (waitMask == 0xFF || pauseMask == waitMask) 
			{
				waitMask = 
				(
					isGamepadEvent ? GAMEPAD_MASK_BACK : RE::BSKeyboardDevice::Keys::kT	
				);
			}
			
			bool pauseBindEvent = buttonEvent->idCode == pauseMask;
			bool waitBindEvent = buttonEvent->idCode == waitMask;
			// Only handling pause or wait bind events.
			if (!pauseBindEvent && !waitBindEvent)
			{
				return false;
			}
			
			//=========================
			// [Emulated Input Checks]:
			// ========================
			
			// Skip potentially triggering ALYSLC menus here if sent by a companion player.
			// Only delay the original bind function until release.
			bool emulatedKeyInput = 
			(
				((buttonEvent->pad24 & 0xFFFF) == 0xCA11) || 
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA)
			);
			if (emulatedKeyInput)
			{
				// No need to only allow through on release if other menus are open,
				// since some modded hotkeys require holding down the Wait/Pause buttons 
				// to trigger events in the open menus.
				if (!onlyAlwaysOpen)
				{
					return false;
				}

				// ButtonEventPressType::kInstantTrigger events are sent with value of 2.0
				// to differentiate them with regular 'IsDown' events, which have value 1.0.
				if (((buttonEvent->IsHeld()) || 
					(buttonEvent->IsDown() && buttonEvent->value == 1.0f)))
				{
					DBG("EMU: event {}, down/held. Blocking.", buttonEvent->userEvent);
					return true;
				}
				else
				{
					DBG
					(
						"EMU: event {}, {}. Allow.",
						buttonEvent->userEvent, 
						buttonEvent->value == 0.0f ? 
						"up" : 
						buttonEvent->value <= 1.0f ?
						"down" : 
						"instant trigger"
					);

					// Up/Instant trigger.
					if (buttonEvent->value == 0.0f || buttonEvent->value == 2.0f)
					{
						// Change to pressed (down) event.
						buttonEvent->heldDownSecs = 0.0f;
						buttonEvent->value = 1.0f;

						float releaseTime = pauseBindEvent ? pauseBindHeldTime : waitBindHeldTime;
						if (releaseTime <= 0.0f)
						{
							releaseTime = 1.0f;
						}

						// NOTE:
						// Necessary to also pair with a button-released event,
						// otherwise the event may not trigger the desired effect here 
						// or later when pressing and releasing the bind again.
						RE::InputEvent* buttonEvent2 = 
						(
							RE::ButtonEvent::Create
							(
								*buttonEvent->device, 
								buttonEvent->userEvent, 
								buttonEvent->idCode, 
								0.0f, 
								releaseTime
							)
						);
						
						// Insert after the current event.
						buttonEvent2->next = buttonEvent->next;
						buttonEvent->next = buttonEvent2;
						a_newEventChainedOut = true;
					}

					return false;
				}
			}
			
			//====================================
			// [Update Held Time And Press State]:
			// ===================================
			
			// Update held time and state first.
			if (pauseBindEvent)
			{
				pauseBindHeldTime = buttonEvent->heldDownSecs;
			}
			else if (waitBindEvent)
			{
				waitBindHeldTime = buttonEvent->heldDownSecs;
			}

			if (pauseBindHeldTime != -1.0f && waitBindHeldTime != -1.0f)
			{
				pauseAndWaitWerePressed = true;
			}
			
			//=========================
			// [Co-op P1 Input Checks]:
			// ========================
			
			// Do not trigger menus if P1's managers are active and this is a gamepad event.
			// Allow through without blocking since we've updated held time and state flags above.
			bool p1ManagersActive =
			(
				(glob.coopSessionActive && glob.cam->IsRunning()) &&
				(glob.coopPlayers[0]->IsRunning() || !ui  || ui->GameIsPaused())
			);
			// Will not trigger any menus or perform its original function
			// if sent by P2 in hybrid mode (P1's controller).
			// Allow the held time and state flags to update to ensure a smooth transition of state
			// if the co-op session ends while the Pause/Wait binds are still pressed.
			// But block the event regardless of press state.
			bool isHybridModeControllerInput = 
			(
				glob.hybridModeActive &&
				glob.coopSessionActive &&
				isGamepadEvent &&
				!emulatedKeyInput
			);
			if ((p1ManagersActive && device == RE::INPUT_DEVICES::kGamepad) || 
				(isHybridModeControllerInput))
			{
				if (buttonEvent->IsUp())
				{
					// Reset hold times on release.
					if (pauseBindEvent)
					{
						pauseBindHeldTime = -1.0f;
					}

					if (waitBindEvent)
					{
						waitBindHeldTime = -1.0f;
					}
				}

				if (isHybridModeControllerInput)
				{
					DBG
					(
						"HYBRID: event {}, val: {}, held time: {}. Blocking.", 
						buttonEvent->userEvent,
						buttonEvent->value,
						buttonEvent->heldDownSecs
					);
					// Block. Block. Block.
					return true;
				}
				else
				{
					DBG
					(
						"CO-OP P1 Controller: managers active, "
						"skip processing button event {} (allow through). "
						"Is emulated input: {}.",
						a_inputEvent->QUserEvent(),
						emulatedKeyInput
					);
					// Allow through without performing Debug/Summoning menu checks.
					return false;
				}
			}
			
			//=================================
			// [Debug/Summoning Menu Handling]:
			// ================================
			
			// Should this input be blocked and not processed by the MenuControls ProcessMessage()
			// hook? Check if button release should open the Debug/Summoning Menu.
			bool blockEvent = false;
			// Both held and one just released.
			if (pauseBindHeldTime != -1.0f && 
				waitBindHeldTime != -1.0f && 
				buttonEvent->IsUp())
			{
				DBG
				(
					"Pause/wait binds held for {}s, {}s and {} released.", 
					pauseBindHeldTime, 
					waitBindHeldTime,
					pauseBindEvent ? "Pause" : "Wait"
				);
				// Button events seem to be chained in a manner 
				// that does not depend on when their buttons were pressed.
				// Check for buttons/keys being pressed/held 
				// in any order to trigger co-op debug/summoning menus.
				bool isDebugMenuBind = 
				(
					pauseBindHeldTime >= waitBindHeldTime
				);
				bool isSummoningMenuBind = 
				(
					pauseBindHeldTime < waitBindHeldTime
				);
				// Check if either menu is triggerable,
				// but do not attempt to open either just yet.
				// NOTE: 
				// The keyboard + mouse can always trigger either menu,
				// aside from the Summoning Menu when outside of co-op.
				// P1's controller will not trigger either menu here when in co-op.
				// Triggering them here would attempt to open each menu twice,
				// since the co-op bind also opens the menu.
				bool shouldTriggerDebugMenu = 
				(
					(!isHybridModeControllerInput) &&
					((!isGamepadEvent) || (!p1ManagersActive && !glob.coopSessionActive)) &&
					(isDebugMenuBind && !debugMenuBindPressed)
				);
				bool shouldTriggerSummoningMenu = 
				(
					(!isHybridModeControllerInput) &&
					((!isGamepadEvent) || (!p1ManagersActive && !glob.coopSessionActive)) &&
					(isSummoningMenuBind && !summoningMenuBindPressed)
				);

				DBG
				(
					"Hybrid controller input: {}, gamepad event: {}, P1 managers active: {}, "
					"co-op session active: {}, debug/summoning menu bind: {}, {}, pressed: {}, {}.",
					isHybridModeControllerInput,
					isGamepadEvent,
					p1ManagersActive,
					glob.coopSessionActive,
					isDebugMenuBind, 
					isSummoningMenuBind,
					debugMenuBindPressed,
					summoningMenuBindPressed
				);

				// Cannot trigger summoning menu with the keyboard to start co-op.
				if (!glob.coopSessionActive && 
					!isGamepadEvent && 
					isSummoningMenuBind)
				{
					DBG
					(
						"Cannot trigger Summoning Menu with keyboard outside of co-op."
					);
					RE::DebugMessageBox
					(
						"[ALYSLC]\nPlease use player 1's controller "
						"to open the Summoning Menu when outside of co-op."
					);
							
					// Reset hold time of released bind.
					if (pauseBindEvent)
					{
						pauseBindHeldTime = -1.0f;
					}
					else
					{
						waitBindHeldTime = -1.0f;
					}
						
					summoningMenuBindPressed = 
					debugMenuBindPressed = false;
					pauseBindHeldTime =
					waitBindHeldTime = -1.0f;
					return true;
				}

				// BEFORE sending events to open any menus.
				// Temp solution (not failproof), 
				// since I can't find a direct way of getting the XInput
				// controller index for the controller Skyrim recognizes as P1's.
				// NOTE: 
				// The BSPCGamepadDeviceDelegate's 'userIndex' member seems to 
				// always equal 0, even if the XInput-reported controller index 
				// for P1 is not 0, so we can't use that member to set P1's DID.
				// 
				// Check to see which controller is requesting to open either co-op menu
				// and assign its ID as P1's DID.
				// Heuristic checks the two buttons' event-reported held times 
				// against the XInput controller state held times.
				// Will sometimes fail if two players press the same binds 
				// at nearly the exact same time (within a couple frames),
				// as the wrong player's DID may be assigned as P1's DID.
				// Fix by manually assigning the P1 DID through the Debug Menu.
				auto devMgr = RE::BSInputDeviceManager::GetSingleton();
				auto gamepad = devMgr ? devMgr->GetGamepad() : nullptr;
				const auto pauseIter = glob.cdh->GAMEMASK_TO_INPUT_ACTION.find
				(
					pauseMask
				);
				const auto waitIter = glob.cdh->GAMEMASK_TO_INPUT_ACTION.find
				(
					waitMask
				);
				// If at least one of the bind masks are not accounted for, 
				// do not check for the DID below.
				// Right now, P1's DID is set to a CID unless in hybrid mode (1 controller).
				bool checkForP1DID = 
				(
					(glob.cdh && glob.player1DID == -1) && 
					(shouldTriggerSummoningMenu || shouldTriggerDebugMenu) &&
					(pauseIter != glob.cdh->GAMEMASK_TO_INPUT_ACTION.end()) &&
					(waitIter != glob.cdh->GAMEMASK_TO_INPUT_ACTION.end())
				);
				DBG
				(
					"Checking for P1 DID: debug menu: {}, summoning menu: {}. "
					"Reported gamepad user index: {}.",
					shouldTriggerDebugMenu, shouldTriggerSummoningMenu,
					gamepad ? gamepad->userIndex : -1337
				);
				if (checkForP1DID)
				{
					int32_t newDID = -1;
					if (glob.cdh->activeControllerCount > 1)
					{
						// Choose the controller with the smallest held time difference.
						float smallestHeldTimeDiffTotal = FLT_MAX;
						for (uint32_t i = 0; i < ALYSLC_MAX_CONTROLLER_COUNT; ++i)
						{
							XINPUT_STATE inputState{ };
							ZeroMemory(&inputState, sizeof(XINPUT_STATE));
							bool succ = 
							(
								XInputGetState(i, std::addressof(inputState)) == 
								ERROR_SUCCESS
							);
							if (!succ)
							{
								continue;
							}

							const auto& inputState1 = 
							(
								glob.cdh->GetInputState
								(
									i, 
									shouldTriggerDebugMenu ?
									pauseIter->second :
									waitIter->second
								)
							);
							const auto& inputState2 = 
							(
								glob.cdh->GetInputState
								(
									i, 
									shouldTriggerDebugMenu ? 
									waitIter->second : 
									pauseIter->second
								)
							);
							const auto& firstPressTP1 = 
							(
								glob.cdh->firstPressTPsList[i]
								[
									shouldTriggerDebugMenu ?
									!pauseIter->second :
									!waitIter->second
								]
							);
							const auto& firstPressTP2 = 
							(
								glob.cdh->firstPressTPsList[i]
								[
									shouldTriggerDebugMenu ?
									!waitIter->second :
									!pauseIter->second
								]
							);

							float heldTimeDiffTotal = FLT_MAX;
							if (shouldTriggerDebugMenu)
							{
								// If not held at all
								heldTimeDiffTotal = 
								(
									fabsf
									(
										pauseBindHeldTime -
										Util::GetElapsedSeconds(firstPressTP1)
									) + 
									fabsf
									(
										waitBindHeldTime -
										Util::GetElapsedSeconds(firstPressTP2)
									)
								);
							}
							else
							{
								heldTimeDiffTotal = 
								(
									fabsf
									(
										waitBindHeldTime - 
										Util::GetElapsedSeconds(firstPressTP1)
									) + 
									fabsf
									(
										pauseBindHeldTime - 
										Util::GetElapsedSeconds(firstPressTP2)
									)
								);
							}
								
							DBG
							(
								"DID {}'s diff total: {}. Current min diff total: {}. "
								"Last recorded input state held times: {}, {}. "
								"Time since last press: {}, {}. "
								"Pressed/just released: {}, {} / {}, {}.",
								i,
								heldTimeDiffTotal,
								smallestHeldTimeDiffTotal,
								inputState1.heldTimeSecs,
								inputState2.heldTimeSecs,
								Util::GetElapsedSeconds(firstPressTP1),
								Util::GetElapsedSeconds(firstPressTP2),
								inputState1.isPressed,
								inputState2.isPressed,
								inputState1.justReleased,
								inputState2.justReleased
							);
							if (heldTimeDiffTotal < smallestHeldTimeDiffTotal)
							{
								smallestHeldTimeDiffTotal = heldTimeDiffTotal;
								newDID = i;
								DBG
								(
									"P1 DID set to {}. Min diff total is now: {}.",
									newDID,
									smallestHeldTimeDiffTotal
								);
							}
						}

					}
					else
					{
						// First keyboard + mouse index if there's 
						// 0 or only 1 controller plugged in.
						newDID = ALYSLC_MAX_CONTROLLER_COUNT;
					}
							
					if (newDID != -1)
					{
						DBG("P1 DID set to {}.", newDID);
						glob.player1DID = newDID;
					}
					else
					{
						DBG("Did not assign P1 DID. Currently {}.", 
							glob.player1DID);
					}
				}

				// Only attempt to open the menu when no temporary menus are open.
				// Otherwise the menu will layer under the currently open menu,
				// which another player may be controlling, 
				// like ALYSLC's pre-summoning tips message box.
				// After performing P1 DID check,
				// we can now open either menu.
				if (shouldTriggerDebugMenu)
				{
					// Can trigger the debug menu when P1's managers are inactive 
					// or when opening it via keyboard.
					if ((glob.player1Actor && onlyAlwaysOpen) && 
						(!p1ManagersActive || !isGamepadEvent))
					{
						DBG
						(
							"Debug menu binds pressed but not triggered. "
							"Opening menu now."
						);
						glob.onDebugMenuRequest.SendEvent
						(
							glob.player1Actor.get(), 
							glob.coopSessionActive ? glob.player1DID : -1,
							0
						);
					}
					
					debugMenuBindPressed = true;
				}
							
				if (onlyAlwaysOpen && shouldTriggerSummoningMenu)
				{
					// NOTE: 
					// Have to wait until there are no downed players to open the Summoning Menu.
					// Summoning global variable is set to 1 
					// in the summoning menu script, 
					// and set to 0 if summoning failed or is complete.
					if (glob.summoningMenuOpenGlob->value == 0.0f)
					{
						bool preventedFromSummoning = false;
						if (glob.globalDataInit && glob.allPlayersInit)
						{
							for (const auto& p : glob.coopPlayers)
							{
								if (!p->isActive)
								{
									continue;
								}

								if (p->isDowned)
								{
									if (glob.coopSessionActive)
									{
										glob.moarm->InsertRequest
										(
											0, 
											InputAction::kCoopSummoningMenu, 
											SteadyClock::now(), 
											RE::MessageBoxMenu::MENU_NAME
										);
									}

									RE::DebugMessageBox
									(
										"[ALYSLC]\n"
										"Cannot summon players while a player is downed!"
									);
									preventedFromSummoning = true;

									break;
								}
							}
						}
						
						if ((!preventedFromSummoning && onlyAlwaysOpen) && 
							(!p1ManagersActive || !isGamepadEvent))
						{
							DBG
							(
								"Summoning menu binds pressed but not triggered. "
								"Opening menu now."
							);
							glob.onSummoningMenuRequest.SendEvent();
						}
					}
					else
					{
						RE::DebugMessageBox
						(
							"[ALYSLC]\n"
							"The summoning process is still active. "
							"Please wait a few seconds before attempting "
							"to summon other players."
						);
					}
					
					summoningMenuBindPressed = true;
				}

				// Reset hold time of released bind.
				if (pauseBindEvent)
				{
					pauseBindHeldTime = -1.0f;
				}
				else
				{
					waitBindHeldTime = -1.0f;
				}

				// After processing, ignore the button event entirely, 
				// since we do not want either the pause or wait menus to trigger 
				// once both the pause and wait binds are pressed at the same time.
				blockEvent = true;
			}
			else if (buttonEvent->IsDown() || buttonEvent->IsHeld())
			{
				if (buttonEvent->IsDown())
				{
					DBG
					(
						"{} pressed/held on its own. Blocking.", buttonEvent->userEvent
					);	
				}

				// Clear out first button event to prevent it 
				// from triggering the Pause or Wait menus
				// while the button is still pressed.
				blockEvent = true;
			}
			else if (buttonEvent->IsUp())
			{
				// No co-op menus triggered and both binds were pressed and held previously.
				// Also, either no menus are open, P1 is in control,
				// the input is from the keyboard when not in hybrid mode,
				// or the P1 override key is pressed.
				// Do not want to trigger the Pause/Wait menu 
				// after already opening either the summoning or debug menus.
				bool allowThrough = 
				(
					(
						!isHybridModeControllerInput &&
						!summoningMenuBindPressed && 
						!debugMenuBindPressed && 
						!pauseAndWaitWerePressed
					) &&
					(
						(onlyAlwaysOpen || glob.menuPID <= 0) ||
						(
							(
								(!isGamepadEvent) &&
								(
									(glob.cam->IsRunning()) || 
									(!glob.cam->waitForToggle && !glob.hybridModeActive)
								)
							) ||
							(Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY))
						)
					)	
				);
				DBG
				(
					"Is NOT hybrid mode controller input: {}, "
					"summoning/debug menu bind NOT pressed: {}, {}, "
					"pause and wait NOT pressed: {}, "
					"emulated input: {}, "
					"menus always open: {}, menu PID: {}, "
					"keyboard input and cam active with 2+ players: {}, "
					"override key pressed: {}.",
					!isHybridModeControllerInput,
					!summoningMenuBindPressed,
					!debugMenuBindPressed,
					!pauseAndWaitWerePressed,
					emulatedKeyInput,
					onlyAlwaysOpen,
					glob.menuPID,
					(!isGamepadEvent) &&
					(
						(glob.cam->IsRunning()) || 
						(!glob.cam->waitForToggle && !glob.hybridModeActive)
					),
					Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY)
				);
				if (allowThrough)
				{
					DBG
					(
						"{} bind released on its own. Event name: {}. Allow through.",
						pauseBindEvent ? "Pause" : "Wait",
						buttonEvent->userEvent
					);

					// Change to pressed (down) event.
					buttonEvent->heldDownSecs = 0.0f;
					buttonEvent->value = 1.0f;

					float releaseTime = pauseBindEvent ? pauseBindHeldTime : waitBindHeldTime;
					if (releaseTime <= 0.0f)
					{
						releaseTime = 1.0f;
					}

					// NOTE:
					// Necessary to also pair with a button-released event,
					// otherwise the event may not trigger the desired effect here 
					// or later when pressing and releasing the bind again.
					RE::InputEvent* buttonEvent2 = 
					(
						RE::ButtonEvent::Create
						(
							*buttonEvent->device, 
							buttonEvent->userEvent, 
							buttonEvent->idCode, 
							0.0f, 
							releaseTime
						)
					);
						
					// Insert after the current event.
					buttonEvent2->next = buttonEvent->next;
					buttonEvent->next = buttonEvent2;
					a_newEventChainedOut = true;
				}
				else
				{
					// A co-op menu was triggered, 
					// so ignore the button event on release.
					DBG
					(
						"{} bind released on its own. Event name: {}. Ignoring.",
						pauseBindEvent ? "Pause" : "Wait", 
						buttonEvent->userEvent
					);
						
					blockEvent = true;
				}

				// Reset hold times on release.
				if (pauseBindEvent)
				{
					pauseBindHeldTime = -1.0f;
				}

				if (waitBindEvent)
				{
					waitBindHeldTime = -1.0f;
				}
			}

			return blockEvent;
		}
		
		bool MenuControlsHooks::CheckForP1DialogueControlInput(RE::InputEvent* a_inputEvent)
		{
			// Check if P1 is requesting control of dialogue
			// or is transferring control to another player.
			// Return true if the event should be blocked.

			if (!a_inputEvent)
			{
				return false;
			}

			auto ui = RE::UI::GetSingleton();
			// Only need to handle P1 inputs when in a co-op session with the Dialogue Menu open
			// and if not using the co-op binds.
			if (!ui || 
				!glob.globalDataInit || 
				!glob.allPlayersInit || 
				!glob.coopSessionActive || 
				!ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || 
				glob.coopPlayers[0]->IsRunning())
			{
				return false;
			}
			
			auto userEvents = RE::UserEvents::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			if (!a_inputEvent->AsButtonEvent())
			{
				return false;
			}
				
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			// Do not do anything while the button/key is held down.
			if (buttonEvent->value == 1.0f)
			{
				return false;
			}

			bool isGamepad = buttonEvent->GetDevice() == RE::INPUT_DEVICES::kGamepad;
			// Start/Journal bind for controllers, F1 for the keyboard.
			auto inputMask = 
			(
				isGamepad ? GAMEPAD_MASK_START : RE::BSKeyboardDevice::Keys::Key::kF1
			);
			if (isGamepad && userEvents && controlMap)
			{
				inputMask = controlMap->GetMappedKey
				(
					userEvents->pause, RE::INPUT_DEVICE::kGamepad
				);
			}

			// Sometimes the associated user event is 'Journal' instead of 'Pause'.
			if (isGamepad && inputMask == 0xFF) 
			{
				inputMask = controlMap->GetMappedKey
				(
					userEvents->journal, RE::INPUT_DEVICE::kGamepad
				);
				// Ensure the mask is valid, despite failing to get mapped button ID code.
				if (isGamepad && inputMask == 0xFF) 
				{
					inputMask = GAMEPAD_MASK_START;
				}
			}

			if (buttonEvent->idCode != inputMask)
			{
				return false;
			}

			DBG("Perform dialogue control switch or request.");
			// Perform dialgoue control switch on button/key release.
			StartFuncs::ChangeDialoguePlayer(glob.coopPlayers[0]);
			return true;
		}

		bool MenuControlsHooks::CheckForP1FavoritesMenuInput(RE::InputEvent* a_inputEvent)
		{
			// 1. Check if P1 is in the Favorites Menu and is trying to hotkey an entry
			// and update its hotkey state accordingly.
			// 2. Check if P1 is in the Favorites Menu and is trying to equip 
			// a quick slot spell/item and (un)equip this item as needed.
			// 3. Check if P1 is in the Favorites Menu and toggle SMORF state if needed.
			// Return true if the event should be blocked.

			// Check if P1 is trying to hotkey a FavoritesMenu entry.
			// Return true if the event triggered a hotkey change and should be invalidated.

			// Must have a valid gamepad event and have initialized all players.
			bool blockInput = false;
			if (!a_inputEvent)
			{
				return blockInput;
			}

			// Should also not be in hybrid mode, since P2 is effectively using P1's controller
			// and should not perform any of the above functions for P1.
			// Do not invalidate otherwise.
			if (!glob.allPlayersInit || glob.hybridModeActive)
			{
				return blockInput;
			}

			auto ue = RE::UserEvents::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			// Must have a valid input event, access to the UI and user events singletons,
			// and be displaying the Favorites Menu.
			if (!ue || !ui || !ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || !controlMap)
			{
				return blockInput;
			}

			if (!a_inputEvent->AsButtonEvent())
			{
				return blockInput;
			}

			auto idEvent = a_inputEvent->AsIDEvent();
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			// Only handle button events with an ID.
			if (!idEvent || !buttonEvent)
			{
				return blockInput;
			}

			//======================//
			// Quick slot equip check:
			//======================//

			// Temp hacky workaround to override entry text changes without a hook:
			// Update here since the previous changes are wiped shortly after opening the menu, 
			// or when P1 has equipped something else.
			// We have to re-apply those changes 
			// through the FavoritesMenu::ProcessEvent() hook.
			// Send a menu update request to update the quick slot tags
			// when P1 releases any input, since the equip state update occurs on press.
			if (buttonEvent->value == 0.0f && buttonEvent->heldDownSecs > 0.0f)
			{
				if (auto msgQ = RE::UIMessageQueue::GetSingleton(); msgQ)
				{
					msgQ->AddMessage
					(
						RE::FavoritesMenu::MENU_NAME, 
						RE::UI_MESSAGE_TYPE::kUpdate,
						nullptr
					);
				}
			}

			// Only handle pause/journal bind presses and only if just pressed.
			bool releasedPauseBind = 
			{
				(buttonEvent->value == 0.0f && buttonEvent->heldDownSecs > 0.0f) &&
				(
					(
						buttonEvent->idCode == 
						controlMap->GetMappedKey(ue->pause, RE::INPUT_DEVICE::kGamepad)
					) ||
					(
						buttonEvent->idCode == 
						controlMap->GetMappedKey(ue->journal, RE::INPUT_DEVICE::kGamepad)
					)
				)
			};
			if (releasedPauseBind)
			{
				glob.mim->EquipP1QSForm();
			}

			// To hotkey an entry,
			// P1 must be clicking in the RS and it must be displaced from center.
			bool isRThumbPressedAndRSMoved = 
			{
				buttonEvent->idCode == GAMEPAD_MASK_RIGHT_THUMB &&
				glob.cdh->GetAnalogStickState
				(
					glob.coopPlayers[0]->deviceID, false
				).normMag > 0.0f
			};
			if (isRThumbPressedAndRSMoved)
			{
				// Set on release, preview on hold.
				glob.mim->HotkeyFavoritedForm(buttonEvent->value == 0.0f);
				blockInput = true;
			}
			
			// Toggle SMORF if just pressed.
			auto taskInterface = SKSE::GetTaskInterface();
			const auto iter = glob.cdh->GAMEMASK_TO_XIMASK.find(buttonEvent->idCode);
			bool shouldToggleSMORF = 
			(
				(taskInterface) &&
				(buttonEvent->value == 1.0f && buttonEvent->heldDownSecs == 0.0f) &&
				(
					iter != glob.cdh->GAMEMASK_TO_XIMASK.end() &&
					iter->second == XINPUT_GAMEPAD_X
				)
			);
			if (shouldToggleSMORF)
			{
				blockInput = true;
				taskInterface->AddUITask
				(
					[]() 
					{
						auto ui = RE::UI::GetSingleton(); 
						if (!ui)
						{
							return;
						}

						auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
						if (!favoritesMenu)
						{
							return;
						}

						auto view = favoritesMenu->uiMovie; 
						if (!view)
						{
							return;
						}

						RE::GFxValue selectedIndex;
						view->GetVariable
						(
							std::addressof(selectedIndex),
							"_root.MenuHolder.Menu_mc.itemList.selectedEntry.index"
						);
						// Index in favorites list.
						uint32_t index = static_cast<uint32_t>(selectedIndex.GetNumber());
						if (index >= favoritesMenu->favorites.size())
						{
							return;
						}

						auto form = favoritesMenu->favorites[index].item;
						if (!form)
						{
							return;
						}

						if (form->formID == 0x64B33)
						{
							if (glob.menuPID != -1 && glob.menuPID == 0)
							{
								const auto& coopP1 = glob.coopPlayers[0];
								coopP1->tm->canSMORF = !coopP1->tm->canSMORF;
								if (coopP1->tm->canSMORF)
								{
									RE::DebugMessageBox
									(
										"A latent power suddenly compels you. Propels you?"
									);
								}
								else
								{
									RE::DebugMessageBox
									(
										"The power ebbs away and you feel grounded again."
									);
								}
						
							}
						}
					}
				);
			}

			return blockInput;
		}

		bool MenuControlsHooks::CheckForP1KeyboardTeleportReq(RE::InputEvent* a_inputEvent)
		{
			// Check if P1 is requesting to teleport to another player
			// and teleport to the closest player in the direction of P1's crosshair ray.
			// Return true if the event should be blocked.

			if (!a_inputEvent)
			{
				return false;
			}

			auto ui = RE::UI::GetSingleton();
			// Only need to handle P1 inputs when in a co-op session,
			// P1 is not controlling menus, and P1 is not using the co-op binds.
			bool shouldSkip = 
			(
				(
					!ui || 
					!glob.globalDataInit || 
					!glob.allPlayersInit || 
					!glob.coopSessionActive || 
					glob.coopPlayers[0]->IsRunning()
				) ||
				(!Util::MenusOnlyAlwaysOpen() && glob.menuPID == 0)
			);
			if (shouldSkip)
			{
				return false;
			}
			
			auto userEvents = RE::UserEvents::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			if (!a_inputEvent->AsButtonEvent())
			{
				return false;
			}
				
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			// Do not do anything while the button/key is held down.
			if (buttonEvent->value == 1.0f)
			{
				return false;
			}

			bool isGamepad = buttonEvent->GetDevice() == RE::INPUT_DEVICES::kGamepad;
			if (isGamepad)
			{
				return false;
			}

			// F1 to teleport.
			auto inputMask = RE::BSKeyboardDevice::Keys::Key::kF1;
			if (buttonEvent->idCode != inputMask)
			{
				return false;
			}

			DBG("Teleport to another player.");
			// Perform teleportation on button/key release.
			StartFuncs::TeleportToPlayer(glob.coopPlayers[0]);
			return true;
		}

		bool MenuControlsHooks::CheckForP1QuickSaveReq(RE::InputEvent* a_inputEvent)
		{
			// Check if P1 is trying to save the game via the Quicksave bind.
			// Block the event if a companion player is currently controlling menus 
			// with player data copied over to P1.
			// Return true if the event should be blocked.

			if (!a_inputEvent)
			{
				return false;
			}

			auto ue = RE::UserEvents::GetSingleton();
			if (!ue)
			{
				return false;
			}

			const auto eventName = a_inputEvent->QUserEvent();
			auto idEvent = a_inputEvent->AsIDEvent();
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			// Initial check to prevent quicksaving while co-op companion data is copied to P1.
			// Also recommended that the player disable auto-saving 
			// while using this mod to remove any chance of saving copied data onto P1, 
			// such as another player's name or race name.
			bool couldSaveWithCopiedData =
			(
				*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone
			);
			bool playerIsDowned = 
			(
				std::any_of
				(
					glob.coopPlayers.begin(),
					glob.coopPlayers.end(),
					[](const auto& a_p)
					{
						return a_p->isActive && a_p->isDowned;
					}
				)
			);
			// Could be triggered by keyboard button events as well.
			bool eventCanLeadToSave = 
			(
				(idEvent && buttonEvent) &&
				(couldSaveWithCopiedData || playerIsDowned) && 
				(eventName == ue->quicksave)
			);
			if (eventCanLeadToSave)
			{
				auto ui = RE::UI::GetSingleton();
				if (ui && !ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) && buttonEvent->IsDown())
				{
					if (couldSaveWithCopiedData)
					{
						RE::DebugMessageBox
						(
							"[ALYSLC]\nCannot save while another player's data "
							"is copied over to P1.\n"
							"Please ensure all menus are closed or reload an older save."
						);
					}
					else
					{
						RE::DebugMessageBox
						(
							"[ALYSLC]\nCannot quicksave while another player is downed!"
						);
					}
				}

				// Block while the bind is held.
				return true;

				/*idEvent->userEvent = "ALYSLC_BLOCKED";
				buttonEvent->idCode = 0xFF;
				buttonEvent->heldDownSecs = 0.0f;
				buttonEvent->value = 0.0f;
				if (*inputEvent->eventType > RE::INPUT_EVENT_TYPE::kKinect)
				{
					inputEvent->eventType = static_cast<RE::INPUT_EVENT_TYPE>
					(
						!(*inputEvent->eventType) - !RE::INPUT_EVENT_TYPE::kKinect + 1
					);
				}*/
			}

			return false;
		}

		bool MenuControlsHooks::CheckForP1ReviveReq(RE::InputEvent* a_inputEvent)
		{
			// Check if P1 is trying to revive another player while the co-op camera is inactive
			// and revive the other player if so.
			// Can revive with the 'Activate' input event from either keyboard or controller.
			// Return true if the event should NOT be processed by the MenuControls hook.
			
			if (!glob.globalDataInit || 
				!glob.allPlayersInit ||
				!glob.coopSessionActive ||
				glob.player1DID == -1 ||
				!glob.coopPlayers[0] ||
				glob.cam->IsRunning())
			{
				return false;
			}

			const auto ui = RE::UI::GetSingleton();
			const auto ue = RE::UserEvents::GetSingleton();
			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!ui || !ue || !p1)
			{
				return false;
			}
			
			const auto& coopP1 = glob.coopPlayers[0];
			// Can't revive another player if downed.
			if (coopP1->isDowned)
			{
				return false;
			}

			auto idEvent = a_inputEvent->AsIDEvent();
			auto buttonEvent = a_inputEvent->AsButtonEvent();
			bool activateToRevive = 
			(
				(buttonEvent && idEvent && idEvent->userEvent == ue->activate) &&
				(
					!glob.hybridModeActive || 
					a_inputEvent->GetDevice() != RE::INPUT_DEVICE::kGamepad
				)
			);
			if (activateToRevive)
			{
				auto pickData = RE::CrosshairPickData::GetSingleton();
				if (pickData)
				{
					auto pIndex = GlobalCoopData::GetCoopPlayerIndex(pickData->targetActor);
					if (pIndex != -1 || coopP1->isRevivingPlayer)
					{
						DBG
						(
							"Activate event: {}, {}s. Pick target: {}.",
							buttonEvent->value,
							buttonEvent->heldDownSecs,
							Util::HandleIsValid(pickData->targetActor) ? 
							pickData->targetActor.get()->GetName() : 
							"NONE"
						);
						coopP1->pam->RevivePlayerP1NoCoopCam
						(
							pIndex,
							buttonEvent->value,
							buttonEvent->heldDownSecs
						);
						// Block this input while reviving.
						return true;
					}
				}

				// Failsafe:
				// Make sure P1's don't move flag is unset on 'Activate' press and release.
				if (buttonEvent->value == 0.0f || buttonEvent->heldDownSecs == 0.0f)
				{
					Util::NativeFunctions::SetDontMove(p1, false);
				}
			}

			return false;
		}

		std::vector<RE::InputEvent*> MenuControlsHooks::FilterInputEvents
		(
			RE::InputEvent** a_inputEvents
		)
		{
			// Check which player sent the input events in the input event chain,
			// and modify individual input events in the chain to block them 
			// from being handled by P1's action handlers, as needed.
			// Return true if the event should be processed by the MenuControls hook.
			// 
			// NOTE: 
			// This function is messy even compared to the rest of the project, I know.
			// 
			// IMPORTANT:
			// InputEvent's 'pad24' member is used to store processing info:
			// 0xC0DAXXXX:	event was already filtered and handled here.
			// 0xXXXXC0DA:	proxied P1 input sent by this plugin 
			// and should be allowed through by this function.
			// 0xXXXXCA11:	emulated P1 input sent by another player from the MIM.
			// 0xXXXXDEAD:	ignore this input event.
			
			// Return a list of all blocked events that should be propagated 
			// in their original states to all following input handlers 
			// registered to receive input events.
			// Allows for certain events to skip menu context processing
			// and still affect P1's character.
			// Ex. Moving the left stick while in the Favorites Menu 
			// will not change the selected favorites entry, 
			// but will allow P1 to move while another player is controlling menus,
			// since the event is forwarded unmodified to P1's MovementHandler.
		
			// List of events that only the menu controls handler should not process.
			std::vector<RE::InputEvent*> eventsToRestore{ };

			const auto ui = RE::UI::GetSingleton();
			const auto ue = RE::UserEvents::GetSingleton();
			auto p1 = RE::PlayerCharacter::GetSingleton();
			// NOTE:
			// Does not have to be a gamepad event here.
			auto inputEvent = *a_inputEvents;
			auto idEvent = inputEvent->AsIDEvent();
			auto buttonEvent = inputEvent->AsButtonEvent();
			auto thumbstickEvent = 
			(
				inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ? 
				static_cast<RE::ThumbstickEvent*>(inputEvent) : 
				nullptr
			);
			
			// No temporary menus open.
			bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
			// Non-P1 player controlling menus.
			bool companionPlayerControllingMenus = 
			(
				glob.globalDataInit && !onlyAlwaysOpen && glob.mim->managerMenuPID != -1
			);
			// P1 controlling menus.
			bool p1ControllingMenus = 
			(
				(!onlyAlwaysOpen) && (glob.menuPID <= 0 || !companionPlayerControllingMenus)
			);
			// P1's co-op managers are active.
			bool p1ManagersActive = 
			(
				(glob.coopSessionActive) && 
				(glob.cam->IsRunning() || glob.coopPlayers[0]->IsRunning())
			);
			if (!ui || !ue || !p1)
			{
				// Prior to returning as processable,
				// restore the original event type flag which may have been modified before 
				// when preventing analog stick inputs from propagating.
				RestoreInputEventType(inputEvent);
				return eventsToRestore;
			}

			// Should this MenuControls handler process the current input event?
			bool shouldProcess = true;
			auto controlMap = RE::ControlMap::GetSingleton(); 
			bool dialogueMenuOpen = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			bool lootMenuOpen = ui->IsMenuOpen(GlobalCoopData::LOOT_MENU);
			RE::InputEvent* prevInputEvent = nullptr;
			RE::BSFixedString eventName{ "" };
			while (inputEvent)
			{
				const auto& eventName = inputEvent->QUserEvent();
				// Reset blocked event flag which seems to carry over once set.
				RestoreInputEventType(inputEvent);

				// Get chained event sub-types.
				idEvent = inputEvent->AsIDEvent();
				// Skip events with no ID.
				if (!idEvent)
				{
					prevInputEvent = inputEvent;
					inputEvent = inputEvent->next;
					continue;
				}
				
				// Clear pad24 first because the pad flag sticks around for reused id events.
				// NOTE: 
				// Top 2 bytes == 0xC0DA means this event has been processed before.
				if (idEvent->pad24 >> 16 == 0xC0DA)
				{
					idEvent->pad24 = 0;
				}
				
				// Ignore this input if pad24 == 0xDEAD.
				bool ignoreInput = (idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xDEAD);
				if (ignoreInput)
				{
					prevInputEvent = inputEvent;
					inputEvent = inputEvent->next;
					continue;
				}

				// Check for P1-specific input events that require special handling 
				// before disabling the binds' original effects.
				bool newEventChained = false;
				bool shouldBlock = 
				(
					CheckForP1FavoritesMenuInput(inputEvent) ||
					CheckForP1DialogueControlInput(inputEvent) ||
					CheckForP1KeyboardTeleportReq(inputEvent) ||
					CheckForP1ReviveReq(inputEvent) ||
					CheckForP1QuickSaveReq(inputEvent) ||
					CheckForMenuTriggeringInput(inputEvent, newEventChained)
				);
				if (shouldBlock)
				{
					BlockInputEvent(inputEvent);
				}

				// Skip over the newly chained event.
				if (newEventChained && inputEvent->next)
				{
					prevInputEvent = inputEvent->next;
					inputEvent = inputEvent->next->next;
					continue;
				}

				// Input event was blocked before, so do not propagate or handle.
				// Can crash or freeze otherwise (looking for the crash/freeze source).
				bool wasBlocked = 
				(
					idEvent->userEvent == "ALYSLC_BLOCKED" && idEvent->idCode == 0xFF
				);
				if (wasBlocked)
				{
					prevInputEvent = inputEvent;
					inputEvent = inputEvent->next;
					continue;
				}
				
				// DBG("Not blocking {}.", idEvent->userEvent);
				// Has a bypass flag indicating that the event was sent by another player.
				// Co-op companion players: 0xCA11
				bool fromCompanionPlayer = 
				(
					(idEvent->pad24 & 0xFFFF) == 0xCA11
				);
				bool companionPlayerMenuInput = 
				(
					companionPlayerControllingMenus && fromCompanionPlayer
				);
				// Downcast.
				buttonEvent = inputEvent->AsButtonEvent();
				thumbstickEvent = 
				(
					inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ? 
					static_cast<RE::ThumbstickEvent*>(inputEvent) : 
					nullptr
				);
				
				// If a companion player is in control of the Lockpicking Menu,
				// allow the P1 input event to rotate the lock
				// while the other player rotates the pick.
				// If P1 is in control of the Lockpicking Menu 
				// and there are only two active players,
				// allow the P1 input even to rotate the pick,
				// while the other player rotates the lock.
				bool canTwoPlayerLockpick = 
				(
					Settings::bTwoPlayerLockpicking && glob.activePlayers == 2
				);
				bool allowP1RotateLock = 
				(
					(canTwoPlayerLockpick) && 
					(
						!fromCompanionPlayer && 
						companionPlayerControllingMenus && 
						idEvent->userEvent == "RotateLock"
					)
				);
				bool allowP2RotateLock = 
				(
					(canTwoPlayerLockpick) && 
					(
						(
							fromCompanionPlayer && 
							!companionPlayerControllingMenus && 
							idEvent->userEvent == "RotateLock"
						)
					)
				);
				bool isBlockedP1RotateLockInput = 
				(
					(canTwoPlayerLockpick) && 
					(
						(
							!fromCompanionPlayer && 
							!companionPlayerControllingMenus && 
							idEvent->userEvent == "RotateLock"
						)
					)
				);

				// [Hybrid Mode Or No Co-op Camera]
				// We want to ignore all keyboard inputs when P2 is controlling menus,
				// except for the 'Esc' key as a failsafe.
				// We also want to ignore all controller inputs that are not flagged as processable
				// when no menus are open or when P1 is controlling menus.
				if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop())
				{
					// P1 can override other players' menu control while holding down 'LCtrl'.
					bool p1Override = 
					(
						(Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY)) &&
						(
							!buttonEvent || 
							buttonEvent->idCode != GlobalCoopData::P1_OVERRIDE_KEY
						)
					);
					bool overrideSubstituteKeyPressed = 
					(
						companionPlayerControllingMenus &&
						buttonEvent &&
						buttonEvent->idCode == GlobalCoopData::P1_OVERRIDE_SUBSTITUTED_KEY &&
						buttonEvent->IsPressed()
					);
					bool overrideSubstituteKeyReleased = 
					(
						companionPlayerControllingMenus &&
						buttonEvent &&
						buttonEvent->idCode == GlobalCoopData::P1_OVERRIDE_SUBSTITUTED_KEY &&
						buttonEvent->IsUp()
					);
					// If the override key is not pressed, 
					// the override substitute key is not released,
					// P1 cannot rotate the lock (if the Lockpicking Menu is open),
					// and the event is not a gamepad event forced through with a bypass flag,
					// we block P1's keyboard and mouse input from affecting menus 
					// when another player is in control.
					bool ignoreP1Input = 
					(
						(
							!p1Override &&
							!allowP1RotateLock &&
							!overrideSubstituteKeyReleased
						) &&
						(
							(isBlockedP1RotateLockInput) ||
							(
								companionPlayerControllingMenus && 
								inputEvent->GetDevice() != RE::INPUT_DEVICE::kGamepad &&
								(idEvent->pad24 & 0xFFFF) != 0xC0DA
							)
						)
					);
					// In hybrid mode, since P2 is using a controller 
					// that controls P1 when the co-op camera is disabled,
					// we must prevent inputs from affecting P1's character.
					// Flagged as processable (0xC0DA) or sent by P2 (0xCA11).
					bool ignoreP2Input = 
					(
						(glob.hybridModeActive) &&
						(!ignoreP1Input && !allowP2RotateLock && !overrideSubstituteKeyReleased) && 
						(inputEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad) &&
						(
							(!companionPlayerControllingMenus) ||
							(
								(idEvent->pad24 & 0xFFFF) != 0xC0DA &&
								(idEvent->pad24 & 0xFFFF) != 0xCA11
							)
						)
					);
					// REMOVE when done debugging.
					DBG
					(
						"Event {} (id code 0x{:X}, device: {}). "
						"Ignore P1 input: {}, ignore P2 input: {}. "
						"Override substitute key released: {}.",
						idEvent->userEvent,
						buttonEvent ? buttonEvent->idCode : 0xFF,
						*inputEvent->device,
						ignoreP1Input,
						ignoreP2Input,
						overrideSubstituteKeyReleased
					);
					if (ignoreP1Input || ignoreP2Input)
					{
						// Block the event from being processed by the MenuControls handler,
						// but allow other handlers to process the event afterward.
						BlockInputEvent(inputEvent);
						if (glob.hybridModeActive && ignoreP2Input)
						{
							if (prevInputEvent)
							{
								prevInputEvent->next = inputEvent->next;
								inputEvent = prevInputEvent->next;
							}
							else
							{
								if (inputEvent->next)
								{
									*a_inputEvents = inputEvent->next;
									inputEvent = *a_inputEvents;
								}
								else
								{
									return eventsToRestore;
								}
							}
						
							// REMOVE when done debugging.
							/*DBG
							(
								"Prev event is now {}, current is {}.",
								prevInputEvent ? prevInputEvent->QUserEvent() : "NONE",
								inputEvent ? inputEvent->QUserEvent() : "NONE"
							);*/
						}
						else
						{
							if (ignoreP1Input && !overrideSubstituteKeyPressed)
							{
								eventsToRestore.emplace_back(inputEvent);
							}

							prevInputEvent = inputEvent;
							inputEvent = inputEvent->next;
						}

						continue;
					}
					else if (overrideSubstituteKeyReleased)
					{
						auto p1GameplayContextEvent = 
						(
							controlMap->GetUserEventName
							(
								GlobalCoopData::P1_OVERRIDE_KEY, RE::INPUT_DEVICE::kKeyboard
							)
						);
						// Process the override key instead.
						if (Hash(p1GameplayContextEvent) != ""_h)
						{
							idEvent->userEvent = p1GameplayContextEvent;
							buttonEvent->idCode = GlobalCoopData::P1_OVERRIDE_KEY;
							buttonEvent->value = 1.0f;
							buttonEvent->heldDownSecs = 0.0f;
						}

						prevInputEvent = inputEvent;
						inputEvent = inputEvent->next;
						continue;
					}
				}

				// Only process gamepad events below here.
				if (inputEvent->GetDevice() != RE::INPUT_DEVICE::kGamepad)
				{
					prevInputEvent = inputEvent;
					inputEvent = inputEvent->next;
					continue;
				}
				
				//======================================
				// Special QuickLoot menu compatibility.
				//======================================
				if (controlMap && lootMenuOpen && buttonEvent)
				{
					if (buttonEvent->heldDownSecs == 0.0f)
					{
						// Save "Ready Weapon" input, if any, 
						// to prepare for giving this player control 
						// when opening the selected container while in the QuickLoot menu.
						// IDK why the event name that QuickLoot uses 
						// to switch to the ContainerMenu here is an empty string,
						// but hey, check the ID code instead, 
						// since that still equals the ID code for the "Ready Weapon" bind.
						bool isCancelBind = 
						(
							buttonEvent->idCode == 
							controlMap->GetMappedKey
							(
								ue->cancel, 
								RE::INPUT_DEVICE::kGamepad,
								RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
							)	
						);
						bool isReadyWeaponBind = 
						(
							buttonEvent->idCode == 
							controlMap->GetMappedKey
							(
								ue->readyWeapon, 
								RE::INPUT_DEVICE::kGamepad
							)	
						);
						bool isWaitBind = 
						(
							buttonEvent->idCode == 
							controlMap->GetMappedKey
							(
								ue->wait, 
								RE::INPUT_DEVICE::kGamepad
							)	
						);
						// Haven't worked out a way to get the player's currently assigned bind 
						// for opening the container for QuickLoot, 
						// so we'll use the default binds ('Wait' and 'Ready Weapon').
						// TODO:
						// Attempt to read the 'Transfer' bind keycode property 
						// from the QuickLootIE MCM script.
						bool shouldGiveControlOfContainer = 
						(
							(ALYSLC::QuickLootCompat::g_isQuickLootIE) ? 
							(isWaitBind) :
							(isReadyWeaponBind)
						);
						if (shouldGiveControlOfContainer)
						{
							// Send Container Menu request for the player controlling menus.
							bool shouldOpenContainer = 
							(
								(
									glob.menuPID != -1 && 
									Util::HandleIsValid(glob.reqQuickLootContainerHandle)
								) &&
								(glob.menuPID == 0 || companionPlayerMenuInput)
							);
							if (shouldOpenContainer)
							{
								glob.moarm->InsertRequest
								(
									glob.menuPID, 
									InputAction::kActivate, 
									SteadyClock::now(),
									RE::ContainerMenu::MENU_NAME,
									glob.reqQuickLootContainerHandle
								);
								// Issues with opening the container,
								// even though the crosshair pick refr 
								// is set to the requested object,
								// means that I'm going to force the issue here.
								// Open the container directly.
								glob.reqQuickLootContainerHandle.get()->OpenContainer
								(
									!RE::ContainerMenu::ContainerMode::kLoot
								);
							}
						}
						else if (!companionPlayerControllingMenus && isCancelBind)
						{
							// Have to stop the Tween Menu from opening for P1 here, 
							// so swallow the 'Cancel' input, 
							// and instead close the Loot Menu by clearing the crosshair refr.
							// Keeps things consistent with the other players,
							// who also close the LootMenu via clearing the crosshair refr.
							// Exit menu and relinquish control when the cancel bind is pressed.
							auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); 
							if (crosshairPickData)
							{
								// Clears crosshair refr data.
								Util::SendCrosshairEvent(nullptr);
								DBG("{} is closing LootMenu.", p1->GetName());
							}
						}

						idEvent->userEvent = "";
					}
				}
				else if (buttonEvent && !companionPlayerMenuInput)
				{
					if (companionPlayerControllingMenus) 
					{
						// Change event name to the corresponding gameplay context event name 
						// for P1 input events when P1 is not controlling menus.
						// Ensures that menu processing will not occur for these inputs.
						auto p1GameplayContextEvent = 
						(
							controlMap->GetUserEventName
							(
								buttonEvent->idCode, RE::INPUT_DEVICE::kGamepad
							)
						);
						if (Hash(p1GameplayContextEvent) != ""_h)
						{
							idEvent->userEvent = p1GameplayContextEvent;
						}
					}
					else if (!onlyAlwaysOpen)
					{
						// Change DPad event name to the corresponding menu context event name 
						// for P1 input events when P1 is controlling menus.
						// Ensures that menu -- and not gameplay -- processing 
						// will occur for these inputs.
						// eg. Instead of 'Hotkey1'/'Hotkey2' triggering 
						// while P1 is in the Favorites Menu 
						// and pressing left/right on the DPad,
						// the proper 'Left'/'Right' DPad user events will be sent instead.
						// DPad gamepad masks are all < 0xF.
						if (buttonEvent->idCode < 0x0000000F) 
						{
							auto p1MenuContextEvent = 
							(
								controlMap->GetUserEventName
								(
									buttonEvent->idCode, 
									RE::INPUT_DEVICE::kGamepad, 
									RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
								)
							);
							if (Hash(p1MenuContextEvent) != ""_h)
							{
								idEvent->userEvent = p1MenuContextEvent;
							}
						}
					}
				}

				//=============================================================================
				// Two tasks to perform here:
				// 1. Check if the event should be processed once returning from this function.
				// If allowed, the subsequent ProcessEvent() call will allow 
				// any open menus to process the chained input events.
				// Otherwise, the event will not be processed by any menus, 
				// but can still be processed by handlers further down the propagation chain.
				// 2. Propagate the input event unmodified.
				// If unmodified, all following handlers, such as P1's action handlers,
				// can process the event.
				// Otherwise, after modification, 
				// the event will not be processed by any subsequent handlers.
				//=============================================================================
				
				// Attempting to attack with the LH/RH hand form.
				bool isAttackInput = 
				(
					idEvent->userEvent == ue->leftAttack || 
					idEvent->userEvent == ue->rightAttack
				);
				// Is an event that should be blocked from propagating.
				bool isBlockedP1Event = false;
				// P1 input event names become the empty string when in the LootMenu.
				// Filter these out as blocked.
				bool isBlockedP1LootMenuEvent = 
				(
					companionPlayerControllingMenus && 
					!companionPlayerMenuInput && 
					Hash(idEvent->userEvent) == ""_h
				);
				// Attacking on foot.
				bool isGroundedAttackInput = !p1->IsOnMount() && isAttackInput;
				// Is trying to assign a hotkey to a favorited form.
				const auto maskIter = 
				(
					buttonEvent ? 
					glob.cdh->GAMEMASK_TO_XIMASK.find(buttonEvent->idCode) :
					glob.cdh->GAMEMASK_TO_XIMASK.end()
				);
				bool isHotkeyAssignmentInput = 
				(
					ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) && 
					buttonEvent && 
					buttonEvent->idCode != 0xFF &&
					maskIter != glob.cdh->GAMEMASK_TO_XIMASK.end() &&
					maskIter->second == XINPUT_GAMEPAD_RIGHT_THUMB
				);
				// Attempting to move the camera.
				bool isLookInput = idEvent->userEvent == ue->look;
				// Attempting to move the player's arms.
				bool isMoveArmsInput = 
				(
					Settings::bEnableArmsRotation && 
					isAttackInput &&
					!p1->IsWeaponDrawn()
				);
				// Attempting to move P1.
				bool isMoveInput = idEvent->userEvent == ue->move;
				// Attempting to start or stop paragliding.
				bool isParaglidingInput = 
				{
					(
						ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider && 
						p1->GetCharController() && 
						p1->GetCharController()->context.currentState ==
						RE::hkpCharacterStateType::kInAir
					) &&
					(idEvent->userEvent == ue->activate || isMoveInput)
				};
				// Attempting to rotate the camera for P1.
				bool isRotateInput = idEvent->userEvent == ue->rotate;
				// Attempting to rotate the camera while mounted.
				bool isMountedCamInputEvent = 
				(
					(p1->IsOnMount()) && (isRotateInput || isLookInput)
				);

				//=============================================================================
				// Container Tab Switch Check:
				//=============================================================================

				// Allow through if other menus are open and P1 is controlling them.
				//bool allowWaitInputEvent = !onlyAlwaysOpen && p1ControllingMenus;
				//if (idEvent->userEvent == ue->wait)
				//{
				//	// Can P1 switch the container tab to/from their inventory?
				//	if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME))
				//	{
				//		auto containerMenu = ui->GetMenu<RE::ContainerMenu>(); 
				//		if (containerMenu)
				//		{
				//			RE::NiPointer<RE::TESObjectREFR> containerRefrPtr{ };
				//			RE::TESObjectREFR::LookupByHandle
				//			(
				//				RE::ContainerMenu::GetTargetRefHandle(), containerRefrPtr
				//			);
				//			// If the container is not a companion player's inventory chest,
				//			// or if P1 is attempting to switch back 
				//			// to the companion player's inventory chest,
				//			// the tab switch request is valid.
				//			if (!GlobalCoopData::IsCoopPlayerInventoryChest(containerRefrPtr))
				//			{
				//				allowWaitInputEvent = true;
				//			}
				//			else if (auto view = containerMenu->uiMovie; view)
				//			{
				//				RE::GFxValue result{ };
				//				view->Invoke
				//				(
				//					"_root.Menu_mc.isViewingContainer",
				//					std::addressof(result),
				//					nullptr,
				//					0
				//				);
				//				bool isViewingContainer = result.GetBool();
				//				// Only allow a tab switch from P1's inventory 
				//				// back to the co-op companion's inventory.
				//				if (!isViewingContainer)
				//				{
				//					allowWaitInputEvent = true;
				//				}
				//			}
				//		}
				//	}
				//}

				//=============================================================================
				// Should Block or Propagate Events:
				//=============================================================================
				
				bool heldBeforeMenusClosed = 
				(
					onlyAlwaysOpen &&
					buttonEvent && 
					buttonEvent->heldDownSecs > 
					Util::GetElapsedSeconds(glob.lastTempMenusClosedTP)
				);
				// Has a bypass flag indicating that the event 
				// was sent/allowed through by this plugin.
				// P1: 0xC0DA
				bool proxiedP1Input = (idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xC0DA);
				if (p1ManagersActive)
				{
					// While P1 is controlled in co-op by its managers,
					// block LootMenu events, attack inputs, arm movement inputs, 
					// and mounted cam adjustment events while no menus are open.
					// And always block P1 inputs that involve activating objects, 
					// opening menus, equipping favorited items,
					// readying weapons, shouting, sneaking, sprinting, 
					// and changing the camera's POV.
					isBlockedP1Event =
					(
						(!isParaglidingInput) &&
						(
							(heldBeforeMenusClosed) ||
							(isBlockedP1LootMenuEvent) ||
							(
								(onlyAlwaysOpen) && 
								(
									isGroundedAttackInput || 
									isMoveArmsInput || 
									isMountedCamInputEvent
								)
							) ||
							//(idEvent->userEvent == ue->wait && !allowWaitInputEvent) ||
							(isHotkeyAssignmentInput) ||
							(
								(!p1ControllingMenus) &&
								(
									idEvent->userEvent == ue->activate ||
									idEvent->userEvent == ue->favorites ||
									idEvent->userEvent == ue->hotkey1 ||
									idEvent->userEvent == ue->hotkey2 ||
									idEvent->userEvent == ue->journal ||
									idEvent->userEvent == ue->pause ||
									idEvent->userEvent == ue->readyWeapon ||
									idEvent->userEvent == ue->shout ||
									idEvent->userEvent == ue->sneak ||
									idEvent->userEvent == ue->sprint ||
									idEvent->userEvent == ue->togglePOV ||
									idEvent->userEvent == ue->tweenMenu || 
									idEvent->userEvent == ue->wait
								)
							)
						)
					);
				}
				else
				{
					if (companionPlayerControllingMenus)
					{
						// Prevent the usual binds from activating objects, 
						// opening menus, changing the camera POV, and equipping items
						// while another player is controlling menus.
						isBlockedP1Event = 
						(
							heldBeforeMenusClosed ||
							isBlockedP1LootMenuEvent ||
							isHotkeyAssignmentInput ||
							idEvent->userEvent == ue->activate ||
							idEvent->userEvent == ue->favorites ||
							idEvent->userEvent == ue->journal ||
							idEvent->userEvent == ue->pause ||
							idEvent->userEvent == ue->leftEquip ||
							idEvent->userEvent == ue->rightEquip ||
							idEvent->userEvent == ue->togglePOV ||
							idEvent->userEvent == ue->tweenMenu ||
							idEvent->userEvent == ue->wait
						);
					}
				}

				bool isLeftStickInput = idEvent->userEvent == ue->leftStick;
				// NOTE:
				// LS and RS inputs are blocked in the Crafting and Dialogue menus.
				// Use the DPad to navigate, which frees up character movement with the LS, 
				// and camera rotation with the RS.
				bool blockedAnalogStickInput = 
				(
					(thumbstickEvent || isLeftStickInput || isRotateInput) &&
					(
						ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
						ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)
					)
				);
				// Menus which overlay the entire screen or block players.
				bool fullscreenMenuOpen = 
				(
					ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
					ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)  || 
					ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME)	
				);
				// P1 Lockpicking input that should be processed by MenuControls.
				bool allowedP1LockpickingEvent = 
				(
					(companionPlayerControllingMenus && allowP1RotateLock) ||
					(!companionPlayerControllingMenus && !isBlockedP1RotateLockInput)
				);
				//=============================================================================
				// Final Determinations for Propagation:
				//=============================================================================
 
				// [FOR CO-OP COMPANIONS]
				// 1. P2 is trying to rotate the lock for P1 (two player lockpicking enabled).
				// 2. Input event sent by a non-P1 player. -AND-
				// 3. P1 not in control of menus. -AND-
				// 4. Not an analog stick input while in the Dialogue/Crafting Menu. -AND-
				// Either:
				// a. Full screen menu is open 
				// (all players immobile and camera rotation prohibited, so allow through).
				// -OR-
				// b. Is not a move/look input 
				// (does not move P1 or rotate the default FP/TP camera).
				bool validCoopCompanionInput = 
				( 
					(allowP2RotateLock) ||
					(
						(
							companionPlayerMenuInput && 
							companionPlayerControllingMenus && 
							!blockedAnalogStickInput
						) && 
						((fullscreenMenuOpen) || (!isMoveInput && !isLookInput)) 
					)
				);
						
				// [FOR P1]
				// 1. Proxied through with bypass flag. -OR-
				// 2. Rotate lock input while another player is attempting to lockpick
				// and two player lockpicking is enabled. -OR-
				// 3.
				//  a. Not a P1 rotate-lock input while two player lockpicking is enabled,
				//	since, with 2 active players, P1 is only given control of rotating the pick.
				//	b. Not an analog stick input while in the Dialogue/Crafting Menu.
				//	-AND-
				//	c. Not an explicitly blocked P1 input event.
				// 
				// -AND-
				// 
				//	a. Another player is not controlling menus.
				//	-OR-
				//	b. No fullscreen menu is open, and the input event is from P1.
				bool validP1Input = 
				{ 
					(proxiedP1Input || allowP1RotateLock) ||
					(
						(!isBlockedP1RotateLockInput) &&
						(!blockedAnalogStickInput && !isBlockedP1Event) && 
						(
							(!companionPlayerControllingMenus) || 
							(!fullscreenMenuOpen && !companionPlayerMenuInput)
						)
					) 
				};

				// NOTE:
				// Processing is done directly after this function by MenuControls, 
				// which modifies open menus based on the input event(s).
				// Unmodified propagation involves allowing the event through 
				// without any modifications that would invalidate it
				// when being processed by P1's action handlers, 
				// which receive the event(s) after the MenuControls handler processes them.

				// Event should be processed by MenuControls 
				// if the block/ignore flags are not set 
				// and the event is a valid companion player event or a processable P1 event.
				bool shouldProcess = 
				(
					(!wasBlocked && !ignoreInput) && 
					(
						(companionPlayerMenuInput) || 
						(fromCompanionPlayer && allowP2RotateLock) ||
						(!fromCompanionPlayer && allowedP1LockpickingEvent)
					)
				);

				// Propagate the event to P1's action handlers if it shouldn't be ignored 
				// and if it is a valid P1 or co-op player input event.
				bool propagateUnmodifiedEvent = 
				(
					(!wasBlocked && !ignoreInput) && (validP1Input || validCoopCompanionInput)
				);

				// REMOVE when done debugging.
				DBG
				(
					"Menu, MIM PID: {}, {}, "
					"EVENT: {} (0x{:X}, type {}), blocked: {}, co-op player in menus: {}, "
					"p1 manager threads active: {} => PROPAGATE: {}, PROCESS: {}, "
					"proxied P1 input: {}, companion player menu input: {}, "
					"from companion player: {}, "
					"ignored: {}, allowed P1 lockpicking input: {}, "
					"dialogue menu open: {}, is blocked event: {}, "
					"valid companion player input: {}, valid p1 input: {}, "
					"two-player P1 lockpicking "
					"(allow P1/P2 rotate lock, blocked P1 rotate lock input): {}, {}, {}. "
					"Held before all temp menus closed: {}.",
					glob.menuPID,
					glob.mim->managerMenuPID,
					idEvent->userEvent,
					buttonEvent ? buttonEvent->idCode : 0xFF,
					*inputEvent->eventType,
					wasBlocked,
					companionPlayerControllingMenus,
					p1ManagersActive,
					propagateUnmodifiedEvent,
					shouldProcess,
					proxiedP1Input,
					companionPlayerMenuInput,
					fromCompanionPlayer,
					ignoreInput,
					allowedP1LockpickingEvent,
					dialogueMenuOpen,
					isBlockedP1Event,
					validCoopCompanionInput,
					validP1Input,
					allowP1RotateLock,
					allowP2RotateLock,
					isBlockedP1RotateLockInput,
					heldBeforeMenusClosed
				);
				
				if (!propagateUnmodifiedEvent || !shouldProcess)
				{
					BlockInputEvent(inputEvent);
				}
				
				if (propagateUnmodifiedEvent && !shouldProcess)
				{
					// Will restore original event data after processing the blocked event.
					eventsToRestore.emplace_back(inputEvent);
				}

				// On to the next one.
				prevInputEvent = inputEvent;
				inputEvent = inputEvent->next;
			}

			//=======================
			//[Post-Filtering Tasks]:
			//=======================

			// Maintain QuickLoot container selection for the menu-controlling player
			// while the LootMenu is open.
			auto pickData = RE::CrosshairPickData::GetSingleton(); 
			bool setCrosshairTarget = 
			(
				glob.coopSessionActive && 
				glob.cam->IsRunning() &&
				pickData && 
				ui &&
				ui->IsMenuOpen(GlobalCoopData::LOOT_MENU)
			);

			if (setCrosshairTarget)
			{
				const auto& p = 
				(
					companionPlayerControllingMenus ? 
					glob.coopPlayers[glob.mim->managerMenuPID] : 
					glob.coopPlayers[0]
				);
				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
				bool baselineValidity = 
				(
					crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
				);
				// Set the pick data target to the controlling player's crosshair target.
				pickData->target = 
				pickData->targetActor = 
				baselineValidity ? p->tm->crosshairRefrHandle : RE::ObjectRefHandle();
			}

			return eventsToRestore;
		}

		void MenuControlsHooks::RestoreInputEventType(RE::InputEvent * a_event)
		{
			// Restore the input event's original typr.

			// Reset the event type flag which may have been set before 
			// when preventing analog stick inputs from propagating unmodified.
			if (*a_event->eventType > RE::INPUT_EVENT_TYPE::kKinect)
			{
				/*DBG
				(
					"Restore input event type {} from {} for {}.",
					!(*a_event->eventType) - !RE::INPUT_EVENT_TYPE::kKinect + 1,
					*a_event->eventType,
					a_event->QUserEvent()
				);*/
				a_event->eventType = static_cast<RE::INPUT_EVENT_TYPE>
				(
					!(*a_event->eventType) - !RE::INPUT_EVENT_TYPE::kKinect + 1
				);
			}
		}

// NINODE HOOKS
		void NiNodeHooks::UpdateDownwardPass
		(
			RE::NiNode* a_this, RE::NiUpdateData& a_data, std::uint32_t a_arg2
		)
		{
			if (!glob.coopSessionActive)
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// Return early and minimize calculation time in this func.
			const auto strings = RE::FixedStrings::GetSingleton();
			if (!strings)
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// Ignore updates to NPCs.
			auto index = GlobalCoopData::GetCoopPlayerIndex(Util::GetRefrFrom3D(a_this));
			if (index == -1)
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// Ignore if neither node rotation adjustment option is on.
			if (!Settings::bEnableArmsRotation && !Settings::bEnableSpinalRotation)
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			const auto& p = glob.coopPlayers[index];
			// The co-op camera is not enabled, so do not restore P1's modified node rotations.
			if (p->isPlayer1 && !glob.cam->IsRunning())
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// Player actor must be valid too.
			if (!Util::ActorIsValid(p->coopActor.get()))
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// Adjusting nodes produces issues when transformed into a werewolf,
			// so for now, do not apply custom rotations.
			if (Util::IsWerewolf(p->coopActor.get()))
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			// First chain of downward pass recursive calls 
			// always has no flags set for the given node.
			if (a_data.flags == RE::NiUpdateData::Flag::kNone)
			{
				// First call in the recursive chain is always the NPC base node,
				// so save the default rotations before any downward pass calls execute.
				if (a_this->name == strings->npc)
				{
					p->mm->nom->SavePlayerNodeWorldTransforms(p);
					// Update default attack position and rotation 
					// after saving default node orientation data.
					p->mm->UpdateAttackSourceOrientationData(true);
				}
			}

			// Save local rotation and then apply our custom rotation
			// before executing the downward pass to visually apply our changes.
			auto nodePtr = RE::NiPointer<RE::NiNode>(a_this);
			p->mm->nom->defaultNodeLocalTransformsMap.insert_or_assign
			(
				nodePtr, a_this->local
			);
			p->mm->nom->ApplyCustomNodeRotation(p, nodePtr);

			_UpdateDownwardPass(a_this, a_data, a_arg2);
		}

// [PLAYER CAMERA TRANSITION STATE HOOKS]: 
		void PlayerCameraTransitionStateHooks::Begin(RE::PlayerCameraTransitionState* a_this)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive || !glob.cam->IsRunning())
			{
				return _Begin(a_this);
			}

			// Only camera transitions to the First Person or Third Person states are allowed.
			if ((!a_this->transitionTo) || 
				(a_this->transitionTo->id == RE::CameraState::kFirstPerson ||
				a_this->transitionTo->id == RE::CameraState::kThirdPerson))
			{
				return _Begin(a_this);
			}
		}

// [PLAYER CHARACTER HOOKS]:
		void PlayerCharacterHooks::AddObjectToContainer
		(
			RE::PlayerCharacter* a_this, 
			RE::TESBoundObject* a_object, 
			RE::ExtraDataList* a_extraList, 
			std::int32_t a_count, 
			RE::TESObjectREFR* a_fromRefr
		)
		{
			// For logging purposes only right now.
			DBG
			(
				"{}: {} of {}, from {}. List: {:p}.",
				a_this->GetName(),
				a_count, 
				a_object ? a_object->GetName() : "NONE",
				a_fromRefr ? a_fromRefr->GetName() : "NONE",
				fmt::ptr(a_extraList)
			);

			if (!glob.globalDataInit || 
				!glob.allPlayersInit ||
				!glob.coopSessionActive || 
				!a_object)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}

			// If a player is controlling menus, they should receive Enderal-specific loot.
			GlobalCoopData::HandleEnderalSpecificLoot
			(
				a_fromRefr, glob.menuPID != -1 ? glob.menuPID : 0, a_object, a_count
			);

			// Moving an object back to self in this way has led to a ton of crashes 
			// and weird bugs from my experience.
			// Change as sent/received from none.
			if (a_fromRefr == a_this)
			{
				DBG("{}: Move {} to/from none, not self.", 
					a_this->GetName(), a_object->GetName());
				a_fromRefr = nullptr;
			}
			
			const auto& coopP1 = glob.coopPlayers[0];
			if (glob.menuPID > 0 && 
				glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory) &&
				a_fromRefr == glob.coopPlayers[glob.menuPID]->em->inventoryChest.get())
			{
				DBG
				(
					"ALERT: {} to P1 from {}'s inventory chest. Can corrupt P1 pointers. "
					"Added from no one instead.",
					a_object->GetName(), 
					glob.coopPlayers[glob.menuPID]->coopActor->GetName()
				);
				a_fromRefr = nullptr;
			}

			const auto ui = RE::UI::GetSingleton(); 
			if (!ui)
			{
				_AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
				return;
			}

			// Another player is controlling menus.
			if (glob.mim->IsRunning() && glob.menuPID > 0)
			{
				// Do not move to another player or P1's inventory chest 
				// if another player requested to drop this item.
				// NOTE:
				// If the drop request is present when running through this hook,
				// the item was moved from the inventory chest while the menu-controlling 
				// player's inventory is NOT copied to P1.
				bool isDropReq = 
				(
					glob.mim->dropReqPair.first == a_object && glob.mim->dropReqPair.second > 0
				);
				const auto& p = glob.coopPlayers[glob.menuPID];
				bool shouldSendToCompanionPlayer = 
				(
					(!isDropReq) &&
					(!glob.mim->inventoryChestOpen) &&
					(
						glob.copiedPlayerDataTypes.none(CopyablePlayerDataTypes::kInventory) &&
						!Util::IsPartyWideItem(a_object)
					) &&
					(!a_extraList || !a_extraList->HasQuestObjectAlias())
				);
				// Gold does not need to be added to the chest because the modified total
				// is restored to P1 when menus close.
				bool shouldMoveToP1InvChest =
				(
					(!isDropReq) &&
					(glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory)) &&
					(
						(Util::IsPartyWideItem(a_object)) || 
						(a_extraList && a_extraList->HasQuestObjectAlias())
					) &&
					(!a_object->IsGold())
				);
				if (shouldSendToCompanionPlayer)
				{
					DBG
					(
						"Adding item {} (x{}) to {} instead of P1.",
						a_object->GetName(),
						a_count,
						p->coopActor->GetName()
					);
					p->em->inventoryChest->AddObjectToContainer
					(
						a_object, 
						a_extraList,
						a_count,
						nullptr
					);
				}
				else if (shouldMoveToP1InvChest)
				{
					DBG
					(
						"Adding item {} (x{}) to P1's inventory chest instead of P1.",
						a_object->GetName(),
						a_count
					);
					coopP1->em->inventoryChest->AddObjectToContainer
					(
						a_object, 
						a_extraList,
						a_count,
						nullptr
					);
				}
				else if (isDropReq)
				{
					// Set to zero at the minimum.
					glob.mim->dropReqPair.second -= min
					(
						glob.mim->dropReqPair.second, max(0, a_count)
					);
					if (glob.mim->dropReqPair.second == 0)
					{
						glob.mim->dropReqPair.first = nullptr;
					}

					DBG
					(
						"Dropping {} (x{}, {:p}). Drop request is now {}, {}.",
						a_object->GetName(),
						a_count, 
						fmt::ptr(a_extraList),
						glob.mim->dropReqPair.first ?
						glob.mim->dropReqPair.first->GetName() :
						"NONE",
						glob.mim->dropReqPair.second
					);
					// Add and drop directly.
					auto dropPos = 
					(
						p->mm->playerTorsoPosition + 
						Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle
							(
								p->coopActor->GetHeading(false)
							)
						) * 0.5f * p->coopActor->GetHeight()
					);
					_AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
					a_this->DropObject
					(
						a_object,
						a_extraList,
						a_count,
						std::addressof(dropPos)
					);
					/*a_this->RemoveItem
					(
						a_object,
						a_count,
						RE::ITEM_REMOVE_REASON::kDropping,
						a_extraList,
						nullptr,
						std::addressof(dropPos)
					);*/
				}
				else
				{
					DBG
					(
						"Adding item {} (x{}) to P1 as usual.",
						a_object->GetName(),
						a_count
					);
					_AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
				}
				
				return;
			}
			

			_AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
		}

		float PlayerCharacterHooks::CheckClampDamageModifier
		(
			RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_delta
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _CheckClampDamageModifier(a_this, a_av, a_delta);
			}

			const auto& coopP1 = glob.coopPlayers[0];
			bool hmsActorValue = 
			(
				a_av == RE::ActorValue::kHealth ||
				a_av == RE::ActorValue::kMagicka || 
				a_av == RE::ActorValue::kStamina
			);

			// Flash the H/M/S bar as needed.
			if (auto trueHUD = ALYSLC::TrueHUDCompat::g_trueHUDAPI3; trueHUD && hmsActorValue)
			{
				const auto handle = a_this->GetHandle();
				float currentValue = a_this->GetActorValue(a_av);
				if (currentValue > 0.0f && currentValue + a_delta <= 0.0f)
				{
					trueHUD->FlashActorValue(handle, a_av, true);
				}
			}

			// Do not modify AVs when no players are being dismissed and
			// the co-op actor is not revived or if an HMS AV is being decreased in god mode.
			bool notDismissingPlayers = 
			(
				!Settings::bUseReviveSystem || glob.livingPlayers == glob.activePlayers
			);
			if ((notDismissingPlayers) && 
				(!coopP1->isRevived || (hmsActorValue && coopP1->isInGodMode && a_delta < 0.0f)))
			{
				// Prevent arcane fever buildup while in god mode.
				bool arcaneFeverActorValue = 
				(
					ALYSLC::EnderalCompat::g_installed &&
					a_av == RE::ActorValue::kLastFlattered
				);
				if (arcaneFeverActorValue)
				{
					return -a_this->GetActorValue(a_av);
				}
				else
				{
					return 0.0f;
				}
			}
			else
			{
				// NOTE:
				// For stamina, the delta amount is scaled by cost multiplier here
				// instead of in the cost functions in the player action function holder
				// because we want a consistent solution for both types of players.
				// Drawback would be not being able to link an action
				// with a specific stamina reduction, 
				// since this function does not provide any context
				// for the source of the AV change.
				// However, any source of stamina damage can be scaled here,
				// including absorption from enemy spells.

				// Apply damage received mult if the player was damaged.
				// Do not care about the source of the damage in this case,
				// as the damage received mult should apply to all sources of damage.
				if (a_av == RE::ActorValue::kHealth && a_delta < 0.0f)
				{
					// Max negative delta (-FLT_MAX) means that this player 
					// should have <= 0 health even if their damage received multiplier is 0, 
					// so don't apply the mult in that case.
					if (a_delta != -FLT_MAX)
					{
						a_delta *= Settings::vfDamageReceivedMult[0];
						// Also apply health cost mult if reviving another player.
						if (coopP1->isRevivingPlayer)
						{
							// Ensure the player does not lose all their health.
							a_delta = max
							(
								-a_this->GetActorValue(RE::ActorValue::kHealth) + 
								Settings::fMinHealthWhileReviving,
								a_delta * Settings::vfReviveHealthCostMult[0]
							);
						}
					}
				}
				else if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
				{
					// Check if another player is healing P1, and if so, give them XP.
					for (const auto& otherP : glob.coopPlayers)
					{
						if (otherP->isPlayer1 || 
							!otherP->isActive || 
							a_this->GetHandle() != otherP->tm->GetRangedTargetActor())
						{
							continue;
						}

						float currentHealth = a_this->GetActorValue(RE::ActorValue::kHealth);
						float currentMaxHealth = Util::GetFullAVAmount
						(
							a_this, RE::ActorValue::kHealth
						);
						float healthDelta = std::clamp
						(
							a_delta, 0.0f, currentMaxHealth - currentHealth
						);
						// Right hand cast of restoration spell that does not target the caster.
						if (healthDelta > 0.0f && 
							otherP->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellRH
							))
						{
							const auto rhSpell = otherP->em->GetRHSpell(); 
							if (rhSpell && 
								rhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									otherP->playerID, 
									RE::ActorValue::kRestoration, 
									healthDelta
								);
							}
						}

						// Left hand cast of restoration spell that does not target the caster.
						if (healthDelta > 0.0f &&
							otherP->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellLH
							))
						{
							const auto lhSpell = otherP->em->GetLHSpell(); 
							if (lhSpell && 
								lhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									otherP->playerID, 
									RE::ActorValue::kRestoration, 
									healthDelta
								);
							}
						}

						// Quick slot cast of restoration spell that does not target the caster.
						if (healthDelta > 0.0f &&
							otherP->pam->perfSkillIncCombatActions.any
							(
								SkillIncCombatActionType::kRestorationSpellQS
							))
						{
							// Restoration spell that does not target the caster.
							if (otherP->em->quickSlotSpell && 
								otherP->em->quickSlotSpell->GetDelivery() != 
								RE::MagicSystem::Delivery::kSelf)
							{
								GlobalCoopData::AddSkillXP
								(
									otherP->playerID, 
									RE::ActorValue::kRestoration, 
									healthDelta
								);
							}
						}
					}
				}
				else if (a_av == RE::ActorValue::kSpeedMult)
				{
					// Handle weird bug where P1's speed mult becomes negative
					// (can only be fixed by the "ResetPlayer1" debug option).
					const auto currentSpeedMult = 
					(
						a_this->GetActorValue(RE::ActorValue::kSpeedMult)
					);
					if (currentSpeedMult != coopP1->mm->speedMult)
					{
						return 
						(
							coopP1->mm->speedMult - currentSpeedMult
						);
					}
				}
				else if (a_delta < 0.0f && a_av == RE::ActorValue::kStamina)
				{
					// If Elden Sprint is installed, 
					// do not expend stamina while outside of combat.
					if (ALYSLC::EldenSprintCompat::g_installed && 
						!glob.isInCoopCombat)
					{
						return 0.0f;
					}

					// NOTE:
					// This applies to all sources of stamina damage,
					// whether the cost of a stamina-consuming action,
					// or stamina absorption from an outside source.
					//
					// However, the same cannot be done with magicka,
					// since we are already scaling the magicka costs for casting spells
					// in the ActorMagicCaster::RequestCastImpl() hook.
					// Scaling the delta here would double the application of the multiplier.
					// We also have no way of linking each call of this function 
					// to its originating action, so there is no way to scale
					// magicka absorption and other sources of magicka damage.
					a_delta *= Settings::vfStaminaCostMult[0];
				}
			}

			return _CheckClampDamageModifier(a_this, a_av, a_delta);
		}

		void PlayerCharacterHooks::DrawWeaponMagicHands(RE::PlayerCharacter* a_this, bool a_draw)
		{
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				glob.player1DID < 0 || 
				!glob.coopPlayers[0])
			{
				return _DrawWeaponMagicHands(a_this, a_draw);
			}

			const auto& coopP1 = glob.coopPlayers[0];
			// Do not allow the game to automatically sheathe/unsheathe 
			// the player actor's weapons/magic.
			// Blocking weapon/magic drawing while transforming crashes the game at times.
			if (!coopP1->IsRunning() || 
				a_draw == coopP1->pam->weapMagReadied || 
				coopP1->isTransforming)
			{
				return _DrawWeaponMagicHands(a_this, a_draw);
			}
			
			// This may do something. Or not.
			a_this->drawSheatheSafetyTimer = FLT_MAX;
		}

		void PlayerCharacterHooks::HandleHealthDamage
		(
			RE::PlayerCharacter* a_this, RE::Actor* a_attacker, float a_damage
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _HandleHealthDamage(a_this, a_attacker, a_damage);
			}

			auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(a_attacker);
			float damageMult = 1.0f;
			// Co-op player inflicted health damage on P1.
			if (playerAttackerIndex != -1)
			{
				// The attacking player.
				const auto& p = glob.coopPlayers[playerAttackerIndex];
				// Check for friendly fire (not from self) and negate damage.
				if (!Settings::vbFriendlyFire[p->playerID] && a_this != p->coopActor.get())
				{
					a_this->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -a_damage
					);
					return;
				}

				// Apply damage dealt mult for the attacking player.
				damageMult *= Settings::vfDamageDealtMult[p->playerID];
				// TEMP until I find a direct way of applying the sneak damage multiplier 
				// to all forms of damage.
				// Apply sneak/additional damage mult if not attacking self.
				if (!p->isPlayer1 && p->pam->attackDamageMultSet && p->pam->reqDamageMult != 1.0f)
				{
					damageMult *= p->pam->reqDamageMult;
					// Reset damage multiplier if performing a ranged sneak attack.
					// Melee sneak attacks reset the damage multiplier on attack stop,
					// but I have yet to find a way to check 
					// if the player no longer has any active projectiles, 
					// so reset the damage multiplier on damaging hit.
					p->pam->ResetAttackDamageMult();
				}
				
				// Add skill XP if P1 is not the attacker and P1 is not in god mode.
				bool p1HitWhileInGodMode = glob.coopPlayers[0]->isInGodMode;
				if (!p->isPlayer1 && !p1HitWhileInGodMode)
				{
					// Check attack source and increment skill XP if needed.
					const auto lhForm = p->em->equippedForms[!EquipIndex::kLeftHand];
					const auto rhForm = p->em->equippedForms[!EquipIndex::kRightHand];
					const auto qsSpellForm = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
					auto addDestructionXP =
					[&p, &a_damage, a_this](RE::TESForm* a_potentialSourceForm) 
					{
						if (!a_potentialSourceForm)
						{
							return;
						}

						// Is not a destruction spell, so exit.
						const auto spell = a_potentialSourceForm->As<RE::SpellItem>(); 
						if (!spell ||
							!spell->avEffectSetting || 
							spell->avEffectSetting->data.associatedSkill != 
							RE::ActorValue::kDestruction)
						{
							return;
						}

						GlobalCoopData::AddSkillXP
						(
							p->playerID, RE::ActorValue::kDestruction, -a_damage
						);
					};

					// Check for destruction spell cast from LH/RH/Quick Slot.
					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellLH
						))
					{
						addDestructionXP(lhForm);
					}

					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellRH
						))
					{
						addDestructionXP(rhForm);
					}

					if (p->pam->perfSkillIncCombatActions.any
						(
							SkillIncCombatActionType::kDestructionSpellQS
						))
					{
						addDestructionXP(qsSpellForm);
					}
				}
			}
			
			// Adjust damage based off new damage mult.
			// Done before death (< 0 HP) checks below.
			// Ignore direct modifications of HP, which occur with direct changes to HP, 
			// such as RestoreActorValue() below.
			// Don't want to get caught in a recursive loop.
			// NOTE: 
			// As a result, certain types of damage without an attributable attacker, 
			// such as explosion damage,
			// will not be affected by the player's damage dealt multiplier.
			// TODO:
			// Find a way to do health damage without this function triggering,
			// since we currently have to adjust the damage dealt
			// via direct modification of the health actor value.
			// Or will have to figure out how to determine 
			// if the damage source has been scaled already.
			const auto& coopP1 = glob.coopPlayers[0];
			float deltaHealth = a_damage * (damageMult - 1.0f); 
			if (deltaHealth != 0.0f && a_attacker)
			{
				// Apply the inverse of the damage received mult for friendly fire, 
				// since the RestoreActorValue() call below will run through
				// our CheckClampDamageModifier() hook
				// and will apply the damage received mult again to any negative health delta.
				// We can cancel out the second application in this way.
				if (Settings::vfDamageReceivedMult[0] > 0.0f)
				{
					// If additional damage is required,
					// damage to apply for this second call is not modified.
					// Otherwise, this hook will only fire once and we can set the damage applied
					// to the original damage (received damage mult already applied)
					// times the attacker damage mult.
					if (deltaHealth < 0.0f)
					{
						// Not modifying the damage arg itself, 
						// since after multiplying it with the computed damage mult, 
						// we'll have one application each of the damage dealt 
						// and received mults, as required.
						deltaHealth *= 
						(
							1.0f / Settings::vfDamageReceivedMult[0]
						);
					}
					else
					{
						a_damage *= damageMult;
					}
				}
				else
				{
					a_damage = 0.0f;
				}
				
				// This hook will run again with no attacker given 
				// and then execution will return here.
				a_this->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth
				);
			}

			// Check if the player must be set as downed when at or below 0 health.
			// Ignore duplicate calls if the player is already downed 
			// or if there are no living players.
			if (a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f && 
				glob.livingPlayers > 0 &&
				!coopP1->isDowned)
			{
				// Set downed state for P1.
				if (Settings::bUseReviveSystem && Settings::bCanRevivePlayer1)
				{
					// Set P1 as downed.
					coopP1->SetAsDowned();
					bool playerStillStanding = std::any_of
					(
						glob.coopPlayers.begin(), glob.coopPlayers.end(),
						[](const auto& a_p) 
						{
							return a_p->isActive && !a_p->isDowned;
						}
					);
					if (!playerStillStanding)
					{
						// All players downed, end co-op session.
						glob.taskRunner->AddTask([](){ GlobalCoopData::YouDiedTask(); });
					}
					else if (a_this->GetActorValue(RE::ActorValue::kHealth) < 0.0f)
					{
						// Stop! Stop! P1's already dead!
						return;
					}
				}
				else
				{
					// If not using the revive system, once one player dies, 
					// all other players die and the co-op session ends.
					auto handle = a_this->GetHandle();
					glob.taskRunner->AddTask
					(
						[handle](){ GlobalCoopData::YouDiedTask(handle); }
					);
				}
			}

			_HandleHealthDamage(a_this, a_attacker, a_damage);
		}

		void PlayerCharacterHooks::ModifyAnimationUpdateData
		(
			RE::PlayerCharacter* a_this, RE::BSAnimationUpdateData& a_data
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _ModifyAnimationUpdateData(a_this, a_data);
			}
			
			const auto& coopP1 = glob.coopPlayers[0];
			if (a_this->HasKeyword(glob.npcKeyword))
			{
				// Speed up (un)equip/dodging anims.
				// TODO: Support for more dodge mods.
				bool isEquipping = false;
				bool isUnequipping = false;
				bool isTDMDodging = false;
				bool isTKDodging = false;
				a_this->GetGraphVariableBool("IsEquipping", isEquipping);
				a_this->GetGraphVariableBool("IsUnequipping", isUnequipping);
				a_this->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
				a_this->GetGraphVariableBool("bIsDodging", isTKDodging);

				if ((Settings::bSpeedUpEquipAnimations) && (isEquipping || isUnequipping))
				{
					a_data.deltaTime *= Settings::fEquipAnimSpeedFactor;
				}
				else if ((Settings::bSpeedUpDodgeAnimations) && (isTDMDodging || isTKDodging))
				{
					a_data.deltaTime *= Settings::fDodgeAnimSpeedFactor;
				}

				// Increase sprint animation playback speed relative to the default
				// base speed of 85 and base sprint movement mult of 1.5.
				// Feels less floaty at higher sprint speed multipliers,
				// since more steps are taken per second with the increased animation speed.
				if (coopP1->pam->isSprinting) 
				{
					a_data.deltaTime *= max
					(
						0.1f,
						(Settings::fBaseSpeed / 85.0f) * (Settings::fSprintingMovMult / 1.5f)
					);
				}

				// Only speed up the getup animation when any of the three extra mechanics 
				// are enabled while in co-op.
				bool speedupGetup = 
				(
					(glob.coopSessionActive) &&
					(
						Settings::bEnableArmsRotation ||
						Settings::bEnableFlopping ||
						Settings::bEnableObjectManipulation
					) &&
					(
						coopP1->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued ||
						coopP1->coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kGetUp
					)
				);
				if (speedupGetup)
				{
					a_data.deltaTime *= 3.0f;
				}
			}

			if (coopP1->mm->isDashDodging)
			{
				// Dash dodge animation speedup depends on LS displacement and equipped weight.
				const float weightAdjAnimSpeedFactor = Util::InterpolateEaseIn
				(
					1.0f, 
					0.5f, 
					std::clamp
					(
						coopP1->mm->dashDodgeEquippedWeight / 75.0f, 
						0.0f, 
						1.0f
					), 
					2.0f
				) * Settings::fDashDodgeAnimSpeedFactor * coopP1->mm->dashDodgeLSDisplacement;
				a_data.deltaTime *= weightAdjAnimSpeedFactor;
			}
			else if (a_this->IsSwimming() && coopP1->pam->IsPerforming(InputAction::kSprint))
			{
				// Speed up swimming animation to match the increased speedmult
				// while 'sprinting' in the water.
				a_data.deltaTime *= max(0.1f, Settings::fSprintingMovMult);
			}

			_ModifyAnimationUpdateData(a_this, a_data);
		}

		bool PlayerCharacterHooks::NotifyAnimationGraph
		(
			RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit)
			{
				return _NotifyAnimationGraph(a_this, a_eventName);
			}

			const auto& coopP1 = glob.coopPlayers[0];
			auto hash = Hash(a_eventName);
			if (glob.partyWiped || coopP1->isDowned)
			{
				DBG
				(
					"Party wiped: {}. P1 state: {}. Co-op session active: {}, living players: {}, "
					"is dead: {}, is paralyzed: {}, is downed: {}, P1 anim: {}.", 
					glob.partyWiped,
					coopP1->coopActor->GetLifeState(),
					glob.coopSessionActive,
					glob.livingPlayers,
					coopP1->coopActor->IsDead(),
					coopP1->coopActor->boolBits.all(RE::Actor::BOOL_BITS::kParalyzed),
					coopP1->isDowned,
					a_eventName,
					glob.p1IsEssential,
					coopP1->coopActor->GetActorValue(RE::ActorValue::kHealth)
				);
				
				if (Util::MenusOnlyAlwaysOpen() && hash == "bleedOutStop"_h)
				{
					// If P1 is downed or all other players are dead,
					// Prevent P1 from exiting the bleedout state.
					return false;
				}
			}

			auto ue = RE::UserEvents::GetSingleton();
			if (!ue)
			{
				return _NotifyAnimationGraph(a_this, a_eventName);
			}

			if (glob.coopSessionActive)
			{
				if (Settings::bUseReviveSystem && hash == "BleedoutStart"_h && !glob.p1IsEssential)
				{
					// Skip bleedout animations when using the co-op revive system.
					// Players will ragdoll and become unresponsive when reaching 0 health instead.
					return _NotifyAnimationGraph(a_this, "bleedOutStop");
				}
				else if ((coopP1->pam->IsPerforming(InputAction::kSprint)) &&
						(hash == "SprintStop"_h || hash == "sprintStop"_h))
				{
					// Prevent the game from stopping sprint
					// while the player is performing the sprint action.
					// Occurs after toggling AI driven on and running the Update() hook.
					return false;
				}
				else if (((coopP1->isDowned && !coopP1->isRevived) || 
						 (coopP1->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && 
						 hash == "GetUpBegin"_h && 
						 coopP1->selfValid)
				{
					// Ignore requests to get up when the player is downed and not revived.
					return false;
				}
				else if ((coopP1->coopActor->IsInKillMove()) && 
						 (hash == "PairEnd"_h || hash == "pairedStop"_h))
				{
					// Sometimes, when a killmove fails, the player will remain locked in place
					// because the game still considers them to be in a killmove,
					// so unset the flag here to signal the player's PAM to stop handling
					// the previously triggered killmove and reset the player's data.
					coopP1->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
				}
				else if (!coopP1->isTransformed && hash == "BiteStart"_h)
				{
					// Can't recall what this was for, but... oh well.
					// It's probably doing something and isn't messing with anything else
					// as far as I can tell.
					return true;
				}
				else if ((hash == "staggerStart"_h) &&
						 (
							 coopP1->isRevivingPlayer || 
							 coopP1->coopActor->IsOnMount() || 
							 Util::HandleIsValid(coopP1->coopActor->GetOccupiedFurniture())
						 ))
				{
					// Prevent stagger when reviving, mounted, or using furniture,
					// which will make the player exit the animation or dismount prematurely 
					// and potentially glitch their equip state.
					return _NotifyAnimationGraph(a_this, "staggerStop");
				}
			}
			// Failsafe to ensure that P1 does not get up when dead after co-op ends.
			else if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
			{
				// P1 should be dead if all other players are dead after the co-op session ends,
				// so ignore requests to get up while downed and waiting for the game to reload.
				// Also attempt to force the issue by killing P1 if a get up is requested.
				if (glob.livingPlayers == 0 && hash == "GetUpBegin"_h)
				{
					if (glob.p1IsEssential)
					{
						// Make sure P1 can get up if set as essential.
						p1->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
						return _NotifyAnimationGraph(a_this, a_eventName);
					}
					else
					{
						// First, make sure the essential flag is unset.
						Util::ChangeEssentialStatus(p1, false);
						p1->KillImpl(p1, FLT_MAX, false, false);
						p1->KillImmediate();
						p1->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);
						// Kill calls fail on P1 at times, 
						// especially when the player dies in water, 
						// and the game will not reload.
						// The kill console command appears to work when this happens, 
						// so as an extra layer of insurance, run that command here.
						const auto scriptFactory = 
						(
							RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
						);
						const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
						if (script)
						{
							script->SetCommand("kill");
							script->CompileAndRun(p1);
							// Cleanup.
							delete script;
						}

						return false;
					}
				}
			}

			return _NotifyAnimationGraph(a_this, a_eventName);
		}

		void PlayerCharacterHooks::PickUpObject
		(
			RE::PlayerCharacter* a_this,
			RE::TESObjectREFR* a_object,
			std::int32_t a_count, 
			bool a_arg3, 
			bool a_playSound
		)
		{
			DBG
			(
				"{}: {} of {}. List: {:p}. Arg3: {}.",
				a_this->GetName(),
				a_count, 
				a_object ? a_object->GetName() : "NONE",
				fmt::ptr(a_object ? std::addressof(a_object->extraList) : nullptr),
				a_arg3
			);

			if (!glob.globalDataInit ||
				!glob.allPlayersInit ||
				!glob.coopSessionActive || 
				!a_object)
			{
				return _PickUpObject(a_this, a_object, a_count, a_arg3, a_playSound);
			}

			// If there is extra ownership data and the owner is a companion player,
			// we know they were set as the owner when picking up the item
			// and that this call is from the Character::PickUpObject() hook.
			// This means that the companion player, and not P1, should receive this object
			// if it is an Enderal skillbook.
			int32_t lootingPID = 0;
			auto exOwnership = a_object->extraList.GetByType<RE::ExtraOwnership>(); 
			if (exOwnership && exOwnership->owner && exOwnership->owner->As<RE::Actor>())
			{
				auto pIndex = GlobalCoopData::GetCoopPlayerIndex
				(
					exOwnership->owner->As<RE::Actor>()
				); 
				if (pIndex > 0)
				{
					lootingPID = pIndex;
				}
			}

			GlobalCoopData::HandleEnderalSpecificLoot
			(
				a_object, lootingPID, a_object->GetBaseObject(), a_count
			);
			
			_PickUpObject(a_this, a_object, a_count, a_arg3, a_playSound);
		}

		RE::ObjectRefHandle* PlayerCharacterHooks::RemoveItem
		(
			RE::PlayerCharacter* a_this, 
			RE::ObjectRefHandle* a_handleOut, 
			RE::TESBoundObject* a_item, 
			std::int32_t a_count, 
			RE::ITEM_REMOVE_REASON a_reason, 
			RE::ExtraDataList* a_extraList, 
			RE::TESObjectREFR* a_moveToRef, 
			const RE::NiPoint3* a_dropLoc, 
			const RE::NiPoint3* a_rotate
		)
		{
			DBG
			(
				"{}: {} of {}, to {}. List: {:p}.",
				a_this->GetName(),
				a_count, 
				a_item ? a_item->GetName() : "NONE",
				a_moveToRef ? a_moveToRef->GetName() : "NONE",
				fmt::ptr(a_extraList)
			);

			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive || !a_item)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}

			// Do not move quest or party-wide items to other players or their inventory chests.
			// Also do not move any items to the companion player or their inventory chest
			// while their inventory is copied over to P1.
			bool shouldNotRemove = false;
			// Another player is controlling menus.
			if (glob.mim->IsRunning() && glob.menuPID > 0)
			{
				const auto& p = glob.coopPlayers[glob.menuPID];
				shouldNotRemove = 
				(
					(a_moveToRef != a_this) && 
					(
						(
							glob.copiedPlayerDataTypes.all
							(
								CopyablePlayerDataTypes::kInventory
							) &&
							(
								a_moveToRef == p->coopActor.get() || 
								a_moveToRef == p->em->inventoryChest.get()
							)
						) ||
						(
							glob.copiedPlayerDataTypes.none
							(
								CopyablePlayerDataTypes::kInventory
							) &&
							(GlobalCoopData::IsCoopEntity(a_moveToRef)) && 
							(
								(Util::IsPartyWideItem(a_item)) ||
								(a_extraList && a_extraList->HasQuestObjectAlias())
							)
						)
					)
				);
			}
			else
			{
				shouldNotRemove = 
				(
					(a_moveToRef != a_this) && 
					(
						(
							glob.copiedPlayerDataTypes.all
							(
								CopyablePlayerDataTypes::kInventory
							)
						) ||
						(
							glob.copiedPlayerDataTypes.none
							(
								CopyablePlayerDataTypes::kInventory
							) &&
							(GlobalCoopData::IsCoopEntity(a_moveToRef)) && 
							(
								(Util::IsPartyWideItem(a_item)) ||
								(a_extraList && a_extraList->HasQuestObjectAlias())
							)
						)
					)
				);

				// This should not happen, as with no companion players in menus,
				// P1's inventory should have been restored.
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					ERR
					(
						"ERR: Cannot move {} of {} from P1 to {} "
						"because another player's inventory is still copied over to P1. "
						"Previous player in control of menus was {}.",
						a_count,
						a_item->GetName(),
						a_moveToRef ? a_moveToRef->GetName() : "NONE",
						glob.prevMenuPID >= 0 ?
						glob.coopPlayers[glob.prevMenuPID]->coopActor->GetName() : 
						"NONE"
					);
				}
			}

			// Removing an item from P1 to the inventory chest that had its inventory changes
			// copied over to P1 will assign the currently opened container to Papyrus PlayerRef
			// script properties. Very, very bad news for stability. And very, very weird.
			// Prevent that from happening here, as there's no reason to move the item to the chest
			// anyway, since the changes to P1 are mirrored to the chest while the inventory changes
			// pointers are equivalent with menus open.
			if (shouldNotRemove)
			{
				DBG
				(
					"ALERT: NOT moving item {} (x{}) to {}. "
					"Companion player controlling menus: {}. "
					"Inventory copied over to P1: {}",
					a_item->GetName(),
					a_count,
					a_moveToRef ? a_moveToRef->GetName() : "NONE",
					glob.mim->IsRunning() && glob.menuPID > 0,
					glob.copiedPlayerDataTypes.all
					(
						CopyablePlayerDataTypes::kInventory
					)
				);
				return nullptr;
			}
			
			auto ui = RE::UI::GetSingleton();
			// Trying to move an item to another player while the Gift Menu is open.
			bool giftingItem = 
			(
				ui &&
				ui->IsMenuOpen(RE::GiftMenu::MENU_NAME) &&
				GlobalCoopData::IsCoopPlayer(glob.mim->gifteePlayerHandle) &&
				glob.mim->IsRunning() && 
				glob.mim->managerMenuPID != -1 &&
				GlobalCoopData::IsCoopPlayer(a_moveToRef)
			);
			// Trying to move an item to a non-co-op entity from P1's inventory,
			// which is really the companion player's inventory copied over to P1.
			bool canTransferToNonCoopEntityOrDrop = 
			(
				(
					a_moveToRef &&
					!GlobalCoopData::IsCoopEntity(a_moveToRef) &&
					glob.mim->IsRunning() && 
					glob.mim->managerMenuPID != -1 &&
					glob.mim->isShowingInventory
				) &&
				(
					(ui) && 
					(
						ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) || 
						ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)
					)
				)
			);

			// WTF:
			// Selling favorited forms unequips everything on P1 for some reason.
			// Two things:
			// Unfavorite before transferring.
			// Unequip/remove from inventory before transferring.
			if (giftingItem || canTransferToNonCoopEntityOrDrop)
			{
				auto invEntry = Util::GetInventoryEntryDataForObject(a_this, a_item, a_extraList);
				const auto& menuP = glob.coopPlayers[glob.mim->managerMenuPID];
				if (invEntry && invEntry->extraLists && !invEntry->extraLists->empty())
				{
					for (const auto exDataList : *invEntry->extraLists) 
					{
						if (!exDataList) 
						{
							continue;
						}
					
						auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
						if (exHotkey)
						{
							DBG("{} is favorited. Remove hotkey data", a_item->GetName());
							exDataList->Remove(RE::ExtraDataType::kHotkey, exHotkey);
						}

						auto exRank = exDataList->GetByType<RE::ExtraRank>();
						if (exRank)
						{
							DBG
							(
								"{} has rank mask 0x{:X}.",
								a_item->GetName(), 
								static_cast<uint32_t>(exRank->rank)
							);

							if ((exRank->rank & 0xFFFF0000) == 0xFFFF0000)
							{
								auto matchingPlayerList = Util::GetEquippedExtraData
								(
									menuP->coopActor.get(), a_item, true
								);
								if (matchingPlayerList)
								{
									DBG
									(
										"{} is in both hands: LH 0x{:X}.",
										a_item->GetName(), static_cast<uint32_t>(exRank->rank)
									);
									menuP->em->UnequipFormAtIndex(EquipIndex::kLeftHand);
								}

								matchingPlayerList = Util::GetEquippedExtraData
								(
									menuP->coopActor.get(), a_item, false
								);
								if (matchingPlayerList)
								{
									DBG
									(
										"{} is in both hands. RH 0x{:X}.",
										a_item->GetName(), 
										static_cast<uint32_t>(exRank->rank)
									);
									menuP->em->UnequipFormAtIndex(EquipIndex::kRightHand);
								}
							}
							else if ((exRank->rank & 0x00FF0000) != 0)
							{
								auto matchingPlayerList = Util::GetEquippedExtraData
								(
									menuP->coopActor.get(), a_item, false
								);
								if (matchingPlayerList)
								{
									DBG
									(
										"{} is in RH/Default slot: 0x{:X}.",
										a_item->GetName(), 
										static_cast<uint32_t>(exRank->rank)
									);
									if (a_item->As<RE::TESAmmo>())
									{
										menuP->em->UnequipAmmo(a_item);
									}
									else if (a_item->As<RE::TESObjectARMO>())
									{
										menuP->em->UnequipArmor
										(
											a_item, matchingPlayerList->GetCount()
										);
									}
									else
									{
										menuP->em->UnequipFormAtIndex(EquipIndex::kRightHand);
									}
								}
							}
							else if ((exRank->rank & 0xFF000000) != 0)
							{
								auto matchingPlayerList = Util::GetEquippedExtraData
								(
									menuP->coopActor.get(), a_item, true
								);
								if (matchingPlayerList)
								{
									DBG
									(
										"{} is in LH: 0x{:X}.",
										a_item->GetName(), 
										static_cast<uint32_t>(exRank->rank)
									);
									menuP->em->UnequipFormAtIndex(EquipIndex::kLeftHand);
								}
							}
						}
					}
				}

				// IMPORTANT:
				// Another player's inventory is copied over to P1,
				// so we must not move any items to P1 directly, 
				// as doing so just adds the item back to the same container.
				if (giftingItem)
				{
					const auto& gifterP = glob.coopPlayers[glob.mim->managerMenuPID];
					const auto& gifteeP = glob.coopPlayers
					[
						GlobalCoopData::GetCoopPlayerIndex(glob.mim->gifteePlayerHandle)
					];
					DBG
					(
						"{} is gifting {} of {} to {}. Move to ref is {} before modification.",
						gifterP->coopActor->GetName(),
						a_count,
						a_item->GetName(),
						gifteeP->coopActor->GetName(),
						a_moveToRef ? a_moveToRef->GetName() : "NONE"
					);

					// If the giftee player is player 1, 
					// this means that we should move the item to P1's inventory chest, 
					// which should contain P1's cached inventory 
					// before the companion player's inventory was copied over 
					// before the Gift Menu opened.
					if (gifteeP->isPlayer1)
					{
						DBG
						(
							"Moving item {} to P1's inventory chest, "
							"the contents of which will be restored as P1's inventory "
							"when the Gift Menu closes.",
							a_item->GetName()
						);
						a_moveToRef = gifteeP->em->inventoryChest.get();
					}
					else
					{
						a_moveToRef = gifteeP->coopActor.get();
					}
				}
				else
				{
					DBG
					(
						"P1 transferring item {} ({:p}, x{}) to {}.",
						a_item->GetName(), fmt::ptr(a_extraList), a_count, a_moveToRef->GetName()
					);

					// Check if there's an exFlag we set before transferring the item,
					// which means the companion player wanted to drop the item 
					// while their inventory is copied to P1.
					// Clear the flag afterward either way.
					bool isDropReq = 
					(
						glob.mim->dropReqPair.first == a_item && glob.mim->dropReqPair.second > 0
					);
					if (isDropReq)
					{
						// Set to zero at the minimum.
						glob.mim->dropReqPair.second -= min
						(
							glob.mim->dropReqPair.second, max(0, a_count)
						);
						if (glob.mim->dropReqPair.second == 0)
						{
							glob.mim->dropReqPair.first = nullptr;
						}

						DBG
						(
							"{}: Dropping {} (x{}, {:p}). Drop request is now {}, {}.",
							menuP->coopActor->GetName(), 
							a_item->GetName(),
							a_count, 
							fmt::ptr(a_extraList),
							glob.mim->dropReqPair.first ?
							glob.mim->dropReqPair.first->GetName() :
							"NONE",
							glob.mim->dropReqPair.second
						);
						auto dropPos = 
						(
							menuP->mm->playerTorsoPosition + 
							Util::RotationToDirectionVect
							(
								0.0f, 
								Util::ConvertAngle
								(
									menuP->coopActor->GetHeading(false)
								)
							) * 0.5f * menuP->coopActor->GetHeight()
						);
						return _RemoveItem
						(
							a_this, 
							a_handleOut,
							a_item, 
							a_count,
							RE::ITEM_REMOVE_REASON::kDropping,
							a_extraList, 
							nullptr, 
							std::addressof(dropPos), 
							a_rotate
						);
					}
				}
			}

			if (a_moveToRef == a_this)
			{
				if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
				{
					DBG
					(
						"Not moving {} of {} from P1 to themselves "
						"while another player's inventory is copied over.",
						a_count, a_item ? a_item->GetName() : "NONE"
					);
					return nullptr;
				}
				else
				{
					DBG
					(
						"Not moving {} of {} from P1 to themselves. Removing from inventory.",
						a_count, a_item ? a_item->GetName() : "NONE"
					);
					a_moveToRef = nullptr;
				}
			}

			return _RemoveItem
			(
				a_this, 
				a_handleOut,
				a_item, 
				a_count,
				a_reason,
				a_extraList, 
				a_moveToRef, 
				a_dropLoc, 
				a_rotate
			);
		}

		void PlayerCharacterHooks::ResetInventory(RE::PlayerCharacter* a_this, bool a_leveledOnly)
		{
			auto ui = RE::UI::GetSingleton();
			if (!ui)
			{
				return _ResetInventory(a_this, a_leveledOnly);	
			}

			DBG("Resetting P1 ({})'s inventory. RaceMenu open: {}.", 
				a_this->GetName(),
				ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME));
			if (!ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME))
			{
				return _ResetInventory(a_this, a_leveledOnly);	
			}
		}

		void PlayerCharacterHooks::Update(RE::PlayerCharacter* a_this, float a_delta)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit)
			{
				return _Update(a_this, a_delta);
			}

			if (!glob.cam->IsRunning())
			{
				a_this->playerFlags.shouldUpdateCrosshair = true;
				return _Update(a_this, a_delta);
			}
			
			// Run game's update first.
			const auto& coopP1 = glob.coopPlayers[0];
			if (coopP1->IsRunning())
			{
				// Remove and then restore AI driven after the player update
				// so that we can clear out fog-of-war, 
				// which is only removed if the player is controls driven.
				// Side effect(s):
				// Resets some P1 player state, so toggling interrupts sprinting, 
				// possibly among other things that I've yet to discover and yet to care about.
				bool performingSprint = coopP1->pam->IsPerforming(InputAction::kSprint);
				bool justStarted = coopP1->pam->JustStarted(InputAction::kSprint);
				// Less stutter if allowing the animation to start without removing AI driven.
				if (justStarted)
				{
					_Update(a_this, a_delta);
				}
				else
				{
					/*if (performingSprint)
					{
						RE::BSAnimationGraphManagerPtr manager{ };
						a_this->GetAnimationGraphManager(manager);
						if (manager)
						{
							manager->variableCache.updateLock.Lock();
							for (const auto& info : manager->variableCache.variableCache)
							{
								DBG("Var name: {}.", info.variableName);
							}

							manager->variableCache.updateLock.Unlock();
						}
					}*/
					
					// Sync player actorstate sprint flag with ALYSLC's sprint player action state.
					// Otherwise, P1 will stop sprinting after the AI driven toggle.
					// Ensures the game handles stamina expenditure.
					// Sync player character singleton flag too for good measure.
					// Who knows what code could access it, whether another mod or the game itself,
					// so better to keep everything in sync.
					bool wasAIDriven = 
					(
						a_this->movementController && !a_this->movementController->controlsDriven
					);
					Util::SetPlayerAIDriven(false);
					a_this->actorState1.sprinting = 
					a_this->playerFlags.isSprinting = performingSprint;

					_Update(a_this, a_delta);
			
					if (wasAIDriven)
					{
						Util::SetPlayerAIDriven(true);
					}

					// Set again if cleared.
					a_this->actorState1.sprinting = 
					a_this->playerFlags.isSprinting = performingSprint;
				}
			}
			else
			{
				// Just perform the update if the manager is not running.
				_Update(a_this, a_delta);
			}
			

			//===================
			// Node Orientations.
			//===================
			// NOTE: 
			// All downward passes for the player's nodes have been performed at this point,
			// so restore all saved default local transforms for the next frame.
			// Reasoning: Sometimes, such as when a havok impulse is applied to the player,
			// the game won't restore the animation-derived local transforms 
			// for all the player's nodes, since the havok impulse 
			// applied its own overriding local transform to the node(s).
			// Thus, any of our local transform modifications from the last frame 
			// will carry over and stack with this frame's,
			// which leads to setting incorrect local transforms (lots of spinning) 
			// unless the defaults are restored first.
			coopP1->mm->nom->RestoreOriginalNodeLocalTransforms(coopP1);

			//===========================
			// Movement and Player State.
			//===========================
			
			// Prevent the game from updating the crosshair text on its own 
			// while the co-op cam is active.
			a_this->playerFlags.shouldUpdateCrosshair = false;

			// Make sure player is set to alive if not downed.
			bool inDownedLifeState = 
			(
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
			);
			if (glob.livingPlayers > 0 && !coopP1->isDowned && inDownedLifeState)
			{
				a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
			}
			/*else if (glob.partyWiped || coopP1->isDowned)
			{
				a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kBleedout;
			}*/

			// Need a valid current process to continue.
			auto currentProc = a_this->currentProcess; 
			if (!currentProc)
			{
				return;
			}

			auto ui = RE::UI::GetSingleton();
			auto high = currentProc->high; 
			bool gamePaused = ui->GameIsPaused();
			if (high && !gamePaused && coopP1->mm->IsRunning())
			{
				auto paraMT = glob.paraglidingMT;
				auto& speeds = 
				(
					high->currentMovementType.defaultData.speeds
				);
				auto& rotateWhileMovingRun = 
				(
					high->currentMovementType.defaultData.rotateWhileMovingRun	
				);
				
				// NOTE: 
				// Base movement type data values seem to only reset 
				// to their defaults each frame 
				// if the player's speedmult is modified.
				// Otherwise, the movement speed changes each frame will accumulate, 
				// reaching infinity and preventing the player from moving.
				float speedMultToSet = coopP1->mm->speedMult;
				if (speedMultToSet < 0.0f || isnan(speedMultToSet) || isinf(speedMultToSet))
				{
					speedMultToSet = coopP1->mm->baseSpeedMult;
				}

				coopP1->coopActor->SetBaseActorValue(RE::ActorValue::kSpeedMult, speedMultToSet);
				// Applies the new speedmult right away,
				coopP1->coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f
				);
				coopP1->coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f
				);

				// NOTE:
				// Another annoying issue to work around:
				// Since movement speed does not update 
				// while the player is ragdolled or getting up,
				// if the player was moving fast before ragdolling, 
				// they'll shoot out in their facing direction
				// once they fully get up and until their movement speed normalizes.
				// Do not allow movement until the player's movement speed zeroes out 
				// if the player has just fully gotten up.
				// Obviously a better solution would involve 
				// finding a way to set movement speed directly to 0
				// when ragdolled or getting up, but for now, this'll have to do.

				// Set movement speed to an obscenely high value to quickly
				// arrest built up momentum while also keeping the player in place
				// with the 'don't move' flag.
				if (coopP1->mm->shouldCurtailMomentum)
				{
					// Ensure the player is set to not move 
					// and any lingering movement offset is cleared.
					// Otherwise, sanic mode.
					coopP1->mm->ClearKeepOffsetFromActor();
					Util::NativeFunctions::SetDontMove(coopP1->coopActor.get(), true);

					// Affects how quickly the player slows down.
					// The higher, the faster the reported movement speed becomes zero.
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kWalk]			=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kRun]				=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kWalk]			=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kRun]				=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kWalk]			=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kRun]				=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kWalk]			=
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kRun]				= 100000.0f;
				}
				else if (auto charController = coopP1->coopActor->GetCharController(); 
						 charController)
				{
					//================
					// Rotation speed.
					//================
					if (coopP1->mm->isDashDodging)
					{
						// No rotation when dodging.
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kWalk]					=
						speeds[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kRun]						=
						rotateWhileMovingRun								= 0.0f;
					}
					else if (coopP1->mm->isParagliding)
					{
						// Scale up default rotation rates.
						if (paraMT)
						{
							const auto& paraglidingSpeeds = 
							(
								paraMT->movementTypeData.defaultData.speeds
							);
							const auto& paraglidingRotateWhileMovingRun = 
							(
								paraMT->movementTypeData.defaultData.rotateWhileMovingRun	
							);

							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kWalk] =
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kWalk] * 
								Settings::fBaseRotationMult *
								Settings::fBaseMTRotationMult
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kRun] =
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kRotations]
								[RE::Movement::MaxSpeeds::kRun] * 
								Settings::fBaseRotationMult * 
								Settings::fBaseMTRotationMult
							);
							rotateWhileMovingRun =
							(
								paraglidingRotateWhileMovingRun * 
								Settings::fBaseRotationMult * 
								Settings::fBaseMTRotationMult
							);
						}
						else
						{
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kWalk] =
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRotations]
							[RE::Movement::MaxSpeeds::kRun] =
							(
								70.0f * TO_RADIANS * 
								Settings::fBaseRotationMult *
								Settings::fBaseMTRotationMult
							);
							rotateWhileMovingRun =
							(
								120.0f * TO_RADIANS * 
								Settings::fBaseRotationMult * 
								Settings::fBaseMTRotationMult
							);
						}
					}
					else
					{
						// Increase rotation speed 
						// since all the movement types' default speeds
						// are too slow when used with KeepOffsetFromActor()
						// and produce sluggish changes in movement direction.
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kWalk] =
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kRun] =
						rotateWhileMovingRun =
						(
							Settings::fBaseRotationMult * Settings::fBaseMTRotationMult * PI
						);

					}

					//=================
					// Movement speeds.
					//=================
					// NOTE:
					// Paraglide dodge velocity changes are char controller velocity-based 
					// and are not handled here.
					// Simply set the movement type data to the paraglide MT equivalent.
					if (coopP1->mm->isParagliding)
					{
						if (paraMT)
						{
							const auto& paraglidingSpeeds = 
							(
								paraMT->movementTypeData.defaultData.speeds
							);

							// Movement speeds.
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kWalk] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kWalk]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kRun] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kLeft]
								[RE::Movement::MaxSpeeds::kRun]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kWalk] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kWalk]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kRun] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kRight]
								[RE::Movement::MaxSpeeds::kRun]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kWalk] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kWalk]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kRun] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kForward]
								[RE::Movement::MaxSpeeds::kRun]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kWalk] = 
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kWalk]
							);
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kRun] =
							(
								paraglidingSpeeds
								[RE::Movement::SPEED_DIRECTIONS::kBack]
								[RE::Movement::MaxSpeeds::kRun]
							);
						}
						else
						{
							// Same movement speeds across the board when paragliding.
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kRun]				=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kWalk]			=
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kRun]				= 700.0f;
						}
					}
					else if (coopP1->mm->isDashDodging)
					{
						// Interpolate between the starting and ending speedmult values.
						float dodgeSpeed = Util::InterpolateEaseInEaseOut
						(
							Settings::fMaxDashDodgeSpeedmult,
							Settings::fMinDashDodgeSpeedmult,
							coopP1->mm->dashDodgeCompletionRatio,
							2.0f
						);

						// Same speed across the board when dodging.
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kLeft]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kLeft]
						[RE::Movement::MaxSpeeds::kRun]				=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRight]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRight]
						[RE::Movement::MaxSpeeds::kRun]				=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kForward]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kForward]
						[RE::Movement::MaxSpeeds::kRun]				=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kBack]
						[RE::Movement::MaxSpeeds::kWalk]			=
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kBack]
						[RE::Movement::MaxSpeeds::kRun]				= dodgeSpeed;
					}
					else if (bool isAIDriven = coopP1->coopActor->movementController && 
							 !coopP1->coopActor->movementController->controlsDriven; isAIDriven)
					{
						RE::NiPoint3 linVelXY = RE::NiPoint3();
						float movementToHeadingAngDiff = -1.0f;
						float range = -1.0f;
						float diffFactor = -1.0f;
						// Player must not be sprinting, mounted, downed, animation driven, 
						// or running their interaction package.
						if (!coopP1->pam->IsPerforming(InputAction::kSprint) && 
							!coopP1->coopActor->IsOnMount() && 
							!coopP1->mm->isAnimDriven && 
							!coopP1->mm->interactionPackageRunning && 
							!coopP1->isDowned)
						{
							
							// The core movement problem when using KeepOffsetFromActor() 
							// with the player themselves as the offset target
							// is slow deceleration/acceleration 
							// when changing directions rapidly.
							// First noticed that playing the 'SprintStart' animation event
							// right as the player starts pivoting causes them to turn
							// and face the new movement direction almost instantly.
							// Increasing the movement type's directional max speed values, 
							// depending on how rapidly the player is turning,
							// has the same effect as forcing the player to briefly sprint 
							// each time they change directions
							// and removes most of the sluggishness.
							// Can still cause rapid bursts of movement at times.

							// Out velocity seems to be the intended velocity 
							// before collisions are accounted for.
							// Do not need velocity Z component.
							linVelXY = RE::NiPoint3
							(
								charController->outVelocity.quad.m128_f32[0], 
								charController->outVelocity.quad.m128_f32[1], 
								0.0f
							);
							auto linVelYaw = 
							(
								linVelXY.Length() == 0.0f ? 
								coopP1->analogStickParams[!AnalogStickParams::kLSCamRelAng] : 
								Util::DirectionToGameAngYaw(linVelXY)
							);
							// Yaw difference between the XY velocity direction 
							// and the direction in which the player wishes to head.
							movementToHeadingAngDiff = 
							(
								coopP1->lsMoved ? 
								Util::NormalizeAngToPi
								(
									coopP1->analogStickParams[!AnalogStickParams::kLSCamRelAng] - 
									linVelYaw
								) : 
								0.0f
							);
							// Sets the bounds for the diff factor applied to movement speed below. 
							// Dependent on rotation speeds -- rotate faster, pivot faster.
							range = max
							(
								1.0f, 
								(Settings::fBaseMTRotationMult * Settings::fBaseRotationMult) / 
								3.0f
							);
							// Max speed factor. Maxes out at 90 degrees.
							diffFactor = 
							(
								1.0f + 
								(
									range * 
									powf
									(
										std::clamp
										(
											fabsf(movementToHeadingAngDiff) / (PI / 2.0f), 
											0.0f, 
											1.0f
										), 
										6.0f
									)
								)
							);

							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kLeft]
							[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kRight]
							[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kWalk]			*= diffFactor;
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kBack]
							[RE::Movement::MaxSpeeds::kRun]				*= diffFactor;
						}
					}
				}

				// Not sure if this affects P1, but max out to prevent armor re-equip.
				high->reEquipArmorTimer = FLT_MAX;
			}

			if (auto midHigh = currentProc->middleHigh; midHigh)
			{
				if (Settings::bUseReviveSystem && 
					Settings::bCanRevivePlayer1 && 
					Settings::bCanKillmoveOtherPlayers)
				{
					// If using the revive system and killed by another player, 
					// prevent game from forcing the player into a bleedout state.
					midHigh->deferredKillTimer = FLT_MAX;
				}
			}

			// [TEMP WORKAROUND 1]:
			// Temporary solution to players becoming "hostile" towards one another.
			// Remove targeted players from this player's combat group.
			a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
			Util::RemovePlayerCombatTargets(a_this);
		}

		std::uint32_t PlayerCharacterHooks::UseAmmo
		(
			RE::PlayerCharacter* a_this, std::uint32_t a_shotCount
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _UseAmmo(a_this, a_shotCount);
			}

			auto invCounts = a_this->GetInventoryCounts();
			const auto iter = invCounts.find(a_this->GetCurrentAmmo());
			if (iter != invCounts.end() && iter->second <= a_shotCount)
			{
				const auto& p = glob.coopPlayers[0];
				auto currentAmmo = p->coopActor->GetCurrentAmmo();
				const auto ammoUsed = _UseAmmo(a_this, a_shotCount);
				currentAmmo = p->coopActor->GetCurrentAmmo();
				p->em->UnequipAmmo(currentAmmo);
				p->em->AutoEquipAmmo(p->em->GetRHWeapon());
				
				// Notify the player that they do not have ammo equipped,
				// and if new ammo was equipped, let them know what type.
				currentAmmo = p->coopActor->GetCurrentAmmo();
				auto exDataList = Util::GetEquippedExtraData
				(
					p->coopActor.get(), currentAmmo, false
				);
				if (currentAmmo)
				{
					p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format
						(
							"P{}: No equipped ammo! Equipped {}", 
							p->playerID + 1,
							Util::GetDescriptiveName(currentAmmo, exDataList)
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					p->tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format("P{}: No equipped ammo!", p->playerID + 1),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState,
							CrosshairMessageType::kTargetSelection 
						},
						0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}

				return ammoUsed;
			}

			return _UseAmmo(a_this, a_shotCount);
		}

		void PlayerCharacterHooks::UseSkill
		(
			RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_points, RE::TESForm* a_arg3
		)
		{
			// NOTE: 
			// For melee-related skills, 
			// eg. OneHanded/TwoHanded/Archery/HeavyArmor/LightArmor/Block,
			// this call fires before the corresponding hit event(s) are propagated
			// and HandleHealthDamage() call(s) are fired,
			// so we cache the results and discard the first call, 
			// which we'll delay until friendly fire processing occurs in the Melee Hit hook.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _UseSkill(a_this, a_av, a_points, a_arg3);
			}

			{
				std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex, std::try_to_lock);
				if (lock)
				{
					DBG
					(
						"Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);

					// For melee skills, cache and delay if cached data does not match call args.
					if (a_av == RE::ActorValue::kOneHanded || a_av == RE::ActorValue::kTwoHanded ||
						a_av == RE::ActorValue::kArchery || a_av == RE::ActorValue::kBlock ||
						a_av == RE::ActorValue::kHeavyArmor || a_av == RE::ActorValue::kLightArmor)
					{
						if (!glob.lastP1MeleeUseSkillCallArgs ||
							glob.lastP1MeleeUseSkillCallArgs->skill != a_av || 
							glob.lastP1MeleeUseSkillCallArgs->points != a_points || 
							glob.lastP1MeleeUseSkillCallArgs->assocForm != a_arg3)
						{
							glob.lastP1MeleeUseSkillCallArgs = 
							(
								std::make_unique<GlobalCoopData::LastP1MeleeUseSkillCallArgs>
								(
									a_av, a_points, a_arg3
								)
							);
							// No points to award here.
							a_points = 0.0f;
							return _UseSkill(a_this, a_av, a_points, a_arg3);
						}
					}

					if (GlobalCoopData::SHARED_SKILL_AVS_SET.contains(a_av))
					{
						float mult = 1.0f;
						// Shared skills XP is usually received while in a menu 
						// (e.g. lockpicking/pickpocketing/smithing),
						// so apply the menu-controlling player's skill XP multiplier.
						if (glob.menuPID != -1)
						{
							mult = Settings::vfSkillXPMult[glob.menuPID];
						}
						else
						{
							// Scale by average of all active players' skill XP multipliers 
							// if no player is controlling menus.
							if (glob.activePlayers != 0.0f)
							{
								mult = 0.0f;
								for (const auto& p : glob.coopPlayers)
								{
									if (!p->isActive)
									{
										continue;
									}

									mult += Settings::vfSkillXPMult[p->playerID];
								}

								mult /= glob.activePlayers;
							}
						}

						a_points *= mult;
					}
					else
					{
						// Use P1's skill XP mult for non-shared skills.
						a_points *= Settings::vfSkillXPMult[0];
					}
				}
				else
				{
					DBG
					(
						"Failed to obtain lock (0x{:X}). Will not use skill.", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					return;
				}
			}

			_UseSkill(a_this, a_av, a_points, a_arg3);
		}

// [PROJECTILE HOOKS]:
		void ProjectileHooks::GetLinearVelocity(RE::Projectile* a_this, RE::NiPoint3& a_velocity)
		{
			// Not handled outside of co-op.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				// Output the game's set velocity.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::BarrierProjectile>())
				{
					_BarrierProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					_BeamProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::ConeProjectile>())
				{
					_ConeProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					_FlameProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					_GrenadeProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else
				{
					_Projectile_GetLinearVelocity(a_this, a_velocity);
				}
				
				return;
			}

			auto projMgr = RE::Projectile::Manager::GetSingleton();
			if (!projMgr)
			{
				return;
			}

			projMgr->projectileLock.Lock();

			if (!a_this || !a_this->GetHandle())
			{
				projMgr->projectileLock.Unlock();
				return;
			}


			// Ensure the projectile's handle is valid first.
			const auto projectileHandle = a_this->GetHandle();
			auto projectilePtr = Util::GetRefrPtrFromHandle(projectileHandle);
			if (!projectilePtr)
			{
				projMgr->projectileLock.Unlock();
				return;
			}

			bool justReleased = a_this->livingTime == 0.0f;
			int32_t firingPlayerIndex = -1;
			bool firedAtPlayer = false;
			GetFiredAtOrByPlayer(projectileHandle, firingPlayerIndex, firedAtPlayer);

			int32_t grabbedByPlayerPID = -1;
			int32_t releasedByPlayerPID = -1;
			GetManipulatingPlayer(projectileHandle, grabbedByPlayerPID, releasedByPlayerPID);
			const int32_t playerID = 
			(
				firingPlayerIndex != -1 ? 
				firingPlayerIndex :
				grabbedByPlayerPID != -1 ?
				grabbedByPlayerPID :
				releasedByPlayerPID != -1 ? 
				releasedByPlayerPID :
				-1
			);
			// Restore our linear velocity if fired by the player.
			if (playerID != -1)
			{
				const auto& p = glob.coopPlayers[playerID];
				// Have to insert as managed on release if this hook was run 
				// before the UpdateImpl() hook.
				if (justReleased && 
					firingPlayerIndex != -1 && 
					!p->tm->mph->IsManaged(a_this->GetHandle()))
				{
					// Overwrite the projectile's velocity and angular orientation.
					DirectProjectileAtTarget
					(
						glob.coopPlayers[firingPlayerIndex], 
						projectileHandle,
						a_this->linearVelocity, 
						justReleased
					);
				}

				// Output the velocity as our saved velocity from the UpdateImpl() hook.
				a_velocity = a_this->linearVelocity;
			}
			else
			{
				// Output the game's set velocity.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::BarrierProjectile>())
				{
					_BarrierProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					_BeamProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::ConeProjectile>())
				{
					_ConeProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					_FlameProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					_GrenadeProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_GetLinearVelocity(a_this, a_velocity);
				}
				else
				{
					_Projectile_GetLinearVelocity(a_this, a_velocity);
				}
			}
			
			projMgr->projectileLock.Unlock();
		}

		void ProjectileHooks::OnProjectileCollision
		(
			RE::Projectile* a_this, 
			RE::hkpAllCdPointCollector* a_AllCdPointCollector
		)
		{
			// Check for thrown active projectile collisions with actors
			// and bonk as needed.
			// NOTE:
			// Unsure if modifying the hits collected and then clearing all recorded hits 
			// before copying back into the collector's in-place hits array is a good idea.
			// And the method is obviously not efficient, 
			// but it does allow us to ignore specific collisions along a projectile's path.
			// Needs thorough testing for stability.

			// Nothing to do if global data is not initialized or no co-op session is active.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_OnArrowCollision(a_this, a_AllCdPointCollector);
				}
				else if (a_this->As<RE::ConeProjectile>())
				{
					_ConeProjectile_OnConeCollision(a_this, a_AllCdPointCollector);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_OnMissileCollision(a_this, a_AllCdPointCollector);
				}
				else
				{
					_Projectile_OnProjectileCollision(a_this, a_AllCdPointCollector);
				}

				return;
			}
			
			const auto projHandle = a_this->GetHandle();
			// Unsure why copy construction via range constructors
			// results in cone projectiles stalling, as if their collision gets disabled,
			// even when all the original hits are copied over.
			// Default construction and then std::ranges::copy seems to work fine
			// as an alternative.
			std::vector<RE::hkpRootCdPoint> newHits{ };
			if (!a_AllCdPointCollector->hits.empty())
			{
				std::ranges::copy
				(
					a_AllCdPointCollector->hits.begin(),
					a_AllCdPointCollector->hits.end(), 
					std::back_inserter(newHits)
				);
			}

			// Hit actors to trigger collisions and potentially start combat with.
			// Pairs of (player actor, targeted actor).
			std::vector<std::pair<RE::Actor*, RE::Actor*>> combatTargetStartPairs{ };
			// Delayed bonks to apply. 
			// Maps aggressor players' PIDs to the hit actor to knock down
			// and the hit position.
			// Ew.
			std::unordered_map<int32_t, std::pair<RE::ActorHandle, RE::NiPoint3>> 
			delayedActorCollisions{ };
			// Set of FIDs for the actors hit or to start combat with.
			std::unordered_set<RE::FormID> combatTargetFIDs{ };
			// Remove any hits that should be ignored.
			std::erase_if
			(
				newHits, 
				[
					a_this,
					&projHandle,
					&combatTargetStartPairs,
					&combatTargetFIDs,
					&delayedActorCollisions
				]
				(const auto& hit)
				{
					auto refrA = 
					(
						hit.rootCollidableA ? 
						RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA) : 
						nullptr
					);
					auto refrB = 
					(
						hit.rootCollidableB ? 
						RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB) : 
						nullptr
					);
					auto objA = 
					(
						hit.rootCollidableA ?
						RE::TESHavokUtilities::FindCollidableObject(*hit.rootCollidableA) :
						nullptr
					);
					auto objB = 
					(
						hit.rootCollidableB ? 
						RE::TESHavokUtilities::FindCollidableObject(*hit.rootCollidableB) : 
						nullptr
					);

					// REMOVE when done debugging.
					/*DBG
					(
						"Collision {} (0x{:X}, {}, {}) <-> {} (0x{:X}, {}, {})", 
						refrA ? refrA->GetName() : "NONE", 
						refrA ? refrA->formID : 0xDEAD,
						refrA && refrA->GetBaseObject() ? 
						Util::GetEditorID(refrA->GetBaseObject()) : 
						"NONE",
						objA ? objA->name : "NONE",
						refrB ? refrB->GetName() : "NONE",
						refrB ? refrB->formID : 0xDEAD,
						refrB && refrB->GetBaseObject() ? 
						Util::GetEditorID(refrB->GetBaseObject()) : 
						"NONE",
						objB ? objB->name : "NONE"
					);	*/

					// Skip if at least one collidable has no associated refr.
					if (!refrA || !refrB)
					{
						return false;
					}

					// Ignore self-collisions.
					if (refrA == refrB)
					{
						return false;
					}

					RE::Actor* hitActor = refrA->As<RE::Actor>();
					if (!hitActor)
					{
						hitActor = refrB->As<RE::Actor>();
					}

					// Skip collisions not involving this projectile.
					if (refrA != a_this && refrB != a_this)
					{
						return false;
					}

					// Check to see if one of the two refrs is a manipulated refr
					// and get the PID of the manipulating player.
					// Also start combat between NPCs and the aggressor player
					// before the hit applies.
					int32_t manipulatingPlayerPID = -1;
					bool hitActorIsPlayer = GlobalCoopData::IsCoopPlayer(hitActor);
					const auto hitActorHandle = 
					(
						hitActor ? hitActor->GetHandle() : RE::ActorHandle()
					);
					for (const auto& p : glob.coopPlayers)
					{
						if (!p->isActive)
						{
							continue;
						}
							
						// Check if this projectile is managed by this player 
						// and hit another managed refr. Ignore the collision if so.
						bool hitAnotherManagedRefr = 
						(
							(
								p->tm->rmm->IsManaged(refrA->GetHandle(), true) &&
								p->tm->rmm->IsManaged(refrB->GetHandle(), true)
							) ||
							(
								p->tm->rmm->IsManaged(refrA->GetHandle(), false) &&
								p->tm->rmm->IsManaged(refrB->GetHandle(), false)
							)
						);
						if (hitAnotherManagedRefr)
						{
							/*DBG
							(
								"IGNORED MANAGED COLLISION {} <-> {}", 
								refrA->GetBaseObject() ? 
								Util::GetEditorID(refrA->GetBaseObject()) : 
								"NONE", 
								refrB->GetBaseObject() ? 
								Util::GetEditorID(refrB->GetBaseObject()) : 
								"NONE"
							);	*/
							return true;
						}

						if (p->tm->rmm->IsManaged(refrA->GetHandle(), false) ||
							p->tm->rmm->IsManaged(refrB->GetHandle(), false))
						{
							manipulatingPlayerPID = p->playerID;
						}

						// See GlobalCoopData::PrecisionPreHitCallback() 
						// for an explanation.
						// Trigger combat between companion players and any NPCs they hit.
						bool actorHitByPlayer =
						(
							hitActor && 
							p->coopActor->GetHandle() == a_this->shooter
						);
						if (actorHitByPlayer)
						{
							// Ignore if P1 is hitting a target 
							// while their managers are not running (no co-op cam).
							if (p->isPlayer1 && !p->IsRunning())
							{
								continue;
							}

							bool isHostile = 
							(
								(!hitActorIsPlayer) &&
								(
									(hitActor->IsHostileToActor(p->coopActor.get())) || 
									(
										Util::HandleIsValid(hitActor->currentCombatTarget) &&
										Util::IsPartyFriendlyActor
										(
											hitActor->currentCombatTarget.get().get()
										)
									)
								)
							);
							bool isPartyFriendlyActor = Util::IsPartyFriendlyActor(hitActor);
							bool isNeutralActor = !isHostile && !isPartyFriendlyActor;
							bool isDesiredTarget = 
							(
								(hitActor) &&
								(
									(hitActorHandle == p->tm->selectedTargetActorHandle) ||
									(
										p->tm->aimMode == AimMode::kTwinStick &&
										hitActorHandle == p->tm->aimCorrectionTargetHandle
									)
								)
							);
							bool isBeneficialProjectile =
							(
								a_this->spell && 
								!Util::HasHostileEffect(a_this->spell)
							);
							// Only allow collisions through if targeting a hostile actor,
							// directly targeting a neutral actor with the crosshair,
							// or targeting an ally with a beneficial projectile
							// or targeting an ally with the crosshair 
							// or while the crosshair is disabled
							// while friendly fire is on.
							bool collisionAllowed = 
							(
								(
									!hitActor->IsGhost() && !hitActor->IsInvulnerable()
								) ||
								(
									(isHostile) ||
									(isNeutralActor && isDesiredTarget) ||
									(
										(isPartyFriendlyActor) && 
										(
											(isBeneficialProjectile) ||
											(
												isDesiredTarget &&
												Settings::vbFriendlyFire[p->playerID]
											)
										)
									)
								)
							);
							if (collisionAllowed)
							{
								DBG
								(
									"ALLOWED {} <-> {}", 
									refrA->GetBaseObject() ? 
									Util::GetEditorID(refrA->GetBaseObject()) : 
									"NONE", 
									refrB->GetBaseObject() ? 
									Util::GetEditorID(refrB->GetBaseObject()) : 
									"NONE"
								);
								// Do not start combat with other players
								// and do not need to start combat for P1.
								if (!hitActorIsPlayer && !p->isPlayer1)
								{
									Util::UpdateCombatTargets
									(
										p->coopActor.get(), hitActor, false
									);
									// Trigger combat if the target is not already hostile,
									// or if the hit actor and player 
									// are not combat targets for each other.
									// 
									// Finally, also only start combat 
									// if this is the first time the actor is hit this frame.
									bool shouldTriggerCombat = 
									(
										(
											!hitActor->IsHostileToActor
											(
												p->coopActor.get()
											) || 
											!hitActor->IsCombatTarget
											(
												p->coopActor.get()
											) ||
											!p->coopActor->IsCombatTarget(hitActor)
										) &&
										(
											combatTargetFIDs.empty() ||
											!combatTargetFIDs.contains(hitActor->formID)
										)
									);
									if (shouldTriggerCombat)
									{
										combatTargetStartPairs.emplace_back
										(
											std::pair<RE::Actor*, RE::Actor*>
											(
												p->coopActor.get(),
												hitActor
											)
										);
										combatTargetFIDs.insert(hitActor->formID);
									}
								}
							}
							else
							{
								DBG
								(
									"IGNORED {} <-> {}", 
									refrA->GetBaseObject() ? 
									Util::GetEditorID(refrA->GetBaseObject()) : 
									"NONE", 
									refrB->GetBaseObject() ? 
									Util::GetEditorID(refrB->GetBaseObject()) : 
									"NONE"
								);
								return true;
							}
						}
					}

					// At least one refr in the hit pair must be a released refr 
					// for one of the active players.
					if (manipulatingPlayerPID == -1)
					{
						return false;
					}

					const auto& p = glob.coopPlayers[manipulatingPlayerPID];
					// Must be manipulated as a released refr.
					const auto iter = 
					(
						p->tm->rmm->releasedRefrHandlesToInfoIndices.find(projHandle)
					);
					if (iter == p->tm->rmm->releasedRefrHandlesToInfoIndices.end())
					{
						return false;
					}

					auto index = iter->second;
					if (index >= p->tm->rmm->releasedRefrInfoList.size())
					{
						return false;
					}

					const auto& releasedRefrInfo = p->tm->rmm->releasedRefrInfoList[index];
					// Hit a new, valid actor that is not the released refr 
					// or the player that released the refr.
					bool shouldBonk = 
					(
						hitActor &&
						hitActor->currentProcess && 
						hitActor != p->coopActor.get() && 
						!releasedRefrInfo->HasAlreadyHitRefr(hitActor)
					);
					if (shouldBonk)
					{
						// Save for later.
						delayedActorCollisions.insert_or_assign
						(
							p->playerID,
							std::pair<RE::ActorHandle, RE::NiPoint3>
							(
								hitActor->GetHandle(),
								ToNiPoint3(hit.contact.position) * HAVOK_TO_GAME
							)
						);
					}

					// Add as a hit refr to prevent multi-hits.
					releasedRefrInfo->AddHitRefr(refrB);
					return false;
				}
			);
				
			// Clear out old hits before re-adding all filtered hits
			// if at least 1 hit was removed.
			if (newHits.size() < a_AllCdPointCollector->hits.size())
			{
				a_AllCdPointCollector->Reset();
				if (!newHits.empty())
				{
					std::ranges::copy
					(
						newHits.begin(),
						newHits.end(), 
						std::back_inserter(a_AllCdPointCollector->hits)
					);
				}
			}
			
			// Start combat between players and any cached hit actors 
			// before allowing the game to process the collision.
			for (const auto& actorStartCombatPair : combatTargetStartPairs)
			{
				if (!actorStartCombatPair.first || !actorStartCombatPair.second)
				{
					continue;
				}
				
				if (actorStartCombatPair.second->IsInRagdollState())
				{
					continue;
				}
				
				DBG
				(
					"Start combat between {} and {}.",
					actorStartCombatPair.first ? 
					actorStartCombatPair.first->GetName() : 
					"NONE",
					actorStartCombatPair.second ? 
					actorStartCombatPair.second->GetName() : 
					"NONE"
				);
				Util::ApplyHit
				(
					actorStartCombatPair.first,
					actorStartCombatPair.second,
					0.0f,
					true
				);		
			}
			
			DBG("{} hits to handle, was {}. {} NPCs to start combat with.",
				newHits.size(), a_AllCdPointCollector->hits.size(), combatTargetStartPairs.size());
			// Now, let the game handle the projectile collision.
			if (a_this->As<RE::ArrowProjectile>())
			{
				_ArrowProjectile_OnArrowCollision(a_this, a_AllCdPointCollector);
			}
			else if (a_this->As<RE::ConeProjectile>())
			{
				_ConeProjectile_OnConeCollision(a_this, a_AllCdPointCollector);
			}
			else if (a_this->As<RE::MissileProjectile>())
			{
				_MissileProjectile_OnMissileCollision(a_this, a_AllCdPointCollector);
			}
			else
			{
				_Projectile_OnProjectileCollision(a_this, a_AllCdPointCollector);
			}
			
			// And lastly, handle delayed thrown projectile knockdowns,
			// which provides more consistent damage output because the game's original function
			// does not always apply damage to hit actors that have already ragdolled.
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(a_this);
			if (!Util::HandleIsValid(projHandle))
			{
				return;
			}

			for (const auto& [pid, actorHitPosPair] : delayedActorCollisions)
			{
				if (pid < 0 || pid >= ALYSLC_MAX_PLAYER_COUNT)
				{
					continue;
				}
					
				const auto& p = glob.coopPlayers[pid];
				const auto iter = 
				(
					p->tm->rmm->releasedRefrHandlesToInfoIndices.find(projHandle)
				);
				if (iter == p->tm->rmm->releasedRefrHandlesToInfoIndices.end())
				{
					continue;
				}

				auto index = iter->second;
				if (index >= p->tm->rmm->releasedRefrInfoList.size())
				{
					continue;
				}

				const auto& releasedRefrInfo = p->tm->rmm->releasedRefrInfoList[index];
				p->tm->HandleBonk
				(
					actorHitPosPair.first, 
					projHandle, 
					hkpRigidBodyPtr ? hkpRigidBodyPtr->motion.GetMass() : 0.0f,
					releasedRefrInfo->fallHeight,
					(
						hkpRigidBodyPtr ? 
						ToNiPoint3
						(
							hkpRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME
						) :
						a_this->linearVelocity
					), 
					actorHitPosPair.second
				);

				// Update fall height to the projectile's position after handling the collision.
				releasedRefrInfo->fallHeight = a_this->data.location.z;
			}
		}

		inline bool ProjectileHooks::ProcessHit
		(
			RE::Projectile* a_this,
			RE::TESObjectREFR* a_hitRefr,
			RE::NiPoint3* a_location,
			RE::hkVector4* a_unknown,
			RE::COL_LAYER a_collisionLayer,
			RE::MATERIAL_ID a_materialID,
			bool* a_handled
		)
		{
			// Check for projectile explosions that hit friendly actors
			// and prevent damage if friendly fire conditions are not met.

			// Not handled if invalid or outside of co-op.
			if (!a_this || 
				!a_hitRefr || 
				!glob.globalDataInit || 
				!glob.allPlayersInit || 
				!glob.coopSessionActive)
			{
				return 
				(
					_Projectile_ProcessHit
					(
						a_this,
						a_hitRefr, 
						a_location, 
						a_unknown, 
						a_collisionLayer,
						a_materialID,
						a_handled
					)
				);
			}

			RE::Actor* hitActor = a_hitRefr->As<RE::Actor>();
			if (!hitActor)
			{
				return 
				(
					_Projectile_ProcessHit
					(
						a_this,
						a_hitRefr, 
						a_location, 
						a_unknown, 
						a_collisionLayer,
						a_materialID,
						a_handled
					)
				);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->shooter);
			if (pIndex != -1)
			{
				const auto& p = glob.coopPlayers[pIndex];
				// See GlobalCoopData::PrecisionPreHitCallback() 
				// for an explanation.
				// Trigger combat between companion players and any NPCs they hit.
				const auto hitActorHandle = hitActor->GetHandle();
				bool hitActorIsPlayer = GlobalCoopData::IsCoopPlayer(hitActor);
				bool isHostile = 
				(
					(!hitActorIsPlayer) &&
					(
						(hitActor->IsHostileToActor(p->coopActor.get())) || 
						(
							Util::HandleIsValid(hitActor->currentCombatTarget) &&
							Util::IsPartyFriendlyActor(hitActor->currentCombatTarget.get().get())
						)
					)
				);
				bool isPartyFriendlyActor = Util::IsPartyFriendlyActor(hitActor);
				bool isNeutralActor = !isHostile && !isPartyFriendlyActor;
				bool isDesiredTarget = 
				(
					(hitActor) &&
					(
						(hitActorHandle == p->tm->selectedTargetActorHandle) ||
						(
							p->tm->aimMode == AimMode::kTwinStick &&
							hitActorHandle == p->tm->aimCorrectionTargetHandle
						)
					)
				);
				bool isBeneficialProjectile =
				(
					a_this->spell && 
					!Util::HasHostileEffect(a_this->spell)
				);
				// Only allow collisions through if targeting a hostile actor,
				// directly targeting an neutral actor with the crosshair,
				// or targeting an ally with a beneficial projectile
				// or targeting an ally with the crosshair while friendly fire is on.
				bool collisionAllowed = 
				(
					(isHostile) ||
					(isNeutralActor && isDesiredTarget) ||
					(
						(isPartyFriendlyActor) && 
						(
							(isBeneficialProjectile) ||
							(
								isDesiredTarget &&
								Settings::vbFriendlyFire[p->playerID]
							)
						)
					)
				);
				if (!collisionAllowed)
				{
					DBG("Collision ignored.");
					return false;
				}
			}

			return 
			(
				_Projectile_ProcessHit
				(
					a_this,
					a_hitRefr, 
					a_location, 
					a_unknown, 
					a_collisionLayer,
					a_materialID,
					a_handled
				)
			);
		}

		bool ProjectileHooks::RunTargetPick(RE::Projectile* a_this)
		{
			// If the player launched this projectile, 
			// ensure the chosen target is the player's current ranged target actor.
			// Allows beam and flame projectiles to hit more consistently.

			// Not handled outside of co-op.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				if (a_this->As<RE::BarrierProjectile>())
				{
					return _BarrierProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					return _BeamProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					return _FlameProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					return _GrenadeProjectile_RunTargetPick(a_this);
				}
				else
				{
					return _Projectile_RunTargetPick(a_this);
				}
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->shooter);
			if (pIndex != -1)
			{
				const auto& p = glob.coopPlayers[pIndex];
				auto rangedTargetActorHandle = p->tm->GetRangedTargetActor();
				const auto magicTargetBefore = 
				(
					a_this->GetMagicTarget() &&
					a_this->GetMagicTarget()->GetTargetAsActor() ?
					a_this->GetMagicTarget()->GetTargetAsActor() : 
					nullptr
				);
				const auto desiredTargetBefore = a_this->desiredTarget;
				a_this->desiredTarget = rangedTargetActorHandle;
				bool result = false;
				if (a_this->As<RE::BarrierProjectile>())
				{
					result = _BarrierProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					result = _BeamProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					result = _FlameProjectile_RunTargetPick(a_this);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					result = _GrenadeProjectile_RunTargetPick(a_this);
				}
				else
				{
					result = _Projectile_RunTargetPick(a_this);
				}
					
				DBG
				(
					"{}: {} (0x{:X}, {}), before: {}, {}, after {}, {}, now {}. Result: {}",
					p->coopActor->GetName(),
					a_this->GetName(),
					a_this->formID,
					Util::GetEditorID(a_this),
					magicTargetBefore ? magicTargetBefore->GetName() : "NONE/OBJ",
					Util::HandleIsValid(desiredTargetBefore) ? 
					desiredTargetBefore.get()->GetName() :
					"NONE",
					a_this->GetMagicTarget() &&
					a_this->GetMagicTarget()->GetTargetAsActor() ? 
					a_this->GetMagicTarget()->GetTargetAsActor()->GetName() : 
					"NONE/OBJ",
					Util::HandleIsValid(a_this->desiredTarget) ? 
					a_this->desiredTarget.get()->GetName() :
					"NONE",
					Util::HandleIsValid(rangedTargetActorHandle) ? 
					rangedTargetActorHandle.get()->GetName() : 
					"NONE",
					result
				);
				a_this->desiredTarget = rangedTargetActorHandle;
				return true; //result;
			}

			if (a_this->As<RE::BarrierProjectile>())
			{
				return _BarrierProjectile_RunTargetPick(a_this);
			}
			else if (a_this->As<RE::BeamProjectile>())
			{
				return _BeamProjectile_RunTargetPick(a_this);
			}
			else if (a_this->As<RE::FlameProjectile>())
			{
				return _FlameProjectile_RunTargetPick(a_this);
			}
			else if (a_this->As<RE::GrenadeProjectile>())
			{
				return _GrenadeProjectile_RunTargetPick(a_this);
			}
			else
			{
				return _Projectile_RunTargetPick(a_this);
			}
		}

		bool ProjectileHooks::ShouldUseDesiredTarget(RE::Projectile* a_this)
		{
			// If the player launched this projectile, 
			// ensure the chosen target is the player's current ranged target actor.
			// Allows beam and flame projectiles to hit more consistently.

			// Not handled outside of co-op.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				if (a_this->As<RE::BarrierProjectile>())
				{
					return _BarrierProjectile_ShouldUseDesiredTarget(a_this);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					return _BeamProjectile_ShouldUseDesiredTarget(a_this);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					return _FlameProjectile_ShouldUseDesiredTarget(a_this);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					return _GrenadeProjectile_ShouldUseDesiredTarget(a_this);
				}
				else
				{
					return _Projectile_ShouldUseDesiredTarget(a_this);
				}
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->shooter);
			if (pIndex != -1)
			{
				const auto& p = glob.coopPlayers[pIndex];
				auto rangedTargetActorHandle = p->tm->GetRangedTargetActor();
				a_this->desiredTarget = rangedTargetActorHandle;
				return Util::HandleIsValid(rangedTargetActorHandle);
			}

			if (a_this->As<RE::BarrierProjectile>())
			{
				return _BarrierProjectile_ShouldUseDesiredTarget(a_this);
			}
			else if (a_this->As<RE::BeamProjectile>())
			{
				return _BeamProjectile_ShouldUseDesiredTarget(a_this);
			}
			else if (a_this->As<RE::FlameProjectile>())
			{
				return _FlameProjectile_ShouldUseDesiredTarget(a_this);
			}
			else if (a_this->As<RE::GrenadeProjectile>())
			{
				return _GrenadeProjectile_ShouldUseDesiredTarget(a_this);
			}
			else
			{
				return _Projectile_ShouldUseDesiredTarget(a_this);
			}
		}

		void ProjectileHooks::UpdateImpl(RE::Projectile* a_this, float a_delta)
		{
			// Not handled outside of co-op.
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				// Run the game's update function.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::BarrierProjectile>())
				{
					_BarrierProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::BeamProjectile>())
				{
					_BeamProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::ConeProjectile>())
				{
					_ConeProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::FlameProjectile>())
				{
					_FlameProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::GrenadeProjectile>())
				{
					_GrenadeProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_UpdateImpl(a_this, a_delta);
				}
				else
				{
					_Projectile_UpdateImpl(a_this, a_delta);
				}
				
				return;
			}

			auto projMgr = RE::Projectile::Manager::GetSingleton();
			if (!projMgr)
			{
				return;
			}

			projMgr->projectileLock.Lock();

			if (!a_this || !a_this->GetHandle())
			{
				projMgr->projectileLock.Unlock();
				return;
			}

			// Maintain constant XY velocity when no longer handled.
			auto savedVel = a_this->linearVelocity;
			// Run the game's update function first.
			if (a_this->As<RE::ArrowProjectile>())
			{
				_ArrowProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::BarrierProjectile>())
			{
				_BarrierProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::BeamProjectile>())
			{
				_BeamProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::ConeProjectile>())
			{
				_ConeProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::FlameProjectile>())
			{
				_FlameProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::GrenadeProjectile>())
			{
				_GrenadeProjectile_UpdateImpl(a_this, a_delta);
			}
			else if (a_this->As<RE::MissileProjectile>())
			{
				_MissileProjectile_UpdateImpl(a_this, a_delta);
			}
			else
			{
				_Projectile_UpdateImpl(a_this, a_delta);
			}

			// Ensure projectile and its handle are still valid
			// after running the game's update before continuing.
			if (!a_this->GetHandle() || !Util::HandleIsValid(a_this->GetHandle()))
			{
				projMgr->projectileLock.Unlock();
				return;
			}

			// Extend the projectile's lifetime via smart ptr while we modify its trajectory.
			const auto projectileHandle = a_this->GetHandle();
			auto projectilePtr = Util::GetRefrPtrFromHandle(projectileHandle);
			if (!projectilePtr)
			{
				projMgr->projectileLock.Unlock();
				return;
			}

			bool justReleased = a_this->livingTime == 0.0f;
			int32_t firingPlayerIndex = -1;
			bool firedAtPlayer = false;
			GetFiredAtOrByPlayer(projectileHandle, firingPlayerIndex, firedAtPlayer);
			// Temporarily highlight arrows/bolts shot by players or fired at players.
			if ((justReleased) && (firingPlayerIndex != -1 || firedAtPlayer))
			{
				a_this->ApplyEffectShader(glob.activateHighlightShader, 5.0f);
			}

			RE::Projectile* projectile = nullptr;
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			if (!projectile)
			{
				projMgr->projectileLock.Unlock();
				return;
			}
			
			int32_t grabbedByPlayerPID = -1;
			int32_t releasedByPlayerPID = -1;
			GetManipulatingPlayer(projectileHandle, grabbedByPlayerPID, releasedByPlayerPID);
			// Adjust projectile position if a player has grabbed or released this projectile.
			bool isManipulatedProjectile = grabbedByPlayerPID != -1 || releasedByPlayerPID != -1;
			// Adjust trajectory if fired by the player and just released (will be set as managed),
			// or if the projectile is still managed by the firing player's projectile handler.
			bool isFiredProjectile = 
			(
				(firingPlayerIndex != -1) &&
				(
					justReleased || 
					glob.coopPlayers[firingPlayerIndex]->tm->mph->IsManaged(projectileHandle)
				)
			);
			bool wasAdjusted = false;
			// Prioritize projectile manipulation by the grabbing/releasing player
			// instead of the firing player who should no longer have control of their projectile.
			if (isManipulatedProjectile)
			{
				// Overwrite the projectile's velocity and angular orientation.
				if (grabbedByPlayerPID != -1)
				{
					wasAdjusted = HandleManipulatedProjectile
					(
						glob.coopPlayers[grabbedByPlayerPID],
						projectileHandle,
						true,
						a_this->linearVelocity
					);
				}
				else if (releasedByPlayerPID != -1)
				{
					wasAdjusted = HandleManipulatedProjectile
					(
						glob.coopPlayers[releasedByPlayerPID],
						projectileHandle,
						false,
						a_this->linearVelocity
					);
				}
			}
			else if (isFiredProjectile)
			{
				wasAdjusted = DirectProjectileAtTarget
				(
					glob.coopPlayers[firingPlayerIndex], 
					projectileHandle, 
					a_this->linearVelocity,
					justReleased
				);
			}

			if (!wasAdjusted)
			{
				// Restore saved XY velocity if the projectile's position or velocity
				// was not adjusted this frame.
				a_this->linearVelocity.x = savedVel.x;
				a_this->linearVelocity.y = savedVel.y;
			}

			projMgr->projectileLock.Unlock();
		}

		bool ProjectileHooks::DirectProjectileAtTarget
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const RE::ObjectRefHandle& a_projectileHandle,
			RE::NiPoint3& a_resultingVelocityOut,
			const bool& a_justReleased
		)
		{
			// Adjust projectile trajectory towards the computed intercept position 
			// or the player's current target.
			// Return true if the projectile was directed at the target position.

			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return false;
			}

			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Smart ptr was invalid, so its managed projectile is as well.
			if (!projectile)
			{
				return false;
			}

			bool isManaged = a_p->tm->mph->IsManaged(a_projectileHandle);
			// Beam and flame projectiles have special handling
			// and should not be removed on impact.
			bool isBeamOrFlameProj = 
			(
				projectile->As<RE::BeamProjectile>() || projectile->As<RE::FlameProjectile>()
			);
			// Remove inactive/invalid managed projectiles first.
			// Remove if invalid, not loaded, deleted, marked for deletion, 
			// has collided (if not a beam or flame projectile), limited, or just released.
			bool shouldRemove = 
			{
				(isManaged) &&
				(
					(!projectile->Is3DLoaded()) || 
					(projectile->IsDeleted()) ||
					(projectile->IsMarkedForDeletion()) ||
					(!isBeamOrFlameProj && !projectile->impacts.empty()) || 
					(projectile->ShouldBeLimited())
				)
			};

			// Sometimes the game re-uses the same FID for consecutive projectiles
			// and the projectile FID might still be tagged as managed,
			// so we remove it here before re-inserting below.
			if (shouldRemove)
			{
				a_p->tm->mph->Remove(a_projectileHandle);
				// No longer managed, ready for insertion again if just released.
				isManaged = false;
			}

			// Sometimes Update() is called 2 times consecutively 
			// when the projectile is just released,
			// but we only want to insert the projectile once,
			// so once it is managed, do not insert again.
			if (a_justReleased && !isManaged)
			{
				projectile->shooter = a_p->coopActor->GetHandle();
				auto targetActorHandle = a_p->tm->GetRangedTargetActor();
				auto targetActorPtr = Util::GetRefrPtrFromHandle(targetActorHandle);
				bool targetActorValidity = 
				(
					targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get())
				);
				// Set desired target if needed.
				if (targetActorValidity)
				{
					projectile->desiredTarget = targetActorHandle;
				}
				else
				{
					projectile->desiredTarget = RE::ActorHandle();
				}

				projectile->flags.reset(RE::Projectile::Flags::kNoDamageOutsideCombat);

				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				bool crosshairRefrValidity = 
				(
					crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
				);
				bool canDirectTowardsCrosshairPos = 
				(
					a_p->mm->reqFaceTarget && a_p->tm->aimMode == AimMode::kFreeAim
				);
				// Actor targeted (aim correction or otherwise), 
				// should face crosshair position (never true while mounted), 
				// or mounted and targeting an object.
				bool adjustTowardsTarget = 
				{
					(targetActorPtr != a_p->coopActor) &&
					(
						(targetActorValidity || canDirectTowardsCrosshairPos) || 
						(a_p->coopActor->IsOnMount() && crosshairRefrValidity)
					)
				};
				bool useHoming = 
				{
					adjustTowardsTarget &&
					!isBeamOrFlameProj &&
					Settings::vuProjectileTrajectoryType[a_p->playerID] == 
					!ProjectileTrajType::kHoming
				};
				bool useAimPrediction = 
				{
					adjustTowardsTarget &&
					!isBeamOrFlameProj &&
					Settings::vuProjectileTrajectoryType[a_p->playerID] == 
					!ProjectileTrajType::kPrediction
				};
				if (useHoming)
				{
					a_p->tm->mph->Insert
					(
						a_p, 
						a_projectileHandle,
						a_resultingVelocityOut,
						ProjectileTrajType::kHoming
					);
				}
				else if (useAimPrediction)
				{
					a_p->tm->mph->Insert
					(
						a_p, 
						a_projectileHandle, 
						a_resultingVelocityOut,
						ProjectileTrajType::kPrediction
					);
				}
				else
				{
					a_p->tm->mph->Insert
					(
						a_p, 
						a_projectileHandle, 
						a_resultingVelocityOut, 
						ProjectileTrajType::kAimDirection
					);
				}

				// Set linear velocity field to the launch velocity.
				projectile->linearVelocity = a_resultingVelocityOut;
			}

			// If the projectile is managed, direct it at the target.
			isManaged = a_p->tm->mph->IsManaged(a_projectileHandle);
			if (isManaged)
			{
				const auto& projTrajType = a_p->tm->mph->GetInfo(a_projectileHandle)->trajType; 
				if (projTrajType == ProjectileTrajType::kHoming)
				{
					// Start homing in on the target once the trajectory apex is reached.
					SetHomingTrajectory(a_p, a_projectileHandle, a_resultingVelocityOut);
				}
				else if (isBeamOrFlameProj)
				{
					// Direct in a straight line at the target.
					SetStraightTrajectory(a_p, a_projectileHandle, a_resultingVelocityOut);
				}
				else
				{
					// Release the projectile along a pre-computed trajectory
					// that terminates at a calculated target intercept position (aim prediction) 
					// or at a position far away in the player's aiming direction (aim direction).
					SetFixedTrajectory(a_p, a_projectileHandle, a_resultingVelocityOut);
				}

				return true;
			}

			return false;
		}

		void ProjectileHooks::GetFiredAtOrByPlayer
		(
			const RE::ObjectRefHandle& a_projectileHandle, 
			int32_t& a_firingPlayerPIDOut,
			bool& a_firedAtPlayerOut
		)
		{
			// Store player index (PID) of the player 
			// that released this projectile in one outparam.
			// -1 if not released by a player.
			// Also store whether or not the projectile 
			// was fired at a player in the other outparam.

			// Default to not fired by a player or at a player.
			a_firingPlayerPIDOut = -1;
			a_firedAtPlayerOut = false;

			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Return early if the projectile is not valid.
			if (!projectile || !projectile->actorCause)
			{
				return;
			}

			auto firingActorHandle = 
			(
				projectile->actorCause ? 
				projectile->actorCause->actor : 
				RE::ActorHandle()
			);
			auto firingActorPtr = Util::GetActorPtrFromHandle(firingActorHandle);
			auto firingRefrHandle = projectile->shooter;
			for (const auto& p : glob.coopPlayers) 
			{
				if (!p->isActive || !p->IsRunning()) 
				{
					continue;
				}

				auto playerHandle = p->coopActor->GetHandle();
				// Fired by a player if the firing actor/refr is a player.
				if (firingActorHandle == playerHandle || firingRefrHandle == playerHandle) 
				{
					a_firingPlayerPIDOut = p->playerID;
				}

				// Fired at a player if the projectile's desired target
				// or the firing actor's combat target is a player.
				bool firedAtPlayer = 
				(
					(GlobalCoopData::IsCoopPlayer(projectile->desiredTarget)) ||
					(
						firingActorPtr && 
						Util::HandleIsValid(firingActorPtr->currentCombatTarget) &&
						GlobalCoopData::IsCoopPlayer(firingActorPtr->currentCombatTarget)
					)
				);
				if (firedAtPlayer) 
				{
					a_firedAtPlayerOut = true;
				}

				// If both outparams were set, we can break early.
				if (a_firingPlayerPIDOut != -1 && a_firedAtPlayerOut)
				{
					break;
				}
			}
		}

		void ProjectileHooks::GetManipulatingPlayer
		(
			const RE::ObjectRefHandle& a_projHandle,
			int32_t& a_grabbingPlayerPID, 
			int32_t& a_releasingPlayerPID
		)
		{
			// Store the player PID for the player grabbing/releasing the given projectile 
			// in the outparams (-1 if not by a player).
			// Can only set one PID or the other since any one projectile
			// can only be grabbed or released at a given time.

			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}

				if (p->tm->rmm->IsManaged(a_projHandle, true))
				{
					a_grabbingPlayerPID = p->playerID;
					break;
				}

				if (p->tm->rmm->IsManaged(a_projHandle, false))
				{
					a_releasingPlayerPID = p->playerID;
					break;
				}
			}
		}

		bool ProjectileHooks::HandleManipulatedProjectile
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const RE::ObjectRefHandle& a_projectileHandle, 
			bool a_isGrabbed, 
			RE::NiPoint3& a_resultingVelocityOut
		)
		{
			// Position a grabbed hostile projectile or guide a released projectile
			// along the trajectory set by the grabbing/releasing player's 
			// reference manipulation manager.
			// Update the velocity through the outparam.
			// Return true if the projectile was manipulated.

			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Smart ptr was invalid, so its managed projectile is as well.
			if (!projectile)
			{
				return false;
			}
			
			if (projectile->ShouldBeLimited())
			{
				return false;
			}

			if (a_isGrabbed)
			{
				const auto& rmm = a_p->tm->rmm;
				const auto iter = rmm->grabbedRefrHandlesToInfoIndices.find(a_projectileHandle);
				int32_t index = 
				(
					iter != rmm->grabbedRefrHandlesToInfoIndices.end() ?
					iter->second : 
					-1
				);
				if (index != -1 && index < rmm->grabbedRefrInfoList.size())
				{
					const auto& info = rmm->grabbedRefrInfoList[index];
					if (info->grabTP.has_value() && 
						Util::GetElapsedSeconds(info->grabTP.value()) <= *g_deltaTimeRealTime)
					{
						const auto handle = a_p->coopActor->GetHandle();
						if (projectile->actorCause)
						{
							auto originalShooterPtr = Util::GetActorPtrFromHandle
							(
								projectile->actorCause->actor
							);
						}
					
						// Prevent the grabbed projectile from colliding with its new shooter:
						// the player.
						projectile->SetActorCause(a_p->coopActor->GetActorCause());
						projectile->shooter = handle;
						// Full credits to fenix31415:
						// https://github.com/TESRSkywind/SkywindProjectiles/blob/master/src/Capturing.h#L17
						if (projectile->unk0E0)
						{
							auto refObj = reinterpret_cast<RE::bhkRefObject*>
							(
								projectile->unk0E0
							);
							if (refObj)
							{
								auto worldObj = static_cast<RE::hkpWorldObject*>
								(
									refObj->referencedObject.get()
								);
								if (worldObj)
								{
									auto collidable = worldObj->GetCollidableRW();
									if (collidable)
									{
										RE::CFilter cFilter{ };
										a_p->coopActor->GetCollisionFilterInfo(cFilter);
										auto& collFilterInfo = 
										(
											collidable->broadPhaseHandle.collisionFilterInfo.filter
										);
										collFilterInfo &= (0x0000FFFF);
										collFilterInfo |= (cFilter.filter << 16);
									}
								}
							}
						}
					}
					
					const auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle
					(
						a_p->tm->aimCorrectionTargetHandle
					);
					if (a_p->tm->aimMode == AimMode::kTwinStick && aimCorrectionTargetPtr)
					{
						auto targetPos = Util::GetTorsoPosition(aimCorrectionTargetPtr.get());
						// Set projectile data angles to face the target.
						projectile->data.angle.x = Util::GetPitchBetweenPositions
						(
							projectile->data.location, targetPos
						);
						projectile->data.angle.z = Util::GetYawBetweenPositions
						(
							projectile->data.location, targetPos
						);
						// Set rotation matrix to maintain consistency 
						// with the previously set refr data angles.
						auto current3DPtr = Util::GetRefr3D(projectile); 
						if (current3DPtr)
						{
							Util::SetRotationMatrixPY
							(
								current3DPtr->local.rotate, 
								projectile->data.angle.x, 
								projectile->data.angle.z
							);
						}
					}
					else if (a_p->tm->aimMode != AimMode::kTwinStick && a_p->mm->reqFaceTarget)
					{
						// Set projectile data angles to face the target.
						projectile->data.angle.x = Util::GetPitchBetweenPositions
						(
							projectile->data.location,
							a_p->tm->crosshairWorldPos
						);
						projectile->data.angle.z = Util::GetYawBetweenPositions
						(
							projectile->data.location,
							a_p->tm->crosshairWorldPos
						);
						// Set rotation matrix to maintain consistency 
						// with the previously set refr data angles.
						auto current3DPtr = Util::GetRefr3D(projectile); 
						if (current3DPtr)
						{
							Util::SetRotationMatrixPY
							(
								current3DPtr->local.rotate, 
								projectile->data.angle.x, 
								projectile->data.angle.z
							);
						}
					}
					else
					{
						// Set projectile data angles to face the player's facing direction.
						projectile->data.angle.x = a_p->mm->aimPitch;
						projectile->data.angle.z = a_p->coopActor->GetHeading(false);
						// Set rotation matrix to maintain consistency 
						// with the previously set refr data angles.
						auto current3DPtr = Util::GetRefr3D(projectile); 
						if (current3DPtr)
						{
							Util::SetRotationMatrixPY
							(
								current3DPtr->local.rotate, 
								projectile->data.angle.x, 
								projectile->data.angle.z
							);
						}
					}

					// Set velocity.
					projectile->linearVelocity = a_resultingVelocityOut = info->lastSetVelocity;
					return true;
				}
			}
			else
			{
				const auto& rmm = a_p->tm->rmm;
				const auto iter = rmm->releasedRefrHandlesToInfoIndices.find(a_projectileHandle);
				int32_t index = 
				(
					iter != rmm->releasedRefrHandlesToInfoIndices.end() ?
					iter->second : 
					-1
				);
				if (index != -1 && index < rmm->releasedRefrInfoList.size())
				{
					const auto& info = rmm->releasedRefrInfoList[index];
					if (info->releaseTP.has_value())
					{
						if (info->isThrown)
						{
							const auto handle = a_p->coopActor->GetHandle();
							float t = Util::GetElapsedSeconds(info->releaseTP.value());
							if (t <= *g_deltaTimeRealTime)
							{
								projectile->shooter = handle;
								projectile->desiredTarget = info->targetRefrHandle;
							}

							auto velToSet = info->GuideRefrAlongTrajectory(a_p);
							// No need to set velocity if 0. Maintain velocity from the last frame.
							if (velToSet.Length() == 0.0f)
							{
								return true;
							}

							// Cap speed to the release speed, as with other refrs.
							info->ApplyVelocity(velToSet);
							projectile->data.angle.x = Util::DirectionToGameAngPitch(velToSet);
							projectile->data.angle.z = Util::DirectionToGameAngYaw(velToSet);

							// Set rotation matrix to maintain consistency 
							// with the previously set refr data angles.
							auto current3DPtr = Util::GetRefr3D(projectile); 
							if (current3DPtr)
							{
								Util::SetRotationMatrixPY
								(
									current3DPtr->local.rotate, 
									projectile->data.angle.x, 
									projectile->data.angle.z
								);
							}

							// Set velocity.
							projectile->linearVelocity = a_resultingVelocityOut = velToSet;
						}
						else
						{
							// Point in direction of velocity.
							projectile->data.angle.x = Util::DirectionToGameAngPitch
							(
								a_resultingVelocityOut
							);
							projectile->data.angle.z = Util::DirectionToGameAngYaw
							(
								a_resultingVelocityOut
							);
						
							// Set rotation matrix to maintain consistency 
							// with the previously set refr data angles.
							auto current3DPtr = Util::GetRefr3D(projectile); 
							if (current3DPtr)
							{
								Util::SetRotationMatrixPY
								(
									current3DPtr->local.rotate, 
									projectile->data.angle.x, 
									projectile->data.angle.z
								);
							}
						}

						return true;
					}
				}
			}

			// Not manipulated by a player.
			return false;
		}

		void ProjectileHooks::SetHomingTrajectory
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const RE::ObjectRefHandle& a_projectileHandle,
			RE::NiPoint3& a_resultingVelocityOut)
		{
			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Smart ptr was invalid, so its managed projectile is as well.
			if (!projectile)
			{
				return;
			}

			// Not a managed projectile, so nothing to do here.
			if (!a_p->tm->mph->IsManaged(a_projectileHandle))
			{
				return;
			}
			
			// Guaranteed to be managed here.
			auto& managedProjInfo = a_p->tm->mph->GetInfo(a_projectileHandle);
			RE::NiPoint3 velToSet = a_resultingVelocityOut;
			RE::NiPoint3 aimTargetPos = a_p->tm->crosshairWorldPos;
			auto targetRefrPtr = Util::GetRefrPtrFromHandle(managedProjInfo->targetRefrHandle);
			bool targetRefrValidity = 
			(
				targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
			);
			auto targetActorPtr = 
			(
				targetRefrValidity ? RE::ActorPtr(targetRefrPtr->As<RE::Actor>()) : nullptr
			);
			bool targetActorValidity = 
			(
				targetActorPtr && 
				Util::IsValidRefrForTargeting(targetActorPtr.get())
			);
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool crosshairRefrIsTarget = 
			{
				crosshairRefrPtr && 
				crosshairRefrPtr == targetRefrPtr && 
				Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
			};
			if (targetRefrValidity)
			{
				if (targetActorValidity)
				{
					aimTargetPos = Util::GetTorsoPosition(targetActorPtr.get());
				}
				else
				{
					aimTargetPos = Util::GetRefrPosition(targetRefrPtr.get());
				}
				
				// Choose the exact crosshair position locally offset from the target actor;
				// otherwise, if not facing the crosshair, target the selected actor's torso.
				// Done to maximize hit chance, since an actor's torso node is most likely 
				// to be within their character controller collider.
				if (crosshairRefrIsTarget && a_p->mm->reqFaceTarget)
				{
					// Targeted with crosshair.
					// Direct at crosshair position offset from the target refr.
					aimTargetPos += a_p->tm->crosshairLocalPosOffset;
				}
			}
			
			// Saved pitch/yaw at launch.
			const float& launchPitch = managedProjInfo->launchPitch;
			const float& launchYaw = managedProjInfo->launchYaw;
			// Pitch and yaw to the target.
			float pitchToTarget = Util::GetPitchBetweenPositions
			(
				projectile->data.location, aimTargetPos
			);
			float yawToTarget = Util::GetYawBetweenPositions
			(
				projectile->data.location, aimTargetPos
			);
			// Predicted time to target along the initial fixed trajectory.
			const float initialTimeToTargetSecs = managedProjInfo->initialTrajTimeToTarget;
			// Get the expected lifetime for the projectile before it potentially despawns.
			float expectedLifetime = FLT_MAX;
			auto base = projectile->GetProjectileBase();
			if (base)
			{
				if (base->data.lifetime > 0.0f)
				{
					expectedLifetime = base->data.lifetime;
				}
				else
				{
					expectedLifetime = base->data.range / base->data.speed;
				}
			}

			if (projectile->livingTime == 0.0f)
			{
				// Set launch angles on release.
				// Set refr data angles.
				projectile->data.angle.x = -launchPitch;
				projectile->data.angle.z = launchYaw;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				auto current3DPtr = Util::GetRefr3D(projectile); 
				if (current3DPtr)
				{
					Util::SetRotationMatrixPY
					(
						current3DPtr->local.rotate,
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}
			}

			float distToTarget = projectile->data.location.GetDistance(aimTargetPos);
			// Continue setting the trajectory while the projectile 
			// is > 1 length away from the target.
			bool continueSettingTrajectory = distToTarget > max(1.0f, projectile->GetHeight());
			// Exit early if too close to the target.
			if (!continueSettingTrajectory)
			{
				// Set as no longer managed to prevent further trajectory adjustment.
				a_p->tm->mph->Remove(a_projectileHandle);
				return;
			}

			const RE::NiPoint3& releasePos = managedProjInfo->releasePos;
			float currentPitch = Util::NormalizeAngToPi
			(
				Util::DirectionToGameAngPitch(a_resultingVelocityOut)
			);
			float currentYaw = Util::NormalizeAng0To2Pi
			(
				Util::DirectionToGameAngYaw(a_resultingVelocityOut)
			);
			// Ensure current pitch and yaw are valid.
			if (isnan(currentPitch) || isinf(currentPitch))
			{
				currentPitch = -launchPitch;
			}
			if (isnan(currentYaw) || isinf(currentYaw))
			{
				currentYaw = Util::ConvertAngle(launchYaw);
			}

			// Pitch and yaw to set below.
			float pitchToSet = currentPitch;
			float yawToSet = currentYaw;

			// Just launched, so set pitch and yaw to saved launch values,
			// and adjust velocity to point in the direction given by these two launch angles.
			if (projectile->livingTime == 0.0f)
			{
				currentPitch = pitchToSet = -launchPitch;
				currentYaw = yawToSet = Util::ConvertAngle(launchYaw);
				a_resultingVelocityOut = 
				(
					Util::RotationToDirectionVect(launchPitch, launchYaw) * 
					a_resultingVelocityOut.Length()
				);
			}

			// Pitch and yaw angle deltas in order to face the target.
			float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - currentPitch);
			float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentYaw);

			// Trajectory data.
			const float& g = managedProjInfo->g;
			const double& mu = managedProjInfo->mu;
			const float& t = projectile->livingTime;
			// Cap in-flight time.
			const bool tooLongToReach = 
			(
				initialTimeToTargetSecs == 0.0f ||
				initialTimeToTargetSecs >= min
				(
					expectedLifetime, Settings::fMaxProjAirborneSecsToTarget
				)
			);
			// Cannot split the trajectory into two parts 
			// if the projectile reaches the target in under two frames,
			// so we'll start homing in right away.
			const bool lessThanTwoFramesToReachTarget = 
			(
				initialTimeToTargetSecs <= *g_deltaTimeRealTime * 2.0f
			);


			// Release speed for fixed trajectory determined by projectile launch data.
			const float& releaseSpeed = managedProjInfo->releaseSpeed;
			// Get velocity along fixed trajectory and speed.
			// Without air resistance.
			// XY, X, Y, and Z components of velocity.
			const float velXY = releaseSpeed * cosf(launchPitch);
			const float velX = velXY * cosf(launchYaw);
			const float velY = velXY * sinf(launchYaw);
			const float velZ = releaseSpeed * sinf(launchPitch) - g * t;
			auto fixedTrajVel = RE::NiPoint3(velX, velY, velZ);
			// Speed to set below.
			float speedToSet = fixedTrajVel.Length();
			
			if (!managedProjInfo->startedHomingIn)
			{
				// With air resistance.
				// NOTE:
				// Not in use right now, simply because the extra computational overhead
				// is not worth producing a marginally more realistic first half 
				// of the homing projectile's trajectory.
				
				/*
				float vx0 = releaseSpeed * cosf(launchPitch);
				float vy0 = releaseSpeed * sinf(launchPitch);
				// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
				float xyD = (vx0 / mu) * (1 - exp(-mu * t));
				//float zD = (-g * t / mu) + (1.0f / mu) * (vy0 + g / mu) * (1.0f - exp(-mu * t));
				// Flip of the game's pitch sign.
				const float pitchOnTraj = 
				(
					atan2f
					(
						(g / (mu * releaseSpeed * cosf(launchPitch))) * 
						(
							1.0f - 
							(
								1.0f / (1.0f - ((xyD * mu) / (releaseSpeed * cosf(launchPitch))))
							)
						) + tanf(launchPitch), 
						1.0f
					)
				);
				*/

				// Next, check if the homing projectile should fully start homing in on the target
				// instead of following its initial fixed trajectory.
				
				// Fixed trajectory XY position and pitch along the trajectory (tangent line).
				const float xy = releaseSpeed * t * cosf(launchPitch);
				const float pitchOnTraj = 
				(
					-atan2f
					(
						tanf(launchPitch) - (g * xy) /
						powf(releaseSpeed * cosf(launchPitch), 2.0f), 
						1.0f
					)
				);
				// Changes smaller than this value are ignored.
				const float epsilon = 1E-3f;
				// Last point at which the projectile can stay on its fixed trajectory 
				// before homing in.
				bool passedHalfwayPoint = 
				(
					projectile->livingTime - 0.5f * initialTimeToTargetSecs >= -epsilon ||
					xy > Util::GetXYDistance(releasePos, managedProjInfo->trajectoryEndPos)
				);
				bool noTargetAndMovingCrosshair =
				(
					!targetRefrValidity &&
					a_p->tm->aimMode == AimMode::kFreeAim &&
					a_p->mm->reqFaceTarget &&
					a_p->pam->IsPerforming(InputAction::kMoveCrosshair)
				);
				// Set as homing if not already set 
				// and one of the above conditions is true.
				bool shouldSetAsHoming =
				(
					(!managedProjInfo->startedHomingIn) && 
					(
						noTargetAndMovingCrosshair || 
						passedHalfwayPoint || 
						tooLongToReach ||
						lessThanTwoFramesToReachTarget
					)
				);
				if (shouldSetAsHoming)
				{
					managedProjInfo->startedHomingIn = true;
					managedProjInfo->startedHomingTP = SteadyClock::now();
				}
				else
				{
					float nextT = (t + *g_deltaTimeRealTime);
					auto targetPos = RE::NiPoint3
					(
						releasePos.x + releaseSpeed * cosf(launchPitch) * cosf(launchYaw) * nextT,
						releasePos.y + releaseSpeed * cosf(launchPitch) * sinf(launchYaw) * nextT,
						releasePos.z + 
						releaseSpeed * sinf(launchPitch) * nextT - 
						0.5f * g * nextT * nextT
					);
					velToSet = (targetPos - projectile->data.location) / *g_deltaTimeRealTime;
					pitchToSet = Util::DirectionToGameAngPitch(velToSet);
					yawToSet = Util::DirectionToGameAngYaw(velToSet);
					speedToSet = velToSet.Length();
				}
			}

			// NOTE: 
			// Might uncomment eventually if air resistance is desired during the fixed portion
			// of the homing trajectory.
			// Right now, it makes a negligible difference that is not worth the extra computation,
			// especially since only at most half of the projectile's time of flight
			// is spent along the fixed initial trajectory.
			//
			// With air resistance.
			/*
			const float velXY = releaseSpeed * cosf(launchPitch) * exp(-mu * t);
			const float velX = velXY * cosf(launchYaw);
			const float velY = velXY * sinf(launchYaw);
			const float velZ = 
			(
				-g / mu + ((releaseSpeed * sinf(launchPitch) + g / mu) * exp(-mu * t))
			);
			auto fixedTrajVel = RE::NiPoint3(velX, velY, velZ);
			float speed = fixedTrajVel.Length();
			*/
			
			// Max distance the projectile will travel in 1 frame at its current velocity.
			float maxDistPerFrame = 
			(
				max(projectile->GetSpeed(), a_resultingVelocityOut.Length()) * *g_deltaTimeRealTime
			);
			// Velocity mult which slows down the projectile when close to the target 
			// to minimize overshooting and jarring course correction.
			float distSlowdownFactor = std::clamp
			(
				powf(distToTarget / (maxDistPerFrame + 0.01f), 5.0f), 0.1f, 1.0f
			);
			// Projectile is now homing in, smooth out pitch and yaw to follow the target.
			// Direction from the current position to the target.
			auto dirToTarget = aimTargetPos - projectile->data.location;
			dirToTarget.Unitize();
			// Last frame's velocity direction.
			auto velDirLastFrame = a_resultingVelocityOut;
			velDirLastFrame.Unitize();
			// Angle between last frame's velocity and the target.
			float angBetweenVelAndToTarget = acosf
			(
				std::clamp(dirToTarget.Dot(velDirLastFrame), -1.0f, 1.0f)
			);

			// Went past the target if velocity direction and direction to target 
			// diverge by >= 90 degrees or the distance to the target 
			// is less than the max distance travelable per frame.
			bool wentPastTarget = 
			(
				angBetweenVelAndToTarget >= PI / 2.0f && distToTarget <= maxDistPerFrame
			);

			if (managedProjInfo->startedHomingIn)
			{
				// First, check if the projectile has moved past the target.

				
				float secsSinceStartedHoming = Util::GetElapsedSeconds
				(
					managedProjInfo->startedHomingTP.value(), true
				);
				// Can't hit target with given launch pitch, 
				// so set pitch and yaw directly to target right away.
				if (tooLongToReach || lessThanTwoFramesToReachTarget)
				{
					// Turn directly to face the target once homing starts, 
					// so the target is no longer behind the projectile.
					// If the projectile eventually goes past the target again, 
					// it won't turn around to face it anymore.
					if (secsSinceStartedHoming == 0.0f)
					{
						wentPastTarget = false;
					}
				}

				if (lessThanTwoFramesToReachTarget)
				{
					pitchToSet = pitchToTarget;
					yawToSet = yawToTarget;
				}
				else 
				{
					float timeToFullyHomeIn = initialTimeToTargetSecs;
					if (tooLongToReach)
					{
						// Home in completely at most 3 seconds post-launch.
						timeToFullyHomeIn = min
						(
							3.0f, min(initialTimeToTargetSecs, expectedLifetime) * 0.75f
						);
					}
					
					// Turn gradually to face.
					float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - currentPitch);
					pitchToSet = Util::NormalizeAngToPi
					(
						currentPitch + 
						Util::InterpolateSmootherStep
						(
							0.0f, 
							pitchDiff,
							min(1.0f, projectile->livingTime / timeToFullyHomeIn)
						)
					);
					float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentYaw);
					yawToSet = Util::NormalizeAng0To2Pi
					(
						currentYaw + 
						Util::InterpolateSmootherStep
						(
							0.0f, 
							yawDiff,
							min(1.0f, projectile->livingTime / timeToFullyHomeIn)
						)
					);
				}

				// Projectile base speed when launched.
				// Modify the current speed by the distance-slowdown factor, 
				// but set a lower bound to avoid instances where the projectile
				// does 0 damage when hitting the target at too low of a speed.
				RE::NiPoint3 targetLinVel{ };
				if (targetActorValidity)
				{
					targetLinVel = Util::GetActorLinearVelocity(targetActorPtr.get());
				}
				else if (targetRefrValidity)
				{
					targetRefrPtr->GetLinearVelocity(targetLinVel);
				}

				speedToSet = max
				(
					speedToSet * distSlowdownFactor, 
					min(releaseSpeed, 1000.0f) + targetLinVel.Length()
				);

				// Continue homing in only if the projectile has not gone past the target.
				continueSettingTrajectory = !wentPastTarget;
			}
			
			if (continueSettingTrajectory)
			{
				// Set refr data angles.
				projectile->data.angle.x = pitchToSet;
				projectile->data.angle.z = yawToSet;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				auto current3DPtr = Util::GetRefr3D(projectile); 
				if (current3DPtr)
				{
					Util::SetRotationMatrixPY
					(
						current3DPtr->local.rotate,
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}
		
				velToSet = 
				(
					Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) * 
					speedToSet
				);
			
				// Set velocity.
				a_resultingVelocityOut = velToSet;
				projectile->linearVelocity = a_resultingVelocityOut;
			}
			else
			{
				// Set as no longer managed to prevent further trajectory adjustment 
				// after this frame.
				a_p->tm->mph->Remove(a_projectileHandle);
			}
		}

		void ProjectileHooks::SetFixedTrajectory
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const RE::ObjectRefHandle& a_projectileHandle,
			RE::NiPoint3& a_resultingVelocityOut
		)
		{
			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Smart ptr was invalid, so its managed projectile is as well.
			if (!projectile)
			{
				return;
			}

			// Not a managed projectile, so nothing to do here.
			if (!a_p->tm->mph->IsManaged(a_projectileHandle))
			{
				return;
			}

			// Guaranteed to be managed here.
			const auto& managedProjInfo = a_p->tm->mph->GetInfo(a_projectileHandle);
			// Pre-computed fixed trajectory data.
			const RE::NiPoint3& releasePos = managedProjInfo->releasePos;
			const RE::NiPoint3& targetPos = managedProjInfo->trajectoryEndPos;
			const float& launchPitch = managedProjInfo->launchPitch;
			const float& launchYaw = managedProjInfo->launchYaw;
			const float& releaseSpeed = managedProjInfo->releaseSpeed;
			const float& g = managedProjInfo->g;
			const double& mu = managedProjInfo->mu;
			const float& t = projectile->livingTime;

			// Immediately direct at the target position
			// if the initial time to hit the target is less than one frame.
			// Won't have multiple frames to move towards the intercept position, 
			// so do it here right after release.
			if (managedProjInfo->initialTrajTimeToTarget <= *g_deltaTimeRealTime)
			{
				// Pitch and yaw to the target.
				float pitchToTarget = Util::GetPitchBetweenPositions
				(
					projectile->data.location, targetPos
				);
				float yawToTarget = Util::GetYawBetweenPositions
				(
					projectile->data.location, targetPos
				);
				// Set refr data angles.
				projectile->data.angle.x = pitchToTarget;
				projectile->data.angle.z = yawToTarget;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				auto current3DPtr = Util::GetRefr3D(projectile); 
				if (current3DPtr)
				{
					Util::SetRotationMatrixPY
					(
						current3DPtr->local.rotate,
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}

				// Set velocity, maintaining original speed.
				auto velToSet =
				(
					Util::RotationToDirectionVect
					(
						-pitchToTarget, Util::ConvertAngle(yawToTarget)
					) * a_resultingVelocityOut.Length()
				);

				a_resultingVelocityOut = velToSet;
				projectile->linearVelocity = a_resultingVelocityOut;

				// No longer handled after directing at the target position.
				a_p->tm->mph->Remove(a_projectileHandle);
				return;
			}
			else if (projectile->livingTime == 0.0f)
			{
				// Set launch angles on release.
				// Set refr data angles.
				projectile->data.angle.x = -launchPitch;
				projectile->data.angle.z = launchYaw;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				auto current3DPtr = Util::GetRefr3D(projectile); 
				if (current3DPtr)
				{
					Util::SetRotationMatrixPY
					(
						current3DPtr->local.rotate,
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}
			}

			// NOTE: 
			// Since the frametime is discrete and certain projectiles move extremely fast,
			// we cannot use the true velocity at any particular time computed
			// from the trajectory's formulas. 
			// We have to instead "connect the dots" between the current projectile position 
			// and the next expected projectile position one frame later 
			// to ensure that it will arrive at the trajectory's endpoint.
			// If the frametimes vary greatly from frame to frame,
			// the position and velocity calculations will not conform as well
			// to the original trajectory, speeding up and slowing down along the path.
			// 
			// Factors in linear air resistance.
			// May remove eventually.
			//
			// Initial X, Y components of velocity.
			float vx0 = releaseSpeed * cosf(launchPitch);
			float vy0 = releaseSpeed * sinf(launchPitch);
			// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
			// XY, and Z positions: 
			// in 2D plane, XY pos is the X coordinate,
			// and Z pos is the Y coordinate.
			float currXY = (vx0 / mu) * (1 - exp(-mu * t));
			float currZ = (-g * t / mu) + (1.0f / mu) * (vy0 + g / mu) * (1.0f - exp(-mu * t));
			// Next projected XY and Z offsets.
			float nextXY = (vx0 / mu) * (1 - exp(-mu * (t + *g_deltaTimeRealTime)));
			float nextZ = 
			(
				(-g * (t + *g_deltaTimeRealTime) / mu) + 
				(1.0f / mu) * 
				(vy0 + g / mu) * 
				(1.0f - exp(-mu * (t + *g_deltaTimeRealTime)))
			);

			// '+' means up in the XY plane, and '-' means down.
			// Pitch to face the next frame's expected position.
			const float pitchOnTraj = atan2f((nextZ - currZ), (nextXY - currXY));
			// Get the estimated speed from dividing the distance 
			// between the two positions by the current frame time.
			const float speedToSet = 
			(
				Util::GetXYDistance(currXY, currZ, nextXY, nextZ) / *g_deltaTimeRealTime
			);
			auto vel = Util::RotationToDirectionVect(pitchOnTraj, launchYaw) * speedToSet;

			// Set our computed velocity and pitch/yaw.
			a_resultingVelocityOut = vel;
			projectile->linearVelocity = a_resultingVelocityOut;
			// Yaw will not change throughout.
			projectile->data.angle.z = Util::ConvertAngle(launchYaw);
			// Pitch is equal to the pitch along the trajectory.
			// NOTE: 
			// Sign flipped, since Skyrim's sign convention for pitch is < 0 
			// when facing up and > 0 when facing down.
			projectile->data.angle.x = -pitchOnTraj;

			// Set rotation matrix to maintain consistency 
			// with the previously set refr data angles.
			auto current3DPtr = Util::GetRefr3D(projectile); 
			if (current3DPtr)
			{
				Util::SetRotationMatrixPY
				(
					current3DPtr->local.rotate, projectile->data.angle.x, projectile->data.angle.z
				);
			}
		}

		void ProjectileHooks::SetStraightTrajectory
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const RE::ObjectRefHandle& a_projectileHandle, 
			RE::NiPoint3& a_resultingVelocityOut
		)
		{
			// Direct flame and beam projectiles in a straight line to the target position,
			// which changes to track the target. Velocity does not change.

			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Smart ptr was invalid, so its managed projectile is as well.
			if (!projectile || projectile->livingTime > 0.0f)
			{
				return;
			}

			// Not a managed projectile, so nothing to do here.
			if (!a_p->tm->mph->IsManaged(a_projectileHandle))
			{
				return;
			}

			auto& managedProjInfo = a_p->tm->mph->GetInfo(a_projectileHandle);
			// Aim at the previously computed trajectory end position by default.
			RE::NiPoint3 aimTargetPos = managedProjInfo->trajectoryEndPos;
			const auto targetActorHandle = a_p->tm->GetRangedTargetActor();
			auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
			bool targetActorValidity = 
			(
				targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get())
			);
			if (targetActorValidity) 
			{
				// Aim at the locally offset crosshair hit position or the target actor's torso.
				if (a_p->mm->reqFaceTarget) 
				{
					aimTargetPos = 
					(
						Util::GetTorsoPosition(targetActorPtr.get()) + 
						a_p->tm->crosshairLocalPosOffset
					);
				}
				else
				{
					aimTargetPos = Util::GetTorsoPosition(targetActorPtr.get());
				}

				projectile->desiredTarget = targetActorHandle;
			}
			else if (a_p->mm->reqFaceTarget && a_p->tm->aimMode != AimMode::kTwinStick)
			{
				// Aim at the crosshair world position that the player is facing.
				aimTargetPos = a_p->tm->crosshairWorldPos;
			}
			else
			{
				// Aim far away in the player's facing direction.
				double farDist = FLT_MAX;
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = 
				(
					iniPrefSettings ? 
					iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : 
					nullptr
				);
				if (projMaxDistSetting && 
					projectile->data.location.GetDistance(a_p->tm->crosshairWorldPos) < 
					projMaxDistSetting->data.f)
				{
					farDist = projMaxDistSetting->data.f;
				}
				else
				{
					farDist = projectile->data.location.GetDistance(a_p->tm->crosshairWorldPos);
				}

				aimTargetPos = 
				(
					projectile->data.location +
					Util::RotationToDirectionVect
					(
						-a_p->mm->aimPitch, //Util::ConvertAngle(a_p->coopActor->data.angle.z)
						Util::ConvertAngle(projectile->data.angle.z)
					) * farDist
				);
			}

			float pitchToSet = 
			(
				Util::GetPitchBetweenPositions(projectile->data.location, aimTargetPos)
			);
			float yawToSet = Util::GetYawBetweenPositions(projectile->data.location, aimTargetPos);
			projectile->data.angle.x = pitchToSet;
			projectile->data.angle.z = yawToSet;

			// Set rotation matrix to maintain consistency 
			// with the previously set refr data angles.
			auto current3DPtr = Util::GetRefr3D(projectile); 
			if (current3DPtr)
			{
				Util::SetRotationMatrixPY
				(
					current3DPtr->local.rotate, projectile->data.angle.x, projectile->data.angle.z
				);
				auto parentPtr = RE::NiPointer<RE::NiAVObject>(current3DPtr->parent); 
				if (parentPtr) 
				{
					current3DPtr->world = parentPtr->world * current3DPtr->local;
				}
				else
				{
					current3DPtr->world = current3DPtr->local;
				}
			}

			// Have the game pick a target for the projectile
			// after directing it at the target.
			if (projectile->livingTime == 0.0f) 
			{
				projectile->RunTargetPick();
			}
		}

// [SPELLITEM HOOKS]:
		void SpellItemHooks::AdjustCost
		(
			RE::SpellItem* a_this, float& a_cost, RE::Actor* a_actor
		)
		{
			// IMPORTANT:
			// Not called before casting in AE.
		
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _AdjustCost(a_this, a_cost, a_actor);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_actor); 
			if (playerIndex == -1)
			{
				return _AdjustCost(a_this, a_cost, a_actor);
			}

			const auto& p = glob.coopPlayers[playerIndex];
			// Not P1.
			if (glob.menuPID > 0)
			{
				// Trying to get P1's spell costs 
				// while a companion player is accessing a menu.
				if (p->coopActor->IsPlayerRef())
				{
					const auto& menuP = glob.coopPlayers[glob.menuPID];
					// Run the adjust cost function on the companion player's base cost
					// for this spell and scale it up/down.
					// Output this new cost to report the companion player's spell costs
					// in the menu instead of P1's.
					float menuPlayerMagCost = 
					(
						// This function gets called on the menu player here.
						a_this->CalculateMagickaCost(menuP->coopActor.get())
					);
					// Do not run the adjustment a second time and just scale down
					// the base cost here.
					a_cost = 
					(
						menuPlayerMagCost * Settings::vfMagickaCostMult[menuP->playerID]
					);
				}
				else
				{
					// Calling CalculateMagickaCost() above on the companion player
					// will call this function, so to avoid creating 
					// an infinite recursive loop and to obtain the original cost, 
					// run the original function here to compute the pre-scaled cost 
					// and return.
					_AdjustCost(a_this, a_cost, a_actor);
				}
			}
			else
			{
				// Scale the base cost by our P1-specific multiplier first.
				a_cost *= Settings::vfMagickaCostMult[p->playerID];
				_AdjustCost(a_this, a_cost, a_actor);
			}
		}

// [TESCAMERA HOOKS]:
		void TESCameraHooks::Update(RE::TESCamera* a_this)
		{
			// Switch to third person when dead, which will keep the HUD and its messages visible.
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			auto playerCam = RE::PlayerCamera::GetSingleton();
			/*
			bool switchToDeathCam = 
			{
				(
					p1 &&
					playerCam &&
					glob.globalDataInit && 
					glob.allPlayersInit &&
					!glob.cam->IsRunning() &&
					glob.partyWiped	
				) &&
				(
					(glob.p1IsEssential && p1->IsBleedingOut()) || 
					(!glob.p1IsEssential && p1->IsDead())
				)
			};
			if (switchToDeathCam)
			{
				if (a_this->currentState->id == RE::CameraState::kBleedout)
				{
					const auto& tpStatePtr = playerCam->cameraStates
					[
						RE::CameraState::kThirdPerson
					];
					if (tpStatePtr)
					{
						a_this->SetState(tpStatePtr.get());
					}

					playerCam->lock.Lock();
					playerCam->ForceThirdPerson();
					playerCam->UpdateThirdPerson(true);
					playerCam->lock.Unlock();
					glob.cam->deathCameraTP = SteadyClock::now();
				}
				else
				{
					glob.cam->UpdateDeathCameraOrientation();
				}

				return;
			}
			*/

			if (glob.globalDataInit &&
				glob.coopSessionActive && 
				!glob.cam->IsRunning() &&
				a_this->currentState->id == RE::CameraState::kBleedout)
			{
				_Update(a_this);
				auto bleedoutState = skyrim_cast<RE::BleedoutCameraState*>
				(
					a_this->currentState.get()
				);
				auto temp = 
				(
					playerCam ? playerCam->cameraStates[RE::CameraState::kThirdPerson] : nullptr
				);
				auto tpState = 
				(
					temp ? skyrim_cast<RE::ThirdPersonState*>(temp.get()) : nullptr
				);
				if (tpState && bleedoutState)
				{
					bleedoutState->randHeading = false;
					bleedoutState->useCurrentHeading = true;
					bleedoutState->targetZoomOffset = tpState->targetZoomOffset;
					bleedoutState->currentZoomOffset = tpState->currentZoomOffset;
					bleedoutState->pitchZoomOffset = tpState->pitchZoomOffset;
					bleedoutState->applyOffsets = tpState->applyOffsets;
					bleedoutState->posOffsetActual = tpState->posOffsetActual;
					bleedoutState->posOffsetExpected = tpState->posOffsetExpected;
				}
				return;
			}

			if (!glob.globalDataInit || 
				!glob.allPlayersInit || 
				!glob.cam->IsRunning() || 
				!p1)
			{
				return _Update(a_this);
			}
			
			// Camera local position/rotation is modified when ragdolled 
			// (bleedout camera position), inactive, staggered, sitting/sleeping, 
			// sprinting,when camera shake is applied (AnimatedCameraDelta), 
			// or when the death camera state is active,
			// and we want to discard these position/rotation changes, 
			// so return without updating.			
			bool orbitStateActive = a_this->currentState->id == RE::CameraState::kAutoVanity;
			bool bleedoutStateActive = a_this->currentState->id == RE::CameraState::kBleedout;
			bool furnitureStateActive = a_this->currentState->id == RE::CameraState::kFurniture;
			bool localRotationModified = 
			{
				orbitStateActive ||
				bleedoutStateActive ||
				furnitureStateActive ||
				p1->IsInRagdollState() ||
				p1->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
				p1->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
				glob.coopPlayers[0]->pam->isSprinting ||
				glob.isCameraShakeActive || 
				glob.cam->inDeathCamState
			};
			if (localRotationModified) 
			{
				glob.cam->SetCamOrientation(true);
				return;
			}
			else
			{
				// Otherwise, run the original func first to allow other plugins 
				// that hook this func to execute their logic first 
				// before we re-apply the co-op camera orientation,
				// which was previously applied in the main hook.
				_Update(a_this);
				glob.cam->SetCamOrientation(false);
				return;
			}
		}

// [TESOBJECTBOOK HOOKS]:
		bool TESObjectBOOKHooks::Activate
		(
			RE::TESObjectBOOK* a_this,
			RE::TESObjectREFR* a_targetRef,
			RE::TESObjectREFR* a_activatorRef, 
			std::uint8_t a_arg3, 
			RE::TESBoundObject* a_object,
			std::int32_t a_targetCount
		)
		{
			if (!glob.globalDataInit ||
				!glob.coopSessionActive || 
				!a_targetRef || 
				!GlobalCoopData::IsCoopPlayer(a_activatorRef))
			{
				return 
				(
					_Activate
					(
						a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount
					)
				);
			}

			// Is an unread skillbook.
			auto book = 
			(
				a_targetRef->GetBaseObject() ? 
				a_targetRef->GetBaseObject()->As<RE::TESObjectBOOK>() : 
				nullptr
			); 
			if (book && !book->IsRead() && book->TeachesSkill())
			{
				// Level up co-op companion players only,
				// since P1 levels up when the BookMenu is triggered on activation.
				for (const auto& p : glob.coopPlayers)
				{
					if (!p->isActive || p->isPlayer1)
					{
						continue;
					}

					p->pam->LevelUpSkillWithBook(book);
				}
			}

			return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
		}

// [TESOBJECTREFR HOOKS]:
		void TESObjectREFRHooks::AddObjectToContainer
		(
			RE::TESObjectREFR* a_this,
			RE::TESBoundObject* a_object,
			RE::ExtraDataList* a_extraList, 
			std::int32_t a_count, 
			RE::TESObjectREFR* a_fromRefr
		)
		{
			// The chest must only have items with at least one extra data list per entry.
			// Thus, the key here is to make sure the requested item has at least 1 extra data list
			// by adding ownership extra data to any item that has none.
			// Fix up the counts afterward to prevent instability.
						
			if (!a_object || a_count == 0 || !glob.globalDataInit || !glob.allPlayersInit)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}

			const auto pIndex = GlobalCoopData::GetCoopPlayerIndexFromChest(a_this);
			// No processing if not a companion player inventory chest.
			if (pIndex <= 0)
			{
				return _AddObjectToContainer(a_this, a_object, a_extraList, a_count, a_fromRefr);
			}
			
			DBG
			(
				"{}: {} of {}, from {}. List: {:p}.",
				a_this->GetName(),
				a_count, 
				a_object ? a_object->GetName() : "NONE",
				a_fromRefr ? a_fromRefr->GetName() : "NONE",
				fmt::ptr(a_extraList)
			);

			// Moving an object back to self in this way has led to a ton of crashes 
			// and weird bugs from my experience.
			// Change as sent/received from none.
			if (a_fromRefr == a_this)
			{
				DBG("{}: Move {} to/from none, not self.", 
					a_this->GetName(), a_object->GetName());
				a_fromRefr = nullptr;
			}

			const auto p1 = RE::PlayerCharacter::GetSingleton();
			bool addSerializableExData = Util::IsEquipableInventoryObject(a_object);
			if (addSerializableExData)
			{
				if (a_extraList)
				{
					if (!a_extraList->HasType<RE::ExtraShouldWear>())
					{
						auto data = static_cast<RE::ExtraShouldWear*>
						(
							a_extraList->Add(RE::BSExtraData::Create<RE::ExtraShouldWear>())
						);
						if (data)
						{
							DBG
							(
								"Added serializable exData to {} ({:p}).",
								a_object->GetName(), 
								fmt::ptr(a_extraList)
							);
						}
						else
						{
							DBG("ERR: Failed to add serializable exData to {} ({:p}):",
								a_object->GetName(), fmt::ptr(a_extraList));
						}
					}
				}
				else
				{
					a_extraList = Util::CreateExtraDataListWithSerializableData();
					if (a_extraList)
					{
						DBG
						(
							"Created extra data list before adding {} of {}. "
							"Should set count: {}.",
							a_count, a_object->GetName(), a_count > 1
						);
						if (a_count > 1)
						{
							a_extraList->SetCount(a_count);
						}
					}
					else
					{
						DBG("ERR: Failed to create extra data list before adding {} of {}.",
							a_count, a_object->GetName());
					}
				}
			}

			const auto& p = glob.coopPlayers[pIndex];
			if (glob.menuPID == pIndex && 
				glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
			{
				if (!p1)
				{
					return;
				}

				DBG
				(
					"ALERT: {} to {}'s inventory chest. Can corrupt P1 script properties. "
					"Move to P1 instead.",
					a_object->GetName(), glob.coopPlayers[pIndex]->coopActor->GetName()
				);
				p1->AddObjectToContainer(a_object, a_extraList, a_count, a_fromRefr);
				return;
			}

			// Credits to ahzaab:
			// https://github.com/ahzaab/iEquipUtil/blob/master/src/Hooks.cpp#L54
			// Ensure not more than the amount of the item specified is added
			// because the extra data list's exCount can be larger than the requested count.
			auto countLeft = a_count;
			// Add a number equal to the exData list's exCount first.
            if (a_extraList) 
			{
                auto count = a_extraList->GetCount();
                countLeft -= count;
				DBG("{}: Adding {} of {} from list {:p} exCount.",
					p->coopActor->GetName(), count, a_object->GetName(), fmt::ptr(a_extraList));
                _AddObjectToContainer(a_this, a_object, a_extraList, count, a_fromRefr);
            }

			// Then add default versions of the same object to make up the difference.
			RE::ExtraDataList* exDataList = nullptr;
			if (addSerializableExData && countLeft > 0)
			{
				exDataList = Util::CreateExtraDataListWithSerializableData();
				if (exDataList)
				{
					DBG
					(
						"{}: Adding {} of serializable lists for {} to make up the difference.",
						p->coopActor->GetName(), countLeft, a_object->GetName()
					);
				}
				else
				{
					DBG("ERR: Failed to create extra data list before adding {} of {}.",
						a_count, a_object->GetName());
				}
			}

            while (countLeft-- > 0) 
			{
                _AddObjectToContainer(a_this, a_object, exDataList, 1, a_fromRefr);
            }

			auto invChanges = a_this->GetInventoryChanges();
			if (!invChanges || !invChanges->entryList)
			{
				return;	
			}

			// Fix up counts in the chest's inventory afterward.
			for (auto& entry : *invChanges->entryList) 
			{
				// Having an allocated extra lists list that is empty will cause crashes
				// if anyone tries to access any element of the list, ex. with front().
				// Since there are no extra lists (all unmodified items) and the countDelta member 
				// of the entry determines the count, there's no reason to have an empty list 
				// of extra lists that pose a crash threat, 
				// so RE::free the memory and assign nullptr to prevent access 
				// as long as a null check is performed.
				// Will remove this if it is ill-advised and causes issues elsewhere, 
				// such as after adding an extra list to this item later via crafting.
				if (entry && entry->extraLists && entry->extraLists->empty())
				{
					ERR("{}. TAHTS GON BE BUG: {}.",
						entry->object->GetName(), entry->countDelta);
					delete entry->extraLists;
					entry->extraLists = nullptr;
				}

				int32_t exListsCount = 0;
				if (!entry || !entry->object || entry->object != a_object)
				{
					continue;
				}
				
				// Should be >= 1.
				DBG("{}: {}'s entry count after potential addition: {}.", 
					p->coopActor->GetName(), a_object->GetName(), entry->countDelta);
				if (entry->extraLists)
				{
					for (auto exDataList : *entry->extraLists)
					{
						if (!exDataList)
						{
							continue;
						}

						exListsCount += exDataList->GetCount();

						auto exOwnership = exDataList->GetByType<RE::ExtraOwnership>();
						bool canRemoveOwnership = 
						(
							(exOwnership) &&
							(
								(!exOwnership->owner) || 
								(!exOwnership->owner->As<RE::Actor>()) ||
								(Util::IsPartyFriendlyActor(exOwnership->owner->As<RE::Actor>()))
							)
						);
						if (canRemoveOwnership)
						{
							DBG
							(
								"Can remove ownership from {} ({}).",
								entry->object->GetName(), 
								exOwnership->owner ? exOwnership->owner->GetName() : "NONE"
							);
							exDataList->Remove(RE::ExtraDataType::kOwnership, exOwnership);
						}
					}
				}

				// Both register as not in the player's inventory, so nothing to correct.
				if (entry->countDelta <= 0 && exListsCount == 0)
				{
					continue;
				}

				int32_t countsDelta = entry->countDelta - exListsCount;
				if (countsDelta < 0)
				{
					ERR
					(
						"{}: Item {}'s entry countDelta is less than "
						"the accumulated extra data list item count (diff of {}). "
						"Setting entry countDelta to {}.",
						p->coopActor->GetName(),
						a_object->GetName(), 
						countsDelta,
						exListsCount
					);
				}
				else if (countsDelta > 0 && addSerializableExData)
				{
					entry->AddExtraList
					(
						Util::CreateExtraDataListWithSerializableData()
					);
					if (entry->extraLists && !entry->extraLists->empty())
					{
						const auto addedList = entry->extraLists->front();
						DBG
						(
							"To account for {} unmodified items: "
							"added serializable exData list to {}: {:p}.",
							countsDelta, entry->object->GetName(), fmt::ptr(addedList)
						);
						DBG
						(
							"Set new exData list for {} {:p}'s count to {}.",
							entry->object->GetName(), 
							fmt::ptr(addedList),
							countsDelta
						);
						addedList->SetCount(countsDelta);
					}
					else
					{
						DBG("ERR: Failed to add serializable exData list to {}:",
							entry->object->GetName());
					}
				}

				entry->countDelta = max(entry->countDelta, exListsCount);
			}
		}

		RE::ObjectRefHandle* TESObjectREFRHooks::RemoveItem
		(
			RE::TESObjectREFR* a_this,
			RE::ObjectRefHandle* a_handleOut,
			RE::TESBoundObject* a_item, 
			std::int32_t a_count, 
			RE::ITEM_REMOVE_REASON a_reason, 
			RE::ExtraDataList* a_extraList, 
			RE::TESObjectREFR* a_moveToRef, 
			const RE::NiPoint3* a_dropLoc,
			const RE::NiPoint3* a_rotate
		)
		{
			if (!a_item || a_count == 0 || !glob.globalDataInit || !glob.allPlayersInit)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}

			const auto pIndex = GlobalCoopData::GetCoopPlayerIndexFromChest(a_this);
			if (pIndex <= 0)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}
			
			DBG
			(
				"{}: {} of {}, to {}. List: {:p}.",
				a_this->GetName(),
				a_count, 
				a_item ? a_item->GetName() : "NONE",
				a_moveToRef ? a_moveToRef->GetName() : "NONE",
				fmt::ptr(a_extraList)
			);
			
			// Moving an object back to self in this way has led to a ton of crashes 
			// and weird bugs from my experience.
			// Change as sent/received from none.
			if (a_moveToRef == a_this)
			{
				DBG("{}: Move {} to/from none, not self.", 
					a_this->GetName(), a_item->GetName());
				a_moveToRef = nullptr;
			}

			const auto& p = glob.coopPlayers[pIndex];
			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (glob.menuPID == pIndex && 
				glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kInventory))
			{
				if (!p1)
				{
					return nullptr;
				}

				DBG
				(
					"ALERT: {} from {}'s inventory chest. Can corrupt P1 script properties. "
					"Remove from P1 to {} instead.",
					a_item->GetName(), 
					glob.coopPlayers[pIndex]->coopActor->GetName(), 
					a_moveToRef ? a_moveToRef->GetName() : "NONE"
				);
				p1->RemoveItem
				(
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
				return nullptr;
			}

			auto invChanges = a_this->GetInventoryChanges();
			if (!invChanges || !invChanges->entryList)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}

			auto ui = RE::UI::GetSingleton();
			// Trying to move an item to P1 from the companion player's inventory.
			bool canTransferOrDrop = 
			(
				a_moveToRef && 
				a_moveToRef->IsPlayerRef() &&
				ui && 
				ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) && 
				glob.mim->IsRunning() && 
				glob.mim->managerMenuPID != -1 &&
				glob.mim->isShowingInventory
			);
			if (!canTransferOrDrop)
			{
				return _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}

			// Transferring/dropping the item after moving to P1.
			DBG
			(
				"P1 receiving/dropping item {} ({:p}, x{})",
				a_item->GetName(), fmt::ptr(a_extraList), a_count
			);

			// Unequip before dropping/transferring to avoid crash.
			// Get the equip index to unequip the form from.
			auto equipType = a_item->As<RE::BGSEquipType>();
			auto equipSlot = equipType ? equipType->equipSlot : nullptr;
			bool isHandForm = 
			(
				equipSlot == glob.bothHandsEquipSlot ||
				equipSlot == glob.rightHandEquipSlot ||
				equipSlot == glob.leftHandEquipSlot ||
				equipSlot == glob.eitherHandEquipSlot
			);
			EquipIndex index = EquipIndex::kRightHand;
			RE::ExtraDataList* equippedPlayerList = nullptr;
			bool worn = Util::HasWornRankMask(a_extraList, false, false);
			if (worn)
			{
				equippedPlayerList = Util::GetEquippedExtraData
				(
					p->coopActor.get(), a_item, false
				);
				if (equippedPlayerList)
				{
					if (isHandForm)
					{
						index = EquipIndex::kRightHand;
					}
					else if (a_item->IsAmmo())
					{
						index = EquipIndex::kAmmo;
					}
					else
					{
						index = EquipIndex::kNone;
					}
						
					DBG
					(
						"Equipped list for removed item {:p} found ({:p}) on the player. "
						"Matching with equipped bound object "
						"{} in right hand or default slot: index {}.", 
						fmt::ptr(a_extraList),
						fmt::ptr(equippedPlayerList),
						a_item->GetName(), 
						index
					);
					p->em->HandleCompanionPlayerUnequip
					(
						a_item, 
						index,
						equippedPlayerList, 
						1, 
						p->em->GetEquipSlotForForm(a_item, index)
					);
				}
				else
				{
					// Shouldn't happen unless the player's equip state and the chest's 
					// are out-of-sync.
					// Just try to match the bound objects and then unequip as a fallback.
					auto foundIter = std::find_if
					(
						p->em->equippedForms.begin(), p->em->equippedForms.end(), 
						[a_item](RE::TESForm* a_form) 
						{
							return a_form == a_item; 
						}
					);
					index = 
					(
						foundIter != p->em->equippedForms.end() ?
						static_cast<EquipIndex>(foundIter - p->em->equippedForms.begin()) : 
						EquipIndex::kTotal
					);
					if (index != EquipIndex::kTotal)
					{
						DBG
						(
							"Matching list for removed item {:p} not found on the player. "
							"Matching with equipped bound object "
							"{} in right hand or default slot: index {}.", 
							fmt::ptr(a_extraList),
							a_item->GetName(), 
							index
						);
						p->em->UnequipFormAtIndex(static_cast<EquipIndex>(index));
					}
				}
			}

			worn = Util::HasWornRankMask(a_extraList, true, false);
			if (worn)
			{
				equippedPlayerList = Util::GetEquippedExtraData
				(
					p->coopActor.get(), a_item, true
				);
				DBG
				(
					"Equipped list for removed item {:p} found ({:p}) on the player. "
					"Matching with equipped bound object {} in the left hand.", 
					fmt::ptr(a_extraList),
					fmt::ptr(equippedPlayerList),
					a_item->GetName()
				);
				p->em->HandleCompanionPlayerUnequip
				(
					a_item,
					EquipIndex::kLeftHand, 
					equippedPlayerList, 
					1, 
					p->em->GetEquipSlotForForm(a_item, EquipIndex::kLeftHand)
				);
			}
			// I stg, if this line was causing the enchantments removal bug from 1.0.3
			// and I just spent 100s of hours on building a new equip system
			// based on the inventory chests to fix the bug, well...
			// Three letters: G A H
			/*else if (p->em->IsEquipped(selectedForm, matchingPlayerList, false, true))
			{
				Util::UnequipObject(menuCoopActorPtr.get(), boundObj, selectedExDataList);
			}*/

			// Get the current number owned before dropping/transferring.
			int32_t currentCount = 0;
			auto inventory = a_this->GetInventory();
			const auto iter = inventory.find(a_item);
			if (iter != inventory.end())
			{
				currentCount = iter->second.first;
			}

			// Unfavorite the item if none of this item will remain 
			// after dropping/transferring one.
			if (currentCount <= 1)
			{
				Util::ChangeFormFavoritesStatus(a_this, a_item, false);
			}
				
			// Check if there's an matching object in the drop request pair 
			// that we set before moving the item,
			// which means the player wanted to drop the item.
			// If it's present, we drop the item after transferring to P1,
			// otherwise, we just transfer the item.
			bool isDropReq = 
			(
				glob.mim->dropReqPair.first == a_item && glob.mim->dropReqPair.second > 0
			);
			RE::ObjectRefHandle* refHandlePtr = nullptr;
			// Remove to P1 and then drop.
			if (isDropReq)
			{
				DBG
				(
					"Drop {} of {} via P1. Move to P1 first: {}. Drop request is: {}, {}", 
					a_count,
					a_item->GetName(), 
					glob.copiedPlayerDataTypes.none(CopyablePlayerDataTypes::kInventory),
					glob.mim->dropReqPair.first ?
					glob.mim->dropReqPair.first->GetName() :
					"NONE",
					glob.mim->dropReqPair.second
				);

				// Make sure to not transfer to P1 if the inventory changes are shared.
				if (glob.copiedPlayerDataTypes.none(CopyablePlayerDataTypes::kInventory))
				{
					_RemoveItem
					(
						a_this, 
						a_handleOut,
						a_item, 
						a_count,
						a_reason,
						a_extraList, 
						a_moveToRef, 
						a_dropLoc, 
						a_rotate
					);
				}

				// Now in P1's inventory, we can drop the item here.
				/*auto dropPos = 
				(
					p->mm->playerTorsoPosition + 
					Util::RotationToDirectionVect
					(
						0.0f, 
						Util::ConvertAngle
						(
							p->coopActor->GetHeading(false)
						)
					) * 0.5f * p->coopActor->GetHeight()
				);
				p1->RemoveItem
				(
					a_item, 
					a_count,
					RE::ITEM_REMOVE_REASON::kDropping,
					a_extraList, 
					nullptr, 
					std::addressof(dropPos), 
					nullptr
				);*/

				a_handleOut = nullptr;
			}
			else
			{
				refHandlePtr = _RemoveItem
				(
					a_this, 
					a_handleOut,
					a_item, 
					a_count,
					a_reason,
					a_extraList, 
					a_moveToRef, 
					a_dropLoc, 
					a_rotate
				);
			}

			// Fix up counts in the chest's inventory afterward.
			for (auto& entry : *invChanges->entryList) 
			{
				if (entry && entry->extraLists && entry->extraLists->empty())
				{
					ERR("{}. TAHTS GON BE BUG: {}.",
						entry->object->GetName(), entry->countDelta);
					delete entry->extraLists;
					entry->extraLists = nullptr;
				}

				int32_t exListsCount = 0;
				if (!entry || !entry->object || entry->object != a_item)
				{
					continue;
				}
				
				DBG("{}: {}'s entry count after potential removal: {}.", 
					p->coopActor->GetName(), a_item->GetName(), entry->countDelta);
				if (entry->extraLists)
				{
					for (auto exDataList : *entry->extraLists)
					{
						if (!exDataList)
						{
							continue;
						}

						exListsCount += exDataList->GetCount();
					}
				}
				
				int32_t countsDelta = entry->countDelta - exListsCount;
				entry->countDelta = max(entry->countDelta, exListsCount);
				if (countsDelta < 0)
				{
					ERR
					(
						"{}: Item {}'s entry countDelta is less than "
						"the accumulated extra data list item count (diff of {}). "
						"Setting entry countDelta to {}.",
						p->coopActor->GetName(),
						a_item->GetName(), 
						countsDelta,
						exListsCount
					);
				}
			}
			
			return refHandlePtr;
		}

		void TESObjectREFRHooks::ResetInventory(RE::TESObjectREFR* a_this, bool a_leveledOnly)
		{
			// Prevent inventory chests from resetting their contents,
			// since a full inventory reset removes items added during the co-op session.

			// Allow inventory resets if global co-op data has not been initialized
			// or if this refr is not an inventory chest.
			if (!glob.globalDataInit || GlobalCoopData::GetCoopPlayerIndexFromChest(a_this) == -1)
			{
				return _ResetInventory(a_this, a_leveledOnly);	
			}

			DBG
			(
				"Not resetting {} (0x{:X})'s inventory.", a_this->GetName(), a_this->formID
			);
		}

		void TESObjectREFRHooks::SetParentCell
		(
			RE::TESObjectREFR* a_this, RE::TESObjectCELL* a_cell
		)
		{
			// Gets called before Load() and links a cell with the refr.
			// If either exterior or interior occlusion is set to removed in the mod's settings, 
			// delete occlusion markers here before they are fully loaded into the cell.
			// NOTE:
			// I am aware that this will reduce performance, but it is the only solution for now
			// if the player has opted to disable camera collisions 
			// and zooms out to a camera position beyond the traversable portion of the cell, 
			// since objects flicker and disappear, making the experience almost unplayable.

			// Set the parent cell first.
			_SetParentCell(a_this, a_cell);
			if (!a_cell || 
				!a_this->parentCell || 
				!a_this->GetBaseObject() || 
				!a_this->GetBaseObject()->IsOcclusionMarker())
			{
				return;
			}

			auto sky = RE::TES::GetSingleton() ? RE::TES::GetSingleton()->sky : nullptr;
			bool shouldRemoveOcclusionMarker =
			(
				(Settings::bRemoveExteriorOcclusion && a_this->parentCell->IsExteriorCell()) ||
				(
					(Settings::bRemoveInteriorOcclusion) && 
					(
						(a_this->parentCell->IsInteriorCell()) || 
						(sky && sky->mode == RE::Sky::Mode::kInterior)
					)
				)
			);
			if (shouldRemoveOcclusionMarker)
			{
				// Delete marker.
				a_this->SetDelete(true);
			}
		}

		void ThirdPersonCameraStatesHooks::Begin(RE::ThirdPersonState* a_this)
		{
			// Skip state transitions when the co-op camera is active.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				return;
			}

			// Run original shifts to bleedout and horse cam states.
			if (skyrim_cast<RE::BleedoutCameraState*>(a_this))
			{
				_BeginBCS(a_this);
			}
			else if (skyrim_cast<RE::HorseCameraState*>(a_this))
			{
				_BeginHCS(a_this);
			}
			else
			{
				_BeginTPCS(a_this);
			}
		}

// [THIRD PERSON CAMERA STATE HOOKS]:
		void ThirdPersonCameraStatesHooks::GetRotation
		(
			RE::ThirdPersonState* a_this, RE::NiQuaternion& a_rotation
		)
		{
			// Store the co-op cam's rotation, instead of P1's, 
			// for other plugins that may be checking the camera's rotation by using this hook.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				RE::NiMatrix3 m{ };
				Util::SetRotationMatrixPY(m, glob.cam->camPitch, glob.cam->camYaw);
				Util::NativeFunctions::NiMatrixToNiQuaternion(a_rotation, m);
				return;
			}

			// Run original functions.
			if (skyrim_cast<RE::BleedoutCameraState*>(a_this))
			{
				_GetRotationBCS(a_this, a_rotation);
			}
			else if (skyrim_cast<RE::HorseCameraState*>(a_this))
			{
				_GetRotationHCS(a_this, a_rotation);
			}
			else
			{
				_GetRotationTPCS(a_this, a_rotation);
			}
		}

		void ThirdPersonCameraStatesHooks::HandleLookInput
		(
			RE::ThirdPersonState* a_this, const RE::NiPoint2& a_input
		)
		{
			// Handle P1 look input only when the co-op camera is inactive.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				return;
			}

			// Run original functions.
			if (skyrim_cast<RE::BleedoutCameraState*>(a_this))
			{
				_HandleLookInputBCS(a_this, a_input);
			}
			else if (skyrim_cast<RE::HorseCameraState*>(a_this))
			{
				_HandleLookInputHCS(a_this, a_input);
			}
			else
			{
				_HandleLookInputTPCS(a_this, a_input);
			}
		}

		void ThirdPersonCameraStatesHooks::SetFreeRotationMode
		(
			RE::ThirdPersonState* a_this, bool a_weaponSheathed
		)
		{
			// Keep free rotation enabled (better for TDM compat) 
			// and set the camera yaw while the co-op cam is active.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				a_this->freeRotationEnabled = true;
				a_this->currentYaw = a_this->targetYaw = glob.cam->camYaw;
				a_this->freeRotation.x = Util::NormalizeAng0To2Pi
				(
					Util::NormalizeAngToPi(glob.cam->camYaw - glob.player1Actor->data.angle.z)
				);
				a_this->freeRotation.y = 0.0f;
				return;
			}

			// Run original functions.
			if (skyrim_cast<RE::BleedoutCameraState*>(a_this))
			{
				_SetFreeRotationModeBCS(a_this, a_weaponSheathed);
			}
			else if (skyrim_cast<RE::HorseCameraState*>(a_this))
			{
				_SetFreeRotationModeHCS(a_this, a_weaponSheathed);
			}
			else
			{
				_SetFreeRotationModeTPCS(a_this, a_weaponSheathed);
			}
		}

		void ThirdPersonCameraStatesHooks::UpdateRotation(RE::ThirdPersonState* a_this)
		{
			// Do not update the camera's rotation here if the co-op camera is active.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				return;
			}

			// Run original functions.
			if (skyrim_cast<RE::BleedoutCameraState*>(a_this))
			{
				_UpdateRotationBCS(a_this);
			}
			else if (skyrim_cast<RE::HorseCameraState*>(a_this))
			{
				_UpdateRotationHCS(a_this);
			}
			else
			{
				auto ui = RE::UI::GetSingleton();
				if (ui && ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME))
				{
					return;
				}

				_UpdateRotationTPCS(a_this);
			}
		}
//
// [ACTIVE EFFECT HOOKS]:
//

// [VALUE MODIFIER EFFECT HOOKS]: 
		void ValueModifierEffectHooks::Start(RE::ValueModifierEffect* a_this)
		{
			// ENDERAL ONLY: Remove all Arcane Fever magic effects if P1 is in god mode.
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				!ALYSLC::EnderalCompat::g_installed)
			{
				return _Start(a_this);
			}

			bool appliedToP1 = a_this->GetTargetActor() == RE::PlayerCharacter::GetSingleton();
			auto baseEffect = (a_this->GetBaseObject());
			bool removeArcaneFeverEffect = 
			{
				(appliedToP1) &&
				(glob.coopPlayers[0]->isInGodMode) &&
				(baseEffect) &&
				(
					baseEffect->data.primaryAV == RE::ActorValue::kLastFlattered ||
					baseEffect->data.secondaryAV == RE::ActorValue::kLastFlattered
				)
			};
			// Don't skip starting this effect.
			if (!removeArcaneFeverEffect)
			{
				return _Start(a_this);
			}

			// Reaching here means that the arcane fever effect will not start
			// since P1 is in god mode.
		}

// [VAMPIRE LORD EFFECT HOOKS]: 
		void VampireLordEffectHooks::Start(RE::VampireLordEffect* a_this)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _Start(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); 
			if (pIndex == -1)
			{
				return _Start(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			DBG
			(
				"{} is about to transform into a vampire lord ({}, 0x{:X}). "
				"Is transformed: {}, transforming: {}.",
				p->coopActor->GetName(),
				a_this->spell ? a_this->spell->GetName() : "NONE",
				a_this->spell ? a_this->spell->formID : 0xDEAD,
				p->isTransformed,
				p->isTransforming
			);

			// Save pre-transformation race to revert to later 
			// if the player is not already transforming/transformed.
			if (!p->isTransformed && !p->isTransforming)
			{
				p->preTransformationRace = p->coopActor->race;
			}
			
			p->mm->nom->InstantlyResetAllNodeData(p);
			p->isTransforming = true;

			if (!p->isPlayer1) 
			{
				// Unequip hand forms for the companion player before the transformation begins.
				// Game does not always do this automatically and the weapon anim object 
				// can stay attached to its hand node after the transformation.
				// Maintain desired forms.
				p->em->EquipFists(false);
			}

			// Start the vampire transformation effect.
			_Start(a_this);
		}

		void VampireLordEffectHooks::Finish(RE::VampireLordEffect* a_this)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _Finish(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); 
			if (pIndex == -1)
			{
				return _Finish(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			DBG
			(
				"{} is about to transform back from vampire lord ({}, 0x{:X}). "
				"Is transformed: {}, transforming: {}.",
				p->coopActor->GetName(),
				a_this->spell ? a_this->spell->GetName() : "NONE",
				a_this->spell ? a_this->spell->formID : 0xDEAD,
				p->isTransformed,
				p->isTransforming
			);

			// Should have already stopped transforming at this point, but if not, clear the flag.
			if (p->isTransforming) 
			{
				p->isTransforming = false;
			}

			// Have the effect finish up.
			_Finish(a_this);
		}

// [WEREWOLF EFFECT HOOKS]: 
		void WerewolfEffectHooks::Start(RE::WerewolfEffect* a_this)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _Start(a_this);
			}

			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); 
			if (pIndex == -1)
			{
				return _Start(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];		
			// Save pre-transformation race to revert to later 
			// if the player is not already transforming/transformed.
			if (!p->isTransformed && !p->isTransforming)
			{
				p->preTransformationRace = p->coopActor->race;
			}

			p->mm->nom->InstantlyResetAllNodeData(p);
			p->isTransforming = true;

			if (!p->isPlayer1)
			{
				// Unequip hand forms for co-op companion once the transformation begins.
				// Game does not always do this automatically and the weapon anim object 
				// can stay attached to its hand node after the transformation.
				// Maintain desired forms.
				p->em->EquipFists(false);
			}

			// Reset to base transformation time.
			p->secsMaxTransformationTime = 5.0f; //150.0f;

			// Start the werewolf trandformation effect.
			_Start(a_this);
		}

		void WerewolfEffectHooks::Finish(RE::WerewolfEffect* a_this)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _Finish(a_this);
			}
				
			auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); 
			if (pIndex == -1)
			{
				return _Finish(a_this);
			}

			const auto& p = glob.coopPlayers[pIndex];
			// Should have already stopped transforming at this point, but if not, clear the flag.
			if (p->isTransforming)
			{
				p->isTransforming = false;
			}

			// Finish up the effect.
			_Finish(a_this);
		}

//=========================
// [MENU PROCESSING HOOKS]:
//=========================

		RE::UI_MESSAGE_RESULTS BarterMenuHooks::ProcessMessage
		(
			RE::BarterMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 
			
			auto strings = RE::InterfaceStrings::GetSingleton();
			auto ui = RE::UI::GetSingleton();

			if (glob.globalDataInit &&
				glob.coopSessionActive &&
				glob.menuPID > 0 &&
				glob.mim->isShowingInventory &&
				ui &&
				ui->IsMenuOpen(a_this->MENU_NAME) &&
				strings &&
				a_message.menu == strings->topMenu && 
				*a_message.type == RE::UI_MESSAGE_TYPE::kInventoryUpdate)
			{
				// Re-apply equip state after the top menu is updated, which resets equip state.
				auto result = _ProcessMessage(a_this, a_message);
				glob.mim->UpdateMenuEntryEquipStates(false, false);
				return result;
			}
			
			// Nothing to do here, co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 	
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			if (opening)
			{
				// Get result first to open the menu and populate Barter Menu target ref handle.
				auto result = _ProcessMessage(a_this, a_message);
				// Do not modify the requests queue,
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);

				// Skip if control is/was not requested by a companion player when opening.
				if (glob.lastResolvedMenuPID <= 0)
				{
					return result;
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;

				// Copy over player data after opening the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					opening, menuName, p->coopActor->GetHandle(), nullptr
				);
				// Have to restore P1's inventory here 
				// if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					DBG
					(
						"Restoring P1's inventory, "
						"since the message to open the menu was ignored. RESULT: {}.",
						result
					);
					GlobalCoopData::CopyOverCoopPlayerData
					(
						false, menuName, p->coopActor->GetHandle(), nullptr
					);
				}

				return result;
			}
			else
			{
				// Do not modify the requests queue, 
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);
				// Skip if control is/was not requested by a companion player,
				// or if no data is still copied over.
				if (glob.lastResolvedMenuPID <= 0 || !hasCopiedData)
				{
					return _ProcessMessage(a_this, a_message);
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;
				// Copy back player data before closing the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					false, menuName, p->coopActor->GetHandle(), nullptr
				);
				
				return _ProcessMessage(a_this, a_message);
			}
			
			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS BookMenuHooks::ProcessMessage
		(
			RE::BookMenu* a_this, RE::UIMessage& a_message
		)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, co-op is not active, 
			// serializable data is not available, or this menu is not the target of the message. 
			if (ignored ||
				!glob.globalDataInit ||
				!glob.coopSessionActive ||
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by a companion player,
			// or if not opening or closing.
			if ((glob.lastResolvedMenuPID <= 0) || (!opening && !closing))
			{
				return result;
			}

			const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;
			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), a_this->GetTargetForm()
			);

			return result;
		}

		void ContainerMenuHooks::AdvanceMovie
		(
			RE::ContainerMenu* a_this, float a_interval, uint32_t a_currentTime
		)
		{
			// Display a companion player's carryweight in the Container Menu 
			// while they are in control.

			_AdvanceMovie(a_this, a_interval, a_currentTime);
			auto ui = RE::UI::GetSingleton();
			if (!ui)
			{
				return;
			}

			if (!ui->IsMenuOpen(a_this->MENU_NAME))
			{
				return;
			}

			if (!glob.globalDataInit || 
				!glob.coopSessionActive ||
				!glob.mim->IsRunning() || 
				glob.menuPID <= 0)
			{
				return;
			}

			auto playerInMenusPtr = Util::GetActorPtrFromHandle(glob.mim->menuCoopActorHandle);
			if (!playerInMenusPtr)
			{
				return;
			}

			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return;
			}

			auto view = a_this->uiMovie;
			if (!view)
			{
				return;
			}

			RE::GFxValue base{ };
			view->GetVariable
			(
				std::addressof(base), "_root.Menu_mc"
			);
			if (base.IsNull() || base.IsUndefined())
			{
				DBG("BUH");
				return;
			}
			
			RE::GFxValue bottomBar{ };
			base.GetMember("bottomBar", std::addressof(bottomBar));
			if (bottomBar.IsNull() || bottomBar.IsUndefined())
			{
				return;
			}
			
			RE::GFxValue playerCardInfo{ };
			bottomBar.GetMember("playerInfoCard", std::addressof(playerCardInfo));
			if (playerCardInfo.IsNull() || playerCardInfo.IsUndefined())
			{
				return;
			}
			
			RE::GFxValue carryweightValue{ };
			playerCardInfo.GetMember("CarryWeightValue", std::addressof(carryweightValue));
			if (!carryweightValue.IsNull() && !carryweightValue.IsUndefined())
			{
				const auto invChest = glob.coopPlayers[glob.menuPID]->em->inventoryChest;
				if (!invChest)
				{
					return;
				}

				float inventoryWeight = invChest->GetWeightInContainer();
				const auto invChanges = invChest->GetInventoryChanges();
				if (invChanges)
				{
					inventoryWeight = invChanges->totalWeight;
				}

				/*const auto& p = glob.coopPlayers[glob.menuPID];
				float inventoryWeight = p->coopActor->GetWeightInContainer();
				const auto invChanges = p->coopActor->GetInventoryChanges();
				if (invChanges)
				{
					inventoryWeight = invChanges->totalWeight;
				}*/

				carryweightValue.SetTextHTML
				(
					fmt::format
					(
						"{} / {}", 
						std::roundf(inventoryWeight),
						max(1.0f, playerInMenusPtr->GetActorValue(RE::ActorValue::kCarryWeight))
					).c_str()
				);
			}

			// TODO (maybe):
			/*
			const auto& p = glob.coopPlayers
			[
				GlobalCoopData::GetCoopPlayerIndex(playerInMenusPtr)
			];
			RE::GFxValue armorRatingValue{ };
			playerCardInfo.GetMember("ArmorRatingValue", std::addressof(armorRatingValue));
			if (!armorRatingValue.IsNull() && !armorRatingValue.IsUndefined())
			{
				armorRatingValue.SetTextHTML
				(
					fmt::format
					(
						"{}", 
						static_cast<int32_t>
						(
							p->em->armorRatings.first + p->em->armorRatings.second
						)
					).c_str()
				);
			}

			RE::GFxValue damageValue{ };
			playerCardInfo.GetMember("DamageValue", std::addressof(damageValue));
			if (!damageValue.IsNull() && !damageValue.IsUndefined())
			{
				float baseDamage = 0.0f;
				float lhDamage = 0.0f;
				float rhDamage = 0.0f;
				if (p->em->IsUnarmed())
				{
					baseDamage = p->coopActor->CalcUnarmedDamage();
				}
				else
				{
					auto lhWeapEntry = p->coopActor->GetEquippedEntryData(true);
					auto rhWeapEntry = p->coopActor->GetEquippedEntryData(false);
					bool is2H = 
					(
						p->em->Has2HRangedWeapEquipped() || p->em->Has2HMeleeWeapEquipped()
					);
					if (rhWeapEntry)
					{
						auto hitData = RE::HitData();
						hitData.Populate(playerInMenusPtr.get(), nullptr, rhWeapEntry);
						rhDamage = hitData.totalDamage;
						baseDamage += rhDamage;
					}

					// Don't double-count 2H weapons.
					if (lhWeapEntry && !is2H)
					{
						auto hitData = RE::HitData();
						hitData.Populate(playerInMenusPtr.get(), nullptr, lhWeapEntry);
						lhDamage = hitData.totalDamage;
						baseDamage += lhDamage;
					}
				}

				bool usingRangedWeap = p->em->Has2HRangedWeapEquipped();
				float ammoDamage = 0.0f;
				if (usingRangedWeap && p->em->equippedForms[!EquipIndex::kAmmo])
				{
					auto inventory = p->coopActor->GetInventory();
					auto ammo = p->em->equippedForms[!EquipIndex::kAmmo]->As<RE::TESBoundObject>();
					if (ammo)
					{
						auto iter = inventory.find(ammo);
						if (iter != inventory.end())
						{
							auto hitData = RE::HitData();
							hitData.Populate
							(
								playerInMenusPtr.get(), nullptr, iter->second.second.get()
							);
							ammoDamage = hitData.totalDamage;
							baseDamage += ammoDamage;
						}
					}
				}
				
				damageValue.SetTextHTML
				(
					fmt::format("{}", static_cast<int32_t>(baseDamage + 0.5f)).c_str()
				);

				float damageDelta = 0.0f;
				if (a_this->itemList)
				{
					auto item = a_this->itemList->GetSelectedItem(); 
					if (item && item->data.objDesc && item->data.objDesc->object)
					{
						auto weap = item->data.objDesc->object->As<RE::TESObjectWEAP>();
						float selectedItemDamage = 0.0f;
						if (weap)
						{
							auto hitData = RE::HitData();
							hitData.Populate(playerInMenusPtr.get(), nullptr, item->data.objDesc);
							selectedItemDamage = hitData.totalDamage;
							bool is2H = weap->equipSlot == glob.bothHandsEquipSlot;
							if (is2H)
							{
								damageDelta = selectedItemDamage - (lhDamage + rhDamage);
							}
							else 
							{
								auto rhWeap = p->em->GetRHWeapon();
								if (rhWeap)
								{
									damageDelta = selectedItemDamage - rhDamage;
								}
								else
								{
									damageDelta = selectedItemDamage;
								}
							}
						}
						else if (auto ammo = item->data.objDesc->object->As<RE::TESObjectWEAP>(); 
								 ammo && usingRangedWeap)
						{
							auto inventory = p->coopActor->GetInventory();
							auto iter = inventory.find(ammo);
							if (iter != inventory.end())
							{
								auto hitData = RE::HitData();
								hitData.Populate
								(
									playerInMenusPtr.get(), nullptr, iter->second.second.get()
								);
								selectedItemDamage = hitData.totalDamage;
								damageDelta = selectedItemDamage - ammoDamage;
							}
						}

						DBG
						(
							"{}: {} has damage {}, damage delta {} from total base damage {}.",
							playerInMenusPtr->GetName(),
							item->data.objDesc->object->GetName(),
							selectedItemDamage,
							damageDelta,
							baseDamage
						);
					}
				}

				damageValue.SetMember("textAutoSize", "shrink");
				if (damageDelta == 0.0f)
				{
					damageValue.SetTextHTML
					(
						fmt::format("{}", static_cast<int32_t>(baseDamage)).c_str()
					);
				}
				else if (damageDelta < 0.0f)
				{
					damageValue.SetTextHTML
					(
						fmt::format
						(
							"{} <font color=\'#FF0000\'>({})</font>", 
							static_cast<int32_t>(baseDamage + damageDelta),
							static_cast<int32_t>(damageDelta)
						).c_str()
					);
				}
				else
				{
					damageValue.SetTextHTML
					(
						fmt::format
						(
							"{} <font color=\'#189515\'>(+{})</font>", 
							static_cast<int32_t>(baseDamage + damageDelta),
							static_cast<int32_t>(damageDelta)
						).c_str()
					);
				}
			}
			*/
		}

		RE::UI_MESSAGE_RESULTS ContainerMenuHooks::ProcessMessage
		(
			RE::ContainerMenu* a_this, RE::UIMessage& a_message
		)
		{
			DBG("Menu: {}, type: {}.", a_message.menu, *a_message.type);
			auto strings = RE::InterfaceStrings::GetSingleton();
			auto ui = RE::UI::GetSingleton();

			if (glob.globalDataInit &&
				glob.coopSessionActive &&
				glob.menuPID > 0 &&
				glob.mim->isShowingInventory &&
				ui &&
				ui->IsMenuOpen(a_this->MENU_NAME) &&
				strings &&
				a_message.menu == strings->topMenu && 
				*a_message.type == RE::UI_MESSAGE_TYPE::kInventoryUpdate)
			{
				// Re-apply equip state after the top menu is updated, which resets equip state.
				auto result = _ProcessMessage(a_this, a_message);
				glob.mim->UpdateMenuEntryEquipStates(false, false);
				return result;
			}
			
			// Nothing to do here, co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 	
			if (!glob.globalDataInit ||
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide ||
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Reset LootMenu request PID, since we want to allow other players to open the menu
			// if this Container Menu closed after a player opened it via the LootMenu previously.
			// Also prevents weird delayed input processing glitch while in the Container Menu
			// if the LootMenu is still open under the Container Menu.
			Util::SendCrosshairEvent(nullptr);
			if (opening)
			{
				// Get result first to open the menu and populate Container Menu target ref handle.
				auto result = _ProcessMessage(a_this, a_message);
				// Do not modify the requests queue,
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);

				// Skip if control is/was not requested by a companion player when opening.
				if (glob.lastResolvedMenuPID <= 0)
				{
					return result;
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;

				// Copy over player data after opening the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					opening, menuName, p->coopActor->GetHandle(), nullptr
				);
				// Have to restore P1's inventory here 
				// if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					DBG
					(
						"Restoring P1's inventory, "
						"since the message to open the menu was ignored. RESULT: {}.",
						result
					);
					GlobalCoopData::CopyOverCoopPlayerData
					(
						false, menuName, p->coopActor->GetHandle(), nullptr
					);
				}

				return result;
			}
			else
			{
				// Do not modify the requests queue, 
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);
				// Skip if control is/was not requested by a companion player,
				// or if no data is still copied over.
				if (glob.lastResolvedMenuPID <= 0 || !hasCopiedData)
				{
					return _ProcessMessage(a_this, a_message);
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;
				// Copy back player data before closing the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					false, menuName, p->coopActor->GetHandle(), nullptr
				);
				
				return _ProcessMessage(a_this, a_message);
			}
			
			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS CraftingMenuHooks::ProcessMessage
		(
			RE::CraftingMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide ||
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}
			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// For companion players, reset to default package and stop interacting.
			const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
			if (closing)
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				if (glob.lastResolvedMenuPID > 0)
				{
					p->mm->interactionPackageRunning = false;
					p->pam->SetAndEveluatePackage();
					p->coopActor->StopInteractingQuick(true);
					// IMPORTANT:
					// If not clearing out the furniture data set when opening the menu, 
					// no player will be able to open most menus
					// (Tween, Stats, Inventory, Map, etc.).
					// Cannot just clear the currently occupied furniture handle,
					// as this will lead to locking players out of using the furniture.
					if (p1)
					{
						DBG
						(
							"Clear P1's occupied furniture handle ({}) "
							"when done interacting.",
							Util::HandleIsValid(p1->GetOccupiedFurniture()) ?
							p1->GetOccupiedFurniture().get()->GetName() : 
							"NONE"
						);
						p1->StopInteractingQuick(true);
					}
				}
			}

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((glob.lastResolvedMenuPID <= 0) || (!opening && !closing))
			{
				return _ProcessMessage(a_this, a_message);
			}

			const RE::BSFixedString menuName = a_this->MENU_NAME;
			RE::TESForm* assocForm = nullptr;
			// Set furniture (crafting station) as the associated form.
			if (a_this->subMenu)
			{
				assocForm = a_this->subMenu->furniture;
			}

			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), assocForm
			);

			// Calculate the result of this message, and if it isn't handled, restore P1's data.
			auto result = _ProcessMessage(a_this, a_message);
			if (opening)
			{
				// Have to restore P1's inventory here 
				// if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					DBG
					(
						"Restoring P1's inventory, "
						"since the message to open the menu was ignored. RESULT: {}.",
						result
					);
					GlobalCoopData::CopyOverCoopPlayerData
					(
						false, menuName, p->coopActor->GetHandle(), assocForm
					);
				}
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS DialogueMenuHooks::ProcessMessage
		(
			RE::DialogueMenu* a_this, RE::UIMessage& a_message
		)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, co-op is not active, 
			// serializable data is not available, or this menu is not the target of the message. 
			if (ignored ||
				!glob.globalDataInit ||
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((glob.lastResolvedMenuPID <= 0) || (!opening && !closing))
			{
				return result;
			}

			const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;
			RE::TESForm* assocForm = nullptr;
			// Get speaker as associated form.
			auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
			if ((menuTopicManager) && 
				(menuTopicManager->speaker.get() || menuTopicManager->lastSpeaker.get()))
			{
				auto speaker = 
				(
					menuTopicManager->speaker.get() ? 
					menuTopicManager->speaker.get() : 
					menuTopicManager->lastSpeaker.get()
				);
				assocForm = speaker.get();
			}

			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), assocForm
			);

			return result;
		}

		RE::UI_MESSAGE_RESULTS FavoritesMenuHooks::ProcessMessage
		(
			RE::FavoritesMenu* a_this, RE::UIMessage& a_message
		)
		{
			// NOTE: 
			// Favorited items are stored in the Favorite Menu's favorites list 
			// sometime between the ProcessMessage() call and the menu opening.
			// Don't call the original ProcessMessage() func 
			// until the requesting player's favorited items have been imported by P1.
			
			// Have not figured out when the Favorites Menu's favorites entry list updates,
			// but it seems to occur after the last 'Top Menu' inventory update message
			// is sent while the Favorites Menu is open.
			// Signal the MIM to update its cached equip entry data 
			// so that the correct favorited items will show as equipped
			// for the companion player controlling menus.
			// If instead updating right after importing favorites.
			// we'll incorrectly store P1's equip entry state,
			// since the list hasn't updated yet.
			auto strings = RE::InterfaceStrings::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			if (glob.globalDataInit &&
				glob.coopSessionActive &&
				glob.menuPID > 0 &&
				ui &&
				ui->IsMenuOpen(a_this->MENU_NAME) &&
				strings &&
				a_message.menu == strings->topMenu && 
				*a_message.type == RE::UI_MESSAGE_TYPE::kInventoryUpdate)
			{
				glob.mim->SignalRefreshMenuEquipState();
				return _ProcessMessage(a_this, a_message);
			}

			// Nothing to do here, since co-op is not active, serializable data is not available,
			// or this menu is not the target of the message.
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}
			
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				// Not all updates made to menu elements through the MIM apply properly.
				// Certain entry elements must be updated right before the message is propagated,
				// so that any overriding changes made by the game 
				// can be overwritten by our own changes.
				auto taskInterface = SKSE::GetTaskInterface(); 
				if (!taskInterface)
				{
					return _ProcessMessage(a_this, a_message);
				}
				
				DBG("Update Favorites Menu for P{}.", glob.menuPID + 1);
				// Update quickslot tags for P1,
				// since the game wipes the tag after hotkeying an item.
				// Run update first before updating entry text.
				auto result = _ProcessMessage(a_this, a_message);
				if (glob.menuPID <= 0) 
				{
					taskInterface->AddUITask
					(
						[]() 
						{
							auto ui = RE::UI::GetSingleton(); 
							if (!ui)
							{
								return;
							}

							auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
							if (!favoritesMenu)
							{
								return;
							}

							auto view = favoritesMenu->uiMovie; 
							if (!view)
							{
								return;
							}

							const auto& p = glob.coopPlayers[0];
							double numEntries = view->GetVariableDouble
							(
								"_root.MenuHolder.Menu_mc.itemList.entryList.length"
							);
							RE::GFxValue entryList;
							view->CreateArray(std::addressof(entryList));
							view->GetVariable
							(
								std::addressof(entryList), 
								"_root.MenuHolder.Menu_mc.itemList.entryList"
							);
							RE::GFxValue entry{ };
							RE::GFxValue entryIndex{ };
							RE::GFxValue entryText{ };
							std::string entryStr = "";
							int32_t index = -1;
							// Iterate through entries, find quick slotted spell/item
							// and update its tag.
							for (uint32_t i = 0; i < numEntries; ++i)
							{
								view->GetVariableArray
								(
									"_root.MenuHolder.Menu_mc.itemList.entryList", 
									i, 
									std::addressof(entry),
									1
								);
								entry.GetMember("index", std::addressof(entryIndex));
								index = static_cast<int32_t>(entryIndex.GetNumber());
								entry.GetMember("text", std::addressof(entryText));
								entryStr = entryText.GetString();

								// Add quick slot item/spell tag.
								bool matching = 
								(
									favoritesMenu->favorites[index].item == 
									p->em->quickSlotItem ||
									favoritesMenu->favorites[index].item == 
									p->em->quickSlotSpell
								);
								if (matching)
								{
									bool isConsumable = index == p->em->equippedQSItemIndex;
									if (entryStr.find("(*QS", 0) == std::string::npos)
									{
										entryStr = fmt::format
										(
											"(*QS{}*) {}", isConsumable ? "I" : "S", entryStr
										);
										entryText.SetString(entryStr);
										entry.SetMember("text", entryText);
									}

									// Apply updated entry to the list.
									view->SetVariableArray
									(
										"_root.MenuHolder.Menu_mc.itemList.entryList", 
										i, 
										std::addressof(entry), 
										1
									);
								}
							}
							
							// Update the favorites entry list.
							view->InvokeNoReturn
							(
								"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
							);
							DBG("Refreshed quick slot tags for P1.");
						}
					);
				}
				else if (glob.menuPID != -1)
				{
					// For companion players, update equip state for all favorited entries 
					// and refresh the item list.
					taskInterface->AddUITask
					(
						[]() 
						{
							auto ui = RE::UI::GetSingleton(); 
							if (!ui)
							{
								return;
							}

							auto favoritesMenu = ui->GetMenu<RE::FavoritesMenu>(); 
							if (!favoritesMenu)
							{
								return;
							}

							auto view = favoritesMenu->uiMovie; 
							if (!view)
							{
								return;
							}

							const auto& p = glob.coopPlayers[glob.menuPID];
							RE::ActorPtr menuCoopActorPtr = 
							(
								Util::GetActorPtrFromHandle(glob.mim->menuCoopActorHandle)
							);
							if (!menuCoopActorPtr)
							{
								return;
							}
							
							const auto& favoritesList = favoritesMenu->favorites;
							double numEntries = view->GetVariableDouble
							(
								"_root.MenuHolder.Menu_mc.itemList.entryList.length"
							);
							RE::GFxValue entryList{ };
							view->CreateArray(std::addressof(entryList));
							view->GetVariable
							(
								std::addressof(entryList),
								"_root.MenuHolder.Menu_mc.itemList.entryList"
							);
							if (!entryList.IsArray())
							{
								return;
							}
							
							numEntries = min(numEntries, entryList.GetArraySize());
							// Iterate through and update all entries for the companion player.
							for (uint32_t i = 0; i < numEntries; ++i)
							{
								numEntries = view->GetVariableDouble
								(
									"_root.MenuHolder.Menu_mc.itemList.entryList.length"
								);
								RE::GFxValue entryIndex{ };
								RE::GFxValue entry{ };
								view->GetVariableArray
								(
									"_root.MenuHolder.Menu_mc.itemList.entryList",
									i, 
									std::addressof(entry),
									1
								);
								if (entry.IsNull() || entry.IsUndefined())
								{
									continue;
								}

								if (!entry.HasMember("index"))
								{
									continue;
								}

								entry.GetMember("index", std::addressof(entryIndex));
								int32_t index = static_cast<int32_t>(entryIndex.GetNumber());
								DBG
								(
									"{} {} / {} ({}).", i, index, numEntries, favoritesList.size()
								);

								RE::TESForm* favoritedItem = 
								(
									index != -1 && index < favoritesList.size() ?
									favoritesList[index].item : 
									nullptr
								);
								if (!favoritedItem)
								{
									continue;
								}
								
								// Get the form ID of the entry.
								RE::GFxValue entryFormId{ };
								entry.GetMember("formId", std::addressof(entryFormId));
								uint32_t formID = 0;
								// For SKYUI users (entries have member "formId").
								if (entryFormId.GetNumber() != 0)
								{
									formID = static_cast<uint32_t>(entryFormId.GetNumber());
								}
								else
								{
									// Vanilla UI.
									formID = favoritedItem ? favoritedItem->formID : 0;
								}
								
								bool isVampireLord = Util::IsVampireLord(p->coopActor.get());
								// If transformed into a Vampire Lord, 
								// this player shares P1's favorites.
								if (isVampireLord || p->em->favoritedFormIDs.contains(formID))
								{
									RE::GFxValue entryText{ };
									entry.GetMember("text", std::addressof(entryText));
									std::string entryStr = entryText.GetString();

									// Update item count to reflect the number of that item 
									// in the co-op player's inventory, not P1's.
									// Ignore spells and shouts, which always have count 1.
									auto boundObj = favoritedItem->As<RE::TESBoundObject>();
									if (boundObj && !favoritedItem->Is
										(
											RE::FormType::Spell, RE::FormType::Shout
										))
									{
										auto exDataList = 
										(
											favoritesList[index].entryData && 
											favoritesList[index].entryData->extraLists ? 
											favoritesList[index].entryData->extraLists->front() :
											nullptr
										);
										
										// Why does Skyrim not use the extra display data 
										// for the entry nameby default? Why?
										entryStr = Util::GetDescriptiveName(boundObj, exDataList);
										// Eww. Gross.
										// Get the matching ex data list for the favorited item's
										// list which is in P1's inventory.
										// Then get the count from the chest.
										uint32_t count = Util::GetIntrinsicallyEqualCount
										(
											p->em->inventoryChest.get(),
											boundObj,
											Util::FindMatchingExtraDataList
											(
												p->em->inventoryChest.get(),
												boundObj,
												exDataList
											)
										);

										DBG
										(
											"Setting {}'s count to {}.",
											favoritedItem->GetName(), count
										);
										entryStr += " (" + std::to_string(count) + ")";
										entryText.SetString(entryStr);
										entry.SetMember("text", entryText);
									}
									
									// Update equip state for the entry.
									// Normal items receive an update to the "caret" equipped icon,
									// while quick slot items have their entry text modified.
									const auto& equipStateNum = 
									(
										glob.mim->favEntryEquipStates[index]
									);
									RE::GFxValue equipState{ };
									equipState.SetNumber(static_cast<double>(equipStateNum));
									entry.SetMember("equipState", equipState);

									// Add quick slot item/spell tag.
									bool matching = 
									(
										favoritesMenu->favorites[index].item == 
										p->em->quickSlotItem ||
										favoritesMenu->favorites[index].item == 
										p->em->quickSlotSpell
									);
									if (matching)
									{
										DBG
										(
											"Index {} ({}) equals one of {}, {}", 
											index,
											favoritedItem->GetName(),
											p->em->equippedQSItemIndex,
											p->em->equippedQSSpellIndex
										);
										bool isConsumable = index == p->em->equippedQSItemIndex;
										if (entryStr.find("(*QS", 0) == std::string::npos)
										{
											entryStr = fmt::format
											(
												"(*QS{}*) {}", isConsumable ? "I" : "S", entryStr
											);
											entryText.SetString(entryStr);
											entry.SetMember("text", entryText);
										}
									}
									
									// Apply updated entry to the list.
									view->SetVariableArray
									(
										"_root.MenuHolder.Menu_mc.itemList.entryList",
										i, 
										std::addressof(entry),
										1
									);
									// Insert (key = favorites list index, value = UI entry number) 
									// pairs into map.
									glob.mim->favMenuIndexToEntryMap.insert_or_assign(index, i);
								}
								else
								{
									// Item was not favorited by the menu-controlling player, 
									// so remove it from the list.
									entryList.RemoveElement(i);
									--i;
									view->SetVariableArraySize
									(
										"_root.MenuHolder.Menu_mc.itemList.entryList", 
										--numEntries
									);
								}
							}
							
							// Clears out entries for favorited items 
							// that no longer exist in the entry list.
							view->InvokeNoReturn
							(
								"_root.MenuHolder.Menu_mc.itemList.InvalidateData", nullptr, 0
							);
							// Update the favorites entry list.
							view->InvokeNoReturn
							(
								"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
							);

							DBG("Refreshed favorites entries for P{}.", glob.menuPID + 1);
						}
					);
				}

				// No more processing to do for this event.
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// Control is/was requested by co-op companion player.
			if (glob.lastResolvedMenuPID != -1 && glob.lastResolvedMenuPID != 0)
			{
				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const auto& coopP1 = glob.coopPlayers[0];
				// Do not import co-op favorites if transformed into a Vampire Lord,
				// so we can have access to P1's Vampire Lord spells.
				if (Util::IsVampireLord(p->coopActor.get()) && opening)
				{
					// Make sure P1 knows the revert form spell 
					// so that this player can transform back.
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
					{
						auto revertSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0xCD5C, "Dawnguard.esm"
						);
						auto p1 = RE::PlayerCharacter::GetSingleton(); 
						if (p1 && revertSpell && !p1->HasSpell(revertSpell))
						{
							p1->AddSpell(revertSpell);
						}
					}

					a_this->isVampire = true;
					return _ProcessMessage(a_this, a_message);
				}
				else if (Util::IsVampireLord(coopP1->coopActor.get()))
				{
					// P1 is a Vampire Lord, but this player is not.
					a_this->isVampire = false;
				}
				else if (closing)
				{
					// Update to P1 Vampire Lord state when closing.
					a_this->isVampire = Util::IsVampireLord(coopP1->coopActor.get());
				}

				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const RE::BSFixedString menuName = a_this->MENU_NAME;
					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData
					(
						opening, menuName, p->coopActor->GetHandle(), nullptr
					);

					// Force PersistentFavorites (https://github.com/QY-MODS/PersistentFavorites) 
					// to sync its cached favorites list 
					// after we import the companion player's favorites or restore P1's favorites.
					// Syncs on toggle favorites bind press: 
					// https://github.com/QY-MODS/PersistentFavorites/blob/main/src/Events.cpp#L4 
					if (ALYSLC::PersistentFavoritesCompat::g_installed) 
					{
						auto ue = RE::UserEvents::GetSingleton(); 
						auto controlMap = RE::ControlMap::GetSingleton();
						if (ue && controlMap) 
						{
							auto userEvent = ue->yButton;
							auto device = RE::INPUT_DEVICE::kGamepad;
							auto keyCode = controlMap->GetMappedKey
							(
								userEvent, device, RE::UserEvents::INPUT_CONTEXT_IDS::kItemMenu
							);
							if (keyCode != 0xFF) 
							{
								std::unique_ptr<RE::InputEvent* const> buttonEvent = 
								(
									std::make_unique<RE::InputEvent* const>
									(
										RE::ButtonEvent::Create
										(
											device, userEvent, keyCode, 0.0f, 1.0f
										)
									)
								);
								// Indicate that the event was sent by a companion player.
								(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
								Util::SendInputEvent(buttonEvent);
							}
						}
					}

					// Have to restore P1's favorited items here 
					// if the game ignores this call to open the menu.
					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							DBG
							(
								"Restoring P1's favorites, "
								"since the message to open the FavoritesMenu was not handled. "
								"RESULT: {}.", 
								result
							);
							GlobalCoopData::CopyOverCoopPlayerData
							(
								false, menuName, p->coopActor->GetHandle(), nullptr
							);
						}
					}
					
					return result;
				}
			}
			else
			{
				// Set favorited forms data for P1, including magical favorites.
				const auto& coopP1 = glob.coopPlayers[0];
				coopP1->em->UpdateFavoritedFormsLists(false);
			}

			return _ProcessMessage(a_this, a_message);
		}
		
		RE::UI_MESSAGE_RESULTS GiftMenuHooks::ProcessMessage
		(
			RE::GiftMenu* a_this, RE::UIMessage& a_message
		)
		{
			DBG("Menu: {}, type: {}.", a_message.menu, *a_message.type);
			
			auto strings = RE::InterfaceStrings::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			if (glob.globalDataInit &&
				glob.coopSessionActive &&
				glob.menuPID > 0 &&
				glob.mim->isShowingInventory &&
				ui &&
				ui->IsMenuOpen(a_this->MENU_NAME) &&
				strings &&
				a_message.menu == strings->topMenu && 
				*a_message.type == RE::UI_MESSAGE_TYPE::kInventoryUpdate)
			{
				// Re-apply equip state after the top menu is updated, which resets equip state.
				auto result = _ProcessMessage(a_this, a_message);
				glob.mim->UpdateMenuEntryEquipStates(false, false);
				return result;
			}

			// Nothing to do here, co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 	
			if (!glob.globalDataInit ||
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			if (opening)
			{
				// Get result first to open the menu and populate Gift Menu target ref handle.
				auto result = _ProcessMessage(a_this, a_message);
				// Do not modify the requests queue,
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);

				// Skip if control is/was not requested by a companion player when opening.
				if (glob.lastResolvedMenuPID <= 0)
				{
					return result;
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;

				// Copy over player data after opening the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					opening, menuName, p->coopActor->GetHandle(), nullptr
				);
				// Have to restore P1's inventory here 
				// if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					DBG
					(
						"Restoring P1's inventory, "
						"since the message to open the menu was ignored. RESULT: {}.",
						result
					);
					GlobalCoopData::CopyOverCoopPlayerData
					(
						false, menuName, p->coopActor->GetHandle(), nullptr
					);
				}

				return result;
			}
			else
			{
				// Do not modify the requests queue, 
				// since the menu input manager still needs this info
				// when setting the request and menu player IDs when this menu opens/closes.
				glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
				(
					a_this->MENU_NAME, false
				);
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				DBG
				(
					"Current menu PID: {}, resolved menu PID: {}, manager PID: {}. "
					"Opening: {}, closing: {}, has copied data: {}.",
					glob.menuPID, 
					glob.lastResolvedMenuPID,
					glob.mim->managerMenuPID,
					opening, 
					closing,
					hasCopiedData
				);
				// Skip if control is/was not requested by a companion player,
				// or if no data is still copied over.
				if (glob.lastResolvedMenuPID <= 0 || !hasCopiedData)
				{
					return _ProcessMessage(a_this, a_message);
				}

				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				const RE::BSFixedString menuName = a_this->MENU_NAME;
				// Copy back player data before closing the menu.
				GlobalCoopData::CopyOverCoopPlayerData
				(
					false, menuName, p->coopActor->GetHandle(), nullptr
				);
				
				return _ProcessMessage(a_this, a_message);
			}
			
			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS InventoryMenuHooks::ProcessMessage
		(
			RE::InventoryMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Open co-op companion's inventory (ContainerMenu)
			// instead of P1's inventory (InventoryMenu) 
			// when attempting to access the InventoryMenu through the TweenMenu or otherwise.

			if (!glob.globalDataInit || 
				!glob.coopSessionActive ||
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Companion player controlling menus.
			if (glob.menuPID > 0)
			{
				// Close P1's inventory, if open, and open the companion player's.
				auto msgQ = RE::UIMessageQueue::GetSingleton(); 
				if (msgQ && opening)
				{
					const auto& reqP = glob.coopPlayers[glob.menuPID];
					msgQ->AddMessage
					(
						RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr
					);

					// Reset P1's damage multiplier so that the co-op player's inventory 
					// correctly reports the their damage, instead of P1's, for weapons.
					glob.player1Actor->SetActorValue(RE::ActorValue::kAttackDamageMult, 1.0f);

					// Companion player requesting to open their inventory.
					bool succ = glob.moarm->InsertRequest
					(
						reqP->playerID,
						InputAction::kInventory, 
						SteadyClock::now(), 
						RE::ContainerMenu::MENU_NAME,
						reqP->coopActor->GetHandle()
					);

					if (succ)
					{
						DBG
						(
							"Opening {}'s inventory instead of P1's.", 
							reqP->coopActor->GetName()
						);
						reqP->em->inventoryChest->OpenContainer
						(
							!RE::ContainerMenu::ContainerMode::kNPCMode
						);
					}

					// Ignore request to prevent further processing,
					// since we just opened the companion player's inventory instead.
					return RE::UI_MESSAGE_RESULTS::kIgnore;
				}
			}
			else
			{
				// Set favorited forms data for P1, including magical forms.
				const auto& coopP1 = glob.coopPlayers[0];
				coopP1->em->UpdateFavoritedFormsLists(false);
			}

			return _ProcessMessage(a_this, a_message);
		}
		
		RE::UI_MESSAGE_RESULTS LoadingMenuHooks::ProcessMessage
		(
			RE::LoadingMenu* a_this, RE::UIMessage& a_message
		)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, global data is not initialized, 
			// serializable data is not available, or this menu is not the target of the message. 
			if (ignored || 
				!glob.globalDataInit || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle opening message.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			if (!opening)
			{
				return result;
			}

			// Restore P1's data if data was copied over before this LoadingMenu opened.
			if (*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) 
			{
				DBG
				(
					"Loading menu opened with data copied (types: 0x{:X}) over to P1. "
					"Restoring P1 data. Co-op session active: {}. RESULT: {}.",
					*glob.copiedPlayerDataTypes, glob.coopSessionActive, result
				);
				GlobalCoopData::CopyOverCoopPlayerData
				(
					false, a_this->MENU_NAME, glob.player1Actor->GetHandle(), nullptr
				);
			}

			return result;
		}
		
		RE::UI_MESSAGE_RESULTS MagicMenuHooks::ProcessMessage
		(
			RE::MagicMenu* a_this, RE::UIMessage& a_message
		)
		{
			DBG("Menu: {}, type: {}.", a_message.menu, *a_message.type);
			
			// Reapply cached equip state since favoriting a spell/shout
			// and some other inputs can wipe the state and apply P1's equip state instead.
			// The menu entries are refreshed after the 'Top Menu' 
			// receives the inventory update message.
			auto ui = RE::UI::GetSingleton();
			auto strings = RE::InterfaceStrings::GetSingleton();
			if (glob.globalDataInit &&
				glob.coopSessionActive &&
				glob.menuPID > 0 &&
				ui &&
				ui->IsMenuOpen(a_this->MENU_NAME) &&
				strings &&
				a_message.menu == strings->topMenu && 
				*a_message.type == RE::UI_MESSAGE_TYPE::kInventoryUpdate)
			{
				// Re-apply equip state after the top menu is updated, which resets equip state.
				auto result = _ProcessMessage(a_this, a_message);
				glob.mim->UpdateMenuEntryEquipStates(false, false);
				return result;
			}

			// Nothing to do here, since co-op is not active, serializable data is not available,
			// or this menu is not the target of the message.
			if (!glob.globalDataInit || 
				!glob.coopSessionActive ||
				glob.serializablePlayerData.empty() ||
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide ||
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
				/*
				// Not all updates made to menu elements through the MIM apply properly.
				// Certain entry elements must be updated right before the message is propagated,
				// so that any overriding changes made by the game 
				// can be overwritten by our own changes.
				// Companion player controlling menus.
				if (glob.menuPID > 0)
				{
					auto taskInterface = SKSE::GetTaskInterface(); 
					if (!taskInterface)
					{
						return _ProcessMessage(a_this, a_message);
					}

					// Update equip state for all magic entries and refresh the item list.
					taskInterface->AddUITask
					(
						[]()
						{
							auto ui = RE::UI::GetSingleton(); 
							if (!ui)
							{
								return;
							}

							auto magicMenu = ui->GetMenu<RE::MagicMenu>(); 
							if (!magicMenu)
							{
								return;
							}

							auto view = magicMenu->uiMovie; 
							if (!view || !magicMenu->unk30)
							{
								return;
							}

							auto magicItemList = reinterpret_cast<RE::ItemList*>(magicMenu->unk30);
							if (!magicItemList)
							{
								return;
							}

							auto& magicEntryList = magicItemList->entryList;
							RE::GFxValue numItemsGFx;
							magicEntryList.GetMember("length", std::addressof(numItemsGFx));
							double numItems = numItemsGFx.GetNumber();
							for (auto i = 0; i < numItems; ++i)
							{
								RE::GFxValue entry;
								magicEntryList.GetElement(i, std::addressof(entry));
								RE::GFxValue newEquipState;
								entry.GetMember("equipState", std::addressof(newEquipState));

								// Set cached equip state.
								newEquipState.SetNumber
								(
									static_cast<double>(glob.mim->magEntryEquipStates[i])
								);
								// Apply updated entry and list.
								entry.SetMember("equipState", newEquipState);
								magicEntryList.SetElement(i, entry);
								magicItemList->view->SetVariable("entryList", magicEntryList);
							}

							// Update the magic entry list.
							view->InvokeNoReturn
							(
								"_root.Menu_mc.inventoryLists.itemList.UpdateList", nullptr, 0
							);
							DBG("Refreshed magic menu equip state.");
						}
					);
				}

				return _ProcessMessage(a_this, a_message);
				*/
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// Control is/was requested by co-op companion player.
			if (glob.lastResolvedMenuPID != -1 && glob.lastResolvedMenuPID != 0)
			{
				const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const RE::BSFixedString menuName = a_this->MENU_NAME;
					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData
					(
						opening, menuName, p->coopActor->GetHandle(), nullptr
					);

					// NO LONGER THE CASE SINCE PERSISTENT FAVORITES UPDATE:
					// Force PersistentFavorites (https://github.com/QY-MODS/PersistentFavorites)
					// to sync its cached favorites list 
					// after we import the companion player's favorites or restore P1's favorites.
					// Syncs on toggle favorites bind press: 
					// https://github.com/QY-MODS/PersistentFavorites/blob/main/src/Events.cpp#L4
					if (ALYSLC::PersistentFavoritesCompat::g_installed)
					{
						auto ue = RE::UserEvents::GetSingleton();
						auto controlMap = RE::ControlMap::GetSingleton();
						if (ue && controlMap)
						{
							auto userEvent = ue->yButton;
							auto device = RE::INPUT_DEVICE::kGamepad;
							auto keyCode = controlMap->GetMappedKey
							(
								userEvent, device, RE::UserEvents::INPUT_CONTEXT_IDS::kItemMenu
							);
							if (keyCode != 0xFF)
							{
								std::unique_ptr<RE::InputEvent* const> buttonEvent = 
								(
									std::make_unique<RE::InputEvent* const>
									(
										RE::ButtonEvent::Create
										(
											device, userEvent, keyCode, 0.0f, 1.0f
										)
									)
								);
								// Indicate that the event was sent by a companion player.
								(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
								Util::SendInputEvent(buttonEvent);
							}
						}
					}

					// Have to restore P1's favorited items here 
					// if the game ignores this call to open the menu.
					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							DBG
							(
								"Restoring P1's magic favorites, "
								"since the message to open the MagicMenu was not handled. "
								"RESULT: {}.",
								result
							);
							GlobalCoopData::CopyOverCoopPlayerData
							(
								false, menuName, p->coopActor->GetHandle(), nullptr
							);
						}
					}

					return result;
				}
			}
			else
			{
				// Set favorited forms data for P1, including magical favorites.
				const auto& coopP1 = glob.coopPlayers[0];
				coopP1->em->UpdateFavoritedFormsLists(false);
			}

			return _ProcessMessage(a_this, a_message);
		}
		
		RE::UI_MESSAGE_RESULTS RaceSexMenuHooks::ProcessMessage
		(
			RE::RaceSexMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Save and restore P1's skill levels, XP, and thresholds,
			// and re-equip all gear.

			if (a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}
			
			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto saveMgr = RE::BGSSaveLoadManager::GetSingleton();
			if (!p1 || !saveMgr)
			{
				return _ProcessMessage(a_this, a_message);
			}

			auto p1ActorBase = p1->GetActorBase();
			if (!p1ActorBase)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			if (opening && glob.globalDataInit)
			{
				DBG
				(
					"RaceMenu opening. "
					"Menu PIDs: {}, {}, {}. P1 races: 1: {}, 2: {}, charGen: {}. "
					"P1 character ID: 0x{:X}",
					glob.menuPID, 
					glob.prevMenuPID, 
					glob.mim->managerMenuPID,
					p1->race ? p1->race->formEditorID : "NONE",
					p1->race2 ? p1->race2->formEditorID : "NONE",
					p1->charGenRace ? p1->charGenRace->formEditorID : "NONE",
					saveMgr ? saveMgr->currentCharacterID : 0xDEAD
				);

				// Save race.
				glob.charGenRace = p1->race;

				glob.charGenSkillDataList.clear();
				auto currentAV = RE::ActorValue::kNone;
				for (auto i = 0; i < Skill::kTotal; ++i)
				{
					currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
					// Save P1's current XP, level, and level threshold for each skill.
					glob.charGenSkillDataList.emplace_back
					(
						RE::PlayerCharacter::PlayerSkills::Data::SkillData
						(
							p1->skills->data->skills[i].level,
							p1->skills->data->skills[i].xp,
							p1->skills->data->skills[i].levelThreshold
						)
					);
					DBG
					(
						"Saving {}'s level as {}, threshold as {}, XP as {}.",
						Util::GetActorValueName(currentAV),
						glob.charGenSkillDataList[i].level,
						glob.charGenSkillDataList[i].levelThreshold,
						glob.charGenSkillDataList[i].xp
					);
				}

				/*
				// TODO: Restore active effects and equipped gear.
				// Save the active effects.
				const auto currentActiveEffects = p1->GetActiveEffectList();
				glob.savedP1ActiveEffectsList->clear();
				if (currentActiveEffects)
				{
					for (const auto activeEffect : *p1->GetActiveEffectList())
					{
						if (!activeEffect)
						{
							continue;
						}

						DBG
						(
							"On entry: has active effect {}. Spell: {}.",
							activeEffect->effect && activeEffect->effect->baseEffect ?
							activeEffect->effect->baseEffect->GetName() :
							"NONE",
							activeEffect->spell ? activeEffect->spell->GetName() : "NONE"
						);
						
						glob.savedP1ActiveEffectsList->emplace_front(activeEffect);
					}
				}

				// Clear all equipped forms.
				glob.charGenEquippedForms.fill(nullptr);
				glob.charGenEquippedExDataLists.fill(nullptr);
				// Set weapon/magic slot forms/exData lists.
				glob.charGenEquippedForms[!EquipIndex::kLeftHand] = p1->GetEquippedObject(true);
				glob.charGenEquippedForms[!EquipIndex::kRightHand] = p1->GetEquippedObject(false);
				glob.charGenEquippedForms[!EquipIndex::kAmmo] = p1->GetCurrentAmmo();
				glob.charGenEquippedExDataLists[!EquipIndex::kLeftHand] =
				Util::GetEquippedExtraData
				(
					p1, glob.charGenEquippedForms[!EquipIndex::kLeftHand], true
				);
				glob.charGenEquippedExDataLists[!EquipIndex::kRightHand] = 
				Util::GetEquippedExtraData
				(
					p1, glob.charGenEquippedForms[!EquipIndex::kRightHand], false
				);
				glob.charGenEquippedExDataLists[!EquipIndex::kAmmo] = Util::GetEquippedExtraData
				(
					p1, glob.charGenEquippedForms[!EquipIndex::kAmmo], false
				);

				auto currentShout = p1->GetCurrentShout();
				glob.charGenEquippedForms[!EquipIndex::kVoice] = currentShout;
				glob.charGenEquippedExDataLists[!EquipIndex::kVoice] = nullptr;
				if (!currentShout)
				{
					glob.charGenEquippedForms[!EquipIndex::kVoice] = p1->selectedPower;
				}
				// Armor.
				uint32_t i = !EquipIndex::kFirstBipedSlot; 
				for (; i <= !EquipIndex::kLastBipedSlot; ++i)
				{
					auto armorInSlot = 
					(
						p1->GetWornArmor
						(
							static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>
							(
								1 << (i - !EquipIndex::kFirstBipedSlot)
							)
						)
					);
					glob.charGenEquippedForms[i] = armorInSlot;
					glob.charGenEquippedExDataLists[i] = Util::GetEquippedExtraData
					(
						p1, glob.charGenEquippedForms[i], false
					);
				}

				DBG("About to unequip all.");
				Util::Papyrus::UnequipAll(p1);
				*/

				if (ALYSLC::RaceMenuCompat::g_installed)
				{
					if (!p1->race2 || glob.coopSessionActive)
					{
						// Tell P1 to save their appearance as a preset to restore later
						// after a companion player changes their character's appearance.
						// Only show the first time the RaceMenu opens,
						// or while in co-op.
						RE::DebugMessageBox
						(
							"[ALYSLC]\nPlayer 1, "
							"please save your appearance as a preset before exiting.\n"
							"You will be prompted to reload this preset onto your character "
							"after other players have customized their characters."
						);
					}
				}
			}

			if (closing)
			{
				// A player has just finished customizing their character, 
				// so save their appearance preset.
				if (glob.globalDataInit)
				{
					DBG
					(
						"RaceMenu closing. "
						"Menu PIDs: {}, {}, {}. P1 races: 1: {}, 2: {}, charGen: {}. "
						"P1 character ID: 0x{:X}",
						glob.menuPID, 
						glob.prevMenuPID, 
						glob.mim->managerMenuPID,
						p1->race ? p1->race->formEditorID : "NONE",
						p1->race2 ? p1->race2->formEditorID : "NONE",
						p1->charGenRace ? p1->charGenRace->formEditorID : "NONE",
						saveMgr ? saveMgr->currentCharacterID : 0xDEAD
					);
					// Restore skill levels, XP, level threshold, and active effects.
					// Also re-equip all gear.
					auto currentAV = RE::ActorValue::kNone;
					for (auto i = 0; i < Skill::kTotal; ++i)
					{
						currentAV = glob.SKILL_TO_AV_MAP.at(static_cast<Skill>(i));
						p1->SetBaseActorValue(currentAV, glob.charGenSkillDataList[i].level);
						p1->skills->data->skills[i] = glob.charGenSkillDataList[i];
						DBG
						(
							"Restoring {}'s level to {}, threshold to {}, XP to {}.",
							Util::GetActorValueName(currentAV),
							glob.charGenSkillDataList[i].level,
							glob.charGenSkillDataList[i].levelThreshold,
							glob.charGenSkillDataList[i].xp
						);
					}

					/*
					// TODO: Restore active effects and equipped gear.
					auto currentEffectsList = p1->GetActiveEffectList();
					if (currentEffectsList)
					{
						for (const auto activeEffect : *currentEffectsList)
						{
							if (!activeEffect)
							{
								continue;
							}

							DBG
							(
								"On exit: has active effect {}.",
								activeEffect->effect && activeEffect->effect->baseEffect ?
								activeEffect->effect->baseEffect->GetName() :
								"NONE"
							);
						}
					}
					else
					{
						DBG("No active effects list.");
					}

					/*
					// NEEDS TESTING AFTER EXTRADATALIST SUPPORT ADDED:
					// Re-equip saved gear.
					RE::TESForm* form{ nullptr };
					RE::ExtraDataList* exDataList{ nullptr };
					auto aem = RE::ActorEquipManager::GetSingleton(); 
					auto taskInterface = SKSE::GetTaskInterface();
					if (aem && taskInterface)
					{
						for (auto i = 0; i < glob.charGenEquippedForms.size(); ++i)
						{
							form = glob.charGenEquippedForms[i];
							exDataList = glob.charGenEquippedExDataLists[i];
							// Do not include items without a loaded name,
							// such as the "SkinNaked" armor. 
							if (!form || strlen(form->GetName()) == 0)
							{
								continue;
							}

							EquipIndex currentSlot = EquipIndex::kTotal;
							if (i < !EquipIndex::kWeapMagTotal)
							{
								currentSlot = static_cast<EquipIndex>(i);
							}

							// Do not equip two handed weapons/spells twice,
							// so skip over the RH item if it is the same 2H item
							// as the earlier-equipped LH item.
							if (currentSlot == EquipIndex::kRightHand)
							{
								auto lhObj = glob.charGenEquippedForms[!EquipIndex::kLeftHand];
								if (lhObj == form && 
									form->As<RE::BGSEquipType>()->equipSlot == 
									glob.bothHandsEquipSlot)
								{
									continue;
								}
							}
								
							DBG("Re-equip {} ({:p}).",
								form->GetName(), fmt::ptr(exDataList));
							// Equip the cached item based on type.
							auto boundObj = form->As<RE::TESBoundObject>();
							switch (*form->formType)
							{
							case RE::FormType::Ammo:
							{
								if (boundObj)
								{
									const auto invCounts = p1->GetInventoryCounts();
									auto iter = invCounts.find(boundObj); 
									if (iter != invCounts.end() && iter->second > 0)
									{
										auto count = iter->second;
										taskInterface->AddTask
										(
											[aem, p1, boundObj, exDataList, count]()
											{
												Util::EquipObject
												(
													p1, boundObj, exDataList, count
												);
											}
										);
									}
								}

								break;
							}
							case RE::FormType::Shout:
							{
								taskInterface->AddTask
								(
									[aem, p1, form]()
									{
										aem->EquipShout(p1, form->As<RE::TESShout>());
									}
								);

								break;
							}
							case RE::FormType::Spell:
							{
								auto spell = form->As<RE::SpellItem>();
								RE::BGSEquipSlot* equipSlot = glob.eitherHandEquipSlot;
								if (i != !EquipIndex::kVoice)
								{
									if (spell->equipSlot == glob.bothHandsEquipSlot)
									{
										equipSlot = glob.bothHandsEquipSlot;
									}
									else
									{
										equipSlot = 
										(
											(i == !EquipIndex::kLeftHand) ? 
											glob.leftHandEquipSlot : 
											glob.rightHandEquipSlot
										);
									}
								}
								else
								{
									equipSlot = glob.voiceEquipSlot;
								}
									
								taskInterface->AddTask
								(
									[aem, p1, spell, equipSlot]()
									{
										aem->EquipSpell(p1, spell, equipSlot);
									}
								);

								break;
							}
							case RE::FormType::Weapon:
							{
								auto lhObj = p1->GetEquippedObject(true);
								auto rhObj = p1->GetEquippedObject(false);
								// Do not equip 2H weapons twice.
								if ((i == !EquipIndex::kLeftHand && form != lhObj) || 
									(i == !EquipIndex::kRightHand && form != rhObj))
								{
									auto equipSlot = glob.eitherHandEquipSlot;
									if (form->As<RE::TESObjectWEAP>()->equipSlot ==
										glob.bothHandsEquipSlot)
									{
										equipSlot = glob.bothHandsEquipSlot;
									}
									else if (i == !EquipIndex::kLeftHand)
									{
										equipSlot = glob.leftHandEquipSlot;
									}
									else if (i == !EquipIndex::kRightHand)
									{
										equipSlot = glob.rightHandEquipSlot;
									}

									if (boundObj)
									{
										taskInterface->AddTask
										(
											[aem, p1, boundObj, exDataList, equipSlot]()
											{
												Util::EquipObject
												(
													p1, boundObj, exDataList, 1, equipSlot
												);
											}
										);
									}
								}

								break;
							}
							default:
							{
								// Equip all other types of forms if they are bound objects.
								if (boundObj)
								{
									taskInterface->AddTask
									(
										[aem, p1, boundObj, exDataList]()
										{
											Util::EquipObject(p1, boundObj, exDataList);
										}
									);
								}

								break;
							}
							}
						}
					}

					// Clear equipped forms and extra data list lists.
					glob.charGenEquippedForms.fill(nullptr);
					glob.charGenEquippedExDataLists.fill(nullptr);
					*/
				}

				// P1 is editing their appearance.
				if (!glob.globalDataInit || 
					glob.menuPID == -1 ||
					glob.menuPID == 0)
				{
					// Set vampiric race.
					if (p1->race)
					{
						bool isVampire = false;
						auto defObjMgr = RE::BGSDefaultObjectManager::GetSingleton();
						if (defObjMgr)
						{
							auto obj = defObjMgr->objects
							[
								RE::DEFAULT_OBJECTS::kPlayerIsVampireVariable
							];
							if (obj)
							{
								auto p1VampireGlob = obj->As<RE::TESGlobal>();
								isVampire = p1VampireGlob && p1VampireGlob->value == 1.0f;
							}
						}

						if (isVampire && !p1->race->HasKeywordByEditorID("Vampire"))
						{
							auto vampiricRace = RE::TESForm::LookupByEditorID<RE::TESRace>
							(
								fmt::format
								(
									"{}Vampire", p1->race->GetFormEditorID()
								).c_str()
							);
							if (vampiricRace)
							{
								DBG
								(
									"Restoring vampiric race {}.", vampiricRace->GetFormEditorID()
								);
							
								p1->race = 
								p1->race2 = 
								p1->charGenRace = 
								p1ActorBase->originalRace = 
								p1ActorBase->race = vampiricRace;
							}
						}
					}

					// Save race as P1's chosen race.
					if (glob.globalDataInit)
					{
						const auto iter = glob.serializablePlayerData.find(p1->formID);
						if (iter != glob.serializablePlayerData.end())
						{
							iter->second->chosenRace = p1->charGenRace;
							DBG
							(
								"Saving P1's chosen race as {} (0x{:X}, editor ID {}).",
								iter->second->chosenRace ? 
								iter->second->chosenRace->GetName() :
								"NONE",
								iter->second->chosenRace ? 
								iter->second->chosenRace->formID :
								0xDEAD,
								Util::GetEditorID(iter->second->chosenRace)
							);
						}
					}
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS SleepWaitMenuHooks::ProcessMessage
		(
			RE::SleepWaitMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Nothing to do here, since the message is ignored, global data is not initialized, 
			// serializable data is not available, or this menu is not the target of the message. 
			if (!glob.globalDataInit || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}
			
			// Only need to handle closing message if another player opened this menu.
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide;
			if (closing)
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				// Reset to default package and stop interacting.
				if (glob.menuPID > 0)
				{
					const auto& p = glob.coopPlayers[glob.menuPID];
					p->mm->interactionPackageRunning = false;
					p->pam->SetAndEveluatePackage();
					p->coopActor->StopInteractingQuick(true);
					// IMPORTANT:
					// If not clearing out the furniture data set when opening the menu, 
					// no player will be able to open most menus
					// (Tween, Stats, Inventory, Map, etc.).
					// Cannot just clear the currently occupied furniture handle,
					// as this will lead to locking players out of using the furniture.
					if (p1)
					{
						DBG
						(
							"Clear P1's occupied furniture handle ({}) "
							"when done interacting.",
							Util::HandleIsValid(p1->GetOccupiedFurniture()) ?
							p1->GetOccupiedFurniture().get()->GetName() : 
							"NONE"
						);
						p1->StopInteractingQuick(true);
					}
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS StatsMenuHooks::ProcessMessage
		(
			RE::StatsMenu* a_this, RE::UIMessage& a_message
		)
		{
			auto view = a_this->uiMovie;
			RE::UI_MESSAGE_RESULTS result = RE::UI_MESSAGE_RESULTS::kPassOn;
			// True if this update modifies P1's name, race name, and HMS meters.
			// Want to undo this update's changes while a companion player is controlling menus
			// and should have their data imported onto P1.
			bool modifiedNames = false;
			if (view)
			{
				RE::GFxValue base{ };
				view->GetVariable
				(
					std::addressof(base), "_root.StatsMenuBaseInstance"
				);
				if (base.IsNull() || base.IsUndefined())
				{
					return _ProcessMessage(a_this, a_message);;
				}
			
				RE::BSFixedString nameBefore{ };
				RE::BSFixedString raceNameBefore{ };
				RE::GFxValue firstLastLabel{ };
				view->GetVariable
				(
					std::addressof(firstLastLabel), 
					"_root.StatsMenuBaseInstance.TopPlayerInfo.FirstLastLabel"
				);
				if (!firstLastLabel.IsNull() && !firstLastLabel.IsUndefined())
				{
					RE::GFxValue text{ };
					firstLastLabel.GetMember("htmlText", std::addressof(text));
					nameBefore = text.GetString();
				}

				RE::GFxValue raceValueLabel{ };
				view->GetVariable
				(
					std::addressof(raceValueLabel), 
					"_root.StatsMenuBaseInstance.TopPlayerInfo.RacevalueLabel"
				);
				if (!raceValueLabel.IsNull() && !raceValueLabel.IsUndefined())
				{
					RE::GFxValue text{ };
					raceValueLabel.GetMember("htmlText", std::addressof(text));
					raceNameBefore = text.GetString();
				}

				// Let the other handlers process the message first 
				// before we potentially modify the UI or import another player's data.
				result = _ProcessMessage(a_this, a_message);
			
				view->GetVariable
				(
					std::addressof(firstLastLabel), 
					"_root.StatsMenuBaseInstance.TopPlayerInfo.FirstLastLabel"
				);
				if (!firstLastLabel.IsNull() && !firstLastLabel.IsUndefined())
				{
					RE::GFxValue text{ };
					firstLastLabel.GetMember("htmlText", std::addressof(text));
					modifiedNames = nameBefore != text.GetString();
				}

				view->GetVariable
				(
					std::addressof(raceValueLabel), 
					"_root.StatsMenuBaseInstance.TopPlayerInfo.RacevalueLabel"
				);
				if (!raceValueLabel.IsNull() && !raceValueLabel.IsUndefined())
				{
					RE::GFxValue text{ };
					raceValueLabel.GetMember("htmlText", std::addressof(text));
					modifiedNames |= nameBefore != text.GetString();
				}
			}
			else
			{
				result = _ProcessMessage(a_this, a_message);
			}

			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, global data is not initialized, 
			// or serializable data is not available. 
			if (ignored || !glob.globalDataInit || glob.serializablePlayerData.empty())
			{
				return result;
			}

			// P1 must be valid below.
			auto p1 = RE::PlayerCharacter::GetSingleton();
			if (!p1)
			{
				return result;
			}

			// TODO: 
			// Implement Vampire Lord and Werewolf perk sync when co-op players are transformed.
			// So for now, do not modify perk data if P1 is transformed.
			bool p1IsTransformed = Util::IsWerewolf(p1) || Util::IsVampireLord(p1);

			// Is P1 requesting to open the StatsMenu?
			// Have to also adjust perk data for P1 when outside of co-op.
			bool p1Req = !glob.coopSessionActive;
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide || 
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (glob.coopSessionActive)
			{
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				RE::ActorPtr playerInMenusPtr{ nullptr };
				if (opening || closing)
				{
					// Do not modify the requests queue, 
					// since the menu input manager still needs this info
					// when setting the request and menu player IDs when this menu opens/closes.
					glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
					(
						a_this->MENU_NAME, false
					);

					DBG
					(
						"Current menu PID: {}, resolved menu PID: {}. "
						"Opening: {}, closing: {}, has copied data: {}.",
						glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
					);
					if (glob.lastResolvedMenuPID > -1)
					{
						playerInMenusPtr = glob.coopPlayers[glob.lastResolvedMenuPID]->coopActor;
					}
				}
				else if (glob.mim->IsRunning() && glob.mim->managerMenuPID > 0)
				{
					// Set to companion player controlling menus.
					playerInMenusPtr = glob.coopPlayers[glob.mim->managerMenuPID]->coopActor;
				}

				if (playerInMenusPtr && view)
				{
					RE::GFxValue base{ };
					RE::GFxValue firstLastLabel{ };
					RE::GFxValue raceValueLabel{ };
					view->GetVariable
					(
						std::addressof(base), "_root.StatsMenuBaseInstance"
					);
					if (base.IsNull() || base.IsUndefined())
					{
						return result;
					}
			
					const auto iter = glob.serializablePlayerData.find(playerInMenusPtr->formID);
					if (iter == glob.serializablePlayerData.end())
					{
						return result;
					}

					const auto& data = iter->second;
					// CHANGE TO DEBUG
					DBG
					(
						"[HMS Breakdown] "
						"Event type {}, "
						"Current levels displayed on P1: H: {}, M: {}, S: {}. "
						"P1 current base: H: {}, M: {}, S: {}. "
						"P1 current permanent: H: {}, M: {}, S: {}. " 
						"P1 base values recorded on entry: H: {}, M: {}, S: {}. "
						"{}'s modifiers (temp, permanent, damage): "
						"H: ({}, {}, {}), M: ({}, {}, {}), S: ({}, {}, {}). "
						"{}'s HMS values: "
						"Current levels: H: {}, M: {}, S: {}. "
						"Current base: H: {}, M: {}, S: {}. "
						"Current permanent: H: {}, M: {}, S: {}. "
						"Serialized values: "
						"Base: H: {}, M: {}, S: {}. "
						"Serialized increases: H: {}, M: {}, S: {}. "
						"Current increases: H: {}, M: {}, S: {}. "
						"To display on P1: H: ({} / {}), M: ({} / {}), S: ({} / {})",
						*a_message.type,
						p1->GetActorValue(RE::ActorValue::kHealth),
						p1->GetActorValue(RE::ActorValue::kMagicka),
						p1->GetActorValue(RE::ActorValue::kStamina),
						p1->GetBaseActorValue(RE::ActorValue::kHealth),
						p1->GetBaseActorValue(RE::ActorValue::kMagicka),
						p1->GetBaseActorValue(RE::ActorValue::kStamina),
						p1->GetPermanentActorValue(RE::ActorValue::kHealth),
						p1->GetPermanentActorValue(RE::ActorValue::kMagicka),
						p1->GetPermanentActorValue(RE::ActorValue::kStamina),
						data->p1HMSBaseAVsOnMenuEntry[0],
						data->p1HMSBaseAVsOnMenuEntry[1],
						data->p1HMSBaseAVsOnMenuEntry[2],
						playerInMenusPtr->GetName(),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
						),
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
						),
						playerInMenusPtr->GetName(),
						playerInMenusPtr->GetActorValue(RE::ActorValue::kHealth),
						playerInMenusPtr->GetActorValue(RE::ActorValue::kMagicka),
						playerInMenusPtr->GetActorValue(RE::ActorValue::kStamina),
						playerInMenusPtr->GetBaseActorValue(RE::ActorValue::kHealth),
						playerInMenusPtr->GetBaseActorValue(RE::ActorValue::kMagicka),
						playerInMenusPtr->GetBaseActorValue(RE::ActorValue::kStamina),
						playerInMenusPtr->GetPermanentActorValue(RE::ActorValue::kHealth),
						playerInMenusPtr->GetPermanentActorValue(RE::ActorValue::kMagicka),
						playerInMenusPtr->GetPermanentActorValue(RE::ActorValue::kStamina),
						data->hmsBasePointsList[0],
						data->hmsBasePointsList[1],
						data->hmsBasePointsList[2],
						data->hmsPointIncreasesList[0],
						data->hmsPointIncreasesList[1],
						data->hmsPointIncreasesList[2],
						p1->GetBaseActorValue(RE::ActorValue::kHealth) - 
						data->p1HMSBaseAVsOnMenuEntry[0],
						p1->GetBaseActorValue(RE::ActorValue::kMagicka) - 
						data->p1HMSBaseAVsOnMenuEntry[1],
						p1->GetBaseActorValue(RE::ActorValue::kStamina) - 
						data->p1HMSBaseAVsOnMenuEntry[2],
						(
							data->hmsBasePointsList[0] + 
							data->hmsPointIncreasesList[0] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kHealth) - 
								data->p1HMSBaseAVsOnMenuEntry[0]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
							) 	 + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
							)
						),
						(
							data->hmsBasePointsList[0] + 
							data->hmsPointIncreasesList[0] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kHealth) - 
								data->p1HMSBaseAVsOnMenuEntry[0]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
							) 	
						),
						(
							data->hmsBasePointsList[1] + 
							data->hmsPointIncreasesList[1] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kMagicka) - 
								data->p1HMSBaseAVsOnMenuEntry[1]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
							)
						),
						(
							data->hmsBasePointsList[1] + 
							data->hmsPointIncreasesList[1] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kMagicka) - 
								data->p1HMSBaseAVsOnMenuEntry[1]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
							) 	
						),
						(
							data->hmsBasePointsList[2] + 
							data->hmsPointIncreasesList[2] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kStamina) - 
								data->p1HMSBaseAVsOnMenuEntry[2]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
							)
						),
						(
							data->hmsBasePointsList[2] + 
							data->hmsPointIncreasesList[2] +
							(
								p1->GetBaseActorValue(RE::ActorValue::kStamina) - 
								data->p1HMSBaseAVsOnMenuEntry[2]
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
							) + 
							playerInMenusPtr->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
							) 	
						)
					);
					view->GetVariable
					(
						std::addressof(firstLastLabel), 
						"_root.StatsMenuBaseInstance.TopPlayerInfo.FirstLastLabel"
					);
					if (!firstLastLabel.IsNull() && !firstLastLabel.IsUndefined())
					{
						firstLastLabel.SetTextHTML(playerInMenusPtr->GetName());
					}

					view->GetVariable
					(
						std::addressof(raceValueLabel), 
						"_root.StatsMenuBaseInstance.TopPlayerInfo.RacevalueLabel"
					);
					if (!raceValueLabel.IsNull() && !raceValueLabel.IsUndefined())
					{
						raceValueLabel.SetTextHTML(playerInMenusPtr->race->GetName());
					}

					RE::GFxValue args[4];
					// Magicka (current, full, color).
					float tempAndPermMod = 
					(
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary,
							RE::ActorValue::kMagicka
						) + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent,
							RE::ActorValue::kMagicka
						) 	
					);
					// FULL:
					// Companion player's recorded base amount + their recorded increase so far +
					// the current change while in the Stats Menu + 
					// any temporary and permanent modifiers from gear, perks, etc.
					// (applied to P1 until export when the menu closes).
					float fullValue = 
					(
						data->hmsBasePointsList[1] + 
						data->hmsPointIncreasesList[1] +
						(
							p1->GetBaseActorValue(RE::ActorValue::kMagicka) - 
							data->p1HMSBaseAVsOnMenuEntry[1]
						) + 
						tempAndPermMod
					);
					// CURRENT:
					// The max value above modified by the companion player's 
					// current damage AV modifier.
					float currentValue = 
					(
						fullValue + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
						)
					);
			
					args[0] = RE::GFxValue(0);
					args[1] = RE::GFxValue(std::roundf(currentValue));
					args[2] = RE::GFxValue(std::roundf(fullValue));
					args[3] = RE::GFxValue
					(
						tempAndPermMod == 0.0f ? 0xFFFFFF : 
						tempAndPermMod < 0.0f ? 0xFF0000 :
						0x00FF00
					);
					base.Invoke("SetMeter", nullptr, args, 4);

					// Health (current, full, color).
					tempAndPermMod = 
					(
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary,
							RE::ActorValue::kHealth
						) + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent,
							RE::ActorValue::kHealth
						) 	
					);
					fullValue = 
					(
						data->hmsBasePointsList[0] + 
						data->hmsPointIncreasesList[0] +
						(
							p1->GetBaseActorValue(RE::ActorValue::kHealth) - 
							data->p1HMSBaseAVsOnMenuEntry[0]
						) + 
						tempAndPermMod
					);
					currentValue = 
					(
						fullValue + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
						)
					);
					args[0] = RE::GFxValue(1);
					args[1] = RE::GFxValue(std::roundf(currentValue));
					args[2] = RE::GFxValue(std::roundf(fullValue));
					args[3] = RE::GFxValue
					(
						tempAndPermMod == 0.0f ? 0xFFFFFF : 
						tempAndPermMod < 0.0f ? 0xFF0000 :
						0x00FF00
					);
					base.Invoke("SetMeter", nullptr, args, 4);

					// Stamina (current, max, color).
					tempAndPermMod = 
					(
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary,
							RE::ActorValue::kStamina
						) + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kPermanent,
							RE::ActorValue::kStamina
						) 	
					);
					fullValue = 
					(
						data->hmsBasePointsList[2] + 
						data->hmsPointIncreasesList[2] +
						(
							p1->GetBaseActorValue(RE::ActorValue::kStamina) - 
							data->p1HMSBaseAVsOnMenuEntry[2]
						) + 
						tempAndPermMod
					);
					currentValue = 
					(
						fullValue + 
						playerInMenusPtr->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
						)
					);
					args[0] = RE::GFxValue(2);
					args[1] = RE::GFxValue(std::roundf(currentValue));
					args[2] = RE::GFxValue(std::roundf(fullValue));
					args[3] = RE::GFxValue
					(
						tempAndPermMod == 0.0f ? 0xFFFFFF : 
						tempAndPermMod < 0.0f ? 0xFF0000 :
						0x00FF00
					);
					base.Invoke("SetMeter", nullptr, args, 4);
				}

				// No need to handle cases where the menu is opening or closing below here.
				if (!opening && !closing)
				{
					// Set as handled if the player's name or race name were modified back to P1's.
					// Do not want any other handlers to re-apply P1's data 
					// over the companion player's.
					if (modifiedNames)
					{
						return RE::UI_MESSAGE_RESULTS::kHandled;
					}
					else
					{
						return result;
					}
				}

				// Control is/was requested by a companion player.
				if (glob.lastResolvedMenuPID != -1 && 
					glob.lastResolvedMenuPID != 0 && 
					!p1IsTransformed)
				{
					// Copy back player data only if data was already copied.
					// Ignore subsequent hide messages once P1's data is restored.
					closing &= hasCopiedData;
					if (opening || closing)
					{
						const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
						const RE::BSFixedString menuName = a_this->MENU_NAME;
						// Copy over player data.
						GlobalCoopData::CopyOverCoopPlayerData
						(
							opening, menuName, p->coopActor->GetHandle(), nullptr
						);
					}
				}
				else
				{
					// If another player is not requesting control, default to P1.
					p1Req = true;
				}
			}

			// Don't adjust data if Enderal is installed.
			if ((p1Req && !p1IsTransformed && !ALYSLC::EnderalCompat::g_installed) && 
				(opening || closing))
			{
				GlobalCoopData::AdjustPerkDataForPlayer1(opening);
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS TrainingMenuHooks::ProcessMessage
		(
			RE::TrainingMenu* a_this, RE::UIMessage& a_message
		)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, 
			// or this menu is not the target of the message. 
			if (!glob.globalDataInit ||
				!glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || 
				a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = 
			(
				*a_message.type == RE::UI_MESSAGE_TYPE::kHide ||
				*a_message.type == RE::UI_MESSAGE_TYPE::kForceHide
			);
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu player IDs when this menu opens/closes.
			glob.lastResolvedMenuPID = glob.moarm->ResolveMenuPlayerID
			(
				a_this->MENU_NAME, false
			);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			DBG
			(
				"Current menu PID: {}, resolved menu PID: {}. "
				"Opening: {}, closing: {}, has copied data: {}.",
				glob.menuPID, glob.lastResolvedMenuPID, opening, closing, hasCopiedData
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((glob.lastResolvedMenuPID <= 0) || (!opening && !closing))
			{
				return _ProcessMessage(a_this, a_message);
			}

			const auto& p = glob.coopPlayers[glob.lastResolvedMenuPID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;
			RE::TESForm* assocForm = nullptr;
			// Set speaker as associated form.
			auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
			if ((menuTopicManager) && 
				(menuTopicManager->speaker.get() || menuTopicManager->lastSpeaker.get()))
			{
				auto speaker = 
				(
					menuTopicManager->speaker.get() ? 
					menuTopicManager->speaker.get() : 
					menuTopicManager->lastSpeaker.get()
				);
				assocForm = speaker.get();
			}

			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), assocForm
			);

			auto result = _ProcessMessage(a_this, a_message);
			if (opening)
			{
				// Have to restore P1's AVs here if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					DBG
					(
						"Restoring AVs for {} and P1, "
						"since the message to open the menu was ignored. RESULT: {}.", 
						p->coopActor->GetName(), result
					);
					GlobalCoopData::CopyOverCoopPlayerData
					(
						false, menuName, p->coopActor->GetHandle(), assocForm
					);
				}
			}

			return result;
		}

//====================
// [P1 HANDLER HOOKS]:
//====================

		bool ActivateHandlerHooks::CanProcess(RE::ActivateHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto buttonEvent = a_event->AsButtonEvent();
			bool hasBypassFlag = 
			(
				(buttonEvent) && (buttonEvent->pad24 & 0xFFFF) == 0xC0DA
			);
			auto charController = p1->GetCharController();
			const bool& canUseParaglider = 
			{
				ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
				p1 && 
				charController && 
				charController->context.currentState == RE::hkpCharacterStateType::kInAir
			};

			// 'Activate' event name and has P1 proxied bypass flag or the player has a paraglider.
			if ((a_event->QUserEvent() == ue->activate) && (hasBypassFlag || canUseParaglider))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool AttackBlockHandlerHooks::CanProcess
		(
			RE::AttackBlockHandler* a_this, RE::InputEvent* a_event
		)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->rightAttack ||
						p1GameplayContextEvent == ue->leftAttack)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			const auto& eventName = a_event->QUserEvent();
			// Only the left and right attack event names (not left/right equip).
			if ((a_event->AsButtonEvent()) && 
				(eventName == ue->rightAttack || eventName == ue->leftAttack))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		
		bool JumpHandlerHooks::CanProcess(RE::JumpHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->jump)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			auto buttonEvent = a_event->AsButtonEvent();
			// 'Jump' event name and has P1 proxied bypass flag.
			if ((buttonEvent) && (a_event->QUserEvent() == ue->jump) && 
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool LookHandlerHooks::CanProcess(RE::LookHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; 
			// ignore since we don't want another player controlling the camera orientation.

			auto idEvent = a_event->AsIDEvent();
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0  &&
				!Util::MenusOnlyAlwaysOpen() &&
				!Util::OpenMenuStopsMovement())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->look ||
						p1GameplayContextEvent == ue->rotate ||
						p1GameplayContextEvent == ue->zoomIn ||
						p1GameplayContextEvent == ue->zoomOut)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && glob.globalDataInit && glob.coopSessionActive && glob.menuPID > 0)
			{
				// Allow mouse or RS camera adjustment processing when motion driven. 
				// Otherwise, do not handle this look event.
				bool menuStopsMovement = Util::OpenMenuStopsMovement();
				auto p1 = RE::PlayerCharacter::GetSingleton(); 
				auto mouseMovementEvent = 
				(
					a_event->GetDevice() == RE::INPUT_DEVICE::kMouse && 
					*a_event->eventType == RE::INPUT_EVENT_TYPE::kMouseMove ? 
					skyrim_cast<RE::MouseMoveEvent*>(a_event) :
					nullptr
				);
				auto thumbstickEvent = 
				(
					a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad && 
					*a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick ? 
					skyrim_cast<RE::ThumbstickEvent*>(a_event) :
					nullptr
				);
				bool isSupportedEvent = 
				(
					(mouseMovementEvent) ||
					(thumbstickEvent && thumbstickEvent->IsRight())
				);
				if (p1 && 
					p1->movementController && 
					p1->movementController->controlsDriven && 
					!menuStopsMovement &&
					isSupportedEvent)
				{
					// Default behavior if override held.
					if (p1OverrideHeld)
					{
						return _CanProcess(a_this, a_event);
					}
					else
					{
						// Allow if not blocked otherwise.
						return true;
					}
				}
				else
				{
					// Thumbstick event that is from P1 and P1 is not motion driven.
					// Ignore when the co-op camera is active.
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool MovementHandlerHooks::CanProcess(RE::MovementHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; 
			// ignore since we don't want another player to control P1's movement.

			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen() &&
				!Util::OpenMenuStopsMovement())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->forward ||
						p1GameplayContextEvent == ue->back ||
						p1GameplayContextEvent == ue->strafeLeft ||
						p1GameplayContextEvent == ue->strafeRight ||
						p1GameplayContextEvent == ue->move ||
						p1GameplayContextEvent == ue->readyWeapon)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			auto thumbstickEvent = 
			(
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad && 
				*a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick ? 
				skyrim_cast<RE::ThumbstickEvent*>(a_event) :
				nullptr
			);

			// Thumbstick event that is not from a companion player.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				// Allow WASD or LS movement processing when motion driven. 
				// Otherwise, do not handle this movement event.
				bool menuStopsMovement = Util::OpenMenuStopsMovement();
				const auto eventName = a_event->QUserEvent();
				auto p1 = RE::PlayerCharacter::GetSingleton(); 
				bool isSupportedEvent = 
				(
					(thumbstickEvent && thumbstickEvent->IsLeft()) ||
					(
						eventName == ue->strafeLeft ||
						eventName == ue->strafeRight ||
						eventName == ue->forward ||
						eventName == ue->back
					)
				);
				if (p1 && 
					p1->movementController && 
					p1->movementController->controlsDriven && 
					!menuStopsMovement && 
					isSupportedEvent)
				{
					// NOTE: 
					// Completely unnecessary if TDM is installed.
					if (thumbstickEvent &&
						!ALYSLC::TrueDirectionalMovementCompat::g_installed) 
					{
						// Adjust the thumbstick event stick displacements
						// so that P1 moves relative to the co-op camera 
						// instead of their own facing direction while motion driven.
						if (thumbstickEvent->xValue != 0.0f && thumbstickEvent->yValue != 0.0f)
						{
							float p1FacingToCamYawDiff = -Util::NormalizeAngToPi
							(
								glob.cam->camYaw - p1->data.angle.z
							);
							float thumbstickAngle = Util::NormalizeAng0To2Pi
							(
								atan2f(thumbstickEvent->yValue, thumbstickEvent->xValue)
							);
							thumbstickAngle = Util::NormalizeAng0To2Pi
							(
								thumbstickAngle + p1FacingToCamYawDiff
							);
							thumbstickEvent->xValue = cosf(thumbstickAngle);
							thumbstickEvent->yValue = sinf(thumbstickAngle);
						}
					}

					// Default behavior if override held.
					if (p1OverrideHeld)
					{
						return _CanProcess(a_this, a_event);
					}
					else
					{
						// Allow otherwise.
						return true;
					}
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool ReadyWeaponHandlerHooks::CanProcess
		(
			RE::ReadyWeaponHandler* a_this, RE::InputEvent* a_event
		)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->readyWeapon)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			// 'Ready Weapon' event and has P1 proxied bypass flag.
			auto buttonEvent = a_event->AsButtonEvent();
			if ((buttonEvent) && 
				(a_event->QUserEvent() == ue->readyWeapon) &&
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA))

			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool ShoutHandlerHooks::CanProcess(RE::ShoutHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->shout)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad, or when P1 is not using co-op binds.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			// 'Shout' event and has P1 proxied bypass flag.
			auto buttonEvent = a_event->AsButtonEvent();
			if ((buttonEvent) && 
				(a_event->QUserEvent() == ue->shout) &&
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool SneakHandlerHooks::CanProcess(RE::SneakHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->sneak)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			// 'Sneak' event and has P1 proxied bypass flag.
			auto buttonEvent = a_event->AsButtonEvent();
			if ((buttonEvent) && 
				(a_event->QUserEvent() == ue->sneak) &&
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool SprintHandlerHooks::CanProcess(RE::SprintHandler* a_this, RE::InputEvent* a_event)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Switch event name to match the corresponding event name in the gameplay context
			// to allow P1 to perform gameplay actions while another player is controlling menus.
			if (GlobalCoopData::IsP1UsingSingleplayerControlsInCoop() &&
				glob.menuPID > 0 &&
				!Util::MenusOnlyAlwaysOpen())
			{
				auto ue = RE::UserEvents::GetSingleton();
				auto controlMap = RE::ControlMap::GetSingleton();
				auto buttonEvent = a_event->AsButtonEvent();
				auto idEvent = a_event->AsIDEvent();
				bool p1OverrideHeld = Util::IsKeyPressed(GlobalCoopData::P1_OVERRIDE_KEY);
				if (ue && controlMap && buttonEvent && idEvent && !p1OverrideHeld)
				{
					auto p1GameplayContextEvent = 
					(
						controlMap->GetUserEventName
						(
							buttonEvent->idCode, *a_event->device
						)
					);
					if (p1GameplayContextEvent == ue->sprint)
					{
						idEvent->userEvent = p1GameplayContextEvent;
						return true;
					}
				}
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->GetDevice() != RE::INPUT_DEVICE::kGamepad ||
				!glob.coopPlayers[0]->IsRunning()) 
			{
				return _CanProcess(a_this, a_event);
			}

			// 'Sprint' event and has P1 proxied bypass flag
			auto buttonEvent = a_event->AsButtonEvent();
			if ((buttonEvent) && 
				(a_event->QUserEvent() == ue->sprint) &&
				((buttonEvent->pad24 & 0xFFFF) == 0xC0DA))
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool TogglePOVHandlerHooks::CanProcess
		(
			RE::TogglePOVHandler* a_this, RE::InputEvent* a_event
		)
		{
			// From companion player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && 
				((idEvent->pad24 & 0xFFFF) == 0xCA11 || (idEvent->pad24 & 0xFFFF) == 0xDEAD))
			{
				return false;
			}

			// While in hybrid mode, if the event is from P2,
			// who is using the controller that controls P1's character, ignore it.
			if (glob.hybridModeActive &&
				glob.coopSessionActive && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			// Ignore while the co-op camera is active to prevent POV changes.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && 
				glob.globalDataInit && 
				glob.coopSessionActive &&
				glob.cam->IsRunning() && 
				a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			return _CanProcess(a_this, a_event);
		}
	}
}

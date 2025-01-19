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
			ActorEquipManagerHooks::InstallHooks();
			ActivateHandlerHooks::InstallHooks();
			AIProcessHooks::InstallHooks();
			AnimationGraphManagerHooks::InstallHooks();
			AttackBlockHandlerHooks::InstallHooks();
			BarterMenuHooks::InstallHooks();
			BookMenuHooks::InstallHooks();
			BSCullingProcessHooks::InstallHooks();
			BSMultiBoundHooks::InstallHooks();
			CharacterHooks::InstallHooks();
			ContainerMenuHooks::InstallHooks();
			CraftingMenuHooks::InstallHooks();
			DialogueMenuHooks::InstallHooks();
			FavoritesMenuHooks::InstallHooks();
			JumpHandlerHooks::InstallHooks();
			InventoryMenuHooks::InstallHooks();
			LoadingMenuHooks::InstallHooks();
			LookHandlerHooks::InstallHooks();
			MagicMenuHooks::InstallHooks();
			MeleeHitHooks::InstallHooks();
			MenuControlsHooks::InstallHooks();
			MovementHandlerHooks::InstallHooks();
			NiNodeHooks::InstallHooks();
			PlayerCameraTransitionStateHooks::InstallHooks();
			PlayerCharacterHooks::InstallHooks();
			ProjectileHooks::InstallHooks();
			ReadyWeaponHandlerHooks::InstallHooks();
			ShoutHandlerHooks::InstallHooks();
			SneakHandlerHooks::InstallHooks();
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
			SPDLOG_INFO("[Hooks] Installed all hooks");
		}

//=============
// [MAIN HOOK]:
//=============

		void MainHook::Update(RE::Main* a_this, float a_a2)
		{
			// Run the game's update first.
			_Update(a_this, a_a2);

			// Skip if global data isn't set yet.
			if (!glob.globalDataInit)
			{
				return;
			}

			// Handle any changes to Enderal progression.
			// Eg. P1 level ups, or changes to crafting, memory, or learning points.
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive)
					{
						// NOTE: Update funcs must be run in this order.
						p->Update();
						p->em->Update();
						p->pam->Update();
						p->mm->Update();
						p->tm->Update();

						if (p->isDowned)
						{
							p->UpdateWhenDowned();
						}
					}
				}
			}

			// Update crosshair text after the players' managers have run their updates.
			if (glob.coopSessionActive && !glob.loadingASave) 
			{
				GlobalCoopData::HandlePlayerArmCollisions();
				GlobalCoopData::SetCrosshairText();
			}
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
			if (playerIndex == -1)
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
			if (playerIndex == -1)
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
			if (playerIndex == -1)
			{
				return _AIProcess_SetRotationSpeedZ3(a_this, a_rotationSpeed);
			}
		}

// [ACTOR EQUIP MANAGER HOOKS]:
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

			// Ignore if P1 or transform(ing/ed).
			const auto& p = glob.coopPlayers[playerIndex];
			if (p->isPlayer1 || p->isTransforming || p->isTransformed)
			{
				return _EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}
					
			SPDLOG_DEBUG("[ActorEquipManager Hooks] EquipObject: {}: {} (0x{:X}, type: 0x{:X}).", 
				a_actor->GetName(), a_object->GetName(), a_object->formID, *a_object->formType);

			if (auto reqEquipSlot = a_objectEquipParams.equipSlot; reqEquipSlot)
			{
				// Ensure 1H weapon gets equipped in the correct hand.
				// Certain races, like the Manakin race, 
				// do not equip weapons in the LH 
				// unless the weapon's equip slot is set to the LH slot.
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
								itemEquipType->SetEquipSlot(glob.eitherHandEquipSlot);
							}
							// Left equip slot to right. Set to right.
							else if (reqEquipSlot == glob.rightHandEquipSlot && 
									 itemEquipType->equipSlot == glob.leftHandEquipSlot)
							{
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
			if (!isBound && a_object != glob.fists)
			{
				//=================================================
				// Cases in which the equip call should be skipped.
				//=================================================

				// Game tries to equip another ammo that was not requested.
				if (a_object->IsAmmo() && 
					a_object != p->em->desiredEquippedForms[!EquipIndex::kAmmo])
				{
					SPDLOG_DEBUG
					(
						"[ActorEquipManager Hooks] {}: trying to equip ammo {}. Ignoring.", 
						a_actor->GetName(), a_object->GetName()
					);
					return;
				}
				else if (a_object->IsArmor())
				{
					if (auto armor = a_object->As<RE::TESObjectARMO>(); armor->IsShield())
					{
						// Game tries to equip a shield that was not requested in the left hand.
						if (armor != p->em->desiredEquippedForms[!EquipIndex::kLeftHand])
						{
							return;
						}
					}
					else
					{
						auto bipedSlotMask = !armor->GetSlotMask();
						int8_t indexOffset = -1;
						// Get equip index offset (from first biped slot index) 
						// by shifting biped slot index right until equal to 0.
						// Number of shifts = index offset.
						while (bipedSlotMask != 0)
						{
							bipedSlotMask = bipedSlotMask >> 1;
							indexOffset++;
						}

						// Only if biped slot is valid.
						if (indexOffset != -1)
						{
							uint8_t equipIndex = !EquipIndex::kFirstBipedSlot + indexOffset;
							// Game is trying to equip armor that the player 
							// did not request to have equipped.
							if (armor != p->em->desiredEquippedForms[equipIndex])
							{
								SPDLOG_DEBUG
								(
									"[ActorEquipManager Hooks] {}: "
									"trying to equip armor {}. Ignoring.", 
									a_actor->GetName(), a_object->GetName()
								);
								return;
							}
						}
					}
				}
				else if (a_object->IsWeapon())
				{
					uint8_t indexToCheck = !EquipIndex::kRightHand;
					bool isLHEquip = 
					(
						(a_objectEquipParams.equipSlot == glob.leftHandEquipSlot) || 
						(
							a_object->As<RE::BGSEquipType>() && 
							a_object->As<RE::BGSEquipType>()->equipSlot == glob.leftHandEquipSlot
						)
					);
					if (isLHEquip)
					{
						indexToCheck = !EquipIndex::kLeftHand;
					}

					// Game is trying to equip a weapon that the player 
					// did not request to have equipped.
					if (a_object != p->em->desiredEquippedForms[indexToCheck])
					{
						SPDLOG_DEBUG
						(
							"[ActorEquipManager Hooks] {}: trying to equip weapon {}. Ignoring.",
							a_actor->GetName(), a_object->GetName()
						);
						return;
					}
				}
				else if (a_object->As<RE::TESShout>())
				{
					if (a_object != p->em->desiredEquippedForms[!EquipIndex::kVoice]) 
					{
						SPDLOG_DEBUG
						(
							"[ActorEquipManager Hooks] {}: trying to equip shout {}. Ignoring.", 
							a_actor->GetName(), a_object->GetName()
						);
						return;
					}
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
						auto equipSlotToUse = 
						(
							weap->equipSlot == glob.bothHandsEquipSlot ?
							weap->equipSlot : 
							a_objectEquipParams.equipSlot
						);
						bool reqToEquip = 
						(
							(
								equipSlotToUse == glob.leftHandEquipSlot && 
								p->pam->boundWeapReqLH
							) ||
							(
								equipSlotToUse == glob.rightHandEquipSlot && 
								p->pam->boundWeapReqRH
							) ||
							(
								equipSlotToUse == glob.bothHandsEquipSlot && 
								p->pam->boundWeapReqLH && 
								p->pam->boundWeapReqRH
							)
						);
						// Player did not request to equip a bound weapon, so ignore.
						if (!reqToEquip)
						{
							SPDLOG_DEBUG
							(
								"[ActorEquipManager Hooks] {}: "
								"trying to equip bound weapon {}. Ignoring.", 
								a_actor->GetName(), a_object->GetName()
							);
							return;
						}
					}
				}
				else
				{
					// Unlike for P1, fists do not automatically get unequipped 
					// for companion players, so do it here.
					UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
				}
			}

			_EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
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
			
			// Ignore if P1, not a supported race, or transform(ing/ed).
			const auto& p = glob.coopPlayers[playerIndex];
			if (p->isPlayer1 || 
				/*!p->coopActor->race || 
				!p->coopActor->race->GetPlayable() ||*/
				p->isTransforming || 
				p->isTransformed)
			{
				return _UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
			}

			SPDLOG_DEBUG("[ActorEquipManager Hooks] UnequipObject: {}: {} (0x{:X}, type: 0x{:X}).", 
				a_actor->GetName(), a_object->GetName(), a_object->formID, *a_object->formType);

			bool isBound = 
			(
				(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
				(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
			);

			// Ignore instances where the game unequips bound weapons 
			// and unequips the "fists" weapon after clearing hand slots.
			if (!isBound && a_object != glob.fists)
			{
				//===================================================
				// Cases in which the unequip call should be skipped.
				//===================================================

				auto armor = a_object->As<RE::TESObjectARMO>(); 
				if ((armor && armor->IsShield()) || a_object->As<RE::TESObjectLIGH>())
				{
					// Ignore if the game is trying to unequip the desired shield/torch.
					if (a_object == p->em->desiredEquippedForms[!EquipIndex::kLeftHand])
					{
						// Previous equip calls to equip a bound weapon to the LH slot only unequip
						// the shield/torch from the LH slot and not from its biped armor slots, 
						// so if we currently have a bound weapon equipped in the LH, 
						// do not ignore the unequip call.
						if (auto lhWeap = p->em->GetLHWeapon(); !lhWeap || !lhWeap->IsBound())
						{
							SPDLOG_DEBUG
							(
								"[ActorEquipManager Hooks] {}: "
								"trying to unequip shield/torch {}. Ignoring.", 
								a_actor->GetName(), a_object->GetName()
							);
							return;
						}
					}
				}
				else if (armor)
				{
					auto bipedSlotMask = !armor->GetSlotMask();
					int8_t indexOffset = -1;
					// Get equip index offset (from first biped slot index) 
					// by shifting biped slot index right until equal to 0.
					// Number of shifts = index offset.
					while (bipedSlotMask != 0)
					{
						bipedSlotMask = bipedSlotMask >> 1;
						indexOffset++;
					}

					// Only if biped slot is valid.
					if (indexOffset != -1)
					{
						uint8_t equipIndex = !EquipIndex::kFirstBipedSlot + indexOffset;
						// Ignore if the game is trying to unequip desired armor.
						if (armor == p->em->desiredEquippedForms[equipIndex])
						{
							SPDLOG_DEBUG
							(
								"[ActorEquipManager Hooks] {}: "
								"trying to unequip armor {}. Ignoring.", 
								a_actor->GetName(), a_object->GetName()
							);
							return;
						}
					}
				}
				else if (a_object->IsWeapon())
				{
					uint8_t indexToCheck = !EquipIndex::kRightHand;
					bool isLHEquip = 
					{
						(a_objectEquipParams.equipSlot == glob.leftHandEquipSlot) || 
						(
							a_object->As<RE::BGSEquipType>() && 
							a_object->As<RE::BGSEquipType>()->equipSlot == glob.leftHandEquipSlot
						)
					};
					if (isLHEquip)
					{
						indexToCheck = !EquipIndex::kLeftHand;
					}

					// Ignore if the game is trying to equip the desired weapon.
					if (a_object == p->em->desiredEquippedForms[indexToCheck])
					{
						SPDLOG_DEBUG
						(
							"[ActorEquipManager Hooks] {}: trying to unequip weapon {}. Ignoring.", 
							a_actor->GetName(), a_object->GetName()
						);
						return;
					}
				}
				else if (a_object->As<RE::TESShout>())
				{
					if (a_object == p->em->desiredEquippedForms[!EquipIndex::kVoice])
					{
						SPDLOG_DEBUG
						(
							"[ActorEquipManager Hooks] {}: trying to unequip shout {}. Ignoring.", 
							a_actor->GetName(), a_object->GetName()
						);
						return;
					}
				}
			}

			_UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
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

			// Set performed action anim event tag so that the player action manager
			// can handle AV modification later when it updates, which minimizes processing done
			// by this game thread.
			auto perfAVAnimEvent = 
			(
				std::pair<PerfAnimEventTag, uint16_t>(PerfAnimEventTag::kNone, 0)
			);
			switch (Hash(a_event->tag))
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
				if (p->mm->lsMoved)
				{
					p->mm->SetDontMove(false);
					p->coopActor->NotifyAnimationGraph("moveStart");
				}
			}
			
			SPDLOG_DEBUG("[AnimationGraphManager Hook] ProcessEvent: {}: {}", 
				p->coopActor->GetName(), a_event->tag);

			p->lastAnimEventTag = a_event->tag;

			SPDLOG_DEBUG
			(
				"[AnimationGraphManager Hook] ProcessEvent: {}: Getting Lock. (0x{:X})",
				p->coopActor->GetName(),
				std::hash<std::jthread::id>()(std::this_thread::get_id())
			);
			{
				std::unique_lock<std::mutex> perfAnimQueueLock(p->pam->avcam->perfAnimQueueMutex);
				SPDLOG_DEBUG
				(
					"[AnimationGraphManager Hook] ProcessEvent: {}: Lock obtained. (0x{:X})", 
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

// [BS CULLING PROCESS HOOKS]:
		void BSCullingProcessHooks::Process1
		(
			RE::BSCullingProcess* a_this, RE::NiAVObject* a_object, std::uint32_t a_arg2
		)
		{
			// NOTE: 
			// Reduces the number of freezes when using this mod with Felisky's Minimap active, 
			// but they still occur.
			// Turning on cam collisions and NOT removing occlusion 
			// seems to be the most stable option.
			if ((!ALYSLC::MiniMapCompat::g_miniMapInstalled) || 
				(!ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround) || 
				(!Settings::bRemoveExteriorOcclusion && !Settings::bRemoveInteriorOcclusion))
			{
				return _Process1(a_this, a_object, a_arg2);
			}

			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if (!p1 || !p1->parentCell)
			{
				return _Process1(a_this, a_object, a_arg2);
			}

			auto oldMode = a_this->cullMode;
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				const auto& mapMenu = ui->GetMenu<RE::MapMenu>();
				if (mapMenu && mapMenu.get())
				{
					a_this->cullMode = 3;
				}
				else
				{
					a_this->cullMode = 0;
				}
			}

			_Process1(a_this, a_object, a_arg2);
			a_this->cullMode = oldMode;
		}

// [BS MULTI BOUND HOOKS]:
		bool BSMultiBoundHooks::QWithinPoint(RE::BSMultiBound* a_this, const RE::NiPoint3& a_pos)
		{
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
						(sky && sky->mode == RE::Sky::Mode::kInterior))
				) ||
				(Settings::bRemoveExteriorOcclusion && cell->IsExteriorCell())	
			);
			if (!checkForMultiboundRefrs)
			{
				return _QWithinPoint(a_this, a_pos);
			}

			for (auto refrMB : cell->loadedData->multiboundRefMap)
			{
				// Treat as within multibound if this multibound is within the current cell
				// Prevents occlusion of refrs inside the multibound.
				if (refrMB.second && 
					refrMB.second->multiBound && 
					refrMB.second->multiBound.get() && 
					refrMB.second->multiBound.get() == a_this)
				{
					// Since there is a multibound refr in the current cell,
					// signal to apply culling workaround.
					if (ALYSLC::MiniMapCompat::g_miniMapInstalled && 
						!ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround)
					{
						ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround = true;
					}
					
					SPDLOG_DEBUG
					(
						"[BSMultiBound Hooks] Cell: {} (0x{:X}): Set as within marker {}.",
						cell->fullName,
						cell->formID,
						refrMB.second->name
					);
					return true;
				}
			}

			return _QWithinPoint(a_this, a_pos);
		}

// [CHARACTER HOOKS]:
		float CharacterHooks::CheckClampDamageModifier
		(
			RE::Character* a_this, RE::ActorValue a_av, float a_delta
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _CheckClampDamageModifier(a_this, a_av, a_delta);
			}

			auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
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
					// Check if the player is self-healing, 
					// and if so, add skill XP to their Restoration skill.
					if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
					{
						// Clamp first.
						float currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
						float currentMaxHealth = 
						(
							p->coopActor->GetBaseActorValue(RE::ActorValue::kHealth) + 
							p->coopActor->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
							)
						);
						float baseXP = std::clamp(a_delta, 0.0f, currentMaxHealth - currentHealth);
						// Prevent full-heal on combat exit for co-op companions.
						// Check if heal delta is larger than 
						// the total health regen from healing effects,
						// which would indicate that an external source of health regen
						// restored the player's health to full.
						if (currentHealth < currentMaxHealth && 
							a_delta >= currentMaxHealth - currentHealth)
						{
							bool isHealing = false;
							float healingDeltaTotal = 0.0f;
							if (a_this->GetActiveEffectList())
							{
								for (auto effect : *a_this->GetActiveEffectList())
								{
									if (!effect)
									{
										continue;
									}

									auto valueModifierEffect = 
									(
										skyrim_cast<RE::ValueModifierEffect*>(effect)
									); 

									bool isHealingEffect = 
									(
										valueModifierEffect && 
										valueModifierEffect->actorValue == 
										RE::ActorValue::kHealth &&
										valueModifierEffect->value > 0.0f
									);

									if (isHealingEffect)
									{
										isHealing = true;
										healingDeltaTotal += valueModifierEffect->value;
									}
								}
							}

							float realDelta = currentMaxHealth - currentHealth;
							// Seems as if the delta that heals the player to full 
							// is always very close to (current max health - current health) + 1.
							// Good enough for filtering out this health change.
							if (fabsf(1.0f - (a_delta - realDelta)) < 0.0001f && 
								a_delta > healingDeltaTotal)
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
									p->controllerID, RE::ActorValue::kRestoration, a_delta
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
									p->controllerID, RE::ActorValue::kRestoration, a_delta
								);
							}
						}
					}
					else if ((a_delta < 0.0f) && 
							(a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina))
					{
						// Scaled by cost multiplier here instead of in the cost functions 
						// in the player action function holder because we want 
						// a consistent solution for both types of players.
						// Drawback would be not being able to link an action
						// with a specific magicka/stamina reduction, 
						// since this function does not provide any context
						// for the source of the AV change.
						float mult = 
						(
							a_av == RE::ActorValue::kMagicka ? 
							Settings::vfMagickaCostMult[p->playerID] : 
							Settings::vfStaminaCostMult[p->playerID]
						);
						a_delta *= mult;
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
					float currentMaxHealth = 
					(
						a_this->GetBaseActorValue(RE::ActorValue::kHealth) + 
						a_this->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
						)
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
								p->controllerID, RE::ActorValue::kRestoration, healthDelta
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
								p->controllerID, RE::ActorValue::kRestoration, healthDelta
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
								p->controllerID, RE::ActorValue::kRestoration, healthDelta
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
			// NOTE: The given damage is negative.
			if (!glob.globalDataInit && !glob.coopSessionActive)
			{
				return _HandleHealthDamage(a_this, a_attacker, a_damage);;
			}

			// Check for damage dealt by a player.
			auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(a_attacker);
			// Check for damage dealt to a player.
			auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
			bool playerVictim = playerVictimIndex != -1;
			bool playerAttacker = playerAttackerIndex != -1;

			float damageMult = 1.0f;
			if (playerVictim)
			{
				// Apply damage received mult if a player was hit.
				const auto& p = glob.coopPlayers[playerVictimIndex];
				damageMult *= Settings::vfDamageReceivedMult[p->playerID];
			}

			if (playerAttacker)
			{
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
				if (p->pam->attackDamageMultSet && p->pam->reqDamageMult != 1.0f)
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
			// Ignore direct modifications of HP or no direct attacker (a_attacker == nullptr).
			// NOTE: 
			// Unfortunately, this means fall damage (no attacker) 
			// will not scale down with the player's damage received mult.
			if (float deltaHealth = a_damage * (damageMult - 1.0f); 
				deltaHealth != 0.0f && a_attacker)
			{
				SPDLOG_DEBUG
				(
					"[Character Hooks] HandleHealthDamage: {} was hit by {} for {} damage, "
					"modified to {}.",
					a_this->GetName(),
					a_attacker ? a_attacker->GetName() : "NONE",
					a_damage,
					a_damage * damageMult
				);
				a_this->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth
				);
				a_damage *= damageMult;
			}

			// Check if the player must be set as downed when at or below 0 health.
			if (playerVictim)
			{
				const auto& p = glob.coopPlayers[playerVictimIndex];
				// Ignore duplicate calls if the player is already downed.
				if (a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f && !p->isDowned)
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
							GlobalCoopData::YouDied();
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
						GlobalCoopData::YouDied(p->coopActor.get());
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
				const auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
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
							p->controllerID, RE::ActorValue::kDestruction, -a_damage
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
					SPDLOG_DEBUG
					(
						"[Character Hooks] HandleHealthDamage: "
						"{} is about to be killed by {}. Reported killer is {}.",
						a_this->GetName(),
						p->coopActor->GetName(),
						Util::HandleIsValid(a_this->myKiller) ?
						a_this->myKiller.get()->GetName() :
						"NONE"
					);

					// NOTE: 
					// Enderal treats dead actors without an associated killer
					// as killed by P1, so clear out the handle here 
					// to get XP from killing this actor.
					// Setting directly to P1 does not properly grant XP for some reason.
					if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
					{
						a_this->boolBits.set(RE::Actor::BOOL_BITS::kMurderAlarm);
						a_this->KillImpl(p->coopActor.get(), FLT_MAX, false, false);
						a_this->myKiller = p->coopActor.get();
					}
					else
					{
						a_this->boolBits.set(RE::Actor::BOOL_BITS::kMurderAlarm);
						a_this->KillImpl(p1, FLT_MAX, false, false);
						a_this->myKiller = p1;
					}

					// Have to store info to give the killer player first rights 
					// to loot the corpse with the QuickLoot menu.
					if (ALYSLC::QuickLootCompat::g_quickLootInstalled)
					{
						// Use extra data to store the real killer, 
						// since P1 takes the blame for any co-op companion kills.
						// NOTE: 
						// I am assuming this exData is not used on corpses, 
						// and that the exDataType's pad is also unused for corpses.
						// Seems to be the case from testing thusfar.

						// Pad14 is set to the killer player's FID and set to 0 
						// once the player has opened the QuickLoot menu for the corpse.
						if (!a_this->extraList.HasType<RE::ExtraForcedTarget>())
						{
							// I guess Add() manages the lifetime 
							// of the newly allocated extra data pointer?
							// No explicit free call after calling Add() 
							// in two other places within the CommonLibSSE source.
							RE::ExtraForcedTarget* exData
							{
								RE::BSExtraData::Create<RE::ExtraForcedTarget>()
							};
							exData->target = RE::ObjectRefHandle();
							exData->pad14 = p->coopActor->formID;
							a_this->extraList.Add(exData);
						}
						else
						{
							auto exData = a_this->extraList.GetByType<RE::ExtraForcedTarget>();
							exData->pad14 = p->coopActor->formID;
						}
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
				if (p->pam->isSprinting) 
				{
					a_data.deltaTime *= 
					(
						(Settings::fBaseSpeed / 85.0f) * (Settings::fSprintingMovMult / 1.5f)
					);
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
				a_data.deltaTime *= Settings::fSprintingMovMult;
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

			// Prevent the game from forcing the co-op companion player 
			// out of sneaking while in combat.
			if (hash == "SneakStop"_h)
			{
				auto package = p->coopActor->GetCurrentPackage();
				bool alwaysSneakFlagSet = false;
				if (package)
				{
					alwaysSneakFlagSet = package->packData.packFlags.all
					(
						RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak
					);
				}

				// Flag gets ignored and is not modified while in combat,
				// so it retains the value set by the player action functions holder,
				// which is used to indicate whether the player wants to sneak or not.
				// If attempting to exit the sneak state while the player wants to remain sneaking,
				// return false.
				if (alwaysSneakFlagSet)
				{
					return false;
				}
			}
			else if (Settings::bUseReviveSystem && hash == "BleedoutStart"_h)
			{
				// Skip bleedout animations when using the co-op revive system.
				// Players will ragdoll and become unresponsive when reaching 0 health instead.
				return _NotifyAnimationGraph(a_this, "BleedOutStop");
			}
			else if (((p->isDowned && !p->isRevived) ||
					 (p->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && 
					  hash == "GetUpBegin"_h)
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

		void CharacterHooks::ResetInventory(RE::Character* a_this, bool a_leveledOnly)
		{
			// Prevent players from resetting their inventory when disabled/resurrected,
			// since a full inventory reset removes items added during the co-op session.

			// Allow inventory resets if no players are initialized
			// or if this character is not a player.
			if (!glob.globalDataInit || 
				!glob.allPlayersInit || 
				!GlobalCoopData::IsCoopPlayer(a_this))
			{
				return _ResetInventory(a_this, a_leveledOnly);	
			}
		}

		void CharacterHooks::Update(RE::Character* a_this, float a_delta)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return _Update(a_this, a_delta);
			}

			if (GlobalCoopData::IsCoopPlayer(a_this))
			{
				// Let the game update the player first
				_Update(a_this, a_delta);

				const auto& p = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(a_this)];
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

				// Don't know how to prevent combat from triggering 
				// for co-op companion players towards other actors, including other players.
				// Best bandaid solution for now is to remove the combat controller each frame,
				// but combat will still initiate for a frame at most 
				// until the controller is cleared here.
				p->coopActor->combatController = nullptr;
				// Make sure the player's life state reports them as alive once no longer downed.
				bool inDownedLifeState = 
				{
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
					a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
				};
				if ((!p->isDowned) && inDownedLifeState)
				{
					a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
				}

				auto currentProc = a_this->currentProcess; 
				if (!currentProc)
				{
					return;
				}

				if (auto high = currentProc->high; high)
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
					if (p->mm->shouldCurtailMomentum && p->mm->dontMoveSet)
					{
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
							 charController && p->mm->IsRunning())
					{
						// NOTE: 
						// Base movement type data values seem to only reset 
						// to their defaults each frame 
						// if the player's speedmult is modified.
						// Otherwise, the movement speed changes each frame will accumulate, 
						// reaching infinity and preventing the player from moving.
						if (!p->coopActor->IsOnMount())
						{
							float speedMultToSet = 
							(
								p->mm->movementOffsetParams[!MoveParams::kSpeedMult]
							);
							if (p->mm->movementOffsetParams[!MoveParams::kSpeedMult] < 0.0f)
							{
								speedMultToSet = p->mm->baseSpeedMult;
							}
									
							p->coopActor->SetBaseActorValue
							(
								RE::ActorValue::kSpeedMult, 
								speedMultToSet
							);
							// Applies the new speedmult right away,
							p->coopActor->RestoreActorValue
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage, 
								RE::ActorValue::kCarryWeight,
								-0.001f
							);
							p->coopActor->RestoreActorValue
							(
								RE::ACTOR_VALUE_MODIFIER::kDamage,
								RE::ActorValue::kCarryWeight, 
								0.001f
							);
						}

						// REMOVE when done debugging.
						/*if (p->mm->isDashDodging || p->mm->isParagliding)
						{
							RE::NiPoint3 linVelXY = RE::NiPoint3
							(
								charController->outVelocity.quad.m128_f32[0], 
								charController->outVelocity.quad.m128_f32[1], 
								0.0f
							);
							RE::NiPoint3 facingDir = Util::RotationToDirectionVect
							(
								0.0f, 
								Util::ConvertAngle(p->coopActor->data.angle.z)
							);
							RE::NiPoint3 lsDir = Util::RotationToDirectionVect
							(
								0.0f,
								Util::ConvertAngle
								(
									p->mm->movementOffsetParams[!MoveParams::kLSGameAng]
								)
							);
							glm::vec3 start = ToVec3(p->coopActor->data.location);
							glm::vec3 offsetFacing = ToVec3(facingDir);
							glm::vec3 offsetLSDir = ToVec3(lsDir);
							glm::vec3 offsetVel = ToVec3(linVelXY);
							DebugAPI::QueueArrow3D
							(
								start, start + offsetFacing * 50.0f, 0xFF0000FF, 5.0f, 3.0f
							);
							DebugAPI::QueueArrow3D
							(
								start, start + offsetLSDir * 50.0f, 0x00FF00FF, 5.0f, 3.0f
							);
							DebugAPI::QueueArrow3D
							(
								start, start + offsetVel * 50.0f, 0x0000FFFF, 5.0f, 3.0f
							);
						}*/

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
								p->mm->movementOffsetParams[!MoveParams::kLSGameAng] : 
								Util::DirectionToGameAngYaw(linVelXY)
							);
							// Yaw difference between the XY velocity direction 
							// and the direction in which the player wishes to head.
							float movementToHeadingAngDiff = 
							(
								p->mm->lsMoved ? 
								Util::NormalizeAngToPi
								(
									p->mm->movementOffsetParams[!MoveParams::kLSGameAng] - 
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
								// right as the player started pivoting caused them to turn
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

				// Already performed the player update, so return.
				return;
			}
			// Not a co-op entity.
			else
			{
				bool isMountedByPlayer = false;
				RE::ActorPtr rider = nullptr;
				if (a_this->IsAMount())
				{
					a_this->GetMountedBy(rider);
					if (rider && rider.get())
					{
						isMountedByPlayer = GlobalCoopData::IsCoopPlayer(rider.get());
					}
				}

				if (isMountedByPlayer)
				{
					// Modify mount speed mult when sprinting to maintain speed consistency 
					// between P1 and other players' mounts.
					const auto& p =
					(
						glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(rider.get())]
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

					// Let the game update this character first.
					_Update(a_this, a_delta);
							
					// [TEMP WORKAROUND 1]:
					// Disable Precision on this actor when ragdolled 
					// to avoid a ragdoll reset position glitch on knock explosion
					// where the hit actor is teleported to their last ragdoll position 
					// instead of staying at their current position.
					// Precision is re-enabled on the actor after they get up.
					if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI3; api)
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
					
					// [TEMP WORKAROUND 2]:
					// Temporary solution to allies/teammates attacking co-op players,
					// even on accidental hits.
					// Stop combat right away if the current combat target is a co-op player.
					// Will maintain aggro if low on health.
					// Player must sheathe weapons to indicate peaceful intentions 
					// if below this threshold.
					float maxHealth = 
					(
						a_this->GetActorValueModifier
						(
							RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
						) + 
						a_this->GetBaseActorValue(RE::ActorValue::kHealth)
					);
					bool aboveQuarterHealth = 
					(
						a_this->GetActorValue(RE::ActorValue::kHealth) / maxHealth > 0.25f
					);
					if (aboveQuarterHealth && Util::IsPartyFriendlyActor(a_this))
					{
						a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
						if (a_this->combatController)
						{
							auto combatTarget1 = 
							(
								Util::GetRefrPtrFromHandle(a_this->currentCombatTarget)
							);
							auto combatTarget2 = 
							(
								Util::GetRefrPtrFromHandle(a_this->combatController->targetHandle)
							);
							auto playerActorTarget = 
							( 
								GlobalCoopData::IsCoopPlayer(combatTarget1.get()) ? 
								combatTarget1->As<RE::Actor>() :
								GlobalCoopData::IsCoopPlayer(combatTarget2.get()) ? 
								combatTarget2->As<RE::Actor>() : 
								nullptr
							);
							if (playerActorTarget)
							{
								if (auto procLists = RE::ProcessLists::GetSingleton(); procLists)
								{
									procLists->StopCombatAndAlarmOnActor(playerActorTarget, false);
								}
							}
						}
					}

					// Already updated.
					return;
				}
			}

			_Update(a_this, a_delta);
		}

// [MELEE HIT HOOKS]:
		void MeleeHitHooks::ProcessHit(RE::Actor* a_victim, RE::HitData& a_hitData)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _ProcessHit(a_victim, a_hitData);
			}

			const auto attacker = Util::GetActorPtrFromHandle(a_hitData.aggressor); 
			if (!attacker || !attacker.get())
			{
				return _ProcessHit(a_victim, a_hitData);;
			}

			auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_victim);
			auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(attacker.get());
			// Handle cached melee UseSkill() calls for P1 
			// (always fires before this hook does for melee skills).
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if ((p1) && 
				(glob.lastP1MeleeUseSkillCallArgs) && 
				(attacker.get() == p1 || a_victim == p1))
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
					(attacker.get() == p1) && 
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
							p->controllerID,
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
							p->controllerID, RE::ActorValue::kLightArmor, lightArmorBaseXP
						);
					}

					if (heavyArmorBaseXP > 0.0f)
					{
						GlobalCoopData::AddSkillXP
						(
							p->controllerID, RE::ActorValue::kHeavyArmor, heavyArmorBaseXP
						);
					}
				}
			}

			_ProcessHit(a_victim, a_hitData);
		}

// [MENU CONTROLS HOOKS]:
		EventResult MenuControlsHooks::ProcessEvent
		(
			RE::MenuControls* a_this, 
			RE::InputEvent* const* a_event, 
			RE::BSTEventSource<RE::InputEvent*>* a_eventSource
		)
		{
			auto ui = RE::UI::GetSingleton();
			if ((!glob.globalDataInit || !ui || !a_event) || !(*a_event))
			{
				return _ProcessEvent(a_this, a_event, a_eventSource);
			}

			// P1's managers are inactive when there is no co-op session, 
			// when the co-op camera is disabled,
			// or when the manager is paused while the game is unpaused.
			bool p1ManagersInactive =
			(
				(!glob.coopSessionActive || !glob.cam->IsRunning()) ||
				(!glob.coopPlayers[glob.player1CID]->IsRunning() && ui && !ui->GameIsPaused())
			);

			bool p1InMenu = !p1ManagersInactive && glob.menuCID == glob.player1CID;
			// Invalidate the event to prevent other handlers from processing the original event.
			bool invalidateEvent = false;
			if (p1InMenu)
			{
				// Check to see if P1 is in the Favorites Menu and is trying to hotkey an entry 
				// or is trying equip a quickslot item or spell.
				invalidateEvent = CheckForP1HotkeyReq(a_event) || CheckForP1QSEquipReq(a_event);
			}
			else if (p1ManagersInactive && Util::MenusOnlyAlwaysOpen())
			{
				// Delay Pause and Wait binds to trigger their actions on release, 
				// rather than on press, when P1's managers are inactive.
				// In addition, check for pressing of the default binds:
				// Pause + Wait -> Co-op Debug Menu
				// Wait + Pause -> Co-op Summoning Menu binds.
				invalidateEvent = CheckForMenuTriggeringInput(a_event);
			}

			if (invalidateEvent) 
			{
				auto inputEvent = *a_event;
				auto idEvent = inputEvent->AsIDEvent();
				auto buttonEvent = inputEvent->AsButtonEvent();
				auto thumbstickEvent = 
				(
					inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ? 
					static_cast<RE::ThumbstickEvent*>(inputEvent) : 
					nullptr
				);
				if (inputEvent && inputEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad)
				{
					while (inputEvent)
					{
						// Get chained event's event sub-types.
						idEvent = inputEvent->AsIDEvent();
						buttonEvent = inputEvent->AsButtonEvent();
						thumbstickEvent =
						(
							inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ?
							static_cast<RE::ThumbstickEvent*>(inputEvent) :
							nullptr
						);

						if (buttonEvent)
						{
							// Reset device connect flag (used to block an analog stick event) 
							// which seems to carry over once set.
							inputEvent->eventType.reset(RE::INPUT_EVENT_TYPE::kDeviceConnect);
							idEvent->userEvent = "BLOCKED";
							buttonEvent->idCode = 0xFF;
							buttonEvent->heldDownSecs = 0.0f;
							buttonEvent->value = 0.0f;
						}
						else
						{
							idEvent->userEvent = "BLOCKED";
							// JANK ALERT:
							// Must also set this event type flag
							// to stop analog stick events from being processed by action handlers.
							inputEvent->eventType.set(RE::INPUT_EVENT_TYPE::kDeviceConnect);
						}

						inputEvent = inputEvent->next;
					}
				}

				// Should still process the modified, invalidated event.
				return _ProcessEvent(a_this, a_event, a_eventSource);
			}
			else
			{
				// Filter out P1 controller inputs that should be ignored 
				// by this menu event handler while in co-op.
				bool shouldProcessHere = FilterInputEvents(a_event);
				if (shouldProcessHere) 
				{
					return _ProcessEvent(a_this, a_event, a_eventSource);
						
				}
				else
				{
					return EventResult::kContinue;
				}
			}

			return _ProcessEvent(a_this, a_event, a_eventSource);
		}

		bool MenuControlsHooks::CheckForMenuTriggeringInput(RE::InputEvent* const* a_constEvent)
		{
			// Check if P1 is trying to open the Summoning/Debug menus.
			// Return true if the event triggered a co-op menu and should be invalidated.

			bool invalidateEvent = false;
			auto eventHead = *a_constEvent;
			auto buttonEvent = eventHead->AsButtonEvent(); 
			if (buttonEvent)
			{
				// Only need to handle gamepad button presses.
				if (eventHead->GetDevice() == RE::INPUT_DEVICE::kGamepad)
				{
					auto userEvents = RE::UserEvents::GetSingleton();
					auto controlMap = RE::ControlMap::GetSingleton();
					// Debug: always Pause + Wait when not in co-op.
					// Summon: always Wait + Pause when not in co-op.
					auto pauseMask = GAMEPAD_MASK_START;
					auto waitMask = GAMEPAD_MASK_BACK;
					if (userEvents && controlMap)
					{
						pauseMask = controlMap->GetMappedKey
						(
							userEvents->pause, RE::INPUT_DEVICE::kGamepad
						);
						waitMask = controlMap->GetMappedKey
						(
							userEvents->wait, RE::INPUT_DEVICE::kGamepad
						);
					}

					// Sometimes the associated user event is 'Journal' instead of 'Pause'.
					if (pauseMask == 0xFF) 
					{
						pauseMask = controlMap->GetMappedKey
						(
							userEvents->journal, RE::INPUT_DEVICE::kGamepad
						);
					}

					// Ensure both masks are valid, despite failing to get mapped button ID code.
					if (pauseMask == 0xFF) 
					{
						pauseMask = GAMEPAD_MASK_START;
					}

					if (waitMask == 0xFF) 
					{
						waitMask = GAMEPAD_MASK_BACK;
					}

					// Both binds are sometimes the same here, 
					// so ensure they aren't by falling back to the default binds.
					if (pauseMask == waitMask) 
					{
						pauseMask = GAMEPAD_MASK_START;
						waitMask = GAMEPAD_MASK_BACK;
					}

					// Button events seem to be chained in a manner 
					// that does not depend on when their buttons were pressed.
					// Check for both Pause and Wait binds being pressed/held 
					// in any order to trigger co-op debug/summoning menus.
					bool pauseBindEvent = buttonEvent->idCode == pauseMask;
					bool waitBindEvent = buttonEvent->idCode == waitMask;
					if (pauseBindEvent || waitBindEvent)
					{
						// Pause/Wait bind pressed by itself.
						if (!eventHead->next)
						{
							pauseBindPressedFirst = pauseBindEvent;
						}
						else if (auto buttonEvent2 = eventHead->next->AsButtonEvent();
								 buttonEvent2)
						{
							bool pauseBindEvent2 = buttonEvent2->idCode == pauseMask;
							bool waitBindEvent2 = buttonEvent2->idCode == waitMask;
							bool debugMenuBindPressedAndReleased = 
							(
								(pauseBindPressedFirst) && 
								(
									(pauseBindEvent && waitBindEvent2) || 
									(pauseBindEvent2 && waitBindEvent)
								) &&
								(buttonEvent->IsUp() || buttonEvent2->IsUp())
							);
							bool summoningMenuBindPressedAndReleased = 
							(
								(!pauseBindPressedFirst) && 
								(
									(waitBindEvent && pauseBindEvent2) || 
									(waitBindEvent2 && pauseBindEvent)
								) &&
								(buttonEvent->IsUp() || buttonEvent2->IsUp())
							);

							// Check if either menu is triggerable,
							// but do not attempt to open either just yet.
							bool shouldTriggerDebugMenu = 
							(
								debugMenuBindPressedAndReleased && !debugMenuTriggered
							);
							
							bool shouldTriggerSummoningMenu = 
							(
								summoningMenuBindPressedAndReleased && !summoningMenuTriggered
							);
							
							// BEFORE sending events to open any menus.
							// Temp solution (not failproof), 
							// since I can't find a direct way of getting the XInput
							// controller index for the controller Skyrim recognizes as P1's.
							// NOTE: 
							// The BSPCGamepadDeviceDelegate's 'userIndex' member seems to 
							// always equal 0, even if the XInput-reported controller index 
							// for P1 is not 0, so we can't use that member to set P1's CID.
							// 
							// Check to see which controller is requesting to open the menu 
							// and assign its ID as P1's CID.
							// Heuristic checks the two buttons' event-reported held times 
							// against the XInput controller state held times.
							// Will sometimes fail if two players press the same binds 
							// at nearly the exact same time (within a couple frames),
							// as the wrong player's CID may be assigned as P1's CID.
							// Fix by manually assigning the P1 CID through the Debug Menu.
							auto devMgr = RE::BSInputDeviceManager::GetSingleton();
							auto gamepad = devMgr ? devMgr->GetGamepad() : nullptr;
							bool checkForP1CID = 
							(
								(
									(glob.cdh) && 
									(
										glob.player1CID == -1 ||
										!gamepad ||
										gamepad->userIndex == -1
									)
								) && 
								(shouldTriggerDebugMenu || shouldTriggerSummoningMenu)
							);
							SPDLOG_DEBUG
							(
								"[MenuControls Hook] CheckForMenuTriggeringInput: "
								"Checking for P1 CID: {}, {}. "
								"Reported gamepad user index: {}.",
								shouldTriggerDebugMenu, shouldTriggerSummoningMenu,
								gamepad ? gamepad->userIndex : -1337
							);
							if (checkForP1CID)
							{
								int32_t newCID = -1;
								// Choose the controller with the smallest held time difference.
								float smallestHeldTimeDiffTotal = FLT_MAX;
								for (uint32_t i = 0; i < ALYSLC_MAX_PLAYER_COUNT; ++i)
								{
									XINPUT_STATE inputState;
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

									float eventReportedPauseHoldTime = 
									(
										pauseBindEvent ? 
										buttonEvent->heldDownSecs : 
										buttonEvent2->heldDownSecs
									);
									float eventReportedWaitHoldTime = 
									(
										waitBindEvent ?
										buttonEvent->heldDownSecs : 
										buttonEvent2->heldDownSecs
									);
									const auto& inputState1 = 
									(
										glob.cdh->GetInputState
										(
											i, 
											shouldTriggerDebugMenu ?
											glob.cdh->GAMEMASK_TO_INPUT_ACTION.at(pauseMask) :
											glob.cdh->GAMEMASK_TO_INPUT_ACTION.at(waitMask)
										)
									);
									const auto& inputState2 = 
									(
										glob.cdh->GetInputState
										(
											i, 
											shouldTriggerDebugMenu ? 
											glob.cdh->GAMEMASK_TO_INPUT_ACTION.at(waitMask) : 
											glob.cdh->GAMEMASK_TO_INPUT_ACTION.at(pauseMask)
										)
									);

									// Skip if neither of the two requisite binds 
									// are pressed on this controller.
									if (!inputState1.isPressed && !inputState2.isPressed)
									{
										SPDLOG_DEBUG
										(
											"[MenuControls Hook] CheckForMenuTriggeringInput: "
											"Skipping held time check for CID {}. "
											"Event reported held times: Pause: {}, Wait: {}. "
											"Input state held times: Pause: {}, Wait: {}. "
											"Is pressed: Pause: {}, Wait: {}.",
											i,
											eventReportedPauseHoldTime,
											eventReportedWaitHoldTime,
											shouldTriggerDebugMenu ? 
											inputState1.heldTimeSecs :
											inputState2.heldTimeSecs,
											shouldTriggerDebugMenu ?
											inputState2.heldTimeSecs : 
											inputState1.heldTimeSecs,
											shouldTriggerDebugMenu ?
											inputState1.isPressed : 
											inputState2.isPressed,
											shouldTriggerDebugMenu ?
											inputState2.isPressed : 
											inputState1.isPressed
										);
										continue;
									}
									else
									{
										SPDLOG_DEBUG
										(
											"[MenuControls Hook] CheckForMenuTriggeringInput: "
											"Performing held time check for CID {}. "
											"Event reported held times: Pause: {}, Wait: {}. "
											"Input state reported held times: "
											"Pause: {}, Wait: {}.",
											i,
											eventReportedPauseHoldTime,
											eventReportedWaitHoldTime,
											shouldTriggerDebugMenu ? 
											inputState1.heldTimeSecs : 
											inputState2.heldTimeSecs,
											shouldTriggerDebugMenu ?
											inputState2.heldTimeSecs : 
											inputState1.heldTimeSecs
										);
									}

									float heldTimeDiffTotal = FLT_MAX;
									if (shouldTriggerDebugMenu)
									{
										heldTimeDiffTotal = 
										(
											fabsf
											(
												eventReportedPauseHoldTime -
												inputState1.heldTimeSecs
											) + 
											fabsf
											(
												eventReportedWaitHoldTime -
												inputState2.heldTimeSecs
											)
										);
									}
									else
									{
										heldTimeDiffTotal = 
										(
											fabsf
											(
												eventReportedWaitHoldTime - 
												inputState1.heldTimeSecs
											) + 
											fabsf
											(
												eventReportedPauseHoldTime - 
												inputState2.heldTimeSecs
											)
										);
									}

									SPDLOG_DEBUG
									(
										"[MenuControls Hook] CheckForMenuTriggeringInput: "
										"Total held time differential for CID {} is {}.",
										i, heldTimeDiffTotal
									);

									if (heldTimeDiffTotal < smallestHeldTimeDiffTotal)
									{
										smallestHeldTimeDiffTotal = heldTimeDiffTotal;
										newCID = i;
									}
								}

								if (newCID != -1)
								{
									SPDLOG_DEBUG
									(
										"[MenuControls Hook] CheckForMenuTriggeringInput: "
										"P1 CID set to {};",
										newCID
									);
									glob.player1CID = newCID;
								}
							}

							// After performing P1 CID check,
							// we can now open either menu.
							if (shouldTriggerDebugMenu)
							{
								// Can trigger the debug menu at any time, 
								// even if the camera/P1 manager threads are inactive.
								if (glob.player1Actor)
								{
									SPDLOG_DEBUG
									(
										"[MenuControls Hook] CheckForMenuTriggeringInput: "
										"Debug menu binds pressed but not triggered. "
										"Opening menu now."
									);
									glob.onDebugMenuRequest.SendEvent
									(
										glob.player1Actor.get(), 
										glob.coopSessionActive ? glob.player1CID : -1
									);
									invalidateEvent = true;
								}

								debugMenuTriggered = true;
							}
							
							if (shouldTriggerSummoningMenu)
							{
								// NOTE: 
								// Have to wait until P1 is out of combat to summon other players.
								// Summoning global variable is set to 1 
								// in the summoning menu script, and set to 0 
								// if summoning failed or is complete.
								if (glob.summoningMenuOpenGlob->value == 0.0f && 
									glob.player1Actor && 
									!glob.player1Actor->IsInCombat())
								{
									SPDLOG_DEBUG
									(
										"[MenuControls Hook] CheckForMenuTriggeringInput: "
										"Summoning menu binds pressed but not triggered. "
										"Opening menu now."
									);
									glob.onSummoningMenuRequest.SendEvent();
									invalidateEvent = true;
								}

								summoningMenuTriggered = true;
							}

							// After processing, ignore the second button event entirely, 
							// since we do not want either the pause or wait menus to trigger once
							// both the pause and wait binds are pressed at the same time.
							if (buttonEvent2->IsDown() ||
								buttonEvent2->IsHeld() || 
								buttonEvent2->IsUp())
							{
								buttonEvent2->heldDownSecs = 0.0f;
								buttonEvent2->value = 0.0f;
								buttonEvent2->idCode = 0xFF;
							}
						}

						if (buttonEvent->IsDown() || buttonEvent->IsHeld())
						{
							if (buttonEvent->IsDown())
							{
								ignoringPauseWaitEvent = !Util::MenusOnlyAlwaysOpen();
								SPDLOG_DEBUG
								(
									"[MenuControls Hook] CheckForMenuTriggeringInput: "
									"{} menu bind is now pressed. "
									"Ignore ({}) and trigger on release "
									"if no co-op menus are triggered by then.",
									pauseBindEvent ? "Pause" : "Wait",
									ignoringPauseWaitEvent
								);
							}

							// Clear out first button event to prevent it 
							// from triggering the Pause or Wait menus
							// while the button is still pressed.
							buttonEvent->heldDownSecs = 0.0f;
							buttonEvent->value = 0.0f;
							buttonEvent->idCode = 0xFF;
						}
						else if (buttonEvent->IsUp())
						{
							if (!summoningMenuTriggered && 
								!debugMenuTriggered && 
								!ignoringPauseWaitEvent && 
								Util::MenusOnlyAlwaysOpen())
							{
								// No co-op menus triggered, 
								// so allow the button event to pass through on release
								// and trigger either the Pause or Wait menu as usual.
								SPDLOG_DEBUG
								(
									"[MenuControls Hook] CheckForMenuTriggeringInput: "
									"{} bind released on its own. "
									"Simulating press to trigger {} menu.",
									pauseBindEvent ? "Pause" : "Wait", 
									pauseBindEvent ? "Pause" : "Wait"
								);

								buttonEvent->heldDownSecs = 0.0f;
								buttonEvent->value = 1.0f;
							}
							else if (summoningMenuTriggered || debugMenuTriggered)
							{
								// A co-op menu was triggered, 
								// so ignore the button event on release.
								SPDLOG_DEBUG
								(
									"[MenuControls Hook] CheckForMenuTriggeringInput: "
									"{} bind released on its own after {} menu triggered. "
									"Ignoring.",
									pauseBindEvent ? "Pause" : "Wait", 
									summoningMenuTriggered ? "Summoning" : "Debug"
								);

								buttonEvent->value = 0.0f;
								buttonEvent->idCode = 0xFF;
							}
						}
					}
				}
			}

			// No button event or at least two button inputs released, 
			// so reset menu triggered states.
			bool buttonInputsReleased = 
			{
				(eventHead && !buttonEvent && !eventHead->next) || 
				(
					(buttonEvent && buttonEvent->IsUp()) && 
					(
						(!buttonEvent->next) || 
						(
							buttonEvent->next->AsButtonEvent() && 
							buttonEvent->next->AsButtonEvent()->IsUp()
						)
					)
				)
			};
			bool needToResetState = 
			(
				summoningMenuTriggered || debugMenuTriggered || ignoringPauseWaitEvent
			);
			if (buttonInputsReleased && needToResetState)
			{
				SPDLOG_DEBUG
				(
					"[MenuControls Hook] CheckForMenuTriggeringInput: Buttons released. "
					"Reset flags to false. Pause bind pressed first: {}, "
					"summoning menu triggered: {}, debug menu triggered: {}, "
					"ignoring Pause/Wait event: {}",
					pauseBindPressedFirst, 
					summoningMenuTriggered, 
					debugMenuTriggered, 
					ignoringPauseWaitEvent
				);

				pauseBindPressedFirst =
				summoningMenuTriggered = 
				debugMenuTriggered = 
				ignoringPauseWaitEvent = false;
			}

			return invalidateEvent;
		}

		bool MenuControlsHooks::CheckForP1HotkeyReq(RE::InputEvent* const* a_constEvent)
		{
			// Check if P1 is trying to hotkey a FavoritesMenu entry.
			// Return true if the event triggered a hotkey change and should be invalidated.

			auto inputEvent = *a_constEvent;
			auto ue = RE::UserEvents::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			// Must have a valid input event, access to the UI and user events singletons,
			// and be displaying the Favorites Menu.
			if (!inputEvent || 
				!ue || 
				!ui || 
				!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || 
				!controlMap)
			{
				return false;
			}

			auto idEvent = inputEvent->AsIDEvent();
			auto buttonEvent = inputEvent->AsButtonEvent();
			// Only handle button events with an ID.
			if (!idEvent || !buttonEvent)
			{
				return false;
			}

			// To hotkey an entry,
			// P1 must be clicking in the RS and it must be displaced from center.
			bool isRThumbPressedAndRSMoved = 
			{
				buttonEvent->idCode == GAMEPAD_MASK_RIGHT_THUMB &&
				glob.cdh->GetAnalogStickState(glob.player1CID, false).normMag > 0.0f
			};

			// Only handle on initial press.
			if (!isRThumbPressedAndRSMoved ||
				buttonEvent->value != 1.0f || 
				buttonEvent->heldDownSecs != 0.0f)
			{
				return false;
			}
			
			glob.mim->HotkeyFavoritedForm();
			return true;
		}

		bool MenuControlsHooks::CheckForP1QSEquipReq(RE::InputEvent* const* a_constEvent)
		{
			// Check if P1 is trying to (un)tag a FavoritesMenu entry 
			// as a quick slot item/spell.
			// Return true if the event triggered a QS (un)equip and should be invalidated.

			auto inputEvent = *a_constEvent;
			auto ue = RE::UserEvents::GetSingleton();
			auto ui = RE::UI::GetSingleton();
			auto controlMap = RE::ControlMap::GetSingleton();
			// Must have a valid input event, access to the UI and user events singletons,
			// and be displaying the Favorites Menu.
			if (!inputEvent || 
				!ue || 
				!ui || 
				!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) || 
				!controlMap)
			{
				return false;
			}

			auto idEvent = inputEvent->AsIDEvent();
			auto buttonEvent = inputEvent->AsButtonEvent();
			// Only handle button events with an ID.
			if (!idEvent || !buttonEvent)
			{
				return false;
			}

			// Only handle pause/journal bind presses.
			bool isPauseBind = 
			{
				(
					buttonEvent->idCode == 
					controlMap->GetMappedKey(ue->pause, RE::INPUT_DEVICE::kGamepad)
				) ||
				(
					buttonEvent->idCode == 
					controlMap->GetMappedKey(ue->journal, RE::INPUT_DEVICE::kGamepad)
				)
			};

			// Only handle on initial press.
			if (!isPauseBind || buttonEvent->value != 1.0f || buttonEvent->heldDownSecs != 0.0f)
			{
				return false;
			}
			
			glob.mim->EquipP1QSForm();
			return true;
		}

		bool MenuControlsHooks::FilterInputEvents(RE::InputEvent* const* a_constEvent)
		{
			// Check which player sent the input events in the input event chain,
			// and modify individual input events in the chain to block them 
			// from being handled by P1's action handlers, as needed.
			// Return true if the event should be processed by the MenuControls hook.
			// 
			// NOTE: 
			// This function is messy, I know.
			// 
			// IMPORTANT:
			// InputEvent's 'pad24' member is used to store processing info:
			// 0xC0DAXXXX:	event was already filtered and handled here.
			// 0xXXXXC0DA:	proxied P1 input sent by this plugin 
			// and should be allowed through by this function.
			// 0xXXXXCA11:	emulated P1 input sent by another player from the MIM.
			// 0xXXXXDEAD:	ignore this input event.
			
			// Should this menu controls handler handle the input event here 
			// or pass it on without processing?
			bool shouldProcess = true;

			const auto ui = RE::UI::GetSingleton();
			const auto ue = RE::UserEvents::GetSingleton();
			auto p1 = RE::PlayerCharacter::GetSingleton();
			auto inputEvent = *a_constEvent;
			auto idEvent = inputEvent->AsIDEvent();
			auto buttonEvent = inputEvent->AsButtonEvent();
			auto thumbstickEvent = 
			(
				inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ? 
				static_cast<RE::ThumbstickEvent*>(inputEvent) : 
				nullptr
			);

			// Non-P1 player controlling menus.
			bool coopPlayerControllingMenus = 
			(
				glob.globalDataInit && glob.mim->managerMenuCID != -1
			);
			// P1's co-op managers are active.
			bool p1ManagersActive = 
			(
				(glob.coopSessionActive) && 
				(glob.cam->IsRunning() || glob.coopPlayers[glob.player1CID]->IsRunning())
			);

			// Initial check to prevent quick or console command-triggered saving
			// while co-op companion data is copied to P1.
			// Also recommended that the player disable auto-saving 
			// while using this mod to remove any chance of saving copied data onto P1, 
			// such as another player's name or race name.
			const auto eventName = inputEvent->QUserEvent();
			if ((*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) &&
				(eventName == ue->quicksave ||
				eventName == ue->console))
			{
				if (idEvent && buttonEvent)
				{
					if (buttonEvent->IsDown())
					{
						if (eventName == ue->quicksave)
						{
							RE::DebugMessageBox
							(
								"[ALYSLC] Cannot save while another player's data "
								"is copied over to P1.\n"
								"Please ensure all menus are closed or reload an older save."
							);
						}
						else
						{
							RE::DebugMessageBox
							(
								"[ALYSLC] Cannot open the console while another player's data "
								"is copied over to P1.\n"
								"Please ensure all menus are closed or reload an older save."
							);
						}
					}

					idEvent->userEvent = "BLOCKED";
					buttonEvent->idCode = 0xFF;
					buttonEvent->heldDownSecs = 0.0f;
					buttonEvent->value = 0.0f;
					return false;
				}
			}

			if (!ui ||
				!ue ||
				!p1 || 
				!inputEvent ||
				inputEvent->GetDevice() != RE::INPUT_DEVICE::kGamepad)
			{
				// Reset device connect flag that may have been set before 
				// when preventing analog stick inputs from propagating unmodified.
				inputEvent->eventType.reset(RE::INPUT_EVENT_TYPE::kDeviceConnect);
				return shouldProcess;
			}

			bool dialogueMenuOpen = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
			bool lootMenuOpen = ui->IsMenuOpen(GlobalCoopData::LOOT_MENU);
			// Open the targeted container while in the LootMenu.
			bool lootMenuOpenContainer = false;
			bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
			while (inputEvent)
			{
				// Get chained event sub-types.
				idEvent = inputEvent->AsIDEvent();
				buttonEvent = inputEvent->AsButtonEvent();
				thumbstickEvent = 
				(
					inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick ? 
					static_cast<RE::ThumbstickEvent*>(inputEvent) : 
					nullptr
				);

				// Reset blocked analog stick event flag which seems to carry over once set.
				inputEvent->eventType.reset(RE::INPUT_EVENT_TYPE::kDeviceConnect);
				if (idEvent)
				{
					// Clear pad24 first because the pad flag sticks around for reused id events.
					// NOTE: 
					// Top 2 bytes == 0xC0DA means this event has been processed before.
					if (idEvent->pad24 >> 16 == 0xC0DA)
					{
						idEvent->pad24 = 0;
					}

					// Has a bypass flag indicating that the event was sent by another player.
					// Co-op companion players: 0xCA11
					bool coopPlayerMenuInput = 
					(
						(coopPlayerControllingMenus && idEvent) && 
						((idEvent->pad24 & 0xFFFF) == 0xCA11)
					);

					//======================================
					// Special QuickLoot menu compatibility.
					//======================================
					bool validLootMenuInput = lootMenuOpen;
					auto controlMap = RE::ControlMap::GetSingleton(); 
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
							if (!lootMenuOpenContainer && 
								buttonEvent->idCode == 
								controlMap->GetMappedKey
								(
									ue->readyWeapon, RE::INPUT_DEVICE::kGamepad
								))
							{
								lootMenuOpenContainer = true;
							}
							else if ((coopPlayerControllingMenus && !coopPlayerMenuInput) &&
									(
										buttonEvent->idCode ==
										controlMap->GetMappedKey
										(
											ue->accept, 
											RE::INPUT_DEVICE::kGamepad, 
											RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
										) ||
										buttonEvent->idCode == 
										controlMap->GetMappedKey
										(
											ue->cancel, 
											RE::INPUT_DEVICE::kGamepad, 
											RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
										) ||
										buttonEvent->idCode == 
										controlMap->GetMappedKey
										(
											ue->xButton, 
											RE::INPUT_DEVICE::kGamepad,
											RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu
										)
									))
							{
								// Questionable formatting as usual.
								// Ignore P1 inputs that would affect the Loot Menu 
								// (eg. Take, Take All, or Exit).
								validLootMenuInput = false;
							}
							else if ((!coopPlayerControllingMenus) && 
									(
										buttonEvent->idCode == 
										controlMap->GetMappedKey
										(
											ue->cancel,
											RE::INPUT_DEVICE::kGamepad,
											RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode
										)
									))
							{
								// Have to stop the Tween Menu from opening for P1 here, 
								// so swallow the 'Cancel' input, 
								// and instead close the Loot Menu by clearing the crosshair refr.
								// Keeps things consistent with the other players,
								// who also close the LootMenu via clearing the crosshair refr.
								validLootMenuInput = false;
								// Exit menu and relinquish control when cancel bind is pressed.
								auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); 
								if (crosshairPickData)
								{
									// Clears crosshair refr data.
									Util::SendCrosshairEvent(nullptr);
								}
							}
						}
					}
					else if (buttonEvent && !coopPlayerMenuInput)
					{
						if (coopPlayerControllingMenus) 
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
						else
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
						coopPlayerControllingMenus && 
						!coopPlayerMenuInput && 
						Hash(idEvent->userEvent) == ""_h
					);
					// Attacking on foot.
					bool isGroundedAttackInput = !p1->IsOnMount() && isAttackInput;
					// Attempting to move the camera.
					bool isLookInput = idEvent->userEvent == ue->look;
					// Attempting to move the player's arms.
					bool isMoveArmsInput = 
					(
						Settings::bRotateArmsWhenSheathed && 
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

					// Can P1 switch the container tab to/from their inventory?
					bool isValidContainerTabSwitch = false;
					if (idEvent->userEvent == ue->wait &&
						ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) 
					{
						auto containerMenu = ui->GetMenu<RE::ContainerMenu>(); 
						if (containerMenu && containerMenu.get())
						{
							RE::NiPointer<RE::TESObjectREFR> containerRefr;
							RE::TESObjectREFR::LookupByHandle
							(
								RE::ContainerMenu::GetTargetRefHandle(), containerRefr
							);
							// If the container is not a companion player's inventory,
							// or if P1 is attempting to switch back 
							// to the companion player's inventory,
							// the tab switch request is valid.
							if (!GlobalCoopData::IsCoopPlayer(containerRefr)) 
							{
								isValidContainerTabSwitch = true;
							}
							else if (auto view = containerMenu->uiMovie; view)
							{
								RE::GFxValue result;
								view->Invoke
								(
									"_root.Menu_mc.isViewingContainer",
									std::addressof(result),
									nullptr,
									0
								);
								bool isViewingContainer = result.GetBool();
								// Only allow a tab switch from P1's inventory 
								// back to the co-op companion's inventory.
								if (!isViewingContainer)
								{
									isValidContainerTabSwitch = true;
								}
							}
						}
					}

					//=============================================================================
					// Should Block or Propagate Events:
					//=============================================================================

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
								(isBlockedP1LootMenuEvent) ||
								(
									(onlyAlwaysOpen) && 
									(
										isGroundedAttackInput || 
										isMoveArmsInput || 
										isMountedCamInputEvent
									)
								) ||
								(idEvent->userEvent == ue->wait && !isValidContainerTabSwitch) ||
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
									idEvent->userEvent == ue->tweenMenu
								)
							)
						);
					}
					else
					{
						if (coopPlayerControllingMenus)
						{
							// Prevent the usual binds from activating objects, 
							// opening menus, changing the camera POV, and equipping items
							// while another player is controlling menus.
							isBlockedP1Event = 
							(
								isBlockedP1LootMenuEvent ||
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
						else
						{
							// Prevent the camera from changing POV at all other times.
							isBlockedP1Event = idEvent->userEvent == ue->togglePOV;
						}
					}

					bool isLeftStickInput = idEvent->userEvent == ue->leftStick;
					// Special P1 input cases where the co-op camera is not active.
					if (!p1ManagersActive && !coopPlayerMenuInput && idEvent && !isBlockedP1Event)
					{
						// Set bypass flag here to allow P1's action handlers
						// to process the event when the co-op camera is not active.
						if (buttonEvent)
						{
							buttonEvent->pad24 = 0xC0DA;
						}

						// Change 'Left Stick' and 'Rotate' (menu) P1 user events 
						// to 'Move' and 'Look' user events (gameplay context)
						// because P1 is not in control of menus 
						// and should not affect another player's menu control.
						if (coopPlayerControllingMenus)
						{
							if (isLeftStickInput)
							{
								idEvent->userEvent = ue->move;
							}
							else if (isRotateInput)
							{
								idEvent->userEvent = ue->look;
							}
						}
					}

					// NOTE: LS and RS inputs are blocked in the Crafting and Dialogue menus.
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
						ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
						ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
						ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) 
					);
					// Ignore this input if pad24 == 0xDEAD.
					bool ignoreInput = (idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xDEAD);
					// P1 input event to rotate the lock while another player rotates the pick.
					bool twoPlayerLockpickingP1Input = 
					(
						Settings::bTwoPlayerLockpicking && 
						coopPlayerControllingMenus &&
						!coopPlayerMenuInput && 
						idEvent->userEvent == "RotateLock"sv
					);
					// P1 input that should be processed by MenuControls.
					bool processableP1Event = 
					(
						!coopPlayerControllingMenus || twoPlayerLockpickingP1Input
					);
					// Has a bypass flag indicating that the event 
					// was sent/allowed through by this plugin.
					// P1: 0xC0DA
					bool proxiedP1Input = (idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xC0DA);
					// Input event was blocked before, so do not propagate or handle.
					bool wasBlocked = idEvent->userEvent == "BLOCKED" && idEvent->idCode == 0xFF;

					//=============================================================================
					// Final Determinations for Propagation:
					//=============================================================================
 
					// [FOR CO-OP COMPANIONS]
					// 1. Input event sent by a non-P1 player.
					// 2. P1 not in control of menus.
					// 3. Not an analog stick input while in the Dialogue/Crafting Menu.
					// 4. Does not move P1 or the camera: 
					// Either:
					// a. Full screen menu is open 
					// (all players immobile and camera rotation prohibited, so allow through).
					// -OR-
					// b. Is not a move/look input 
					// (does not move P1 or rotate the default FP/TP camera).
					bool validCoopCompanionInput = 
					( 
						(
							coopPlayerMenuInput && 
							coopPlayerControllingMenus && 
							!blockedAnalogStickInput
						) && 
						((fullscreenMenuOpen) || (!isMoveInput && !isLookInput)) 
					);
						
					// [FOR P1]
					// 1. Proxied through with bypass flag.
					// 2. Rotate lock input while another player is attempting to lockpick
					// and two player lockpicking is enabled.
					// 3.
					//	a. Another player is not controlling menus.
					//	-OR-
					//	b. No fullscreen menu is open, and the input event is from P1.
					// 
					// -AND-
					// 
					//	a. Not an analog stick input while in the Dialogue/Crafting Menu.
					//	-AND-
					//	b. Not an explicitly blocked P1 input event.
					bool validP1Input = 
					{ 
						(proxiedP1Input || twoPlayerLockpickingP1Input) ||
						(
							(!isBlockedP1Event && !blockedAnalogStickInput) && 
							(
								(!coopPlayerControllingMenus) || 
								(!coopPlayerMenuInput && !fullscreenMenuOpen)
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
					shouldProcess = 
					(
						(!wasBlocked && !ignoreInput) && 
						(coopPlayerMenuInput || processableP1Event)
					);

					// Propagate the event to P1's action handlers if it shouldn't be ignored 
					// and if it is a valid P1 or co-op player input event.
					bool propagateUnmodifiedEvent = 
					(
						(!wasBlocked && !ignoreInput) && (validP1Input || validCoopCompanionInput)
					);

					// REMOVE when done debugging.
					/*SPDLOG_DEBUG
					(
						"[MenuControls Hook] FilterInputEvents: Menu, MIM CID: {}, {}, "
						"EVENT: {} (0x{:X}, type {}), blocked: {}, co-op player in menus: {}, "
						"p1 manager threads active: {} => PROPAGATE: {}, HANDLE: {}, "
						"proxied P1 input: {}, coop player menu input: {}, "
						"dialogue menu open: {}, is blocked event: {}, "
						"valid co-op companion input: {}, valid p1 input: {}, "
						"two-player P1 lockpicking input: {}",
						glob.menuCID,
						glob.mim->managerMenuCID,
						idEvent->userEvent,
						buttonEvent ? buttonEvent->idCode : 0xFF,
						*inputEvent->eventType,
						wasBlocked,
						coopPlayerControllingMenus,
						p1ManagersActive,
						propagateUnmodifiedEvent,
						shouldProcess,
						proxiedP1Input,
						coopPlayerMenuInput,
						dialogueMenuOpen,
						isBlockedP1Event,
						validCoopCompanionInput,
						validP1Input,
						twoPlayerLockpickingP1Input);*/

					if (!propagateUnmodifiedEvent)
					{
						if (buttonEvent)
						{
							idEvent->userEvent = "BLOCKED";
							buttonEvent->idCode = 0xFF;
							buttonEvent->heldDownSecs = 0.0f;
							buttonEvent->value = 0.0f;
						}
						else
						{
							idEvent->userEvent = "BLOCKED";
							// Must also set this event type flag 
							// to stop analog stick events being processed by action handlers.
							inputEvent->eventType.set(RE::INPUT_EVENT_TYPE::kDeviceConnect);
						}
					}

					// Set as handled.
					idEvent->pad24 = 0xC0DA0000 + (idEvent->pad24 & 0xFFFF);
				}

				inputEvent = inputEvent->next;
			}

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
					coopPlayerControllingMenus ? 
					glob.coopPlayers[glob.mim->managerMenuCID] : 
					glob.coopPlayers[glob.player1CID]
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

			return shouldProcess;
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

			const auto& p = glob.coopPlayers[index];
			// The co-op camera is not enabled, so do not restore P1's node rotations.
			if (p->isPlayer1 && !glob.cam->IsRunning())
			{
				return _UpdateDownwardPass(a_this, a_data, a_arg2);
			}

			{
				std::unique_lock<std::mutex> lock
				(
					p->mm->nom->rotationDataMutex, std::try_to_lock
				);
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
			}

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
		float PlayerCharacterHooks::CheckClampDamageModifier
		(
			RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_delta
		)
		{
			if (!glob.globalDataInit || !glob.coopSessionActive)
			{
				return _CheckClampDamageModifier(a_this, a_av, a_delta);
			}

			const auto& p = glob.coopPlayers[glob.player1CID];
			bool hmsActorValue = 
			(
				a_av == RE::ActorValue::kHealth ||
				a_av == RE::ActorValue::kMagicka || 
				a_av == RE::ActorValue::kStamina
			);
			// Do not modify AVs when no players are being dismissed and
			// the co-op actor is not revived or if an HMS AV is being decreased in god mode.
			bool notDismissingPlayers = 
			(
				!Settings::bUseReviveSystem || glob.livingPlayers == glob.activePlayers
			);
			if ((notDismissingPlayers) && 
				(!p->isRevived || (hmsActorValue && p->isInGodMode && a_delta < 0.0f)))
			{
				// Prevent arcane fever buildup while in god mode.
				bool arcaneFeverActorValue = 
				(
					ALYSLC::EnderalCompat::g_enderalSSEInstalled &&
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
				// Check if another player is healing P1, and if so, give them XP.
				if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
				{
					for (const auto& otherP : glob.coopPlayers)
					{
						if (otherP->isPlayer1 || 
							!otherP->isActive || 
							a_this->GetHandle() != otherP->tm->GetRangedTargetActor())
						{
							continue;
						}

						float currentHealth = a_this->GetActorValue(RE::ActorValue::kHealth);
						float currentMaxHealth = 
						(
							a_this->GetBaseActorValue(RE::ActorValue::kHealth) + 
							a_this->GetActorValueModifier
							(
								RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
							)
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
									otherP->controllerID, 
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
									otherP->controllerID, 
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
									otherP->controllerID, 
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
					if (currentSpeedMult != p->mm->movementOffsetParams[!MoveParams::kSpeedMult])
					{
						return 
						(
							p->mm->movementOffsetParams[!MoveParams::kSpeedMult] - currentSpeedMult
						);
					}
				}
				else if ((a_delta < 0.0f) && 
						 (a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina))
				{
					// Scaled by cost multiplier here instead of in the cost functions 
					// in the player action function holder
					// because those cost functions do not affect P1.
					// Drawback would be not being able to link an action 
					// with a specific magicka/stamina reduction,
					// since this function does not provide any context 
					// about the source of the stamina or magicka change.
					float mult = 
					(
						a_av == RE::ActorValue::kMagicka ? 
						Settings::vfMagickaCostMult[p->playerID] : 
						Settings::vfStaminaCostMult[p->playerID]
					);
					a_delta *= mult;
				}
			}

			return _CheckClampDamageModifier(a_this, a_av, a_delta);
		}

		void PlayerCharacterHooks::DrawWeaponMagicHands(RE::PlayerCharacter* a_this, bool a_draw)
		{
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				glob.player1CID < 0 || 
				glob.player1CID >= ALYSLC_MAX_PLAYER_COUNT)
			{
				return _DrawWeaponMagicHands(a_this, a_draw);
			}

			const auto& p = glob.coopPlayers[glob.player1CID];
			// Do not allow the game to automatically sheathe/unsheathe 
			// the player actor's weapons/magic.
			// Blocking weapon/magic drawing while transforming crashes the game at times.
			if (!p->IsRunning() ||
				a_draw == p->pam->weapMagReadied || 
				//p->isTransformed || 
				p->isTransforming)
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
			float damageMult = Settings::vfDamageReceivedMult[0];
			// Co-op player inflicted health damage on P1.
			if (playerAttackerIndex != -1)
			{
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

				// Modify damage mult.
				if (!p->isPlayer1 && p->pam->attackDamageMultSet && p->pam->reqDamageMult != 1.0f)
				{
					damageMult *= 
					(
						p->pam->reqDamageMult * Settings::vfDamageDealtMult[p->playerID]
					);
					// Reset damage multiplier if performing a ranged sneak attack.
					// Melee sneak attacks reset the damage multiplier on attack stop,
					// but I have yet to find a way to check 
					// if the player no longer has any active projectiles, 
					// so reset the damage multiplier on damaging hit.
					p->pam->ResetAttackDamageMult();
				}
				else
				{
					// Regular attack.
					// P1 can attack themselves and this multiplier will apply as well.
					damageMult *= Settings::vfDamageDealtMult[p->playerID];
				}

				SPDLOG_DEBUG
				(
					"[PlayerCharacter Hooks] HandleHealthDamage: "
					"{} attacked for a base damage of {} and mult {}.",
					a_attacker ? a_attacker->GetName() : "NONE", a_damage, damageMult
				);

				// Add skill XP if P1 is not the attacker and P1 is not in god mode.
				bool p1HitWhileInGodMode = glob.coopPlayers[glob.player1CID]->isInGodMode;
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
							p->controllerID, RE::ActorValue::kDestruction, -a_damage
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
			// Ignore direct modifications of HP or if there is no direct attacker:
			// (a_attacker == nullptr).
			// NOTE: 
			// Unfortunately, this means fall damage (no attacker) 
			// will not scale down with the player's damage received mult.
			float deltaHealth = a_damage * (damageMult - 1.0f); 
			if (deltaHealth != 0.0f && a_attacker)
			{
				a_this->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth
				);
				a_damage *= damageMult;
			}

			const auto& p = glob.coopPlayers[glob.player1CID];
			// Check if the player must be set as downed when at or below 0 health.
			// Ignore duplicate calls if the player is already downed.
			if (a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f && !p->isDowned)
			{
				// Set downed state for P1.
				if (Settings::bUseReviveSystem && Settings::bCanRevivePlayer1)
				{
					// Set P1 as downed.
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
						GlobalCoopData::YouDied();
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
					GlobalCoopData::YouDied(a_this);
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
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				if (coopP1->pam->isSprinting)
				{
					a_data.deltaTime *= 
					(
						(Settings::fBaseSpeed / 85.0f) * (Settings::fSprintingMovMult / 1.5f)
					);
				}
			}

			const auto& p = glob.coopPlayers[glob.player1CID];
			if (p->mm->isDashDodging)
			{
				// Dash dodge animation speedup depends on LS displacement and equipped weight.
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
				a_data.deltaTime *= Settings::fSprintingMovMult;
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

			const auto& p = glob.coopPlayers[glob.player1CID];
			auto hash = Hash(a_eventName);
			if (glob.coopSessionActive)
			{
				if (Settings::bUseReviveSystem && hash == "BleedoutStart"_h)
				{
					// Skip bleedout animations when using the co-op revive system.
					// Players will ragdoll and become unresponsive when reaching 0 health instead.
					return _NotifyAnimationGraph(a_this, "BleedOutStop");
				}
				else if (((p->isDowned && !p->isRevived) || 
						 (p->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && 
						 hash == "GetUpBegin"_h)
				{
					// Ignore requests to get up when the player is downed and not revived.
					return false;
				}
				else if ((p->coopActor->IsInKillMove()) && 
						 (hash == "PairEnd"_h || hash == "pairedStop"_h))
				{
					// Sometimes, when a killmove fails, the player will remain locked in place
					// because the game still considers them to be in a killmove,
					// so unset the flag here to signal the player's PAM to stop handling
					// the previously triggered killmove and reset the player's data.
					p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
				}
				else if (!p->isTransformed && hash == "BiteStart"_h)
				{
					// Can't recall what this was for, but... oh well.
					return true;
				}
			}
			// Failsafe to ensure that P1 does not get up when dead after co-op ends.
			else if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
			{
				// Ensure P1 is dead if all other players are dead after the co-op session ends.
				// Ignore requests to get up while downed.
				if (glob.livingPlayers == 0 && hash == "GetUpBegin"_h)
				{
					// First, make sure the essential flag is unset.
					Util::NativeFunctions::SetActorBaseDataFlag
					(
						p1->GetActorBase(), RE::ACTOR_BASE_DATA::Flag::kEssential, false
					);

					p1->KillImpl(p1, FLT_MAX, false, false);
					p1->KillImmediate();
					p1->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);

					// Kill calls fail on P1 at times, especially when the player dies in water, 
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
						delete script;
					}

					return false;
				}
			}

			return _NotifyAnimationGraph(a_this, a_eventName);
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
			_Update(a_this, a_delta);
				
			const auto& p = glob.coopPlayers[glob.player1CID];
			//===================
			// Node Orientations.
			//===================
			// NOTE: All downward passes for the player's nodes have been performed at this point,
			// so restore all saved default local transforms for the next frame.
			// Reasoning: Sometimes, such as when a havok impulse is applied to the player,
			// the game won't restore the animation-derived local transforms 
			// for all the player's nodes, since the havok impulse 
			// applied its own overriding local transform to the node(s).
			// Thus, any of our local transform modifications from the last frame 
			// will carry over and stack with this frame's,
			// which leads to setting incorrect local transforms (lots of spinning) 
			// unless the defaults are restored first.
			p->mm->nom->RestoreOriginalNodeLocalTransforms(p);

			//===========================
			// Movement and Player State.
			//===========================
			
			// Prevent game from updating crosshair text while co-op cam is active.
			a_this->playerFlags.shouldUpdateCrosshair = false;

			// Make sure player is set to alive if not downed.
			bool inDownedLifeState = 
			(
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
				a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
			);
			if ((!p->isDowned) && inDownedLifeState)
			{
				a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
			}

			// Need a valid current process to continue.
			auto currentProc = a_this->currentProcess; 
			if (!currentProc)
			{
				return;
			}

			if (auto high = currentProc->high; high)
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

				/*SPDLOG_DEBUG
				(
					"[PlayerCharacter Hooks] Update: {}: BEFORE: "
					"MT data: unkF8: {}, unkFC: {}, unk100: {}, unk104: {}, unk108: {}, "
					"unk10C: {}, unk110: {}, unk114: {}, unk118: {}, unk11C: {}, unk120: {}. "
					"Curtail momentum: {}, don't move set: {}. Movement speed: {}.",
					p->coopActor->GetName(),
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRotations]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRotations]
					[RE::Movement::MaxSpeeds::kRun],
					rotateWhileMovingRun,
					p->mm->shouldCurtailMomentum,
					p->mm->dontMoveSet,
					p->coopActor->DoGetMovementSpeed()
				);
				*/
				
				// Set movement speed to an obscenely high value to quickly
				// arrest built up momentum while also keeping the player in place
				// with the 'don't move' flag.
				if (p->mm->shouldCurtailMomentum && p->mm->dontMoveSet)
				{
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
						 charController && p->mm->IsRunning())
				{
					// NOTE: 
					// Base movement type data values seem to only reset 
					// to their defaults each frame 
					// if the player's speedmult is modified.
					// Otherwise, the movement speed changes each frame will accumulate, 
					// reaching infinity and preventing the player from moving.
					float speedMultToSet = p->mm->movementOffsetParams[!MoveParams::kSpeedMult];
					if (p->mm->movementOffsetParams[!MoveParams::kSpeedMult] < 0.0f)
					{
						speedMultToSet = p->mm->baseSpeedMult;
					}

					p->coopActor->SetBaseActorValue
					(
						RE::ActorValue::kSpeedMult, 
						speedMultToSet
					);
					// Applies the new speedmult right away,
					p->coopActor->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f
					);
					p->coopActor->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f
					);

					// REMOVE when done debugging.
					/*
					if (p->mm->isDashDodging || p->mm->isParagliding) 
					{
						RE::NiPoint3 linVelXY = RE::NiPoint3
						(
							charController->outVelocity.quad.m128_f32[0], 
							charController->outVelocity.quad.m128_f32[1], 
							0.0f
						);
						RE::NiPoint3 facingDir = Util::RotationToDirectionVect
						(
							0.0f, Util::ConvertAngle(p->coopActor->data.angle.z)
						);
						RE::NiPoint3 lsDir = Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle
							(
								p->mm->movementOffsetParams[!MoveParams::kLSGameAng]
							)
						);
						glm::vec3 start = ToVec3(p->coopActor->data.location);
						glm::vec3 offsetFacing = ToVec3(facingDir);
						glm::vec3 offsetLSDir = ToVec3(lsDir);
						glm::vec3 offsetVel = ToVec3(linVelXY);
						DebugAPI::QueueArrow3D
						(
							start, start + offsetFacing * 50.0f, 0xFF0000FF, 5.0f, 3.0f
						);
						DebugAPI::QueueArrow3D
						(
							start, start + offsetLSDir * 50.0f, 0x00FF00FF, 5.0f, 3.0f
						);
						DebugAPI::QueueArrow3D
						(
							start, start + offsetVel * 50.0f, 0x0000FFFF, 5.0f, 3.0f
						);
					}
					*/

					//================
					// Rotation speed.
					//================
					if (p->mm->isDashDodging)
					{
						// No rotation when dodging.
						speeds
						[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kWalk]					=
						speeds[RE::Movement::SPEED_DIRECTIONS::kRotations]
						[RE::Movement::MaxSpeeds::kRun]						=
						rotateWhileMovingRun								= 0.0f;
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
					else if (bool isAIDriven = p->coopActor->movementController && 
							 !p->coopActor->movementController->unk1C5; isAIDriven)
					{
						RE::NiPoint3 linVelXY = RE::NiPoint3();
						float movementToHeadingAngDiff = -1.0f;
						float range = -1.0f;
						float diffFactor = -1.0f;
						// Player must not be sprinting, mounted, downed, animation driven, 
						// or running their interaction package.
						if (!p->pam->IsPerforming(InputAction::kSprint) && 
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
							// right as the player started pivoting caused them to turn
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
								p->mm->movementOffsetParams[!MoveParams::kLSGameAng] : 
								Util::DirectionToGameAngYaw(linVelXY)
							);
							// Yaw difference between the XY velocity direction 
							// and the direction in which the player wishes to head.
							movementToHeadingAngDiff = 
							(
								p->mm->lsMoved ? 
								Util::NormalizeAngToPi
								(
									p->mm->movementOffsetParams[!MoveParams::kLSGameAng] - 
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

				// REMOVE when done debugging.
				/*SPDLOG_DEBUG
				(
					"[PlayerCharacter Hooks] Update: {}: AFTER: "
					"MT data: unkF8: {}, unkFC: {}, unk100: {}, unk104: {}, unk108: {}, "
					"unk10C: {}, unk110: {}, unk114: {}, unk118: {}, unk11C: {}, unk120: {}. "
					"Curtail momentum: {}, don't move set: {}. Movement speed: {}.",
					p->coopActor->GetName(),
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kLeft]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRight]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kForward]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kBack]
					[RE::Movement::MaxSpeeds::kRun],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRotations]
					[RE::Movement::MaxSpeeds::kWalk],
					speeds
					[RE::Movement::SPEED_DIRECTIONS::kRotations]
					[RE::Movement::MaxSpeeds::kRun],
					rotateWhileMovingRun,
					p->mm->shouldCurtailMomentum,
					p->mm->dontMoveSet,
					p->coopActor->DoGetMovementSpeed()
				);*/

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
		}

		void PlayerCharacterHooks::UseSkill
		(
			RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_points, RE::TESForm* a_arg3
		)
		{
			// NOTE: For melee-related skills, 
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
					SPDLOG_DEBUG("[PlayerCharacter Hooks] UseSkill: Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));

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
						if (glob.menuCID != -1)
						{
							mult = 
							(
								Settings::vfSkillXPMult[glob.coopPlayers[glob.menuCID]->playerID]
							);
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
						const auto& p = glob.coopPlayers[glob.player1CID];
						// Use P1's skill XP mult for non-shared skills.
						a_points *= Settings::vfSkillXPMult[p->playerID];
					}
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[PlayerCharacter Hooks] UseSkill: "
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
			if (!a_this || !a_this->GetHandle())
			{
				return;
			}

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

			// Ensure the projectile's handle is valid first.
			const auto projectileHandle = a_this->GetHandle();
			auto projectilePtr = Util::GetRefrPtrFromHandle(projectileHandle);
			if (!projectilePtr)
			{
				return;
			}

			bool justReleased = a_this->livingTime == 0.0f;
			int32_t firingPlayerIndex = -1;
			bool firedAtPlayer = false;
			GetFiredAtOrByPlayer(projectileHandle, firingPlayerIndex, firedAtPlayer);
			// Restore our linear velocity if fired by the player.
			if (firingPlayerIndex != -1)
			{
				const auto& p = glob.coopPlayers[firingPlayerIndex];
				// Have to insert as managed on release if this hook was run 
				// before the UpdateImpl() hook.
				if (justReleased && !p->tm->mph->IsManaged(a_this->GetHandle()))
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
		}

		void ProjectileHooks::UpdateImpl(RE::Projectile* a_this, float a_delta)
		{
			// Not handled outside of co-op.
			if (!a_this || !a_this->GetHandle())
			{
				return;
			}

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
				return;
			}

			// Extend the projectile's lifetime via smart ptr while we modify its trajectory.
			const auto projectileHandle = a_this->GetHandle();
			auto projectilePtr = Util::GetRefrPtrFromHandle(projectileHandle);
			if (!projectilePtr)
			{
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

			// Adjust trajectory if fired by the player and just released (will be set as managed),
			// or if the projectile is still managed by the firing player's projectile handler.
			bool shouldAdjustTrajectory = 
			{
				(firingPlayerIndex != -1) &&
				(
					justReleased || 
					glob.coopPlayers[firingPlayerIndex]->tm->mph->IsManaged(projectileHandle)
				)
			};

			RE::Projectile* projectile = nullptr;
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			if (!projectile)
			{
				return;
			}

			if (shouldAdjustTrajectory)
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
			else
			{
				// Restore saved XY velocity.
				a_this->linearVelocity.x = savedVel.x;
				a_this->linearVelocity.y = savedVel.y;
			}
		}

		void ProjectileHooks::DirectProjectileAtTarget
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const RE::ObjectRefHandle& a_projectileHandle,
			RE::NiPoint3& a_resultingVelocityOut,
			const bool& a_justReleased
		)
		{
			if (!glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return;
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
				return;
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

				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				bool crosshairRefrValidity = 
				(
					crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
				);
				// Actor targeted (aim correction or otherwise), 
				// should face crosshair position (never true while mounted), 
				// or mounted and targeting an object.
				bool adjustTowardsTarget = 
				{
					(targetActorPtr != a_p->coopActor) &&
					(
						(targetActorValidity || a_p->mm->shouldFaceTarget) || 
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

				// REMOVE when done debugging.
				/*if (auto current3D = Util::GetRefr3D(projectile); current3D) 
				{
					SPDLOG_DEBUG
					(
						"[Projectile Hooks] {}: DirectProjectileAtTarget: {} (0x{:X}), "
						"node {}, type {}. Pitch, yaw: {}, {}.",
						a_p->coopActor->GetName(),
						projectile->GetName(),
						projectile->formID,
						current3D->name,
						projectile->GetProjectileBase() ? 
						(int32_t)(*projectile->GetProjectileBase()->data.types) : 
						-1,
						projectile->data.angle.x * TO_DEGREES,
						projectile->data.angle.z * TO_DEGREES
					);

					glm::vec3 start = ToVec3(current3D->world.translate);
					glm::vec3 offsetX = ToVec3
					(
						current3D->world.rotate * RE::NiPoint3(1.0f, 0.0f, 0.f)
					);
					glm::vec3 offsetY = ToVec3
					(
						current3D->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.f)
					);
					glm::vec3 offsetZ = ToVec3
					(
						current3D->world.rotate * RE::NiPoint3(0.0f, 0.0f, 1.f)
					);
					DebugAPI::QueuePoint3D
					(
						start, Settings::vuOverlayRGBAValues[a_p->playerID], 5.0f
					);
					DebugAPI::QueueArrow3D(start, start + offsetX * 15.0f, 0xFF0000FF, 3.0f, 2.0f);
					DebugAPI::QueueArrow3D(start, start + offsetY * 15.0f, 0x00FF00FF, 3.0f, 2.0f);
					DebugAPI::QueueArrow3D(start, start + offsetZ * 15.0f, 0x0000FFFF, 3.0f, 2.0f);
				}*/
			}
		}

		void ProjectileHooks::GetFiredAtOrByPlayer
		(
			const RE::ObjectRefHandle& a_projectileHandle, 
			int32_t& a_firingPlayerCIDOut,
			bool& a_firedAtPlayerOut
		)
		{
			// Store player index (CID) of the player 
			// that released this projectile in one out param.
			// -1 if not released by a player.
			// Also store whether or not the projectile 
			// was fired at a player in the other out param.

			// Default to not fired by a player or at a player.
			a_firingPlayerCIDOut = -1;
			a_firedAtPlayerOut = false;

			RE::Projectile* projectile = nullptr;
			auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
			if (projectilePtr)
			{
				projectile = projectilePtr->As<RE::Projectile>();
			}

			// Return early if the projectile is not valid.
			if (!projectile || !projectile->actorCause || !projectile->actorCause.get())
			{
				return;
			}

			auto firingActorHandle = 
			(
				projectile->actorCause && projectile->actorCause.get() ? 
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
					a_firingPlayerCIDOut = p->controllerID;
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
				if (a_firingPlayerCIDOut != -1 && a_firedAtPlayerOut)
				{
					break;
				}
			}
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
			auto targetActorPtr = Util::GetActorPtrFromHandle(managedProjInfo->targetActorHandle);
			bool targetActorValidity = 
			(
				targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get())
			);
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool crosshairRefrIsTarget = 
			{
				crosshairRefrPtr && 
				crosshairRefrPtr == targetActorPtr && 
				Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
			};
			if (targetActorValidity)
			{
				// TODO: Snap projectile to closest position 
				// on the targeted actor's character controller collider.
				// Get targeted node, if available.
				// Using aim correction and a targeted actor node was selected.
				//if (Settings::vbUseAimCorrection[a_p->playerID] && 
				//	  managedProjInfo->targetedActorNode)
				//{
				//	aimTargetPos = managedProjInfo->targetedActorNode->world.translate;
				//  auto hkpRigidBody = Util::GethkpRigidBody
				//	(
				//		managedProjInfo->targetedActorNode.get()
				//	); 
				//	if (hkpRigidBody)
				//	{
				//		// Direct at node's center of mass.
				//		// NOTE: 
				//		// Projectile is not guaranteed to collide with this node, 
				//		// unless it is within the actor's character collider.
				//		aimTargetPos = 
				//		(
				//			ToNiPoint3
				//			(
				//				hkpRigidBody->motion.motionState.sweptTransform.centerOfMass0
				//			) * HAVOK_TO_GAME
				//		);
				//	}
				//}
				//else 
				
				// Choose the exact crosshair position locally offset from the target actor;
				// otherwise, if not facing the crosshair, target the selected actor's torso.
				// Done to maximize hit chance, since an actor's torso node is most likely 
				// to be within their character controller collider.
				if (crosshairRefrIsTarget && a_p->mm->shouldFaceTarget)
				{
					// Targeted with crosshair.
					// Direct at crosshair position offset from the target actor.
					aimTargetPos = 
					(
						targetActorPtr->data.location + a_p->tm->crosshairLastHitLocalPos
					);
				}
				else
				{
					// Aim correction target. Aim at the target's torso position.
					aimTargetPos = Util::GetTorsoPosition(targetActorPtr.get());
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
			const float& initialTimeToTargetSecs = managedProjInfo->initialTrajTimeToTarget;

			// Immediately direct at the target position
			// if the initial time to hit the target is less than one frame.
			// Won't have multiple frames to direct it at the target,
			// so do it here right after release.
			if (initialTimeToTargetSecs <= *g_deltaTimeRealTime) 
			{
				// Set refr data angles.
				projectile->data.angle.x = pitchToTarget;
				projectile->data.angle.z = yawToTarget;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				auto current3D = Util::GetRefr3D(projectile); 
				if (current3D && current3D.get())
				{
					Util::SetRotationMatrixPY
					(
						current3D->local.rotate, 
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}

				// Set velocity, keeping the original speed.
				auto velToSet = 
				(
					Util::RotationToDirectionVect
					(
						-pitchToTarget, 
						Util::ConvertAngle(yawToTarget)
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
				if (auto current3D = Util::GetRefr3D(projectile); current3D && current3D.get())
				{
					Util::SetRotationMatrixPY
					(
						current3D->local.rotate,
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

			// Release speed for fixed trajectory determined by projectile launch data.
			const float& releaseSpeed = managedProjInfo->releaseSpeed;
			// Cap in-flight time.
			const bool tooLongToReach = 
			(
				initialTimeToTargetSecs >= Settings::fMaxProjAirborneSecsToTarget
			);

			// Fixed trajectory XY position and pitch along the trajectory (tangent line).
			const float xy = releaseSpeed * t * cosf(launchPitch);
			const float pitchOnTraj = 
			(
				-atan2f
				(
					tanf(launchPitch) - (g * xy) / powf(releaseSpeed * cosf(launchPitch), 2.0f), 
					1.0f
				)
			);
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

				// Set projectile pitch to trajectory pitch when not homing.
				pitchToSet = pitchOnTraj;
				// Can't hit target with the given launch pitch.
				if (initialTimeToTargetSecs == 0.0 || tooLongToReach)
				{
					// Set yaw to directly face the target right away.
					yawToSet = yawToTarget;
				}
				else
				{
					// Lerp yaw to slowly rotate the projectile to face the target in the XY plane.
					// Fully face the target in the XY plane once half 
					// of the projectile's time to the initial target position has elapsed.
					yawToSet = Util::NormalizeAng0To2Pi
					(
						currentYaw + 
						Util::InterpolateSmootherStep
						(
							0.0f, 
							0.75f, 
							min(1.0f, projectile->livingTime / (0.5f * initialTimeToTargetSecs))
						) * yawDiff
					);
				}

				// Next, check if the homing projectile should fully start homing in on the target
				// instead of following its initial fixed trajectory.
				// Check if the projectile is projected to reach its apex 
				// between now and the next frame.
				const float nextXY = releaseSpeed * (t + *g_deltaTimeRealTime) * cosf(launchPitch);
				const float nextPitchOnTraj = 
				(
					-atan2f
					(
						tanf(launchPitch) - 
						(g * nextXY) / powf(releaseSpeed * cosf(launchPitch), 2.0f), 
						1.0f
					)
				);
				// Pitch is zero at the apex.
				bool atApex = pitchOnTraj == 0.0f;
				// Different sign: angled up to angled down next frame.
				bool willHitApexBeforeNextFrame = nextPitchOnTraj >= 0.0f && pitchOnTraj <= 0.0f;
				bool reachingApex = atApex || willHitApexBeforeNextFrame;
				// Target has moved above the projectile at a steeper pitch 
				// than the projectile's current pitch.
				bool moveUpwardOffTraj = (pitchOnTraj < 0.0f && pitchToTarget < pitchOnTraj);
				// Target has moved below the projectile at a steeper pitch 
				// than the projectile's current pitch.
				bool moveDownwardOffTraj = (pitchOnTraj > 0.0f && pitchToTarget > pitchOnTraj);
				// Last point at which the projectile can stay on its fixed trajectory 
				// before homing in.
				bool passedHalfTimeOfFlight = 
				(
					projectile->livingTime > 0.5f * managedProjInfo->initialTrajTimeToTarget
				);
				// If the gravitational constant was modified and is arbitrarily close to 0, 
				// also start homing in.
				bool noGrav = g < 1e-5f;

				// Set as homing if not already set 
				// and one of the above conditions is true.
				bool shouldSetAsHoming = 
				(
					(!managedProjInfo->startedHomingIn) && 
					(
						passedHalfTimeOfFlight || 
						reachingApex || 
						moveUpwardOffTraj ||
						moveDownwardOffTraj || 
						noGrav || 
						tooLongToReach
					)
				);
				if (shouldSetAsHoming)
				{
					managedProjInfo->startedHomingIn = true;
					managedProjInfo->startedHomingTP = SteadyClock::now();
				}
			}

			// NOTE: 
			// Might uncomment eventually if air resistance is desired.
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

			// Get velocity along fixed trajectory and speed.
			// Without air resistance.
			// XY, X, Y, and Z components of velocity.
			const float velXY = releaseSpeed * cosf(launchPitch);
			const float velX = velXY * cosf(launchYaw);
			const float velY = velXY * sinf(launchYaw);
			const float velZ = releaseSpeed * sinf(launchPitch) - g * t;
			auto fixedTrajVel = RE::NiPoint3(velX, velY, velZ);
			float speed = fixedTrajVel.Length();

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
			// diverge by >= 90 degrees and the distance to the target 
			// is less than the max distance travelable per frame.
			bool wentPastTarget = 
			(
				angBetweenVelAndToTarget >= PI / 2.0f && distToTarget <= maxDistPerFrame
			);

			// Remaining portion of the rotation smoothing interval.
			float remainingRotSmoothingLifetimeRatio = 1.0f;
			// Projectile is now homing in, smooth out pitch and yaw to follow the target.
			if (managedProjInfo->startedHomingIn)
			{
				float secsSinceStartedHoming = Util::GetElapsedSeconds
				(
					managedProjInfo->startedHomingTP.value(), true
				);
				// Can't hit target with given launch pitch, 
				// so set pitch directly to target right away.
				if (initialTimeToTargetSecs == 0.0 || 
					tooLongToReach ||
					!managedProjInfo->startedHomingTP.has_value())
				{
					pitchToSet = pitchToTarget;
					yawToSet = yawToTarget;
					remainingRotSmoothingLifetimeRatio = 0.0f;

					// Turn directly to face the target once homing starts, 
					// so the target is no longer behind the projectile.
					// If the projectile eventually goes past the target again, 
					// it won't turn around to face it anymore.
					if (secsSinceStartedHoming == 0.0f)
					{
						wentPastTarget = false;
					}
				}
				else
				{
					// Full pitch smoothing lifetime is at most half 
					// of the projectile's fixed trajectory lifetime.
					float smoothingLifetimeSecs = max(0.0f, 0.5f * initialTimeToTargetSecs);
					remainingRotSmoothingLifetimeRatio = 
					(
						smoothingLifetimeSecs == 0.0f ? 
						0.0f : 
						1.0f - min(1.0f, secsSinceStartedHoming / smoothingLifetimeSecs)
					);

					// Gradually rotate towards target.
					pitchToSet = Util::NormalizeAngToPi
					(
						currentPitch + 
						Util::InterpolateEaseIn
						(
							0.0f, 
							0.75f, 
							1.0f - remainingRotSmoothingLifetimeRatio,
							3.0f
						) * pitchDiff
					);
					yawToSet = Util::NormalizeAng0To2Pi
					(
						currentYaw + 
						Util::InterpolateEaseIn
						(
							0.0f, 
							0.75f, 
							1.0f - remainingRotSmoothingLifetimeRatio,
							3.0f
						) * yawDiff
					);
				}

				// Projectile base speed when launched.
				if (auto baseSpeed = projectile->GetSpeed(); baseSpeed > 0.0f)
				{
					// Modify the current speed by the distance-slowdown factor, 
					// but set a lower bound to avoid instances where the projectile
					// does 0 damage when hitting the target at too low of a speed.
					speed = max(speed * distSlowdownFactor, min(baseSpeed, 1000.0f));
				}
				else
				{
					// No base speed, but set lower bound anyway.
					speed = max(speed * distSlowdownFactor, 1000.0f);
				}

				// Continue homing in only if the original flight time has not been reached 
				// or if the projectile has not gone past the target.
				continueSettingTrajectory = 
				(
					remainingRotSmoothingLifetimeRatio > 0.0f || !wentPastTarget
				);
			}

			if (continueSettingTrajectory)
			{
				// Set refr data angles.
				projectile->data.angle.x = pitchToSet;
				projectile->data.angle.z = yawToSet;
				// Set rotation matrix to maintain consistency 
				// with the previously set refr data angles.
				if (auto current3D = Util::GetRefr3D(projectile); current3D && current3D.get())
				{
					Util::SetRotationMatrixPY
					(
						current3D->local.rotate, 
						projectile->data.angle.x, 
						projectile->data.angle.z
					);
				}

				// Set velocity.
				if (managedProjInfo->startedHomingIn) 
				{
					auto currentDir = a_resultingVelocityOut;
					currentDir.Unitize();
					auto interpFactor = Util::InterpolateSmootherStep
					(
						0.0f, 1.0f, 1.0f - remainingRotSmoothingLifetimeRatio
					);
					// Adjust the velocity's direction to slowly converge 
					// to the direction towards the target position.
					velToSet.x = currentDir.x + (dirToTarget.x - currentDir.x) * interpFactor;
					velToSet.y = currentDir.y + (dirToTarget.y - currentDir.y) * interpFactor;
					velToSet.z = currentDir.z + (dirToTarget.z - currentDir.z) * interpFactor;

					velToSet.Unitize();
					velToSet *= speed;
				}
				else
				{
					// Set pitch and yaw to have the projectile 
					// angled parallel to its pre-homing fixed trajectory.
					velToSet = 
					(
						Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) * 
						speed
					);
				}

				a_resultingVelocityOut = velToSet;
				projectile->linearVelocity = a_resultingVelocityOut;

				// REMOVE when done debugging.
				/*SPDLOG_DEBUG
				(
					"[Projectile Hook] SetHomingTrajectory: {} (0x{:X}) is now homing: {}, "
					"speed to set: {}, distance to target: {}, initial time to target: {}, "
					"remaining rotation smoothing lifetime ratio: {}, moved: {}, "
					"pitch/yaw to target: {}, {}, current pitch/yaw: {}, {}, yaw diff: {}, "
					"pitch/yaw to set homing: {}, {}, resulting velocity: ({}, {}, {})",
					projectile->GetName(), 
					projectile->formID,
					managedProjInfo->startedHomingIn,
					speed,
					projectile->data.location.GetDistance(aimTargetPos),
					initialTimeToTargetSecs,
					remainingRotSmoothingLifetimeRatio,
					projectile->distanceMoved,
					pitchToTarget * TO_DEGREES,
					yawToTarget * TO_DEGREES,
					currentPitch * TO_DEGREES,
					currentYaw * TO_DEGREES,
					yawDiff * TO_DEGREES,
					pitchToSet * TO_DEGREES,
					yawToSet * TO_DEGREES,
					a_resultingVelocityOut.x, 
					a_resultingVelocityOut.y, 
					a_resultingVelocityOut.z
				);*/
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
				if (auto current3D = Util::GetRefr3D(projectile); current3D && current3D.get())
				{
					Util::SetRotationMatrixPY
					(
						current3D->local.rotate, 
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
				if (auto current3D = Util::GetRefr3D(projectile); current3D && current3D.get())
				{
					Util::SetRotationMatrixPY
					(
						current3D->local.rotate, projectile->data.angle.x, projectile->data.angle.z
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
			if (auto current3D = Util::GetRefr3D(projectile))
			{
				Util::SetRotationMatrixPY
				(
					current3D->local.rotate, projectile->data.angle.x, projectile->data.angle.z
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
			if (!projectile)
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
				if (a_p->mm->shouldFaceTarget) 
				{
					aimTargetPos = 
					(
						targetActorPtr->data.location + a_p->tm->crosshairLastHitLocalPos
					);
				}
				else
				{
					aimTargetPos = Util::GetTorsoPosition(targetActorPtr.get());
				}

				projectile->desiredTarget = targetActorHandle;
			}
			else if (a_p->mm->shouldFaceTarget)
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
						-a_p->mm->aimPitch, Util::ConvertAngle(projectile->data.angle.z)
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
			if (auto current3D = Util::GetRefr3D(projectile); current3D && current3D.get())
			{
				Util::SetRotationMatrixPY
				(
					current3D->local.rotate, projectile->data.angle.x, projectile->data.angle.z
				);
				auto parent = RE::NiPointer<RE::NiAVObject>(current3D->parent); 
				if (parent && parent.get()) 
				{
					current3D->world = parent->world * current3D->local;
				}
				else
				{
					current3D->world = current3D->local;
				}
			}

			// Have the game pick a target for the projectile
			// after directing it at the target.
			if (projectile->livingTime == 0.0f) 
			{
				projectile->RunTargetPick();
			}
		}

// [TESCAMERA HOOKS]:
		void TESCameraHooks::Update(RE::TESCamera* a_this)
		{
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if (!glob.globalDataInit || !glob.cam->IsRunning() || !p1)
			{
				return _Update(a_this);
			}

			const auto& coopP1 = glob.coopPlayers[glob.player1CID];
			// Camera local position/rotation is modified when ragdolled 
			// (bleedout camera position), inactive, staggered, sitting/sleeping, 
			// or sprinting (AnimatedCameraDelta), 
			// and we want to discard these position/rotation changes, so return without updating.
			bool orbitStateActive = a_this->currentState->id == RE::CameraState::kAutoVanity;
			bool bleedoutStateActive = a_this->currentState->id == RE::CameraState::kBleedout;
			bool furnitureStateActive = a_this->currentState->id == RE::CameraState::kFurniture;
			bool localRotationModified = 
			{
				orbitStateActive || bleedoutStateActive || furnitureStateActive ||
				p1->IsInRagdollState() ||
				p1->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
				p1->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
				coopP1->pam->isSprinting
			};
			if (localRotationModified) 
			{
				return;
			}
			else
			{
				// Otherwise, run the original func first to allow other plugins 
				// that hook this func to execute their logic first 
				// before we re-apply the co-op camera orientation,
				// which was previously applied in the main hook.
				_Update(a_this);
				glob.cam->SetCamOrientation();
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
				SPDLOG_DEBUG("[TESObjectREFR Hooks] Cell: {} (0x{:X}): delete marker {} (0x{:X}).",
					a_this->parentCell->fullName,
					a_this->parentCell->formID,
					a_this->GetName(),
					a_this->formID);
				// Delete marker.
				a_this->SetDelete(true);
				// Set skybox only for interior cells to sometimes remove fog
				// when zooming out beyond the traversable area.
				Util::SetSkyboxOnlyForInteriorModeCell(a_this->parentCell);
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
			else
			{
				_BeginHCS(a_this);
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
				RE::NiMatrix3 m;
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
				_UpdateRotationTPCS(a_this);
			}
		}
		
// [VALUE MODIFIER EFFECT HOOKS]: 
		void ValueModifierEffectHooks::Start(RE::ValueModifierEffect* a_this)
		{
			// ENDERAL ONLY: Remove all Arcane Fever magic effects if P1 is in god mode.
			if (!glob.globalDataInit || 
				!glob.coopSessionActive || 
				!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				return _Start(a_this);
			}

			const auto& coopP1 = glob.coopPlayers[glob.player1CID]; 
			bool appliedToP1 = a_this->GetTargetActor() == RE::PlayerCharacter::GetSingleton();
			auto baseEffect = (a_this->GetBaseObject());
			bool removeArcaneFeverEffect = 
			{
				(appliedToP1) &&
				(coopP1->isInGodMode) &&
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

			// Save pre-transformation race to revert to later 
			// if the player is not already transforming/transformed.
			if (!p->isTransformed && !p->isTransforming)
			{
				p->preTransformationRace = p->coopActor->race;
			}

			p->isTransforming = true;

			if (!p->isPlayer1) 
			{
				// Unequip hand forms for the companion player before the transformation begins.
				// Game does not always do this automatically and the weapon anim object 
				// can stay attached to its hand node after the transformation.
				p->em->EquipFists();
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

			p->isTransforming = true;

			if (!p->isPlayer1)
			{
				// Unequip hand forms for co-op companion once the transformation begins.
				// Game does not always do this automatically and the weapon anim object 
				// can stay attached to its hand node after the transformation.
				p->em->EquipFists();
			}

			// Reset to base transformation time.
			p->secsMaxTransformationTime = 150.0f;
			p->transformationTP = SteadyClock::now();

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
			if (pIndex != -1)
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			SPDLOG_DEBUG
			(
				"[BarterMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. "
				"Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);
			
			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return _ProcessMessage(a_this, a_message);
			}

			const auto& p = glob.coopPlayers[resolvedCID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;

			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), nullptr
			);

			// Calculate the result of this message, and if it isn't handled, restore P1's data.
			auto result = _ProcessMessage(a_this, a_message);
			if (opening)
			{
				// Have to restore P1's inventory here 
				// if the game ignores this call to open the menu.
				if (result != RE::UI_MESSAGE_RESULTS::kHandled)
				{
					SPDLOG_DEBUG
					(
						"[BarterMenu Hooks] ERR: ProcessMessage. Restoring P1's inventory, "
						"since the message to open the menu was ignored. RESULT: {}.", 
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[BookMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. "
				"Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return result;
			}

			const auto& p = glob.coopPlayers[resolvedCID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;
			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), a_this->GetTargetForm()
			);

			return result;
		}

		RE::UI_MESSAGE_RESULTS ContainerMenuHooks::ProcessMessage
		(
			RE::ContainerMenu* a_this, RE::UIMessage& a_message
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[ContainerMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return result;
			}

			const auto& p = glob.coopPlayers[resolvedCID];
			const RE::BSFixedString menuName = a_this->MENU_NAME;

			// Copy over player data.
			GlobalCoopData::CopyOverCoopPlayerData
			(
				opening, menuName, p->coopActor->GetHandle(), nullptr
			);
			// Check if the opened container is a companion player's inventory.
			const auto containerMode = a_this->GetContainerMode();
			if (closing && containerMode == RE::ContainerMenu::ContainerMode::kNPCMode) 
			{
				RE::NiPointer<RE::TESObjectREFR> containerRefr;
				bool succ = RE::TESObjectREFR::LookupByHandle
				(
					RE::ContainerMenu::GetTargetRefHandle(), containerRefr
				);
				auto menuContainerHandle = 
				(
					containerRefr && containerRefr.get() ? 
					containerRefr->GetHandle() : 
					RE::ObjectRefHandle()
				);
				// If the container is the co-op companion player themselves, 
				// they are accessing their inventory.
				if (GlobalCoopData::GetCoopPlayerIndex(menuContainerHandle) == p->controllerID)
				{
					// Since the player may have (un)favorited new forms, 
					// update favorited form data.
					// No need to update magic favorites because there's no way 
					// of modifying them through the container menu.
					p->em->UpdateFavoritedFormsLists(true);
				}
			}

			return result;
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[CraftingMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return _ProcessMessage(a_this, a_message);
			}

			const auto& p = glob.coopPlayers[resolvedCID];
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
					SPDLOG_DEBUG
					(
						"[CraftingMenu Hooks] ERR: ProcessMessage: "
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[DialogueMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return result;
			}

			const auto& p = glob.coopPlayers[resolvedCID];
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
				// Not all updates made to menu elements through the MIM apply properly.
				// Certain entry elements must be updated right before the message is propagated,
				// so that any overriding changes made by the game 
				// can be overwritten by our own changes.
				auto taskInterface = SKSE::GetTaskInterface(); 
				if (!taskInterface)
				{
					return _ProcessMessage(a_this, a_message);
				}

				// Update quickslot tags for P1,
				// since the game wipes the tag after hotkeying an item.
				if (glob.menuCID == glob.player1CID) 
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
							if (!favoritesMenu || !favoritesMenu.get())
							{
								return;
							}

							auto view = favoritesMenu->uiMovie; 
							if (!view)
							{
								return;
							}

							const auto& p = glob.coopPlayers[glob.player1CID];
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
							RE::GFxValue entry;
							RE::GFxValue entryIndex;
							RE::GFxValue entryText;
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
								if (index == p->em->equippedQSItemIndex || 
									index == p->em->equippedQSSpellIndex)
								{
									bool isConsumable = index == p->em->equippedQSItemIndex;
									if (entryStr.find("[*QS", 0) == std::string::npos)
									{
										entryStr = fmt::format
										(
											"[*QS{}*] {}", isConsumable ? "I" : "S", entryStr
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
									// Update the favorites entry list.
									view->InvokeNoReturn
									(
										"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
									);

									SPDLOG_DEBUG
									(
										"[FavoritesMenu Hooks] ProcessMessage: "
										"Refreshing quickslot tags for P1."
									);
								}
							}
						}
					);
				}
				else if (glob.menuCID != -1)
				{
					// Update equip state for all favorited entries and refresh the item list.
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
							if (!favoritesMenu || !favoritesMenu.get())
							{
								return;
							}

							auto view = favoritesMenu->uiMovie; 
							if (!view)
							{
								return;
							}

							const auto& p = glob.coopPlayers[glob.menuCID];
							RE::ActorPtr menuCoopActorPtr = 
							(
								Util::GetActorPtrFromHandle(glob.mim->menuCoopActorHandle)
							);
							if (!menuCoopActorPtr || !menuCoopActorPtr.get())
							{
								return;
							}
							
							const auto& favoritesList = favoritesMenu->favorites;
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
							// Iterate through and update all entries for the companion player.
							for (uint32_t i = 0; i < numEntries; ++i)
							{
								RE::GFxValue entryIndex;
								RE::GFxValue entry;
								view->GetVariableArray
								(
									"_root.MenuHolder.Menu_mc.itemList.entryList",
									i, 
									std::addressof(entry),
									1
								);
								entry.GetMember("index", std::addressof(entryIndex));
								int32_t index = static_cast<int32_t>(entryIndex.GetNumber());

								RE::TESForm* favoritedItem = 
								(
									index != -1 ? favoritesList[index].item : nullptr
								);
								if (!favoritedItem)
								{
									continue;
								}

								// Get the form ID of the entry.
								RE::GFxValue entryFormId;
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
									RE::GFxValue entryText;
									entry.GetMember("text", std::addressof(entryText));
									std::string entryStr = entryText.GetString();

									// Update item count to reflect the number of that item 
									// in the co-op player's inventory, not P1's.
									// Ignore spells and shouts, which always have count 1.
									auto invCounts = menuCoopActorPtr->GetInventoryCounts();
									if (!favoritedItem->Is
										(
											RE::FormType::Spell, RE::FormType::Shout
										))
									{
										auto boundObj = favoritedItem->As<RE::TESBoundObject>();
										uint32_t count = 
										(
											invCounts.contains(boundObj) ? 
											invCounts.at(boundObj) :
											0
										);
										if (count > 1)
										{
											auto parenPos1 = entryStr.find("(");
											auto parenPos2 = entryStr.find(")");
											if (parenPos1 != std::string::npos &&
												parenPos2 != std::string::npos)
											{
												entryStr = 
												(
													entryStr.substr(0, parenPos1) + 
													"(" + 
													std::to_string(count) + 
													entryStr.substr(parenPos2)
												);
											}
											else
											{
												entryStr += " (" + std::to_string(count) + ")";
											}

											entryText.SetString(entryStr);
											entry.SetMember("text", entryText);
										}
									}

									// Update equip state for the entry.
									// Normal items receive an update to the "caret" equipped icon,
									// while quick slot items have their entry text modified.
									const auto& equipStateNum = 
									(
										glob.mim->favEntryEquipStates[index]
									);
									SPDLOG_DEBUG
									(
										"[FavoritesMenu Hooks] ProcessMessage: "
										"Favorites index {} item {} "
										"is getting its equip state set to {}",
										index, favoritedItem->GetName(), equipStateNum
									);
									RE::GFxValue equipState;
									equipState.SetNumber(static_cast<double>(equipStateNum));
									entry.SetMember("equipState", equipState);

									// Add quick slot item/spell tag.
									if (index == p->em->equippedQSItemIndex ||
										index == p->em->equippedQSSpellIndex)
									{
										bool isConsumable = index == p->em->equippedQSItemIndex;
										if (entryStr.find("[*QS", 0) == std::string::npos)
										{
											entryStr = fmt::format
											(
												"[*QS{}*] {}", isConsumable ? "I" : "S", entryStr
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

							// Update the favorites entry list.
							view->InvokeNoReturn
							(
								"_root.MenuHolder.Menu_mc.itemList.UpdateList", nullptr, 0
							);
						}
					);
				}

				// No more processing to do for this event.
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[FavoritesMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				const auto& p = glob.coopPlayers[resolvedCID];
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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
					if (ALYSLC::PersistentFavoritesCompat::g_persistentFavoritesInstalled) 
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
								// Indicate as sent by companion player.
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
							SPDLOG_DEBUG
							(
								"[FavoritesMenu Hooks] ERR: ProcessMessage. "
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
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				coopP1->em->UpdateFavoritedFormsLists(false);
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
			if (glob.menuCID != -1 && glob.menuCID != glob.player1CID)
			{
				// Close P1's inventory, if open, and open the companion player's.
				auto msgQ = RE::UIMessageQueue::GetSingleton(); 
				if (msgQ && opening)
				{
					const auto& reqP = glob.coopPlayers[glob.menuCID];
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
						reqP->controllerID,
						InputAction::kInventory, 
						SteadyClock::now(), 
						RE::ContainerMenu::MENU_NAME,
						reqP->coopActor->GetHandle()
					);

					if (succ)
					{
						SPDLOG_DEBUG
						(
							"[InventoryMenu Hooks] ProcessMessage: "
							"Opening {}'s inventory instead of P1's.", 
							reqP->coopActor->GetName()
						);
						Util::Papyrus::OpenInventory(reqP->coopActor.get());
					}

					// Ignore request to prevent further processing,
					// since we just opened the companion player's inventory instead.
					return RE::UI_MESSAGE_RESULTS::kIgnore;
				}
			}
			else
			{
				// Set favorited forms data for P1, including magical forms.
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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
				SPDLOG_DEBUG
				(
					"[LoadingMenu Hooks] ProcessMessage. "
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
				// Not all updates made to menu elements through the MIM apply properly.
				// Certain entry elements must be updated right before the message is propagated,
				// so that any overriding changes made by the game 
				// can be overwritten by our own changes.
				// Companion player controlling menus.
				if (glob.menuCID != -1 && glob.menuCID != glob.player1CID)
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
							if (!magicMenu || !magicMenu.get())
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
						}
					);
				}

				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[MagicMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				const auto& p = glob.coopPlayers[resolvedCID];
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
					if (ALYSLC::PersistentFavoritesCompat::g_persistentFavoritesInstalled)
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
							SPDLOG_DEBUG
							(
								"[MagicMenu Hooks] ERR: ProcessMessage. "
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
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				coopP1->em->UpdateFavoritedFormsLists(false);
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS StatsMenuHooks::ProcessMessage
		(
			RE::StatsMenu* a_this, RE::UIMessage& a_message
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

			// TODO: 
			// Implement Vampire Lord and Werewolf perk sync when co-op players are transformed.
			// So for now, do not modify perk data if P1 is transformed.
			auto p1 = RE::PlayerCharacter::GetSingleton();
			bool p1IsTransformed = Util::IsWerewolf(p1) || Util::IsVampireLord(p1);

			// Is P1 requesting to open the StatsMenu?
			// Have to also adjust perk data for P1 when outside of co-op.
			bool p1Req = !glob.coopSessionActive;
			if (glob.coopSessionActive)
			{
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				// Do not modify the requests queue, 
				// since the menu input manager still needs this info
				// when setting the request and menu controller IDs when this menu opens/closes.
				int32_t resolvedCID = glob.moarm->ResolveMenuControllerID
				(
					a_this->MENU_NAME, false
				);

				SPDLOG_DEBUG
				(
					"[StatsMenu Hooks] ProcessMessage. Current menu CID: {}, "
					"resolved menu CID: {}. Opening: {}, closing: {}.",
					glob.menuCID, resolvedCID, opening, closing
				);

				// Control is/was requested by co-op companion player.
				if (resolvedCID != -1 && resolvedCID != glob.player1CID && !p1IsTransformed)
				{
					// Copy back player data only if data was already copied.
					// Ignore subsequent hide messages once P1's data is restored.
					closing &= hasCopiedData;
					if (opening || closing)
					{
						const auto& p = glob.coopPlayers[resolvedCID];
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
			if ((p1Req && !p1IsTransformed && !ALYSLC::EnderalCompat::g_enderalSSEInstalled) && 
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
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[TrainingMenu Hooks] ProcessMessage. Current menu CID: {}, "
				"resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing
			);

			// Ignore subsequent hide messages once P1's data is restored.
			closing &= hasCopiedData;
			// Skip if control is/was not requested by co-op companion player,
			// or if not opening or closing.
			if ((resolvedCID == -1 || resolvedCID == glob.player1CID) || (!opening && !closing))
			{
				return _ProcessMessage(a_this, a_message);
			}

			const auto& p = glob.coopPlayers[resolvedCID];
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
					SPDLOG_DEBUG
					(
						"[TrainingMenu Hooks] ERR: ProcessMessage. "
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
			{
				return _CanProcess(a_this, a_event);
			}

			auto buttonEvent = a_event->AsButtonEvent();
			// 'Jump' event name and has P1 proxied bypass flag.
			if ((buttonEvent) && 
				(a_event->QUserEvent() == ue->jump) && 
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
			// From co-op player; 
			// ignore since we don't want another player controlling the camera orientation.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}
			
			auto thumbstickEvent = 
			(
				a_event->device == RE::INPUT_DEVICE::kGamepad &&
				*a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick ?
				skyrim_cast<RE::ThumbstickEvent*>(a_event) :
				nullptr
			);

			// Thumbstick event that is not from a companion player.
			// Ignore when the co-op camera is active.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && 
				glob.globalDataInit && 
				glob.coopSessionActive && 
				glob.cam->IsRunning() &&
				thumbstickEvent)
			{
				return false;
			}

			return _CanProcess(a_this, a_event);
		}

		bool MovementHandlerHooks::CanProcess(RE::MovementHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; 
			// ignore since we don't want another player to control P1's movement.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11)) 
			{
				return false;
			}

			auto thumbstickEvent = 
			(
				a_event->device == RE::INPUT_DEVICE::kGamepad && 
				*a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick ? 
				skyrim_cast<RE::ThumbstickEvent*>(a_event) :
				nullptr
			);

			// Thumbstick event that is not from a companion player.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && 
				glob.globalDataInit && 
				glob.coopSessionActive &&
				glob.cam->IsRunning() && 
				thumbstickEvent)
			{
				// Allow LS movement processing when motion driven. 
				// Otherwise, do not handle this movement event.
				bool menuStopsMovement = Util::OpenMenuStopsMovement();
				auto p1 = RE::PlayerCharacter::GetSingleton(); 
				if (p1 && 
					p1->movementController && 
					p1->movementController->unk1C5 && 
					thumbstickEvent->IsLeft() && 
					!menuStopsMovement)
				{
					const auto& coopP1 = glob.coopPlayers[glob.player1CID];
					// NOTE: 
					// Completely unnecessary if TDM is installed.
					if (!ALYSLC::TrueDirectionalMovementCompat::g_trueDirectionalMovementInstalled) 
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

					return true;
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore when not in co-op or not from a gamepad.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (!ue ||
				!glob.globalDataInit || 
				!glob.coopSessionActive || 
				a_event->device != RE::INPUT_DEVICE::kGamepad) 
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
			// From co-op player; ignore since we don't want another player controlling P1.
			auto idEvent = a_event->AsIDEvent(); 
			if ((idEvent) && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore while the co-op camera is active to prevent POV changes.
			auto ue = RE::UserEvents::GetSingleton(); 
			if (ue && 
				glob.globalDataInit && 
				glob.coopSessionActive &&
				glob.cam->IsRunning() && 
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			return _CanProcess(a_this, a_event);
		}
	}
}

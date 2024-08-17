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
			logger::debug("[Hooks] Installed all hooks");
		}

//=============
// [MAIN HOOK]:
//=============

		void MainHook::Update(RE::Main* a_this, float a_a2)
		{
			// Run the game's update first.
			_Update(a_this, a_a2);
			if (glob.globalDataInit)
			{
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
				// Draw the menu control overlay if a player is controlling menus while in co-op or the co-op summoning menu.
				glob.mim->DrawPlayerMenuControlOverlay();

				if (glob.allPlayersInit)
				{
					// Sequential updates to each players' managers for now.
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
						}
					}
				}

				// Update crosshair text after the players' managers have run their updates.
				if (glob.coopSessionActive) 
				{
					GlobalCoopData::HandlePlayerArmCollisions();
					GlobalCoopData::SetCrosshairText();
				}
			}
		}

//=================
// [GENERAL HOOKS]:
//=================

// [ACTOR EQUIP MANAGER HOOKS]:
		void ActorEquipManagerHooks::EquipObject(RE::ActorEquipManager* a_this, RE::Actor* a_actor, RE::TESBoundObject* a_object, const RE::ObjectEquipParams& a_objectEquipParams)
		{
			if (a_actor && a_object && glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_actor); playerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerIndex];
					// Is co-op companion with a playable race and not transforming.
					if (!p->isPlayer1 && p->coopActor->race && p->coopActor->race->GetPlayable() && 
						!p->isTransforming && !p->isTransformed)
					{
						logger::debug("[ActorEquipManager Hooks] EquipObject: {}: {} (0x{:X}, type: 0x{:X}).", a_actor->GetName(), a_object->GetName(), a_object->formID, *a_object->formType);
						if (auto reqEquipSlot = a_objectEquipParams.equipSlot; reqEquipSlot)
						{
							// Ensure 1H weapon gets equipped in the correct hand.
							// Certain races, like the Manakin race, do not equip weapons in the LH unless the weapon's equip slot is set to the LH slot.
							// NOTE: Has not been tested thoroughly for long-term side-effects, but nothing to report so far.
							if (auto itemEquipType = a_object->As<RE::BGSEquipType>(); itemEquipType && itemEquipType->equipSlot)
							{
								if (a_object->As<RE::TESObjectWEAP>())
								{
									// Right equip slot to left. Set to either.
									if (reqEquipSlot == glob.leftHandEquipSlot && itemEquipType->equipSlot == glob.rightHandEquipSlot)
									{
										itemEquipType->SetEquipSlot(glob.eitherHandEquipSlot);
									}
									// Left equip slot to right. Set to right.
									else if (reqEquipSlot == glob.rightHandEquipSlot && itemEquipType->equipSlot == glob.leftHandEquipSlot)
									{
										itemEquipType->SetEquipSlot(glob.rightHandEquipSlot);
									}
								}
							}
						}

						// Do not want to unequip auto-equipped bound objects, since these objects are typically equipped after a spell is cast
						// and take the place of the cast spell in its hand slot.
						// Also ignore instances where the game equips the "fists" weapon to clear hand slots.
						bool isBound = {
							(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
							(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
						};
						if (!isBound && a_object != glob.fists)
						{
							//=================================================
							// Cases in which the equip call should be skipped.
							//=================================================

							// Game tries to equip another ammo that was not requested.
							if ((a_object->IsAmmo()) && a_object != p->em->desiredEquippedForms[!EquipIndex::kAmmo])
							{
								logger::debug("[ActorEquipManager Hooks] {}: trying to equip ammo {}. Ignoring.", a_actor->GetName(), a_object->GetName());
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
									// Get equip index offset (from first biped slot index) by shifting biped slot index right until equal to 0.
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
										// Game is trying to equip armor that the player did not request to have equipped.
										if (armor != p->em->desiredEquippedForms[equipIndex])
										{
											logger::debug("[ActorEquipManager Hooks] {}: trying to equip armor {}. Ignoring.", a_actor->GetName(), a_object->GetName());
											return;
										}
									}
								}
							}
							else if (a_object->IsWeapon())
							{
								uint8_t indexToCheck = !EquipIndex::kRightHand;
								bool isLHEquip = { 
									(a_objectEquipParams.equipSlot == glob.leftHandEquipSlot) || 
									(a_object->As<RE::BGSEquipType>() && a_object->As<RE::BGSEquipType>()->equipSlot == glob.leftHandEquipSlot)
								};
								if (isLHEquip)
								{
									indexToCheck = !EquipIndex::kLeftHand;
								}

								// Game is trying to equip a weapon that the player did not request to have equipped.
								if (a_object != p->em->desiredEquippedForms[indexToCheck])
								{
									logger::debug("[ActorEquipManager Hooks] {}: trying to equip weapon {}. Ignoring.", a_actor->GetName(), a_object->GetName());
									return;
								}
							}
							else if (a_object->As<RE::TESShout>())
							{
								if (a_object != p->em->desiredEquippedForms[!EquipIndex::kVoice]) 
								{
									logger::debug("[ActorEquipManager Hooks] {}: trying to equip shout {}. Ignoring.", a_actor->GetName(), a_object->GetName());
									return;
								}
							}
						}
						else
						{
							// Special bound weapon handling for co-op companions.
							if (isBound)
							{
								// Have to check if this bound weapon was equipped following a request from the player.
								// The game will automatically try to equip the bound bow even if we've unequipped it on weapon sheathe or after its duration has elapsed.
								if (a_object->IsWeapon())
								{
									auto weap = a_object->As<RE::TESObjectWEAP>();
									auto equipSlotToUse = weap->equipSlot == glob.bothHandsEquipSlot ? weap->equipSlot : a_objectEquipParams.equipSlot;
									bool reqToEquip = 
									{
										(equipSlotToUse == glob.leftHandEquipSlot && p->pam->boundWeapReqLH) ||
										(equipSlotToUse == glob.rightHandEquipSlot && p->pam->boundWeapReqRH) ||
										(equipSlotToUse == glob.bothHandsEquipSlot && p->pam->boundWeapReqLH && p->pam->boundWeapReqRH)
									};
									// Player did not request to equip a bound weapon, so ignore.
									if (!reqToEquip)
									{
										logger::debug("[ActorEquipManager Hooks] {}: trying to equip bound weapon {}. Ignoring.", a_actor->GetName(), a_object->GetName());
										return;
									}
								}
							}
							else
							{
								// Unlike for P1, fists do not automatically get unequipped for companion players, so do it here.
								UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
							}
						}
					}
				}
			}

			_EquipObject(a_this, a_actor, a_object, a_objectEquipParams);
		}

		void ActorEquipManagerHooks::UnequipObject(RE::ActorEquipManager* a_this, RE::Actor* a_actor, RE::TESBoundObject* a_object, const RE::ObjectEquipParams& a_objectEquipParams)
		{
			if (a_actor && a_object && glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_actor); playerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerIndex];
					// Is co-op companion with a playable race and not transforming.
					if (!p->isPlayer1 && p->coopActor->race && p->coopActor->race->GetPlayable() &&
						!p->isTransforming && !p->isTransformed)
					{
						logger::debug("[ActorEquipManager Hooks] UnequipObject: {}: {} (0x{:X}, type: 0x{:X}).", a_actor->GetName(), a_object->GetName(), a_object->formID, *a_object->formType);
						bool isBound = {
							(a_object->IsWeapon() && a_object->As<RE::TESObjectWEAP>()->IsBound()) ||
							(a_object->IsAmmo() && a_object->HasKeywordByEditorID("WeapTypeBoundArrow"))
						};

						// Ignore instances where the game unequips bound weapons and unequips the "fists" weapon after clearing hand slots.
						if (!isBound && a_object != glob.fists)
						{
							//===================================================
							// Cases in which the unequip call should be skipped.
							//===================================================

							if (auto armor = a_object->As<RE::TESObjectARMO>(); (armor && armor->IsShield()) || a_object->As<RE::TESObjectLIGH>())
							{
								// Ignore if the game is trying to unequip the desired shield/torch.
								if (a_object == p->em->desiredEquippedForms[!EquipIndex::kLeftHand])
								{
									// Previous equip calls to equip a bound weapon to the LH slot only unequip
									// the shield/torch from the LH slot and not from its biped armor slots, so
									// if we currently have a bound weapon equipped in the LH, do not ignore the unequip call.
									if (auto lhWeap = p->em->GetLHWeapon(); !lhWeap || !lhWeap->IsBound())
									{
										logger::debug("[ActorEquipManager Hooks] {}: trying to unequip shield/torch {}. Ignoring.", a_actor->GetName(), a_object->GetName());
										return;
									}
								}
							}
							else if (armor)
							{
								auto bipedSlotMask = !armor->GetSlotMask();
								int8_t indexOffset = -1;
								// Get equip index offset (from first biped slot index) by shifting biped slot index right until equal to 0.
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
										logger::debug("[ActorEquipManager Hooks] {}: trying to unequip armor {}. Ignoring.", a_actor->GetName(), a_object->GetName());
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
									(a_object->As<RE::BGSEquipType>() && a_object->As<RE::BGSEquipType>()->equipSlot == glob.leftHandEquipSlot)
								};
								if (isLHEquip)
								{
									indexToCheck = !EquipIndex::kLeftHand;
								}

								// Ignore if the game is trying to equip the desired weapon.
								if (a_object == p->em->desiredEquippedForms[indexToCheck])
								{
									logger::debug("[ActorEquipManager Hooks] {}: trying to unequip weapon {}. Ignoring.", a_actor->GetName(), a_object->GetName());
									return;
								}
							}
							else if (a_object->As<RE::TESShout>())
							{
								if (a_object == p->em->desiredEquippedForms[!EquipIndex::kVoice])
								{
									logger::debug("[ActorEquipManager Hooks] {}: trying to unequip shout {}. Ignoring.", a_actor->GetName(), a_object->GetName());
									return;
								}
							}
						}
					}
				}
			}

			_UnequipObject(a_this, a_actor, a_object, a_objectEquipParams);
		}

// [ANIMATION GRAPH MANAGER HOOKS]:
		EventResult AnimationGraphManagerHooks::ProcessEvent(RE::BSAnimationGraphManager* a_this, const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
		{
			if (glob.globalDataInit && glob.allPlayersInit && glob.coopSessionActive)
			{
				if (a_event && a_event->holder)
				{
					if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_event->holder->formID); playerIndex != -1)
					{
						const auto& p = glob.coopPlayers[playerIndex];
						auto& pam = p->pam;
						if (pam->IsRunning())
						{
							// When transformed into the Vampire Lord:
							// (un)equip spells and start/stop glow FX.
							if (!p->isPlayer1 && p->isTransformed && Util::IsVampireLord(p->coopActor.get())) 
							{
								if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
								{
									if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
									{
										auto vampireBodyFloatFX = dataHandler->LookupForm<RE::BGSArtObject>(0x15FC5, "Dawnguard.esm");
										// Play/stop hit effect art.
										if (a_event->tag == "GroundStart")
										{
											Util::StopHitArt(p->coopActor.get(), vampireBodyFloatFX);
											aem->EquipObject(p->coopActor.get(), glob.fists);
										}
										else if (a_event->tag == "LevitateStart")
										{
											Util::StartHitArt(p->coopActor.get(), vampireBodyFloatFX, p->coopActor.get());
											RE::SpellItem* leveledDrainSpell = nullptr;
											RE::SpellItem* leveledRaiseDeadSpell = nullptr;
											float playerLevel = p->coopActor->GetLevel();
											if (playerLevel <= 10.0f)
											{
												leveledDrainSpell = dataHandler->LookupForm<RE::SpellItem>(0x19324, "Dawnguard.esm");
												leveledRaiseDeadSpell = dataHandler->LookupForm<RE::SpellItem>(0xBA54, "Dawnguard.esm");
											}
											else if (playerLevel <= 20.0f)
											{
												leveledDrainSpell = dataHandler->LookupForm<RE::SpellItem>(0x19326, "Dawnguard.esm");
												leveledRaiseDeadSpell = dataHandler->LookupForm<RE::SpellItem>(0x13EC8, "Dawnguard.esm");
											}
											else if (playerLevel <= 30.0f)
											{
												leveledDrainSpell = dataHandler->LookupForm<RE::SpellItem>(0x19AD5, "Dawnguard.esm");
												leveledRaiseDeadSpell = dataHandler->LookupForm<RE::SpellItem>(0x13EC9, "Dawnguard.esm");
											}
											else if (playerLevel <= 40.0f)
											{
												leveledDrainSpell = dataHandler->LookupForm<RE::SpellItem>(0x19AD6, "Dawnguard.esm");
												leveledRaiseDeadSpell = dataHandler->LookupForm<RE::SpellItem>(0x13ECA, "Dawnguard.esm");
											}
											else
											{
												leveledDrainSpell = dataHandler->LookupForm<RE::SpellItem>(0x19AD7, "Dawnguard.esm");
												leveledRaiseDeadSpell = dataHandler->LookupForm<RE::SpellItem>(0x13ECB, "Dawnguard.esm");
											}

											if (leveledDrainSpell)
											{
												leveledDrainSpell = p->em->CopyToPlaceholderSpell(leveledDrainSpell, PlaceholderMagicIndex::kRH);
												aem->EquipSpell(p->coopActor.get(), leveledDrainSpell->As<RE::SpellItem>(), glob.rightHandEquipSlot);
											}

											if (leveledRaiseDeadSpell)
											{
												leveledRaiseDeadSpell = p->em->CopyToPlaceholderSpell(leveledRaiseDeadSpell, PlaceholderMagicIndex::kLH);
												aem->EquipSpell(p->coopActor.get(), leveledRaiseDeadSpell->As<RE::SpellItem>(), glob.leftHandEquipSlot);
											}
										}
									}
								}
							}

							// Set performed action anim event tag so that the player action manager
							// can handle AV modification later when it updates, which minimizes processing done
							// by this game thread.
							auto perfAVAnimEvent = std::pair<PerfAnimEventTag, uint16_t>(PerfAnimEventTag::kNone, 0);
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
							// Attack complete. Reset weapon damage mult once the attack stop animation event fires.
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
							default:
								// No need to handle.
								return EventResult::kContinue;
							}

							// Increment with wrap-around.
							pam->lastAnimEventID = pam->lastAnimEventID == UINT16_MAX ? 1 : pam->lastAnimEventID + 1;

							// REMOVE when done debugging.
							logger::debug("[AnimationGraphManager Hook] ProcessEvent: player: {}, payload: {}, tag: {}, sessionAnimsCount: {}",
								a_event->holder ? a_event->holder->GetName() : "NONE",
								a_event->payload.data(), a_event->tag,
								pam->lastAnimEventID);

							// Improves recovery speed from transition from dodge end to movement start.
							if (Hash(a_event->tag) == "TKDodgeStop"_h || Hash(a_event->tag) == "TKDR_DodgeEnd"_h)
							{
								//p->coopActor->StopMoving(*g_deltaTimeRealTime);
								p->mm->SetDontMove(true);
								p->coopActor->NotifyAnimationGraph("moveStop");
								if (p->mm->lsMoved)
								{
									p->mm->SetDontMove(false);
									p->coopActor->NotifyAnimationGraph("moveStart");
								}
							}

							p->lastAnimEventTag = a_event->tag;

							// REMOVE
							const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
							logger::debug("[AnimationGraphManager Hook] ProcessEvent: {}: Lock: 0x{:X}.", p->coopActor->GetName(), hash);
							{
								std::unique_lock<std::mutex> perfAnimQueueLock(p->pam->avcam->perfAnimQueueMutex);
								logger::debug("[AnimationGraphManager Hook] ProcessEvent: {}: Lock obtained: 0x{:X}.", p->coopActor->GetName(), hash);
								if (perfAVAnimEvent.first != PerfAnimEventTag::kNone)
								{
									p->pam->avcam->perfAnimEventsQueue.emplace(std::move(perfAVAnimEvent));
								}
							}
						}
					}
				}
			}

			return _ProcessEvent(a_this, a_event, a_eventSource);
		}

// [BS CULLING PROCESS HOOKS]:
		void BSCullingProcessHooks::Process1(RE::BSCullingProcess* a_this, RE::NiAVObject* a_object, std::uint32_t a_arg2)
		{
			// NOTE: Reduces the number of freezes when using this mod with the Minimap active, but they still occur.
			// Turning on cam collisions and NOT removing occlusion seems to be the most stable option.
			if ((ALYSLC::MiniMapCompat::g_miniMapInstalled && ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround) && 
				(Settings::bRemoveExteriorOcclusion || Settings::bRemoveInteriorOcclusion))
			{
				if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && p1->parentCell)
				{
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
					return;
				}
			}

			_Process1(a_this, a_object, a_arg2);
		}

// [BS MULTI BOUND HOOKS]:
		bool BSMultiBoundHooks::QWithinPoint(RE::BSMultiBound* a_this, const RE::NiPoint3& a_pos)
		{
			if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && p1->parentCell)
			{
				auto cell = p1->parentCell;
				auto sky = RE::TES::GetSingleton() ? RE::TES::GetSingleton()->sky : nullptr;
				if ((Settings::bRemoveInteriorOcclusion && (cell->IsInteriorCell() || (sky && sky->mode == RE::Sky::Mode::kInterior))) ||
					(Settings::bRemoveExteriorOcclusion && cell->IsExteriorCell()))
				{
					for (auto refrMB : cell->loadedData->multiboundRefMap)
					{
						if (refrMB.second && refrMB.second->multiBound && refrMB.second->multiBound.get() && refrMB.second->multiBound.get() == a_this)
						{
							const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
							if (ALYSLC::MiniMapCompat::g_miniMapInstalled && !ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround)
							{
								ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround = true;
							}

							return true;
						}
					}
				}
			}

			return _QWithinPoint(a_this, a_pos);
		}

// [CHARACTER HOOKS]:
		float CharacterHooks::CheckClampDamageModifier(RE::Character* a_this, RE::ActorValue a_av, float a_delta)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
				// Do not allow any AVs to change when downed and do not allow health, magicka, or stamina
				// to change while in god mode.
				bool hmsActorValue = a_av == RE::ActorValue::kHealth || a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina;
				// Is a player.
				if (playerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerIndex];
					// Do not modify AVs when no players are being dismissed and
					// the co-op actor is not revived or if an HMS AV is being decreased in god mode.
					bool notDismissingPlayers = !Settings::bUseReviveSystem || glob.livingPlayers == glob.activePlayers;
					if ((notDismissingPlayers) && (!p->isRevived || (hmsActorValue && p->isInGodMode && a_delta < 0.0f)))
					{
						return 0.0f;
					}
					else
					{
						// Check if the player is self-healing, and if so, add skill XP to their Restoration skill.
						if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
						{
							// Clamp first.
							float currentHealth = p->coopActor->GetActorValue(RE::ActorValue::kHealth);
							float currentMaxHealth = 
							(
								p->coopActor->GetBaseActorValue(RE::ActorValue::kHealth) + 
								p->coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth)
							);
							float baseXP = std::clamp(a_delta, 0.0f, currentMaxHealth - currentHealth);
							// Prevent full-heal on combat exit for co-op companions.
							// Check if heal delta is larger than per-frame health regen/total healing effect regen.
							if (currentHealth < currentMaxHealth && a_delta >= currentMaxHealth - currentHealth)
							{
								bool isHealing = false;
								float healingDeltaTotal = 0.0f;
								if (a_this->GetActiveEffectList())
								{
									for (auto effect : *a_this->GetActiveEffectList())
									{
										if (effect)
										{
											if (auto valueModifierEffect = skyrim_cast<RE::ValueModifierEffect*>(effect); 
												valueModifierEffect && valueModifierEffect->actorValue == RE::ActorValue::kHealth && valueModifierEffect->value > 0.0f)
											{
												isHealing = true;
												healingDeltaTotal += valueModifierEffect->value;
											}
										}
									}
								}

								float realDelta = currentMaxHealth - currentHealth;
								// REMOVE
								float maxHealthRegenDeltaPerFrame = currentMaxHealth * (p->coopActor->GetActorValue(RE::ActorValue::kHealRate) / 100.0f) * (p->coopActor->GetActorValue(RE::ActorValue::kHealRateMult) / 100.0f);
								logger::debug("[Character Hook] CheckClampDamageModifier: {} was healed to full: {} -> {}, delta: {}, real delta: {}, diff: {}. Max health regen delta per frame: {}, healing delta total: {}. Is healing: {}, in combat: {}",
									a_this->GetName(), currentHealth, currentMaxHealth, a_delta, realDelta, a_delta - realDelta,
									maxHealthRegenDeltaPerFrame, healingDeltaTotal, isHealing,
									a_this->IsInCombat());

								// Seems as if the delta that heals the player to full is always very close to (current max health - current health) + 1.
								// Good enough for filtering out this health change.
								if (abs(1.0f - (a_delta - realDelta)) < 0.0001f && a_delta > healingDeltaTotal)
								{
									return 0.0f;
								}
							}

							if (baseXP > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellRH))
							{
								// Targets self.
								if (const auto rhSpell = p->em->GetRHSpell(); rhSpell && rhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
								{
									GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, a_delta);
								}
							}

							if (baseXP > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellLH))
							{
								// Targets self.
								if (const auto lhSpell = p->em->GetLHSpell(); lhSpell && lhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
								{
									GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, a_delta);
								}
							}
						}
						else if ((a_delta < 0.0f) && (a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina))
						{
							// Scaled by cost multiplier here instead of in the cost functions in the player action function holder
							// because we want a consistent solution for both types of players.
							// Drawback would be not being able to link an action with a specific magicka/stamina reduction, 
							// since this function does not provide any context.
							float mult = a_av == RE::ActorValue::kMagicka ? Settings::vfMagickaCostMult[p->playerID] : Settings::vfStaminaCostMult[p->playerID];
							a_delta *= mult;
						}
					}
				}
				else
				{
					// Check if this actor is being healed by a co-op player.
					if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
					{
						for (const auto& p : glob.coopPlayers)
						{
							if (p->isActive && a_this->GetHandle() == p->tm->GetRangedTargetActor())
							{
								float currentHealth = a_this->GetActorValue(RE::ActorValue::kHealth);
								float currentMaxHealth = a_this->GetBaseActorValue(RE::ActorValue::kHealth) + a_this->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
								float healthDelta = std::clamp(a_delta, 0.0f, currentMaxHealth - currentHealth);
								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellRH))
								{
									// Restoration spell that does not target the caster.
									if (const auto rhSpell = p->em->GetRHSpell(); rhSpell && rhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}

								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellLH))
								{
									// Restoration spell that does not target the caster.
									if (const auto lhSpell = p->em->GetLHSpell(); lhSpell && lhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}

								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellQS))
								{
									// Restoration spell that does not target the caster.
									if (p->em->quickSlotSpell && p->em->quickSlotSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}
							}
						}
					}
				}
			}

			return _CheckClampDamageModifier(a_this, a_av, a_delta);
		}

		void CharacterHooks::DrawWeaponMagicHands(RE::Character* a_this, bool a_draw)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				// Do not allow the game to automatically sheathe/unsheathe the player actor's weapons/magic.
				if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); playerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerIndex];
					// Blocking weapon/magic drawing while transforming crashes the game at times.
					if (a_draw != p->pam->weapMagReadied && !p->isTransforming)
					{
						return;
					}
				}
			}

			_DrawWeaponMagicHands(a_this, a_draw);
		}

		void CharacterHooks::HandleHealthDamage(RE::Character* a_this, RE::Actor* a_attacker, float a_damage)
		{
			// NOTE: Damage is negative.
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				// Check for damage dealt by a player.
				auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(a_attacker);
				// Check for damage dealt to a player.
				auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
				bool playerVictim = playerVictimIndex != -1;
				bool playerAttacker = playerAttackerIndex != -1;

				float damageMult = 1.0f;
				if (playerVictim)
				{
					const auto& p = glob.coopPlayers[playerVictimIndex];
					damageMult *= Settings::vfDamageReceivedMult[p->playerID];
				}

				if (playerAttacker)
				{
					const auto& p = glob.coopPlayers[playerAttackerIndex];
					// Check for friendly fire (not from self) and negate damage.
					if (!Settings::vbFriendlyFire[p->playerID] && Util::IsPartyFriendlyActor(a_this) && a_this != p->coopActor.get())
					{
						a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -a_damage);
						return;
					}

					// Apply damage dealt mult.
					damageMult *= Settings::vfDamageDealtMult[p->playerID];
					// TEMP until I find a direct way of applying the sneak damage multiplier to all forms of damage.
					if (p->pam->attackDamageMultSet && p->pam->reqDamageMult != 1.0f)
					{
						// Apply sneak attack mult.
						damageMult *= p->pam->reqDamageMult;
						// Reset damage multiplier if performing a ranged sneak attack.
						// Melee sneak attacks reset the damage multiplier on attack stop,
						// but I have yet to find a way to check if the player no longer has any active
						// projectiles, so reset the damage multiplier on damaging hit.
						p->pam->ResetAttackDamageMult();
					}
				}

				// Adjust damage based off new damage mult.
				// Done before death (< 0 HP) checks below.
				// Ignore direct modifications of HP or no direct attacker (a_attacker == nullptr).
				// NOTE: Unfortunately, this means fall damage (no attacker) will not scale down with the player's damage received mult.
				if (float deltaHealth = a_damage * (damageMult - 1.0f); deltaHealth != 0.0f && a_attacker)
				{
					a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth);

					// REMOVE
					logger::debug("[Character Hook] HandleHealthDamage: {}: Damage received goes from {} to {} (x{}). Health delta: {}. Attacker: {}, new health: {}.",
						a_this->GetName(),
						a_damage,
						a_damage * damageMult,
						damageMult,
						deltaHealth,
						a_attacker ? a_attacker->GetName() : "NONE",
						a_this->GetActorValue(RE::ActorValue::kHealth));

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
							logger::debug("[Character Hook] HandleHealthDamage: Using revive system: {} is downed and needs to be revived! {} inflicted {} damage.",
								p->coopActor->GetName(), a_attacker ? a_attacker->GetName() : "NONE", a_damage);
							p->isDowned = true;
							p->isRevived = false;
							p->lastDownedTP = SteadyClock::now();
							p->RequestStateChange(ManagerState::kPaused);

							// Check if all players are downed, and if so, end the co-op session.
							bool playerStillStanding = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
								[](const auto& a_p) {
									return a_p->isActive && !a_p->isDowned;
								});
							if (!playerStillStanding)
							{
								logger::debug("[Character Hook] HandleHealthDamage: All players are downed. Ending co-op session.");
								GlobalCoopData::YouDied();
							}
							else
							{
								logger::debug("[Character Hook] HandleHealthDamage: {} is downed and starting downed state countdown.", p->coopActor->GetName());
								// Set health/health regen to 0 to prevent player from getting up prematurely when being revived.
								p->coopActor->SetBaseActorValue(RE::ActorValue::kHealRateMult, 0.0f);
								p->coopActor->RestoreActorValue
								(
									RE::ACTOR_VALUE_MODIFIER::kDamage, 
									RE::ActorValue::kHealth, 
									-p->coopActor->GetActorValue(RE::ActorValue::kHealth)
								);
								// Put in an alive ragdoll state.
								Util::PushActorAway(p->coopActor.get(), p->coopActor->data.location, -1.0f, true);
								// Ensure that the player will stay downed and no death events trigger during the downed state countdown.
								// Certain attacks such as spider venom while co-op data is copied onto P1 seem to reset the essential flag.
								p->pam->SetEssentialForReviveSystem();
								p->taskRunner->AddTask([&p]() { p->DownedStateCountdownTask(); });
								// Already 'dead', no need to continue handling removed health.
								if (a_this->GetActorValue(RE::ActorValue::kHealth) < 0.0f)
								{
									return;
								}
							}
						}
						else
						{
							logger::debug("[Character Hook] HandleHealthDamage: Not using revive system: {} is dead. Handling player dismissal and ending co-op session.", p->coopActor->GetName());
							// If not using the revive system, once one player dies, all other players die and the co-op session ends.
							GlobalCoopData::YouDied(p->coopActor.get());
						}
					}
				}

				// Player inflicted health damage on this character.
				// Add XP, if the attacker is not P1, and set killer if this character is now dead.
				if (playerAttacker)
				{
					// NOTE: Handled friendly fire check above already, so guaranteed to either have friendly fire enabled 
					// or attacking a target that is not party-friendly.
					const auto& p = glob.coopPlayers[playerAttackerIndex];
					// Do not give attacking player XP if attacking another player that is in god mode.
					const auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_this);
					bool victimPlayerInGodMode = playerVictimIndex != -1 && glob.coopPlayers[playerVictimIndex]->isInGodMode;
					if (!p->isPlayer1 && !victimPlayerInGodMode)
					{
						// Check spell attack source and increment skill XP if needed.
						const auto lhForm = p->em->equippedForms[!EquipIndex::kLeftHand];
						const auto rhForm = p->em->equippedForms[!EquipIndex::kRightHand];
						const auto qsSpellForm = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
						auto addDestructionXP =
							[&p, &a_damage, a_this](RE::TESForm* a_potentialSourceForm) {
								if (a_potentialSourceForm)
								{
									// Is destruction spell.
									if (const auto spell = a_potentialSourceForm->As<RE::SpellItem>(); spell && 
										spell->avEffectSetting && spell->avEffectSetting->data.associatedSkill == RE::ActorValue::kDestruction)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kDestruction, -a_damage);
									}
								}
							};

						// Check for destruction spell cast from LH/RH/Quick Slot.
						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellLH))
						{
							addDestructionXP(lhForm);
						}

						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellRH))
						{
							addDestructionXP(rhForm);
						}

						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellQS))
						{
							addDestructionXP(qsSpellForm);
						}
					}

					// Killed by co-op player.
					if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && !a_this->IsEssential() && a_this->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)
					{
						// Enderal treats dead actors without an associated killer as killed by P1,
						// so clear out the handle here to get XP from killing this actor.
						if (ALYSLC::EnderalCompat::g_enderalSSEInstalled) 
						{
							a_this->myKiller = RE::ActorHandle();
						}
						else
						{
							a_this->myKiller = p1;
						}

						// Have to store info to give the killer player first rights to loot the corpse with the QuickLoot menu.
						if (ALYSLC::QuickLootCompat::g_quickLootInstalled)
						{
							// Use extra data to store the real killer, since P1 takes the blame for any co-op companion kills.
							// NOTE: I am assuming this exData is not used on corpses, 
							// and that the exDataType's pad is also unused for corpses.
							// Seems to be the case from testing thusfar.

							// Pad14 is set to the killer player's FID and set to 0 
							// once the player has opened the QuickLoot menu for the corpse.
							if (!a_this->extraList.HasType<RE::ExtraForcedTarget>())
							{
								logger::debug("[Character Hook] HandleHealthDamage: {} was killed by {}. Saving this player as the real killer through exData's pad14.",
									a_this->GetName(), p->coopActor->GetName());
								// I guess Add() manages the lifetime of the newly allocated extra data pointer?
								// No explicit free call after calling Add() in two other places within the CommonLibSSE source.
								RE::ExtraForcedTarget* exData{ RE::BSExtraData::Create<RE::ExtraForcedTarget>() };
								exData->target = RE::ObjectRefHandle();
								exData->pad14 = p->coopActor->formID;
								a_this->extraList.Add(exData);
							}
							else
							{
								logger::debug("[Character Hook] HandleHealthDamage: {} was killed by {}. Setting real killer. Already has exData with target {}, pad14: 0x{:X}.",
									a_this->GetName(), p->coopActor->GetName(),
									Util::HandleIsValid(a_this->extraList.GetByType<RE::ExtraForcedTarget>()->target) ? a_this->extraList.GetByType<RE::ExtraForcedTarget>()->target.get()->GetName() : "NONE",
									a_this->extraList.GetByType<RE::ExtraForcedTarget>()->pad14);
								auto exData = a_this->extraList.GetByType<RE::ExtraForcedTarget>();
								exData->pad14 = p->coopActor->formID;
							}
						}
					}
				}
			}

			_HandleHealthDamage(a_this, a_attacker, a_damage);
		}

		void CharacterHooks::ModifyAnimationUpdateData(RE::Character* a_this, RE::BSAnimationUpdateData& a_data)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (const auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(a_this); playerIndex != -1)
				{
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
					}

					if (p->mm->isDashDodging)
					{
						a_data.deltaTime *= Settings::fDashDodgeAnimSpeedFactor;
					}
				}
			}

			_ModifyAnimationUpdateData(a_this, a_data);
		}

		bool CharacterHooks::NotifyAnimationGraph(RE::Character* a_this, const RE::BSFixedString& a_eventName)
		{
			if (glob.globalDataInit && glob.allPlayersInit && glob.coopSessionActive)
			{
				int8_t playerIndex = -1;
				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive)
					{
						// This character is really a IAnimationGraphManagerHolder ptr, so accessing its FID returns garbage.
						// Perform a pointer comp instead.
						if (fmt::ptr(a_this) == fmt::ptr(p->coopActor->As<RE::IAnimationGraphManagerHolder>()))
						{
							playerIndex = p->controllerID;
							break;
						}
					}
				}

				if (playerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerIndex];
					auto hash = Hash(a_eventName);
					
					// REMOVE when done debugging.
					//logger::debug("[Character Hooks] NotifyAnimationGraph: {}: {}.", p->coopActor->GetName(), a_eventName);

					if (p->isTransformed) 
					{
						// Do not allow the game to toggle the levitation state again
						// after the player's toggle request is being fulfilled.
						// Each time an equip/unequip event fires, the game tries to toggle levitation 2 times.
						if (p->isTogglingLevitationState && (hash == "LevitationToggle"_h || hash == "LevitationToggleMoving"_h)) 
						{
							return false;
						}
					}

					// Prevent game from forcing the co-op companion player out of sneaking while in combat.
					if (hash == "SneakStop"_h)
					{
						auto package = p->coopActor->GetCurrentPackage();
						bool alwaysSneakFlagSet = false;
						if (package)
						{
							alwaysSneakFlagSet = package->packData.packFlags.all(RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak);
						}

						// Flag gets ignored and is not modified while in combat,
						// so it retains the value set by the player action functions holder,
						// which is used to indicate whether the player wants to sneak or not.
						// If attempting to exit the sneak state while the player wants to remain
						// sneaking, return false.
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
					else if (((p->isDowned && !p->isRevived) || (p->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && hash == "GetUpBegin"_h)
					{
						// Ignore requests to get up when the player is downed and not revived.
						return false;
					}
					else if (p->mm->isRequestingDashDodge && !p->mm->isDashDodging && hash == "SneakStart"_h)
					{
						bool succ = _NotifyAnimationGraph(a_this, a_eventName);
						// SneakStart fails when transformed.
						if (succ || p->isTransformed)
						{
							// REMOVE
							const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
							logger::debug("[Character Hooks] NotifyAnimationGraph: {}: ProcessEvent: Lock: 0x{:X}.", p->coopActor->GetName(), hash);
							{
								std::unique_lock<std::mutex> perfAnimQueueLock(p->pam->avcam->perfAnimQueueMutex);
								logger::debug("[Character Hooks] NotifyAnimationGraph: {}: ProcessEvent: Lock obtained: 0x{:X}.", p->coopActor->GetName(), hash);

								// Queue dodge anim event tag so that this player's player action manager can handle stamina expenditure.
								p->pam->avcam->perfAnimEventsQueue.emplace(std::pair<PerfAnimEventTag, uint16_t>(PerfAnimEventTag::kDodgeStart, p->pam->lastAnimEventID));
								p->pam->lastAnimEventID = p->pam->lastAnimEventID == UINT16_MAX ? 1 : p->pam->lastAnimEventID + 1;
							}
						}

						return succ || p->isTransformed;
					}
					else if ((p->coopActor->IsInKillMove()) && (hash == "PairEnd"_h || hash == "pairedStop"_h))
					{
						// Sometimes, when a killmove fails, the player will remain locked in place
						// because the game still considers them to be in a killmove,
						// so unset the flag here to signal the player's PAM to stop handling the
						// previously triggered killmove and reset the player's data.
						logger::debug("[Character Hooks] NotifyAnimationGraph: {}: Stopping killmove: {}.", p->coopActor->GetName(), a_eventName);
						p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
					}
				}
			}

			return _NotifyAnimationGraph(a_this, a_eventName);
		}

		void CharacterHooks::ResetInventory(RE::Character* a_this, bool a_leveledOnly)
		{
			// Prevent players from resetting their inventory when disabled/resurrected,
			// since a full inventory reset removes items added during the co-op session.
			if (glob.globalDataInit)
			{
				if (glob.allPlayersInit)
				{
					if (GlobalCoopData::IsCoopPlayer(a_this))
					{
						logger::debug("[Character Hooks] ResetInventory: {}, leveled only: {}.", a_this->GetName(), a_leveledOnly);
						return;
					}
				}
			}

			_ResetInventory(a_this, a_leveledOnly);
		}

		void CharacterHooks::Update(RE::Character* a_this, float a_delta)
		{
			auto& cam = glob.cam;
			// Update rotation only when the co-op camera is enabled and the camera is not changing states.
			if (glob.globalDataInit && glob.allPlayersInit && glob.coopSessionActive)
			{
				bool isCoopEntity = glob.coopEntityBlacklistFIDSet.contains(a_this->formID);
				if (!isCoopEntity)
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

					if (!isMountedByPlayer)
					{
						if (auto currentProc = a_this->currentProcess; currentProc && currentProc->middleHigh)
						{
							// Let the game update this character first.
							_Update(a_this, a_delta);
							// [Temp workaround]:
							// Disable Precision on this actor when ragdolled to avoid a ragdoll reset position glitch on knock explosion
							// where the hit actor is teleported to their last ragdoll position instead of staying at their current position.
							// Precision is re-enabled on the actor after they get up.
							if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI3; api)
							{
								const auto handle = a_this->GetHandle();
								if (a_this->IsInRagdollState() && api->IsActorActive(handle))
								{
									api->ToggleDisableActor(handle, true);
								}
								else if (!a_this->IsInRagdollState() && a_this->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal && !api->IsActorActive(handle))
								{
									api->ToggleDisableActor(handle, false);
								}
							}

							// Temporary solution to allies/teammates attacking co-op players, even on accidental hits.
							// Stop combat right away if the current combat target is a co-op player.
							// Will maintain aggro if low on health.
							// Player must sheathe weapons to indicate peaceful intentions if below this threshold.
							float maxHealth = 
							(
								a_this->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) + 
								a_this->GetBaseActorValue(RE::ActorValue::kHealth)
							);
							bool aboveQuarterHealth = a_this->GetActorValue(RE::ActorValue::kHealth) / maxHealth > 0.25f;
							if (aboveQuarterHealth && Util::IsPartyFriendlyActor(a_this))
							{
								a_this->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
								if (a_this->combatController)
								{
									auto combatTarget1 = Util::GetRefrPtrFromHandle(a_this->currentCombatTarget);
									auto combatTarget2 = Util::GetRefrPtrFromHandle(a_this->combatController->targetHandle);
									auto playerActorTarget = 
									( 
										GlobalCoopData::IsCoopPlayer(combatTarget1.get()) ? combatTarget1->As<RE::Actor>() :
										GlobalCoopData::IsCoopPlayer(combatTarget2.get()) ? combatTarget2->As<RE::Actor>() : nullptr
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

							return;
						}
					}
					else
					{
						// Modify mount speed mult when sprinting to maintain speed consistency between P1 and other players' mounts.
						const auto& p = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(rider.get())];
						if (p->pam->IsPerforming(InputAction::kSprint))
						{
							a_this->SetActorValue(RE::ActorValue::kSpeedMult, 120.0f * Settings::fSprintSpeedMult);
							a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
							a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
						}
						else
						{
							a_this->SetActorValue(RE::ActorValue::kSpeedMult, 120.0f);
							a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
							a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
						}
					}
				}
				else if (GlobalCoopData::IsCoopPlayer(a_this))
				{
					// Let the game update the player first.
					_Update(a_this, a_delta);

					const auto& p = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(a_this)];
					// Don't know how to prevent combat from triggering for co-op companion players towards other actors, including other players.
					// Best bandaid solution for now is to remove the combat controller each frame, but
					// combat will still initiate for a frame at most until the controller is cleared here.
					p->coopActor->combatController = nullptr;

					if (auto currentProc = a_this->currentProcess; currentProc)
					{
						if (auto high = currentProc->high; high)
						{
							// Another annoying issue to work around:
							// Since movement speed does not update while the player is ragdolled or getting up,
							// if the player was moving fast before ragdolling, they'll shoot out in their facing direction
							// once they fully get up, until their movement speed normalizes.
							// Do not allow movement until the player's movement speed zeroes out if the player has just fully gotten up.
							// Obviously a better solution would involve finding a way to set movement speed
							// directly to 0 when ragdolled or getting up, but for now, this'll have to do.

							if (p->mm->shouldCurtailMomentum && p->mm->dontMoveSet)
							{
								// Affects how quickly the player slows down.
								// The higher, the faster the reported movement speed becomes zero.
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun] = 100000.0f;
							}
							else if (auto charController = p->coopActor->GetCharController(); charController && p->mm->IsRunning())
							{
								// Set speed mult to refresh current movement type data next frame.
								if (!p->coopActor->IsOnMount())
								{
									p->coopActor->SetActorValue(RE::ActorValue::kSpeedMult, p->mm->movementOffsetParams[!MoveParams::kSpeedMult]);
									p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
									p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
									if (p->mm->movementOffsetParams[!MoveParams::kSpeedMult] < 0.0f)
									{
										logger::critical("[Character Hooks] Update: {}'s speedmult ({}) is < 0. Resetting to base.", p->coopActor->GetName(), p->mm->movementOffsetParams[!MoveParams::kSpeedMult]);
										p->coopActor->SetActorValue(RE::ActorValue::kSpeedMult, p->mm->baseSpeedMult);
										p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
										p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
									}
								}

								// REMOVE when done debugging.
								/*if (p->mm->isDashDodging || p->mm->isParagliding)
								{
									RE::NiPoint3 linVelXY = RE::NiPoint3(charController->outVelocity.quad.m128_f32[0], charController->outVelocity.quad.m128_f32[1], 0.0f);
									RE::NiPoint3 facingDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(p->coopActor->data.angle.z));
									RE::NiPoint3 lsDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(p->mm->movementOffsetParams[!MoveParams::kLSGameAng]));
									glm::vec3 start = ToVec3(p->coopActor->data.location);
									glm::vec3 offsetFacing = ToVec3(facingDir);
									glm::vec3 offsetLSDir = ToVec3(lsDir);
									glm::vec3 offsetVel = ToVec3(linVelXY);
									DebugAPI::QueueArrow3D(start, start + offsetFacing * 50.0f, 0xFF0000FF, 5.0f, 3.0f);
									DebugAPI::QueueArrow3D(start, start + offsetLSDir * 50.0f, 0x00FF00FF, 5.0f, 3.0f);
									DebugAPI::QueueArrow3D(start, start + offsetVel * 50.0f, 0x0000FFFF, 5.0f, 3.0f);
								}*/

								//================
								// Rotation speed.
								//================
								if (p->mm->isDashDodging)
								{
									// No rotation when dodging.
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
									high->currentMovementType.defaultData.rotateWhileMovingRun = 0.0f;
								}
								else if (p->mm->isParagliding)
								{
									// Scale up default rotation rates.
									if (glob.paraglidingMT)
									{
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.rotateWhileMovingRun = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.rotateWhileMovingRun * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
									}
									else
									{
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] = 
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
										(
											70.0f * TO_RADIANS * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.rotateWhileMovingRun = 
										(
											120.0f * TO_RADIANS * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
									}
								}
								else
								{
									// Increase rotation speed since all the movement types' default speeds are too slow when used with KeepOffsetFromActor().
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
									high->currentMovementType.defaultData.rotateWhileMovingRun = 
									(
										Settings::fBaseRotationMult * Settings::fBaseMTRotationMult * PI
									);
								}

								//=================
								// Movement speeds.
								//=================
								// NOTE: Paraglide dodge velocity changes are char controller velocity-based and are not handled here.
								// Simply set the movement type data to the paraglide MT equivalent.
								if (p->mm->isParagliding)
								{
									if (glob.paraglidingMT)
									{
										// Movement speeds.
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun] =
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]
										);
									}
									else
									{
										// Same movement speeds across the board when paragliding.
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		= 700.0f;
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
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		= dodgeSpeed;
								}
								else
								{
									// Out velocity seems to be the intended velocity before collisions are accounted for.
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
									// Yaw difference between the XY velocity direction and the direction in which the player wishes to head.
									float movementToHeadingAngDiff = 
									(
										p->mm->lsMoved ? 
										Util::NormalizeAngToPi(p->mm->movementOffsetParams[!MoveParams::kLSGameAng] - linVelYaw) : 
										0.0f
									);
									// Sets the bounds for the diff factor applied to movement speed below. Dependent on rotation speeds -- rotate faster, pivot faster.
									float range = max(1.0f, (Settings::fBaseMTRotationMult * Settings::fBaseRotationMult) / 4.0f);
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
												9.0f
											)
										)
									);

									// Player must not be sprinting, mounted, downed, animation driven, or running their interaction package.
									if (!p->pam->IsPerforming(InputAction::kSprint) && !p->coopActor->IsOnMount() &&
										!p->mm->isAnimDriven && !p->mm->interactionPackageRunning && !p->isDowned)
									{
										// The core movement problem when using KeepOffsetFromActor() with the player themselves as the target
										// is slow deceleration/acceleration when changing directions rapidly.
										// First noticed that playing the 'SprintStart' animation event right as the player started pivoting
										// caused them to turn and face the new movement direction almost instantly.
										// Increasing the movement type's directional max speed values, depending on how rapidly the player is turning,
										// has the same effect as forcing the player to briefly sprint each time they change directions, which is obviously not as ideal.
										// The bump in movement speed minimizes the 'ice skating' slow-down/speed-up effect, but can still cause rapid bursts of movement at times.
										//
										// NOTE: Base movement type data values seem to only reset to their defaults each frame if the player's speedmult is modified.
										// Otherwise, the diff factor multiplications each frame will accumulate, reaching infinity and preventing the player from moving.
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
									}
								}
							}

							// Prevent automatic armor re-equip.
							high->reEquipArmorTimer = FLT_MAX;
						}

						if (auto midHigh = currentProc->middleHigh; midHigh)
						{
							// Prevent the game from automatically equipping a torch on co-op companions while in dim environments.
							// Seems to attempt torch equip when the timer hits 0 or below.
							midHigh->torchEvaluationTimer = FLT_MAX;

							// If using the revive system and killed by another player, prevent the game from forcing the player into bleedout.
							if (Settings::bUseReviveSystem && Settings::bCanKillmoveOtherPlayers)
							{
								midHigh->deferredKillTimer = FLT_MAX;
							}
						}

						bool inDownedLifeState = {
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
						};
						if ((!p->isDowned) && inDownedLifeState)
						{
							a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
						}
					}

					return;
				}
			}

			_Update(a_this, a_delta);
		}

// [MELEE HIT HOOKS]:
		void MeleeHitHooks::ProcessHit(RE::Actor* a_victim, RE::HitData& a_hitData)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (const auto attacker = Util::GetActorPtrFromHandle(a_hitData.aggressor); attacker)
				{
					auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(a_victim);
					auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(attacker.get());
					// Handle cached melee UseSkill() calls for P1 (always fires before this hook does for melee skills).
					if (auto p1 = RE::PlayerCharacter::GetSingleton(); (p1) && (glob.lastP1MeleeUseSkillCallArgs) && (attacker.get() == p1 || a_victim == p1))
					{
						// Conditions to execute delayed UseSkill call:
						// 1. P1 is the attacker:
						//	- Either must have friendly fire enabled or must not be attacking party-friendly actor.
						// 2. P1 is the victim:
						//	- The attacker is not another player or is a player that has friendly fire enabled.
						bool p1AttackerCanUseSkill = ((attacker.get() == p1) && (Settings::vbFriendlyFire[0] || !Util::IsPartyFriendlyActor(a_victim)));
						bool p1VictimCanUseSkill = ((a_victim == p1) && (playerAttackerIndex == -1 || Settings::vbFriendlyFire[glob.coopPlayers[playerAttackerIndex]->playerID]));
						if (p1AttackerCanUseSkill || p1VictimCanUseSkill)
						{
							p1->UseSkill(glob.lastP1MeleeUseSkillCallArgs->skill, glob.lastP1MeleeUseSkillCallArgs->points, glob.lastP1MeleeUseSkillCallArgs->assocForm);
						}

						glob.lastP1MeleeUseSkillCallArgs.reset();
					}

					// Co-op player was hit; add armor/block XP as needed.
					if (playerVictimIndex != -1)
					{
						const auto& p = glob.coopPlayers[playerVictimIndex];
						// Not player 1 and not attacked by another player or not in god mode and friendly fire enabled.
						const bool canAddXP = {
							!p->isPlayer1 &&
							(playerAttackerIndex == -1 || (!p->isInGodMode && Settings::vbFriendlyFire[glob.coopPlayers[playerAttackerIndex]->playerID]))
						};
						if (canAddXP)
						{
							const auto& armorRatings = p->em->armorRatings;
							float rawDamage = a_hitData.physicalDamage;
							// Block skill.
							if (a_hitData.percentBlocked > 0.0f)
							{
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kBlock, rawDamage * a_hitData.percentBlocked);
							}

							// Armor skills.
							float lightArmorBaseXP = rawDamage * armorRatings.first / ((armorRatings.first + armorRatings.second) == 0.0f ? 1.0f : (armorRatings.first + armorRatings.second));
							float heavyArmorBaseXP = rawDamage * armorRatings.second / ((armorRatings.first + armorRatings.second) == 0.0f ? 1.0f : (armorRatings.first + armorRatings.second));
							if (lightArmorBaseXP > 0.0f)
							{
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kLightArmor, lightArmorBaseXP);
							}

							if (heavyArmorBaseXP > 0.0f)
							{
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kHeavyArmor, heavyArmorBaseXP);
							}
						}
					}
				}
			}

			_ProcessHit(a_victim, a_hitData);
		}

// [MENU CONTROLS HOOKS]:
		EventResult MenuControlsHooks::ProcessEvent(RE::MenuControls* a_this, RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
		{
			if (glob.globalDataInit)
			{
				if (a_event && *a_event)
				{
					bool p1ManagerThreadsInactive =
					{
						!glob.coopSessionActive || !glob.cam->IsRunning() ||
						(glob.coopSessionActive && (!glob.coopPlayers[glob.player1CID]->IsRunning()))
					};

					bool p1InMenu = !p1ManagerThreadsInactive && glob.menuCID == glob.player1CID;
					if (p1InMenu)
					{
						// Check to see if P1 is in the Favorites Menu and is trying to equip a quickslot item or spell.
						CheckForP1QSEquipReq(a_event);
					}
					else if (p1ManagerThreadsInactive && Util::MenusOnlyAlwaysOpenInMap())
					{
						// Delay Journal and Wait binds to trigger their actions on release, rather than on press.
						// In addition, check for pressing of the default binds:
						// Journal + Wait -> Co-op Debug Menu
						// Wait + Journal -> Co-op Summoning Menu binds.
						CheckForMenuTriggeringInput(a_event);
					}

					// Filter out P1 controller inputs that should be ignored while in co-op.
					if (bool shouldProcess = FilterInputEvents(a_event); shouldProcess)
					{
						return _ProcessEvent(a_this, a_event, a_eventSource);
					}
					else
					{
						return EventResult::kContinue;
					}
				}
			}

			return _ProcessEvent(a_this, a_event, a_eventSource);
		}

		void MenuControlsHooks::CheckForMenuTriggeringInput(RE::InputEvent* const* a_constEvent)
		{
			// Check if P1 is trying to open the Summoning/Debug menus.

			auto eventHead = *a_constEvent;
			auto buttonEvent = eventHead->AsButtonEvent(); 
			if (buttonEvent)
			{
				// Only need to handle gamepad button presses.
				if (eventHead->GetDevice() == RE::INPUT_DEVICE::kGamepad)
				{
					auto userEvents = RE::UserEvents::GetSingleton();
					auto controlMap = RE::ControlMap::GetSingleton();
					// Debug: always Journal + Wait when not in co-op.
					// Summon: always Wait + Journal when not in co-op.
					auto journalMask = GAMEPAD_MASK_START;
					auto waitMask = GAMEPAD_MASK_BACK;
					if (userEvents && controlMap)
					{
						journalMask = controlMap->GetMappedKey(userEvents->journal, RE::INPUT_DEVICE::kGamepad);
						waitMask = controlMap->GetMappedKey(userEvents->wait, RE::INPUT_DEVICE::kGamepad);
					}

					// Button events seem to be chained in a manner that does not depend on when their buttons were pressed.
					// Check for both Journal and Wait binds being pressed/held to trigger co-op debug/summoning menus.
					bool journalBindEvent = buttonEvent->idCode == journalMask;
					bool waitBindEvent = buttonEvent->idCode == waitMask;
					if (journalBindEvent || waitBindEvent)
					{
						// Journal/Wait bind pressed by itself.
						if (!eventHead->next)
						{
							journalBindPressedFirst = journalBindEvent;
						}
						else
						{
							if (auto buttonEvent2 = eventHead->next->AsButtonEvent(); buttonEvent2)
							{
								bool journalBindEvent2 = buttonEvent2->idCode == journalMask;
								bool waitBindEvent2 = buttonEvent2->idCode == waitMask;
								bool debugMenuBindPressed = journalBindPressedFirst && ((journalBindEvent && waitBindEvent2) || (journalBindEvent2 && waitBindEvent));
								bool summoningMenuBindPressed = !journalBindPressedFirst && ((waitBindEvent && journalBindEvent2) || (waitBindEvent2 && journalBindEvent));
								// Can trigger the debug menu at any time, even if the camera/p1 manager threads are inactive.
								if (debugMenuBindPressed && !debugMenuTriggered)
								{
									if (glob.player1Actor)
									{
										logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: Debug menu binds pressed but not triggered. Opening menu now.");
										glob.onDebugMenuRequest.SendEvent(glob.player1Actor.get(), glob.coopSessionActive ? glob.player1CID : -1);
									}

									debugMenuTriggered = true;
								}

								if (summoningMenuBindPressed && !summoningMenuTriggered)
								{
									// NOTE: Have to wait until P1 is out of combat to summon other players.
									// Summoning global variable is set to 1 in the summoning menu script, 
									// and set to 0 if summoning failed or is complete.
									if (glob.summoningMenuOpenGlob->value == 0.0f && glob.player1Actor && !glob.player1Actor->IsInCombat())
									{
										logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: Summoning menu binds pressed but not triggered. Opening menu now.");
										glob.onSummoningMenuRequest.SendEvent();
									}

									summoningMenuTriggered = true;
								}

								// Temp solution (not failproof), since I can't find a direct way of linking the controller triggering input events
								// handled here to one of the active controllers by using the XInput API:
								// Check to see which controller is requesting to open the menu and assign its ID as player 1's CID.
								// Heuristic checks the two buttons' event-reported held times against the XInput controller state held times.
								// Will sometimes fail if two players press the same binds at nearly the exact same time (within a couple frames),
								// as the wrong player's CID may be assigned as player 1's CID.
								// Fix by manually assigning the CID through the debug menu.
								if (glob.cdh && glob.player1CID == -1 && (debugMenuTriggered || summoningMenuTriggered))
								{
									int32_t newCID = -1;
									// Choose the controller with the smallest reported held time difference.
									float smallestHeldTimeDiffTotal = FLT_MAX;
									for (uint32_t i = 0; i < ALYSLC_MAX_PLAYER_COUNT; ++i)
									{
										XINPUT_STATE inputState;
										ZeroMemory(&inputState, sizeof(XINPUT_STATE));
										if (XInputGetState(i, std::addressof(inputState)) == ERROR_SUCCESS)
										{
											float eventReportedJournalHoldTime = journalBindEvent ? buttonEvent->heldDownSecs : buttonEvent2->heldDownSecs;
											float eventReportedWaitHoldTime = waitBindEvent ? buttonEvent->heldDownSecs : buttonEvent2->heldDownSecs;
											const auto& inputState1 = glob.cdh->GetInputState(i, debugMenuTriggered ? InputAction::kStart : InputAction::kBack);
											const auto& inputState2 = glob.cdh->GetInputState(i, debugMenuTriggered ? InputAction::kBack : InputAction::kStart);

											// One of the two requisite binds was not pressed on this controller, or the controller state held time(s) are longer than the event-reported held time(s)
											// Note: Event-reported held times are always >= 1 frame delta longer than the controller state held time for P1's controller.
											if (!inputState1.isPressed || !inputState2.isPressed ||
												(debugMenuTriggered && inputState1.heldTimeSecs > eventReportedJournalHoldTime) ||
												(debugMenuTriggered && inputState2.heldTimeSecs > eventReportedWaitHoldTime) ||
												(summoningMenuTriggered && inputState1.heldTimeSecs > eventReportedWaitHoldTime) ||
												(summoningMenuTriggered && inputState2.heldTimeSecs > eventReportedJournalHoldTime))
											{
												continue;
											}

											float heldTimeDiffTotal = FLT_MAX;
											if (debugMenuTriggered)
											{
												heldTimeDiffTotal = abs(eventReportedJournalHoldTime - inputState1.heldTimeSecs) + abs(eventReportedWaitHoldTime - inputState2.heldTimeSecs);
											}
											else
											{
												heldTimeDiffTotal = abs(eventReportedWaitHoldTime - inputState1.heldTimeSecs) + abs(eventReportedJournalHoldTime - inputState2.heldTimeSecs);
											}


											if (heldTimeDiffTotal < smallestHeldTimeDiffTotal)
											{
												smallestHeldTimeDiffTotal = heldTimeDiffTotal;
												newCID = i;
											}
										}
									}

									if (newCID != -1)
									{
										// REMOVE
										logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: Assigning CID {} as P1 CID.", newCID);
										glob.player1CID = newCID;
									}
								}

								// After processing, ignore the second button event entirely, 
								// since we do not want either the journal or wait menus to trigger once
								// both the journal and wait binds are pressed at the same time.
								if (buttonEvent2->IsDown() || buttonEvent2->IsHeld() || buttonEvent2->IsUp())
								{
									buttonEvent2->heldDownSecs = 0.0f;
									buttonEvent2->value = 0.0f;
									buttonEvent2->idCode = 0xFF;
								}
							}
						}

						if (buttonEvent->IsDown() || buttonEvent->IsHeld())
						{
							if (buttonEvent->IsDown())
							{
								ignoringJournalWaitEvent = !Util::MenusOnlyAlwaysOpenInMap();
								logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: {} menu bind is now pressed. Ignore ({}) and trigger on release if no co-op menus are triggered by then.",
									journalBindEvent ? "Journal" : "Wait",
									ignoringJournalWaitEvent);
							}

							// Clear out first button event to prevent it from triggering the Journal or Wait menus
							// while the button is still pressed.
							buttonEvent->heldDownSecs = 0.0f;
							buttonEvent->value = 0.0f;
							buttonEvent->idCode = 0xFF;
						}
						else if (buttonEvent->IsUp())
						{
							if (!summoningMenuTriggered && !debugMenuTriggered && !ignoringJournalWaitEvent && Util::MenusOnlyAlwaysOpenInMap())
							{
								// No co-op menus triggered, so allow the button event to pass through on release
								// and trigger either the Journal or Wait menu as usual.
								logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: {} bind released on its own. Simulating press to trigger {} menu.",
									journalBindEvent ? "Journal" : "Wait", journalBindEvent ? "Pause" : "Wait");
								buttonEvent->heldDownSecs = 0.0f;
								buttonEvent->value = 1.0f;
							}
							else if (summoningMenuTriggered || debugMenuTriggered)
							{
								// A co-op menu was triggered, so ignore the button event on release.
								logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: {} bind released on its own after {} menu triggered. Ignoring.",
									journalBindEvent ? "Journal" : "Wait", summoningMenuTriggered ? "Summoning" : "Debug");
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
			bool needToResetState = summoningMenuTriggered || debugMenuTriggered || ignoringJournalWaitEvent;
			if (buttonInputsReleased && needToResetState)
			{
				logger::debug("[MenuControls Hook] CheckForMenuTriggeringInput: Buttons released. Reset flags to false. Journal bind pressed first: {}, summoning menu triggered: {}, debug menu triggered: {}, ignoring Journal/Wait event: {}",
					journalBindPressedFirst, summoningMenuTriggered, debugMenuTriggered, ignoringJournalWaitEvent);
				journalBindPressedFirst = summoningMenuTriggered = debugMenuTriggered = ignoringJournalWaitEvent = false;
			}
		}

		void MenuControlsHooks::CheckForP1QSEquipReq(RE::InputEvent* const* a_constEvent)
		{
			// Check if P1 is trying to (un)tag a FavoritesMenu entry 
			// as a quick slot item/spell.

			auto inputEvent = *a_constEvent;
			auto idEvent = inputEvent->AsIDEvent();
			auto buttonEvent = inputEvent->AsButtonEvent();
			auto userEvents = RE::UserEvents::GetSingleton();
			if (idEvent && idEvent->userEvent == userEvents->journal &&
				RE::UI::GetSingleton()->IsMenuOpen(RE::FavoritesMenu::MENU_NAME) &&
				buttonEvent->value == 1.0f &&
				buttonEvent->heldDownSecs == 0.0f)
			{
				glob.mim->EquipP1QSForm();
			}
		}

		bool MenuControlsHooks::FilterInputEvents(RE::InputEvent* const* a_constEvent)
		{
			// Check which player sent the input events in the input event chain,
			// and modify individual input events in the chain 
			// to block them from being handled by P1's action handlers, as needed.
			// Return true if the event should be processed by the MenuControls hook.
			// 
			// NOTE: This function is messy, I know.
			// IMPORTANT: InputEvent's 'pad24' member is used to store processing info:
			// 0xC0DAXXXX: event was already filtered and handled here.
			// 0xXXXXC0DA: proxied P1 input sent by this plugin and should be allowed through by this function.
			// 0xXXXXCA11: emulated P1 input sent by another player from the MIM.
			// 0xXXXXDEAD: ignore this input event.

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

			// Should this menu controls handler handle the input event here (or pass it on without processing), and should it propagate the event unchanged.
			bool shouldProcess = true;
			// Non-P1 player controlling menus.
			bool coopPlayerControllingMenus = glob.globalDataInit && glob.mim->managerMenuCID != -1;
			// P1's co-op managers are active.
			bool p1ManagersActive = glob.coopSessionActive && (glob.cam->IsRunning() || glob.coopPlayers[glob.player1CID]->IsRunning());

			// Initial check to prevent quick or console command-triggered saving while co-op companion data is copied to P1.
			// Also recommended that the player disable auto-saving while using this mod to remove any chance
			// of copied data onto P1, such as another player's name or race name, persisting across save games.
			if ((glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) &&
				(Hash(inputEvent->QUserEvent()) == Hash(ue->quicksave) ||
					(Hash(inputEvent->QUserEvent()) == Hash(ue->console))))
			{
				if (idEvent && buttonEvent)
				{
					if (buttonEvent->IsDown())
					{
						if (Hash(inputEvent->QUserEvent()) == Hash(ue->quicksave))
						{
							RE::DebugMessageBox("[ALYSLC] Cannot save while another player's data is copied over to P1.\nPlease ensure all menus are closed or reload an older save.");
						}
						else
						{
							RE::DebugMessageBox("[ALYSLC] Cannot open console while another player's data is copied over to P1.\nPlease ensure all menus are closed or reload an older save.");
						}
					}

					idEvent->userEvent = "BLOCKED";
					buttonEvent->idCode = 0xFF;
					buttonEvent->heldDownSecs = 0.0f;
					buttonEvent->value = 0.0f;
					return false;
				}
			}

			if (ui && ue && p1 && inputEvent && inputEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad)
			{
				bool dialogueMenuOpen = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
				bool lootMenuOpen = ui->IsMenuOpen("LootMenu");
				// Open the container targeted while in the LootMenu.
				bool lootMenuOpenContainer = false;
				bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpenInMap();
				while (inputEvent)
				{
					// Get event sub-types.
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
						// NOTE: Top 2 bytes == 0xC0DA means this event has been processed before.
						if (idEvent->pad24 >> 16 == 0xC0DA)
						{
							idEvent->pad24 = 0;
						}

						// Has a bypass flag indicating that the event was sent by another player.
						// Co-op companion players: 0xCA11
						bool coopPlayerMenuInput = coopPlayerControllingMenus && idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11);

						//======================================
						// Special QuickLoot menu compatibility.
						//======================================
						bool validLootMenuInput = lootMenuOpen;
						if (auto controlMap = RE::ControlMap::GetSingleton(); controlMap && lootMenuOpen && buttonEvent)
						{
							if (buttonEvent->heldDownSecs == 0.0f)
							{
								// Save "Ready Weapon" input, if any, to prepare for giving this player control when opening the selected container while in the QuickLoot menu.
								// IDK why the event name that QuickLoot uses to switch to the ContainerMenu here is an empty string,
								// but hey, check the ID code instead, since that still equals the ID code for the "Ready Weapon" bind.
								if (!lootMenuOpenContainer && buttonEvent->idCode == controlMap->GetMappedKey(ue->readyWeapon, RE::INPUT_DEVICE::kGamepad))
								{
									lootMenuOpenContainer = true;
								}
								else if (coopPlayerControllingMenus && !coopPlayerMenuInput &&
										(buttonEvent->idCode == controlMap->GetMappedKey(ue->accept, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode) ||
										buttonEvent->idCode == controlMap->GetMappedKey(ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode) ||
										buttonEvent->idCode == controlMap->GetMappedKey(ue->xButton, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kItemMenu)))
								{
									// Ignore P1 inputs that would affect the Loot Menu (eg. Take, Take All, or Exit).
									validLootMenuInput = false;
								}
								else if (!coopPlayerControllingMenus && 
										 buttonEvent->idCode == controlMap->GetMappedKey(ue->cancel, RE::INPUT_DEVICE::kGamepad, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode))
								{
									// Have to stop the Tween Menu from opening for P1 here, so swallow the 'Cancel' input, 
									// and instead close the Loot Menu by clearing the crosshair refr.
									// Keeps things consistent with the other players, who also close the LootMenu via clearing the crosshair refr.
									validLootMenuInput = false;
									// Exit menu and relinquish control when cancel bind is pressed.
									if (auto crosshairPickData = RE::CrosshairPickData::GetSingleton(); crosshairPickData)
									{
										// Clears crosshair refr data.
										Util::SendCrosshairEvent(nullptr);
									}
								}
							}
						}
						else if (buttonEvent && coopPlayerControllingMenus && !coopPlayerMenuInput)
						{
							// Change event name to the corresponding gameplay context event name for P1 input events when P1 is not controlling menus.
							// Ensures that menu processing will not occur for these inputs.
							auto p1GameplayContextEvent = controlMap->GetUserEventName(buttonEvent->idCode, RE::INPUT_DEVICE::kGamepad);
							if (Hash(p1GameplayContextEvent) != ""_h)
							{
								idEvent->userEvent = p1GameplayContextEvent;
							}
						}

						//==========================================================================================================================================
						// Two tasks to perform here:
						// 1. Check if the event should be processed once returning from this function.
						// If allowed, the subsequent ProcessEvent() call will allow any open menus to process the chained input events.
						// Otherwise, the event will not be processed by any menus, but can still be processed by handlers further down
						// the propagation chain.
						// 2. Propagate the input event unmodified. If unmodified, all following handlers, such as P1's action handlers,
						// can process the event. Otherwise, after modification, the event will not be processed by any handlers further down the propagation chain.
						//==========================================================================================================================================
						bool isAttackInput = (idEvent->userEvent == ue->leftAttack || idEvent->userEvent == ue->rightAttack);
						bool isBlockedP1Event = false;
						// P1 input event names become the empty string when in the LootMenu. Filter these out as blocked.
						bool isBlockedP1LootMenuEvent = 
						(
							coopPlayerControllingMenus && !coopPlayerMenuInput && Hash(idEvent->userEvent) == ""_h
						);
						bool isGroundedAttackInput = !p1->IsOnMount() && isAttackInput;
						bool isLookInput = idEvent->userEvent == ue->look;
						bool isMoveArmsInput = Settings::bRotateArmsWhenSheathed && isAttackInput && !p1->IsWeaponDrawn();
						bool isMoveInput = idEvent->userEvent == ue->move;
						bool isParaglidingInput = 
						{
							(
								ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider && 
								p1->GetCharController() && 
								p1->GetCharController()->context.currentState == RE::hkpCharacterStateType::kInAir
							) &&
							(idEvent->userEvent == ue->activate || isMoveInput)
						};
						bool isRotateInput = idEvent->userEvent == ue->rotate;
						bool isMountedCamInputEvent = p1->IsOnMount() && (isRotateInput || isLookInput);

						// Can P1 switch the container tab to/from their inventory?
						bool isValidContainerTabSwitch = false;
						if (idEvent->userEvent == ue->wait && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) 
						{
							if (auto containerMenu = ui->GetMenu<RE::ContainerMenu>(); containerMenu && containerMenu.get())
							{
								RE::NiPointer<RE::TESObjectREFR> containerRefr;
								RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle(), containerRefr);
								// If container is not a co-op companion's inventory, the tab switch request is valid.
								if (!GlobalCoopData::IsCoopPlayer(containerRefr)) 
								{
									isValidContainerTabSwitch = true;
								}
								else
								{
									if (auto view = containerMenu->uiMovie; view)
									{
										RE::GFxValue result;
										view->Invoke("_root.Menu_mc.isViewingContainer", std::addressof(result), nullptr, 0);
										bool isViewingContainer = result.GetBool();
										// Only allow a tab switch from P1's inventory back to the co-op companion's inventory.
										if (!isViewingContainer)
										{
											isValidContainerTabSwitch = true;
										}
									}
								}
							}
						}

						if (p1ManagersActive)
						{
							// While P1 is controlled in co-op by its managers,
							// block LootMenu events, attack inputs, arm movement inputs, and mounted cam adjustment events while no menus are open.
							// And always block P1 inputs that involve activating objects, opening menus, equipping favorited items,
							// readying weapons, shouting, sneaking, sprinting, and changing the camera's POV.
							isBlockedP1Event =
							(
								(!isParaglidingInput) &&
								(
									(isBlockedP1LootMenuEvent) ||
									(onlyAlwaysOpen && (isGroundedAttackInput || isMoveArmsInput || isMountedCamInputEvent)) ||
									(idEvent->userEvent == ue->wait && !isValidContainerTabSwitch) ||
									(
										idEvent->userEvent == ue->activate ||
										idEvent->userEvent == ue->favorites ||
										idEvent->userEvent == ue->hotkey1 ||
										idEvent->userEvent == ue->hotkey2 ||
										idEvent->userEvent == ue->journal ||
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
								// Prevent the usual binds from activating objects, opening menus, changing the camera POV, and equipping items
								// while another player is controlling menus.
								isBlockedP1Event = {
									isBlockedP1LootMenuEvent ||
									idEvent->userEvent == ue->activate ||
									idEvent->userEvent == ue->favorites ||
									idEvent->userEvent == ue->journal ||
									idEvent->userEvent == ue->leftEquip ||
									idEvent->userEvent == ue->rightEquip ||
									idEvent->userEvent == ue->togglePOV ||
									idEvent->userEvent == ue->tweenMenu ||
									idEvent->userEvent == ue->wait
								};
							}
							else
							{
								// Prevent the camera from changing POV at all other times.
								isBlockedP1Event = idEvent->userEvent == ue->togglePOV;
							}
						}

						bool isLeftStickInput = idEvent->userEvent == ue->leftStick;
						// Special P1 input cases where the co-op camera is not active.
						if (!p1ManagersActive)
						{
							if (!coopPlayerMenuInput && idEvent)
							{
								if (!isBlockedP1Event)
								{
									// Set bypass flag here to allow P1's action handlers to process the event when the co-op camera is not active.
									if (buttonEvent)
									{
										buttonEvent->pad24 = 0xC0DA;
									}

									// Change 'Left Stick' and 'Rotate' (menu) P1 user events to 'Move' and 'Look' user events (gameplay context)
									// because P1 is not in control of menus and should not affect another player's menu control.
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
							}
						}

						// NOTE: LS and RS inputs are blocked in the Crafting and Dialogue menus.
						// Use the DPad to navigate, which frees up character movement with the LS, and camera rotation with the RS.
						bool blockedAnalogStickInput = 
						{
							(thumbstickEvent || isLeftStickInput || isRotateInput) &&
							(ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) || ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME))
						};
						// Menus which overlay the entire screen or block players.
						bool fullscreenMenuOpen = 
						{ 
							ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
							ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
							ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) 
						};
						// Ignore this input if pad24 == 0xDEAD.
						bool ignoreInput = idEvent && ((idEvent->pad24 & 0xFFFF) == 0xDEAD);
						// Is P1 input event to rotate the lock while another player rotates the pick.
						bool twoPlayerLockpickingP1Input = 
						{
							Settings::bTwoPlayerLockpicking && coopPlayerControllingMenus &&
							!coopPlayerMenuInput && idEvent->userEvent == "RotateLock"sv
						};
						// P1 input that should be processed by MenuControls.
						bool processableP1Event = (!coopPlayerControllingMenus || twoPlayerLockpickingP1Input);
						// Has a bypass flag indicating that the event was sent/allowed through by this plugin.
						// P1: 0xC0DA
						bool proxiedP1Input = idEvent && ((idEvent->pad24 & 0xFFFF) == 0xC0DA);
						// Input event was blocked before, so do not propagate or handle.
						bool wasBlocked = idEvent->userEvent == "BLOCKED" && idEvent->idCode == 0xFF;

						// [FOR CO-OP COMPANIONS]
						// 1. Input event sent by a non-P1 player.
						// 2. P1 not in control of menus.
						// 3. Not an analog stick input while in dialogue/crafting menu.
						// 4. Does not move P1 or the camera: 
						// Either:
						//		a. Full screen menu is open (all players immobile and camera rotation prohibited, so allow through).
						//		-OR-
						//		b. Is not a move/look input (does not move P1 or rotate the default FP/TP camera).
						bool validCoopCompanionInput = 
						{ 
							(coopPlayerMenuInput && coopPlayerControllingMenus && !blockedAnalogStickInput) && 
							(fullscreenMenuOpen || (!isMoveInput && !isLookInput)) 
						};
						
						// [FOR P1]
						// 1. Proxied through with bypass flag.
						// 2. Rotate lock input while another player is attempting to lockpick and two player lockpicking is enabled.
						// 3. i.
						//		Either:
						//		a. Another player is not controlling menus.
						//		-OR-
						//		b. No fullscreen menu is open, and the input event is from P1.
						//	  -AND-
						//    ii.
						//		a. Not an analog stick input while in dialogue/crafting menu.
						//		-AND-
						//		b. Not an explicitly blocked P1 input event.
						bool validP1Input = 
						{ 
							(proxiedP1Input || twoPlayerLockpickingP1Input) ||
							((!isBlockedP1Event && !blockedAnalogStickInput) && (!coopPlayerControllingMenus || (!coopPlayerMenuInput && !fullscreenMenuOpen))) 
						};

						// NOTE:
						// Processing is done directly after this function by MenuControls, which modifies open menus based on the input event(s).
						// Unmodified propagation involves allowing the event through without any modifications that would invalidate it
						// when processed by P1's action handlers.

						// MenuControls event should be processed if the ignore flag is not set 
						// and the event is a valid co-op companion input event or a processable P1 event.
						shouldProcess = (!wasBlocked && !ignoreInput) && (coopPlayerMenuInput || processableP1Event);

						// Propagate the event to P1's action handlers if it shouldn't be ignored 
						// and if it is a valid P1 or co-op player input event.
						bool propagateUnmodifiedEvent = !wasBlocked && !ignoreInput && (validP1Input || validCoopCompanionInput);

						// REMOVE when done debugging.
						/*logger::debug("[MenuControls Hook] FilterInputEvents: Menu, MIM CID: {}, {}, EVENT: {} (0x{:X}, type {}), blocked: {}, co-op player in menus: {}, p1 manager threads active: {} => PROPAGATE: {}, HANDLE: {}, proxied P1 input: {}, coop player menu input: {}, dialogue menu open: {}, is blocked event: {}, valid co-op companion input: {}, valid p1 input: {}, two-player P1 lockpicking input: {}",
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
								// Must also set this event type flag to stop analog stick events being processed by action handlers.
								inputEvent->eventType.set(RE::INPUT_EVENT_TYPE::kDeviceConnect);
							}
						}

						// Set as handled.
						idEvent->pad24 = 0xC0DA0000 + (idEvent->pad24 & 0xFFFF);
					}

					inputEvent = inputEvent->next;
				}

				if (auto pickData = RE::CrosshairPickData::GetSingleton(); ui && ui->IsMenuOpen("LootMenu") && pickData)
				{
					if (glob.coopSessionActive)
					{
						if (glob.cam->IsRunning())
						{
							const auto& p = coopPlayerControllingMenus ? glob.coopPlayers[glob.mim->managerMenuCID] : glob.coopPlayers[glob.player1CID];
							auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
							bool baselineValidity = crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
							// Set the pick data target to the controlling player's crosshair target before opening the container.
							pickData->target = pickData->targetActor = baselineValidity ? p->tm->crosshairRefrHandle : RE::ObjectRefHandle();
							if (lootMenuOpenContainer && baselineValidity && glob.reqQuickLootContainerHandle == crosshairRefrPtr->GetHandle())
							{
								logger::debug("[MenuControls Hooks] FilterInputEvents: About to open {}'s container from the LootMenu for {}.",
									crosshairRefrPtr->GetName(), p->coopActor->GetName());
								// NOTE: Keeping for now if needed.
								// LootMenu closes and reopens before the container menu does, so a LootMenu request is also needed to maintain menu control for this player.
								/*glob.moarm->InsertRequest(p->controllerID, InputAction::kMoveCrosshair, SteadyClock::now(), "LootMenu", crosshairRefrPtr->GetHandle());
								glob.moarm->InsertRequest(p->controllerID, InputAction::kActivate, SteadyClock::now(), RE::ContainerMenu::MENU_NAME, crosshairRefrPtr->GetHandle());*/
							}
						}
					}
				}
			}
			else
			{
				// Reset device connect flag that may have been set before when preventing analog stick inputs from propagating unmodified.
				inputEvent->eventType.reset(RE::INPUT_EVENT_TYPE::kDeviceConnect);
			}

			return shouldProcess;
		}

// NINODE HOOKS
		void NiNodeHooks::UpdateDownwardPass(RE::NiNode* a_this, RE::NiUpdateData& a_data, std::uint32_t a_arg2)
		{
			if (glob.coopSessionActive)
			{
				// Set stored arm/torso rotations for active players before running downward pass.
				// If not done here, and performed in the main loop instead, an intervening function
				// call will undo our changes.
				RestoreSavedNodeOrientation(a_this, a_data);
			}

			_UpdateDownwardPass(a_this, a_data, a_arg2);
		}

		void NiNodeHooks::RestoreSavedNodeOrientation(RE::NiNode* a_node, RE::NiUpdateData& a_data)
		{
			if (glob.coopSessionActive)
			{
				// Return early and minimize calculation time in this func.
				const auto strings = RE::FixedStrings::GetSingleton();
				if (!strings)
				{
					return;
				}

				// Check if a supported node first.
				bool isLeftArmNode = GlobalCoopData::ADJUSTABLE_PLAYER_LEFT_ARM_NODES.contains(a_node->name);
				bool isRightArmNode = GlobalCoopData::ADJUSTABLE_PLAYER_RIGHT_ARM_NODES.contains(a_node->name);
				bool isTorsoNode = GlobalCoopData::ADJUSTABLE_PLAYER_TORSO_NODES.contains(a_node->name);
				if (!isLeftArmNode && !isRightArmNode && !isTorsoNode)
				{
					return;
				}

				if (auto index = GlobalCoopData::GetCoopPlayerIndex(Util::GetRefrFrom3D(a_node)); index == -1)
				{
					return;
				}
				else
				{
					const auto& p = glob.coopPlayers[index];
					auto nodeNameHash = Hash(a_node->name);
					// Torso node orientation restoration first, as the check is the least intensive.
					if (p->mm->nrm->lastSetTorsoLocalRotations.contains(nodeNameHash) && p->mm->nrm->restoreTorsoNodeRotations)
					{
						a_node->local.rotate = p->mm->nrm->lastSetTorsoLocalRotations.at(nodeNameHash);
						return;
					}

					// Arm node orientation restoration.
					// Only updated when weapon is not drawn.
					if (!Settings::bRotateArmsWhenSheathed || p->coopActor->IsWeaponDrawn())
					{
						return;
					}

					if (p->mm->nrm->lastSetArmLocalRotations.contains(nodeNameHash) && p->mm->nrm->restoreArmNodeRotations)
					{
						a_node->local.rotate = p->mm->nrm->lastSetArmLocalRotations.at(nodeNameHash);
						a_data.flags.set(RE::NiUpdateData::Flag::kDirty, RE::NiUpdateData::Flag::kDisableCollision);
					}
				}
			}
		}

// [PLAYER CAMERA TRANSITION STATE HOOKS]: 
		void PlayerCameraTransitionStateHooks::Begin(RE::PlayerCameraTransitionState* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				// Prevent camera transitions to any state that is not the First Person or Third Person states.
				if ((a_this->transitionTo) && (a_this->transitionTo->id != RE::CameraState::kFirstPerson && a_this->transitionTo->id != RE::CameraState::kThirdPerson))
				{
					return;
				}
			}

			_Begin(a_this);
		}

// [PLAYER CHARACTER HOOKS]:
		float PlayerCharacterHooks::CheckClampDamageModifier(RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_delta)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				const auto& p = glob.coopPlayers[glob.player1CID];
				bool hmsActorValue = a_av == RE::ActorValue::kHealth || a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina;
				// Do not modify AVs when no players are being dismissed and
				// the co-op actor is not revived or if an HMS AV is being decreased in god mode.
				bool notDismissingPlayers = !Settings::bUseReviveSystem || glob.livingPlayers == glob.activePlayers;
				if ((notDismissingPlayers) && (!p->isRevived || (hmsActorValue && p->isInGodMode && a_delta < 0.0f)))
				{
					// Prevent arcane fever buildup while in god mode.
					bool arcaneFeverActorValue = ALYSLC::EnderalCompat::g_enderalSSEInstalled && a_av == RE::ActorValue::kLastFlattered;
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
					if (a_av == RE::ActorValue::kHealth && a_delta > 0.0f)
					{
						for (const auto& p : glob.coopPlayers)
						{
							// Check if another player is healing player 1, and if so, give them XP.
							if (!p->isPlayer1 && p->isActive && a_this->GetHandle() == p->tm->GetRangedTargetActor())
							{
								float currentHealth = a_this->GetActorValue(RE::ActorValue::kHealth);
								float currentMaxHealth = a_this->GetBaseActorValue(RE::ActorValue::kHealth) + a_this->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
								float healthDelta = std::clamp(a_delta, 0.0f, currentMaxHealth - currentHealth);
								// Right hand cast of restoration spell that does not target the caster.
								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellRH))
								{
									if (const auto rhSpell = p->em->GetRHSpell(); rhSpell && rhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}

								// Left hand cast of restoration spell that does not target the caster.
								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellLH))
								{
									if (const auto lhSpell = p->em->GetLHSpell(); lhSpell && lhSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}

								// Quick slot cast of restoration spell that does not target the caster.
								if (healthDelta > 0.0f && p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kRestorationSpellQS))
								{
									// Restoration spell that does not target the caster.
									if (p->em->quickSlotSpell && p->em->quickSlotSpell->GetDelivery() != RE::MagicSystem::Delivery::kSelf)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kRestoration, healthDelta);
									}
								}
							}
						}
					}
					else if (a_av == RE::ActorValue::kSpeedMult)
					{
						// Handle weird bug where P1's speed mult becomes negative (can only be reset by "ResetPlayer1" debug option).
						const auto currentSpeedMult = a_this->GetActorValue(RE::ActorValue::kSpeedMult);
						if (currentSpeedMult != p->mm->movementOffsetParams[!MoveParams::kSpeedMult])
						{
							logger::error("[PlayerCharacter Hook] CheckClampDamageModifier: current speed mult ({}, delta {}) does not match movement manager speed mult {}. New delta: {}.",
								currentSpeedMult, a_delta, p->mm->movementOffsetParams[!MoveParams::kSpeedMult], p->mm->movementOffsetParams[!MoveParams::kSpeedMult] - currentSpeedMult);
							return p->mm->movementOffsetParams[!MoveParams::kSpeedMult] - currentSpeedMult;
						}
					}
					else if ((a_delta < 0.0f) && (a_av == RE::ActorValue::kMagicka || a_av == RE::ActorValue::kStamina))
					{
						// Scaled by cost multiplier here instead of in the cost functions in the player action function holder
						// because those cost functions do not affect player 1.
						// Drawback would be not being able to link an action with a specific magicka/stamina reduction, since this function
						// does not provide any context.
						float mult = a_av == RE::ActorValue::kMagicka ? Settings::vfMagickaCostMult[p->playerID] : Settings::vfStaminaCostMult[p->playerID];
						a_delta *= mult;
					}
				}
			}

			return _CheckClampDamageModifier(a_this, a_av, a_delta);
		}

		void PlayerCharacterHooks::DrawWeaponMagicHands(RE::PlayerCharacter* a_this, bool a_draw)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (glob.player1CID >= 0 && glob.player1CID < GlobalCoopData::MAX_PLAYER_COUNT)
				{
					const auto& p = glob.coopPlayers[glob.player1CID];
					// Do not allow the game to automatically sheathe/unsheathe the player actor's weapons/magic.
					// Blocking weapon/magic drawing while transforming crashes the game at times.
					if (p->IsRunning() && a_draw != p->pam->weapMagReadied && !p->isTransformed && !p->isTransforming)
					{
						a_this->drawSheatheSafetyTimer = FLT_MAX;
						return;
					}
				}
			}

			_DrawWeaponMagicHands(a_this, a_draw);
		}

		void PlayerCharacterHooks::HandleHealthDamage(RE::PlayerCharacter* a_this, RE::Actor* a_attacker, float a_damage)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				logger::debug("[PlayerCharacter Hook] HandleHealthDamage: {} damaged P1 for {} damage, pre-processed!",
					a_attacker ? a_attacker->GetName() : "NONE", a_damage);
				auto playerAttackerIndex = GlobalCoopData::GetCoopPlayerIndex(a_attacker);
				float damageMult = Settings::vfDamageReceivedMult[0];
				// Co-op player inflicted health damage on player 1.
				if (playerAttackerIndex != -1)
				{
					const auto& p = glob.coopPlayers[playerAttackerIndex];
					// Check for friendly fire (not from self) and negate damage.
					if (!Settings::vbFriendlyFire[p->playerID] && a_this != p->coopActor.get())
					{
						a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -a_damage);
						return;
					}

					// Modify damage mult.
					if (!p->isPlayer1 && p->pam->attackDamageMultSet && p->pam->reqDamageMult != 1.0f)
					{
						damageMult *= (p->pam->reqDamageMult * Settings::vfDamageDealtMult[p->playerID]);
						// Reset damage multiplier if performing a ranged sneak attack.
						// Melee sneak attacks reset the damage multiplier on attack stop,
						// but I have yet to find a way to check if the player no longer has any active
						// projectiles, so reset the damage multiplier on damaging hit.
						p->pam->ResetAttackDamageMult();
					}
					else
					{
						// Regular attack.
						// P1 can attack themselves and this multiplier will apply as well.
						damageMult *= Settings::vfDamageDealtMult[p->playerID];
					}

					// Add skill XP if P1 is not the attacker and P1 is not in god mode.
					bool p1HitWhileInGodMode = glob.coopPlayers[glob.player1CID]->isInGodMode;
					if (!p->isPlayer1 && !p1HitWhileInGodMode)
					{
						// Check attack source and increment skill XP if needed.
						const auto lhForm = p->em->equippedForms[!EquipIndex::kLeftHand];
						const auto rhForm = p->em->equippedForms[!EquipIndex::kRightHand];
						const auto qsSpellForm = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
						auto addDestructionXP =
							[&p, &a_damage, a_this](RE::TESForm* a_potentialSourceForm) {
								if (a_potentialSourceForm)
								{
									// Is destruction spell.
									if (const auto spell = a_potentialSourceForm->As<RE::SpellItem>(); spell && 
										spell->avEffectSetting && spell->avEffectSetting->data.associatedSkill == RE::ActorValue::kDestruction)
									{
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kDestruction, -a_damage);
									}
								}
							};

						// Check for destruction spell cast from LH/RH/Quick Slot.
						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellLH))
						{
							addDestructionXP(lhForm);
						}

						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellRH))
						{
							addDestructionXP(rhForm);
						}

						if (p->pam->perfSkillIncCombatActions.any(SkillIncCombatActionType::kDestructionSpellQS))
						{
							addDestructionXP(qsSpellForm);
						}
					}
				}

				// Adjust damage based off new damage mult.
				// Ignore direct modifications of HP or no direct attacker (a_attacker == nullptr).
				// NOTE: Unfortunately, this means fall damage (no attacker) will not scale down with the player's damage received mult.
				if (float deltaHealth = a_damage * (damageMult - 1.0f); deltaHealth != 0.0f && a_attacker)
				{
					a_this->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, deltaHealth);

					// REMOVE
					logger::debug("[PlayerCharacter Hook] HandleHealthDamage: {}: Damage received goes from {} to {} (x{}). Health delta: {}. Attacker: {}, new health: {}.",
						a_this->GetName(),
						a_damage,
						a_damage * damageMult,
						damageMult,
						deltaHealth,
						a_attacker ? a_attacker->GetName() : "NONE",
						a_this->GetActorValue(RE::ActorValue::kHealth));

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
						logger::debug("[PlayerCharacter Hook] HandleHealthDamage: Using revive system: {} is downed and needs to be revived!",
							p->coopActor->GetName());
						p->isDowned = true;
						p->isRevived = false;
						p->lastDownedTP = SteadyClock::now();
						p->RequestStateChange(ManagerState::kPaused);

						// Check if all players are downed, and if so, end the co-op session.
						bool playerStillStanding = std::any_of(glob.coopPlayers.begin(), glob.coopPlayers.end(),
							[](const auto& a_p) {
								return a_p->isActive && !a_p->isDowned;
							});
						if (!playerStillStanding)
						{
							logger::debug("[PlayerCharacter Hook] HandleHealthDamage: All players are downed. Ending co-op session.");
							GlobalCoopData::YouDied();
						}
						else
						{
							logger::debug("[PlayerCharacter Hook] HandleHealthDamage: {} is downed and starting downed state countdown.", p->coopActor->GetName());
							// Set health/health regen to 0 to prevent player from getting up prematurely when being revived.
							p->coopActor->SetBaseActorValue(RE::ActorValue::kHealRateMult, 0.0f);
							p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
								RE::ActorValue::kHealth, -p->coopActor->GetActorValue(RE::ActorValue::kHealth));
							// Put in an alive ragdoll state.
							Util::PushActorAway(p->coopActor.get(), p->coopActor->data.location, -1.0f, true);
							// Ensure that the player will stay downed and no death events trigger during the downed state countdown.
							// Certain attacks such as spider venom while co-op data is copied onto P1 seem to reset the essential flag.
							p->pam->SetEssentialForReviveSystem();
							p->taskRunner->AddTask([&p]() { p->DownedStateCountdownTask(); });
							// Already 'dead', no need to continue handling removed health.
							if (a_this->GetActorValue(RE::ActorValue::kHealth) < 0.0f)
							{
								return;
							}
						}
					}
					else
					{
						logger::debug("[PlayerCharacter Hook] HandleHealthDamage: Not using revive system: P1 is dead. Ending co-op session.");
						// If not using the revive system, once one player dies, all other players die and the co-op session ends.
						GlobalCoopData::YouDied(a_this);
					}
				}
			}

			_HandleHealthDamage(a_this, a_attacker, a_damage);
		}

		void PlayerCharacterHooks::ModifyAnimationUpdateData(RE::PlayerCharacter* a_this, RE::BSAnimationUpdateData& a_data)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
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
				}

				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				if (coopP1->mm->isDashDodging)
				{
					a_data.deltaTime *= Settings::fDashDodgeAnimSpeedFactor;
				}
			}

			_ModifyAnimationUpdateData(a_this, a_data);
		}

		bool PlayerCharacterHooks::NotifyAnimationGraph(RE::Character* a_this, const RE::BSFixedString& a_eventName)
		{
			if (glob.globalDataInit && glob.allPlayersInit)
			{
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
					else if (((p->isDowned && !p->isRevived) || (p->coopActor->GetActorValue(RE::ActorValue::kHealth) <= 0.0f)) && hash == "GetUpBegin"_h)
					{
						// Ignore requests to get up when the player is downed and not revived.
						return false;
					}
					else if (p->mm->isRequestingDashDodge && !p->mm->isDashDodging && hash == "SneakStart"_h)
					{
						bool succ = _NotifyAnimationGraph(a_this, a_eventName);
						// SneakStart fails when transformed.
						if (succ || p->isTransformed)
						{
							// REMOVE
							const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
							logger::debug("[PlayerCharacter Hooks] NotifyAnimationGraph: {}: ProcessEvent: Lock: 0x{:X}.", p->coopActor->GetName(), hash);
							{
								std::unique_lock<std::mutex> perfAnimQueueLock(p->pam->avcam->perfAnimQueueMutex);
								logger::debug("[PlayerCharacter Hooks] NotifyAnimationGraph: {}: ProcessEvent: Lock obtained: 0x{:X}.", p->coopActor->GetName(), hash);

								// Queue dodge anim event tag so that this player's player action manager can handle stamina expenditure.
								p->pam->avcam->perfAnimEventsQueue.emplace(std::pair<PerfAnimEventTag, uint16_t>(PerfAnimEventTag::kDodgeStart, p->pam->lastAnimEventID));
								p->pam->lastAnimEventID = p->pam->lastAnimEventID == UINT16_MAX ? 1 : p->pam->lastAnimEventID + 1;
							}
						}

						return succ;
					}
					else if ((p->coopActor->IsInKillMove()) && (hash == "PairEnd"_h || hash == "pairedStop"_h))
					{
						// Sometimes, when a killmove fails, the player will remain locked in place
						// because the game still considers them to be in a killmove,
						// so unset the flag here to signal the player's PAM to stop handling the
						// previously triggered killmove and reset the player's data.
						logger::debug("[PlayerCharacter Hooks] NotifyAnimationGraph: {}: Stopping killmove: {}.", p->coopActor->GetName(), a_eventName);
						p->coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kIsInKillMove);
					}
					else if (!p->isTransformed && hash == "BiteStart"_h)
					{
						logger::debug("[PlayerCharacter Hooks] NotifyAnimationGraph: {}: Vampire bite killmove triggered by another player: {}. Let animation event through.", p->coopActor->GetName(), a_eventName);
						return true;
					}
				}
				// Failsafe to ensure that P1 is dead and the game reloads.
				else if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
				{
					// Ensure P1 is dead if all other players are dead after the co-op session ends.
					// Ignore request to get up while downed.
					if (glob.livingPlayers == 0 && hash == "GetUpBegin"_h)
					{
						// First, make sure the essential flag is unset.
						Util::NativeFunctions::SetActorBaseDataFlag(p1->GetActorBase(), RE::ACTOR_BASE_DATA::Flag::kEssential, false);

						p1->KillImpl(p1, FLT_MAX, false, false);
						p1->KillImmediate();
						p1->SetLifeState(RE::ACTOR_LIFE_STATE::kDead);

						// Kill calls fail on P1 at times, especially when the player dies in water, and the game will not reload.
						// The kill console command appears to work when this happens, so as an extra layer of insurance, run that command here.
						const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
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
			}

			return _NotifyAnimationGraph(a_this, a_eventName);
		}

		void PlayerCharacterHooks::Update(RE::PlayerCharacter* a_this, float a_delta)
		{
			if (glob.globalDataInit && glob.allPlayersInit)
			{
				if (glob.cam->IsRunning())
				{
					// Run game's update first.
					_Update(a_this, a_delta);
					// Prevent game from updating crosshair text while co-op cam is active.
					a_this->playerFlags.shouldUpdateCrosshair = false;

					const auto& p = glob.coopPlayers[glob.player1CID];
					if (auto currentProc = a_this->currentProcess; currentProc)
					{
						if (auto high = currentProc->high; high)
						{
							// Another annoying issue to work around after modifying movement speeds:
							// Since movement speed does not update while the player is ragdolled or getting up,
							// if the player was moving fast before ragdolling, they'll shoot out in their facing direction
							// once they fully get up, until their movement speed normalizes.
							// Do not allow movement until the player's movement speed zeroes out if the player has just fully gotten up.
							// Obviously a better solution would involve finding a way to set movement speed
							// directly to 0 when ragdolled or getting up, but for now, this'll have to do.
							
							if (p->mm->shouldCurtailMomentum && p->mm->dontMoveSet)
							{
								// Affects how quickly the player slows down.
								// The higher, the faster the reported movement speed becomes zero.
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
								high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk] = 100000.0f;
							}
							else if (auto charController = p->coopActor->GetCharController(); charController && p->mm->IsRunning())
							{
								// Set speed mult to refresh current movement type data next frame.
								if (!p->coopActor->IsOnMount())
								{
									p->coopActor->SetActorValue(RE::ActorValue::kSpeedMult, p->mm->movementOffsetParams[!MoveParams::kSpeedMult]);
									p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
									p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
									if (p->mm->movementOffsetParams[!MoveParams::kSpeedMult] < 0.0f)
									{
										logger::critical("[PlayerCharacter Hooks] Update: {}'s speedmult ({}) is < 0. Resetting to base.", p->coopActor->GetName(), p->mm->movementOffsetParams[!MoveParams::kSpeedMult]);
										p->coopActor->SetActorValue(RE::ActorValue::kSpeedMult, p->mm->baseSpeedMult);
										p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
										p->coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);
									}
								}

								// REMOVE when done debugging.
								//if (p->mm->isDashDodging || p->mm->isParagliding) 
								{
									RE::NiPoint3 linVelXY = RE::NiPoint3(charController->outVelocity.quad.m128_f32[0], charController->outVelocity.quad.m128_f32[1], 0.0f);
									RE::NiPoint3 facingDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(p->coopActor->data.angle.z));
									RE::NiPoint3 lsDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(p->mm->movementOffsetParams[!MoveParams::kLSGameAng]));
									glm::vec3 start = ToVec3(p->coopActor->data.location);
									glm::vec3 offsetFacing = ToVec3(facingDir);
									glm::vec3 offsetLSDir = ToVec3(lsDir);
									glm::vec3 offsetVel = ToVec3(linVelXY);
									DebugAPI::QueueArrow3D(start, start + offsetFacing * 50.0f, 0xFF0000FF, 5.0f, 3.0f);
									DebugAPI::QueueArrow3D(start, start + offsetLSDir * 50.0f, 0x00FF00FF, 5.0f, 3.0f);
									DebugAPI::QueueArrow3D(start, start + offsetVel * 50.0f, 0x0000FFFF, 5.0f, 3.0f);
								}

								//================
								// Rotation speed.
								//================
								if (p->mm->isDashDodging)
								{
									// No rotation when dodging.
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
									high->currentMovementType.defaultData.rotateWhileMovingRun = 0.0f;
								}
								else if (p->mm->isParagliding)
								{
									// Scale up default rotation rates.
									if (glob.paraglidingMT)
									{
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.rotateWhileMovingRun =
										(
											glob.paraglidingMT->movementTypeData.defaultData.rotateWhileMovingRun * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
									}
									else
									{
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
										(
											70.0f * TO_RADIANS * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
										high->currentMovementType.defaultData.rotateWhileMovingRun =
										(
											120.0f * TO_RADIANS * Settings::fBaseRotationMult * Settings::fBaseMTRotationMult
										);
									}
								}
								else
								{
									// Increase rotation speed since all the movement types' default speeds are too slow when used with KeepOffsetFromActor().
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk] =
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun] =
									high->currentMovementType.defaultData.rotateWhileMovingRun =
									(
										Settings::fBaseRotationMult * Settings::fBaseMTRotationMult * PI
									);
								}

								//=================
								// Movement speeds.
								//=================
								// NOTE: Paraglide dodge velocity changes are char controller velocity-based and are not handled here.
								// Simply set the movement type data to the paraglide MT equivalent.
								if (p->mm->isParagliding)
								{
									if (glob.paraglidingMT)
									{
										// Movement speeds.
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk] = 
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]
										);
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun] =
										(
											glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]
										);
									}
									else
									{
										// Same movement speeds across the board when paragliding.
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		=
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		= 700.0f;
									}
								}
								else if (p->mm->isDashDodging)
								{
									// Interpolate between the starting and ending speedmult values.
									float dodgeSpeed = Util::InterpolateEaseInEaseOut(
										Settings::fMaxDashDodgeSpeedmult,
										Settings::fMinDashDodgeSpeedmult,
										p->mm->dashDodgeCompletionRatio,
										2.0f
									);

									// Same speed across the board when dodging.
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		=
									high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		= dodgeSpeed;
								}
								else if (bool isAIDriven = p->coopActor->movementController && !p->coopActor->movementController->unk1C5; isAIDriven)
								{
									// REMOVE when done debugging.
									/*logger::debug("[PlayerCharacter Hooks] Update: {}: BEFORE: MT data: unkF8: {}, unkFC: {}, unk100: {}, unk104: {}, unk108: {}, unk10C: {}, unk110: {}, unk114: {}, unk118: {}, unk11C: {}, unk120: {}.",
										p->coopActor->GetName(),
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.rotateWhileMovingRun);*/

									RE::NiPoint3 linVelXY = RE::NiPoint3();
									float movementToHeadingAngDiff = -1.0f;
									float range = -1.0f;
									float diffFactor = -1.0f;
									// Player must not be sprinting, mounted, downed, animation driven, or running their interaction package.
									if (!p->pam->IsPerforming(InputAction::kSprint) && !p->coopActor->IsOnMount() && 
										!p->mm->isAnimDriven && !p->mm->interactionPackageRunning && !p->isDowned)
									{
										// The core movement problem when using KeepOffsetFromActor() with the player themselves as the target
										// is slow deceleration/acceleration when changing directions rapidly.
										// First noticed that playing the 'SprintStart' animation event right as the player started pivoting
										// caused them to turn and face the new movement direction almost instantly.
										// Increasing the movement type's directional max speed values, depending on how rapidly the player is turning,
										// has the same effect as forcing the player to briefly sprint each time they change directions, which is obviously not as ideal.
										// The bump in movement speed minimizes the 'ice skating' slow-down/speed-up effect, but can still cause rapid bursts of movement at times.
										//
										// NOTE: Base movement type data values seem to only reset to their defaults each frame if the player's speedmult is modified.
										// Otherwise, the diff factor multiplications each frame will accumulate, reaching infinity and preventing the player from moving.

										// Out velocity seems to be the intended velocity before collisions are accounted for.
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
										// Yaw difference between the XY velocity direction and the direction in which the player wishes to head.
										movementToHeadingAngDiff = 
										(
											p->mm->lsMoved ? 
											Util::NormalizeAngToPi(p->mm->movementOffsetParams[!MoveParams::kLSGameAng] - linVelYaw) : 
											0.0f
										);
										// Sets the bounds for the diff factor applied to movement speed below. Dependent on rotation speeds -- rotate faster, pivot faster.
										range = max(1.0f, (Settings::fBaseMTRotationMult * Settings::fBaseRotationMult) / 4.0f);
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
													9.0f
												)
											)
										);

										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]	*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk]		*= diffFactor;
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun]		*= diffFactor;
									}

									/*logger::debug("[PlayerCharacter Hooks] Update: {}: AFTER: MT data: unkF8: {}, unkFC: {}, unk100: {}, unk104: {}, unk108: {}, unk10C: {}, unk110: {}, unk114: {}, unk118: {}, unk11C: {}, unk120: {}. DIFF FACTOR: {}, lin vel XY: {}, {}, movement ang diff: {}, range: {}.",
										p->coopActor->GetName(),
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kLeft][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRight][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kBack][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kWalk],
										high->currentMovementType.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kRotations][RE::Movement::MaxSpeeds::kRun],
										high->currentMovementType.defaultData.rotateWhileMovingRun,
										diffFactor,
										linVelXY.x,
										linVelXY.y,
										movementToHeadingAngDiff,
										range);*/
								}
							}

							// Not sure if this affects P1, but max out to prevent armor re-equip.
							high->reEquipArmorTimer = FLT_MAX;
						}

						if (auto midHigh = currentProc->middleHigh; midHigh)
						{
							if (Settings::bUseReviveSystem && Settings::bCanRevivePlayer1 && Settings::bCanKillmoveOtherPlayers)
							{
								// If using the revive system and killed by another player, prevent game from forcing the player into a bleedout state.
								midHigh->deferredKillTimer = FLT_MAX;
							}
						}

						// Make sure player is set to alive if not downed.
						bool inDownedLifeState = 
						{
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kBleedout ||
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kEssentialDown ||
							a_this->GetLifeState() == RE::ACTOR_LIFE_STATE::kUnconcious
						};
						if ((!p->isDowned) && inDownedLifeState)
						{
							logger::debug("[PlayerCharacter Hooks] Update: life state is {}. Setting to alive ({}).", a_this->GetLifeState(), RE::ACTOR_LIFE_STATE::kAlive);
							a_this->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
						}
					}

					return;
				}
				else
				{
					a_this->playerFlags.shouldUpdateCrosshair = true;
				}
			}

			_Update(a_this, a_delta);
		}

		void PlayerCharacterHooks::UseSkill(RE::PlayerCharacter* a_this, RE::ActorValue a_av, float a_points, RE::TESForm* a_arg3)
		{
			// NOTE: For melee-related skills, eg. OneHanded/TwoHanded/Archery/HeavyArmor/LightArmor/Block,
			// this call fires before the corresponding hit event(s) are propagated and HandleHealthDamage() call(s) are fired,
			// so we cache the results and discard the first call, which we'll delay until friendly fire processing occurs in the Melee Hit hook.
			if (glob.globalDataInit && glob.allPlayersInit && glob.coopSessionActive)
			{
				const auto& p = glob.coopPlayers[glob.player1CID];
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("[PlayerCharacter Hooks] UseSkill: Lock: 0x{:X}.", hash);
				{
					std::unique_lock<std::mutex> lock(glob.p1SkillXPMutex, std::try_to_lock);
					if (lock)
					{
						// REMOVE
						logger::debug("[PlayerCharacter Hooks] UseSkill: Lock obtained: 0x{:X}.", hash);
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
								logger::debug("[PlayerCharacter Hooks] UseSkill: first call points negated: ({}, {}, {}).", Util::GetActorValueName(a_av), a_points, a_arg3 ? a_arg3->GetName() : "NONE");
								glob.lastP1MeleeUseSkillCallArgs = std::make_unique<GlobalCoopData::LastP1MeleeUseSkillCallArgs>(a_av, a_points, a_arg3);
								a_points = 0.0f;
								return _UseSkill(a_this, a_av, a_points, a_arg3);
							}
							else
							{
								logger::debug("[PlayerCharacter Hooks] UseSkill: second call allowed through: ({}, {}, {}).", Util::GetActorValueName(a_av), a_points, a_arg3 ? a_arg3->GetName() : "NONE");
							}
						}

						logger::debug("[PlayerCharacter Hooks] UseSkill: {}, points before: {}.", a_av, a_points);
						if (GlobalCoopData::SHARED_SKILL_AVS_SET.contains(a_av))
						{
							float mult = 1.0f;
							// Shared skills XP is usually received while in a menu (e.g. lockpicking/pickpocketing/smithing),
							// so apply the menu-controlling player's skill XP multiplier.
							if (glob.menuCID != -1)
							{
								mult = Settings::vfSkillXPMult[glob.coopPlayers[glob.menuCID]->playerID];
							}
							else
							{
								// Scale by average of all players' skill XP multipliers if no player is controlling menus.
								if (glob.activePlayers != 0.0f)
								{
									mult = 0.0f;
									for (const auto& p : glob.coopPlayers)
									{
										if (p->isActive)
										{
											mult += Settings::vfSkillXPMult[p->playerID];
										}
									}

									mult /= glob.activePlayers;
								}
							}

							a_points *= mult;
						}
						else
						{
							// Use player 1's skill XP mult for non-shared skills.
							a_points *= Settings::vfSkillXPMult[p->playerID];
						}
					}
					else
					{
						// REMOVE
						logger::debug("[PlayerCharacter Hooks] UseSkill: Failed to obtain lock: 0x{:X}. Will not use skill.", hash);
						return;
					}
				}
			}

			return _UseSkill(a_this, a_av, a_points, a_arg3);
		}

		
// [PROJECTILE HOOKS]:
		void ProjectileHooks::GetLinearVelocity(RE::Projectile* a_this, RE::NiPoint3& a_velocity)
		{
			// Not handled outside of co-op.
			if (!a_this || !glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				// Output the game's set velocity.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_GetLinearVelocity(a_this, a_velocity);
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

			bool justReleased = a_this->livingTime == 0.0f;
			int32_t firingPlayerIndex = -1;
			bool firedAtPlayer = false;
			GetFiredAtOrByPlayer(a_this, firingPlayerIndex, firedAtPlayer);
			// Restore our linear velocity if fired by the player.
			if (firingPlayerIndex != -1)
			{
				const auto& p = glob.coopPlayers[firingPlayerIndex];
				// Have to insert as managed on release if this hook was run before the UpdateImpl() hook.
				if (justReleased && !p->tm->managedProjHandler->IsManaged(a_this))
				{
					// Overwrite the projectile's velocity and angular orientation.
					DirectProjectileAtTarget(glob.coopPlayers[firingPlayerIndex], a_this, a_this->linearVelocity, justReleased);
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
			if (!a_this || !glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				// Run the game's update function.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_UpdateImpl(a_this, a_delta);
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

			bool justReleased = a_this->livingTime == 0.0f;
			int32_t firingPlayerIndex = -1;
			bool firedAtPlayer = false;
			GetFiredAtOrByPlayer(a_this, firingPlayerIndex, firedAtPlayer);
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
				(justReleased || glob.coopPlayers[firingPlayerIndex]->tm->managedProjHandler->IsManaged(a_this))
			};
			if (shouldAdjustTrajectory)
			{
				// Run the game's update function first.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_UpdateImpl(a_this, a_delta);
				}
				else
				{
					_Projectile_UpdateImpl(a_this, a_delta);
				}

				// Overwrite the projectile's velocity and angular orientation.
				DirectProjectileAtTarget(glob.coopPlayers[firingPlayerIndex], a_this, a_this->linearVelocity, justReleased);
			}
			else
			{
				// Maintain constant XY velocity when no longer handled.
				auto savedVel = a_this->linearVelocity;
				// Run the game's update function.
				if (a_this->As<RE::ArrowProjectile>())
				{
					_ArrowProjectile_UpdateImpl(a_this, a_delta);
				}
				else if (a_this->As<RE::MissileProjectile>())
				{
					_MissileProjectile_UpdateImpl(a_this, a_delta);
				}
				else
				{
					_Projectile_UpdateImpl(a_this, a_delta);
				}

				// Restore saved XY velocity.
				a_this->linearVelocity.x = savedVel.x;
				a_this->linearVelocity.y = savedVel.y;
			}
		}

		void ProjectileHooks::DirectProjectileAtTarget(const std::shared_ptr<CoopPlayer>& a_p, RE::Projectile* a_projectile, RE::NiPoint3& a_resultingVelocity, const bool& a_justReleased)
		{
			if (!a_projectile || !glob.globalDataInit || !glob.allPlayersInit || !glob.coopSessionActive)
			{
				return;
			}
	
			bool isManaged = a_p->tm->managedProjHandler->IsManaged(a_projectile);
			// Remove inactive/invalid managed projectiles first.
			// Remove if invalid, not loaded, deleted, marked for deletion, has collided, limited, or just released.
			bool shouldRemove = 
			{
				(isManaged) &&
				(
					!a_projectile->Is3DLoaded() || 
					a_projectile->IsDeleted() || a_projectile->IsMarkedForDeletion() ||
					!a_projectile->impacts.empty() || a_projectile->ShouldBeLimited()
				)
			};

			// Sometimes the game re-uses the same FID for consecutive projectiles
			// and the projectile FID might still be tagged as managed,
			// so we remove it here before re-inserting below.
			if (shouldRemove)
			{
				a_p->tm->managedProjHandler->Remove(a_projectile);
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
				bool targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
				// Set desired target if needed.
				if (targetActorValidity)
				{
					a_projectile->desiredTarget = targetActorHandle;
				}

				// Set launch pitch/yaw for arrows/bolts.
				// Maintain visual consistency by angling the just-released projectile in the player's attack source direction.
				if (a_projectile->ammoSource)
				{
					float drawnPitch = Util::DirectionToGameAngPitch(a_p->mm->playerAttackSourceDir);
					float drawnYaw = Util::DirectionToGameAngYaw(a_p->mm->playerAttackSourceDir);
					a_projectile->data.angle.x = drawnPitch;
					a_projectile->data.angle.z = drawnYaw;
					if (auto current3D = Util::GetRefr3D(a_projectile); current3D)
					{
						Util::SetRotationMatrix(current3D->local.rotate, a_projectile->data.angle.x, a_projectile->data.angle.z);
					}
				}

				auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
				bool crosshairRefrValidity = crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
				// Actor targeted (aim correction or otherwise), should face crosshair position (never true while mounted), or mounted and targeting an object.
				bool adjustTowardsTarget = 
				{
					(targetActorPtr != a_p->coopActor) &&
					((targetActorValidity || a_p->mm->shouldFaceTarget) || (a_p->coopActor->IsOnMount() && crosshairRefrValidity))
				};
				bool useHoming = adjustTowardsTarget && Settings::vuProjectileTrajectoryType[a_p->playerID] == !ProjectileTrajType::kHoming;
				bool useAimPrediction = adjustTowardsTarget && Settings::vuProjectileTrajectoryType[a_p->playerID] == !ProjectileTrajType::kPrediction;
				if (useHoming)
				{
					logger::debug("[Projectile Hook] DirectProjectileAtTarget: {}: Insert homing projectile.", a_p->coopActor->GetName());
					a_p->tm->managedProjHandler->Insert(a_p, a_projectile, a_resultingVelocity, ProjectileTrajType::kHoming);
				}
				else if (useAimPrediction)
				{
					logger::debug("[Projectile Hook] DirectProjectileAtTarget: {}: Insert aim prediction projectile.", a_p->coopActor->GetName());
					a_p->tm->managedProjHandler->Insert(a_p, a_projectile, a_resultingVelocity, ProjectileTrajType::kPrediction);
				}
				else
				{
					logger::debug("[Projectile Hook] DirectProjectileAtTarget: {}: Insert aim direction projectile.", a_p->coopActor->GetName());
					a_p->tm->managedProjHandler->Insert(a_p, a_projectile, a_resultingVelocity, ProjectileTrajType::kAimDirection);
				}

				// Set linear velocity field to the launch velocity.
				a_projectile->linearVelocity = a_resultingVelocity;
			}

			// If the projectile is managed, direct it at the target.
			isManaged = a_p->tm->managedProjHandler->IsManaged(a_projectile);
			if (isManaged)
			{
				if (const auto& projTrajType = a_p->tm->managedProjHandler->GetInfo(a_projectile)->trajType; projTrajType == ProjectileTrajType::kHoming)
				{
					// Start homing in on the target once the trajectory apex is reached.
					SetHomingTrajectory(a_p, a_projectile, a_resultingVelocity);
				}
				else
				{
					// Release the projectile along a pre-computed trajectory that terminates at a calculated
					// target intercept position (aim prediction) or at a position far away in the player's aiming direction (aim direction).
					SetFixedTrajectory(a_p, a_projectile, a_resultingVelocity);
				}
			}
		}

		void ProjectileHooks::GetFiredAtOrByPlayer(RE::Projectile* a_projectile, int32_t& a_firingPlayerCIDOut, bool& a_firedAtPlayerOut)
		{
			// Store player index (CID) of the player that released this projectile in one out param.
			// -1 if not released by a player.
			// Also store whether or not the projectile was fired at a player in the other out param.

			// Default to not fired by a player or at a player.
			a_firingPlayerCIDOut = -1;
			a_firedAtPlayerOut = false;
			// Return early if the projectile is not valid.
			if (!a_projectile || !a_projectile->actorCause || !a_projectile->actorCause.get())
			{
				return;
			}

			auto firingActorHandle = a_projectile->actorCause && a_projectile->actorCause.get() ? a_projectile->actorCause->actor : RE::ActorHandle();
			auto firingActorPtr = Util::GetActorPtrFromHandle(firingActorHandle);
			auto firingRefrHandle = a_projectile->shooter;
			for (const auto& p : glob.coopPlayers) 
			{
				if (p->isActive && p->IsRunning()) 
				{
					auto playerHandle = p->coopActor->GetHandle();
					// Fired by a player if the firing actor/refr is a player.
					if (firingActorHandle == playerHandle || firingRefrHandle == playerHandle) 
					{
						a_firingPlayerCIDOut = p->controllerID;
					}

					// Fired at a player if the projectile's desired target
					// or the firing actor's combat target is a player.
					bool firedAtPlayer = {
						(GlobalCoopData::IsCoopPlayer(a_projectile->desiredTarget)) ||
						(firingActorPtr && Util::HandleIsValid(firingActorPtr->currentCombatTarget) &&
						 GlobalCoopData::IsCoopPlayer(firingActorPtr->currentCombatTarget))
					};
					if (firedAtPlayer) 
					{
						a_firedAtPlayerOut = true;
					}
				}
			}
		}

		void ProjectileHooks::SetHomingTrajectory(const std::shared_ptr<CoopPlayer>& a_p, RE::Projectile* a_projectile, RE::NiPoint3& a_resultingVelocity)
		{
			// Not a managed projectile, so nothing to do here.
			if (!a_p->tm->managedProjHandler->IsManaged(a_projectile))
			{
				return;
			}

			// Guaranteed to be managed here.
			auto& managedProjInfo = a_p->tm->managedProjHandler->GetInfo(a_projectile);
			RE::NiPoint3 velToSet = a_resultingVelocity;
			RE::NiPoint3 aimTargetPos = a_p->tm->crosshairWorldPos;
			auto targetActorPtr = Util::GetActorPtrFromHandle(managedProjInfo->targetActorHandle);
			bool targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
			bool crosshairRefrIsTarget = 
			{
				crosshairRefrPtr && 
				crosshairRefrPtr == targetActorPtr && 
				Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
			};
			if (targetActorValidity)
			{
				// Using aim correction and a targeted actor node was selected.
				if (Settings::vbUseAimCorrection[a_p->playerID] && managedProjInfo->targetedActorNode)
				{
					aimTargetPos = managedProjInfo->targetedActorNode->world.translate;
					if (auto hkpRigidBody = Util::GethkpRigidBody(managedProjInfo->targetedActorNode.get()); hkpRigidBody)
					{
						// Direct at node's center of mass.
						// NOTE: Projectile is not guaranteed to collide with this node, unless it is within the actor's character collider.
						aimTargetPos = ToNiPoint3(hkpRigidBody->motion.motionState.sweptTransform.centerOfMass0) * HAVOK_TO_GAME;
					}
				}
				else if (crosshairRefrIsTarget)
				{
					// Targeted with crosshair.
					// Direct at crosshair position offset from the target actor.
					aimTargetPos = targetActorPtr->data.location + a_p->tm->crosshairLastHitLocalPos;
				}
				else
				{
					// Aim correction target. Aim at the target's torso position.
					aimTargetPos = Util::GetTorsoPosition(targetActorPtr.get());
				}
			}

			// Pitch and yaw to the target.
			float pitchToTarget = Util::GetPitchBetweenPositions(a_projectile->data.location, aimTargetPos);
			float yawToTarget = Util::GetYawBetweenPositions(a_projectile->data.location, aimTargetPos);
			// Predicted time to target along the initial fixed trajectory.
			const float& initialTimeToTargetSecs = managedProjInfo->initialTrajTimeToTarget;

			// Immediately direct at the target position
			// if the initial time to hit the target is less than one frame.
			// Won't have multiple frames to direct it at the target,
			// so do it here right after release.
			if (initialTimeToTargetSecs <= *g_deltaTimeRealTime) 
			{
				// Set refr data angles.
				a_projectile->data.angle.x = pitchToTarget;
				a_projectile->data.angle.z = yawToTarget;
				// Set rotation matrix to maintain consistency with the previously set refr data angles.
				if (auto current3D = Util::GetRefr3D(a_projectile); current3D)
				{
					Util::SetRotationMatrix(current3D->local.rotate, a_projectile->data.angle.x, a_projectile->data.angle.z);
				}

				// Set velocity.
				auto velToSet = Util::RotationToDirectionVect(-pitchToTarget, Util::ConvertAngle(yawToTarget)) * a_resultingVelocity.Length();
				a_resultingVelocity = velToSet;
				a_projectile->linearVelocity = a_resultingVelocity;

				// No longer handled after directing at the target position.
				a_p->tm->managedProjHandler->Remove(a_projectile);
				return;
			}

			float distToTarget = a_projectile->data.location.GetDistance(aimTargetPos);
			// Continue setting the trajectory while the projectile is > 1 length away from the target.
			bool continueSettingTrajectory = distToTarget > max(1.0f, a_projectile->GetHeight());
			// Exit early if too close to the target.
			if (!continueSettingTrajectory)
			{
				// Set as no longer managed to prevent further trajectory adjustment.
				a_p->tm->managedProjHandler->Remove(a_projectile);
				return;
			}

			const RE::NiPoint3& releasePos = managedProjInfo->releasePos;
			const float& launchPitch = managedProjInfo->launchPitch;
			const float& launchYaw = managedProjInfo->launchYaw;
			float currentPitch = Util::NormalizeAngToPi(Util::DirectionToGameAngPitch(a_resultingVelocity));
			float currentYaw = Util::NormalizeAng0To2Pi(Util::DirectionToGameAngYaw(a_resultingVelocity));
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
			// Just launched, so set pitch and yaw to saved launch values.
			if (a_projectile->livingTime == 0.0f)
			{
				currentPitch = pitchToSet = -launchPitch;
				currentYaw = yawToSet = Util::ConvertAngle(launchYaw);
			}

			// Pitch and yaw angle deltas in order to face the target.
			float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - currentPitch);
			float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentYaw);

			// Trajectory data.
			const float& g = managedProjInfo->g;
			const double& mu = managedProjInfo->mu;
			const float& t = a_projectile->livingTime;

			// Release speed for fixed trajectory determined by projectile launch data.
			const float& releaseSpeed = managedProjInfo->releaseSpeed;
			// Cap in-flight time.
			const bool tooLongToReach = initialTimeToTargetSecs >= Settings::fMaxProjAirborneSecsToTarget;

			// Fixed trajectory XY position and pitch along the trajectory (tangent line).
			const float xy = releaseSpeed * t * cosf(launchPitch);
			const float pitchOnTraj = -atan2(tanf(launchPitch) - (g * xy) / powf(releaseSpeed * cosf(launchPitch), 2.0f), 1.0f);
			if (!managedProjInfo->startedHomingIn)
			{
				// With air resistance.
				/*
				float vx0 = releaseSpeed * cosf(launchPitch);
				float vy0 = releaseSpeed * sinf(launchPitch);
				// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
				float xyD = (vx0 / mu) * (1 - exp(-mu * t));
				//float zD = (-g * t / mu) + (1.0f / mu) * (vy0 + g / mu) * (1.0f - exp(-mu * t));
				// Flip of game's pitch sign.
				const float pitchOnTraj = atan2((g / (mu * releaseSpeed * cosf(launchPitch))) * (1.0f - (1.0f / (1.0f - ((xyD * mu) / (releaseSpeed * cosf(launchPitch)))))) + tanf(launchPitch), 1.0f);
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
					// Fully face the target in the XY plane once half of the projectile's time to the initial target position has elapsed.
					yawToSet = Util::NormalizeAng0To2Pi
					(
						currentYaw + 
						Util::InterpolateSmootherStep
						(
							0.0f, 
							0.75f, 
							min(1.0f, a_projectile->livingTime / (0.5f * initialTimeToTargetSecs))
						) * yawDiff
					);
				}

				// Next, check if the homing projectile should fully start homing in on the target instead of following its initial fixed trajectory.
				// Check if the projectile is projected to reach its apex between now and the next frame.
				const float nextXY = releaseSpeed * (t + *g_deltaTimeRealTime) * cosf(launchPitch);
				const float nextPitchOnTraj = -atan2(tanf(launchPitch) - (g * nextXY) / powf(releaseSpeed * cosf(launchPitch), 2.0f), 1.0f);
				// Pitch is zero at the apex.
				bool atApex = pitchOnTraj == 0.0f;
				// Different sign: angled up to angled down next frame.
				bool willHitApexBeforeNextFrame = nextPitchOnTraj >= 0.0f && pitchOnTraj <= 0.0f;
				bool reachingApex = atApex || willHitApexBeforeNextFrame;
				// Target has moved above the projectile at a steeper pitch than the projectile's current pitch.
				bool moveUpwardOffTraj = (pitchOnTraj < 0.0f && pitchToTarget < pitchOnTraj);
				// Target has moved below the projectile at a steeper pitch than the projectile's current pitch.
				bool moveDownwardOffTraj = (pitchOnTraj > 0.0f && pitchToTarget > pitchOnTraj);
				// Last point at which the projectile can stay on its fixed trajectory before homing in.
				bool passedHalfTimeOfFlight = a_projectile->livingTime > 0.5f * managedProjInfo->initialTrajTimeToTarget;
				// If the gravitational constant was modified and is arbitrarily close to 0, also start homing in..
				bool noGrav = g < 1e-5f;

				// Set as homing if not already set 
				// and one of the above conditions is true.
				if (!managedProjInfo->startedHomingIn && 
				   (passedHalfTimeOfFlight || reachingApex || moveUpwardOffTraj ||
					moveDownwardOffTraj || noGrav || tooLongToReach))
				{
					managedProjInfo->startedHomingIn = true;
					managedProjInfo->startedHomingTP = SteadyClock::now();
				}
			}

			// NOTE: Might uncomment eventually if air resistance is desired.
			// Right now, it makes a negligible difference that is not worth the extra computation,
			// especially since only at most half of the projectile's time of flight
			// is spent along the fixed initial trajectory.
			//
			// With air resistance.
			/*
			const float velXY = releaseSpeed * cosf(launchPitch) * exp(-mu * t);
			const float velX = velXY * cosf(launchYaw);
			const float velY = velXY * sinf(launchYaw);
			const float velZ = -g / mu + ((releaseSpeed * sinf(launchPitch) + g / mu) * exp(-mu * t));
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
			float maxDistPerFrame = max(a_projectile->GetSpeed(), a_resultingVelocity.Length()) * *g_deltaTimeRealTime;
			// Velocity mult which slows down the projectile when close to the target to minimize overshooting and
			// jarring course correction.
			float distSlowdownFactor = std::clamp(powf(distToTarget / (maxDistPerFrame + 0.01f), 5.0f), 0.1f, 1.0f);
			// Direction from the current positino to the target.
			auto dirToTarget = aimTargetPos - a_projectile->data.location;
			dirToTarget.Unitize();
			// Last frame's velocity direction.
			auto velDirLastFrame = a_resultingVelocity;
			velDirLastFrame.Unitize();
			// Angle between last frame's velocity and the target.
			float angBetweenVelAndToTarget = acosf(std::clamp(dirToTarget.Dot(velDirLastFrame), -1.0f, 1.0f));
			// Went past the target if velocity direction and direction to target diverge by >= 90 degrees and
			// the distance to the target is less than the max distance travelable per frame.
			bool wentPastTarget = angBetweenVelAndToTarget >= PI / 2.0f && distToTarget <= maxDistPerFrame;

			// Remaining portion of rotation smoothing interval.
			float remainingRotSmoothingLifetimeRatio = 1.0f;
			// Projectile is now homing in, smooth out pitch and yaw to follow the target.
			if (managedProjInfo->startedHomingIn)
			{
				float secsSinceStartedHoming = Util::GetElapsedSeconds(managedProjInfo->startedHomingTP.value(), true);
				// Can't hit target with given launch pitch, so set pitch directly to target right away.
				if (initialTimeToTargetSecs == 0.0 || tooLongToReach || !managedProjInfo->startedHomingTP.has_value())
				{
					pitchToSet = pitchToTarget;
					yawToSet = yawToTarget;
					remainingRotSmoothingLifetimeRatio = 0.0f;
					// Turned directly to face the target once homing starts, so the target is no longer behind the projectile.
					// If the projectile eventually goes past the target again, it won't turn around to face it anymore.
					if (secsSinceStartedHoming == 0.0f)
					{
						wentPastTarget = false;
					}
				}
				else
				{
					// Full pitch smoothing lifetime is at most half of the projectile's fixed trajectory lifetime.
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
				if (auto baseSpeed = a_projectile->GetSpeed(); baseSpeed > 0.0f)
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

				// Continue homing in only if the original flight time has not been reached or
				// if the projectile has not gone past the target.
				continueSettingTrajectory = (remainingRotSmoothingLifetimeRatio > 0.0f || !wentPastTarget);
			}

			if (continueSettingTrajectory)
			{
				// Set refr data angles.
				a_projectile->data.angle.x = pitchToSet;
				a_projectile->data.angle.z = yawToSet;
				// Set rotation matrix to maintain consistency with the previously set refr data angles.
				if (auto current3D = Util::GetRefr3D(a_projectile); current3D)
				{
					Util::SetRotationMatrix(current3D->local.rotate, a_projectile->data.angle.x, a_projectile->data.angle.z);
				}

				// Set velocity.
				if (managedProjInfo->startedHomingIn) 
				{
					auto currentDir = a_resultingVelocity;
					currentDir.Unitize();
					auto interpFactor = Util::InterpolateSmootherStep(0.0f, 1.0f, 1.0f - remainingRotSmoothingLifetimeRatio);
					// Adjust the velocity's direction to slowly converge to the direction towards the target position.
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
					velToSet = Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) * speed;
				}

				a_resultingVelocity = velToSet;
				a_projectile->linearVelocity = a_resultingVelocity;

				// REMOVE when done debugging.
				/*logger::debug("[Projectile Hook] SetHomingTrajectory: {} (0x{:X}) is now homing: {}, speed to set: {}, distance to target: {}, initial time to target: {}, remaining rotation smoothing lifetime ratio: {}, moved: {}, pitch/yaw to target: {}, {}, current pitch/yaw: {}, {}, yaw diff: {}, pitch/yaw to set homing: {}, {}, resulting velocity: ({}, {}, {})",
					a_projectile->GetName(), a_projectile->formID,
					managedProjInfo->startedHomingIn,
					speed,
					a_projectile->data.location.GetDistance(aimTargetPos),
					initialTimeToTargetSecs,
					remainingRotSmoothingLifetimeRatio,
					a_projectile->distanceMoved,
					pitchToTarget * TO_DEGREES,
					yawToTarget * TO_DEGREES,
					currentPitch * TO_DEGREES,
					currentYaw * TO_DEGREES,
					yawDiff * TO_DEGREES,
					pitchToSet * TO_DEGREES,
					yawToSet * TO_DEGREES,
					a_resultingVelocity.x, a_resultingVelocity.y, a_resultingVelocity.z);*/
			}
			else
			{
				// Set as no longer managed to prevent further trajectory adjustment after this frame.
				a_p->tm->managedProjHandler->Remove(a_projectile);
			}
		}

		void ProjectileHooks::SetFixedTrajectory(const std::shared_ptr<CoopPlayer>& a_p, RE::Projectile* a_projectile, RE::NiPoint3& a_resultingVelocity)
		{
			// Not a managed projectile, so nothing to do here.
			if (!a_p->tm->managedProjHandler->IsManaged(a_projectile))
			{
				return;
			}

			// Guaranteed to be managed here.
			const auto& managedProjInfo = a_p->tm->managedProjHandler->GetInfo(a_projectile);
			// Pre-computed fixed trajectory data.
			const RE::NiPoint3& releasePos = managedProjInfo->releasePos;
			const RE::NiPoint3& targetPos = managedProjInfo->trajectoryEndPos;
			const float& launchPitch = managedProjInfo->launchPitch;
			const float& launchYaw = managedProjInfo->launchYaw;
			const float& releaseSpeed = managedProjInfo->releaseSpeed;
			const float& g = managedProjInfo->g;
			const double& mu = managedProjInfo->mu;
			const float& t = a_projectile->livingTime;

			// Immediately direct at the target position
			// if the initial time to hit the target is less than one frame.
			// Won't have multiple frames to move towards the intercept position, 
			// so do it here right after release.
			if (managedProjInfo->initialTrajTimeToTarget <= *g_deltaTimeRealTime)
			{
				// Pitch and yaw to the target.
				float pitchToTarget = Util::GetPitchBetweenPositions(a_projectile->data.location, targetPos);
				float yawToTarget = Util::GetYawBetweenPositions(a_projectile->data.location, targetPos);
				// Set refr data angles.
				a_projectile->data.angle.x = pitchToTarget;
				a_projectile->data.angle.z = yawToTarget;
				// Set rotation matrix to maintain consistency with the previously set refr data angles.
				if (auto current3D = Util::GetRefr3D(a_projectile); current3D)
				{
					Util::SetRotationMatrix(current3D->local.rotate, a_projectile->data.angle.x, a_projectile->data.angle.z);
				}

				// Set velocity.
				auto velToSet = Util::RotationToDirectionVect(-pitchToTarget, Util::ConvertAngle(yawToTarget)) * a_resultingVelocity.Length();
				a_resultingVelocity = velToSet;
				a_projectile->linearVelocity = a_resultingVelocity;

				// No longer handled after directing at the target position.
				a_p->tm->managedProjHandler->Remove(a_projectile);
				return;
			}

			// NOTE: 
			// Since the frametime is discrete and certain projectiles move extremely fast,
			// we cannot use the true velocity at any particular time computed
			// from the trajectory's formulas. We have to instead "connect the dots"
			// between the current projectile position and the next expected projectile
			// position one frame later to ensure that it will arrive at the trajectory's endpoint.
			// If the frametimes vary greatly from frame to frame,
			// the position and velocity calculations will not conform as well to the original
			// trajectory, speeding up and slowing down along the path.
			// 
			// Factors in linear air resistance.
			// May remove eventually.
			//
			// Initial X, Y components of velocity.
			float vx0 = releaseSpeed * cosf(launchPitch);
			float vy0 = releaseSpeed * sinf(launchPitch);
			// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
			// XY, and Z positions: in 2D plane, XY pos is the X coordinate, and Z pos is the Y coordinate.
			float currXY = (vx0 / mu) * (1 - exp(-mu * t));
			float currZ = (-g * t / mu) + (1.0f / mu) * (vy0 + g / mu) * (1.0f - exp(-mu * t));
			// Next projected XY and Z offsets.
			float nextXY = (vx0 / mu) * (1 - exp(-mu * (t + *g_deltaTimeRealTime)));
			float nextZ = (-g * (t + *g_deltaTimeRealTime) / mu) + (1.0f / mu) * (vy0 + g / mu) * (1.0f - exp(-mu * (t + *g_deltaTimeRealTime)));

			// Pitch along the trajectory and the XY, X, Y, and Z components of velocity.
			// '+' means up in the XY plane, and '-' means down.
			// Pitch to face the next frame's expected position.
			const float pitchOnTraj = atan2f((nextZ - currZ), (nextXY - currXY));
			// Get the estimated speed from dividing the distance 
			// between the two positions by the current frame time.
			const float speedToSet = Util::GetXYDistance(currXY, currZ, nextXY, nextZ) / *g_deltaTimeRealTime;
			auto vel = Util::RotationToDirectionVect(pitchOnTraj, launchYaw) * speedToSet;

			// Set our computed velocity and pitch/yaw.
			a_resultingVelocity = vel;
			a_projectile->linearVelocity = a_resultingVelocity;
			// Yaw will not change throughout.
			a_projectile->data.angle.z = Util::ConvertAngle(launchYaw);
			// Pitch is equal to the pitch along the trajectory.
			// NOTE: Sign flipped, since Skyrim's sign convention for pitch is < 0 when facing up and > 0 when facing down.
			a_projectile->data.angle.x = -pitchOnTraj;

			// Set rotation matrix to maintain consistency with the previously set refr data angles.
			if (auto current3D = Util::GetRefr3D(a_projectile))
			{
				Util::SetRotationMatrix(current3D->local.rotate, a_projectile->data.angle.x, a_projectile->data.angle.z);
			}
		}

// [TESCAMERA HOOKS]:
		void TESCameraHooks::Update(RE::TESCamera* a_this)
		{
			// Don't do anything here if the co-op camera is active,
			// since all camera orientation logic is handled by the co-op camera manager.
			if (glob.globalDataInit && glob.cam->IsRunning())
			{
				if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1) 
				{
					// Camera local position/rotation is modified when ragdolled (bleedout camera position),
					// inactive, staggered, sitting/sleeping, or sprinting (AnimatedCameraDelta),
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
						p1->IsSprinting()
					};
					if (localRotationModified) 
					{
						return;
					}
					else
					{
						// Otherwise, run the original func first to allow other plugins that hook this func
						// to execute their logic first before we apply the co-op camera orientation.
						_Update(a_this);
						glob.cam->SetCamOrientation();
						return;
					}
				}
			}

			_Update(a_this);
		}

// [TESOBJECTBOOK HOOKS]:
		bool TESObjectBOOKHooks::Activate(RE::TESObjectBOOK* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3, RE::TESBoundObject* a_object, std::int32_t a_targetCount)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (a_targetRef && GlobalCoopData::IsCoopPlayer(a_activatorRef))
				{
					// Is an unread skillbook.
					if (auto book = a_targetRef->GetBaseObject() ? a_targetRef->GetBaseObject()->As<RE::TESObjectBOOK>() : nullptr; 
						book && !book->IsRead() && book->TeachesSkill())
					{
						// Level up co-op companion players only, since P1 levels up when the BookMenu is triggered on activation.
						for (const auto& p : glob.coopPlayers)
						{
							if (p->isActive && !p->isPlayer1)
							{
								p->pam->LevelUpSkillWithBook(book);
							}
						}
					}
				}
			}

			return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
		}

// [TESOBJECTREFR HOOKS]:
		void TESObjectREFRHooks::SetParentCell(RE::TESObjectREFR* a_this, RE::TESObjectCELL* a_cell)
		{
			// Gets called before Load() and links a cell with the refr.
			// If either exterior or interior occlusion is set to removed in the mod's settings, 
			// delete occlusion markers here before they are fully loaded into the cell.

			// Set the parent cell first.
			_SetParentCell(a_this, a_cell);
			if (a_cell)
			{
				if (a_this->parentCell && a_this->GetBaseObject() && a_this->GetBaseObject()->IsOcclusionMarker())
				{
					auto sky = RE::TES::GetSingleton() ? RE::TES::GetSingleton()->sky : nullptr;
					if ((Settings::bRemoveInteriorOcclusion && (a_this->parentCell->IsInteriorCell() || 
						(sky && sky->mode == RE::Sky::Mode::kInterior))) ||
						(Settings::bRemoveExteriorOcclusion && a_this->parentCell->IsExteriorCell()))
					{
						// Delete marker.
						a_this->SetDelete(true);
						// Set skybox only for interior cells to sometimes remove fog when zooming out beyond the traversable area.
						Util::SetSkyboxOnlyForInteriorModeCell(a_this->parentCell);
					}
				}
			}
		}

		void ThirdPersonCameraStatesHooks::Begin(RE::ThirdPersonState* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				return;
			}

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
		void ThirdPersonCameraStatesHooks::GetRotation(RE::ThirdPersonState* a_this, RE::NiQuaternion& a_rotation)
		{
			// Store the co-op cam's rotation, instead of player 1's, for other plugins that may be
			// checking the camera's rotation by using this hook.
			if (glob.globalDataInit && glob.coopSessionActive && glob.cam->IsRunning())
			{
				RE::NiMatrix3 m;
				Util::SetRotationMatrix(m, glob.cam->camPitch, glob.cam->camYaw);
				a_rotation = Util::RotationMatrixToQuaternion(m);
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

		void ThirdPersonCameraStatesHooks::HandleLookInput(RE::ThirdPersonState* a_this, const RE::NiPoint2& a_input)
		{
			// Handle player 1 look input only when the co-op camera is inactive.
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

		void ThirdPersonCameraStatesHooks::SetFreeRotationMode(RE::ThirdPersonState* a_this, bool a_weaponSheathed)
		{
			// Keep free rotation enabled (better for TDM compat) and set the camera yaw while the co-op cam is active.
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
			if (glob.globalDataInit && glob.coopSessionActive && ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				bool appliedToP1 = a_this->GetTargetActor() == RE::PlayerCharacter::GetSingleton();
				bool arcaneFeverEffect = 
				{
					(a_this->GetBaseObject()) &&
					(a_this->GetBaseObject()->data.primaryAV == RE::ActorValue::kLastFlattered ||
					 a_this->GetBaseObject()->data.secondaryAV == RE::ActorValue::kLastFlattered)
				};
				if (appliedToP1 && arcaneFeverEffect)
				{
					if (const auto& p1 = glob.coopPlayers[glob.player1CID]; p1->isInGodMode)
					{
						a_this->Dispel(true);
						return;
					}
				}
			}

			return _Start(a_this);
		}

// [VAMPIRE LORD EFFECT HOOKS]: 
		void VampireLordEffectHooks::Start(RE::VampireLordEffect* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); pIndex != -1)
				{
					logger::debug("[VampireLordEffect Hooks] Start: {}", a_this->GetTargetActor()->GetName());
					const auto& p = glob.coopPlayers[pIndex];

					// Save pre-transformation race to revert to later if the player is not already transforming/transformed.
					if (!p->isTransformed && !p->isTransforming)
					{
						p->preTransformationRace = p->coopActor->race;
					}

					p->isTransforming = true;

					if (!p->isPlayer1) 
					{
						// Unequip hand forms for co-op companion once the transformation begins.
						// Game does not always do this automatically and the weapon anim object can
						// stay attached to its hand node after the transformation.
						p->em->EquipFists();
					}

					p->transformationTP = SteadyClock::now();
				}
			}

			return _Start(a_this);
		}

		void VampireLordEffectHooks::Finish(RE::VampireLordEffect* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); pIndex != -1)
				{
					const auto& p = glob.coopPlayers[pIndex];
					// Should have already stopped transforming at this point, but if not, clear the flag.
					if (p->isTransforming) 
					{
						p->isTransforming = false;
					}
					logger::debug("[VampireLordEffectHooks Hooks] Finish: {}. Race: {}, is transformed: {}",
						a_this->GetTargetActor()->GetName(),
						p->coopActor->race ? p->coopActor->race->GetName() : "NONE",
						p->isTransformed);
				}
			}

			return _Finish(a_this);
		}

// [WEREWOLF EFFECT HOOKS]: 
		void WerewolfEffectHooks::Start(RE::WerewolfEffect* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); pIndex != -1)
				{
					logger::debug("[WerewolfEffectHooks Hooks] Start: {}", a_this->GetTargetActor()->GetName());
					const auto& p = glob.coopPlayers[pIndex];
					
					// Save pre-transformation race to revert to later if the player is not already transforming/transformed.
					if (!p->isTransformed && !p->isTransforming)
					{
						p->preTransformationRace = p->coopActor->race;
					}

					p->isTransforming = true;

					if (!p->isPlayer1)
					{
						// Unequip hand forms for co-op companion once the transformation begins.
						// Game does not always do this automatically and the weapon anim object can
						// stay attached to its hand node after the transformation.
						p->em->EquipFists();
					}

					// Reset to base transformation time.
					p->secsMaxTransformationTime = 150.0f;
					p->transformationTP = SteadyClock::now();
				}
			}

			return _Start(a_this);
		}

		void WerewolfEffectHooks::Finish(RE::WerewolfEffect* a_this)
		{
			if (glob.globalDataInit && glob.coopSessionActive)
			{
				if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_this->GetTargetActor()); pIndex != -1)
				{
					const auto& p = glob.coopPlayers[pIndex];
					// Should have already stopped transforming at this point, but if not, clear the flag.
					if (p->isTransforming)
					{
						p->isTransforming = false;
					}
					logger::debug("[WerewolfEffectHooks Hooks] Finish: {}. Race: {}, is transformed: {}", 
						a_this->GetTargetActor()->GetName(),
						p->coopActor->race ? p->coopActor->race->GetName() : "NONE",
						p->isTransformed);
				}
			}

			return _Finish(a_this);
		}

//=========================
// [MENU PROCESSING HOOKS]:
//=========================

		RE::UI_MESSAGE_RESULTS BarterMenuHooks::ProcessMessage(RE::BarterMenu* a_this, RE::UIMessage& a_message)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (!glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[BarterMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), nullptr);

					// Calculate the result of this message, and if it isn't handled, restore P1's data.
					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						// Have to restore P1's inventory here if the game ignores this call to open the menu.
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							logger::error("[BarterMenu Hooks] ERR: ProcessMessage. Restoring player 1's inventory, since the message to open the menu was ignored. RESULT: {}.", result);
							GlobalCoopData::CopyOverCoopPlayerData(false, menuName, p->coopActor->GetHandle(), nullptr);
						}
					}

					return result;
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS BookMenuHooks::ProcessMessage(RE::BookMenu* a_this, RE::UIMessage& a_message)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (ignored || !glob.globalDataInit || !glob.coopSessionActive ||
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[BookMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), a_this->GetTargetForm());
				}
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS ContainerMenuHooks::ProcessMessage(RE::ContainerMenu* a_this, RE::UIMessage& a_message)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (ignored || !glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[ContainerMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), nullptr);
				}
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS CraftingMenuHooks::ProcessMessage(RE::CraftingMenu* a_this, RE::UIMessage& a_message)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (!glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[CraftingMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;
					RE::TESForm* assocForm = nullptr;
					// Set furniture (crafting station) as associated form.
					if (a_this->subMenu)
					{
						assocForm = a_this->subMenu->furniture;
					}

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), assocForm);

					// Calculate the result of this message, and if it isn't handled, restore P1's data.
					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						// Have to restore P1's inventory here if the game ignores this call to open the menu.
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							logger::error("[CraftingMenu Hooks] ERR: ProcessMessage: Restoring player 1's inventory, since the message to open the menu was ignored. RESULT: {}.", result);
							GlobalCoopData::CopyOverCoopPlayerData(false, menuName, p->coopActor->GetHandle(), assocForm);
						}
					}

					return result;
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS DialogueMenuHooks::ProcessMessage(RE::DialogueMenu* a_this, RE::UIMessage& a_message)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (ignored || !glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return result;
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[DialogueMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;
					RE::TESForm* assocForm = nullptr;
					// Get speaker as associated form.
					if (auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
						menuTopicManager && (menuTopicManager->speaker.get() || menuTopicManager->lastSpeaker.get()))
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
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), assocForm);
				}
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS FavoritesMenuHooks::ProcessMessage(RE::FavoritesMenu* a_this, RE::UIMessage& a_message)
		{
			logger::debug("[FavoritesMenu Hooks] ProcessMessage: {}.",
				*a_message.type);
			// NOTE: Favorited items are stored in the Favorite Menu's favorites list sometime between the ProcessMessage() call and the menu opening.
			// Don't call the original ProcessMessage() func until the requesting player's favorited items have been imported by P1.

			// Nothing to do here, since co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (!glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[FavoritesMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				const auto& p = glob.coopPlayers[resolvedCID];
				const auto& coopP1 = glob.coopPlayers[glob.player1CID];
				// Do not import co-op favorites if transformed into a Vampire Lord,
				// so we have access to P1's Vampire Lord spells.
				if (Util::IsVampireLord(p->coopActor.get()) && opening)
				{
					// Make sure P1 knows the revert form spell so that this player can transform back.
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
					{
						auto revertSpell = dataHandler->LookupForm<RE::SpellItem>(0xCD5C, "Dawnguard.esm");
						if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && revertSpell && !p1->HasSpell(revertSpell))
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
					// Reset to P1 Vampire Lord state when closing.
					a_this->isVampire = Util::IsVampireLord(coopP1->coopActor.get());
				}

				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const RE::BSFixedString menuName = a_this->MENU_NAME;

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), nullptr);

					// Have to restore P1's favorited items here if the game ignores this call to open the menu.
					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							logger::error("[FavoritesMenu Hooks] ERR: ProcessMessage. Restoring player 1's favorites, since the message to open the FavoritesMenu was not handled. RESULT: {}.", result);
							GlobalCoopData::CopyOverCoopPlayerData(false, menuName, p->coopActor->GetHandle(), nullptr);
						}
					}

					return result;
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

		RE::UI_MESSAGE_RESULTS InventoryMenuHooks::ProcessMessage(RE::InventoryMenu* a_this, RE::UIMessage& a_message)
		{
			// Open co-op companion's inventory (ContainerMenu) instead of P1's inventory (InventoryMenu) 
			// when attempting to access the InventoryMenu through the TweenMenu or otherwise.
			if (glob.globalDataInit && glob.coopSessionActive &&
				glob.menuCID != -1 && glob.menuCID != glob.player1CID && Hash(a_message.menu) == Hash(RE::InventoryMenu::MENU_NAME))
			{
				bool isOpenReq = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
				const auto& reqP = glob.coopPlayers[glob.menuCID];
				if (auto msgQ = RE::UIMessageQueue::GetSingleton(); msgQ)
				{
					if (isOpenReq)
					{
						msgQ->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
						// Reset player 1 damage multiplier so that the co-op player's inventory reports the correct damage for weapons.
						glob.player1Actor->SetActorValue(RE::ActorValue::kAttackDamageMult, 1.0f);
						bool succ = glob.moarm->InsertRequest(reqP->controllerID, InputAction::kInventory, SteadyClock::now(), RE::ContainerMenu::MENU_NAME, reqP->coopActor->GetHandle());
						if (succ)
						{
							logger::debug("[InventoryMenu Hooks] ProcessMessage: Opening {}'s inventory instead of P1's.", reqP->coopActor->GetName());
							Util::Papyrus::OpenInventory(reqP->coopActor.get());
						}

						// Ignore request to prevent further processing.
						return RE::UI_MESSAGE_RESULTS::kIgnore;
					}
				}
			}

			return _ProcessMessage(a_this, a_message);
		}
		
		RE::UI_MESSAGE_RESULTS LoadingMenuHooks::ProcessMessage(RE::LoadingMenu* a_this, RE::UIMessage& a_message)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, global data is not initialized, serializable data is not available,
			// or this menu is not the target of the message. 
			if (ignored || !glob.globalDataInit || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle opening message.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			if (!opening)
			{
				return result;
			}

			// Restore P1 data if data was copied over before this LoadingMenu opened.
			if (*glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone) 
			{
				logger::debug("[LoadingMenu Hooks] ProcessMessage. Loading menu opened with data copied (types: 0x{:X}) over to P1. Restoring P1 data. Co-op session active: {}.",
					*glob.copiedPlayerDataTypes, glob.coopSessionActive);
				GlobalCoopData::CopyOverCoopPlayerData(false, a_this->MENU_NAME, glob.player1Actor->GetHandle(), nullptr);
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS StatsMenuHooks::ProcessMessage(RE::StatsMenu* a_this, RE::UIMessage& a_message)
		{
			auto result = _ProcessMessage(a_this, a_message);
			bool ignored = result == RE::UI_MESSAGE_RESULTS::kIgnore;
			// Nothing to do here, since the message is ignored, global data is not initialized, serializable data is not available,
			// or this menu is not the target of the message. 
			if (ignored || !glob.globalDataInit || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return result;
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return result;
			}

			// TODO: Implement Vampire Lord and Werewolf perk sync when co-op players are transformed.
			// So for now, do not modify perk data if P1 is transformed.
			auto p1 = RE::PlayerCharacter::GetSingleton();
			bool p1IsTransformed = Util::IsWerewolf(p1) || Util::IsVampireLord(p1);

			// Is P1 requesting to open the StatsMenu?
			// Have to also adjust perk data for P1 when outside of co-op.
			bool p1Req = !glob.coopSessionActive;
			if (glob.coopSessionActive)
			{
				bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;
				// Do not modify the requests queue, since the menu input manager still needs this info
				// when setting the request and menu controller IDs when this menu opens/closes.
				int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);

				// REMOVE when done debugging.
				logger::debug("[StatsMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
					glob.menuCID, resolvedCID, opening, closing);

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
						GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), nullptr);
					}
				}
				else
				{
					// If another player is not requesting control, default to P1.
					p1Req = true;
				}
			}

			// Don't adjust data if Enderal is installed.
			if (p1Req && !p1IsTransformed && !ALYSLC::EnderalCompat::g_enderalSSEInstalled && (opening || closing))
			{
				GlobalCoopData::AdjustPerkDataForPlayer1(opening);
			}

			return result;
		}

		RE::UI_MESSAGE_RESULTS TrainingMenuHooks::ProcessMessage(RE::TrainingMenu* a_this, RE::UIMessage& a_message)
		{
			// Nothing to do here, since co-op is not active, serializable data is not available, or this menu is not the target of the message. 
			if (!glob.globalDataInit || !glob.coopSessionActive || 
				glob.serializablePlayerData.empty() || a_message.menu != a_this->MENU_NAME)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Only need to handle open/close messages.
			bool opening = *a_message.type == RE::UI_MESSAGE_TYPE::kShow;
			bool closing = *a_message.type == RE::UI_MESSAGE_TYPE::kHide || *a_message.type == RE::UI_MESSAGE_TYPE::kForceHide;
			if (!opening && !closing)
			{
				return _ProcessMessage(a_this, a_message);
			}

			// Do not modify the requests queue, since the menu input manager still needs this info
			// when setting the request and menu controller IDs when this menu opens/closes.
			int32_t resolvedCID = glob.moarm->ResolveMenuControllerID(a_this->MENU_NAME, false);
			bool hasCopiedData = *glob.copiedPlayerDataTypes != CopyablePlayerDataTypes::kNone;

			// REMOVE when done debugging.
			logger::debug("[TrainingMenu Hooks] ProcessMessage. Current menu CID: {}, resolved menu CID: {}. Opening: {}, closing: {}.",
				glob.menuCID, resolvedCID, opening, closing);

			// Control is/was requested by co-op companion player.
			if (resolvedCID != -1 && resolvedCID != glob.player1CID)
			{
				// Copy back player data only if data was already copied.
				// Ignore subsequent hide messages once P1's data is restored.
				closing &= hasCopiedData;
				if (opening || closing)
				{
					const auto& p = glob.coopPlayers[resolvedCID];
					const RE::BSFixedString menuName = a_this->MENU_NAME;
					RE::TESForm* assocForm = nullptr;
					// Set speaker as associated form.
					if (auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); menuTopicManager && (menuTopicManager->speaker.get() || menuTopicManager->lastSpeaker.get()))
					{
						auto speaker = menuTopicManager->speaker.get() ? menuTopicManager->speaker.get() : menuTopicManager->lastSpeaker.get();
						assocForm = speaker.get();
					}

					// Copy over player data.
					GlobalCoopData::CopyOverCoopPlayerData(opening, menuName, p->coopActor->GetHandle(), assocForm);

					auto result = _ProcessMessage(a_this, a_message);
					if (opening)
					{
						// Have to restore P1's AVs here if the game ignores this call to open the menu.
						if (result != RE::UI_MESSAGE_RESULTS::kHandled)
						{
							logger::error("[TrainingMenu Hooks] ERR: ProcessMessage. Restoring AVs for {} and P1, since the message to open the menu was ignored. RESULT: {}.", 
								p->coopActor->GetName(), result);
							GlobalCoopData::CopyOverCoopPlayerData(false, menuName, p->coopActor->GetHandle(), assocForm);
						}
					}

					return result;
				}
			}

			return _ProcessMessage(a_this, a_message);
		}

//====================
// [P1 HANDLER HOOKS]:
//====================

		bool ActivateHandlerHooks::CanProcess(RE::ActivateHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive && 
				a_event->device == RE::INPUT_DEVICE::kGamepad) 
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				bool hasBypassFlag = a_event->AsButtonEvent() && (a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA;
				const bool& canUseParaglider = 
				{
					ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
					p1 && p1->GetCharController() && p1->GetCharController()->context.currentState == RE::hkpCharacterStateType::kInAir
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

			return _CanProcess(a_this, a_event);
		}

		bool AttackBlockHandlerHooks::CanProcess(RE::AttackBlockHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive && 
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				const auto& eventName = a_event->QUserEvent();
				// Only the left and right attack event names (not left/right equip).
				if (a_event->AsButtonEvent() && (eventName == ue->rightAttack || eventName == ue->leftAttack))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}
		
		bool JumpHandlerHooks::CanProcess(RE::JumpHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			auto ue = RE::UserEvents::GetSingleton();
			if (ue && glob.globalDataInit && glob.coopSessionActive && a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				// 'Jump' event name and has P1 proxied bypass flag.
				if (a_event->AsButtonEvent() && a_event->QUserEvent() == ue->jump && 
					((a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool LookHandlerHooks::CanProcess(RE::LookHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling the camera orientation.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11)) 
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
			// Thumbstick event that is not from co-op companion player.
			// Ignore when the co-op camera is active.
			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive &&
				glob.cam->IsRunning() && thumbstickEvent)
			{
				return false;
			}

			return _CanProcess(a_this, a_event);
		}

		bool MovementHandlerHooks::CanProcess(RE::MovementHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player to control P1's movement.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11)) 
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
			// Thumbstick event that is not from co-op companion player.
			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive &&
				glob.cam->IsRunning() && thumbstickEvent)
			{
				// Allow LS movement processing when motion driven. 
				// Otherwise, do not handle this movement event.
				bool menuStopsMovement = Util::OpenMenuStopsMovement();
				auto p1 = RE::PlayerCharacter::GetSingleton(); 
				if (p1 && p1->movementController && p1->movementController->unk1C5 && thumbstickEvent->IsLeft() && !menuStopsMovement)
				{
					const auto& coopP1 = glob.coopPlayers[glob.player1CID];
					// NOTE: Completely unnecessary if TDM is installed.
					if (!ALYSLC::TrueDirectionalMovementCompat::g_trueDirectionalMovementInstalled) 
					{
						// Adjust the thumbstick event stick displacements so that P1 moves relative
						// to the co-op camera instead of their own facing direction while motion driven.
						if (thumbstickEvent->xValue != 0.0f && thumbstickEvent->yValue != 0.0f)
						{
							float p1FacingToCamYawDiff = -Util::NormalizeAngToPi(glob.cam->camYaw - p1->data.angle.z);
							float thumbstickAngle = Util::NormalizeAng0To2Pi
							(
								atan2f(thumbstickEvent->yValue, thumbstickEvent->xValue)
							);
							thumbstickAngle = Util::NormalizeAng0To2Pi(thumbstickAngle + p1FacingToCamYawDiff);
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

		bool ReadyWeaponHandlerHooks::CanProcess(RE::ReadyWeaponHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}
			
			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive && 
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				// 'Ready Weapon' event and has P1 proxied bypass flag.
				if (a_event->AsButtonEvent() && a_event->QUserEvent() == ue->readyWeapon && 
					((a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA))
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			
			return _CanProcess(a_this, a_event);
		}

		bool ShoutHandlerHooks::CanProcess(RE::ShoutHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive && 
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				// 'Shout' event and has P1 proxied bypass flag.
				if (a_event->AsButtonEvent() && a_event->QUserEvent() == ue->shout && 
					((a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool SneakHandlerHooks::CanProcess(RE::SneakHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive &&
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				// 'Sneak' event and has P1 proxied bypass flag
				if (a_event->AsButtonEvent() && a_event->QUserEvent() == ue->sneak && 
					((a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool SprintHandlerHooks::CanProcess(RE::SprintHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}
			
			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive && 
				a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				// 'Sprint' event and has P1 proxied bypass flag
				if (a_event->AsButtonEvent() && a_event->QUserEvent() == ue->sprint && 
					((a_event->AsButtonEvent()->pad24 & 0xFFFF) == 0xC0DA))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			return _CanProcess(a_this, a_event);
		}

		bool TogglePOVHandlerHooks::CanProcess(RE::TogglePOVHandler* a_this, RE::InputEvent* a_event)
		{
			// From co-op player; ignore since we don't want another player controlling P1.
			if (auto idEvent = a_event->AsIDEvent(); idEvent && ((idEvent->pad24 & 0xFFFF) == 0xCA11))
			{
				return false;
			}

			// Ignore while the co-op camera is active to prevent POV changes.
			if (auto ue = RE::UserEvents::GetSingleton(); ue && glob.globalDataInit && glob.coopSessionActive &&
				glob.cam->IsRunning() && a_event->device == RE::INPUT_DEVICE::kGamepad)
			{
				return false;
			}

			return _CanProcess(a_this, a_event);
		}
	}
}

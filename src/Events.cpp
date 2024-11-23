#pragma once
#include "Events.h"
#include <queue>
#include <Compatibility.h>
#include <Controller.h>
#include <DebugAPI.h>
#include <GlobalCoopData.h>
#include <Player.h>
#include <Proxy.h>
#include <Settings.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();
	using EventResult = RE::BSEventNotifyControl;

	void Events::RegisterEvents()
	{
		// Register bleedout event handler.
		CoopBleedoutEventHandler::Register();
		// Register cell change event handler.
		CoopCellChangeHandler::Register();
		// Register cell fully loaded event handler.
		CoopCellFullyLoadedHandler::Register();
		// Register container change event handler.
		CoopContainerChangedHandler::Register();
		// Register death event handler.
		CoopDeathEventHandler::Register();
		// Register equip event handler.
		CoopEquipEventHandler::Register();
		// Register hit event handler.
		CoopHitEventHandler::Register();
		// Register load game event handler.
		CoopLoadGameEventHandler::Register();
		// Register menu open/close event handler.
		CoopMenuOpenCloseHandler::Register();
		// Register position player event handler.
		CoopPositionPlayerEventHandler::Register();
		logger::info("[Events] RegisterEvents: event registration complete.");
	}

	void Events::ResetMenuState()
	{
		glob.mim->ToggleCoopPlayerMenuMode(-1);
		GlobalCoopData::ResetMenuCIDs();
		glob.supportedMenuOpen.store(false);
		glob.lastSupportedMenusClosedTP = SteadyClock::now();
	}

	CoopBleedoutEventHandler* CoopBleedoutEventHandler::GetSingleton()
	{
		static CoopBleedoutEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopBleedoutEventHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopBleedoutEventHandler::GetSingleton());
			logger::info("[Events] Registered for bleedout events.");
		}
		else
		{
			logger::error("[Events] ERR: Could not register for bleedout events.");
		}
	}

	EventResult CoopBleedoutEventHandler::ProcessEvent(const RE::TESEnterBleedoutEvent* a_bleedoutEvent, RE::BSTEventSource<RE::TESEnterBleedoutEvent>*)
	{
		if (glob.allPlayersInit && a_bleedoutEvent && a_bleedoutEvent->actor && Settings::bUseReviveSystem)
		{
			if (glob.livingPlayers == 0 && a_bleedoutEvent->actor->IsPlayerRef()) 
			{
				auto p1 = a_bleedoutEvent->actor->As<RE::Actor>();
				Util::NativeFunctions::SetActorBaseDataFlag(p1->GetActorBase(), RE::ACTOR_BASE_DATA::Flag::kEssential, false);
				// Set to zero health, which usually triggers a death event.
				p1->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -p1->GetActorValue(RE::ActorValue::kHealth));
			}
			else if (auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(a_bleedoutEvent->actor); foundIndex != -1) 
			{
				if (const auto& p = glob.coopPlayers[foundIndex]; p->coopActor->currentProcess && p->coopActor->currentProcess->middleHigh) 
				{
					auto midHighProc = p->coopActor->currentProcess->middleHigh;
					midHighProc->deferredKillTimer = FLT_MAX;
				}

				return EventResult::kStop;
			}
		}

		return EventResult::kContinue;
	}

	CoopCellChangeHandler* CoopCellChangeHandler::GetSingleton() 
	{
		static CoopCellChangeHandler singleton;
		return std::addressof(singleton);
	}

	void CoopCellChangeHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopCellChangeHandler::GetSingleton());
			logger::info("[Events] Registered for cell change events.");
		}
		else 
		{ 
			logger::error("[Events] ERR: Could not register for cell change events."); 
		}
	}

	EventResult CoopCellChangeHandler::ProcessEvent(const RE::TESMoveAttachDetachEvent* a_cellChangeEvent, RE::BSTEventSource<RE::TESMoveAttachDetachEvent>*)
	{
		if (a_cellChangeEvent && a_cellChangeEvent->movedRef)
		{
			auto attachedRef = a_cellChangeEvent->movedRef;
			if (attachedRef && attachedRef->GetCurrent3D()) 
			{
				// Ensure actor does not fade in co-op with our co-op cam enabled.
				attachedRef->GetCurrent3D()->flags.set(RE::NiAVObject::Flag::kIgnoreFade, RE::NiAVObject::Flag::kAlwaysDraw);
				RE::NiUpdateData updateData;
				attachedRef->GetCurrent3D()->UpdateDownwardPass(updateData, 0);
			}

			if (glob.coopSessionActive) 
			{
				ALYSLC::Log("[Events] Cell change event: {} {}.", 
					a_cellChangeEvent->movedRef && a_cellChangeEvent->movedRef.get() ? a_cellChangeEvent->movedRef->GetName() : nullptr,
					a_cellChangeEvent->isCellAttached ? "attached" : "detached");

				if (auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(attachedRef); foundIndex != -1)
				{
					// Ensure player does not get faded while in co-op.
					if (attachedRef->GetCurrent3D())
					{
						if (!attachedRef->GetCurrent3D()->flags.all(RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade))
						{
							attachedRef->GetCurrent3D()->flags.set(RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade);
							RE::NiUpdateData updateData;
							attachedRef->GetCurrent3D()->UpdateDownwardPass(updateData, 0);
						}
					}

					const auto& p = glob.coopPlayers[foundIndex];
					// Prevent equip state bug mentioned in the equip manager where two handed weapons' animations break.
					for (const auto& p : glob.coopPlayers)
					{
						if (p->isActive && !p->isPlayer1 && p->coopActor->currentProcess)
						{
							p->pam->ReadyWeapon(false);
							p->em->ReEquipHandForms();
						}
					}
				}
			}
		}

		return EventResult::kContinue;
	}

	CoopContainerChangedHandler* CoopContainerChangedHandler::GetSingleton()
	{
		static CoopContainerChangedHandler singleton;
		return std::addressof(singleton);
	}

	void CoopContainerChangedHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopContainerChangedHandler::GetSingleton());
			logger::info("[Events] Registered for container changed events.");
		}
		else
		{
			logger::error("[Events] ERR: Could not register for container changed events.");
		}
	}

	EventResult CoopContainerChangedHandler::ProcessEvent(const RE::TESContainerChangedEvent* a_containerChangedEvent, RE::BSTEventSource<RE::TESContainerChangedEvent>*)
	{
		if (glob.globalDataInit && glob.coopSessionActive) 
		{
			if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1 && a_containerChangedEvent && a_containerChangedEvent->baseObj)
			{
				bool fromP1 = a_containerChangedEvent->oldContainer == p1->formID;
				bool toP1 = a_containerChangedEvent->newContainer == p1->formID;
				bool fromCoopEntity = glob.coopEntityBlacklistFIDSet.contains(a_containerChangedEvent->oldContainer);
				bool toCoopPlayer = GlobalCoopData::IsCoopPlayer(a_containerChangedEvent->newContainer);
				int32_t fromCoopPlayerIndex = GlobalCoopData::GetCoopPlayerIndex(a_containerChangedEvent->oldContainer);

				// Update P1's has-paraglider state before doing anything else.
				if ((ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled) && (toP1 || fromP1))
				{
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
					{
						if (auto paraglider = dataHandler->LookupForm<RE::TESObjectMISC>(0x802, "Paragliding.esp"); paraglider)
						{
							if (a_containerChangedEvent->baseObj == paraglider->formID)
							{
								auto invCounts = p1->GetInventoryCounts();
								ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider = invCounts.contains(paraglider) && invCounts.at(paraglider) > 0;

								// Add gale spell if not known already (Enderal only).
								if (ALYSLC::EnderalCompat::g_enderalSSEInstalled &&
									ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
									!p1->HasSpell(glob.tarhielsGaleSpell))
								{
									p1->AddSpell(glob.tarhielsGaleSpell);
								}
							}
						}
					}
				}

				// Added to player 1 or added to co-op player.
				// Prioritize co-op companion player loot through menus before performing Enderal-specific item transfers.
				const auto ui = RE::UI::GetSingleton(); 
				if (ui && glob.mim->managerMenuCID != -1)
				{
					if (!fromCoopEntity && toCoopPlayer)
					{
						bool fromContainerMenu = ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME);
						bool transferFromContainer = fromContainerMenu || ui->IsMenuOpen(GlobalCoopData::LOOT_MENU);
						if (transferFromContainer)
						{
							if (fromContainerMenu)
							{
								auto mode = ui->GetMenu<RE::ContainerMenu>()->GetContainerMode();
								transferFromContainer = 
								{ 
									mode == RE::ContainerMenu::ContainerMode::kLoot || 
									mode == RE::ContainerMenu::ContainerMode::kPickpocket ||
									mode == RE::ContainerMenu::ContainerMode::kSteal 
								};
							}
							else
							{
								transferFromContainer = true;
							}
						}

						// Move from player 1 to the co-op companion player if looting/stealing/pickpocketing from container or if buying the object from a vendor.
						if (transferFromContainer)
						{
							RE::TESForm* baseObj = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj);
							auto refr = Util::GetRefrPtrFromHandle(a_containerChangedEvent->reference);
							if (toP1)
							{
								const auto& p = glob.coopPlayers[glob.mim->managerMenuCID];
								// Add to co-op player controlling menus if not a shared item.
								if (baseObj && !Util::IsPartyWideItem(baseObj))
								{
									if (auto boundObj = baseObj->As<RE::TESBoundObject>(); boundObj)
									{
										ALYSLC::Log("[Events] Container Changed Event: Removing base item {} (x{}) and giving to {}.", 
											boundObj->GetName(), a_containerChangedEvent->itemCount, p->coopActor->GetName());
										p1->RemoveItem(boundObj, a_containerChangedEvent->itemCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
										// IMPORTANT:
										// Adding the object does not flag the player's inventory as changed and does not trigger the
										// game's equip calculations, which normally clear out the player's currently equipped gear.
										p->coopActor->AddObjectToContainer(boundObj, nullptr, a_containerChangedEvent->itemCount, p->coopActor.get());
									}

									return EventResult::kContinue;
								}
								else if (refr && !Util::IsPartyWideItem(refr.get()))
								{
									if (auto boundObj = refr->GetBaseObject(); boundObj)
									{
										ALYSLC::Log("[Events] Container Changed Event: Removing reference item {} (x{}) and giving to {}.", 
											boundObj->GetName(), a_containerChangedEvent->itemCount, p->coopActor->GetName());
										p1->RemoveItem(boundObj, a_containerChangedEvent->itemCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
										// IMPORTANT:
										// Adding the object does not flag the player's inventory as changed and does not trigger the
										// game's equip calculations, which normally clear out the player's currently equipped gear.
										p->coopActor->AddObjectToContainer(boundObj, nullptr, a_containerChangedEvent->itemCount, p->coopActor.get());
									}

									return EventResult::kContinue;
								}
							}
							else if (toCoopPlayer)
							{
								if (baseObj && Util::IsPartyWideItem(baseObj))
								{
									// Give any looted keys/regular books/notes to player 1, since these items can be tough to find after being (un)intentionally looted by co-op companions.
									if (fromCoopPlayerIndex != -1)
									{
										const auto& p = glob.coopPlayers[fromCoopPlayerIndex];
										RE::TESBoundObject* boundObj = nullptr;
										if (refr)
										{
											boundObj = refr->GetBaseObject();
										}

										if (baseObj && !boundObj)
										{
											boundObj = baseObj->As<RE::TESBoundObject>();
										}

										if (boundObj)
										{
											ALYSLC::Log("[Events] Container Changed Event: Removing item from {} (x{}) to P1 (from 0x{:X}).",
												p->coopActor->GetName(), a_containerChangedEvent->itemCount, a_containerChangedEvent->oldContainer);
											p->coopActor->RemoveItem(boundObj, a_containerChangedEvent->itemCount, RE::ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, p1);
										}

										return EventResult::kContinue;
									}
								}
							}
						}
					}
				}

				bool fromCoopCompanionPlayer = fromCoopPlayerIndex != -1 && !fromP1;
				// A co-op companion player is attempting to gift items to another co-op companion player
				// by way of the GiftMenu through P1. Transfer all items added to P1 to the giftee companion player.
				bool giftMenuOpen = ui && ui->IsMenuOpen(RE::GiftMenu::MENU_NAME);
				if (giftMenuOpen && fromCoopCompanionPlayer && toP1 && glob.mim->IsRunning() && glob.mim->managerMenuCID != -1 && Util::HandleIsValid(glob.mim->gifteePlayerHandle)) 
				{
					const auto& giftingP = glob.coopPlayers[fromCoopPlayerIndex];
					auto gifteePtr = Util::GetActorPtrFromHandle(glob.mim->gifteePlayerHandle);
					if (!gifteePtr || !gifteePtr.get()) 
					{
						return EventResult::kContinue;
					}

					RE::TESForm* baseObj = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj);
					auto refr = Util::GetRefrPtrFromHandle(a_containerChangedEvent->reference);
					RE::TESBoundObject* boundObj = nullptr;
					if (refr)
					{
						boundObj = refr->GetBaseObject();
					}

					if (baseObj && !boundObj)
					{
						boundObj = baseObj->As<RE::TESBoundObject>();
					}

					if (boundObj) 
					{
						ALYSLC::Log("[Events] Container Changed Event: Removing {} (x{}) from P1 to {} (from gifting player {}).",
							boundObj->GetName(), a_containerChangedEvent->itemCount,
							gifteePtr->GetName(), giftingP->coopActor->GetName());
						p1->RemoveItem(boundObj, a_containerChangedEvent->itemCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
						// IMPORTANT:
						// Adding the object does not flag the player's inventory as changed and does not trigger the
						// game's equip calculations, which normally clear out the player's currently equipped gear.
						gifteePtr->AddObjectToContainer(boundObj, nullptr, a_containerChangedEvent->itemCount, gifteePtr.get());
					}

					return EventResult::kContinue;
				}

				bool barterMenuOpen = ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME);
				// Enderal-specific gold scaling and skillbook loot.
				// To a player but not from another player, and not from a transaction (barter menu open).
				if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && !fromCoopEntity && toCoopPlayer && !barterMenuOpen)
				{
					if (auto form = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj); form && form->IsGold())
					{
						// Scale added gold with party size.
						// NOTE: Gold always goes to P1, as P1's gold acts as a shared pool for all players.
						if (Settings::fAdditionalGoldPerPlayerMult > 0.0f)
						{
							int32_t additionalGold = a_containerChangedEvent->itemCount * (glob.activePlayers - 1) * Settings::fAdditionalGoldPerPlayerMult;
							p1->AddObjectToContainer(form->As<RE::TESObjectMISC>(), nullptr, additionalGold, p1);
							bool inMenu = !Util::MenusOnlyAlwaysOpen();
							// If not in a menu and activating all gold in activation range, each individual gold piece added 
							// triggers a container changed event, so the total amount is unspecificed when printing a notification here.
							if (inMenu) 
							{
								RE::DebugNotification(fmt::format("Received an additional {} gold from party size scaling.", additionalGold).c_str());
							}
							else
							{
								RE::DebugNotification(fmt::format("Received an additional gold from party size scaling (x{}).", glob.activePlayers * Settings::fAdditionalGoldPerPlayerMult).c_str());
							}

							return EventResult::kContinue;
						}
					}
					else if (GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.contains(a_containerChangedEvent->baseObj))
					{
						// Give each active player, aside from the player receiving the current skillbook, a random skillbook of the same tier.
						if (Settings::bEveryoneGetsALootedEnderalSkillbook)
						{
							const auto& tierAndSkill = GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.at(a_containerChangedEvent->baseObj);
							const auto& tier = tierAndSkill.first;
							const auto& skill = tierAndSkill.second;
							std::mt19937 generator;
							generator.seed(SteadyClock::now().time_since_epoch().count());

							// REMOVE
							const auto& toP = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(a_containerChangedEvent->newContainer)];
							for (const auto& p : glob.coopPlayers)
							{
								if (p->isActive && p->coopActor->formID != a_containerChangedEvent->newContainer)
								{
									uint32_t numAdded = 0;
									while (numAdded < a_containerChangedEvent->itemCount)
									{
										float rand = static_cast<uint8_t>(GlobalCoopData::ENDERAL_SKILL_TO_SKILLBOOK_INDEX_MAP.size() * (generator() / (float)((std::mt19937::max)())));
										const auto newSkillbookFID = GlobalCoopData::ENDERAL_TIERED_SKILLBOOKS_MAP.at(tier)[rand];
										auto newSkillbook = RE::TESForm::LookupByID(newSkillbookFID);
										p->coopActor->AddObjectToContainer(newSkillbook->As<RE::AlchemyItem>(), nullptr, 1, p->coopActor.get());

										// Show in TrueHUD recent loot widget by adding and removing the skillbook from P1.
										if (ALYSLC::TrueHUDCompat::g_trueHUDInstalled && toP->coopActor.get() == p1)
										{
											p1->AddObjectToContainer(newSkillbook->As<RE::AlchemyItem>(), nullptr, 1, p1);
											p1->RemoveItem(newSkillbook->As<RE::AlchemyItem>(), 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
										}

										RE::DebugNotification(fmt::format("{} received 1 {}.", p->coopActor->GetName(), newSkillbook->GetName()).c_str());
										++numAdded;
									}
								}
							}

							return EventResult::kContinue;
						}
					}
				}
			}
		}

		return EventResult::kContinue;
	}


	CoopDeathEventHandler* CoopDeathEventHandler::GetSingleton() 
	{
		static CoopDeathEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopDeathEventHandler::Register() 
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopDeathEventHandler::GetSingleton());
			logger::info("[Events] Registered for death events.");
		}
		else 
		{ 
			logger::error("[Events] ERR: Could not register for death events."); 
		}
	}

	EventResult CoopDeathEventHandler::ProcessEvent(const RE::TESDeathEvent* a_deathEvent, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		// REMOVE after debugging.
		ALYSLC::Log("[Events] Death Event: {}, killed by death, I mean, erm... by {}. Is dead: {}, essential flag set: {}, {}",
			a_deathEvent->actorDying ? a_deathEvent->actorDying->GetName() : "N/A",
			a_deathEvent->actorKiller ? a_deathEvent->actorKiller->GetName() : "N/A",
			a_deathEvent->actorDying->IsDead(),
			a_deathEvent->actorDying->As<RE::Actor>()->GetActorBase() ?
			a_deathEvent->actorDying->As<RE::Actor>()->GetActorBase()->actorData.actorBaseFlags.all(RE::ACTOR_BASE_DATA::Flag::kEssential) :
			false,
			a_deathEvent->actorDying->As<RE::Actor>()->boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential));

		return EventResult::kContinue;
	}

	CoopEquipEventHandler* CoopEquipEventHandler::GetSingleton()
	{
		static CoopEquipEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopEquipEventHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopEquipEventHandler::GetSingleton());
			logger::info("[Events] Registered for equip events.");
		}
		else 
		{ 
			logger::error("[Events] ERR: Could not register for equip events."); 
		}
	}

	EventResult CoopEquipEventHandler::ProcessEvent(const RE::TESEquipEvent* a_equipEvent, RE::BSTEventSource<RE::TESEquipEvent>*)
	{
		if (glob.coopSessionActive && a_equipEvent && a_equipEvent->baseObject)
		{
			if (auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(a_equipEvent->actor); foundIndex != -1) 
			{
				ALYSLC::Log("[Events] Equip Event: {} -> {} (0x{:X}): equipped: {}, original refr: 0x{:X}, unique id: 0x{:X}",
					(a_equipEvent && a_equipEvent->actor.get()) ? (a_equipEvent->actor->GetName()) : "N/A",
					(a_equipEvent && a_equipEvent->baseObject && RE::TESForm::LookupByID(a_equipEvent->baseObject)) ? RE::TESForm::LookupByID(a_equipEvent->baseObject)->GetName() : "N/A",
					(a_equipEvent && a_equipEvent->baseObject && RE::TESForm::LookupByID(a_equipEvent->baseObject)) ? RE::TESForm::LookupByID(a_equipEvent->baseObject)->formID : 0xDEAD,
					a_equipEvent->equipped,
					a_equipEvent->originalRefr,
					a_equipEvent->uniqueID);

				const auto& p = glob.coopPlayers[foundIndex];
				// Don't handle equip event if the player is not loaded, downed, or dead.
				if (p->coopActor->Is3DLoaded() && !p->isDowned && !p->coopActor->IsDead())
				{
					auto equipForm = RE::TESForm::LookupByID(a_equipEvent->baseObject);
					// Equipped while bartering (with a co-op companion player's inventory copied over to P1).
					// Ignore these equip events and do not refresh equip state.
					bool affectedByInventoryTransfer = 
					{
						RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME) &&
						((glob.mim->managerMenuCID != -1 && p->coopActor == glob.player1Actor) ||
						 (glob.mim->managerMenuCID == p->controllerID))
					};

					// Game will sometimes unequip P1's weapons/magic during killmoves. Don't refresh equip state.
					if ((equipForm && !affectedByInventoryTransfer) && 
						(!p->isPlayer1 || (!p->coopActor->IsInKillMove() && !p->pam->isBeingKillmovedByAnotherPlayer)))
					{
						p->em->RefreshEquipState(RefreshSlots::kAll, equipForm, a_equipEvent->equipped);
					}
				}
			}
		}

		return EventResult::kContinue;
	}

	CoopHitEventHandler* CoopHitEventHandler::GetSingleton() 
	{
		static CoopHitEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopHitEventHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopHitEventHandler::GetSingleton());
			logger::info("[Events] Registered for hit events.");
		}
		else 
		{ 
			logger::error("[Events] ERR: Could not register for onhit events.");
		}
	}

	EventResult CoopHitEventHandler::ProcessEvent(const RE::TESHitEvent* a_hitEvent, RE::BSTEventSource<RE::TESHitEvent>*) 
	{
		if (glob.coopSessionActive && a_hitEvent)
		{
			auto aggressorRefr = a_hitEvent->cause;
			auto hitRefr = a_hitEvent->target;
			if (aggressorRefr && hitRefr && !hitRefr->IsDead())
			{
				// Essential flag gets toggled off when the game is about to killmove the hit actor.
				// Have to set the flag again to prevent P1 and other players from being killed 
				// during a killmove animation while using the revive system.
				if (Settings::bUseReviveSystem) 
				{
					if (auto foundVictimIndex = GlobalCoopData::GetCoopPlayerIndex(hitRefr.get()); foundVictimIndex != -1)
					{
						const auto& p = glob.coopPlayers[foundVictimIndex];
						if (!p->coopActor->IsEssential())
						{
							p->pam->SetEssentialForReviveSystem();
						}
					}
				}

				/*
				// Just realized that spell damage does not level up armor skills.
				// Going to comment this out for now, just in case it's needed later.
				if (auto foundTargetIndex = GlobalCoopData::GetCoopPlayerIndex(hitRefr); foundTargetIndex != -1)
				{
					const auto& p = glob.coopPlayers[foundTargetIndex];
					auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source);
					auto attackingProj = RE::TESForm::LookupByID(a_hitEvent->projectile);
					float damage = 0.0f;
					if (attackingObj)
					{
						if (attackingObj->IsWeapon()) 
						{
							damage = attackingObj->As<RE::TESObjectWEAP>()->GetAttackDamage();
						}
						else if (attackingObj->IsMagicItem() && attackingObj->As<RE::SpellItem>())
						{
							damage = attackingObj->As<RE::SpellItem>()->GetCostliestEffectItem()->GetMagnitude();
						}

						// No spell attack damage event hook.
						// Workaround: use magnitude of spell.
						if (attackingObj->IsMagicItem() && *a_hitEvent->flags != RE::TESHitEvent::Flag::kNone) 
						{
							const auto& armorRatings = p->em->armorRatings;
							float lightArmorBaseXP = damage * armorRatings.first / ((armorRatings.first + armorRatings.second) == 0.0f ? 1.0f : (armorRatings.first + armorRatings.second));
							float heavyArmorBaseXP = damage * armorRatings.second / ((armorRatings.first + armorRatings.second) == 0.0f ? 1.0f : (armorRatings.first + armorRatings.second));

							if (lightArmorBaseXP > 0.0f)
							{
								ALYSLC::Log("[Events] Hit Event: {} was hit by {}'s {}, adding Light Armor Skill XP, base XP: {}.",
									p->coopActor->GetName(),
									aggressorRefr ? aggressorRefr->GetName() : "NONE",
									attackingObj->GetName(),
									lightArmorBaseXP);
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kLightArmor, lightArmorBaseXP);
							}

							if (heavyArmorBaseXP > 0.0f)
							{
								ALYSLC::Log("[Events] Hit Event: {} was hit by {}'s {}, adding Heavy Armor Skill XP, base XP: {}.",
									p->coopActor->GetName(),
									aggressorRefr ? aggressorRefr->GetName() : "NONE",
									attackingObj->GetName(),
									heavyArmorBaseXP);
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kHeavyArmor, heavyArmorBaseXP);
							}
						}
					}
				}
				*/

				// Do not add XP if attacking another player or player teammate.
				if (auto foundAggressorIndex = GlobalCoopData::GetCoopPlayerIndex(aggressorRefr); foundAggressorIndex != -1)
				{
					const auto& p = glob.coopPlayers[foundAggressorIndex];

					// If a non-P1 player hit this refr, send a duplicate hit event with P1 as the aggressor
					// to trigger any OnHit events linked with P1 hitting the refr.
					if (!p->isPlayer1)
					{
						// Check placeholder spells for the source FID, and if found, send the copied spell's FID instead.
						RE::FormID sourceFID = a_hitEvent->source;
						if (auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source); attackingObj && attackingObj->As<RE::SpellItem>())
						{
							for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i)
							{
								if (p->em->placeholderMagic[i] && p->em->placeholderMagic[i]->formID == a_hitEvent->source && p->em->copiedMagic[i])
								{
									sourceFID = p->em->copiedMagic[i]->formID;
									break;
								}
							}
						}

						// Remove sneak attack flag, if any, to prevent the new P1 hit event from triggering a sneak attack.
						auto flags = a_hitEvent->flags;
						if (flags.all(RE::TESHitEvent::Flag::kSneakAttack))
						{
							flags.reset(RE::TESHitEvent::Flag::kSneakAttack);
						}

						Util::SendHitEvent(glob.player1Actor.get(), a_hitEvent->target.get(), sourceFID, a_hitEvent->projectile, flags);
					}

					// Handle hit event for actor hit by player.
					if (auto hitActor = hitRefr->As<RE::Actor>(); hitActor) 
					{
						auto projectileRefr = RE::TESForm::LookupByID(a_hitEvent->projectile);
						if (!hitActor->IsPlayerTeammate() && !hitActor->IsPlayerRef())
						{
							// Start combat between hit NPC and co-op companion player.
							// Start assault alarm on P1 to trigger bounty if the hit actor
							// is not angry with the player and is not in combat currently.

							float detectionPct = (std::clamp(static_cast<float>(hitActor->RequestDetectionLevel(p->coopActor.get())), -20.0f, 0.0f) + 20.0f) * 5.0f;
							if ((detectionPct > 0.0f) && (!hitActor->IsHostileToActor(p->coopActor.get()) && !hitActor->IsInCombat()))
							{
								// Caused by player + projectile is the hit actor (splat) or hit event projectile's form type is not projectile (bonk).
								bool isBonkOrSplatHitEvent = (a_hitEvent->cause == p->coopActor) && 
															 ((a_hitEvent->projectile == hitActor->formID) || 
															  (projectileRefr && !projectileRefr->As<RE::BGSProjectile>()));
								if (!isBonkOrSplatHitEvent || !p->isInGodMode) 
								{
									Util::Papyrus::StartCombat(hitActor, p->coopActor.get());
									Util::Papyrus::SendAssaultAlarm(hitActor);
								}
							}
						}

						if (p->IsRunning())
						{
							auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source); 
							const float damageMult = p->pam->reqDamageMult;

							// Grant co-op companion XP.
							// Do not give attacking player XP if hitting another player while they are in god mode.
							const auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(hitActor);
							bool victimPlayerInGodMode = playerVictimIndex != -1 && glob.coopPlayers[playerVictimIndex]->isInGodMode;
							// Also, do not give XP if friendly fire is disabled and hitting a friendly actor.
							bool canGrantXP = !victimPlayerInGodMode && (Settings::vbFriendlyFire[p->controllerID] || !Util::IsPartyFriendlyActor(hitActor));
							if (!p->isPlayer1 && canGrantXP)
							{
								auto weap = attackingObj ? attackingObj->As<RE::TESObjectWEAP>() : nullptr;
								// Add XP to attacking player.
								if (weap)
								{
									if (weap->IsRanged() && !weap->IsStaff())
									{
										ALYSLC::Log("[Events] Hit Event: Adding {} XP to {}, base XP: {}, weapon: {}",
											RE::ActorValue::kArchery, p->coopActor->GetName(), weap->GetAttackDamage(), weap->GetName());
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kArchery, weap->GetAttackDamage());
									}
									else if (weap->IsOneHandedAxe() || weap->IsOneHandedDagger() || weap->IsOneHandedMace() || weap->IsOneHandedSword())
									{
										ALYSLC::Log("[Events] Hit Event: Adding {} XP to {}, base XP: {}, weapon: {}",
											RE::ActorValue::kOneHanded, p->coopActor->GetName(), weap->GetAttackDamage(), weap->GetName());
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kOneHanded, weap->GetAttackDamage());
									}
									else if (weap->IsTwoHandedAxe() || weap->IsTwoHandedSword())
									{
										ALYSLC::Log("[Events] Hit Event: Adding {} XP to {}, base XP: {}, weapon: {}",
											RE::ActorValue::kTwoHanded, p->coopActor->GetName(), weap->GetAttackDamage(), weap->GetName());
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kTwoHanded, weap->GetAttackDamage());
									}
								}

								// Block XP.
								if (p->pam->isBashing && p->em->HasShieldEquipped())
								{
									ALYSLC::Log("[Events] Hit Event: Adding 5 XP to {}'s Block Skill for a successful shield bash", p->coopActor->GetName());
									GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kBlock, 5.0f);
								}

								// Print sneak attack message for co-op player if needed.
								auto magicItem = attackingObj ? attackingObj->As<RE::MagicItem>() : nullptr;
								bool isSneakAttack = (damageMult > 1.0f && ((weap || magicItem) && p->coopActor->IsWeaponDrawn() && p->coopActor->IsSneaking()));
								// Sneak message and XP.
								if (isSneakAttack)
								{
									const auto sneakMsg = fmt::format("{} performed a sneak attack for {:.1f}x damage!", p->coopActor->GetName(), damageMult);
									RE::DebugNotification(sneakMsg.data(), "UISneakAttack", true);
									if ((weap && weap->IsRanged()) || magicItem)
									{
										ALYSLC::Log("[Events] Hit Event: Adding 2.5 XP to {}'s Sneak Skill for a successful ranged sneak attack with {} (0x{:X}, {})",
											p->coopActor->GetName(),
											weap ? weap->GetName() : magicItem ? magicItem->GetName() :   "NONE",
											weap ? weap->formID : magicItem ? magicItem->formID : 0xDEAD,
											weap ? "weapon" : magicItem ? "magic item" : "");
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kSneak, 2.5f);
									}
									else
									{
										ALYSLC::Log("[Events] Hit Event: Adding 30 XP to {}'s Sneak Skill for a successful melee sneak attack with {} (0x{:X}, {})",
											p->coopActor->GetName(),
											weap ? weap->GetName() : "fists",
											weap ? weap->formID : 0xDEAD,
											weap ? RE::FormType::Weapon : attackingObj ? *attackingObj->formType :  RE::FormType::None);
										GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kSneak, 30.0f);
									}

									Util::SendCriticalHitEvent(p->coopActor.get(), weap, true);
								}
							}

							// Thrown object sneak attacks (for all players).
							if (canGrantXP && a_hitEvent->flags.any(RE::TESHitEvent::Flag::kSneakAttack) && projectileRefr && !projectileRefr->As<RE::BGSProjectile>())
							{
								ALYSLC::Log("[Events] Hit Event: Adding 2.5 XP to {}'s Sneak Skill for a successful thrown object sneak attack with {} (0x{:X}, {})",
									p->coopActor->GetName(), projectileRefr->GetName(), projectileRefr->formID, *projectileRefr->formType);
								GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kSneak, 2.5f);
								const auto sneakMsg = fmt::format("{} performed a sneak attack for 2.0x damage!", p->coopActor->GetName());
								RE::DebugNotification(sneakMsg.data(), "UISneakAttack", true);
								Util::SendCriticalHitEvent(p->coopActor.get(), nullptr, true);
							}
						}
					}
				}
			}
		}

		return EventResult::kContinue;
	}

	CoopLoadGameEventHandler* CoopLoadGameEventHandler::GetSingleton()
	{
		static CoopLoadGameEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopLoadGameEventHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopLoadGameEventHandler::GetSingleton());
			logger::info("[Events] Registered for load game events.");
		}
		else
		{
			logger::error("[Events] ERR: Could not register for load game events.");
		}
	}

	EventResult CoopLoadGameEventHandler::ProcessEvent(const RE::TESLoadGameEvent* a_loadGameEvent, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		// Teardown any active co-op session when P1 loads a save.
		if (glob.coopSessionActive && a_loadGameEvent)
		{
			GlobalCoopData::TeardownCoopSession(false);
		}

		return EventResult::kContinue;
	}

	CoopMenuOpenCloseHandler* CoopMenuOpenCloseHandler::GetSingleton() 
	{
		static CoopMenuOpenCloseHandler singleton;
		return std::addressof(singleton);
	}

	void CoopMenuOpenCloseHandler::Register() 
	{
		auto ui = RE::UI::GetSingleton();
		if (ui) 
		{
			ui->AddEventSink(CoopMenuOpenCloseHandler::GetSingleton());
			logger::info("[Events] Registered for menu open/close events.");
		}
		else
		{ 
			logger::error("[Events] ERR: Could not register for menu open/close events."); 
		}
	}

	EventResult CoopMenuOpenCloseHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_menuEvent, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) 
	{
		if (glob.globalDataInit) 
		{
			bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();

			// REMOVE
			ALYSLC::Log("[Events] Menu Open/Close Event: menu name {}, {}, menu CIDs: current {}, prev: {}, manager: {}, empty data: {}. Only always open: {}.",
				a_menuEvent->menuName, 
				a_menuEvent->opening ? "OPENING" : "CLOSING",
				glob.menuCID,
				glob.prevMenuCID,
				glob.mim->managerMenuCID,
				glob.serializablePlayerData.empty(),
				onlyAlwaysOpen);
			
			// Have to modify P1's level threshold each time the Stats menu opens and the LevelUp menu closes if changes were made to the XP threshold mult.
			// Modifying the threshold right as the LevelUp menu opens will not always see changes reflected in P1's XP total 
			// even after P1's current level increases by 1, so we have to do it after the menu closes and the game is fully done leveling up P1.
			if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled && Settings::fLevelUpXPThresholdMult != 1.0f) 
			{
				bool statsMenuEvent = Hash(a_menuEvent->menuName) == Hash(RE::StatsMenu::MENU_NAME);
				bool levelupMenuEvent = Hash(a_menuEvent->menuName) == Hash(RE::LevelUpMenu::MENU_NAME);
				if (auto p1 = RE::PlayerCharacter::GetSingleton(); (p1) && ((levelupMenuEvent && !a_menuEvent->opening) || (statsMenuEvent && a_menuEvent->opening)))
				{
					// REMOVE
					float playerXP = p1->skills->data->xp;
					float playerXPThreshold = p1->skills->data->levelThreshold;
					float fXPLevelUpMult = 25.0f;
					float fXPLevelUpBase = 75.0f;
					auto valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult");
					if (valueOpt.has_value())
					{
						fXPLevelUpMult = valueOpt.value();
					}

					if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase"); valueOpt.has_value())
					{
						fXPLevelUpBase = valueOpt.value();
					}

					// REMOVE
					ALYSLC::Log("[Events] Menu Open/Close Event: BEFORE: {} menu {}. P1 XP: {}, XP threshold: {}, XP mult/base: {}, {}",
						levelupMenuEvent ? "LevelUp" : "Stats",
						a_menuEvent->opening ? "opening" : "closing", 
						playerXP, playerXPThreshold,
						fXPLevelUpMult, fXPLevelUpBase);

					GlobalCoopData::ModifyLevelUpXPThreshold(glob.coopSessionActive);

					// REMOVE
					playerXP = p1->skills->data->xp;
					playerXPThreshold = p1->skills->data->levelThreshold;
					valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult");
					if (valueOpt.has_value())
					{
						fXPLevelUpMult = valueOpt.value();
					}

					if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase"); valueOpt.has_value())
					{
						fXPLevelUpBase = valueOpt.value();
					}

					// REMOVE
					ALYSLC::Log("[Events] Menu Open/Close Event: AFTER: {} menu {}. P1 XP: {}, XP threshold: {}, XP mult/base: {}, {}",
						levelupMenuEvent ? "LevelUp" : "Stats",
						a_menuEvent->opening ? "opening" : "closing", 
						playerXP, playerXPThreshold,
						fXPLevelUpMult, fXPLevelUpBase);
				}
			}

			//=======================================
			// Special processing for specific menus:
			//=======================================

			const auto ui = RE::UI::GetSingleton();
			auto msgQ = RE::UIMessageQueue::GetSingleton();
			if (ui && glob.allPlayersInit && a_menuEvent->opening && Hash(a_menuEvent->menuName) == Hash(RE::LoadingMenu::MENU_NAME))
			{
				// Close message box menu ('You died' message).
				if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME)) 
				{
					if (msgQ)
					{
						msgQ->AddMessage(RE::MessageBoxMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
					}
				}

				if (glob.coopSessionActive) 
				{
					// Prevents 2H weapon animation stuttering bug due to corrupted equip slots.
					for (const auto& p : glob.coopPlayers)
					{
						if (p->isActive && !p->isPlayer1 && p->coopActor->currentProcess)
						{
							p->pam->ReadyWeapon(false);
							p->em->ReEquipHandForms();
						}
					}
				}
			}

			if (ui && glob.coopSessionActive && a_menuEvent)
			{
				// REMOVE when done debugging.
				ALYSLC::Log("[Events] ===========[Menu Map BEGIN]===========");
				for (auto& menu : ui->menuMap)
				{
					if (ui->IsMenuOpen(menu.first))
					{
						ALYSLC::Log("[Events] Menu {} is open. Pauses game: {}, always open: {}. Flags: 0b{:B}.", 
							menu.first,
							menu.second.menu->PausesGame(),
							menu.second.menu->AlwaysOpen(),
							*menu.second.menu->menuFlags);
					}
				}

				ALYSLC::Log("[Events] ===========[Menu Map END]===========");

				ALYSLC::Log("[Events] ===========[Menu Stack BEGIN]===========");

				for (auto iter = ui->menuStack.begin(); iter != ui->menuStack.end(); ++iter)
				{
					const auto& menu = *iter;
					for (const auto& [name, menuEntry] : ui->menuMap)
					{
						if (menuEntry.menu == menu)
						{
							ALYSLC::Log("[Events] Index {}: Menu {} is open. Pauses game: {}, always open: {}. Flags: 0b{:B}.",
								iter - ui->menuStack.begin(),
								name,
								menu->PausesGame(),
								menu->AlwaysOpen(),
								*menu->menuFlags);
						}
					}
				}

				ALYSLC::Log("[Events] ===========[Menu Stack END]===========");

				if (auto controlMap = RE::ControlMap::GetSingleton(); controlMap) 
				{
					ALYSLC::Log("[Events] ===========[Menu Priority Stack BEGIN]===========");

					for (auto i = 0; i < controlMap->contextPriorityStack.size(); ++i)
					{
						const auto& context = controlMap->contextPriorityStack[i];
						ALYSLC::Log("[Events] Index {}: context: {}.", i, context);
					}

					ALYSLC::Log("[Events] ===========[Menu Priority Stack END]===========");
				}

				// Open the ALYSLC overlay if it isn't open already.
				if (!ui->IsMenuOpen(DebugOverlayMenu::MENU_NAME))
				{
					ALYSLC::Log("[Events] Menu Open/Close Event: ALYSLC overlay not open. Opening.");
					DebugOverlayMenu::Load();
				}

				// NOTE: May not be necessary anymore.
				// Ensure HUD stays open.
				const auto& hudMenu = ui->GetMenu<RE::HUDMenu>();
				if (!ui->IsMenuOpen(RE::HUDMenu::MENU_NAME) || (hudMenu && hudMenu->uiMovie && !hudMenu->uiMovie->GetVisible()))
				{
					if (msgQ)
					{
						msgQ->AddMessage(RE::HUDMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
					}

					if (hudMenu) 
					{
						hudMenu->uiMovie->SetVisible(true);
					}
				}

				// [Enderal]: Keep perks synced among all players whenever a MessageBox menu opens/closes.
				if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && Hash(a_menuEvent->menuName) == Hash(RE::MessageBoxMenu::MENU_NAME))
				{
					GlobalCoopData::SyncSharedPerks();
				}

				// Reset dialogue control CID when dialogue menu opens and closes.
				if (Hash(a_menuEvent->menuName) == Hash(RE::DialogueMenu::MENU_NAME))
				{
					const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
					{
						std::unique_lock<std::mutex> lock(glob.moarm->reqDialogueControlMutex, std::try_to_lock);
						if (lock)
						{
							ALYSLC::Log("[Events] Menu Open/Close Event: Dialogue menu CID. Lock obtained. (0x{:X})", hash);
							glob.moarm->reqDialoguePlayerCID = -1;
						}
						else
						{
							ALYSLC::Log("[Events] Menu Open/Close Event: Dialogue menu CID. Failed to obtain lock. (0x{:X})", hash);
						}
					}
				}

				// Record changes in open/close state for supported menus.
				bool wasSupportedMenuOpen = glob.supportedMenuOpen;
				glob.supportedMenuOpen = GlobalCoopData::IsSupportedMenuOpen();
				if (wasSupportedMenuOpen && !glob.supportedMenuOpen.load())
				{
					glob.lastSupportedMenusClosedTP = SteadyClock::now();
				}

				//========================================================
				// Check for co-op companion player menu control requests.
				//========================================================

				if (a_menuEvent->opening)
				{
					// Resolve the CID which will modify the requests queue and clear out fulfilled requests
					// even if we don't need to set the menu CID to the resolved CID.
					auto resolvedCID = glob.moarm->ResolveMenuControllerID(a_menuEvent->menuName);
					// REMOVE prints when done debugging.
					if (glob.mim->IsRunning() && glob.mim->managerMenuCID != -1)
					{
						GlobalCoopData::SetMenuCIDs(glob.mim->managerMenuCID);
						ALYSLC::Log("[Events] Menu Open/Close Event: OPENING: Set menu CIDs. Menu input manager running. CIDs are now: {}, {}, {}.", 
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);
					}
					else if (glob.menuCID == -1)
					{
						GlobalCoopData::SetMenuCIDs(resolvedCID);
						ALYSLC::Log("[Events] Menu Open/Close Event: OPENING: Set menu CIDs. Resolve CID from requests. MIM not running. CIDs are now: {}, {}, {}.",
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);
					}
					else
					{
						ALYSLC::Log("[Events] Menu Open/Close Event: OPENING: Set menu CIDs. Menu CID is already set. MIM not running. CIDs are now: {}, {}, {}.",
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);
					}
				}

				// Co-op player requesting menu control.
				if (glob.menuCID != -1 && glob.menuCID != glob.player1CID)
				{
					const auto& p = glob.coopPlayers[glob.menuCID];
					if (bool isSupportedMenu = glob.SUPPORTED_MENU_NAMES.contains(std::string_view(a_menuEvent->menuName.c_str())); isSupportedMenu)
					{
						auto menuNameHash = Hash(a_menuEvent->menuName);
						// Companion player's inventory menu (Container Menu) is open and the Tween Menu is already open.
						// Close Tween Menu since it will not auto-close after the Container Menu closes.
						if (!a_menuEvent->opening && menuNameHash == Hash(RE::ContainerMenu::MENU_NAME) && ui->IsMenuOpen(RE::TweenMenu::MENU_NAME))
						{
							if (msgQ)
							{
								msgQ->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kForceHide, nullptr);
							}
						}

						// Set newly opened menu in the menu input manager.
						glob.mim->SetOpenedMenu(a_menuEvent->menuName, a_menuEvent->opening);
						ALYSLC::Log("[Events] Menu Open/Close Event: Menu {} {} by CID {}, menus open: {}", 
							a_menuEvent->menuName, 
							a_menuEvent->opening ? "opened" : "closed", 
							glob.menuCID, 
							glob.mim->managedCoopMenusCount);

						// NOTE: Don't know of a way to hook ProcessMessage() for custom menus, so we'll copy player data here instead.
						// Must have Maxsu2017's awesome 'Hero Menu Enhanced' mod installed:
						// https://www.nexusmods.com/enderalspecialedition/mods/563
						if (menuNameHash == Hash(GlobalCoopData::ENHANCED_HERO_MENU))
						{
							if (a_menuEvent->opening)
							{
								ALYSLC::Log("[Events] Menu Open/Close Event: Enderal Hero Menu: Should copy AVs and name.");
								if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
								{
									GlobalCoopData::CopyOverActorBaseData(p->coopActor.get(), true, true, false, false);
									glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kName);
								}

								if (!glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
								{
									GlobalCoopData::CopyOverAVs(p->coopActor.get(), true);
									glob.copiedPlayerDataTypes.set(CopyablePlayerDataTypes::kSkillsAndHMS);
								}
							}
							else
							{
								ALYSLC::Log("[Events] Menu Open/Close Event: Enderal Hero Menu: Should copy back AVs and name.");
								if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kName))
								{
									GlobalCoopData::CopyOverActorBaseData(p->coopActor.get(), false, true, false, false);
									glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kName);
								}

								if (glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS))
								{
									GlobalCoopData::CopyOverAVs(p->coopActor.get(), false);
									glob.copiedPlayerDataTypes.reset(CopyablePlayerDataTypes::kSkillsAndHMS);
								}
							}
						}
						else if (menuNameHash == Hash(RE::CraftingMenu::MENU_NAME))
						{
							// Stop co-op companion from interacting with crafting station once the menu opens.
							auto defPackage = p->pam->GetDefaultPackage();
							p->pam->SetCurrentPackage(defPackage);
							p->pam->EvaluatePackage();
						}

						// Give co-op companion player menu control.
						if (a_menuEvent->opening)
						{
							glob.mim->ToggleCoopPlayerMenuMode(glob.menuCID);
						}
						//else if (glob.mim->managedCoopMenusCount == 0 && !menuPausesListeners && glob.mim->IsRunning())
						else if (glob.mim->managedCoopMenusCount == 0 && glob.mim->IsRunning())
						{
							glob.mim->ToggleCoopPlayerMenuMode(-1);
						}
					}
					else if (Hash(a_menuEvent->menuName) == Hash(RE::LockpickingMenu::MENU_NAME))
					{
						// Notify player to perform lockpicking task to give them LockpickingMenu control.
						p->taskRunner->AddTask([&p]() { p->LockpickingTask(); });
					}
				}
				else if (a_menuEvent->opening && Hash(a_menuEvent->menuName) == Hash(RE::FavoritesMenu::MENU_NAME))
				{
					// Update quick slot item/spell entry texts for player 1 if they are accessing the favorites menu.
					glob.mim->InitP1QSFormEntries();
				}

				if (!a_menuEvent->opening)
				{
					// Reset menu controller ID once all menus are closed.
					if (onlyAlwaysOpen && glob.menuCID != -1)
					{
						GlobalCoopData::ResetMenuCIDs();
						ALYSLC::Log("[Events] Menu Open/Close Event: CLOSING: MIM signalled to close. Reset menu CIDs. CIDs are now: {}, {}, {}.",
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);
					}
					else if (!onlyAlwaysOpen && glob.mim->managedCoopMenusCount == 0 && glob.menuCID != glob.player1CID)
					{
						// Co-op companion player relinquishes control of menus but at least one is still open, so give control to P1.
						GlobalCoopData::ResetMenuCIDs();
						ALYSLC::Log("[Events] Menu Open/Close Event: CLOSING: MIM signalled to close with controllable menus open. Give control to P1. CIDs are now: {}, {}, {}.",
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);
					}
					else
					{
						// Catch-all case (may not need handling).
						ALYSLC::Log("[Events] Menu Open/Close Event: CLOSING: Fallthrough: only always open: {}, MIM managed menus count: {}. CIDs are now: {}, {}, {}.",
							onlyAlwaysOpen,
							glob.mim->managedCoopMenusCount,
							glob.menuCID,
							glob.prevMenuCID,
							glob.mim->managerMenuCID);

						// If no menus are open, the managed menus count should be 0.
						// Stop menu input manager and reset menu controller IDs if not.
						if (onlyAlwaysOpen && glob.mim->managedCoopMenusCount > 0) 
						{
							ALYSLC::Log("[Events] ERR: Menu Open/Close Event: CLOSING: MIM managed menus count should be 0 here. Pausing menu input manager and resetting menu CIDs.");
							glob.mim->ToggleCoopPlayerMenuMode(-1);
							GlobalCoopData::ResetMenuCIDs();
						}
					}
				}
			}
			else if (glob.mim->IsRunning() && glob.mim->managedCoopMenusCount > 0)
			{
				// Prevents open menus count from being positive once the session ends,
				// a bug which can be produced by opening the Summoning Menu with
				// a co-op player-controlled menu open (e.g. Dialogue menu).
				// This bug will keep the menu input manager running even without
				// any co-op player-controlled menus open and lead to co-op player
				// inputs affecting player 1 due to emulated keypresses.
				// Give P1 control here.
				Events::ResetMenuState();
			}
		}

		return EventResult::kContinue;
	}

	CoopCellFullyLoadedHandler* CoopCellFullyLoadedHandler::GetSingleton()
	{
		static CoopCellFullyLoadedHandler singleton;
		return std::addressof(singleton);
	}

	void CoopCellFullyLoadedHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			scriptEventSourceHolder->AddEventSink(CoopCellFullyLoadedHandler::GetSingleton());
			logger::info("[Events] Registered for cell fully loaded events.");
		}
		else
		{
			logger::error("[Events] ERR: Could not register for cell fully loaded events.");
		}
	}

	EventResult CoopCellFullyLoadedHandler::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_cellFullyLoadedEvent, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
	{
		if (a_cellFullyLoadedEvent && a_cellFullyLoadedEvent->cell) 
		{
			const uint32_t p1CellNameHash = Hash(a_cellFullyLoadedEvent->cell->fullName);
			if (p1CellNameHash != lastLoadP1CellNameHash)
			{
				lastLoadP1CellNameHash = p1CellNameHash;

				if (ALYSLC::MiniMapCompat::g_miniMapInstalled && (Settings::bRemoveExteriorOcclusion && a_cellFullyLoadedEvent->cell->IsExteriorCell()) || (Settings::bRemoveInteriorOcclusion && a_cellFullyLoadedEvent->cell->IsInteriorCell())) 
				{
					ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround = false;
					// Persistent refrs: only handle room markers, since portal markers and occlusion planes are not persistent.
					for (auto obj : a_cellFullyLoadedEvent->cell->objectList)
					{
						// If this cell has a room marker, signal culling proc to apply freeze workaround if using the MiniMap mod.
						if (obj->GetBaseObject() && obj->GetBaseObject()->formID == 0x1F)
						{
							ALYSLC::MiniMapCompat::g_shouldApplyCullingWorkaround = true;
							break;
						}
					}
				}
			}

			Util::ResetFadeOnAllObjectsInCell(a_cellFullyLoadedEvent->cell);
		}

		return EventResult::kContinue;
	}

	CoopPositionPlayerEventHandler* CoopPositionPlayerEventHandler::GetSingleton()
	{
		static CoopPositionPlayerEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopPositionPlayerEventHandler::Register()
	{
		if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			p1->AddEventSink(CoopPositionPlayerEventHandler::GetSingleton());
			logger::info("[Events] Registered for position player events.");
		}
		else
		{
			logger::error("[Events] ERR: Could not register for position player events.");
		}
	}

	EventResult CoopPositionPlayerEventHandler::ProcessEvent(const RE::PositionPlayerEvent* a_positionPlayerEvent, RE::BSTEventSource<RE::PositionPlayerEvent>* a_eventSource)
	{
		// NOTE: Needs testing.
		// Would like to see if this event fires when the player validity checks in the player manager do
		// not signal to move co-op companion players to P1. 
		// Examples where this could occur include short teleports with a fader menu opening and closing.
		// Used to clear grabbed actors/released refrs when before P1 moves
		// and then move companion players to P1 when P1 is finished moving.
		
		if (!glob.globalDataInit || !glob.coopSessionActive)
		{
			return EventResult::kContinue;
		}

		auto tes = RE::TES::GetSingleton(); 
		if (!tes || !a_positionPlayerEvent)
		{
			return EventResult::kContinue;
		}

		ALYSLC::Log("[Events] Position Player Event: {}. Should move players to P1: {}.",
			*a_positionPlayerEvent->type,
			a_positionPlayerEvent->type.any(RE::PositionPlayerEvent::EVENT_TYPE::kFinish));
		if (a_positionPlayerEvent->type.any(RE::PositionPlayerEvent::EVENT_TYPE::kPre, RE::PositionPlayerEvent::EVENT_TYPE::kPreUpdatePackages)) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive) 
				{
					if (p->IsRunning()) 
					{
						ALYSLC::Log("[Events] Position Player Event: P{}'s managers are running. Pausing now before P1 moves.", p->playerID + 1);
						p->RequestStateChange(ManagerState::kPaused);
					}

					// Clear all released refrs and grabbed actors,
					// since I haven't figured out a consistent, bug-free way
					// of moving grabbed actors through load doors.
					p->tm->rmm->ClearReleasedRefrs();
					p->tm->rmm->ClearGrabbedActors(p);
				}
			}
		}

		if (a_positionPlayerEvent->type.any(RE::PositionPlayerEvent::EVENT_TYPE::kFinish)) 
		{
			auto p1 = RE::PlayerCharacter::GetSingleton();
			bool player1Valid = 
			{
				p1 && !p1->IsDisabled() && p1->Is3DLoaded() && p1->IsHandleValid() &&
				p1->loadedData && p1->currentProcess && p1->GetCharController() && p1->parentCell
			};
			if (!player1Valid) 
			{
				ALYSLC::Log("[Events] Position Player Event: ERR: P1 is not valid after event finished.");
				return EventResult::kContinue;
			}

			for (const auto& p : glob.coopPlayers) 
			{
				if (p->isActive)
				{
					if (p->IsRunning())
					{
						ALYSLC::Log("[Events] Position Player Event: P{}'s managers are running. Pausing now after P1 moved.", p->playerID + 1);
						p->RequestStateChange(ManagerState::kPaused);
					}

					if (!p->isPlayer1) 
					{
						ALYSLC::Log("[Events] Position Player Event: Moving player {} to P1.", p->coopActor->GetName());
						p->pam->ReadyWeapon(false);
						p->coopActor->MoveTo(p1);
					}

					// Also move all the player's grabbed objects to P1.
					p->tm->rmm->MoveUnloadedGrabbedObjectsToPlayer(p);
				}
			}
		}

		return EventResult::kContinue;
	}
}

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
		// Register crosshair event handler.
		CoopCrosshairEventHandler::Register();
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

		// For debugging only as of now:
		// Register actor kill event handler.
		//CoopActorKillEventHandler::Register();
		// Register combat event handler.
		//CoopCombatEventHandler::Register();
		// Register death event handler.
		//CoopDeathEventHandler::Register();

		SPDLOG_INFO("[Events] RegisterEvents: event registration complete.");
	}

	void Events::ResetMenuState()
	{
		// Reset our handled menu data instantly:
		// Stop MIM, reset menu controller IDs,
		// set supported menus as closed.

		glob.mim->ToggleCoopPlayerMenuMode(-1);
		GlobalCoopData::ResetMenuCIDs();
		glob.supportedMenuOpen.store(false);
		glob.lastSupportedMenusClosedTP = SteadyClock::now();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Ensure the allow saving flag is set for the HUD Menu,
		// since we may have unset it previously, preventing all saving.
		if (p1 && *glob.copiedPlayerDataTypes == CopyablePlayerDataTypes::kNone)
		{
			p1->byCharGenFlag = RE::PlayerCharacter::ByCharGenFlag::kNone;
		}
	}

	CoopActorKillEventHandler* CoopActorKillEventHandler::GetSingleton()
	{
		static CoopActorKillEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopActorKillEventHandler::Register()
	{
		auto source = RE::ActorKill::GetEventSource();
		if (source)
		{
			auto singleton = GetSingleton();
			source->AddEventSink(singleton);
			SPDLOG_INFO("[Events] Registered for actor kill events.");

			// Find our added sink.
			int32_t sinkIndex = -1;
			for (auto i = 0; i < source->sinks.size(); ++i)
			{
				if (source->sinks[i] == singleton)
				{
					sinkIndex = i;
				}
			}

			if (sinkIndex == -1)
			{
				SPDLOG_ERROR("[Events] ERR: Could not get registered actor kill event sink.");
			}
			else
			{
				SPDLOG_DEBUG("[Events] Actor kill event sink found at index {}.", sinkIndex);
				// Move our sink to the front so it processes events first.
				for (auto i = 0; i < sinkIndex; ++i)
				{
					source->sinks[i + 1] = source->sinks[i]; 
				}

				source->sinks[0] = singleton;
			}
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for actor kill events."); 
		}
	}

	EventResult CoopActorKillEventHandler::ProcessEvent
	(
		const RE::ActorKill::Event* a_actorKillEvent, RE::BSTEventSource<RE::ActorKill::Event>*
	)
	{
		// NOTE:
		// Purely for debugging purposes right now.
		// May replace DeathEvent handling eventually.

		SPDLOG_DEBUG
		(
			"[Events] Actor kill event: {} ({}) killed {}.",
			a_actorKillEvent->killer ? a_actorKillEvent->killer->GetName() : "NONE",
			a_actorKillEvent->victim &&
			a_actorKillEvent->victim->myKiller ? 
			a_actorKillEvent->victim->myKiller.get()->GetName() : 
			"NONE",
			a_actorKillEvent->victim ? a_actorKillEvent->victim->GetName() : "NONE"
		);

		return EventResult::kContinue;
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
			SPDLOG_INFO("[Events] Registered for bleedout events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for bleedout events.");
		}
	}

	EventResult CoopBleedoutEventHandler::ProcessEvent
	(
		const RE::TESEnterBleedoutEvent* a_bleedoutEvent,
		RE::BSTEventSource<RE::TESEnterBleedoutEvent>*
	)
	{
		if (!glob.allPlayersInit || 
			!Settings::bUseReviveSystem ||
			!a_bleedoutEvent || 
			!a_bleedoutEvent->actor)
		{
			return EventResult::kContinue;
		}

		if (glob.livingPlayers == 0 && a_bleedoutEvent->actor->IsPlayerRef())
		{
			// All companion players are dead and P1 has just been downed.
			// Unset essential flag to allow for player death.
			auto p1 = a_bleedoutEvent->actor->As<RE::Actor>();
			Util::ChangeEssentialStatus(p1, false);
			// Set to zero health, which somtimes triggers a death event.
			p1->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage,
				RE::ActorValue::kHealth,
				-p1->GetActorValue(RE::ActorValue::kHealth)
			);
		}
		else if (auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(a_bleedoutEvent->actor); 
				 foundIndex != -1)
		{
			// Prevent deferred kill from triggering.
			// Might remove if not necessary.
			const auto& p = glob.coopPlayers[foundIndex];
			if (p->coopActor->currentProcess && p->coopActor->currentProcess->middleHigh)
			{
				auto midHighProc = p->coopActor->currentProcess->middleHigh;
				midHighProc->deferredKillTimer = FLT_MAX;
			}

			return EventResult::kStop;
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
			SPDLOG_INFO("[Events] Registered for cell change events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for cell change events.");
		}
	}

	EventResult CoopCellChangeHandler::ProcessEvent
	(
		const RE::TESMoveAttachDetachEvent* a_cellChangeEvent,
		RE::BSTEventSource<RE::TESMoveAttachDetachEvent>*
	)
	{
		if (!glob.globalDataInit ||
			!a_cellChangeEvent || 
			!a_cellChangeEvent->movedRef)
		{
			return EventResult::kContinue;
		}

		auto attachedRefrPtr = a_cellChangeEvent->movedRef;
		if (!attachedRefrPtr)
		{
			return EventResult::kContinue;
		}
		
		if (auto refr3DPtr = Util::GetRefr3D(attachedRefrPtr.get()); refr3DPtr)
		{
			// Ensure actor does not fade during co-op with our co-op cam enabled.
			if (!refr3DPtr->flags.all
				(
					RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
				))
			{
				refr3DPtr->flags.set
				(
					RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
				);
				RE::NiUpdateData updateData{ };
				refr3DPtr->UpdateDownwardPass(updateData, 0);
			}
		}

		SPDLOG_DEBUG("[Events] Cell change event: {} {}.",
			attachedRefrPtr->GetName(),
			a_cellChangeEvent->isCellAttached ? "attached" : "detached");
		auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(attachedRefrPtr);
		if (foundIndex != -1)
		{
			const auto& p = glob.coopPlayers[foundIndex];
			// Prevent equip state bug mentioned in the equip manager
			// where two handed weapons' animations break.
			// Sheathing and re-equipping on cell attach seems to do the trick in most cases.
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive && !p->isPlayer1 && p->coopActor->currentProcess)
				{
					p->pam->ReadyWeapon(false);
					p->em->ReEquipHandForms();
				}
			}
		}

		return EventResult::kContinue;
	}

	CoopCombatEventHandler* CoopCombatEventHandler::GetSingleton()
	{
		static CoopCombatEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopCombatEventHandler::Register()
	{
		auto scriptEventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		if (scriptEventSourceHolder)
		{
			auto singleton = GetSingleton();
			scriptEventSourceHolder->AddEventSink(CoopCombatEventHandler::GetSingleton());
			auto source = scriptEventSourceHolder->GetEventSource<RE::TESCombatEvent>();
			if (!source)
			{
				return;
			}

			// Find our added sink.
			int32_t sinkIndex = -1;
			for (auto i = 0; i < source->sinks.size(); ++i)
			{
				if (source->sinks[i] == singleton)
				{
					sinkIndex = i;
				}
			}

			if (sinkIndex == -1)
			{
				SPDLOG_ERROR("[Events] ERR: Could not get registered combat event sink.");
			}
			else
			{
				SPDLOG_DEBUG("[Events] Combat event sink found at index {}.", sinkIndex);
				// Move our sink to the front so it processes events first.
				for (auto i = 0; i < sinkIndex; ++i)
				{
					source->sinks[i + 1] = source->sinks[i]; 
				}

				source->sinks[0] = singleton;
			}

			SPDLOG_INFO("[Events] Registered for combat events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for combat events.");
		}
	}

	EventResult CoopCombatEventHandler::ProcessEvent
	(
		const RE::TESCombatEvent* a_combatEvent, RE::BSTEventSource<RE::TESCombatEvent>*
	)
	{
		// NOTE: 
		// Purely for debugging purposes right now.
		if (!glob.globalDataInit || 
			!glob.coopSessionActive ||
			!a_combatEvent->actor || 
			!a_combatEvent->targetActor)
		{
			return EventResult::kContinue;
		}
		
		SPDLOG_DEBUG("[Events] Combat Event: {} -> {}: {}.",
			a_combatEvent->actor->GetName(),
			a_combatEvent->targetActor->GetName(),
			*a_combatEvent->newState);

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
			SPDLOG_INFO("[Events] Registered for container changed events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for container changed events.");
		}
	}

	EventResult CoopContainerChangedHandler::ProcessEvent
	(
		const RE::TESContainerChangedEvent* a_containerChangedEvent,
		RE::BSTEventSource<RE::TESContainerChangedEvent>*
	)
	{
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!glob.globalDataInit || 
			!glob.coopSessionActive ||
			!p1 ||
			!a_containerChangedEvent || 
			!a_containerChangedEvent->baseObj)
		{
			return EventResult::kContinue;
		}

		// IMPORTANT NOTE:
		// Adding the object to a companion player after removing it from P1, 
		// instead of removing the item from P1 directly to the companion player
		// does not flag the companion player's inventory as changed 
		// and does not trigger the game's equip calculations, 
		// which normally clear out the player's currently equipped gear
		// and force the equip manager to re-equip the companion player's hand forms.
		// Always remove then add any transferred items.

		bool fromP1 = a_containerChangedEvent->oldContainer == p1->formID;
		bool toP1 = a_containerChangedEvent->newContainer == p1->formID;
		// From a player or a co-op chest.
		bool fromCoopEntity = 
		(
			glob.coopEntityBlacklistFIDSet.contains(a_containerChangedEvent->oldContainer) || 
			std::any_of
			(
				glob.coopInventoryChests.begin(), glob.coopInventoryChests.end(),
				[a_containerChangedEvent](const auto& a_chestRefrPtr)
				{
					return 
					(
						a_chestRefrPtr &&
						a_chestRefrPtr->formID == a_containerChangedEvent->oldContainer
					);
				}
			)
		);
		bool toCoopPlayer = GlobalCoopData::IsCoopPlayer(a_containerChangedEvent->newContainer);
		int32_t fromCoopPlayerIndex = 
		(
			GlobalCoopData::GetCoopPlayerIndex(a_containerChangedEvent->oldContainer)
		);
		int32_t toCoopPlayerIndex = 
		(
			GlobalCoopData::GetCoopPlayerIndex(a_containerChangedEvent->newContainer)
		);

		// Update player's encumbrance value when an item is moved to/from their inventory.
		if ((fromCoopPlayerIndex != toCoopPlayerIndex) && 
			(fromCoopPlayerIndex != -1 || toCoopPlayerIndex != -1))
		{
			int32_t indexToUse = 
			(
				fromCoopPlayerIndex != -1 ?
				fromCoopPlayerIndex :
				toCoopPlayerIndex
			);
			const auto& p = glob.coopPlayers[indexToUse];
			p->mm->UpdateEncumbranceFactor();
		}

		// Update P1's has-paraglider state before doing anything else.
		if ((ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled) && (toP1 || fromP1))
		{
			if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
			{
				auto paraglider = 
				(
					dataHandler->LookupForm<RE::TESObjectMISC>(0x802, "Paragliding.esp")
				); 
				if (paraglider)
				{
					if (a_containerChangedEvent->baseObj == paraglider->formID)
					{
						auto invCounts = p1->GetInventoryCounts();
						const auto iter = invCounts.find(paraglider);
						ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider = 
						(
							iter != invCounts.end() && iter->second > 0
						);

						// Add gale spell if not known already.
						// Enderal only, since the quest to obtain the paraglider
						// and learn Tarhiel's Gale is not present in Enderal.
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

		// Added to P1 or added to co-op player.
		// Prioritize companion player loot through menus 
		// before performing Enderal-specific item transfers.
		const auto ui = RE::UI::GetSingleton(); 
		// A companion player controlling menus
		// and an item was transferred to a player from a non-coop entity.
		if (ui && glob.mim->managerMenuCID != -1 && !fromCoopEntity && toCoopPlayer)
		{
			bool fromCraftingMenu = ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME);
			bool fromContainerMenu = ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME);
			bool fromLootMenu = ui->IsMenuOpen(GlobalCoopData::LOOT_MENU);
			// Should move the item from the container to the player.
			bool transferFromContainer = false;
			if (fromCraftingMenu || fromContainerMenu || fromLootMenu)
			{
				if (fromContainerMenu)
				{
					auto mode = ui->GetMenu<RE::ContainerMenu>()->GetContainerMode();
					// Must be looting, pickpocketing, or stealing.
					transferFromContainer = 
					{ 
						mode == RE::ContainerMenu::ContainerMode::kLoot || 
						mode == RE::ContainerMenu::ContainerMode::kPickpocket ||
						mode == RE::ContainerMenu::ContainerMode::kSteal 
					};
				}
				else
				{
					// Always transfer from Crafting or Loot menus.
					transferFromContainer = true;
				}
			}

			// Move from P1 to the companion player 
			// if crafting, looting, stealing, or pickpocketing,
			// or move party-wide items to P1 if a companion player received the item.
			if (transferFromContainer)
			{
				auto baseObj = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj);
				auto refr = Util::GetRefrPtrFromHandle(a_containerChangedEvent->reference);
				if (toP1)
				{
					const auto& p = glob.coopPlayers[glob.mim->managerMenuCID];
					// Add to co-op player controlling menus if not a shared item.
					if (baseObj && !Util::IsPartyWideItem(baseObj))
					{
						if (auto boundObj = baseObj->As<RE::TESBoundObject>(); boundObj)
						{
							const int32_t count = a_containerChangedEvent->itemCount;
							// Do not transfer quest objects.
							auto inventory = p1->GetInventory();
							const auto iter = inventory.find(boundObj); 
							if (iter != inventory.end())
							{
								const auto& invEntryData = iter->second.second;
								if (invEntryData && invEntryData->IsQuestObject())
								{
									SPDLOG_DEBUG
									(
										"[Events] Container Changed Event: "
										"Not transfering quest item object {} (x{}) to {}.",
										baseObj->GetName(),
										count,
										p->coopActor->GetName()
									);
									return EventResult::kContinue;
								}
							}

							// Run on the main thread by running the removal/addition code
							// through a synced task with the player's task runner thread.
							// Could prevent threading issues from crashing the game here,
							// since this event handler is not run by the main thread.
							// Ugly, but fixes a RaceMenu crash 
							// when transferring over a single torch. Needs more testing.
							p->taskRunner->AddTask
							(
								[p, p1, boundObj, count]()
								{
									Util::AddSyncedTask
									(
										[p, p1, boundObj, count]()
										{
											SPDLOG_DEBUG
											(
												"[Events] Container Changed Event: "
												"Removing base item {} (x{}) and giving to {}.", 
												boundObj->GetName(),
												count, 
												p->coopActor->GetName()
											);
											p1->RemoveItem
											(
												boundObj, 
												count,
												RE::ITEM_REMOVE_REASON::kRemove, 
												nullptr, 
												nullptr
											);
											p->coopActor->AddObjectToContainer
											(
												boundObj, 
												nullptr, 
												count, 
												p->coopActor.get()
											);
										}
									);
								}
							);
						}

						return EventResult::kContinue;
					}
					else if (refr && !Util::IsPartyWideItem(refr.get()))
					{
						const int32_t count = a_containerChangedEvent->itemCount;
						// Do not transfer quest objects.
						bool isQuestItem = 
						{
							refr->extraList.HasType(RE::ExtraDataType::kAliasInstanceArray) ||
							refr->extraList.HasType(RE::ExtraDataType::kFromAlias)
						};
						if (isQuestItem)
						{
							SPDLOG_DEBUG
							(
								"[Events] Container Changed Event: "
								"Not moving quest item refr {} (x{}) to {}.",
								refr->GetName(),
								count,
								p->coopActor->GetName()
							);
							return EventResult::kContinue;
						}

						if (auto boundObj = refr->GetBaseObject(); boundObj)
						{
							p->taskRunner->AddTask
							(
								[p, p1, boundObj, count]()
								{
									Util::AddSyncedTask
									(
										[p, p1, boundObj, count]()
										{
											SPDLOG_DEBUG
											(
												"[Events] Container Changed Event: "
												"Removing reference item {} (x{}) "
												"and giving to {}.", 
												boundObj->GetName(), 
												count,
												p->coopActor->GetName()
											);
											p1->RemoveItem
											(
												boundObj, 
												count,
												RE::ITEM_REMOVE_REASON::kRemove, 
												nullptr, 
												nullptr
											);
											p->coopActor->AddObjectToContainer
											(
												boundObj, 
												nullptr, 
												count, 
												p->coopActor.get()
											);
										}
									);
								}
							);
						}

						return EventResult::kContinue;
					}
				}
				else if (toCoopPlayer)
				{							
					// Give any looted keys/regular books/notes to P1, 
					// since these items can be tough to find 
					// after being (un)intentionally looted by co-op companions.
					if (baseObj && Util::IsPartyWideItem(baseObj))
					{
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
								const int32_t count = a_containerChangedEvent->itemCount;
								// Do not transfer quest objects.
								auto inventory = p->coopActor->GetInventory();
								const auto iter = inventory.find(boundObj); 
								if (iter != inventory.end())
								{
									SPDLOG_DEBUG
									(
										"[Events] Container Changed Event: "
										"Moving quest item {} (x{}) from {} to P1.",
										baseObj->GetName(),
										count,
										p->coopActor->GetName()
									);
									const auto& invEntryData = iter->second.second;
									if (invEntryData && invEntryData->IsQuestObject())
									{
										return EventResult::kContinue;
									}
								}

								p->taskRunner->AddTask
								(
									[p, p1, boundObj, count]()
									{
										Util::AddSyncedTask
										(
											[p, p1, boundObj, count]()
											{
												SPDLOG_DEBUG
												(
													"[Events] Container Changed Event: "
													"Moving party-wide item from {} (x{}) to P1.",
													p->coopActor->GetName(), 
													count
												);
												p->coopActor->RemoveItem
												(
													boundObj, 
													count, 
													RE::ITEM_REMOVE_REASON::kStoreInTeammate, 
													nullptr, 
													p1
												);
											}
										);
									}
								);
							}

							return EventResult::kContinue;
						}
					}
				}
			}
		}

		bool fromCoopCompanionPlayer = fromCoopPlayerIndex != -1 && !fromP1;
		// A co-op companion player is attempting to gift items 
		// to another co-op companion player by way of the GiftMenu through P1. 
		// Transfer any items added to P1 to the giftee companion player instead.
		bool giftMenuOpen = ui && ui->IsMenuOpen(RE::GiftMenu::MENU_NAME);
		if (giftMenuOpen && 
			fromCoopCompanionPlayer && 
			toP1 && 
			glob.mim->IsRunning() && 
			glob.mim->managerMenuCID != -1 && 
			Util::HandleIsValid(glob.mim->gifteePlayerHandle)) 
		{
			const auto& giftingP = glob.coopPlayers[fromCoopPlayerIndex];
			auto gifteePtr = Util::GetActorPtrFromHandle(glob.mim->gifteePlayerHandle);
			if (!gifteePtr) 
			{
				return EventResult::kContinue;
			}

			auto baseObj = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj);
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
				const int32_t count = a_containerChangedEvent->itemCount;
				giftingP->taskRunner->AddTask
				(
					[giftingP, gifteePtr, p1, boundObj, count]()
					{
						Util::AddSyncedTask
						(
							[giftingP, gifteePtr, p1, boundObj, count]()
							{
								SPDLOG_DEBUG
								(
									"[Events] Container Changed Event: "
									"Removing {} (x{}) from P1 to {} (from gifting player {}).",
									boundObj->GetName(), 
									count,
									gifteePtr->GetName(),
									giftingP->coopActor->GetName()
								);
								p1->RemoveItem
								(
									boundObj, 
									count,
									RE::ITEM_REMOVE_REASON::kRemove, 
									nullptr, 
									nullptr
								);
								gifteePtr->AddObjectToContainer
								(
									boundObj, 
									nullptr, 
									count, 
									gifteePtr.get()
								);
							}
						);
					}
				);
			}

			return EventResult::kContinue;
		}

		bool barterMenuOpen = ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME);
		// Enderal-specific gold scaling and skillbook loot.
		// To a player but not from another player, and not from a transaction (Barter Menu open).
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && 
			!fromCoopEntity && 
			toCoopPlayer && 
			!barterMenuOpen)
		{
			auto form = RE::TESForm::LookupByID(a_containerChangedEvent->baseObj);
			if (form && form->IsGold())
			{
				// Scale added gold with party size.
				// NOTE: 
				// Gold always goes to P1, 
				// as P1's gold acts as a shared pool for all players.
				if (Settings::fAdditionalGoldPerPlayerMult > 0.0f)
				{
					int32_t additionalGold = 
					(
						a_containerChangedEvent->itemCount * 
						(glob.activePlayers - 1) * 
						Settings::fAdditionalGoldPerPlayerMult
					);
					const auto& toP = glob.coopPlayers[toCoopPlayerIndex];
					const auto gold = form->As<RE::TESObjectMISC>();
					toP->taskRunner->AddTask
					(
						[toP, p1, gold, additionalGold]()
						{
							Util::AddSyncedTask
							(
								[toP, p1, gold, additionalGold]()
								{
									p1->AddObjectToContainer
									(
										gold, 
										nullptr, 
										additionalGold, 
										p1
									);
								}
							);
						}
					);

					bool inMenu = !Util::MenusOnlyAlwaysOpen();
					// If not in a menu and activating all gold in activation range, 
					// each individual gold piece added triggers a container changed event, 
					// so the total amount looted is unknown until all events fire
					// and we cannot print a single notification with that total here.
					if (inMenu) 
					{
						RE::DebugNotification
						(
							fmt::format
							(
								"Received an additional {} gold from party size scaling.", 
								additionalGold
							).c_str()
						);
					}
					else
					{
						RE::DebugNotification
						(
							fmt::format
							(
								"Received additional gold from party size scaling (x{}).", 
								glob.activePlayers * Settings::fAdditionalGoldPerPlayerMult
							).c_str()
						);
					}

					return EventResult::kContinue;
				}
			}
			else 
			{
				if (!Settings::bEveryoneGetsALootedEnderalSkillbook)
				{
					return EventResult::kContinue;
				}

				bool isEnderalSkillbook = 
				(
					GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.contains
					(
						a_containerChangedEvent->baseObj
					)
				);
				if (!isEnderalSkillbook)
				{
					return EventResult::kContinue;
				}

				// Give each active player, 
				// aside from the player receiving the current skillbook, 
				// a random skillbook of the same tier.
				const auto& tierAndSkill = 
				(
					GlobalCoopData::ENDERAL_SKILLBOOK_FIDS_TO_TIER_SKILL_MAP.at
					(
						a_containerChangedEvent->baseObj
					)
				);
				const auto& tier = tierAndSkill.first;
				const auto& skill = tierAndSkill.second;
				std::mt19937 generator{ };
				generator.seed(SteadyClock::now().time_since_epoch().count());

				const auto& toP = 
				(
					glob.coopPlayers
					[
						GlobalCoopData::GetCoopPlayerIndex
						(
							a_containerChangedEvent->newContainer
						)
					]
				);
				for (const auto& p : glob.coopPlayers)
				{
					// Not the looting player.
					if (p->isActive && 
						p->coopActor->formID != a_containerChangedEvent->newContainer)
					{
						// To each player, add the same number as the number looted.
						uint32_t numAdded = 0;
						while (numAdded < a_containerChangedEvent->itemCount)
						{
							// Random skillbook index.
							const auto totalSkillBooksCount = 
							(
								GlobalCoopData::ENDERAL_SKILL_TO_SKILLBOOK_INDEX_MAP.size()
							);
							float rand = 
							(
								static_cast<uint8_t>
								(
									totalSkillBooksCount * 
									(generator() / (float)((std::mt19937::max)()))
								)
							);
							const auto newSkillbookFID = 
							(
								GlobalCoopData::ENDERAL_TIERED_SKILLBOOKS_MAP.at(tier)[rand]
							);
							auto newSkillbook = RE::TESForm::LookupByID<RE::AlchemyItem>
							(
								newSkillbookFID
							);
							if (newSkillbook)
							{
								p->taskRunner->AddTask
								(
									[p, p1, newSkillbook]()
									{
										Util::AddSyncedTask
										(
											[p, p1, newSkillbook]()
											{

												p->coopActor->AddObjectToContainer
												(
													newSkillbook,
													nullptr, 
													1, 
													p->coopActor.get()
												);
											}
										);
									}
								);
								// Show in TrueHUD recent loot widget 
								// by adding and removing the skillbook from P1.
								if (ALYSLC::TrueHUDCompat::g_trueHUDInstalled && 
									toP->coopActor.get() == p1)
								{
									toP->taskRunner->AddTask
									(
										[p1, newSkillbook]()
										{
											Util::AddSyncedTask
											(
												[p1, newSkillbook]()
												{
													p1->AddObjectToContainer
													(
														newSkillbook->As<RE::AlchemyItem>(),
														nullptr, 
														1, 
														p1
													);
													p1->RemoveItem
													(
														newSkillbook->As<RE::AlchemyItem>(),
														1, 
														RE::ITEM_REMOVE_REASON::kRemove, 
														nullptr, 
														nullptr
													);
												}
											);
										}
									);
								}

								RE::DebugNotification
								(
									fmt::format
									(
										"{} received 1 {}.", 
										p->coopActor->GetName(), 
										newSkillbook->GetName()
									).c_str()
								);
							}

							++numAdded;
						}
					}
				}
			}
		}

		return EventResult::kContinue;
	}

	CoopCrosshairEventHandler* CoopCrosshairEventHandler::GetSingleton()
	{
		static CoopCrosshairEventHandler singleton;
		return std::addressof(singleton);
	}

	void CoopCrosshairEventHandler::Register()
	{
		auto source = SKSE::GetCrosshairRefEventSource();
		if (source)
		{
			auto singleton = GetSingleton();
			source->AddEventSink(singleton);
			SPDLOG_INFO("[Events] Registered for crosshair events.");

			// Find our added sink.
			int32_t sinkIndex = -1;
			for (auto i = 0; i < source->sinks.size(); ++i)
			{
				if (source->sinks[i] == singleton)
				{
					sinkIndex = i;
				}
			}

			if (sinkIndex == -1)
			{
				SPDLOG_ERROR("[Events] ERR: Could not get registered crosshair event sink.");
			}
			else
			{
				SPDLOG_DEBUG("[Events] Crosshair event sink found at index {}.", sinkIndex);
				// Move our sink to the front so it processes events first.
				for (auto i = 0; i < sinkIndex; ++i)
				{
					source->sinks[i + 1] = source->sinks[i]; 
				}

				source->sinks[0] = singleton;
			}
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for crosshair events."); 
		}
	}

	EventResult CoopCrosshairEventHandler::ProcessEvent
	(
		const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*
	)
	{
		// While co-op is active, discard requests to set the crosshair refr
		// to any refr other than the one requested by the currently active players.
		// Will prevent the LootMenu from opening or closing
		// when the game crosshair's pick refr changes.

		if (glob.coopSessionActive)
		{
			bool matchesRequestedRefr = 
			(
				(
					a_event->crosshairRef &&
					a_event->crosshairRef->GetHandle() == glob.reqQuickLootContainerHandle
				) ||
				(
					!a_event->crosshairRef &&
					glob.reqQuickLootContainerHandle == RE::ObjectRefHandle()
				)
			);
			if (matchesRequestedRefr)
			{
				return EventResult::kContinue;
			}
			else
			{
				// Stop propagation to prevent QuickLoot's event handler 
				// from processing this request and opening/closing the LootMenu.
				return EventResult::kStop;
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
			auto singleton = GetSingleton();
			scriptEventSourceHolder->AddEventSink(CoopDeathEventHandler::GetSingleton());
			SPDLOG_INFO("[Events] Registered for death events.");

			auto source = scriptEventSourceHolder->GetEventSource<RE::TESDeathEvent>();
			if (!source)
			{
				return;
			}

			// Find our added sink.
			int32_t sinkIndex = -1;
			for (auto i = 0; i < source->sinks.size(); ++i)
			{
				if (source->sinks[i] == singleton)
				{
					sinkIndex = i;
				}
			}

			if (sinkIndex == -1)
			{
				SPDLOG_ERROR("[Events] ERR: Could not get registered death event sink.");
			}
			else
			{
				SPDLOG_DEBUG("[Events] Death event sink found at index {}.", sinkIndex);
				// Move our sink to the front so it processes events first.
				for (auto i = 0; i < sinkIndex; ++i)
				{
					source->sinks[i + 1] = source->sinks[i]; 
				}

				source->sinks[0] = singleton;
			}
		}
		else 
		{ 
			SPDLOG_ERROR("[Events] ERR: Could not register for death events."); 
		}
	}

	EventResult CoopDeathEventHandler::ProcessEvent
	(
		const RE::TESDeathEvent* a_deathEvent,
		RE::BSTEventSource<RE::TESDeathEvent>* a_source
	)
	{
		// Purely for debugging purposes right now.
		SPDLOG_DEBUG
		(
			"[Events] Death Event: {}, "
			"killed by death, I mean, erm... by {}. "
			"Is dead: {}, essential flag set: {}, {}",
			a_deathEvent->actorDying ? a_deathEvent->actorDying->GetName() : "N/A",
			a_deathEvent->actorKiller ? a_deathEvent->actorKiller->GetName() : "N/A",
			a_deathEvent->actorDying->IsDead(),
			a_deathEvent->actorDying->As<RE::Actor>()->GetActorBase() ?
			a_deathEvent->actorDying->As<RE::Actor>()->GetActorBase()->actorData.actorBaseFlags.all
			(
				RE::ACTOR_BASE_DATA::Flag::kEssential
			) :
			false,
			a_deathEvent->actorDying->As<RE::Actor>()->boolFlags.all
			(
				RE::Actor::BOOL_FLAGS::kEssential
			)
		);

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
			SPDLOG_INFO("[Events] Registered for equip events.");
		}
		else 
		{ 
			SPDLOG_ERROR("[Events] ERR: Could not register for equip events."); 
		}
	}

	EventResult CoopEquipEventHandler::ProcessEvent
	(
		const RE::TESEquipEvent* a_equipEvent, RE::BSTEventSource<RE::TESEquipEvent>*
	)
	{
		if (!glob.coopSessionActive || !a_equipEvent || !a_equipEvent->baseObject)
		{
			return EventResult::kContinue;
		}

		auto foundIndex = GlobalCoopData::GetCoopPlayerIndex(a_equipEvent->actor); 
		if (foundIndex == -1) 
		{	
			return EventResult::kContinue;
		}

		SPDLOG_DEBUG
		(
			"[Events] Equip Event: {} -> {} (0x{:X}): "
			"equipped: {} (type: 0x{:X}), original refr: 0x{:X}, unique id: 0x{:X}",
			a_equipEvent ? (a_equipEvent->actor->GetName()) : "N/A",
			a_equipEvent && 
			a_equipEvent->baseObject && 
			RE::TESForm::LookupByID(a_equipEvent->baseObject) ? 
			RE::TESForm::LookupByID(a_equipEvent->baseObject)->GetName() : 
			"N/A",
			a_equipEvent && 
			a_equipEvent->baseObject && 
			RE::TESForm::LookupByID(a_equipEvent->baseObject) ? 
			RE::TESForm::LookupByID(a_equipEvent->baseObject)->formID :
			0xDEAD,
			a_equipEvent->equipped,
			a_equipEvent && 
			a_equipEvent->baseObject && 
			RE::TESForm::LookupByID(a_equipEvent->baseObject) ? 
			*RE::TESForm::LookupByID(a_equipEvent->baseObject)->formType :
			RE::FormType::None,
			a_equipEvent->originalRefr,
			a_equipEvent->uniqueID
		);

		const auto& p = glob.coopPlayers[foundIndex];
		// Don't handle equip event if the player is not loaded, downed, or dead.
		if (!p->coopActor->Is3DLoaded() || p->isDowned || p->coopActor->IsDead())
		{
			return EventResult::kContinue;
		}

		// Check if equipped while bartering 
		// (with a co-op companion player's inventory copied over to P1).
		// Ignore these equip events and do not refresh equip state.
		auto ui = RE::UI::GetSingleton();
		bool affectedByInventoryTransfer = 
		{
			(ui && ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) &&
			(
				(glob.mim->managerMenuCID != -1 && p->coopActor == glob.player1Actor) ||
				(glob.mim->managerMenuCID == p->controllerID)
			)
		};
		// Game will sometimes unequip P1's weapons/magic during killmoves. 
		// Don't refresh equip state in that case either.
		auto equipForm = RE::TESForm::LookupByID(a_equipEvent->baseObject);
		bool shouldRefreshEquipState = 
		(
			(equipForm && !affectedByInventoryTransfer) && 
			(
				(!p->isPlayer1) || 
				(
					!p->coopActor->IsInKillMove() && 
					!p->pam->isBeingKillmovedByAnotherPlayer
				)
			)
		);
		if (shouldRefreshEquipState)
		{
			p->em->RefreshEquipState(RefreshSlots::kAll, equipForm, a_equipEvent->equipped);
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
			auto singleton = GetSingleton();
			scriptEventSourceHolder->AddEventSink(singleton);
			SPDLOG_INFO("[Events] Registered for hit events.");
			
			auto source = scriptEventSourceHolder->GetEventSource<RE::TESHitEvent>();
			if (!source)
			{
				return;
			}

			// Find our added sink.
			int32_t sinkIndex = -1;
			for (auto i = 0; i < source->sinks.size(); ++i)
			{
				if (source->sinks[i] == singleton)
				{
					sinkIndex = i;
				}
			}

			if (sinkIndex == -1)
			{
				SPDLOG_ERROR("[Events] ERR: Could not get registered hit event sink.");
			}
			else
			{
				SPDLOG_DEBUG("[Events] Hit event sink found at index {}.", sinkIndex);
				// Move our sink to the front so it processes events first.
				for (auto i = 0; i < sinkIndex; ++i)
				{
					source->sinks[i + 1] = source->sinks[i]; 
				}

				source->sinks[0] = singleton;
			}
		}
		else 
		{ 
			SPDLOG_ERROR("[Events] ERR: Could not register for onhit events.");
		}
	}

	EventResult CoopHitEventHandler::ProcessEvent
	(
		const RE::TESHitEvent* a_hitEvent, RE::BSTEventSource<RE::TESHitEvent>*
	) 
	{
		if (!glob.coopSessionActive || !a_hitEvent)
		{
			return EventResult::kContinue;
		}

		auto aggressorRefr = a_hitEvent->cause;
		auto hitRefr = a_hitEvent->target;
		if (!aggressorRefr || !hitRefr || hitRefr->IsDead())
		{
			return EventResult::kContinue;
		}

		// Essential flag sometimes gets toggled off 
		// when the game is about to killmove the hit actor.
		// Have to set the flag again to prevent P1 
		// and other players from being killed 
		// during a killmove animation while using the co-op revive system.
		if (Settings::bUseReviveSystem) 
		{
			auto foundVictimIndex = GlobalCoopData::GetCoopPlayerIndex(hitRefr.get());
			if (foundVictimIndex != -1)
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
		auto foundTargetIndex = GlobalCoopData::GetCoopPlayerIndex(hitRefr); 
		if (foundTargetIndex != -1)
		{
			const auto& p = glob.coopPlayers[foundTargetIndex];
			auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source);
			auto attackingProj = RE::TESForm::LookupByID(a_hitEvent->projectile);
			float damage = 0.0f;
			if (attackingObj)
			{
				// No spell attack damage event hook.
				// Approximation: use magnitude of spell.
				if (attackingObj->IsWeapon()) 
				{
					damage = (attackingObj->As<RE::TESObjectWEAP>()->GetAttackDamage();
				}
				else if (attackingObj->IsMagicItem() && attackingObj->As<RE::SpellItem>())
				{
					damage = 
					(
						attackingObj->As<RE::SpellItem>()->GetCostliestEffectItem()->GetMagnitude()
					);
				}

				if (attackingObj->IsMagicItem() && 
					*a_hitEvent->flags != RE::TESHitEvent::Flag::kNone) 
				{
					const auto& armorRatings = p->em->armorRatings;
					float lightArmorBaseXP = 
					(
						damage * armorRatings.first / 
						(
							armorRatings.first + armorRatings.second == 0.0f ? 
							1.0f :
							armorRatings.first + armorRatings.second
						)
					);
					float heavyArmorBaseXP = 
					(
						damage * armorRatings.second / 
						(
							armorRatings.first + armorRatings.second == 0.0f ? 
							1.0f : 
							armorRatings.first + armorRatings.second
						)
					);

					if (lightArmorBaseXP > 0.0f)
					{
						SPDLOG_DEBUG
						(
							"[Events] Hit Event: {} was hit by {}'s {}, "
							"adding Light Armor Skill XP, base XP: {}.",
							p->coopActor->GetName(),
							aggressorRefr ? aggressorRefr->GetName() : "NONE",
							attackingObj->GetName(),
							lightArmorBaseXP
						);
						GlobalCoopData::AddSkillXP
						(
							p->controllerID, 
							RE::ActorValue::kLightArmor, 
							lightArmorBaseXP
						);
					}

					if (heavyArmorBaseXP > 0.0f)
					{
						SPDLOG_DEBUG
						(
							"[Events] Hit Event: {} was hit by {}'s {}, "
							"adding Heavy Armor Skill XP, base XP: {}.",
							p->coopActor->GetName(),
							aggressorRefr ? aggressorRefr->GetName() : "NONE",
							attackingObj->GetName(),
							heavyArmorBaseXP
						);
						GlobalCoopData::AddSkillXP
						(
							p->controllerID,
							RE::ActorValue::kHeavyArmor, 
							heavyArmorBaseXP
						);
					}
				}
			}
		}
		*/

		auto foundAggressorIndex = GlobalCoopData::GetCoopPlayerIndex(aggressorRefr); 
		// Ignore hits that do not originate from a player.
		if (foundAggressorIndex == -1)
		{
			return EventResult::kContinue;
		}

		// Hit by a player.
		const auto& p = glob.coopPlayers[foundAggressorIndex];
		auto hitActor = hitRefr->As<RE::Actor>();
		// Is the hit actor hostile to a player?
		bool isHostileToAPlayer = false;
		// Was the refr hit by a flop or thrown object?
		bool isBonkOrSplatHitEvent = false;
		// How can she slap?
		bool isSlapEvent = false;
		// Handle hit event for an actor hit by a player.
		if (hitActor) 
		{
			auto projectileForm = RE::TESForm::LookupByID(a_hitEvent->projectile);
			// Caused by player + projectile is the hit actor (splat) 
			// or hit event projectile's form type is not projectile (bonk).
			// Also flagged as a power attack.
			isBonkOrSplatHitEvent = 
			(
				(a_hitEvent->cause == p->coopActor) && 
				(
					(a_hitEvent->projectile == hitActor->formID) || 
					(projectileForm && !projectileForm->As<RE::BGSProjectile>())
				) &&
				(
					((!(*a_hitEvent->flags) & (!AdditionalHitEventFlags::kBonk)) != 0) || 
					((!(*a_hitEvent->flags) & (!AdditionalHitEventFlags::kSplat)) != 0)
				)
			);

			// Caused by the player, projectile is the player,
			// and flagged as a normal attack.
			isSlapEvent = 
			(
				(a_hitEvent->cause == p->coopActor) && 
				(a_hitEvent->projectile == p->coopActor->formID) &&
				((!(*a_hitEvent->flags) & (!AdditionalHitEventFlags::kSlap)) != 0)
			);

			// Check if an actor that is not a player teammate is hostile to any player.
			if (!hitActor->IsPlayerTeammate() && !hitActor->IsPlayerRef())
			{
				isHostileToAPlayer = std::find_if
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(),
					[&hitActor](const auto& p) 
					{
						if (!p->isActive)
						{
							return false;
						}

						return hitActor->IsHostileToActor(p->coopActor.get());
					}
				) != glob.coopPlayers.end();
			}

			// XP and damage modifications.
			if (p->IsRunning())
			{
				auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source); 
				const float damageMult = p->pam->reqDamageMult;

				// Do not give the attacking player XP 
				// if hitting another player while they are in god mode.
				const auto playerVictimIndex = GlobalCoopData::GetCoopPlayerIndex(hitActor);
				bool victimPlayerInGodMode = 
				(
					playerVictimIndex != -1 && 
					glob.coopPlayers[playerVictimIndex]->isInGodMode
				);
				// Also, do not give XP if friendly fire is disabled 
				// and hitting a friendly actor.
				bool canGrantXP = 
				(
					(!victimPlayerInGodMode) && 
					(
						Settings::vbFriendlyFire[p->controllerID] || 
						!Util::IsPartyFriendlyActor(hitActor)
					)
				);
				// XP formulas pulled from UESP:
				// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
				if (!p->isPlayer1 && canGrantXP)
				{
					auto weap = attackingObj ? attackingObj->As<RE::TESObjectWEAP>() : nullptr;
					// Add XP to the attacking player.
					if (weap)
					{
						if (weap->IsRanged() && !weap->IsStaff())
						{
							SPDLOG_DEBUG
							(
								"[Events] Hit Event: Adding {} XP to {}, "
								"base XP: {}, weapon: {}",
								RE::ActorValue::kArchery, 
								p->coopActor->GetName(), 
								weap->GetAttackDamage(),
								weap->GetName()
							);
							GlobalCoopData::AddSkillXP
							(
								p->controllerID, 
								RE::ActorValue::kArchery, 
								weap->GetAttackDamage()
							);
						}
						else if (weap->IsOneHandedAxe() || weap->IsOneHandedDagger() || 
								 weap->IsOneHandedMace() || weap->IsOneHandedSword())
						{
							SPDLOG_DEBUG
							(
								"[Events] Hit Event: Adding {} XP to {}, "
								"base XP: {}, weapon: {}",
								RE::ActorValue::kOneHanded,
								p->coopActor->GetName(),
								weap->GetAttackDamage(),
								weap->GetName()
							);
							GlobalCoopData::AddSkillXP
							(
								p->controllerID,
								RE::ActorValue::kOneHanded,
								weap->GetAttackDamage()
							);
						}
						else if (weap->IsTwoHandedAxe() || weap->IsTwoHandedSword())
						{
							SPDLOG_DEBUG
							(
								"[Events] Hit Event: Adding {} XP to {}, "
								"base XP: {}, weapon: {}",
								RE::ActorValue::kTwoHanded, 
								p->coopActor->GetName(),
								weap->GetAttackDamage(), 
								weap->GetName()
							);
							GlobalCoopData::AddSkillXP
							(
								p->controllerID,
								RE::ActorValue::kTwoHanded, 
								weap->GetAttackDamage()
							);
						}
					}

					// Block XP.
					if (p->pam->isBashing && p->em->HasShieldEquipped())
					{
						SPDLOG_DEBUG
						(
							"[Events] Hit Event: Adding 5 XP to {}'s Block Skill "
							"for a successful shield bash",
							p->coopActor->GetName()
						);
						GlobalCoopData::AddSkillXP(p->controllerID, RE::ActorValue::kBlock, 5.0f);
					}

					// Display sneak attack message for companion player if needed.
					auto magicItem = attackingObj ? attackingObj->As<RE::MagicItem>() : nullptr;
					// Is a sneak attack if the player's damage mult was previously set
					// to a value above 1.0.
					// Must also have their weapons out and be sneaking when the attack hits.
					bool isSneakAttack = 
					(
						(damageMult > 1.0f) && 
						(
							(weap || magicItem) && 
							p->coopActor->IsWeaponDrawn() &&
							p->coopActor->IsSneaking()
						)
					);
					// Sneak message and XP.
					if (isSneakAttack)
					{
						const auto sneakMsg = fmt::format
						(
							"{} performed a sneak attack for {:.1f}x damage!",
							p->coopActor->GetName(), 
							damageMult
						);
						RE::DebugNotification(sneakMsg.data(), "UISneakAttack", true);

						// Ranged and melee attack sources give different XP.
						if ((weap && weap->IsRanged()) || magicItem)
						{
							SPDLOG_DEBUG
							(
								"[Events] Hit Event: Adding 2.5 XP to {}'s Sneak Skill "
								"for a successful ranged sneak attack with {} (0x{:X}, {})",
								p->coopActor->GetName(),
								weap ? weap->GetName() : magicItem ? magicItem->GetName() : "NONE",
								weap ? weap->formID : magicItem ? magicItem->formID : 0xDEAD,
								weap ? "weapon" : magicItem ? "magic item" : ""
							);
							GlobalCoopData::AddSkillXP
							(
								p->controllerID, 
								RE::ActorValue::kSneak, 
								2.5f
							);
						}
						else
						{
							SPDLOG_DEBUG
							(
								"[Events] Hit Event: Adding 30 XP to {}'s Sneak Skill "
								"for a successful melee sneak attack with {} (0x{:X}, {})",
								p->coopActor->GetName(),
								weap ? weap->GetName() : "fists",
								weap ? weap->formID : 0xDEAD,
								weap ?
								RE::FormType::Weapon : 
								attackingObj ?
								*attackingObj->formType : 
								RE::FormType::None
							);
							GlobalCoopData::AddSkillXP
							(
								p->controllerID, 
								RE::ActorValue::kSneak, 
								30.0f
							);
						}

						// Send the sneak attack event.
						Util::SendCriticalHitEvent(p->coopActor.get(), weap, true);
					}
				}

				// Thrown object sneak attacks (for all players).
				// Same XP awarded as for a ranged weapon sneak attack.
				// 2.0 damage multiplier.
				if (canGrantXP && a_hitEvent->flags.any(RE::TESHitEvent::Flag::kSneakAttack) && 
					projectileForm && !projectileForm->As<RE::BGSProjectile>())
				{
					SPDLOG_DEBUG
					(
						"[Events] Hit Event: Adding 2.5 XP to {}'s Sneak Skill "
						"for a successful thrown object sneak attack with {} (0x{:X}, {})",
						p->coopActor->GetName(), 
						projectileForm->GetName(), 
						projectileForm->formID,
						*projectileForm->formType
					);
					GlobalCoopData::AddSkillXP
					(
						p->controllerID, 
						RE::ActorValue::kSneak, 
						2.5f
					);
					const auto sneakMsg = fmt::format
					(
						"{} performed a sneak attack for 2.0x damage!", 
						p->coopActor->GetName()
					);

					RE::DebugNotification(sneakMsg.data(), "UISneakAttack", true);
					Util::SendCriticalHitEvent(p->coopActor.get(), nullptr, true);
				}
			}
		}
		
		// Nothing further to do if we hit another player.
		bool hitPlayer = GlobalCoopData::IsCoopPlayer(hitActor);
		if (hitPlayer)
		{
			return EventResult::kContinue;
		}
		
		// WARNING. INSANE JANK AHEAD. TURN BACK NOW.
		// P1 only receives 1000 bounty for murder if they deal the killing blow,
		// so we have to send additional hit events to pin blame on P1 
		// if another player is assaulting an actor.
		// Will also allow companion players to 'hit' targets through P1.

		auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
		// Hit the dialogue target with a bonk, slap, or splat.
		bool hitDialogueTargetWithSpecialAttack = 
		(
			(isBonkOrSplatHitEvent || isSlapEvent) &&
			(menuTopicManager && menuTopicManager->speaker) &&
			(
				hitRefr == Util::GetRefrPtrFromHandle(menuTopicManager->speaker) ||
				hitRefr == Util::GetRefrPtrFromHandle(menuTopicManager->lastSpeaker)
			)
		);
		// Want to ignore hitting the dialogue target with anything
		// that might ragdoll or aggro them, since this locks up the Dialogue Menu
		// or can prevent the dialogue target from triggering their dialogue again
		// after getting up. 
		// Would require console commands and a few prayers to fix without restarting the game.
		if (hitDialogueTargetWithSpecialAttack || isSlapEvent)
		{
			return EventResult::kContinue;
		}

		// Apply and send an additional hit event 
		// to trigger an assault alarm and potentially apply bonk/splat damage.
		// Ignore non actors.
		// 
		// If a bonk or splat event, only apply damage 
		// and draw aggro if not in god mode or if hitting a hostile actor.
		// If a normal hit event, only apply damage
		// and draw aggro if the attacker is not P1 and if the actor 
		// is not already hostile to any player.
		bool shouldApplyDamageOrDrawAggro = 
		(
			(
				(hitActor) && 
				(
					((isBonkOrSplatHitEvent) && (!p->isInGodMode || isHostileToAPlayer)) ||
					(!isBonkOrSplatHitEvent && !p->isPlayer1 && !isHostileToAPlayer)
				)
			)
		);
		if (shouldApplyDamageOrDrawAggro)
		{
			// Constructing and applying hit data seems to more consistently trigger
			// both the initial assault bounty (40) 
			// and any subsequent murder bounty (1000).
			// IMPORTANT NOTE: 
			// If companion players commit crimes while P1 is hidden, 
			// no bounty is accrued no matter what.
			RE::HitData hitData{ };
			Util::NativeFunctions::HitData_Ctor(std::addressof(hitData));
			hitData.Populate(glob.player1Actor.get(), hitActor, nullptr);
			hitData.bonusHealthDamageMult =
			hitData.criticalDamageMult =
			hitData.reflectedDamage =
			hitData.resistedPhysicalDamage =
			hitData.resistedTypedDamage =
			hitData.targetedLimbDamage =
			hitData.physicalDamage =
			hitData.totalDamage = 0.0f;

			// Remove sneak attack bonus, if any,
			// to prevent the new P1 hit event from triggering
			// an additional sneak attack bonus.
			hitData.sneakAttackBonus = 1.0f;
			hitData.flags.reset(RE::HitData::Flag::kSneakAttack);

			// Set corresponding flags in the hit data.
			if (a_hitEvent->flags.all(RE::TESHitEvent::Flag::kPowerAttack))
			{
				hitData.flags.set(RE::HitData::Flag::kPowerAttack);
			}

			if (a_hitEvent->flags.all(RE::TESHitEvent::Flag::kBashAttack))
			{
				hitData.flags.set(RE::HitData::Flag::kBash);
			}

			if (a_hitEvent->flags.all(RE::TESHitEvent::Flag::kHitBlocked))
			{
				hitData.flags.set(RE::HitData::Flag::kBlocked);
			}

			// Triggers the hit and sends the event.
			Util::NativeFunctions::Actor_ApplyHitData
			(
				hitActor, std::addressof(hitData)
			);
		}
		else if (!p->isPlayer1)
		{
			// Also send a duplicate hit event with P1 as the aggressor
			// to trigger any OnHit events or effects, but not any assault alarms.

			RE::FormID sourceFID = a_hitEvent->source;
			auto attackingObj = RE::TESForm::LookupByID(a_hitEvent->source); 

			// Check placeholder spells for the source FID, 
			// and if found, send the copied spell's FID instead.
			if (attackingObj && attackingObj->As<RE::SpellItem>())
			{
				for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i)
				{
					if (p->em->placeholderMagic[i] && 
						p->em->placeholderMagic[i]->formID == a_hitEvent->source && 
						p->em->copiedMagic[i])
					{
						sourceFID = p->em->copiedMagic[i]->formID;
						break;
					}
				}
			}

			// Remove sneak attack flag, if any,
			// to prevent the new P1 hit event from triggering
			// an additional sneak attack bonus.
			auto flags = 
			(
				RE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t>(*a_hitEvent->flags)
			);
			flags.reset(RE::TESHitEvent::Flag::kSneakAttack);
			// Remove our additional hit flags (bonk, slap, and splat starting at 1 << 4).
			flags = static_cast<RE::TESHitEvent::Flag>((!(*flags) & ((1 << 4) - 1)));
			Util::SendHitEvent
			(
				glob.player1Actor.get(),
				a_hitEvent->target.get(), 
				sourceFID,
				a_hitEvent->projectile, 
				flags
			);
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
			SPDLOG_INFO("[Events] Registered for load game events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for load game events.");
		}
	}

	EventResult CoopLoadGameEventHandler::ProcessEvent
	(
		const RE::TESLoadGameEvent* a_loadGameEvent, RE::BSTEventSource<RE::TESLoadGameEvent>*
	)
	{
		// Tear down any active co-op session when P1 loads a save.
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
			SPDLOG_INFO("[Events] Registered for menu open/close events.");
		}
		else
		{ 
			SPDLOG_ERROR("[Events] ERR: Could not register for menu open/close events."); 
		}
	}

	EventResult CoopMenuOpenCloseHandler::ProcessEvent
	(
		const RE::MenuOpenCloseEvent* a_menuEvent, RE::BSTEventSource<RE::MenuOpenCloseEvent>*
	) 
	{
		if (!glob.globalDataInit)
		{
			return EventResult::kContinue;
		}

		bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();

		SPDLOG_DEBUG
		(
			"[Events] |MENU EVENT|: "
			"menu name {}, {}, menu CIDs: current {}, prev: {}, manager: {}, empty data: {}. "
			"Only always open: {}. "
			"Copied data types: 0x{:X}",
			a_menuEvent->menuName, 
			a_menuEvent->opening ? "OPENING" : "CLOSING",
			glob.menuCID,
			glob.prevMenuCID,
			glob.mim->managerMenuCID,
			glob.serializablePlayerData.empty(),
			onlyAlwaysOpen,
			*glob.copiedPlayerDataTypes
		);
			
		// Have to modify P1's level threshold 
		// each time the Stats menu opens and the LevelUp menu closes 
		// if changes were made to the XP threshold mult.
		// Modifying the threshold right as the LevelUp menu opens 
		// will not always see changes reflected in P1's XP total 
		// even after P1's current level increases by 1,
		// so we have to do it after the menu closes 
		// and the game is fully done leveling up P1.
		if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled &&
			Settings::fLevelUpXPThresholdMult != 1.0f) 
		{
			bool statsMenuOpening = 
			(
				a_menuEvent->menuName == RE::StatsMenu::MENU_NAME && a_menuEvent->opening
			);
			bool levelupMenuClosing = 
			(
				a_menuEvent->menuName == RE::LevelUpMenu::MENU_NAME && !a_menuEvent->opening
			);
			auto p1 = RE::PlayerCharacter::GetSingleton(); 
			if ((p1) && (statsMenuOpening || levelupMenuClosing))
			{
#ifdef ALYSLC_DEBUG_MODE
				float playerXP = p1->skills->data->xp;
				float playerXPThreshold = p1->skills->data->levelThreshold;
				float fXPLevelUpMult = glob.defXPLevelUpMult;
				float fXPLevelUpBase = glob.defXPLevelUpBase;
				auto valueOpt = Util::GetGameSettingFloat("fXPLevelUpMult");
				if (valueOpt.has_value())
				{
					fXPLevelUpMult = valueOpt.value();
				}

				if (valueOpt = Util::GetGameSettingFloat("fXPLevelUpBase"); valueOpt.has_value())
				{
					fXPLevelUpBase = valueOpt.value();
				}

				SPDLOG_DEBUG
				(
					"[Events] Menu Open/Close Event: BEFORE: {} menu {}. "
					"P1 XP: {}, XP threshold: {}, XP mult/base: {}, {}",
					levelupMenuClosing ? "LevelUp" : "Stats",
					a_menuEvent->opening ? "opening" : "closing",
					playerXP, playerXPThreshold,
					fXPLevelUpMult, fXPLevelUpBase
				);
#endif

				GlobalCoopData::ModifyLevelUpXPThreshold(glob.coopSessionActive);

#ifdef ALYSLC_DEBUG_MODE
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

				SPDLOG_DEBUG
				(
					"[Events] Menu Open/Close Event: AFTER: {} menu {}. "
					"P1 XP: {}, XP threshold: {}, XP mult/base: {}, {}",
					levelupMenuClosing ? "LevelUp" : "Stats",
					a_menuEvent->opening ? "opening" : "closing", 
					playerXP, playerXPThreshold,
					fXPLevelUpMult, fXPLevelUpBase
				);
#endif
			}
		}

		//=======================================
		// Special processing for specific menus:
		//=======================================

		const auto ui = RE::UI::GetSingleton();
		auto msgQ = RE::UIMessageQueue::GetSingleton();
		if (ui && 
			glob.allPlayersInit && 
			a_menuEvent->opening && 
			a_menuEvent->menuName == RE::LoadingMenu::MENU_NAME)
		{
			// Close message box menu ('You died' message).
			if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME)) 
			{
				if (msgQ)
				{
					msgQ->AddMessage
					(
						RE::MessageBoxMenu::MENU_NAME,
						RE::UI_MESSAGE_TYPE::kForceHide, 
						nullptr
					);
				}
			}

			if (glob.coopSessionActive) 
			{
				// Helps prevent 2H weapon animation stuttering bug due to corrupted equip slots.
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
#ifdef ALYSLC_DEBUG_MODE
			SPDLOG_DEBUG("[Events] ===========[Menu Map BEGIN]===========");
			for (auto& menu : ui->menuMap)
			{
				if (ui->IsMenuOpen(menu.first))
				{
					SPDLOG_DEBUG
					(
						"[Events] Menu {} is open. Pauses game: {}, always open: {}. "
						"Flags: 0b{:B}.", 
						menu.first,
						menu.second.menu->PausesGame(),
						menu.second.menu->AlwaysOpen(),
						*menu.second.menu->menuFlags
					);
				}
			}

			SPDLOG_DEBUG("[Events] ===========[Menu Map END]===========");

			SPDLOG_DEBUG("[Events] ===========[Menu Stack BEGIN]===========");

			for (auto iter = ui->menuStack.begin(); iter != ui->menuStack.end(); ++iter)
			{
				const auto& menu = *iter;
				for (const auto& [name, menuEntry] : ui->menuMap)
				{
					if (menuEntry.menu == menu)
					{
						SPDLOG_DEBUG
						(
							"[Events] Index {}: Menu {} is open. "
							"Pauses game: {}, always open: {}. Flags: 0b{:B}. "
							"Mouse/controller counts: {}, {}. Input context: 0b{:B}.",
							iter - ui->menuStack.begin(),
							name,
							menu->PausesGame(),
							menu->AlwaysOpen(),
							*menu->menuFlags,
							menu->uiMovie ?
							menu->uiMovie->GetMouseCursorCount() :
							-1,
							menu->uiMovie ?
							menu->uiMovie->GetControllerCount() :
							-1,
							static_cast<uint32_t>(menu->inputContext.underlying())
						);
					}
				}
			}

			SPDLOG_DEBUG("[Events] ===========[Menu Stack END]===========");

			if (auto controlMap = RE::ControlMap::GetSingleton(); controlMap) 
			{
				SPDLOG_DEBUG("[Events] ===========[Menu Priority Stack BEGIN]===========");

				for (auto i = 0; i < controlMap->contextPriorityStack.size(); ++i)
				{
					const auto& context = controlMap->contextPriorityStack[i];
					SPDLOG_DEBUG("[Events] Index {}: context: {}.", i, context);
				}

				SPDLOG_DEBUG("[Events] ===========[Menu Priority Stack END]===========");
			}
#endif

			// Open the ALYSLC overlay if it isn't open already.
			if (!ui->IsMenuOpen(DebugOverlayMenu::MENU_NAME))
			{
				SPDLOG_DEBUG("[Events] Menu Open/Close Event: ALYSLC overlay not open. Opening.");
				DebugOverlayMenu::Load();
			}

			// [Enderal]: 
			// Keep perks synced among all players whenever a MessageBox menu opens/closes.
			// Can't differentiate between a regular MessageBox and Enderal's level up MessageBox,
			// so we've got to sync whenever a MessageBox opens.
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled &&
				a_menuEvent->menuName == RE::MessageBoxMenu::MENU_NAME)
			{
				GlobalCoopData::SyncSharedPerks();
			}

			// Reset dialogue control CID when dialogue menu opens and closes.
			if (GlobalCoopData::TRANSFERABLE_CONTROL_MENU_NAMES.contains(a_menuEvent->menuName))
			{
				{
					std::unique_lock<std::mutex> lock
					(
						glob.moarm->reqTransferMenuControlMutex, std::try_to_lock
					);
					if (lock)
					{
						SPDLOG_DEBUG
						(
							"[Events] Menu Open/Close Event: {}. Transfer menu control CID. "
							"Lock obtained: (0x{:X}).", 
							a_menuEvent->menuName,
							std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
						glob.moarm->reqTransferMenuControlPlayerCID = -1;
					}
					else
					{
						SPDLOG_DEBUG
						(
							"[Events] Menu Open/Close Event: {}. Transfer menu control CID. "
							"Failed to obtain lock: (0x{:X})", 
							a_menuEvent->menuName,
							std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
					}
				}
			}

			// Record changes in open/close state for supported menus.
			bool wasSupportedMenuOpen = glob.supportedMenuOpen;
			glob.supportedMenuOpen = GlobalCoopData::IsSupportedMenuOpen();
			// Just closed.
			if (wasSupportedMenuOpen && !glob.supportedMenuOpen.load())
			{
				glob.lastSupportedMenusClosedTP = SteadyClock::now();
			}

			//========================================================
			// Check for co-op companion player menu control requests.
			//========================================================

			if (a_menuEvent->opening)
			{
				// Resolve the CID which will modify the requests queue 
				// and clear out fulfilled requests even if we don't need to 
				// set the menu CID to the resolved CID.
				glob.lastResolvedMenuCID = glob.moarm->ResolveMenuControllerID
				(
					a_menuEvent->menuName
				);
				if (glob.mim->IsRunning() && glob.mim->managerMenuCID != -1)
				{
					// Give the companion player continued control of menus.
					GlobalCoopData::SetMenuCIDs(glob.mim->managerMenuCID);
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: OPENING: Set menu CIDs. "
						"Menu input manager running. CIDs are now: {}, {}, {}.", 
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);
				}
				else if (glob.menuCID == -1)
				{
					// Give the player with the resolved CID control of menus.
					GlobalCoopData::SetMenuCIDs(glob.lastResolvedMenuCID);
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: OPENING: Set menu CIDs. "
						"Resolve CID from requests. MIM not running. CIDs are now: {}, {}, {}.",
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: OPENING: Set menu CIDs. "
						"Menu CID is already set. MIM not running. CIDs are now: {}, {}, {}.",
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);
				}
			}

			// Co-op player requesting menu control.
			if (glob.menuCID != -1 && glob.menuCID != glob.player1CID)
			{
				const auto& p = glob.coopPlayers[glob.menuCID];
				const auto menuName = std::string_view(a_menuEvent->menuName.c_str());
				bool isSupportedMenu = glob.SUPPORTED_MENU_NAMES.contains(menuName); 
				if (isSupportedMenu)
				{
					// Companion player's inventory menu (Container Menu) is open
					// and the Tween Menu is already open.
					// Close Tween Menu here since it will not auto-close 
					// after the Container Menu closes.
					if (!a_menuEvent->opening && 
						a_menuEvent->menuName == RE::ContainerMenu::MENU_NAME && 
						ui->IsMenuOpen(RE::TweenMenu::MENU_NAME))
					{
						if (msgQ)
						{
							msgQ->AddMessage
							(
								RE::TweenMenu::MENU_NAME, 
								RE::UI_MESSAGE_TYPE::kForceHide,
								nullptr
							);
						}
					}

					// Set newly opened menu in the menu input manager.
					glob.mim->SetOpenedMenu(a_menuEvent->menuName, a_menuEvent->opening);

					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: Menu {} {} by CID {}, menus open: {}", 
						a_menuEvent->menuName, 
						a_menuEvent->opening ? "opened" : "closed", 
						glob.menuCID, 
						glob.mim->managedCoopMenusCount
					);

					// NOTE:
					// Don't know of a way to hook ProcessMessage() for custom menus, 
					// so we'll copy player data here instead.
					// Must have Maxsu2017's awesome 'Hero Menu Enhanced' mod installed:
					// https://www.nexusmods.com/enderalspecialedition/mods/563
					if (a_menuEvent->menuName == GlobalCoopData::ENHANCED_HERO_MENU)
					{
						GlobalCoopData::CopyOverCoopPlayerData
						(
							a_menuEvent->opening, 
							a_menuEvent->menuName, 
							p->coopActor->GetHandle(),
							nullptr
						);
					}
					else if (a_menuEvent->menuName == RE::CraftingMenu::MENU_NAME)
					{
						// Stop co-op companion from interacting 
						// with crafting station once the menu opens.
						p->pam->SetAndEveluatePackage();
					}

					if (a_menuEvent->opening)
					{
						// Give co-op companion player menu control.
						glob.mim->ToggleCoopPlayerMenuMode(glob.menuCID);
					}
					else if (glob.mim->managedCoopMenusCount == 0 && glob.mim->IsRunning())
					{
						// Stop the MIM and relinquish menu control.
						glob.mim->ToggleCoopPlayerMenuMode(-1);
					}
				}
				else if (a_menuEvent->menuName == RE::LockpickingMenu::MENU_NAME)
				{
					// Start lockpicking task to give the companion player LockpickingMenu control.
					p->taskRunner->AddTask([&p]() { p->LockpickingTask(true); });
				}
			}
			else if (Settings::bTwoPlayerLockpicking && 
					 glob.activePlayers == 2 &&
					 a_menuEvent->opening && 
					 a_menuEvent->menuName == RE::LockpickingMenu::MENU_NAME)
			{
				// P1 is controlling the Lockpicking Menu, but since we have two players,
				// and two player lockpicking is enabled, we can grant the other player 
				// control of rotating the lock.
				// Get the other player.
				for (const auto& otherP : glob.coopPlayers)
				{
					if (otherP->isActive && !otherP->isPlayer1)
					{
						// Start lockpicking task to give the companion player
						// partial Lockpicking Menu control.
						otherP->taskRunner->AddTask
						(
							[&otherP]() { otherP->LockpickingTask(false); }
						);
					}
				}
			}
			else if (a_menuEvent->opening && a_menuEvent->menuName == RE::FavoritesMenu::MENU_NAME)
			{
				// Update Favorites Menu item entries with quick slot item/spell tags for P1.
				glob.mim->InitP1QSFormEntries();
			}

			if (!a_menuEvent->opening)
			{
				if (onlyAlwaysOpen && glob.menuCID != -1)
				{
					// Reset menu controller IDs once all menus are closed.
					GlobalCoopData::ResetMenuCIDs();
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: CLOSING: "
						"MIM signalled to close. Reset menu CIDs. CIDs are now: {}, {}, {}.",
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);
				}
				else if (!onlyAlwaysOpen && 
						 glob.mim->managedCoopMenusCount == 0 && 
						 glob.menuCID != glob.player1CID)
				{
					// Co-op companion player relinquishes control of their menus
					// but at least one is still open, so give control to P1.
					GlobalCoopData::ResetMenuCIDs();
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: CLOSING: "
						"MIM signalled to close with controllable menus open. "
						"Give control to P1. CIDs are now: {}, {}, {}.",
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);
				}
				else
				{
					SPDLOG_DEBUG
					(
						"[Events] Menu Open/Close Event: CLOSING: "
						"Fallthrough: only always open: {}, MIM managed menus count: {}. "
						"CIDs are now: {}, {}, {}.",
						onlyAlwaysOpen,
						glob.mim->managedCoopMenusCount,
						glob.menuCID,
						glob.prevMenuCID,
						glob.mim->managerMenuCID
					);

					// If no menus are open, the managed menus count should be 0.
					// Stop the MIM and reset menu controller IDs if not.
					if (onlyAlwaysOpen && glob.mim->managedCoopMenusCount > 0) 
					{
						SPDLOG_DEBUG
						(
							"[Events] ERR: Menu Open/Close Event: CLOSING: "
							"MIM managed menus count should be 0 here. "
							"Pausing menu input manager and resetting menu CIDs."
						);
						glob.mim->ToggleCoopPlayerMenuMode(-1);
						GlobalCoopData::ResetMenuCIDs();
					}
				}
			}
		}
		else if (glob.mim->IsRunning() && glob.mim->managedCoopMenusCount > 0)
		{
			// Prevents open menus count from being positive once the session ends,
			// a bug which can be produced by opening the Summoning Menu
			// with a companion player-controlled menu open (e.g. Dialogue menu).
			// This bug will keep the MIM running 
			// even without any co-op player-controlled menus open 
			// and lead to co-op player inputs affecting P1 due to emulated keypresses.
			// Give P1 control here.
			Events::ResetMenuState();
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
			SPDLOG_INFO("[Events] Registered for cell fully loaded events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for cell fully loaded events.");
		}
	}

	EventResult CoopCellFullyLoadedHandler::ProcessEvent
	(
		const RE::TESCellFullyLoadedEvent* a_cellFullyLoadedEvent,
		RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*
	)
	{
		if (!a_cellFullyLoadedEvent || !a_cellFullyLoadedEvent->cell)
		{
			return EventResult::kContinue;
		}
		
		SPDLOG_DEBUG
		(
			"[Events] Cell {} (0x{:X}) fully loaded. Resetting fade on all objects in cell. "
			"Is loading a save: {}.",
			a_cellFullyLoadedEvent->cell->GetName(),
			a_cellFullyLoadedEvent->cell->formID,
			glob.loadingASave
		);

		// Reset fade on all the cell's objects, since our co-op cam
		// may have left some fade values modified.
		Util::ResetFadeOnAllObjectsInCell(a_cellFullyLoadedEvent->cell);

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
			SPDLOG_INFO("[Events] Registered for position player events.");
		}
		else
		{
			SPDLOG_ERROR("[Events] ERR: Could not register for position player events.");
		}
	}

	EventResult CoopPositionPlayerEventHandler::ProcessEvent
	(
		const RE::PositionPlayerEvent* a_positionPlayerEvent, 
		RE::BSTEventSource<RE::PositionPlayerEvent>* a_eventSource
	)
	{
		// NOTE: 
		// Needs testing.
		// Would like to see if this event fires and covers cases
		// when the player validity checks in the player manager 
		// do not signal to move companion players to P1. 
		// Examples where this could occur include short teleports 
		// with a fader menu opening and closing.
		// Used to clear grabbed actors/released refrs before P1 moves
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

		// Fader Menu paired with a P1 move event
		// typically indicates a need to move all other players to P1.
		// Do not want to move all other players to P1 if P1 is attempting to
		// teleport to another player, which will also trigger a position player event,
		// but not open the Fader Menu.
		auto ui = RE::UI::GetSingleton();
		if (ui && !ui->IsMenuOpen(RE::FaderMenu::MENU_NAME))
		{
			return EventResult::kContinue;
		}

		SPDLOG_DEBUG
		(
			"[Events] Position Player Event: {}. Should move players to P1: {}.",
			*a_positionPlayerEvent->type,
			a_positionPlayerEvent->type == RE::PositionPlayerEvent::EVENT_TYPE::kFinish
		);
		bool preMove = 
		(
			a_positionPlayerEvent->type == RE::PositionPlayerEvent::EVENT_TYPE::kPre ||
			a_positionPlayerEvent->type == RE::PositionPlayerEvent::EVENT_TYPE::kPreUpdatePackages
		);
		if (preMove) 
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive) 
				{
					continue;
				}

				// Pause first, if running.
				if (p->IsRunning()) 
				{
					SPDLOG_DEBUG
					(
						"[Events] Position Player Event: P{}'s managers are running. "
						"Pausing now before P1 moves.", 
						p->playerID + 1
					);
					p->RequestStateChange(ManagerState::kPaused);
				}
				
				// Clear all released refrs and also grabbed actors,
				// since I haven't figured out a consistent, 
				// bug-free way of moving grabbed actors through load doors yet.
				p->tm->rmm->ClearReleasedRefrs();
				p->tm->rmm->ClearGrabbedActors(p);
				SPDLOG_DEBUG
				(
					"[Events] Position Player Event: Pre-move: "
					"Cleared P{}'s managed grabbed and released refrs.", 
					p->playerID + 1
				);
			}
		}

		bool postMove = 
		(
			a_positionPlayerEvent->type == RE::PositionPlayerEvent::EVENT_TYPE::kFinish
		);
		if (postMove) 
		{
			auto p1 = RE::PlayerCharacter::GetSingleton();
			bool player1Valid = Util::ActorIsValid(p1);
			if (!player1Valid) 
			{
				SPDLOG_DEBUG
				(
					"[Events] ERR: Position Player Event: P1 is not valid after event finished."
				);
				return EventResult::kContinue;
			}

			for (const auto& p : glob.coopPlayers) 
			{
				if (!p->isActive)
				{
					continue;
				}

				// Pause if running still.
				if (p->IsRunning())
				{
					SPDLOG_DEBUG
					(
						"[Events] Position Player Event: P{}'s managers are running. "
						"Pausing now after P1 moved.",
						p->playerID + 1
					);
					p->RequestStateChange(ManagerState::kPaused);
				}

				if (!p->isPlayer1) 
				{
					SPDLOG_DEBUG("[Events] Position Player Event: About to move player {} to P1.", 
						p->coopActor->GetName());
					// Sheathe before moving to minimize incidence of equip state bugs.
					p->pam->ReadyWeapon(false);
					auto taskInterface = SKSE::GetTaskInterface();
					if (taskInterface)
					{
						taskInterface->AddTask
						(
							[p, p1]()
							{
								SPDLOG_DEBUG
								(
									"[Events] Position Player Event: Now moving player {} to P1.", 
									p->coopActor->GetName()
								);
								p->coopActor->MoveTo(p1); 
							}
						);
					}
				}

				// TODO:
				// Also move all the player's grabbed refrs and actors to P1.
				// p->tm->rmm->MoveUnloadedGrabbedObjectsToPlayer(p);
				
				SPDLOG_DEBUG
				(
					"[Events] Position Player Event: Post-move: "
					"Should clear P{}'s managed grabbed and released refrs.", 
					p->playerID + 1
				);
				// For now, clear all managed refrs.
				p->tm->rmm->ClearAll();
				SPDLOG_DEBUG
				(
					"[Events] Position Player Event: Post-move: "
					"Cleared P{}'s managed grabbed and released refrs.", 
					p->playerID + 1
				);
			}
		}

		return EventResult::kContinue;
	}
}

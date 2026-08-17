#include "EquipManager.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	// The equipping/unequipping system for NPCs is convoluted and does not work at times.
	// Much sanity was lost creating and debugging this manager's functions.
	// Truly the embodiment of "it just works". Sometimes anyways.
	EquipManager::EquipManager() :
		Manager(ManagerType::kEM)
	{ }

	void EquipManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p && 
			a_p->deviceID > -1 && 
			a_p->playerID > -1 &&
			a_p->playerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			DBG
			(
				"Constructor for {} (0x{:X}), PID, DID: {}, {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p && p->coopActor ? p->coopActor->formID : 0xDEAD,
				p ? p->playerID : -1,
				p ? p->deviceID : -1,
				p.use_count()
			);
			// Init once.
			if (!glob.allPlayersInit)
			{
				lastCycledSpellCategory = 
				lhSpellCategory = 
				rhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
				lastCycledWeaponCategory =
				lhWeaponCategory = 
				rhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
				currentCycledLHSpellsList.fill(nullptr);
				currentCycledRHSpellsList.fill(nullptr);
				currentCycledLHWeaponsList.fill(nullptr);
				currentCycledRHWeaponsList.fill(nullptr);
				favoritedEmoteIdles.fill(""sv);
				currentCycledAmmo = currentCycledVoiceMagic = nullptr;
				currentCycledIdleIndexPair = { "IdleStop"sv, -1 };
				lastChosenHotkeyedForm = nullptr;
				lastCycledIdleIndexPair = currentCycledIdleIndexPair;
				lastCycledForm = nullptr;
			}
			
			RefreshData();
		}
		else
		{
			ERR
			(
				"Cannot construct Equip Manager for device ID {}, player ID {}.", 
				a_p ? a_p->deviceID : -1,
				a_p ? a_p->playerID : -1
			);
		}
	}

	void EquipManager::MainTask()
	{
		// Re-equip forms that were automatically unequipped by the game.
		// Temporary solution until figuring out how to prevent the game from auto-equipping
		// the "best" gear for co-op actors.
		
		// Controller error check.
		XINPUT_STATE tempState{ };
		ZeroMemory(&tempState, sizeof(XINPUT_STATE));
		if (XInputGetState(deviceID, &tempState) != ERROR_SUCCESS)
		{
			return;
		}

		// P1 does not require assistance, leave them be.
		if (p->isPlayer1)
		{
			return;
		}

		// Must re-equip hand forms after teleporting to P1 because the game typically unequips 
		// whatever items are in the player's hands.
		std::unique_lock<std::mutex> lock(extReEquipHandFormsMutex, std::try_to_lock);
		if (lock)
		{
			if (extReEquipHandForms)
			{
				DBG("{}: Re-equip hand forms on request.", coopActor->GetName());
				ReEquipHandForms();
				// Draw weapons/magic after re-equipping because they are sheathed 
				// when the re-equip request is made.
				if (!coopActor->IsWeaponDrawn())
				{
					p->pam->ReadyWeapon(true);
				}

				extReEquipHandForms = false;
			}
		}

		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return;
		}

		auto invChanges = 
		(
			p->isPlayer1 ? 
			coopActor->GetInventoryChanges() :
			inventoryChest->GetInventoryChanges()
		);
		// Favorited itens may have changed.
		if (invChanges && invChanges->changed)
		{
			DBG("Inventory changed.");
			UpdateFavoritedFormsLists(true);
			invChanges->changed = false;
		}
	}

	void EquipManager::PrePauseTask()
	{
		// Sheathe before awaiting refresh because the equip state can easily glitch 
		// upon moving the companion player with MoveTo().
		// (missing spell hand glow, casters failing to fire, stuttering animations).
		/*if (!p->isPlayer1 && nextState == ManagerState::kAwaitingRefresh)
		{
			p->pam->ReadyWeapon(false);
		}*/

		return;
	}

	void EquipManager::PreStartTask()
	{
		DBG("P{}", playerID + 1);

		if (!p->isPlayer1)
		{
			// For companion players, move everything that isn't equipped 
			// and already present in the inventory chest to the inventory chest
			// if resuming after awaiting refresh or if uninitialized.
			if (p->extRefreshData ||
				currentState == ManagerState::kAwaitingRefresh ||
				currentState == ManagerState::kUninitialized) 
			{
				// Do not remove items when unequipping because this leads to a failed transfer.
				// So we skip equip processing.
				skipEquipProcessing = true;
				PrepInventoriesForCoop();
				skipEquipProcessing = false;
			}
		}

		// Make sure the player's inventory is ready for co-op.
		FixInventory();
		// Update our cached equip state for P1 if just starting co-op.
		if ((p->isPlayer1) && 
			(
				p->extRefreshData || currentState == ManagerState::kAwaitingRefresh
			))
		{
			RefreshEquipState(RefreshSlots::kAll);
		}

		// Don't re-equip items when transformed or when just unpausing without a data refresh.
		if (!p->isTransformed && currentState != ManagerState::kPaused) 
		{
			// Unequip all if the player is not a humanoid.
			if (!coopActor->HasKeyword(glob.npcKeyword))
			{
				desiredForms.fill(nullptr);
				desiredExtraDataLists.fill(nullptr);
				equippedForms.fill(nullptr);
				UnequipAll();

				for (auto i = 0; i < RE::Actor::SlotTypes::kTotal; ++i)
				{
					if (auto caster = coopActor->magicCasters[i]; caster)
					{
						caster->ClearMagicNode();
						caster->currentSpell = nullptr;
					}
				}

				coopActor->InterruptCast(false);
			}
			else if (p->isPlayer1)
			{
				// For P1, only re-equip hand forms, as the equip state for them may be glitched.
				// ReEquipHandForms();
			}
			else
			{
				// Re-equip all saved forms for companion players
				// in case there was some lingering glitched equip state.
				//p->pam->ReadyWeapon(false);
				ReEquipAll(false);
				//p->pam->ReadyWeapon(true);
			}
			
			// Fixes skin glow/tone mismatches.
			// IMPORTANT:
			// Resetting while on horseback causes horse warp glitch upon resumption.
			if (!coopActor->IsOnMount())
			{
				if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface)
				{
					taskInterface->AddTask
					(
						[this]()
						{
							DBG("{}: Reset3D.", coopActor->GetName());
							coopActor->UpdateHairColor();
							coopActor->UpdateSkinColor();
							if (auto actorBase = coopActor->GetActorBase(); actorBase)
							{
								actorBase->UpdateNeck(coopActor->GetFaceNodeSkinned());
							}

							coopActor->Update3DModel();
							coopActor->DoReset3D(true);
						}
					);
				}
			}
		}

		// Reset weapon speed multiplier, which may have been modified.
		coopActor->SetActorValue(RE::ActorValue::kWeaponSpeedMult, 0.0f);
		// Ensure player is visible.
		coopActor->SetAlpha(1.0f);
		// Clear all lingering shader effects.
		Util::StopAllEffectShaders(coopActor.get());

		// Draw weapons/magic if data was refreshed.
		/*if (currentState != ManagerState::kPaused)
		{
			p->pam->ReadyWeapon(true);
		}*/
	}

	void EquipManager::RefreshData()
	{
		// Player data.
		coopActor = RE::ActorPtr{ p->coopActor };
		deviceID = p->deviceID;
		playerID = p->playerID;
		skipEquipProcessing = false;
		// Get serialized data to initialize some data members.
		const auto& data = glob.serializablePlayerData.at(coopActor->formID);
		// Inventory chest.
		inventoryChest = glob.coopInventoryChests[data->GetPlayerCharacterID()];

		// Spells and quick slot forms.
		quickSlotItem = nullptr;
		quickSlotSpell = nullptr;
		voiceForm = nullptr;
		voiceSpell = nullptr;

		// Cached bound weapons the player last requested to equip.
		lastReqBoundWeapLH = 
		lastReqBoundWeapRH = nullptr;

		// Armor ratings for XP calc.
		armorRatings.first = armorRatings.second = 0.0f;

		// Spells copied to placeholder spells. Retrieve from serialized data.
		copiedMagic.fill(nullptr);
		copiedMagic = data->copiedMagic;
		// Copied placeholder spell form ids.
		copiedMagicFormIDs.fill(0);
		for (uint8_t i = 0; i < copiedMagic.size(); ++i)
		{
			copiedMagicFormIDs[i] = copiedMagic[i] ? copiedMagic[i]->formID : 0;
		}

		// Placeholder spells.
		placeholderMagic.fill(nullptr);
		for (uint8_t i = 0; i < !PlaceholderMagicIndex::kTotal; ++i)
		{
			placeholderMagic[i] = 
			(
				glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * playerID + i]
			);
		}

		// Favorited items.
		favoritedFormIDs.clear();
		favoritedForms.clear();
		favoritedEmoteIdles = data->cyclableEmoteIdleEvents;

		// Hotkeyed forms.
		hotkeyedForms = data->hotkeyedForms;
		hotkeyedFormsToSlotsSetMap.clear();
		lastChosenHotkeyedForm = nullptr;

		// Favorited/equipped forms maps and lists.
		cyclableFormsMap.clear();
		desiredForms.fill(nullptr);
		desiredExtraDataLists.fill(nullptr);
		equippedForms.fill(nullptr);

		// Favorites list indices for equipped quick slot forms
		// (quick slot item, quick slot spell).
		equippedQSItemIndex = -1;
		equippedQSSpellIndex = -1;
		// Highest known shout variation for the current equipped shout.
		highestShoutVarIndex = -1;

		// Multithreaded access.
		// Need to re-equip after refreshing all data and restarting this manager.
		// Otherwise, spell visuals (hand glow) can bug out 
		// until sheathing and drawing weapons again.
		{
			std::unique_lock<std::mutex> lock(extReEquipHandFormsMutex);
			extReEquipHandForms = true;
		}

		// Apply serializd equip state.
		SetInitialEquipState();
		// Pull in our serialized favorited forms lists for items and magic.
		UpdateFavoritedFormsLists(true);

		DBG("{}.", coopActor ? coopActor->GetName() : "NONE");
	}

	const ManagerState EquipManager::ShouldSelfPause()
	{
		// Suspension triggered externally.
		return currentState;
	}

	const ManagerState EquipManager::ShouldSelfResume()
	{
		// Resumption triggered externally.
		return currentState;
	}

	// NOTE:
	// Currently unused since I cannot properly execute the 'Shout' package procedure
	// when running the ranged attack package on companion players.
	/*
	RE::TESShout* EquipManager::CopyToPlaceholderShout(RE::TESShout* a_shoutToCopy)
	{
		// Copy selected spell into placeholder spell in the same hand slot
		RE::TESShout* copiedShoutToEquip = nullptr;
		if (a_shoutToCopy)
		{
			auto placeholderVoiceForm = placeholderMagic[!PlaceholderMagicIndex::kVoice];
			// Do not re-copy.
			if (a_shoutToCopy == placeholderVoiceForm)
			{
				return a_shoutToCopy;
			}

			copiedShoutToEquip = glob.placeholderShouts[playerID];
			copiedShoutToEquip->Copy(a_shoutToCopy);
			copiedShoutToEquip->variations[0] = a_shoutToCopy->variations[0];
			copiedShoutToEquip->variations[1] = a_shoutToCopy->variations[1];
			copiedShoutToEquip->variations[2] = a_shoutToCopy->variations[2];
			copiedShoutToEquip->fullName = 
			(
				RE::BSFixedString("[Co-op] " + std::string(a_shoutToCopy->fullName.c_str()))
			);

			// Ensure the placeholder shout does not show in the UI.
			if (copiedShoutToEquip)
			{
				for (uint32_t i = 0; i < RE::TESShout::VariationIDs::kTotal; ++i) 
				{
					auto variation = copiedShoutToEquip->variations[i];
					if (variation.spell)
					{
						if (variation.spell->avEffectSetting) 
						{
							variation.spell->avEffectSetting->data.flags.set
							(
								RE::EffectSetting::EffectSettingData::Flag::kHideInUI
							);
							variation.word->GetKnown();
						}

						for (auto effect : variation.spell->effects)
						{
							if (effect && effect->baseEffect)
							{
								DBG
								(
									"Shout variation spell {} {} "
									"has archetype: {}",
									variation.spell->GetName(),
									i, 
									effect->baseEffect->GetArchetype()
								);
								effect->baseEffect->data.flags.set
								(
									RE::EffectSetting::EffectSettingData::Flag::kHideInUI)
								);
							}
						}
					}
				}
			}

			copiedShoutToEquip->SetAltered(true);
			DBG
			(
				"Copied shout data to placeholder shout {}.", 
				copiedShoutToEquip->GetName()
			);
			SetCopiedMagicAndFID(a_shoutToCopy, PlaceholderMagicIndex::kVoice);
			// Make sure the cached placeholder magic form is set to the newly copied shout.
			placeholderMagic[!PlaceholderMagicIndex::kVoice] = copiedShoutToEquip;
		}

		return copiedShoutToEquip;
	}
	*/

	RE::ExtraDataList* EquipManager::AddItemFromInventoryChest
	(
		RE::TESBoundObject* a_object, 
		RE::ExtraDataList* a_extraDataList, 
		uint32_t a_count,
		bool a_equipsToLH,
		bool a_keepInChest
	)
	{
		// Add the given item with the given extra data list in the inventory chest
		// to the player's inventory.
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!IMPORTANT NOTE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// NEVER and I mean NEVER set the player themselves as the 'a_fromRefr' arg 
		// in AddObjectToContainer(). Will for some reason corrupt the player actor's pointer
		// in some way that prevents Papyrus casts of that pointer to a script type 
		// that is attached to the player actor. Broke all ALYSLC's companion player scripts
		// and events dispatched to them failed to run the event handling code afterward.
		// Really bad bug. Don't ever do this again.

		if (!a_object || a_count == 0)
		{
			return nullptr;
		}
		
		DBG
		(
			"{}: {} of count {}, extra count {}, list {:p}. {}.", 
			coopActor->GetName(), 
			a_object->GetName(), 
			a_count, 
			a_extraDataList ? a_extraDataList->GetCount() : 0,
			fmt::ptr(a_extraDataList),
			a_equipsToLH ? "LH" : "RH"
		);
		auto equipType = a_object->As<RE::BGSEquipType>();
		RE::ExtraDataList* playerInvExDataList = Util::GetEquippedExtraData
		(
			coopActor.get(), a_object, a_equipsToLH
		);
		// Already equipped, so return the equipped exData list.
		if (playerInvExDataList)
		{
			auto exRank = playerInvExDataList->GetByType<RE::ExtraRank>();
			DBG
			(
				"{}: {} ({:p}) is already equipped: 0x{:X}.",
				coopActor->GetName(),
				a_object->GetName(), 
				fmt::ptr(a_extraDataList),
				exRank ? static_cast<uint32_t>(exRank->rank) : 0x0
			);
			return playerInvExDataList;
		}

		// Check if the same item with the same worn rank exData is already present 
		// in the player's inventory and is just not equipped.
		// If so, return the list and exit, since we don't need to add the same exact
		// item/list combo again.
		playerInvExDataList = Util::GetWornRankExtraDataList
		(
			coopActor.get(), a_object, a_equipsToLH
		);
		// Already has a list with the same worn rank exData,
		// so return the matching unequipped exData list.
		if (playerInvExDataList)
		{
			auto exRank = playerInvExDataList->GetByType<RE::ExtraRank>();
			DBG
			(
				"{}: {} ({:p}) was already equipped previously: 0x{:X}.",
				coopActor->GetName(),
				a_object->GetName(),
				fmt::ptr(playerInvExDataList),
				exRank ? static_cast<uint32_t>(exRank->rank) : 0x0
			);
			return playerInvExDataList;
		}

		// Make a copy of the extra data list before adding to the player's inventory.
		// Using the original extra data list when equipping leads to frequent crashing
		// and I haven't figured out the reason for it yet.
		auto list = Util::CopyExtraDataList(a_extraDataList);
		DBG("{}: Equip {} from sent list {:p} -> {:p}.", 
			coopActor->GetName(), a_object->GetName(), fmt::ptr(a_extraDataList), fmt::ptr(list));
		// Remove copied worn rank mask since we'll set it later instead
		// after atttempting to equip the item.
		auto exRank = list ? list->GetByType<RE::ExtraRank>() : nullptr;
		if (exRank)
		{
			DBG("{}: Clearing worn rank mask for new list {:p}. Was 0x{:X}.", 
				coopActor->GetName(), fmt::ptr(list), static_cast<uint32_t>(exRank->rank));
			exRank->rank &= 0x0000FFFF;
		}

		/*
		* NOTE:
		* Commented out for now, but if adding the item directly 
		* without a container changed event firing is required, uncomment this block.
		auto invChanges = coopActor->GetInventoryChanges();
		if (!invChanges)
		{
			return nullptr;
		}

		RE::InventoryEntryData* existingEntry = nullptr;
		for (auto& entry : *invChanges->entryList) 
		{
			if (entry && entry->object && entry->object == a_object)
			{
				existingEntry = entry;
				break;
			}
		}

		bool addedToExisting = false;
		if (existingEntry && list)
		{
			// Prioritize adding to existing entry's list of lists.
			// Do not want to add additional entries for the same bound object
			// to the player's inventory or this will cause an assert failure 
			// and crash the game when CLib's GetInventory() is called.
			DBG
			(
				"Adding list {:p} to EXISTING entry {:p} for {}.",
				fmt::ptr(list),
				fmt::ptr(existingEntry),
				a_object->GetName()
			);
			existingEntry->AddExtraList(list);
			addedToExisting = true;
		}
		else if (!existingEntry)
		{
			coopActor->AddObjectToContainer(a_object, nullptr, a_count, inventoryChest.get());
			for (auto& entry : *invChanges->entryList) 
			{
				if (entry && entry->object && entry->object == a_object)
				{
					existingEntry = entry;
					if (list)
					{
						entry->AddExtraList(list);
						playerInvExDataList = 
						(
							entry->extraLists ? entry->extraLists->front() : nullptr
						);
					}
					else
					{
						playerInvExDataList = nullptr;
					}

					DBG
					(
						"Adding list {:p} to NEW ADDED entry {:p} for {}. Result: {:p}",
						fmt::ptr(list),
						fmt::ptr(entry),
						a_object->GetName(),
						fmt::ptr(playerInvExDataList)
					);
					break;
				}
			}

			// Fallback to malloc'ing our own entry and adding the list to it
			// if AddObjectToContainer() fails.
			if (!existingEntry)
			{
				// Insert new entry with all the extra list's extra data copied over.
				// Malloc'd using the game's memory manager.
				RE::InventoryEntryData* entry = new RE::InventoryEntryData(a_object, a_count);
				if (!entry)
				{
					DBG
					(
						"ERR: Failed to allocate NEW ENTRY for {}.",
						a_object->GetName()
					);
					return nullptr;
				}

				if (list)
				{
					entry->AddExtraList(list);
					playerInvExDataList = entry->extraLists ? entry->extraLists->front() : nullptr;
					DBG
					(
						"Adding list {:p} to NEW MALLOC'D entry {:p} for {}. Result: {:p}",
						fmt::ptr(list),
						fmt::ptr(entry),
						a_object->GetName(),
						fmt::ptr(playerInvExDataList)
					);
				}
				else
				{
					DBG
					(
						"Adding NEW MALLOC'D entry {:p} for {}.",
						fmt::ptr(entry),
						a_object->GetName()
					);
				}

				invChanges->AddEntryData(entry);
			}
		}
		*/

		// Add at least the number of items given by the extra data list's exCount data.
		// If the requested count is larger than the exCount, 
		// add unmodified versions to make up the difference.
		if (list)
		{
			int32_t listCount = list->GetCount();
			int32_t totalCount = list ? max(listCount, a_count) : a_count;
			if (a_count <= listCount)
			{
				DBG
				(
					"{}: Add {} of list {:p} modified {}.", 
					coopActor->GetName(), 
					listCount,
					fmt::ptr(list), 
					a_object->GetName()
				);
				coopActor->AddObjectToContainer(a_object, list, listCount, inventoryChest.get());
			}
			else
			{
				DBG
				(
					"{}: Add {} of list {:p} modified and {} of unmodified {}.", 
					coopActor->GetName(), 
					listCount,
					fmt::ptr(list),
					a_count - listCount, 
					a_object->GetName()
				);
				coopActor->AddObjectToContainer
				(
					a_object, list, listCount, inventoryChest.get()
				);
				coopActor->AddObjectToContainer
				(
					a_object, nullptr, a_count - listCount, inventoryChest.get()
				);
			}
		}
		else
		{
			DBG
			(
				"{}: Add {} of unmodified {}.", 
				coopActor->GetName(), a_count, a_object->GetName()
			);
			// No exData list given, so add the requested number of unmodified items.
			coopActor->AddObjectToContainer(a_object, nullptr, a_count, inventoryChest.get());
		}
		
		// Only have to search for the added list if it wasn't set above.
		if (!playerInvExDataList && list)
		{
			auto playerInventory = coopActor->GetInventory();
			const auto iter = playerInventory.find(a_object);
			// Set to the player inventory extra data list we have just inserted.
			if (iter != playerInventory.end())
			{
				if (iter->second.second)
				{
					if (!iter->second.second->extraLists ||
						iter->second.second->extraLists->empty())
					{
						ERR
						(
							"{}: {} has no/empty extra lists.", 
							coopActor->GetName(), a_object->GetName()
						);
						playerInvExDataList = nullptr;
					}
					else
					{
						playerInvExDataList = iter->second.second->extraLists->front();
						//DBG("List is now {:p}", fmt::ptr(playerInvExDataList));
						uint32_t j = 0;
						for (const auto exDataList : *iter->second.second->extraLists)
						{
							DBG("Extra data list #{} for {} is {:p}", 
								j, a_object->GetName(), fmt::ptr(exDataList));
							++j;
						}
					}
				}
				else
				{
					ERR
					(
						"ERR: Will fail equip. {} has no inventory entry.", a_object->GetName()
					);
				}
			}
			else
			{
				ERR
				(
					"ERR: Will fail equip. {} not in {}'s inventory.", 
					a_object->GetName(), coopActor->GetName()
				);
			}
		}
		
		DBG
		(
			"FINAL: {:p} in {}.", fmt::ptr(playerInvExDataList), a_equipsToLH ? "LH" : "RH"
		);
		return playerInvExDataList;
	}

	void EquipManager::AutoEquipAmmo(RE::TESBoundObject* a_equippedObject)
	{
		// Equip matching highest count/damage ammo if the setting is enabled 
		// and the given bound object is a ranged weapon.
		// 
		// Must equip highest count/damage ammo for co-op companions, since the ranged package,
		// which used to automatically equip the appropriate ammo, 
		// is no longer used for firing ranged weapons.

		if (Settings::uAmmoAutoEquipMode == !AmmoAutoEquipMode::kNone)
		{
			return;
		}

		auto weap = a_equippedObject ? a_equippedObject->As<RE::TESObjectWEAP>() : nullptr;
		if (!weap)
		{
			return;
		}

		bool isBound = weap->IsBound();
		bool isBow = weap->IsBow();
		bool isCrossbow = weap->IsCrossbow();
		// We already equip bound arrows elsewhere 
		// when a bound bow is equipped.
		// Only auto-equip ammo when equipping a non-bound ranged weapon.
		if ((isBound) || (!isBow && !isCrossbow))
		{
			return;
		}

		auto desiredAmmo = 
		(
			desiredForms[!EquipIndex::kAmmo] ?
			desiredForms[!EquipIndex::kAmmo]->As<RE::TESAmmo>() : 
			nullptr
		);
		// Also only auto-equip if no current ammo, no desired ammo,
		// mismatching current ammo, or if bound ammo is equipped.
		auto currentAmmo = coopActor->GetCurrentAmmo();
		auto equippedAmmo = 
		(
			equippedForms[!EquipIndex::kAmmo] ?
			equippedForms[!EquipIndex::kAmmo]->As<RE::TESAmmo>() : 
			nullptr
		);
		DBG
		(
			"{}: Current ammo: {}, equipped ammo: {}, desired ammo: {}. Is bow/crossbow: {}, {}",
			coopActor->GetName(), 
			currentAmmo ? currentAmmo->GetName() : "NONE",
			equippedAmmo ? equippedAmmo->GetName() : "NONE",
			desiredAmmo ? desiredAmmo->GetName() : "NONE",
			isBow,
			isCrossbow
		);
		if ((!currentAmmo || !equippedAmmo || !desiredAmmo) || 
			(
				(desiredAmmo->IsBolt() && isBow) || 
				(!desiredAmmo->IsBolt() && isCrossbow)
			) ||
			(desiredAmmo->HasKeywordByEditorID("WeapTypeBoundArrow")))
		{
			DBG("{}: Unequip current ammo {}.", 
				coopActor->GetName(), currentAmmo ? currentAmmo->GetName() : "NONE");
			// First, unequip what the game has cached as the current ammo.
			if (currentAmmo && currentAmmo->IsBoundObject())
			{
				UnequipAmmo(currentAmmo);
			}
									
			auto ammoAndCount = 
			(
				Settings::uAmmoAutoEquipMode == 
				!AmmoAutoEquipMode::kHighestCount ?
				Util::GetHighestCountAmmo
				(
					p->isPlayer1 ? coopActor.get() : inventoryChest.get(), isBow
				) :
				Util::GetHighestDamageAmmo
				(
					p->isPlayer1 ? coopActor.get() : inventoryChest.get(), isBow
				)
			);
			if (ammoAndCount.first && 
				ammoAndCount.second > 0 &&
				coopActor->GetCurrentAmmo() != ammoAndCount.first)
			{
				desiredAmmo = ammoAndCount.first;
				DBG
				(
					"{}: Equip highest {} ammo {}. Count: {}.",
					coopActor->GetName(),
					Settings::uAmmoAutoEquipMode == 
					!AmmoAutoEquipMode::kHighestCount ?
					"count" :
					"damage",
					desiredAmmo->GetName(),
					ammoAndCount.second
				);
				auto invChanges = inventoryChest->GetInventoryChanges();
				if (!invChanges)
				{
					return;
				}

				RE::ExtraDataList* frontList = nullptr;
				if (invChanges->entryList)
				{
					for (auto entry : *invChanges->entryList)
					{
						if (!entry)
						{
							continue;
						}

						if (entry->object == desiredAmmo)
						{
							if (entry->extraLists)
							{
								if (entry->extraLists->empty())
								{
									ERR("{}. TAHTS GON BE BUG: {}.",
										entry->object->GetName(), entry->countDelta);
									delete entry->extraLists;
									entry->extraLists = nullptr;
									continue;
								}
								else if (entry->extraLists->front())
								{
									frontList = entry->extraLists->front();
								}
								
							}

							break;
						}
					}
				
				}

				EquipAmmo(desiredAmmo, frontList);
			}
		}
	}

	void EquipManager::ChangeWornRankExData
	(
		RE::TESBoundObject* a_object, 
		bool a_equipsToLH, 
		bool a_add,
		RE::ExtraDataList* a_chestListToChange
	)
	{
		// Add to specified list or remove all worn exRank data from chest extra data lists
		// for the given item.
		// 
		// Will only add/remove if the item is equipped/unequipped in the given hand.
		// If no chest exData list is provided when trying to add exRank data on equip,
		// attempt to find a matching chest exData list for the equipped exData list 
		// in the same hand before adding worn exRank data.
		
		DBG
		(
			"{}, LH: {}, add: {}, list to change: {:p}.",
			a_object ? a_object->GetName() : "NONE", 
			a_equipsToLH,
			a_add, 
			fmt::ptr(a_chestListToChange)
		);

		if (!a_object || !Util::IsEquipableInventoryObject(a_object))
		{
			return;
		}
		
		// Do not add/remove worn rank exData if skipping equip processing.
		if (skipEquipProcessing)
		{
			return;
		}

		const auto playerInvChanges = coopActor->GetInventoryChanges();
		if (!playerInvChanges || !playerInvChanges->entryList)
		{
			return;
		}

		auto equipType = a_object->As<RE::BGSEquipType>();
		auto equipSlot = equipType ? equipType->equipSlot : nullptr;
		// Two handed weapons and shields can visually occupy the left hand slot,
		// but have ExtraWorn data, not ExtraWornLeft data, when equipped.
		bool checkWornLH = 
		(
			a_equipsToLH && 
			equipSlot && 
			equipSlot != glob.bothHandsEquipSlot &&
			equipSlot != glob.shieldEquipSlot
		);
		// Get the equipped extra data list and inventory entry on the player.
		RE::InventoryEntryData* playerInvEntryData = nullptr;
		RE::ExtraDataList* playerEquippedList{ nullptr };
		for (const auto entry : *playerInvChanges->entryList)
		{
			if (!entry)
			{
				continue;
			}

			if (entry->object == a_object)
			{
				for (const auto list : *entry->extraLists)
				{
					if (!list)
					{
						continue;
					}

					if ((!checkWornLH && list->HasType(RE::ExtraDataType::kWorn)) ||
						(checkWornLH && list->HasType(RE::ExtraDataType::kWornLeft)))
					{
						playerInvEntryData = entry;
						playerEquippedList = list;
						break;
					}
				}
			}

			// Got both inventory entry and exData list, so we can exit now.
			if (playerEquippedList)
			{
				break;
			}
		}

		// We need both a player-equipped list and a matching chest list to add worn rank data.
		if (a_add)
		{
			if (!playerEquippedList)
			{
				// No equipped list on the player means the equip failed 
				// and we don't have to add any worn exRank data to the chest 
				// or player extra data lists for the given object.
				DBG
				(
					"ERR: {}: No player-equipped extra data list found for {} in the {} slot.", 
					coopActor->GetName(), a_object->GetName(), a_equipsToLH ? "LH" : "RH/Default"
				);
				return;
			}

			// Retrieve matching chest list if none was given.
			if (!a_chestListToChange)
			{
				a_chestListToChange = Util::FindMatchingExtraDataList
				(
					inventoryChest.get(), a_object, playerEquippedList
				);
				if (!a_chestListToChange)
				{
					DBG("{}: No chest extra data list found for equipped {}.", 
						coopActor->GetName(), a_object->GetName());
					return;
				}
			}
			
		}
		
		// Get entry for the object from the chest.
		const auto chestInvChanges = inventoryChest->GetInventoryChanges();
		if (!chestInvChanges || !chestInvChanges->entryList)
		{
			return;
		}

		RE::InventoryEntryData* chestInvEntryData = nullptr;
		for (const auto entry : *chestInvChanges->entryList)
		{
			if (!entry || entry->object != a_object)
			{
				continue;
			}

			// Get the entry that contains the chest list we'd like to modify.
			if (a_chestListToChange)
			{
				if (entry->extraLists)
				{
					for (auto exDataList : *entry->extraLists)
					{
						if (exDataList == a_chestListToChange)
						{
							chestInvEntryData = entry;
							break;
						}
					}
				}
			}
			else
			{
				// Nothing given so just use the first entry that matches the given object.
				chestInvEntryData = entry;
			}

			// Found the chest list, so we can exit.
			if (chestInvEntryData)
			{
				break;
			}
		}

		// Add if attempted to equip.
		if (a_add)
		{
			DBG
			(
				"{}: Equipped {}'s chest list {:p} as {:p}. Is equipped to LH: {}.",
				coopActor->GetName(), 
				a_object->GetName(), 
				fmt::ptr(a_chestListToChange),
				fmt::ptr(playerEquippedList),
				a_equipsToLH
			);
			// Add extra rank mask for the same hand.
			// To both player and chest lists to keep things in sync.
			// Player.
			Util::AddWornRankExtraData(playerInvEntryData, playerEquippedList, a_equipsToLH);
			// Chest.
			Util::AddWornRankExtraData(chestInvEntryData, a_chestListToChange, a_equipsToLH);
		}

		// Remove all worn rank exData in the same hand if attempted to unequip,
		// or remove every other worn rank exData in th same hand if attempted to equip.
		// There can only be one instance of ExtraWorn and ExtraWornLeft exData
		// per inventory entry for a specific item,
		// so we can remove all other corresponding instances of the same worn rank data.

		if (!chestInvEntryData)
		{
			DBG("{}: {} not found in inventory chest.",
				coopActor->GetName(), a_object->GetName());
			return;
		}

		if (!chestInvEntryData->extraLists)
		{
			DBG("{}: No extra lists found for {} in inventory chest.",
				coopActor->GetName(), a_object->GetName());
			return;
		}

		for (const auto extraDataList : *chestInvEntryData->extraLists)
		{
			if (!extraDataList)
			{
				continue;
			}

			// Skip equipped list which just had worn rank data added to it.
			if (a_add && extraDataList == a_chestListToChange)
			{
				continue;
			}
			
			Util::RemoveWornRankExtraData(a_object, extraDataList, a_equipsToLH);
		}

		if (!playerInvEntryData)
		{
			DBG("{}: {} not found in player inventory.",
				coopActor->GetName(), a_object->GetName());
			return;
		}

		if (!playerInvEntryData->extraLists)
		{
			DBG("{}: No extra lists found for {} in player inventory.",
				coopActor->GetName(), a_object->GetName());
			return;
		}

		for (const auto extraDataList : *playerInvEntryData->extraLists)
		{
			if (!extraDataList)
			{
				continue;
			}

			// Skip equipped list which just had worn rank data added to it.
			if (a_add && extraDataList == playerEquippedList)
			{
				continue;
			}
			
			Util::RemoveWornRankExtraData(a_object, extraDataList, a_equipsToLH);
		}
	}

	void EquipManager::ClearDesiredEquippedForm
	(
		RE::TESForm* a_object, const RE::BGSEquipSlot* a_slot, const EquipIndex& a_equipIndex
	)
	{
		// Remove form from desired forms list at all indices that contain it, 
		// plus any specific index to remove the item from.

		if (!a_object)
		{
			return;
		}

		if (auto asArmor = a_object->As<RE::TESObjectARMO>(); asArmor)
		{
			// Must remove all armor entries that correspond to the requested armor to unequip,
			// since armor pieces can fit into multiple biped slots.
			// Remove from desired equipped forms list.
			auto slotMask = asArmor->bipedModelData.bipedObjectSlots;
			// VERY IMPORTANT:
			// Special shield case. 
			// Also clear LH slot in desired equipped forms list.
			// Otherwise, the shield will linger in the desired forms list at its biped slot index
			// and re-equipping hand forms will equip the shield instead of whatever replaced it.
			bool isShield = asArmor->IsShield();
			for (uint8_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
			{
				auto bitMask = 
				(
					static_cast<RE::BIPED_MODEL::BipedObjectSlot>
					(
						1 << (i - !EquipIndex::kFirstBipedSlot)
					)
				);
				// Form mask contains the bit, 
				// so clear the corresponding desired equipped forms entry.
				if (slotMask.all(bitMask))
				{
					ClearDesiredEquippedFormAtIndex(a_object, i);
				}
			}

			// Clear the hand index as well since the shield occupies both biped indices 
			// and a hand index.
			if (isShield)
			{
				ClearDesiredEquippedFormAtIndex(a_object, !EquipIndex::kLeftHand);
			}
		}
		else if (a_object->As<RE::TESAmmo>())
		{
			// Clear from desired list first.
			ClearDesiredEquippedFormAtIndex(a_object, !EquipIndex::kAmmo);
		}
		else
		{
			// Everything else.
			// Remove from desired equipped forms list before unequipping.
			bool isHandForm = 
			(
				a_equipIndex == EquipIndex::kLeftHand ||
				a_equipIndex == EquipIndex::kRightHand
			);
			if (isHandForm)
			{
				if (a_slot != glob.bothHandsEquipSlot)
				{
					ClearDesiredEquippedFormAtIndex(a_object, !a_equipIndex);
				}
				else
				{
					ClearDesiredEquippedFormAtIndex(a_object, !EquipIndex::kLeftHand);
					ClearDesiredEquippedFormAtIndex(a_object, !EquipIndex::kRightHand);
				}
			}
			else if (a_equipIndex != EquipIndex::kNone)
			{
				ClearDesiredEquippedFormAtIndex(a_object, !a_equipIndex);
			}
		}
	}

	RE::SpellItem* EquipManager::CopyToPlaceholderSpell
	(
		RE::SpellItem* a_spellToCopy, const PlaceholderMagicIndex& a_index
	)
	{
		// Copy the given spell's data to the requested placeholder spell, 
		// save FID of the copied spell, and return the modified placeholder spell.
		// Placeholder spells are used to allow companion players to cast any spell
		// through their ranged attack package.

		if (!a_spellToCopy || 
			!placeholderMagic[!a_index] || 
			placeholderMagic[!a_index]->IsNot(RE::FormType::Spell)) 
		{
			return nullptr;
		}

		// Do not copy a placeholder spell to itself.
		auto placeholderSpellForm = placeholderMagic[!a_index];
		if (a_spellToCopy == placeholderSpellForm)
		{
			return a_spellToCopy;
		}

		// IMPORTANT:
		// Copying over the spell in its entirety using Copy() causes crashes 
		// when cycling certain spells while between casts (e.g. Sparks -> Flames), 
		// and I've yet to dig into the reason for it.
		// Instead, we copy over the magic item data, spell data, and effects + effect setting.
		RE::SpellItem* copiedSpellToEquip = 
		(
			glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * playerID + !a_index]
		);
		copiedSpellToEquip->CopyMagicItemData(a_spellToCopy);
		copiedSpellToEquip->avEffectSetting = a_spellToCopy->avEffectSetting;
		copiedSpellToEquip->data = a_spellToCopy->data;
		copiedSpellToEquip->effects = a_spellToCopy->effects;
		// Modify the full name so we can differentiate
		// between the original and the placeholder spell.
		copiedSpellToEquip->fullName = 
		(
			RE::BSFixedString("[Co-op] " + std::string(a_spellToCopy->fullName.c_str()))
		);
		if (a_spellToCopy->equipSlot != glob.bothHandsEquipSlot)
		{
			// Set equip slot to match placeholder slot.
			if (a_index == PlaceholderMagicIndex::kLH || a_index == PlaceholderMagicIndex::kRH)
			{
				copiedSpellToEquip->SetEquipSlot
				(
					a_index == PlaceholderMagicIndex::kRH ? 
					glob.rightHandEquipSlot : 
					glob.leftHandEquipSlot
				);
			}
			else if (a_index == PlaceholderMagicIndex::kVoice)
			{
				copiedSpellToEquip->SetEquipSlot(glob.voiceEquipSlot);
			}
			else
			{
				// Should not equip 1H/voice spell into 2H placeholder spell.
				return nullptr;
			}
		}
		else
		{
			// Set equip slot to match hand.
			copiedSpellToEquip->SetEquipSlot(glob.bothHandsEquipSlot);
		}

		// Save the copied spell and its FID.
		SetCopiedMagicAndFID(a_spellToCopy, a_index);
		// Make sure the cached placeholder magic form is set to the newly copied spell.
		placeholderMagic[!a_index] = copiedSpellToEquip;

		DBG
		(
			"{}: spell: {} -> {}, index: {}.",
			coopActor->GetName(), 
			a_spellToCopy ? a_spellToCopy->GetName() : "NONE", 
			copiedSpellToEquip ? copiedSpellToEquip->GetName() : "NONE",
			a_index
		);

		return copiedSpellToEquip;
	}

	void EquipManager::CycleAmmo()
	{
		// Pick out next favorited ammo to equip.

		// Update cyclable ammo list.
		SetCyclableFavForms(CyclableForms::kAmmo);
		auto cyclableAmmoList = cyclableFormsMap[CyclableForms::kAmmo];
		// If the player does not have any favorited ammo, return here.
		if (cyclableAmmoList.empty())
		{
			currentCycledAmmo = nullptr;
			return;
		}

		// Cycle weapon-matching ammo if the player has a 2H ranged weapon equipped.
		enum
		{
			kArrow = 0,
			kBolt,
			kEither
		};
		auto ammoTypeToCycle = kEither;
		if (auto rhWeap = GetRHWeapon(); rhWeap && rhWeap->IsRanged())
		{
			ammoTypeToCycle = rhWeap->IsBow() ? kArrow : kBolt;
		}

		// Create list of favorited ammo with the requested type.
		// Index of the current ammo in the new cyclable list.
		int32_t currentCycledAmmoIndex = -1;
		if (ammoTypeToCycle == kEither)
		{
			// Find the index of the currently cycled ammo.
			for (uint32_t i = 0; i < cyclableAmmoList.size(); ++i)
			{
				const auto ammoForm = cyclableAmmoList[i];
				if (!ammoForm)
				{
					continue;
				}

				auto ammo = ammoForm->As<RE::TESAmmo>(); 
				if (!ammo)
				{
					continue;
				}

				// Set the index of the currently cycled ammo.
				if (ammo == currentCycledAmmo)
				{
					currentCycledAmmoIndex = i;
					break;
				}
			}
		}
		else
		{
			// Reconstruct cyclable ammo list, only populating it with ammo of the proper type.
			cyclableAmmoList.clear();
			for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kAmmo].size(); ++i)
			{
				const auto ammoForm = cyclableFormsMap[CyclableForms::kAmmo][i];
				if (!ammoForm)
				{
					continue;
				}

				auto ammo = ammoForm->As<RE::TESAmmo>(); 
				if (!ammo)
				{
					continue;
				}

				// Set the new list's index of the currently cycled ammo.
				if (ammo == currentCycledAmmo)
				{
					currentCycledAmmoIndex = j;
				}

				if ((ammoTypeToCycle == kArrow && !ammo->IsBolt()) ||
					(ammoTypeToCycle == kBolt && ammo->IsBolt()))
				{
					// Ammo type matches, so add it to the list and update the index.
					cyclableAmmoList.emplace_back(ammo);
					++j;
				}
			}

			// If the player does not have any favorited ammo of the correct type, return here.
			if (cyclableAmmoList.empty())
			{
				currentCycledAmmo = nullptr;
				return;
			}
		}

		DBG
		(
			"{}: Has {} cyclable ammo. Current index: {}, ammo: {}.",
			coopActor->GetName(), 
			cyclableAmmoList.size(),
			currentCycledAmmoIndex,
			currentCycledAmmo ? currentCycledAmmo->GetName() : "NONE"
		);

		// Find next ammo to cycle to.
		int32_t nextAmmoIndex = currentCycledAmmoIndex;
		if (currentCycledAmmoIndex == -1)
		{
			// No previously cycled ammo, so pick the first one.
			nextAmmoIndex = 0;
		}
		else
		{
			// Check if the next favorited item's extra data list is the last one
			// in the inventory entry's list of lists, and if so,
			// we can move on to the next weapon in the favorited weapons list; 
			// otherwise, maintain the current weapon index.
			bool shouldMoveToNextIndex = false;
			bool inOtherHand = false;
			GetNextFavoritedExDataList
			(
				currentCycledAmmo, false, shouldMoveToNextIndex, inOtherHand
			);
			if (shouldMoveToNextIndex)
			{
				// With wraparound.
				nextAmmoIndex = 
				(
					currentCycledAmmoIndex == cyclableAmmoList.size() - 1 ? 
					0 : 
					currentCycledAmmoIndex + 1
				);
			}
			else
			{
				nextAmmoIndex = currentCycledAmmoIndex;
			}
		}

		currentCycledAmmo = 
		(
			cyclableAmmoList[nextAmmoIndex] ? 
			cyclableAmmoList[nextAmmoIndex]->As<RE::TESAmmo>() : 
			nullptr
		);

		DBG
		(
			"{}: current cycled ammo: {} from index {} (current: {}). Total: {}.",
			coopActor->GetName(),
			currentCycledAmmo ? currentCycledAmmo->GetName() : "NONE",
			nextAmmoIndex,
			currentCycledAmmoIndex,
			cyclableFormsMap[CyclableForms::kAmmo].size()
		);
	}

	void EquipManager::CycleEmoteIdles()
	{
		// Choose next emote idle to play.

		// Select first emote idle when there was no previously cycled emote idle.
		if (currentCycledIdleIndexPair.second == -1)
		{
			currentCycledIdleIndexPair.first = favoritedEmoteIdles[0];
			currentCycledIdleIndexPair.second = 0;
		}
		else
		{
			// With wraparound.
			currentCycledIdleIndexPair.second = 
			(
				(currentCycledIdleIndexPair.second + 1) % favoritedEmoteIdles.size()
			);
			currentCycledIdleIndexPair.first = 
			(
				favoritedEmoteIdles[currentCycledIdleIndexPair.second]
			);
		}

		DBG
		(
			"{}: current idle: {}, index: {}.",
			coopActor->GetName(),
			currentCycledIdleIndexPair.first,
			currentCycledIdleIndexPair.second
		);
	}

	void EquipManager::CycleHandSlotMagic(bool&& a_rightHand)
	{
		// Choose next favorited hand-slot spell to equip.

		// Update list of cyclable spells.
		SetCyclableFavForms(CyclableForms::kSpell);
		const FavMagicCyclingCategory& category = a_rightHand ? rhSpellCategory : lhSpellCategory;

		// If no spells are favorited, clear current cycled spells list 
		// and reset the category to 'All Favorites' and return.
		if (cyclableFormsMap[CyclableForms::kSpell].empty())
		{
			if (a_rightHand)
			{
				currentCycledRHSpellsList[!category] = nullptr;
				rhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
			}
			else
			{
				currentCycledLHSpellsList[!category] = nullptr;
				lhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
			}

			return;
		}

		std::vector<RE::TESForm*> cyclableSpellsList{ };
		// Index of the current cycled spell in the new cyclable spells list.
		int32_t currentCycledSpellIndex = -1;
		RE::TESForm* spellForm = nullptr;
		RE::TESForm* currentCycledSpellForm = 
		(
			a_rightHand ? 
			currentCycledRHSpellsList[!category] : 
			currentCycledLHSpellsList[!category]
		);

		// Build list of cyclable spells based on the current category 
		// and set the current spell's index.
		// 'j' will hold the number of spells that match the chosen category.
		for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kSpell].size(); ++i)
		{
			spellForm = cyclableFormsMap[CyclableForms::kSpell][i];
			if (!spellForm)
			{
				continue;
			}

			auto spell = spellForm->As<RE::SpellItem>(); 
			if (!spell)
			{
				continue;
			}

			auto spellType = spell->GetSpellType();
			// Not a hand spell.
			if (spellType != RE::MagicSystem::SpellType::kSpell)
			{
				continue;
			}

			// Current spell matches, so set its index.
			if (spell == currentCycledSpellForm)
			{
				currentCycledSpellIndex = j;
			}

			// Match with the chosen category and update the match index.
			switch (category)
			{
			case FavMagicCyclingCategory::kAllFavorites:
			{
				cyclableSpellsList.emplace_back(spell);
				++j;
				continue;
			}
			case FavMagicCyclingCategory::kAlteration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kAlteration)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			case FavMagicCyclingCategory::kConjuration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kConjuration)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			case FavMagicCyclingCategory::kDestruction:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kDestruction)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			case FavMagicCyclingCategory::kIllusion:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kIllusion)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			case FavMagicCyclingCategory::kRestoration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kRestoration)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			case FavMagicCyclingCategory::kRitual:
			{
				// 2H spell.
				if (spell->equipSlot == glob.bothHandsEquipSlot)
				{
					cyclableSpellsList.emplace_back(spell);
					++j;
				}

				continue;
			}
			default:
			{
				continue;
			}
			}
		}

		// If there still are no spells to cycle through, clear current list 
		// and reset the spell category before returning.
		if (cyclableSpellsList.empty())
		{
			if (a_rightHand)
			{
				currentCycledRHSpellsList[!category] = nullptr;
				rhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
			}
			else
			{
				currentCycledLHSpellsList[!category] = nullptr;
				lhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
			}

			return;
		}

		// Now in the new cyclable spells list.
		int32_t nextSpellIndex = currentCycledSpellIndex;
		if (currentCycledSpellIndex == -1)
		{
			// Nothing previously selected, so select the first one.
			nextSpellIndex = 0;
		}
		else
		{
			RE::TESForm* currentlyEquippedForm = 
			(
				equippedForms[a_rightHand ? 
				!EquipIndex::kRightHand : 
				!EquipIndex::kLeftHand]
			);
			if (!p->isPlayer1)
			{
				// Have to get the spell copied to the corresponding placeholder spell.
				auto currentlyEquippedSpell = 
				(
					currentlyEquippedForm ?
					currentlyEquippedForm->As<RE::SpellItem>() : 
					nullptr
				);

				if (currentlyEquippedSpell)
				{
					bool is2HSpell = currentlyEquippedSpell->equipSlot == glob.bothHandsEquipSlot;
					if (is2HSpell)
					{
						currentlyEquippedForm = copiedMagic[!PlaceholderMagicIndex::k2H];
					}
					else if (a_rightHand)
					{
						currentlyEquippedForm = copiedMagic[!PlaceholderMagicIndex::kRH];
					}
					else
					{
						currentlyEquippedForm = copiedMagic[!PlaceholderMagicIndex::kLH];
					}
				}
			}

			// With wrap around.
			nextSpellIndex = 
			(
				currentCycledSpellIndex == cyclableSpellsList.size() - 1 ? 
				0 : 
				currentCycledSpellIndex + 1
			);
		}

		RE::SpellItem* nextSpell = cyclableSpellsList[nextSpellIndex]->As<RE::SpellItem>();
		if (nextSpell)
		{
			// Update cycled spell.
			if (a_rightHand)
			{
				currentCycledRHSpellsList[!rhSpellCategory] = nextSpell;
			}
			else
			{
				currentCycledLHSpellsList[!lhSpellCategory] = nextSpell;
			}
		}

		DBG
		(
			"{}: right hand: {}, spell category {} and currently cycled spell {} from index {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhSpellCategory : lhSpellCategory,
			nextSpell ? nextSpell->GetName() : "NONE",
			nextSpellIndex
		);
	}

	void EquipManager::CycleHandSlotMagicCategory(bool&& a_rightHand)
	{
		// Set favorited spells category to cycle hand-slot spells from.

		// Refresh cyclable spells first.
		SetCyclableFavForms(CyclableForms::kSpell);
		const FavMagicCyclingCategory& initialCategory = 
		(
			a_rightHand ? 
			rhSpellCategory : 
			lhSpellCategory
		);
		FavMagicCyclingCategory newCategory = 
		(
			static_cast<FavMagicCyclingCategory>
			(
				(!initialCategory + 1) % (!FavMagicCyclingCategory::kTotal)
			)
		);

		// Only consider categories that have at least one favorited item.
		// Also stop cycling once the initial category is reached on wraparound.
		while (!HasCyclableSpellInCategory(newCategory) && newCategory != initialCategory)
		{
			newCategory = 
			(
				static_cast<FavMagicCyclingCategory>
				(
					(!newCategory + 1) % (!FavMagicCyclingCategory::kTotal)
				)
			);
		}

		if (a_rightHand)
		{
			rhSpellCategory = newCategory;
		}
		else
		{
			lhSpellCategory = newCategory;
		}

		DBG
		(
			"{}: right hand: {}, spell category is now: {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhSpellCategory : lhSpellCategory
		);
	}

	void EquipManager::CycleVoiceSlotMagic()
	{
		// Choose the next favorited voice slot spell to equip.

		// Update cyclable voice spells first.
		SetCyclableFavForms(CyclableForms::kVoice);
		const auto& cyclableVoiceMagicList = cyclableFormsMap[CyclableForms::kVoice];
		// If the player does not have any favorited powers/shouts, return here.
		if (cyclableVoiceMagicList.empty())
		{
			currentCycledVoiceMagic = nullptr;
			return;
		}

		// Create list of favorited voice magic forms with the requested type.
		int32_t currentCycledVoiceMagicIndex = -1;
		int32_t nextVoiceMagicIndex = currentCycledVoiceMagicIndex;
		for (uint32_t i = 0; i < cyclableVoiceMagicList.size(); ++i)
		{
			// Get the index of the currently cycled voice magic.
			if (cyclableVoiceMagicList[i] == currentCycledVoiceMagic)
			{
				currentCycledVoiceMagicIndex = i;
				break;
			}
		}

		if (currentCycledVoiceMagicIndex == -1)
		{
			nextVoiceMagicIndex = 0;
		}
		else
		{
			// Wrap around.
			nextVoiceMagicIndex = 
			(
				currentCycledVoiceMagicIndex == cyclableVoiceMagicList.size() - 1 ?
				0 : 
				currentCycledVoiceMagicIndex + 1
			);
		}

		currentCycledVoiceMagic = cyclableVoiceMagicList[nextVoiceMagicIndex];

		DBG
		(
			"{}: currently cycled voice magic: {} from index {}.",
			coopActor->GetName(),
			currentCycledVoiceMagic ? currentCycledVoiceMagic->GetName() : "NONE",
			nextVoiceMagicIndex
		);
	}

	void EquipManager::CycleWeaponCategory(bool&& a_rightHand)
	{
		// Set the favorited weapons category to cycle weapons from.

		// Refresh cyclable weapons first.
		SetCyclableFavForms(CyclableForms::kWeapon);
		const FavWeaponCyclingCategory& initialCategory = 
		(
			a_rightHand ? 
			rhWeaponCategory : 
			lhWeaponCategory
		);
		FavWeaponCyclingCategory newCategory = 
		(
			static_cast<FavWeaponCyclingCategory>
			(
				(!initialCategory + 1) % (!FavWeaponCyclingCategory::kTotal)
			)
		);

		// Only consider categories that have at least one favorited item.
		// Also stop cycling once the initial category is reached on wraparound.
		while (!HasCyclableWeaponInCategory(newCategory, a_rightHand) && 
				newCategory != initialCategory)
		{
			newCategory =
			(
				static_cast<FavWeaponCyclingCategory>
				(
					(!newCategory + 1) % (!FavWeaponCyclingCategory::kTotal)
				)
			);
		}

		// Set new category.
		if (a_rightHand)
		{
			rhWeaponCategory = newCategory;
		}
		else
		{
			lhWeaponCategory = newCategory;
		}

		DBG
		(
			"{}: right hand: {}, weapon category is now: {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhWeaponCategory : lhWeaponCategory
		);

	}

	void EquipManager::CycleWeapons(bool&& a_rightHand)
	{
		// Choose the next favorited weapon to equip.

		// Update cyclable weapons list first.
		SetCyclableFavForms(CyclableForms::kWeapon);
		const FavWeaponCyclingCategory& category = 
		(
			a_rightHand ?
			rhWeaponCategory : 
			lhWeaponCategory
		);
		// If no weapons are favorited, clear current list 
		// and reset category before returning here.
		if (cyclableFormsMap[CyclableForms::kWeapon].empty())
		{
			if (a_rightHand)
			{
				currentCycledRHWeaponsList[!rhWeaponCategory] = nullptr;
				rhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
			}
			else
			{
				currentCycledLHSpellsList[!lhWeaponCategory] = nullptr;
				lhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
			}

			return;
		}

		std::vector<RE::TESForm*> cyclableWeaponsList{ };
		RE::TESForm* form = nullptr;
		RE::TESForm* currentCycledWeaponForm = 
		(
			a_rightHand ? 
			currentCycledRHWeaponsList[!category] : 
			currentCycledLHWeaponsList[!category]
		);
		// Index of the current cycled weapon in the new cyclable weapons list.
		int32_t currentCycledWeaponIndex = -1;
		// Build list of favorited weapons in the current category 
		// and set the index of the current weapon.
		// After the loop, 'j' holds the number of cyclable weapons within our chosen category.
		for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kWeapon].size(); ++i)
		{
			form = cyclableFormsMap[CyclableForms::kWeapon][i];
			if (auto equipType = form->As<RE::BGSEquipType>(); equipType)
			{
				// Handle shield and torch first, 
				// so that the switch statement below can handle weapons exclusively.
				bool isShield = 
				(
					form->As<RE::TESObjectARMO>() && form->As<RE::TESObjectARMO>()->IsShield()
				);
				bool isTorch =
				( 
					form->As<RE::TESObjectLIGH>() && 
					form->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry)
				);
				// Incompatible equip slot.
				if ((a_rightHand) && (isShield || isTorch))
				{
					continue;
				}

				// Found the index for the current cycled weapon form.
				if (form == currentCycledWeaponForm)
				{
					currentCycledWeaponIndex = j;
				}

				// Add weapons to the list if they fall into our category.
				if (auto weapon = form->As<RE::TESObjectWEAP>(); weapon)
				{
					switch (category)
					{
					case FavWeaponCyclingCategory::kAllFavorites:
					{
						cyclableWeaponsList.emplace_back(form);
						++j;
						continue;
					}
					case FavWeaponCyclingCategory::kAxe:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandAxe || 
							weapon->HasKeywordString("WeapTypeWarAxe"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kBattleaxe:
					{
						// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && 
							weapon->HasKeywordString("WeapTypeBattleaxe"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kBow:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow ||
							weapon->HasKeywordString("WeapTypeBow"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kCrossbow:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow)
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kDagger:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandDagger ||
							weapon->HasKeywordString("WeapTypeDagger"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kGreatsword:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandSword || 
							weapon->HasKeywordString("WeapTypeGreatsword"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kMace:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandMace || 
							weapon->HasKeywordString("WeapTypeMace"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}

					case FavWeaponCyclingCategory::kShieldAndTorch:
					{
						continue;
					}
					case FavWeaponCyclingCategory::kStaff:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff || 
							weapon->HasKeywordString("WeapTypeStaff"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kSword:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandSword || 
							weapon->HasKeywordString("WeapTypeSword"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kUnique:
					{
						if (weapon->HasKeywordString("WeapTypeUnique"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kWarhammer:
					{
						// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe &&
							weapon->HasKeywordString("WeapTypeWarhammer"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					default:
					{
						continue;
					}
					}
				}
				else if ((isShield || isTorch) &&
						 (category == FavWeaponCyclingCategory::kAllFavorites ||
						  category == FavWeaponCyclingCategory::kShieldAndTorch))
				{
					cyclableWeaponsList.emplace_back(form);
					++j;
					continue;
				}
			}
		}

		// If there still are no cyclable weapons, 
		// clear the current cycled weapon 
		// and reset the category before returning.
		if (cyclableWeaponsList.empty())
		{
			if (a_rightHand)
			{
				currentCycledRHWeaponsList[!rhWeaponCategory] = nullptr;
				rhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
			}
			else
			{
				currentCycledLHWeaponsList[!lhWeaponCategory] = nullptr;
				lhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
			}

			return;
		}

		DBG
		(
			"{}: Has {} cyclable weapons for the {}. Category {}. Current index: {}, weapon: {}.",
			coopActor->GetName(), 
			cyclableWeaponsList.size(),
			a_rightHand ? "RH" : "LH",
			a_rightHand ? rhWeaponCategory : lhWeaponCategory,
			currentCycledWeaponIndex,
			currentCycledWeaponForm ? currentCycledWeaponForm->GetName() : "NONE"
		);

		// In new cyclable list.
		int32_t nextWeaponIndex = currentCycledWeaponIndex;
		if (currentCycledWeaponIndex == -1)
		{
			// Select first weapon if none was cycled before.
			nextWeaponIndex = 0;
		}
		else
		{
			// Check if the next favorited item's extra data list is the last one
			// in the inventory entry's list of lists, and if so,
			// we can move on to the next weapon in the favorited weapons list; 
			// otherwise, maintain the current weapon index.
			bool shouldMoveToNextIndex = false;
			bool inOtherHand = false;
			GetNextFavoritedExDataList
			(
				currentCycledWeaponForm, !a_rightHand, shouldMoveToNextIndex, inOtherHand
			);
			if (shouldMoveToNextIndex)
			{
				// When wrapping around in the "All Favorites" category, 
				// return nullptr to signal the PAFH to equip fists.
				bool wrapAround = currentCycledWeaponIndex == cyclableWeaponsList.size() - 1;
				if (category == FavWeaponCyclingCategory::kAllFavorites && wrapAround)
				{
					nextWeaponIndex = -1;
				}
				else
				{
					nextWeaponIndex = 
					(
						currentCycledWeaponIndex == cyclableWeaponsList.size() - 1 ? 
						0 : 
						currentCycledWeaponIndex + 1
					);
				}
			}
			else
			{
				nextWeaponIndex = currentCycledWeaponIndex;
			}
		}

		RE::TESForm* nextForm = 
		(
			nextWeaponIndex != -1 ? 
			cyclableWeaponsList[nextWeaponIndex] : 
			nullptr
		);
		if (a_rightHand)
		{
			currentCycledRHWeaponsList[!rhWeaponCategory] = nextForm;
		}
		else
		{
			currentCycledLHWeaponsList[!lhWeaponCategory] = nextForm;
		}

		DBG
		(
			"{}: right hand: {}, category is now {}, cycled weapon {} from index {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhWeaponCategory : lhWeaponCategory,
			nextForm ? nextForm->GetName() : "NONE",
			nextWeaponIndex
		);
	}

	void EquipManager::EquipAmmo
	(
		RE::TESForm* a_toEquip,
		RE::ExtraDataList* a_exData, 
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip, 
		bool a_forceEquip, 
		bool a_playSounds,
		bool a_applyNow
	)
	{
		// Equip the given ammo.

		DBG
		(
			"{}: equip {}.", 
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE"
		);

		auto aem = RE::ActorEquipManager::GetSingleton(); 
		auto ammo = a_toEquip ? a_toEquip->As<RE::TESAmmo>() : nullptr; 
		if (!aem || !ammo)
		{
			return;
		}

		if (p->isPlayer1)
		{
			// Unequip current ammo before equipping new one.
			if (RE::TESForm* currentAmmoForm = equippedForms[!EquipIndex::kAmmo]; currentAmmoForm)
			{
				UnequipAmmo(currentAmmoForm);
			}

			Util::EquipObject(coopActor.get(), ammo);
		}
		else
		{
			// NOTE: 
			// The game has issues un/equipping ammo when count is large (e.g. 100000), 
			// so only un/equip 1 at a time.
			HandleCompanionPlayerEquip
			(
				ammo,
				EquipIndex::kAmmo,
				a_exData,
				1, 
				a_slot, 
				a_queueEquip, 
				a_forceEquip, 
				a_playSounds, 
				a_applyNow
			);
		}
	}

	void EquipManager::EquipArmor
	(
		RE::TESForm* a_toEquip, 
		RE::ExtraDataList* a_exData,
		uint32_t a_count, 
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip,
		bool a_forceEquip,
		bool a_playSounds, 
		bool a_applyNow
	)
	{
		// Equip the given armor.
		// IMPORTANT for companion players:
		// Precondition: 
		// Extra data list must be nullptr or a list linked with the bound object
		// in the player's inventory chest, not their true inventory.
		// Matching ex data list is generated if the item isn't already present
		// in the player's inventory and then used on equip.

		DBG
		(
			"{}: equip {} (0x{:X}).",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE",
			a_toEquip ? a_toEquip->formID : 0xDEAD
		);

		auto boundObj = a_toEquip ? a_toEquip->As<RE::TESBoundObject>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton(); 
		if (!boundObj || !aem)
		{
			return;
		}

		if (p->isPlayer1)
		{
			// Directly equip on P1 without forcing the equip.

			// Unequip current form first.
			if (auto asArmor = a_toEquip->As<RE::TESObjectARMO>(); asArmor)
			{
				auto slotMask = asArmor->bipedModelData.bipedObjectSlots;
				bool isShield = asArmor->IsShield();
				const RE::BGSEquipSlot* slot = 
				(
					isShield ? 
					a_toEquip->As<RE::TESObjectARMO>()->equipSlot :
					a_slot
				);
				DBG
				(
					"{}: {} slot mask 0b{:B}.",
					coopActor->GetName(),
					a_toEquip ? a_toEquip->GetName() : "NONE",
					*slotMask
				);
				const auto& biped = coopActor->GetBiped2();
				for (uint8_t i = !EquipIndex::kFirstBipedSlot; 
					 i <= !EquipIndex::kLastBipedSlot; 
					 ++i)
				{
					auto bitMask = 
					(
						static_cast<RE::BIPED_MODEL::BipedObjectSlot>
						(
							1 << (i - !EquipIndex::kFirstBipedSlot)
						)
					);
					if (slotMask.all(bitMask))
					{
						// Unequip armor in same slot first.
						// Check our cached equipped armor in the same slot first.
						auto currentArmorForm = equippedForms[i];
						if (currentArmorForm)
						{
							UnequipArmor(currentArmorForm);
						}
						else
						{
							// If nothing is cached, check the current biped object's item.
							// This does not always match the equipped item unfortunately
							// and is sometimes an armature item, which will not unequip.
							currentArmorForm = 
							(
								biped && biped->objects ? 
								biped->objects[i - !EquipIndex::kFirstBipedSlot].item :
								nullptr
							);
							if (currentArmorForm)
							{
								UnequipArmor(currentArmorForm);
							}
						}
					}
				}
			}

			Util::EquipObject
			(
				coopActor.get(), 
				boundObj, 
				a_exData, 
				a_count, 
				a_slot, 
				a_queueEquip, 
				false, 
				a_playSounds, 
				a_applyNow
			);
		}
		else
		{
			HandleCompanionPlayerEquip
			(
				boundObj,
				EquipIndex::kNone,
				a_exData,
				a_count, 
				a_slot, 
				a_queueEquip, 
				a_forceEquip, 
				a_playSounds, 
				a_applyNow
			);
		}
	}

	void EquipManager::EquipDummy1H(const RE::BGSEquipSlot* a_slot, bool a_clearDesiredSlots)
	{
		// Equip dummy 1H weapon to clear out the given hand slot.
		// NOTE: 
		// Does not clear the desired hand slot form in the same slot.

		DBG("{}.", coopActor->GetName());

		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!aem)
		{
			return;
		}

		// NOTE: 
		// Very important: calling EquipObject() or UnequipObject() for P1 
		// with the a_forceEquip param set to true messes up P1's equip state,
		// which means a previously equipped item will auto-equip 
		// even when trying to equip a different item.
		// NEVER force equip with P1.
		// 
		// Also do not queue the equip here, we want it to happen ASAP.
		if (p->isPlayer1)
		{
			Util::EquipObject
			(
				coopActor.get(), 
				glob.dummy1H,
				nullptr, 
				1, 
				a_slot
			);
		}
		else
		{
			// Equip index arg determines if requesting to clear desired array slots or not.
			if (a_clearDesiredSlots)
			{
				HandleCompanionPlayerEquip
				(
					glob.dummy1H, 
					a_slot == glob.leftHandEquipSlot ? 
					EquipIndex::kLeftHand : 
					EquipIndex::kRightHand,
					nullptr,
					1,
					a_slot
				);
			}
			else
			{
				HandleCompanionPlayerEquip
				(
					glob.dummy1H, 
					EquipIndex::kNone,
					nullptr,
					1,
					a_slot
				);
			}
			
		}
		
		desiredExtraDataLists
		[
			a_slot == glob.rightHandEquipSlot ? !EquipIndex::kLeftHand : !EquipIndex::kRightHand
		] = nullptr;
	}

	void EquipManager::EquipFists(bool a_clearDesiredSlots)
	{
		// Clear out both hand slots by equipping the 'fists' item.
		// Can choose to also clear out desired forms/exData list slots.

		DBG("{}.", coopActor->GetName());
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!aem)
		{
			return;
		}

		// NOTE: 
		// Very important: calling EquipObject() or UnequipObject() for P1 
		// with the a_forceEquip param set to true messes up P1's equip state,
		// which means a previously equipped item will auto-equip 
		// even when trying to equip a different item.
		// NEVER force equip with P1.
		// 
		// Also do not queue the equip here, we want it to happen ASAP.

		if (p->isPlayer1)
		{
			Util::EquipObject
			(
				coopActor.get(), 
				glob.fists,
				nullptr, 
				1, 
				glob.bothHandsEquipSlot
			);
		}
		else
		{
			// Equip index arg determines if requesting to clear desired array slots or not.
			HandleCompanionPlayerEquip
			(
				glob.fists,
				a_clearDesiredSlots ? EquipIndex::kHands : EquipIndex::kNone,
				nullptr, 
				1,
				glob.bothHandsEquipSlot
			);
		}
	}

	void EquipManager::EquipForm
	(
		RE::TESForm* a_toEquip, 
		const EquipIndex& a_equipIndex, 
		RE::ExtraDataList* a_exData,
		uint32_t a_count, 
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip,
		bool a_forceEquip,
		bool a_playSounds,
		bool a_applyNow
	)
	{
		// Equip the given form.
		// 
		// IMPORTANT for companion players:
		// Precondition: 
		// Extra data list must be nullptr or a list linked with the bound object
		// in the player's inventory chest, not their true inventory.
		// Matching ex data list is generated if the item isn't already present
		// in the player's inventory and then used on equip.

		DBG
		(
			"{}: equip {}, list {:p}.", 
			coopActor->GetName(), a_toEquip ? a_toEquip->GetName() : "NONE",
			fmt::ptr(a_exData)
		);

		auto boundObj = a_toEquip ? a_toEquip->As<RE::TESBoundObject>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!boundObj || !aem)
		{
			return;
		}

		// Special case if trying to equip dummy1H/fists here.
		// Desired form NOT cleared first.
		if (a_toEquip == glob.fists || a_toEquip == glob.dummy1H) 
		{
			if (a_toEquip == glob.fists )
			{
				EquipFists(false);
			}
			else
			{
				EquipDummy1H(a_slot, false);
			}

			return;
		}

		// Must unequip the opposite hand's object if this equip request is to equip
		// the very same object. Otherwise, the object will duplicate and equip to both hands.
		if (p->isPlayer1)
		{
			bool alreadyEquippedInOtherHand = false;
			// Number owned is sometimes less than 1 even though the player 
			// is equipping the item from their inventory. Not good.
			auto numberOwned = 
			(
				Util::GetIntrinsicallyEqualCount
				(
					coopActor.get(), boundObj, a_exData
				)
			);
			RE::TESForm* oppositeHandForm = nullptr;
			RE::ExtraDataList* oppositeHandExData = nullptr;
			auto oppositeEquipIndex = EquipIndex::kNone;
			const auto equipSlot = 
			(
				a_toEquip->As<RE::BGSEquipType>() && 
				a_toEquip->As<RE::BGSEquipType>()->equipSlot ? 
				a_toEquip->As<RE::BGSEquipType>()->equipSlot :
				nullptr
			);
			if (equipSlot && a_equipIndex != EquipIndex::kNone)
			{
				oppositeEquipIndex = 
				(
					a_equipIndex == EquipIndex::kLeftHand ? 
					EquipIndex::kRightHand :
					EquipIndex::kLeftHand
				);
				oppositeHandForm = coopActor->GetEquippedObject
				(
					oppositeEquipIndex == EquipIndex::kLeftHand
				);
				oppositeHandExData = Util::GetEquippedExtraData
				(
					coopActor.get(), oppositeHandForm, oppositeEquipIndex == EquipIndex::kLeftHand
				);
				alreadyEquippedInOtherHand = 
				(
					(oppositeHandForm) &&
					(
						(
							oppositeHandForm == a_toEquip && 
							a_toEquip->As<RE::BGSEquipType>() && 
							a_toEquip->As<RE::BGSEquipType>()->equipSlot != glob.bothHandsEquipSlot
						) &&
						(
							oppositeHandExData == a_exData || 
							Util::AreIntrinsicallyEquivalentExDataLists
							(
								oppositeHandExData, a_exData
							)
						)
					)
				);
				DBG
				(
					"{}: {} has count {}. Other hand: {}, matches: {}, {} ({:p} <=> {:p}). "
					"Unequip first: {}. Equip slot: {}.",
					coopActor->GetName(), 
					a_toEquip->GetName(), 
					numberOwned, 
					oppositeHandForm ? oppositeHandForm->GetName() : "NONE", 
					oppositeHandForm == a_toEquip, 
					(
						oppositeHandExData == a_exData || 
						Util::AreIntrinsicallyEquivalentExDataLists(oppositeHandExData, a_exData)
					),
					fmt::ptr(oppositeHandExData),
					fmt::ptr(a_exData),
					alreadyEquippedInOtherHand,
					a_toEquip->As<RE::BGSEquipType>() && 
					a_toEquip->As<RE::BGSEquipType>()->equipSlot ? 
					Util::GetEditorID(a_toEquip->As<RE::BGSEquipType>()->equipSlot) : 
					"NONE"
				);
			}

			// Before equipping in the other hand,
			// unequip and remove + add back if P1 only owns one of the form.
			if (alreadyEquippedInOtherHand && numberOwned <= 1)
			{
				EquipFists(false);
			}
			else
			{
				// Unequip current form(s) first.
				if (a_slot == glob.bothHandsEquipSlot)
				{
					UnequipHandForms(glob.bothHandsEquipSlot);
				}
				else
				{
					UnequipFormAtIndex(a_equipIndex);
				}
			}

			
			// Once again, do not force equip for P1.
			Util::EquipObject
			(
				coopActor.get(),
				boundObj, 
				a_exData, 
				a_count, 
				a_slot, 
				a_queueEquip, 
				false, 
				a_playSounds,
				a_applyNow
			);

			// Causing crashes if equipped back to the other hand
			// right after unequipping the original and equipping another copy 
			// to the requested hand above.
			//if (alreadyEquippedInOtherHand && numberOwned > 1)
			//{
			//	// Re-equip other hand item if the player has more than one of the item.
			//	const auto oppositeSlot = 
			//	(
			//		a_slot == glob.rightHandEquipSlot ? 
			//		glob.leftHandEquipSlot :
			//		glob.rightHandEquipSlot
			//	);
			//	DBG
			//	(
			//		"Re-equip {} in the other hand with the {} equip slot.",
			//		oppositeHandForm->GetName(), 
			//		Util::GetEditorID(oppositeSlot)
			//	);
			//	Util::EquipObject
			//	(
			//		coopActor.get(), 
			//		oppositeHandForm->As<RE::TESBoundObject>(), 
			//		oppositeHandExData, 
			//		1, 
			//		oppositeSlot
			//	);
			//}
		}
		else
		{
			HandleCompanionPlayerEquip
			(
				boundObj,
				a_equipIndex,
				a_exData,
				a_count, 
				a_slot, 
				a_queueEquip, 
				a_forceEquip, 
				a_playSounds, 
				a_applyNow
			);
		}

		// Auto equip matching ammo, if necessary.
		AutoEquipAmmo(boundObj);
	}

	void EquipManager::EquipShout(RE::TESForm* a_toEquip)
	{
		// Equip the given shout.

		DBG
		(
			"{}: equip {}.", coopActor->GetName(), a_toEquip ? a_toEquip->GetName() : "NONE"
		);

		auto shout = a_toEquip ? a_toEquip->As<RE::TESShout>() : nullptr;
		auto aem = RE::ActorEquipManager::GetSingleton(); 
		if (!shout || !aem)
		{
			return;
		}

		if (!p->isPlayer1)
		{
			// NOTE:
			// Adding the shout to a companion player with AddShout() here 
			// can cause a save corruption crash on load.
			
			// Unequip current spell/shout first.
			if (auto currentVoiceForm = equippedForms[!EquipIndex::kVoice]; currentVoiceForm)
			{
				if (currentVoiceForm->As<RE::SpellItem>())
				{
					// Power.
					UnequipSpell(currentVoiceForm, EquipIndex::kVoice);
				}
				else
				{
					// Shout.
					UnequipShout(currentVoiceForm);
				}
			}

			// Add to desired equipped forms list.
			desiredForms[!EquipIndex::kVoice] = shout;
		}

		aem->EquipShout(coopActor.get(), shout);
	}

	void EquipManager::EquipSpell
	(
		RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, const RE::BGSEquipSlot* a_slot
	)
	{
		// Equip the given spell.

		DBG
		(
			"{}: equip {}.", coopActor->GetName(), a_toEquip ? a_toEquip->GetName() : "NONE"
		);

		auto spell = a_toEquip ? a_toEquip->As<RE::SpellItem>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton(); 
		if (!spell || !aem) 
		{
			return;
		}

		if (p->isPlayer1)
		{
			// Simply equip for P1. Easy as it gets.
			aem->EquipSpell(coopActor.get(), spell, a_slot);
		}
		else
		{
			// Ensure P1 and co-op player spell lists remain in sync.
			// Do not add placeholder spells to P1's known list.
			if (!coopActor->HasSpell(spell) && !glob.placeholderSpellsSet.contains(spell))
			{
				coopActor->AddSpell(spell);
			}

			bool is2HSpell = a_slot == glob.bothHandsEquipSlot;
			// Add to desired equipped forms list.
			if (is2HSpell)
			{
				// Unequip LH and RH forms first before equipping 2H spell.
				UnequipFormAtIndex(EquipIndex::kLeftHand);
				UnequipFormAtIndex(EquipIndex::kRightHand);
				// Copy to placeholder spell as needed.
				if (spell != placeholderMagic[!PlaceholderMagicIndex::k2H])
				{
					spell = CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::k2H);
				}
			}
			else
			{
				// Unequip LH/RH form first.
				if (a_equipIndex != EquipIndex::kVoice)
				{
					// If the current spell in the LH is a 2H spell, 
					// we have to also equip and unequip the requested spell 
					// in the RH to properly equip the spell into only the LH afterward. 
					// Idk why, but hey.
					if (a_equipIndex == EquipIndex::kLeftHand)
					{
						RE::TESForm* currentForm = equippedForms[!a_equipIndex];
						if (currentForm && currentForm->Is(RE::FormType::Spell) && 
							currentForm->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot)
						{
							EquipSpell(a_toEquip, EquipIndex::kRightHand, glob.rightHandEquipSlot);
							UnequipSpell(a_toEquip, EquipIndex::kRightHand);
						}
					}

					// Clear saved equipped form and unequip in this hand.
					UnequipFormAtIndex(a_equipIndex);
					// Fix placeholder spell being equipped in the wrong hand.
					bool lhPlaceholderSpellInWrongHand = 
					(
						equippedForms[!EquipIndex::kRightHand] == 
						placeholderMagic[!PlaceholderMagicIndex::kLH]
					);
					bool rhPlaceholderSpellInWrongHand = 
					(
						equippedForms[!EquipIndex::kLeftHand] == 
						placeholderMagic[!PlaceholderMagicIndex::kRH]
					);
					if (lhPlaceholderSpellInWrongHand || rhPlaceholderSpellInWrongHand)
					{
						auto toUnequip = 
						(
							lhPlaceholderSpellInWrongHand ? 
							placeholderMagic[!PlaceholderMagicIndex::kLH] : 
							placeholderMagic[!PlaceholderMagicIndex::kRH]
						);
						auto index = 
						(
							lhPlaceholderSpellInWrongHand ? 
							!EquipIndex::kRightHand : 
							!EquipIndex::kLeftHand
						);
						desiredForms[index] = nullptr;
					}

					// Handle placeholder spell copying before equipping.
					// Don't copy the placeholder spell to itself.
					bool shouldCopyToPlaceholder = 
					(
						(
							a_equipIndex == EquipIndex::kLeftHand && 
							spell != placeholderMagic[!PlaceholderMagicIndex::kLH]
						) ||
						(
							a_equipIndex == EquipIndex::kRightHand &&
							spell != placeholderMagic[!PlaceholderMagicIndex::kRH]
						)
					);
					if (shouldCopyToPlaceholder)
					{
						spell = CopyToPlaceholderSpell
						(
							spell, 
							a_equipIndex == EquipIndex::kRightHand ? 
							PlaceholderMagicIndex::kRH : 
							PlaceholderMagicIndex::kLH
						);
					}
				}
				else
				{
					// No need to copy voice spell to placeholder, 
					// since this spell is directly cast with no animation
					// using the player's instant magic caster.
							
					// Unequip voice slot spell/shout first.
					if (auto currentForm = equippedForms[!a_equipIndex]; currentForm)
					{
						if (currentForm->Is(RE::FormType::Spell))
						{
							UnequipSpell(currentForm, a_equipIndex);
						}
						else if (currentForm->Is(RE::FormType::Shout))
						{
							UnequipShout(currentForm);
						}
					}
				}
			}

			if (a_slot != glob.bothHandsEquipSlot)
			{
				desiredForms[!a_equipIndex] = spell;
			}
			else
			{
				desiredForms[!EquipIndex::kLeftHand] = spell;
				desiredForms[!EquipIndex::kRightHand] = spell;
			}

			aem->EquipSpell(coopActor.get(), spell, a_slot);
		}
	}

	std::vector<RE::TESForm*> EquipManager::GetEquipableSpells(bool a_inHandSlot) const
	{
		// NOTE:
		// Unused for now, but keeping for reference or if needed again in the future.
		// Get all equipable hand-slot or voice-slot spells known by P1 and this player.

		DBG
		(
			"{}: in hand slot: {}.", coopActor->GetName(), a_inHandSlot
		);

		std::vector<RE::TESForm*> equipableSpells{};
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return equipableSpells;
		}

		auto spellType = RE::MagicSystem::SpellType::kSpell;
		bool handSlotSpell = false;
		bool voiceSlotSpell = false;
		if (auto p1ActorBase = p1->GetActorBase(); p1ActorBase)
		{
			auto p1BaseSpellList = 
			(
				p1ActorBase->actorEffects ? 
				p1ActorBase->actorEffects->spells : 
				nullptr
			); 
			if (p1BaseSpellList)
			{
				uint32_t spellListSize = p1ActorBase->actorEffects->numSpells;
				// Get spells from P1's actorbase spell list.
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					auto spell = p1BaseSpellList[i]; 
					if (!spell)
					{
						continue;
					}

					spellType = spell->GetSpellType();
					handSlotSpell = 
					(
						a_inHandSlot && spellType == RE::MagicSystem::SpellType::kSpell
					);
					voiceSlotSpell = 
					(
						(!a_inHandSlot) && 
						(
							spellType == RE::MagicSystem::SpellType::kLesserPower ||
							spellType == RE::MagicSystem::SpellType::kPower ||
							spellType == RE::MagicSystem::SpellType::kVoicePower
						)
					);

					if (handSlotSpell || voiceSlotSpell)
					{
						equipableSpells.push_back(spell);
					}
				}
			}

			// Add shouts that the co-op player has by virtue of their actor base, and
			// add shouts that P1 has by virtue of their actor base.
			if (!a_inHandSlot)
			{
				auto shoutList = 
				(
					p1ActorBase->actorEffects ? 
					p1ActorBase->actorEffects->shouts : 
					nullptr
				); 
				if (shoutList)
				{
					uint32_t shoutListSize = p1ActorBase->actorEffects->numShouts;
					for (uint32_t i = 0; i < shoutListSize; ++i)
					{
						if (!shoutList[i] || strlen(shoutList[i]->GetName()) <= 1)
						{
							continue;
						}

						equipableSpells.emplace_back(shoutList[i]);
					}
				}

				if (auto coopPlayerActorBase = coopActor->GetActorBase(); coopPlayerActorBase) 
				{
					shoutList = 
					(
						coopPlayerActorBase->actorEffects ? 
						coopPlayerActorBase->actorEffects->shouts : 
						nullptr
					);
					if (shoutList)
					{
						uint32_t shoutListSize = coopPlayerActorBase->actorEffects->numShouts;
						for (uint32_t i = 0; i < shoutListSize; ++i)
						{
							if (!shoutList[i] || strlen(shoutList[i]->GetName()) <= 1)
							{
								continue;
							}
							
							equipableSpells.emplace_back(shoutList[i]);
						}
					}
				}
			}
		}

		// Add all hand or voice spells that P1 has learned.
		for (auto spell : p1->addedSpells)
		{
			if (!spell)
			{
				continue;
			}

			spellType = spell->GetSpellType();
			handSlotSpell = a_inHandSlot && spellType == RE::MagicSystem::SpellType::kSpell;
			voiceSlotSpell = 
			{ 
				(!a_inHandSlot) && 
				(
					spellType == RE::MagicSystem::SpellType::kLesserPower ||
					spellType == RE::MagicSystem::SpellType::kPower ||
					spellType == RE::MagicSystem::SpellType::kVoicePower
				) 
			};

			if (handSlotSpell || voiceSlotSpell)
			{
				equipableSpells.push_back(spell);
			}
		}

		return equipableSpells;
	}

	EquipIndex EquipManager::GetEquipIndexForForm
	(
		RE::TESForm* a_form, const EquipIndex& a_preferredIndex
	) const
	{
		// Resolve equip slot for the given form with the requested equip index.

		DBG
		(
			"{}: form: {}, preferred index: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_preferredIndex
		);

		if (!a_form) 
		{
			DBG("{}: No form -> given index {}.", coopActor->GetName(), a_preferredIndex);
			return a_preferredIndex;
		}
		
		auto asEquipType = a_form->As<RE::BGSEquipType>();
		if (!asEquipType)
		{
			if (a_form->As<RE::TESAmmo>())
			{
				DBG("{}: {} -> Ammo.", coopActor->GetName(), a_form->GetName());
				return EquipIndex::kAmmo;
			}
			
			DBG("{}: {} -> No equip type, given index {}.",
				coopActor->GetName(), a_form->GetName(), a_preferredIndex);
			return a_preferredIndex;
		}
		else if ((a_form->Is(RE::FormType::Spell, RE::FormType::Shout)) &&
				 (
					 a_preferredIndex != EquipIndex::kLeftHand &&
					 a_preferredIndex != EquipIndex::kRightHand &&
					 a_preferredIndex != EquipIndex::kVoice
				 ))
		{
			DBG("{}: {} -> Quick slot spell.", coopActor->GetName(), a_form->GetName());
			return EquipIndex::kQuickSlotSpell;
		}
		else if (a_form->Is(RE::FormType::Shout))
		{
			DBG("{}: {} -> Shout.", coopActor->GetName(), a_form->GetName());
			return EquipIndex::kVoice;
		}
		else if (Util::IsConsumable(a_form))
		{
			DBG("{}: {} -> Quick slot item.", coopActor->GetName(), a_form->GetName());
			return EquipIndex::kQuickSlotItem;
		}
		
		auto equipSlot = glob.eitherHandEquipSlot;
		if (asEquipType->equipSlot == glob.bothHandsEquipSlot)
		{
			DBG
			(
				"{}: {} -> Two handed form, right hand.",
				coopActor->GetName(), a_form->GetName()
			);
			return EquipIndex::kRightHand;
		}
		else if (asEquipType->equipSlot == glob.shieldEquipSlot)
		{
			DBG("{}: {} -> Shield form, left hand.", coopActor->GetName(), a_form->GetName());
			return EquipIndex::kLeftHand;
		}
		else if (asEquipType->equipSlot == glob.voiceEquipSlot)
		{
			DBG("{}: {} -> Voice form, voice slot.", coopActor->GetName(), a_form->GetName());
			return EquipIndex::kVoice;
		}
		
		DBG("{}: {} -> Other form, given index: {}.", 
			coopActor->GetName(), a_form->GetName(), a_preferredIndex);
		return a_preferredIndex;
	}

	RE::BGSEquipSlot* EquipManager::GetEquipSlotForForm
	(
		RE::TESForm* a_form, const EquipIndex& a_index
	) const
	{
		// Resolve equip slot for the given form with the requested equip index.

		DBG
		(
			"{}: form: {}, index: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_index
		);

		if (!a_form || !a_form->As<RE::BGSEquipType>()) 
		{
			return nullptr;
		}
		
		auto equipSlot = glob.eitherHandEquipSlot;
		auto asEquipType = a_form->As<RE::BGSEquipType>();
		if ((asEquipType->equipSlot == glob.bothHandsEquipSlot) && 
			(a_index == EquipIndex::kLeftHand || a_index == EquipIndex::kRightHand))
		{
			equipSlot = glob.bothHandsEquipSlot;
		}
		else if ((asEquipType->equipSlot == glob.shieldEquipSlot) && 
				(a_index == EquipIndex::kLeftHand || a_index == EquipIndex::kRightHand))
		{
			equipSlot = glob.shieldEquipSlot;
		}
		else if (a_index == EquipIndex::kLeftHand)
		{
			equipSlot = glob.leftHandEquipSlot;
		}
		else if (a_index == EquipIndex::kRightHand)
		{
			equipSlot = glob.rightHandEquipSlot;
		}
		else
		{
			equipSlot = asEquipType->equipSlot;
		}

		return equipSlot;
	}

	std::string_view EquipManager::FavMagCyclingCategoryToString
	(
		const FavMagicCyclingCategory& a_category
	) const
	{
		// Favorited magic cycling category mapped to its name.

		switch (a_category)
		{
		case FavMagicCyclingCategory::kAllFavorites:
		{
			return "All Favorites"sv;
		}
		case FavMagicCyclingCategory::kAlteration:
		{
			if (ALYSLC::EnderalCompat::g_installed)
			{
				return "Mentalism"sv;
			}
			else
			{
				return "Alteration"sv;
			}
		}
		case FavMagicCyclingCategory::kConjuration:
		{
			if (ALYSLC::EnderalCompat::g_installed)
			{
				return "Entropy"sv;
			}
			else
			{
				return "Conjuration"sv;
			}
		}
		case FavMagicCyclingCategory::kDestruction:
		{
			if (ALYSLC::EnderalCompat::g_installed)
			{
				return "Elementalism"sv;
			}
			else
			{
				return "Destruction"sv;
			}
		}
		case FavMagicCyclingCategory::kIllusion:
		{
			if (ALYSLC::EnderalCompat::g_installed)
			{
				return "Psionics"sv;
			}
			else
			{
				return "Illusion"sv;
			}
		}
		case FavMagicCyclingCategory::kRestoration:
		{
			if (ALYSLC::EnderalCompat::g_installed)
			{
				return "Light Magic"sv;
			}
			else
			{
				return "Restoration"sv;
			}
		}
		case FavMagicCyclingCategory::kRitual:
		{
			return "Ritual"sv;
		}
		default:
		{
			return "INVALID"sv;
		}
		}
	}

	std::string_view EquipManager::FavWeaponCyclingCategoryToString
	(
		const FavWeaponCyclingCategory& a_category
	) const
	{
		// Favorited weapon cycling category mapped to its name.

		switch (a_category)
		{
		case FavWeaponCyclingCategory::kAllFavorites:
		{
			return "All Favorites"sv;
		}
		case FavWeaponCyclingCategory::kAxe:
		{
			return "Axe"sv;
		}
		case FavWeaponCyclingCategory::kBattleaxe:
		{
			return "Battleaxe"sv;
		}
		case FavWeaponCyclingCategory::kBow:
		{
			return "Bow"sv;
		}
		case FavWeaponCyclingCategory::kCrossbow:
		{
			return "Crossbow"sv;
		}
		case FavWeaponCyclingCategory::kDagger:
		{
			return "Dagger"sv;
		}
		case FavWeaponCyclingCategory::kGreatsword:
		{
			return "Greatsword"sv;
		}
		case FavWeaponCyclingCategory::kMace:
		{
			return "Mace"sv;
		}
		case FavWeaponCyclingCategory::kShieldAndTorch:
		{
			return "Shield and Torch"sv;
		}
		case FavWeaponCyclingCategory::kStaff:
		{
			return "Staff"sv;
		}
		case FavWeaponCyclingCategory::kSword:
		{
			return "Sword"sv;
		}
		case FavWeaponCyclingCategory::kUnique:
		{
			return "Unique"sv;
		}
		case FavWeaponCyclingCategory::kWarhammer:
		{
			return "Warhammer"sv;
		}
		default:
		{
			return "INVALID"sv;
		}
		}
	}

	void EquipManager::FixInventory()
	{
		// Add extra ownership data to all equipable items in the player's inventory
		// and then fix counts for all the inventory's items.
		// Inventory here means P1's on-player inventory 
		// and the player inventory chest for companion players.

		DBG
		(
			"{}. Inv changes: {:p}.", 
			coopActor->GetName(), 
			fmt::ptr
			(
				p->isPlayer1 ? 
				coopActor->GetInventoryChanges() : 
				inventoryChest->GetInventoryChanges()
			)
		);

		for (const auto& otherP : glob.coopPlayers)
		{
			if (otherP->isActive && otherP != p)
			{
				DBG
				(
					"{}'s inventory changes is {:p}, chest is {:p}.",
					otherP->coopActor->GetName(), 
					fmt::ptr(otherP->coopActor->GetInventoryChanges()), 
					fmt::ptr(otherP->em->inventoryChest->GetInventoryChanges())
				);
			}
		}

		if (p->isPlayer1)
		{
			return;
		}

		auto invChanges = inventoryChest->GetInventoryChanges();
		if (!invChanges || !invChanges->entryList)
		{
			return;
		}

		// For companion players:
		// Add serializable exData to all equipable items to ensure all their inventory entries 
		// have at least one extra data list.
		// Required to make sure we can check the equip status of items
		// and match chest extra data lists to player inventory extra data lists.
		bool addSerializableExData = false;
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		for (auto& entry : *invChanges->entryList) 
		{
			int32_t exListsCount = 0;
			if (!entry || !entry->object)
			{
				continue;
			}
				
			addSerializableExData = Util::IsEquipableInventoryObject(entry->object);
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

					if (addSerializableExData)
					{
						if (!exDataList->HasType<RE::ExtraShouldWear>())
						{
							auto data = static_cast<RE::ExtraShouldWear*>
							(
								exDataList->Add(RE::BSExtraData::Create<RE::ExtraShouldWear>())
							);
							if (data)
							{
								DBG
								(
									"MALLOC: Added ownership exData to {} ({:p}).",
									entry->object->GetName(), 
									fmt::ptr(exDataList)
								);
							}
							else
							{
								DBG
								(
									"ERR: MALLOC: Failed to add ownership exData to {} ({:p}):",
									entry->object->GetName(), fmt::ptr(exDataList)
								);
							}
						}
					}
				}
			}
			else if (addSerializableExData)
			{
				entry->AddExtraList
				(
					Util::CreateExtraDataListWithSerializableData()
				);
				if (entry->extraLists && !entry->extraLists->empty())
				{
					const auto addedList = entry->extraLists->front();
					DBG("Added serializable exData list to {}: {:p}.",
						entry->object->GetName(), fmt::ptr(addedList));
					if (entry->countDelta > 0)
					{
						DBG
						(
							"Set new exData list for {} {:p}'s count to {}.",
							entry->object->GetName(), 
							fmt::ptr(addedList),
							entry->countDelta
						);
						addedList->SetCount(entry->countDelta);
					}

					exListsCount += addedList->GetCount();
				}
				else
				{
					DBG("ERR: Failed to add serializable exData list to {}:",
						entry->object->GetName());
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
					coopActor->GetName(),
					entry->object->GetName(), 
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

	RE::ExtraDataList* EquipManager::GetNextFavoritedExDataList
	(
		RE::TESForm* a_form, bool a_checkWornLeft, bool& a_shouldUnequip, bool& a_inOtherHand
	)
	{
		// Used to cycle-equip favorited items that have multiple favorited extra data lists.
		// LISTS LISTS LISTS, I LOVE LISTS.
		// 
		// Check if the given form has a favorited extra data list in the given hand,
		// and if one exists, look for and return the next favorited extra data list 
		// further along in the list of extra data lists that can be equipped.
		// 
		// If the currently equipped favorited extra data list in the given hand
		// is the last equipable one in the lists of extra data lists,
		// return it and set the unequip outparam to true
		// because we need the extra data list to unequip the item before equipping something else.
		// 
		// Otherwise, return the next favorited extra data list that can be equipped
		// in the given hand, and return nullptr if the given form is not favorited,
		// or if there are no additional favorited extra data lists 
		// in the item's list of extra data lists.
		
		// Set to true if there is a favorited list equipped in the other hand.
		a_inOtherHand = false;
		// No form given, so unequip is desired.
		if (!a_form)
		{
			a_shouldUnequip = true;
			return nullptr;
		}

		// Keep whatever is equipped but do not equip a magic form, which never has extraData.
		if (a_form->Is(RE::FormType::Spell, RE::FormType::Shout))
		{
			a_shouldUnequip = false;
			return nullptr;
		}

		auto equipType = a_form->As<RE::BGSEquipType>();
		// Check only if the item is a one handed weapon.
		// Two handers and shields receive ExtraWorn data not ExtraWornLeft data when equipped.
		// Modify the original bool here so the caller doesn't have to deal with adjusting it
		// to account for the requested hand form's equip slot.
		a_checkWornLeft = 
		(
			(a_checkWornLeft) && 
			(
				equipType && 
				equipType->equipSlot != glob.bothHandsEquipSlot && 
				equipType->equipSlot != glob.shieldEquipSlot
			)
		);
		auto inventory = p->isPlayer1 ? coopActor->GetInventory() : inventoryChest->GetInventory();
		for (const auto& [boundObj, countInvEntryPair] : inventory)
		{
			const auto& [count, invEntry] = countInvEntryPair;
			if (!boundObj || boundObj != a_form || count <= 0 || !invEntry || !invEntry->extraLists)
			{
				continue;
			}

			RE::ExtraDataList* equippedFavList = nullptr;
			RE::ExtraDataList* firstUnequippedFavList = nullptr;
			auto iter = invEntry->extraLists->begin(); 
			for (; iter != invEntry->extraLists->end(); ++iter)
			{
				auto extraDataList = *iter;
				if (!extraDataList)
				{
					continue;
				}
				
				DBG
				(
					"{}: {}: Checking for {}: HAS: {:p}. Count: {}", 
					coopActor->GetName(), 
					boundObj->GetName(), 
					a_checkWornLeft ? "WORN LEFT" : "WORN", 
					fmt::ptr(extraDataList),
					extraDataList->GetCount()
				);
				// Not favorited.
				if (!extraDataList->GetByType<RE::ExtraHotkey>())
				{
					continue;
				}

				// If we find a list that's already equipped in the requested hand,
				// we save it and continue.
				// The next favorited list without worn data will be returned.
				bool wornLH = false;
				bool wornRH = false;
				if (p->isPlayer1)
				{
					DBG
					(
						"{}: {}: {:p} has worn L/R: {}, {}", 
						coopActor->GetName(), 
						boundObj->GetName(),
						fmt::ptr(extraDataList),
						extraDataList->HasType<RE::ExtraWornLeft>(),
						extraDataList->HasType<RE::ExtraWorn>()
					);
					wornLH = extraDataList->GetByType<RE::ExtraWornLeft>();
					wornRH = extraDataList->GetByType<RE::ExtraWorn>();
					if ((a_checkWornLeft && wornLH) || (!a_checkWornLeft && wornRH))
					{
						DBG("{}: {}: EQUIPPED: {:p}.", 
							coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
						equippedFavList = extraDataList;
						continue;
					}
					else if ((a_checkWornLeft && wornRH) || (!a_checkWornLeft && wornLH))
					{
						DBG("{}: {}: IN THE OTHER HAND: {:p}.", 
							coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
						a_inOtherHand = true;
						continue;
					}
				}
				else if (const auto exRank = extraDataList->GetByType<RE::ExtraRank>(); exRank)
				{
					DBG
					(
						"{}: {}: {:p} has worn L/R: {}, {} (rank: 0x{:X})", 
						coopActor->GetName(), 
						boundObj->GetName(),
						fmt::ptr(extraDataList),
						((exRank->rank & 0xFF000000) == 0xFF000000),
						((exRank->rank & 0x00FF0000) == 0x00FF0000),
						static_cast<uint32_t>(exRank->rank)
					);
					wornLH = ((exRank->rank & 0xFF000000) == 0xFF000000);
					wornRH = ((exRank->rank & 0x00FF0000) == 0x00FF0000);
					if ((a_checkWornLeft && wornLH) || (!a_checkWornLeft && wornRH))
					{
						DBG("{}: {}: EQUIPPED: {:p}.", 
							coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
						equippedFavList = extraDataList;
						continue;
					}
					else if ((a_checkWornLeft && wornRH) || (!a_checkWornLeft && wornLH))
					{
						DBG("{}: {}: IN THE OTHER HAND: {:p}.", 
							coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
						a_inOtherHand = true;
						continue;
					}
				}

				// Return this unequipped list since it is the first one after the equipped list.
				if (equippedFavList)
				{
					DBG("{}: {}: NEXT TO EQUIP: {:p}.", 
						coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
					a_shouldUnequip = false;
					return extraDataList;
				}

				// Set the first unequipped list once.
				// Can set if not equipped in either hand or if equipped in the opposite hand
				// but possessing more than onw.
				if ((!firstUnequippedFavList) && 
					((!wornLH && !wornRH) || (extraDataList->GetCount() > 1)))
				{
					DBG("{}: {}: FIRST UNEQUIPPED: {:p}.", 
						coopActor->GetName(), boundObj->GetName(), fmt::ptr(extraDataList));
					firstUnequippedFavList = extraDataList;
				}
			}

			// The currently equipped list is the last one in the list of lists.
			// Return the list and signal that the item is equipped through the outparam.
			if (!invEntry->extraLists->empty() &&
				equippedFavList && 
				iter == invEntry->extraLists->end())
			{
				DBG("{}: {}: LAST EQUIPPED: {:p}. EQUIP NOTHING NEXT.", 
					coopActor->GetName(), boundObj->GetName(), fmt::ptr(equippedFavList));
				a_shouldUnequip = true;
				return equippedFavList;
			}

			// Return the first unequipped list in the list of extra data.
			if (firstUnequippedFavList)
			{
				DBG("{}: {}: FIRST UNEQUIPPED TO EQUIP: {:p}.", 
					coopActor->GetName(), boundObj->GetName(), fmt::ptr(firstUnequippedFavList));
				a_shouldUnequip = false;
				return firstUnequippedFavList;
			}
			else if (a_inOtherHand)
			{
				DBG("{}: {}: SHOULD UNEQUIP, only available list is in the other hand.", 
					coopActor->GetName(), boundObj->GetName(), fmt::ptr(firstUnequippedFavList));
				a_shouldUnequip = true;
				return nullptr;
			}

			break;
		}
		
		DBG("{}: {}, NOTHING to equip.", coopActor->GetName(), a_form->GetName());
		a_shouldUnequip = false;
		return nullptr;
	}

	void EquipManager::HandleCompanionPlayerEquip
	(
		RE::TESBoundObject* a_object,
		const EquipIndex& a_equipIndex,
		RE::ExtraDataList* a_exDataList,
		uint32_t a_count, 
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip, 
		bool a_forceEquip,
		bool a_playSounds, 
		bool a_applyNow
	)
	{
		// Setup equip request by adding the item from the companion player's chest
		// and providing the proper extra data list.
		// Can specify a specific equip index to set in the desired forms/exData arrays.
		// 'kNone' to have the function compute it based on the item type or to not fill any slot.

		if (p->isPlayer1 || !a_object)
		{
			return;
		}
		
		DBG
		(
			"{}: {}, index: {}, exData {:p}, count: {}, slot: {}.",
			coopActor->GetName(),
			a_object->GetName(),
			a_equipIndex,
			fmt::ptr(a_exDataList),
			a_count,
			Util::GetEditorID(a_slot)
		);

		// Special case for fists.
		// If equip index 'kHands' is specified, 
		// remove worn rank exData and clear desired array slots.
		if (a_object == glob.fists)
		{
			if (a_equipIndex == EquipIndex::kHands)
			{
				auto lhObj = coopActor->GetEquippedObject(true);
				if (lhObj)
				{
					ChangeWornRankExData(lhObj->As<RE::TESBoundObject>(), true, false);
				}

				auto rhObj = coopActor->GetEquippedObject(false);
				if (rhObj)
				{
					ChangeWornRankExData(rhObj->As<RE::TESBoundObject>(), false, false);
				}
				
				auto lhObj2 = equippedForms[!EquipIndex::kLeftHand];
				if (lhObj2 && lhObj2 != lhObj)
				{
					ChangeWornRankExData(lhObj2->As<RE::TESBoundObject>(), true, false);
				}
				
				auto rhObj2 = equippedForms[!EquipIndex::kRightHand];
				if (rhObj2 && rhObj2 != rhObj)
				{
					ChangeWornRankExData(rhObj2->As<RE::TESBoundObject>(), false, false);
				}
				
				// Clear out indices in desired form/extra data lists.
				ClearDesiredEquippedForm
				(
					desiredForms[!EquipIndex::kLeftHand],
					GetEquipSlotForForm
					(
						desiredForms[!EquipIndex::kLeftHand],
						EquipIndex::kLeftHand
					),
					EquipIndex::kLeftHand				
				);
				ClearDesiredEquippedForm
				(
					desiredForms[!EquipIndex::kRightHand],
					GetEquipSlotForForm
					(
						desiredForms[!EquipIndex::kRightHand],
						EquipIndex::kRightHand
					),
					EquipIndex::kRightHand				
				);
			}
			
			Util::EquipObject
			(
				coopActor.get(), 
				glob.fists,
				nullptr, 
				1, 
				glob.bothHandsEquipSlot
			);

			return;
		}

		// Special case for dummy 1H.
		// If equip index 'kRightHand' or 'kLeftHand' is specified, 
		// remove worn rank exData and clear desired array slots.
		if (a_object == glob.dummy1H)
		{
			if (a_equipIndex == EquipIndex::kLeftHand || 
				a_equipIndex == EquipIndex::kRightHand)
			{
				auto obj = coopActor->GetEquippedObject(a_equipIndex == EquipIndex::kLeftHand);
				if (obj)
				{
					ChangeWornRankExData
					(
						obj->As<RE::TESBoundObject>(), a_equipIndex == EquipIndex::kLeftHand, false 
					);
				}

				auto obj2 = equippedForms[!a_equipIndex];
				if (obj2 && obj2 != obj)
				{
					ChangeWornRankExData
					(
						obj2->As<RE::TESBoundObject>(), a_equipIndex == EquipIndex::kLeftHand, false
					);
				}
				
				// Clear out indices in desired form/extra data lists.
				if (a_equipIndex == EquipIndex::kLeftHand)
				{
					ClearDesiredEquippedForm
					(
						desiredForms[!EquipIndex::kLeftHand],
						GetEquipSlotForForm
						(
							desiredForms[!EquipIndex::kLeftHand],
							EquipIndex::kLeftHand
						),
						EquipIndex::kLeftHand				
					);
				}
				else
				{
					ClearDesiredEquippedForm
					(
						desiredForms[!EquipIndex::kRightHand],
						GetEquipSlotForForm
						(
							desiredForms[!EquipIndex::kRightHand],
							EquipIndex::kRightHand
						),
						EquipIndex::kRightHand				
					);
				}
			}

			Util::EquipObject
			(
				coopActor.get(), 
				glob.dummy1H,
				nullptr, 
				1, 
				a_slot
			);
			return;
		}
		
		// Grab equivalent chest extra data to equip.
		RE::ExtraDataList* chestExDataList = a_exDataList;
		bool isEquipable = Util::IsEquipableInventoryObject(a_object);
		if (isEquipable)
		{
			chestExDataList = Util::FindMatchingExtraDataList
			(
				inventoryChest.get(), a_object, a_exDataList
			);
			if (!a_exDataList || !chestExDataList)
			{
				DBG("Given extra data list is nullptr: {}, chest list is nullptr: {}.",
					!a_exDataList, !chestExDataList);	
				if (!chestExDataList)
				{
					ERR
					(
						"ERR: {}: Could not get matching inventory chest list for {} (given {:p}), "
						"equipped to index {}, with equip slot {}, and count {}. "
						"Equip will likely fail.",
						coopActor->GetName(),
						a_object->GetName(),
						fmt::ptr(a_exDataList),
						a_equipIndex,
						a_slot ? Util::GetEditorID(a_slot) : "NONE",
						a_count
					);
				}
			}
		}

		bool shouldReEquipOtherHandForm = false;
		RE::TESForm* oppositeHandForm = nullptr;
		RE::ExtraDataList* oppositeHandExData = nullptr;
		if (auto asArmor = a_object->As<RE::TESObjectARMO>(); asArmor)
		{
			// Must add all armor indices that correspond to the requested item to equip,
			// since armor pieces can fit into multiple biped slots.
			std::vector<uint8_t> equipIndices{ };
			auto slotMask = asArmor->bipedModelData.bipedObjectSlots;
			bool isShield = asArmor->IsShield();
			const RE::BGSEquipSlot* slot = 
			(
				isShield ? 
				a_object->As<RE::TESObjectARMO>()->equipSlot :
				a_slot
			);
			DBG
			(
				"{}: {} slot mask 0b{:B}.",
				coopActor->GetName(),
				a_object ? a_object->GetName() : "NONE",
				*slotMask
			);
			const auto& biped = coopActor->GetBiped2();
			for (uint8_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
			{
				auto bitMask = 
				(
					static_cast<RE::BIPED_MODEL::BipedObjectSlot>
					(
						1 << (i - !EquipIndex::kFirstBipedSlot)
					)
				);
				if (slotMask.all(bitMask))
				{
					equipIndices.emplace_back(i);
					// Unequip armor in same slot first.
					// Check our cached equipped armor in the same slot first.
					auto currentArmorForm = equippedForms[i];
					if (currentArmorForm)
					{
						UnequipArmor(currentArmorForm);
					}
					else
					{
						// If nothing is cached, check the current biped object's item.
						// This does not always match the equipped item unfortunately
						// and is sometimes an armature item, which will not unequip.
						currentArmorForm = 
						(
							biped && biped->objects ? 
							biped->objects[i - !EquipIndex::kFirstBipedSlot].item :
							nullptr
						);
						if (currentArmorForm)
						{
							DBG
							(
								"Comp {} (0x{:X}, type {}) to {} (0x{:X}, type: {}).",
								Util::GetEditorID(currentArmorForm),
								currentArmorForm->formID,
								*currentArmorForm->formType,
								equippedForms[i] ? Util::GetEditorID(equippedForms[i]) : "NONE",
								equippedForms[i] ? equippedForms[i]->formID : 0xDEAD,
								equippedForms[i] ? 
								*equippedForms[i]->formType : 
								RE::FormType::None
							);
							UnequipArmor(currentArmorForm);
						}
					}
				}
			}

			// Add to desired equipped forms list at each biped slot index.
			for (auto index : equipIndices)
			{
				desiredForms[index] = a_object;
				desiredExtraDataLists[index] = a_exDataList;
			}

			// Special shield case: also update LH slot in desired equipped forms list.
			if (isShield)
			{
				UnequipFormAtIndex(EquipIndex::kLeftHand);
				desiredForms[!EquipIndex::kLeftHand] = a_object;
				desiredExtraDataLists[!EquipIndex::kLeftHand] = a_exDataList;
			}
		}
		else if (a_object->As<RE::TESAmmo>())
		{
			// Unequip current ammo before equipping new one.
			if (RE::TESForm* currentAmmoForm = equippedForms[!EquipIndex::kAmmo]; currentAmmoForm)
			{
				UnequipAmmo(currentAmmoForm);
			}

			// Add to desired equipped forms list.
			desiredForms[!EquipIndex::kAmmo] = a_object;
			desiredExtraDataLists[!EquipIndex::kAmmo] = a_exDataList;
		}
		else
		{
			if (a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kRightHand)
			{
				if (a_slot != glob.bothHandsEquipSlot)
				{
					// Unequip form in the opposite hand if equipping a two-handed weapon, 
					// or unequip the same weapon if equipped in the other hand 
					// and if the co-op actor only owns one.
					// Done to prevent the equip function from duplicating the weapon 
					// and equipping it in both hands.
					auto oppositeEquipIndex = 
					(
						a_equipIndex == EquipIndex::kLeftHand ?
						EquipIndex::kRightHand : 
						EquipIndex::kLeftHand
					);
					oppositeHandForm = coopActor->GetEquippedObject
					(
						oppositeEquipIndex == EquipIndex::kLeftHand
					);
					oppositeHandExData = Util::GetEquippedExtraData
					(
						coopActor.get(),
						oppositeHandForm, 
						oppositeEquipIndex == EquipIndex::kLeftHand
					);
					auto oppositeHandChestExData = Util::FindMatchingExtraDataList
					(
						inventoryChest.get(),
						oppositeHandForm ? oppositeHandForm->As<RE::TESBoundObject>() : nullptr,
						oppositeHandExData
					);
					// Number owned is sometimes less than 1 even though the player 
					// is equipping the item from their inventory. Not good.
					auto numberOwned = 
					(
						Util::GetIntrinsicallyEqualCount
						(
							inventoryChest.get(), a_object, chestExDataList
						)
					);
					bool alreadyEquippedInOtherHand = 
					(
						(oppositeHandForm) &&
						(
							oppositeHandForm == a_object && 
							a_object->As<RE::BGSEquipType>() && 
							a_object->As<RE::BGSEquipType>()->equipSlot != 
							glob.bothHandsEquipSlot
						) &&
						(
							(
								oppositeHandChestExData &&
								oppositeHandChestExData == chestExDataList
							) || 
							(
								Util::AreIntrinsicallyEquivalentExDataLists
								(
									oppositeHandExData, chestExDataList
								)
							)
						)
					);
					DBG
					(
						"{}: {} has count {}. Other hand: {}, matches: {}, {} ({:p} <=> {:p}). "
						"Unequip first: {}.",
						coopActor->GetName(), 
						a_object->GetName(), 
						numberOwned, 
						oppositeHandForm ? oppositeHandForm->GetName() : "NONE", 
						oppositeHandForm == a_object, 
						(
							oppositeHandChestExData == chestExDataList || 
							Util::AreIntrinsicallyEquivalentExDataLists
							(
								oppositeHandExData, chestExDataList
							)
						),
						fmt::ptr(oppositeHandChestExData),
						fmt::ptr(chestExDataList),
						alreadyEquippedInOtherHand
					);
					if (alreadyEquippedInOtherHand && numberOwned <= 1)
					{
						// If owning 1, clear out both slots and do not remove desired items
						// before re-equipping in this hand.
						EquipFists(false);
					}
					else
					{
						// Unequip current form(s) first.
						UnequipFormAtIndex(a_equipIndex);
					}

					// Set desired equipped form at the given index.
					desiredForms[!a_equipIndex] = a_object;
					desiredExtraDataLists[!a_equipIndex] = chestExDataList;
					// Equip enchantment as well if equipping a staff.
					auto weap = a_object->As<RE::TESObjectWEAP>(); 
					if (weap && weap->IsStaff() && weap->formEnchanting) 
					{
						Util::EquipObject(coopActor.get(), weap->formEnchanting);
					}
				}
				else
				{
					// Clear both hands first.
					UnequipHandForms(glob.bothHandsEquipSlot);
					// Set both LH and RH indices if this form is 2H.
					desiredForms[!EquipIndex::kLeftHand] =
					desiredForms[!EquipIndex::kRightHand] = a_object;
					desiredExtraDataLists[!EquipIndex::kLeftHand] = 
					desiredExtraDataLists[!EquipIndex::kRightHand] = chestExDataList;
				}
			}
			else if (a_equipIndex != EquipIndex::kNone)
			{
				// Quick slot, consumables, etc.
				// Just update the index, no need to unequip, or clear out.
				desiredForms[!a_equipIndex] = a_object;
				desiredExtraDataLists[!a_equipIndex] = chestExDataList;
			}
		}

		bool checkLeftHand = a_slot == glob.leftHandEquipSlot;
		// Move consumables from the chest to the player before equipping.
		bool isConsumable = Util::IsConsumable(a_object);
		if (isConsumable)
		{
			// Equip right away once moved and no need to change worn rank exData for the chest list
			// since the item is not equipped.
			// Using a copy since a sporadic crash occurs if the original is moved over
			// before the equip. Want to see if a copy prevents this from happening.
			auto copiedList = Util::CopyExtraDataList(chestExDataList);
			coopActor->AddObjectToContainer
			(
				a_object, copiedList, a_count, inventoryChest.get()
			);
			// New item gets consumed and removed on equip.
			Util::EquipObject
			(
				coopActor.get(),
				a_object,
				copiedList,
				a_count,
				a_slot,
				a_queueEquip,
				a_forceEquip,
				a_playSounds,
				a_applyNow
			);
			
			// Remove the original from chest afterwards.
			const auto invCounts = coopActor->GetInventoryCounts();
			const auto iter = invCounts.find(a_object);
			if (iter == invCounts.end() || iter->second <= 0)
			{
				inventoryChest->RemoveItem
				(
					a_object,
					a_count,
					RE::ITEM_REMOVE_REASON::kRemove,
					chestExDataList, 
					nullptr
				);
			}

			return;
		}
		else if (isEquipable)
		{	
			bool alreadyPresent = Util::HasExtraDataList
			(
				coopActor.get(), a_object, a_exDataList
			);
			if (alreadyPresent)
			{
				DBG
				(
					"{}: exData list for {}'s list {:p} already in inventory. "
					"Equip right away.",
					coopActor->GetName(), 
					a_object->GetName(), 
					fmt::ptr(a_exDataList)
				);
			}

			// Make sure the extra data list is from the chest,
			// as we don't want to equip any external items,
			// which are definitely not requested by the player.
			if (!chestExDataList)
			{
				DBG
				(
					"{}: No matching chest exData list for {}'s list {:p} "
					"(found {:p}).",
					coopActor->GetName(), 
					a_object->GetName(), 
					fmt::ptr(a_exDataList),
					fmt::ptr(chestExDataList)
				);
			}
			else
			{
				DBG
				(
					"{}: Got chest exData list {:p} for {}'s list {:p}. ",
					coopActor->GetName(), 
					fmt::ptr(chestExDataList),
					a_object->GetName(), 
					fmt::ptr(a_exDataList)
				);
			}
					
			// Add a copy of the requested chest item to the player before equipping.
			// Otherwise, the object will not be visibly equipped to the player's hands,
			// despite the correct animations playing for the item.
			// No invisible swords and shields, etc.
			auto newExDataList = 
			(
				alreadyPresent ?
				a_exDataList :
				AddItemFromInventoryChest
				(
					a_object, chestExDataList, a_count, checkLeftHand
				)
			);
			// Equip with new extra data list.
			Util::EquipObject
			(
				coopActor.get(),
				a_object,
				newExDataList,
				a_count,
				a_slot,
				a_queueEquip,
				a_forceEquip,
				a_playSounds,
				a_applyNow
			);
			
			// Causing crashes if equipped back to the other hand
			// right after unequipping the original and equipping another copy 
			// to the requested hand above.
			/*if (shouldReEquipOtherHandForm)
			{
				const auto oppositeSlot = 
				(
					a_slot == glob.rightHandEquipSlot ? 
					glob.leftHandEquipSlot :
					glob.rightHandEquipSlot
				);
				DBG
				(
					"Re-equip {} in the other hand with the {} equip slot.",
					oppositeHandForm->GetName(), 
					Util::GetEditorID(oppositeSlot)
				);
				Util::EquipObject
				(
					coopActor.get(), 
					oppositeHandForm->As<RE::TESBoundObject>(), 
					oppositeHandExData, 
					1, 
					oppositeSlot
				);
			}*/
		}

		// Add worn data to the chest's extra data list once the equip completes.
		ChangeWornRankExData(a_object, checkLeftHand, true, chestExDataList);
	}

	void EquipManager::HandleCompanionPlayerUnequip
	(
		RE::TESBoundObject* a_object,
		const EquipIndex& a_equipIndex,
		RE::ExtraDataList* a_exDataList, 
		uint32_t a_count,
		const RE::BGSEquipSlot* a_slot, 
		bool a_queueEquip, 
		bool a_forceEquip, 
		bool a_playSounds,
		bool a_applyNow,
		const RE::BGSEquipSlot* a_slotToReplace
	)
	{
		// IMPORTANT:
		// Given extra data list should always be from the player's inventory.
		// Remove items, extra data lists, inventory entries, and clean up after unequipping.
		// Can specify a specific equip index to clear out in the desired forms/exData arrays.
		// 'kNone' to have the function compute it based on the item type or to not clear any slot.

		if (p->isPlayer1 || !a_object)
		{
			return;
		}
		
		// Make sure the shield equip slot is used for shields.
		auto equipSlot = 
		(
			a_object->As<RE::TESObjectARMO>() &&
			a_object->As<RE::TESObjectARMO>()->IsShield() ? 
			a_object->As<RE::TESObjectARMO>()->equipSlot :
			a_slot
		);
		ClearDesiredEquippedForm(a_object, equipSlot, a_equipIndex);
		
		bool checkLeftHand = equipSlot == glob.leftHandEquipSlot;
		bool wasEquipped = p->em->IsEquipped(a_object, a_exDataList, checkLeftHand);
		// Unequip first.
		Util::UnequipObject
		(
			coopActor.get(),
			a_object,
			a_exDataList,
			a_count,
			equipSlot,
			a_queueEquip,
			a_forceEquip,
			a_playSounds,
			a_applyNow,
			a_slotToReplace
		);

		// Have to remove the biped object (object model) 
		// because the unequip call and removal from the inventory below
		// sometimes fails to do this for us.
		// I hope this is adequate and accounts for odd situations 
		// where the left hand item is in other slots too,
		// but at this point, f--- it.
		const auto biped = coopActor->GetBiped();
		if (biped && wasEquipped)
		{
			for (auto bipedIndex = 0; bipedIndex < RE::BIPED_OBJECT::kTotal; ++bipedIndex)
			{
				const auto& bipedObj = biped->objects[bipedIndex];
				if (bipedObj.item == a_object)
				{
					if ((equipSlot == glob.shieldEquipSlot &&
						bipedIndex == RE::BIPED_OBJECT::kShield) ||
						(equipSlot != glob.leftHandEquipSlot &&
						bipedIndex != RE::BIPED_OBJECT::kShield) ||
						(equipSlot == glob.leftHandEquipSlot &&
						bipedIndex == RE::BIPED_OBJECT::kShield))
					{
						DBG("Remove {}'s biped object at index {}.",
							a_object->GetName(), bipedIndex);
						coopActor->RemoveWeapon
						(
							static_cast<RE::BIPED_OBJECT>(bipedIndex)
						);
					}
				}
			}
		}

		/*coopActor->RemoveItem
		(
			a_object,
			a_count,
			RE::ITEM_REMOVE_REASON::kRemove,
			a_exDataList,
			nullptr
		);

		return;*/

		// REMOVE when done debugging.
		bool inInventory = coopActor->GetInventory().contains(a_object);
		DBG("In inventory: {}.", inInventory);
		bool isEquipped = p->em->IsEquipped
		(
			a_object, 
			a_exDataList, 
			equipSlot == glob.leftHandEquipSlot
		);
		DBG
		(
			"{}: {} with list {:p} {} and is now {}.", 
			coopActor->GetName(), 
			a_object->GetName(),
			fmt::ptr(a_exDataList),
			wasEquipped ? "was equipped" : "was not equipped",
			isEquipped ? "equipped" : "not equipped"
		);

		auto invChanges = coopActor->GetInventoryChanges();
		if (!invChanges)
		{
			return;
		}

		// New list of inventory entries to potentially swap in if any entries are removed.
		auto newEntryList = new std::remove_pointer_t<decltype(invChanges->entryList)>();
		if (!newEntryList)
		{
			ERR
			(
				"ERR: {}: Failed to allocate new inventory entry list.", coopActor->GetName()
			);
			return;
		}

		// Indicates that the unequipped item should not be 
		// in the player's inventory anymore and the new entry list,
		// with the item's entry removed, should be swapped in.
		bool changedEntryList = false;
		if (invChanges->entryList)
		{
			auto oldLength = std::distance
			(
				invChanges->entryList->begin(), invChanges->entryList->end()
			);
			DBG("Old entry lists has size {}.", oldLength);
			for (auto iter = invChanges->entryList->begin();
				iter != invChanges->entryList->end();
				++iter)
			{
				auto entry = *iter;
				if (!entry || entry->object != a_object)
				{
					if (newEntryList)
					{
						newEntryList->push_front(entry);
					}

					continue;
				}
						
				// Allocate another list of lists when finding a new matching bound object,
				// just in case there are multiple inventory entries for the same object.
				auto newExDataLists = 
				(
					new std::remove_pointer_t<decltype(entry->extraLists)>()
				);
				bool swappedInNewLists = false;
				// Should add the current entry back to the list, instead of skipping it,
				// which removes the item when our new entry list
				// is swapped with the current one.
				bool shouldKeepEntry = false;
				if (entry->extraLists)
				{
					oldLength = std::distance
					(
						entry->extraLists->begin(), entry->extraLists->end()
					);
					const auto oldDelta = entry->countDelta;
					DBG
					(
						"Old extra lists has size {} and entry count delta is {}.",
						oldLength, oldDelta
					);
					for (const auto list : *entry->extraLists)
					{
						if (list != a_exDataList)
						{
							auto count = list->GetCount();
							DBG
							(
								"Not removing list {:p} when searching for {:p}. Count: {}",
								fmt::ptr(list),
								fmt::ptr(a_exDataList),
								count
							);
							newExDataLists->push_front(list);
						}
						else
						{
							DBG
							(
								"Still has {} of {} with list {:p}.",
								list->GetCount(),
								a_object->GetName(),
								fmt::ptr(list)
							);
						}
					}

					if (newExDataLists->empty())
					{
						DBG
						(
							"New lists empty: {}, count delta: {}.",
							newExDataLists->empty(), entry->countDelta
						);
						// Remove the inventory entry since there are no lists for it.
						// Also, no lists remain, so RE::free our allocated lists.
					}
					else if (auto newLength = std::distance
							(
								newExDataLists->begin(), newExDataLists->end()
							); newLength != oldLength || entry->countDelta != oldDelta)
					{
						DBG
						(
							"New lists has size {} and count delta is {}. "
							"Removed matching lists.",
							std::distance(newExDataLists->begin(), newExDataLists->end()),
							entry->countDelta
						);
						// Swap newly allocated list with current and free current.
						auto oldLists = entry->extraLists;
						entry->extraLists = newExDataLists;
						delete oldLists;
						oldLists = nullptr;
						// Will NOT RE::free our new lists.
						swappedInNewLists = true;
						// Do not skip adding this modified entry.
						shouldKeepEntry = true;
					}
					else
					{
						DBG
						(
							"New lists same as old ones. No replacement required."
						);
						// All current lists should remain unchanged, 
						// so free our allocated ones.
						// Keep entry as is and add.
						shouldKeepEntry = true;
					}
				}
				else
				{
					DBG
					(
						"Dec matched entry count delta to {}. Remove entry: {}.",
						entry->countDelta - 1, entry->countDelta - 1 <= 0
					);
					entry->countDelta -= 1;
					// Will remove the inventory entry when the count hits 0.
					// Otherwise, keep the entry.
					if (entry->countDelta > 0)
					{
						// Add the entry back with its modified count.
						shouldKeepEntry = true;
					}

					// No exData list to swap in since there isn't one originally,
					// so RE::free our allocated extra data lists.
				}

				DBG
				(
					"{}'s count delta is now {}. From entry {:p}.",
					a_object->GetName(),
					entry->countDelta,
					fmt::ptr(entry)
				);

				if (!swappedInNewLists)
				{
					DBG
					(
						"CHECKED ENTRY: Free allocated extra data lists {:p} for {}.",
						fmt::ptr(newExDataLists), a_object->GetName()
					);
					delete newExDataLists;
					newExDataLists = nullptr;
				}
				else
				{
					DBG
					(
						"CHECKED ENTRY: Swapped in new extra data lists {:p} for {}.",
							fmt::ptr(newExDataLists), a_object->GetName()
					);
				}

				if (shouldKeepEntry)
				{
					DBG
					(
						"CHECKED ENTRY: Keep old entry {:p} for {}.",
						fmt::ptr(entry), a_object->GetName()
					);
					newEntryList->push_front(entry);
				}
				else
				{
					DBG
					(
						"CHECKED ENTRY: Remove old entry {:p} for {}.",
						fmt::ptr(entry), a_object->GetName()
					);
					changedEntryList = true;
				}
			}
		}

		if (changedEntryList)
		{
			DBG
			(
				"FINISH: Removing inventory entry for {}. "
				"New entry list has size {}. In inventory: {}, changed entry list: {}.",
				a_object->GetName(),
				std::distance(newEntryList->begin(), newEntryList->end()),
				inInventory,
				changedEntryList
			);
			auto oldList = invChanges->entryList;
			invChanges->entryList = newEntryList;
			delete oldList;
			oldList = nullptr;
		}
		else
		{
			DBG
			(
				"FINISH: New entries same as old ones. No removal required."
			);
			// RE::free new inventory entry list.
			delete newEntryList;
			newEntryList = nullptr;
		}

		// Remove chest worn exData as needed afterward.
		ChangeWornRankExData(a_object, checkLeftHand, false);
	}
	
	void EquipManager::HandleEquipRequest
	(
		RE::TESForm* a_form, 
		RE::ExtraDataList* a_exData,
		const EquipIndex& a_index,
		bool a_shouldEquip
	)
	{
		// Handle companion player (un)equip request for the given form at the given index.
		// NOTE:
		// Never called on P1.

		DBG
		(
			"{}: form: {}, index: {}, should equip: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_index, 
			a_shouldEquip
		);

		if (!a_form)
		{
			return;
		}

		if (a_shouldEquip)
		{
			// Equip.
			switch (*a_form->formType)
			{
			case RE::FormType::Weapon:
			{
				EquipForm(a_form, a_index, a_exData, 1, GetEquipSlotForForm(a_form, a_index));

				break;
			}
			case RE::FormType::Armor:
			case RE::FormType::Armature:
			{
				EquipArmor(a_form, a_exData);

				break;
			}
			case RE::FormType::Spell:
			{
				EquipSpell(a_form, a_index, GetEquipSlotForForm(a_form, a_index));

				break;
			}
			case RE::FormType::Ammo:
			{
				EquipAmmo(a_form, a_exData);

				break;
			}
			case RE::FormType::Shout:
			{
				EquipShout(a_form);

				break;
			}
			case RE::FormType::Light:
			{
				auto asLight = a_form->As<RE::TESObjectLIGH>(); 
				if (!asLight || !asLight->CanBeCarried())
				{
					return;
				}
				
				EquipForm(a_form, EquipIndex::kLeftHand, a_exData, 1, asLight->equipSlot);

				break;
			}
			case RE::FormType::AlchemyItem:
			{
				auto asAlchemyItem = a_form->As<RE::AlchemyItem>();
				if (!asAlchemyItem)
				{
					return;
				}

				if (asAlchemyItem->IsPoison())
				{
					// Apply poison if the requested hand has inventory entry data.
					auto weapInvData = coopActor->GetEquippedEntryData
					(
						a_index == EquipIndex::kLeftHand
					);

					if (weapInvData)
					{
						weapInvData->PoisonObject(asAlchemyItem, 1);
						// Remove after applying the poison.
						if (p->isPlayer1)
						{
							coopActor->RemoveItem
							(
								asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
							);
						}
						else
						{
							inventoryChest->RemoveItem
							(
								asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
							);
						}
					}
				}
				else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Equip the alchemy item as an object to use it.
					// Just equip, do not update the desired equipped forms list 
					// since there is no slot for it.
					if (p->isPlayer1)
					{
						Util::EquipObject
						(
							coopActor.get(),
							asAlchemyItem,
							a_exData,
							1,
							nullptr
						);
					}
					else
					{
						HandleCompanionPlayerEquip
						(
							asAlchemyItem,
							EquipIndex::kNone,
							a_exData,
							1,
							nullptr
						);
					}
				}

				break;
			}
			case RE::FormType::Ingredient: 
			{
				auto asIngredientItem = a_form->As<RE::IngredientItem>();
				if (!asIngredientItem)
				{
					return;
				}
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Equip the alchemy item as an object to use it.
					// Just equip, do not update the desired equipped forms list 
					// since there is no slot for it.
					if (p->isPlayer1)
					{
						Util::EquipObject
						(
							coopActor.get(),
							asIngredientItem,
							a_exData,
							1,
							nullptr
						);
					}
					else
					{
						HandleCompanionPlayerEquip
						(
							asIngredientItem,
							EquipIndex::kNone,
							a_exData,
							1,
							nullptr
						);
					}
				}

				break;
			}
			default:
			{
				auto boundObj = a_form->As<RE::TESBoundObject>(); 
				auto aem = RE::ActorEquipManager::GetSingleton(); 
				if (!aem || !boundObj)
				{
					return;
				}
				
				// Just equip, do not update the desired equipped forms list 
				// since there is no slot for it.
				if (p->isPlayer1)
				{
					Util::EquipObject
					(
						coopActor.get(),
						boundObj,
						a_exData,
						1,
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerEquip
					(
						boundObj,
						EquipIndex::kNone,
						a_exData,
						1,
						nullptr
					);
				}

				break;
			}
			}
		}
		else
		{
			// Unequip.
			switch (*a_form->formType)
			{
			case RE::FormType::Weapon:
			{
				UnequipForm(a_form, a_index, 1, GetEquipSlotForForm(a_form, a_index));

				break;
			}
			case RE::FormType::Armor:
			case RE::FormType::Armature:
			{
				auto asArmor = a_form->As<RE::TESObjectARMO>(); 
				if (!asArmor)
				{
					return;
				}

				UnequipArmor(a_form);

				break;
			}
			case RE::FormType::Spell:
			{
				auto spell = a_form->As<RE::SpellItem>(); 
				if (!spell)
				{
					return;
				}

				// LH, RH, 2H, voice.
				UnequipSpell(a_form, a_index);

				break;
			}
			case RE::FormType::Ammo:
			{
				UnequipAmmo(a_form);

				break;
			}
			case RE::FormType::Shout:
			{
				UnequipShout(a_form);

				break;
			}
			case RE::FormType::Light:
			{
				auto asLight = a_form->As<RE::TESObjectLIGH>(); 
				if (!asLight || !asLight->CanBeCarried())
				{
					return;
				}

				UnequipForm(a_form, EquipIndex::kLeftHand, 1, asLight->equipSlot);

				break;
			}
			default:
			{
				auto boundObj = a_form->As<RE::TESBoundObject>(); 
				auto aem = RE::ActorEquipManager::GetSingleton(); 
				if (!aem || !boundObj)
				{
					return;
				}

				// Just unequip, do not update the desired equipped forms list 
				// since there is no slot for it.
				if (p->isPlayer1)
				{
					Util::UnequipObject
					(
						coopActor.get(),
						boundObj,
						a_exData,
						1,
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerUnequip
					(
						boundObj, 
						EquipIndex::kNone,
						a_exData, 
						1, 
						nullptr
					);
				}

				break;
			}
			}
		}
	}

	void EquipManager::HandleMenuEquipRequest
	(
		RE::ObjectRefHandle a_fromContainerHandle, 
		RE::TESForm* a_form, 
		RE::ExtraDataList* a_exData,
		const EquipIndex& a_index, 
		bool a_placeholderMagicChanged
	)
	{
		// Handle MIM equip request for the given form, 
		// from the given container, at the given index.
		// Also take into account whether a placeholder spell was changed 
		// from copying over the form to equip.
		
		// NOTE: 
		// Never called on P1.
		// NOTE 2: 
		// Spells to equip should not be placeholder spells.

		DBG
		(
			"{}: container: {}, form: {}, index: {}, "
			"placeholder spell changed: {}.", 
			coopActor->GetName(),
			Util::HandleIsValid(a_fromContainerHandle) ? 
			Util::GetRefrPtrFromHandle(a_fromContainerHandle)->GetName() : 
			"NONE",
			a_form ? a_form->GetName() : "NONE",
			a_index,
			a_placeholderMagicChanged
		);

		// Must have a container from which the item originated
		// and a valid item.
		auto fromContainerPtr = Util::GetRefrPtrFromHandle(a_fromContainerHandle);
		if (!fromContainerPtr || !a_form)
		{
			return;
		}

		// Do not attempt to unequip an item from a non-player container if it is already equipped.
		if (fromContainerPtr != coopActor &&
			fromContainerPtr != inventoryChest &&
			IsEquipped(a_form, a_exData, a_index == EquipIndex::kLeftHand))
		{
			return;
		}

		// Equip the form if it isn't in the requested slot already; otherwise, unequip it.
		switch (*a_form->formType)
		{
		case RE::FormType::Weapon:
		{
			auto equipSlot = GetEquipSlotForForm(a_form, a_index);
			if (auto currentFormInHand = equippedForms[!a_index]; a_form != currentFormInHand)
			{
				EquipForm(a_form, a_index, a_exData, 1, equipSlot);
			}
			else
			{
				UnequipForm(a_form, a_index, 1, equipSlot);
			}

			break;
		}
		case RE::FormType::Armor:
		case RE::FormType::Armature:
		{
			RE::TESObjectARMO* currentArmorInSlot = nullptr;
			auto asArmor = a_form->As<RE::TESObjectARMO>(); 
			if (!asArmor)
			{
				return;
			}

			currentArmorInSlot = coopActor->GetWornArmor(asArmor->GetSlotMask());
			if (a_form != currentArmorInSlot)
			{
				EquipArmor(a_form, a_exData);
			}
			else
			{
				UnequipArmor(a_form);
			}

			break;
		}
		case RE::FormType::Scroll:
		{
			// TODO:
			// Scrolls equip their corresponding spells to the player,
			// but the player's ranged attack package fails to cast the spell.
			// Cast the scroll spell straight away for now.

			RE::DebugNotification
			(
				"[ALYSLC] Equipping scrolls not yet implemented. Casting scroll spell instead."
			);
			DBG
			(
				"{}: Attempting to equip a scroll: {}.", coopActor->GetName(), a_form->GetName()
			);
			
			p->pam->CastScrollSpell(a_form->As<RE::ScrollItem>());

			break;
		}
		case RE::FormType::Spell:
		{
			// Check if already equipped by comparing the requested spell 
			// with the corresponding copied spell,
			// and comparing the currently equipped spell 
			// to the placeholder spell in the same slot.
			RE::TESForm* currentForm = equippedForms[!a_index];
			// Should equip if there is no currently-equipped form in the slot.
			bool shouldEquip = !currentForm;
			RE::TESForm* currentCopiedSpell = nullptr;
			if (!shouldEquip) 
			{
				bool currentSpellIsVoice =
				(
					currentForm->As<RE::BGSEquipType>() && 
					currentForm->As<RE::BGSEquipType>()->equipSlot == glob.voiceEquipSlot
				);
				if (currentSpellIsVoice)
				{
					// Equip voice slot spell if it is not the same as the current one.
					shouldEquip = a_form != currentForm;
				}
				else
				{
					bool currentSpellIs2H =
					(
						currentForm->As<RE::BGSEquipType>() &&
						currentForm->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
					);
					PlaceholderMagicIndex currentPlaceholderIndex = 
					(
						currentSpellIs2H ?
						PlaceholderMagicIndex::k2H :
						static_cast<PlaceholderMagicIndex>(!a_index)
					);
					currentCopiedSpell = GetCopiedMagic(currentPlaceholderIndex);
					// If the placeholder spell will change 
					// when the requested spell is copied over
					// or if not already equipped, equip the spell.
					shouldEquip = 
					(
						(a_placeholderMagicChanged) || 
						(
							a_form != currentCopiedSpell || 
							currentForm != GetPlaceholderMagic(currentPlaceholderIndex)
						)
					);
				}
			}

			// CHANGE TO DEBUG
			DBG
			(
				"{}: {} requested form {} (0x{:X}). Current form: {} (0x{:X}), "
				"current copied spell: {} (0x{:X}). Placeholder magic changed: {}.",
				coopActor->GetName(), 
				shouldEquip ? "equip" : "unequip",
				a_form ? a_form->GetName() : "NONE",
				a_form ? a_form->formID : 0xDEAD,
				currentForm ? currentForm->GetName() : "NONE",
				currentForm ? currentForm->formID : 0xDEAD,
				currentCopiedSpell ? currentCopiedSpell->GetName() : "NONE",
				currentCopiedSpell ? currentCopiedSpell->formID : 0xDEAD,
				a_placeholderMagicChanged
			);

			if (shouldEquip)
			{
				EquipSpell(a_form, a_index, GetEquipSlotForForm(a_form, a_index));
			}
			else
			{
				UnequipSpell(a_form, a_index);
			}

			break;
		}
		case RE::FormType::Ammo:
		{
			if (auto currentAmmo = equippedForms[!EquipIndex::kAmmo]; a_form != currentAmmo)
			{

				EquipAmmo(a_form, a_exData);
			}
			else
			{
				UnequipAmmo(a_form);
			}

			break;
		}
		case RE::FormType::Shout:
		{
			auto currentVoiceMag = equippedForms[!EquipIndex::kVoice]; 
			if (a_form != currentVoiceMag)
			{
				EquipShout(a_form);
			}
			else
			{
				UnequipShout(a_form);
			}

			break;
		}
		case RE::FormType::Light:
		{
			// Torch.
			auto asLight = a_form->As<RE::TESObjectLIGH>(); 
			if (!asLight || !asLight->CanBeCarried())
			{
				return;
			}

			auto currentLHForm = equippedForms[!EquipIndex::kLeftHand]; 
			if (a_form != currentLHForm)
			{
				EquipForm(a_form, EquipIndex::kLeftHand, a_exData, 1, asLight->equipSlot);
			}
			else
			{
				UnequipForm(a_form, EquipIndex::kLeftHand, 1, asLight->equipSlot);
			}
			
			break;
		}
		case RE::FormType::AlchemyItem:
		{
			auto asAlchemyItem = a_form->As<RE::AlchemyItem>();
			if (!asAlchemyItem)
			{
				return;
			}

			if (asAlchemyItem->IsPoison())
			{
				// Apply poison if the requested hand has inventory entry data.
				auto weapInvData = coopActor->GetEquippedEntryData
				(
					a_index == EquipIndex::kLeftHand
				);
				if (weapInvData)
				{
					weapInvData->PoisonObject(asAlchemyItem, 1);
					// Remove after applying the poison.
					if (p->isPlayer1)
					{
						coopActor->RemoveItem
						(
							asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
						);
					}
					else
					{
						inventoryChest->RemoveItem
						(
							asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
						);
					}
				}
			}
			else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				// Equip the alchemy item as an object to use it.
				// Just equip, do not update the desired equipped forms list 
				// since there is no slot for it.
				if (p->isPlayer1)
				{
					Util::EquipObject
					(
						coopActor.get(),
						asAlchemyItem,
						a_exData,
						1,
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerEquip
					(
						asAlchemyItem,
						EquipIndex::kNone,
						a_exData,
						1,
						nullptr
					);
				}
			}

			break;
		}
		case RE::FormType::Ingredient: 
		{
			auto asIngredientItem = a_form->As<RE::IngredientItem>();
			if (!asIngredientItem)
			{
				return;
			}

			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				// Equip the alchemy item as an object to use it.
				// Just equip, do not update the desired equipped forms list 
				// since there is no slot for it.
				if (p->isPlayer1)
				{
					Util::EquipObject
					(
						coopActor.get(),
						asIngredientItem,
						a_exData,
						1,
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerEquip
					(
						asIngredientItem,
						EquipIndex::kNone,
						a_exData,
						1,
						nullptr
					);
				}
			}

			break;
		}
		default:
		{
			auto aem = RE::ActorEquipManager::GetSingleton(); 
			auto boundObj = a_form->As<RE::TESBoundObject>(); 
			if (!aem || !boundObj)
			{
				return;
			}

			// Just (un)equip, do not update the desired equipped forms list 
			// since there is no slot for it.

			// NOTE: 
			// Revert to always equipping if issues arise.
			if (!IsEquipped(a_form, a_exData, a_index == EquipIndex::kLeftHand)) 
			{
				if (p->isPlayer1)
				{
					Util::EquipObject
					(
						coopActor.get(), 
						boundObj, 
						a_exData, 
						1, 
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerEquip
					(
						a_form->As<RE::TESBoundObject>(),
						EquipIndex::kNone,
						a_exData,
						1,
						nullptr
					);
				}
			}
			else
			{
				if (p->isPlayer1)
				{
					Util::UnequipObject
					(
						coopActor.get(), 
						boundObj, 
						a_exData, 
						1, 
						nullptr
					);
				}
				else
				{
					HandleCompanionPlayerUnequip
					(
						a_form->As<RE::TESBoundObject>(),
						EquipIndex::kNone,
						a_exData,
						1,
						nullptr
					);
				}
				
			}

			break;
		}
		}
	}

	bool EquipManager::HasCyclableSpellInCategory(const FavMagicCyclingCategory& a_category)
	{
		// Does the given favorited spell category have a cyclable, equipable spell?

		// Invalid category.
		if (a_category == FavMagicCyclingCategory::kNone || 
			a_category == FavMagicCyclingCategory::kTotal)
		{
			return false;
		}

		// All Favorites: only has a spell if at least one spell is favorited.
		if (a_category == FavMagicCyclingCategory::kAllFavorites)
		{
			return !cyclableFormsMap[CyclableForms::kSpell].empty();
		}

		const auto& formsList = cyclableFormsMap[CyclableForms::kSpell];
		RE::SpellItem* spell = nullptr;
		for (const auto spellForm : formsList)
		{
			spell = spellForm->As<RE::SpellItem>(); 
			if (!spell)
			{
				continue;
			}

			auto spellType = spell->GetSpellType();
			// Not a hand spell.
			if (spellType != RE::MagicSystem::SpellType::kSpell)
			{
				continue;
			}

			// Check associated skill or equip slot and match with category.
			switch (a_category)
			{
			case FavMagicCyclingCategory::kAlteration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kAlteration)
				{
					return true;
				}

				continue;
			}
			case FavMagicCyclingCategory::kConjuration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kConjuration)
				{
					return true;
				}

				continue;
			}
			case FavMagicCyclingCategory::kDestruction:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kDestruction)
				{
					return true;
				}

				continue;
			}
			case FavMagicCyclingCategory::kIllusion:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kIllusion)
				{
					return true;
				}

				continue;
			}
			case FavMagicCyclingCategory::kRestoration:
			{
				if (spell->GetAssociatedSkill() == RE::ActorValue::kRestoration)
				{
					return true;
				}

				continue;
			}
			case FavMagicCyclingCategory::kRitual:
			{
				// 2H spell.
				if (spell->equipSlot == glob.bothHandsEquipSlot)
				{
					return true;
				}

				continue;
			}
			default:
			{
				return false;
			}
			}
		}

		return false;
	}

	bool EquipManager::HasCyclableWeaponInCategory
	(
		const FavWeaponCyclingCategory& a_category, const bool& a_rightHand
	)
	{
		// Does the given favorited weapon category have a cyclable, 
		// equipable weapon for the given hand?

		// Invalid category.
		if (a_category == FavWeaponCyclingCategory::kNone || 
			a_category == FavWeaponCyclingCategory::kTotal)
		{
			return false;
		}

		// All favorites: only has a spell if at least one physical, 
		// hand-equipable form is favorited.
		if (a_category == FavWeaponCyclingCategory::kAllFavorites)
		{
			return !cyclableFormsMap[CyclableForms::kWeapon].empty();
		}

		const auto& formsList = cyclableFormsMap[CyclableForms::kWeapon];
		RE::TESObjectWEAP* weapon = nullptr;
		for (const auto form : formsList)
		{
			auto equipType = form->As<RE::BGSEquipType>();
			bool isShield = 
			(
				form->As<RE::TESObjectARMO>() && 
				form->As<RE::TESObjectARMO>()->IsShield()
			);
			bool isTorch = 
			(
				form->As<RE::TESObjectLIGH>() && 
				form->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry)
			);
			// Incompatible equip slot.
			if ((a_rightHand) && (isShield || isTorch))
			{
				continue;
			}

			// Check weapon type and weapon keyword as needed.
			if (weapon = form->As<RE::TESObjectWEAP>(); weapon)
			{
				// Check associated skill or equip slot and match with category.
				switch (a_category)
				{
				case FavWeaponCyclingCategory::kAxe:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandAxe || 
						weapon->HasKeywordString("WeapTypeWarAxe"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kBattleaxe:
				{
					// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && 
						weapon->HasKeywordString("WeapTypeBattleaxe"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kBow:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow || 
						weapon->HasKeywordString("WeapTypeBow"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kCrossbow:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow)
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kDagger:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandDagger || 
						weapon->HasKeywordString("WeapTypeDagger"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kGreatsword:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandSword || 
						weapon->HasKeywordString("WeapTypeGreatsword"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kMace:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandMace ||
						weapon->HasKeywordString("WeapTypeMace"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kShieldAndTorch:
				{
					// Handled below instead, since neither are weapons.
					continue;
				}
				case FavWeaponCyclingCategory::kStaff:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff || 
						weapon->HasKeywordString("WeapTypeStaff"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kSword:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandSword ||
						weapon->HasKeywordString("WeapTypeSword"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kUnique:
				{
					if (weapon->HasKeywordString("WeapTypeUnique"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kWarhammer:
				{
					// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && 
						weapon->HasKeywordString("WeapTypeWarhammer"sv))
					{
						return true;
					}

					continue;
				}
				default:
				{
					return false;
				}
				}
			}
			else if ((isShield || isTorch) && 
					 (a_category == FavWeaponCyclingCategory::kAllFavorites ||  
					  a_category == FavWeaponCyclingCategory::kShieldAndTorch))
			{
				return true;
			}
		}

		return false;
	}

	void EquipManager::ImportCoopFavorites(bool&& a_onlyMagicFavorites)
	{
		// Import this companion player's favorited items/magic onto P1.
		// Can choose to only import the companion player's favorited spells.
		// Wipe all P1's favorites and hotkeys before importing the companion player's.
		
		// Obviously should not be called on P1.
		if (p->isPlayer1)
		{
			return;
		}

		DBG
		(
			"{}. Only magic favorites: {}.", coopActor->GetName(), a_onlyMagicFavorites
		);

		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1)
		{
			return;
		}

		const auto& coopP1 = glob.coopPlayers[0];
		// Update P1's favorites and check for new favorited spells, 
		// instead of using serialized data.
		coopP1->em->UpdateFavoritedFormsLists(false);
		// Use cached magic favorites here for the companion player
		// since the current list of magic favorites is P1's
		// and we do not want to import P1's magic favorites instead.
		UpdateFavoritedFormsLists(true);

		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (!magicFavorites)
		{
			DBG
			(
				"ImportCoopFavorites: {}: Could not get magic favorites singleton.", 
				coopActor->GetName()
			);
			return;
		}

		const auto& p1Favorites = coopP1->em->favoritedForms;
		const auto& favorites = favoritedForms;
		auto& p1FavoritesMap = glob.p1FavoritedFormsMap;
		p1FavoritesMap.clear();

		// Clear out hotkeyed forms, since they linger behind even after the form is unfavorited.
		// If the companion player hotkeys the same form to a different slot,
		// there can be bugs when using the serialized data, 
		// since the same form is serialized multiple times.
		// P1's hotkeys and magic favorites were cached above already 
		// and will be restored later on.
		for (auto i = 0; i < magicFavorites->hotkeys.size(); ++i)
		{
			magicFavorites->hotkeys[i] = nullptr;
		}

		// Unfavorite all of P1's magic favorites before favoriting
		// this companion player's magic favorites.
		// RemoveFavorite() modifies and shifts elements of the magical favorites list.
		// Remove the first element until the list is empty.
		uint32_t removalIndex = 0;
		while (removalIndex < magicFavorites->spells.size())
		{
			auto magForm = magicFavorites->spells[removalIndex];
			// Skip over empty entries and start removing from the next index.
			if (!magForm) 
			{
				++removalIndex;
				continue;
			}

			// No extra data lists for magic forms.
			p1FavoritesMap.insert
			(
				{ 
					magForm, 
					std::set<std::pair<RE::ExtraDataList*, int8_t>>
					(
						{ { nullptr, !RE::ExtraHotkey::Hotkey::kUnbound } }
					) 
				}
			);
			magicFavorites->RemoveFavorite(magForm);
			DBG
			(
				"{}: Remove P1 favorited magic {} at index {}. List size: {}.", 
				coopActor->GetName(),
				magForm ? magForm->GetName() : "NONE",
				removalIndex,
				magicFavorites->spells.size()
			);
		}

		// Clear when done.
		magicFavorites->spells.clear();

		if (!a_onlyMagicFavorites) 
		{
			auto p1Inv = p1->GetInventory();
			for (const auto& [boundObj, countInvEntryPair] : p1Inv)
			{
				if (!boundObj || 
					!countInvEntryPair.second ||
					!countInvEntryPair.second->extraLists ||
					countInvEntryPair.first <= 0)
				{
					continue;
				}	

				for (auto exDataList : *countInvEntryPair.second->extraLists)
				{
					if (!exDataList)
					{
						continue;
					}

					// Favorited if hotkey data exists.
					auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
					if (!exHotkey)
					{
						continue;
					}

					DBG
					(
						"P1: BEFORE: {} was favorited and has hotkey {} "
						"on list {:p} from inventory.",
						countInvEntryPair.second->object->GetName(), 
						*exHotkey->hotkey,
						fmt::ptr(exDataList)
					);

					// Save this extra data list and hotkey to restore favorites status later.
					const auto iter = p1FavoritesMap.find(countInvEntryPair.second->object);
					if (iter != p1FavoritesMap.end())
					{
						iter->second.insert({ exDataList, !*exHotkey->hotkey });
					}
					else
					{
						p1FavoritesMap.insert
						(
							{ 
								countInvEntryPair.second->object, 
								std::set<std::pair<RE::ExtraDataList*, int8_t>>
								(
									{ { exDataList, !*exHotkey->hotkey } }
								)
							}
						);
					}

					// Do not remove the item, just clear the favorites status.
					Util::NativeFunctions::Unfavorite
					(
						p1->GetInventoryChanges(), countInvEntryPair.second.get(), exDataList
					);
					exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
					DBG
					(
						"P1: AFTER: {} is favorited ({}) and has hotkey {} "
						"on list {:p} from inventory.",
						countInvEntryPair.second->object->GetName(), 
						(bool)exHotkey,
						exHotkey ? !*exHotkey->hotkey : 255,
						fmt::ptr(exDataList)
					);
				}
			}

			// Add companion player's favorites.
			auto chestInv = inventoryChest->GetInventory();
			for (const auto& [boundObj, countInvEntryPair] : chestInv)
			{
				if (!boundObj ||
					!countInvEntryPair.second ||
					!countInvEntryPair.second->extraLists)
				{
					continue;
				}	

				for (auto exDataList : *countInvEntryPair.second->extraLists)
				{
					if (!exDataList)
					{
						continue;
					}

					// Favorited if hotkey data exists.
					auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
					if (!exHotkey)
					{
						continue;
					}

					DBG
					(
						"{}: {} was favorited and has hotkey {} "
						"on list {:p} from inventory chest.",
						coopActor->GetName(), 
						countInvEntryPair.second->object->GetName(), 
						*exHotkey->hotkey,
						fmt::ptr(exDataList)
					);
					p1->AddObjectToContainer
					(
						countInvEntryPair.second->object,
						Util::CopyExtraDataList(exDataList),
						1,
						nullptr
					);
				}
			}

			p1Inv = p1->GetInventory();
			for (const auto& [boundObj, countInvEntryPair] : p1Inv)
			{
				if (!boundObj || 
					!countInvEntryPair.second || 
					!countInvEntryPair.second->extraLists || 
					countInvEntryPair.first <= 0)
				{
					continue;
				}	

				for (auto exDataList : *countInvEntryPair.second->extraLists)
				{
					if (!exDataList)
					{
						continue;
					}

					// Favorited if hotkey data exists.
					auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
					if (!exHotkey)
					{
						continue;
					}
							
					for (auto type = RE::ExtraDataType::kNone; 
						type <= RE::ExtraDataType::kUnkBF; 
						type = static_cast<RE::ExtraDataType>(!type + 1))
					{
						if (auto data = exDataList->GetByType(type); data)
						{
							DBG
							(
								"IN P1: Favorited object {} has exData list {:p} "
								"with data {:p} of type 0x{:X}.",
								countInvEntryPair.second->object->GetName(),
								fmt::ptr(exDataList),
								fmt::ptr(data),
								type
							);
						}
					}

					if (*exHotkey->hotkey != RE::ExtraHotkey::Hotkey::kUnbound)
					{
						// Remove any hotkeys that the companion player has not applied.
						auto setIter = hotkeyedFormsToSlotsSetMap.find
						(
							countInvEntryPair.second->object->formID
						);
						if (setIter == hotkeyedFormsToSlotsSetMap.end() || 
							!setIter->second.contains(!*exHotkey->hotkey))
						{
							DBG
							(
								"IN P1: {} has hotkey {} which should be removed on list {:p}.",
								countInvEntryPair.second->object->GetName(), 
								*exHotkey->hotkey,
								fmt::ptr(exDataList)
							);
							Util::ChangeFormHotkeyStatus
							(
								p1, countInvEntryPair.second->object, -1, exDataList
							);
						}
					}

					DBG
					(
						"IN P1: {} was favorited and has hotkey {} "
						"on list {:p}.",
						countInvEntryPair.second->object->GetName(), 
						*exHotkey->hotkey,
						fmt::ptr(exDataList)
					);
				}
			}
		}

		// Favorite all the companion player's favorited magical forms and update hotkeys.
		for (auto i = 0; i < favorites.size(); ++i)
		{
			const auto form = favorites[i];
			if (!form)
			{
				continue;
			}
			
			// Skip hotkey importation for physical forms which were already handled above.
			if (!a_onlyMagicFavorites && form->IsNot(RE::FormType::Spell, RE::FormType::Shout))
			{
				continue;
			}

			if (form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				DBG
				(
					"{}: Add favorited magic {}.", coopActor->GetName(), form->GetName()
				);
				magicFavorites->SetFavorite(form);
			}

			// Hotkeyed by the companion player, so set the corresponding hotkey slot.
			const auto iter = hotkeyedFormsToSlotsSetMap.find(form->formID);
			if (iter != hotkeyedFormsToSlotsSetMap.end())
			{
				// Should not happen, but remove the hotkey if it is not in the set.
				if (iter->second.empty())
				{
					DBG
					(
						"{}: Removing hotkey for {}.",
						coopActor->GetName(), 
						form->GetName()
					);
					Util::ChangeFormHotkeyStatus(p1, form, -1);
				}
				else
				{
					for (const auto& hotkeyIndex : iter->second)
					{
						DBG
						(
							"{}: Adding hotkey {} for {}.",
							coopActor->GetName(), 
							hotkeyIndex == -1 ? -1 : hotkeyIndex + 1, 
							form->GetName()
						);
						Util::ChangeFormHotkeyStatus(p1, form, hotkeyIndex);
					}
				}
			}
		}
	}

	bool EquipManager::IsEquipped
	(
		RE::TESForm* a_form, RE::ExtraDataList* a_exDataList, bool a_leftHand, bool a_eitherHand
	)
	{
		if (!a_form)
		{
			return false;
		}

		bool nonPhysFormEquipped = 
		(
			(
				!a_form->As<RE::TESBoundObject>() || 
				a_form->Is(RE::FormType::Spell, RE::FormType::Shout)
			) && 
			(equippedFormFIDs.contains(a_form->formID))
		);
		if (nonPhysFormEquipped)
		{
			return true;
		}

		auto invChanges = coopActor->GetInventoryChanges(); 
		if (invChanges && invChanges->entryList)
		{
			for (auto invEntryData : *invChanges->entryList)
			{
				if (invEntryData && 
					invEntryData->object && 
					invEntryData->object == a_form &&
					invEntryData->extraLists)
				{
					for (const auto& exDataList : *invEntryData->extraLists)
					{
						if (!exDataList)
						{
							continue;
						}
							
						// If no extra data list is specified, we just need to check 
						// if there is coresponding worn data in the given hand for any exData list.
						// Otherwise, there the given exData list also has to be present 
						// with the correct worn exData.
						bool isEquipped =
						(
							(!a_exDataList || a_exDataList == exDataList) &&
							(	
								(
									(a_eitherHand) && 
									(
										exDataList->HasType(RE::ExtraDataType::kWorn) ||
										exDataList->HasType(RE::ExtraDataType::kWornLeft)
									)
								) ||
								(
									!a_leftHand && 
									exDataList->HasType(RE::ExtraDataType::kWorn)
								) ||
								(
									(a_leftHand) && 
									(
										(exDataList->HasType(RE::ExtraDataType::kWornLeft)) ||
										(
											a_form->As<RE::BGSEquipType>() && 
											a_form->As<RE::BGSEquipType>()->equipSlot == 
											glob.bothHandsEquipSlot &&
											exDataList->HasType(RE::ExtraDataType::kWorn)
										)
									)
								)
							)
						);
						if (isEquipped)
						{
							return true;
						}			
					}
				}
			}
		}

		return false;
	}

	bool EquipManager::IsUnarmed() const
	{
		// Return true if the player has no hand forms equipped.

		return 
		(
			(!equippedForms[!EquipIndex::kLeftHand] && !equippedForms[!EquipIndex::kRightHand]) ||
			(
				equippedForms[!EquipIndex::kLeftHand] == glob.fists && 
				equippedForms[!EquipIndex::kRightHand] == glob.fists
			)
		);
	}

	void EquipManager::PrepInventoriesForCoop()
	{
		// Prep the player's inventory and inventory chest for co-op:
		// 1. Move any new items in the player's inventory to the chest.
		// 2. Remove any items in the player's inventory that are already in the chest.
		// 3. Clear any items that were marked as desired but are no longer in the inventory chest.
		// 4. Remove items from the chest that are marked as equipped in the chest 
		// but are no longer in the player's inventory.
		
		if (p->isPlayer1)
		{
			return;
		}

		// Keep desired forms in sync with chest state.
		for (auto i = 0; i < desiredForms.size(); ++i)
		{
			auto form = desiredForms[i];
			if (!form || form->As<RE::MagicItem>())
			{
				continue;
			}

			const auto boundObj = form->As<RE::TESBoundObject>();
			if (!boundObj)
			{
				continue;
			}

			const auto wornDataLH = Util::GetWornRankExtraDataList
			(
				inventoryChest.get(), boundObj, true
			);
			const auto wornDataRH = Util::GetWornRankExtraDataList
			(
				inventoryChest.get(), boundObj, false
			);
			// Clear desired form slot because there is no longer any recorded
			// worn extra data for this item in the player' inventory chest.
			if (!wornDataLH && !wornDataRH)
			{
				DBG
				(
					"{}: {} is no longer marked as worn in the inventory chest. "
					"Clearing desired form from slot {}.",
					coopActor->GetName(), boundObj->GetName(), i
				);
				desiredForms[i] = nullptr;
				desiredExtraDataLists[i] = nullptr;
			}

			DBG
			(
				"{}: {} has worn LH/RH data: {}, {} ({:p}, {:p}).",
				coopActor->GetName(),
				boundObj->GetName(),
				(bool)wornDataLH,
				(bool)wornDataRH,
				fmt::ptr(wornDataLH),
				fmt::ptr(wornDataRH)
			);

			// Check if still marked as worn and in the player's inventory,
			// which may've changed outside of co-op.
			// If the item no longer exists in the player's inventory,
			// such as if it were taken by P1 while P2 is a follower,
			// remove from the inventory chest to keep things in sync 
			// and prevent duplicating said item when re-summoned.
			// Also remove from desired forms list. No cheese, please.
			bool noLongerInPlayerInventory = 
			(
				wornDataLH && 
				!Util::GetWornRankExtraDataList(coopActor.get(), boundObj, true)
			);
			if (noLongerInPlayerInventory)
			{
				const auto count = wornDataLH->GetCount();
				// Must have at least 1 in the chest.
				if (count > 0)
				{
					DBG
					(
						"{}: {} is marked as worn LH in the inventory chest "
						"but is no longer in the player's inventory. "
						"Removing x{} from chest and clearing desired form from slot {}.",
						coopActor->GetName(), boundObj->GetName(), count, i
					);
					// Remove 1 because the player inventory will only have at most 1 
					// of this particular item with this extra data list.
					// Do not want to remove all of the corresponding item from the chest.
					inventoryChest->RemoveItem
					(
						boundObj, 
						1, 
						RE::ITEM_REMOVE_REASON::kRemove,
						wornDataLH,
						nullptr
					);
				}
				else
				{
					DBG
					(
						"{}: {}, worn LH, has a count less than 1 ({}), not removing from chest.",
						coopActor->GetName(), boundObj->GetName(), count
					);
				}

				desiredForms[i] = nullptr;
				desiredExtraDataLists[i] = nullptr;
			}

			// IMPORTANT:
			// Do not remove the same worn data twice. 
			// Will cause a delayed crash when loading an older save.
			noLongerInPlayerInventory = 
			(
				wornDataRH && 
				wornDataRH != wornDataLH &&
				!Util::GetWornRankExtraDataList(coopActor.get(), boundObj, false)
			);
			if (noLongerInPlayerInventory)
			{
				const auto count = wornDataRH->GetCount();
				// Must have at least 1 in the chest.
				if (count > 0)
				{
					DBG
					(
						"{}: {} is marked as worn RH in the inventory chest "
						"but is no longer in the player's inventory. "
						"Removing x{} from chest and clearing desired form from slot {}.",
						coopActor->GetName(), boundObj->GetName(), count, i
					);
					// Remove 1 because the player inventory will only have at most 1 
					// of this particular item with this extra data list.
					// Do not want to remove all of the corresponding item from the chest.
					inventoryChest->RemoveItem
					(
						boundObj, 
						1, 
						RE::ITEM_REMOVE_REASON::kRemove,
						wornDataRH,
						nullptr
					);
				}
				else
				{
					DBG
					(
						"{}: {}, worn RH, has a count less than 1 ({}), not removing from chest.",
						coopActor->GetName(), boundObj->GetName(), count
					);
				}
			
				desiredForms[i] = nullptr;
				desiredExtraDataLists[i] = nullptr;
			}
		}
		
		auto playerInvChanges = coopActor->GetInventoryChanges();
		if (!playerInvChanges || !playerInvChanges->entryList)
		{
			return;
		}

		// Ensure the inventory chest has at least all the same items as the player's inventory.
		// Chest item set is a superset of the player's inventory item set.
		for (const auto entry : *playerInvChanges->entryList)
		{
			if (!entry)
			{
				continue;
			}

			auto boundObj = entry->object;
			if (!boundObj)
			{
				continue;
			}

			if (Util::GetInventoryEntryDataForObject
				(
					inventoryChest.get(), boundObj, nullptr
				))
			{
				if (entry->countDelta > 0)
				{
					DBG
					(
						"Removing x{} {} from {}.", 
						entry->countDelta,
						boundObj->GetName(),
						coopActor->GetName()
					);
					coopActor->RemoveItem
					(
						boundObj, 
						entry->countDelta,
						RE::ITEM_REMOVE_REASON::kRemove, 
						nullptr,
						nullptr
					);
				}
			}
			else
			{
				DBG
				(
					"Removing x{} {} from {} to the inventory chest.", 
					entry->countDelta,
					boundObj->GetName(),
					coopActor->GetName()
				);
				Util::MoveAllOfItem
				(
					coopActor.get(), 
					inventoryChest.get(), 
					boundObj, 
					false,
					entry->extraLists, 
					entry->countDelta
				);
			}
		}

		// Ensure the inventory chest has at least all the same items as the player's inventory.
		// Chest item set is a superset of the player's inventory item set.
		/*auto inventory = coopActor->GetInventory();
		auto chestInventory = inventoryChest->GetInventory();
		for (const auto& [boundObj, entry] : inventory)
		{
			if (chestInventory.find(boundObj) == chestInventory.end())
			{
				DBG
				(
					"Removing x{} {} from {} to the inventory chest.", 
					entry.first,
					boundObj->GetName(),
					coopActor->GetName()
				);
				Util::MoveAllOfItem
				(
					coopActor.get(), 
					inventoryChest.get(), 
					boundObj, 
					false,
					entry.second->extraLists, 
					entry.first
				);
			}
			else
			{
				DBG
				(
					"Removing x{} {} from {}.", 
					entry.first,
					boundObj->GetName(),
					coopActor->GetName()
				);
				coopActor->RemoveItem
				(
					boundObj, 
					entry.first,
					RE::ITEM_REMOVE_REASON::kRemove, 
					nullptr,
					nullptr
				);
			}
		}*/
	}

	void EquipManager::ReEquipAll(bool a_refreshBeforeEquipping, bool a_resetInventoryFirst)
	{
		// Re-equip all forms for this player, optionally refreshing the cached equipped state 
		// or resetting the companion player's inventory beforehand.

		DBG("{}.", coopActor->GetName());

		// Refresh all equipped forms before re-equipping.
		if (a_refreshBeforeEquipping)
		{
			RefreshEquipState(RefreshSlots::kAll);
		}

		if (!p->isPlayer1 && a_resetInventoryFirst)
		{
			// Remove all items from the player's inventory first.
			// Using this as a failsafe to remove any corrupted inventory items
			// before re-equipping all the player's desired items.
			// Signal ResetInventory() hook to allow request.
			skipEquipProcessing = true;
			coopActor->ResetInventory(false);
			skipEquipProcessing = false;
		}

		RE::TESForm* item{ nullptr };
		for (auto i = 0; i < desiredForms.size(); ++i)
		{
			item = desiredForms[i];
			DBG("{}: {} at index {}.", 
				coopActor->GetName(), item ? item->GetName() : "NONE", i);
			// Do not include items without a loaded name,
			// such as the "SkinNaked" armor. 
			if (!item || strlen(item->GetName()) == 0)
			{
				continue;
			}

			EquipIndex currentIndex = EquipIndex::kTotal;
			if (i < !EquipIndex::kWeapMagTotal)
			{
				currentIndex = static_cast<EquipIndex>(i);
			}

			// Do not equip two handed weapons/spells twice,
			// so skip over the RH item if it is the same 2H item
			// as the earlier-equipped LH item.
			if (currentIndex == EquipIndex::kRightHand)
			{
				auto lhObj = desiredForms[!EquipIndex::kLeftHand];
				if (lhObj == item && 
					item->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot)
				{
					continue;
				}
			}
			else if (currentIndex == EquipIndex::kQuickSlotItem)
			{
				DBG("{}: Quickslot item: {}.", coopActor->GetName(), item->GetName());
				quickSlotItem = equippedForms[i] = item;
				continue;
			}
			else if (currentIndex == EquipIndex::kQuickSlotSpell)
			{
				auto asSpell = item ? item->As<RE::SpellItem>() : nullptr;
				if (!asSpell)
				{
					continue;
				}
				
				DBG
				(
					"{}: Quickslot spell: {}.", coopActor->GetName(), item->GetName()
				);
				equippedForms[i] = item;
				quickSlotSpell = asSpell;
				continue;
			}

			// Equip the cached item based on type.
			switch (*item->formType)
			{
			case RE::FormType::Ammo:
			{
				auto exDataList = 
				(
					p->isPlayer1 ? 
					Util::GetEquippedExtraData(coopActor.get(), item, false) : 
					Util::GetWornRankExtraDataList
					(
						inventoryChest.get(), 
						item->As<RE::TESBoundObject>(), 
						false
					)
				);
				DBG("{}: Ammo: {} ({:p}).", 
					coopActor->GetName(), item->GetName(), fmt::ptr(exDataList));
				EquipAmmo(item, exDataList);

				break;
			}
			case RE::FormType::Armature:
			case RE::FormType::Armor:
			{
				auto exDataList = 
				(
					p->isPlayer1 ? 
					Util::GetEquippedExtraData(coopActor.get(), item, false) : 
					Util::GetWornRankExtraDataList
					(
						inventoryChest.get(), 
						item->As<RE::TESBoundObject>(), 
						item->As<RE::TESObjectARMO>() && 
						item->As<RE::TESObjectARMO>()->equipSlot == glob.leftHandEquipSlot
					)
				);
				DBG("{}: Armor: {} ({:p}).", 
					coopActor->GetName(), item->GetName(), fmt::ptr(exDataList));
				EquipArmor(item, exDataList);

				break;
			}
			case RE::FormType::Shout:
			{
				DBG("{}: Shout: {}.", coopActor->GetName(), item->GetName());
				EquipShout(item);
				break;
			}
			case RE::FormType::Spell:
			{
				// Quick slot spell is not equipped by the game,
				// as it is cast directly on demand. Break early.
				if (i == !EquipIndex::kQuickSlotSpell)
				{
					break;
				}

				if (i != !EquipIndex::kVoice)
				{
					auto spell = item->As<RE::SpellItem>();
					auto equipSlot = glob.eitherHandEquipSlot;
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
					
					DBG("{}: Hand spell: {}.", coopActor->GetName(), item->GetName());
					// Directly equip, if P1.
					if (p->isPlayer1)
					{
						EquipSpell
						(
							spell, 
							i == !EquipIndex::kLeftHand ? 
							EquipIndex::kLeftHand : 
							EquipIndex::kRightHand, 
							equipSlot
						);
					}
					else
					{
						// Copy to placeholder spell before equipping.
						if (equipSlot == glob.bothHandsEquipSlot)
						{
							spell = 
							(
								copiedMagic[!PlaceholderMagicIndex::k2H] ? 
								copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
								nullptr
							);
							if (spell)
							{
								EquipSpell
								(
									CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::k2H), 
									EquipIndex::kRightHand, 
									equipSlot
								);
							}
						}
						else
						{
							if (i == !EquipIndex::kRightHand)
							{
								spell = 
								(
									copiedMagic[!PlaceholderMagicIndex::kRH] ? 
									copiedMagic[!PlaceholderMagicIndex::kRH]->As<RE::SpellItem>() : 
									nullptr
								);
							}
							else
							{
								spell = 
								(
									copiedMagic[!PlaceholderMagicIndex::kLH] ? 
									copiedMagic[!PlaceholderMagicIndex::kLH]->As<RE::SpellItem>() : 
									nullptr
								);
							}

							EquipIndex index = 
							(
								i == !EquipIndex::kLeftHand ? 
								EquipIndex::kLeftHand : 
								EquipIndex::kRightHand
							);
							if (spell)
							{
								EquipSpell
								(
									CopyToPlaceholderSpell
									(
										spell, 
										index == EquipIndex::kRightHand ? 
										PlaceholderMagicIndex::kRH : 
										PlaceholderMagicIndex::kLH
									), 
									index, 
									equipSlot
								);
							}
						}
					}
				}
				else
				{
					DBG("{}: Voice spell: {}.", coopActor->GetName(), item->GetName());
					auto spell = item->As<RE::SpellItem>();
					auto equipSlot = glob.voiceEquipSlot;
					EquipSpell
					(
						p->isPlayer1 ? 
						spell :
						CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::kVoice),
						EquipIndex::kVoice, 
						equipSlot
					);

				}

				break;
			}
			case RE::FormType::Weapon:
			{
				auto lhObj = coopActor->GetEquippedObject(true);
				auto rhObj = coopActor->GetEquippedObject(false);
				// Do not equip 2H weapons twice.
				if ((i == !EquipIndex::kLeftHand && item != lhObj) || 
					(i == !EquipIndex::kRightHand && item != rhObj))
				{
					auto equipSlot = glob.eitherHandEquipSlot;
					if (item->As<RE::TESObjectWEAP>()->equipSlot == glob.bothHandsEquipSlot)
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
					
					auto exDataList = 
					(
						p->isPlayer1 ? 
						Util::GetEquippedExtraData
						(
							coopActor.get(), 
							item, 
							i == !EquipIndex::kLeftHand && equipSlot != glob.bothHandsEquipSlot
						) : 
						Util::GetWornRankExtraDataList
						(
							inventoryChest.get(),
							item->As<RE::TESBoundObject>(),
							i == !EquipIndex::kLeftHand && equipSlot != glob.bothHandsEquipSlot
						)
					);
					DBG("{}: Weapon: {} ({:p}).", 
						coopActor->GetName(), item->GetName(), fmt::ptr(exDataList));
					EquipForm
					(
						item, 
						i == !EquipIndex::kLeftHand ? 
						EquipIndex::kLeftHand : 
						EquipIndex::kRightHand, 
						exDataList, 
						1, 
						equipSlot
					);

					// If the setting is enabled, auto-equip highest damage/count ammo 
					// based on ranged weapon type. 
					if (Settings::uAmmoAutoEquipMode != !AmmoAutoEquipMode::kNone) 
					{
						auto weap = item->As<RE::TESObjectWEAP>(); 
						if (weap &&
							weap->IsRanged() && 
							!weap->IsStaff() && !desiredForms[!EquipIndex::kAmmo])
						{
							auto ammoAndCount = 
							(
								Settings::uAmmoAutoEquipMode == 
								!AmmoAutoEquipMode::kHighestDamage ? 
								Util::GetHighestDamageAmmo(coopActor.get(), weap->IsBow()) :
								Util::GetHighestCountAmmo(coopActor.get(), weap->IsBow())
							);
							if (ammoAndCount.first)
							{
								// Valid ammo.
								auto exDataList = 
								(
									p->isPlayer1 ? 
									Util::GetEquippedExtraData
									(
										coopActor.get(), ammoAndCount.first, false
									) : 
									Util::GetWornRankExtraDataList
									(
										inventoryChest.get(), ammoAndCount.first, false
									)
								);
								DBG
								(
									"{}: Auto-equip ammo: {} ({:p}).", 
									coopActor->GetName(),
									ammoAndCount.first->GetName(),
									fmt::ptr(exDataList)
								);
								if (ammoAndCount.first)
								{
									EquipAmmo(ammoAndCount.first, exDataList);
								}
							}
						}
					}
				}

				break;
			}
			default:
			{
				DBG("{}: Not equipping {}.", coopActor->GetName(), item->GetName());
				break;
			}
			}
		}
	}

	void EquipManager::ReEquipHandForm(bool a_rhSlot)
	{
		// Re-equip desired forms in this player's hands.


		// Interrupts Vampire Lord levitation, 
		// and Werewolves have no equipped items, so return here.
		if (p->isTransformed)
		{
			return;
		}

		auto handForm = desiredForms
		[
			a_rhSlot ? !EquipIndex::kRightHand : !EquipIndex::kLeftHand
		];
		DBG
		(
			"{}: {}: {}.", 
			coopActor->GetName(), 
			a_rhSlot ? "RH" : "LH",
			handForm ? handForm->GetName() : "NONE"
		);
		if (!handForm)
		{
			// Still unequip to clear out hand slot, since the desired form is none.
			UnequipHandForms(a_rhSlot ? glob.rightHandEquipSlot : glob.leftHandEquipSlot);
			return;
		}
		
		const auto equipIndex = a_rhSlot ? EquipIndex::kRightHand : EquipIndex::kLeftHand;
		auto equipSlot = GetEquipSlotForForm(handForm, equipIndex);
		// If a 2H form, unequip and re-equip from both hands instead.
		if (equipSlot == glob.bothHandsEquipSlot)
		{
			ReEquipHandForms();
			return;
		}
		
		skipEquipProcessing = true;
		// Save extra data list to re-apply worn ex data to affter unequip.
		auto exDataList = 
		(
			p->isPlayer1 ? 
			Util::GetEquippedExtraData
			(
				coopActor.get(), handForm, !a_rhSlot
			) :
			Util::GetWornRankExtraDataList
			(
				inventoryChest.get(), 
				handForm->As<RE::TESBoundObject>(), 
				!a_rhSlot
			)
		);
		// Unequip to clear out hand slot before re-equipping.
		UnequipHandForms(a_rhSlot ? glob.rightHandEquipSlot : glob.leftHandEquipSlot);
		// Re-equip.
		if (auto spell = handForm->As<RE::SpellItem>(); spell)
		{
			if (p->isPlayer1)
			{
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
				{
					aem->EquipSpell(coopActor.get(), spell, equipSlot);
				}
			}
			else
			{
				if (spell == 
					(
						a_rhSlot ? 
						placeholderMagic[!PlaceholderMagicIndex::kRH] : 
						placeholderMagic[!PlaceholderMagicIndex::kLH]
							
					))
				{
					EquipSpell(spell, equipIndex, equipSlot);
				}
				else
				{
					// Copy to placeholder spell, if needed.
					EquipSpell
					(
						CopyToPlaceholderSpell
						(
							spell, 
							a_rhSlot ? PlaceholderMagicIndex::kRH : PlaceholderMagicIndex::kLH
						), 
						equipIndex,
						equipSlot
					);
				}
			}
		}
		else if (auto asArmor = handForm->As<RE::TESObjectARMO>(); asArmor)
		{
			// Is armor.
			if (p->isPlayer1) 
			{
				Util::EquipObject(coopActor.get(), handForm->As<RE::TESBoundObject>(), exDataList);
			}
			else
			{
				EquipArmor(handForm, exDataList);
			}
		}
		else
		{
			// Anything else gets equipped normally to the hand slot.
			equipSlot = GetEquipSlotForForm(handForm, equipIndex);
			if (p->isPlayer1)
			{
				Util::EquipObject
				(
					coopActor.get(), 
					handForm->As<RE::TESBoundObject>(),
					exDataList, 
					1, 
					equipSlot
				);
			}
			else
			{
				EquipForm(handForm, equipIndex, exDataList, 1, equipSlot);
			}
		}

		skipEquipProcessing = false;
	}

	void EquipManager::ReEquipHandForms()
	{
		// Re-equip desired forms in this player's hands.

		DBG("{}.", coopActor->GetName());

		// Interrupts Vampire Lord levitation, 
		// and Werewolves have no equipped items, so return here.
		if (p->isTransformed)
		{
			return;
		}
		
		skipEquipProcessing = true;
		auto lhForm = 
		(
			p->isPlayer1 ? 
			coopActor->GetEquippedObject(true) :
			desiredForms[!EquipIndex::kLeftHand]
		);
		auto rhForm = 
		(
			p->isPlayer1 ? 
			coopActor->GetEquippedObject(false) : 
			desiredForms[!EquipIndex::kRightHand]
		);
		auto equipSlot = glob.eitherHandEquipSlot;
		auto lhEquipType = lhForm ? lhForm->As<RE::BGSEquipType>() : nullptr;
		auto rhEquipType = rhForm ? rhForm->As<RE::BGSEquipType>() : nullptr;

		// Save stored extra data lists to re-add after the worn ex data is removed 
		// from the corresponding chest entry lists on unequip.
		RE::ExtraDataList* lhExtraDataList = nullptr;
		if (p->isPlayer1)
		{
			lhExtraDataList = Util::GetEquippedExtraData(coopActor.get(), lhForm, true);
			if (lhExtraDataList)
			{
				auto exWorn = lhExtraDataList->GetByType<RE::ExtraWorn>();
				if (exWorn)
				{
					lhExtraDataList->Remove(RE::ExtraDataType::kWorn, exWorn);
				}

				auto exWornLeft = lhExtraDataList->GetByType<RE::ExtraWornLeft>();
				if (exWornLeft)
				{
					lhExtraDataList->Remove(RE::ExtraDataType::kWornLeft, exWornLeft);
				}
			}
		}
		else
		{
			lhExtraDataList = 
			(
				lhForm ? 
				Util::GetWornRankExtraDataList
				(
					inventoryChest.get(), lhForm->As<RE::TESBoundObject>(), true
				) : 
				nullptr
			);
		}
		
		RE::ExtraDataList* rhExtraDataList = nullptr;
		if (p->isPlayer1)
		{
			rhExtraDataList = Util::GetEquippedExtraData(coopActor.get(), rhForm, false);
			if (rhExtraDataList)
			{
				auto exWorn = rhExtraDataList->GetByType<RE::ExtraWorn>();
				if (exWorn)
				{
					rhExtraDataList->Remove(RE::ExtraDataType::kWorn, exWorn);
				}

				auto exWornLeft = rhExtraDataList->GetByType<RE::ExtraWornLeft>();
				if (exWornLeft)
				{
					rhExtraDataList->Remove(RE::ExtraDataType::kWornLeft, exWornLeft);
				}
			}
		}
		else
		{
			rhExtraDataList = 
			(
				rhForm ? 
				Util::GetWornRankExtraDataList
				(
					inventoryChest.get(), rhForm->As<RE::TESBoundObject>(), false
				) : 
				nullptr
			);
		}

		// CHANGE TO DEBUG
		DBG
		(
			"{}. Forms to re-equip: {}, {} ({:p}, {:p})", 
			coopActor->GetName(),
			lhForm ? lhForm->GetName() : "NONE",
			rhForm ? rhForm->GetName() : "NONE",
			fmt::ptr(lhExtraDataList),
			fmt::ptr(rhExtraDataList)
		);

		// Unequip to clear out hand slots before re-equipping.
		UnequipHandForms(glob.bothHandsEquipSlot);
		// Equip RH and then LH forms.
		if (rhForm)
		{
			if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
			{
				equipSlot = GetEquipSlotForForm(rhForm, EquipIndex::kRightHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
					{
						aem->EquipSpell(coopActor.get(), rhSpell, equipSlot);
					}
				}
				else
				{
					if (rhSpell == placeholderMagic[!PlaceholderMagicIndex::kRH] ||
						rhSpell == placeholderMagic[!PlaceholderMagicIndex::k2H])
					{
						EquipSpell(rhSpell, EquipIndex::kRightHand, equipSlot);
					}
					else
					{
						// Copy to placeholder spell, if needed.
						if (equipSlot == glob.bothHandsEquipSlot)
						{
							EquipSpell
							(
								CopyToPlaceholderSpell(rhSpell, PlaceholderMagicIndex::k2H), 
								EquipIndex::kRightHand, 
								equipSlot
							);
						}
						else
						{
							EquipSpell
							(
								CopyToPlaceholderSpell(rhSpell, PlaceholderMagicIndex::kRH), 
								EquipIndex::kRightHand,
								equipSlot
							);
						}
					}
				}
			}
			else if (auto asBipedObjForm = rhForm->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				// Is armor.
				if (p->isPlayer1) 
				{
					Util::EquipObject
					(
						coopActor.get(), rhForm->As<RE::TESBoundObject>(), rhExtraDataList
					);
				}
				else
				{
					EquipArmor(rhForm, rhExtraDataList);
				}
			}
			else
			{
				// Anything else gets equipped normally to the RH slot.
				equipSlot = GetEquipSlotForForm(rhForm, EquipIndex::kRightHand);
				if (p->isPlayer1)
				{
					DBG
					(
						"RH: {}, {:p}, slot {}. Has worn/worn left data: {}, {}",
						rhForm ? rhForm->GetName() : "NONE", 
						fmt::ptr(rhExtraDataList),
						Util::GetEditorID(equipSlot),
						rhExtraDataList && rhExtraDataList->HasType<RE::ExtraWorn>(),
						rhExtraDataList && rhExtraDataList->HasType<RE::ExtraWornLeft>()
					);
					Util::EquipObject
					(
						coopActor.get(), 
						rhForm->As<RE::TESBoundObject>(),
						rhExtraDataList, 
						1, 
						equipSlot
					);
				}
				else
				{
					EquipForm(rhForm, EquipIndex::kRightHand, rhExtraDataList, 1, equipSlot);
				}
			}
		}

		// No need to equip LH form if it is a 2H form
		// since it would've been equipped as the RH form above.
		if ((lhForm) && (!lhEquipType || lhEquipType->equipSlot != glob.bothHandsEquipSlot))
		{
			if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell)
			{
				equipSlot = GetEquipSlotForForm(lhForm, EquipIndex::kLeftHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipSpell(coopActor.get(), lhSpell, equipSlot);
					}
				}
				else
				{
					if (lhSpell == placeholderMagic[!PlaceholderMagicIndex::kLH] || 
						lhSpell == placeholderMagic[!PlaceholderMagicIndex::k2H])
					{
						EquipSpell(lhSpell, EquipIndex::kLeftHand, equipSlot);
					}
					else
					{
						// Copy to placeholder spell, if needed.
						if (equipSlot == glob.bothHandsEquipSlot)
						{
							EquipSpell
							(
								CopyToPlaceholderSpell(lhSpell, PlaceholderMagicIndex::k2H), 
								EquipIndex::kLeftHand, 
								equipSlot
							);
						}
						else
						{
							EquipSpell
							(
								CopyToPlaceholderSpell(lhSpell, PlaceholderMagicIndex::kLH), 
								EquipIndex::kLeftHand, 
								equipSlot
							);
						}
					}
				}
			}
			else if (auto asBipedObjForm = lhForm->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				// Is armor.
				if (p->isPlayer1)
				{
					Util::EquipObject
					(
						coopActor.get(), lhForm->As<RE::TESBoundObject>(), lhExtraDataList
					);
				}
				else
				{
					EquipArmor(lhForm, lhExtraDataList);
				}
			}
			else
			{
				// Everything else gets equipped normally to the LH slot.
				equipSlot = GetEquipSlotForForm(lhForm, EquipIndex::kLeftHand);
				if (p->isPlayer1)
				{
					DBG
					(
						"LH: {}, {:p}, slot {}. Has worn/worn left data: {}, {}",
						lhForm ? lhForm->GetName() : "NONE", 
						fmt::ptr(lhExtraDataList),
						Util::GetEditorID(equipSlot),
						lhExtraDataList && lhExtraDataList->HasType<RE::ExtraWorn>(),
						lhExtraDataList && lhExtraDataList->HasType<RE::ExtraWornLeft>()
					);
					Util::EquipObject
					(
						coopActor.get(),
						lhForm->As<RE::TESBoundObject>(), 
						lhExtraDataList, 
						1, 
						equipSlot
					);
				}
				else
				{
					EquipForm(lhForm, EquipIndex::kLeftHand, lhExtraDataList, 1, equipSlot);
				}
			}
		}

		// IMPORTANT:
		// Resetting while on horseback causes horse warp glitch upon resumption.
		// Re-loads weapon BIPED_OBJECTS, so if the weapon models themselves are missing,
		// this should fix it.
		if (!coopActor->IsOnMount())
		{
			if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface)
			{
				taskInterface->AddTask
				(
					[this]()
					{
						DBG("{}: Reset3D.", coopActor->GetName());
						coopActor->DoReset3D(true);
					}
				);
			}
		}

		skipEquipProcessing = false;
	}

	void EquipManager::ReEquipVoiceForm()
	{
		// Re-equip the player's desired voice magic form.

		DBG("{}.", coopActor->GetName());
		UnequipFormAtIndex(EquipIndex::kVoice);

		auto toEquip = desiredForms[!EquipIndex::kVoice]; 
		if (!toEquip) 
		{
			return;
		}

		if (toEquip->As<RE::TESShout>())
		{
			EquipShout(toEquip);
		}
		else
		{
			EquipSpell(toEquip, EquipIndex::kVoice);
		}
	}

	void EquipManager::RestoreP1Favorites(bool&& a_onlyMagicFavorites)
	{
		// Restore P1's previously saved favorited items/spells
		// after removing any companion player's favorites that are not in common.

		// Don't need to restore P1's favorites to themselves.
		if (p->isPlayer1)
		{
			return;
		}

		DBG
		(
			"{}: Only magic favorites: {}.", coopActor->GetName(), a_onlyMagicFavorites
		);

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		// Update the companion player's cached magic favorites,
		// which may have changed since import,
		// before restoring P1's favorites below.
		UpdateFavoritedFormsLists(false);

		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (!magicFavorites)
		{
			return;
		}

		// Clear out hotkeyed forms, since they linger behind even after the form is unfavorited.
		// P1's hotkeys and magic favorites were saved on import already 
		// and will be restored below.
		for (auto i = 0; i < magicFavorites->hotkeys.size(); ++i)
		{
			magicFavorites->hotkeys[i] = nullptr;
		}

		const auto& coopP1 = glob.coopPlayers[0];
		// RemoveFavorite() modifies and shifts elements of the magical favorites list.
		// Remove the first element until the list is empty.
		uint32_t removalIndex = 0;
		while (removalIndex < magicFavorites->spells.size())
		{
			auto magForm = magicFavorites->spells[removalIndex];
			// Skip over empty entries and start removing from the next index.
			if (!magForm) 
			{
				++removalIndex;
				continue;
			}

			magicFavorites->RemoveFavorite(magForm);
			DBG
			(
				"{}: Remove P1 favorited magic {} at index {}. List size: {}.", 
				coopActor->GetName(),
				magForm ? magForm->GetName() : "NONE",
				removalIndex,
				magicFavorites->spells.size()
			);
		}

		// Clear out the list as well to be safe.
		magicFavorites->spells.clear();

		if (!a_onlyMagicFavorites)
		{
			// WARNING:
			// Nested loop hell below. Comes with the extra data/inventory entry territory.
			// IMPORTANT:
			// Always use a newly populated inventory map here because for some reason 
			// not all P1's items will show up in the inventory changes.
			// Possibly some items move to P1's underlying container while in the Favorites Menu.
			auto p1Inv = p1->GetInventory();

			auto& p1FavoritesMap = glob.p1FavoritedFormsMap;
			for (const auto& [form, extraDataListSet] : p1FavoritesMap)
			{
				if (form)
				{
					DBG("SET: {}.", form->GetName());
				}
			}

			std::unordered_map<RE::ExtraDataList*, int8_t> chestExDataToRefavorite{ };
			for (const auto& [boundObj, countInvEntryPair] : p1Inv)
			{
				if (!boundObj || !countInvEntryPair.second || countInvEntryPair.first <= 0)
				{
					continue;
				}

				// Remove all favorites before restoring P1's favorites.
				// The only favorited forms should be the companion player's imported favorites,
				// since we removed all P1's favorites on import.
				if (countInvEntryPair.second->extraLists)
				{
					DBG("ITEM: {}.", countInvEntryPair.second->object->GetName());
					for (auto exDataList : *countInvEntryPair.second->extraLists)
					{
						if (!exDataList)
						{
							continue;
						}

						auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>(); 
						if (!exHotkey)
						{
							continue;
						}

						const auto iter = p1FavoritesMap.find(boundObj);
						bool notFavoritedByP1 = iter == p1FavoritesMap.end();
						if (!notFavoritedByP1)
						{
							// Check if the P1 favorite exData list matches one of the lists
							// in the inventory and if so this item should remain as favorited 
							// by P1 and not move to the inventory chest.
							notFavoritedByP1 = true;
							for (const auto& [exDataList2, hotkey] : iter->second)
							{
								if (exDataList2 == exDataList)
								{
									notFavoritedByP1 = false;
									break;
								}
							}
						}

						// Have to remove the item and re-favorite it since removal, 
						// even if it remains present in the chest,
						// still removes the hotkey extra data. Gahd.
						if (notFavoritedByP1)
						{
							DBG
							(
								"Removing favorited item {} with list {:p} "
								"and hotkey {} from P1.",
								countInvEntryPair.second->object->GetName(),
								fmt::ptr(exDataList),
								*exHotkey->hotkey
							);
							p1->RemoveItem
							(
								countInvEntryPair.second->object,
								1, 
								RE::ITEM_REMOVE_REASON::kRemove,
								exDataList, 
								nullptr
							);
							//chestExDataToRefavorite.insert({ exDataList, !*exHotkey->hotkey });
						}
						else
						{
							DBG
							(
								"NOT removing favorited item {} with list {:p} "
								"and hotkey {} from P1 to the inventory chest.",
								countInvEntryPair.second->object->GetName(),
								fmt::ptr(exDataList),
								*exHotkey->hotkey
							);
						}
					}
				}

				const auto iter = p1FavoritesMap.find(countInvEntryPair.second->object);
				if (iter == p1FavoritesMap.end())
				{
					continue;
				}

				if (!iter->first || iter->second.empty())
				{
					continue;
				}

				// Restore saved favorite statuses and hotkeys for P1 forms.
				for (const auto& [exDataList, hotkey] : iter->second)
				{
					if (!exDataList)
					{
						continue;
					}
						
					DBG
					(
						"P1's inventory had item {} exDataList {:p} "
						"with favorites status to restore.",
						iter->first->GetName(),
						fmt::ptr(exDataList)
					);
					if (!exDataList->GetByType<RE::ExtraHotkey>())
					{
						Util::NativeFunctions::Favorite
						(
							p1->GetInventoryChanges(), 
							countInvEntryPair.second.get(), 
							exDataList
						);
					}

					if (hotkey != !RE::ExtraHotkey::Hotkey::kUnbound)
					{
						DBG
						(
							"P1's inventory had item {} exDataList {:p} "
							"with hotkey {} to restore.",
							iter->first->GetName(),
							fmt::ptr(exDataList),
							hotkey
						);
						Util::ChangeFormHotkeyStatus
						(
							p1, iter->first, hotkey, exDataList
						);
					}
				}
			}

			// REMOVE when done debugging.
			auto chestInv = inventoryChest->GetInventory();
			for (const auto& [boundObj, countInvEntryPair] : chestInv)
			{
				if (!boundObj ||
					!countInvEntryPair.second ||
					!countInvEntryPair.second->extraLists)
				{
					continue;
				}	

				for (auto exDataList : *countInvEntryPair.second->extraLists)
				{
					if (!exDataList)
					{
						continue;
					}

					// Favorited if hotkey data exists.
					auto exHotkey = exDataList->GetByType<RE::ExtraHotkey>();
					if (!exHotkey)
					{
						continue;
					}

					DBG
					(
						"{}: {} is favorited and has hotkey {} "
						"on list {:p} from inventory chest.",
						coopActor->GetName(), 
						boundObj->GetName(), 
						*exHotkey->hotkey,
						fmt::ptr(exDataList)
					);
				}
			}
		}

		// Re-favorite all of P1's cached magical favorited forms and restore hotkeys.
		const auto& p1Favorites = coopP1->em->favoritedForms;
		for (auto form : p1Favorites)
		{
			if (!form)
			{
				continue;
			}

			// Skip hotkey restoration for physical forms which were already handled above.
			if (!a_onlyMagicFavorites && form->IsNot(RE::FormType::Spell, RE::FormType::Shout))
			{
				continue;
			}

			if (form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				DBG
				(
					"{}: Restoring favorited magic {} for P1.", 
					coopActor->GetName(), form->GetName()
				);
				magicFavorites->SetFavorite(form);
			}

			const auto iter = coopP1->em->hotkeyedFormsToSlotsSetMap.find(form->formID);
			if (iter != coopP1->em->hotkeyedFormsToSlotsSetMap.end())
			{
				// Should not happen, but remove the hotkey if it is not in the set.
				if (iter->second.empty())
				{
					DBG
					(
						"{}: Removing hotkey for {}.",
						coopActor->GetName(), 
						form->GetName()
					);
					Util::ChangeFormHotkeyStatus(p1, form, -1);
				}
				else
				{
					for (const auto& hotkeyIndex : iter->second)
					{
						DBG
						(
							"{}: Reapplying P1 hotkey {} for {}.",
							coopActor->GetName(),
							hotkeyIndex == -1 ? -1 : hotkeyIndex + 1, 
							form->GetName()
						);
						Util::ChangeFormHotkeyStatus(p1, form, hotkeyIndex);
					}
				}
			}
		}
	}

	void EquipManager::RefreshEquipState
	(
		const RefreshSlots& a_slots, RE::TESForm* a_formEquipped, bool a_isEquipped
	)
	{
		// Refresh all equipped gear for this player in the given slots.
		// For specific equip/unequip events, the form of interest is given, 
		// as well as whether or not it is now equipped.

		// CHANGE TO DEBUG
		DBG
		(
			"{}: slots to refresh: {}, form: {}, is equipped: {}. "
			"Manager states: P: {}, PAM: {}, EM: {}, MM: {}, TM: {}", 
			coopActor->GetName(),
			a_slots,
			a_formEquipped ? a_formEquipped->GetName() : "NONE",
			a_isEquipped,
			p->currentState,
			p->pam->currentState,
			currentState,
			p->mm->currentState,
			p->tm->currentState
		);

		if (!glob.allPlayersInit)
		{
			return;
		}

		{
			std::unique_lock<std::mutex> lock(equipStateMutex, std::try_to_lock);
			if (lock)
			{
				DBG
				(
					"{}: Lock obtained. (0x{:X})", 
					coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				// Clear out cached forms first.
				equippedForms.fill(nullptr);
				equippedFormFIDs.clear();
				
				if (a_slots == RefreshSlots::kWeapMag || a_slots == RefreshSlots::kAll)
				{
					// Get LH, RH objects, shout, and ammo.
					auto lhObj = coopActor->GetEquippedObject(true);
					auto rhObj = coopActor->GetEquippedObject(false);
					auto ammo = coopActor->GetCurrentAmmo();

					DBG
					(
						"{}: LH: {}, RH: {}",
						coopActor->GetName(),
						(lhObj) ? lhObj->GetName() : "NONE",
						(rhObj) ? rhObj->GetName() : "NONE"
					);
					DBG
					(
						"{}: Current ammo: {}.", 
						coopActor->GetName(), ammo ? ammo->GetName() : "NONE"
					);

					// Could be indicative of an equip state bug 
					// that causes choppy/mismatching attack animations and wonky hitboxes
					// (requires full player reset to fix).
					// Seems to occur when a 2H weapon is equipped to only one hand. 
					// Repro when reloading another save where the player had a 2H weapon equipped. 
					// The game does not equip it properly when the player is summoned.
					// Did not see this bug in Enderal SSE.
					bool onlyLHHas2HWeap = 
					(
						!rhObj &&
						lhObj && 
						lhObj->As<RE::TESObjectWEAP>() && 
						lhObj->As<RE::TESObjectWEAP>()->equipSlot == glob.bothHandsEquipSlot
					);
					bool onlyRHHas2HWeap = 
					(
						!lhObj && 
						rhObj && 
						rhObj->As<RE::TESObjectWEAP>() && 
						rhObj->As<RE::TESObjectWEAP>()->equipSlot == glob.bothHandsEquipSlot
					);
					// Notify the player and provide workaround steps.
					if (onlyLHHas2HWeap || onlyRHHas2HWeap)
					{
						ERR
						(
							"{}: 2H stuttering equip state bug "
							"is likely since the 2H weapon {} is only in the {}. " 
							"Fix the bug temporarily by using "
							"the Debug Menu's 'Reset Player' option "
							"or the 'DebugResetPlayer' bind.",
							coopActor->GetName(), 
							onlyLHHas2HWeap ? 
							lhObj->GetName() : 
							onlyRHHas2HWeap ? 
							rhObj->GetName() : 
							"ERROR",
							onlyLHHas2HWeap ? "LH" : onlyRHHas2HWeap ? "RH" : "ERROR"
						);
					}

					// Sometimes the call to GetCurrentAmmo() still returns the unequipped ammo 
					// that triggered this equip state refresh.
					// Clear out the cached current ammo, 
					// which will then properly correspond to the actual equip state
					// after the unequip fully completes.
					if (ammo && a_formEquipped == ammo && !a_isEquipped)
					{
						ammo = nullptr;
					}

					// And sometimes the call to GetCurrentAmmo() returns nullptr 
					// while this event fires upon equipping ammo.
					// Similarly update the cached current ammo 
					// to reflect the post-equip event state.
					if (!ammo && 
						a_formEquipped && 
						a_isEquipped &&
						a_formEquipped->As<RE::TESAmmo>())
					{
						ammo = a_formEquipped->As<RE::TESAmmo>();
					}

					// Initially set as previous desired voice form.
					voiceForm = 
					(
						!desiredForms.empty() ? 
						desiredForms[!EquipIndex::kVoice] :
						nullptr
					);
					if (a_formEquipped && a_isEquipped)
					{
						bool isShout = a_formEquipped->Is(RE::FormType::Shout);
						bool isSpell = a_formEquipped->Is(RE::FormType::Spell);
						bool isVoiceSpell = false;
						if (isSpell)
						{
							if (auto spell = a_formEquipped->As<RE::SpellItem>(); spell)
							{
								auto spellType = spell->GetSpellType();
								isVoiceSpell = 
								(
									spellType == RE::MagicSystem::SpellType::kVoicePower ||
									spellType == RE::MagicSystem::SpellType::kPower ||
									spellType == RE::MagicSystem::SpellType::kLesserPower
								);
							}
						}

						if (isShout || isVoiceSpell)
						{
							voiceForm = a_formEquipped;
						}
					}
					else if (a_formEquipped && !a_isEquipped && voiceForm == a_formEquipped)
					{
						voiceForm = nullptr;
					}

					// Update voice form to match equipped shout, 
					// if any, and set the highest shout variation spell.
					SetCurrentVoiceSpell();

					// Ensure each equipped hand spell 
					// holds a valid placeholder spell before caching.
					if (!p->isPlayer1)
					{
						bool is2HSpell = false;
						if (auto lhSpell = lhObj ? lhObj->As<RE::SpellItem>() : nullptr; lhSpell)
						{
							is2HSpell = lhSpell->equipSlot == glob.bothHandsEquipSlot;
							lhObj = CopyToPlaceholderSpell
							(
								lhSpell, 
								is2HSpell ? 
								PlaceholderMagicIndex::k2H : 
								PlaceholderMagicIndex::kLH
							);
						}

						if (auto rhSpell = rhObj ? rhObj->As<RE::SpellItem>() : nullptr; rhSpell)
						{
							is2HSpell = rhSpell->equipSlot == glob.bothHandsEquipSlot;
							rhObj = CopyToPlaceholderSpell
							(
								rhSpell, 
								is2HSpell ? 
								PlaceholderMagicIndex::k2H : 
								PlaceholderMagicIndex::kRH
							);
						}
					}

					DBG
					(
						"{}: Voice magic: {} (formType: 0x{:X}), Ammo: {}",
						coopActor->GetName(),
						voiceForm ? voiceForm->GetName() : "NONE",
						voiceForm ? *voiceForm->formType : RE::FormType::None,
						ammo ? ammo->GetName() : "NONE"
					);

					// Set weapon/magic slot forms.
					equippedForms[!EquipIndex::kLeftHand] = lhObj;
					equippedForms[!EquipIndex::kRightHand] = rhObj;
					equippedForms[!EquipIndex::kAmmo] = ammo;
					equippedForms[!EquipIndex::kVoice] = voiceForm;
					equippedForms[!EquipIndex::kQuickSlotItem] = quickSlotItem;
					equippedForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;
					// Keep in sync with equipped forms 
					// since these two forms are not actually equipped to a slot
					// and are cast immediately with the co-op actor's instant magic caster 
					// or consumed when the quick slot item bind is pressed.
					desiredForms[!EquipIndex::kQuickSlotItem] = quickSlotItem;
					desiredForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;

					DBG
					(
						"{}: Quick slot spell, item: {}, {}",
						coopActor->GetName(),
						(quickSlotSpell) ? quickSlotSpell->GetName() : "NONE",
						(quickSlotItem) ? quickSlotItem->GetName() : "NONE"
					);
				}

				if (a_slots == RefreshSlots::kArmor || a_slots == RefreshSlots::kAll)
				{
					// Clear cached armor ratings.
					armorRatings.first = armorRatings.second = 0.0f;
					// Get armor in each biped slot
					std::set<RE::FormID> equippedFormIDs;

					for (uint32_t i = !EquipIndex::kFirstBipedSlot; 
						 i <= !EquipIndex::kLastBipedSlot; 
						 ++i)
					{
						auto armorInSlot = 
						(
							coopActor->GetWornArmor
							(
								static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>
								(
									1 << (i - !EquipIndex::kFirstBipedSlot)
								)
							)
						);
						if (armorInSlot)
						{
							// NOTE: 
							// Leaving this for now just in case the bug re-surfaces.
							// Have to handle the odd case where the unequip event 
							// for a piece of armor fires, 
							// but the game hasn't finished unequipping it yet here.
							// Either the armor in this slot is not the equipped armor 
							// sent from the equip event, or the armor in this slot 
							// is the equip event armor and it is being equipped.
							if (armorInSlot != a_formEquipped || a_isEquipped)
							{
								// Do not double count armor that takes up multiple biped slots.
								if (!equippedFormIDs.contains(armorInSlot->formID))
								{
									if (armorInSlot->IsLightArmor())
									{
										armorRatings.first += armorInSlot->GetArmorRating();
									}
									else
									{
										armorRatings.second += armorInSlot->GetArmorRating();
									}
								}

								equippedForms[i] = armorInSlot;
								equippedFormIDs.insert(armorInSlot->formID);
							}
							else
							{
								// The sent form was unequipped 
								// but is still in the biped armor slot (yet to be removed), 
								// so remove from the equipped list.
								equippedForms[i] = nullptr;
							}
						}
						else
						{
							equippedForms[i] = nullptr;
						}
					}

					DBG
					(
						"{}: New armor ratings for light/heavy armor are: {}, {}",
						coopActor->GetName(), armorRatings.first, armorRatings.second
					);
				}

				// If P1, serialize desired list of equipped forms, 
				// since the game does not auto-equip forms on cell or inventory change for P1.
				// Find mismatches for companion players, and if any are found,
				// DO NOT serialize the desired forms list.
				bool mismatch = false;
				if (p->isPlayer1)
				{
					// Fists count as empty equipped form slots.
					// Also, do not save bound weapons/ammo.
					auto lhForm = equippedForms[!EquipIndex::kLeftHand];
					bool lhFormIsBound = 
					(
						lhForm && 
						lhForm->As<RE::TESObjectWEAP>() && 
						lhForm->As<RE::TESObjectWEAP>()->IsBound()
					);
					auto rhForm = equippedForms[!EquipIndex::kRightHand];
					bool rhFormIsBound = 
					(
						rhForm && 
						rhForm->As<RE::TESObjectWEAP>() && 
						rhForm->As<RE::TESObjectWEAP>()->IsBound()
					);
					if (!lhForm || lhForm == glob.fists)
					{
						desiredForms[!EquipIndex::kLeftHand] = nullptr;
					}
					else if (!lhFormIsBound)
					{
						desiredForms[!EquipIndex::kLeftHand] = lhForm;
					}

					if (!rhForm || rhForm == glob.fists)
					{
						desiredForms[!EquipIndex::kRightHand] = nullptr;
					}
					else if (!rhFormIsBound)
					{
						desiredForms[!EquipIndex::kRightHand] = rhForm;
					}

					auto ammo = equippedForms[!EquipIndex::kAmmo];
					bool ammoIsBound = ammo && ammo->HasKeywordByEditorID("WeapTypeBoundArrow");
					if (!ammo)
					{
						desiredForms[!EquipIndex::kAmmo] = nullptr;
					}
					else if (!ammoIsBound)
					{
						desiredForms[!EquipIndex::kAmmo] = ammo;
					}

					// Copy over the rest.
					for (uint8_t i = !EquipIndex::kAmmo + 1; i < !EquipIndex::kTotal; ++i)
					{
						desiredForms[i] = equippedForms[i];
					}
				}
				else
				{
					// Find a mismatch and break.
					for (uint8_t i = 0; i < !EquipIndex::kTotal; ++i)
					{
						const auto form = equippedForms[i];
						if (mismatch = form != desiredForms[i]; mismatch)
						{
							DBG
							(
								"{}: MISMATCH at index {}: equipped {} (0x{:X}) "
								"vs. should have equipped {} (0x{:X}).",
								coopActor->GetName(),
								i,
								form ? form->GetName() : "NOTHING",
								form ? form->formID : 0xDEAD,
								desiredForms[i] ? 
								desiredForms[i]->GetName() : 
								"NOTHING",
								desiredForms[i] ? 
								desiredForms[i]->formID : 
								0xDEAD
							);
						}
					}

					// Signal menu input manager to refresh menu equip state 
					// if it is currently running.
					if (glob.coopSessionActive && glob.mim->IsRunning())
					{
						glob.mim->SignalRefreshMenuEquipState();
					}
				}

				const auto iter = glob.serializablePlayerData.find(coopActor->formID);
				if (iter == glob.serializablePlayerData.end())
				{
					ERR
					(
						"Could not get serializable player data for {} (0x{:X}). Returning.",
						coopActor ? coopActor->GetName() : "NONE",
						coopActor ? coopActor->formID : 0xDEAD
					);
					return;
				}

				auto& serializableEquippedForms = 
				(
					glob.serializablePlayerData.at(coopActor->formID)->equippedForms
				);
				serializableEquippedForms = 
				(
					std::vector<RE::TESForm*>(!EquipIndex::kTotal, nullptr)
				);
				std::copy
				(
					desiredForms.begin(), 
					desiredForms.end(), 
					serializableEquippedForms.begin()
				);
				DBG
				(
					"{}: Copying desired forms list to serializable equipped forms list. "
					"New desired forms list size: {}",
					coopActor->GetName(), 
					serializableEquippedForms.size()
				);

#ifdef ALYSLC_DEBUG_MODE
				// REMOVE the following prints after sufficient debugging.
				for (auto item : equippedForms)
				{
					if (item)
					{
						equippedFormFIDs.insert(item->formID);
						DBG
						(
							"{} has a(n) {} (0x{:X}) in EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName(), item->formID
						);
					}
				}

				for (auto item : desiredForms)
				{
					if (item)
					{
						DBG
						(
							"{} has a(n) {} (0x{:X}) in DESIRED EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName(), item->formID
						);
					}
				}

				for (auto item : serializableEquippedForms)
				{
					if (item)
					{
						DBG
						(
							"{} has a(n) {} (0x{:X}) in SERIALIZABLE EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName(), item->formID
						);
					}
				}
#endif
			}
			else
			{
				DBG
				(
					"{}: Failed to obtain lock. (0x{:X})", 
					coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}
	}

	void EquipManager::RemoveUndesiredItems()
	{
		// Remove all items that were not equipped by the player
		// from the player character's inventory.

		// IMPORTANT (for my careless self):
		// Must use inventory changes here since we'll be removing items while iterating through
		// and using GetInventory() returns a view that will not update removed inventory entries
		// and could allow us to erroneously access pointers that were freed upon item removal.
		auto invChanges = coopActor->GetInventoryChanges();
		auto chestInventory = inventoryChest->GetInventory();
		std::set<RE::FormID> desiredFIDs{ };
		uint32_t reqUID = 0;
		for (auto i = 0; i < desiredForms.size(); ++i)
		{
			const auto form = desiredForms[i];
			if (!form)
			{
				continue;
			}

			const auto boundObj = form->As<RE::TESBoundObject>();
			if (!boundObj)
			{
				continue;
			}

			// DBG("{} (0x{:X}) at index {}.", boundObj->GetName(), boundObj->formID, i);
			desiredFIDs.insert(form->formID);
		}

		bool hasDesiredForms = !desiredFIDs.empty();
		// Not very expensive per-frame as the player's inventory
		// will only contain equipped items and any unequipped items will be removed here.
		std::unordered_map<RE::TESBoundObject*, HandIndex> equippedBoundObjMap{ };
		// Extra data lists to use when removing the current bound object from the inventory.
		std::unordered_map<RE::TESBoundObject*, std::vector<RE::ExtraDataList*>> itemsToRemove
		{ };
		if (invChanges && invChanges->entryList)
		{
			for (auto invEntry : *invChanges->entryList)
			{
				if (!invEntry)
				{
					continue;
				}

				auto boundObj = invEntry->object;
				if (!boundObj || invEntry->countDelta <= 0)
				{
					continue;
				}

				// Allow bound objects in the player's inventory.
				// Will unequip them elsewhere when their duration expires.
				bool isBound = 
				(
					(boundObj->IsWeapon() && boundObj->As<RE::TESObjectWEAP>()->IsBound()) ||
					(boundObj->IsAmmo() && boundObj->HasKeywordByEditorID("WeapTypeBoundArrow"))
				);
				if (isBound)
				{
					continue;
				} 

				if (hasDesiredForms && !desiredFIDs.contains(boundObj->formID))
				{
					DBG("Not a desired form: REMOVING {} ({}) from {}'s inventory.",
						boundObj->GetName(), invEntry->countDelta, coopActor->GetName());
					coopActor->RemoveItem
					(
						boundObj,
						invEntry->countDelta,
						RE::ITEM_REMOVE_REASON::kRemove,
						nullptr,
						nullptr
					);
					continue;
				}

				if (invEntry->extraLists)
				{
					if (invEntry->extraLists->empty())
					{
						DBG
						(
							"Item {}'s extra lists list is empty.", boundObj->GetName()
						);
						// Having an allocated extra lists list that is empty will cause crashes
						// if anyone tries to access any element of the list, ex. with front().
						// Since there are no extra lists (all unmodified items) 
						// and the countDelta member of the entry determines the count,
						// there's no reason to have an empty list of extra lists 
						// that pose a crash threat, so RE::free the memory 
						// and assign nullptr to prevent access 
						// as long as a null check is performed.
						// Will remove this if it is ill-advised and causes issues elsewhere, 
						// such as after adding an extra list to this item later via crafting.
						
						ERR("{}. TAHTS GON BE BUG: {}.",
							invEntry->object->GetName(), invEntry->countDelta);
						delete invEntry->extraLists;
						invEntry->extraLists = nullptr;
						continue;
					}

					for (auto extraDataList : *invEntry->extraLists)
					{
						if (!extraDataList)
						{
							continue;
						}
				
						auto isWorn = extraDataList->HasType<RE::ExtraWorn>();
						auto isWornLeft = extraDataList->HasType<RE::ExtraWornLeft>();
						auto exRank = extraDataList->GetByType<RE::ExtraRank>();
						// Equipped and should remain so, as long as the chest also has the item.
						if ((isWorn || isWornLeft) && (chestInventory.contains(boundObj)))
						{
							DBG
							(
								"{} ({:p}) is worn {}. Rank mask: 0x{:X}.",
								boundObj->GetName(), 
								fmt::ptr(extraDataList), 
								isWorn ? "right" : "left",
								exRank ? static_cast<uint32_t>(exRank->rank) : 0x0
							);
							equippedBoundObjMap.insert
							(
								{ 
									boundObj, 
									isWorn && isWornLeft ?
									HandIndex::kBoth : 
									isWorn ? 
									HandIndex::kRH :
									HandIndex::kLH 
								}
							);
						}
						else
						{
							DBG
							(
								"{} ({:p}) is NOT worn: {}, not in chest: {}. Rank mask: 0x{:X}.",
								boundObj->GetName(), 
								fmt::ptr(extraDataList), 
								!(isWorn || isWornLeft),
								!(chestInventory.contains(boundObj)),
								exRank ? static_cast<uint32_t>(exRank->rank) : 0x0
							);
							const auto iter = itemsToRemove.find(boundObj);
							if (iter == itemsToRemove.end())
							{
								itemsToRemove.insert
								(
									{ boundObj, { extraDataList } }
								);
							}
							else
							{
								iter->second.emplace_back(extraDataList);
							}
						}
					}
				}
			}
		}

		// IMPORTANT:
		// Do not be dumb like me and call RemoveItem()/AddObjectToContainer 
		// while iterating through the inventory changes entries
		// since the next inventory entry or extra data list pointer 
		// can become invalidated and cause crashes when accessed.
		// This is why we gathered all the extra data lists for items to remove
		// so we can do it afterward.
		// 
		// Remove and then re-equip if marked as worn in the chest.
		for (const auto& [boundObj, extraDataLists] : itemsToRemove)
		{
			if (!boundObj || extraDataLists.empty())
			{
				continue;
			}

			for (auto extraDataList : extraDataLists)
			{
				if (!extraDataList)
				{
					continue;
				}
				
				DBG
				(
					"REMOVE: In inventory AND not designated as worn OR not present "
					"in the inventory chest: {}. "
					"Removing {} ({}, {:p}) from {}'s inventory "
					"and NOT re-equipping.",
					!chestInventory.contains(boundObj),
					boundObj->GetName(),
					extraDataList->GetCount(), 
					fmt::ptr(extraDataList),
					coopActor->GetName()
				);
				coopActor->RemoveItem
				(
					boundObj,
					extraDataList->GetCount(),
					RE::ITEM_REMOVE_REASON::kRemove,
					extraDataList,
					nullptr
				);

				/*
				auto wornRankRH = Util::GetWornRankExtraDataList
				(
					inventoryChest.get(), boundObj, false
				);
				auto wornRankLH = Util::GetWornRankExtraDataList
				(
					inventoryChest.get(), boundObj, true
				);
				if (!wornRankLH && !wornRankRH)
				{
					DBG
					(
						"REMOVE: In inventory AND not designated as worn (present: {}) "
						"in the inventory chest. "
						"Removing {} ({}, {:p}) from {}'s inventory "
						"and NOT re-equipping.",
						chestInventory.contains(boundObj),
						boundObj->GetName(),
						extraDataList->GetCount(), 
						fmt::ptr(extraDataList),
						coopActor->GetName()
					);
					coopActor->RemoveItem
					(
						boundObj,
						extraDataList->GetCount(),
						RE::ITEM_REMOVE_REASON::kRemove,
						extraDataList,
						nullptr
					);
				}
				else
				{	
					auto isWorn = extraDataList->HasType<RE::ExtraWorn>();
					auto isWornLeft = extraDataList->HasType<RE::ExtraWornLeft>();
					
					DBG
					(
						"RE-EQUIP: In inventory and {} BUT designated as worn {} "
						"in the inventory chest. "
						"Re-equipping {} ({}, {:p}) from {}'s inventory.",
						isWorn && isWornLeft ?
						"worn LH/RH" :
						isWorn ? 
						"worn RH" :
						isWornLeft ?
						"worn LH" :
						"not worn",
						wornRankLH && wornRankRH ? 
						"LH/RH" :
						wornRankRH ? 
						"RH" :
						"LH",
						boundObj->GetName(),
						extraDataList->GetCount(), 
						fmt::ptr(extraDataList),
						coopActor->GetName()
					);
					auto equipType = boundObj->As<RE::BGSEquipType>();
					auto equipSlot = 
					(
						wornRankLH ? 
						glob.leftHandEquipSlot : 
						equipType && 
						equipType->equipSlot == glob.bothHandsEquipSlot ?
						glob.bothHandsEquipSlot : 
						equipType ? 
						glob.rightHandEquipSlot :
						nullptr
					);

					// The game auto-unequipped the item.
					// FUUUUUUUUUU.
					// Should re-equip straight away.
					if (wornRankRH)
					{
						Util::EquipObject
						(
							coopActor.get(), boundObj, extraDataList, 1, equipSlot
						);
					}

					if (wornRankLH)
					{
						Util::EquipObject
						(
							coopActor.get(), boundObj, extraDataList, 1, equipSlot
						);
					}
									
					// NOTE:
					// Didn't call HandleCompanionPlayerEquip() above, 
					// because it will unequip current, re-add, 
					// which will loop back to this function 
					// via AddObjectToContaienr, and then equip.
					// The item is already in the player's inventory
					// and likely in the desired forms list too (need to verify),
					// so no need to go through the whole song and dance again.
					// REMOVE when done debugging.
					bool found = false;
					for (auto i = 0; i < desiredForms.size(); ++i)
					{
						const auto form = desiredForms[i];
						if (!form || form != boundObj)
						{
							continue;
						}

						DBG
						(
							"Found re-equipped item {} (0x{:X}) at index {} "
							"in desired forms list.",
							boundObj->GetName(), boundObj->formID, i
						);
						found = true;
					}

					if (!found)
					{
						ERR
						(
							"ERR: {} not found in desired forms list "
							"after re-equipping.",
							boundObj->GetName()
						);
					}
				}
				*/
			}
		}

		// Remove worn rank exData from chest lists 
		// for objects that are not equipped on the player.
		for (const auto& [boundObj, countInvEntryPair] : chestInventory)
		{
			if (!boundObj ||
				countInvEntryPair.first <= 0 ||
				!countInvEntryPair.second || 
				!countInvEntryPair.second->extraLists)
			{
				continue;
			}

			const auto boundObjEquipped = equippedBoundObjMap.contains(boundObj);
			for (auto extraDataList : *countInvEntryPair.second->extraLists)
			{
				if ((!extraDataList || boundObjEquipped) ||
					(hasDesiredForms && desiredFIDs.contains(boundObj->formID)))
				{
					continue;
				}
				
				// Not equipped by the player, 
				// so ensure no straggling worn rank data is present.
				if (Util::HasWornRankMask(extraDataList, true, true))
				{
					auto exRank = extraDataList->GetByType<RE::ExtraRank>();
					DBG
					(
						"{}: Remove all worn rank data for {} on chest list {:p}. "
						"Rank: 0x{:X}.",
						coopActor->GetName(), 
						boundObj->GetName(),
						fmt::ptr(extraDataList),
						static_cast<uint32_t>(exRank->rank)
					);
					Util::RemoveWornRankExtraData(extraDataList);
				}
			}
		}
	}

	void EquipManager::SetCopiedMagicAndFID
	(
		RE::TESForm* a_formToCopy, const PlaceholderMagicIndex& a_index
	)
	{
		// Save the spell form (and its FID) copied to the placeholder spell at the given index.

		DBG
		(
			"{}: Form: {}, index: {}.", 
			coopActor->GetName(),
			a_formToCopy ? a_formToCopy->GetName() : "NONE",
			a_index
		);

		if (a_formToCopy && !a_index >= 0 && !a_index < !PlaceholderMagicIndex::kTotal)
		{
			copiedMagicFormIDs[!a_index] = a_formToCopy->formID;
			copiedMagic[!a_index] = a_formToCopy;
			// Save to serializable data.
			glob.serializablePlayerData.at(coopActor->formID)->copiedMagic = copiedMagic;
		}
	}

	void EquipManager::SetFavoritedEmoteIdles(std::vector<RE::BSFixedString> a_emoteIdlesList)
	{
		// Set the player's list of cyclable emote idles to the given list.

		DBG("{}.", coopActor->GetName());

		favoritedEmoteIdles = GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS;
		for (auto i = 0; i < a_emoteIdlesList.size() && i < favoritedEmoteIdles.size(); ++i) 
		{
			favoritedEmoteIdles[i] = a_emoteIdlesList[i];
		}

		// Update current idle if a new idle was assigned to the current cycled index.
		// First: event name.
		// Second: index in favorited idles list.
		if (currentCycledIdleIndexPair.second < favoritedEmoteIdles.size() &&
			currentCycledIdleIndexPair.first != 
			favoritedEmoteIdles[currentCycledIdleIndexPair.second])
		{
			currentCycledIdleIndexPair.first =
			favoritedEmoteIdles[currentCycledIdleIndexPair.second];
		}

		// Serialize the changes.
		glob.serializablePlayerData.at(coopActor->formID)->cyclableEmoteIdleEvents = 
		favoritedEmoteIdles;
	}

	void EquipManager::SetInitialEquipState()
	{
		// Update initial equip state after refreshing data and before the equip manager starts.
		// Set and equip all the serialized desired forms.

		auto& savedEquippedForms = 
		(
			glob.serializablePlayerData.at(coopActor->formID)->equippedForms
		);
		if (savedEquippedForms.empty() || 
			savedEquippedForms.size() == 0 || 
			savedEquippedForms.size() != desiredForms.size())
		{
			DBG
			(
				"SetInitialEquipState: "
				"{}: saved equipped forms list is {} ({}).",
				coopActor->GetName(),
				savedEquippedForms.empty() || savedEquippedForms.size() == 0 ? 
				"empty" : 
				"not the right size", 
				savedEquippedForms.size()
			);
		}
		else
		{
			desiredForms.fill(nullptr);
			// Explicitly copy the saved equipped forms list into the desired forms list.
			std::copy
			(
				savedEquippedForms.begin(),
				savedEquippedForms.end(), 
				desiredForms.begin()
			);
		}

		// Ensure the placeholder spells hold a valid copied spell
		// before adding back to desired equipped forms.
		copiedMagic = glob.serializablePlayerData.at(coopActor->formID)->copiedMagic;
		auto lhObj = desiredForms[!EquipIndex::kLeftHand];
		auto rhObj = desiredForms[!EquipIndex::kRightHand];
		auto voiceObj = desiredForms[!EquipIndex::kVoice];
		if (!p->isPlayer1)
		{
			bool is2HSpell = false;
			RE::SpellItem* copiedSpell = nullptr;
			if (auto lhSpell = lhObj ? lhObj->As<RE::SpellItem>() : nullptr; lhSpell)
			{
				is2HSpell = lhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::k2H] && 
						copiedMagic[!PlaceholderMagicIndex::k2H]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
						nullptr
					);
					desiredForms[!EquipIndex::kLeftHand] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::k2H)
					);
				}
				else
				{
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::kLH] && 
						copiedMagic[!PlaceholderMagicIndex::kLH]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::kLH]->As<RE::SpellItem>() :
						nullptr
					);
					desiredForms[!EquipIndex::kLeftHand] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kLH)
					);
				}
			}

			if (auto rhSpell = rhObj ? rhObj->As<RE::SpellItem>() : nullptr; rhSpell)
			{
				is2HSpell = rhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::k2H] && 
						copiedMagic[!PlaceholderMagicIndex::k2H]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
						nullptr
					);
					desiredForms[!EquipIndex::kRightHand] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::k2H)
					);
				}
				else
				{
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::kRH] && 
						copiedMagic[!PlaceholderMagicIndex::kRH]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::kRH]->As<RE::SpellItem>() :
						nullptr
					);
					desiredForms[!EquipIndex::kRightHand] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kRH)
					);
				}
			}

			if (voiceObj)
			{
				// Make sure desired equipped form is the voice placeholder spell
				// if the saved voice form is a spell.
				// Otherwise, directly set as the saved shout.
				if (voiceObj->As<RE::SpellItem>()) 
				{
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::kVoice] &&
						copiedMagic[!PlaceholderMagicIndex::kVoice]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::kVoice]->As<RE::SpellItem>() :
						nullptr
					);
					desiredForms[!EquipIndex::kVoice] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kVoice)
					);
				}
				else
				{
					desiredForms[!EquipIndex::kVoice] = voiceObj;
				}
			}
		}

		// Set initial quick slot item and spell to saved forms here 
		// since they won't be set when refreshing equip state 
		// (not equipped in a slot like the other forms).
		// Any modification to these two members afterward is done in the menu input manager
		// when the player reassigns forms to these slots.
		// They get saved to the desired equipped forms list 
		// and serialized when refreshing the equip state.
		quickSlotItem = 
		equippedForms[!EquipIndex::kQuickSlotItem] = 
		desiredForms[!EquipIndex::kQuickSlotItem];

		auto spellForm = desiredForms[!EquipIndex::kQuickSlotSpell];
		// Ensure the serialized form is a spell before setting.
		quickSlotSpell =
		(
			spellForm && 
			spellForm->Is(RE::FormType::Spell) ? 
			spellForm->As<RE::SpellItem>() : 
			nullptr
		);
		equippedForms[!EquipIndex::kQuickSlotSpell] = 
		desiredForms[!EquipIndex::kQuickSlotSpell] =
		quickSlotSpell;
		
		if (p->isPlayer1)
		{
			// Update equipped forms list to reflect the current equip state for P1,
			// which may be different from the saved equip state
			// copied into the desired equipped forms list.
			// Get P1's equip state updated before the manager starts.
			RefreshEquipState(RefreshSlots::kAll);
		}
	}

	void EquipManager::SetCyclableFavForms(CyclableForms a_favFormType)
	{
		// Set cyclable lists of favorited forms 
		// (ammo, hand spells, voice powers/shouts, and weapons) of the given type.

		DBG("{}.", coopActor->GetName());

		// Clear out before updating.
		cyclableFormsMap.insert_or_assign(a_favFormType, std::vector<RE::TESForm*>());
		if (a_favFormType == CyclableForms::kAmmo || a_favFormType == CyclableForms::kWeapon)
		{
			auto inventory = 
			(
				p->isPlayer1 ? coopActor->GetInventory() : inventoryChest->GetInventory()
			);
			for (const auto& [boundObj, entryDataPair] : inventory)
			{
				if (!boundObj || entryDataPair.first <= 0 || !entryDataPair.second) 
				{
					continue;
				}

				if (!entryDataPair.second->extraLists)
				{
					continue;
				}

				for (auto exDataList : *entryDataPair.second->extraLists)
				{
					if (!exDataList || !exDataList->HasType(RE::ExtraDataType::kHotkey))
					{
						continue;
					}

					bool isWeapon = *boundObj->formType == RE::FormType::Weapon;
					bool isShield = 
					(
						boundObj->As<RE::TESObjectARMO>() && 
						boundObj->As<RE::TESObjectARMO>()->IsShield()
					);
					bool isTorch = 
					(
						boundObj->As<RE::TESObjectLIGH>() && 
						boundObj->As<RE::TESObjectLIGH>()->data.flags.all
						(
							RE::TES_LIGHT_FLAGS::kCanCarry
						)
					);
					// Weapons, shields, torches, ammo.
					if ((a_favFormType == CyclableForms::kWeapon) &&
						(isWeapon || isShield || isTorch))
					{
						cyclableFormsMap[a_favFormType].push_back(boundObj);
					}
					else if (a_favFormType == CyclableForms::kAmmo && 
							 *boundObj->formType == RE::FormType::Ammo)
					{
						cyclableFormsMap[a_favFormType].push_back(boundObj);
					}
				}
			}
		}
		else
		{
			// Read in the cached magical favorites and assign to each category.
			const auto iter = glob.serializablePlayerData.find(coopActor->formID);
			if (iter != glob.serializablePlayerData.end())
			{
				auto& data = iter->second;
				for (auto spellForm : data->favoritedMagForms)
				{
					if (!spellForm)
					{
						continue;
					}

					if (a_favFormType == CyclableForms::kVoice && spellForm->As<RE::TESShout>())
					{
						cyclableFormsMap[CyclableForms::kVoice].push_back(spellForm);
					}
					else if (auto spell = spellForm->As<RE::SpellItem>(); spell)
					{
						auto spellType = spell->GetSpellType();
						bool isVoiceSlotSpell = 
						(
							spellType == RE::MagicSystem::SpellType::kVoicePower ||
							spellType == RE::MagicSystem::SpellType::kPower ||
							spellType == RE::MagicSystem::SpellType::kLesserPower
						);

						if (a_favFormType == CyclableForms::kVoice && isVoiceSlotSpell)
						{
							cyclableFormsMap[a_favFormType].push_back(spellForm);
						}
						else if (a_favFormType == CyclableForms::kSpell && !isVoiceSlotSpell)
						{
							cyclableFormsMap[a_favFormType].push_back(spellForm);
						}
					}
				}
			}
		}

		// Remove duplicates.
		if (auto& favFormsList = cyclableFormsMap[a_favFormType]; !favFormsList.empty())
		{
			auto newEnd = std::unique(favFormsList.begin(), favFormsList.end());
			if (newEnd != favFormsList.end())
			{
				favFormsList.erase(newEnd, favFormsList.end());
			}
		}
	}

	void EquipManager::SetCurrentVoiceSpell()
	{
		// Get highest known shout variation and set the voice spell to that variation.
		// Or if a power is equipped, set the voice spell to that power.

		if (!voiceForm)
		{
			return;
		}

		highestShoutVarIndex = -1;
		voiceSpell = nullptr;
		// Is a shout.
		if (auto shout = voiceForm->As<RE::TESShout>(); shout)
		{
			// Get highest known variation spell and return its FID.
			uint32_t i = 0;
			for (; i < RE::TESShout::VariationIDs::kTotal; i++)
			{
				if (auto word = shout->variations[i].word; word)
				{
					bool isKnown = word->GetKnown();
					if (!isKnown)
					{
						// Stop once the first unknown word is found.
						break;
					}
				}
				else if (auto spell = shout->variations[i].spell; !spell)
				{
					// Word/spell is invalid, so break.
					break;
				}
			}

			// Decrement to keep highest variation within bounds (< 3)
			// Set shout var index.
			--i;
			highestShoutVarIndex = i;
			if (i >= 0 && i < RE::TESShout::VariationIDs::kTotal)
			{
				voiceSpell = shout->variations[i].spell;
			}

			// Ensure both our cached shout and the high proc shout are consistent with each other.
			auto highProc = 
			(
				coopActor->currentProcess ?
				coopActor->currentProcess->high : 
				nullptr
			); 
			if (highProc)
			{
				highProc->currentShout = shout;
				highProc->currentShoutVariation = 
				(
					static_cast<RE::TESShout::VariationID>(highestShoutVarIndex)
				);
			}
		}
		else
		{
			// Is a power.
			voiceSpell = voiceForm->As<RE::SpellItem>();
		}

		DBG
		(
			"{}: voice form: {}, voice spell: {}, highest shout var index: {}.", 
			coopActor->GetName(),
			voiceForm ? voiceForm->GetName() : "NONE",
			voiceSpell ? voiceSpell->GetName() : "NONE",
			highestShoutVarIndex
		);
	}

	void EquipManager::SwitchWeaponGrip(RE::TESObjectWEAP* a_weapon, bool a_equipRH)
	{
		// WIP: 
		// Switch given weapon's 1H weapon grip to 2H and vice versa.
		// A bridge too far for the ever-present feature creep in this mod.
		// For now. Heh.
		// 
		// Do not change grip for bows and crossbows.
		// They just won't work afterward. Oh well.

		bool usesAmmo = 
		(
			(a_weapon) && 
			(
				*a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kBow ||
				*a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kCrossbow
			)
		);
		if (!a_weapon || usesAmmo)
		{
			return;
		}
		
		// Save extra data to re-equip later.
		const auto exDataList = 
		(
			p->isPlayer1 ? 
			Util::GetEquippedExtraData(coopActor.get(), a_weapon, !a_equipRH) :
			Util::GetWornRankExtraDataList(inventoryChest.get(), a_weapon, !a_equipRH)
		);
		// 1H to 2H.
		if (a_weapon->equipSlot != glob.bothHandsEquipSlot)
		{
			UnequipForm
			(
				a_weapon, 
				a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand,
				1, 
				a_weapon->equipSlot, 
				true, 
				true, 
				false,
				true,
				glob.bothHandsEquipSlot
			);
			a_weapon->SetEquipSlot(glob.bothHandsEquipSlot);
		}
		// 2H to 1H
		else
		{
			auto equipSlot1H = a_equipRH ? glob.rightHandEquipSlot : glob.leftHandEquipSlot;
			UnequipForm
			(
				a_weapon, 
				a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand,
				1, 
				glob.bothHandsEquipSlot,
				true,
				true,
				false,
				true,
				equipSlot1H
			);
			a_weapon->SetEquipSlot(equipSlot1H);
		}

		// Switch weapon animation type with grip change.
		auto currentType = *a_weapon->weaponData.animationType;
		const auto iter = GlobalCoopData::WEAP_ANIM_SWITCH_MAP.find(currentType);
		if (iter != GlobalCoopData::WEAP_ANIM_SWITCH_MAP.end())
		{
			auto newType = iter->second;
			// Special case with staves switching back to 1H ranged cast animations 
			// from 2H melee animations.
			if (a_weapon->HasKeyword(glob.weapTypeKeywordsList[!RE::WEAPON_TYPE::kStaff]) &&
				currentType != RE::WEAPON_TYPE::kStaff)
			{
				newType = RE::WEAPON_TYPE::kStaff;
			}
				
			a_weapon->weaponData.animationType.reset(currentType);
			a_weapon->weaponData.animationType.set(newType);
			a_weapon->SetAltered(true);

			DBG
			(
				"{}: Switched {}'s weapon animations from type {} to {}",
				coopActor->GetName(), a_weapon->GetName(), currentType, newType
			);
		}
		
		// Equip once grip and animation type have been changed.
		EquipForm
		(
			a_weapon, 
			a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, 
			exDataList, 
			1, 
			a_weapon->GetEquipSlot()
		);
	}

	void EquipManager::UnequipAll()
	{
		// Unequip all equipped gear after re-assigning saved equipped forms 
		// to the desired forms list.

		DBG("{}.", coopActor->GetName());

		// Re-assign saved serialized forms.
		desiredForms.fill(nullptr);
		desiredExtraDataLists.fill(nullptr);
		auto& savedEquippedForms = 
		(
			glob.serializablePlayerData.at(coopActor->formID)->equippedForms
		);
		std::copy
		(
			savedEquippedForms.begin(),
			savedEquippedForms.end(), 
			desiredForms.begin()
		);

		EquipIndex equipIndex = EquipIndex::kTotal;
		for (uint8_t i = 0; i < !EquipIndex::kTotal; ++i)
		{
			equipIndex = static_cast<EquipIndex>(i);
			// Ignore accompanying equips to the forms that the player chose to equip.
			// Examples include bound weapons/ammo that are equipped 
			// once the corresponding bound weapon spell is cast
			// and enchantments that equip with the weapon they are attached to.
			bool isBound = 
			{
				(
					(equippedForms[i]) && 
					(
						equipIndex == EquipIndex::kLeftHand || equipIndex == EquipIndex::kRightHand
					) &&
					(equippedForms[i]->Is(RE::FormType::Weapon)) && 
					(equippedForms[i]->As<RE::TESObjectWEAP>()->IsBound())
				) ||
				(
					equippedForms[i] && 
					equipIndex == EquipIndex::kAmmo && 
					equippedForms[i]->Is(RE::FormType::Ammo) &&
					equippedForms[i]->As<RE::TESAmmo>()->HasKeywordByEditorID("WeapTypeBoundArrow")
				)
			};
			bool isEnchantment = 
			{
				(equippedForms[i]) && 
				(equipIndex == EquipIndex::kLeftHand || equipIndex == EquipIndex::kRightHand) &&
				(equippedForms[i]->Is(RE::FormType::Enchantment))
			};

			// Slots to still clear out regardless, 
			// since they have the greatest chance of causing 
			// equip-related soft locks from my terrible logic:
			// No bound weapons/enchantments, and only mismatches or hand slot forms.
			if ((!isBound && !isEnchantment) && 
				(equippedForms[i] != desiredForms[i] || 
				 equipIndex == EquipIndex::kLeftHand || 
				 equipIndex == EquipIndex::kRightHand))
			{
				desiredForms[i] = nullptr;
				desiredExtraDataLists[i] = nullptr;
			}
		}

		Util::Papyrus::UnequipAll(coopActor.get());
	}

	void EquipManager::UnequipAmmo
	(
		RE::TESForm* a_toUnequip,
		const RE::BGSEquipSlot* a_slot, 
		bool a_queueEquip, 
		bool a_forceEquip,
		bool a_playSounds,
		bool a_applyNow,
		const RE::BGSEquipSlot* a_slotToReplace
	)
	{
		// Unequip the given ammo.

		DBG
		(
			"{}: unequip {}.", coopActor->GetName(), a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto ammo = a_toUnequip ? a_toUnequip->As<RE::TESAmmo>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!ammo || !aem)
		{
			return;
		}
		
		const auto exDataList = Util::GetEquippedExtraData(coopActor.get(), a_toUnequip);
		auto invCounts = coopActor->GetInventoryCounts();
		const auto iter = invCounts.find(ammo);
		auto currentAmmoCount = 
		(
			iter != invCounts.end() ? 
			iter->second : 
			0
		);

		// Have to re-verify if this is true: 
		// The game has issues un/equipping ammo when count is large (e.g. 100000), 
		// so remove and re-add as a failsafe after unequipping.
		// Ugly but seems to work.
		if (p->isPlayer1)
		{
			Util::UnequipObject(coopActor.get(), ammo, exDataList); 
		}
		else
		{
			HandleCompanionPlayerUnequip
			(
				ammo, 
				EquipIndex::kAmmo,
				exDataList,
				currentAmmoCount, 
				a_slot, 
				a_queueEquip,
				a_forceEquip,
				a_playSounds,
				a_applyNow, 
				a_slotToReplace
			);
		}
	}

	void EquipManager::UnequipArmor
	(
		RE::TESForm* a_toUnequip,
		uint32_t a_count, 
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip,
		bool a_forceEquip, 
		bool a_playSounds,
		bool a_applyNow,
		const RE::BGSEquipSlot* a_slotToReplace
	)
	{
		// Unequip the given armor.

		DBG
		(
			"{}: unequip {} (0x{:X}).", 
			coopActor->GetName(), 
			a_toUnequip ? Util::GetEditorID(a_toUnequip) : "NONE",
			a_toUnequip ? a_toUnequip->formID : 0xDEAD
		);

		auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton(); 
		if (!boundObj || !aem)
		{
			return;
		}
		
		const auto exDataList = Util::GetEquippedExtraData
		(
			coopActor.get(), 
			a_toUnequip, 
			(a_toUnequip->As<RE::BGSEquipType>()) &&
			(
				a_toUnequip->As<RE::BGSEquipType>()->equipSlot == glob.leftHandEquipSlot
			)
		);
		if (p->isPlayer1)
		{
			// Nothing special to do for P1.
			Util::UnequipObject
			(
				coopActor.get(), 
				boundObj,
				exDataList,
				a_count, 
				a_slot, 
				a_queueEquip, 
				false, 
				a_playSounds,
				a_applyNow, 
				a_slotToReplace
			);
		}
		else
		{
			HandleCompanionPlayerUnequip
			(
				boundObj,
				EquipIndex::kNone,
				exDataList, 
				a_count,
				a_slot,
				a_queueEquip, 
				a_forceEquip,
				a_playSounds,
				a_applyNow,
				a_slotToReplace
			);
		}
	}

	void EquipManager::UnequipForm
	(
		RE::TESForm* a_toUnequip, 
		const EquipIndex& a_equipIndex,
		uint32_t a_count,
		const RE::BGSEquipSlot* a_slot,
		bool a_queueEquip,
		bool a_forceEquip,
		bool a_playSounds, 
		bool a_applyNow, 
		const RE::BGSEquipSlot* a_slotToReplace
	)
	{
		// Unequip the given form.

		DBG
		(
			"{}: unequip {}.", coopActor->GetName(), a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr;
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!boundObj || !aem)
		{
			return;
		}
		
		const auto exDataList = Util::GetEquippedExtraData
		(
			coopActor.get(), a_toUnequip, a_equipIndex == EquipIndex::kLeftHand
		);
		// Special case if trying to unequip dummy1H/fists here.
		// Do not clear desired equipped forms entry.
		if (a_toUnequip == glob.fists || a_toUnequip == glob.dummy1H) 
		{
			Util::UnequipObject
			(
				coopActor.get(), 
				a_toUnequip->As<RE::TESBoundObject>(), 
				exDataList, 
				1, 
				a_slot
			);
			return;
		}

		if (p->isPlayer1)
		{
			// Nothing special to do for P1.
			Util::UnequipObject
			(
				coopActor.get(), 
				boundObj, 
				exDataList,
				a_count,
				a_slot,
				a_queueEquip,
				false,
				a_playSounds, 
				a_applyNow,
				a_slotToReplace
			);
		}
		else
		{
			HandleCompanionPlayerUnequip
			(
				boundObj, 
				a_equipIndex,
				exDataList, 
				a_count,
				a_slot, 
				a_queueEquip,
				a_forceEquip,
				a_playSounds,
				a_applyNow,
				a_slotToReplace
			);
		}
	}

	void EquipManager::UnequipFormAtIndex(const EquipIndex& a_equipIndex)
	{
		// Unequip whatever form is at the given equip index.

		DBG
		(
			"{}: index: {}.", coopActor->GetName(), a_equipIndex
		);
		
		// No item is equipped at such an index. What are you thinking, melad?
		if (a_equipIndex == EquipIndex::kNone)
		{
			return;
		}
		else if (a_equipIndex == EquipIndex::kQuickSlotItem ||
				 a_equipIndex == EquipIndex::kQuickSlotSpell)
		{
			// Just clear out the equipped/desired equipped forms slots since these forms 
			// were never 'equipped' by the game.
			desiredForms[!a_equipIndex] = equippedForms[!a_equipIndex] = nullptr;
			return;
		}

		RE::TESForm* currentForm = nullptr;
		if (a_equipIndex == EquipIndex::kLeftHand ||
			a_equipIndex == EquipIndex::kRightHand) 
		{
			currentForm = coopActor->GetEquippedObject
			(
				a_equipIndex == EquipIndex::kLeftHand
			); 
		}

		if (!currentForm)
		{
			currentForm = equippedForms[!a_equipIndex];
			if (!currentForm)
			{
				return;
			}
		}
		
		// Handle special cases first. Make sure torch and shield are unequipped,
		// since they may have a lingering entry in the biped slots section 
		// of the equipped forms list that can cause problems.
		/*bool hasShieldInHand = 
		(
			(
				(a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kShield) ||
				(
					a_equipIndex == EquipIndex::kRightHand &&
					currentForm->As<RE::BGSEquipType>() &&
					currentForm->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot
				)
			) && 
			(HasShieldEquipped())	
		);
		if (hasShieldInHand)
		{
			UnequipShield();
		}*/
		
		if (currentForm->Is(RE::FormType::Spell))
		{
			UnequipSpell(currentForm, a_equipIndex);
		}
		else if (currentForm->Is(RE::FormType::Armor))
		{
			UnequipArmor(currentForm);
		}
		else if (currentForm->Is(RE::FormType::Shout))
		{
			UnequipShout(currentForm);
		}
		else
		{
			RE::BGSEquipSlot* equipSlot = nullptr;
			if (auto equipType = currentForm->As<RE::BGSEquipType>(); equipType)
			{
				// Unequipping from the "either hand" equip slot 
				// causes a "lingering equip state" bug
				// where the unequipped item still shows as equipped 
				// in the inventory/container menu,
				// and will require additional unequip requests to full unequip.
				// So we force the equip slot for our unequip call 
				// to match the passed-in equip index.
				if (equipType->equipSlot == glob.bothHandsEquipSlot)
				{
					equipSlot = glob.bothHandsEquipSlot;
				}
				else if (a_equipIndex == EquipIndex::kLeftHand)
				{
					equipSlot = glob.leftHandEquipSlot;
				}
				else if (a_equipIndex == EquipIndex::kRightHand)
				{
					equipSlot = glob.rightHandEquipSlot;
				}
				else
				{
					equipSlot = equipType->equipSlot;
				}
			}

			// Reset bound weapon equip intervals for co-op companions.
			if (!p->isPlayer1) 
			{
				if (auto weap = currentForm->As<RE::TESObjectWEAP>(); weap && weap->IsBound())
				{
					// Special case when unequipping bound bow: also unequip bound arrows.
					if (weap->IsBow())
					{
						if (auto boundArrow = equippedForms[!EquipIndex::kAmmo]; 
							boundArrow && 
							boundArrow->HasKeywordByEditorID("WeapTypeBoundArrow"))
						{
							UnequipForm(boundArrow->As<RE::TESAmmo>(), EquipIndex::kAmmo);
						}
					}
						
					auto aem = RE::ActorEquipManager::GetSingleton();
					if (equipSlot == glob.bothHandsEquipSlot)
					{
						// Clearing out both hands means all bound weapons will be unequipped,
						// so clear out the 1H requests as well.
						p->pam->boundWeapReq2H = false;
						p->pam->boundWeapReqLH = false;
						p->pam->boundWeapReqRH = false;
						p->pam->secsSinceBoundWeap2HReq = 0.0f;
						p->pam->secsSinceBoundWeapLHReq = 0.0f;
						p->pam->secsSinceBoundWeapRHReq = 0.0f;
					}
					else if (a_equipIndex == EquipIndex::kLeftHand)
					{
						p->pam->boundWeapReqLH = false;
						p->pam->secsSinceBoundWeapLHReq = 0.0f;
					}
					else if (a_equipIndex == EquipIndex::kRightHand)
					{
						p->pam->boundWeapReqRH = false;
						p->pam->secsSinceBoundWeapRHReq = 0.0f;
					}
				}
			}
				
			DBG
			(
				"{}: {} from equip slot {}.", 
				coopActor->GetName(), 
				currentForm->GetName(),
				equipSlot ? Util::GetEditorID(equipSlot) : "NONE"
			);
			UnequipForm(currentForm, a_equipIndex, 1, equipSlot);
		}
	}

	void EquipManager::UnequipHandForms(RE::BGSEquipSlot* a_slot)
	{
		// Unequip form(s) in the given hand equip slot.

		DBG
		(
			"{}: slot: {} (0x{:X}).", 
			coopActor->GetName(), 
			a_slot ? Util::GetEditorID(a_slot) : "NONE",
			a_slot ? a_slot->formID : 0xDEAD
		);

		if (p->isPlayer1) 
		{
			auto aem = RE::ActorEquipManager::GetSingleton();
			if (!aem)
			{
				return;
			}
			
			/*
			* NOTE:
			* Unused for now and more of a failsafe if P1 has exWorn data on an unequipped object.
			auto lhExtraDataList = Util::GetEquippedExtraData
			(
				coopActor.get(), equippedForms[!EquipIndex::kLeftHand], true
			);
			if (lhExtraDataList &&
				equippedForms[!EquipIndex::kLeftHand] != coopActor->GetEquippedObject(true))
			{
				DBG
				(
					"{}: ERR: LH list {:p} will have its worn exData removed "
					"because the cached and current LH forms ({} and {}) do not match.", 
					coopActor->GetName(),
					fmt::ptr(lhExtraDataList),
					equippedForms[!EquipIndex::kLeftHand] ?
					equippedForms[!EquipIndex::kLeftHand]->GetName() :
					"NONE",
					coopActor->GetEquippedObject(true) ?
					coopActor->GetEquippedObject(true)->GetName() :
					"NONE"
				);
				auto exWorn = lhExtraDataList->GetByType<RE::ExtraWorn>();
				if (exWorn)
				{
					lhExtraDataList->Remove(RE::ExtraDataType::kWorn, exWorn);
				}

				auto exWornLeft = lhExtraDataList->GetByType<RE::ExtraWornLeft>();
				if (exWornLeft)
				{
					lhExtraDataList->Remove(RE::ExtraDataType::kWornLeft, exWornLeft);
				}
			}

			auto rhExtraDataList = Util::GetEquippedExtraData
			(
				coopActor.get(), equippedForms[!EquipIndex::kRightHand], false
			);
			if (rhExtraDataList &&
				equippedForms[!EquipIndex::kRightHand] != coopActor->GetEquippedObject(false))
			{
				DBG
				(
					"{}: ERR: RH list {:p} will have its worn exData removed "
					"because the cached and current LH forms ({} and {}) do not match.", 
					coopActor->GetName(),
					fmt::ptr(rhExtraDataList),
					equippedForms[!EquipIndex::kRightHand] ?
					equippedForms[!EquipIndex::kRightHand]->GetName() :
					"NONE",
					coopActor->GetEquippedObject(false) ?
					coopActor->GetEquippedObject(false)->GetName() :
					"NONE"
				);
				auto exWorn = rhExtraDataList->GetByType<RE::ExtraWorn>();
				if (exWorn)
				{
					rhExtraDataList->Remove(RE::ExtraDataType::kWorn, exWorn);
				}

				auto exWornLeft = rhExtraDataList->GetByType<RE::ExtraWornLeft>();
				if (exWornLeft)
				{
					rhExtraDataList->Remove(RE::ExtraDataType::kWornLeft, exWornLeft);
				}
			}
			*/

			// Unequipping the individual hand forms
			// seems to work better for P1 than just brute-force equipping fists.
			if (a_slot == glob.bothHandsEquipSlot)
			{
				if (auto lhForm = coopActor->GetEquippedObject(true); lhForm) 
				{
					if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell) 
					{
						Util::NativeFunctions::UnequipSpell
						(
							coopActor.get(),
							lhSpell, 
							!EquipIndex::kLeftHand
						);
					}
					else if (auto lhBoundObj = lhForm->As<RE::TESBoundObject>(); lhBoundObj)
					{
						auto lhEquipType = lhBoundObj->As<RE::BGSEquipType>();
						Util::UnequipObject
						(
							coopActor.get(),
							lhBoundObj, 
							Util::GetEquippedExtraData(coopActor.get(), lhBoundObj, true)
						);
					}
				}

				if (auto rhForm = coopActor->GetEquippedObject(false); rhForm)
				{
					if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
					{
						Util::NativeFunctions::UnequipSpell
						(
							coopActor.get(), 
							rhSpell, 
							!EquipIndex::kRightHand
						);
					}
					else if (auto rhBoundObj = rhForm->As<RE::TESBoundObject>(); rhBoundObj)
					{
						Util::UnequipObject
						(
							coopActor.get(),
							rhBoundObj, 
							Util::GetEquippedExtraData(coopActor.get(), rhBoundObj, false)
						);
					}
				}

				// Fists to ensure slots are cleared out. Fists!
				EquipFists(false);
			}
			else if (a_slot == glob.leftHandEquipSlot)
			{
				if (auto lhForm = coopActor->GetEquippedObject(true); lhForm)
				{
					if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell)
					{
						Util::NativeFunctions::UnequipSpell
						(
							coopActor.get(), 
							lhSpell, 
							!EquipIndex::kLeftHand
						);
					}
					else if (auto lhBoundObj = lhForm->As<RE::TESBoundObject>(); lhBoundObj)
					{
						auto lhEquipType = lhBoundObj->As<RE::BGSEquipType>();
						Util::UnequipObject
						(
							coopActor.get(),
							lhBoundObj, 
							Util::GetEquippedExtraData(coopActor.get(), lhBoundObj, true)
						);
					}
				}
			}
			else if (a_slot == glob.rightHandEquipSlot)
			{
				if (auto rhForm = coopActor->GetEquippedObject(false); rhForm)
				{
					if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
					{
						Util::NativeFunctions::UnequipSpell
						(
							coopActor.get(), 
							rhSpell, 
							!EquipIndex::kRightHand
						);
					}
					else if (auto rhBoundObj = rhForm->As<RE::TESBoundObject>(); rhBoundObj)
					{
						Util::UnequipObject
						(
							coopActor.get(),
							rhBoundObj, 
							Util::GetEquippedExtraData(coopActor.get(), rhBoundObj, false)
						);
					}
				}
			}
		}
		else
		{
			if (a_slot == glob.bothHandsEquipSlot)
			{
				// Fists to ensure slots are cleared out. Fists!
				EquipFists(true);
			}
			else if (a_slot == glob.leftHandEquipSlot)
			{
				/*desiredForms[!EquipIndex::kLeftHand] = nullptr;
				desiredExtraDataLists[!EquipIndex::kLeftHand] = nullptr;*/
				UnequipFormAtIndex(EquipIndex::kLeftHand);
			}
			else if (a_slot == glob.rightHandEquipSlot)
			{
				/*desiredForms[!EquipIndex::kRightHand] = nullptr;
				desiredExtraDataLists[!EquipIndex::kRightHand] = nullptr;*/
				UnequipFormAtIndex(EquipIndex::kRightHand);
			}
		}
	}

	void EquipManager::UnequipShield()
	{
		// Unequip any equipped shield.

		DBG("{}.", coopActor->GetName());

		auto shield = GetShield(); 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!shield || !aem)
		{
			return;
		}

		const auto exDataList = Util::GetEquippedExtraData(coopActor.get(), shield, false);
		if (p->isPlayer1)
		{
			Util::UnequipObject
			(
				coopActor.get(), 
				shield,
				exDataList, 
				1, 
				shield->equipSlot
			);
		}
		else
		{
			HandleCompanionPlayerUnequip
			( 
				shield,
				EquipIndex::kNone,
				exDataList, 
				1, 
				shield->equipSlot
			);
		}
	}

	void EquipManager::UnequipShout(RE::TESForm* a_toUnequip)
	{
		// Unequip the given shout.

		DBG
		(
			"{}: unequip {}.", coopActor->GetName(), a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto shout = a_toUnequip ? a_toUnequip->As<RE::TESShout>() : nullptr;
		if (!shout)
		{
			return;
		}

		if (!p->isPlayer1)
		{
			ClearDesiredEquippedFormAtIndex(a_toUnequip, !EquipIndex::kVoice);
		}

		Util::NativeFunctions::UnequipShout(coopActor.get(), shout);
	}

	void EquipManager::UnequipSpell(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex)
	{
		// Unequip the given spell from the given equip index.

		DBG
		(
			"{}: unequip {}, index: {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE",
			a_equipIndex
		);

		auto spell = a_toUnequip ? a_toUnequip->As<RE::SpellItem>() : nullptr; 
		if (!spell)
		{
			return;
		}

		auto slot = spell->equipSlot;
		bool is2HSpell = slot == glob.bothHandsEquipSlot;
		bool gameSlotIndex = a_equipIndex == EquipIndex::kVoice ? 2 : !a_equipIndex;
		if (p->isPlayer1)
		{
			if (!is2HSpell)
			{
				Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, gameSlotIndex);
			}
			else
			{
				// Both hands.
				Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, 0);
				Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, 1);
			}
		}
		else
		{
			if (a_equipIndex != EquipIndex::kVoice)
			{
				if (is2HSpell)
				{
					spell = placeholderMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>();
				}
				else if (a_equipIndex == EquipIndex::kLeftHand)
				{
					spell = placeholderMagic[!PlaceholderMagicIndex::kLH]->As<RE::SpellItem>();
				}
				else if (a_equipIndex == EquipIndex::kRightHand)
				{
					spell = placeholderMagic[!PlaceholderMagicIndex::kRH]->As<RE::SpellItem>();
				}
			}

			// Remove from desired equipped forms list.
			if (!is2HSpell)
			{
				ClearDesiredEquippedFormAtIndex(spell, !a_equipIndex);
				Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, gameSlotIndex);
			}
			else
			{
				// Equip "fists" to clear out spell slots.
				EquipFists(true);
			}
		}
	}

	void EquipManager::UpdateFavoritedFormsLists(bool&& a_useCachedMagicFavorites)
	{
		// Update all favorited form data and list(s) to serialize (physical and magical).
		// Also update assigned hotkeys for all favorited forms.
		// Can choose to simply assign the cached serialized magic favorites list,
		// instead of constructing an up-to-date list from the magic favorites singleton.

		DBG
		(
			"{}. Use cached magic favorites: {}.", coopActor->GetName(), a_useCachedMagicFavorites
		);

		favoritedForms.clear();
		favoritedFormIDs.clear();
		cyclableFormsMap.clear();
		cyclableFormsMap[CyclableForms::kAmmo] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kSpell] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kVoice] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kWeapon] = std::vector<RE::TESForm*>();
		hotkeyedForms.fill(nullptr);
		hotkeyedFormsToSlotsSetMap.clear();

		// Total number of favorited items for this player.
		uint32_t numFavoritedItems = 0;
		// Physical forms first.
		auto inventory = p->isPlayer1 ? coopActor->GetInventory() : inventoryChest->GetInventory();
		for (auto& [boundObj, entryDataPair] : inventory)
		{
			if (!boundObj || entryDataPair.first <= 0 || !entryDataPair.second)
			{
				continue;
			}

			auto exDataListList = entryDataPair.second->extraLists;
			if (!exDataListList)
			{
				continue;
			}
				
			for (auto exDataList : *exDataListList)
			{
				if (!exDataList)
				{
					continue;
				}

				auto exDataHotkey = exDataList->GetByType<RE::ExtraHotkey>(); 
				if (!exDataHotkey)
				{
					continue;
				}

				DBG("{}: {} is favorited on list {:p}.", 
					coopActor->GetName(), boundObj->GetName(), fmt::ptr(exDataList));

				favoritedFormIDs.insert(boundObj->formID);
				favoritedForms.emplace_back(boundObj);

				bool isWeapon = *boundObj->formType == RE::FormType::Weapon;
				bool isShield = 
				(
					boundObj->As<RE::TESObjectARMO>() && 
					boundObj->As<RE::TESObjectARMO>()->IsShield()
				);
				bool isTorch = 
				(
					boundObj->As<RE::TESObjectLIGH>() && 
					boundObj->As<RE::TESObjectLIGH>()->data.flags.all
					(
						RE::TES_LIGHT_FLAGS::kCanCarry
					)
				);
				// Weapons, shields, torches, ammo.
				if (isWeapon || isShield || isTorch)
				{
					cyclableFormsMap[CyclableForms::kWeapon].push_back(boundObj);
				}
				else if (*boundObj->formType == RE::FormType::Ammo)
				{
					cyclableFormsMap[CyclableForms::kAmmo].push_back(boundObj);
				}

				// Item was hotkeyed.
				if ((int8_t)(*exDataHotkey->hotkey) != (int8_t)(RE::ExtraHotkey::Hotkey::kUnbound))
				{
					auto slot = (int8_t)(*exDataHotkey->hotkey);
					auto oldHotkeyedForm = hotkeyedForms[slot];
					bool removedHotkey = false;
					if (oldHotkeyedForm && oldHotkeyedForm != boundObj) 
					{
						const auto iter = hotkeyedFormsToSlotsSetMap.find(oldHotkeyedForm->formID);
						if (iter != hotkeyedFormsToSlotsSetMap.end())
						{
							// Form is hotkeyed in this slot but also in another slot previously, 
							// so remove the previously linked hotkey.
							std::erase_if
							(
								iter->second, 
								[this, oldHotkeyedForm, boundObj, &slot]
								(const int8_t& a_hotkeyIndex)
								{
									// Keeping this print for now.
									if (a_hotkeyIndex == slot)
									{
										DBG
										(
											"{}: FORM {} was already hotkeyed in slot {}. "
											"Not saving {} as hotkeyed "
											"and now removing its duplicate hotkey.",
											coopActor->GetName(),
											oldHotkeyedForm->GetName(), 
											slot == -1 ? -1 : slot + 1, 
											boundObj->GetName()
										);
										return true;
									}

									return false;
								}
							);

							// Once there are no remaining hotkey indices in the set,
							// remove the map entry as well.
							if (iter->second.empty())
							{
								DBG
								(
									"{}: FORM {} is no longer hotkeyed in any slot. "
									"Removing from map.",
									coopActor->GetName(),
									oldHotkeyedForm->GetName()
								);
								hotkeyedFormsToSlotsSetMap.erase(iter);
							}

							exDataHotkey->hotkey = RE::ExtraHotkey::Hotkey::kUnbound;
							removedHotkey = true;
						}
					}

					if (!removedHotkey)
					{
						// Assign form to previously empty hotkey slot 
						// or link a new hotkey slot to this form.
						hotkeyedForms[slot] = boundObj;
						const auto iter = hotkeyedFormsToSlotsSetMap.find(boundObj->formID);
						if (iter != hotkeyedFormsToSlotsSetMap.end())
						{
							iter->second.insert(slot);
						}
						else
						{
							hotkeyedFormsToSlotsSetMap.emplace
							(
								boundObj->formID, std::set<int8_t>({ slot })
							);
						}

						DBG
						(
							"{}: PHYS FORM {} is hotkeyed in slot {}.",
							coopActor->GetName(),
							boundObj->GetName(), 
							slot == -1 ? -1 : slot + 1
						);
					}
				}

				DBG
				(
					"{}. ITEM {} is favorited.", coopActor->GetName(), boundObj->GetName()
				);
				++numFavoritedItems;
			}
		}

		const auto iter = glob.serializablePlayerData.find(coopActor->formID);
		if (iter == glob.serializablePlayerData.end())
		{
			DBG
			(
				"UpdateFavoritedFormsLists: {}: "
				"No serialized data found. Cannot update or modify cached magic favorites.", 
				coopActor->GetName()
			);
			return;
		}

		auto& data = iter->second;
		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (!magicFavorites)
		{
			DBG
			(
				"UpdateFavoritedFormsLists: {}: Could not get magic favorites singleton.", 
				coopActor->GetName()
			);
			return;
		}

		std::vector<RE::TESForm*> magFavoritesList{ };
		// Magical forms next.
		if (a_useCachedMagicFavorites) 
		{
			// Set magic favorites list to the serialized list.
			magFavoritesList = data->favoritedMagForms;
			for (int8_t i = 0; i < data->hotkeyedForms.size(); ++i) 
			{
				auto hotkeyedForm = data->hotkeyedForms[i];
				if (!hotkeyedForm || hotkeyedForm->IsNot(RE::FormType::Spell, RE::FormType::Shout)) 
				{
					continue;
				}

				auto oldHotkeyedForm = hotkeyedForms[i];
				// NOTE:
				// If another form, which can only be a physical favorited form here,
				// is in the same hotkey slot, keep the physical form, since it is more up to date
				// compared to the cached magical favorite form.
				// For example, if the companion player opens their inventory 
				// and hotkeys a weapon in the same slot as a spell was saved to previously, 
				// we now want that weapon to be equipable from this slot, not the spell.
				// Two or more magical forms will never occupy the same slot, 
				// since we always update the cached magic favorites when exiting the Magic Menu,
				// which is the only place players can change their favorited magical forms.
				if (oldHotkeyedForm &&
					oldHotkeyedForm != hotkeyedForm && 
					hotkeyedFormsToSlotsSetMap.contains(oldHotkeyedForm->formID))
				{
					DBG
					(
						"{}: SAVED OLD HOTKEYED FORM {} will remain in slot {}, instead of {}.",
						coopActor->GetName(),
						oldHotkeyedForm->GetName(),
						i + 1,
						hotkeyedForm->GetName()
					);
					continue;
				}

				hotkeyedForms[i] = hotkeyedForm;
				const auto iter = hotkeyedFormsToSlotsSetMap.find(hotkeyedForm->formID);
				if (iter != hotkeyedFormsToSlotsSetMap.end())
				{
					iter->second.insert(i);
				}
				else
				{
					hotkeyedFormsToSlotsSetMap.emplace
					(
						hotkeyedForm->formID, std::set<int8_t>({ i })
					);
				}

				DBG
				(
					"{}: SAVED MAGIC FORM {} is hotkeyed in slot {}.", 
					coopActor->GetName(), hotkeyedForm->GetName(), i + 1
				);
			}
		}
		else
		{
			// TODO: Persistent Favorites compat for most recent version.
			// Update the list of favorited magic forms only if Persistent Favorites 
			// is not installed or if the Favorites Menu is not open.
			// Before the Favorites Menu closes, Persistent Favorites re-adds P1's 
			// favorited magic forms before this call,
			// so we do not want to update the companion player's list of favorited magic forms.
			// We DO need to update the hotkeys when the Favorites Menu closes, 
			// since they could have changed while the menu was open.
			/*
			auto ui = RE::UI::GetSingleton();
			if (!ALYSLC::PersistentFavoritesCompat::g_installed ||
				!ui ||
				!ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME))
			{
				// Get list of current magical favorites.
				if (!magicFavorites->spells.empty()) 
				{
					for (auto magForm : magicFavorites->spells) 
					{
						if (!magForm)
						{
							continue;
						}

						magFavoritesList.emplace_back(magForm);	
					}
				}

				// Update list of magic favorites to serialize.
				data->favoritedMagForms = magFavoritesList;
			}
			*/

			// Get list of current magical favorites.
			if (!magicFavorites->spells.empty()) 
			{
				for (auto magForm : magicFavorites->spells) 
				{
					if (!magForm)
					{
						continue;
					}

					magFavoritesList.emplace_back(magForm);	
				}
			}

			// Update list of magic favorites to serialize.
			data->favoritedMagForms = magFavoritesList;

			// Update our hotkey data based on the current magical favorites list.
			for (int8_t i = 0; i < magicFavorites->hotkeys.size(); ++i)
			{
				auto magForm = magicFavorites->hotkeys[i]; 
				if (!magForm)
				{
					continue;
				}

				auto oldHotkeyedForm = hotkeyedForms[i];					
				bool removedHotkey = false;
				if (oldHotkeyedForm && oldHotkeyedForm != magForm) 
				{
					const auto iter = hotkeyedFormsToSlotsSetMap.find(oldHotkeyedForm->formID);
					if (iter != hotkeyedFormsToSlotsSetMap.end())
					{
						// Form is hotkeyed in this slot but also in another slot previously,
						// so remove the old linked hotkey.
						// This magic form should have hotkey precedence over the previous form,
						// since it is not a cached magic form and is more up to date.
						std::erase_if
						(
							iter->second, 
							[this, oldHotkeyedForm, magForm, &i]
							(const int8_t& a_hotkeyIndex)
							{
								// Keeping this print for now.
								if (a_hotkeyIndex == i)
								{
									DBG
									(
										"{}: FORM {} was already hotkeyed in slot {}. "
										"Not saving {} as hotkeyed "
										"and now removing its duplicate hotkey.",
										coopActor->GetName(), 
										oldHotkeyedForm->GetName(), 
										i == -1 ? -1 : i + 1, 
										magForm->GetName()
									);
									return true;
								}

								return false;
							}
						);

						// Once there are no remaining hotkey indices in the set,
						// remove the map entry as well.
						if (iter->second.empty())
						{
							DBG
							(
								"{}: FORM {} is no longer hotkeyed in any slot. "
								"Removing from map.",
								coopActor->GetName(),
								oldHotkeyedForm->GetName()
							);
							hotkeyedFormsToSlotsSetMap.erase(iter);
						}

						magicFavorites->hotkeys[i] = nullptr;
						removedHotkey = true;
					}
				}

				if (!removedHotkey)
				{
					// Assign form to previously empty hotkey slot
					// or link a new hotkey slot to this form.
					hotkeyedForms[i] = magForm;
					const auto iter = hotkeyedFormsToSlotsSetMap.find(magForm->formID);
					if (iter != hotkeyedFormsToSlotsSetMap.end())
					{
						iter->second.insert(i);
					}
					else
					{
						hotkeyedFormsToSlotsSetMap.emplace
						(
							magForm->formID, std::set<int8_t>({ i })
						);
					}

					DBG
					(
						"{}: MAGIC FORM {} is hotkeyed in slot {}.",
						coopActor->GetName(), magForm->GetName(), i + 1
					);
				}
			}
		}

		// Save hotkeyed forms to serialized data.
		data->hotkeyedForms = hotkeyedForms;

		for (auto magForm : magFavoritesList)
		{
			if (!magForm)
			{
				continue;
			}

			favoritedFormIDs.insert(magForm->formID);
			favoritedForms.emplace_back(magForm);
			if (magForm->As<RE::TESShout>())
			{
				cyclableFormsMap[CyclableForms::kVoice].push_back(magForm);
			}
			else if (auto spell = magForm->As<RE::SpellItem>(); spell)
			{
				auto spellType = spell->GetSpellType();
				if (spellType == RE::MagicSystem::SpellType::kVoicePower ||
					spellType == RE::MagicSystem::SpellType::kPower ||
					spellType == RE::MagicSystem::SpellType::kLesserPower)
				{
					cyclableFormsMap[CyclableForms::kVoice].push_back(magForm);
				}
				else
				{
					cyclableFormsMap[CyclableForms::kSpell].push_back(magForm);
				}
			}

			DBG
			(
				"{}. SPELL {} is favorited.", coopActor->GetName(), magForm->GetName()
			);
			++numFavoritedItems;
		}

		if (numFavoritedItems == 0)
		{
			return;
		}

		// Remove duplicates.
		for (auto i = 0; i < !CyclableForms::kTotal; ++i)
		{
			auto& favFormsList = cyclableFormsMap[static_cast<CyclableForms>(i)]; 
			if (favFormsList.empty())
			{
				continue;
			}
				
			auto newEnd = std::unique(favFormsList.begin(), favFormsList.end());
			if (newEnd != favFormsList.end())
			{
				favFormsList.erase(newEnd, favFormsList.end());
			}
		}
	}
}

#include "EquipManager.h"
#include <Compatibility.h>
#include <GlobalCoopData.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	// The equipping/unequipping system for NPCs is convoluted and does not work at times.
	// Much sanity was lost debugging this manager's functions.
	// Truly the embodiment of "it just works". Sometimes anyways.
	EquipManager::EquipManager() :
		Manager(ManagerType::kEM)
	{ }

	void EquipManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p && a_p->controllerID > -1 && a_p->controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			logger::debug("[MM] Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count());
			// Init once.
			lastCycledSpellCategory = lhSpellCategory = rhSpellCategory = FavMagicCyclingCategory::kAllFavorites;
			lastCycledWeaponCategory = lhWeaponCategory = rhWeaponCategory = FavWeaponCyclingCategory::kAllFavorites;
			currentCycledLHSpellsList.fill(nullptr);
			currentCycledRHSpellsList.fill(nullptr);
			currentCycledLHWeaponsList.fill(nullptr);
			currentCycledRHWeaponsList.fill(nullptr);
			favoritedEmoteIdles.fill(""sv);
			currentCycledAmmo = currentCycledVoiceMagic = nullptr;
			currentCycledIdleIndexPair = { "IdleStop"sv, -1 };
			lastCycledIdleIndexPair = currentCycledIdleIndexPair;
			lastCycledForm = nullptr;
			RefreshData();
		}
		else
		{
			logger::error("[EM] ERR: Initialize: Cannot construct Equip Manager for controller ID {}.", a_p ? a_p->controllerID : -1);
		}
	}

#pragma region MANAGER_FUNCS_IMPL
	void EquipManager::MainTask()
	{
		// Re-equip forms that were automatically unequipped by the game.
		// Temporary solution until figuring out how to prevent the game from auto-equipping
		// the "best" gear for co-op actors.

		if (!p->isPlayer1)
		{
			// Only resolve mismatches when only menus that are always open are showing (no Inventory/Favorites/Magic menus, etc. are open).
			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				// Ammo equipped one unit at a time to prevent odd reload/nocking and inventory display issues when a large amount of ammo is equipped at once.
				// Have to re-equip 1 unit here once the current cached ammo is cleared upon releasing the projectile.
				if (!coopActor->GetCurrentAmmo() && desiredEquippedForms[!EquipIndex::kAmmo])
				{
					auto ammoToReEquip = desiredEquippedForms[!EquipIndex::kAmmo]->As<RE::TESBoundObject>();
					// Also add 1 ammo unit when in god mode to maintain the same ammo count.
					if (p->isInGodMode)
					{
						// Add 1 ammo unit before releasing arrow/bolt.
						coopActor->AddObjectToContainer(ammoToReEquip, nullptr, 1, coopActor.get());
					}

					const auto invCounts = coopActor->GetInventoryCounts();
					if (invCounts.contains(ammoToReEquip) && invCounts.at(ammoToReEquip) > 0)
					{
						EquipAmmo(ammoToReEquip);
					}
				}

				// Check if the game unequipped desired hand forms and re-equip them as needed.
				// Sometimes these forms are unequipped without any equip event firing.
				// Can also loop infintely in certain instances where the game interferes with the equip request's equip slots.
				// Solution TBD.
				bool isEquipping = false;
				bool isUnequipping = false;
				coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
				coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
				// Not transforming/transformed, not (un)equipping, and not attacking or casting.
				if (!p->isTransforming && !p->isTransformed && !isEquipping && !isUnequipping && !p->pam->isAttacking && !p->pam->isInCastingAnim) 
				{
					// Attempt to validate and re-equip.
					ValidateEquipState();
				}
			}
		}
	}

	void EquipManager::PrePauseTask()
	{
		// No pre-state change tasks.
		return;
	}

	void EquipManager::PreStartTask()
	{
		logger::debug("[EM] PreStartTask: P{}", playerID + 1);
		RefreshEquipState(RefreshSlots::kAll);
		// "Infinite" carryweight for coop players.
		// NOTE: Modifies temporary modifier, so the previous temp buffs/debuffs are wiped.
		float permMod = coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight);
		float tempMod = coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight);
		float damageMod = coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight);
		if (Settings::bInfiniteCarryweight)
		{
			coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, (float)INT32_MAX - coopActor->GetActorValue(RE::ActorValue::kCarryWeight) - 1.0f);
		}
		else
		{
			coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -tempMod);
		}

		// Fixes skin glow/tone mismatches.
		if (!p->isPlayer1)
		{
			coopActor->DoReset3D(false);
		}

		// Don't re-equip items when transformed or when un-pausing without refreshing data.
		if (!p->isTransformed && currentState == ManagerState::kAwaitingRefresh) 
		{
			// Unequip all if the player is not a humanoid.
			if (!coopActor->HasKeyword(glob.npcKeyword))
			{
				desiredEquippedForms.fill(nullptr);
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
				// Only re-equip hand forms, as the equip state for them may be glitched.
				ReEquipHandForms();
			}
			else
			{
				// Re-equip all saved forms for co-op companions in case there was some lingering glitched equip state.
				EquipFists();
				ReEquipAll(false);
			}
		}

		// Ensure player is visible.
		coopActor->SetAlpha(1.0f);
	}

	void EquipManager::RefreshData()
	{
		// Player data.
		coopActor = RE::ActorPtr{ p->coopActor };
		controllerID = p->controllerID;
		playerID = p->playerID;
		// Spells and quick slot forms.
		quickSlotItem = nullptr;
		quickSlotSpell = nullptr;
		voiceForm = nullptr;
		voiceSpell = nullptr;
		// Grip type.
		lhNewGripType = RE::WEAPON_TYPE::kHandToHandMelee;
		lhOriginalType = RE::WEAPON_TYPE::kHandToHandMelee;
		rhNewGripType = RE::WEAPON_TYPE::kHandToHandMelee;
		rhOriginalType = RE::WEAPON_TYPE::kHandToHandMelee;
		// Armor ratings for XP calc.
		armorRatings.first = armorRatings.second = 0.0f;
		// Spells copied to placeholder spells. Retrieve from serialized data.
		copiedMagic.fill(nullptr);
		copiedMagic = glob.serializablePlayerData.at(coopActor->formID)->copiedMagic;
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
			placeholderMagic[i] = glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * controllerID + i];
		}

		// Favorited items.
		favItemsEquippedIndices.clear();
		favoritedFormIDs.clear();
		favoritesIndicesInCommon.clear();
		favoritedItemWasAdded.clear();
		favoritedPhysForms.clear();
		favoritedEmoteIdles = glob.serializablePlayerData.at(coopActor->formID)->cyclableEmoteIdleEvents;
		favoritedEmoteIdlesHashes.clear();
		for (const auto& eventName : favoritedEmoteIdles) 
		{
			favoritedEmoteIdlesHashes.insert(Hash(eventName));
		}

		// Map of lists of cyclable forms indexed by type.
		cyclableFormsMap.clear();
		// Desired equipped forms list.
		desiredEquippedForms.fill(nullptr);
		// Current equipped forms list.
		equippedForms.fill(nullptr);
		// List of all equipable magic (spells, shouts).
		equipableMagic = { {}, {} };
		// Equipable weapons.
		equipableWeapons.clear();

		// Favorites list indices for equipped quick slot forms (quick slot item, quick slot spell).
		equippedQSItemIndex = -1;
		equippedQSSpellIndex = -1;
		// Highest known shout variation for the current equipped shout.
		highestShoutVarIndex = -1;
		// Number of favorited forms.
		numFavoritedItems = 0;

		SetInitialEquipState();
		SetFavoritedForms(false);

		logger::debug("[EM] Refreshed data for {}.", coopActor ? coopActor->GetName() : "NONE");
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

#pragma endregion

	// NOTE: Currently unused since the 'Shout' package procedure does not work.
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

			copiedShoutToEquip = glob.placeholderShouts[controllerID];
			copiedShoutToEquip->Copy(a_shoutToCopy);
			copiedShoutToEquip->variations[0] = a_shoutToCopy->variations[0];
			copiedShoutToEquip->variations[1] = a_shoutToCopy->variations[1];
			copiedShoutToEquip->variations[2] = a_shoutToCopy->variations[2];
			copiedShoutToEquip->fullName = RE::BSFixedString("[Co-op] " + std::string(a_shoutToCopy->fullName.c_str()));

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
							variation.spell->avEffectSetting->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
							variation.word->GetKnown();
						}

						for (auto effect : variation.spell->effects)
						{
							if (effect && effect->baseEffect)
							{
								logger::debug("[EM] Shout variation spell {} {} has archetype: {}",
									variation.spell->GetName(), i, effect->baseEffect->GetArchetype());
								effect->baseEffect->data.flags.set(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
							}
						}
					}
				}
			}

			copiedShoutToEquip->SetAltered(true);
			logger::debug("[EM] Copied shout data to placeholder shout {}.", copiedShoutToEquip->GetName());
			SetCopiedMagicAndFID(a_shoutToCopy, PlaceholderMagicIndex::kVoice);
			// Make sure the cached placeholder magic form is set to the newly copied shout.
			placeholderMagic[!PlaceholderMagicIndex::kVoice] = copiedShoutToEquip;
		}

		return copiedShoutToEquip;
	}
	*/

	RE::SpellItem* EquipManager::CopyToPlaceholderSpell(RE::SpellItem* a_spellToCopy, const PlaceholderMagicIndex& a_index)
	{
		// Copy spell data to placeholder spell, save FID of the copied spell, and return placeholder spell.

		logger::debug("[EM] CopyToPlaceholderSpell: {}: {} to index {}.", coopActor->GetName(), a_spellToCopy ? a_spellToCopy->GetName() : "NONE", a_index);

		RE::SpellItem* copiedSpellToEquip = nullptr;
		if (a_spellToCopy && placeholderMagic[!a_index]->Is(RE::FormType::Spell))
		{
			// Do not re-copy.
			auto placeholderSpellForm = placeholderMagic[!a_index];
			if (a_spellToCopy == placeholderSpellForm)
			{
				return a_spellToCopy;
			}

			copiedSpellToEquip = glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * controllerID + !a_index];
			// IMPORTANT:
			// Copying over the spell in its entirety using Copy() causes crashes when cycling certain spells
			// while between casts (e.g. Sparks -> Flames), and I've yet to dig into the reason for it.
			// Instead, we copy over the magic item data, spell data, and effects + effect setting.
			//copiedSpellToEquip->Copy(a_spellToCopy);
			copiedSpellToEquip->CopyMagicItemData(a_spellToCopy);
			copiedSpellToEquip->avEffectSetting = a_spellToCopy->avEffectSetting;
			copiedSpellToEquip->data = a_spellToCopy->data;
			copiedSpellToEquip->effects = a_spellToCopy->effects;
			copiedSpellToEquip->fullName = RE::BSFixedString("[Co-op] " + std::string(a_spellToCopy->fullName.c_str()));
			if (a_spellToCopy->equipSlot != glob.bothHandsEquipSlot)
			{
				// Set equip slot to match placeholder slot.
				if (a_index == PlaceholderMagicIndex::kLH || a_index == PlaceholderMagicIndex::kRH)
				{
					copiedSpellToEquip->SetEquipSlot(a_index == PlaceholderMagicIndex::kRH ? glob.rightHandEquipSlot : glob.leftHandEquipSlot);
				}
				else if (a_index == PlaceholderMagicIndex::kVoice)
				{
					copiedSpellToEquip->SetEquipSlot(glob.voiceEquipSlot);
				}
				else
				{
					logger::error("[EM] ERR: CopyToPlaceholderSpell: {}: should not equip 1H/voice spell {} (0x{:X}) with equip slot 0x{:X} into 2H placeholder spell.",
						coopActor->GetName(), a_spellToCopy->GetName(), a_spellToCopy->formID, a_spellToCopy->equipSlot->formID);
					return nullptr;
				}
			}
			else
			{
				// Set equip slot to match hand.
				copiedSpellToEquip->SetEquipSlot(glob.bothHandsEquipSlot);
			}

			SetCopiedMagicAndFID(a_spellToCopy, a_index);
			// Make sure the cached placeholder magic form is set to the newly copied spell.
			placeholderMagic[!a_index] = copiedSpellToEquip;
		}

		logger::debug("[EM] CopyToPlaceholderSpell: {}: spell: {} -> {}, index: {}.",
			coopActor->GetName(), 
			a_spellToCopy ? a_spellToCopy->GetName() : "NONE", 
			copiedSpellToEquip ? copiedSpellToEquip->GetName() : "NONE",
			a_index);
		return copiedSpellToEquip;
	}

	void EquipManager::CycleAmmo()
	{
		// Pick out next favorited ammo to equip.

		logger::debug("[EM] CycleAmmo: {}.", coopActor->GetName());

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
		if (ammoTypeToCycle != kEither)
		{
			cyclableAmmoList.clear();
			for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kAmmo].size(); ++i)
			{
				const auto ammoForm = cyclableFormsMap[CyclableForms::kAmmo][i];
				if (auto ammo = ammoForm->As<RE::TESAmmo>(); ammo)
				{
					// Get the new list's index of the currently cycled ammo.
					if (ammo == currentCycledAmmo)
					{
						currentCycledAmmoIndex = j;
					}

					if ((ammoTypeToCycle == kArrow && !ammo->IsBolt()) ||
						(ammoTypeToCycle == kBolt && ammo->IsBolt()))
					{
						cyclableAmmoList.emplace_back(ammo);
						++j;
					}
				}
			}
		}

		// If the player does not have any favorited ammo of the correct type, return here.
		if (cyclableAmmoList.empty())
		{
			currentCycledAmmo = nullptr;
			return;
		}

		// Find next ammo to cycle to.
		int32_t nextAmmoIndex = currentCycledAmmoIndex;
		if (currentCycledAmmoIndex == -1)
		{
			nextAmmoIndex = 0;
		}
		else
		{
			// Wrap around.
			nextAmmoIndex = currentCycledAmmoIndex == cyclableAmmoList.size() - 1 ? 0 : currentCycledAmmoIndex + 1;
		}

		currentCycledAmmo = cyclableAmmoList[nextAmmoIndex] ? cyclableAmmoList[nextAmmoIndex]->As<RE::TESAmmo>() : nullptr;

		logger::debug("[EM] CycleAmmo: {}: current cycled ammo: {} from index {}.",
			coopActor->GetName(),
			currentCycledAmmo ? currentCycledAmmo->GetName() : "NONE",
			nextAmmoIndex);
	}

	void EquipManager::CycleEmoteIdles()
	{
		// Choose next emote idle to play.

		logger::debug("[EM] CycleEmoteIdles: {}.", coopActor->GetName());

		// Select first emote idle when there was no previous cycled emote idle.
		if (currentCycledIdleIndexPair.second == -1)
		{
			currentCycledIdleIndexPair.first = favoritedEmoteIdles[0];
			currentCycledIdleIndexPair.second = 0;
		}
		else
		{
			currentCycledIdleIndexPair.second = (currentCycledIdleIndexPair.second + 1) % favoritedEmoteIdles.size();
			currentCycledIdleIndexPair.first = favoritedEmoteIdles[currentCycledIdleIndexPair.second];
		}

		logger::debug("[EM] CycleEmoteIdles: {}: current idle: {}, index: {}.",
			coopActor->GetName(),
			currentCycledIdleIndexPair.first,
			currentCycledIdleIndexPair.second);
	}

	void EquipManager::CycleHandSlotMagic(bool&& a_rightHand)
	{
		// Choose next favorited hand-slot spell to equip.

		logger::debug("[EM] CycleHandSlotMagic: {}.", coopActor->GetName());

		SetCyclableFavForms(CyclableForms::kSpell);
		const FavMagicCyclingCategory& category = a_rightHand ? rhSpellCategory : lhSpellCategory;
		// If no spells are favorited, clear current cycled spells list and reset the category to 'All Favorites' and return.
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

		std::vector<RE::TESForm*> cyclableSpellsList;
		cyclableSpellsList.clear();
		// Index of the current cycled spell in the new cyclable spells list.
		int32_t currentCycledSpellIndex = -1;
		RE::TESForm* spellForm = nullptr;
		RE::TESForm* currentCycledSpellForm = a_rightHand ? currentCycledRHSpellsList[!category] : currentCycledLHSpellsList[!category];
		// Build list of cyclable spells based on the current category and set the current spell's index.
		for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kSpell].size(); ++i)
		{
			spellForm = cyclableFormsMap[CyclableForms::kSpell][i];
			if (auto spell = spellForm->As<RE::SpellItem>(); spell)
			{
				auto spellType = spell->GetSpellType();
				// Not a hand spell.
				if (spellType != RE::MagicSystem::SpellType::kSpell)
				{
					continue;
				}

				if (spell == currentCycledSpellForm)
				{
					currentCycledSpellIndex = j;
				}

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
					logger::critical("[EM] ERR: CycleHandSlotMagic: {}: default case for {}.", coopActor->GetName(), spell->GetName());
					continue;
				}
				}
			}
		}

		// If there still are no spells to cycle through, clear current list and reset spell category before returning.
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

		// In new cyclable list.
		int32_t nextSpellIndex = currentCycledSpellIndex;
		if (currentCycledSpellIndex == -1)
		{
			nextSpellIndex = 0;
		}
		else
		{
			RE::TESForm* currentlyEquippedForm = equippedForms[a_rightHand ? !EquipIndex::kRightHand : !EquipIndex::kLeftHand];
			if (!p->isPlayer1)
			{
				// Have to get the spell copied to the corresponding placeholder spell.
				if (auto currentlyEquippedSpell = currentlyEquippedForm ? currentlyEquippedForm->As<RE::SpellItem>() : nullptr; currentlyEquippedSpell)
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
			nextSpellIndex = currentCycledSpellIndex == cyclableSpellsList.size() - 1 ? 0 : currentCycledSpellIndex + 1;
		}

		RE::SpellItem* nextSpell = cyclableSpellsList[nextSpellIndex]->As<RE::SpellItem>();
		if (nextSpell)
		{
			if (a_rightHand)
			{
				currentCycledRHSpellsList[!rhSpellCategory] = nextSpell;
			}
			else
			{
				currentCycledLHSpellsList[!lhSpellCategory] = nextSpell;
			}
		}

		logger::debug("[EM] CycleHandSlotMagic: {}: right hand: {}, spell category {} and currently cycled spell {} from index {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhSpellCategory : lhSpellCategory,
			nextSpell ? nextSpell->GetName() : "NONE",
			nextSpellIndex);
	}

	void EquipManager::CycleHandSlotMagicCategory(bool&& a_rightHand)
	{
		// Set favorited spells category to cycle hand-slot spells from.

		logger::debug("[EM] CycleHandSlotMagicCategory: {}.", coopActor->GetName());

		// Refresh favorited forms first.
		SetCyclableFavForms(CyclableForms::kSpell);
		const FavMagicCyclingCategory& initialCategory = a_rightHand ? rhSpellCategory : lhSpellCategory;
		FavMagicCyclingCategory newCategory = static_cast<FavMagicCyclingCategory>((!initialCategory + 1) % (!FavMagicCyclingCategory::kTotal));
		// Only consider categories that have at least one favorited item.
		// Also stop cycling once the initial category is reached on wraparound.
		while (!HasCyclableSpellInCategory(newCategory) && newCategory != initialCategory)
		{
			newCategory = static_cast<FavMagicCyclingCategory>((!newCategory + 1) % (!FavMagicCyclingCategory::kTotal));
		}

		if (a_rightHand)
		{
			rhSpellCategory = newCategory;
		}
		else
		{
			lhSpellCategory = newCategory;
		}

		logger::debug("[EM] CycleHandSlotMagicCategory: {}: right hand: {}, spell category is now: {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhSpellCategory : lhSpellCategory);
	}

	void EquipManager::CycleVoiceSlotMagic()
	{
		// Choose the next favorited voice slot spell to equip.

		logger::debug("[EM] CycleVoiceSlotMagic: {}.", coopActor->GetName());

		SetCyclableFavForms(CyclableForms::kVoice);
		const auto& cyclableVoiceMagicList = cyclableFormsMap[CyclableForms::kVoice];
		// If the player does not have any favorited powers/shouts, return here.
		if (cyclableVoiceMagicList.empty())
		{
			currentCycledVoiceMagic = nullptr;
			return;
		}

		// Create list of favorited voice magic with the requested type.
		// Index of the current ammo in the new cyclable list, which is the
		// same as the favorited voice magic list in this case.
		int32_t currentCycledVoiceMagicIndex = -1;
		int32_t nextVoiceMagicIndex = currentCycledVoiceMagicIndex;
		for (uint32_t i = 0; i < cyclableVoiceMagicList.size(); ++i)
		{
			// Get the index of the currently cycled voice magic.
			if (cyclableVoiceMagicList[i] == currentCycledVoiceMagic)
			{
				currentCycledVoiceMagicIndex = i;
			}
		}

		if (currentCycledVoiceMagicIndex == -1)
		{
			nextVoiceMagicIndex = 0;
		}
		else
		{
			// Wrap around.
			nextVoiceMagicIndex = currentCycledVoiceMagicIndex == cyclableVoiceMagicList.size() - 1 ? 0 : currentCycledVoiceMagicIndex + 1;
		}

		currentCycledVoiceMagic = cyclableVoiceMagicList[nextVoiceMagicIndex];

		logger::debug("[EM] CycleVoiceSlotMagic: {}: currently cycled voice magic: {} from index {}.",
			coopActor->GetName(),
			currentCycledVoiceMagic ? currentCycledVoiceMagic->GetName() : "NONE",
			nextVoiceMagicIndex);
	}

	void EquipManager::CycleWeaponCategory(bool&& a_rightHand)
	{
		// Set the favorited weapons category to cycle weapons from.

		logger::debug("[EM] CycleWeaponCategory: {}.", coopActor->GetName());

		// Refresh favorited forms first.
		SetCyclableFavForms(CyclableForms::kWeapon);
		const FavWeaponCyclingCategory& initialCategory = a_rightHand ? rhWeaponCategory : lhWeaponCategory;
		FavWeaponCyclingCategory newCategory = static_cast<FavWeaponCyclingCategory>((!initialCategory + 1) % (!FavWeaponCyclingCategory::kTotal));
		// Only consider categories that have at least one favorited item.
		// Also stop cycling once the initial category is reached on wraparound.
		while (!HasCyclableWeaponInCategory(newCategory, a_rightHand) && newCategory != initialCategory)
		{
			newCategory = static_cast<FavWeaponCyclingCategory>((!newCategory + 1) % (!FavWeaponCyclingCategory::kTotal));
		}

		if (a_rightHand)
		{
			rhWeaponCategory = newCategory;
		}
		else
		{
			lhWeaponCategory = newCategory;
		}

		logger::debug("[EM] CycleWeaponCategory: {}: right hand: {}, weapon category is now: {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhWeaponCategory : lhWeaponCategory);

	}

	void EquipManager::CycleWeapons(bool&& a_rightHand)
	{
		// Choose the next favorited weapon to equip.

		logger::debug("[EM] CycleWeapons: {}.", coopActor->GetName());

		SetCyclableFavForms(CyclableForms::kWeapon);
		const FavWeaponCyclingCategory& category = a_rightHand ? rhWeaponCategory : lhWeaponCategory;
		// If no weapons are favorited, clear current list and reset category before returning nullptr here.
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

		std::vector<RE::TESForm*> cyclableWeaponsList;
		cyclableWeaponsList.clear();
		RE::TESForm* form = nullptr;
		RE::TESForm* currentCycledWeaponForm = a_rightHand ? currentCycledRHWeaponsList[!category] : currentCycledLHWeaponsList[!category];
		// Index of the current cycled weapon in the new cyclable weapons list.
		int32_t currentCycledWeaponIndex = -1;
		// Build list of favorited weapons in the current category and set index of current weapon.
		for (uint32_t i = 0, j = 0; i < cyclableFormsMap[CyclableForms::kWeapon].size(); ++i)
		{
			form = cyclableFormsMap[CyclableForms::kWeapon][i];
			if (auto equipType = form->As<RE::BGSEquipType>(); equipType)
			{
				// Incompatible equip slot.
				bool isShield = form->As<RE::TESObjectARMO>() && form->As<RE::TESObjectARMO>()->IsShield();
				bool isTorch = form->As<RE::TESObjectLIGH>() && form->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry);
				if (a_rightHand && (isShield || isTorch))
				{
					continue;
				}

				if (form == currentCycledWeaponForm)
				{
					currentCycledWeaponIndex = j;
				}

				// Handle shield and torch first, so that the switch statement below can handle weapons exclusively.
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
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandAxe || weapon->HasKeywordString("WeapTypeWarAxe"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kBattleaxe:
					{
						// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && weapon->HasKeywordString("WeapTypeBattleaxe"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kBow:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow || weapon->HasKeywordString("WeapTypeBow"sv))
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
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandDagger || weapon->HasKeywordString("WeapTypeDagger"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kGreatsword:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandSword || weapon->HasKeywordString("WeapTypeGreatsword"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kMace:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandMace || weapon->HasKeywordString("WeapTypeMace"sv))
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
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff || weapon->HasKeywordString("WeapTypeStaff"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					case FavWeaponCyclingCategory::kSword:
					{
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandSword || weapon->HasKeywordString("WeapTypeSword"sv))
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
						if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && weapon->HasKeywordString("WeapTypeWarhammer"sv))
						{
							cyclableWeaponsList.emplace_back(form);
							++j;
						}

						continue;
					}
					default:
					{
						logger::critical("[EM] ERR: CycleWeapons: {}: default case for {}.", coopActor->GetName(), form->GetName());
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

		// If there still are no cyclable weapons, clear the current cycled weapon and reset category before returning.
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

		// In new cyclable list.
		int32_t nextWeaponIndex = currentCycledWeaponIndex;
		if (currentCycledWeaponIndex == -1)
		{
			nextWeaponIndex = 0;
		}
		else
		{
			RE::TESForm* currentlyEquippedForm = equippedForms[a_rightHand ? !EquipIndex::kRightHand : !EquipIndex::kLeftHand];
			// When wrapping around in the "All Favorites" category, return nullptr to signal the PAFH to equip fists.
			bool wrapAround = currentCycledWeaponIndex == cyclableWeaponsList.size() - 1;
			if (category == FavWeaponCyclingCategory::kAllFavorites && wrapAround)
			{
				nextWeaponIndex = -1;
			}
			else
			{
				nextWeaponIndex = currentCycledWeaponIndex == cyclableWeaponsList.size() - 1 ? 0 : currentCycledWeaponIndex + 1;
			}
		}

		RE::TESForm* nextForm = nextWeaponIndex != -1 ? cyclableWeaponsList[nextWeaponIndex] : nullptr;
		if (a_rightHand)
		{
			currentCycledRHWeaponsList[!rhWeaponCategory] = nextForm;
		}
		else
		{
			currentCycledLHWeaponsList[!lhWeaponCategory] = nextForm;
		}

		logger::debug("[EM] CycleWeapons: {}: right hand: {}, category is now {}, cycled weapon {} from index {}.",
			coopActor->GetName(),
			a_rightHand,
			a_rightHand ? rhWeaponCategory : lhWeaponCategory,
			nextForm ? nextForm->GetName() : "NONE",
			nextWeaponIndex);
	}

	void EquipManager::EquipAmmo(RE::TESForm* a_toEquip, RE::ExtraDataList* a_exData, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow)
	{
		// Equip the given ammo.

		logger::debug("[EM] EquipAmmo: {}: equip {}.", 
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE");

		if (auto ammo = a_toEquip ? a_toEquip->As<RE::TESAmmo>() : nullptr; ammo)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				if (!p->isPlayer1)
				{
					// Unequip current ammo before equipping new one.
					if (RE::TESForm* currentAmmoForm = equippedForms[!EquipIndex::kAmmo]; currentAmmoForm)
					{
						UnequipAmmo(currentAmmoForm);
					}
					else
					{
						// Equips multiple single ammo units sometimes, which clutter up the inventory menu.
						// Remove all of this ammo and add back to reset equip state.
						// Ugly but seems to avoid creating new entries.
						const auto& invCounts = coopActor->GetInventoryCounts();
						int32_t currentAmmoCount = invCounts.contains(ammo) ? invCounts.at(ammo) : -1;
						if (currentAmmoCount != -1)
						{
							bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toEquip);
							coopActor->RemoveItem(ammo, currentAmmoCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
							coopActor->AddObjectToContainer(ammo, nullptr, currentAmmoCount, coopActor.get());
							// Have to re-favorite once re-added.
							if (wasFavorited)
							{
								Util::ChangeFormFavoritesStatus(coopActor.get(), a_toEquip, true);
							}
						}
					}

					// Add to desired equipped forms list.
					desiredEquippedForms[!EquipIndex::kAmmo] = a_toEquip;
					// NOTE: The game has issues un/equipping ammo when count is large (e.g. 100000), so only un/equip 1 at a time.
					aem->EquipObject(coopActor.get(), a_toEquip->As<RE::TESBoundObject>(), a_exData, 1, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow);
				}
				else
				{
					// Unequip current ammo before equipping new one.
					if (RE::TESForm* currentAmmoForm = equippedForms[!EquipIndex::kAmmo]; currentAmmoForm)
					{
						UnequipAmmo(currentAmmoForm);
					}

					const auto& invCounts = coopActor->GetInventoryCounts();
					if (int32_t newAmmoCount = invCounts.contains(ammo) ? invCounts.at(ammo) : -1; newAmmoCount != -1)
					{
						bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toEquip);
						// Remove directly back into P1's inventory.
						coopActor->RemoveItem(ammo, newAmmoCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, coopActor.get());
						// Have to re-favorite once re-added.
						if (wasFavorited)
						{
							Util::ChangeFormFavoritesStatus(coopActor.get(), a_toEquip, true);
						}

						aem->EquipObject(coopActor.get(), a_toEquip->As<RE::TESBoundObject>());
					}
				}
			}
		}
	}

	void EquipManager::EquipArmor(RE::TESForm* a_toEquip, RE::ExtraDataList* a_exData, uint32_t a_count, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow)
	{
		// Equip the given armor.

		logger::debug("[EM] EquipArmor: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE");

		if (auto boundObj = a_toEquip ? a_toEquip->As<RE::TESBoundObject>() : nullptr; boundObj)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				if (!p->isPlayer1)
				{
					// Must add all armor indices that correspond to the requested item to equip,
					// since armor pieces can fit into multiple biped slots.
					if (auto asBipedObjForm = a_toEquip->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
					{
						std::vector<uint8_t> equipIndices;
						auto slotMask = asBipedObjForm->bipedModelData.bipedObjectSlots;
						bool isShield = asBipedObjForm->IsShield();
						const RE::BGSEquipSlot* slot = isShield ? a_toEquip->As<RE::TESObjectARMO>()->equipSlot : a_slot;
						for (uint8_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
						{
							if (slotMask.all(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1 << (i - !EquipIndex::kFirstBipedSlot))))
							{
								equipIndices.emplace_back(i);
								// Unequip armor in same slot first.
								if (RE::TESForm* currentArmorForm = equippedForms[i]; currentArmorForm)
								{
									UnequipArmor(currentArmorForm);
								}
							}
						}

						// Add to desired equipped forms list.
						for (auto index : equipIndices)
						{
							desiredEquippedForms[index] = a_toEquip;
						}

						// Special shield case: also update LH slot in desired equipped forms list.
						if (isShield)
						{
							UnequipFormAtIndex(EquipIndex::kLeftHand);
							desiredEquippedForms[!EquipIndex::kLeftHand] = a_toEquip;
						}

						aem->EquipObject(coopActor.get(), boundObj, a_exData, a_count, slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow);
					}
					else
					{
						// Any other form which does not fit in a biped slot.
						aem->EquipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow);
					}
				}
				else
				{
					// Directly equip on P1 without forcing the equip.
					aem->EquipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, false, a_playSounds, a_applyNow);
				}
			}
		}
	}

	void EquipManager::EquipFists()
	{
		// Clear out both hand slots by equipping the 'fists' item.

		logger::debug("[EM] EquipFists: {}.", coopActor->GetName());
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (aem)
		{
			// NOTE: Very important: calling EquipObject() or UnequipObject() for P1 
			// with the a_forceEquip param set to true messes up P1's equip state
			// which means a previously equipped item will auto-equip 
			// even when trying to equip a different item.
			// NEVER force equip with P1.
			// 
			// Also do not queue the equip here, we want it to happen ASAP.
			aem->EquipObject(coopActor.get(), glob.fists, nullptr, 1, glob.bothHandsEquipSlot, false, p->isPlayer1 ? false : true, false, true);
			if (!p->isPlayer1) 
			{
				// NOTE: Can cause the game to stutter if unequipping right after equipping.
				// Also, the game only unequips fists automatically for player 1. Must be done manually for other players.
				aem->UnequipObject(coopActor.get(), glob.fists, nullptr, 1, glob.bothHandsEquipSlot, false, p->isPlayer1 ? false : true, false, true);
			}
		}
	}

	void EquipManager::EquipForm(RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, RE::ExtraDataList* a_exData, uint32_t a_count, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow)
	{
		// Equip the given form.

		logger::debug("[EM] EquipForm: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE");

		if (auto boundObj = a_toEquip ? a_toEquip->As<RE::TESBoundObject>() : nullptr; boundObj)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				// Special case if trying to equip fists here.
				// Desired forms not cleared.
				if (a_toEquip == glob.fists) 
				{
					aem->EquipObject(coopActor.get(), glob.fists, nullptr, 1, a_slot, false, p->isPlayer1 ? false : true, false, true);
					return;
				}

				if (!p->isPlayer1)
				{
					// Add to desired equipped forms list.
					if (a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kRightHand)
					{
						if (a_slot != glob.bothHandsEquipSlot)
						{
							// Unequip form in the opposite hand if equipping a two-handed weapon, or unequip the same weapon
							// if equipped in the other hand and if the co-op actor only owns one.
							// Done to prevent the equip function from duplicating the weapon and equipping in both hands.
							auto oppositeEquipIndex = a_equipIndex == EquipIndex::kLeftHand ? EquipIndex::kRightHand : EquipIndex::kLeftHand;
							auto oppositeHandForm = equippedForms[!oppositeEquipIndex];
							auto numberOwned = coopActor->GetInventoryCounts().at(boundObj);
							bool alreadyEquippedInOtherHand = numberOwned == 1 && oppositeHandForm == a_toEquip && a_toEquip->As<RE::BGSEquipType>() && a_toEquip->As<RE::BGSEquipType>()->equipSlot != glob.bothHandsEquipSlot;
							if (alreadyEquippedInOtherHand)
							{
								UnequipHandForms(glob.bothHandsEquipSlot);
								// And add back to ensure the game fully unequips it.
								// Had issues without this call before.
								coopActor->RemoveItem(boundObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, coopActor.get());
							}
							else
							{
								// Unequip LH/RH form first.
								UnequipFormAtIndex(a_equipIndex);
							}

							// Set desired equipped form at the given index.
							desiredEquippedForms[!a_equipIndex] = a_toEquip;
							// Equip enchantment as well if equipping a staff.
							if (auto weap = boundObj->As<RE::TESObjectWEAP>(); weap && weap->IsStaff() && weap->formEnchanting) 
							{
								aem->EquipObject(coopActor.get(), weap->formEnchanting);
							}
						}
						else
						{
							// Clear both hands first.
							UnequipHandForms(glob.bothHandsEquipSlot);
							// Set both LH and RH indices if this form is 2H.
							desiredEquippedForms[!EquipIndex::kLeftHand] = a_toEquip;
							desiredEquippedForms[!EquipIndex::kRightHand] = a_toEquip;
						}
					}
					else
					{
						// Quick slot, consumables, etc.
						// Just update the index, no need to unequip, or clear out.
						desiredEquippedForms[!a_equipIndex] = a_toEquip;
					}

					aem->EquipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow);
				}
				else
				{
					auto oppositeEquipIndex = a_equipIndex == EquipIndex::kLeftHand ? EquipIndex::kRightHand : EquipIndex::kLeftHand;
					auto oppositeHandForm = equippedForms[!oppositeEquipIndex];
					auto numberOwned = coopActor->GetInventoryCounts().at(boundObj);
					bool alreadyEquippedInOtherHand = numberOwned == 1 && oppositeHandForm == a_toEquip && a_toEquip->As<RE::BGSEquipType>() && a_toEquip->As<RE::BGSEquipType>()->equipSlot != glob.bothHandsEquipSlot;
					if (alreadyEquippedInOtherHand)
					{
						UnequipHandForms(glob.bothHandsEquipSlot);
						coopActor->RemoveItem(boundObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, coopActor.get());
					}

					// Do not force equip once again.
					aem->EquipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, false, a_playSounds, a_applyNow);
				}
			}
		}
	}

	void EquipManager::EquipShout(RE::TESForm* a_toEquip)
	{
		// Equip the given shout.

		logger::debug("[EM] EquipShout: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE");

		if (auto shout = a_toEquip ? a_toEquip->As<RE::TESShout>() : nullptr; shout)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				if (!p->isPlayer1)
				{
					// Ensure P1 and co-op player's shout lists remain in sync.
					if (!coopActor->HasShout(shout))
					{
						coopActor->AddShout(shout);
					}

					// Unequip current spell/shout first.
					if (RE::TESForm* currentVoiceForm = equippedForms[!EquipIndex::kVoice]; currentVoiceForm)
					{
						// Power.
						if (currentVoiceForm->As<RE::SpellItem>())
						{
							UnequipSpell(currentVoiceForm, EquipIndex::kVoice);
						}
						else
						{
							// Shout.
							UnequipShout(currentVoiceForm);
						}
					}

					// Add to desired equipped forms list.
					desiredEquippedForms[!EquipIndex::kVoice] = shout;
				}

				aem->EquipShout(coopActor.get(), shout);
			}
		}
	}

	void EquipManager::EquipSpell(RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, const RE::BGSEquipSlot* a_slot)
	{
		// Equip the given spell.

		logger::debug("[EM] EquipSpell: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE");

		if (auto spell = a_toEquip ? a_toEquip->As<RE::SpellItem>() : nullptr; spell)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				if (!p->isPlayer1)
				{
					// Ensure P1 and co-op player spell lists remain in sync.
					// Do not add placeholder spells to known list.
					if (!coopActor->HasSpell(spell) && !glob.placeholderSpellsSet.contains(spell))
					{
						coopActor->AddSpell(spell);
					}

					bool is2HSpell = a_slot == glob.bothHandsEquipSlot;
					// Add to desired equipped forms list.
					if (!is2HSpell)
					{
						// Unequip LH/RH form first.
						if (a_equipIndex != EquipIndex::kVoice)
						{
							// If the current spell in the LH is a 2H spell, we have to also equip and unequip the requested spell 
							// in the RH to properly equip the spell into only the LH. Idk why, but hey.
							if (a_equipIndex == EquipIndex::kLeftHand)
							{
								if (RE::TESForm* currentForm = equippedForms[!a_equipIndex]; currentForm && currentForm->Is(RE::FormType::Spell) && currentForm->As<RE::SpellItem>()->equipSlot == glob.bothHandsEquipSlot)
								{
									EquipSpell(a_toEquip, EquipIndex::kRightHand, glob.rightHandEquipSlot);
									UnequipSpell(a_toEquip, EquipIndex::kRightHand);
								}
							}

							// Clear saved equipped form and unequip in this hand.
							UnequipFormAtIndex(a_equipIndex);
							// Fix placeholder spell being equipped in the wrong hand.
							bool lhPlaceholderSpellInWrongHand = equippedForms[!EquipIndex::kRightHand] == placeholderMagic[!PlaceholderMagicIndex::kLH];
							bool rhPlaceholderSpellInWrongHand = equippedForms[!EquipIndex::kLeftHand] == placeholderMagic[!PlaceholderMagicIndex::kRH];
							if (lhPlaceholderSpellInWrongHand || rhPlaceholderSpellInWrongHand)
							{
								auto toUnequip = lhPlaceholderSpellInWrongHand ? placeholderMagic[!PlaceholderMagicIndex::kLH] : placeholderMagic[!PlaceholderMagicIndex::kRH];
								desiredEquippedForms[lhPlaceholderSpellInWrongHand ? !EquipIndex::kRightHand : !EquipIndex::kLeftHand] = nullptr;
							}

							// Handle placeholder spell copying.
							// Don't copy the placeholder spell to itself.
							if ((a_equipIndex == EquipIndex::kLeftHand && spell != placeholderMagic[!PlaceholderMagicIndex::kLH]) ||
								(a_equipIndex == EquipIndex::kRightHand && spell != placeholderMagic[!PlaceholderMagicIndex::kRH]))
							{
								spell = CopyToPlaceholderSpell(spell, a_equipIndex == EquipIndex::kRightHand ? PlaceholderMagicIndex::kRH : PlaceholderMagicIndex::kLH);
							}
						}
						else
						{
							// No need to copy voice spell to placeholder, since this spell is directly cast 
							// using the player's instant magic caster with no animation.
							
							// Unequip voice slot spell/shout first.
							if (RE::TESForm* currentForm = equippedForms[!a_equipIndex]; currentForm)
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
					else
					{
						// Unequip LH and RH forms first.
						UnequipFormAtIndex(EquipIndex::kLeftHand);
						UnequipFormAtIndex(EquipIndex::kRightHand);
						// Copy to placeholder spell as needed.
						if (spell != placeholderMagic[!PlaceholderMagicIndex::k2H])
						{
							spell = CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::k2H);
						}
					}

					if (a_slot != glob.bothHandsEquipSlot)
					{
						desiredEquippedForms[!a_equipIndex] = spell;
					}
					else
					{
						desiredEquippedForms[!EquipIndex::kLeftHand] = spell;
						desiredEquippedForms[!EquipIndex::kRightHand] = spell;
					}

					aem->EquipSpell(coopActor.get(), spell, a_slot);
				}
				else
				{
					// Simply equip for P1. Easy as it gets.
					aem->EquipSpell(coopActor.get(), spell, a_slot);
				}
			}
		}
	}

	std::vector<RE::TESForm*> EquipManager::GetEquipableSpells(bool a_inHandSlot) const
	{
		// Get all equipable hand-slot or voice-slot spells known by P1.

		logger::debug("[EM] GetEquipableSpells: {}: in hand slot: {}.", 
			coopActor->GetName(),
			a_inHandSlot);

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
			if (auto p1BaseSpellList = (p1ActorBase->actorEffects) ? p1ActorBase->actorEffects->spells : nullptr; p1BaseSpellList)
			{
				uint32_t spellListSize = p1ActorBase->actorEffects->numSpells;
				// Get player 1's actorbase spell list.
				for (uint32_t i = 0; i < spellListSize; ++i)
				{
					if (auto spell = p1BaseSpellList[i]; spell)
					{
						spellType = spell->GetSpellType();
						handSlotSpell = a_inHandSlot && spellType == RE::MagicSystem::SpellType::kSpell;
						voiceSlotSpell = !a_inHandSlot && (spellType == RE::MagicSystem::SpellType::kLesserPower ||
														  spellType == RE::MagicSystem::SpellType::kPower ||
														  spellType == RE::MagicSystem::SpellType::kVoicePower);

						if (handSlotSpell || voiceSlotSpell)
						{
							equipableSpells.push_back(spell);
						}
					}
				}
			}

			// Add shouts that the co-op player has by virtue of their actor base, and
			// add shouts that player 1 has by virtue of their actor base.
			if (!a_inHandSlot)
			{
				auto shoutList = (p1ActorBase->actorEffects) ? p1ActorBase->actorEffects->shouts : nullptr; 
				if (shoutList)
				{
					uint32_t shoutListSize = p1ActorBase->actorEffects->numShouts;
					for (uint32_t i = 0; i < shoutListSize; ++i)
					{
						if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1)
						{
							equipableSpells.emplace_back(shoutList[i]);
						}
					}
				}

				if (auto coopPlayerActorBase = coopActor->GetActorBase(); coopPlayerActorBase) 
				{
					shoutList = (coopPlayerActorBase->actorEffects) ? coopPlayerActorBase->actorEffects->shouts : nullptr;
					if (shoutList)
					{
						uint32_t shoutListSize = coopPlayerActorBase->actorEffects->numShouts;
						for (uint32_t i = 0; i < shoutListSize; ++i)
						{
							if (shoutList[i] && strlen(shoutList[i]->GetName()) > 1)
							{
								equipableSpells.emplace_back(shoutList[i]);
							}
						}
					}
				}
			}
		}

		// Add all hand or voice spells that player 1 has learned.
		for (auto spell : p1->addedSpells)
		{
			if (spell)
			{
				spellType = spell->GetSpellType();
				handSlotSpell = a_inHandSlot && spellType == RE::MagicSystem::SpellType::kSpell;
				voiceSlotSpell = { 
					!a_inHandSlot && 
					(spellType == RE::MagicSystem::SpellType::kLesserPower ||
					spellType == RE::MagicSystem::SpellType::kPower ||
					spellType == RE::MagicSystem::SpellType::kVoicePower) 
				};
				if (handSlotSpell || voiceSlotSpell)
				{
					equipableSpells.push_back(spell);
				}
			}
		}

		return equipableSpells;
	}

	RE::BGSEquipSlot* EquipManager::GetEquipSlotForForm(RE::TESForm* a_form, const EquipIndex& a_index) const
	{
		// Get equip slot for the given form with the requested equip index.

		logger::debug("[EM] GetEquipSlotForForm: {}: form: {}, index: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_index);

		auto equipSlot = glob.eitherHandEquipSlot;
		if (a_form && a_form->As<RE::BGSEquipType>())
		{
			auto asEquipType = a_form->As<RE::BGSEquipType>();
			if ((asEquipType->equipSlot == glob.bothHandsEquipSlot) && (a_index == EquipIndex::kLeftHand || a_index == EquipIndex::kRightHand))
			{
				equipSlot = glob.bothHandsEquipSlot;
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
		}

		return equipSlot;
	}

	std::vector<RE::TESForm*> EquipManager::GetFavoritedPhysForms(bool a_shouldUpdate)
	{
		// Get all favorited 'physical', meaning not magical, forms.
		// Can also update the cached list.

		logger::debug("[EM] GetFavoritedPhysForms: {}: should update: {}.", 
			coopActor->GetName(),
			a_shouldUpdate);

		if (a_shouldUpdate)
		{
			SetFavoritedForms(false);
		}

		return favoritedPhysForms;
	}

	float EquipManager::GetWornWeight() const
	{
		// Get the total weight of this player's worn gear.

		float wornWeight = 0.0f;
		for (auto& bpObj : equippedForms)
		{
			if (bpObj)
			{
				wornWeight += bpObj->GetWeight();
			}
		}

		return (wornWeight < 0.0f) ? 0.0f : wornWeight;
	}

	std::string_view EquipManager::FavMagCyclingCategoryToString(const FavMagicCyclingCategory& a_category) const
	{
		// Favorited magic cycling category mapped to its name.

		logger::debug("[EM] FavMagCyclingCategoryToString: {}: category: {}.", 
			coopActor->GetName(), 
			a_category);

		switch (a_category)
		{
		case FavMagicCyclingCategory::kAllFavorites:
		{
			return "All Favorites"sv;
		}
		case FavMagicCyclingCategory::kAlteration:
		{
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
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

	std::string_view EquipManager::FavWeaponCyclingCategoryToString(const FavWeaponCyclingCategory& a_category) const
	{
		// Favorited weapon cycling category mapped to its name.

		logger::debug("[EM] FavWeaponCyclingCategoryToString: {}: category: {}.", 
			coopActor->GetName(),
			a_category);

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

	void EquipManager::HandleEquipRequest(RE::TESForm* a_form, const EquipIndex& a_index, bool a_shouldEquip)
	{
		// Handle companion player (un)equip request for the given form at the given index.
		// NOTE: Never called on P1.

		logger::debug("[EM] HandleEquipRequest: {}: form: {}, index: {}, should equip: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_index, 
			a_shouldEquip);

		if (a_form)
		{
			if (a_shouldEquip)
			{
				// Equip.
				switch (*a_form->formType)
				{
				case RE::FormType::Weapon:
				{
					EquipForm(a_form, a_index, nullptr, 1, GetEquipSlotForForm(a_form, a_index));
					break;
				}
				case RE::FormType::Armor:
				case RE::FormType::Armature:
				{
					EquipArmor(a_form);
					break;
				}
				case RE::FormType::Spell:
				{
					EquipSpell(a_form, a_index, GetEquipSlotForForm(a_form, a_index));
					break;
				}
				case RE::FormType::Ammo:
				{
					EquipAmmo(a_form);
					break;
				}
				case RE::FormType::Shout:
				{
					EquipShout(a_form);
					break;
				}
				case RE::FormType::Light:
				{
					if (auto asLight = a_form->As<RE::TESObjectLIGH>(); asLight && asLight->CanBeCarried())
					{
						EquipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
					}
				}
				default:
				{
					if (auto boundObj = a_form->As<RE::TESBoundObject>(); boundObj)
					{
						if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
						{
							// Just equip, do not update desired equipped forms list since there is no slot for it.
							aem->EquipObject(coopActor.get(), boundObj, nullptr, 1, nullptr, false, true, false, true);
						}
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
					UnequipForm(a_form, a_index, nullptr, 1, GetEquipSlotForForm(a_form, a_index));
					break;
				}
				case RE::FormType::Armor:
				case RE::FormType::Armature:
				{
					if (auto asBipedObjForm = a_form->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
					{
						UnequipArmor(a_form);
					}

					break;
				}
				case RE::FormType::Spell:
				{
					if (auto spell = a_form->As<RE::SpellItem>(); spell)
					{
						// LH, RH, 2H, voice.
						UnequipSpell(a_form, a_index);
					}

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
					if (auto asLight = a_form->As<RE::TESObjectLIGH>(); asLight && asLight->CanBeCarried())
					{
						UnequipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
					}
				}
				default:
				{
					if (auto boundObj = a_form->As<RE::TESBoundObject>(); boundObj)
					{
						if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
						{
							// Just equip, do not update desired equipped forms list since there is no slot for it.
							aem->UnequipObject(coopActor.get(), boundObj, nullptr, 1, nullptr, false, true, false, true);
						}
					}

					break;
				}
				}
			}
		}
	}

	void EquipManager::HandleMenuEquipRequest(RE::ObjectRefHandle a_fromContainerHandle, RE::TESForm* a_form, const EquipIndex& a_index, bool a_placeholderMagicChanged)
	{
		// Handle MIM equip request for the given form, from the given container, at the given index.
		// Also indicates whether a placeholder spell was changed from copying over the form to equip.
		
		// NOTE: Never called on P1.
		logger::debug("[EM] HandleMenuEquipRequest: {}: container: {}, form: {}, index: {}, placeholder spell changed: {}.", 
			coopActor->GetName(),
			Util::HandleIsValid(a_fromContainerHandle) ? Util::GetRefrPtrFromHandle(a_fromContainerHandle)->GetName() : "NONE",
			a_form ? a_form->GetName() : "NONE",
			a_index,
			a_placeholderMagicChanged);

		// NOTE: Spells to equip should not be placeholder spells.
		auto fromContainer = Util::GetRefrPtrFromHandle(a_fromContainerHandle);
		if (!fromContainer)
		{
			logger::critical("[EM] HandleMenuEquipRequest: ERR: {}: source container is invalid.", coopActor->GetName());
			return;
		}

		if (a_form)
		{
			// Trying to equip from container, so transfer the requested item to the player before equipping.
			if (fromContainer && fromContainer != coopActor && a_form->As<RE::TESBoundObject>())
			{
				fromContainer->RemoveItem(a_form->As<RE::TESBoundObject>(), 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, coopActor.get());
			}

			switch (*a_form->formType)
			{
			case RE::FormType::Weapon:
			{
				auto equipSlot = GetEquipSlotForForm(a_form, a_index);
				if (auto currentFormInHand = equippedForms[!a_index]; a_form != currentFormInHand)
				{
					EquipForm(a_form, a_index, nullptr, 1, equipSlot);
				}
				else
				{
					UnequipForm(a_form, a_index, nullptr, 1, equipSlot);
				}

				break;
			}
			case RE::FormType::Armor:
			case RE::FormType::Armature:
			{
				RE::TESObjectARMO* currentArmorInSlot = nullptr;

				if (auto asBipedObjForm = a_form->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
				{
					currentArmorInSlot = coopActor->GetWornArmor(asBipedObjForm->GetSlotMask());
					if (a_form != currentArmorInSlot)
					{
						EquipArmor(a_form);
					}
					else
					{
						UnequipArmor(a_form);
					}
				}

				break;
			}
			case RE::FormType::Spell:
			{
				// Check if already equipped by comparing with the requested spell with the corresponding copied spell,
				// and the currently equipped spell with the placeholder spell in the same slot.
				RE::TESForm* currentForm = equippedForms[!a_index];
				// Should equip if no currently equipped form in the slot.
				bool shouldEquip = !currentForm;
				if (!shouldEquip) 
				{
					bool currentSpellIsVoice = currentForm->As<RE::BGSEquipType>() && currentForm->As<RE::BGSEquipType>()->equipSlot == glob.voiceEquipSlot;
					if (currentSpellIsVoice)
					{
						// Equip voice slot spell if it is not the same as the current one.
						shouldEquip = a_form != currentForm;
					}
					else
					{
						bool currentSpellIs2H = currentForm->As<RE::BGSEquipType>() && currentForm->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot;
						PlaceholderMagicIndex currentPlaceholderIndex = currentSpellIs2H ? PlaceholderMagicIndex::k2H : static_cast<PlaceholderMagicIndex>(!a_index);
						auto currentCopiedSpell = GetCopiedMagic(currentPlaceholderIndex);
						// If the placeholder spell will change when the requested spell is copied over or if not already equipped, equip the spell.
						shouldEquip = (a_placeholderMagicChanged) || (a_form != currentCopiedSpell || currentForm != GetPlaceholderMagic(currentPlaceholderIndex));
					}
				}

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
					EquipAmmo(a_form);
				}
				else
				{
					UnequipAmmo(a_form);
				}

				break;
			}
			case RE::FormType::Shout:
			{
				if (auto currentVoiceMag = equippedForms[!EquipIndex::kVoice]; a_form != currentVoiceMag)
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
				if (auto asLight = a_form->As<RE::TESObjectLIGH>(); asLight && asLight->CanBeCarried())
				{
					if (auto currentLHForm = equippedForms[!EquipIndex::kLeftHand]; a_form != currentLHForm)
					{
						EquipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
					}
					else
					{
						UnequipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
					}
				}
			}
			default:
			{
				if (auto boundObj = a_form->As<RE::TESBoundObject>(); boundObj)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						// Just equip, do not update desired equipped forms list since there is no slot for it.
						aem->EquipObject(coopActor.get(), boundObj, nullptr, 1, nullptr, false, true, false, true);
					}
				}

				break;
			}
			}
		}
	}

	bool EquipManager::HasCyclableSpellInCategory(const FavMagicCyclingCategory& a_category)
	{
		// Does the given favorited spell category have a cyclable, equipable spell?

		logger::debug("[EM] HasCyclableSpellInCategory: {}: category: {}.", 
			coopActor->GetName(), 
			a_category);

		// Invalid category.
		if (a_category == FavMagicCyclingCategory::kNone || a_category == FavMagicCyclingCategory::kTotal)
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
			if (spell = spellForm->As<RE::SpellItem>(); spell)
			{
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
					logger::error("[EM] ERR: HasCyclableSpellInCategory: {}: default case for {}.", coopActor->GetName(), spell->GetName());
					return false;
				}
				}
			}
		}

		return false;
	}

	bool EquipManager::HasCyclableWeaponInCategory(const FavWeaponCyclingCategory& a_category, const bool& a_rightHand)
	{
		// Does the given favorited weapon category have a cyclable, equipable weapon for the given hand?

		logger::debug("[EM] HasCyclableWeaponInCategory: {}: category: {}, right hand: {}.", 
			coopActor->GetName(),
			a_category, 
			a_rightHand);

		// Invalid category.
		if (a_category == FavWeaponCyclingCategory::kNone || a_category == FavWeaponCyclingCategory::kTotal)
		{
			return false;
		}

		// All favorites: only has a spell if at least one physical, hand-equipable form is favorited.
		if (a_category == FavWeaponCyclingCategory::kAllFavorites)
		{
			return !cyclableFormsMap[CyclableForms::kWeapon].empty();
		}

		const auto& formsList = cyclableFormsMap[CyclableForms::kWeapon];
		RE::TESObjectWEAP* weapon = nullptr;
		for (const auto form : formsList)
		{
			auto equipType = form->As<RE::BGSEquipType>();
			// Incompatible equip slot.
			bool isShield = form->As<RE::TESObjectARMO>() && form->As<RE::TESObjectARMO>()->IsShield();
			bool isTorch = form->As<RE::TESObjectLIGH>() && form->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry);
			if (a_rightHand && (isShield || isTorch))
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
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandAxe || weapon->HasKeywordString("WeapTypeWarAxe"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kBattleaxe:
				{
					// Two handed axe WEAPON_TYPE includes both battleaxes and warhammers.
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && weapon->HasKeywordString("WeapTypeBattleaxe"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kBow:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow || weapon->HasKeywordString("WeapTypeBow"sv))
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
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandDagger || weapon->HasKeywordString("WeapTypeDagger"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kGreatsword:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandSword || weapon->HasKeywordString("WeapTypeGreatsword"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kMace:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandMace || weapon->HasKeywordString("WeapTypeMace"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kShieldAndTorch:
				{
					continue;
				}
				case FavWeaponCyclingCategory::kStaff:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff || weapon->HasKeywordString("WeapTypeStaff"sv))
					{
						return true;
					}

					continue;
				}
				case FavWeaponCyclingCategory::kSword:
				{
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandSword || weapon->HasKeywordString("WeapTypeSword"sv))
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
					if (weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe && weapon->HasKeywordString("WeapTypeWarhammer"sv))
					{
						return true;
					}

					continue;
				}
				default:
				{
					logger::error("[EM] ERR: HasCyclableWeaponInCategory: {}: default case for {}.", coopActor->GetName(), weapon->GetName());
					return false;
				}
				}
			}
			else if ((isShield || isTorch) && (a_category == FavWeaponCyclingCategory::kAllFavorites ||  a_category == FavWeaponCyclingCategory::kShieldAndTorch))
			{
				return true;
			}
		}

		return false;
	}

	void EquipManager::ImportCoopFavorites()
	{
		// Import this companion player's favorited items/magic onto P1.
		
		// Obviously should not be called on P1.
		if (p->isPlayer1)
		{
			return;
		}

		logger::debug("[EM] ImportCoopFavorites: {}.", coopActor->GetName());

		if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			auto p1Favorites = glob.coopPlayers[glob.player1CID]->em->GetFavoritedPhysForms(true);
			const auto& favorites = GetFavoritedPhysForms(true);
			favoritesIndicesInCommon.clear();
			favoritedItemWasAdded.clear();
			favoritesIndicesInCommon = std::vector<bool>(favorites.size(), false);
			favoritedItemWasAdded = std::vector<bool>(favorites.size(), false);
			for (auto i = 0; i < favorites.size(); ++i)
			{
				const auto form = favorites[i];
				if (Util::IsFavorited(p1, form))
				{
					// P1 has also favorited the item, so no need to add to their inventory or change its favorites status.
					favoritesIndicesInCommon[i] = true;
				}
				else if (form->IsBoundObject())
				{
					auto boundObj = form->As<RE::TESBoundObject>();
					auto invCounts = p1->GetInventoryCounts();
					// Not already favorited by P1 and P1 does not have the form,
					// so add the favorited form to player 1 before favoriting it.
					// Tag this favorited form as added, so it can be removed
					// when P1's favorites are restored.
					if (!invCounts.contains(boundObj) || invCounts.at(boundObj) <= 0) 
					{
						p1->AddObjectToContainer(form->As<RE::TESBoundObject>(), nullptr, 1, p1);
						favoritedItemWasAdded[i] = true;
					}

					Util::ChangeFormFavoritesStatus(p1, form, true);
				}
			}

			for (auto i = 0; i < p1Favorites.size(); ++i)
			{
				const auto form = p1Favorites[i];
				if (!favoritedFormIDs.contains(form->formID))
				{
					// Favorited by P1 but not by this player, so have P1 unfavorite the item.
					Util::ChangeFormFavoritesStatus(p1, form, false);
				}
			}
		}
	}

	void EquipManager::ReEquipAll(bool a_refreshBeforeEquipping)
	{
		// Re-equip all this player's desired forms.
		// Can also refresh the cached equip state before re-equipping.

		logger::debug("[EM] ReEquipAll: {}.", coopActor->GetName());

		// Refresh all equipped forms before re-equipping.
		if (a_refreshBeforeEquipping)
		{
			RefreshEquipState(RefreshSlots::kAll);
		}

		auto inventoryCounts = coopActor->GetInventoryCounts();
		for (auto i = 0; i < desiredEquippedForms.size(); ++i)
		{
			RE::TESForm* item = desiredEquippedForms[i];
			// Do not include items without an editor name defined in the CK,
			// such as the "SkinNaked" armor. 
			// These items typically do not have a full name and only an editor ID.
			if (item && strlen(item->GetName()) > 0)
			{
				EquipIndex currentSlot = EquipIndex::kTotal;
				if (i < !EquipIndex::kWeapMagTotal)
				{
					currentSlot = static_cast<EquipIndex>(i);
				}

				// Do not equip two handed weapons/spells twice,
				// so skip over RH item if it is the same as the earlier-equipped LH item.
				if (currentSlot == EquipIndex::kRightHand)
				{
					auto lhObj = desiredEquippedForms[!EquipIndex::kLeftHand];
					if (lhObj == item && item->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot)
					{
						continue;
					}
				}

				// Add item back to co-op player's inventory if missing.
				if (auto boundObj = item->IsBoundObject() ? item->As<RE::TESBoundObject>() : nullptr; boundObj)
				{
					// Getting the inventory count for a light source with a valid bound object crashes the game;
					// exclude it from the check, along with forms that are not bound objects (spells, shouts, etc.).
					if (!inventoryCounts.contains(boundObj) && !item->Is(RE::FormType::Spell, RE::FormType::Shout, RE::FormType::Light))
					{
						logger::debug("[EM] ReEquipAll: {}: does not have {} according to inventory counts. Adding 1 back.",
							coopActor->GetName(), boundObj->GetName());
						//std::addressof(coopActor->extraList)
						coopActor->AddObjectToContainer(boundObj, (RE::ExtraDataList*)nullptr, 1, coopActor.get());
					}
				}

				switch (*item->formType)
				{
				case RE::FormType::Ammo:
				{
					if (auto currentAmmo = coopActor->GetCurrentAmmo(); item != currentAmmo)
					{
						EquipAmmo(item);
					}

					break;
				}
				case RE::FormType::Armature:
				case RE::FormType::Armor:
				{
					RE::TESObjectARMO* currentArmorInSlot = nullptr;
					if (auto asBipedObjForm = item->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
					{
						currentArmorInSlot = coopActor->GetWornArmor(asBipedObjForm->GetSlotMask());
					}

					if (item != currentArmorInSlot)
					{
						EquipArmor(item);
					}

					break;
				}
				case RE::FormType::Shout:
				{
					EquipShout(item);
					break;
				}
				case RE::FormType::Spell:
				{
					// Quick slot spell is not equipped by the game,
					// as it is cast directly on demand.
					if (i != !EquipIndex::kQuickSlotSpell)
					{
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
								equipSlot = (i == !EquipIndex::kLeftHand) ? glob.leftHandEquipSlot : glob.rightHandEquipSlot;
							}

							// Directly equip, if player 1.
							if (p->isPlayer1)
							{
								EquipSpell(spell, (i == !EquipIndex::kLeftHand) ? EquipIndex::kLeftHand : EquipIndex::kRightHand, equipSlot);
							}
							else
							{
								// Copy to placeholder spell before equipping.
								if (equipSlot == glob.bothHandsEquipSlot)
								{
									spell = copiedMagic[!PlaceholderMagicIndex::k2H] ? copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() : nullptr;
									if (spell)
									{
										EquipSpell(CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::k2H), EquipIndex::kRightHand, equipSlot);
									}
								}
								else
								{
									if (i == !EquipIndex::kRightHand)
									{
										spell = copiedMagic[!PlaceholderMagicIndex::kRH] ? copiedMagic[!PlaceholderMagicIndex::kRH]->As<RE::SpellItem>() : nullptr;
									}
									else
									{
										spell = copiedMagic[!PlaceholderMagicIndex::kLH] ? copiedMagic[!PlaceholderMagicIndex::kLH]->As<RE::SpellItem>() : nullptr;
									}

									EquipIndex index = (i == !EquipIndex::kLeftHand) ? EquipIndex::kLeftHand : EquipIndex::kRightHand;
									if (spell)
									{
										EquipSpell(CopyToPlaceholderSpell(spell, index == EquipIndex::kRightHand ? PlaceholderMagicIndex::kRH : PlaceholderMagicIndex::kLH), index, equipSlot);
									}
								}
							}
						}
						else
						{
							auto spell = item->As<RE::SpellItem>();
							auto equipSlot = glob.voiceEquipSlot;
							EquipSpell(CopyToPlaceholderSpell(spell, PlaceholderMagicIndex::kVoice), EquipIndex::kVoice, equipSlot);
						}
					}

					break;
				}
				case RE::FormType::Weapon:
				{
					auto lhObj = coopActor->GetEquippedObject(true);
					auto rhObj = coopActor->GetEquippedObject(false);
					// Do not equip 2H weapons twice.
					if ((i == !EquipIndex::kLeftHand && item != lhObj) || (i == !EquipIndex::kRightHand && item != rhObj))
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

						EquipForm(item, (i == !EquipIndex::kLeftHand) ? EquipIndex::kLeftHand : EquipIndex::kRightHand, (RE::ExtraDataList*)nullptr, 1, equipSlot);

						if (Settings::uAmmoAutoEquipMode != !AmmoAutoEquipMode::kNone) 
						{
							if (auto weap = item->As<RE::TESObjectWEAP>(); weap && weap->IsRanged() && !weap->IsStaff() && !desiredEquippedForms[!EquipIndex::kAmmo])
							{
								auto ammoAndCount = Settings::uAmmoAutoEquipMode == !AmmoAutoEquipMode::kHighestDamage ? 
													Util::GetHighestDamageAmmo(coopActor.get(), weap->IsBow()) :
													Util::GetHighestCountAmmo(coopActor.get(), weap->IsBow());
								if (ammoAndCount.first)
								{
									auto ammo = ammoAndCount.first;
									EquipAmmo(ammo);
								}
							}
						}
					}

					break;
				}
				default:
				{
					break;
				}
				}
			}
		}
	}

	void EquipManager::ReEquipHandForms()
	{
		// Re-equip desired forms in this player's hands.

		logger::debug("[EM] ReEquipHandForms: {}.", coopActor->GetName());
		// Interrupts Vampire Lord levitation, and Werewolves have no equipped items, so return here.
		if (p->isTransformed)
		{
			return;
		}

		// Unequip to clear out hand slots before re-equipping.
		auto lhForm = desiredEquippedForms[!EquipIndex::kLeftHand];
		auto rhForm = desiredEquippedForms[!EquipIndex::kRightHand];
		auto equipSlot = glob.eitherHandEquipSlot;
		auto lhEquipType = lhForm ? lhForm->As<RE::BGSEquipType>() : nullptr;
		auto rhEquipType = rhForm ? rhForm->As<RE::BGSEquipType>() : nullptr;

		UnequipHandForms(glob.bothHandsEquipSlot);
		if (rhForm)
		{
			if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
			{
				equipSlot = GetEquipSlotForForm(rhForm, EquipIndex::kRightHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
					{
						aem->EquipSpell(p->coopActor.get(), rhSpell, equipSlot);
					}
				}
				else
				{
					// Copy to placeholder spell, if needed.
					if (rhSpell == placeholderMagic[!PlaceholderMagicIndex::kRH] || rhSpell == placeholderMagic[!PlaceholderMagicIndex::k2H])
					{
						EquipSpell(rhSpell, EquipIndex::kRightHand, equipSlot);
					}
					else
					{
						if (equipSlot == glob.bothHandsEquipSlot)
						{
							EquipSpell(CopyToPlaceholderSpell(rhSpell, PlaceholderMagicIndex::k2H), EquipIndex::kRightHand, equipSlot);
						}
						else
						{
							EquipSpell(CopyToPlaceholderSpell(rhSpell, PlaceholderMagicIndex::kRH), EquipIndex::kRightHand, equipSlot);
						}
					}
				}
			}
			else if (auto asBipedObjForm = rhForm->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				if (p->isPlayer1) 
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject(p->coopActor.get(), rhForm->As<RE::TESBoundObject>());
					}
				}
				else
				{
					EquipArmor(rhForm);
				}
			}
			else
			{
				equipSlot = GetEquipSlotForForm(rhForm, EquipIndex::kRightHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject(p->coopActor.get(), rhForm->As<RE::TESBoundObject>(), nullptr, 1, equipSlot);
					}
				}
				else
				{
					EquipForm(rhForm, EquipIndex::kRightHand, nullptr, 1, equipSlot);
				}
			}
		}

		if (lhForm && (!lhEquipType || lhEquipType->equipSlot != glob.bothHandsEquipSlot))
		{
			if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell)
			{
				equipSlot = GetEquipSlotForForm(lhForm, EquipIndex::kLeftHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipSpell(p->coopActor.get(), lhSpell, equipSlot);
					}
				}
				else
				{
					// Copy to placeholder spell, if needed.
					if (lhSpell == placeholderMagic[!PlaceholderMagicIndex::kLH] || lhSpell == placeholderMagic[!PlaceholderMagicIndex::k2H])
					{
						EquipSpell(lhSpell, EquipIndex::kLeftHand, equipSlot);
					}
					else
					{
						if (equipSlot == glob.bothHandsEquipSlot)
						{
							EquipSpell(CopyToPlaceholderSpell(lhSpell, PlaceholderMagicIndex::k2H), EquipIndex::kLeftHand, equipSlot);
						}
						else
						{
							EquipSpell(CopyToPlaceholderSpell(lhSpell, PlaceholderMagicIndex::kLH), EquipIndex::kLeftHand, equipSlot);
						}
					}
				}
			}
			else if (auto asBipedObjForm = lhForm->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject(p->coopActor.get(), lhForm->As<RE::TESBoundObject>());
					}
				}
				else
				{
					EquipArmor(lhForm);
				}
			}
			else
			{
				equipSlot = GetEquipSlotForForm(lhForm, EquipIndex::kLeftHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject(p->coopActor.get(), lhForm->As<RE::TESBoundObject>(), nullptr, 1, equipSlot);
					}
				}
				else
				{
					EquipForm(lhForm, EquipIndex::kLeftHand, nullptr, 1, equipSlot);
				}
			}
		}
	}

	void EquipManager::ReEquipVoiceForm()
	{
		// Re-equip the player's desired voice magic form.

		logger::debug("[EM] ReEquipVoiceForm: {}.", coopActor->GetName());
		UnequipFormAtIndex(EquipIndex::kVoice);
		if (auto toEquip = desiredEquippedForms[!EquipIndex::kVoice]; toEquip) 
		{
			if (toEquip->As<RE::TESShout>())
			{
				EquipShout(toEquip);
			}
			else
			{
				EquipSpell(toEquip, EquipIndex::kVoice);
			}
		}
	}

	void EquipManager::RestoreP1Favorites()
	{
		// Restore P1's previously saved favorited items
		// after removing any companion player's favorites that are not in common.

		logger::debug("[EM] RestoreP1Favorites: {}.", coopActor->GetName());

		if (const auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			for (auto i = 0; i < favoritesIndicesInCommon.size(); ++i)
			{
				const auto form = favoritedPhysForms[i];
				// Remove co-op player-favorited items that P1 has not also favorited and does not have equipped.
				if (!favoritesIndicesInCommon[i] && form && form->IsBoundObject())
				{
					// If the co-op player favorited form was added previously, it should now be removed.
					if (favoritedItemWasAdded[i])
					{
						p1->RemoveItem(form->As<RE::TESBoundObject>(), 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
					
					Util::ChangeFormFavoritesStatus(p1, form, false);
				}
			}

			// Re-favorite all of P1's cached favorited forms.
			auto p1Favorites = glob.coopPlayers[glob.player1CID]->em->GetFavoritedPhysForms(false);
			for (auto form : p1Favorites)
			{
				Util::ChangeFormFavoritesStatus(p1, form, true);
			}
		}
	}

	void EquipManager::RefreshEquipState(const RefreshSlots& a_slots, RE::TESForm* a_formEquipped, bool a_isEquipped)
	{
		// Refresh all equipped gear for this player in the given slots.
		// For specific equip/unequip events, the form of interest is given, 
		// as well as whether or not it is now equipped.

		logger::debug("[EM] RefreshEquipState: {}: slots to refresh: {}, form: {}, is equipped: {}.", 
			coopActor->GetName(),
			a_slots,
			a_formEquipped ? a_formEquipped->GetName() : "NONE",
			a_isEquipped);

		const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
		// REMOVE
		logger::debug("[EM] RefreshEquipState: {}: Try to lock: 0x{:X}.", coopActor->GetName(), hash);
		{
			std::unique_lock<std::mutex> lock(equipStateMutex, std::try_to_lock);
			if (lock)
			{
				// REMOVE
				logger::debug("[EM] RefreshEquipState: {}: Lock obtained: 0x{:X}.", coopActor->GetName(), hash);

				// Clear out cached equipped forms first.
				equippedForms.fill(nullptr);
				if (a_slots == RefreshSlots::kWeapMag || a_slots == RefreshSlots::kAll)
				{
					// Get L, R hand objects, shout, and ammo
					auto lhObj = coopActor->GetEquippedObject(true);
					auto rhObj = coopActor->GetEquippedObject(false);
					logger::debug("[EM] RefreshEquipState: {}: LH: {}, RH: {}",
						coopActor->GetName(),
						(lhObj) ? lhObj->GetName() : "NONE",
						(rhObj) ? rhObj->GetName() : "NONE");
					auto ammo = coopActor->GetCurrentAmmo();
					logger::debug("[EM] RefreshEquipState: {}: Current ammo: {}.", coopActor->GetName(), ammo ? ammo->GetName() : "NONE");

					// Could be indicative of equip state bug that causes choppy/mismatching attack animations and wonky hitboxes
					// (requires full player reset to fix).
					// Seems to occur when a 2H weapon is equipped to only one hand. Repro when reloading another save where
					// the player had a 2H weapon equipped. The game does not equip it properly when the player is summoned.
					// Did not see this bug in Enderal SSE.
					bool onlyLHHas2HWeap = (!rhObj && lhObj && lhObj->As<RE::TESObjectWEAP>() && lhObj->As<RE::TESObjectWEAP>()->equipSlot == glob.bothHandsEquipSlot);
					bool onlyRHHas2HWeap = (!lhObj && rhObj && rhObj->As<RE::TESObjectWEAP>() && rhObj->As<RE::TESObjectWEAP>()->equipSlot == glob.bothHandsEquipSlot);
					if (onlyLHHas2HWeap || onlyRHHas2HWeap)
					{
						logger::critical("[EM] RefreshEquipState: {}: 2H stuttering equip state bug is likely since the 2H weapon {} is only in the {}. Fix the bug temporarily by using the Debug Menu or the 'DebugResetPlayer' bind.",
							coopActor->GetName(), onlyLHHas2HWeap ? lhObj->GetName() : onlyRHHas2HWeap ? rhObj->GetName() :
																										   "ERROR",
							onlyLHHas2HWeap ? "LH" : onlyRHHas2HWeap ? "RH" :
																		 "ERROR");
					}

					// Disable combat aim correction on arrows/bolts.
					if (ammo)
					{
						// REMOVE
						logger::debug("[EM] RefreshEquipState: {}: {}'s ammo flags: {}.", coopActor->GetName(), ammo->GetName(), *ammo->data.flags);
						auto projectile = ammo->data.projectile;
						if (projectile)
						{
							projectile->data.flags.set(RE::BGSProjectileData::BGSProjectileFlags::kDisableCombatAimCorrection);
						}
					}

					// Sometimes the call to GetCurrentAmmo() still returns the unequipped ammo that triggered this equip state refresh.
					// Clear out the cached current ammo, which will then properly correspond to the actual equip state after the unequip call fully completes.
					if (ammo && a_formEquipped == ammo && !a_isEquipped)
					{
						logger::debug("[EM] RefreshEquipState: {}: game unequipped ammo {}, but the GetCurrentAmmo() call still returns this ammo. Clearing cached ammo.",
							coopActor->GetName(), ammo->GetName());
						ammo = nullptr;
					}

					// And sometimes the call to GetCurrentAmmo() returns nullptr while this event fires upon equipping ammo.
					// Similarly update the cached current ammo to reflect the post-equip event state.
					if (!ammo && a_formEquipped && a_formEquipped->As<RE::TESAmmo>() && a_isEquipped)
					{
						logger::debug("[EM] RefreshEquipState: {}: ammo equip event fired for {}, but the GetCurrentAmmo() call returns nothing. Set {} as cached ammo.",
							coopActor->GetName(), a_formEquipped->As<RE::TESAmmo>()->GetName(), a_formEquipped->As<RE::TESAmmo>()->GetName());
						ammo = a_formEquipped->As<RE::TESAmmo>();
					}

					// Initially set as previous desired voice form.
					voiceForm = !desiredEquippedForms.empty() ? desiredEquippedForms[!EquipIndex::kVoice] : nullptr;
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
								isVoiceSpell = spellType == RE::MagicSystem::SpellType::kVoicePower ||
											   spellType == RE::MagicSystem::SpellType::kPower ||
											   spellType == RE::MagicSystem::SpellType::kLesserPower;
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

					// Update Actor data spells, voice form to match equipped shout, and set the highest shout variation spell.
					SetCurrentVoiceSpell();

					// Ensure each equipped hand spell holds a valid placeholder spell before caching.
					if (!p->isPlayer1)
					{
						bool is2HSpell = false;
						if (auto lhSpell = lhObj ? lhObj->As<RE::SpellItem>() : nullptr; lhSpell)
						{
							is2HSpell = lhSpell->equipSlot == glob.bothHandsEquipSlot;
							lhObj = CopyToPlaceholderSpell(lhSpell, is2HSpell ? PlaceholderMagicIndex::k2H : PlaceholderMagicIndex::kLH);
						}

						if (auto rhSpell = rhObj ? rhObj->As<RE::SpellItem>() : nullptr; rhSpell)
						{
							is2HSpell = rhSpell->equipSlot == glob.bothHandsEquipSlot;
							rhObj = CopyToPlaceholderSpell(rhSpell, is2HSpell ? PlaceholderMagicIndex::k2H : PlaceholderMagicIndex::kRH);
						}
					}

					// Must equip highest count/damage ammo for co-op companions since the ranged package,
					// which used to automatically equip the appropriate ammo, is no longer used for firing ranged weapons.
					if (Settings::uAmmoAutoEquipMode != !AmmoAutoEquipMode::kNone)
					{
						if (a_isEquipped && a_formEquipped && a_formEquipped->IsWeapon())
						{
							auto weap = a_formEquipped->As<RE::TESObjectWEAP>();
							bool isBound = weap->IsBound();
							bool isBow = weap->IsBow();
							bool isCrossbow = weap->IsCrossbow();
							// We already equip bound arrows elsewhere when a bound bow is equipped.
							if (!isBound && (isBow || isCrossbow))
							{
								auto desiredAmmo = desiredEquippedForms[!EquipIndex::kAmmo] ? desiredEquippedForms[!EquipIndex::kAmmo]->As<RE::TESAmmo>() : nullptr;
								// Only auto equip if no current ammo, mismatching current ammo, or if equipping a bound bow.
								if (!desiredAmmo || ((desiredAmmo->IsBolt() && isBow) || (!desiredAmmo->IsBolt() && isCrossbow)))
								{
									if (auto currentAmmo = coopActor->GetCurrentAmmo(); currentAmmo && currentAmmo->IsBoundObject())
									{
										UnequipAmmo(currentAmmo);
									}

									auto ammoAndCount = Settings::uAmmoAutoEquipMode == !AmmoAutoEquipMode::kHighestCount ?
															Util::GetHighestCountAmmo(coopActor.get(), isBow) :
															  Util::GetHighestDamageAmmo(coopActor.get(), isBow);
									if (ammoAndCount.first && coopActor->GetCurrentAmmo() != ammoAndCount.first)
									{
										ammo = ammoAndCount.first;
									}

									if (ammo)
									{
										EquipAmmo(ammo);
									}
								}
							}
						}
					}

					logger::debug("[EM] RefreshEquipState: {}: Voice magic: {} (formType: 0x{:X}), Ammo: {}",
						coopActor->GetName(),
						voiceForm ? voiceForm->GetName() : "NONE",
						voiceForm ? *voiceForm->formType : RE::FormType::None,
						ammo ? ammo->GetName() : "NONE");

					// Set weapon/magic slot forms.
					equippedForms[!EquipIndex::kLeftHand] = lhObj;
					equippedForms[!EquipIndex::kRightHand] = rhObj;
					equippedForms[!EquipIndex::kAmmo] = ammo;
					equippedForms[!EquipIndex::kVoice] = voiceForm;
					equippedForms[!EquipIndex::kQuickSlotItem] = quickSlotItem;
					equippedForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;
					// Keep in sync with equipped forms since these two forms are not actually equipped to a slot
					// and are cast immediately with the co-op actor's instant magic caster or consumed when the quick slot item
					// bind is pressed.
					desiredEquippedForms[!EquipIndex::kQuickSlotItem] = quickSlotItem;
					desiredEquippedForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;

					logger::debug("[EM] RefreshEquipState: {}: quick slot spell, item: {}, {}",
						coopActor->GetName(),
						(quickSlotSpell) ? quickSlotSpell->GetName() : "NONE",
						(quickSlotItem) ? quickSlotItem->GetName() : "NONE");

					// NOTE: Not used right now, but may be enabled in the future.
					/*
					// Set original weapon type(s) for LH/RH/2H weapon.
					// 2H.
					if (lhObj && lhObj->IsWeapon() && rhObj && lhObj == rhObj)
					{
						SetOriginalWeaponTypeFromKeyword(HandIndex::k2H);
					}
					else
					{
						// LH.
						if (lhObj && lhObj->IsWeapon())
						{
							SetOriginalWeaponTypeFromKeyword(HandIndex::kLH);
						}

						// RH.
						if (rhObj && rhObj->IsWeapon())
						{
							SetOriginalWeaponTypeFromKeyword(HandIndex::kRH);
						}
					}
					*/
				}

				if (a_slots == RefreshSlots::kArmor || a_slots == RefreshSlots::kAll)
				{
					// Clear cached armor ratings.
					armorRatings.first = armorRatings.second = 0.0f;
					// Get armor in each biped slot
					std::set<RE::FormID> equippedFormIDs;
					for (uint32_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
					{
						auto armorInSlot = coopActor->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1 << (i - !EquipIndex::kFirstBipedSlot)));
						if (armorInSlot)
						{
							// NOTE: Leaving this for now just in case the bug re-surfaces.
							// Have to handle the odd case where the unequip event for a piece of armor fires,
							// but the game hasn't finished unequipping it yet here.
							if (armorInSlot != a_formEquipped || a_isEquipped)
							{
								// Either the armor in this slot is not the equipped armor sent from the equip event,
								// or the armor in this slot is the equip event armor and it is being equipped.
								logger::debug("[EM] {} has {} (armor) equipped in the {} biped slot. Equipped forms index: {}",
									coopActor->GetName(), armorInSlot->GetName(), (i - !EquipIndex::kFirstBipedSlot), i);

								if (armorInSlot->IsLightArmor())
								{
									armorRatings.first += armorInSlot->GetArmorRating();
								}
								else
								{
									armorRatings.second += armorInSlot->GetArmorRating();
								}

								equippedForms[i] = armorInSlot;
							}
							else
							{
								// The sent form was unequipped but is still in the biped armor slot (yet to be removed), so remove from the equipped list.
								logger::debug("[EM] {} has nothing equipped in the {} biped slot since {} was just unequipped. Equipped forms index: {}",
									coopActor->GetName(), (i - !EquipIndex::kFirstBipedSlot),
									armorInSlot ? armorInSlot->GetName() : "NONE", i);
								equippedForms[i] = nullptr;
							}
						}
						else
						{
							equippedForms[i] = nullptr;
						}
					}

					logger::debug("[EM] RefreshEquipState: {}: new armor ratings for light/heavy armor are: {}, {}",
						coopActor->GetName(), armorRatings.first, armorRatings.second);
				}

				// If P1, serialize desired list of equipped forms, since the game does not auto-equip forms on cell or inventory change for P1.
				bool mismatch = false;
				if (p->isPlayer1)
				{
					if (auto midHighProc = coopActor->currentProcess ? coopActor->currentProcess->middleHigh : nullptr; midHighProc)
					{
						auto queuedEquip = midHighProc->itemstoEquipUnequip;
						while (queuedEquip) 
						{
							logger::debug("[EM] RefreshEquipState: {}: Queued {} {}.",
								coopActor->GetName(), 
								queuedEquip->equip ? "equip" : "unequip",
								queuedEquip->object ? queuedEquip->object->GetName() : "NONE");
							queuedEquip = queuedEquip->next;
						}
					
					}
					// Fists count as empty equipped form slots.
					// Also, do not saved bound weapons/ammo.
					auto lhForm = equippedForms[!EquipIndex::kLeftHand];
					bool lhFormIsBound = lhForm && lhForm->As<RE::TESObjectWEAP>() && lhForm->As<RE::TESObjectWEAP>()->IsBound();
					auto rhForm = equippedForms[!EquipIndex::kRightHand];
					bool rhFormIsBound = rhForm && rhForm->As<RE::TESObjectWEAP>() && rhForm->As<RE::TESObjectWEAP>()->IsBound();
					if (lhForm == glob.fists)
					{
						desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
					}
					else if (!lhFormIsBound)
					{
						desiredEquippedForms[!EquipIndex::kLeftHand] = lhForm;
					}

					if (rhForm == glob.fists)
					{
						desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
					}
					else if (!rhFormIsBound)
					{
						desiredEquippedForms[!EquipIndex::kRightHand] = rhForm;
					}

					if (bool ammoIsBound = equippedForms[!EquipIndex::kAmmo] && equippedForms[!EquipIndex::kAmmo]->HasKeywordByEditorID("WeapTypeBoundArrow"); !ammoIsBound)
					{
						desiredEquippedForms[!EquipIndex::kAmmo] = equippedForms[!EquipIndex::kAmmo];
					}

					// Copy over the rest.
					for (uint8_t i = !EquipIndex::kAmmo + 1; i < !EquipIndex::kTotal; ++i)
					{
						desiredEquippedForms[i] = equippedForms[i];
					}
				}
				else
				{
					// Find a mismatch and break.
					for (uint8_t i = 0; i < !EquipIndex::kTotal; ++i)
					{
						if (mismatch = equippedForms[i] != desiredEquippedForms[i]; mismatch)
						{
							// REMOVE after debugging.
							logger::debug("[EM] RefreshEquipState: {}: MISMATCH at index {}: equipped {} vs. should have equipped {}.",
								coopActor->GetName(), i,
								equippedForms[i] ? equippedForms[i]->GetName() : "NOTHING",
								desiredEquippedForms[i] ? desiredEquippedForms[i]->GetName() : "NOTHING");
							break;
						}
					}

					// Signal menu input manager to refresh equip state if it is currently running.
					if (glob.mim->IsRunning())
					{
						glob.mim->SignalRefreshFavMagMenuEquipState();
					}
				}

				auto& serializableEquippedForms = glob.serializablePlayerData.at(coopActor->formID)->equippedForms;
				// Save to serializable data if both equipped item lists are equivalent.
				if (!mismatch)
				{
					serializableEquippedForms = std::vector<RE::TESForm*>(!EquipIndex::kTotal, nullptr);
					std::copy(equippedForms.begin(), equippedForms.end(), serializableEquippedForms.begin());
					logger::debug("[EM] RefreshEquipState: {}: copying list to serializable equipped forms list. LISTS MATCH. New equipped forms size: {}", coopActor->GetName(), serializableEquippedForms.size());
				}

				// REMOVE the following prints after fully debugging.
				/*
				for (auto item : equippedForms)
				{
					if (item)
					{
						logger::debug("[EM] RefreshAllEquippedItems: {} has a(n) {} in EQUIPPED forms list.", coopActor->GetName(), item->GetName());
					}
				}

				// REMOVE
				for (auto item : desiredEquippedForms)
				{
					if (item)
					{
						logger::debug("[EM] RefreshAllEquippedItems: {} has a(n) {} in DESIRED EQUIPPED forms list.", coopActor->GetName(), item->GetName());
					}
				}

				// REMOVE
				for (auto item : serializableEquippedForms)
				{
					if (item)
					{
						logger::debug("[EM] RefreshAllEquippedItems: {} has a(n) {} in SERIALIZABLE EQUIPPED forms list.", coopActor->GetName(), item->GetName());
					}
				}
				*/
			}
			else
			{
				// REMOVE
				logger::error("[EM] RefreshEquipState: {}: Failed to obtain lock for dialogue control CID: 0x{:X}.", coopActor->GetName(), hash);
			}
		}
		
	}

	void EquipManager::SetCopiedMagicAndFID(RE::TESForm* a_formToCopy, const PlaceholderMagicIndex& a_index)
	{
		// Save the spell form (and its FID) copied to the placeholder spell at the given index.

		logger::debug("[EM] SetCopiedMagicAndFID: {}: form: {}, index: {}.", 
			coopActor->GetName(),
			a_formToCopy ? a_formToCopy->GetName() : "NONE",
			a_index);

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

		logger::debug("[EM] SetFavoritedEmoteIdles: {}.", coopActor->GetName());

		favoritedEmoteIdles = GlobalCoopData::DEFAULT_CYCLABLE_EMOTE_IDLE_EVENTS;
		favoritedEmoteIdlesHashes.clear();
		for (auto i = 0; i < a_emoteIdlesList.size() && i < favoritedEmoteIdles.size(); ++i) 
		{
			favoritedEmoteIdles[i] = a_emoteIdlesList[i];
			favoritedEmoteIdlesHashes.insert(Hash(a_emoteIdlesList[i]));
		}

		// Update current idle if a new idle was assigned to the current cycled index.
		// First: event name.
		// Second: index in favorited idles list.
		if (currentCycledIdleIndexPair.second < favoritedEmoteIdles.size() && currentCycledIdleIndexPair.first != favoritedEmoteIdles[currentCycledIdleIndexPair.second])
		{
			currentCycledIdleIndexPair.first = favoritedEmoteIdles[currentCycledIdleIndexPair.second];
		}

		glob.serializablePlayerData.at(coopActor->formID)->cyclableEmoteIdleEvents = favoritedEmoteIdles;
	}

	void EquipManager::SetInitialEquipState()
	{
		// Update initial equip state after refreshing data and before the equip manager starts.

		logger::debug("[EM] SetInitialEquipState: {}.", coopActor->GetName());

		auto& savedEquippedForms = glob.serializablePlayerData.at(coopActor->formID)->equippedForms;
		if (savedEquippedForms.empty() || savedEquippedForms.size() == 0 || savedEquippedForms.size() != desiredEquippedForms.size())
		{
			logger::debug("[EM] ERR: SetInitialEquipState: {}: saved equipped forms list is {} ({}).",
				coopActor->GetName(), savedEquippedForms.empty() || savedEquippedForms.size() == 0 ? "empty" : "not the right size", savedEquippedForms.size());
		}
		else
		{
			// Explicitly copy the saved equipped forms list into the desired forms list.
			std::copy(savedEquippedForms.begin(), savedEquippedForms.end(), desiredEquippedForms.begin());
		}

		// Ensure the placeholder spells hold a valid copied spell before adding back to desired equipped forms.
		copiedMagic = glob.serializablePlayerData.at(coopActor->formID)->copiedMagic;
		auto lhObj = desiredEquippedForms[!EquipIndex::kLeftHand];
		auto rhObj = desiredEquippedForms[!EquipIndex::kRightHand];
		auto voiceObj = desiredEquippedForms[!EquipIndex::kVoice];
		if (!p->isPlayer1)
		{
			bool is2HSpell = false;
			RE::SpellItem* copiedSpell = nullptr;
			if (auto lhSpell = lhObj ? lhObj->As<RE::SpellItem>() : nullptr; lhSpell)
			{
				is2HSpell = lhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					copiedSpell = copiedMagic[!PlaceholderMagicIndex::k2H] && copiedMagic[!PlaceholderMagicIndex::k2H]->Is(RE::FormType::Spell) ?
								  copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
								  nullptr;
					desiredEquippedForms[!EquipIndex::kLeftHand] = CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::k2H);
				}
				else
				{
					copiedSpell = copiedMagic[!PlaceholderMagicIndex::kLH] && copiedMagic[!PlaceholderMagicIndex::kLH]->Is(RE::FormType::Spell) ?
								  copiedMagic[!PlaceholderMagicIndex::kLH]->As<RE::SpellItem>() :
								  nullptr;
					desiredEquippedForms[!EquipIndex::kLeftHand] = CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kLH);
				}
			}

			if (auto rhSpell = rhObj ? rhObj->As<RE::SpellItem>() : nullptr; rhSpell)
			{
				is2HSpell = rhSpell->equipSlot == glob.bothHandsEquipSlot;
				if (is2HSpell)
				{
					copiedSpell = copiedMagic[!PlaceholderMagicIndex::k2H] && copiedMagic[!PlaceholderMagicIndex::k2H]->Is(RE::FormType::Spell) ?
								  copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
								  nullptr;
					desiredEquippedForms[!EquipIndex::kRightHand] = CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::k2H);
				}
				else
				{
					copiedSpell = copiedMagic[!PlaceholderMagicIndex::kRH] && copiedMagic[!PlaceholderMagicIndex::kRH]->Is(RE::FormType::Spell) ?
								  copiedMagic[!PlaceholderMagicIndex::kRH]->As<RE::SpellItem>() :
								  nullptr;
					desiredEquippedForms[!EquipIndex::kRightHand] = CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kRH);
				}
			}

			if (voiceObj)
			{
				// Make sure desired equipped form is the voice placeholder spell if the saved voice form is a spell.
				// Otherwise, directly set as the saved shout.
				if (voiceObj->As<RE::SpellItem>()) 
				{
					copiedSpell = copiedMagic[!PlaceholderMagicIndex::kVoice] && copiedMagic[!PlaceholderMagicIndex::kVoice]->Is(RE::FormType::Spell) ?
								  copiedMagic[!PlaceholderMagicIndex::kVoice]->As<RE::SpellItem>() :
								  nullptr;
					desiredEquippedForms[!EquipIndex::kVoice] = CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kVoice);
				}
				else
				{
					desiredEquippedForms[!EquipIndex::kVoice] = voiceObj;
				}
			}
		}

		// Set initial quick slot item and spell to saved forms here since they won't be set when refreshing equip state 
		// (not really equipped in a slot like the other forms).
		// Any modification to these two members afterward is done in the menu input manager when the player reassigns forms to these slots.
		// They get saved to the desired equipped forms list and serialized when refreshing the equip state.
		quickSlotItem = equippedForms[!EquipIndex::kQuickSlotItem] = desiredEquippedForms[!EquipIndex::kQuickSlotItem];
		auto spellForm = desiredEquippedForms[!EquipIndex::kQuickSlotSpell];
		quickSlotSpell = spellForm && spellForm->Is(RE::FormType::Spell) ? spellForm->As<RE::SpellItem>() : nullptr;
		equippedForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;

		// Update equipped forms list to reflect the current equip state for this player, which may be different from the saved equip state
		// copied into the desired equipped forms list.
		// Any mismatches between the current equip state and the desired equip state will be resolved in the main task.
		if (p->isPlayer1)
		{
			// Get player 1's equip state updated before the manager starts.
			RefreshEquipState(RefreshSlots::kAll);
		}
		else if (!p->isTransformed)
		{
			// NOTE:
			// Since there may be mismatches that need handling, all items are always re-equipped
			// when the equip manager starts.
			// This call will only clear out mismatching forms that are not in the desired forms list 
			// after passing through the UnequipObject() hook.
			Util::Papyrus::UnequipAll(coopActor.get());
		}
	}

	void EquipManager::SetCyclableFavForms(CyclableForms a_favFormType)
	{
		// Set cyclable lists of favorited forms 
		// (ammo, hand spells, voice powers/shouts, and weapons) of the given type.

		logger::debug("[EM] SetCyclableFavForms: {}.", coopActor->GetName());

		cyclableFormsMap.insert_or_assign(a_favFormType, std::vector<RE::TESForm*>());
		if (a_favFormType == CyclableForms::kAmmo || a_favFormType == CyclableForms::kWeapon)
		{
			auto inventory = coopActor->GetInventory();
			for (const auto& [boundObj, entryDataPair] : inventory)
			{
				if (boundObj && entryDataPair.first > 0 && entryDataPair.second.get())
				{
					auto exDataListList = entryDataPair.second->extraLists;
					if (exDataListList)
					{
						for (auto exDataList : *exDataListList)
						{
							if (exDataList && exDataList->HasType(RE::ExtraDataType::kHotkey))
							{
								// Weapons, shields, torches.
								if (a_favFormType == CyclableForms::kWeapon &&
									((*boundObj->formType == RE::FormType::Weapon) ||
										(boundObj->As<RE::TESObjectARMO>() && boundObj->As<RE::TESObjectARMO>()->IsShield()) ||
										(boundObj->As<RE::TESObjectLIGH>() && boundObj->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry))))
								{
									cyclableFormsMap[a_favFormType].push_back(boundObj);
								}
								else if (a_favFormType == CyclableForms::kAmmo && *boundObj->formType == RE::FormType::Ammo)
								{
									cyclableFormsMap[a_favFormType].push_back(boundObj);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			if (auto magicFavorites = RE::MagicFavorites::GetSingleton(); magicFavorites)
			{
				for (auto spellForm : magicFavorites->spells)
				{
					if (spellForm)
					{
						if (a_favFormType == CyclableForms::kVoice && spellForm->As<RE::TESShout>())
						{
							cyclableFormsMap[CyclableForms::kVoice].push_back(spellForm);
						}
						else if (spellForm->As<RE::SpellItem>())
						{
							auto spell = spellForm->As<RE::SpellItem>();
							auto spellType = spell->GetSpellType();

							bool isVoiceSlotSpell = spellType == RE::MagicSystem::SpellType::kVoicePower ||
													spellType == RE::MagicSystem::SpellType::kPower ||
													spellType == RE::MagicSystem::SpellType::kLesserPower;

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
		}

		// Remove duplicates.
		if (auto& favFormsList = cyclableFormsMap[a_favFormType]; !favFormsList.empty())
		{
			auto newEnd = std::unique(favFormsList.begin(), favFormsList.end());
			if (newEnd != favFormsList.end())
			{
				uint32_t prevSize = favFormsList.size();
				favFormsList.erase(newEnd, favFormsList.end());
			}
		}
	}

	void EquipManager::SetFavoritedForms(bool a_onlySetFIDs)
	{
		// Set lists of all favorited physical and magical forms, or just their FIDs.

		logger::debug("[EM] SetFavoritedForms: {}.", coopActor->GetName());

		favoritedPhysForms.clear();
		favoritedFormIDs.clear();
		cyclableFormsMap.clear();
		cyclableFormsMap[CyclableForms::kAmmo] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kSpell] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kVoice] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kWeapon] = std::vector<RE::TESForm*>();
		numFavoritedItems = 0;

		// Physical forms first.
		auto inventory = coopActor->GetInventory();
		for (auto& [boundObj, entryDataPair] : inventory)
		{
			if (boundObj && entryDataPair.first > 0 && entryDataPair.second.get())
			{
				auto exDataListList = entryDataPair.second->extraLists;
				if (exDataListList)
				{
					for (auto exDataList : *exDataListList)
					{
						if (exDataList && exDataList->HasType(RE::ExtraDataType::kHotkey))
						{
							favoritedFormIDs.insert(boundObj->formID);
							if (!a_onlySetFIDs)
							{
								favoritedPhysForms.emplace_back(boundObj);
								// Weapons, shields, torches.
								if ((*boundObj->formType == RE::FormType::Weapon) ||
									(boundObj->As<RE::TESObjectARMO>() && boundObj->As<RE::TESObjectARMO>()->IsShield()) ||
									(boundObj->As<RE::TESObjectLIGH>() && boundObj->As<RE::TESObjectLIGH>()->data.flags.all(RE::TES_LIGHT_FLAGS::kCanCarry)))
								{
									cyclableFormsMap[CyclableForms::kWeapon].push_back(boundObj);
								}
								// Ammo.
								else if (*boundObj->formType == RE::FormType::Ammo)
								{
									cyclableFormsMap[CyclableForms::kAmmo].push_back(boundObj);
								}
							}

							++numFavoritedItems;
						}
					}
				}
			}
		}

		// Magical forms next.
		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (magicFavorites)
		{
			for (auto spellForm : magicFavorites->spells)
			{
				if (spellForm)
				{
					favoritedFormIDs.insert(spellForm->formID);
					if (!a_onlySetFIDs)
					{
						if (spellForm->As<RE::TESShout>())
						{
							cyclableFormsMap[CyclableForms::kVoice].push_back(spellForm);
						}
						else if (spellForm->As<RE::SpellItem>())
						{
							auto spell = spellForm->As<RE::SpellItem>();
							auto spellType = spell->GetSpellType();
							if (spellType == RE::MagicSystem::SpellType::kVoicePower ||
								spellType == RE::MagicSystem::SpellType::kPower ||
								spellType == RE::MagicSystem::SpellType::kLesserPower)
							{
								cyclableFormsMap[CyclableForms::kVoice].push_back(spellForm);
							}
							else
							{
								cyclableFormsMap[CyclableForms::kSpell].push_back(spellForm);
							}
						}
					}

					++numFavoritedItems;
				}
			}
		}

		if (numFavoritedItems > 0)
		{
			// Remove duplicates.
			for (auto i = 0; i < !CyclableForms::kTotal; ++i)
			{
				if (auto& favFormsList = cyclableFormsMap[static_cast<CyclableForms>(i)]; !favFormsList.empty())
				{
					auto newEnd = std::unique(favFormsList.begin(), favFormsList.end());
					if (newEnd != favFormsList.end())
					{
						uint32_t prevSize = favFormsList.size();
						favFormsList.erase(newEnd, favFormsList.end());
					}
				}
			}
		}
	}

	void EquipManager::SetCurrentVoiceSpell()
	{
		// Get highest known shout variation and set the voice spell to that variation.
		// Or if a power is equipped, set the voice spell to that power.

		logger::debug("[EM] SetCurrentVoiceSpell: {}.", coopActor->GetName());

		highestShoutVarIndex = -1;
		voiceSpell = nullptr;
		if (voiceForm)
		{
			// Is a shout.
			if (voiceForm->GetFormType() == RE::FormType::Shout)
			{
				auto shout = voiceForm->As<RE::TESShout>();
				// Get highest known variation spell and return its formID
				uint32_t i = 0;
				bool wordsInvalid = !((bool)shout->variations[0].word);
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
				if (auto highProc = (coopActor->currentProcess) ? coopActor->currentProcess->high : nullptr; highProc)
				{
					highProc->currentShout = shout;
					highProc->currentShoutVariation = static_cast<RE::TESShout::VariationID>(highestShoutVarIndex);
				}
			}
			// Is a power.
			else
			{
				voiceSpell = voiceForm->As<RE::SpellItem>();
			}
		}

		logger::debug("[EM] SetCurrentVoiceSpell: {}: voice form: {}, voice spell: {}, highest shout var index: {}.", 
			coopActor->GetName(),
			voiceForm ? voiceForm->GetName() : "NONE",
			voiceSpell ? voiceSpell->GetName() : "NONE",
			highestShoutVarIndex);
	}

	void EquipManager::SetOriginalWeaponTypeFromKeyword(HandIndex&& a_handSlot)
	{
		// Update the cached LH/RH/2H weapon type to the weapon's default type based on its keywords.
		logger::debug("[EM] SetOriginalWeaponTypeFromKeyword: {}: slot: {}.", 
			coopActor->GetName(),
			a_handSlot);

		if (a_handSlot == HandIndex::kNone || a_handSlot == HandIndex::kBoth) 
		{
			logger::critical("[EM] ERR: SetOriginalWeaponTypeFromKeyword: {}: Invalid hand slot {}. Cannot set original weapon type.", coopActor->GetName(), a_handSlot);
			return;
		}

		RE::TESObjectWEAP* asWeap = nullptr;
		if (a_handSlot == HandIndex::kLH)
		{
			asWeap = GetLHWeapon();
		}
		else
		{
			asWeap = GetRHWeapon();
		}

		if (asWeap)
		{
			auto foundIter = std::find_if(glob.weapTypeKeywordsList.begin(), glob.weapTypeKeywordsList.end(),
				[asWeap](RE::BGSKeyword* keyword) {
					return keyword && asWeap->HasKeyword(keyword);
				});

			auto keyword = (foundIter != glob.weapTypeKeywordsList.end()) ? *foundIter : nullptr;
			if (keyword)
			{
				auto weaponType = Util::GetWeaponTypeFromKeyword(keyword);
				if (a_handSlot == HandIndex::kLH)
				{
					lhOriginalType = weaponType;
					logger::debug("[EM] SetOriginalWeaponTypeFromKeyword: {}: LH weapon has keyword {}, which corresponds to weapon type: {}",
						coopActor->GetName(), keyword->formEditorID, static_cast<uint32_t>(lhOriginalType));
				}
				else if (a_handSlot == HandIndex::kRH)
				{
					rhOriginalType = weaponType;
					logger::debug("[EM] SetOriginalWeaponTypeFromKeyword: {}: RH weapon has keyword {}, which corresponds to weapon type: {}",
						coopActor->GetName(), keyword->formEditorID, static_cast<uint32_t>(rhOriginalType));
				}
				else
				{
					lhOriginalType = rhOriginalType = weaponType;
					logger::debug("[EM] SetOriginalWeaponTypeFromKeyword: {}: 2H weapon has keyword {}, which corresponds to weapon type: {}", 
						coopActor->GetName(), keyword->formEditorID, static_cast<uint32_t>(lhOriginalType));
				}
			}
			else
			{
				logger::debug("[EM] SetOriginalWeaponTypeFromKeyword: {}: Could not find matching weapon type keyword for 2H weapon {}", coopActor->GetName(),asWeap->GetName());
			}
		}
		else
		{
			logger::critical("[EM] SetOriginalWeaponTypeFromKeyword: {}: Could not get weapon. Cannot set original weapon type.", coopActor->GetName());
			return;
		}
	}

	void EquipManager::SwitchWeaponGrip(RE::TESObjectWEAP* a_weapon, bool a_equipRH)
	{
		// WIP: Switch given weapon's 1H weapon grip to 2H and vice versa.
		logger::debug("[EM] SwitchWeaponGrip: {}: weapon: {}, equip in RH: {}",
			coopActor->GetName(), 
			a_weapon ? a_weapon->GetName() : "NONE", 
			a_equipRH);

		// Do not change grip for bows and crossbows.
		// They just won't work afterward. Oh well.
		bool usesAmmo = a_weapon && (*a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kBow ||
									 *a_weapon->weaponData.animationType == RE::WEAPON_TYPE::kCrossbow);
		if (a_weapon && !usesAmmo)
		{
			// 1H to 2H.
			if (a_weapon->equipSlot != glob.bothHandsEquipSlot)
			{
				auto equipSlot1H = a_weapon->equipSlot;
				UnequipForm(a_weapon, a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, (RE::ExtraDataList*)nullptr, 1, equipSlot1H, true, true, false, true, glob.bothHandsEquipSlot);
				a_weapon->SetEquipSlot(glob.bothHandsEquipSlot);
			}
			// 2H to 1H
			else
			{
				auto equipSlot1H = a_equipRH ? glob.rightHandEquipSlot : glob.leftHandEquipSlot;
				UnequipForm(a_weapon, a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, (RE::ExtraDataList*)nullptr, 1, glob.bothHandsEquipSlot, true, true, false, true, equipSlot1H);
				a_weapon->SetEquipSlot(equipSlot1H);
			}

			auto currentType = *a_weapon->weaponData.animationType;
			if (GlobalCoopData::WEAP_ANIM_SWITCH_MAP.contains(currentType))
			{
				auto newType = GlobalCoopData::WEAP_ANIM_SWITCH_MAP.at(currentType);
				// Special case with staves switching back to 1H ranged cast animations from 2H melee animations.
				if (a_weapon->HasKeyword(glob.weapTypeKeywordsList[static_cast<uint32_t>(RE::WEAPON_TYPE::kStaff)]) &&
					currentType != RE::WEAPON_TYPE::kStaff)
				{
					newType = RE::WEAPON_TYPE::kStaff;
				}
				a_weapon->weaponData.animationType.reset(currentType);
				a_weapon->weaponData.animationType.set(newType);
				a_weapon->SetAltered(true);
				logger::debug("[EM] SwitchWeaponGrip: {}: Switched {}'s weapon animations from type {} to {}",
					coopActor->GetName(), a_weapon->GetName(), currentType, newType);
			}
			else
			{
				logger::critical("[EM] ERR: SwitchWeaponGrip: {}: Current weapon type for {} is invalid ({})", coopActor->GetName(), a_weapon->GetName(), currentType);
			}

			EquipForm(a_weapon, a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, (RE::ExtraDataList*)nullptr, 1, a_weapon->GetEquipSlot());
		}
	}

	void EquipManager::UnequipAll()
	{
		// Unequip all equipped gear after re-assigning saved equipped forms to the desired forms list.
		logger::debug("[EM] UnequipAll: {}.", coopActor->GetName());

		// Re-assign saved serialized forms.
		desiredEquippedForms.fill(nullptr);
		auto& savedEquippedForms = glob.serializablePlayerData.at(coopActor->formID)->equippedForms;
		std::copy(savedEquippedForms.begin(), savedEquippedForms.end(), desiredEquippedForms.begin());

		EquipIndex equipIndex = EquipIndex::kTotal;
		for (uint8_t i = 0; i < !EquipIndex::kTotal; ++i)
		{
			equipIndex = static_cast<EquipIndex>(i);
			// Ignore accompanying equips to the forms that the player chose to equip.
			// Examples include bound weapons that are equipped once the corresponding bound weapon spell is cast
			// and enchantments that equip with the weapon they are attached to.
			bool isBound = {
				(equippedForms[i] && (equipIndex == EquipIndex::kLeftHand || equipIndex == EquipIndex::kRightHand) &&
					equippedForms[i]->Is(RE::FormType::Weapon) && equippedForms[i]->As<RE::TESObjectWEAP>()->IsBound()) ||
				(equippedForms[i] && equipIndex == EquipIndex::kAmmo && equippedForms[i]->Is(RE::FormType::Ammo) &&
					equippedForms[i]->As<RE::TESAmmo>()->HasKeywordID(0x10D501))
			};
			bool isEnchantment = {
				equippedForms[i] && (equipIndex == EquipIndex::kLeftHand || equipIndex == EquipIndex::kRightHand) &&
				equippedForms[i]->Is(RE::FormType::Enchantment)
			};

			// Slots to still clear out, since they have the greatest chance of causing equip-related soft locks from my terrible logic:
			// No bound weapons/enchantments, and only mismatches or hand slot forms.
			if (!isBound && !isEnchantment && 
				equippedForms[i] != desiredEquippedForms[i] || equipIndex == EquipIndex::kLeftHand || equipIndex == EquipIndex::kRightHand)
			{
				desiredEquippedForms[i] = nullptr;
			}
		}

		Util::Papyrus::UnequipAll(coopActor.get());
	}

	void EquipManager::UnequipAmmo(RE::TESForm* a_toUnequip, RE::ExtraDataList* a_exData, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow, const RE::BGSEquipSlot* a_slotToReplace)
	{
		// Unequip the given ammo.

		logger::debug("[EM] UnequipAmmo: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE");

		if (auto ammo = a_toUnequip ? a_toUnequip->As<RE::TESAmmo>() : nullptr; ammo)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				const auto& invCounts = coopActor->GetInventoryCounts();
				auto currentAmmoCount = invCounts.at(ammo);
				bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toUnequip);
				if (!p->isPlayer1)
				{
					ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kAmmo, a_slot);
					// NOTE: The game has issues un/equipping ammo when count is large (e.g. 100000), so remove and re-add as a failsafe after unequipping.
					// Ugly but seems to work.
					aem->UnequipObject(coopActor.get(), ammo, a_exData, currentAmmoCount, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow, a_slotToReplace);
					
					coopActor->RemoveItem(ammo, currentAmmoCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					coopActor->AddObjectToContainer(ammo, nullptr, currentAmmoCount, coopActor.get());
				}
				else
				{
					aem->UnequipObject(coopActor.get(), ammo); 
					// NOTE: The game has issues un/equipping ammo when count is large (e.g. 100000), so remove and re-add as a failsafe after unequipping.
					// Ugly but seems to work.
					coopActor->RemoveItem(ammo, currentAmmoCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, coopActor.get());
				}

				// Have to re-favorite once re-added.
				if (wasFavorited)
				{
					Util::ChangeFormFavoritesStatus(coopActor.get(), a_toUnequip, true);
				}
			}
		}
	}

	void EquipManager::UnequipArmor(RE::TESForm* a_toUnequip, RE::ExtraDataList* a_exData, uint32_t a_count, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow, const RE::BGSEquipSlot* a_slotToReplace)
	{
		// Unequip the given armor.

		logger::debug("[EM] UnequipArmor: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE");

		if (auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr; boundObj)
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				if (!p->isPlayer1)
				{
					// Must remove all armor entries that correspond to the requested item to unequip,
					// since armor pieces can fit into multiple biped slots.
					if (auto asBipedObjForm = a_toUnequip->As<RE::BGSBipedObjectForm>(); asBipedObjForm && a_toUnequip->As<RE::TESBoundObject>())
					{
						// Remove from desired equipped forms list.
						auto slotMask = asBipedObjForm->bipedModelData.bipedObjectSlots;
						bool isShield = asBipedObjForm->IsShield();
						const RE::BGSEquipSlot* slot = isShield ? a_toUnequip->As<RE::TESObjectARMO>()->equipSlot : a_slot;
						for (uint8_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
						{
							if (slotMask.all(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1 << (i - !EquipIndex::kFirstBipedSlot))))
							{
								ClearDesiredEquippedFormOnUnequip(a_toUnequip, i, slot);
							}
						}

						// Special shield case: also clear LH slot in desired equipped forms list.
						if (isShield)
						{
							ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kLeftHand, slot);
						}

						aem->UnequipObject(coopActor.get(), boundObj, a_exData, a_count, slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow, a_slotToReplace);
					}
					else
					{
						// All other forms.
						aem->UnequipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow, a_slotToReplace);
					}
				}
				else
				{
					aem->UnequipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, false, a_playSounds, a_applyNow, a_slotToReplace);
				}
			}
		}
	}

	void EquipManager::UnequipForm(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex, RE::ExtraDataList* a_exData, uint32_t a_count, const RE::BGSEquipSlot* a_slot, bool a_queueEquip, bool a_forceEquip, bool a_playSounds, bool a_applyNow, const RE::BGSEquipSlot* a_slotToReplace)
	{
		// Unequip the given form.

		logger::debug("[EM] UnequipForm: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE");

		if (auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr; boundObj)
		{
			auto aem = RE::ActorEquipManager::GetSingleton();
			if (aem)
			{
				// Special case if trying to unequip fists here.
				if (a_toUnequip == glob.fists)
				{
					aem->UnequipObject(coopActor.get(), glob.fists, nullptr, 1, a_slot, false, true, false, true);
					return;
				}

				if (!p->isPlayer1)
				{
					// Remove from desired equipped forms list.
					bool isHandForm = a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kRightHand;
					if (isHandForm)
					{
						if (a_slot != glob.bothHandsEquipSlot)
						{
							ClearDesiredEquippedFormOnUnequip(a_toUnequip, !a_equipIndex, a_slot);
						}
						else
						{
							ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kLeftHand, a_slot);
							ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kRightHand, a_slot);
						}
					}
					else
					{
						ClearDesiredEquippedFormOnUnequip(a_toUnequip, !a_equipIndex, a_slot);
					}

					aem->UnequipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, a_forceEquip, a_playSounds, a_applyNow, a_slotToReplace);
				}
				else
				{
					aem->UnequipObject(coopActor.get(), boundObj, a_exData, a_count, a_slot, a_queueEquip, false, a_playSounds, a_applyNow, a_slotToReplace);
				}
			}
		}
	}

	void EquipManager::UnequipFormAtIndex(const EquipIndex& a_equipIndex)
	{
		// Unequip whatever form is at the given equip index.

		logger::debug("[EM] UnequipFormAtIndex: {}: index: {}.", 
			coopActor->GetName(), 
			a_equipIndex);

		// Handle special cases first. Make sure torch and shield are unequipped,
		// since they may have a lingering entry in the biped slots section of the equipped forms list that can cause problems.
		if (a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kShield) 
		{
			UnequipShield();
		}

		if (RE::TESForm* currentForm = equippedForms[!a_equipIndex]; currentForm)
		{
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
					// Unequipping from the "either hand" equip slot causes "lingering equip state"
					// where the unequipped item still shows as equipped in the inventory/container menu, and will
					// require additional unequip requests to full unequip.
					// Force equip slot to match the passed-in equip index in this case.
					// Otherwise, use the weapon's default equip slot.
					if (equipType->equipSlot == glob.eitherHandEquipSlot)
					{
						if (a_equipIndex == EquipIndex::kLeftHand)
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
						// Hardcoded form ID, icky.
						if (weap->IsBow())
						{
							if (auto boundArrow = equippedForms[!EquipIndex::kAmmo]; boundArrow && boundArrow->HasKeywordByEditorID("WeapTypeBoundArrow"))
							{
								UnequipForm(boundArrow->As<RE::TESAmmo>(), EquipIndex::kAmmo);
							}
						}

						if (equipSlot == glob.bothHandsEquipSlot)
						{
							p->pam->boundWeapReqLH = false;
							p->pam->boundWeapReqRH = false;
							p->pam->secsSinceBoundWeapLHReq = p->pam->secsSinceBoundWeapRHReq = 0.0f;
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

				UnequipForm(currentForm, a_equipIndex, nullptr, 1, equipSlot);
			}
		}
	}

	void EquipManager::UnequipHandForms(RE::BGSEquipSlot* a_slot)
	{
		// Unequip form(s) in the given hand equip slot.

		logger::debug("[EM] UnequipHandForms: {}: slot: 0x{:X}.", 
			coopActor->GetName(),
			a_slot ? a_slot->formID : 0xDEAD);

		if (p->isPlayer1) 
		{
			if (auto aem = RE::ActorEquipManager::GetSingleton(); aem) 
			{
				if (a_slot == glob.bothHandsEquipSlot)
				{
					auto lhForm = coopActor->GetEquippedObject(true);
					if (lhForm) 
					{
						if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell) 
						{
							Util::NativeFunctions::UnequipSpell(coopActor.get(), lhSpell, !EquipIndex::kLeftHand);
						}
						else if (auto lhBoundObj = lhForm->As<RE::TESBoundObject>(); lhBoundObj)
						{
							aem->UnequipObject(coopActor.get(), lhBoundObj);
						}
					}

					auto rhForm = coopActor->GetEquippedObject(false);
					if (rhForm)
					{
						if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
						{
							Util::NativeFunctions::UnequipSpell(coopActor.get(), rhSpell, !EquipIndex::kRightHand);
						}
						else if (auto rhBoundObj = rhForm->As<RE::TESBoundObject>(); rhBoundObj)
						{
							aem->UnequipObject(coopActor.get(), rhBoundObj);
						}
					}

					//EquipFists();
				}
				else if (a_slot == glob.leftHandEquipSlot)
				{
					auto lhForm = coopActor->GetEquippedObject(true);
					if (lhForm)
					{
						if (auto lhSpell = lhForm->As<RE::SpellItem>(); lhSpell)
						{
							Util::NativeFunctions::UnequipSpell(coopActor.get(), lhSpell, !EquipIndex::kLeftHand);
						}
						else if (auto lhBoundObj = lhForm->As<RE::TESBoundObject>(); lhBoundObj)
						{
							aem->UnequipObject(coopActor.get(), lhBoundObj);
						}
					}

					// Re-equip form in the other hand. Save old form here before emptying hands.
					/*auto rhForm = coopActor->GetEquippedObject(false);
					EquipFists();
					if (rhForm) 
					{
						if (auto asSpell = rhForm->As<RE::SpellItem>(); asSpell) 
						{
							aem->EquipSpell(coopActor.get(), asSpell);
						}
						else if (auto boundObj = rhForm->As<RE::TESBoundObject>(); boundObj)
						{
							aem->EquipObject(coopActor.get(), boundObj);
						}
					}*/
				}
				else if (a_slot == glob.rightHandEquipSlot)
				{
					auto rhForm = coopActor->GetEquippedObject(false);
					if (rhForm)
					{
						if (auto rhSpell = rhForm->As<RE::SpellItem>(); rhSpell)
						{
							Util::NativeFunctions::UnequipSpell(coopActor.get(), rhSpell, !EquipIndex::kRightHand);
						}
						else if (auto rhBoundObj = rhForm->As<RE::TESBoundObject>(); rhBoundObj)
						{
							aem->UnequipObject(coopActor.get(), rhBoundObj);
						}
					}

					// Re-equip form in the other hand. Save old form here before emptying hands.
					/*auto lhForm = coopActor->GetEquippedObject(true); 
					EquipFists();
					if (lhForm)
					{
						if (auto asSpell = lhForm->As<RE::SpellItem>(); asSpell)
						{
							aem->EquipSpell(coopActor.get(), asSpell);
						}
						else if (auto boundObj = lhForm->As<RE::TESBoundObject>(); boundObj)
						{
							aem->EquipObject(coopActor.get(), boundObj);
						}
					}*/
				}
			}
		}
		else
		{
			if (a_slot == glob.bothHandsEquipSlot)
			{
				desiredEquippedForms[!EquipIndex::kLeftHand] = desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
				EquipFists();
			}
			else if (a_slot == glob.leftHandEquipSlot)
			{
				desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
				UnequipFormAtIndex(EquipIndex::kLeftHand);
			}
			else if (a_slot == glob.rightHandEquipSlot)
			{
				desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
				UnequipFormAtIndex(EquipIndex::kRightHand);
			}
		}
	}

	void EquipManager::UnequipShield()
	{
		// Unequip any equipped shield.

		logger::debug("[EM] UnequipShield: {}.", coopActor->GetName());

		if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
		{
			if (auto shield = GetShield(); shield)
			{
				if (p->isPlayer1)
				{
					aem->UnequipObject(coopActor.get(), shield, nullptr, 1, shield->equipSlot, false, true, true, true);
				}
				else
				{
					// Clear out LH slot first.
					desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
					// Clear out biped slots. Looping through just in case this shield has other biped slots
					// in addition to the shield biped slot.
					auto slotMask = shield->bipedModelData.bipedObjectSlots;
					for (uint8_t i = !EquipIndex::kFirstBipedSlot; i <= !EquipIndex::kLastBipedSlot; ++i)
					{
						if (slotMask.all(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1 << (i - !EquipIndex::kFirstBipedSlot))))
						{
							ClearDesiredEquippedFormOnUnequip(shield, i, shield->equipSlot);
						}
					}

					aem->UnequipObject(coopActor.get(), shield, nullptr, 1, shield->equipSlot, false, true, true, true);
				}
			}
		}
	}

	void EquipManager::UnequipShout(RE::TESForm* a_toUnequip)
	{
		// Unequip the given shout.

		logger::debug("[EM] UnequipShout: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE");

		auto shout = a_toUnequip ? a_toUnequip->As<RE::TESShout>() : nullptr;
		if (shout)
		{
			if (!p->isPlayer1)
			{
				ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kVoice, glob.voiceEquipSlot);
			}

			Util::Papyrus::UnequipShout(coopActor.get(), shout);
		}
	}

	void EquipManager::UnequipSpell(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex)
	{
		// Unequip the given spell.

		logger::debug("[EM] UnequipSpell: {}: unequip {}, index: {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE",
			a_equipIndex);

		if (auto spell = a_toUnequip ? a_toUnequip->As<RE::SpellItem>() : nullptr; spell)
		{
			auto slot = spell->equipSlot;
			bool is2HSpell = slot == glob.bothHandsEquipSlot;
			bool slotIndex = a_equipIndex == EquipIndex::kVoice ? 2 : !a_equipIndex;
			if (!p->isPlayer1)
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
					ClearDesiredEquippedFormOnUnequip(spell, !a_equipIndex, spell->equipSlot);
					Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, slotIndex);
				}
				else
				{
					// Equip "fists" to clear out spell slots.
					ClearDesiredEquippedFormOnUnequip(spell, !EquipIndex::kLeftHand, spell->equipSlot);
					ClearDesiredEquippedFormOnUnequip(spell, !EquipIndex::kRightHand, spell->equipSlot);
					EquipFists();
				}
			}
			else
			{
				if (!is2HSpell)
				{
					Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, slotIndex);
				}
				else
				{
					// Both hands.
					Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, 0);
					Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, 1);
				}
			}
		}
	}

	void EquipManager::ValidateEquipState()
	{
		// Try to force the game to re-equip the player's desired forms
		// without equip slot mismatches and other issues.
		// To deal with the game's equip system or my own code's BS.

		auto currentLHForm = coopActor->GetEquippedObject(true);
		auto currentRHForm = coopActor->GetEquippedObject(false);
		auto currentVoiceForm = equippedForms[!EquipIndex::kVoice];
		auto currentLHEquipType = currentLHForm ? currentLHForm->As<RE::BGSEquipType>() : nullptr;
		auto currentRHEquipType = currentRHForm ? currentRHForm->As<RE::BGSEquipType>() : nullptr;
		auto desiredLHForm = desiredEquippedForms[!EquipIndex::kLeftHand];
		auto desiredRHForm = desiredEquippedForms[!EquipIndex::kRightHand];
		auto desiredVoiceForm = desiredEquippedForms[!EquipIndex::kVoice];
		auto desiredLHEquipType = desiredLHForm ? desiredLHForm->As<RE::BGSEquipType>() : nullptr;
		auto desiredRHEquipType = desiredRHForm ? desiredRHForm->As<RE::BGSEquipType>() : nullptr;
		bool shouldCheckHandForms =
			(!currentLHForm && desiredLHForm) ||
			(!currentRHForm && desiredRHForm) ||
			((currentLHForm == glob.fists) && (desiredLHForm || desiredRHForm));
		bool shouldReEquip = false;

		// Fix and modify desired equipped forms entries.
		if (shouldCheckHandForms) 
		{
			// 2H not in both slots. Rectifiable.
			if (desiredLHEquipType && desiredLHEquipType->equipSlot == glob.bothHandsEquipSlot)
			{
				if (!desiredRHForm)
				{
					logger::debug("[EM] ValidateEquipState: {}: 2H form {} should be in both LH and RH desired form slots. Adding to RH slot now.",
						coopActor->GetName(), desiredLHForm->GetName());
					desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
					shouldReEquip = true;
				}
				else if (!desiredRHEquipType || desiredRHEquipType->equipSlot != glob.bothHandsEquipSlot)
				{
					logger::debug("[EM] ValidateEquipState: {}: 2H form {} is not also in the RH slot and RH form {} is not a 2H form. Moving 2H form to RH.",
						coopActor->GetName(), desiredLHForm->GetName(), desiredRHForm ? desiredRHForm->GetName() : "INVALID");
					desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
					shouldReEquip = true;
				}
			}
			else if (desiredRHEquipType && desiredRHEquipType->equipSlot == glob.bothHandsEquipSlot)
			{
				if (!desiredLHForm)
				{
					logger::debug("[EM] ValidateEquipState: {}: 2H form {} should be in both LH and RH desired form slots. Adding to LH slot now.",
						coopActor->GetName(), desiredRHForm->GetName());
					desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
					shouldReEquip = true;
				}
				else if (!desiredLHEquipType || desiredLHEquipType->equipSlot != glob.bothHandsEquipSlot)
				{
					logger::debug("[EM] ValidateEquipState: {}: 2H form {} is not also in the LH slot and LH form {} is not a 2H form. Moving 2H form to LH.",
						coopActor->GetName(), desiredRHForm->GetName(), desiredLHForm ? desiredLHForm->GetName() : "INVALID");
					desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
					shouldReEquip = true;
				}
			}

			// Mismatching equip slots. Rectifiable.
			if (desiredLHEquipType && desiredLHEquipType->equipSlot == glob.rightHandEquipSlot)
			{
				logger::debug("[EM] ValidateEquipState: {}: LH form {} has RH equip slot. Moving to RH.",
					coopActor->GetName(), desiredLHForm->GetName());
				desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
				desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
				shouldReEquip = true;
			}

			if (desiredRHEquipType && desiredRHEquipType->equipSlot == glob.leftHandEquipSlot)
			{
				logger::debug("[EM] ValidateEquipState: {}: RH form {} has LH equip slot. Moving to LH.",
					coopActor->GetName(), desiredRHForm->GetName());
				desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
				desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
				shouldReEquip = true;
			}
		}
		else
		{
			// 1H weapon, which the player only owns 1 of, is in both LH and RH slots. Rectifiable.
			if (currentLHForm && currentLHForm->As<RE::TESObjectWEAP>() &&
				currentLHEquipType->equipSlot != glob.bothHandsEquipSlot &&
				currentLHForm != glob.fists &&
				currentLHForm == currentRHForm)
			{
				auto weap = currentLHForm->As<RE::TESObjectWEAP>();
				const auto invCounts = coopActor->GetInventoryCounts();
				if (invCounts.contains(weap) && invCounts.at(weap) == 1)
				{
					// Unequip from LH, keep in RH.
					logger::debug("[EM] ValidateEquipState: {}: 1H form {} with count 1 is in both LH and RH desired form slots. Unequipping from the LH slot now.",
						coopActor->GetName(), currentLHForm->GetName());
					desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
					desiredEquippedForms[!EquipIndex::kRightHand] = weap;
					shouldReEquip = true;
				}
			}
		}

		// If there were no corrections made to the desired forms list,
		// check if there are equip mismatches between the current and desired hand/voice forms,
		// and re-equip the desired forms.
		// Do not re-equip desired hand form when the current hand form is bound.
		if (!shouldReEquip) 
		{
			shouldReEquip |= ((!p->pam->boundWeapReqLH && !p->pam->boundWeapReqRH) && (currentLHForm != desiredLHForm || currentRHForm != desiredRHForm));
			if (shouldReEquip) 
			{
				logger::debug("[EM] ValidateEquipState: {}: Current LH form ({}, is bound weap req active: {}) does not match desired form ({}): {}, current RH form ({}, is bound weap req active: {}) does not match desired form ({}): {}, current Voice form ({}) does not match desired form ({}): {}",
					coopActor->GetName(), 
					currentLHForm ? currentLHForm->GetName() : "EMPTY",
					p->pam->boundWeapReqLH,
					desiredLHForm ? desiredLHForm->GetName() : "EMPTY",
					currentLHForm != desiredLHForm,
					currentRHForm ? currentRHForm->GetName() : "EMPTY",
					p->pam->boundWeapReqRH,
					desiredRHForm ? desiredRHForm->GetName() : "EMPTY",
					currentRHForm != desiredRHForm,
					currentVoiceForm ? currentVoiceForm->GetName() : "NONE",
					desiredVoiceForm ? desiredVoiceForm->GetName() : "NONE",
					currentVoiceForm != desiredVoiceForm);
			}
		}

		if (shouldReEquip)
		{
			logger::debug("[EM] ValidateEquipState: {}: Desired equipped forms were invalid and modified. Re-equipping desired equipped hand forms.", coopActor->GetName());
			ReEquipHandForms();
		}
	}
}

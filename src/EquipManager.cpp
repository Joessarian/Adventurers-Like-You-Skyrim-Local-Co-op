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
		if (a_p && a_p->controllerID > -1 && a_p->controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			SPDLOG_DEBUG
			(
				"[EM] Initialize: Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count()
			);
			// Init once.
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
			RefreshData();
		}
		else
		{
			SPDLOG_ERROR
			(
				"[EM] ERR: Initialize: "
				"Cannot construct Equip Manager for controller ID {}.", 
				a_p ? a_p->controllerID : -1
			);
		}
	}

	void EquipManager::MainTask()
	{
		// Re-equip forms that were automatically unequipped by the game.
		// Temporary solution until figuring out how to prevent the game from auto-equipping
		// the "best" gear for co-op actors.

		// P1 does not require assistance, leave them be.
		if (p->isPlayer1)
		{
			return;
		}

		auto ui = RE::UI::GetSingleton(); 
		if (!ui)
		{
			return;
		}

		// Ammo equipped one unit at a time to prevent odd reload/nocking 
		// and inventory display issues when a large amount of ammo is equipped at once.
		// Have to re-equip 1 unit here once the currently cached ammo is cleared 
		// upon releasing the projectile.
		if (!coopActor->GetCurrentAmmo() && desiredEquippedForms[!EquipIndex::kAmmo])
		{
			auto ammoToReEquip = 
			(
				desiredEquippedForms[!EquipIndex::kAmmo]->As<RE::TESBoundObject>()
			);
			// Also add 1 ammo unit when in god mode to maintain the same ammo count.
			if (p->isInGodMode)
			{
				coopActor->AddObjectToContainer(ammoToReEquip, nullptr, 1, coopActor.get());
			}

			// Make sure the player still has at least 1 of the ammo before equipping.
			const auto invCounts = coopActor->GetInventoryCounts();
			if (invCounts.contains(ammoToReEquip) && invCounts.at(ammoToReEquip) > 0)
			{
				EquipAmmo(ammoToReEquip);
			}
		}

		// Check if the game unequipped desired hand forms and re-equip them as needed.
		// Sometimes these forms are unequipped without any equip event firing.
		// Can also loop infintely in certain instances 
		// where the game interferes with the equip request's equip slots.
		// Solution TBD.
		bool isEquipping = false;
		bool isUnequipping = false;
		coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
		coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
		// Not transforming/transformed, not (un)equipping, and not attacking or casting.
		if (!p->isTransforming && 
			!p->isTransformed &&
			!isEquipping && 
			!isUnequipping && 
			!p->pam->isAttacking && 
			!p->pam->isInCastingAnim) 
		{
			// Attempt to validate and re-equip.
			ValidateEquipState();
		}
	}

	void EquipManager::PrePauseTask()
	{
		// No pre-state change tasks.
		return;
	}

	void EquipManager::PreStartTask()
	{
		SPDLOG_DEBUG("[EM] PreStartTask: P{}", playerID + 1);

		// Update our cached equip state.
		RefreshEquipState(RefreshSlots::kAll);
		// "Infinite" carryweight for coop players.
		// NOTE: 
		// Adjusts the temporary modifier, so the previous temp buffs/debuffs are wiped.
		float permMod = coopActor->GetActorValueModifier
		(
			RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight
		);
		float tempMod = coopActor->GetActorValueModifier
		(
			RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight
		);
		float damageMod = coopActor->GetActorValueModifier
		(
			RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight
		);
		if (Settings::bInfiniteCarryweight)
		{
			coopActor->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, 
				RE::ActorValue::kCarryWeight, 
				(float)INT32_MAX - coopActor->GetActorValue(RE::ActorValue::kCarryWeight) - 1.0f
			);
		}
		else
		{
			coopActor->RestoreActorValue
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kCarryWeight, -tempMod
			);
		}

		// Fixes skin glow/tone mismatches.
		if (!p->isPlayer1)
		{
			coopActor->DoReset3D(false);
		}

		// Don't re-equip items when transformed or when just unpausing without a data refresh.
		if (!p->isTransformed && currentState != ManagerState::kPaused) 
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
				// For P1, only re-equip hand forms, as the equip state for them may be glitched.
				ReEquipHandForms();
			}
			else
			{
				// Re-equip all saved forms for companion players
				// in case there was some lingering glitched equip state.
				EquipFists();
				ReEquipAll(false);
			}
		}

		// Reset weapon speed multiplier, which may have been modified.
		coopActor->SetActorValue(RE::ActorValue::kWeaponSpeedMult, 0.0f);
		// Ensure player is visible.
		coopActor->SetAlpha(1.0f);
		// Clear all lingering shader effects.
		Util::StopAllEffectShaders(coopActor.get());
	}

	void EquipManager::RefreshData()
	{
		// Player data.
		coopActor = RE::ActorPtr{ p->coopActor };
		controllerID = p->controllerID;
		playerID = p->playerID;

		// Get serialized data to initialize some data members.
		const auto& data = glob.serializablePlayerData.at(coopActor->formID);

		// Spells and quick slot forms.
		quickSlotItem = nullptr;
		quickSlotSpell = nullptr;
		voiceForm = nullptr;
		voiceSpell = nullptr;

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
				glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * controllerID + i]
			);
		}

		// Favorited items.
		favItemsEquippedIndices.clear();
		favoritedFormIDs.clear();
		favoritesIndicesInCommon.clear();
		favoritedItemWasAdded.clear();
		favoritedForms.clear();
		favoritedEmoteIdles = data->cyclableEmoteIdleEvents;

		// Hotkeyed forms.
		hotkeyedForms = data->hotkeyedForms;
		hotkeyedFormsToSlotsMap.clear();
		lastChosenHotkeyedForm = nullptr;

		// Favorited/equipped forms maps and lists.
		cyclableFormsMap.clear();
		desiredEquippedForms.fill(nullptr);
		equippedForms.fill(nullptr);

		// Favorites list indices for equipped quick slot forms
		// (quick slot item, quick slot spell).
		equippedQSItemIndex = -1;
		equippedQSSpellIndex = -1;
		// Highest known shout variation for the current equipped shout.
		highestShoutVarIndex = -1;

		// Apply serializd equip state.
		SetInitialEquipState();
		// Pull in our serialized favorited forms lists for items and magic.
		UpdateFavoritedFormsLists(true);

		SPDLOG_DEBUG("[EM] RefreshData: {}.", coopActor ? coopActor->GetName() : "NONE");
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

			copiedShoutToEquip = glob.placeholderShouts[controllerID];
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
								SPDLOG_DEBUG
								(
									"[EM] Shout variation spell {} {} "
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
			SPDLOG_DEBUG("[EM] Copied shout data to placeholder shout {}.", 
				copiedShoutToEquip->GetName());
			SetCopiedMagicAndFID(a_shoutToCopy, PlaceholderMagicIndex::kVoice);
			// Make sure the cached placeholder magic form is set to the newly copied shout.
			placeholderMagic[!PlaceholderMagicIndex::kVoice] = copiedShoutToEquip;
		}

		return copiedShoutToEquip;
	}
	*/

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
			glob.placeholderSpells[!PlaceholderMagicIndex::kTotal * controllerID + !a_index]
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

		SPDLOG_DEBUG
		(
			"[EM] CopyToPlaceholderSpell: {}: spell: {} -> {}, index: {}.",
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
		if (ammoTypeToCycle != kEither)
		{
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
			// No previously cycled ammo, so pick the first one.
			nextAmmoIndex = 0;
		}
		else
		{
			// Wrap around.
			nextAmmoIndex = 
			(
				currentCycledAmmoIndex == cyclableAmmoList.size() - 1 ? 
				0 : 
				currentCycledAmmoIndex + 1
			);
		}

		currentCycledAmmo = 
		(
			cyclableAmmoList[nextAmmoIndex] ? 
			cyclableAmmoList[nextAmmoIndex]->As<RE::TESAmmo>() : 
			nullptr
		);

		SPDLOG_DEBUG
		(
			"[EM] CycleAmmo: {}: current cycled ammo: {} from index {}.",
			coopActor->GetName(),
			currentCycledAmmo ? currentCycledAmmo->GetName() : "NONE",
			nextAmmoIndex
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

		SPDLOG_DEBUG
		(
			"[EM] CycleEmoteIdles: {}: current idle: {}, index: {}.",
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

		SPDLOG_DEBUG
		(
			"[EM] CycleHandSlotMagic: {}: right hand: {}, spell category {} "
			"and currently cycled spell {} from index {}.",
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

		SPDLOG_DEBUG
		(
			"[EM] CycleHandSlotMagicCategory: {}: right hand: {}, "
			"spell category is now: {}.",
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

		SPDLOG_DEBUG
		(
			"[EM] CycleVoiceSlotMagic: {}: currently cycled voice magic: "
			"{} from index {}.",
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

		SPDLOG_DEBUG
		(
			"[EM] CycleWeaponCategory: {}: right hand: {}, weapon category is now: {}.",
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

		// In new cyclable list.
		int32_t nextWeaponIndex = currentCycledWeaponIndex;
		if (currentCycledWeaponIndex == -1)
		{
			// Select first weapon if none was cycled before.
			nextWeaponIndex = 0;
		}
		else
		{
			RE::TESForm* currentlyEquippedForm = 
			(
				equippedForms[a_rightHand ? 
				!EquipIndex::kRightHand : 
				!EquipIndex::kLeftHand]
			);
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

		SPDLOG_DEBUG
		(
			"[EM] CycleWeapons: {}: right hand: {}, category is now {}, "
			"cycled weapon {} from index {}.",
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

		SPDLOG_DEBUG
		(
			"[EM] EquipAmmo: {}: equip {}.", 
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
			else
			{
				const auto& invCounts = coopActor->GetInventoryCounts();
				int32_t newAmmoCount = invCounts.contains(ammo) ? invCounts.at(ammo) : -1; 
				if (newAmmoCount != -1)
				{
					bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toEquip);
					int32_t hotkeyIndex =
					(
						hotkeyedFormsToSlotsMap.contains(ammo->formID) ?
						hotkeyedFormsToSlotsMap[ammo->formID] :
						-1
					);
					// Remove directly back into P1's inventory.
					coopActor->RemoveItem
					(
						ammo,
						newAmmoCount,
						RE::ITEM_REMOVE_REASON::kRemove,
						nullptr, 
						coopActor.get()
					);

					// Have to re-favorite once re-added.
					if (wasFavorited)
					{
						Util::ChangeFormFavoritesStatus(coopActor.get(), a_toEquip, true);
					}

					// Re-apply hotkey.
					if (hotkeyIndex != -1)
					{
						Util::ChangeFormHotkeyStatus(coopActor.get(), a_toEquip, hotkeyIndex);
					}
				}
			}

			aem->EquipObject(coopActor.get(), ammo);
		}
		else
		{
			// Unequip current ammo before equipping new one.
			if (RE::TESForm* currentAmmoForm = equippedForms[!EquipIndex::kAmmo]; currentAmmoForm)
			{
				UnequipAmmo(currentAmmoForm);
			}
			else
			{
				// NOTE: 
				// The game equips multiple single ammo units sometimes,
				// creating multiple inventory entries which clutter up the inventory menu.
				// Remove all of this ammo and add back to reset equip state.
				// Ugly but seems to avoid creating new entries.
				const auto& invCounts = coopActor->GetInventoryCounts();
				int32_t newAmmoCount = invCounts.contains(ammo) ? invCounts.at(ammo) : -1;
				if (newAmmoCount != -1)
				{
					bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toEquip);
					int32_t hotkeyIndex = 
					(
						hotkeyedFormsToSlotsMap.contains(ammo->formID) ? 
						hotkeyedFormsToSlotsMap[ammo->formID] : 
						-1
					);
					coopActor->RemoveItem
					(
						ammo, 
						newAmmoCount, 
						RE::ITEM_REMOVE_REASON::kRemove,
						nullptr, 
						coopActor.get()
					);

					// Have to re-favorite once re-added.
					if (wasFavorited)
					{
						Util::ChangeFormFavoritesStatus(coopActor.get(), a_toEquip, true);
					}

					// Re-apply hotkey.
					if (hotkeyIndex != -1) 
					{
						Util::ChangeFormHotkeyStatus(coopActor.get(), a_toEquip, hotkeyIndex);
					}
				}
			}

			// Add to desired equipped forms list.
			desiredEquippedForms[!EquipIndex::kAmmo] = a_toEquip;
			// NOTE: 
			// The game has issues un/equipping ammo when count is large (e.g. 100000), 
			// so only un/equip 1 at a time.
			aem->EquipObject
			(
				coopActor.get(), 
				a_toEquip->As<RE::TESBoundObject>(), 
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

		SPDLOG_DEBUG
		(
			"[EM] EquipArmor: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE"
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
			aem->EquipObject
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
			// Must add all armor indices that correspond to the requested item to equip,
			// since armor pieces can fit into multiple biped slots.
			if (auto asBipedObjForm = a_toEquip->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				std::vector<uint8_t> equipIndices;
				auto slotMask = asBipedObjForm->bipedModelData.bipedObjectSlots;
				bool isShield = asBipedObjForm->IsShield();
				const RE::BGSEquipSlot* slot = 
				(
					isShield ? 
					a_toEquip->As<RE::TESObjectARMO>()->equipSlot :
					a_slot
				);
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
						equipIndices.emplace_back(i);
						// Unequip armor in same slot first.
						if (auto currentArmorForm = equippedForms[i]; currentArmorForm)
						{
							UnequipArmor(currentArmorForm);
						}
					}
				}

				// Add to desired equipped forms list at each biped slot index.
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

				aem->EquipObject
				(
					coopActor.get(), 
					boundObj, 
					a_exData, 
					a_count, 
					slot,
					a_queueEquip,
					a_forceEquip,
					a_playSounds,
					a_applyNow
				);
			}
			else
			{
				// Any other form that does not fit in a biped slot.
				aem->EquipObject
				(
					coopActor.get(),
					boundObj,
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
	}

	void EquipManager::EquipFists()
	{
		// Clear out both hand slots by equipping the 'fists' item.

		SPDLOG_DEBUG("[EM] EquipFists: {}.", coopActor->GetName());

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
		aem->EquipObject
		(
			coopActor.get(), 
			glob.fists,
			nullptr, 
			1, 
			glob.bothHandsEquipSlot, 
			false, 
			p->isPlayer1 ? false : true, 
			false, 
			true
		);
		if (!p->isPlayer1) 
		{
			// NOTE: 
			// Can cause the game to stutter if unequipping right after equipping.
			// Also, the game only unequips fists automatically for P1. 
			// Must be done here for other players.
			aem->UnequipObject
			(
				coopActor.get(),
				glob.fists,
				nullptr,
				1,
				glob.bothHandsEquipSlot, 
				false, 
				p->isPlayer1 ? false : true, 
				false, 
				true
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

		SPDLOG_DEBUG
		(
			"[EM] EquipForm: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE"
		);

		auto boundObj = a_toEquip ? a_toEquip->As<RE::TESBoundObject>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!boundObj || !aem)
		{
			return;
		}

		// Special case if trying to equip fists here.
		// Desired form NOT cleared first.
		if (a_toEquip == glob.fists) 
		{
			aem->EquipObject
			(
				coopActor.get(), 
				glob.fists, 
				nullptr, 
				1, 
				a_slot, 
				false, 
				p->isPlayer1 ? false : true, 
				false, 
				true
			);
			return;
		}

		if (p->isPlayer1)
		{
			auto oppositeEquipIndex = 
			(
				a_equipIndex == EquipIndex::kLeftHand ? 
				EquipIndex::kRightHand :
				EquipIndex::kLeftHand
			);
			auto oppositeHandForm = equippedForms[!oppositeEquipIndex];
			auto invCounts = coopActor->GetInventoryCounts();
			auto numberOwned = 
			(
				invCounts.contains(boundObj) ? 
				invCounts.at(boundObj) : 
				0
			);
			bool alreadyEquippedInOtherHand = 
			(
				numberOwned == 1 && 
				oppositeHandForm == a_toEquip && 
				a_toEquip->As<RE::BGSEquipType>() && 
				a_toEquip->As<RE::BGSEquipType>()->equipSlot != glob.bothHandsEquipSlot
			);
			// Before equipping in the other hand,
			// unequip and remove + add back if P1 only owns one of the form.
			if (alreadyEquippedInOtherHand)
			{
				UnequipHandForms(glob.bothHandsEquipSlot);
				coopActor->RemoveItem
				(
					boundObj, 
					1, 
					RE::ITEM_REMOVE_REASON::kRemove,
					nullptr, 
					coopActor.get()
				);
			}

			// Once again, do not force equip for P1.
			aem->EquipObject
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
			// Add to desired equipped forms list.
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
					auto oppositeHandForm = equippedForms[!oppositeEquipIndex];
					auto invCounts = coopActor->GetInventoryCounts();
					auto numberOwned = 
					(
						invCounts.contains(boundObj) ? 
						invCounts.at(boundObj) : 
						0
					);
					bool alreadyEquippedInOtherHand = 
					(
						numberOwned == 1 && 
						oppositeHandForm == a_toEquip && 
						a_toEquip->As<RE::BGSEquipType>() && 
						a_toEquip->As<RE::BGSEquipType>()->equipSlot != glob.bothHandsEquipSlot
					);
					if (alreadyEquippedInOtherHand)
					{
						UnequipHandForms(glob.bothHandsEquipSlot);
						// And add back to ensure the game fully unequips it.
						// Had issues without this call before.
						coopActor->RemoveItem
						(
							boundObj,
							1, 
							RE::ITEM_REMOVE_REASON::kRemove, 
							nullptr, 
							coopActor.get()
						);
					}
					else
					{
						// Unequip LH/RH form first.
						UnequipFormAtIndex(a_equipIndex);
					}

					// Set desired equipped form at the given index.
					desiredEquippedForms[!a_equipIndex] = a_toEquip;

					// Equip enchantment as well if equipping a staff.
					if (auto weap = boundObj->As<RE::TESObjectWEAP>(); 
						weap && weap->IsStaff() && weap->formEnchanting) 
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

			aem->EquipObject
			(
				coopActor.get(),
				boundObj,
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

	void EquipManager::EquipShout(RE::TESForm* a_toEquip)
	{
		// Equip the given shout.

		SPDLOG_DEBUG
		(
			"[EM] EquipShout: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE"
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
			desiredEquippedForms[!EquipIndex::kVoice] = shout;
		}

		aem->EquipShout(coopActor.get(), shout);
	}

	void EquipManager::EquipSpell
	(
		RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, const RE::BGSEquipSlot* a_slot
	)
	{
		// Equip the given spell.

		SPDLOG_DEBUG
		(
			"[EM] EquipSpell: {}: equip {}.",
			coopActor->GetName(),
			a_toEquip ? a_toEquip->GetName() : "NONE"
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
						desiredEquippedForms[index] = nullptr;
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
				desiredEquippedForms[!a_equipIndex] = spell;
			}
			else
			{
				desiredEquippedForms[!EquipIndex::kLeftHand] = spell;
				desiredEquippedForms[!EquipIndex::kRightHand] = spell;
			}

			aem->EquipSpell(coopActor.get(), spell, a_slot);
		}
	}

	std::vector<RE::TESForm*> EquipManager::GetEquipableSpells(bool a_inHandSlot) const
	{
		// NOTE:
		// Unused for now, but keeping for reference or if needed again in the future.
		// Get all equipable hand-slot or voice-slot spells known by P1 and this player.

		SPDLOG_DEBUG
		(
			"[EM] GetEquipableSpells: {}: in hand slot: {}.", 
			coopActor->GetName(),
			a_inHandSlot
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

	RE::BGSEquipSlot* EquipManager::GetEquipSlotForForm
	(
		RE::TESForm* a_form, const EquipIndex& a_index
	) const
	{
		// Resolve equip slot for the given form with the requested equip index.

		SPDLOG_DEBUG
		(
			"[EM] GetEquipSlotForForm: {}: form: {}, index: {}.", 
			coopActor->GetName(),
			a_form ? a_form->GetName() : "NONE",
			a_index
		);

		auto equipSlot = glob.eitherHandEquipSlot;
		if (!a_form || !a_form->As<RE::BGSEquipType>()) 
		{
			return equipSlot;
		}

		auto asEquipType = a_form->As<RE::BGSEquipType>();
		if ((asEquipType->equipSlot == glob.bothHandsEquipSlot) && 
			(a_index == EquipIndex::kLeftHand || a_index == EquipIndex::kRightHand))
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

	void EquipManager::HandleEquipRequest
	(
		RE::TESForm* a_form, const EquipIndex& a_index, bool a_shouldEquip
	)
	{
		// Handle companion player (un)equip request for the given form at the given index.
		// NOTE:
		// Never called on P1.

		SPDLOG_DEBUG
		(
			"[EM] HandleEquipRequest: {}: form: {}, index: {}, should equip: {}.", 
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
				auto asLight = a_form->As<RE::TESObjectLIGH>(); 
				if (!asLight || !asLight->CanBeCarried())
				{
					return;
				}
				
				EquipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);

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
						coopActor->RemoveItem
						(
							asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
						);
					}
				}
				else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Equip the alchemy item as an object to use it.
					// Just equip, do not update the desired equipped forms list 
					// since there is no slot for it.
					aem->EquipObject
					(
						coopActor.get(),
						asAlchemyItem, 
						nullptr, 
						1, 
						nullptr, 
						false, 
						true,
						false, 
						true
					);
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
				aem->EquipObject
				(
					coopActor.get(),
					boundObj, 
					nullptr, 
					1, 
					nullptr, 
					false, 
					true,
					false, 
					true
				);

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
				auto asBipedObjForm = a_form->As<RE::BGSBipedObjectForm>();
				if (!asBipedObjForm)
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

				UnequipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);

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
				aem->UnequipObject
				(
					coopActor.get(), 
					boundObj, 
					nullptr, 
					1, 
					nullptr, 
					false, 
					true,
					false,
					true
				);

				break;
			}
			}
		}
	}

	void EquipManager::HandleMenuEquipRequest
	(
		RE::ObjectRefHandle a_fromContainerHandle, 
		RE::TESForm* a_form, 
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

		SPDLOG_DEBUG
		(
			"[EM] HandleMenuEquipRequest: {}: container: {}, form: {}, index: {}, "
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
		if (fromContainerPtr != coopActor && IsEquipped(a_form))
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
			auto asBipedObjForm = a_form->As<RE::BGSBipedObjectForm>(); 
			if (!asBipedObjForm)
			{
				return;
			}

			currentArmorInSlot = coopActor->GetWornArmor(asBipedObjForm->GetSlotMask());
			if (a_form != currentArmorInSlot)
			{
				EquipArmor(a_form);
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
			// Do nothing for now.

			RE::DebugNotification("Scrolls usage not yet implemented for players 2-4.");
			SPDLOG_DEBUG("[EM] HandleMenuEquipRequest: {}: Attempting to equip a scroll: {}.",
				coopActor->GetName(), a_form->GetName());

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
					auto currentCopiedSpell = GetCopiedMagic(currentPlaceholderIndex);
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
				EquipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
			}
			else
			{
				UnequipForm(a_form, EquipIndex::kLeftHand, nullptr, 1, asLight->equipSlot);
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
					coopActor->RemoveItem
					(
						asAlchemyItem, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
					);
				}
			}
			else if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
			{
				// Equip the alchemy item as an object to use it.
				// Just equip, do not update the desired equipped forms list 
				// since there is no slot for it.
				aem->EquipObject
				(
					coopActor.get(),
					asAlchemyItem, 
					nullptr, 
					1, 
					nullptr, 
					false, 
					true,
					false, 
					true
				);
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
			if (!IsEquipped(a_form)) 
			{
				aem->EquipObject
				(
					coopActor.get(), 
					boundObj, 
					nullptr, 
					1,
					nullptr,
					false,
					true,
					false, 
					true
				);
			}
			else
			{
				aem->UnequipObject
				(
					coopActor.get(), 
					boundObj, 
					nullptr, 
					1, 
					nullptr, 
					false, 
					true,
					false,
					true
				);
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

		SPDLOG_DEBUG("[EM] ImportCoopFavorites: {}. Only magic favorites: {}.", 
			coopActor->GetName(), a_onlyMagicFavorites);

		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		if (!p1)
		{
			return;
		}

		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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
			SPDLOG_DEBUG
			(
				"[EM] ERR: ImportCoopFavorites: {}: "
				"Could not get magic favorites singleton.", 
				coopActor->GetName()
			);
			return;
		}

		const auto& p1Favorites = coopP1->em->favoritedForms;
		const auto& favorites = favoritedForms;
		favoritesIndicesInCommon.clear();
		favoritedItemWasAdded.clear();
		favoritesIndicesInCommon = std::vector<bool>(favorites.size(), false);
		favoritedItemWasAdded = std::vector<bool>(favorites.size(), false);

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

			SPDLOG_DEBUG
			(
				"[EM] ImportCoopFavorites: {}: Remove P1 favorited magic {} at index {}. "
				"List size: {}.", 
				coopActor->GetName(),
				magForm ? magForm->GetName() : "NONE",
				removalIndex,
				magicFavorites->spells.size()
			);
			magicFavorites->RemoveFavorite(magForm);
		}

		// Clear when done.
		magicFavorites->spells.clear();

		if (!a_onlyMagicFavorites) 
		{
			// Remove P1's physical favorites that aren't shared and all hotkeys first.
			for (auto i = 0; i < p1Favorites.size(); ++i)
			{
				const auto form = p1Favorites[i];
				// Skip invalid and magical forms.
				if (!form || form->Is(RE::FormType::Spell, RE::FormType::Shout))
				{
					continue;
				}

				// Unfavorite physical forms that P1 has not also favorited.
				if (!favoritedFormIDs.contains(form->formID))
				{
					SPDLOG_DEBUG
					(
						"[EM] ImportCoopFavorites: {}: Removing P1 favorite {}.",
						coopActor->GetName(), form->GetName()
					);
					Util::ChangeFormFavoritesStatus(p1, form, false);
				}

				// Hotkeyed by P1, so remove the hotkey.
				if (coopP1->em->hotkeyedFormsToSlotsMap.contains(form->formID))
				{
					SPDLOG_DEBUG
					(
						"[EM] ImportCoopFavorites: {}: Removing P1 hotkey {} for {}.",
						coopActor->GetName(), 
						coopP1->em->hotkeyedFormsToSlotsMap[form->formID] + 1, 
						form->GetName()
					);
					Util::ChangeFormHotkeyStatus(p1, form, -1);
				}
			}
		}

		// Add companion player's favorites.
		for (auto i = 0; i < favorites.size(); ++i)
		{
			const auto form = favorites[i];
			if (!form)
			{
				continue;
			}

			if (a_onlyMagicFavorites || form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				SPDLOG_DEBUG
				(
					"[EM] ImportCoopFavorites: {}: Add favorited magic {}.", 
					coopActor->GetName(), form->GetName()
				);
				magicFavorites->SetFavorite(form);
			}
			else
			{
				if (Util::IsFavorited(p1, form))
				{
					// P1 has also favorited the item, 
					// so no need to add to their inventory or change its favorites status.
					favoritesIndicesInCommon[i] = true;
					SPDLOG_DEBUG
					(
						"[EM] ImportCoopFavorites: {}: P1 has also favorited {}. "
						"Not favoriting.",
						coopActor->GetName(), form->GetName()
					);
				}
				else
				{
					auto boundObj = form->As<RE::TESBoundObject>();
					auto invCounts = p1->GetInventoryCounts();
					// Not already favorited by P1 and P1 does not have the form,
					// so add the favorited form to P1 before favoriting it.
					// Tag this favorited form as added, so it can be removed
					// when P1's favorites are restored.
					if (!invCounts.contains(boundObj) || invCounts.at(boundObj) <= 0)
					{
						p1->AddObjectToContainer(form->As<RE::TESBoundObject>(), nullptr, 1, p1);
						favoritedItemWasAdded[i] = true;
					}

					Util::ChangeFormFavoritesStatus(p1, form, true);
					SPDLOG_DEBUG
					(
						"[EM] ImportCoopFavorites: {}: P1 has NOT favorited {}. "
						"Favoriting.",
						coopActor->GetName(), form->GetName()
					);
				}
			}

			// Hotkeyed by the companion player, so set the corresponding hotkey slot.
			if (hotkeyedFormsToSlotsMap.contains(form->formID))
			{
				auto slot = hotkeyedFormsToSlotsMap[form->formID];
				SPDLOG_DEBUG
				(
					"[EM] ImportCoopFavorites: {}: Adding hotkey {} for {}.",
					coopActor->GetName(), slot == -1 ? -1 : slot + 1, form->GetName()
				);
				Util::ChangeFormHotkeyStatus(p1, form, slot);
			}
		}
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

	void EquipManager::ReEquipAll(bool a_refreshBeforeEquipping)
	{
		// Re-equip all this player's desired forms.
		// Can also refresh the cached equip state before re-equipping.

		SPDLOG_DEBUG("[EM] ReEquipAll: {}.", coopActor->GetName());

		// Refresh all equipped forms before re-equipping.
		if (a_refreshBeforeEquipping)
		{
			RefreshEquipState(RefreshSlots::kAll);
		}

		auto inventoryCounts = coopActor->GetInventoryCounts();
		RE::TESForm* item{ nullptr };
		for (auto i = 0; i < desiredEquippedForms.size(); ++i)
		{
			item = desiredEquippedForms[i];
			// Do not include items without a loaded name,
			// such as the "SkinNaked" armor. 
			if (!item || strlen(item->GetName()) == 0)
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
				auto lhObj = desiredEquippedForms[!EquipIndex::kLeftHand];
				if (lhObj == item && 
					item->As<RE::BGSEquipType>()->equipSlot == glob.bothHandsEquipSlot)
				{
					continue;
				}
			}

			// Add item back to co-op player's inventory if missing.
			auto boundObj = item->IsBoundObject() ? item->As<RE::TESBoundObject>() : nullptr; 
			if (boundObj)
			{
				// Getting the inventory count for a light source 
				// with a valid bound object seems to crash the game;
				// exclude it from the check, along with forms that are not bound objects.
				if (!inventoryCounts.contains(boundObj) && 
					!item->Is(RE::FormType::Spell, RE::FormType::Shout, RE::FormType::Light))
				{
					coopActor->AddObjectToContainer
					(
						boundObj, (RE::ExtraDataList*)nullptr, 1, coopActor.get()
					);
				}
			}

			// Equip the cached item based on type.
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
				// Get the armor in the same slot.
				// Only equip if different.
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
					auto spell = item->As<RE::SpellItem>();
					auto equipSlot = glob.voiceEquipSlot;
					EquipSpell
					(
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

					EquipForm
					(
						item, 
						i == !EquipIndex::kLeftHand ? 
						EquipIndex::kLeftHand : 
						EquipIndex::kRightHand, 
						(RE::ExtraDataList*)nullptr, 
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
							!weap->IsStaff() && !desiredEquippedForms[!EquipIndex::kAmmo])
						{
							auto ammoAndCount = 
							(
								Settings::uAmmoAutoEquipMode == 
								!AmmoAutoEquipMode::kHighestDamage ? 
								Util::GetHighestDamageAmmo(coopActor.get(), weap->IsBow()) :
								Util::GetHighestCountAmmo(coopActor.get(), weap->IsBow())
							);
							// Valid ammo.
							if (ammoAndCount.first)
							{
								EquipAmmo(ammoAndCount.first);
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

	void EquipManager::ReEquipHandForms()
	{
		// Re-equip desired forms in this player's hands.

		SPDLOG_DEBUG("[EM] ReEquipHandForms: {}.", coopActor->GetName());

		// Interrupts Vampire Lord levitation, 
		// and Werewolves have no equipped items, so return here.
		if (p->isTransformed)
		{
			return;
		}

		auto lhForm = desiredEquippedForms[!EquipIndex::kLeftHand];
		auto rhForm = desiredEquippedForms[!EquipIndex::kRightHand];
		auto equipSlot = glob.eitherHandEquipSlot;
		auto lhEquipType = lhForm ? lhForm->As<RE::BGSEquipType>() : nullptr;
		auto rhEquipType = rhForm ? rhForm->As<RE::BGSEquipType>() : nullptr;
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
						aem->EquipSpell(p->coopActor.get(), rhSpell, equipSlot);
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
				// Anything else gets equipped normally to the RH slot.
				equipSlot = GetEquipSlotForForm(rhForm, EquipIndex::kRightHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject
						(
							p->coopActor.get(), 
							rhForm->As<RE::TESBoundObject>(),
							nullptr, 
							1, 
							equipSlot
						);
					}
				}
				else
				{
					EquipForm(rhForm, EquipIndex::kRightHand, nullptr, 1, equipSlot);
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
						aem->EquipSpell(p->coopActor.get(), lhSpell, equipSlot);
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
				// Everything else gets equipped normally to the LH slot.
				equipSlot = GetEquipSlotForForm(lhForm, EquipIndex::kLeftHand);
				if (p->isPlayer1)
				{
					if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
					{
						aem->EquipObject
						(
							p->coopActor.get(),
							lhForm->As<RE::TESBoundObject>(), 
							nullptr, 
							1, 
							equipSlot
						);
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

		SPDLOG_DEBUG("[EM] ReEquipVoiceForm: {}.", coopActor->GetName());
		UnequipFormAtIndex(EquipIndex::kVoice);

		auto toEquip = desiredEquippedForms[!EquipIndex::kVoice]; 
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

		SPDLOG_DEBUG
		(
			"[EM] RestoreP1Favorites: {}. Only magic favorites: {}.", 
			coopActor->GetName(), a_onlyMagicFavorites
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

		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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

			SPDLOG_DEBUG
			(
				"[EM] RestoreP1Favorites: {}: Remove P1 favorited magic {} at index {}. "
				"List size: {}.", 
				coopActor->GetName(),
				magForm ? magForm->GetName() : "NONE",
				removalIndex,
				magicFavorites->spells.size()
			);
			magicFavorites->RemoveFavorite(magForm);
		}

		// Clear out the list as well to be safe.
		magicFavorites->spells.clear();

		if (!a_onlyMagicFavorites)
		{
			for (auto i = 0; i < favoritedForms.size(); ++i)
			{
				const auto form = favoritedForms[i];
				if (!form || form->Is(RE::FormType::Spell, RE::FormType::Shout))
				{
					continue;
				}

				// Remove co-op player-favorited forms that P1 has not also favorited 
				// and does not have equipped.
				if (!favoritesIndicesInCommon[i])
				{
					Util::ChangeFormFavoritesStatus(p1, form, false);
					SPDLOG_DEBUG
					(
						"[EM] RestoreP1Favorites: {}: "
						"Unfavoriting not-in-common favorite {} for P1.",
						coopActor->GetName(), form->GetName()
					);

					// If the co-op player favorited form was added previously, 
					// remove it now.
					if (favoritedItemWasAdded[i])
					{
						SPDLOG_DEBUG
						(
							"[EM] RestoreP1Favorites: {}: Removing {} (x1) from P1.",
							coopActor->GetName(), form->GetName()
						);
						p1->RemoveItem
						(
							form->As<RE::TESBoundObject>(), 
							1, 
							RE::ITEM_REMOVE_REASON::kRemove,
							nullptr,
							nullptr
						);
					}
				}
				else
				{
					bool sharedHotkey = 
					{
						coopP1->em->hotkeyedFormsToSlotsMap.contains(form->formID) &&
						hotkeyedFormsToSlotsMap.contains(form->formID) &&
						coopP1->em->hotkeyedFormsToSlotsMap[form->formID] == 
						hotkeyedFormsToSlotsMap[form->formID]
					};
					if (!sharedHotkey)
					{
						// Remove hotkey on the shared favorited form
						// if P1 has not applied the same hotkey to the form.
						// If there is a different cached hotkey saved for this form,
						// it will get re-added below for P1.
						Util::ChangeFormHotkeyStatus(p1, form, -1);
					}
				}
			}
		}
		
		// Re-favorite all of P1's cached favorited forms and restore hotkeys.
		const auto& p1Favorites = coopP1->em->favoritedForms;
		for (auto form : p1Favorites)
		{
			if (!form)
			{
				continue;
			}

			if (a_onlyMagicFavorites || form->Is(RE::FormType::Spell, RE::FormType::Shout))
			{
				SPDLOG_DEBUG
				(
					"[EM] RestoreP1Favorites: {}: Refavoriting {} for P1.", 
					coopActor->GetName(), form->GetName()
				);
				magicFavorites->SetFavorite(form);
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[EM] RestoreP1Favorites: {}: Refavoriting {} for P1.",
					coopActor->GetName(), form->GetName()
				);
				Util::ChangeFormFavoritesStatus(p1, form, true);
			}

			if (coopP1->em->hotkeyedFormsToSlotsMap.contains(form->formID))
			{
				const auto slot = coopP1->em->hotkeyedFormsToSlotsMap[form->formID];
				SPDLOG_DEBUG
				(
					"[EM] RestoreP1Favorites: {}: Reapplying P1 hotkey {} for {}.",
					coopActor->GetName(), slot + 1, form->GetName()
				);
				Util::ChangeFormHotkeyStatus(p1, form, slot);
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

		SPDLOG_DEBUG
		(
			"[EM] RefreshEquipState: {}: "
			"slots to refresh: {}, form: {}, is equipped: {}.", 
			coopActor->GetName(),
			a_slots,
			a_formEquipped ? a_formEquipped->GetName() : "NONE",
			a_isEquipped
		);

		{
			std::unique_lock<std::mutex> lock(equipStateMutex, std::try_to_lock);
			if (lock)
			{
				SPDLOG_DEBUG
				(
					"[EM] RefreshEquipState: {}: Lock obtained. (0x{:X})", 
					coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				// Clear out cached equipped forms first.
				equippedForms.fill(nullptr);
				equippedFormFIDs.clear();

				if (a_slots == RefreshSlots::kWeapMag || a_slots == RefreshSlots::kAll)
				{
					// Get LH, RH objects, shout, and ammo.
					auto lhObj = coopActor->GetEquippedObject(true);
					auto rhObj = coopActor->GetEquippedObject(false);
					auto ammo = coopActor->GetCurrentAmmo();

					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: LH: {}, RH: {}",
						coopActor->GetName(),
						(lhObj) ? lhObj->GetName() : "NONE",
						(rhObj) ? rhObj->GetName() : "NONE"
					);
					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: Current ammo: {}.", 
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
						SPDLOG_ERROR
						(
							"[EM] RefreshEquipState: {}: 2H stuttering equip state bug "
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

					// Disable combat aim correction on arrows/bolts.
					if (ammo && ammo->data.projectile)
					{
						ammo->data.projectile->data.flags.set
						(
							RE::BGSProjectileData::BGSProjectileFlags::kDisableCombatAimCorrection
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
						!desiredEquippedForms.empty() ? 
						desiredEquippedForms[!EquipIndex::kVoice] :
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

					// Must equip highest count/damage ammo 
					// for co-op companions, since the ranged package,
					// which used to automatically equip the appropriate ammo, 
					// is no longer used for firing ranged weapons.
					if (Settings::uAmmoAutoEquipMode != !AmmoAutoEquipMode::kNone)
					{
						if (a_isEquipped && a_formEquipped && a_formEquipped->IsWeapon())
						{
							auto weap = a_formEquipped->As<RE::TESObjectWEAP>();
							bool isBound = weap->IsBound();
							bool isBow = weap->IsBow();
							bool isCrossbow = weap->IsCrossbow();
							// We already equip bound arrows elsewhere 
							// when a bound bow is equipped.
							if ((!isBound) && (isBow || isCrossbow))
							{
								auto desiredAmmo = 
								(
									desiredEquippedForms[!EquipIndex::kAmmo] ?
									desiredEquippedForms[!EquipIndex::kAmmo]->As<RE::TESAmmo>() : 
									nullptr
								);
								// Only auto equip if no current ammo, mismatching current ammo, 
								// or if equipping a bound bow.
								if ((!desiredAmmo) || 
									((desiredAmmo->IsBolt() && isBow) || 
									 (!desiredAmmo->IsBolt() && isCrossbow)))
								{
									// First, unequip what the game has cached as the current ammo.
									if (auto currentAmmo = coopActor->GetCurrentAmmo(); 
										currentAmmo && currentAmmo->IsBoundObject())
									{
										UnequipAmmo(currentAmmo);
									}

									auto ammoAndCount = 
									(
										Settings::uAmmoAutoEquipMode == 
										!AmmoAutoEquipMode::kHighestCount ?
										Util::GetHighestCountAmmo(coopActor.get(), isBow) :
										Util::GetHighestDamageAmmo(coopActor.get(), isBow)
									);
									if (ammoAndCount.first && 
										coopActor->GetCurrentAmmo() != ammoAndCount.first)
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

					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: "
						"Voice magic: {} (formType: 0x{:X}), Ammo: {}",
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
					desiredEquippedForms[!EquipIndex::kQuickSlotItem] = quickSlotItem;
					desiredEquippedForms[!EquipIndex::kQuickSlotSpell] = quickSlotSpell;

					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: Quick slot spell, item: {}, {}",
						coopActor->GetName(),
						(quickSlotSpell) ? quickSlotSpell->GetName() : "NONE",
						(quickSlotItem) ? quickSlotItem->GetName() : "NONE"
					);
					
					// NOTE: 
					// Not used right now, but may be enabled in the future
					// when grip switch is implemented and thoroughly tested.
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

					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: "
						"New armor ratings for light/heavy armor are: {}, {}",
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

					auto ammo = equippedForms[!EquipIndex::kAmmo];
					bool ammoIsBound = ammo && ammo->HasKeywordByEditorID("WeapTypeBoundArrow");
					if (!ammoIsBound)
					{
						desiredEquippedForms[!EquipIndex::kAmmo] = 
						equippedForms[!EquipIndex::kAmmo];
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
							SPDLOG_DEBUG
							(
								"[EM] RefreshEquipState: {}: "
								"MISMATCH at index {}: equipped {} vs. should have equipped {}.",
								coopActor->GetName(),
								i,
								equippedForms[i] ? equippedForms[i]->GetName() : "NOTHING",
								desiredEquippedForms[i] ? 
								desiredEquippedForms[i]->GetName() : 
								"NOTHING"
							);
							break;
						}
					}

					// Signal menu input manager to refresh menu equip state 
					// if it is currently running.
					if (glob.mim->IsRunning())
					{
						glob.mim->SignalRefreshMenuEquipState();
					}
				}

				auto& serializableEquippedForms = 
				(
					glob.serializablePlayerData.at(coopActor->formID)->equippedForms
				);
				// Save to serializable data if both equipped item lists are equivalent.
				if (!mismatch)
				{
					serializableEquippedForms = 
					(
						std::vector<RE::TESForm*>(!EquipIndex::kTotal, nullptr)
					);
					std::copy
					(
						equippedForms.begin(), 
						equippedForms.end(), 
						serializableEquippedForms.begin()
					);

					SPDLOG_DEBUG
					(
						"[EM] RefreshEquipState: {}: "
						"Copying list to serializable equipped forms list. "
						"LISTS MATCH. New equipped forms size: {}",
						coopActor->GetName(), 
						serializableEquippedForms.size()
					);
				}

#ifdef ALYSLC_DEBUG_MODE
				// REMOVE the following prints after sufficient debugging.
				for (auto item : equippedForms)
				{
					if (item)
					{
						equippedFormFIDs.insert(item->formID);
						SPDLOG_DEBUG
						(
							"[EM] RefreshAllEquippedItems: "
							"{} has a(n) {} in EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName()
						);
					}
				}

				for (auto item : desiredEquippedForms)
				{
					if (item)
					{
						SPDLOG_DEBUG
						(
							"[EM] RefreshAllEquippedItems: "
							"{} has a(n) {} in DESIRED EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName()
						);
					}
				}

				for (auto item : serializableEquippedForms)
				{
					if (item)
					{
						SPDLOG_DEBUG
						(
							"[EM] RefreshAllEquippedItems: "
							"{} has a(n) {} in SERIALIZABLE EQUIPPED forms list.", 
							coopActor->GetName(), item->GetName()
						);
					}
				}
#endif
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[EM] RefreshEquipState: "
					"{}: Failed to obtain lock. (0x{:X})", 
					coopActor->GetName(), 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}
		
	}

	void EquipManager::SetCopiedMagicAndFID
	(
		RE::TESForm* a_formToCopy, const PlaceholderMagicIndex& a_index
	)
	{
		// Save the spell form (and its FID) copied to the placeholder spell at the given index.

		SPDLOG_DEBUG
		(
			"[EM] SetCopiedMagicAndFID: {}: Form: {}, index: {}.", 
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

		SPDLOG_DEBUG("[EM] SetFavoritedEmoteIdles: {}.", coopActor->GetName());

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

		SPDLOG_DEBUG("[EM] SetInitialEquipState: {}.", coopActor->GetName());

		auto& savedEquippedForms = 
		(
			glob.serializablePlayerData.at(coopActor->formID)->equippedForms
		);
		if (savedEquippedForms.empty() || 
			savedEquippedForms.size() == 0 || 
			savedEquippedForms.size() != desiredEquippedForms.size())
		{
			SPDLOG_DEBUG
			(
				"[EM] ERR: SetInitialEquipState: "
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
			desiredEquippedForms.fill(nullptr);
			// Explicitly copy the saved equipped forms list into the desired forms list.
			std::copy
			(
				savedEquippedForms.begin(),
				savedEquippedForms.end(), 
				desiredEquippedForms.begin()
			);
		}

		// Ensure the placeholder spells hold a valid copied spell
		// before adding back to desired equipped forms.
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
					copiedSpell = 
					(
						copiedMagic[!PlaceholderMagicIndex::k2H] && 
						copiedMagic[!PlaceholderMagicIndex::k2H]->Is(RE::FormType::Spell) ?
						copiedMagic[!PlaceholderMagicIndex::k2H]->As<RE::SpellItem>() :
						nullptr
					);
					desiredEquippedForms[!EquipIndex::kLeftHand] = 
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
					desiredEquippedForms[!EquipIndex::kLeftHand] = 
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
					desiredEquippedForms[!EquipIndex::kRightHand] = 
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
					desiredEquippedForms[!EquipIndex::kRightHand] = 
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
					desiredEquippedForms[!EquipIndex::kVoice] = 
					(
						CopyToPlaceholderSpell(copiedSpell, PlaceholderMagicIndex::kVoice)
					);
				}
				else
				{
					desiredEquippedForms[!EquipIndex::kVoice] = voiceObj;
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
		desiredEquippedForms[!EquipIndex::kQuickSlotItem];

		auto spellForm = desiredEquippedForms[!EquipIndex::kQuickSlotSpell];
		// Ensure the serialized form is a spell before setting.
		quickSlotSpell =
		(
			spellForm && 
			spellForm->Is(RE::FormType::Spell) ? 
			spellForm->As<RE::SpellItem>() : 
			nullptr
		);
		equippedForms[!EquipIndex::kQuickSlotSpell] = 
		desiredEquippedForms[!EquipIndex::kQuickSlotSpell] =
		quickSlotSpell;

		if (p->isPlayer1)
		{
			// Update equipped forms list to reflect the current equip state for P1,
			// which may be different from the saved equip state
			// copied into the desired equipped forms list.
			// Get P1's equip state updated before the manager starts.
			RefreshEquipState(RefreshSlots::kAll);
		}
		else if (!p->isTransformed)
		{
			// NOTE:
			// Since the game may have equipped new gear
			// that does not match the serialized forms list, 
			// we do not perform an equip state refresh here for companion players.
			// Also, since there may be mismatches that need handling, 
			// all items are always re-equipped when the equip manager starts
			// and then the equip state is refreshed via the propagated equip events.
			// 
			// This call to unequip all will clear out mismatching forms 
			// that are not in the desired forms list 
			// after passing through the UnequipObject() hook.
			Util::Papyrus::UnequipAll(coopActor.get());
		}
	}

	void EquipManager::SetCyclableFavForms(CyclableForms a_favFormType)
	{
		// Set cyclable lists of favorited forms 
		// (ammo, hand spells, voice powers/shouts, and weapons) of the given type.

		SPDLOG_DEBUG("[EM] SetCyclableFavForms: {}.", coopActor->GetName());

		// Clear out before updating.
		cyclableFormsMap.insert_or_assign(a_favFormType, std::vector<RE::TESForm*>());
		if (a_favFormType == CyclableForms::kAmmo || a_favFormType == CyclableForms::kWeapon)
		{
			auto inventory = coopActor->GetInventory();
			for (const auto& [boundObj, entryDataPair] : inventory)
			{
				if (!boundObj || entryDataPair.first <= 0 || !entryDataPair.second.get()) 
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
			if (glob.serializablePlayerData.contains(coopActor->formID))
			{
				auto& data = glob.serializablePlayerData.at(coopActor->formID);
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

		SPDLOG_DEBUG
		(
			"[EM] SetCurrentVoiceSpell: {}: voice form: {}, voice spell: {}, "
			"highest shout var index: {}.", 
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
		if (a_weapon && !usesAmmo)
		{
			// 1H to 2H.
			if (a_weapon->equipSlot != glob.bothHandsEquipSlot)
			{
				UnequipForm
				(
					a_weapon, 
					a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, 
					(RE::ExtraDataList*)nullptr, 
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
					(RE::ExtraDataList*)nullptr, 
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
			if (GlobalCoopData::WEAP_ANIM_SWITCH_MAP.contains(currentType))
			{
				auto newType = GlobalCoopData::WEAP_ANIM_SWITCH_MAP.at(currentType);
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

				SPDLOG_DEBUG
				(
					"[EM] SwitchWeaponGrip: {}: "
					"Switched {}'s weapon animations from type {} to {}",
					coopActor->GetName(), a_weapon->GetName(), currentType, newType
				);
			}

			// Equip once grip and animation type have been changed.
			EquipForm
			(
				a_weapon, 
				a_equipRH ? EquipIndex::kRightHand : EquipIndex::kLeftHand, 
				(RE::ExtraDataList*)nullptr, 
				1, 
				a_weapon->GetEquipSlot()
			);
		}
	}

	void EquipManager::UnequipAll()
	{
		// Unequip all equipped gear after re-assigning saved equipped forms 
		// to the desired forms list.

		SPDLOG_DEBUG("[EM] UnequipAll: {}.", coopActor->GetName());

		// Re-assign saved serialized forms.
		desiredEquippedForms.fill(nullptr);
		auto& savedEquippedForms = 
		(
			glob.serializablePlayerData.at(coopActor->formID)->equippedForms
		);
		std::copy
		(
			savedEquippedForms.begin(),
			savedEquippedForms.end(), 
			desiredEquippedForms.begin()
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
				(equippedForms[i] != desiredEquippedForms[i] || 
				 equipIndex == EquipIndex::kLeftHand || 
				 equipIndex == EquipIndex::kRightHand))
			{
				desiredEquippedForms[i] = nullptr;
			}
		}

		Util::Papyrus::UnequipAll(coopActor.get());
	}

	void EquipManager::UnequipAmmo
	(
		RE::TESForm* a_toUnequip,
		RE::ExtraDataList* a_exData,
		const RE::BGSEquipSlot* a_slot, 
		bool a_queueEquip, 
		bool a_forceEquip,
		bool a_playSounds,
		bool a_applyNow,
		const RE::BGSEquipSlot* a_slotToReplace
	)
	{
		// Unequip the given ammo.

		SPDLOG_DEBUG
		(
			"[EM] UnequipAmmo: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto ammo = a_toUnequip ? a_toUnequip->As<RE::TESAmmo>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!ammo || !aem)
		{
			return;
		}

		auto invCounts = coopActor->GetInventoryCounts();
		auto currentAmmoCount = 
		(
			invCounts.contains(ammo) ? 
			invCounts.at(ammo) : 
			0
		);
		bool wasFavorited = Util::IsFavorited(coopActor.get(), a_toUnequip);
		int32_t hotkeyIndex = 
		(
			hotkeyedFormsToSlotsMap.contains(ammo->formID) ? 
			hotkeyedFormsToSlotsMap[ammo->formID] : 
			-1
		);

		// NOTE: 
		// The game has issues un/equipping ammo when count is large (e.g. 100000), 
		// so remove and re-add as a failsafe after unequipping.
		// Ugly but seems to work.
		if (p->isPlayer1)
		{
			aem->UnequipObject(coopActor.get(), ammo); 
			coopActor->RemoveItem
			(
				ammo, 
				currentAmmoCount,
				RE::ITEM_REMOVE_REASON::kRemove,
				nullptr,
				coopActor.get()
			);
		}
		else
		{
			// Clear from desired list first.
			ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kAmmo);
			aem->UnequipObject
			(
				coopActor.get(), 
				ammo, 
				a_exData,
				currentAmmoCount, 
				a_slot, 
				a_queueEquip,
				a_forceEquip,
				a_playSounds,
				a_applyNow, 
				a_slotToReplace
			);
			coopActor->RemoveItem
			(
				ammo, 
				currentAmmoCount,
				RE::ITEM_REMOVE_REASON::kRemove,
				nullptr,
				coopActor.get()
			);
		}

		// Have to re-favorite once re-added.
		if (wasFavorited)
		{
			Util::ChangeFormFavoritesStatus(coopActor.get(), a_toUnequip, true);
		}

		// Re-apply hotkey too.
		if (hotkeyIndex != -1)
		{
			Util::ChangeFormHotkeyStatus(coopActor.get(), a_toUnequip, hotkeyIndex);
		}
	}

	void EquipManager::UnequipArmor
	(
		RE::TESForm* a_toUnequip, 
		RE::ExtraDataList* a_exData,
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

		SPDLOG_DEBUG
		(
			"[EM] UnequipArmor: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr; 
		auto aem = RE::ActorEquipManager::GetSingleton(); 
		if (!boundObj || !aem)
		{
			return;
		}

		if (p->isPlayer1)
		{
			// Nothing special to do for P1.
			aem->UnequipObject
			(
				coopActor.get(), 
				boundObj,
				a_exData,
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
			// Must remove all armor entries that correspond to the requested armor to unequip,
			// since armor pieces can fit into multiple biped slots.
			if (auto asBipedObjForm = a_toUnequip->As<RE::BGSBipedObjectForm>(); asBipedObjForm)
			{
				// Remove from desired equipped forms list.
				auto slotMask = asBipedObjForm->bipedModelData.bipedObjectSlots;
				bool isShield = asBipedObjForm->IsShield();
				const RE::BGSEquipSlot* slot = 
				(
					isShield ? 
					a_toUnequip->As<RE::TESObjectARMO>()->equipSlot :
					a_slot
				);
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
					// Form mask contains the bit, 
					// so clear the corresponding desired equipped forms entry.
					if (slotMask.all(bitMask))
					{
						ClearDesiredEquippedFormOnUnequip(a_toUnequip, i);
					}
				}

				// Special shield case: also clear LH slot in desired equipped forms list.
				if (isShield)
				{
					ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kLeftHand);
				}

				aem->UnequipObject
				(
					coopActor.get(),
					boundObj,
					a_exData, 
					a_count,
					slot,
					a_queueEquip, 
					a_forceEquip,
					a_playSounds,
					a_applyNow,
					a_slotToReplace
				);
			}
			else
			{
				// All other forms.
				aem->UnequipObject
				(
					coopActor.get(), 
					boundObj, 
					a_exData,
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
	}

	void EquipManager::UnequipForm
	(
		RE::TESForm* a_toUnequip, 
		const EquipIndex& a_equipIndex,
		RE::ExtraDataList* a_exData,
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

		SPDLOG_DEBUG
		(
			"[EM] UnequipForm: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto boundObj = a_toUnequip ? a_toUnequip->As<RE::TESBoundObject>() : nullptr;
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!boundObj || !aem)
		{
			return;
		}

		// Special case if trying to unequip fists here.
		// Do not clear desired equipped forms entry.
		if (a_toUnequip == glob.fists)
		{
			aem->UnequipObject
			(
				coopActor.get(), 
				glob.fists, 
				nullptr, 
				1, 
				a_slot,
				false, 
				true, 
				false, 
				true
			);
			return;
		}

		if (p->isPlayer1)
		{
			// Nothing special to do for P1.
			aem->UnequipObject
			(
				coopActor.get(), 
				boundObj, 
				a_exData,
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
					ClearDesiredEquippedFormOnUnequip(a_toUnequip, !a_equipIndex);
				}
				else
				{
					ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kLeftHand);
					ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kRightHand);
				}
			}
			else
			{
				ClearDesiredEquippedFormOnUnequip(a_toUnequip, !a_equipIndex);
			}

			aem->UnequipObject
			(
				coopActor.get(),
				boundObj, 
				a_exData, 
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

		SPDLOG_DEBUG
		(
			"[EM] UnequipFormAtIndex: {}: index: {}.", 
			coopActor->GetName(), 
			a_equipIndex
		);

		// Handle special cases first. Make sure torch and shield are unequipped,
		// since they may have a lingering entry in the biped slots section 
		// of the equipped forms list that can cause problems.
		if ((a_equipIndex == EquipIndex::kLeftHand || a_equipIndex == EquipIndex::kShield) && 
			HasShieldEquipped())
		{
			UnequipShield();
		}

		if (auto currentForm = equippedForms[!a_equipIndex]; currentForm)
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
					if (equipType->equipSlot == glob.eitherHandEquipSlot)
					{
						// Unequipping from the "either hand" equip slot 
						// causes a "lingering equip state" bug
						// where the unequipped item still shows as equipped 
						// in the inventory/container menu,
						// and will require additional unequip requests to full unequip.
						// So we force the equip slot for our unequip call 
						// to match the passed-in equip index.
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
						// Otherwise, use the weapon's default equip slot.
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

				UnequipForm(currentForm, a_equipIndex, nullptr, 1, equipSlot);
			}
		}
	}

	void EquipManager::UnequipHandForms(RE::BGSEquipSlot* a_slot)
	{
		// Unequip form(s) in the given hand equip slot.

		SPDLOG_DEBUG
		(
			"[EM] UnequipHandForms: {}: slot: 0x{:X}.", 
			coopActor->GetName(),
			a_slot ? a_slot->formID : 0xDEAD
		);

		if (p->isPlayer1) 
		{
			auto aem = RE::ActorEquipManager::GetSingleton();
			if (!aem)
			{
				return;
			}

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
						aem->UnequipObject(coopActor.get(), lhBoundObj);
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
						aem->UnequipObject(coopActor.get(), rhBoundObj);
					}
				}
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
						aem->UnequipObject(coopActor.get(), lhBoundObj);
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
						aem->UnequipObject(coopActor.get(), rhBoundObj);
					}
				}
			}
		}
		else
		{
			if (a_slot == glob.bothHandsEquipSlot)
			{
				desiredEquippedForms[!EquipIndex::kLeftHand] = 
				desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
				UnequipFormAtIndex(EquipIndex::kLeftHand);
				UnequipFormAtIndex(EquipIndex::kRightHand);
				// Fists to ensure slots are cleared out. Fists!
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

		SPDLOG_DEBUG("[EM] UnequipShield: {}.", coopActor->GetName());

		auto shield = GetShield(); 
		auto aem = RE::ActorEquipManager::GetSingleton();
		if (!shield || !aem)
		{
			return;
		}

		if (p->isPlayer1)
		{
			aem->UnequipObject
			(
				coopActor.get(), 
				shield,
				nullptr, 
				1, 
				shield->equipSlot, 
				false,
				true,
				true, 
				true
			);
		}
		else
		{
			// Clear out LH slot first.
			desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;

			// Clear out biped slots.
			// Looping through just in case this shield has other biped slots
			// in addition to the shield biped slot.
			auto slotMask = shield->bipedModelData.bipedObjectSlots;
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
					ClearDesiredEquippedFormOnUnequip(shield, i);
				}
			}

			aem->UnequipObject
			(
				coopActor.get(), 
				shield,
				nullptr, 
				1, 
				shield->equipSlot, 
				false, 
				true, 
				true, 
				true
			);
		}
	}

	void EquipManager::UnequipShout(RE::TESForm* a_toUnequip)
	{
		// Unequip the given shout.

		SPDLOG_DEBUG
		(
			"[EM] UnequipShout: {}: unequip {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE"
		);

		auto shout = a_toUnequip ? a_toUnequip->As<RE::TESShout>() : nullptr;
		if (!shout)
		{
			return;
		}

		if (!p->isPlayer1)
		{
			ClearDesiredEquippedFormOnUnequip(a_toUnequip, !EquipIndex::kVoice);
		}

		Util::NativeFunctions::UnequipShout(coopActor.get(), shout);
	}

	void EquipManager::UnequipSpell(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex)
	{
		// Unequip the given spell from the given equip index.

		SPDLOG_DEBUG("[EM] UnequipSpell: {}: unequip {}, index: {}.",
			coopActor->GetName(),
			a_toUnequip ? a_toUnequip->GetName() : "NONE",
			a_equipIndex);

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
				ClearDesiredEquippedFormOnUnequip(spell, !a_equipIndex);
				Util::NativeFunctions::UnequipSpell(coopActor.get(), spell, gameSlotIndex);
			}
			else
			{
				// Equip "fists" to clear out spell slots.
				ClearDesiredEquippedFormOnUnequip(spell, !EquipIndex::kLeftHand);
				ClearDesiredEquippedFormOnUnequip(spell, !EquipIndex::kRightHand);
				EquipFists();
			}
		}
	}

	void EquipManager::UpdateFavoritedFormsLists(bool&& a_useCachedMagicFavorites)
	{
		// Update all favorited form data and list(s) to serialize (physical and magical).
		// Also update assigned hotkeys for all favorited forms.
		// Can choose to simply assign the cached serialized magic favorites list,
		// instead of constructing an up-to-date list from the magic favorites singleton.

		SPDLOG_DEBUG
		(
			"[EM] UpdateFavoritedFormsLists: {}. Use cached magic favorites: {}.", 
			coopActor->GetName(), a_useCachedMagicFavorites
		);

		favoritedForms.clear();
		favoritedFormIDs.clear();
		cyclableFormsMap.clear();
		cyclableFormsMap[CyclableForms::kAmmo] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kSpell] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kVoice] = std::vector<RE::TESForm*>();
		cyclableFormsMap[CyclableForms::kWeapon] = std::vector<RE::TESForm*>();
		hotkeyedForms.fill(nullptr);
		hotkeyedFormsToSlotsMap.clear();

		// Total number of favorited items for this player.
		uint32_t numFavoritedItems = 0;
		// Physical forms first.
		auto inventory = coopActor->GetInventory();
		for (auto& [boundObj, entryDataPair] : inventory)
		{
			if (!boundObj || entryDataPair.first <= 0 || !entryDataPair.second.get())
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
				if (!exDataList || !exDataList->HasType(RE::ExtraDataType::kHotkey))
				{
					continue;
				}

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

				auto exDataHotkey = exDataList->GetByType<RE::ExtraHotkey>(); 
				if (!exDataHotkey)
				{
					continue;
				}

				// Item was hotkeyed.
				if ((int8_t)(*exDataHotkey->hotkey) != (int8_t)(RE::ExtraHotkey::Hotkey::kUnbound))
				{
					auto slot = (int8_t)(*exDataHotkey->hotkey);
					auto oldHotkeyedForm = hotkeyedForms[slot];
					if (oldHotkeyedForm && 
						oldHotkeyedForm != boundObj && 
						hotkeyedFormsToSlotsMap.contains(oldHotkeyedForm->formID)) 
					{
						// Form is hotkeyed in this slot but also in another slot previously, 
						// so remove the previously linked hotkey.
						SPDLOG_DEBUG
						(
							"[EM] UpdateFavoritedFormsLists: {}: "
							"FORM {} was already hotkeyed in slot {}. "
							"Not saving {} as hotkeyed and now removing its duplicate hotkey.",
							coopActor->GetName(),
							oldHotkeyedForm->GetName(), 
							slot == -1 ? -1 : slot + 1, 
							boundObj->GetName()
						);

						hotkeyedFormsToSlotsMap.erase(oldHotkeyedForm->formID);
						exDataHotkey->hotkey = RE::ExtraHotkey::Hotkey::kUnbound;
					}
					else
					{
						// Assign form to previously empty hotkey slot 
						// or link a new hotkey slot to this form.
						hotkeyedForms[slot] = boundObj;
						hotkeyedFormsToSlotsMap.insert_or_assign(boundObj->formID, slot);

						SPDLOG_DEBUG
						(
							"[EM] UpdateFavoritedFormsLists: {}: "
							"PHYS FORM {} is hotkeyed in slot {}.",
							coopActor->GetName(),
							boundObj->GetName(), 
							slot == -1 ? -1 : slot + 1
						);
					}
				}

				SPDLOG_DEBUG("[EM] UpdateFavoritedFormsLists: {}. ITEM {} is favorited.",
					coopActor->GetName(), boundObj->GetName());
				++numFavoritedItems;
			}
		}

		if (!glob.serializablePlayerData.contains(coopActor->formID))
		{
			SPDLOG_DEBUG
			(
				"[EM] ERR: UpdateFavoritedFormsLists: {}: "
				"No serialized data found. Cannot update or modify cached magic favorites.", 
				coopActor->GetName()
			);
			return;
		}

		auto& data = glob.serializablePlayerData.at(coopActor->formID);
		auto magicFavorites = RE::MagicFavorites::GetSingleton();
		if (!magicFavorites)
		{
			SPDLOG_DEBUG
			(
				"[EM] ERR: UpdateFavoritedFormsLists: {}: "
				"Could not get magic favorites singleton.", 
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
			for (auto i = 0; i < data->hotkeyedForms.size(); ++i) 
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
					hotkeyedFormsToSlotsMap.contains(oldHotkeyedForm->formID))
				{
					SPDLOG_DEBUG
					(
						"[EM] UpdateFavoritedFormsLists: {}: "
						"SAVED OLD HOTKEYED FORM {} will remain in slot {}, instead of {}.",
						coopActor->GetName(),
						oldHotkeyedForm->GetName(),
						i + 1,
						hotkeyedForm->GetName()
					);
					continue;
				}

				hotkeyedForms[i] = hotkeyedForm;
				hotkeyedFormsToSlotsMap.insert_or_assign(hotkeyedForm->formID, i);
				SPDLOG_DEBUG
				(
					"[EM] UpdateFavoritedFormsLists: {}: "
					"SAVED MAGIC FORM {} is hotkeyed in slot {}.", 
					coopActor->GetName(), hotkeyedForm->GetName(), i + 1
				);
			}
		}
		else
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

			// Update our hotkey data based on the current magical favorites list.
			for (auto i = 0; i < magicFavorites->hotkeys.size(); ++i)
			{
				auto magForm = magicFavorites->hotkeys[i]; 
				if (!magForm)
				{
					continue;
				}

				auto oldHotkeyedForm = hotkeyedForms[i];
				if (oldHotkeyedForm &&
					oldHotkeyedForm != magForm &&
					hotkeyedFormsToSlotsMap.contains(oldHotkeyedForm->formID))
				{
					// Form is hotkeyed in this slot but also in another slot previously,
					// so remove the old linked hotkey.
					// This magic form should have hotkey precedence over the previous form,
					// since it is not a cached magic form and is more up to date.
					SPDLOG_DEBUG
					(
						"[EM] UpdateFavoritedFormsLists: {}: "
						"FORM {} was already hotkeyed in slot {}. "
						"Not saving {} as hotkeyed and now removing its duplicate hotkey.",
						coopActor->GetName(), 
						oldHotkeyedForm->GetName(), 
						i == -1 ? -1 : i + 1, 
						magForm->GetName()
					);
					hotkeyedFormsToSlotsMap.erase(oldHotkeyedForm->formID);
					magicFavorites->hotkeys[i] = nullptr;
				}
				else
				{
					// Assign form to previously empty hotkey slot
					// or link a new hotkey slot to this form.
					hotkeyedForms[i] = magForm;
					hotkeyedFormsToSlotsMap.insert_or_assign(magForm->formID, i);
					SPDLOG_DEBUG
					(
						"[EM] UpdateFavoritedFormsLists: {}: "
						"MAGIC FORM {} is hotkeyed in slot {}.",
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

			SPDLOG_DEBUG
			(
				"[EM] UpdateFavoritedFormsLists: {}. SPELL {} is favorited.",
				coopActor->GetName(), magForm->GetName()
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
				uint32_t prevSize = favFormsList.size();
				favFormsList.erase(newEnd, favFormsList.end());
			}
		}
	}

	void EquipManager::ValidateEquipState()
	{
		// Try to force the game to re-equip the player's desired forms
		// without equip slot mismatches and other issues.
		// Exists to deal with the game's equip system or my own code's BS.

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
		// Check if desired LH/RH form is not in the corresponding hand
		// or if fists are equipped when there is a desired LH or RH form.
		bool shouldCheckHandForms =
		(
			(!currentLHForm && desiredLHForm) ||
			(!currentRHForm && desiredRHForm) ||
			((currentLHForm == glob.fists) && (desiredLHForm || desiredRHForm))
		);
		bool shouldReEquip = false;

		// Fix and modify desired equipped forms entries.
		if (shouldCheckHandForms) 
		{
			// 2H not in both slots. Rectifiable.
			if (desiredLHEquipType && desiredLHEquipType->equipSlot == glob.bothHandsEquipSlot)
			{
				if (!desiredRHForm)
				{
					// RH empty and LH form should also be in the RH slot.
					SPDLOG_DEBUG
					(
						"[EM] ValidateEquipState: {}: "
						"2H form {} should be in both LH and RH desired form slots. "
						"Adding to RH slot now.",
						coopActor->GetName(), 
						desiredLHForm->GetName()
					);

					desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
					shouldReEquip = true;
				}
				else if (!desiredRHEquipType || 
						 desiredRHEquipType->equipSlot != glob.bothHandsEquipSlot)
				{
					// RH form invalid or RH form is not a 2H form.
					// Move LH form into RH slot.
					SPDLOG_DEBUG
					(
						"[EM] ValidateEquipState: {}: "
						"2H form {} is not also in the RH slot and RH form {} is not a 2H form. "
						"Moving 2H form to RH.",
						coopActor->GetName(), 
						desiredLHForm->GetName(), 
						desiredRHForm ? desiredRHForm->GetName() : "INVALID"
					);

					desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
					shouldReEquip = true;
				}
			}
			else if (desiredRHEquipType && 
					 desiredRHEquipType->equipSlot == glob.bothHandsEquipSlot)
			{
				if (!desiredLHForm)
				{
					// LH empty and RH form should also be in the LH slot.
					SPDLOG_DEBUG
					(
						"[EM] ValidateEquipState: {}: "
						"2H form {} should be in both LH and RH desired form slots. "
						"Adding to LH slot now.",
						coopActor->GetName(), 
						desiredRHForm->GetName()
					);

					desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
					shouldReEquip = true;
				}
				else if (!desiredLHEquipType || 
						 desiredLHEquipType->equipSlot != glob.bothHandsEquipSlot)
				{
					// LH form invalid or LH form is not a 2H form.
					// Move RH form into LH slot.
					SPDLOG_DEBUG
					(
						"[EM] ValidateEquipState: {}: "
						"2H form {} is not also in the LH slot and LH form {} is not a 2H form. "
						"Moving 2H form to LH.",
						coopActor->GetName(), 
						desiredRHForm->GetName(), 
						desiredLHForm ? desiredLHForm->GetName() : "INVALID"
					);

					desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
					shouldReEquip = true;
				}
			}

			// Mismatching equip slots. Rectifiable.
			// NOTE:
			// Most items with the RH equip slot can be equipped in the LH equip slot, 
			// so I'm commenting this out for now.
			/*if (desiredLHEquipType && desiredLHEquipType->equipSlot == glob.rightHandEquipSlot)
			{
				SPDLOG_DEBUG
				(
					"[EM] ValidateEquipState: {}: "
					"LH form {} has RH equip slot. Moving to RH.",
					coopActor->GetName(), desiredLHForm->GetName()
				);
				desiredEquippedForms[!EquipIndex::kRightHand] = desiredLHForm;
				desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
				shouldReEquip = true;
			}*/

			// RH form with a LH equipslot is in the desired RH slot,
			// so move it to the desired LH slot and clear the desired RH slot.
			if (desiredRHEquipType && desiredRHEquipType->equipSlot == glob.leftHandEquipSlot)
			{
				SPDLOG_DEBUG
				(
					"[EM] ValidateEquipState: {}: " 
					"RH form {} has LH equip slot. Moving to LH.",
					coopActor->GetName(), desiredRHForm->GetName()
				);
				desiredEquippedForms[!EquipIndex::kLeftHand] = desiredRHForm;
				desiredEquippedForms[!EquipIndex::kRightHand] = nullptr;
				shouldReEquip = true;
			}
		}
		else
		{
			// 1H weapon that the player only owns 1 of, is in both LH and RH slots. Rectifiable.
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
					SPDLOG_DEBUG
					(
						"[EM] ValidateEquipState: {}: "
						"1H form {} with count 1 is in both LH and RH desired form slots. "
						"Unequipping from the LH slot now.",
						coopActor->GetName(), currentLHForm->GetName()
					);

					desiredEquippedForms[!EquipIndex::kLeftHand] = nullptr;
					desiredEquippedForms[!EquipIndex::kRightHand] = weap;
					shouldReEquip = true;
				}
			}
		}

		// If there were no corrections made to the desired forms list,
		// check if there are equip mismatches between the current and desired hand/voice forms,
		// and re-equip the desired forms.
		// Do not re-equip desired hand forms when there is at least one bound weapon equipped.
		if (!shouldReEquip) 
		{
			shouldReEquip |= 
			(
				(!p->pam->boundWeapReq2H && !p->pam->boundWeapReqLH && !p->pam->boundWeapReqRH) && 
				(currentLHForm != desiredLHForm || currentRHForm != desiredRHForm)
			);
			if (shouldReEquip) 
			{
				SPDLOG_DEBUG
				(
					"[EM] ValidateEquipState: {}: "
					"Current LH form ({}, is bound weap req active: {}, {}) "
					"does not match desired form ({}): {}, "
					"current RH form ({}, is bound weap req active: {}, {}) "
					"does not match desired form ({}): {}, "
					"current Voice form ({}) "
					"does not match desired form ({}): {}",
					coopActor->GetName(), 
					currentLHForm ? currentLHForm->GetName() : "EMPTY",
					p->pam->boundWeapReqLH,
					p->pam->boundWeapReq2H,
					desiredLHForm ? desiredLHForm->GetName() : "EMPTY",
					currentLHForm != desiredLHForm,
					currentRHForm ? currentRHForm->GetName() : "EMPTY",
					p->pam->boundWeapReqRH,
					p->pam->boundWeapReq2H,
					desiredRHForm ? desiredRHForm->GetName() : "EMPTY",
					currentRHForm != desiredRHForm,
					currentVoiceForm ? currentVoiceForm->GetName() : "NONE",
					desiredVoiceForm ? desiredVoiceForm->GetName() : "NONE",
					currentVoiceForm != desiredVoiceForm
				);
			}
		}

		if (shouldReEquip)
		{
			SPDLOG_DEBUG
			(
				"[EM] ValidateEquipState: {}: "
				"Desired equipped forms were invalid and modified. "
				"Re-equipping desired equipped hand forms.", 
				coopActor->GetName()
			);
			ReEquipHandForms();
		}
	}
}

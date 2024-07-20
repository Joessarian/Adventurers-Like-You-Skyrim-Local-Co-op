#pragma once
#include <Enums.h>
#include <Player.h>

namespace ALYSLC
{
	class CoopPlayer;
	// Maintains an up-to-date view of all equipped forms and offers
	// cycling of equippable forms when pressing different hotkeys.
	struct EquipManager : public Manager
	{
		// Constructors.
		EquipManager();
		// Delayed construction after the player is default-constructed 
		// and the player shared pointer is added to the list of co-op players in the global data holder.
		void Initialize(std::shared_ptr<CoopPlayer> a_p);

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		// Remove form from desired forms list when it is unequipped.
		inline void ClearDesiredEquippedFormOnUnequip(RE::TESForm* a_toUnequip, const uint32_t& a_listIndex, const RE::BGSEquipSlot* a_slot) 
		{
			if (!a_toUnequip)
			{
				return;
			}

			// If the requested form to unequip is not the same as the one already in this slot, do not clear the slot.
			bool diffFormAlreadyInSlot = desiredEquippedForms[a_listIndex] && desiredEquippedForms[a_listIndex] != a_toUnequip;
			if (!diffFormAlreadyInSlot)
			{
				desiredEquippedForms[a_listIndex] = nullptr;
			}
		}

		// Get all currently equipped forms for this player.
		inline const std::array<RE::TESForm*, (size_t)EquipIndex::kTotal>& GetAllEquippedForms() const
		{
			return equippedForms;
		}

		// Get form ID for the spell copied into the placeholder spell at the given index.
		inline RE::FormID GetCopiedMagicFormID(const PlaceholderMagicIndex& a_index) const
		{
			return !a_index >= 0 && !a_index < !PlaceholderMagicIndex::kTotal ? copiedMagicFormIDs[!a_index] : 0;
		}

		// Get the spell form copied into the placeholder spell at the given index.
		inline RE::TESForm* GetCopiedMagic(const PlaceholderMagicIndex& a_index) const
		{
			return !a_index >= 0 && !a_index < !PlaceholderMagicIndex::kTotal ? copiedMagic[!a_index] : nullptr;
		}

		// Get the spell corresponding to the highest shout variation that the player knows 
		// for the currently equipped shout.
		inline RE::SpellItem* GetHighestShoutVariation() const
		{
			return voiceSpell;
		}

		// Get the placeholder spell form at the given index.
		inline RE::TESForm* GetPlaceholderMagic(const PlaceholderMagicIndex& a_index) const
		{
			return !a_index >= 0 && !a_index < !PlaceholderMagicIndex::kTotal ? placeholderMagic[!a_index] : nullptr;
		}
		
		// Get the max reach for any equipped weapons.
		// Not sure what the base reach is in in-game units, so the weapon's
		// reach is multiplied by the actor's height.
		inline float GetMaxWeapReach() const
		{
			const auto lhForm = equippedForms[!EquipIndex::kLeftHand];
			const auto lhWeapReach = (lhForm && lhForm->As<RE::TESObjectWEAP>()) ?
										 lhForm->As<RE::TESObjectWEAP>()->GetReach() * coopActor->GetHeight() :
										   coopActor->race->data.unarmedReach;
			const auto rhForm = equippedForms[!EquipIndex::kRightHand];
			const auto rhWeapReach = (rhForm && rhForm->As<RE::TESObjectWEAP>()) ?
										 rhForm->As<RE::TESObjectWEAP>()->GetReach() * coopActor->GetHeight() :
										   coopActor->race->data.unarmedReach;
			return (lhWeapReach > rhWeapReach) ? lhWeapReach : rhWeapReach;
		}

		// Get the spell equipped in the left hand, if any.
		inline RE::SpellItem* GetLHSpell() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; lhObj) 
			{
				return lhObj->As<RE::SpellItem>();
			}

			return nullptr;
		}

		// Get the weapon equipped in the left hand, if any.
		inline RE::TESObjectWEAP* GetLHWeapon() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; lhObj)
			{
				return lhObj->As<RE::TESObjectWEAP>();
			}

			return nullptr;
		}

		// Get the spell equipped in the right hand, if any.
		inline RE::SpellItem* GetRHSpell() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; rhObj)
			{
				return rhObj->As<RE::SpellItem>();
			}

			return nullptr;
		}

		// Get the weapon equipped in the right hand, if any.
		inline RE::TESObjectWEAP* GetRHWeapon() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; rhObj)
			{
				return rhObj->As<RE::TESObjectWEAP>();
			}

			return nullptr;
		}

		// Get equipped shield, if any.
		inline RE::TESObjectARMO* GetShield() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; lhObj)
			{
				if (auto lhArmor = lhObj->As<RE::TESObjectARMO>(); lhArmor && lhArmor->IsShield()) 
				{
					return lhArmor;
				}
			}

			if (const auto shieldBipedObj = equippedForms[!EquipIndex::kShield]; shieldBipedObj) 
			{
				if (auto shield = shieldBipedObj->As<RE::TESObjectARMO>(); shield) 
				{
					return shield;
				}
			}

			return nullptr;
		}

		// Check if the given hand is empty or not.
		inline bool HandIsEmpty(const bool& a_rightHand) const
		{
			return a_rightHand ? !equippedForms[!EquipIndex::kRightHand] : !equippedForms[!EquipIndex::kLeftHand];
		}

		// Check if the player has a two hand melee weapon equipped.
		inline bool Has2HMeleeWeapEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return (rhObj && rhObj->IsWeapon() && 
					(rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedAxe() ||
					rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedSword()));
		}

		// Check if the player has a two hand ranged weapon equipped.
		inline bool Has2HRangedWeapEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return 
			{
				(rhObj && rhObj->As<RE::TESObjectWEAP>()) &&
				(rhObj->As<RE::TESObjectWEAP>()->IsBow() ||
				 rhObj->As<RE::TESObjectWEAP>()->IsCrossbow())
			};
		}

		// Check if the player has a spell equipped in their right hand.
		inline bool Has2HSpellEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return
			{ 
				rhObj && rhObj->As<RE::SpellItem>() && 
				rhObj->As<RE::BGSEquipType>()->equipSlot &&
				rhObj->As<RE::BGSEquipType>()->equipSlot->flags.all(RE::BGSEquipSlot::Flag::kUseAllParents) 
			};
		}

		// Check if the player has a bow equipped.
		inline bool HasBowEquipped() const
		{
			return (Has2HRangedWeapEquipped() && equippedForms[!EquipIndex::kRightHand]->As<RE::TESObjectWEAP>()->IsBow());
		}

		// Check if the player has a crossbow equipped.
		inline bool HasCrossbowEquipped() const
		{
			return (Has2HRangedWeapEquipped() && equippedForms[!EquipIndex::kRightHand]->As<RE::TESObjectWEAP>()->IsCrossbow());
		}

		// Check if the player has a one hand melee weapon equipped in their left hand.
		inline bool HasLHMeleeWeapEquipped() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; lhObj && lhObj->IsWeapon())
			{
				const auto lhWeapType = !lhObj->As<RE::TESObjectWEAP>()->GetWeaponType();
				return (lhWeapType >= 1 && lhWeapType <= 4);
			}

			return false;
		}

		// Check if the player has a spell equipped in their left hand.
		inline bool HasLHSpellEquipped() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			return (lhObj && lhObj->As<RE::SpellItem>());
		}

		// Check if the player has a staff equipped in their left hand.
		inline bool HasLHStaffEquipped() const 
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			return (lhObj && lhObj->Is(RE::FormType::Weapon) && lhObj->As<RE::TESObjectWEAP>()->IsStaff());
		}

		// Check if the player has a one hand weapon equipped in their left hand.
		inline bool HasLHWeapEquipped() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; lhObj && lhObj->IsWeapon())
			{
				const auto lhWeapType = !lhObj->As<RE::TESObjectWEAP>()->GetWeaponType();
				return (lhWeapType >= 1 && lhWeapType <= 4) || lhWeapType == 8;
			}

			return false;
		}

		// Check if the player has a one hand or two hand ranged weapon equipped in either hand.
		inline bool HasRangedWeapEquipped() const
		{
			return Has2HRangedWeapEquipped() || HasLHStaffEquipped() || HasRHStaffEquipped();
		}

		// Check if the player has a one/two hand ranged weapon or spell equipped in either hand.
		inline bool HasRangedWeapOrSpellEquipped() const
		{
			return Has2HRangedWeapEquipped() || HasLHSpellEquipped() || HasRHSpellEquipped() || HasLHStaffEquipped() || HasRHStaffEquipped();
		}

		// Check if the player has a one hand melee weapon equipped in their right hand.
		inline bool HasRHMeleeWeapEquipped() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; rhObj && rhObj->IsWeapon())
			{
				const auto rhWeapType = !rhObj->As<RE::TESObjectWEAP>()->GetWeaponType();
				return (rhWeapType >= 1 && rhWeapType <= 4);
			}

			return false;
		}

		// Check if the player has a spell equipped in their right hand.
		inline bool HasRHSpellEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return (rhObj && rhObj->As<RE::SpellItem>());
		}

		// Check if the player has a staff equipped in their right hand.
		inline bool HasRHStaffEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return (rhObj && rhObj->Is(RE::FormType::Weapon) && rhObj->As<RE::TESObjectWEAP>()->IsStaff());
		}

		// Check if the player has a one hand weapon equipped in their right hand.
		inline bool HasRHWeapEquipped() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; rhObj && rhObj->IsWeapon())
			{
				const auto rhWeapType = !rhObj->As<RE::TESObjectWEAP>()->GetWeaponType();
				return (rhWeapType >= 1 && rhWeapType <= 4) || rhWeapType == 8;
			}
			
			return false;
		}

		// Check if the player has a shield equipped.
		inline bool HasShieldEquipped() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];

			return ((lhObj && lhObj->IsArmor() && lhObj->As<RE::TESObjectARMO>()->IsShield()) ||
					(rhObj && rhObj->IsArmor() && rhObj->As<RE::TESObjectARMO>()->IsShield()));
		}

		// Check if the player has a torch equipped.
		inline bool HasTorchEquipped() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			return (lhObj && lhObj->Is(RE::FormType::Light) && lhObj->As<RE::TESObjectLIGH>()->CanBeCarried());
		}

		// Check if the given form is equipped (in the player's equipped forms list or has ExtraWorn data in its inventory entry).
		inline bool IsEquipped(RE::TESForm* a_form) 
		{
			if (!a_form)
			{
				return false;
			}

			if (equippedFormFIDs.contains(a_form->formID))
			{
				return true;
			}

			if (auto invChanges = coopActor->GetInventoryChanges(); invChanges && invChanges->entryList)
			{
				for (auto invChangesEntry : *invChanges->entryList)
				{
					if (invChangesEntry && invChangesEntry->object && invChangesEntry->object == a_form)
					{
						return invChangesEntry->IsWorn();
					}
				}
			}

			return false;
		}

		// Check if the player is dual wielding weapons.
		inline bool IsDualWielding() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return lhObj && rhObj && lhObj->IsWeapon() && rhObj->IsWeapon() &&
				   (!rhObj->As<RE::TESObjectWEAP>()->IsRanged() &&
					   !rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedAxe() &&
					   !rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedSword());
		}

		// Check if the player is unarmed.
		inline bool IsUnarmed() const 
		{
			return (!equippedForms[!EquipIndex::kLeftHand] && !equippedForms[!EquipIndex::kRightHand]);
		}

		// Check if the player's left hand is empty.
		inline bool LHEmpty() const 
		{
			return !equippedForms[!EquipIndex::kLeftHand];
		}

		// Check if the player only has melee weapons equipped.
		inline bool OnlyHasMeleeWeapsEquipped() const
		{
			return !HasRangedWeapOrSpellEquipped();
		}

		// Check if the player's right hand is empty.
		inline bool RHEmpty() const
		{
			return !equippedForms[!EquipIndex::kRightHand];
		}

		// Unequip all forms and clear the player's desired forms list.
		inline void UnequipAllAndResetEquipState() 
		{
			desiredEquippedForms.fill(nullptr);
			Util::Papyrus::UnequipAll(coopActor.get());
		}
		
		//
		// Member funcs
		//

		// NOTE: Currently unused since the 'Shout' package procedure does not work.
		//RE::TESShout* CopyToPlaceholderShout(RE::TESShout* a_shoutToCopy);

		// Copy the given spell to the placeholder spell at the given index.
		RE::SpellItem* CopyToPlaceholderSpell(RE::SpellItem* a_spellToCopy, const PlaceholderMagicIndex& a_index);
		
		// Cycle to the next favorited ammo choice that matches the currently equipped ranged weapon.
		void CycleAmmo();
		
		// Cycle to the next favorited emote idle.
		void CycleEmoteIdles();
		
		// Cycle to the next favorited spell that is equipable in the given hand and is also in the current spell cycling category.
		void CycleHandSlotMagic(bool&& a_rightHand);
		
		// Cycle to the next available hand slot magic category for the given hand.
		void CycleHandSlotMagicCategory(bool&& a_rightHand);
		
		// Cycle to the next favorited voice slot magic spell/shout.
		void CycleVoiceSlotMagic();
		
		// Cycle to the next available weapon category for the given hand.
		void CycleWeaponCategory(bool&& a_rightHand);
		
		// Cycle to the next favorited weapon that is equipable in the given hand and is also in the current weapon cycling category.
		void CycleWeapons(bool&& a_rightHand);
		
		// Equip ammo and update desired forms for co-op companion players.
		void EquipAmmo(RE::TESForm* a_toEquip, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true);
		
		// Equip armor and update desired forms for co-op companion players.
		void EquipArmor(RE::TESForm* a_toEquip, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, uint32_t a_count = 1, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true);
		
		// Equip fists to clear out hand slots.
		// NOTE: Does not clear desired hand slot forms.
		void EquipFists();
		
		// Equip form and update desired forms for co-op companion players.
		void EquipForm(RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, uint32_t a_count = 1, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true);
		
		// Equip shout and update desired forms for co-op companion players.
		void EquipShout(RE::TESForm* a_toEquip);
		
		// Equip spell and update desired forms for co-op companion players.
		void EquipSpell(RE::TESForm* a_toEquip, const EquipIndex& a_equipIndex, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr);
		
		// Get name for the given favorited magic cycling category.
		std::string_view FavMagCyclingCategoryToString(const FavMagicCyclingCategory& a_category) const;
		
		// Get name for the given weapon magic cycling category.
		std::string_view FavWeaponCyclingCategoryToString(const FavWeaponCyclingCategory& a_category) const;
		
		// NOTE: Unused for now, but keeping for reference or if needed again in the future.
		// Get equipable spells in the hand slots or powers/shouts in voice slot.
		// Checks all of player 1's known spells/shouts and this player's known spells/shouts to compile the list.
		std::vector<RE::TESForm*> GetEquipableSpells(bool a_inHandSlot) const;
		
		// Get the equip slot for the given form at the given equip index.
		RE::BGSEquipSlot* GetEquipSlotForForm(RE::TESForm* a_form, const EquipIndex& a_index) const;
		
		// Get a list of all physical (non-magic) spells favorited by this player.
		// Can update the list before returning the result.
		std::vector<RE::TESForm*> GetFavoritedPhysForms(bool a_shouldUpdate);
		
		// Get the total weight of all equipped forms.
		float GetWornWeight() const;
		
		// Un/equip the desired form at the given index.
		// NOTE: Not currently used and should never be called on P1.
		void HandleEquipRequest(RE::TESForm* a_form, const EquipIndex& a_index, bool a_shouldEquip);
		
		// Un/equip the desired form from the given container at the given index.
		// If a placeholder spell has changed, re-copy over the requested spell before equipping.
		void HandleMenuEquipRequest(RE::ObjectRefHandle a_fromContainerHandle, RE::TESForm* a_form, const EquipIndex& a_index, bool a_placeholderMagicChanged);
		
		// Checks if the player has a favorited spell in the given category that matches the given hand.
		bool HasCyclableSpellInCategory(const FavMagicCyclingCategory& a_category);
		
		// Checks if the player has a favorited weapon in the given category that matches the given hand.
		bool HasCyclableWeaponInCategory(const FavWeaponCyclingCategory& a_category, const bool& a_rightHand);
		
		// Add favorited items from this player to P1 as needed, favorite them, and unfavorite all P1's favorited items.
		// NOTE: Should not be called on P1 since there's no need to re-import P1's favorites onto themselves.
		void ImportCoopFavorites();
		
		// Re-equip all forms for this player, optionally refreshing the cached equipped state beforehand.
		void ReEquipAll(bool a_refreshBeforeEquipping);
		
		// Unequip and re-equip forms in the two hand slots.
		void ReEquipHandForms();

		// Unequip and re-equip voice form (power/shout).
		void ReEquipVoiceForm();
		
		// Unfavorite this player's favorited items and restore P1's favorited items.
		// NOTE: Also should not be called on P1.
		void RestoreP1Favorites();
		
		// Update cached equip data in the given slots, auto-equip ammo, update shout spell variation, copy spells to placeholder spells,
		// update armor ratings, signal menu input manager to update displayed equip state,
		// and check for mismatches between the current equipped forms and the desired equipped forms lists.
		// If there are no mismatches, save equipped forms list to this player's serializable data.
		void RefreshEquipState(const RefreshSlots& a_slots, RE::TESForm* a_formEquipped = nullptr, bool a_isEquipped = true);
		
		// Set cached copied magic form and form ID with the given copied magic form at the given placeholder spell index.
		void SetCopiedMagicAndFID(RE::TESForm* a_magicFormToCopy, const PlaceholderMagicIndex& a_index);
		
		// Set hand spells if equipped and voice spells corresponding to the current power or current shout's highest known variation.
		void SetCurrentVoiceSpell();
		
		// Populate cached lists of cyclable favorited items of the given type.
		void SetCyclableFavForms(CyclableForms a_favFormType);
		
		// Assign new list of favorited emote idles.
		void SetFavoritedEmoteIdles(std::vector<RE::BSFixedString> a_emoteIdlesList);
		
		// Update favorited forms or favorited forms FIDs list(s) based on 
		// items in the player's inventory that have the Hotkey extra data member.
		void SetFavoritedForms(bool a_onlySetFIDs);
		
		// Populate desired equipped forms based on serialized equipped forms list,
		// update copied magic/copy to placeholder spells as needed, set quick slot item/spell,
		// and either refresh equip state, if this player is P1,
		// or unequip all if this player is a co-op companion (desired forms will be re-equipped later).
		void SetInitialEquipState();
		
		// For grip switching, set the original weapon type for the weapon in hand slot (0 = LH, 1 = RH, 2 = 2H).
		void SetOriginalWeaponTypeFromKeyword(HandIndex&& a_handSlot);
		
		// NOT USED FOR NOW:
		// Switch weapon grip type.
		void SwitchWeaponGrip(RE::TESObjectWEAP* a_weapon, bool a_equipRH);
		
		// Re-assign serialized forms to desired list, clear out mismatches,
		// and then unequip all forms.
		void UnequipAll();
		
		// Unequip ammo and update desired forms for co-op companion players.
		void UnequipAmmo(RE::TESForm* a_toUnequip, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true, const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr);
		
		// Unequip armor and update desired forms for co-op companion players.
		void UnequipArmor(RE::TESForm* a_toUnequip, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, uint32_t a_count = 1, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true, const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr);
		
		// Unequip form and update desired forms for co-op companion players.
		void UnequipForm(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex, RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, uint32_t a_count = 1, const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, bool a_queueEquip = true, bool a_forceEquip = true, bool a_playSounds = false, bool a_applyNow = true, const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr);
		
		// Unequip the form at the given index and update desired forms for co-op companion players.
		// Will also unequip bound weapons/ammo together.
		void UnequipFormAtIndex(const EquipIndex& a_equipIndex);
		
		// Clear desired hand form(s) in the given slot and then unequip the form(s).
		void UnequipHandForms(RE::BGSEquipSlot* a_slot);
		
		// Unequip shield and update desired forms for co-op companion players.
		void UnequipShield();
		
		// Unequip shout and update desired forms for co-op companion players.
		void UnequipShout(RE::TESForm* a_toUnequip);
		
		// Unequip spell and update desired forms for co-op companion players.
		void UnequipSpell(RE::TESForm* a_toUnequip, const EquipIndex& a_equipIndex);
		
		// Attempts to rectify mismatches and equip state issues with the player's equipped forms,
		// and then re-equip the desired forms.
		void ValidateEquipState();
		
		//
		// Members
		//

		// The co-op player.
		std::shared_ptr<CoopPlayer> p;
		// The co-op actor.
		RE::ActorPtr coopActor;
		// Spell that corresponds to the highest shout variation,
		// if equipped, or power, if equipped, that player 1 has learned.
		RE::SpellItem* voiceSpell;
		// Quick slot spell.
		RE::SpellItem* quickSlotSpell;
		// Current cycled ammo and voice magic forms.
		RE::TESForm* currentCycledAmmo;
		RE::TESForm* currentCycledVoiceMagic;
		// Last cycled emote idle event name and index 
		// recorded while pressing the emote idle cycling bind.
		std::pair<RE::BSFixedString, int8_t> lastCycledIdleIndexPair;
		// Last cycled form (spell, weapon, voice magic, or ammo) 
		// recorded while pressing the current cycling bind.
		RE::TESForm* lastCycledForm;
		// Quick slot consumable item.
		RE::TESForm* quickSlotItem;
		// Form in the power/voice slot.
		// Saved each time player 1 equips a power/shout,
		// and once at the start of the co-op session.
		RE::TESForm* voiceForm;
		// Weapon types (original/grip changed) for LH/RH forms.
		RE::WEAPON_TYPE lhNewGripType;
		RE::WEAPON_TYPE lhOriginalType;
		RE::WEAPON_TYPE rhNewGripType;
		RE::WEAPON_TYPE rhOriginalType;
		// Favorites cycling categories for LH/RH spells and weapons.
		FavMagicCyclingCategory lastCycledSpellCategory;
		FavMagicCyclingCategory lhSpellCategory;
		FavMagicCyclingCategory rhSpellCategory;
		FavWeaponCyclingCategory lastCycledWeaponCategory;
		FavWeaponCyclingCategory lhWeaponCategory;
		FavWeaponCyclingCategory rhWeaponCategory;
		// List of all forms that the player wants to equip.
		// The list of currently equipped forms is adjusted to match this list.
		// Slots: hands, quick slots, ammo slot, voice slot, and biped slots.
		std::array<RE::TESForm*, (size_t)EquipIndex::kTotal> desiredEquippedForms;
		// List of currently equipped forms in: hands, quick slots, ammo slot, voice slot, and biped slots.
		std::array<RE::TESForm*, (size_t)EquipIndex::kTotal> equippedForms;
		// Form IDs of spells + shout copied over into the player's placeholder magic forms.
		std::array<RE::FormID, (size_t)PlaceholderMagicIndex::kTotal> copiedMagicFormIDs;
		// Spells + shout copied over to placeholder spells.
		std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> copiedMagic;
		// Placeholder LH, RH, 2H, and Voice spells/shout to copy chosen equipped magic into.
		std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> placeholderMagic;
		// Lists of currently cycled favorited items per cycling category.
		std::array<RE::TESForm*, (size_t)FavMagicCyclingCategory::kTotal> currentCycledLHSpellsList;
		std::array<RE::TESForm*, (size_t)FavMagicCyclingCategory::kTotal> currentCycledRHSpellsList;
		std::array<RE::TESForm*, (size_t)FavWeaponCyclingCategory::kTotal> currentCycledLHWeaponsList;
		std::array<RE::TESForm*, (size_t)FavWeaponCyclingCategory::kTotal> currentCycledRHWeaponsList;
		// Current list of favorited emote idles.
		std::array<RE::BSFixedString, 8> favoritedEmoteIdles;
		// Mutex for refreshing the player's equip state.
		std::mutex equipStateMutex;
		// (Light, Heavy) armor ratings pair.
		std::pair<float, float> armorRatings;
		// Pair of current cycled emote, index which can be triggered by pressing the special action binds
		// while the player's weapons are sheathed.
		// Up to 128 emotes.
		std::pair<RE::BSFixedString, int8_t> currentCycledIdleIndexPair;
		// Cached lists of hand-equipable spells and voice slot spells/shouts.
		std::pair<std::vector<RE::SpellItem*>, std::vector<RE::TESForm*>> equipableMagic;
		// Set of equipped items' form IDs for the co-op player.
		std::set<RE::FormID> equippedFormFIDs;
		// Indices of currently equipped favorited items.
		std::set<uint32_t> favItemsEquippedIndices;
		// Set of favorited emote idles' hashes.
		std::set<uint32_t> favoritedEmoteIdlesHashes;
		// Set of favorited items' form IDs for the co-op player.
		std::set<RE::FormID> favoritedFormIDs;
		// Favorited items separated into lists based on form type.
		std::unordered_map<CyclableForms, std::vector<RE::TESForm*>> cyclableFormsMap;
		// Cached equipable weapons in inventory
		std::vector<RE::TESForm*> equipableWeapons;
		// List of bound object forms favorited by the co-op player.
		std::vector<RE::TESForm*> favoritedPhysForms;
		// List of flags indicating whether this co-op player's favorited items at each index
		// are also favorited by player 1.
		std::vector<bool> favoritesIndicesInCommon;
		// List of flags indicating whether this co-op player's favorited items at each index
		// were added to P1 on import.
		std::vector<bool> favoritedItemWasAdded;
		// Controller ID for this player.
		int32_t controllerID;
		// Player ID for this player.
		int32_t playerID;
		// Favorites list indices for equipped quick slot forms (quick slot item, quick slot spell).
		int32_t equippedQSItemIndex;
		int32_t equippedQSSpellIndex;
		int32_t highestShoutVarIndex;
		uint32_t numFavoritedItems;
	};
}

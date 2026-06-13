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
		// Delayed initialization after the player is default-constructed 
		// and the player shared pointer is added to the list of co-op players 
		// in the global data holder.
		void Initialize(std::shared_ptr<CoopPlayer> a_p);

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		inline void ClearDesiredEquippedFormAtIndex
		(
			RE::TESForm* a_form, const std::underlying_type_t<EquipIndex>& a_listIndex
		) 
		{
			// Must have a valid item to unequip and an index within the bounds
			// of the desired forms/exData lists arrays.
			if (!a_form || 
				a_listIndex == !EquipIndex::kNone ||
				a_listIndex >= !EquipIndex::kTotal)
			{
				return;
			}

			DBG("Remove {}, index {}.", a_form->GetName(), a_listIndex);
			// If the requested form to unequip is not the same as 
			// the one already in this slot, do not clear the slot.
			bool diffFormAlreadyInSlot = 
			(
				desiredForms[a_listIndex] && 
				desiredForms[a_listIndex] != a_form
			);
			if (!diffFormAlreadyInSlot)
			{
				desiredForms[a_listIndex] = nullptr;
				desiredExtraDataLists[a_listIndex] = nullptr;
			}
		}

		// Get all currently equipped forms for this player.
		inline const std::array<RE::TESForm*, (size_t)EquipIndex::kTotal>& GetAllEquippedForms() 
		const
		{
			return equippedForms;
		}

		// Get form ID for the spell copied into the placeholder spell at the given index.
		inline RE::FormID GetCopiedMagicFormID(const PlaceholderMagicIndex& a_index) const
		{
			return 
			(
				!a_index >= 0 && 
				!a_index < !PlaceholderMagicIndex::kTotal ? copiedMagicFormIDs[!a_index] : 0
			);
		}

		// Get the spell form copied into the placeholder spell at the given index.
		inline RE::TESForm* GetCopiedMagic(const PlaceholderMagicIndex& a_index) const
		{
			return 
			(
				!a_index >= 0 && 
				!a_index < !PlaceholderMagicIndex::kTotal ? copiedMagic[!a_index] : nullptr
			);
		}

		// Get the spell corresponding to the highest shout variation that the player knows 
		// for the currently equipped shout.
		inline RE::SpellItem* GetHighestShoutVariation() const
		{
			return voiceSpell;
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

		// Get the max reach for any equipped weapons.
		// Not sure what the base reach is in in-game units, 
		// so the weapon's reach is multiplied by the actor's height.
		inline float GetMaxWeapReach() const
		{
			const auto lhForm = equippedForms[!EquipIndex::kLeftHand];
			const auto lhWeapReach = 
			(
				lhForm && lhForm->As<RE::TESObjectWEAP>() ?
				lhForm->As<RE::TESObjectWEAP>()->GetReach() * coopActor->GetHeight() :
				coopActor->race->data.unarmedReach
			);
			const auto rhForm = equippedForms[!EquipIndex::kRightHand];
			const auto rhWeapReach = 
			(
				rhForm && rhForm->As<RE::TESObjectWEAP>() ?
				rhForm->As<RE::TESObjectWEAP>()->GetReach() * coopActor->GetHeight() :
				coopActor->race->data.unarmedReach
			);
			return (lhWeapReach > rhWeapReach) ? lhWeapReach : rhWeapReach;
		}
		
		// Get the placeholder spell form at the given index.
		inline RE::TESForm* GetPlaceholderMagic(const PlaceholderMagicIndex& a_index) const
		{
			return 
			(
				!a_index >= 0 && 
				!a_index < !PlaceholderMagicIndex::kTotal ? placeholderMagic[!a_index] : nullptr
			);
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
			return 
			(
				a_rightHand ? 
				!equippedForms[!EquipIndex::kRightHand] : 
				!equippedForms[!EquipIndex::kLeftHand]
			);
		}

		// Check if the player has a two hand melee weapon equipped.
		inline bool Has2HMeleeWeapEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return 
			(
				(rhObj && rhObj->IsWeapon()) && 
				(
					rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedAxe() ||
					rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedSword()
				)
			);
		}

		// Check if the player has a two hand ranged weapon equipped.
		inline bool Has2HRangedWeapEquipped() const
		{
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return 
			{
				(rhObj && rhObj->As<RE::TESObjectWEAP>()) &&
				(
					rhObj->As<RE::TESObjectWEAP>()->IsBow() ||
					rhObj->As<RE::TESObjectWEAP>()->IsCrossbow()
				)
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
				rhObj->As<RE::BGSEquipType>()->equipSlot->flags.all
				(
					RE::BGSEquipSlot::Flag::kUseAllParents
				) 
			};
		}

		// Check if the player has a bow equipped.
		inline bool HasBowEquipped() const
		{
			return 
			(
				Has2HRangedWeapEquipped() && 
				equippedForms[!EquipIndex::kRightHand]->As<RE::TESObjectWEAP>()->IsBow()
			);
		}

		// Check if the player has a crossbow equipped.
		inline bool HasCrossbowEquipped() const
		{
			return 
			(
				Has2HRangedWeapEquipped() && 
				equippedForms[!EquipIndex::kRightHand]->As<RE::TESObjectWEAP>()->IsCrossbow()
			);
		}

		// Check if the player has a one hand melee weapon equipped in their left hand.
		inline bool HasLHMeleeWeapEquipped() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; 
				lhObj && lhObj->IsWeapon())
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
			return 
			(
				lhObj && 
				lhObj->Is(RE::FormType::Weapon) && 
				lhObj->As<RE::TESObjectWEAP>()->IsStaff()
			);
		}

		// Check if the player has a one hand weapon equipped in their left hand.
		inline bool HasLHWeapEquipped() const
		{
			if (const auto lhObj = equippedForms[!EquipIndex::kLeftHand]; 
				lhObj && lhObj->IsWeapon())
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
			return 
			(
				Has2HRangedWeapEquipped() || 
				HasLHSpellEquipped() || 
				HasRHSpellEquipped() || 
				HasLHStaffEquipped() || 
				HasRHStaffEquipped()
			);
		}

		// Check if the player has a one hand melee weapon equipped in their right hand.
		inline bool HasRHMeleeWeapEquipped() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; 
				rhObj && rhObj->IsWeapon())
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
			return 
			(
				rhObj && 
				rhObj->Is(RE::FormType::Weapon) && 
				rhObj->As<RE::TESObjectWEAP>()->IsStaff()
			);
		}

		// Check if the player has a one hand weapon equipped in their right hand.
		inline bool HasRHWeapEquipped() const
		{
			if (const auto rhObj = equippedForms[!EquipIndex::kRightHand]; 
				rhObj && rhObj->IsWeapon())
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

			if ((lhObj && lhObj->IsArmor() && lhObj->As<RE::TESObjectARMO>()->IsShield()) ||
				(rhObj && rhObj->IsArmor() && rhObj->As<RE::TESObjectARMO>()->IsShield()))
			{
				return true;
			}

			// Check the current shield biped slot next.
			auto biped = coopActor->GetBiped();
			if (biped)
			{
				auto form = biped->objects[RE::BIPED_OBJECT::kShield].item;
				if (form &&
					form->As<RE::TESObjectARMO>() &&
					form->As<RE::TESObjectARMO>()->IsShield())
				{
					return true;
				}
			}

			return false;
		}

		// Check if the player has a torch equipped.
		inline bool HasTorchEquipped() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			return 
			(
				lhObj && 
				lhObj->Is(RE::FormType::Light) && 
				lhObj->As<RE::TESObjectLIGH>()->CanBeCarried()
			);
		}

		// Check if the player is dual wielding weapons.
		inline bool IsDualWielding() const
		{
			const auto lhObj = equippedForms[!EquipIndex::kLeftHand];
			const auto rhObj = equippedForms[!EquipIndex::kRightHand];
			return 
			(
				(
					lhObj && rhObj && lhObj->IsWeapon() && rhObj->IsWeapon()
				) &&
				(
					!rhObj->As<RE::TESObjectWEAP>()->IsRanged() &&
					!rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedAxe() &&
					!rhObj->As<RE::TESObjectWEAP>()->IsTwoHandedSword()
				)
			);
		}

		// Return true if the item given by the bound object and extra data list 
		// is in the inventory chest.
		// Can also match the given list with an equivalent one that has the same worn data.
		inline bool IsInInventoryChest
		(
			RE::TESBoundObject* a_object, RE::ExtraDataList* a_exDatalist, bool a_inLeftHand
		)
		{
			auto invChanges = inventoryChest->GetInventoryChanges();
			if (!invChanges || !invChanges->entryList)
			{
				return false;
			}

			for (const auto entry : *invChanges->entryList)
			{
				if (!entry)
				{
					continue;
				}

				if (!a_exDatalist && entry->object == a_object)
				{
					DBG
					(
						"No specified extra data list for {}, which exists in the chest.", 
						a_object->GetName()
					);
					return true;
				}

				if (entry->object != a_object || !entry->extraLists)
				{
					continue;
				}

				for (const auto list : *entry->extraLists)
				{
					if (list == a_exDatalist)
					{
						DBG
						(
							"{}. {:p} matches {:p}.", 
							a_object->GetName(),
							fmt::ptr(list),
							fmt::ptr(a_exDatalist)
						);
						return true;
					}
				}
			}

			return false;
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

		// Clear the player's desired forms list and then unequip all forms.
		inline void UnequipAllAndResetEquipState() 
		{
			desiredForms.fill(nullptr);
			desiredForms.fill(0);
			Util::Papyrus::UnequipAll(coopActor.get());
		}
		
		//
		// Member funcs
		//

		RE::ExtraDataList* AddItemFromInventoryChest
		(
			RE::TESBoundObject* a_object,
			RE::ExtraDataList* a_extraDataList,
			uint32_t a_count,
			bool a_equipsToLH,
			bool a_keepInChest = true
		);

		// Equip matching highest count/damage ammo if the setting is enabled 
		// and the given bound object is a ranged weapon.
		void AutoEquipAmmo(RE::TESBoundObject* a_equippedObject);

		// Add to specified list or remove all worn exRank data from chest extra data lists
		// for the given item.
		// 
		// Will only add/remove if the item is equipped/unequipped in the given hand.
		// If no chest exData list is provided when trying to add exRank data on equip,
		// attempt to find a matching chest exData list for the equipped exData list 
		// in the same hand before adding worn exRank data.
		void ChangeWornRankExData
		(
			RE::TESBoundObject* a_object,
			bool a_equipsToLH,
			bool a_add,
			RE::ExtraDataList* a_chestListToChange = nullptr
		);
		
		// Remove form from desired forms list at all indices that contain it, 
		// plus any given specific index to remove the item from.
		void ClearDesiredEquippedForm
		(
			RE::TESForm* a_form, const RE::BGSEquipSlot* a_slot, const EquipIndex& a_equipIndex
		);

		// NOTE:
		// Currently unused since I can't seem to trigger the 'Shout' package procedure 
		// in the same way as I'm triggering the 'UseMagic' procedure for spellcasting.
		//RE::TESShout* CopyToPlaceholderShout(RE::TESShout* a_shoutToCopy);

		// Copy the given spell to the placeholder spell at the given index.
		// Return the placeholder spell with the requested spell copied into it.
		RE::SpellItem* CopyToPlaceholderSpell
		(
			RE::SpellItem* a_spellToCopy, const PlaceholderMagicIndex& a_index
		);

		// Cycle to the next favorited ammo choice 
		// that matches the currently equipped ranged weapon.
		void CycleAmmo();
		
		// Cycle to the next favorited emote idle.
		void CycleEmoteIdles();
		
		// Cycle to the next favorited spell that is equipable in the given hand 
		// and is also in the current spell cycling category.
		void CycleHandSlotMagic(bool&& a_rightHand);
		
		// Cycle to the next available hand slot magic category for the given hand.
		void CycleHandSlotMagicCategory(bool&& a_rightHand);
		
		// Cycle to the next favorited voice slot magic spell/shout.
		void CycleVoiceSlotMagic();
		
		// Cycle to the next available weapon category for the given hand.
		void CycleWeaponCategory(bool&& a_rightHand);
		
		// Cycle to the next favorited weapon that is equipable in the given hand 
		// and is also in the current weapon cycling category.
		void CycleWeapons(bool&& a_rightHand);
		
		// Equip ammo and update desired forms for co-op companion players.
		void EquipAmmo
		(
			RE::TESForm* a_toEquip,
			RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, 
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, 
			bool a_queueEquip = true, 
			bool a_forceEquip = true, 
			bool a_playSounds = false,
			bool a_applyNow = true
		);
		
		// Equip armor and update desired forms for co-op companion players.
		void EquipArmor
		(
			RE::TESForm* a_toEquip, 
			RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, 
			uint32_t a_count = 1, 
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, 
			bool a_queueEquip = true,
			bool a_forceEquip = true, 
			bool a_playSounds = false, 
			bool a_applyNow = true
		);
		
		// Equip dummy 1H weapon to clear out the given hand slot.
		// NOTE: 
		// Can choose to also clear out desired forms/exData list slots.
		void EquipDummy1H(const RE::BGSEquipSlot* a_slot, bool a_clearDesiredSlots);

		// Equip fists to clear out hand slots.
		// Can choose to also clear out desired forms/exData list slots.
		void EquipFists(bool a_clearDesiredSlots);
		
		// Equip form and update desired forms for co-op companion players.
		void EquipForm
		(
			RE::TESForm* a_toEquip,
			const EquipIndex& a_equipIndex, 
			RE::ExtraDataList* a_exData = (RE::ExtraDataList*)nullptr, 
			uint32_t a_count = 1,
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr,
			bool a_queueEquip = true,
			bool a_forceEquip = true, 
			bool a_playSounds = false, 
			bool a_applyNow = true
		);
		
		// Equip shout and update desired forms for co-op companion players.
		void EquipShout(RE::TESForm* a_toEquip);
		
		// Equip spell and update desired forms for co-op companion players.
		void EquipSpell
		(
			RE::TESForm* a_toEquip,
			const EquipIndex& a_equipIndex, 
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr
		);
		
		// Get name for the given favorited magic cycling category.
		std::string_view FavMagCyclingCategoryToString
		(
			const FavMagicCyclingCategory& a_category
		) const;
		
		// Get name for the given weapon cycling category.
		std::string_view FavWeaponCyclingCategoryToString
		(
			const FavWeaponCyclingCategory& a_category
		) const;
		
		// Add extra ownership data to all equipable items in the player's inventory
		// and then fix counts for all the inventory's items.
		// Inventory here means P1's on-player inventory 
		// and the player inventory chest for companion players.
		void FixInventory();

		// NOTE: 
		// Unused for now, but keeping for reference or if needed again in the future.
		// Get equipable spells in the hand slots or powers/shouts in voice slot.
		// Checks all of P1's known spells/shouts 
		// and this player's known spells/shouts to compile the list.
		std::vector<RE::TESForm*> GetEquipableSpells(bool a_inHandSlot) const;
		
		// Get the equip slot for the given form at the given equip index.
		RE::BGSEquipSlot* GetEquipSlotForForm
		(
			RE::TESForm* a_form, const EquipIndex& a_index
		) const;
		
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
		RE::ExtraDataList* GetNextFavoritedExDataList
		(
			RE::TESForm* a_form, 
			bool a_checkWornLeft, 
			bool& a_nothingToEquip
		);
		
		// Setup equip request by adding the item from the companion player's chest
		// and providing the proper extra data list.
		// Can specify a specific equip index to set in the desired forms/exData arrays.
		// 'kNone' to have the function compute it based on the item type or to not fill any slot.
		void HandleCompanionPlayerEquip
		(
			RE::TESBoundObject* a_object, 
			const EquipIndex& a_equipIndex = EquipIndex::kNone,
			RE::ExtraDataList* a_extraData = nullptr, 
			uint32_t a_count = 1, 
			const RE::BGSEquipSlot* a_slot = nullptr,
			bool a_queueEquip = true, 
			bool a_forceEquip = false, 
			bool a_playSounds = true, 
			bool a_applyNow = false
		);

		// IMPORTANT:
		// Given extra data list should always be from the player's inventory.
		// Remove items, extra data lists, inventory entries, and clean up after unequipping.
		// Can specify a specific equip index to clear out in the desired forms/exData arrays.
		// 'kNone' to have the function compute it based on the item type or to not clear any slot.
		void HandleCompanionPlayerUnequip
		(
			RE::TESBoundObject* a_object, 
			const EquipIndex& a_equipIndex = EquipIndex::kNone,
			RE::ExtraDataList* a_exDataList = nullptr, 
			uint32_t a_count = 1, 
			const RE::BGSEquipSlot* a_slot = nullptr,
			bool a_queueEquip = true, 
			bool a_forceEquip = false, 
			bool a_playSounds = true, 
			bool a_applyNow = false,
			const RE::BGSEquipSlot* a_slotToReplace = nullptr
		);

		// Un/equip the desired form at the given index.
		// NOTE: 
		// Should never be called on P1.
		void HandleEquipRequest
		(
			RE::TESForm* a_form, 
			RE::ExtraDataList* a_exData,
			const EquipIndex& a_index, 
			bool a_shouldEquip
		);
		
		// Un/equip the desired form from the given container at the given index.
		// If a placeholder spell has changed, re-copy over the requested spell before equipping.
		void HandleMenuEquipRequest
		(
			RE::ObjectRefHandle a_fromContainerHandle,
			RE::TESForm* a_form,
			RE::ExtraDataList* a_exData,
			const EquipIndex& a_index, 
			bool a_placeholderMagicChanged
		);

		// Checks if the player has a favorited spell in the given category.
		bool HasCyclableSpellInCategory(const FavMagicCyclingCategory& a_category);
		
		// Checks if the player has a favorited weapon 
		// in the given category that matches the given hand.
		bool HasCyclableWeaponInCategory
		(
			const FavWeaponCyclingCategory& a_category, const bool& a_rightHand
		);
		
		// Add favorited items/spells from this player to P1 as needed, favorite them, 
		// and unfavorite all P1's favorited items/spells.
		// NOTE: 
		// Should not be called on P1 
		// since there's no need to re-import P1's favorites onto themselves.
		void ImportCoopFavorites(bool&& a_onlyMagicFavorites);
		
		// Check if the given form is equipped 
		// (in the player's equipped forms list, if not a bound object,
		// or has ExtraWorn(Left) data.
		bool IsEquipped
		(
			RE::TESForm* a_form, 
			RE::ExtraDataList* a_exDataList, 
			bool a_leftHand = false,
			bool a_eitherHand = false
		);

		// Check if the player is unarmed.
		bool IsUnarmed() const;

		// Re-equip all forms for this player, optionally refreshing the cached equipped state 
		// or resetting the companion player's inventory beforehand.
		void ReEquipAll(bool a_refreshBeforeEquipping, bool a_resetInventoryFirst = true);
		
		// Unequip and re-equip 1H form in the given slot (LH or RH).
		void ReEquipHandForm(bool a_rhSlot);

		// Unequip and re-equip forms in the two hand slots.
		void ReEquipHandForms();

		// Unequip and re-equip voice form (power/shout).
		void ReEquipVoiceForm();

		// Unfavorite this player's favorited items/spells and restore P1's favorited items/spells.
		// NOTE: 
		// Also should not be called on P1.
		void RestoreP1Favorites(bool&& a_onlyMagicFavorites);
		
		// Update cached equip data in the given slots, auto-equip ammo, 
		// update shout spell variation, copy spells to placeholder spells,
		// update armor ratings, signal menu input manager to update displayed equip state,
		// and check for mismatches between the current equipped forms 
		// and the desired equipped forms lists.
		// If there are no mismatches, save equipped forms list to this player's serializable data.
		void RefreshEquipState
		(
			const RefreshSlots& a_slots, 
			RE::TESForm* a_formEquipped = nullptr,
			bool a_isEquipped = true
		);
		
		// Remove all items that were not equipped by the player
		// from the player character's inventory.
		// Done when an item is added to the player's inventory.
		// Ignore the given most recently added item's extra data list, 
		// since this list was added before equipping the item.
		void RemoveUndesiredItems();

		// Set cached copied magic form and form ID 
		// with the given copied magic form at the given placeholder spell index.
		void SetCopiedMagicAndFID
		(
			RE::TESForm* a_magicFormToCopy, const PlaceholderMagicIndex& a_index
		);
		
		// Set equipped voice spell corresponding to the current power 
		// or current shout's highest known variation.
		void SetCurrentVoiceSpell();
		
		// Populate cached lists of cyclable favorited items of the given type.
		void SetCyclableFavForms(CyclableForms a_favFormType);

		// Assign new list of favorited emote idles.
		void SetFavoritedEmoteIdles(std::vector<RE::BSFixedString> a_emoteIdlesList);
		
		// Populate desired equipped forms based on serialized equipped forms list,
		// update copied magic/copy to placeholder spells as needed, set quick slot item/spell,
		// and either refresh equip state, if this player is P1,
		// or unequip all if this player is a co-op companion 
		// (desired forms will be re-equipped later).
		void SetInitialEquipState();
		
		// NOT USED FOR NOW:
		// A bridge too far for the ever-present feature creep in this mod.
		// For now. Heh.
		// Switch weapon grip type for the given weapon 
		// and equip it to the right or left hand slot.
		void SwitchWeaponGrip(RE::TESObjectWEAP* a_weapon, bool a_equipRH);
		
		// Re-assign serialized forms to desired list, clear out mismatches,
		// and then unequip all forms.
		void UnequipAll();
		
		// Unequip ammo and update desired forms for co-op companion players.
		void UnequipAmmo
		(
			RE::TESForm* a_toUnequip, 
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr,
			bool a_queueEquip = true,
			bool a_forceEquip = true,
			bool a_playSounds = false, 
			bool a_applyNow = true, 
			const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr
		);
		
		// Unequip armor and update desired forms for co-op companion players.
		void UnequipArmor
		(
			RE::TESForm* a_toUnequip,
			uint32_t a_count = 1,
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, 
			bool a_queueEquip = true, 
			bool a_forceEquip = true,
			bool a_playSounds = false,
			bool a_applyNow = true,
			const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr
		);
		
		// Unequip form and update desired forms for co-op companion players.
		void UnequipForm
		(
			RE::TESForm* a_toUnequip,
			const EquipIndex& a_equipIndex, 
			uint32_t a_count = 1,
			const RE::BGSEquipSlot* a_slot = (const RE::BGSEquipSlot*)nullptr, 
			bool a_queueEquip = true, 
			bool a_forceEquip = true, 
			bool a_playSounds = false, 
			bool a_applyNow = true,
			const RE::BGSEquipSlot* a_slotToReplace = (const RE::BGSEquipSlot*)nullptr
		);
		
		// Unequip the form at the given index 
		// and update desired forms for co-op companion players.
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
		
		// Update favorited forms list and the magic favorites list to serialize.
		// Also update cyclable and hotkeyed forms.
		// If requested, use the serialized magic favorites list to copy over 
		// to the current magic favorites list.
		void UpdateFavoritedFormsLists(bool&& a_useCachedMagicFavorites);

		//
		// Members
		//

		// The co-op player.
		std::shared_ptr<CoopPlayer> p;
		// The co-op actor.
		RE::ActorPtr coopActor;
		// Spell that corresponds to the highest shout variation or power, 
		// if equipped, that P1 has learned.
		RE::SpellItem* voiceSpell;
		// Quick slot spell.
		RE::SpellItem* quickSlotSpell;
		// LH/RH bound weapons the player last requested to equip.
		// If 2H, LH and RH are set to the same weapon.
		RE::TESForm* lastReqBoundWeapLH;
		RE::TESForm* lastReqBoundWeapRH;
		// Current cycled ammo and voice magic forms.
		RE::TESForm* currentCycledAmmo;
		RE::TESForm* currentCycledVoiceMagic;
		// Last selected hotkeyed form.
		RE::TESForm* lastChosenHotkeyedForm;
		// Inventory for the player (a chest in a galaxy far, far away).
		RE::TESObjectREFRPtr inventoryChest;
		// Last cycled emote idle event name and index 
		// recorded while pressing the emote idle cycling bind.
		std::pair<RE::BSFixedString, int8_t> lastCycledIdleIndexPair;
		// Last cycled form (spell, weapon, voice magic, or ammo) 
		// recorded while pressing the current cycling bind.
		RE::TESForm* lastCycledForm;
		// Quick slot consumable item.
		RE::TESForm* quickSlotItem;
		// Form in the power/voice slot.
		// Saved each time P1 equips a power/shout,
		// and once at the start of the co-op session.
		RE::TESForm* voiceForm;
		// Favorites cycling categories for LH/RH spells and weapons.
		FavMagicCyclingCategory lastCycledSpellCategory;
		FavMagicCyclingCategory lhSpellCategory;
		FavMagicCyclingCategory rhSpellCategory;
		FavWeaponCyclingCategory lastCycledWeaponCategory;
		FavWeaponCyclingCategory lhWeaponCategory;
		FavWeaponCyclingCategory rhWeaponCategory;
		// List of all forms that the player wants to equip.
		// Form IDs of spells + shout copied over into the player's placeholder magic forms.
		std::array<RE::FormID, (size_t)PlaceholderMagicIndex::kTotal> copiedMagicFormIDs;
		// Spells + shout copied over to placeholder spells.
		std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> copiedMagic;
		// Lists of currently cycled favorited items per cycling category.
		std::array<RE::TESForm*, (size_t)FavMagicCyclingCategory::kTotal> 
			currentCycledLHSpellsList;
		std::array<RE::TESForm*, (size_t)FavMagicCyclingCategory::kTotal> 
			currentCycledRHSpellsList;
		std::array<RE::TESForm*, (size_t)FavWeaponCyclingCategory::kTotal>
			currentCycledLHWeaponsList;
		std::array<RE::TESForm*, (size_t)FavWeaponCyclingCategory::kTotal> 
			currentCycledRHWeaponsList;
		// The list of currently equipped forms is adjusted to match this list.
		// Slots: hands, quick slots, ammo slot, voice slot, and biped slots.
		std::array<RE::TESForm*, (size_t)EquipIndex::kTotal> desiredForms;
		// Unique IDs for all equipped items.
		// Used to distinguish between items of the same type when (un)equipping them.
		std::array<RE::ExtraDataList*, (size_t)EquipIndex::kTotal> desiredExtraDataLists;
		// List of currently equipped forms in: 
		// hands, quick slots, ammo slot, voice slot, and biped slots.
		std::array<RE::TESForm*, (size_t)EquipIndex::kTotal> equippedForms;
		// Current list of favorited emote idles.
		std::array<RE::BSFixedString, 8> favoritedEmoteIdles;
		// Up to 8 hotkeyed favorited forms.
		// Nullptr if the slot has no hotkeyed form.
		std::array<RE::TESForm*, 8> hotkeyedForms;
		// Placeholder LH, RH, 2H, and Voice spells/shout to copy chosen equipped magic into.
		std::array<RE::TESForm*, (size_t)PlaceholderMagicIndex::kTotal> placeholderMagic;
		// Mutex for refreshing the player's equip state.
		std::mutex equipStateMutex;
		// (Light, Heavy) armor ratings pair.
		std::pair<float, float> armorRatings;
		// Pair of (current cycled emote, index) 
		// which can be triggered by pressing the special action binds
		// while the player's weapons are sheathed.
		// Up to 128 emotes.
		std::pair<RE::BSFixedString, int8_t> currentCycledIdleIndexPair;
		// Set of equipped items' form IDs for the co-op player.
		std::set<RE::FormID> equippedFormFIDs;
		// Set of favorited items' form IDs for the co-op player.
		std::set<RE::FormID> favoritedFormIDs;
		// Favorited items separated into lists based on form type.
		std::unordered_map<CyclableForms, std::vector<RE::TESForm*>> cyclableFormsMap;
		// Maps hotkeyed forms' FIDs to a set of their hotkey indices.
		// The same form ID, but different exData list, can be favorited to multiple hotkeys.
		std::unordered_map<RE::FormID, std::set<int8_t>> hotkeyedFormsToSlotsSetMap;
		// List of bound object and spell forms favorited by the co-op player.
		std::vector<RE::TESForm*> favoritedForms;
		// (Un)equipObject hooks should not perform their normal functions when this is set;
		// also prevents changes to worn rank data for companion players when set to true.
		// Bracket function calls/blocks of code with this flag set to true and then false afterward
		// to prevent companion players from equipping the item from the chest or removing the item
		// and invalidating extra data lists once unequipped, 
		// which can prevent item transfer from succeeding.
		// Can also bookend blocks of code to prevent changes to worn rank data, 
		// streamlining re-equipping of items without first caching their worn rank data to restore.
		bool skipEquipProcessing;
		// Input device ID for this player.
		// Controller IDs fall in the range [0, 3] and keyboard + mouse IDs are >= 4.
		int32_t deviceID;
		// Player ID for this player.
		int32_t playerID;
		// Favorites list indices for equipped quick slot forms 
		// (quick slot item, quick slot spell).
		int32_t equippedQSItemIndex;
		int32_t equippedQSSpellIndex;
		// Number of unlocked words in the currently-equipped shout (-1 if unknown or no shout).
		int32_t highestShoutVarIndex;
	};
}

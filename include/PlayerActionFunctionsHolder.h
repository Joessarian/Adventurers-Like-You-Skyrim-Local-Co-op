#pragma once
#include <Player.h>
#include <PlayerActionInfoHolder.h>

namespace ALYSLC 
{
	using SteadyClock = std::chrono::steady_clock;
	enum PAFuncType
	{
		kCondFunc,
		kStartFunc,
		kProgressFunc,
		kCleanupFunc
	};

	// Holds condition, start, progress, and cleanup functions for each Player Action.
	// These functions are called at different times based on the player's configured binds.
	class PlayerActionFunctionsHolder
	{
		using CondFunc		=	
		std::optional<std::function<bool(const std::shared_ptr<CoopPlayer>&)>>;
		using StartFunc		=	
		std::optional<std::function<void(const std::shared_ptr<CoopPlayer>&)>>;
		using ProgressFunc	=	
		std::optional<std::function<void(const std::shared_ptr<CoopPlayer>&)>>;
		using CleanupFunc	=	
		std::optional<std::function<void(const std::shared_ptr<CoopPlayer>&)>>;

	public:
		PlayerActionFunctionsHolder();
		PlayerActionFunctionsHolder(const PlayerActionFunctionsHolder& _other) = delete;
		PlayerActionFunctionsHolder(PlayerActionFunctionsHolder&& _other) = delete;

		PlayerActionFunctionsHolder& operator=(const PlayerActionFunctionsHolder& _other) = delete;
		PlayerActionFunctionsHolder& operator=(PlayerActionFunctionsHolder&& _other) = delete;

		constexpr bool CallPAFunc
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const InputAction& a_playerAction, 
			PAFuncType&& a_funcType
		) const noexcept
		{
			// If not calling a condition function, 
			// returns true if the function exists and was called successfully.
			// If calling a condition function, 
			// returns true if the func does not exist (no conditions for the action).
			// or if the func exists and returns true (conditions held).

			if (a_playerAction == InputAction::kNone)
			{
				return false;
			}
			else
			{
				bool succ = true;
				switch (a_funcType)
				{
				case PAFuncType::kCondFunc:
				{
					const auto& func = _condFuncs[!a_playerAction - !InputAction::kFirstAction];
					succ = !func.has_value() || (*func)(a_p);
					break;
				}
				case PAFuncType::kStartFunc:
				{
					const auto& func = _startFuncs[!a_playerAction - !InputAction::kFirstAction];
					succ = func.has_value();
					if (succ)
					{
						(*func)(a_p);
					}

					break;
				}
				case PAFuncType::kProgressFunc:
				{
					const auto& func = _progFuncs[!a_playerAction - !InputAction::kFirstAction];
					succ = func.has_value();
					if (succ)
					{
						(*func)(a_p);
					}

					break;
				}
				case PAFuncType::kCleanupFunc:
				{
					const auto& func = _cleanupFuncs[!a_playerAction - !InputAction::kFirstAction];
					succ = func.has_value();
					if (succ)
					{
						(*func)(a_p);
					}

					break;
				}
				default:
					// Should not happen.
					succ = false;
					break;
				}

				return succ;
			}
		};

		// Populate the function lists for each type of Player Action function.
		void PopulateFuncLists();

	private:
		// See 'PerfType' enum in Enums.h for more details on
		// which functions are run for different action perform types.

		// List of condition functions for each action.
		// Condition functions are evaluated to check if actions
		// can be performed and can continue being performed once started.
		// They are the first function type run.
		std::vector<CondFunc> _condFuncs;
		// List of start functions for each action.
		// Certain perform types make use of the start function
		// to perform an action right after their composing inputs are pressed.
		std::vector<StartFunc> _startFuncs;
		// List of progress functions for each action.
		// Progress functions are run each frame 
		// while the action's composing inputs are pressed.
		std::vector<ProgressFunc> _progFuncs;
		// List of cleanup functions for each action.
		// Cleanup functions run when at least one of the action's inputs are released,
		// or if the action was interrupted (and the action does not prevent cleanup on interrupt).
		std::vector<CleanupFunc> _cleanupFuncs;
	};

	namespace ConditionFuncs
	{
		// Specific to player action.
		bool Activate(const std::shared_ptr<CoopPlayer>& a_p);
		bool ActivateAllOfType(const std::shared_ptr<CoopPlayer>& a_p);
		bool ActivateCancel(const std::shared_ptr<CoopPlayer>& a_p);
		bool AdjustAimPitch(const std::shared_ptr<CoopPlayer>& a_p);
		bool AttackLH(const std::shared_ptr<CoopPlayer>& a_p);
		bool AttackRH(const std::shared_ptr<CoopPlayer>& a_p);
		bool Bash(const std::shared_ptr<CoopPlayer>& a_p);
		bool Block(const std::shared_ptr<CoopPlayer>& a_p);
		bool CastLH(const std::shared_ptr<CoopPlayer>& a_p);
		bool CastRH(const std::shared_ptr<CoopPlayer>& a_p);
		bool ChangeDialoguePlayer(const std::shared_ptr<CoopPlayer>& a_p);
		bool DebugRagdollPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		bool DebugRefreshPlayerManagers(const std::shared_ptr<CoopPlayer>& a_p);
		bool DebugResetPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		bool Dismount(const std::shared_ptr<CoopPlayer>& a_p);
		bool Dodge(const std::shared_ptr<CoopPlayer>& a_p);
		bool Favorites(const std::shared_ptr<CoopPlayer>& a_p);
		bool GrabRotateYZ(const std::shared_ptr<CoopPlayer>& a_p);
		bool Jump(const std::shared_ptr<CoopPlayer>& a_p);
		bool PowerAttackDual(const std::shared_ptr<CoopPlayer>& a_p);
		bool PowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p);
		bool PowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p);
		bool Sheathe(const std::shared_ptr<CoopPlayer>& a_p);
		bool Sneak(const std::shared_ptr<CoopPlayer>& a_p);
		bool SpecialAction(const std::shared_ptr<CoopPlayer>& a_p);
		bool Sprint(const std::shared_ptr<CoopPlayer>& a_p);
		bool TeleportToPlayer(const std::shared_ptr<CoopPlayer>& a_p);

		// Multiple use.
		// Checks if the co-op camera manager is running.
		bool CamActive(const std::shared_ptr<CoopPlayer>& a_p);

		// Camera is adjustable if the player is not power attacking or trying to power attack,
		// not moving their arms, and moving the right stick.
		bool CanAdjustCamera(const std::shared_ptr<CoopPlayer>& a_p);

		// Can power attack if not already power attacking, 
		// on the ground, not sprinting, has the right weapon(s) equipped,
		// and has enough stamina.
		bool CanPowerAttack(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action);

		// Can this player rotate their forearms/hands/shoulders?
		bool CanRotateArms(const std::shared_ptr<CoopPlayer>& a_p);

		// Can cycle equipment if not attacking/casting, not changing gear, 
		// and the player does not have  their inventory copied over to P1.
		bool CycleEquipment(const std::shared_ptr<CoopPlayer>& a_p);

		// Can open (not item and perks related) menu 
		// if no other players are controlling menus already, 
		// and not changing gear or is transformed.
		bool PlayerCanOpenMenu(const std::shared_ptr<CoopPlayer>& a_p);

		// Can open item menu (Favorites, Inventory, Magic) 
		// if no other players are controlling menus, and not transformed.
		bool PlayerCanOpenItemMenu(const std::shared_ptr<CoopPlayer>& a_p);

		// Can open perks menu (Stats, Tween) if no other players are controlling menus already, 
		// and not changing gear or is transfromed.
		// In addition, P1 must be transformed or the requesting player must not be transformed.
		bool PlayerCanOpenPerksMenu(const std::shared_ptr<CoopPlayer>& a_p);
	};

	namespace HelperFuncs
	{
		// Returns true if the action has just started 
		// (condition check passed, input(s) just pressed).
		const bool ActionJustStarted
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);

		// Play idle to instantly bash, even in cases where the player cannot normally bash.
		void BashInstant(const std::shared_ptr<CoopPlayer>& a_p);

		// Send animation event to start/stop blocking.
		void BlockInstant(const std::shared_ptr<CoopPlayer>& a_p, bool&& a_shouldStart);

		// Returns true if the given cycling action can cycle on hold.
		// Must have cycle-on-hold enabled as a setting, 
		// or must be cycling emotes, which is the default behavior.
		const bool CanCycleOnHold
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);

		// Can the player dual cast their selected spells.
		// NOTE: 
		// Currently not functional.
		bool CanDualCast(const std::shared_ptr<CoopPlayer>& a_p);

		// Cache AV cost for special action and return true if the special action can be performed.
		const bool CanPerformSpecialAction
		(
			const std::shared_ptr<CoopPlayer>& a_p, const SpecialActionType& a_actionType
		);

		// Check if the currently targeted crosshair refr actor should be executed with a killmove,
		// and perform the killmove if conditions are met.
		// Return true if a killmove was performed.
		bool CheckForKillmove(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action);

		// Cache AV cost for this player action 
		// and return true if the player action can be performed.
		const bool EnoughOfAVToPerformPA
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);

		// Equip bound weapon with task.
		void EquipBoundWeapon
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::SpellItem* a_boundWeapSpell, 
			RE::TESObjectWEAP* a_boundWeap,
			RE::BGSEquipSlot* a_slot, 
			RE::MagicCaster* a_caster
		);

		// Equip the cached hotkeyed item to the requested equip index.
		void EquipHotkeyedForm
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			RE::TESForm* a_hotkeyedForm, 
			EquipIndex&& a_equipIndex
		);

		// Get player action HMS costs.
		const float GetPAHealthCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);
		const float GetPAMagickaCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);
		const float GetPAStaminaCost
		(
			const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action
		);

		// Get the currently selected hotkey slot based on the player's RS orientation.
		int32_t GetSelectedHotkeySlot(const std::shared_ptr<CoopPlayer>& a_p);

		// Co-op companions only. Equip bound weapon into the appropriate hand 
		// based on the player action performed.
		void HandleBoundWeaponEquip
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const InputAction& a_action
		);

		// Handle a delayed ('HotkeyEquip' bind released) hotkey equip request.
		// Return true if the current occurring action should skip executing its perf funcs,
		// since it is being used to equip the chosen hotkeyed item.
		bool HandleDelayedHotkeyEquipRequest
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const InputAction& a_occurringAction,
			const PlayerActionManager::PlayerActionState& a_paState
		);

		// Set up and run special interaction package if interacting with furniture.
		// Returns true if special interaction package was evaluated.
		bool InitiateSpecialInteractionPackage
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			RE::TESObjectREFR* a_interactionRefr, 
			RE::TESBoundObject* a_interactionBaseObj
		);

		// Have the given player loot all lootable items from the given container.
		void LootAllItemsFromContainer
		(
			const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_containerPtr
		);

		// Have the given player loot all lootable items from the given corpse.
		void LootAllItemsFromCorpse
		(
			const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_corpseRefrPtr
		);

		// Have the given player activate the given lootable refr (through P1 or otherwise).
		void LootRefr(const std::shared_ptr<CoopPlayer>& a_p, RE::TESObjectREFRPtr a_refrPtr);

		// Check if the player is moving in a particular direction 
		// relative to the direction they're facing.
		bool MovingBackward(const std::shared_ptr<CoopPlayer>& a_p, float a_lookMoveDiffAng);
		bool MovingForward(const std::shared_ptr<CoopPlayer>& a_p, float a_lookMoveDiffAng);
		bool MovingLeft(const std::shared_ptr<CoopPlayer>& a_p, float a_lookMoveDiffAng);
		bool MovingRight(const std::shared_ptr<CoopPlayer>& a_p, float a_lookMoveDiffAng);

		// Open menu corresponding to the given player action by emulating keyboard input for P1.
		void OpenMenuWithKeyboard
		(
			const std::shared_ptr<CoopPlayer>& a_p, 
			const InputAction& a_action
		);

		// Cycle through and highlight nearby interactable refrs while holding the 'Activate' bind.
		void PerformActivationCycling(const std::shared_ptr<CoopPlayer>& a_p);
		
		// Play or stop the currently cycled emote idle.
		void PlayEmoteIdle(const std::shared_ptr<CoopPlayer>& a_p);
		
		// Choose and play a killmove idle from the killmove list, 
		// if orientation and killmove type conditions hold.
		// Return true if a killmove was successfully played.
		bool PlayKillmoveFromList
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			RE::Actor* a_targetActor, 
			const bool& a_isOtherPlayer,
			const bool& a_hasCharacterSkeleton,
			const std::vector<RE::TESIdleForm*>& a_killmoveIdlesList
		);

		// Play a melee power attack animation based on the angular offset 
		// between player movement and facing directions.
		void PlayPowerAttackAnimation
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			const InputAction& a_action
		);

		// Modify the player's X angle before cssting a target location spell
		// to maximize chance of successfully casting the spell.
		void PrepForTargetLocationSpellCast
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			RE::SpellItem* a_spell
		);

		// Remove the LH/RH casting package from the top of the player's package stack 
		// to stop the companion player from casting in the LH/RH.
		void RemoveCastingPackage
		(
			const std::shared_ptr<CoopPlayer>& a_p, bool&& a_lhCast, bool&& a_rhCast
		);

		// Returns true if P1 is requesting to use the paraglider from Loki's awesome mod:
		// https://www.nexusmods.com/skyrimspecialedition/mods/53256
		bool RequestToUseParaglider(const std::shared_ptr<CoopPlayer>& a_p);

		// Change the co-op camera's adjustment mode (None, Rotate, or Zoom).
		void SetCameraAdjustmentMode
		(
			const int32_t& a_reqCID, const InputAction& a_action, bool&& a_set
		);

		// Change the co-op camera's state to the requested state 
		// or back to default if already set to that state.
		// Valid states are (Auto-Trail (default), Lock On, or Manual Positioning).
		void SetCameraState(const int32_t& a_reqCID, const InputAction& a_action);

		// Set up casting package to have a companion player cast a spell in the LH/RH.
		void SetUpCastingPackage
		(
			const std::shared_ptr<CoopPlayer>& a_p, bool&& a_lhCast, bool&& a_rhCast
		);
		
		// Update the target for the player's given magic caster.
		void UpdatePlayerMagicCasterTarget
		(
			const std::shared_ptr<CoopPlayer>& a_p,
			RE::MagicSystem::CastingSource&& a_casterSource
		);

		// Draw and use an equipped weapon while holding an attack bind.
		void UseWeaponOnHold(const std::shared_ptr<CoopPlayer>& a_p, const InputAction& a_action);

		// Set activation refr as interactable or not, 
		// and set the player's crosshair text to reflect the result.
		void ValidateActivationRefr(const std::shared_ptr<CoopPlayer>& a_p);
	};

	namespace ProgressFuncs
	{
		void Activate(const std::shared_ptr<CoopPlayer>& a_p);
		void AttackLH(const std::shared_ptr<CoopPlayer>& a_p);
		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p);
		void Bash(const std::shared_ptr<CoopPlayer>& a_p);
		void CastLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CastRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p);
		void GrabObject(const std::shared_ptr<CoopPlayer>& a_p);
		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p);
		void QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p);
		void Shout(const std::shared_ptr<CoopPlayer>& a_p);
		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p);
	};

	namespace StartFuncs
	{
		void ActivateAllOfType(const std::shared_ptr<CoopPlayer>& a_p);
		void ActivateCancel(const std::shared_ptr<CoopPlayer>& a_p);
		void Block(const std::shared_ptr<CoopPlayer>& a_p);
		void CamLockOn(const std::shared_ptr<CoopPlayer>& a_p);
		void CamManualPos(const std::shared_ptr<CoopPlayer>& a_p);
		void ChangeDialoguePlayer(const std::shared_ptr<CoopPlayer>& a_p);
		void CoopDebugMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void CoopIdlesMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void CoopMiniGamesMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void CoopSummoningMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void DebugRagdollPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		void DebugReEquipHandForms(const std::shared_ptr<CoopPlayer>& a_p);
		void DebugRefreshPlayerManagers(const std::shared_ptr<CoopPlayer>& a_p);
		void DebugResetPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		void DisableCoopCam(const std::shared_ptr<CoopPlayer>& a_p);
		void Dismount(const std::shared_ptr<CoopPlayer>& a_p);
		void Dodge(const std::shared_ptr<CoopPlayer>& a_p);
		void FaceTarget(const std::shared_ptr<CoopPlayer>& a_p);
		void Favorites(const std::shared_ptr<CoopPlayer>& a_p);
		void Inventory(const std::shared_ptr<CoopPlayer>& a_p);
		void Jump(const std::shared_ptr<CoopPlayer>& a_p);
		void MagicMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void MapMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void Pause(const std::shared_ptr<CoopPlayer>& a_p);
		void PowerAttackDual(const std::shared_ptr<CoopPlayer>& a_p);
		void PowerAttackLH(const std::shared_ptr<CoopPlayer>& a_p);
		void PowerAttackRH(const std::shared_ptr<CoopPlayer>& a_p);
		void QuickSlotItem(const std::shared_ptr<CoopPlayer>& a_p);
		void ResetAim(const std::shared_ptr<CoopPlayer>& a_p);
		void ResetCamOrientation(const std::shared_ptr<CoopPlayer>& a_p);
		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p);
		void Sheathe(const std::shared_ptr<CoopPlayer>& a_p);
		void Sneak(const std::shared_ptr<CoopPlayer>& a_p);
		void Sprint(const std::shared_ptr<CoopPlayer>& a_p);
		void StatsMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void TeleportToPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		void TradeWithPlayer(const std::shared_ptr<CoopPlayer>& a_p);
		void TweenMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void WaitMenu(const std::shared_ptr<CoopPlayer>& a_p);
		void ZoomCam(const std::shared_ptr<CoopPlayer>& a_p);
	};


	namespace CleanupFuncs
	{
		void Activate(const std::shared_ptr<CoopPlayer>& a_p);
		void AttackLH(const std::shared_ptr<CoopPlayer>& a_p);
		void AttackRH(const std::shared_ptr<CoopPlayer>& a_p);
		void Bash(const std::shared_ptr<CoopPlayer>& a_p);
		void Block(const std::shared_ptr<CoopPlayer>& a_p);
		void CastLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CastRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleAmmo(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellCategoryLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellCategoryRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleSpellRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleVoiceSlotMagic(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponCategoryLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponCategoryRH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponLH(const std::shared_ptr<CoopPlayer>& a_p);
		void CycleWeaponRH(const std::shared_ptr<CoopPlayer>& a_p);
		void GrabObject(const std::shared_ptr<CoopPlayer>& a_p);
		void HotkeyEquip(const std::shared_ptr<CoopPlayer>& a_p);
		void QuickSlotCast(const std::shared_ptr<CoopPlayer>& a_p);
		void RotateCam(const std::shared_ptr<CoopPlayer>& a_p);
		void Shout(const std::shared_ptr<CoopPlayer>& a_p);
		void SpecialAction(const std::shared_ptr<CoopPlayer>& a_p);
		void Sprint(const std::shared_ptr<CoopPlayer>& a_p);
		void ZoomCam(const std::shared_ptr<CoopPlayer>& a_p);
	}
}

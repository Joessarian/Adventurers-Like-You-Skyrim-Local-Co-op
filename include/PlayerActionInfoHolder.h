#pragma once
#include <set>
#include <Controller.h>
#include <Enums.h>

namespace ALYSLC
{
	// Contains player-specific and default binds information.
	struct PlayerActionInfoHolder
	{
		// Maps action groups to their associated actions' indices.
		using ActionGroupToIndicesMap = std::unordered_map<ActionGroup, std::set<uint32_t>>;
		// Maps actions' indices to their associated action groups.
		using ActionIndicesToGroupsMap = std::unordered_map<int32_t, ActionGroup>;
		// Player actions -> user event strings map.
		using ActionsToUEStringsMap = std::unordered_map<uint32_t, std::string_view>;
		// Maps input groups to their associated DXSCs.
		using InputGroupsToDXSCsMap = std::unordered_map<InputGroup, std::set<int32_t>>;
		// List of composing input actions for each player action.
		// Could be comprised of inputs and/or player actions.
		using ComposingInputActionList = std::vector<InputAction>;
		// List of all player actions and their corresponding lists of composing input actions.
		// Indexed with (player action index - index of the first player action). 
		// Const time random access over logarithmic time map access.
		using PACompInputActionLists = 
		std::array<ComposingInputActionList, (size_t)(InputAction::kActionTotal)>;
		// Flags representing press conditions to trigger a player action.
		using TriggerFlags = SKSE::stl::enumeration<TriggerFlag>;
		// Maps XInput masks to their associated input groups.
		using XInputMasksToInputGroups = std::unordered_map<uint32_t, InputGroup>;

		// Parameters related to triggering a specific action.
		class PlayerActionParams
		{
		public: 
			PlayerActionParams();
			PlayerActionParams
			(
				PerfType a_perfType, 
				TriggerFlags a_triggerFlags,
				ComposingInputActionList a_composingInputs, 
				uint32_t a_inputMask, 
				uint32_t a_composingPlayerActionsCount
			);
			PlayerActionParams(const PlayerActionParams& a_other);
			PlayerActionParams(PlayerActionParams&& a_other);

			PlayerActionParams& operator=(const PlayerActionParams& a_other);
			PlayerActionParams& operator=(PlayerActionParams&& a_other);

			// Buttons which must be pressed/analog sticks 
			// that must be moved to trigger the action.
			ComposingInputActionList composingInputs;
			// When to perform the action.
			PerfType perfType;
			// Flags related to triggering the action 
			// and what happens when the action is already triggered.
			TriggerFlags triggerFlags;
			// Input mask retrieved from bit operations on the composing inputs list.
			// Bits represent an input index, and if set, 
			// the input is pressed/moved for this action.
			uint32_t inputMask;
			// Number of composing player actions in this bind 
			// that were decomposed into composing inputs.
			uint32_t composingPlayerActionsCount;
		};

		// List of player actions and their trigger parameters.
		// Indexed with player action index.
		using PAParamsList = std::array<PlayerActionParams, (size_t)(InputAction::kActionTotal)>;

		PlayerActionInfoHolder();
		~PlayerActionInfoHolder() = default;

		PlayerActionInfoHolder(const PlayerActionInfoHolder& a_other) = delete;
		PlayerActionInfoHolder(PlayerActionInfoHolder&& a_other) = delete;

		PlayerActionInfoHolder& operator=(const PlayerActionInfoHolder& a_other) = delete;
		PlayerActionInfoHolder& operator=(PlayerActionInfoHolder&& a_other) = delete;

		//
		// Members
		//

		// Default player action parameters list.
		PAParamsList defPAParamsList;
		// Player action parameters lists for each player.
		std::array<PAParamsList, (size_t)ALYSLC_MAX_PLAYER_COUNT> playerPAParamsLists;

		// Maps action groups to lists of indices for actions that are part of that group.
		// Useful for getting all related actions for filtering.
		const ActionGroupToIndicesMap DEF_ACTION_GROUPS_TO_INDICES = 
		{
			{ 
				ActionGroup::kNone,
				{ 
					!InputAction::kNone 
				} 
			},
			{ 
				ActionGroup::kActivation,
				{ 
					!InputAction::kActivate, 
					!InputAction::kActivateAllOfType, 
					!InputAction::kActivateCancel 
				} 
			},
			{ 
				ActionGroup::kCamera,
				{ 
					!InputAction::kCamLockOn, 
					!InputAction::kCamManualPos, 
					!InputAction::kDisableCoopCam,
					!InputAction::kResetCamOrientation,
					!InputAction::kRotateCam, 
					!InputAction::kZoomCam 
				} 
			},
			{ 
				ActionGroup::kCombat,
				{ 
					!InputAction::kAttackLH, 
					!InputAction::kAttackRH,
					!InputAction::kBash, 
					!InputAction::kBlock, 
					!InputAction::kCastLH, 
					!InputAction::kCastRH,
					!InputAction::kPowerAttackDual, 
					!InputAction::kPowerAttackLH,
					!InputAction::kPowerAttackRH,
					!InputAction::kQuickSlotCast,
					!InputAction::kQuickSlotItem,
					!InputAction::kShout, 
					!InputAction::kSpecialAction
				}
			},
			{ 
				ActionGroup::kDebug,
				{ 
					!InputAction::kDebugRagdollPlayer, 
					!InputAction::kDebugReEquipHandForms, 
					!InputAction::kDebugRefreshPlayerManagers, 
					!InputAction::kDebugResetPlayer
				} 
			},
			{ 
				ActionGroup::kEquip,
				{ 
					!InputAction::kCycleAmmo, 
					!InputAction::kCycleSpellCategoryLH, 
					!InputAction::kCycleSpellCategoryRH, 
					!InputAction::kCycleSpellLH, 
					!InputAction::kCycleSpellRH, 
					!InputAction::kCycleVoiceSlotMagic, 
					!InputAction::kCycleWeaponCategoryLH, 
					!InputAction::kCycleWeaponCategoryRH, 
					!InputAction::kCycleWeaponLH, 
					!InputAction::kCycleWeaponRH, 
					!InputAction::kHotkeyEquip,
					!InputAction::kSheathe
				}
			},
			{ 
				ActionGroup::kMenu,
				{
					!InputAction::kCoopDebugMenu, 
					!InputAction::kCoopIdlesMenu, 
					!InputAction::kCoopMiniGamesMenu, 
					!InputAction::kCoopSummoningMenu, 
					!InputAction::kFavorites, 
					!InputAction::kInventory, 
					!InputAction::kMagicMenu, 
					!InputAction::kMapMenu, 
					!InputAction::kPause, 
					!InputAction::kStatsMenu, 
					!InputAction::kTradeWithPlayer, 
					!InputAction::kTweenMenu,
					!InputAction::kWaitMenu,
				} 
			},
			{ 
				ActionGroup::kMovement,
				{
					!InputAction::kDismount,
					!InputAction::kDodge,
					!InputAction::kFaceTarget,
					!InputAction::kJump,
					!InputAction::kRotateLeftForearm,
					!InputAction::kRotateLeftHand,
					!InputAction::kRotateLeftShoulder,
					!InputAction::kRotateRightForearm,
					!InputAction::kRotateRightHand,
					!InputAction::kRotateRightShoulder,
					!InputAction::kSneak,
					!InputAction::kSprint,
					!InputAction::kTeleportToPlayer
				} 
			},
			{ 
				ActionGroup::kTargeting,
				{ 
					!InputAction::kAdjustAimPitch,
					!InputAction::kChangeDialoguePlayer,
					!InputAction::kGrabObject, 
					!InputAction::kGrabRotateYZ, 
					!InputAction::kMoveCrosshair, 
					!InputAction::kResetAim,
				}
			},
		};

		// Maps player action indices to user event strings.
		// Used to get the corresponding P1 input event name for a player action.
		const ActionsToUEStringsMap ACTIONS_TO_P1_UE_STRINGS = 
		{
			{ !InputAction::kActivate, RE::UserEvents::GetSingleton()->activate },
			{ !InputAction::kAttackLH, RE::UserEvents::GetSingleton()->leftAttack },
			{ !InputAction::kAttackRH, RE::UserEvents::GetSingleton()->rightAttack },
			{ !InputAction::kBlock, RE::UserEvents::GetSingleton()->leftAttack },
			{ !InputAction::kCastLH, RE::UserEvents::GetSingleton()->leftAttack },
			{ !InputAction::kCastRH, RE::UserEvents::GetSingleton()->rightAttack },
			{ !InputAction::kFavorites, RE::UserEvents::GetSingleton()->favorites },
			{ !InputAction::kInventory, RE::UserEvents::GetSingleton()->quickInventory },
			{ !InputAction::kJump, RE::UserEvents::GetSingleton()->jump },
			{ !InputAction::kMagicMenu, RE::UserEvents::GetSingleton()->quickMagic },
			{ !InputAction::kMapMenu, RE::UserEvents::GetSingleton()->quickMap },
			{ !InputAction::kPause, RE::UserEvents::GetSingleton()->pause },
			{ !InputAction::kSheathe, RE::UserEvents::GetSingleton()->readyWeapon },
			{ !InputAction::kShout, RE::UserEvents::GetSingleton()->shout },
			{ !InputAction::kSneak, RE::UserEvents::GetSingleton()->sneak },
			{ !InputAction::kSprint, RE::UserEvents::GetSingleton()->sprint },
			{ !InputAction::kStatsMenu, RE::UserEvents::GetSingleton()->quickStats },
			{ !InputAction::kTweenMenu, RE::UserEvents::GetSingleton()->tweenMenu },
			{ !InputAction::kWaitMenu, RE::UserEvents::GetSingleton()->wait }
		};

		// Maps player action indices to action groups.
		// Useful for checking the type of a particular player action
		// when filtering or grouping the action.
		const ActionIndicesToGroupsMap DEF_ACTION_INDICES_TO_GROUPS = 
		{
			{ !InputAction::kActivate, ActionGroup::kActivation },
			{ !InputAction::kActivateAllOfType, ActionGroup::kActivation },
			{ !InputAction::kActivateCancel, ActionGroup::kActivation },
			{ !InputAction::kAdjustAimPitch, ActionGroup::kTargeting },
			{ !InputAction::kAttackLH, ActionGroup::kCombat },
			{ !InputAction::kAttackRH, ActionGroup::kCombat },
			{ !InputAction::kBash, ActionGroup::kCombat },
			{ !InputAction::kBlock, ActionGroup::kCombat },
			{ !InputAction::kCastLH, ActionGroup::kCombat },
			{ !InputAction::kCastRH, ActionGroup::kCombat },
			{ !InputAction::kCamLockOn, ActionGroup::kCamera },
			{ !InputAction::kCamManualPos, ActionGroup::kCamera },
			{ !InputAction::kChangeDialoguePlayer, ActionGroup::kTargeting },
			{ !InputAction::kCoopDebugMenu, ActionGroup::kMenu },
			{ !InputAction::kCoopIdlesMenu, ActionGroup::kMenu },
			{ !InputAction::kCoopMiniGamesMenu, ActionGroup::kMenu },
			{ !InputAction::kCoopSummoningMenu, ActionGroup::kMenu },
			{ !InputAction::kCycleAmmo, ActionGroup::kEquip },
			{ !InputAction::kCycleSpellCategoryLH, ActionGroup::kEquip },
			{ !InputAction::kCycleSpellCategoryRH, ActionGroup::kEquip },
			{ !InputAction::kCycleSpellLH, ActionGroup::kEquip },
			{ !InputAction::kCycleSpellRH, ActionGroup::kEquip },
			{ !InputAction::kCycleVoiceSlotMagic, ActionGroup::kEquip },
			{ !InputAction::kCycleWeaponCategoryLH, ActionGroup::kEquip },
			{ !InputAction::kCycleWeaponCategoryRH, ActionGroup::kEquip },
			{ !InputAction::kCycleWeaponLH, ActionGroup::kEquip },
			{ !InputAction::kCycleWeaponRH, ActionGroup::kEquip },
			{ !InputAction::kDebugRagdollPlayer, ActionGroup::kDebug },
			{ !InputAction::kDebugReEquipHandForms, ActionGroup::kDebug },
			{ !InputAction::kDebugRefreshPlayerManagers, ActionGroup::kDebug },
			{ !InputAction::kDebugResetPlayer, ActionGroup::kDebug },
			{ !InputAction::kDisableCoopCam, ActionGroup::kCamera },
			{ !InputAction::kDismount, ActionGroup::kMovement },
			{ !InputAction::kDodge, ActionGroup::kMovement },
			{ !InputAction::kFaceTarget, ActionGroup::kMovement },
			{ !InputAction::kFavorites, ActionGroup::kMenu },
			{ !InputAction::kGrabObject, ActionGroup::kTargeting },
			{ !InputAction::kGrabRotateYZ, ActionGroup::kTargeting },
			{ !InputAction::kHotkeyEquip, ActionGroup::kEquip },
			{ !InputAction::kInventory, ActionGroup::kMenu },
			{ !InputAction::kJump, ActionGroup::kMovement },
			{ !InputAction::kMagicMenu, ActionGroup::kMenu },
			{ !InputAction::kMapMenu, ActionGroup::kMenu },
			{ !InputAction::kMoveCrosshair, ActionGroup::kTargeting },
			{ !InputAction::kPause, ActionGroup::kMenu },
			{ !InputAction::kPowerAttackDual, ActionGroup::kCombat },
			{ !InputAction::kPowerAttackLH, ActionGroup::kCombat },
			{ !InputAction::kPowerAttackRH, ActionGroup::kCombat },
			{ !InputAction::kQuickSlotCast, ActionGroup::kCombat },
			{ !InputAction::kQuickSlotItem, ActionGroup::kCombat },
			{ !InputAction::kResetAim, ActionGroup::kTargeting },
			{ !InputAction::kResetCamOrientation, ActionGroup::kCamera },
			{ !InputAction::kRotateCam, ActionGroup::kCamera },
			{ !InputAction::kRotateLeftForearm, ActionGroup::kMovement },
			{ !InputAction::kRotateLeftHand, ActionGroup::kMovement },
			{ !InputAction::kRotateLeftShoulder, ActionGroup::kMovement },
			{ !InputAction::kRotateRightForearm, ActionGroup::kMovement },
			{ !InputAction::kRotateRightHand, ActionGroup::kMovement },
			{ !InputAction::kRotateRightShoulder, ActionGroup::kMovement },
			{ !InputAction::kSheathe, ActionGroup::kEquip },
			{ !InputAction::kShout, ActionGroup::kCombat },
			{ !InputAction::kSneak, ActionGroup::kMovement },
			{ !InputAction::kSpecialAction, ActionGroup::kCombat },
			{ !InputAction::kSprint, ActionGroup::kMovement },
			{ !InputAction::kStatsMenu, ActionGroup::kMenu },
			{ !InputAction::kTeleportToPlayer, ActionGroup::kMovement },
			{ !InputAction::kTradeWithPlayer, ActionGroup::kMenu },
			{ !InputAction::kTweenMenu, ActionGroup::kMenu },
			{ !InputAction::kWaitMenu, ActionGroup::kMenu },
			{ !InputAction::kZoomCam, ActionGroup::kCamera }
		};

		// Maps input groups to the DXSCs for inputs that make up those groups.
		// Used to get all the DXSCs for inputs that are part of a input group.
		const InputGroupsToDXSCsMap INPUT_GROUPS_TO_DXSCS = 
		{
			{ 
				InputGroup::kBumper, 
				{ DXSC_LEFT_SHOULDER, DXSC_RIGHT_SHOULDER } 
			},
			{ 
				InputGroup::kDPad, 
				{ DXSC_DPAD_UP, DXSC_DPAD_DOWN, DXSC_DPAD_LEFT, DXSC_DPAD_RIGHT }
			},
			{ 
				InputGroup::kMainFour, 
				{ DXSC_A, DXSC_B, DXSC_X, DXSC_Y }
			},
			{ 
				InputGroup::kMiddleTwo, 
				{ DXSC_START, DXSC_BACK } 
			},
			{ 
				InputGroup::kThumbstick, 
				{ DXSC_LEFT_THUMB, DXSC_RIGHT_THUMB } 
			},
			{ 
				InputGroup::kTrigger, 
				{ DXSC_LT, DXSC_RT } 
			}
		};

		// Default list (indexed by player action index) of composing input actions 
		// for each player action.
		const PACompInputActionLists DEF_PA_COMP_INPUT_ACTIONS_LIST = 
			PACompInputActionLists(
			{
				// Activate
				{ 
					InputAction::kA 
				},
				// ActivateAllOfType
				{ 
					InputAction::kActivate, 
					InputAction::kLShoulder 
				},
				// ActivateCancel
				{ 
					InputAction::kActivate, 
					InputAction::kRShoulder 
				},
				// AdjustAimPitch
				{ 
					InputAction::kLShoulder, 
					InputAction::kRShoulder, 
					InputAction::kRS 
				},
				// AttackLH
				{ 
					InputAction::kLT 
				},
				// AttackRH
				{
					InputAction::kRT 
				},
				// Bash
				{ 
					InputAction::kAttackRH 
				},
				// Block
				{ 
					InputAction::kAttackLH 
				},
				// CamLockOn
				{ 
					InputAction::kLShoulder, 
					InputAction::kRThumb 
				},
				// CamManualPos
				{ 
					InputAction::kRShoulder, 
					InputAction::kRThumb
				},
				// CastLH
				{ 
					InputAction::kAttackLH 
				},
				// CastRH
				{ 
					InputAction::kAttackRH 
				},
				// ChangeDialoguePlayer
				{ 
					InputAction::kPause 
				},
				// CoopDebugMenu
				{ 
					InputAction::kStart, 
					InputAction::kBack 
				},
				// CoopIdlesMenu
				{
					InputAction::kRShoulder, 
					InputAction::kDPadR 
				},
				// CoopMiniGamesMenu
				{ 
					InputAction::kRShoulder,
					InputAction::kDPadL
				},
				// CoopSummoningMenu
				{ 
					InputAction::kBack, 
					InputAction::kStart 
				},
				// CycleAmmo
				{ 
					InputAction::kLShoulder,
					InputAction::kY
				},
				// CycleSpellCategoryLH
				{ 
					InputAction::kY, 
					InputAction::kLShoulder 
				},
				// CycleSpellCategoryRH
				{
					InputAction::kY,
					InputAction::kRShoulder 
				},
				// CycleSpellLH
				{ 
					InputAction::kLShoulder,
					InputAction::kX
				},
				// CycleSpellRH
				{ 
					InputAction::kRShoulder, 
					InputAction::kX 
				},
				// CycleVoiceSlotMagic
				{
					InputAction::kRShoulder,
					InputAction::kY 
				},
				// CycleWeaponCategoryLH
				{ 
					InputAction::kY, 
					InputAction::kLT 
				},
				// CycleWeaponCategoryRH
				{ 
					InputAction::kY,
					InputAction::kRT 
				},
				// CycleWeaponLH
				{ 
					InputAction::kLShoulder, 
					InputAction::kB 
				},
				// CycleWeaponRH
				{ 
					InputAction::kRShoulder,
					InputAction::kB 
				},
				// DebugRagdollPlayer
				{ 
					InputAction::kLShoulder, 
					InputAction::kDPadR
				},
				// DebugReEquipHandForms
				{ 
					InputAction::kLShoulder,
					InputAction::kDPadL
				},
				// DebugRefreshPlayerManagers
				{ 
					InputAction::kLShoulder, 
					InputAction::kDPadD 
				},
				// DebugResetPlayer
				{ 
					InputAction::kLShoulder, 
					InputAction::kDPadU 
				},
				// DisableCoopCam
				{ 
					InputAction::kLThumb, 
					InputAction::kRThumb 
				},
				// Dismount
				{ 
					InputAction::kSneak 
				},
				// Dodge
				{ 
					InputAction::kSprint
				},
				// FaceTarget
				{
					InputAction::kLShoulder
				},
				// Favorites
				{
					InputAction::kBack 
				},
				// GrabObject
				{ 
					InputAction::kFaceTarget, 
					InputAction::kActivate 
				},
				// GrabRotateYZ
				{ 
					InputAction::kFaceTarget,
					InputAction::kRThumb, 
					InputAction::kRS 
				},
				// HotkeyEquip
				{ 
					InputAction::kRThumb,
					InputAction::kRS
				},
				// Inventory
				{ 
					InputAction::kDPadR
				},
				// Jump
				{ 
					InputAction::kY
				},
				// MagicMenu
				{
					InputAction::kDPadL
				},
				// MapMenu
				{ 
					InputAction::kDPadD
				},
				// MoveCrosshair
				{ 
					InputAction::kRS 
				},
				// Pause
				{
					InputAction::kStart
				},
				// PowerAttackDual
				{ 
					InputAction::kRShoulder, 
					InputAction::kAttackRH, 
					InputAction::kAttackLH 
				},
				// PowerAttackLH
				{ 
					InputAction::kRShoulder,
					InputAction::kAttackLH
				},
				// PowerAttackRH
				{ 
					InputAction::kRShoulder,
					InputAction::kAttackRH
				},
				// QuickSlotCast
				{
					InputAction::kLShoulder,
					InputAction::kRShoulder, 
					InputAction::kSpecialAction 
				},
				// QuickSlotItem
				{ 
					InputAction::kRShoulder, 
					InputAction::kLShoulder,
					InputAction::kSpecialAction 
				},
				// ResetAim
				{
					InputAction::kRShoulder
				},
				// ResetCamOrientation
				{
					InputAction::kLShoulder,
					InputAction::kRShoulder,
					InputAction::kRThumb,
				},
				// RotateCam
				{ 
					InputAction::kRShoulder, 
					InputAction::kRS 
				},
				// RotateLeftForearm
				{ 
					InputAction::kLShoulder, 
					InputAction::kAttackLH,
					InputAction::kRS 
				},
				// RotateLeftHand
				{ 
					InputAction::kLShoulder, 
					InputAction::kAttackLH, 
					InputAction::kRThumb, 
					InputAction::kRS 
				},
				// RotateLeftShoulder
				{ 
					InputAction::kAttackLH,
					InputAction::kRS
				},
				// RotateRightForearm
				{ 
					InputAction::kRShoulder, 
					InputAction::kAttackRH,
					InputAction::kRS 
				},
				// RotateRightHand
				{ 
					InputAction::kRShoulder,
					InputAction::kAttackRH, 
					InputAction::kRThumb, 
					InputAction::kRS 
				},
				// RotateRightShoulder
				{ 
					InputAction::kAttackRH,
					InputAction::kRS
				},
				// Sheathe
				{ 
					InputAction::kRThumb 
				},
				// Shout
				{ 
					InputAction::kRShoulder,
					InputAction::kA 
				},
				// Sneak
				{ 
					InputAction::kLThumb
				},
				// SpecialAction
				{
					InputAction::kX 
				},
				// Sprint
				{ 
					InputAction::kB
				},
				// StatsMenu
				{
					InputAction::kDPadU 
				},
				// TeleportToPlayer
				{ 
					InputAction::kRShoulder, 
					InputAction::kDPadU 
				},
				// TradeWithPlayer
				{
					InputAction::kRShoulder,
					InputAction::kDPadD 
				},
				// TweenMenu
				{ 
					InputAction::kRShoulder, 
					InputAction::kStart 
				},
				// WaitMenu
				{ 
					InputAction::kLShoulder,
					InputAction::kBack
				},
				// ZoomCam
				{ 
					InputAction::kLShoulder,
					InputAction::kRS 
				} 
			}
		);

		// Maps inputs' XInput masks to the input group those inputs belong to.
		// Useful for checking if XInput masks are in the same group of inputs.
		const XInputMasksToInputGroups XIMASKS_TO_INPUT_GROUPS =
		XInputMasksToInputGroups
		(
			{ 
				{ XINPUT_GAMEPAD_DPAD_UP, InputGroup::kDPad },
				{ XINPUT_GAMEPAD_DPAD_DOWN, InputGroup::kDPad },
				{ XINPUT_GAMEPAD_DPAD_LEFT, InputGroup::kDPad },
				{ XINPUT_GAMEPAD_DPAD_RIGHT, InputGroup::kDPad },
				{ XINPUT_GAMEPAD_START, InputGroup::kMiddleTwo },
				{ XINPUT_GAMEPAD_BACK, InputGroup::kMiddleTwo },
				{ XINPUT_GAMEPAD_LEFT_THUMB, InputGroup::kThumbstick },
				{ XINPUT_GAMEPAD_RIGHT_THUMB, InputGroup::kThumbstick },
				{ XINPUT_GAMEPAD_LEFT_SHOULDER, InputGroup::kBumper },
				{ XINPUT_GAMEPAD_RIGHT_SHOULDER, InputGroup::kBumper },
				{ XINPUT_GAMEPAD_A, InputGroup::kMainFour },
				{ XINPUT_GAMEPAD_B, InputGroup::kMainFour },
				{ XINPUT_GAMEPAD_X, InputGroup::kMainFour },
				{ XINPUT_GAMEPAD_Y, InputGroup::kMainFour },
				{ XMASK_LT, InputGroup::kTrigger },
				{ XMASK_RT, InputGroup::kTrigger } 
			}
		);

		// Base perform types for actions.
		const std::vector<PerfType> ACTION_BASE_PERF_TYPES =
		{
			// Activate
			PerfType::kOnHold,
			// ActivateAllOfType
			PerfType::kOnRelease,
			// ActivateCancel
			PerfType::kOnRelease,
			// AdjustAimPitch
			PerfType::kOnPressAndRelease,
			// AttackLH
			PerfType::kOnHold,
			// AttackRH
			PerfType::kOnHold,
			// Bash
			PerfType::kOnHold,
			// Block
			PerfType::kOnPressAndRelease,
			// CamLockOn
			PerfType::kOnRelease,
			// CamManualPos
			PerfType::kOnRelease,
			// CastLH
			PerfType::kOnHold,
			// CastRH
			PerfType::kOnHold,
			// ChangeDialoguePlayer
			PerfType::kOnRelease,
			// CoopDebugMenu
			PerfType::kOnRelease,
			// CoopIdlesMenu
			PerfType::kOnRelease,
			// CoopMiniGamesMenu
			PerfType::kOnRelease,
			// CoopSummoningMenu
			PerfType::kOnRelease,
			// CycleAmmo
			PerfType::kOnHold,
			// CycleSpellCategoryLH
			PerfType::kOnHold,
			// CycleSpellCategoryRH
			PerfType::kOnHold,
			// CycleSpellLH
			PerfType::kOnHold,
			// CycleSpellRH
			PerfType::kOnHold,
			// CycleVoiceSlotMagic
			PerfType::kOnHold,
			// CycleWeaponCategoryLH
			PerfType::kOnHold,
			// CycleWeaponCategoryRH
			PerfType::kOnHold,
			// CycleWeaponLH
			PerfType::kOnHold,
			// CycleWeaponRH
			PerfType::kOnHold,
			// DebugRagdollPlayer
			PerfType::kOnRelease,
			// DebugReEquipHandForms
			PerfType::kOnRelease,
			// DebugRefreshPlayerManagers
			PerfType::kOnRelease,
			// DebugResetPlayer
			PerfType::kOnRelease,
			// DisableCoopCam
			PerfType::kOnRelease,
			// Dismount
			PerfType::kOnRelease,
			// Dodge
			PerfType::kOnRelease,
			// FaceTarget
			PerfType::kOnRelease,
			// Favorites
			PerfType::kOnRelease,
			// GrabObject
			PerfType::kOnHold,
			// GrabRotateYZ
			PerfType::kNoAction,
			// HotkeyEquip
			PerfType::kOnHold,
			// Inventory
			PerfType::kOnRelease,
			// Jump
			PerfType::kOnRelease,
			// MagicMenu
			PerfType::kOnRelease,
			// MapMenu
			PerfType::kOnRelease,
			// MoveCrosshair
			PerfType::kNoAction,
			// Pause
			PerfType::kOnRelease,
			// PowerAttackDual
			PerfType::kOnRelease,
			// PowerAttackLH
			PerfType::kOnRelease,
			// PowerAttackRH
			PerfType::kOnRelease,
			// QuickSlotCast
			PerfType::kOnHold,
			// QuickSlotItem
			PerfType::kOnRelease,
			// ResetAim
			PerfType::kOnRelease,
			// ResetCamOrientation
			PerfType::kOnRelease,
			// RotateCam
			PerfType::kOnPressAndRelease,
			// RotateLeftForearm
			PerfType::kNoAction,
			// RotateLeftHand
			PerfType::kNoAction,
			// RotateLeftShoulder
			PerfType::kNoAction,
			// RotateRightForearm
			PerfType::kNoAction,
			// RotateRightHand
			PerfType::kNoAction,
			// RotateRightShoulder
			PerfType::kNoAction,
			// Sheathe
			PerfType::kOnRelease,
			// Shout
			PerfType::kOnHold,
			// Sneak
			PerfType::kOnRelease,
			// SpecialAction
			PerfType::kOnHold,
			// Sprint
			PerfType::kOnPressAndRelease,
			// StatsMenu
			PerfType::kOnRelease,
			// TeleportToPlayer
			PerfType::kOnRelease,
			// TradeWithPlayer
			PerfType::kOnRelease,
			// TweenMenu
			PerfType::kOnRelease,
			// WaitMenu
			PerfType::kOnRelease,
			// ZoomCam
			PerfType::kOnPressAndRelease
		};

		// Base trigger flags for actions.
		const std::vector<TriggerFlags> ACTION_BASE_TRIGGER_FLAGS = 
		{
			// Activate
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure
			),
			// ActivateAllOfType
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// ActivateCancel
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// AdjustAimPitch
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// AttackLH
			TriggerFlags
			(
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// AttackRH
			TriggerFlags
			(
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// Bash
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Block
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// CamLockOn
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CamManualPos
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CastLH
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// CastRH
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// ChangeDialoguePlayer
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CoopDebugMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CoopIdlesMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CoopMiniGamesMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CoopSummoningMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// CycleAmmo
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleSpellCategoryLH
			TriggerFlags
			(
				TriggerFlag::kLoneAction,
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleSpellCategoryRH
			TriggerFlags
			(
				TriggerFlag::kLoneAction,
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleSpellLH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleSpellRH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleVoiceSlotMagic
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleWeaponCategoryLH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleWeaponCategoryRH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleWeaponLH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// CycleWeaponRH
			TriggerFlags
			(
				TriggerFlag::kLoneAction, 
				TriggerFlag::kNoCleanupAfterInterrupt
			),
			// DebugRagdollPlayer
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// DebugReEquipHandForms
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// DebugRefreshPlayerManagers
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// DebugResetPlayer
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// DisableCoopCam
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering, 
				TriggerFlag::kLoneAction
			),
			// Dismount
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// Dodge
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// FaceTarget
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Favorites
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// GrabObject
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// GrabRotateYZ
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// HotkeyEquip
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// Inventory
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// Jump
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// MagicMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// MapMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// MoveCrosshair
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Pause
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// PowerAttackDual
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// PowerAttackLH
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// PowerAttackRH
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// QuickSlotCast
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// QuickSlotItem
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// ResetAim
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// ResetCamOrientation
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// RotateCam
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateLeftForearm
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateLeftHand
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateLeftShoulder
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateRightForearm
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateRightHand
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// RotateRightShoulder
			TriggerFlags
			(
				TriggerFlag::kBlockOnConditionFailure, 
				TriggerFlag::kDoNotUseCompActionsOrdering
			),
			// Sheathe
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Shout
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Sneak
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// SpecialAction
			TriggerFlags
			(
				TriggerFlag::kDefault
			),
			// Sprint
			TriggerFlags
			(
				TriggerFlag::kMinHoldTime, 
				TriggerFlag::kIgnoreConflictingActions
			),
			// StatsMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// TeleportToPlayer
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// TradeWithPlayer
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// TweenMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// WaitMenu
			TriggerFlags
			(
				TriggerFlag::kLoneAction
			),
			// ZoomCam
			TriggerFlags
			(
				TriggerFlag::kDoNotUseCompActionsOrdering
			)
		};

		//
		// Member funcs
		//

		// The given player action is decomposed into its composing inputs 
		// recursively up to a predetermined depth, which prevents infinite recursion.
		// The list of composing InputAction lists for the action 
		// is referenced to make these decompositions.
		// A set of composing inputs (not actions) is maintained to prevent duplicate insertions.
		// Returns a list of these composing inputs.
		// Also sets the number of player actions broken down 
		// and the max depth of recursion reached through the outparams.
		std::vector<InputAction> GetComposingInputs
		(
			const InputAction& a_playerAction, 
			const PACompInputActionLists& a_paCompInputActionsLists,
			std::set<InputAction>& a_composingInputsSet,
			uint32_t& a_numComposingPlayerActionsOut, 
			uint8_t& a_recursionDepthOut
		) const noexcept;

		// Get the input mask from a list of composing inputs, NOT player actions.
		// NOTE: 
		// Break down lists of composing InputActions into lists of composing inputs
		// before passing those lists into this function.
		const uint32_t GetInputMask
		(
			const ComposingInputActionList& a_composingInputs
		) const noexcept;
	};

	using PlayerActionParams = PlayerActionInfoHolder::PlayerActionParams;
};

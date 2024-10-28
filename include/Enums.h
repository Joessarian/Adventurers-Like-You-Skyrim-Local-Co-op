#pragma once

// Compat with fmt 10 and above through global override of format_as().
// Print enum types' underlying values for all enum types defined in CLib and this mod.
// Print strings' underlying char arrays for CLib-defined string types.
// Credits to Louis Go for the solution:
// https://stackoverflow.com/questions/77751341/fmt-failed-to-compile-while-formatting-enum-after-upgrading-to-v10-and-above
namespace RE
{
	template <typename T>
	requires std::is_enum_v<T> 
	auto format_as(T t)
	{
		return fmt::underlying(t);
	}

	namespace detail
	{
		template <typename T>
		requires std::is_same_v<T, BSFixedString<char>> 
		auto format_as(T t)
		{
			return t.c_str();
		}
	}
}

// Still have to call c_str() on passed-in BSString's when
// attempting to format with fmt::format(), even with custom format_as() func defined.
// Formatted string becomes garbage otherwise.
// Defining a formatter below seems to work though.
#ifdef FMT_VERSION
namespace fmt
{
	template <>
	struct formatter<RE::BSString>
	{
		template <class ParseContext>
		constexpr auto parse(ParseContext& a_ctx)
		{
			return a_ctx.begin();
		}

		template <class FormatContext>
		auto format(const RE::BSString& a_string, FormatContext& a_ctx)
		{
			return fmt::format_to(a_ctx.out(), "{}", a_string.data());
		}
	};
}
#endif

#ifdef __cpp_lib_format
namespace std
{
	template <class CharT>
	struct formatter<RE::BSString, CharT> : std::formatter<std::string_view, CharT>
	{
		template <class FormatContext>
		auto format(RE::BSString a_string, FormatContext& a_ctx)
		{
			return formatter<std::string_view, CharT>::format(a_string.data(), a_ctx);
		}
	};
}
#endif

namespace ALYSLC
{
	//========
	//[Camera]
	//========

	// Modes when manually adjusting camera orientation.
	enum class CamAdjustmentMode : std::uint8_t
	{
		kNone,		// Camera orientation is not being modified.
		kRotate,	// Rotating the camera.
		kZoom		// Adjusting camera zoom.
	};

	// Level of assistance provided when locked on to a target.
	enum class CamLockOnAssistanceLevel : std::uint8_t
	{
		kFull,		// Camera rotates and zooms out to keep players and lock on target in frame as much as possible.
		kRotation,	// Camera rotates to keep the lock on target on screen. Can zoom in/out normally.
		kZoom		// Camera zooms out to keep all players and the lock on target in frame. Can adjust rotation manually.
	};

	// Update camera data types if at least the given number of frames have elapsed.
	enum class CamMaxUpdateFrameCount : std::uint32_t
	{
		kLFP_CURVE = 36,		// Lock on focus point curve.
		kOP_CURVE = 12,			// Origin point curve.
		kTP_CURVE = 12,			// Target position point curve.
		kMOVEMENT_PITCH = 30,	// Movement pitch calculations.
		kMOVEMENT_YAW = 30		// Movement yaw calculations.
	};

	// Camera state types.
	enum class CamState : std::uint8_t
	{
		kAutoTrail,			// Camera automatically follows the party.
		kLockOn,			// Camera automatically tracks a target.
		kManualPositioning	// Camera is positioned and rotated with the Zoom and Rotation adjustment modes, and is static otherwise.
	};

	//============
	//[Controller]
	//============

	// Right stick locations:
	// 1: Top Right: X: (0.0f, 1.0f], Y: (0.0f, 1.0f]
	// 2: Bottom Right: X: (0.0f, 1.0f], Y: [-1.0f, 0.0f)
	// 3: Bottom Left: X: [-1.0f, 0.0f], Y: [-1.0f, 0.0f)
	// 4: Top Left: X: [-1.0f, 0.0f), Y: (0.0f, 1.0f]
	// 5. Pos X Axis: X: (0.0f, 1.0f], Y: 0.0f
	// 6. Neg Y Axis: X: 0.0f, Y: [-1.0f, 0.0f)
	// 7. Neg X Axis: X: [-1.0f, 0.0f), Y: 0.0f
	// 6. Pos Y Axis: X: 0.0f, Y: (0.0f, 1.0f]
	// Otherwise: 0 -> centered at (0.0f, 0.0f).
	enum class AnalogStickLocation
	{
		kCenter,
		kTopRight,
		kBottomRight,
		kBottomLeft,
		kTopLeft,
		kPosXAxis,
		kNegYAxis,
		kNegXAxis,
		kPosYAxis
	};

	// Input actions: include buttons, analog sticks, and performable player actions.
	// Some indices are used purely for indexing (such as kFirstAction, kLastAction, etc.
	enum class InputAction : std::uint32_t
	{
		kNone = 0xFFFFFFFF,
		kFirst = 0,
		kDPadU = kFirst,
		kDPadD,
		kDPadL,
		kDPadR,
		kStart,
		kBack,
		kLThumb,
		kRThumb,
		kLShoulder,
		kRShoulder,
		kA,
		kB,
		kX,
		kY,
		kLT,
		kRT,
		kButtonTotal,

		kLS = kButtonTotal,
		kRS,
		kInputTotal,

		kFirstAction = kInputTotal,
		kActivate = kFirstAction,
		kActivateAllOfType,
		kActivateCancel,
		kAdjustAimPitch,
		kAttackLH,
		kAttackRH,
		kBash,
		kBlock,
		kCamLockOn,
		kCamManualPos,
		kCastLH,
		kCastRH,
		kChangeDialoguePlayer,
		kCoopDebugMenu,
		kCoopIdlesMenu,
		kCoopMiniGamesMenu,
		kCoopSummoningMenu,
		kCycleAmmo,
		kCycleSpellCategoryLH,
		kCycleSpellCategoryRH,
		kCycleSpellLH,
		kCycleSpellRH,
		kCycleVoiceSlotMagic,
		kCycleWeaponCategoryLH,
		kCycleWeaponCategoryRH,
		kCycleWeaponLH,
		kCycleWeaponRH,
		kDebugRagdollPlayer,
		kDebugReEquipHandForms,
		kDebugRefreshPlayerManagers,
		kDebugResetPlayer,
		kDisableCoopCam,
		kDismount,
		kDodge,
		kFaceTarget,
		kFavorites,
		kGrabObject,
		kGrabRotateYZ,
		kInventory,
		kJump,
		kMagicMenu,
		kMapMenu,
		kMoveCrosshair,
		kPause,
		kPowerAttackDual,
		kPowerAttackLH,
		kPowerAttackRH,
		kQuickSlotCast,
		kQuickSlotItem,
		kResetAim,
		kRotateCam,
		kRotateLeftForearm,
		kRotateLeftHand,
		kRotateLeftShoulder,
		kRotateRightForearm,
		kRotateRightHand,
		kRotateRightShoulder,
		kSheathe,
		kShout,
		kSneak,
		kSpecialAction,
		kSprint,
		kStatsMenu,
		kTeleportToPlayer,
		kTradeWithPlayer,
		kTweenMenu,
		kWaitMenu,
		kZoomCam,
		kLastAction = kZoomCam,

		kTotal,
		kActionTotal = kTotal - kFirstAction,
		kInvalid = 0xFFFFFFFF
	};

	//=======
	//[Equip]
	//=======

	// Automatically equip ammo of a certain type.
	enum class AmmoAutoEquipMode : std::uint8_t
	{
		kNone,			// No auto-equip.
		kHighestCount,	// Equip matching ammo with the highest count.
		kHighestDamage	// Equip matching ammo with the highest damage.
	};

	// Types of cyclable items.
	enum class CyclableForms : std::uint8_t
	{
		kAmmo,
		kSpell,
		kVoice,
		kWeapon,

		kTotal
	};

	// Cyclable favorited spell categories.
	enum class FavMagicCyclingCategory : std::int8_t
	{
		kNone = -1,
		kAllFavorites,
		kAlteration,
		kConjuration,
		kDestruction,
		kIllusion,
		kRestoration,
		kRitual,
		kTotal
	};

	// Cyclable favorited weapon categories.
	enum class FavWeaponCyclingCategory : std::int8_t
	{
		kNone = -1,
		kAllFavorites,
		kSword,
		kDagger,
		kAxe,
		kMace,
		kBattleaxe,
		kWarhammer,
		kGreatsword,
		kBow,
		kCrossbow,
		kStaff,
		kShieldAndTorch,
		kUnique,
		kTotal
	};

	// Equip indices are used to keep track of equipped gear
	// and ensure gear is equipped into the correct slots.
	enum class EquipIndex : std::uint8_t
	{
		kLeftHand = 0,
		kRightHand,
		kAmmo,
		kVoice,
		kQuickSlotItem,
		kQuickSlotSpell,
		kWeapMagTotal,
		kFirstBipedSlot = kWeapMagTotal,
		kHead = kFirstBipedSlot,
		kHair,
		kBody,
		kHands,
		kForearms,
		kAmulet,
		kRing,
		kFeet,
		kCalves,
		kShield,
		kTail,
		kLongHair,
		kCirclet,
		kEars,
		kModMouth,
		kModNeck,
		kModChestPrimary,
		kModBack,
		kModMisc1,
		kModPelvisPrimary,
		kDecapitateHead,
		kDecapitate,
		kModPelvisSecondary,
		kModLegRight,
		kModLegLeft,
		kModFaceJewelry,
		kModChestSecondary,
		kModShoulder,
		kModArmLeft,
		kModArmRight,
		kModMisc2,
		kFX01,
		kTotal,
		kLastBipedSlot = kTotal - 1,
		kBipedTotal = kLastBipedSlot - kFirstBipedSlot + 1
	};

	// Indices representing the equip state of forms in menu item lists.
	// Corresponds to the 'caret' icon that appears to the left of the item's menu entry.
	enum class EntryEquipState : std::uint8_t
	{
		kNone,		// Not equipped (no caret displayed in front of name)
		kDefault,	// Voice/shield slot (empty caret displayed in front of name)
		kLH,		// Left hand (caret with "L" displayed in front of name)
		kRH,		// Right hand (caret with 'R' displayed in front of name)
		kBothHands	// Left and right hands (caret with 'LR' displayed in front of name)
	};

	// What slots to refresh when updating a player's equip state.
	enum class RefreshSlots : std::uint8_t
	{
		kAll,		// Armor, and weapons/magic slots.
		kArmor,		// Only armor.
		kWeapMag,	// Only weapons/magic.
	};

	//============
	//[Menu Input]
	//============

	// Helper (UIExtensions) menu request indices.
	enum class HelperMenu : std::uint8_t
	{
		kIdles = 0,
		kMiniGames,
		kStats,
		kTeleport,
		kTrade
	};

	// Type of input event triggered by a co-op companion player.
	enum class MenuInputEventType : std::uint8_t
	{
		kReleasedNoEvent,	// Inputs(s) released, no special handling.
		kPressedNoEvent,	// Inputs pressed, no special handling.
		kEquipReq,			// Requested to equip an item.
		kEmulateInput,		// Requested to send an input event from P1's controller/the keyboard.
		kTakeItemReq,		// Requested to loot an item.

		kTotal
	};

	// Menus controllable by co-op companions.
	// Also indicates when what control map to use.
	enum class SupportedMenu : std::uint8_t
	{
		kBarter,
		kBook, 
		kContainer,
		kDefault,		// Catch-all.
		kDialogue,
		kFavorites,
		kGift,
		kInventory,
		kJournal,
		kLockpicking,
		kLoot,
		kMagic,
		kMap,

		kTotal
	};

	//==========
	//[Movement]
	//==========

	// Arm node type.
	enum class ArmNodeType
	{
		kHand,
		kForearm,
		kShoulder
	};

	// Arm orientation about shoulder joint.
	enum class ArmOrientation
	{
		kUpward,
		kDownward,
		kInward,
		kOutward,
		kForward,
		kBackward,
		kTotal
	};

	// Movement params' index values.
	// NOTES:
	// - Components range: [-1.0f, 1.0]
	// - Delta LS game angle (Z rotation) gives the difference
	// in LS game angles from the previous iteration to the current one.
	// - Absolute game angles (Z rotation) are the sticks' angles
	// in the classical Cartesian coordinate system converted to
	// the game's angular coordinate system.
	// - Game angles (Z rotation) are the sticks' angles in Cartesian converted
	// to the game's coordinate system and then offset with the
	// camera's game angle (Z rotation).
	enum class MoveParams : std::uint16_t
	{
		kLSXComp,				// X (left/right) component of LS motion [-1.0f, 1.0f] relative to the camera's yaw (cam yaw points along the +Y axis).
		kLSYComp,				// Y (up/down) component of LS motion [-1.0f, 1.0f]	relative to the camera's yaw (cam yaw points along the +Y axis).	
		kRSXComp,				// X (left/right) component of RS motion [-1.0f, 1.0f] relative to the camera's yaw (cam yaw points along the +Y axis).
		kRSYComp,				// Y (up/down) component of RS motion [-1.0f, 1.0f]	relative to the camera's yaw (cam yaw points along the +Y axis).
		kSpeedMult,				// Player actor value speedmult to set.
		kLSGameAng,				// LS angle relative to the camera's yaw (0 along Y axis, increasing clockwise).
		kRSGameAng,				// RS angle relative to the camera's yaw (0 along Y axis, increasing clockwise).
		kDeltaLSAbsoluteAng,	// Change in LS game angle from the last frame to the current one.
		kLSAbsoluteAng,			// LS angle in the game's coordinate system (0 along +Y axis, increasing clockwise).
		kRSAbsoluteAng,			// RS angle in the game's coordinate system (0 along +Y axis, increasing clockwise).

		kTotal
	};

	// Custom node rotation blend status.
	enum class NodeRotationBlendStatus : std::uint8_t
	{
		kDefaultReached,		// Reached the rotation set by the game before modification.
		kBlendIn,				// Blending in towards the target rotation.
		kBlendOut,				// Blending out towards the default rotation.
		kTargetReached			// Reached the player-set target rotation.
	};

	//========
	//[Player]
	//========

	// Filter playable races for character customization.
	enum class SelectableRaceType : std::uint8_t
	{
		kPlayable,				// Only races with the playable flag.
		kUsedByNPCActorBase,	// Only races that have the 'ActorTypeNPC' keyword and are used by at least 1 actor base.
		kHasNPCKeyword,			// Has 'ActorTypeNPC' keyword in the CK.
		kUsedByAnyActorBase,	// Only races used by at least 1 actor base.
		kAll,					// Include all races.

		kTotal
	};

	//==================
	//[Threads/Managers]
	//==================

	// Running (or not) state of managers.
	enum class ManagerState : std::int32_t
	{
		kNone = -1,			// Invalid.
		kAwaitingRefresh,	// Awaiting refreshing of player data.
		kPaused,			// Inactive until resumed.
		kRunning,			// Active.
		kUninitialized,		// Data not initialized yet.

		kTotal
	};

	// Manager types. Primarily for debug purposes.
	enum class ManagerType : std::int32_t
	{
		kNone = -1,	// Invalid.
		kCAM,		// Camera.
		kEM,		// Equip Manager.
		kMIM,		// Menu Input Manager.
		kMM,		// Movement Manager
		kP,			// Player manager.
		kPAM,		// Player Action Manager.
		kTM,		// Targeting Manager.

		kTotal
	};

	//====================
	//[Player Action Info]
	//====================

	// Action categories to group similar player actions.
	enum class ActionGroup : std::int8_t
	{
		kNone = -1,
		kActivation,
		kCamera,
		kCombat,
		kDebug,
		kEquip,
		kMenu,
		kMovement,
		kTargeting,

		kTotal
	};

	// Group related inputs.
	enum class InputGroup : std::int8_t
	{
		kNone = -1,
		kBumper,		// LB/RB or L1/R1.
		kDPad,
		kMainFour,		// A/B/X/Y or X/Square/Triangle/Circle.
		kMiddleTwo,		// Back and Start or Share/Select and Options
		kThumbstick,	
		kTrigger,

		kTotal
	};

	// Enumerates a set of animation event tags for easy comparison.
	// (no need for hashing or string equality checks)
	// Used in conjunction with last AV modifying action request
	// to determine the AV cost for that action.
	enum class PerfAnimEventTag : std::uint8_t
	{
		kNone,

		kAttackStop,
		kBeginCastLeft,
		kBeginCastRight,
		kBowRelease,
		kCastLeft,
		kCastRight,
		kCastStop,
		kDodgeStart,
		kDodgeStop,
		kPreHitFrame,
		kHitFrame,
		kSprintStart,
		kSprintStop,

		kTotal
	};

	enum class PerfStage : std::uint8_t
	{
		kBlocked,				// Blocked from executing start/hold/cleanup funcs.
		kConflictingAction,		// Interrupted by conflicting action before or after starting.
		kFailedConditions,		// Interrupted by failed conditions after starting.
		kInputsPressed,			// All composing inputs pressed.
		kInputsReleased,		// Fully-stopped action: all required composing inputs released.
		kSomeInputsPressed,		// Some composing inputs are pressed and the action has not already started.
		kSomeInputsReleased,	// Some composing inputs released after the action has started.
		kStarted				// Started action: inputs pressed, conditions passed, and is being performed.
	};

	// When to perform an action.
	// Funcs run by type:
	// Disabled:			NONE
	// OnConsecTap:			Condition -> Start
	// OnHold:				Condition -> Progress -> Cleanup
	// OnPress:				Condition -> Start
	// OnPressAndRelease:	Condition -> Start -> Cleanup
	// OnRelease:			Condition -> Start
	// NoAction:			Condition
	// NOTE: Not all actions have 'Condition' functions.
	enum class PerfType : std::uint8_t
	{
		kDisabled = 0,
		kOnConsecTap,
		kOnHold,
		kOnPress,
		kOnPressAndRelease,
		kOnRelease,
		kNoAction
	};

	// Flags which describe how to handle changes in state for player actions.
	// PAs = player actions.
	enum class TriggerFlag : std::uint8_t
	{
		// Default (None):
		// - Block conflicting PAs.
		// - When interrupted, stop performing action and resume performing
		// only if all inputs are pressed once more, conditions hold, and no
		// other action has blocked this one in the meantime.
		// - Not a lone action, so other actions can be performed at the same time.
		// - No min hold time, so actions start on press, hold, or release without delay.
		// - Run cleanup function if one exists for this action and if this action
		// was interrupted. Can run once some composing inputs are released.
		kDefault = 0,
		// Change perf stage to 'Blocked' instead of 'Failed Conditions' when this action fails its condition check.
		// Prevents resumption if the condition check for the action passes later while the bind is still pressed.
		kBlockOnConditionFailure = 1 << 0,
		// Do not block any of this action's conflicting PAs.
		kDoNotBlockConflictingActions = 1 << 1,
		// Order in which the action's composing actions are pressed does not matter.
		kDoNotUseCompActionsOrdering = 1 << 2,
		// Ignore condition failure and continue performing.
		kIgnoreConditionFailure = 1 << 3,
		// Ignore interrupts from conflicting actions.
		kIgnoreConflictingActions = 1 << 4,
		// Only trigger if this is the only action whose composing inputs have been pressed.
		// (IGNORING LS/RS movement).
		kLoneAction = 1 << 5,
		// Only perform after the last input (or any input if ordering doesn't matter)
		// in the composing inputs list is held for a min interval.
		// Otherwise, perform on press/hold/release as normal.
		kMinHoldTime = 1 << 6,
		// Do not clean up the action after it is interrupted.
		kNoCleanupAfterInterrupt = 1 << 7
	};

	//==========================
	//[Player Action Management]
	//==========================

	// Lists out performable actions that modify an actor's actor values
	// once an associated animation event triggers.
	// Actov Value Cost Actions.
	enum class AVCostAction : std::uint16_t
	{
		kNone = 0,

		kBash = 1 << 0,
		kCastDual = 1 << 1,
		kCastLeft = 1 << 2,
		kCastRight = 1 << 3,
		kDodge = 1 << 4,
		kPowerAttackDual = 1 << 5,
		kPowerAttackLeft = 1 << 6,
		kPowerAttackRight = 1 << 7,
		kSneakRoll = 1 << 8,
		kSprint = 1 << 9
	};

	// Button press type to send when emulating P1 input.
	enum class ButtonEventPressType : std::uint8_t
	{
		kInstantTrigger,	// Trigger right away: InputEvent 'value' arg set to 1, held time is not set (0).
		kPressAndHold,		// Button is being held after first press: InputEvent 'value' arg is 1 and held time is set.
		kRelease,			// Button was released: InputEvent 'value' arg is set to 0 and final held time is set.

		kTotal
	};

	// Index for each co-op companion player's magic casting global variables used in their ranged attack package.
	enum class CastingGlobIndex : std::uint8_t
	{
		kLH,	// Left hand cast.
		kRH,	// Right hand cast.
		k2H,	// Two handed cast (Ritual spells).
		kDual,	// Left and right hand simultaneous cast.
		kShout,	// Currently unused: perform Shout.
		kVoice,	// Currently unused: cast Power.
		kTotal
	};

	// Last hand an attack originated from.
	// Used to set weapon damage multiplier,
	// and determine weapon skill XP increases.
	enum class HandIndex : std::int8_t
	{
		kNone = -1,			// Invalid
		kDefault = kNone,	// Default slot.
		kLH,				// Left hand.
		kRH,				// Right hand.
		k2H,				// Two-handed.
		kBoth,				// Left hand and right hand (dual attack).

		kTotal
	};

	// Indices for each player's co-op packages.
	enum class PackageIndex : std::uint8_t
	{
		kDefault = 0,			// No package running.
		kCombatOverride,		// Overrides combat.
		kRangedAttack,			// Performing ranged attack (only spellcasting for now).
		kSpecialInteraction,	// Performing activation of furniture or activators.

		kTotal
	};

	// Indices for co-op companion player placeholder magic spells.
	// Spells are copied into these placeholders so that the ranged package
	// can cast any spell in its procedures, instead of just predetermined spells.
	// See the .esp's for details.
	enum class PlaceholderMagicIndex : std::uint8_t
	{
		kLH,
		kRH,
		k2H,
		kVoice,
		kTotal
	};

	// Indicates the type of skill-increasing combat action being performed and from what source.
	// QS = quick slot.
	enum class SkillIncCombatActionType : std::uint32_t
	{
		kNone = 0,
		kAlterationSpellLH = 1 << 0,
		kAlterationSpellRH = 1 << 1,
		kAlterationSpellQS = 1 << 2,
		kArchery = 1 << 3,
		kBlock = 1 << 4,
		kConjurationSpellLH = 1 << 5,
		kConjurationSpellRH = 1 << 6,
		kConjurationSpellQS = 1 << 7,
		kDestructionSpellLH = 1 << 8,
		kDestructionSpellRH = 1 << 9, 
		kDestructionSpellQS = 1 << 10,
		kIllusionSpellLH = 1 << 11,
		kIllusionSpellRH = 1 << 12,
		kIllusionSpellQS = 1 << 13,
		kOneHandedLH = 1 << 14,
		kOneHandedRH = 1 << 15,
		kRestorationSpellLH = 1 << 16,
		kRestorationSpellRH = 1 << 17,
		kRestorationSpellQS = 1 << 18,
		kTwoHanded = 1 << 19
	};

	// Types of contextual actions that can be performed with the 'SpecialAction' bind.
	enum class SpecialActionType
	{
		kNone,
		kBash,
		kBlock,
		kCastBothHands,
		kCycleOrPlayEmoteIdle,
		kDodge,
		kDualCast,
		kGetUp,
		kQuickCast,
		kQuickItem,
		kRagdoll,
		kTransformation
	};

	//===============
	//[Serialization]
	//===============

	// Identifiers for serializable co-op data.
	enum class SerializableDataType : std::uint32_t
	{
		kPlayerFirstSavedLevel = 'PFSL', 
		kPlayerLevel = 'PLVL',
		kPlayerLevelXP = 'PLXP',
		kPlayerBaseHMSPointsList = 'PBPL',
		kPlayerHMSPointsIncList = 'PPIL',
		kPlayerBaseSkillLevelsList = 'PBSL',
		kPlayerSkillIncreasesList = 'PSIL',
		kPlayerSkillXPList = 'PSXL',
		kPlayerCopiedMagicList = 'PCML',
		kPlayerEquippedObjectsList = 'PEOL',
		kPlayerAvailablePerkPoints = 'PAPP',
		kPlayerUnlockedPerksList = 'PUPL',
		kPlayerSharedPerksUnlocked = 'PSPU',
		kPlayerUsedPerkPoints = 'PUPP',
		kPlayerExtraPerkPoints = 'PEPP',
		kPlayerEmoteIdleEvents = 'PEIE',
		kSerializationVersion = 0
	};

	//==================
	//[Shared Data/Util]
	//==================

	// Tiers of Enderal skill books for indexing purposes.
	enum class EnderalSkillbookTier : std::uint8_t
	{
		kApprentice,
		kAdept,
		kExpert,
		kMaster,

		kTotal
	};

	// Used to categorize killmove idles.
	enum class KillmoveType : std::int8_t
	{
		kNone = -1, 
		
		kFirst = 0,
		kH2H = kFirst,		// Hand to hand.
		k1H,
		k2H,
		kDW,				// Dual wield.
		k1HAxe,
		k1HDagger,
		k1HMace,
		k1HSword,
		k2HAxe,
		k2HSword,
		kShield,
		kSneakH2H,
		kSneak1H,
		kSneak2H,
		kGeneral,			// No weapon category.
		kVampireLord,
		kWerewolf,
		kTotal,
		kLast = kTotal - 1
	};

	// Indices for each default movement type used by Characters.
	enum class NPCMovementTypeID : std::uint16_t
	{
		k1HM,
		k2HM,
		kAttacking2H,
		kAttacking,
		kBleedOut,
		kBlocking,
		kBlockingShieldCharge,
		kBow,
		kBowDrawn,
		kBowDrawnQuickShot,
		kDefault,
		kDrunk,
		kHorse,
		kMagic,
		kMagicCasting,
		kPowerAttacking,
		kSneaking,
		kSprinting,
		kSwimming,

		kTotal
	};

	//===========
	//[Targeting]
	//===========

	// Projectile trajectory type for each player.
	enum class ProjectileTrajType : std::uint8_t
	{
		kAimDirection,	// Shoot in aiming direction, no trajectory modification after launch.
		kHoming,		// Shoot at target/crosshair position, trajectory modification during lifetime of projectile.
		kPrediction		// Predict target intercept position on launch, no trajectory modification afterward.
	};

	// Used when comparing and grouping objects for activation when looting objects within the player's reach.
	enum class RefrCompType : std::uint8_t
	{
		kSameBaseForm,
		kSameBaseFormType
	};

	// Type of actor targeted by the player's crosshair or in their facing direction.
	enum class TargetActorType : std::int8_t
	{
		kNone = -1,		// Invalid/no target.
		kAimCorrection,	// Closest actor relative to the player's facing direction. Aim Correction setting must be enabled.
		kLinkedRefr,	// Linked refr target used by ranged attack package and Quick Slot casting.
		kSelected,		// Actor selected by the player's crosshair.

		kTotal
	};

	//====
	//[UI]
	//====

	// Data copyable from one player to another when opening certain menus.
	enum class CopyablePlayerDataTypes : std::uint8_t
	{
		kNone = 0,
		kCarryWeight = 1 << 0,
		kFavorites = 1 << 1,
		kInventory = 1 << 2,
		kName = 1 << 3,
		kPerkList = 1 << 4,
		kPerkTree = 1 << 5,
		kRaceName = 1 << 6,
		kSkillsAndHMS = 1 << 7
	};

	// Crosshair text notification type.
	enum class CrosshairMessageType : std::uint16_t
	{
		kNone,
		kActivationInfo,		// Info about what object is being targeted for activation.
		kCamera,				// Changes to camera state or adjustment mode.
		kEquippedItem,			// Info about a newly-equipped item.
		kGeneralNotification,	// Catch-all.
		kReviveAlert,			// Life/Revive percent for downed players.
		kShoutCooldown,			// Shout cooldown remaining when attempting to shout while on cooldown.
		kStealthState,			// Detection percent overall and for selected target, if any.
		kTargetSelection,		// Info about a selected actor or object.

		kTotal
	};

	// Credits to Ryan-rsm-McKenzie and his quick loot repo for
	// the code on modifying the crosshair text.
	// https://github.com/Ryan-rsm-McKenzie
	enum HUDBaseArgs : std::size_t
	{
		kActivate,
		kName,
		kShowButton,
		kTextOnly,
		kFavorMode,
		kShowCrosshair,
		kWeight,
		kCost,
		kFieldValue,
		kFieldText,

		kTotal
	};

	// Player indicator visibility settings.
	enum PlayerIndicatorVisibilityType : std::uint8_t
	{
		kDisabled,		// Never show.
		kLowVisibility,	// Only show when the player is obscured or far away.
		kAlways			// Always show.
	};

	// Maps each action to its string name for logger prints.
	static inline std::string_view InputActionToString(InputAction a_action) noexcept
	{
		switch (a_action)
		{
		case InputAction::kDPadU:
			return "DPadU";
		case InputAction::kDPadD:
			return "DPadD";
		case InputAction::kDPadL:
			return "DPadL";
		case InputAction::kDPadR:
			return "DPadR";
		case InputAction::kStart:
			return "Start";
		case InputAction::kBack:
			return "Back";
		case InputAction::kLThumb:
			return "LThumb";
		case InputAction::kRThumb:
			return "RThumb";
		case InputAction::kLShoulder:
			return "LShoulder";
		case InputAction::kRShoulder:
			return "RShoulder";
		case InputAction::kA:
			return "A";
		case InputAction::kB:
			return "B";
		case InputAction::kX:
			return "X";
		case InputAction::kY:
			return "Y";
		case InputAction::kLT:
			return "LT";
		case InputAction::kRT:
			return "RT";
		case InputAction::kLS:
			return "LS";
		case InputAction::kRS:
			return "RS";
		case InputAction::kActivate:
			return "Activate";
		case InputAction::kActivateAllOfType:
			return "ActivateAllOfType";
		case InputAction::kActivateCancel:
			return "ActivateCancel";
		case InputAction::kAdjustAimPitch:
			return "AdjustAimPitch";
		case InputAction::kAttackLH:
			return "AttackLH";
		case InputAction::kAttackRH:
			return "AttackRH";
		case InputAction::kBash:
			return "Bash";
		case InputAction::kBlock:
			return "Block";
		case InputAction::kCamLockOn:
			return "CamLockOn";
		case InputAction::kCamManualPos:
			return "CamManualPos";
		case InputAction::kCastLH:
			return "CastLH";
		case InputAction::kCastRH:
			return "CastRH";
		case InputAction::kChangeDialoguePlayer:
			return "ChangeDialoguePlayer";
		case InputAction::kCoopDebugMenu:
			return "CoopDebugMenu";
		case InputAction::kCoopIdlesMenu:
			return "CoopIdlesMenu";
		case InputAction::kCoopMiniGamesMenu:
			return "CoopMiniGamesMenu";
		case InputAction::kCoopSummoningMenu:
			return "CoopSummoningMenu";
		case InputAction::kCycleAmmo:
			return "CycleAmmo";
		case InputAction::kCycleSpellCategoryLH:
			return "CycleSpellCategoryLH";
		case InputAction::kCycleSpellCategoryRH:
			return "CycleSpellCategoryRH";
		case InputAction::kCycleSpellLH:
			return "CycleSpellLH";
		case InputAction::kCycleSpellRH:
			return "CycleSpellRH";
		case InputAction::kCycleVoiceSlotMagic:
			return "CycleVoiceSlotMagic";
		case InputAction::kCycleWeaponCategoryLH:
			return "CycleWeaponCategoryLH";
		case InputAction::kCycleWeaponCategoryRH:
			return "CycleWeaponCategoryRH";
		case InputAction::kCycleWeaponLH:
			return "CycleWeaponLH";
		case InputAction::kCycleWeaponRH:
			return "CycleWeaponRH";
		case InputAction::kDebugRagdollPlayer:
			return "DebugRagdollPlayer";
		case InputAction::kDebugReEquipHandForms:
			return "DebugReEquipHandForms";
		case InputAction::kDebugRefreshPlayerManagers:
			return "DebugRefreshPlayerManagers";
		case InputAction::kDebugResetPlayer:
			return "DebugResetPlayer";
		case InputAction::kDisableCoopCam:
			return "DisableCoopCam";
		case InputAction::kDismount:
			return "Dismount";
		case InputAction::kDodge:
			return "Dodge";
		case InputAction::kFaceTarget:
			return "FaceTarget";
		case InputAction::kFavorites:
			return "Favorites";
		case InputAction::kGrabObject:
			return "GrabObject";
		case InputAction::kGrabRotateYZ:
			return "GrabRotateYZ";
		case InputAction::kInventory:
			return "Inventory";
		case InputAction::kJump:
			return "Jump";
		case InputAction::kMagicMenu:
			return "MagicMenu";
		case InputAction::kMapMenu:
			return "MapMenu";
		case InputAction::kMoveCrosshair:
			return "MoveCrosshair";
		case InputAction::kNone:
			return "NONE";
		case InputAction::kPause:
			return "Pause";
		case InputAction::kPowerAttackDual:
			return "PowerAttackDual";
		case InputAction::kPowerAttackLH:
			return "PowerAttackLH";
		case InputAction::kPowerAttackRH:
			return "PowerAttackRH";
		case InputAction::kQuickSlotCast:
			return "QuickSlotCast";
		case InputAction::kQuickSlotItem:
			return "QuickSlotItem";
		case InputAction::kResetAim:
			return "ResetAim";
		case InputAction::kRotateCam:
			return "RotateCam";
		case InputAction::kRotateLeftForearm:
			return "RotateLeftForearm";
		case InputAction::kRotateLeftHand:
			return "RotateLeftHand";
		case InputAction::kRotateLeftShoulder:
			return "RotateLeftShoulder";
		case InputAction::kRotateRightForearm:
			return "RotateRightForearm";
		case InputAction::kRotateRightHand:
			return "RotateRightHand";
		case InputAction::kRotateRightShoulder:
			return "RotateRightShoulder";
		case InputAction::kSheathe:
			return "Sheathe";
		case InputAction::kShout:
			return "Shout";
		case InputAction::kSneak:
			return "Sneak";
		case InputAction::kSpecialAction:
			return "SpecialAction";
		case InputAction::kSprint:
			return "Sprint";
		case InputAction::kStatsMenu:
			return "StatsMenu";
		case InputAction::kTeleportToPlayer:
			return "TeleportToPlayer";
		case InputAction::kTradeWithPlayer:
			return "TradeWithPlayer";
		case InputAction::kTweenMenu:
			return "TweenMenu";
		case InputAction::kWaitMenu:
			return "WaitMenu";
		case InputAction::kZoomCam:
			return "ZoomCam";
		default:
			return "N/A";
		}
	}

	// REMOVE. FOR DEBUGGING.
	static inline std::string_view ManagerStateToString(ManagerState a_state)
	{
		switch (a_state)
		{
		case ManagerState::kAwaitingRefresh:
		{
			return "'Awaiting Refresh'"sv;
		}
		case ManagerState::kPaused:
		{
			return "'Paused'"sv;
		}
		case ManagerState::kRunning:
		{
			return "'Running'"sv;
		}
		case ManagerState::kUninitialized:
		{
			return "'Uninitialized'"sv;
		}
		}

		return "'INVALID'"sv;
	}

	static inline std::string_view ManagerTypeToString(ManagerType a_state)
	{
		switch (a_state)
		{
		case ManagerType::kCAM:
		{
			return "[CAM]"sv;
		}
		case ManagerType::kEM:
		{
			return "[EM]"sv;
		}
		case ManagerType::kMIM:
		{
			return "[MIM]"sv;
		}
		case ManagerType::kMM:
		{
			return "[MM]"sv;
		}
		case ManagerType::kP:
		{
			return "[P]"sv;
		}
		case ManagerType::kPAM:
		{
			return "[PAM]"sv;
		}
		case ManagerType::kTM:
		{
			return "[TM]"sv;
		}
		default:
		{
			return "[INVALID MANAGER TYPE]";
		}
		}
	}

	// Overrides default formatting for select enums.
	template <typename T>
	requires (std::is_enum_v<T>) 
	auto format_as(T t)
	{
		if constexpr (std::is_same_v<T, InputAction>)
		{
			return InputActionToString(t);
		}
		else if constexpr (std::is_same_v<T, ManagerState>)
		{
			return ManagerStateToString(t);
		}
		else if constexpr (std::is_same_v<T, ManagerType>)
		{
			return ManagerTypeToString(t);
		}
		else
		{
			return fmt::underlying(t);
		}
	}
};


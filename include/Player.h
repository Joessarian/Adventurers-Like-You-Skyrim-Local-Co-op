#pragma once
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <Windows.h>
#include <Xinput.h>

#include <Manager.h>
#include <Controller.h>
#include <Enums.h>
#include <EquipManager.h>
#include <MovementManager.h>
#include <PlayerActionManager.h>
#include <TargetingManager.h>
#pragma comment (lib, "xinput.lib")


namespace ALYSLC
{
	using SteadyClock = std::chrono::steady_clock;
	class EquipManager;
	class MovementManager;
	class PlayerActionManager;
	class TargetingManager;

	class CoopPlayer : public Manager
	{
	public:
		// Friend classes (managers).
		friend class EquipManager;
		friend class MovementManager;
		friend class PlayerActionManager;
		friend class TargetingManager;

		CoopPlayer();
		CoopPlayer
		(
			int32_t a_controllerID, RE::Actor* a_coopActor,  uint32_t a_packageFormListStartIndex
		);

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		// Get player's current mount. Can be none.
		inline RE::ActorPtr GetCurrentMount()
		{
			if (currentMountHandle && currentMountHandle.get()) 
			{
				return currentMountHandle.get();
			}

			return nullptr;
		}

		// Set the co-op actor's race to the given race.
		inline void SetCoopPlayerRace(RE::TESRace* a_race)
		{
			if (coopActor && coopActor->race && a_race) 
			{
				coopActor->SwitchRace(a_race, false);
				coopActor->race = a_race;
				coopActor->GetActorBase()->originalRace = a_race;
				coopActor->GetActorBase()->race = a_race;
			}
		}

		// Set mount to target before running mount task.
		inline void SetTargetedMount(RE::ActorHandle a_handle)
		{
			targetedMountHandle.reset();
			if (a_handle.get()) 
			{
				targetedMountHandle = a_handle;
			}
		}
		//
		// Member funcs
		//

		// Copy base NPC's headparts, skin tone, gender, and more to the player.
		// Set opposite gender animations, if needed.
		void CopyNPCAppearanceToPlayer(RE::TESNPC* a_baseToCopy, bool a_setOppositeGenderAnims);

		// Set as dismissed, pause all managers, reset revive/downed/transformation state,
		// and send dismissal event to script to handle cleanup.
		void DismissPlayer();

		// If the player's CID has changed since starting co-op, get all CIDs
		// and re-assign them to all players.
		void HandleControllerInputError();
		
		// Check if this player is active, and if so, initialize all player data.
		void InitializeCoopPlayer();

		// Register player for co-op script events.
		void RegisterEvents();

		// Request state change for sub-managers (EM, MM, PAM, TM).
		void RequestSubManagerStateChange(ManagerState&& a_newState);

		// Debug option to reset P1's state via resurrection and re-equipping hand forms.
		void ResetPlayer1();
		
		// Revert to saved pre-transformation race.
		bool RevertTransformation();

		// Queue task to send animation event.
		// Run from task/detached threads or secondary game threads.
		void SendAnimEventSynced(RE::BSFixedString a_animEvent);

		// Set this player as downed and reset all downed player state
		// in preparation for updating the player until they are revived or the co-op session ends.
		void SetAsDowned();

		// Set up player for co-op by changing various actorbase data and form flags.
		void SetCoopPlayerFlags();

		// Checks if a fader menu is opened following player activation of a 
		// refr with teleport extra data. Teleport the co-op companion player
		// to P1 when P1 is repositioned to the teleport endpoint.
		// Run in self-pause and self-resume funcs.
		bool ShouldTeleportToP1(bool&& a_selfPauseCheck);
		
		// Make sure all co-op companions have the same factions as P1.
		void SyncPlayerFactions();

		// Unregister player for co-op script events.
		void UnregisterEvents();
		
		// Refresh all player data for this player, post-initialization.
		void UpdateCoopPlayer
		(
			int32_t a_controllerID, RE::Actor* a_coopActor, uint32_t a_packageFormListStartIndex
		);

		// Update gender, animations, skin tone, headparts, 
		// and refresh the player actor's 3D model when done.
		void UpdateGenderAndBody(bool a_setFemale, bool a_setOppositeGenderAnims);

		// Update player crosshair text, actor state, and more when downed.
		// NOTE:
		// Run in the Main() hook.
		void UpdateWhenDowned();

		//
		// Tasks
		//
		
		// NOTE: The delayed funcs below are all executed on the player's task runner.

		// Run when the player is downed and until revived or dead.
		// Keep track of downed player revive time and remaining life time, and update
		// the crosshair text to show how much of each remains.
		// Once revived, handle resetting the player so that they can continue co-op,
		// and if not revived, signal that the player died and end the co-op session.
		void DownedStateCountdownTask();

		// Emulate P1 menu controls if a co-op player is controlling the Lockpicking Menu.
		// NOTE: Currently run as a task to avoid a bug 
		// with repeated attempts to lockpick the same refr.
		// May move back to MIM if a solution for this bug is found.
		void LockpickingTask();

		// Attempt to (dis)mount the targeted refr.
		void MountTask();

		// Debug option to refresh all managers for this player.
		void RefreshPlayerManagersTask();

		// Debug option to reset a co-op companion player.
		// Stops the player, 
		// resets any active transformation, 
		// re-equips saved gear/unequips all gear,
		// resurrects the actor, 
		// disables and re-enables them, 
		// re-attaches havok, 
		// resets temporary flags,
		// and finally re-enables movement.
		void ResetCompanionPlayerStateTask(const bool& a_unequipAll, const bool& a_reattachHavok);

		// Perform a shout.
		// TODO: Cast different shout variations and adjust charge and release animation lengths 
		// dependent on how long the player has held the shout bind.
		// Currently casts the highest known shout variation spell every time.
		void ShoutTask();

		// Teleport to another player through an entry and exit portal.
		void TeleportTask(RE::ActorHandle a_targetHandle);
		
		// For co-op companions, toggle levitation state while transformed into a Vampire Lord.
		void ToggleVampireLordLevitationTask();

		//
		// Members
		//

		// NOTE: Time points are reset by the specific sub-manager they are linked to.
		// Time at which the player's crosshair was last active:
		// Object selected, selected object changed, crosshair moved, 
		// or crosshair hit the edge of the screen.
		SteadyClock::time_point crosshairLastActiveTP;
		// Time point indicating when LOS to the current crosshair target was last checked.
		SteadyClock::time_point crosshairRefrVisibilityCheckTP;
		// Time point indicating when LOS to the current crosshair target was lost.
		SteadyClock::time_point crosshairRefrVisibilityLostTP;
		// Time at which the co-op player's stamina was last updated while sprinting.
		SteadyClock::time_point expendSprintStaminaTP;
		// Time at which a invalid companion player was last moved to P1.
		SteadyClock::time_point invalidPlayerMovedTP;
		// Time point indicating when the player started jumping.
		SteadyClock::time_point jumpStartTP;
		// Time point indicating when the last activation check was performed.
		SteadyClock::time_point lastActivationCheckTP;
		// Time point indicating when the player last started any attack.
		SteadyClock::time_point lastAttackStartTP;
		// Time point indicating when the last grabbed refr was auto-grabbed.
		SteadyClock::time_point lastAutoGrabTP;
		// Time point indicating when the player last successfully cast a bound weapon spell 
		// (companion players only).
		SteadyClock::time_point lastBoundWeaponLHReqTP;
		SteadyClock::time_point lastBoundWeaponRHReqTP;
		// Time point indicating when the player's crosshair was last updated.
		SteadyClock::time_point lastCrosshairUpdateTP;
		// Time point indicating when favorited items/emote idles were last cycled to equip/play.
		// NOTE: All cycling binds share a cycling time point, so configure player binds such that
		// only one cycling action can be performed at once.
		SteadyClock::time_point lastCyclingTP;
		// Time at which the player last started dash dodging.
		SteadyClock::time_point lastDashDodgeTP;
		// Time at which the player was last downed.
		SteadyClock::time_point lastDownedTP;
		// Time at which the player last started to get up after being revived.
		SteadyClock::time_point lastGetupAfterReviveTP;
		// Time at which the player last fully got up after being ragdolled.
		SteadyClock::time_point lastGetupTP;
		// Time point indicating when the player was last hidden 
		// within the stealth radius of a hostile actor.
		SteadyClock::time_point lastHiddenInStealthRadiusTP;
		// Time point indicating when the player's input actions were last blocked.
		SteadyClock::time_point lastInputActionBlockTP;
		// Time point indicating when the last killmove check was made.
		SteadyClock::time_point lastKillmoveCheckTP;
		// Time point indicating when the last LH spell cast started (companion players only).
		SteadyClock::time_point lastLHCastStartTP;
		// Time point indicating when the player last attempted to start moving.
		SteadyClock::time_point lastMovementStartReqTP;
		// Time point indicating when the player last attempted to stop moving.
		SteadyClock::time_point lastMovementStopReqTP;
		// Time point indicating when the player's paragliding state changed.
		SteadyClock::time_point lastParaglidingStateChangeTP;
		// Time point indicating when a non-concentration quick slot spell was cast.
		SteadyClock::time_point lastQSSCastStartTP;
		// Time point indicating when the last time this player's health was updated 
		// while reviving another player.
		SteadyClock::time_point lastReviveCheckTP;
		// Time point indicating when the last RH spell cast started (companion players only).
		SteadyClock::time_point lastRHCastStartTP;
		// Time point indicating when the last staff LH cast started (companion players only).
		SteadyClock::time_point lastStaffLHCastTP;
		// Time point indicating when the last staff RH cast started (companion players only).
		SteadyClock::time_point lastStaffRHCastTP;
		// Time point indicating when the player's stamina was last checked while in cooldown.
		SteadyClock::time_point lastStaminaCooldownCheckTP;
		// Time point indicating when the player's stealth state was last checked.
		SteadyClock::time_point lastStealthStateCheckTP;
		// Time point indicating when the player last ran out of stamina (companion players only).
		SteadyClock::time_point outOfStaminaTP;
		// Time point indicating when the player last started shouting.
		SteadyClock::time_point shoutStartTP;
		// Time point indicating when the player last transformed into a werewolf/vampire 
		// or other race via spell cast.
		SteadyClock::time_point transformationTP;

		// Task interface for queueing tasks.
		const SKSE::TaskInterface* taskInterface;
		// Registration for player dismissal script event.
		SKSE::Impl::RegistrationSet<void, RE::TESForm*, int32_t> onCoopEndReg =
			SKSE::Impl::RegistrationSet<std::enable_if_t<std::conjunction_v<
			RE::BSScript::is_return_convertible<RE::TESForm*>,
			RE::BSScript::is_return_convertible<int32_t>>>, RE::TESForm*, int32_t>("OnCoopEnd"sv);
		// Sub-managers.
		std::unique_ptr<EquipManager> em;
		std::unique_ptr<MovementManager> mm;
		std::unique_ptr<PlayerActionManager> pam;
		std::unique_ptr<TargetingManager> tm;
		// Detached thread running async tasks.
		std::unique_ptr<TaskRunner> taskRunner;

		// Player's character.
		RE::ActorPtr coopActor;
		// Currently ridden mount.
		RE::ActorHandle currentMountHandle;
		// Actor the player wishes to mount.
		RE::ActorHandle targetedMountHandle;
		// Last processable (used by AV cost manager) animation event's tag.
		RE::BSFixedString lastAnimEventTag;
		// Keyword used to link and set a targetable refr 
		// when running the co-op companion player's ranged attack package.
		RE::BGSKeyword* aimTargetKeyword;
		// Saved player race prior to transforming.
		RE::TESRace* preTransformationRace;
		// Has this manager handled the present controller input error yet?
		bool handledControllerInputError;
		// Has this player been dismissed (DismissPlayer() called) 
		// during the current co-op session?
		// Set to true BEFORE the current co-op session ends 
		// (before session cleanup is finished for all players).
		bool hasBeenDismissed;
		// Has this player been initialized and assigned an active controller 
		// for the current/next co-op session?
		bool isActive;
		// Is another player transferring their health to this downed player?
		bool isBeingRevived;
		// Is this player downed?
		// Separate from not revived: 
		// [true] when health hits or goes below 0.
		// [false] when revived and standing up.
		bool isDowned;
		// Is the player getting up after being downed?
		bool isGettingUpAfterRevive;
		// Is this player in god mode?
		bool isInGodMode;
		// Is this player P1?
		bool isPlayer1;
		// Is this player revived?
		// Separate state from not downed: 
		// [true] when a reviving player transfers the full revive health amount
		// or if the player is not downed.
		// [false] if the player is downed and the full revive health amount 
		// is not set by a reviving player.
		bool isRevived;
		// Is this player reviving another player?
		bool isRevivingPlayer;
		// Is the player toggling their levitation state while in Vampire Lord form?
		bool isTogglingLevitationState;
		// Is the levitation toggle task running? Used to prevent more than one such task 
		// from being queued at a time.
		bool isTogglingLevitationStateTaskRunning;
		// Has this player transformed via spell cast (i.e. now a member of a non-playable race)?
		bool isTransformed;
		// Is this player transforming post-spell cast (not the transformation race yet)?
		bool isTransforming;
		// Is the player actor valid this frame or was it invalid previously?
		bool selfValid;
		bool selfWasInvalid;
		// Should this player teleport to P1?
		bool shouldTeleportToP1;
		// Health level once this downed player is fully revived.
		float fullReviveHealth;
		// How much health a reviver player has given to a downed player so far 
		// while reviving them.
		float revivedHealth;
		// How many seconds this player has been downed for since their downed countdown began.
		float secsDowned;
		// Maximum amount of seconds this player can remain transformed.
		float secsMaxTransformationTime;
		// Seconds since the player was invalid and moved to P1.
		float secsSinceInvalidPlayerMoved;
		// Controller ID (CID = XInput index) for this player.
		int32_t controllerID;
		// Player 1 always has an ID of 0. 
		// Other players have their IDs assigned based on their CID,
		// incrementing by 1 for each player with a higher CID.
		int32_t playerID;
		// Index in global packages list at which this player's first package is found.
		uint32_t packageFormListStartIndex;
	};
}

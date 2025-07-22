#pragma once
#include <Enums.h>
#include <Player.h>
#include <PlayerActionInfoHolder.h>

namespace ALYSLC 
{
	using SteadyClock = std::chrono::steady_clock;
	class CoopPlayer;

	// Contains actor value expenditure info about a requested player action 
	// that has already started.
	// Marked as complete once the appropriate animation event has fired 
	// and the associated actor value is modified. 
	// Actions progress from requested, to in-progress, and then completed 
	// (removed from in-progress action set).
	// Examples: power attacks, bashes, sprinting, and spell casting.
	struct AVCostActionManager
	{
		AVCostActionManager()
		{
			Clear();
		}

		AVCostActionManager& operator=(const AVCostActionManager& a_avcam) 
		{
			perfAnimEventsQueue = a_avcam.perfAnimEventsQueue;
			reqActionsSet = a_avcam.reqActionsSet;
			avCostsMap = a_avcam.avCostsMap;
			actionsInProgress = a_avcam.actionsInProgress;

			return *this;
		}
		AVCostActionManager& operator=(AVCostActionManager&& a_avcam)
		{
			perfAnimEventsQueue = std::move(a_avcam.perfAnimEventsQueue);
			reqActionsSet = std::move(a_avcam.reqActionsSet);
			avCostsMap = std::move(a_avcam.avCostsMap);
			actionsInProgress = std::move(a_avcam.actionsInProgress);

			return *this;
		}

		inline void Clear() 
		{
			// Clear all data.
			avCostsMap.clear();
			reqActionsSet = std::set<AVCostAction>();
			actionsInProgress = AVCostAction::kNone;

			{
				std::unique_lock<std::mutex> perfAnimQueueLock
				(
					perfAnimQueueMutex, std::try_to_lock
				);
				if (perfAnimQueueLock)
				{
					SPDLOG_DEBUG("[PAM] AVCostManager: Clear: Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					while (!perfAnimEventsQueue.empty())
					{
						perfAnimEventsQueue.pop();
					}
				}
				else
				{
					SPDLOG_DEBUG("[PAM] AVCostManager: Clear: Failed to obtain lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
				}
			}
		}

		// Get actor value cost for this action once it's performed.
		// Return the cached cost, if any, or nullopt otherwise.
		inline std::optional<float> GetCost(const AVCostAction& a_action) 
		{
			if (const auto iter = avCostsMap.find(a_action); iter != avCostsMap.end())
			{
				return iter->second;
			}

			return std::nullopt;
		}

		// Insert a new AV cost action request.
		inline void InsertRequestedAction(AVCostAction a_action)
		{
			reqActionsSet.insert(a_action);
		}
		
		// Remove an AV cost action request.
		// Return true if any action request was removed.
		inline bool RemoveRequestedAction(AVCostAction a_action)
		{
			if (const auto iter = reqActionsSet.find(a_action); iter != reqActionsSet.end())
			{
				reqActionsSet.erase(iter);
				return true;
			}

			return false;
		}

		// Remove all AV cost action requests.
		// Return true if at least one of the given action requests were removed.
		template <class... Args>
		inline bool RemoveRequestedActions(Args... a_actions)
			requires(std::same_as<Args, AVCostAction>&&...)
		{
			return (RemoveRequestedAction(a_actions) | ...);
		}
		
		// Remove a started AV cost action.
		// Return true if any started action was removed.
		inline bool RemoveStartedAction(AVCostAction a_action)
		{
			if (actionsInProgress.any(a_action))
			{
				actionsInProgress.reset(a_action);
				return true;
			}

			return false;
		}

		// Remove all started AV cost actions.
		// Return true if at least one of the given started actions were removed.
		template <class... Args>
		inline bool RemoveStartedActions(Args... a_actions)
			requires(std::same_as<Args, AVCostAction>&&...)
		{
			return (RemoveStartedAction(a_actions) | ...);
		}

		// Set actor value cost for this action.
		inline void SetCost(AVCostAction a_action, float a_cost)
		{
			avCostsMap.insert_or_assign(a_action, a_cost);
		}

		// Add AV cost action to in-progress set if it was previously requested.
		// Return true if an action was added as started.
		inline bool SetStartedAction(AVCostAction a_action)
		{
			if (reqActionsSet.contains(a_action))
			{
				actionsInProgress.set(a_action);
				return true;
			}

			return false;
		}

		// Set multiple actions as started.
		// Return true if at least one of the given actions was added as started.
		template <class... Args>
		inline void SetStartedActions(Args... a_actions)
			requires(std::same_as<Args, AVCostAction>&&...)
		{
			return (SetStartedAction(a_actions) | ...);
		}

		// Mutex for accessing/modifying the performed animation events queue.
		std::mutex perfAnimQueueMutex;
		// Queue holding pairs of performed animation events and their IDs,
		// which help distinguish between instances of the same anim event 
		// being performed in quick succession.
		std::queue<std::pair<PerfAnimEventTag, uint16_t>> perfAnimEventsQueue;
		// Set holding requested AV cost actions.
		// Requested actions get removed once completed.
		std::set<AVCostAction> reqActionsSet;
		// Maps AV cost actions to their AV cost.
		std::unordered_map<AVCostAction, float> avCostsMap;
		// Shows which AV cost actions are in progress.
		// Using an enumeration since only one of each type can be performed at once.
		SKSE::stl::enumeration<AVCostAction, std::uint16_t> actionsInProgress;
	};

	struct PlayerActionManager : public Manager
	{
		// List of composing input actions for each player action.
		using ComposingInputActionList = PlayerActionInfoHolder::ComposingInputActionList;
		// List of sets of conflicting player actions 
		// that should be blocked/interrupted for each player action.
		// Indexed by player action index.
		using PAConflictSetsList = 
		std::array<std::set<InputAction>, (size_t)(InputAction::kActionTotal)>;
		// Parameters related to triggering a specific action.
		using PAParams = PlayerActionInfoHolder::PlayerActionParams;
		// List of player actions and their trigger parameters.
		// Indexed with player action index.
		using PAParamsList = PlayerActionInfoHolder::PAParamsList;
		// Flags representing press conditions to trigger a player action.
		using TriggerFlags = PlayerActionInfoHolder::TriggerFlags;

		// Holds the state and bind information for all of this player's player actions.
		struct PlayerActionState
		{
			PlayerActionState();
			PlayerActionState(const PAParams& a_params);
			~PlayerActionState() = default;

			// Player action trigger-related info.
			PAParams paParams;
			// Current action perform stage.
			PerfStage perfStage;
			// Time point indicating when all of the action's inputs were last pressed.
			// Action not started yet.
			SteadyClock::time_point pressTP;
			// Time point indicating when some of the action's inputs were last released.
			// Action may not have started beforehand.
			SteadyClock::time_point releaseTP;
			// Time point indicating when the action last started.
			SteadyClock::time_point startTP;
			// Time point indicating when some/all 
			// of a previously started action's inputs were released.
			SteadyClock::time_point stopTP;
			// Actor value base cost (pre-player cost modifier) per second.
			float avBaseCost;
			// Note to self: Think of better names. Please.
			// How many seconds this action has been performed for.
			float secsPerformed;
			// Action priority when checking for conflicts.
			// High priority actions are checked first
			// and can block other conflicting lower-priority actions.
			uint32_t priority;
		};

		// List of player action states stored by player action index.
		using PAStatesList = std::array<PlayerActionState, (size_t)(InputAction::kActionTotal)>;

		PlayerActionManager();
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

		//
		// PA state-related funcs
		//

		// All buttons pressed for all of the given actions.
		template <class... Args>
		inline bool AllButtonsPressedForAllActions(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (AllButtonsPressedForAction(a_actions) && ...);
		}

		// All buttons pressed for at least one of the given actions.
		template <class... Args>
		inline bool AllButtonsPressedForAtLeastOneAction(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (AllButtonsPressedForAction(a_actions) || ...);
		}

		// All inputs pressed for all of the given actions.
		template <class... Args>
		inline bool AllInputsPressedForAllActions(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (AllInputsPressedForAction(a_actions) && ...);
		}

		// All inputs pressed for at least one of the given actions.
		template <class... Args>
		inline bool AllInputsPressedForAtLeastOneAction(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (AllInputsPressedForAction(a_actions) || ...);
		}

		// Are all inputs pressed for this action?
		inline const bool AllPressed(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kInputsPressed
			);
		}

		// Are all inputs are released for this action?
		inline const bool AllReleased(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kInputsReleased
			);
		}

		// Conditions failed after this action started.
		inline const bool ConditionsFailed(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kFailedConditions
			);
		}

		// Seconds since all inputs for the given action were pressed or some inputs were released.
		inline const float GetSecondsSinceLastInputStateChange
		(
			const InputAction& a_action, 
			bool&& a_lastPressed
		) 
		{
			if (a_lastPressed) 
			{
				return 
				(
					Util::GetElapsedSeconds
					(
						paStatesList[!a_action - !InputAction::kFirstAction].pressTP, true
					)
				);
			}
			else
			{
				return 
				(
					Util::GetElapsedSeconds
					(
						paStatesList[!a_action - !InputAction::kFirstAction].releaseTP, true
					)
				);
			}
		}

		// Seconds since this action was started.
		inline const float GetSecondsSinceLastStart(const InputAction& a_action) 
		{
			return 
			(
				Util::GetElapsedSeconds
				(
					paStatesList[!a_action - !InputAction::kFirstAction].startTP
				)
			); 
		}

		// Seconds since some/all of this action's inputs were released 
		// after starting the action previously.
		inline const float GetSecondsSinceLastStop(const InputAction& a_action)
		{
			return 
			(
				Util::GetElapsedSeconds
				(
					paStatesList[!a_action - !InputAction::kFirstAction].stopTP
				)
			);
		}

		// Check if this action was interrupted by another action.
		inline const bool InterruptedByConflictingPA(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kConflictingAction
			);
		}

		// Is this action currently blocked from being performed?
		inline const bool IsBlocked(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kBlocked
			);
		}

		// Is this action interrupted: stopped by conflicting action or failed conditions?
		inline const bool IsInterrupted(const InputAction& a_action)
		{
			return 
			(
				(
					paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
					PerfStage::kConflictingAction
				) ||
				(
					paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
					PerfStage::kFailedConditions
				)
			);
		}

		// Is this action not being performed (any state except start)?
		inline const bool IsNotPerforming(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage != 
				PerfStage::kStarted
			);
		}

		// Is not performing any of the given actions.
		template <class... Args>
		inline bool IsNotPerformingAnyOf(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (IsNotPerforming(a_actions) && ...);
		}

		// Is not performing at least one of the given actions.
		template <class... Args>
		inline bool IsNotPerformingOneOf(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (IsNotPerforming(a_actions) || ...);
		}

		// Is this action being performed (action has started)?
		inline const bool IsPerforming(const InputAction& a_action) 
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
				PerfStage::kStarted
			);
		}

		// Is performing all of the given actions.
		template <class... Args>
		inline bool IsPerformingAllOf(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (IsPerforming(a_actions) && ...);
		}

		// Is performing at least one of the given actions.
		template <class... Args>
		inline bool IsPerformingOneOf(Args... a_actions)
			requires(std::same_as<Args, InputAction>&&...)
		{
			return (IsPerforming(a_actions) || ...);
		}

		// Is the player casting their quick slot spell?
		// Since some spellcasts launch their projectiles on bind release,
		// also return true if the cast bind's inputs were just released.
		bool IsPerformingQuickSlotCast()
		{
			return 
			(
				(
					IsPerforming(InputAction::kQuickSlotCast) ||
					GetPlayerActionInputJustReleased(InputAction::kQuickSlotCast, false)
				) ||
				(
					(reqSpecialAction == SpecialActionType::kQuickCast) &&
					(
						IsPerforming(InputAction::kSpecialAction) ||
						GetPlayerActionInputJustReleased(InputAction::kSpecialAction, false)
					)
				)
			);
		}

		// Is the player performing a spell cast action?
		// Since some spellcasts launch their projectiles on bind release,
		// also return true if the cast bind's inputs were just released.
		bool IsPerformingSpellCastAction()
		{
			return 
			(
				(

					IsPerformingOneOf
					(
						InputAction::kCastLH, InputAction::kCastRH, InputAction::kQuickSlotCast
					) ||
					GetPlayerActionInputJustReleased(InputAction::kCastLH, false) ||
					GetPlayerActionInputJustReleased(InputAction::kCastRH, false) ||
					GetPlayerActionInputJustReleased(InputAction::kQuickSlotCast, false)

				) ||
				(
					(
						reqSpecialAction == SpecialActionType::kCastBothHands ||
						reqSpecialAction == SpecialActionType::kDualCast ||
						reqSpecialAction == SpecialActionType::kQuickCast
					) &&
					(
						IsPerforming(InputAction::kSpecialAction) ||
						GetPlayerActionInputJustReleased(InputAction::kSpecialAction, false)
					)
				)
			);
		}

		// Is the player rotating their arms?
		bool IsRotatingArms() 
		{
			return IsPerformingOneOf
			(
				InputAction::kRotateRightForearm,
				InputAction::kRotateRightHand,
				InputAction::kRotateRightShoulder,
				InputAction::kRotateLeftForearm,
				InputAction::kRotateLeftHand,
				InputAction::kRotateLeftShoulder
			);
		}

		// Did this action just start?
		inline const bool JustStarted(const InputAction& a_action) 
		{
			return 
			(
				(
					paStatesList[!a_action - !InputAction::kFirstAction].perfStage == 
					PerfStage::kStarted 
				) &&
				(paStatesList[!a_action - !InputAction::kFirstAction].secsPerformed == 0.0f)
			);
		}

		// Were some of this action's inputs released?
		inline const bool SomeReleased(const InputAction& a_action)
		{
			return 
			(
				paStatesList[!a_action - !InputAction::kFirstAction].perfStage ==
				PerfStage::kSomeInputsReleased
			);
		}

		//
		// Other
		//

		// Get negative of the spell's base cost.
		inline float GetSpellDeltaMagickaCost(RE::SpellItem* a_spell)
		{
			if (!a_spell)
			{
				return 0.0f;
			}

			return -a_spell->CalculateMagickaCost(coopActor.get());
		}

		
		
		// Modify the current value of the given actor value by the given amount.
		inline void ModifyAV(const RE::ActorValue& a_av, float a_deltaAmount)
		{
			if (auto avValueOwner = coopActor->As<RE::ActorValueOwner>(); avValueOwner)
			{
				avValueOwner->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage, a_av, a_deltaAmount
				);
			}
		}

		//
		// Member funcs
		//

		// Check for an associated AV cost action for the give player action,
		// and if one hasn't been queued already, add a request for the resolved AV cost action.
		void AddAVCostActionRequest(const InputAction& a_action);

		// Are all of this action's buttons pressed? 
		// Ignores analog stick movement.
		bool AllButtonsPressedForAction(const InputAction& a_action) noexcept;

		// Are all of this action's buttons pressed and analog sticks moved?
		bool AllInputsPressedForAction(const InputAction& a_action) noexcept;

		// Block currently pressed player actions from being performed 
		// by setting their perform stage to 'Blocked',
		// and optionally update the block TP, if requested, 
		// to allow for blocking over an interval.
		void BlockCurrentInputActions(bool a_startBlockInterval = false);

		// Cast the associated rune projectile from the given spell 
		// at the world position the player is targeting.
		// The game's magic casting package procedure fails to cast rune spells,
		// so we have to do it directly here through the player's magic caster(s).
		void CastRuneProjectile(RE::SpellItem* a_spell);

		// Start/stop casting a spell equipped at the given equip index, 
		// using the associated magic caster at that index.
		// Can also wait until the casting animation fires before casting 
		// or cast the spell with P1's magic caster.
		void CastSpellWithMagicCaster
		(
			const EquipIndex& a_index, 
			const bool& a_justStarted,
			bool&& a_startCast, 
			bool&& a_waitForCastingAnim, 
			const bool& a_shouldCastWithP1
		);

		// Cast/stop casting with the given staff equipped in the given hand.
		// NOTE: 
		// Haven't figured out how to trigger staff casting animations for companion players yet.
		// The magic casting package procedure once again fails to execute.
		void CastStaffSpell
		(
			RE::TESObjectWEAP* a_staff, const bool& a_isLeftHand, bool&& a_shouldCast
		);

		// Chain all queued P1 input events and send.
		void ChainAndSendP1ButtonEvents();

		// Level up the co-op player's skill(s) through advancing the same skill(s) for P1.
		void CheckForCompanionPlayerLevelUps();

		// Reset ranged attack package cast data if the player has released a FNF spell
		// and the package is still executing.
		void CheckForDelayedCastCompletion();

		// Update combat skills data when the player starts/stops performing a combat skill.
		void CheckIfPerformingCombatSkills();

		// Copy the highest shared skill level among all players 
		// to this player for all shared skills.
		void CopyOverSharedSkillAVs();

		// Evaluate the current package atop the player's package stack.
		void EvaluatePackage();

		// Expend stamina when a stamina AV cost action is in progress or when sprinting.
		void ExpendStamina();

		// Directly expend stamina equal to the given cost.
		void ExpendStamina(const float& a_cost);

		// Flash the shout meter around the compass.
		void FlashShoutMeter();

		// Returns an integer that represents the priority of the given player action
		// when multiple actions are being evaluated for conflicts before starting execution.
		// Larger integers indicate higher priority.
		const int32_t GetActionPriority(const InputAction& a_action) const noexcept;

		// Get the co-op package corresponding to the given index.
		RE::TESPackage* GetCoopPackage(const PackageIndex& a_index);

		// Get current package atop the player's package stack.
		RE::TESPackage* GetCurrentPackage();

		// Get the player's default package to evaluate.
		RE::TESPackage* GetDefaultPackage();

		// Get how long this player action's inputs have been held for in seconds.
		// NOTE:
		// Does not require that the action is being performed.
		float GetPlayerActionInputHoldTime(const InputAction& a_action);

		// Return true if all inputs for the given action were just pressed
		// and the action is now performable (check button ordering).
		bool GetPlayerActionInputJustPressed(const InputAction& a_action);
		
		// Return true if one or all (if requested) inputs for the given action were just released
		// (order in which they were released does not matter).
		bool GetPlayerActionInputJustReleased
		(
			const InputAction& a_action, bool&& a_checkIfAllReleased
		);
		
		// For companion players, check if XP should be awarded 
		// when a magicka AV cost action is in progress or completes.
		void GrantSpellcastXP();

		// Update the player's dialogue state, have the player and speaker look at one another,
		// and close the DialogueMenu if the player is too far from the speaker.
		void HandleDialogue();

		// Handle killmove requests by ensuring the killmove victim dies after the killmove ends.
		// Sounds kind of dark, but that's what it does. Most of the time.
		void HandleKillmoveRequests();
		
		// Modify health, magicka, stamina actor values and grant XP
		// based on the player's ongoing AV action requests
		// and animation events that have triggered over the last frame.
		void HandlePerformedAnimationEvents();

		// Return true if the given player actor has enough magicka to cast the given spell.
		// Accounts for player-specific magicka cost multiplier.
		bool HasEnoughMagickaToCast(RE::MagicItem* a_spell);

		// Level up the skill taught by this book.
		void LevelUpSkillWithBook(RE::TESObjectBOOK* a_book);

		// Returns true if none of the action's inputs are pressed (not including analog sticks).
		bool NoButtonsPressedForAction(const InputAction& a_action);

		// Returns true if none of the action's inputs are pressed.
		bool NoInputsPressedForAction(const InputAction& a_action);

		// Returns true if the given action's inputs were double tapped
		// within the preset consecutive taps interval.
		bool PassesConsecTapsCheck(const InputAction& a_action);

		// Returns true if the given action's inputs are pressed in the correct order
		// and/or held for long enough to trigger the action.
		bool PassesInputPressCheck(const InputAction& a_action);

		// Queue a P1 input event to send based on 
		// the given player action, input device, and press type.
		// Can also set the held time for the input, 
		// and toggle AI driven to false before sending the event 
		// and back to true after handling the event.
		void QueueP1ButtonEvent
		(
			const InputAction& a_inputAction, 
			RE::INPUT_DEVICE&& a_inputDevice, 
			ButtonEventPressType&& a_buttonStateToTrigger, 
			float&& a_heldDownSecs = 0.0f, 
			bool&& a_toggleAIDriven = true
		) noexcept;

		// Draw or sheathe the player's weapons/magic.
		// If 'ignore state' is true, force the player to draw/sheathe 
		// regardless of their current weapon state.
		// Otherwise, only draw when fully sheathed and only sheathe when fully drawn.
		void ReadyWeapon(const bool& a_shouldDraw, bool&& a_ignoreState = true);

		// Reset killmove data for this player and the killmove victim player, if any.
		void ResetAllKillmoveData(const int32_t& a_targetPlayerIndex);

		// Reset attack speed and damage multiplier actor values
		// and clear out the cached requested damage multiplier.
		void ResetAttackDamageMult();

		// Stop ongoing killmove idle and reset player killmove victim data, 
		// if the victim is a player.
		void ResetKillmoveVictimData(const int32_t& a_targetPlayerIndex);

		// Stop all casting for this player by evaluating the current package twice,
		// one with all casting globals set and one with all of them reset.
		// Helps prevent movement bugs or instances where the player is stuck casting without
		// pressing any casting binds.
		void ResetPackageCastingState();

		// Reset all time points to the current time.
		void ResetTPs();

		// Restore the given actor value to its full, maximum value (base + temp modifier).
		void RestoreAVToMaxValue(RE::ActorValue a_av);

		// Revive the targeted downed player.
		// Remove and transfer health from the reviving player 
		// until the downed player is fully revived.
		void RevivePlayer();

		// Instantly send a P1 button event directly without queuing/chaining
		// based on the given input action, device, and press type.
		// Can also set the held time for the input, 
		// and toggle AI driven to false before sending the event 
		// and back to true after handling the event.
		void SendButtonEvent
		(
			const InputAction& a_inputAction, 
			RE::INPUT_DEVICE&& a_inputDevice, 
			ButtonEventPressType&& a_buttonStateToTrigger,
			const float& a_heldDownSecs = 0.0f,
			bool&& a_toggleAIDriven = true
		) noexcept;

		// Set the player's attack damage mult actor values for sneak attacks.
		// Is applied when hit connects and deals damage in the HandleHealthDamage() hooks.
		void SetAttackDamageMult();
		
		// Set the current package atop the player's package stack to the given package,
		// or the default package if none is provided,
		// and evaluate the new package if it differs from the old one or regardless.
		void SetAndEveluatePackage
		(
			RE::TESPackage* a_package = nullptr, bool a_evaluateOnlyIfDifferent = false
		);

		// Set the package atop the player's package stack to the given package.
		void SetCurrentPackage(RE::TESPackage* a_package);

		// Set the player as essential if using the revive system.
		void SetEssentialForReviveSystem();

		// Set/unset the given package flag for all of a companion player's packages.
		void SetPackageFlag(RE::PACKAGE_DATA::GeneralFlag&& a_flag, bool&& a_set);

		// Stop the player from casting hand spells instantly.
		void StopCastingHandSpells();

		// Stop combat between normally neutral actors, 
		// player allies/followers/summons, and co-op players.
		void StopCombatWithFriendlyActors();

		// Stop any ongoing idle animation or attack for this player.
		void StopCurrentIdle();

		// Return true if the player should turn to face a target 
		// if attacking, bashing, blocking, casting, or shouting.
		bool TurnToTargetForCombatAction();
		
		// Return true if the player should turn to face a target 
		// if attacking, bashing, blocking, casting, or shouting.
		// Also return whether or not a combat action has just started in the outparam.
		bool TurnToTargetForCombatAction(bool& a_combatActionJustStarted);

		// Update current HMS actor values, HMS regen multipliers, 
		// stamina/shout cooldowns, and the player's carryweight actor value.
		void UpdateAVsAndCooldowns();

		// Update bound weapon lifetime data.
		// Unequip bound weapon(s) when their lifetime expires and reset cached bound weapon data.
		void UpdateBoundWeaponTimers();

		// Update animation graph variables and other variables dependent on them.
		void UpdateGraphVariableStates();

		// Set the hand the player last used to attack/cast with,
		// depending on what player action is being performed.
		void UpdateLastAttackingHand();

		// Update the player action binds and associated data for this player.
		void UpdatePlayerBinds();

		// Update the player's stamina regeneration cooldown interval.
		void UpdateStaminaCooldown();

		// Keep tabs on the player's transformation 
		// and revert their form once the transformation timer expires.
		// Also check if the player is transformed or has reverted their form,
		// and (un)equip items unique to the player's transformation 
		// (ex. vampiric drain spell or werewolf howl).
		void UpdateTransformationState();

		//
		// Members
		//
		 
		// Co-op player.
		std::shared_ptr<CoopPlayer> p;
		// Downed player targeted for revive.
		std::shared_ptr<CoopPlayer> downedPlayerTarget;
		// Co-op actor.
		RE::ActorPtr coopActor;
		// Handle for the player actor killmove-ing this player.
		RE::ActorHandle killerPlayerActorHandle;
		// Handle for the actor being targeted by the player's killmove request.
		RE::ActorHandle killmoveTargetActorHandle;
		// P1 ref alias registered for script events.
		RE::BGSRefAlias* player1RefAlias;
		// Is the player using a staff in their LH or RH?
		RE::TESGlobal* usingLHStaff;
		RE::TESGlobal* usingRHStaff;
		// Any performed combat skills that give XP.
		SKSE::stl::enumeration<SkillIncCombatActionType> perfSkillIncCombatActions;
		// Casting source globals used with companion player's ranged attack package 
		// to cast spells.
		std::array<RE::TESGlobal*, (size_t)CastingGlobIndex::kTotal> castingGlobVars;
		// Occurring player actions. 
		// By the end of the main task, 
		// only actions with the 'Started' stage remain in this list.
		std::forward_list<InputAction> occurringPAs;
		// Handles AV cost action requests and AV expenditures associated with them.
		std::unique_ptr<AVCostActionManager> avcam;
		// Map holding formlists/stacks of co-op player packages.
		std::unordered_map<PackageIndex, RE::BGSListForm*> packageStackMap;
		// Holds any queued P1 input events to send this frame.
		std::vector<std::unique_ptr<RE::InputEvent* const>> queuedP1ButtonEvents;

		// Last attack's associated hand.
		HandIndex lastAttackingHand;
		// All actions that can be interrupted by each action when there's a conflict.
		PAConflictSetsList paConflictSetsList;
		// This player's player action parameters.
		PAParamsList paParamsList;
		// Holds action state information for all player actions.
		PAStatesList paStatesList;
		// Special action type requested to be performed next.
		SpecialActionType reqSpecialAction;

		//
		// Health
		// 
		
		// Base actor value.
		float baseHealth;
		// Base heal rate multiplier actor value.
		float baseHealthRegenRateMult;
		// Current actor value.
		float currentHealth;
		// Full actor value.
		float fullHealth;

		//
		// Magicka.
		//

		// Base actor value.
		float baseMagicka;
		// Base magicka rate multiplier actor value.
		float baseMagickaRegenRateMult;
		// Current actor value.
		float currentMagicka;
		// Full actor value.
		float fullMagicka;
		// How long the player has been casting for with their LH.
		float lhCastDuration;
		// How long the player has been casting for with their RH.
		float rhCastDuration;

		//
		// Stamina.
		//

		// Base actor value.
		float baseStamina;
		// Base stamina rate multiplier actor value.
		float baseStaminaRegenRateMult;
		// Current actor value.
		float currentStamina;
		// Full actor value.
		float fullStamina;

		// Requested damage multiplier for all sources of damage.
		float reqDamageMult;

		// Time intervals for routine updates.
		// Bound weapon equip durations from base effects.
		// Note: Will not be modified by cost-reduction, as this is the base duration.
		float secsBoundWeapon2HDuration;
		float secsBoundWeaponLHDuration;
		float secsBoundWeaponRHDuration;
		// Currently equipped shout's cooldown.
		float secsCurrentShoutCooldown;
		// Seconds since the bound weapon in the LH/RH or in 2H was equipped.
		float secsSinceBoundWeap2HReq;
		float secsSinceBoundWeapLHReq;
		float secsSinceBoundWeapRHReq;
		// Seconds since the last staff cast (LH/RH) with the player's hand caster.
		float secsSinceLastLHStaffCast;
		float secsSinceLastRHStaffCast;
		// Seconds since the player last shouted.
		float secsSinceLastShout;
		// Seconds since last starting to cast a quick slot spell.
		float secsSinceQSSCastStart;
		// Seconds since last revive check while reviving another player.
		float secsSinceReviveCheck;
		// Total number of seconds the player must wait before stamina starts regenerating again.
		float secsTotalStaminaRegenCooldown;

		//
		// Action state
		// 
		
		// Is attacking/casting.
		bool isAttacking;
		// Is performing a bash attack.
		bool isBashing;
		// Is being killmoved by another player and is not dead yet.
		bool isBeingKillmovedByAnotherPlayer;
		// Is blocking with a shield/weapon/torch (from graph var).
		bool isBlocking;
		// NOTE: 
		// Not used right now, at least until dual casting is implemented.
		bool isCastingDual;
		// Is casting with the LH/RH (NOT from graph var, set when casting animation triggers).
		bool isCastingLH;
		bool isCastingRH;
		// Is controlling menus while the game is not paused.
		bool isControllingUnpausedMenu;
		// Is in any casting animation (LH/RH/Dual, from graph var).
		bool isInCastingAnim;
		// Is in a casting animation (LH/RH/Dual, from graph var).
		bool isInCastingAnimDual;
		bool isInCastingAnimLH;
		bool isInCastingAnimRH;
		// Is controlling open DialogueMenu.
		bool isInDialogue;
		// Is currently jumping (from graph var).
		bool isJumping;
		// Is performing a killmove as the aggressor, 
		// with BOTH aggressor and victim still in the killmove state.
		bool isPerformingKillmove;
		// Is playing an emote idle.
		bool isPlayingEmoteIdle;
		// Is power attacking (set when power attack animation triggers). 
		bool isPowerAttacking;
		// Is attacking with a ranged weapon or spell.
		bool isRangedAttack;
		// Is attacking with a ranged weapon (NOT a spell).
		bool isRangedWeaponAttack;
		// Is riding a mount (from graph var).
		bool isRiding;
		// Is performing a silent roll (set when animation triggers).
		bool isRolling;
		// Is shouting according to the player's animation graph variable.
		bool isShouting;
		// Is casting a power or shout from the voice slot (set when performing shout task).
		bool isVoiceCasting;
		// Is in sneak state (from graph var).
		bool isSneaking;
		// Is sprinting (set when sprint animation triggers).
		bool isSprinting;
		// Is attacking with a weapon.
		bool isWeaponAttack;
		// Is trying to perform a melee spellcast killmove,
		// which requires special handling on cleanup.
		bool reqMeleeSpellcastKillmove;
		// Does the player want to sneak?
		bool wantsToSneak;
		// Was the player sprinting during the last frame?
		bool wasSprinting;

		//
		// AV conditionals/misc
		//

		// Has the player's weapon damage mult been set?
		bool attackDamageMultSet;
		// Should close Dialogue Menu since the player is too far away from the speaker.
		bool autoEndDialogue;
		// Should block all input actions while set.
		bool blockAllInputActions;
		// Requested to equip bound weapon in the LH/RH or in 2H.
		// Cleared on unequip.
		bool boundWeapReq2H;
		bool boundWeapReqLH;
		bool boundWeapReqRH;
		// Player can shout (cooldown expired).
		bool canShout;
		// Has the player requested to paraglide while activating?
		bool requestedToParaglide;
		// Are all queued P1 input events that toggle animation driven to false being sent?
		bool sendingP1MotionDrivenEvents;
		// Was hand spellcasting cancelled via the sheathing weapons/magic?
		bool spellcastingCancelled;
		// Has the player started cycling through nearby objects for activation?
		bool startedActivationCycling;
		// Does the player want to drawn their weapons/magic?
		// This information is kept here and used to 
		// prevent the game from sheathing/drawing weapons/magic on its own.
		bool weapMagReadied;

		// Current number of player animation events processed 
		// for this player during the current co-op session.
		uint16_t lastAnimEventID;
		// Bit mask for all pressed buttons/moved analog sticks.
		uint32_t inputBitMask;
		// Player controller ID.
		int32_t controllerID;
		// Player ID.
		int32_t playerID;
	};
}

#include "PlayerActionManager.h"
#include <algorithm>
#include <Compatibility.h>
#include <GlobalCoopData.h>
#include <Settings.h>
#include <Util.h>
#include <Windows.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	using CastingSource = RE::MagicSystem::CastingSource;
	using CastingType = RE::MagicSystem::CastingType;
	using Delivery = RE::MagicSystem::Delivery;

	PlayerActionManager::PlayerActionManager() :
		Manager(ManagerType::kPAM)
	{ }

	void PlayerActionManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p->controllerID > -1 && a_p->controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			logger::debug("[PAM] Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count());
			RefreshData();
			// Reset health, magicka, and stamina.
			RestoreAVToMaxValue(RE::ActorValue::kHealth);
			RestoreAVToMaxValue(RE::ActorValue::kMagicka);
			RestoreAVToMaxValue(RE::ActorValue::kStamina);
		}
		else
		{
			logger::error("[PAM] ERR: Cannot construct Player Action Manager for controller ID {}.", a_p ? a_p->controllerID : -1);
		}
	}

#pragma region MANAGER_FUNCS_IMPL
	void PlayerActionManager::MainTask()
	{
		// Player action holders.
		const auto& paFuncs = glob.paFuncsHolder;
		const auto& paInfo = glob.paInfoHolder;
		// Loop data.
		// All required inputs pressed for current action.
		bool allReqPressed = false;
		// All required inputs released for current action.
		bool allReqReleased = false;
		// Can perform current action if condition checks pass.
		bool canPerfOnCondPass = false;
		// Disable current action if an unpaused menu is open.
		bool blockActionInUnpausedMenu = false;
		// Current action's bit mask contains the LS/RS bits.
		bool maskIncludesAnalogStick = false;
		// Current action passes condition checks.
		bool passesConditions = false;
		// Some of the current action's required inputs are pressed.
		bool someReqPressed = false;
		// Player action bitmask, modified from the current input action bit mask.
		uint32_t paBitMask = 0;

		// Needs to be replaced with something that isn't a heuristic eventually. Will do for now.
		// Sorts candidate player actions by priority.
		auto priorityComp =
			[this, paBitMask](const InputAction& a_left, const InputAction& a_right) {
				float lPriority = GetActionPriority(a_left);
				float rPriority = GetActionPriority(a_right);
				return lPriority < rPriority;
			};
		std::priority_queue<InputAction, std::deque<InputAction>, decltype(priorityComp)> pressedPACandidates(priorityComp);

		// Ensure player is always visible.
		if (coopActor->GetAlpha() != 1.0f)
		{
			coopActor->SetAlpha(1.0f);
		}

		// Supported menu is open and controlled by this player.
		// Since this manager pauses when the game pauses, this main task wouldn't be executed,
		// so the game must be unpaused here.
		isControllingUnpausedMenu = glob.supportedMenuOpen.load() && GlobalCoopData::IsControllingMenus(p->controllerID);
		// Make sure package lists are valid.
		// Seems to clear when going through load doors at times.
		if (packageStackMap.empty() || !packageStackMap.contains(PackageIndex::kDefault) || !packageStackMap.contains(PackageIndex::kCombatOverride))
		{
			logger::error("[PAM] ERR: MainTask: {}: Co-op package stack map is empty ({}), does not contain default package ({}), does not contain combat override package ({}).",
				coopActor->GetName(),
				packageStackMap.empty(), 
				!packageStackMap.contains(PackageIndex::kDefault), 
				!packageStackMap.contains(PackageIndex::kCombatOverride));

			packageStackMap.insert_or_assign(PackageIndex::kDefault, glob.coopPackageFormlists[p->packageFormListStartIndex]);
			packageStackMap.insert_or_assign(PackageIndex::kCombatOverride, glob.coopPackageFormlists[p->packageFormListStartIndex + 1]);

			if (!glob.coopPackageFormlists[p->packageFormListStartIndex] || !glob.coopPackageFormlists[p->packageFormListStartIndex + 1]) 
			{
				logger::error("[PAM] ERR: MainTask: Default/combat override co-op package formlist for {} is invalid: {}, {}.", 
					coopActor->GetName(),
					(bool)!glob.coopPackageFormlists[p->packageFormListStartIndex],
					(bool)!glob.coopPackageFormlists[p->packageFormListStartIndex + 1]);
			}
		}

		// NOTE: Block over an interval is not in use currently.
		if (blockAllInputActions)
		{
			float secsSinceBlockIntervalStarted = Util::GetElapsedSeconds(p->lastInputActionBlockTP);
			if (secsSinceBlockIntervalStarted > Settings::fSecsToBlockAllInputActions)
			{
				blockAllInputActions = false;
			}
		}

		//================
		// Pre-pass tasks.
		//================
	
		// Read in graph variables' states.
		UpdateGraphVariableStates();
		// Update currently performed combat skills.
		CheckIfPerformingCombatSkills();
		// Handle auto dialogue exit and head tracking if talking to an NPC.
		HandleDialogue();
		// Update actor values and cooldowns.
		UpdateAVsAndCooldowns();
		if (!p->isPlayer1) 
		{
			// Check if a companion player has leveled up a skill.
			CheckForCoopCompanionLevelUps();
		}

		// Failsafe.
		// Ensure that dual casts release properly
		// when there is a delay between the releasing of the casting buttons.
		// A bug would cause the player to continue to cast for free on the hand
		// corresponding to the second button released, 
		// despite neither attack button being held.
		bool stuckInCastingAnim = 
		{
			!p->isPlayer1 && coopActor->HasKeyword(glob.npcKeyword) && isInCastingAnim &&
			castingGlobVars[!CastingGlobIndex::kLH]->value == 0.0f && castingGlobVars[!CastingGlobIndex::kRH]->value == 0.0f
		};
		if (stuckInCastingAnim)
		{
			EvaluatePackage();
		}

		//==========================
		// Player action check loop.
		//==========================
		
		// Current input action mask from controller state.
		inputBitMask = glob.cdh->inputMasksList[controllerID];
		// Ensure input actions aren't blocked before looping.
		if (!blockAllInputActions)
		{
			//====================================================================
			// [Pass 1]: Check Controller Input State and Select Candidate Actions
			//====================================================================
			
			// Update PA perform states to reflect the input states of their composing inputs.
			// Add any new/resumable player actions to candidate player actions list.
			uint32_t i = 0;
			auto actionIndex = !InputAction::kFirstAction;
			// Loop through all actions.
			for (; actionIndex <= !InputAction::kLastAction; ++i, ++actionIndex)
			{
				const auto action = static_cast<InputAction>(actionIndex);
				auto& checkedPAState = paStatesList[i];
				const auto& checkedPAInputMask = checkedPAState.paParams.inputMask;
				// Copy of input bit mask that may be modified below
				// specifically for the current action.
				paBitMask = inputBitMask;

				// Action is disabled. Set as blocked if not already and move on.
				if (checkedPAState.paParams.perfType == PerfType::kDisabled)
				{
					if (checkedPAState.perfStage != PerfStage::kBlocked) 
					{
						checkedPAState.perfStage = PerfStage::kBlocked;
					}

					continue;
				}

				maskIncludesAnalogStick = (checkedPAInputMask & (1 << !InputAction::kLS)) != 0 || (checkedPAInputMask & (1 << !InputAction::kRS)) != 0;
				if (!maskIncludesAnalogStick)
				{
					// Ignore LS/RS movement if the action's input mask does not include either analog stick.
					paBitMask &= (paBitMask & ((1 << !InputAction::kButtonTotal) - 1));
				}
				else
				{
					// If only one analog stick is required
					// remove the other analog stick's bit from the mask.

					bool lsOnly = (checkedPAInputMask & (1 << !InputAction::kLS)) != 0 &&
								  (checkedPAInputMask & (1 << !InputAction::kRS)) == 0;
					bool rsOnly = (checkedPAInputMask & (1 << !InputAction::kRS)) != 0 &&
								  (checkedPAInputMask & (1 << !InputAction::kLS)) == 0;
					if (lsOnly)
					{
						paBitMask &= ~((1 << !InputAction::kRS));
					}
					else if (rsOnly)
					{
						paBitMask &= ~((1 << !InputAction::kLS));
					}
				}

				// Block all actions that share binds with menu controls
				// while in control of an unpaused supported menu.
				// Any actions that don't interact with the menu or that do use the analog sticks can still be performed.
				// (Ex. movement, camera rotation)
				// Also block other players' menu-opening actions if they are not in control of menus.
				float secsSinceAllSupportedMenusClosed = Util::GetElapsedSeconds(glob.lastSupportedMenusClosedTP);
				bool shouldBlockActionsInMenu = glob.supportedMenuOpen.load();
				blockActionInUnpausedMenu = false;
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					if (shouldBlockActionsInMenu) 
					{
						if (!isControllingUnpausedMenu)
						{
							blockActionInUnpausedMenu = paInfo->DEF_ACTION_INDICES_TO_GROUPS.at(actionIndex) == ActionGroup::kMenu;
						}
						else if (ui->menuStack.size() > 0)
						{
							// Back of the stack -> front of the stack => top-most -> bottom-most menus.
							// Still have to get the corresponding menu from the menu map, 
							// since the equality checks below do not work 
							// when a stack GPtr<IMenu> or IMenu* is compared to a map GPtr<IMenu> or IMenu*.
							bool lootMenuOpen = false;
							bool customMenuOpen = false;
							bool dialogueMenuOpen = false;
							// Look for the first supported menu that is open and also on the stack, starting from the top.
							for (auto iter = ui->menuStack.end(); iter != ui->menuStack.begin(); --iter)
							{
								const auto& stackMenu = *iter;
								for (const auto& [name, menuEntry] : ui->menuMap)
								{
									if (menuEntry.menu == stackMenu && GlobalCoopData::SUPPORTED_MENU_NAMES.contains(name))
									{
										lootMenuOpen = name == "LootMenu"sv;
										customMenuOpen = name == "CustomMenu"sv;
										dialogueMenuOpen = name == RE::DialogueMenu::MENU_NAME;
										// Exit outer loop too.
										iter = ui->menuStack.begin() + 1;
										break;
									}
								}
							}

							if (lootMenuOpen)
							{
								// Block actions that are composed of the 'Take', 'Take All', 'Cancel', and DPad buttons.
								blockActionInUnpausedMenu = 
								{ 
									(
										checkedPAInputMask & 
										(
											(1 << !InputAction::kA) | (1 << !InputAction::kX) | (1 << !InputAction::kB) |
											(1 << !InputAction::kDPadD) | (1 << !InputAction::kDPadL) | (1 << !InputAction::kDPadR) | (1 << !InputAction::kDPadU)
										)
									) != 0 
								};
							}
							else if (customMenuOpen || dialogueMenuOpen)
							{
								// Block actions that are composed of the 'Select', 'Cancel', and DPad buttons.
								blockActionInUnpausedMenu = 
								{ 
									(
										checkedPAInputMask &
										(
											(1 << !InputAction::kA) | (1 << !InputAction::kB) |
											(1 << !InputAction::kDPadD) | (1 << !InputAction::kDPadL) | (1 << !InputAction::kDPadR) | (1 << !InputAction::kDPadU)
										)
									) != 0 
								};
							}
							else
							{
								// Block action if any button is pressed.
								blockActionInUnpausedMenu = (checkedPAInputMask & ((1 << !InputAction::kButtonTotal) - 1)) != 0;
							}
						}
					}
					else
					{
						// No supported menu open.
						// At least one button is pressed after supported menus closed.
						bool buttonsPressedWhileMenusClosed = (inputBitMask & ((1 << !InputAction::kButtonTotal) - 1)) != 0;
						// Block any actions with buttons pressed while the last supported menu was still open
						// so that these inputs do not carry over and trigger their corresponding actions now that all supported menus are closed.
						// Eg. The 'Sprint' bind (if bound to 'B') triggering after exiting a menu with the 'B' button.
						// Unblocked once released and pressed again while no menus are open.
						if (bool wasControllingMenus = glob.prevMenuCID == controllerID; wasControllingMenus && buttonsPressedWhileMenusClosed)
						{
							InputAction button = InputAction::kNone;
							for (auto i = !InputAction::kFirst; i < !InputAction::kButtonTotal; ++i)
							{
								// One of this action's composing buttons is pressed.
								if ((checkedPAInputMask & 1 << i) != 0)
								{
									button = static_cast<InputAction>(i);
									const auto& buttonState = glob.cdh->GetInputState(controllerID, button);
									// Button was first pressed while supported menus were still open.
									if (buttonState.isPressed && buttonState.heldTimeSecs >= secsSinceAllSupportedMenusClosed)
									{
										blockActionInUnpausedMenu = true;
									}
								}
							}
						}
					}
				}

				// Check if all required inputs are pressed.
				if (checkedPAState.paParams.triggerFlags.all(TriggerFlag::kLoneAction))
				{
					// Check if a lone action, which means that only the action's composing inputs are pressed.
					allReqPressed = (checkedPAInputMask ^ paBitMask) == 0;
				}
				else
				{
					// Check that all required inputs pressed and possibly others.
					allReqPressed = (checkedPAInputMask & paBitMask) == checkedPAInputMask;
				}

				// Can perform if:
				// 1. Action is not blocked by unpaused menu -AND-
				// 2. All required inputs are pressed -AND-
				// 3. Not blocked, interrupted, or already started -AND-
				// 4. Passes input ordering/hold time check.
				canPerfOnCondPass =
				{
					!blockActionInUnpausedMenu && allReqPressed &&
					checkedPAState.perfStage != PerfStage::kBlocked &&
					checkedPAState.perfStage != PerfStage::kConflictingAction &&
					checkedPAState.perfStage != PerfStage::kStarted &&
					PassesInputPressCheck(action)
				};

				// Some, but not all, required inputs are pressed for this action.
				someReqPressed = (checkedPAInputMask & paBitMask) != 0 && (checkedPAInputMask & paBitMask) != checkedPAInputMask;
				// All required inputs for this action were released.
				allReqReleased = (checkedPAInputMask & paBitMask) == 0;

				if (canPerfOnCondPass)
				{
					// Update last press TP.
					if (checkedPAState.perfStage != PerfStage::kInputsPressed) 
					{
						checkedPAState.pressTP = SteadyClock::now();
					}

					// Set as inputs pressed and add to candidates queue.
					checkedPAState.perfStage = PerfStage::kInputsPressed;
					pressedPACandidates.emplace(action);
				}
				else if (!blockActionInUnpausedMenu && allReqReleased && checkedPAState.perfStage != PerfStage::kInputsReleased)
				{
					// Not blocked, all required inputs released, and not set as inputs released yet.
					checkedPAState.perfStage = PerfStage::kInputsReleased;
				}
				else if (blockActionInUnpausedMenu && checkedPAState.perfStage != PerfStage::kBlocked)
				{
					// Set to blocked so that the action cannot trigger until
					// its inputs are all pressed once again after menus are closed.
					checkedPAState.perfStage = PerfStage::kBlocked;
				}
				else if (someReqPressed)
				{
					if (checkedPAState.perfStage == PerfStage::kStarted) 
					{
						// Only some inputs are now pressed for this started action,
						// so some inputs were released.
						// Can check for resumption once all inputs are pressed again.
						checkedPAState.perfStage = PerfStage::kSomeInputsReleased;
						checkedPAState.releaseTP = SteadyClock::now();
					}
					else if (checkedPAState.perfStage == PerfStage::kInputsPressed)
					{
						// Only some composing inputs are now pressed when all were pressed previously.
						checkedPAState.perfStage = PerfStage::kSomeInputsPressed;
						checkedPAState.releaseTP = SteadyClock::now();
					}
				}
			}

			//================================================================
			// [Pass 2]: Check Candidate Actions and Block Conflicting Actions
			//================================================================
			
			// Check conditions for pressed (not started) candidate actions:
			// - If the action just passed input checks, its specific conditions must also hold before starting.
			// - Block any conflicting actions or set as interrupted if necessary.
			// - Add uninterrupted/unblocked candidate actions to the occurring actions list.

			// Current candidate action.
			auto candidatePA = InputAction::kNone;
			// Keeps track of valid candidate PAs that can be started (no duplicates).
			std::set<InputAction> paCandidatesSet{ };
			// Iterate through the candidates priority queue, from high to low priority.
			while (!pressedPACandidates.empty())
			{
				candidatePA = pressedPACandidates.top();
				pressedPACandidates.pop();
				auto& checkedPAState = paStatesList[!candidatePA - !InputAction::kFirstAction];

				// REMOVE when done debugging.
				/*logger::debug("[PAM] MainTask: {}: PASS 2: candidatePA: {}, stage {}",
					coopActor->GetName(), candidatePA, checkedPAState.perfStage);*/

				// All inputs pressed.
				if (checkedPAState.perfStage == PerfStage::kInputsPressed)
				{
					// Check if the candidate player action's conditions hold before
					// traversing its conflicting actions list to block/interrupt those actions.
					passesConditions = paFuncs->CallPAFunc(p, candidatePA, PAFuncType::kCondFunc);
					if (passesConditions)
					{
						// Add to the set of candidate player actions once condition checks hold.
						paCandidatesSet.insert(candidatePA);
						// Should block conflicting actions.
						if (checkedPAState.paParams.triggerFlags.none(TriggerFlag::kDoNotBlockConflictingActions))
						{
							for (const auto& conflictingAction : paConflictSetsList[!candidatePA - !InputAction::kFirstAction])
							{
								auto& otherPAState = paStatesList[!conflictingAction - !InputAction::kFirstAction];
								auto& otherPerfStage = otherPAState.perfStage;

								// Don't block/interrupt actions that have the ignore conflicting actions flag
								// or more composing inputs than the current player action.
								const bool ignoreBlockRequest = 
								{
									otherPAState.paParams.triggerFlags.any(TriggerFlag::kIgnoreConflictingActions) ||
									otherPAState.paParams.composingInputs.size() > checkedPAState.paParams.composingInputs.size() 
								};
								if (!ignoreBlockRequest)
								{
									if (otherPerfStage == PerfStage::kStarted)
									{
										// Interrupted if started already.

										// REMOVE when done debugging.
										logger::debug("[PAM] MainTask: {}: PASS 2: candidate PA {} ({}): conflicting STARTED action {} ({}) is now interrupted.",
											coopActor->GetName(),
											candidatePA, paStatesList[!candidatePA - !InputAction::kFirstAction].priority,
											conflictingAction, paStatesList[!conflictingAction - !InputAction::kFirstAction].priority);

										otherPerfStage = PerfStage::kConflictingAction;
									}
									else if (otherPerfStage != PerfStage::kBlocked)
									{
										// Block conflicting candidate action that is not blocked yet.

										// REMOVE when done debugging.
										logger::debug("[PAM] MainTask: {}: PASS 2: candidate PA {}'s conflicting CANDIDATE/INTERRUPTED PA {} (with perf stage {}) is now blocked from performing.",
											coopActor->GetName(), candidatePA, conflictingAction, otherPerfStage);

										otherPerfStage = PerfStage::kBlocked;
									}
								}
							}
						}
					}
				}
			}

			// Add candidate actions which haven't started yet to the occurring player actions list.
			for (const auto& action : paCandidatesSet)
			{
				const auto& paState = paStatesList[!action - !InputAction::kFirstAction];
				if (paState.perfStage == PerfStage::kInputsPressed)
				{
					// Add AV cost action request if needed.
					if (paState.avCost != 0.0f)
					{
						AddAVCostActionRequest(action);
					}

					// Add to occurring list if not blocked following the queue checks.
					occurringPAs.push_front(action);
				}
			}

			//=====================================================================================
			// [Pass 3]: Remove Any Blocked/Interrupted Player Actions From Occurring Actions List.
			//=====================================================================================
			
			// If conflicting actions are cleaned up after the next pass instead, the cleanup could affect active actions.
			// E.g. Sprint is already occurring and then ShieldCharge becomes active and interrupts Sprint. 
			// The sprint start action idle then promptly gets cancelled in the cleanup function for the conflicting action Sprint, 
			// stopping the shield charge from occurring right after it starts.
			// Cleanup interrupted actions here first to prevent interference.
			std::erase_if(occurringPAs,
				[this, &paFuncs](const InputAction& a_action) {
					const auto& perfStage = paStatesList[!a_action - !InputAction::kFirstAction].perfStage;
					const auto& perfType = paStatesList[!a_action - !InputAction::kFirstAction].paParams.perfType;
					// NOTE: Perform stage is not set to 'failed conditions' until Pass 4 after the action has already started, 
					// so it is not included as one of the interrupted stages for this pass.
					bool interrupted = perfStage == PerfStage::kConflictingAction || perfStage == PerfStage::kBlocked;
					if (interrupted)
					{
						// Perform cleanup for OnHold/OnPressAndRelease action (only perform types with cleanup funcs)
						// if this action cleans up on interrupt/block.
						bool cleanUpAfterInterrupt = 
						{
							(perfType == PerfType::kOnHold || perfType == PerfType::kOnPressAndRelease) &&
							(paStatesList[!a_action - !InputAction::kFirstAction].paParams.triggerFlags.none(TriggerFlag::kNoCleanupAfterInterrupt))
						};
						if (cleanUpAfterInterrupt)
						{
							// REMOVE when done debugging.
							logger::debug("[PAM] MainTask: {}: PASS 3: performing cleanup on interrupted occurring action {} with perf stage {}.",
								coopActor->GetName(), a_action, perfStage);

							paFuncs->CallPAFunc(p, a_action, PAFuncType::kCleanupFunc);
						}

						// REMOVE when done debugging.
						logger::debug("[PAM] MainTask: {}: PASS 3: Interrupted action {} should be removed (perf stage {}), removing from occurring PAs list.",
							coopActor->GetName(), a_action, perfStage);
					}

					return interrupted;
				});

			//====================================================================================================
			// [Pass 4]: Run Player Action Funcs on Occurring Actions and Update Occurring Actions' Perform Stages
			//====================================================================================================
			
			// - If the action's conditions are satisfied:
			//		- Run Start funcs on press, if the occurring action is an OnPress/OnConsecPress/OnPressAndRelease action.
			//		- Run Progress funcs on press and hold, if the occurring action is an OnHold action.
			//		- Run Start funcs on release, if the occurring action is an OnRelease action.
			//		- Run Cleanup funcs on release, if the occurring action is an OnHold/OnPressAndRelease action.
			// - If conditions fail:
			//		- Tag as blocked if the action should be blocked on condition failure.
			//		- Tag as interrupted by failed conditions if not already tagged.
			for (auto action : occurringPAs)
			{
				auto& paState = paStatesList[!action - !InputAction::kFirstAction];
				auto& perfStage = paState.perfStage;
				const auto& perfType = paState.paParams.perfType;

				// Check conditions for occurring actions each iteration.
				passesConditions = paFuncs->CallPAFunc(p, action, PAFuncType::kCondFunc);
				// Can't be performed if interrupted.
				bool isNotInterrupted = 
				{
					perfStage != PerfStage::kBlocked &&
					perfStage != PerfStage::kConflictingAction &&
					perfStage != PerfStage::kFailedConditions
				};
				if (passesConditions && isNotInterrupted)
				{
					// Just started this action.
					bool justStarted = perfStage == PerfStage::kInputsPressed;
					if (justStarted)
					{
						// Set start time point and seconds performed to 0.
						paState.startTP = SteadyClock::now();
						paState.secsPerformed = 0.0f;

						// Set as started now.
						perfStage = PerfStage::kStarted;
					}
					else
					{
						// Update seconds performed.
						paState.secsPerformed = Util::GetElapsedSeconds(paState.startTP);
					}

					if (justStarted)
					{
						// REMOVE when done debugging.
						logger::debug("[PAM] MainTask: {}: PASS 4: {} just started with action perf type {}.",
							coopActor->GetName(), action, perfType);

						// Start performing OnPress/OnPressAndRelease/OnHold actions.
						if (perfType == PerfType::kOnPress || perfType == PerfType::kOnPressAndRelease || perfType == PerfType::kOnHold)
						{
							if (perfType == PerfType::kOnHold)
							{
								paFuncs->CallPAFunc(p, action, PAFuncType::kProgressFunc);
							}
							else
							{
								paFuncs->CallPAFunc(p, action, PAFuncType::kStartFunc);
							}
						}
					}
					else if (perfStage == PerfStage::kStarted)
					{
						// Already started.
						// Only OnHold actions are performed once already started.
						if (perfType == PerfType::kOnHold)
						{
							paFuncs->CallPAFunc(p, action, PAFuncType::kProgressFunc);
						}
					}
					else if (perfStage == PerfStage::kSomeInputsReleased || perfStage == PerfStage::kSomeInputsPressed || perfStage == PerfStage::kInputsReleased)
					{
						// Some/all inputs released.
						// Set release time point.
						paState.stopTP = SteadyClock::now();
						if (perfType == PerfType::kOnRelease || perfType == PerfType::kOnConsecTap)
						{
							// REMOVE when done debugging.
							logger::debug("[PAM] MainTask: {}: PASS 4: {}, perf type {}, is about to be performed ON RELEASE. Perf stage: {}",
								coopActor->GetName(), action, perfType, perfStage);

							// Perform OnRelease/OnConsecTap actions on release.
							paFuncs->CallPAFunc(p, action, PAFuncType::kStartFunc);
						}
						else if (perfType == PerfType::kOnHold || perfType == PerfType::kOnPressAndRelease)
						{
							// REMOVE when done debugging.
							logger::debug("[PAM] MainTask: {}: PASS 4: {}, perf type {}, is about to clean up ON RELEASE. Perf stage: {}",
								coopActor->GetName(), action, perfType, perfStage);

							// Clean up OnHold and OnPressAndRelease actions on release.
							paFuncs->CallPAFunc(p, action, PAFuncType::kCleanupFunc);
						}
					}
				}
				else if (!passesConditions)
				{
					// Conditions failed, so either block or set as interrupted by failed conditions.
					if (paState.paParams.triggerFlags.all(TriggerFlag::kBlockOnConditionFailure) && perfStage != PerfStage::kBlocked) 
					{
						// REMOVE when done debugging.
						logger::debug("[PAM] MainTask: {}: PASS 4: {} failed conditions. Set to blocked. Perf stage: {}",
							coopActor->GetName(), action, perfStage);

						// Will not resume until released and pressed again.
						perfStage = PerfStage::kBlocked;
					}
					else if (paState.paParams.triggerFlags.none(TriggerFlag::kBlockOnConditionFailure) && perfStage != PerfStage::kFailedConditions)
					{
						// REMOVE when done debugging.
						logger::debug("[PAM] MainTask: {}: PASS 4: {} failed conditions. Set to interrupted. Perf stage: {}",
							coopActor->GetName(), action, perfStage);

						// Can resume if conditions hold once more,
						// even without releasing and pressing the bind again.
						perfStage = PerfStage::kFailedConditions;
					}
				}
				else
				{
					// REMOVE when done debugging.
					logger::debug("[PAM] MainTask: {}: PASS 4: {} is already interrupted with perf stage {}.",
						coopActor->GetName(), action, perfStage);
				}
			}

			//============================================================
			// [Pass 5]: Remove Interrupted Actions and Clean Up If Needed
			//============================================================

			// Final pass to ensure only started or pressed actions remain in the occurring player actions list.
			std::erase_if(occurringPAs,
				[this, &paFuncs](const InputAction& a_action) {
					const auto& perfStage = paStatesList[!a_action - !InputAction::kFirstAction].perfStage;
					const auto& perfType = paStatesList[!a_action - !InputAction::kFirstAction].paParams.perfType;
					// Some/all inputs released or interrupted.
					bool shouldStop = perfStage != PerfStage::kInputsPressed && perfStage != PerfStage::kStarted;
					if (shouldStop)
					{
						bool interrupted = perfStage == PerfStage::kConflictingAction || perfStage == PerfStage::kFailedConditions || perfStage == PerfStage::kBlocked;
						// Perform cleanup for OnHold/OnPressAndRelease action (only perform types with cleanup funcs)
						// if this action cleans up on interrupt/block.
						bool cleanUpAfterInterrupt = 
						{
							(interrupted) &&
							(perfType == PerfType::kOnHold || perfType == PerfType::kOnPressAndRelease) &&
							(paStatesList[!a_action - !InputAction::kFirstAction].paParams.triggerFlags.none(TriggerFlag::kNoCleanupAfterInterrupt))
						};
						if (cleanUpAfterInterrupt)
						{
							// REMOVE when done debugging.
							logger::debug("[PAM] MainTask: {}: PASS 5: performing cleanup on interrupted occurring action {} with perf stage {}.",
								coopActor->GetName(), a_action, perfStage);

							paFuncs->CallPAFunc(p, a_action, PAFuncType::kCleanupFunc);
						}

						// REMOVE when done debugging.
						logger::debug("[PAM] MainTask: {}: PASS 5: {} should be removed (perf stage {}), removing from occurring PAs list.",
							coopActor->GetName(), a_action, perfStage);
					}

					return shouldStop;
				});
		}
		else
		{
			// Continue blocking input actions.
			BlockCurrentInputActions();
		}

		// Post-pass tasks.
		if (p->isPlayer1)
		{
			// Send any queued button events.
			ChainAndSendP1ButtonEvents();

			// No level up threshold to modify for Enderal since it has its own progression system.
			if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled)
			{
				// Modify level up threshold if needed (player's level changed and no menus are open).
				// Obviously not a big fan of checking this periodically but updating the threshold is not
				// a particularly time sensitive task, so check every second.
				// Also updated when entering the StatsMenu or entering/exiting the LevelUp Menu.
				float secsSinceXPThresholdCheck = Util::GetElapsedSeconds(glob.lastXPThresholdCheckTP);
				if (secsSinceXPThresholdCheck > 1.0f && Util::MenusOnlyAlwaysOpenInMap())
				{
					GlobalCoopData::ModifyLevelUpXPThreshold(true);
					glob.lastXPThresholdCheckTP = SteadyClock::now();
				}
			}
		}
		else
		{
			// Hacky, but the only solution for now.
			// Update bound weapon state for companion players
			// and unequip bound weapons once their lifetime expires.
			UpdateBoundWeaponTimers();
		}

		// Update player transformation state.
		UpdateTransformationState();
		// Update the last hand used to perform an attack or cast.
		UpdateLastAttackingHand();
		// Expend stamina or magicka if the appropriate animation event triggers.
		HandleAVExpenditure();
		// Make sure all players are set as essential if using the revive system.
		// Game will sometimes reset the essential flag after it is set, so check each iteration.
		SetEssentialForReviveSystem();

		// NOTE: Failsafe for killmoves.
		// If this player is performing a killmove on another actor, 
		// ensure the victim actor is actually dead after the killmove completes.
		if (Settings::bUseKillmovesSystem) 
		{
			HandleKillmoveRequests();
		}
	}

	void PlayerActionManager::PrePauseTask()
	{
		logger::debug("[PAM] PrePauseTask: P{}: Refresh data: {}", playerID + 1, nextState == ManagerState::kAwaitingRefresh);
		// Reset AVs, clear delayed AV cost actions, and change packages to default.
		ResetAttackDamageMult();
		avcam->Clear();

		// Reset base HMS rate mults back to the default of 100, since we've modified them.
		coopActor->SetBaseActorValue(RE::ActorValue::kHealRateMult, 100.0f);
		coopActor->SetBaseActorValue(RE::ActorValue::kMagickaRateMult, 100.0f);
		coopActor->SetBaseActorValue(RE::ActorValue::kStaminaRateMult, 100.0f);

		if (!p->isPlayer1)
		{
			// Stop sneaking before awaiting refresh.
			if (coopActor->IsSneaking() && nextState == ManagerState::kAwaitingRefresh)
			{
				SetPackageFlag(RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak, false);
			}

			// Reset packages to default.
			if (packageStackMap[PackageIndex::kDefault] && glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kDefault]) 
			{
				packageStackMap[PackageIndex::kDefault]->forms[0] = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kDefault];
			}

			if (packageStackMap[PackageIndex::kCombatOverride] && glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kCombatOverride]) 
			{
				packageStackMap[PackageIndex::kCombatOverride]->forms[0] = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kCombatOverride];
			}
		}

		// Clear out button events queue.
		for (auto& ptr : queuedP1ButtonEvents)
		{
			ptr.release();
		}

		queuedP1ButtonEvents.clear();

		// If necessary, elinquish control of the camera befire pausing.
		if (glob.cam->IsRunning()) 
		{
			auto& controllingCID = glob.cam->controlCamCID;
			if (controllingCID == controllerID && glob.cam->camAdjMode != CamAdjustmentMode::kNone)
			{
				controllingCID = -1;
				glob.cam->camAdjMode = CamAdjustmentMode::kNone;
			}
		}
	}

	void PlayerActionManager::PreStartTask()
	{
		logger::debug("[PAM] PreStartTask: P{}", playerID + 1);
		if (p->isPlayer1) 
		{
			// Ensure all controls are enabled.
			Util::ToggleAllControls(true);
		}
		else
		{
			// Activate self with P1 to display this character in Party Combat Parameters UI.
			if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1) 
			{
				Util::ActivateRef(coopActor.get(), p1, 0, coopActor->GetBaseObject(), 1, false);
			}
		}

		// Set base HMS rate mults to the default of 100 initially. Will be modified later.
		coopActor->SetBaseActorValue(RE::ActorValue::kHealRateMult, 100.0f);
		coopActor->SetBaseActorValue(RE::ActorValue::kMagickaRateMult, 100.0f);
		coopActor->SetBaseActorValue(RE::ActorValue::kStaminaRateMult, 100.0f);

		// Block any already-held inputs from triggering input actions when starting the action manager.
		BlockCurrentInputActions();
		// Set all time points to now before starting main task.
		ResetTPs();
		// Clear out data for AV cost action manager.
		avcam->Clear();

		// Clear out all queued P1 button events.
		for (auto& ptr : queuedP1ButtonEvents)
		{
			ptr.release();
		}

		queuedP1ButtonEvents.clear();
	}

	void PlayerActionManager::RefreshData()
	{
		// Player/actor data
		coopActor = p->coopActor;
		controllerID = p->controllerID;
		downedPlayerTarget = nullptr;
		playerID = p->playerID;
		player1RefAlias = glob.player1RefAlias;
		killerPlayerActorHandle = killmoveTargetActorHandle = RE::ActorHandle();

		// Clear assigned packages to P1 ref alias.
		if (p->isPlayer1 && player1RefAlias && player1RefAlias->fillData.uniqueActor.uniqueActor)
		{
			player1RefAlias->fillData.uniqueActor.uniqueActor->aiPackages.packages.clear();
		}

		// Setup initial default and combat override package formlists and packages.
		packageStackMap.insert_or_assign(PackageIndex::kDefault, glob.coopPackageFormlists[p->packageFormListStartIndex]);
		packageStackMap.insert_or_assign(PackageIndex::kCombatOverride, glob.coopPackageFormlists[p->packageFormListStartIndex + 1]);

		// Attacking hand.
		lastAttackingHand = HandIndex::kNone;
		// Player action bookkeeping.
		// AV cost manager for triggered player actions.
		avcam = std::make_unique<AVCostActionManager>();
		// Currently occurring PAs.
		occurringPAs.clear();
		// Sets of conflicting PAs for each PA.
		paConflictSetsList.fill({});
		// List of PA parameters for each PA.
		paParamsList.fill({});
		// List of PA states for each PA.
		paStatesList.fill({});
		// Clear out button events queue.
		for (auto& ptr : queuedP1ButtonEvents)
		{
			ptr.release();
		}

		queuedP1ButtonEvents.clear();

		// Special action.
		reqSpecialAction = SpecialActionType::kNone;
		// Staff usage global variables for ranged attack package.
		RE::TESForm* globForm = RE::TESForm::LookupByEditorID(std::string("__CoopPlayerUseLHStaff") + std::to_string(controllerID + 1));
		usingLHStaff = globForm ? globForm->As<RE::TESGlobal>() : nullptr;
		globForm = RE::TESForm::LookupByEditorID(std::string("__CoopPlayerUseRHStaff") + std::to_string(controllerID + 1));
		usingRHStaff = globForm ? globForm->As<RE::TESGlobal>() : nullptr;
		// Casting.
		lhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
		rhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
		castingGlobVars.fill(nullptr);
		for (uint8_t i = 0; i < !CastingGlobIndex::kTotal; ++i) 
		{
			castingGlobVars[i] = glob.castingGlobVars[!CastingGlobIndex::kTotal * controllerID + i];
			castingGlobVars[i]->value = 0.0f;
		}

		// Combat skill that give XP.
		perfSkillIncCombatActions = SkillIncCombatActionType::kNone;
		// Health.
		baseHealth = coopActor->GetBaseActorValue(RE::ActorValue::kHealth);
		currentHealth = coopActor->GetActorValue(RE::ActorValue::kHealth);
		baseHealthRegenRateMult = coopActor->GetBaseActorValue(RE::ActorValue::kHealRateMult);
		// Magicka.
		baseMagicka = coopActor->GetBaseActorValue(RE::ActorValue::kMagicka);
		currentMagicka = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		lhCastDuration = rhCastDuration = 0.0f;
		baseMagickaRegenRateMult = coopActor->GetBaseActorValue(RE::ActorValue::kMagickaRateMult);
		// Stamina.
		baseStamina = coopActor->GetBaseActorValue(RE::ActorValue::kStamina);
		currentStamina = coopActor->GetActorValue(RE::ActorValue::kStamina);
		secsTotalStaminaRegenCooldown = 0.0f;
		baseStaminaRegenRateMult = coopActor->GetBaseActorValue(RE::ActorValue::kStaminaRateMult);
		// Seconds since X event happened.
		secsCurrentShoutCooldown =
		secsSinceBoundWeapLHReq =
		secsSinceBoundWeapRHReq =
		secsSinceLastActivationCyclingCheck =
		secsSinceLastShout =
		secsSinceQSSCastStart =
		secsSinceReviveCheck = 0.0f;
		// Bound weapon equip duration (default set here).
		secsBoundWeaponLHDuration = secsBoundWeaponRHDuration = 120.0f;
		// Damage mult.
		reqDamageMult = 1.0f;
		// Bools.
		attackDamageMultSet = false;
		autoEndDialogue = false;
		blockAllInputActions = false;
		boundWeapReqLH = false;
		boundWeapReqRH = false;
		canShout = true;
		isAttacking = false;
		isBashing = false;
		isBeingKillmovedByAnotherPlayer = false;
		isBlocking = false;
		isCastingDual = false;
		isCastingLH = false;
		isCastingRH = false;
		isControllingUnpausedMenu = false;
		isInCastingAnim = false;
		isInCastingAnimDual = false;
		isInCastingAnimLH = false;
		isInCastingAnimRH = false;
		isInDialogue = false;
		isJumping = false;
		isPerformingKillmove = false;
		isPlayingEmoteIdle = false;
		isPowerAttacking = false;
		isRangedAttack = false;
		isRangedWeaponAttack = false;
		isRiding = false;
		isRolling = false;
		isSneaking = false;
		isSprinting = false;
		isVoiceCasting = false;
		isWeaponAttack = false;
		requestedToParaglide = false;
		sendingP1MotionDrivenEvents = false;
		weapMagReadied = coopActor->IsWeaponDrawn();
		// Ints.
		lastAnimEventID = 0;
		inputBitMask = 0;

		// Set player binds.
		UpdatePlayerBinds();
		// Copy shared AVs from P1 to to co-op players.
		CopyOverSharedSkillAVs();
		// Reset time points.
		ResetTPs();
		logger::debug("[PAM] Refreshed data for {}.", coopActor ? coopActor->GetName() : "NONE");
	}

	const ManagerState PlayerActionManager::ShouldSelfPause()
	{
		// Suspension triggered externally.
		return currentState;
	}

	const ManagerState PlayerActionManager::ShouldSelfResume()
	{
		// Resumption triggered externally.
		return currentState;
	}

#pragma endregion

	void PlayerActionManager::AddAVCostActionRequest(const InputAction& a_action)
	{
		// Queue an actor value (AV) cost action request based on the given player action.
		// The actor value cost action request will be handled once the related animation 
		// event triggers for the given action, and the player's HMS actor values are updated accordingly.
		// Links player actions' AV costs to animation events.

		AVCostAction avAction = AVCostAction::kNone;
		switch (a_action)
		{
		case InputAction::kAttackLH:
		case InputAction::kBash:
		{
			// Bash with left attack while using a bow/crossbow.
			avAction = AVCostAction::kBash;
			break;
		}
		case InputAction::kCastLH:
		{
			// P1 magicka expenditure handled by the game already.
			if (!p->isPlayer1)
			{
				avAction = AVCostAction::kCastLeft;
			}

			break;
		}
		case InputAction::kCastRH:
		{
			// P1 magicka expenditure handled by the game already.
			if (!p->isPlayer1)
			{
				avAction = AVCostAction::kCastRight;
			}

			break;
		}
		case InputAction::kDodge:
		{
			avAction = AVCostAction::kDodge;
			break;
		}
		case InputAction::kPowerAttackDual:
		{
			avAction = AVCostAction::kPowerAttackDual;
			break;
		}
		case InputAction::kPowerAttackLH:
		{
			avAction = AVCostAction::kPowerAttackLeft;
			break;
		}
		case InputAction::kPowerAttackRH:
		{
			avAction = AVCostAction::kPowerAttackRight;
			break;
		}
		case InputAction::kSprint:
		{
			// Roll or sprint/shield charge.
			avAction = coopActor->IsSneaking() ? AVCostAction::kSneakRoll : AVCostAction::kSprint;
			break;
		}
		case InputAction::kSpecialAction:
		{
			// Direct insertion based on requested special action type.
			if (reqSpecialAction == SpecialActionType::kCastBothHands)
			{
				// P1 magicka expenditure handled by the game already.
				if (!p->isPlayer1)
				{
					// Special case with double insertion of CastLH and CastRH requests. 
					// Return after inserting both.
					avcam->InsertRequestedAction(AVCostAction::kCastLeft);
					avcam->InsertRequestedAction(AVCostAction::kCastRight);
					avcam->SetCost(AVCostAction::kCastLeft, paStatesList[!InputAction::kCastLH - !InputAction::kFirstAction].avCost);
					avcam->SetCost(AVCostAction::kCastRight, paStatesList[!InputAction::kCastRH - !InputAction::kFirstAction].avCost);
				}

				return;
			}
			else if (reqSpecialAction == SpecialActionType::kDodge)
			{
				// Insert directly and return.
				avcam->InsertRequestedAction(AVCostAction::kDodge);
				avcam->SetCost(AVCostAction::kDodge, paStatesList[!InputAction::kDodge - !InputAction::kFirstAction].avCost);
				return;
			}
			else if (reqSpecialAction == SpecialActionType::kDualCast)
			{
				// P1 magicka expenditure handled by the game already.
				if (!p->isPlayer1)
				{
					avcam->InsertRequestedAction(AVCostAction::kCastDual);
					avcam->SetCost(AVCostAction::kCastDual, paStatesList[!InputAction::kSpecialAction - !InputAction::kFirstAction].avCost);
				}

				return;
			}

			break;
		}
		default:
			return;
		}

		// If an AV cost action was set requested is not already accounted for.
		if ((avAction != AVCostAction::kNone) && (avcam->reqActionsSet.empty() || !avcam->reqActionsSet.contains(avAction)))
		{
			avcam->InsertRequestedAction(avAction);
			avcam->SetCost(avAction, paStatesList[!a_action - !InputAction::kFirstAction].avCost);
		}
	}

	void PlayerActionManager::BlockCurrentInputActions(bool a_startBlockInterval)
	{
		// Set any held inputs' input actions to blocked.
		// Have to release and re-press to trigger the blocked actions.
		// Can also block all actions over an interval, if requested.
		// Set the block request start TP.

		InputAction action = InputAction::kNone;
		for (auto actionIndex = !InputAction::kFirstAction; actionIndex <= !InputAction::kLastAction; ++actionIndex)
		{
			action = static_cast<InputAction>(actionIndex);
			if (AllInputsPressedForAction(action))
			{
				auto& actionState = paStatesList[actionIndex];
				actionState.perfStage = PerfStage::kBlocked;
			}
		}

		if (a_startBlockInterval) 
		{
			blockAllInputActions = true;
			p->lastInputActionBlockTP = SteadyClock::now();
		}
	}

	void PlayerActionManager::CastRuneProjectile(RE::SpellItem* a_spell)
	{
		// Directly cast a spell's associated rune projectile,
		// since companion players' package casting procedure fails to cast rune spells.

		// No magic caster, no cast. Simple as.
		auto magicCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		if (!magicCaster)
		{
			return;
		}

		auto baseProj = a_spell->GetAVEffect()->data.projectileBase;
		// Not sure what the angles are relative to at the moment.
		// Setting to aim pitch/yaw for the initial cast.
		RE::Projectile::ProjectileRot angles{ coopActor->GetAimAngle(), coopActor->GetAimHeading() };
		RE::NiPoint3 targetPos = Util::HandleIsValid(p->tm->crosshairRefrHandle) ? p->tm->crosshairWorldPos : coopActor->data.location;
		RE::ProjectileHandle resultProj{ };

		glm::vec4 startPos = ToVec4(magicCaster->GetMagicNode()->world.translate);
		glm::vec4 aimDir = ToVec4(Util::RotationToDirectionVect(-p->mm->aimPitch, Util::ConvertAngle(coopActor->data.angle.z)));
		float pitch = 0.0f;
		float yaw = 0.0f;
		// Cast far away in the direction that the player is aiming.
		// Place the rune projectile at the first hit location, if any.
		// Otherwise, the run projectile will be placed at the crosshair world position or at the player's feet.
		if (auto raycastResult = Raycast::hkpCastRay(startPos, startPos + aimDir * 16384.0f, true); raycastResult.hit)
		{
			targetPos = ToNiPoint3(raycastResult.hitPos + raycastResult.rayNormal * min(raycastResult.rayLength, 25.0f));
			// Parallel to the hit surface.
			pitch = Util::NormalizeAngToPi(Util::DirectionToGameAngPitch(ToNiPoint3(raycastResult.rayNormal)) + PI / 2.0f);
			// Facing the camera.
			yaw = Util::NormalizeAngToPi(Util::NormalizeAng0To2Pi(glob.cam->GetCurrentYaw() + PI));
		}

		RE::Projectile::LaunchSpell(std::addressof(resultProj), coopActor.get(), a_spell, targetPos, angles);
		// Set rotation after launch.
		if (resultProj && resultProj.get())
		{
			auto proj = resultProj.get();
			proj->data.angle.x = pitch;
			proj->data.angle.z = yaw;
			if (auto proj3D = Util::GetRefr3D(proj.get()); proj3D)
			{
				Util::SetRotationMatrix(proj3D->local.rotate, proj->data.angle.x, proj->data.angle.z);
			}
		}
	}

	void PlayerActionManager::CastSpellWithMagicCaster(const EquipIndex& a_index, bool&& a_startCast, bool&& a_waitForCastingAnim, const bool& a_shouldCastWithP1)
	{
		// Start/stop casting the spell equipped at the given index, optionally waiting for the player's casting animations
		// to trigger or casting the spell with one of P1's magic casters instead.
		auto form = p->em->equippedForms[!a_index]; 
		if (a_index == EquipIndex::kVoice)
		{
			form = p->em->voiceSpell;
		}

		// Invalid spell in slot.
		if (!form || !form->As<RE::SpellItem>()) 
		{
			return;
		}
		
		RE::SpellItem* spell = form->As<RE::SpellItem>();
		if (a_startCast)
		{
			// Quick slot or voice spell cast.
			if (a_index == EquipIndex::kQuickSlotSpell || a_index == EquipIndex::kVoice)
			{
				// Set ranged attack package target.
				p->tm->UpdateAimTargetLinkedRefr(a_index);
				auto targetPtr = Util::GetRefrPtrFromHandle(p->tm->aimTargetLinkedRefrHandle);
				bool targetValidity = targetPtr && Util::IsValidRefrForTargeting(targetPtr.get());
				// Will use instant caster.
				auto magicCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				if (spell && magicCaster && targetValidity)
				{
					auto spellType = spell->GetSpellType();
					bool isVoicePower = 
					{
						spellType == RE::MagicSystem::SpellType::kAbility ||
						spellType == RE::MagicSystem::SpellType::kLesserPower ||
						spellType == RE::MagicSystem::SpellType::kPower ||
						spellType == RE::MagicSystem::SpellType::kVoicePower
					};
					bool isConcSpell = spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
					// Can only cast self-targeted powers with player 1, so this means only one power can be used
					// among the entire party at any time.
					if (!p->isPlayer1 && a_shouldCastWithP1)
					{
						// Have P1 cast spells with image space modifiers, so that they properly display on the screen.
						magicCaster = glob.player1Actor->GetMagicCaster(RE::MagicSystem::CastingSource::kOther);
						if (magicCaster)
						{
							// NOTE: Battle Cry and some other powers do not seem to work when cast using either method below.
							if (spell->formID == 0xE40C3) 
							{
								magicCaster->desiredTarget = coopActor->GetHandle();
								magicCaster->currentSpell = spell;
								magicCaster->SpellCast(true, 0, spell);
							}
							else
							{
								// Set spell for player 1.
								magicCaster->SetCurrentSpellImpl(spell);
								magicCaster->currentSpell = spell;
								// Zero out magicka cost for P1 since they're not the one requesting the cast.
								magicCaster->currentSpellCost = 0.0f;
								// Cast at caster.
								if (targetPtr == coopActor)
								{
									targetPtr = glob.player1Actor;
								}

								magicCaster->desiredTarget = targetPtr->GetHandle();
								secsSinceQSSCastStart = Util::GetElapsedSeconds(p->lastQSSCastStartTP);
								float magickaCost = paStatesList[!InputAction::kQuickSlotCast - !InputAction::kFirstAction].avCost;
								// Concentration spells' cost scales with cast time.
								if (isConcSpell)
								{
									// Cost per second * number of seconds.
									magickaCost *= secsSinceQSSCastStart;
								}

								// Only cast if this player has enough magicka.
								// Constantly cast or cast at an interval equal to the spell's charge time if not casting a concentration spell.
								float minChargeTime = max(0.5f, spell->data.chargeTime);
								if ((magickaCost <= currentMagicka) && (isConcSpell || secsSinceQSSCastStart > minChargeTime))
								{
									p->lastQSSCastStartTP = SteadyClock::now();

									magicCaster->CastSpellImmediate(spell, false, targetPtr.get(), 1.0f, false, 0.0f, coopActor.get());
									float deltaMagicka = max(-currentMagicka, -magickaCost);
									ModifyAV(RE::ActorValue::kMagicka, deltaMagicka);

									// Add skill XP for non-healing spells cast at a target actor while not in god mode.
									// Done here instead of in the HandleHealthDamage() hook because the attacker
									// would be listed as P1 instead of this player.
									if (!p->isInGodMode && spell->avEffectSetting && Util::HandleIsValid(p->tm->GetRangedTargetActor()))
									{
										auto spellSkillAV = spell->avEffectSetting->data.associatedSkill;
										if (glob.AV_TO_SKILL_MAP.contains(spellSkillAV))
										{
											// Restoration skill, or spell modifies health.
											bool notHealingSpell = 
											{
												(spellSkillAV != RE::ActorValue::kRestoration) ||
												(spell->avEffectSetting->data.primaryAV != RE::ActorValue::kHealth &&
												spell->avEffectSetting->data.secondaryAV != RE::ActorValue::kHealth)
											};
											if (notHealingSpell)
											{
												GlobalCoopData::AddSkillXP(controllerID, spellSkillAV, magickaCost);
											}
										}
									}
								}
							}
						}
					}
					else
					{
						auto archetype = spell->avEffectSetting ? spell->avEffectSetting->GetArchetype() : RE::EffectSetting::Archetype::kNone;
						bool isTransformationSpell = archetype == RE::EffectSetting::Archetype::kWerewolf || archetype == RE::EffectSetting::Archetype::kVampireLord;
						bool isCoopVampireRevertFormSpell = spell->formID == 0x200CD5C;
						// Sheathe weapons to make sure they aren't visible after transforming.
						// A werewolf with an unusable weapon protruding from its claws looks kind of odd, no?
						if (isTransformationSpell)
						{
							ReadyWeapon(false);
						}

						// Revert form for companion players if they are transformed into their Vampire Lord form.
						if (isCoopVampireRevertFormSpell) 
						{
							ReadyWeapon(false);
							p->RevertTransformation();
							return;
						}

						// Check for magicka expenditure if not casting a power.
						if (!isVoicePower)
						{
							secsSinceQSSCastStart = Util::GetElapsedSeconds(p->lastQSSCastStartTP);
							// Cast spell immediate function does not automatically update the player's magicka
							// when casting a non-concentration spell. Do it here.
							float magickaCost = paStatesList[!InputAction::kQuickSlotCast - !InputAction::kFirstAction].avCost;
							if (isConcSpell)
							{
								magickaCost *= secsSinceQSSCastStart;
							}

							// Only cast if this player has enough magicka.
							// Constantly cast or cast at an interval equal to the spell's charge time if not casting a concentration spell.
							float minChargeTime = max(0.5f, spell->data.chargeTime);
							if ((magickaCost <= currentMagicka) && (isConcSpell || secsSinceQSSCastStart >= minChargeTime))
							{
								p->lastQSSCastStartTP = SteadyClock::now();

								// Does not seem like 'target location' spells can be cast using the
								// "CastSpellImmediate" function.
								magicCaster->desiredTarget = targetPtr->GetHandle();
								magicCaster->SetCurrentSpellImpl(spell);
								magicCaster->CastSpellImmediate(spell, false, targetPtr.get(), 1.0f, false, 0.0f, coopActor.get());

								// Expend magicka.
								float deltaMagicka = -magickaCost;
								ModifyAV(RE::ActorValue::kMagicka, deltaMagicka);
								
								// Add skill XP for non-healing spells cast at a target actor while not in god mode.
								if (!p->isInGodMode && spell->avEffectSetting && Util::HandleIsValid(p->tm->GetRangedTargetActor()))
								{
									auto spellSkillAV = spell->avEffectSetting->data.associatedSkill;
									if (glob.AV_TO_SKILL_MAP.contains(spellSkillAV))
									{
										// Restoration skill, or spell modifies health.
										bool notHealingSpell = 
										{
											(spellSkillAV != RE::ActorValue::kRestoration) ||
											(spell->avEffectSetting->data.primaryAV != RE::ActorValue::kHealth &&
											 spell->avEffectSetting->data.secondaryAV != RE::ActorValue::kHealth)
										};
										if (notHealingSpell)
										{
											GlobalCoopData::AddSkillXP(p->controllerID, spellSkillAV, magickaCost);
										}
									}
								}

								// Directly place down runes since casting runes with any non-P1 magic caster does not work.
								if (!p->isPlayer1 && Util::HasRuneProjectile(spell))
								{
									CastRuneProjectile(spell);
								}
							}
						}
						else
						{
							// No need for magicka check for powers.
							magicCaster->CastSpellImmediate(spell, false, nullptr, 1.0f, false, 0.0f, coopActor.get());
							magicCaster->SpellCast(true, 0, spell);
						}
					}
				}
			}
			else
			{
				// Start casting hand slot spell.
				bool isInLHCastingAnim = false;
				bool isInRHCastingAnim = false;
				coopActor->GetGraphVariableBool("IsCastingLeft", isInLHCastingAnim);
				coopActor->GetGraphVariableBool("IsCastingRight", isInRHCastingAnim);
				// Cast once casting animation starts, if requested.
				if ((a_index == EquipIndex::kLeftHand && (!a_waitForCastingAnim || isInLHCastingAnim)) || 
					(a_index == EquipIndex::kRightHand && (!a_waitForCastingAnim || isInRHCastingAnim)))
				{
					if (a_shouldCastWithP1)
					{
						if (auto otherCaster = glob.player1Actor->GetMagicCaster(RE::MagicSystem::CastingSource::kOther); otherCaster)
						{
							// No magicka cost for P1.
							otherCaster->currentSpellCost = 0.0f;
							p->tm->UpdateAimTargetLinkedRefr(a_index);
							auto targetPtr = Util::GetRefrPtrFromHandle(p->tm->aimTargetLinkedRefrHandle);
							bool targetValidity = targetPtr && Util::IsValidRefrForTargeting(targetPtr.get());
							// Target the caster.
							if (targetPtr && targetPtr == coopActor)
							{
								targetPtr = glob.player1Actor;
							}

							// Cast at valid target.
							if (targetValidity)
							{
								otherCaster->CastSpellImmediate(spell, false, targetPtr.get(), 1.0f, false, 0.0f, coopActor.get());
							}
						}
					}
					else
					{
						// Set target.
						p->tm->UpdateAimTargetLinkedRefr(a_index);
						auto targetPtr = Util::GetRefrPtrFromHandle(p->tm->aimTargetLinkedRefrHandle);
						auto handCaster = coopActor->GetMagicCaster
						(
							a_index == EquipIndex::kLeftHand ? 
							RE::MagicSystem::CastingSource::kLeftHand : 
							RE::MagicSystem::CastingSource::kRightHand
						); 
						// Use hand caster.
						if (handCaster)
						{
							handCaster->CastSpellImmediate(spell, false, targetPtr ? targetPtr.get() : coopActor.get(), 1.0f, false, 0.0f, coopActor.get());
						}
					}
				}
			}
		}
		else
		{
			// Stop casting with caster.
			if (a_index == EquipIndex::kQuickSlotSpell || a_index == EquipIndex::kVoice) 
			{
				if (a_shouldCastWithP1)
				{
					if (auto otherCaster = glob.player1Actor->GetMagicCaster(RE::MagicSystem::CastingSource::kOther); otherCaster)
					{
						if (otherCaster->castingTimer > 0 || otherCaster->currentSpell == spell)
						{
							otherCaster->currentSpell = nullptr;
						}
					}
				}
				else
				{
					if (auto instantCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); instantCaster)
					{
						instantCaster->currentSpell = nullptr;
					}
				}
			}
			else if (a_index == EquipIndex::kLeftHand || a_index == EquipIndex::kRightHand)
			{
				if (a_shouldCastWithP1)
				{
					if (auto otherCaster = glob.player1Actor->GetMagicCaster(RE::MagicSystem::CastingSource::kOther); otherCaster)
					{
						if (otherCaster->castingTimer > 0 || otherCaster->currentSpell == spell)
						{
							otherCaster->currentSpell = nullptr;
						}
					}
				}
				else
				{
					auto handCaster = coopActor->GetMagicCaster
					(
						a_index == EquipIndex::kLeftHand ? 
						RE::MagicSystem::CastingSource::kLeftHand : 
						RE::MagicSystem::CastingSource::kRightHand
					); 
					if (handCaster)
					{
						handCaster->currentSpell = nullptr;
					}
				}
			}

			// Clear ranged attack package target.
			p->tm->ClearTarget(TargetActorType::kLinkedRefr);
		}
	}

	void PlayerActionManager::CastStaffSpell(RE::TESObjectWEAP* a_staff, const bool& a_isLeftHand, bool&& a_shouldCast)
	{
		// Stop or start casting a staff's spell directly in the requested hand.
		// NOTE: Companion players will not cast with the staff casting animations.
		// Haven't figured out how to trigger those animations in tandem with the cast yet.
		if (p->isPlayer1) 
		{
			// Fill staff enchantment if in god mode.
			if (p->isInGodMode)
			{
				if (a_staff->formEnchanting && a_staff->amountofEnchantment == 0.0f)
				{
					// Initially set to have enough charge for one cast each time this func is run.
					a_staff->amountofEnchantment = a_staff->formEnchanting->data.costOverride + 1.0f;
					auto inventory = coopActor->GetInventory();
					// Attempt to set to full charge below.
					// Game will reset the charge once P1 attempts to use the staff without running this func.
					if (inventory.contains(a_staff))
					{
						auto& invEntry = coopActor->GetInventory().at(a_staff).second;
						if (invEntry->extraLists)
						{
							for (auto& xList : *invEntry->extraLists)
							{
								if (xList)
								{
									auto xEnch = xList->GetByType<RE::ExtraEnchantment>();
									if (xEnch && xEnch->enchantment && xEnch->charge != 0)
									{
										// Set to max charge level.
										a_staff->amountofEnchantment = xEnch->charge;
									}
								}
							}
						}
					}
				}
			}

			if (a_shouldCast) 
			{
				// Mounted attacks and staff casting do not require toggling AI driven.
				QueueP1ButtonEvent(a_isLeftHand ? InputAction::kAttackLH : InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kPressAndHold, 0.0f, false);
				// Update staff usage globals.
				if (a_isLeftHand)
				{
					usingLHStaff->value = 1.0f;
				}
				else
				{
					usingRHStaff->value = 1.0f;
				}
			}
			else
			{
				// Mounted attacks and staff casting do not require toggling AI driven.
				QueueP1ButtonEvent(a_isLeftHand ? InputAction::kAttackLH : InputAction::kAttackRH, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kRelease, 0.0f, false);
				// Update staff usage globals.
				if (a_isLeftHand)
				{
					usingLHStaff->value = 0.0f;
				}
				else
				{
					usingRHStaff->value = 0.0f;
				}
			}
		}
		else
		{
			if (a_shouldCast) 
			{
				if (auto staffEnchSpell = a_staff->formEnchanting; staffEnchSpell)
				{
					auto handCaster = coopActor->magicCasters[a_isLeftHand ? !RE::MagicSystem::CastingSource::kLeftHand : !RE::MagicSystem::CastingSource::kRightHand]; 
					if (handCaster)
					{
						// Update time since last cast.
						auto& secsSinceLastCast = a_isLeftHand ? secsSinceLastLHStaffCast : secsSinceLastRHStaffCast;
						auto& castTP = a_isLeftHand ? p->lastStaffLHCastTP : p->lastStaffRHCastTP;
						secsSinceLastCast = Util::GetElapsedSeconds(castTP);
						bool concSpell = staffEnchSpell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration;
						// Cast at an interval equal to the staff's spell charge time.
						float minChargeTime = max(0.75f, staffEnchSpell->GetChargeTime());
						if (concSpell || secsSinceLastCast > minChargeTime)
						{
							castTP = SteadyClock::now();
							std::optional<double> enchCharge = -1.0;
							// Does not update outside of the player's inventory for some reason,
							// so the staff's charge will stay at whatever value was cached
							// the last time the co-op companion opened their inventory.
							if (p->isInGodMode)
							{
								// Get charge level from staff directly.
								enchCharge = a_staff->amountofEnchantment;
							}
							else if (auto inventory = coopActor->GetInventory(); inventory.contains(a_staff))
							{
								// Get charge level from inventory entry.
								auto& invEntry = inventory.at(a_staff).second;
								enchCharge = invEntry->GetEnchantmentCharge();
							}

							if (enchCharge > 0.0f)
							{
								auto targetPtr = Util::GetActorPtrFromHandle(p->tm->GetRangedTargetActor());
								bool targetValidity = targetPtr && Util::IsValidRefrForTargeting(targetPtr.get());
								// Cast spell immediately (no animations.)
								handCaster->desiredTarget = targetValidity ? targetPtr->GetHandle() : coopActor->GetHandle();
								handCaster->SetSkipCheckCast();
								handCaster->CastSpellImmediate(staffEnchSpell, false, targetValidity ? targetPtr.get() : coopActor.get(), 1.0f, false, 0.0f, nullptr);
								// Update staff usage globals.
								if (a_isLeftHand)
								{
									usingLHStaff->value = 1.0f;
								}
								else
								{
									usingRHStaff->value = 1.0f;
								}
							}
							else
							{
								// Notify the player that their staff has insufficient charge.
								RE::DebugNotification(fmt::format("{} has insufficient charge", a_staff->GetName()).c_str(), "MAGFailSD");
							}
						}
					}
				}

				// Set weapon mults, if necessary.
				lastAttackingHand = a_isLeftHand ? HandIndex::kLH : HandIndex::kRH;
				SetAttackDamageMult();
			}
			else
			{
				// Clear hand caster spell when done casting.
				auto handCaster = coopActor->magicCasters[a_isLeftHand ? !RE::MagicSystem::CastingSource::kLeftHand : !RE::MagicSystem::CastingSource::kRightHand];
				if (handCaster) 
				{
					handCaster->currentSpell = nullptr;
				}

				// Interrupt cast and reset staff cast globals.
				coopActor->InterruptCast(false);
				if (a_isLeftHand)
				{
					usingLHStaff->value = 0.0f;
				}
				else
				{
					usingRHStaff->value = 0.0f;
				}
			}
			
		}
	}

	void PlayerActionManager::ChainAndSendP1ButtonEvents()
	{
		// Chain queued P1 input events before sending them.
		// Toggle AI driven as needed.

		auto bsInputMgr = RE::BSInputDeviceManager::GetSingleton();
		if (!bsInputMgr)
		{
			logger::error("[PAM] ERR: ChainAndSendP1ButtonEvents: {}: Could not get device input manager.", coopActor->GetName());
			return;
		}

		if (queuedP1ButtonEvents.size() > 0) 
		{
			// Link em up.
			for (uint32_t i = 0; i < queuedP1ButtonEvents.size() - 1; ++i)
			{
				if (queuedP1ButtonEvents[i] && queuedP1ButtonEvents[i].get() && queuedP1ButtonEvents[i + 1] && queuedP1ButtonEvents[i + 1].get()) 
				{
					(*(queuedP1ButtonEvents[i].get()))->next = *(queuedP1ButtonEvents[i + 1].get());
				}
			}

			// Certain actions do not trigger or terminate properly when the DontMove flag is set on P1.
			// For example, P1 cannot cast if DontMove is set while the cast bind is pressed,
			// or will continue casting if DontMove is set while the cast bind is released.
			p->mm->SetDontMove(false);
			if (queuedP1ButtonEvents[0] && queuedP1ButtonEvents[0].get()) 
			{
				// '1C0DA' pad means toggle AI driven.
				if ((*(queuedP1ButtonEvents[0].get()))->AsIDEvent()->pad24 == 0x1C0DA)
				{
					// P1 is motion driven here until all events are processed.
					sendingP1MotionDrivenEvents = true;
					Util::SetPlayerAIDriven(false);
					bsInputMgr->lock.Lock();
					bsInputMgr->SendEvent(queuedP1ButtonEvents[0].get());
					bsInputMgr->lock.Unlock();
				}
				else
				{
					bsInputMgr->lock.Lock();
					bsInputMgr->SendEvent(queuedP1ButtonEvents[0].get());
					bsInputMgr->lock.Unlock();
				}
			}

			// Clear out padding before freeing input event.
			for (auto& ptr : queuedP1ButtonEvents) 
			{
				if (ptr && ptr.get() && (*ptr.get())->AsIDEvent()) 
				{
					(*ptr.get())->AsIDEvent()->pad24 = 0x0;
				}

				ptr.release();
			}

			queuedP1ButtonEvents.clear();
		}
		else
		{
			// No queued events, so all have been sent and handled.
			sendingP1MotionDrivenEvents = false;
		}
	}

	void PlayerActionManager::CheckForCoopCompanionLevelUps()
	{
		// First, check if the party has leveled up through P1, and update all players' saved levels to match.
		// Then, check if this companion player can level up any skills, 
		// and if so, trigger a skill level up message through P1. 
		// Then, add skill XP to P1's total XP.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto actorValueList = RE::ActorValueList::GetSingleton();
		// Enderal does not use default skill curve data.
		if (ALYSLC::EnderalCompat::g_enderalSSEInstalled || !p1 || !actorValueList || p->isPlayer1) 
		{
			return;
		}

		// Should not check for level ups while modifying stats in the Stats/LevelUp menus or when AVs are copied over to P1.
		if (auto ui = RE::UI::GetSingleton(); ui) 
		{
			if (ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::LevelUpMenu::MENU_NAME) || 
				glob.copiedPlayerDataTypes.all(CopyablePlayerDataTypes::kSkillsAndHMS)) 
			{
				return;
			}
		}

		// Get skill curve exponent.
		float skillCurveExp = 1.95f;
		auto valueOpt = Util::GetGameSettingFloat("fSkillUseCurve");
		if (valueOpt.has_value())
		{
			skillCurveExp = valueOpt.value();
		}

		// Skill XP calculation formulas from UESP:
		// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
		if (auto p1Skills = p1->skills; p1Skills)
		{
			auto& skillIncList = glob.serializablePlayerData.at(coopActor->formID)->skillLevelIncreasesList;
			auto& skillXPList = glob.serializablePlayerData.at(coopActor->formID)->skillXPList;
			float levelUpThreshold = 0.0f;
			RE::ActorValue currentAV = RE::ActorValue::kNone;
			Skill currentSkill = Skill::kTotal;
			// Check each skill, which is mapped to an AV.
			for (auto i = 0; i < Skill::kTotal; ++i)
			{
				currentSkill = static_cast<Skill>(i);
				currentAV = glob.SKILL_TO_AV_MAP.contains(currentSkill) ? glob.SKILL_TO_AV_MAP.at(currentSkill) : RE::ActorValue::kNone;
				if (const auto avInfo = actorValueList->GetActorValue(currentAV); avInfo && avInfo->skill)
				{
					const auto skill = avInfo->skill;
					// Saved player XP for this skill.
					float& skillXP = skillXPList[i];
					const float avLvl = coopActor->GetBaseActorValue(currentAV);
					// XP required to level up this skill.
					levelUpThreshold = skill->improveMult * pow((avLvl), skillCurveExp) + skill->improveOffset;
					// Can level up.
					if (skillXP >= levelUpThreshold)
					{
						// Trigger skill level up message.
						bool succ = Util::TriggerFalseSkillLevelUp(currentAV, currentSkill, glob.AV_TO_SKILL_NAME_MAP.at(currentAV), avLvl + 1);
						if (succ)
						{
							// REMOVE when done debugging.
							logger::debug("[PAM] CheckForLevelUps: {} leveled up Skill {} from level {} to {}. Skill inc list entry goes from {} to {}. XP now resets to {} from {}, threshold was {}",
								coopActor->GetName(),
								Util::GetActorValueName(currentAV),
								avLvl,
								avLvl + 1,
								skillIncList[i], skillIncList[i] + 1,
								skillXP - levelUpThreshold, skillXP, levelUpThreshold);

							// Notify the player of the level up through the crosshair text.
							p->tm->SetCrosshairMessageRequest(
								CrosshairMessageType::kGeneralNotification,
								fmt::format("P{}: <font color=\"#E66100\">Leveled up '{}' to [{}]</font>", playerID + 1, glob.AV_TO_SKILL_NAME_MAP.at(currentAV), avLvl + 1),
								{ CrosshairMessageType::kNone, CrosshairMessageType::kStealthState, CrosshairMessageType::kTargetSelection },
								Settings::fSecsBetweenDiffCrosshairMsgs * 2.0f);

							// Set to XP overshoot amount after level up.
							skillXP = skillXP - levelUpThreshold;
							// Increment saved skill increments entry.
							skillIncList[i]++;
							// Increment player skill AV.
							coopActor->SetBaseActorValue(currentAV, avLvl + 1);

							// From UESP:
							// https://en.uesp.net/wiki/Skyrim:Leveling#Level_XP
							// Character XP gained = Skill level acquired * fXPPerSkillRank
							float fXPPerSkillRank = 1.0f / glob.livingPlayers;
							auto valueOpt = Util::GetGameSettingFloat("fXPPerSkillRank");
							if (valueOpt.has_value())
							{
								// Already set during co-op initialization
								// Do not need to scale by inverse of active player count again,
								// so just use the unmodified retrieved value.
								fXPPerSkillRank = valueOpt.value();
							}

							// Update level XP through P1.
							// Do not increase player levels until they opt to level up through the LevelUpMenu.
							const float newLevelXP = p1Skills->data->xp + fXPPerSkillRank * (avLvl + 1);

							// REMOVE when done debugging.
							logger::debug("[PAM] {}: about to add {} XP to player 1's level XP. XP is now {}, threshold: {}, fXPPerSkillRank: {}.",
								coopActor->GetName(), newLevelXP - p1Skills->data->xp, p1Skills->data->xp, p1Skills->data->levelThreshold, fXPPerSkillRank);
							p1Skills->data->xp = newLevelXP;
							
							// Level XP is shared among all players, so update every serialized value.
							for (auto& [_, data] : glob.serializablePlayerData)
							{
								data->levelXP = newLevelXP;
							}
						}
					}
				}
			}
		}
	}

	void PlayerActionManager::CheckIfPerformingCombatSkills()
	{
		// For keeping track of what skills this player can level based on
		// what combat-related actions they are performing.
		// Also will be useful for eventual papyrus framework.

		if (isWeaponAttack)
		{
			// Attacking with a weapon.
			if (p->em->Has2HMeleeWeapEquipped())
			{
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kTwoHanded);
			}
			else if (p->em->Has2HRangedWeapEquipped())
			{
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kArchery);
			}
			else if (p->em->HasRHWeapEquipped() && lastAttackingHand == HandIndex::kRH)
			{
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kOneHandedRH);
			}
			else if (p->em->HasLHWeapEquipped() && lastAttackingHand == HandIndex::kLH)
			{
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kOneHandedLH);
			}
			else if (p->em->HasLHWeapEquipped() && p->em->HasRHWeapEquipped() && lastAttackingHand == HandIndex::kBoth)
			{
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kOneHandedLH);
				perfSkillIncCombatActions.set(SkillIncCombatActionType::kOneHandedRH);
			}
		}
		else
		{
			// Clear all when not attacking with a weapon.
			if (perfSkillIncCombatActions.any(
				SkillIncCombatActionType::kArchery, 
				SkillIncCombatActionType::kOneHandedLH,
				SkillIncCombatActionType::kOneHandedRH, 
				SkillIncCombatActionType::kTwoHanded))
			{
				perfSkillIncCombatActions.reset
				(
					SkillIncCombatActionType::kArchery, SkillIncCombatActionType::kOneHandedLH,
					SkillIncCombatActionType::kOneHandedRH, SkillIncCombatActionType::kTwoHanded
				);
			}
		}

		if (isInCastingAnim)
		{
			// Casting with the RH. Get associated skill to determine which combat action bit to set.
			if (isInCastingAnimRH)
			{
				if (auto rhSpell = p->em->GetRHSpell(); rhSpell && rhSpell->avEffectSetting)
				{
					switch (rhSpell->avEffectSetting->data.associatedSkill)
					{
					case RE::ActorValue::kAlteration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kAlterationSpellRH);
						break;
					}
					case RE::ActorValue::kConjuration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kConjurationSpellRH);
						break;
					}
					case RE::ActorValue::kDestruction:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kDestructionSpellRH);
						break;
					}
					case RE::ActorValue::kIllusion:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kIllusionSpellRH);
						break;
					}
					case RE::ActorValue::kRestoration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kRestorationSpellRH);
						break;
					}
					default:
						break;
					}
				}
			}
			else
			{
				// Not casting, so clear all RH action bits.
				if (perfSkillIncCombatActions.any(
					SkillIncCombatActionType::kAlterationSpellRH,
					SkillIncCombatActionType::kConjurationSpellRH,
					SkillIncCombatActionType::kDestructionSpellRH,
					SkillIncCombatActionType::kIllusionSpellRH,
					SkillIncCombatActionType::kRestorationSpellRH))
				{
					perfSkillIncCombatActions.reset
					(
						SkillIncCombatActionType::kAlterationSpellRH,
						SkillIncCombatActionType::kConjurationSpellRH,
						SkillIncCombatActionType::kDestructionSpellRH,
						SkillIncCombatActionType::kIllusionSpellRH,
						SkillIncCombatActionType::kRestorationSpellRH
					);
				}
			}

			// Casting with the LH. Get associated skill to determine which combat action bit to set.
			if (isInCastingAnimLH)
			{
				if (auto lhSpell = p->em->GetLHSpell(); lhSpell && lhSpell->avEffectSetting)
				{
					switch (lhSpell->avEffectSetting->data.associatedSkill)
					{
					case RE::ActorValue::kAlteration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kAlterationSpellLH);
						break;
					}
					case RE::ActorValue::kConjuration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kConjurationSpellLH);
						break;
					}
					case RE::ActorValue::kDestruction:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kDestructionSpellLH);
						break;
					}
					case RE::ActorValue::kIllusion:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kIllusionSpellLH);
						break;
					}
					case RE::ActorValue::kRestoration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kRestorationSpellLH);
						break;
					}
					default:
						break;
					}
				}
			}
			else
			{
				// Not casting, so clear all LH action bits.
				if (perfSkillIncCombatActions.any(
					SkillIncCombatActionType::kAlterationSpellLH,
					SkillIncCombatActionType::kConjurationSpellLH,
					SkillIncCombatActionType::kDestructionSpellLH,
					SkillIncCombatActionType::kIllusionSpellLH,
					SkillIncCombatActionType::kRestorationSpellLH))
				{
					perfSkillIncCombatActions.reset
					(
						SkillIncCombatActionType::kAlterationSpellLH,
						SkillIncCombatActionType::kConjurationSpellLH,
						SkillIncCombatActionType::kDestructionSpellLH,
						SkillIncCombatActionType::kIllusionSpellLH,
						SkillIncCombatActionType::kRestorationSpellLH
					);
				}
			}
		}
		else
		{
			// Not casting at all, so clear all LH and RH action bits.
			if (perfSkillIncCombatActions.any(
				SkillIncCombatActionType::kAlterationSpellLH, SkillIncCombatActionType::kAlterationSpellRH,
				SkillIncCombatActionType::kConjurationSpellLH, SkillIncCombatActionType::kConjurationSpellRH,
				SkillIncCombatActionType::kDestructionSpellLH, SkillIncCombatActionType::kDestructionSpellRH,
				SkillIncCombatActionType::kIllusionSpellLH, SkillIncCombatActionType::kIllusionSpellRH,
				SkillIncCombatActionType::kRestorationSpellLH, SkillIncCombatActionType::kRestorationSpellRH))
			{
				perfSkillIncCombatActions.reset
				(
					SkillIncCombatActionType::kAlterationSpellLH, SkillIncCombatActionType::kAlterationSpellRH,
					SkillIncCombatActionType::kConjurationSpellLH, SkillIncCombatActionType::kConjurationSpellRH,
					SkillIncCombatActionType::kDestructionSpellLH, SkillIncCombatActionType::kDestructionSpellRH,
					SkillIncCombatActionType::kIllusionSpellLH, SkillIncCombatActionType::kIllusionSpellRH,
					SkillIncCombatActionType::kRestorationSpellLH, SkillIncCombatActionType::kRestorationSpellRH
				);
			}
		}

		// Using the block skill.
		if (isBlocking)
		{
			perfSkillIncCombatActions.set(SkillIncCombatActionType::kBlock);
		}
		else
		{
			perfSkillIncCombatActions.reset(SkillIncCombatActionType::kBlock);
		}

		// If performing a quick slot cast, get the spell's skill and set/unset the corresponding combat action.
		if (auto instantCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); instantCaster && IsPerforming(InputAction::kQuickSlotCast))
		{
			if (p->em->quickSlotSpell)
			{
				if (p->em->quickSlotSpell->avEffectSetting)
				{
					switch (p->em->quickSlotSpell->avEffectSetting->data.associatedSkill)
					{
					case RE::ActorValue::kAlteration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kAlterationSpellQS);
						break;
					}
					case RE::ActorValue::kConjuration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kConjurationSpellQS);
						break;
					}
					case RE::ActorValue::kDestruction:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kDestructionSpellQS);
						break;
					}
					case RE::ActorValue::kIllusion:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kIllusionSpellQS);
						break;
					}
					case RE::ActorValue::kRestoration:
					{
						perfSkillIncCombatActions.set(SkillIncCombatActionType::kRestorationSpellQS);
						break;
					}
					default:
						break;
					}
				}
			}
		}
		else if (perfSkillIncCombatActions.any(
				 SkillIncCombatActionType::kAlterationSpellQS,
				 SkillIncCombatActionType::kConjurationSpellQS,
				 SkillIncCombatActionType::kDestructionSpellQS,
				 SkillIncCombatActionType::kIllusionSpellQS,
				 SkillIncCombatActionType::kRestorationSpellQS))
		{
			// Clear all once the player stops quick slot casting.
			perfSkillIncCombatActions.reset
			(
				SkillIncCombatActionType::kAlterationSpellQS,
				SkillIncCombatActionType::kConjurationSpellQS,
				SkillIncCombatActionType::kDestructionSpellQS,
				SkillIncCombatActionType::kIllusionSpellQS,
				SkillIncCombatActionType::kRestorationSpellQS
			);
		}
	}

	void PlayerActionManager::CopyOverSharedSkillAVs()
	{
		// Copy over the highest skill AV level among all players for each shared skill.

		for (const auto& av : glob.SHARED_SKILL_AVS_SET)
		{
			if (auto value = GlobalCoopData::GetHighestSharedAVLevel(av); value != -1.0f) 
			{
				coopActor->SetBaseActorValue(av, value);
			}
		}
	}

	void PlayerActionManager::EvaluatePackage() 
	{
		// Evaluate the package atop this player's package stack.
		// Interrupt cast for companion players as needed.

		// REMOVE when done debugging.
		logger::debug("[PAM] EvaluatePackage: {}: Is casting: {}, LH: {}, RH: {}, 2H: {}, Dual: {}, Shout: {}, Voice: {}, spells + shout: LH: {}, RH: {}, Voice/Shout: {}",
			coopActor->GetName(),
			isInCastingAnim,
			castingGlobVars[!CastingGlobIndex::kLH]->value,
			castingGlobVars[!CastingGlobIndex::kRH]->value,
			castingGlobVars[!CastingGlobIndex::k2H]->value,
			castingGlobVars[!CastingGlobIndex::kDual]->value,
			castingGlobVars[!CastingGlobIndex::kShout]->value,
			castingGlobVars[!CastingGlobIndex::kVoice]->value,
			p->em->equippedForms[!EquipIndex::kLeftHand] ? 
			p->em->equippedForms[!EquipIndex::kLeftHand]->GetName() : "NONE",
			p->em->equippedForms[!EquipIndex::kRightHand] ? 
			p->em->equippedForms[!EquipIndex::kRightHand]->GetName() : "NONE",
			p->em->equippedForms[!EquipIndex::kVoice] ?
			p->em->equippedForms[!EquipIndex::kVoice]->GetName() : "NONE");

		const bool isInCombat = coopActor->IsInCombat();
		// Package stack to modify before evaluation (default, or combat override if in combat).
		auto& packageStack = 
		(
			isInCombat ?
			packageStackMap[PackageIndex::kCombatOverride]->forms :
			packageStackMap[PackageIndex::kDefault]->forms
		);
		// Default package for each package stack.
		auto defPackage =
		(
			isInCombat ?
			glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kCombatOverride] :
			glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kDefault]
		);
		// Ranged attack and interaction packages to use 
		// if the companion player is casting or attempting to use furniture.
		auto rangedAttackPackage = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kRangedAttack];
		auto interactionPackage = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kSpecialInteraction];

		// Interrupt cast, if necessary, when the current package atop the stack is the default or ranged attack package.
		// Just evaluate otherwise.
		bool shouldInterruptCast = false;
		if (packageStack[0] == defPackage || packageStack[0] == rangedAttackPackage) 
		{
			auto lhCasting = castingGlobVars[!CastingGlobIndex::kLH];
			auto rhCasting = castingGlobVars[!CastingGlobIndex::kRH];
			auto casting2H = castingGlobVars[!CastingGlobIndex::k2H];
			auto dualCasting = castingGlobVars[!CastingGlobIndex::kDual];
			auto shouting = castingGlobVars[!CastingGlobIndex::kShout];
			auto voiceCasting = castingGlobVars[!CastingGlobIndex::kVoice];
			// Player was/is trying to cast a spell with the ranged attack package if any of the globals are set.
			if (lhCasting->value == 0.0f && rhCasting->value == 0.0f && casting2H->value == 0.0f &&
				dualCasting->value == 0.0f && shouting->value == 0.0f && voiceCasting->value == 0.0f)
			{
				// Start cast momentarily and then evaluate again after clearing all the globals.
				// Is more consistent with interrupting casting this way.
				shouldInterruptCast = true;
				lhCasting->value =
				rhCasting->value =
				casting2H->value =
				dualCasting->value =
				shouting->value =
				voiceCasting->value = 1.0f;

				// Evaluate as casting.
				coopActor->EvaluatePackage(true, false);

				// Switch to the default package.
				packageStack[0] = defPackage;
				// Clear out globals.
				lhCasting->value =
				rhCasting->value =
				casting2H->value =
				dualCasting->value =
				shouting->value =
				voiceCasting->value = 0.0f;
			}
		}

		// Reset AI when trying to interrupt casting.
		coopActor->EvaluatePackage(true, shouldInterruptCast);

		// Signal that the package has been evaluated.
		// NOTE: BUG with cast interruption: 
		// When the cast stops, the game forces the player to rotate 
		// to their last orientation if the cast was stopped when moving.
		// Starting and stopping casting from a standstill fixes this bug.
		if (!p->isPlayer1 && shouldInterruptCast)
		{
			coopActor->InterruptCast(false);
		}
	}

	void PlayerActionManager::ExpendMagicka()
	{
		// Reduce the companion player's magicka based on what spell(s) they are casting.
		
		// Game already handles player 1's magicka expenditure and regen.
		if (p->isPlayer1)
		{
			return;
		}

		bool dualCasting = avcam->actionsInProgress.all(AVCostAction::kCastDual);
		bool usingLHSpell = avcam->actionsInProgress.all(AVCostAction::kCastLeft);
		bool usingRHSpell = avcam->actionsInProgress.all(AVCostAction::kCastRight);
		bool isCasting = dualCasting || usingLHSpell || usingRHSpell;
		if (isCasting)
		{

			// Get LH and RH spells.
			RE::SpellItem* lhSpell = p->em->GetLHSpell();
			RE::SpellItem* rhSpell = p->em->GetRHSpell();
			// LH/RH spell associated skills.
			RE::ActorValue magicSkillLH = RE::ActorValue::kNone;
			RE::ActorValue magicSkillRH = RE::ActorValue::kNone;

			// Negative cost if expenditure conditions met.
			float deltaMagCost = 0.0f;
			float lhMagCostDelta = 0.0f;
			float rhMagCostDelta = 0.0f;
			// Requested AV cost actions.
			auto& reqActionsSet = avcam->reqActionsSet;

			// One hand or both hands cast.
			if (!dualCasting)
			{
				if (usingRHSpell && rhSpell)
				{
					auto castingType = rhSpell->GetCastingType();
					// Check and update magicka every frame if casting a concentration spell,
					// otherwise, update magicka once on spell cast.
					if (castingType == CastingType::kConcentration)
					{
						float newCastDuration = Util::GetElapsedSeconds(p->lastRHCastStartTP);
						if (reqActionsSet.contains(AVCostAction::kCastRight))
						{
							// Use cached cost and multiply by frametime.
							rhMagCostDelta = -avcam->GetCost(AVCostAction::kCastRight) * *g_deltaTimeRealTime;
						}
						else
						{
							// No cached cost, so recalculate.
							rhMagCostDelta = GetSpellDeltaMagickaCost(rhSpell) * *g_deltaTimeRealTime;
						}

						// Update cast duration.
						rhCastDuration = newCastDuration;
					}
					else
					{
						if (reqActionsSet.contains(AVCostAction::kCastRight))
						{
							// Use cached cost.
							rhMagCostDelta = -avcam->GetCost(AVCostAction::kCastRight);
						}
						else
						{
							// No cached cost, so recalculate.
							rhMagCostDelta = GetSpellDeltaMagickaCost(rhSpell);
						}

						// Done casting once FNF spell is fired off.
						avcam->RemoveStartedAction(AVCostAction::kCastRight);
						rhCastDuration = 0.0f;
						
						// Directly place down runes since casting runes with any non-P1 magic caster does not work.
						// Player ranged package still plays casting animations but no spell is released, 
						// so we do it here once the begin cast animation event fires.
						if (Util::HasRuneProjectile(rhSpell))
						{
							CastRuneProjectile(rhSpell);
						}
					}

					// Get associated skill for the spell.
					if (rhSpell->avEffectSetting)
					{
						magicSkillRH = rhSpell->avEffectSetting->data.associatedSkill;
					}
				}

				if (usingLHSpell && lhSpell)
				{
					auto castingType = lhSpell->GetCastingType();
					// Check and update magicka every frame if casting a concentration spell,
					// otherwise, update magicka once on spell cast.
					if (castingType == CastingType::kConcentration)
					{
						float newCastDuration = Util::GetElapsedSeconds(p->lastLHCastStartTP);
						if (reqActionsSet.contains(AVCostAction::kCastLeft))
						{
							// Use cached cost and multiply by frametime.
							lhMagCostDelta = -avcam->GetCost(AVCostAction::kCastLeft) * *g_deltaTimeRealTime;
						}
						else
						{
							// No cached cost, so recalculate.
							lhMagCostDelta = GetSpellDeltaMagickaCost(lhSpell) * *g_deltaTimeRealTime;
						}

						// Update cast duration.
						lhCastDuration = newCastDuration;
					}
					else
					{
						if (reqActionsSet.contains(AVCostAction::kCastLeft))
						{
							// Use cached cost.
							lhMagCostDelta = -avcam->GetCost(AVCostAction::kCastLeft);
						}
						else
						{
							// No cached cost, so recalculate.
							lhMagCostDelta = GetSpellDeltaMagickaCost(lhSpell);
						}

						// Done casting once FNF spell is fired off.
						avcam->RemoveStartedAction(AVCostAction::kCastLeft);
						lhCastDuration = 0.0f;

						// Directly place down runes since casting runes with any non-P1 magic caster does not work.
						// Player ranged package still plays casting animations but no spell is released, 
						// so we do it here once the begin cast animation event fires.
						if (Util::HasRuneProjectile(lhSpell))
						{
							CastRuneProjectile(lhSpell);
						}
					}

					// Get associated skill for the spell.
					if (lhSpell->avEffectSetting)
					{
						magicSkillLH = lhSpell->avEffectSetting->data.associatedSkill;
					}
				}

				// Add LH and RH costs together.
				deltaMagCost = lhMagCostDelta + rhMagCostDelta;
			}
			else
			{
				// TODO: Dual Casting.
				// Check if two spells are equipped.
				if (lhSpell && rhSpell)
				{
					// Since both spells should be the same here, use the RH one.
					auto castingType = rhSpell->GetCastingType();
					// Check and update magicka every frame if casting a concentration spell,
					// otherwise, update magicka once on spell cast.
					// x2.8 modifier when dual casting:
					// https://en.uesp.net/wiki/Skyrim:Magic_Overview#Dual-Casting
					if (castingType == CastingType::kConcentration)
					{
						float newCastDuration = Util::GetElapsedSeconds(p->lastRHCastStartTP);
						if (reqActionsSet.contains(AVCostAction::kCastDual))
						{
							// Use cached cost and multiply by frametime.
							deltaMagCost = -avcam->GetCost(AVCostAction::kCastDual) * *g_deltaTimeRealTime;
						}
						else
						{
							// No cached cost, so recalculate.
							deltaMagCost = GetSpellDeltaMagickaCost(rhSpell) * 2.8f;
						}

						// Same duration for both hands since the spells are cast at the same time.
						rhCastDuration = lhCastDuration = newCastDuration;
					}
					else
					{
						if (reqActionsSet.contains(AVCostAction::kCastDual))
						{
							// Use cached cost and multiply by frametime.
							deltaMagCost = -avcam->GetCost(AVCostAction::kCastDual);
						}
						else
						{
							// No cached cost, so recalculate.
							deltaMagCost = GetSpellDeltaMagickaCost(rhSpell) * 2.8f;
						}

						// Done casting once FNF spell is fired off.
						avcam->RemoveStartedAction(AVCostAction::kCastDual);
						rhCastDuration = lhCastDuration = 0.0f;

						// Directly place down runes since casting runes with any non-P1 magic caster does not work.
						// Player ranged package still plays casting animations but no spell is released, 
						// so we do it here once the begin cast animation event fires.
						if (Util::HasRuneProjectile(lhSpell)) 
						{
							logger::debug("[PAM] ExpendMagicka: {}: dual cast LH rune spell at target location.", coopActor->GetName());
							CastRuneProjectile(lhSpell);
						}

						if (Util::HasRuneProjectile(rhSpell))
						{
							logger::debug("[PAM] ExpendMagicka: {}: dual cast LH rune spell at target location.", coopActor->GetName());
							CastRuneProjectile(rhSpell);
						}
					}

					// Same associated skill if dual casting.
					if (rhSpell->avEffectSetting)
					{
						magicSkillLH = magicSkillRH = rhSpell->avEffectSetting->data.associatedSkill;
					}
				}
			}

			// Only modify magicka if there was a computed delta and if the player is not in god mode.
			if (deltaMagCost < 0.0f && !p->isInGodMode)
			{
				ModifyAV(RE::ActorValue::kMagicka, deltaMagCost);
				// Player is out of magicka if after subtracting the cost, the player is left with 0 or less magicka.
				if (currentMagicka + deltaMagCost <= 0.0f)
				{
					logger::debug("[PAM] {} is out of magicka! Delta mag: {}", coopActor->GetName(), deltaMagCost);
					// Remove requested casting actions, since the player will no longer be casting.
					avcam->RemoveRequestedActions(AVCostAction::kCastDual, AVCostAction::kCastLeft, AVCostAction::kCastRight);
				}
			}

			// Add to player magic skill(s)' XP total(s) on cast.
			// Restoration spells that heal actors and destruction spells are excluded, 
			// as their base XP amount is determined by the amount of healed/damaged HP, 
			// not by the spell's magicka cost.
			// Only give XP if targeting a valid actor.
			// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
			if (Util::HandleIsValid(p->tm->GetRangedTargetActor()))
			{
				// Separate deltas for XP calc.
				if (!dualCasting)
				{
					if (rhSpell && glob.AV_TO_SKILL_MAP.contains(magicSkillRH))
					{
						// Ignore spells that do not grant XP on magicka expenditure.
						bool grantsXPOnAVChange = 
						{ 
							magicSkillRH == RE::ActorValue::kDestruction ||
							(magicSkillRH == RE::ActorValue::kRestoration &&
							(rhSpell->avEffectSetting->data.primaryAV == RE::ActorValue::kHealth ||
							 rhSpell->avEffectSetting->data.secondaryAV == RE::ActorValue::kHealth)) 
						};
						if (!grantsXPOnAVChange)
						{
							GlobalCoopData::AddSkillXP(controllerID, magicSkillRH, -rhMagCostDelta);
						}
					}

					if (lhSpell && glob.AV_TO_SKILL_MAP.contains(magicSkillLH))
					{
						// Ignore spells that do not grant XP on magicka expenditure.
						bool grantsXPOnAVChange = 
						{
							magicSkillLH == RE::ActorValue::kDestruction ||
							(magicSkillLH == RE::ActorValue::kRestoration &&
							(lhSpell->avEffectSetting->data.primaryAV == RE::ActorValue::kHealth ||
							 lhSpell->avEffectSetting->data.secondaryAV == RE::ActorValue::kHealth))
						};
						if (!grantsXPOnAVChange)
						{
							GlobalCoopData::AddSkillXP(controllerID, magicSkillLH, -lhMagCostDelta);
						}
					}
				}
				else
				{
					if (lhSpell && rhSpell && glob.AV_TO_SKILL_MAP.contains(magicSkillLH) && glob.AV_TO_SKILL_MAP.contains(magicSkillRH))
					{
						// Just check RH spell since both are the same.
						// Ignore spells that do not grant XP on magicka expenditure.
						bool grantsXPOnAVChange = 
						{
							magicSkillRH == RE::ActorValue::kDestruction ||
							(magicSkillRH == RE::ActorValue::kRestoration &&
							(rhSpell->avEffectSetting->data.primaryAV == RE::ActorValue::kHealth ||
							 rhSpell->avEffectSetting->data.secondaryAV == RE::ActorValue::kHealth))
						};
						if (!grantsXPOnAVChange)
						{
							GlobalCoopData::AddSkillXP(controllerID, magicSkillRH, -deltaMagCost);
						}
					}
				}
			}

		}
		else
		{
			// Reset cast durations and remove casting AV actions when not casting.
			rhCastDuration = lhCastDuration = 0.0f;
			avcam->RemoveRequestedActions(AVCostAction::kCastDual, AVCostAction::kCastLeft, AVCostAction::kCastRight);
		}
	}

	void PlayerActionManager::ExpendStamina() 
	{
		// Reduce the player's stamina based on what AV cost actions they are performing.
		// Not for player 1, unless mounted, as their stamina regen and cooldown are handled by the game already.
		
		// NOTE: All formulas (aside from sneak roll cost) are from UESP:
		// https://en.uesp.net/wiki/Skyrim:Stamina

		// New stamina to set.
		float newStamina = currentStamina;
		auto& actionsInProgress = avcam->actionsInProgress;
		auto& reqActionsSet = avcam->reqActionsSet;

		// Sprint and shield charge have a continuous cost while sprinting.
		// Do not remove request here, as the action is ongoing until an animation event triggers
		// or a player action completes.

		// Mount sprint not triggered by an animation, but rather by performing the sprint action.
		bool mountSprint = coopActor->IsOnMount() && IsPerforming(InputAction::kSprint);
		if (mountSprint || actionsInProgress.any(AVCostAction::kSprint))
		{
			// Get seconds since starting to sprint.
			float secsSinceLastCheck = Util::GetElapsedSeconds(p->expendSprintStaminaTP);
			p->expendSprintStaminaTP = SteadyClock::now();
			// Sprint cost is modified by the number of seconds sprinted. 
			if (reqActionsSet.contains(AVCostAction::kSprint))
			{
				// Cached cost.
				newStamina = currentStamina - avcam->GetCost(AVCostAction::kSprint) * secsSinceLastCheck;
			}
			else
			{
				// Recalculated cost.
				newStamina = currentStamina - (7.0f + 0.02f * p->em->GetWornWeight()) * secsSinceLastCheck;
			}
		}

		// NOTE: Since bash and power attacks are triggered via action command and idle,
		// their stamina cost is already accounted for automatically.
		// The stamina modification code should only be used if a power attack were triggered by an animation event,
		// which does not automatically modify the player's stamina.
		// No cost to handle here for now, but keeping the AV cost action removal lines to
		// allow the power attacking flag to reset properly.
		
		// One-time actions that can be removed right away once stamina is spent.
		if (actionsInProgress.all(AVCostAction::kBash) && reqActionsSet.contains(AVCostAction::kBash)) 
		{
			// Bash's stamina cost is handled by the game, so just remove the action request here.
			avcam->RemoveRequestedAction(AVCostAction::kBash);
		}

		if (actionsInProgress.all(AVCostAction::kDodge) && reqActionsSet.contains(AVCostAction::kDodge))
		{
			// Use our cached dodge cost.
			newStamina = currentStamina - avcam->GetCost(AVCostAction::kDodge);
			avcam->RemoveRequestedAction(AVCostAction::kDodge);
		}

		if (actionsInProgress.all(AVCostAction::kSneakRoll) && reqActionsSet.contains(AVCostAction::kSneakRoll))
		{
			// Use our cached roll cost.
			newStamina = currentStamina - avcam->GetCost(AVCostAction::kSneakRoll);
			avcam->RemoveRequestedAction(AVCostAction::kSneakRoll);
		}

		bool isPowerAttackDual = actionsInProgress.all(AVCostAction::kPowerAttackDual);
		bool isPowerAttackLeft = actionsInProgress.all(AVCostAction::kPowerAttackLeft);
		bool isPowerAttackRight = actionsInProgress.all(AVCostAction::kPowerAttackRight);
		if (isPowerAttackDual && reqActionsSet.contains(AVCostAction::kPowerAttackDual))
		{
			avcam->RemoveRequestedAction(AVCostAction::kPowerAttackDual);
		}

		if (isPowerAttackLeft && reqActionsSet.contains(AVCostAction::kPowerAttackLeft)) 
		{
			avcam->RemoveRequestedAction(AVCostAction::kPowerAttackLeft);
		}

		if (isPowerAttackRight && reqActionsSet.contains(AVCostAction::kPowerAttackRight)) 
		{
			avcam->RemoveRequestedAction(AVCostAction::kPowerAttackRight);
		}

		// No stamina cost if in god mode.
		if (!p->isInGodMode) 
		{
			if (!p->isPlayer1) 
			{
				// Handle companion player's stamina cooldown here.
				if (newStamina < 0.0f)
				{
					p->outOfStaminaTP = SteadyClock::now();
					p->lastStaminaCooldownCheckTP = SteadyClock::now();

					// Set total stamina regeneration cooldown.
					// Player cannot use stamina until this duration elapses.
					// Min of two seconds for sprint stamina cooldown,
					// and max of five seconds for all stamina cooldowns.
					if (isSprinting)
					{
						secsTotalStaminaRegenCooldown = min(2.0f + 0.02f * p->em->GetWornWeight(), 5.0f);
						// Stop the player from sprinting right after running out of stamina.
						p->coopActor->NotifyAnimationGraph("sprintStop");
					}
					else
					{
						// Scales down as regeneration rate multiplier increases.
						secsTotalStaminaRegenCooldown = min((0.0f - newStamina) / (baseStaminaRegenRateMult / 100.0f), 5.0f);
					}
				}

				// Set new stamina if changed and not at 0 stamina currently.
				if (newStamina != currentStamina && currentStamina > 0.0f)
				{
					ModifyAV(RE::ActorValue::kStamina, newStamina - currentStamina);
				}
			}
			else
			{
				// For P1, only mount-sprint stamina expenditure.
				if (newStamina != currentStamina && currentStamina > 0.0f)
				{
					ModifyAV(RE::ActorValue::kStamina, newStamina - currentStamina);
				}
			}
		}
		else if (secsTotalStaminaRegenCooldown != 0.0f)
		{
			// No stamina cooldown when in god mode.
			secsTotalStaminaRegenCooldown = 0.0f;
		}
	}

	void PlayerActionManager::FlashShoutMeter()
	{	
		p->taskInterface->AddUITask
		(
			[]() {
				auto ui = RE::UI::GetSingleton();
				if (!ui) 
				{
					return;
				}

				auto hudMenu = ui->GetMenu<RE::HUDMenu>(); 
				if (!hudMenu)
				{
					return;
				}

				auto view = hudMenu->uiMovie; 
				if (!view)
				{
					return;
				}

				RE::GFxValue shoutMeter;
				view->GetVariable(&shoutMeter, "_root.HUDMovieBaseInstance.ShoutMeter_mc");
				if (!shoutMeter.IsNull() && !shoutMeter.IsUndefined() && shoutMeter.IsObject())
				{
					shoutMeter.Invoke("FlashMeter");
				}

				view->Invoke("_root.HudMovieBaseInstance.FlashShoutMeter", nullptr, nullptr, 0);
			}
		);
	}

	const int32_t PlayerActionManager::GetActionPriority(const InputAction& a_action) const noexcept
	{
		// Returns an integer that represents the priority of the given action
		// when multiple actions are being compared before starting execution.
		// Larger integers indicate higher priority.
		
		// Why would this happen? I dunno, but it could.
		if (a_action == InputAction::kNone)
		{
			return 0.0f;
		}

		const auto& paInfo = glob.paInfoHolder;
		int32_t priority = 0;
		const auto& params = paParamsList[!a_action - !InputAction::kFirstAction];

		// Since actions are only inserted into the candidates queue when 
		// all their composing inputs are pressed, actions with
		// more composing inputs will be considered first.
		priority += params.composingInputs.size();

		// Actions composed of other actions should also be considered
		// before actions that are only composed of inputs.
		priority += static_cast<float>(params.composingPlayerActionsCount);

		// Consec tap, min hold time, lone action, then everything else.
		if (params.perfType == PerfType::kOnConsecTap) 
		{
			priority += 3.0f;
		}
		else if (params.triggerFlags.all(TriggerFlag::kMinHoldTime))
		{
			priority += 2.0f;
		}
		else if (params.triggerFlags.all(TriggerFlag::kLoneAction))
		{
			priority += 1.0f;
		}

		return priority;
	}

	RE::TESPackage* PlayerActionManager::GetCurrentPackage()
	{
		// Get the package on top of the currently-used package stack.

		const bool isInCombat = coopActor->IsInCombat();
		auto& packageStack = 
		( 
			isInCombat ?
			packageStackMap[PackageIndex::kCombatOverride]->forms :
			packageStackMap[PackageIndex::kDefault]->forms 
		);
		return packageStack[0] ? packageStack[0]->As<RE::TESPackage>() : nullptr;
	}

	RE::TESPackage* PlayerActionManager::GetDefaultPackage()
	{
		// Get the default package to run when out of/in combat.

		if (coopActor->IsInCombat()) 
		{
			return glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kCombatOverride];
		}
		else
		{
			return glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kDefault];
		}
	}

	float PlayerActionManager::GetPlayerActionInputHoldTime(const InputAction& a_action)
	{
		// Get the hold time for the given action based on the action's input held times,
		// currently performed or not.
		// -1.0 if action is invalid or has no composing inputs.

		if (a_action != InputAction::kNone) 
		{
			auto& paState = paStatesList[!a_action - !InputAction::kFirstAction];
			const auto& composingInputs = paState.paParams.composingInputs;
			if (!composingInputs.empty()) 
			{
				// If input ordering matters, simply check hold time of last input in the sequence,
				// which is the most recently pressed input.
				// Otherwise, return the hold time of most recently-pressed input by iterating through the sequence.
				if (paState.paParams.triggerFlags.all(TriggerFlag::kDoNotUseCompActionsOrdering)) 
				{
					// Out of sequence.
					float minHoldTime = FLT_MAX;
					for (auto input : composingInputs)
					{
						const auto& inputState = glob.cdh->GetInputState(controllerID, input);
						if (inputState.heldTimeSecs < minHoldTime)
						{
							minHoldTime = inputState.heldTimeSecs;
						}
					}

					return minHoldTime;
				}
				else
				{
					// In sequence.
					auto& lastInputIndex = composingInputs[composingInputs.size() - 1];
					const auto& inputState = glob.cdh->GetInputState(controllerID, lastInputIndex);

					return inputState.heldTimeSecs;
				}
			}
		}

		return 0.0f;
	}

	void PlayerActionManager::HandleAVExpenditure()
	{
		// Modify health, magicka, stamina actor values based on the player's ongoing AV action requests
		// and animation events that have triggered over the last frame.
		auto& actionsInProgress = avcam->actionsInProgress;
		auto& reqActionsSet = avcam->reqActionsSet;

		// Copy over the queue to unlock it faster for the hook thread adding animation events to the same queue.
		std::queue<std::pair<PerfAnimEventTag, uint16_t>> copiedQueue;
		{
			// REMOVE
			if (!avcam->perfAnimEventsQueue.empty()) 
			{
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("[PAM] HandleAVExpenditure: {}: Try to lock: 0x{:X}.", coopActor->GetName(), hash);
				{
					std::unique_lock<std::mutex> perfAnimQueueLock(avcam->perfAnimQueueMutex, std::try_to_lock);
					if (perfAnimQueueLock)
					{
						logger::debug("[PAM] HandleAVExpenditure: {}: Lock obtained: 0x{:X}.", coopActor->GetName(), hash);
						while (!avcam->perfAnimEventsQueue.empty())
						{
							const auto& perfAnimEvent = avcam->perfAnimEventsQueue.front();
							copiedQueue.push(std::pair<PerfAnimEventTag, uint16_t>(perfAnimEvent.first, perfAnimEvent.second));
							avcam->perfAnimEventsQueue.pop();
						}
					}
					else
					{
						// REMOVE
						logger::error("[PAM] ERR: HandleAVExpenditure: {}: failed to lock perf anim event queue. Returning.", coopActor->GetName());
						return;
					}
				}
			}
		}

		// Handle copied anim events.
		while (!copiedQueue.empty())
		{
			const auto& perfAnimEvent = copiedQueue.front();
			copiedQueue.pop();
			// No tag, ignore.
			if (perfAnimEvent.first == PerfAnimEventTag::kNone) 
			{
				continue;
			}

			// REMOVE when done debugging.
			logger::debug("[PAM] HandleAVExpenditure: {} received new anim event num {} ({}). Actions in progress: {}, queue size: {}",
				coopActor->GetName(), perfAnimEvent.second, perfAnimEvent.first, *avcam->actionsInProgress, copiedQueue.size());

			// Handle state update of requested actions first.
			// Set actions as in progress when the proper anim event is performed
			// and the action is not already executing.
			if (perfAnimEvent.first == PerfAnimEventTag::kPreHitFrame &&
				actionsInProgress.none(AVCostAction::kBash) &&
				reqActionsSet.contains(AVCostAction::kBash))
			{
				// Triggers on pre-hit frame if the action was requested.
				avcam->SetStartedAction(AVCostAction::kBash);
				isBashing = true;
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kCastLeft &&
					 actionsInProgress.none(AVCostAction::kCastLeft) &&
					 reqActionsSet.contains(AVCostAction::kCastLeft))
			{
				avcam->SetStartedAction(AVCostAction::kCastLeft);
				// Set cast start TP, reset duration, and set flag.
				p->lastLHCastStartTP = SteadyClock::now();
				lhCastDuration = 0.0f;
				isCastingLH = true;
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kCastRight &&
					 actionsInProgress.none(AVCostAction::kCastRight) &&
					 reqActionsSet.contains(AVCostAction::kCastRight))
			{
				avcam->SetStartedAction(AVCostAction::kCastRight);
				// Set cast start TP, reset duration, and set flag.
				p->lastRHCastStartTP = SteadyClock::now();
				rhCastDuration = 0.0f;
				isCastingRH = true;
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kDodgeStart &&
					 actionsInProgress.none(AVCostAction::kDodge) &&
					 reqActionsSet.contains(AVCostAction::kDodge))
			{
				avcam->SetStartedAction(AVCostAction::kDodge);
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kPreHitFrame)
			{
				// Triggers on pre-hit frame if the action was requested.
				bool powerAttackDualReq = 
				{ 
					reqActionsSet.contains(AVCostAction::kPowerAttackDual) &&
					actionsInProgress.none(AVCostAction::kPowerAttackDual) 
				};
				bool powerAttackLeftReq = 
				{ 
					reqActionsSet.contains(AVCostAction::kPowerAttackLeft) &&
					actionsInProgress.none(AVCostAction::kPowerAttackLeft) 
				};
				bool powerAttackRightReq = 
				{ 
					reqActionsSet.contains(AVCostAction::kPowerAttackRight) &&
					actionsInProgress.none(AVCostAction::kPowerAttackRight) 
				};

				isPowerAttacking = powerAttackDualReq || powerAttackLeftReq || powerAttackRightReq;
				if (powerAttackDualReq)
				{
					avcam->SetStartedAction(AVCostAction::kPowerAttackDual);
				}
				else if (powerAttackLeftReq)
				{
					avcam->SetStartedAction(AVCostAction::kPowerAttackLeft);
				}
				else if (powerAttackRightReq)
				{
					avcam->SetStartedAction(AVCostAction::kPowerAttackRight);
				}
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kSprintStart &&
					 actionsInProgress.none(AVCostAction::kSneakRoll) &&
					 reqActionsSet.contains(AVCostAction::kSneakRoll))
			{
				avcam->SetStartedAction(AVCostAction::kSneakRoll);
				isRolling = true;
			}
			else if (perfAnimEvent.first == PerfAnimEventTag::kSprintStart &&
					 actionsInProgress.none(AVCostAction::kSprint) &&
					 reqActionsSet.contains(AVCostAction::kSprint))
			{
				avcam->SetStartedAction(AVCostAction::kSprint);
				isSprinting = true;
			}

			// Set weapon mults when melee/ranged attack triggers.
			bool physWeapAttackStarted = 
			{
				perfAnimEvent.first == PerfAnimEventTag::kPreHitFrame ||
				perfAnimEvent.first == PerfAnimEventTag::kBowRelease
			};
			bool magAttackStarted = 
			{
				perfAnimEvent.first == PerfAnimEventTag::kCastLeft ||
				perfAnimEvent.first == PerfAnimEventTag::kCastRight
			};
			if ((!p->isPlayer1 && !attackDamageMultSet) && (physWeapAttackStarted || magAttackStarted))
			{
				SetAttackDamageMult();
			}

			// Reset weapon mults after melee attack stops.
			// NOTE: Ranged weapon or spell attack mult reset is handled on hit event.
			if (!p->isPlayer1 && attackDamageMultSet && !p->em->Has2HRangedWeapEquipped() && 
				perfAnimEvent.first == PerfAnimEventTag::kAttackStop)
			{
				ResetAttackDamageMult();
			}

			//=============================================================================================================
			// Update action(s) in progress to complete if the proper stop anim event is performed,
			// or, as failsafes, also set as complete if the request was removed (AV cost handled once, request fulfilled),
			// or if the action is no longer being performed but the action request/in-progress state
			// is still present (AV cost handled on hold).
			//=============================================================================================================

			//--------------------------
			// One-time AV expenditures.
			//--------------------------
			// Attack stop ends any attacks/bashes.
			bool stopOnAnimEvent = 
			{ 
				perfAnimEvent.first == PerfAnimEventTag::kAttackStop &&
				actionsInProgress.any
				(
					AVCostAction::kPowerAttackRight, 
					AVCostAction::kBash, 
					AVCostAction::kPowerAttackLeft, 
					AVCostAction::kPowerAttackDual
				) 
			};
			// In progress, but not requested any longer. 
			bool stopIfRequested = 
			{ 
				(actionsInProgress.any(AVCostAction::kPowerAttackRight) && !reqActionsSet.contains(AVCostAction::kPowerAttackRight)) ||
				(actionsInProgress.any(AVCostAction::kBash) && !reqActionsSet.contains(AVCostAction::kBash)) ||
				(actionsInProgress.any(AVCostAction::kPowerAttackLeft) && !reqActionsSet.contains(AVCostAction::kPowerAttackLeft)) ||
				(actionsInProgress.any(AVCostAction::kPowerAttackDual) && !reqActionsSet.contains(AVCostAction::kPowerAttackDual)) 
			};
			if (stopOnAnimEvent || stopIfRequested)
			{
				avcam->RemoveStartedActions
				(
					AVCostAction::kBash, AVCostAction::kPowerAttackDual, 
					AVCostAction::kPowerAttackLeft, AVCostAction::kPowerAttackRight
				);
				isBashing = isPowerAttacking = false;
			}

			// Dodge stop animation played, or no longer requested.
			if ((actionsInProgress.any(AVCostAction::kDodge)) && 
				(perfAnimEvent.first == PerfAnimEventTag::kDodgeStop || 
				 !reqActionsSet.contains(AVCostAction::kDodge)))
			{
				avcam->RemoveRequestedAction(AVCostAction::kDodge);
				avcam->RemoveStartedAction(AVCostAction::kDodge);
			}
		}

		//--------------------------
		// On-hold AV expenditures.
		//--------------------------
		// Sprint stop ends any shield charge/sprint/roll animations.
		// Failsafe: check that the sprint inputs(s) is released.
		if ((IsNotPerforming(InputAction::kSprint)) && 
			(actionsInProgress.any(AVCostAction::kSprint) || reqActionsSet.contains(AVCostAction::kSprint)))
		{
			avcam->RemoveRequestedActions(AVCostAction::kSprint);
			avcam->RemoveStartedActions(AVCostAction::kSprint);
			isSprinting = false;
		}

		if ((IsNotPerforming(InputAction::kSprint)) && 
			(actionsInProgress.any(AVCostAction::kSneakRoll) || reqActionsSet.contains(AVCostAction::kSneakRoll)))
		{
			avcam->RemoveRequestedActions(AVCostAction::kSneakRoll);
			avcam->RemoveStartedActions(AVCostAction::kSneakRoll);
			isRolling = false;
		}

		// CastStop anim event tag only appears when all casting stops,
		// so casting with both hands and releasing one hand will not trigger an anim event.
		// Have to check for block/interrupt/input release instead.
		if ((IsNotPerforming(InputAction::kSpecialAction)) && 
			(actionsInProgress.any(AVCostAction::kCastDual) || reqActionsSet.contains(AVCostAction::kCastDual)))
		{
			avcam->RemoveStartedAction(AVCostAction::kCastDual);
			avcam->RemoveRequestedAction(AVCostAction::kCastDual);
			rhCastDuration = lhCastDuration = 0.0f;
			isCastingDual = false;
		}

		// Not performing LH or special action both hands cast.
		if ((IsNotPerforming(InputAction::kCastLH) && IsNotPerforming(InputAction::kSpecialAction)) && 
			(actionsInProgress.any(AVCostAction::kCastLeft) || reqActionsSet.contains(AVCostAction::kCastLeft)))
		{
			avcam->RemoveStartedAction(AVCostAction::kCastLeft);
			avcam->RemoveRequestedAction(AVCostAction::kCastLeft);
			lhCastDuration = 0.0f;
			isCastingLH = false;
		}

		// Not performing RH or special action both hands cast.
		if (IsNotPerforming(InputAction::kCastRH) && IsNotPerforming(InputAction::kSpecialAction) && 
		    (actionsInProgress.any(AVCostAction::kCastRight) || reqActionsSet.contains(AVCostAction::kCastRight)))
		{
			avcam->RemoveStartedAction(AVCostAction::kCastRight);
			avcam->RemoveRequestedAction(AVCostAction::kCastRight);
			rhCastDuration = 0.0f;
			isCastingRH = false;
		}

		// Expend magicka/stamina only if AV cost actions are still in progress.
		// Mount sprint is not triggered by an animation event, since only the mount's speedmult is modifed.
		bool mountSprint = coopActor->IsOnMount() && IsPerforming(InputAction::kSprint);
		bool shouldExpendStamina = 
		{
			mountSprint ||
			actionsInProgress.any
			(
				AVCostAction::kBash, 
				AVCostAction::kDodge,
				AVCostAction::kPowerAttackDual, 
				AVCostAction::kPowerAttackLeft, 
				AVCostAction::kPowerAttackRight,
				AVCostAction::kSneakRoll, 
				AVCostAction::kSprint
			)
		};
		if (shouldExpendStamina)
		{
			ExpendStamina();
		}
		else if (actionsInProgress.any(AVCostAction::kCastDual, AVCostAction::kCastLeft, AVCostAction::kCastRight))
		{
			ExpendMagicka();
		}
	}

	void PlayerActionManager::HandleDialogue()
	{
		// Have speaker NPC headtrack the dialogue-controlling player.
		// Also auto-end dialogue when the player moves too far away from the speaker.

		auto ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
		{
			if (isInDialogue || autoEndDialogue) 
			{
				isInDialogue = false;
				autoEndDialogue = false;
			}

			return;
		}

		isInDialogue = isControllingUnpausedMenu;
		if (isInDialogue)
		{
			auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
			auto speakerHandle = menuTopicManager->speaker; 
			if (!menuTopicManager || !speakerHandle || !speakerHandle.get())
			{
				if (autoEndDialogue)
				{
					autoEndDialogue = false;
				}
				
				return;
			}

			auto speakerRefr = speakerHandle.get();
			bool closeEnoughToTalk = 
			{
				coopActor->data.location.GetDistance(speakerRefr->data.location) <= Settings::fAutoEndDialogueRadius
			};
			if (closeEnoughToTalk)
			{
				autoEndDialogue = false;
				// Have the speaker look at the player.
				if (auto actorSpeakingWith = speakerRefr->As<RE::Actor>(); actorSpeakingWith)
				{
					if (auto currentProc = actorSpeakingWith->currentProcess; currentProc)
					{
						auto headTrackHandle = currentProc->GetHeadtrackTarget();
						auto headTrackTarget = Util::GetRefrPtrFromHandle(headTrackHandle);
						if (!headTrackTarget || headTrackTarget != coopActor)
						{
							auto lookAtActorPos = coopActor->GetLookingAtLocation();
							currentProc->SetHeadtrackTarget(actorSpeakingWith, lookAtActorPos);
						}
					}
				}
			}
			else if (!autoEndDialogue)
			{
				// Not close enough to the speaker NPC and dialogue still active.
				autoEndDialogue = true;
				if (auto ue = RE::UserEvents::GetSingleton(); ue)
				{
					// Close the dialogue with the 'Cancel' bind.
					auto cancelBind = RE::ControlMap::GetSingleton()->GetMappedKey(ue->cancel, RE::INPUT_DEVICE::kKeyboard, RE::UserEvents::INPUT_CONTEXT_ID::kMenuMode);
					Util::SendButtonEvent(RE::INPUT_DEVICE::kKeyboard, ue->cancel, cancelBind, 1.0f, 0.0f, false);
				}
			}
		}
	}

	void PlayerActionManager::HandleKillmoveRequests()
	{
		// Handled here in a delayed fashion instead of in the player action functions holder because some killmoves
		// frequently bug out and do not set the targeted actor's health to 0 or kill them after the paired animation ends.
		// Also, both the targeted and targeting actors are flagged as not in a killmove at different times,
		// which leads to issues with executing killmoves as well.
		// Here, we force the target's HP to 0 if the killmove animation finishes playing for the targeting actor.
		
		// Must have a valid target.
		auto targetActorPtr = Util::GetActorPtrFromHandle(killmoveTargetActorHandle);
		auto targetValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
		if (!targetValidity) 
		{
			// Reset data if this player was performing a killmove, or if there was a target previously (valid or not).
			if (isPerformingKillmove || killmoveTargetActorHandle) 
			{
				ResetAllKillmoveData(-1);
			}

			return;
		}

		int32_t pIndex = GlobalCoopData::GetCoopPlayerIndex(killmoveTargetActorHandle);
		bool targetIsPlayer = pIndex != -1;
		// Other players cannot be killmoved unless the setting is enabled,
		// they are not in god mode, and they are damageable.
		bool otherPlayerIsNotKillmoveable = 
		{
			(targetIsPlayer) && 
			(
				!Settings::bCanKillmoveOtherPlayers || 
				glob.coopPlayers[pIndex]->isInGodMode ||
				Settings::vfDamageReceivedMult[glob.coopPlayers[pIndex]->playerID] == 0.0f
			)
		};
		if (otherPlayerIsNotKillmoveable) 
		{
			ResetAllKillmoveData(pIndex);
			return;
		}

		float secsSinceKillmoveRequest = Util::GetElapsedSeconds(p->lastKillmoveCheckTP);
		// Potential killmove must be performed for 2 seconds (or the target actor dies) before this player is considered as in a killmove.
		if (!isPerformingKillmove)
		{
			if (secsSinceKillmoveRequest <= 2.0f && coopActor->IsInKillMove() && targetActorPtr->IsInKillMove() && !targetActorPtr->IsDead()) 
			{
				isPerformingKillmove = true;
			}
			else if (secsSinceKillmoveRequest > 2.0f)
			{
				ResetAllKillmoveData(pIndex);
				return;
			}
		}
		else
		{
			// The killmove target player sometimes will stand back up because they still have non-zero health
			// Also, the "kIsInKillmove" flag is set even though the animation has ended
			// and the other player is no longer in a killmove.
			// If changing the wait condition to either player being in a killmove, 
			// the killmoved player will get up upon being revived and then enter bleedout,
			// which also glitches movement and may lead to problems if the game thinks the player is dead.
			// Haven't found a way to fully remove bleedout yet, so some killmoves won't terminate properly for now.
			// If the bleedout glitch still occurs, reset the offending player.
			
			// If this player is not loaded in, end the killmove.
			bool playerValidity = coopActor && coopActor->Is3DLoaded() && coopActor->IsHandleValid();
			if (!playerValidity)
			{
				ResetAllKillmoveData(pIndex);
				return;
			}

			// Set killmove flag which is used to prevent equip state changes from being carried over post-revive.
			if (targetIsPlayer)
			{
				const auto& targetP = glob.coopPlayers[pIndex];
				targetP->pam->killerPlayerActorHandle = coopActor->GetHandle();
				targetP->pam->isBeingKillmovedByAnotherPlayer = true;
			}

			// Aggressor and victim exit the killmove at different points.
			// Reset data one at a time.
			// Allow at most 30 seconds for the killmove to complete.
			bool aggressorStillInKillmove = 
			{
				(secsSinceKillmoveRequest < 30.0f && 
				!targetActorPtr->IsDead()) && 
				(coopActor->IsInKillMove() || coopActor->IsAttacking())
			};
			bool victimStillInKillmove = 
			{
				secsSinceKillmoveRequest < 30.0f && 
				!targetActorPtr->IsDead() && 
				targetActorPtr->IsInKillMove()
			};

			// Killmove target is dead or done with the paired animation.
			if (!victimStillInKillmove) 
			{
				// Set to below 0 health to trigger downed state.
				// Otherwise the player will enter bleedout, since they are set as essential.
				if (targetActorPtr->GetActorValue(RE::ActorValue::kHealth) > 0.0f)
				{
					if (targetIsPlayer)
					{
						glob.coopPlayers[pIndex]->pam->ModifyAV(RE::ActorValue::kHealth, -targetActorPtr->GetActorValue(RE::ActorValue::kHealth));
					}
					else if (auto avOwner = targetActorPtr->As<RE::ActorValueOwner>(); avOwner)
					{
						avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -FLT_MAX);
						// Sometimes still doesn't die after being set <= zero health, so directly call the kill func.
						if (!targetActorPtr->IsDead())
						{
							targetActorPtr->currentProcess->KnockExplosion(targetActorPtr.get(), targetActorPtr->data.location, -1.0f);
							targetActorPtr->KillImpl(coopActor.get(), FLT_MAX, true, true);
							targetActorPtr->KillImmediate();
						}
					}
				}

				// Clear out victim's data.
				ResetKillmoveVictimData(pIndex);
			}

			// This player is finished attacking and done with the killmove paired animation.
			if (!aggressorStillInKillmove) 
			{
				// Stop any ongoing killmove idle.
				StopCurrentIdle();
				// Sheathe/unsheathe if spellcasting unarmed killmove was performed.
				// Otherwise, spells remain visually equipped but casting will trigger unarmed attacks post-killmove.
				if ((!p->isTransformed) && 
					(Settings::bUseUnarmedKillmovesForSpellcasting && coopActor->IsWeaponDrawn()) && 
					(p->em->GetRHSpell() || p->em->GetLHSpell()))
				{
					ReadyWeapon(false);
					ReadyWeapon(true);
				}
			}

			// Both aggressor (this player) and victim are no longer in a killmove, so reset flag and target handle.
			if (!victimStillInKillmove && !aggressorStillInKillmove)
			{
				logger::debug("[PAM] HandleKillmoveRequests: {}: No longer performing killmove.", coopActor->GetName());
				isPerformingKillmove = false;
				killmoveTargetActorHandle = RE::ActorHandle();
			}
		}
	}

	void PlayerActionManager::LevelUpSkillWithBook(RE::TESObjectBOOK* a_book)
	{
		// Read skillbook to level up this player's skill AV.

		// Book has not been read and teaches a skill.
		if (a_book && !a_book->IsRead() && a_book->GetSkill() != RE::ActorValue::kNone) 
		{
			// Skill for this book.
			auto skillAV = a_book->GetSkill();
			// Get index in serializable skill increments list
			// that corresponds to this books taught skill AV.
			int32_t skillAVIndex = -1;
			if (GlobalCoopData::AV_TO_SKILL_MAP.contains(skillAV)) 
			{
				skillAVIndex = GlobalCoopData::AV_TO_SKILL_MAP.at(skillAV);
			}

			if (skillAVIndex != -1)
			{
				const float avLvl = coopActor->GetBaseActorValue(skillAV);
				// Increment skill increments list index.
				auto& skillIncList = glob.serializablePlayerData.at(coopActor->formID)->skillLevelIncreasesList;
				skillIncList[skillAVIndex]++;
				// Set new leveled up AV.
				coopActor->SetBaseActorValue(skillAV, avLvl + 1);

				// REMOVE when done debugging.
				logger::debug("[PAM] LevelUpSkillWithBook: {} leveled up Skill {} from level {} to {} by reading {}. Skill inc list entry goes from {} to {}.",
					coopActor->GetName(),
					skillAV,
					avLvl,
					avLvl + 1,
					a_book->GetName(),
					skillIncList[skillAVIndex],
					skillIncList[skillAVIndex] + 1);
			}
		}
	}

	bool PlayerActionManager::PassesInputPressCheck(const InputAction& a_action)
	{
		// Check if all of the inputs for the given action are pressed 
		// in order, if order matters, and in any order otherwise.
		// Also ensure that actions that require a minimum hold time
		// have all their inputs held for at least the minimum hold time.
		// And check for consecutive taps for actions that require them.

		// Check for failure, but assume success as default.
		bool passedPressCheck = true;
		const auto& params = paStatesList[!a_action - !InputAction::kFirstAction].paParams;
		const auto& inputComp = params.composingInputs;
		if (a_action != InputAction::kNone)
		{
			// Ensure all inputs are pressed. Order does not matter.
			if (params.triggerFlags.all(TriggerFlag::kDoNotUseCompActionsOrdering))
			{
				for (auto inputIndex : inputComp) 
				{
					// One input not pressed.
					if (!glob.cdh->GetInputState(controllerID, inputIndex).isPressed) 
					{
						passedPressCheck = false;
						break;
					}
				}
			}
			else
			{
				// Have to check if inputs are held long enough and pressed in sequence.
				if (!inputComp.empty())
				{
					// If more than one composing input, ensure they are pressed in sequence.
					if (inputComp.size() > 1)
					{
						// Skip first index since we're comparing current to previous held times.
						uint32_t actionIndex = 1;
						while (actionIndex <= inputComp.size() - 1 && passedPressCheck)
						{
							auto currentInputState = glob.cdh->GetInputState(controllerID, inputComp[actionIndex]);
							auto prevInputState = glob.cdh->GetInputState(controllerID, inputComp[actionIndex - 1]);
							if (inputComp[actionIndex - 1] == InputAction::kLS || inputComp[actionIndex - 1] == InputAction::kRS) 
							{
								// Skip over comparing hold times if the previous input is the LS or RS,
								// as these do not have to be pressed in order.
								// Make sure both inputs are pressed.
								passedPressCheck &= prevInputState.isPressed && currentInputState.isPressed;
							}
							else
							{
								// Check that the current input is pressed and held for less time than the previous
								// to ensure the proper ordering.
								passedPressCheck &= currentInputState.heldTimeSecs < prevInputState.heldTimeSecs && currentInputState.isPressed;
							}

							++actionIndex;
						}
					}
					else
					{
						auto singularInputState = glob.cdh->GetInputState(controllerID, inputComp[0]);
						// Singular composing input is pressed.
						passedPressCheck = singularInputState.isPressed;
					}
				}
			}

			// Check for consecutive taps next.
			if (passedPressCheck && params.perfType == PerfType::kOnConsecTap)
			{
				passedPressCheck &= PassesConsecTapsCheck(a_action);
			}

			// Only check hold time if necessary and the other checks passed.
			if (passedPressCheck && params.triggerFlags.all(TriggerFlag::kMinHoldTime))
			{
				passedPressCheck &= GetPlayerActionInputHoldTime(a_action) > Settings::fSecsDefMinHoldTime;
			}
		}

		return passedPressCheck;
	}

	bool PlayerActionManager::PassesConsecTapsCheck(const InputAction& a_action)
	{
		// Check if any/the last input in the action's composing inputs list was double tapped,
		// depending on if the ordering of the composing inputs matters.

		if (a_action != InputAction::kNone) 
		{
			auto& inputComp = paStatesList[!a_action - !InputAction::kFirstAction].paParams.composingInputs;
			if (!inputComp.empty()) 
			{
				// Any input has to be tapped at least twice if ordering does not matter.
				if (paStatesList[!a_action - !InputAction::kFirstAction].paParams.triggerFlags.all(TriggerFlag::kDoNotUseCompActionsOrdering)) 
				{
					return 
					(
						std::any_of
						(
							inputComp.begin(), inputComp.end(), 
							[this](const InputAction& a_action) 
							{ 
								return glob.cdh->GetInputState(controllerID, a_action).consecPresses > 1; 
							}
						)
					);
				}
				else
				{
					// Check if the last input in the composing inputs list is tapped at least twice.
					return glob.cdh->GetInputState(controllerID, inputComp.back()).consecPresses > 1;	
				}
			}
		}

		return false;
	}

	void PlayerActionManager::QueueP1ButtonEvent(const InputAction& a_inputAction, RE::INPUT_DEVICE&& a_inputDevice, ButtonEventPressType&& a_buttonStateToTrigger, float&& a_heldDownSecs, bool&& a_toggleAIDriven) noexcept
	{
		// Create and queue (not send!) a P1 input event with the button ID code mapped to the given action,
		// using the given device, button press type, and held time.
		// Also toggle AI driven if requested.

		auto controlMap = RE::ControlMap::GetSingleton(); 
		if (!controlMap)
		{
			return;
		}

		auto& paInfo = glob.paInfoHolder;
		// Get event name to send.
		if (paInfo->ACTIONS_TO_P1_UE_STRINGS.contains(!a_inputAction))
		{
			const std::string_view& ueString = paInfo->ACTIONS_TO_P1_UE_STRINGS.at(!a_inputAction);
			// Value indicates if the button is pressed (1.0) or not (0.0).
			const float value = a_buttonStateToTrigger == ButtonEventPressType::kRelease ? 0.0f : 1.0f;
			float heldTimeSecs = 0.0f;
			// If using a default of 0 held time, use the given action's held time,
			// unless the button press type is 'Instant Trigger' which will
			// send an event as if the input were just pressed (0 held time).
			if (a_heldDownSecs == 0.0f) 
			{
				if (a_inputAction >= InputAction::kFirstAction && a_inputAction <= InputAction::kLastAction)
				{
					if (a_buttonStateToTrigger != ButtonEventPressType::kInstantTrigger)
					{
						heldTimeSecs = GetPlayerActionInputHoldTime(a_inputAction);
					}
				}
			}
			else
			{
				heldTimeSecs = a_heldDownSecs;
			}
				
			// Get game mask from the input event name.
			const uint32_t& buttonCode = controlMap->GetMappedKey(ueString, a_inputDevice);
			// If a valid mask and event name, send the event.
			if (buttonCode != 255 && !ueString.empty())
			{
				auto buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(a_inputDevice, ueString, buttonCode, value, heldTimeSecs));
				// Using the pad: toggle AI driven (1), or not (2).
				(*buttonEvent.get())->AsButtonEvent()->pad24 = a_toggleAIDriven ? 0x1C0DA : 0x2C0DA;
				// Queued up.
				queuedP1ButtonEvents.emplace_back(std::move(buttonEvent));
			}
		}
	}

	void PlayerActionManager::ReadyWeapon(const bool& a_shouldDraw) 
	{
		// Sheathe or draw the player's weapons/magic.
		// This function is yet another flavor of "it just works".

		if (p->isPlayer1) 
		{
			weapMagReadied = a_shouldDraw;
			// Redundancy, I know.
			// But sometimes individual calls fail.
			SendButtonEvent(InputAction::kSheathe, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger);
			Util::RunPlayerActionCommand(a_shouldDraw ? RE::DEFAULT_OBJECT::kActionDraw : RE::DEFAULT_OBJECT::kActionSheath, coopActor.get());
			coopActor->DrawWeaponMagicHands(a_shouldDraw);
		}
		else
		{
			// Unequip bound weapons when sheathing weapons.
			if (!a_shouldDraw) 
			{
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Right hand.
					if (auto rhForm = coopActor->GetEquippedObject(false); rhForm)
					{
						if (auto weap = rhForm->As<RE::TESObjectWEAP>(); weap && weap->IsBound())
						{
							aem->UnequipObject(coopActor.get(), weap);
							p->em->EquipFists();
							if (weap->IsBow())
							{
								// Unequip bound arrow too.
								if (auto boundArrow = p->em->equippedForms[!EquipIndex::kAmmo]; 
									boundArrow && boundArrow->HasKeywordByEditorID("WeapTypeBoundArrow"))
								{
									aem->UnequipObject(coopActor.get(), boundArrow->As<RE::TESAmmo>());
								}
							}
						}
					}

					// Left hand.
					if (auto lhForm = coopActor->GetEquippedObject(true); lhForm)
					{
						if (auto weap = lhForm->As<RE::TESObjectWEAP>(); weap && weap->IsBound())
						{
							aem->UnequipObject(coopActor.get(), weap);
							p->em->EquipFists();
						}
					}

					// Reset bound weapon state.
					boundWeapReqLH = false;
					boundWeapReqRH = false;
					secsSinceBoundWeapLHReq = secsSinceBoundWeapRHReq = 0.0f;
				}
			}

			// More redundancy.
			if (coopActor->currentProcess)
			{
				weapMagReadied = a_shouldDraw;
				if (a_shouldDraw)
				{
					coopActor->currentProcess->lowProcessFlags.set(RE::AIProcess::LowProcessFlags::kAlert, RE::AIProcess::LowProcessFlags::kCurrentActionComplete);
				}
				else
				{
					coopActor->currentProcess->lowProcessFlags.reset(RE::AIProcess::LowProcessFlags::kAlert, RE::AIProcess::LowProcessFlags::kCurrentActionComplete);
				}

				Util::RunPlayerActionCommand(a_shouldDraw ? RE::DEFAULT_OBJECT::kActionDraw : RE::DEFAULT_OBJECT::kActionSheath, coopActor.get());
				coopActor->DrawWeaponMagicHands(a_shouldDraw);
			}
		}
	}

	void PlayerActionManager::RevivePlayer()
	{
		// Transfer health from this player to a health pool that is given to the targeted
		// downed player all at once when they are fully revived. 
		// Keep track of how much health was transferred and if the downed player is fully revived.

		if (!downedPlayerTarget) 
		{
			logger::error("[PAM] ERR: RevivePlayer: cannot get downed player to revive. CID is invalid.");
			return;
		}

		secsSinceReviveCheck = Util::GetElapsedSeconds(p->lastReviveCheckTP);
		// Downed target must not be revived yet.
		if (!downedPlayerTarget->isRevived)
		{
			p->lastReviveCheckTP = SteadyClock::now();

			auto& revivePAState = paStatesList[!InputAction::kActivate - !InputAction::kFirstAction];
			// Can transfer health up until the minimum remaining health level.
			float healthCost = min(revivePAState.avCost * secsSinceReviveCheck, currentHealth - Settings::fMinHealthWhileReviving);
			// Total transferable health for a full revive.
			float fullHealthCost = revivePAState.avCost * Settings::fSecsReviveTime;
			// Ratio of the downed player's health after being fully revived
			// to the health this player must give up to fully revive them.
			float healthTransferRatio = downedPlayerTarget->fullReviveHealth / fullHealthCost;
			// Don't reduce this player's health when in god mode.
			if (!p->isInGodMode) 
			{
				ModifyAV(RE::ActorValue::kHealth, -healthCost);
			}

			// Amount of health this player transfers away this check.
			p->revivedHealth += healthCost;
			// Amount of health the downed player target will gain from this check.
			downedPlayerTarget->revivedHealth += healthTransferRatio * healthCost;
			// Done reviving when the total health the downed player should receive
			// is greater than or equal to their health after a full revive.
			if (downedPlayerTarget->revivedHealth >= downedPlayerTarget->fullReviveHealth)
			{
				// Now revived, reset revived health data.
				downedPlayerTarget->isRevived = true;
				p->revivedHealth = 0.0f;
			}
		}
	}

	void PlayerActionManager::ResetAllKillmoveData(const int32_t& a_targetPlayerIndex)
	{
		// Reset killmove data for this player and their targeted victim.
		// Clear flag and target handle too.

		ResetKillmoveVictimData(a_targetPlayerIndex);
		StopCurrentIdle();
		isPerformingKillmove = false;
		killmoveTargetActorHandle = RE::ActorHandle();
	}


	void PlayerActionManager::ResetAttackDamageMult()
	{
		// Reset player weapon speed and attack damage actor value multipliers.

		coopActor->As<RE::ActorValueOwner>()->SetActorValue(RE::ActorValue::kWeaponSpeedMult, 1.0f);
		coopActor->As<RE::ActorValueOwner>()->SetActorValue(RE::ActorValue::kAttackDamageMult, 1.0f);
		reqDamageMult = 1.0f;
		attackDamageMultSet = false;
	}

	void PlayerActionManager::ResetKillmoveVictimData(const int32_t& a_targetPlayerIndex) 
	{
		// Stop any ongoing killmove idle on the victim's side.
		// If the victim is a player, clear out their killer player handle
		// and reset killmove victim flag.

		auto targetActorPtr = Util::GetActorPtrFromHandle(killmoveTargetActorHandle);
		if (targetActorPtr)
		{
			Util::NativeFunctions::SetDontMove(targetActorPtr.get(), false);
			if (targetActorPtr->currentProcess)
			{
				targetActorPtr->currentProcess->StopCurrentIdle(targetActorPtr.get(), true);
			}

			targetActorPtr->NotifyAnimationGraph("IdleStop");
			targetActorPtr->NotifyAnimationGraph("attackStop");
			targetActorPtr->NotifyAnimationGraph("moveStart");
		}

		if (a_targetPlayerIndex != -1)
		{
			const auto& targetP = glob.coopPlayers[a_targetPlayerIndex];
			targetP->pam->killerPlayerActorHandle = RE::ActorHandle();
			targetP->pam->isBeingKillmovedByAnotherPlayer = false;
		}
	}

	void PlayerActionManager::ResetPackageCastingState()
	{
		// Have to "flush out" the ranged attack package by briefly
		// setting both hands's casting globals, evaluating the package to update,
		// and then clearing both globals before evaluating again.
		// Done to work around odd instances where the some portion of
		// the package/casting state does not full reset and produces
		// movement bugs.

		auto& lhCasting = castingGlobVars[!CastingGlobIndex::kLH];
		auto& rhCasting = castingGlobVars[!CastingGlobIndex::kRH];
		auto& casting2H = castingGlobVars[!CastingGlobIndex::k2H];
		auto& dualCasting = castingGlobVars[!CastingGlobIndex::kDual];
		auto& shouting = castingGlobVars[!CastingGlobIndex::kShout];
		auto& voiceCasting = castingGlobVars[!CastingGlobIndex::kVoice];
		lhCasting->value = 
		rhCasting->value = 
		casting2H->value =
		dualCasting->value =
		shouting->value = 
		voiceCasting->value = 1.0f;
		EvaluatePackage();
		lhCasting->value =
		rhCasting->value =
		casting2H->value =
		dualCasting->value =
		shouting->value =
		voiceCasting->value = 0.0f;
		EvaluatePackage();
	}

	void PlayerActionManager::ResetTPs()
	{
		// Reset all player timepoints handled by this manager to the current time.

		p->expendSprintStaminaTP		=
		p->lastActivationCheckTP		=
		p->lastAttackStartTP			=
		p->lastBoundWeaponLHReqTP		=
		p->lastBoundWeaponRHReqTP		=
		p->lastCyclingTP				=
		p->lastDownedTP					=
		p->lastInputActionBlockTP		= 
		p->lastKillmoveCheckTP			=
		p->lastLHCastStartTP			=
		p->lastQSSCastStartTP			=
		p->lastReviveCheckTP			=
		p->lastRHCastStartTP			=
		p->lastStaffLHCastTP			=
		p->lastStaffRHCastTP			=
		p->lastStaminaCooldownCheckTP	=
		p->outOfStaminaTP				=
		p->shoutStartTP					= SteadyClock::now();
	}

	void PlayerActionManager::RestoreAVToMaxValue(RE::ActorValue a_av)
	{
		// Restore the given actor value to its max value (base + temporary mod).

		auto avValueOwner = coopActor->As<RE::ActorValueOwner>(); 
		if (!avValueOwner) 
		{
			return;
		}

		const float baseAV = coopActor->GetBaseActorValue(a_av);
		// Get the amount to increase the current value by.
		float deltaAmount = 
		{ 
			coopActor->GetBaseActorValue(a_av) +
			coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) -
			coopActor->GetActorValue(a_av) 
		};

		if (deltaAmount > 0.0f)
		{
			avValueOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_av, deltaAmount);
		}
	}

	void PlayerActionManager::SendButtonEvent(const InputAction& a_inputAction, RE::INPUT_DEVICE&& a_inputDevice, ButtonEventPressType&& a_buttonStateToTrigger, float&& a_heldDownSecs, bool&& a_toggleAIDriven) noexcept
	{
		// Send (not queue!) a button event for the button mask/event linked with the given action and device.
		// Set the value and held time based on the given button press type and held time.
		// Toggle AI driven if necessary.

		if (auto controlMap = RE::ControlMap::GetSingleton(); controlMap)
		{
			auto& paInfo = glob.paInfoHolder;
			// Check if the given action maps to a valid P1 input event name.
			if (paInfo->ACTIONS_TO_P1_UE_STRINGS.contains(!a_inputAction))
			{
				const std::string_view& ueString = paInfo->ACTIONS_TO_P1_UE_STRINGS.at(!a_inputAction);
				// Pressed == 1.0, released == 0.0.
				const float value = a_buttonStateToTrigger == ButtonEventPressType::kRelease ? 0.0f : 1.0f;
				float heldTimeSecs = 0.0f;
				if (a_heldDownSecs == 0.0f)
				{
					if (a_inputAction >= InputAction::kFirstAction && a_inputAction <= InputAction::kLastAction)
					{
						// Instant trigger means just pressed and does not need a paired 'released' button event (value == 0.0).
						if (a_buttonStateToTrigger != ButtonEventPressType::kInstantTrigger)
						{
							// Set held time as given action's held time.
							heldTimeSecs = GetPlayerActionInputHoldTime(a_inputAction);
						}
					}
				}
				else
				{
					// Use the given held time otherwise.
					heldTimeSecs = a_heldDownSecs;
				}

				// Get button mask from event.
				const uint32_t& buttonCode = controlMap->GetMappedKey(ueString, a_inputDevice);
				logger::debug("[PAM] SendButtonEvent: got button bind {} for PA {}, User Event '{}'. Button state to trigger: {}, Held for {}s. Toggle AI driven: {}.",
					buttonCode, a_inputAction, ueString, a_buttonStateToTrigger, heldTimeSecs, a_toggleAIDriven);

				// Is a valid button mask and event.
				if (buttonCode != 255 && !ueString.empty())
				{
					// Certain actions do not trigger or terminate properly when DontMove flag is set on P1.
					// For example, P1 cannot cast if DontMove is set while the cast button is pressed,
					// or will continue casting if DontMove is set while the cast button is released.
					p->mm->SetDontMove(false);
					Util::SendButtonEvent(a_inputDevice, ueString, buttonCode, value, heldTimeSecs, a_toggleAIDriven, true);
				}
			}
		}
	}

	void PlayerActionManager::SetAttackDamageMult()
	{
		// Set power attack (currently not needed) and sneak attack damage multipliers
		// based on the attacking weapon/spell and player perks.

		float damageMult = 1.0f;
		RE::TESForm* handObj = nullptr;
		// Get attacking weapon/spell.
		if (lastAttackingHand == HandIndex::kLH)
		{
			handObj = p->em->equippedForms[!EquipIndex::kLeftHand];
		}
		else if (lastAttackingHand == HandIndex::kRH)
		{
			handObj = p->em->equippedForms[!EquipIndex::kRightHand];
		}
		else if (lastAttackingHand == HandIndex::k2H)
		{
			if (handObj = p->em->equippedForms[!EquipIndex::kRightHand]; !handObj)
			{
				handObj = p->em->equippedForms[!EquipIndex::kLeftHand];
			}
		}
		else
		{
			handObj = p->em->equippedForms[!EquipIndex::kRightHand];
		}

		/*
		kNone = 0,
		kVeryLow = 1,
		kLow = 2,
		kNormal = 3,
		kHigh = 4,
		kCritical = 5
		*/
		// Set damage mults with sneaking taken into consideration.
		if (coopActor->IsSneaking())
		{
			auto targetActorHandle = p->tm->selectedTargetActorHandle;
			int32_t detectionPct = 0;
			// Not targeting an actor with the crosshair, so use the aim correction target,
			// or choose a new target based on proximity and facing angle.
			if (!Util::HandleIsValid(targetActorHandle))
			{
				if (Settings::vbUseAimCorrection[playerID])
				{
					targetActorHandle = p->tm->aimCorrectionTargetHandle;
				}
				else
				{
					targetActorHandle = p->tm->GetClosestTargetableActorInFOV(coopActor->GetHandle(), PI, false, -1.0f, false);
				}
			}

			auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
			// For melee sneak attacks, ensure the target actor is in melee range. If not, pick a new one.
			if (handObj && handObj->As<RE::TESObjectWEAP>() && !handObj->As<RE::TESObjectWEAP>()->IsRanged())
			{
				const float weapReach = p->em->GetMaxWeapReach();
				// Get closest actor in front if target actor is not in within weapon reach.
				if (targetActorPtr && targetActorPtr->data.location.GetDistance(coopActor->data.location) > weapReach)
				{
					targetActorHandle = p->tm->GetClosestTargetableActorInFOV(coopActor->GetHandle(), PI, true, weapReach, false);
				}
			}

			targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
			if (targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get()))
			{
				// If there is a valid sneak attack target, check their detection
				// level of the player before changing the player's attack damage multiplier.
				const auto nameHash = Hash(targetActorPtr->GetName());
				// Filter out blacklisted actors
				auto mount = p->GetCurrentMount();
				// Filter out mount.
				const bool blacklisted = (mount && targetActorPtr == mount);
				if (!blacklisted)
				{
					detectionPct = (std::clamp(static_cast<float>(targetActorPtr->RequestDetectionLevel(coopActor.get())), -20.0f, 0.0f) + 20.0f) * 5.0f;
					// Fully hidden.
					if (detectionPct == 0.0f)
					{
						// Damage mult values and affecting perks from:
						// https://en.uesp.net/wiki/Skyrim:Sneak#Sneak_Attacks
						if (handObj && handObj->IsWeapon())
						{
							const auto actorBase = coopActor->GetActorBase();
							const auto weapType = handObj->As<RE::TESObjectWEAP>()->GetWeaponType();
							switch (weapType)
							{
							case RE::WEAPON_TYPE::kBow:
							case RE::WEAPON_TYPE::kCrossbow:
							{
								damageMult = 2.0f;
								if (coopActor->HasPerk(glob.deadlyAimPerk))
								{
									damageMult = 3.0f;
								}

								break;
							}
							case RE::WEAPON_TYPE::kStaff:
							{
								damageMult = 1.5f;
								break;
							}
							case RE::WEAPON_TYPE::kTwoHandAxe:
							case RE::WEAPON_TYPE::kTwoHandSword:
							{
								damageMult = 2.0f;
								break;
							}
							case RE::WEAPON_TYPE::kOneHandAxe:
							case RE::WEAPON_TYPE::kOneHandDagger:
							case RE::WEAPON_TYPE::kOneHandMace:
							case RE::WEAPON_TYPE::kOneHandSword:
							case RE::WEAPON_TYPE::kHandToHandMelee:
							{
								damageMult = 3.0f;
								if (weapType == RE::WEAPON_TYPE::kOneHandDagger && coopActor->HasPerk(glob.assassinsBladePerk))
								{
									damageMult = 15.0f;
								}
								else if (coopActor->HasPerk(glob.backstabPerk))
								{
									damageMult = 6.0f;
								}

								break;
							}
							default:
							{
								damageMult = 2.0f;
								break;
							}
							}
						}
						else if (handObj && !handObj->IsWeapon())
						{
							// Is a spell. Probably.
							damageMult = 1.5f;
						}
						else
						{
							// Fists.
							damageMult = 2.0f;
						}
					}
				}
			}
		}

		// REMOVE when done debugging.
		logger::debug("[PAM] SetAttackDamageMult: Attacking hand: {}, weapon attack damage mult: {}, bashing: {}, power attacking: {}, sneaking: {}",
			lastAttackingHand, damageMult, isBashing, isPowerAttacking, isSneaking);

		if (damageMult != 1.0f)
		{
			reqDamageMult = damageMult;
			attackDamageMultSet = true;
		}
	}

	void PlayerActionManager::SetCurrentPackage(RE::TESPackage* a_package)
	{
		// Set the player's current package to evaluate as the given package.
		const bool isInCombat = coopActor->IsInCombat();
		auto& packageStack =
		(
			isInCombat ?
			packageStackMap[PackageIndex::kCombatOverride]->forms :
			packageStackMap[PackageIndex::kDefault]->forms
		);

		// Only set if different.
		if (packageStack[0] != a_package)
		{
			packageStack[0] = a_package;
		}
	}

	void PlayerActionManager::SetEssentialForReviveSystem()
	{
		// Set essential if using the revive system, since players do not 'die' 
		// right away and instead enter a suspended animation 'downed' state where they can be revived.

		bool setEssential = false;
		if ((!p->coopActor->IsInKillMove() && glob.coopSessionActive && Settings::bUseReviveSystem && coopActor->GetActorBase()) &&
			(coopActor->GetActorBase()->actorData.actorBaseFlags.none(RE::ACTOR_BASE_DATA::Flag::kEssential) || coopActor->boolFlags.none(RE::Actor::BOOL_FLAGS::kEssential)))
		{
			setEssential = p->isPlayer1 ? Settings::bCanRevivePlayer1 : true;
		}

		if (setEssential)
		{
			// REMOVE when done debugging.
			logger::debug("[PAM] SetEssentialForReviveSystem: {} is using the revive system. SET ESSENTIAL", coopActor->GetName());

			Util::NativeFunctions::SetActorBaseDataFlag(coopActor->GetActorBase(), RE::ACTOR_BASE_DATA::Flag::kEssential, true);
			coopActor->boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
		}
	}

	void PlayerActionManager::SetPackageFlag(RE::PACKAGE_DATA::GeneralFlag&& a_flag, bool&& a_set)
	{
		// Set/unset given flag for all the player's packages.

		for (uint32_t i = 0; i < !PackageIndex::kTotal; i++)
		{
			if (glob.coopPackages[!PackageIndex::kTotal * controllerID + i])
			{
				if (a_set) 
				{
					glob.coopPackages[!PackageIndex::kTotal * controllerID + i]->packData.packFlags.set(a_flag);
				}
				else
				{
					glob.coopPackages[!PackageIndex::kTotal * controllerID + i]->packData.packFlags.reset(a_flag);
				}
			}
		}
	}

	void PlayerActionManager::SetWeaponGrip(bool a_resetGrip)
	{
		// WIP: Set equip/animation type for both LH and RH weapons.
		// 1H -> 2H or 2H -> 1H depending on each one's current grip type.
		// Currently a bridge too far for the ever-present feature creep in this mod.
		// Heh.

		auto lhWeap = p->em->GetLHWeapon();
		auto rhWeap = p->em->GetRHWeapon();
		if (lhWeap) 
		{
			coopActor->UpdateWeaponAbility(lhWeap, nullptr, true);
		}

		if (rhWeap)
		{
			coopActor->UpdateWeaponAbility(rhWeap, nullptr, false);
		}

		if (a_resetGrip) 
		{
			// 2H weapon that was originally 1H.
			if (lhWeap && rhWeap && lhWeap == rhWeap && lhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
			{
				logger::debug("[PAM] SetWeaponGrip: {}: Setting original 1H grip flags for 2H weap: {}.",
					coopActor->GetName(), lhWeap->GetName());

				lhWeap->weaponData.animationType.reset(RE::WEAPON_TYPE::kTwoHandSword);
				lhWeap->weaponData.animationType.reset(RE::WEAPON_TYPE::kTwoHandAxe);
				lhWeap->weaponData.animationType.set(p->em->lhOriginalType);
				rhWeap->weaponData.animationType.reset(RE::WEAPON_TYPE::kTwoHandSword);
				rhWeap->weaponData.animationType.reset(RE::WEAPON_TYPE::kTwoHandAxe);
				rhWeap->weaponData.animationType.set(p->em->rhOriginalType);
				logger::debug("[PAM] SetWeaponGrip: {}: Reset 2H grip flags and set original 1H grip flags: {}, {}.",
					coopActor->GetName(),
					static_cast<uint32_t>(p->em->lhOriginalType),
					static_cast<uint32_t>(p->em->rhOriginalType));

				lhWeap->SetEquipSlot(glob.leftHandEquipSlot);
				rhWeap->SetEquipSlot(glob.rightHandEquipSlot);
			}
			else
			{
				// LH weapon that was orignally 2H.
				if (lhWeap && lhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
				{
					logger::debug("[PAM] SetWeaponGrip: {}: Setting original 2H grip flags for LH weap: {}.",
						coopActor->GetName(),
						lhWeap->GetName());
					lhWeap->weaponData.animationType.reset(lhWeap->GetWeaponType());
					lhWeap->weaponData.animationType.set(p->em->lhOriginalType);
					logger::debug("[PAM] SetWeaponGrip: {}: Set original 2H grip flag: {} for {}.",
						coopActor->GetName(),
						static_cast<uint32_t>(p->em->lhOriginalType), lhWeap->GetName());
					lhWeap->SetEquipSlot(glob.bothHandsEquipSlot);
				}

				// RH weapon that was orignally 2H.
				if (rhWeap && rhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
				{
					logger::debug("[PAM] SetWeaponGrip: {}: Setting original 2H grip flags for RH weap {}.",
						coopActor->GetName(),
						rhWeap->GetName());
					rhWeap->weaponData.animationType.reset(rhWeap->GetWeaponType());
					rhWeap->weaponData.animationType.set(p->em->rhOriginalType);
					logger::debug("[PAM] SetWeaponGrip: {}: Set original 2H grip flag: {} for {}.",
						coopActor->GetName(),
						static_cast<uint32_t>(p->em->rhOriginalType), rhWeap->GetName());
					rhWeap->SetEquipSlot(glob.bothHandsEquipSlot);
				}
			}
		}
		else 
		{
			// Set new 2H anim type for weapon that was originally 1H.
			if (lhWeap && rhWeap && lhWeap == rhWeap && lhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
			{
				logger::debug("[PAM] SetWeaponGrip: {}: Setting new 2H grip flag for 1H weapon: {}.",
					coopActor->GetName(),
					lhWeap->GetName());
				lhWeap->weaponData.animationType.reset(p->em->lhOriginalType);
				lhWeap->weaponData.animationType.set(p->em->lhNewGripType);
				rhWeap->weaponData.animationType.reset(p->em->rhOriginalType);
				rhWeap->weaponData.animationType.set(p->em->rhNewGripType);
				logger::debug("[PAM] SetWeaponGrip: {}: Set new grip types: {}, {} for {}.",
					coopActor->GetName(),
					static_cast<uint32_t>(p->em->lhNewGripType),
					static_cast<uint32_t>(p->em->rhNewGripType),
					lhWeap->GetName());
				lhWeap->SetEquipSlot(glob.bothHandsEquipSlot);
				rhWeap->SetEquipSlot(glob.bothHandsEquipSlot);
			}
			else
			{
				// Set new 1H anim type for LH weapon that was originally 2H.
				if (lhWeap && lhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
				{
					logger::debug("[PAM] SetWeaponGrip: {}: Setting new 2H grip flag for LH weapon: {}.",
						coopActor->GetName(),
						lhWeap->GetName());
					lhWeap->weaponData.animationType.reset(p->em->lhOriginalType);
					lhWeap->weaponData.animationType.set(p->em->lhNewGripType);
					logger::debug("[PAM] SetWeaponGrip: {}: Set new grip type: {} for {}.",
						coopActor->GetName(),
						static_cast<uint32_t>(p->em->lhNewGripType), lhWeap->GetName());
					lhWeap->SetEquipSlot(glob.leftHandEquipSlot);
				}

				// Set new 1H anim type for RH weapon that was originally 2H.
				if (rhWeap && rhWeap->weaponData.flags.any(RE::TESObjectWEAP::Data::Flag::kEmbeddedWeapon))
				{
					logger::debug("[PAM] SetWeaponGrip: {}: SetSetting new 2H grip flag for RH weapon: {}.",
						coopActor->GetName(),
						rhWeap->GetName());
					rhWeap->weaponData.animationType.reset(p->em->rhOriginalType);
					rhWeap->weaponData.animationType.set(p->em->rhNewGripType);
					logger::debug("[PAM] SetWeaponGrip: {}: Set new grip type: {} for {}.",
						coopActor->GetName(),
						static_cast<uint32_t>(p->em->rhNewGripType), rhWeap->GetName());
					rhWeap->SetEquipSlot(glob.rightHandEquipSlot);
				}
			}
		}
	}

	void PlayerActionManager::StopCombatWithFriendlyActors()
	{
		// Stop combat between this player and all friendly actors:
		// followers, teammates, commanded actors, 
		// and normally neutral actors that are hostile to a player without any accrued bounty.

		auto procLists = RE::ProcessLists::GetSingleton();
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!procLists || !p1)
		{
			return;
		}

		std::set<RE::ActorHandle, ActorHandleComp> friendlyActorHandles;
		if (auto followerExData = p1->extraList.GetByType<RE::ExtraFollower>(); followerExData)
		{
			// Followers
			for (const auto& follower : followerExData->actorFollowers)
			{
				if (Util::HandleIsValid(follower.actor))
				{
					friendlyActorHandles.insert(follower.actor);
				}
			}
		}

		// If the player has bounty (through P1), do not stop combat and alarm
		// after attempting to pacify each friendly/neutral actor.
		bool hasZeroTotalBounty = true;
		// Teammates and summons and mounts. 
		// Not sure how to directly access a list of the player's teammates and summons, 
		// so we'll iterate through the high process actors.
		for (auto& handle : procLists->highActorHandles)
		{
			if (Util::HandleIsValid(handle))
			{
				auto actor = handle.get().get();
				bool isSelf = actor == coopActor.get();
				// No self-conflict, I guess.
				if (isSelf) 
				{
					continue;
				}

				// Is friendly to P1 or the party.
				if (Util::IsPartyFriendlyActor(actor)) 
				{
					friendlyActorHandles.insert(handle);
					continue;
				}

				// Check this actor's factions to see if P1 has a bounty with any faction,
				// or if the faction is an enemy faction.
				bool hasBounty = false;
				bool isEnemy = actor->GetActorBase() ? actor->GetActorBase()->AggroRadiusBehaviourIsEnabled() : false;
				actor->VisitFactions([p1, actor, &isEnemy, &hasBounty](RE::TESFaction* a_faction, const int8_t& a_rank) {
					if (a_faction) 
					{
						if (a_faction->GetCrimeGold() > 0.0f)
						{
							hasBounty = true;
							return true;
						}

						if (a_faction->IsPlayerEnemy()) 
						{
							isEnemy = true;
							return true;
						}
					}

					return false;
				});

				if (hasBounty && hasZeroTotalBounty)
				{
					hasZeroTotalBounty = false;
				}

				// Pacify aggroed actors that are not normally antagonistic towards the player.
				// Must have 0 crimegold/bounty with all of this actor's factions.
				bool angryWithPlayer = actor->boolFlags.all(RE::Actor::BOOL_FLAGS::kAngryWithPlayer);
				if ((!isEnemy && !hasBounty) && (angryWithPlayer || actor->IsHostileToActor(p1) || actor->IsHostileToActor(coopActor.get())))
				{
					friendlyActorHandles.insert(handle);
				}
			}
		}

		for (auto actorHandle : friendlyActorHandles)
		{
			if (auto actor = Util::GetActorPtrFromHandle(actorHandle); actor)
			{
				// Either a party-friendly actor or a normally neutral one that is above 25% of their max health.
				bool canPacify = Util::IsPartyFriendlyActor(actor.get());
				if (!canPacify) 
				{
					float maxHealth = 
					(
						actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth) + 
						actor->GetBaseActorValue(RE::ActorValue::kHealth)
					);
					canPacify = actor->GetActorValue(RE::ActorValue::kHealth) / maxHealth > 0.25f;
				}

				if ((canPacify) && (actor->IsHostileToActor(coopActor.get())))
				{
					// Stop attacking and combat.
					actor->NotifyAnimationGraph("attackStop");
					if (actor->combatController)
					{
						actor->combatController->stoppedCombat = true;
					}

					actor->StopCombat();
					actor->StopAlarmOnActor();
					actor->currentProcess->lowProcessFlags.reset(RE::AIProcess::LowProcessFlags::kAlert);

					// REMOVE when done debugging.
					logger::debug("[PAM] StopCombatWithFriendlyActors: Stopped combat between {} and {}.", coopActor->GetName(), actor->GetName());
				}
			}
		}

		if (hasZeroTotalBounty) 
		{
			procLists->StopCombatAndAlarmOnActor(coopActor.get(), false);
			procLists->ClearCachedFactionFightReactions();

			// Stop alarm and combat on the player's end.
			coopActor->StopAlarmOnActor();
			coopActor->StopCombat();
			p1->StopAlarmOnActor();
			p1->StopCombat();
		}
	}

	void PlayerActionManager::StopCurrentIdle()
	{
		// Stop any ongoing idle.

		if (coopActor)
		{
			Util::NativeFunctions::SetDontMove(coopActor.get(), false);
			if (coopActor->currentProcess)
			{
				coopActor->currentProcess->StopCurrentIdle(coopActor.get(), true);
			}
			coopActor->NotifyAnimationGraph("IdleStop");
			coopActor->NotifyAnimationGraph("attackStop");
			coopActor->NotifyAnimationGraph("moveStart");
		}
	}

	void PlayerActionManager::UpdateLastAttackingHand()
	{
		// Update the hand the player last attacked with based on what actions they are performing.

		if (IsPerformingAllOf(InputAction::kAttackLH, InputAction::kAttackRH)) 
		{
			// Both LH and RH attack.
			lastAttackingHand = HandIndex::kBoth;
		}
		else if (IsPerforming(InputAction::kAttackLH)) 
		{
			if (p->em->Has2HRangedWeapEquipped()) 
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else
			{
				lastAttackingHand = HandIndex::kLH;
			}
		}
		else if (IsPerforming(InputAction::kAttackRH))
		{
			if (p->em->Has2HMeleeWeapEquipped() || p->em->Has2HRangedWeapEquipped()) 
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else
			{
				lastAttackingHand = HandIndex::kRH;
			}
		}
		else if ((IsPerforming(InputAction::kBash)) || 
				(IsPerforming(InputAction::kSpecialAction) && reqSpecialAction == SpecialActionType::kBash))
		{
			// Set to hand of the bashing weapon.
			if (p->em->HasShieldEquipped() || p->em->HasLHWeapEquipped()) 
			{
				lastAttackingHand = HandIndex::kLH;
			}
			else if (p->em->Has2HMeleeWeapEquipped())
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else
			{
				lastAttackingHand = HandIndex::kRH;
			}
		}
		else if (IsPerforming(InputAction::kSpecialAction) && 
				(reqSpecialAction == SpecialActionType::kCastBothHands || reqSpecialAction == SpecialActionType::kDualCast)) 
		{
			// Simultaneous cast with both hands.
			lastAttackingHand = HandIndex::kBoth;
		}
		else if (IsPerforming(InputAction::kCastLH))
		{
			auto lhSpell = p->em->GetLHSpell();
			if (lhSpell && lhSpell->equipSlot == glob.bothHandsEquipSlot) 
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else if (lhSpell)
			{
				lastAttackingHand = HandIndex::kLH;
			}
		}
		else if (IsPerforming(InputAction::kCastRH))
		{
			auto rhSpell = p->em->GetRHSpell();
			if (rhSpell && rhSpell->equipSlot == glob.bothHandsEquipSlot)
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else if (rhSpell)
			{
				lastAttackingHand = HandIndex::kRH;
			}
		}
		else if (IsPerforming(InputAction::kPowerAttackDual))
		{
			lastAttackingHand = HandIndex::kBoth;
		}
		else if (IsPerforming(InputAction::kPowerAttackLH))
		{
			if (p->em->Has2HMeleeWeapEquipped())
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else
			{
				lastAttackingHand = HandIndex::kLH;
			}
		}
		else if (IsPerforming(InputAction::kPowerAttackRH))
		{
			if (p->em->Has2HMeleeWeapEquipped()) 
			{
				lastAttackingHand = HandIndex::k2H;
			}
			else
			{
				lastAttackingHand = HandIndex::kRH;
			}
		}
		else if (IsPerformingAllOf(InputAction::kSprint, InputAction::kBlock))
		{
			// Can only perform both Sprint and Block simultaneously if shield charging.
			lastAttackingHand = HandIndex::kLH;
		}
	}

	void PlayerActionManager::UpdateAVsAndCooldowns()
	{
		// Update current health, stamina, and magicka,
		// player HMS rate multipliers based on player state changes, 
		// shout cooldown, and carryweight.

		currentHealth = coopActor->GetActorValue(RE::ActorValue::kHealth);
		currentStamina = coopActor->GetActorValue(RE::ActorValue::kStamina);
		currentMagicka = coopActor->GetActorValue(RE::ActorValue::kMagicka);

		// Update player carryweights if not using the infinite carryweight setting and another player's carryweight is not imported onto P1.
		if (!Settings::bInfiniteCarryweight && glob.copiedPlayerDataTypes.none(CopyablePlayerDataTypes::kCarryWeight))
		{
			float permCarryWeightInc = coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight);
			if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && !p->isPlayer1)
			{
				float p1PermCarryWeightInc = glob.player1Actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight);
				// Should have the same base carryweight as P1, since carryweight scales with collected ice claws.
				if (permCarryWeightInc != p1PermCarryWeightInc)
				{
					coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, p1PermCarryWeightInc - permCarryWeightInc);
				}
			}
			else if (!ALYSLC::EnderalCompat::g_enderalSSEInstalled && glob.serializablePlayerData.contains(coopActor->formID))
			{
				// Get HMS AVs inc per level up.
				uint32_t iAVDhmsLevelUp = 10;
				auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
				if (valueOpt.has_value())
				{
					iAVDhmsLevelUp = valueOpt.value();
				}

				// Carryweight increase per level.
				float carryWeightIncPerLevel = 5.0f;
				valueOpt = Util::GetGameSettingFloat("fLevelUpCarryWeightMod");
				if (valueOpt.has_value())
				{
					carryWeightIncPerLevel = valueOpt.value();
				}

				// Check how many times stamina was leveled and multiply by carryweight-increase-per-level value.
				float staminaLevelInc = glob.serializablePlayerData.at(coopActor->formID)->hmsPointIncreasesList[2] / iAVDhmsLevelUp;
				float newPermCarryWeightInc = staminaLevelInc * carryWeightIncPerLevel;
				// Update if not already set.
				if (permCarryWeightInc != newPermCarryWeightInc)
				{
					coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, newPermCarryWeightInc - permCarryWeightInc);
				}
			}
		}

		// Shout cooldown must be updated manually for co-op players since
		// the high proc recovery value does not update when the player shouts.
		if (!p->isPlayer1) 
		{
			if (secsCurrentShoutCooldown > 0.0f)
			{
				secsSinceLastShout = Util::GetElapsedSeconds(p->shoutStartTP);
				if (secsSinceLastShout > secsCurrentShoutCooldown)
				{
					// Cooldown expired.
					secsCurrentShoutCooldown = 0.0f;
					secsSinceLastShout = 0.0f;
					canShout = true;
				}
				else
				{
					canShout = false;
				}
			}
			else
			{
				// No cooldown on the current shout.
				canShout = true;
			}
		}
		else if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
		{
			// Enderal: Remove arcane fever related effects, since reaching 100% arcane fever will not only
			// fail to kill P1 while in god mode, it will also completely prevent leveling up in the future.
			if (p->isInGodMode)
			{
				// Dispel arcane fever-related effects.
				for (auto effect : *coopActor->GetActiveEffectList())
				{
					if (effect)
					{
						if (effect->GetBaseObject()->data.primaryAV == RE::ActorValue::kLastFlattered || effect->GetBaseObject()->data.secondaryAV == RE::ActorValue::kLastFlattered) 
						{
							effect->Dispel(true);
						}
					}
				}

				// Reset arcane fever AVs to 0.
				if (auto avOwner = coopActor->As<RE::ActorValueOwner>(); 
					avOwner && coopActor->GetActorValue(RE::ActorValue::kLastFlattered) != 0.0f) 
				{
					// Current and base go to 0.
					coopActor->SetActorValue(RE::ActorValue::kLastFlattered, 0.0f);
					coopActor->SetBaseActorValue(RE::ActorValue::kLastFlattered, 0.0f);

					// All modifiers go to 0.
					float restoreAmount = -coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLastFlattered);
					avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kLastFlattered, restoreAmount);
					restoreAmount = -coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kLastFlattered);
					avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kLastFlattered, restoreAmount);
					restoreAmount = -coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kLastFlattered);
					avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kLastFlattered, restoreAmount);
					restoreAmount = -coopActor->GetActorValue(RE::ActorValue::kLastFlattered);
					avOwner->ModActorValue(RE::ActorValue::kLastFlattered, restoreAmount);
				}
			}
		}

		auto ui = RE::UI::GetSingleton();
		// All players need to have their stamina regen cooldowns updated while it is active.
		if (secsTotalStaminaRegenCooldown > 0.0f && (ui && !ui->GameIsPaused()))
		{
			UpdateStaminaCooldown();
		}
		else
		{
			p->lastStaminaCooldownCheckTP = SteadyClock::now();
		}

		if (coopActor->race)
		{
			// Does something? Maybe.
			/*coopActor->race->data.angleTolerance = 0.1f;
			coopActor->race->data.angleAccelerate = 0.1f;*/

			// Only update health regen to 0 here when downed.
			if (auto avOwner = coopActor->As<RE::ActorValueOwner>(); avOwner)
			{
				// Must apply combat HMS rate multiplier here for co-op companions since the game does not seem to do this for NPCs 
				// and because calling IsInCombat() on co-op companion actors is inconsistent, 
				// so first check if any player is in combat (usually P1) instead.  
				bool playerInCombat = 
				{ 
					std::any_of
					(
						glob.coopPlayers.begin(), glob.coopPlayers.end(), 
						[](auto& a_p) 
						{ 
							return a_p->isActive && a_p->coopActor->IsInCombat(); 
						}
					) 
				};

				if (p->isPlayer1) 
				{
					// Divide player-specific combat mults by default combat mults first for P1.
					baseHealthRegenRateMult = 100.0f * Settings::vfHealthRegenMult[playerID] * (playerInCombat ? Settings::vfHealthRegenCombatRatio[playerID] / 0.7f : 1.0f);
					baseMagickaRegenRateMult = 100.0f * Settings::vfMagickaRegenMult[playerID] * (playerInCombat ? Settings::vfMagickaRegenCombatRatio[playerID] / 0.33f : 1.0f);
					baseStaminaRegenRateMult = 100.0f * Settings::vfStaminaRegenMult[playerID] * (playerInCombat ? Settings::vfStaminaRegenCombatRatio[playerID] / 0.35f : 1.0f);
				}
				else
				{
					// No default mult applied, so apply the player-specific mult directly.
					baseHealthRegenRateMult = 100.0f * Settings::vfHealthRegenMult[playerID] * (playerInCombat ? Settings::vfHealthRegenCombatRatio[playerID] : 1.0f);
					baseMagickaRegenRateMult = 100.0f * Settings::vfMagickaRegenMult[playerID] * (playerInCombat ? Settings::vfMagickaRegenCombatRatio[playerID] : 1.0f);
					baseStaminaRegenRateMult = 100.0f * Settings::vfStaminaRegenMult[playerID] * (playerInCombat ? Settings::vfStaminaRegenCombatRatio[playerID] : 1.0f);
				}

				// Keep consistent with P1.
				if (!p->isPlayer1 && coopActor->GetActorValue(RE::ActorValue::kCombatHealthRegenMultiply) != 0.7f) 
				{
					coopActor->SetActorValue(RE::ActorValue::kCombatHealthRegenMultiply, 0.7f);
				}

				if ((p->isRevivingPlayer || p->isDowned) && (coopActor->GetBaseActorValue(RE::ActorValue::kHealRateMult) != 0.0f))
				{
					// Do not regen health when reviving another player or when downed.
					
					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {} is reviving another player or downed and their health regen rate is not 0. Setting now.", coopActor->GetName());

					avOwner->SetBaseActorValue(RE::ActorValue::kHealRateMult, 0.0f);
				}
				else if (!p->isRevivingPlayer && !p->isDowned && coopActor->GetBaseActorValue(RE::ActorValue::kHealRateMult) != baseHealthRegenRateMult)
				{
					// Reset to the base value once not downed or not reviving.

					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {} is not downed. Setting health regen from {} to {}.", 
						coopActor->GetName(), coopActor->GetBaseActorValue(RE::ActorValue::kHealRateMult), baseHealthRegenRateMult);

					avOwner->SetBaseActorValue(RE::ActorValue::kHealRateMult, baseHealthRegenRateMult);
				}

				bool isCasting = isInCastingAnim || IsPerformingOneOf(InputAction::kCastLH, InputAction::kCastRH, InputAction::kQuickSlotCast);
				if (isCasting && coopActor->GetBaseActorValue(RE::ActorValue::kMagickaRateMult) != 0.0f)
				{
					// Do not regen magicka when casting.
					
					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {} is casting and their magicka regen rate is not 0. Setting now.", coopActor->GetName());

					avOwner->SetBaseActorValue(RE::ActorValue::kMagickaRateMult, 0.0f);
				}
				else if (!isCasting && coopActor->GetBaseActorValue(RE::ActorValue::kMagickaRateMult) != baseMagickaRegenRateMult)
				{
					// No longer casting, so reset to the base value.

					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {} is not casting. Setting magicka regen from {} to {}.",
						coopActor->GetName(), coopActor->GetBaseActorValue(RE::ActorValue::kMagickaRateMult), baseMagickaRegenRateMult);

					avOwner->SetBaseActorValue(RE::ActorValue::kMagickaRateMult, baseMagickaRegenRateMult);
				}

				bool staminaOnCooldown = !p->isPlayer1 && secsTotalStaminaRegenCooldown != 0.0f;
				if (staminaOnCooldown && coopActor->GetBaseActorValue(RE::ActorValue::kStaminaRateMult) != 0.0f)
				{
					// Do not regen stamina when on cooldown.

					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {}'s stamina is on cooldown and their stamina regen rate is not 0. Setting now.", coopActor->GetName());

					avOwner->SetBaseActorValue(RE::ActorValue::kStaminaRateMult, 0.0f);
				}
				else if (!staminaOnCooldown && coopActor->GetBaseActorValue(RE::ActorValue::kStaminaRateMult) != baseStaminaRegenRateMult)
				{
					// Reset to base value when not on cooldown.

					// REMOVE when done debugging.
					logger::debug("[PAM] UpdateAVsAndCooldowns: {}'s stamina is not on cooldown. Setting stamina regen from {} to {}.",
						coopActor->GetName(), coopActor->GetBaseActorValue(RE::ActorValue::kStaminaRateMult), baseStaminaRegenRateMult);

					avOwner->SetBaseActorValue(RE::ActorValue::kStaminaRateMult, baseStaminaRegenRateMult);
				}
			}
		}
	}

	void PlayerActionManager::UpdateBoundWeaponTimers()
	{
		// NOTE: For co-op companions only. Bound weapons work fine for P1.
		// Since bound weapons either equip correctly but crash when unequipped/sheathing weapons,
		// or bug out the equip state completely, as in the case of the bound bow, 
		// we'll manually equip/unequip the bound weapons themselves 
		// and keep track of their lifetime separately without need for their magic effects.

		auto lhWeap = p->em->GetLHWeapon();
		auto rhWeap = p->em->GetRHWeapon();
		bool boundWeapLH = lhWeap && lhWeap->IsBound();
		bool boundWeapRH = rhWeap && rhWeap->IsBound();
		bool boundWeap2H = boundWeapRH && rhWeap->equipSlot == glob.bothHandsEquipSlot;
		// NOTE: Bound weapon duration is equal to the bound weapon effect setting's base duration
		// until I find a way to get the active effect to apply without screwing up the player's equip state 
		// and a million other things.
		if (boundWeap2H) 
		{
			// Using either time point and interval is fine, since both should be equal when a 2H bound weapon is equipped.
			secsSinceBoundWeapLHReq = secsSinceBoundWeapRHReq = Util::GetElapsedSeconds(p->lastBoundWeaponRHReqTP);
			if (secsSinceBoundWeapRHReq > secsBoundWeaponRHDuration)
			{
				// Time's up.
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Unequip right hand weapon.
					aem->UnequipObject(coopActor.get(), rhWeap);
					if (rhWeap->IsBow()) 
					{
						// Also unequip the bound arrows.
						if (auto boundArrow = p->em->equippedForms[!EquipIndex::kAmmo]; boundArrow && boundArrow->HasKeywordByEditorID("WeapTypeBoundArrow"))
						{
							aem->UnequipObject(coopActor.get(), boundArrow->As<RE::TESAmmo>());
						}
					}

					// Clear out hand slots and reset request duration data.
					p->em->EquipFists();
					secsSinceBoundWeapLHReq = secsSinceBoundWeapRHReq = 0.0f;
				}
			}
		}
		else if (boundWeapLH)
		{
			secsSinceBoundWeapLHReq = Util::GetElapsedSeconds(p->lastBoundWeaponLHReqTP);
			if (secsSinceBoundWeapLHReq > secsBoundWeaponLHDuration)
			{
				// Time's up.
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Unequip left hand weapon.
					aem->UnequipObject(coopActor.get(), lhWeap);
					// Reset request duration data.
					secsSinceBoundWeapLHReq = 0.0f;
				}
			}
		}
		else if (boundWeapRH)
		{
			secsSinceBoundWeapRHReq = Util::GetElapsedSeconds(p->lastBoundWeaponRHReqTP);
			if (secsSinceBoundWeapRHReq > secsBoundWeaponRHDuration)
			{
				// Time's up.
				if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
				{
					// Unequip right hand weapon.
					aem->UnequipObject(coopActor.get(), rhWeap);
					// Reset request duration data.
					secsSinceBoundWeapRHReq = 0.0f;
				}
			}
		}

		// Failsafes:
		// After 5 seconds, reset state if request(s) failed (no bound weapon equipped).
		float secsSinceReq = Util::GetElapsedSeconds(p->lastBoundWeaponRHReqTP);
		if (!boundWeap2H && boundWeapReqLH && boundWeapReqRH && secsSinceReq > 5.0f) 
		{
			logger::critical("[PAM] UpdateBoundWeaponTimers: {}: 2H request expired after {}s. Reset state.", coopActor->GetName(), secsSinceReq);
			boundWeapReqLH = boundWeapReqRH = false;
			secsSinceBoundWeapLHReq = secsSinceBoundWeapRHReq = 0.0f;
		}

		if (!boundWeapRH && boundWeapReqRH && secsSinceReq > 5.0f)
		{
			logger::critical("[PAM] UpdateBoundWeaponTimers: {}: RH request expired after {}s. Reset state.", coopActor->GetName(), secsSinceReq);
			boundWeapReqRH = false;
			secsSinceBoundWeapRHReq = 0.0f;
		}

		secsSinceReq = Util::GetElapsedSeconds(p->lastBoundWeaponLHReqTP);
		if (!boundWeapLH && boundWeapReqLH && secsSinceReq > 5.0f)
		{
			logger::critical("[PAM] UpdateBoundWeaponTimers: {}: LH request expired after {}s. Reset state.", coopActor->GetName(), secsSinceReq);
			boundWeapReqLH = false;
			secsSinceBoundWeapLHReq = 0.0f;
		}
	}

	void PlayerActionManager::UpdateGraphVariableStates() 
	{
		// Update cached graph variables and dependent variables.

		bool wasAttacking = isAttacking || isBashing || isInCastingAnim;
		
		coopActor->GetGraphVariableBool("IsCastingDual", isInCastingAnimDual);
		coopActor->GetGraphVariableBool("IsCastingLeft", isInCastingAnimLH);
		coopActor->GetGraphVariableBool("IsCastingRight", isInCastingAnimRH);
		isInCastingAnim = isInCastingAnimRH || isInCastingAnimLH || isInCastingAnimDual;
		
		coopActor->GetGraphVariableBool("IsAttacking", isAttacking);
		coopActor->GetGraphVariableBool("IsBlocking", isBlocking);
		coopActor->GetGraphVariableBool("bInJumpState", isJumping);
		coopActor->GetGraphVariableBool("bIsRiding", isRiding);
		coopActor->GetGraphVariableBool("IsSneaking", isSneaking);

		if (p->isPlayer1)
		{
			// Paragliding graph variable only updates for P1. 
			coopActor->GetGraphVariableBool("bParaGliding", p->mm->isParagliding);
			p->mm->isParagliding &= coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal;
		}

		if ((!wasAttacking) && (isAttacking || isBashing || isInCastingAnim)) 
		{
			// Set attack start TP if a new attack just started.
			p->lastAttackStartTP = SteadyClock::now();
		}

		// Player should not be attacking and blocking at the same time.
		if (isAttacking && isBlocking) 
		{
			coopActor->SetGraphVariableBool("IsBlocking", false);
			coopActor->SetGraphVariableInt("iState_NPCBlocking", 0);
		}

		// Attacking with a weapon.
		isWeaponAttack = isAttacking && !isInCastingAnim;
		// Attacking a ranged weapon.
		isRangedWeaponAttack = isWeaponAttack && p->em->Has2HRangedWeapEquipped();
		// Attacking with a spell or ranged weapon.
		isRangedAttack = isRangedWeaponAttack || isInCastingAnim;
	}

	void PlayerActionManager::UpdatePlayerBinds()
	{
		// Import the player's personal binds
		// and generate sets of conflicting actions for each action
		// based on these binds.

		// Player action state imported from holder using this player's ID.
		paParamsList = glob.paInfoHolder->playerPAParamsLists[playerID];
		// Initialize player action states using their corresponding params.
		// Set all actions as blocked to start.
		for (auto i = 0; i < !InputAction::kActionTotal; ++i) 
		{
			paStatesList[i] = PlayerActionState(paParamsList[i]);
			paStatesList[i].perfStage = PerfStage::kBlocked;
		}

		// Generate sets of conflicting actions per action.
		for (auto i = 0; i < !InputAction::kActionTotal; ++i)
		{
			// Composing inputs for action 1.
			const auto& compInputs1 = paStatesList[i].paParams.composingInputs;
			// Must not be disabled.
			if (paParamsList[i].perfType != PerfType::kDisabled) 
			{
				for (auto j = 0; j < !InputAction::kActionTotal; ++j)
				{
					// Composing inputs for action 2.
					const auto& compInputs2 = paStatesList[j].paParams.composingInputs;
					// Ensure not comparing to the same action and the action is not disabled.
					if (j != i && paParamsList[j].perfType != PerfType::kDisabled)
					{
						// Subset check.
						// If action 2's composing inputs set contains ALL of action 1's composing inputs,
						// action 2 conflicts with action 1. Order does not matter.
						// 
						// Example: 
						// AdjustAimPitch: LB + RB + RS movement
						// RotateCamera: RB + RS movement
						// 
						// Since RB and RS movement are both in AdjustAimPitch's composing inputs list,
						// RotateCam should be in AdjustAimPitch's conflicts set and AdjustAimPitch blocks RotateCam.
						// 
						// The reverse is not true -- RotateCam's composing inputs list does not
						// contain all of AdjustAimPitch's composing inputs,
						// so RotateCam does not block AdjustAimPitch.
						std::set<InputAction> compInputs2Set{ compInputs2.begin(), compInputs2.end() };
						bool conflicts = 
						{
							!std::any_of
							(
								compInputs1.begin(), compInputs1.end(), 
								[&compInputs2Set, &conflicts](const InputAction& a_action) 
								{ 
									return !compInputs2Set.contains(a_action); 
								}
							)
						};

						if (conflicts)
						{
							paConflictSetsList[j].insert(static_cast<InputAction>(i + !InputAction::kFirstAction));

							// REMOVE
							if (paStatesList[j].paParams.triggerFlags.none(TriggerFlag::kDoNotBlockConflictingActions))
							{
								logger::debug("[PAM] UpdatePlayerBinds: {} is blocked by conflicting action {}.",
									static_cast<InputAction>(i + !InputAction::kFirstAction),
									static_cast<InputAction>(j + !InputAction::kFirstAction));
							}
						}
					}
				}
			}
		}
	}

	void PlayerActionManager::UpdateStaminaCooldown() 
	{
		// Check how much time has elapsed since the player's stamina went on cooldown
		// and clear the total cooldown duration if that amount of time has elapsed
		// or if the player's stamina is now above zero.

		auto secsSinceOutOfStamina = Util::GetElapsedSeconds(p->outOfStaminaTP);
		auto secsSinceLastCooldownCheck = Util::GetElapsedSeconds(p->lastStaminaCooldownCheckTP);
		// Update every 0.1 seconds.
		if (secsSinceLastCooldownCheck >= 0.1f)
		{
			p->lastStaminaCooldownCheckTP = SteadyClock::now();
			// Stamina above zero check to provide compat with Valhalla Combat,
			// which restores some stamina on connected attacks.
			if (currentStamina > 0.0f || (secsTotalStaminaRegenCooldown - secsSinceOutOfStamina < 0.0f && !isSprinting))
			{
				secsTotalStaminaRegenCooldown = 0.0f;
			}
		}
	}

	void PlayerActionManager::UpdateTransformationState()
	{
		// While transforming, for companion players, 
		// equip the accompanying spells/gear for the Vampire Lord and Werewolf transformations.
		// Once transformed, check if the player is no longer transformed and update the flag accordingly.
		// Also revert werewolf transformations once the max transformation time has elapsed.

		bool wasTransformed = p->isTransformed;
		if (p->isTransformed)
		{
			// Revert werewolf/unplayable race form after 150 seconds.

			// Werewolf form reversion for co-op companion players.
			// P1's reversion already handled by the game in Skyrim.
			// Had to force the issue with Enderal.
			bool coopCompanionWerewolfFormReversion = !p->isPlayer1 && coopActor->race->formEditorID == "WerewolfBeastRace"sv;
			// Enderal reversion fails when in co-op. True fix TBD, so signal to revert here manually.
			bool enderalP1TheriantrophistFormReversion = 
			{
				ALYSLC::EnderalCompat::g_enderalSSEInstalled && p->isPlayer1 &&
				coopActor->race->formEditorID == "_00E_Theriantrophist_PlayerWerewolfRace"sv
			};

			// For compatibility with other transformations that use the Werewolf transformation spell archetype.
			// Does not include Vampire Lord transformation, which must be manually reverted via the favorited power.
			bool generalFormReversion = !coopActor->race->GetPlayable() && coopActor->race->formEditorID != "DLC1VampireBeastRace"sv;
			// Transformed for more than the time limit.
			bool timesUp = Util::GetElapsedSeconds(p->transformationTP) > p->secsMaxTransformationTime;
			bool revertedForm = false;
			if (timesUp && (coopCompanionWerewolfFormReversion || enderalP1TheriantrophistFormReversion || generalFormReversion))
			{
				revertedForm = p->RevertTransformation();
			}

			p->isTransformed = !revertedForm && coopActor->race && Util::IsRaceWithTransformation(coopActor->race);
		}
		else if (p->isTransforming)
		{
			// Ensure Vampire Lord's privates aren't showing, among other things.
			if (Util::IsVampireLord(p->coopActor.get()))
			{
				if (!p->isPlayer1) 
				{
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
					{
						// Equip base bats power.
						if (auto batsPower = dataHandler->LookupForm<RE::SpellItem>(0x38B9, "Dawnguard.esm"); batsPower)
						{
							p->em->EquipSpell(batsPower, EquipIndex::kVoice);
						}

						// Apply level-dependent Vampire Claws ability.
						RE::SpellItem* clawsSpell = nullptr;
						if (auto playerLevel = coopActor->GetLevel(); playerLevel <= 10.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A36, "Dawnguard.esm");
						}
						else if (playerLevel <= 15.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A37, "Dawnguard.esm");
						}
						else if (playerLevel <= 20.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A38, "Dawnguard.esm");
						}
						else if (playerLevel <= 25.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A39, "Dawnguard.esm");
						}
						else if (playerLevel <= 30.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A3A, "Dawnguard.esm");
						}
						else if (playerLevel <= 35.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A3B, "Dawnguard.esm");
						}
						else if (playerLevel <= 40.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A3C, "Dawnguard.esm");
						}
						else if (playerLevel <= 45.0f)
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A3D, "Dawnguard.esm");
						}
						else
						{
							clawsSpell = dataHandler->LookupForm<RE::SpellItem>(0x7A3E, "Dawnguard.esm");
						}

						// Add the spell temporarily.
						if (clawsSpell)
						{
							coopActor->AddSpell(clawsSpell);
						}

						if (auto aem = RE::ActorEquipManager::GetSingleton(); aem)
						{
							// Have some decency, lad.
							if (auto vampireLoinCloth = dataHandler->LookupForm<RE::TESObjectARMO>(0x11A84, "Dawnguard.esm"); vampireLoinCloth)
							{
								// If only using the AEM equip call or the equip console command on their own,
								// the armor is not equipped for some reason.
								const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
								const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
								if (script && aem)
								{
									script->SetCommand(fmt::format("equipitem {:X}", vampireLoinCloth->formID));
									script->CompileAndRun(coopActor.get());
								}

								aem->EquipObject(coopActor.get(), vampireLoinCloth, nullptr, 1, nullptr, false, true, false, true);
							}
						}
					}

					// Reset levitation state flags before toggling levitation for the first time.
					p->isTogglingLevitationState = false;
					p->isTogglingLevitationStateTaskRunning = false;

					// Start levitating.
					bool isLevitating = false;
					coopActor->GetGraphVariableBool("IsLevitating", isLevitating);
					if (!isLevitating) 
					{
						p->taskRunner->AddTask
						(
							[this]()
							{
								p->ToggleVampireLordLevitationTask();
							}
						);
					}
				}

				// Now transformed.
				p->isTransforming = false;
				p->isTransformed = true;
				ReadyWeapon(true);
			}
			else if (Util::IsWerewolf(coopActor.get()))
			{
				if (!p->isPlayer1)
				{
					// Apply level-dependent Werewolf Claws ability.
					RE::SpellItem* clawsSpell = nullptr;
					if (auto playerLevel = coopActor->GetLevel(); playerLevel <= 10.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl10AndBelowAbility");
					}
					else if (playerLevel <= 15.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl15AndBelowAbility");
					}
					else if (playerLevel <= 20.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl20AndBelowAbility");
					}
					else if (playerLevel <= 25.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl25AndBelowAbility");
					}
					else if (playerLevel <= 30.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl30AndBelowAbility");
					}
					else if (playerLevel <= 35.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl35AndBelowAbility");
					}
					else if (playerLevel <= 40.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl40AndBelowAbility");
					}
					else if (playerLevel <= 45.0f)
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl45AndBelowAbility");
					}
					else
					{
						clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>("PlayerWerewolfLvl50AndOverAbility");
					}

					// Add the spell temporarily.
					if (clawsSpell)
					{
						coopActor->AddSpell(clawsSpell);
					}

					// Equip base howl shout.
					// TODO: Equip P1's last equipped werewolf shout instead.
					if (auto howlOfTerror = RE::TESForm::LookupByEditorID("HowlWerewolfFear"); howlOfTerror)
					{
						p->em->EquipShout(howlOfTerror);
					}

					// Add werewolf feeding perk.
					coopActor->AddPerk(RE::TESForm::LookupByEditorID<RE::BGSPerk>("PlayerWerewolfFeed"));
				}

				// Now transformed.
				p->isTransforming = false;
				p->isTransformed = true;
				ReadyWeapon(true);
			}
			else if (Util::IsRaceWithTransformation(coopActor->race))
			{
				// Now transformed into something else.
				// Who knows what.
				p->isTransforming = false;
				p->isTransformed = true;
				ReadyWeapon(true);
			}
		}
	}

	PlayerActionManager::PlayerActionState::PlayerActionState() :
		paParams(PAParams()), perfStage(PerfStage::kInputsReleased),
		pressTP(SteadyClock::now()), releaseTP(SteadyClock::now()),
		startTP(SteadyClock::now()), stopTP(SteadyClock::now()),
		avCost(0.0f), secsPerformed(0.0f), priority(0.0f)
	{ }

	PlayerActionManager::PlayerActionState::PlayerActionState(const PAParams& a_params) :
		paParams(a_params), perfStage(PerfStage::kInputsReleased),
		pressTP(SteadyClock::now()), releaseTP(SteadyClock::now()),
		startTP(SteadyClock::now()), stopTP(SteadyClock::now()),
		avCost(0.0f), secsPerformed(0.0f), priority(0.0f)
	{ }
}

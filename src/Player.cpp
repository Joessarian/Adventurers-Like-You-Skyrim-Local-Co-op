#include "Player.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <Windows.h>
#include <Xinput.h>

#include <Compatibility.h>
#include <Controller.h>
#include <GlobalCoopData.h>
#include <Settings.h>
#include <Util.h>
#pragma comment (lib, "xinput.lib")

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	CoopPlayer::CoopPlayer() : 
		Manager(ManagerType::kP),
		taskInterface(SKSE::GetTaskInterface())
	{
		// State bools.
		extRefreshData =
		hasBeenDismissed =
		isActive =
		isBeingRevived =
		isDowned =
		isGettingUpAfterRevive = 
		isInGodMode =
		isPlayer1 =
		isRevivingPlayer =
		isTogglingLevitationState =
		isTogglingLevitationStateTaskRunning = 
		isTransformed =
		isTransforming = false;
		selfValid = true;
		selfWasInvalid = false;
		shouldTeleportToP1 = false;
		isRevived = true;
		// IDs.
		deviceID = -1;
		playerID = -1;
		// Actors.
		coopActor = nullptr;
		currentMountHandle = targetedMountHandle = RE::ActorHandle();
		// Time points.
		expendSprintStaminaTP = SteadyClock::now();
		jumpStartTP = SteadyClock::now();
		lastActivationCheckTP = SteadyClock::now();
		lastActivationStartTP = SteadyClock::now();
		lastAutoGrabTP = SteadyClock::now();
		lastCrosshairUpdateTP = SteadyClock::now();
		lastCyclingTP = SteadyClock::now();
		lastDownedTP = SteadyClock::now();
		lastGetupAfterReviveTP = SteadyClock::now();
		lastGetupTP = SteadyClock::now();
		lastHiddenInStealthRadiusTP = SteadyClock::now();
		lastLHCastChargeStartTP = SteadyClock::now();
		lastLHCastStartTP = SteadyClock::now();
		lastParaglidingStateChangeTP = SteadyClock::now();
		lastQSSCastStartTP = SteadyClock::now();
		lastReviveCheckTP = SteadyClock::now();
		lastRHCastChargeStartTP = SteadyClock::now();
		lastRHCastStartTP = SteadyClock::now();
		lastStaminaCooldownCheckTP = SteadyClock::now();
		lastSubManagerPauseTP = SteadyClock::now();
		lastSubManagerStartTP = SteadyClock::now();
		crosshairRefrVisibilityCheckTP = SteadyClock::now();
		outOfStaminaTP = SteadyClock::now();
		shoutStartTP = SteadyClock::now();
		crosshairRefrVisibilityLostTP = SteadyClock::now();
		transformationTP = SteadyClock::now();
		// Lists, sets, maps.
		analogStickParams.fill(0.0f);
		// Strings.
		lastAnimEventTag = ""sv;
		// Floats
		fullReviveHealth = 
		revivedHealth = 
		secsDowned =
		secsMaxTransformationTime =
		secsSinceInvalidPlayerMoved = 0.0f;
		// Keywords.
		aimTargetKeyword = nullptr;
		// Pre-transformation race.
		preTransformationRace = nullptr;
	}

	CoopPlayer::CoopPlayer
	(
		int32_t a_deviceID, 
		int32_t a_playerID,
		RE::Actor* a_coopActor
	) : 
		Manager(ManagerType::kP), 
		deviceID(a_deviceID), 
		playerID(a_playerID),
		coopActor(a_coopActor),
		taskInterface(SKSE::GetTaskInterface())
	{
		InitializeCoopPlayer();
	}

	void CoopPlayer::MainTask()
	{
		// Update analog stick data while the player is active.
		UpdateAnalogStickData();
		return;
	}

	void CoopPlayer::PrePauseTask()
	{
		// Set TP.
		lastSubManagerPauseTP = SteadyClock::now();
		// Pause all sub-managers at the same time.
		em->RequestStateChange(nextState);
		mm->RequestStateChange(nextState);
		pam->RequestStateChange(nextState);
		tm->RequestStateChange(nextState);
	}

	void CoopPlayer::PreStartTask()
	{
		// Set TP.
		lastSubManagerStartTP = SteadyClock::now();
		// Start all sub-managers at the same time.
		em->RequestStateChange(nextState);
		mm->RequestStateChange(nextState);
		pam->RequestStateChange(nextState);
		tm->RequestStateChange(nextState);
	}

	void CoopPlayer::RefreshData()
	{
		// Refresh all sub-managers' data at the same time.
		em->RefreshData();
		mm->RefreshData();
		pam->RefreshData();
		tm->RefreshData();
	}

	const ManagerState CoopPlayer::ShouldSelfPause()
	{
		// Await refresh once the game loads.
		if (glob.loadingASave)
		{
			return ManagerState::kAwaitingRefresh;		
		}

		// For now until keyboard + mouse support is implemented,
		// managers do not run when the player's input device is the keyboard + mouse.
		if (deviceID >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			return ManagerState::kPaused;
		}

		// Controller error check.
		/*
		XINPUT_STATE tempState{ };
		ZeroMemory(&tempState, sizeof(XINPUT_STATE));
		if (XInputGetState(deviceID, &tempState) != ERROR_SUCCESS)
		{
			DBG
			(
				"{}: controller input error for DID {}. "
				"About to pause all managers and end the co-op session.",
				coopActor->GetName(), deviceID
			);

			RE::DebugNotification
			(
				fmt::format
				(
					"[ALYSLC] ERROR: Controller {} not found. Ending session.", deviceID
				).data()
			);
			GlobalCoopData::TearDownCoopSession(true, true);

			// Must re-assign P1 device ID upon re-summoning.
			glob.player1DID = -1;
			return ManagerState::kAwaitingRefresh;
		}
		*/

		// Player dismissed or no co-op session active.
		if ((currentState != ManagerState::kUninitialized) && 
			(hasBeenDismissed || !glob.coopSessionActive)) 
		{
			return ManagerState::kAwaitingRefresh;
		}

		// Downed check.
		if (isDowned)
		{
			return ManagerState::kPaused;
		}

		// Companion player validity check. 
		// If invalid, pause and attempt to move to P1 until valid again.
		selfValid = Util::ActorIsValid(coopActor.get());
		if (!selfValid)
		{
			DBG
			(
				"Disabled: {}, 3d NOT loaded: {}, handle NOT valid: {}, "
				"NO loaded data: {}, NO current proc: {}, NO char controller: {}, "
				"parent cell NOT attached: {}",
				coopActor->IsDisabled(),
				!coopActor->Is3DLoaded(),
				!coopActor->IsHandleValid(),
				!coopActor->loadedData,
				!coopActor->currentProcess,
				!coopActor->GetCharController(),
				!coopActor->parentCell || !coopActor->parentCell->IsAttached()
			);
			selfWasInvalid = !selfValid;
			return ManagerState::kPaused;
		}

		// Open menus and P1 camera checks.
		auto ui = RE::UI::GetSingleton();
		bool player1WaitForCam = isPlayer1 && glob.cam->IsPaused();
		shouldTeleportToP1 = ShouldTeleportToP1(true);
		bool fullscreenMenuOpen = 
		(
			ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME) 	
		);
		// Pause if P1 and co-op cam are disabled,
		// or if the companion player should teleport to P1,
		// if the game is paused, 
		// saving is disabled, 
		// or if a 'fullscreen' menu is open.
		if (player1WaitForCam || 
			shouldTeleportToP1 || 
			ui->GameIsPaused() || 
			!ui->IsSavingAllowed() ||
			fullscreenMenuOpen) 
		{
			DBG
			(
				"{}: P1 wait for cam: {}, "
				"should teleport to P1: {}, game is paused: {}, "
				"saving not allowed: {}, lockpicking menu open: {}.",
				coopActor->GetName(),
				player1WaitForCam,
				shouldTeleportToP1,
				ui->GameIsPaused(),
				!ui->IsSavingAllowed(),
				fullscreenMenuOpen
			);

			return ManagerState::kPaused;
		}

		// Maintain current state otherwise.
		return currentState;
	}

	const ManagerState CoopPlayer::ShouldSelfResume()
	{
		// Maintain current state when there is no co-op session or when waiting for one to start.
		if (!glob.coopSessionActive || currentState == ManagerState::kAwaitingRefresh) 
		{
			return currentState;
		}

		// For now until keyboard + mouse support is implemented,
		// managers remain paused when the player's input device is the keyboard + mouse.
		if (deviceID >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			return currentState;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Check if the player should teleport to P1 or remain paused.
		auto ui = RE::UI::GetSingleton(); 
		if (!ui || !p1)
		{
			return ManagerState::kPaused;
		}
		
		// Controller error check.
		/*XINPUT_STATE tempState{ };
		ZeroMemory(&tempState, sizeof(XINPUT_STATE));
		if (XInputGetState(deviceID, &tempState) != ERROR_SUCCESS)
		{
			DBG
			(
				"{}: controller input error for DID {}. Remain paused.",
				coopActor->GetName(), deviceID
			);
			return currentState;
		}*/

		// Special Case:
		// Keep companion players invisible and above P1 while the title sequence plays.
		if (!isPlayer1 && ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME))
		{
			coopActor->SetAlpha(0.0f);
			if (coopActor->parentCell != p1->parentCell)
			{
				taskInterface->AddTask
				(
					[this, p1]() 
					{ 
						// Have to sheathe weapon before teleporting, 
						// otherwise the equip state gets bugged.
						pam->ReadyWeapon(false);
						DBG
						(
							"Now moving player {} to P1. Movement actor: {}", 
							coopActor->GetName(),
							mm->movementActorPtr ? 
							mm->movementActorPtr->GetName() : 
							"NONE"
						);
						if (mm->movementActorPtr != coopActor)
						{
							mm->movementActorPtr->MoveTo(p1);
						}

						coopActor->MoveTo(p1);
					}
				);
			}
			else 
			{
				coopActor->SetPosition
				(
					p1->data.location + RE::NiPoint3(0.0f, 0.0f, p1->GetHeight() * 2.0f), true
				);
			}

			return ManagerState::kPaused;
		}

		shouldTeleportToP1 = ShouldTeleportToP1(false);
		// Player validity check and resolution attempt.		
		selfValid = !shouldTeleportToP1 && Util::ActorIsValid(coopActor.get());
		if (!selfValid)
		{
			secsSinceInvalidPlayerMoved = Util::GetElapsedSeconds(invalidPlayerMovedTP);
			// Attempt to move to P1 every couple of seconds.
			if (secsSinceInvalidPlayerMoved >= Settings::fSecsBetweenInvalidPlayerMoveRequests)
			{
				// P1 must also be valid as the moveto target.
				bool player1Valid = Util::ActorIsValid(p1);
				if (player1Valid)
				{
					if (coopActor->IsHandleValid() && 
						Util::HandleIsValid(coopActor->GetHandle())) 
					{
						DBG("Moving player {} to P1.", coopActor->GetName());
						// Signal to re-equip after resuming managers.
						em->reEquipOnTeleport = true;
						// Temporary solution until I figure out what triggers 
						// the 'character controller and 3D desync warp glitch',
						// which occurs ~0.5 seconds after unpausing
						// with a player previously grabbed.
						// Ragdolling fixes the issue, 
						// but I need to find a way to detect 
						// if this desync is happening 
						// and correct it in the UpdateGrabbedReferences() call.
						// Solution for now: 
						// If grabbed by another player, 
						// release this player before moving them.
						tm->rmm->ClearPlayerIfGrabbed(tm->p);
						tm->rmm->ClearGrabbedActors(tm->p);
						taskInterface->AddTask
						(
							[this, p1]() 
							{ 
								// Since the player is ragdolled and paralyzed while downed,
								// disable to increase the likelihood that the MoveTo() 
								// call won't fail.
								if (isDowned)
								{
									coopActor->Disable();
									coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
									coopActor->PotentiallyFixRagdollState();
									coopActor->NotifyAnimationGraph("GetUpBegin");
								}

								// Have to sheathe weapon before teleporting, 
								// otherwise the equip state gets bugged.
								pam->ReadyWeapon(false);
								DBG
								(
									"Now moving player {} to P1. Movement actor: {}", 
									coopActor->GetName(),
									mm->movementActorPtr ? 
									mm->movementActorPtr->GetName() : 
									"NONE"
								);
								if (mm->movementActorPtr != coopActor)
								{
									mm->movementActorPtr->MoveTo(p1);
								}

								coopActor->MoveTo(p1);
								if (isDowned)
								{
									coopActor->Enable(false);
								}
							}
						);
					}

					selfWasInvalid = true;
				}

				invalidPlayerMovedTP = SteadyClock::now();
			}

			// Remain paused while invalid.
			return ManagerState::kPaused;
		}

		// Downed check.
		if (isDowned)
		{
			return ManagerState::kPaused;
		}

		// Menu and P1 camera checks.
		bool onlyAlwaysUnpaused= Util::MenusOnlyAlwaysUnpaused();
		bool player1WaitForCam = isPlayer1 && glob.cam->IsPaused();
		bool faderMenuOpen = 
		(
			ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) && 
			ui->GetMenu<RE::FaderMenu>()->PausesGame()
		);
		bool fullscreenMenuOpen = 
		(
			ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
			ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)
		);
		// Remain paused if temp menus are open, 
		// if P1 and co-op camera are disabled, 
		// a fader menu is open,
		// if the game is paused, 
		// saving is disabled, 
		// or if a 'fullscreen' menu is open.
		if (!onlyAlwaysUnpaused ||
			player1WaitForCam || 
			faderMenuOpen ||
			ui->GameIsPaused() || 
			!ui->IsSavingAllowed() ||
			fullscreenMenuOpen)
		{
			return ManagerState::kPaused;
		}
			
		// Re-equip hand forms if invalid earlier and not P1.
		if ((!isPlayer1) && (selfWasInvalid || !selfValid))
		{
			/*DBG("{}: Re-equip hand forms after character was invalid.", coopActor->GetName());
			em->ReEquipHandForms();*/
			selfWasInvalid = false;
		}

		DBG("{}: Resuming all co-op player manager threads.", coopActor->GetName());
		return ManagerState::kRunning;
	}

	void CoopPlayer::Update()
	{
		// Update this manager and then update all submanagers.
		Manager::Update();

		if (extRefreshData)
		{
			em->PrePauseTask();
			mm->PrePauseTask();
			pam->PrePauseTask();
			tm->PrePauseTask();
			RefreshData();
			em->PreStartTask();
			mm->PreStartTask();
			pam->PreStartTask();
			tm->PreStartTask();
			extRefreshData = false;

			// Notify the player afterward, since refreshing the targeting manager
			// clears out the crosshair text.
			tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kGeneralNotification, 
				fmt::format("P{}: Refreshed player managers", playerID + 1),
				{ 
					CrosshairMessageType::kNone, 
					CrosshairMessageType::kStealthState, 
					CrosshairMessageType::kTargetingState 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
		}
		
		// NOTE: 
		// Update funcs must be run in this order.
		em->Update();
		pam->Update();
		mm->Update();
		tm->Update();

		// Perform update when downed.
		if (isDowned)
		{
			UpdateWhenDowned();
		}

		// Make sure all players are set as essential if using the revive system.
		// Game will sometimes reset the essential flag after it is set,
		// so check each iteration.
		// Can still update even when the player submanagers are paused.
		SetEssentialForReviveSystem();
	}

	void CoopPlayer::InitializeCoopPlayer() 
	{
		// Refresh/set all members for this co-op player.
		
		// NOTE:
		// Device ID, player actor, and package form start index 
		// are already set through the constructor or UpdateCoopPlayer function at this point.

		DBG
		(
			"Init player with device/player IDs: {}, {}. {}: FID: 0x{:X}.", 
			deviceID,
			playerID, 
			isActive ? "ACTIVE" : "INACTIVE", 
			coopActor ? coopActor->formID : 0x0
		);

		// Active if the player has an assigned device ID.
		isActive = playerID != -1;
		if (isActive)
		{
			// Player-specific data.
			isPlayer1 = coopActor->IsPlayerRef();
			currentMountHandle = targetedMountHandle = RE::ActorHandle();
			extRefreshData = 
			hasBeenDismissed =
			isBeingRevived =
			isDowned =
			isGettingUpAfterRevive = 
			isInGodMode =
			isRevivingPlayer =
			isTogglingLevitationState =
			isTogglingLevitationStateTaskRunning =
			isTransformed =
			isTransforming = false;
			selfValid = true;
			selfWasInvalid = false;
			isRevived = true;
			// Time points.
			expendSprintStaminaTP = SteadyClock::now();
			jumpStartTP = SteadyClock::now();
			lastActivationCheckTP = SteadyClock::now();
			lastActivationStartTP = SteadyClock::now();
			lastAutoGrabTP = SteadyClock::now();
			lastCrosshairUpdateTP = SteadyClock::now();
			lastLHCastStartTP = SteadyClock::now();
			lastDownedTP = SteadyClock::now();
			lastGetupTP = SteadyClock::now();
			lastHiddenInStealthRadiusTP = SteadyClock::now();
			lastParaglidingStateChangeTP = SteadyClock::now();
			lastQSSCastStartTP = SteadyClock::now();
			lastReviveCheckTP = SteadyClock::now();
			lastRHCastStartTP = SteadyClock::now();
			lastStaminaCooldownCheckTP = SteadyClock::now();
			lastStealthStateCheckTP = SteadyClock::now();
			lastSubManagerPauseTP = SteadyClock::now();
			lastSubManagerStartTP = SteadyClock::now();
			crosshairRefrVisibilityCheckTP = SteadyClock::now();
			outOfStaminaTP = SteadyClock::now();
			shoutStartTP = SteadyClock::now();
			crosshairRefrVisibilityLostTP = SteadyClock::now();
			transformationTP = SteadyClock::now();
			// Lists, sets, maps.
			analogStickParams.fill(0.0f);
			// Strings.
			lastAnimEventTag = ""sv;
			// Floats.
			fullReviveHealth = 
			(
				0.5f * Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kHealth)
			);
			revivedHealth = secsDowned = 0.0f;
			secsMaxTransformationTime = 150.0f;
			secsSinceInvalidPlayerMoved = 0.0f;
			// Aim target keyword
			auto keywordForm = RE::TESForm::LookupByEditorID
			(
				fmt::format("__CoopAimTarget{}", playerID + 1)
			);
			aimTargetKeyword = keywordForm ? keywordForm->As<RE::BGSKeyword>() : nullptr;
			// Pre-transformation race.
			preTransformationRace = nullptr;
			// Ensure all players' factions are equivalent to P1's.
			SyncPlayerFactions();
			// Set player actor flags.
			SetCoopPlayerFlags();
			// Add serialized perks to the player.
			GlobalCoopData::ImportUnlockedPerks(coopActor.get());

			// Skyrim's Paraglider compat: check if P1 has a paraglider.
			if (isPlayer1 && 
				coopActor.get() && 
				ALYSLC::SkyrimsParagliderCompat::g_installed)
			{
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					auto paraglider = dataHandler->LookupForm<RE::TESObjectMISC>
					(
						0x802, "Paragliding.esp"
					); 
					if (paraglider)
					{
						auto invCounts = coopActor->GetInventoryCounts();
						const auto iter = invCounts.find(paraglider);
						ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider = 
						(
							iter != invCounts.end() &&
							iter->second > 0
						);
						// Add gale spell if not known already.
						// Enderal only, since the quest to obtain it is not compatible.
						if (ALYSLC::EnderalCompat::g_installed &&
							ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
							!coopActor->HasSpell(glob.tarhielsGaleSpell))
						{
							coopActor->AddSpell(glob.tarhielsGaleSpell);
						}
					}
				}
			}

			// Make sure the companion player is and remains on amicable terms with P1.
			if (!isPlayer1)
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				const auto scriptFactory = 
				(
					RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
				);
				const auto script = 
				(
					scriptFactory ? scriptFactory->Create() : nullptr
				);
				if (script)
				{
					script->SetCommand
					(
						fmt::format("setrelationshiprank {:X} 3", p1->formID)
					);
					script->CompileAndRun(coopActor.get());
					script->SetCommand
					(
						fmt::format("setrelationshiprank {:X} 3", coopActor->formID)
					);
					script->CompileAndRun(p1);
					delete script;
				}

				coopActor->SetActorValue(RE::ActorValue::kAggression, 0.0f);
				coopActor->SetActorValue(RE::ActorValue::kConfidence, 4.0f);
			}

			if (em && mm && pam && tm && taskRunner) 
			{
				// Prepare managers for data refresh when signalled to resume.
				// No need to construct new managers.
				RequestStateChange(ManagerState::kAwaitingRefresh);
			}
			else 
			{
				// Otherwise, this player has not been fully constructed before,
				// and must create new equip, movement, player action, and targeting managers.
				// Plus a task runner.
				em = std::make_unique<EquipManager>();
				mm = std::make_unique<MovementManager>();
				pam = std::make_unique<PlayerActionManager>();
				tm = std::make_unique<TargetingManager>();
				taskRunner = std::make_unique<TaskRunner>
				(
					fmt::format("[P{}]", playerID + 1).c_str()
				);
				// Set as unitialized.
				RequestStateChange(ManagerState::kUninitialized);
			}
		}
	}

	void CoopPlayer::UpdateAnalogStickData()
	{
		// Update player movement parameters derived from controller analog stick movement
		// in both in-game coordinates and absolute coordinates.

		if (!glob.globalDataInit || 
			!glob.allPlayersInit ||
			!isActive ||
			deviceID >= ALYSLC_MAX_CONTROLLER_COUNT)
		{
			return;
		}

		const auto& lsData = glob.cdh->GetAnalogStickState(deviceID, true);
		const auto& rsData = glob.cdh->GetAnalogStickState(deviceID, false);
		// Analog stick components and normalized displacement magnitudes.
		const float& lsX = lsData.xComp;
		const float& lsY = lsData.yComp;
		const float& rsX = rsData.xComp;
		const float& rsY = rsData.yComp;
		const float& lsMag = lsData.normMag;
		const float& rsMag = rsData.normMag;
		// Orientation angle of controller thumbsticks. 
		// NOT relative to the camera.
		float lsGameAng = 0.0f;
		float rsGameAng = 0.0f;
		// Components of thumbstick displacement vectors.
		float lxComp = 0.0f;
		float lyComp = 0.0f;
		float rxComp = 0.0f;
		float ryComp = 0.0f;

		// Get camera yaw angle.
		auto playerCam = RE::PlayerCamera::GetSingleton();
		float camYaw = glob.cam->GetCurrentYaw();
		// Game yaw angle relative to the camera for the LS.
		float lsCamRelAng = 0.0f;
		// Game yaw angle relative to the camera for the RS.
		float rsCamRelAng = 0.0f;
		// Obtain Cartesian angle for left stick orientation.
		if (lsX == 0.0f && lsY == 0.0f) 
		{
			// Previous, no change, since the LS is centered.
			lsGameAng = analogStickParams[!AnalogStickParams::kLSGameAng];
		}
		else
		{
			lsGameAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(atan2f(lsY, lsX)));
		}

		if (rsX == 0.0f && rsY == 0.0f) 
		{
			// Previous, no change, since the RS is centered.
			rsGameAng = analogStickParams[!AnalogStickParams::kRSGameAng];
		}
		else
		{
			rsGameAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(atan2f(rsY, rsX)));
		}

		// Yaw angles for both analog sticks in the world's coordinate space 
		// (relative to the camera).
		lsCamRelAng = Util::NormalizeAng0To2Pi(camYaw + lsGameAng);
		rsCamRelAng = Util::NormalizeAng0To2Pi(camYaw + rsGameAng);

		// Get the absolute change in LS angle since the last check.
		const float deltaLSGameAngMag = Util::NormalizeAngToPi
		(
			fabsf(lsGameAng - analogStickParams[!AnalogStickParams::kLSGameAng])
		);
		const float deltaRSGameAngMag = Util::NormalizeAngToPi
		(
			fabsf(rsGameAng - analogStickParams[!AnalogStickParams::kRSGameAng])
		);

		// Get X, Y components for both analog sticks, with respect to the camera's yaw.
		if (rsMag != 0.0f)
		{
			rsCamRelAng = Util::ConvertAngle(rsCamRelAng);
			rxComp = cosf(rsCamRelAng);
			ryComp = sinf(rsCamRelAng);
			rsCamRelAng = Util::ConvertAngle(rsCamRelAng);
		}
		else
		{
			// Unchanged.
			rsCamRelAng = analogStickParams[!AnalogStickParams::kRSCamRelAng];
			rxComp = 0.0f;
			ryComp = 0.0f;
		}

		if (lsMag != 0.0f)
		{
			lsCamRelAng = Util::ConvertAngle(lsCamRelAng);
			lxComp = cosf(lsCamRelAng);
			lyComp = sinf(lsCamRelAng);
			lsCamRelAng = Util::ConvertAngle(lsCamRelAng);
		}
		else
		{
			// Unchanged.
			lsCamRelAng = analogStickParams[!AnalogStickParams::kLSCamRelAng];
			lxComp = 0.0f;
			lyComp = 0.0f;
		}
		
		// Update moved flags next.
		bool prevLSMoved = lsMoved;
		// LS/RS stopped when centered for two frames (norm mag is 0 this frame and last frame).
		bool prevMoved = lsData.prevNormMag != 0.0f;
		lsMoved = prevMoved || lxComp != 0.0f || lyComp != 0.0f;
		prevMoved = rsData.prevNormMag != 0.0f;
		rsMoved = prevMoved || rxComp != 0.0f || ryComp != 0.0f;
		if (prevLSMoved && !lsMoved) 
		{
			lastMovementStopReqTP = SteadyClock::now();
		}
		else if (!prevLSMoved && lsMoved)
		{
			lastMovementStartReqTP = SteadyClock::now();
		}

		// All angles are in game coordinates before adding to params list.
		analogStickParams[!AnalogStickParams::kLSXComp] = lxComp;
		analogStickParams[!AnalogStickParams::kLSYComp] = lyComp;
		analogStickParams[!AnalogStickParams::kRSXComp] = rxComp;
		analogStickParams[!AnalogStickParams::kRSYComp] = ryComp;
		analogStickParams[!AnalogStickParams::kLSCamRelAng] = lsCamRelAng;
		analogStickParams[!AnalogStickParams::kRSCamRelAng] = rsCamRelAng;
		analogStickParams[!AnalogStickParams::kDeltaLSGameAngMag] = deltaLSGameAngMag;
		analogStickParams[!AnalogStickParams::kDeltaRSGameAngMag] = deltaRSGameAngMag;
		analogStickParams[!AnalogStickParams::kLSGameAng] = lsGameAng;
		analogStickParams[!AnalogStickParams::kRSGameAng] = rsGameAng;
		analogStickParams[!AnalogStickParams::kLSCamRelAngMovingFromCenter] = 
		(
			lsData.MovingAwayFromCenter() ? 
			lsCamRelAng : 
			analogStickParams[!AnalogStickParams::kLSCamRelAngMovingFromCenter]
		);
		analogStickParams[!AnalogStickParams::kRSCamRelAngMovingFromCenter] = 
		(
			rsData.MovingAwayFromCenter() ? 
			rsCamRelAng : 
			analogStickParams[!AnalogStickParams::kRSCamRelAngMovingFromCenter]
		);
	}

	void CoopPlayer::UpdateCoopPlayer
	(
		int32_t a_deviceID, int32_t a_playerID, RE::Actor* a_coopActor
	)
	{
		// Update an already-constructed co-op player by setting the given data 
		// and refreshing all other members.

		DBG
		(
			"Updating co-op player: {}, DID: {}, PID: {}.", 
			a_coopActor ? a_coopActor->GetName() : "NONE", 
			a_deviceID,
			a_playerID
		);

		if (a_deviceID < 0 || a_playerID < 0 || a_playerID >= ALYSLC_MAX_PLAYER_COUNT)
		{
			ERR
			(
				"{}: invalid DID or PID: {}, {}.",
				coopActor ? coopActor->GetName() : "NONE", a_deviceID, a_playerID
			);

			// Set as inactive.
			isActive = false;
			return;
		}
		
		deviceID = a_deviceID;
		playerID = a_playerID;
		coopActor = RE::ActorPtr(a_coopActor);
		taskInterface = SKSE::GetTaskInterface();
		InitializeCoopPlayer();
	}

	void CoopPlayer::CopyNPCAppearanceToPlayer
	(
		RE::TESNPC* a_baseToCopy, bool a_setOppositeGenderAnims
	)
	{
		// Update gender and body-related data by copying the given base NPC's appearance 
		// to the player's character.

		// Make sure all the data we require is valid first.
		if (!a_baseToCopy || 
			!a_baseToCopy->race || 
			!a_baseToCopy->race->faceRelatedData ||
			!coopActor || 
			!coopActor->race || 
			!coopActor->race->faceRelatedData ||
			!coopActor->GetActorBase() || 
			!coopActor->GetActorBase()->race)
		{
			return;
		}

		DBG
		(
			"Copying {}'s appearance to {}, "
			"set opposite gender animations: {}, "
			"current race, race to set: {}, {}, equal: {}.",
			a_baseToCopy ? a_baseToCopy->GetName() : "NONE", 
			coopActor->GetName(), 
			a_setOppositeGenderAnims,
			coopActor->race ? coopActor->race->GetName() : "NONE",
			a_baseToCopy && a_baseToCopy->race ? a_baseToCopy->race->GetName() : "NONE",
			coopActor->race == a_baseToCopy->race
		);

		auto actorBase = coopActor->GetActorBase();
		DBG
		(
			"Base is female: {}, current is female: {}, "
			"current uses opposite gender anims: {}, "
			"req opposite gender anims: {}, "
			"should change gender: {}, should set opposite gender anims: {}.",
			a_baseToCopy->IsFemale(),
			actorBase->IsFemale(),
			actorBase->UsesOppositeGenderAnims(),
			a_setOppositeGenderAnims,
			(!a_baseToCopy->IsFemale() && actorBase->IsFemale()) || 
			(a_baseToCopy->IsFemale() && !actorBase->IsFemale()),
			(actorBase->UsesOppositeGenderAnims() && !a_setOppositeGenderAnims) || 
			(!actorBase->UsesOppositeGenderAnims() && a_setOppositeGenderAnims)
		);
		// Update race and gender before importing headparts from the new actor base.
		Util::SetActorRaceAndGender
		(
			coopActor.get(),
			a_baseToCopy->race,
			a_baseToCopy->GetSex() == RE::SEX::kFemale, 
			a_setOppositeGenderAnims
		);
		// Remove all the player's current headparts.
		Util::RemoveAllHeadParts(coopActor.get());
		// Add new headparts from NPC to the player.
		Util::ImportHeadPartsFromBase(a_baseToCopy, actorBase);
		DBG
		(
			"Imported {}'s appearance to {}", a_baseToCopy->GetName(), coopActor->GetName()
		);
	}

	void CoopPlayer::DismissPlayer() 
	{
		// Dismiss co-op companion if dead, the co-op session ended, 
		// or if the Summoning Menu is about to open.
		
		glob.canStartCoopGlob->value =  false;
		// Stop managers first.
		RequestStateChange(ManagerState::kAwaitingRefresh);
		// Remove essential flag on dismissal.
		if (!isPlayer1 || !glob.p1IsEssential)
		{
			Util::ChangeEssentialStatus(coopActor.get(), false);
		}

		// Revert to original race if transformed.
		RevertTransformation();

		// Ensure player is not set to downed.
		isDowned = false;
		isRevived = false;
		isGettingUpAfterRevive = false;
		hasBeenDismissed = true;

		// Have player Papyrus script run its cleanup.

		// Remove co-op player keywords.
		pam->UpdateCoopPlayerKeyword(false);
		mm->ClearKeepOffsetFromActor();

		if (isPlayer1)
		{
			Util::ToggleAllControls(true);
		}
		else if (coopActor->Is3DLoaded())
		{
			taskRunner->AddTask
			(
				coopActor->GetName(),
				__FUNCTION__,
				[this](){ GlobalCoopData::TeleportToP1OrAwayTask(coopActor->GetHandle(), false); }
			);
		}

		DBG
		(
			"Handled dismissal of {} (0x{:X}).", 
			coopActor->GetName(), coopActor->formID
		);
		glob.canStartCoopGlob->value = true;
	}

	std::string CoopPlayer::GetHMSStatNotificationText()
	{
		// Get health/magicka/stamina stat notification text for this player.
		// Empty string if health/magicka/stamina are all full or if the player is downed.

		std::string hmsText = "";
		if (isDowned)
		{
			return hmsText;
		}

		// Is full if all 3 actor values are close enough to their full amounts.
		bool hmsFull = true;
		const float currentHealth = coopActor->GetActorValue(RE::ActorValue::kHealth);
		float healthPercent = 
		(
			100.0f * 
			currentHealth /
			Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kHealth)
		);
		if (isnan(healthPercent) || isinf(healthPercent))
		{
			healthPercent = 0.0f;
			hmsFull = false;
		}
		else if (healthPercent < 99.999f && hmsFull)
		{
			hmsFull = false;
		}
		
		const float currentMagicka = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		float magickaPercent = 
		(
			100.0f * 
			currentMagicka /
			Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kMagicka)
		);
		if (isnan(magickaPercent) || isinf(magickaPercent))
		{
			magickaPercent = 0.0f;
			hmsFull = false;
		}
		else if (magickaPercent < 99.999f && hmsFull)
		{
			hmsFull = false;
		}
		
		const float currentStamina = coopActor->GetActorValue(RE::ActorValue::kStamina);
		float staminaPercent = 
		(
			100.0f * 
			currentStamina /
			Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kStamina)
		);
		if (isnan(staminaPercent) || isinf(staminaPercent))
		{
			staminaPercent = 0.0f;
			hmsFull = false;
		}
		else if (staminaPercent < 99.999f && hmsFull)
		{
			hmsFull = false;
		}

		// Nothing to show if full.
		if (hmsFull)
		{
			return hmsText;
		}

		if (tm->crosshairMessage->text != "")
		{
			hmsText += "\n";
		}
		else
		{
			hmsText = fmt::format("P{}: ", playerID + 1);
		}

		hmsText += fmt::format
		(
			"<font color=\"#C41B1E\">[</font>"
			"<font color=\"#{:X}\">{:.0f}</font>"
			"<font color=\"#C41B1E\">]</font>", 
			Util::GetGrayscalePercentRGB(healthPercent),
			currentHealth
		);
		hmsText += fmt::format
		(
			" <font color=\"#243B9F\">[</font>"
			"<font color=\"#{:X}\">{:.0f}</font>"
			"<font color=\"#243B9F\">]</font>",
			Util::GetGrayscalePercentRGB(magickaPercent),
			currentMagicka
		);
		hmsText += fmt::format
		(
			" <font color=\"#1BBD4C\">[</font>"
			"<font color=\"#{:X}\">{:.0f}</font>"
			"<font color=\"#1BBD4C\">]</font>", 
			Util::GetGrayscalePercentRGB(staminaPercent),
			currentStamina
		);

		return hmsText;
	}

	void CoopPlayer::RequestSubManagerStateChange(ManagerState&& a_newState)
	{
		// Change sub managers' running states.
		em->RequestStateChange(a_newState);
		mm->RequestStateChange(a_newState);
		pam->RequestStateChange(a_newState);
		tm->RequestStateChange(a_newState);
	}

	void CoopPlayer::ResetPlayer1()
	{
		// Debug option to reset P1 when glitches occur.

		if (!isPlayer1)
		{
			return;
		}
		
		auto lhForm = em->desiredForms[!EquipIndex::kLeftHand];
		auto rhForm = em->desiredForms[!EquipIndex::kRightHand];
		auto equipSlot = glob.eitherHandEquipSlot;
		auto lhEquipType = lhForm ? lhForm->As<RE::BGSEquipType>() : nullptr;
		auto rhEquipType = rhForm ? rhForm->As<RE::BGSEquipType>() : nullptr;
		// Get off mount/stop interacting with furniture.
		coopActor->StopInteractingQuick(true);

		bool wasTransformed = isTransforming || isTransformed;
		// Sheathe current weapons first.
		pam->QueueP1ButtonEvent
		(
			InputAction::kSheathe,
			RE::INPUT_DEVICE::kGamepad, 
			ButtonEventPressType::kInstantTrigger
		);

		// Save health and active effects to restore after resurrection.
		float healthBefore = coopActor->GetActorValue(RE::ActorValue::kHealth);
		float magickaBefore = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		float staminaBefore = coopActor->GetActorValue(RE::ActorValue::kStamina);
		auto effectList = coopActor->GetActiveEffectList();
		std::set<RE::MagicItem*> activeEffectSpells{ };
		std::unordered_map<RE::MagicItem*, float> effectToElapsedMap{ };
		if (effectList)
		{
			for (const auto effect : *effectList)
			{
				if (!effect)
				{
					continue;
				}
				
				// REMOVE when done debugging.
				DBG
				(
					"BEFORE {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
					"elapsed time: {}, duration: {}.",
					fmt::ptr(effectList),
					coopActor->GetName(),
					effect->effect && effect->effect->baseEffect ? 
					effect->effect->baseEffect->GetName() :
					"NONE",
					effect->effect && effect->effect->baseEffect ?
					effect->effect->baseEffect->formID :
					0xDEAD,
					effect->spell ? 
					effect->spell->GetName() :
					"NONE",
					effect->elapsedSeconds,
					effect->duration
				);
				if (effect->spell)
				{
					activeEffectSpells.insert(effect->spell);
					effectToElapsedMap.insert_or_assign(effect->spell, effect->elapsedSeconds);
				}
			}
		}

		// Resetting 3D can cause crashes.
		coopActor->Resurrect(false, false);

		// Clear out all active effects before re-applying to ensure no duplicates are proc'd.
		effectList = coopActor->GetActiveEffectList();
		if (effectList)
		{
			for (const auto effect : *effectList)
			{
				if (!effect)
				{
					continue;
				}

				// REMOVE when done debugging.
				DBG
				(
					"AFTER1 {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
					"elapsed time: {}, duration: {}.",
					fmt::ptr(effectList),
					coopActor->GetName(),
					effect->effect && effect->effect->baseEffect ? 
					effect->effect->baseEffect->GetName() :
					"NONE",
					effect->effect && effect->effect->baseEffect ?
					effect->effect->baseEffect->formID :
					0xDEAD,
					effect->spell ? 
					effect->spell->GetName() :
					"NONE",
					effect->elapsedSeconds,
					effect->duration
				);
			}

			effectList->clear();
		}
		else
		{
			DBG("No active effects list after resurrection.");
		}
		
		//coopActor->CastPermanentMagic(true, true, true, true);
		// Proc all the previous active effects.	
		auto instantCaster = coopActor->GetMagicCaster
		(
			RE::MagicSystem::CastingSource::kInstant
		);
		if (instantCaster)
		{
			for (const auto spell : activeEffectSpells)
			{
				if (!spell)
				{
					continue;
				}

				DBG
				(
					"Casting spell {} (0x{:X}).", spell->GetName(), spell->formID
				);
				instantCaster->CastSpellImmediate
				(
					spell, true, coopActor.get(), 1.0f, false, 0.0f, nullptr
				);
			}
		}

		// Might have caused a crash previously. Commented out for now.
		// Re-equip all items to add their active effects to the mix.
		/*
		em->ReEquipAll(false);

		effectList = coopActor->GetActiveEffectList();
		if (effectList)
		{
			for (const auto effect : *effectList)
			{
				if (!effect)
				{
					continue;
				}

				// Restore elapsed time.
				if (effect->spell)
				{
					auto iter = effectToElapsedMap.find(effect->spell); 
					if (iter != effectToElapsedMap.end())
					{
						effect->elapsedSeconds = iter->second;
					}
				}
				
				// REMOVE when done debugging.
				DBG
				(
					"AFTER2 {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
					"elapsed time: {}, duration: {}.",
					fmt::ptr(effectList),
					coopActor->GetName(),
					effect->effect && effect->effect->baseEffect ? 
					effect->effect->baseEffect->GetName() :
					"NONE",
					effect->effect && effect->effect->baseEffect ?
					effect->effect->baseEffect->formID :
					0xDEAD,
					effect->spell ? 
					effect->spell->GetName() :
					"NONE",
					effect->elapsedSeconds,
					effect->duration
				);
			}
		}
		*/

		// Restore the original values when done.
		const float healthAfter = coopActor->GetActorValue(RE::ActorValue::kHealth);
		if (healthAfter != healthBefore)
		{
			pam->ModifyAV
			(
				RE::ActorValue::kHealth,
				healthBefore - healthAfter,
				healthBefore - healthAfter < 0.0f
			);
		}

		const float magickaAfter = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		if (magickaAfter != magickaBefore)
		{
			pam->ModifyAV
			(
				RE::ActorValue::kMagicka,
				magickaBefore - magickaAfter,
				magickaBefore - magickaAfter < 0.0f
			);
		}

		const float staminaAfter = coopActor->GetActorValue(RE::ActorValue::kStamina);
		if (staminaAfter != staminaBefore)
		{
			pam->ModifyAV
			(
				RE::ActorValue::kStamina,
				staminaBefore - staminaAfter,
				staminaBefore - staminaAfter < 0.0f
			);
		}

		// NOTE on the apparent jank (likely from a modded game):
		// Ok, sooo... after a few days of banging my head against the wall,
		// here's a bandaid solution to failing to restore P1's HMS on menu exit.
		// Some active effects, such as those applied by perks,
		// will latently modify P1's HMS modifiers after the menu closes
		// and apply the changes after we've properly restored P1's HMS.
		// Since this smells like script lag, we'll wait until P1's managers resume + 1 second 
		// before restoring the correct HMS values once again.
		/*
		taskRunner->AddTask
		(
			coopActor->GetName(),
			__FUNCTION__,
			[this, healthBefore, magickaBefore, staminaBefore]()
			{
				SteadyClock::time_point waitStartTP = SteadyClock::now();
				float secsWaited = 0.0f;
				while (!IsRunning() && secsWaited < 2.0f)
				{
					std::this_thread::sleep_for(0.1s);
					secsWaited = Util::GetElapsedSeconds(waitStartTP);
				}
					
				std::this_thread::sleep_for(1.0s);
				Util::AddSyncedTask
				(
					[this, &healthBefore, magickaBefore, staminaBefore]()
					{
						float change = 
						(
							healthBefore - 
							coopActor->GetActorValue(RE::ActorValue::kHealth)
						);
						if (change != 0.0f)
						{
							pam->ModifyAV
							(
								RE::ActorValue::kHealth, change, change < 0.0f
							);
						}

						change = 
						(
							magickaBefore - 
							coopActor->GetActorValue(RE::ActorValue::kMagicka)
						);
						if (change != 0.0f)
						{
							pam->ModifyAV
							(
								RE::ActorValue::kMagicka, change, change < 0.0f
							);
						}

						change = 
						(
							staminaBefore - 
							coopActor->GetActorValue(RE::ActorValue::kStamina)
						);
						if (change != 0.0f)
						{
							pam->ModifyAV
							(
								RE::ActorValue::kStamina, change, change < 0.0f
							);
						}
							
						DBG
						(
							"{}: HMS before, after: ({}, {}, {}), ({}, {}, {}).",
							coopActor->GetName(),
							healthBefore,
							magickaBefore,
							staminaBefore,
							coopActor->GetActorValue(RE::ActorValue::kHealth),
							coopActor->GetActorValue(RE::ActorValue::kMagicka),
							coopActor->GetActorValue(RE::ActorValue::kStamina)
						);
					}
				);
			}
		);
		*/

		// Re-attach havok.
		coopActor->DetachHavok(coopActor->GetCurrent3D());
		coopActor->InitHavok();
		coopActor->MoveHavok(true);

		// Make sure the player is not paralyzed and attempt to fix their ragdoll state.
		coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
		coopActor->PotentiallyFixRagdollState();

		// Revert any transformation, if needed.
		// Reverting right before resurrection causes a vertices glitch.
		// P1 gets spaghettified.
		// Sheathe weapons first again.
		pam->ReadyWeapon(false);
		if (wasTransformed)
		{
			RevertTransformation();
		}

		// Re-equip hand forms.
		em->ReEquipHandForms();
		pam->ReadyWeapon(true);
		// Reset 'ghost' flag used for I-frames.
		if (auto actorBase = coopActor->GetActorBase(); actorBase)
		{
			actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
		}
	}

	bool CoopPlayer::RevertTransformation()
	{
		// Only revert form if transformed.
		// Return true if successful.
		
		if (!coopActor || !coopActor->race || !coopActor->GetActorBase())
		{
			return false;
		}
		
		const auto p1 = RE::PlayerCharacter::GetSingleton();
		// Default to the actor base's reported original race or P1's chargen race.
		auto originalRace = 
		(
			preTransformationRace ? 
			preTransformationRace : 
			isPlayer1 && p1 ? 
			p1->charGenRace :
			coopActor->GetActorBase()->originalRace
		);
		// Each time a companion player customizes their character,
		// their race is saved to the serialized data,
		// so elect for that saved race, if available, 
		// as the reported original race can sometimes be overwritten by the transformation race,
		// which would prevent players from transforming back into their original race.
		const auto iter = glob.serializablePlayerData.find(coopActor->formID);
		if (iter != glob.serializablePlayerData.end())
		{
			if (iter->second->chosenRace)
			{
				originalRace = iter->second->chosenRace;
			}
		}
		
		// Do not revert if the pre-transformation race is the same as the player's current race 
		// or the player is going from a race without a transformation 
		// to one with a transformation (ex. Nord to Werewolf).
		bool currentRaceHasTransformation = Util::IsRaceWithTransformation(coopActor->race);
		bool originalRaceHasTransformation = Util::IsRaceWithTransformation(originalRace);
		bool skipTransformation = 
		(
			(!originalRace || originalRace == coopActor->race) ||
			(!currentRaceHasTransformation)
		);
		DBG
		(
			"{}: Pre-transform race: {}, original: {}, current: {}, chosen: {}, "
			"is transformed: {}. Skip: {}.",
			coopActor->GetName(),
			preTransformationRace ? preTransformationRace->GetName() : "NONE",
			originalRace ? originalRace->GetName() : "NONE",
			coopActor->race ? coopActor->race->GetName() : "NONE",
			iter != glob.serializablePlayerData.end() &&
			iter->second->chosenRace ? 
			iter->second->chosenRace->GetName() : 
			"NONE",
			isTransformed,
			skipTransformation
		);
		if (skipTransformation)
		{
			return false;
		}
		
		// Unequip transformation-specific spells that were equipped
		// when the companion player transformed.
		if (!isPlayer1)
		{
			if (Util::IsWerewolf(coopActor.get()))
			{
				// Unequip base howl shout.
				auto howlOfTerror = RE::TESForm::LookupByEditorID("HowlWerewolfFear"); 
				if (howlOfTerror)
				{
					em->UnequipShout(howlOfTerror);
				}

				// Remove level-dependent Werewolf Claws spell.
				RE::SpellItem* clawsSpell = nullptr;
				if (auto playerLevel = coopActor->GetLevel(); playerLevel <= 10.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl10AndBelowAbility"
					);
				}
				else if (playerLevel <= 15.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl15AndBelowAbility"
					);
				}
				else if (playerLevel <= 20.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl20AndBelowAbility"
					);
				}
				else if (playerLevel <= 25.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl25AndBelowAbility"
					);
				}
				else if (playerLevel <= 30.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl30AndBelowAbility"
					);
				}
				else if (playerLevel <= 35.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl35AndBelowAbility"
					);
				}
				else if (playerLevel <= 40.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl40AndBelowAbility"
					);
				}
				else if (playerLevel <= 45.0f)
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl45AndBelowAbility"
					);
				}
				else
				{
					clawsSpell = RE::TESForm::LookupByEditorID<RE::SpellItem>
					(
						"PlayerWerewolfLvl50AndOverAbility"
					);
				}

				if (clawsSpell)
				{
					coopActor->RemoveSpell(clawsSpell);
				}

				// Play transformation shader.
				auto revertFX = RE::TESForm::LookupByEditorID<RE::TESEffectShader>
				(
					"WerewolfTrans02FXS"
				); 
				if (revertFX)
				{
					Util::StartEffectShader(coopActor.get(), revertFX, 5.0f);
				}

				// Remove feeding perk.
				coopActor->RemovePerk
				(
					RE::TESForm::LookupByEditorID<RE::BGSPerk>
					(
						"PlayerWerewolfFeed"
					)
				);
			}
			else if (Util::IsVampireLord(coopActor.get()))
			{
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					// Unequip base bats power.
					auto batsPower = dataHandler->LookupForm<RE::SpellItem>
					(
						0x38B9, "Dawnguard.esm"
					); 
					if (batsPower)
					{
						em->UnequipSpell(batsPower, EquipIndex::kVoice);
					}

					// Remove level-dependent Vampire Claws spell.
					RE::SpellItem* clawsSpell = nullptr;
					if (auto playerLevel = coopActor->GetLevel(); playerLevel <= 10.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A36, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 15.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A37, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 20.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A38, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 25.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A39, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 30.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A3A, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 35.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A3B, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 40.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A3C, "Dawnguard.esm"
						);
					}
					else if (playerLevel <= 45.0f)
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A3D, "Dawnguard.esm"
						);
					}
					else
					{
						clawsSpell = dataHandler->LookupForm<RE::SpellItem>
						(
							0x7A3E, "Dawnguard.esm"
						);
					}

					if (clawsSpell)
					{
						coopActor->RemoveSpell(clawsSpell);
					}

					// Unequip loincloth by removing it.
					// Humans can't/don't want to wear it anyways.
					auto vampireLoinCloth = dataHandler->LookupForm<RE::TESObjectARMO>
					(
						0x11A84, "Dawnguard.esm"
					); 
					if (vampireLoinCloth)
					{
						coopActor->RemoveItem
						(
							vampireLoinCloth, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
						);
					}

					// Play transformation shader.
					auto revertFX = dataHandler->LookupForm<RE::TESEffectShader>
					(
						0x15372, "Dawnguard.esm"
					); 
					if (revertFX)
					{
						Util::StartEffectShader(coopActor.get(), revertFX, 5.0f);
					}

					// Reset levitation state flags.
					isTogglingLevitationState = false;
					isTogglingLevitationStateTaskRunning = false;
				}
			}
		}

		// Let Enderal's revert spell script handle everything for Theriantrophist transformations.
		if (ALYSLC::EnderalCompat::g_installed && 
			isPlayer1 && 
			Util::IsWerewolf(coopActor.get()))
		{
			bool succ = false;
			if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
			{
				auto revertSpell = dataHandler->LookupForm<RE::SpellItem>
				(
					0x2E750, "Enderal - Forgotten Stories.esm"
				); 
				if (revertSpell)
				{
					auto instantCaster = coopActor->GetMagicCaster
					(
						RE::MagicSystem::CastingSource::kInstant
					); 
					if (instantCaster)
					{
						instantCaster->CastSpellImmediate
						(
							revertSpell, false, coopActor.get(), 1.0f, false, 0.0f, coopActor.get()
						);

						succ = true;
					}
				}
			}

			return succ;
		}
		else
		{
			// Revert race to saved one using a console command.
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (!script)
			{
				return false;
			}
			
			// The transformation reversion will remove at least half of the player's health,
			// so ensure their health is full before transforming back to prevent the player
			// from instantly dying/entering a downed state.
			DBG("{}: Restore health.", coopActor->GetName());
			Util::RestoreAVToMaxValue(coopActor.get(), RE::ActorValue::kHealth);
			if (auto effectList = coopActor->GetActiveEffectList(); effectList)
			{
				for (auto effect : *effectList)
				{
					bool ignore = 
					(
						(!effect || !effect->effect || !effect->effect->baseEffect) ||
						(
							!effect->effect->baseEffect->HasArchetype
							(
								RE::EffectSetting::Archetype::kVampireLord
							) &&
							!effect->effect->baseEffect->HasArchetype
							(
								RE::EffectSetting::Archetype::kWerewolf
							)
						)
					);
					if (ignore)
					{
						continue;
					}

					effect->Dispel(true);
				}
			}
			
			bool wasWerewolf = Util::IsWerewolf(coopActor.get());
			bool wasVampireLord = Util::IsVampireLord(coopActor.get());
			if (isPlayer1 && wasWerewolf)
			{
				// Doesn't auto-unequip the werewolf FX armor for P1 
				// when setting to the original race, so do it here.
				auto werewolfFXArmor = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>
				(
					"ArmorFXWerewolfTransitionSkin"
				);
				coopActor->RemoveItem
				(
					werewolfFXArmor, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr
				);
			}
			else if (!isPlayer1 && wasVampireLord)
			{
				// Unequip any vampire spells.
				em->UnequipHandForms(glob.bothHandsEquipSlot);
			}
			
			script->SetCommand(fmt::format("setrace {}", originalRace->formEditorID));
			script->CompileAndRun(coopActor.get());
			Util::SetActorRace(coopActor.get(), originalRace);
			// Cleanup.
			delete script;
			// Clear out pre-transformation race, since we've already reverted to it.
			preTransformationRace = nullptr;
			
			if (!isPlayer1)
			{
				// Rescale health, magicka, and stamina to stored values.
				float firstLevel = 1.0f;
				auto iter = glob.serializablePlayerData.find(coopActor->formID); 
				if (iter != glob.serializablePlayerData.end())
				{
					firstLevel = iter->second->firstSavedLevel;
				}

				GlobalCoopData::RescaleHMS(coopActor.get(), firstLevel);
				// Re-equip all gear without resetting inventory.
				em->ReEquipAll(false, false);
			}

			return true;
		}
	
		// Failed.
		return false;
	}

	void CoopPlayer::SendAnimEventSynced(RE::BSFixedString a_animEvent)
	{
		// Queue up a task to play the requested animation event
		// and wait until it is done executing before returning.

		Util::AddSyncedTask
		(
			[this, &a_animEvent]() { coopActor->NotifyAnimationGraph(a_animEvent); }
		);
	}

	void CoopPlayer::SetAsDowned()
	{
		// Reset downed state, 
		// pause managers,
		// ensure the player's essential flag is set to prevent death while downed, 
		// prevent health regen and set health to 0, 
		// ragdoll and paralyze the player to keep them from getting up while downed,
		// and set initial revive data.

		DBG
		(
			"{}. Is ragdolled: {} (knock state {}), "
			"is in killmove: {}, is dead: {}, is essential: {}, health: {}.",
			coopActor->GetName(),
			coopActor->IsInRagdollState(),
			coopActor->GetKnockState(),
			coopActor->IsInKillMove(),
			coopActor->IsDead(),
			coopActor->boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential),
			coopActor->GetActorValue(RE::ActorValue::kHealth)
		);

		isDowned = true;
		isRevived = false;
		isGettingUpAfterRevive = false;
		secsDowned = 0.0f;
		revivedHealth = 0.0f;
		lastDownedTP = SteadyClock::now();

		auto resAV = coopActor->GetActorValue(RE::ActorValue::kRestoration);
		// Health post-revive scales with the player's restoration skill level.
		// Half-to-full health from levels 15-100.
		float resAVMult = std::lerp(0.5f, 1.0f, std::clamp((resAV - 15.0f) / (85.0f), 0.0f, 1.0f));
		fullReviveHealth = 
		(
			resAVMult * Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kHealth)
		);

		// Make sure the player's managers are paused.
		RequestStateChange(ManagerState::kPaused);
		
		// Ensure that the player will stay downed 
		// and no death events trigger during the downed state countdown.
		// Certain attacks, such as spider venom, that occur while co-op data 
		// is copied onto P1 seem to reset the essential flag.
		SetEssentialForReviveSystem();

		// Set health/health regen to 0 to prevent player 
		// from getting up prematurely when being revived.
		coopActor->SetBaseActorValue(RE::ActorValue::kHealRateMult, 0.0f);
		coopActor->RestoreActorValue
		(
			RE::ACTOR_VALUE_MODIFIER::kDamage, 
			RE::ActorValue::kHealth, 
			-coopActor->GetActorValue(RE::ActorValue::kHealth)
		);

		// Remove all damaging active effects that could down the player again
		// soon after they fully get up.
		// Also ragdoll the player if they are not ragdolled already.
		if (auto effectList = coopActor->GetActiveEffectList(); effectList)
		{
			for (auto effect : *effectList)
			{
				if (!effect || !effect->IsCausingHealthDamage())
				{
					continue;
				}

				effect->Dispel(true);
			}
		}

		// Put in an alive ragdoll state.
		Util::NativeFunctions::ClearKeepOffsetFromActor(coopActor.get());
		Util::PushActorAway
		(
			coopActor.get(), coopActor->data.location, -1.0f, true
		);
	}

	void CoopPlayer::SetCoopPlayerFlags()
	{
		// Set actor flags to prepare this player for co-op.
		
		// Set essential flags and bleedout override if using the revive system.
		auto actorBase = coopActor->GetActorBase();
		if (actorBase)
		{
			if (Settings::bUseReviveSystem)
			{
				if (!isPlayer1 || Settings::bCanRevivePlayer1)
				{
					DBG("{} is now set as essential.", coopActor->GetName());
					// Not P1 or can revive P1, so set as essential.
					Util::ChangeEssentialStatus(coopActor.get(), true, !glob.p1IsEssential);
				}
				else
				{
					DBG
					(
						"Cannot revive P1. P1 essential designation: {}.", glob.p1IsEssential
					);
					// Is P1 and cannot revive P1, so defer to previous essential designation.
					Util::ChangeEssentialStatus
					(
						coopActor.get(), glob.p1IsEssential, !glob.p1IsEssential
					);
				}
			}
			else
			{
				if (isPlayer1)
				{
					DBG
					(
						"Cannot revive players. P1 essential designation: {}.", glob.p1IsEssential
					);
					// Defer to previous essential designation for P1.
					Util::ChangeEssentialStatus
					(
						coopActor.get(), glob.p1IsEssential, glob.p1IsEssential
					);
				}
				else
				{
					DBG
					(
						"Cannot revive players {} unset as essential.", coopActor->GetName()
					);
					// Player revive disabled, so clear essential flags.
					Util::ChangeEssentialStatus(coopActor.get(), false, true);
				}
			}
		}
		
		// Make sure the player is not paralyzed.
		coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
		// Extra flags to modify for companion players.
		if (!isPlayer1)
		{
			// Set as teammate to prevent friendly fire and pickpocketing.
			coopActor->boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
			// Allow rotation.
			coopActor->boolBits.set(RE::Actor::BOOL_BITS::kShouldRotateToTrack);
			// Make sure the companion player is tagged as persistent.
			coopActor->formFlags |= RE::Actor::RecordFlags::kPersistent;
			// Ensure co-op companion players do not start combat with P1.
			coopActor->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
			// Prevent P1 from talking to this companion player.
			coopActor->AllowPCDialogue(false);
			// No talking while downed.
			coopActor->AllowBleedoutDialogue(false);
		}
	}
	
	void CoopPlayer::SetDefaultRacialAppearance(bool a_setFemale, bool a_setOppositeGenderAnims)
	{
		// Import default racial headparts, update gender, animations, skin tone,
		// and refresh the player actor's 3D model when done.
		// Does not update appearance preset or change the player's race.

		if (!coopActor || 
			!coopActor->race || 
			!coopActor->race->faceRelatedData ||
			!coopActor->GetActorBase() || 
			!coopActor->GetActorBase()->race)
		{
			return;
		}

		DBG
		(
			"{}: set female: {}, set opposite gender animations: {}, current race: {}",
			coopActor->GetName(), a_setFemale, a_setOppositeGenderAnims, coopActor->race->GetName()
		);
		auto actorBase = coopActor->GetActorBase();
		// Remove all headparts from the player.
		// The game will then supply the defaults.
		Util::RemoveAllHeadParts(coopActor.get());
		// Switch gender before applying new head parts.
		Util::SetActorGender(coopActor.get(), a_setFemale, a_setOppositeGenderAnims);
		// Import the default race-given headparts after.
		Util::ImportDefaultRacialHeadParts(coopActor->race, a_setFemale, actorBase);
	}

	void CoopPlayer::SetEssentialForReviveSystem()
	{
		// Set essential if using the revive system, 
		// since players do not 'die' right away and instead enter a suspended animation
		// 'downed' state where they can be revived.

		// If global data or players are not initialized or not in co-op or all players are dead,
		// we've got nothing to do.
		if (!glob.globalDataInit || 
			!glob.allPlayersInit || 
			!glob.coopSessionActive || 
			glob.livingPlayers == 0)
		{
			return;
		}

		// Set essential if:
		// 1. The revive system is enabled -AND-
		// 2. Saving is allowed -AND-
		// 3. The player is not in a killmove -AND-
		// 4. Either the actor base or actor essential flags are not set -AND-
		// 5. The player is not P1, or P1 revival is enabled 
		// and P1 was not designated as essential.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		bool canSetAsEssential = 
		(
			(
				Settings::bUseReviveSystem && 
				!coopActor->IsInKillMove() && 
				coopActor->GetActorBase()
			) &&
			(!p1 || p1->byCharGenFlag != RE::PlayerCharacter::ByCharGenFlag::kDisableSaving) &&
			(
				(
					!isPlayer1 && 
					coopActor->GetActorBase()->actorData.actorBaseFlags.none
					(
						RE::ACTOR_BASE_DATA::Flag::kEssential
					)
				) || 
				coopActor->boolFlags.none(RE::Actor::BOOL_FLAGS::kEssential)
			) &&
			(
				(!isPlayer1) || 
				(
					(Settings::bCanRevivePlayer1) && 
					((!glob.p1IsEssential) || (isDowned && !coopActor->IsDead()))
				)
			)
		);

		if (canSetAsEssential)
		{
			// Set both actor base and actor flags.
			Util::ChangeEssentialStatus(coopActor.get(), !isPlayer1 || !glob.p1IsEssential);
		}
	}

	bool CoopPlayer::ShouldTeleportToP1(bool&& a_selfPauseCheck)
	{
		// Check if a companion player should teleport to P1
		// if a fader menu has opened after a player activates a teleport door/refr.

		if (isPlayer1)
		{
			return false;
		}

		// FaderMenu must be open.
		auto ui = RE::UI::GetSingleton(); 
		if (!ui || !ui->IsMenuOpen(RE::FaderMenu::MENU_NAME)) 
		{
			return false;
		}

		// Check menu opening requests for a request 
		// with an associated form that is a teleport door/refr.
		for (auto i = 0; i < glob.moarm->menuOpeningActionRequests.size(); ++i)
		{
			const auto& list = glob.moarm->menuOpeningActionRequests[i];
			const auto& reqP = glob.coopPlayers[i];
			for (auto& req : list)
			{
				float secsSinceReq = Util::GetElapsedSeconds(req.timestamp);
				// Must be a recent request with an associated refr.
				if (secsSinceReq >= 2.0f || !Util::HandleIsValid(req.assocRefrHandle))
				{
					continue;
				}

				// Must have extra teleport data that could've triggered the FaderMenu.
				auto objRefr = req.assocRefrHandle.get().get(); 
				auto exTeleport = objRefr->extraList.GetByType<RE::ExtraTeleport>();
				if (!exTeleport)
				{
					continue;
				}

				if (a_selfPauseCheck)
				{
					// Run by self-pause check, so this player's managers will pause
					// as long as there was a menu-opening activation request with a teleport door.
					return true;
				}
				else
				{
					// Run by self-resume check, so this player's managers will continue 
					// to pause while it attempts to teleport to P1 
					// once P1 is close enough to the teleport endpoint.
					// Get teleport endpoint location.
					auto teleportData = exTeleport->teleportData; 
					if (!teleportData)
					{
						continue;
					}

					// NOTE: 
					// Might change the close-enough radius. Needs testing.
					return 
					(
						glob.player1Actor->data.location.GetDistance(teleportData->position) <= 
						100.0f
					);
				}
			}
		}

		return false;
	}

	void CoopPlayer::SyncPlayerFactions()
	{
		// All companion players should have the same factions as P1.
		// Add to a couple of co-op related faction as well.
		
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto actorBase = coopActor->GetActorBase();
		if (!p1 || !actorBase)
		{
			return;
		}

		// Only add if not already a member.
		for (const auto coopFaction : glob.coopPlayerFactions)
		{
			if (!coopFaction || coopActor->IsInFaction(coopFaction))
			{
				continue;
			}

			coopActor->AddToFaction(coopFaction, 0);
			DBG
			(
				"{} added to co-op faction {} (0x{:X}): {}.",
				coopActor->GetName(),
				coopFaction->GetName(),
				coopFaction->formID,
				coopActor->IsInFaction(coopFaction)
			);
		}

		if (!isPlayer1)
		{
			p1->VisitFactions
			(
				[this](RE::TESFaction* a_faction, int8_t a_rank) 
				{
					if (!coopActor->IsInFaction(a_faction))
					{
						coopActor->AddToFaction(a_faction, a_rank);
					}

					DBG
					(
						"{} now is in faction {} (0x{:X}): {}.",
						coopActor->GetName(),
						a_faction->GetName(),
						a_faction->formID,
						coopActor->IsInFaction(a_faction)
					);

					return false;
				}
			);
		}
		

		coopActor->VisitFactions
		(
			[this](RE::TESFaction* a_faction, int8_t a_rank) 
			{
				DBG
				(
					"{} is in faction {} (0x{:X}): {}.",
					coopActor->GetName(),
					a_faction->GetName(),
					a_faction->formID,
					coopActor->IsInFaction(a_faction)
				);

				/*if ((a_faction->formID & 0x00FFFFFF) == 0x016EB3)
				{
					coopActor->RemoveFromFaction(a_faction);
					DBG("Removing from faction {}: {}.",
						Util::GetEditorID(a_faction), !coopActor->IsInFaction(a_faction));
				}*/

				return false;
			}
		);
		// IDK, but since we updated the player's factions, might as well refresh reactions too.
		if (auto procLists = RE::ProcessLists::GetSingleton(); procLists)
		{
			procLists->ClearCachedFactionFightReactions();
		}
	}

	void CoopPlayer::UpdateWhenDowned()
	{
		// Downed state changes are reflected in the crosshair text entry for the downed player.
		// Exit conditions:
		// - Player is revived and no longer in a downed state.
		// - Player is not revived in time (all players are killed).
		// - Co-op session ends while the player is downed:
		//		- Players are dismissed.
		//		- P1 is killed.
		//		- Another save is loaded.

		if (glob.loadingASave)
		{
			// All data will be re-initialized once the save loads, so nothing to clean up here.
			DBG
			(
				"Stopped downed countdown for {}. Game is loading a save file. Skipping cleanup.",
				coopActor->GetName()
			);
			return;
		}

		// A loading screen opened while downed.
		bool loadingMenuOpened = false;
		// Check if the LoadingMenu has opened.
		const auto ui = RE::UI::GetSingleton();
		if (ui)
		{
			loadingMenuOpened = ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
		}

		// Interval is over once the player has been downed for longer than the revive window.
		// If true, the revive window is over and all players die.
		bool reviveIntervalOver = secsDowned > Settings::fSecsUntilDownedDeath;
		// Stop counting down if the co-op session ends, the game is loading a save,
		// the LoadingMenu opens, this player is revived, the revive window is over, 
		// or this player is dead.
		bool stopCountingDown = 
		(
			!glob.coopSessionActive || 
			glob.loadingASave ||
			loadingMenuOpened || 
			isRevived || 
			reviveIntervalOver || 
			coopActor->IsDead()
		);
		// Last time the player's downed state was checked.
		RE::BSFixedString reviveText = ""sv;
		if (stopCountingDown)
		{
			// Remove all new damaging active effects that could down the player again
			// before the player fully gets up.
			if (auto effectList = coopActor->GetActiveEffectList(); effectList)
			{
				for (auto effect : *effectList)
				{
					if (!effect || !effect->IsCausingHealthDamage())
					{
						continue;
					}

					effect->Dispel(true);
				}
			}

			// Post-revive success/fail tasks.
			// 
			// If the co-op session is still active and the player has not died,
			// the player could be fully revived, 
			// getting up after being revived,
			// or the revive window could have passed.
			if (glob.coopSessionActive && !coopActor->IsDead())
			{
				if (reviveIntervalOver)
				{
					// Failure! The player's revive window has closed.
					// 
					// One last crosshair text update with final revive statistics.
					reviveText = fmt::format
					(
						"P{}: <font color=\"#FF0000\">[Life]: 0.0%</font>, "
						"<font color=\"#00FF00\">[Revive]: {:.1f}%</font>",
						playerID + 1,
						100.0f * min(1.0f, revivedHealth / fullReviveHealth)
					);
					tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert, 
						reviveText,
						{ },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					tm->UpdateCrosshairMessage();

					DBG
					(
						"{} was NOT revived. About to teardown co-op session.", 
						coopActor->GetName()
					);

					// Uh-oh!
					auto handle = coopActor->GetHandle();
					glob.taskRunner->AddTask
					(
						"GLOB Runner",
						__FUNCTION__,
						[handle](){ GlobalCoopData::YouDiedTask(handle); }
					);

					isDowned = false;
					isRevived = false;
					isGettingUpAfterRevive = false;
				}
				else if (isRevived && !isGettingUpAfterRevive)
				{
					// Yay! Successful revive.
					//
					// Now getting up after revive.
					isGettingUpAfterRevive = true;
					mm->isGettingUp = true;
					lastGetupAfterReviveTP = SteadyClock::now();

					// One last crosshair text update with fully revived message.
					reviveText = fmt::format
					(
						"P{}: <font color=\"#FF0000\">[Life]: {:.1f}%</font>, "
						"<font color=\"#00FF00\">[Revive]: 100.0%</font>",
						playerID + 1,
						100.0f * 
						max
						(
							0.0f, 
							(1.0f - secsDowned / max(1.0f, Settings::fSecsUntilDownedDeath))
						)
					);
					tm->SetCrosshairMessageRequest
					(
						CrosshairMessageType::kReviveAlert, 
						reviveText,
						{ },
						Settings::fSecsBetweenDiffCrosshairMsgs
					);
					tm->UpdateCrosshairMessage();

					DBG
					(
						"{} was revived. Toggle god mode until fully up. "
						"Health to restore: {}",
						coopActor->GetName(),
						max
						(
							0.0f, 
							revivedHealth - coopActor->GetActorValue(RE::ActorValue::kHealth)
						)
					);
					
					// Invulnerable while getting up after revive.
					GlobalCoopData::ToggleGodModeForPlayer(playerID, true, false);
					// Indicates the player is temporarily invulnerable.
					Util::StartEffectShader(coopActor.get(), glob.ghostFXShader);

					// Set full revive health, un-paralyze, and set to alive.
					pam->ModifyAV
					(
						RE::ActorValue::kHealth,
						max
						(
							0.0f, 
							revivedHealth - coopActor->GetActorValue(RE::ActorValue::kHealth)
						)
					);
					coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					coopActor->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
				}
				else if (isGettingUpAfterRevive)
				{
					// Wait until the player is standing up before restarting managers.
					// Failsafe interval of 5 seconds.
					float secsSinceGetUpStart = Util::GetElapsedSeconds(lastGetupAfterReviveTP);
					const auto& knockState = coopActor->actorState1.knockState;
					if (secsSinceGetUpStart < 5.0f && knockState != RE::KNOCK_STATE_ENUM::kNormal)
					{
						// Curtail momentum to stop the player while they get up.
						mm->shouldCurtailMomentum = true;
						mm->ClearKeepOffsetFromActor();
						mm->SetForceDontMove(true);

						// Force the player to getup if not started already.
						if (knockState != RE::KNOCK_STATE_ENUM::kGetUp)
						{
							coopActor->PotentiallyFixRagdollState();
							coopActor->NotifyAnimationGraph("GetUpBegin");
						}
						else
						{
							// Nothing to do but wait now.
							return;
						}
					}
					else
					{
						// Make sure the player can move before resuming managers.
						mm->SetForceDontMove(false);
						// Reset downed time and health.
						revivedHealth = secsDowned = 0.0f;
						// No longer downed once the player has gotten up.
						isDowned = false;
						// Make sure the player is set as revived (should be true).
						isRevived = true;
						// Player has gotten up.
						isGettingUpAfterRevive = false;

						// Toggle off god mode and remove god mode indicator shader.
						GlobalCoopData::ToggleGodModeForPlayer(playerID, false, false);
						Util::StopEffectShader(coopActor.get(), glob.ghostFXShader);

						// IMPORTANT:
						// Need to re-equip the cached hand forms for P1
						// because the game will sometimes unequip any equipped hand spells
						// if P1 is downed by a killmove or by gravity.
						// Can also prevents the player (P1 or not) 
						// from executing an unarmed 'phantom' killmove on themselves 
						// after getting up if the game has emptied their hands.
						em->ReEquipHandForms();

						// Restart managers.
						RequestStateChange(ManagerState::kRunning);

						DBG
						(
							"{} was revived and is no longer downed. Success!", 
							coopActor->GetName()
						);
					}
				}
			}
			else
			{
				// If reaching this point, the player was not revived one way or another, 
				// so make sure the co-op session ends.
				DBG
				(
					"{} was not revived: {}. "
					"Revive interval not over: {}, co-op session ended: {}, "
					"loading a save: {}, loading menu opened: {}, dead: {}. "
					"Dismissing all players.",
					coopActor->GetName(),
					!isRevived, 
					!reviveIntervalOver,
					!glob.coopSessionActive,
					glob.loadingASave,
					loadingMenuOpened,
					coopActor->IsDead()
				);

				// Not revived or downed anymore.
				isRevived = isDowned = isGettingUpAfterRevive = false;
				// Reset revive data.
				revivedHealth = secsDowned = 0.0f;
				// Party was wiped. RIP.
				glob.partyWiped = true;
				// End co-op session, but keep the co-op camera active
				// to transition over to the death camera state.
				GlobalCoopData::TearDownCoopSession(true, false);
			}
		}
		else
		{
			// While downed, update downed time.
			secsDowned = Util::GetElapsedSeconds(lastDownedTP);
			// Remove all damaging active effects that could down the player again
			// soon after they fully get up.
			if (auto effectList = coopActor->GetActiveEffectList(); effectList)
			{
				for (auto effect : *effectList)
				{
					if (!effect || !effect->IsCausingHealthDamage())
					{
						continue;
					}

					effect->Dispel(true);
				}
			}
			
			// Also ragdoll and paralyze the player if they are not ragdolled already.
			if ((!isPlayer1 || !glob.p1IsEssential) && !coopActor->IsInRagdollState())
			{
				Util::NativeFunctions::ClearKeepOffsetFromActor(coopActor.get());
				Util::PushActorAway(coopActor.get(), coopActor->data.location, -1.0f, true);
			}
		
			// Set as unconscious when ragdolled 
			// to prevent enemies from aggro-ing this downed player.
			coopActor->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kUnconcious;
			// Draw indicator at all times while the player is downed.
			tm->DrawPlayerIndicator();

			// Update crosshair text to set.
			// Set crosshair text to allow players to see the downed player's
			// percentage of remaining life and revived percentage.
			// - Life percent: 100% * (time spent unrevived / unrevived time until death).
			// - Revive percent: 100% * (revived health / full revive health).
			reviveText = fmt::format
			(
				"P{}: <font color=\"#FF0000\">[Life]: {:.1f}%</font>, <font color=\"#00FF00\">"
				"[Revive]: {:.1f}%</font>",
				playerID + 1, 
				100.0f * 
				max(0.0f, (1.0f - secsDowned / max(1.0f, Settings::fSecsUntilDownedDeath))),
				100.0f * 
				min(1.0f, revivedHealth / fullReviveHealth)
			);
			tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kReviveAlert, 
				reviveText,
				{ },
				Settings::fSecsBetweenDiffCrosshairMsgs
			);
			tm->UpdateCrosshairMessage();
		}
	}

	//=====================
	// [PLAYER TASK FUNCS]:
	//=====================
	// NOTE: 
	// All run in a separate thread asynchronously.

	void CoopPlayer::LockpickingTask(bool a_fullControl)
	{
		// Never run with P1.
		// NOTE: 
		// Menu input manager crashes the game when the Lockpicking menu is opened twice 
		// by the same co-op player.
		// Have yet to figure out a direct fix for this bug, 
		// so running the lockpicking menu code in a task separate
		// from the main input manager task will have to suffice for now.

		auto ui = RE::UI::GetSingleton();
		auto controlMap = RE::ControlMap::GetSingleton();
		if (!ui || !controlMap || isPlayer1)
		{
			return;
		}

		DBG("{}.", coopActor->GetName());

		// Set PIDs, as the MIM would normally.
		if (a_fullControl)
		{
			glob.mim->managerMenuPID = glob.prevMenuPID = playerID;
		}

		// Flags indicating whether either analog stick was moved.
		bool lsWasMoved = false;
		bool rsWasMoved = false;
		// Seconds this loop iteration took.
		float secsIteration = 0.0f;
		// Time in seconds to wait for, allowing this thread to sync with the game's threads.
		float waitTimeSecs = 0.0f;
		SteadyClock::time_point iterationTP = SteadyClock::now();
		// Continue looping for input until the LockpickingMenu closes.
		while (ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME))
		{
			// Make sure this player has control throughout.
			if ((a_fullControl) && (glob.mim->managerMenuPID == -1 || glob.prevMenuPID == -1))
			{
				glob.mim->managerMenuPID = glob.prevMenuPID = playerID;
			}

			// Wait an additional amount of time to sync with the global time delta.
			secsIteration = Util::GetElapsedSeconds(iterationTP);
			iterationTP = SteadyClock::now();
			waitTimeSecs = max(0.0f, (*g_deltaTimeRealTime - secsIteration));

			// Always rotate the pick if given full control.
			if (a_fullControl)
			{
				// Rotate pick with the LS.
				const auto& lsData = glob.cdh->GetAnalogStickState(deviceID, true);
				const auto& lsX = lsData.xComp;
				const auto& lsY = lsData.yComp;
				const auto& lsMag = lsData.normMag;
				// LS was centered if true.
				const bool lsMovedToRest = (lsWasMoved && lsMag == 0.0f);
				if (lsMag > 0.0f || lsMovedToRest)
				{
					RE::BSFixedString eventName = "RotatePick"sv;
					lsWasMoved = lsMag != 0.0f;
					// Create thumbstick event to send.
					auto thumbstickEvent = std::make_unique<RE::InputEvent* const>
					(
						Util::CreateThumbstickEvent(eventName, lsX * lsMag, lsY * lsMag, true)
					);
					// Set pad to indicate that a companion player sent the input, not P1.
					(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
					Util::AddSyncedTask
					(
						[&thumbstickEvent]() { Util::SendInputEvent(thumbstickEvent); }
					);
				}
			}

			// Companion player in Lockpicking Menu also rotates the lock 
			// if two player lockpicking is not enabled,
			// or if there are more than 2 players, 
			// or if sharing control with P1, who is in charge of rotating the pick.
			if (!Settings::bTwoPlayerLockpicking || glob.activePlayers > 2 || !a_fullControl)
			{
				// Rotate lock with the RS.
				const auto& rsData = glob.cdh->GetAnalogStickState(deviceID, false);
				const auto& rsX = rsData.xComp;
				const auto& rsY = rsData.yComp;
				const auto& rsMag = rsData.normMag;
				// RS was centered if true.
				const bool rsMovedToRest = (rsWasMoved && rsMag == 0.0f);
				if (rsMag > 0.0f || rsMovedToRest)
				{
					RE::BSFixedString eventName = "RotateLock"sv;
					rsWasMoved = rsMag != 0.0f;
					// Create thumbstick event to send.
					auto thumbstickEvent = std::make_unique<RE::InputEvent* const>
					(
						Util::CreateThumbstickEvent(eventName, rsX * rsMag, rsY * rsMag, false)
					);
					// Set pad to indicate that a companion player sent the input, not P1.
					(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
					Util::AddSyncedTask
					(
						[&thumbstickEvent]() { Util::SendInputEvent(thumbstickEvent); }
					);
				}
			}

			// Check if the exit menu bind was pressed and close the menu 
			// if the companion player has full control.
			if (a_fullControl)
			{
				XINPUT_STATE buttonState{ };
				ZeroMemory(&buttonState, sizeof(buttonState));
				if (XInputGetState(deviceID, &buttonState) == ERROR_SUCCESS)
				{
					// Get XInput and game mask for the 'Cancel' bind.
					// Default to the 'B' button.
					auto escapeXIMask = XINPUT_GAMEPAD_B;
					uint32_t idCode = GAME_INPUT_CODE_B;
					RE::BSFixedString eventName = "Cancel"sv;
					if (auto userEvents = RE::UserEvents::GetSingleton(); userEvents) 
					{
						// Set id code, event name, and XInputMask.
						eventName = userEvents->cancel;
						idCode = controlMap->GetMappedKey
						(
							eventName, 
							RE::INPUT_DEVICE::kGamepad, 
							RE::ControlMap::InputContextID::kMenuMode
						);
						const auto iter = glob.cdh->GAMEMASK_TO_XIMASK.find(idCode);
						if (iter != glob.cdh->GAMEMASK_TO_XIMASK.end())
						{
							escapeXIMask = iter->second;
						}
					}

					// Button is pressed according to XInput controller state.
					if (buttonState.Gamepad.wButtons & escapeXIMask)
					{
						// Create button event and send through task.
						std::unique_ptr<RE::InputEvent* const> buttonEvent = 
						(
							std::make_unique<RE::InputEvent* const>
							(
								RE::ButtonEvent::Create
								(
									RE::INPUT_DEVICE::kGamepad, eventName, idCode, 1.0f, 0.0f
								)
							)
						);
						// Sent by companion player.
						(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
						Util::AddSyncedTask
						(
							[&buttonEvent]() { Util::SendInputEvent(buttonEvent); }
						);
					}
				}
			}

			// When done, wait to sync with the main thread by waiting one global time delta.
			if (waitTimeSecs > 0.0f)
			{
				std::this_thread::sleep_for
				(
					std::chrono::milliseconds
					(
						static_cast<long long>(max(0.0f, 1000.0f * waitTimeSecs))
					)
				);
			}
		}

		// Reset MIM DID/PID to -1 once the LockpickingMenu closes.
		glob.mim->managerMenuDID = -1;
		glob.mim->managerMenuPID = -1;
	}

	void CoopPlayer::MountTask()
	{
		// Attempt to mount the player's targeted mount asynchronously.
		// Mounting through activation of the refr alone fails often, 
		// mostly due to interference from scene/run-once packages,
		// and the companion player floats around and never attempts to mount
		// when approaching from the mount's right side or when the player's weapon is drawn.
		// Have to forcibly place the actor at the mounting point before activating
		// in order to successfully mount.
		// 
		// Ugly solution until the cause of the activation failure is found.

		auto targetedMountPtr = Util::GetActorPtrFromHandle(targetedMountHandle);
		if (!targetedMountPtr) 
		{
			mm->isMounting = false;
			return;
		}

		// Already mounted, so no need to try to mount again.
		if (coopActor->IsOnMount())
		{
			mm->isMounting = false;
			return;
		}

		// Player wants to mount and is mounting while this task is executing.
		mm->wantsToMount = true;
		mm->isMounting = true;

		// Must fully sheathe weapons first to trigger the mount animation.
		bool drawn = coopActor->IsWeaponDrawn();
		if (drawn)
		{
			Util::AddSyncedTask([this]() { pam->ReadyWeapon(false); });
			const float secsMaxWait = 3.0f;
			float secsWaited = 0.0f;
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			bool isEquipping = false;
			bool isUnequipping = false;
			coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			// Wait until fully sheathed.
			while ((secsWaited < secsMaxWait) && 
				   (coopActor->IsWeaponDrawn() || isEquipping || isUnequipping))
			{
				std::this_thread::sleep_for(0.1s);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
				coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			}

			DBG
			(
				"{} waited {}s before attempting mount. Draw state: {}, (un)equipping: {}, {}",
				coopActor->GetName(), 
				secsWaited,
				!coopActor->actorState2.weaponState,
				isEquipping,
				isUnequipping
			);
		}

		// Mount point is to the left of the mount.
		// Place there at a 100 unit offset.
		auto leftOfMountPt = 
		(
			targetedMountPtr->data.location +
			Util::RotationToDirectionVect
			(
				0.0f, 
				Util::ConvertAngle
				(
					Util::NormalizeAng0To2Pi(targetedMountPtr->data.angle.z - PI / 2.0f)
				)
			) * 100.0f
		);

		// Now we can attempt to mount.
		if (!coopActor->IsOnMount())
		{
			Util::AddSyncedTask
			(
				[this, targetedMountPtr, &leftOfMountPt]() 
				{
					// Move to mount point and activate the mount.
					coopActor->SetGraphVariableBool("bAnimationDriven", true);
					coopActor->SetGraphVariableBool("bIsSynced", true);
					coopActor->SetPosition(leftOfMountPt, true);
					coopActor->Update3DPosition(true);
					DBG("{}: Mount target: {}.", 
						coopActor->GetName(),
						targetedMountPtr ? targetedMountPtr->GetName() : "NONE");
					Util::ActivateRefr
					(
						targetedMountPtr.get(), 
						coopActor.get(), 
						0, 
						targetedMountPtr->GetBaseObject(),
						1, 
						false, 
						false
					);
					if (!isPlayer1) 
					{
						// Not sure if this helps the companion player 
						// mount successfully more often, but keeping it for now.
						coopActor->SetLastRiddenMount(targetedMountHandle);
						coopActor->PutActorOnMountQuick();
					}
				}
			);
		}

		// Give the player half a second to start mounting before resetting animation variables.
		std::this_thread::sleep_for(0.5s);
		coopActor->SetGraphVariableBool("bAnimationDriven", false);
		coopActor->SetGraphVariableBool("bIsSynced", false);
		if (coopActor->IsOnMount())
		{
			// Mount successful.
			currentMountHandle = targetedMountHandle;
		}
		else
		{
			bool isEquipping = false;
			bool isUnequipping = false;
			coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			DBG
			(
				"{} failed mount. Draw state: {}, (un)equipping: {}, {}.",
				coopActor->GetName(), 
				!coopActor->actorState2.weaponState,
				isEquipping,
				isUnequipping
			);
			// Mount failed, so resurrect the mount just in case it glitched out.
			Util::AddSyncedTask
			(
				[targetedMountPtr]() { targetedMountPtr->Resurrect(false, true); }
			);
			currentMountHandle.reset();
			// Flag as no longer wants to mount. Have to try again.
			mm->wantsToMount = false;
		}

		// Draw the player's weapons/magic once fully mounted
		// if they were drawn before attempting the mount.
		if (drawn)
		{
			float maxWaitTimeSecs = 2.0f;
			float secsWaited = 0.0f;
			SteadyClock::time_point startTP = SteadyClock::now();
			while (secsWaited < maxWaitTimeSecs &&
				   coopActor->GetSitSleepState() != RE::SIT_SLEEP_STATE::kRidingMount)
			{
				std::this_thread::sleep_for(0.5s);
				secsWaited = Util::GetElapsedSeconds(startTP);
			}

			Util::AddSyncedTask([this]() { pam->ReadyWeapon(true); });
		}

		// Done attempting mount.
		mm->isMounting = false;
	}

	void CoopPlayer::RefreshPlayerManagersTask()
	{
		// Debug option to signal all player managers to await refresh and then resume afterward, 
		// which will refresh their data.

		DBG("{}: START: Current state: {}.", coopActor->GetName(), currentState);
		if (currentState != ManagerState::kAwaitingRefresh)
		{
			RequestStateChange(ManagerState::kAwaitingRefresh);
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			float secsWaited = 0.0f;
			// Wait until the manager's state changes to awaiting refresh.
			// 1 second failsafe.
			while (secsWaited < 1.0f && currentState != ManagerState::kAwaitingRefresh)
			{
				// Wait one frame at a time.
				std::this_thread::sleep_for
				(
					std::chrono::milliseconds
					(
						static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)
					)
				);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
			}
		}
		
		DBG("{}: MID: Current state: {}.", coopActor->GetName(), currentState);
		if (currentState != ManagerState::kRunning)
		{
			// Change back to running.
			RequestStateChange(ManagerState::kRunning);
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			float secsWaited = 0.0f;
			// Wait until the manager's state changes to running.
			// 3 second failsafe.
			while (secsWaited < 3.0f && currentState != ManagerState::kRunning)
			{
				// Wait one frame at a time.
				std::this_thread::sleep_for
				(
					std::chrono::milliseconds
					(
						static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)
					)
				);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
			}
		}
		
		DBG("{}: END: Current state: {}.", coopActor->GetName(), currentState);
	}

	void CoopPlayer::ResetCompanionPlayerStateTask
	(
		const bool& a_unequipAll, const bool& a_reattachHavok
	)
	{
		// Debug option to reset the companion player, 
		// optionally unequipping all their gear and re-attaching havok + ragdolling them.
		// Acts as a catch-all debug option for whatever bugginess 
		// my bad code may inflict on this player.
		// Godspeed, my friend!
		
		em->skipEquipProcessing = true;
		bool wasTransformed = isTransforming || isTransformed;
		// Ensure the player maintains their original health.
		float healthBefore = coopActor->GetActorValue(RE::ActorValue::kHealth);
		float magickaBefore = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		float staminaBefore = coopActor->GetActorValue(RE::ActorValue::kStamina);
		std::array<std::array<float, 3>, 3> hmsModsBefore{ };
		hmsModsBefore[0] = 
		{
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
			)
		};
		hmsModsBefore[1] = 
		{
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
			)
		};
		hmsModsBefore[2] = 
		{
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
			),
			coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
			)
		};

		DBG
		(
			"Vals before: H: {}, {}, {}, M: {}, {}, {}, S: {}, {}, {}. HMS: {}, {}, {}",
			hmsModsBefore[0][0],
			hmsModsBefore[0][1],
			hmsModsBefore[0][2],
			hmsModsBefore[1][0],
			hmsModsBefore[1][1],
			hmsModsBefore[1][2],
			hmsModsBefore[2][0],
			hmsModsBefore[2][1],
			hmsModsBefore[2][2],
			healthBefore,
			magickaBefore,
			staminaBefore
		);

		// Save desired equip forms to re-equip later.
		auto savedLHForm = em->desiredForms[!EquipIndex::kLeftHand];
		auto savedRHForm = em->desiredForms[!EquipIndex::kRightHand];
		auto savedLHExtraDataList = Util::GetWornRankExtraDataList
		(
			em->inventoryChest.get(),
			savedLHForm ? savedLHForm->As<RE::TESBoundObject>() : nullptr,
			true
		);
		auto savedRHExtraDataList = Util::GetWornRankExtraDataList
		(
			em->inventoryChest.get(),
			savedRHForm ? savedRHForm->As<RE::TESBoundObject>() : nullptr,
			false
		);
		std::unordered_map<RE::MagicItem*, float> effectToElapsedMap{ };
		// Make sure the player is not moving during the reset.
		Util::NativeFunctions::SetDontMove(coopActor.get(), true);

		std::this_thread::sleep_for(0.1s);
		Util::AddSyncedTask
		(
			[
				this, 
				a_unequipAll, 
				wasTransformed, 
				&hmsModsBefore, 
				&healthBefore,
				&magickaBefore, 
				&staminaBefore,
				&effectToElapsedMap
			]() 
			{
				// Reset to default package first.
				pam->SetAndEveluatePackage();

				// Get off mount/stop interacting with furniture.
				coopActor->StopInteractingQuick(true);

				// Clear movement offset and sheathe weapons.
				mm->ClearKeepOffsetFromActor();
				pam->ReadyWeapon(false);

				// Revert any transformation, if needed.
				if (wasTransformed)
				{
					RevertTransformation();
				}

				// Unequip all or just the player's hand forms.
				if (a_unequipAll && !wasTransformed)
				{
					em->UnequipAllAndResetEquipState();
				}
				else
				{
					em->UnequipFormAtIndex(EquipIndex::kLeftHand);
					em->UnequipFormAtIndex(EquipIndex::kRightHand);
				}

				// Resurrect without resetting or attaching 3D.
				// NOTE:
				// This resets the player's health to full.

				auto effectList = coopActor->GetActiveEffectList();
				std::vector<RE::MagicItem*> activeEffectSpells{ };
				if (effectList)
				{
					for (const auto effect : *effectList)
					{
						if (!effect)
						{
							continue;
						}

						// REMOVE when done debugging.
						DBG
						(
							"BEFORE {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
							"elapsed time: {}, duration: {}.",
							fmt::ptr(effectList),
							coopActor->GetName(),
							effect->effect && effect->effect->baseEffect ? 
							effect->effect->baseEffect->GetName() :
							"NONE",
							effect->effect && effect->effect->baseEffect ?
							effect->effect->baseEffect->formID :
							0xDEAD,
							effect->spell ? 
							effect->spell->GetName() :
							"NONE",
							effect->elapsedSeconds,
							effect->duration
						);
						if (effect->spell)
						{
							activeEffectSpells.emplace_back(effect->spell);
							effectToElapsedMap.insert_or_assign
							(
								effect->spell, effect->elapsedSeconds
							);
						}
					}
				}
				
				// Resetting 3D can cause crashes.
				// ReEquipAll() call down below will double apply the gear HMS bonuses
				// because ResetInventory() on its own does not clear out gear enchantments.
				// Resetting here with Resurrect does, though, preventing double applicatoin.
				// BUG (?):
				// Resurrect does health damage when certain objects are equipped? Why?
				// Seems to always deduct 30 HP if The Gauldur Amulet was equipped.
				coopActor->Resurrect(true, false);

				// NOTE:
				// For some reason, clearing the active effects list 
				// causes the active effects to double once all gear is re-equipped, 
				// even without casting all the saved active effects' spells.
				/*
				effectList = coopActor->GetActiveEffectList();
				if (effectList)
				{
					for (const auto effect : *effectList)
					{
						if (!effect)
						{
							continue;
						}

						// REMOVE when done debugging.
						DBG
						(
							"AFTER1 {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
							"elapsed time: {}, duration: {}.",
							fmt::ptr(effectList),
							coopActor->GetName(),
							effect->effect && effect->effect->baseEffect ? 
							effect->effect->baseEffect->GetName() :
							"NONE",
							effect->effect && effect->effect->baseEffect ?
							effect->effect->baseEffect->formID :
							0xDEAD,
							effect->spell ? 
							effect->spell->GetName() :
							"NONE",
							effect->elapsedSeconds,
							effect->duration
						);
					}

					effectList->clear();
				}
				else
				{
					DBG("No active effects list after resurrection.");
				}
				*/
				
				// REMOVE if unnecessary.
				//coopActor->CastPermanentMagic(true, true, true, true);
				// Proc all the previous active effects.	
				/*auto instantCaster = coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kInstant
				);
				if (instantCaster)
				{
					for (const auto spell : activeEffectSpells)
					{
						if (!spell)
						{
							continue;
						}

						DBG
						(
							"Casting spell {} (0x{:X}).", spell->GetName(), spell->formID
						);
						instantCaster->CastSpellImmediate
						(
							spell, true, coopActor.get(), 1.0f, false, 0.0f, nullptr
						);
					}
				}*/

				std::array<std::array<float, 3>, 3> hmsModsAfter{ };
				hmsModsAfter[0] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
					)
				};
				hmsModsAfter[1] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
					)
				};
				hmsModsAfter[2] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
					)
				};
				
				DBG
				(
					"Vals after: H: {}, {}, {}, M: {}, {}, {}, S: {}, {}, {}. HMS: {}, {}, {}",
					hmsModsAfter[0][0],
					hmsModsAfter[0][1],
					hmsModsAfter[0][2],
					hmsModsAfter[1][0],
					hmsModsAfter[1][1],
					hmsModsAfter[1][2],
					hmsModsAfter[2][0],
					hmsModsAfter[2][1],
					hmsModsAfter[2][2],
					coopActor->GetActorValue(RE::ActorValue::kHealth),
					coopActor->GetActorValue(RE::ActorValue::kMagicka),
					coopActor->GetActorValue(RE::ActorValue::kStamina)
				);
			}
		);

		std::this_thread::sleep_for(0.1s);

		// Wait until no longer equipping.
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsMaxWait = 10.0f;
		float secsWaited = 0.0f;
		bool isEquipping = false;
		bool isUnequipping = false;
		coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
		coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
		while ((secsWaited < secsMaxWait) && (isEquipping || isUnequipping))
		{
			std::this_thread::sleep_for(0.1s);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
		}

		std::this_thread::sleep_for(0.1s);

		// Disable the actor and wait until fully disabled.
		Util::AddSyncedTask([this]() { coopActor->Disable(); });

		secsMaxWait = 2.0f;
		secsWaited = 0.0f;
		waitStartTP = SteadyClock::now();
		while (secsWaited < secsMaxWait && !coopActor->IsDisabled())
		{
			std::this_thread::sleep_for(0.1s);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
		}

		std::this_thread::sleep_for(0.1s);

		// Re-enable and wait until fully enabled.
		Util::AddSyncedTask([this]() { coopActor->Enable(false); });

		secsWaited = 0.0f;
		waitStartTP = SteadyClock::now();
		while ((secsWaited < secsMaxWait) && 
			   (coopActor->IsDisabled() || !Util::GetRefr3D(coopActor.get())))
		{
			std::this_thread::sleep_for(0.1s);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
		}

		std::this_thread::sleep_for(0.1s);

		// Detach and re-attach havok, then ragdoll.
		if (a_reattachHavok)
		{
			Util::AddSyncedTask
			(
				[this]() 
				{
					coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					if (auto player3DPtr = Util::GetRefr3D(coopActor.get()); player3DPtr)
					{
						coopActor->DetachHavok(player3DPtr.get());
						coopActor->InitHavok();
						coopActor->MoveHavok(true);
					}
				}
			);

			secsWaited = 0.0f;
			waitStartTP = SteadyClock::now();
			while ((secsWaited < secsMaxWait) && 
				   (
					   !coopActor->currentProcess ||
					   !coopActor->GetCharController() ||
					   !Util::GetRefr3D(coopActor.get())
				   ))
			{
				std::this_thread::sleep_for(0.1s);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
			}

			std::this_thread::sleep_for(0.1s);
			// Do not ragdoll until 3D is fully loaded, char controller is present,
			// and the player's current process is active.
			// May crash otherwise.
			Util::AddSyncedTask
			(
				[this]() 
				{
					Util::PushActorAway(coopActor.get(), coopActor->data.location, -1.0f);
				}
			);
		}

		// Restore previously equipped hand forms.
		if (!a_unequipAll || wasTransformed)
		{
			em->desiredForms[!EquipIndex::kLeftHand] = savedLHForm;
			em->desiredForms[!EquipIndex::kRightHand] = savedRHForm;
			em->desiredExtraDataLists[!EquipIndex::kLeftHand] = savedLHExtraDataList;
			em->desiredExtraDataLists[!EquipIndex::kRightHand] = savedRHExtraDataList;
		}

		Util::AddSyncedTask
		(
			[
				this,
				&healthBefore, 
				&magickaBefore,
				&staminaBefore,
				&hmsModsBefore,
				&effectToElapsedMap
			]() 
			{
				// Reset 'ghost' flag used for I-frames.
				if (auto actorBase = coopActor->GetActorBase(); actorBase)
				{
					actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				}

				// Re-equip everything. Clean inventory slate.
				em->ReEquipAll(false);

				auto effectList = coopActor->GetActiveEffectList();
				if (effectList)
				{
					for (const auto effect : *effectList)
					{
						if (!effect)
						{
							continue;
						}

						// Restore elapsed time.
						if (effect->spell)
						{
							auto iter = effectToElapsedMap.find(effect->spell); 
							if (iter != effectToElapsedMap.end())
							{
								effect->elapsedSeconds = iter->second;
							}
						}
				
						// REMOVE when done debugging.
						DBG
						(
							"AFTER2 {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
							"elapsed time: {}, duration: {}.",
							fmt::ptr(effectList),
							coopActor->GetName(),
							effect->effect && effect->effect->baseEffect ? 
							effect->effect->baseEffect->GetName() :
							"NONE",
							effect->effect && effect->effect->baseEffect ?
							effect->effect->baseEffect->formID :
							0xDEAD,
							effect->spell ? 
							effect->spell->GetName() :
							"NONE",
							effect->elapsedSeconds,
							effect->duration
						);
					}
				}
				
				// IMPORTANT:
				// Resetting while on horseback causes horse warp glitch upon resumption.
				// Re-loads weapon BIPED_OBJECTS, so if the weapon models themselves are missing,
				// this should fix it.
				if (!coopActor->IsOnMount())
				{
					DBG("{}: Reset3D.", coopActor->GetName());
					coopActor->DoReset3D(true);
				}

				// Refresh equip state when done.
				em->RefreshEquipState(RefreshSlots::kAll);
			}
		);
		
		// Ensure health/magicka/stamina is set to previous pre-resurrection value.
		// Issue is, the bonuses from gear do not kick in for a while 
		// after everything is re-equipped, so if we restore the original HMS values too early,
		// the gear bonuses will modify the values and adjust them away from the restored values.
		// Potential workaround for now:
		// Wait. Wait some more and hope.
		std::this_thread::sleep_for(1s);
		Util::AddSyncedTask
		(
			[
				this,
				&healthBefore, 
				&magickaBefore,
				&staminaBefore,
				&hmsModsBefore,
				savedLHForm,
				savedLHExtraDataList, 
				savedRHForm, 
				savedRHExtraDataList
			]() 
			{
				std::array<std::array<float, 3>, 3> hmsModsAfter{ };
				hmsModsAfter[0] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kHealth
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth
					)
				};
				hmsModsAfter[1] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
					)
				};
				hmsModsAfter[2] = 
				{
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kStamina
					),
					coopActor->GetActorValueModifier
					(
						RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina
					)
				};

				/*coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary,
					RE::ActorValue::kHealth,
					hmsModsBefore[0][2] - hmsModsAfter[0][2]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent,
					RE::ActorValue::kHealth,
					hmsModsBefore[0][1] - hmsModsAfter[0][1]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage,
					RE::ActorValue::kHealth,
					hmsModsBefore[0][0] - hmsModsAfter[0][0]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary,
					RE::ActorValue::kMagicka,
					hmsModsBefore[1][2] - hmsModsAfter[1][2]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent,
					RE::ActorValue::kMagicka,
					hmsModsBefore[1][1] - hmsModsAfter[1][1]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage,
					RE::ActorValue::kMagicka,
					hmsModsBefore[1][0] - hmsModsAfter[1][0]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary,
					RE::ActorValue::kStamina,
					hmsModsBefore[2][2] - hmsModsAfter[2][2]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent,
					RE::ActorValue::kStamina,
					hmsModsBefore[2][1] - hmsModsAfter[2][1]
				);
				coopActor->RestoreActorValue
				(
					RE::ACTOR_VALUE_MODIFIER::kDamage,
					RE::ActorValue::kStamina,
					hmsModsBefore[2][0] - hmsModsAfter[2][0]
				);*/

				// Restore the original values when done.
				const float healthAfter = coopActor->GetActorValue(RE::ActorValue::kHealth);
				if (healthAfter != healthBefore)
				{
					DBG("Mod health: {}", healthBefore - healthAfter);
					// Always a positive delta, so no need to undo damage received mult.
					pam->ModifyAV
					(
						RE::ActorValue::kHealth,
						healthBefore - healthAfter,
						healthBefore - healthAfter < 0.0f
					);
				}

				const float magickaAfter = coopActor->GetActorValue(RE::ActorValue::kMagicka);
				if (magickaAfter != magickaBefore)
				{
					DBG("Mod magicka: {}", healthBefore - healthAfter);
					pam->ModifyAV
					(
						RE::ActorValue::kMagicka,
						magickaBefore - magickaAfter,
						magickaBefore - magickaAfter < 0.0f
					);
				}

				const float staminaAfter = coopActor->GetActorValue(RE::ActorValue::kStamina);
				if (staminaAfter != staminaBefore)
				{
					DBG("Mod stamina: {}", healthBefore - healthAfter);
					pam->ModifyAV
					(
						RE::ActorValue::kStamina,
						staminaBefore - staminaAfter,
						staminaBefore - staminaAfter < 0.0f
					);
				}
				
				DBG
				(
					"Diffs: H: {}, {}, {}, M: {}, {}, {}, S: {}, {}, {}. Values after: {}, {}, {}",
					hmsModsBefore[0][0] - hmsModsAfter[0][0],
					hmsModsBefore[0][1] - hmsModsAfter[0][1],
					hmsModsBefore[0][2] - hmsModsAfter[0][2],
					hmsModsBefore[1][0] - hmsModsAfter[1][0],
					hmsModsBefore[1][1] - hmsModsAfter[1][1],
					hmsModsBefore[1][2] - hmsModsAfter[1][2],
					hmsModsBefore[2][0] - hmsModsAfter[2][0],
					hmsModsBefore[2][1] - hmsModsAfter[2][1],
					hmsModsBefore[2][2] - hmsModsAfter[2][2],
					coopActor->GetActorValue(RE::ActorValue::kHealth),
					coopActor->GetActorValue(RE::ActorValue::kMagicka),
					coopActor->GetActorValue(RE::ActorValue::kStamina)
				);

				auto effectList = coopActor->GetActiveEffectList();
				if (effectList)
				{
					for (const auto effect : *effectList)
					{
						if (!effect)
						{
							continue;
						}
						
						// REMOVE when done debugging.
						DBG
						(
							"AFTER {:p}: {} has active effect with base {} (0x{:X}), spell {}, "
							"elapsed time: {}, duration: {}.",
							fmt::ptr(effectList),
							coopActor->GetName(),
							effect->effect && effect->effect->baseEffect ? 
							effect->effect->baseEffect->GetName() :
							"NONE",
							effect->effect && effect->effect->baseEffect ?
							effect->effect->baseEffect->formID :
							0xDEAD,
							effect->spell ? 
							effect->spell->GetName() :
							"NONE",
							effect->elapsedSeconds,
							effect->duration
						);
					}
				}

				// Lastly, make sure the player is visible, just in case their 3D's hidden flag
				// was set previously and not cleared.
				auto player3DPtr = Util::GetRefr3D(coopActor.get()); 
				if (player3DPtr && player3DPtr->flags.all(RE::NiAVObject::Flag::kHidden))
				{
					player3DPtr->flags.reset(RE::NiAVObject::Flag::kHidden);
				}
			}
		);

		// Enable movement again.
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);
		em->skipEquipProcessing = false;
	}

	void CoopPlayer::ShoutTask()
	{
		// If the currently equipped voice form is a shout, 
		// get the shout variation spell to cast 
		// and play shout start and release animations,
		// depending on what shout was equipped.
		// If it is a power, cast instantly.

		pam->isVoiceCasting = true;
		auto voiceForm = em->voiceForm;
		// Spell to cast corresponding to the highest shout variation or power.
		auto voiceSpell = em->voiceSpell;
		auto shout = voiceForm->As<RE::TESShout>();
		// Get voice spell associated with shout/power.
		auto highestVar = em->highestShoutVarIndex;

		// No voice form equipped, 
		// no voice spell equipped,
		// or P1 does not know any words of power for the current shout, 
		// so return.
		if ((!voiceForm) || (!voiceSpell) || (shout && highestVar < 0))
		{
			pam->isVoiceCasting = false;
			return;
		}

		if (!voiceSpell)
		{
			pam->isVoiceCasting = false;
			return;
		}

		// Send shout animations.
		if (shout)
		{
			// Set cooldown.
			pam->secsCurrentShoutCooldown = 
			(
				shout->variations[highestVar].recoveryTime * 
				coopActor->GetActorValue(RE::ActorValue::kShoutRecoveryMult)
			);
			// Release and stop animation events to play, 
			// and delay time between release and stop animations.
			// All approximated, until a better working method is found and implemented. 
			// Ideally, I'd like to implement shouting
			// through the companion player's ranged attack package.
			RE::BSFixedString shoutReleaseAnim = "";
			RE::BSFixedString shoutStopAnim = "";
			std::chrono::duration secsDelayAfterStart = 0.5s;
			if (Util::IsWerewolf(coopActor.get()) || Util::IsVampireLord(coopActor.get()))
			{
				shoutReleaseAnim = "HowlStart";
				shoutStopAnim = "HowlRelease";
				secsDelayAfterStart = 0.0s;
			}
			else
			{
				// Whirlwind sprint and slow time both have special shout release animations
				// and have to be handled separately.
				if (shout->formID == 0x2F7BA)
				{
					// [Whirlwind Sprint]
					// Player will catapult forward until the stop animation plays,
					// so the delay between sending animations directly determines
					// the length of the displacement.
					if (highestVar == 0)
					{
						shoutReleaseAnim = "ShoutSprintMediumStart";
						secsDelayAfterStart = 0.05s;
					}
					else if (highestVar == 1)
					{
						shoutReleaseAnim = "ShoutSprintLongStart";
						secsDelayAfterStart = 0.075s;
					}
					else if (highestVar == 2)
					{
						shoutReleaseAnim = "ShoutSprintLongestStart";
						secsDelayAfterStart = 0.1s;
					}
				}
				else if (shout->formID == 0x48AC9)
				{
					// [Slow Time]
					shoutReleaseAnim = "shoutReleaseSlowTime";
					secsDelayAfterStart = 1s;
				}
				else
				{
					// All other shouts.
					if (coopActor->IsWeaponDrawn())
					{
						shoutReleaseAnim = "CombatReady_BreathExhaleShort";
					}
					else
					{
						shoutReleaseAnim = "MT_BreathExhaleShort";
					}
				}

				// Shout stop anim for every shout.
				shoutStopAnim = "shoutStop";
				// Play shout start anim for every shout.
				SendAnimEventSynced("shoutStart");
			}

			// Shout starts once the release animation plays.
			shoutStartTP = SteadyClock::now();
			SendAnimEventSynced(shoutReleaseAnim);
			// Hold it. HOLD IT.
			std::this_thread::sleep_for(secsDelayAfterStart);

			// Play associated shout sounds.
			// Needs testing.
			// TODO: 
			// Also play voice sound clips for each word in the shout,
			// depending on the player's chosen voice type too.
			const auto audioMgr = RE::BSAudioManager::GetSingleton(); 
			if (audioMgr)
			{
				for (auto i = 0; i <= highestVar; ++i)
				{
					auto varSpell = shout->variations[i].spell; 
					if (!varSpell || varSpell->effects.empty())
					{
						continue;
					}

					auto primaryEffect = varSpell->effects[0]; 
					if (!primaryEffect)
					{
						continue;
					}

					auto baseEffect = primaryEffect->baseEffect; 
					if (!baseEffect)
					{
						continue;
					}

					if (baseEffect->effectSounds.size() <= !RE::MagicSystem::SoundID::kRelease)
					{
						continue;
					}

					auto releaseSound = 
					(
						baseEffect->effectSounds[!RE::MagicSystem::SoundID::kRelease].sound
					);
					if (!releaseSound)
					{
						continue;
					}
					
					RE::BSSoundHandle handle{ };
					bool succ = audioMgr->BuildSoundDataFromDescriptor(handle, releaseSound);
					if (succ)
					{
						auto player3DPtr = Util::GetRefr3D(coopActor.get());
						handle.SetPosition(coopActor->data.location);
						if (player3DPtr)
						{
							handle.SetObjectToFollow(player3DPtr.get());
						}

						handle.Play();
					}
				}	
			}
			
			// Stop the shout.
			// Spell will be cast right after.
			SendAnimEventSynced(shoutStopAnim);
		}

		// Now we can cast the spell for the shout/power.
		bool shouldCastWithP1 = Util::ShouldCastWithP1(voiceSpell);
		Util::AddSyncedTask
		(
			[this, shouldCastWithP1]() 
			{
				pam->CastSpellWithMagicCaster
				(
					EquipIndex::kVoice, true, true, false, shouldCastWithP1
				); 
			}
		);

		// Done shouting/using power.
		pam->isVoiceCasting = false;
	}

	void CoopPlayer::TeleportTask(RE::ActorHandle a_targetHandle)
	{
		// Teleport to another player through a pair of portals.

		auto targetActorPtr = Util::GetActorPtrFromHandle(a_targetHandle); 
		if (!targetActorPtr)
		{
			return;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		auto targetActor = targetActorPtr.get();
		bool targetActorIsOOB = false;
		Util::AddSyncedTask
		(
			[this, targetActor, &targetActorIsOOB]() 
			{
				// Cast downward from the target actor's head height.
				// If nothing is hit, the player is likely under the map and freefalling.
				const float lowerBound = 
				(
					Util::GetVertCollPoints
					(
						Util::GetRefrPosition(targetActor) + 
						RE::NiPoint3(0.0f, 0.0f, targetActor->GetHeight())
					).second
				);
				targetActorIsOOB = 
				(
					lowerBound <= -131072.0f || isnan(lowerBound)
				);
			}
		);

		if (targetActorIsOOB)
		{
			Util::AddSyncedTask
			(
				[this, p1, targetActor]() 
				{
					auto tes = RE::TES::GetSingleton();
					if (!tes)
					{
						ERR
						(
							"ERR: Players are out of bounds and could not get TES singleton. Boooo."
						);
						return;
					}

					if (p1->parentCell)
					{
						DBG("Teleport to P1's parent cell {} (0x{:X}).",
							Util::GetEditorID(p1->parentCell), p1->parentCell->formID);
						p1->CenterOnCell(p1->parentCell);
					}
					else if (auto currentCell = tes->GetCell(targetActor->data.location); 
							 currentCell)
					{
						DBG("Teleport to P1's current cell {} (0x{:X}).",
							Util::GetEditorID(currentCell), currentCell->formID);
						p1->CenterOnCell(currentCell);
					}
					else if (tes->worldSpace && tes->worldSpace->persistentCell)
					{
						DBG
						(
							"Teleport to the current worldspace's persistent cell {} (0x{:X}).",
							Util::GetEditorID(tes->worldSpace->persistentCell), 
							tes->worldSpace->persistentCell->formID
						);
						p1->CenterOnCell(tes->worldSpace->persistentCell);
					}
					else
					{
						ERR
						(
							"ERR: Players are out of bounds "
							"and no valid teleport position was found. Boooo."
						);
					}
				}
			);

			DBG
			(
				"{}: {} is out of bounds. "
				"Moving from ({}, {}, {}) to closest door position: ({}, {}, {}). "
				"Parent cell: {} (0x{:X}).",
				coopActor->GetName(),
				targetActorPtr->GetName(),
				coopActor->data.location.x,
				coopActor->data.location.y,
				coopActor->data.location.z,
				p1->data.location.x,
				p1->data.location.y,
				p1->data.location.z,
				Util::GetEditorID(p1->parentCell),
				p1->parentCell ? p1->parentCell->formID : 0xDEAD
			);
		}

		const auto exitPortalPos = 
		(
			targetActorIsOOB ? p1->data.location : targetActorPtr->data.location
		);

		// Don't move before teleporting.
		Util::NativeFunctions::SetDontMove(coopActor.get(), true);
		// Get up. Teleport will fail otherwise.
		// Also, if the player is paralyzed when they are moved, 
		// they go full Sanic mode and endlessly run on an invisbile treadmill 
		// until ragdolled or reset.
		if (!isDowned && coopActor->IsInRagdollState())
		{
			Util::AddSyncedTask
			(
				[this]() 
				{
					coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					coopActor->NotifyAnimationGraph("GetUpBegin");
					coopActor->PotentiallyFixRagdollState();
				}
			);

			// Wait until the player is getting up or at most 2 seconds.
			/*float secsWaited = 0.0f;
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			while (coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kGetUp && secsWaited < 2.0f)
			{
				DBG("Waiting until getting up. Knock state: {}, waited {}s.", 
					coopActor->GetKnockState(), secsWaited);
				std::this_thread::sleep_for(0.5s);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
			}*/
		}

		// Get portal form.
		auto teleportalActivator = RE::TESForm::LookupByID<RE::TESObjectACTI>(0x7CD55); 
		if (!teleportalActivator || !mm->movementActorPtr)
		{
			// No portal or player/mount, no teleportation, it's that simple, 
			// but ensure the player can move afterward.
			Util::NativeFunctions::SetDontMove(coopActor.get(), false);
			return;
		}

		// MoveTo if both parent cells are not loaded or one actor is in an interior/exterior cell
		// while the other actor is in an exterior/interior cell, 
		// or the teleporting actor is not loaded while the other actor's cell is attached.
		// Otherwise, the players are in the same cell, 
		// so simply set the teleporting actor's position to the exit portal's position.
		bool shouldMoveTo = 
		(
			(!targetActor->parentCell || !coopActor->parentCell) ||
			(
				targetActor->parentCell->IsExteriorCell() && 
				coopActor->parentCell->IsInteriorCell()
			) ||
			(
				targetActor->parentCell->IsInteriorCell() &&
				coopActor->parentCell->IsExteriorCell()
			) ||
			(targetActor->parentCell->IsAttached() && !coopActor->Is3DLoaded())
		);

		// Place down the entry portal and set position to the entry portal.
		Util::AddSyncedTask
		(
			[this, teleportalActivator]() 
			{
				const auto entryPortalPtr = mm->movementActorPtr->PlaceObjectAtMe
				(
					teleportalActivator, false
				);
				if (!entryPortalPtr)
				{
					return;
				}

				mm->movementActorPtr->SetPosition(entryPortalPtr.get()->data.location, true);
			}
		);

		// Let it materialize.
		std::this_thread::sleep_for(0.25s);
		// Then place the exit portal at the target.
		RE::TESObjectREFRPtr exitPortalPtr{ };
		Util::AddSyncedTask
		(
			[this, &exitPortalPtr, &exitPortalPos, targetActor, teleportalActivator]() 
			{
				exitPortalPtr = targetActor->PlaceObjectAtMe(teleportalActivator, false);
				if (exitPortalPtr)
				{
					exitPortalPtr->SetPosition(exitPortalPos);
				}
			}
		);
		std::this_thread::sleep_for(0.25s);

		// If the portal was successfully placed, move the player to the exit portal.
		if (exitPortalPtr)
		{
			if (shouldMoveTo)
			{
				Util::AddSyncedTask
				(
					[this, &exitPortalPtr]() { mm->movementActorPtr->MoveTo(exitPortalPtr.get()); }
				);
			}
			else
			{
				Util::AddSyncedTask
				(
					[this, &exitPortalPtr]() 
					{
						mm->movementActorPtr->SetPosition(exitPortalPtr->data.location, true);
					}
				);
			}
		}
		else
		{
			// Move directly to the target actor otherwise.
			if (shouldMoveTo)
			{
				Util::AddSyncedTask
				(
					[this, targetActor]() 
					{
						mm->movementActorPtr->MoveTo(targetActor); 
					}
				);
			}
			else
			{
				Util::AddSyncedTask
				(
					[this, targetActor]() 
					{
						mm->movementActorPtr->SetPosition(targetActor->data.location, true);
					}
				);
			}

			// Then move to the place where the exit portal failed to spawn
			// if the target actor is in free-fall.
			if (targetActorIsOOB)
			{
				Util::AddSyncedTask
				(
					[this, targetActor, &exitPortalPos]() 
					{
						mm->movementActorPtr->SetPosition(exitPortalPos, true);
					}
				);
			}
		}

		std::this_thread::sleep_for(0.25s);
		// Can move again.
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);
	}

	void CoopPlayer::ToggleVampireLordLevitationTask()
	{
		// Toggle levitation on/off when transformed into a vampire lord
		// and wait until the levitation state changes.
		// 
		// NOTE: 
		// If the levitation state goes out of sync with the FX and spells,
		// especially after performing a killmove,
		// toggle sneak twice to sync everything up again.

		// If P1 or not in Vampire Lord form, return here.
		if (isPlayer1 || !Util::IsVampireLord(coopActor.get()))
		{
			return;
		}

		// Task starts here.
		isTogglingLevitationStateTaskRunning = true;
		auto dataHandler = RE::TESDataHandler::GetSingleton(); 
		if (!dataHandler)
		{
			// Failure, so reset state to allow for toggling again.
			isTogglingLevitationState = false;
			isTogglingLevitationStateTaskRunning = false;
			return;
		}

		bool succ = false;
		// Get leviation state before toggling levitation.
		bool wasLevitating = false;
		coopActor->GetGraphVariableBool("IsLevitating", wasLevitating);
		Util::AddSyncedTask
		(
			[this, dataHandler, &succ]() 
			{
				succ = coopActor->NotifyAnimationGraph("LevitationToggleMoving");
				// Once the animation event request is sent, levitation is being toggled.
				isTogglingLevitationState = true;
			}
		);

		// Levitation toggle animation event was not triggered, so do not continue.
		if (!succ)
		{
			isTogglingLevitationState = false;
			isTogglingLevitationStateTaskRunning = false;
			return;
		}

		// Wait until the levitation state is fully toggled to the opposite of what it was before.
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsWaited = 0.0f;
		bool isLevitating = false;
		coopActor->GetGraphVariableBool("IsLevitating", isLevitating);
		// Bail after 2 seconds if no state change occurs.
		while ((secsWaited < 2.0f) && (isLevitating == wasLevitating))
		{
			// One frame at a time.
			std::this_thread::sleep_for
			(
				std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f))
			);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			coopActor->GetGraphVariableBool("IsLevitating", isLevitating);
		}

		// Done toggling, so this task can be queued again.
		isTogglingLevitationState = false;
		isTogglingLevitationStateTaskRunning = false;
	}
};

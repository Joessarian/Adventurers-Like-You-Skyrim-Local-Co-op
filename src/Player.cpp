#include "Player.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
//#include <PCH.cpp>
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
		hasBeenDismissed =
		isActive =
		isBeingRevived =
		isDowned =
		isInGodMode =
		isPlayer1 =
		isRevivingPlayer =
		isTogglingLevitationState =
		isTogglingLevitationStateTaskRunning = 
		isTransformed =
		isTransforming = 
		selfValid =
		selfWasInvalid =
		shouldTeleportToP1 = false;
		handledControllerInputError =
		isRevived = true;
		// IDs.
		controllerID = -1;
		playerID = -1;
		// Actors.
		coopActor = nullptr;
		currentMountHandle = targetedMountHandle = RE::ActorHandle();
		// Time points.
		expendSprintStaminaTP = SteadyClock::now();
		jumpStartTP = SteadyClock::now();
		lastActivationCheckTP = SteadyClock::now();
		lastAutoGrabTP = SteadyClock::now();
		lastCrosshairUpdateTP = SteadyClock::now();
		lastCyclingTP = SteadyClock::now();
		lastDownedTP = SteadyClock::now();
		lastGetupTP = SteadyClock::now();
		lastHiddenInStealthRadiusTP = SteadyClock::now();
		lastLHCastStartTP = SteadyClock::now();
		lastParaglidingStateChangeTP = SteadyClock::now();
		lastQSSCastStartTP = SteadyClock::now();
		lastReviveCheckTP = SteadyClock::now();
		lastRHCastStartTP = SteadyClock::now();
		lastStaminaCooldownCheckTP = SteadyClock::now();
		lastStealthStateCheckTP = SteadyClock::now();
		crosshairRefrVisibilityCheckTP = SteadyClock::now();
		outOfStaminaTP = SteadyClock::now();
		shoutStartTP = SteadyClock::now();
		crosshairRefrVisibilityLostTP = SteadyClock::now();
		transformationTP = SteadyClock::now();
		// Strings.
		lastAnimEventTag = ""sv;
		// Floats
		fullReviveHealth = revivedHealth = secsDowned = 0.0f;
		// Ints.
		packageFormListStartIndex = 0;
		// Keywords.
		aimTargetKeyword = nullptr;
		// Pre-transformation race.
		preTransformationRace = nullptr;
	}

	CoopPlayer::CoopPlayer(int32_t a_controllerID, RE::Actor* a_coopActor, uint32_t a_packageFormListStartIndex) : 
		Manager(ManagerType::kP), 
		controllerID(a_controllerID), 
		playerID(-1),
		coopActor(a_coopActor),
		packageFormListStartIndex(a_packageFormListStartIndex),
		taskInterface(SKSE::GetTaskInterface())
	{
		InitializeCoopPlayer();
	}

#pragma region MANAGER_FUNCS_IMPL
	void CoopPlayer::MainTask()
	{
		// Nothing for now.
		return;
	}

	void CoopPlayer::PrePauseTask()
	{
		// Pause all sub-managers at the same time.
		em->RequestStateChange(nextState);
		mm->RequestStateChange(nextState);
		pam->RequestStateChange(nextState);
		tm->RequestStateChange(nextState);
	}

	void CoopPlayer::PreStartTask()
	{
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
		// Controller error check.
		XINPUT_STATE tempState;
		ZeroMemory(&tempState, sizeof(XINPUT_STATE));
		if (XInputGetState(controllerID, &tempState) != ERROR_SUCCESS)
		{
			ALYSLC::Log("[P] ShouldSelfPause: {}: controller input error for CID {}. About to pause all managers.",
				coopActor->GetName(), controllerID);
			handledControllerInputError = false;
			return ManagerState::kAwaitingRefresh;
		}

		// Player dismissed or no co-op session active.
		if ((hasBeenDismissed || !glob.coopSessionActive) && currentState != ManagerState::kUninitialized) 
		{
			return ManagerState::kAwaitingRefresh;
		}

		// Downed check.
		if (isDowned)
		{
			return ManagerState::kPaused;
		}

		// Companion player validity check. If invalid, pause and attempt to move to P1 until valid again.
		selfValid = 
		(
			!coopActor->IsDisabled() && coopActor->Is3DLoaded() && 
			coopActor->IsHandleValid() && coopActor->loadedData && 
			coopActor->currentProcess && coopActor->GetCharController() && 
			coopActor->parentCell && coopActor->parentCell->IsAttached()
		);
		if (!selfValid)
		{
			// REMOVE when done debugging.
			ALYSLC::Log("[P] ShouldSelfPause: Disabled: {}, 3d NOT loaded: {}, handle NOT valid: {}, NO loaded data: {}, NO current proc: {}, NO char controller: {}, parent cell NOT attached: {}",
				coopActor->IsDisabled(),
				!coopActor->Is3DLoaded(),
				!coopActor->IsHandleValid(),
				!coopActor->loadedData,
				!coopActor->currentProcess,
				!coopActor->GetCharController(),
				!coopActor->parentCell || !coopActor->parentCell->IsAttached());
			selfWasInvalid = !selfValid;
			return ManagerState::kPaused;
		}

		// Open menus and P1 camera checks.
		auto ui = RE::UI::GetSingleton();
		bool player1WaitForCam = isPlayer1 && glob.cam->IsPaused();
		shouldTeleportToP1 = ShouldTeleportToP1(true);
		// Pause if P1 and cam is disabled, or if the companion player should teleport to P1,
		// or if the game is paused or saving is disabled.
		if (player1WaitForCam || shouldTeleportToP1 || ui->GameIsPaused() || !ui->IsSavingAllowed()) 
		{
			return ManagerState::kPaused;
		}

		// Maintain current state otherwise.
		return currentState;
	}

	const ManagerState CoopPlayer::ShouldSelfResume()
	{
		if (glob.coopSessionActive && currentState != ManagerState::kAwaitingRefresh) 
		{
			// Controller input error check and resolution attempt.
			if (!handledControllerInputError)
			{
				HandleControllerInputError();
				XINPUT_STATE tempState;
				ZeroMemory(&tempState, sizeof(XINPUT_STATE));
				if (XInputGetState(controllerID, &tempState) != ERROR_SUCCESS)
				{
					handledControllerInputError = false;
					return ManagerState::kAwaitingRefresh;
				}
				else
				{
					ALYSLC::Log("[P] ShouldSelfResume: {}'s controller input error has been resolved. CID is now {}.",
						coopActor->GetName(), controllerID);
					handledControllerInputError = true;
				}
			}

			if (auto ui = RE::UI::GetSingleton(); ui)
			{
				shouldTeleportToP1 = ShouldTeleportToP1(false);
				// Player validity check and resolution attempt.		
				selfValid = 
				(
					!shouldTeleportToP1 && !coopActor->IsDisabled() &&
					coopActor->Is3DLoaded() && coopActor->IsHandleValid() && 
					coopActor->loadedData && coopActor->currentProcess && 
					coopActor->GetCharController() && coopActor->parentCell && 
					coopActor->parentCell->IsAttached()
				);
				if (!selfValid)
				{
					secsSinceInvalidPlayerMoved = Util::GetElapsedSeconds(invalidPlayerMovedTP);
					// Attempt to move to P1 every couple of seconds.
					if (secsSinceInvalidPlayerMoved >= Settings::fSecsBetweenInvalidPlayerMoveRequests)
					{
						// Player 1 must also be valid as the moveto target.
						auto p1 = RE::PlayerCharacter::GetSingleton();
						bool player1Valid = p1 && !p1->IsDisabled() && p1->Is3DLoaded() && p1->IsHandleValid() && p1->loadedData && p1->currentProcess && p1->GetCharController() && p1->parentCell;
						if (player1Valid)
						{
							if (coopActor->IsHandleValid() && Util::HandleIsValid(coopActor->GetHandle())) 
							{
								ALYSLC::Log("[P] ShouldSelfResume: Moving {} to p1.", coopActor->GetName());
								taskInterface->AddTask
								(
									[this, p1]() 
									{ 
										// Have to sheathe weapon before teleporting, otherwise the equip state gets bugged.
										pam->ReadyWeapon(false);
										coopActor->MoveTo(p1);
									}
								);
							}

							selfWasInvalid = true;
						}

						invalidPlayerMovedTP = SteadyClock::now();
					}

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
				bool faderMenuOpen = ui->IsMenuOpen(RE::FaderMenu::MENU_NAME);
				// Remain paused if paused temp menus are open, if P1 and cam is disabled, a fader menu is open,
				// or if the game is paused or saving is disabled.
				if (!onlyAlwaysUnpaused || player1WaitForCam || faderMenuOpen || ui->GameIsPaused() || !ui->IsSavingAllowed())
				{
					return ManagerState::kPaused;
				}
			}
			
			// Re-equip hand forms if invalid earlier and not P1.
			if (!isPlayer1 && (selfWasInvalid || !selfValid))
			{
				em->EquipFists();
				selfWasInvalid = false;
			}

			ALYSLC::Log("[P] ShouldSelfResume: {}: Resuming all co-op player manager threads.", coopActor->GetName());
			return ManagerState::kRunning;
		}
		
		// Maintain current state otherwise.
		return currentState;
	}

#pragma endregion

#pragma region PLAYER_INIT_UPDATE
	void CoopPlayer::InitializeCoopPlayer() 
	{
		// NOTE: Controller ID, player actor, and package form start index 
		// already set through constructor or update function at this point.

		ALYSLC::Log("[P] InitializeCoopPlayer: Init player with controller ID: {}, editor id: {}", controllerID, coopActor ? coopActor->GetFormEditorID() : "NONE");
		// Active if the player has a valid controller ID.
		isActive = controllerID != -1;
		if (isActive)
		{
			// Player-specific data.
			handledControllerInputError = true;
			isPlayer1 = coopActor->IsPlayerRef();
			currentMountHandle = targetedMountHandle = RE::ActorHandle();
			hasBeenDismissed =
			isBeingRevived =
			isDowned =
			isInGodMode =
			isRevivingPlayer =
			isTogglingLevitationState =
			isTogglingLevitationStateTaskRunning =
			isTransformed =
			isTransforming =
			selfValid =
			selfWasInvalid = 
			shouldTeleportToP1 = false;
			handledControllerInputError =
			isRevived = true;

			// Time points.
			expendSprintStaminaTP = SteadyClock::now();
			jumpStartTP = SteadyClock::now();
			lastActivationCheckTP = SteadyClock::now();
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
			crosshairRefrVisibilityCheckTP = SteadyClock::now();
			outOfStaminaTP = SteadyClock::now();
			shoutStartTP = SteadyClock::now();
			crosshairRefrVisibilityLostTP = SteadyClock::now();
			transformationTP = SteadyClock::now();

			// Strings.
			lastAnimEventTag = ""sv;
			// Floats.
			fullReviveHealth = coopActor->GetBaseActorValue(RE::ActorValue::kHealth) / 2.0f;
			revivedHealth = secsDowned = 0.0f;
			secsMaxTransformationTime = 150.0f;
			// Aim target keyword
			auto keywordForm = RE::TESForm::LookupByEditorID(fmt::format("__CoopAimTarget{}", controllerID + 1));
			aimTargetKeyword = keywordForm ? keywordForm->As<RE::BGSKeyword>() : nullptr;
			// Pre-transformation race.
			preTransformationRace = nullptr;
			// Set player actor flags.
			SetCoopPlayerFlags();
			// Ensure all players' factions are equivalent to player 1's.
			SyncPlayerFactions();
			// Add serialized perks to the player.
			GlobalCoopData::ImportUnlockedPerks(coopActor.get());

			// Skyrim's Paraglider compat: check if P1 has a paraglider.
			if (isPlayer1 && coopActor.get() && ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled)
			{
				if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
				{
					if (auto paraglider = dataHandler->LookupForm<RE::TESObjectMISC>(0x802, "Paragliding.esp"); paraglider)
					{
						auto invCounts = coopActor->GetInventoryCounts();
						ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider = invCounts.contains(paraglider) && invCounts.at(paraglider) > 0;
						// Add gale spell if not known already (Enderal only).
						if (ALYSLC::EnderalCompat::g_enderalSSEInstalled &&
							ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
							!coopActor->HasSpell(glob.tarhielsGaleSpell))
						{
							coopActor->AddSpell(glob.tarhielsGaleSpell);
						}
					}
				}
			}

			// Create managers and task runner.
			// If managers and task runner have already been constructed, signal them to await the next data refresh.
			if (em && mm && pam && tm && taskRunner) 
			{
				// Prepare listener threads for data refresh when signalled to resume.
				// No need to construct new managers.
				RequestStateChange(ManagerState::kAwaitingRefresh);
			}
			else 
			{
				// Otherwise, this player has not been fully constructed before,
				// and must create new equip, targeting, movement, and player actions managers.
				em = std::make_unique<EquipManager>();
				mm = std::make_unique<MovementManager>();
				pam = std::make_unique<PlayerActionManager>();
				tm = std::make_unique<TargetingManager>();
				taskRunner = std::make_unique<TaskRunner>();
				// Set as unitialized.
				RequestStateChange(ManagerState::kUninitialized);
			}
		}
	}

	void CoopPlayer::UpdateCoopPlayer(int32_t a_controllerID, RE::Actor* a_coopActor, uint32_t a_packageFormListStartIndex)
	{
		ALYSLC::Log("[P] UpdateCoopPlayer: Updating co-op player: {}, CID: {}", a_coopActor ? a_coopActor->GetName() : "NONE", a_controllerID);
		if ((a_controllerID > -1 && a_controllerID < ALYSLC_MAX_PLAYER_COUNT) && a_packageFormListStartIndex != -1)
		{
			controllerID = a_controllerID;
			// Player ID is dependent on the player construction order.
			// Set to -1 until after all players are constructed.
			playerID = -1;
			coopActor = RE::ActorPtr(a_coopActor);
			packageFormListStartIndex = a_packageFormListStartIndex;
			taskInterface = SKSE::GetTaskInterface();
			InitializeCoopPlayer();
		}
		else
		{
			ALYSLC::Log("[P] ERR: UpdateCoopPlayer: {}: controller ID is not between 0 and 3: {}, package start index not found: {}.",
				coopActor ? coopActor->GetName() : "NONE",
				a_controllerID <= -1 || a_controllerID >= 4,
				a_packageFormListStartIndex == -1);
			// Invalid CID/package start index. Set as inactive.
			isActive = false;
		}
	}
#pragma endregion

	void CoopPlayer::CopyNPCAppearanceToPlayer(RE::TESNPC* a_baseToCopy, bool a_setOppositeGenderAnims)
	{
		// Update gender and body-related data. Does not update appearance preset.

		if (!a_baseToCopy || !a_baseToCopy->race || !a_baseToCopy->race->faceRelatedData ||
			!coopActor || !coopActor->race || !coopActor->race->faceRelatedData ||
			!coopActor->GetActorBase() || !coopActor->GetActorBase()->race)
		{
			return;
		}

		ALYSLC::Log("[P] CopyNPCAppearanceToPlayer: copying {}'s appearance to {}, set opposite gender animations: {}, current race, race to set: {}, {}",
			a_baseToCopy ? a_baseToCopy->GetName() : "NONE", 
			coopActor->GetName(), a_setOppositeGenderAnims,
			coopActor->race ? coopActor->race->GetName() : "NONE",
			a_baseToCopy && a_baseToCopy->race ? a_baseToCopy->race->GetName() : "NONE");

		auto actorBase = coopActor->GetActorBase();
		// Set sex first before accessing preset NPCs array.
		const bool setFemale = a_baseToCopy->IsFemale();
		const bool isFemale = actorBase->IsFemale();
		if ((!setFemale && isFemale) || (setFemale && !isFemale))
		{
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kFemale, setFemale);
		}

		// Set opposite gender animations flag if necessary.
		bool usesOppositeGenderAnims = actorBase->UsesOppositeGenderAnims();
		if ((usesOppositeGenderAnims && !a_setOppositeGenderAnims) || (!usesOppositeGenderAnims && a_setOppositeGenderAnims))
		{
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kOppositeGenderAnims, a_setOppositeGenderAnims);
		}

		if (auto faceRelatedData = coopActor->race->faceRelatedData[actorBase->GetSex()]; faceRelatedData)
		{
			if (actorBase->race != a_baseToCopy->race)
			{
				coopActor->SwitchRace(a_baseToCopy->race, false);
				actorBase->race = a_baseToCopy->race;
				actorBase->originalRace = a_baseToCopy->race;
			}

			// Remove all headparts from actor.
			if (actorBase->headParts)
			{
				uint32_t headPartIndex = 0;
				while (actorBase->numHeadParts > 0)
				{
					if (actorBase->headParts[0])
					{
						Util::NativeFunctions::RemoveHeadPart(actorBase, *actorBase->headParts[0]->type);
					}
					else
					{
						// Something went wrong if the head part is invalid, since the head parts count does not match the
						// actual head parts array's size.
						ALYSLC::Log("[P] ERR: CopyNPCAppearanceToPlayer: Num head parts not in sync with actual head parts array. No head part at index {}. Number of current head parts reported: {}. Copying preset head parts over directly: {} parts. Address of invalid head parts list: 0x{:p}.",
							headPartIndex, actorBase->numHeadParts, a_baseToCopy->numHeadParts, fmt::ptr(actorBase->headParts));
						actorBase->numHeadParts = 0;
						// Freeing the invalid array pointer causes the game to hang on save load.
						// Since the array pointer changes once a new head part is added below in ChangeHeadPart(),
						// I'm hoping that the game frees this pointer first, since we can't free it here..
						//RE::free(actorBase->headParts);
						break;
					}

					++headPartIndex;
				}
			}

			// Add new headparts from NPC.
			if (a_baseToCopy->headParts)
			{
				for (auto i = 0; i < a_baseToCopy->numHeadParts; ++i)
				{
					auto& headPart = a_baseToCopy->headParts[i];
					if (headPart)
					{
						actorBase->ChangeHeadPart(headPart);
					}
				}
			}

			if (a_baseToCopy->headRelatedData)
			{
				actorBase->headRelatedData = a_baseToCopy->headRelatedData;
				actorBase->SetFaceTexture(a_baseToCopy->headRelatedData->faceDetails);
				actorBase->SetHairColor(a_baseToCopy->headRelatedData->hairColor);
			}

			actorBase->faceNPC = a_baseToCopy->faceNPC;
			actorBase->faceData = a_baseToCopy->faceData;
			actorBase->farSkin = a_baseToCopy->farSkin;
			actorBase->skin = a_baseToCopy->skin;
			actorBase->tintLayers = a_baseToCopy->tintLayers;
			actorBase->bodyTintColor = a_baseToCopy->bodyTintColor;
			coopActor->UpdateSkinColor();
			actorBase->UpdateNeck(coopActor->GetFaceNodeSkinned());
			coopActor->Update3DModel();
			coopActor->DoReset3D(true);

			ALYSLC::Log("[P] CopyNPCAppearanceToPlayer: Imported {}'s appearance to {}", a_baseToCopy->GetName(), coopActor->GetName());
		}
	}

	void CoopPlayer::DismissPlayer() 
	{
		// Dismiss co-op companion if dead, co-op session ended, or if Summoning Menu is about to open.
		// Player is now inactive.

		RequestStateChange(ManagerState::kAwaitingRefresh);
		// Remove essential flag on dismiss.
		if (auto actorBase = coopActor->GetActorBase(); actorBase) 
		{
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false);
			coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
		}

		// Revert to original race if transformed.
		RevertTransformation();

		// Ensure player is not set to downed.
		isDowned = false;
		isRevived = false;
		hasBeenDismissed = true;

		// Have script run its cleanup.
		onCoopEndReg.SendEvent(coopActor.get(), controllerID);
		ALYSLC::Log("[P] DismissPlayer: Handled dismissal of {}. Script is now completing cleanup.", coopActor->GetName());
	}

	void CoopPlayer::HandleControllerInputError()
	{
		// Re-assign controller ID(s) when disconnected during co-op or when their XInput state(s) cannot be obtained.

		if (currentState == ManagerState::kPaused || currentState == ManagerState::kAwaitingRefresh)
		{
			ALYSLC::Log("[P] ERR: HandleControllerInputError: Failed to get XInput state for CID {}, player {}.",
				controllerID, coopActor->GetName());
			// Get controller "rank" or index in list of active controllers ordered by controller ID.
			uint8_t controllerRank = 0;
			for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i)
			{
				const auto& p = glob.coopPlayers[i];
				if (p->isActive && p->coopActor != coopActor)
				{
					++controllerRank;
				}
			}

			// Update connected controllers list.
			const auto newControllerIDsList = glob.cdh->SetupConnectedCoopControllers();
			// Ensure controller rank does not extend past the end of the new list.
			if (controllerRank < newControllerIDsList.size())
			{
				const auto oldCID = controllerID;
				controllerID = newControllerIDsList[controllerRank];

				// Shift player into new controller ID slot.
				const auto& swappedPlayer = glob.coopPlayers[controllerID];
				// Only want to swap active player's CID (inactive players have a CID of -1).
				if (swappedPlayer->isActive) 
				{
					ALYSLC::Log("[P] HandleControllerInputError: Swapping {} with {}. Swapped player {}'s new CID is now {}.",
						coopActor->GetName(), swappedPlayer->coopActor->GetName(),
						swappedPlayer->coopActor->GetName(), oldCID);
					swappedPlayer->controllerID = oldCID;
				}

				// Have to also recalculate player IDs, which are dependent on ordering in the player list.
				uint8_t currentID = 1;
				for (uint8_t i = 0; i < glob.coopPlayers.size(); ++i)
				{
					const auto& p = glob.coopPlayers[i];
					if (p->isActive)
					{
						ALYSLC::Log("[P] HandleControllerInputError: Recalculating player IDs. {}'s ID was {} and is now {}.",
							p->coopActor->GetName(), p->playerID, p->isPlayer1 ? 0 : currentID);
						if (!p->isPlayer1)
						{
							p->playerID = currentID;
							++currentID;
						}
						else
						{
							// P1 always has a PID of 0.
							p->playerID = 0;
						}
					}
				}

				// Swap pointers after setting new controller/player IDs.
				glob.coopPlayers[oldCID].swap(glob.coopPlayers[controllerID]);
			}
			else
			{
				RE::DebugMessageBox
				(
					fmt::format
					(
						"[ALYSLC] ERROR: Could not get input from controller {}.\nEnding co-op session.\nPlease ensure at least two controllers are plugged in.", controllerID
					).data()
				);
				GlobalCoopData::TeardownCoopSession(true);
			}
		}
	}

	void CoopPlayer::RegisterEvents()
	{
		// Register player for script events.

		if (!glob.player1RefAlias || !coopActor) 
		{
			return;
		}

		if (!isPlayer1)
		{
			if (!onCoopEndReg.Register(coopActor.get()))
			{
				logger::error("[P] ERR: RegisterEvents: Failed to register {} for dismissal event.", coopActor->GetName());
			}
		}

		if (!onCoopEndReg.Register(glob.player1RefAlias))
		{
			logger::error("[P] ERR: RegisterEvents: Failed to register {} for dismissal event.", coopActor->GetName());
		}
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

		bool wasTransformed = isTransforming || isTransformed;
		// Sheathe current weapons first.
		pam->QueueP1ButtonEvent(InputAction::kSheathe, RE::INPUT_DEVICE::kGamepad, ButtonEventPressType::kInstantTrigger);
		// Save health and restore after resurrection.
		float healthBefore = coopActor->GetActorValue(RE::ActorValue::kHealth);
		coopActor->Resurrect(false, false);
		float healthAfter = coopActor->GetActorValue(RE::ActorValue::kHealth);
		if (healthAfter != healthBefore)
		{
			pam->ModifyAV(RE::ActorValue::kHealth, healthBefore - healthAfter);
		}

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
		// Sheathe weapons first.
		pam->ReadyWeapon(false);
		if (wasTransformed)
		{
			RevertTransformation();
		}

		// Re-equip hand forms.
		em->ReEquipHandForms();
		// Reset 'ghost' flag used for I-frames.
		if (auto actorBase = coopActor->GetActorBase(); actorBase)
		{
			actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
		}
	}

	bool CoopPlayer::RevertTransformation()
	{
		// Only revert form if transformed.

		if (!isTransformed)
		{
			return false;
		}

		if (!coopActor || !coopActor.get() || !coopActor->race || !coopActor->GetActorBase())
		{
			return false;
		}
		
		// Revert if the pre-transformation race is different from the player's current race and 
		// the player is not going from a race without a transformation to one with a transformation (ex. Nord to Werewolf).
		auto originalRace = isTransformed && preTransformationRace ? preTransformationRace : coopActor->GetActorBase()->originalRace;
		bool changingToRaceWithTransformation = !Util::IsRaceWithTransformation(coopActor->race) && Util::IsRaceWithTransformation(originalRace);
		if (originalRace && originalRace != coopActor->race && !changingToRaceWithTransformation) 
		{
			// Revert race to saved one.
			const auto scriptFactory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (script)
			{
				// The transformation reversion will remove at least half of the player's health,
				// so ensure their health is full before transforming back to prevent the player
				// from instantly dying/entering a downed state.
				pam->RestoreAVToMaxValue(RE::ActorValue::kHealth);
				// Unequip transformation-specific spells that were equipped when the co-op companion player transformed.
				if (!isPlayer1)
				{
					if (Util::IsWerewolf(coopActor.get()))
					{
						// Unequip base howl shout.
						if (auto howlOfTerror = RE::TESForm::LookupByEditorID("HowlWerewolfFear"); howlOfTerror)
						{
							em->UnequipShout(howlOfTerror);
						}

						// Remove level-dependent Werewolf Claws spell.
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

						if (clawsSpell)
						{
							coopActor->RemoveSpell(clawsSpell);
						}

						// Play transformation shader.
						if (auto revertFX = RE::TESForm::LookupByEditorID<RE::TESEffectShader>("WerewolfTrans02FXS"); revertFX)
						{
							Util::StartEffectShader(coopActor.get(), revertFX, 5.0f);
						}

						// Remove feeding perk.
						coopActor->RemovePerk(RE::TESForm::LookupByEditorID<RE::BGSPerk>("PlayerWerewolfFeed"));
					}
					else if (Util::IsVampireLord(coopActor.get()))
					{
						if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
						{
							// Unequip base bats power.
							if (auto batsPower = dataHandler->LookupForm<RE::SpellItem>(0x38B9, "Dawnguard.esm"); batsPower)
							{
								em->UnequipSpell(batsPower, EquipIndex::kVoice);
							}

							// Remove level-dependent Vampire Claws spell.
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

							if (clawsSpell)
							{
								coopActor->RemoveSpell(clawsSpell);
							}

							// Unequip loincloth by removing it.
							if (auto vampireLoinCloth = dataHandler->LookupForm<RE::TESObjectARMO>(0x11A84, "Dawnguard.esm"); vampireLoinCloth)
							{
								coopActor->RemoveItem(vampireLoinCloth, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
							}

							// Play transformation shader.
							if (auto revertFX = dataHandler->LookupForm<RE::TESEffectShader>(0x15372, "Dawnguard.esm"); revertFX)
							{
								Util::StartEffectShader(coopActor.get(), revertFX, 5.0f);
							}

							// Reset levitation state flags.
							isTogglingLevitationState = false;
							isTogglingLevitationStateTaskRunning = false;
						}
					}
				}

				// Let Enderal's revert script handle everything for Theriantrophist transformations.
				if (ALYSLC::EnderalCompat::g_enderalSSEInstalled && isPlayer1 && Util::IsWerewolf(coopActor.get()))
				{
					bool succ = false;
					if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
					{
						if (auto revertSpell = dataHandler->LookupForm<RE::SpellItem>(0x2E750, "Enderal - Forgotten Stories.esm"); revertSpell)
						{
							if (auto instantCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant); instantCaster)
							{
								instantCaster->CastSpellImmediate(revertSpell, false, coopActor.get(), 1.0f, false, 1.0f, coopActor.get());

								/*script->SetCommand(fmt::format("cast {:X} {} instant", revertSpell->formID, coopActor->formID));
								script->CompileAndRun(coopActor.get());*/
								succ = true;
							}
						}
					}

					// Do not attempt to revert the player's form without running the script, so return here.
					delete script;
					return succ;
				}
				else
				{
					bool wasWerewolf = Util::IsWerewolf(coopActor.get());
					script->SetCommand(fmt::format("setrace {}", originalRace->formEditorID));
					script->CompileAndRun(coopActor.get());

					if (isPlayer1 && wasWerewolf)
					{
						// Doesn't auto-unequip the werewolf FX armor for P1 when setting to the original race, so do it here.
						auto werewolfFXArmor = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("ArmorFXWerewolfTransitionSkin");
						coopActor->RemoveItem(werewolfFXArmor, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}

					// Clear out pre-transformation race, since we've already reverted to it.
					preTransformationRace = nullptr;
					delete script;
					return true;
				}
			}
		}

		// Failed.
		return false;
	}

	void CoopPlayer::SendAnimEventSynced(RE::BSFixedString a_animEvent)
	{
		// Queue up a task to run the animation event and wait until it is done executing.

		Util::AddSyncedTask([this, &a_animEvent]() { coopActor->NotifyAnimationGraph(a_animEvent); });
	}

	void CoopPlayer::SetCoopPlayerFlags()
	{
		// Set essential flags and bleedout override if using revive system.

		auto actorBase = coopActor->GetActorBase();
		if (isPlayer1)
		{
			if (actorBase)
			{
				if (Settings::bUseReviveSystem && Settings::bCanRevivePlayer1)
				{
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, true);
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kBleedoutOverride, true);
					coopActor->boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
					actorBase->actorData.bleedoutOverride = -INT16_MAX;
				}
				else
				{
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false);
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kBleedoutOverride, false);
					coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
					actorBase->actorData.bleedoutOverride = 0.0f;
				}
			}

			// Make sure the player is not paralyzed.
			coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
		}
		else if (!isPlayer1)
		{
			if (actorBase)
			{
				if (Settings::bUseReviveSystem)
				{
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, true);
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kBleedoutOverride, true);
					coopActor->boolFlags.set(RE::Actor::BOOL_FLAGS::kEssential);
					actorBase->actorData.bleedoutOverride = -INT16_MAX;
				}
				else
				{
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kEssential, false);
					Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kBleedoutOverride, false);
					coopActor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kEssential);
					actorBase->actorData.bleedoutOverride = 0.0f;
				}
			}

			// Make sure the player is not paralyzed.
			coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed, RE::Actor::BOOL_BITS::kShouldRotateToTrack);
			// Make sure the companion player is tagged as persistent.
			coopActor->formFlags |= RE::Actor::RecordFlags::kPersistent;
			// Prevent P1 from talking to this companion player.
			coopActor->AllowPCDialogue(false);
			coopActor->AllowBleedoutDialogue(false);
		}

		if (!isPlayer1)
		{
			// Set as teammate to prevent friendly fire and pickpocketing.
			coopActor->boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
			// Ensure co-op companion players do not start combat with P1.
			coopActor->formFlags |= RE::TESObjectREFR::RecordFlags::kIgnoreFriendlyHits;
		}

		// Add to special co-op player faction.
		if (auto coopPlayerFaction = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(0x53AF5, GlobalCoopData::PLUGIN_NAME); coopPlayerFaction && coopActor->IsInFaction(coopPlayerFaction))
		{
			coopActor->AddToFaction(coopPlayerFaction, 0);
		}
	}

	bool CoopPlayer::ShouldTeleportToP1(bool&& a_selfPauseCheck)
	{
		// Check if a co-op companion player should teleport to P1
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

		// Check menu opening requests for a request with an associated form that is a teleport door/refr.
		for (auto i = 0; i < glob.moarm->menuOpeningActionRequests.size(); ++i)
		{
			const auto& list = glob.moarm->menuOpeningActionRequests[i];
			const auto& reqP = glob.coopPlayers[i];
			for (auto& req : list)
			{
				float secsSinceReq = Util::GetElapsedSeconds(req.timestamp);
				// Must be a recent request with an associated refr.
				if (secsSinceReq < 2.0f && Util::HandleIsValid(req.assocRefrHandle))
				{
					// Has extra teleport data that could've triggered the FaderMenu.
					if (auto objRefr = req.assocRefrHandle.get().get(); objRefr->extraList.HasType(RE::ExtraDataType::kTeleport))
					{
						if (a_selfPauseCheck)
						{
							// Run by self-pause check, so this player's managers will pause.
							return true;
						}
						else if (auto exTeleport = objRefr->extraList.GetByType<RE::ExtraTeleport>(); exTeleport)
						{
							// Run by self-resume check, so this player's managers will continue to pause while it attempts to teleport to P1 
							// once P1 is close enough to the teleport endpoint.
							// Get teleport endpoint location.
							if (auto teleportData = exTeleport->teleportData; teleportData)
							{
								// NOTE: Might change the close-enough radius. Needs testing.
								return glob.player1Actor->data.location.GetDistance(teleportData->position) <= 100.0f;
							}
						}
					}
				}
			}
		}

		return false;
	}

	void CoopPlayer::SyncPlayerFactions()
	{
		// All companion players should have the same factions as P1.

		if (isPlayer1)
		{
			return;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto actorBase = coopActor->GetActorBase();
		if (!p1 || !actorBase)
		{
			return;
		}

		actorBase->factions.clear();
		coopActor->AddToFaction(glob.coopCompanionFaction, 0);
		p1->VisitFactions
		(
			[this](RE::TESFaction* a_faction, int8_t a_rank) 
			{
				if (!coopActor->IsInFaction(a_faction))
				{
					coopActor->AddToFaction(a_faction, a_rank);
				}

				return false;
			}
		);
	}

	void CoopPlayer::UnregisterEvents() 
	{
		// Unregister this player for script events.

		if (!glob.player1RefAlias || !coopActor)
		{
			return;
		}

		if (!isPlayer1)
		{
			if (!onCoopEndReg.Unregister(coopActor.get()))
			{
				logger::error("[P] ERR: UnregisterEvents: Could not unregister {} for dismissal event.", coopActor->GetName());
			}
		}

		if (!onCoopEndReg.Unregister(glob.player1RefAlias))
		{
			logger::error("[P] ERR: UnregisterEvents: Could not unregister {} for dismissal event.", coopActor->GetName());
		}
	}


	void CoopPlayer::UpdateGenderAndBody(bool a_setFemale, bool a_setOppositeGenderAnims)
	{
		// Update gender and body-related data. Does not update appearance preset.

		if (!coopActor || !coopActor.get() || !coopActor->race || !coopActor->race->faceRelatedData ||
			!coopActor->GetActorBase() || !coopActor->GetActorBase()->race)
		{
			return;
		}

		ALYSLC::Log("[P] UpdateGenderAndBody: {}: set female: {}, set opposite gender animations: {}, current race: {}",
			coopActor->GetName(), a_setFemale, a_setOppositeGenderAnims, coopActor->race->GetName());

		auto actorBase = coopActor->GetActorBase();
		coopActor->Update3DModel();
		coopActor->DoReset3D(true);
		// Remove all headparts from actor.
		// Game will then supply the defaults.
		if (actorBase->headParts)
		{
			while (actorBase->numHeadParts > 0)
			{
				if (actorBase->headParts[0])
				{
					Util::NativeFunctions::RemoveHeadPart(actorBase, *actorBase->headParts[0]->type);
				}
				else
				{
					// Something went wrong if the head part is invalid, since the head parts count does not match the
					// actual head parts array's size.
					ALYSLC::Log("[P] ERR: UpdateGenderAndBody: Num head parts not in sync with actual head parts array. No head part at index 0. Number of current head parts reported: {}. Address of invalid head parts list: 0x{:p}.",
						actorBase->numHeadParts, fmt::ptr(actorBase->headParts));
					actorBase->numHeadParts = 0;
					// Freeing the invalid array pointer causes the game to hang on save load.
					// Since the array pointer changes once a new head part is added below in ChangeHeadPart(),
					// I'm hoping that the game frees this pointer first, since we can't do it here.
					//RE::free(actorBase->headParts);
					break;
				}
			}
		}

		// Set sex first before accessing face-related data.
		const bool isFemale = actorBase->IsFemale();
		if ((!a_setFemale && isFemale) || (a_setFemale && !isFemale))
		{
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kFemale, a_setFemale);
		}

		// Set opposite gender animations flag if necessary.
		bool usesOppositeGenderAnims = actorBase->UsesOppositeGenderAnims();
		if ((usesOppositeGenderAnims && !a_setOppositeGenderAnims) || (!usesOppositeGenderAnims && a_setOppositeGenderAnims))
		{
			Util::NativeFunctions::SetActorBaseDataFlag(actorBase, RE::ACTOR_BASE_DATA::Flag::kOppositeGenderAnims, a_setOppositeGenderAnims);
		}

		// Add default headparts for the sex choice.
		if (auto faceRelatedData = coopActor->race->faceRelatedData[actorBase->GetSex()]; faceRelatedData && faceRelatedData->headParts)
		{
			const auto headParts = faceRelatedData->headParts;
			for (auto headPart : *headParts)
			{
				if (headPart)
				{
					actorBase->ChangeHeadPart(headPart);
				}
			}
		}

		// Update skin color and player model.
		coopActor->UpdateSkinColor();
		actorBase->UpdateNeck(coopActor->GetFaceNodeSkinned());
		coopActor->Update3DModel();
		coopActor->DoReset3D(true);
	}


// NOTE: All run in a separate thread asynchronously.
#pragma region TASK_FUNCS

	void CoopPlayer::DownedStateCountdownTask()
	{
		// Downed state changes are reflected in the crosshair text entry for the downed player.
		// Exit conditions:
		// - Player is revived and no longer in a downed state.
		// - Player is not revived in time (all players are killed).
		// - Co-op session ends while the player is downed:
		//		- Players are dismissed.
		//		- Player 1 is killed.
		//		- Another save is loaded.

		ALYSLC::Log("[P] DownedStateCountdownTask: {}", coopActor->GetName());
		auto p1 = RE::PlayerCharacter::GetSingleton();
		auto resAV = coopActor->GetActorValue(RE::ActorValue::kRestoration);
		// Health post-revive scales with the player's restoration skill level.
		// Half-to-full health from levels 15-100.
		float resAVMult = std::lerp(0.5f, 1.0f, (resAV - 15.0f) / (85.0f));
		fullReviveHealth = resAVMult * coopActor->GetBaseActorValue(RE::ActorValue::kHealth) + coopActor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);

		// Remove all damaging active effects that could down the player again
		// after they are no longer downed but before this task finishes.
		// Also ragdoll the player if they are not ragdolled already.
		Util::AddSyncedTask([this]() {
			for (auto effect : *coopActor->GetActiveEffectList())
			{
				if (effect && effect->IsCausingHealthDamage())
				{
					effect->Dispel(true);
				}
			}

			if (!coopActor->IsInRagdollState())
			{
				Util::NativeFunctions::ClearKeepOffsetFromActor(coopActor.get());
				Util::PushActorAway(coopActor.get(), coopActor->data.location, -1.0f, true);
			}
		});

		// If true, the revive window is over and all players die.
		bool reviveIntervalOver = false;
		// Should stop the downed state countdown.
		bool stopCountingDown = false;
		// A loading screen opened while downed.
		bool loadingMenuOpened = false;
		// Reset data.
		revivedHealth = secsDowned = 0.0f;
		// When the crosshair text was last updated with downed state info.
		SteadyClock::time_point lastCrosshairTextUpdateTP = SteadyClock::now();
		// Last time the player's downed state was checked.
		SteadyClock::time_point lastDownedUpdateTP = SteadyClock::now();
		std::array<RE::GFxValue, HUDBaseArgs::kTotal> args;
		RE::BSFixedString reviveText = ""sv;
		while (!stopCountingDown)
		{
			Util::AddSyncedTask([this]() {
				// Set as unconscious to prevent enemies from aggro-ing this downed player.
				coopActor->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kUnconcious;
				// Draw indicator at all times while the player is downed.
				tm->DrawPlayerIndicator();
			});

			// Update time the player has been downed for.
			secsDowned += Util::GetElapsedSeconds(lastDownedUpdateTP);
			lastDownedUpdateTP = SteadyClock::now();

			// Update crosshair text every frame.
			float secsSinceCrosshairTextUpdate = Util::GetElapsedSeconds(lastCrosshairTextUpdateTP);
			if (secsSinceCrosshairTextUpdate > *g_deltaTimeRealTime)
			{
				lastCrosshairTextUpdateTP = SteadyClock::now();
				// Set crosshair text to allow players to see the downed player's
				// remaining life time and time to revive.
				// - Life percent: 100% * (time spent unrevived / unrevived time until death).
				// - Revive percent: 100% * (revived health / full revive health).
				reviveText = fmt::format("P{}: <font color=\"#FF0000\">[Life]: {:.1f}%</font>, <font color=\"#00FF00\">[Revive]: {:.1f}%</font>",
					playerID + 1, 100.0f * max(0.0f, (1.0f - secsDowned / max(1.0f, Settings::fSecsUntilDownedDeath))),
					100.0f * min(1.0f, revivedHealth / fullReviveHealth));
				tm->SetCrosshairMessageRequest(CrosshairMessageType::kReviveAlert, reviveText);
				tm->UpdateCrosshairMessage();
			}

			// Interval is over once the player has been downed for longer than the revive window.
			reviveIntervalOver = secsDowned > Settings::fSecsUntilDownedDeath;
			// Check if the LoadingMenu has opened.
			if (const auto ui = RE::UI::GetSingleton(); ui)
			{
				loadingMenuOpened = ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
			}

			// Stop counting down if the co-op session ends, this player is revived, the revive window is over, 
			// the LoadingMenu opens, or this player is dead.
			stopCountingDown = !glob.coopSessionActive || isRevived || reviveIntervalOver || loadingMenuOpened || coopActor->IsDead();
		}

		ALYSLC::Log("[P] DownedStateCountdownTask: Stopped downed countdown for {}. Reason: session ended: {}, is dead: {}, is revived: {}, revive window over: {} ({} : {})",
			coopActor->GetName(), !glob.coopSessionActive, coopActor->IsDead(), isRevived, reviveIntervalOver,
			secsDowned, Settings::fSecsUntilDownedDeath);

		// Remove all new damaging active effects that could down the player again before this task finishes.
		Util::AddSyncedTask([this]() {
			for (auto effect : *coopActor->GetActiveEffectList())
			{
				if (effect && effect->IsCausingHealthDamage())
				{
					effect->Dispel(true);
				}
			}
		});

		// Post-revive success/fail tasks.
		if (glob.coopSessionActive)
		{
			if (reviveIntervalOver)
			{
				// One last crosshair text update with final revive statistics.
				reviveText = fmt::format("P{}: <font color=\"#FF0000\">[Life]: 0.0%</font>, <font color=\"#00FF00\">[Revive]: {:.1f}%</font>",
					playerID + 1, 100.0f * min(1.0f, revivedHealth / fullReviveHealth));;
				tm->SetCrosshairMessageRequest(CrosshairMessageType::kReviveAlert, reviveText);
				tm->UpdateCrosshairMessage();

				ALYSLC::Log("[P] DownedStateCountdownTask: {} was NOT revived. About to teardown co-op session.", coopActor->GetName());

				// Uh-oh!
				Util::AddSyncedTask([this]() { GlobalCoopData::YouDied(coopActor.get()); });
				return;
			}
			else if (isRevived && !coopActor->IsDead())
			{
				// Yay! Successful revive.
				// One last crosshair text update with fully revived message.
				reviveText = fmt::format("P{}: <font color=\"#FF0000\">[Life]: {:.1f}%</font>, <font color=\"#00FF00\">[Revive]: 100.0%</font>",
					playerID + 1, 100.0f * max(0.0f, (1.0f - secsDowned / max(1.0f, Settings::fSecsUntilDownedDeath))));
				tm->SetCrosshairMessageRequest(CrosshairMessageType::kReviveAlert, reviveText);
				tm->UpdateCrosshairMessage();

				ALYSLC::Log("[P] DownedStateCountdownTask: {} was revived. Toggle god mode until fully up.", coopActor->GetName());

				Util::AddSyncedTask([this]() {
					// Invulnerable while getting up after revive and until weapons are equipped again.
					GlobalCoopData::ToggleGodModeForPlayer(controllerID, true);
					// Indicates the player is temporarily invulnerable.
					Util::StartEffectShader(coopActor.get(), glob.ghostFXShader);

					// Set full revive health, un-paralyze, and set to alive.
					pam->ModifyAV(RE::ActorValue::kHealth, max(0.0f, revivedHealth - coopActor->GetActorValue(RE::ActorValue::kHealth)));
					coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					coopActor->actorState1.lifeState = RE::ACTOR_LIFE_STATE::kAlive;
				});

				// Wait until the player is standing up before restarting managers.
				// Failsafe interval of 5 seconds.
				SteadyClock::time_point getUpStartTP = SteadyClock::now();
				float secsSinceGetUpStart = 0.0f;
				while (coopActor->actorState1.knockState != RE::KNOCK_STATE_ENUM::kNormal && secsSinceGetUpStart < 5.0f && glob.coopSessionActive)
				{
					if (coopActor->actorState1.knockState != RE::KNOCK_STATE_ENUM::kGetUp)
					{
						coopActor->PotentiallyFixRagdollState();
						SendAnimEventSynced("GetUpBegin");
					}

					std::this_thread::sleep_for(std::chrono::seconds(static_cast<long long>(*g_deltaTimeRealTime)));
					secsSinceGetUpStart = Util::GetElapsedSeconds(getUpStartTP);
				}

				if (glob.coopSessionActive) 
				{
					// Curtail momentum to stop the player after resuming.
					mm->shouldCurtailMomentum = true;
					// Restart managers.
					RequestStateChange(ManagerState::kRunning);
					// Reset downed time and health.
					revivedHealth = secsDowned = 0.0f;
					// No longer downed once the player has gotten up.
					isDowned = false;

					// Wait an extra second before toggling off god mode.
					std::this_thread::sleep_for(1s);
					Util::AddSyncedTask([this]() {
						GlobalCoopData::ToggleGodModeForPlayer(controllerID, false);
						Util::StopEffectShader(coopActor.get(), glob.ghostFXShader);
					});

					ALYSLC::Log("[P] DownedStateCountdownTask: {} was revived and is no longer downed. Success!", coopActor->GetName());
				}
				else
				{
					ALYSLC::Log("[P] DownedStateCountdownTask: Co-op session ended while {} was getting up!", coopActor->GetName());
				}

				return;
			}
		}

		// If reaching this point, the player was not revived one way or another, so make sure the co-op session ends.
		ALYSLC::Log("[P] DownedStateCountdownTask: {} was not revived: {}. Revive interval not over: {}, co-op session ended: {}, loading menu opened: {}, dead: {}. Dismissing all players.",
			coopActor->GetName(), !isRevived, !reviveIntervalOver, !glob.coopSessionActive, loadingMenuOpened, coopActor->IsDead());

		// Always reset the paralysis flag.
		coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
		// Not revived or downed anymore.
		isRevived = isDowned = false;
		// Reset revive data.
		revivedHealth = secsDowned = 0.0f;
		// End co-op session.
		GlobalCoopData::TeardownCoopSession(true);
	}

	void CoopPlayer::LockpickingTask()
	{
		// NOTE: Menu input manager crashes the game when the Lockpicking menu is opened twice by the same co-op player.
		// Have yet to figure out a direct fix for this bug, so running the lockpicking menu code in a task separate
		// from the main input manager task will have to suffice for now.

		auto ui = RE::UI::GetSingleton();
		auto controlMap = RE::ControlMap::GetSingleton();
		if (!ui || !controlMap)
		{
			return;
		}

		// Set CIDs, as the MIM wouuld normally.
		glob.mim->managerMenuCID = glob.prevMenuCID = controllerID;
		bool lsWasMoved = false;
		bool rsWasMoved = false;
		float secsIteration = 0.0f;
		float waitTimeSecs = 0.0f;
		SteadyClock::time_point iterationTP = SteadyClock::now();
		// Continue looping for input until the LockpickingMenu closes.
		while (ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME))
		{
			// Make sure this player has control throughout.
			if (glob.mim->managerMenuCID == -1 || glob.prevMenuCID == -1)
			{
				glob.mim->managerMenuCID = glob.prevMenuCID = controllerID;
			}

			// Wait an additional amount of time to sync with the global time delta.
			secsIteration = Util::GetElapsedSeconds(iterationTP);
			iterationTP = SteadyClock::now();
			waitTimeSecs = max(0.0f, (*g_deltaTimeRealTime - secsIteration));

			// Rotate pick with LS.
			const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
			const auto& lsX = lsData.xComp;
			const auto& lsY = lsData.yComp;
			const auto& lsMag = lsData.normMag;
			const bool lsMovedToRest = (lsWasMoved && lsMag == 0.0f);
			if (lsMag > 0.0f || lsMovedToRest)
			{
				RE::BSFixedString eventName = "RotatePick"sv;
				lsWasMoved = lsMag != 0.0f;
				// Create thumbstick event to send.
				auto thumbstickEvent = std::make_unique<RE::InputEvent* const>(Util::CreateThumbstickEvent(eventName, lsX * lsMag, lsY * lsMag, true));
				// Set pad to indicate that the co-op player sent the input, not P1.
				(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
				Util::AddSyncedTask([&thumbstickEvent]() { Util::SendInputEvent(thumbstickEvent); });
			}

			// Co-op player in lockpicking menu also rotates the lock if two player lockpicking is not enabled.
			if (!Settings::bTwoPlayerLockpicking)
			{
				// Rotate lock with RS.
				const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
				const auto& rsX = rsData.xComp;
				const auto& rsY = rsData.yComp;
				const auto& rsMag = rsData.normMag;
				const bool rsMovedToRest = (rsWasMoved && rsMag == 0.0f);
				if (rsMag > 0.0f || rsMovedToRest)
				{
					RE::BSFixedString eventName = "RotateLock"sv;
					rsWasMoved = rsMag != 0.0f;
					// Create thumbstick event to send.
					auto thumbstickEvent = std::make_unique<RE::InputEvent* const>(Util::CreateThumbstickEvent(eventName, rsX * rsMag, rsY * rsMag, false));
					// Set pad to indicate that the co-op player sent the input, not P1.
					(*thumbstickEvent)->AsIDEvent()->pad24 = 0xCA11;
					Util::AddSyncedTask([&thumbstickEvent]() { Util::SendInputEvent(thumbstickEvent); });
				}
			}

			// Check if the exit menu bind was pressed.
			XINPUT_STATE buttonState;
			ZeroMemory(&buttonState, sizeof(buttonState));
			if (XInputGetState(controllerID, &buttonState) == ERROR_SUCCESS)
			{
				// Get XInput and game mask for the 'Cancel' bind.
				// Default to the 'B' button.
				auto escapeXIMask = XINPUT_GAMEPAD_B;
				uint32_t idCode = GAMEPAD_MASK_B;
				RE::BSFixedString eventName = "Cancel"sv;
				if (auto userEvents = RE::UserEvents::GetSingleton(); userEvents) 
				{
					// Set id code, event name, and XInputMask.
					eventName = userEvents->cancel;
					idCode = controlMap->GetMappedKey(eventName, RE::INPUT_DEVICE::kGamepad, RE::ControlMap::InputContextID::kMenuMode);
					if (glob.cdh->GAMEMASK_TO_XIMASK.contains(idCode))
					{
						escapeXIMask = glob.cdh->GAMEMASK_TO_XIMASK.at(idCode);
					}
				}

				// Button is pressed according to XInput controller state.
				if (buttonState.Gamepad.wButtons & escapeXIMask)
				{
					// Create button event and send through task.
					std::unique_ptr<RE::InputEvent* const> buttonEvent = std::make_unique<RE::InputEvent* const>(RE::ButtonEvent::Create(RE::INPUT_DEVICE::kGamepad, eventName, idCode, 1.0f, 0.0f));
					(*buttonEvent.get())->AsIDEvent()->pad24 = 0xCA11;
					Util::AddSyncedTask([&buttonEvent]() { Util::SendInputEvent(buttonEvent); });
				}
			}

			// Wait to sync with global time delta.
			if (waitTimeSecs > 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(max(0.0f, 1000.0f * waitTimeSecs))));
			}
		}

		// Reset MIM CID to -1 once the LockpickingMenu closes.
		glob.mim->managerMenuCID = -1;
	}

	void CoopPlayer::MountTask()
	{
		// Attempt to mount the player's targeted mount asynchronously.
		// Mounting through activation of the refr alone fails often,
		// and the co-op actor floats around and never attempts to mount
		// when approaching from the mount's right side or when the player's weapon is drawn.
		// Have to forcibly place the actor at the mounting point before activating.
		// Ugly solution until the cause of the activation failure is found.

		auto targetedMountPtr = Util::GetActorPtrFromHandle(targetedMountHandle);
		if (!targetedMountPtr || !targetedMountPtr.get()) 
		{
			mm->isMounting = false;
			return;
		}

		// Player is mounting while this task is executing.
		mm->isMounting = true;

		// Must sheathe weapons first to trigger mount animation.
		bool drawn = coopActor->IsWeaponDrawn();
		if (drawn)
		{
			Util::AddSyncedTask(
				[this]() {
					pam->ReadyWeapon(false);
				});

			// Must wait until fully sheathed to trigger mounting animation.
			const float secsMaxWait = 3.0f;
			float secsWaited = 0.0f;
			SteadyClock::time_point waitStartTP = SteadyClock::now();
			bool isEquipping = false;
			bool isUnequipping = false;
			coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			while (secsWaited < secsMaxWait && (coopActor->IsWeaponDrawn() || isEquipping || isUnequipping))
			{
				std::this_thread::sleep_for(0.1s);
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
				coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			}
		}

		// Mount point is to the left of the mount.
		auto leftOfMountPt = targetedMountPtr->data.location + Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(targetedMountPtr->data.angle.z - PI / 2.0f))) * 100.0f;
		if (!coopActor->IsOnMount())
		{
			Util::AddSyncedTask([this, targetedMountPtr, &leftOfMountPt]() {
				// Move to mount point and activate the mount.
				coopActor->SetGraphVariableBool("bAnimationDriven", true);
				coopActor->SetGraphVariableBool("bIsSynced", true);
				coopActor->SetPosition(leftOfMountPt, true);
				coopActor->Update3DPosition(true);
				Util::ActivateRef(targetedMountPtr.get(), coopActor.get(), 0, nullptr, 1, false);

				if (!isPlayer1) 
				{
					// Not sure if this helps the companion player mount successfully more often.
					coopActor->SetLastRiddenMount(targetedMountHandle);
					coopActor->PutActorOnMountQuick();
				}
			});
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
			// Mount failed, resurrect mount just in case it glitched out.
			Util::AddSyncedTask([targetedMountPtr]() {
				targetedMountPtr->Resurrect(false, true);
			});
			currentMountHandle.reset();
		}

		// Draw the player's weapons/magic if they were drawn before attempting the mount.
		if (drawn)
		{
			float maxWaitTimeSecs = 2.0f;
			float secsWaited = 0.0f;
			SteadyClock::time_point startTP = SteadyClock::now();
			while (coopActor->GetSitSleepState() != RE::SIT_SLEEP_STATE::kRidingMount && secsWaited < maxWaitTimeSecs)
			{
				std::this_thread::sleep_for(0.5s);
				secsWaited = Util::GetElapsedSeconds(startTP);
			}

			Util::AddSyncedTask(
				[this]() {
					pam->ReadyWeapon(true);
				});
		}

		// Done attempting mount.
		mm->isMounting = false;
	}

	void CoopPlayer::RefreshPlayerManagersTask()
	{
		// Debug option to signal all player managers to await refresh and then resume afterward, which will refresh their data.

		RequestStateChange(ManagerState::kAwaitingRefresh);
		SteadyClock::time_point waitStartTP = SteadyClock::now();
		float secsWaited = 0.0f;
		// Wait until the manager's state changes to awaiting refresh.
		// 1 second failsafe.
		while (currentState != ManagerState::kAwaitingRefresh && secsWaited < 1.0f)
		{
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
			// Wait one frame at a time.
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
		}

		// Change back to running.
		RequestStateChange(ManagerState::kRunning);
	}

	void CoopPlayer::ResetCompanionPlayerStateTask(const bool& a_unequipAll, const bool& a_reattachHavok)
	{
		// Debug option to reset the companion player, 
		// optionally unequipping all their gear and re-attaching havok + ragdolling them.
		// Acts as a catch-all debug option for whatever bugginess my bad code may inflict on this player.
		// Godspeed, my friend!

		bool wasTransformed = isTransforming || isTransformed;
		float healthBefore = coopActor->GetActorValue(RE::ActorValue::kHealth);
		// Save desired equip forms to restore later.
		auto savedLHForm = em->desiredEquippedForms[!EquipIndex::kLeftHand];
		auto savedRHForm = em->desiredEquippedForms[!EquipIndex::kRightHand];
		// Make sure the player is not moving during the reset.
		Util::NativeFunctions::SetDontMove(coopActor.get(), true);

		std::this_thread::sleep_for(0.1s);
		Util::AddSyncedTask([this, a_unequipAll, wasTransformed]() {
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
			coopActor->Resurrect(false, false);
		});

		std::this_thread::sleep_for(0.1s);

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
		Util::AddSyncedTask([this]() {
			coopActor->Disable();
		});

		secsMaxWait = 2.0f;
		secsWaited = 0.0f;
		waitStartTP = SteadyClock::now();
		while (!coopActor->IsDisabled() && secsWaited < secsMaxWait)
		{
			std::this_thread::sleep_for(0.1s);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
		}

		std::this_thread::sleep_for(0.1s);

		// Re-enable and wait until fully enabled.
		Util::AddSyncedTask([this]() {
			coopActor->Enable(false);
		});

		secsWaited = 0.0f;
		waitStartTP = SteadyClock::now();
		while (coopActor->IsDisabled() && secsWaited < secsMaxWait)
		{
			std::this_thread::sleep_for(0.1s);
			secsWaited = Util::GetElapsedSeconds(waitStartTP);
		}

		std::this_thread::sleep_for(0.1s);

		// Detach and re-attach havok, then ragdoll.
		if (a_reattachHavok)
		{
			Util::AddSyncedTask([this]() {
				coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				if (auto player3D = Util::GetRefr3D(coopActor.get()); player3D)
				{
					coopActor->DetachHavok(player3D.get());
					coopActor->InitHavok();
					coopActor->MoveHavok(true);
				}

				Util::PushActorAway(coopActor.get(), coopActor->data.location, -1.0f);
			});

			std::this_thread::sleep_for(0.1s);
		}

		// Restore previously equipped hand forms.
		if (!a_unequipAll || wasTransformed)
		{
			em->desiredEquippedForms[!EquipIndex::kLeftHand] = savedLHForm;
			em->desiredEquippedForms[!EquipIndex::kRightHand] = savedRHForm;
		}

		// Ensure health is set to previous pre-resurrection value.
		Util::AddSyncedTask([this, &healthBefore]() {
			float healthAfter = coopActor->GetActorValue(RE::ActorValue::kHealth);
			if (healthAfter != healthBefore)
			{
				pam->ModifyAV(RE::ActorValue::kHealth, healthBefore - healthAfter);
			}

			// Reset 'ghost' flag used for I-frames.
			if (auto actorBase = coopActor->GetActorBase(); actorBase)
			{
				actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
			}
		});

		// Enable movement again.
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);
	}

	void CoopPlayer::ShoutTask()
	{
		// If current voice form is a shout, get shout variation spell to cast and play shout start and release animations,
		// depending on what shout was equipped.
		// If it is a power, cast instantly.

		pam->isVoiceCasting = true;
		auto voiceForm = em->voiceForm;
		auto voiceSpell = em->voiceSpell;
		// Get voice spell associated with shout/power.
		bool isShout = voiceForm && voiceForm->Is(RE::FormType::Shout);
		auto highestVar = em->highestShoutVarIndex;

		// No voice form equipped, no voice spell equipped,
		// or P1 does not know any words of power for the current shout, so return.
		if ((!voiceForm) || (!voiceSpell) || (isShout && highestVar < 0))
		{
			return;
		}

		// Cast the spell corresponding to the highest shout variation or power.
		if (voiceSpell)
		{
			if (isShout)
			{
				auto shout = voiceForm->As<RE::TESShout>();
				// Set cooldown.
				pam->secsCurrentShoutCooldown = shout->variations[highestVar].recoveryTime * coopActor->GetActorValue(RE::ActorValue::kShoutRecoveryMult);
				// Release and stop animation events to play, and delay time between release and stop animations.
				RE::BSFixedString shoutReleaseAnim = "";
				RE::BSFixedString shoutStopAnim = "";
				std::chrono::duration secsDelayAfterStart = (0.5s);

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
					else if (shout && shout->formID == 0x48AC9)
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

				// Shout starts once release anim plays.
				shoutStartTP = SteadyClock::now();
				SendAnimEventSynced(shoutReleaseAnim);
				// Hold. HOLD.
				std::this_thread::sleep_for(secsDelayAfterStart);

				// Play associated shout sounds.
				// TODO: Testing.
				for (auto i = 0; i <= highestVar; ++i)
				{
					if (auto varSpell = shout->variations[i].spell; varSpell && !varSpell->effects.empty())
					{
						if (auto primaryEffect = varSpell->effects[0]; primaryEffect)
						{
							if (auto baseEffect = primaryEffect->baseEffect; baseEffect)
							{
								if (baseEffect->effectSounds.size() > !RE::MagicSystem::SoundID::kRelease)
								{
									auto releaseSound = baseEffect->effectSounds[!RE::MagicSystem::SoundID::kRelease].sound;
									if (const auto audioMgr = RE::BSAudioManager::GetSingleton(); audioMgr && releaseSound)
									{
										audioMgr->Play(releaseSound);
									}
								}
							}
						}
					}
				}

				// Stop the shout.
				// Spell will be cast right after.
				SendAnimEventSynced(shoutStopAnim);
			}

			bool shouldCastWithP1 = Util::ShouldCastWithP1(voiceSpell);
			Util::AddSyncedTask([this, shouldCastWithP1]() { pam->CastSpellWithMagicCaster(EquipIndex::kVoice, true, false, shouldCastWithP1); });
		}

		// Done shouting/using power.
		pam->isVoiceCasting = false;
	}

	void CoopPlayer::TeleportTask(RE::ActorHandle a_targetHandle)
	{
		// Teleport to another player through a pair of portals.

		auto targetActorPtr = Util::GetActorPtrFromHandle(a_targetHandle); 
		if (!targetActorPtr || !targetActorPtr.get())
		{
			return;
		}

		auto targetActor = targetActorPtr.get();
		// Don't move before teleporting.
		Util::NativeFunctions::SetDontMove(coopActor.get(), true);
		// Get portal form.
		if (auto teleportalActivator = RE::TESForm::LookupByID<RE::TESObjectACTI>(0x7CD55); teleportalActivator)
		{
			// MoveTo if parent cells are not loaded or one player is in an interior/exterior cell
			// the other player is in an exterior/interior cell, or the teleporting player is not loaded,
			// but the other player's cell is attached.
			// Otherwise, the players are in the same cell, so simply change position to the exit portal's position.
			bool shouldMoveTo = {
				(!targetActor->parentCell || !coopActor->parentCell) ||
				(targetActor->parentCell->IsExteriorCell() && coopActor->parentCell->IsInteriorCell()) ||
				(targetActor->parentCell->IsInteriorCell() && coopActor->parentCell->IsExteriorCell()) ||
				(targetActor->parentCell->IsAttached() && !coopActor->Is3DLoaded())
			};

			// Place down entry portal and set position to the entry portal.
			Util::AddSyncedTask([this, teleportalActivator]() {
				const auto entryPortal = coopActor->PlaceObjectAtMe(teleportalActivator, false);
				if (entryPortal && entryPortal.get())
				{
					coopActor->SetPosition(entryPortal.get()->data.location, true);
				}
			});

			// Let it materialize.
			std::this_thread::sleep_for(0.25s);
			// Then place exit portal at the target.
			RE::TESObjectREFRPtr exitPortal{};
			Util::AddSyncedTask([this, &exitPortal, targetActor, teleportalActivator]() {
				exitPortal = targetActor->PlaceObjectAtMe(teleportalActivator, false);
			});
			std::this_thread::sleep_for(0.25s);

			// If the portal was successfully placed, move the player to the exit portal.
			if (exitPortal && exitPortal.get())
			{
				if (shouldMoveTo)
				{
					Util::AddSyncedTask([this, &exitPortal]() {
						coopActor->MoveTo(exitPortal.get());
					});
				}
				else
				{
					Util::AddSyncedTask([this, &exitPortal]() {
						coopActor->SetPosition(exitPortal.get()->data.location, true);
					});
				}
			}
			else
			{
				if (shouldMoveTo)
				{
					Util::AddSyncedTask([this, targetActor]() {
						coopActor->MoveTo(targetActor);
					});
				}
				else
				{
					Util::AddSyncedTask([this, targetActor]() {
						coopActor->SetPosition(targetActor->data.location, true);
					});
				}
			}

			std::this_thread::sleep_for(0.25s);
		}

		// Can move again.
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);
	}

	void CoopPlayer::ToggleVampireLordLevitationTask()
	{
		// Toggle levitation on/off when transformed into a vampire lord
		// and wait until the levitation state changes.
		// 
		// NOTE: If levitation state goes out of sync with the FX and spells,
		// especially after performing a killmove,
		// toggle sneak twice to sync everything up again.

		// If P1 or not in Vampire Lord form, return here.
		if (isPlayer1 || !Util::IsVampireLord(coopActor.get()))
		{
			return;
		}

		// Task starts here.
		isTogglingLevitationStateTaskRunning = true;
		if (auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler)
		{
			bool succ = false;
			// Get leviation state before toggling levitation.
			bool wasLevitating = false;
			coopActor->GetGraphVariableBool("IsLevitating", wasLevitating);
			Util::AddSyncedTask(
				[this, dataHandler, &succ]() {
					succ = coopActor->NotifyAnimationGraph("LevitationToggleMoving");
					// Once the animation event request is sent, levitation is being toggled.
					isTogglingLevitationState = true;
				});

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
			while (secsWaited < 2.0f && (isLevitating == wasLevitating))
			{
				secsWaited = Util::GetElapsedSeconds(waitStartTP);
				// One frame at a time.
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(*g_deltaTimeRealTime * 1000.0f)));
				coopActor->GetGraphVariableBool("IsLevitating", isLevitating);
			}
		}

		// Done toggling, so this task can be queued again.
		isTogglingLevitationState = false;
		isTogglingLevitationStateTaskRunning = false;
	}
#pragma endregion

};

#include "CameraManager.h"
#include <Compatibility.h>
#include <Controller.h>
#include <DebugAPI.h>
#include <GlobalCoopData.h>
#include <Player.h>
#include <Settings.h>
#include <Util.h>
#include <numbers>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	CameraManager::CameraManager() :
		Manager(ManagerType::kCAM)
	{
		camLockOnTargetHandle =
		camDialogueTargetHandle = RE::ObjectRefHandle();
		camBaseTargetPos =  
		camRefrFocusPoint = 
		camCentroidPointDelta = 
		camCollisionOriginPoint = 
		camCollisionTargetPos = 
		camFocusPoint = 
		camLockOnFocusPoint = 
		camOriginPoint = 
		camTargetPos = RE::NiPoint3();
		camTargetXYOffset = 
		camXYOffset = RE::NiPoint2();
		camMaxAnchorPointZCoord = camMinAnchorPointZCoord = 0.0f;
		camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;
		playerCam = RE::PlayerCamera::GetSingleton();

		if (playerCam)
		{
			if (auto camState = playerCam->currentState.get(); camState)
			{
				tpState = 
				(
					skyrim_cast<RE::ThirdPersonState*>
					(
						playerCam->cameraStates[RE::CameraState::kThirdPerson].get()
					)
				);
			}
		}

		currentCell = nullptr;
		// Starts with no adjustment mode active and in the autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		camAdjMode = CamAdjustmentMode::kNone;
		lockOnIndicatorOscillationInterpData = std::make_unique<TwoWayInterpData>();
		lockOnIndicatorOscillationInterpData->SetInterpInterval
		(
			Settings::fSecsCamLockOnIndicatorOscillationUpdate, true
		);
		lockOnIndicatorOscillationInterpData->SetInterpInterval
		(
			Settings::fSecsCamLockOnIndicatorOscillationUpdate, false
		);
		movementAngleMultInterpData = std::make_unique<TwoWayInterpData>();
		movementAngleMultInterpData->SetInterpInterval(2.0f, true);
		movementAngleMultInterpData->SetInterpInterval(2.0f, false);
		// State bools.
		adjustedAfterReachingDialoguePos = false;
		autoRotateSuspended = false;
		delayedZoomInUnderExteriorRoof = false;
		delayedZoomOutUnderExteriorRoof = false;
		exteriorCell = false;
		inDeathCamState = false;
		inDialogueCamState = false;
		isAutoTrailing = true;
		isManuallyPositioned = false;
		isLockedOn = false;
		isTogglingPOV = false;
		lockInteriorOrientationOnInit = false;
		lockOnTargetInSight = false;
		movingToDialogueStartPos = false;
		shoulderOffsetRight = true;
		waitForToggle = false;
		// Positional offset floats.
		avgPlayerHeight = 100.0f;
		camRadialDistanceOffset = camSavedRadialDistanceOffset = 0.0f;
		camMinTrailingDistance = Settings::fCamMinTrailingDistance; 
		camTargetRadialDistance = 
		camTrueRadialDistance = 400.0f;
		camBaseHeightOffset = camHeightOffset = 0.0f;
		// Rotation floats.
		camBaseTargetPosPitch = camTargetPosPitch = 0.0f;
		camBaseTargetPosYaw = camTargetPosYaw = 0.0f;
		camCurrentPitchToFocus = camCurrentYawToFocus = 0.0f;
		camMaxPitchAngMag = 89.0f * PI / 180.0f;
		movementPitchRunningTotal = movementYawToCamRunningTotal = 0.0f;
		numMovementPitchReadings = numMovementYawToCamReadings = 0;
		// Other floats.
		camFOV = 75.0f;
		// Player IDs.
		adjustingCamPID = 
		focalPID =
		softFocalPID = -1;

		// XInput mask for the button that toggles the co-op camera.
		// Set by default to the 'Toggle POV' bind's XInput mask.
		auto controlMap = RE::ControlMap::GetSingleton();
		auto userEvents = RE::UserEvents::GetSingleton();
		camToggleXIMask =
		(
			controlMap && userEvents ? 
			controlMap->GetMappedKey(userEvents->togglePOV, RE::INPUT_DEVICE::kGamepad) :
			GAME_INPUT_CODE_RIGHT_THUMB
		);
		// Cam pitch and yaw calculated in the main task function.
		camPitch = camYaw = 0.0f;

		ResetTPs();
		ResetFadeAndClearObstructions();
	}

	void CameraManager::MainTask()
	{
		playerCam = RE::PlayerCamera::GetSingleton();
		if (!playerCam)
		{
			// Need to have a valid player camera.
			return;
		}

		// Disable auto-vanity cam.
		playerCam->idleTimer = FLT_MAX;
		playerCam->allowAutoVanityMode = false;
		if (!tpState) 
		{
			tpState = 
			(
				skyrim_cast<RE::ThirdPersonState*>
				(
					playerCam->cameraStates[RE::CameraState::kThirdPerson].get()
				)
			);
		}

		// Update camere state first.
		UpdateCamState();
		// Then focal PIDs.
		UpdateFocalPIDs();

		// Then adjust orientation.
		if (isAutoTrailing || isLockedOn || isManuallyPositioned || inDialogueCamState)
		{
			if (!isTogglingPOV)
			{
				SetCamInterpFactors();
				UpdateParentCell();
				CheckLockOnTarget();
				UpdateDialogueStateData();
				CalcNextOriginPoint();
				CalcNextFocusPoint();
				CalcNextTargetPosition();
				UpdatePlayerFadeAmounts();
				UpdateFOV();

				if (isAutoTrailing)
				{
					UpdateCamHeight();
					UpdateCamZoom();
					UpdateCamRotation();
				}
				else if (isLockedOn || inDialogueCamState)
				{
					UpdateCamHeight();
					UpdateCamRotation();
					UpdateCamZoom();
				}
				else
				{
					UpdateCamHeight();
					UpdateCamRotation();
				}

				FadeObstructions();
			}
		}
		else if (inDeathCamState)
		{
			UpdateDeathCameraOrientation();
		}
		
		// Set the camera's orientation and override its local rotation.
		// Since this manager runs through the Main() hook, 
		// updating here allows any other mod that changes the camera's local rotation 
		// after us to have their changes stack on our own changes 
		// and not the game's original computed local rotation for this frame.
		SetCamOrientation(true);
		// Update previous state.
		prevCamState = camState;

		// REMOVE when done debugging.
		/*if (camTargetPos == RE::NiPoint3() || 
			isnan(camTargetPos.x) ||
			isnan(camTargetPos.x) ||
			isnan(camTargetPos.z))*/
		/*{
			DBG
			(
				"Target pos: {}, focus: {}, centroid: {}, "
				"pitches: ({}, {}, {}), yaws: ({}, {}, {}), "
				"leading cam PID: {}, movement pitch/yaw offsets: {}, {}.",
				camTargetPos, camFocusPoint, camCentroidPoint, camBaseTargetPosPitch, 
				camTargetPosPitch * TO_DEGREES, camPitch * TO_DEGREES, 
				camBaseTargetPosYaw * TO_DEGREES, camTargetPosYaw * TO_DEGREES, 
				camYaw * TO_DEGREES, leadingCamPID, movementPitchOffset * TO_DEGREES, 
				movementYawOffset * TO_DEGREES
			);
		}*/
		
		// REMOVE when done debugging.
		/*DebugAPI::QueuePoint3D(ToVec3(camOriginPoint), 0xFFFF00FF, 5.0f);
		DebugAPI::QueuePoint3D(ToVec3(camFocusPoint), 0xFF00FFFF, 5.0f);
		if (isLockedOn)
		{
			DebugAPI::QueuePoint3D(ToVec3(camLockOnFocusPoint), 0x00FFFFFF, 5.0f);
		}*/
	}

	void CameraManager::PrePauseTask()
	{
		DBG("PrePauseTask");
		
		// Reset no fade flags for all players.
		SetPlayerFadePrevention(false);
		// Add back camera-actor collisions before switching to default cam.
		SetCamActorCollisions(true);
		// Ensure all players are visible.
		UpdatePlayerFadeAmounts(true);
		// Toggle all of P1's controls back on.
		Util::ToggleAllControls(true);
		// P1 should be motion driven again.
		Util::SetPlayerAIDriven(false);
		// Reset third person camera orientation.
		Util::ResetTPCamOrientation();

		// Reset fade on handled objects.
		ResetFadeAndClearObstructions();

		// Reset crosshair text and position.
		GlobalCoopData::SetCrosshairText(true);
	}

	void CameraManager::PreStartTask()
	{
		DBG("PreStartTask");
		
		// Prevent the game from fading all players while the camera is active.
		SetPlayerFadePrevention(true);
		// Remove camera-actor collisions before switching to co-op cam.
		SetCamActorCollisions(false);
		// Refresh data.
		RefreshData();
		// Ensure all players are visible.
		UpdatePlayerFadeAmounts(true);
		// Reset fade and clear obstruction data.
		ResetFadeAndClearObstructions();

		// Make sure P1's managers are running once enabled.
		bool shouldEnableP1Managers = 
		(
			(glob.globalDataInit && glob.player1DID != -1) &&
			(
				(!glob.coopSessionActive && glob.singleplayerModeActive) ||
				(glob.allPlayersInit && glob.coopSessionActive)
			)
		);
		if (shouldEnableP1Managers)
		{
			const auto& coopP1 = glob.coopPlayers[0];
			if (coopP1 && !coopP1->isDowned)
			{
				coopP1->RequestStateChange(ManagerState::kRunning);
			}
		}
	}

	void CameraManager::RefreshData()
	{
		DBG("RefreshData");

		// Update parent cell.
		UpdateParentCell();
		// Reset all time points and orientation data.
		ResetTPs();
		ResetCamData();

		// Reset toggle state on each refresh.
		isTogglingPOV = false;
		waitForToggle = false;
	}

	const ManagerState CameraManager::ShouldSelfPause()
	{
		if (glob.loadingASave)
		{
			return ManagerState::kAwaitingRefresh;		
		}

		// Never active when in hybrid mode.
		if (glob.hybridModeActive)
		{
			return ManagerState::kPaused;
		}

		// Check if all players are valid, and if one isn't, pause.
		if (!glob.partyWiped)
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}
			
				bool isInvalid = 
				{
					(p->coopActor->parentCell && !p->coopActor->parentCell->IsAttached()) ||
					p->coopActor->IsDisabled() ||
					!p->coopActor->Is3DLoaded() ||
					!p->coopActor->loadedData ||
					!p->coopActor->currentProcess ||
					!p->coopActor->GetCharController()
				};

				if (isInvalid)
				{
					return ManagerState::kPaused;
				}
			}
		}
		

		// Pause when the map menu is open to prevent glitches upon closure 
		// and to also enable fast travel while in the map menu.
		// Pause when a fader menu opens since P1 is likely being repositioned.
		// 
		// Pause in the fader/loading menu to prevent carry-over 
		// of the previous cell's camera position, which is usually still applied
		// when the game auto-saves and results in a nice shot of the unloaded void 
		// in the generated savegame thumbnail.
		// Pause in the race menu to allow P1 to see their character edits more easily.
		auto ui = RE::UI::GetSingleton(); 
		if ((ui) && 
			(
				/*(
					ALYSLC::AlternateConversationCameraCompat::g_installed &&
					ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) 
				) ||*/
				ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
				ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)
			))
		{
			return ManagerState::kPaused;
		}

		// Keep paused while changing POV.
		if (isTogglingPOV) 
		{
			return ManagerState::kPaused;
		}

		// Since P1 will stop mining unless the camera is allowed to change states,
		// keep the camera manager paused until no longer mining.
		/*if (glob.player1Actor->GetOccupiedFurniture())
		{
			auto furnitureRefrPtr = Util::GetRefrPtrFromHandle
			(
				glob.player1Actor->GetOccupiedFurniture()
			);
			auto furniture = 
			(
				furnitureRefrPtr && furnitureRefrPtr->GetBaseObject() && 
				furnitureRefrPtr->GetBaseObject()->Is(RE::FormType::Furniture) ? 
				furnitureRefrPtr->GetBaseObject()->As<RE::TESFurniture>() : 
				nullptr 
			);
			if ((furniture) && 
				(furniture->HasKeywordString("isPickaxeFloor") ||
				 furniture->HasKeywordString("isPickaxeTable") || 
				 furniture->HasKeywordString("isPickaxeWall")))
			{
				return ManagerState::kPaused;
			}
		}*/

		return currentState;
	}

	const ManagerState CameraManager::ShouldSelfResume()
	{
		// Never resume when in hybrid mode.
		if (glob.hybridModeActive)
		{
			return currentState;
		}

		if (waitForToggle || !glob.coopSessionActive)
		{
			return currentState;
		}

		bool allPlayersValid = false;
		if (glob.livingPlayers > 1)
		{
			// Maintain paused state when changing POV.
			if (isTogglingPOV)
			{
				return currentState;
			}

			// First, check for player validity.
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}

				allPlayersValid = 
				{
					!p->coopActor->IsDisabled() &&
					p->coopActor->Is3DLoaded() &&
					p->coopActor->loadedData &&
					p->coopActor->currentProcess &&
					p->coopActor->GetCharController() &&
					p->coopActor->parentCell && 
					p->coopActor->parentCell->IsAttached()
				};

				if (!allPlayersValid)
				{
					return currentState;
				}
			}
			
			// If P1 is downed, resume right away, 
			// since other players will not be able to control the bleedout camera.
			if (glob.coopPlayers[0]->isDowned)
			{
				return ManagerState::kRunning;
			}

			// Then ensure the map menu is not open.
			// Pause when the map menu is open to prevent glitches upon closure 
			// and to also enable fast travel while in the map menu.
			// Remain paused until the loading menu closes 
			// and P1 has been positioned in the new cell.
			// Remain paused in the race menu while P1 edits their character.
			auto ui = RE::UI::GetSingleton(); 
			if ((ui) && 
				(
					/*(
						ALYSLC::AlternateConversationCameraCompat::g_installed &&
						ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) 
					) ||*/
					ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)
				))
			{
				return currentState;
			}

			/*
			// Have to pause here because the player will stop mining while the camera is enabled, 
			// even if the camera's current state is set to furniture.
			if (glob.player1Actor->GetOccupiedFurniture())
			{
				auto furnitureRefr = Util::GetRefrPtrFromHandle
				(
					glob.player1Actor->GetOccupiedFurniture()
				);
				auto furniture = 
				(
					furnitureRefr && furnitureRefr->GetBaseObject() &&
					furnitureRefr->GetBaseObject()->Is(RE::FormType::Furniture) ?
					furnitureRefr->GetBaseObject()->As<RE::TESFurniture>() :
					nullptr
				);

				// Special interaction keywords.
				// Thanks to Dakraid:
				// https://github.com/Dakraid/ImmersiveFirstPersonView/blob/master/ImmersiveFirstPersonView/States/SpecialFurniture.cs#L7
				//"FurnitureWoodChoppingBlock"
				//"FurnitureResourceObjectSawmill"
				//"isCartTravelPlayer"
				//"isPickaxeTable"
				//"isPickaxeWall"
				//"isPickaxeFloor"

				if ((furniture) && 
					(furniture->HasKeywordString("isPickaxeFloor") || 
					 furniture->HasKeywordString("isPickaxeTable") || 
					 furniture->HasKeywordString("isPickaxeWall")))
				{
					return ManagerState::kPaused;
				}
			}
			*/

			// Next, when waiting to toggle the camera back on, 
			// ensure that all menus that pause the game are closed
			// and that the cam toggle bind was released.
			/*
			if (allPlayersValid && waitForToggle)
			{
				auto ui = RE::UI::GetSingleton();
				auto dataHandler = RE::TESDataHandler::GetSingleton();
				bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				bool allMenusClosed = 
				(
					!ui->GameIsPaused() && 
					ui->IsSavingAllowed() && 
					onlyAlwaysOpen
				);
				bool isAutoSaving = dataHandler->autoSaving || dataHandler->saveLoadGame;
				bool shouldResume = false;
				// Resume if a co-op session is active, 
				// all menus are closed, 
				// the game is not autosaving, 
				// and the thread state is running or paused 
				// with all players moving after cell change.
				bool menusClosed = allMenusClosed && !isAutoSaving;

				if (menusClosed)
				{
					// RS click used as cam toggle button (POV switch disabled).
					auto controlMap = RE::ControlMap::GetSingleton();
					auto userEvents = RE::UserEvents::GetSingleton();
					camToggleXIMask =
					(
						controlMap && userEvents ?
						controlMap->GetMappedKey
						(
							userEvents->togglePOV, RE::INPUT_DEVICE::kGamepad
						) :
						GAME_INPUT_CODE_RIGHT_THUMB
					);
					InputAction toggleAction = InputAction::kRThumb;
					const auto iter = glob.cdh->GAMEMASK_TO_INPUT_ACTION.find(camToggleXIMask);
					if (iter != glob.cdh->GAMEMASK_TO_INPUT_ACTION.end())
					{
						toggleAction = iter->second;
					}

					// Double tap to resume.
					// TODO: Keyboard support.
					shouldResume = 
					(
						glob.cdh->GetInputState(glob.player1DID, toggleAction).consecPresses > 1
					);
				}

				if (shouldResume)
				{
					// When toggled back on, make sure P1's managers also resume.
					// P1 will only be able to move around otherwise 
					// since none of their managers are active.
					if (glob.allPlayersInit && glob.coopSessionActive && glob.player1DID != -1)
					{
						const auto& coopP1 = glob.coopPlayers[0];
						if (!coopP1->isDowned)
						{
							coopP1->RequestStateChange(ManagerState::kRunning);
						}
					}

					return ManagerState::kRunning;
				}
				else
				{
					return currentState;
				}
			}
			*/
		}

		return allPlayersValid ? ManagerState::kRunning : currentState;
	}

	bool CameraManager::AllPlayersOnScreenAtCamOrientation
	(
		bool a_checkBoundingBox, const std::vector<RE::BSFixedString>& a_nodeNamesToCheck
	)
	{
		// Check if all players are within the camera's frustum 
		// at the current position and rotation.
		// Check the player's refr position or check a list of player nodes.

		return AllPlayersOnScreenAtCamOrientation
		(
			camTargetPos, 
			{ camPitch, camYaw },
			std::move(a_checkBoundingBox),
			a_nodeNamesToCheck
		);
	}

	bool CameraManager::AllPlayersOnScreenAtCamOrientation
	(
		const RE::NiPoint3& a_camPos, 
		const RE::NiPoint2& a_rotation,
		bool a_checkBoundingBox, 
		const std::vector<RE::BSFixedString>& a_nodeNamesToCheck
	)
	{
		// Check if the players's entire bounding box (bounding diamond approximation) 
		// is on screen at the current/given camera orientation.
		// Will check if at least one node is on screen if not checking the players' bounding boxes.
		
		auto niCamPtr = Util::GetNiCamera();
		const auto hud = DebugAPI::GetHUD(); 
		if (!niCamPtr || !hud || !hud->uiMovie)
		{
			return false;
		}

		const auto strings = RE::FixedStrings::GetSingleton();
		bool allPlayersInFrontOfPoint = true;
		auto getRefrInFrontOfPoint = 
		[&](RE::TESObjectREFR* a_refr)
		{
			if (!a_refr)
			{
				return false;
			}

			if (a_checkBoundingBox)
			{
				if (!playerCam || !playerCam->cameraRoot)
				{
					return false;
				}

				// Larger FPS hit (~8-10% vs 4-8%) than checking one node, but also more precise.
				RE::NiPoint3 boundMax{ };
				RE::NiPoint3 boundMin{ };
				RE::NiPoint3 boundCenter{ };
				RE::NiMatrix3 rotMat{ }; 
				Util::GetWorldBoundData(a_refr, boundMin, boundMax, boundCenter, rotMat);

				// Offset from the bounding box's center to one of the corners 
				// along the positive X and Y axes.
				auto halfExtent = (boundMax - boundMin) / 2.0f;
				std::vector<RE::NiPoint3> boundVertices{ };

				// Negative Z extent vertices first because typically an actor/refr's 
				// bottom vertices are most likely to lie offscreen.
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
				);

				// Positive Z extent vertices.
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
				);
				boundVertices.emplace_back
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				
				//
				// [Bounding Diamond Approximation]:
				// Cut down on the number of expensive WorldPtToScreenPt3() calls from 8 to 1-4
				// per call of this function.
				//

				// Camera directions to project the vertex offsets onto, 
				// 'flattening' the world offsets to the plane of the screen.
				const auto camUp = 
				(
					playerCam->cameraRoot->local.rotate * RE::NiPoint3(0.0f, 0.0f, 1.0f)
				);
				const auto camRight = 
				(
					playerCam->cameraRoot->local.rotate * RE::NiPoint3(1.0f, 0.0f, 0.0f)
				);
				// Project the vertex offsets onto the camera direction vectors
				// to obtain 4 min/max bounding diamond vertices:
				// Up, down, left, right relative to the camera's facing direction.
				float maxCamRightDot= -FLT_MAX;
				float maxCamUpDot = -FLT_MAX;
				float minCamRightDot = FLT_MAX;
				float minCamUpDot = FLT_MAX;
				RE::NiPoint3 maxCamRightDotOffset{ };
				RE::NiPoint3 maxCamUpDotOffset{ };
				RE::NiPoint3 minCamRightDotOffset{ };
				RE::NiPoint3 minCamUpDotOffset{ };
				for (auto i = 0; i < boundVertices.size(); ++i)
				{
					const auto& vert = boundVertices[i];
					const auto offset = vert - boundCenter;
					float dot = offset.Dot(camRight);
					if (dot > maxCamRightDot)
					{
						maxCamRightDot = dot;
						maxCamRightDotOffset = dot * camRight;
					}

					if (dot < minCamRightDot)
					{
						minCamRightDot = dot;
						minCamRightDotOffset = dot * camRight;
					}

					dot = offset.Dot(camUp);
					if (dot > maxCamUpDot)
					{
						maxCamUpDot = dot;
						maxCamUpDotOffset = dot * camUp;
					}

					if (dot < minCamUpDot)
					{
						minCamUpDot = dot;
						minCamUpDotOffset = dot * camUp;
					}
				}
			
				/*
				// REMOVE when done debugging.
				// No actual performance improvement over checking 4 bounding box nodes instead.

				uint8_t checkedNum = 0;

				// Are all bounding box verts on screen?
				bool onScreen = false;
				// Ratio of width world offset length over height world offset length.
				const float widthToHeightOffsetRatio = maxCamRightDot / maxCamUpDot;
				// Approximating the ratio of the width/height world and screen ratios.
				// This is required because I have not mathematically derived the exact ratio 
				// for the discrepancy in 3D/2D bounding box width-to-height ratios.
				// Can skip another WorldPtToScreenPt3() call.
				// Yippee for marginal performance gains!
				const float approxScreenToWorldRatio = 1.111f;

				RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
				const float rectWidth = fabsf(gRect.right - gRect.left);
				const float rectHeight = fabsf(gRect.bottom - gRect.top);
				const float heightMarginPixels = 0.05f * rectHeight;
				const float widthMarginPixels = 0.05f * rectWidth;
				RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };

				// Check if center is onscreen first.
				float centerX = 0.0f;
				float centerY = 0.0f;
				float centerZ = 0.0f;
				RE::NiCamera::WorldPtToScreenPt3
				(
					niCamPtr->worldToCam, port, boundCenter, centerX, centerY, centerZ, 1e-5f
				);
				onScreen =
				(
					centerX >= gRect.left + widthMarginPixels && 
					centerX <= gRect.right - widthMarginPixels && 
					centerY >= gRect.top + heightMarginPixels && 
					centerY <= gRect.bottom - heightMarginPixels && 
					centerZ < 1.0f && 
					centerZ > -1.0f
				);

				++checkedNum;

				if (onScreen)
				{
					// Down is the next most likely offset to be offscreen.
					float offsetX = 0.0f;
					float offsetY = 0.0f;
					float offsetZ = 0.0f;
					RE::NiCamera::WorldPtToScreenPt3
					(
						niCamPtr->worldToCam,
						port,
						boundCenter + minCamUpDotOffset,
						offsetX,
						offsetY,
						offsetZ, 
						1e-5f
					);
					onScreen =
					(
						offsetX >= gRect.left + widthMarginPixels && 
						offsetX <= gRect.right - widthMarginPixels && 
						offsetY >= gRect.top + heightMarginPixels && 
						offsetY <= gRect.bottom - heightMarginPixels && 
						offsetZ < 1.0f && 
						offsetZ > -1.0f
					);

					++checkedNum;

					const float screenHalfExtentHeight = Util::GetXYDistance
					(
						offsetX, offsetY, centerX, centerY
					);
					if (onScreen)
					{
						// Check up now. We don't need to obtain its screen coordinates 
						// via the expensive world-to-cam projection this time.
						// It shares the same X and Z screen coordinates as the down offset pos.
						offsetY = centerY - screenHalfExtentHeight;
						onScreen = offsetY >= gRect.top + heightMarginPixels;

						++checkedNum;

						if (onScreen)
						{
							// Next up is right. We'll use the magic width/height ratio 
							// to approximate the pixel width of the target.
							const float screenHalfExtentWidth =
							(
								screenHalfExtentHeight * 
								widthToHeightOffsetRatio *
								approxScreenToWorldRatio
							);
							offsetX = centerX + screenHalfExtentWidth;
							onScreen =
							(
								offsetX >= gRect.left + widthMarginPixels && 
								offsetX <= gRect.right - widthMarginPixels && 
								offsetY >= gRect.top + heightMarginPixels && 
								offsetY <= gRect.bottom - heightMarginPixels && 
								offsetZ < 1.0f && 
								offsetZ > -1.0f
							);

							++checkedNum;

							DBG
							(
								"Screen half extents (W, H): {}, {}, ratio: {}, "
								"world offset ratio: {}. Ratio of the corrected ratios: {}. "
								"RATIOOOOOOOO!",
								screenHalfExtentWidth,
								screenHalfExtentHeight,
								screenHalfExtentWidth / screenHalfExtentHeight,
								widthToHeightOffsetRatio,
								screenHalfExtentWidth / 
								screenHalfExtentHeight /
								widthToHeightOffsetRatio
							);
							if (onScreen)
							{
								// And lastly, left. No need to call WorldPtToScreenPt3 again,
								// for the same reason as for the up and right offsets.
								offsetX = centerX - screenHalfExtentWidth;
								onScreen = offsetX >= gRect.left + widthMarginPixels;

								++checkedNum;
							}
						}
					}

					//DBG("{} checks for {}. All on screen: {}.", 
					//	checkedNum, a_refr->GetName(), onScreen);
				}
				*/
				
				// Four verts for a rectangle in the plane of the camera 
				// that contain all 8 3D bounding box verts.
				std::vector<RE::NiPoint3> bound2DVerts = 
				{
					boundCenter + maxCamRightDotOffset + maxCamUpDotOffset, 
					boundCenter + minCamRightDotOffset + minCamUpDotOffset, 
					boundCenter + maxCamRightDotOffset + minCamUpDotOffset, 
					boundCenter + minCamRightDotOffset + maxCamUpDotOffset, 
				};

				uint8_t checkedNum = 0;
				bool onScreen = true;
				for (const auto& vert : bound2DVerts)
				{
					// No need to continue checking once one vert is offscreen.
					if (!onScreen)
					{
						break;
					}

					onScreen &= PointOnScreenAtCamOrientationScreenspaceMargin(vert, 0.05f);
					checkedNum++;
				}

				return onScreen;

				// Old code to switch back to if the above bounding box check is too expensive.
				// Approximate lowest screen position for non-humanoid NPC targets;
				// use actual refr position for humanoids.
				/*auto checkPos = 
				(
					a_refr->HasKeyword(glob.npcKeyword) ? 
					a_refr->data.location : 
					a_refr->data.location + 
					RE::NiPoint3
					(
						0.0f,
						0.0f,
						a_refr->GetHeight() - 
						1.5f * Util::GetBoundMaxOrMinEdgeDist(a_refr, true, false)
					)
				);
				return PointOnScreenAtCamOrientationScreenspaceMargin(checkPos, 0.05f);*/
			}
			else
			{
				// Invalid 3D for one player means not all players 
				// are in front of the camera. Return early.
				auto loadedData = a_refr->loadedData;
				if (!loadedData)
				{
					return false;
				}

				auto data3DPtr = loadedData->data3D;
				if (!data3DPtr || !data3DPtr->parent)
				{
					return false;
				}

				bool onePlayerNodeOnScreen = false;
				if (a_nodeNamesToCheck.size() > 0)
				{
					// Check provided list.
					for (const auto& nodeName : a_nodeNamesToCheck)
					{
						// Minimum of one node must be visible 
						// from the camera target position
						// to consider the player as in view of the camera.
						auto nodePtr = RE::NiPointer<RE::NiAVObject>
						(
							data3DPtr->GetObjectByName(nodeName)
						); 
						if (nodePtr)
						{
							onePlayerNodeOnScreen |= PointOnScreenAtCamOrientationScreenspaceMargin
							(
								nodePtr->world.translate, 0.05f
							);
							// No need to check other nodes if one is visible.
							if (onePlayerNodeOnScreen)
							{
								break;
							}
							else
							{
								continue;
							}
						}
					}
				}
				else
				{
					// Check default list of nodes for visibility.
					for (const auto& nodeName : GlobalCoopData::CAM_VISIBILITY_NPC_NODES)
					{
						// Minimum of one node must be visible 
						// from the camera target position
						// to consider the player as in view of the camera.
						auto nodePtr = RE::NiPointer<RE::NiAVObject>
						(
							data3DPtr->GetObjectByName(nodeName)
						);
						if (nodePtr)
						{
							onePlayerNodeOnScreen |= PointOnScreenAtCamOrientationScreenspaceMargin
							(
								nodePtr->world.translate, 0.05f
							);
							// No need to check other nodes if one is visible.
							if (onePlayerNodeOnScreen)
							{
								break;
							}
							else
							{
								continue;
							}
						}
					}
				}

				return onePlayerNodeOnScreen;
			}
		};
		
		// First, set the given cam orientation if it differs from the last set orientation.
		bool fromCurrentOrientation = 
		(
			a_camPos == camTargetPos && a_rotation.x == camPitch && a_rotation.y == camYaw
		);
		if (!fromCurrentOrientation)
		{
			SetCamOrientation(a_camPos, a_rotation.x, a_rotation.y, true);
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			// Skip non-focal players.
			if (focalPID != -1 && p->playerID != focalPID)
			{
				continue;
			}

			// Break once one player is not on screen at this point.
			allPlayersInFrontOfPoint = getRefrInFrontOfPoint
			(
				p->mm->movementActorPtr && p->coopActor->IsOnMount() ? 
				p->mm->movementActorPtr.get() :
				p->coopActor.get()
			);
			if (!allPlayersInFrontOfPoint)
			{
				break;
			}
		}

		if (allPlayersInFrontOfPoint && ShouldConsiderCamTargetAsPlayer() && !facingLockOnTarget)
		{
			const auto targetPtr = Util::GetRefrPtrFromHandle
			(
				inDialogueCamState ? camDialogueTargetHandle : camLockOnTargetHandle
			);
			if (targetPtr)
			{
				allPlayersInFrontOfPoint = getRefrInFrontOfPoint(targetPtr.get());
			}
		}

		return allPlayersInFrontOfPoint;
	}

	bool CameraManager::AllPlayersVisibleAtPoint
	(
		const RE::NiPoint3& a_point, bool&& a_checkAllNodes
	)
	{
		// Check all players are visible at the given point.
		// For each player, check if any player nodes are blocked from view.
		// 'Visibility' here means a raycast to one or all nodes for each player does not hit
		// any intervening object.

		const auto strings = RE::FixedStrings::GetSingleton();
		const glm::vec4 point = ToVec4(a_point);
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			// Invalid 3D means this player is not visible.
			// Move on to the next player.
			auto loadedData = p->coopActor->loadedData;
			if (!loadedData)
			{
				continue;
			}

			auto data3DPtr = loadedData->data3D;
			if (!data3DPtr || !data3DPtr->parent)
			{
				continue;
			}
				
			bool allNodesVisible = true;
			bool hitPlayerNode = false;
			bool oneNodeVisible = false;
			for (const auto& nodeName : GlobalCoopData::CAM_VISIBILITY_NPC_NODES)
			{
				// All checked nodes must be visible from the camera target position
				// to consider the player as in view of the camera.
				auto nodePtr = RE::NiPointer<RE::NiAVObject>
				(
					data3DPtr->GetObjectByName(nodeName)
				); 
				if (nodePtr)
				{
					auto losCheck = Raycast::hkpCastRay
					(
						ToVec4(nodePtr->world.translate), 
						point, 
						std::vector<RE::NiAVObject*>
						{
							playerCam->cameraRoot.get(), data3DPtr.get() 
						}, 
						RE::COL_LAYER::kLOS
					);
					bool hitPlayerNode = !losCheck.hit;
					oneNodeVisible = hitPlayerNode;
					allNodesVisible &= oneNodeVisible;
					if (a_checkAllNodes)
					{
						if (allNodesVisible)
						{
							// Keep checking nodes if this node was visible.
							continue;
						}
						else
						{
							// If this player has a node that is not visible,
							// we can return early.
							return false;
						}
					}
					else
					{
						if (oneNodeVisible)
						{
							// Break here to move on to the next player since one node was visible.
							break;
						}
						else
						{
							// Continue checking this player for one visible node.
							continue;
						}
					}
				}
			}

			if (a_checkAllNodes)
			{
				// Not all nodes visible for this player. End check.
				if (!allNodesVisible)
				{
					return false;
				}
			}
			else
			{
				// One node was not visible for this player. End check.
				if (!oneNodeVisible)
				{
					return false;
				}
			}
		}

		// All players checked and either all nodes or > 1 node for each player were visible.
		return true;
	}

	void CameraManager::CalcNextFocusPoint()
	{
		// Calculate the next focus point (origin point offset along the Z axis).
		// If in auto-trail mode or locked on without a valid target,
		// adjust the focus point relative to the origin point.

		auto camNodePos = tpState->camera->cameraRoot->world.translate;
		auto oldFocusPoint = camFocusPoint;
		auto prevOffset = camHeightOffset;

		bool skipAddingHeightOffset = 
		(
			isLockedOn && Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
		);
		if (camCollisions)
		{
			if (isManuallyPositioned)
			{
				// Focus point is the node point when in free cam mode.
				camFocusPoint = camNodePos;
			}
			else
			{
				camFocusPoint = RE::NiPoint3
				(
					camOriginPoint.x, 
					camOriginPoint.y, 
					camOriginPoint.z + (skipAddingHeightOffset ? 0.0f : camHeightOffset)
				);
			}
		}
		else
		{
			// Same point if collisions are not enabled.
			camFocusPoint = RE::NiPoint3
			(
				camOriginPoint.x, 
				camOriginPoint.y, 
				camOriginPoint.z + (skipAddingHeightOffset ? 0.0f : camBaseHeightOffset)
			);
		}
	}

	void CameraManager::CalcNextOriginPoint()
	{
		// Calculate the next target origin point.
		// Base point is equidistant to all players.
		// Other derived origin points account for collisions with geometry
		// and are kept from going 'out of bounds' to a normally unreachable position, 
		// since the collision origin points are vital for calculating the cam target positions.

		//====================
		//[Base Origin Point]:
		//====================

		auto oldCentroidPoint = camCentroidPoint;
		auto oldCollisionOriginPoint = camCollisionOriginPoint;
		auto oldOriginPoint = camOriginPoint;
		// If true, no players are visible from the origin point.
		bool originViewObstructed = false;
		// Was there a raycast hit from the old origin point to the new base origin point?
		bool hitToBasePos = false;
		// Was there a raycast hit from the old origin point
		// to the collision hit point obtained from raycasting
		// from the old origin point to the base origin point 
		// and shifting the result above ground?
		bool hitToCollisionPos = false;
		// Vertical coordinate bounds obtained from clamping 
		// vertical raycasts hit results.
		// +- FLT_MAX if unbounded.
		std::pair<float, float> bounds{ oldOriginPoint.z, oldOriginPoint.z };
		// Additional offset to apply above/below the vertical bounds.
		float minZOffset = std::clamp(avgPlayerHeight, 50.0f, 100.0f);

		camCentroidPoint =
		camOriginPoint = RE::NiPoint3();

		// Calculate the point equidistant to all players first (no collisions accounted for).		
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			auto mountPtr = p->GetCurrentMount();
			camCentroidPoint += 
			(
				mountPtr ?
				mountPtr->data.location :
				p->coopActor->data.location
			);
		}

		camCentroidPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
		camCentroidPoint.x = Util::InterpolateSmootherStep
		(
			oldCentroidPoint.x, camCentroidPoint.x, 0.5f * camInterpFactor
		);
		camCentroidPoint.y = Util::InterpolateSmootherStep
		(
			oldCentroidPoint.y, camCentroidPoint.y, 0.5f * camInterpFactor
		);
		camCentroidPoint.z = Util::InterpolateSmootherStep
		(
			oldCentroidPoint.z, camCentroidPoint.z, 0.5f * camInterpFactor
		);

		const auto camDir = Util::RotationToDirectionVect
		(
			-camPitch, Util::ConvertAngle(camTargetPosYaw)
		);
		const auto centroidFocusPoint = 
		(
			camCentroidPoint + RE::NiPoint3(0.0f, 0.0f, avgPlayerHeight)
		);
		auto centroidPointDirXY = camCentroidPointDelta;
		centroidPointDirXY.z = 0.0f;
		
		// Origin set as the point along the camera's direction ray that is orthogonal 
		// to the position of player that is closest/furthest behind the camera.
		// Done to keep more players on screen by having the camera placed 
		// as far away along the back walls of a cell as possible.
		// 
		// Project XY position of player closest/farthest behind the camera
		// onto the camera's XY direction ray.
		// Z coordinate matches the player's Z coordinate.
		float maxDot = -FLT_MAX;
		float minDot = FLT_MAX;
		int32_t prevLeadingCamPID = leadingCamPID;
		int32_t prevOriginPointPID = originPointPID;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			const auto& pos = p->tm->playerMotionState->cPos;
			const RE::NiPoint3 centroidToPlayerXY =
			{
				pos.x - centroidFocusPoint.x, pos.y - centroidFocusPoint.y, 0.0f
			};
			float aDotB = centroidToPlayerXY.Dot(camDir);
			if (aDotB < minDot)
			{
				const RE::NiPoint3 camOriginPointXY = centroidFocusPoint + camDir * aDotB;
				camOriginPoint.x = camOriginPointXY.x;
				camOriginPoint.y = camOriginPointXY.y;
				camOriginPoint.z = pos.z + avgPlayerHeight;
				minDot = aDotB;
				originPointPID = p->playerID;
			}
			
			aDotB = centroidPointDirXY.Dot(centroidToPlayerXY); 
			if (aDotB > maxDot)
			{
				maxDot = aDotB;
				leadingCamPID = p->playerID;
			}
		}

		if (leadingCamPID != prevLeadingCamPID)
		{
			leadingCamPIDChangeTP = SteadyClock::now();
		}

		if (originPointPID != prevOriginPointPID)
		{
			originPointPIDChangeTP = SteadyClock::now();
		}
		
		// Slow down and smooth out origin point movement 
		// if the player closest to the origin has changed.
		float originPointPIDChangeMult = 1.0f;
		float secsSinceChange = Util::GetElapsedSeconds(originPointPIDChangeTP);
		if (secsSinceChange <= 1.0f)
		{
			originPointPIDChangeMult = Util::InterpolateSmootherStep
			(
				0.2f, 1.0f, secsSinceChange / 1.0f
			);
		}

		// NOTE:
		// Keeping for now if the no-collisions origin point does not work out.
		//camOriginPoint.z = camCentroidPoint.z + avgPlayerHeight;
		/*
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			auto mountPtr = p->GetCurrentMount();
			camOriginPoint += 
			(
				mountPtr ?
				mountPtr->data.location :
				p->coopActor->data.location
			);
		}

		// Base origin point before processing.
		camOriginPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
		camOriginPoint.z += avgPlayerHeight;
		*/

		if (camCollisions)
		{
			//========================
			//[Modified Origin Point]:
			//========================
			// Set the next origin point by accounting for collisions
			// when moving from the previous origin point to the new base origin point.
			// Want to ensure the origin point is in a valid, reachable position,
			// and not clipping through geometry.
			
			glm::vec4 castStartPoint{ ToVec4(oldCollisionOriginPoint) };
			glm::vec4 castEndPoint{ ToVec4(camOriginPoint) };
			auto result = Raycast::CastRay(castStartPoint, castEndPoint, camAnchorPointHullSize);
			if (result.hit)
			{
				hitToBasePos = true;
			}

			// Get point above ground at the base origin point's XY coords.
			RE::NiPoint3 basePointAboveGround = camOriginPoint;
			bounds = Util::GetVertCollPoints(basePointAboveGround, 0.0f);
			ClampToZCoordAboveLowerBound
			(
				basePointAboveGround.z, 
				minZOffset, 
				camAnchorPointHullSize, 
				bounds.first, 
				bounds.second
			);

			if (AllPlayersVisibleAtPoint(basePointAboveGround, true))
			{
				camCollisionOriginPoint = basePointAboveGround;
			}
		}

		/*
		if (camCollisions)
		{
			//========================
			//[Modified Origin Point]:
			//========================
			// Set the next origin point by accounting for collisions
			// when moving from the previous origin point to the new base origin point.
			// Want to ensure the origin point is in a valid, reachable position,
			// and not clipping through geometry.
			
			glm::vec4 castStartPoint{ ToVec4(oldOriginPoint) };
			glm::vec4 castEndPoint{ ToVec4(camOriginPoint) };
			auto result = Raycast::CastRay(castStartPoint, castEndPoint, camAnchorPointHullSize);
			if (result.hit)
			{
				hitToBasePos = true;
			}

			// Get point above ground at the base origin point's XY coords.
			RE::NiPoint3 basePointAboveGround = camOriginPoint;
			bounds = Util::GetVertCollPoints(basePointAboveGround, 0.0f);
			ClampToZCoordAboveLowerBound
			(
				basePointAboveGround.z, 
				minZOffset, 
				camAnchorPointHullSize, 
				bounds.first, 
				bounds.second
			);

			// Initially, set to base origin point shifted above ground.
			auto camCollisionOriginPoint = basePointAboveGround;
			// If no LOS to a player at the base above-ground position, 
			// move to above-ground raycast collision position.
			bool noPlayersInView = NoPlayersVisibleAtPoint(camCollisionOriginPoint, true); 
			if (noPlayersInView)
			{
				if (result.hit)
				{
					castEndPoint = result.hitPos;
				}

				// Second cast to hit pos or base pos moved above ground.
				bounds = Util::GetVertCollPoints(ToNiPoint3(castEndPoint), 0.0f);
				ClampToZCoordAboveLowerBound
				(
					castEndPoint.z, 
					minZOffset, 
					camAnchorPointHullSize, 
					bounds.first, 
					bounds.second
				);

				auto result = Raycast::CastRay
				(
					castStartPoint, castEndPoint, camAnchorPointHullSize
				);
				if (result.hit)
				{
					hitToCollisionPos = true;
					// Offset away from hit position towards the previous position 
					// to prevent clipping.
					camCollisionOriginPoint = ToNiPoint3
					(
						result.hitPos + 
						(glm::normalize(castStartPoint - castEndPoint)) * 
						min(result.rayLength, camAnchorPointHullSize)
					);
				}
				else
				{
					// No hit, so the previous hit position was unobstructed.
					camCollisionOriginPoint = ToNiPoint3(castEndPoint);
				}
			}

			if (focalPID == -1)
			{
				// Only bound above and below + set min/max anchor point positions
				// when there is a clear path to the next origin position.
				// Clamping bounds during collisions leads to inconsistent shifts
				// to both the lower and upper bounds if the collision point shifts
				// the next origin position up or down, 
				// e.g. riding up a post in one of Solitude's guard towers.
				if (!hitToBasePos && !hitToCollisionPos) 
				{
					bounds = Util::GetVertCollPoints(camCollisionOriginPoint, 0.0f);
					camMaxAnchorPointZCoord = bounds.first;
					camMinAnchorPointZCoord = bounds.second;
				}
				else
				{
					camMaxAnchorPointZCoord =
					camMinAnchorPointZCoord = camCollisionOriginPoint.z;
				}
			}
			else
			{
				// Bound the player focus point above and below.
				bounds = Util::GetVertCollPoints(camRefrFocusPoint, 0.0f);
				camMaxAnchorPointZCoord = bounds.first;
				camMinAnchorPointZCoord = bounds.second;
			}

			//DBG
			//(
			//	"Hit to base: {}, to coll pos: {}, bounds: ({}, {}), "
			//	"collision origin point z: {}, original Z = {}.",
			//	hitToBasePos,
			//	hitToCollisionPos, 
			//	bounds.first,
			//	bounds.second,
			//	camCollisionOriginPoint.z,
			//	camOriginPoint.z
			//);
			camOriginPoint = camCollisionOriginPoint;
		}
		*/

		if (Settings::bOriginPointSmoothing)
		{
			camOriginPoint.x = Util::InterpolateSmootherStep
			(
				oldOriginPoint.x, camOriginPoint.x, camInterpFactor * originPointPIDChangeMult
			);
			camOriginPoint.y = Util::InterpolateSmootherStep
			(
				oldOriginPoint.y, camOriginPoint.y, camInterpFactor * originPointPIDChangeMult
			);
			camOriginPoint.z = Util::InterpolateSmootherStep
			(
				oldOriginPoint.z, camOriginPoint.z, camInterpFactor * originPointPIDChangeMult
			);
		}

		// Save centroid point delta for auto pitch adjustments.
		auto oldDelta = camCentroidPointDelta;
		camCentroidPointDelta = camCentroidPoint - oldCentroidPoint;
	}

	void CameraManager::CalcNextTargetPosition()
	{
		// Calculate the next position to place the camera at,
		// and other target position points which are used for raycasting.

		auto lastSetCamTargetPos = camTargetPos;
		auto lastSetCamCollisionTargetPos = camCollisionTargetPos;
		if (isManuallyPositioned)
		{
			camBaseTargetPos = lastSetCamTargetPos;
			if (IsAdjustingZoom())
			{
				const auto& rsData = glob.cdh->GetAnalogStickState
				(
					glob.coopPlayers[adjustingCamPID]->deviceID, false
				);
				const auto& rsX = rsData.xComp;
				const auto& rsY = rsData.yComp;
				const auto& rsMag = rsData.normMag;
				// Horizontal RS movements move the camera left or right
				// on the XY plane with normal vector perpendicular to the
				// camera's pitch direction.

				if (rsMag > 0.0f)
				{
					auto rsZAngle = atan2f(rsY, rsX);
					// To game coords before adding cam yaw.
					rsZAngle = Util::ConvertAngle(Util::NormalizeAngTo2Pi(rsZAngle));
					rsZAngle = Util::NormalizeAngTo2Pi(camYaw + rsZAngle);
					// Convert back before calculating target position components.
					rsZAngle = Util::ConvertAngle(rsZAngle);
					RE::NiPoint3 targetPosMovementOffset = RE::NiPoint3();
					targetPosMovementOffset.x += cosf(-camTargetPosPitch) * cosf(rsZAngle);
					targetPosMovementOffset.y += cosf(-camTargetPosPitch) * sinf(rsZAngle);
					targetPosMovementOffset.z += 
					(
						sinf(-camTargetPosPitch) * std::clamp(rsY, -1.0f, 1.0f)
					);
					targetPosMovementOffset.Unitize();
					targetPosMovementOffset *= 
					(
						camManualPosMaxMovementSpeed * *g_deltaTimeRealTime * rsMag
					);

					// Base pos set before checking for collisions.
					camBaseTargetPos = lastSetCamTargetPos + targetPosMovementOffset;
					auto newTargetPos = camBaseTargetPos;

					// Prevent camera from clipping into objects/surfaces 
					// by setting the new target position
					// to the movement direction raycast hit position, if any.
					if (Settings::bTargetPosSmoothing) 
					{
						newTargetPos.x = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, newTargetPos.x, camInterpFactor
						);
						newTargetPos.y = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y, newTargetPos.y, camInterpFactor
						);
						newTargetPos.z = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z, newTargetPos.z, camInterpFactor
						);
					}

					camCollisionTargetPos = newTargetPos;
					if (camCollisions)
					{
						Raycast::RayResult movementHitResult = Raycast::CastRay
						(
							ToVec4(lastSetCamTargetPos), ToVec4(newTargetPos), 10.0f
						);
						if (movementHitResult.hit)
						{
							camCollisionTargetPos = ToNiPoint3
							(
								movementHitResult.hitPos + movementHitResult.rayNormal * 10.1f
							);
						}
					}
				}

				if (camCollisions)
				{
					camTargetPos = camCollisionTargetPos;
				}
				else
				{
					if (Settings::bTargetPosSmoothing) 
					{
						camTargetPos.x = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, camBaseTargetPos.x, camInterpFactor
						);
						camTargetPos.y = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y, camBaseTargetPos.y, camInterpFactor
						);
						camTargetPos.z = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z, camBaseTargetPos.z, camInterpFactor
						);
					}
					else
					{
						camTargetPos = camBaseTargetPos;
					}
				}
			}
		}
		else
		{
			// Calculate base target position first.
			// Using spherical coordinates.
			float r = camTargetRadialDistance;
			float phi = Util::ConvertAngle(camTargetPosYaw);
			float theta = PI / 2.0f + camTargetPosPitch;
			
			// 15 frames to rotate quickly and face the target.
			const bool positionQuicklyToFaceSpeaker = 
			(
				inDialogueCamState && 
				Settings::bDialogueCamEnabled && 
				Settings::bDialogueCamSwitchSpeakers &&
				!movingToDialogueStartPos && 
				Util::GetElapsedSeconds(dialogueSpeakerChangedTP) <= 
				15.0f * *g_deltaTimeRealTime
			);
			bool inDialogue = 
			(
				inDialogueCamState && 
				Settings::bDialogueCamEnabled &&
				Util::HandleIsValid(camDialogueTargetHandle) &&
				glob.menuPID >= 0
			);
			if (inDialogue)
			{
				// Position is based off the listener's head position 
				// and focuses on the speaker's torso.
				const auto subtitleManager = RE::SubtitleManager::GetSingleton();
				const auto& dialogueP = glob.coopPlayers[glob.menuPID];
				const auto dialogueTargetPtr = camDialogueTargetHandle.get();
				const auto asActor = dialogueTargetPtr->As<RE::Actor>();
				const bool targetIsSpeaking = 
				(
					!Util::HandleIsValid(camDialogueSpeakerHandle) ||
					camDialogueSpeakerHandle == camDialogueTargetHandle
				);
				auto speakerPos = RE::NiPoint3();
				auto listenerToSpeakerDir = RE::NiPoint3();
				// Always focus on the dialogue target NPC if not switching positions
				// to track the speaker.
				if (targetIsSpeaking || !Settings::bDialogueCamSwitchSpeakers)
				{
					speakerPos = 
					(
						dialogueTargetPtr->As<RE::Actor>() ? 
						Util::GetHeadPosition(dialogueTargetPtr->As<RE::Actor>()) : 
						//Util::GetTorsoPosition(dialogueTargetPtr->As<RE::Actor>()) : 
						Util::GetRefrPosition(dialogueTargetPtr.get())
					);
					listenerToSpeakerDir = Util::RotationToDirectionVect
					(
						0.0f, Util::ConvertAngle
						(
							Util::GetYawBetweenPositions
							(
								dialogueP->coopActor->data.location, speakerPos
							)
						)
					);
				}
				else
				{
					speakerPos = Util::GetHeadPosition(dialogueP->coopActor.get());
					//dialogueP->mm->playerTorsoPosition;
					//Util::GetTorsoPosition(dialogueP->coopActor.get());
					listenerToSpeakerDir = Util::RotationToDirectionVect
					(
						0.0f, Util::ConvertAngle
						(
							Util::GetYawBetweenPositions
							(
								dialogueTargetPtr->data.location, speakerPos
							)
						)
					);
				}
				
				float xyDistToTarget = Util::GetXYDistance(camTargetPos, speakerPos);
				// Default radius at which to start slowing rotation.
				float radius = min
				(
					Settings::fTargetAttackSourceDistToSlowRotation, 
					Settings::fAutoEndDialogueRadius
				);
				// Slow down when within a multiple of the player actor's bounds.
				auto player3DPtr = Util::GetRefr3D(dialogueP->coopActor.get()); 
				if (player3DPtr &&
					!isnan(player3DPtr->worldBound.radius) &&
					!isinf(player3DPtr->worldBound.radius) &&
					!isnan(player3DPtr->worldBound.center.Length()) &&
					!isinf(player3DPtr->worldBound.center.Length()))
				{
					radius = player3DPtr->worldBound.radius * 4.0f;
				}
				else
				{
					DBG("ERR: {}'s worldbound is corrupted.", dialogueP->coopActor->GetName());
				}

				float dirYawDiff = 0.0f;
				if (targetIsSpeaking || !Settings::bDialogueCamSwitchSpeakers)
				{
					dirYawDiff = 
					(
						dialogueP->analogStickParams[!AnalogStickParams::kLSWorldAng] - 
						Util::GetYawBetweenPositions
						(
							dialogueP->coopActor->data.location, speakerPos
						)
					);
				}
				else
				{
					dirYawDiff = 
					(
						dialogueP->analogStickParams[!AnalogStickParams::kLSWorldAng] - 
						Util::GetYawBetweenPositions
						(
							dialogueTargetPtr->data.location, speakerPos
						)
					);
				}

				bool prevOffsetRight = shoulderOffsetRight;
				shoulderOffsetRight = Util::NormalizeAng0To2Pi(dirYawDiff) <= PI;
				if (shoulderOffsetRight == prevOffsetRight)
				{
					shoulderOffsetMaintainedTP = SteadyClock::now();
				}
				else
				{
					shoulderOffsetChangedTP = SteadyClock::now();
				}

				auto playerHeadingDir = Util::RotationToDirectionVect
				(
					0.0f, Util::ConvertAngle
					(
						dialogueP->analogStickParams[!AnalogStickParams::kLSWorldAng]
					)
				);
				camTargetXYOffset = ToNiPoint2
				(
					playerHeadingDir - 
					(playerHeadingDir.Dot(listenerToSpeakerDir) * listenerToSpeakerDir)
				);
				// Offset further away in the direction of the player's movement 
				// to give better visibility of the speaher.
				// Done when first positioning the camera or when not moving directly 
				// towards/away from the NPC because the shoulder switch would happen constantly 
				// while moving otherwise.
				if (!dialogueP->lsMoved)
				{
					camTargetXYOffset.Unitize();
				}

				camTargetXYOffset *= 
				(
					adjustedAfterReachingDialoguePos ? 
					Settings::fDialogueCamZoomedInMaxHorizontalOffset * 1.5f : 
					Settings::fDialogueCamZoomedInMaxHorizontalOffset
				);

				camXYOffset.x = Util::InterpolateSmootherStep
				(
					camXYOffset.x, 
					camTargetXYOffset.x,
					0.5f * camInterpFactor
				);
				camXYOffset.y = Util::InterpolateSmootherStep
				(
					camXYOffset.y, 
					camTargetXYOffset.y,
					0.5f * camInterpFactor
				);

				if (targetIsSpeaking || !Settings::bDialogueCamSwitchSpeakers)
				{
					camRefrFocusPoint =
					(
						dialogueP->coopActor->data.location +
						RE::NiPoint3
						(
							camXYOffset.x,
							camXYOffset.y,
							dialogueP->coopActor->IsSneaking() ?
							0.5f * dialogueP->coopActor->GetHeight() :
							dialogueP->coopActor->GetHeight()
						)	
					);
				}
				else
				{
					camRefrFocusPoint =
					(
						dialogueTargetPtr->data.location +
						RE::NiPoint3
						(
							camXYOffset.x,
							camXYOffset.y,
							asActor && asActor->IsSneaking() ?
							0.5f * asActor->GetHeight() :
							asActor ? 
							asActor->GetHeight() :
							dialogueTargetPtr->GetHeight()
						)	
					);
				}

				auto prevBaseTargetPos = camBaseTargetPos;
				camBaseTargetPos = 
				(
					camRefrFocusPoint + 
					(
						adjustedAfterReachingDialoguePos ?
						RE::NiPoint3(0.0f, 0.0f, camHeightOffset) :
						RE::NiPoint3(0.0f, 0.0f, Settings::fDialogueCamZoomedInVerticalOffset)
					)
				);
				
				float phi = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(camBaseTargetPos, speakerPos)
				);
				float theta = 
				(
					PI / 2.0f + Util::GetPitchBetweenPositions(camBaseTargetPos, speakerPos)
				);
				camBaseTargetPos.z -= r * cosf(theta);
				camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
				camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
				
				// Smooth out movement to the next base target position.
				float tRatio = camInterpFactor;
				if (positionQuicklyToFaceSpeaker)
				{
					// Jump right to the base target position if not using transitional smoothing.
					tRatio = 
					(
						Settings::bDialogueCamFocusSwitchSmoothing ? 
						1.0f - powf(0.95f, 60.0f * *g_deltaTimeRealTime) : 1.0f
					);
				}
				else
				{
					// Since positional data is noisy when based off the player's movement angle,
					// interpolate between the previous and next base target positions
					// when not switching speakers.
					tRatio = 
					(
						Util::InterpolateEaseOut
						(
							0.0f, 1.0f, min(1.0f, xyDistToTarget / max(0.01f, radius)), 9.0f
						) *
						(
							movingToDialogueStartPos ? 
							min
							(
								1.0f,
								Util::GetElapsedSeconds(dialogueCameraTP) / 
								secsCamDialogueStartTransition
							) :
							0.8f * camInterpFactor
						)
					);
				}
				
				camBaseTargetPos.x = Util::InterpolateSmootherStep
				(
					prevBaseTargetPos.x, camBaseTargetPos.x, tRatio
				);
				camBaseTargetPos.y = Util::InterpolateSmootherStep
				(
					prevBaseTargetPos.y, camBaseTargetPos.y, tRatio
				);
				camBaseTargetPos.z = Util::InterpolateSmootherStep
				(
					prevBaseTargetPos.z, camBaseTargetPos.z, tRatio
				);
			}
			else
			{
				if (focalPID == -1) 
				{
					// Base target position is offset from the base focus position,
					// and is not guaranteed to be a reachable spot.
					// If turning to face the party focus point, originate the target position
					// from the lock on target's focus point instead.
					camBaseTargetPos = 
					(
						isLockedOn && 
						Util::HandleIsValid(camLockOnTargetHandle) &&
						Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom ? 
						camLockOnFocusPoint :
						camFocusPoint
					);
					// Pitch adjusts the target Z coordinate if camera collisions are on
					// or if the lock on assistance level is set to zoom.
					// Otherwise, the focus point solely determines the target Z coordinate
					// to prevent the camera from phasing through the ground as much 
					// since collisions are off.
					bool pitchAdjustsZCoordinate = 
					(
						camCollisions ||
						!isLockedOn ||
						!Util::HandleIsValid(camLockOnTargetHandle) ||
						Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom	
					);
					if (pitchAdjustsZCoordinate)
					{
						camBaseTargetPos.z -= r * cosf(theta);
					}

					camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
					camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
				}
				else
				{
					// Base target position is the focal player's focus point,
					// which is almmost guaranteed to be valid,
					// since it is offset from the player's position.
					const auto& focalP = glob.coopPlayers[focalPID];
					camRefrFocusPoint = focalP->mm->coopActor->data.location;
					if (!focalP->coopActor->IsOnMount())
					{
						camRefrFocusPoint += RE::NiPoint3
						(
							0.0f,
							0.0f,
							focalP->coopActor->IsSneaking() ?
							0.5f * focalP->coopActor->GetHeight() :
							focalP->coopActor->GetHeight()
						);
					}

					camBaseTargetPos = 
					(
						camRefrFocusPoint + RE::NiPoint3(0.0f, 0.0f, camHeightOffset)
					);
					camBaseTargetPos.z -= r * cosf(theta);
					camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
					camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);

					// Tack on shoulder XY offset.
					float playerFacingYaw = camYaw;
					if (Util::HandleIsValid(focalP->tm->crosshairRefrHandle) &&
						focalP->tm->crosshairRefrHandle == 
						focalP->tm->targetMotionState->targetRefrHandle)
					{
						playerFacingYaw = Util::GetYawBetweenPositions
						(
							focalP->tm->playerMotionState->cPos, focalP->tm->targetMotionState->cPos
						);
					}
					
					camTargetXYOffset = ToNiPoint2
					(
						Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle
							(
								shoulderOffsetRight ? 
								Util::NormalizeAng0To2Pi(playerFacingYaw + PI / 2.0f) :
								Util::NormalizeAng0To2Pi(playerFacingYaw - PI / 2.0f)
							)
						)
					);
					camTargetXYOffset *= 
					(
						Settings::fFocalCamBaseHorizontalOffset + 
						focalP->tm->playerMotionState->cVel.Length() / 20.0f
					);
					camXYOffset.x = Util::InterpolateSmootherStep
					(
						camXYOffset.x, 
						camTargetXYOffset.x,
						std::lerp
						(
							0.5f * camInterpFactor,
							1.0f,
							min(1.0f, Util::GetElapsedSeconds(shoulderOffsetChangedTP) / 1.0f)
						)
					);
					camXYOffset.y = Util::InterpolateSmootherStep
					(
						camXYOffset.y, 
						camTargetXYOffset.y,
						std::lerp
						(
							0.5f * camInterpFactor,
							1.0f,
							min(1.0f, Util::GetElapsedSeconds(shoulderOffsetChangedTP) / 1.0f)
						)
					);

					camBaseTargetPos.x += camXYOffset.x;
					camBaseTargetPos.y += camXYOffset.y;
					camBaseTargetPos.z += Settings::fFocalCamBaseVerticalOffset;
				}
			}
			
			// Focus point from which the target position is based.
			const RE::NiPoint3& focusPoint = 
			(
				focalPID == -1 && !inDialogue ? camCollisionOriginPoint : camRefrFocusPoint
			);
			if (camCollisions && !movingToDialogueStartPos)
			{
				// [(Questionable?) Methods to the madness below]:
				// 
				// To ensure that we have a valid target position that isn't out of bounds
				// or in an area that no player can reach, we need a valid starting position
				// to raycast to our base target position, which could also be invalid.
				// 
				// We use player-to-target LOS hit positions as the base positions 
				// for determining the next target postiion, since these are always valid positions 
				// (because they are offset from the players' positions themselves).

				// No matter what, players should try to stay close together as much as possible
				// for the smoothest experience when using the co-op camera.

				// Raycast result from the previous position to the current base position.
				// If there's a hit, this means that the camera will hit a surface 
				// if it moves directly to the base target position.
				Raycast::RayResult movementResult = Raycast::CastRay
				(
					ToVec4(lastSetCamTargetPos), ToVec4(camBaseTargetPos), camTargetPosHullSize
				);
				/*if (movementResult.hit)
				{
					DBG("BRANCH: Colliding to next base pos.");
				}*/
			
				//=================================================================================
				// [Camera Collision Position]:
				//=================================================================================
				// Set (hopefully) to a position that is reachable 
				// and not outside the world geometry.

				// Basic system to minimize camera jumping and maximize visibility of all players:
				// 1. Check for visibility of the base target position from the focus point 
				// and all active players' focus points.
				// Use two raycasts per focus point to check visibility.
				// 2. If the hull result does not hit or if the hit position 
				// is close to the start position and the zero-hull raycast does not hit, 
				// the base target position is valid and visible. 
				// The reason for the distance check from the hull cast hit position 
				// is to prevent the target position from jumping forward to the focus point 
				// unless there is an obstruction to the base target position. 
				// Example: All active players are within a hull size from a wall, 
				// which causes the hull casts starting from their focus points 
				// to hit the wall right away. However, the zero-hull cast will not hit the wall,
				// unless all players have their focus points clipping through the wall, 
				// which shouldn't happen.
				// The next target position is then set to the base target position, 
				// instead of one of the wall-hit positions, since the base target position 
				// is still reachable from the players' focus points.
				// 3. Otherwise, for the next target position, 
				// choose the hull cast hit point that is closest to the previous target position,
				// which will minimize camera jumping. 
				// The hull cast hit position is adjusted to avoid clipping into geometry
				// and is always at a valid, reachable position. 
				// Unless offsetting via the hit's normal vector causes the adjusted position 
				// to lie within the collided surface. But that's outside our control.

				// Sources of stuttering:
				// 1. Raycast hit results from different start positions 
				// changing from hit to no hit recorded and vice versa on a frame-to-frame basis. 
				// 2. Raycast hit normal changing rapidly from one frame to the next,
				// even when cast from almost the same start and end positions.
				// 3. Auto-zoom consistency problems, 
				// leading to a rapidly varying target radial distance,
				// and thus, a rapidly changing base camera target position.
				
				// For debugging.
				int32_t closestIndex = -1337;

				const glm::vec4 baseTargetPos = ToVec4(camBaseTargetPos);
				const glm::vec4 lastSetTargetPos = ToVec4(lastSetCamTargetPos);
				glm::vec4 closestHitPos = lastSetTargetPos;
				// Offset from the camera collision focus point,
				// which should be within the traversable part of the world.
				glm::vec4 castStartPos = ToVec4(focusPoint);
				// Raycast result.
				// Raycast hit position adjusted to avoid hit geometry.
				glm::vec4 adjHitResultPos{ };
				// Normalized reversed direction of the raycast.
				glm::vec4 endToStartDir{ };
				// Distance from hit position to the last set target position.
				float dist = 0.0f; 
				// Save raycast hit position distance to target position for comparison purposes.
				float closestDist = FLT_MAX;
				// Hull raycast hit result.
				// From focus point first.
				auto result = Raycast::CastRay
				(
					castStartPos, baseTargetPos, camTargetPosHullSize
				);
				if (glm::length(baseTargetPos - castStartPos) <= 1e-5f)
				{
					endToStartDir = glm::vec4();
				}
				else
				{
					endToStartDir = glm::normalize(baseTargetPos - castStartPos);
				}
				
				// Check if at least one player is in view at the base target position.
				if (!NoPlayersVisibleAtPoint(camBaseTargetPos, true))
				{
					playerInViewTP = SteadyClock::now();
				}

				bool baseTargetPosVisible = !result.hit;
				if (focalPID != -1 || inDialogue)
				{
					//DBG("BRANCH: Focal player or in dialogue.");
					// Set directly to offset hit position if there is a focal player
					// or in dialogue.
					// Not necessary to raycast for visibility from the other active players.
					if (baseTargetPosVisible)
					{
						closestHitPos = baseTargetPos;
					}
					else
					{
						closestHitPos = result.hitPos + result.rayNormal * camTargetPosHullSize;
					}
				}
				else if (baseTargetPosVisible && !movementResult.hit)
				{
					//DBG("BRANCH: Base pos visible from focus.");
					// Camera is not colliding with a surface when moving to the base target pos
					// and it is visible from the focus point, so we can move it there directly.
					// NOTE:
					// I'm aware that the focus point may not be a reachable, valid position,
					// which can lead to the camera moving to a position inside geometry here.
					// But the tradeoff is having fewer jumps from near one player to another
					// when they move far apart and the focus point remains in a reachable spot.
					closestHitPos = baseTargetPos;
				}
				else
				{
					// Now cast from each player's focus point
					// to check for a closer hit position.
					// We have an obstruction to the base target position,
					// so adjust the hit position away from the obstruction now.
					// For debugging.
					closestIndex = 1337;

					// Commented out for now because the closest hit position from the focus point
					// may sit in an out-of-bounds position.
					if (softFocalPID == -1)
					{
						// Only allow the focus point cast's hit result position to serve
						// as the new target position if there is no soft focal player 
						// because the camera can easily snag onto a surface between players 
						// near the focus point, like the ceiling between two different floors, 
						// and block both players from view.
						adjHitResultPos =
						(
							result.hitPos +
							result.rayNormal *
							//endToStartDir *
							//(result.rayNormal + endToStartDir) *
							min(result.rayLength, camTargetPosHullSize)
						);
						// Set the initial closest distance to the previous target position.
						closestDist = glm::distance(adjHitResultPos, lastSetTargetPos); 
						closestHitPos = adjHitResultPos;
					}
					
					// Check from refr target's focus point if in dialogue or locked on
					// and not facing the target, since the camera is then following the target.
					// Reason we skip this check in zoom mode while facing the target
					// is we don't want to choose a target position that is only visible 
					// from the lock on target because this means the players aren't visible
					// at that position.
					// Ex. Wall between the players and the lock on target, 
					// but the target position is behind the lock on target.
					bool checkFromLockOnTarget = 
					(
						ShouldConsiderCamTargetAsPlayer() && !facingLockOnTarget
					);

					// Now cast from each player's focus point
					// to check for a closer hit position.
					for (const auto& p : glob.coopPlayers)
					{
						if (!p->isActive)
						{
							continue;
						}

						castStartPos = ToVec4(Util::GetActorFocusPoint(p->coopActor.get()));
						result = Raycast::CastRay
						(
							castStartPos, baseTargetPos, camTargetPosHullSize
						);
						if (glm::length(baseTargetPos - castStartPos) <= 1e-5f)
						{
							endToStartDir = glm::vec4();
						}
						else
						{
							endToStartDir = glm::normalize(baseTargetPos - castStartPos);
						}

						// If the player's focus point is too close to or is in an object,
						// back the cast starting position away from the object 
						// before casting again towards the target position.
						if (result.hit && 
							glm::distance(result.hitPos, castStartPos) <= camTargetPosHullSize)
						{
							castStartPos = 
							(
								result.hitPos +
								result.rayNormal *
								camTargetPosHullSize
							);
							result = Raycast::CastRay
							(
								castStartPos, baseTargetPos, camTargetPosHullSize
							);
						}

						baseTargetPosVisible = !result.hit;

						// Stop casting if there is no hit, 
						// and therefore no obstruction, from a cast.
						if (baseTargetPosVisible)
						{
							/*DBG("BRANCH: Base pos visible from {}'s focus.", 
								p->coopActor->GetName());*/
							closestIndex = -p->playerID;
							closestHitPos = baseTargetPos;
							break;
						}
						else if (p->playerID != softFocalPID && !checkFromLockOnTarget)
						{
							/*DBG("BRANCH: Raycast hit pos from {}'s focus.", 
								p->coopActor->GetName());*/
							closestIndex = p->playerID;
							adjHitResultPos =
							(
								result.hitPos +
								result.rayNormal *
								camTargetPosHullSize
							);
							// Check for update to the closest hit position again.
							dist = glm::distance(adjHitResultPos, lastSetTargetPos); 
							if (dist < closestDist)
							{
								closestDist = dist;
								closestHitPos = adjHitResultPos;
							}
						}
					}

					if (checkFromLockOnTarget && !baseTargetPosVisible)
					{
						castStartPos = ToVec4
						(
							inDialogueCamState ? 
							Util::GetRefrPosition(camDialogueTargetHandle.get().get()) : 
							Util::GetActorFocusPoint
							(
								camLockOnTargetHandle.get()->As<RE::Actor>()
							)
						);
						result = Raycast::CastRay
						(
							castStartPos, baseTargetPos, camTargetPosHullSize
						);
						if (glm::length(baseTargetPos - castStartPos) <= 1e-5f)
						{
							endToStartDir = glm::vec4();
						}
						else
						{
							endToStartDir = glm::normalize(baseTargetPos - castStartPos);
						}
						
						// If the player's focus point is too close to or is in an object,
						// back the cast starting position away from the object 
						// before casting again towards the target position.
						if (result.hit && 
							glm::distance(result.hitPos, castStartPos) <= camTargetPosHullSize)
						{
							castStartPos = 
							(
								result.hitPos +
								result.rayNormal *
								camTargetPosHullSize
							);
							result = Raycast::CastRay
							(
								castStartPos, baseTargetPos, camTargetPosHullSize
							);
						}

						baseTargetPosVisible = !result.hit;

						// Stop casting if there is no hit, 
						// and therefore no obstruction, from a cast.
						if (baseTargetPosVisible)
						{
							closestIndex = -69420;
							closestHitPos = baseTargetPos;
						}
						else if (softFocalPID == -1 &&
								 Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
						{
							// Also allow the lock-on target to determine LOS 
							// on the base target position if the lock-on assistance level 
							// is set to zoom. 
							// Will find more instances where the base target position 
							// is in traversible space.
							closestIndex = 69420;
							adjHitResultPos =
							(
								result.hitPos +
								result.rayNormal *
								camTargetPosHullSize
							);
							// Check for update to the closest hit position again.
							dist = glm::distance(adjHitResultPos, lastSetTargetPos); 
							if (dist < closestDist)
							{
								closestDist = dist;
								closestHitPos = adjHitResultPos;
							}
						}
					}
				}
				
				const bool isAdjustingCam = IsAdjustingCamera();
				if ((!baseTargetPosVisible && !movementResult.hit) && 
					(isAdjustingCam || Util::GetElapsedSeconds(playerInViewTP) <= 0.5f))
				{
					// Even if the base target position is not visible from the focus point.
					// stay back instead of jumping forward if less than half a second
					// has elapsed since at least one player was visible.
					// 
					// Prevents excessive jumps forward and back again when players pass by
					// thin obstacles like trees while the camera is zoomed out.
					// 
					//DBG("BRANCH: Stay back in valid position.");
					closestHitPos = baseTargetPos;
				}

				// Set next collision target position and set colliding flag.
				camCollisionTargetPos = ToNiPoint3(closestHitPos);

				// Not colliding if the collision target position 
				// is the same as the base target position and there is no movement hit.
				if (camCollisionTargetPos == camBaseTargetPos && !movementResult.hit) 
				{
					//DBG("BRANCH: Not colliding after calculating collision pos.");
					isColliding = false;
				}
				else
				{
					//DBG("BRANCH: Colliding after calculating collision pos.");
					isColliding = true;
				}

				// Apply smoothing if enabled.
				// NOTE: 
				// Camera can still phase through surfaces 
				// when transitioning from the last set position (can be OOB) 
				// to the target position (not OOB) 
				// since the interpolated position is between the two positions. 
				// Only jumping instantly to the target position will prevent this from occurring,
				// but obviously, this is more jarring.
				if (Settings::bTargetPosSmoothing && !positionQuicklyToFaceSpeaker)
				{
					camCollisionTargetPos = 
					{
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, 
							camCollisionTargetPos.x, 
							camInterpFactor
						),
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y,
							camCollisionTargetPos.y,
							camInterpFactor
						),
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z,
							camCollisionTargetPos.z,
							camInterpFactor
						)
					};
				}

				camTargetPos = camCollisionTargetPos;
			}
			else
			{
				isColliding = false;
				if (Settings::bTargetPosSmoothing && !positionQuicklyToFaceSpeaker)
				{
					camTargetPos.x = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.x, camBaseTargetPos.x, camInterpFactor
					);
					camTargetPos.y = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.y, camBaseTargetPos.y, camInterpFactor
					);
					camTargetPos.z = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.z, camBaseTargetPos.z, camInterpFactor
					);
				}
				else
				{
					camTargetPos = camBaseTargetPos;
				}

				camCollisionTargetPos = camTargetPos;
			}
			
			// Save the final target position's radial distance for zoom calculations later.
			camTrueRadialDistance = camTargetPos.GetDistance(focusPoint);
		}
	}

	void CameraManager::CheckLockOnTarget()
	{
		// Set a new lock-on target if there is a valid request,
		// or check if the current target is valid, 
		// clearing out invalid targets as needed.

		auto camTargetPtr = Util::GetRefrPtrFromHandle
		(
			inDialogueCamState ? 
			camDialogueTargetHandle :
			camLockOnTargetHandle
		);
		const auto asActor = camTargetPtr ? camTargetPtr->As<RE::Actor>() : nullptr;
		bool validLockOnTarget = static_cast<bool>(camTargetPtr);
		if (isLockedOn || inDialogueCamState)
		{
			// Check if target is still valid (in LOS, 3D loaded, handle valid, etc.)
			RE::NiPoint3 oldCamLockOnFocusPoint = camLockOnFocusPoint;
			const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(camTargetPtr);
			// Check if the lock-on target is a player, and if so, 
			// the target is only valid if not downed.
			// Lock on targets must be actors.
			if (isLockedOn)
			{
				validLockOnTarget &= 
				(
					(asActor) && (pIndex == -1 || !glob.coopPlayers[pIndex]->isDowned)
				);
			}
			
			if (validLockOnTarget)
			{
				if (isLockedOn)
				{
					secsSinceLockOnTargetLOSChecked = Util::GetElapsedSeconds(lockOnLOSCheckTP);
					if (secsSinceLockOnTargetLOSChecked > 
						Settings::fSecsBetweenTargetVisibilityChecks)
					{
						lockOnLOSCheckTP = SteadyClock::now();
						bool hadLOS = lockOnTargetInSight;
						bool inFrustum = false;
						auto p1 = RE::PlayerCharacter::GetSingleton();
						// Use P1's LOS check.
						lockOnTargetInSight = 
						(
							p1 && 
							p1->HasLineOfSight(camTargetPtr.get(), inFrustum)
						);
						bool lostLOS = camTargetPtr && hadLOS && !lockOnTargetInSight;
						bool noLOS = camTargetPtr && !lockOnTargetInSight;
						bool regainedLOS = camTargetPtr && !hadLOS && lockOnTargetInSight;
						if (lostLOS)
						{
							lockOnLOSLostTP = SteadyClock::now();
						}
						else if (regainedLOS)
						{
							secsSinceLockOnTargetLOSLost = 0.0f;
						}
						else if (noLOS)
						{
							secsSinceLockOnTargetLOSLost = Util::GetElapsedSeconds(lockOnLOSLostTP);
						}
					}

					// Clear out after not having LOS for a certain amount of time.
					// Do not invalidate if in dialogue with the target NPC.
					bool invalidateAfterNoLOS = 
					(
						secsSinceLockOnTargetLOSLost > Settings::fSecsWithoutLOSToInvalidateTarget
					);
					if (invalidateAfterNoLOS || 
						camTargetPtr->IsDead() ||
						!camTargetPtr->Is3DLoaded() || 
						!camTargetPtr->IsHandleValid() || 
						!camTargetPtr->GetParentCell() || 
						!camTargetPtr->GetParentCell()->IsAttached())
					{
						// Reset LOS lost interval since the target is not valid.
						if (invalidateAfterNoLOS)
						{
							secsSinceLockOnTargetLOSLost = 0.0f;
						}

						validLockOnTarget = false;
					}
					else
					{
						// Crosshair refr is valid, so we can update lock-on pos.
						camLockOnFocusPoint = 
						(
							facingLockOnTarget ? 
							Util::GetTorsoPosition(asActor) :
							Util::GetHeadPosition(asActor) + 
							RE::NiPoint3(0.0f, 0.0f, 0.5f * asActor->GetHeight())
						);
					}
				}
				else
				{
					if (!camTargetPtr->Is3DLoaded() || 
						!camTargetPtr->IsHandleValid() || 
						!camTargetPtr->GetParentCell() || 
						!camTargetPtr->GetParentCell()->IsAttached())
					{
						validLockOnTarget = false;
					}
					else
					{
						// Crosshair refr is valid, so we can update lock-on pos.
						camLockOnFocusPoint = 
						(
							asActor ?
							Util::GetTorsoPosition(asActor) :
							Util::GetRefrPosition(camTargetPtr.get())
						);
					}
				}

				// Add height offset to focus point if in zoom mode 
				// to offset the point along the target's vertical axis.
				if (Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
				{
					camLockOnFocusPoint += RE::NiPoint3(0.0f, 0.0f, camHeightOffset);
				}
			}

			if (validLockOnTarget)
			{
				// Draw lock-on indicator above the target's head.
				RE::NiPoint3 lockOnIndicatorCenter
				{
					asActor ? 
					Util::WorldToScreenPoint3
					(
						Util::GetTorsoPosition(asActor)
					) :
					Util::WorldToScreenPoint3
					(
						Util::GetRefrPosition(camTargetPtr.get()) + 
						RE::NiPoint3(0.0f, 0.0f, camTargetPtr->GetHeight())
					) 
				};

				DrawLockOnIndicator(lockOnIndicatorCenter.x, lockOnIndicatorCenter.y);

				// Smooth out tracking of lock-on target.
				camLockOnFocusPoint.x = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.x, camLockOnFocusPoint.x, camInterpFactor
				);
				camLockOnFocusPoint.y = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.y, camLockOnFocusPoint.y, camInterpFactor
				);
				camLockOnFocusPoint.z = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.z, camLockOnFocusPoint.z, camInterpFactor
				);
			}
		}

		// Clear lock-on data if the lock-on target is invalid or
		// not in lock-on mode but a lock-on target is still set.
		if ((camLockOnTargetHandle) && (!isLockedOn || !validLockOnTarget))
		{
			ClearLockOnData();
		}
	}

	void CameraManager::DrawLockOnIndicator(const float& a_centerX, const float& a_centerY)
	{
		// Draw the lock-on marker on the camera's lock-on target.

		if (focalPID != -1)
		{
			return;
		}

		auto camTargetPtr = Util::GetRefrPtrFromHandle
		(
			isLockedOn ? 
			camLockOnTargetHandle :
			camDialogueTargetHandle
		);
		if (!camTargetPtr) 
		{
			return;
		}

		// Do not draw if zoomed in while in dialogue or if not requesting to draw the indicator
		// while zoomed out.
		bool skipDrawingInDialogue = false;
		if (inDialogueCamState)
		{
			if (Settings::bDialogueCamEnabled)
			{
				skipDrawingInDialogue = 
				(
					!adjustedAfterReachingDialoguePos || 
					!Settings::bDialogueCamZoomedOutSpeakerIndicator
				);
			}
			else
			{
				skipDrawingInDialogue = !Settings::bDialogueCamZoomedOutSpeakerIndicator;
			}
		}

		if (skipDrawingInDialogue)
		{
			return;
		}

		float indicatorBaseLength = Settings::fCamLockOnIndicatorLength / 2.0f;
		const float& indicatorBaseThickness = Settings::fCamLockOnIndicatorThickness;
		float targetPixelHeight = Util::GetBoundPixelDist(camTargetPtr.get(), true);
		targetPixelHeight = targetPixelHeight == 0.0f ? indicatorBaseLength : targetPixelHeight;
		// Scale with target's pixel height and bound above and below.
		indicatorBaseLength = std::clamp
		(
			indicatorBaseLength,
			min
			(
				indicatorBaseLength,
				targetPixelHeight / 4.0f
			),
			max
			(
				indicatorBaseLength,
				targetPixelHeight / 4.0f
			)
		);

		// Cap the radius and modify thickness.
		float radius = indicatorBaseLength;
		const float thickness = min(0.2f * radius, indicatorBaseThickness);
		const auto center = glm::vec3(a_centerX, a_centerY, 0.0f);
		float gapDelta = 0.0f;
		// Animate for better visibility.
		if ((lockOnIndicatorOscillationInterpData->interpToMax &&
			lockOnIndicatorOscillationInterpData->value != 1.0f) ||
			(lockOnIndicatorOscillationInterpData->interpToMin && 
			lockOnIndicatorOscillationInterpData->value != 0.0f))
		{
			lockOnIndicatorOscillationInterpData->UpdateInterpolatedValue
			(
				lockOnIndicatorOscillationInterpData->directionChangeFlag
			);
		}
		else
		{
			lockOnIndicatorOscillationInterpData->UpdateInterpolatedValue
			(
				!lockOnIndicatorOscillationInterpData->directionChangeFlag
			);
		}

		gapDelta = (lockOnIndicatorOscillationInterpData->value * radius);
		auto movementDir = Util::GetLinearVelocity(camTargetPtr.get());
		auto screenLateralMovementOffset = 
		(
			DebugAPI::WorldToScreenPoint(ToVec3(camTargetPtr->data.location + movementDir)) - 
			DebugAPI::WorldToScreenPoint(ToVec3(camTargetPtr->data.location))
		).x;
		lockOnIndicatorRotOffset += 
		(
			(
				(2.0f * PI * *g_deltaTimeRealTime) * 
				std::clamp(5.0f * screenLateralMovementOffset / DebugAPI::screenResX, -1.0f, 1.0f)
			) / 
			lockOnIndicatorOscillationInterpData->secsInterpToMaxInterval
		);
		float rotAng1 = Util::NormalizeAng0To2Pi(3.0f * PI / 4.0f + lockOnIndicatorRotOffset);
		float rotAng2 = Util::NormalizeAng0To2Pi(PI / 4.0f + lockOnIndicatorRotOffset);
		float rotAng3 = Util::NormalizeAng0To2Pi(-PI / 4.0f + lockOnIndicatorRotOffset);
		float rotAng4 = Util::NormalizeAng0To2Pi(5.0f * PI / 4.0f + lockOnIndicatorRotOffset);
		// Arrows 1 and 2, circles 1 and 2.
		std::vector<uint32_t> elementColors = 
		{
			0x000000FF,
			0xFFFFFFFF,
			0x000000FF,
			0xFFFFFFFF
		};
		// Retract arrows when not facing target.
		radius *= lockOnIndicatorOscillationInterpData->value;
		if (radius != 0.0f)
		{
			auto newCenter = center + gapDelta * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			// Outer.
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				elementColors[2],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				elementColors[2],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				elementColors[2],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				elementColors[2],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);

			// Inner.
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 
				0.75f * radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				elementColors[3],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				elementColors[3],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				elementColors[3],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				elementColors[3],
				thickness,
				thickness,
				0.0f
			);
		}

		// Fewer segments to draw when the gap is small (no readily apparent loss in quality).
		uint32_t numSegments = std::clamp(static_cast<int>(gapDelta * 3), 8, 48);
		DebugAPI::QueueCircle2D
		(
			center,
			elementColors[0],
			numSegments,
			false,
			thickness + gapDelta,
			thickness,
			0.0f
		);
		DebugAPI::QueueCircle2D
		(
			center,
			elementColors[1],
			numSegments,
			false,
			gapDelta,
			thickness,
			0.0f
		);

		// Outline with two circles if near the edge of the screen for better visibility.
		const float buffer = DebugAPI::screenResY / 25.0f;
		if (center.x < buffer || 
			center.x > DebugAPI::screenResX - buffer ||
			center.y < buffer || 
			center.y > DebugAPI::screenResY - buffer)
		{
			DebugAPI::QueueCircle2D
			(
				center, 
				elementColors[0], 
				numSegments,
				false,
				2.0f * thickness + gapDelta + radius,
				2.0f * thickness,
				0.0f
			);
			DebugAPI::QueueCircle2D
			(
				center, 
				elementColors[1], 
				numSegments,
				false,
				gapDelta + radius,
				2.0f * thickness,
				0.0f
			);
		}
	}

	void CameraManager::FaceLockOnTarget(const int32_t& a_playerID)
	{
		// NOTE: 
		// Called externally.
		// Adjust rotation to instantly face the lock on target.
		
		if (!isLockedOn || a_playerID == -1)
		{
			return;
		}

		// Update lock on target's focus position first.
		CheckLockOnTarget();
		if (!Util::HandleIsValid(camLockOnTargetHandle))
		{
			return;
		}

		// Update rotation next.
		// Pitch directly if toggling facing direction and not in zoom assist mode;
		// otherwise, maintain the player's adjusted pitch.
		if (lockOnShouldAutoRotate && 
			Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kZoom)
		{
			camBaseTargetPosPitch = 
			camTargetPosPitch = 
			camPitch = Util::NormalizeAngToPi
			(
				facingLockOnTarget ? 
				Util::GetPitchBetweenPositions
				(
					camFocusPoint, camLockOnFocusPoint
				) :
				Util::GetPitchBetweenPositions
				(
					camLockOnFocusPoint, camFocusPoint
				)
			);
		}

		if (camCollisions || 
			lockOnShouldAutoRotate || 
			Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kZoom)
		{
			camBaseTargetPosYaw = 
			camTargetPosYaw = 
			camYaw = Util::NormalizeAng0To2Pi
			(
				facingLockOnTarget ? 
				Util::GetYawBetweenPositions
				(
					camFocusPoint, camLockOnFocusPoint
				) :
				Util::GetYawBetweenPositions
				(
					camLockOnFocusPoint, camFocusPoint
				)
			);
		}

		// Facing direction changed.
		if (lockOnShouldAutoRotate)
		{
			glob.coopPlayers[a_playerID]->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kCamera,
				fmt::format
				(
					"P{}: Now facing {} with camera", 
					a_playerID + 1,
					facingLockOnTarget ? 
					camLockOnTargetHandle.get()->GetName() :
					"the party"
				)
			);
		}
		else
		{
			// Auto-rotation resumed.
			glob.coopPlayers[a_playerID]->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kCamera,
				fmt::format
				(
					"P{}: Auto-rotating to face {}", 
					a_playerID + 1,
					facingLockOnTarget ? 
					camLockOnTargetHandle.get()->GetName() :
					"the party"
				)
			);
		}
	}

	void CameraManager::FadeObstructions()
	{
		// Fade or unfade objects that obstruct the LOS from the camera to each player.
		// Of course, creating a shader that selectively fades obstructions,
		// preferably partially, especially for those objects without a fade node,
		// would be the best solution here instead of fully fading each obstruction.
		
		// Maps objects to a triplet (fade index, hit distance from camera).
		std::unordered_map<RE::NiPointer<RE::NiAVObject>, std::pair<int32_t, float>> 
		obstructions;
		// Check raycast hits from the camera to each player.
		for (const auto& p : glob.coopPlayers)
		{
			// Ignore inactive and non-focal players if a focal player is set.
			if (!p->isActive || focalPID != -1 && p->playerID != focalPID)
			{
				continue;
			}

			auto actorCenter = p->mm->playerTorsoPosition;
			auto player3DPtr = Util::GetRefr3D(p->coopActor.get());
			auto camForwardOffset = 
			(
				Util::RotationToDirectionVect(-camPitch, Util::ConvertAngle(camYaw)) * 
				camTargetPosHullSize
			);
			// Cast from the actor's center to the camera node's position, offset by one hull size.
			auto camNodePos = camTargetPos + camForwardOffset;
			auto results = Raycast::GetAllHavokCastHitResults
			(
				ToVec4(actorCenter),
				ToVec4(camNodePos), 
				{ player3DPtr ? player3DPtr.get() : nullptr }
			);
			for (uint32_t i = 0; i < results.size(); ++i)
			{
				const auto& result = results[i];
				if (result.hitObjectPtr &&
					!result.hitObjectPtr->flags.all(RE::NiAVObject::Flag::kHidden))
				{
					auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
					auto asActor = hitRefrPtr ? hitRefrPtr->As<RE::Actor>() : nullptr;
					auto asActivator = 
					(
						hitRefrPtr ? 
						hitRefrPtr->As<RE::TESObjectACTI>() : 
						nullptr
					);
						
					// Do not fade out the lock on target.
					if (Util::HandleIsValid(result.hitRefrHandle) && 
						result.hitRefrHandle == camLockOnTargetHandle)
					{
						continue;
					}

					// NOTE: 
					// May remove if this causes issues later.
					// Add statics and lights that were set to never fade.
					if (hitRefrPtr && 
						hitRefrPtr->GetBaseObject() && 
						hitRefrPtr->GetBaseObject()->Is(RE::FormType::Static)) 
					{
						auto asStatic = 
						(
							hitRefrPtr->GetBaseObject()->As<RE::TESObjectSTAT>()
						);
						auto neverFades = 
						(
							asStatic->formFlags & 
							RE::TESObjectSTAT::RecordFlags::kNeverFades
						);
						if (neverFades != 0) 
						{
							asStatic->formFlags ^= 
							RE::TESObjectSTAT::RecordFlags::kNeverFades;
						}
					}

					if (hitRefrPtr && 
						hitRefrPtr->GetBaseObject() && 
						hitRefrPtr->GetBaseObject()->Is(RE::FormType::Light))
					{
						auto neverFades = 
						(
							hitRefrPtr->formFlags & 
							RE::TESObjectREFR::RecordFlags::kNeverFades
						);
						if (neverFades != 0)
						{
							hitRefrPtr->formFlags ^= 
							RE::TESObjectREFR::RecordFlags::kNeverFades;
						}
					}

					if (!asActor)
					{
						auto object3DPtr = 
						(
							hitRefrPtr ? 
							Util::GetRefr3D(hitRefrPtr.get()) : 
							result.hitObjectPtr
						);
						if (object3DPtr && object3DPtr->GetRefCount() > 0)
						{
							// If not already added as an obstruction, 
							// add directly with raycast hit index as fade index.
							if (obstructions.empty() || !obstructions.contains(object3DPtr))
							{
								obstructions.insert
								(
									{
										object3DPtr, 
										{
											i, camTargetPos.GetDistance(ToNiPoint3(result.hitPos)) 
										}
									}
								);
							}
						}
					}
				}
			}
		}

		// NOTE: 
		// NiAVObjects in obstructions list should not be invalid since they've been IncRef'd
		// when constructed as NiPointers.
		// And the naked NiAVObject ptrs in the handled set are kept valid while they are inserted
		// into the fade data list, which also wraps them in NiPointers.
		
		// NOTE:
		// Even if the fade setting is not active, 
		// compile a set of obstructingn objects between the camera and each player
		// for crosshair selection purposes.
		if (!Settings::bFadeObstructions)
		{
			ResetFadeAndClearObstructions();
		}

		// Add new obstructions or update fade indices if already added.
		for (const auto& [object3DPtr, fadeIndexDistPair] : obstructions) 
		{
			if (!object3DPtr || object3DPtr->GetRefCount() == 0)
			{
				continue;
			}

			const auto iter = obstructionFadeDataMap.find(object3DPtr);
			if (iter == obstructionFadeDataMap.end())
			{
				// Insert new obstruction to fade.
				obstructionFadeDataMap.insert_or_assign
				(
					object3DPtr,
					std::make_unique<ObjectFadeData>
					(
						object3DPtr,
						fadeIndexDistPair.first,
						fadeIndexDistPair.second,
						true
					)
				);
			}
			else if (const auto& fadeData = iter->second; 
					 fadeData && fadeData->fadeIndex < fadeIndexDistPair.first)
			{
				// Updated fade index means we have to modify its fade amount.
				fadeData->SignalFadeStateChange
				(
					object3DPtr, fadeData->shouldFadeOut, fadeIndexDistPair.first
				);
			}
		}

		// Only adjust fade for collated objects if the setting is enabled
		if (Settings::bFadeObstructions)
		{
			// Update fade data for handled obstructions.
			auto iter = obstructionFadeDataMap.begin(); 
			while (iter != obstructionFadeDataMap.end()) 
			{
				const auto& handled3DPtr = iter->first;
				const auto& fadeData = iter->second;
				// Must be a valid object, 
				// be within the radial distance of the camera if proximity fade is active,
				// and must be a smaller object without a fade node 
				// if not fading larger obstructions.
				bool canFade = 
				(
					(handled3DPtr && handled3DPtr->GetRefCount() > 0) &&
					(
						!Settings::bProximityFadeOnly ||
						fadeData->hitToCamDist < camTargetRadialDistance
					) && 
					(
						Settings::bFadeLargerObstructions || 
						!handled3DPtr->AsFadeNode()
					)
				);
				bool shouldRemove = false;
				if (canFade)
				{
					// Check if the object is not in the current obstructions set, 
					// and fade it back in if it isn't.
					bool shouldFadeIn = 
					{
						(fadeData->shouldFadeOut) && 
						(
							(obstructions.empty() || !obstructions.contains(handled3DPtr))
						)
					};
					if (shouldFadeIn)
					{
						fadeData->SignalFadeStateChange(handled3DPtr, false, fadeData->fadeIndex);
					}

					// Remove fully faded in/out or invalid obstructions.
					shouldRemove = !fadeData->UpdateFade(handled3DPtr);
				}
				else
				{ 
					// If the object ptr is still valid, fully fade in before removing.
					if (handled3DPtr && handled3DPtr->GetRefCount() > 0)
					{
						fadeData->InstantlyResetFade(handled3DPtr);
					}

					shouldRemove = true;
				}

				// Only increment the iter if data was not removed from the map.
				// Otherwise, the iter should remain where the previous data was removed
				// since a different element may be present at that location.
				if (shouldRemove)
				{
					iter = obstructionFadeDataMap.erase(iter);
				}
				else
				{
					iter++;
				}
			}
		}
	}

	float CameraManager::GetAutoRotateAngle(bool&& a_computePitch)
	{
		// Get the average movement pitch delta to add to the base camera pitch.
		// Attempts to improve visibility when the party moves up or down slopes.

		if (IsAdjustingCamera())
		{
			leadingCamPIDChangeTP = SteadyClock::now();
			return 0.0f;
		}

		// Per-player angle delta based on their movement.
		float autoRotateAngle = 0.0f;
		// Multiplier based on how long has elapsed since players have started moving [0.0, 1.0].
		float autoRotateMovementStartMult = 1.0f;
		// Averaged angle delta to add on top of base pitch/yaw.
		float avgAutoRotateAngle = 0.0f;
		// Is the current player mounted?
		bool isMounted = false;
		// Is the current player not ragdolled and not getting up?
		bool normalKnockState = true;
		// Only consider players that are not using furniture.
		bool notUsingFurniture = true;
		// Should turn the camera towards the focal player's crosshair target.
		bool turnTowardsTarget = 
		(
			(softFocalPID != -1) ||
			(
				focalPID != -1 && 
				Util::HandleIsValid(glob.coopPlayers[focalPID]->tm->selectedTargetActorHandle) &&
				!glob.coopPlayers[focalPID]->tm->selectedTargetActorHandle.get()->IsDead()
			)
		);

		float maxDotProd = -FLT_MAX;
		// Turn towards the soft focal player more slowly when they are near the centroid point.	
		float focalPlayerDistRotMult = 1.0f;
		const auto centroidFocusPoint = 
		(
			camCentroidPoint + RE::NiPoint3(0.0f, 0.0f, avgPlayerHeight)
		);
		auto focusPoint = centroidFocusPoint;
		// Number of players considered when determining the movement auto-rotate angle.
		// Will divide into the total movement pitch accumulated.
		uint32_t consideredPlayersCount = 0;
		
		// Four elevation change scenarios relative to the camera:
		// 1. Up a slope away from camera: pitch cam upward.
		// 2. Down a slope away from camera: pitch cam downward.
		// 3. Up a slope towards the camera: pitch cam downward.
		// 4. Down a slope towards the camera: pitch cam upward.
		float signAdjustment = 1.0f;
		// Use camera origin position's path direction 
		// as the camera movement direction. 
		if (camCentroidPointDelta.Length() > 1E-2f) 
		{
			float originMovYaw = Util::DirectionToGameAngYaw
			(
				camCentroidPointDelta
			); 
			float moveAngRelToFacing = Util::NormalizeAngToPi
			(
				originMovYaw - camYaw
			);
			signAdjustment = cosf(moveAngRelToFacing);
		}

		// Trying out automatic switching of the soft focal player.
		// Needs work, so commenting out for now.
		/*bool movingTowardsCam =
		(
			centroidPointDirXY.Dot
			(
				Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(camYaw))
			) < 0.0f
		);
		if (softFocalPID != -1 && 
			leadingCamPID != softFocalPID && 
			Util::GetElapsedSeconds(leadingCamPIDChangeTP) > 1.0f &&
			!movingTowardsCam)
		{
			DBG("AUTO SOFT FOCAL PID CHANGE: {} -> {}.", softFocalPID, leadingCamPID);
			softFocalPID = leadingCamPID;
		}*/

		if (a_computePitch && !turnTowardsTarget)
		{
			float sign = signAdjustment < 0.0f ? -1.0f : 1.0f;
			if (camCentroidPointDelta.Length() > 0.1f)
			{
				avgAutoRotateAngle = Util::InterpolateSmootherStep
				(
					0.0f, 
					std::clamp
					(
						Util::NormalizeAngToPi
						(
							(0.8f * sign * Util::DirectionToGameAngPitch(camCentroidPointDelta)) + 
							Settings::fCamAutoRotateBasePitch -
							camPitch
						),
						-camMaxPitchAngMag,
						camMaxPitchAngMag
					),
					fabsf(signAdjustment)
				);
			}
			else
			{
				avgAutoRotateAngle = 0.0f;
			}

			// REMOVE when done debugging.
			/*DBG
			(
				"PITCH: Pitch delta: {} ({} - {}), sign adjustment: {}. "
				"Delta length: {} ({},{},{}). Interp factor: {}.",
				avgAutoRotateAngle * TO_DEGREES,
				camCentroidPointDelta.Length() > 0.1f ? 
				0.8f * sign * Util::DirectionToGameAngPitch
				(
					camCentroidPointDelta
				) * TO_DEGREES :
				movementPitchOffset * TO_DEGREES,
				camPitch * TO_DEGREES,
				signAdjustment,
				camCentroidPointDelta.Length(),
				camCentroidPointDelta.x,
				camCentroidPointDelta.y,
				camCentroidPointDelta.z,
				camInterpFactor
			);
			DebugAPI::QueuePoint3D(ToVec3(focusPoint), 0xFF0000FF, 5.0f);
			DebugAPI::QueueArrow3D
			(
				ToVec3(focusPoint), 
				ToVec3(focusPoint + camCentroidPointDelta * 50.0f),
				0xFF0000FF, 5.0f, 2.0f
			);
			*/
		}
		else
		{
			for (const auto& p : glob.coopPlayers)
			{
				if (!p->isActive)
				{
					continue;
				}
			
				// Only consider the focal player for calculating the movement auto-rotate angle.
				if (focalPID != -1 && p->playerID != focalPID)
				{
					continue;
				}

				// Do not adjust pitch/yaw to face non-soft focal players.
				if (softFocalPID != -1 && p->playerID != softFocalPID)
				{
					continue;
				}

				// Use the player actor or their mount, if mounted.
				const auto& movementActor = p->mm->movementActorPtr;
				if (!movementActor)
				{
					continue;
				}
			
				// This player's movement will affect the result.
				++consideredPlayersCount;

				auto charController = movementActor->GetCharController();
				isMounted = movementActor->IsAMount();
				normalKnockState = movementActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal;
				notUsingFurniture = 
				(
					!Util::HandleIsValid(p->coopActor->GetOccupiedFurniture())
				);
				// Only add auto-rotate angle for this player if they have a char controller
				// and are mounted or not using furniture and they are moving,
				// not moving their crosshair, not selecting a refr via flick,
				// and are not the focal player or the focal player is not facing the crosshair.
				bool addToTotal =
				(
					(normalKnockState) &&
					(isMounted || notUsingFurniture) && 
					(!p->pam->IsPerforming(InputAction::kMoveCrosshair)) &&
					(!p->tm->cycleSelectionWithLS) &&
					(
						p->pam->GetSecondsSinceLastStop(InputAction::kMoveCrosshair) > 
						2.0f * Settings::fSecsDefFlickInterval 
					) &&
					(
						(a_computePitch) || 
						(
							(!p->coopActor->IsAnimationDriven()) &&
							(
								turnTowardsTarget ||
								movementActor->actorState1.movingBack ||
								movementActor->actorState1.movingForward ||
								movementActor->actorState1.movingLeft ||
								movementActor->actorState1.movingRight
							)
						)
					)
				);

				if (!addToTotal)
				{
					continue;
				}
			
				// When turning towards a target, slow down rotation 
				// as the target moves closer to the center of the screen.
				if (turnTowardsTarget)
				{
					focusPoint = 
					(
						focalPID != -1 ? 
						Util::GetActorFocusPoint
						(
							p->tm->selectedTargetActorHandle.get().get(),
							p->tm->targetMotionState->cPos
						) : 
						Util::GetActorFocusPoint
						(
							p->coopActor.get(), 
							p->tm->playerMotionState->cPos
						)
					);

					SetCamOrientation();
					auto focusScreenPos = Util::WorldToScreenPoint3(focusPoint, false);
					auto screenFracX = 
					(
						fabsf(focusScreenPos.x - (0.5f * DebugAPI::screenResX)) / 
						(0.2f * DebugAPI::screenResX)
					);
					auto screenFracY = 
					(
						fabsf(focusScreenPos.y - (0.5f * DebugAPI::screenResY)) / 
						(0.2f * DebugAPI::screenResY)
					);
					auto screenFracZ = 0.5f * (1.0f - focusScreenPos.z);
					focalPlayerDistRotMult = Util::InterpolateEaseInEaseOut
					(
						0.0f, 
						1.0f,
						min
						(
							1.0f, 
							a_computePitch ? 
							(screenFracY + screenFracZ) :
							(screenFracX + screenFracZ)
						),
						5.0f
					);
					/*DBG
					(
						"{}: {}: Focal dist rot mult: {}. Focus: ({}, {}, {}), "
						"screen res: ({}, {}), "
						"frac X, Y, Z: {}, {}, {}.",
						p->coopActor->GetName(),
						a_computePitch ? "PITCH" : "YAW",
						focalPlayerDistRotMult,
						focusScreenPos.x,
						focusScreenPos.y,
						focusScreenPos.z,
						DebugAPI::screenResX,
						DebugAPI::screenResY,
						screenFracX, 
						screenFracY, 
						screenFracZ
					);*/
				}

				const auto& lsData = glob.cdh->GetAnalogStickState(p->deviceID, true);
				if (a_computePitch)
				{
					if (turnTowardsTarget)
					{
						autoRotateAngle = Util::NormalizeAngToPi
						(
							Util::GetPitchBetweenPositions
							(
								focalPID != -1 ? camRefrFocusPoint : centroidFocusPoint, focusPoint
							) - camPitch
						);
						autoRotateAngle *= focalPlayerDistRotMult;
					}
				}
				else
				{
					if (turnTowardsTarget)
					{
						autoRotateAngle = Util::NormalizeAngToPi
						(
							Util::NormalizeAng0To2Pi
							(
								Util::GetYawBetweenPositions
								(
									focalPID != -1 ? camRefrFocusPoint : centroidFocusPoint, 
									focusPoint
								)
							) - camYaw
						);
						autoRotateAngle *= focalPlayerDistRotMult;
					}
					else if (p->playerID == focalPID)
					{
						autoRotateAngle = Util::NormalizeAngToPi
						(
							Util::NormalizeAng0To2Pi(movementActor->data.angle.z) - camYaw
						);
					}
					else
					{
						autoRotateAngle = Util::NormalizeAngToPi
						(
							Util::NormalizeAng0To2Pi
							(
								p->analogStickParams[!AnalogStickParams::kLSWorldAng]
							) - camYaw
						);
					}

					float sign = autoRotateAngle < 0.0f ? -1.0f : 1.0f;
					autoRotateAngle = 
					(
						fabsf(autoRotateAngle) > PI / 2.0f ? 
						sign * PI - autoRotateAngle : 
						autoRotateAngle
					);
					// Dependent on how committed the player is to moving 
					// in their heading direction.
					// Also, slow down rotation if the player is moving into an obstacle,
					// which is indicated by a small position-based speed
					// compared to their actor state speed (does not account for collisions).
					if (softFocalPID == -1 && focalPID == -1)
					{
						const auto movementSpeed = p->coopActor->DoGetMovementSpeed();
						if (movementSpeed == 0.0f)
						{
							autoRotateAngle = 0.0f;
						}
						else 
						{
							autoRotateAngle *=
							(
								lsData.normMag * 
								std::clamp
								(
									fabsf
									(
										p->tm->playerMotionState->cVel.Length() / 
										movementSpeed
									),
									0.0f,
									1.0f
								)
							);	
						}
					}
				}
			
				// Do not factor in movement stop/start rotation delay if facing a target.
				if (turnTowardsTarget)
				{
					avgAutoRotateAngle += autoRotateAngle;
				}
				else
				{
					autoRotateMovementStartMult = Util::InterpolateSmootherStep
					(
						0.0f,
						movementAngleMultInterpData->value, 
						std::clamp
						(
							Util::GetElapsedSeconds(p->lastMovementStartReqTP) / 1.5f, 
							0.0f, 
							1.0f
						)
					);
					if (!a_computePitch)
					{
						avgAutoRotateAngle += autoRotateAngle;
					}
				}
			}
		}

		avgAutoRotateAngle /= max(1, consideredPlayersCount);
		avgAutoRotateAngle *= autoRotateMovementStartMult;

		// REMOVE when done debugging.
		/*if (softFocalPID != -1 && leadingCamPID != -1)
		{
			DBG
			(
				"{} DELTA: {} after mult by {}, div by {} players. "
				"Movement start mult: {}. Current: ({}, {}, {})", 
				a_computePitch ? "PITCH" : "YAW",
				avgAutoRotateAngle * RE::BSTimer::QGlobalTimeMultiplier() * TO_DEGREES, 
				focalPlayerDistRotMult,
				consideredPlayersCount,
				autoRotateMovementStartMult,
				a_computePitch ? 
				camBaseTargetPosPitch * TO_DEGREES : 
				camBaseTargetPosYaw * TO_DEGREES,
				a_computePitch ? 
				camTargetPosPitch * TO_DEGREES :
				camTargetPosYaw * TO_DEGREES,
				a_computePitch ?
				camPitch * TO_DEGREES :
				camYaw * TO_DEGREES
			);
		}*/

		return avgAutoRotateAngle * RE::BSTimer::QGlobalTimeMultiplier();
	}

	bool CameraManager::NoPlayersVisibleAtPoint
	(
		const RE::NiPoint3& a_point, bool&& a_checkAllNodes
	)
	{
		// Check if there are no players visible at the given point.
		// For each player, check if all player nodes are blocked from camera view or just one.
		// 'Visibility' here means a raycast to one or all nodes for each player does not hit
		// any intervening object.

		const auto strings = RE::FixedStrings::GetSingleton();
		const glm::vec4 point = ToVec4(a_point);
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			// Invalid 3D means this player is not visible.
			// Move on to the next player.
			auto loadedData = p->coopActor->loadedData;
			if (!loadedData)
			{
				continue;
			}

			auto data3DPtr = loadedData->data3D;
			if (!data3DPtr || !data3DPtr->parent)
			{
				continue;
			}
				
			bool allNodesVisible = true;
			bool hitPlayerNode = false;
			bool oneNodeVisible = false;
			for (const auto& nodeName : GlobalCoopData::CAM_VISIBILITY_NPC_NODES)
			{
				// All checked nodes must be visible from the camera target position
				// to consider the player as in view of the camera.
				auto nodePtr = RE::NiPointer<RE::NiAVObject>
				(
					data3DPtr->GetObjectByName(nodeName)
				); 
				if (nodePtr)
				{
					auto losCheck = Raycast::hkpCastRay
					(
						ToVec4(nodePtr->world.translate), 
						point, 
						std::vector<RE::NiAVObject*>
						{
							playerCam->cameraRoot.get(), data3DPtr.get() 
						}, 
						RE::COL_LAYER::kLOS
					);
					bool hitPlayerNode = !losCheck.hit;
					oneNodeVisible = hitPlayerNode;
					allNodesVisible &= oneNodeVisible;
					if (a_checkAllNodes)
					{
						if (allNodesVisible)
						{
							// Keep checking nodes if this node was visible.
							continue;
						}
						else
						{
							// Move to next player if this player 
							// has a node that is not visible.
							break;
						}
					}
					else
					{
						if (oneNodeVisible)
						{
							// Break here since one node was visible.
							break;
						}
						else
						{
							// Continue checking for one visible node.
							continue;
						}
					}
				}
			}

			if (a_checkAllNodes)
			{
				// All nodes visible for this player. End check.
				if (allNodesVisible)
				{
					return false;
				}
			}
			else
			{
				// One node was visible for this player. End check.
				if (oneNodeVisible)
				{
					return false;
				}
			}
		}

		return true;
	}

	void CameraManager::PerformStateTransition()
	{
		// Transition the camera from one state to another when the previous state differs
		// from the current one.

		DBG("State {} -> {}.", prevCamState, camState);

		// Nothing to do if the previous state is the same as the current one.
		if (camState == prevCamState)
		{
			return;
		}

		// Nothing to do when switching from one user-toggleable state to another.
		// Any setup is performed prior to the state change in the state toggle 
		// player action function.
		bool switchBetweenUserToggleableStates = 
		(
			(
				prevCamState == CamState::kAutoTrail ||
				prevCamState == CamState::kLockOn ||
				prevCamState == CamState::kManualPositioning 
			) &&
			(
				camState == CamState::kAutoTrail ||
				camState == CamState::kLockOn ||
				camState == CamState::kManualPositioning 
			) 
		);
		if (switchBetweenUserToggleableStates)
		{
			return;
		}
		

		// Clear out old dialogue target.
		if (prevCamState == CamState::kDialogue)
		{
			DBG("Clear out dialogue target handle.");
			camDialogueTargetHandle = RE::ObjectRefHandle();
		}

		// Set up death or dialogue state.
		if (camState == CamState::kDeath || camState == CamState::kDialogue)
		{
			// Unfreeze time, if needed.
			Util::ToggleFreezeTime(false);
			// Clear lock on target.
			camLockOnTargetHandle = RE::ActorHandle();
			if (camState == CamState::kDeath)
			{
				// Set P1 as controls driven.
				Util::SetPlayerAIDriven(false);
				// Force third person on transition.
				if (playerCam && playerCam->currentState->id == RE::CameraState::kBleedout)
				{
					playerCam->lock.Lock();
					playerCam->ForceThirdPerson();
					playerCam->UpdateThirdPerson(true);
					playerCam->lock.Unlock();
				}

				// Set start TP.
				deathCameraTP = SteadyClock::now();
				DBG("Set up death state.");
			}
			else
			{
				auto menuTopicManager = RE::MenuTopicManager::GetSingleton(); 
				if (menuTopicManager)
				{
					camDialogueTargetHandle = menuTopicManager->speaker;
					if (!Util::HandleIsValid(camDialogueTargetHandle))
					{
						camDialogueTargetHandle = menuTopicManager->lastSpeaker;

					}

					adjustedAfterReachingDialoguePos = false;
				}

				// Validate the new dialogue target.
				CheckLockOnTarget();
				// Set start and speaker change TP.
				dialogueCameraTP = 
				dialogueSpeakerChangedTP = SteadyClock::now();
				DBG
				(
					"Set up dialogue state. Target is {}.",
					Util::HandleIsValid(camDialogueTargetHandle) ? 
					camDialogueTargetHandle.get()->GetName() : 
					"NONE"
				);
			}
		}
	}

	bool CameraManager::PointOnScreenAtCamOrientationScreenspaceMargin
	(
		const RE::NiPoint3& a_point, const float& a_marginRatio
	)
	{
		// Is the given point in the camera's frustum at the current camera position and rotation,
		// also accounting for a pixel ratio at the edges of the screen, if given ([0, 1]).

		bool onScreen = false;
		auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr)
		{
			return false;
		}

		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		RE::NiCamera::WorldPtToScreenPt3
		(
			niCamPtr->worldToCam, niCamPtr->port, a_point, x, y, z, 1e-5f
		);
		// Factor in screen dimensions and margin.
		onScreen = 
		(
			x >= a_marginRatio && 
			x <= 1.0f - a_marginRatio && 
			y >= a_marginRatio && 
			y <= 1.0f - a_marginRatio && 
			z < 1.0f &&
			z > -1.0f
		);
		
		return onScreen;
	}

	bool CameraManager::PointOnScreenAtCamOrientationScreenspaceMargin
	(
		const RE::NiPoint3& a_point, 
		const RE::NiPoint3& a_camPos,
		const RE::NiPoint2& a_rotation,
		const float& a_marginRatio
	)
	{
		// Is the given point in the camera's frustum at the given camera position and rotation,
		// also accounting for a pixel ratio at the edges of the screen, if given ([0, 1]).
		
		// Temporarily move the camera to the given position and set the given rotation.
		// Will use current orientation otherwise.
		SetCamOrientation(a_camPos, a_rotation.x, a_rotation.y, true);
		return PointOnScreenAtCamOrientationScreenspaceMargin(a_point, a_marginRatio);
	}

	bool CameraManager::PointOnScreenAtCamOrientationWorldspaceMargin
	(
		const RE::NiPoint3& a_point, const float& a_marginWorldDist
	)
	{
		// Is the given point in the camera's frustum at the current camera position and rotation,
		// also accounting for a worldspace distance margin around the given point.

		bool onScreen = false;
		auto niCamPtr = Util::GetNiCamera();
		// Need the NiCamera and Debug Overlay Menu view.
		if (!niCamPtr)
		{
			return false;
		}

		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		// Compute four points offset from the origin point by the given margin distance
		// in the four axial world directions of the camera.
		const auto camUp = playerCam->cameraRoot->local.rotate * RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const auto camRight = playerCam->cameraRoot->local.rotate * RE::NiPoint3(1.0f, 0.0f, 0.0f);
		const auto camMaxXWorldPos = a_point + camRight * a_marginWorldDist;
		const auto camMinXWorldPos = a_point + -camRight * a_marginWorldDist;
		const auto camMaxYWorldPos = a_point + -camUp * a_marginWorldDist;
		const auto camMinYWorldPos = a_point + camUp * a_marginWorldDist;
		
		// Convert and check all four world positions, accounting for the worldspace margin.
		auto isOnScreen = 
		[&niCamPtr](const RE::NiPoint3& a_pos, float& x, float& y, float& z) -> bool
		{
			const float zeroTolerance = 1e-5f;
			RE::NiCamera::WorldPtToScreenPt3
			(
				niCamPtr->worldToCam, niCamPtr->port, a_pos, x, y, z, zeroTolerance
			);
			return 
			(
				x >= -zeroTolerance && 
				x <= 1.0f + zeroTolerance && 
				y >= -zeroTolerance && 
				y <= 1.0f + zeroTolerance && 
				z <= 1.0f + zeroTolerance &&
				z >= -1.0f - zeroTolerance
			);
		};

		onScreen = 
		(
			isOnScreen(camMaxXWorldPos, x, y, z) &&
			isOnScreen(camMinXWorldPos, x, y, z) &&
			isOnScreen(camMaxYWorldPos, x, y, z) &&
			isOnScreen(camMinYWorldPos, x, y, z)
		);
		
		return onScreen;
	}

	bool CameraManager::PointOnScreenAtCamOrientationWorldspaceMargin
	(
		const RE::NiPoint3& a_point,
		const RE::NiPoint3& a_camPos, 
		const RE::NiPoint2& a_rotation,
		const float& a_marginWorldDist
	)
	{
		// Is the given point in the camera's frustum at the given camera position and rotation,
		// also accounting for a worldspace distance margin around the given point.

		// Temporarily move the camera to the given position and set the given rotation.
		// Will use current orientation otherwise.
		SetCamOrientation(a_camPos, a_rotation.x, a_rotation.y, true);
		return PointOnScreenAtCamOrientationWorldspaceMargin(a_point, a_marginWorldDist);
	}

	void CameraManager::ResetCamData()
	{
		// Reset all camera data.
		
		// Reset player IDs.
		adjustingCamPID = -1;
		leadingCamPID = -1;
		originPointPID = -1;
		if ((focalPID != -1) && 
			(
				!glob.coopSessionActive ||
				!glob.coopPlayers[focalPID]->isActive || 
				!glob.coopPlayers[focalPID]->selfValid
			))
		{
			focalPID = -1;
		}

		if ((softFocalPID != -1) && 
			(
				!glob.coopSessionActive ||
				!glob.coopPlayers[softFocalPID]->isActive || 
				!glob.coopPlayers[softFocalPID]->selfValid
			))
		{
			softFocalPID = -1;
		}
		
		// Starts with no adjustment mode active and in the autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		camAdjMode = CamAdjustmentMode::kNone;

		// Reset interp data.
		lockOnIndicatorOscillationInterpData->Reset();
		movementAngleMultInterpData->Reset(false, true);

		// Reset interp factors and ratio for blending pitch/yaw changes.
		camInterpFactor = 
		(
			1.0f - powf(1.0f - Settings::fCamInterpFactor, 60.0f * *g_deltaTimeRealTime)
		);

		prevRotInterpRatio = 0.0f;

		// Reset to autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		autoRotateSuspended = false;
		// Update collisions flag before camera orientation modifications below.
		camCollisions = 
		(
			(
				Settings::bCamExteriorCollisions && exteriorCell
			) ||
			(
				Settings::bCamInteriorCollisions && !exteriorCell	
			)
		);
		adjustedAfterReachingDialoguePos = false;
		delayedZoomInUnderExteriorRoof = delayedZoomOutUnderExteriorRoof = false;
		facingLockOnTarget = false;
		inDeathCamState = false;
		inDialogueCamState = false;
		isAutoTrailing = true;
		isColliding = false;
		isManuallyPositioned = false;
		isLockedOn = false;
		lockInteriorOrientationOnInit = false;
		lockOnShouldAutoRotate = true;
		movingToDialogueStartPos = false;

		// Make sure time was not frozen. Unfreeze if so.
		Util::ToggleFreezeTime(false);
		manualPositioningTimeFrozen = false;

		// Reset lock-on-related data.
		lockOnTargetInSight = false;
		camLockOnTargetHandle = RE::ActorHandle();
		camDialogueTargetHandle = RE::ObjectRefHandle();

		playerCam = RE::PlayerCamera::GetSingleton();
		if (playerCam)
		{
			if (!tpState) 
			{
				if (auto camState = playerCam->currentState.get(); camState)
				{
					tpState = skyrim_cast<RE::ThirdPersonState*>
					(
						playerCam->cameraStates[RE::CameraState::kThirdPerson].get()
					); 
					if (!tpState)
					{
						ERR("Could not get third person state.");
					}
				}
				else
				{
					ERR("Could not get camera state.");
				}
			}
		}
		else
		{
			ERR("Could not get player cam.");
		}

		// Set rotation-related data.
		movementPitchRunningTotal = movementYawToCamRunningTotal = 0.0f;
		numMovementPitchReadings = numMovementYawToCamReadings = 0;

		// Positions.
		// Set focus point to the origin point.
		camCentroidPoint = RE::NiPoint3();
		// Set average player height to offset the base origin point.
		avgPlayerHeight = 0.0f;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			avgPlayerHeight += p->coopActor->GetHeight();
			camCentroidPoint += p->coopActor->data.location;
		}
		
		avgPlayerHeight /= glob.livingPlayers;
		DBG("Average player height: {}.", avgPlayerHeight);
		camCentroidPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
		camCentroidPoint.z += avgPlayerHeight;

		camFocusPoint =
		camLockOnFocusPoint = 
		camCollisionOriginPoint = 
		camOriginPoint = camCentroidPoint;

		camCentroidPointDelta = RE::NiPoint3();
		auto bounds = Util::GetVertCollPoints(camOriginPoint, 0.0f);
		camMaxAnchorPointZCoord = bounds.first;
		camMinAnchorPointZCoord = bounds.second;

		// Set target positions equal to the node/P1 looking at position.
		auto p1LookingAt = glob.player1Actor->GetLookingAtLocation();
		camRefrFocusPoint = 
		camBaseTargetPos =
		camTargetPos =
		camCollisionTargetPos =
		(
			playerCam && playerCam->cameraRoot ?
			playerCam->cameraRoot->world.translate : 
			p1LookingAt
		);
		camTargetXYOffset =
		camXYOffset = RE::NiPoint2();
		
		// Set radial distance equal to the node's distance from the origin point.
		camRadialDistanceOffset = camSavedRadialDistanceOffset = 0.0f;
		camMinTrailingDistance = Settings::fCamMinTrailingDistance;
		camTargetRadialDistance = 
		camTrueRadialDistance = camBaseTargetPos.GetDistance(camFocusPoint);
		// Reset base height, zoom, and other offsets.
		camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;
		camBaseHeightOffset = camHeightOffset = 0.0f;
		lockOnIndicatorRotOffset = 0.0f;
		movementPitchOffset = movementYawOffset = 0.0f;
		
		// Set initial rotation to the current node rotation/P1 rotation.
		if (playerCam && playerCam->cameraRoot) 
		{
			const auto camForward = 
			(
				playerCam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f)
			);
			camPitch = 
			camCurrentPitchToFocus = 
			camBaseTargetPosPitch = 
			camTargetPosPitch = Util::DirectionToGameAngPitch(camForward);

			camYaw = 
			camCurrentYawToFocus =
			camBaseTargetPosYaw = 
			camTargetPosYaw = Util::DirectionToGameAngYaw(camForward);

			camFOV = playerCam->worldFOV;
		}
		else
		{
			camPitch = 
			camCurrentPitchToFocus = 
			camBaseTargetPosPitch = 
			camTargetPosPitch = glob.player1Actor->data.angle.x;

			camYaw =
			camCurrentYawToFocus =
			camBaseTargetPosYaw = 
			camTargetPosYaw = glob.player1Actor->GetHeading(false);

			// Set default camera FOV.
			camFOV = 75.0f;
		}

		reqToAutoPitch = fabsf(camBaseTargetPosPitch) < Settings::fCamAutoPitchReqAngMag;
		std::optional<RE::TESObjectREFR*> closestTeleportDoor = std::nullopt;
		std::optional<float> closestTeleportDoorDistComp = std::nullopt;
		// Check for nearby teleport doors within the current cam radial distance of P1.
		// If so, the camera should be set in between the door and P1.
		Util::ForEachReferenceInRange
		(
			p1LookingAt, camTargetRadialDistance + camTargetPosHullSize, false,
			[&](RE::TESObjectREFR* a_refr) 
			{
				if (!a_refr || 
					!Util::HandleIsValid(a_refr->GetHandle()) || 
					!a_refr->IsHandleValid())
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				// Ensure that the object reference is an interactable object.
				if (!a_refr->Is3DLoaded() || !a_refr->GetCurrent3D() || 
					a_refr->IsDeleted() || strlen(a_refr->GetName()) == 0)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				if ((a_refr->Is(RE::FormType::Door, RE::FormType::Activator)) || 
					(a_refr->data.objectReference && 
					 a_refr->data.objectReference->Is
					 (
						 RE::FormType::Door, RE::FormType::Activator
					 )))
				{
					// Only consider doors that teleport the player when activated.
					if (a_refr->extraList.HasType<RE::ExtraTeleport>())
					{
						auto refrCenter = Util::Get3DCenterPos(a_refr);
						float p1ToDoorDist = p1LookingAt.GetDistance(refrCenter);
						if ((!closestTeleportDoorDistComp.has_value()) ||
							(p1ToDoorDist < closestTeleportDoorDistComp.value()))
						{
							closestTeleportDoorDistComp = p1ToDoorDist;
							closestTeleportDoor = a_refr;
						}
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		// Clamp trailing distance to place the camera between the door and P1.
		if (!exteriorCell && closestTeleportDoorDistComp.has_value())
		{
			camRadialDistanceOffset = camSavedRadialDistanceOffset = 0.0f;
			camTargetRadialDistance = closestTeleportDoorDistComp.value();
			lockInteriorOrientationOnInit = true;
		}

		// If the cam is automatically resuming, 
		// adjust initial yaw to position the camera between P1 and the closest load door.
		if (!waitForToggle)
		{
			auto doorRefr = 
			(
				closestTeleportDoor.has_value() ? 
				closestTeleportDoor.value() : 
				nullptr
			); 
			if (doorRefr)
			{
				camYaw =
				camCurrentYawToFocus =
				camBaseTargetPosYaw =
				camTargetPosYaw = Util::GetYawBetweenPositions
				(
					Util::Get3DCenterPos(doorRefr), p1LookingAt
				);
			}
		}
	}

	void CameraManager::ResetFadeAndClearObstructions()
	{
		// Reset fade on all handled obstructions and then clear them.
		
		if (obstructionFadeDataMap.empty())
		{
			return;
		}
		
		for (const auto& [objectPtr, objectData] : obstructionFadeDataMap)
		{
			if (objectPtr && objectPtr->GetRefCount() > 0)
			{
				objectData->InstantlyResetFade(objectPtr);
			}
		}

		obstructionFadeDataMap.clear();
	}

	void CameraManager::SetCamActorCollisions(bool&& a_set)
	{
		// Remove collisions between the camera and character controllers
		// to prevent actors from fading when they are too close to
		// the camera. We'll apply our own fade later.

		if (!playerCam)
		{
			return;
		}

		auto rigidBodyPtr = playerCam->rigidBody; 
		if (!rigidBodyPtr)
		{
			return;
		}

		auto world = rigidBodyPtr->GetWorld1(); 
		if (!world)
		{
			return;
		}

		auto filterInfo = (RE::bhkCollisionFilter*)world->collisionFilter; 
		if (!filterInfo)
		{
			return;
		}

		// Credits to ersh1 for the code on setting what other collision layers 
		// collide with a collision layer:
		// https://github.com/ersh1/Precision/blob/main/src/Hooks.cpp#L848
		if (a_set)
		{
			// Camera collides with char controller.
			filterInfo->layerBitfields[!RE::COL_LAYER::kCameraPick] |= 
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			);
			// Char controller collides with camera.
			filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] |= 
			(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCameraPick)
			);
		}
		else
		{
			// Camera won't collide with char controller.
			filterInfo->layerBitfields[!RE::COL_LAYER::kCameraPick] &= 
			~(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
			);
			// Char controller won't collide with camera.
			filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] &= 
			~(
				static_cast<uint64_t>(1) << 
				static_cast<uint8_t>(!RE::COL_LAYER::kCameraPick)
			);
		}
	}

	void CameraManager::SetCamOrientation()
	{
		// Determine if the camera's local rotation also needs modification
		// and then update its orientation.

		if (!playerCam || !playerCam->cameraRoot)
		{
			return;
		}

		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		const auto& coopP1 = glob.coopPlayers[0];
		bool orbitStateActive = playerCam->currentState->id == RE::CameraState::kAutoVanity;
		bool bleedoutStateActive = playerCam->currentState->id == RE::CameraState::kBleedout;
		bool furnitureStateActive = playerCam->currentState->id == RE::CameraState::kFurniture;
		bool overrideLocalRotation =
		{
			orbitStateActive ||
			bleedoutStateActive ||
			furnitureStateActive ||
			p1->IsInRagdollState() ||
			p1->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			p1->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
			coopP1->pam->isSprinting ||
			glob.isCameraShakeActive
		};

		SetCamOrientation(overrideLocalRotation);
	}

	void CameraManager::SetCamOrientation(bool a_overrideLocalRotation)
	{
		// Set the camera's orientation using the cached rotation and position data.
		// Can also override the camera's local rotation as well.

		if (!playerCam || !playerCam->cameraRoot)
		{
			return;
		}

		Util::SetCameraRotation
		(
			playerCam, 
			camPitch, 
			camYaw, 
			a_overrideLocalRotation
		);
		Util::SetCameraPosition(playerCam, camTargetPos);
		if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
		{
			Util::NativeFunctions::UpdateWorldToScaleform(niCamPtr.get());
		}

		playerCam->worldFOV = camFOV;
		RE::NiUpdateData updateData{ };
		playerCam->cameraRoot->UpdateDownwardPass(updateData, 0);
	}

	void CameraManager::SetCamOrientation
	(
		const RE::NiPoint3& a_position,
		const float& a_pitch,
		const float& a_yaw
	)
	{
		// Set the camera's orientation using the given rotation and position data.
		// Can also override the camera's local rotation as well.

		if (!playerCam || !playerCam->cameraRoot)
		{
			return;
		}

		const auto& coopP1 = glob.coopPlayers[0];
		auto p1 = RE::PlayerCharacter::GetSingleton(); 
		bool orbitStateActive = playerCam->currentState->id == RE::CameraState::kAutoVanity;
		bool bleedoutStateActive = playerCam->currentState->id == RE::CameraState::kBleedout;
		bool furnitureStateActive = playerCam->currentState->id == RE::CameraState::kFurniture;
		bool overrideLocalRotation =
		{
			orbitStateActive ||
			bleedoutStateActive ||
			furnitureStateActive ||
			p1->IsInRagdollState() ||
			p1->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			p1->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
			coopP1->pam->isSprinting ||
			glob.isCameraShakeActive
		};
		Util::SetCameraRotation
		(
			playerCam, 
			a_pitch, 
			a_yaw, 
			overrideLocalRotation
		);
		Util::SetCameraPosition(playerCam, a_position);
		if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
		{
			Util::NativeFunctions::UpdateWorldToScaleform(niCamPtr.get());
		}

		playerCam->worldFOV = camFOV;
		RE::NiUpdateData updateData{ };
		playerCam->cameraRoot->UpdateDownwardPass(updateData, 0);
	}

	void CameraManager::SetCamOrientation
	(
		const RE::NiPoint3& a_position,
		const float& a_pitch, 
		const float& a_yaw, 
		bool a_overrideLocalRotation
	)
	{
		// Set the camera's orientation using the given rotation and position data.
		// Can also override the camera's local rotation as well.

		if (!playerCam || !playerCam->cameraRoot)
		{
			return;
		}

		Util::SetCameraRotation
		(
			playerCam, 
			a_pitch, 
			a_yaw, 
			a_overrideLocalRotation
		);
		Util::SetCameraPosition(playerCam, a_position);
		if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
		{
			Util::NativeFunctions::UpdateWorldToScaleform(niCamPtr.get());
		}

		playerCam->worldFOV = camFOV;
		RE::NiUpdateData updateData{ };
		playerCam->cameraRoot->UpdateDownwardPass(updateData, 0);
	}

	void CameraManager::SetPlayerFadePrevention(bool&& a_noFade)
	{
		// Enable/disable fading of players.

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			auto player3DPtr = Util::GetRefr3D(p->coopActor.get()); 
			if (!player3DPtr)
			{
				continue;
			}

			if (a_noFade) 
			{
				player3DPtr->fadeAmount = 1.0f;
				player3DPtr->flags.set
				(
					RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
				);
			}
			else
			{
				player3DPtr->flags.reset
				(
					RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
				);
			}

			RE::NiUpdateData updateData{ };
			player3DPtr->UpdateDownwardPass(updateData, 0);
		}

		// Make sure the lock on target is visible.
		const auto camLockOnTargetPtr = Util::GetRefrPtrFromHandle(camLockOnTargetHandle);
		if (camLockOnTargetPtr)
		{
			auto refr3DPtr = Util::GetRefr3D(camLockOnTargetPtr.get()); 
			if (refr3DPtr)
			{
				if (a_noFade) 
				{
					refr3DPtr->fadeAmount = 1.0f;
					refr3DPtr->flags.set
					(
						RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
					);
				}
				else
				{
					refr3DPtr->flags.reset
					(
						RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade
					);
				}

				RE::NiUpdateData updateData{ };
				refr3DPtr->UpdateDownwardPass(updateData, 0);
			}
		}
	}

	void CameraManager::SetWaitForToggle(bool a_set)
	{
		// Signal the camera manager to wait for toggle.
		// Co-op camera is only re-enabled by P1 and if at least two controllers are connected.

		waitForToggle = (a_set) && (!glob.hybridModeActive || !glob.coopSessionActive);
	}

	void CameraManager::SwapShoulders(const int32_t& a_playerID)
	{
		// NOTE: 
		// Called externally.
		// Swap shoulder offset position.
		// Only takes effect when the offset is applicable 
		// (dialogue or focal player crosshair target).

		if (a_playerID == -1)
		{
			return;
		}

		shoulderOffsetRight = !glob.cam->shoulderOffsetRight;
		shoulderOffsetChangedTP = SteadyClock::now();

		glob.coopPlayers[a_playerID]->tm->SetCrosshairMessageRequest
		(
			CrosshairMessageType::kCamera,
			fmt::format
			(
				"P{}: Shoulder swap", a_playerID + 1, shoulderOffsetRight ? "right" : "left"
			)
		);
	}

	void CameraManager::ToggleCoopCamera(bool a_enable)
	{
		// External request to toggle the co-op camera on/off.

		// Do not enable if there is only one controller plugged in.
		if (glob.cdh->activeControllerCount <= 1)
		{
			return;
		}

		if (a_enable)
		{
			RequestStateChange(ManagerState::kRunning);
		}
		else
		{
			if (!IsUninitialized())
			{
				RequestStateChange(ManagerState::kPaused);
			}
		}
	}

	bool CameraManager::ToggleSoftFocalPlayerTracking(const int32_t& a_playerID)
	{
		// Set the camera to track the player given by the player ID, 
		// or switch back to following the party.
		// Return true if now tracking the soft focal player, or false if now tracking the party.

		if (a_playerID == -1 || focalPID != -1)
		{
			return false;
		}

		const auto& softFocalP = glob.coopPlayers[a_playerID];
		if (!softFocalP->isActive)
		{
			return false;
		}

		bool toggledOn = softFocalPID == -1 || softFocalPID != a_playerID;
		if (toggledOn)
		{
			setSoftFocalTargetPosTP = SteadyClock::now();
			softFocalPID = a_playerID;
			const auto focusPoint = Util::GetActorFocusPoint
			(
				softFocalP->coopActor.get(), 
				softFocalP->tm->playerMotionState->cPos
			);
			
			const auto centroidFocusPoint = 
			(
				camCentroidPoint + RE::NiPoint3(0.0f, 0.0f, avgPlayerHeight)
			);
			camBaseTargetPosPitch = camTargetPosPitch = camPitch = Util::NormalizeAngToPi
			(
				Util::GetPitchBetweenPositions
				(
					centroidFocusPoint, focusPoint
				)
			);
			camBaseTargetPosYaw = camTargetPosYaw = camYaw = Util::NormalizeAng0To2Pi
			(
				Util::GetYawBetweenPositions
				(
					centroidFocusPoint, focusPoint
				)
			);

			softFocalP->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kCamera,
				fmt::format("P{}: Tracked with camera", a_playerID + 1)
			);
		}
		else
		{
			softFocalPID = -1;
			softFocalP->tm->SetCrosshairMessageRequest
			(
				CrosshairMessageType::kCamera,
				fmt::format("P{}: No longer tracked by camera", a_playerID + 1)
			);
		}

		return toggledOn;
	}

	void CameraManager::ToThirdPersonState(bool&& a_fromFirstPerson)
	{
		// Switch back to the third person state. 
		// Special handling for transitions from the first person state.

		if (!playerCam)
		{
			return;
		}

		// Need to wait for the camera to fully transition to the FP state 
		// before toggling back to the TP state.
		// Otherwise, the player's FP arms will stick around 
		// and their TP skeleton will be invisible.
		// Hacky, but it works well enough.
		if (a_fromFirstPerson)
		{
			{
				std::unique_lock<std::mutex> togglePOVLock
				(
					camTogglePOVMutex, std::try_to_lock
				);
				if (togglePOVLock)
				{
					DBG("Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					isTogglingPOV = true;
				}
				else
				{
					// Could not obtain lock to toggle POV, 
					// so return here without enqueueing any tasks.
					DBG("Failed to obtain lock: (0x{:X})",
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					return;
				}
			}

			glob.taskRunner->AddTask
			(
				"[CAM]",
				__FUNCTION__,
				[this]() 
				{
					auto controlMap = RE::ControlMap::GetSingleton(); 
					if (!controlMap)
					{
						return;
					}

					if (auto ue = RE::UserEvents::GetSingleton(); ue)
					{
						// Wait, toggle to TP state, then wait again.
						std::this_thread::sleep_for(1s);
						Util::AddSyncedTask
						(
							[this, controlMap]() 
							{
								playerCam->lock.Lock();
								playerCam->ForceThirdPerson();
								playerCam->UpdateThirdPerson(true);
								playerCam->lock.Unlock();
							}
						);
						std::this_thread::sleep_for(1s);
					}

					DBG
					(
						"Getting lock from global task runner. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id())
					);
					{
						std::unique_lock<std::mutex> togglePOVLock(camTogglePOVMutex);
						isTogglingPOV = false;
					}
				}
			);
		}
		else
		{
			// Force switch to TP state here does not produce any problems,
			// since there are no FP arms to unload first.
			isTogglingPOV = true;
			playerCam->lock.Lock();
			playerCam->ForceThirdPerson();
			playerCam->UpdateThirdPerson(true);
			playerCam->lock.Unlock();
			isTogglingPOV = false;
		}
	}

	void CameraManager::UpdateAutoRotateAngleMult()
	{
		// Update the auto-rotation angles' (pitch/yaw) multiplier.
		
		// No change to auto-rotate angle if auto-rotate is not enabled 
		// or if there are no restrictions.
		if ((!Settings::bAutoRotateCamPitch && !Settings::bAutoRotateCamYaw) ||
			(Settings::uAutoRotateCriteria == !CamAutoRotateCriteria::kNoRestrictions))
		{
			movementAngleMultInterpData->value = 1.0f;
			return;
		}

		// A player is in combat (no focal players).s
		const bool partyCamInCombat = glob.isInCoopCombat && focalPID == -1 && softFocalPID == -1;
		// Player is moving their crosshair.
		bool playerMovingCrosshair = false;
		// No auto-rotate while there is a focal player.
		bool noAutoRotationWithFocalPlayer = false;
		// Is the current player performing a combat action?
		bool isPerformingCombatAction =  false;
		// Should suspend auto-rotation and interp towards 0 if a player is adjusting the camera
		// to avoid interfering.
		bool shouldSuspend = IsAdjustingCamera();
		if (shouldSuspend)
		{
			movementAngleMultInterpData->Reset(true, true);
			return;
		}

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
		
			// By default, suspend if the focal player is not sprinting 
			// and does not have a crosshair target.
			noAutoRotationWithFocalPlayer |= 
			(
				focalPID != -1 && 
				p->playerID == focalPID && 
				!p->pam->isSprinting && 
				!Util::HandleIsValid(p->tm->crosshairRefrHandle)
			);
			shouldSuspend |= noAutoRotationWithFocalPlayer;
			// No need to check the rest of the players.
			// Set directly to 0 and return.
			if (shouldSuspend)
			{
				if (!Util::HandleIsValid(p->tm->crosshairRefrHandle))
				{
					movementAngleMultInterpData->Reset(true, true);
				}
				else
				{
					movementAngleMultInterpData->UpdateInterpolatedValue(false);
				}

				return;
			}

			// To avoid affecting player aim, 
			// suspend auto-rotation when in combat, when a player is moving their crosshair,
			// or the focal player is not sprinting and facing the crosshair.
			playerMovingCrosshair |= 
			(
				(p->pam->IsPerforming(InputAction::kMoveCrosshair)) ||
				(
					p->pam->GetSecondsSinceLastStop(InputAction::kMoveCrosshair) <= 
					2.0f * Settings::fSecsDefFlickInterval 
				)
			);
			if (Settings::uAutoRotateCriteria == !CamAutoRotateCriteria::kAllRestrictions)
			{
				shouldSuspend |= partyCamInCombat || playerMovingCrosshair;
			}
			else if (Settings::uAutoRotateCriteria == !CamAutoRotateCriteria::kNoCrosshairMovement)
			{
				shouldSuspend |= playerMovingCrosshair;
			}
			else if (Settings::uAutoRotateCriteria == !CamAutoRotateCriteria::kOutsideOfCombat)
			{
				shouldSuspend |= partyCamInCombat;
			}

			// Only approach the full auto-rotate angle if no players
			// are attacking, bashing, blocking, casting, or performing any combat-related actions.
			isPerformingCombatAction = 
			{ 
				(p->pam->isAttacking || p->pam->isBlocking ||
				 p->pam->isBashing || p->pam->isInCastingAnim) 
			};
			if (!isPerformingCombatAction && p->coopActor->IsWeaponDrawn())
			{
				const auto& combatGroup = 
				(
					glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at(ActionGroup::kCombat)
				);
				for (auto actionIndex : combatGroup)
				{
					isPerformingCombatAction |= 
					(
						p->pam->IsPerforming(static_cast<InputAction>(actionIndex))
					);
					if (isPerformingCombatAction)
					{
						break;
					}
				}
			}

			shouldSuspend |= focalPID == -1 && softFocalPID == -1 && isPerformingCombatAction;
			if (shouldSuspend)
			{
				movementAngleMultInterpData->UpdateInterpolatedValue(false);
				return;
			}
		}
		
		// Interpolate towards 1 if we're not suspending auto-rotation.
		movementAngleMultInterpData->UpdateInterpolatedValue(true);
	}

	void CameraManager::UpdateFOV()
	{
		// Update the FOV to set for the camera.
		// Dependent on the type of the current player parent cell and whether or not 
		// the camera is in the dialogue state.
		// First, set FOV.
		float targetFOV = camFOV;
		if (exteriorCell) 
		{
			targetFOV = Settings::fCamExteriorFOV;
		}
		else
		{
			targetFOV = Settings::fCamInteriorFOV;
		}
		
		float tRatio = camInterpFactor;
		// Zoomed-in FOV to set when moving to the starting position or maintaining focus
		// on the dialogue target while the special dialogue camera is enabled.
		if ((inDialogueCamState && Settings::bDialogueCamEnabled) && 
			(movingToDialogueStartPos || !adjustedAfterReachingDialoguePos))
		{
			targetFOV *= Settings::fDialogueCamFOVRatio;
			tRatio = min
			(
				1.0f, Util::GetElapsedSeconds(dialogueCameraTP) / secsCamDialogueStartTransition
			);
		}

		if (camFOV != targetFOV)
		{
			camFOV = Util::InterpolateSmootherStep(camFOV, targetFOV, tRatio);
		}
	}

	void CameraManager::UpdateCamHeight()
	{
		// Update the camera focus point's Z offset, or 'height' above the origin point.
		
		// No height offset to set when manually positioned.
		if (isManuallyPositioned)
		{
			return;
		}
		
		if (movingToDialogueStartPos)
		{
			// Reset and do not change when moving the camera on rails to the initial position
			// when in dialogue.
			camBaseHeightOffset = 0.0f;
			return;
		}

		// Can adjust height if:
		// 1. There is no focal player -AND-
		// 2. Not locked on or if there is no target or if zoom controls are enabled -AND-
		// 3. A player is controlling the camera and trying to adjust the height.
		auto camLockOnTargetPtr = Util::GetRefrPtrFromHandle(camLockOnTargetHandle);
		bool canAdjustHeight = 
		{
			(IsAdjustingZoom()) &&
			(
				!isLockedOn || 
				!camLockOnTargetPtr || 
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
			)
		};
		// Save previous base height offset to restore later if the anchor points are bound.
		float prevBaseOffset = camBaseHeightOffset;
		if (canAdjustHeight)
		{
			// Can use the LS, so we have to check the camera adjustment bind.
			const auto& p = glob.coopPlayers[adjustingCamPID];
			const auto& paramsList = p->pam->paParamsList;
			const auto& stickData = glob.cdh->GetAnalogStickState
			(
				p->deviceID, 
				(
					paramsList[!InputAction::kZoomCam - !InputAction::kFirstAction].inputMask &
					(1 << !InputAction::kLS)
				) == (1 << !InputAction::kLS)
			);
			const auto& stickX = stickData.xComp;
			const auto& stickY = stickData.yComp;
			const auto& stickMag = stickData.normMag;
			// Change height of the focus point if the x comp is larger than the y comp.
			if (fabsf(stickX) > fabsf(stickY))
			{
				// Right to increase height, left to decrease.
				camBaseHeightOffset += 
				(
					stickX * stickMag * camMaxMovementSpeed * *g_deltaTimeRealTime
				);
			}
		}
		else if (isLockedOn && 
				 camLockOnTargetPtr && 
				 Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kFull)
		{
			// Origin point already offset by average player height.
			// Auto-set z offset to reach feet level or up to 4 body-lengths above feet level 
			// when the target is at the max upward/downward pitch relative to the origin point.
			float originPitchToTarget = Util::NormalizeAngToPi
			(
				Util::GetPitchBetweenPositions(camOriginPoint, camLockOnFocusPoint)
			);
			float newZOffset = std::lerp
			(
				0.0f, 
				avgPlayerHeight, 
				(originPitchToTarget / (PI / 2.0f) + 1.0f) / 2.0f
			);
			camBaseHeightOffset = newZOffset;	
		}

		float prevHeight = camHeightOffset;
		float newHeight = camBaseHeightOffset;

		// NOTE:
		// Keeping for now if the no-collisions origin point does not work out.
		/*
		float currentFocusZPos = 
		(
			focalPID == -1 ? 
			camOriginPoint.z + newHeight :
			camRefrFocusPoint.z + newHeight
		);
		float boundsDiff = fabsf(camMaxAnchorPointZCoord - camMinAnchorPointZCoord);
		bool isBound = false;

		// Clamp the height offset to force the focus point between the anchor point bounds
		// when camera collisions are active.
		if (camCollisions)
		{
			if ((boundsDiff < camAnchorPointHullSize) && 
			(
				currentFocusZPos < camMinAnchorPointZCoord || 
				currentFocusZPos > camMaxAnchorPointZCoord
			))
			{
				// Offset to the point equidistant between the two bounds.
				newHeight = 
				(
					camMinAnchorPointZCoord + boundsDiff / 2.0f - camOriginPoint.z
				);
				isBound = true;
			}
			else
			{
				if (currentFocusZPos > camMaxAnchorPointZCoord)
				{
					// Offset below the upper bound.
					newHeight = min
					(
						newHeight, 
						camMaxAnchorPointZCoord - 
						camOriginPoint.z - 
						camAnchorPointHullSize
					);
					isBound = true;
				}

				if (currentFocusZPos < camMinAnchorPointZCoord)
				{
					// Offset above the lower bound.
					newHeight = max
					(
						newHeight, 
						camMinAnchorPointZCoord - 
						camOriginPoint.z + 
						camAnchorPointHullSize
					);
					isBound = true;
				}
			}
		}
		*/

		// Approach the new height offset.
		camHeightOffset = Util::InterpolateSmootherStep
		(
			prevHeight, newHeight, camInterpFactor
		);

		// Cap the base height offset too when attempting to move beyond the anchor point bounds.
		// Do not want to adjust the base offset while this is occurring
		// since the base offset adjustment will have no effect on the true height offset
		// and the player would have to adjust the base offset back into the bounded range
		// before the actual camera height offset changes
		// (delayed, with no visual indication that it is changing).
		/*if (isBound)
		{
			camBaseHeightOffset = prevBaseOffset;
		}*/
	}

	void CameraManager::UpdateCamRotation()
	{
		// Update the base and current camera pitch and yaw to set.
		
		// IF the special dialogue camera is enabled, point the camera from the behind the listener 
		// to the dialogue target or current speaker.
		if (inDialogueCamState && Settings::bDialogueCamEnabled)
		{
			const auto& dialogueP = glob.coopPlayers[glob.menuPID];
			const auto dialogueTargetPtr = camDialogueTargetHandle.get();
			const auto asActor = dialogueTargetPtr->As<RE::Actor>();
			const bool targetIsSpeaking = 
			(
				!Util::HandleIsValid(camDialogueSpeakerHandle) ||
				camDialogueSpeakerHandle == camDialogueTargetHandle
			);
			auto speakerPos = RE::NiPoint3();
			if (targetIsSpeaking || !Settings::bDialogueCamSwitchSpeakers)
			{
				speakerPos = 
				(
					dialogueTargetPtr->As<RE::Actor>() ? 
					Util::GetHeadPosition(dialogueTargetPtr->As<RE::Actor>()) : 
					Util::GetRefrPosition(dialogueTargetPtr.get())
				);
			}
			else
			{
				speakerPos = Util::GetHeadPosition(dialogueP->coopActor.get());
			}

			float xyDistToTarget = Util::GetXYDistance(camTargetPos, speakerPos);
			// Default radius at which to start slowing rotation.
			float radius = min
			(
				Settings::fTargetAttackSourceDistToSlowRotation, 
				Settings::fAutoEndDialogueRadius
			);
			// Slow down when within a multiple of the player actor's bounds.
			auto player3DPtr = Util::GetRefr3D(dialogueP->coopActor.get()); 
			if (player3DPtr &&
				!isnan(player3DPtr->worldBound.radius) &&
				!isinf(player3DPtr->worldBound.radius) &&
				!isnan(player3DPtr->worldBound.center.Length()) &&
				!isinf(player3DPtr->worldBound.center.Length()))
			{
				radius = player3DPtr->worldBound.radius * 4.0f;
			}
			else
			{
				DBG("ERR: {}'s worldbound is corrupted.", dialogueP->coopActor->GetName());
			}

			const float startingTargetPitch = Util::GetPitchBetweenPositions
			(
				camRefrFocusPoint, speakerPos
			);
			const float startingTargetYaw = Util::GetYawBetweenPositions
			(
				camRefrFocusPoint, speakerPos
			);
			float pitchDelta = Util::NormalizeAngToPi(startingTargetPitch - camPitch);
			float yawDelta = Util::NormalizeAngToPi(startingTargetYaw - camYaw);
			float tRatio = camInterpFactor;
			float rotMult = 1.0f;
			// 15 frames to rotate quickly and face the target.
			const bool turnQuicklyToFaceSpeaker = 
			(
				Settings::bDialogueCamSwitchSpeakers && 
				!movingToDialogueStartPos && 
				Util::GetElapsedSeconds(dialogueSpeakerChangedTP) <= 15.0f * *g_deltaTimeRealTime
			);
			if (turnQuicklyToFaceSpeaker)
			{
				rotMult = 1.0f;
				tRatio = 
				(
					Settings::bDialogueCamFocusSwitchSmoothing ? 
					1.0f - powf(0.95f, 60.0f * *g_deltaTimeRealTime) : 
					1.0f
				);
			}
			else
			{
				rotMult = Util::InterpolateEaseOut
				(
					0.0f, 1.0f, min(1.0f, xyDistToTarget / max(0.01f, radius)), 5.0f
				);
				tRatio = 
				(
					movingToDialogueStartPos ? 
					min
					(
						1.0f,
						Util::GetElapsedSeconds(dialogueCameraTP) / secsCamDialogueStartTransition
					) :
					camInterpFactor
				);
			}
			
			pitchDelta = Util::InterpolateSmootherStep(0.0f, rotMult * pitchDelta, tRatio);
			yawDelta = Util::InterpolateSmootherStep(0.0f, rotMult * yawDelta, tRatio);

			// Apply the deltas for this frame.
			camPitch = Util::NormalizeAngToPi(camPitch + pitchDelta);
			camYaw = Util::NormalizeAng0To2Pi(camYaw + yawDelta);
			
			// Set equal to cam yaw angle to set.
			camTargetPosPitch = camBaseTargetPosPitch = camPitch;
			camTargetPosYaw = camBaseTargetPosYaw = camYaw;

			// Set all to P1's pitch/yaw if invalid.
			if (isnan(camTargetPosYaw) || isinf(camTargetPosYaw))
			{
				camYaw = 
				camTargetPosYaw = 
				camBaseTargetPosYaw = glob.player1Actor->data.angle.z;
			}

			if (isnan(camTargetPosPitch) || isinf(camTargetPosPitch))
			{
				camPitch = 
				camTargetPosPitch = 
				camBaseTargetPosPitch = glob.player1Actor->data.angle.x;
			}
			
			return;
		}
		
		const bool isAdjustingRotation = IsAdjustingRotation();
		// Cap rotation speed.
		float maxRotRads = camMaxAngRotRate * *g_deltaTimeRealTime;
		// Changes in pitch/yaw to apply.
		auto pitchDelta = 0.0f;
		auto yawDelta = 0.0f;
		float rsX = 0.0f;
		float rsY = 0.0f;
		float rsMag = 0.0f;
		if (isAdjustingRotation)
		{
			// Right stick displacement components and magnitude.
			const auto& rsData = glob.cdh->GetAnalogStickState
			(
				glob.coopPlayers[adjustingCamPID]->deviceID, false
			);
			rsX = rsData.xComp;
			rsY = rsData.yComp;
			// Squared to allow for more precision when slightly displaced from center.
			rsMag = powf(rsData.normMag, 2.0f);
		}

		// Can still manually rotate the camera if there is no lock-on target
		// or if the lock-on assistance is set to zoom only, or if there is a focal player.
		auto camLockOnTargetPtr = Util::GetRefrPtrFromHandle(camLockOnTargetHandle);
		if (isAutoTrailing || 
			isManuallyPositioned || 
			!camLockOnTargetPtr || 
			Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
		{
			camMaxPitchAngMag = isAutoTrailing ? autoTrailPitchMax : PI / 2.0f;
			if (isAdjustingRotation)
			{
				// Moving the RS left or right causes counterclockwise or
				// clockwise rotation of the camera.
				yawDelta = maxRotRads * rsX * rsMag;
				camBaseTargetPosYaw += yawDelta;
				// Moving the RS up or down causes the camera to pitch
				// upward or downward.
				// Upward results in a negative pitch change,
				// downward results in a positive pitch change, 
				// so we flip the sign.
				pitchDelta = -maxRotRads * rsY * rsMag;
				camBaseTargetPosPitch += pitchDelta;

				// Update auto-pitch request flat when rotating.
				reqToAutoPitch = fabsf(camBaseTargetPosPitch) < Settings::fCamAutoPitchReqAngMag;
			}
		}

		// Cam pitch is clamped to +- the pre-determined max pitch magnitude.
		if (!isnan(camBaseTargetPosPitch) && !isinf(camBaseTargetPosPitch))
		{
			camBaseTargetPosPitch = std::clamp
			(
				camBaseTargetPosPitch, 
				-camMaxPitchAngMag, 
				camMaxPitchAngMag
			);
		}
		else
		{
			camBaseTargetPosPitch = 0.0f;
		}

		// Ensure that the base target position pitch and yaw are valid before using them
		// as a basis for other pitch and yaw calculations.
		if (!isnan(camBaseTargetPosYaw) && !isinf(camBaseTargetPosYaw))
		{
			camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw);
		}
		else
		{
			camBaseTargetPosYaw = glob.player1Actor->data.angle.z;
		}

		// Blend target and to-focus rotation angles if auto-trailing 
		// or in partially automated lock-on state.
		if ((!isManuallyPositioned) && 
			(
				isAutoTrailing || !camLockOnTargetPtr
			))
		{
			// Apply movement pitch deltas calculated above.
			float movementPitchDelta = 0.0f;
			float movementYawDelta = 0.0f;
			// Set directly to target pitch/yaw if not auto-rotating.
			if (!Settings::bAutoRotateCamPitch && !Settings::bAutoRotateCamYaw)
			{
				camTargetPosPitch = std::clamp
				(
					camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag
				);
				camTargetPosYaw = 
				camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw);
			}
			else
			{
				UpdateAutoRotateAngleMult();
				if (Settings::bAutoRotateCamPitch && !isAdjustingRotation && reqToAutoPitch)
				{
					const float framesToReachFullDelta = 
					(
						120.0f * ((1.0f / *g_deltaTimeRealTime) / 60.0f)
					);
					const float decayFactor = 30.0f;
					const float currentOffset = movementPitchOffset;
					movementPitchOffset = GetAutoRotateAngle(true);
					movementPitchOffset = Util::NormalizeAngToPi
					(
						Util::InterpolateSmootherStep
						(
							currentOffset, 
							movementPitchOffset, 
							camInterpFactor
						)
					);
					float radialDistFactor = 1.0f; 
					if (focalPID == -1)
					{
						radialDistFactor = Util::InterpolateEaseInEaseOut
						(
							1.0f,
							0.0f,
							min
							(
								1.0f,
								camMinTrailingDistance / 
								max
								(
									camMinTrailingDistance, 
									camTargetPos.GetDistance
									(
										camCentroidPoint + RE::NiPoint3(0.0f, 0.0f, avgPlayerHeight)
									)
								)
							),
							2.0f
						);
					}

					// Pitch increments/decrements are smaller when approaching PI/2 
					// in the direction of the average movement pitch. 
					// Done to prevent over-adjustment when already at a steep pitch.
					const float proportionOfMaxPitch = Util::InterpolateSmootherStep
					(
						0.0f,
						1.0f,
						min(1.0f, fabsf(camBaseTargetPosPitch / (camMaxPitchAngMag)))
					);
					pitchDelta = 
					(
						movementPitchOffset * 
						std::clamp
						(
							(
								movementAngleMultInterpData->value * 
								Settings::fAutoRotateCamPitchRateMult *
								(1.0f - proportionOfMaxPitch) *
								radialDistFactor
							) / framesToReachFullDelta,
							0.0f,
							1.0f
						)
					);
					
					camTargetPosPitch = 
					camBaseTargetPosPitch = 
					std::clamp
					(
						camBaseTargetPosPitch + pitchDelta, -camMaxPitchAngMag, camMaxPitchAngMag
					);
				}
				else
				{
					camTargetPosPitch = camBaseTargetPosPitch; 
					movementPitchOffset = 0.0f;
				}

				if (Settings::bAutoRotateCamYaw && !isAdjustingRotation)
				{
					const float framesToReachFullDelta = 
					(
						10.0f * ((1.0f / *g_deltaTimeRealTime) / 60.0f)
					);
					const float decayFactor = 30.0f;
					const float currentOffset = movementYawOffset;
					movementYawOffset = GetAutoRotateAngle(false);
					movementYawOffset = Util::NormalizeAngToPi
					(
						Util::InterpolateSmootherStep
						(
							currentOffset, 
							movementYawOffset, 
							camInterpFactor
						)
					);
					float radialDistFactor = 1.0f; 
					if (focalPID == -1)
					{
						// Slow down auto rotation when players are far apart.
						radialDistFactor = Util::InterpolateEaseInEaseOut
						(
							0.1f,
							1.0f,
							min
							(
								1.0f,
								camMinTrailingDistance / 
								max
								(
									camMinTrailingDistance, 
									camTargetPos.GetDistance
									(
										camCentroidPoint + RE::NiPoint3(0.0f, 0.0f, avgPlayerHeight)
									)
								)
							),
							2.0f
						);
					}

					yawDelta = 
					(
						movementYawOffset * 
						std::clamp
						(
							(
								movementAngleMultInterpData->value * 
								Settings::fAutoRotateCamYawRateMult * 
								radialDistFactor
							) / 
							framesToReachFullDelta,
							0.0f,
							1.0f
						)
					);
					camTargetPosYaw =
					camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw + yawDelta);
				}
				else
				{
					camTargetPosYaw = camBaseTargetPosYaw;
					movementYawOffset = 0.0f;
				}
			}

			if (focalPID != -1) 
			{
				camCurrentPitchToFocus = Util::NormalizeAngToPi
				(
					Util::GetPitchBetweenPositions(camTargetPos, camRefrFocusPoint)
				);
				camCurrentYawToFocus = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions(camTargetPos, camRefrFocusPoint)
				);
			}
			else
			{
				camCurrentPitchToFocus = Util::NormalizeAngToPi
				(
					Util::GetPitchBetweenPositions(camTargetPos, camFocusPoint)
				);
				camCurrentYawToFocus = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions(camTargetPos, camFocusPoint)
				);
			}

			float oldPitch = camPitch;
			float oldYaw = camYaw;
			if (camCollisions)
			{
				// TL;DR: Blend pitch/yaw when the camera is moving quickly 
				// relative to the focus point, meaning the angle from the target position 
				// to the focus point is rapidly changing, making training the camera 
				// on the focus point directly a bad idea. 
				// 
				// I may be overthinking this and missing an obvious solution,
				// but this is the best I can do for now. 
				// 
				// If the below trailing distance and/or pitch factors are large, 
				// this means that the current camera target position is either
				// close to the focus point or pitched sharply with respect to the focus point.
				// In either case, large changes in pitch/yaw occur since we are trying to 
				// angle the camera at the focus point, ideally at all times.
				// For example, if the focus point moves behind the target position,
				// the camera will attempt to flip and face the focus, 
				// which to put it lightly, is jarring and nauseating.
				// With this in mind, and without an exact mathematical solution 
				// to find when the more stable and focus-independent 
				// target position pitch/yaw readings should be switched to, 
				// some rough blending is required to bounce between 
				// the two sets of pitch/yaw readings.
				//
				// 1. Stick to the focus point-relative readings 
				// when the trailing distance and pitch factors are small.
				// 2. Approaching the target pos pitch/yaw readings 
				// when the base radial distance approaches the min radial distance,
				// or the base camera pitch magnitude approaches the max allowable pitch magnitude, 
				// both "danger zones" where the pitch/yaw to the focus point changes rapidly.
				// 3. The max pitch is also capped to prevent the camera 
				// from getting too close to the focus point in the XY plane.
				// NOTE: 
				// As a result of this imperfect blending, 
				// a small hitch results when moving towards and then away from max pitch,
				// but it is, in my opinion, an acceptable tradeoff for smoother camera rotation 
				// that nearly stays affixed to the focus point, 
				// regardless of camera collisions and distance to the focus point.

				float xyTrailingDist = max(0.1f, Util::GetXYDistance(camTargetPos, camFocusPoint));
				// Larger when the camera's XY distance to the focus point is small.
				float trailingDistFactor = 
				(
					powf(1.0f - (xyTrailingDist - camMinTrailingDistance) / xyTrailingDist, 0.5f)
				);
				// Interpolation power and ratio to raise to that power
				// when computing the next pitch/yaw to set.
				const float interpPower = 9.0f;
				float interpRatio = prevRotInterpRatio;
				if (focalPID != -1 || isColliding) 
				{
					// Quickly reach the target position pitch/yaw
					// when there is a focal player.
					// Less volatile to use the target position
					// rotations here instead of the pitch/yaw
					// to the player focus point,
					// which varies greatly given that the camera
					// is typically very close to the focal player,
					// especially in interior cells.
					interpRatio += (1.0f - prevRotInterpRatio);
				}
				else if (camAdjMode != CamAdjustmentMode::kRotate)
				{
					// Ease into an interp ratio of 1 (target pos pitch/yaw) when not rotating 
					// or when the camera is moving through/along an obstruction.
					interpRatio += (1.0f - prevRotInterpRatio) / 10.0f;
				}
				else
				{
					// Catch up to target interp ratio fast when rotating.
					float diff = (min(trailingDistFactor, 1.0f) - prevRotInterpRatio);
					// Slower shift towards pitch/yaw to focus point (interp ratio decreasing).
					if (diff < 0.0f)
					{
						diff /= 10.0f;
					}

					interpRatio += diff;
				}

				interpRatio = Util::InterpolateSmootherStep
				(
					prevRotInterpRatio, interpRatio, camInterpFactor
				);
				// 'Ease in' from to-focus rotations to target pos rotations.
				const float finalInterpFactor = powf(interpRatio, interpPower);

				float targetPitch = Util::NormalizeAngToPi
				(
					Util::InterpolateSmootherStep
					(
						camCurrentPitchToFocus, camTargetPosPitch, finalInterpFactor
					)
				);

				float yawStart = camCurrentYawToFocus;
				float yawEnd = camTargetPosYaw;
				// Set cam pitch/yaw to apply.
				float yawDiff = Util::NormalizeAngToPi(yawEnd - yawStart);
				// Interpolate the diff instead of the target yaw
				// to avoid issues with the interpolation taking the longer path to the target yaw.
				float targetYaw = Util::NormalizeAng0To2Pi
				(
					yawStart + Util::InterpolateSmootherStep(0.0f, yawDiff, finalInterpFactor)
				);
				
				// Save our new values to set.
				camPitch = targetPitch;
				camYaw = targetYaw;
				prevRotInterpRatio = interpRatio;
			}
			else
			{
				// All set to base when not using camera collisions.
				camPitch =
				camTargetPosPitch = std::clamp
				(
					camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag
				);
				camYaw = camTargetPosYaw = camBaseTargetPosYaw;
			}
		}
		else if (isLockedOn && camLockOnTargetPtr)
		{
			// NOTE: 
			// Temporarily using the base target position as the rotation target 
			// to lessen camera jumping relative to a rapidly changing collision focus point.
			auto camLockOnTargetPtr = Util::GetRefrPtrFromHandle(camLockOnTargetHandle);
			if (!camLockOnTargetPtr)
			{
				// No target. Bye.
				return;
			}
			
			const bool zoomMode = 
			(
				Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
			);
			const bool isAdjustingRotation = IsAdjustingRotation();
			if (zoomMode)
			{
				// No auto rotation when player is rotating the camera.
				if (isAdjustingRotation)
				{
					lockOnShouldAutoRotate = false;
				}
			}
			else
			{
				facingLockOnTarget = true;
				lockOnShouldAutoRotate = true;
			}

			// Set the camera orientation to the previous frame's orientation,
			// since it may have been modified since the start of this iteration of the main task.
			const RE::NiPoint3 targetScreenPos =  Util::WorldToScreenPoint3
			(
				zoomMode && !facingLockOnTarget ? camFocusPoint : camLockOnFocusPoint, false
			);
			// Rotate to reach this screen position and line up the target's position with it.
			const RE::NiPoint3 screenFocusPos = 
			(
				zoomMode && !facingLockOnTarget ? 
				Util::WorldToScreenPoint3(camLockOnFocusPoint, false) :
				zoomMode && facingLockOnTarget ? 
				Util::WorldToScreenPoint3(camFocusPoint, false) :
				RE::NiPoint3
				(
					0.5f * DebugAPI::screenResX,
					0.5f * DebugAPI::screenResY,
					-1.0f
				)
			);
			const RE::NiPoint3 lockOnFocusScreenPoint = Util::WorldToScreenPoint3
			(
				camLockOnFocusPoint, false
			);
			if (lockOnShouldAutoRotate)
			{
				// No auto pitch when locked on in zoom mode without cam collisions
				// because the cam will phase through the ground if the target is above the party.
				if (!zoomMode || camCollisions)
				{
					bool adjustingHeight = 
					(
						camAdjMode == CamAdjustmentMode::kZoom && 
						fabsf(rsX) > fabsf(rsY) &&
						Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
					);
					float targetPitch = 0.0f;
					if (zoomMode)
					{
						// Pitch to face the lock on target or the party focus point,
						// depending on facing direction.
						const float maxPitchMult = facingLockOnTarget ? 0.2f : 0.4f;
						targetPitch = std::clamp
						(
							facingLockOnTarget ? 
							Util::GetPitchBetweenPositions(camFocusPoint, camLockOnFocusPoint) :
							Util::GetPitchBetweenPositions(camLockOnFocusPoint, camFocusPoint),
							-maxPitchMult * camMaxPitchAngMag,
							maxPitchMult * camMaxPitchAngMag
						);
					}
					else
					{
						// Uhh. Approach the pitch to lock on focus point when the lock on target 
						// is near the top/bottom of the screen to better ensure it stays on screen.
						const float pitchToOrigin =
						(
							Util::GetPitchBetweenPositions(camTargetPos, camOriginPoint)
						);
						const float pitchToLockOnFocusPoint =
						(
							Util::GetPitchBetweenPositions(camTargetPos, camLockOnFocusPoint)
						);
						targetPitch = Util::NormalizeAngToPi
						(
							(
								std::lerp
								(
									pitchToOrigin,
									pitchToLockOnFocusPoint,
									std::clamp
									(
										targetScreenPos.y <= 0.5f * DebugAPI::screenResY ? 
										1.0f - (targetScreenPos.y / (0.5f * DebugAPI::screenResY)) :
										(targetScreenPos.y / (0.5f * DebugAPI::screenResY)) - 1.0f,
										0.0f, 
										1.0f
									)
								)
							) * 
							(adjustingHeight ? 1.0f : 0.333333f)
						);
					}

					// Cap max change in pitch/yaw to the maximum rotatable angle this frame,
					// based on the camera's set max rotation speed.
					pitchDelta = std::clamp
					(
						Util::NormalizeAngToPi(targetPitch - camPitch), 
						-maxRotRads, 
						maxRotRads
					);
					// Slow down pitch change when the target is close to the camera.
					float pitchDepthMult = std::clamp
					(
						max(0.0f, targetScreenPos.z - 0.9f) * 10.0f,
						0.0f,
						1.0f
					);
					// Multiplier for the pitch  to rotate in order to directly face the target.
					float pitchDeltaMult = 1.0f;
					// Essentially, start pitching to face the target faster when beyond the edges
					// of the screen.
					if (targetScreenPos.y < 0.0f)
					{
						pitchDeltaMult = Util::InterpolateEaseOut
						(
							0.15f,
							1.0f, 
							std::clamp
							(
								-targetScreenPos.y / (0.1f * DebugAPI::screenResY),
								0.0f, 
								1.0f
							),
							3.0f
						);
					}
					else if (targetScreenPos.y > DebugAPI::screenResY)
					{
						pitchDeltaMult = Util::InterpolateEaseOut
						(
							0.15f,
							1.0f, 
							std::clamp
							(
								10.0f * (targetScreenPos.y / DebugAPI::screenResY - 1.0f), 
								0.0f, 
								1.0f
							),
							3.0f
						);
					}
					else
					{
						pitchDeltaMult = Util::InterpolateEaseIn
						(
							0.0f,
							0.5f, 
							std::clamp
							(
								fabsf
								(
									targetScreenPos.y - screenFocusPos.y
								) / (0.5f * DebugAPI::screenResY),
								0.0f, 
								1.0f
							),
							3.0f
						);
					}
				
					pitchDelta *= std::clamp
					(
						pitchDepthMult * pitchDeltaMult, 0.0f, 1.0f
					);
				}
				
				float targetYaw = 0.0f;
				if (zoomMode)
				{
					// Turn to face the lock on target or the party focus point,
					// depending on facing direction.
					targetYaw = Util::NormalizeAng0To2Pi
					(
						facingLockOnTarget ? 
						Util::GetYawBetweenPositions(camFocusPoint, camLockOnFocusPoint) :
						Util::GetYawBetweenPositions(camLockOnFocusPoint, camFocusPoint)
					);
				}
				else
				{
					// Directly face the lock on focus point from the current target position.
					targetYaw = Util::NormalizeAng0To2Pi
					(
						Util::GetYawBetweenPositions(camTargetPos, camLockOnFocusPoint)
					);
				}

				yawDelta = std::clamp
				(
					Util::NormalizeAngToPi(targetYaw - camYaw), 
					-maxRotRads, 
					maxRotRads
				);
			
				// Rotate about the Z axis faster when the target is behind the camera.
				float yawDepthMult = max(1.0f, -targetScreenPos.z + 1.0f * 2.0f);
				// Multiplier for the yaw angle to rotate in order to directly face the target.
				float yawDeltaMult = 1.0f;
				// Slow down rotation when the target is on the lower half of the screen,
				// or in the middle of the screen in zoom mode.
				float vertScreenPosYawMult = 
				(
					zoomMode ? 
					Util::InterpolateSmootherStep
					(
						0.2f,
						1.0f,
						2.0f * 
						std::clamp
						(
							fabsf((targetScreenPos.y - screenFocusPos.y) / (DebugAPI::screenResY)),
							0.0f, 
							0.5f
						)
					) :
					Util::InterpolateEaseIn
					(
						0.333333f,
						1.0f,
						(
							2.0 * 
							std::clamp
							(
								-max
								(
									0.0f,
									(targetScreenPos.y - screenFocusPos.y) / (DebugAPI::screenResY)
								),
								-0.5f, 
								0.0f
							) + 1.0f
						),
						3.0f
					)
				);
				// Rotate to face the target faster when it is close to the vertical edges 
				// of the screen or when off the screen completely. 
				// Cancel out the vertical position factor when off screen as well.
				yawDeltaMult = Util::InterpolateEaseIn
				(
					0.0f,
					1.0f / vertScreenPosYawMult, 
					std::clamp
					(
						(
							fabsf(targetScreenPos.x - screenFocusPos.x) / 
							(0.5f * DebugAPI::screenResX)
						),
						0.0f, 
						1.0f
					),
					3.0f
				);

				yawDelta *= std::clamp
				(
					yawDepthMult * yawDeltaMult * vertScreenPosYawMult, 0.0f, 1.0f
				);
			}

			// Apply the deltas for this frame.
			camPitch = std::clamp
			(
				Util::NormalizeAngToPi(camPitch + pitchDelta), 
				-camMaxPitchAngMag, 
				camMaxPitchAngMag
			);
			camYaw = Util::NormalizeAng0To2Pi(camYaw + yawDelta);
			camTargetPosPitch = camBaseTargetPosPitch = camPitch;
			camTargetPosYaw = camBaseTargetPosYaw = camYaw;
		}
		else
		{
			// Set directly to target pos values 
			// when in a fully-automated lock-on or manually positioned state.
			camPitch = 
			camTargetPosPitch = std::clamp
			(
				camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag
			);
			camYaw = camTargetPosYaw = camBaseTargetPosYaw;
		}

		// Set all to P1's pitch/yaw if invalid.
		if (isnan(camTargetPosYaw) || isinf(camTargetPosYaw))
		{
			camYaw = 
			camTargetPosYaw = 
			camBaseTargetPosYaw = glob.player1Actor->data.angle.z;
		}

		if (isnan(camTargetPosPitch) || isinf(camTargetPosPitch))
		{
			camPitch = 
			camTargetPosPitch = 
			camBaseTargetPosPitch = glob.player1Actor->data.angle.x;
		}
	}

	void CameraManager::UpdateCamState()
	{
		// Check if the Skyrim camera perspective should switch to third person,
		// update the co-op camera state flags, and perform state transitions as needed.

		if (!playerCam)
		{
			return;
		}

		bool isInSupportedCamState = 
		(
			playerCam->currentState->id == RE::CameraState::kThirdPerson ||
			playerCam->currentState->id == RE::CameraState::kMount ||
			playerCam->currentState->id == RE::CameraState::kDragon ||
			playerCam->currentState->id == RE::CameraState::kFurniture ||
			playerCam->currentState->id == RE::CameraState::kBleedout
		);
		// Auto-switch back to the third person camera state 
		// if currently not in a supported state.
		if (!isTogglingPOV && playerCam->currentState && !isInSupportedCamState)
		{
			ToThirdPersonState(playerCam->currentState->id == RE::CameraState::kFirstPerson);
		}

		// Check if the camera should transition to the death/dialogue camera states,
		// which are event-driven and not user-selectable.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		bool switchToDeathState = 
		{
			(camState != CamState::kDeath) &&
			(
				p1 &&
				glob.globalDataInit && 
				glob.allPlayersInit &&
				glob.partyWiped	
			) &&
			(
				(glob.p1IsEssential && p1->IsBleedingOut()) || 
				(!glob.p1IsEssential && p1->IsDead())
			)
		};
		if (switchToDeathState)
		{
			camState = CamState::kDeath;
		}

		auto ui = RE::UI::GetSingleton();
		auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
		// Must have the dialogue menu open with a player in control and a recorded speaker.
		bool switchToDialogueState = 
		{
			(
				camState != CamState::kDialogue &&
				glob.coopSessionActive && 
				glob.menuPID >= 0 &&
				menuTopicManager &&
				ui &&
				ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)
			) && 
			(
				Util::HandleIsValid(menuTopicManager->speaker) ||
				Util::HandleIsValid(menuTopicManager->lastSpeaker)
			)
		};
		// Switch back to auto-trail if currently in the dialogue state
		// and no player is controlling menus, the dialogue menu has closed, 
		// or the speaker is no longer valid.
		bool switchBackToAutoTrail = 
		(
			(camState == CamState::kDialogue) &&
			(
				(glob.menuPID < 0) ||
				(ui && !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) ||
				(!Util::HandleIsValid(camDialogueTargetHandle))
			)
		);
		if (switchToDialogueState)
		{
			camState = CamState::kDialogue;
		}
		else if (switchBackToAutoTrail)
		{
			camState = CamState::kAutoTrail;
		}

		// Also switch back to auto trail if the lock on target becomes invalid.
		switchBackToAutoTrail = 
		(
			camState == CamState::kLockOn && !Util::HandleIsValid(camLockOnTargetHandle)
		);
		if (switchBackToAutoTrail)
		{
			camState = CamState::kAutoTrail;
		}

		// Update state flags.
		isAutoTrailing = camState == CamState::kAutoTrail;
		isLockedOn = camState == CamState::kLockOn;
		isManuallyPositioned = camState == CamState::kManualPositioning;
		inDeathCamState = camState == CamState::kDeath;
		inDialogueCamState = camState == CamState::kDialogue;
		// Update collisions flag.
		camCollisions = 
		(
			(
				Settings::bCamExteriorCollisions && exteriorCell
			) ||
			(
				Settings::bCamInteriorCollisions && !exteriorCell	
			)
		);

		// On state change, reset TPs, transition to new state.
		if (camState != prevCamState)
		{
			ResetTPs();
			PerformStateTransition();
		}
	}

	void CameraManager::UpdateCamZoom()
	{
		// Update the camera's zoom, auto-zooming out
		// to keep all players in view and auto- zooming in
		// when under an exterior roof, as necessary.

		// Zoom in instantly when transitioning to dialogue start position.
		// Maintain the zoom radial distance until adjusted.
		if ((inDialogueCamState && Settings::bDialogueCamEnabled) && 
			(movingToDialogueStartPos || !adjustedAfterReachingDialoguePos))
		{
			camRadialDistanceOffset = 0.0f;
			camMinTrailingDistance =
			camTargetRadialDistance = Settings::fDialogueCamZoomedInRadialDistance;
			return;
		}

		// Set the minimum trailing distance first.
		if (focalPID == -1)
		{
			camMinTrailingDistance = Settings::fCamMinTrailingDistance;	
		}
		else
		{
			camMinTrailingDistance = Settings::fFocalMinRadialDistance;
		}

		// No zoom when in manual positioning mode.
		if (isManuallyPositioned)
		{
			return;
		}
		
		float stickX = 0.0f;
		float stickY = 0.0f;
		float stickMag = 0.0f;
		// Auto-zoom in/out.
		const float prevRadialDistance = camTargetRadialDistance;
		// Zoom offset decreases (zoom in) when moving the RS up,
		// and increases (zoom out) when moving the RS down.
		// Behaves the same for all camera modes.
		// Only adjust base radial distance if requested.
		auto camLockOnTargetPtr = Util::GetRefrPtrFromHandle(camLockOnTargetHandle);
		// Can adjust zoom if:
		// 1. Not locked on or if there is no target or if zoom controls are enabled -AND-
		// 2. A player is controlling the camera and trying to adjust the zoom.
		bool canAdjustZoom = 
		{
			(IsAdjustingZoom()) &&
			(
				!isLockedOn || 
				!camLockOnTargetPtr || 
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
			)
		};
		if (canAdjustZoom)
		{
			const auto& p = glob.coopPlayers[adjustingCamPID];
			const auto& paramsList = p->pam->paParamsList;
			// Can use the LS, so we have to check the camera adjustment bind.
			const auto& stickData = glob.cdh->GetAnalogStickState
			(
				p->deviceID, 
				(
					paramsList[!InputAction::kZoomCam - !InputAction::kFirstAction].inputMask &
					(1 << !InputAction::kLS)
				) == (1 << !InputAction::kLS)
			);
			stickX = stickData.xComp;
			stickY = stickData.yComp;
			stickMag = stickData.normMag;
			if (fabsf(stickY) > fabsf(stickX))
			{
				// Reset the base radial distance when the camera is colliding.
				// Do not want to increase the base radial distance to zoom out 
				// when hitting an obstruction behind the camera
				// or decrease the base radial distance to zoom in 
				// when hitting an obstruction in front of the camera
				// since the base radial distance adjustment will have no effect 
				// on the true radial distance set after camera collision processing
				// and the player would have to adjust the base radial distance 
				// back into the bounded range before the true camera radial distance changes
				// (delayed, with no visual indication that it is changing).
				if (camCollisions && isColliding)
				{
					// Can adjust zoom when zooming in when colliding with a surface 
					// behind the camera, or zooming out when colliding with a surface 
					// in front of the camera.

					// If just moved from center, set to the true radial distance
					// before modifying the offset. This will prevent a delayed zoom response
					// as the offset approaches the true radial distance.
					if (glob.coopPlayers[adjustingCamPID]->pam->JustStarted
						(
							InputAction::kZoomCam
						))
					{
						camRadialDistanceOffset = max
						(
							0.0f,
							camTrueRadialDistance - camMinTrailingDistance
						);
					}

					// Can zoom in/out if the camera is moving away from the surface 
					// it is colliding with.
					const float epsilon = 1E-3f;
					const bool canZoom = 
					(
						(
							stickY > 0.0f &&
							camTrueRadialDistance <= camTargetRadialDistance + epsilon
						) ||
						(
							stickY < 0.0f &&
							camTrueRadialDistance >= camTargetRadialDistance - epsilon
						)
					);
					if (canZoom)
					{
						// Do not exceed the max camera movement speed when zooming in/out.
						camRadialDistanceOffset = max
						(
							0.0f, 
							camRadialDistanceOffset - 
							(*g_deltaTimeRealTime * camMaxMovementSpeed * stickY * stickMag)
						);
					}
				}
				else
				{
					// Do not exceed the max camera movement speed when zooming in/out.
					camRadialDistanceOffset = max
					(
						0.0f, 
						camRadialDistanceOffset - 
						(*g_deltaTimeRealTime * camMaxMovementSpeed * stickY * stickMag)
					);
				}
			}
		}
		else if (isLockedOn && 
				 camLockOnTargetPtr &&
				 Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kFull)
		{
			// Offset is kept at 0 when full lock-on assistance is enabled.
			camRadialDistanceOffset = 0.0f;
		}

		camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;

		// If not using auto-zoom, we can set the target radial distance directly here 
		// and return early.
		if (!Settings::bAutoAdjustCamZoom)
		{
			camTargetRadialDistance = camMinTrailingDistance + camRadialDistanceOffset;
			// Interp from previous.
			camTargetRadialDistance = Util::InterpolateSmootherStep
			(
				prevRadialDistance, camTargetRadialDistance, camInterpFactor
			);
			// Ensure the radial distance to set is never below the minimum trailing distance.
			if (!isColliding || camTargetRadialDistance < prevRadialDistance)
			{
				camTargetRadialDistance = max(camTargetRadialDistance, camMinTrailingDistance);
			}

			return;
		}

		// Raycast hits and on-screen checks seem inconsistent 
		// when zoomed out beyond a variable distance (likely the cell's clip distance).
		// Zooming out beyond this distance will result in the game 
		// considering all players offscreen (and no raycast hits),
		// even though their positions are in front of the camera and visually in view.
		// Get approximation for the max settable radial distance by binary searching a range.
		const uint32_t maxIterations = 
		(
			log2f(Settings::fMaxRaycastAndZoomOutDistance / Settings::fMinAutoZoomDelta) + 1
		);
		float radialDistanceRangeMin = 0.0f;
		float radialDistanceRangeMax = Settings::fMaxRaycastAndZoomOutDistance;
		float radialDistanceRangeMid = Settings::fMaxRaycastAndZoomOutDistance / 2.0f;
		float lastOnScreenRadialDist = Settings::fMaxRaycastAndZoomOutDistance;
		// Focus point is the party's focus point or the focal player/dialogue target's focus point.
		bool useFocalTargetFocusPoint = 
		(
			(focalPID != -1) || (inDialogueCamState && Settings::bDialogueCamEnabled)
		);
		bool useLockOnTargetFocusPoint = 
		(
			isLockedOn && 
			Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
		);
		auto focusPoint = camFocusPoint;
		if (useFocalTargetFocusPoint) 
		{
			focusPoint = camRefrFocusPoint;
		}
		else if (useLockOnTargetFocusPoint)
		{
			focusPoint = camLockOnFocusPoint;
		}

		auto dirFromFocus = camBaseTargetPos - focusPoint;
		dirFromFocus.Unitize();
		// Position from which to test for visibility of all players.
		auto onScreenTestPos = focusPoint + dirFromFocus * radialDistanceRangeMid;
		const RE::NiPoint2 rotationToCheck = RE::NiPoint2(camTargetPosPitch, camTargetPosYaw);
		bool minDiffReached = false;
		bool currentCheckOnScreen = false;
		bool usePlayerPositions = 
		(
			!Util::HandleIsValid(camLockOnTargetHandle) || 
			Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
		);
		uint32_t i = 0;
		// To be safe, we'll cap it to prevent an infinite loop.
		/*
		while (!minDiffReached && i <= maxIterations)
		{
			currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation
			(
				onScreenTestPos, rotationToCheck, usePlayerPositions
			);
			if (currentCheckOnScreen)
			{
				// On screen at the checked position, so attempt to zoom out more.
				radialDistanceRangeMin = 
				lastOnScreenRadialDist = radialDistanceRangeMid;
			}
			else
			{
				// Not on screen at the checked position, so zoom in.
				radialDistanceRangeMax = radialDistanceRangeMid;
			}

			radialDistanceRangeMid = (radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f;
			onScreenTestPos = focusPoint + dirFromFocus * (radialDistanceRangeMid);
			minDiffReached = 
			(
				radialDistanceRangeMax - radialDistanceRangeMin < Settings::fMinAutoZoomDelta
			);
			++i;
		}

		// Converged on a max zoom out distance.
		if (currentCheckOnScreen)
		{
			camMaxZoomOutDist = radialDistanceRangeMid;
		}
		else if (lastOnScreenRadialDist < Settings::fMaxRaycastAndZoomOutDistance)
		{
			camMaxZoomOutDist = lastOnScreenRadialDist;
		}
		else
		{
			// If the check failed to find a distance 
			// at which all players are on screen (range min is 0),
			// set to the previous radial distance to prevent jarring changes in zoom.
			camMaxZoomOutDist = max(prevRadialDistance, radialDistanceRangeMin);
		}
		*/

		// When outside, zoom in when all players are under a roof
		// or any protruding surface above their heads.
		auto tes = RE::TES::GetSingleton();
		auto sky = tes ? tes->sky : nullptr;
		// NOTE:
		// Some exterior cells have the interior sky mode,
		// and thus are not likely to have an exposed skybox above.
		// So we'll consider such cells as effectively interior ones and not auto-zoom in.
		bool outside = 
		(
			exteriorCell && 
			sky &&
			sky->mode != RE::Sky::Mode::kInterior
		);
		if (outside)
		{
			if (!useFocalTargetFocusPoint && !useLockOnTargetFocusPoint)
			{
				bool allPlayersUnderExteriorRoof = true;
				bool onePlayerUnderExteriorRoof = false;
				for (const auto& p : glob.coopPlayers)
				{
					if (!p->isActive)
					{
						continue;
					}

					glm::vec4 headPos = ToVec4(Util::GetHeadPosition(p->coopActor.get()));
					// Ignore actors and activators, since neither should affect 
					// the visibility of players or zoom in distance.
					auto aboveResult = Raycast::hkpCastRay
					(
						headPos, 
						headPos + glm::vec4(0.0f, 0.0f, 100000.0f, 0.0f),
						std::vector<RE::NiAVObject*>{ },
						{ RE::FormType::ActorCharacter, RE::FormType::Activator }
					);
					if (!aboveResult.hit)
					{
						allPlayersUnderExteriorRoof = false;
					}
					else
					{
						onePlayerUnderExteriorRoof = true;
					}
				}

				// Zoom in close when all players are under a roof.
				// Maintain zoom level (disable zooming out manually) 
				// while at least one player is under the roof.
				// Keeps the camera from stuttering as much 
				// from noisy raycast hits recorded when casting to a base target position 
				// well outside the roof that the players are under.
				if (allPlayersUnderExteriorRoof && 
					!delayedZoomInUnderExteriorRoof && 
					!delayedZoomOutUnderExteriorRoof)
				{
					// Save the originally-set base radial offset to restore later
					// once all players are no longer under the roof.
					camSavedRadialDistanceOffset = camRadialDistanceOffset;
					// Start zooming in.
					delayedZoomInUnderExteriorRoof = true;
					delayedZoomOutUnderExteriorRoof = false;
					underExteriorRoofZoomInTP = SteadyClock::now();
				}
				else if (delayedZoomInUnderExteriorRoof)
				{
					if (onePlayerUnderExteriorRoof && 
						Util::GetElapsedSeconds(underExteriorRoofZoomInTP) > 1.5f)
					{
						// Zoom in all the way when under a roof.
						camRadialDistanceOffset = 0.0f;
					}
					else if (!onePlayerUnderExteriorRoof && !delayedZoomOutUnderExteriorRoof)
					{
						// Start zooming out now that all players are no longer under the roof.
						delayedZoomOutUnderExteriorRoof = true;
						delayedZoomInUnderExteriorRoof = false;
						noPlayersUnderExteriorRoofTP = SteadyClock::now();
					}
				}
				else if (delayedZoomOutUnderExteriorRoof)
				{
					if (!onePlayerUnderExteriorRoof && 
						Util::GetElapsedSeconds(noPlayersUnderExteriorRoofTP) > 1.5f)
					{
						// Restore radial offset when no players are under a roof.
						camRadialDistanceOffset = camSavedRadialDistanceOffset;
						delayedZoomOutUnderExteriorRoof = false;
						delayedZoomInUnderExteriorRoof = false;
					}
					else if (onePlayerUnderExteriorRoof && !delayedZoomInUnderExteriorRoof)
					{
						// Start zooming in again if previously zooming out
						// but a player moves under the roof again.
						delayedZoomOutUnderExteriorRoof = false;
						delayedZoomInUnderExteriorRoof = true;
						underExteriorRoofZoomInTP = SteadyClock::now();
					}
				}
				else
				{
					// No changes to zoom, so simply update the TPs.
					underExteriorRoofZoomInTP = 
					noPlayersUnderExteriorRoofTP = SteadyClock::now();
				}
			}
			else
			{
				// No changes to apply when there is a focal player
				// or when the special dialogue camera is enabled.
				camSavedRadialDistanceOffset = camRadialDistanceOffset;
				delayedZoomOutUnderExteriorRoof = false;
				delayedZoomInUnderExteriorRoof = false;
				underExteriorRoofZoomInTP = noPlayersUnderExteriorRoofTP = SteadyClock::now();
			}
		}

		// Lock radial distance when loading into a new cell since the target position 
		// and focus point will be placed right behind P1 
		// and the cam will track to follow this point, 
		// which changes rapidly when close to the camera.
		bool wasLocked = lockInteriorOrientationOnInit;
		float distToFocus = camBaseTargetPos.GetDistance(focusPoint);
		lockInteriorOrientationOnInit = wasLocked && distToFocus < camMinTrailingDistance;
		if (wasLocked && !lockInteriorOrientationOnInit)
		{
			// No longer locked, so set the initial radial distance 
			// equal to the min trailing distance.
			camTargetRadialDistance = camMinTrailingDistance;
		}

		// Only update auto-zoom radial distance when not adjusting the camera's height.
		bool adjustingHeight = 
		(
			camAdjMode == CamAdjustmentMode::kZoom && fabsf(stickX) > fabsf(stickY)
		);
		if (!adjustingHeight)
		{
			// Have to find a new minimum radial distance 
			// that puts all players and the lock-on target (if any) in view.
			currentCheckOnScreen = false;
			// Binary search to subdivide the radial distance range repeatedly
			// until the min zoom delta is reached 
			// (arbitrarily close to the target radial distance).
			// Start from the base radial distance.
			radialDistanceRangeMin = 0.0f;
			radialDistanceRangeMax =
			(
				camTargetRadialDistance < camMaxZoomOutDist ?
				camMaxZoomOutDist :
				camTargetRadialDistance * 2.0f
			);
			radialDistanceRangeMid = max
			(
				prevRadialDistance,
				(radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f
			);
			lastOnScreenRadialDist = camMaxZoomOutDist;
			onScreenTestPos = focusPoint + dirFromFocus * radialDistanceRangeMid;
			bool minZoomRangeReached = false;
			// Reset counter.
			i = 0;
			// To be safe, we'll cap it to prevent an infinite loop.
			while (!minZoomRangeReached && i <= maxIterations)
			{
				currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation
				(
					onScreenTestPos, 
					rotationToCheck,
					usePlayerPositions
				);
				if (currentCheckOnScreen)
				{
					// On screen at the checked position, so zoom in.
					radialDistanceRangeMax = 
					lastOnScreenRadialDist = radialDistanceRangeMid;
				}
				else
				{
					// Not on screen at the checked position, so zoom out further.
					radialDistanceRangeMin = radialDistanceRangeMid;
				}

				radialDistanceRangeMid = 
				(
					(radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f
				);
				onScreenTestPos = 
				(
					focusPoint + dirFromFocus * radialDistanceRangeMid
				);
				// Stop once the minimum zoom delta is reached.
				minZoomRangeReached = 
				(
					radialDistanceRangeMax - radialDistanceRangeMin < 
					Settings::fMinAutoZoomDelta
				);
				++i;
			}
				
			// Finicky, so keeping debug prints for troubleshooting if needed.
			if (currentCheckOnScreen)
			{
				// Set to current midpoint if on screen.
				camTargetRadialDistance = radialDistanceRangeMid + camRadialDistanceOffset;
				/*DBG
				(
					"ON SCREEN: {} from ({}, {}, {}) and offset {}.",
					camTargetRadialDistance, 
					radialDistanceRangeMin,
					radialDistanceRangeMid,
					radialDistanceRangeMax,
					camRadialDistanceOffset
				);*/
			}
			else if (lastOnScreenRadialDist < camMaxZoomOutDist)
			{
				// Choose last radial distance at which all players were on screen, 
				// if modified.
				camTargetRadialDistance = lastOnScreenRadialDist + camRadialDistanceOffset;
				/*DBG
				(
					"OFF SCREEN BELOW MAX: "
					"{} from ({}, {}, {}), las on screen {} and offset {}.",
					camTargetRadialDistance, 
					radialDistanceRangeMin,
					radialDistanceRangeMid,
					radialDistanceRangeMax,
					lastOnScreenRadialDist,
					camRadialDistanceOffset
				);*/
			}
			else
			{
				// Reached max zoom out distance and still not on the screen 
				// (happens at times).
				// Set to previous value to avoid zooming out all of a sudden.
				camTargetRadialDistance = prevRadialDistance;
				/*DBG
				(
					"OFF SCREEN ABOVE MAX: {} from ({}, {}, {}) and prev {}.",
					camTargetRadialDistance, 
					radialDistanceRangeMin,
					radialDistanceRangeMid,
					radialDistanceRangeMax,
					prevRadialDistance
				);*/
			}
		}
		
		/*DBG
		(
			"Target: {} from ({}, {}, {}) and prev {}. True: {}. Is colliding: {}. "
			"Outside: {}, delayed: {}, {}, offset: {}, max zoom out dist: {}, can adjust: {}. "
			"Just started: {}, can zoom in: {}, can zoom out: {}, diff: {}, trying to zoom {}. "
			"Lock on target: {}, use player positions: {}",
			camTargetRadialDistance, 
			radialDistanceRangeMin,
			radialDistanceRangeMid,
			radialDistanceRangeMax,
			prevRadialDistance,
			camTrueRadialDistance,
			isColliding,
			outside,
			delayedZoomInUnderExteriorRoof,
			delayedZoomOutUnderExteriorRoof,
			camRadialDistanceOffset,
			camMaxZoomOutDist,
			canAdjustZoom,
			adjustingCamPID != -1 ? 
			glob.coopPlayers[adjustingCamPID]->pam->JustStarted(InputAction::kZoomCam) : 
			false,
			(
				stickY > 0.0f &&
				camTrueRadialDistance >= camTargetRadialDistance
			),
			(
				stickY < 0.0f &&
				camTrueRadialDistance <= camTargetRadialDistance
			),
			camTrueRadialDistance - camTargetRadialDistance,
			stickY >= 0.0f ? "IN" : "OUT",
			Util::HandleIsValid(camLockOnTargetHandle) ?
			camLockOnTargetHandle.get()->GetName() : 
			"NONE",
			usePlayerPositions
		);*/

		// Interp from previous when zooming in.
		// Do not interp when auto-zooming out because this introduces delay
		// that will not keep the target on screen.
		camTargetRadialDistance = 
		(
			prevRadialDistance < camTargetRadialDistance && !IsAdjustingZoom() ? 
			camTargetRadialDistance : 
			Util::InterpolateSmootherStep
			(
				prevRadialDistance, camTargetRadialDistance, camInterpFactor
			)
		);

		// Ensure the radial distance to set is never below the minimum trailing distance.
		if (!isColliding || camTargetRadialDistance < prevRadialDistance)
		{
			camTargetRadialDistance = max(camTargetRadialDistance, camMinTrailingDistance);
		}
	}

	void CameraManager::UpdateDeathCameraOrientation()
	{
		// Set the death camera position and rotation.
		// While pitched down at P1's torso, 
		// zoom out slowly and rotate a bit à la the closing scene in Breaking Bad.
		
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!glob.globalDataInit || 
			!glob.player1Actor ||
			!playerCam || 
			playerCam->IsInFirstPerson() || 
			//playerCam->IsInBleedoutMode() || 
			!playerCam->cameraRoot ||
			!p1)
		{
			return;	
		}

		RE::NiPoint3 currentPos = playerCam->cameraRoot->world.translate;
		const float secsSinceDead = Util::GetElapsedSeconds(deathCameraTP);
		const float maxZoomOutTime = 10.0f;
		const float tRatio = std::clamp(secsSinceDead / maxZoomOutTime, 0.0f, 1.0f);

		RE::NiPoint3 startPos = 
		(
			Util::GetTorsoPosition(glob.player1Actor.get()) + 
			RE::NiPoint3(0.0f, 0.0f, glob.player1Actor->GetHeight() * 0.5f)
		);
		RE::NiPoint3 endPos = startPos + RE::NiPoint3(0.0f, 0.0f, 2000.0f);
		bool collisionsEnabled = 
		(
			(glob.hybridModeActive) ||
			(
				(p1->parentCell) &&
				(
					(Settings::bCamExteriorCollisions && p1->parentCell->IsExteriorCell()) ||
					(Settings::bCamInteriorCollisions && p1->parentCell->IsInteriorCell())
				)
			)
		);
		if (collisionsEnabled)
		{
			auto result = Raycast::hkpCastRay
			(
				ToVec4(startPos),
				ToVec4(endPos),
				std::vector<RE::NiAVObject*>({ playerCam->cameraRoot.get() }), 
				std::vector<RE::FormType>({ RE::FormType::ActorCharacter })
			);
			if (result.hit)
			{
				endPos = ToNiPoint3
				(
					result.hitPos + 
					result.rayNormal * 
					min(result.rayLength, 5.0f)
				);
			}
		}
		
		RE::NiPoint3 targetPos = 
		{
			endPos.x,
			endPos.y,
			startPos.z + 
			Util::InterpolateSmootherStep(0.0f, (endPos.z - startPos.z), tRatio)
		};
		camTargetPos = 
		{
			Util::InterpolateEaseOut(currentPos.x, targetPos.x, tRatio, 3.0f),
			Util::InterpolateEaseOut(currentPos.y, targetPos.y, tRatio, 3.0f),
			Util::InterpolateEaseOut(currentPos.z, targetPos.z, tRatio, 3.0f)
		};

		bool wouldCollide = false;
		if (collisionsEnabled)
		{
			auto result = Raycast::hkpCastRay
			(
				ToVec4(currentPos), ToVec4(camTargetPos), true
			);
			if (result.hit)
			{
				wouldCollide = true;
				camTargetPos = ToNiPoint3
				(
					result.hitPos + 
					result.rayNormal * 
					min(result.rayLength, 5.0f)
				);
			}
		}

		const RE::NiPoint3 camForward = 
		(
			playerCam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f)
		);
		const float yawToTargetPos = Util::GetYawBetweenPositions(currentPos, startPos);
		const float currentPitch = Util::DirectionToGameAngPitch(camForward);
		const float currentYaw = Util::DirectionToGameAngYaw(camForward);
		camPitch =
		(
			currentPitch + Util::InterpolateSmootherStep
			(
				0.0f, Util::NormalizeAngToPi(PI / 2.0f - currentPitch), tRatio
			)
		);
		camYaw = Util::NormalizeAng0To2Pi
		(
			camYaw - Util::InterpolateSmootherStep
			(
				0.0f, PI / 720.0f, tRatio
			)
		);
	}

	void CameraManager::UpdateDialogueStateData()
	{
		// Update move-to-starting-position flag, speaker handle, and speaker changed TP.

		if (camState != CamState::kDialogue)
		{
			adjustedAfterReachingDialoguePos = 
			movingToDialogueStartPos = false;
			return;
		}

		// Update dialogue start transition and camera post-start adjustment flag.
		movingToDialogueStartPos =
		(
			camState == CamState::kDialogue &&
			Util::HandleIsValid(camDialogueTargetHandle) && 
			glob.menuPID > -1 &&
			Util::GetElapsedSeconds(dialogueCameraTP) < secsCamDialogueStartTransition
		);
		if (movingToDialogueStartPos)
		{
			adjustedAfterReachingDialoguePos = false;
		}
		else if (camAdjMode == CamAdjustmentMode::kZoom)
		{
			// Keep zoomed out after a player attempts to adjust zoom.
			adjustedAfterReachingDialoguePos = true;
		}

		if (camState == CamState::kDialogue && glob.menuPID > -1)
		{
			const auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
			// REMOVE when done debugging.
			/*DBG
			(
				"{} and {}. Prev: {}, speaker: {}, menu topic info valid: {}. "
				"Unks: B3: {}, B4: {}, B5: {}, B6: {}, B7: {}, B8: {}, B9: {}, BA: {}, BB: {}",
				glob.coopPlayers[glob.menuPID]->coopActor->GetName(),
				Util::HandleIsValid(camDialogueTargetHandle) ? 
				camDialogueTargetHandle.get()->GetName() :
				"NONE",
				Util::HandleIsValid(camDialogueSpeakerHandle) ? 
				camDialogueSpeakerHandle.get()->GetName() :
				"NONE",
				menuTopicManager && Util::HandleIsValid(menuTopicManager->speaker) ? 
				menuTopicManager->speaker.get()->GetName() :
				"NONE",
				menuTopicManager && menuTopicManager->currentTopicInfo,
				menuTopicManager && menuTopicManager->unkB3,
				menuTopicManager && menuTopicManager->unkB4,
				menuTopicManager && menuTopicManager->unkB5,
				menuTopicManager && menuTopicManager->unkB6,
				menuTopicManager && menuTopicManager->unkB7,
				menuTopicManager && menuTopicManager->unkB8,
				menuTopicManager && menuTopicManager->unkB9,
				menuTopicManager && menuTopicManager->unkBA,
				menuTopicManager && menuTopicManager->unkBB
			);*/
			if (menuTopicManager && Util::HandleIsValid(menuTopicManager->speaker))
			{
				auto prevSpeakerHandle = camDialogueSpeakerHandle;
				// Checking if 'currentTopicInfo' is available is not enough 
				// for switching back to a position behind the player 
				// because it sometimes gets cleared and repopulated after one frame.
				// UnkB8 is set not set to true when this one frame change happens.
				if (menuTopicManager->currentTopicInfo)
				{
					camDialogueSpeakerHandle = camDialogueTargetHandle;
				}
				else if (menuTopicManager->unkB8)
				{
					camDialogueSpeakerHandle = 
					(
						glob.coopPlayers[glob.menuPID]->coopActor->GetHandle()
					);
				}

				if (Util::HandleIsValid(camDialogueSpeakerHandle) && 
					camDialogueSpeakerHandle != prevSpeakerHandle)
				{
					dialogueSpeakerChangedTP = SteadyClock::now();
				}
			}
		}
	}

	void CameraManager::UpdateFocalPIDs()
	{
		// Update focal player IDs. 
		// Clears focal PID if the player is downed or the mode is disabled.
		// Clears the soft focal PID if the player is downed.

		// Reset focal player PID if the setting is now disabled 
		// or if the focal player is downed.
		bool shouldAutoResetFocalPID = 
		(
			(focalPID != -1) && 
			(!Settings::bFocalPlayerMode || glob.coopPlayers[focalPID]->isDowned)
		);
		if (shouldAutoResetFocalPID) 
		{
			focalPID = -1;
		}
		
		// Reset if the soft focal player is downed.
		bool shouldAutoResetSoftFocalPID = 
		(
			(focalPID != -1) || (softFocalPID != -1 && glob.coopPlayers[softFocalPID]->isDowned)
		);
		if (shouldAutoResetSoftFocalPID)
		{
			if (focalPID == -1)
			{
				glob.coopPlayers[softFocalPID]->tm->SetCrosshairMessageRequest
				(
					CrosshairMessageType::kCamera,
					fmt::format("P{}: Removed soft camera focus", softFocalPID + 1)
				);
			}

			softFocalPID = -1;
		}
	}

	void CameraManager::UpdateParentCell()
	{
		// Update the cached parent cell for the camera
		// based on the current parent cell for P1.
		// Reset fade and cam data if transitioning
		// from an exterior cell to an interior cell or vice versa.

		auto p1Cell = glob.player1Actor->GetParentCell();
		if (!p1Cell || currentCell == p1Cell || p1Cell->formID == 0x0)
		{
			return;
		}

		bool newIsExterior = p1Cell->IsExteriorCell();
		// Interior/Invalid -> Exterior or Exterior/Invalid -> Interior.
		bool diffCellType = 
		{
			(newIsExterior && (!currentCell || currentCell->IsInteriorCell())) ||
			(!newIsExterior && (!currentCell || !currentCell->IsInteriorCell()))
		};

		currentCell = p1Cell;
		exteriorCell = newIsExterior;
		// Reset fade for all our handled objects.
		//ResetFadeAndClearObstructions();
		// For extra peace of mind, ensure all objects in the new cell are fully faded in.
		Util::ResetFadeOnAllObjectsInCell(currentCell);
		// Set new default orientation when the cell type changes.
		if (diffCellType)
		{
			ResetCamData();
		}

		// Set all map markers within the cell.
		cellMapMarkers.clear();
		currentCell->ForEachReference
		(
			[&](RE::TESObjectREFR* a_refr)
			{
				// Skip if an invalid refr or not map marker.
				if (!a_refr || 
					!a_refr->GetBaseObject() || 
					a_refr->GetBaseObject()->formID != 0x10)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				cellMapMarkers.emplace_back(a_refr);
				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		// Some cells have fog that appears quickly when zooming out 
		// and a max zoom out distance for the camera before checking if points are on screen fail 
		// (messes with auto-zoom).
		// Modify the clip distance and weaken fog when matching occlusion removal setting is on.
		if ((Settings::bRemoveExteriorOcclusion && exteriorCell) || 
			(Settings::bRemoveInteriorOcclusion && !exteriorCell))
		{
			const auto scriptFactory = 
			(
				RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()
			);
			const auto script = scriptFactory ? scriptFactory->Create() : nullptr;
			if (script)
			{ 
				script->SetCommand("setclipdist 1000000000");
				script->CompileAndRun(nullptr);
				// Complete fog removal.
				//script->SetCommand("setfog 0 10000000000");
				//script->CompileAndRun(nullptr);
				// Tone down interior cell fog.
				if (!exteriorCell)
				{
					script->SetCommand("setfog 0 15000");
					script->CompileAndRun(nullptr);
				}

				delete script;
			}
		}

		// Havok world may've changed, so enable ragdoll <-> actor collisions on cell change.
		GlobalCoopData::EnableRagdollToActorCollisions();
	}

	void CameraManager::UpdatePlayerFadeAmounts(bool&& a_reset)
	{
		// Fade out players gradually as they approach the camera or
		// fade them in as they move away from the camera.
		// Reset instantly to fully-faded in if requested.

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			auto player3DPtr = Util::GetRefr3D(p->coopActor.get()); 	
			if (a_reset)
			{
				auto currentProc = p->coopActor->currentProcess; 
				if (currentProc && currentProc->high) 
				{
					currentProc->high->fadeAlpha = 1.0f;
					currentProc->high->fadeState.reset
					(
						RE::HighProcessData::FADE_STATE::kIn,
						RE::HighProcessData::FADE_STATE::kOut,
						RE::HighProcessData::FADE_STATE::kOutDelete,
						RE::HighProcessData::FADE_STATE::kOutDisable,
						RE::HighProcessData::FADE_STATE::kTeleportIn,
						RE::HighProcessData::FADE_STATE::kTeleportOut
					);
					currentProc->high->fadeState.set(RE::HighProcessData::FADE_STATE::kNormal);
				}

				if (player3DPtr)
				{
					player3DPtr->fadeAmount = 1.0f;
					player3DPtr->flags.reset(RE::NiAVObject::Flag::kHidden);
					RE::NiUpdateData updateData{ };
					player3DPtr->UpdateDownwardPass(updateData, 0);
				}
			}
			else
			{
				if (!player3DPtr || 
					isnan(player3DPtr->worldBound.radius) ||
					isinf(player3DPtr->worldBound.radius) ||
					isnan(player3DPtr->worldBound.center.Length()) ||
					isinf(player3DPtr->worldBound.center.Length()))
				{
					DBG("ERR: {}'s worldbound is corrupted. Extents: {}",
						p->coopActor->GetName(),
						(p->coopActor->GetBoundMax() - p->coopActor->GetBoundMin()).Length());
					continue;
				}
				
				// Prevent the player from fading 
				// when the camera is far enough away to not require fading.
				float prevFadeAmount = player3DPtr->fadeAmount;
				float newFadeAmount = prevFadeAmount;
				// Get the player's extent for a rough radius at which to start fading out.
				float maxXYExtent = 
				(
					(
						p->coopActor->GetBoundMax() - 
						p->coopActor->GetBoundMin()
					).Length() / 2.0f
				);
				if (maxXYExtent == 0.0f)
				{
					maxXYExtent = player3DPtr->worldBound.radius;
				}

				// Fade out when the player's torso is within one max extent of the camera.
				float torsoDistToCam = 
				(
					p->mm->playerTorsoPosition.GetDistance(camTargetPos)
				);
				if (camTargetPos.Length() != 0.0f && torsoDistToCam < maxXYExtent)
				{
					newFadeAmount = Util::InterpolateEaseInEaseOut
					(
						0.0f, 1.0f, torsoDistToCam / maxXYExtent, 2.0f
					);
				}
				else
				{
					newFadeAmount = 1.0f;
				}

				// Set new fade amount if different from the previously set one.
				if (newFadeAmount != prevFadeAmount)
				{
					player3DPtr->fadeAmount = newFadeAmount;
				}
			}
		}
	}
}

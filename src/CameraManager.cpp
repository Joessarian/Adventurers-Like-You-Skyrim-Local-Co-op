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
		camLockOnTargetHandle = RE::ActorHandle();
		lockOnActorReq = std::nullopt;
		camBaseTargetPos =  
		camCollisionTargetPos = 
		camFocusPoint = 
		camLockOnFocusPoint = 
		camOriginPoint = 
		camOriginPointDirection = 
		camTargetPos = RE::NiPoint3();
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
		lockOnIndicatorOscillationInterpData = std::make_unique<InterpolationData<float>>
		(
			0.0f,
			0.0f, 
			0.0f, 
			Settings::fSecsCamLockOnIndicatorOscillationUpdate
		);
		movementAngleMultInterpData = std::make_unique<TwoWayInterpData>();
		movementAngleMultInterpData->SetInterpInterval(2.0f, true);
		movementAngleMultInterpData->SetInterpInterval(2.0f, false);
		movementPitchInterpData = std::make_unique<InterpolationData<float>>
		(
			0.0f, 
			0.0f, 
			0.0f, 
			Settings::fSecsCamMovementPitchUpdate
		);
		movementYawInterpData = std::make_unique<InterpolationData<float>>
		(
			0.0f, 
			0.0f, 
			0.0f, 
			Settings::fSecsCamMovementYawUpdate
		);

		// State bools.
		autoRotateSuspended = false;
		delayedZoomInUnderExteriorRoof = false;
		delayedZoomOutUnderExteriorRoof = false;
		exteriorCell = false;
		isAutoTrailing = true;
		isManuallyPositioned = false;
		isLockedOn = false;
		isTogglingPOV = false;
		lockInteriorOrientationOnInit = false;
		lockOnTargetInSight = false;
		toggleBindPressedWhileWaiting = false;
		waitForToggle = false;
		// Positional offset floats.
		avgPlayerHeight = 100.0f;
		camRadialDistanceOffset = camSavedRadialDistanceOffset = 0.0f;
		camMinTrailingDistance = 100.0f; 
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
		// Controller IDs.
		controlCamCID = -1;
		focalPlayerCID = -1;

		// XInput mask for the button that toggles the co-op camera.
		// Set by default to the 'Toggle POV' bind's XInput mask.
		auto controlMap = RE::ControlMap::GetSingleton();
		auto userEvents = RE::UserEvents::GetSingleton();
		camToggleXIMask =
		(
			controlMap && userEvents ? 
			controlMap->GetMappedKey(userEvents->togglePOV, RE::INPUT_DEVICE::kGamepad) :
			GAMEPAD_MASK_RIGHT_THUMB
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
		if (!isTogglingPOV && playerCam && playerCam->currentState && !isInSupportedCamState)
		{
			ToThirdPersonState(playerCam->currentState->id == RE::CameraState::kFirstPerson);
		}

		isAutoTrailing = camState == CamState::kAutoTrail;
		isLockedOn = camState == CamState::kLockOn;
		isManuallyPositioned = camState == CamState::kManualPositioning;
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

		// On state change, reset TPs.
		if (camState != prevCamState)
		{
			ResetTPs();
		}

		if (!isTogglingPOV)
		{
			SetCamInterpFactors();
			UpdateParentCell();
			CalcNextOriginPoint();
			CalcNextFocusPoint();
			CalcNextTargetPosition();
			CheckLockOnTarget();
			UpdatePlayerFadeAmounts();

			if (isAutoTrailing)
			{
				UpdateCamHeight();
				UpdateCamZoom();
				UpdateCamRotation();
			}
			else if (isLockedOn)
			{
				UpdateCamRotation();
				UpdateCamZoom();
				UpdateCamHeight();
			}
			else
			{
				UpdateCamHeight();
				UpdateCamRotation();
			}

			FadeObstructions();
		}
		
		// Set the camera's orientation and override its local rotation.
		// Since this manager runs through the Main() hook, 
		// updating here allows any other mod that changes the camera's local rotation 
		// after us to have their changes stack on our own changes 
		// and not the game's original computed local rotation for this frame.
		SetCamOrientation(true);
		// Update previous state.
		prevCamState = camState;
	}

	void CameraManager::PrePauseTask()
	{
		SPDLOG_DEBUG("[CAM] PrePauseTask");

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
		// Reset toggle press flag every time before pausing.
		toggleBindPressedWhileWaiting = false;

		// Reset fade on handled objects.
		ResetFadeAndClearObstructions();

		// Reset crosshair text and position.
		GlobalCoopData::SetCrosshairText(true);
	}

	void CameraManager::PreStartTask()
	{
		SPDLOG_DEBUG("[CAM] PreStartTask");

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
	}

	void CameraManager::RefreshData()
	{
		SPDLOG_DEBUG("[CAM] RefreshData");

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

		// Check if all players are valid, and if one isn't, pause.
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
		if (playerCam->currentState->id == RE::CameraState::kFurniture &&
			glob.player1Actor->GetOccupiedFurniture())
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
		}

		return currentState;
	}

	const ManagerState CameraManager::ShouldSelfResume()
	{
		bool allPlayersValid = false;
		if (glob.coopSessionActive && glob.livingPlayers > 1)
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
					p->coopActor->parentCell && p->coopActor->parentCell->IsAttached()
				};

				if (!allPlayersValid)
				{
					return currentState;
				}
			}
			
			// If P1 is downed, resume right away, 
			// since other players will not be able to control the bleedout camera.
			if (glob.coopPlayers[glob.player1CID]->isDowned)
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
					ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::MapMenu::MENU_NAME) ||
					ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)
				))
			{
				return currentState;
			}

			// Have to pause here because the player will stop mining while the camera is enabled, 
			// even if the camera's current state is set to furniture.
			if (playerCam->currentState->id == RE::CameraState::kFurniture && 
				glob.player1Actor->GetOccupiedFurniture())
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
				if ((furniture) && 
					(furniture->HasKeywordString("isPickaxeFloor") || 
					 furniture->HasKeywordString("isPickaxeTable") || 
					 furniture->HasKeywordString("isPickaxeWall")))
				{
					return ManagerState::kPaused;
				}
			}

			// Next, when waiting to toggle the camera back on, 
			// ensure that all menus that pause the game are closed
			// and that the cam toggle bind was released.
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
						GAMEPAD_MASK_RIGHT_THUMB
					);
					// Check if the toggle bind is pressed and released by P1.
					XINPUT_STATE tempState{ };
					ZeroMemory(&tempState, sizeof(XINPUT_STATE));
					if (!(XInputGetState(glob.player1CID, &tempState)) == ERROR_SUCCESS)
					{
						// Could not read P1's controller state, so pause.
						return ManagerState::kPaused;
					}

					toggleBindPressedWhileWaiting = 
					(
						(toggleBindPressedWhileWaiting) || 
						((tempState.Gamepad.wButtons & camToggleXIMask) == camToggleXIMask)
					);
					// Previously pressed and now released the toggle bind.
					shouldResume = 
					(
						(toggleBindPressedWhileWaiting) && 
						((tempState.Gamepad.wButtons & camToggleXIMask) == 0)
					);
				}

				if (shouldResume)
				{
					return ManagerState::kRunning;
				}
				else
				{
					return currentState;
				}
			}
		}

		return allPlayersValid ? ManagerState::kRunning : currentState;
	}

	bool CameraManager::AllPlayersOnScreenAtCamOrientation
	(
		const RE::NiPoint3& a_camPos, 
		const RE::NiPoint2& a_rotation,
		bool&& a_usePlayerPos, 
		const std::vector<RE::BSFixedString>&& a_nodeNamesToCheck
	)
	{
		// Check if all players are within the camera's frustum 
		// at the given position and rotation.
		// Check the player's refr position or check a list of player nodes.

		const auto strings = RE::FixedStrings::GetSingleton();
		bool allPlayersInFrontOfPoint = true;

		auto getActorInFrontOfPoint = 
		[&](RE::Actor* a_actor)
		{
			if (a_usePlayerPos)
			{
				return PointOnScreenAtCamOrientationWorldspaceMargin
				(
					a_actor->data.location + 
					RE::NiPoint3(0.0f, 0.0f, a_actor->GetHeight() * 0.75f), 
					a_camPos,
					a_rotation, 
					a_actor->GetHeight() / 2.0f
				);
			}
			else
			{

				// Invalid 3D for one player means not all players 
				// are in front of the camera. Return early.
				auto loadedData = a_actor->loadedData;
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
							onePlayerNodeOnScreen |= PointOnScreenAtCamOrientationWorldspaceMargin
							(
								nodePtr->world.translate, 
								a_camPos, 
								a_rotation, 
								a_actor->GetHeight() / 2.0f
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
							onePlayerNodeOnScreen |= PointOnScreenAtCamOrientationWorldspaceMargin
							(
								nodePtr->world.translate, 
								a_camPos, 
								a_rotation, 
								a_actor->GetHeight() / 2.0f
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

		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			// Break once one player is not on screen at this point.
			allPlayersInFrontOfPoint = getActorInFrontOfPoint(p->coopActor.get());
			if (!allPlayersInFrontOfPoint)
			{
				break;
			}
		}

		if (allPlayersInFrontOfPoint && ShouldConsiderLockOnTargetAsPlayer())
		{
			allPlayersInFrontOfPoint = getActorInFrontOfPoint(camLockOnTargetHandle.get().get());
		}

		return allPlayersInFrontOfPoint;
	}

	void CameraManager::CalcNextFocusPoint()
	{
		// Calculate the next focus point (origin point offset along the Z axis).
		// If in auto-trail mode or locked on without a valid target,
		// adjust the focus point relative to the origin point.

		auto camNodePos = tpState->camera->cameraRoot->world.translate;
		auto oldFocusPoint = camFocusPoint;
		auto prevOffset = camHeightOffset;

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
					camOriginPoint.z + camHeightOffset
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
				camOriginPoint.z + camBaseHeightOffset
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

		camOriginPoint = RE::NiPoint3();
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
		if (ShouldConsiderLockOnTargetAsPlayer())
		{			
			camOriginPoint += camLockOnTargetHandle.get()->data.location;
			camOriginPoint *= (1.0f / static_cast<float>(glob.livingPlayers + 1));
			camOriginPoint.z += 
			(
				(
					(avgPlayerHeight * static_cast<float>(glob.livingPlayers)) +
					camLockOnTargetHandle.get()->GetHeight()
				) / static_cast<float>(glob.livingPlayers + 1)
			);
		}
		else
		{
			camOriginPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
			camOriginPoint.z += avgPlayerHeight;
		}
		
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
			originViewObstructed = NoPlayersVisibleAtPoint(camCollisionOriginPoint, true); 
			if (originViewObstructed)
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

			if (focalPlayerCID == -1)
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
				bounds = Util::GetVertCollPoints(camPlayerFocusPoint, 0.0f);
				camMaxAnchorPointZCoord = bounds.first;
				camMinAnchorPointZCoord = bounds.second;
			}

			/*SPDLOG_DEBUG
			(
				"[CAM] CalcNextOriginPoint: "
				"Hit to base: {}, to coll pos: {}, bounds: ({}, {}), "
				"collision origin point z: {}, original Z = {}.",
				hitToBasePos,
				hitToCollisionPos, 
				bounds.first,
				bounds.second,
				camCollisionOriginPoint.z,
				camOriginPoint.z
			);*/
			camOriginPoint = camCollisionOriginPoint;
		}


		if (Settings::bOriginPointSmoothing)
		{
			camOriginPoint.x = Util::InterpolateSmootherStep
			(
				oldOriginPoint.x, camOriginPoint.x, camInterpFactor
			);
			camOriginPoint.y = Util::InterpolateSmootherStep
			(
				oldOriginPoint.y, camOriginPoint.y, camInterpFactor
			);
			camOriginPoint.z = Util::InterpolateSmootherStep
			(
				oldOriginPoint.z, camOriginPoint.z, camInterpFactor
			);
		}

		// Save origin point direction for auto pitch adjustments.
		camOriginPointDirection = camOriginPoint - oldOriginPoint;
		camOriginPointDirection.Unitize();
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
			if (camAdjMode == CamAdjustmentMode::kZoom && 
				controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
			{
				const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
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
			if (focalPlayerCID == -1) 
			{
				// Base target position is offset from the base focus position,
				// and is not guaranteed to be a reachable spot.
				camBaseTargetPos = camFocusPoint;
				camBaseTargetPos.z -= r * cosf(theta);
				camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
				camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
			}
			else
			{
				// Base target position is the focal player's focus point,
				// which is almmost guaranteed to be valid,
				// since it is offset from the player's position.
				const auto& focalP = glob.coopPlayers[focalPlayerCID];
				camPlayerFocusPoint = 
				(
					focalP->coopActor->data.location +
					RE::NiPoint3
					(
						0.0f,
						0.0f,
						focalP->coopActor->IsSneaking() ?
						0.5f * focalP->coopActor->GetHeight() :
						focalP->coopActor->GetHeight()
					)	
				);
				camBaseTargetPos = camPlayerFocusPoint + RE::NiPoint3(0.0f, 0.0f, camHeightOffset);
				camBaseTargetPos.z -= r * cosf(theta);
				camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
				camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
			}

			if (camCollisions)
			{
				// [(Questionable?) Methods to the Madness Below]:
				// 
				// To ensure that we have a valid target position that isn't out of bounds
				// or in an area that no player can reach, we need a valid starting position
				// to raycast to our base target position, which could also be invalid.
				// 
				// We use player-to-target LOS hit positions as the base positions 
				// for determining the next target postiion, since these are always valid positions 
				// (because they are offset from the players' positions themselves).
				// 
				// An additional feature is that if players are far apart, 
				// each player can have the camera follow them as long as they rotate it
				// while they are not in LOS of the rest of the players.
				// The camera will automatically focus on the entire party again
				// when the focal player is visible and close enough to the rest of the party.

				// No matter what, players should try to stay close together as much as possible
				// for the smoothest experience when using the co-op camera.

				// Raycast result from the previous position to the current base position.
				// If there's a hit, this means that the camera will hit a surface 
				// if it moves directly to the base target position.

				Raycast::RayResult movementResult = Raycast::CastRay
				(
					ToVec4(lastSetCamTargetPos), ToVec4(camBaseTargetPos), camTargetPosHullSize
				);
				if (movementResult.hit)
				{
					// Upon movement hit, the camera is now colliding with geometry.
					isColliding = true;
				}
			
				// Reset focal player CID if the setting is now disabled 
				// or if the focal player is downed.
				bool shouldAutoResetFocalPlayer = 
				(
					(focalPlayerCID != -1) && 
					(!Settings::bFocalPlayerMode || glob.coopPlayers[focalPlayerCID]->isDowned)
				);
				if (shouldAutoResetFocalPlayer) 
				{
					focalPlayerCID = -1;
				}

				//=================================================================================
				// [Camera Collision Positions]:
				//=================================================================================
				// Set (hopefully) to a position that is reachable 
				// and not outside the world geometry.
				// One collision position is used to place the camera 
				// if camera collisions are enabled.
				// The other collision position originates from the base focus position,
				// which can be outside the traversable worldspace, and is used for crosshair
				// selection when camera collisions are disabled.

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
				// 
				// Min 2 raycasts (2 from camera focus point).
				// Max 6-10 raycasts (2 from camera focus point + 2 per active player).

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
				glm::vec4 castStartPos = ToVec4(camFocusPoint);
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

				bool baseTargetPosVisible = !result.hit;
				if (baseTargetPosVisible)
				{
					closestHitPos = baseTargetPos;
				}
				else
				{
					// Now cast from each player's focus point
					// to check for a closer hit position.
					closestIndex = 1337;
					// We have an obstruction to the base target position,
					// so adjust the hit position away from the obstruction now.
					adjHitResultPos =
					(
						result.hitPos +
						(result.rayNormal + endToStartDir) *
						min(result.rayLength, camTargetPosHullSize)
					);
					// Set the initial closest distance to the previous target position.
					closestDist = glm::distance(adjHitResultPos, lastSetTargetPos); 
					closestHitPos = adjHitResultPos;
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
								(result.rayNormal + endToStartDir) *
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
							closestIndex = -p->controllerID;
							closestHitPos = baseTargetPos;
							break;
						}
						else
						{
							closestIndex = p->controllerID;
							adjHitResultPos =
							(
								result.hitPos +
								(result.rayNormal + endToStartDir) *
								camTargetPosHullSize
								//min(result.rayLength, camTargetPosHullSize)
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

					// Also check from lock-on target's focus point.
					if (ShouldConsiderLockOnTargetAsPlayer())
					{
						castStartPos = ToVec4
						(
							Util::GetActorFocusPoint(camLockOnTargetHandle.get().get())
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
								(result.rayNormal + endToStartDir) *
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
						else if (Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
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
								(result.rayNormal + endToStartDir) *
								camTargetPosHullSize
								//min(result.rayLength, camTargetPosHullSize)
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

				// Set next collision target position and set colliding flag.
				camCollisionTargetPos = ToNiPoint3(closestHitPos);
				// Not colliding if the collision target position 
				// is the same as the base target position and there is no movement hit.
				if (camCollisionTargetPos == camBaseTargetPos && !movementResult.hit) 
				{
					isColliding = false;
				}
				else
				{
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
				if (Settings::bTargetPosSmoothing)
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

				camCollisionTargetPos = camTargetPos;
			}

			// Save the final target position's radial distance for zoom calculations later.
			camTrueRadialDistance = camTargetPos.GetDistance(camFocusPoint);
		}
	}

	Raycast::RayResult CameraManager::ClusterCast
	(
		const glm::vec4& a_start, 
		const glm::vec4& a_end, 
		const float& a_radius, 
		const uint32_t& a_additionalRingsOfCasts
	)
	{
		Raycast::RayResult result = Raycast::hkpCastRay(a_start, a_end, true);
		// If there's a hit from the initial cast position,
		// or if there are no requested additional casts,
		// or if the start and end positions for the cast are the same,
		// we can just return the first result.
		if (result.hit || a_additionalRingsOfCasts == 0 || a_start == a_end)
		{
			return result;
		}

		// Cast in concentric clusters of 4 about the initial cast start point,
		// shrinking the radius with each additional ring.
		// 
		// Visualization:
		// 
		// For one additional ring:
		// 
		//						1
		// 
		// 
		//				4		X		2
		// 
		// 
		//						3
		//
		// For two additional rings:
		//
		//						1
		// 
		//					5		6
		//				4		X		2
		//					8		7
		// 
		//						3
		// 
		// Offset distance from the initial cast is equal to the given radius.

		const float castsAngleOffset = PI / 2.0f;
		RE::NiPoint3 dir = ToNiPoint3(a_end - a_start);
		dir.Unitize();
		const RE::NiPoint3 worldUp
		{
			dir == RE::NiPoint3(0.0f, 0.0f, 1.0f) ?
			RE::NiPoint3(0.0f, 1.0f, 0.0f) :
			RE::NiPoint3(0.0f, 0.0f, 1.0f)
		};
		RE::NiPoint3 initialOffset{ dir.UnitCross(worldUp) }; 
		Util::RotateVectorAboutAxis(initialOffset, dir, -PI / 4.0f);
		RE::NiPoint3 offset{ initialOffset }; 

		uint32_t numCasts = 0;
		uint32_t currentRing = 1;
		float minDistFromStart = FLT_MAX;
		float distFromStart = 0.0f;
		float radius = a_radius;
		float radiusDelta = a_radius / a_additionalRingsOfCasts;
		glm::vec4 newStart = a_start;
		Raycast::RayResult additionalResult{ };
		while (numCasts < a_additionalRingsOfCasts * 4)
		{
			if (numCasts % 4 == 0)
			{
				Util::RotateVectorAboutAxis(initialOffset, dir, -PI / 4.0f);
				offset = initialOffset;
				radius = a_radius - static_cast<float>(currentRing - 1) * radiusDelta;
				currentRing++;
			}

			newStart = a_start + ToVec4(offset * radius);
			additionalResult = Raycast::hkpCastRay(newStart, a_end, true);
			// Find and set the hit result that has a hit position closest
			// to its starting position.
			if (additionalResult.hit)
			{
				distFromStart = glm::length(additionalResult.hitPos - newStart);
				if (distFromStart < minDistFromStart)
				{
					result = additionalResult;
					minDistFromStart = distFromStart;
				}
			}

			// Update the offset for the next cast.
			Util::RotateVectorAboutAxis(offset, dir, castsAngleOffset);
			numCasts++;
		}

		return result;
	}

	void CameraManager::CheckLockOnTarget()
	{
		// Set a new lock-on target if there is a valid request,
		// or check if the current target is valid, 
		// clearing out invalid targets as needed.

		if (lockOnActorReq.has_value())
		{
			camLockOnTargetHandle = lockOnActorReq.value();
			auto actorPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
			if (!actorPtr)
			{
				ClearLockOnData();
			}
			else
			{
				// LOS first checked (valid selected actor) before request is sent,
				// so target is always valid here initially.
				// Change lock-on data to reflect this.
				secsSinceLockOnTargetLOSChecked = secsSinceLockOnTargetLOSLost = 0.0f;
				lockOnLOSCheckTP = SteadyClock::now();
				lockOnTargetInSight = true;
			}

			// Indicate request was handled by clearing it.
			lockOnActorReq = std::nullopt;
		}

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		bool validLockOnTarget = static_cast<bool>(camLockOnTargetPtr);
		if (isLockedOn)
		{
			// Check if target is still valid (in LOS, 3D loaded, handle valid, etc.)
			RE::NiPoint3 oldCamLockOnFocusPoint = camLockOnFocusPoint;
			// Check if the lock-on target is a player, and if so, 
			// the target is only valid if not downed.
			const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(camLockOnTargetPtr);
			validLockOnTarget &= pIndex == -1 || !glob.coopPlayers[pIndex]->isDowned;
			if (validLockOnTarget)
			{
				secsSinceLockOnTargetLOSChecked = Util::GetElapsedSeconds(lockOnLOSCheckTP);
				if (secsSinceLockOnTargetLOSChecked > Settings::fSecsBetweenTargetVisibilityChecks)
				{
					lockOnLOSCheckTP = SteadyClock::now();
					bool hadLOS = lockOnTargetInSight;
					bool inFrustum = false;
					auto p1 = RE::PlayerCharacter::GetSingleton();
					// Use P1's LOS check.
					lockOnTargetInSight = 
					(
						p1 && 
						p1->HasLineOfSight(camLockOnTargetPtr.get(), inFrustum)
					);
					bool lostLOS = camLockOnTargetPtr && hadLOS && !lockOnTargetInSight;
					bool noLOS = camLockOnTargetPtr && !lockOnTargetInSight;
					bool regainedLOS = camLockOnTargetPtr && !hadLOS && lockOnTargetInSight;
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
				bool invalidateAfterNoLOS = 
				(
					secsSinceLockOnTargetLOSLost > Settings::fSecsWithoutLOSToInvalidateTarget
				);
				if (invalidateAfterNoLOS || 
					!camLockOnTargetPtr->Is3DLoaded() || 
					!camLockOnTargetPtr->IsHandleValid() || 
					!camLockOnTargetPtr->GetParentCell() || 
					!camLockOnTargetPtr->GetParentCell()->IsAttached())
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
					camLockOnFocusPoint = Util::GetHeadPosition(camLockOnTargetPtr.get());
				}
			}

			if (validLockOnTarget)
			{
				// Draw lock-on indicator above the target's head.
				auto fixedStrings = RE::FixedStrings::GetSingleton();
				auto niCamPtr = Util::GetNiCamera();
				RE::NiPoint3 lockOnIndicatorCenter{ };
				RE::NiPoint3 topOfTheHeadPos = Util::GetHeadPosition(camLockOnTargetPtr.get());
				if (fixedStrings && niCamPtr && niCamPtr.get())
				{
					// Based on the head body part's radius.
					auto headRadius = Util::GetHeadRadius(camLockOnTargetPtr.get());
					topOfTheHeadPos.z += headRadius;
					lockOnIndicatorCenter = Util::WorldToScreenPoint3(topOfTheHeadPos);
				}
				else
				{
					// Fallback.
					lockOnIndicatorCenter = Util::WorldToScreenPoint3
					(
						topOfTheHeadPos + 
						RE::NiPoint3(0.0f, 0.0f, 0.1f * camLockOnTargetPtr->GetHeight())
					);
				}

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

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		if (!camLockOnTargetPtr) 
		{
			return;
		}

		float indicatorBaseLength = Settings::fCamLockOnIndicatorLength;
		const float& indicatorBaseThickness = Settings::fCamLockOnIndicatorThickness;
		float targetPixelHeight = Util::GetBoundPixelDist(camLockOnTargetPtr.get(), true);
		targetPixelHeight = targetPixelHeight == 0.0f ? indicatorBaseLength : targetPixelHeight;
		// Scale with target's pixel height and bound above and below.
		indicatorBaseLength = std::clamp
		(
			indicatorBaseLength,
			min
			(
				max
				(
					4.0f * indicatorBaseThickness, 
					indicatorBaseLength / 2.0f
				), targetPixelHeight / 4.0f
			),
			max
			(
				max
				(
					4.0f * indicatorBaseThickness, 
					indicatorBaseLength / 2.0f
				), targetPixelHeight / 4.0f
			)
		);
		auto upperPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS;
		auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;
		float baseScalingFactor = 
		(
			indicatorBaseLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH
		);
		const float indicatorLength = indicatorBaseLength * baseScalingFactor;
		const float indicatorThickness = indicatorBaseThickness * baseScalingFactor;
		const float indicatorGap = max(2.0f, indicatorLength);

		// Oscillation interp data: dynamically inc/dec gap size.
		float endPointGapDelta = lockOnIndicatorOscillationInterpData->next;
		if (lockOnIndicatorOscillationInterpData->current == endPointGapDelta)
		{
			// Switch gap delta endpoint when reached.
			endPointGapDelta = endPointGapDelta == 0.0f ? indicatorGap : 0.0f;
		}

		// Start a new interp cycle when the endpoint changes.
		if (lockOnIndicatorOscillationInterpData->next != endPointGapDelta)
		{
			lockOnIndicatorOscillationInterpData->SetTimeSinceUpdate(0.0f);
			lockOnIndicatorOscillationInterpData->ShiftEndpoints(endPointGapDelta);
		}

		lockOnIndicatorOscillationInterpData->next = endPointGapDelta;
		// Continue interpolating until reaching the endpoint.
		if (lockOnIndicatorOscillationInterpData->current != endPointGapDelta)
		{
			lockOnIndicatorOscillationInterpData->InterpolateSmootherStep
			(
				min
				(
					lockOnIndicatorOscillationInterpData->secsSinceUpdate / 
					lockOnIndicatorOscillationInterpData->secsUpdateInterval, 
					1.0f
				)
			);
			lockOnIndicatorOscillationInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
			// Endpoint now reached, so we're done.
			if (lockOnIndicatorOscillationInterpData->current == endPointGapDelta)
			{
				lockOnIndicatorOscillationInterpData->SetUpdateDurationAsComplete();
				lockOnIndicatorOscillationInterpData->SetData
				(
					endPointGapDelta, endPointGapDelta, endPointGapDelta
				);
			}
		}

		// Points are offset downward from origin (+Y Scaleform axis).
		// Have to rebase from the bottom tip by subtracting the length for each segment,
		// multiplying with the base scaling offset, and then factoring in the gap.
		float gapDelta = lockOnIndicatorOscillationInterpData->current;
		for (auto& offset : upperPortionOffsets)
		{
			offset *= baseScalingFactor;
			offset.y -= gapDelta + indicatorGap;
		}

		for (auto& offset : lowerPortionOffsets)
		{
			offset *= baseScalingFactor;
			offset.y -= gapDelta + indicatorGap;
		}

		const auto port = Util::GetPort();
		const float trueLength = 
		(
			indicatorLength + 2.0f * indicatorThickness + gapDelta + indicatorGap
		);
		const float trueWidth = 
		(
			0.5f * 
			baseScalingFactor *
			(
				GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[4].x - 
				GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[0].x
			)
		);
		bool onScreen = 
		(
			a_centerX - trueLength * cosf(PI / 4.0f) > port.left &&
			a_centerX + trueLength * cosf(PI / 4.0f) < port.right &&
			a_centerY - trueLength > port.top &&
			a_centerY < port.bottom
		);

		glm::vec2 posScreenCoords{ a_centerX, a_centerY };
		DebugAPI::ClampPointToScreen(posScreenCoords);
		float startingRotation = -PI / 4.0f;
		if (!onScreen)
		{
			// Rotate all prongs to point in the direction of the off-screen the lock-on target.
			// Add/subtract an additional offset to fit the indicator to the edges of the screen 
			// more snugly.
			float indicatorRotRads = 0.0f;
			if (posScreenCoords.x == port.right && posScreenCoords.y == port.top)
			{
				// Top right corner.
				indicatorRotRads = 3.0f * PI / 4.0f;
				posScreenCoords.x -= trueWidth;
				posScreenCoords.y += trueWidth;
			}
			else if (posScreenCoords.x == port.left && posScreenCoords.y == port.top)
			{
				// Top left corner.
				indicatorRotRads = -3.0f * PI / 4.0f;
				posScreenCoords.x += trueWidth;
				posScreenCoords.y += trueWidth;
			}
			else if (posScreenCoords.x == port.left && posScreenCoords.y == port.bottom)
			{
				// Bottom left corner.
				indicatorRotRads = -PI / 4.0f;
				posScreenCoords.x += trueWidth;
				posScreenCoords.y -= trueWidth;
			}
			else if (posScreenCoords.x == port.right && posScreenCoords.y == port.bottom)
			{
				// Bottom right corner.
				indicatorRotRads = PI / 4.0f;
				posScreenCoords.x -= trueWidth;
				posScreenCoords.y -= trueWidth;
			}
			else if (posScreenCoords.y == port.top)
			{
				// Top edge of the screen.
				indicatorRotRads = PI;
				posScreenCoords.y -= indicatorGap;
			}
			else if (posScreenCoords.x == port.left)
			{
				// Left edge of the screen.
				indicatorRotRads = -PI / 2.0f;
				posScreenCoords.x -= indicatorGap;
			}
			else if (posScreenCoords.y == port.bottom)
			{
				// Bottom edge of the screen.
				indicatorRotRads = 0.0f;
				posScreenCoords.y += indicatorGap;
			}
			else if (posScreenCoords.x == port.right)
			{
				// Right edge of the screen.
				indicatorRotRads = PI / 2.0f;
				posScreenCoords.x += indicatorGap;
			}

			// Add the additional angle to rotate when off screen.
			startingRotation += indicatorRotRads;
		}
		
		// Three prongs, with two rotated += 45 degrees about the origin.
		DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, startingRotation);
		DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, startingRotation);
		DebugAPI::QueueShape2D
		(
			posScreenCoords,
			lowerPortionOffsets,
			0x000000FF, 
			false, 
			indicatorThickness,
			0.0f
		);
		DebugAPI::QueueShape2D(posScreenCoords, lowerPortionOffsets, 0xFFFFFFFF);
		DebugAPI::QueueShape2D
		(
			posScreenCoords,
			upperPortionOffsets,
			0x000000FF,
			false,
			indicatorThickness,
			0.0f
		);
		DebugAPI::QueueShape2D(posScreenCoords, upperPortionOffsets, 0xFFFFFFFF);
		for (uint8_t i = 0; i < 2; ++i)
		{
			DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, PI / 4.0f);
			DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, PI / 4.0f);
			DebugAPI::QueueShape2D
			(
				posScreenCoords,
				lowerPortionOffsets, 
				0x000000FF,
				false,
				indicatorThickness, 
				0.0f       
			);
			DebugAPI::QueueShape2D(posScreenCoords, lowerPortionOffsets, 0xFFFFFFFF);
			DebugAPI::QueueShape2D
			(
				posScreenCoords,
				upperPortionOffsets, 
				0x000000FF, 
				false,
				indicatorThickness,
				0.0f
			);
			DebugAPI::QueueShape2D(posScreenCoords, upperPortionOffsets, 0xFFFFFFFF);
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
			if (!p->isActive || focalPlayerCID != -1 && p->controllerID != focalPlayerCID)
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
			for (auto iter = obstructionFadeDataMap.begin(); 
				 iter != obstructionFadeDataMap.end(); ) 
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

				// Only increment the iter if data was removed from the map.
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

		float autoRotateAngle = 0.0f;
		float autoRotateAngleMult = 1.0f;
		float avgAutoRotateAngle = 0.0f;
		// Is the current player mounted?
		bool isMounted = false;
		// Is the current player not ragdolled and not getting up?
		bool normalKnockState = true;
		// Only consider players that are not using furniture.
		bool notUsingFurniture = true;
		// Number of players considered when determining the movement auto-rotate angle.
		// Will divide into the total movement pitch accumulated.
		uint32_t consideredPlayersCount = 0;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			// Only consider the focal player for calculating the movement auto-rotate angle.
			if (focalPlayerCID != -1 && p->controllerID != focalPlayerCID)
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
			// and are mounted or not using furniture and they are moving and not attacking.
			bool addToTotal =
			(
				(charController) &&
				(normalKnockState) &&
				(isMounted || notUsingFurniture) && 
				(
					movementActor->actorState1.movingBack ||
					movementActor->actorState1.movingForward ||
					movementActor->actorState1.movingLeft ||
					movementActor->actorState1.movingRight
				)
			);
			if (!addToTotal)
			{
				continue;
			}
			
			const auto& lsData = glob.cdh->GetAnalogStickState(p->controllerID, true);
			if (a_computePitch)
			{
				auto velocity = Util::GetActorLinearVelocity(movementActor.get());
				auto& currentState = charController->context.currentState;
				// Velocity-based incline angle when in the air/flying/jumping.
				if (currentState == RE::hkpCharacterStateType::kFlying ||
					currentState == RE::hkpCharacterStateType::kInAir ||
					currentState == RE::hkpCharacterStateType::kJumping)
				{
					auto speed = velocity.Length();
					auto velPitch = speed > 0.0f ? asinf(velocity.z / speed) : 0.0f;
					// Divide by 2 to prevent too large of a swing in pitch.
					autoRotateAngle = velPitch / 2.0f;
				}
				else
				{
					// Surface support-based incline angle 
					// when on the ground/climbing/swimming.
					auto normalZComp = 
					(
						charController->surfaceInfo.surfaceNormal.quad.m128_f32[2]
					);
					auto supportSurfaceIncline = fabsf(asinf(normalZComp) - PI / 2.0f);

					// Supporting surface's normal must be pointing up.
					// Flat or down indicates that the player is walking on a surface 
					// that is parallel to their upright direction or above them, 
					// and that's not possible, I think.
					// Report an incline of 0 in that case.
					if (charController->surfaceInfo.surfaceNormal.quad.m128_f32[2] > 0.0f) 
					{
						RE::NiPoint3 normal = ToNiPoint3
						(
							charController->surfaceInfo.surfaceNormal, true
						);
						RE::NiPoint3 camRight = Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle(Util::NormalizeAng0To2Pi(camYaw + PI / 2.0f))
						);
						RE::NiPoint3 camForwardXY = Util::RotationToDirectionVect
						(
							0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(camYaw))
						);
						float angNormalToForwardXY = acosf
						(
							std::clamp(normal.Dot(camForwardXY), -1.0f, 1.0f)
						);
						if (isnan(angNormalToForwardXY) || isinf(angNormalToForwardXY))
						{
							angNormalToForwardXY = PI / 2.0f;
						}

						supportSurfaceIncline = fabsf(angNormalToForwardXY - PI / 2.0f);
					}
					else
					{
						supportSurfaceIncline = 0.0f;
					}
							
					// Moving uphill means the pitch must decrease to angle the camera
					// upward towards the players.
					if (velocity.z > 0.0f)
					{
						supportSurfaceIncline = -supportSurfaceIncline;
					}
					else if (velocity.z == 0.0f)
					{
						// If the player's z velocity is 0, set the incline to 0.
						supportSurfaceIncline = 0.0f;
					}

					autoRotateAngle = supportSurfaceIncline;
				}
			}
			else
			{
				autoRotateAngle = Util::NormalizeAngToPi
				(
					Util::NormalizeAng0To2Pi(movementActor->data.angle.z) - camYaw
				);
				float sign = autoRotateAngle < 0.0f ? -1.0f : 1.0f;
				autoRotateAngle = 
				(
					fabsf(autoRotateAngle) > PI / 2.0f ? 
					sign * PI - autoRotateAngle : 
					autoRotateAngle
				);
				// Dependent on how committed the player is to moving 
				// in their heading direction.
				autoRotateAngle *= lsData.normMag;
			}

			// Should've been updated earlier, so we'll just grab the value now.
			// Slow down rotation by an additional factor 
			// based on how long it has been since this player started moving.
			autoRotateAngleMult *= Util::InterpolateSmootherStep
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
			avgAutoRotateAngle += autoRotateAngle * autoRotateAngleMult;
		}

		// Four elevation change scenarios relative to the camera:
		// 1. Up a slope away from camera: pitch cam upward.
		// 2. Down a slope away from camera: pitch cam downward.
		// 3. Up a slope towards the camera: pitch cam downward.
		// 4. Down a slope towards the camera: pitch cam upward.
		avgAutoRotateAngle /= max(1, consideredPlayersCount);
		if (a_computePitch)
		{
			float signAdjustment = 1.0f;
			// Use camera origin position's path direction as the camera movement direction. 
			if (camOriginPointDirection.Length() > 0.0f) 
			{
				float originMovYaw = Util::DirectionToGameAngYaw(camOriginPointDirection); 
				float moveAngRelToFacing = Util::NormalizeAngToPi(originMovYaw - camYaw);
				signAdjustment = cosf(moveAngRelToFacing);
			}

			avgAutoRotateAngle *= signAdjustment;
		}

		return avgAutoRotateAngle;
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

		bool onScreen = false;
		auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr)
		{
			return false;
		}

		// Temporarily move the camera to the given position and set the given rotation.
		SetCamOrientation(a_camPos, a_rotation.x, a_rotation.y, true);

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

	bool CameraManager::PointOnScreenAtCamOrientationWorldspaceMargin
	(
		const RE::NiPoint3& a_point,
		const RE::NiPoint3& a_camPos, 
		const RE::NiPoint2& a_rotation, 
		const float& a_marginWorldDist)
	{
		// Is the given point in the camera's frustum at the given camera position and rotation,
		// also accounting for a worldspace distance margin around the given point.

		bool onScreen = false;
		auto niCamPtr = Util::GetNiCamera();
		// Need the NiCamera and Debug Overlay Menu view.
		if (!niCamPtr)
		{
			return false;
		}

		// Temporarily move the camera to the given position and set the given rotation.
		SetCamOrientation(a_camPos, a_rotation.x, a_rotation.y, true);

		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		// Compute four points offset from the origin point by the given margin distance
		// in the four axial world directions of the camera.
		auto camUp = playerCam->cameraRoot->local.rotate * RE::NiPoint3(0.0f, 0.0f, 1.0f);
		auto camRight = playerCam->cameraRoot->local.rotate * RE::NiPoint3(1.0f, 0.0f, 0.0f);
		auto camMaxXWorldPos = a_point + camRight * a_marginWorldDist;
		auto camMinXWorldPos = a_point + -camRight * a_marginWorldDist;
		auto camMaxYWorldPos = a_point + -camUp * a_marginWorldDist;
		auto camMinYWorldPos = a_point + camUp * a_marginWorldDist;
		
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

	void CameraManager::ResetCamData()
	{
		// Reset all camera data.
		
		// Reset controller IDs.
		controlCamCID = -1;
		focalPlayerCID = -1;
		
		// Starts with no adjustment mode active and in the autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		camAdjMode = CamAdjustmentMode::kNone;

		// Reset interp data.
		lockOnIndicatorOscillationInterpData->ResetData();
		movementAngleMultInterpData->Reset(false, true);
		movementPitchInterpData->ResetData();
		movementYawInterpData->ResetData();

		// Reset interp factors and ratio for blending pitch/yaw changes.
		camInterpFactor = Settings::fCamInterpFactor;

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
		delayedZoomInUnderExteriorRoof = delayedZoomOutUnderExteriorRoof = false;
		isAutoTrailing = true;
		isColliding = false;
		isManuallyPositioned = false;
		isLockedOn = false;
		lockInteriorOrientationOnInit = false;

		// Reset lock-on-related data.
		lockOnTargetInSight = false;
		camLockOnTargetHandle = RE::ActorHandle();
		lockOnActorReq = std::nullopt;

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
						SPDLOG_ERROR("[CAM] ERR: ResetCamData: Could not get third person state.");
					}
				}
				else
				{
					SPDLOG_ERROR("[CAM] ERR: ResetCamData: Could not get camera state.");
				}
			}
		}
		else
		{
			SPDLOG_ERROR("[CAM] ERR: ResetCamData: Could not get player cam.");
		}

		// Set rotation-related data.
		movementPitchRunningTotal = movementYawToCamRunningTotal = 0.0f;
		numMovementPitchReadings = numMovementYawToCamReadings = 0;

		// Positions.
		// Set focus point to the origin point.
		camOriginPoint = RE::NiPoint3();
		// Set average player height to offset the base origin point.
		avgPlayerHeight = 0.0f;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			avgPlayerHeight += p->mm->playerScaledHeight;
			camOriginPoint += p->coopActor->data.location;
		}
		
		avgPlayerHeight /= glob.livingPlayers;
		camOriginPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
		camOriginPoint.z += avgPlayerHeight;

		camFocusPoint =
		camLockOnFocusPoint = camOriginPoint;

		camOriginPointDirection = RE::NiPoint3();
		auto bounds = Util::GetVertCollPoints(camOriginPoint, 0.0f);
		camMaxAnchorPointZCoord = bounds.first;
		camMinAnchorPointZCoord = bounds.second;

		// Set target positions equal to the node/P1 looking at position.
		auto p1LookingAt = glob.player1Actor->GetLookingAtLocation();
		camBaseTargetPos =
		camTargetPos =
		camCollisionTargetPos =
		(
			playerCam && playerCam->cameraRoot ?
			playerCam->cameraRoot->world.translate : 
			p1LookingAt
		);
		
		// Set radial distance equal to the node's distance from the origin point.
		camRadialDistanceOffset = camSavedRadialDistanceOffset = 0.0f;
		camMinTrailingDistance = 100.0f;
		camTargetRadialDistance = 
		camTrueRadialDistance = camBaseTargetPos.GetDistance(camFocusPoint);
		// Reset base height and zoom offsets.
		camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;
		camBaseHeightOffset = camHeightOffset = 0.0f;
		
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
		}

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
		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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

		playerCam->worldFOV = 
		(
			exteriorCell ? 
			Settings::fCamExteriorFOV :
			Settings::fCamInteriorFOV
		);
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

		const auto& coopP1 = glob.coopPlayers[glob.player1CID];
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

		playerCam->worldFOV = 
		(
			exteriorCell ? 
			Settings::fCamExteriorFOV :
			Settings::fCamInteriorFOV
		);
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

		playerCam->worldFOV = 
		(
			exteriorCell ? 
			Settings::fCamExteriorFOV :
			Settings::fCamInteriorFOV
		);
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
	}

	void CameraManager::ToggleCoopCamera(bool a_enable)
	{
		// External request to toggle the co-op camera on/off.

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
					SPDLOG_DEBUG("[CAM] ToThirdPersonState. Lock obtained. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					isTogglingPOV = true;
				}
				else
				{
					// Could not obtain lock to toggle POV, 
					// so return here without enqueueing any tasks.
					SPDLOG_DEBUG("[CAM] ToThirdPersonState. Failed to obtain lock: (0x{:X})",
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					return;
				}
			}

			glob.taskRunner->AddTask
			(
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

					SPDLOG_DEBUG
					(
						"[CAM] ToThirdPersonState. "
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

		// Player is in combat (no focal player).
		const bool partyCamInCombat = glob.isInCoopCombat && focalPlayerCID == -1;
		// Player is moving their crosshair.
		bool playerMovingCrosshair = false;
		// No auto-rotate while there is a focal player.
		bool noAutoRotationWithFocalPlayer = false;
		// Is the current player performing a combat action?
		bool isPerformingCombatAction =  false;
		// Should suspend auto-rotation and interp towards 0.
		bool shouldSuspend = false;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}
			
			// By default, suspend if the focal player is not sprinting and not facing a target.
			noAutoRotationWithFocalPlayer |= 
			(
				focalPlayerCID != -1 &&
				p->controllerID == focalPlayerCID &&
				!p->pam->isSprinting && 
				!p->mm->reqFaceTarget
			);
			shouldSuspend |= noAutoRotationWithFocalPlayer;
			// No need to check the rest of the players.
			// Set directly to 0 and return.
			if (shouldSuspend)
			{
				movementAngleMultInterpData->Reset(true, true);
				return;
			}

			// To avoid affecting player aim, 
			// suspend auto-rotation when in combat, when a player is moving their crosshair,
			// or the focal player is not sprinting and facing the crosshair.
			playerMovingCrosshair |= p->pam->IsPerforming(InputAction::kMoveCrosshair);
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

			shouldSuspend |= focalPlayerCID == -1 && isPerformingCombatAction;
			if (shouldSuspend)
			{
				movementAngleMultInterpData->UpdateInterpolatedValue(false);
				return;
			}
		}
		
		// Interpolate towards 1 if we're not suspending auto-rotation.
		movementAngleMultInterpData->UpdateInterpolatedValue(true);
	}

	void CameraManager::UpdateCamHeight()
	{
		// Update the camera focus point's Z offset, or 'height' above the origin point.
		
		// No height offset to set when manually positioned.
		if (isManuallyPositioned)
		{
			return;
		}

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		// Can adjust height if:
		// 1. There is no focal player -AND-
		// 2. Not locked on or if there is no target or if zoom controls are enabled -AND-
		// 3. A player is controlling the camera and trying to adjust the height.
		bool canAdjustHeight = 
		{
			(
				!isLockedOn || 
				!camLockOnTargetPtr || 
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
			) &&
			(
				camAdjMode == CamAdjustmentMode::kZoom && 
				controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT
			)
		};
		// Save previous base height offset to restore later if the anchor points are bound.
		float prevBaseOffset = camBaseHeightOffset;
		if (canAdjustHeight)
		{
			const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
			const auto& rsX = rsData.xComp;
			const auto& rsY = rsData.yComp;
			const auto& rsMag = rsData.normMag;
			// Change height of the focus point if the x comp is larger than the y comp.
			if (fabsf(rsX) > fabsf(rsY))
			{
				// Right to increase height, left to decrease.
				camBaseHeightOffset += rsX * rsMag * camMaxMovementSpeed * *g_deltaTimeRealTime;
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
		float currentFocusZPos = 
		(
			focalPlayerCID == -1 ? 
			camOriginPoint.z + newHeight :
			camPlayerFocusPoint.z + newHeight
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
		if (isBound)
		{
			camBaseHeightOffset = prevBaseOffset;
		}
	}

	void CameraManager::UpdateCamRotation()
	{
		// Update the base and current camera pitch and yaw to set.
		
		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		// Cap rotation speed.
		float maxRotRads = camMaxAngRotRate * *g_deltaTimeRealTime;
		// Changes in pitch/yaw to apply.
		auto pitchDelta = 0.0f;
		auto yawDelta = 0.0f;
		// Right stick displacement components and magnitude.
		const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
		const float& rsX = rsData.xComp;
		const float& rsY = rsData.yComp;
		const float& rsMag = rsData.normMag;
		// Can still manually rotate the camera if there is no lock-on target
		// or if the lock-on assistance is set to zoom only.
		if (isAutoTrailing || 
			isManuallyPositioned || 
			!camLockOnTargetPtr || 
			Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
		{
			camMaxPitchAngMag = isAutoTrailing ? autoTrailPitchMax : PI / 2.0f;
			if (camAdjMode == CamAdjustmentMode::kRotate && 
				controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
			{
				if (rsMag != 0.0f)
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
					pitchDelta = maxRotRads * rsY * rsMag;
					camBaseTargetPosPitch -= pitchDelta;
				}
			}
		}

		// For auto-rotation, if a setting is enabled 
		// and the camera is not in manual positioning mode,
		// calculate pitch incline offset/yaw diff only when rotation controls are unlocked,
		// and the camera-controlling player is not rotating the camera.
		auto ui = RE::UI::GetSingleton();
		bool autoRotate = 
		{
			(Settings::bAutoRotateCamPitch || Settings::bAutoRotateCamYaw) &&
			(camAdjMode != CamAdjustmentMode::kRotate || rsMag == 0.0f) &&
			(
				(!isManuallyPositioned) && 
				(
					isAutoTrailing || 
					!camLockOnTargetPtr ||
					Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
				)
			) &&
			(ui && !ui->GameIsPaused())
		};
		if (autoRotate)
		{
			UpdateAutoRotateAngleMult();
			if (Settings::bAutoRotateCamPitch && !isColliding)
			{
				movementPitchInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
				if (movementPitchInterpData->secsSinceUpdate >= 
					movementPitchInterpData->secsUpdateInterval)
				{
					// Sometimes becomes NAN, and must be reset. Temp solution.
					movementPitchRunningTotal = 
					(
						isnan(movementPitchRunningTotal) ?
						0.0f : 
						movementPitchRunningTotal
					);
					auto movementPitch = 
					(
						numMovementPitchReadings != 0.0f ? 
						movementPitchRunningTotal /
						static_cast<float>(numMovementPitchReadings) : 
						0.0f
					);
					SetMovementPitchRunningTotal(true);
					float sign = movementPitch < 0.0f ? -1.0f : 1.0f;
					// It (maybe) just works. 
					// Used Desmos (https://www.desmos.com/calculator)
					// to create a curve that smooths out the changes in camera pitch relative
					// to average support surface/vertical velocity pitch.
					movementPitch = 
					(
						(Settings::fAutoRotateCamPitchRateMult) *
						(1.5f * tanf(0.4f * movementPitch - 0.1f) * cosf(movementPitch) + 0.15f)
					);
					// Pitch increments/decrements are smaller when approaching PI/2 
					// in the direction of the average movement pitch. 
					// Done to prevent over-adjustment when already at a steep pitch.
					float proportionOfMaxPitch = 
					(
						(sign == 1.0f) ? 
						1.0f - camBaseTargetPosPitch / (PI / 2.0f) : 
						camBaseTargetPosPitch / (PI / 2.0f) + 1.0f
					);
					movementPitch *= proportionOfMaxPitch;
					movementPitchInterpData->ShiftEndpoints(movementPitch);
					movementPitchInterpData->SetTimeSinceUpdate(*g_deltaTimeRealTime);
				}

				SetMovementPitchRunningTotal(false);
				float tRatio = min
				(
					movementPitchInterpData->secsSinceUpdate / 
					movementPitchInterpData->secsUpdateInterval, 
					1.0f
				);
				movementPitchInterpData->InterpolateSmootherStep(tRatio);
			}

			if (Settings::bAutoRotateCamYaw)
			{
				movementYawInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
				if (movementYawInterpData->secsSinceUpdate >= 
					movementYawInterpData->secsUpdateInterval)
				{
					movementYawToCamRunningTotal = 
					(
						isnan(movementYawToCamRunningTotal) ? 
						0.0f : 
						movementYawToCamRunningTotal
					);
					float movementYaw = 
					(
						numMovementYawToCamReadings != 0.0f ? 
						movementYawToCamRunningTotal / 
						static_cast<float>(numMovementYawToCamReadings) :
						0.0f
					);

					SetMovementYawToCamRunningTotal(true);
					// Since the party's averaged movement direction varies less relative to 
					// the camera's facing direction at larger trailing distances,
					// less auto-rotation is required to keep players in frame.
					// Apply radial distance factor to decrease yaw auto-rotation 
					// when the party moves farther from the camera.
					float radialDistFactor = Util::InterpolateSmootherStep
					(
						0.1f,
						1.0f,
						sqrtf
						(
							camMinTrailingDistance / 
							max
							(
								camMinTrailingDistance, 
								camTargetRadialDistance
							)
						)
					);
					movementYaw = std::clamp
					(
						(movementYaw / (PI / 2.0f)) * 
						Settings::fAutoRotateCamYawRateMult *
						radialDistFactor *
						camMaxAngRotRate * 
						*g_deltaTimeRealTime, 
						-fabsf(movementYaw), 
						fabsf(movementYaw)
					);
					movementYawInterpData->ShiftEndpoints(movementYaw);
					movementYawInterpData->SetTimeSinceUpdate(*g_deltaTimeRealTime);
				}

				SetMovementYawToCamRunningTotal(false);
				float tRatio = min
				(
					movementYawInterpData->secsSinceUpdate / 
					movementYawInterpData->secsUpdateInterval, 
					1.0f
				);
				auto prev = movementYawInterpData->current;
				movementYawInterpData->InterpolateSmootherStep(tRatio);
			}
		}
		else if (numMovementPitchReadings != 0 || 
				 numMovementYawToCamReadings != 0 || 
				 movementAngleMultInterpData->value != 0.0f)
		{
			// If not already reset, reset movement pitch/yaw totals 
			// when auto-rotate is not active.
			SetMovementPitchRunningTotal(true);
			SetMovementYawToCamRunningTotal(true);
			movementAngleMultInterpData->Reset(true, true);
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
				isAutoTrailing || 
				!camLockOnTargetPtr || 
				Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom
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
				if (Settings::bAutoRotateCamPitch)
				{
					movementPitchDelta = movementPitchInterpData->current;
					if (isnan(movementPitchDelta) || isinf(movementPitchDelta))
					{
						movementPitchDelta = 0.0f;
					}

					camTargetPosPitch = std::clamp
					(
						camBaseTargetPosPitch + movementPitchDelta, 
						-camMaxPitchAngMag, 
						camMaxPitchAngMag
					);
				}
				else
				{
					camTargetPosPitch = std::clamp
					(
						camBaseTargetPosPitch, 
						-camMaxPitchAngMag, 
						camMaxPitchAngMag
					);
				}

				if (Settings::bAutoRotateCamYaw)
				{
					movementYawDelta = movementYawInterpData->current;
					if (isnan(movementYawDelta) || isinf(movementYawDelta))
					{
						movementYawDelta = 0.0f;
					}

					camTargetPosYaw =
					camBaseTargetPosYaw = Util::NormalizeAng0To2Pi
					(
						camBaseTargetPosYaw + movementYawDelta
					);
				}
				else
				{
					camTargetPosYaw = 
					camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw);
				}
			}

			if (focalPlayerCID == -1) 
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
			else
			{
				camCurrentPitchToFocus = Util::NormalizeAngToPi
				(
					Util::GetPitchBetweenPositions(camTargetPos, camPlayerFocusPoint)
				);
				camCurrentYawToFocus = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions(camTargetPos, camPlayerFocusPoint)
				);
			}

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
				if (focalPlayerCID != -1 || isColliding) 
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
			else if (lockInteriorOrientationOnInit)
			{
				// All set to base when not using camera collisions or when orientation is locked.
				camPitch =
				camTargetPosPitch = std::clamp
				(
					camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag
				);
				camYaw = camTargetPosYaw = camBaseTargetPosYaw;
			}
			else
			{
				if (focalPlayerCID == -1)
				{
					// Set directly to pitch/yaw to focus point values 
					// if collisions are not enabled, since we don't have to worry about 
					// performing the hacky blending above.
					camPitch = std::clamp
					(
						camCurrentPitchToFocus, -camMaxPitchAngMag, camMaxPitchAngMag
					);
					camYaw = camCurrentYawToFocus;
				}
				else
				{
					// Rotate to directly face the focal player.
					camPitch = Util::GetPitchBetweenPositions(camTargetPos, camPlayerFocusPoint);
					camYaw = Util::GetYawBetweenPositions(camTargetPos, camPlayerFocusPoint);
				}
			}
		}
		else if (isLockedOn &&
				 camLockOnTargetPtr && 
				 Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kZoom)
		{
			// NOTE: 
			// Temporarily using the base target position as the rotation target 
			// to lessen camera jumping relative to a rapidly changing collision focus point.
			auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
			if (!camLockOnTargetPtr)
			{
				// No target. Bye.
				return;
			}

			bool adjustingHeight = 
			(
				camAdjMode == CamAdjustmentMode::kZoom && 
				fabsf(rsX) > fabsf(rsY) &&
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
			);
			float pitchToTarget = Util::NormalizeAngToPi
			(
				(Util::GetPitchBetweenPositions(camTargetPos, camLockOnFocusPoint)) * 
				(adjustingHeight ? 1.0f : 0.5f)
			);
			float yawToTarget = Util::NormalizeAng0To2Pi
			(
				Util::GetYawBetweenPositions(camTargetPos, camLockOnFocusPoint)
			);

			// Cap max change in pitch/yaw to the maximum rotatable angle this frame,
			// based on the camera's set max rotation speed.
			pitchDelta = std::clamp
			(
				Util::NormalizeAngToPi(pitchToTarget - camPitch), 
				-maxRotRads, 
				maxRotRads
			);
			yawDelta = std::clamp
			(
				Util::NormalizeAngToPi(yawToTarget - camYaw), 
				-maxRotRads, 
				maxRotRads
			);
			
			// Set the camera orientation to the previous frame's orientation,
			// since it may have been modified since the start of this iteration of the main task.
			RE::NiPoint3 targetScreenPos = Util::WorldToScreenPoint3
			(
				camLockOnFocusPoint, false
			);
			RE::NiPoint3 screenCenterPos = RE::NiPoint3
			(
				0.5f * DebugAPI::screenResX,
				0.5f * DebugAPI::screenResY,
				-1.0f
			);

			// Slow down pitch change when the target is close to the camera.
			float pitchDepthMult = std::clamp
			(
				max(0.0f, targetScreenPos.z - 0.9f) * 10.0f,
				0.0f,
				1.0f
			);
			// Multiplier for the pitch angle to rotate in order to directly face the target.
			float pitchDeltaMult = 1.0f;
			// Essentially, start pitching to face the target faster when it is beyond the edges
			// of the screen.
			if (targetScreenPos.y < 0.0f)
			{
				pitchDeltaMult = Util::InterpolateEaseOut
				(
					0.15f,
					1.0f, 
					std::clamp(-targetScreenPos.y / (0.1f * DebugAPI::screenResY), 0.0f, 1.0f),
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
						10.0f * (targetScreenPos.y / DebugAPI::screenResY - 1.0f), 0.0f, 1.0f
					),
					3.0f
				);
			}
			else
			{
				pitchDeltaMult = Util::InterpolateEaseIn
				(
					0.0f,
					0.15f, 
					std::clamp
					(
						fabsf
						(
							targetScreenPos.y - screenCenterPos.y
						) / (0.5f * DebugAPI::screenResY),
						0.0f, 
						1.0f
					),
					3.0f
				);
			}
			
			// Rotate about the Z axis faster when the target is behind the camera.
			float yawDepthMult = max(1.0f, -targetScreenPos.z + 1.0f * 2.0f);
			// Multiplier for the yaw angle to rotate in order to directly face the target.
			float yawDeltaMult = 1.0f;
			// Slow down rotation when the target is on the lower half of the screen.
			float vertScreenPosYawMult = Util::InterpolateEaseIn
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
							(targetScreenPos.y - screenCenterPos.y)	 / (DebugAPI::screenResY)
						),
						-0.5f, 
						0.0f
					) + 1.0f
				),
				3.0f
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
						fabsf(targetScreenPos.x - screenCenterPos.x) / 
						(0.5f * DebugAPI::screenResX)
					),
					0.0f, 
					1.0f
				),
				7.0f
			);

			yawDelta *= std::clamp(yawDepthMult * yawDeltaMult * vertScreenPosYawMult, 0.0f, 1.0f);
			// Approach the pitch to target directly
			// when a player is adjusting the cam height offset.
			// Want to prevent excessive auto-zoom-out if the target is not on screen
			// once the camera adjustment mode returns to default.
			if (adjustingHeight)
			{
				pitchDelta *= min
				(
					1.0f,
					Util::GetElapsedSeconds
					(
						glob.coopPlayers[controlCamCID]->pam->paStatesList
						[!InputAction::kZoomCam - !InputAction::kFirstAction].startTP
					)
				);
			}
			else
			{
				pitchDelta *= std::clamp(pitchDepthMult * pitchDeltaMult, 0.0f, 1.0f);
			}

			// Apply the deltas for this frame.
			camPitch = Util::NormalizeAngToPi(camPitch + pitchDelta);
			camYaw = Util::NormalizeAng0To2Pi(camYaw + yawDelta);
			
			camTargetPosPitch = camBaseTargetPosPitch = camPitch;
			// Set to zero so it has no bearing on zoom/focus point Z offset changes.
			//camTargetPosPitch = camBaseTargetPosPitch = 0.0f;
			// Set equal to cam facing direction yaw angle.
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

	void CameraManager::UpdateCamZoom()
	{
		// Update the camera's zoom, auto-zooming out
		// to keep all players in view and auto- zooming in
		// when under an exterior roof, as necessary.

		// Set the minimum trailing distance first.
		if (focalPlayerCID == -1)
		{
			camMinTrailingDistance = 100.0f;
		}
		else
		{
			camMinTrailingDistance = 200.0f;
		}

		// No zoom when in manual positioning mode.
		if (isManuallyPositioned)
		{
			return;
		}
		
		// NOTE:
		// Will uncomment if frequent stuttering on rotation starts occurring again.
		// Auto-zooming in/out while rotating the camera can contribute to stuttering,
		// so do not auto-zoom until rotation is complete.
		/*if (camAdjMode == CamAdjustmentMode::kRotate)
		{
			return;
		}*/

		// Auto-zoom in/out.
		const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
		const auto& rsX = rsData.xComp;
		const auto& rsY = rsData.yComp;
		const auto& rsMag = rsData.normMag;
		const float prevRadialDistance = camTargetRadialDistance;
		// Zoom offset decreases (zoom in) when moving the RS up,
		// and increases (zoom out) when moving the RS down.
		// Behaves the same for all camera modes.
		// Only adjust base radial distance if requested.
		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		// Can adjust zoom if:
		// 1. Not locked on or if there is no target or if zoom controls are enabled -AND-
		// 2. A player is controlling the camera and trying to adjust the zoom.
		bool canAdjustZoom = 
		{
			(
				!isLockedOn || 
				!camLockOnTargetPtr || 
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull
			) &&
			(
				camAdjMode == CamAdjustmentMode::kZoom && 
				controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT
			)
		};
		if (canAdjustZoom)
		{
			if (fabsf(rsY) > fabsf(rsX))
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
					if (glob.coopPlayers[controlCamCID]->pam->JustStarted
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

					if ((camTrueRadialDistance < camTargetRadialDistance && rsY > 0.0f) ||
						(camTrueRadialDistance > camTargetRadialDistance && rsY < 0.0f))
					{
						// Do not exceed the max camera movement speed when zooming in/out.
						camRadialDistanceOffset = max
						(
							0.0f, 
							camRadialDistanceOffset - 
							(*g_deltaTimeRealTime * camMaxMovementSpeed * rsY * rsMag)
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
						(*g_deltaTimeRealTime * camMaxMovementSpeed * rsY * rsMag)
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
		// when zoomed out beyond a variable distance.
		// Zooming out beyond this distance will result in the game 
		// considering all players offscreen (and no raycast hits),
		// even though their positions are in front of the camera and visually in view.
		// Get approximation for the max settable radial distance by binary searching a range.
		float radialDistanceRangeMin = 0.0f;
		float radialDistanceRangeMax = Settings::fMaxRaycastAndZoomOutDistance;
		float radialDistanceRangeMid = Settings::fMaxRaycastAndZoomOutDistance / 2.0f;
		float lastOnScreenRadialDist = Settings::fMaxRaycastAndZoomOutDistance;
		auto focusPoint = focalPlayerCID == -1 ? camFocusPoint : camPlayerFocusPoint;
		auto dirFromFocus = camBaseTargetPos - focusPoint;
		dirFromFocus.Unitize();
		// Position from which to test for visibility of all players.
		auto onScreenTestPos = focusPoint + dirFromFocus * radialDistanceRangeMid;
		const RE::NiPoint2 rotationToCheck = RE::NiPoint2(camTargetPosPitch, camTargetPosYaw);
		bool minDiffReached = false;
		bool currentCheckOnScreen = false;
		uint32_t i = 0;
		// With the default min zoom delta, this iterates 11 times at most.
		while (!minDiffReached)
		{
			currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation
			(
				onScreenTestPos, rotationToCheck, true
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
			if (focalPlayerCID == -1) 
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
				// No changes to apply when there is a focal player.
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
			camAdjMode == CamAdjustmentMode::kZoom && fabsf(rsX) > fabsf(rsY)
		);
		if (!adjustingHeight)
		{
			if (focalPlayerCID == -1)
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
				// Will iterate at most 22 times with the default min zoom delta.
				bool lockOnTargetOnScreen = false;
				while (!minZoomRangeReached)
				{
					currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation
					(
						onScreenTestPos, rotationToCheck, true
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
					/*SPDLOG_DEBUG
					(
						"[CAM] UpdateCamZoom: ON SCREEN: "
						"{} from ({}, {}, {}) and offset {}.",
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
					/*SPDLOG_DEBUG
					(
						"[CAM] UpdateCamZoom: OFF SCREEN BELOW MAX: "
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
					/*SPDLOG_DEBUG
					(
						"[CAM] UpdateCamZoom: OFF SCREEN ABOVE MAX: "
						"{} from ({}, {}, {}) and prev {}.",
						camTargetRadialDistance, 
						radialDistanceRangeMin,
						radialDistanceRangeMid,
						radialDistanceRangeMax,
						prevRadialDistance
					);*/
				}
			}
			else
			{
				// Offset on top of the minimum trailing distance when there is a focal player.
				camTargetRadialDistance = camMinTrailingDistance + camRadialDistanceOffset;
			}
		}
		
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

		// Some interior cells have fog and a max zoom out distance for the camera
		// before checking if points are on screen fail (messes with auto-zoom).
		// Switching to skybox only sometimes circumvents this issue 
		// and allows for uncapped zooming out in interior cells.
		// The alternative - disabling the sky altogether - 
		// has some clearly undesirable side effects 
		// like creating much harsher lighting throughout the cell.
		Util::SetSkyboxModeForCell(currentCell);
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
				if (!player3DPtr)
				{
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

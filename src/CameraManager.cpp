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
		camBaseFocusPoint = 
		camBaseOriginPoint = 
		camBaseTargetPos = 
		camCollisionFocusPoint = 
		camCollisionOriginPoint = 
		camCollisionTargetPos = 
		camCollisionTargetPos2 =
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
				tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->cameraStates[RE::CameraState::kThirdPerson].get());
			}
		}

		// Current cell.
		currentCell = nullptr;
		// Starts with no adjustment mode active and in the autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		camAdjMode = CamAdjustmentMode::kNone;
		// Set InterpData and accompanying update intervals.
		lockOnIndicatorOscillationInterpData = std::make_unique<InterpolationData<float>>
		(
			0.0f,
			0.0f, 
			0.0f, 
			Settings::fSecsCamLockOnIndicatorOscillationUpdate
		);
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
		camBaseRadialDistance = 
		camCollisionRadialDistance = 
		camTargetRadialDistance = 
		camSavedBaseRadialDistance = 400.0f;
		camBaseFocusPointZOffset = camFocusPointZOffset = 0.0f;
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
		// Reset time points.
		ResetTPs();
		// Clear out objects to fade.
		ResetFadeOnObjects();
	}


#pragma region MANAGER_FUNCS_IMPL
	void CameraManager::MainTask()
	{
		if (!playerCam)
		{
			playerCam = RE::PlayerCamera::GetSingleton();
		}

		if (playerCam) 
		{
			// Disable auto-vanity cam.
			playerCam->idleTimer = FLT_MAX;
			playerCam->allowAutoVanityMode = false;
			if (!tpState) 
			{
				tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->cameraStates[RE::CameraState::kThirdPerson].get());
			}

			bool isInSupportedCamState = 
			(
				playerCam->currentState->id == RE::CameraState::kThirdPerson ||
				playerCam->currentState->id == RE::CameraState::kMount ||
				playerCam->currentState->id == RE::CameraState::kDragon ||
				playerCam->currentState->id == RE::CameraState::kFurniture ||
				playerCam->currentState->id == RE::CameraState::kBleedout
			);
			// Auto-switch back to the third person camera state if currently not in a supported state.
			if (!isTogglingPOV && playerCam && playerCam->currentState && !isInSupportedCamState)
			{
				ToThirdPersonState(playerCam->currentState->id == RE::CameraState::kFirstPerson);
			}

			isAutoTrailing = camState == CamState::kAutoTrail;
			isLockedOn = camState == CamState::kLockOn;
			isManuallyPositioned = camState == CamState::kManualPositioning;

			// On state change, reset TPs.
			if (camState != prevCamState)
			{
				ResetTPs();
				// Set cam target position rotations to current pitch/yaw if switching away from lock on state.
				if (prevCamState == CamState::kLockOn) 
				{
					camBaseTargetPosPitch = camTargetPosPitch = camPitch;
					camBaseTargetPosYaw = camTargetPosYaw = camYaw;
				}
			}

			prevCamState = camState;
			if (!isTogglingPOV)
			{
				// Set base interpolation factor.
				if (camState == CamState::kManualPositioning) 
				{
					camInterpFactor = Settings::fCamManualPosInterpFactor;
				}
				else
				{
					camInterpFactor = 
					(
						Settings::bCamCollisions ? 
						Settings::fCamCollisionInterpFactor : 
						Settings::fCamNoCollisionInterpFactor
					);
				}

				// Framerate dependent factor.
				camFrameDepInterpFactor = min(1.0f, *g_deltaTimeRealTime * 60.0f * camInterpFactor);

				UpdateParentCell();
				CalcNextOriginPoint();
				CalcNextFocusPoint();
				CalcNextTargetPosition();
				CheckLockOnTarget();
				UpdatePlayerFadeAmounts();

				if (isAutoTrailing)
				{
					// Cam controls:
					//
					// Toggle one of the other two modes off to switch to this mode:
					// Adjustment modes:
					// 1. None:
					// Camera automatically follows focus point.
					// 2. Rotate:
					// Rotate about a circle centered at focus point.
					// RS left/right to change yaw about the focus point.
					// RS up/down to change the pitch.
					// 3. Zoom: 
					// RS up/down to increase/decrease zoom,
					// RS left/right to decrease/increase height.
					UpdateCamHeight();
					UpdateCamRotation();
					UpdateCamZoom();
				}
				else if (isLockedOn)
				{
					// Cam controls:
					// Same controls as for auto-trail, but zoom/rotation/height
					// adjustment can be disabled depending on the set lock on assistance mode.
					UpdateCamZoom();
					UpdateCamRotation();
					UpdateCamHeight();
				}
				else
				{
					// Cam controls:
					// Adjustment modes:
					// 1. None: 
					// Camera stays put at its last configured position.
					// 2. Rotate: 
					// RS left/right to change cam yaw.
					// RS up/down to change pitch.
					// 3. Zoom: 
					// LS up/down/left/right to move forward/backward/left/right in the camera's facing direction.
					UpdateCamHeight();
					UpdateCamRotation();
				}

				// Adjust fade for obstructions between the camera and active players.
				if (Settings::bFadeObstructions)
				{
					FadeObstructions();
				}

				// Set the newly calculated camera position and rotation.
				SetCamOrientation();
			}
		}
	}

	void CameraManager::PrePauseTask()
	{
		ALYSLC::Log("[CAM] PrePauseTask");

		// Reset no fade flags for all players.
		SetPlayerFadePrevention(false);
		// Add back camera-actor collisions before switching to default cam.
		SetCamActorCollisions(true);
		// Ensure all players are visible.
		UpdatePlayerFadeAmounts(true);
		Util::ToggleAllControls(true);
		Util::SetPlayerAIDriven(false);
		Util::ResetTPCamOrientation();
		// Reset toggle press flag every time before pausing.
		toggleBindPressedWhileWaiting = false;

		// NOTE: Check for stability on cell change:
		// Reset fade on handled objects.
		ResetFadeOnObjects();
	}

	void CameraManager::PreStartTask()
	{
		ALYSLC::Log("[CAM] PreStartTask: cam FOV: {}", 
			playerCam ? playerCam->worldFOV : -1.0f);
		// Prevent the game from fading all players while the camera is active.
		SetPlayerFadePrevention(true);
		// Remove camera-actor collisions before switching to co-op cam.
		SetCamActorCollisions(false);
		// Refresh data.
		RefreshData();
		// Ensure all players are visible.
		UpdatePlayerFadeAmounts(true);
		// NOTE: Check for stability on cell change:
		// Reset fade on handled objects.
		ResetFadeOnObjects();
	}

	void CameraManager::RefreshData()
	{
		ALYSLC::Log("[CAM] RefreshData");

		// Update parent cell.
		UpdateParentCell();
		// Reset all time points and orientation data.
		ResetTPs();
		ResetCamData();

		// Reset toggle state.
		isTogglingPOV = false;
		waitForToggle = false;
	}

	const ManagerState CameraManager::ShouldSelfPause()
	{
		// Check if all players are valid, and if one isn't, pause.
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				bool isInvalid = 
				{
					p->coopActor->IsDisabled() ||
					!p->coopActor->Is3DLoaded() ||
					!p->coopActor->loadedData ||
					!p->coopActor->currentProcess ||
					!p->coopActor->GetCharController() ||
					(p->coopActor->parentCell && !p->coopActor->parentCell->IsAttached())
				};

				if (isInvalid)
				{
					return ManagerState::kPaused;
				}
			}
		}

		// Pause when the map menu is open to prevent glitches upon closure and to also enable fast travel while in the map menu.
		// Pause when a fader menu opens since P1 is likely being repositioned.
		if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::MapMenu::MENU_NAME))
		{
			return ManagerState::kPaused;
		}

		if (isTogglingPOV) 
		{
			return ManagerState::kPaused;
		}

		// Keep camera paused until no longer mining.
		if (playerCam->currentState->id == RE::CameraState::kFurniture && glob.player1Actor->GetOccupiedFurniture())
		{
			auto furnitureRef = Util::GetRefrPtrFromHandle(glob.player1Actor->GetOccupiedFurniture());
			auto furniture = 
			(
				furnitureRef && furnitureRef.get() && furnitureRef->GetBaseObject() && 
				furnitureRef->GetBaseObject()->Is(RE::FormType::Furniture) ? 
				furnitureRef->GetBaseObject()->As<RE::TESFurniture>() : 
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
			if (isTogglingPOV)
			{
				return currentState;
			}

			// First, check for player validity.
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive)
				{
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
			}

			// Then ensure the map/fader menus are not open.
			// Pause when the map menu is open to prevent glitches upon closure and to also enable fast travel while in the map menu.
			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::MapMenu::MENU_NAME))
			{
				return currentState;
			}

			// Have to pause here because the player will stop mining while the camera is enabled, even if the camera's current state is set to furniture.
			if (playerCam->currentState->id == RE::CameraState::kFurniture && glob.player1Actor->GetOccupiedFurniture())
			{
				auto furnitureRef = Util::GetRefrPtrFromHandle(glob.player1Actor->GetOccupiedFurniture());
				auto furniture = 
				(
					furnitureRef && furnitureRef.get() && furnitureRef->GetBaseObject() &&
					furnitureRef->GetBaseObject()->Is(RE::FormType::Furniture) ?
					furnitureRef->GetBaseObject()->As<RE::TESFurniture>() :
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

			// Next, ensure that all menus that pause the game are closed and that either
			// the cam toggle bind was released or all players are moving.
			if (allPlayersValid && waitForToggle)
			{
				auto ui = RE::UI::GetSingleton();
				auto dataHandler = RE::TESDataHandler::GetSingleton();
				bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				bool allMenusClosed = !ui->GameIsPaused() && ui->IsSavingAllowed() && onlyAlwaysOpen;
				bool isAutoSaving = dataHandler->autoSaving || dataHandler->saveLoadGame;
				bool shouldResume = false;
				// Resume if a co-op session is active, all menus are closed, the game is not autosaving, and
				// the thread state is running or paused with all players moving after cell change.
				bool menusClosed = allMenusClosed && !isAutoSaving;

				if (menusClosed)
				{
					// RS click used as cam toggle button (POV switch disabled).
					auto controlMap = RE::ControlMap::GetSingleton();
					auto userEvents = RE::UserEvents::GetSingleton();
					camToggleXIMask =
					(
						controlMap && userEvents ?
						controlMap->GetMappedKey(userEvents->togglePOV, RE::INPUT_DEVICE::kGamepad) :
						GAMEPAD_MASK_RIGHT_THUMB
					);
					// Check if the toggle bind is pressed and released by player 1.
					XINPUT_STATE tempState;
					ZeroMemory(&tempState, sizeof(XINPUT_STATE));
					if (!(XInputGetState(glob.player1CID, &tempState)) == ERROR_SUCCESS)
					{
						return ManagerState::kPaused;
					}

					toggleBindPressedWhileWaiting = 
					(
						(toggleBindPressedWhileWaiting) || 
						((tempState.Gamepad.wButtons & camToggleXIMask) == camToggleXIMask)
					);
					shouldResume = (toggleBindPressedWhileWaiting) && ((tempState.Gamepad.wButtons & camToggleXIMask) == 0);
				}

				if (shouldResume)
				{
					// Reset toggle state.
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

#pragma endregion

	bool CameraManager::AllPlayersOnScreenAtCamOrientation(const RE::NiPoint3& a_camPos, const RE::NiPoint2& a_rotation, bool&& a_usePlayerPos, const std::vector<RE::BSFixedString>&& a_nodeNamesToCheck)
	{
		// Check if all players are in within the camera's frustum at the given position and rotation.
		// Check the player's refr position or check a list of player nodes.

		const auto strings = RE::FixedStrings::GetSingleton();
		bool allPlayersInFrontOfPoint = true;
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				if (a_usePlayerPos)
				{
					allPlayersInFrontOfPoint &= PointOnScreenAtCamOrientation
					(
						p->coopActor->data.location + RE::NiPoint3(0.0f, 0.0f, p->coopActor->GetHeight() / 2.0f), 
						a_camPos,
						a_rotation, 
						DebugAPI::screenResX / 10.0f
					);
				}
				else
				{

					// Invalid 3D for one player means not all players 
					// are in front of the camera. Return early.
					auto loadedData = p->coopActor->loadedData;
					if (!loadedData)
					{
						return false;
					}

					auto data3D = loadedData->data3D;
					if (!data3D || !data3D->parent)
					{
						return false;
					}

					bool onePlayerNodeOnScreen = false;
					if (a_nodeNamesToCheck.size() > 0)
					{
						for (const auto& nodeName : a_nodeNamesToCheck)
						{
							// Minimum of one node must be visible from the camera target position
							// to consider the player as in view of the camera.
							if (auto nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); nodePtr && nodePtr.get())
							{
								onePlayerNodeOnScreen |= PointOnScreenAtCamOrientation
								(
									nodePtr->world.translate, 
									a_camPos, 
									a_rotation, 
									DebugAPI::screenResX / 10.0f
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
						for (const auto& nodeName : GlobalCoopData::CAM_VISIBILITY_NPC_NODES)
						{
							// Minimum of one node must be visible from the camera target position
							// to consider the player as in view of the camera.
							if (auto nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); nodePtr && nodePtr.get())
							{
								onePlayerNodeOnScreen |= PointOnScreenAtCamOrientation
								(
									nodePtr->world.translate, 
									a_camPos, 
									a_rotation, 
									DebugAPI::screenResX / 10.0f
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

					allPlayersInFrontOfPoint &= onePlayerNodeOnScreen;
				}

				// Break once one player is not on screen at this point.
				if (!allPlayersInFrontOfPoint)
				{
					break;
				}
			}
		}

		return allPlayersInFrontOfPoint;
	}

	void CameraManager::CalcNextFocusPoint()
	{
		// Calculate the next focus point (origin point offset along the Z axis).
		// If in auto trail mode or locked on without a valid target,
		// adjust the focus point relative to the origin point.

		auto camNodePos = tpState->camera->cameraRoot->world.translate;
		auto oldFocusPoint = camFocusPoint;
		auto prevOffset = camFocusPointZOffset;

		camCollisionFocusPoint = RE::NiPoint3
		(
			camCollisionOriginPoint.x, 
			camCollisionOriginPoint.y, 
			camCollisionOriginPoint.z + camFocusPointZOffset
		);
		if (Settings::bCamCollisions)
		{
			if (!isManuallyPositioned)
			{
				camCollisionFocusPoint.z = camCollisionOriginPoint.z + camFocusPointZOffset;
			}
			else
			{
				camFocusPoint = camCollisionFocusPoint = camNodePos;
			}

			camBaseFocusPoint = RE::NiPoint3
			(
				camBaseOriginPoint.x, 
				camBaseOriginPoint.y, 
				camBaseOriginPoint.z + camFocusPointZOffset
			);
			camFocusPoint = camCollisionFocusPoint;
		}
		else
		{
			camFocusPoint = camBaseFocusPoint = RE::NiPoint3
			(
				camOriginPoint.x, 
				camOriginPoint.y, 
				camOriginPoint.z + camFocusPointZOffset
			);
		}
	}

	void CameraManager::CalcNextOriginPoint()
	{
		// Calculate the next target origin point.
		// Base point is equidistant to all players.
		// Other derived origin points account for collisions with geometry
		// and are kept from going 'out of bounds' to a normally unreachable position, 
		// as this point is vital for calculating the cam target positions.

		auto oldOriginPoint = camOriginPoint;
		auto oldBaseOriginPoint = camBaseOriginPoint;
		bool originViewObstructed = false;
		bool hitToBasePos = false;
		bool hitToCollisionPos = false;
		std::pair<float, float> bounds{ oldOriginPoint.z, oldOriginPoint.z };
		float minZOffset = std::clamp(avgPlayerHeight, 50.0f, 100.0f);

		//====================
		//[Base Origin Point]:
		//====================

		camBaseOriginPoint = RE::NiPoint3();
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				camBaseOriginPoint += p->coopActor->data.location;
			}
		}

		// Base origin point before processing.
		camBaseOriginPoint *= (1.0f / static_cast<float>(glob.livingPlayers));
		camBaseOriginPoint.z += avgPlayerHeight;

		//========================
		//[Modified Origin Point]:
		//========================
		// Set the next origin point by accounting for collisions
		// when moving from the previous origin point
		// to the new base origin point.
		// Want to ensure the origin point is in a valid, reachable position,
		// and not clipping through geometry.

		glm::vec4 castStartPoint{ ToVec4(oldOriginPoint) };
		glm::vec4 castEndPoint{ ToVec4(camBaseOriginPoint) };
		auto result = Raycast::CastRay(castStartPoint, castEndPoint, camAnchorPointHullSize);
		if (result.hit)
		{
			hitToBasePos = true;
		}

		// Get point above ground at the base origin point's XY coords.
		RE::NiPoint3 basePointAboveGround = camBaseOriginPoint;
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
		camCollisionOriginPoint = basePointAboveGround;
		// If no LOS to a player at the base above-ground position, move to above-ground raycast collision position.
		if (originViewObstructed = NoPlayersVisibleAtPoint(camCollisionOriginPoint, true); originViewObstructed)
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
			result = Raycast::CastRay(castStartPoint, castEndPoint, camAnchorPointHullSize);
			if (result.hit)
			{
				hitToCollisionPos = true;
				camCollisionOriginPoint = ToNiPoint3(result.hitPos + result.rayNormal * camAnchorPointHullSize);
			}
			else
			{
				camCollisionOriginPoint = ToNiPoint3(castEndPoint);
			}

		}

		// Only bound above and below + set min/max anchor point positions
		// when there is a clear path to the next origin position.
		// Checking bounds during collisions leads to inconsistent shifts
		// to both the lower and upper bounds if the collision point shifts
		// the next origin position up or down, e.g. riding up a post in one of Solitude's guard towers.
		if (!hitToBasePos && !hitToCollisionPos) 
		{
			bounds = Util::GetVertCollPoints(camCollisionOriginPoint, 0.0f);
			camMaxAnchorPointZCoord = bounds.first;
			camMinAnchorPointZCoord = bounds.second;
		}

		if (Settings::bCamCollisions)
		{
			if (Settings::bOriginPointSmoothing)
			{
				camBaseOriginPoint.x = Util::InterpolateSmootherStep
				(
					oldBaseOriginPoint.x, camBaseOriginPoint.x, camFrameDepInterpFactor);
				camBaseOriginPoint.y = Util::InterpolateSmootherStep
				(
					oldBaseOriginPoint.y, camBaseOriginPoint.y, camFrameDepInterpFactor
				);
				camBaseOriginPoint.z = Util::InterpolateSmootherStep
				(
					oldBaseOriginPoint.z, camBaseOriginPoint.z, camFrameDepInterpFactor
				);
				camCollisionOriginPoint.x = Util::InterpolateSmootherStep
				(
					oldOriginPoint.x, camCollisionOriginPoint.x, camFrameDepInterpFactor
				);
				camCollisionOriginPoint.y = Util::InterpolateSmootherStep
				(
					oldOriginPoint.y, camCollisionOriginPoint.y, camFrameDepInterpFactor
				);
				camCollisionOriginPoint.z = Util::InterpolateSmootherStep
				(
					oldOriginPoint.z, camCollisionOriginPoint.z, camFrameDepInterpFactor
				);
			}

			camOriginPoint = camCollisionOriginPoint;
		}
		else
		{
			if (Settings::bOriginPointSmoothing)
			{
				camOriginPoint.x = camBaseOriginPoint.x = Util::InterpolateSmootherStep
				(
					oldOriginPoint.x, camBaseOriginPoint.x, camFrameDepInterpFactor
				);
				camOriginPoint.y = camBaseOriginPoint.y = Util::InterpolateSmootherStep
				(
					oldOriginPoint.y, camBaseOriginPoint.y, camFrameDepInterpFactor
				);
				camOriginPoint.z = camBaseOriginPoint.z = Util::InterpolateSmootherStep
				(
					oldOriginPoint.z, camBaseOriginPoint.z, camFrameDepInterpFactor
				);
			}
			else
			{
				camOriginPoint = camBaseOriginPoint;
			}
		}

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
			camBaseTargetPos = tpState->camera->cameraRoot->local.translate;
			if (camAdjMode == CamAdjustmentMode::kZoom && controlCamCID > -1 && controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
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
					targetPosMovementOffset.z += sinf(-camTargetPosPitch) * std::clamp(rsY, -1.0f, 1.0f);
					targetPosMovementOffset.Unitize();
					targetPosMovementOffset *= camManualPosMaxMovementSpeed * *g_deltaTimeRealTime * rsMag;

					camBaseTargetPos = lastSetCamTargetPos + targetPosMovementOffset;
					// Prevent camera from clipping into objects/surfaces by setting the new target position
					// to the movement direction raycast hit position, if any.
					auto newTargetPos = camBaseTargetPos;
					if (Settings::bTargetPosSmoothing) 
					{
						newTargetPos.x = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, newTargetPos.x, camFrameDepInterpFactor
						);
						newTargetPos.y = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y, newTargetPos.y, camFrameDepInterpFactor
						);
						newTargetPos.z = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z, newTargetPos.z, camFrameDepInterpFactor
						);
					}

					camCollisionTargetPos = camCollisionTargetPos2 = newTargetPos;
					Raycast::RayResult movementHitResult = Raycast::CastRay
					(
						ToVec4(lastSetCamTargetPos), ToVec4(newTargetPos), 10.0f
					);
					if (movementHitResult.hit)
					{
						camCollisionTargetPos = camCollisionTargetPos2 = ToNiPoint3
						(
							movementHitResult.hitPos + movementHitResult.rayNormal * 10.1f
						);
					}
				}

				if (Settings::bCamCollisions)
				{
					camTargetPos = camCollisionTargetPos;
				}
				else
				{
					if (Settings::bTargetPosSmoothing) 
					{
						camTargetPos.x = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, camBaseTargetPos.x, camFrameDepInterpFactor
						);
						camTargetPos.y = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y, camBaseTargetPos.y, camFrameDepInterpFactor
						);
						camTargetPos.z = Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z, camBaseTargetPos.z, camFrameDepInterpFactor
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
			float r = camTargetRadialDistance;
			float phi = Util::ConvertAngle(camTargetPosYaw);
			float theta = PI / 2.0f + camTargetPosPitch;
			if (focalPlayerCID == -1) 
			{
				// Base target position is offset from the base focus position,
				// and is not guaranteed to be in a reachable spot.
				camBaseTargetPos = camBaseFocusPoint;
				camBaseTargetPos.z -= r * cosf(theta);
				camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
				camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
			}
			else
			{
				// Base target position is the focal player's focus point,
				// which is guaranteed to be valid since it's based on the player's position.
				const auto& focalP = glob.coopPlayers[focalPlayerCID];
				camPlayerFocusPoint = Util::GetPlayerFocusPoint(focalP->coopActor.get());
				r = camBaseRadialDistance;
				camBaseTargetPos = camPlayerFocusPoint;
				camBaseTargetPos.z -= r * cosf(theta);
				camBaseTargetPos.x -= r * cosf(phi) * sinf(theta);
				camBaseTargetPos.y -= r * sinf(phi) * sinf(theta);
			}

			// [(Questionable?) Methods to the Madness Below]:
			// 
			// NOTE: 
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
			// when the focal player is visible and close enough to the rest of the party again.

			// NOTE 2: 
			// No matter what, players should try to stay close together as much as possible
			// for the smoothest experience when using the co-op camera.

			// Raycast result from the previous position to the current base position.
			// If there's a hit, this means that the camera will hit a surface 
			// if it moves directly to the base target position.
			Raycast::RayResult movementResult = Raycast::CastRay(ToVec4(lastSetCamTargetPos), ToVec4(camBaseTargetPos), camTargetPosHullSize);
			// Keep track of whether or not the camera was colliding last frame.
			bool wasColliding = isColliding;
			if (movementResult.hit)
			{
				// Upon movement hit, the camera is now colliding with geometry.
				isColliding = true;
			}
			

			// Reset focal player CID if the setting is now disabled.
			if (!Settings::bFocalPlayerMode && focalPlayerCID != -1) 
			{
				focalPlayerCID = -1;
			}

			// Set/reset the focal player CID if the setting is enabled.
			if (Settings::bFocalPlayerMode) 
			{
				// Check for a new focal player to set if a player is adjusting the camera.
				if (camAdjMode != CamAdjustmentMode::kNone && controlCamCID != -1)
				{
					if (controlCamCID != focalPlayerCID)
					{
						if (focalPlayerCID == -1)
						{
							const auto& p = glob.coopPlayers[controlCamCID];
							bool reqPlayerOnScreen = Util::PointIsOnScreen(Util::GetPlayerFocusPoint(p->coopActor.get()));
							bool tooCloseToFocus = lastSetCamTargetPos.GetDistance(camFocusPoint) < camMinTrailingDistance;
							// The requesting player must be off-screen
							// or the camera's last target position must be too close to the focus point.
							if (!reqPlayerOnScreen || tooCloseToFocus || !Settings::bCamCollisions)
							{
								if (tooCloseToFocus && !reqPlayerOnScreen) 
								{
									// Set directly without LOS check when too close to the focus point
									// and the requesting player is not on the screen.
									focalPlayerCID = controlCamCID;
								}
								else
								{
									RE::NiPoint3 losStartPoint{ camFocusPoint };
									RE::NiPoint3 losEndPoint{ Util::GetPlayerFocusPoint(p->coopActor.get()) };
									// If neither all other players nor the focus point
									// have LOS on the player rotating the camera,
									// set the camera-rotating player as the focal player.

									// Focus point first.
									bool losOnFocalPlayer = false;	//!Raycast::hkpCastRay(ToVec4(losStartPoint), ToVec4(losEndPoint), true).hit;
									// No LOS from the focus, so check for LOS from all other players.
									if (!losOnFocalPlayer)
									{
										for (const auto& otherP : glob.coopPlayers)
										{
											if (otherP->isActive && otherP != p)
											{
												losStartPoint = Util::GetPlayerFocusPoint(otherP->coopActor.get());
												if (!Raycast::hkpCastRay(ToVec4(losStartPoint), ToVec4(losEndPoint), true).hit)
												{
													losOnFocalPlayer = true;
													break;
												}
											}
										}
									}

									// No LOS at all.
									if (!losOnFocalPlayer)
									{
										focalPlayerCID = controlCamCID;
									}
								}							}
						}
						else
						{
							// If there is already a focal player and another player adjusts the camera,
							// set the other player as the new focal player instantly.
							focalPlayerCID = controlCamCID;
						}
					}
				}
				else if (focalPlayerCID != -1)
				{
					const auto& focalP = glob.coopPlayers[controlCamCID];
					// Within a reasonable range of not needing focal player-centric rotation anymore.
					if (focalP->coopActor->data.location.GetDistance(camFocusPoint) < 500.0f)
					{
						RE::NiPoint3 losStartPoint{};
						RE::NiPoint3 losEndPoint{ Util::GetPlayerFocusPoint(focalP->coopActor.get()) };

						// First, check if the focal player is on-screen
						// and if any other player or the focus point has LOS on the focal player.
						// If on screen and there is LOS, clear the focal player CID.
						bool focalPlayerOnScreen = 
						(
							!Settings::bCamCollisions || 
							Util::PointIsOnScreen(Util::GetPlayerFocusPoint(glob.coopPlayers[focalPlayerCID]->coopActor.get()))
						);
						if (focalPlayerOnScreen)
						{
							// Focus point first.
							bool losOnFocalPlayer = false; //!Raycast::hkpCastRay(ToVec4(camFocusPoint), ToVec4(losEndPoint), true).hit;
							if (!losOnFocalPlayer)
							{
								for (const auto& otherP : glob.coopPlayers)
								{
									if (otherP->isActive && otherP != focalP)
									{
										losStartPoint = Util::GetPlayerFocusPoint(otherP->coopActor.get());
										if (!Raycast::hkpCastRay(ToVec4(losStartPoint), ToVec4(losEndPoint), true).hit)
										{
											losOnFocalPlayer = true;
											break;
										}
									}
								}
							}

							// LOS from other player or focus point,
							// so clear the focal player.
							if (losOnFocalPlayer)
							{
								focalPlayerCID = -1;
							}
						}
					}
				}
			}
			
			// Basic system to minimize camera jumping and maximize visibility of all players:
			// 1. Check for visibility of the base target position from the focus point 
			// and all active players' focus points. Use two raycasts per focus point to check visibility.
			// 2. If the hull result does not hit or if the hit position is close to the start position
			// and the zero-hull raycast does not hit, the base target position is valid and visible.
			// Reason for the distance check from the hull cast hit position is to prevent the target position
			// from jumping forward to the focus point unless there is an obstruction to the base target position. 
			// Example: All active players are within a hull size from a wall, which causes the hull casts starting
			// from their focus points to hit the wall right away. However, the zero-hull cast will not hit the wall,
			// unless all players have their focus points clipping through the wall, which shouldn't happen.
			// The next target position is set to the base target position, instead of one of the wall-hit positions,
			// since the base target position is still reachable from the players' focus points.
			// 3. Otherwise, for the next target position, choose the hull cast hit point that is closest to the previous target position,
			// which will minimize camera jumping. The hull cast hit position is adjusted to avoid clipping into geometry
			// and is always at a valid, reachable position.
			// 
			// Min 2 raycasts (2 from camera focus point).
			// Max 6-10 raycasts (2 from camera focus point + 2 per active player).

			const glm::vec4 baseTargetPos = ToVec4(camBaseTargetPos);
			const glm::vec4 lastSetTargetPos = ToVec4(lastSetCamTargetPos);
			glm::vec4 closestHitPosToPrevTargetPos = lastSetTargetPos;
			glm::vec4 castStartPos = ToVec4(camFocusPoint);
			glm::vec4 adjHitResultPos{};
			float closestDistToPrevTargetPos = FLT_MAX;
			Raycast::RayResult hitResultWithHull{};
			Raycast::RayResult hitResultNoHull{};

			hitResultWithHull = Raycast::CastRay(castStartPos, baseTargetPos, camTargetPosHullSize);
			hitResultNoHull = Raycast::CastRay(castStartPos, baseTargetPos, 0.0f);
			bool baseTargetPosVisible = 
			{
				(!hitResultWithHull.hit) || 
				(glm::distance(hitResultWithHull.hitPos, castStartPos) <= camTargetPosHullSize && !hitResultNoHull.hit)
			};
			if (baseTargetPosVisible)
			{
				closestHitPosToPrevTargetPos = baseTargetPos;
			}
			else
			{
				adjHitResultPos =
				(
					hitResultWithHull.hitPos +
					hitResultWithHull.rayNormal *
					min(hitResultWithHull.rayLength, camTargetPosHullSize)
				);

				if (float dist = glm::distance(adjHitResultPos, lastSetTargetPos); dist < closestDistToPrevTargetPos)
				{
					closestDistToPrevTargetPos = dist;
					closestHitPosToPrevTargetPos = adjHitResultPos;
				}

				for (const auto& p : glob.coopPlayers)
				{
					if (p->isActive)
					{
						castStartPos = ToVec4(Util::GetPlayerFocusPoint(p->coopActor.get()));
						hitResultWithHull = Raycast::CastRay(castStartPos, baseTargetPos, camTargetPosHullSize);
						hitResultNoHull = Raycast::CastRay(castStartPos, baseTargetPos, 0.0f);
						baseTargetPosVisible = 
						{
							(!hitResultWithHull.hit) || 
							(glm::distance(hitResultWithHull.hitPos, castStartPos) <= camTargetPosHullSize && !hitResultNoHull.hit)
						};
						if (baseTargetPosVisible)
						{
							closestHitPosToPrevTargetPos = baseTargetPos;
							break;
						}
						else
						{
							adjHitResultPos =
							(
								hitResultWithHull.hitPos +
								hitResultWithHull.rayNormal *
								min(hitResultWithHull.rayLength, camTargetPosHullSize)
							);

							if (float dist = glm::distance(adjHitResultPos, lastSetTargetPos); dist < closestDistToPrevTargetPos)
							{
								closestDistToPrevTargetPos = dist;
								closestHitPosToPrevTargetPos = adjHitResultPos;
							}
						}
					}
				}
			}
			
			// Set next collision target position and set colliding flag.
			camCollisionTargetPos = ToNiPoint3(closestHitPosToPrevTargetPos);
			// Not colliding if the collision target position is the same as the base target position
			// and there is no movement hit.
			if (camCollisionTargetPos == camBaseTargetPos && !movementResult.hit) 
			{
				isColliding = false;
			}
			else
			{
				isColliding = true;
			}

			// Collision target position 2 is used for crosshair selection LOS checks
			// when camera collisions are disabled.
			// Check for a hit from the base focus position.
			if (!Settings::bCamCollisions && camCollisionTargetPos != camBaseTargetPos)
			{
				if (focalPlayerCID == -1) 
				{
					auto result = Raycast::CastRay(ToVec4(camBaseFocusPoint), ToVec4(camBaseTargetPos), camTargetPosHullSize);
					if (result.hit)
					{
						camCollisionTargetPos2 = ToNiPoint3(result.hitPos + result.rayNormal * camTargetPosHullSize);
					}
					else
					{
						camCollisionTargetPos2 = camBaseTargetPos;
					}
				}
				else
				{
					camCollisionTargetPos2 = camCollisionTargetPos;
				}
			}

			if (Settings::bCamCollisions || lockInteriorOrientationOnInit) 
			{
				RE::NiPoint3 losCheckPos = camCollisionTargetPos;
				// Apply smoothing if enabled.
				// NOTE: Camera can still phase through surfaces when transitioning from the last set position (can be OOB) to the target position (not OOB)
				// since the interpolated position is between the two positions. Only jumping instantly to the target position will prevent this from occurring.
				if (Settings::bTargetPosSmoothing)
				{
					camCollisionTargetPos = 
					{
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.x, camCollisionTargetPos.x, camFrameDepInterpFactor
						),
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.y, camCollisionTargetPos.y, camFrameDepInterpFactor
						),
						Util::InterpolateSmootherStep
						(
							lastSetCamTargetPos.z, camCollisionTargetPos.z, camFrameDepInterpFactor
						)
					};
				}

				camTargetPos = camCollisionTargetPos;
			}
			else
			{
				if (Settings::bTargetPosSmoothing)
				{
					camTargetPos.x = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.x, camBaseTargetPos.x, camFrameDepInterpFactor
					);
					camTargetPos.y = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.y, camBaseTargetPos.y, camFrameDepInterpFactor
					);
					camTargetPos.z = Util::InterpolateSmootherStep
					(
						lastSetCamTargetPos.z, camBaseTargetPos.z, camFrameDepInterpFactor
					);
				}
				else
				{
					camTargetPos = camBaseTargetPos;
				}
			}

			camCollisionRadialDistance = camTargetPos.GetDistance(camBaseFocusPoint);
		}
	}

	void CameraManager::CheckLockOnTarget()
	{
		// Set a new lock on target if there is a valid request,
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
				// Change lock on data to reflect this.
				secsSinceLockOnTargetLOSChecked = secsSinceLockOnTargetLOSLost = 0.0f;
				lockOnLOSCheckTP = SteadyClock::now();
				lockOnTargetInSight = true;
			}

			// Indicate request was handled.
			lockOnActorReq = std::nullopt;
		}

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		bool validLockOnTarget = camLockOnTargetPtr && camLockOnTargetPtr.get();
		if (isLockedOn)
		{
			// Check if target is still valid (in LOS, 3D loaded, handle valid, etc.)
			RE::NiPoint3 oldCamLockOnFocusPoint = camLockOnFocusPoint;
			if (validLockOnTarget)
			{
				// Check if current target is valid.
				// LOS checks.
				secsSinceLockOnTargetLOSChecked = Util::GetElapsedSeconds(lockOnLOSCheckTP);
				if (secsSinceLockOnTargetLOSChecked > Settings::fSecsBetweenTargetVisibilityChecks)
				{
					lockOnLOSCheckTP = SteadyClock::now();

					bool hadLOS = lockOnTargetInSight;
					bool falseRef = false;
					auto p1 = RE::PlayerCharacter::GetSingleton();
					lockOnTargetInSight = p1 && p1->HasLineOfSight(camLockOnTargetPtr.get(), falseRef);
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

				bool invalidateAfterNoLOS = secsSinceLockOnTargetLOSLost > Settings::fSecsWithoutLOSToInvalidateTarget;
				// Clear invalid target.
				if (invalidateAfterNoLOS || !camLockOnTargetPtr->Is3DLoaded() || !camLockOnTargetPtr->IsHandleValid() || 
					!camLockOnTargetPtr->GetParentCell() || !camLockOnTargetPtr->GetParentCell()->IsAttached())
				{
					// Reset LOS lost interval.
					if (invalidateAfterNoLOS)
					{
						secsSinceLockOnTargetLOSLost = 0.0f;
					}
				}
				else
				{
					// Crosshair refr is valid.
					validLockOnTarget = true;
					// Update lock on pos.
					camLockOnFocusPoint = Util::GetHeadPosition(camLockOnTargetPtr.get());
				}
			}

			if (validLockOnTarget)
			{
				// Draw lock on indicator above the target's head.
				auto lockOnIndicatorCenter = DebugAPI::WorldToScreenPoint
				(
					{ 
						camLockOnFocusPoint.x, 
						camLockOnFocusPoint.y, 
						camLockOnFocusPoint.z + 0.2f * camLockOnTargetPtr->GetHeight() 
					}
				);
				DrawLockOnIndicator(lockOnIndicatorCenter.x, lockOnIndicatorCenter.y);
				// Smooth out tracking of lock on target.
				camLockOnFocusPoint.x = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.x, camLockOnFocusPoint.x, camFrameDepInterpFactor
				);
				camLockOnFocusPoint.y = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.y, camLockOnFocusPoint.y, camFrameDepInterpFactor
				);
				camLockOnFocusPoint.z = Util::InterpolateSmootherStep
				(
					oldCamLockOnFocusPoint.z, camLockOnFocusPoint.z, camFrameDepInterpFactor
				);
			}
		}

		// Clear lock on data if the lock on target is invalid or
		// not in lock on mode but lock on target is still set.
		if ((!isLockedOn && camLockOnTargetHandle) || (isLockedOn && !validLockOnTarget && camLockOnTargetHandle))
		{
			ClearLockOnData();
		}
	}

	void CameraManager::DrawLockOnIndicator(const float& a_centerX, const float& a_centerY)
	{
		// Draw the lock on marker on the camera's lock on target.

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		if (!camLockOnTargetPtr || !camLockOnTargetPtr.get()) 
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
			min(max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), targetPixelHeight / 4.0f),
			max(max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), targetPixelHeight / 4.0f)
		);
		const float indicatorGap = max(1.0f, indicatorBaseLength / 4.0f);
		auto upperPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS;
		auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;
		float baseScalingFactor = indicatorBaseLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
		float indicatorLength = indicatorBaseLength * baseScalingFactor;
		float indicatorThickness = indicatorBaseThickness * baseScalingFactor;

		// Oscillation interp data: dynamically inc/dec gap size.
		float endPointGapDelta = lockOnIndicatorOscillationInterpData->next;
		if (lockOnIndicatorOscillationInterpData->current == endPointGapDelta)
		{
			// Switch gap delta endpoint when reached.
			endPointGapDelta = endPointGapDelta == 0.0f ? indicatorLength : 0.0f;
		}

		if (lockOnIndicatorOscillationInterpData->next != endPointGapDelta)
		{
			lockOnIndicatorOscillationInterpData->SetTimeSinceUpdate(0.0f);
			lockOnIndicatorOscillationInterpData->ShiftEndpoints(endPointGapDelta);
		}

		lockOnIndicatorOscillationInterpData->next = endPointGapDelta;
		if (lockOnIndicatorOscillationInterpData->current != endPointGapDelta)
		{
			lockOnIndicatorOscillationInterpData->InterpolateSmootherStep
			(
				min
				(
					lockOnIndicatorOscillationInterpData->secsSinceUpdate / lockOnIndicatorOscillationInterpData->secsUpdateInterval, 
					1.0f
				)
			);
			lockOnIndicatorOscillationInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
			if (lockOnIndicatorOscillationInterpData->current == endPointGapDelta)
			{
				lockOnIndicatorOscillationInterpData->SetUpdateDurationAsComplete();
				lockOnIndicatorOscillationInterpData->SetData(endPointGapDelta, endPointGapDelta, endPointGapDelta);
			}
		}

		// Points are offset downward from origin (+Y Scaleform axis).
		// Have to rebase from the bottom tip by subtracting length for each segment.
		float gapDelta = lockOnIndicatorOscillationInterpData->current;
		for (auto& offset : upperPortionOffsets)
		{
			offset.y -= GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
			offset *= baseScalingFactor;
			offset.y -= gapDelta + indicatorGap;
		}

		for (auto& offset : lowerPortionOffsets)
		{
			offset.y -= GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
			offset *= baseScalingFactor;
			offset.y -= gapDelta + indicatorGap;
		}

		const auto port = Util::GetPort();
		glm::vec2 origin
		{
			std::clamp
			(
				a_centerX, 
				port.left + (gapDelta + indicatorGap + 2.0f * indicatorThickness + indicatorLength), 
				port.right - (gapDelta + indicatorGap + 2.0f * indicatorThickness + indicatorLength)
			),
			std::clamp
			(
				a_centerY, 
				port.top + (gapDelta + indicatorGap + 2.0f * indicatorThickness + indicatorLength), 
				port.bottom - (gapDelta + indicatorGap + 2.0f * indicatorThickness + indicatorLength)
			)
		};

		float startingRotation = -PI / 4.0f;
		DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, startingRotation);
		DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, startingRotation);
		DebugAPI::QueueShape2D(origin, lowerPortionOffsets, 0x000000FF, false, indicatorBaseThickness, 0.0f);
		DebugAPI::QueueShape2D(origin, lowerPortionOffsets, 0xFFFFFFFF);
		DebugAPI::QueueShape2D(origin, upperPortionOffsets, 0x000000FF, false, indicatorBaseThickness, 0.0f);
		DebugAPI::QueueShape2D(origin, upperPortionOffsets, 0xFFFFFFFF);
		for (uint8_t i = 0; i < 2; ++i)
		{
			DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, PI / 4.0f);
			DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, PI / 4.0f);
			DebugAPI::QueueShape2D(origin, lowerPortionOffsets, 0x000000FF, false, indicatorBaseThickness, 0.0f);
			DebugAPI::QueueShape2D(origin, lowerPortionOffsets, 0xFFFFFFFF);
			DebugAPI::QueueShape2D(origin, upperPortionOffsets, 0x000000FF, false, indicatorBaseThickness, 0.0f);
			DebugAPI::QueueShape2D(origin, upperPortionOffsets, 0xFFFFFFFF);
		}
	}

	void CameraManager::FadeObstructions()
	{
		// Fade or unfade objects that obstruct the LOS from the camera to each player.

		RE::NiPointer<RE::NiCamera> niCam = Util::GetNiCamera();
		if (niCam)
		{
			std::unordered_map<RE::NiPointer<RE::NiAVObject>, std::pair<int32_t, int32_t>> obstructions;
			for (const auto& p : glob.coopPlayers)
			{
				if (p->isActive)
				{
					auto actorCenter = Util::GetTorsoPosition(p->coopActor.get());
					auto camForwardOffset = Util::RotationToDirectionVect(-camPitch, Util::ConvertAngle(camYaw)) * camTargetPosHullSize;
					auto camNodePos = camTargetPos + camForwardOffset;
					auto results = Raycast::GetAllHavokCastHitResults(ToVec4(actorCenter), ToVec4(camNodePos));
					for (uint32_t i = 0; i < results.size(); ++i)
					{
						const auto& result = results[i];
						if (result.hitObject && result.hitObject.get() && !result.hitObject->flags.all(RE::NiAVObject::Flag::kHidden))
						{
							if (!Settings::bProximityFadeOnly || camTargetPos.GetDistance(ToNiPoint3(result.hitPos)) < camTargetRadialDistance)
							{
								auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
								auto asActor = hitRefrPtr ? hitRefrPtr->As<RE::Actor>() : nullptr;
								auto asActivator = hitRefrPtr ? hitRefrPtr->As<RE::TESObjectACTI>() : nullptr;
								
								if (hitRefrPtr && hitRefrPtr->GetBaseObject() && hitRefrPtr->GetBaseObject()->Is(RE::FormType::Static)) 
								{
									auto asStatic = hitRefrPtr->GetBaseObject()->As<RE::TESObjectSTAT>();
									if ((asStatic->formFlags & RE::TESObjectSTAT::RecordFlags::kNeverFades) != 0) 
									{
										asStatic->formFlags ^= RE::TESObjectSTAT::RecordFlags::kNeverFades;
									}
								}

								if (hitRefrPtr && hitRefrPtr->GetBaseObject() && hitRefrPtr->GetBaseObject()->Is(RE::FormType::Light))
								{
									if ((hitRefrPtr->formFlags & RE::TESObjectREFR::RecordFlags::kNeverFades) != 0)
									{
										hitRefrPtr->formFlags ^= RE::TESObjectREFR::RecordFlags::kNeverFades;
									}
								}

								if (!asActor)
								{
									auto object3D = hitRefrPtr ? Util::GetRefr3D(hitRefrPtr.get()) : result.hitObject;
									if (object3D && object3D.get() && object3D->GetRefCount() > 0)
									{
										RE::NiPointer<RE::NiAVObject> ptr{ object3D };
										// Can only fade smaller objects if larger object fade setting is not enabled.
										// If not a faded object, add directly with raycast hit index as fade index.
										bool canFadeObj = Settings::bFadeLargerObstructions || !object3D->AsFadeNode();
										if ((canFadeObj) && (obstructions.empty() || !obstructions.contains(ptr)))
										{
											obstructions.insert({ ptr, { i, p->controllerID } });
										}
									}
								}
							}
						}
					}
				}
			}

			// NiAVObjects in obstructions list should not be invalid since they've been IncRef'd
			// when constructed as NiPointers.
			// And the naked NiAVObject ptrs in the handled set are kept valid while inserted
			// into the fade data list, which also wraps them in NiPointers.
			for (const auto& [object3D, fadeIndexCIDPair] : obstructions) 
			{
				if (object3D && object3D.get() && object3D->GetRefCount() > 0)
				{
					if (obstructionsToFadeIndicesMap.empty() || !obstructionsToFadeIndicesMap.contains(object3D))
					{
						obstructionFadeDataSet.insert
						(
							std::make_unique<ObjectFadeData>(object3D.get(), fadeIndexCIDPair.first, true)
						);
						obstructionsToFadeIndicesMap.insert({ object3D, fadeIndexCIDPair.first });
					}
					else if (obstructionsToFadeIndicesMap.at(object3D) < fadeIndexCIDPair.first)
					{
						// Update fade index.
						obstructionsToFadeIndicesMap[object3D] = fadeIndexCIDPair.first;
					}
				}
			}

			for (const auto& fadeData : obstructionFadeDataSet) 
			{
				const auto& handled3D = fadeData->object;
				if (handled3D && handled3D.get() && handled3D->GetRefCount() > 0)
				{
					// Check if the object is not in the current obstructions set, and fade it back in if it isn't.
					bool shouldFadeIn = 
					{
						(fadeData->shouldFadeOut) && 
						((obstructions.empty()) || (!obstructions.empty() && !obstructions.contains(handled3D)))
					};

					if (shouldFadeIn)
					{
						fadeData->SignalFadeStateChange(false, fadeData->fadeIndex);
					}

					if (int32_t updatedFadeIndex = obstructionsToFadeIndicesMap[handled3D]; updatedFadeIndex > fadeData->fadeIndex)
					{
						fadeData->SignalFadeStateChange(fadeData->shouldFadeOut, updatedFadeIndex);
					}

					bool shouldRemove = !fadeData->UpdateFade();
					if (shouldRemove)
					{
						obstructionsToFadeIndicesMap.erase(handled3D);
						obstructionFadeDataSet.erase(fadeData);
					}
				}
				else
				{
					obstructionFadeDataSet.erase(fadeData);
				}
			}
		}
	}

	float CameraManager::GetAverageMovementPitch()
	{
		// Get the average movement pitch delta to add to the base camera pitch.
		// Attempts to improve visibility when the party moves up or down slopes.

		float avgMovPitch = 0.0f;
		uint32_t consideredPlayersCount = 0;
		for (const auto& p : glob.coopPlayers)
		{
			if (focalPlayerCID != -1 && p->controllerID != focalPlayerCID)
			{
				continue;
			}

			if (p->isActive)
			{
				++consideredPlayersCount;

				// To avoid affecting player aim, only rotate 
				// in the direction of the party's movement when not in combat.	
				// No need to continue -- return 0 here to reset aim pitch 
				// over the next interpolation interval.
				if (p->coopActor->IsInCombat()) 
				{
					return 0.0f;
				}

				RE::ActorPtr mount{ nullptr };
				p->coopActor->GetMount(mount);
				RE::Actor* actor = mount && mount.get() ? mount.get() : p->coopActor.get();
				if (auto charController = actor->GetCharController(); charController)
				{
					const auto& lsData = glob.cdh->GetAnalogStickState(p->controllerID, true);
					// Ensure that the player is moving fully in any direction 
					// and not attacking/bashing before adding movement pitch delta.
					auto occupiedFurnitureHandle = actor->GetOccupiedFurniture();
					bool notUsingFurniture = 
					{
						!occupiedFurnitureHandle || !occupiedFurnitureHandle.get() || !occupiedFurnitureHandle.get().get()
					};
					if (notUsingFurniture)
					{
						auto velocity = Util::GetActorLinearVelocity(actor);
						auto& currentState = charController->context.currentState;

						// Velocity-based incline angle when in the air/flying/jumping.
						if (currentState == RE::hkpCharacterStateType::kFlying ||
							currentState == RE::hkpCharacterStateType::kInAir ||
							currentState == RE::hkpCharacterStateType::kJumping)
						{
							auto speed = velocity.Length();
							auto velPitch = speed > 0.0f ? asinf(velocity.z / speed) : 0.0f;
							avgMovPitch += velPitch / 2.0f;
						}
						// Surface support-based incline angle when on the ground/climbing/swimming.
						else
						{
							auto normalZComp = charController->surfaceInfo.surfaceNormal.quad.m128_f32[2];
							// Angle the camera downward if the player is moving downhill (no change to sign).
							auto supportSurfaceIncline = fabsf(asinf(normalZComp) - PI / 2.0f);

							// Supporting surface's normal must be pointing up.
							// Flat or down indicates that the player is walking on a surface 
							// that is parallel to their upright direction or above them, 
							// and that's not possible, I think.
							// Report an incline of 0 in that case.
							if (charController->surfaceInfo.surfaceNormal.quad.m128_f32[2] > 0.0f) 
							{
								RE::NiPoint3 normal = ToNiPoint3(charController->surfaceInfo.surfaceNormal, true);
								RE::NiPoint3 camRight = Util::RotationToDirectionVect
								(
									0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(camYaw + PI / 2.0f))
								);
								RE::NiPoint3 camForwardXY = Util::RotationToDirectionVect
								(
									0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(camYaw))
								);
								float angNormalToForwardXY = acosf(std::clamp(normal.Dot(camForwardXY), -1.0f, 1.0f));
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
							// If the player's z velocity is insignificantly small, set the incline to 0.
							else if (velocity.z == 0.0f)
							{
								supportSurfaceIncline = 0.0f;
							}

							avgMovPitch += supportSurfaceIncline;
						}
					}
				}
			}
		}

		// Four elevation change scenarios relative to the camera:
		// 1. up a slope away from camera: pitch cam upward.
		// 2. down a slope away from camera: pitch cam downward.
		// 3. up a slope towards the camera: pitch cam downward.
		// 4. down a slope towards the camera: pitch cam upward.
		avgMovPitch /= max(1, consideredPlayersCount);
		float signAdjustment = 1.0f;
		if (camOriginPointDirection.Length() > 0.0f) 
		{
			float originMovYaw = Util::DirectionToGameAngYaw(camOriginPointDirection); 
			float moveAngRelToFacing = Util::NormalizeAngToPi(originMovYaw - camYaw);
			signAdjustment = cosf(moveAngRelToFacing);
		}

		avgMovPitch *= signAdjustment;
		return avgMovPitch;
	}

	float CameraManager::GetAverageMovementYawToCam()
	{
		// Get the average movement yaw relative to the camera's yaw
		// to add onto the base camera yaw.
		// Attempt to turn the camera automatically to face the direction
		// the party is moving in.

		float yawDiff = 0.0f;
		float avgMovementYawRelToCam = 0.0f;
		bool notUsingFurniture = true;
		RE::ObjectRefHandle occupiedFurnitureHandle;
		
		uint32_t consideredPlayersCount = 0;
		for (const auto& p : glob.coopPlayers)
		{
			if (focalPlayerCID != -1 && p->controllerID != focalPlayerCID)
			{
				continue;
			}

			if (p->isActive)
			{
				++consideredPlayersCount;

				// To avoid affecting player aim, only rotate 
				// in the direction of the party's movement when not in combat.
				// No need to continue -- return 0 here to reset aim yaw 
				// over the next interpolation interval.
				if (p->coopActor->IsInCombat())
				{
					return 0.0f;
				}

				RE::ActorPtr mount{ nullptr };
				const auto& movementActor = p->mm->movementActor;
				bool isMounted = movementActor && movementActor.get() && movementActor->IsAMount();
				auto charController = movementActor && movementActor.get() ? movementActor->GetCharController() : nullptr;
				// Ensure that the player is moving fully in any direction 
				// and not attacking/bashing/using furniture before adding yaw delta.
				bool addYawDiff = 
				{ 
					(charController) && 
					(
						(
							charController->speedPct != 0.0f &&
							!p->pam->isAttacking && 
							!p->pam->isBashing
						) && 
						(
							movementActor->actorState1.movingBack ||
							movementActor->actorState1.movingForward ||
							movementActor->actorState1.movingLeft ||
							movementActor->actorState1.movingRight
						)
					) &&
					(isMounted || notUsingFurniture)
				};

				if (addYawDiff) 
				{
					yawDiff = Util::NormalizeAngToPi(Util::NormalizeAng0To2Pi(movementActor->data.angle.z) - camYaw);
					float sign = yawDiff < 0.0f ? -1.0f : 1.0f;
					yawDiff = fabsf(yawDiff) > PI / 2.0f ? sign * PI - yawDiff : yawDiff;
					const auto& lsData = glob.cdh->GetAnalogStickState(p->controllerID, true);
					// Dependent on how committed the player is to moving 
					// in their heading direction.
					yawDiff *= lsData.normMag;
					// Ease into the full movement yaw delta to prevent sharp yaw changes.
					yawDiff *= Util::InterpolateSmootherStep
					(
						0.0f,
						1.0f, 
						std::clamp
						(
							Util::GetElapsedSeconds(p->lastMovementStartReqTP) / 3.0f, 
							0.0f, 
							1.0f
						)
					);
					avgMovementYawRelToCam += yawDiff;
				}
			}
		}

		avgMovementYawRelToCam /= max(1, consideredPlayersCount);
		return avgMovementYawRelToCam;
	}

	bool CameraManager::GetPlayerHasLOSOnBaseTargetPos(RE::NiPoint3& a_playerClosestLOSPos)
	{
		// Perform raycasts along each players' vertical axis towards the base target position.
		// If there is no obstruction to the base target position 
		// (a ray does not hit for any player), the player has 'LOS', so return true.
		// Also store the closest player 'LOS' position in the outparam.

		RE::NiPoint3 playerFocusPoint = camCollisionFocusPoint;
		float closestHitDist = FLT_MAX;
		std::optional<glm::vec4> closestHitStartPos{ };
		std::optional<glm::vec4> closestHitEndPos{ };
		glm::vec4 startPos{ };
		glm::vec4 endPos{ };
		Raycast::RayResult result;
		for (const auto& p : glob.coopPlayers)
		{
			if (!p->isActive)
			{
				continue;
			}

			playerFocusPoint = p->coopActor->data.location;
			playerFocusPoint.z += p->coopActor->GetHeight();
			auto vertBounds = Util::GetVertCollPoints(playerFocusPoint, 0.0f);
			float startingZPos = vertBounds.second == -FLT_MAX ? p->coopActor->data.location.z : vertBounds.second;
			ClampToZCoordAboveLowerBound
			(
				startingZPos, 
				camTargetPosHullSize * 2.0f, 
				camTargetPosHullSize * 2.0f, 
				vertBounds.first, 
				vertBounds.second
			);
			const float maxOffset = Settings::fMaxRaycastAndZoomOutDistance;
			float endingZPos = 
			{
				vertBounds.first == FLT_MAX ?
				vertBounds.second + maxOffset :
				max(startingZPos, vertBounds.first - camTargetPosHullSize * 2.0f)
			};

			// Three casts along the player (feet level, torso level, head level),
			// Two casts above the player and below the upper bound (halfway between, at upper bound).
			const float zIncAlongPlayer = (playerFocusPoint.z - startingZPos) / 2.0f;
			const float zIncAbovePlayer = max(0.0f, min(endingZPos - playerFocusPoint.z, maxOffset)) / 2.0f;
			startPos = glm::vec4(playerFocusPoint.x, playerFocusPoint.y, startingZPos, 0.0f);
			endPos = ToVec4(camBaseTargetPos);
			result.hit = false;
			uint8_t castIndex = 0;
			const uint32_t numCastsAlongVertAxis = 5;
			const uint32_t numCastsAlongPlayer = 3;
			while (castIndex < numCastsAlongVertAxis)
			{
				DebugAPI::QueuePoint3D(startPos, Settings::vuOverlayRGBAValues[p->playerID], 3.0f, 0.0f);

				result = Raycast::CastRay(startPos, endPos, 0.0f);
				// Went through base target position, so the position is visible from the player's vertical axis.
				if (!result.hit)
				{
					a_playerClosestLOSPos = camBaseTargetPos;
					return true;
				}
				else if (float hitDist = glm::distance(result.hitPos, endPos); hitDist < closestHitDist)
				{
					// If this raycast hit closer to the base target position,
					// set the new closest LOS position to the adjusted hit position.
					closestHitDist = hitDist;
					closestHitStartPos = startPos;
					closestHitEndPos = endPos;
				}

				// Three casts along the player and two above to the upper bound.
				if (castIndex < numCastsAlongPlayer)
				{
					startPos.z += zIncAlongPlayer;
				}
				else
				{
					startPos.z += zIncAbovePlayer;
				}

				++castIndex;
			}
		}

		if (closestHitStartPos.has_value() && closestHitEndPos.has_value()) 
		{
			result = Raycast::CastRay
			(
				closestHitStartPos.value(), closestHitEndPos.value(), 2.0f * camTargetPosHullSize
			);
			if (result.hit) 
			{
				a_playerClosestLOSPos = ToNiPoint3
				(
					result.hitPos + result.rayNormal * min(result.rayLength, camTargetPosHullSize)
				);
			}
		}

		return false;
	}

	RE::NiPoint3 CameraManager::GetRaycastApproxTargetPosFromOrigin(const bool& a_baseTargetPosIsOOB, const RE::NiPoint3& a_playerClosestLOSPos)
	{
		// OOB = 'Out of Bounds'
		// Perform casts along a vertical ray originating at the current true origin point.
		// Break up this ray into subranges based on all the raycast hits along the ray and
		// perform a set number of evenly-spaced casts in each range to the base target position.
		// If the base target position is OOB or no player has LOS on the base target position, 
		// choose the closest cast hit point that is not OOB 
		// but never the base target position itself.
		// Otherwise, choose the base target position itself as the next target position to set,
		// since it is guaranteed to be in-bounds and at least one player has LOS.

		const size_t maxEndpoints = 11;
		const float maxZOffset = Settings::fMaxRaycastAndZoomOutDistance;
		// Get lower bound Z pos offset slightly more than cam target pos hull size to prevent the first
		// raycast from hitting the starting point.
		auto bounds = Util::GetVertCollPoints(camOriginPoint, 0.0f);
		// If unbound below, use cam collision origin point's Z coord, which should be valid.
		// Give some leeway for subsequent casts along the origin vertical axis 
		// by increasing the z offset from the lower bound.
		float startZ = 
		(
			bounds.second != -FLT_MAX ? 
			bounds.second + camTargetPosHullSize * 2.1f : 
			camOriginPoint.z
		);
		float endZ = startZ + maxZOffset;

		glm::vec4 startPos = { camOriginPoint.x, camOriginPoint.y, startZ, 0.0f };
		glm::vec4 endPos = { camOriginPoint.x, camOriginPoint.y, endZ, 0.0f };
		// Giving leeway for casts to the target position by doubling the hull size.
		auto fromOriginPointResults = Raycast::GetAllCamCastHitResults
		(
			startPos, endPos, camTargetPosHullSize * 2.0f
		);
		// First range endpoint at the start pos.
		std::vector<glm::vec4> raycastRangeEndpoints;
		raycastRangeEndpoints.emplace_back(startPos);
		if (!fromOriginPointResults.empty())
		{
			size_t resultNum = 0;
			// Minus 2 (exterior cell, start + end range enpoints) or 1 (interior cell, only start range endpoint).
			size_t maxNumResults = min
			(
				fromOriginPointResults.size(), 
				exteriorCell ? 
				maxEndpoints - 2 : 
				maxEndpoints - 1
			);
			while (resultNum < maxNumResults)
			{
				auto result = fromOriginPointResults[resultNum];
				if (auto result = fromOriginPointResults[resultNum]; result.hit)
				{
					// Adjust pos to maintain a distance slightly larger than the target pos hull size.
					// Below and above the hit point.
					raycastRangeEndpoints.emplace_back
					(
						result.hitPos + result.rayNormal * min(result.rayLength, camTargetPosHullSize * 2.0f)
					);
				}

				++resultNum;
			}
		}

		// Since the camera is free to move as high into the sky as possible when outside and is not out of bounds, 
		// there will always be at least 2 end points.
		// When inside, the camera is constrained to the ceiling to stay in bounds.
		if (exteriorCell) 
		{
			raycastRangeEndpoints.emplace_back(endPos);
		}

		const size_t numRanges = max(raycastRangeEndpoints.size() - 1, 1);
		const size_t castsPerRange = std::clamp(maxEndpoints / numRanges, 1ull, 10ull);

		Raycast::RayResult result;
		result.hit = false;
		glm::vec4 adjustedHitPos{};
		glm::vec4 castStartPos{};
		glm::vec4 deltaPos{};
		glm::vec4 closestHitPos = ToVec4(camCollisionTargetPos);
		const glm::vec4 baseTargetPos = ToVec4(camBaseTargetPos);
		const glm::vec4 focusPoint = ToVec4(camBaseFocusPoint);
		const glm::vec4 toFocusDir = glm::normalize(focusPoint - baseTargetPos);
		const glm::vec4 playerClosestLOSPos = ToVec4(a_playerClosestLOSPos);
		// Closest Z coordinate difference between a raycast hit position below
		// and the closest player LOS position to the base target position.
		float closestHitZDist = FLT_MAX;
		size_t totalCastCount = 0;
		bool hitBaseTargetPos = false;
		bool playerHasLOS = a_playerClosestLOSPos == camBaseTargetPos;
		// Vertical offset ranges within which to raycast.
		for (uint32_t endPointNum = 0; endPointNum < raycastRangeEndpoints.size() - 1; ++endPointNum)
		{
			if (hitBaseTargetPos) 
			{
				break; 
			}

			glm::vec4 endPointStartPos = raycastRangeEndpoints[endPointNum];
			glm::vec4 endPointEndPos = raycastRangeEndpoints[endPointNum + 1];
			deltaPos = (endPointEndPos - endPointStartPos) / static_cast<float>(castsPerRange);
			size_t rangeCastCount = 0;
			// Raycast a set number of times within each range.
			while (rangeCastCount < castsPerRange)
			{
				castStartPos = endPointStartPos + deltaPos * static_cast<float>(rangeCastCount);
				result = Raycast::CastRay(castStartPos, baseTargetPos, camTargetPosHullSize);
				// Hit means the cam target position is not reachable via the raycast.
				if (result.hit)
				{
					adjustedHitPos = 
					(
						result.hitPos + result.rayNormal * min(result.rayLength, camTargetPosHullSize)
					);
					float hitZDist = fabsf(playerClosestLOSPos.z - adjustedHitPos.z);
					// If this raycast hit was closer in the vertical direction,
					// set it as the new closest position to the closest player LOS position.
					if (hitZDist < closestHitZDist)
					{
						closestHitPos = adjustedHitPos;
						closestHitZDist = hitZDist;
					}
				}
				else if (!a_baseTargetPosIsOOB && playerHasLOS)
				{
					// If the base target position is not out of bounds,
					// at least one player has LOS on the base target position,
					// and we haven't hit any obstructions while raycasting, 
					// the base target position is valid. Break and return it now.
					closestHitPos = baseTargetPos;
					hitBaseTargetPos = true;
					break;
				}

				++rangeCastCount;
				++totalCastCount;
			}
		}

		return ToNiPoint3(closestHitPos);
	}

	bool CameraManager::NoPlayersVisibleAtPoint(const RE::NiPoint3& a_point, bool&& a_checkAllNodes)
	{
		// Check if there are no players visible at the given point.
		// For each player, check if all player nodes are blocked from camera view or just one.
		// 'Visibility' here means a raycast to one or all nodes for each player does not hit
		// any intervening object.

		const auto strings = RE::FixedStrings::GetSingleton();
		const glm::vec4 point = ToVec4(a_point);
		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				// Invalid 3D means this player is not visible.
				// Move on to the next player.
				auto loadedData = p->coopActor->loadedData;
				if (!loadedData)
				{
					continue;
				}

				auto data3D = loadedData->data3D;
				if (!data3D || !data3D->parent)
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
					if (auto nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); nodePtr && nodePtr.get())
					{
						// Cast from node to point instead of the other way around because 
						// if there is a "one-way" surface that is facing up between 
						// the point and the node (such as some terrain 3D objects),
						// the raycast results will not contain a hit when casting from point to node.
						auto losCheck = Raycast::hkpCastRay
						(
							ToVec4(nodePtr->world.translate), 
							point, 
							std::vector<RE::NiAVObject*>{ playerCam->cameraRoot.get(), data3D.get() }, 
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
								// Move to next player if this player has a node that is not visible.
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
		}

		return true;
	}

	bool CameraManager::PointOnScreenAtCamOrientation(const RE::NiPoint3& a_point, const RE::NiPoint3& a_camPos, const RE::NiPoint2& a_rotation, const float& a_pixelMargin)
	{
		// Is the given point in the camera's frustum at the given camera position and rotation,
		// also accounting for a pixel margin at the edges of the screen, if given.

		bool onScreen = false;
		const auto& niCam = Util::GetNiCamera();
		Util::SetRotationMatrixPY(playerCam->cameraRoot->local.rotate, a_rotation.x, a_rotation.y);
		Util::SetCameraPosition(playerCam, a_camPos);
		RE::NiUpdateData updateData;
		playerCam->cameraRoot->UpdateDownwardPass(updateData, 0);
		if (const auto hud = DebugAPI::GetHUD(); hud)
		{
			RE::GRect gRect = hud->uiMovie->GetVisibleFrameRect();
			const float rectWidth = fabsf(gRect.right - gRect.left);
			const float rectHeight = fabsf(gRect.bottom - gRect.top);
			RE::NiRect<float> port{ gRect.left, gRect.right, gRect.top, gRect.bottom };
			float x = 0.0f, y = 0.0f, z = 0.0f;
			RE::NiCamera::WorldPtToScreenPt3(niCam->worldToCam, port, a_point, x, y, z, 1e-5f);
			onScreen = 
			(
				x >= gRect.left + a_pixelMargin && 
				x <= gRect.right - a_pixelMargin && 
				y >= gRect.top + a_pixelMargin && 
				y <= gRect.bottom - a_pixelMargin && 
				z < 1.0f &&
				z > -1.0f
			);
		}

		return onScreen;
	}

	void CameraManager::ResetCamData()
	{
		// Reset all camera data.
		
		// Reset controller IDs.
		controlCamCID = -1;
		focalPlayerCID = -1;

		// Reset interp data.
		lockOnIndicatorOscillationInterpData->ResetData();
		movementPitchInterpData->ResetData();
		movementYawInterpData->ResetData();
		// Reset interp factors and ratio for blending pitch/yaw changes.
		if (camState == CamState::kManualPositioning)
		{
			camInterpFactor = Settings::fCamManualPosInterpFactor;
		}
		else
		{
			camInterpFactor = 
			(
				Settings::bCamCollisions ? 
				Settings::fCamCollisionInterpFactor : 
				Settings::fCamNoCollisionInterpFactor
			);
		}

		camFrameDepInterpFactor = min(1.0f, *g_deltaTimeRealTime * 60.0f * camInterpFactor);
		prevRotInterpRatio = 0.0f;
		// Reset to autotrail state.
		prevCamState = camState = CamState::kAutoTrail;
		delayedZoomInUnderExteriorRoof = delayedZoomOutUnderExteriorRoof = false;
		isAutoTrailing = true;
		isColliding = false;
		isManuallyPositioned = false;
		isLockedOn = false;
		lockInteriorOrientationOnInit = false;
		// Reset lock on-related data.
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
						ALYSLC::Log("[CAM] ERR: ResetCamData: Could not get third person state.");
					}
				}
				else
				{
					ALYSLC::Log("[CAM] ERR: ResetCamData: Could not get camera state.");
				}
			}
		}
		else
		{
			ALYSLC::Log("[CAM] ERR: ResetCamData: Could not get player cam.");
		}

		// Set height. Higher above if exterior cell.
		avgPlayerHeight = 0.0f;
		std::for_each(glob.coopPlayers.begin(), glob.coopPlayers.end(),
			[&](const auto& a_p) {
				if (a_p->isActive)
				{
					avgPlayerHeight += a_p->mm->playerScaledHeight;
				}
			});
		avgPlayerHeight /= glob.livingPlayers;
		// Set rotation-related data.
		movementPitchRunningTotal = movementYawToCamRunningTotal = 0.0f;
		numMovementPitchReadings = numMovementYawToCamReadings = 0;
		// All anchor points set to origin point initially.
		camBaseFocusPointZOffset = camFocusPointZOffset = 0.0f;
		// Set radial distance. Further away from players if exterior cell.
		camTargetRadialDistance = 
		camBaseRadialDistance =
		camCollisionRadialDistance = 
		camSavedBaseRadialDistance = exteriorCell ? 700.0f : 200.0f;
		camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;

		auto camNodePos = tpState->camera->cameraRoot->local.translate;
		camBaseOriginPoint = camNodePos;
		/*if (playerCam)
		{
			camBaseOriginPoint = playerCam->cameraRoot->world.translate;
		}
		else */if (auto p1 = RE::PlayerCharacter::GetSingleton(); p1)
		{
			camBaseOriginPoint = p1->GetLookingAtLocation();
		}

		camCollisionOriginPoint = camBaseOriginPoint;
		camOriginPointDirection = RE::NiPoint3();
		camBaseFocusPoint =
		camBaseTargetPos =
		camCollisionTargetPos =
		camCollisionTargetPos2 =
		camCollisionFocusPoint =
		camFocusPoint =
		camOriginPoint =
		camTargetPos =
		camLockOnFocusPoint = camBaseOriginPoint;
		auto bounds = Util::GetVertCollPoints(camCollisionOriginPoint, 0.0f);
		camMaxAnchorPointZCoord = bounds.first;
		camMinAnchorPointZCoord = bounds.second;

		auto p1LookingAt = glob.player1Actor->GetLookingAtLocation();
		float p1Heading = glob.player1Actor->GetHeading(false);
		// Set initial rotation.
		camPitch = 
		camCurrentPitchToFocus = 
		camBaseTargetPosPitch = 
		camTargetPosPitch = 
		(
			exteriorCell ? atanf(1.0f / sqrtf(2.0f)) : 5.0f * PI / 180.0f
		);
		if (playerCam && playerCam->cameraRoot) 
		{
			float nodeYaw = Util::DirectionToGameAngYaw
			(
				playerCam->cameraRoot->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f)
			);
			camYaw = camCurrentYawToFocus = camBaseTargetPosYaw = camTargetPosYaw = nodeYaw;
		}
		else
		{
			camYaw = camCurrentYawToFocus = camBaseTargetPosYaw = camTargetPosYaw = p1Heading;
		}

		std::optional<RE::TESObjectREFR*> closestTeleportDoor = std::nullopt;
		std::optional<float> closestTeleportDoorDistComp = std::nullopt;
		// Check for nearby teleport doors within the current cam radial distance of player 1.
		// If so, the camera should be set in front of the party when enabled.
		Util::ForEachReferenceInRange(
			p1LookingAt, camTargetRadialDistance + camTargetPosHullSize, false,
			[&](RE::TESObjectREFR* a_refr) {
				if (!a_refr || !Util::HandleIsValid(a_refr->GetHandle()) || !a_refr->IsHandleValid())
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				// Ensure that the object reference is an interactable object.
				if (a_refr->Is3DLoaded() && a_refr->GetCurrent3D() && 
					!a_refr->IsDeleted() && strlen(a_refr->GetName()) > 0)
				{
					if ((a_refr->Is(RE::FormType::Door, RE::FormType::Activator)) || 
						(a_refr->data.objectReference && a_refr->data.objectReference->Is(RE::FormType::Door, RE::FormType::Activator)))
					{
						// Only consider doors that teleport the player when activated.
						if (a_refr->extraList.HasType<RE::ExtraTeleport>())
						{
							// Set in front, if in exterior cell on cam enable.
							if (exteriorCell)
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
							else
							{
								// Otherwise, place between door and P1 initially.
								auto refrCenter = Util::Get3DCenterPos(a_refr);
								auto toRefrCenterXY = refrCenter - p1LookingAt;
								toRefrCenterXY.z = 0.0f;
								toRefrCenterXY.Unitize();
								auto camBackwardXY = Util::RotationToDirectionVect
								(
									0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(p1Heading + PI))
								);
								float xyComponentMag = toRefrCenterXY.Dot(camBackwardXY);

								if (xyComponentMag >= 0.0f)
								{
									if ((!closestTeleportDoorDistComp.has_value()) || 
										(xyComponentMag < closestTeleportDoorDistComp.value()))
									{
										closestTeleportDoorDistComp = xyComponentMag;
										closestTeleportDoor = a_refr;
									}
								}
							}
						}
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		// Clamp trailing distance to place the camera between the door and P1.
		if (!exteriorCell && closestTeleportDoorDistComp.has_value())
		{
			camTargetRadialDistance = 
			camBaseRadialDistance = 
			camSavedBaseRadialDistance =  closestTeleportDoorDistComp.value();
			lockInteriorOrientationOnInit = true;
		}

		// If the cam is automatically resuming, adjust initial yaw to position the camera between P1 and the closest load door.
		if (!waitForToggle)
		{
			if (auto doorRefr = closestTeleportDoor.has_value() ? closestTeleportDoor.value() : nullptr; doorRefr)
			{
				camYaw =
				camCurrentYawToFocus =
				camBaseTargetPosYaw =
				camTargetPosYaw = Util::GetYawBetweenPositions(Util::Get3DCenterPos(doorRefr), p1LookingAt);
			}
		}
	}

	void CameraManager::ResetFadeOnObjects()
	{
		// Reset fade on all handled objects.

		if (!obstructionFadeDataSet.empty() || !obstructionsToFadeIndicesMap.empty())
		{
			for (const auto& objectData : obstructionFadeDataSet)
			{
				objectData->InstantlyResetFade();
			}

			obstructionFadeDataSet.clear();
			obstructionsToFadeIndicesMap.clear();
		}
	}

	void CameraManager::SetCamActorCollisions(bool&& a_set)
	{
		// Remove collisions between the camera and character controllers
		// to prevent actors from fading when they are too close to
		// the camera.

		if (auto rBody = playerCam->rigidBody; rBody)
		{
			if (auto world = rBody->GetWorld1(); world)
			{
				if (auto filterInfo = (RE::bhkCollisionFilter*)world->collisionFilter; filterInfo)
				{
					// Credits to ersh1 for the code on setting what other collision layers collide with a collision layer:
					// https://github.com/ersh1/Precision/blob/main/src/Hooks.cpp#L848
					if (a_set)
					{
						// Camera collides with char controller.
						filterInfo->layerBitfields[!RE::COL_LAYER::kCamera] |= 
						(
							static_cast<uint64_t>(1) << static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
						);
						// Char controller collides with camera.
						filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] |= 
						(
							static_cast<uint64_t>(1) << static_cast<uint8_t>(!RE::COL_LAYER::kCamera)
						);
					}
					else
					{
						// Camera won't collide with char controller.
						filterInfo->layerBitfields[!RE::COL_LAYER::kCamera] &= 
						~(
							static_cast<uint64_t>(1) << static_cast<uint8_t>(!RE::COL_LAYER::kCharController)
						);
						// Char controller won't collide with camera.
						filterInfo->layerBitfields[!RE::COL_LAYER::kCharController] &= 
						~(
							static_cast<uint64_t>(1) << static_cast<uint8_t>(!RE::COL_LAYER::kCamera)
						);
					}
				}
			}
		}
	}

	void CameraManager::SetPlayerFadePrevention(bool&& a_noFade)
	{
		// Enable/disable fading of players.

		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				if (auto p3D = Util::GetRefr3D(p->coopActor.get()); p3D)
				{
					if (a_noFade) 
					{
						p3D->fadeAmount = 1.0f;
						p3D->flags.set(RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade);
						RE::NiUpdateData updateData;
						p3D->UpdateDownwardPass(updateData, 0);
					}
					else
					{
						p3D->flags.reset(RE::NiAVObject::Flag::kAlwaysDraw, RE::NiAVObject::Flag::kIgnoreFade);
						RE::NiUpdateData updateData;
						p3D->UpdateDownwardPass(updateData, 0);
					}
				}
			}
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

		if (playerCam = RE::PlayerCamera::GetSingleton(); playerCam)
		{
			// Need to wait for the camera to fully transition to FP state before toggling back to TP state.
			// Otherwise, the player's FP arms will stick around and their TP skeleton will be invisible.
			// Hacky, but it works well enough.
			if (a_fromFirstPerson)
			{
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				{
					std::unique_lock<std::mutex> togglePOVLock(camTogglePOVMutex, std::try_to_lock);
					if (togglePOVLock)
					{
						ALYSLC::Log("[CAM] ToThirdPersonState. Lock obtained. (0x{:X})", hash);
						isTogglingPOV = true;
					}
					else
					{
						// Could not obtain lock to toggle POV, so return here without enqueueing any tasks.
						return;
					}
				}

				glob.taskRunner->AddTask([this]() {
					if (auto controlMap = RE::ControlMap::GetSingleton(); controlMap)
					{
						if (auto ue = RE::UserEvents::GetSingleton(); ue)
						{
							std::this_thread::sleep_for(1s);
							Util::AddSyncedTask(
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

						// REMOVE
						const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
						ALYSLC::Log("[CAM] ToThirdPersonState. Getting lock from global task runner. (0x{:X})", hash);
						{
							std::unique_lock<std::mutex> togglePOVLock(camTogglePOVMutex);
							isTogglingPOV = false;
						}
					}
				});
			}
			else
			{
				// Force switch to TP state here does not produce any problems.
				isTogglingPOV = true;
				playerCam->lock.Lock();
				playerCam->ForceThirdPerson();
				playerCam->UpdateThirdPerson(true);
				playerCam->lock.Unlock();
				isTogglingPOV = false;
			}
		}
	}

	void CameraManager::UpdateCamRotation()
	{
		// Update the base and current camera pitch and yaw to set.

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		bool validLockOnTarget = camLockOnTargetPtr && camLockOnTargetPtr.get();
		// Snap to new targets, but rotate slowly when adjusting angle to the same target.
		float maxRotRads = camMaxAngRotRate * *g_deltaTimeRealTime;
		auto pitchDelta = 0.0f;
		auto yawDelta = 0.0f;
		float rsX = 0.0f;
		float rsY = 0.0f;
		float rsMag = 0.0f;
		if (isAutoTrailing)
		{
			camMaxPitchAngMag = autoTrailPitchMax;
			if (camAdjMode == CamAdjustmentMode::kRotate && controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
			{
				const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
				rsX = rsData.xComp;
				rsY = rsData.yComp;
				rsMag = rsData.normMag;
				if (rsMag != 0.0f)
				{
					// Moving the RS left or right causes counterclockwise or
					// clockwise rotation of the camera.
					yawDelta = maxRotRads * rsX * rsMag;
					camBaseTargetPosYaw += yawDelta;
					// Moving the RS up or down causes the camera to pitch
					// upward or downward.
					// Upward results in a negative pitch change,
					// downward results in a positive pitch change.
					pitchDelta = maxRotRads * rsY * rsMag;
					camBaseTargetPosPitch -= pitchDelta;
				}
			}
		}
		else if (isLockedOn)
		{
			if (Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom || !validLockOnTarget)
			{
				if (camAdjMode == CamAdjustmentMode::kRotate && controlCamCID > -1 &&
					controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
				{
					camMaxPitchAngMag = autoTrailPitchMax;
					const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
					rsX = rsData.xComp;
					rsY = rsData.yComp;
					rsMag = rsData.normMag;
					if (rsMag != 0.0f)
					{
						yawDelta = maxRotRads * rsX * rsMag;
						pitchDelta = maxRotRads * rsY * rsMag;

						camBaseTargetPosPitch -= pitchDelta;
						camBaseTargetPosYaw += yawDelta;
					}
				}
			}
		}
		else
		{
			camMaxPitchAngMag = PI / 2.0f;
			if (camAdjMode == CamAdjustmentMode::kRotate && controlCamCID > -1 && 
				controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
			{
				const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
				rsX = rsData.xComp;
				rsY = rsData.yComp;
				rsMag = rsData.normMag;
				if (rsMag > 0.0f)
				{
					yawDelta = maxRotRads * rsX * rsMag;
					camBaseTargetPosYaw += yawDelta;
					pitchDelta = maxRotRads * rsY * rsMag;
					camBaseTargetPosPitch -= pitchDelta;
				}
			}
		}

		// Calculate pitch incline offset/yaw diff only when autotrailing or in lock on mode without a target
		// and the camera-controlling player is not rotating the camera.
		bool playerInCombat = std::any_of
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(), 
			[](const auto& a_p) 
			{ 
				return a_p->isActive && a_p->coopActor->IsInCombat(); 
			}
		);
		bool autoRotate = 
		{
			(Settings::bAutoRotateCamPitch || Settings::bAutoRotateCamYaw) &&
			(camAdjMode != CamAdjustmentMode::kRotate || rsMag == 0.0f) &&
			(
				!isManuallyPositioned && (isAutoTrailing || !validLockOnTarget ||
				Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom)
			)
		};
		if (autoRotate)
		{
			if (Settings::bAutoRotateCamPitch)
			{
				movementPitchInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
				if (movementPitchInterpData->secsSinceUpdate >= movementPitchInterpData->secsUpdateInterval)
				{
					// Randomly becomes NAN, and must be reset. Temp solution.
					movementPitchRunningTotal = 
					(
						isnan(movementPitchRunningTotal) ?
						0.0f : 
						movementPitchRunningTotal
					);
					auto movementPitch = 
					(
						numMovementPitchReadings != 0.0f ? 
						movementPitchRunningTotal / static_cast<float>(numMovementPitchReadings) : 
						0.0f
					);
					SetMovementPitchRunningTotal(true);
					float sign = movementPitch < 0.0f ? -1.0f : 1.0f;
					// It (maybe) just works. Used Desmos (https://www.desmos.com/calculator)
					// to create a curve that smooths out the changes in camera pitch relative
					// to average support surface/vertical velocity pitch.
					movementPitch = 1.5f * tanf(0.4f * movementPitch - 0.1f) * cosf(movementPitch) + 0.15f;
					// Pitch increments/decrements are smaller when approaching PI/2 in the direction of the
					// average movement pitch. Done to prevent over-adjustment when already at a steep pitch.
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
					movementPitchInterpData->secsSinceUpdate / movementPitchInterpData->secsUpdateInterval, 
					1.0f
				);
				movementPitchInterpData->InterpolateSmootherStep(tRatio);
			}

			if (Settings::bAutoRotateCamYaw)
			{
				movementYawInterpData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
				if (movementYawInterpData->secsSinceUpdate >= movementYawInterpData->secsUpdateInterval)
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
						movementYawToCamRunningTotal / static_cast<float>(numMovementYawToCamReadings) :
						0.0f
					);
					SetMovementYawToCamRunningTotal(true);
					// Since the party's averaged movement direction varies less relative to 
					// the camera's facing direction at larger trailing distances,
					// less auto-rotation is required to keep players in frame.
					// Apply radial distance factor to decrease yaw auto rotation 
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
								focalPlayerCID == -1 ? 
								camTargetRadialDistance :
								camBaseRadialDistance
							)
						)
					);
					movementYaw = std::clamp
					(
						radialDistFactor * (movementYaw / (PI / 2.0f)) * camMaxAngRotRate * *g_deltaTimeRealTime, 
						-fabsf(movementYaw), 
						fabsf(movementYaw)
					);
					movementYawInterpData->ShiftEndpoints(movementYaw);
					movementYawInterpData->SetTimeSinceUpdate(*g_deltaTimeRealTime);
				}

				SetMovementYawToCamRunningTotal(false);
				float tRatio = min
				(
					movementYawInterpData->secsSinceUpdate / movementYawInterpData->secsUpdateInterval, 
					1.0f
				);
				auto prev = movementYawInterpData->current;
				movementYawInterpData->InterpolateSmootherStep(tRatio);
			}
		}
		else if (numMovementPitchReadings != 0 || numMovementYawToCamReadings != 0)
		{
			// If not already reset, reset movement pitch/yaw totals when auto rotate is not active.
			SetMovementPitchRunningTotal(true);
			SetMovementYawToCamRunningTotal(true);
		}

		// Cam pitch is clamped to +- autotrail/lock on max pitch.
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

		// Blend target/to focus pitch/yaw values if auto trailing or in partially automated lock on state.
		if (!isManuallyPositioned && (isAutoTrailing || !validLockOnTarget || 
			Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kZoom))
		{
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
					camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw + movementYawDelta);
				}
				else
				{
					camTargetPosYaw = 
					camBaseTargetPosYaw = Util::NormalizeAng0To2Pi(camBaseTargetPosYaw);
				}
			}

			float oldPitchToFocus = camCurrentPitchToFocus;
			float oldYawToFocus = camCurrentYawToFocus;

			if (focalPlayerCID == -1) 
			{
				camCurrentPitchToFocus = Util::NormalizeAngToPi
				(
					Util::GetPitchBetweenPositions(camTargetPos, camBaseFocusPoint)
				);
				camCurrentYawToFocus = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions(camTargetPos, camBaseFocusPoint)
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
					Util::GetYawBetweenPositions(camTargetPos, camBaseFocusPoint)
				);
			}

			if (Settings::bCamCollisions)
			{
				// TL;DR: Blend pitch/yaw when the camera is moving quickly relative to the focus point. 
				// Best I can do for now.
				// 
				// If the below trailing distance and/or pitch factors are large, 
				// this means that the current camera target position is either
				// close to the focus point or pitched sharply with respect to the focus point.
				// In either case, large changes in pitch/yaw occur since we are trying to 
				// angle the camera at the focus point, ideally at all times.
				// For example, if the focus point moves behind the target position,
				// the camera will attempt to flip and face the focus, 
				// which to put it lightly, is jarring and nauseating.
				// With this in mind, and without an exact mathematical solution to find when the more stable 
				// and focus-independent target position pitch/yaw readings should be switched to, 
				// some rough blending is required to bounce between the two sets of pitch/yaw readings.
				//
				// 1. Stick to the focus point-relative readings 
				// when the trailing distance and pitch factors are small.
				// 2. Switch over to approaching the target pos pitch/yaw readings 
				// when the base radial distance approaches the min radial distance,
				// or the base camera pitch magnitude approaches the max allowable pitch magnitude, 
				// both "danger zones" where the pitch/yaw to the focus point changes rapidly.
				// 3. The max pitch is also capped to prevent the camera 
				// from getting too close to the focus point in the XY plane.
				// NOTE: As a result of this imperfect blending, 
				// a hitch results when moving towards and then away from max pitch,
				// but it is, in my opinion, an acceptable tradeoff for smoother camera rotation 
				// that nearly stays affixed to the focus point, 
				// regardless of camera collisions and distance to the focus point.

				float xyTrailingDist = max(0.1f, Util::GetXYDistance(camTargetPos, camFocusPoint));
				float trailingDistFactor = 
				(
					powf(1.0f - (xyTrailingDist - camMinTrailingDistance) / xyTrailingDist, 0.5f)
				);
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
					// or when the camera moving through/along an obstruction.
					interpRatio += (1.0f - prevRotInterpRatio) / 10.0f;
				}
				else
				{
					// Catch up to target interp ratio fast when rotating.
					float diff = (min(trailingDistFactor, 1.0f) - prevRotInterpRatio);
					// Slower shift towards pitch/yaw to focus point.
					if (diff < 0.0f)
					{
						diff /= 10.0f;
					}

					interpRatio += diff;
				}

				interpRatio = Util::InterpolateSmootherStep
				(
					prevRotInterpRatio, interpRatio, camFrameDepInterpFactor
				);
				const float finalInterpFactor = powf(interpRatio, interpPower);

				float targetPitch = Util::NormalizeAngToPi
				(
					Util::InterpolateSmootherStep
					(
						camCurrentPitchToFocus, camTargetPosPitch, finalInterpFactor
					)
				);

				// Modify target yaw by adding 2 PI if the difference between the two yaws is greater than PI to
				// ensure that the interpolation heads in the correct direction.
				// Pitch difference never reaches PI in magnitude, so no need to modify either endpoint.
				float yawStart = camCurrentYawToFocus;
				float yawEnd = camTargetPosYaw;
				// Set cam pitch/yaw to apply.
				float yawDiff = Util::NormalizeAngToPi(yawEnd - yawStart);
				float targetYaw = Util::NormalizeAng0To2Pi
				(
					yawStart + Util::InterpolateSmootherStep(0.0f, yawDiff, finalInterpFactor)
				);
				camPitch = targetPitch;
				camYaw = targetYaw;
				prevRotInterpRatio = interpRatio;

			}
			else if (lockInteriorOrientationOnInit)
			{
				// All set to base when not using camera collisions or orientation is locked.
				camPitch =
				camTargetPosPitch = std::clamp(camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag);
				camYaw = camTargetPosYaw = camBaseTargetPosYaw;
			}
			else
			{
				if (focalPlayerCID == -1)
				{
					// Set directly to target pos values if not blending.
					// Must be far away enough from cam as well.
					camPitch = std::clamp(camCurrentPitchToFocus, -camMaxPitchAngMag, camMaxPitchAngMag);
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
		else if (isLockedOn && validLockOnTarget && Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kZoom)
		{
			// NOTE: Temporarily using the base target position as the rotation
			// target to prevent frequent camera jumping from focus point hit to focus point hit.
			// Have to figure out what's causing it, but the bug likely stems
			// from rotation-to-position cross-influence and interpolation layered on top of that.
			auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
			if (!camLockOnTargetPtr)
			{
				return;
			}

			float pitchToTarget = Util::NormalizeAngToPi
			(
				Util::GetPitchBetweenPositions(camBaseTargetPos, camLockOnFocusPoint)
			);
			float pitchToParty = Util::NormalizeAngToPi
			(
				Util::GetPitchBetweenPositions(camBaseTargetPos, camOriginPoint)
			);
			float yawToTarget = Util::NormalizeAng0To2Pi
			(
				Util::GetYawBetweenPositions(camBaseTargetPos, camLockOnFocusPoint)
			);

			float avgPitchPartyAndTarget = (pitchToTarget + pitchToParty) / 2.0f;
			pitchDelta = std::clamp
			(
				Util::NormalizeAngToPi(avgPitchPartyAndTarget - camPitch), 
				-maxRotRads, 
				maxRotRads
			);
			yawDelta = std::clamp
			(
				Util::NormalizeAngToPi(yawToTarget - camYaw), 
				-maxRotRads, 
				maxRotRads
			);

			float currentOriginToTargetXYDist = Util::GetXYDistance(camOriginPoint, camLockOnFocusPoint);
			float distToOriginPoint = camBaseTargetPos.GetDistance(camOriginPoint);
			float targetPosToTargetFactor = Util::InterpolateEaseIn
			(
				0.0f, 
				1.0f, 
				std::clamp
				(
					Util::GetXYDistance(camBaseTargetPos, camLockOnFocusPoint) / (distToOriginPoint * 1.25f), 
					0.0f, 
					1.0f
				), 
				3.0f
			);
			float yawDiff = std::clamp
			(
				fabsf
				(
					Util::NormalizeAngToPi
					(
						Util::NormalizeAng0To2Pi
						(
							Util::GetYawBetweenPositions(camOriginPoint, camLockOnFocusPoint)
						) - camYaw
					)
				), 
				0.0f, 
				PI
			);
			if (yawDiff >= PI / 2.0f && yawDiff <= PI)
			{
				yawDiff = PI - yawDiff;
			}

			float partyToTargetYawFactor = Util::InterpolateSmootherStep(0.1, 0.4f, yawDiff / (PI / 2.0f));
			float movementSpeedFactor = Util::InterpolateSmootherStep
			(
				0.1f,
				0.4f, 
				std::clamp
				(
					log2f(camLockOnTargetPtr->DoGetMovementSpeed() / camMaxMovementSpeed + 1.0f), 
					0.0f, 
					1.0f
				)
			);

			pitchDelta = std::clamp
			(
				pitchDelta * targetPosToTargetFactor * (partyToTargetYawFactor + movementSpeedFactor), 
				-maxRotRads,
				maxRotRads
			);
			yawDelta = std::clamp
			(
				yawDelta * targetPosToTargetFactor * (partyToTargetYawFactor + movementSpeedFactor), 
				-maxRotRads, 
				maxRotRads
			);
			camPitch = Util::NormalizeAngToPi(camPitch + pitchDelta);
			camYaw = Util::NormalizeAng0To2Pi(camYaw + yawDelta);

			// Set to zero so it has no bearing on zoom/focus point Z offset changes.
			camTargetPosPitch = camBaseTargetPosPitch = 0.0f;
			// Set equal to cam facing direction yaw angle.
			camTargetPosYaw = camBaseTargetPosYaw = camYaw;
		}
		else
		{
			// Set directly to target pos values when in a fully-automated lock on or manually positioned state.
			camPitch = 
			camTargetPosPitch = std::clamp(camBaseTargetPosPitch, -camMaxPitchAngMag, camMaxPitchAngMag);
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

		/*
		// REMOVE DEBUG DRAWS
		// Red: Target Pitch/Yaw
		// Green: New Pitch/Yaw to Focus.
		// Blue: Last Set Pitch/Yaw to Focus.
		const float linePixelLength = 200.0f;
		glm::vec2 pitchCenter{ DebugAPI::screenResX / 3.0f, DebugAPI::screenResY / 2.0f };
		glm::vec2 yawCenter{ 2.0f * DebugAPI::screenResX / 3.0f, DebugAPI::screenResY / 2.0f };

		// Pitch
		DebugAPI::QueueLine2D(pitchCenter,
			{ 
				pitchCenter.x - linePixelLength * cosf(camTargetPosPitch),
				pitchCenter.y + linePixelLength * sinf(camTargetPosPitch) 
			},
			0xFF0000FF, 2.0f);

		DebugAPI::QueueLine2D(pitchCenter,
			{ 
				pitchCenter.x - linePixelLength * cosf(camCurrentPitchToFocus),
				pitchCenter.y + linePixelLength * sinf(camCurrentPitchToFocus) 
			},
			0x00FF00FF, 2.0f);

		DebugAPI::QueueLine2D(pitchCenter,
			{
				pitchCenter.x - linePixelLength * cosf(camPitch),
				pitchCenter.y + linePixelLength * sinf(camPitch) 
			},
			0x0000FFFF, 2.0f);

		// Yaw.
		DebugAPI::QueueLine2D(yawCenter,
			{ 
				yawCenter.x - linePixelLength * cosf(Util::ConvertAngle(camTargetPosYaw)),
				yawCenter.y + linePixelLength * sinf(Util::ConvertAngle(camTargetPosYaw)) 
			},
			0xFF0000FF, 2.0f);

		DebugAPI::QueueLine2D(yawCenter,
			{ 
				yawCenter.x - linePixelLength * cosf(Util::ConvertAngle(camCurrentYawToFocus)),
				yawCenter.y + linePixelLength * sinf(Util::ConvertAngle(camCurrentYawToFocus)) 
			},
			0x00FF00FF, 2.0f);

		DebugAPI::QueueLine2D(yawCenter,
			{ 
				yawCenter.x - linePixelLength * cosf(Util::ConvertAngle(camYaw)),
				yawCenter.y + linePixelLength * sinf(Util::ConvertAngle(camYaw))
			},
			0x0000FFFF, 2.0f);
		*/
	}


	void CameraManager::UpdateCamHeight()
	{
		// Update the camera focus point's Z offset.
		
		// No height offset to set when manually positioned.
		if (isManuallyPositioned)
		{
			return;
		}

		auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
		bool validLockOnTarget = camLockOnTargetPtr && camLockOnTargetPtr.get();
		bool canAdjustPosOffset = 
		{
			(focalPlayerCID == -1) &&
			(!isLockedOn || !validLockOnTarget || Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull) &&
			(camAdjMode == CamAdjustmentMode::kZoom && controlCamCID > -1 && controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
		};
		if (canAdjustPosOffset)
		{
			const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
			const auto& rsX = rsData.xComp;
			const auto& rsY = rsData.yComp;
			const auto& rsMag = rsData.normMag;
			// Change height of the focus point if the x comp is larger than the y comp.
			if (fabsf(rsX) > fabsf(rsY))
			{
				// Right to increase height, down to decrease.
				camBaseFocusPointZOffset += rsX * rsMag * camMaxMovementSpeed * *g_deltaTimeRealTime;
			}
		}
		else if (isLockedOn && validLockOnTarget && Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kFull)
		{
			// Origin point already offset by average player height.
			// Auto-set z offset to reach feet/head level when the target is at max upward/downward pitch relative to the origin point.
			float originPitchToTarget = Util::NormalizeAngToPi
			(
				Util::GetPitchBetweenPositions(camOriginPoint, camLockOnFocusPoint)
			);
			float newZOffset = std::lerp
			(
				-avgPlayerHeight, 
				3.0f * avgPlayerHeight, 
				(originPitchToTarget / (PI / 2.0f) + 1.0f) / 2.0f
			);
			camBaseFocusPointZOffset = newZOffset;
		}

		float prevOffset = camFocusPointZOffset;
		if (Settings::bCamCollisions)
		{
			float newZOffset = camBaseFocusPointZOffset;
			float currentFocusZPos = camCollisionOriginPoint.z + newZOffset;
			float boundsDiff = fabsf(camMaxAnchorPointZCoord - camMinAnchorPointZCoord);
			if ((boundsDiff < camAnchorPointHullSize) && 
				(currentFocusZPos < camMinAnchorPointZCoord || currentFocusZPos > camMaxAnchorPointZCoord))
			{
				newZOffset = camMinAnchorPointZCoord + boundsDiff / 2.0f;
			}
			else
			{
				if (currentFocusZPos > camMaxAnchorPointZCoord)
				{
					newZOffset = min
					(
						newZOffset, 
						camMaxAnchorPointZCoord - camCollisionOriginPoint.z - camAnchorPointHullSize
					);
				}

				if (currentFocusZPos < camMinAnchorPointZCoord)
				{
					newZOffset = max
					(
						newZOffset, 
						camMinAnchorPointZCoord - camCollisionOriginPoint.z + camAnchorPointHullSize
					);
				}
			}

			camFocusPointZOffset = Util::InterpolateSmootherStep
			(
				prevOffset, newZOffset, camFrameDepInterpFactor
			);
		}
		else
		{
			camFocusPointZOffset = Util::InterpolateSmootherStep
			(
				prevOffset, camBaseFocusPointZOffset, camFrameDepInterpFactor
			);
		}
	}

	void CameraManager::UpdateCamZoom()
	{
		// Update the camera's zoom, auto-zooming out
		// to keep all players in view and auto- zooming in
		// when under an exterior roof, as necessary.

		// Auto zoom in/out.
		if (!isManuallyPositioned)
		{
			const float prevRadialDistance = camTargetRadialDistance;
			// Zoom offset decreases (zoom in) when moving the RS up,
			// and increases (zoom out) when moving the RS down.
			// Behaves the same for all camera modes.
			// Only adjust base radial distance if requested.
			auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
			bool validLockOnTarget = camLockOnTargetPtr && camLockOnTargetPtr.get();
			bool canAdjustZoom = 
			{
				(!isLockedOn || !validLockOnTarget || Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kFull) &&
				(camAdjMode == CamAdjustmentMode::kZoom && controlCamCID > -1 && controlCamCID < ALYSLC_MAX_PLAYER_COUNT)
			};
			if (canAdjustZoom)
			{
				const auto& rsData = glob.cdh->GetAnalogStickState(controlCamCID, false);
				const auto& rsX = rsData.xComp;
				const auto& rsY = rsData.yComp;
				const auto& rsMag = rsData.normMag;
				if (fabsf(rsY) > fabsf(rsX))
				{
					camBaseRadialDistance -= *g_deltaTimeRealTime * camMaxMovementSpeed * rsY * rsMag;
					camBaseRadialDistance = max(camBaseRadialDistance, camMinTrailingDistance);
				}
			}
			else if (isLockedOn && validLockOnTarget && Settings::uLockOnAssistance == !CamLockOnAssistanceLevel::kFull)
			{
				camBaseRadialDistance = camMinTrailingDistance;
			}

			camMaxZoomOutDist = Settings::fMaxRaycastAndZoomOutDistance;
			// Raycast hits and on-screen checks seem inconsistent when zoomed out beyond a variable distance.
			// Zooming out beyond this distance will result in the game considering all players offscreen (and no raycast hits),
			// even though their positions are in front of the camera and in view.
			// Get approximation by binary searching a range.
			float radialDistanceRangeMin = 0.0f;
			float radialDistanceRangeMax = Settings::fMaxRaycastAndZoomOutDistance;
			float radialDistanceRangeMid = Settings::fMaxRaycastAndZoomOutDistance / 2.0f;
			auto dirFromFocus = Util::RotationToDirectionVect
			(
				camTargetPosPitch, 
				Util::ConvertAngle(Util::NormalizeAng0To2Pi(camTargetPosYaw + PI))
			);
			auto onScreenTestPos = camBaseFocusPoint + dirFromFocus * radialDistanceRangeMid;
			bool minDiffReached = false;
			bool currentCheckOnScreen = false;
			uint32_t i = 0;
			// With the default min zoom delta, this iterates 22 times.
			while (!minDiffReached)
			{
				currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation(onScreenTestPos, { camPitch, camYaw }, true);
				if (currentCheckOnScreen)
				{
					radialDistanceRangeMin = radialDistanceRangeMid;
				}
				else
				{
					radialDistanceRangeMax = radialDistanceRangeMid;
				}

				radialDistanceRangeMid = (radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f;
				onScreenTestPos = camBaseFocusPoint + dirFromFocus * (radialDistanceRangeMid);
				minDiffReached = radialDistanceRangeMax - radialDistanceRangeMin < Settings::fMinAutoZoomDelta;
				++i;
			}

			if (currentCheckOnScreen)
			{
				camMaxZoomOutDist = radialDistanceRangeMid;
			}
			else
			{
				// If the check failed to find a distance at which all players are on screen (range min is 0),
				// set to the previous radial distance to prevent jarring changes in zoom.
				camMaxZoomOutDist = max(prevRadialDistance, radialDistanceRangeMin);
			}

			// Zoom in when all players are under a roof or protruding surface above their heads.
			if (exteriorCell)
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
						auto aboveResult = Raycast::CastRay(headPos, headPos + glm::vec4(0.0f, 0.0f, 100000.0f, 0.0f), 0.0f);
						if (!aboveResult.hit)
						{
							allPlayersUnderExteriorRoof = false;
						}
						else
						{
							onePlayerUnderExteriorRoof = true;
						}
					}

					// Zoom in close when under a roof.
					// Maintain zoom level (disable zooming out manually) while under the roof.
					// Keeps the camera from stuttering as much from noisy raycast hits recorded when casting to
					// a base target position well outside of the enclosure that the players are inside.
					if (allPlayersUnderExteriorRoof && !delayedZoomInUnderExteriorRoof && !delayedZoomOutUnderExteriorRoof)
					{
						camSavedBaseRadialDistance = camBaseRadialDistance;
						delayedZoomInUnderExteriorRoof = true;
						delayedZoomOutUnderExteriorRoof = false;
						underExteriorRoofZoomInTP = SteadyClock::now();
					}
					else if (delayedZoomInUnderExteriorRoof)
					{
						if (onePlayerUnderExteriorRoof && Util::GetElapsedSeconds(underExteriorRoofZoomInTP) > 1.5f)
						{
							camBaseRadialDistance = camMinTrailingDistance;
						}
						else if (!onePlayerUnderExteriorRoof && !delayedZoomOutUnderExteriorRoof)
						{
							delayedZoomOutUnderExteriorRoof = true;
							delayedZoomInUnderExteriorRoof = false;
							noPlayersUnderExteriorRoofTP = SteadyClock::now();
						}
					}
					else if (delayedZoomOutUnderExteriorRoof)
					{
						if (!onePlayerUnderExteriorRoof && Util::GetElapsedSeconds(noPlayersUnderExteriorRoofTP) > 1.5f)
						{
							// Restore zoom when no players are under a roof.
							camBaseRadialDistance = camSavedBaseRadialDistance;
							delayedZoomOutUnderExteriorRoof = false;
							delayedZoomInUnderExteriorRoof = false;
						}
						else if (onePlayerUnderExteriorRoof && !delayedZoomInUnderExteriorRoof)
						{
							delayedZoomOutUnderExteriorRoof = false;
							delayedZoomInUnderExteriorRoof = true;
							underExteriorRoofZoomInTP = SteadyClock::now();
						}
					}
					else
					{
						underExteriorRoofZoomInTP = noPlayersUnderExteriorRoofTP = SteadyClock::now();
					}
				}
				else
				{
					// Restore zoom when no players are under a roof.
					camSavedBaseRadialDistance = camBaseRadialDistance;
					delayedZoomOutUnderExteriorRoof = false;
					delayedZoomInUnderExteriorRoof = false;
					underExteriorRoofZoomInTP = noPlayersUnderExteriorRoofTP = SteadyClock::now();
				}
				
			}

			// Lock position when loading into a new cell since the target position 
			// and focus point will be placed right behind P1 
			// and the cam will track to follow this point, 
			// which changes rapidly when close to the camera.
			bool wasLocked = lockInteriorOrientationOnInit;
			float distToFocus = camTargetPos.GetDistance(camBaseFocusPoint);
			lockInteriorOrientationOnInit = wasLocked && distToFocus < camMinTrailingDistance;
			if (wasLocked && !lockInteriorOrientationOnInit)
			{
				camTargetRadialDistance = camBaseRadialDistance = camMinTrailingDistance;
			}

			// Set to base radial distance before auto-zoom adjustment below.
			camTargetRadialDistance = camBaseRadialDistance;
			if (focalPlayerCID == -1)
			{
				// Zoom out.
				// First check if all players and any selected target are on screen
				// at the base radial distance target position.
				onScreenTestPos = camBaseFocusPoint + dirFromFocus * camTargetRadialDistance;
				currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation(
					onScreenTestPos, { camPitch, camYaw }, true);
				if (isLockedOn && validLockOnTarget && camLockOnTargetPtr &&
					Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kRotation)
				{
					currentCheckOnScreen &= PointOnScreenAtCamOrientation(
						camLockOnTargetPtr->data.location,
						onScreenTestPos,
						{ camPitch, camYaw },
						DebugAPI::screenResX / 10.0f);
				}

				// Not on the screen at the base radial distance target position.
				// Have to find a new radial distance that puts all players and the lock on target in view.
				if (!currentCheckOnScreen)
				{
					// Use a binary search-esque method to subdivide the radial distance range repeatedly until
					// the min zoom delta is reached (arbitrarily close to the target radial distance).
					// Start from the base radial distance.
					radialDistanceRangeMin = camTargetRadialDistance;
					radialDistanceRangeMax =
						(camTargetRadialDistance < camMaxZoomOutDist ?
								camMaxZoomOutDist :
								  camTargetRadialDistance * 2.0f);
					radialDistanceRangeMid = max(
						prevRadialDistance,
						(radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f);
					onScreenTestPos = camBaseFocusPoint + dirFromFocus * radialDistanceRangeMid;
					// REMOVE
					bool minZoomRangeReached = false;
					uint32_t i = 0;
					// Will iterate at most 22 times with the default min zoom delta.
					bool lockOnTargetOnScreen = false;
					while (!minZoomRangeReached)
					{
						currentCheckOnScreen = AllPlayersOnScreenAtCamOrientation(
							onScreenTestPos, { camPitch, camYaw }, true);
						if (isLockedOn && validLockOnTarget &&
							Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kRotation)
						{
							lockOnTargetOnScreen = PointOnScreenAtCamOrientation(
								camLockOnFocusPoint,
								onScreenTestPos,
								{ camPitch, camYaw },
								DebugAPI::screenResX / 10.0f);
							currentCheckOnScreen &= lockOnTargetOnScreen;
						}

						if (currentCheckOnScreen)
						{
							radialDistanceRangeMax = radialDistanceRangeMid;
						}
						else
						{
							radialDistanceRangeMin = radialDistanceRangeMid;
						}

						radialDistanceRangeMid = (radialDistanceRangeMax + radialDistanceRangeMin) / 2.0f;
						onScreenTestPos = camBaseFocusPoint + dirFromFocus * radialDistanceRangeMid;
						minZoomRangeReached = radialDistanceRangeMax - radialDistanceRangeMin < Settings::fMinAutoZoomDelta;
						++i;
					}

					// If zoomed out beyond the max zoom out distance computed above, do not modify the radial distance.
					// Set to current midpoint if on screen, or set to the max point if the current midpoint is not on screen.
					if (currentCheckOnScreen)
					{
						camTargetRadialDistance = radialDistanceRangeMid;
					}
					else if (radialDistanceRangeMax < camMaxZoomOutDist)
					{
						// Continue zooming out if not on screen.
						// Max endpoint was on the screen previously.
						camTargetRadialDistance = radialDistanceRangeMax;
					}
					else
					{
						// Reached max zoom out distance and still not on the screen (happens at times).
						// Set to previous value to avoid zooming out all of a sudden.
						camTargetRadialDistance = prevRadialDistance;
					}
				}

				camTargetRadialDistance = Util::InterpolateSmootherStep
				(
					prevRadialDistance, camTargetRadialDistance, camFrameDepInterpFactor
				);

				// Do not zoom out when colliding with a surface
				// or passing through an object.
				if (!isColliding || camTargetRadialDistance < prevRadialDistance)
				{
					camTargetRadialDistance = max(camTargetRadialDistance, camMinTrailingDistance);
				}
			}
		}
		else
		{
			camTargetRadialDistance = camBaseRadialDistance;
		}
	}

	void CameraManager::UpdateParentCell()
	{
		// Update the cached parent cell for the camera
		// based on the current parent cell for P1.
		// Reset fade and cam data if transitioning
		// from an exterior cell to an interior cell or vice versa.

		auto p1Cell = glob.player1Actor->GetParentCell();
		if (p1Cell && currentCell != p1Cell && p1Cell->formID != 0x0)
		{
			bool newIsExterior = !p1Cell->IsInteriorCell();
			// Interior/Invalid -> Exterior or Exterior/Invalid to Interior.
			bool diffCellType = 
			{
				(newIsExterior && (!currentCell || currentCell->IsInteriorCell())) ||
				(!newIsExterior && (!currentCell || !currentCell->IsInteriorCell()))
			};

			currentCell = p1Cell;
			exteriorCell = newIsExterior;
			ResetFadeOnObjects();
			// Ensure all objects in the new cell are fully faded in.
			Util::ResetFadeOnAllObjectsInCell(currentCell);
			// Set new default orientation when the cell type changes.
			if (diffCellType)
			{
				ResetCamData();
			}

			// Some interior cells have fog and a max zoom out distance for the camera
			// before checking if points are on screen fail (messes with auto-zoom).
			// Switching to skybox only sometimes circumvents this issue 
			// and allows for uncapped zooming out in interior cells.
			// The alternative - disabling the sky altogether - has some clear undesirable side effects 
			// like creating much harsher lighting throughout the cell.
			Util::SetSkyboxOnlyForInteriorModeCell(currentCell);
		}
	}

	void CameraManager::UpdatePlayerFadeAmounts(bool&& a_reset)
	{
		// Fade in/out players gradually as they move away from or too close to the camera.

		for (const auto& p : glob.coopPlayers)
		{
			if (p->isActive)
			{
				if (a_reset)
				{
					if (auto currentProc = p->coopActor->currentProcess; currentProc && currentProc->high) 
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

					if (auto player3D = Util::GetRefr3D(p->coopActor.get()); player3D)
					{
						player3D->fadeAmount = 1.0f;
						player3D->flags.reset(RE::NiAVObject::Flag::kHidden);
						RE::NiUpdateData updateData;
						player3D->UpdateDownwardPass(updateData, 0);
					}
				}
				else
				{
					const RE::NiPoint3& camPos = camTargetPos;
					// Prevent player from fading when the camera is far enough away to not require fading.
					if (auto player3D = Util::GetRefr3D(p->coopActor.get()); player3D)
					{
						float prevFadeAmount = player3D->fadeAmount;
						float newFadeAmount = prevFadeAmount;
						float maxXYExtent = (p->coopActor->GetBoundMax() - p->coopActor->GetBoundMin()).Length() / 2.0f;
						if (maxXYExtent == 0.0f)
						{
							maxXYExtent = player3D->worldBound.radius;
						}

						float torsoDistToCam = Util::GetTorsoPosition(p->coopActor.get()).GetDistance(camPos);
						if (camPos.Length() != 0.0f && torsoDistToCam < maxXYExtent)
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

						if (newFadeAmount != prevFadeAmount)
						{
							player3D->fadeAmount = newFadeAmount;
							RE::NiUpdateData updateData;
							player3D->UpdateDownwardPass(updateData, 0);
						}
					}
				}
			}
		}
	}
}

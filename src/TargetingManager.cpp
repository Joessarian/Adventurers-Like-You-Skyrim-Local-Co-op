#include "TargetingManager.h"
#include <Compatibility.h>
#include <DebugAPI.h>
#include <GlobalCoopData.h>
#include <Settings.h>
#include <Util.h>
#include <valarray>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	TargetingManager::TargetingManager() : 
		Manager(ManagerType::kTM)
	{ }

	void TargetingManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p->controllerID > -1 && a_p->controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			SPDLOG_DEBUG
			(
				"[TM] Initialize: Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count()
			);
			RefreshData();
		}
		else
		{
			SPDLOG_ERROR
			(
				"[TM] ERR: Initialize: Cannot construct Targeting Manager for controller ID {}.",
				a_p ? a_p->controllerID : -1
			);
		}
	}

	void TargetingManager::MainTask()
	{
		// Update crosshair position and selection first, 
		// and draw the crosshair, player indicator, and aim pitch indicator
		// if no fullscreen menu is open or not controlling menus.
		UpdateTargetingOverlay();
		// Select or clear the aim correction target if aim correction is enabled.
		UpdateAimCorrectionTarget();
		// Update target motion state next once a crosshair target or aim correction target
		// have been selected or cleared.
		UpdateTargetedRefrMotionState();
		// Open/close QuickLoot menu if the mod is installed and if targeting a valid refr.
		HandleQuickLootMenu();
		// Update the player's detection state and award Sneak skill XP as necessary.
		UpdateSneakState();
		// Update the player's crosshair text entry with externally-requested
		// or periodic information.
		UpdateCrosshairMessage();
		// Handle grabbed reference motion and positioning.
		// Done here because the crosshair target and motion state must be updated first
		// if throwing any grabbed references.
		HandleReferenceManipulation();
		// Draw projected trajectories for any projectile-launching attacks
		// last after all data relevant to computing trajectories is available.
		DrawTrajectories();
	}

	void TargetingManager::PrePauseTask()
	{
		auto ui = RE::UI::GetSingleton();
		SPDLOG_DEBUG
		(
			"[TM] PrePauseTask: P{}. Self invalid: {}. Grabbed refr info list size: {}.",
			playerID + 1, !p->selfValid, rmm->grabbedRefrInfoList.size()
		);
		if (nextState == ManagerState::kAwaitingRefresh)
		{
			// Clear all targets.
			ClearTargetHandles();
			// No longer selecting a crosshair target.
			validCrosshairRefrHit = false;
			rmm->ClearAll();
			// Reset crosshair position.
			ResetCrosshairPosition();
		}
		else
		{
			rmm->ClearReleasedRefrs();
		}
	}

	void TargetingManager::PreStartTask()
	{
		SPDLOG_DEBUG("[TM] PreStartTask: P{}", playerID + 1);
		// Reset TPs before starting.
		ResetTPs();
		// Deselect target and reset manipulated refrs/crosshair if data was refreshed.
		auto ui = RE::UI::GetSingleton();
		if (currentState == ManagerState::kAwaitingRefresh)
		{
			// Clear all targets.
			ClearTargetHandles();
			// No longer selecting a crosshair target.
			validCrosshairRefrHit = false;
			// Clear all grabbed and released refrs if a data refresh is required
			// to stop grabbing refrs and checking for released refr collisions.
			if (currentState == ManagerState::kAwaitingRefresh)
			{
				rmm->ClearAll();
			}

			// Reset crosshair position.
			ResetCrosshairPosition();
		}
		else
		{
			// Temporary solution until I figure out what triggers the 'character controller 
			// and 3D desync warp glitch', which occurs ~0.5 seconds after unpausing 
			// with a player previously grabbed.
			// Ragdolling fixes the issue, but I need to find a way to detect 
			// if this desync is happening  and correct it in the UpdateGrabbedReferences() call.
			// Solution: If grabbed by another player, release this player before resuming.
			rmm->ClearPlayerIfGrabbed(p);
			// Clear out any lingering released refrs.
			rmm->ClearReleasedRefrs();
		}

		// Clear out game crosshair pick refr too.
		if (p->isPlayer1)
		{
			Util::SendCrosshairEvent(nullptr);
		}
	}

	void TargetingManager::RefreshData()
	{
		// Player data.
		controllerID = p->controllerID;
		playerID = p->playerID;
		coopActor = p->coopActor;

		// Projectile manager.
		mph = std::make_unique<ManagedProjectileHandler>();
		// Grabbed/released object manipulation manager.
		rmm = std::make_unique<RefrManipulationManager>();
		// Motion state.
		targetMotionState = std::make_unique<RefrTargetMotionState>();
		// Crosshair text messages.
		crosshairMessage = std::make_unique<CrosshairMessage>(p->controllerID);
		extCrosshairMessage = std::make_unique<CrosshairMessage>(p->controllerID);
		lastCrosshairMessage = std::make_unique<CrosshairMessage>(p->controllerID);
		// UI element fade data.
		aimPitchIndicatorFadeInterpData = std::make_unique<TwoWayInterpData>();
		aimPitchIndicatorFadeInterpData->SetInterpInterval(0.25f, true);
		aimPitchIndicatorFadeInterpData->SetInterpInterval(0.5f, false);
		crosshairFadeInterpData = std::make_unique<TwoWayInterpData>();
		crosshairFadeInterpData->SetInterpInterval(0.5f, true);
		crosshairFadeInterpData->SetInterpInterval(1.0f, false);
		playerIndicatorFadeInterpData = std::make_unique<TwoWayInterpData>();
		playerIndicatorFadeInterpData->SetInterpInterval(1.0f, true);
		playerIndicatorFadeInterpData->SetInterpInterval(1.0f, false);
		// Crosshair interpolation data.
		crosshairOscillationData = std::make_unique<InterpolationData<float>>
		(
			0.0f, 0.0f, 0.0f, Settings::vfSecsToOscillateCrosshair[playerID]
		);
		crosshairRotationData = std::make_unique<InterpolationData<float>>
		(
			0.0f, 0.0f, 0.0f, Settings::vfSecsToRotateCrosshair[playerID]
		);

		// Target handles.
		// Clear all target handles, not just crosshair selection-related ones.
		ClearTargetHandles();

		// World positions.
		crosshairMovementHitPosOffset = 
		crosshairLocalPosOffset =
		crosshairWorldPos = 
		lastActivationReqPos = RE::NiPoint3();

		// Crosshair scaleform position.
		ResetCrosshairPosition();

		// Nearby refrs.
		nearbyObjectsOfSameType.clear();
		nearbyReferences.clear();
		// Bools.
		baseCanDrawOverlayElements = true;
		canActivateRefr = false;
		choseClosestResult = false;
		crosshairRefrInRangeForQuickLoot = false;
		crosshairRefrInSight = false;
		isMARFing = false;
		useProximityInteraction = false;
		validCrosshairRefrHit = false;
		// Floats.
		closestHostileActorDist = FLT_MAX;
		crosshairLocalPosPitchDiff = 0.0f;
		crosshairLocalPosYawDiff = 0.0f;
		crosshairSpeedMult = 1.0f;
		detectionPct = 100.0f;
		grabbedRefrDistanceOffset = 0.0f;
		lastActivationFacingAngle = coopActor->GetHeading(false);
		// Reach set to twice the actor's height initially.
		maxReachActivationDist = coopActor->GetHeight() * 2.0f;
		// Reset durations.
		secsSinceLastStealthStateCheck = 
		secsSinceTargetVisibilityLost = 
		secsSinceVisibleOnScreenCheck = 0.0f;
		// Ints.
		// 100% percent detection corresponds to green.
		detectionPctRGB = 0x00FF00;

		// Lastly, set player's pitch angle to 0, so that their pitch
		// angle since the last session was active does not carry over.
		coopActor->data.angle.x = 0.0f;
		// Reset all target handles, related data, and time points.
		ResetTargeting();
		ResetTPs();
		SPDLOG_DEBUG("[TM] RefreshData: {}.", coopActor ? coopActor->GetName() : "NONE");
	}

	const ManagerState TargetingManager::ShouldSelfPause()
	{
		// Suspension triggered externally.
		return currentState;
	}

	const ManagerState TargetingManager::ShouldSelfResume()
	{
		// Resumption triggered externally.
		return currentState;
	}

	void TargetingManager::ClearTarget(const TargetActorType& a_targetType)
	{
		// Clear the actor target handle that corresponds to the given target type.

		{
			std::unique_lock<std::mutex> targetingLock(targetingMutex, std::try_to_lock);
			if (targetingLock)
			{
				SPDLOG_DEBUG
				(
					"[TM] ClearTarget: {}: Lock obtained. (0x{:X})",
					coopActor->GetName(), std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				if (a_targetType == TargetActorType::kAimCorrection)
				{
					auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle
					(
						aimCorrectionTargetHandle
					); 
					if (aimCorrectionTargetPtr && aimCorrectionTargetPtr.get()) 
					{
						aimCorrectionTargetHandle.reset();
					}
				}
				else if (a_targetType == TargetActorType::kLinkedRefr)
				{
					auto aimTargetLinkedRefrPtr = Util::GetRefrPtrFromHandle
					(
						aimTargetLinkedRefrHandle
					); 
					if (aimTargetLinkedRefrPtr && aimTargetLinkedRefrPtr.get())
					{
						// Update linked refr using the aim target keyword first.
						if (p->aimTargetKeyword) 
						{
							coopActor->extraList.SetLinkedRef(nullptr, p->aimTargetKeyword);
						}

						aimTargetLinkedRefrHandle.reset();
					}
				}
				else if (a_targetType == TargetActorType::kSelected)
				{
					auto selectedTargetActorPtr = Util::GetActorPtrFromHandle
					(
						selectedTargetActorHandle
					); 
					if (selectedTargetActorPtr && selectedTargetActorPtr.get())
					{
						selectedTargetActorHandle.reset();
					}
				}
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[TM] ClearTarget: {}: Failed to obtain lock. (0x{:X})",
					coopActor->GetName(), std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}
	}

	void TargetingManager::DrawAimPitchIndicator()
	{
		// Draw the player's aim pitch adjustment indicator
		// when the player is adjusting their aim pitch 
		// or for a short time after the player's aim pitch resets.

		if (!Settings::vbDrawAimPitchIndicator[playerID])
		{
			return;
		}

		bool adjustingAimPitch = 
		(
			p->pam->IsPerforming(InputAction::kAdjustAimPitch) || 
			p->pam->GetSecondsSinceLastStop(InputAction::kResetAim) < 0.25f
		);
		aimPitchIndicatorFadeInterpData->UpdateInterpolatedValue
		(
			baseCanDrawOverlayElements && adjustingAimPitch
		);
		if (!adjustingAimPitch && 
			!aimPitchIndicatorFadeInterpData->interpToMax && 
			!aimPitchIndicatorFadeInterpData->interpToMin)
		{
			return;
		}

		// Max of:
		// 1/4 the height of the arrow head.
		// 2 * last point's Y coordinate - (origin) first point's Y coordinate.
		const float& thickness = max
		(
			2.0f,
			0.25f * 
			fabsf
			(
				GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS
				[GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS.size() - 1].y -
				GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS[0].y
			)
		);
		RE::NiPoint3 eyePos = Util::GetEyePosition(coopActor.get());
		RE::NiPoint3 eyePosScreenPoint = Util::WorldToScreenPoint3(eyePos);
		RE::NiPoint3 arrowHeadScreenPoint = Util::WorldToScreenPoint3(p->mm->aimPitchPos);
		RE::NiPoint3 deltaPos = (p->mm->aimPitchPos - eyePos);
		deltaPos.Unitize();
		if (rmm->isGrabbing)
		{
			// Add the grabbed refr offset to better show where grabbed objects will be suspended.
			float grabSuspensionOffset = max
			(
				0.0f, (p->mm->aimPitchPos - eyePos).Length() + grabbedRefrDistanceOffset
			);
			arrowHeadScreenPoint = Util::WorldToScreenPoint3
			(
				eyePos + 
				deltaPos * 
				grabSuspensionOffset
			);

			if (grabSuspensionOffset == 0.0f)
			{
				deltaPos = RE::NiPoint3();
			}
		}

		// Base of the arrow.
		const glm::vec2 eyeOrigin = glm::vec2(eyePosScreenPoint.x, eyePosScreenPoint.y);
		// Tip of the arrow.
		const glm::vec2 arrowHeadOrigin = glm::vec2
		(
			arrowHeadScreenPoint.x, arrowHeadScreenPoint.y
		);

		// All three shapes' offsets.
		auto outerShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS;
		auto middleShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_MID_PIXEL_OFFSETS;
		auto innerShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_INNER_PIXEL_OFFSETS;

		// Get the angle (in screenspace) by which to rotate the shape.
		// If length is 0, direct the arrow head straight up on screen.
		const glm::vec2 arrowHeadScreenDir = glm::normalize(arrowHeadOrigin - eyeOrigin);
		float angToRotate = 
		(
			glm::length(arrowHeadOrigin - eyeOrigin) == 0.0f ?
			PI / 2.0f:
			-atan2f(arrowHeadScreenDir.y, arrowHeadScreenDir.x)
		);

		// Rotate to point in the player's facing direction.
		DebugAPI::RotateOffsetPoints2D(outerShapeOffsets, angToRotate);
		DebugAPI::RotateOffsetPoints2D(middleShapeOffsets, angToRotate);
		DebugAPI::RotateOffsetPoints2D(innerShapeOffsets, angToRotate);

		// Draw each shape and their outlines.
		uint8_t alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value *
			static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
		);

		// Line portion of the arrow.
		DebugAPI::QueueLine2D
		(
			eyeOrigin, 
			arrowHeadOrigin, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			3.0f * thickness
		);
		DebugAPI::QueueLine2D
		(
			eyeOrigin, 
			arrowHeadOrigin, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			thickness
		);
			
		// Outer shape.
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			outerShapeOffsets, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);

		// Middle shape.
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			middleShapeOffsets, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);

		// Inner shape.
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			innerShapeOffsets, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);
	}

	void TargetingManager::DrawCrosshair()
	{
		// Draw crosshair lines and outlines based on 
		// if the crosshair is targeting a valid reference.

		// Update fade value if inactive crosshair fading is enabled.
		if (Settings::vbFadeInactiveCrosshair[playerID])
		{
			float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
			// Active if locked on target, moving, facing a target,
			// or if not in the process of being re-centered while inactive.
			// Allow 1x inactive interval while static + 0.5x an inactive interval
			// while auto-recentering to elapse before fading out.
			bool isCrosshairActive = 
			{
				Util::GetRefrPtrFromHandle(crosshairRefrHandle) || 
				p->pam->IsPerforming(InputAction::kMoveCrosshair) || 
				p->mm->reqFaceTarget || 
				secsSinceActive <= 1.5f * Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
			};
			crosshairFadeInterpData->UpdateInterpolatedValue
			(
				baseCanDrawOverlayElements && isCrosshairActive
			);
		}

		if (Settings::vbSkyrimStyleCrosshair[playerID]) 
		{
			// Draw a Skyrim-style pronged crosshair.
			DrawSkyrimStyleCrosshair();
		}
		else
		{
			// Draw a retro-style crosshair with four lines for prongs.
			// Draw lines first.
			DrawCrosshairLines();
			// First, inner outline.
			DrawCrosshairOutline(1.0f, Settings::vuCrosshairInnerOutlineRGBAValues[playerID]);
			// Second, outer outline if the crosshair raycast check selected a valid object.
			if (validCrosshairRefrHit)
			{
				DrawCrosshairOutline(2.0f, Settings::vuCrosshairOuterOutlineRGBAValues[playerID]);
			}
		}
	}

	void TargetingManager::DrawCrosshairLines()
	{
		// Draw the main four lines of the crosshair using the player's assigned crosshair color 
		// and size params.

		// When facing a target, rotate all lines 45 degrees.
		float angToRotate = p->mm->reqFaceTarget ? -PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate the mode change rotation and contraction/expansion if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}

		const float& crosshairLength = Settings::vfCrosshairLength[playerID];
		const float& crosshairGap = Settings::vfCrosshairGapRadius[playerID] + gapDelta;
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];
		// Draw crosshair lines.
		// '+' shape when not facing a target, 'X' shape otherwise.
		// Pairs of 2D line start and end points.
		std::pair<glm::vec2, glm::vec2> crosshairUp = 
		{
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y + crosshairGap),
			glm::vec2
			(
				crosshairScaleformPos.x, crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairDown = 
		{
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap),
			glm::vec2
			(
				crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairLeft = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairRight = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y
			)
		};

		if (angToRotate != 0.0f)
		{
			// Rotate all crosshair line segments by 45 degrees.
			DebugAPI::RotateLine2D(crosshairUp, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(crosshairDown, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(crosshairLeft, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(crosshairRight, crosshairScaleformPos, angToRotate);
		}

		// Use interped fade value if enabled; otherwise, use the player's static fade value.
		uint8_t alpha = 
		(
			Settings::vbFadeInactiveCrosshair[playerID] ?
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			) :
			0xFF
		);
		// Up.
		DebugAPI::QueueLine2D
		(
			crosshairUp.first, 
			crosshairUp.second, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Down.
		DebugAPI::QueueLine2D
		(
			crosshairDown.first,
			crosshairDown.second, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Left.
		DebugAPI::QueueLine2D
		(
			crosshairLeft.first,
			crosshairLeft.second, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Right.
		DebugAPI::QueueLine2D
		(
			crosshairRight.first,
			crosshairRight.second, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
			crosshairThickness
		);

		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 25.0f))
		{
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				64, 
				2.0f * crosshairThickness + crosshairGap + crosshairLength, 
				2.0f * crosshairThickness
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				0xFFFFFF00 + alpha, 
				64,
				4.0f * crosshairThickness + crosshairGap + crosshairLength,
				2.0f * crosshairThickness
			);
		}
	}

	void TargetingManager::DrawCrosshairOutline
	(
		float&& a_outlineIndex, const uint32_t& a_outlineRGBA
	)
	{
		// Always outline the four main crosshair lines 
		// and also outline that inner outline with another outline
		// when a valid object is selected by the player's crosshair.
		// Outline index is a whole number value that indicates 
		// the multiple of crosshair thicknesses
		// from the center four lines at which to draw the outline.
		// The higher the index, the further from the crosshair base lines
		// the outline will be drawn.

		// Rotate the outlines when facing a target to match the rotation of the crosshair body.
		float angToRotate = p->mm->reqFaceTarget ? -PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate the rotation, contraction, and expansion, if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}
		
		// Must be a whole number to prevent overlap.
		a_outlineIndex = floorf(a_outlineIndex);
		float crosshairLength = Settings::vfCrosshairLength[playerID];
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];
		// Offset from crosshair body lines.
		const float outlineThicknessOffset = crosshairThickness * a_outlineIndex;
		// Longer than crosshair body lines and inner outlines.
		crosshairLength += 2.0f * outlineThicknessOffset;
		// Add gap delta and thickness offset.
		const float crosshairGap = 
		(
			Settings::vfCrosshairGapRadius[playerID] - outlineThicknessOffset + gapDelta
		);

		// Pairs of 2D line start and end coordinates.
		// 
		// Up outlines for each prong.
		std::pair<glm::vec2, glm::vec2> up1 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset, 
				crosshairScaleformPos.y + crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset,
				crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> up2 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset, 
				crosshairScaleformPos.y + crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset,
				crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> up3 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset, 
				crosshairScaleformPos.y + crosshairGap + crosshairLength
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset, 
				crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> up4 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset, 
				crosshairScaleformPos.y + crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset,
				crosshairScaleformPos.y + crosshairGap
			)
		};

		// Down outlines for each prong.
		std::pair<glm::vec2, glm::vec2> down1 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset,
				crosshairScaleformPos.y - crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset, 
				crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> down2 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset,
				crosshairScaleformPos.y - crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset,
				crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> down3 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset, 
				crosshairScaleformPos.y - crosshairGap - crosshairLength
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset, 
				crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> down4 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + outlineThicknessOffset, 
				crosshairScaleformPos.y - crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x - outlineThicknessOffset, 
				crosshairScaleformPos.y - crosshairGap
			)
		};

		// Left outlines for each prong.
		std::pair<glm::vec2, glm::vec2> left1 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap,
				crosshairScaleformPos.y - outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength,
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> left2 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap, 
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength, 
				crosshairScaleformPos.y + outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> left3 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength,
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength,
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> left4 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap,
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap, 
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};

		// Right outlines for each prong.
		std::pair<glm::vec2, glm::vec2> right1 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap, 
				crosshairScaleformPos.y - outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength, 
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> right2 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap, 
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength, 
				crosshairScaleformPos.y + outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> right3 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength,
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength,
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};
		std::pair<glm::vec2, glm::vec2> right4 = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap, 
				crosshairScaleformPos.y + outlineThicknessOffset
			),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap,
				crosshairScaleformPos.y - outlineThicknessOffset
			)
		};

		// Rotate if facing crosshair target.
		if (angToRotate != 0.0f)
		{
			DebugAPI::RotateLine2D(up1, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(up2, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(up3, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(up4, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(down1, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(down2, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(down3, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(down4, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(left1, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(left2, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(left3, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(left4, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(right1, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(right2, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(right3, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(right4, crosshairScaleformPos, angToRotate);
		}

		// Use interped fade value if enabled; otherwise, use the player's static fade value.
		uint8_t alpha = 
		(
			Settings::vbFadeInactiveCrosshair[playerID] ?
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * static_cast<float>(a_outlineRGBA & 0xFF)
			) :
			0xFF
		);

		// Up
		DebugAPI::QueueLine2D
		(
			up1.first, up1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			up2.first, up2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			up3.first, up3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			up4.first, up4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);

		// Down.
		DebugAPI::QueueLine2D
		(
			down1.first, down1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			down2.first, down2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			down3.first, down3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			down4.first, down4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);

		// Left.
		DebugAPI::QueueLine2D
		(
			left1.first, left1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			left2.first, left2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			left3.first, left3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			left4.first, left4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);

		// Right.
		DebugAPI::QueueLine2D
		(
			right1.first, right1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			right2.first, right2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			right3.first, right3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
		DebugAPI::QueueLine2D
		(
			right4.first, right4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness
		);
	}
	
	void TargetingManager::DrawPlayerIndicator()
	{
		// Draw a quest marker above the player's head 
		// when player-specific visibility conditions are met.

		const auto& visibilityType = Settings::vuPlayerIndicatorVisibilityType[playerID];
		// If the indicator is disabled, no need to draw. Bye.
		if (visibilityType == !PlayerIndicatorVisibilityType::kDisabled) 
		{
			return;
		}

		// If the player is not on screen, 
		// draw the player indicator pointed at the player's position.
		// If on screen, draw player indicator above their head when:
		// - 'Always draw' setting is set -OR-
		// - There is no LOS to them -OR-
		// - Their pixel height is below a certain threshold.

		// Get viewport dimensions for the overlay menu.
		const auto port = Util::GetPort();
		// Check that the player's center is visible.
		RE::NiPoint3 posScreenCoords;
		bool onScreen = Util::PointIsOnScreen
		(
			Util::GetTorsoPosition(coopActor.get()), posScreenCoords
		);
		// Ensure two outlines can fit inside outermost outline.
		// Scale with player's pixel height and bound above and below.
		float indicatorBaseLength = Settings::vfPlayerIndicatorLength[playerID];
		const float& indicatorBaseThickness = Settings::vfPlayerIndicatorThickness[playerID];
		float playerPixelHeight = Util::GetBoundPixelDist(coopActor.get(), true);
		playerPixelHeight = playerPixelHeight == 0.0f ? indicatorBaseLength : playerPixelHeight;
		// Lower/upper bound are the smaller/larger of:
		// 4 indicator thicknesses or player pixel height scaled down by a factor.
		indicatorBaseLength = std::clamp
		(
			indicatorBaseLength,
			min
			(
				max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), 
				playerPixelHeight / 4.0f
			),
			max
			(
				max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), 
				playerPixelHeight / 4.0f
			)
		);
		// Scaling factor used to scale up/down hardcoded point offsets for the indicator's shape.
		float scalingFactor = indicatorBaseLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
		float indicatorLength = indicatorBaseLength * scalingFactor;
		float indicatorThickness = indicatorBaseThickness * scalingFactor;
		if (onScreen)
		{
			auto playerCam = RE::PlayerCamera::GetSingleton();
			bool shouldDraw = 
			(
				baseCanDrawOverlayElements && 
				playerCam &&
				playerCam->cameraRoot && 
				playerCam->cameraRoot.get()
			);
			if (shouldDraw) 
			{
				bool falseRef = false;
				const auto& camPos = 
				(
					glob.cam->IsRunning() ? 
					glob.cam->camTargetPos : 
					playerCam->cameraRoot->world.translate
				);
				const auto playerTorsoPos = Util::GetTorsoPosition(coopActor.get());

				// Condition for raycasting to check for LOS from cam to player:
				// 1. Low visibility mode is set,
				// 2. Not downed,
				// 3. The player's height is more than 1/15 of the screen's height.
				// 4. The player's torso is on screen.
				// Otherwise, no raycast is needed and the indicator will be drawn.
				bool shouldRaycastForLOS = 
				{ 
					visibilityType == !PlayerIndicatorVisibilityType::kLowVisibility && 
					!p->isDowned &&						 
					Util::GetBoundPixelDist(coopActor.get(), true) >= 
					DebugAPI::screenResY / 15.0f &&
					Util::PointIsOnScreen(playerTorsoPos) 
				};

				// If checking raycast LOS, and the player is not visible, draw the indicator.
				bool hasLOS = true;
				if (shouldRaycastForLOS)
				{
					// From cam to player torso.
					// If there is LOS, the cast will either hit nothing or the player.
					auto result = Raycast::hkpCastRay
					(
						{ camPos.x, camPos.y, camPos.z, 0.0f }, 
						{ playerTorsoPos.x, playerTorsoPos.y, playerTorsoPos.z, 0.0f }, 
						std::vector<RE::NiAVObject*>({ playerCam->cameraRoot.get() }), 
						RE::COL_LAYER::kLOS
					);
					auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
					// No hit or if the player is hit means that there is LOS on the player.
					hasLOS = 
					(
						(!result.hitObjectPtr || !result.hitObjectPtr.get()) || 
						(hitRefrPtr && hitRefrPtr.get() == coopActor.get())
					);
				}

				shouldDraw = !shouldRaycastForLOS || !hasLOS;
			}

			// Update fade value before fading in/out.
			// Will continue fading in/out until done even if the draw condition is not met.
			playerIndicatorFadeInterpData->UpdateInterpolatedValue(shouldDraw);
			if (shouldDraw || 
				playerIndicatorFadeInterpData->interpToMax || 
				playerIndicatorFadeInterpData->interpToMin) 
			{
				// Point facing downward above the player's head.
				posScreenCoords = Util::WorldToScreenPoint3
				(
					Util::GetHeadPosition(coopActor.get()) + 
					RE::NiPoint3(0.0f, 0.0f, 0.25f * coopActor->GetHeight())
				);
				// Origin and lower/upper shape offsets from this origin.
				glm::vec2 origin
				{
					std::clamp
					(
						posScreenCoords.x, 
						port.left + indicatorThickness, 
						port.right - indicatorThickness
					),
					std::clamp
					(
						posScreenCoords.y, 
						port.top + indicatorThickness,
						port.bottom - indicatorThickness
					)
				};

				auto upperPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS;
				auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;

				// Scale offsets one again.
				for (auto& offset : upperPortionOffsets)
				{
					offset *= scalingFactor;
				}

				for (auto& offset : lowerPortionOffsets)
				{
					offset *= scalingFactor;
				}

				// Draw each shape and their outlines.
				uint8_t alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				);
				// Lower portion.
				DebugAPI::QueueShape2D
				(
					origin, 
					lowerPortionOffsets, 
					(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false, 
					indicatorThickness
				);
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				);
				DebugAPI::QueueShape2D
				(
					origin, 
					lowerPortionOffsets, 
					(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
				);

				// Upper portion.
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value *
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				);
				DebugAPI::QueueShape2D
				(
					origin,
					upperPortionOffsets,
					(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false,
					indicatorThickness
				);
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value *
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				);
				DebugAPI::QueueShape2D
				(
					origin,
					upperPortionOffsets, 
					(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
				);
			}
		}
		else
		{
			// Fade in/out.
			playerIndicatorFadeInterpData->UpdateInterpolatedValue(baseCanDrawOverlayElements);
			// Default indicator orientation is bottom half facing down,
			// which is also the default Skyrim quest marker orientation.
			float indicatorRotRads = 0.0f;
			if (posScreenCoords.x == port.right && posScreenCoords.y == port.top)
			{
				// Top right corner.
				indicatorRotRads = 3.0f * PI / 4.0f;
			}
			else if (posScreenCoords.x == port.left && posScreenCoords.y == port.top)
			{
				// Top left corner.
				indicatorRotRads = -3.0f * PI / 4.0f;
			}
			else if (posScreenCoords.x == port.left && posScreenCoords.y == port.bottom)
			{
				// Bottom left corner.
				indicatorRotRads = -PI / 4.0f;
			}
			else if (posScreenCoords.x == port.right && posScreenCoords.y == port.bottom)
			{
				// Bottom right corner.
				indicatorRotRads = PI / 4.0f;
			}
			else if (posScreenCoords.y == port.top)
			{
				// Top edge of the screen.
				indicatorRotRads = PI;
			}
			else if (posScreenCoords.x == port.left)
			{
				// Left edge of the screen.
				indicatorRotRads = -PI / 2.0f;
			}
			else if (posScreenCoords.y == port.bottom)
			{
				// Bottom edge of the screen.
				indicatorRotRads = 0.0f;
			}
			else if (posScreenCoords.x == port.right)
			{
				// Right edge of the screen.
				indicatorRotRads = PI / 2.0f;
			}

			// 2D point offsets for the indicator's upper/lower portion shapes.
			auto upperPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS;
			auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;

			// Scale both by scaling factor defined above.
			for (auto& offset : upperPortionOffsets)
			{
				offset *= scalingFactor;
			}

			for (auto& offset : lowerPortionOffsets)
			{
				offset *= scalingFactor;
			}

			// Origin with respect to which all the above offset points 
			// are traced out when drawing the shape.
			glm::vec2 origin
			{
				std::clamp
				(
					posScreenCoords.x,
					port.left + indicatorThickness, 
					port.right - indicatorThickness
				),
				std::clamp
				(
					posScreenCoords.y, 
					port.top + indicatorThickness, 
					port.bottom - indicatorThickness
				)
			};

			// Rotate and draw lower portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, indicatorRotRads);
			uint8_t alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				lowerPortionOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				false, 
				indicatorThickness
			);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				lowerPortionOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);

			// Rotate and draw upper portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, indicatorRotRads);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				upperPortionOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				false, 
				indicatorThickness
			);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				upperPortionOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);
		}
	}

	void TargetingManager::DrawSkyrimStyleCrosshair()
	{
		// Draw a Skyrim-style crosshair with a player-specific colorway.

		// Rotate 45 degrees when facing a target.
		float angToRotate = p->mm->reqFaceTarget ? -PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate rotation, contraction, and expansion, if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}

		// Center at the crosshair position.
		const auto origin = glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y);
		const float& crosshairLength = Settings::vfCrosshairLength[playerID];
		const float& crosshairGap = Settings::vfCrosshairGapRadius[playerID] + gapDelta;
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];

		// Points are offset to the right of the origin (+X Scaleform axis).
		std::vector<glm::vec2> defProngOffsets = GlobalCoopData::CROSSHAIR_PRONG_PIXEL_OFFSETS;
		std::vector<glm::vec2> baseProngOffsets = defProngOffsets;
		std::vector<glm::vec2> prongOffsets = defProngOffsets;
		std::vector<glm::vec2> prongRotatedOffsets = defProngOffsets;
		// Factor with which to scale the shape offset points and shape dimensions
		// relative to the default prong length.
		// Match the player's chosen crosshair length.
		float scalingFactor = crosshairLength / GlobalCoopData::CROSSHAIR_PRONG_DEF_LENGTH;
		for (auto& coord : baseProngOffsets)
		{
			coord *= scalingFactor;
		}

		// Draw outer, then inner outline of the prong, then the prong itself.
		uint8_t alpha = 0xFF;
		// Outer outline only drawn when the crosshair is over a valid object.
		if (validCrosshairRefrHit)
		{
			// [Outer outline]
			prongOffsets = baseProngOffsets;
			for (auto& coord : prongOffsets)
			{
				coord.x += crosshairGap;
			}

			// Four prongs.
			for (uint8_t i = 0; i < 4; ++i)
			{
				// 90 degrees between each prong.
				// Don't rotate the first prong.
				if (i > 0)
				{
					DebugAPI::RotateOffsetPoints2D(prongOffsets, PI / 2.0f);
				}

				prongRotatedOffsets = prongOffsets;
				// Rotate through the additional face-target angle offset.
				DebugAPI::RotateOffsetPoints2D(prongRotatedOffsets, angToRotate);
				// Interped fade value or full alpha.
				alpha = 
				(
					Settings::vbFadeInactiveCrosshair[playerID] ?
					static_cast<uint8_t>
					(
						crosshairFadeInterpData->value * 
						static_cast<float>
						(
							Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
						)
					) :
					0xFF 
				);
				DebugAPI::QueueShape2D
				(
					origin,
					prongRotatedOffsets, 
					(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false, 
					2.0f * crosshairThickness
				);
			}
		}

		// [Inner outline]
		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += crosshairGap;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
			// Don't rotate the first prong.
			if (i > 0)
			{
				DebugAPI::RotateOffsetPoints2D(prongOffsets, PI / 2.0f);
			}

			prongRotatedOffsets = prongOffsets;
			// Rotate through the additional face-target angle offset.
			DebugAPI::RotateOffsetPoints2D(prongRotatedOffsets, angToRotate);
			// Interped fade value or full alpha.
			alpha = 
			(
				Settings::vbFadeInactiveCrosshair[playerID] ?
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				) :
				0xFF
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				prongRotatedOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				false, 
				crosshairThickness
			);
		}

		// [Prong]
		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += crosshairGap;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
			// Don't rotate the first prong.
			if (i > 0)
			{
				DebugAPI::RotateOffsetPoints2D(prongOffsets, PI / 2.0f);
			}

			prongRotatedOffsets = prongOffsets;
			// Rotate through the additional face-target angle offset.
			DebugAPI::RotateOffsetPoints2D(prongRotatedOffsets, angToRotate);
			// Interped fade value or full alpha.
			alpha = 
			(
				Settings::vbFadeInactiveCrosshair[playerID] ?
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				) :
				0xFF
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				prongRotatedOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);
		}


		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 25.0f))
		{
			alpha = 
			(
				Settings::vbFadeInactiveCrosshair[playerID] ? 
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				) :
				0xFF
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				64, 
				2.0f * crosshairThickness + crosshairGap + crosshairLength,
				2.0f * crosshairThickness
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				0xFFFFFF00 + alpha, 
				64, 
				4.0f * crosshairThickness + crosshairGap + crosshairLength,
				2.0f * crosshairThickness
			);
		}
	}

	void TargetingManager::DrawTrajectories()
	{
		// Draw trajectories for projectiles that the player is attempting to release.

		if (!Settings::vbDrawPredictedProjectileTrajectoryCurves[p->playerID])
		{
			return;
		}

		//=======================
		// [Set Trajectory Type]:
		//=======================

		// Get potential trajectory type first.
		ProjectileTrajType trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[p->playerID]
		);
		auto targetActorHandle = GetRangedTargetActor();
		auto targetActorPtr = Util::GetRefrPtrFromHandle(targetActorHandle);
		bool targetActorValidity = 
		(
			targetActorPtr && 
			targetActorPtr.get() &&
			Util::IsValidRefrForTargeting(targetActorPtr.get())
		);
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		bool crosshairRefrValidity = 
		(
			crosshairRefrPtr && 
			crosshairRefrPtr.get() &&
			Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
		);
		// Actor targeted (aim correction or otherwise), 
		// should face crosshair position (never true while mounted), 
		// or mounted and targeting an object.
		bool adjustTowardsTarget = 
		{
			(targetActorPtr != coopActor) &&
			(
				(targetActorValidity || p->mm->reqFaceTarget) || 
				(coopActor->IsOnMount() && crosshairRefrValidity)
			)
		};
		// Aim in the player's facing direction if there is no target.
		if (!adjustTowardsTarget)
		{
			trajType = ProjectileTrajType::kAimDirection;
		}

		//======================
		// [Bows and Crossbows]:
		//======================
		
		const auto& meleeAttackState = coopActor->actorState1.meleeAttackState;
		bool aimingWithRangedWeap = false;
		if (p->em->HasBowEquipped())
		{
			// Avoid the 'kBowDraw' portion of the bow drawing process, 
			// since the arrow is not fully pulled from its quiver.
			aimingWithRangedWeap = 
			(
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
			);

		}
		else if (p->em->HasCrossbowEquipped())
		{
			// Avoid the 'kBowAttached' portion of the crossbow firing process,
			// since this state is set after firing the crossbow and when loading another bolt.
			aimingWithRangedWeap = 
			(
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
			);
		}

		if (aimingWithRangedWeap)
		{
			auto ammoForm = p->em->equippedForms[!EquipIndex::kAmmo];
			if (ammoForm)
			{
				auto ammo = ammoForm->As<RE::TESAmmo>();
				if (ammo && ammo->data.projectile)
				{
					auto baseProj = ammo->data.projectile;
					// Special case for beam/flame projectiles:
					// aim in the player's facing direction.
					if (baseProj->data.types.any
						(
							RE::BGSProjectileData::Type::kBeam,
							RE::BGSProjectileData::Type::kFlamethrower
						))
					{
						trajType = ProjectileTrajType::kAimDirection;
					}
					const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
					(
						p,
						baseProj,
						p->em->GetRHWeapon(),
						nullptr, 
						p->mm->playerAttackSourcePos,
						trajType
					);
					
					DrawTrajectory
					(
						trajInfo->releasePos,
						trajInfo->trajectoryEndPos,
						trajInfo->initialTrajTimeToTarget,
						trajInfo->releaseSpeed,
						trajInfo->launchPitch,
						trajInfo->launchYaw,
						trajInfo->g,
						trajInfo->mu,
						baseProj->data.range,
						trajInfo->trajType,
						trajInfo->canReachTarget,
						true,
						false
					);
				}
			}
		}

		//========================
		// [Spellcast Projectiles:
		//========================

		// Left hand spell.
		auto lhSpell = p->em->GetLHSpell(); 
		auto lhSpellDelivery = 
		(
			lhSpell ? lhSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto lhCaster = coopActor->magicCasters[RE::Actor::SlotTypes::kLeftHand];
		// Caster loaded up with a targeted spell and actively casting.
		bool canDrawLHSpellTraj = 
		(
			lhSpell && 
			lhSpell->avEffectSetting && 
			lhSpell->avEffectSetting->data.projectileBase &&
			lhSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			lhCaster && 
			lhCaster->state != RE::MagicCaster::State::kNone
		);
		if (canDrawLHSpellTraj)
		{
			auto baseProj = lhSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Release from left hand node, or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (lhCaster->magicNode) 
			{
				releasePos = lhCaster->magicNode->world.translate;
			}
			else
			{
				auto leftHandNodePtr = Util::Get3DObjectByName
				(
					coopActor.get(), "NPC L Hand [LHnd]"
				); 
				if (leftHandNodePtr && leftHandNodePtr.get())
				{
					releasePos = leftHandNodePtr->world.translate;
				}
			}
			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				lhSpell->avEffectSetting, 
				releasePos,
				trajType
			);

			DrawTrajectory
			(
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		// Right hand spell.
		auto rhSpell = p->em->GetRHSpell(); 
		auto rhSpellDelivery = 
		(
			rhSpell ? rhSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto rhCaster = coopActor->magicCasters[RE::Actor::SlotTypes::kRightHand];
		// Caster loaded up with a targeted spell and actively casting.
		bool canDrawRHSpellTraj = 
		(
			rhSpell && 
			rhSpell->avEffectSetting && 
			rhSpell->avEffectSetting->data.projectileBase &&
			rhSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			rhCaster && 
			rhCaster->state != RE::MagicCaster::State::kNone
		);
		if (canDrawRHSpellTraj)
		{
			auto baseProj = rhSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}
			
			// Release from right hand node, or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (rhCaster->magicNode) 
			{
				releasePos = rhCaster->magicNode->world.translate;
			}
			else
			{
				auto rightHandNodePtr = Util::Get3DObjectByName
				(
					coopActor.get(), "NPC R Hand [RHnd]"
				); 
				if (rightHandNodePtr && rightHandNodePtr.get())
				{
					releasePos = rightHandNodePtr->world.translate;
				}
			}
			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				rhSpell->avEffectSetting, 
				releasePos,
				trajType
			);

			DrawTrajectory
			(
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		// Quick slot spell.
		auto qsSpell = p->em->quickSlotSpell; 
		auto qsSpellDelivery = 
		(
			qsSpell ? qsSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto instantCaster = coopActor->magicCasters[RE::Actor::SlotTypes::kPowerOrShout];
		// Instant caster loaded up with a targeted spell and the player is trying to cast.
		bool canDrawQSSpellTraj = 
		(
			qsSpell && 
			qsSpell->avEffectSetting && 
			qsSpell->avEffectSetting->data.projectileBase &&
			qsSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			instantCaster &&
			p->pam->IsPerforming(InputAction::kQuickSlotCast)
		);
		if (canDrawQSSpellTraj)
		{
			auto baseProj = qsSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Release from instant caster position or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (instantCaster->magicNode) 
			{
				releasePos = instantCaster->magicNode->world.translate;
			}
			else
			{
				auto strings = RE::FixedStrings::GetSingleton();
				if (strings)
				{
					auto headMagicNodePtr = Util::Get3DObjectByName
					(
						coopActor.get(), strings->npcHeadMagicNode
					); 
					if (headMagicNodePtr && headMagicNodePtr.get())
					{
						releasePos = headMagicNodePtr->world.translate;
					}
				}
			}
			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				qsSpell->avEffectSetting, 
				releasePos,
				trajType
			);

			DrawTrajectory
			(
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}


		//==========
		// [Shouts]:
		//==========

		// Shout variation or power spell.
		auto voiceSpell = p->em->voiceSpell; 
		auto voiceSpellDelivery = 
		(
			voiceSpell ? voiceSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		// Instant caster loaded up with a targeted shout/power spell 
		// and the player is trying to cast.
		bool canShoutSpellTraj = 
		(
			voiceSpell && 
			voiceSpell->avEffectSetting && 
			voiceSpell->avEffectSetting->data.projectileBase &&
			voiceSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			instantCaster &&
			p->pam->IsPerforming(InputAction::kShout)
		);
		if (canShoutSpellTraj)
		{
			auto baseProj = voiceSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Cast from instant caster node position or from the look at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (instantCaster->magicNode) 
			{
				releasePos = instantCaster->magicNode->world.translate;
			}
			else
			{
				auto strings = RE::FixedStrings::GetSingleton();
				if (strings)
				{
					auto headMagicNodePtr = Util::Get3DObjectByName
					(
						coopActor.get(), strings->npcHeadMagicNode
					); 
					if (headMagicNodePtr && headMagicNodePtr.get())
					{
						releasePos = headMagicNodePtr->world.translate;
					}
				}
			}
			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				voiceSpell->avEffectSetting, 
				releasePos,
				trajType
			);

			DrawTrajectory
			(
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		//===================
		// [Grabbed Objects]:
		//===================

		// Trying to throw a grabbed object at the crosshair target position.
		if (p->mm->reqFaceTarget &&
			rmm->isGrabbing && 
			rmm->GetNumGrabbedRefrs() > 0 && 
			p->pam->IsPerforming(InputAction::kGrabObject))
		{
			// IMPORTANT NOTE:
			// Drawing all grabbed refrs' projected release trajectories 
			// would be WAY too performance intensive and unnecessary,
			// since all refrs have the same release speed + launch angles on release.
			// The only difference is their release position, but drawing one trajectory
			// starting from the position of the first grabbed refr should provide
			// a helpful picture of how all the refrs will behave when thrown.

			// Need to have at least 1 valid grabbed refr.
			auto firstRefrHandle = 
			(
				rmm->grabbedRefrInfoList[0] && rmm->grabbedRefrInfoList[0]->IsValid() ?
				rmm->grabbedRefrInfoList[0]->refrHandle :
				RE::ObjectRefHandle()
			);
			if (!Util::HandleIsValid(firstRefrHandle))
			{
				return;
			}

			std::unique_ptr<ReleasedReferenceInfo> firstRefrInfo = 
			(
				std::make_unique<ReleasedReferenceInfo>
				(
					controllerID, firstRefrHandle
				)
			);
			if (!firstRefrInfo || !firstRefrInfo.get())
			{
				return;
			}

			// Update magicka overflow slowdown factor to use when determining the trajectory
			// of thrown refrs.
			float secsGrabHeldTime = 
			(
				p->pam->paStatesList
				[!InputAction::kGrabObject - !InputAction::kFirstAction].secsPerformed
			);
			magickaOverflowSlowdownFactor = 
			(
				GetThrownRefrMagickaOverflowSlowdownFactor
				(
					GetTotalThrownRefrMagickaCost(secsGrabHeldTime)
				)
			);

			// Populate trajectory data.
			firstRefrInfo->InitPreviewTrajectory(p);
			DrawTrajectory
			(
				firstRefrInfo->releasePos,
				firstRefrInfo->trajectoryEndPos,
				firstRefrInfo->initialTimeToTarget,
				firstRefrInfo->releaseSpeed,
				firstRefrInfo->launchPitch,
				firstRefrInfo->launchYaw,
				Util::GetGravitationalConstant(),
				Settings::fMu,
				FLT_MAX,
				firstRefrInfo->trajType,
				firstRefrInfo->canReachTarget,
				false,
				true,
				firstRefrInfo->refrHandle
			);
		}
	}

	void TargetingManager::DrawTrajectory
	(
		const RE::NiPoint3& a_releasePos, 
		const RE::NiPoint3& a_targetPos,
		const double& a_initialProjectedTimeToTarget,
		const double& a_releaseSpeed,
		const double& a_launchPitch, 
		const double& a_launchYaw,
		const double& a_g,
		const double& a_mu,
		const float& a_maxRange,
		const ProjectileTrajType& a_trajType,
		const bool& a_canReachTarget,
		bool a_isWeapMagProj,
		bool&& a_capVelocity,
		RE::ObjectRefHandle a_projHandle
	)
	{
		// Draw trajectory based on the given launch parameters.
		// Can handle weapon/magic projectiles, or thrown refrs.
		// Can choose to cap the velocity, and thus the displacement, per time slice.
		// Projected trajectory is most accurate when the target position
		// is close to the release position.
		// 
		// IMPORTANT NOTES:
		// 1. Framerate dependent -- the smoother the framerate, the more accurate the trajectory.
		// 2. The 'fixed' portions of the trajectory,
		// ex. the entirety of the 'Predictive' or 'Aim Direction' trajectories,
		// or the first portion of the 'Homing' trajectory before homing in,
		// will correspond almost perfectly to the actual trajectory 
		// that the released projectile takes.
		// 3. The more time slices, the smoother the curve, but the higher the performance hit
		// from drawing more line segments and performing more raycasts along the curve.
		// 4. For the homing-in part of the trajectory, the projected trajectory
		// will not perfectly conform to the released projectile's actual path 
		// and will tend to undershoot at close range
		// (start homing in earlier due to the time slice being smaller than the frame delta time),
		// and overshoot when aiming far away
		// (start homing in later due to the time slice being larger than the frame delta time).
		// 5. For a perfect 1-1 correspondence, the time slice would have to equal the frame time,
		// but this would lead to a HUGE hit in performance if the time-to-target
		// is large. For example, at 60 FPS and therefore 60 time slices a second, 
		// shooting at the sky will typically take 15+ seconds 
		// and therefore 900+ line segments queued and raycasts performed PER trajectory drawn. 
		// Not feasible.
		// 6. Collision check raycasts are not done with a hull size equal to the radius 
		// of the projectile for performance reasons.
		// The havok pick data cast is much faster than the camera sphere cast
		// with a customizable hull size.
		// However, this will result in the predicted trajectory not correctly showing collisions
		// with geometry if the drawn trajectory segment is within a radius-length of an obstacle.
		
		// Don't draw if the time to target is 0 
		// or if the release position is the same as the target position.
		if (a_initialProjectedTimeToTarget == 0.0f || a_releasePos == a_targetPos)
		{
			return;
		}
		
		// Hit the crosshair/ranged actor target when walking the curve.
		bool hitTarget = false;
		// Did the projectile hit another, non-target refr that is selectable?
		bool hitSelectableNonTargetRefr = false;
		// Should set a homing trajectory.
		// Should begin or continue homing in on the target position.
		bool shouldHomeIn = false;
		// Hit an object en-route to the target position.
		bool trajCollision = false;
		// Projectile and current crosshair target refrs.
		auto projRefrPtr = Util::GetRefrPtrFromHandle(a_projHandle);
		bool projRefrValidity = projRefrPtr && projRefrPtr.get();
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		bool crosshairRefrValidity = crosshairRefrPtr && crosshairRefrPtr.get();
		auto rangedTargetActorHandle = GetRangedTargetActor();
		bool rangedTargetActorValidity = Util::HandleIsValid(rangedTargetActorHandle);

		// Hit result for casts between time slice start and end positions.
		Raycast::RayResult result{ };
		// Ignore the firing player, the crosshair target, if selected, 
		// and the projectile itself, if given, when filtering through raycast collision results 
		// for each time slice.
		std::vector<RE::TESObjectREFR*> raycastExcludedRefrs{ coopActor.get() };
		if (projRefrValidity)
		{
			raycastExcludedRefrs.emplace_back(projRefrPtr.get());
		}

		// Insert all grabbed/thrown refrs if this is a grabbed/thrown refr.
		// Want to ignore collisions between them.
		if (!a_isWeapMagProj)
		{
			if (rmm->isGrabbing)
			{
				for (const auto& info : rmm->grabbedRefrInfoList)
				{
					if (!info->IsValid() || info->refrHandle == a_projHandle)
					{
						continue;
					}

					raycastExcludedRefrs.emplace_back(info->refrHandle.get().get());
				}
			}
			else
			{
				for (const auto& info : rmm->releasedRefrInfoList)
				{
					if (!info->IsValid() || info->refrHandle == a_projHandle)
					{
						continue;
					}

					raycastExcludedRefrs.emplace_back(info->refrHandle.get().get());
				}
			}
		}

		// Don't handle position/angle deltas below this value.
		const float epsilon = 1E-3f;
		// Initial XY, X, Y, and Z components of velocity at launch.
		const float initVelXY = a_releaseSpeed * cosf(a_launchPitch);
		const float initVelX = initVelXY * cosf(a_launchYaw);
		const float initVelY = initVelXY * sinf(a_launchYaw);
		const float initVelZ = a_releaseSpeed * sinf(a_launchPitch);
		// Distance in the XY plane to the target position.
		const float xyDistToTargetPos = Util::GetXYDistance(a_releasePos, a_targetPos);

		// Alpha values for the first and last line segment to draw along the trajectory.
		const float startingAlphaRatio = 0.2f;
		const float endingAlphaRatio = 0.7f;
		// RBG values at the start and end.
		const float rStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0xFF000000) >> 24;
		const float gStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0x00FF0000) >> 16;
		const float bStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0x0000FF00) >> 8;
		const float rEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0xFF000000) >> 24
		);
		const float gEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0x00FF0000) >> 16
		);
		const float bEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0x0000FF00) >> 8
		);
		// Current RGBA and alpha values to set when drawing a line segment.
		uint32_t rgba = Settings::vuOverlayRGBAValues[p->playerID];
		uint32_t alpha = static_cast<uint32_t>(0xFF * startingAlphaRatio);
		
		// Capped projectile trajectory flight time.
		float totalFlightTime = a_initialProjectedTimeToTarget;
		// Cannot split the trajectory into two parts 
		// if the projectile reaches the target in under two frames,
		// so we'll start homing in right away, if this is a homing projectile.
		bool lessThanTwoFramesToReachTarget = 
		(
			a_initialProjectedTimeToTarget <= *g_deltaTimeRealTime * 2.0f
		);
		bool tooLongToReach = 
		(
			totalFlightTime == 0.0f ||
			(
				a_isWeapMagProj ? 
				totalFlightTime >= Settings::fMaxProjAirborneSecsToTarget :
				totalFlightTime >= Settings::fMaxSecsBeforeClearingReleasedRefr
			)
		);
		// Do not draw the entire trajectory beyond a certain interval to improve performance.
		if (tooLongToReach)
		{
			totalFlightTime = Settings::fMaxProjTrajectorySecsToTarget;
		}

		// Total number of time slices to split up the trajectory into.
		const uint32_t totalTimeSlices = 50.0f;
		// Seconds elapsed between each line segment the trajectory is broken up into.
		float secsSlice = totalFlightTime / totalTimeSlices;
		// Current number of elapsed time slices when walking the curve.
		uint32_t sliceCount = 0;
		// Number of time slices that must elapse before drawing a line segment.
		// Produces a 'dotted' effect.
		// Set to 1 to trace the entire trajectory curve.
		uint32_t slicesBeforeDrawing = 2;
		// Incremented until reaching the number of slices before drawing, then wrap around.
		// Line segments are drawn when this equals 0.
		uint32_t currentSliceIndex = 0;
		// Total number of drawn line segments.
		uint32_t drawnSegments = 0;
		// Current time elapsed along the trajectory.
		// Used to get the starting endpoint along the curve for this time slice.
		float currentT = 0.0f;
		// The next iteration's elapsed time along the trajectory.
		// Used to get the next endpoint along the curve for this time slice.
		float nextT = secsSlice;
		// Current distance of the last set endpoint from the release position.
		float distanceFromReleasePos = 0.0f;
		// Distance traversed along the trajectory so far.
		float distanceTraversed = 0.0f;
		// Start and end world positions along the curve for the current time slice.
		RE::NiPoint3 start{ a_releasePos };
		RE::NiPoint3 end{ a_releasePos };
		// Tangent vector for drawing the endpoint cap.
		glm::vec3 tangent{ ToVec3(Util::RotationToDirectionVect(a_launchPitch, a_launchYaw)) };

		/*SPDLOG_DEBUG
		(
			"[TM] DrawTrajectory: {}: Time slices: {}, secs slice: {}, slices before drawing: {}. "
			"Projected ttt: {}, g: {}, mu: {}, xy dist to target: {}, initXY Vel: {}, "
			"air resistance t: {}. Melee attack state: {}.",
			coopActor->GetName(), 
			totalTimeSlices, 
			secsSlice,
			slicesBeforeDrawing, 
			totalFlightTime,
			a_g,
			a_mu,
			xyDistToTargetPos,
			initVelXY,
			(max(0.0f, -logf(1.0f - ((xyDistToTargetPos * a_mu) / initVelXY)) / a_mu)),
			static_cast<uint32_t>(coopActor->GetAttackState())
		);*/

		while (sliceCount < totalTimeSlices)
		{
			// Set color values first.
			alpha = static_cast<uint32_t>
			(
				0xFF * 
				Util::InterpolateEaseIn
				(
					startingAlphaRatio,
					endingAlphaRatio, 
					sliceCount / static_cast<float>(totalTimeSlices), 
					3.0f
				)
			);
			rgba = 
			(
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							rStart, 
							rEnd,
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 24
				) |
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							gStart, 
							gEnd, 
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 16
				) |
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							bStart, 
							bEnd, 
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 8
				) |
				alpha
			);
			
			// Current velocity along the fixed part of this trajectory.
			auto fixedTrajVelocity = RE::NiPoint3
			(
				initVelX,
				initVelY,
				initVelZ - a_g * currentT
			);
			// Pitch and yaw along the fixed part of the trajectory.
			float fixedTrajPitch = Util::GetPitchBetweenPositions
			(
				RE::NiPoint3(), fixedTrajVelocity
			);
			float fixedTrajYaw = Util::GetYawBetweenPositions
			(
				RE::NiPoint3(), fixedTrajVelocity
			);
			// Pitch and yaw from the last computed endpoint to the target position.
			const float pitchToTarget = Util::GetPitchBetweenPositions
			(
				end, a_targetPos
			);
			const float yawToTarget = Util::GetYawBetweenPositions
			(
				end, a_targetPos
			);
			// Pitch/yaw set to launch values (in game coords) for the first time slice,
			// since start == end before the first iteration completes.
			// Last set pitch and yaw from the previous time slices.
			const float lastSetPitch = 
			(
				sliceCount == 0 ?
				-a_launchPitch : 
				Util::DirectionToGameAngPitch(end - start)
			);
			const float lastSetYaw = 
			(
				sliceCount == 0 ?
				Util::ConvertAngle(a_launchYaw) : 
				Util::DirectionToGameAngYaw(end - start)
			);

			if (a_trajType == ProjectileTrajType::kHoming)
			{
				bool wasHomingin = shouldHomeIn;
				if (!shouldHomeIn)
				{
					// Should home in on the target position if past the halfway point,
					// the target position is too far away to reach in time,
					// or if the projectile will reach the target position in under 2 frames.
					bool passedHalfwayPoint = 
					(
						currentT - 0.5f * a_initialProjectedTimeToTarget > -0.1f * secsSlice ||
						Util::GetXYDistance(a_releasePos, end) >=
						0.5f * Util::GetXYDistance(a_releasePos, a_targetPos)
					);
					shouldHomeIn = 
					(
						passedHalfwayPoint || 
						tooLongToReach ||
						lessThanTwoFramesToReachTarget
					);
				}
				
				if (shouldHomeIn)
				{
					//=================================
					// [Set Pitch/Yaw to Track Target]:
					//=================================

					float pitchToSet = fixedTrajPitch;
					float yawToSet = fixedTrajYaw;
					if (tooLongToReach || lessThanTwoFramesToReachTarget)
					{
						// Can't hit target with given launch pitch, 
						// so set yaw directly to target right away.
						pitchToSet = pitchToTarget;
						yawToSet = yawToTarget;
					}
					else
					{
						// Slowly turn to face.
						float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - lastSetPitch);
						pitchToSet = Util::NormalizeAngToPi
						(
							lastSetPitch + 
							Util::InterpolateSmootherStep
							(
								0.0f, 
								pitchDiff,
								min(1.0f, currentT / (a_initialProjectedTimeToTarget))
							)
						);
						float yawDiff = Util::NormalizeAngToPi(yawToTarget - lastSetYaw);
						yawToSet = Util::NormalizeAng0To2Pi
						(
							lastSetYaw + 
							Util::InterpolateSmootherStep
							(
								0.0f, 
								yawDiff,
								min(1.0f, currentT / (a_initialProjectedTimeToTarget))
							)
						);
					}

					// Velocity and speed used to obtain the time slice endpoints.
					auto newVel = RE::NiPoint3();
					float speed = fixedTrajVelocity.Length();
					// Max distance the projectile will travel in 1 frame 
					// at its current velocity.
					float maxDistPerFrame = 
					(
						max(a_releaseSpeed, speed) * *g_deltaTimeRealTime
					);
					// Velocity mult which slows down the projectile when close to the target 
					// to minimize overshooting and jarring course correction.
					float distSlowdownFactor = std::clamp
					(
						powf
						(
							start.GetDistance(a_targetPos) / (maxDistPerFrame + 0.01f), 5.0f
						), 
						0.1f,
						1.0f
					);
						
					if (a_isWeapMagProj)
					{
						speed = max(speed * distSlowdownFactor, min(a_releaseSpeed, 1000.0f));
					}

					newVel = RE::NiPoint3
					(
						Util::RotationToDirectionVect
						(
							-pitchToSet, Util::ConvertAngle(yawToSet)
						) * speed
					);
					
					// once homing, because the projectile's speed is now dependent 
					// on how far away it is from the target, we have to update
					// the time slice interval once the projectile starts homing in.
					uint32_t remainingSlices = totalTimeSlices - sliceCount;
					if (remainingSlices <= 0)
					{
						break;
					}

					secsSlice = 
					(
						(end.GetDistance(a_targetPos) / speed) / remainingSlices
					);

					// Starting from the previous endpoint, 
					// add the velocity * the time slice length.
					start = end;
					end += newVel * secsSlice;
				}
				else
				{
					// Set directly to the kinematically-derived positions when not homing.
					start = RE::NiPoint3
					(
						a_releasePos.x + initVelX * currentT,
						a_releasePos.y + initVelY * currentT,
						a_releasePos.z + initVelZ * currentT - 
						0.5f * a_g * currentT * currentT
					);
					end = RE::NiPoint3
					(
						a_releasePos.x + initVelX * nextT,
						a_releasePos.y + initVelY * nextT,
						a_releasePos.z + initVelZ * nextT - 
						0.5f * a_g * nextT * nextT
					);
				}
			}
			else if (a_g == 0.0f)
			{
				// Straight line towards the target position.
				start = RE::NiPoint3
				(
					a_releasePos.x + initVelX * currentT,
					a_releasePos.y + initVelY * currentT,
					a_releasePos.z + initVelZ * currentT
				);
				end = RE::NiPoint3
				(
					a_releasePos.x + initVelX * nextT,
					a_releasePos.y + initVelY * nextT,
					a_releasePos.z + initVelZ * nextT
				);
			}
			else if (!a_isWeapMagProj)
			{
				// No air resistance considerations for released refrs.
				// Set directly to the kinematically-derived positions.
				start = RE::NiPoint3
				(
					a_releasePos.x + initVelX * currentT,
					a_releasePos.y + initVelY * currentT,
					a_releasePos.z + initVelZ * currentT - 
					0.5f * a_g * currentT * currentT
				);
				end = RE::NiPoint3
				(
					a_releasePos.x + initVelX * nextT,
					a_releasePos.y + initVelY * nextT,
					a_releasePos.z + initVelZ * nextT - 
					0.5f * a_g * nextT * nextT
				);
			}
			else
			{
				// NOTE: 
				// Since the frametime is discrete and certain projectiles move extremely fast,
				// we cannot use the true velocity at any particular time computed
				// from the trajectory's formulas. 
				// We have to instead "connect the dots" between the current trajectory position 
				// and the next expected trajectory position one frame later 
				// to ensure that it will arrive at the next endpoint.
				// If the frametimes vary greatly from frame to frame,
				// the position and velocity calculations will not conform as well
				// to the original trajectory, speeding up and slowing down along the path.
				// 
				// Factors in linear air resistance.
				// May remove eventually.
				//
				// Initial X, Y components of velocity.
				float vx0 = a_releaseSpeed * cosf(a_launchPitch);
				float vy0 = a_releaseSpeed * sinf(a_launchPitch);
				// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
				// XY, and Z positions: 
				// In the 2D plane, the XY pos is the X coordinate,
				// and Z pos is the Y coordinate.
				float currXY = (vx0 / a_mu) * (1 - exp(-a_mu * currentT));
				float currZ = 
				(
					(-a_g * currentT / a_mu) + 
					(1.0f / a_mu) * 
					(vy0 + a_g / a_mu) * 
					(1.0f - exp(-a_mu * currentT))
				);
				// Next projected XY and Z offsets.
				float nextXY = (vx0 / a_mu) * (1 - exp(-a_mu * nextT));
				float nextZ = 
				(
					(-a_g * nextT / a_mu) + 
					(1.0f / a_mu) * 
					(vy0 + a_g / a_mu) * 
					(1.0f - exp(-a_mu * nextT))
				);

				// Here, '+' means up, and '-' means down, unlike the game's pitch convention.
				// Pitch to face the next frame's expected position.
				const float pitchOnTraj = atan2f((nextZ - currZ), (nextXY - currXY));
				// Get the estimated speed from dividing the distance 
				// between the two positions by time slice length.
				const float speedToSet = 
				(
					Util::GetXYDistance(currXY, currZ, nextXY, nextZ) / secsSlice
				);
				// Launch yaw maintained throughout.
				auto vel = Util::RotationToDirectionVect(pitchOnTraj, a_launchYaw) * speedToSet;
				
				// Starting from the previous endpoint, 
				// add the velocity * the time slice length.
				start = end;
				end = start + vel * secsSlice;
			}

			// Cap the velocity once setting both endpoints
			// and then add the capped velocity * time slice length.
			if (a_capVelocity && shouldHomeIn)
			{
				auto deltaPos = end - start;
				auto oldVel = deltaPos.Length() / secsSlice;
				if (oldVel > a_releaseSpeed)
				{
					deltaPos.Unitize();
					end = start + deltaPos * a_releaseSpeed * secsSlice;
				}
			}

			// If the time slice end position is past the target position 
			// on the projectile's way down, set the next time slice end position 
			// to the target position, and break to finish drawing the trajectory.
			RE::NiPoint3 deltaPos = end - start;
			RE::NiPoint3 deltaPosDir = deltaPos;
			RE::NiPoint3 endToTargetPosDir = a_targetPos - end;
			deltaPosDir.Unitize();
			endToTargetPosDir.Unitize();
			auto dot = std::clamp(deltaPosDir.Dot(endToTargetPosDir), -1.0f, 1.0f);
			if (dot + 1.0f <= epsilon && deltaPos.z < 0.0f)
			{
				distanceTraversed += end.GetDistance(start);
				end = a_targetPos;
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}

				break;
			}
			
			float deltaPosDist = deltaPos.Length();
			distanceTraversed += deltaPosDist;
			float distanceOvershoot = distanceTraversed - a_maxRange;
			// Stop walking the curve if the max range was reached.
			if (distanceOvershoot >= 0.0f)
			{
				auto deltaPosDir = end - start;
				deltaPosDir.Unitize();
				// Move end point to within the max range.
				end = start + deltaPosDir * (deltaPosDist - distanceOvershoot);
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}

				break;
			}

			// Raycast and potentially draw a line segment if there are no collisions.
			// At least one endpoint must be on screen.
			if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
			{	
				result = Raycast::hkpCastRay
				(
					ToVec4(start),
					ToVec4(end),
					raycastExcludedRefrs, 
					RE::COL_LAYER::kLOS
				);
				if (result.hit)
				{
					auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
					bool hitRefrValidity =
					(
						hitRefrPtr &&
						hitRefrPtr.get() && 
						Util::IsValidRefrForTargeting(hitRefrPtr.get())
					);
					// No need to continue walking the curve afterward.
					trajCollision = true;
					// Hit the crosshair refr or the aim correction target refr.
					hitTarget = 
					(
						(Util::HandleIsValid(result.hitRefrHandle)) &&
						(
							crosshairRefrValidity &&
							hitRefrValidity &&
							hitRefrPtr == crosshairRefrHandle.get()
						) ||
						(
							rangedTargetActorValidity && 
							hitRefrPtr == rangedTargetActorHandle.get()
						)
					);
					// Hit a selectable refr that wasn't targeted.
					hitSelectableNonTargetRefr = 
					(
						hitRefrValidity && !hitTarget && Util::IsSelectableRefr(hitRefrPtr.get())
					); 
					
					// Hit something, so set the time slice end point 
					// to the target or hit position,
					// and connect a line segment to it.
					if (hitTarget)
					{
						end = a_targetPos;
					}
					else
					{
						end = ToNiPoint3(result.hitPos);
					}

					if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
					{
						distanceFromReleasePos = end.GetDistance(a_releasePos);
						DebugAPI::QueueLine3D
						(
							ToVec3(start),
							ToVec3(end),
							rgba,
							Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
							(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
						);
						++drawnSegments;
					}

					break;
				}

				// Draw line segment if the the current slice index
				// divides into the number of slices before drawing,
				// or if this slice is the last one and will connect to the target position.
				// Also make sure either endpoint is on screen first.
				if ((currentSliceIndex == 0 || sliceCount == totalTimeSlices - 1) && 
					(Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end)))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}
			}

			// Before the next iteration, update the slice index, time points, slice count,
			// and last tangent vector to the curve.
			currentSliceIndex = (currentSliceIndex + 1) % slicesBeforeDrawing;
			currentT += secsSlice;
			nextT += secsSlice;
			++sliceCount;

			tangent = glm::normalize(ToVec3(end - start));
		}
		
		// Thickness for the 'cap' circle that marks the end of the trajectory.
		const float trajectoryCapThickness = 
		(
			0.5f * 
			max
			(
				Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID],
				0.125f * Settings::vfCrosshairGapRadius[p->playerID]
			) / 
			(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
		);
		bool inRange = distanceTraversed < a_maxRange;
		if (a_trajType == ProjectileTrajType::kAimDirection)
		{
			// If there was a hit on the frame before reaching the target position,
			// it is almost always the case that the trajectory hits 
			// the same object that the crosshair raycast hit, 
			// meaning it has effectively reached the target position.
			if ((!trajCollision || sliceCount >= totalTimeSlices - 1) &&
				(inRange && start != end && end != a_targetPos))
			{
				// Connect to the target position if in range.
				start = end;
				end = a_targetPos;
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}
			}
			
			if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
			{
				// Also hit the target if one is selected and there was no collision 
				// along the trajectory to the crosshair world position.
				hitTarget |= 
				(
					(!trajCollision) &&
					(
						(rangedTargetActorValidity) || 
						(crosshairRefrValidity && p->mm->reqFaceTarget)
					)
				);
				if ((inRange) && (hitTarget || hitSelectableNonTargetRefr))
				{
					// Hit something selectable with an aim direction projectile,
					// so notify the player of great success!
					// Draw a crosshair outer outline-colored circle.
					DebugAPI::QueueCircle3D
					(
						ToVec3(end),
						glm::normalize(tangent),
						(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0xFFFFFF00) + 
						static_cast<uint32_t>(endingAlphaRatio * 0xFF),
						16,
						Settings::vfCrosshairGapRadius[p->playerID],
						trajectoryCapThickness,
						true,
						true,
						0.0f
					);
				}
				else
				{
					// Red circle otherwise.
					DebugAPI::QueueCircle3D
					(
						ToVec3(end),
						glm::normalize(tangent),
						0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
						16,
						Settings::vfCrosshairGapRadius[p->playerID],
						trajectoryCapThickness,
						true,
						true,
						0.0f
					);
				}
			}
		}
		else
		{
			// If there was a hit on the frame before reaching the target position,
			// it is almost always the case that the trajectory hits 
			// the same object that the crosshair raycast hit, 
			// meaning it has effectively reached the target position.
			if (!trajCollision || hitTarget || sliceCount >= totalTimeSlices - 1)
			{
				bool connectToTargetPos = 
				(
					(start != end && end != a_targetPos) &&
					(
						(a_isWeapMagProj && inRange) ||
						(!a_isWeapMagProj && a_canReachTarget)
					)
				);
				if (connectToTargetPos)
				{
					// Connect to the target position if in range.
					start = end;
					end = a_targetPos;
					if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
					{
						distanceFromReleasePos = end.GetDistance(a_releasePos);
						DebugAPI::QueueLine3D
						(
							ToVec3(start),
							ToVec3(end),
							rgba,
							Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
							(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
						);
						++drawnSegments;
					}
				}

				if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
				{
					if (a_canReachTarget && inRange)
					{
						// Cap off with a circle to show that the target position/max range 
						// was reached without an intervening collision.
						DebugAPI::QueueCircle3D
						(
							ToVec3(end),
							glm::normalize(tangent),
							(
								Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 
								0xFFFFFF00
							) + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
							16,
							Settings::vfCrosshairGapRadius[p->playerID],
							trajectoryCapThickness,
							true,
							true,
							0.0f
						);
					}
					else
					{
						// Red circle when it is not possible to reach the target position.
						DebugAPI::QueueCircle3D
						(
							ToVec3(end),
							glm::normalize(tangent),
							0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
							16,
							Settings::vfCrosshairGapRadius[p->playerID],
							trajectoryCapThickness,
							true,
							true,
							0.0f
						);
					}
				}
			}
			else if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
			{
				// Red circle when beyond the projectile's range, 
				// or colliding and failing to reach the target position.
				DebugAPI::QueueCircle3D
				(
					ToVec3(end),
					glm::normalize(tangent),
					0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
					16,
					Settings::vfCrosshairGapRadius[p->playerID],
					trajectoryCapThickness,
					true,
					true,
					0.0f
				);
			}
		}

		/*SPDLOG_DEBUG
		(
			"[TM] DrawTrajectory: {}: Time slices: {}, slice count: {}, segments drawn: {}. "
			"Can reach target: {}, traj collision: {}, with target: {}, dist traversed: {}. "
			"Time taken: {} (expected: {}), max range: {}, in range: {}. Traj type: {}.",
			coopActor->GetName(), 
			totalTimeSlices, 
			sliceCount,
			drawnSegments, 
			a_canReachTarget,
			trajCollision,
			hitTarget,
			distanceTraversed,
			currentT,
			totalFlightTime,
			a_maxRange,
			inRange,
			a_trajType
		);*/
	}

	RE::ActorHandle TargetingManager::GetClosestTargetableActorInFOV
	(
		const float& a_fovRads, 
		RE::ObjectRefHandle a_sourceRefrHandle, 
		const bool& a_useXYDistance, 
		const float& a_range,
		const bool& a_combatDependentSelection
	)
	{
		// Get the closest targetable actor to the player or given source refr, if any,
		// using the given FOV in radians centered at their heading angle,
		// and the given maximum range (XY or XYZ distance) to consider targets.
		// If combat dependent selection is requested, 
		// only target hostile actors unless attempting to heal a target.

		auto processLists = RE::ProcessLists::GetSingleton();
		if (!processLists || !coopActor)
		{
			return RE::ActorHandle();
		}

		// Check if the close refr position is within range of the player/source refr position
		// and if the angle the player must turn to face the target 
		// is within the defined FOV window.
		auto isNewClosestActorInFOV =
		[]
		(
			const RE::NiPoint3& a_coopPlayerPos,
			const RE::NiPoint3& a_closeRefrPos, 
			float& a_minWeight,
			const float& a_targetingAngle, 
			const float& a_fovRads,
			const bool& a_useXYDistance,
			const float& a_range, 
			const RE::ObjectRefHandle a_sourceRefrHandle
		) 
		{
			// Within FOV.
			const float turnToFaceActorAngMag = fabsf
			(
				Util::NormalizeAngToPi
				(
					Util::GetYawBetweenPositions(a_coopPlayerPos, a_closeRefrPos) - 
					a_targetingAngle
				)
			);
			const bool inFOV = turnToFaceActorAngMag <= (a_fovRads / 2.0f);
			// Don't need to check range if not in FOV.
			if (!inFOV)
			{
				return false;
			}

			// Disregard range when set to -1.
			bool useRange = a_range != -1.0f;

			// Get distance between source and close refr position.
			float distanceFromSource = FLT_MAX;
			auto sourceRefrPtr = Util::GetRefrPtrFromHandle(a_sourceRefrHandle); 
			if (sourceRefrPtr && sourceRefrPtr.get())
			{
				if (a_useXYDistance)
				{
					distanceFromSource = Util::GetXYDistance
					(
						a_closeRefrPos, sourceRefrPtr->data.location
					);
				}
				else
				{
					distanceFromSource = a_closeRefrPos.GetDistance(sourceRefrPtr->data.location);
				}
			}
			else
			{
				if (a_useXYDistance)
				{
					distanceFromSource = Util::GetXYDistance(a_closeRefrPos, a_coopPlayerPos);
				}
				else
				{
					distanceFromSource = a_closeRefrPos.GetDistance(a_coopPlayerPos);
				}
			}

			// Return false if this actor is not in range.
			// No need to compare distance-angle weight.
			if (useRange && distanceFromSource > a_range)
			{
				return false;
			}

			RE::NiPoint3 toRefrDirXY = a_closeRefrPos - a_coopPlayerPos;
			toRefrDirXY.z = 0.0f;
			toRefrDirXY.Unitize();

			// Minimum selection factor [0, 2]. 
			// Negate the dot product, meaning the more the player has to turn to face the object,
			// the larger the factor.
			// Then we add 1 to ensure all dot product results are > 0, 
			// and mult by 0.5 to set the range to [0, 1]
			// Lastly scale by the distance from the player to the object,
			// meaning objects that are further away have a larger factor.
			// Divide by the range to constrain the factor to [0, 1]
			float minSelectionFactor = FLT_MAX;
			const RE::NiPoint3 facingDirXY = Util::RotationToDirectionVect(0.0f, a_targetingAngle);
			if (useRange)
			{
				minSelectionFactor = 
				(
					(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + 
					(min(1.0f, a_coopPlayerPos.GetDistance(a_closeRefrPos) / a_range))
				);
			}
			else
			{
				minSelectionFactor = 1.0f - facingDirXY.Dot(toRefrDirXY);
			}

			if (minSelectionFactor < a_minWeight)
			{
				// Change min weight, since this actor's is smaller.
				a_minWeight = minSelectionFactor;
				// Return true since this actor is closer (distance and angle-wise).
				return true;
			}

			return false;
		};
		
		// The closest valid actor within the given FOV window.
		RE::Actor* closestActorInFOV = nullptr;
		// Should only target allies if casting a non-hostile spell.
		// Don't want to go around healing enemies, do we?
		bool shouldOnlyTargetAllies = false;
		// Does the attack source chosen based on the player's current combat action
		// contain a spell.
		bool sourceHasSpell = false;
		// Choosing to select based on combat and attack state.
		if (a_combatDependentSelection) 
		{
			RE::TESForm* attackSource = nullptr;
			// Ranged options in right and left hand + quick cast + shout.
			if ((p->pam->AllInputsPressedForAction(InputAction::kCastRH) && 
				 p->em->HasRHSpellEquipped()) ||
				(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) && 
				 p->em->HasRHStaffEquipped()) ||
				(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) &&
				 p->em->Has2HRangedWeapEquipped()))
			{
				attackSource = p->em->equippedForms[!EquipIndex::kRightHand];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kCastLH) && 
					  p->em->HasLHSpellEquipped()) ||
					 (p->pam->AllInputsPressedForAction(InputAction::kAttackLH) &&
					  p->em->HasLHStaffEquipped()))
			{
				attackSource = p->em->equippedForms[!EquipIndex::kLeftHand];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kQuickSlotCast) && 
					  p->em->quickSlotSpell) ||
					  (p->pam->reqSpecialAction == SpecialActionType::kQuickCast))
			{
				attackSource = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
			}
			else if (p->pam->AllInputsPressedForAction(InputAction::kShout) && p->em->voiceSpell) 
			{
				attackSource = p->em->equippedForms[!EquipIndex::kVoice];
			}

			if (p->pam->reqSpecialAction == SpecialActionType::kCastBothHands ||
				p->pam->reqSpecialAction == SpecialActionType::kDualCast)
			{
				// Must be casting healing spells in both hands.
				// Don't want a situation where the player is firing a destruction spell 
				// and a healing spell at a friendly target.
				sourceHasSpell = 
				(
					(p->em->HasLHSpellEquipped() || p->em->HasLHStaffEquipped()) &&
					(p->em->HasRHSpellEquipped() || p->em->HasRHStaffEquipped())
				);
				shouldOnlyTargetAllies = 
				{
					sourceHasSpell && 
					!Util::HasHostileSpell(p->em->equippedForms[!EquipIndex::kLeftHand]) && 
					!Util::HasHostileSpell(p->em->equippedForms[!EquipIndex::kRightHand])
				};
			}
			else if (attackSource)
			{
				// Single attack source, so check if it has a hostile spell.
				sourceHasSpell = attackSource->As<RE::SpellItem>();
				shouldOnlyTargetAllies = sourceHasSpell && !Util::HasHostileSpell(attackSource);
			}
		}

		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Angle around which the FOV window is centered.
		auto targetingAngle = 
		(
			p->mm->lsMoved ? 
			p->mm->movementOffsetParams[!MoveParams::kLSGameAng] :
			p->pam->isAttacking ? 
			Util::DirectionToGameAngYaw(p->mm->playerDefaultAttackSourceDir) :
			coopActor->data.angle.z
		);
		// Lowest distance-angle weight. Starts at max possible value.
		float minAngDistWeight = FLT_MAX;
		// From the player's center.
		const auto playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		// Check all high actors.
		bool isClosest = false;
		// Another actor is in combat with this player.
		bool inCombat = false;
		for (const auto& closeActorHandle : processLists->highActorHandles)
		{
			auto actorPtr = Util::GetActorPtrFromHandle(closeActorHandle); 
			if (!actorPtr || 
				!actorPtr.get() || 
				!Util::IsValidRefrForTargeting(actorPtr.get()) || 
				actorPtr->IsDead())
			{
				continue;
			}

			// Targetable actors:
			// 1. Not blacklisted -AND-
			// 2. When only targeting allies (when casting healing spells),
			// must be a normally friendly actor that is not hostile -OR-
			// 3. When not targeting allies, must be a hostile actor.
				
			// Blacklist set, current mount, and non-players in the co-op entity blacklist.
			bool filteredOut = 
			{
				(actorPtr == coopActor) ||
				(actorPtr == p->GetCurrentMount()) ||
				(
					!GlobalCoopData::IsCoopPlayer(actorPtr.get()) && 
					glob.coopEntityBlacklistFIDSet.contains(actorPtr->formID)
				)
			};
			if (filteredOut)
			{
				continue;
			}

			// Is hostile to and fully detects any active player or player-friendly actor.
			const bool isActivelyHostileToAPlayerOrAlly = std::any_of
			(
				glob.coopPlayers.begin(), glob.coopPlayers.end(), 
				[&actorPtr, p1](const auto& a_p) 
				{
					return 
					(
						(a_p->isActive) &&
						(
							(actorPtr->IsHostileToActor(a_p->coopActor.get())) ||
							(!a_p->isPlayer1 && p1 && actorPtr->IsHostileToActor(p1))
						) &&
						(
							Util::HandleIsValid(actorPtr->currentCombatTarget) &&
							Util::IsPartyFriendlyActor
							(
								actorPtr->currentCombatTarget.get().get()
							)
						)
					);
				}
			);

			// At least one actor is angry at this player.
			if (!inCombat && isActivelyHostileToAPlayerOrAlly)
			{
				inCombat = true;
			}

			// Next, filter out targets based on friendliness
			// and the player's current combat state.
			// Filter out hostile actors when selecting allies and vice versa.
			filteredOut = 
			(
				(a_combatDependentSelection) && 
				(
					(
						(shouldOnlyTargetAllies) && 
						(
							!Util::IsPartyFriendlyActor(actorPtr.get()) || 
							isActivelyHostileToAPlayerOrAlly
						)
					) || 
					(!shouldOnlyTargetAllies && !isActivelyHostileToAPlayerOrAlly)
				)
			);
			if (filteredOut)
			{
				continue;
			}
				
			// Run close actor check to update the new closest actor within the FOV window.
			isClosest = 
			(
				isNewClosestActorInFOV
				(
					playerCenterPos,
					Util::Get3DCenterPos(actorPtr.get()), 
					minAngDistWeight,
					targetingAngle,
					a_fovRads,
					a_useXYDistance,
					a_range, 
					a_sourceRefrHandle
				)
			);
			if (isClosest) 
			{
				closestActorInFOV = actorPtr.get();
			}
		}
		
		// If not in combat and either not casting or casting a hostile spell, 
		// do not pick a close actor target.
		if ((!inCombat) && (!sourceHasSpell || !shouldOnlyTargetAllies)) 
		{
			return RE::ActorHandle();
		}

		// Also add P1 if the companion player is performing this check.
		if (!p->isPlayer1) 
		{
			// No combat-dependent filter or if trying to heal P1.
			if (!a_combatDependentSelection || shouldOnlyTargetAllies)
			{
				// Perform new closest actor in FOV check on P1.
				isClosest = 
				(
					isNewClosestActorInFOV
					(
						playerCenterPos, 
						Util::Get3DCenterPos(glob.player1Actor.get()), 
						minAngDistWeight, 
						targetingAngle, 
						a_fovRads, 
						a_useXYDistance, 
						a_range,
						a_sourceRefrHandle
					)	
				);
				if (isClosest) 
				{
					closestActorInFOV = glob.player1Actor.get();
				}
			}
		}

		// Return handle.
		return closestActorInFOV ? closestActorInFOV->GetHandle() : RE::ActorHandle();
	}

	uint32_t TargetingManager::GetDetectionLvlRGB
	(
		const float& a_detectionLvl, bool&& a_fromRawLevel
	)
	{
		// Get gradient RGB value corresponding to the given detection level.
		// Raw detection level ranges from -1000 to 1000, 
		// but the range [-20, 0] holds the most relevant detection levels 
		// with a noticeable change in awareness,
		// so only raw detection levels in this range are considered.
		// 
		// Gradient RGB values generated using https://rgb.birdflop.com/
		// Credits to MaxSu2019 for the detection level clamping method:
		// https://github.com/max-su-2019/MaxsuDetectionMeter/blob/223b70c779635b7a8388fbf067efa1fed6318194/src/DataHandler.cpp#L19

		uint32_t detectionPctRGB = 0xFFFFFF;
		// Goes from fully green when hidden to fully red when completely detected.
		if (a_fromRawLevel) 
		{
			// From raw detection level [-1000, 1000].
			if (a_detectionLvl <= -20.0f)
			{
				detectionPctRGB = 0x00FF00;
			}
			else if (a_detectionLvl <= -18.0f)
			{
				detectionPctRGB = 0x33FF33;
			}
			else if (a_detectionLvl <= -16.0f)
			{
				detectionPctRGB = 0x66FF66;
			}
			else if (a_detectionLvl <= -14.0f)
			{
				detectionPctRGB = 0x99FF99;
			}
			else if (a_detectionLvl <= -12.0f)
			{
				detectionPctRGB = 0xCCFFCC;
			}
			else if (a_detectionLvl <= -10.0f)
			{
				detectionPctRGB = 0xFFFFFF;
			}
			else if (a_detectionLvl <= -8.0f)
			{
				detectionPctRGB = 0xFFCCCC;
			}
			else if (a_detectionLvl <= -6.0f)
			{
				detectionPctRGB = 0xFF9999;
			}
			else if (a_detectionLvl <= -4.0f)
			{
				detectionPctRGB = 0xFF6666;
			}
			else if (a_detectionLvl <= -2.0f)
			{
				detectionPctRGB = 0xFF3333;
			}
			else
			{
				detectionPctRGB = 0xFF0000;
			}
		}
		else
		{
			// From percent [0, 100].
			if (a_detectionLvl == 0.0f)
			{
				detectionPctRGB = 0x00FF00;
			}
			else if (a_detectionLvl <= 10.0f)
			{
				detectionPctRGB = 0x33FF33;
			}
			else if (a_detectionLvl <= 20.0f)
			{
				detectionPctRGB = 0x66FF66;
			}
			else if (a_detectionLvl <= 30.0f)
			{
				detectionPctRGB = 0x99FF99;
			}
			else if (a_detectionLvl <= 40.0f)
			{
				detectionPctRGB = 0xCCFFCC;
			}
			else if (a_detectionLvl <= 50.0f)
			{
				detectionPctRGB = 0xFFFFFF;
			}
			else if (a_detectionLvl <= 60.0f)
			{
				detectionPctRGB = 0xFFCCCC;
			}
			else if (a_detectionLvl <= 70.0f)
			{
				detectionPctRGB = 0xFF9999;
			}
			else if (a_detectionLvl <= 80.0f)
			{
				detectionPctRGB = 0xFF6666;
			}
			else if (a_detectionLvl <= 90.0f)
			{
				detectionPctRGB = 0xFF3333;
			}
			else
			{
				detectionPctRGB = 0xFF0000;
			}
		}

		return detectionPctRGB;
	}

	uint32_t TargetingManager::GetLevelDifferenceRGB(const RE::ActorHandle& a_actorHandle)
	{
		// Get gradient RGB value corresponding to the level difference 
		// between the player and the given actor.
		// 
		// Gradient RGB values generated using https://rgb.birdflop.com/

		uint32_t levelRGB = 0xFFFFFF;
		auto actorPtr = Util::GetActorPtrFromHandle(a_actorHandle); 
		if (!actorPtr || !actorPtr.get())
		{
			return levelRGB;
		}

		int16_t levelDiff = actorPtr->GetLevel() - coopActor->GetLevel();
		// Level RGB ranges from light green if the player
		// is 10 or more levels above the target,
		// to light blue if the player is at the same level
		// as the target, and then to light red if the player
		// is 10 or more levels below the target.
		if (levelDiff <= -10.0f)
		{
			levelRGB = 0x55ff55;
		}
		else if (levelDiff <= -8.0f)
		{
			levelRGB = 0x6DF577;
		}
		else if (levelDiff <= -6.0f)
		{
			levelRGB = 0x85EB99;
		}
		else if (levelDiff <= -4.0f)
		{
			levelRGB = 0x9CE0BB;
		}
		else if (levelDiff <= -2.0f)
		{
			levelRGB = 0xB4D6DD;
		}
		else if (levelDiff <= 0.0f)
		{
			levelRGB = 0xCCCCFF;
		}
		else if (levelDiff <= 2.0f)
		{
			levelRGB = 0xD6B4DD;
		}
		else if (levelDiff <= 4.0f)
		{
			levelRGB = 0xE09CBB;
		}
		else if (levelDiff <= 6.0f)
		{
			levelRGB = 0xEB8599;
		}
		else if (levelDiff <= 8.0f)
		{
			levelRGB = 0xF56D77;
		}
		else
		{
			levelRGB = 0xFF5555;
		}

		return levelRGB;
	}

	const std::vector<RE::ObjectRefHandle>& TargetingManager::GetNearbyRefrsOfSameType
	(
		RE::ObjectRefHandle a_refrHandle, RefrCompType&& a_compType
	)
	{
		// Get a list of nearby refrs that are the of the same base form type
		// or share the same base form with the given refr.
		
		// Clear old nearby objects of the same type before refreshing.
		nearbyObjectsOfSameType.clear();
		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		if (!refrPtr || !refrPtr.get())
		{
			return nearbyObjectsOfSameType;
		}

		const auto playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		auto refrBaseObject = refrPtr->GetBaseObject();
		// Player wants to steal objects when sneaking.
		bool canSteal = coopActor->IsSneaking();
		// Check each refr in range.
		Util::ForEachReferenceInRange
		(
			playerCenterPos, GetMaxActivationDist(), true,
			[&](RE::TESObjectREFR* a_refr) 
			{
				// Ensure that the object reference is an interactable object, 
				// not a crime to activate,
				// and of the same form type as requested. 

				// Refrs without a loaded name are usually statics or other uninteractable objects.
				if (!a_refr || 
					!Util::HandleIsValid(a_refr->GetHandle()) || 
					!a_refr->IsHandleValid() || 
					!a_refr->Is3DLoaded() || 
					!a_refr->GetCurrent3D() || 
					a_refr->IsDeleted() ||
					strlen(a_refr->GetName()) == 0) 
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				auto baseObj = a_refr->GetBaseObject();
				// Lootable and either the player is choosing to steal the object 
				// or the object is not a crime to activate.
				if ((canSteal || !a_refr->IsCrimeToActivate()) && (Util::IsLootableRefr(a_refr)))
				{
					bool sameType = false;
					if (a_compType == RefrCompType::kSameBaseForm)
					{
						sameType = baseObj && refrBaseObject && baseObj == refrBaseObject;
					}
					else if (a_compType == RefrCompType::kSameBaseFormType)
					{
						sameType = 
						(
							baseObj && 
							refrBaseObject && 
							*baseObj->formType == *refrBaseObject->formType
						);
					}
					else
					{
						// Won't likely happen, but same refr is also an option.
						sameType = a_refr->formID == a_refr->formID;
					}

					if (sameType)
					{
						nearbyObjectsOfSameType.emplace_back(a_refr);
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		return nearbyObjectsOfSameType;
	}

	RE::ObjectRefHandle TargetingManager::GetNextObjectToActivate()
	{
		// Cycle through nearby objects to get an interactable refr,
		// or choose the player's current crosshair refr.
		// Set the result as the next object to activate when the bind is released.

		if (useProximityInteraction)
		{
			// Cycle through nearby objects for a valid, interactable one.
			SelectProximityRefr();
			activationRefrHandle = proximityRefrHandle;
			// Update last activation orientation after setting the proximity refr.
			SetLastActivationCyclingOrientation();
		}
		else
		{
			activationRefrHandle = crosshairRefrHandle;
		}
		
		return activationRefrHandle;
	}

	RE::ObjectRefHandle TargetingManager::GetRangedPackageTargetRefr
	(
		RE::TESForm* a_rangedAttackSource
	)
	{
		// Get a target actor to set as the linked refr for the player's ranged package.
		// Target selection is based on the type of form triggering the attack.

		using CastingType = RE::MagicSystem::CastingType;
		using Delivery = RE::MagicSystem::Delivery;
		RE::ObjectRefHandle targetHandle = RE::ObjectRefHandle();

		// Check if ranged attack source is available first.
		if (!a_rangedAttackSource)
		{
			return targetHandle;
		}

		bool hasSelectedTargetActor = 
		{
			Util::HandleIsValid(selectedTargetActorHandle)
		};
		bool hasAimCorrectionTarget = 
		(
			Util::HandleIsValid(aimCorrectionTargetHandle)
		);
		auto spell = a_rangedAttackSource->As<RE::SpellItem>();

		// Rules:
		// - Target self if for self-targeting spells.
		// - If a spell targets an actor, choose the crosshair-selected actor, 
		// aim correction actor, or the closest actor in the player's FOV.
		// - For touch range spells, choose the closest actor in touch range
		// in front of the player.
		if (spell && spell->GetDelivery() == Delivery::kSelf)
		{
			targetHandle = coopActor->GetHandle();
		}
		else if (spell && spell->GetDelivery() == Delivery::kTouch)
		{
			// If in range, choose the already-selected or aim correction target actor.
			bool cachedTargetChosen = false;
			float distToTarget = FLT_MAX;
			if (hasSelectedTargetActor) 
			{
				distToTarget = Util::GetTorsoPosition(coopActor.get()).GetDistance
				(
					Util::GetTorsoPosition(selectedTargetActorHandle.get().get())
				);
				if (distToTarget <= maxReachActivationDist)
				{
					targetHandle = selectedTargetActorHandle;
					cachedTargetChosen = true;
				}
			}
			else if (hasAimCorrectionTarget)
			{
				distToTarget = Util::GetTorsoPosition(coopActor.get()).GetDistance
				(
					Util::GetTorsoPosition(aimCorrectionTargetHandle.get().get())
				);
				if (distToTarget <= maxReachActivationDist)
				{
					targetHandle = aimCorrectionTargetHandle;
					cachedTargetChosen = true;
				}
			}

			// If neither the selected target actor or aim correction target are valid, 
			// select a new target actor.
			if (!cachedTargetChosen)
			{
				// Get the closest, non-blacklisted actor in a 180 degree arc 
				// in front of the player and within reach distance.
				// Use XY distance to ignore vertical displacements.
				targetHandle = GetClosestTargetableActorInFOV
				(
					PI, RE::ObjectRefHandle(), true, maxReachActivationDist, true
				);
			}
		}
		else if (spell && spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetActor)
		{
			// If a cached target actor is already available, choose it.
			if (hasSelectedTargetActor) 
			{
				targetHandle = selectedTargetActorHandle;
			}
			else if (hasAimCorrectionTarget)
			{
				targetHandle = aimCorrectionTargetHandle;
			}
			else
			{
				// Otherwise, get the closest, non-blacklisted actor 
				// in a 180 degree arc in front of the player.
				// Use XY distance to ignore vertical displacements.
				targetHandle = GetClosestTargetableActorInFOV
				(
					PI, RE::ObjectRefHandle(), true, -1.0f, true
				);
			}
		}
		else if (hasSelectedTargetActor)
		{
			targetHandle = selectedTargetActorHandle;
		}
		else if (hasAimCorrectionTarget)
		{
			targetHandle = aimCorrectionTargetHandle;
		}

		// Last resort: target the player if the target chosen is not valid.
		if (!Util::HandleIsValid(targetHandle))
		{
			targetHandle = coopActor->GetHandle();
		}

		return targetHandle;
	}

	RE::ActorHandle TargetingManager::GetRangedTargetActor()
	{
		// Get the currently targeted actor, if any.

		if (Util::HandleIsValid(selectedTargetActorHandle))
		{
			return selectedTargetActorHandle;
		}
		else if (Settings::vbUseAimCorrection[playerID] &&
				 !p->mm->reqFaceTarget && 
				 Util::HandleIsValid(aimCorrectionTargetHandle))
		{
			return aimCorrectionTargetHandle;
		}
		
		// If there is no selected target and no aim correction target,
		// but the player is running their ranged attack package, 
		// choose the aim target linked refr, if available.
		// Ignore the player if they are the linked refr, 
		// since this is the fall-through case that simply 
		// enables the ranged attack package to execute.
		const auto refrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle); 
		if (refrPtr && refrPtr.get() && refrPtr->As<RE::Actor>() && refrPtr != coopActor)
		{
			return refrPtr->As<RE::Actor>()->GetHandle();
		}

		// No targeted actor.
		return RE::ActorHandle();
	}

	float TargetingManager::GetThrownRefrMagickaCost
	(
		RE::TESObjectREFR* a_refrToThrow, const float& a_grabBindHoldTime
	)
	{
		// Return the magicka cost for throwing the given refr,
		// factoring in the given bind held time to adjust the cost.

		if (p->isInGodMode || !a_refrToThrow)
		{
			return 0.0f;
		}

		float cappedHoldTime = min
		(
			a_grabBindHoldTime,
			max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
		);
		// Adjust release speed based on how long the grab bind was held for.
		// Normalize it.
		float normHoldTime = std::lerp
		(
			0.0f, 
			1.0f,
			cappedHoldTime / max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
		);
		float objectWeight = max(0.0f, a_refrToThrow->GetWeight()) + 0.1f;
		auto asActor = a_refrToThrow->As<RE::Actor>();
		if (asActor)
		{
			// Weights can sometimes be -1, so ensure the weight is at least 0.
			float inventoryWeight = max(0.0f, asActor->GetWeightInContainer());
			const auto invChanges = asActor->GetInventoryChanges();
			if (invChanges)
			{
				inventoryWeight = invChanges->totalWeight;
			}
			objectWeight = objectWeight + inventoryWeight;
			return
			(
				(4.0f * sqrtf(objectWeight)) * 
				(0.5f * normHoldTime + 0.5f) *
				(Settings::vfMagickaCostMult[p->playerID])
			);
		}
		else
		{
			objectWeight = objectWeight + max(0.0f, a_refrToThrow->GetWeightInContainer());
			return
			(
				(4.0f * sqrtf(objectWeight)) * 
				(0.5f * normHoldTime + 0.5f) *
				(Settings::vfMagickaCostMult[p->playerID])
			);
		}
	}

	float TargetingManager::GetThrownRefrMagickaOverflowSlowdownFactor
	(
		const float& a_totalMagickaCost
	)
	{
		// Factor by which to slow down all thrown refrs' release speeds.
		// The more magicka used up below a magicka level of 0, 
		// the smaller the factor and the slower the release speed.

		if (p->isInGodMode)
		{
			return 1.0f;
		}

		const float maxMagicka = Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kMagicka);
		const float currentMagicka = coopActor->GetActorValue(RE::ActorValue::kMagicka);
		// Throw speed slowdown factor based on how much extra magicka
		// the player would need to throw their grabbed refrs.
		// If the cost is larger than the player's remaining magicka,
		// they will throw all refrs at a reduced speed.
		return
		(
			std::clamp
			(
				1.0f - max(0.0f, (a_totalMagickaCost - currentMagicka) / maxMagicka),
				0.25f,
				1.0f
			)
		);
	}

	float TargetingManager::GetTotalThrownRefrMagickaCost(const float& a_grabBindHoldTime)
	{
		// Return the total magicka cost for throwing all this player's grabbed refrs,
		// factoring in the given bind hold time to adjust the cost.

		// No cost when in god mode.
		if (p->isInGodMode)
		{
			return 0.0f;
		}

		float totalMagickaCost = 0.0f;
		float cappedHoldTime = min
		(
			a_grabBindHoldTime,
			max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
		);
		// Adjust release speed based on how long the grab bind was held for.
		// Normalize it.
		float normHoldTime = std::lerp
		(
			0.0f, 
			1.0f,
			cappedHoldTime / max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
		);
	
		if (rmm->isGrabbing && !rmm->grabbedRefrInfoList.empty())
		{
			for (auto i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
			{
				const auto& info = rmm->grabbedRefrInfoList[i];
				if (!info->IsValid())
				{
					continue;
				}

				auto objectPtr = Util::GetRefrPtrFromHandle(info->refrHandle);
				if (!objectPtr || !objectPtr.get())
				{
					continue;
				}

				float objectWeight = max(0.0f, objectPtr->GetWeight()) + 0.1f;
				auto asActor = objectPtr->As<RE::Actor>();
				if (asActor)
				{
					// Weights can sometimes be -1, so ensure the weight is at least 0.
					float inventoryWeight = max(0.0f, asActor->GetWeightInContainer());
					const auto invChanges = asActor->GetInventoryChanges();
					if (invChanges)
					{
						inventoryWeight = invChanges->totalWeight;
					}
					objectWeight = objectWeight + inventoryWeight;
					totalMagickaCost += 
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normHoldTime + 0.5f) *
						(Settings::vfMagickaCostMult[p->playerID])
					);
				}
				else
				{
					objectWeight = objectWeight + max(0.0f, objectPtr->GetWeightInContainer());
					totalMagickaCost +=
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normHoldTime + 0.5f) *
						(Settings::vfMagickaCostMult[p->playerID])
					);
				}
			}
		}
		else if (!rmm->isGrabbing && !rmm->releasedRefrInfoList.empty())
		{
			for (auto i = 0; i < rmm->releasedRefrInfoList.size(); ++i)
			{
				const auto& info = rmm->releasedRefrInfoList[i];
				if (!info->IsValid())
				{
					continue;
				}

				auto objectPtr = Util::GetRefrPtrFromHandle(info->refrHandle);
				if (!objectPtr || !objectPtr.get())
				{
					continue;
				}

				float objectWeight = max(0.0f, objectPtr->GetWeight()) + 0.1f;
				auto asActor = objectPtr->As<RE::Actor>();
				if (asActor)
				{
					// Weights can sometimes be -1, so ensure the weight is at least 0.
					float inventoryWeight = max(0.0f, asActor->GetWeightInContainer());
					const auto invChanges = asActor->GetInventoryChanges();
					if (invChanges)
					{
						inventoryWeight = invChanges->totalWeight;
					}
					objectWeight = objectWeight + inventoryWeight;
					totalMagickaCost += 
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normHoldTime + 0.5f) *
						(Settings::vfMagickaCostMult[p->playerID])
					);
				}
				else
				{
					objectWeight = objectWeight + max(0.0f, objectPtr->GetWeightInContainer());
					totalMagickaCost +=
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normHoldTime + 0.5f) *
						(Settings::vfMagickaCostMult[p->playerID])
					);
				}
			}
		}

		// Apply the player-dependent magicka modifier last.
		return totalMagickaCost * Settings::vfMagickaCostMult[p->playerID];
	}

	void TargetingManager::HandleBonk
	(
		RE::ActorHandle a_hitActorHandle, 
		RE::ObjectRefHandle a_releasedRefrHandle,
		float a_collidingMass,
		const RE::NiPoint3& a_collidingVelocity,
		const RE::NiPoint3& a_contactPos
	)
	{
		// Apply damage to the given hit actor based on the physical properties 
		// of both the given released refr and the hit actor.

		auto hitActorPtr = Util::GetActorPtrFromHandle(a_hitActorHandle);
		auto releasedRefrPtr = Util::GetRefrPtrFromHandle(a_releasedRefrHandle);
		// The hit actor and released refr must be valid.
		if (!hitActorPtr || 
			!hitActorPtr.get() ||
			!releasedRefrPtr || 
			!releasedRefrPtr.get())
		{
			return;
		}

		// Set minimum mass to 1.
		if (a_collidingMass == 0.0f)
		{
			a_collidingMass = 1.0f;
		}

		// Get havok impact speed from the given colliding velocity, 
		// with refr linear speed as a fallback.
		float havokImpactSpeed = a_collidingVelocity.Length() * GAME_TO_HAVOK;
		if (havokImpactSpeed == 0.0f) 
		{
			RE::NiPoint3 linVel{ };
			releasedRefrPtr->GetLinearVelocity(linVel);
			havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
		}

		// Multiplier to apply for a sneak bonk.
		float sneakMult = 
		(
			coopActor->IsSneaking() && detectionPct < 100.0f ? 
			max(2.0f, coopActor->GetActorValue(RE::ActorValue::kAttackDamageMult)) : 
			1.0f
		);
		
		auto asActor = releasedRefrPtr->As<RE::Actor>();
		float actorWeight = 0.0f;
		float releasedRefrWeight = releasedRefrPtr->GetWeight();
		float inventoryWeight = releasedRefrPtr->GetWeightInContainer();
		if (asActor) 
		{
			actorWeight = asActor->GetWeight();
			const auto invChanges = asActor->GetInventoryChanges();
			if (invChanges)
			{
				inventoryWeight = invChanges->totalWeight;
			}

			// Actor weight can be 0, and if the actor has nothing equipped,
			// their total weight would come out to 0.
			// This does not make physical sense since an actor should not weigh less than 
			// a tomato, unless they are Mr. Burns or something, I dunno.
			// Add 50 base weight.
			releasedRefrWeight += inventoryWeight + 50.0f;
		}

		// Ragdoll the hit actor with a force dependent on the colliding body's impact speed.
		auto hitActorRigidBodyPtr = Util::GethkpRigidBody(hitActorPtr.get()); 
		if (hitActorRigidBodyPtr && hitActorRigidBodyPtr.get())
		{
			// TODO: 
			// Add impulse without knockdown setting for less-impactful collisions.
			if (auto precisionAPI4 = ALYSLC::PrecisionCompat::g_precisionAPI4; precisionAPI4)
			{
				precisionAPI4->ApplyHitImpulse2
				(
					a_hitActorHandle, 
					coopActor->GetHandle(), 
					hitActorRigidBodyPtr.get(), 
					ToNiPoint3(a_collidingVelocity), 
					TohkVector4(a_contactPos) * GAME_TO_HAVOK, 
					1.0f
				);
			}

			// Knockout!
			Util::PushActorAway(hitActorPtr.get(), a_contactPos, 0.0f);
		}

		// Set power attack, bonk, and potentially the sneak attack flags
		// before sending a hit event.
		RE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> hitFlags{ };
		hitFlags.set
		(
			RE::TESHitEvent::Flag::kPowerAttack, 
			static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kBonk)
		);
		if (sneakMult > 1.0f)
		{
			hitFlags.set(RE::TESHitEvent::Flag::kSneakAttack);
		}

		// Criteria for damageable actors:
		// Not a ghost or invulnerable.
		bool damageable = 
		{
			(!hitActorPtr->IsGhost() && !hitActorPtr->IsInvulnerable())
		};
		if (damageable) 
		{
			// 4x damage at player level 100.
			float levelDamageFactor = 
			(
				1.0f + 3.0f * max(coopActor->GetLevel() - 1.0f, 0.0f) / 99.0f
			);
			// Scale up damage if the flopping actor is close to 
			// or exceeding their base carryweight.
			float inventoryWeightFactor = 1.0f;
			float weightFactor = sqrtf(max(0.1f, releasedRefrWeight) / 12.0f);
			// Scale up actor-actor collision damage based on the released actor's base, 
			// equipped, and potentially inventory weight.
			if (asActor) 
			{
				float equippedWeight = asActor->GetEquippedWeight();
				// Scale up damage based on the player's inventory weight
				// relative to their base carryweight. 
				// The more over-encumbered the merrier.
				float baseCarryWeight = coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight);
				inventoryWeightFactor = 
				(
					1.0f + 
					(
						inventoryWeight / 
						max(baseCarryWeight, 1.0f)
					)
				);

				// 1x damage at 0 equip weight,
				// 3x damage at 100 equip weight, 
				// approaching 5x at infinite weight.
				weightFactor = 
				(
					1.0f + 
					((actorWeight + 100.0f) / 50.0f) / 
					(1 + expf((90.0f - equippedWeight) / 10.0f))
				);
			}
		
			// Subject to change, but this works for now.
			float damage = 
			(
				havokImpactSpeed *
				inventoryWeightFactor * 
				weightFactor *
				levelDamageFactor *
				sneakMult
			);

			// REMOVE when done debugging.
			SPDLOG_DEBUG
			(
				"[TM] HandleBonk: {}: Hit actor {}. "
				"Thrown object {}'s mass: {}, weight: {}, equipped weight: {}, "
				"impact speed: {}, inventory weight factor: {}, "
				"weight factor: {}, level damage factor: {} (player level: {}). "
				"Sneak mult: {}. Base carryweight: {}. Final Damage: {}.", 
				coopActor->GetName(), 
				hitActorPtr->GetName(),
				releasedRefrPtr->GetName(),
				a_collidingMass,
				releasedRefrPtr->GetWeight(),
				asActor ? asActor->GetEquippedWeight() : -1.0f,
				havokImpactSpeed,
				inventoryWeightFactor,
				weightFactor,
				levelDamageFactor, 
				coopActor->GetLevel(),
				sneakMult,
				coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight),
				damage
			);

			// Handle health damage.
			// Ignore damage to friendly actors if friendly fire is off.
			if ((damage != 0.0f) &&
				(
					Settings::vbFriendlyFire[playerID] || 
					!Util::IsPartyFriendlyActor(hitActorPtr.get())
				))
			{
				// Damage will not be modified in either HandleHealthDamage() hook 
				// because the damage will not be attributed to the player
				// (attacker param is nullptr) since we are directly modifying the health AV here.
				// Therefore, to get the same damage modifications here, 
				// we tack on the thrown object damage mult, 
				// or flop damage mult if the released refr is the player themselves,
				// and multiply the result by the damage received mult if the target is a player.
				if (releasedRefrPtr == coopActor)
				{
					damage *= Settings::vfFlopDamageMult[playerID];
				}
				else
				{
					damage *= Settings::vfThrownObjectDamageMult[playerID];
				}

				// Apply non-zero damage.
				// If hitting another player, apply the damage directly
				// without dealing damage through a second hit event.
				auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(hitActorPtr.get()); 
				if (playerIndex != -1)
				{
					const auto& hitP = glob.coopPlayers[playerIndex];
					damage *= Settings::vfDamageReceivedMult[hitP->controllerID];

					// No requested damage to deal in second hit event.
					rmm->reqSpecialHitDamageAmount = 0.0f;
					// Apply damage directly to the health AV.
					// No attacker source will be reported in the HandleHealthDamage() hook,
					// so our damage here is the final damage which will be applied on hit.
					hitActorPtr->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -damage
					);
				}
				else
				{
					// Divide by P1's damage dealt multiplier to nullify its application
					// in the HandleHealthDamage() hook, which fires on the second hit event
					// that is sent from our Hit Event Handler 
					// (second hit event applies damage, first triggers any P1 OnHit events).
					const auto& p1DamageDealtMult = 
					(
						Settings::vfDamageDealtMult[glob.coopPlayers[glob.player1CID]->playerID]	
					);
					damage = 
					(
						p1DamageDealtMult == 0.0f ?
						0.0f :
						damage / p1DamageDealtMult
					);
					
					// Set the requested special hit damage to apply
					// when sending the second hit event that triggers an alarm/bounty
					// in the Hit Event Handler.
					// No damage to directly apply here.
					rmm->reqSpecialHitDamageAmount = damage;
				}
			}

			// Send the hit event after caching or applying damage.
			Util::SendHitEvent
			(
				coopActor.get(), 
				hitActorPtr.get(), 
				coopActor->formID,
				releasedRefrPtr->formID,
				hitFlags
			);
		}

		// Play sound.
		auto audioManager = RE::BSAudioManager::GetSingleton(); 
		if (!audioManager)
		{
			return;
		}
				
		RE::BSSoundHandle handle{ };
		RE::BGSSoundDescriptorForm* flopSFX =
		(
			RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xAF664)
		);
		if (!flopSFX)
		{	
			return;
		}

		bool succ = audioManager->BuildSoundDataFromDescriptor(handle, flopSFX);
		if (succ)
		{
			handle.SetPosition(a_contactPos);
			auto actor3DPtr = Util::GetRefr3D(hitActorPtr.get());
			if (actor3DPtr && actor3DPtr.get())
			{
				handle.SetObjectToFollow(actor3DPtr.get());
				handle.SetVolume(min(1.0f, havokImpactSpeed / 5.0f));
				handle.Play();
			}
		}

		// Send detection event for the aggressor player.
		Util::SetActorsDetectionEvent
		(
			coopActor.get(), 
			releasedRefrPtr.get(), 
			a_collidingMass,
			a_contactPos
		);
	}

	void TargetingManager::HandleQuickLootMenu()
	{
		// Open the LootMenu when the player moves their crosshair over a lootable container,
		// or close the LootMenu if the player moves their crosshair off the container 
		// or if the container becomes invalid.

		// Only run if QuickLoot is loaded,
		// no temporary menus are open, 
		// and the player is not transformed or transforming.
		if (!ALYSLC::QuickLootCompat::g_quickLootInstalled)
		{
			return;
		}

		if (!Util::MenusOnlyAlwaysUnpaused())
		{
			return;
		}

		// LootMenu opens but is not visible sometimes 
		// when targeting a container while transformed,
		// so don't attempt to open it until the player reverts their form.
		// Players can still loot by selecting the container with their crosshair 
		// and activating it as usual.
		if (p->isTransforming || p->isTransformed)
		{
			return;
		}

		// Check for changes to the player's crosshair-selected refr.
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		auto prevCrosshairRefrPtr = Util::GetRefrPtrFromHandle(prevCrosshairRefrHandle);
		bool crosshairRefrValidity = 
		(
			crosshairRefrPtr &&
			crosshairRefrPtr.get() &&
			Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
		);
		// Has the player moved into/out of range of their targeted refr?
		bool wasInRange = crosshairRefrInRangeForQuickLoot;
		crosshairRefrInRangeForQuickLoot = 
		(
			crosshairRefrValidity ?
			RefrIsInActivationRange(crosshairRefrHandle) :
			false
		);
		// Before sending a crosshair event to change the state of the QuickLoot menu,
		// ensure no other menus are opening back up in quick succession,
		// which will cause flickering due to many requests triggering in at once.
		// Truly an icky solution below.
		float secsSinceAllSupportedMenusClosed = Util::GetElapsedSeconds
		(
			glob.lastSupportedMenusClosedTP
		);
		bool newCrosshairRefr = prevCrosshairRefrPtr != crosshairRefrPtr;
		// Check if this player sent the last successful QuickLoot menu opening request.
		bool sentLastCrosshairRefrEvent = glob.quickLootReqCID == controllerID;
		// If no temporary menus are open and an eighth of a second has passed, 
		// any player can request to open the QuickLoot menu.
		bool anyPlayerCanSet = 
		(
			!glob.supportedMenuOpen.load() && secsSinceAllSupportedMenusClosed > 0.125f
		);
		// Is this player controlling menus?
		bool controllingMenus = 
		(
			glob.supportedMenuOpen.load() && GlobalCoopData::IsControllingMenus(controllerID)
		);
		// Send a new crosshair event to open the QuickLoot menu 
		// if the player's crosshair refr is valid,
		// any player can open the menu, and the refr is now in range + 
		// if it was just selected, not previously in range, 
		// or the player did not send the last opening request.
		bool shouldSendNewSetCrosshairEvent = 
		{
			(crosshairRefrValidity && anyPlayerCanSet) &&
			(
				(crosshairRefrInRangeForQuickLoot) && 
				(newCrosshairRefr || !wasInRange || !sentLastCrosshairRefrEvent)
			)
		};
		// Validate the crosshair event if the crosshair refr is valid,
		// the player is controlling menus,
		// and the player just selected a new refr that is in range.
		bool shouldValidateNewCrosshairEvent = 
		{
			crosshairRefrValidity && 
			controllingMenus && 
			newCrosshairRefr &&
			crosshairRefrInRangeForQuickLoot
		};
		// Close the QuickLoot menu if the player is controlling the menu 
		// and the crosshair refr is no longer valid, or is no longer in range.
		bool shouldSendClearCrosshairEvent = 
		{
			(controllingMenus) && 
			(
				(!crosshairRefrPtr && prevCrosshairRefrPtr) || 
				(wasInRange && !crosshairRefrInRangeForQuickLoot)
			)
		};

		// Can potentially open the QuickLoot menu.
		if (shouldSendNewSetCrosshairEvent || shouldValidateNewCrosshairEvent)
		{
			// Crosshair refr must be lootable.
			bool hasLoot = crosshairRefrPtr->HasContainer();
			if (hasLoot)
			{
				// Check inventory first.
				hasLoot = false;
				auto inventory = crosshairRefrPtr->GetInventory(Util::IsLootableObject);
				for (const auto& [boundObj, invEntryData] : inventory)
				{
					if (boundObj && invEntryData.second && invEntryData.first > 0)
					{
						hasLoot = true;
						break;
					}
				}

				// Then the refr's check dropped inventory.
				if (!hasLoot)
				{
					auto droppedInventory = crosshairRefrPtr->GetDroppedInventory
					(
						Util::IsLootableObject
					);
					for (const auto& [boundObj, objHandleData] : droppedInventory)
					{
						if (boundObj && !objHandleData.second.empty() && objHandleData.first > 0)
						{
							hasLoot = true;
							break;
						}
					}
				}
			}

			// Must have loot, be a container/corpse in range, 
			// be unlocked and not activation blocked,
			// and the player must not be in combat.
			bool canOpenLootMenu = 
			{
				(!crosshairRefrPtr->As<RE::Actor>() || crosshairRefrPtr->IsDead()) &&
				(
					crosshairRefrValidity &&
					hasLoot &&
					crosshairRefrInRangeForQuickLoot &&
					!crosshairRefrPtr->IsLocked() &&
					!crosshairRefrPtr->IsActivationBlocked() &&
					!coopActor->IsInCombat()
				)
			};

			bool firstTimeLootingKilledActor = false;
			// If looting a corpse, give the killing player first rights.
			if (canOpenLootMenu)
			{
				// Is corpse.
				if (auto corpse = crosshairRefrPtr->As<RE::Actor>(); corpse)
				{
					// Saved killing player's FID in the 'ExtraForcedTarget' exData 
					// when the HandleHealthDamage() hook fired before this actor died.
					const auto targetExData = corpse->extraList.GetByType<RE::ExtraForcedTarget>();
					bool killedByAPlayer = GlobalCoopData::IsCoopPlayer(corpse->myKiller);
					firstTimeLootingKilledActor = 
					{
						killedByAPlayer && 
						targetExData && 
						targetExData->pad14 == coopActor->formID
					};

					// Is looted if there is no exData or once the exData's pad is cleared.
					bool corpseLootedByKiller = 
					(
						(killedByAPlayer) && (!targetExData || targetExData->pad14 == 0)
					);
					// Can loot now if this player is looting the actor 
					// they killed for the first time,
					// or if the actor was not killed by a player
					// or looted already by the killer player.
					canOpenLootMenu &= 
					(
						(firstTimeLootingKilledActor) || (!killedByAPlayer || corpseLootedByKiller)
					);
				}
			}

			// Can now open the QuickLoot menu if the final LOS check passes.
			// LOS check last, since it is the most expensive.
			if (canOpenLootMenu)
			{
				bool passesLOSCheck = 
				(
					(Settings::bCamCollisions) || 
					(
						crosshairRefrValidity && 
						Util::HasLOS
						(
							crosshairRefrPtr.get(), 
							coopActor.get(),
							false, 
							true,
							crosshairWorldPos
						)
					)
				);
				if (passesLOSCheck)
				{
					glob.moarm->InsertRequest
					(
						controllerID, 
						InputAction::kMoveCrosshair, 
						SteadyClock::now(),
						GlobalCoopData::LOOT_MENU,
						crosshairRefrPtr->GetHandle()
					);
					// Send SKSE crosshair event to allow QuickLoot menu to trigger.
					// Deselect current crosshair refr first if sending a new crosshair event.
					if (shouldSendNewSetCrosshairEvent)
					{
						Util::SendCrosshairEvent(nullptr);
					}

					Util::SendCrosshairEvent(crosshairRefrPtr.get());

					// After sending a crosshair event to open the LootMenu for a corpse,
					// clear out the exData pad so other players can freely loot the corpse.
					if (firstTimeLootingKilledActor)
					{
						auto selectedTargetActor = Util::GetActorPtrFromHandle
						(
							selectedTargetActorHandle
						); 
						if (selectedTargetActor)
						{
							auto exForcedTarget = 
							(
								selectedTargetActor->extraList.GetByType<RE::ExtraForcedTarget>()	
							);
							exForcedTarget->pad14 = 0;
						}
					}
				}
			}
			else if (shouldValidateNewCrosshairEvent)
			{
				// Clear crosshair pick refr if the player's new crosshair refr is not lootable.
				// Closes the menu.
				Util::SendCrosshairEvent(nullptr);
			}
		}
		else if (shouldSendClearCrosshairEvent)
		{
			// Close the menu on by clearing the crosshair pick refr on request.
			Util::SendCrosshairEvent(nullptr);
		}
	}

	void TargetingManager::HandleReferenceManipulation()
	{
		// Handle positioning and collisions for the player's grabbed and released refrs.
		// Can throw object on release if the player is facing a target.
		// Otherwise, the grabbed object will be dropped when releasing the grab object bind.
		
		// Reset grabbing flag if at least one object was grabbed initially
		// but is now invalid and the number of managed grabbed objects is 0.
		if (rmm->isGrabbing && rmm->GetNumGrabbedRefrs() == 0) 
		{
			rmm->isGrabbing = false;
		}

		// First, a M.A.R.F check.
		isMARFing = false;
		for (const auto& otherP : glob.coopPlayers)
		{
			if (!otherP->isActive || otherP == p)
			{
				continue;
			}

			// Two different players are grabbing each other.
			if (rmm->IsManaged(otherP->coopActor->GetHandle(), true) && 
				otherP->tm->rmm->IsManaged(coopActor->GetHandle(), true))
			{
				isMARFing = true;
				break;
			}
		}

		// Player has grabbed at least one object.
		if (!rmm->grabbedRefrInfoList.empty()) 
		{
			// Clear all invalid grabbed refrs first.
			rmm->ClearInvalidRefrs(true);
			// Release grabbed refrs when no longer grabbing.
			if (!rmm->isGrabbing)
			{
				for (uint8_t i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
				{
					auto& grabbedRefrInfo = rmm->grabbedRefrInfoList[i];
					const auto& handle = grabbedRefrInfo->refrHandle;
					// Now managed as a released refr.
					rmm->AddReleasedRefr(p, handle);

					// Reset paralysis flag and max out ragdoll timer on actors 
					// to prevent the game from instantly signalling them 
					// to get up after being released.
					// Set even if actor getup removal setting is enabled 
					// since we don't want NPCs getting up in midair.
					auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
					if (!refrPtr || !refrPtr.get()) 
					{
						continue;
					}

					auto asActor = refrPtr->As<RE::Actor>(); 
					if (!asActor)
					{
						continue;
					}

					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					if (asActor->currentProcess && asActor->currentProcess->middleHigh)
					{
						asActor->currentProcess->middleHigh->unk2B0 = FLT_MAX;
					}
				}

				// Clear all grabbed refrs on release.
				rmm->ClearGrabbedRefrs();
				
				// If one refr is thrown, all are thrown.
				bool isThrown = 
				(
					rmm->releasedRefrInfoList.empty() ?
					false :
					rmm->releasedRefrInfoList[0]->isThrown
				);
				// Expend magicka if throwing all previously grabbed refrs.
				if (isThrown)
				{
					// Update magicka overflow slowdown factor to use 
					// when adjusting the release velocity of thrown refrs.
					float secsGrabHeldTime = 
					(
						p->pam->paStatesList
						[!InputAction::kGrabObject - !InputAction::kFirstAction].secsPerformed
					);
					float magickaCost = GetTotalThrownRefrMagickaCost(secsGrabHeldTime);
					magickaOverflowSlowdownFactor = GetThrownRefrMagickaOverflowSlowdownFactor
					(
						magickaCost
					);

					// Expend magicka.
					if (magickaCost > 0.0f)
					{
						p->pam->ModifyAV(RE::ActorValue::kMagicka, -magickaCost);
					}
				}
			}
			else
			{
				// Update grabbed refrs' positioning via their velocity.
				// If grabbing multiple objects, all other objects are suspended 
				// in a ring around the first one.
				// Radius of ring at which to suspend all subsequent grabbed refrs.
				float ringBufferDist = 0.0f;
				bool unloaded = false;
				for (uint8_t i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
				{
					auto& grabbedRefrInfo = rmm->grabbedRefrInfoList[i];
					if (!grabbedRefrInfo || !grabbedRefrInfo.get())
					{
						continue;
					}

					const auto& handle = grabbedRefrInfo->refrHandle;
					auto refrPtr = Util::GetRefrPtrFromHandle(handle);
					if (!refrPtr || !refrPtr.get())
					{
						continue;
					}

					// Get buffer distance if it has not been set yet.
					if (ringBufferDist == 0.0f)
					{
						// Must use the first valid refr's radius for spacing purposes.
						auto refr3DPtr = Util::GetRefr3D(refrPtr.get()); 
						if (refr3DPtr && refr3DPtr.get())
						{
							ringBufferDist = 
							(
								refr3DPtr->worldBound.radius * 0.5f
							);
						}
						else
						{
							ringBufferDist = refrPtr->GetHeight() * 0.25f;
						}
					}

					// Paralyze living actor to prevent the game from automatically
					// signalling the actor to get up once the ragdoll timer hits 0.
					// Only done if the actor getup removal setting is enabled.
					if (Settings::bRemoveGrabbedActorAutoGetUp)
					{
						auto asActor = refrPtr->As<RE::Actor>(); 
						if (asActor &&
							!asActor->IsDead() &&
							asActor->boolBits.none(RE::Actor::BOOL_BITS::kParalyzed))
						{
							asActor->boolBits.set(RE::Actor::BOOL_BITS::kParalyzed);
							if (asActor->currentProcess && asActor->currentProcess->middleHigh)
							{
								asActor->currentProcess->middleHigh->unk2B0 = FLT_MAX;
							}
						}
					}

					// If not manipulable, don't handle.
					if (rmm->CanManipulateGrabbedRefr(p, i))
					{
						grabbedRefrInfo->UpdateGrabbedReference(p, i, ringBufferDist);
					}
				}
			}
		}

		if (!rmm->releasedRefrInfoList.empty()) 
		{
			// Clear all invalid/inactive released refrs before updating.
			rmm->ClearInvalidRefrs(false);
			rmm->ClearInactiveReleasedRefrs();
			// Handle contact events first.
			rmm->HandleQueuedContactEvents(p);

			// Two tasks for each released refr:
			// 1. Perform raycast collision check, as the havok contact listener 
			// fails to detect collisions sometimes. Increases the likelihood of a hit.
			// 2. Adjust the trajectory of the released refr if using homing projectiles.
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
			for (uint8_t i = 0; i < rmm->releasedRefrInfoList.size(); ++i)
			{
				auto& releasedRefrInfo = rmm->releasedRefrInfoList[i];
				const auto& handle = releasedRefrInfo->refrHandle;
				// Must have been released.
				if (!releasedRefrInfo->releaseTP.has_value())
				{
					rmm->ClearRefr(handle);
					// Decrement index since the next element has shifted 
					// into this index upon erasure.
					--i;
					continue;
				}

				auto releasedRefrPtr = Util::GetRefrPtrFromHandle
				(
					releasedRefrInfo->refrHandle
				);
				float secsSinceRelease = Util::GetElapsedSeconds
				(
					releasedRefrInfo->releaseTP.value()
				);
				// Seconds since the released refr first hit another refr.
				float secsSinceFirstHit = 0.0f;
				if (releasedRefrInfo->firstHitTP.has_value()) 
				{
					secsSinceFirstHit = Util::GetElapsedSeconds
					(
						releasedRefrInfo->firstHitTP.value()
					);
				}

				// Get active projectile, if the relased refr was demarcated as one.
				// No collision handling, as the game already does it for us.
				auto asActiveProjectile = 
				(
					releasedRefrInfo->isActiveProjectile ? 
					releasedRefrPtr->As<RE::Projectile>() : 
					nullptr
				);
				if (asActiveProjectile)
				{
					continue;
				}

				// Clear invalid refr and move on to the next one.
				if (!releasedRefrPtr || !releasedRefrPtr.get())
				{
					rmm->ClearRefr(handle);
					--i;
					continue;
				}

				// Do perform released refr collision checks any longer if:
				// 1. The released refr is a live actor that has gotten up from ragdolling -OR-
				// 2. The released refr hit the player's target refr -OR-
				// 3. The post-release handling period has ended.
				bool shouldNoLongerHandleCollisions =
				(
					(
						!releasedRefrPtr->IsDead() &&
						releasedRefrPtr->Is(RE::FormType::ActorCharacter) &&
						releasedRefrPtr->As<RE::Actor>()->actorState1.knockState == 
						RE::KNOCK_STATE_ENUM::kNormal
					) ||
					(
						crosshairRefrPtr && 
						crosshairRefrPtr.get() &&
						releasedRefrInfo->hitRefrFIDs.contains(crosshairRefrPtr->formID)
					) ||
					(
						secsSinceRelease > Settings::fMaxSecsBeforeClearingReleasedRefr
					)
				);
				// Clear out released refr once we don't need to handle collisions for it.
				if (shouldNoLongerHandleCollisions)
				{
					rmm->ClearRefr(handle);
					--i;
					continue;
				}

				// Add a new contact listener for the current cell, 
				// if there is no active one currently.
				if (auto cell = releasedRefrPtr->parentCell; cell)
				{
					if (auto bhkWorld = cell->GetbhkWorld(); bhkWorld)
					{
						auto ahkpWorld = bhkWorld->GetWorld2(); 
						if (ahkpWorld && glob.contactListener->world != ahkpWorld)
						{
							glob.contactListener->world = ahkpWorld;
							glob.contactListener->AddContactListener(ahkpWorld);
						}
					}
				}

				// Raycast collision checks.
				auto releasedRefr3DPtr = Util::GetRefr3D(releasedRefrPtr.get());
				auto releasedRefrRigidBodyPtr = Util::GethkpRigidBody(releasedRefrPtr.get()); 
				// Must have both valid 3D and rigid body for raycast collision checks.
				if (!releasedRefr3DPtr ||
					!releasedRefr3DPtr.get() ||
					!releasedRefrRigidBodyPtr || 
					!releasedRefrRigidBodyPtr.get())
				{
					continue;
				}

				auto releasedActor = releasedRefrPtr->As<RE::Actor>();
				RE::ObjectRefHandle hitRefrHandle{ };
				RE::NiPoint3 hitPos{ };
				RE::NiPoint3 velDir{ };
				glm::vec4 start{ };
				glm::vec4 end{ };
				glm::vec4 velOffset{ };
						
				// Raycast once per major NPC skeleton node.
				// Only need a single raycast to hit before breaking.
				uint32_t numNodesCastFrom = 0;
				// A raycast from the released refr hit something.
				bool hit = false;
				if (!Settings::bSimpleActorCollisionRaycast)
				{
					// Actor collisions -- multiple raycasts per actor.
					// Again, must have valid loaded 3D.
					if (!releasedRefrPtr->loadedData || 
						!releasedRefrPtr->loadedData->data3D)
					{
						continue;
					}

					auto loadedData = releasedRefrPtr->loadedData; 
					auto data3D = loadedData->data3D;
					RE::BSVisit::TraverseScenegraphObjects
					(
						data3D.get(),
						[
							this, 
							&releasedRefrPtr,
							&start, 
							&end, 
							&hit,
							&hitRefrHandle,
							&hitPos, 
							&velDir,
							&velOffset,
							&numNodesCastFrom
						]
						(RE::NiAVObject* a_node)
						{
							auto nodePtr = RE::NiPointer<RE::NiAVObject>(a_node);
							// Invalid node.
							if (!a_node || 
								!nodePtr || 
								!nodePtr.get() || 
								!nodePtr->AsNode())
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Need a collision object from the node's 3D.
							auto collisionObject = nodePtr->GetCollisionObject();
							if (!collisionObject)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}
							
							// Dedundant nodes which usually match with the skeleton,
							// so we won't cast from them.
							if (nodePtr->name.contains("CME"))
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Need a rigid body.
							auto hkpRigidBodyPtr = Util::GethkpRigidBody(nodePtr.get()); 
							if (!hkpRigidBodyPtr || 
								!hkpRigidBodyPtr.get() || 
								!hkpRigidBodyPtr->GetCollidable())
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Havok shape for its radius.
							const RE::hkpShape* hkpShape = 
							(
								hkpRigidBodyPtr->collidable.shape
							);
							if (!hkpShape)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							auto shape = static_cast<const RE::hkpConvexShape*>(hkpShape);
							// Invalid shape or radius.
							// The radius is sometimes reported as negative
							// or impossibly large, and any one node should not
							// have a radius larger than the height of the object
							// it belongs to anyways. Ignore these nodes.
							if (!shape || 
								shape->radius <= 0.0f ||
								shape->radius > 
								releasedRefrPtr->GetHeight() * GAME_TO_HAVOK)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							velDir = ToNiPoint3
							(
								hkpRigidBodyPtr->motion.linearVelocity, true
							);
							// Zero velocity -> no collision.
							if (velDir.Length() == 0.0f)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							float distPerFrame = 
							(
								hkpRigidBodyPtr->motion.linearVelocity.Length3() *
								*g_deltaTimeRealTime *
								HAVOK_TO_GAME
							);
							float radius = (shape->radius * HAVOK_TO_GAME) + distPerFrame;
							// Cast from node's world position outward a length 
							// equal to the node's radius in the direction 
							// of the node's velocity.
							velOffset = ToVec4(velDir * radius);
							start = ToVec4(nodePtr->world.translate);
							end = start + velOffset;
							auto result = Raycast::hkpCastRay
							(
								start, 
								end, 
								std::vector<RE::TESObjectREFR*>({ releasedRefrPtr.get() }),
								std::vector<RE::FormType>()
							);
							// Increment cast count.
							numNodesCastFrom++;
									
							// REMOVE when done debugging.
							/*DebugAPI::QueueArrow3D
							(
								start, 
								end,
								Settings::vuOverlayRGBAValues[p->playerID],
								3.0f,
								2.0f
							);*/
							/*SPDLOG_DEBUG
							(
								"[TM] HandleReferenceManipulation: {}: {}: {}. "
								"Nodes cast from: {}",
								coopActor->GetName(),
								releasedRefrPtr->GetName(),
								nodePtr->name,
								numNodesCastFrom + 1
							);*/

							// Only need a single hit, so once there is a hit, 
							// we save the hit refr and position and then break.
							if (result.hit)
							{
								hit = true;
								hitRefrHandle = result.hitRefrHandle;
								hitPos = ToNiPoint3(result.hitPos);
								return RE::BSVisit::BSVisitControl::kStop;
							}

							return RE::BSVisit::BSVisitControl::kContinue;
						}
					);
				}

				// If not using the more comprehensive raycast collision detection system,
				// or if the released refr had no valid nodes to raycast from, 
				// fall back to the simpler raycast collision check.
				if (Settings::bSimpleActorCollisionRaycast || numNodesCastFrom == 0) 
				{
					// A single raycast starting from the released refr's reported location 
					// or center and in the direction of the node/refr's velocity.
					// Cast length equals the distance the node travels per frame 
					// at the current velocity plus a small increment 
					// equal to half the refr's height or equal to its radius.
							
					velDir = ToNiPoint3
					(
						releasedRefrRigidBodyPtr->motion.linearVelocity, true
					);
					float distPerFrame = 
					(
						releasedRefrRigidBodyPtr->motion.linearVelocity.Length3() *
						*g_deltaTimeRealTime *
						HAVOK_TO_GAME
					);
					float incThrownRefrRadius = 0.0f;
					if (releasedActor)
					{
						incThrownRefrRadius = 
						(
							releasedRefrPtr->GetHeight() / 2.0f + distPerFrame
						);
					}
					else
					{
						incThrownRefrRadius = 
						(
							releasedRefr3DPtr->worldBound.radius + distPerFrame
						);
					}

					if (releasedActor)
					{
						start = ToVec4(Util::GetTorsoPosition(releasedActor));
					}
					else
					{
						start = ToVec4(releasedRefr3DPtr->worldBound.center);
					}

					if (releasedActor)
					{
						end = ToVec4
						(
							Util::GetTorsoPosition(releasedActor) + velDir * incThrownRefrRadius
						);
					}
					else
					{
						end = ToVec4
						(
							releasedRefr3DPtr->worldBound.center +
							velDir * 
							incThrownRefrRadius
						);
					}

					auto result = Raycast::hkpCastRay
					(
						start, 
						end, 
						std::vector<RE::TESObjectREFR*>({ releasedRefrPtr.get() }),
						std::vector<RE::FormType>()
					);
					hit = result.hit;
					hitRefrHandle = result.hitRefrHandle;
					hitPos = ToNiPoint3(result.hitPos);

					// REMOVE when done debugging.
					/*DebugAPI::QueueArrow3D
					(
						start, 
						end,
						Settings::vuOverlayRGBAValues[p->playerID],
						3.0f,
						2.0f
					);*/
				}
						
				auto hitRefrPtr = 
				(
					hit ? Util::GetRefrPtrFromHandle(hitRefrHandle) : nullptr
				); 
				bool hitRefrValid = hitRefrPtr && hitRefrPtr.get();
				// Do not continue setting the released refr's trajectory,
				// or bonk or splat if the hit actor is a player that is dash dodging.
				if (hitRefrValid)
				{
					auto hitPlayerIndex = GlobalCoopData::GetCoopPlayerIndex
					(
						hitRefrPtr.get()
					);
					if (hitPlayerIndex != -1 && 
						glob.coopPlayers[hitPlayerIndex]->mm->isDashDodging)
					{
						const auto& trajType = 
						(
							Settings::vuProjectileTrajectoryType[p->playerID]
						);
						// Also clear the released refr, so we don't continue 
						// setting its trajectory if it is homing in on the target.
						// Otherwise, it'll go through the player, come back around,
						// and hit the player once their dodge I-frames end.
						if (trajType == !ProjectileTrajType::kHoming)
						{
							rmm->ClearRefr(handle);
						}

						continue;
					}
				}

				// Adjust trajectory to reach the trajectory end position 
				// or home in on the target if necessary.
				if (releasedRefrInfo->isThrown)
				{
					auto velToSet = releasedRefrInfo->GuideRefrAlongTrajectory(p);
					// Cap speed to the release speed, 
					// which effectively caps bonk damage as well.
					releasedRefrInfo->ApplyVelocity(velToSet);
				}

				// Handle potential collisions.
				if (hitRefrValid)
				{
					bool hasAlreadyHitRefr = 
					(
						releasedRefrInfo->HasAlreadyHitRefr(hitRefrPtr.get())
					);

					// Ignore collisions between managed released refrs.
					if (rmm->IsManaged(hitRefrHandle, false))
					{
						continue;
					}

					// Ignore refrs without collision, such as activators.
					auto hitRigidBodyPtr = Util::GethkpRigidBody(hitRefrPtr.get()); 
					bool hasCollidable =
					(
						hitRigidBodyPtr &&
						hitRigidBodyPtr.get() && 
						hitRigidBodyPtr->GetCollidable()
					);
					if (!hasCollidable)
					{
						continue;
					}

					// Add hit refr to cached hit form IDs set.
					releasedRefrInfo->AddHitRefr(hitRefrPtr.get());
					auto hitActor = hitRefrPtr->As<RE::Actor>(); 
					// Hit a new, valid actor that is not the released refr 
					// or the player that released the refr.
					bool shouldBonk = 
					(
						hitActor && 
						hitActor->currentProcess && 
						hitRefrPtr != releasedRefrPtr && 
						hitRefrPtr != coopActor && 
						!hasAlreadyHitRefr
					);
					if (shouldBonk)
					{
						HandleBonk
						(
							hitActor->GetHandle(), 
							releasedRefrPtr->GetHandle(), 
							releasedRefrRigidBodyPtr->motion.GetMass(),
							ToNiPoint3
							(
								releasedRefrRigidBodyPtr->motion.linearVelocity *
								HAVOK_TO_GAME
							),
							hitPos
						);
					}

					// Thrown actor hit a new refr that isn't itself. Splat.
					bool shouldSplat = 
					(
						releasedActor &&
						releasedActor != hitRefrPtr.get() &&
						!hasAlreadyHitRefr && 
						!releasedRefrInfo->hitRefrFIDs.empty()
					);
					if (shouldSplat)
					{
						HandleSplat
						(
							releasedActor->GetHandle(),
							releasedRefrInfo->hitRefrFIDs.size()
						);
					}
				}
			}
		}
		else
		{
			// No managed released refrs, so clear out cached collision refr pairs.
			if (!rmm->collidedRefrFIDPairs.empty())
			{
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set
					// once there are no remaining released refrs to handle.
					rmm->collidedRefrFIDPairs.clear();
				}
			}

			// Also clear any queued contact events, which do not need handling anymore.
			if (!rmm->queuedReleasedRefrContactEvents.empty())
			{
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set once there 
					// are no remaining released refrs to handle.
					rmm->queuedReleasedRefrContactEvents.clear();
				}
			}

			// Finally, clear out released refr map if not empty already.
			if (!rmm->releasedRefrHandlesToInfoIndices.empty()) 
			{
				rmm->releasedRefrHandlesToInfoIndices.clear();
			}
		}
	}

	void TargetingManager::HandleSplat
	(
		RE::ActorHandle a_thrownActorHandle, const uint32_t& a_hitCount
	)
	{
		// Apply impact damage to thrown/ragdolled actor.

		auto thrownActorPtr = Util::GetActorPtrFromHandle(a_thrownActorHandle);
		// Invalid thrown actor.
		if (!thrownActorPtr || !thrownActorPtr.get())
		{
			return;
		}
		
		auto releasedRefrRigidBodyPtr = Util::GethkpRigidBody(thrownActorPtr.get()); 
		float havokImpactSpeed = 0.0f;
		float damage = 0.0f;
		// Not a ghost or invulnerable.
		bool damageable = 
		{
			(!thrownActorPtr->IsGhost() && !thrownActorPtr->IsInvulnerable())
		};
		if (damageable)
		{
			if (releasedRefrRigidBodyPtr && releasedRefrRigidBodyPtr.get()) 
			{
				havokImpactSpeed = releasedRefrRigidBodyPtr->motion.linearVelocity.Length3();
				// Get refr linear speed if rigidbody speed is 0.
				if (havokImpactSpeed == 0.0f)
				{
					RE::NiPoint3 linVel;
					thrownActorPtr->GetLinearVelocity(linVel);
					havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
				}

				// Higher armor rating -> less damage taken.
				// 1 / 2 the damage at an armor rating of 100.
				float armorRatingFactor = sqrtf
				(
					1.0f / 
					(max(thrownActorPtr->CalcArmorRating() / 25.0f, 1.0f))
				);
				// Take 1 / 2 the damage at level 100.
				float levelDamageFactor = 
				(
					1.0f / 
					(1.0f + max(thrownActorPtr->GetLevel() - 1.0f, 0.0f) / 99.0f)
				);

				float inventoryWeight = thrownActorPtr->GetWeightInContainer();
				const auto invChanges = thrownActorPtr->GetInventoryChanges();
				if (invChanges)
				{
					inventoryWeight = invChanges->totalWeight;
				}

				float baseCarryWeight = coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight);
				// Actors that are nearly or over-encumbered take more damage.
				float inventoryWeightFactor = 
				(
					1.0f + 
					(
						inventoryWeight / 
						max(baseCarryWeight, 1.0f)
					)
				);

				bool isFlop = thrownActorPtr == coopActor;
				float flopSelfDamageMult = 1.0f;
				if (isFlop)
				{
					// Players take more damage if they weigh less 
					// and if they have higher equipped weight.
					float actorWeight = thrownActorPtr->GetWeight();
					float equippedWeight = thrownActorPtr->GetEquippedWeight();
					flopSelfDamageMult = 
					(
						1.0f + 
						((-actorWeight + 200.0f) / 50.0f) / 
						(1 + expf((50.0f - equippedWeight) / 10.0f))
					);
				}

				damage = 
				(
					levelDamageFactor * 
					armorRatingFactor * 
					inventoryWeightFactor *
					sqrtf(havokImpactSpeed) * 
					flopSelfDamageMult *
					(1.0f / static_cast<float>(max(1, a_hitCount)))
				);

				// REMOVE when done debugging.
				SPDLOG_DEBUG
				(
					"[TM] HandleSplat: {}: "
					"Thrown actor: {}. Mass: {}, impact speed: {}, actor linear speed: {}, "
					"armor rating factor: {}, inventory weight factor: {}, "
					"level damage factor: {}, flop self-damage mult: {}, damage: {}. Hit #{}",
					coopActor->GetName(),
					thrownActorPtr->GetName(),
					releasedRefrRigidBodyPtr->motion.GetMass(),
					havokImpactSpeed,
					Util::GetActorLinearVelocity(thrownActorPtr.get()).Length() * GAME_TO_HAVOK,
					armorRatingFactor,
					inventoryWeightFactor,
					levelDamageFactor,
					flopSelfDamageMult,
					damage, 
					a_hitCount
				);
			}

			// Handle health damage.
			// Ignore damage to friendly actors if friendly fire is off.
			bool shouldDamage = 
			(
				Settings::vbFriendlyFire[playerID] || 
				thrownActorPtr == coopActor || 
				!Util::IsPartyFriendlyActor(thrownActorPtr.get())
			);
			if (damage != 0.0f && shouldDamage)
			{
				damage *= Settings::vfThrownObjectDamageMult[playerID];

				// Apply non-zero damage.
				// If hitting another player, apply the damage directly
				// without dealing damage through a second hit event,
				// which is used to attribute blame for the hit to P1 and trigger alarms/bounties. 
				auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(thrownActorPtr.get());
				if (playerIndex != -1)
				{
					const auto& thrownP = glob.coopPlayers[playerIndex];
					damage *= Settings::vfDamageReceivedMult[thrownP->controllerID];
					// No requested damage to deal in second hit event.
					rmm->reqSpecialHitDamageAmount = 0.0f;
					// Apply damage directly to the health AV.
					// No attacker source will be reported in the HandleHealthDamage() hook,
					// so our damage here is the final damage which will be applied on hit.
					thrownActorPtr->RestoreActorValue
					(
						RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -damage
					);
				}
				else
				{
					// Divide by P1's damage dealt multiplier to nullify its application
					// in the HandleHealthDamage() hook, which fires on the second hit event
					// that is sent from our Hit Event Handler 
					// (second hit event applies damage, first triggers any P1 OnHit events).
					const auto& p1DamageDealtMult = 
					(
						Settings::vfDamageDealtMult[glob.coopPlayers[glob.player1CID]->playerID]	
					);
					damage = 
					(
						p1DamageDealtMult == 0.0f ?
						0.0f :
						damage / p1DamageDealtMult
					);
					// Set the requested special hit damage to apply
					// when sending the second hit event that triggers an alarm/bounty
					// in the Hit Event Handler.
					// No damage to directly apply here.
					rmm->reqSpecialHitDamageAmount = damage;
				}
			}

			// Power attack flag to add compatibility with Maximum Carnage,
			// which triggers gore effects on power attack kills.
			// Set splat hit flag as well.
			SKSE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> flags{ };
			flags.set
			(
				RE::TESHitEvent::Flag::kPowerAttack, 
				static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kSplat)
			);
			Util::SendHitEvent
			(
				coopActor.get(),
				thrownActorPtr.get(),
				coopActor->formID,
				thrownActorPtr->formID,
				flags
			);
		}

		// Play sound.
		auto audioManager = RE::BSAudioManager::GetSingleton(); 
		if (!audioManager)
		{
			return;
		}
				
		RE::BSSoundHandle handle{ };
		RE::BGSSoundDescriptorForm* flopSFX =
		(
			RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xAF664)
		);
		if (!flopSFX)
		{	
			return;
		}

		bool succ = audioManager->BuildSoundDataFromDescriptor(handle, flopSFX);
		if (succ)
		{
			handle.SetPosition(thrownActorPtr->data.location);
			auto actor3DPtr = Util::GetRefr3D(thrownActorPtr.get());
			if (actor3DPtr && actor3DPtr.get())
			{
				handle.SetObjectToFollow(actor3DPtr.get());
				handle.SetVolume(min(1.0f, havokImpactSpeed / 5.0f));
				handle.Play();
			}
		}
		
		// Send detection event for the aggressor player.
		Util::SetActorsDetectionEvent
		(
			coopActor.get(), 
			thrownActorPtr.get(), 
			releasedRefrRigidBodyPtr && releasedRefrRigidBodyPtr.get() ? 
			releasedRefrRigidBodyPtr->motion.GetMass() : 
			0.0f,
			thrownActorPtr->data.location
		);
	}

	bool TargetingManager::IsRefrValidForCrosshairSelection(RE::ObjectRefHandle a_refrHandle)
	{
		// Is the given refr targetable by the player's crosshair?

		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		// No target refr.
		if (!refrPtr || !refrPtr.get()) 
		{
			return false;
		}

		// First baseline validity check for handle validity, 3D loaded, not disabled, etc.
		if (!Util::IsValidRefrForTargeting(refrPtr.get()))
		{
			return false;
		}

		// Blacklist check.
		bool isSelf = refrPtr == coopActor;
		bool isCoopPlayer = GlobalCoopData::IsCoopPlayer(refrPtr.get());
		bool isNotSelectableCoopEntity = 
		{ 
			(isSelf) || 
			(isCoopPlayer && !Settings::vbCanTargetOtherPlayers[playerID]) ||
			(!isCoopPlayer && glob.coopEntityBlacklistFIDSet.contains(refrPtr->formID))
		};
		if (isNotSelectableCoopEntity)
		{
			return false;
		}

		// Skip LOS checks if targeting another player.
		if (isCoopPlayer)
		{
			crosshairRefrInSight = true;
			return true;
		}

		// Check LOS if the chosen result is not the closest once,
		// meaning another refr is blocking at least part of the chosen refr.
		// Need to also check LOS if the player is not moving their crosshair,
		// since the target refr can become obscured by other objects
		// if it moves relative to the player.
		bool checkLOS = !choseClosestResult || !p->pam->IsPerforming(InputAction::kMoveCrosshair);
		// Check once when the chosen refr is first selected.
		bool newSelection = crosshairRefrHandle != a_refrHandle;
		// Initial LOS check.
		if (newSelection && checkLOS) 
		{
			// Can't select if there is no LOS.
			if (!Util::HasLOS(refrPtr.get(), coopActor.get(), true, true, crosshairWorldPos))
			{
				return false;
			}
		}

		// New crosshair refr is on the screen, at least initially.
		if (newSelection)
		{
			crosshairRefrInSight = true;
		}

		// Subsequent on-screen/LOS checks.
		secsSinceVisibleOnScreenCheck = Util::GetElapsedSeconds(p->crosshairRefrVisibilityCheckTP);
		if (secsSinceVisibleOnScreenCheck > Settings::fSecsBetweenTargetVisibilityChecks)
		{
			p->crosshairRefrVisibilityCheckTP = SteadyClock::now();
			bool wasVisible = crosshairRefrInSight;
			bool falseRef = false;
			auto refr3DPtr = Util::GetRefr3D(refrPtr.get());
			// No current 3D -> not on screen or visible.
			if (!refr3DPtr || !refr3DPtr.get()) 
			{
				return false;
			}

			// Check if target is on the screen first.
			// Use three positions on the refr.
			crosshairRefrInSight = Util::PointIsOnScreen(refrPtr->data.location);
			if (!crosshairRefrInSight)
			{
				crosshairRefrInSight = 
				(
					Util::PointIsOnScreen(refr3DPtr->worldBound.center) || 
					Util::PointIsOnScreen(refr3DPtr->world.translate)
				);
			}

			// Then, if the target is on screen and an LOS check is warranted, perform LOS check.
			if (crosshairRefrInSight && checkLOS) 
			{
				crosshairRefrInSight = Util::HasLOS
				(
					refrPtr.get(), coopActor.get(), true, true, crosshairWorldPos
				);
			}

			bool lostVisibility = wasVisible && !crosshairRefrInSight;
			bool notVisible = !crosshairRefrInSight;
			bool regainedVisibility = !wasVisible && crosshairRefrInSight;
			if (lostVisibility)
			{
				// Keep track of when visibility was last lost.
				p->crosshairRefrVisibilityLostTP = SteadyClock::now();
				secsSinceTargetVisibilityLost = 0.0f;
			}
			else if (regainedVisibility)
			{
				// Player regained sight of the refr, so reset lost visibility duration.
				secsSinceTargetVisibilityLost = 0.0f;
			}
			else if (notVisible)
			{
				// Refr is currently not visible, so update lost visibility duration.
				secsSinceTargetVisibilityLost = Util::GetElapsedSeconds
				(
					p->crosshairRefrVisibilityLostTP
				);
			}
		}

		// Give the player a grace period to regain sight of their targeted refr,
		// since the player or the target might move relative to one another
		// and the target may come back into view.
		bool invalidateAfterNotVisible = 
		(
			secsSinceTargetVisibilityLost > Settings::fSecsWithoutLOSToInvalidateTarget
		);
		// Clear invalid target.
		if (invalidateAfterNotVisible)
		{
			// Reset lost visibility duration and time point.
			secsSinceTargetVisibilityLost = 0.0f;
			p->crosshairRefrVisibilityLostTP = SteadyClock::now();
			return false;
		}
		else
		{
			// Crosshair refr is valid.
			return true;
		}
	}

	Raycast::RayResult TargetingManager::PickRaycastHitResult
	(
		const std::vector<Raycast::RayResult>& a_raycastResults,
		const bool& a_inCombat,
		const bool&& a_crosshairActiveForSelection
	)
	{
		// Choose crosshair raycast result to use for setting the crosshair world position 
		// and for selecting the next refr targeted by the player's crosshair.
		// Can filter out players, player teammates, and non-hostile actors when in combat.
		// Also, if choosing a result while the player's crosshair is active for selection,
		// meaning either moving or over a previous valid selection,
		// only choose hits with selectable refrs and ignore activators,
		// which do not have visible surfaces and would result in the crosshair world position 
		// floating in midair if selected.

		// Crosshair raycast result to use for the crosshair world position and hit refr.
		Raycast::RayResult chosenResult{ };
		// Keep track of activator hits.
		bool activatorHit = false;
		// Blacklisted.
		bool excluded = false;
		// Was at least one player teammate hit by the crosshair raycast while in combat?
		// We want to prioritize non-teammate targets while in combat,
		// so that if a player or ally is between the near plane raycast start position
		// and another refr this player is trying to target with the crosshair, 
		// the refr, which is farther away than the teammate,
		// will be chosen as the crosshair target.
		bool alreadyHitATeammateInCombat = false;
		// Continue looking for a raycast that hits another player.
		bool continueSearchingForPlayerTarget = Settings::vbCanTargetOtherPlayers[playerID];
		// Must be in front of the camera to select.
		bool inFrontOfCam = false;
		// The current hit result's hit object is an activator.
		bool isActivator = false;
		// The hit object is faded by the camera.
		bool isFadedByCam = false;

		// No hit to start.
		chosenResult.hit = false;
		chosenResult.hitObjectPtr = nullptr;

		// No NiCam means no bounds-in-frustum checks, so return result early.
		auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr || !niCamPtr.get())
		{
			return chosenResult;
		}

		// Save hit indices to (un)set the closest hit result after the loop.
		uint32_t i = 0;
		int32_t chosenHitResultIndex = -1;
		int32_t chosenHitPosIndex = -1;
		int32_t firstNonActivatorHitIndex = -1;
		for (; i < a_raycastResults.size(); ++i)
		{
			auto& result = a_raycastResults[i];

			// REMOVE after debugging.
			/*RE::NiPoint3 hitPoint = ToNiPoint3(result.hitPos);
			if (p->isPlayer1)
			{
				SPDLOG_DEBUG
				(
					"[TM] PickRaycastHitResult: {}: For target selection: {}. "
					"Pre-parent recurse result {}: hit: {}, {} (refr name: {}, 0x{:X}, type: {}). "
					"Distance to camera: {}, distance to player: {}.",
					coopActor->GetName(),
					a_crosshairActiveForSelection,
					i,
					result.hit,
					result.hitObjectPtr ? result.hitObjectPtr->name.c_str() : "NONE",
					Util::HandleIsValid(result.hitRefrHandle) ? 
					result.hitRefrHandle.get()->GetName() : 
					"NONE",
					Util::HandleIsValid(result.hitRefrHandle) ? 
					result.hitRefrHandle.get()->formID : 
					0xDEAD,
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetObjectReference() ? 
					RE::FormTypeToString
					(
						*result.hitRefrHandle.get()->GetObjectReference()->formType
					) : 
					RE::FormTypeToString(RE::FormType::None),
					hitPoint.GetDistance(niCamPtr->world.translate),
					hitPoint.GetDistance(coopActor->data.location)
				);
			}*/
			
			// Must have hit an NiAVObject.
			if (!result.hit || !result.hitObjectPtr || !result.hitObjectPtr.get())
			{
				continue;
			}

			auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
			// If the hit refr is valid and selectable and not the player,
			// set crosshair refr and positional data.
			if (hitRefrPtr && hitRefrPtr.get())
			{
				// Filter out self, current mount, non-targetable players, and blacklisted actors.
				bool isCoopPlayer = glob.IsCoopPlayer(hitRefrPtr->formID);
				excluded =
				(
					(hitRefrPtr == coopActor) ||
					(hitRefrPtr == p->GetCurrentMount()) ||
					(isCoopPlayer && !Settings::vbCanTargetOtherPlayers[playerID]) ||
					(!isCoopPlayer && glob.coopEntityBlacklistFIDSet.contains(hitRefrPtr->formID))
				);

				// Check three points on the hit refr to see
				// if any of them are in front of the camera.
				// Then if none of those points are in front, 
				// perform a more expensive refr bounds check.
				inFrontOfCam = 
				{
					Util::IsInFrontOfCam(hitRefrPtr->data.location) ||
					Util::IsInFrontOfCam(result.hitObjectPtr->world.translate) ||
					Util::IsInFrontOfCam(result.hitObjectPtr->worldBound.center) ||
					RE::NiCamera::BoundInFrustum(result.hitObjectPtr->worldBound, niCamPtr.get())
				};

				// Faded objects are hit on their 'outward-facing' surface 
				// by the crosshair raycast, which is not a surface visible to the players
				// that are beyond the faded object, so exclude such objects from determining
				// the crosshair's world position and selected refr.
				isFadedByCam = glob.cam->obstructionsToFadeIndicesMap.contains
				(
					result.hitObjectPtr
				);
				if (excluded || !inFrontOfCam || isFadedByCam)
				{
					continue;
				}

				isActivator = 
				{
					(
						hitRefrPtr->GetObjectReference() && 
						hitRefrPtr->GetObjectReference()->Is(RE::FormType::Activator)
					) ||
					(
						result.hitObjectPtr->userData && 
						result.hitObjectPtr->userData->GetObjectReference() && 
						result.hitObjectPtr->userData->GetObjectReference()->Is
						(
							RE::FormType::Activator
						)
					)
				};

				// An activator was hit in this group of hit results.
				if (isActivator && !activatorHit)
				{
					activatorHit = true;
				}

				bool inCombatAndHitTeammate = false;
				// Moving the crosshair, can select a crosshair refr.
				if (a_crosshairActiveForSelection) 
				{
					bool validType = Util::IsSelectableRefr(hitRefrPtr.get());
					if (a_inCombat)
					{
						// TODO:
						// Implement additional teammate combat targeting options.
						auto asActor = hitRefrPtr->As<RE::Actor>();
						// Check if a teammate was hit while in combat.
						inCombatAndHitTeammate = 
						(
							asActor && 
							asActor->IsPlayerTeammate() &&
							!asActor->IsHostileToActor(coopActor.get())
						);
						if (!alreadyHitATeammateInCombat)
						{
							alreadyHitATeammateInCombat = inCombatAndHitTeammate;
						}
						
						// Do not target corpses while in combat.
						validType &= !asActor || !asActor->IsDead();
					}

					// Not a valid type to use for selection,
					// but still set the hit position to use as the crosshair world position
					// if no other result was chosen yet and if the hit object is not an activator.
					if (!validType)
					{
						// Set first non-activator hit index.
						if (firstNonActivatorHitIndex == -1 && !isActivator)
						{
							firstNonActivatorHitIndex = i;
						}

						// Do not set chosen hit pos to activator's hit pos, 
						// since activators have no collision.
						if (!chosenResult.hit && !isActivator) 
						{
							chosenResult.hit = true;
							chosenResult.hitPos = result.hitPos;
							chosenHitPosIndex = i;
						}
						
						// Keep looking for a result to use for crosshair refr selection.
						continue;
					}

					// While in combat, continue searching if this hit result is a teammate.
					// Still set the chosen hit result to the hit teammate's result,
					// just in case no other hits are recorded.

					// Chosen hit result can be set directly at this point.
					chosenResult = result;
					chosenHitPosIndex = chosenHitResultIndex = i;

					// Set first non-activator hit index.
					if (firstNonActivatorHitIndex == -1 && !isActivator)
					{
						firstNonActivatorHitIndex = i;
					}

					// Prioritize non-teammate hits while in combat.
					if (inCombatAndHitTeammate)
					{
						continue;
					}
				
					// Stop iterating through the hit results if another player was hit.
					if (continueSearchingForPlayerTarget && 
						isCoopPlayer && 
						hitRefrPtr != coopActor)
					{
						break;
					}

					// Also stop searching if no activators or no consecutive activators 
					// have been hit so far.
					if (!activatorHit || !isActivator) 
					{
						break;
					}
				}
				else
				{
					// Since we only need the raycast hit result in order to set 
					// the crosshair world position when not attempting to select a refr, 
					// ignore activator hits.
					// Their hit position should not be used for the crosshair world position,
					// since it will affect the target position for drawn projectile trajectories.
					if (isActivator) 
					{
						continue;
					}
					else
					{
						chosenResult = result;
						chosenHitPosIndex = chosenHitResultIndex = i;

						// Set first non-activator hit index.
						if (firstNonActivatorHitIndex == -1)
						{
							firstNonActivatorHitIndex = i;
						}

						break;
					}
				}
			}
			else
			{
				// Hit object is a navmesh block or terrain without an associated refr.
				// Still have to check if in front of the cam and not faded.
				// Not valid for target selection if so.
				inFrontOfCam = 
				{
					Util::IsInFrontOfCam(result.hitObjectPtr->world.translate) ||
					Util::IsInFrontOfCam(result.hitObjectPtr->worldBound.center) ||
					RE::NiCamera::BoundInFrustum(result.hitObjectPtr->worldBound, niCamPtr.get())
				};

				isFadedByCam = glob.cam->obstructionsToFadeIndicesMap.contains
				(
					result.hitObjectPtr
				);
				// Also ignore if a teammate was hit previously while in combat.
				if (!inFrontOfCam || isFadedByCam || alreadyHitATeammateInCombat)
				{
					continue;
				}
				
				// Set first non-activator hit index.
				if (firstNonActivatorHitIndex == -1)
				{
					firstNonActivatorHitIndex = i;
				}

				if (a_crosshairActiveForSelection) 
				{
					if (continueSearchingForPlayerTarget && !chosenResult.hit)
					{
						// First hit.
						// Set as hit position target, even if not selectable, 
						// for aiming purposes.
						chosenResult.hit = true;
						chosenResult.hitPos = result.hitPos;
						chosenHitPosIndex = i;
					}
					else if (!continueSearchingForPlayerTarget && !activatorHit)
					{
						// If no longer searching for another player and no activator was hit,
						// set the hit result to this object.
						chosenResult = result;
						chosenHitResultIndex = chosenHitPosIndex = i;
						break;
					}
				}
				else
				{
					// If crosshair is not active for target selection,
					// choose this hit result and stop looking.
					chosenResult = result;
					chosenHitPosIndex = chosenHitResultIndex = i;
					break;
				}
			}
		}

		// Closest result is the first one.
		//choseClosestResult = i == 0 && chosenResult.hit;
		choseClosestResult = chosenResult.hit && i == firstNonActivatorHitIndex;
		// REMOVE after debugging.
		/*if (p->isPlayer1)
		{
			auto hitRefrPtr = Util::GetRefrPtrFromHandle(chosenResult.hitRefrHandle);
			SPDLOG_DEBUG
			(
				"[TM] PickRaycastHitResult: {}: chose result {}, for hit: {}. "
				"{} (0x{:X}, type: {}). "
				"For target selection: {}, is closest result: {} (first non-activator hit: {}). "
				"Hit a player teammate in combat: {}. Searching for player target: {}.",
				coopActor->GetName(),
				chosenHitResultIndex,
				chosenHitPosIndex,
				hitRefrPtr ?
				hitRefrPtr->GetName() : 
				chosenResult.hitObjectPtr ? 
				chosenResult.hitObjectPtr->name : 
				"NONE",
				hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
				hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
				RE::FormTypeToString(*hitRefrPtr->GetObjectReference()->formType) :
				RE::FormTypeToString(RE::FormType::None),
				a_crosshairActiveForSelection, 
				choseClosestResult, 
				firstNonActivatorHitIndex,
				alreadyHitATeammateInCombat,
				continueSearchingForPlayerTarget
			);
		}*/

		return chosenResult;
	}

	bool TargetingManager::RefrIsInActivationRange(RE::ObjectRefHandle a_refrHandle) const
	{
		// Check if the given refr is within the player's activation range.

		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		// Invalid refr.
		if (!refrPtr || !refrPtr.get()) 
		{
			return false;
		}

		// Can reach objects further away when mounted.
		float maxCheckDist = GetMaxActivationDist();
		bool isDownedPlayer = std::any_of
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(), 
			[&refrPtr](const auto& a_p) 
			{ 
				return a_p->isActive && a_p->coopActor == refrPtr && a_p->isDowned; 
			}
		);
		
		if (isDownedPlayer) 
		{
			// Use downed player revive distance setting.
			return 
			(
				coopActor->data.location.GetDistance(refrPtr->data.location) <= 
				Settings::fMaxDistToRevive
			);
		}
		else
		{
			// Get refr location to use:
			// Default to half-way up the refr,
			// then if the refr is crosshair-selected, use the crosshair's position.
			// Otherwise, use the position corresponding to the refr's 3D center 
			// or half-way up the refr's 3D object.
			RE::NiPoint3 refrLoc = 
			(
				refrPtr->data.location + RE::NiPoint3(0.0f, 0.0f, refrPtr->GetHeight() / 2.0f)
			);
			if (a_refrHandle == crosshairRefrHandle)
			{
				refrLoc = crosshairWorldPos;
			}
			else if (auto refr3DPtr = Util::GetRefr3D(refrPtr.get()); refr3DPtr && refr3DPtr.get())
			{
				refrLoc = refr3DPtr->worldBound.center;
				if (refrLoc.Length() == 0.0f && 
					refr3DPtr->world.translate.Length() != 0.0f)
				{
					refrLoc = 
					(
						refr3DPtr->world.translate + 
						RE::NiPoint3(0.0f, 0.0f, refrPtr->GetHeight() / 2.0f)
					);
				}
			}
		
			return refrLoc.GetDistance(Util::Get3DCenterPos(coopActor.get())) <= maxCheckDist;
		}
	}

	void TargetingManager::ResetTPs()
	{
		// Reset all player timepoints handled by this manager to the current time.

		p->crosshairLastActiveTP				=
		p->lastAutoGrabTP						=
		p->lastCrosshairUpdateTP				=
		p->lastHiddenInStealthRadiusTP			=
		p->lastStealthStateCheckTP				=
		p->crosshairRefrVisibilityLostTP		= 
		p->crosshairRefrVisibilityCheckTP		= SteadyClock::now();
	}

	void TargetingManager::SelectProximityRefr()
	{
		// Choose a valid nearby refr to use for activation.

		const auto playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		// Check if downed player is in activation range, and if so, prioritize selecting them.
		for (const auto& p : glob.coopPlayers) 
		{
			if (p->isActive && p->isDowned && RefrIsInActivationRange(p->coopActor->GetHandle())) 
			{
				proximityRefrHandle = p->coopActor->GetHandle();
				return;
			} 
		}

		// Clear out crosshair pick handle, which will be updated below if valid.
		crosshairPickRefrHandle = RE::ObjectRefHandle();
		const auto currentMount = p->GetCurrentMount();
		// Re-populate nearby references if needed.
		if (nearbyReferences.empty() || ShouldRefreshNearbyReferences())
		{
			// Clear out any cached objects.
			nearbyReferences.clear();

			// Player heading angle in Cartesian convention.
			const float convHeadingAng =
			(
				p->mm->lsMoved ?
				Util::ConvertAngle(p->mm->movementOffsetParams[!MoveParams::kLSGameAng]) :
				Util::ConvertAngle(coopActor->GetHeading(false))
			);

			// Player's facing direction in the XY plane (yaw direction).
			RE::NiPoint3 facingDirXY = Util::RotationToDirectionVect(0.0f, convHeadingAng);
			facingDirXY.Unitize();

			// Max activation reach distance.
			const float maxCheckDist = GetMaxActivationDist();

			// Get each reference within range and record the magnitude of
			// its angular distance from the player's facing angle.
			Util::ForEachReferenceInRange
			(
				playerCenterPos, maxCheckDist, true,
				[
					this, 
					&currentMount, 
					&playerCenterPos, 
					&facingDirXY, 
					&maxCheckDist
				]
				(RE::TESObjectREFR* a_refr) 
				{
					// On to the next one.
					if (!a_refr || 
						!Util::HandleIsValid(a_refr->GetHandle()) || 
						!a_refr->IsHandleValid())
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					auto baseObj = a_refr->GetBaseObject();
					// On to the next one x2.
					if (!baseObj || 
						!a_refr->Is3DLoaded() || 
						!a_refr->GetCurrent3D() ||
						a_refr->IsDeleted()) 
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					auto asActor = a_refr->As<RE::Actor>();
					// Filter out blacklisted actors
					const bool blacklisted =
					{ 
						(currentMount && a_refr == currentMount.get()) ||
						(asActor && asActor->IsPlayerTeammate()) ||
						(glob.coopEntityBlacklistFIDSet.contains(a_refr->formID)) 
					};
					// Useless to activate hostile actors in combat.
					const bool activateHostileActor = 
					{ 
						(asActor && !asActor->IsDead()) && 
						(
							asActor->IsHostileToActor(glob.player1Actor.get()) || 
							asActor->IsHostileToActor(coopActor.get())
						)
					};

					// This reference is an interactable object that is not a blacklisted actor 
					// or a live hostile actor in combat with the player.
					if (blacklisted || 
						activateHostileActor || 
						!Util::IsSelectableRefr(a_refr) || 
						!Util::IsValidRefrForTargeting(a_refr))
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					// Check three points on the refr for the best measurement 
					// of where the refr is located relative to the player.
					// Start with the reported refr location.
					RE::NiPoint3 refrLoc1 = a_refr->data.location;
					RE::NiPoint3 toRefrDirXY = refrLoc1 - playerCenterPos;
					toRefrDirXY.z = 0.0f;
					toRefrDirXY.Unitize();

					// Minimum selection factor [0, 2]. 
					// Get the minimum factor among the (potentially) three refr positions.
					// Negate the dot product, meaning the more the player has to turn 
					// to face the object, the larger the factor.
					// Then we add 1 to ensure all dot product results are > 0, 
					// and mult by 0.5 to set the range to [0, 1]
					// Lastly scale by the distance from the player to the object,
					// meaning objects that are further away have a larger factor.
					// Divide by max reach distance to set range to [0, 1]
					float minSelectionFactor = 
					(
						(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) +
						(playerCenterPos.GetDistance(refrLoc1) / maxCheckDist)
					);

					// Next two positions only exist if the refr's 3D is available.
					std::optional<RE::NiPoint3> refrLoc2 = std::nullopt;
					std::optional<RE::NiPoint3> refrLoc3 = std::nullopt;
					if (auto refr3DPtr = Util::GetRefr3D(a_refr); refr3DPtr && refr3DPtr.get())
					{
						refrLoc2 = refr3DPtr->world.translate;
						refrLoc3 = refr3DPtr->worldBound.center;
					}

					// Refr 3D world position.
					if (refrLoc2.has_value())
					{
						toRefrDirXY = refrLoc2.value() - playerCenterPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();
						float selectionFactor = 
						(
							(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) +
							(playerCenterPos.GetDistance(refrLoc2.value()) / maxCheckDist)
						);
						if (selectionFactor < minSelectionFactor) 
						{
							minSelectionFactor = selectionFactor;
						}
					}

					// Refr 3D bound center position.
					if (refrLoc3.has_value())
					{
						toRefrDirXY = refrLoc3.value() - playerCenterPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();
						float selectionFactor = 
						(
							(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + 
							(playerCenterPos.GetDistance(refrLoc3.value()) / maxCheckDist)
						);
						if (selectionFactor < minSelectionFactor)
						{
							minSelectionFactor = selectionFactor;
						}
					}

					nearbyReferences.insert
					(
						std::pair<float, RE::ObjectRefHandle>
						(
							minSelectionFactor, a_refr->GetHandle()
						)
					);
				}
			);
				
			// Add the game's crosshair pick refr if any.
			if (auto pickData = RE::CrosshairPickData::GetSingleton(); pickData)
			{
				auto pickRefrPtr = Util::GetRefrPtrFromHandle(pickData->target); 
				if (pickRefrPtr && 
					pickRefrPtr.get() && 
					Util::IsValidRefrForTargeting(pickRefrPtr.get()))
				{
					const bool blacklisted =
					(
						(currentMount && pickRefrPtr == currentMount) ||
						(
							pickRefrPtr->As<RE::Actor>() && 
							pickRefrPtr->As<RE::Actor>()->IsPlayerTeammate()
						) ||
						(glob.coopEntityBlacklistFIDSet.contains(pickRefrPtr->formID))
					);
					if (!blacklisted) 
					{
						// Save pick data refr handle.
						crosshairPickRefrHandle = pickData->target;

						// Same three tests as for the nearby refrs above.
						RE::NiPoint3 refrLoc1 = pickRefrPtr->data.location;
						RE::NiPoint3 toRefrDirXY = refrLoc1 - playerCenterPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();

						float minSelectionFactor = 
						(
							(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) +
							(playerCenterPos.GetDistance(refrLoc1) / maxCheckDist)
						);

						std::optional<RE::NiPoint3> refrLoc2 = std::nullopt;
						std::optional<RE::NiPoint3> refrLoc3 = std::nullopt;
						auto refr3DPtr = Util::GetRefr3D(pickRefrPtr.get()); 
						if (refr3DPtr && refr3DPtr.get())
						{
							refrLoc2 = refr3DPtr->world.translate;
							refrLoc3 = refr3DPtr->worldBound.center;
						}

						if (refrLoc2.has_value())
						{
							toRefrDirXY = refrLoc2.value() - playerCenterPos;
							toRefrDirXY.z = 0.0f;
							toRefrDirXY.Unitize();
							float selectionFactor = 
							(
								(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) +
								(playerCenterPos.GetDistance(refrLoc2.value()) / maxCheckDist)
							);
							if (selectionFactor < minSelectionFactor) 
							{
								minSelectionFactor = selectionFactor;
							}
						}

						if (refrLoc3.has_value())
						{
							toRefrDirXY = refrLoc3.value() - playerCenterPos;
							toRefrDirXY.z = 0.0f;
							toRefrDirXY.Unitize();
							float selectionFactor = 
							(
								(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + 
								(playerCenterPos.GetDistance(refrLoc3.value()) / maxCheckDist)
							);
							if (selectionFactor < minSelectionFactor)
							{
								minSelectionFactor = selectionFactor;
							}
						}

						nearbyReferences.insert
						(
							std::pair<float, RE::ObjectRefHandle>
							(
								minSelectionFactor, crosshairPickRefrHandle
							)
						);
					}
				}
			}
		}

		// Clear old proximity refr handle before setting a new one.
		proximityRefrHandle = RE::ObjectRefHandle();

		// Nothing to do if there are no neaby refrs after checking.
		if (nearbyReferences.empty())
		{
			return;
		}

		// Get next selectable refr in view of the camera and remove it from the map.
		while (!nearbyReferences.empty())
		{
			auto nextRefrNodeHandle = nearbyReferences.extract(nearbyReferences.begin());
			if (nextRefrNodeHandle.empty())
			{
				continue;
			}
			
			// Do not select invalid refrs or refrs not in view of the camera.
			// NOTE: 
			// These refrs, aside from the crosshair pick refr, 
			// are not directly selected with a player's crosshair,
			// so LOS still has to be checked.
			const auto& nextRefrHandle = nextRefrNodeHandle.mapped();
			auto nextRefrPtr = Util::GetRefrPtrFromHandle(nextRefrHandle); 
			if (nextRefrPtr && nextRefrPtr.get()) 
			{
				// Finally set the proximity refr if the player has LOS on the refr.
				if (nextRefrHandle == crosshairPickRefrHandle || 
					Util::HasLOS
					(
						nextRefrPtr.get(), coopActor.get(), false, false, crosshairWorldPos
					))
				{
					proximityRefrHandle = nextRefrHandle;
					break;
				}
			}
		}
	}

	void TargetingManager::SetPeriodicCrosshairMessage(const CrosshairMessageType& a_type)
	{
		// Update crosshair text entry to show a periodic message
		// that gives information on the player's targeted crosshair refr
		// or the player's detection level(s) if sneaking.

		// Message text and type to set.
		RE::BSFixedString text = ""sv;
		CrosshairMessageType type = CrosshairMessageType::kNone;
		auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle); 
		if (a_type == CrosshairMessageType::kTargetSelection)
		{
			if (selectedTargetActorPtr && 
				selectedTargetActorPtr.get() && 
				!selectedTargetActorPtr->IsDead())
			{
				// Alive actor. Show name and level.
				// Hostile actors' names are displayed in red.
				auto levelRGB = GetLevelDifferenceRGB(selectedTargetActorHandle);
				text = fmt::format
				(
					"P{}: {} <font color=\"#{:X}\">L{}</font> <font color=\"#{:X}\">{}</font>",
					playerID + 1, p->mm->reqFaceTarget ? "Facing" : "Targeting",
					levelRGB, selectedTargetActorPtr->GetLevel(),
					selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ? 
					0xFF0000 : 
					0xFFFFFF,
					selectedTargetActorPtr->GetDisplayFullName()
				);
				type = a_type;
			}
			else if (auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle); 
					 crosshairRefrPtr && crosshairRefrPtr.get())
			{
				// Get activation text for the crosshair refr.
				RE::BSString activationText = crosshairRefrPtr->GetName();
				if (auto objRef = crosshairRefrPtr->GetObjectReference(); objRef)
				{
					objRef->GetActivateText(crosshairRefrPtr.get(), activationText);
				}

				text = fmt::format("P{}: {}", playerID + 1, activationText);
				type = a_type;
			}
		}
		else if (a_type == CrosshairMessageType::kStealthState)
		{
			if (coopActor->IsSneaking())
			{
				const auto isInCombat = coopActor->IsInCombat();
				const bool checkSelectedTarget = 
				(
					selectedTargetActorPtr &&
					selectedTargetActorPtr.get() && 
					!selectedTargetActorPtr->IsDead()
				);
				// Set sneak info text to indicate the player's hidden percent,
				// which is determined by their remaining stealth points:
				// 
				// player's total stealth points - 
				// max(all aggro'd actors' stealth point decrements)
				
				// If a particular target actor is selected, 
				// show their individual detection level of the player as well.
				if (checkSelectedTarget)
				{
					const bool isPlayer = GlobalCoopData::IsCoopPlayer
					(
						selectedTargetActorPtr.get()
					);
					float targetDetectionPct = static_cast<uint8_t>
					(
						Util::GetDetectionPercent(coopActor.get(), selectedTargetActorPtr.get())
					);
					uint32_t targetDetectionPctRGB = GetDetectionLvlRGB(targetDetectionPct, false);
					// Set sneak info text to indicate the currently selected/aim correction
					// target's  detection level of the player 
					// and the overall detection percentage of the player
					// for all relevant high process actors.
					// Hostile actors have their names reported in red.
					text = fmt::format
					(
						"P{}: Detected by <font color=\"#{:X}\">{}</font> "
						"(<font color=\"#{:X}\">{}%</font>), overall "
						"(<font color=\"#{:X}\">{}%</font>)",
						playerID + 1,
						!isPlayer && selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ?
						0xFF0000 : 
						0xFFFFFF,
						selectedTargetActorPtr->GetDisplayFullName(),
						targetDetectionPctRGB, 
						targetDetectionPct,
						detectionPctRGB,
						detectionPct
					);
				}
				else
				{
					// Detection percent reported accounts for all relevant actors 
					// in the high process.
					text = fmt::format
					(
						"P{}: Detected (<font color=\"#{:X}\">{}%</font>)",
						playerID + 1, detectionPctRGB, detectionPct
					);
				}

				type = a_type;
			}
		}

		SetCurrentCrosshairMessage
		(
			false,
			std::move(type),
			text, 
			{ CrosshairMessageType::kNone }, 
			0.25f
		);
	}

	bool TargetingManager::ShouldRefreshNearbyReferences()
	{
		// Check if a list of nearby refrs should be re-calculated 
		// when getting the next proximity refr to activate.

		const RE::NiPoint3& playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		const float& facingAngle = coopActor->GetHeading(false);
		// Refresh if either far away from previous activation position,
		// or if turning away enough from last activation rotation.
		bool facingNewDir = 
		(
			fabsf(Util::NormalizeAngToPi(facingAngle - lastActivationFacingAngle)) > 
			Settings::fMinTurnAngToRefreshRefrs
		);
		bool farAwayFromPrevSpot = 
		(
			playerCenterPos.GetDistance(lastActivationReqPos) > 
			Settings::fMinMoveDistToRefreshRefrs
			
		);
		return facingNewDir || farAwayFromPrevSpot;
	}

	void TargetingManager::UpdateAimCorrectionTarget()
	{
		// Update aim correction target if the player is attempting
		// to perform or is performing a ranged attack.
		// Clear the target otherwise.

		// Player must have aim correction enabled.
		if (!Settings::vbUseAimCorrection[playerID])
		{
			return;
		}
		
		// First, if the current target is no longer selectable, clear it.
		auto currentTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle);
		bool isNoLongerTargetable = 
		(
			currentTargetPtr && 
			currentTargetPtr.get() &&
			!Util::IsValidRefrForTargeting(currentTargetPtr.get())
		);
		if (isNoLongerTargetable)
		{
			ClearTarget(TargetActorType::kAimCorrection);
			return;
		}

		// Player is trying to/is performing/just finished an action that requires having a target.
		bool rangedAttackOrBlockRequest = 
		{
			(p->pam->isAttacking) ||
			(p->pam->isBlocking) ||
			(p->pam->isBashing) ||
			(p->pam->isInCastingAnim) ||
			(p->pam->isCastingLH) ||
			(p->pam->isCastingRH) ||
			(p->pam->isCastingDual) ||
			(
				(p->em->Has2HRangedWeapEquipped() || p->em->HasRHStaffEquipped()) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kAttackRH, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kAttackRH)
				)
			) ||
			(
				(p->em->HasRHSpellEquipped()) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kCastRH, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kCastRH)
				)
			) ||
			(
				(p->em->HasLHSpellEquipped()) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kCastLH, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kCastLH)
				)
			) ||
			(
				(p->em->HasLHStaffEquipped()) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kAttackLH, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kAttackLH)
				)
			) ||
			(
				(p->em->quickSlotSpell) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kQuickSlotCast, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kQuickSlotCast)
				)
			) ||
			(
				(p->em->voiceSpell) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kShout, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kShout)
				)
			) ||
			(
				(
					p->pam->reqSpecialAction == SpecialActionType::kCastBothHands || 
					p->pam->reqSpecialAction == SpecialActionType::kDualCast || 
					p->pam->reqSpecialAction == SpecialActionType::kQuickCast
				) &&
				(
					p->pam->GetPlayerActionInputJustReleased(InputAction::kSpecialAction, false) ||
					p->pam->AllInputsPressedForAction(InputAction::kSpecialAction)
				)
			)
		};
		
		const auto& lsState = glob.cdh->GetAnalogStickState(controllerID, true);
		auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle); 
		// Can only check for a new target if the LS is either fully displaced or centered,
		// and the player is performing a ranged attack, not facing the crosshair position,
		// and has not selected a target actor with their crosshair.
		bool canSelectTarget = 
		(
			lsState.normMag > 0.0f && 
			lsState.normMag - lsState.prevNormMag >= -1E-3f &&
			rangedAttackOrBlockRequest && 
			!p->mm->reqFaceTarget && 
			!selectedTargetActorPtr
		);
		if (canSelectTarget)
		{
			// Trying to perform/performing ranged attack,
			// no selected actor, and not facing the crosshair position.
			auto nextTargetPtr = Util::GetActorPtrFromHandle
			(
				GetClosestTargetableActorInFOV
				(
					Settings::vfAimCorrectionFOV[playerID]/*,
					RE::ObjectRefHandle(),
					false,
					Settings::fMaxRaycastAndZoomOutDistance*/
				)
			);
			bool diffTarget = nextTargetPtr != currentTargetPtr;
			if ((diffTarget) && (!nextTargetPtr || !nextTargetPtr.get()))
			{
				// Clear current target if there is no next target while moving away.
				ClearTarget(TargetActorType::kAimCorrection);
			}
			else
			{	
				// Set valid, different target that is within LOS of the player.
				bool canSet = 
				(
					(
						nextTargetPtr && 
						nextTargetPtr.get() &&
						diffTarget && 
						Util::IsValidRefrForTargeting(nextTargetPtr.get())
					) &&
					Util::HasLOS
					(
						nextTargetPtr.get(), coopActor.get(), true, false, crosshairWorldPos
					)	
				);
				if (canSet)
				{
					aimCorrectionTargetHandle = nextTargetPtr->GetHandle();
				}
				else if (currentTargetPtr && currentTargetPtr.get())
				{
					// Check to see if the current target should be cleared
					// if it is not within the player's FOV window.
					// Angle around which the FOV window is centered.
					auto targetingAngle = 
					(
						p->mm->lsMoved ? 
						p->mm->movementOffsetParams[!MoveParams::kLSGameAng] :
						p->pam->isAttacking ? 
						Util::DirectionToGameAngYaw(p->mm->playerDefaultAttackSourceDir) :
						coopActor->data.angle.z
					);
					// Within FOV.
					const float turnToFaceActorAngMag = fabsf
					(
						Util::NormalizeAngToPi
						(
							Util::GetYawBetweenPositions
							(
								coopActor->data.location, currentTargetPtr->data.location
							) - 
							targetingAngle
						)
					);
					const bool currentTargetInSelectionFOV = 
					(
						turnToFaceActorAngMag <= (Settings::vfAimCorrectionFOV[playerID] / 2.0f)
					);
					
					// New close actor is invalid for use as an aim correction target,
					// and the current target is not in the selection FOV,
					// so clear the current one, 
					// since the player is moving away from the current target.
					if (!currentTargetInSelectionFOV)
					{
						ClearTarget(TargetActorType::kAimCorrection);
					}
				}
			}
		}
		else if (!rangedAttackOrBlockRequest)
		{
			// Clear aim correction target when not attacking or trying to attack, blocking,
			// or when a crosshair target actor is selected.
			const auto& combatGroup = glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at
			(
				ActionGroup::kCombat
			);
			bool combatActionBindsPressed = false;
			if (coopActor->IsWeaponDrawn()) 
			{
				for (auto actionIndex : combatGroup)
				{
					combatActionBindsPressed |= p->pam->AllInputsPressedForAction
					(
						static_cast<InputAction>(actionIndex)
					);
					if (combatActionBindsPressed)
					{
						break;
					}
				}
			}
			 
			if ((currentTargetPtr && currentTargetPtr.get()) && 
				((selectedTargetActorPtr) || (!combatActionBindsPressed && !p->pam->isAttacking)))
			{
				ClearTarget(TargetActorType::kAimCorrection);
			}
		}
	}

	bool TargetingManager::UpdateAimTargetLinkedRefr(const EquipIndex& a_attackSlot)
	{
		// Set the aim target linked reference for the player's ranged attack package
		// based on what form is in the given equip slot.
		// Then return true if a new one was set or the old one was cleared.

		// Requires the aim target keyword to set the aim target linked refr.
		if (!p->aimTargetKeyword)
		{
			return false;
		}

		// Check the given attack slot for a ranged weapon/spell.
		auto weapMagObj = p->em->equippedForms[!a_attackSlot];
		bool isRanged = false;
		if (weapMagObj && weapMagObj->IsWeapon())
		{
			auto weap = weapMagObj->As<RE::TESObjectWEAP>();
			isRanged = 
			(
				weap->IsBow() || weap->IsCrossbow() || weap->IsStaff()
			);
		}
		else if (weapMagObj)
		{
			isRanged = weapMagObj->IsMagicItem() || weapMagObj->As<RE::TESShout>();
		}

		if (isRanged)
		{
			auto currentTargetRefrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle);
			auto newTargetRefrPtr = Util::GetRefrPtrFromHandle
			(
				GetRangedPackageTargetRefr(weapMagObj)
			);
			bool newTargetIsValid = 
			(
				newTargetRefrPtr && 
				newTargetRefrPtr.get() &&
				Util::IsValidRefrForTargeting(newTargetRefrPtr.get())
			);
			if (newTargetIsValid && newTargetRefrPtr != currentTargetRefrPtr)
			{
				// Set new valid linked refr.
				coopActor->extraList.SetLinkedRef(newTargetRefrPtr.get(), p->aimTargetKeyword);
				aimTargetLinkedRefrHandle = newTargetRefrPtr->GetHandle();
			}
			else if (!newTargetIsValid && currentTargetRefrPtr)
			{
				// Clear old linked refr if no new one was selected.
				coopActor->extraList.SetLinkedRef(nullptr, p->aimTargetKeyword);
				aimTargetLinkedRefrHandle.reset();
			}

			return currentTargetRefrPtr != newTargetRefrPtr;
		}

		return false;
	}

	void TargetingManager::UpdateAnimatedCrosshairInterpData() 
	{
		// Update crosshair rotation and oscillation interpolation data
		// to animate the crosshair.
		
		// Interpolated rotation of the crosshair when toggling face target mode.
		const bool& facingTarget = p->mm->reqFaceTarget;
		// Rotate 45 degrees when fully facing the crosshair position.
		float endPointAng = facingTarget ? PI / 4.0f : 0.0f;
		crosshairRotationData->next = endPointAng;
		if (crosshairRotationData->current != endPointAng)
		{
			crosshairRotationData->InterpolateSmootherStep
			(
				min
				(
					crosshairRotationData->secsSinceUpdate / 
					crosshairRotationData->secsUpdateInterval, 
					1.0f
				)
			);
			crosshairRotationData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
			if (crosshairRotationData->current == endPointAng)
			{
				// Fully rotated.
				crosshairRotationData->SetUpdateDurationAsComplete();
				crosshairRotationData->SetData(endPointAng, endPointAng, endPointAng);
			}
		}

		// Interpolated motion of crosshair expansion and contraction.
		const float& crosshairLength = Settings::vfCrosshairLength[playerID];
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];
		// Current interpolated gap.
		const float currentCrosshairGap = 
		(
			Settings::vfCrosshairGapRadius[playerID] + crosshairOscillationData->current
		);
		// Crosshair gap at max expansion.
		const float maxCrosshairGap = max
		(
			crosshairLength,
			Settings::vfCrosshairGapRadius[playerID] * 2.0f
		);
		// Includes inner outline, prong itself, and current interpolated gap.
		float currentProngDistFromCenter = 
		(
			2.0f * crosshairThickness + currentCrosshairGap + crosshairLength
		);
		// Check if the expanding/contracting crosshair is near the edge of the screen.
		const bool isNearEdgeOfScreen = 
		{ 
			crosshairScaleformPos.x <= currentProngDistFromCenter || 
			crosshairScaleformPos.x >= DebugAPI::screenResX - currentProngDistFromCenter ||
			crosshairScaleformPos.y <= currentProngDistFromCenter || 
			crosshairScaleformPos.y >= DebugAPI::screenResY - currentProngDistFromCenter 
		};
		// New gap value to set.
		float endPointGapDelta = crosshairOscillationData->next;
		// Do not oscillate when moving the crosshair and not near the edge of the screen.
		if (p->pam->IsPerforming(InputAction::kMoveCrosshair) && !isNearEdgeOfScreen)
		{
			endPointGapDelta = 0.0f;
		}
		else if (crosshairOscillationData->current == endPointGapDelta)
		{
			// Switch gap delta endpoint when reached.
			endPointGapDelta = endPointGapDelta == 0.0f ? maxCrosshairGap : 0.0f;
		}

		// Interpolation endpoint changed, signal state change.
		if (crosshairOscillationData->next != endPointGapDelta)
		{
			crosshairOscillationData->SetTimeSinceUpdate(0.0f);
			crosshairOscillationData->ShiftEndpoints(endPointGapDelta);
		}

		// Set new target interpolation endpoint.
		crosshairOscillationData->next = endPointGapDelta;
		// Update the current interpolated gap value 
		// if it hasn't reached the interpolation endpoint yet.
		if (crosshairOscillationData->current != endPointGapDelta)
		{
			crosshairOscillationData->InterpolateSmootherStep
			(
				min
				(
					crosshairOscillationData->secsSinceUpdate / 
					crosshairOscillationData->secsUpdateInterval, 
					1.0f
				)
			);
			crosshairOscillationData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);

			// Reached the interpolation endpoint now, so signal completion,
			// and set previous, current, and next interpolation values to the endpoint value.
			if (crosshairOscillationData->current == endPointGapDelta)
			{
				crosshairOscillationData->SetUpdateDurationAsComplete();
				crosshairOscillationData->SetData
				(
					endPointGapDelta, endPointGapDelta, endPointGapDelta
				);
			}
		}
	}

	void TargetingManager::UpdateCrosshairPosAndSelection()
	{
		// Update the player's crosshair position and selected refr data.

		auto niCamPtr = Util::GetNiCamera();
		auto ui = RE::UI::GetSingleton();
		// Overlay menu to draw the crosshair on.
		auto overlay = ui ? ui->GetMenu(DebugOverlayMenu::MENU_NAME) : nullptr;
		auto view = overlay ? overlay->uiMovie : nullptr;
		if (!niCamPtr || !niCamPtr.get() || !ui || !overlay || !view)
		{
			// Set last update time point before returning early.
			p->lastCrosshairUpdateTP = SteadyClock::now();
			return;
		}

		// Get dimensions from the view's visible frame.
		auto gRect = view->GetVisibleFrameRect();
		const float rectWidth = fabsf(gRect.right - gRect.left);
		const float rectHeight = fabsf(gRect.bottom - gRect.top);
		const bool isMovingCrosshair = p->pam->IsPerforming(InputAction::kMoveCrosshair);
		// Only living actors are selectable when in combat.
		// Check if any player in the party is in combat.
		bool playerInCombat = std::any_of
		(
			glob.coopPlayers.begin(), glob.coopPlayers.end(), 
			[](const auto& a_p) 
			{
				return a_p->isActive && a_p->coopActor->IsInCombat(); 
			}
		);

		// Previously selected refr and crosshair 2D position to compare against for changes.
		prevCrosshairRefrHandle = crosshairRefrHandle;
		glm::vec3 prevCrosshairScaleformPos = crosshairScaleformPos;
		auto prevCrosshairRefrPtr = Util::GetRefrPtrFromHandle(prevCrosshairRefrHandle);
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle);

		//====================================
		// [Crosshair Movement and Selection]:
		//====================================

		// When the player wants to move their crosshair,
		// update the crosshair's 2D and 3D crosshair positions, 
		// and the selected crosshair refr and actor, if any.
		if (isMovingCrosshair)
		{
			// Get RS data.
			const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
			const auto& rsX = rsData.xComp;
			// Scaleform Y is inverted with respect to the analog stick's Y axis.
			const auto& rsY = -rsData.yComp;
			const auto& rsMag = rsData.normMag * rsData.normMag;
			const float secsSinceCrosshairUpdated = Util::GetElapsedSeconds
			(
				p->lastCrosshairUpdateTP
			);

			// Max pixels per second that the crosshair can travel across 
			// along the X and Y screen axes.
			float crosshairMaxXSpeedPPS = 
			(
				Settings::vfCrosshairHorizontalSensitivity[playerID] * 
				Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			float crosshairMaxYSpeedPPS = 
			(
				Settings::vfCrosshairVerticalSensitivity[playerID] *
				Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			// X and Y crosshair speed multipliers which are adjusted if the player 
			// is moving their crosshair across a selected refr with crosshair magnetism enabled.
			float crosshairSpeedMultX = 1.0f;
			float crosshairSpeedMultY = 1.0f;
			const auto port = Util::GetPort();
			// Slow down the moving crosshair when an actor or refr is selected.
			// NOTE: VERY rough estimates of pixel dimensions. Will be improved in the future.
			bool adjustMovementSpeedOverActor = 
			(
				Settings::vbCrosshairMagnetismForActors[playerID] && 
				selectedTargetActorPtr && 
				selectedTargetActorPtr.get()
			);
			bool adjustMovementSpeedOverRefr = 
			(
				Settings::vbCrosshairMagnetismForObjRefs[playerID] && 
				crosshairRefrPtr &&
				crosshairRefrPtr.get()
			);
			if (adjustMovementSpeedOverActor || adjustMovementSpeedOverRefr)
			{
				const auto& targetPtr = 
				(
					adjustMovementSpeedOverActor ? selectedTargetActorPtr : crosshairRefrPtr
				);
				float pixelHeight = Util::GetBoundPixelDist(targetPtr.get(), true);
				float pixelWidth = Util::GetBoundPixelDist(targetPtr.get(), false);
				// Do not modify speed mults if the selected refr is too narrow,
				// or if the minimum traversal time or max pixels per second
				// are erroneously set to 0 or below.
				crosshairSpeedMultX = 
				(
					(
						pixelWidth == 1.0f ||
						Settings::vfMinSecsCrosshairTargetTraversal[playerID] <= 0.0f || 
						crosshairMaxXSpeedPPS <= 0.0
					) ?
					1.0f :
					min
					(
						pixelWidth / 
						(
							crosshairMaxXSpeedPPS * 
							Settings::vfMinSecsCrosshairTargetTraversal[playerID]
						), 
						1.0f
					)
				);
				crosshairSpeedMultY = 
				(
					(
						pixelHeight == 1.0f || 
						Settings::vfMinSecsCrosshairTargetTraversal[playerID] == 0.0f || 
						crosshairMaxYSpeedPPS == 0.0f 
					)?
					1.0f :
					min
					(
						pixelHeight / 
						(
							crosshairMaxYSpeedPPS *
							Settings::vfMinSecsCrosshairTargetTraversal[playerID]
						),
						1.0f
					)
				);
			}

			// In the case that true the raycast-collidable distance across the target
			// is larger than the estimated bound height/width,
			// the crosshair will move much too slow, 
			// so set a lower bound to the crosshair speed mults to keep things moving.
			crosshairSpeedMultX = max(0.1f, crosshairSpeedMultX);
			crosshairSpeedMultY = max(0.1f, crosshairSpeedMultY);
			// Number of pixels to move across in the X and Y directions this update.
			float deltaXPx = 
			(
				crosshairSpeedMultX * 
				rsX * 
				rsMag * 
				secsSinceCrosshairUpdated * 
				crosshairMaxXSpeedPPS
			);
			float deltaYPx = 
			(
				crosshairSpeedMultY * 
				rsY * 
				rsMag *
				secsSinceCrosshairUpdated * 
				crosshairMaxYSpeedPPS
			);

			// When moving over a refr, add the pixel deltas 
			// relative to the initial 'entry' position 
			// which was set when the crosshair first selected the refr. 
			// This will allow the crosshair to 'stick' to moving targets
			// while moving it across the target,
			// since the change in pixels is made relative to the target's movement.
			if (crosshairRefrPtr && crosshairRefrPtr.get())
			{
				auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
				// Set the hit position's local offset from the hit refr's base position.
				// Base position is the torso position for actors 
				// and the center position for all other refrs.
				const auto refrBasePos = 
				(
					hitActor ? 
					Util::GetTorsoPosition(hitActor) : 
					Util::Get3DCenterPos(crosshairRefrPtr.get())
				);
				// Get updated world position by adding the stored local offset 
				// to the refr's reported base position.
				auto newBaseCrosshairWorldPos = refrBasePos + crosshairLocalPosOffset;
				// Get corresponding screen position.
				auto screenPos = Util::WorldToScreenPoint3(newBaseCrosshairWorldPos);
				// Add deltas to this base screen position 
				// to allow the crosshair to move relative to the selected refr.
				crosshairScaleformPos.x = std::clamp(screenPos.x + deltaXPx, 0.0f, rectWidth);
				crosshairScaleformPos.y = std::clamp(screenPos.y + deltaYPx, 0.0f, rectHeight);
				crosshairScaleformPos.z = 0.0f;
			}
			else
			{
				// Update scaleform position directly with the pixel deltas.
				crosshairScaleformPos.x = std::clamp
				(
					crosshairScaleformPos.x + deltaXPx, 0.0f, rectWidth
				);
				crosshairScaleformPos.y = std::clamp
				(
					crosshairScaleformPos.y + deltaYPx, 0.0f, rectHeight
				);
				crosshairScaleformPos.z = 0.0f;
			}

			// Clear selected actor and crosshair refr
			// before checking for raycast/proximity refr hits below.
			selectedTargetActorHandle = RE::ActorHandle();
			crosshairRefrHandle = RE::ObjectRefHandle();

			// Calculate near and far plane world positions for the current scaleform position.
			glm::mat4 pvMat{};
			// Transpose first.
			pvMat[0][0] = niCamPtr->worldToCam[0][0];
			pvMat[1][0] = niCamPtr->worldToCam[0][1];
			pvMat[2][0] = niCamPtr->worldToCam[0][2];
			pvMat[3][0] = niCamPtr->worldToCam[0][3];
			pvMat[0][1] = niCamPtr->worldToCam[1][0];
			pvMat[1][1] = niCamPtr->worldToCam[1][1];
			pvMat[2][1] = niCamPtr->worldToCam[1][2];
			pvMat[3][1] = niCamPtr->worldToCam[1][3];
			pvMat[0][2] = niCamPtr->worldToCam[2][0];
			pvMat[1][2] = niCamPtr->worldToCam[2][1];
			pvMat[2][2] = niCamPtr->worldToCam[2][2];
			pvMat[3][2] = niCamPtr->worldToCam[2][3];
			pvMat[0][3] = niCamPtr->worldToCam[3][0];
			pvMat[1][3] = niCamPtr->worldToCam[3][1];
			pvMat[2][3] = niCamPtr->worldToCam[3][2];
			pvMat[3][3] = niCamPtr->worldToCam[3][3];
			// Then invert.
			auto invPVMat = glm::inverse(pvMat);
			// Causes crosshair jitter if the Z component is set to +-1, 
			// so they're set reasonably close to those values instead.
			glm::vec4 clipSpacePosNear = glm::vec4
			(
				crosshairScaleformPos.x / (rectWidth * 0.5f) - 1.0f, 
				1.0f - crosshairScaleformPos.y / (rectHeight * 0.5f), 
				-0.999999f, 
				1.0f
			);
			glm::vec4 clipSpacePosFar = glm::vec4
			(
				crosshairScaleformPos.x / (rectWidth * 0.5f) - 1.0f, 
				1.0f - crosshairScaleformPos.y / (rectHeight * 0.5f), 
				0.999999f, 
				1.0f
			);
			// Derive world positions using the inverted projection view matrix
			// and the clip space near/far vectors.
			glm::vec4 worldPosNear = (invPVMat * clipSpacePosNear);
			glm::vec4 worldPosFar = (invPVMat * clipSpacePosFar);
			worldPosNear /= worldPosNear.w;
			worldPosFar /= worldPosFar.w;

			// Set initial crosshair world position to the far plane point.
			crosshairWorldPos = ToNiPoint3(worldPosFar);
			// Raycast for selectable refrs. Get all hits from near to far plane points.
			auto results = Raycast::GetAllHavokCastHitResults(worldPosNear, worldPosFar);
			// Pick a hit result with a potentially-selectable refr.
			Raycast::RayResult centerResult = PickRaycastHitResult(results, playerInCombat, true);
			// Clear valid flag since we'll be updating it below if the chosen hit was valid.
			validCrosshairRefrHit = false;
			// Only need to check the result if it has a hit.
			if (centerResult.hit)
			{
				// Update crosshair world pos, regardless of whether or not 
				// the raycast hits anything selectable.
				crosshairWorldPos = ToNiPoint3(centerResult.hitPos);
				if (Util::HandleIsValid(centerResult.hitRefrHandle))
				{
					// Must be valid for selection.
					validCrosshairRefrHit = IsRefrValidForCrosshairSelection
					(
						centerResult.hitRefrHandle
					); 
					if (validCrosshairRefrHit)
					{
						// Set crosshair refr handle.
						crosshairRefrHandle = centerResult.hitRefrHandle;
						crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
						auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
						// Set selected actor handle if the hit refr is an actor.
						if (hitActor)
						{
							selectedTargetActorHandle = hitActor->GetHandle();
						}
					
						const auto refrBasePos = 
						(
							hitActor ? 
							Util::GetTorsoPosition(hitActor) : 
							Util::Get3DCenterPos(crosshairRefrPtr.get())
						);
						// The local position offset to apply is the same as 
						// the movement offset when the crosshair is moved.
						crosshairLocalPosOffset = 
						crosshairMovementHitPosOffset = crosshairWorldPos - refrBasePos;

						crosshairLocalPosPitchDiff = Util::NormalizeAngToPi
						(
							Util::GetPitchBetweenPositions(refrBasePos, crosshairWorldPos) - 
							crosshairRefrPtr->data.angle.x
						);
						crosshairLocalPosYawDiff = Util::NormalizeAng0To2Pi
						(
							Util::GetYawBetweenPositions(refrBasePos, crosshairWorldPos) - 
							crosshairRefrPtr->data.angle.z
						);
					}
				}
			}
		}
		else if (crosshairRefrPtr)
		{
			// Refr selected when not moving the crosshair.
			// While not moving the crosshair, 
			// stick the crosshair to the target until it becomes invalid.

			// Check if targeted refr is still selectable and valid.
			validCrosshairRefrHit = 
			(
				IsRefrValidForCrosshairSelection(crosshairRefrHandle) && 
				Util::IsSelectableRefr(crosshairRefrPtr.get())
			);
			if (validCrosshairRefrHit)
			{
				// Update the crosshair world position using
				// the initial local hit position and the refr's new position.
				auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
				const auto refrBasePos = 
				(
					hitActor ? 
					Util::GetTorsoPosition(hitActor) : 
					Util::Get3DCenterPos(crosshairRefrPtr.get())
				);

				// Update local positional offset so that the crosshair stays attached
				// to the crosshair refr at the same position 
				// (originally set while moving the crosshair)
				// relative to the crosshair refr's facing angle.
				// Maintain the same original distance from the refr base position.
				crosshairLocalPosOffset =
				(
					Util::RotationToDirectionVect
					(
						-Util::NormalizeAngToPi
						(
							crosshairRefrPtr->data.angle.x + crosshairLocalPosPitchDiff
						),
						Util::ConvertAngle
						(
							Util::NormalizeAng0To2Pi
							(
								crosshairRefrPtr->data.angle.z + crosshairLocalPosYawDiff
							)
						)
					) * 
					crosshairMovementHitPosOffset.Length()
				);
				// Offset the base position by the new offset 
				// to get the next crosshair world position.
				crosshairWorldPos = refrBasePos + crosshairLocalPosOffset;
				// Update the crosshair's scaleform position based on its new world position.
				const glm::vec2 temp = DebugAPI::WorldToScreenPoint(ToVec3(crosshairWorldPos));
				crosshairScaleformPos.x = temp.x;
				crosshairScaleformPos.y = temp.y;
			}
			else
			{
				// No longer valid, time to reset data.
				// Set previous refr handle.
				prevCrosshairRefrHandle = crosshairRefrHandle;
				// Clear out selected actor, refr, and initial hit local position.
				// Then set pixel deltas to 0.
				selectedTargetActorHandle = RE::ActorHandle();
				crosshairRefrHandle = RE::ObjectRefHandle();
				crosshairLocalPosOffset = crosshairMovementHitPosOffset = RE::NiPoint3();
			}
		}
		else
		{
			// No targeted refr, so we only have to update the crosshair world position.

			// Calculate near and far plane world positions for the current scaleform position.
			glm::mat4 pvMat{};
			// Transpose first.
			pvMat[0][0] = niCamPtr->worldToCam[0][0];
			pvMat[1][0] = niCamPtr->worldToCam[0][1];
			pvMat[2][0] = niCamPtr->worldToCam[0][2];
			pvMat[3][0] = niCamPtr->worldToCam[0][3];
			pvMat[0][1] = niCamPtr->worldToCam[1][0];
			pvMat[1][1] = niCamPtr->worldToCam[1][1];
			pvMat[2][1] = niCamPtr->worldToCam[1][2];
			pvMat[3][1] = niCamPtr->worldToCam[1][3];
			pvMat[0][2] = niCamPtr->worldToCam[2][0];
			pvMat[1][2] = niCamPtr->worldToCam[2][1];
			pvMat[2][2] = niCamPtr->worldToCam[2][2];
			pvMat[3][2] = niCamPtr->worldToCam[2][3];
			pvMat[0][3] = niCamPtr->worldToCam[3][0];
			pvMat[1][3] = niCamPtr->worldToCam[3][1];
			pvMat[2][3] = niCamPtr->worldToCam[3][2];
			pvMat[3][3] = niCamPtr->worldToCam[3][3];
			// Then invert.
			auto invPVMat = glm::inverse(pvMat);
			// Causes crosshair jitter if the Z component is set to +-1,
			// so they're set reasonably close to those values instead.
			glm::vec4 clipSpacePosNear = glm::vec4
			(
				crosshairScaleformPos.x / (rectWidth * 0.5f) - 1.0f,
				1.0f - crosshairScaleformPos.y / (rectHeight * 0.5f),
				-0.999999f,
				1.0f
			);
			glm::vec4 clipSpacePosFar = glm::vec4
			(
				crosshairScaleformPos.x / (rectWidth * 0.5f) - 1.0f,
				1.0f - crosshairScaleformPos.y / (rectHeight * 0.5f),
				0.999999f,
				1.0f
			);
			// Derive world positions using the inverted projection view matrix 
			// and the clip space vectors.
			glm::vec4 worldPosNear = (invPVMat * clipSpacePosNear);
			glm::vec4 worldPosFar = (invPVMat * clipSpacePosFar);
			worldPosNear /= worldPosNear.w;
			worldPosFar /= worldPosFar.w;

			// Set initial crosshair world position to the far plane point.
			crosshairWorldPos = ToNiPoint3(worldPosFar);
			// Raycast for selectable refrs. Get all hits from near to far plane points.
			auto results = Raycast::GetAllHavokCastHitResults(worldPosNear, worldPosFar);
			// Get a valid result that does not have to contain a selectable refr.
			Raycast::RayResult centerResult = PickRaycastHitResult(results, playerInCombat, false);
			// Set crosshair world position on hit.
			if (centerResult.hit)
			{
				crosshairWorldPos = ToNiPoint3(centerResult.hitPos);
			}
		}

		//=======================================
		// [Crosshair Activity and Re-centering]:
		//=======================================

		// Check if the crosshair is being actively or passively
		// adjusted by the player in some way.
		bool prevScaleformPosOnEdgeOfScreen = 
		{
			prevCrosshairScaleformPos.x >= rectWidth || prevCrosshairScaleformPos.x <= 0.0f ||
			prevCrosshairScaleformPos.y >= rectHeight || prevCrosshairScaleformPos.y <= 0.0f
		};
		bool scaleformPosOnEdgeOfScreen = 
		{ 
			crosshairScaleformPos.x >= rectWidth || crosshairScaleformPos.x <= 0.0f ||
			crosshairScaleformPos.y >= rectHeight || crosshairScaleformPos.y <= 0.0f 
		};
		auto playerCam = RE::PlayerCamera::GetSingleton();
		// Is a player rotating the co-op camera or is P1 rotating the default TP camera?
		bool isCamRotating = 
		{
			(glob.cam->IsRunning()) ?
			(
				glob.cam->camAdjMode == CamAdjustmentMode::kRotate && 
				glob.cam->controlCamCID != -1 && 
				glob.coopPlayers[glob.cam->controlCamCID]->pam->IsPerforming
				(
					InputAction::kRotateCam
				)
			) :
			(
				(playerCam) && 
				(playerCam->rotationInput.x != 0.0f || playerCam->rotationInput.y != 0.0f)
			)
		};

		// The crosshair is considered active when moving the crosshair,
		// when facing a target, when selecting a new target,
		// or when first hitting the edge of the screen.
		if ((crosshairRefrPtr) ||
			(isMovingCrosshair || p->mm->reqFaceTarget) ||	
			(crosshairRefrPtr != prevCrosshairRefrPtr) ||
			(scaleformPosOnEdgeOfScreen && !prevScaleformPosOnEdgeOfScreen))
		{
			p->crosshairLastActiveTP = SteadyClock::now();
		}

		// Re-center the crosshair after an interval passes if there is no valid target,
		// the player is not moving their crosshair, 
		// and the player is not facing the crosshair world position.
		if (Settings::vbRecenterInactiveCrosshair[playerID])
		{
			float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
			if (!crosshairRefrPtr &&
				!isMovingCrosshair && 
				!p->mm->reqFaceTarget &&
				secsSinceActive > Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID])
			{
				// Offset left or right about the center of the screen based on player index.
				float targetPosX = 
				(
					(DebugAPI::screenResX / 2.0f) + 
					(100.0f * (fmod(playerID, 2) * 2 - 1) * ceil((playerID + 1) / 2.0f))
				);
				// Along screen's center line.
				float targetPosY = DebugAPI::screenResY / 2.0f;
				// Continue interpolating the position back 
				// towards the default position until reached.
				if (crosshairScaleformPos.x != targetPosX || crosshairScaleformPos.y != targetPosY)
				{
					// Re-centering completes after about 1.5 inactivity intervals elapse.
					float tRatio = std::clamp
					(
						(
							secsSinceActive / 
							max
							(
								0.1f, Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
							)
						) - 1.0f,
						0.0f, 
						1.0f
					);
					crosshairScaleformPos.x = Util::InterpolateSmootherStep
					(
						crosshairScaleformPos.x, targetPosX, tRatio
					);
					crosshairScaleformPos.y = Util::InterpolateSmootherStep
					(
						crosshairScaleformPos.y, targetPosY, tRatio
					);
					crosshairScaleformPos.z = 0.0f;
				}
			}
		}

		// Set last update time point.
		p->lastCrosshairUpdateTP = SteadyClock::now();
	}

	void TargetingManager::UpdateCrosshairMessage()
	{
		// Update the player's crosshair text entry to set the next time
		// the crosshair text message is updated.
		
		// NOTE:
		// Only TargetSelection and StealthState message types are set periodically.
		// Other messages types are triggered externally.

		// Can set without delaying.
		bool noDelay = false;
		// This message's type is one of the delayed types listed by the currently set message.
		bool isDelayedType = false;
		// The current message's delay restriction interval has passed.
		bool delayPassed = false;
		// External message was set.
		bool extMessageSet = false;
		// Set to the last message so that no new message is set if there is no update below.
		crosshairMessage->CopyMessageData(lastCrosshairMessage);
		// If there is an externally-requested crosshair message, prioritize it.
		if (extCrosshairMessage->type != CrosshairMessageType::kNone) 
		{
			noDelay = lastCrosshairMessage->delayedMessageTypes.empty();
			isDelayedType = 
			(
				!noDelay && 
				lastCrosshairMessage->delayedMessageTypes.contains(extCrosshairMessage->type)
			);
			delayPassed = 
			(
				Util::GetElapsedSeconds(lastCrosshairMessage->setTP) > 
				lastCrosshairMessage->secsMaxDisplayTime
			);
			if (noDelay || !isDelayedType || delayPassed) 
			{
				// Choose external message.
				crosshairMessage->CopyMessageData(extCrosshairMessage);
				extMessageSet = true;
			}
		}
		
		// Now check if a periodic message should be set.
		if (!extMessageSet)
		{
			// Display selection text if not sneaking 
			// or if highlighting a non-actor or corpse refr with the crosshair.
			// Display stealth state text otherwise.
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
			auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle);
			bool displayTargetSelectionMessage = 
			(
				(!coopActor->IsSneaking()) || 
				(
					(crosshairRefrPtr && validCrosshairRefrHit) && 
					(!selectedTargetActorPtr || selectedTargetActorPtr->IsDead())
				)	
			);
			if (displayTargetSelectionMessage)
			{
				// Selected target.
				SetPeriodicCrosshairMessage(CrosshairMessageType::kTargetSelection);
			}
			else
			{
				// Stealth.
				SetPeriodicCrosshairMessage(CrosshairMessageType::kStealthState);
			}
		}

		// Only set the last message if current and last are different.
		if (Hash(lastCrosshairMessage->text) != Hash(crosshairMessage->text)) 
		{
			crosshairMessage->setTP = SteadyClock::now();
			lastCrosshairMessage->CopyMessageData(crosshairMessage);
		}
		else
		{
			// Otherwise, just update the last message's start TP,
			// just in case the current crosshair message was set
			// by a more recent request, despite its text and type remaining unchanged.
			lastCrosshairMessage->setTP = crosshairMessage->setTP;
		}

		// Clear external message each frame here,
		// since we've just handled it.
		if (extCrosshairMessage->type != CrosshairMessageType::kNone) 
		{
			extCrosshairMessage->Clear();
		}
	}

	void TargetingManager::UpdateSneakState()
	{
		// Check periodically and update the player's detection state
		// before awarding any Sneak XP if the player becomes hidden
		// or stays undetected within the detection radius of a hostile actor.
		if (coopActor->IsSneaking())
		{
			secsSinceLastStealthStateCheck = Util::GetElapsedSeconds(p->lastStealthStateCheckTP);
			if (secsSinceLastStealthStateCheck > Settings::fSecsBetweenStealthStateChecks)
			{
				p->lastStealthStateCheckTP = SteadyClock::now();
				// Previous values to diff.
				const float prevDetectionPct = detectionPct;
				const float prevClosestHostileActorDist = closestHostileActorDist;
				const auto prevClosestHostileActorPtr = Util::GetActorPtrFromHandle
				(
					closestHostileActorHandle
				);

				// Update stealth detection state after storing the previous frame's values.
				UpdateStealthDetectionState();

				// Save the previous closest hostile actor and their distance from the player
				// to use in Sneak XP calculations after updating the stealth state.
				// P1's Sneak skill progression is already handled by the game.
				if (!p->isPlayer1)
				{
					// Add Sneak XP as needed.
					// XP formulae from:
					// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
					auto closestHostileActorPtr = Util::GetActorPtrFromHandle
					(
						closestHostileActorHandle
					);
					// Not fully detected (< 100% detection) 
					// within stealth radius of a hostile actor.
					if (detectionPct < 100.0f && 
						closestHostileActorPtr && 
						closestHostileActorPtr.get() &&
						closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						// Just got within stealth XP radius of hostile actor.
						if (!prevClosestHostileActorPtr || 
							prevClosestHostileActorDist > Settings::fHostileTargetStealthRadius)
						{
							// NOTE: 
							// May not need this, but keeping for now.
							// Set last hidden time point.
							p->lastHiddenInStealthRadiusTP = SteadyClock::now();
						}
						else
						{
							GlobalCoopData::AddSkillXP
							(
								controllerID,
								RE::ActorValue::kSneak, 
								0.625f * secsSinceLastStealthStateCheck
							);
						}
					}
					else if (!closestHostileActorPtr)
					{
						// Keep last hidden TP updated when not near any hostile actor.
						p->lastHiddenInStealthRadiusTP = SteadyClock::now();
					}

					// Becoming hidden (0% detected) after being detected 
					// within stealth radius of hostile actor.
					if (prevDetectionPct > 0.0f &&
						detectionPct == 0.0f && 
						closestHostileActorPtr && 
						closestHostileActorPtr.get() &&
						closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						GlobalCoopData::AddSkillXP(controllerID, RE::ActorValue::kSneak, 2.5f);
					}
				}
			}
		}
		else if (secsSinceLastStealthStateCheck != 0.0f)
		{
			// Reset stealth state check interval when not sneaking 
			// since no checks are being performed.
			secsSinceLastStealthStateCheck = 0.0f;
		}
	}

	void TargetingManager::UpdateStealthDetectionState()
	{
		// Update the overall detection percentage for the player
		// and set its corresponding RGB value for the crosshair text stealth message.
		// Also keep tabs on the closest hostile actor and the player's distance to them.

		// Reset the data we want to update.
		closestHostileActorHandle = RE::ActorHandle();
		closestHostileActorDist = FLT_MAX;
		detectionPct = 100.0f;

		// Invalid proc lists -> fully detected.
		auto processLists = RE::ProcessLists::GetSingleton(); 
		if (!processLists)
		{
			detectionPctRGB = GetDetectionLvlRGB(100.0f, false);
			return;
		}

		const auto isInCombat = coopActor->IsInCombat();
		// Accumulate detection percent.
		detectionPct = 0.0f;
		// Check each high process actor.
		for (const auto handle : processLists->highActorHandles)
		{
			// Must be valid, targetable, and not dead.
			// Dead men tell no tales, after all.
			auto actorPtr = Util::GetActorPtrFromHandle(handle);
			if (!actorPtr || 
				!actorPtr.get() || 
				!Util::IsValidRefrForTargeting(actorPtr.get()) || 
				actorPtr->IsDead())
			{
				continue;
			}

			const auto nameHash = Hash(actorPtr->GetName());
			// Ignore actors that do not modify the player's stealth state,
			// the player themselves, and the player's current mount.
			const bool ignored = 
			{
				actorPtr->boolFlags.any(RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter) ||
				actorPtr == coopActor ||
				actorPtr == p->GetCurrentMount()
			};
			if (ignored)
			{
				continue;
			}

			// Update detection percent if this actor's detection level of the player
			// is higher than the previous value.
			if (isInCombat)
			{
				// Stealth points:
				// Points (0-100) from this actor's hostile combat group are removed 
				// from the player's stealth points total.
				// Once the player's stealth points hit 0 with a single combat group, 
				// they are fully detected, so the amount deducted is directly related 
				// to the player's detection level.
				detectionPct = max
				(
					detectionPct, 
					max
					(
						0.0f,
						static_cast<uint8_t>
						(
							Util::GetStealthPointsLost(coopActor.get(), actorPtr.get())
						)
					)
				);
			}
			else
			{
				// Request detection level directly when not in combat.
				detectionPct = max
				(
					detectionPct, 
					static_cast<uint8_t>
					(
						Util::GetDetectionPercent(coopActor.get(), actorPtr.get())
					)
				);
			}

			// Update the closest hostile actor and their distance to the player.
			const float distToPlayer = actorPtr->data.location.GetDistance
			(
				coopActor->data.location
			);
			if (actorPtr->IsHostileToActor(coopActor.get()) && 
				distToPlayer < closestHostileActorDist)
			{
				closestHostileActorHandle = actorPtr->GetHandle();
				closestHostileActorDist = distToPlayer;
			}
		}

		// Get detection percent RGB value after updating the player's detection percent.
		detectionPctRGB = GetDetectionLvlRGB(detectionPct, false);
	}

	void TargetingManager::UpdateTargetedRefrMotionState()
	{
		// Update motion state data for the targeted refr.

		auto targetedActorHandle = GetRangedTargetActor();
		if (Util::HandleIsValid(targetedActorHandle))
		{
			// Prioritize targeting the selected/aim correction actor.
			targetMotionState->UpdateMotionState(targetedActorHandle);
		}
		else if (Util::HandleIsValid(crosshairRefrHandle))
		{
			// Target the crosshair refr, which is NOT an actor.
			targetMotionState->UpdateMotionState(crosshairRefrHandle);
		}
		else
		{
			// Clear current motion state data.
			targetMotionState->UpdateMotionState(RE::ObjectRefHandle());
		}
	}

	void TargetingManager::UpdateTargetingOverlay()
	{
		// Draw all targeting overlay elements if there are no fullscreen menus open,
		// no temporary menus open, or this player is not controlling menus.

		baseCanDrawOverlayElements = false;
		if (auto ui = RE::UI::GetSingleton(); ui) 
		{
			bool fullscreenMenuOpen = 
			(
				ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)
			);
			bool onlyAlwaysUnpaused = Util::MenusOnlyAlwaysUnpaused();
			bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(controllerID);
			baseCanDrawOverlayElements = 
			(
				(onlyAlwaysUnpaused || anotherPlayerControllingMenus) &&
				(!ui->GameIsPaused() && !fullscreenMenuOpen)
			);
		}

		// Update crosshair position and selected refr/actor handles first.
		if (baseCanDrawOverlayElements) 
		{
			UpdateCrosshairPosAndSelection();
		}

		// Update and draw all UI elements.
		DrawCrosshair();
		DrawAimPitchIndicator();
		DrawPlayerIndicator();
	}

	void TargetingManager::GrabbedReferenceInfo::RestoreSavedCollisionLayer()
	{
		// Restore the previously saved collision layer for non-actor refrs.

		if (!IsValid())
		{
			return;
		}
		
		auto objectPtr = refrHandle.get();

		// NOTE:
		// Will uncomment if changing actors' collision layers produces bugs.
		/*if (objectPtr->As<RE::Actor>())
		{
			return;
		}*/

		auto refr3DPtr = Util::GetRefr3D(objectPtr.get()); 
		if (!refr3DPtr || !refr3DPtr.get())
		{
			return;
		}

		refr3DPtr->SetCollisionLayer(savedCollisionLayer);
		Util::StopEffectShader(objectPtr.get(), glob.ghostFXShader);
		refr3DPtr->fadeAmount = 1.0f;
		hasCollision = savedCollisionLayer != RE::COL_LAYER::kNonCollidable;
	}

	void TargetingManager::GrabbedReferenceInfo::SaveCollisionLayer()
	{
		// Saved the refr's collision layer to restore later,
		// and then set the refr's collision layer to the given layer.

		if (!IsValid())
		{
			return;
		}
		
		auto objectPtr = refrHandle.get();

		// NOTE:
		// Will uncomment if changing actors' collision layers produces bugs.
		/*if (objectPtr->As<RE::Actor>())
		{
			return;
		}*/

		auto refr3DPtr = Util::GetRefr3D(objectPtr.get()); 
		if (!refr3DPtr || !refr3DPtr.get())
		{
			return;
		}
		
		savedCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
		hasCollision = savedCollisionLayer != RE::COL_LAYER::kNonCollidable;
	}

	void TargetingManager::GrabbedReferenceInfo::ToggleCollision()
	{
		// Toggle collision on/off for this grabbed refr.
		// Also play a shader and adjust fade amount.

		if (!IsValid())
		{
			return;
		}

		auto objectPtr = Util::GetRefrPtrFromHandle(refrHandle);

		// NOTE:
		// Will uncomment if changing actors' collision layers produces bugs.
		/*if (objectPtr->As<RE::Actor>())
		{
			return;
		}*/

		auto refr3DPtr = Util::GetRefr3D(objectPtr.get());
		if (!refr3DPtr || !refr3DPtr.get())
		{
			return;
		}

		auto currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
		// Restore saved and do not toggle if still unidentified.
		if (currentCollisionLayer == RE::COL_LAYER::kUnidentified)
		{
			refr3DPtr->SetCollisionLayer(savedCollisionLayer);
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
			return;
		}

		// Turn off collision and play shader to indicate the change.
		if (currentCollisionLayer != RE::COL_LAYER::kNonCollidable)
		{	
			refr3DPtr->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
			if (!hasCollision)
			{
				Util::StartEffectShader(objectPtr.get(), glob.ghostFXShader);
			}
		}
		else
		{
			// Turn collision back on and stop the shader.
			refr3DPtr->SetCollisionLayer(savedCollisionLayer);
			Util::StopEffectShader(objectPtr.get(), glob.ghostFXShader);
			refr3DPtr->fadeAmount = 1.0f;
			if (auto fadeNode = refr3DPtr->AsFadeNode(); fadeNode)
			{
				fadeNode->currentFade = 1.0f;
			}
			
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
		}
	}

	void TargetingManager::GrabbedReferenceInfo::UpdateGrabbedReference
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const uint8_t& a_index, 
		const float& a_firstGrabbedReferenceBufferDist
	)
	{
		// Update the position of grabbed refrs by setting their velocity.
		// Arrange multiple grabbed refrs in a (very poorly formed) ring 
		// about the first grabbed refr.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		// Fade out the object slightly if its collision is toggled off.
		if (!hasCollision)
		{
			auto refr3DPtr = Util::GetRefr3D(objectPtr.get());
			if (refr3DPtr && refr3DPtr.get())
			{
				refr3DPtr->fadeAmount = 0.5f;
				if (auto fadeNode = refr3DPtr->AsFadeNode(); fadeNode)
				{
					fadeNode->currentFade = 0.5f;
				}
			}
		}

		// Update grabbed refr orientation.
		bool isRagdolled = a_p->coopActor->IsInRagdollState();
		float facingAng = a_p->coopActor->GetHeading(false);
		if (!a_p->mm->reqFaceTarget)
		{
			// The last recorded LS game angle.
			facingAng = a_p->mm->movementOffsetParams[!MoveParams::kLSGameAng];
		}
		else if (isRagdolled)
		{
			// For M.A.R.F-ing, attempt to place the other grabbed players
			// between the player and the crosshair world position when facing it.
			facingAng = Util::GetYawBetweenPositions
			(
				a_p->coopActor->data.location, a_p->tm->crosshairWorldPos
			);
		}
		
		// Suspend the grabbed objects in front of the player
		// at a distance dependent on their max reach and the object's height.
		float objectHeight = objectPtr->GetHeight();
		float baseSuspensionDist = a_p->tm->GetMaxActivationDist() / 3.0f;
		float xySuspensionDist =
		(
			max(objectHeight, baseSuspensionDist)
		);

		// Can move the grabbed object(s) closer or farther from the player
		// by displacing the RS right (farther) and left (closer).
		const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
		if (a_p->pam->IsPerforming(InputAction::kAdjustAimPitch) && 
			fabsf(rsData.xComp) > fabsf(rsData.yComp))
		{
			a_p->tm->grabbedRefrDistanceOffset = std::clamp
			(
				a_p->tm->grabbedRefrDistanceOffset + 
				(
					rsData.xComp * 
					rsData.normMag *
					a_p->mm->playerScaledHeight *
					*g_deltaTimeRealTime
				),
				-baseSuspensionDist,
				2.0f * baseSuspensionDist
			);
		}

		// Arranged in a circle about the index 0 grabbed object.
		// Sweeps out clockwise from directly above the index 0 object.
		// Positional offset from the first object.
		auto indexBasedOffset = RE::NiPoint3();
		// Spacing between subsequent grabbed objects is dependent on the objects' radii/heights.
		float indexOffsetScalar = a_firstGrabbedReferenceBufferDist;
		if (auto object3DPtr = Util::GetRefr3D(objectPtr.get()); object3DPtr && object3DPtr.get())
		{
			// Add both the central and current grabbed object's radii.
			indexOffsetScalar = 
			(
				a_firstGrabbedReferenceBufferDist + 
				min(objectHeight * 0.25f, object3DPtr->worldBound.radius * 0.25f)
			);
		}

		auto forward = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(facingAng));
		// Set direction from the central object based on the object's index.
		if (a_index > 0)
		{
			indexBasedOffset = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			Util::RotateVectorAboutAxis
			(
				indexBasedOffset,
				forward, 
				2.0f * PI * ((float)(a_index - 1) / (float)(Settings::uMaxGrabbedReferences - 1))
			);
		}
		
		// Full credits to ersh1 once again for the steps 
		// to access a refr's motion type and apply linear velocity:
		// https://github.com/ersh1/Precision/blob/702428bc065c75b3964a0324992658b1ab0a0821/src/Havok/ContactListener.cpp#L8

		// Additional distance and speed mults if paragliding or M.A.R.F-ing.
		float suspensionDistMult = 1.0f;
		float maxSpeedMult = 1.0f;
		// Absolute max speed the grabbed object can reach.
		float grabbedRefrMaxSpeed = Settings::fBaseGrabbedRefrMaxSpeed;
		// Speedmult and suspension distance adjustments specifically for grabbing other players.
		if (Settings::bCanGrabOtherPlayers && GlobalCoopData::IsCoopPlayer(objectPtr.get()))
		{
			if (a_p->mm->isParagliding)
			{
				// Carry other players while paragliding.
				// Make sure they keep up.
				maxSpeedMult = 2.0f;
				suspensionDistMult = 2.0f;
			}
			else if (a_p->tm->isMARFing)
			{
				// Uhh, we have Skyrim's Paraglider at home, guys. Really!
				// M.A.R.F is on.
				if (ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled && 
					glob.tarhielsGaleEffect && 
					a_p->coopActor->HasMagicEffect(glob.tarhielsGaleEffect))
				{
					// Additional speed boost when using the gale spell.
					maxSpeedMult = 2.0f;
					suspensionDistMult = 2.0f;
				}
			}
			else if (a_p->coopActor->IsInRagdollState())
			{
				// This player was thrown by the other player.
				// Have the throwing player follow the thrown player at warp speed.
				// Unreachable max speed cap.
				maxSpeedMult = FLT_MAX / grabbedRefrMaxSpeed;
			}
			else
			{
				// Otherwise, mult is equal to the grabbed player max speed mult.
				maxSpeedMult = Settings::fGrabbedPlayerMaxSpeedMult;
			}
		}

		// Target position the grabbed reference should move to.
		// The grabbed refr will get arbitrarily close to the position,
		// as its velocity will scale down as it approaches the position.
		RE::NiPoint3 targetPosition = a_p->coopActor->data.location;
		// Still spins like a Beyblade when M.A.R.F-ing
		// and when players are attempting to move in opposite directions,
		// but a bit better when based from the player's torso
		// instead of from their head.
		RE::NiPoint3 basePos =
		(
			a_p->tm->isMARFing ?
			Util::GetTorsoPosition(a_p->coopActor.get()) :
			RE::NiPoint3
			(
				a_p->coopActor->data.location.x,
				a_p->coopActor->data.location.y,
				a_p->coopActor->data.location.z + a_p->mm->playerScaledHeight
			)
		);
		targetPosition = 
		{
			(
				basePos.x + 
				(
					(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
					suspensionDistMult * 
					cosf(Util::ConvertAngle(facingAng)) *
					cosf(a_p->mm->aimPitch)
				)
			),
			(
				basePos.y + 
				(
					(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
					suspensionDistMult * 
					sinf(Util::ConvertAngle(facingAng)) * 
					cosf(a_p->mm->aimPitch)
				)
			),
			(
				basePos.z
			)
		};

		// Can move the grabbed refr vertically in an arc around the player by adjusting aim pitch.
		if (a_p->tm->isMARFing)
		{
			targetPosition.z += 
			(
				(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
				suspensionDistMult * 
				-sinf(a_p->mm->aimPitch)
			);
		}
		else
		{
			targetPosition.z += 
			(
				(a_p->mm->playerScaledHeight + a_p->tm->grabbedRefrDistanceOffset) * 
				suspensionDistMult * 
				-sinf(a_p->mm->aimPitch)
			);
		}

		// Apply positional offset scalar to the normalized offset after rotation.
		indexBasedOffset *= indexOffsetScalar;
		// Now finalize the target position by adding the positional offset based on grab index.
		targetPosition += indexBasedOffset;

		// Positional delta to reach the target position.
		auto posDelta = targetPosition - objectPtr->data.location;
		const float distToTargetPos = posDelta.Length();
		// Normalized direction.
		auto dir = distToTargetPos == 0.0f ? RE::NiPoint3() : posDelta / distToTargetPos;

		// Apply max speed mult.
		grabbedRefrMaxSpeed *= maxSpeedMult;
		// Should cap out at the player's movement speed if higher than the pre-defined max speed.
		float playerMovementSpeed = 
		(
			isRagdolled && a_p->coopActor->GetCharController() ? 
			a_p->coopActor->GetCharController()->outVelocity.Length3() :
			a_p->coopActor->DoGetMovementSpeed()
		);
		grabbedRefrMaxSpeed = max(grabbedRefrMaxSpeed, playerMovementSpeed);
		// Slow down when nearing the target position. Reduces jitter.
		const float slowdownRadius = a_p->tm->GetMaxActivationDist(); //3000.0f
		float slowdownFactor = Util::InterpolateEaseOut
		(
			0.0f, 
			1.0f, 
			std::clamp
			(
				distToTargetPos / slowdownRadius, 
				0.0f, 
				1.0f
			), 
			5.0f
		);

		// Don't move at all when too close.
		auto playerToTargetDir = targetPosition - a_p->coopActor->data.location;
		auto objectToPlayerDir = a_p->coopActor->data.location - objectPtr->data.location;
		playerToTargetDir.Unitize();
		objectToPlayerDir.Unitize();
		float catchupFactor = 5.5f + playerToTargetDir.Dot(objectToPlayerDir) * 4.5f;
		// Velocity to apply to the refr.
		auto havokVelocity = RE::NiPoint3();
		if ((!isRagdolled) && (a_p->mm->lsMoved || playerMovementSpeed > 0.0f))
		{
			havokVelocity = 
			(
				dir * 
				fmin
				(
					playerMovementSpeed * catchupFactor, 
					grabbedRefrMaxSpeed
				) 
			);
		}
		else
		{
			havokVelocity = posDelta * catchupFactor;
		}
		
		// Cap to prevent overshooting.
		if (float speed = havokVelocity.Length(); speed != 0.0f)
		{
			havokVelocity = 
			(
				(havokVelocity / speed) * min(speed, distToTargetPos / *g_deltaTimeRealTime)
			);
		}

		// Convert to havok units before setting below.
		havokVelocity *= GAME_TO_HAVOK;
		
		// Adjust havok velocity here or, if there is no valid rigid body,
		// as in the case of some active projectiles,
		// still save the velocity to apply regardless.
		// Our projectile UpdateImpl() hook will still update active projectiles' velocity 
		// even if they do not have a rigid body.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		if (isActiveProjectile)
		{
			RE::NiPoint3 oldVelocity = asProjectile->linearVelocity;
			havokVelocity.x = Util::InterpolateSmootherStep
			(
				oldVelocity.x, 
				havokVelocity.x, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.y = Util::InterpolateSmootherStep
			(
				oldVelocity.y, 
				havokVelocity.y, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.z = Util::InterpolateSmootherStep
			(
				oldVelocity.z,
				havokVelocity.z, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			// Save our new velocity to apply later in the UpdateImpl() hook.
			lastSetVelocity = havokVelocity * HAVOK_TO_GAME;
		}
		else
		{
			// Need a valid rigid body if not an active projectile.
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get()); 
			if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get())
			{
				return;
			}

			RE::NiPoint3 oldVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity);
			havokVelocity.x = Util::InterpolateSmootherStep
			(
				oldVelocity.x, 
				havokVelocity.x, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.y = Util::InterpolateSmootherStep
			(
				oldVelocity.y, 
				havokVelocity.y, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.z = Util::InterpolateSmootherStep
			(
				oldVelocity.z,
				havokVelocity.z, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);

			// Activate the refr and set the computed velocity.
			Util::NativeFunctions::hkpEntity_Activate(hkpRigidBodyPtr.get());
			hkpRigidBodyPtr->motion.motionState.angularDamping = 0.0f;
			hkpRigidBodyPtr->motion.motionState.linearDamping =	0.0f;
			hkpRigidBodyPtr->motion.SetLinearVelocity(havokVelocity);
			// Save our new velocity.
			lastSetVelocity = havokVelocity * HAVOK_TO_GAME;

			// Adjust the grabbed object's rotation if performing the requisite action.
			// [Rotation controls]:
			// 1. Move the right stick up and down to rotate along the horizontal axis 
			// facing right relative to the player, 
			// 2. Move the right stick left and right
			// to rotate along the vertical axis facing upward.

			bool shouldAdjustRotation = a_p->pam->IsPerforming(InputAction::kGrabRotateYZ);
			if (shouldAdjustRotation)
			{
				const auto& headingAngle = a_p->coopActor->GetHeading(false);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[0] = 
				(
					-Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * cosf(headingAngle)
				);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[1] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * sinf(headingAngle)
				);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[2] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.xComp
				);
			}
			else
			{
				// Zero out angular velocity when not rotating.
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[0] = 0.0f;
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[1] = 0.0f;
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[2] = 0.0f;
			}
		}
	}

	void TargetingManager::ReleasedReferenceInfo::ApplyVelocity(RE::NiPoint3& a_velocityToSet)
	{
		// Save the given velocity, cap the given speed of this released refr 
		// to the release speed, and then apply the capped velocity.

		if (!IsValid())
		{
			Clear();
			return;
		}
		
		float currentSpeed = a_velocityToSet.Length();
		// No need to set a velocity of 0.
		if (currentSpeed == 0.0f)
		{
			return;
		}

		auto objectPtr = refrHandle.get();
		// Clamp the velocity.
		if (isHoming && currentSpeed > releaseSpeed)
		{
			a_velocityToSet = (a_velocityToSet / currentSpeed) * (releaseSpeed);
		}

		// Apply the velocity directly here to any refrs with a rigid body.
		// Active projectiles have their velocity saved here and set in the UpdateImpl() hook.
		lastSetVelocity = a_velocityToSet;

		if (!isActiveProjectile)
		{
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get()); 
			if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get() || !isThrown)
			{
				return;
			}

			hkpRigidBodyPtr->motion.motionState.angularDamping = 
			hkpRigidBodyPtr->motion.motionState.linearDamping =  0.0f;
			hkpRigidBodyPtr->motion.SetLinearVelocity(a_velocityToSet * GAME_TO_HAVOK);
		}
	}
	
	RE::NiPoint3 TargetingManager::ReleasedReferenceInfo::CalculatePredInterceptPos
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Calculate the position at which the released refr is likely to collide
		// with the target once the refr is released. Use the target's physical motion
		// data to perform this calculation.

		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		// No valid target refr, so aim at the crosshair position.
		if (!targetRefrPtr || !targetRefrPtr.get())
		{
			return a_p->tm->crosshairWorldPos;
		}

		// Get targeted actor, if targeting one.
		auto targetActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		bool targetActorValidity = targetActorPtr && targetActorPtr.get();
		// Aim at crosshair position/closest node position if an actor is selected 
		// by the crosshair or if facing the crosshair target position.
		// Otherwise, aim at the aim correction target's torso.
		const auto currentAimedAtPos = trajectoryEndPos;
		// Invert pitch convention for use with trig functions.
		const double& aimPitch = -a_p->mm->aimPitch;
		// Max number of iterations, current number of iterations.
		uint8_t steps = 50;
		uint8_t step = 0;
		// Set the initial predicted intercept/hit position to the current aimed-at position.
		RE::NiPoint3 predHitPos = currentAimedAtPos;
		// Next predicted velocity for the target. Set to current velocity initially.
		RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
		// Axis to rotate velocity vector around.
		RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
		// Flight time deltas at which to bail out of the calculation loop.
		double timeBailDeltaMin = 1E-4;
		double timeBailDeltaMax = 1000.0;
		// XY and Z offsets to the predicted position from the release position.
		double xy = Util::GetXYDistance(predHitPos, releasePos);
		double z = (predHitPos - releasePos).z;
		// Time to target.
		double t = xy / releaseSpeed * cosf(aimPitch);
		// Previously calculated time to target.
		double tPrev = 0.0;
		// Difference in the calculated times to target.
		double tDiff = fabsf(t - tPrev);
		// Current delta yaw and yaw rotation speed.
		const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
		float currentZRotSpeed = 0.0f;
		if (targetActorValidity)
		{
			currentZRotSpeed =
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ?
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z :
				0.0f
			);
		}

		// Average of current and average per interval yaw deltas.
		float avgYawDeltaPerFrame = 
		(
			(
				currentYawAngDelta / (*g_deltaTimeRealTime) + 
				a_p->tm->targetMotionState->apiYawAngDelta / (*g_deltaTimeRealTime)
			) / 2.0
		);
		// Average of current and average per interval change in speed.
		const float avgSpeedDelta = 
		(
			(
				a_p->tm->targetMotionState->apiSpeedDelta +
				a_p->tm->targetMotionState->cSpeedDeltaPerFrame
			) / 2.0f
		);
		// Attempt to accurately estimate the target intercept position
		// and continue looping until the reported time-to-target values converge
		// to below the minimum time diff (success),
		// or diverge above the maximum time diff (failure),
		// or until the maximum number of iterations is reached.
		while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
		{
			// SUPER NOTE: 
			// Everything below is obviously not mathematically correct,
			// since the target's velocity and acceleration are changing constantly,
			// which means that finding the best predicted hit position
			// would require integration over the time of flight.
			// However the recorded acceleration and velocity motion data
			// for targets is very noisy, which leads to huge overshoots
			// when using the proper formulas for calculating the predicted position at time t.
			// This temporary, manually-tested solution performs slightly better 
			// than not accounting for acceleration at all.

			// Rotate predicted velocity vector by the yaw diff 
			// which will occur over the time delta.
			double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
			double speed = nPredTargetVel.Length();
			// Rotate and re-apply original speed.
			Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
			nPredTargetVel.Unitize();
			nPredTargetVel *= speed;
			// Offset the current aimed at position by the delta position calculated
			// using the position delta over the elapsed time frame from the previous iteration.
			auto posDelta = nPredTargetVel * (t - tPrev);
			predHitPos += posDelta;

			// Update positional offsets based on the new predicted hit position.
			xy = Util::GetXYDistance(predHitPos - releasePos);
			z = (predHitPos - releasePos).z;
			// Set previous time to target to current.
			tPrev = t;
			// Update current time to target using the new XY positional offset,
			// since release speed and aim pitch are kept constant.
			t = xy / releaseSpeed * cosf(aimPitch);
			// Calculate the change in time to target.
			tDiff = fabsf(t - tPrev);
			// On to the next step.
			++step;
		}

		if (isnan(tDiff) || tDiff >= timeBailDeltaMax)
		{
			// Failed to find intercept position, so set to the initially-aimed-at position.
			return currentAimedAtPos;
		}
		else
		{
			// Either converged on a particular intercept position, 
			// with the change in time to target under the lower bail precision (success),
			// or didn't quite meet that required precision (failed).
			return predHitPos;
		}
	}

	RE::NiPoint3 TargetingManager::ReleasedReferenceInfo::GuideRefrAlongTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Direct this released refr at either the initial target position 
		// along a fixed trajectory or continuously towards the target position/target refr.

		RE::NiPoint3 velToSet = RE::NiPoint3();
		if (!IsValid()) 
		{
			Clear();
			return velToSet;
		}

		auto objectPtr = refrHandle.get();
		if (!releaseTP.has_value())
		{
			Clear();
			return velToSet;
		}
		
		// Stop setting velocity to force the released refr along the predicted fixed trajectory
		// if the released refr has hit something. Collisions are still active though.
		bool stopSettingPredictedTraj = 
		(
			trajType == ProjectileTrajType::kPrediction && firstHitTP.has_value()
		);
		if (stopSettingPredictedTraj)
		{
			return velToSet;
		}

		// Get gravitational constant.
		double g = Util::GetGravitationalConstant();
		bool shouldUseHomingTrajectory = trajType == ProjectileTrajType::kHoming;
		// Set the target position. Default to crosshair world position first.
		RE::NiPoint3 aimTargetPos = a_p->tm->crosshairWorldPos;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrValidity = targetRefrPtr && targetRefrPtr.get();
		auto asActorPtr = 
		(
			targetRefrValidity ? RE::ActorPtr(targetRefrPtr->As<RE::Actor>()) : nullptr
		);
		bool targetActorValidity = 
		(
			asActorPtr && 
			asActorPtr.get() &&
			Util::IsValidRefrForTargeting(asActorPtr.get())
		);
		if (shouldUseHomingTrajectory)
		{
			targetLocalPosOffset = 
			(
				targetRefrValidity ? 
				a_p->tm->crosshairLocalPosOffset : 
				RE::NiPoint3()
			);
			if (targetActorValidity) 
			{
				// TODO:
				// Snap released refr to the closest position 
				// on the targeted actor's character controller collider.
				// Get targeted node, if available.
				// Otherwise, aim at the position offset from the actor's refr location.
				/*if (Settings::vbUseAimCorrection[a_p->playerID] && 
					  targetedActorNode && targetedActorNode.get()) 
				{
					aimTargetPos = targetedActorNode->world.translate;
					auto hkpRigidBodyPtr = Util::GethkpRigidBody(targetedActorNode.get()); 
					if (hkpRigidBodyPtr && hkpRigidBodyPtr.get())
					{
						aimTargetPos = 
						(
							ToNiPoint3
							(
								hkpRigidBodyPtr->motion.motionState.sweptTransform.centerOfMass0
							) * HAVOK_TO_GAME
						);
					}
				}
				else
				{
					aimTargetPos = 
					(
						Util::GetTorsoPosition(targetActorPtr.get()) + targetLocalPosOffset
					);
				}*/

				aimTargetPos = Util::GetTorsoPosition(asActorPtr.get()) + targetLocalPosOffset;
			}
			else if (targetRefrValidity)
			{
				aimTargetPos = Util::Get3DCenterPos(targetRefrPtr.get()) + targetLocalPosOffset;
			}
		}
		else
		{
			aimTargetPos = trajectoryEndPos;
		}
		
		// Pitch and yaw to the target position.
		float pitchToTarget = Util::GetPitchBetweenPositions
		(
			objectPtr->data.location, aimTargetPos
		);
		float yawToTarget = Util::GetYawBetweenPositions(objectPtr->data.location, aimTargetPos);
		
		// Get the released refr's current velocity.
		RE::NiPoint3 currentVelocity{ };
		// Use the havok velocity here or, if there is no valid rigid body,
		// still set the velocity to apply regardless.
		// Our projectile UpdateImpl() hook can still update active projectiles' velocity 
		// even if they do not have a rigid body.
		if (isActiveProjectile)
		{
			auto asProjectile = objectPtr->As<RE::Projectile>();
			if (asProjectile)
			{
				currentVelocity = asProjectile->linearVelocity;
			}
			else
			{
				objectPtr->GetLinearVelocity(currentVelocity);
			}
		}
		else
		{
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get());
			if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get())
			{
				return velToSet;
			}

			currentVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME);
		}

		const float t = Util::GetElapsedSeconds(releaseTP.value());
		// Set speed to the corresponding speed along the fixed trajectory.
		// Will cap speed later if necessary.
		const float velXY = releaseSpeed * cosf(launchPitch);
		const float velX = velXY * cosf(launchYaw);
		const float velY = velXY * sinf(launchYaw);
		const float velZ = releaseSpeed * sinf(launchPitch) - g * t;
		float speedToSet = RE::NiPoint3(velX, velY, velZ).Length();
		
		const bool tooLongToReach = 
		(
			initialTimeToTarget == 0.0f ||
			initialTimeToTarget >= Settings::fMaxSecsBeforeClearingReleasedRefr
		);
		// Cannot split the trajectory into two parts 
		// if the projectile reaches the target in under two frames,
		// so we'll start homing in right away, if this is a homing projectile.
		const bool lessThanTwoFramesToReachTarget = 
		(
			initialTimeToTarget <= *g_deltaTimeRealTime * 2.0f
		);
		// Direct the released refr along the fixed trajectory determined at launch,
		// but keep the fixed trajectory pitch until the refr starts homing in.
		// Also check if the released refr should start homing in.
		// Save previous homing state.
		bool wasHoming = isHoming;
		if (!startedHomingIn || !shouldUseHomingTrajectory)
		{
			// Maintain launch yaw and current pitch along the fixed trajectory portion
			// of the flight.
			// Current XY and Z positions relative to the release position.
			const float xy = Util::GetXYDistance(releasePos, objectPtr->data.location);
			const float z = objectPtr->data.location.z - releasePos.z;

			float nextT = (t + *g_deltaTimeRealTime);
			RE::NiPoint3 targetPos = RE::NiPoint3
			(
				releasePos.x + releaseSpeed * cosf(launchPitch) * cosf(launchYaw) * nextT,
				releasePos.y + releaseSpeed * cosf(launchPitch) * sinf(launchYaw) * nextT,
				releasePos.z + 
				releaseSpeed * sinf(launchPitch) * nextT - 
				0.5f * g * nextT * nextT
			);

			velToSet = (targetPos - objectPtr->data.location) / *g_deltaTimeRealTime;
			lastSetTargetPosition = targetPos;
			
			// Nothing to do now if not guiding a homing projectile.
			if (!shouldUseHomingTrajectory)
			{
				return velToSet;
			}

			//======================
			// [Start-homing Check]:
			//======================
			// Check if homing projectile should fully start homing in on the target 
			// instead of following its initial fixed trajectory.
			// Either:
			// 1. Too long to reach the target.
			// 2. Past half the distance/time of flight.
			// 3. Will reach the target in under 2 frames.
			const float epsilon = 1E-3f;
			bool passedHalfwayPoint = 
			(
				fabsf
				(
					t - 0.5f * initialTimeToTarget
				) <= epsilon ||
				xy > (0.5f * Util::GetXYDistance(releasePos, aimTargetPos))
			);
			// Set homing flags if not already homing.
			if (passedHalfwayPoint || tooLongToReach || lessThanTwoFramesToReachTarget)
			{
				// Used to check if the projectile should switch to homing mode.
				startedHomingIn = isHoming = true;
			}
			else
			{
				// Nothing more to do until the projectile starts homing in.
				return velToSet;
			}
		}

		bool justStartedHomingIn = !wasHoming && isHoming;
		// Direction from the current position to the target.
		auto dirToTarget = aimTargetPos - objectPtr->data.location;
		dirToTarget.Unitize();
		// Last frame's velocity direction.
		auto velDirLastFrame = lastSetVelocity;
		velDirLastFrame.Unitize();
		// Angle between last frame's velocity and the target.
		float angBetweenVelAndToTarget = acosf
		(
			std::clamp(dirToTarget.Dot(velDirLastFrame), -1.0f, 1.0f)
		);

		// Went past the target if velocity direction and direction to target 
		// diverge by >= 90 degrees and the distance to the target 
		// is less than the max distance travelable per frame.
		bool wentPastTarget = 
		(
			angBetweenVelAndToTarget >= PI / 2.0f && 
			objectPtr->data.location.GetDistance(aimTargetPos) <= 
			currentVelocity.Length() * *g_deltaTimeRealTime
		);
		isHoming = 
		(
			(justStartedHomingIn) || 
			(
				(!wentPastTarget) && 
				(
					(hitRefrFIDs.empty()) ||
					(targetRefrValidity && !hitRefrFIDs.contains(targetRefrPtr->formID))
				)
			)
		);
		if (isHoming)
		{
			//=================================
			// [Set Pitch/Yaw to Track Target]:
			//=================================

			float currentVelPitch = Util::GetPitchBetweenPositions
			(
				RE::NiPoint3(), currentVelocity
			);
			// Sometimes invalid at launch time for some reason (-nan).
			float currentVelYaw = Util::GetYawBetweenPositions(RE::NiPoint3(), currentVelocity);
			const float t = Util::GetElapsedSeconds(releaseTP.value());
			float pitchToSet = currentVelPitch;
			float yawToSet = currentVelYaw;
			// Just launched or invalid, so set pitch and yaw to saved launch values.
			if (t == 0.0f || 
				justStartedHomingIn || 
				isnan(currentVelPitch) || 
				isinf(currentVelPitch))
			{
				currentVelPitch = -launchPitch;
			}

			if (t == 0.0f || justStartedHomingIn || isnan(currentVelYaw) || isinf(currentVelYaw))
			{
				currentVelYaw = Util::ConvertAngle(launchYaw);
			}

			// Lerp pitch/yaw to slowly rotate the projectile to face the target in the XY plane.
			// Fully face the target in the XY plane once the projectile's initial time 
			// to the target position has elapsed.
			// Velocity on fixed trajectory determined by projectile launch data.
			if (tooLongToReach || lessThanTwoFramesToReachTarget)
			{
				// Can't hit target with given launch pitch, 
				// so set yaw directly to target right away.
				pitchToSet = pitchToTarget;
				yawToSet = yawToTarget;
			}
			else
			{
				// Slowly turn to face.
				float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - currentVelPitch);
				pitchToSet = Util::NormalizeAngToPi
				(
					currentVelPitch + 
					Util::InterpolateSmootherStep
					(
						0.0f, pitchDiff, min(1.0f, t / (initialTimeToTarget))
					)
				);
				float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentVelYaw);
				yawToSet = Util::NormalizeAng0To2Pi
				(
					currentVelYaw + 
					Util::InterpolateSmootherStep
					(
						0.0f, yawDiff, min(1.0f, t / (initialTimeToTarget))
					)
				);
			}

			// Set velocity.
			velToSet = 
			(
				Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) *
				speedToSet
			);

			auto targetPos = lastSetTargetPosition + velToSet * *g_deltaTimeRealTime;
			lastSetTargetPosition = targetPos;
		}

		return velToSet;
	}

	bool TargetingManager::ReleasedReferenceInfo::InitPreviewTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Set initial release trajectory info for this refr prior to its release.
		// Determines the released refr's initial orientation, speed, time to target,
		// targeted trajectory end position, 
		// and whether or not the released refr was thrown or dropped.
		// The refr's speed is not modified, nor is magicka deducted from the player.
		// Return true if the player can successfully throw this refr.

		if (!IsValid())
		{
			return false;
		}

		auto objectPtr = refrHandle.get();
		if (!objectPtr || !objectPtr.get())
		{
			return false;
		}
		
		// If not trying to throw this refr, return early.
		if ((!a_p->pam->IsPerforming(InputAction::kGrabObject) ||
			!a_p->mm->reqFaceTarget) ||
			objectPtr == a_p->coopActor ||
			refrHandle == a_p->tm->crosshairRefrHandle)
		{
			return false;
		}

		canReachTarget = true;
		collisionActive = true;
		startedHomingIn = false;
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrPtrValidity = 
		(
			targetRefrPtr && 
			targetRefrPtr.get() &&
			Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActor = targetRefrPtrValidity ? targetRefrPtr->As<RE::Actor>() : nullptr;
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		if (targetActor)
		{
			trajectoryEndPos = Util::GetTorsoPosition(targetActor) + targetLocalPosOffset;
		}
		else if (targetRefrPtrValidity)
		{
			trajectoryEndPos = Util::Get3DCenterPos(targetRefrPtr.get()) + targetLocalPosOffset;
		}

		// Released from suspended position.
		releasePos = objectPtr->data.location;
		// Angle straight at the initial intercept position.
		launchPitch = -Util::NormalizeAngToPi
		(
			Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos)
		);
		launchYaw = Util::NormalizeAng0To2Pi
		(
			Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos))
		);
		trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[a_p->playerID]
		);

		// TODO: 
		// Snap released refr to the closest position 
		// on the targeted actor's character controller collider.
		// Set closest node and trajectory end position to that node's position
		// if aiming at an actor with aim correction.
		/*
		if (targetActorValidity && 
			Settings::vbUseAimCorrection[a_p->playerID] && 
			targetActorHandle == a_p->tm->crosshairRefrHandle)
		{
			if (auto actor3D = Util::GetRefr3D(targetActorPtr.get()); actor3D)
			{
				float closestNodeDist = FLT_MAX;
				RE::NiPoint3 nodeCenterOfMassPos = trajectoryEndPos;
				Util::TraverseChildNodesDFS
				(
					actor3D.get(),
					[this, &closestNodeDist, &nodeCenterOfMassPos](RE::NiAVObject* a_node) 
					{
						if (!a_node)
						{
							return;
						}

						auto node3DPtr = RE::NiPointer<RE::NiAVObject>(a_node); 
						if (!node3DPtr || !node3DPtr.get())
						{
							return;
						}

						auto hkpRigidBodyPtr = Util::GethkpRigidBody(node3DPtr.get());
						// Must have havok rigid body to collide with.
						if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get())
						{
							return;
						}

						// Check distance from the current trajectory end position 
						// (crosshair position) and the node's center of mass.
						const auto centerOfMass = 
						(
							ToNiPoint3
							(
								hkpRigidBodyPtr->motion.motionState.sweptTransform.centerOfMass0
							) * HAVOK_TO_GAME
						);
						float dist = centerOfMass.GetDistance(trajectoryEndPos); 
						if (dist < closestNodeDist)
						{
							// New closest node to the crosshair position.
							if (auto collidable = hkpRigidBodyPtr->GetCollidable(); collidable)
							{
								closestNodeDist = dist;
								targetedActorNode = RE::NiPointer<RE::NiAVObject>(a_node);
								nodeCenterOfMassPos = centerOfMass;
							}
						}
					}
				);

				// Set new trajectory end position to the node's center of mass.
				trajectoryEndPos = nodeCenterOfMassPos;
			}
		}
		*/

		// Release velocity to set.
		releaseVelocity = RE::NiPoint3();
		auto asActor = objectPtr->As<RE::Actor>();
		// Projectile motion equation used to get max
		// and min launch angles at max launch speed,
		// and launch angle with minimized launch speed:
		// https://en.wikipedia.org/wiki/Projectile_motion#Angle_%CE%B8_required_to_hit_coordinate_(x,_y)

		// Get grab bind hold time, which directly influences 
		// the speed at which the refr is thrown.
		auto actionIndex = !InputAction::kGrabObject - !InputAction::kFirstAction;
		float cappedHoldTime = 
		(
			min
			(
				a_p->pam->paStatesList[actionIndex].secsPerformed,
				max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
			)
		);
		// Normalize it.
		float normHoldTime = std::lerp
		(
			0.0f, 
			1.0f,
			cappedHoldTime / max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
		);
	
		// Get HMS AVs inc per level up.
		uint32_t iAVDhmsLevelUp = 10;
		auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
		if (valueOpt.has_value())
		{
			iAVDhmsLevelUp = valueOpt.value();
		}

		// Total increase to the player's magicka so far.
		// Default to serialized data.
		float magickaTotalInc = 
		(
			glob.serializablePlayerData.at
			(
				a_p->coopActor->formID
			)->hmsPointIncreasesList[1]
		);
		// Tack on any modifiers.
		magickaTotalInc += 
		(
			a_p->coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
			) +
			a_p->coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
			)
		);

		// Required total number of level ups put into magicka 
		// to get the current magicka level.
		float magickaLevelInc = magickaTotalInc / iAVDhmsLevelUp;
		// Increase max throw speed by 10% of the base throw speed per level up.
		float releaseSpeedInc = 
		(
			Settings::fBaseMaxThrownObjectReleaseSpeed * magickaLevelInc * 0.1f
		);
		// g is taken to be positive for all calculations below.
		double g = Util::GetGravitationalConstant();
				
		// Expend magicka when throwing.
		// The heavier the thrown object and the faster the release speed,
		// the higher the magicka cost.

		// Throw speed slowdown factor based on how much extra magicka
		// the player would need to throw this object.
		// If the cost is larger than the player's remaining magicka,
		// they will throw this refr at a reduced speed.

		// Slow down the base projectile release speed if the player is low on magicka.
		// Will lower the damage dealt by this thrown object on hit,
		// since damage scales up with hit velocity.
		// Additional release speed multiplier if moving.
		float releaseSpeedMult = 
		(
			glob.cdh->GetInputState(a_p->controllerID, InputAction::kLS).isPressed ? 
			1.5f * a_p->tm->magickaOverflowSlowdownFactor : 
			a_p->tm->magickaOverflowSlowdownFactor
		);
		// Interpolate between the base throw speed and the max throw speed
		// based on how long the bind was held for.
		// Cap velocity at max refr release speed.
		const float v = Util::InterpolateSmootherStep
		(
			releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed, 
			std::clamp
			(
				(
					releaseSpeedMult * 
					(Settings::fBaseMaxThrownObjectReleaseSpeed + releaseSpeedInc)
				),
				releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed,
				Settings::fAbsoluteMaxThrownRefrReleaseSpeed 
			), 
			normHoldTime
		);

		// Once the initial release speed is set, update the intercept position, 
		// if using aim prediction.
		releaseSpeed = v;
		if (trajType == ProjectileTrajType::kPrediction)
		{
			trajectoryEndPos = CalculatePredInterceptPos(a_p);
		}
		
		// NOTE:
		// Calcs do not account for air drag.
		// When the actor is aiming at a target, holding the grab bind
		// modifies the launch angle (flatter trajectory if held longer).
		const float xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
		const float z = (trajectoryEndPos.z - releasePos.z);
		auto dirToTarget = trajectoryEndPos - releasePos;
		dirToTarget.Unitize();
		// Angle straight at the new intercept position.
		launchYaw = Util::NormalizeAng0To2Pi(atan2f(dirToTarget.y, dirToTarget.x));
		// Bounds for launch pitch.
		float steepestLaunchAng = 0.0f;
		float flattestLaunchAng = 0.0f;
		// Only can hit the target with the calculated velocity
		// if the discriminant is positive.
		float discriminant = (v * v * v * v) - ((g * g * xy * xy) + (2 * g * z * v * v));
		bool withinRange = discriminant >= 0;
		if (!withinRange)
		{
			// Get max range launch angle when launched at max speed.
			// Pitch from release position to end position.
			float alpha = atanf(z / xy);
			// Halfway between the pitch between release and end positions
			// and the fully vertical pitch of 90 degrees.
			launchPitch = (PI / 2.0f) - (0.5f * (PI / 2.0f - alpha));
			canReachTarget = false;
		}
		else
		{
			// Two solutions from the discriminant.
			float plusSoln = atan2f(((v * v) + sqrtf(discriminant)), (g * xy));
			float minusSoln = atan2f(((v * v) - sqrtf(discriminant)), (g * xy));

			// NOTE: 
			// Pitch convention here is the opposite of the game's:
			// '+' is up, '-' is down.
			if (plusSoln >= minusSoln)
			{
				flattestLaunchAng = minusSoln;
				steepestLaunchAng = plusSoln;
			}
			else
			{
				flattestLaunchAng = plusSoln;
				steepestLaunchAng = minusSoln;
			}

			launchPitch = std::lerp(steepestLaunchAng, flattestLaunchAng, normHoldTime);
		}

		// New squared velocity based on the new launch pitch.
		float root = 
		(
			(g * xy * xy) / 
			(2.0f * cosf(launchPitch) * cosf(launchPitch) * (xy * tanf(launchPitch) - z))
		);
		releaseSpeed = withinRange ? sqrtf(root) : min(sqrtf(root), v);

		// Components of velocity.
		float velX = releaseSpeed * cosf(launchYaw) * cosf(launchPitch);
		float velY = releaseSpeed * sinf(launchYaw) * cosf(launchPitch);
		float velZ = releaseSpeed * sinf(launchPitch);

		// XY velocity remains constant throughout since air resistance 
		// is removed before setting the object's velocity below.
		const float velXY = releaseSpeed * cosf(launchPitch);
		// Set the time to reach the target position.
		initialTimeToTarget = 
		(
			velXY == 0.0f || !withinRange ? 
			Settings::fMaxSecsBeforeClearingReleasedRefr : 
			Util::GetXYDistance(releasePos, trajectoryEndPos) / velXY
		);

		releaseVelocity = RE::NiPoint3(velX, velY, velZ);
		releaseSpeed = releaseVelocity.Length();		
		// Set as thrown.
		isThrown = true;
		// Set as our release velocity.
		lastSetVelocity = releaseVelocity;
		lastSetTargetPosition = releasePos;
		// Released now.
		releaseTP = SteadyClock::now();

		return true;
	}

	void TargetingManager::ReleasedReferenceInfo::InitTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Set initial release trajectory info for this released refr.
		// Determines the released refr's initial orientation, speed, time to target,
		// targeted trajectory end position, and whether or not 
		// the released refr was thrown or dropped.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		canReachTarget = true;
		collisionActive = true;
		startedHomingIn = false;
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrPtrValidity = 
		(
			targetRefrPtr && 
			targetRefrPtr.get() &&
			Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActor = targetRefrPtrValidity ? targetRefrPtr->As<RE::Actor>() : nullptr;
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		if (targetActor)
		{
			trajectoryEndPos = Util::GetTorsoPosition(targetActor) + targetLocalPosOffset;
		}
		else if (targetRefrPtrValidity)
		{
			trajectoryEndPos = Util::Get3DCenterPos(targetRefrPtr.get()) + targetLocalPosOffset;
		}
		// Released from suspended position.
		releasePos = objectPtr->data.location;
		// Angle straight at the initial intercept position.
		launchPitch = -Util::NormalizeAngToPi
		(
			Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos)
		);
		launchYaw = Util::NormalizeAng0To2Pi
		(
			Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos))
		);
		trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[a_p->playerID]
		);

		// TODO: 
		// Snap released refr to the closest position 
		// on the targeted actor's character controller collider.
		// Set closest node and trajectory end position to that node's position
		// if aiming at an actor with aim correction.
		/*
		if (targetActorValidity && 
			Settings::vbUseAimCorrection[a_p->playerID] && 
			targetActorHandle == a_p->tm->crosshairRefrHandle)
		{
			if (auto actor3D = Util::GetRefr3D(targetActorPtr.get()); actor3D)
			{
				float closestNodeDist = FLT_MAX;
				RE::NiPoint3 nodeCenterOfMassPos = trajectoryEndPos;
				Util::TraverseChildNodesDFS
				(
					actor3D.get(),
					[this, &closestNodeDist, &nodeCenterOfMassPos](RE::NiAVObject* a_node) 
					{
						if (!a_node)
						{
							return;
						}

						auto node3DPtr = RE::NiPointer<RE::NiAVObject>(a_node); 
						if (!node3DPtr || !node3DPtr.get())
						{
							return;
						}

						auto hkpRigidBodyPtr = Util::GethkpRigidBody(node3DPtr.get());
						// Must have havok rigid body to collide with.
						if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get())
						{
							return;
						}

						// Check distance from the current trajectory end position 
						// (crosshair position) and the node's center of mass.
						const auto centerOfMass = 
						(
							ToNiPoint3
							(
								hkpRigidBodyPtr->motion.motionState.sweptTransform.centerOfMass0
							) * HAVOK_TO_GAME
						);
						float dist = centerOfMass.GetDistance(trajectoryEndPos); 
						if (dist < closestNodeDist)
						{
							// New closest node to the crosshair position.
							if (auto collidable = hkpRigidBodyPtr->GetCollidable(); collidable)
							{
								closestNodeDist = dist;
								targetedActorNode = RE::NiPointer<RE::NiAVObject>(a_node);
								nodeCenterOfMassPos = centerOfMass;
							}
						}
					}
				);

				// Set new trajectory end position to the node's center of mass.
				trajectoryEndPos = nodeCenterOfMassPos;
			}
		}
		*/

		auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get()); 
		bool hkpRigidBodyValidity = hkpRigidBodyPtr && hkpRigidBodyPtr.get();
		// Release velocity to apply.
		if (hkpRigidBodyValidity)
		{
			releaseVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity) * HAVOK_TO_GAME;
		}
		else if (auto asProjectile = objectPtr->As<RE::Projectile>(); 
				 asProjectile && isActiveProjectile)
		{
			releaseVelocity = asProjectile->linearVelocity;
		}
		else
		{
			objectPtr->GetLinearVelocity(releaseVelocity);
		}

		// Throw the refr if facing the crosshair position,
		// if it is not the player themselves, 
		// and if it is not the target refr.
		// Drop the refr otherwise.
		if (a_p->mm->reqFaceTarget && 
			objectPtr != a_p->coopActor &&
			refrHandle != a_p->tm->crosshairRefrHandle)
		{
			auto asActor = objectPtr->As<RE::Actor>();
			// Throw refr telekinetically.
			// Zero out velocity first.
			if (!isActiveProjectile)
			{
				hkpRigidBodyPtr->motion.SetPosition(releasePos * GAME_TO_HAVOK);
				hkpRigidBodyPtr->motion.SetLinearVelocity({ 0 });
				hkpRigidBodyPtr->motion.SetAngularVelocity({ 0 });
			}

			// Projectile motion equation used to get max 
			// and min launch angles at max launch speed,
			// and launch angle with minimized launch speed:
			// https://en.wikipedia.org/wiki/Projectile_motion#Angle_%CE%B8_required_to_hit_coordinate_(x,_y)

			// Get grab bind hold time, which directly influences 
			// the speed at which the refr is thrown.

			float cappedHoldTime = Settings::fGrabHoldSecsToMaxReleaseSpeed / 2.0f;
			float secondsSinceGrabRelease = a_p->pam->GetSecondsSinceLastStop
			(
				InputAction::kGrabObject
			);
			float triggeredByGrabRelease = a_p->pam->GetPlayerActionInputJustReleased
			(
				InputAction::kGrabObject, false
			);
			// Adjust release speed based on how long the grab bind was held for
			// if the bind was just released this frame.
			// Otherwise, the released refr could have been slapped down,
			// so we'll release it at the half the max speed.
			if (triggeredByGrabRelease)
			{
				auto actionIndex = !InputAction::kGrabObject - !InputAction::kFirstAction;
				cappedHoldTime = 
				(
					min
					(
						a_p->pam->paStatesList[actionIndex].secsPerformed,
						max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
					)
				);
			}

			// Normalize it.
			float normHoldTime = std::lerp
			(
				0.0f, 
				1.0f,
				cappedHoldTime / max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
			);
	
			// Get HMS AVs inc per level up.
			uint32_t iAVDhmsLevelUp = 10;
			auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
			if (valueOpt.has_value())
			{
				iAVDhmsLevelUp = valueOpt.value();
			}

			// Total increase to the player's magicka so far.
			// Default to serialized data.
			float magickaTotalInc = 
			(
				glob.serializablePlayerData.at
				(
					a_p->coopActor->formID
				)->hmsPointIncreasesList[1]
			);
			// Tack on any modifiers.
			magickaTotalInc += 
			(
				a_p->coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
				) +
				a_p->coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
				)
			);
			// Required total number of level ups put into magicka 
			// to get the current magicka level.
			float magickaLevelInc = magickaTotalInc / iAVDhmsLevelUp;
			// Increase max throw speed by 10% of the base throw speed per level up.
			float releaseSpeedInc = 
			(
				Settings::fBaseMaxThrownObjectReleaseSpeed * magickaLevelInc * 0.1f
			);
			// g is taken to be positive for all calculations below.
			double g = Util::GetGravitationalConstant();

			// Slow down the base projectile release speed if the player is low on magicka.
			// Will lower the damage dealt by this thrown object on hit,
			// since damage scales up with hit velocity.
			// Additional release speed multiplier if moving.
			float releaseSpeedMult = 
			(
				glob.cdh->GetInputState(a_p->controllerID, InputAction::kLS).isPressed ? 
				1.5f * a_p->tm->magickaOverflowSlowdownFactor : 
				a_p->tm->magickaOverflowSlowdownFactor
			);
			// Interpolate between the base throw speed and the max throw speed
			// based on how long the bind was held for.
			// Cap velocity at max refr release speed.
			const float v = Util::InterpolateSmootherStep
			(
				releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed, 
				std::clamp
				(
					(
						releaseSpeedMult * 
						(Settings::fBaseMaxThrownObjectReleaseSpeed + releaseSpeedInc)
					),
					releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed,
					Settings::fAbsoluteMaxThrownRefrReleaseSpeed 
				), 
				normHoldTime
			);
			
			// Once release speed is set, update the intercept position, if using aim prediction.
			releaseSpeed = v;
			if (trajType == ProjectileTrajType::kPrediction)
			{
				trajectoryEndPos = CalculatePredInterceptPos(a_p);
			}
			
			// NOTE: 
			// Calcs do not account for air drag.
			// When the actor is aiming at a target, holding the grab bind
			// modifies the launch angle (flatter trajectory if held longer).
			const float xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			const float z = (trajectoryEndPos.z - releasePos.z);
			auto dirToTarget = trajectoryEndPos - releasePos;
			dirToTarget.Unitize();
			// Angle straight at target.
			launchYaw = Util::NormalizeAng0To2Pi(atan2f(dirToTarget.y, dirToTarget.x));
			// Bounds for launch pitch.
			float steepestLaunchAng, flattestLaunchAng = 0.0f;
			// Only can hit the target with the calculated velocity
			// if the discriminant is positive.
			float discriminant = (v * v * v * v) - ((g * g * xy * xy) + (2 * g * z * v * v));
			bool withinRange = discriminant >= 0;
			if (withinRange)
			{
				// Two solutions from the discriminant.
				float plusSoln = atan2f(((v * v) + sqrtf(discriminant)), (g * xy));
				float minusSoln = atan2f(((v * v) - sqrtf(discriminant)), (g * xy));

				// NOTE: Pitch convention here is the opposite of the game's:
				// '+' is up, '-' is down.
				if (plusSoln >= minusSoln)
				{
					flattestLaunchAng = minusSoln;
					steepestLaunchAng = plusSoln;
				}
				else
				{
					flattestLaunchAng = plusSoln;
					steepestLaunchAng = minusSoln;
				}

				launchPitch = std::lerp(steepestLaunchAng, flattestLaunchAng, normHoldTime);
			}
			else
			{
				// Get max range launch angle when launched at max speed.
				// Pitch from release position to end position.
				float alpha = atanf(z / xy);
				// Halfway between the pitch between release and end positions
				// and the fully vertical pitch of 90 degrees.
				launchPitch = (PI / 2.0f) - (0.5f * (PI / 2.0f - alpha));
				canReachTarget = false;
			}

			// New squared velocity based on the new launch pitch.
			float root = 
			(
				(g * xy * xy) / 
				(2.0f * cosf(launchPitch) * cosf(launchPitch) * (xy * tanf(launchPitch) - z))
			);
			releaseSpeed = withinRange ? sqrtf(root) : min(sqrtf(root), v);

			// Components of velocity.
			float velX = releaseSpeed * cosf(launchYaw) * cosf(launchPitch);
			float velY = releaseSpeed * sinf(launchYaw) * cosf(launchPitch);
			float velZ = releaseSpeed * sinf(launchPitch);
			// XY velocity remains constant throughout since air resistance 
			// is removed before setting the object's velocity below.
			const float velXY = releaseSpeed * cosf(launchPitch);
			// Set the time to reach the target position.
			initialTimeToTarget = 
			(
				velXY == 0.0f || !withinRange ? 
				Settings::fMaxSecsBeforeClearingReleasedRefr : 
				Util::GetXYDistance(releasePos, trajectoryEndPos) / velXY
			);
			releaseVelocity = RE::NiPoint3(velX, velY, velZ);
			releaseSpeed = releaseVelocity.Length();

			// Zero out inertia and damping before throwing
			// and then revert inertia changes after launch.
			// Warning: hacky. Since I can't find a way of accounting for different
			// objects' air resistances, set air resistance to 0 so that the
			// thrown object will land directly at the target position
			// when launched with our previously computed launch velocity.
			if (hkpRigidBodyValidity)
			{
				hkpRigidBodyPtr->motion.motionState.angularDamping = 0.0f;
				hkpRigidBodyPtr->motion.motionState.linearDamping = 0.0f;
				hkpRigidBodyPtr->motion.SetLinearVelocity(releaseVelocity * GAME_TO_HAVOK);
			}

			// Set as thrown.
			isThrown = true;
		}
		else
		{
			// Drop object.
			if (hkpRigidBodyValidity)
			{
				hkpRigidBodyPtr->motion.motionState.angularDamping = 0.0f;
				hkpRigidBodyPtr->motion.motionState.linearDamping = 0.0f;
			}

			// Not thrown.
			isThrown = false;
		}

		// Set as our release velocity.
		lastSetVelocity = releaseVelocity;
		lastSetTargetPosition = releasePos;
		// Released now.
		releaseTP = SteadyClock::now();
	}

	void TargetingManager::RefrManipulationManager::AddGrabbedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle
	)
	{
		// Add the given refr to the managed grabbed refrs data set,
		// set its grab time point, and ragdoll the refr, if it is an actor,
		// to allow for positional manipulation.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr)
		{
			return;
		}

		// Must have space for another grabbed refr and not already grabbed..
		auto nextOpenIndex = grabbedRefrInfoList.size();
		if (nextOpenIndex >= Settings::uMaxGrabbedReferences ||
			grabbedRefrHandlesToInfoIndices.contains(a_handle))
		{
			return;
		}

		// Before adding the grabbed refr, 
		// if it was released earlier and still handled as a released refr,
		// remove the grabbed refr from the released list.
		if (IsReleased(a_handle)) 
		{
			ClearRefr(a_handle);
		}

		// Save handle-to-index mapping and then add to grabbed list.
		grabbedRefrHandlesToInfoIndices.insert({ a_handle, nextOpenIndex });
		grabbedRefrInfoList.emplace_back(std::make_unique<GrabbedReferenceInfo>(a_handle));

		const auto& info = grabbedRefrInfoList[nextOpenIndex];
		// Set grab TP.
		info->grabTP = SteadyClock::now();
		// Save the original collision layer right after grabbing.
		info->SaveCollisionLayer();
		// Ragdoll actor if necesssary to allow manipulation.
		if (auto asActor = objectPtr->As<RE::Actor>(); asActor)
		{
			// If the actor to ragdoll is this player, push upward slightly.
			// Otherwise, knock down.
			if (asActor == a_p->coopActor.get()) 
			{
				RE::NiPoint3 forceOrigin = Util::GetTorsoPosition(a_p->coopActor.get());
				// Adjust the force application point to allow the player
				// to either gain a bit more air when flopping while looking up,
				// or body slam with mean intentions when flopping while looking down.
				if (Settings::bAimPitchAffectsFlopTrajectory)
				{
					float forceOriginHeightOffset = 
					(
						a_p->coopActor->GetHeight() * 
						std::lerp(-0.5f, 0.5f, (a_p->mm->aimPitch + PI / 2.0f) / PI)
					);
					forceOrigin.z = forceOrigin.z + forceOriginHeightOffset;
				}

				Util::PushActorAway(asActor, forceOrigin, 10.0f);
			}
			else
			{
				Util::PushActorAway(asActor, asActor->data.location, -1.0f);
			}

			asActor->PotentiallyFixRagdollState();
		}

		// Set active projectile flag.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		info->isActiveProjectile = 
		(
			asProjectile && !asProjectile->ShouldBeLimited()
		);
	}

	void TargetingManager::RefrManipulationManager::AddReleasedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle
	) 
	{
		// Add the given refr to the managed released refrs data set.
		// Also set its release trajectory information (dropped or thrown)
		// to use when guiding its motion upon release.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr || !objectPtr.get())
		{
			return;
		}

		// Must not have been released already.
		if (releasedRefrHandlesToInfoIndices.contains(a_handle))
		{
			return;
		}

		auto nextOpenIndex = releasedRefrInfoList.size();
		// Store mapped index and then add to list.
		releasedRefrHandlesToInfoIndices.insert_or_assign(a_handle, nextOpenIndex);
		releasedRefrInfoList.emplace_back
		(
			std::make_unique<ReleasedReferenceInfo>(a_p->controllerID, a_handle)
		);
		const auto& info = releasedRefrInfoList[nextOpenIndex];
		// Set active projectile flag.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		info->isActiveProjectile = 
		(
			asProjectile && !asProjectile->ShouldBeLimited()
		);
		// Set initial homing/aim prediction trajectory info.
		info->InitTrajectory(a_p);
	}

	bool TargetingManager::RefrManipulationManager::CanGrabAnotherRefr()
	{
		// Return true if the number of managed grabbed refrs 
		// is less than the maximum allowable number of grabbed refrs.

		return grabbedRefrInfoList.size() < Settings::uMaxGrabbedReferences;
	}

	bool TargetingManager::RefrManipulationManager::CanGrabRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Return true if the given refr is valid, not already managed,
		// and there's room for another grabbed refr.

		return Util::HandleIsValid(a_handle) && !IsManaged(a_handle, true) && CanGrabAnotherRefr();
	}

	bool TargetingManager::RefrManipulationManager::CanManipulateGrabbedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index
	)
	{
		// Returns true if the given player can manipulate 
		// the managed grabbed refr at the given index.

		// Index must be less than the size of the grabbed refrs list.
		if (a_index >= grabbedRefrInfoList.size())
		{
			return false;
		}

		// Must have valid info.
		const auto& info = grabbedRefrInfoList[a_index];
		if (!info->IsValid())
		{
			return false;
		}

		// Must have a set grabbed time point.
		// Clear if not.
		auto objectPtr = info->refrHandle.get();
		if (!info->grabTP.has_value())
		{
			info->Clear();
			return false;
		}

		// Wait 1 second or until the actor is ragdolled, whichever comes first.
		// Do not manipulate until then.
		const auto asActor = objectPtr->As<RE::Actor>();
		float secsSinceRagdolled = Util::GetElapsedSeconds(info->grabTP.value());
		if (asActor && !asActor->IsInRagdollState() && secsSinceRagdolled < 1.0f)
		{
			return false;
		}

		// Game will automatically attempt to fix ragdoll states for actors after
		// a certain period of stable motion. 
		// Check if the grabbed actor is no longer ragdolling 
		// and either reset this player's grab-related state (if grabbing a player),
		// or attempt to re-grab the non-player actor.
		// Haven't found a hook to prevent the ragdoll timer from being set to 0 yet,
		// so chalk this solution up to more jank.
		if (asActor && !asActor->IsDead() && !asActor->IsInRagdollState())
		{
			if (GlobalCoopData::IsCoopPlayer(asActor)) 
			{
				// Player is not grabbed anymore.
				info->Clear();
				asActor->PotentiallyFixRagdollState();
				return false;
			}
			else
			{
				if (Settings::bRemoveGrabbedActorAutoGetUp && 
					asActor->currentProcess &&
					asActor->currentProcess->middleHigh)
				{
					// Paralyze living actors to prevent them from getting up constantly.
					if (asActor->boolBits.none(RE::Actor::BOOL_BITS::kParalyzed))
					{
						asActor->boolBits.set(RE::Actor::BOOL_BITS::kParalyzed);
					}

					// Knock 'em down again.
					Util::PushActorAway(asActor, asActor->data.location, 0.0f);
					asActor->PotentiallyFixRagdollState();
					asActor->currentProcess->middleHigh->unk2B0 = FLT_MAX;
					return true;
				}
				else
				{
					return false;
				}
			}
		}

		return true;
	}

	void TargetingManager::RefrManipulationManager::ClearAll()
	{
		// Clear all managed grabbed and released refrs + their cached data,
		// and also clear out all queued contact event-related data.
		// Player will not longer be grabbing any refrs afterward.
		
		{
			std::unique_lock<std::mutex> lock(contactEventsQueueMutex, std::try_to_lock);
			if (lock)
			{
				SPDLOG_DEBUG
				(
					"[TM] RefrManipulationManager: ClearAll: Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				collidedRefrFIDPairs.clear();
				queuedReleasedRefrContactEvents.clear();
			}
			else
			{
				SPDLOG_DEBUG
				(
					"[TM] RefrManipulationManager: ClearAll: Failed to obtain lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}

		ClearGrabbedRefrs();
		ClearReleasedRefrs();
		// No longer grabbing.
		isAutoGrabbing = isGrabbing = false;
		reqSpecialHitDamageAmount = 0.0f;
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedActors
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Clear all grabbed actors and refresh mappings if any were cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList,
			[&](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{
				auto refrPtr = Util::GetRefrPtrFromHandle(a_info->refrHandle);
				// Invalid or not an actor so do not clear.
				if (!refrPtr || !refrPtr.get() || !refrPtr->As<RE::Actor>())
				{
					return false;
				}
				
				// Restore cached collision layer before clearing.
				a_info->RestoreSavedCollisionLayer();
				auto asActor = refrPtr->As<RE::Actor>();
				// No longer paralyzed + signal to get up.
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				if (!asActor->IsDead()) 
				{
					asActor->NotifyAnimationGraph("GetUpBegin");
				}

				return true;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(true);
		}
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Clear the given refr from the grabbed list.
		// Refresh handle-to-index mappings if the refr was cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList,
			[&a_handle](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{
				if (a_info->refrHandle == a_handle)
				{
					// Restore cached collision layer before clearing.
					a_info->RestoreSavedCollisionLayer();
					return true;
				}

				return false;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(true);
		}

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr && refrPtr.get())
		{
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				// Ensure actors are no longer paralyzed.
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedRefrs() noexcept
	{
		// Clear all managed grabbed refrs + their cached data.

		for (const auto& info : grabbedRefrInfoList)
		{
			// Reset collision layer, just in case collisions were disabled.
			info->RestoreSavedCollisionLayer();

			const auto& handle = info->refrHandle;
			if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr&& refrPtr.get())
			{
				// Ensure actors are no longer paralyzed.
				if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
				{
					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}
		}

		grabbedRefrInfoList.clear();
		grabbedRefrHandlesToInfoIndices.clear();
	}

	void TargetingManager::RefrManipulationManager::ClearInactiveReleasedRefrs()
	{
		// Clear all managed released refrs that are no longer monitored for collisions
		// or have no recorded release time point.
		// Refresh handle-to-index mappings if at least one released refr was cleared.

		auto numErased = std::erase_if
		(
			releasedRefrInfoList, 
			[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info)
			{
				if (!a_info->collisionActive) 
				{
					const auto& handle = a_info->refrHandle;
					auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
					if (refrPtr && refrPtr.get())
					{
						// Ensure actors are no longer paralyzed.
						if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
						{
							asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
						}
					}
				}

				return !a_info->collisionActive || !a_info->releaseTP.has_value();
			}
		);

		if (numErased != 0) 
		{
			RefreshHandleToIndexMappings(false);
		}
	}

	void TargetingManager::RefrManipulationManager::ClearInvalidRefrs(bool&& a_grabbed)
	{
		// Clear all managed grabbed or released refrs that are no longer valid.
		// Refresh handle-to-index mappings if at least one refr was cleared.

		size_t numErased = 0;
		if (a_grabbed) 
		{
			numErased = std::erase_if
			(
				grabbedRefrInfoList, 
				[](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
				{
					if (!a_info->IsValid())
					{
						// Reset collision layer before clearing.
						a_info->RestoreSavedCollisionLayer();
						return true;
					}

					return false;
				}
			);
		}
		else
		{
			numErased = std::erase_if
			(
				releasedRefrInfoList, 
				[](const std::unique_ptr<ReleasedReferenceInfo>& a_info) 
				{ 
					return !a_info->IsValid();
				}
			);
		}

		if (numErased != 0) 
		{
			RefreshHandleToIndexMappings(a_grabbed);
		}
	}

	void TargetingManager::RefrManipulationManager::ClearPlayerIfGrabbed
	(
		const std::shared_ptr<CoopPlayer>& a_p)
	{
		// If the given player is grabbed by another active player,
		// have the grabbing player release this player.

		const auto handle = a_p->coopActor->GetHandle();
		for (const auto& otherP : glob.coopPlayers)
		{
			if (!otherP->isActive || otherP == a_p)
			{
				continue;
			}

			if (otherP->tm->rmm->IsManaged(handle, true))
			{
				otherP->tm->rmm->ClearRefr(handle);
				a_p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				a_p->coopActor->NotifyAnimationGraph("GetUpBegin");
				break;
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearRefr(const RE::ObjectRefHandle& a_handle)
	{
		// Clear the given refr from the grabbed and/or released lists.
		// Refresh handle-to-index mappings if the refr was cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList, 
			[&a_handle](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{ 
				if (a_info->refrHandle == a_handle)
				{
					// Restore original collision layer before clearing.
					a_info->RestoreSavedCollisionLayer();
					return true;
				}

				return false;
			}
		);

		if (numErased != 0) 
		{
			RefreshHandleToIndexMappings(true);
		}

		numErased = std::erase_if
		(
			releasedRefrInfoList, 
			[&a_handle](const std::unique_ptr<ReleasedReferenceInfo>& a_info) 
			{
				return a_info->refrHandle == a_handle;
			}
		);

		if (numErased != 0) 
		{
			RefreshHandleToIndexMappings(false);
		}

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr && refrPtr.get())
		{
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				// Ensure actors are no longer paralyzed.
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearReleasedRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Clear the given refr from the released list.
		// Refresh handle-to-index mappings if the refr was cleared.
		
		size_t numErased = std::erase_if
		(
			releasedRefrInfoList,
			[&a_handle](const std::unique_ptr<ReleasedReferenceInfo>& a_info) 
			{
				return a_info->refrHandle == a_handle;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(false);
		}

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr && refrPtr.get())
		{
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				// Ensure actors are no longer paralyzed.
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearReleasedRefrs() noexcept
	{
		// Clear all managed released refrs + their cached data.

		for (const auto& info : releasedRefrInfoList)
		{
			const auto& handle = info->refrHandle;
			auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
			if (!refrPtr || !refrPtr.get())
			{
				continue;
			}

			// Ensure all released actors are not paralyzed before clearing.
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			}
		}

		releasedRefrInfoList.clear();
		releasedRefrHandlesToInfoIndices.clear();
	}

	void TargetingManager::RefrManipulationManager::HandleQueuedContactEvents
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Ragdoll and apply damage to any hit actors from the contact events queue.

		// No released refrs, so no contact events to handle.
		if (a_p->tm->rmm->releasedRefrInfoList.empty())
		{
			return;
		}

		{
			std::unique_lock<std::mutex> lock
			(
				a_p->tm->rmm->contactEventsQueueMutex, std::try_to_lock
			);
			if (!lock)
			{
				return;
			}
			
			const auto& releasedRefrIndicesMap = 
			(
				a_p->tm->rmm->releasedRefrHandlesToInfoIndices
			);
			// Must obtain the point of contact between two collidables,
			// then get their handles and the associated refrs.
			RE::NiPoint3 contactPoint;
			RE::TESObjectREFRPtr refrPtrA = nullptr;
			RE::TESObjectREFRPtr refrPtrB = nullptr;
			// Unmanaged refr that the other managed refr collided with/was hit by.
			RE::TESObjectREFRPtr collidedWithRefrPtr = nullptr;
			// Rigid bodies for the hit refr and released refr, if any.
			RE::hkRefPtr<RE::hkpRigidBody> hitRigidBodyPtr{ nullptr };
			RE::hkRefPtr<RE::hkpRigidBody> releasedRigidBodyPtr{ nullptr };
			// Movin' through the queue.
			for (auto iter = queuedReleasedRefrContactEvents.begin(); 
				 iter != queuedReleasedRefrContactEvents.end(); 
				 ++iter)
			{
				const auto& contactEvent = *iter;
				// Must have two colliding bodies.
				if (!contactEvent->rigidBodyA || 
					!contactEvent->rigidBodyA.get() || 
					!contactEvent->rigidBodyB ||
					!contactEvent->rigidBodyB.get())
				{
					continue;
				}

				refrPtrA = Util::GetRefrPtrFromHandle(contactEvent->handleA);
				refrPtrB = Util::GetRefrPtrFromHandle(contactEvent->handleB);

				// SPECIAL CASE:
				// If one refr is invalid, it means a thrown refr collided with an object
				// that has no associated refr, such as a terrain block,
				// so we have to record the hit and potentially handle the splat and cleanup.
				if (!refrPtrA || !refrPtrA.get() || !refrPtrB || !refrPtrB.get()) 
				{
					auto releasedRefrPtr =
					(
						refrPtrA && refrPtrA.get() ?
						refrPtrA :
						refrPtrB && refrPtrB.get()?
						refrPtrB :
						nullptr
					);
					if (!releasedRefrPtr || !releasedRefrPtr.get())
					{
						continue;
					}

					auto releasedRefrHandle = releasedRefrPtr->GetHandle();
					if (!Util::HandleIsValid(releasedRefrHandle))
					{
						continue;
					}

					bool releasedByAPlayer = releasedRefrIndicesMap.contains(releasedRefrHandle);
					if (!releasedByAPlayer)
					{
						continue;
					}

					const auto index = releasedRefrIndicesMap.at(releasedRefrPtr->GetHandle());
					const auto& releasedRefrInfo = a_p->tm->rmm->releasedRefrInfoList[index];
					// Set first hit, if necessary.
					// Ignore hits within 10 frames of release to allow the released refr
					// to get off the ground and start on its trajectory if it hasn't already.
					// This applies to heavy or not very aerodynamic objects/actors,
					// such as fish, rabbits, crabs, and dragons.
					// Can't set through the AddHitRefr() func,
					// since terrain does not have an FID to store.
					if (!releasedRefrInfo->firstHitTP.has_value() &&
						releasedRefrInfo->releaseTP.has_value() &&
						Util::GetElapsedSeconds(releasedRefrInfo->releaseTP.value()) > 
						*g_deltaTimeRealTime * 10)
					{
						releasedRefrInfo->firstHitTP = SteadyClock::now();
					}

					// Now, check if the released refr was an actor,
					// and if so, apply splat damage.
					auto releasedActor = releasedRefrPtr->As<RE::Actor>();
					if (!releasedActor)
					{
						continue;
					}

					auto releasedActorHandle = releasedActor->GetHandle();
					// Hit 3D object without an associated refr.
					// eg. Navmesh or terrain block.
					a_p->tm->HandleSplat
					(
						releasedActorHandle, releasedRefrInfo->hitRefrFIDs.size() + 1
					);
					// We're done here.
					continue;
				}
				
				// Have to have two valid handles to handle collisions below.
				if (!Util::HandleIsValid(contactEvent->handleA) || 
					!Util::HandleIsValid(contactEvent->handleB))
				{
					continue;
				}

				// Check for instances where one of the two colliding refrs
				// is managed and the other is not.
				// Want to ignore collisions between non-managed refrs
				// and between two managed refrs.
				int32_t collidingReleasedRefrIndex = -1;
				if (releasedRefrIndicesMap.contains(contactEvent->handleA) && 
					!releasedRefrIndicesMap.contains(contactEvent->handleB))
				{
					collidedWithRefrPtr = refrPtrB;
					collidingReleasedRefrIndex = releasedRefrIndicesMap.at
					(
						contactEvent->handleA
					);
					hitRigidBodyPtr = contactEvent->rigidBodyB;
					releasedRigidBodyPtr = contactEvent->rigidBodyA;
				}

				if (releasedRefrIndicesMap.contains(contactEvent->handleB) && 
					!releasedRefrIndicesMap.contains(contactEvent->handleA))
				{
					collidedWithRefrPtr = refrPtrA;
					collidingReleasedRefrIndex = releasedRefrIndicesMap.at
					(
						contactEvent->handleB
					);
					hitRigidBodyPtr = contactEvent->rigidBodyA;
					releasedRigidBodyPtr = contactEvent->rigidBodyB;
				}

				// Why are you hitting yourself? Eh, whatever. Next!
				if (!collidedWithRefrPtr || 
					!collidedWithRefrPtr.get() || 
					collidedWithRefrPtr == a_p->coopActor)
				{
					continue;
				}

				// Ignore refrs without collision, such as activators.
				bool hasCollidable =
				(
					hitRigidBodyPtr &&
					hitRigidBodyPtr.get() && 
					hitRigidBodyPtr->GetCollidable()
				);
				if (!hasCollidable)
				{
					continue;
				}

				// No index for the managed refr.
				if (collidingReleasedRefrIndex == -1)
				{
					continue;
				}

				// Do not handle if the hit refr is a player that is dash dodging.
				auto hitPlayerIndex = GlobalCoopData::GetCoopPlayerIndex(collidedWithRefrPtr);
				if (hitPlayerIndex != -1 &&
					glob.coopPlayers[hitPlayerIndex]->mm->isDashDodging)
				{
					const auto& trajType = 
					(
						Settings::vuProjectileTrajectoryType[a_p->playerID]
					);
					// Also clear the released refr, so we don't continue 
					// setting its trajectory if it is homing in on the target.
					// Otherwise, it'll go through the player, come back around,
					// and hit the player once their dodge I-frames end.
					if (trajType == !ProjectileTrajType::kHoming)
					{
						ClearRefr(collidedWithRefrPtr->GetHandle());
					}

					continue;
				}
				
				// No released refr rigid body.
				if (!releasedRigidBodyPtr || !releasedRigidBodyPtr.get())
				{
					continue;
				}

				// Get released refr info now that the rigid body is valid.
				const auto& releasedRefrInfo =
				(
					a_p->tm->rmm->releasedRefrInfoList[collidingReleasedRefrIndex]
				);
				auto releasedRefrPtr = Util::GetRefrPtrFromHandle
				(
					releasedRefrInfo->refrHandle
				);
				// Don't want repeated hits.
				bool hasAlreadyHitRefr = releasedRefrInfo->HasAlreadyHitRefr
				(
					collidedWithRefrPtr.get()
				);
				// Ignore collisions between managed released refrs.
				if (a_p->tm->rmm->IsManaged(collidedWithRefrPtr->GetHandle(), false))
				{
					continue;
				}
				
				// Add hit.
				releasedRefrInfo->AddHitRefr(collidedWithRefrPtr.get());
				auto hitActor = collidedWithRefrPtr->As<RE::Actor>(); 
				// Managed refr hit a new actor that isn't itself. Bonk.
				bool shouldBonk = 
				(
					hitActor && 
					hitActor->currentProcess && 
					releasedRefrPtr != collidedWithRefrPtr && 
					!hasAlreadyHitRefr	
				);
				if (shouldBonk)
				{
					a_p->tm->HandleBonk
					(
						hitActor->GetHandle(), 
						releasedRefrPtr->GetHandle(),
						releasedRigidBodyPtr->motion.GetMass(),
						ToNiPoint3(releasedRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME),
						ToNiPoint3(contactEvent->contactPosition * HAVOK_TO_GAME)
					);
				}

				// Thrown actor hit a new refr that isn't itself. Splat.
				auto thrownActor = releasedRefrPtr->As<RE::Actor>(); 
				bool shouldSplat = 
				(
					thrownActor && 
					thrownActor != collidedWithRefrPtr.get() &&
					!hasAlreadyHitRefr && 
					!releasedRefrInfo->hitRefrFIDs.empty()
				);
				if (shouldSplat)
				{
					a_p->tm->HandleSplat
					(
						thrownActor->GetHandle(), releasedRefrInfo->hitRefrFIDs.size()
					);
				}


				// Get havok collision speed.
				float havokHitSpeed = contactEvent->contactSpeed;
				if (havokHitSpeed == 0.0f)
				{
					auto refr3DPtr = Util::GetRefr3D(releasedRefrPtr.get());
					if (refr3DPtr && refr3DPtr.get())
					{
						havokHitSpeed = Util::GetParentNodeHavokPointVelocity
						(
							refr3DPtr.get(), contactEvent->contactPosition
						).Length3();
					}
					else
					{
						havokHitSpeed = 
						(
							releasedRigidBodyPtr->motion.linearVelocity.Length3()
						);
					}
				}

				// Too slow to cause damage.
				if (havokHitSpeed < 1E-5f)
				{
					continue;
				}
				
				// Damage destructible objects.
				auto taskInterface = RE::TaskQueueInterface::GetSingleton(); 
				if (!taskInterface)
				{
					continue;
				}

				if (!releasedRefrPtr->Is(RE::FormType::ActorCharacter))
				{
					taskInterface->QueueUpdateDestructibleObject
					(
						releasedRefrPtr.get(),
						max(releasedRefrPtr->GetWeight(), 0.0f) * havokHitSpeed, 
						false,
						a_p->coopActor.get()
					);
				}
						
				if (!collidedWithRefrPtr->Is(RE::FormType::ActorCharacter))
				{
					taskInterface->QueueUpdateDestructibleObject
					(
						collidedWithRefrPtr.get(),
						max(releasedRefrPtr->GetWeight(), 0.0f) * havokHitSpeed,
						false, 
						a_p->coopActor.get()
					);
				}
			}

			// No more events to handle.
			queuedReleasedRefrContactEvents.clear();
		}
	}

	const bool TargetingManager::RefrManipulationManager::IsManaged
	(
		const RE::ObjectRefHandle& a_handle, bool a_grabbed
	)
	{
		// Check if the given refr is handled as either a grabbed or released refr, 
		// depending on the given grabbed flag.

		if (a_grabbed)
		{
			return grabbedRefrHandlesToInfoIndices.contains(a_handle);
		}
		else
		{
			return releasedRefrHandlesToInfoIndices.contains(a_handle);
		}
	}

	void TargetingManager::RefrManipulationManager::MoveUnloadedGrabbedObjectsToPlayer
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// NOTE:
		// Not working consistently enough and unused for now.
		// Move grabbed objects to the player. Should call when P1 has moved to a new location.
		// NOTE 2: 
		// Unfortunately, I could not get MoveTo() to work consistently with grabbed actors,
		// so only object teleportation between cells is supported.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		bool unloaded = false;
		for (uint8_t i = 0; i < grabbedRefrInfoList.size(); ++i)
		{
			// Move unloaded or far away grabbed objects to the player.
			auto& grabbedRefrInfo = grabbedRefrInfoList[i];
			if (!grabbedRefrInfo || !grabbedRefrInfo.get())
			{
				continue;
			}

			const auto& handle = grabbedRefrInfo->refrHandle;
			auto refrPtr = Util::GetRefrPtrFromHandle(handle);
			if (!refrPtr || !refrPtr.get() || refrPtr->As<RE::Actor>())
			{
				continue;
			}
			
			SPDLOG_DEBUG
			(
				"[TM] MoveUnloadedGrabbedRefrsToPlayer: {} was grabbed on unload.",
				refrPtr->GetName()
			);
			unloaded = 
			{
				(!refrPtr->IsDeleted()) &&
				(
					!refrPtr->Is3DLoaded() ||
					!refrPtr->loadedData ||
					!refrPtr->parentCell ||
					!refrPtr->parentCell->IsAttached() ||
					refrPtr->parentCell != p1->parentCell
				)
			};

			if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface) 
			{
				taskInterface->AddTask
				(
					[refrPtr, p1]() 
					{
						refrPtr->Disable();
						refrPtr->SetParentCell(p1->parentCell);
						SPDLOG_DEBUG
						(
							"[TM] MoveUnloadedGrabbedRefrsToPlayer: Moving {} to P1 on unload.",
							refrPtr->GetName()
						);
						refrPtr->MoveTo(p1);
						refrPtr->Enable(false);
					}
				);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::RefreshHandleToIndexMappings
	(
		const bool& a_grabbed
	)
	{
		// Reconstruct the grabbed/released handle-to-list-index mappings
		// to account for cleared refrs.

		uint32_t i = 0;
		if (a_grabbed) 
		{
			grabbedRefrHandlesToInfoIndices.clear();
			for (; i < grabbedRefrInfoList.size(); ++i)
			{
				grabbedRefrHandlesToInfoIndices.insert({ grabbedRefrInfoList[i]->refrHandle, i });
			}
		}
		else
		{
			releasedRefrHandlesToInfoIndices.clear();
			for (; i < releasedRefrInfoList.size(); ++i)
			{
				releasedRefrHandlesToInfoIndices.insert
				(
					{ releasedRefrInfoList[i]->refrHandle, i }
				);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ToggleGrabbedRefrCollisions()
	{
		// For all grabbed refrs, toggle collision off for refrs 
		// without the non-collidable collision layer,
		// and toggle collision on for refrs with that layer.

		if (!isGrabbing)
		{
			return;
		}

		for (const auto& info : grabbedRefrInfoList)
		{
			if (!info->IsValid())
			{
				continue;
			}

			info->ToggleCollision();
		}
	}

	void TargetingManager::RefrTargetMotionState::UpdateMotionState
	(
		RE::ObjectRefHandle a_targetRefrHandle
	)
	{
		// Update physical motion-related data for given the target refr.
		// Used for predictive projectile trajectory calculations.

		// New target.
		if (a_targetRefrHandle != targetRefrHandle) 
		{
			Refresh();
		}

		targetRefrHandle = a_targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle); 
		bool targetRefrValidity = 
		(
			targetRefrPtr && targetRefrPtr.get() && targetRefrPtr->IsHandleValid()
		);
		if (!targetRefrValidity)
		{
			return;
		}

		auto asActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		bool isActor = asActorPtr && asActorPtr.get();
		// Need a valid char controller for targeted actors.
		if (isActor && !asActorPtr->GetCharController())
		{
			return;
		}

		lastUpdateTP = SteadyClock::now();
		if (firstUpdate)
		{
			// Need to set both current and previous positions 
			// to the initial position on the first update.
			// Same with the previous and current yaw values.
			if (isActor)
			{
				pPos = cPos = Util::GetActorWorldPosition(asActorPtr.get());
				pVel = cVel = Util::GetActorLinearVelocity(asActorPtr.get());
				pYaw = cYaw = 
				(
					cVel.Length() == 0.0f ? 
					asActorPtr->GetHeading(false) : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}
			else
			{
				pPos = cPos = targetRefrPtr->data.location;
				RE::NiPoint3 linVel{ };
				targetRefrPtr->GetLinearVelocity(linVel);
				pVel = cVel = linVel;
				pYaw = cYaw = 
				(
					cVel.Length() == 0.0f ? 
					targetRefrPtr->data.angle.z : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}

			firstUpdate = false;
		}
		else
		{
			++avgDataFrameCount;

			// Set previous data to current data before updating the current data.
			pPos = cPos;
			pVel = cVel;
			pAccelPerFrame = cAccelPerFrame;
			pYaw = cYaw;
			pYawAngDeltaPerFrame = cYawAngDeltaPerFrame;

			if (isActor)
			{
				cPos = Util::GetActorWorldPosition(asActorPtr.get());
				cVel = Util::GetActorLinearVelocity(asActorPtr.get());
				cVel.Unitize();
				// The reported actor movement speed seems less noisy 
				// than the havok linear velocity magnitude.
				cVel *= asActorPtr->DoGetMovementSpeed();
				cYaw = 
				(
					cVel.Length() == 0.0f ? 
					asActorPtr->GetHeading(false) : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}
			else
			{
				cPos = targetRefrPtr->data.location;
				RE::NiPoint3 linVel{ };
				targetRefrPtr->GetLinearVelocity(linVel);
				cVel = linVel;
				cYaw = 
				(
					cVel.Length() == 0.0f ? 
					targetRefrPtr->data.angle.z : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}

			// Acceleration, change in speed and yaw per frame.
			cAccelPerFrame = (cVel - pVel);
			cSpeedDeltaPerFrame = (cVel.Length() - pVel.Length());
			cYawAngDeltaPerFrame = Util::NormalizeAngToPi(cYaw - pYaw);

			// Total-over-an-interval data used to get the average per interval.
			toiAccel += cAccelPerFrame;
			toiSpeedDelta += cSpeedDeltaPerFrame;
			toiVel += cVel;
			toiYawAngDelta += cYawAngDeltaPerFrame;

			// Reported velocity changes are drastic even over a short amount of time,
			// leading to large, accumulating errors in aim prediction.
			// Can't reliably use them alone, so the average-per-interval is also calculated.
			if (avgDataFrameCount == FRAMES_BETWEEN_AVG_DATA_UPDATES)
			{
				apiAccel = toiAccel / avgDataFrameCount;
				apiVel = toiVel / avgDataFrameCount;
				apiSpeedDelta = toiSpeedDelta / avgDataFrameCount;
				apiYawAngDelta = toiYawAngDelta / avgDataFrameCount;

				// Reset totals and frame count each time the averages are set.
				toiAccel = 
				toiVel = RE::NiPoint3();
				toiSpeedDelta =
				toiYawAngDelta =
				avgDataFrameCount = 0;
			}
		}
	}

	RE::NiPoint3 TargetingManager::ManagedProjTrajectoryInfo::CalculatePredInterceptPos
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const bool& a_adjustReleaseSpeed,
		double& a_timeToTarget
	)
	{
		// Calculate the position at which the launched projectile is likely to collide
		// with the target. 
		// Use the target's physical motion data to perform this calculation.
		// Adjust the projectile's release speed, if requested,
		// to allow it to hit the predicted target position, despite the effects of air drag.
		// 
		// Return the time taken to hit the target at the computed intercept position
		// through the outparam.
		// 'NaN' or 'inf' if the projectile cannot hit the target position.
		
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		// No valid target refr, so aim at the crosshair position.
		if (!targetRefrPtr || !targetRefrPtr.get())
		{
			double xy = Util::GetXYDistance(trajectoryEndPos, releasePos);
			double z = (trajectoryEndPos - releasePos).z;
			if (a_adjustReleaseSpeed)
			{
				releaseSpeed = GetReleaseSpeedToTarget(xy, z, -a_p->mm->aimPitch);
			}

			a_timeToTarget = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
			return trajectoryEndPos;
		}

		// Aim at the target actor if valid and not mounted,
		// or if mounted and selected with the crosshair.
		// Want to avoid shooting at an aim correction target 
		// while mounted and targeting another object.
		auto targetActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		auto targetActorValidity = targetActorPtr && targetActorPtr.get();
		bool aimAtActor = 
		{
			(targetActorValidity) && 
			(
				!a_p->coopActor->IsOnMount() || 
				targetActorPtr->GetHandle() == a_p->tm->selectedTargetActorHandle
			)
		};
		const auto currentAimedAtPos = trajectoryEndPos;
		// Max number of iterations, current number of iterations.
		uint8_t steps = 50;
		uint8_t step = 0;
		// Set the initial predicted intercept/hit position to the current aimed-at position.
		RE::NiPoint3 predHitPos = currentAimedAtPos;
		// Next predicted velocity for the target. Set to current velocity initially.
		RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
		// Axis to rotate velocity vector around.
		RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
		// Flight time deltas at which to bail out of the calculation loop.
		// Converging on a time-of-flight if below this value.
		double timeBailDeltaMin = 1E-4;
		// Diverging time-of-flight if above this value.
		double timeBailDeltaMax = 1000.0;
		// XY and Z offsets to the predicted position from the release position.
		double xy = Util::GetXYDistance(predHitPos, releasePos);
		double z = (currentAimedAtPos - releasePos).z;
		// Initial release speed, adjust as needed.
		double firstReleaseSpeed = 
		releaseSpeed = 
		(
			a_adjustReleaseSpeed ? 
			GetReleaseSpeedToTarget(xy, z, launchPitch) : 
			releaseSpeed
		);
		// Time to target, accounting for air resistance.
		double t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
		// Previously calculated time to target.
		double tPrev = 0.0;
		// Difference in the calculated times to target.
		double tDiff = fabsf(t - tPrev);
		// Current delta yaw and yaw rotation speed.
		const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
		float currentZRotSpeed = 0.0f;
		if (targetActorValidity)
		{
			currentZRotSpeed =
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ?
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z :
				0.0f
			);
		}
			
		float rotationSign = currentYawAngDelta < 0.0f ? -1.0f : 1.0f;
		// Average of current and average per interval yaw deltas.
		float avgYawDeltaPerFrame = 
		(
			(
				currentYawAngDelta +
				a_p->tm->targetMotionState->apiYawAngDelta
			) / 2.0f
		);
		// Average of current and average per interval change in speed.
		const float avgSpeedDelta = 
		(
			(
				a_p->tm->targetMotionState->cSpeedDeltaPerFrame +
				a_p->tm->targetMotionState->apiSpeedDelta
			) / 2.0f
		);

		// Attempt to accurately estimate the target intercept position
		// and continue looping until the reported time-to-target values converge
		// to below the minimum time diff (success), 
		// or diverge above the maximum time diff (failure),
		// or until the maximum number of iterations is reached (could go either way).
		while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
		{
			// SUPER NOTE: 
			// Everything below is obviously not mathematically correct, 
			// since the target's velocity and acceleration are changing constantly,
			// which means that finding the best predicted hit position
			// would require integration over the time of flight.
			// However the recorded acceleration and velocity motion data
			// for targets is very noisy, which leads to huge overshoots
			// when using the proper formulas for calculating the predicted position at time t.
			// This temporary, manually-tested solution performs slightly better 
			// than not accounting for angular velocitiy or acceleration at all.
			
			// Rotate predicted velocity vector by the yaw diff 
			// which will occur over the time delta.
			double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
			double speed = nPredTargetVel.Length();
			// Rotate and re-apply original speed.
			Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
			nPredTargetVel.Unitize();
			nPredTargetVel *= speed;
			// Offset the current aimed at position by the delta position calculated
			// using the position delta over the elapsed time frame from the previous iteration.
			auto posDelta = nPredTargetVel * (t - tPrev);
			predHitPos += posDelta;

			// Update positional offsets based on the new predicted hit position.
			xy = Util::GetXYDistance(predHitPos - releasePos);
			z = (predHitPos - releasePos).z;
			// Adjust the release speed to account for air drag again.
			if (a_adjustReleaseSpeed)
			{
				releaseSpeed = GetReleaseSpeedToTarget(xy, z, launchPitch);
			}

			// Set previous time to target to current.
			tPrev = t;
			// Update current time to target using the new XY positional offset
			// and release speed.
			t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
			// Calculate the change in time to target.
			tDiff = fabsf(t - tPrev);
			// On to the next step.
			++step;
		} 

		if (isnan(tDiff) || tDiff >= timeBailDeltaMax)
		{
			// Failure.
			// Set to initial release speed.
			releaseSpeed = firstReleaseSpeed;
			// Failed to find intercept position, 
			// so set to the initially-aimed-at position as a fallback.
			predHitPos = currentAimedAtPos;
		}

		// Set the final time to target before returning the predicted intercept position.
		xy = Util::GetXYDistance(predHitPos - releasePos);
		z = (predHitPos - releasePos).z;
		a_timeToTarget = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;

		return predHitPos;
	}

	double TargetingManager::ManagedProjTrajectoryInfo::GetReleaseSpeedToTarget
	(
		const double& a_xy, const double& a_z, const double& a_launchPitch
	)
	{
		// Accounting for linear air resistance, 
		// get the release speed required to hit the target position
		// given by the XY and Z offsets and the launch pitch.
		
		// Get release speed first.
		double releaseSpeedNew = releaseSpeed;
		double w = -exp((a_z * mu * mu / g) - (a_xy * tanf(a_launchPitch) * mu * mu / g) - 1.0);
		const auto solnPair = Util::LambertWFunc::ApproxRealSolutionBothBranches(w, 1E-10);
		// Two potential solutions.
		double launchSpeed1 = 
		(
			solnPair.first.has_value() ? 
			(a_xy * mu) / (cosf(a_launchPitch) * (solnPair.first.value() + 1.0)) : 
			-1.0
		);
		double launchSpeed2 = 
		(
			solnPair.second.has_value() ?
			(a_xy * mu) / (cosf(a_launchPitch) * (solnPair.second.value() + 1.0)) : 
			-1.0
		);

		// Set to whichever one is valid first.
		if (launchSpeed1 > 0.0)
		{
			releaseSpeedNew = launchSpeed1;
		}
		else if (launchSpeed2 > 0.0)
		{
			releaseSpeedNew = launchSpeed2;
		}

		// If using aim direction projectiles, 
		// arrows/bolts must be fully drawn to set to 
		// the exact release speed to reach the target.
		// Otherwise, set to the old release speed 
		// which will make the projectile fall short.
		if (trajType == ProjectileTrajType::kAimDirection &&
			isPhysicalProj && 
			releaseSpeed < maxReleaseSpeed) 
		{
			// Smallest of the three.
			return min(releaseSpeed, min(maxReleaseSpeed, releaseSpeedNew));
		}

		// Can't be larger than the max release speed.
		return min(maxReleaseSpeed, releaseSpeedNew);
	}

	float TargetingManager::ManagedProjTrajectoryInfo::GetRoughMinLaunchPitch
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Get rough estimate of the minimum launch pitch required to hit the target, 
		// based on the given projectile's release speed.
		
		// NOTE: 
		// Cannot analytically find a solution for this minimum launch pitch
		// when applying air resistance, so calculate the drop when aiming in a straight line 
		// at the predicted intercept position and then add the drop
		// to the predicted intercept position to compensate.
		// Finally, recalculate the pitch to the new temporary intercept position.
		// Still will fail to hit the intercept position at range, 
		// but is more accurate than simply setting the
		// minimum launch pitch to the straight-line-to-target pitch.

		float straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
		// Time to target and positional offsets, as usual.
		double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
		double tAirDrag = 
		(
			-log(1.0 - ((xy * mu) / (releaseSpeed * cosf(straightLinePitch)))) / mu
		);
		double z = 
		(
			(-g * tAirDrag / mu) + 
			((releaseSpeed * sinf(straightLinePitch) + g / mu) / mu) * 
			(1.0 - exp(-mu * tAirDrag))
		);

		double straightLineTrajDrop = trajectoryEndPos.z - releasePos.z - z;
		if (isnan(straightLineTrajDrop) || isinf(straightLineTrajDrop)) 
		{
			// Invalid drop result, so just aim straight at the intercept position.
			// Oh well.
			return straightLinePitch;
		}
		else
		{
			// Add the straight-line trajectory drop distance to compensate.
			return 
			(
				-Util::GetPitchBetweenPositions
				(
					releasePos, trajectoryEndPos + RE::NiPoint3(0.0f, 0.0f, straightLineTrajDrop)
				)
			);
		}
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetInitialBaseProjectileData
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		const RE::ObjectRefHandle& a_projectileHandle, 
		const float& a_releaseSpeed
	)
	{
		// Set physical data and projectile data that depends on the base projectile type
		// for the given projectile and its given initial release speed.
		
		RE::Projectile* projectile = nullptr;
		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle); 
		if (projectilePtr && projectilePtr.get()) 
		{
			projectile = projectilePtr->As<RE::Projectile>();
		}

		// Smart ptr was invalid, so its managed projectile is as well, return early.
		if (!projectile)
		{
			return;
		}

		// Base projectile-dependent data.
		projGravFactor = 1.0;
		if (const auto ammo = projectile->ammoSource; ammo && ammo->data.projectile)
		{
			// Is a physical projectile.
			isPhysicalProj = true;
			maxReleaseSpeed = ammo->data.projectile->data.speed;
			if (const auto weap = projectile->weaponSource; weap && weap->IsBow())
			{
				// Set release speed based on draw time.
				float fullDrawTime = 
				(
					0.4f + 
					(
						1.66f / 
						(
							weap->GetSpeed() * 
							(
								1.0f + (float)a_p->coopActor->HasPerk(glob.quickShotPerk)
							)
						)
					) +
					0.6f
				);
				float drawTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kAttackRH);
				float power = 
				(
					std::clamp(drawTime, fullDrawTime * 0.35f, fullDrawTime) / fullDrawTime
				);
				releaseSpeed = maxReleaseSpeed * power;
			}
			else
			{
				// Fixed initial release speed otherwise.
				releaseSpeed = ammo->data.projectile->data.speed;
			}

			// Add projectile base gravity factor.
			projGravFactor += ammo->data.projectile->data.gravity;
		}
		else if (auto avEffect = projectile->avEffect; avEffect && avEffect->data.projectileBase)
		{
			// Is a magic projectile.
			isPhysicalProj = false;
			// Max and initial release speeds are the same.
			maxReleaseSpeed = releaseSpeed = avEffect->data.projectileBase->data.speed;
			if (avEffect->data.projectileBase->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				// Beams and flames are not affected by gravity.
				projGravFactor = 0.0f;
			}
			else
			{
				// Add projectile base gravity factor.
				projGravFactor += avEffect->data.projectileBase->data.gravity;
			}
		}
		else
		{
			// Anything else.
			isPhysicalProj = true;
			maxReleaseSpeed = releaseSpeed = a_releaseSpeed;
		}

		// Physical constants.
		mu = Settings::fMu;
		g = Util::GetGravitationalConstant() * projGravFactor;
		// And lastly, the release position.
		releasePos = projectile->data.location;
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetInitialBaseProjectileData
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		RE::BGSProjectile* a_projectileBase, 
		RE::TESObjectWEAP* a_weaponSource,
		RE::EffectSetting* a_magicEffectSource, 
		const RE::NiPoint3& a_releasePos
	)
	{
		// Set based on a base projectile form.
		// Used to obtain trajectory data when a projectile has not been fired yet.
		// If wishing to set a magic projectile's trajectory, specify the magic effect
		// associated with the projectile; nullptr if not a magical projectile.
		
		if (!a_projectileBase)
		{
			return;
		}

		// Base projectile-dependent data.
		projGravFactor = 1.0;
		if (a_magicEffectSource && a_magicEffectSource->data.projectileBase)
		{
			// Is a magic projectile.
			isPhysicalProj = false;
			// Max and initial release speeds are the same.
			maxReleaseSpeed = releaseSpeed = a_magicEffectSource->data.projectileBase->data.speed;
			if (a_magicEffectSource->data.projectileBase->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				// Beams and flames are not affected by gravity.
				projGravFactor = 0.0f;
			}
			else
			{
				// Add gravity factor.
				projGravFactor += a_magicEffectSource->data.projectileBase->data.gravity;
			}
		}
		else if (a_weaponSource)
		{
			// Is a physical projectile.
			isPhysicalProj = true;
			maxReleaseSpeed = a_projectileBase->data.speed;
			if (a_weaponSource->IsBow())
			{
				// Set release speed based on draw time.
				float fullDrawTime = 
				(
					0.4f + 
					(
						1.66f / 
						(
							a_weaponSource->GetSpeed() * 
							(
								1.0f + (float)a_p->coopActor->HasPerk(glob.quickShotPerk)
							)
						)
					) + 
					0.6f
				);
				float drawTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kAttackRH);
				float power = 
				(
					std::clamp(drawTime, fullDrawTime * 0.35f, fullDrawTime) / fullDrawTime
				);
				releaseSpeed = maxReleaseSpeed * power;
			}
			else
			{
				// Fixed initial release speed otherwise.
				releaseSpeed = a_projectileBase->data.speed;
			}
			
			// Add projectile base gravity factor.
			projGravFactor += a_projectileBase->data.gravity;
		}
		else
		{
			// Anything else.
			isPhysicalProj = true;
			maxReleaseSpeed = releaseSpeed = 2500.0f;
		}

		// Physical constants.
		mu = Settings::fMu;
		g = Util::GetGravitationalConstant() * projGravFactor;
		// And lastly, the release position.
		releasePos = a_releasePos;
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::ObjectRefHandle& a_projectileHandle, 
		RE::NiPoint3& a_initialVelocityOut, 
		const ProjectileTrajType& a_trajType
	)
	{
		// Sets up the initial trajectory data for the given projectile
		// based on the given starting velocity (which is modified) and the trajectory type.
		// NOTE: 
		// Should be run once when the projectile of interest is released.

		RE::Projectile* projectile = nullptr;
		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
		if (projectilePtr && projectilePtr.get())
		{
			projectile = projectilePtr->As<RE::Projectile>();
		}

		// Smart ptr was invalid, so its managed projectile is as well.
		if (!projectile)
		{
			return;
		}

		// Set initial base projectile data first.
		SetInitialBaseProjectileData(a_p, a_projectileHandle, a_initialVelocityOut.Length());
		// Set trajectory data common to both the predicted and at-launch trajectories.
		SetTrajectory
		(
			a_p, 
			projectile->data.location,
			a_trajType, 
			projectile->data.angle.z, 
			projectile->As<RE::BeamProjectile>() || projectile->As<RE::FlameProjectile>()
		);

		// Re-scale the initial velocity sent on launch to our computed release speed.
		a_initialVelocityOut.Unitize();
		a_initialVelocityOut *= releaseSpeed;

		// Perform ammo projectile damage scaling based on 
		// the ratio of the computed release speed over the max release speed.
		// Projectile power always defaults to 1 for companion players,
		// so we can scale it directly.
		// For P1, we have to compute the original weapon damage (before power scales it)
		// by dividing the current weapon damage by the power set on launch. 
		// Then we also scale the power/resultant weapon damage 
		// based on our own release speed factor.
		// This will directly adjust the output damage of the projectile on hit.
		if (const auto ammo = projectile->ammoSource; ammo && ammo->data.projectile)
		{
			// Scale arrow/bolt's damage based on the computed power.
			double releaseSpeedFactor = std::clamp(releaseSpeed / maxReleaseSpeed, 0.1, 1.0);
			if (a_p->isPlayer1) 
			{
				float originalWeaponDamage = 
				(
					projectile->weaponDamage / max(0.1f, projectile->power)
				);
				projectile->weaponDamage = originalWeaponDamage * releaseSpeedFactor;
				projectile->power = releaseSpeedFactor;
			}
			else
			{
				projectile->weaponDamage *= releaseSpeedFactor;
				projectile->power = releaseSpeedFactor;
			}
		}
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		RE::BGSProjectile* a_projectileBase, 
		RE::TESObjectWEAP* a_weaponSource,
		RE::EffectSetting* a_magicEffectSource, 
		const RE::NiPoint3& a_releasePos,
		const ProjectileTrajType& a_trajType
	)
	{
		// Sets up the initial trajectory data for the given projectile
		// based on the given starting velocity (which is modified) and the trajectory type.

		if (!a_projectileBase)
		{
			return;
		}

		// Set initial base projectile data first.
		SetInitialBaseProjectileData
		(
			a_p, a_projectileBase, a_weaponSource, a_magicEffectSource, a_releasePos
		);
		// Targeting angle at which the projectile would be released.
		float targetingAngle = 
		(
			a_p->pam->isAttacking ? 
			Util::DirectionToGameAngYaw(a_p->mm->playerDefaultAttackSourceDir) :
			a_p->coopActor->data.angle.z	
		);
		// Set trajectory data common to both the predicted and at-launch trajectories.
		SetTrajectory
		(
			a_p, 
			a_releasePos,
			a_trajType, 
			targetingAngle,
			a_projectileBase->data.types.any
			(
				RE::BGSProjectileData::Type::kBeam,
				RE::BGSProjectileData::Type::kFlamethrower
			)
		);
	}

	
	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::NiPoint3& a_releasePos,
		const ProjectileTrajType& a_trajType, 
		const float& a_initialYaw, 
		const bool& a_setStraightTrajectory
	)
	{
		// Sets up the initial trajectory data in common 
		// for both projected and launch trajectories.

		trajType = a_trajType;
		startedHomingIn = false;
		canReachTarget = true;
		// Set target refr regardless of projectile trajectory type.
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrValidity = 
		(
			targetRefrPtr && 
			targetRefrPtr.get() && 
			Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActorPtr = 
		(
			targetRefrValidity ? RE::ActorPtr(targetRefrPtr->As<RE::Actor>()) : nullptr
		);
		bool targetActorValidity = targetActorPtr && targetActorPtr.get();
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		// When facing the crosshair, choose the exact crosshair position 
		// locally offset from the target refr; 
		// otherwise, target the selected refr's center position.
		// Done to maximize hit chance, especially for actors,
		// since an actor's center position is most likely 
		// to be within their character controller collider.
		if (targetRefrValidity) 
		{
			trajectoryEndPos =
			(
				targetActorValidity ? 
				Util::GetTorsoPosition(targetActorPtr.get()) :
				Util::Get3DCenterPos(targetRefrPtr.get())
			);
			// Refr is selected by the crosshair and the player is facing it.
			if (a_p->mm->reqFaceTarget && targetRefrHandle == a_p->tm->crosshairRefrHandle) 
			{
				trajectoryEndPos += targetLocalPosOffset;
			}
		}

		// TODO: 
		// Snap released refr to the closest position 
		// on the targeted actor's character controller collider.
		// If just fired and aiming at an actor, 
		// direct at the closest node to the crosshair world position.
		// Seems to improve hit recognition, since the crosshair world position
		// may not always intersect with the target's collision volumes.
		// Alas, projectiles still pass through actors without colliding at times.
		/*
		if (targetActorValidity && 
			Settings::vbUseAimCorrection[a_p->playerID] && 
			targetActorHandle == a_p->tm->crosshairRefrHandle)
		{
			auto actor3DPtr = Util::GetRefr3D(targetActorPtr.get()); 
			if (!actor3DPtr || !actor3DPtr.get())
			{
				return;
			}

			float closestNodeDist = FLT_MAX;
			RE::NiPoint3 nodeCenterOfMassPos = trajectoryEndPos;
			Util::TraverseChildNodesDFS
			(
				actor3DPtr.get(), 
				[this, &closestNodeDist, &nodeCenterOfMassPos](RE::NiAVObject* a_node) 
				{
					if (!a_node)
					{
						return;
					}

					auto node3DPtr = RE::NiPointer<RE::NiAVObject>(a_node); 
					if (!node3DPtr || !node3DPtr.get())
					{
						return;
					}

						// Must have havok rigid body to collide with.
					auto hkpRigidBodyPtr = Util::GethkpRigidBody(node3DPtr.get()); 
					if (!hkpRigidBodyPtr || !hkpRigidBodyPtr.get())
					{
						return;
					}

					// Get center of mass to gauge distance.
					const auto centerOfMass = 
					(
						ToNiPoint3
						(
							hkpRigidBodyPtr->motion.motionState.sweptTransform.centerOfMass0
						) * HAVOK_TO_GAME
					);
					float dist = centerOfMass.GetDistance(trajectoryEndPos); 
					if (dist >= closestNodeDist)
					{
						return;
					}

					auto collidable = hkpRigidBodyPtr->GetCollidable(); 
					if (!collidable)
					{
						return;
					}
					
					// New closest node. Set closest distance too.
					closestNodeDist = dist;
					targetedActorNode = RE::NiPointer<RE::NiAVObject>(a_node);
					nodeCenterOfMassPos = centerOfMass;
				}
			);

			// Set to closest node's center of mass.
			trajectoryEndPos = nodeCenterOfMassPos;
		}
		*/

		// Firing an aim prediction or aim direction projectile 
		// while aiming at an actor or facing the target refr.
		bool predictInterceptPos = 
		(
			(a_trajType != ProjectileTrajType::kHoming) && 
			(!a_setStraightTrajectory) &&
			((targetActorValidity) || (a_p->mm->reqFaceTarget))
		);

		if (predictInterceptPos) 
		{
			// XY and Z offsets from the release position to the trajectory end position.
			double xy = 0.0;
			double z = 0.0;

			float minLaunchPitch = GetRoughMinLaunchPitch(a_p);
			launchPitch = std::clamp
			(
				a_p->mm->aimPitchManuallyAdjusted ?
				-a_p->mm->aimPitch :
				minLaunchPitch, 
				-89.9f * PI / 180.0f,
				89.9f * PI / 180.0f
			);
			
			// Add some arc to fast projectiles by decreasing their release speed
			// when released at a steeper angle.
			float straightLinePitch = -Util::GetPitchBetweenPositions
			(
				releasePos, trajectoryEndPos
			);
			if (releaseSpeed > 10000.0f)
			{
				maxReleaseSpeed = releaseSpeed;
				double ratio = std::clamp
				(
					1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
				);
				releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
			}

			// Calculate the position at which the projectile is predicted to hit the target actor.
			// As of now, no release speed modifications for projectiles.
			// Used to only modify the release speed of physical projectiles instead of modifying 
			// the gravitational constant to hit the target position,
			// but doing so would negate the bow draw-time mechanic, so I've decided against it.
			// 
			// Set both the intercept position and the initial time to reach that position.
			trajectoryEndPos = CalculatePredInterceptPos(a_p, false, initialTrajTimeToTarget);
			// Launch towards the computed end position.
			launchYaw = Util::ConvertAngle
			(
				Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
			);
			// XY offset from release pos to trajectory end pos.
			xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			// Z offset from release pos to trajectory end pos.
			z = (trajectoryEndPos - releasePos).z;

			// Since we are not modifying the projectile's release speed,
			// in order to still hit the intercept position, 
			// we modify the gravitational constant.
			// Better obviously for launching accurate, fast-arcing projectiles
			// instead of lowering the release speed while keeping g constant
			// in order to hit the same position.
			// But looks a bit odd for flat trajectory shots when g is low.
			// Tradeoffs, schmadeoffs.
			
			// Save base projectile-determined gravitational constant.
			float baseG = g;
			// New g to allow the projectile to hit the target.
			g = 
			(
				(
					(mu * mu * releaseSpeed * cosf(launchPitch)) * 
					(z - xy * tanf(launchPitch))
				) / 
				(
					(
						releaseSpeed * 
						cosf(launchPitch) * 
						log(1 - (xy * mu) / (releaseSpeed * cosf(launchPitch)))
					) + xy * mu
				)
			);

			// Can reach if:
			// 1. Gravity does not have to be negated -OR-
			// 2. If the player has not adjusted their aim -AND-
			// 3. The time to target is beyond the manageable interval -AND-
			// 4. The gravitational constant and time to target are valid.
			canReachTarget = 
			(
				(g >= 0.0f || !a_p->mm->aimPitchManuallyAdjusted) &&
				initialTrajTimeToTarget > 0.0f &&
				initialTrajTimeToTarget < Settings::fMaxProjAirborneSecsToTarget &&
				!isnan(g) &&
				!isinf(g) &&
				!isnan(initialTrajTimeToTarget) &&
				!isinf(initialTrajTimeToTarget)
			);

			// Reset g to default for a more natural path when not reaching the target.
			if (!canReachTarget)
			{
				g = baseG;
				if (isnan(initialTrajTimeToTarget) || isinf(initialTrajTimeToTarget))
				{
					// Shoot far away in the aiming direction.
					initialTrajTimeToTarget = Settings::fMaxProjAirborneSecsToTarget;
					xy = 
					(
						(releaseSpeed * cosf(launchPitch) / mu) * 
						(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
					);
					z = 
					(
						(-g * Settings::fMaxProjTrajectorySecsToTarget / mu) + 
						((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
						(1.0 - exp(-mu *Settings::fMaxProjTrajectorySecsToTarget))
					);
				}
				else
				{
					// Finite and manageable time to target, 
					// so maintain launch orientation and speed.
					xy = 
					(
						(releaseSpeed * cosf(launchPitch) / mu) * 
						(1.0 - exp(-mu * initialTrajTimeToTarget))
					);
					z = 
					(
						(-g * initialTrajTimeToTarget / mu) + 
						((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
						(1.0 - exp(-mu * initialTrajTimeToTarget))
					);
				}

				// If the projectile cannot reach the target, 
				// adjust the trajectory end position to a position 
				// far away in the player's aiming direction.
				trajectoryEndPos = RE::NiPoint3
				(
					releasePos.x + xy * cosf(launchYaw), 
					releasePos.y + xy * sinf(launchYaw),
					releasePos.z + z
				);
			}
		}
		else if (a_trajType == ProjectileTrajType::kHoming)
		{
			// Set straight-line pitch from release position to end position
			// after calculating the trajectory end position.
			// NOTE: 
			// Launch pitch/straight line pitch is sign-flipped
			// relative to the game's pitch sign convention.
			float straightLinePitch = -a_p->mm->aimPitch;
			// Aim at target actor if valid and not mounted,
			// or if mounted and selected with the crosshair.
			// Want to avoid shooting at an aim correction target 
			// while mounted and targeting another object.
			auto selectedTargetActorPtr = Util::GetActorPtrFromHandle
			(
				a_p->tm->selectedTargetActorHandle
			);
			bool aimAtActor = 
			{
				(targetActorValidity) && 
				(!a_p->coopActor->IsOnMount() || targetActorPtr == selectedTargetActorPtr)
			};
			if (aimAtActor) 
			{
				auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle
				(
					a_p->tm->aimCorrectionTargetHandle
				);
				// If the target is an aim correction target, target the torso.
				trajectoryEndPos = 
				(
					!selectedTargetActorPtr && 
					aimCorrectionTargetPtr && aimCorrectionTargetPtr.get() ? 
					Util::GetTorsoPosition(aimCorrectionTargetPtr.get()) : 
					trajectoryEndPos
				);
				straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);

				// Set launch angles, always above the straight line pitch.
				launchPitch = std::clamp
				(
					max(straightLinePitch, -a_p->mm->aimPitch), 
					-89.9f * PI / 180.0f, 
					89.9f * PI / 180.0f
				);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);

				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (releaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = releaseSpeed;
					double ratio = std::clamp
					(
						1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
					);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE:
				// No air resistance, so the XY component of velocity 
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max
				(
					0.0, 
					Util::GetXYDistance(releasePos, trajectoryEndPos) / 
					(releaseSpeed * cosf(launchPitch))
				);
			}
			else
			{
				// XY offset to trajectory end position.
				double xy = 0.0f;
				// Aim as far away as the max navmesh move distance 
				// or crosshair world position, whichever is farther away.
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = 
				(
					iniPrefSettings ? 
					iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : 
					nullptr
				); 
				if (projMaxDistSetting && 
					releasePos.GetDistance(a_p->tm->crosshairWorldPos) <
					projMaxDistSetting->data.f)
				{
					xy = projMaxDistSetting->data.f;
					trajectoryEndPos = RE::NiPoint3
					(
						releasePos.x + xy * cosf(launchYaw),
						releasePos.y + xy * sinf(launchYaw),
						releasePos.z
					);
				}
				else
				{
					trajectoryEndPos = a_p->tm->crosshairWorldPos;
					xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
				}

				straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
				// Set launch angles.
				launchPitch = std::clamp
				(
					max(straightLinePitch, -a_p->mm->aimPitch),
					-89.9f * PI / 180.0f,
					89.9f * PI / 180.0f
				);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);

				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (releaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = releaseSpeed;
					double ratio = std::clamp
					(
						1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
					);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE: 
				// No air resistance, so the XY component of velocity
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max(0.0, xy / (releaseSpeed * cosf(launchPitch)));
			}

			// The usual, except without air resistance.
			double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			double z = (releasePos - trajectoryEndPos).z;
			g = 
			(
				(2.0 / xy) * 
				(
					(powf(releaseSpeed, 2.0f) * cosf(launchPitch) * sinf(launchPitch)) + 
					((z * powf(releaseSpeed * cosf(launchPitch), 2.0f)) / (xy))
				)
			);
			g = isnan(g) || isinf(g) ? g = 0.0 : g;
		}
		else if (a_setStraightTrajectory)
		{
			// Aim far away in the projectile's initial facing direction, 
			// or directly at the current crosshair position without modifying the release speed.

			// Set launch angles, end position, and time to target.
			if (!a_p->mm->reqFaceTarget && !targetActorValidity) 
			{
				// Launch far away in aiming direction.
				launchPitch = -a_p->mm->aimPitch;
				launchYaw = Util::ConvertAngle(a_initialYaw);
				RE::NiPoint3 launchDir = Util::RotationToDirectionVect(launchPitch, launchYaw);
				// Choose endpoint that is far from the release point.
				double farDist = FLT_MAX;
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = 
				(
					iniPrefSettings ?
					iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : 
					nullptr
				); 
				if (projMaxDistSetting && 
					releasePos.GetDistance(a_p->tm->crosshairWorldPos) < 
					projMaxDistSetting->data.f)
				{
					farDist = projMaxDistSetting->data.f;
				}
				else
				{
					farDist = max
					(
						Settings::fMaxRaycastAndZoomOutDistance, 
						releasePos.GetDistance(a_p->tm->crosshairWorldPos)
					);
				}

				trajectoryEndPos = releasePos + launchDir * farDist;
			}
			else
			{
				launchPitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);
			}

			initialTrajTimeToTarget = releasePos.GetDistance(trajectoryEndPos) / releaseSpeed;
		}
		else
		{
			// Aim direction projectile and not facing the crosshair world position.
			// Aim far, far away in the direction that the player is facing.

			// Set launch angles.
			launchPitch = std::clamp
			(
				-a_p->mm->aimPitch, -89.9f * PI / 180.0f, 89.9f * PI / 180.0f
			);
			launchYaw = Util::ConvertAngle(a_p->coopActor->GetAimHeading());

			// Choose endpoint that is far from the release point.
			// Default time of flight is arbitrary, but should be relatively large.
			// Accounting for air resistance.
			double xy = 0.0;
			double z = 0.0;
			double tAirDrag = 0.0;

			auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
			auto projMaxDistSetting = 
			(
				iniPrefSettings ? iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : nullptr
			); 
			if (projMaxDistSetting) 
			{
				xy = projMaxDistSetting->data.f;;
				tAirDrag = 
				(
					-log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu
				);
				z = 
				(
					(-g * tAirDrag / mu) + 
					((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
					(1.0 - exp(-mu * tAirDrag))
				);
			}
			else
			{
				xy = 
				(
					(releaseSpeed * cosf(launchPitch) / mu) * 
					(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
				);
				tAirDrag = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
				z = 
				(
					(-g * Settings::fMaxProjTrajectorySecsToTarget / mu) + 
					((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
					(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
				);
			}
			
			initialTrajTimeToTarget = 
			(
				isnan(tAirDrag) ? 
				static_cast<double>(Settings::fMaxProjAirborneSecsToTarget) :
				std::clamp
				(
					tAirDrag,
					0.0,
					static_cast<double>(Settings::fMaxProjAirborneSecsToTarget)
				)
			);

			trajectoryEndPos = RE::NiPoint3
			(
				releasePos.x + xy * cosf(launchYaw), 
				releasePos.y + xy * sinf(launchYaw),
				releasePos.z + z
			);
		}
	}

	void TargetingManager::ManagedProjectileHandler::Insert
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::ObjectRefHandle& a_projectileHandle, 
		RE::NiPoint3& a_initialVelocityOut, 
		const ProjectileTrajType& a_trajType
	)
	{
		// Insert the given projectile into the managed list.
		// Then set its trajectory information 
		// and update its initial velocity through the outparam.

		// Housekeeping first.
		// Keep the managed projectiles map at a modest size by removing expired projectiles
		// if the queue size is above a certain threshold.
		if (managedProjHandleToTrajInfoMap.size() >= 
			Settings::uManagedPlayerProjectilesBeforeRemoval)
		{
			RE::Projectile* projectile = nullptr;
			RE::TESObjectREFRPtr projectilePtr{ };
			for (const auto& [handle, _] : managedProjHandleToTrajInfoMap) 
			{
				projectilePtr = Util::GetRefrPtrFromHandle(handle);
				if (projectilePtr)
				{
					projectile = projectilePtr->As<RE::Projectile>();
				}

				// Remove if:
				// Invalid, not loaded, deleted, marked for deletion, 
				// has collided (if not a beam or flames), or limited.
				bool shouldRemove = 
				{
					(!projectile) ||
					(!projectile->Is3DLoaded()) ||
					(projectile->IsDeleted()) ||
					(projectile->IsMarkedForDeletion()) ||
					(
						!projectile->As<RE::BeamProjectile>() && 
						!projectile->As<RE::FlameProjectile>() && 
						!projectile->impacts.empty()
					) ||
					(projectile->ShouldBeLimited())
				};
				if (shouldRemove) 
				{
					managedProjHandleToTrajInfoMap.erase(handle);
				}
			}
		}

		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
		// Smart ptr was invalid, so its managed projectile is as well.
		if (!projectilePtr || !projectilePtr.get())
		{
			return;
		}

		// Insert constructed trajectory info for this projectile.
		// NOTE: 
		// Construction sets all the trajectory data automatically.
		managedProjHandleToTrajInfoMap.insert_or_assign
		(
			a_projectileHandle, 
			std::make_unique<ManagedProjTrajectoryInfo>
			(
				a_p, a_projectileHandle, a_initialVelocityOut, a_trajType
			)
		);
	}
}

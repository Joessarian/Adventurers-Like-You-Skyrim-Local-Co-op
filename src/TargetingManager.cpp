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
			ALYSLC::Log("[TM] Initialize: Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count());
			RefreshData();
		}
		else
		{
			logger::error("[TM] ERR: Initialize: Cannot construct Targeting Manager for controller ID {}.", a_p ? a_p->controllerID : -1);
		}
	}

	void TargetingManager::MainTask()
	{
		// Update target motion state.
		targetMotionState->UpdateMotionState(GetRangedTargetActor());
		// Update crosshair position and selection, 
		// and draw the crosshair, player indicator, and aim pitch indicator
		// if no fullscreen menu is open or not controlling menus.
		DrawTargetingOverlay();
		// Open/close QuickLoot menu if the mod is installed and if targeting a valid refr.
		HandleQuickLootMenu();
		// Update the player's detection state and award Sneak skill XP as necessary.
		UpdateSneakState();
		// Update the player's crosshair text entry with externally-requested
		// or periodic information.
		UpdateCrosshairMessage();
		// Select or clear the aim correction target if aim correction is enabled.
		UpdateAimCorrectionTarget();
		// Handle grabbed reference motion and positioning.
		HandleReferenceManipulation();
	}

	void TargetingManager::PrePauseTask()
	{
		ALYSLC::Log("[TM] PrePauseTask: P{}", playerID + 1);
		auto ui = RE::UI::GetSingleton(); 
		if ((nextState == ManagerState::kAwaitingRefresh) || (ui && !ui->GameIsPaused())) 
		{
			// Clear all targets.
			ClearTargetHandles();
			// No longer selecting a crosshair target.
			validCrosshairRefrHit = false;
			// Clear all grabbed and released refrs
			// to stop grabbing refrs and checking for released refr collisions.
			// Maintain management of refrs if the game is paused.
			rmm->ClearAll();

			// Reset crosshair position.
			ResetCrosshairPosition();
		}
	}

	void TargetingManager::PreStartTask()
	{
		ALYSLC::Log("[TM] PreStartTask: P{}", playerID + 1);
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
			// Temporary solution until I figure out what triggers the 'character controller and 3D desync warp glitch',
			// which occurs ~0.5 seconds after unpausing with a player previously grabbed.
			// Ragdolling fixes the issue, but I need to find a way to detect if this desync is happening
			// and correct it in the UpdateGrabbedReferences() call.
			// Solution: If grabbed by another player, release this player before resuming.
			const auto handle = coopActor->GetHandle();
			for (const auto& otherP : glob.coopPlayers)
			{
				if (otherP->isActive && otherP != p)
				{
					if (otherP->tm->rmm->IsManaged(handle, true))
					{
						otherP->tm->rmm->ClearRefr(handle);
						break;
					}
				}
			}
		}

		// Clear out game crosshair pick refr.
		Util::SendCrosshairEvent(nullptr);
	}

	void TargetingManager::RefreshData()
	{
		// Player data.
		controllerID = p->controllerID;
		playerID = p->playerID;
		coopActor = p->coopActor;

		// Projectile manager.
		managedProjHandler = std::make_unique<ManagedProjectileHandler>();
		// Grabbed/released object manipulation manager.
		rmm = std::make_unique<RefrManipulationManager>();
		// Motion state.
		targetMotionState = std::make_unique<ActorTargetMotionState>();
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
		crosshairOscillationData = std::make_unique<InterpolationData<float>>(0.0f, 0.0f, 0.0f, Settings::vfSecsToOscillateCrosshair[playerID]);
		crosshairRotationData = std::make_unique<InterpolationData<float>>(0.0f, 0.0f, 0.0f, Settings::vfSecsToRotateCrosshair[playerID]);

		// Target handles.
		// Clear all target handles, not just crosshair selection-related ones.
		ClearTargetHandles();

		// World positions.
		crosshairLastHitLocalPos = crosshairWorldPos = lastActivationReqPos = RE::NiPoint3();
		crosshairInitialHitLocalPos = std::nullopt;
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
		crosshairSpeedMult = 1.0f;
		detectionPct = 100.0f;
		lastActivationFacingAngle = coopActor->GetHeading(false);
		// Reach set to twice the actor's height.
		maxReachActivationDist = coopActor->GetHeight() * 2.0f;
		secsSinceLastStealthStateCheck = secsSinceTargetVisibilityLost = secsSinceVisibleOnScreenCheck = 0.0f;
		// Ints.
		// 100% percent detection corresponds to green.
		detectionPctRGB = 0x00FF00;

		// Lastly, set player's pitch angle to 0, so that their pitch
		// angle since the last session was active does not carry over.
		coopActor->data.angle.x = 0.0f;
		// Reset all target handles, related data, and time points.
		ResetTargeting();
		ResetTPs();
		ALYSLC::Log("[TM] RefreshData: {}.", coopActor ? coopActor->GetName() : "NONE");
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

#pragma endregion

	void TargetingManager::ClearTarget(const TargetActorType& a_targetType)
	{
		// Clear the actor target handle that corresponds to the given target type.
		
		const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
		{
			std::unique_lock<std::mutex> targetingLock(targetingMutex, std::try_to_lock);
			if (targetingLock)
			{
				ALYSLC::Log("[TM] ClearTarget: {}: Lock obtained. (0x{:X})", coopActor->GetName(), hash);

				if (a_targetType == TargetActorType::kAimCorrection)
				{
					if (auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle); aimCorrectionTargetPtr) 
					{
						aimCorrectionTargetHandle.reset();
					}
				}
				else if (a_targetType == TargetActorType::kLinkedRefr)
				{
					if (auto aimTargetLinkedRefrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle); aimTargetLinkedRefrPtr)
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
					if (auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle); selectedTargetActorPtr)
					{
						selectedTargetActorHandle.reset();
					}
				}
			}
			else
			{
				ALYSLC::Log("[TM] ClearTarget: {}: Failed to obtain lock. (0x{:X})", coopActor->GetName(), hash);
			}
		}
	}

	void TargetingManager::DrawAimPitchIndicator()
	{
		// Draw the player's aim pitch adjustment indicator when the player is adjusting their aim pitch 
		// or for a short time after the player's aim pitch resets.
		bool adjustingAimPitch = (p->mm->shouldAdjustAimPitch || p->pam->GetSecondsSinceLastStop(InputAction::kResetAim) < 0.25f);
		aimPitchIndicatorFadeInterpData->UpdateInterpolatedValue(baseCanDrawOverlayElements && adjustingAimPitch);
		if (adjustingAimPitch || aimPitchIndicatorFadeInterpData->interpToMax || aimPitchIndicatorFadeInterpData->interpToMin)
		{
			const float& thickness = Settings::vfCrosshairThickness[playerID];
			RE::NiPoint3 eyePos = Util::GetEyePosition(coopActor.get());
			// Draw two other arrows to the right of the main arrow relative to the camera's facing direction,
			// offset at half of the crosshair's thickness.
			RE::NiPoint3 niCamRightOffset = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(Util::NormalizeAng0To2Pi(glob.cam->GetCurrentYaw() + PI / 2.0f)));
			const glm::vec3 camRight = ToVec3(niCamRightOffset);
			// Draw from the character's eyes to their aim pitch position.
			const glm::vec3 from = ToVec3(eyePos);
			const glm::vec3 to = ToVec3(p->mm->aimPitchPos);

			// One arrow colored to match each crosshair component (body and two outlines).
			uint8_t alpha = static_cast<uint8_t>(aimPitchIndicatorFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueArrow3D(from, to, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 15.0f, thickness);
			alpha = static_cast<uint8_t>(aimPitchIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueArrow3D(from - camRight * thickness * 0.5f, to - camRight * thickness * 0.5f, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 15.0f, thickness);
			alpha = static_cast<uint8_t>(aimPitchIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueArrow3D(from + camRight * thickness * 0.5f, to + camRight * thickness * 0.5f, (Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 15.0f, thickness);
		}
	}

	void TargetingManager::DrawCrosshair()
	{
		// Draw crosshair lines and outlines based on if the crosshair is targeting a valid reference.

		// Update fade value if inactive crosshair fading is enabled.
		if (Settings::vbFadeInactiveCrosshair[playerID]) 
		{
			float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
			// Active if locked on target, moving, facing a target, or if not in the process of being re-centered while inactive.
			// Allow 1x inactive interval while static + 0.5x an inactive interval while auto-recentering to elapse before fading out.
			bool isCrosshairActive = 
			{
				Util::GetRefrPtrFromHandle(crosshairRefrHandle) || 
				p->pam->IsPerforming(InputAction::kMoveCrosshair) || 
				p->mm->shouldFaceTarget || 
				secsSinceActive <= 1.5f * Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
			};
			crosshairFadeInterpData->UpdateInterpolatedValue(baseCanDrawOverlayElements && isCrosshairActive);
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
		// Draw the main four lines of the crosshair using the player's assigned crosshair color and size params.

		// When facing a target, rotate all lines 45 degrees.
		float angToRotate = p->mm->shouldFaceTarget ? -PI / 4.0f : 0.0f;
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
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y + crosshairGap + crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> crosshairDown = 
		{
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap),
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap - crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> crosshairLeft = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y),
			glm::vec2(crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y)
		};
		std::pair<glm::vec2, glm::vec2> crosshairRight = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y),
			glm::vec2(crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y)
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
			static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)) :
			0xFF
		);
		// Up.
		DebugAPI::QueueLine2D(crosshairUp.first, crosshairUp.second, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, crosshairThickness);
		// Down.
		DebugAPI::QueueLine2D(crosshairDown.first, crosshairDown.second, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, crosshairThickness);
		// Left.
		DebugAPI::QueueLine2D(crosshairLeft.first, crosshairLeft.second, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, crosshairThickness);
		// Right.
		DebugAPI::QueueLine2D(crosshairRight.first, crosshairRight.second, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, crosshairThickness);

		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 50.0f))
		{
			DebugAPI::QueueCircle2D(crosshairScaleformPos, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 64, 2.0f * crosshairThickness + crosshairGap + crosshairLength, 2.0f * crosshairThickness);
			DebugAPI::QueueCircle2D(crosshairScaleformPos, 0xFFFFFF00 + alpha, 64, 4.0f * crosshairThickness + crosshairGap + crosshairLength, 2.0f * crosshairThickness);
		}
	}

	void TargetingManager::DrawCrosshairOutline(float&& a_outlineIndex, const uint32_t& a_outlineRGBA)
	{
		// Always outline the four main crosshair lines and also outline that inner outline with another outline
		// when a valid object is selected by the player's crosshair.
		// Outline index is a whole number value that indicates the multiple of crosshair thicknesses
		// from the center four lines at which to draw the outline.
		// The higher the index, the further from the crosshair base lines the outline will be drawn.

		// Rotate the outlines when facing a target to match the rotation of the crosshair body.
		float angToRotate = p->mm->shouldFaceTarget ? -PI / 4.0f : 0.0f;
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
		const float crosshairGap = Settings::vfCrosshairGapRadius[playerID] - outlineThicknessOffset + gapDelta;

		// Pairs of 2D line start and end coordinates.
		// 
		// Up outlines.
		std::pair<glm::vec2, glm::vec2> up1 = 
		{
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap + crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> up2 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap),
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap + crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> up3 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap + crosshairLength),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap + crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> up4 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y + crosshairGap)
		};

		// Down outlines.
		std::pair<glm::vec2, glm::vec2> down1 = 
		{
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap - crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> down2 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap),
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap - crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> down3 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap - crosshairLength),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap - crosshairLength)
		};
		std::pair<glm::vec2, glm::vec2> down4 = 
		{
			glm::vec2(crosshairScaleformPos.x + outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap),
			glm::vec2(crosshairScaleformPos.x - outlineThicknessOffset, crosshairScaleformPos.y - crosshairGap)
		};

		// Left outlines.
		std::pair<glm::vec2, glm::vec2> left1 = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y - outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y - outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> left2 = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y + outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> left3 = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y - outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> left4 = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y - outlineThicknessOffset)
		};

		// Right outlines.
		std::pair<glm::vec2, glm::vec2> right1 = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y - outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y - outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> right2 = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y + outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> right3 = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y - outlineThicknessOffset)
		};
		std::pair<glm::vec2, glm::vec2> right4 = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y + outlineThicknessOffset),
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y - outlineThicknessOffset)
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
			static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(a_outlineRGBA & 0xFF)) :
			0xFF
		);
		// Up
		DebugAPI::QueueLine2D(up1.first, up1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(up2.first, up2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(up3.first, up3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(up4.first, up4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		// Down.
		DebugAPI::QueueLine2D(down1.first, down1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(down2.first, down2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(down3.first, down3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(down4.first, down4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		// Left.
		DebugAPI::QueueLine2D(left1.first, left1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(left2.first, left2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(left3.first, left3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(left4.first, left4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		// Right.
		DebugAPI::QueueLine2D(right1.first, right1.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(right2.first, right2.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(right3.first, right3.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
		DebugAPI::QueueLine2D(right4.first, right4.second, (a_outlineRGBA & 0xFFFFFF00) + alpha, crosshairThickness);
	}
	
	void TargetingManager::DrawPlayerIndicator()
	{
		// Draw a quest marker above the player's head when player-specific visibility conditions are met.

		const auto& visibilityType = Settings::vuPlayerIndicatorVisibilityType[playerID];
		// If the indicator is disabled, no need to draw. Bye.
		if (visibilityType == !PlayerIndicatorVisibilityType::kDisabled) 
		{
			return;
		}

		// If the player is not on screen, draw the player indicator pointed at the player's position.
		// If on screen, draw player indicator above their head when:
		// - 'Always draw' setting is set -OR-
		// - There is no LOS to them -OR-
		// - Their pixel height is below a certain threshold.

		// Get viewport dimensions for the overlay menu.
		const auto port = Util::GetPort();
		// Check that the player's center is visible.
		RE::NiPoint3 posScreenCoords;
		bool onScreen = Util::PointIsOnScreen(Util::GetTorsoPosition(coopActor.get()), posScreenCoords);
		// Ensure two outlines can fit inside outermost outline.
		// Scale with player's pixel height and bound above and below.
		float indicatorBaseLength = Settings::vfPlayerIndicatorLength[playerID];
		const float& indicatorBaseThickness = Settings::vfPlayerIndicatorThickness[playerID];
		float playerPixelHeight = Util::GetBoundPixelDist(coopActor.get(), true);
		playerPixelHeight = playerPixelHeight == 0.0f ? indicatorBaseLength : playerPixelHeight;
		// Lower/upper bound are the smaller/larget of:
		// 4 indicator thicknesses or player pixel height scaled down by a factor.
		indicatorBaseLength = std::clamp
		(
			indicatorBaseLength,
			min(max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), playerPixelHeight / 4.0f),
			max(max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), playerPixelHeight / 4.0f)
		);
		// Scaling factor used to scale up/down hardcoded point offsets for the indicator's shape.
		float scalingFactor = indicatorBaseLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
		float indicatorLength = indicatorBaseLength * scalingFactor;
		float indicatorThickness = indicatorBaseThickness * scalingFactor;
		if (!onScreen)
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

			// Origin with respect to which all the above offset points are traced out when drawing the shape.
			glm::vec2 origin
			{
				std::clamp(posScreenCoords.x, port.left + indicatorThickness + indicatorLength, port.right - (indicatorThickness + indicatorLength)),
				std::clamp(posScreenCoords.y, port.top + indicatorThickness + indicatorLength, port.bottom - (indicatorThickness + indicatorLength))
			};

			// Rotate and draw lower portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, indicatorRotRads);
			uint8_t alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueShape2D(origin, lowerPortionOffsets, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, indicatorThickness);
			alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueShape2D(origin, lowerPortionOffsets, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha);

			// Rotate and draw upper portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, indicatorRotRads);
			alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueShape2D(origin, upperPortionOffsets, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, indicatorThickness);
			alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF));
			DebugAPI::QueueShape2D(origin, upperPortionOffsets, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha);
		}
		else
		{
			auto playerCam = RE::PlayerCamera::GetSingleton();
			bool shouldDraw = baseCanDrawOverlayElements && playerCam && playerCam->cameraRoot && playerCam->cameraRoot.get();
			if (shouldDraw) 
			{
				bool falseRef = false;
				const auto& camPos = glob.cam->IsRunning() ? glob.cam->camTargetPos : playerCam->cameraRoot->world.translate;
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
					Util::GetBoundPixelDist(coopActor.get(), true) >= DebugAPI::screenResY / 15.0f &&
					Util::PointIsOnScreen(playerTorsoPos) 
				};


				// If checking raycast LOS, and the player is not visible, draw the indicator.
				bool hasLOS = true;
				if (shouldRaycastForLOS)
				{
					// From cam to player torso.
					// If there is LOS, the cast will either hit nothing or the player.
					auto result = Raycast::hkpCastRay({ camPos.x, camPos.y, camPos.z, 0.0f }, { playerTorsoPos.x, playerTorsoPos.y, playerTorsoPos.z, 0.0f }, std::vector<RE::NiAVObject*>({ playerCam->cameraRoot.get() }), RE::COL_LAYER::kLOS);
					auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
					hasLOS = (!result.hitObject || !result.hitObject.get()) || (hitRefrPtr && hitRefrPtr.get() == coopActor.get());
				}

				shouldDraw = !shouldRaycastForLOS || !hasLOS;
			}

			// Update fade value before fading in/out.
			// Will continue fading in/out until done even if the draw condition is not met.
			playerIndicatorFadeInterpData->UpdateInterpolatedValue(shouldDraw);
			if (shouldDraw || playerIndicatorFadeInterpData->interpToMax || playerIndicatorFadeInterpData->interpToMin) 
			{
				// Point facing downward above the player's head.
				posScreenCoords = Util::WorldToScreenPoint3(Util::GetHeadPosition(coopActor.get()) + RE::NiPoint3(0.0f, 0.0f, 0.5f * coopActor->GetHeight()));
				// Origin and lower/upper shape offsets from this origin.
				glm::vec2 origin
				{
					std::clamp(posScreenCoords.x, port.left + indicatorThickness + indicatorLength, port.right - (indicatorThickness + indicatorLength)),
					std::clamp(posScreenCoords.y, port.top + indicatorThickness + indicatorLength, port.bottom - (indicatorThickness + indicatorLength))
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
				uint8_t alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF));
				DebugAPI::QueueShape2D(origin, lowerPortionOffsets, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, indicatorThickness);
				alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF));
				DebugAPI::QueueShape2D(origin, lowerPortionOffsets, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha);
				alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF));
				DebugAPI::QueueShape2D(origin, upperPortionOffsets, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, indicatorThickness);
				alpha = static_cast<uint8_t>(playerIndicatorFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF));
				DebugAPI::QueueShape2D(origin, upperPortionOffsets, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha);
			}
		}
	}

	void TargetingManager::DrawSkyrimStyleCrosshair()
	{
		// Draw a Skyrim-style crosshair with a player-specific colorway.

		// Rotate 45 degrees when facing a target.
		float angToRotate = p->mm->shouldFaceTarget ? -PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate rotation, contraction, and expansion, if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}

		// Center at the crosshair positino.
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
					static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF)) :
					0xFF 
				);
				DebugAPI::QueueShape2D(origin, prongRotatedOffsets, (Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, 2.0f * crosshairThickness);
			}
		}

		// [Inner Outline]
		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += crosshairGap;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
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
				static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)) :
				0xFF
			);
			DebugAPI::QueueShape2D(origin, prongRotatedOffsets, (Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, false, crosshairThickness);
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
				static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)) :
				0xFF
			);
			DebugAPI::QueueShape2D(origin, prongRotatedOffsets, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha);
		}


		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 50.0f))
		{
			alpha = Settings::vbFadeInactiveCrosshair[playerID] ? 
					static_cast<uint8_t>(crosshairFadeInterpData->value * static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)) :
					0xFF;
			DebugAPI::QueueCircle2D(crosshairScaleformPos, (Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 64, 2.0f * crosshairThickness + crosshairGap + crosshairLength, 2.0f * crosshairThickness);
			DebugAPI::QueueCircle2D(crosshairScaleformPos, 0xFFFFFF00 + alpha, 64, 4.0f * crosshairThickness + crosshairGap + crosshairLength, 2.0f * crosshairThickness);
		}
	}

	void TargetingManager::DrawTargetingOverlay()
	{
		// Draw all targeting overlay elements if there are no fullscreen menus open,
		// no temporary menus open, or this player is not controlling menus.

		baseCanDrawOverlayElements = false;
		if (auto ui = RE::UI::GetSingleton(); ui) 
		{
			bool fullscreenMenuOpen = ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || ui->IsMenuOpen(RE::StatsMenu::MENU_NAME);
			bool onlyAlwaysUnpaused = Util::MenusOnlyAlwaysUnpaused();
			bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(controllerID);
			baseCanDrawOverlayElements = !ui->GameIsPaused() && !fullscreenMenuOpen && (onlyAlwaysUnpaused || anotherPlayerControllingMenus);
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

	RE::ActorHandle TargetingManager::GetClosestTargetableActorInFOV(RE::ObjectRefHandle a_sourceRefrHandle, const float& a_fovRads, const bool& a_useXYDistance, const float& a_range, const bool& a_combatDependentSelection)
	{
		// Get the closest targetable actor to the source refr using the given FOV in radians centered at their heading angle,
		// and the given maximum range (XY or XYZ distance) to consider targets.
		// If combat dependent selection is requested, only target hostile actors unless attempting to heal a target.

		auto processLists = RE::ProcessLists::GetSingleton();
		if (!processLists || !coopActor)
		{
			return RE::ActorHandle();
		}

		// The closest valid actor within the given FOV window.
		RE::Actor* closestActorInFOV = nullptr;
		// Should only target allies if casting a non-hostile spell.
		// Don't want to go around healing enemies, do we?
		bool shouldOnlyTargetAllies = false;
		// Does the attack source chosen based on the player's current combat action contain a spell.
		bool sourceHasSpell = false;
		// Player is in combat.
		bool inCombat = coopActor->IsInCombat() || glob.player1Actor->IsInCombat();
		// Choosing to select based on combat and attack state.
		if (a_combatDependentSelection) 
		{
			RE::TESForm* attackSource = nullptr;
			// Ranged options in right and left hand + quick cast.
			if ((p->pam->AllInputsPressedForAction(InputAction::kCastRH) && p->em->HasRHSpellEquipped()) ||
				(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) && p->em->HasRHStaffEquipped()) ||
				(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) && p->em->Has2HRangedWeapEquipped()))
			{
				attackSource = p->em->equippedForms[!EquipIndex::kRightHand];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kCastLH) && p->em->HasLHSpellEquipped()) ||
					 (p->pam->AllInputsPressedForAction(InputAction::kAttackLH) && p->em->HasLHStaffEquipped()))
			{
				attackSource = p->em->equippedForms[!EquipIndex::kLeftHand];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kQuickSlotCast) && p->em->quickSlotSpell) ||
						p->pam->reqSpecialAction == SpecialActionType::kQuickCast)
			{
				attackSource = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
			}

			if (p->pam->reqSpecialAction == SpecialActionType::kDualCast)
			{
				// Must be casting healing spells in both hands.
				// Don't want a situation where the player is firing a destruction spell and a healing spell at a friendly target.
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

			// If not in combat and either not casting or casting a hostile spell, do not pick a close actor target.
			if (!inCombat && (!sourceHasSpell || !shouldOnlyTargetAllies)) 
			{
				return RE::ActorHandle();
			}
		}

		// Check if the close refr position is within range of the player/source refr position
		// and if the angle the player must turn to face the target is within the defined FOV window.
		auto isNewClosestActorInFOV =
		[](const RE::NiPoint3& a_coopPlayerPos, const RE::NiPoint3& a_closeRefrPos, float& a_minWeight, const float& a_targetingAngle, const float& a_fovRads, const bool& a_useXYDistance, const float& a_range, const RE::ObjectRefHandle a_sourceRefrHandle) 
		{
			// Within FOV.
			const float turnToFaceActorAngMag = fabsf(Util::NormalizeAngToPi(Util::GetYawBetweenPositions(a_coopPlayerPos, a_closeRefrPos) - a_targetingAngle));
			const bool inFOV = turnToFaceActorAngMag <= (a_fovRads / 2.0f);
			// Don't need to check range if not in FOV.
			if (inFOV)
			{
				// Disregard range when set to -1.
				bool useRange = a_range != -1.0f;

				// Get distance between source and close refr position.
				float distanceFromSource = FLT_MAX;
				if (auto sourceRefrPtr = Util::GetRefrPtrFromHandle(a_sourceRefrHandle); sourceRefrPtr)
				{
					if (a_useXYDistance)
					{
						distanceFromSource = Util::GetXYDistance(a_closeRefrPos, sourceRefrPtr->data.location);
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

				const float distAngWeight = distanceFromSource * turnToFaceActorAngMag;
				if (distAngWeight < a_minWeight)
				{
					// Change min weight, since this actor's is smaller.
					a_minWeight = distAngWeight;
					// Return true since this actor is closer (distance and angle-wise).
					return true;
				}
			}

			return false;
		};

		// Use the LS's game angle as the targeting angle if the moving the LS;
		// otherwise, use the player actor's heading angle.
		auto targetingAngle = p->mm->lsMoved ? p->mm->movementOffsetParams[!MoveParams::kLSGameAng] : coopActor->GetHeading(false);
		// Lowest distance-angle weight. Starts at max possible value.
		float minAngDistWeight = FLT_MAX;
		// From the player's center.
		const auto playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		// Check all high actors.
		for (const auto& closeActorHandle : processLists->highActorHandles)
		{
			if (auto actorPtr = Util::GetActorPtrFromHandle(closeActorHandle); actorPtr && Util::IsValidRefrForTargeting(actorPtr.get()) && !actorPtr->IsDead())
			{
				// Blacklist set, current mount, and non-players in the co-op entity blacklist.
				const bool blacklisted = 
				{
					(actorPtr == coopActor) ||
					(actorPtr == p->GetCurrentMount()) ||
					(
						!GlobalCoopData::IsCoopPlayer(actorPtr.get()) && 
						glob.coopEntityBlacklistFIDSet.contains(actorPtr->formID)
					)
				};
				// Is hostile to any active player.
				const bool isHostileToAPlayer = std::any_of
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(), 
					[&actorPtr](const auto& a_p) 
					{
						return a_p->isActive && actorPtr->IsHostileToActor(a_p->coopActor.get());
					}
				);

				// If choosing to filter out more than just the blacklisted actors (a_combatDependentSelection == true):
				// Targetable actors:
				// 1. Not blacklisted -AND-
				// 2. When only targeting allies (when casting healing spells), must be a normally friendly actor that is not hostile -OR-
				// 3. When not targeting allies, if in combat, must be a hostile actor; otherwise any actor is fair game.
				if ((blacklisted) || 
					((a_combatDependentSelection) && 
					(((shouldOnlyTargetAllies) && (!Util::IsPartyFriendlyActor(actorPtr.get()) || isHostileToAPlayer)) || 
					((!shouldOnlyTargetAllies) && inCombat && !isHostileToAPlayer))))
				{
					continue;
				}

				// Run close actor check to update the new closest actor in FOV.
				if (isNewClosestActorInFOV(playerCenterPos, Util::Get3DCenterPos(actorPtr.get()), minAngDistWeight, targetingAngle, a_fovRads, a_useXYDistance, a_range, a_sourceRefrHandle)) 
				{
					closestActorInFOV = actorPtr.get();
				}
			}
		}

		// Also add P1 if the companion player is performing this check.
		if (!p->isPlayer1) 
		{
			// No combat-dependent filter or if trying to heal P1.
			if (!a_combatDependentSelection || shouldOnlyTargetAllies)
			{
				// Perform new closest actor in FOV check on P1.
				if (isNewClosestActorInFOV(playerCenterPos, Util::Get3DCenterPos(glob.player1Actor.get()), minAngDistWeight, targetingAngle, a_fovRads, a_useXYDistance, a_range, a_sourceRefrHandle)) 
				{
					closestActorInFOV = glob.player1Actor.get();
				}
			}
		}

		// Return handle.
		return closestActorInFOV ? closestActorInFOV->GetHandle() : RE::ActorHandle();
	}

	uint32_t TargetingManager::GetDetectionLvlRGB(const float& a_detectionLvl, bool&& a_fromRawLevel)
	{
		// Get gradient RGB value corresponding to the given detection level.
		// Raw detection level ranges from -1000 to 1000, 
		// but the range [-20, 0] holds the most relevant detection levels with a noticeable change in awareness,
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
		// Get gradient RGB value corresponding to the level difference between the player and the given actor.
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

	const std::vector<RE::ObjectRefHandle>& TargetingManager::GetNearbyRefrsOfSameType(RE::ObjectRefHandle a_refrHandle, RefrCompType&& a_compType)
	{
		// Get a list of nearby refrs that are the of the same base form type
		// or share the same base form with the given refr.
		
		// Clear old nearby objects of the same type before refreshing.
		nearbyObjectsOfSameType.clear();
		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		if (!refrPtr)
		{
			return nearbyObjectsOfSameType;
		}

		const auto playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		auto refrBaseObject = refrPtr->GetBaseObject();
		// Player wants to steal objects when sneaking.
		bool canSteal = coopActor->IsSneaking();
		float maxCheckDist = (coopActor->IsOnMount()) ? Settings::fMountedActivationReachMult * maxReachActivationDist : maxReachActivationDist;
		// Check each refr in range.
		Util::ForEachReferenceInRange
		(
			playerCenterPos, maxCheckDist, true,
			[&](RE::TESObjectREFR* a_refr) 
			{
				// Ensure that the object reference is an interactable object, not a crime to activate,
				// and of the same form type as requested. 

				// Refrs without a loaded name are usually statics or other uninteractable objects.
				if (!a_refr || !Util::HandleIsValid(a_refr->GetHandle()) || 
					!a_refr->IsHandleValid() || !a_refr->Is3DLoaded() || 
					!a_refr->GetCurrent3D() || a_refr->IsDeleted() ||
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
						sameType = baseObj && refrBaseObject && *baseObj->formType == *refrBaseObject->formType;
					}
					else
					{
						// Won't happen, but same refr is also an option.
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
			// Update last activation orientation after setting proximity refr.
			SetLastActivationCyclingOrientation();
		}
		else
		{
			activationRefrHandle = crosshairRefrHandle;
		}
		
		return activationRefrHandle;
	}

	RE::ObjectRefHandle TargetingManager::GetRangedPackageTargetRefr(RE::TESForm* a_rangedAttackSource)
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

		bool hasSelectedTarget = 
		{
			Util::HandleIsValid(selectedTargetActorHandle) && 
			Util::IsValidRefrForTargeting(selectedTargetActorHandle.get().get()) 
		};
		auto spell = a_rangedAttackSource->As<RE::SpellItem>();

		// Rules:
		// - Target self if for self-targeting spells.
		// - If a spell targets an actor, choose the crosshair-selected actor, 
		// aim correction actor, or the closest actor in the player's FOV, 
		// and choose the FAI target or closest actor to the FAI if in free aim mode.
		// - For touch range spells, choose the closest actor in touch range in front of the player.
		// - For ranged weapon attacks, spells targeting a location, or aimed spells, aim at the FAI.
		if (spell && spell->GetDelivery() == Delivery::kSelf)
		{
			targetHandle = coopActor->GetHandle();
		}
		else if (spell && spell->GetDelivery() == Delivery::kTouch)
		{
			// If in range, choose the already-selected or aim correction target actor.
			bool cachedTargetChosen = false;
			float distToTarget = FLT_MAX;
			if (hasSelectedTarget) 
			{
				distToTarget = Util::GetTorsoPosition(coopActor.get()).GetDistance(Util::GetTorsoPosition(selectedTargetActorHandle.get().get()));
				if (distToTarget <= maxReachActivationDist)
				{
					targetHandle = selectedTargetActorHandle;
					cachedTargetChosen = true;
				}
			}
			else if (aimCorrectionTargetHandle)
			{
				distToTarget = Util::GetTorsoPosition(coopActor.get()).GetDistance(Util::GetTorsoPosition(aimCorrectionTargetHandle.get().get()));
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
				targetHandle = GetClosestTargetableActorInFOV(RE::ObjectRefHandle(), PI, true, maxReachActivationDist, true);
			}
		}
		else if (spell && spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetActor)
		{
			// If a cached target actor is already available, choose it.
			if (hasSelectedTarget) 
			{
				targetHandle = selectedTargetActorHandle;
			}
			else if (aimCorrectionTargetHandle)
			{
				targetHandle = aimCorrectionTargetHandle;
			}
			else
			{
				// Otherwise, get the closest, non-blacklisted actor 
				// in a 180 degree arc in front of the player.
				// Use XY distance to ignore vertical displacements.
				targetHandle = GetClosestTargetableActorInFOV(RE::ActorHandle(), PI, true, -1.0f, true);
			}
		}
		else if (hasSelectedTarget)
		{
			targetHandle = selectedTargetActorHandle;
		}
		else if (aimCorrectionTargetHandle)
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

		if (selectedTargetActorHandle)
		{
			return selectedTargetActorHandle;
		}
		else if (Settings::vbUseAimCorrection[playerID] && !p->mm->shouldFaceTarget && Util::HandleIsValid(aimCorrectionTargetHandle))
		{
			return aimCorrectionTargetHandle;
		}
		else if (const auto refrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle); refrPtr && refrPtr->As<RE::Actor>() && refrPtr != coopActor)
		{
			// If there is no selected target and no aim correction target,
			// but the player is running their ranged attack package, 
			// choose the aim target linked refr, if available.
			// Ignore the player if they are the linked refr, 
			// since this is the fall-through case that simply enables the ranged attack package to execute.
			return refrPtr->As<RE::Actor>()->GetHandle();
		}

		return RE::ActorHandle();
	}

	void TargetingManager::HandleBonk(RE::ActorHandle a_hitActorHandle, RE::ObjectRefHandle a_releasedRefrHandle, RE::hkpRigidBody* a_releasedRefrRigidBody, const RE::NiPoint3& a_contactPos)
	{
		// Apply damage to the given hit actor based on the physical properties of the given released refr.

		auto hitActorPtr = Util::GetActorPtrFromHandle(a_hitActorHandle);
		auto releasedRefrPtr = Util::GetRefrPtrFromHandle(a_releasedRefrHandle);
		// The hit actor, released refr, and released refr's rigid body all have to be valid.
		if (!a_releasedRefrRigidBody || !hitActorPtr || !releasedRefrPtr || !hitActorPtr->currentProcess)
		{
			return;
		}

		// Set minimum mass to 1.
		float mass = a_releasedRefrRigidBody->motion.GetMass();
		if (mass == 0.0f)
		{
			mass = 1.0f;
		}

		// 3x damage at player level 100.
		float levelDamageFactor = 1.0f + 3.0f * max(coopActor->GetLevel() - 1.0f, 0.0f) / 99.0f;
		// Get impact speed from rigidbody, with refr linear speed as a fallback.
		float havokImpactSpeed = a_releasedRefrRigidBody->motion.linearVelocity.Length3();
		if (havokImpactSpeed == 0.0f) 
		{
			RE::NiPoint3 linVel;
			releasedRefrPtr->GetLinearVelocity(linVel);
			havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
		}

		// Get sneak attack mult, if any.
		float detectionPct = (std::clamp(static_cast<float>(hitActorPtr->RequestDetectionLevel(coopActor.get())), -20.0f, 0.0f) + 20.0f) * 5.0f;
		float sneakMult = coopActor->IsSneaking() && detectionPct < 100.0f ? max(2.0f, coopActor->GetActorValue(RE::ActorValue::kAttackDamageMult)) : 1.0f;
		float weightFactor = max(0.1f, releasedRefrPtr->GetWeight());
		// Scale up actor-actor collision damage based on the released actor's base, equipped, and potentially inventory weight.
		if (auto asActor = releasedRefrPtr->As<RE::Actor>(); asActor) 
		{
			float actorWeight = asActor->GetWeight();
			float equippedWeight = asActor->GetEquippedWeight();
			const auto& inventory = asActor->GetInventory();
			float inventoryWeight = 0.0f;
			for (auto& [boundObj, exDataCount] : inventory) 
			{
				if (boundObj && exDataCount.first > 0) 
				{
					inventoryWeight += boundObj->GetWeight();
				}
			}

			// Optionally also apply their inventory weight.
			weightFactor = (actorWeight + 50.0f) * powf((1.0f + ((equippedWeight) / 100.0f)), 2.0f);
		}

		// Subject to change, but this factor works for now.
		float speedWeightFactor = sqrtf(weightFactor * havokImpactSpeed);

		// REMOVE when done debugging.
		/*ALYSLC::Log("[TM] {}: HandleBonk: Hit actor {}. Thrown object {}'s mass: {}, weight: {}, impact speed: {}, speedweight factor: {}. Skill damage factor: {} (player level: {}). Sneak mult: {}. Final Damage: {}.", 
			coopActor->GetName(), 
			hitActorPtr->GetName(),
			releasedRefrPtr->GetName(),
			mass,
			releasedRefrPtr->GetWeight(),
			havokImpactSpeed,
			speedWeightFactor,
			levelDamageFactor, 
			coopActor->GetLevel(),
			sneakMult,
			speedWeightFactor * sneakMult * skillDamageFactor);*/

		// Ragdoll the hit actor with a force dependent on the colliding body's impact speed.
		if (auto hitActorRigidBody = Util::GethkpRigidBody(hitActorPtr.get()); hitActorRigidBody)
		{
			Util::PushActorAway(hitActorPtr.get(), a_contactPos, havokImpactSpeed);
		}

		// Set power attack and potentially the sneak attack flag before sending a hit event.
		RE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> hitFlags = RE::TESHitEvent::Flag::kPowerAttack;
		if (sneakMult > 1.0f)
		{
			hitFlags.set(RE::TESHitEvent::Flag::kSneakAttack);
		}

		// Criteria for damageable actors:
		// Not a ghost or invulnerable and either a player or not essential/protected or hostile to the player.
		bool damageable = 
		{
			(!hitActorPtr->IsGhost() && !hitActorPtr->IsInvulnerable())
		};
		if (damageable) 
		{
			float damage = speedWeightFactor * sneakMult * levelDamageFactor;
			// Will send assault alarm in hit event, unless in god mode.
			// Power attack flag to add compatibility with Maximum Carnage,
			// which will trigger gore effects on powerattack kill.
			Util::SendHitEvent(coopActor.get(), hitActorPtr.get(), coopActor->formID, releasedRefrPtr->formID, hitFlags);

			// Handle health damage.
			// Ignore damage to friendly actors if friendly fire is off.
			if ((damage != 0.0f) && (Settings::vbFriendlyFire[playerID] || !Util::IsPartyFriendlyActor(hitActorPtr.get())))
			{
				// Damage will not be modified in either HandleHealthDamage() hook because the damage will not be attributed to the player
				// (attacker param is nullptr) since we are directly modifying the health AV here.
				// Therefore, to get the same damage modifications here, we tack on the thrown object damage mult, or
				// flop damage mult if the released refr is the player themselves,
				// and multiply the result by the damage received mult if the target is a player.
				if (releasedRefrPtr == coopActor)
				{
					damage *= Settings::vfFlopDamageMult[playerID];
				}
				else
				{
					damage *= Settings::vfThrownObjectDamageMult[playerID];
				}

				if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(hitActorPtr.get()); playerIndex != -1)
				{
					const auto& hitP = glob.coopPlayers[playerIndex];
					damage *= Settings::vfDamageReceivedMult[hitP->controllerID];
				}

				// Apply non-zero damage.
				if (damage != 0.0f)
				{
					hitActorPtr->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -damage);
				}
			}
		}
	}

	void TargetingManager::HandleQuickLootMenu()
	{
		// Open the LootMenu when the player moves their crosshair over a lootable container,
		// or close the LootMenu if the player moves their crosshair off the container 
		// or the container becomes invalid.

		// Only run if QuickLoot is loaded, the crosshair pick data singleton is available,
		// no temporary menus are open, and the player is not transformed or transforming.
		if (!ALYSLC::QuickLootCompat::g_quickLootInstalled)
		{
			return;
		}

		auto crosshairPickData = RE::CrosshairPickData::GetSingleton();
		if (!crosshairPickData)
		{
			return;
		}

		if (!Util::MenusOnlyAlwaysUnpaused())
		{
			return;
		}

		// LootMenu opens but is not visible sometimes when targeting a container while transformed,
		// so don't attempt to open it until the player reverts their form.
		// Players can still loot by selecting the container with their crosshair and activating it as usual.
		if (p->isTransforming || p->isTransformed)
		{
			return;
		}

		// Check for changes to the player's crosshair-selected refr.
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		auto prevCrosshairRefrPtr = Util::GetRefrPtrFromHandle(prevCrosshairRefrHandle);
		bool crosshairRefrValidity = Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
		// Has the player moved into/out of range of their targeted refr?
		bool wasInRange = crosshairRefrInRangeForQuickLoot;
		crosshairRefrInRangeForQuickLoot = crosshairRefrPtr ? RefrIsInActivationRange(crosshairRefrHandle) : false;
		// Before sending a crosshair event to change the state of the QuickLoot menu,
		// ensure no other menus are opening back up in quick succession,
		// which will cause flickering due to many requests triggering in at once.
		// Truly an icky solution below.
		float secsSinceAllSupportedMenusClosed = Util::GetElapsedSeconds(glob.lastSupportedMenusClosedTP);
		bool newCrosshairRefr = prevCrosshairRefrPtr != crosshairRefrPtr;
		// Check if this player sent the last successful QuickLoot menu opening request.
		bool sentLastCrosshairRefrEvent = glob.quickLootReqCID == controllerID;
		// If no temporary menus are open and it has been an eighth of a second, any player can request to open the QuickLoot menu.
		bool anyPlayerCanSet = !glob.supportedMenuOpen.load() && secsSinceAllSupportedMenusClosed > 0.125f;
		// Is this player controlling menus?
		bool controllingMenus = glob.supportedMenuOpen.load() && GlobalCoopData::IsControllingMenus(controllerID);
		// Send a new crosshair event to open the QuickLoot menu if the player's crosshair refr is valid,
		// any player can open the menu, and the refr is now in range + 
		// was just selected, not previously in range, or the player did not send the last opening request.
		bool shouldSendNewSetCrosshairEvent = 
		{
			(crosshairRefrValidity && anyPlayerCanSet) &&
			(crosshairRefrInRangeForQuickLoot && (newCrosshairRefr || !wasInRange || !sentLastCrosshairRefrEvent))
		};
		// Validate the crosshair event if the crosshair refr is valid, the player is controlling menus,
		// and the player just selected a new refr that is in range.
		bool shouldValidateNewCrosshairEvent = 
		{
			crosshairRefrValidity && controllingMenus && newCrosshairRefr && crosshairRefrInRangeForQuickLoot
		};
		// Close the QuickLoot menu if the player is controlling the menu 
		// and the crosshair refr is no longer valid, or is no longer in range.
		bool shouldSendClearCrosshairEvent = {
			(controllingMenus) && 
			((!crosshairRefrPtr && prevCrosshairRefrPtr) || (wasInRange && !crosshairRefrInRangeForQuickLoot))
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
				if (!hasLoot)
				{
					auto inventory = crosshairRefrPtr->GetInventory(Util::IsLootableObject);
					for (const auto& [boundObj, invEntryData] : inventory)
					{
						if (boundObj && invEntryData.second && invEntryData.first > 0)
						{
							hasLoot = true;
							break;
						}
					}
				}

				// Then check dropped inventory.
				if (!hasLoot)
				{
					auto droppedInventory = crosshairRefrPtr->GetDroppedInventory(Util::IsLootableObject);
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
				crosshairRefrValidity &&
				hasLoot &&
				crosshairRefrInRangeForQuickLoot &&
				(!crosshairRefrPtr->As<RE::Actor>() || crosshairRefrPtr->IsDead()) &&
				!crosshairRefrPtr->IsLocked() &&
				!crosshairRefrPtr->IsActivationBlocked() &&
				!coopActor->IsInCombat()
			};

			bool firstTimeLootingKilledActor = false;
			// If looting a corpse, give the killer player first rights.
			if (canOpenLootMenu)
			{
				// Is corpse.
				if (auto corpse = crosshairRefrPtr->As<RE::Actor>(); corpse)
				{
					const auto targetExData = corpse->extraList.GetByType<RE::ExtraForcedTarget>();
					bool killedByAPlayer = GlobalCoopData::IsCoopPlayer(corpse->myKiller);
					firstTimeLootingKilledActor = 
					{
						killedByAPlayer && targetExData && targetExData->pad14 == coopActor->formID
					};

					// Is looted if there is no exData or once the exData's pad is cleared.
					bool corpseLootedByKiller = (killedByAPlayer) && (!targetExData || targetExData->pad14 == 0);
					// Can loot now if this player is looting the actor they killed for the first time,
					// or if the actor was not killed by a player or already by the killer player.
					canOpenLootMenu &= (firstTimeLootingKilledActor) || (!killedByAPlayer || corpseLootedByKiller);
				}
			}

			// Can now open the QuickLoot menu if the final LOS check passes.
			// LOS check last, since it is the most expensive.
			if (canOpenLootMenu)
			{
				bool passesLOSCheck = (Settings::bCamCollisions || (crosshairRefrPtr && Util::HasLOS(crosshairRefrPtr.get(), coopActor.get(), false, true, crosshairWorldPos)));
				if (passesLOSCheck)
				{
					// Save container handle for menu CID resolution later.
					glob.reqQuickLootContainerHandle = crosshairRefrHandle;
					glob.moarm->InsertRequest(controllerID, InputAction::kMoveCrosshair, SteadyClock::now(), GlobalCoopData::LOOT_MENU, crosshairRefrPtr->GetHandle());
					// Send SKSE crosshair event to allow QuickLoot menu to trigger.
					// Clear out first if sending a new crosshair event.
					if (shouldSendNewSetCrosshairEvent)
					{
						Util::SendCrosshairEvent(nullptr);
					}

					Util::SendCrosshairEvent(crosshairRefrPtr.get());

					// After sending crosshair event to open the LootMenu for a corpse,
					// clear out the exData pad so other players can freely loot the corpse.
					if (firstTimeLootingKilledActor)
					{
						if (auto selectedTargetActor = Util::GetActorPtrFromHandle(selectedTargetActorHandle); selectedTargetActor)
						{
							selectedTargetActor->extraList.GetByType<RE::ExtraForcedTarget>()->pad14 = 0;
						}
					}
				}
			}
			else if (shouldValidateNewCrosshairEvent)
			{
				// Clear crosshair pick refr if the player's crosshair refr is not lootable.
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
			if (otherP->isActive && otherP != p)
			{
				// Two different players are grabbing each other.
				if (rmm->IsManaged(otherP->coopActor->GetHandle(), true) && otherP->tm->rmm->IsManaged(coopActor->GetHandle(), true))
				{
					isMARFing = true;
					break;
				}
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
					// Reset paralysis flag and max out ragdoll timer on actors to prevent the game from instantly 
					// signalling them to get up after being released.
					// Set even if actor getup removal setting is enabled since we don't want NPCs getting up in midair.
					if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr) 
					{
						if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
						{
							asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
							if (asActor->currentProcess && asActor->currentProcess->middleHigh)
							{
								asActor->currentProcess->middleHigh->unk2B0 = FLT_MAX;
							}
						}
					}
				}

				// Clear all grabbed refrs on release.
				rmm->ClearGrabbedRefrs();
			}
			else
			{
				// Update grabbed refrs' positioning via their velocity.
				// If grabbing multiple objects, all other objects are suspended in a ring around the first one.
				// Must get the first object's radius for spacing purposes.
				float firstGrabbedRefrRadius = 0.0f;
				auto firstRefrHandle = rmm->grabbedRefrInfoList[0]->refrHandle;
				if (auto firstRefrPtr = Util::GetRefrPtrFromHandle(firstRefrHandle); firstRefrPtr)
				{
					if (auto firstRefr3D = Util::GetRefr3D(firstRefrPtr.get()); firstRefr3D)
					{
						firstGrabbedRefrRadius = firstRefr3D->worldBound.radius;
					}
				}

				for (uint8_t i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
				{
					auto& grabbedRefrInfo = rmm->grabbedRefrInfoList[i];
					const auto& handle = grabbedRefrInfo->refrHandle;

					// Paralyze living actor to prevent the game from automatically 
					// signalling the actor to get up once the ragdoll timer hits 0.
					// Only if actor getup removal setting is enabled.
					if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr && Settings::bRemoveGrabbedActorAutoGetUp)
					{
						if (auto asActor = refrPtr->As<RE::Actor>(); asActor && 
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
					if (!rmm->CanManipulateGrabbedRefr(p, i))
					{
						continue;
					}
					else
					{
						grabbedRefrInfo->UpdateGrabbedReference(p, i, firstGrabbedRefrRadius);
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
			for (uint8_t i = 0; i < rmm->releasedRefrInfoList.size(); ++i)
			{
				auto& releasedRefrInfo = rmm->releasedRefrInfoList[i];
				const auto& handle = releasedRefrInfo->refrHandle;
				// Must have been released.
				if (releasedRefrInfo->releaseTP.has_value())
				{
					auto releasedRefrPtr = Util::GetRefrPtrFromHandle(releasedRefrInfo->refrHandle);
					auto targetActorPtr = Util::GetRefrPtrFromHandle(releasedRefrInfo->targetActorHandle);
					float secsSinceRelease = Util::GetElapsedSeconds(releasedRefrInfo->releaseTP.value());
					// Seconds since the released refr first hit another refr.
					float secsSinceFirstHit = 0.0f;
					if (releasedRefrInfo->firstHitTP.has_value()) 
					{
						secsSinceFirstHit = Util::GetElapsedSeconds(releasedRefrInfo->firstHitTP.value());
					}

					// Do not handle released refr any longer if:
					// 1. The released refr is an actor that has gotten up from ragdolling.
					// 2. The released refr hit the player's target actor.
					// 3. The handling duration has expired.
					// 4. The duration after first hit has expired.
					bool shouldClearReleasedRefr = 
					{
						releasedRefrPtr &&
						((releasedRefrPtr->Is(RE::FormType::ActorCharacter) && releasedRefrPtr->As<RE::Actor>()->actorState1.knockState == RE::KNOCK_STATE_ENUM::kNormal) ||
						(targetActorPtr && releasedRefrInfo->hitRefrFIDs.contains(targetActorPtr->formID)) ||
						(secsSinceRelease > Settings::fMaxSecsBeforeClearingReleasedRefr) ||
						(secsSinceFirstHit > Settings::fSecsAfterCollisionBeforeClearingReleasedRefr))
					};

					if (shouldClearReleasedRefr)
					{
						rmm->ClearRefr(handle);
					}
					else if (releasedRefrPtr)
					{
						// Add a new contact listener for the current cell, 
						// if there is no active one currently.
						if (auto cell = releasedRefrPtr->parentCell; cell)
						{
							if (auto bhkWorld = cell->GetbhkWorld(); bhkWorld)
							{
								if (auto ahkpWorld = bhkWorld->GetWorld2(); ahkpWorld && glob.contactListener->world != ahkpWorld)
								{
									glob.contactListener->world = ahkpWorld;
									glob.contactListener->AddContactListener(ahkpWorld);
								}
							}
						}

						// Raycast collision check.
						auto releasedRefr3D = Util::GetRefr3D(releasedRefrPtr.get());
						auto releasedRefrRigidBody = Util::GethkpRigidBody(releasedRefrPtr.get()); 
						// Must have both valid 3D and rigid body.
						if (!releasedRefr3D || !releasedRefrRigidBody || !releasedRefrRigidBody.get())
						{
							continue;
						}

						bool hit = false;
						RE::ObjectRefHandle hitRefrHandle{};
						RE::NiPoint3 hitPos{};
						RE::NiPoint3 velDir{};
						glm::vec4 start{};
						glm::vec4 end{};
						glm::vec4 velOffset{};
						bool isActor = releasedRefrPtr->Is(RE::FormType::ActorCharacter);
						// Actor collisions -- multiple raycasts per actor.
						if (isActor && !Settings::bSimpleActorCollisionRaycast) 
						{
							// Again, must have valid loaded 3D.
							if (!releasedRefrPtr->loadedData || !releasedRefrPtr->loadedData->data3D)
							{
								continue;
							}

							auto loadedData = releasedRefrPtr->loadedData; 
							auto data3D = loadedData->data3D;
							// Raycast once per major NPC skeleton node.
							// Only need a single raycast to hit before breaking.
							for (const auto& nodeName : GlobalCoopData::RAGDOLL_COLLISION_NPC_NODES)
							{
								if (auto node = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); node && node.get())
								{
									if (auto hkpRigidBody = Util::GethkpRigidBody(node.get()); hkpRigidBody)
									{
										const RE::hkpShape* hkpShape = hkpRigidBody->collidable.shape;
										velDir = ToNiPoint3(hkpRigidBody->motion.linearVelocity, true);
										if (auto convexShape = static_cast<const RE::hkpConvexShape*>(hkpShape); convexShape)
										{
											// Cast from node's world position outward a length equal to the node's radius
											// in the direction of the node's velocity.
											velOffset = ToVec4(velDir * convexShape->radius * HAVOK_TO_GAME);
											start = ToVec4(node->world.translate);
											end = start + velOffset;
											auto result = Raycast::hkpCastRay(start, end, std::vector<RE::NiAVObject*>({ releasedRefr3D.get() }), RE::COL_LAYER::kActorZone);

											// Only need a single hit, so once there is a hit, 
											// we save the hit refr and position and then break.
											if (result.hit)
											{
												hit = true;
												hitRefrHandle = result.hitRefrHandle;
												hitPos = ToNiPoint3(result.hitPos);
												break;
											}
										}
									}
								}
							}
						}
						else
						{
							// A single raycast starting from the released refr's reported location or center
							// and in the direction of the node's velocity.
							// Cast length equals the distance the node travels per frame at the current velocity
							// plus a small increment equal to half the refr's height or equal to its radius.
							velDir = ToNiPoint3(releasedRefrRigidBody->motion.linearVelocity, true);
							float distPerFrame = releasedRefrRigidBody->motion.linearVelocity.Length3() * *g_deltaTimeRealTime * HAVOK_TO_GAME;
							float incThrownRefrRadius = 0.0f;
							if (isActor)
							{
								incThrownRefrRadius = releasedRefrPtr->GetHeight() / 2.0f + distPerFrame;
							}
							else
							{
								incThrownRefrRadius = releasedRefr3D->worldBound.radius + distPerFrame;
							}

							// Actor world bound center, particularly P1's, is much more erratic
							// and sometimes not even within the visible 3D model, 
							// so use the refr data position for actors instead.
							if (isActor)
							{
								start = 
								{
									releasedRefrPtr->data.location.x,
									releasedRefrPtr->data.location.y,
									releasedRefrPtr->data.location.z, 0.0f
								};
							}
							else
							{
								start = 
								{
									releasedRefr3D->worldBound.center.x,
									releasedRefr3D->worldBound.center.y,
									releasedRefr3D->worldBound.center.z, 0.0f
								};
							}

							if (isActor)
							{
								end = 
								{
									releasedRefrPtr->data.location.x + velDir.x * incThrownRefrRadius,
									releasedRefrPtr->data.location.y + velDir.y * incThrownRefrRadius,
									releasedRefrPtr->data.location.z + velDir.z * incThrownRefrRadius, 0.0f
								};
							}
							else
							{
								end = 
								{
									releasedRefr3D->worldBound.center.x + velDir.x * incThrownRefrRadius,
									releasedRefr3D->worldBound.center.y + velDir.y * incThrownRefrRadius,
									releasedRefr3D->worldBound.center.z + velDir.z * incThrownRefrRadius, 0.0f
								};
							}

							auto result = Raycast::hkpCastRay(start, end, std::vector<RE::NiAVObject*>({ releasedRefr3D.get() }), RE::COL_LAYER::kActorZone);
							hit = result.hit;
							hitRefrHandle = result.hitRefrHandle;
							hitPos = ToNiPoint3(result.hitPos);
						}

						if (hit)
						{
							if (auto hitRefrPtr = Util::GetRefrPtrFromHandle(hitRefrHandle); hitRefrPtr)
							{
								auto contactPos = ToNiPoint3(hitPos);
								bool hasAlreadyHitRefr = releasedRefrInfo->HasAlreadyHitRefr(hitRefrPtr.get());
								// Add hit refr to cached hit form IDs set.
								releasedRefrInfo->AddHitRefr(hitRefrPtr.get());
								// Hit a new, valid actor that is not the released refr or the player that released the refr.
								if (auto hitActor = hitRefrPtr->As<RE::Actor>(); 
									hitActor && hitActor->currentProcess && 
									hitRefrPtr != releasedRefrPtr && hitRefrPtr != coopActor && !hasAlreadyHitRefr)
								{
									HandleBonk(hitActor->GetHandle(), releasedRefrPtr->GetHandle(), releasedRefrRigidBody.get(), contactPos);
								}
							}
						}

						// Adjust trajectory to home in on the target if necessary.
						if (Settings::vuProjectileTrajectoryType[playerID] == !ProjectileTrajType::kHoming && releasedRefrInfo->isThrown)
						{
							// If conditions are met, start homing in.
							if (!releasedRefrInfo->startedHomingIn)
							{
								releasedRefrInfo->PerformInitialHomingCheck(p);
							}

							if (releasedRefrInfo->isHoming)
							{
								releasedRefrInfo->SetHomingTrajectory(p);
							}
						}
					}
				}
				else
				{
					// No valid release time point.
					// Cleared on the next iteration once reset.
					rmm->ClearRefr(handle);
				}
			}
		}
		else
		{
			// No managed released refrs, so clear out cached collision refr pairs.
			if (!rmm->collidedRefrFIDPairs.empty())
			{
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set once there are no remaining released refrs to handle.
					rmm->collidedRefrFIDPairs.clear();
				}
			}

			// Also clear any queued contact events, which do not need handling anymore.
			if (!rmm->queuedReleasedRefrContactEvents.empty())
			{
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set once there are no remaining released refrs to handle.
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

	void TargetingManager::HandleSplat(RE::ActorHandle a_thrownActorHandle, const uint32_t& a_hitCount)
	{
		// Apply impact damage to thrown/ragdolled actor.

		auto thrownActorPtr = Util::GetActorPtrFromHandle(a_thrownActorHandle);
		// Invalid thrown actor.
		if (!thrownActorPtr)
		{
			return;
		}

		float damage = 0.0f;
		// Criteria for damageable actors:
		// Not a ghost or invulnerable and either a player or not essential/protected or is hostile to the player.
		bool damageable = 
		{
			(!thrownActorPtr->IsGhost() && !thrownActorPtr->IsInvulnerable())
		};
		if (damageable)
		{
			auto releasedRefrRigidBody = Util::GethkpRigidBody(thrownActorPtr.get()); 
			if (releasedRefrRigidBody) 
			{
				float havokImpactSpeed = releasedRefrRigidBody->motion.linearVelocity.Length3();
				// Get refr linear speed if rigidbody speed is 0.
				if (havokImpactSpeed == 0.0f)
				{
					RE::NiPoint3 linVel;
					thrownActorPtr->GetLinearVelocity(linVel);
					havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
				}

				float actorWeight = thrownActorPtr->GetWeight();
				float equippedWeight = thrownActorPtr->GetEquippedWeight();
				// https://www.desmos.com/calculator/tl3snnbl8e
				// Graphs U and L (upper and lower) indicate base damage:
				// Base damage of upper bound weight (100) and lower bound weight (0),
				// with average impact speed (5).
				// Also graphed ratio of U and L.
				float weightFactor = 0.5f * (actorWeight + 50.0f) * powf((1.0f + (equippedWeight / 100.0f)), 2.0f);
				float speedWeightFactor = sqrtf(weightFactor * havokImpactSpeed);
				// 3x damage at player level 100.
				float levelDamageFactor = 1.0f + 3.0f * max(coopActor->GetLevel() - 1.0f, 0.0f) / 99.0f;
				// Damage tapers off as the inverse square of the hit count.
				damage = speedWeightFactor * levelDamageFactor * 1.0f / ((float)(a_hitCount) * (float)(a_hitCount));

				// REMOVE when done debugging.
				/*ALYSLC::Log("[TM] HandleSplat: {}: Thrown actor: {}. Mass: {}, impact speed: {}, speedweight factor: {}, damage: {}. Hit #{}",
				p->coopActor->GetName(),
				thrownActorPtr->GetName(),
				releasedRefrRigidBody->motion.GetMass(),
				havokImpactSpeed,
				speedWeightFactor,
				damage, a_hitCount);*/

				// Will send assault alarm in hit event, unless in god mode.
				// Power attack flag to add compatibility with Maximum Carnage,
				// which will trigger gore effects on powerattack kill.
				Util::SendHitEvent
				(
					coopActor.get(),
					thrownActorPtr.get(),
					coopActor->formID,
					thrownActorPtr->formID,
					RE::TESHitEvent::Flag::kPowerAttack
				);
			}

			// Handle health damage.
			// Ignore damage to friendly actors if friendly fire is off.
			bool shouldDamage = Settings::vbFriendlyFire[playerID] || thrownActorPtr == coopActor || !Util::IsPartyFriendlyActor(thrownActorPtr.get());
			if (damage != 0.0f && shouldDamage)
			{
				// Damage will not be modified in either HandleHealthDamage() hook because the damage will not be attributed to the player
				// (attacker param is nullptr) since we are directly modifying the health AV here.
				// Therefore, to get the same modifications here, we tack on the thrown object damage mult 
				// and damage received mult if the player is throwing another player or themselves.
				damage *= Settings::vfThrownObjectDamageMult[playerID];
				if (auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(thrownActorPtr.get()); playerIndex != -1)
				{
					const auto& thrownP = glob.coopPlayers[playerIndex];
					damage *= Settings::vfDamageReceivedMult[thrownP->controllerID];
				}

				if (damage != 0.0f)
				{
					thrownActorPtr->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -damage);
				}
			}
		}
	}

	bool TargetingManager::IsRefrValidForCrosshairSelection(RE::ObjectRefHandle a_refrHandle)
	{
		// Is the given refr targetable by the player's crosshair?

		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		// No target refr.
		if (!refrPtr) 
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

		// Check LOS if the chosen result is not the closest once,
		// meaning another refr is blocking at least part of the chosen refr,
		// or if the player is not moving their crosshair,
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
			auto refr3D = Util::GetRefr3D(refrPtr.get());
			// No current 3D -> not on screen or visible.
			if (!refr3D) 
			{
				return false;
			}

			// Check if target is on the screen first.
			// Use three positions on the refr.
			crosshairRefrInSight = Util::PointIsOnScreen(refrPtr->data.location);
			if (!crosshairRefrInSight && refr3D)
			{
				crosshairRefrInSight = 
				(
					Util::PointIsOnScreen(refr3D->worldBound.center) || 
					Util::PointIsOnScreen(refr3D->world.translate)
				);
			}

			// Then, if the target is on screen and an LOS check is warranted, perform LOS check.
			if (crosshairRefrInSight && checkLOS) 
			{
				crosshairRefrInSight = Util::HasLOS(refrPtr.get(), coopActor.get(), true, true, crosshairWorldPos);
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
				secsSinceTargetVisibilityLost = Util::GetElapsedSeconds(p->crosshairRefrVisibilityLostTP);
			}
		}

		// Give the player a grace period to regain sight of their targeted refr,
		// since the player or the target might move relative to one another
		// and the target may come back into view.
		bool invalidateAfterNotVisible = secsSinceTargetVisibilityLost > Settings::fSecsWithoutLOSToInvalidateTarget;
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

	Raycast::RayResult TargetingManager::PickRaycastHitResult(const std::vector<Raycast::RayResult>& a_raycastResults, const bool& a_inCombat, const bool&& a_crosshairActiveForSelection)
	{
		// Choose crosshair raycast result to use for setting the crosshair world position 
		// and for selecting the next refr targeted by the player's crosshair.
		// Filter out players, player teammates, and non-hostile actors when in combat.
		// Also, if choosing a result while the player's crosshair is active for selection,
		// meaning either moving or over a previous valid selection,
		// only choose hits with selectable refrs and ignore activators,
		// which do not have visible surfaces and would result in the crosshair world position 
		// floating in midair if selected.

		Raycast::RayResult chosenResult;
		// Keep track of activator hits.
		bool activatorHit = false;
		// Blacklisted.
		bool excluded = false;
		// Must be in front of the camera to select.
		bool inFrontOfCam = false;
		// The current hit result's hit object is an activator.
		bool isActivator = false;
		// The hit object is faded by the camera.
		bool isFadedByCam = false;

		// No hit to start.
		chosenResult.hit = false;
		chosenResult.hitObject = nullptr;

		// No NiCam means no bounds-in-frustum checks, so return result early.
		auto niCam = Util::GetNiCamera();
		if (!niCam)
		{
			return chosenResult;
		}

		// Save hit index to (un)set the closest hit result flag after the loop.
		uint32_t i = 0;
		for (; i < a_raycastResults.size(); ++i)
		{
			auto& result = a_raycastResults[i];

			// REMOVE after debugging.
			/*
			RE::NiPoint3 hitPoint = ToNiPoint3(result.hitPos);
			ALYSLC::Log("[TM] PickRaycastHitResult: {}: For target selection: {}. Pre-parent recurse result {}: hit: {}, {} (refr name: {}, 0x{:X}, type: {}). Distance to camera: {}, distance to player: {}.",
				coopActor->GetName(),
				a_crosshairActiveForSelection,
				i,
				result.hit,
				result.hitObject ? result.hitObject->name.c_str() : "NONE",
				result.hitObject && result.hitObject->userData ? result.hitObject->userData->GetName() : result.hitObject ? fmt::format("{} NO HIT REFR", result.hitObject->name).c_str() : "NONE",
				result.hitObject && result.hitObject->userData ? result.hitObject->userData->formID : 0xDEAD,
				result.hitObject && result.hitObject->userData && result.hitObject->userData->GetObjectReference() ? RE::FormTypeToString(*result.hitObject->userData->GetObjectReference()->formType) : RE::FormTypeToString(RE::FormType::None),
				hitPoint.GetDistance(niCam->world.translate),
				hitPoint.GetDistance(coopActor->data.location));
			*/

			if (!result.hitObject)
			{
				continue;
			}

			auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
			// If the hit refr is valid and selectable and not the player,
			// set crosshair refr and positional data.
			if (hitRefrPtr)
			{
				// Filter out current mount, non-targetable players, and blacklisted actors.
				bool isCoopPlayer = glob.IsCoopPlayer(hitRefrPtr->formID);
				excluded =
				(
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
					Util::IsInFrontOfCam(result.hitObject->world.translate) ||
					Util::IsInFrontOfCam(result.hitObject->worldBound.center) ||
					(niCam && RE::NiCamera::BoundInFrustum(result.hitObject->worldBound, niCam.get()))
				};

				// Faded objects are hit on their 'outward-facing' surface by the crosshair raycast,
				// which is not a surface visible to the players that are beyond
				// the faded object, so exclude such objects from determining
				// the crosshair's world position and selected refr.
				isFadedByCam = glob.cam->obstructionsToFadeIndicesMap.contains(result.hitObject);
				if (excluded || !inFrontOfCam || isFadedByCam)
				{
					continue;
				}

				isActivator = 
				{
					(hitRefrPtr->GetObjectReference() && hitRefrPtr->GetObjectReference()->Is(RE::FormType::Activator)) ||
					(
						result.hitObject->userData && 
						result.hitObject->userData->GetObjectReference() && 
						result.hitObject->userData->GetObjectReference()->Is(RE::FormType::Activator)
					)
				};

				// An activator was hit in this group of hit results.
				if (isActivator && !activatorHit)
				{
					activatorHit = true;
				}

				if (a_crosshairActiveForSelection) 
				{
					bool validType = Util::IsSelectableRefr(hitRefrPtr.get());
					if (a_inCombat)
					{
						auto asActor = hitRefrPtr->As<RE::Actor>();
						// Do not choose corpses, other players,
						// or player teammates that are not hostile to this player.
						validType &= 
						(
							!asActor || 
							(
								!asActor->IsDead() && !GlobalCoopData::IsCoopPlayer(asActor) && 
								(!asActor->IsPlayerTeammate() || asActor->IsHostileToActor(coopActor.get()))
							)
						);
					}

					// Not a valid type to use for selection,
					// but still set the hit position to use as the crosshair world position
					// if no other result was chosen yet and if the hit object is not an activator.
					if (!validType)
					{
						// Do not set chosen hit pos to activator's hit pos, 
						// since activators have no character collision.
						if (!chosenResult.hit && !isActivator) 
						{
							// Set as hit position target, even if not selectable, for aiming purposes.
							chosenResult.hit = true;
							chosenResult.hitPos = result.hitPos;
						}

						// Keep looking for a result to use for crosshair refr selection.
						continue;
					}
				}

				// Chosen hit result can be set directly at this point.
				chosenResult = result;
				// Only continue searching if an activator was hit
				// and the current hit result is an activator.
				if (!activatorHit || !isActivator) 
				{
					break;
				}
			}
			else
			{
				// Hiht object is a navmesh block or terrain without an associated refr.
				// Still have to check if in front of the cam and not faded.
				// Not valid for target selection.
				inFrontOfCam = 
				{
					Util::IsInFrontOfCam(result.hitObject->world.translate) ||
					Util::IsInFrontOfCam(result.hitObject->worldBound.center) ||
					(niCam && RE::NiCamera::BoundInFrustum(result.hitObject->worldBound, niCam.get()))
				};

				isFadedByCam = glob.cam->obstructionsToFadeIndicesMap.contains(result.hitObject);
				if (!inFrontOfCam || isFadedByCam)
				{
					continue;
				}

				// If no activator was hit or not selecting a crosshair refr,
				// set hit result to this object.
				if (!activatorHit || !a_crosshairActiveForSelection)
				{
					chosenResult = result;
					break;
				}
			}
		}

		// Closest result is the first one.
		choseClosestResult = i == 0 && chosenResult.hit;

		// REMOVE after debugging.
		/*
		auto activationRefrPtr = Util::GetRefrPtrFromHandle(chosenResult.hitRefrHandle);
		ALYSLC::Log("[TM] PickRaycastHitResult: {}: chose result {}: {}. For target selection: {}, is closest result: {}.",
			coopActor->GetName(), chosenResult.hit ? i : -1,
			activationRefrPtr ? activationRefrPtr->GetName() : chosenResult.hitObject ? chosenResult.hitObject->name : "NONE",
			a_crosshairActiveForSelection, choseClosestResult);
		*/

		return chosenResult;
	}

	bool TargetingManager::RefrIsInActivationRange(RE::ObjectRefHandle a_refrHandle) const
	{
		// Check if the given refr is within the player's activation range.

		auto refrPtr = Util::GetRefrPtrFromHandle(a_refrHandle);
		// Invalid refr.
		if (!refrPtr) 
		{
			return false;
		}

		// Can reach objects further away when mounted.
		float maxCheckDist = 
		(
			(coopActor->IsOnMount()) ? 
			Settings::fMountedActivationReachMult * maxReachActivationDist : 
			maxReachActivationDist
		);
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
			return coopActor->data.location.GetDistance(refrPtr->data.location) <= Settings::fMaxDistToRevive;
		}
		else
		{
			// Get refr location to use:
			// Default to half-way up the refr,
			// then if the refr is crosshair-selected, use the crosshair's position.
			// Otherwise, use the position corresponding to the refr's 3D center 
			// or half-way up the refr's 3D object.
			RE::NiPoint3 refrLoc = refrPtr->data.location + RE::NiPoint3(0.0f, 0.0f, refrPtr->GetHeight() / 2.0f);
			if (a_refrHandle == crosshairRefrHandle)
			{
				refrLoc = crosshairWorldPos;
			}
			else if (auto refr3D = Util::GetRefr3D(refrPtr.get()); refr3D)
			{
				refrLoc = refr3D->worldBound.center;
				if (refr3D->worldBound.center.Length() == 0.0f && refr3D->world.translate.Length() != 0.0f)
				{
					refrLoc = refr3D->world.translate + RE::NiPoint3(0.0f, 0.0f, refrPtr->GetHeight() / 2.0f);
				}
			}
		
			return refrLoc.GetDistance(Util::Get3DCenterPos(coopActor.get())) <= maxCheckDist;
		}
	}

	void TargetingManager::ResetTPs()
	{
		// Reset all player timepoints handled by this manager to the current time.

		p->crosshairLastActiveTP			=
		p->lastAutoGrabTP					=
		p->lastCrosshairUpdateTP			=
		p->lastHiddenInStealthRadiusTP		=
		p->lastStealthStateCheckTP			=
		p->crosshairRefrVisibilityLostTP	= 
		p->crosshairRefrVisibilityCheckTP	= SteadyClock::now();
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
			const float maxCheckDist =
			(
				(coopActor->IsOnMount()) ?
				Settings::fMountedActivationReachMult * maxReachActivationDist :
				maxReachActivationDist
			);

			// Get each reference within range and record the magnitude of
			// its angular distance from the player's facing angle.
			Util::ForEachReferenceInRange
			(
				playerCenterPos, maxCheckDist, true,
				[this, &currentMount, &playerCenterPos, &convHeadingAng, &facingDirXY, &maxCheckDist](RE::TESObjectREFR* a_refr) 
				{
					// On to the next one.
					if (!a_refr || !Util::HandleIsValid(a_refr->GetHandle()) || !a_refr->IsHandleValid())
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					auto baseObj = a_refr->GetBaseObject();
					// On to the next one x2.
					if (!baseObj || !a_refr->Is3DLoaded() || !a_refr->GetCurrent3D() || a_refr->IsDeleted()) 
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
					// Useless to activate alert, hostile actors in combat.
					const bool activateHostileActor = 
					{ 
						(asActor && !asActor->IsDead()) && 
						(asActor->IsHostileToActor(glob.player1Actor.get()) || asActor->IsHostileToActor(coopActor.get())) && 
						(asActor->RequestDetectionLevel(coopActor.get()) > 0)
					};

					// This reference is an interactable object that is not a blacklisted actor 
					// or a live hostile actor in combat with the player.
					if (!blacklisted && !activateHostileActor && Util::IsSelectableRefr(a_refr) && Util::IsValidRefrForTargeting(a_refr))
					{
						// Check three points on the refr for the best measurement 
						// of where the refr is located relative to the player.
						// Start with the reported refr location.
						RE::NiPoint3 refrLoc1 = a_refr->data.location;
						RE::NiPoint3 toRefrDirXY = refrLoc1 - playerCenterPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();

						// Minimum selection factor [0, 2]. 
						// Get the minimum factor among the (potentially) three refr positions.
						// Negate the dot product, meaning the more the player has to turn to face the object,
						// the larger the factor.
						// Then we add 1 to ensure all dot product results are > 0, 
						// and mult by 0.5 to set the range to [0, 1]
						// Lastly scale by the distance from the player to the object,
						// meaning objects that are further away have a larger factor.
						// Divide by max reach distance to set range to [0, 1]
						float minSelectionFactor = (0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + (playerCenterPos.GetDistance(refrLoc1) / maxCheckDist);

						// Next two positions only exist if the refr's 3D is available.
						std::optional<RE::NiPoint3> refrLoc2 = std::nullopt;
						std::optional<RE::NiPoint3> refrLoc3 = std::nullopt;
						if (auto refr3D = Util::GetRefr3D(a_refr); refr3D)
						{
							refrLoc2 = refr3D->world.translate;
							refrLoc3 = refr3D->worldBound.center;
						}

						// Refr 3D world position.
						if (refrLoc2.has_value())
						{
							toRefrDirXY = refrLoc2.value() - playerCenterPos;
							toRefrDirXY.z = 0.0f;
							toRefrDirXY.Unitize();
							float selectionFactor = (0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + (playerCenterPos.GetDistance(refrLoc2.value()) / maxCheckDist);
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
							float selectionFactor = (0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + (playerCenterPos.GetDistance(refrLoc3.value()) / maxCheckDist);
							if (selectionFactor < minSelectionFactor)
							{
								minSelectionFactor = selectionFactor;
							}
						}

						nearbyReferences.insert(std::pair<float, RE::ObjectRefHandle>(minSelectionFactor, a_refr->GetHandle()));
					}

					return RE::BSContainer::ForEachResult::kContinue;
				});
				
			// Add the game's crosshair pick refr if any.
			if (auto pickData = RE::CrosshairPickData::GetSingleton(); pickData)
			{
				if (auto pickRefrPtr = Util::GetRefrPtrFromHandle(pickData->target); pickRefrPtr)
				{
					RE::NiPoint3 refrLoc = pickRefrPtr->data.location;
					if (auto refr3D = Util::GetRefr3D(pickRefrPtr.get()); refr3D)
					{
						refrLoc = refr3D->world.translate;
					}

					if (refrLoc.GetDistance(playerCenterPos) < maxCheckDist && Util::IsValidRefrForTargeting(pickRefrPtr.get())) 
					{
						const bool blacklisted =
						(
							(currentMount && pickRefrPtr == currentMount) ||
							(pickRefrPtr->As<RE::Actor>() && pickRefrPtr->As<RE::Actor>()->IsPlayerTeammate()) ||
							(glob.coopEntityBlacklistFIDSet.contains(pickRefrPtr->formID))
						);
						if (!blacklisted) 
						{
							// Save pick data refr handle.
							crosshairPickRefrHandle = pickData->target;

							RE::NiPoint3 toRefrDirXY = refrLoc - playerCenterPos;
							toRefrDirXY.z = 0.0f;
							toRefrDirXY.Unitize();
							float selectionFactor = 
							(
								(0.5f * (1.0f - facingDirXY.Dot(toRefrDirXY))) + (playerCenterPos.GetDistance(refrLoc) / maxCheckDist)
							);
							nearbyReferences.insert(std::pair<float, RE::ObjectRefHandle>(selectionFactor, crosshairPickRefrHandle));
						}
					}
				}
			}
		}

		// Clear old proximity refr handle before setting a new one.
		proximityRefrHandle = RE::ObjectRefHandle();
		if (!nearbyReferences.empty())
		{
			// Get next selectable refr in view of the camera and remove it from the map.
			while (!nearbyReferences.empty())
			{
				auto nextRefrNodeHandle = nearbyReferences.extract(nearbyReferences.begin());
				// Do not select invalid refrs or refrs not in view of the camera.
				// NOTE: These refrs, aside from the crosshair pick refr, 
				// are not directly selected with a player's crosshair,
				// so LOS still has to be checked.
				if (!nextRefrNodeHandle.empty())
				{
					const auto& nextRefrHandle = nextRefrNodeHandle.mapped();
					if (auto nextRefrPtr = Util::GetRefrPtrFromHandle(nextRefrHandle); nextRefrPtr) 
					{
						// Choose if the player has LOS on the refr.
						if (nextRefrHandle == crosshairPickRefrHandle || 
							Util::HasLOS(nextRefrPtr.get(), coopActor.get(), false, false, crosshairWorldPos))
						{
							proximityRefrHandle = nextRefrHandle;
							break;
						}
					}
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
			type = a_type;
			if (selectedTargetActorPtr && !selectedTargetActorPtr->IsDead())
			{
				// Alive actor. Show name and level.
				// Hostile actors' names are displayed in red.
				auto levelRGB = GetLevelDifferenceRGB(selectedTargetActorHandle);
				text = fmt::format
				(
					"P{}: {} <font color=\"#{:X}\">L{}</font> <font color=\"#{:X}\">{}</font>",
					playerID + 1, p->mm->shouldFaceTarget ? "Facing" : "Targeting",
					levelRGB, selectedTargetActorPtr->GetLevel(),
					selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ? 0xFF0000 : 0xFFFFFF,
					selectedTargetActorPtr->GetDisplayFullName()
				);
			}
			else if (auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle); crosshairRefrPtr)
			{
				// Get activation text for the crosshair refr.
				RE::BSString activationText = crosshairRefrPtr->GetName();
				if (auto objRef = crosshairRefrPtr->GetObjectReference(); objRef)
				{
					objRef->GetActivateText(crosshairRefrPtr.get(), activationText);
				}

				text = fmt::format("P{}: {}", playerID + 1, activationText);
			}
		}
		else if (a_type == CrosshairMessageType::kStealthState)
		{
			type = a_type;
			if (coopActor->IsSneaking())
			{
				const auto isInCombat = coopActor->IsInCombat();
				const bool checkSelectedTarget = selectedTargetActorPtr && !selectedTargetActorPtr->IsDead() && !isInCombat;
				// Set sneak info text to indicate the player's hidden percent,
				// which is determined by their remaining stealth points:
				// (player's total stealth points - max(all aggro'd actors' stealth point decrements))
				
				// If a particular target actor is selected, 
				// show their individual detection level of the player as well.
				if (checkSelectedTarget)
				{
					const bool isPlayer = GlobalCoopData::IsCoopPlayer(selectedTargetActorPtr.get());
					float detectionLvl = selectedTargetActorPtr->RequestDetectionLevel(coopActor.get());
					float targetDetectionPct = static_cast<uint8_t>((std::clamp(detectionLvl, -20.0f, 0.0f) + 20.0f) * 5.0f);
					uint32_t targetDetectionPctRGB = GetDetectionLvlRGB(targetDetectionPct, false);
					// Set sneak info text to indicate the currently selected/aim correction target's
					// detection level of the player and the overall detection percentage 
					// of the player for all relevant high process actors.
					// Hostile actors have their names reported in red.
					text = fmt::format
					(
						"P{}: Detected by <font color=\"#{:X}\">{}</font> (<font color=\"#{:X}\">{}%</font>), overall: (<font color=\"#{:X}\">{}%</font>)",
						playerID + 1,
						!isPlayer && selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ? 0xFF0000 : 0xFFFFFF,
						selectedTargetActorPtr->GetDisplayFullName(),
						targetDetectionPctRGB, targetDetectionPct,
						detectionPctRGB, detectionPct
					);
				}
				else
				{
					// Detection percent reported accounts for all relevant actors in the high process.
					text = fmt::format("P{}: Detected (<font color=\"#{:X}\">{}%</font>)",
						playerID + 1, detectionPctRGB, detectionPct);
				}
			}
		}

		SetCurrentCrosshairMessage(false, std::move(type), text);
	}

	bool TargetingManager::ShouldRefreshNearbyReferences()
	{
		// Check if a list of nearby refrs should be re-calculated when getting the next proximity refr to activate.

		const RE::NiPoint3& playerCenterPos = Util::Get3DCenterPos(coopActor.get());
		const float& facingAngle = coopActor->GetHeading(false);
		// Refresh if either far away from previous activation position,
		// or if turning away enough from last activation rotation.
		bool facingNewDir = fabsf(Util::NormalizeAngToPi(facingAngle - lastActivationFacingAngle)) > Settings::fMinTurnAngToRefreshRefrs;
		bool farAwayFromPrevSpot = playerCenterPos.GetDistance(lastActivationReqPos) > Settings::fMinMoveDistToRefreshRefrs;
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

		bool rangedAttackOrBlockRequest = 
		{
			(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) && p->em->Has2HRangedWeapEquipped()) ||
			(p->pam->AllInputsPressedForAction(InputAction::kCastRH) && p->em->HasRHSpellEquipped()) ||
			(p->pam->AllInputsPressedForAction(InputAction::kAttackRH) && p->em->HasRHStaffEquipped()) ||
			(p->pam->AllInputsPressedForAction(InputAction::kCastLH) && p->em->HasLHSpellEquipped()) ||
			(p->pam->AllInputsPressedForAction(InputAction::kAttackLH) && p->em->HasLHStaffEquipped()) ||
			(p->pam->AllInputsPressedForAction(InputAction::kQuickSlotCast) && p->em->quickSlotSpell) ||
			(p->pam->reqSpecialAction == SpecialActionType::kDualCast || p->pam->reqSpecialAction == SpecialActionType::kQuickCast) ||
			(p->pam->isBlocking) ||
			(p->pam->isBashing)
		};

		auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle); 
		if (rangedAttackOrBlockRequest && !p->mm->shouldFaceTarget && !selectedTargetActorPtr)
		{
			// Trying to perform/performing ranged attack,
			// no selected actor, and not facing the crosshair position.
			auto prevTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle);
			auto nextTargetPtr = Util::GetActorPtrFromHandle(GetClosestTargetableActorInFOV(RE::ObjectRefHandle(), Settings::vfAimCorrectionFOV[playerID]));
			bool baselineValidity = nextTargetPtr && Util::IsValidRefrForTargeting(nextTargetPtr.get());
			if ((baselineValidity && nextTargetPtr != prevTargetPtr) &&
				Util::HasLOS(nextTargetPtr.get(), coopActor.get(), false, false, crosshairWorldPos))
			{
				// Set valid, different target that is within LOS of the player.
				aimCorrectionTargetHandle = nextTargetPtr->GetHandle();
			}
			else if (!nextTargetPtr && nextTargetPtr != prevTargetPtr)
			{
				// Clear old target if there is no current target.
				ClearTarget(TargetActorType::kAimCorrection);
			}
		}
		else
		{
			// Clear aim correction target when not attacking or trying to attack, blocking,
			// or when a crosshair target actor is selected.
			const auto& combatGroup = glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at(ActionGroup::kCombat);
			bool combatActionBindsPressed = false;
			if (coopActor->IsWeaponDrawn()) 
			{
				for (auto actionIndex : combatGroup)
				{
					combatActionBindsPressed |= p->pam->AllInputsPressedForAction(static_cast<InputAction>(actionIndex));
					if (combatActionBindsPressed)
					{
						break;
					}
				}
			}

			auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle); 
			if ((aimCorrectionTargetPtr) && ((selectedTargetActorPtr) || (!combatActionBindsPressed && !p->pam->isAttacking)))
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
			isRanged = weap->IsBow() || weap->IsCrossbow() ||
					   weap->IsStaff();
		}
		else if (weapMagObj)
		{
			isRanged = weapMagObj->IsMagicItem() || weapMagObj->As<RE::TESShout>();
		}

		if (isRanged)
		{
			auto currentTargetRefrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle);
			auto newTargetRefrPtr = Util::GetRefrPtrFromHandle(GetRangedPackageTargetRefr(weapMagObj));
			bool newIsValid = newTargetRefrPtr && Util::IsValidRefrForTargeting(newTargetRefrPtr.get());
			if (newIsValid && newTargetRefrPtr != currentTargetRefrPtr)
			{
				// Set new valid linked refr.
				coopActor->extraList.SetLinkedRef(newTargetRefrPtr.get(), p->aimTargetKeyword);
				aimTargetLinkedRefrHandle = newTargetRefrPtr->GetHandle();
			}
			else if (!newIsValid && currentTargetRefrPtr)
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
		const bool& facingTarget = p->mm->shouldFaceTarget;
		// Rotate 45 degrees when fully facing the crosshair position.
		float endPointAng = facingTarget ? PI / 4.0f : 0.0f;
		crosshairRotationData->next = endPointAng;
		if (crosshairRotationData->current != endPointAng)
		{
			crosshairRotationData->InterpolateSmootherStep(min(crosshairRotationData->secsSinceUpdate / crosshairRotationData->secsUpdateInterval, 1.0f));
			crosshairRotationData->IncrementUpdateInterval(*g_deltaTimeRealTime);
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
		const float currentCrosshairGap = Settings::vfCrosshairGapRadius[playerID] + crosshairOscillationData->current;
		// Crosshair gap at max expansion.
		const float maxCrosshairGap = max(crosshairLength, Settings::vfCrosshairGapRadius[playerID] * 2.0f);
		// Includes inner outline, prong itself, and current interpolated gap.
		float currentProngDistFromCenter = 2.0f * crosshairThickness + currentCrosshairGap + crosshairLength;
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
		// Update the current interpolated gap value if it hasn't reached the interpolation endpoint yet.
		if (crosshairOscillationData->current != endPointGapDelta)
		{
			crosshairOscillationData->InterpolateSmootherStep(min(crosshairOscillationData->secsSinceUpdate / crosshairOscillationData->secsUpdateInterval, 1.0f));
			crosshairOscillationData->IncrementUpdateInterval(*g_deltaTimeRealTime);

			// Reached the interpolation endpoint now, so signal completion,
			// and set previous, current, and next interpolation values to the endpoint value.
			if (crosshairOscillationData->current == endPointGapDelta)
			{
				crosshairOscillationData->SetUpdateDurationAsComplete();
				crosshairOscillationData->SetData(endPointGapDelta, endPointGapDelta, endPointGapDelta);
			}
		}
	}

	void TargetingManager::UpdateCrosshairPosAndSelection()
	{
		// Update the player's crosshair position and selected refr data.

		auto niCam = Util::GetNiCamera();
		auto ui = RE::UI::GetSingleton();
		// Overlay menu to draw the crosshair on.
		auto overlay = ui ? ui->GetMenu(DebugOverlayMenu::MENU_NAME) : nullptr;
		auto view = overlay ? overlay->uiMovie : nullptr;
		if (!niCam || !ui || !overlay || !view)
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
		// Only actors are selectable when in combat. \
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
			const float secsSinceCrosshairUpdated = Util::GetElapsedSeconds(p->lastCrosshairUpdateTP);

			// Max pixels per second that the crosshair can travel across along the X and Y screen axes.
			float crosshairMaxXSpeedPPS = 
			(
				Settings::vfCrosshairHorizontalSensitivity[playerID] * 
				Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			float crosshairMaxYSpeedPPS = 
			(
				Settings::vfCrosshairVerticalSensitivity[playerID] * Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			// X and Y crosshair speed multipliers which are adjusted if the player 
			// is moving their crosshair across a selected refr with crosshair magnetism enabled.
			float crosshairSpeedMultX = 1.0f;
			float crosshairSpeedMultY = 1.0f;
			const auto port = Util::GetPort();
			// Slow down the moving crosshair when an actor or refr is selected.
			// NOTE: VERY rough estimates of pixel dimensions. Will be improved in the future.
			bool adjustMovementOverActor = Settings::vbCrosshairMagnetismForActors[playerID] && selectedTargetActorPtr;
			bool adjustMovementOverRefr = Settings::vbCrosshairMagnetismForObjRefs[playerID] && crosshairRefrPtr;
			if (adjustMovementOverActor || adjustMovementOverRefr)
			{
				const auto& targetPtr = adjustMovementOverActor ? selectedTargetActorPtr : crosshairRefrPtr;
				float pixelHeight = Util::GetBoundPixelDist(targetPtr.get(), true);
				float pixelWidth = Util::GetBoundPixelDist(targetPtr.get(), false);
				// Do not modify if speed mults the selected refr is too narrow,
				// or if the minimum traversal time or max pixels per second
				// are erroneously set to 0 or below.
				crosshairSpeedMultX = 
				(
					pixelWidth == 1.0f || Settings::vfMinSecsCrosshairTargetTraversal[playerID] <= 0.0f || crosshairMaxXSpeedPPS <= 0.0f ?
					1.0f :
					min(pixelWidth / (crosshairMaxXSpeedPPS * Settings::vfMinSecsCrosshairTargetTraversal[playerID]), 1.0f)
				);
				crosshairSpeedMultY = 
				(
					pixelHeight == 1.0f || Settings::vfMinSecsCrosshairTargetTraversal[playerID] == 0.0f || crosshairMaxYSpeedPPS == 0.0f ?
					1.0f :
					min(pixelHeight / (crosshairMaxYSpeedPPS * Settings::vfMinSecsCrosshairTargetTraversal[playerID]), 1.0f)
				);
			}

			// In the case that the raycast-collidable distance across the target is larger than the estimated bound height/width,
			// the crosshair will move much too slow, so set a lower bound to the crosshair speed mults.
			crosshairSpeedMultX = max(0.1f, crosshairSpeedMultX);
			crosshairSpeedMultY = max(0.1f, crosshairSpeedMultY);
			// Number of pixels to move across in the X and Y directions this update.
			float deltaXPx = crosshairSpeedMultX * rsX * rsMag * secsSinceCrosshairUpdated * crosshairMaxXSpeedPPS;
			float deltaYPx = crosshairSpeedMultY * rsY * rsMag * secsSinceCrosshairUpdated * crosshairMaxYSpeedPPS;

			// When moving over a refr, add the pixel deltas relative to the initial 'entry' position 
			// which was set when the crosshair first selected the refr. 
			// This will allow the crosshair to 'stick' to moving targets while moving it across the target,
			// since the change in pixels is made relative to the target's movement.
			if (crosshairRefrPtr && crosshairInitialHitLocalPos.has_value())
			{
				// Add to cumulative pixels deltas.
				crosshairOnRefrPixelXYDeltas.x += deltaXPx;
				crosshairOnRefrPixelXYDeltas.y += deltaYPx;
				const auto refrPos = crosshairRefrPtr->data.location;
				// Get updated world position that corresponds to 
				// the initial hit position on the current crosshair refr,
				// which was stored as a local offset to the refr's reported position.
				auto newBaseCrosshairWorldPos = refrPos + crosshairInitialHitLocalPos.value();
				// Get corresponding screen position.
				auto screenPos = Util::WorldToScreenPoint3(newBaseCrosshairWorldPos);
				// Add deltas to this base screen position 
				// to allow the crosshair to move relative to the selected refr.
				crosshairScaleformPos.x = std::clamp(screenPos.x + crosshairOnRefrPixelXYDeltas.x, 0.0f, rectWidth);
				crosshairScaleformPos.y = std::clamp(screenPos.y + crosshairOnRefrPixelXYDeltas.y, 0.0f, rectHeight);
				crosshairScaleformPos.z = 0.0f;
			}
			else
			{
				// Update scaleform position with the pixel deltas.
				crosshairScaleformPos.x = std::clamp(crosshairScaleformPos.x + deltaXPx, 0.0f, rectWidth);
				crosshairScaleformPos.y = std::clamp(crosshairScaleformPos.y + deltaYPx, 0.0f, rectHeight);
				crosshairScaleformPos.z = 0.0f;
			}

			// Clear selected actor and crosshair refr before checking for raycast/proximity refr hits below.
			selectedTargetActorHandle = RE::ActorHandle();
			crosshairRefrHandle = RE::ObjectRefHandle();

			// Calculate near and far plane world positions for the current scaleform position.
			glm::mat4 pvMat{};
			// Transpose first.
			pvMat[0][0] = niCam->worldToCam[0][0];
			pvMat[1][0] = niCam->worldToCam[0][1];
			pvMat[2][0] = niCam->worldToCam[0][2];
			pvMat[3][0] = niCam->worldToCam[0][3];
			pvMat[0][1] = niCam->worldToCam[1][0];
			pvMat[1][1] = niCam->worldToCam[1][1];
			pvMat[2][1] = niCam->worldToCam[1][2];
			pvMat[3][1] = niCam->worldToCam[1][3];
			pvMat[0][2] = niCam->worldToCam[2][0];
			pvMat[1][2] = niCam->worldToCam[2][1];
			pvMat[2][2] = niCam->worldToCam[2][2];
			pvMat[3][2] = niCam->worldToCam[2][3];
			pvMat[0][3] = niCam->worldToCam[3][0];
			pvMat[1][3] = niCam->worldToCam[3][1];
			pvMat[2][3] = niCam->worldToCam[3][2];
			pvMat[3][3] = niCam->worldToCam[3][3];
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
			// Derive world positions using the inverted projection view matrix and the clip space vectors.
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
			const bool& hasHit = centerResult.hit;
			if (hasHit)
			{
				// Update crosshair world pos if the raycast hits anything, selectable or not.
				crosshairWorldPos = ToNiPoint3(centerResult.hitPos);
				if (Util::HandleIsValid(centerResult.hitRefrHandle))
				{
					// Must be valid for selection.
					if (validCrosshairRefrHit = IsRefrValidForCrosshairSelection(centerResult.hitRefrHandle); validCrosshairRefrHit)
					{
						// Set crosshair refr handle.
						crosshairRefrHandle = centerResult.hitRefrHandle;
						crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
						// Set the hit position's local offset from the hit refr's position.
						const auto refrPos = crosshairRefrPtr->data.location;
						crosshairLastHitLocalPos = crosshairWorldPos - refrPos;
						// If no refr was selected or a new one is selected, 
						// set the initial local hit pos to the last calculated one.
						if (!prevCrosshairRefrPtr || crosshairRefrPtr != prevCrosshairRefrPtr)
						{
							crosshairInitialHitLocalPos = crosshairLastHitLocalPos;
							// Has just selected the refr, 
							// so no crosshair movement across it yet.
							crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
						}

						// Set selected actor handle if the hit refr is an actor.
						if (auto hitActor = crosshairRefrPtr->As<RE::Actor>(); hitActor)
						{
							selectedTargetActorHandle = hitActor->GetHandle();
						}
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
			validCrosshairRefrHit = IsRefrValidForCrosshairSelection(crosshairRefrHandle) && Util::IsSelectableRefr(crosshairRefrPtr.get());
			if (validCrosshairRefrHit)
			{
				// Update the crosshair world position using
				// the initial local hit position and the refr's new position.
				const RE::NiPoint3 refrPos = crosshairRefrPtr->data.location;
				crosshairWorldPos = refrPos + crosshairLastHitLocalPos;
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
				crosshairInitialHitLocalPos = std::nullopt;
				crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
			}
		}
		else
		{
			// No targeted refr, so we only have to update the crosshair world position.

			// Calculate near and far plane world positions for the current scaleform position.
			glm::mat4 pvMat{};
			// Transpose first.
			pvMat[0][0] = niCam->worldToCam[0][0];
			pvMat[1][0] = niCam->worldToCam[0][1];
			pvMat[2][0] = niCam->worldToCam[0][2];
			pvMat[3][0] = niCam->worldToCam[0][3];
			pvMat[0][1] = niCam->worldToCam[1][0];
			pvMat[1][1] = niCam->worldToCam[1][1];
			pvMat[2][1] = niCam->worldToCam[1][2];
			pvMat[3][1] = niCam->worldToCam[1][3];
			pvMat[0][2] = niCam->worldToCam[2][0];
			pvMat[1][2] = niCam->worldToCam[2][1];
			pvMat[2][2] = niCam->worldToCam[2][2];
			pvMat[3][2] = niCam->worldToCam[2][3];
			pvMat[0][3] = niCam->worldToCam[3][0];
			pvMat[1][3] = niCam->worldToCam[3][1];
			pvMat[2][3] = niCam->worldToCam[3][2];
			pvMat[3][3] = niCam->worldToCam[3][3];
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
			// Derive world positions using the inverted projection view matrix and the clip space vectors.
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
		// Is a player rotating the co-op camera or is P1 rotating the default TP camera.
		bool isCamRotating = 
		{
			(glob.cam->IsRunning()) ?
			(glob.cam->camAdjMode == CamAdjustmentMode::kRotate && 
			 glob.cam->controlCamCID != -1 && 
			 glob.coopPlayers[glob.cam->controlCamCID]->pam->IsPerforming(InputAction::kRotateCam)) :
			((playerCam) && (playerCam->rotationInput.x != 0.0f || playerCam->rotationInput.y != 0.0f))
		};

		// Adjusted when actively moving the crosshair,
		// when facing a target, when selecting a new target,
		// or when first hitting the edge of the screen.
		if ((crosshairRefrPtr) ||
			(isMovingCrosshair || p->mm->shouldFaceTarget) ||	
			(crosshairRefrPtr != prevCrosshairRefrPtr) ||
			(scaleformPosOnEdgeOfScreen && !prevScaleformPosOnEdgeOfScreen))
		{
			p->crosshairLastActiveTP = SteadyClock::now();
		}

		// Re-center the crosshair after an interval passes if there is no valid target,
		// the player is not moving their crosshair, and the player is not facing the crosshair world position.
		if (Settings::vbRecenterInactiveCrosshair[playerID])
		{
			float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
			if (!crosshairRefrPtr && !isMovingCrosshair && 
				!p->mm->shouldFaceTarget && secsSinceActive > Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID])
			{
				// Offset left or right about the center of the screen based on player index..
				float targetPosX = DebugAPI::screenResX / 2.0f + 100.0f * (fmod(playerID, 2) * 2 - 1) * ceil((playerID + 1) / 2.0f);
				float targetPosY = DebugAPI::screenResY / 2.0f;
				// Continue interpolating the position back towards the default position until reached.
				if (crosshairScaleformPos.x != targetPosX || crosshairScaleformPos.y != targetPosY)
				{
					// Re-centering completes after about 1.5 inactivity intervals elapse.
					float tRatio = std::clamp
					(
						(secsSinceActive / max(0.1f, Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID])) - 1.0f,
						0.0f, 
						1.0f
					);
					crosshairScaleformPos.x = Util::InterpolateSmootherStep(crosshairScaleformPos.x, targetPosX, tRatio);
					crosshairScaleformPos.y = Util::InterpolateSmootherStep(crosshairScaleformPos.y, targetPosY, tRatio);
					crosshairScaleformPos.z = 0.0f;

					// Reset pixel deltas too.
					crosshairOnRefrPixelXYDeltas = glm::vec2(0.0f, 0.0f);
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
		
		// NOTE: Only TargetSelection and StealthState message types are set periodically.
		// Other messages types are triggered externally.
		// Can set without delaying.
		bool noDelay = false;
		// This message's type is one of the delayed types 
		// listed by the currently set message.
		bool isDelayedType = false;
		// The current message's delay restriction interval has passed.
		bool delayPassed = false;
		// External message was set.
		bool extMessageSet = false;
		// Clear current message before setting it below.
		crosshairMessage->Clear();
		// If there is an externally-requested crosshair message, prioritize it.
		if (extCrosshairMessage->type != CrosshairMessageType::kNone) 
		{
			noDelay = lastCrosshairMessage->delayedMessageTypes.empty();
			isDelayedType = !noDelay && lastCrosshairMessage->delayedMessageTypes.contains(extCrosshairMessage->type);
			delayPassed = Util::GetElapsedSeconds(lastCrosshairMessage->setTP) > lastCrosshairMessage->secsMaxDisplayTime;
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
			// Set to last message so that no new message is set if there is no update below.
			crosshairMessage->CopyMessageData(lastCrosshairMessage);
			// Display selection text if not sneaking 
			// or if highlighting a non-actor or corpse refr with the crosshair.
			// Display stealth state text otherwise.
			auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
			auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle);
			if ((!coopActor->IsSneaking()) || 
				((crosshairRefrPtr && validCrosshairRefrHit) && (!selectedTargetActorPtr || selectedTargetActorPtr->IsDead())))
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

		// Only set if different.
		if (Hash(crosshairMessage->text) != Hash(lastCrosshairMessage->text)) 
		{
			crosshairMessage->setTP = SteadyClock::now();
			lastCrosshairMessage->CopyMessageData(crosshairMessage);
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
				const auto prevClosestHostileActorPtr = Util::GetActorPtrFromHandle(closestHostileActorHandle);

				// Update stealth detection state after storing previous frame's values.
				UpdateStealthDetectionState();

				// Save the previous closest hostile actor and their distance from the player
				// to use in Sneak XP calculations after updating the stealth state.
				// P1's Sneak skill progression is already handled by the game.
				if (!p->isPlayer1)
				{
					// Add Sneak XP as needed.
					// XP formulae from:
					// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
					auto closestHostileActorPtr = Util::GetActorPtrFromHandle(closestHostileActorHandle);
					// Not fully detected (< 100% detection) within stealth radius of a hostile actor.
					if (detectionPct < 100.0f && closestHostileActorPtr && 
						closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						// Just got within stealth XP radius of hostile actor.
						if (!prevClosestHostileActorPtr || prevClosestHostileActorDist > Settings::fHostileTargetStealthRadius)
						{
							// NOTE: May not need this, but keeping for now.
							// Set last hidden time point.
							p->lastHiddenInStealthRadiusTP = SteadyClock::now();
						}
						else
						{
							GlobalCoopData::AddSkillXP(controllerID, RE::ActorValue::kSneak, 0.625f * secsSinceLastStealthStateCheck);
						}
					}
					else if (!closestHostileActorPtr)
					{
						// Keep last hidden TP updated when not near any hostile actor.
						p->lastHiddenInStealthRadiusTP = SteadyClock::now();
					}

					// Becoming hidden (0% detected) after being detected within stealth radius of hostile actor.
					if (prevDetectionPct > 0.0f && detectionPct == 0.0f && 
						closestHostileActorPtr && closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						GlobalCoopData::AddSkillXP(controllerID, RE::ActorValue::kSneak, 2.5f);
					}
				}
			}
		}
		else if (secsSinceLastStealthStateCheck != 0.0f)
		{
			// Reset stealth state check interval when not sneaking since no checks are being performed.
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
		const auto isInCombat = coopActor->IsInCombat();
		if (auto processLists = RE::ProcessLists::GetSingleton(); processLists)
		{
			// Accumulate detection percent.
			detectionPct = 0.0f;
			// Check each high process actor.
			for (const auto handle : processLists->highActorHandles)
			{
				// Must be valid.
				if (auto actorPtr = Util::GetActorPtrFromHandle(handle);
					actorPtr && Util::IsValidRefrForTargeting(actorPtr.get()) && !actorPtr->IsDead())
				{
					const auto nameHash = Hash(actorPtr->GetName());
					// Ignore actors that do not modify the stealth state,
					// the player themselves, and the player's current mount.
					const bool ignored = 
					{
						actorPtr->boolFlags.any(RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter) ||
						actorPtr == coopActor ||
						actorPtr == p->GetCurrentMount()
					};
					if (!ignored)
					{
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
							detectionPct = max(detectionPct, max(0.0f, static_cast<uint8_t>(Util::GetStealthPointsLost(coopActor.get(), actorPtr.get()))));
						}
						else
						{
							// Request detection level directly.
							detectionPct = max(detectionPct, static_cast<uint8_t>(Util::GetDetectionPercent(coopActor.get(), actorPtr.get())));
						}

						// Update the closest hostile actor and their distance to the player.
						const float distToPlayer = actorPtr->data.location.GetDistance(coopActor->data.location);
						if (actorPtr->IsHostileToActor(coopActor.get()) && distToPlayer < closestHostileActorDist)
						{
							closestHostileActorHandle = actorPtr->GetHandle();
							closestHostileActorDist = distToPlayer;
						}
					}
				}
			}
		}

		// Get detection percent RGB value after updating the player's detection percent.
		detectionPctRGB = GetDetectionLvlRGB(detectionPct, false);
	}

	void TargetingManager::ActorTargetMotionState::UpdateMotionState(RE::ActorHandle a_targetActorHandle)
	{
		// Update physical motion-related data for given the target actor.
		// Used for projectile trajectory calculations if the player is using AimPrediction projectiles.
		if (a_targetActorHandle != targetActorHandle) 
		{
			Refresh();
		}

		targetActorHandle = a_targetActorHandle;
		if (auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle); 
			targetActorPtr && targetActorPtr->IsHandleValid() && targetActorPtr->GetCharController()) 
		{
			lastUpdateTP = SteadyClock::now();
			if (firstUpdate)
			{
				// Need to set both current and previous positions to the initial position on the first update.
				pPos = cPos = Util::GetActorWorldPosition(targetActorPtr.get());
				pVel = cVel = Util::GetActorLinearVelocity(targetActorPtr.get());
				// Same with the previous and current yaw values.
				pYaw = cYaw = cVel.Length() == 0.0f ? targetActorPtr->GetHeading(false) : Util::GetYawBetweenPositions(RE::NiPoint3(), cVel);
				firstUpdate = false;
			}
			else
			{
				++avgDataFrameCount;

				// Set previous data to current data before updating the current data.
				pPos = cPos;
				cPos = Util::GetActorWorldPosition(targetActorPtr.get());
				pVel = cVel;
				cVel = Util::GetActorLinearVelocity(targetActorPtr.get());
				cVel.Unitize();
				cVel *= targetActorPtr->DoGetMovementSpeed();

				pAccelPerFrame = cAccelPerFrame;
				cAccelPerFrame = (cVel - pVel);
				cSpeedDeltaPerFrame = (cVel.Length() - pVel.Length());

				pYaw = cYaw;
				cYaw = targetActorPtr->GetHeading(false);
				pYawAngDeltaPerFrame = cYawAngDeltaPerFrame;
				cYawAngDeltaPerFrame = Util::NormalizeAngToPi(cYaw - pYaw);

				// Total over an interval data used to get the average per interval.
				toiAccel += cAccelPerFrame;
				toiSpeedDelta += cSpeedDeltaPerFrame;
				toiVel += cVel;
				toiYawAngDelta += cYawAngDeltaPerFrame;

				// Reported velocity changes are drastic even over a short amount of time,
				// leading to large, accumulating errors in aim prediction.
				// Can't reliably use them alone, so the average per interval is also calculated.
				if (avgDataFrameCount == FRAMES_BETWEEN_AVG_DATA_UPDATES)
				{
					apiAccel = toiAccel / (avgDataFrameCount);
					apiVel = toiVel / (avgDataFrameCount);
					apiSpeedDelta = toiSpeedDelta / (avgDataFrameCount);
					apiYawAngDelta = toiYawAngDelta / (avgDataFrameCount);

					// Reset totals and frame count each time the averages are set.
					toiAccel = 
					toiVel = RE::NiPoint3();
					toiSpeedDelta =
					toiYawAngDelta =
					avgDataFrameCount = 0;
				}
			}

			// Keep incrementing the number of frames
			// until all data is populated (includes the averaged data).
			if (populateCount < FRAMES_BETWEEN_AVG_DATA_UPDATES)
			{
				populateCount++;
			}
		}
	}

	void TargetingManager::GrabbedReferenceInfo::UpdateGrabbedReference(const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index, const float& a_firstGrabbedReferenceRadius)
	{
		// Update the position of grabbed refrs by setting their velocity.
		// Arrange multiple grabbed refrs in a (very poorly formed) ring about the first grabbed refr.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		// Update grabbed refr orientation.
		// Get heaviness factor to better account for different objects when setting linear velocity.
		// Still a work in progress.
		float heavinessFactor = 1.0f;
		if (auto asActor = objectPtr->As<RE::Actor>(); asActor)
		{
			if (auto charController = asActor->GetCharController(); charController)
			{
				heavinessFactor = max(0.0f, objectPtr->GetWeight() - sqrt(asActor->equippedWeight));
			}
		}
		else
		{
			heavinessFactor = sqrt(HAVOK_TO_GAME * (max(0.0f, objectPtr->GetWeight() - 5.0f) + 10.0f));
		}

		bool isRagdolled = a_p->coopActor->IsInRagdollState();
		float facingAng = a_p->coopActor->GetHeading(false);
		if (!a_p->mm->shouldFaceTarget)
		{
			// The last recorded LS game angle.
			facingAng = a_p->mm->movementOffsetParams[!MoveParams::kLSGameAng];
		}
		else if (isRagdolled)
		{
			// For M.A.R.F-ing, attempt to place the other grabbed players
			// between the player and the crosshair world position when facing it.
			facingAng = Util::GetYawBetweenPositions(a_p->coopActor->data.location, a_p->tm->crosshairWorldPos);
		}

		// Suspend the grabbed objects in front of the player
		// at a distance dependent on their max reach and height.
		float xySuspensionDist = max(objectPtr->GetHeight(), a_p->tm->GetMaxActivationDist() / 3.0f);
		float height = a_p->mm->playerScaledHeight;

		// Arranged in a circle about the index 0 grabbed object.
		// Sweeps out clockwise from directly above the index 0 object.
		auto indexBasedOffset = RE::NiPoint3();
		// Offset from index 0 (center) grabbed object.
		// Dependent on the objects' radii/heights.
		float indexOffsetScalar = a_firstGrabbedReferenceRadius;
		if (auto object3D = Util::GetRefr3D(objectPtr.get()); object3D)
		{
			// Add both the central and current grabbed object's radii.
			indexOffsetScalar = a_firstGrabbedReferenceRadius + min(objectPtr->GetHeight(), object3D->worldBound.radius);
		}

		auto forward = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(facingAng));
		// Set direction from the central object based on the object's index.
		if (a_index > 0)
		{
			indexBasedOffset = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			Util::RotateVectorAboutAxis(indexBasedOffset, forward, 2.0f * PI * ((float)(a_index - 1) / (float)(Settings::uMaxGrabbedReferences - 1)));
		}

		// Apply positional offset scalar to the normalized offset after rotation.
		indexBasedOffset *= indexOffsetScalar;

		// Full credits to ersh1 once again for the steps to access motion type and apply linear velocity:
		// https://github.com/ersh1/Precision/blob/702428bc065c75b3964a0324992658b1ab0a0821/src/Havok/ContactListener.cpp#L8
		if (auto hkpRigidBody = Util::GethkpRigidBody(objectPtr.get()); hkpRigidBody)
		{
			// Additional distance and speed mults if paragliding or M.A.R.F-ing.
			float suspensionDistMult = 1.0f;
			float maxSpeedMult = 1.0f;
			// Max speed the grabbed object can reach.
			float grabbedRefrMaxSpeed = Settings::fBaseGrabbedRefrMaxSpeed;
			// Speedmult adjustments specifically for grabbing other players.
			if (Settings::bCanGrabOtherPlayers && GlobalCoopData::IsCoopPlayer(objectPtr.get()))
			{
				const auto& otherP = glob.coopPlayers[GlobalCoopData::GetCoopPlayerIndex(objectPtr)];
				if (a_p->mm->isParagliding)
				{
					// Carry other players while paragliding.
					// Keep them close by.
					maxSpeedMult = 2.0f;
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
			// with players attempting to move in opposite directions,
			// but a bit better when based from the player's torso
			// instead of from their head.
			RE::NiPoint3 basePos =
			(
				a_p->tm->isMARFing ?
				Util::GetTorsoPosition(a_p->coopActor.get()) :
				Util::GetHeadPosition(a_p->coopActor.get())
			);
			targetPosition = 
			{
				basePos.x + xySuspensionDist * suspensionDistMult * cosf(Util::ConvertAngle(facingAng)) * cosf(a_p->mm->aimPitch),
				basePos.y + xySuspensionDist * suspensionDistMult * sinf(Util::ConvertAngle(facingAng)) * cosf(a_p->mm->aimPitch),
				basePos.z
			};

			// Can move the grabbed refr vertically in an arc around the player by adjusting aim pitch.
			if (a_p->tm->isMARFing)
			{
				targetPosition.z += xySuspensionDist * suspensionDistMult * -sinf(a_p->mm->aimPitch);
			}
			else
			{
				targetPosition.z += height * suspensionDistMult * -sinf(a_p->mm->aimPitch);
			}

			// Now add the positional offset based on grab index.
			targetPosition += indexBasedOffset;

			// Positional delta to reach the target position.
			auto posDelta = targetPosition - objectPtr->data.location;
			float distToTargetPos = posDelta.Length();
			// Normalized direction.
			auto dir = distToTargetPos == 0.0f ? RE::NiPoint3() : posDelta / distToTargetPos;

			// Apply max speed mult.
			grabbedRefrMaxSpeed *= maxSpeedMult;
			// Should cap out at the player's movement speed if higher than the pre-defined max speed.
			grabbedRefrMaxSpeed = max(grabbedRefrMaxSpeed, a_p->coopActor->DoGetMovementSpeed());
			// Slow down when nearing the target position.
			// Reduces jitter.
			float slowdownFactor = Util::InterpolateEaseOut
			(
				0.0f, 
				1.0f, 
				std::clamp
				(
					distToTargetPos / (Settings::fBaseGrabbedRefrMaxSpeed * 0.5f), 
					0.0f, 
					1.0f
				), 
				2.0f
			);

			// Don't move at all when too close.
			auto velocity = posDelta.Length() > 1.0f ? dir * fmin(heavinessFactor * posDelta.Length() * slowdownFactor, grabbedRefrMaxSpeed) : RE::NiPoint3();
			velocity *= GAME_TO_HAVOK;

			// Activate the refr and set the computed velocity.
			Util::NativeFunctions::hkpEntity_Activate(hkpRigidBody.get());
			hkpRigidBody->motion.SetLinearVelocity({ velocity.x, velocity.y, velocity.z, 0.0f });

			// Adjust the grabbed object's rotation if performing the requisite action.
			// [Rotation controls]:
			// 1. Move the right stick up and down to rotate along the horizontal axis 
			// facing right relative to the player, 
			// 2. Move the right stick left and right to rotate along the vertical axis facing upward.

			const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
			bool shouldAdjustRotation = a_p->pam->IsPerforming(InputAction::kGrabRotateYZ);
			if (shouldAdjustRotation)
			{
				const auto& headingAngle = a_p->coopActor->GetHeading(false);
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[0] = 
				(
					-Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * cosf(headingAngle)
				);
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[1] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * sinf(headingAngle)
				);
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[2] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.xComp
				);
			}
			else
			{
				// Zero out angular velocity when not rotating.
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[0] = 0.0f;
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[1] = 0.0f;
				hkpRigidBody->motion.angularVelocity.quad.m128_f32[2] = 0.0f;
			}
		}
	}

	RE::NiPoint3 TargetingManager::ReleasedReferenceInfo::CalculatePredInterceptPos(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Calculate the position at which the released refr is likely to collide
		// with the target once the refr is released. Use the target's physical motion
		// data to perform this calculation.
		// TODO: Refactor to handle thrown refrs and projectiles in the same function.
		
		// Has the actor been targeted long enough to get an initial reading
		// for position, velocity, and acceleration?
		// If not, there is not enough information to predict
		// where to fire the projectile to hit the target,
		// so set the predicted intercept position as the current aimed-at position.
		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		if (!targetActorPtr || !a_p->tm->targetMotionState->IsFullyPopulated())
		{
			return a_p->tm->crosshairWorldPos;
		}
		else
		{
			// Aim at crosshair position/closest node position if an actor is selected 
			// by the crosshair or if facing the crosshair target position.
			// Otherwise, aim at the aim correction target's torso.
			const auto currentAimedAtPos = 
			(
				Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle) || a_p->mm->shouldFaceTarget ? 
				trajectoryEndPos : 
				Util::GetTorsoPosition(targetActorPtr.get()));
			// Invert pitch convention for use with trig functions.
			const double& aimPitch = -a_p->mm->aimPitch;
			// Max number of iterations, current number of iterations.
			uint8_t steps = 50;
			uint8_t step = 0;
			// Set the initial predicted intercept/hit position to the current aimed-at position.
			RE::NiPoint3 predHitPos = currentAimedAtPos;

			// Average per interval, current, next from target motion data.
			RE::NiPoint3 apiPredTargetAccel = a_p->tm->targetMotionState->apiAccel;
			RE::NiPoint3 apiPredTargetVel = a_p->tm->targetMotionState->apiVel;
			RE::NiPoint3 cPredTargetAccel = a_p->tm->targetMotionState->cAccelPerFrame;
			RE::NiPoint3 cPredTargetVel = a_p->tm->targetMotionState->cVel;
			RE::NiPoint3 nPredTargetAccel = a_p->tm->targetMotionState->cAccelPerFrame;
			RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
			RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
			// Flight time deltas at which to bail out of the calculation loop.
			double timeBailDeltaMin = 1E-4;
			double timeBailDeltaMax = 1000.0;
			// XY and Z offsets to the predicted position from the release position.
			double xy = Util::GetXYDistance(predHitPos, releasePos);
			double z = (predHitPos - releasePos).z;
			// Initial release speed.
			double firstReleaseSpeed = releaseSpeed;
			// Time to target.
			double t = xy / releaseSpeed * cosf(aimPitch);
			// Previously calculated time to target.
			double tPrev = 0.0;
			// Difference in the calculated times to target.
			double tDiff = fabsf(t - tPrev);
			// Current delta yaw and yaw rotation speed.
			const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
			const float currentZRotSpeed = 
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ? 
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z : 
				0.0f
			);
			// Average of current and average per interval yaw deltas.
			float avgYawDeltaPerFrame = (currentYawAngDelta / (*g_deltaTimeRealTime) + a_p->tm->targetMotionState->apiYawAngDelta / (*g_deltaTimeRealTime)) / 2.0f;
			// Average of current and average per interval change in speed.
			const float avgSpeedDelta = (a_p->tm->targetMotionState->apiSpeedDelta + a_p->tm->targetMotionState->cSpeedDeltaPerFrame) / 2.0f;
			// Attempt to accurately estimate the target intercept position
			// and continue looping until the reported time-to-target values converge
			// to below the minimum time diff (success(, or diverge above the maximum time diff (failure),
			// or until the maximum number of iterations is reached.
			while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
			{
				// SUPER NOTE: Everything below is obviously not mathematically correct,
				// since the target's velocity and acceleration are changing constantly,
				// which means that finding the best predicted hit position
				// would require integration over the time of flight.
				// However the recorded acceleration and velocity motion data
				// for targets is very noisy, which leads to huge overshoots
				// when using the proper formulas for calculating the predicted position at time t.
				// This temporary, manually-tested solution performs slightly better than not accounting for acceleration at all.

				// Rotate predicted velocity vector by the yaw diff which will occur over the time delta.
				double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
				double speed = nPredTargetVel.Length();
				Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
				nPredTargetVel.Unitize();
				// Update predicted speed based on average-per-interval speed delta.
				nPredTargetVel *= speed + a_p->tm->targetMotionState->apiSpeedDelta * tDiff;
				// Offset the current aimed at position by the delta position calculated
				// using the average of three velocity measurements over the new time to target.
				predHitPos = currentAimedAtPos + ((apiPredTargetVel + cPredTargetVel + nPredTargetVel) / 3.0f) * t;

				/*
				// Haven't properly implemented the method below yet, so it does not work as well as the above approximation.
				// Runge-Kutta method.
				// Sources:
				// https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods#The_Runge%E2%80%93Kutta_method
				// https://gafferongames.com/post/integration_basics/
				// https://scicomp.stackexchange.com/questions/19020/applying-the-runge-kutta-method-to-second-order-odes
				enum
				{
					kPos,
					kVel,
					kAcc
				};

				uint32_t iSteps = 1000;
				uint32_t iStep = 0;
				double dt = tDiff / iSteps;
				std::valarray<RE::NiPoint3> pva{ RE::NiPoint3(), 3 };
				pva[kPos] = predHitPos;
				pva[kVel] = nPredTargetVel;
				pva[kAcc] = nPredTargetAccel;

				std::valarray<RE::NiPoint3> dpva{ RE::NiPoint3(), 3 };
				dpva[kPos] = nPredTargetVel;
				dpva[kVel] = nPredTargetAccel;
				dpva[kAcc] = RE::NiPoint3();

				auto getDeriv =
					[&](const double& dt, const std::valarray<RE::NiPoint3>& cpva, const std::valarray<RE::NiPoint3>& dcpva) {
						std::valarray<RE::NiPoint3> pvaNew{ RE::NiPoint3(), 3 };
						pvaNew[kAcc] = cpva[kAcc] + dcpva[kAcc] * dt;
						pvaNew[kVel] = cpva[kVel] + dcpva[kVel] * dt;
						pvaNew[kPos] = cpva[kPos] + dcpva[kPos] * dt;


						std::valarray<RE::NiPoint3> dpvaNew{ RE::NiPoint3(), 3 };
						dpvaNew[kAcc] = RE::NiPoint3();
						dpvaNew[kVel] = pvaNew[kAcc];
						dpvaNew[kPos] = pvaNew[kVel];
						return dpvaNew;
					};

				auto temp = std::valarray<RE::NiPoint3>{ RE::NiPoint3(), 3 };
				while (iStep < iSteps)
				{
					auto dpvaA = getDeriv(0.0f, pva, dpva);
					temp[kPos] = dpva[kPos] + dpvaA[kPos] * 0.5f * dt;
					temp[kVel] = dpva[kVel] + dpvaA[kVel] * 0.5f * dt;
					temp[kAcc] = dpva[kAcc] + dpvaA[kAcc] * 0.5f * dt;
					auto dpvaB = getDeriv(0.5f * dt, pva, temp);
					temp[kPos] = dpva[kPos] + dpvaB[kPos] * 0.5f * dt;
					temp[kVel] = dpva[kVel] + dpvaB[kVel] * 0.5f * dt;
					temp[kAcc] = dpva[kAcc] + dpvaB[kAcc] * 0.5f * dt;
					auto dpvaC = getDeriv(0.5f * dt, pva, temp);
					temp[kPos] = dpva[kPos] + dpvaC[kPos] * dt;
					temp[kVel] = dpva[kVel] + dpvaC[kVel] * dt;
					temp[kAcc] = dpva[kAcc] + dpvaC[kAcc] * dt;
					auto dpvaD = getDeriv(dt, pva, temp);

					RE::NiPoint3 dpdt = (dpvaA[kPos] + ((dpvaB[kPos] + dpvaC[kPos]) * 2.0) + dpvaD[kPos]) * (1.0 / 6.0);
					RE::NiPoint3 dvdt = (dpvaA[kVel] + ((dpvaB[kVel] + dpvaC[kVel]) * 2.0) + dpvaD[kVel]) * (1.0 / 6.0);
					RE::NiPoint3 dadt = (dpvaA[kAcc] + ((dpvaB[kAcc] + dpvaC[kAcc]) * 2.0) + dpvaD[kAcc]) * (1.0 / 6.0);

					pva[kPos] += dpdt * dt;
					pva[kVel] += dvdt * dt;
					pva[kAcc] += dadt * dt;
					dpva[kPos] += dvdt * dt;
					dpva[kVel] += dadt * dt;
					dpva[kAcc] = RE::NiPoint3();
					++iStep;
				}
				
				predHitPos = pva[kPos];
				nPredTargetVel = pva[kVel];
				nPredTargetAccel = pva[kAcc];
				*/

				// Update positional offsets based on the new predicated hit position.
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
				// Failure.
				// Set to initial release speed.
				releaseSpeed = firstReleaseSpeed;

				// REMOVE when done debugging.
				/*ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Exceeded MAX t precision ({} > {} in {} steps)! Cannot hit {}, so setting target position to current aimed at pos. Release speed: {}",
					a_p->coopActor->GetName(), tDiff, timeBailDeltaMax, step, targetActorPtr->GetName(), releaseSpeed);*/

				// Failed to find intercept position, so set to the initially-aimed-at position.
				return currentAimedAtPos;
			}
			else
			{
				// REMOVE when done debugging.
				/*if (tDiff <= timeBailDeltaMin)
				{
					ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Met MIN t precision in {} steps: {}!", 
						a_p->coopActor->GetName(), step, tDiff);
				}
				else
				{
					ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Did NOT meet MIN t precision! tDiff: {} in {} steps", 
						a_p->coopActor->GetName(), tDiff, step);
				}*/

				// Either converged on a particular intercept position, 
				// with the change in time to target under the lower bail precision (success),
				// or didn't quite meet that required precision (failed).
				return predHitPos;
			}
		}
	}

	void TargetingManager::ReleasedReferenceInfo::PerformInitialHomingCheck(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Check if the released refr should start homing in on the target, 
		// and if so set the appropriate flags.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		if (!releaseTP.has_value())
		{
			Clear();
			return;
		}

		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		// Get target position, and pitch/yaw to target.
		RE::NiPoint3 aimTargetPos = 
		{
			targetActorPtr ?
			targetActorPtr->data.location + targetLocalPosOffset :
			a_p->tm->crosshairWorldPos
		};
		float pitchToTarget = Util::GetPitchBetweenPositions(objectPtr->data.location, aimTargetPos);
		float yawToTarget = Util::GetYawBetweenPositions(objectPtr->data.location, aimTargetPos);

		// Get gravitational constant.
		float g = Settings::fG * HAVOK_TO_GAME;
		if (glob.player1Actor->parentCell)
		{
			auto bhkWorld = glob.player1Actor->parentCell->GetbhkWorld();
			if (bhkWorld && bhkWorld->GetWorld1())
			{
				g = -bhkWorld->GetWorld1()->gravity.quad.m128_f32[2] * HAVOK_TO_GAME;
			}
		}

		float t = Util::GetElapsedSeconds(releaseTP.value());
		const float xy = Util::GetXYDistance(objectPtr->data.location, releasePos);
		// Pitch of the object along its trajectory.
		const float pitchOnTraj = -atan2f(tanf(launchPitch) - (g * xy) / powf(releaseSpeed * cosf(launchPitch), 2.0f), 1.0f);

		// Check if homing projectile should fully start homing in on the target 
		// instead of following its initial fixed trajectory.
		// Either:
		// 1. At the apex of the trajectory (exactly halfway through time of flight).
		// 2. Past half the time of flight.
		// 3. The target is above the released refr as it is on its way to the apex.
		// 4. The target is behind the released refr as it is on its way down from the apex.
		bool atApex = pitchOnTraj == 0.0f;
		bool moveUpwardOffTraj = (pitchOnTraj < 0.0f && pitchToTarget < pitchOnTraj);
		bool moveDownwardOffTraj = (pitchOnTraj > 0.0f && pitchToTarget > pitchOnTraj);
		bool passedHalfTimeOfFlight = t > 0.5f * initialTimeToTarget;
		// Set homing flags if not already homing.
		if (!startedHomingIn && (passedHalfTimeOfFlight || atApex || moveUpwardOffTraj || moveDownwardOffTraj))
		{
			// Used to check if the projectile should switch to homing mode.
			startedHomingIn = isHoming = true;
		}
	}

	void TargetingManager::ReleasedReferenceInfo::SetHomingTrajectory(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Home in the released refr on the target position.
		
		// Must be signalled to start homing already.
		if (!startedHomingIn || !isHoming)
		{
			return;
		}

		if (!IsValid()) 
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		if (!releaseTP.has_value())
		{
			Clear();
			return;
		}

		// Get the released refr's current velocity.
		RE::NiPoint3 currentVel;
		if (objectPtr->Is(RE::FormType::ActorCharacter)) 
		{
			RE::hkVector4 vel{ 0.0f };
			if (auto charController = objectPtr->As<RE::Actor>()->GetCharController(); charController) 
			{
				charController->GetLinearVelocityImpl(vel);
				currentVel = ToNiPoint3(vel) * HAVOK_TO_GAME;
			}
		}
		else
		{
			objectPtr->GetLinearVelocity(currentVel);
		}

		// Set the target position.
		RE::NiPoint3 aimTargetPos = a_p->tm->crosshairWorldPos;
		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		bool targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
		if (targetActorValidity) 
		{
			// Get targeted node, if available.
			// Otherwise, aim at the position offset from the actor's refr location.
			if (Settings::vbUseAimCorrection[a_p->playerID] && targetedActorNode && targetedActorNode.get()) 
			{
				aimTargetPos = targetedActorNode->world.translate;
				if (auto hkpRigidBody = Util::GethkpRigidBody(targetedActorNode.get()); hkpRigidBody)
				{
					aimTargetPos = ToNiPoint3(hkpRigidBody->motion.motionState.sweptTransform.centerOfMass0) * HAVOK_TO_GAME;
				}
			}
			else
			{
				aimTargetPos = targetActorPtr->data.location + targetLocalPosOffset;
			}
		}

		// Stop homing when close enough to the target position (within a frame of reaching).
		bool closeToTarget = objectPtr->data.location.GetDistance(aimTargetPos) <= currentVel.Length() * *g_deltaTimeRealTime;
		if (objectPtr->Is(RE::FormType::ActorCharacter)) 
		{
			// Released refr is an actor.
			// Continue homing if there hasn't been a recorded collision 
			// and if not too close to the target position.
			isHoming = hitRefrFIDs.empty() && !closeToTarget;
		}
		else
		{
			if (targetActorPtr)
			{
				// Continue homing if the released refr has not hit the target actor
				// and if not too close to the target position.
				isHoming = !hitRefrFIDs.contains(targetActorPtr->formID) && !closeToTarget;
			}
			else
			{
				// Continue homing if not too close to the target position.
				isHoming = !closeToTarget;
			}
		}

		// Pitch and yaw to the target position.
		float pitchToTarget = Util::GetPitchBetweenPositions(objectPtr->data.location, aimTargetPos);
		float yawToTarget = Util::GetYawBetweenPositions(objectPtr->data.location, aimTargetPos);
		if (isHoming)
		{
			float currentVelPitch = Util::GetPitchBetweenPositions(RE::NiPoint3(), currentVel);
			// Sometimes invalid at launch time for some reason (-nan).
			float currentVelYaw = Util::GetYawBetweenPositions(RE::NiPoint3(), currentVel);

			// Gravitational constant.
			float g = Settings::fG * HAVOK_TO_GAME;
			if (glob.player1Actor->parentCell)
			{
				auto bhkWorld = glob.player1Actor->parentCell->GetbhkWorld();
				if (bhkWorld && bhkWorld->GetWorld1())
				{
					g = -bhkWorld->GetWorld1()->gravity.quad.m128_f32[2] * HAVOK_TO_GAME;
				}
			}

			const float t = Util::GetElapsedSeconds(releaseTP.value());
			float pitchToSet = currentVelPitch;
			float yawToSet = currentVelYaw;
			// Just launched or invalid, so set pitch and yaw to saved launch values.
			if (t == 0.0f || isnan(currentVelPitch) || isinf(currentVelPitch))
			{
				currentVelPitch = -launchPitch;
			}

			if (t == 0.0f || isnan(currentVelYaw) || isinf(currentVelYaw))
			{
				currentVelYaw = Util::ConvertAngle(launchYaw);
			}

			// Lerp yaw to slowly rotate the projectile to face the target in the XY plane.
			// Fully face the target in the XY plane once half of the projectile's initial time 
			// to the target position has elapsed.
			// Velocity on fixed trajectory determined by projectile launch data.
			if (initialTimeToTarget == 0.0 || initialTimeToTarget >= Settings::fMaxProjAirborneSecsToTarget)
			{
				// Can't hit target with given launch pitch, so set yaw directly to target right away.
				yawToSet = yawToTarget;
			}
			else
			{
				// Slowly turn to face.
				float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentVelYaw);
				yawToSet = Util::NormalizeAng0To2Pi(currentVelYaw + Util::InterpolateSmootherStep(0.0f, yawDiff, min(1.0f, t / (0.5f * initialTimeToTarget))));
			}

			// Adjust pitch to face target.
			pitchToSet = pitchToTarget;

			// Set speed to the corresponding speed along the fixed trajectory.
			const float velXY = releaseSpeed * cosf(launchPitch);
			const float velX = velXY * cosf(launchYaw);
			const float velY = velXY * sinf(launchYaw);
			const float velZ = releaseSpeed * sinf(launchPitch) - g * t;
			auto fixedTrajVel = RE::NiPoint3(velX, velY, velZ);
			float speed = fixedTrajVel.Length();

			// Set velocity.
			RE::NiPoint3 velToSet = Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) * speed;
			if (auto hkpRigidBody = Util::GethkpRigidBody(objectPtr.get()); hkpRigidBody)
			{
				// Zero out inertia and damping before throwing
				// and then revert inertia changes after launch.
				// Warning: hacky. Since I can't find a way of accounting for different
				// objects' air resistances, set air resistance to 0 so that the
				// thrown object will land directly at the target position
				// when launched with our previously computed launch velocity.
				hkpRigidBody->motion.motionState.angularDamping = 0.0f;
				hkpRigidBody->motion.motionState.linearDamping = 0.0f;
				RE::hkVector4 savedInertia = hkpRigidBody->motion.inertiaAndMassInv;
				RE::hkVector4 zeroedInertia = RE::hkVector4(0.0f, 0.0f, 0.0f, hkpRigidBody->motion.inertiaAndMassInv.quad.m128_f32[3]);
				hkpRigidBody->motion.inertiaAndMassInv = zeroedInertia;
				hkpRigidBody->motion.SetLinearVelocity(velToSet * GAME_TO_HAVOK);
				hkpRigidBody->motion.inertiaAndMassInv = savedInertia;
			}
		}
	}

	void TargetingManager::ReleasedReferenceInfo::SetReleaseTrajectoryInfo(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Set release trajectory info for this released refr.
		// Determines the released refr's initial orientation, speed, time to target,
		// targeted trajectory end position, and whether or not the released refr was thrown or dropped.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		collisionActive = true;
		startedHomingIn = false;
		targetActorHandle = a_p->tm->targetMotionState->targetActorHandle;
		targetLocalPosOffset = Util::HandleIsValid(targetActorHandle) ? a_p->tm->crosshairLastHitLocalPos : RE::NiPoint3();
		targetedActorNode.reset();
		trajectoryEndPos = a_p->tm->crosshairWorldPos;

		// Set closest node and trajectory end position to that node's position
		// if aiming at an actor with aim correction.
		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		bool targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
		if (targetActorValidity && Settings::vbUseAimCorrection[a_p->playerID] && targetActorHandle == a_p->tm->crosshairRefrHandle)
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

						if (auto node3D = RE::NiPointer<RE::NiAVObject>(a_node); node3D)
						{
							// Must have havok rigid body to collide with.
							if (auto hkpRigidBody = Util::GethkpRigidBody(node3D.get()); hkpRigidBody)
							{
								// Check distance from the current trajectory end position (crosshair position)
								// and the node's center of mass.
								const auto centerOfMass = ToNiPoint3(hkpRigidBody->motion.motionState.sweptTransform.centerOfMass0) * HAVOK_TO_GAME;
								if (float dist = centerOfMass.GetDistance(trajectoryEndPos); dist < closestNodeDist)
								{
									// New closest node to the crosshair position.
									if (auto collidable = hkpRigidBody->GetCollidable(); collidable)
									{
										closestNodeDist = dist;
										targetedActorNode = RE::NiPointer<RE::NiAVObject>(a_node);
										nodeCenterOfMassPos = centerOfMass;
									}
								}
							}
						}
					}
				);

				// Set new trajectory end position to the node's center of mass.
				trajectoryEndPos = nodeCenterOfMassPos;
			}
		}

		// Released from suspended position.
		releasePos = objectPtr->data.location;
		if (auto hkpRigidBody = Util::GethkpRigidBody(objectPtr.get()); hkpRigidBody)
		{
			// Zero out velocity first.
			releaseVelocity = RE::NiPoint3();

			// Throw the refr if it is not the player themselves, 
			// and if facing the crosshair position.
			// Drop the refr otherwise.
			if (a_p->mm->shouldFaceTarget && objectPtr != a_p->coopActor)
			{
				// Throw refr telekinetically.
				hkpRigidBody->motion.SetPosition(releasePos * GAME_TO_HAVOK);
				hkpRigidBody->motion.SetLinearVelocity({ 0 });
				hkpRigidBody->motion.SetAngularVelocity({ 0 });

				// Projectile motion equation used to get max and min launch angles at max launch speed,
				// and launch angle with minimized launch speed:
				// https://en.wikipedia.org/wiki/Projectile_motion#Angle_%CE%B8_required_to_hit_coordinate_(x,_y)

				// Get grab bind hold time, which directly influences 
				// the speed at which the refr is thrown.
				float cappedHoldTime = 
				(
					min
					(
						a_p->pam->paStatesList[!InputAction::kGrabObject - !InputAction::kFirstAction].secsPerformed,
						max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed)
					)
				);
				// Normalize it.
				float normHoldTime = std::lerp(0.0f, 1.0f, cappedHoldTime / max(0.01f, Settings::fGrabHoldSecsToMaxReleaseSpeed));

				// Carryweight scales up the launch velocity.
				float carryweightRatio = 
				(
					a_p->coopActor->GetActorValue(RE::ActorValue::kCarryWeight) / 
					a_p->coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight)
				);
				if (a_p->coopActor->GetRace() && a_p->coopActor->GetRace()->data.baseCarryWeight != 0.0f) 
				{
					carryweightRatio = a_p->coopActor->GetActorValue(RE::ActorValue::kCarryWeight) / a_p->coopActor->GetRace()->data.baseCarryWeight;
				}

				float carryWeightFactor = max(0.0f, logf(carryweightRatio) + 1.0f);

				// g is taken to be positive for all calculations below.
				float g = 9.81f;
				if (glob.player1Actor->parentCell)
				{
					auto bhkWorld = glob.player1Actor->parentCell->GetbhkWorld();
					if (bhkWorld && bhkWorld->GetWorld1())
					{
						g = -bhkWorld->GetWorld1()->gravity.quad.m128_f32[2] * HAVOK_TO_GAME;
					}
				}

				// Additional release speed multiplier if moving.
				bool releaseSpeedMult = glob.cdh->GetInputState(a_p->controllerID, InputAction::kLS).isPressed ? 1.5f : 1.0f;
				auto dirToTarget = trajectoryEndPos - releasePos;
				dirToTarget.Unitize();
				// Angle straight at target.
				launchYaw = Util::NormalizeAng0To2Pi(atan2f(dirToTarget.y, dirToTarget.x));
				// NOTE: Calcs do not account for air drag.
				// When the actor is aiming at a target, holding the grab bind
				// modifies the launch angle (flatter trajectory if held longer).
				const float xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
				const float z = trajectoryEndPos.z - releasePos.z;
				// Velocity modified by carryweight, release multipliers on top of the base release speed.
				// Cap at max refr release speed.
				const float v = Util::InterpolateSmootherStep
				(
					Settings::fBaseMaxThrownRefrReleaseSpeed, 
					min(Settings::fAbsoluteMaxThrownRefrReleaseSpeed, carryWeightFactor * releaseSpeedMult * Settings::fBaseMaxThrownRefrReleaseSpeed), 
					normHoldTime
				);  
				// Bounds for launch pitch.
				float steepestLaunchAng, flattestLaunchAng = 0.0f;

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
				}
				else
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

				// New squared velocity based on the new launch pitch.
				float root = (g * xy * xy) / (2.0f * cosf(launchPitch) * cosf(launchPitch) * (xy * tanf(launchPitch) - z));
				releaseSpeed = withinRange ? sqrtf(root) : min(sqrtf(root), v);

				// Components of velocity.
				float velX = releaseSpeed * cosf(launchYaw) * cosf(launchPitch);
				float velY = releaseSpeed * sinf(launchYaw) * cosf(launchPitch);
				float velZ = releaseSpeed * sinf(launchPitch);

				// Once release speed is set, update the intercept position, if using aim prediction.
				if (trajType == ProjectileTrajType::kPrediction)
				{
					trajectoryEndPos = CalculatePredInterceptPos(a_p);
				}

				// XY velocity remains constant throughout since air resistance 
				// is removed before setting the object's velocity below.
				const float velXY = releaseSpeed * cosf(launchPitch);
				// Set the time to reach the target position.
				initialTimeToTarget = 
				(
					velXY == 0.0f || !withinRange ? 
					Settings::fMaxProjAirborneSecsToTarget : 
					Util::GetXYDistance(releasePos, trajectoryEndPos) / velXY
				);

				// Don't know how to compensate for actors' air drag, 
				// so I'm scaling up launch velocity here based on manual testing in game.
				// Won't be perfectly accurate obviously.
				float actorFactor = 1.0f;
				if (objectPtr->Is(RE::FormType::ActorCharacter))
				{
					if (ALYSLC::EnderalCompat::g_enderalSSEInstalled)
					{
						actorFactor *= 3.0f;
					}
					else
					{
						actorFactor *= 15.0f;
					}
				}

				releaseVelocity = RE::NiPoint3(velX, velY, velZ) * GAME_TO_HAVOK * actorFactor;
				// Zero out inertia and dampig before throwing
				// and then revert inertia changes after launch.
				// Warning: hacky. Since I can't find a way of accounting for different
				// objects' air resistances, set air resistance to 0 so that the
				// thrown object will land directly at the target position
				// when launched with our previously computed launch velocity.
				hkpRigidBody->motion.motionState.angularDamping = 0.0f;
				hkpRigidBody->motion.motionState.linearDamping = 0.0f;
				RE::hkVector4 savedInertia = hkpRigidBody->motion.inertiaAndMassInv;
				RE::hkVector4 zeroedInertia = RE::hkVector4(0.0f, 0.0f, 0.0f, hkpRigidBody->motion.inertiaAndMassInv.quad.m128_f32[3]);
				hkpRigidBody->motion.inertiaAndMassInv = zeroedInertia;
				hkpRigidBody->motion.SetLinearVelocity(releaseVelocity);
				hkpRigidBody->motion.inertiaAndMassInv = savedInertia;

				// Set as thrown.
				isThrown = true;
			}
			else
			{
				// Drop object.
				// Zero out inertia and damping before dropping
				// and then revert inertia changes after launch.
				hkpRigidBody->motion.motionState.angularDamping = 0.0f;
				hkpRigidBody->motion.motionState.linearDamping = 0.0f;
				RE::hkVector4 savedInertia = hkpRigidBody->motion.inertiaAndMassInv;
				RE::hkVector4 zeroedInertia = RE::hkVector4(0.0f, 0.0f, 0.0f, hkpRigidBody->motion.inertiaAndMassInv.quad.m128_f32[3]);
				hkpRigidBody->motion.inertiaAndMassInv = zeroedInertia;
				hkpRigidBody->motion.SetLinearVelocity(releaseVelocity);
				hkpRigidBody->motion.inertiaAndMassInv = savedInertia;

				// Not thrown.
				isThrown = false;
			}

			// Released now.
			releaseTP = SteadyClock::now();
		}
	}

	void TargetingManager::RefrManipulationManager::AddGrabbedRefr(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle)
	{
		// Add the given refr to the managed grabbed refrs data set,
		// set its grab time point, and ragdoll the refr, if it is an actor,
		// to allow for positional manipulation.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr)
		{
			return;
		}

		// Must have space for another grabbed refr.
		auto nextOpenIndex = grabbedRefrInfoList.size();
		if (nextOpenIndex < Settings::uMaxGrabbedReferences && !grabbedRefrHandlesToInfoIndices.contains(a_handle))
		{
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
							a_p->coopActor->GetHeight() * std::lerp(-0.5f, 0.5f, (a_p->mm->aimPitch + PI / 2.0f) / PI)
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
		}
	}

	void TargetingManager::RefrManipulationManager::AddReleasedRefr(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle) 
	{
		// Add the given refr to the managed released refrs data set.
		// Also set its release trajectory information (dropped or thrown)
		// to use when guiding its motion upon release.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr)
		{
			return;
		}

		if (!releasedRefrHandlesToInfoIndices.contains(a_handle))
		{
			auto nextOpenIndex = releasedRefrInfoList.size();
			// Store mapped index and then add to list.
			releasedRefrHandlesToInfoIndices.insert_or_assign(a_handle, nextOpenIndex);
			releasedRefrInfoList.emplace_back(std::make_unique<ReleasedReferenceInfo>(a_p->controllerID, a_handle));
			const auto& info = releasedRefrInfoList[nextOpenIndex];
			// Set initial homing/aim prediction trajectory info.
			info->SetReleaseTrajectoryInfo(a_p);
		}
	}

	bool TargetingManager::RefrManipulationManager::CanGrabAnotherRefr()
	{
		// Return true if the number of managed grabbed refrs 
		// is less than the maximum allowable number of grabbed refrs.

		return grabbedRefrInfoList.size() < Settings::uMaxGrabbedReferences;
	}

	bool TargetingManager::RefrManipulationManager::CanGrabRefr(const RE::ObjectRefHandle& a_handle)
	{
		// Return true if the given refr is valid, not already managed,
		// and there's room for another grabbed refr.

		return Util::HandleIsValid(a_handle) && !IsManaged(a_handle, true) && CanGrabAnotherRefr();
	}

	bool TargetingManager::RefrManipulationManager::CanManipulateGrabbedRefr(const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index)
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
		if (asActor && !asActor->IsInRagdollState())
		{
			if (GlobalCoopData::IsCoopPlayer(asActor)) 
			{
				// Player is not grabbed anymore.
				info->Clear();
				a_p->mm->aimPitch = 0.0f;
				asActor->PotentiallyFixRagdollState();
				return false;
			}
			else
			{
				if (Settings::bRemoveGrabbedActorAutoGetUp && asActor->currentProcess && asActor->currentProcess->middleHigh)
				{
					// Prevent NPC from getting up initially,
					// if the actor getup removal setting is enabled.
					// Knock 'em down again first.
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
		
		const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
		{
			std::unique_lock<std::mutex> lock(contactEventsQueueMutex, std::try_to_lock);
			if (lock)
			{
				ALYSLC::Log("[TM] RefrManipulationManager: ClearAll: Lock obtained. (0x{:X})", hash);
				collidedRefrFIDPairs.clear();
				queuedReleasedRefrContactEvents.clear();
			}
			else
			{
				ALYSLC::Log("[TM] RefrManipulationManager: ClearAll: Failed to obtain lock. (0x{:X})", hash);
			}
		}

		grabbedRefrHandlesToInfoIndices.clear();
		ClearGrabbedRefrs();
		releasedRefrHandlesToInfoIndices.clear();
		ClearReleasedRefrs();
		// No longer grabbing.
		isAutoGrabbing = isGrabbing = false;
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedRefr(const RE::ObjectRefHandle& a_handle)
	{
		// Clear the given refr from the grabbed list.
		// Refresh handle-to-index mappings if the refr was cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList,
			[&a_handle](const std::unique_ptr<GrabbedReferenceInfo>& a_info) {
				return a_info->refrHandle == a_handle;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(true);
		}

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr)
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
			const auto& handle = info->refrHandle;
			if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr)
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
		// Clear all managed released refrs that are no longer monitored for collisions.
		// Refresh handle-to-index mappings if at least one released refr was cleared.

		auto numErased = std::erase_if
		(
			releasedRefrInfoList, 
			[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info)
			{
				if (!a_info->collisionActive) 
				{
					const auto& handle = a_info->refrHandle;
					if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr)
					{
						// Ensure actors are no longer paralyzed.
						if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
						{
							asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
						}
					}
				}

				return !a_info->collisionActive;
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
					return !a_info->IsValid();
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

	void TargetingManager::RefrManipulationManager::ClearRefr(const RE::ObjectRefHandle& a_handle)
	{
		// Clear the given refr from the grabbed and/or released lists.
		// Refresh handle-to-index mappings if the refr was cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList, 
			[&a_handle](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{ 
				return a_info->refrHandle == a_handle;
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

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr)
		{
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				// Ensure actors are no longer paralyzed.
				asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearReleasedRefr(const RE::ObjectRefHandle& a_handle)
	{
		// Clear the given refr from the released list.
		// Refresh handle-to-index mappings if the refr was cleared.
		
		size_t numErased = std::erase_if
		(
			releasedRefrInfoList,
			[&a_handle](const std::unique_ptr<ReleasedReferenceInfo>& a_info) {
				return a_info->refrHandle == a_handle;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(false);
		}

		if (auto refrPtr = Util::GetRefrPtrFromHandle(a_handle); refrPtr)
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
			if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr)
			{
				// Ensure all released actors are not paralyzed.
				if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
				{
					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}
		}

		releasedRefrInfoList.clear();
		releasedRefrHandlesToInfoIndices.clear();
	}

	void TargetingManager::RefrManipulationManager::HandleQueuedContactEvents(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Ragdoll and apply damage to any hit actors from the contact events queue.

		// No released refrs, so no contact events to handle.
		if (a_p->tm->rmm->releasedRefrInfoList.empty())
		{
			return;
		}

		{
			std::unique_lock<std::mutex> lock(a_p->tm->rmm->contactEventsQueueMutex, std::try_to_lock);
			if (lock)
			{
				// Must obtain the point of contact between two collidables,
				// then get their handles and the associated refrs.
				RE::NiPoint3 contactPoint;
				RE::TESObjectREFR* refrA = nullptr;
				RE::TESObjectREFR* refrB = nullptr;
				// Unmanaged refr that the other managed refr collided with/was hit by.
				RE::TESObjectREFR* collidedWithRefr = nullptr;
				RE::ObjectRefHandle handleA;
				RE::ObjectRefHandle handleB;
				// Movin' through the queue. Clearing out the queue too.
				while (!queuedReleasedRefrContactEvents.empty())
				{
					const auto& contactEvent = queuedReleasedRefrContactEvents.front();
					queuedReleasedRefrContactEvents.pop_front();

					auto collidableA = contactEvent.bodies[0]->GetCollidable();
					auto collidableB = contactEvent.bodies[1]->GetCollidable();

					if (contactEvent.contactPoint)
					{
						contactPoint = ToNiPoint3(contactEvent.contactPoint->position) * HAVOK_TO_GAME;
					}
					else
					{
						// No contact point, nothing to handle for this event.
						continue;
					}

					// Two valid colliding objects.
					if (collidableA && collidableB)
					{
						refrA = RE::TESHavokUtilities::FindCollidableRef(*collidableA);
						refrB = RE::TESHavokUtilities::FindCollidableRef(*collidableB);
						// Must have two valid associated refrs.
						if (refrA && refrB)
						{
							handleA = refrA->GetHandle();
							handleB = refrB->GetHandle();

							if (Util::HandleIsValid(handleA) && Util::HandleIsValid(handleB))
							{
								// Check for instances where one of the two colliding refrs is managed and the other is not.
								// Want to ignore collisions between non-managed refrs and between two managed refrs.
								int32_t collidingReleasedRefrIndex = -1;
								if (a_p->tm->rmm->releasedRefrHandlesToInfoIndices.contains(handleA) && 
									!a_p->tm->rmm->releasedRefrHandlesToInfoIndices.contains(handleB))
								{
									collidedWithRefr = refrB;
									collidingReleasedRefrIndex = a_p->tm->rmm->releasedRefrHandlesToInfoIndices.at(handleA);
								}

								if (a_p->tm->rmm->releasedRefrHandlesToInfoIndices.contains(handleB) && 
									!a_p->tm->rmm->releasedRefrHandlesToInfoIndices.contains(handleA))
								{
									collidedWithRefr = refrA;
									collidingReleasedRefrIndex = a_p->tm->rmm->releasedRefrHandlesToInfoIndices.at(handleB);
								}

								// Why are you hitting yourself? Eh, whatever. Next!
								if (!collidedWithRefr || collidedWithRefr == a_p->coopActor.get())
								{
									continue;
								}

								// No index for the managed refr.
								if (collidingReleasedRefrIndex == -1)
								{
									continue;
								}

								auto& releasedRefrInfo = a_p->tm->rmm->releasedRefrInfoList[collidingReleasedRefrIndex];
								// Don't want repeated hits.
								bool hasAlreadyHitRefr = releasedRefrInfo->HasAlreadyHitRefr(collidedWithRefr);
								// Add the refr the managed refr collided with.
								releasedRefrInfo->AddHitRefr(collidedWithRefr);
								auto releasedRefrPtr = Util::GetRefrPtrFromHandle(releasedRefrInfo->refrHandle);

								// Managed refr hit a new actor that isn't itself. Bonk.
								if (auto hitActor = collidedWithRefr->As<RE::Actor>(); 
									hitActor && hitActor->currentProcess && releasedRefrPtr.get() != collidedWithRefr && !hasAlreadyHitRefr)
								{
									if (auto hkpRigidBody = Util::GethkpRigidBody(releasedRefrPtr.get()); hkpRigidBody) 
									{
										a_p->tm->HandleBonk(hitActor->GetHandle(), releasedRefrPtr->GetHandle(), hkpRigidBody.get(), contactPoint);
									}
								}

								// Thrown actor hit a new refr that isn't itself. Splat.
								if (auto thrownActor = releasedRefrPtr->As<RE::Actor>(); 
									thrownActor && thrownActor != collidedWithRefr && !hasAlreadyHitRefr && !releasedRefrInfo->hitRefrFIDs.empty())
								{
									a_p->tm->HandleSplat(thrownActor->GetHandle(), releasedRefrInfo->hitRefrFIDs.size());
								}
							}
						}
						else
						{
							// At least one refr is invalid.
							continue;
						}
					}
				}

				// No more events to handle.
				queuedReleasedRefrContactEvents.clear();
			}
		}
	}

	const bool TargetingManager::RefrManipulationManager::IsManaged(const RE::ObjectRefHandle& a_handle, bool a_grabbed)
	{
		// Check if the given refr is handled as either a grabbed or released refr, 
		// depending on the given grabbed param.

		if (a_grabbed)
		{
			return grabbedRefrHandlesToInfoIndices.contains(a_handle);
		}
		else
		{
			return releasedRefrHandlesToInfoIndices.contains(a_handle);
		}
	}

	void TargetingManager::RefrManipulationManager::RefreshHandleToIndexMappings(const bool& a_grabbed)
	{
		// Reconstruct the grabbed/released handle-to-index mappings
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
				releasedRefrHandlesToInfoIndices.insert({ releasedRefrInfoList[i]->refrHandle, i });
			}
		}
	}

	RE::NiPoint3 TargetingManager::ManagedProjTrajectoryInfo::CalculatePredInterceptPos(const std::shared_ptr<CoopPlayer>& a_p, const bool& a_adjustReleaseSpeed)
	{
		// Calculate the position at which the launched projectile is likely to collide
		// with the target. Use the target's physical motion data to perform this calculation.
		// Adjust the projectile's release speed, if requested,
		// to allow it to hit the predicted target position despite the effects of air drag.
		// TODO: Refactor to handle thrown refrs and projectiles in the same function.

		// Aim at target actor if valid and not mounted,
		// or if mounted and selected with the crosshair.
		// Want to avoid shooting at an aim correction target 
		// while mounted and targeting another object.
		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		bool aimAtActor = 
		{
			(targetActorPtr) && 
			(!a_p->coopActor->IsOnMount() || targetActorHandle == a_p->tm->selectedTargetActorHandle)
		};

		// Has the actor been targeted long enough to get initial readings
		// for position, velocity, and acceleration?
		// If not, there is not enough information to reasonably predict
		// where to fire the projectile to hit the target,
		// so set the predicted intercept position to the current aimed-at position.

		// Also, if using aim direction projectiles, or shooting at an inanimate object,
		// there's no need to account for target movement, 
		// so return the current aimed-at position too.
		if (trajType == ProjectileTrajType::kAimDirection || 
			!aimAtActor || 
			!a_p->tm->targetMotionState->IsFullyPopulated())
		{
			// Adjust the release speed to account for linear air drag before returning.
			if (a_adjustReleaseSpeed)
			{
				double xy = Util::GetXYDistance(trajectoryEndPos, releasePos);
				double z = (trajectoryEndPos - releasePos).z;
				releaseSpeed = GetReleaseSpeedToTarget(xy, z, -a_p->mm->aimPitch);
			}

			return trajectoryEndPos;
		}
		else
		{
			// Aim at crosshair position/closest node position 
			// if an actor is selected by the crosshair 
			// or if facing the crosshair target position.
			// Otherwise, aim at the aim correction target's torso.
			const auto currentAimedAtPos =
			(
				Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle) || a_p->mm->shouldFaceTarget ?
				trajectoryEndPos :
				Util::GetTorsoPosition(targetActorPtr.get())
			);
			// Invert pitch convention for use with trig functions.
			const double& aimPitch = -a_p->mm->aimPitch;
			// Max number of iterations, current number of iterations.
			uint8_t steps = 50;
			uint8_t step = 0;
			// Set the initial predicted intercept/hit position to the current aimed-at position.
			RE::NiPoint3 predHitPos = currentAimedAtPos;

			// Average per interval, current, next from target motion data.
			RE::NiPoint3 apiPredTargetAccel = a_p->tm->targetMotionState->apiAccel;
			RE::NiPoint3 apiPredTargetVel = a_p->tm->targetMotionState->apiVel;
			RE::NiPoint3 cPredTargetAccel = a_p->tm->targetMotionState->cAccelPerFrame;
			RE::NiPoint3 cPredTargetVel = a_p->tm->targetMotionState->cVel;
			RE::NiPoint3 nPredTargetAccel = a_p->tm->targetMotionState->cAccelPerFrame;
			RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
			RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
			// Flight time deltas at which to bail out of the calculation loop.
			double timeBailDeltaMin = 1E-4;
			double timeBailDeltaMax = 1000.0;
			// XY and Z offsets to the predicted position from the release position.
			double xy = Util::GetXYDistance(predHitPos, releasePos);
			double z = (currentAimedAtPos - releasePos).z;
			// Initial release speed, adjust as needed.
			double firstReleaseSpeed = releaseSpeed = 
			(
				a_adjustReleaseSpeed ? 
				GetReleaseSpeedToTarget(xy, z, aimPitch) : 
				releaseSpeed
			);
			// Time to target, accounting for air resistance.
			double t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(aimPitch)))) / mu;
			// Previously calculated time to target.
			double tPrev = 0.0;
			// Difference in the calculated times to target.
			double tDiff = fabsf(t - tPrev);
			// Current delta yaw and yaw rotation speed.
			const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
			const float currentZRotSpeed =
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ?
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z :
				0.0f
			);
			// Average of current and average per interval yaw deltas.
			float avgYawDeltaPerFrame = (currentYawAngDelta / (*g_deltaTimeRealTime) + a_p->tm->targetMotionState->apiYawAngDelta / (*g_deltaTimeRealTime)) / 2.0f;
			// Average of current and average per interval change in speed.
			const float avgSpeedDelta = (a_p->tm->targetMotionState->apiSpeedDelta + a_p->tm->targetMotionState->cSpeedDeltaPerFrame) / 2.0f;
			// Attempt to accurately estimate the target intercept position
			// and continue looping until the reported time-to-target values converge
			// to below the minimum time diff (success(, or diverge above the maximum time diff (failure),
			// or until the maximum number of iterations is reached.
			while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
			{
				// SUPER NOTE: Everything below is obviously not mathematically correct, 
				// since the target's velocity and acceleration are changing constantly,
				// which means that finding the best predicted hit position
				// would require integration over the time of flight.
				// However the recorded acceleration and velocity motion data
				// for targets is very noisy, which leads to huge overshoots
				// when using the proper formulas for calculating the predicted position at time t.
				// This temporary, manually-tested solution performs slightly better than not accounting for acceleration at all.

				// Rotate predicted velocity vector by the yaw diff which will occur over the time delta.
				double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
				double speed = nPredTargetVel.Length();
				Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
				nPredTargetVel.Unitize();
				// Update predicted speed based on average-per-interval speed delta.
				nPredTargetVel *= speed + a_p->tm->targetMotionState->apiSpeedDelta * tDiff;
				// Offset the current aimed at position by the delta position calculated
				// using the average of three velocity measurements over the new time to target.
				predHitPos = currentAimedAtPos + ((apiPredTargetVel + cPredTargetVel + nPredTargetVel) / 3.0f) * t;

				/*
				// Haven't properly implemented the method below yet, so it does not work as well as the above approximation in practice..
				// Runge-Kutta method.
				// Sources:
				// https://en.wikipedia.org/wiki/Runge%E2%80%93Kutta_methods#The_Runge%E2%80%93Kutta_method
				// https://gafferongames.com/post/integration_basics/
				// https://scicomp.stackexchange.com/questions/19020/applying-the-runge-kutta-method-to-second-order-odes
				enum
				{
					kPos,
					kVel,
					kAcc
				};

				uint32_t iSteps = 1000;
				uint32_t iStep = 0;
				double dt = tDiff / iSteps;
				std::valarray<RE::NiPoint3> pva{ RE::NiPoint3(), 3 };
				pva[kPos] = predHitPos;
				pva[kVel] = nPredTargetVel;
				pva[kAcc] = nPredTargetAccel;

				std::valarray<RE::NiPoint3> dpva{ RE::NiPoint3(), 3 };
				dpva[kPos] = nPredTargetVel;
				dpva[kVel] = nPredTargetAccel;
				dpva[kAcc] = RE::NiPoint3();

				auto getDeriv =
					[&](const double& dt, const std::valarray<RE::NiPoint3>& cpva, const std::valarray<RE::NiPoint3>& dcpva) {
						std::valarray<RE::NiPoint3> pvaNew{ RE::NiPoint3(), 3 };
						pvaNew[kAcc] = cpva[kAcc] + dcpva[kAcc] * dt;
						pvaNew[kVel] = cpva[kVel] + dcpva[kVel] * dt;
						pvaNew[kPos] = cpva[kPos] + dcpva[kPos] * dt;


						std::valarray<RE::NiPoint3> dpvaNew{ RE::NiPoint3(), 3 };
						dpvaNew[kAcc] = RE::NiPoint3();
						dpvaNew[kVel] = pvaNew[kAcc];
						dpvaNew[kPos] = pvaNew[kVel];
						return dpvaNew;
					};

				auto temp = std::valarray<RE::NiPoint3>{ RE::NiPoint3(), 3 };
				while (iStep < iSteps)
				{
					auto dpvaA = getDeriv(0.0f, pva, dpva);
					temp[kPos] = dpva[kPos] + dpvaA[kPos] * 0.5f * dt;
					temp[kVel] = dpva[kVel] + dpvaA[kVel] * 0.5f * dt;
					temp[kAcc] = dpva[kAcc] + dpvaA[kAcc] * 0.5f * dt;
					auto dpvaB = getDeriv(0.5f * dt, pva, temp);
					temp[kPos] = dpva[kPos] + dpvaB[kPos] * 0.5f * dt;
					temp[kVel] = dpva[kVel] + dpvaB[kVel] * 0.5f * dt;
					temp[kAcc] = dpva[kAcc] + dpvaB[kAcc] * 0.5f * dt;
					auto dpvaC = getDeriv(0.5f * dt, pva, temp);
					temp[kPos] = dpva[kPos] + dpvaC[kPos] * dt;
					temp[kVel] = dpva[kVel] + dpvaC[kVel] * dt;
					temp[kAcc] = dpva[kAcc] + dpvaC[kAcc] * dt;
					auto dpvaD = getDeriv(dt, pva, temp);

					RE::NiPoint3 dpdt = (dpvaA[kPos] + ((dpvaB[kPos] + dpvaC[kPos]) * 2.0) + dpvaD[kPos]) * (1.0 / 6.0);
					RE::NiPoint3 dvdt = (dpvaA[kVel] + ((dpvaB[kVel] + dpvaC[kVel]) * 2.0) + dpvaD[kVel]) * (1.0 / 6.0);
					RE::NiPoint3 dadt = (dpvaA[kAcc] + ((dpvaB[kAcc] + dpvaC[kAcc]) * 2.0) + dpvaD[kAcc]) * (1.0 / 6.0);

					pva[kPos] += dpdt * dt;
					pva[kVel] += dvdt * dt;
					pva[kAcc] += dadt * dt;
					dpva[kPos] += dvdt * dt;
					dpva[kVel] += dadt * dt;
					dpva[kAcc] = RE::NiPoint3();
					++iStep;
				}
				
				predHitPos = pva[kPos];
				nPredTargetVel = pva[kVel];
				nPredTargetAccel = pva[kAcc];
				*/

				// Update positional offsets based on the new predicated hit position.
				xy = Util::GetXYDistance(predHitPos - releasePos);
				z = (predHitPos - releasePos).z;
				// Adjust the release speed to account for air drag again.
				if (a_adjustReleaseSpeed)
				{
					releaseSpeed = GetReleaseSpeedToTarget(xy, z, aimPitch);
				}

				// Set previous time to target to current.
				tPrev = t;
				// Update current time to target using the new XY positional offset
				// and release speed.
				t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(aimPitch)))) / mu;
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

				// REMOVE when done debugging.
				/*ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Exceeded MAX t precision ({} > {} in {} steps)! Cannot hit {}, so setting target position to current aimed at pos. Release speed: {}",
					a_p->coopActor->GetName(), tDiff, timeBailDeltaMax, step, targetActorPtr->GetName(), releaseSpeed);*/

				// Failed to find intercept position, so set to the initially-aimed-at position.
				return currentAimedAtPos;
			}
			else
			{
				// REMOVE when done debugging.
				/*if (tDiff <= timeBailDeltaMin)
				{
					ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Met MIN t precision in {} steps: {}!", 
						a_p->coopActor->GetName(), step, tDiff);
				}
				else
				{
					ALYSLC::Log("[TM] CalculatePredInterceptPos: {}: RESULT: Did NOT meet MIN t precision! tDiff: {} in {} steps", 
						a_p->coopActor->GetName(), tDiff, step);
				}*/

				// Either converged on a particular intercept position,
				// with the change in time to target under the lower bail precision (success),
				// or didn't quite meet that required precision (failed).
				return predHitPos;
			}
		}
	}

	double TargetingManager::ManagedProjTrajectoryInfo::GetReleaseSpeedToTarget(const double& a_xy, const double& a_z, const double& a_launchPitch)
	{
		// Accounting for linear air resistance, 
		// get the release speed required to hit the target position
		// given by the XY and Z offsets and the launch pitch.
		
		// Get release speed first.
		double releaseSpeedNew = releaseSpeed;
		double w = -exp((a_z * mu * mu / g) - (a_xy * tanf(a_launchPitch) * mu * mu / g) - 1.0);
		const auto solnPair = Util::LambertWFunc::ApproxRealSolutionBothBranches(w, 1E-10);
		// Two potential solutions.
		double launchSpeed1 = solnPair.first.has_value() ? (a_xy * mu) / (cosf(a_launchPitch) * (solnPair.first.value() + 1.0)) : -1.0;
		double launchSpeed2 = solnPair.second.has_value() ? (a_xy * mu) / (cosf(a_launchPitch) * (solnPair.second.value() + 1.0)) : -1.0;

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
		if (trajType == ProjectileTrajType::kAimDirection && isPhysicalProj && releaseSpeed < maxReleaseSpeed) 
		{
			// Smallest of the three.
			return min(releaseSpeed, min(maxReleaseSpeed, releaseSpeedNew));
		}

		// Can't be larger than the max release speed.
		return min(maxReleaseSpeed, releaseSpeedNew);
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetInitialBaseProjectileData(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, const float& a_releaseSpeed)
	{
		// Set physical data and projectile data that depends on the base projectile type.
		
		RE::Projectile* projectile = nullptr;
		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle); 
		if (projectilePtr) 
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
				float fullDrawTime = 0.4f + (1.66f / (weap->GetSpeed() * (1.0f + (float)a_p->coopActor->HasPerk(glob.quickShotPerk)))) + 0.6f;
				float drawTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kAttackRH);
				float power = std::clamp(drawTime, fullDrawTime * 0.35f, fullDrawTime) / fullDrawTime;
				releaseSpeed = maxReleaseSpeed * power;
			}
			else
			{
				// Fixed initial release speed otherwise.
				releaseSpeed = ammo->data.projectile->data.speed;
			}

			// Add gravity factor.
			projGravFactor += ammo->data.projectile->data.gravity;
		}
		else if (const auto avEffect = projectile->avEffect; avEffect && avEffect->data.projectileBase)
		{
			// Is a magic projectile.
			isPhysicalProj = false;
			// Max and initial release speeds are the same.
			maxReleaseSpeed = releaseSpeed = avEffect->data.projectileBase->data.speed;
			// Add gravity factor.
			projGravFactor += avEffect->data.projectileBase->data.gravity;
		}
		else
		{
			// Anything else.
			isPhysicalProj = true;
			maxReleaseSpeed = releaseSpeed = a_releaseSpeed;
		}

		// Physical constants.
		mu = Settings::fMu;
		g = Settings::fG * HAVOK_TO_GAME * projGravFactor;
		if (glob.player1Actor->parentCell)
		{
			auto bhkWorld = glob.player1Actor->parentCell->GetbhkWorld();
			if (bhkWorld && bhkWorld->GetWorld1())
			{
				g = -bhkWorld->GetWorld1()->gravity.quad.m128_f32[2] * HAVOK_TO_GAME * projGravFactor;
			}
		}

		// And lastly, the release position.
		releasePos = projectile->data.location;
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, RE::NiPoint3& a_initialVelocityOut, const ProjectileTrajType& a_trajType)
	{
		// Sets up the initial trajectory data for the given projectile
		// based on the given starting velocity (which is modified) and the trajectory type.

		// NOTE: Should be run once when the projectile of interest is released.
		trajType = a_trajType;
		startedHomingIn = false;
		// Set target actor regardless of projectile trajectory type.
		targetActorHandle = a_p->tm->targetMotionState->targetActorHandle;
		targetLocalPosOffset = Util::HandleIsValid(targetActorHandle) ? a_p->tm->crosshairLastHitLocalPos : RE::NiPoint3();
		targetedActorNode.reset();

		// Fall back on crosshair world position if there is no targeted actor.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;

		// If just fired and aiming at an actor, direct at the closest node to the crosshair world position.
		// Seems to improve hit recognition, since the crosshair world position
		// may not intersect with the target's collision volumes.
		// Alas, projectiles still pass through actors without colliding at times.
		auto targetActorPtr = Util::GetActorPtrFromHandle(targetActorHandle);
		bool targetActorValidity = targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get());
		if (targetActorValidity && Settings::vbUseAimCorrection[a_p->playerID] && targetActorHandle == a_p->tm->crosshairRefrHandle)
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

						if (auto node3D = RE::NiPointer<RE::NiAVObject>(a_node); node3D && node3D.get())
						{
							// Must have havok rigid body to collide with.
							if (auto hkpRigidBody = Util::GethkpRigidBody(node3D.get()); hkpRigidBody)
							{
								// Get center of mass to gauge distance.
								const auto centerOfMass = ToNiPoint3(hkpRigidBody->motion.motionState.sweptTransform.centerOfMass0) * HAVOK_TO_GAME;
								if (float dist = centerOfMass.GetDistance(trajectoryEndPos); dist < closestNodeDist)
								{
									if (auto collidable = hkpRigidBody->GetCollidable(); collidable)
									{
										// New closest node. Set closest distance too.
										closestNodeDist = dist;
										targetedActorNode = RE::NiPointer<RE::NiAVObject>(a_node);
										nodeCenterOfMassPos = centerOfMass;
									}
								}
							}
						}
					}
				);

				// Set to closest node's center of mass.
				trajectoryEndPos = nodeCenterOfMassPos;
			}
		}

		// Save initial release speed.
		const float initialReleaseSpeed = a_initialVelocityOut.Length();
		// Set initial base projectile data first.
		SetInitialBaseProjectileData(a_p, a_projectileHandle, initialReleaseSpeed);

		// Aim prediction or aim direction while aiming at crosshair target or facing target.
		bool predictInterceptPos = 
		{
			(a_trajType == ProjectileTrajType::kPrediction) ||
			(
				(a_trajType == ProjectileTrajType::kAimDirection) &&
				(a_p->mm->shouldFaceTarget || a_p->tm->GetRangedTargetActor())
			)
		};
		if (predictInterceptPos) 
		{
			// XY and Z offsets from the release position to the trajectory end position.
			double xy = 0.0;
			double z = 0.0;

			float minLaunchPitch = GetRoughMinLaunchPitch(a_p);
			// Use min launch pitch estimate if aim pitch was not manually adjusted.
			launchPitch = std::clamp(a_p->mm->aimPitchAdjusted ? max(minLaunchPitch, -a_p->mm->aimPitch) : minLaunchPitch, -89.9f * PI / 180.0f, 89.9f * PI / 180.0f);
			// Add some arc to fast projectiles by decreasing their release speed
			// when released at a steeper angle.
			float straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
			if (initialReleaseSpeed > 10000.0f)
			{
				maxReleaseSpeed = initialReleaseSpeed;
				double ratio = std::clamp(1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0);
				releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
			}

			// Calculate the position at which the projectile is predicted to hit the target actor.
			// As of now, no release speed modifications for projectiles.
			// Used to only modify the release speed of physical projectiles instead of modifying 
			// the gravitational constant to hit the same intercept position,
			// but doing so would negate the draw-time dependent release speed of bows,
			// so I've decided against it.
			trajectoryEndPos = CalculatePredInterceptPos(a_p, false);

			launchYaw = Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos));
			// XY position to release pos.
			xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			// Z position relative to release pos.
			z = (trajectoryEndPos - releasePos).z;

			// Since we are not modifying the projectile's release speed,
			// in order to still hit the intercept position, 
			// we modify the gravitational constant.
			// Better obviously for launching accurate, fast-arcing projectiles
			// instead of lowering the release speed while keeping g constant
			// in order to hit the same position.
			// But looks a bit odd for flat trajectory shots when g is low.
			// Tradeoffs, schmadeoffs.
			g = 
			(
				((mu * mu * releaseSpeed * cosf(launchPitch)) * (z - xy * tanf(launchPitch))) / 
				((releaseSpeed * cosf(launchPitch) * log(1 - (xy * mu) / (releaseSpeed * cosf(launchPitch)))) + xy * mu)
			);
			g = isnan(g) || isinf(g) ? g = 0.0 : g;

			// Set the total time of flight before 
			// the launched projectile hits the intercept position, if unimpeded.
			initialTrajTimeToTarget = max(-log(1.0f - (xy * mu) / (releaseSpeed * cosf(launchPitch))) / mu, 0.0);
		}
		else if (a_trajType == ProjectileTrajType::kHoming)
		{
			// Set straight-line pitch from release position to end position
			// after calculating the trajectory end position.
			// NOTE: Launch pitch/straight line pitch is sign-flipped relative to the game's pitch sign convention.
			float straightLinePitch = -a_p->mm->aimPitch;
			// Aim at target actor if valid and not mounted,
			// or if mounted and selected with the crosshair.
			// Want to avoid shooting at an aim correction target 
			// while mounted and targeting another object.
			auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(a_p->tm->selectedTargetActorHandle);
			bool aimAtActor = 
			{
				(targetActorPtr) && 
				(!a_p->coopActor->IsOnMount() || targetActorPtr == selectedTargetActorPtr)
			};
			if (aimAtActor) 
			{
				// REMOVE
				auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle(a_p->tm->aimCorrectionTargetHandle);
				// If the target is an aim correction target, target the torso.
				trajectoryEndPos = 
				(
					!selectedTargetActorPtr && aimCorrectionTargetPtr ? 
					Util::GetTorsoPosition(aimCorrectionTargetPtr.get()) : 
					trajectoryEndPos
				);
				straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);

				// Set launch angles.
				launchPitch = std::clamp(max(straightLinePitch, -a_p->mm->aimPitch), -89.9f * PI / 180.0f, 89.9f * PI / 180.0f);
				launchYaw = Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos));

				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (initialReleaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = initialReleaseSpeed;
					double ratio = std::clamp(1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE: No air resistance, so the XY component of velocity 
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max(0.0, Util::GetXYDistance(releasePos, trajectoryEndPos) / (releaseSpeed * cosf(launchPitch)));
			}
			else
			{
				// XY offset to trajectory end position.
				double xy = 0.0f;
				// Aim as far away as max navmesh move distance 
				// or crosshair world position, whichever is closer.
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = iniPrefSettings ? iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : nullptr; 
				if (projMaxDistSetting && releasePos.GetDistance(a_p->tm->crosshairWorldPos) > projMaxDistSetting->data.f)
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
				launchPitch = std::clamp(max(straightLinePitch, -a_p->mm->aimPitch), -89.9f * PI / 180.0f, 89.9f * PI / 180.0f);
				launchYaw = Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos));
				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (initialReleaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = initialReleaseSpeed;
					double ratio = std::clamp(1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE: No air resistance, so the XY component of velocity
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max(0.0, xy / (releaseSpeed * cosf(launchPitch)));
			}

			// Same as usual.
			double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			double z = (releasePos - trajectoryEndPos).z;

			g = (2.0 / xy) * ((powf(releaseSpeed, 2.0f) * cosf(launchPitch) * sinf(launchPitch)) - ((z * powf(releaseSpeed * cosf(launchPitch), 2.0f)) / (xy)));
			g = isnan(g) || isinf(g) ? g = 0.0 : g;
		}
		else
		{
			// Aim direction projectile and not facing the crosshair world position.
			// Aim far, far away in the direction that the player is facing.

			// Set launch angles.
			launchPitch = std::clamp(-a_p->mm->aimPitch, -89.9f * PI / 180.0f, 89.9f * PI / 180.0f);
			launchYaw = Util::ConvertAngle(a_p->coopActor->GetHeading(false));
			// Choose endpoint that is far from the release point.
			// Default time of flight is arbitrary, but should be relatively large. Chose 5 seconds here.
			// Accounting for air resistance.
			double xy = (releaseSpeed * cosf(launchPitch) / mu) * (1.0 - exp(-mu * 5.0));
			double z = (-g * 5.0 / mu) + ((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * (1.0 - exp(-mu * 5.0));

			auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
			auto projMaxDistSetting = iniPrefSettings ? iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : nullptr; 
			if (projMaxDistSetting) 
			{
				xy = projMaxDistSetting->data.f;
				double t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
				z = (-g * t / mu) + ((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * (1.0 - exp(-mu * t));
			}

			trajectoryEndPos = RE::NiPoint3
			(
				releasePos.x + xy * cosf(launchYaw), 
				releasePos.y + xy * sinf(launchYaw),
				releasePos.z + z
			);

			initialTrajTimeToTarget = max(-log(1.0f - (xy * mu) / (releaseSpeed * cosf(launchPitch))) / mu, 0.0);
		}

		// Re-scale the initial speed sent on launch to our computed release speed.
		a_initialVelocityOut.Unitize();
		a_initialVelocityOut *= releaseSpeed;

		RE::Projectile* projectile = nullptr;
		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
		if (projectilePtr)
		{
			projectile = projectilePtr->As<RE::Projectile>();
		}

		// Smart ptr was invalid, so its managed projectile is as well.
		if (!projectile)
		{
			return;
		}
		// Perform ammo projectile damage scaling based on 
		// the ratio of the computed release speed over the max release speed.
		// Projectile power always defaults to 1 for companion players,
		// so we have to scale it directly.
		// For P1, we have to compute the original weapon damage 
		// by dividing the current weapon damage by the power set on launch. 
		// Then we also scale the power/weapon damage based on our own release speed factor.
		// This will directly adjust the output damage of the projectile on hit.
		if (const auto ammo = projectile->ammoSource; ammo && ammo->data.projectile)
		{
			// Scale arrow/bolt's damage based on the computed power.
			double releaseSpeedFactor = std::clamp(releaseSpeed / maxReleaseSpeed, 0.1, 1.0);
			projectile->power = releaseSpeedFactor;
			if (a_p->isPlayer1) 
			{
				float originalWeaponDamage = projectile->weaponDamage / max(0.1f, projectile->power);
				projectile->weaponDamage = originalWeaponDamage * releaseSpeedFactor;
			}
			else
			{
				projectile->weaponDamage *= releaseSpeedFactor;
			}
		}
	}

	float TargetingManager::ManagedProjTrajectoryInfo::GetRoughMinLaunchPitch(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Get rough estimate of the minimum launch pitch required to hit the target, 
		// based on the given projectile's release speed.
		
		// NOTE: 
		// Cannot analytically find a solution for this minimum launch pitch when applying air resistance, 
		// so calculate the drop when aiming in a straight line at the predicted intercept position 
		// and then add the drop to the predicted intercept position to compensate.
		// Finally, recalculate the pitch to the new temporary intercept position.
		// Still will fail to hit the intercept position at range, 
		// but is more accurate than simply setting the
		// minimum launch pitch to the straight-line-to-target pitch.

		float straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
		if (a_p->mm->aimPitchAdjusted && -a_p->mm->aimPitch >= straightLinePitch)
		{
			// Already aiming above the straight line pitch,
			// so it can be used as the launch pitch.
			return -a_p->mm->aimPitch;
		}

		// Positional offsets, as usual.
		double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
		double z = 
		(
			(g / (mu * mu)) * 
			(
				(log(1 - ((xy * mu) / (releaseSpeed * cosf(straightLinePitch))))) + 
				(xy / (releaseSpeed * cosf(straightLinePitch))) * ((releaseSpeed * sinf(straightLinePitch)) + (g / mu))
			)
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
			return -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos + RE::NiPoint3(0.0f, 0.0f, straightLineTrajDrop));
		}
	}

	void TargetingManager::ManagedProjectileHandler::Insert(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, RE::NiPoint3& a_initialVelocityOut, const ProjectileTrajType& a_trajType)
	{
		// Insert the given projectile into the managed list.
		// Then set its trajectory information 
		// and update its initial velocity through the outparam.

		RE::Projectile* projectile = nullptr;
		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
		if (projectilePtr)
		{
			projectile = projectilePtr->As<RE::Projectile>();
		}

		// Smart ptr was invalid, so its managed projectile is as well, return early.
		if (!projectile)
		{
			return;
		}

		// Keep queue at a modest size by removing expired projectiles
		// if the queue size is above a certain threshold.
		if (!managedProjHandles.empty() && managedProjHandles.size() >= Settings::uManagedPlayerProjectilesBeforeRemoval)
		{
			for (const auto& [handle, _] : managedProjHandleToTrajInfoMap) 
			{
				projectilePtr = Util::GetRefrPtrFromHandle(handle);
				if (projectilePtr)
				{
					projectile = projectilePtr->As<RE::Projectile>();
				}

				// Remove if invalid, not loaded, deleted, marked for deletion, has collided, or limited.
				if (!projectile || !projectile->Is3DLoaded() || projectile->IsDeleted() || projectile->IsMarkedForDeletion() || 
					!projectile->impacts.empty() || projectile->ShouldBeLimited()) 
				{
					managedProjHandleToTrajInfoMap.erase(handle);
				}
			}

			// Reconstruct list of managed FIDs.
			managedProjHandles.clear();
			for (const auto& [fid, _] : managedProjHandleToTrajInfoMap) 
			{
				managedProjHandles.push_back(fid);
			}
		}

		// Insert FID to managed list if not already managed.
		bool alreadyManaged = 
		{
			!managedProjHandleToTrajInfoMap.empty() && 
			managedProjHandleToTrajInfoMap.contains(a_projectileHandle)
		};
		if (!alreadyManaged) 
		{
			managedProjHandles.push_back(a_projectileHandle);
		}

		// Insert constructed trajectory info for this projectile.
		// NOTE: Construction sets all the trajectory data automatically.
		managedProjHandleToTrajInfoMap.insert_or_assign
		(
			a_projectileHandle, 
			std::make_unique<ManagedProjTrajectoryInfo>(a_p, a_projectileHandle, a_initialVelocityOut, a_trajType)
		);
	}
}

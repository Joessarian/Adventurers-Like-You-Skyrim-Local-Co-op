#include "MovementManager.h"
#include <GlobalCoopData.h>
#include <Settings.h>
#include <Util.h>
#include <Compatibility.h>

namespace ALYSLC
{
	// Global co-op data.
	static GlobalCoopData& glob = GlobalCoopData::GetSingleton();

	MovementManager::MovementManager() :
		Manager(ManagerType::kMM)
	{ }

	void MovementManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p && a_p->controllerID > -1 && a_p->controllerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			SPDLOG_DEBUG("[MM] Initialize: Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count());
			RefreshData();
		}
		else
		{
			SPDLOG_ERROR
			(
				"[MM] ERR: Initialize: Cannot construct Movement Manager for controller ID {}.", 
				a_p ? a_p->controllerID : -1
			);
		}
	}

	void MovementManager::MainTask()
	{
		UpdateMovementParameters();
		UpdateMovementState();
		UpdateAttackSourceOrientationData(false);
		UpdateAimPitch();
		SetHeadTrackTarget();
		SetPlayerOrientation();
	}

	void MovementManager::PrePauseTask()
	{
		SPDLOG_DEBUG("[MM] PrePauseTask: P{}", playerID + 1);

		// Set P1 as motion driven when the manager is not active
		// to restore normal movement.
		if (p->isPlayer1)
		{
			Util::SetPlayerAIDriven(false);
		}

		// Force the player to get up out of ragdoll state before pausing.
		if (coopActor->IsInRagdollState() &&
			glob.coopSessionActive &&
			!p->isDowned && 
			!coopActor->IsDead())
		{
			coopActor->NotifyAnimationGraph("GetUpBegin");
			coopActor->PotentiallyFixRagdollState();
		}

		// Reset pitch angle, speedmult.
		coopActor->data.angle.x = 0.0f;
		coopActor->SetActorValue(RE::ActorValue::kSpeedMult, 100.0f);
		coopActor->RestoreActorValue
		(
			RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f
		);
		coopActor->RestoreActorValue
		(
			RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f
		);

		// Stop movement.
		ClearKeepOffsetFromActor();
		coopActor->StopMoving(*g_deltaTimeRealTime);
		Util::NativeFunctions::SetDontMove(coopActor.get(), true);
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);

		// Reset node rotations.
		if (nextState == ManagerState::kAwaitingRefresh) 
		{
			nom->InstantlyResetAllNodeData(p);
		}
	}

	void MovementManager::PreStartTask()
	{
		SPDLOG_DEBUG("[MM] PreStartTask: P{}", playerID + 1);
		ResetTPs();

		// Set P1 as AI driven to allow for movement manipulation with this manager.
		if (p->isPlayer1)
		{
			Util::SetPlayerAIDriven(true);
		}

		// Ensure that the player is upright if ragdolled or in a glitched ragdoll state.
		if (glob.coopSessionActive && !p->isDowned && !coopActor->IsDead()) 
		{
			coopActor->NotifyAnimationGraph("GetUpBegin");
		}

		// Ensure the player is not set to stationary.
		Util::NativeFunctions::SetDontMove(coopActor.get(), false);

		// If Precision is installed, make sure Precision is enabled on this actor.
		if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI3; api)
		{
			api->ToggleDisableActor(coopActor->GetHandle(), false);
		}

		// Initial aim pitch position.
		aimPitchPos = glob.player1Actor->data.location;
		coopActor->data.angle.x = 0.0f;

		// Reset node rotations.
		if (currentState == ManagerState::kAwaitingRefresh)
		{
			nom->InstantlyResetAllNodeData(p);
		}
		
		// Make sure collision is enabled for the player.
		Util::EnableCollisionForActor(coopActor.get());
	}

	void MovementManager::RefreshData()
	{
		// Player and actors.
		coopActor = p->coopActor;
		movementActor = coopActor;
		// CID.
		controllerID = p->controllerID;
		// Player ID.
		playerID = p->playerID;
		// Positions.
		aimPitchPos = coopActor->data.location;
		dashDodgeDir = RE::NiPoint3();
		playerAttackSourcePos = 
		playerDefaultAttackSourcePos = 
		(
			coopActor->data.location + RE::NiPoint3(0.0f, 0.0f, 0.75f * coopActor->GetHeight())
		);
		playerAttackSourceDir =
		playerDefaultAttackSourceDir = 
		playerTorsoAxisOfRotation = RE::NiPoint3(0.0f, 0.0f, 0.0f);
		playerTorsoPosition = Util::GetTorsoPosition(coopActor.get());
		// Externally set flags.
		reqFaceTarget = reqResetAimAndBody = reqStartJump = false;
		// Movement parameters list.
		movementOffsetParams = std::vector<float>(!MoveParams::kTotal, 0.0f);
		// Node orientation manager.
		nom = std::make_unique<NodeOrientationManager>();
		// Booleans.
		adjustAimPitchToFaceTarget = false;
		aimPitchAdjusted = false;
		aimPitchManuallyAdjusted = false;
		attemptDiscovery = false;
		dontMoveSet = true;
		faceTarget = false;
		hasMovementOffset = false;
		inRangeOfUndiscoveredMarker = false;
		interactionPackageRunning = false;
		isAirborneWhileJumping = false;
		isAnimDriven = false;
		isBackStepDodge = false;
		isDashDodging = false;
		isFallingWhileJumping = false;
		isGettingUp = false;
		isMounting = false;
		isParagliding = false;
		isParaglidingTiltAngleReset = false;
		isRequestingDashDodge = false;
		isSwimming = false;
		isSynced = false;
		lsMoved = false;
		playerRagdollTriggered = false;
		menuStopsMovement = false;
		movementYawTargetChanged = false;
		rsMoved = false;
		sentJumpFallEvent = false;
		shouldCurtailMomentum = true;
		shouldParaglide = false;
		shouldStartMoving = false;
		shouldStopMoving = false;
		turnToTarget = false;
		// Floats.
		aimPitch = PI / 18.0f;
		baseHeightMult = max(0.001f, static_cast<float>(coopActor->refScale) / 100.0f);
		baseSpeedMult = Settings::fBaseSpeed * (1.0f / baseHeightMult);
		dashDodgeCompletionRatio = 0.0f;
		dashDodgeEquippedWeight = 0.0f;
		dashDodgeInitialSpeed = 0.0f;
		dashDodgeLSDisplacement = 0.0f;
		dashDodgeTorsoPitchOffset = 0.0f;
		dashDodgeTorsoRollOffset = 0.0f;
		lsAngAtMaxDisp = rsAngAtMaxDisp = 0.0f;
		oldLSAngle = 0.0f;
		playerScaledHeight = coopActor->GetHeight();
		playerPitch = 0.0f;
		playerYaw = coopActor->GetHeading(false);
		prevLSAngAtMaxDisp = prevRSAngAtMaxDisp = 0.0f;
		magicParaglideEndZVel = magicParaglideStartZVel = magicParaglideVelInterpFactor = 0.0f;
		framesSinceAttemptingDiscovery =
		framesSinceRequestingDashDodge = 
		framesSinceStartingDashDodge = 
		framesSinceStartingJump = 
		framesToCompleteDashDodge = 0;

		// Reset time points used by this manager.
		ResetTPs();
		// Update encumbrance factor.
		UpdateEncumbranceFactor();
		SPDLOG_DEBUG("[MM] RefreshData: {}.", coopActor ? coopActor->GetName() : "NONE");
	}

	const ManagerState MovementManager::ShouldSelfPause()
	{
		// Suspension triggered externally.
		return currentState;
	}

	const ManagerState MovementManager::ShouldSelfResume()
	{
		// Resumption triggered externally.
		return currentState;
	}

	void MovementManager::ClearKeepOffsetFromActor()
	{
		// Clear movement offset if one is set.

		if (hasMovementOffset)
		{
			if (movementActor && movementActor.get())
			{
				Util::NativeFunctions::ClearKeepOffsetFromActor(movementActor.get());
			}

			hasMovementOffset = false;
		}
	}

	float MovementManager::GetArmRotationFactor(bool&& a_forArmRotationSpeed)
	{
		// Get arm/player rotation slowdown factor when the player is rotating their arms.
		// If requesting the factor for arm rotation speed, 
		// the lower the stamina level relative to max, the lower the rotation factor,
		// meaning a tired player will flail their arms about and rotate more slowly.
		// If requesting the factor for slap force application,
		// the lower the stamina level relative to max, the lower the force applied.
		// Return rotation speed factor related to arm rotation speed or slap force.

		// No speed reduction when in god mode.
		if (p->isInGodMode)
		{
			return 1.0f;
		}

		const float maxStamina = Util::GetFullAVAmount(coopActor.get(), RE::ActorValue::kStamina);
		const float currentStamina = coopActor->GetActorValue(RE::ActorValue::kStamina);
		if (a_forArmRotationSpeed)
		{
			return (0.25f * currentStamina / maxStamina + 0.75f);
		}
		else
		{
			return (0.5f * currentStamina / maxStamina + 0.5f);
		}
	}

	float MovementManager::GetRotationMult()
	{
		// Set player rotation speed multiplier based on what action the player is performing.

		auto mult = 1.0f;
		if (p->pam->isAttacking)
		{
			if (p->pam->isRangedAttack)
			{
				mult = Settings::fRangedAttackRotMult;
			}
			else
			{
				mult = Settings::fMeleeAttackRotMult;
			}
		}
		if (p->pam->isBashing)
		{
			mult = Settings::fBashingRotMult;
		}
		if (p->pam->isBlocking)
		{
			mult = Settings::fBlockingRotMult;
		}
		if (p->pam->isInCastingAnim)
		{
			mult = Settings::fCastingRotMult;
		}
		if (p->pam->isJumping)
		{
			mult = Settings::fJumpingRotMult;
		}
		if (p->pam->isRiding)
		{
			mult = Settings::fRidingRotMult;
		}
		if (p->pam->isSneaking)
		{
			mult = Settings::fSneakRotMult;
		}
		if (p->pam->isSprinting) 
		{
			mult = Settings::fSprintingRotMult;
		}

		return std::clamp(mult, 0.0f, 1.0f);
	}

	bool MovementManager::HasRotatedArms()
	{
		// Return true if the player has rotated at least one arm node.
		for (const auto& armNodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES)
		{
			if (!nom->nodeNameToRotationDataMap.contains(armNodeName))
			{
				continue;
			}

			if (nom->nodeNameToRotationDataMap.at(armNodeName)->rotationModified)
			{
				return true;
			}
		}

		for (const auto& armNodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES)
		{
			if (!nom->nodeNameToRotationDataMap.contains(armNodeName))
			{
				continue;
			}

			if (nom->nodeNameToRotationDataMap.at(armNodeName)->rotationModified)
			{
				return true;
			}
		}

		return false;
	}

	bool MovementManager::HasRotatedTorso()
	{
		// Return true if the player has rotated at least one torso node.

		for (const auto& torsoNodeName : GlobalCoopData::ADJUSTABLE_TORSO_NODES)
		{
			if (!nom->nodeNameToRotationDataMap.contains(torsoNodeName))
			{
				continue;
			}

			if (nom->nodeNameToRotationDataMap.at(torsoNodeName)->rotationModified)
			{
				return true;
			}
		}

		return false;
	}

	void MovementManager::KeepOffsetFromActor
	(
		const RE::ActorHandle& a_targetHandle, 
		const RE::NiPoint3& a_posOffset, 
		const RE::NiPoint3& a_angOffset, 
		float a_catchUpRadius,
		float a_followRadius
	)
	{
		// Set movement offset from the player's movement actor (themselves or mount).

		if (movementActor && movementActor.get())
		{
			Util::NativeFunctions::KeepOffsetFromActor
			(
				movementActor.get(), 
				a_targetHandle, 
				a_posOffset, 
				a_angOffset, 
				a_catchUpRadius, 
				a_followRadius
			);
			hasMovementOffset = true;
		}
	}

	void MovementManager::PerformDashDodge()
	{
		// Movement-based, 360 degree dodge with I-frames. 
		// Looks janky at times, especially with foot IK wonkiness, but it
		// works regardless of the player's attack state,
		// with weapons sheathed, and even in midair while paragliding.
		// Won't trigger when flying, mounted, jumping, blocking, or sneaking.
		
		// Must have a character controller attached.
		auto charController = coopActor->GetCharController(); 
		if (!charController) 
		{
			isRequestingDashDodge = isDashDodging = isBackStepDodge = false;
			framesSinceStartingDashDodge = framesSinceRequestingDashDodge = 0;
			dashDodgeCompletionRatio = 0.0f;
			return;
		}
		
		bool isSwimming = coopActor->IsSwimming();
		if (isRequestingDashDodge && !isDashDodging)
		{
			framesSinceStartingDashDodge = framesSinceRequestingDashDodge = 0;
			dashDodgeCompletionRatio = 0.0f;
			if (reqFaceTarget)
			{
				coopActor->data.angle.z = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions
					(
						coopActor->data.location, p->tm->crosshairWorldPos
					)
				);
			}

			// Clearly, the player has to move.
			SetDontMove(false);
			// Stop attacking/sprinting before dodging.
			coopActor->NotifyAnimationGraph("attackStop");
			coopActor->NotifyAnimationGraph("SprintStop");
			coopActor->NotifyAnimationGraph("staggerStop");
			coopActor->NotifyAnimationGraph("moveStart");
			// Can dash dodge only if the sneak start animation event is processed successfully,
			// or if paragliding or swimming, since the sneak animation will always fail 
			// unless the player is on the ground. Will also fail when transformed.
			bool succ = 
			(
				isParagliding || 
				isSwimming ||
				p->isTransformed || 
				coopActor->NotifyAnimationGraph("SneakStart")
			);

			if (succ)
			{
				SPDLOG_DEBUG("[MM] PerformDashDodge: {}: Getting lock. (0x{:X})", 
					coopActor->GetName(),
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				{
					std::unique_lock<std::mutex> perfAnimQueueLock
					(
						p->pam->avcam->perfAnimQueueMutex
					);
					SPDLOG_DEBUG("[MM] PerformDashDodge: {}: Lock obtained. (0x{:X})", 
						coopActor->GetName(), 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));

					// Queue dodge anim event tag so that this player's player action manager 
					// can handle stamina expenditure.
					p->pam->avcam->perfAnimEventsQueue.emplace
					(
						std::pair<PerfAnimEventTag, uint16_t>
						(
							PerfAnimEventTag::kDodgeStart, 
							p->pam->lastAnimEventID
						)
					);
					p->pam->lastAnimEventID = 
					(
						p->pam->lastAnimEventID == UINT16_MAX ? 1 :
						p->pam->lastAnimEventID + 1
					);
				}
			}

			isDashDodging = succ;
			isRequestingDashDodge = !succ;
			// Back step if not moving the LS.
			const auto& lsMag = glob.cdh->GetAnalogStickState(controllerID, true).normMag;
			isBackStepDodge = isDashDodging && !lsMoved;
		}
		
		if (isDashDodging)
		{
			const uint32_t noCollFlag = !RE::CFilter::Flag::kNoCollision;
			if (framesSinceRequestingDashDodge == 0) 
			{
				// Varying I-Frames/speed to maintain the same dodge distance 
				// at different framerates. Set once at dodge start.
				const float frameScalingFactor = 1.0f / (60.0f * *g_deltaTimeRealTime);
				framesToCompleteDashDodge = 
				(
					frameScalingFactor * 
					static_cast<float>
					(
						Settings::uDashDodgeSetupFrameCount +
						Settings::uDashDodgeBaseAnimFrameCount
					)
				);

				p->lastDashDodgeTP = SteadyClock::now();
				// Cache LS displacement and equipped weight.
				dashDodgeLSDisplacement = 
				(
					isBackStepDodge ? 
					1.0f : 
					glob.cdh->GetAnalogStickState(controllerID, true).normMag
				);
				dashDodgeEquippedWeight = coopActor->GetEquippedWeight();
			}

			float secsSinceStartingDodge = Util::GetElapsedSeconds(p->lastDashDodgeTP);
			// Frame progress towards completing the dodge.
			dashDodgeCompletionRatio = std::clamp
			(
				static_cast<float>(framesSinceRequestingDashDodge) / framesToCompleteDashDodge, 
				0.0f, 
				1.0f
			);

			// Ensure the player can rotate so that they keep their dodge momentum
			// even when interrupting a power attack, 
			// during the duration of which rotation is disabled.
			bool allowRotation = false;
			coopActor->GetGraphVariableBool("bAllowRotation", allowRotation);
			// Yes, I'm not sure why it's named 'bAllowRotation' 
			// when setting it to true does just the opposite here.
			if (allowRotation) 
			{
				coopActor->SetGraphVariableBool("bAllowRotation", false);
			}

			// No longer animation driven or stopped from moving.
			coopActor->SetGraphVariableBool("bAnimationDriven", false);
			Util::NativeFunctions::SetDontMove(coopActor.get(), false);

			bool dodgeDurationExpired = dashDodgeCompletionRatio == 1.0f;
			// Can dodge with either the regular dodge bind, or the special action bind.
			// Check the time since the more recently-triggered action.
			float secsSinceLastSpecialAction = p->pam->GetSecondsSinceLastStop
			(
				InputAction::kSpecialAction
			);
			float secsSinceLastDodge = p->pam->GetSecondsSinceLastStop(InputAction::kDodge);
			bool startedAttackDuringDodge = 
			(
				Util::GetElapsedSeconds(p->lastAttackStartTP) < 
				min(secsSinceLastDodge, secsSinceLastSpecialAction)
			);
			// If an attack was started while dodging 
			// or if dodge frame duration is up, stop dodging.
			if (dodgeDurationExpired || startedAttackDuringDodge)
			{
				isDashDodging = isBackStepDodge = false;

				// Do not stop in mid-air.
				if (!isParagliding) 
				{
					// Stop moving once the dodge stops.
					if (!lsMoved)
					{
						SetDontMove(true);
					}

					// Stop sneak animation which played for the duration of the dodge.
					if (!p->isTransformed)
					{
						coopActor->NotifyAnimationGraph("SneakStop");
					}

					// Set the state back to swimming to avoid dropping like a rock in the water
					// after the dodge completes.
					if (isSwimming)
					{
						charController->wantState = 
						charController->context.currentState = 
						RE::hkpCharacterStateType::kSwimming;
					}

					// Reset torso and character controller pitch.
					charController->pitchAngle =
					charController->rollAngle = 
					dashDodgeTorsoPitchOffset = 
					dashDodgeTorsoRollOffset = 0.0f;
				}

				// Remove AV cost action, if it still hasn't been processed for some reason.
				p->pam->avcam->RemoveRequestedAction(AVCostAction::kDodge);
				p->pam->avcam->RemoveStartedAction(AVCostAction::kDodge);

				// Reset ghost flag to terminate I-frames.
				if (auto actorBase = coopActor->GetActorBase(); actorBase)
				{
					actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				}
			}
			else if (auto actorBase = coopActor->GetActorBase(); actorBase)
			{
				auto& baseFlags = actorBase->actorData.actorBaseFlags;
				bool isGhost = coopActor->IsGhost();
				// Set I-frames when inside the window, reset otherwise.
				if (!isGhost)
				{
					baseFlags.set(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				}

				// Set direction and XY speed on the first frame of the dodge.
				// Rotation is locked by the movement type 
				// and the only change made to the XY velocity after the first frame
				// is the XY speed.
				if (framesSinceStartingDashDodge == 0)
				{
					if (isBackStepDodge || !lsMoved)
					{
						// Dodge backward.
						dashDodgeDir = Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle
							(
								Util::NormalizeAng0To2Pi(coopActor->GetHeading(false) - PI)
							)
						);
					}
					else
					{
						// Dodge in the direction of the LS.
						dashDodgeDir = Util::RotationToDirectionVect
						(
							0.0f,
							Util::ConvertAngle(movementOffsetParams[!MoveParams::kLSGameAng])
						);
					}

					// Set initial speed as magnitude of the current pre-processed XY velocity.
					dashDodgeInitialSpeed = RE::hkVector4
					(
						charController->initialVelocity.quad.m128_f32[0], 
						charController->initialVelocity.quad.m128_f32[1], 
						0.0f, 
						0.0f
					).Length3();
				}

				// Z velocity set elsewhere, so save the Z component and restore later.
				RE::hkVector4 havokVel;
				charController->GetLinearVelocityImpl(havokVel);
				float zComp = havokVel.quad.m128_f32[2];
				// Get dodge speed mult ratio (max / min)
				// and multiply the default paraglide MT forward movement value by this value
				// to get the max target XY speed in game units.
				// Max may not always be greater than min, so ensure the ratio is never below 1.
				const float dodgeVelRatio = 
				(
					Settings::fMinDashDodgeSpeedmult == 0.0f ? 
					2.0f : 
					max(1.0f, Settings::fMaxDashDodgeSpeedmult / Settings::fMinDashDodgeSpeedmult)
				);
				float maxXYSpeed = dodgeVelRatio;
				if (isParagliding) 
				{
					if (glob.paraglidingMT)
					{
						const auto& speeds = 
						(
							glob.paraglidingMT->movementTypeData.defaultData.speeds
						);
						maxXYSpeed *= 
						(
							speeds
							[RE::Movement::SPEED_DIRECTIONS::kForward]
							[RE::Movement::MaxSpeeds::kRun]
						);
					}
					else
					{
						maxXYSpeed *= 700.0f;
					}

					// P1 dodges further than companion players, 
					// even if the dash dodge speeds are set to the same value.
					// Halve the dodge speed for P1 here.
					maxXYSpeed *= 0.5f;
				}
				else
				{
					maxXYSpeed = 
					(
						Settings::fMaxDashDodgeSpeedmult
					);
				}

				// Scale down max dodge speed based on equipped weight.
				maxXYSpeed *= Util::InterpolateSmootherStep
				(
					1.0f,
					1.0f / dodgeVelRatio,
					std::clamp
					(
						dashDodgeEquippedWeight / 75.0f,
						0.0f,
						1.0f
					)
				);

				// Cannot dodge slower than the initial dodge speed.
				maxXYSpeed = max(dashDodgeInitialSpeed, maxXYSpeed);

				// Get dodge XY speed to set.
				// Dodge distance (directly influenced by speed) depends on LS displacement.
				float dashDodgeXYSpeed = 0.0f;
				// Burst of speed that peaks at the dodge midpoint.
				if (dashDodgeCompletionRatio <= 0.5f) 
				{
					dashDodgeXYSpeed = Util::InterpolateEaseOut
					(
						dashDodgeInitialSpeed,
						maxXYSpeed,
						dashDodgeCompletionRatio * 2.0f,
						2.0f
					) * dashDodgeLSDisplacement;
				}
				else
				{
					dashDodgeXYSpeed = Util::InterpolateEaseIn
					(
						maxXYSpeed,
						dashDodgeInitialSpeed,
						(dashDodgeCompletionRatio - 0.5f) * 2.0f,
						2.0f
					) * dashDodgeLSDisplacement;
				}

				// Convert back to havok units and then to hkVector4.
				havokVel = TohkVector4(dashDodgeDir * dashDodgeXYSpeed * GAME_TO_HAVOK);
				// Restore original Z component if paragliding or swimming.
				if (isParagliding || isSwimming) 
				{
					havokVel.quad.m128_f32[2] = zComp;
				}
				else if (charController->surfaceInfo.supportedState != 
						 RE::hkpSurfaceInfo::SupportedState::kSupported)
				{
					// Apply gravity to get downward velocity 
					// (gravity is not in effect if velocity was set directly while in the air).
					havokVel.quad.m128_f32[2] = -9.8f * secsSinceStartingDodge;
				}
				
				// NOTE:
				// Direct velocity changes are only possible
				// when the current state is in-air or flying.
				charController->wantState = 
				charController->context.currentState = RE::hkpCharacterStateType::kInAir;

				// Set new dodge velocity.
				charController->SetLinearVelocityImpl(havokVel);
				// Check if the player should land when dash dodging on the ground.
				if (!isParagliding && !isSwimming)
				{
					charController->flags.set
					(
						RE::CHARACTER_FLAGS::kCheckSupport, 
						RE::CHARACTER_FLAGS::kJumping
					);
				}

				// Update leaning.
				const float setupRatio = 
				(
					static_cast<float>(Settings::uDashDodgeSetupFrameCount) / 
					(
						Settings::uDashDodgeBaseAnimFrameCount + 
						Settings::uDashDodgeSetupFrameCount
					)
				);
				// Past the setup frames portion of the dodge.
				if (dashDodgeCompletionRatio >= setupRatio)
				{
					// Adjust player character tilt to lean in the direction of the dodge.
					const float animRatio = dashDodgeCompletionRatio - setupRatio;
					const float animCompletionRatio = 1.0f - setupRatio;
					const float halfAnimCompletionRatio = animCompletionRatio / 2.0f;
					float maxLeanAngle = 0.0f;
					if (isParagliding)
					{
						maxLeanAngle = p->isPlayer1 ? PI / 4.0f : PI / 2.0f;
					}
					else if (isSwimming)
					{
						// Lean less when swimming slowly.
						maxLeanAngle = Util::InterpolateSmootherStep
						(
							PI / 3.0f, 
							PI / 6.0f, 
							dashDodgeXYSpeed / Settings::fMaxDashDodgeSpeedmult
						);
					}
					else
					{
						// Lean less when moving fast (trip hazard).
						maxLeanAngle = Util::InterpolateSmootherStep
						(
							7.0f * PI / 18.0f, 
							PI / 6.0f, 
							dashDodgeXYSpeed / Settings::fMaxDashDodgeSpeedmult
						);
					}

					// Reference to the pitch/roll angle we want to modify.
					// We want to tilt the entire character controller when paragliding,
					// but only the torso nodes when on the ground or swimming.
					float& pitchToSet = 
					(
						isParagliding ? 
						charController->pitchAngle : 
						dashDodgeTorsoPitchOffset
					);
					float& rollToSet = 
					(
						isParagliding ? 
						charController->rollAngle : 
						dashDodgeTorsoRollOffset
					);
					// Endpoint to interp from in the first half of the interval 
					// and to interp to in the second half of the interval.
					// NOTE: 
					// Our changes to char controller angles are wiped by the game each frame.
					float endpointPitch = 0.0f;
					float endpointRoll = 0.0f;
					endpointPitch = 
					(
						animRatio < halfAnimCompletionRatio ? 
						pitchToSet : 
						0.0f
					);

					endpointRoll = 
					(
						animRatio < halfAnimCompletionRatio ? 
						rollToSet : 
						0.0f
					);

					if (isBackStepDodge)
					{
						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean back.
							// Flip the sign if setting the char controller tilt angles.
							pitchToSet = Util::InterpolateEaseOut
							(
								endpointPitch, 
								isParagliding || isSwimming ? -maxLeanAngle : maxLeanAngle, 
								(animRatio / halfAnimCompletionRatio),
								2.0f
							);
						}
						else
						{
							// Straighten back out by the time the dodge ends.
							// Flip the sign if setting the char controller tilt angles.
							pitchToSet = Util::InterpolateEaseIn
							(
								isParagliding || isSwimming ? -maxLeanAngle : maxLeanAngle,
								endpointPitch,
								(animRatio / halfAnimCompletionRatio) - 1.0f,
								2.0f
							);
						}
					}
					else
					{
						// Lean in direction of movement, 
						// a mix of pitch and roll angle modifications.
						float normSpeed = dashDodgeDir.Unitize();
						auto dashDodgeYaw = Util::DirectionToGameAngYaw(dashDodgeDir);
						// Backward (180 degree diff) if not moving.
						float movementToFacingYawDiff = 
						(
							normSpeed == 0.0f ? 
							PI : 
							Util::NormalizeAngToPi(dashDodgeYaw - coopActor->data.angle.z)
						);
						// Magnitude of the angle between the player's movement 
						// and facing angles.
						float absAngDiffMod = fmodf(fabsf(movementToFacingYawDiff), PI);
						// Proportion of the max lean angle to set as pitch.
						float pitchRatio = 1.0f;
						// 1 - the pitch ratio. The two ratios add up to 1.
						float rollRatio = 0.0f;
						float pitchSign = 1.0f;
						float rollSign = 1.0f;
						if (!isSwimming)
						{
							pitchRatio = 
							(
								absAngDiffMod <= PI / 2.0f ? 
								(1.0f - absAngDiffMod / (PI / 2.0f)) : 
								(absAngDiffMod / (PI / 2.0f) - 1.0f)
							);
							rollRatio = 1.0f - pitchRatio;
							pitchSign = fabsf(movementToFacingYawDiff) <= PI / 2.0f ? -1.0f : 1.0f;
							rollSign = movementToFacingYawDiff <= 0.0f ? -1.0f : 1.0f;
						}
						
						// Flip the sign if setting the char controller tilt angles.
						if (isParagliding || isSwimming) 
						{
							pitchSign *= -1.0f;
							rollSign *= -1.0f;
						}

						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean in the movement direction.
							pitchToSet = Util::InterpolateEaseOut
							(
								endpointPitch, 
								maxLeanAngle * pitchRatio * pitchSign, 
								(animRatio / halfAnimCompletionRatio), 
								2.0f
							);
							rollToSet = Util::InterpolateEaseOut
							(
								endpointRoll, 
								maxLeanAngle * rollRatio * rollSign, 
								(animRatio / halfAnimCompletionRatio), 
								2.0f
							);
						}
						else
						{
							// Straighten back out by the time the dodge ends.
							pitchToSet = Util::InterpolateEaseIn
							(
								maxLeanAngle * pitchRatio * pitchSign, 
								endpointPitch, 
								(animRatio / halfAnimCompletionRatio) - 1.0f,
								2.0f
							);
							rollToSet = Util::InterpolateEaseIn
							(
								maxLeanAngle * rollRatio * rollSign, 
								endpointRoll, 
								(animRatio / halfAnimCompletionRatio) - 1.0f, 
								2.0f
							);
						}
					}
				}

				++framesSinceStartingDashDodge;
			}

			++framesSinceRequestingDashDodge;
		}
		else if (isRequestingDashDodge)
		{
			const float frameScalingFactor = 1.0f / (60.0f * *g_deltaTimeRealTime);
			// Max number of seconds to wait until resetting data,
			// if a request was made and the player did not start dodging
			// or already completed their dodge.
			const float maxSecsToWait = 
			(
				2.0f * frameScalingFactor * 
				static_cast<float>
				(
					Settings::uDashDodgeSetupFrameCount + 
					Settings::uDashDodgeBaseAnimFrameCount
				) / 60.0f
			);
			if (p->pam->GetSecondsSinceLastStop(InputAction::kDodge) > maxSecsToWait)
			{
				// Failsafe to reset after twice the dodge duration 
				// and the dodge request was not handled.
				isRequestingDashDodge = false;
				p->pam->avcam->RemoveRequestedAction(AVCostAction::kDodge);
				p->pam->avcam->RemoveStartedAction(AVCostAction::kDodge);
				if (auto actorBase = coopActor->GetActorBase(); actorBase)
				{
					actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				}
			}
		}
	}

	void MovementManager::PerformJump()
	{
		// Jump. That's it.

		auto charController = coopActor->GetCharController();
		if (!charController)
		{
			return;
		}

		auto& currentHKPState = charController->context.currentState;
		// Number of frames to spend ascending to the apex of the jump.
		// Not less than 1.
		const uint32_t jumpAscentFramecount = max
		(
			1, 
			static_cast<uint32_t>
			(
				Settings::fSecsAfterGatherToFall * 
				(1.0f / *g_deltaTimeRealTime) + 0.5f
			)
		);
		// Start jump. Play gather animation(s) and invert gravity for the player.
		if (reqStartJump)
		{
			// Gotta move.
			SetDontMove(false);
			RE::hkVector4 velBeforeJumpVect{ };
			charController->GetLinearVelocityImpl(velBeforeJumpVect);
			if (velBeforeJumpVect.Length3() == 0.0f)
			{
				coopActor->NotifyAnimationGraph("JumpStandingStart");
			}
			else
			{
				coopActor->NotifyAnimationGraph("JumpDirectionalStart");
			}

			// Plain jump
			charController->lock.Lock();
			{
				charController->flags.set(RE::CHARACTER_FLAGS::kJumping);
				charController->context.currentState = RE::hkpCharacterStateType::kInAir;
				const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
				velBeforeJumpVect = RE::hkVector4
				(
					velBeforeJumpVect.quad.m128_f32[0] + 
					(
						GAME_TO_HAVOK *
						Settings::fJumpAdditionalLaunchSpeed *
						lsData.normMag *
						cosf(Util::ConvertAngle(movementOffsetParams[!MoveParams::kLSGameAng]))
					),
					velBeforeJumpVect.quad.m128_f32[1] + 
					(
						GAME_TO_HAVOK *
						Settings::fJumpAdditionalLaunchSpeed *
						lsData.normMag *
						sinf(Util::ConvertAngle(movementOffsetParams[!MoveParams::kLSGameAng]))
					),
					havokInitialJumpZVelocity + 
					velBeforeJumpVect.quad.m128_f32[2] +
					(
						GAME_TO_HAVOK *
						Settings::fJumpAdditionalLaunchSpeed
					),
					0.0f
				);

				// Invert gravity and set initial velocity.
				charController->gravity = -Settings::fJumpingGravityMult;
				charController->SetLinearVelocityImpl(velBeforeJumpVect);
			}
			charController->lock.Unlock();

			// Jump has started.
			framesSinceStartingJump = 1;
			isAirborneWhileJumping = true;
			isFallingWhileJumping = false;
			sentJumpFallEvent = false;
			reqStartJump = false;
			p->jumpStartTP = SteadyClock::now();
		}
		else if (isAirborneWhileJumping)
		{
			// Abort jump if ragdolling.
			if (coopActor->IsInRagdollState())
			{
				// Reset gravity and jump state variables,
				// plus set fall start height and time.
				charController->lock.Lock();
				{
					charController->flags.reset(RE::CHARACTER_FLAGS::kJumping);
					charController->gravity = 1.0f;
					RE::hkVector4 havokPos{};
					charController->GetPositionImpl(havokPos, false);
					float zPos = havokPos.quad.m128_f32[2] * HAVOK_TO_GAME;
					charController->fallStartHeight = zPos;
					charController->fallTime = 0.0f;
				}
				charController->lock.Unlock();

				isAirborneWhileJumping = false;
				isFallingWhileJumping = false;
				reqStartJump = false;
				sentJumpFallEvent = false;

				p->jumpStartTP = SteadyClock::now();
				framesSinceStartingJump = 0;
				return;
			}
			else if (p->mm->isParagliding)
			{
				// Reset gravity and jump state variables.
				charController->lock.Lock();
				{
					charController->flags.reset
					(
						RE::CHARACTER_FLAGS::kJumping
					);
					charController->gravity = 1.0f;
				}
				charController->lock.Unlock();

				isAirborneWhileJumping = false;
				isFallingWhileJumping = false;
				reqStartJump = false;
				sentJumpFallEvent = false;

				p->jumpStartTP = SteadyClock::now();
				framesSinceStartingJump = 0;
				return;
			}

			// Handle ascent to peak of the jump at which the player begins to fall.
			if (!isFallingWhileJumping)
			{
				isFallingWhileJumping = framesSinceStartingJump >= jumpAscentFramecount;
				charController->lock.Lock();
				{
					// Zero gravity at apex.
					charController->gravity = Util::InterpolateEaseIn
					(
						-Settings::fJumpingGravityMult,
						0.0f,
						static_cast<float>(framesSinceStartingJump) / jumpAscentFramecount,
						2.0f
					);
				}
				charController->lock.Unlock();
				framesSinceStartingJump++;
			}
			else
			{
				// Only send the fall animation, which cancels all melee/ranged attack animations,
				// if the player is not attacking or casting.
				bool startedAttackAfterJumping = 
				(
					Util::GetElapsedSeconds(p->lastAttackStartTP) <
					p->pam->GetSecondsSinceLastStart(InputAction::kJump)
				);
				if (!sentJumpFallEvent && !startedAttackAfterJumping && !p->pam->isAttacking)
				{
					charController->lock.Lock();
					{
						charController->flags.reset(RE::CHARACTER_FLAGS::kJumping);
						// Set fall start time and height.
						RE::hkVector4 havokPos{};
						charController->GetPositionImpl(havokPos, false);
						float zPos = havokPos.quad.m128_f32[2] * HAVOK_TO_GAME;
						charController->fallStartHeight = zPos;
						charController->fallTime = 0.0f;
					}
					charController->lock.Unlock();
					coopActor->NotifyAnimationGraph("JumpFall");
					sentJumpFallEvent = true;
				}
					
				// Check if the player has landed, reset state, and return early.
				bool canLand = 
				(
					(
						charController->flags.all
						(
							RE::CHARACTER_FLAGS::kCanJump, 
							RE::CHARACTER_FLAGS::kSupport
						)
					) && 
					(
						charController->context.currentState == 
						RE::hkpCharacterStateType::kOnGround &&
						charController->surfaceInfo.supportedState.get() != 
						RE::hkpSurfaceInfo::SupportedState::kUnsupported
					) 
				);
				// Have to check for a collidable surface under the player 
				// with a single raycast, since the char controller flags and surface info
				// sometimes indicate the player can land while they are still in midair.
				if (canLand)
				{
					glm::vec4 start =
					{
						coopActor->data.location.x,
						coopActor->data.location.y,
						coopActor->data.location.z + coopActor->GetHeight(),
						0.0f
					};
					glm::vec4 end = 
					(
						start - glm::vec4(0.0f, 0.0f, 1.25f * coopActor->GetHeight(), 0.0f)
					);
					auto result = Raycast::hkpCastRay
					(
						start, 
						end, 
						std::vector<RE::TESObjectREFR*>({ coopActor.get() }),
						std::vector<RE::FormType>
						(
							{ RE::FormType::Activator, RE::FormType::TalkingActivator }
						)
					);
					// No surface beneath the player, so they cannot land.
					if (!result.hit)
					{
						canLand = false;
					}
				}

				if (canLand)
				{
					// Reset jump state variables.
					charController->lock.Lock();
					{
						charController->flags.reset(RE::CHARACTER_FLAGS::kJumping);
						charController->gravity = 1.0f;
						// Set fall start time and height.
						RE::hkVector4 havokPos{};
						charController->GetPositionImpl(havokPos, false);
						float zPos = havokPos.quad.m128_f32[2] * HAVOK_TO_GAME;
						charController->fallStartHeight = zPos;
						charController->fallTime = 0.0f;
					}
					charController->lock.Unlock();

					isAirborneWhileJumping = false;
					isFallingWhileJumping = false;
					reqStartJump = false;

					// Have to manually trigger the landing animation 
					// to minimize occurrences of the hovering bug.
					// No more 'Surf's up, dude!'.
					coopActor->NotifyAnimationGraph("JumpLand");
					charController->lock.Lock();
					{
						charController->surfaceInfo.surfaceNormal = RE::hkVector4(0.0f);
						charController->surfaceInfo.surfaceDistanceExcess = 0.0f;
						charController->surfaceInfo.supportedState = 
						RE::hkpSurfaceInfo::SupportedState::kSupported;
					}
					charController->lock.Unlock();
					// Update jump start TP on landing.
					p->jumpStartTP = SteadyClock::now();
					framesSinceStartingJump = 0;

					return;
				}

				// Continue falling.
				charController->lock.Lock();
				{
					charController->gravity = Util::InterpolateEaseIn
					(
						0.0f, 
						Settings::fJumpingGravityMult,
						(
							max(framesSinceStartingJump - jumpAscentFramecount, 0.0f) /
							jumpAscentFramecount
						),
						2.0f
					);
				}
				charController->lock.Unlock();
				framesSinceStartingJump++;
			}
		}
	}

	void MovementManager::PerformMagicalParaglide()
	{
		// Companion players' counterpart to P1's paraglider, 
		// if the 'Skyrim's Paraglider' mod is installed
		// and P1 has obtained a paraglider.
		// Now with 1000% more jank and less polish.
		// The paraglider's there in spirit, I promise.
		// All credit goes to Loki:
		// https://github.com/LXIV-CXXVIII/Skyrims-Paraglider/blob/main/src/main.cpp
		
		// Nothing else to do for P1.
		if (p->isPlayer1 ||
			!ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled || 
			!ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider)
		{
			return;
		}

		auto charController = coopActor->GetCharController();
		if (!charController) 
		{
			// Stop art effects and reset data if the char controller is invalid.
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect1);
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect2);
			shouldParaglide = false;
			isParagliding = false;
			magicParaglideVelInterpFactor = magicParaglideEndZVel = magicParaglideStartZVel = 0.0f;
			p->lastParaglidingStateChangeTP = SteadyClock::now();
			return;
		}

		bool isAirborne = 
		(
			charController->context.currentState == RE::hkpCharacterStateType::kInAir
		);
		// Reset paragliding request flag if now on the ground.
		if (!isAirborne && (shouldParaglide || isParagliding))
		{
			// Set fall height once done paragliding.
			RE::hkVector4 havokPos{};
			charController->GetPositionImpl(havokPos, false);
			float zPos = havokPos.quad.m128_f32[2] * HAVOK_TO_GAME;
			charController->fallStartHeight = zPos;
			charController->fallTime = 0.0f;
			// Stop art effects.
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect1);
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect2);
			// Reset all data.
			magicParaglideVelInterpFactor = magicParaglideEndZVel = magicParaglideStartZVel = 0.0f;
			isParagliding = shouldParaglide = isParaglidingTiltAngleReset = false;
			p->lastParaglidingStateChangeTP = SteadyClock::now();
		}

		// Start/continue pseudo-paragliding.
		if (shouldParaglide)
		{
			RE::hkVector4 havokVel;
			charController->GetLinearVelocityImpl(havokVel);

			// Start art effects and adjust velocity.
			if (!isParagliding) 
			{
				Util::StartHitArt
				(
					coopActor.get(), glob.paraglideIndicatorEffect1, coopActor.get()
				);
				Util::StartHitArt
				(
					coopActor.get(), glob.paraglideIndicatorEffect2, coopActor.get()
				);

				// Set starting Z velocity.
				magicParaglideStartZVel = havokVel.quad.m128_f32[2];
				// Is now paragliding.
				isParagliding = true;
				p->lastParaglidingStateChangeTP = SteadyClock::now();
			}

			// Make sure the player is continuously falling while paragliding.
			// NOTE: 
			// Game attempts to land the player periodically, even when in the air,
			// and no related animation event to catch is sent via the NotifyAnimationGraph() hook.
			// Sending the fall animation each frame cancels the landing animation,
			// but still leads to a 'hiccup' whenever the game tries to land the player.
			if (coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal) 
			{
				coopActor->NotifyAnimationGraph("JumpFallDirectional");
			}

			// No fall damage while still paragliding.
			charController->fallStartHeight = 0.0f;
			charController->fallTime = 0.0f;

			// Hardcoded defaults for now.
			// Will read from the paraglide config file later.
			// Default fall speed.
			magicParaglideEndZVel = -2.3f;
			// Rise while gale is active.
			if (glob.tarhielsGaleEffect && coopActor->HasMagicEffect(glob.tarhielsGaleEffect))
			{
				// Gale speed.
				magicParaglideEndZVel = 15.0f;
			}

			auto newHavokZVel = std::lerp
			(
				magicParaglideStartZVel, magicParaglideEndZVel, magicParaglideVelInterpFactor
			);
			if (magicParaglideVelInterpFactor < 1.0f)
			{
				(
					glob.tarhielsGaleEffect && coopActor->HasMagicEffect(glob.tarhielsGaleEffect) ? 
					magicParaglideVelInterpFactor += 0.01f : 
					magicParaglideVelInterpFactor += 0.025f
				);
			}

			// Set the new Z component and apply the velocity.
			havokVel.quad.m128_f32[2] = newHavokZVel;
			charController->SetLinearVelocityImpl(havokVel);
			
			// Tilt the character controller in the player's gliding direction 
			// when not dash dodging.
			if (!isDashDodging) 
			{
				const float maxTiltAngle = PI / 2.0f;
				RE::NiPoint3 linVelXY = RE::NiPoint3
				(
					charController->outVelocity.quad.m128_f32[0], 
					charController->outVelocity.quad.m128_f32[1], 
					0.0f
				);
				float normXYSpeed = linVelXY.Unitize();
				// Proportion of the max lean angle to set as pitch or roll.
				// The two ratios add to 1.
				float pitchRatio = 0.0f;
				float pitchSign = 1.0f;
				float rollRatio = 0.0f;
				float rollSign = 1.0f;
				// Remain upright if not moving.
				if (normXYSpeed != 0.0f)
				{
					auto linVelYaw = Util::DirectionToGameAngYaw(linVelXY);
					// Difference between the player's moving and facing angles.
					float movementToFacingYawDiff = Util::NormalizeAngToPi
					(
						linVelYaw - coopActor->data.angle.z
					);
					float absAngDiffMod = fmodf(fabsf(movementToFacingYawDiff), PI);
					pitchRatio = 
					(
						absAngDiffMod <= PI / 2.0f ? 
						(1.0f - absAngDiffMod / (PI / 2.0f)) : 
						(absAngDiffMod / (PI / 2.0f) - 1.0f)
					);
					rollRatio = 1.0f - pitchRatio;
					pitchSign = fabsf(movementToFacingYawDiff) <= PI / 2.0f ? 1.0f : -1.0f;
					rollSign = movementToFacingYawDiff <= 0.0f ? 1.0f : -1.0f;
				}

				charController->pitchAngle = Util::InterpolateEaseInEaseOut
				(
					charController->pitchAngle,
					maxTiltAngle * pitchSign * pitchRatio, 
					0.25f, 
					2.0f
				);
				charController->rollAngle = Util::InterpolateEaseInEaseOut
				(
					charController->rollAngle,
					maxTiltAngle * rollSign * rollRatio,
					0.25f,
					2.0f
				);
			}

			// Not resetting tilt while paragliding.
			isParaglidingTiltAngleReset = false;
		}
		else
		{
			// Stop paragliding.
			if (isParagliding) 
			{
				Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect1);
				Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect2);

				// Set fall height once done paragliding.
				RE::hkVector4 havokPos{};
				charController->GetPositionImpl(havokPos, false);
				float zPos = havokPos.quad.m128_f32[2] * HAVOK_TO_GAME;
				charController->fallStartHeight = zPos;
				charController->fallTime = 0.0f;

				// Reset interp factor, end, and start speed.
				magicParaglideVelInterpFactor = 0.0f;
				magicParaglideEndZVel = magicParaglideStartZVel = 0.0f;
				// Target char controller pitch not reset yet.
				isParaglidingTiltAngleReset = false;
				// Is not paragliding.
				isParagliding = false;
				p->lastParaglidingStateChangeTP = SteadyClock::now();
			}


			// Rotate char controller back to the upright position.
			charController->pitchAngle = Util::InterpolateEaseInEaseOut
			(
				charController->pitchAngle, 0.0f, 0.25f, 2.0f
			);
			charController->rollAngle = Util::InterpolateEaseInEaseOut
			(
				charController->rollAngle, 0.0f, 0.25f, 2.0f
			);
			// Close enough to 0 to set directly.
			isParaglidingTiltAngleReset = 
			(
				charController->pitchAngle < 1e-5f && charController->rollAngle < 1e-5f
			);
			if (isParaglidingTiltAngleReset)
			{
				charController->pitchAngle = charController->rollAngle = 0.0f;
			}
		}
	}

	void MovementManager::ResetTPs()
	{
		// Reset all player timepoints handled by this manager to the current time.

		p->jumpStartTP					=
		p->lastDashDodgeTP				=
		p->lastGetupTP					=
		p->lastMovementStartReqTP		=
		p->lastMovementStopReqTP		=
		p->lastParaglidingStateChangeTP = SteadyClock::now();
	}

	void MovementManager::SetAimRotation()
	{
		// Set the player's aiming graph variables, 
		// and rotate the player's magic casters to face the current target.
		
		// Need to retrieve nodes from the player's 3D.
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings || !coopActor->loadedData || !coopActor->loadedData->data3D)
		{
			return;
		}

		const auto& data3D = coopActor->loadedData->data3D;
		// Head and LH/RH magic nodes.
		auto headMagicNode = data3D->GetObjectByName(strings->npcHeadMagicNode);
		auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>
		(
			data3D->GetObjectByName(strings->npcLMagicNode)
		);
		auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>
		(
			data3D->GetObjectByName(strings->npcRMagicNode)
		);

		bool isCastingDual = false;
		bool isCastingLH = false;
		bool isCastingRH = false;
		auto instantCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		bool isCastingQS = 
		{
			p->pam->IsPerforming(InputAction::kQuickSlotCast) &&
			instantCaster &&
			instantCaster->state.none(RE::MagicCaster::State::kNone)
		};
		coopActor->GetGraphVariableBool("IsCastingDual", isCastingDual);
		coopActor->GetGraphVariableBool("IsCastingLeft", isCastingLH);
		coopActor->GetGraphVariableBool("IsCastingRight", isCastingRH);

		// Make sure the magic casters are active, 
		// in addition to having the casting animation playing.
		auto lhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
		isCastingLH &= 
		(
			lhCaster &&
			lhCaster->state.none(RE::MagicCaster::State::kNone)
		);
		isCastingLH |= isCastingDual || p->pam->usingLHStaff->value == 1.0f;
		auto rhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
		isCastingRH &= 
		(
			rhCaster &&
			rhCaster->state.none(RE::MagicCaster::State::kNone)
		);
		isCastingRH |= isCastingDual || p->pam->usingRHStaff->value == 1.0f;

		// Set node's rotation to face the target position.
		auto directNodeAtTarget = 
		[this]
		(
			const RE::MagicSystem::CastingSource&& a_source,
			const RE::TESObjectREFRPtr& a_targetPtr, 
			const RE::NiPoint3& a_targetPos
		) 
		{
			// By default, choose aim pitch direction if there is no target.
			std::pair<float, float> pitchYawPair{ aimPitch, coopActor->data.angle.z };
			if (auto magicCaster = coopActor->GetMagicCaster(a_source); magicCaster)
			{
				auto magNodePtr = RE::NiPointer<RE::NiAVObject>(magicCaster->GetMagicNode());
				if (!magNodePtr || !magNodePtr.get()) 
				{
					return;
				}

				RE::NiPoint3 forward{ 0.0f, 1.0f, 0.0f };	
				// Get pitch/yaw to the target position.
				if (a_targetPtr || reqFaceTarget)
				{
					float pitch = Util::GetPitchBetweenPositions
					(
						magNodePtr->world.translate, a_targetPos
					);
					float yaw = Util::GetYawBetweenPositions
					(
						magNodePtr->world.translate, a_targetPos
					);
					pitchYawPair.first = pitch;
					pitchYawPair.second = yaw;
				}

				Util::SetRotationMatrixPY
				(
					magNodePtr->world.rotate, pitchYawPair.first, pitchYawPair.second
				);
			}
		};

		// Not sure if changing the casting direction individually for each
		// casting node is possible, so for now, all that can be done 
		// is changing the player's aim angle graph variables. 
		// Obvisously this means that dual casting will be less accurate
		// as both nodes will cast at the same angle, 
		// irrespective of their individual rotations to the targeted location.

		uint8_t activeNodesCount = 0;
		std::pair<float, float> avgPitchYawPair{ 0.0f, 0.0f };
		const RE::NiPoint3 forward{ 0.0f, 1.0f, 0.0f };

		// Initially target the crosshair world position.
		auto targetPos = p->tm->crosshairWorldPos;
		auto targetPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
		bool targetValidity = 
		(
			targetPtr && targetPtr.get() && Util::IsValidRefrForTargeting(targetPtr.get())
		);
		if (!targetValidity && Settings::vbUseAimCorrection[playerID])
		{
			// Set to aim correction target's torso position.
			targetPtr = Util::GetRefrPtrFromHandle(p->tm->aimCorrectionTargetHandle); 
			targetValidity = 
			(
				targetPtr && targetPtr.get() && Util::IsValidRefrForTargeting(targetPtr.get())
			);
			if (targetValidity)
			{
				targetPos = Util::GetTorsoPosition(targetPtr->As<RE::Actor>());
			}
		}

		// Clear target if it is not valid, so that we don't aim at it
		// when directing the node at the target position below.
		if (!targetValidity) 
		{
			targetPtr = nullptr;
		}

		// No target and not facing the crosshair position,
		// so use default aim direction pitch/yaw.
		bool useAimPitchPos = !targetValidity && !reqFaceTarget;
		if (useAimPitchPos)
		{
			avgPitchYawPair = { aimPitch, coopActor->data.angle.z };
			targetPos = aimPitchPos;
		}

		// Add up casting nodes' pitch and yaw angles to the target position.
		// Keep track of how many nodes are active.
		// Will average the angles afterward before setting the corresponding graph variables.
		auto accumulateAvgPitchYaw = 
		[
			this, &avgPitchYawPair, &activeNodesCount, &useAimPitchPos, &targetPos, strings
		]
		(RE::MagicSystem::CastingSource&& a_source) 
		{
			auto actorMagicCaster = coopActor->magicCasters[!a_source]; 
			if (actorMagicCaster && actorMagicCaster->magicNode)
			{
				if (!useAimPitchPos) 
				{
					avgPitchYawPair.first += Util::GetPitchBetweenPositions
					(
						actorMagicCaster->magicNode->world.translate, targetPos
					);
					avgPitchYawPair.second += Util::GetYawBetweenPositions
					(
						actorMagicCaster->magicNode->world.translate, targetPos
					);
				}

				glm::vec3 start = ToVec3(actorMagicCaster->magicNode->world.translate);
				glm::vec3 offset = ToVec3
				(
					actorMagicCaster->magicNode->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f)
				);

				++activeNodesCount;
				return;
			}
				
			bool valid3D = coopActor->loadedData && coopActor->loadedData->data3D;
			if (!valid3D)
			{
				return;
			}
			
			// Just in case if the caster node is not available, 
			// even though the instant caster is casting our quick slot spell.
			auto headMagNodePtr = RE::NiPointer<RE::NiAVObject>
			(
				coopActor->loadedData->data3D->GetObjectByName(strings->npcHeadMagicNode)
			); 
			if (headMagNodePtr && 
				headMagNodePtr.get() && 
				p->pam->IsPerforming(InputAction::kQuickSlotCast))
			{
				if (!useAimPitchPos)
				{
					avgPitchYawPair.first += Util::GetPitchBetweenPositions
					(
						headMagNodePtr->world.translate, targetPos
					);
					avgPitchYawPair.second += Util::GetYawBetweenPositions
					(
						headMagNodePtr->world.translate, targetPos
					);
				}

				++activeNodesCount;
				return;
			}
			else 
			{
				// Just in case if the head magic node is not available,
				// even though the instant caster is casting our quick slot spell.
				auto lookNodePtr = RE::NiPointer<RE::NiAVObject>
				(
					coopActor->loadedData->data3D->GetObjectByName(strings->npcLookNode)
				); 
				if (!lookNodePtr || 
					!lookNodePtr.get() || 
					!p->pam->IsPerforming(InputAction::kQuickSlotCast))
				{
					return;
				}

				if (!useAimPitchPos)
				{
					avgPitchYawPair.first += Util::GetPitchBetweenPositions
					(
						lookNodePtr->world.translate, targetPos
					);
					avgPitchYawPair.second += Util::GetYawBetweenPositions
					(
						lookNodePtr->world.translate, targetPos
					);
				}

				++activeNodesCount;
			}
		};

		// Since co-op companions do not have staff casting animations,
		// and thus do not use their AimPitch/Heading animation variables,
		// direct the casting magic node on the staff at the target.
		bool hasSpell = p->em->HasLHSpellEquipped();
		bool hasStaff = p->em->HasLHStaffEquipped(); 
		if ((isCastingLH) && (hasSpell || hasStaff))
		{
			accumulateAvgPitchYaw(RE::MagicSystem::CastingSource::kLeftHand);
			if (!p->isPlayer1 && hasStaff) 
			{
				directNodeAtTarget
				(
					RE::MagicSystem::CastingSource::kLeftHand, targetPtr, targetPos
				);
			}
		}

		hasSpell = p->em->HasRHSpellEquipped(); 
		hasStaff = p->em->HasRHStaffEquipped(); 
		if ((isCastingRH) && (hasSpell || hasStaff))
		{
			accumulateAvgPitchYaw(RE::MagicSystem::CastingSource::kRightHand);
			if (!p->isPlayer1 && hasStaff)
			{
				directNodeAtTarget
				(
					RE::MagicSystem::CastingSource::kRightHand, targetPtr, targetPos
				);
			}
		}

		if (auto qsSpell = p->em->quickSlotSpell; qsSpell && isCastingQS)
		{
			accumulateAvgPitchYaw(RE::MagicSystem::CastingSource::kInstant);
		}

		// Enables aim adjustment for the head magic node.
		coopActor->SetGraphVariableBool("bAimActive", true);
		if (activeNodesCount > 0) 
		{
			// Divide by total active nodes to get the average.
			if (!useAimPitchPos) 
			{
				avgPitchYawPair.first /= activeNodesCount;
				avgPitchYawPair.second /= activeNodesCount;
			}

			// Sign is opposite of the player pitch/yaw angle.
			coopActor->SetGraphVariableFloat("AimPitchCurrent", -avgPitchYawPair.first);
			// Is offset from the player's facing direction.
			coopActor->SetGraphVariableFloat
			(
				"AimHeadingCurrent",
				-Util::NormalizeAngToPi(avgPitchYawPair.second - coopActor->data.angle.z)
			);

			glm::vec3 start = ToVec3(headMagicNode->world.translate);
			glm::vec3 offset = ToVec3
			(
				Util::RotationToDirectionVect
				(
					-avgPitchYawPair.first, Util::ConvertAngle(avgPitchYawPair.second)
				)
			);
		}
		else
		{
			// No active nodes, so adjust aim pitch to our custom pitch.
			coopActor->SetGraphVariableFloat("AimPitchCurrent", -aimPitch);
		}
	}

	void MovementManager::SetDontMove(bool&& a_set)
	{
		// Prevent or allow movement for the player or their mount,
		// based on player movement speed and LS position (centered or not).

		auto mount = p->GetCurrentMount();
		if (a_set) 
		{
			// Set once to stop the mount from moving.
			if (!dontMoveSet && mount && mount.get())
			{
				Util::NativeFunctions::SetDontMove(mount.get(), true);
			}

			// Send move stop animation when the player is not in a killmove
			// and has not been told to stop while still moving.
			// Keeping commented out for now to see if everything still works fine without it.
			if (!coopActor->IsInKillMove() && !dontMoveSet)
			{
				movementActor->NotifyAnimationGraph("moveStop");
			}

			// Set once.
			if (!dontMoveSet) 
			{
				Util::NativeFunctions::SetDontMove(coopActor.get(), true);
			}

			dontMoveSet = true;
		}
		else
		{
			// Set once to allow the mount to move.
			if (dontMoveSet && mount && mount.get()) 
			{
				Util::NativeFunctions::SetDontMove(mount.get(), false);
			}

			// Allow the player to move if they were stopped.
			if (dontMoveSet)
			{
				Util::NativeFunctions::SetDontMove(coopActor.get(), false);
			}

			if ((!coopActor->IsInKillMove()) && ((shouldStartMoving) || (dontMoveSet && lsMoved)))
			{
				// If not in a killmove and signalled to start moving 
				// or stopped moving but the LS is moved,
				// send the move start animation event.
				// Stop any playing idles as well before moving.
				movementActor->NotifyAnimationGraph("IdleStopInstant");
				movementActor->NotifyAnimationGraph("moveStart");
			}
			else if (!lsMoved && 
					 !isDashDodging && 
					 !isRequestingDashDodge && 
					 !isParagliding && 
					 !interactionPackageRunning && 
					 dontMoveSet)
			{
				// If the LS is centered and the player has been stopped 
				// and is not dash dodging, paragliding, or interacting with an object, 
				// send the move stop animation event.
				// Player could still be rotating in place here, but they should not move
				// from their current spot.
				//movementActor->NotifyAnimationGraph("moveStop");
			}

			dontMoveSet = false;
		}
	}

	void MovementManager::SetHeadTrackTarget()
	{
		// Set the context-dependent position which should be used 
		// as the player's headtracking target.

		bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
		if (!onlyAlwaysOpen)
		{
			return;
		}

		// Don't set while in dialogue.
		auto currentProc = coopActor->currentProcess; 
		if (!currentProc || !currentProc->high || p->pam->isInDialogue)
		{
			return;
		}

		if (coopActor->IsAttacking()) 
		{
			// While attacking, if targeting an actor while not facing them,
			// look at the actor's torso; otherwise look at the crosshair world position.
			auto rangedTargetActorPtr = Util::GetActorPtrFromHandle(p->tm->GetRangedTargetActor());
			bool lookAtTorso = 
			(
				!reqFaceTarget && rangedTargetActorPtr && rangedTargetActorPtr.get()
			);
			if (lookAtTorso)
			{
				auto torsoPos = Util::GetTorsoPosition(rangedTargetActorPtr.get());
				// Only look at the target's torso if it is on screen.
				if (torsoPos != RE::NiPoint3() && Util::PointIsOnScreen(torsoPos))
				{
					currentProc->SetHeadtrackTarget(coopActor.get(), torsoPos);
				}
				else
				{
					currentProc->SetHeadtrackTarget(coopActor.get(), p->tm->crosshairWorldPos);
				}
			}
			else
			{
				currentProc->SetHeadtrackTarget(coopActor.get(), p->tm->crosshairWorldPos);
			}
		}
		else
		{
			if (p->pam->IsPerforming(InputAction::kActivate))
			{
				// Look at activation target.
				auto interactionTargetPtr = Util::GetRefrPtrFromHandle
				(
					p->tm->activationRefrHandle
				);
				bool interactionTargetvalid = 
				(
					interactionTargetPtr &&
					interactionTargetPtr.get() && 
					Util::IsValidRefrForTargeting(interactionTargetPtr.get()) && 
					interactionTargetPtr->parentCell->IsAttached()
				);
				if (!interactionTargetvalid)
				{
					return;
				}

				if (interactionTargetPtr->Is(RE::FormType::ActorCharacter, RE::FormType::NPC))
				{
					// Look at NPC's eyes. Must be on-screen.
					auto targetEyePos = 
					(
						interactionTargetPtr->As<RE::Actor>()->GetLookingAtLocation()
					);
					if (Util::PointIsOnScreen(targetEyePos))
					{
						currentProc->SetHeadtrackTarget(coopActor.get(), targetEyePos);
					}
				}
				else
				{
					// Only look at selectable objects.
					if (!Util::IsSelectableRefr(interactionTargetPtr.get()))
					{
						return;
					}
						
					// Looking at in-air projectiles sometimes causes a crash,
					// so avert your eyes.
					auto baseObj = interactionTargetPtr->GetBaseObject();
					if (!baseObj || baseObj->Is(RE::FormType::Projectile))
					{
						return;
					}
						
					// Look at object's center if selectable.
					auto targetCenter = Util::Get3DCenterPos(interactionTargetPtr.get());
					if (Util::PointIsOnScreen(targetCenter))
					{
						currentProc->SetHeadtrackTarget(coopActor.get(), targetCenter);
					}
				}
			}
			else
			{
				const auto targetActorPtr = Util::GetActorPtrFromHandle
				(
					p->tm->GetRangedTargetActor()
				); 
				if (targetActorPtr &&
					targetActorPtr.get() && 
					Util::IsValidRefrForTargeting(targetActorPtr.get()))
				{
					// Look at the targeted actor's eyes if on-screen.
					auto targetEyePos = targetActorPtr->GetLookingAtLocation();
					if (Util::PointIsOnScreen(targetEyePos))
					{
						currentProc->SetHeadtrackTarget(coopActor.get(), targetEyePos);
					}
				}
				else 
				{
					const auto targetRefrPtr = 
					(
						Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle)
					); 
					// Not valid.
					if (!targetRefrPtr ||
						!targetRefrPtr.get() ||
						!Util::IsValidRefrForTargeting(targetRefrPtr.get()))
					{
						return;
					}

					// Look at the crosshair refr's center if on-screen.
					auto targetCenter = Util::Get3DCenterPos(targetRefrPtr.get());
					if (Util::PointIsOnScreen(targetCenter))
					{
						currentProc->SetHeadtrackTarget(coopActor.get(), targetCenter);
					}
				}
			}
		}
	}

	void MovementManager::SetPlayerOrientation()
	{
		// Set the player/mount's rotation and keep/clear a movement offset
		// from the player/mount themselves to allow them to start/stop moving.
	 	
		//===========
		// [Rotation]
		//===========
		// Normalize current facing angle first.
		movementActor->data.angle.z = Util::NormalizeAng0To2Pi(movementActor->data.angle.z);
		// Get rotation multipluer from base and context-dependent rotation speed multipliers.
		float rotMult = Settings::fBaseRotationMult * GetRotationMult();
		// The target yaw angle to set for the player or their mount.
		float playerTargetYaw = movementActor->data.angle.z;
		// Can set rotation if AI driven, no menu is open that stops movement,
		// not animation driven, not in a synced animation, 
		// not mounting, not in a killmove, and not staggered.
		if (!menuStopsMovement && 
			!isAnimDriven && 
			!isSynced && 
			!isMounting && 
			!coopActor->IsInKillMove() && 
			coopActor->actorState1.knockState == RE::KNOCK_STATE_ENUM::kNormal)
		{
			// Target actor -- either crosshair-selected or downed player.
			RE::ActorPtr targetPtr = Util::GetActorPtrFromHandle(p->tm->GetRangedTargetActor());
			// Prioritize the downed player.
			if (p->isRevivingPlayer && p->pam->downedPlayerTarget)
			{
				targetPtr = p->pam->downedPlayerTarget->coopActor;
			}

			// Default to targeting the aim pitch position.
			auto targetLocation = aimPitchPos;
			if (!p->isRevivingPlayer)
			{
				if (targetPtr && targetPtr->GetHandle() == p->tm->aimCorrectionTargetHandle)
				{
					// Face the aim correction target's torso.
					targetLocation = Util::GetTorsoPosition(targetPtr.get());
				}
				else
				{
					// Face the crosshair position otherwise.
					targetLocation = p->tm->crosshairWorldPos;
				}
			}
			else if (targetPtr)
			{
				// Face the downed player's torso.
				targetLocation = Util::GetTorsoPosition(targetPtr.get());
			}

			bool isTKDodging = false;
			bool isTDMDodging = false;
			coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
			coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
			auto crosshairTargetPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
			bool crosshairTargetValidity = 
			(
				crosshairTargetPtr && Util::IsValidRefrForTargeting(crosshairTargetPtr.get())
			);
			float yawToTarget = Util::DirectionToGameAngYaw
			(
				targetLocation - coopActor->data.location
			);


			// Save old turn to/face target state to record changes.
			bool oldFaceTarget = faceTarget;
			bool oldTurnToTarget = turnToTarget;

			// Stay facing the target position.
			// Conditions:
			// 1. FaceTarget toggled on.
			// 2. Not reviving another player.
			// 3. Not dodging.
			// 4. Not mounted.
			// 5. Not swimming.
			// 6. Not sprinting and not sneak rolling.
			// 7. No crosshair target or not trying to face grabbed/released target.
			faceTarget = 
			{
				(
					reqFaceTarget && 
					!p->isRevivingPlayer && 
					!isTKDodging && 
					!isTDMDodging && 
					!coopActor->IsOnMount() && 
					!coopActor->IsSwimming() && 
					!p->pam->isSprinting
				) && 
				(
					!coopActor->IsSneaking() || 
					!p->pam->IsPerforming(InputAction::kSprint)
				) &&
				(
					(!crosshairTargetValidity) || 
					(
						!p->tm->rmm->IsGrabbed(p->tm->crosshairRefrHandle) && 
						!p->tm->rmm->IsReleased(p->tm->crosshairRefrHandle)
					)
				)
			};

			// Check if the player is using their weapons/magic.
			bool isUsingWeapMag = 
			{ 
				(p->pam->isAttacking || p->pam->isBlocking ||
				 p->pam->isBashing || p->pam->isInCastingAnim) 
			};

			// Secondary check to see if the player is pressing 
			// binds for combat-related player actions.
			if (!isUsingWeapMag && coopActor->IsWeaponDrawn())
			{
				const auto& combatGroup = 
				(
					glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at(ActionGroup::kCombat)
				);
				for (auto actionIndex : combatGroup)
				{
					isUsingWeapMag |= 
					(
						p->pam->IsPerforming(static_cast<InputAction>(actionIndex))
					);
					if (isUsingWeapMag)
					{
						break;
					}
				}
			}

			// If not facing the target at all times,
			// the player will still temporarily face the selected target 
			// if performing certain actions.
			// Conditions:
			// 1. Not always facing the target.
			// 2. Not dodging.
			// 3. Not mounted.
			// 4. Not swimming.
			// 5. Reviving another player or using weapons/magic 
			// with a valid target while not sprinting.
			turnToTarget = 
			{ 
				(
					!faceTarget && 
					!isTKDodging && 
					!isTDMDodging && 
					!isDashDodging && 
					!coopActor->IsOnMount() &&
					!coopActor->IsSwimming()
				) &&
				(
					(p->isRevivingPlayer) || 
					(
						isUsingWeapMag && 
						!p->pam->IsPerforming(InputAction::kSprint) &&
						targetPtr && 
						Util::IsValidRefrForTargeting(targetPtr.get())
					)
				) 
			};

			// Check if the facing/movement angle target has changed
			// from the LS angle to the angle-to-target or vice versa.
			movementYawTargetChanged =
			(
				(
					(turnToTarget || faceTarget) && 
					(!oldTurnToTarget && !oldFaceTarget)
				) ||
				(
					(!turnToTarget && !faceTarget) && 
					(oldTurnToTarget || oldFaceTarget)
				)
			);

			if (turnToTarget || faceTarget) 
			{
				// Slow down rotation quickly if too close to the target
				// since the angle to the target changes too rapidly 
				// and causes jittering when rotating to directly face the target.
				float xyDistToTarget = Util::GetXYDistance
				(
					movementActor->data.location, targetLocation
				);
				float minDistToSlowRotation = Util::GetXYDistance
				(
					coopActor->data.location, movementActor->data.location
				) * 0.1f;
				// Default radius at which to start slowing rotation.
				float radius = Settings::fTargetAttackSourceDistToSlowRotation;
				// Slow down when within the player actor's bounds.
				auto player3DPtr = Util::GetRefr3D(coopActor.get()); 
				if (player3DPtr && player3DPtr.get()) 
				{
					radius = player3DPtr->worldBound.radius;
				}
				rotMult *= Util::InterpolateEaseInEaseOut
				(
					0.0f, 
					1.0f, 
					(
						min
						(
							xyDistToTarget - minDistToSlowRotation, radius
						) / 
						max
						(
							0.01f, radius
						)
					), 
					3.0f
				);
				
				bool isAimingWithRangedWeapon = 
				{
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
				};
				// Turn so that weapon (notched arrow/loaded bolt) node's direction 
				// is pointed at the target.
				if (isAimingWithRangedWeapon && p->em->HasRangedWeapEquipped())
				{
					// Directly face the target position by tacking on the aim heading offset.
					yawToTarget = Util::DirectionToGameAngYaw
					(
						targetLocation - movementActor->data.location
					);
					playerTargetYaw = 
					(
						movementActor->data.angle.z + 
						(
							Util::NormalizeAngToPi
							(
								yawToTarget - 
								coopActor->GetAimHeading()
							)
						)
					);
					
					// NOTE: 
					// Interferes with the torso node rotation code,
					// leading to spinal curvature and an odd aiming orientation 
					// when beyond an aim pitch of +- 90 degrees,
					// since the attack source direction points behind the player.
					// Commented out for now until a proper solution is found.
					/*playerTargetYaw = 
					(
						movementActor->data.angle.z + 
						(
							Util::NormalizeAngToPi
							(
								yawToTarget - 
								Util::DirectionToGameAngYaw(playerAttackSourceDir)
							)
						) * rotMult
					);*/
				}
				else
				{
					// Face the target directly.
					playerTargetYaw = yawToTarget;
				}
			}
			else if (lsMoved)
			{
				// Turn to face the player movement direction.
				const auto& moveZAngle = movementOffsetParams[!MoveParams::kLSGameAng];
				playerTargetYaw = moveZAngle;
			}

			// Normalize.
			playerTargetYaw = Util::NormalizeAng0To2Pi(playerTargetYaw);
		}

		//==================
		// [Movement Offset]
		//==================

		// Need to have an active middle high process.
		auto midHighProc = movementActor->GetMiddleHighProcess(); 
		if (!midHighProc)
		{
			return;
		}

		const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
		const auto& lsMag = lsData.normMag;
		// X axis is to the right of the player's facing direction.
		// Y axis is in the player's facing direction.
		float xPosOffset = 0.0f;
		float yPosOffset = 0.0f;

		// NOTE:
		// The player does not directly follow their movement offset right away,
		// so when changing the player's facing direction rapidly
		// while a movement type change also occurs, ex. when starting to sprint away
		// after facing a target, or when starting to attack a target when not facing them,
		// the player will sluggishly continue moving in their facing direction,
		// which is directly tied to their rotation rate. 
		// The player's movement path curves until their facing
		// and movement directions equalize. Since I can't fully decouple
		// movement direction and rotation when using a movement offset,
		// even while setting a rotation offset of 0 and directly setting their facing angle, 
		// I have to speed up rotation when the player is changing their facing direction rapidly.
		float yawDiff = Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z);
		if (movementYawTargetChanged)
		{
			speedUpRotOnYawTargetChange = true;
		}
		else if (fabsf(yawDiff) <= PI / 180.0f)
		{
			// Stop speedingn up rotation when the difference between facing and moving
			// yaw is near zero.
			speedUpRotOnYawTargetChange = false;
		}

		float interpFactorScalar = 1.0f;
		if (speedUpRotOnYawTargetChange)
		{
			interpFactorScalar =
			(
				1.0f + 3.0f * (1.0f / Settings::fBaseRotationMult) * fabsf(yawDiff / PI)
			);
		}
		
		// Interpolated yaw offset from the player's current yaw to the target yaw.
		// NOTE: 
		// Keeping a rotational offset with KeepOffsetFromActor() leads to jittering
		// when moving the character, so we just interp the player's data angle instead
		// to set the target rotation, and only use the movement offset for translational motion.
		// NOTE 2: 
		// Since the player will rotate through the same yaw offset
		// less frequently at lower framerates,
		// we want to scale up the offset higher at lower framerates to compensate.
		// Yaw angle offset to add on top of the player's yaw this frame.
		float rawYawOffset = std::lerp
		(
			0.0f,
			rotMult * yawDiff,
			min(playerRotInterpFactor * interpFactorScalar, 1.0f) * 
			max(1.0f, 60.0f * *g_deltaTimeRealTime)
		);
		
		// P1 is not AI driven if attempting discovery or motion driven flag is set.
		bool p1MotionDriven = 
		{
			(p->isPlayer1) && 
			(
				(attemptDiscovery) || 
				(
					glob.player1Actor->movementController && 
					glob.player1Actor->movementController->controlsDriven
				)
			)
		};
		
		if (p1MotionDriven) 
		{
			if (attemptDiscovery)
			{
				// Don't move while attempting discovery.
				ClearKeepOffsetFromActor();
				SetDontMove(true);
			}
			else
			{
				// Clear the movement offset to allow the game 
				// to handle P1's movement while not AI driven.
				SetDontMove(false);
				ClearKeepOffsetFromActor();
			}

			
			bool isEquipping = false;
			bool isUnequipping = false;
			coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
			coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);
			// Must rotate to face the player's movement direction
			// when equipping or unequipping, 
			// since P1 is motion driven and may not 
			// have 360 degree movement when not AI driven.
			// Obviously, with True Directional Movement installed, 
			// this is redundant and unnecessary.
			bool shouldRotateWhileMotionDriven = 
			{
				(isParagliding) || 
				(
					(!ALYSLC::TrueDirectionalMovementCompat::g_trueDirectionalMovementInstalled) && 
					(isEquipping || isUnequipping)
				)
			};
			if (shouldRotateWhileMotionDriven) 
			{
				float newYaw = Util::NormalizeAng0To2Pi
				(
					movementActor->data.angle.z + rawYawOffset
				);
				movementActor->SetHeading(newYaw);
				midHighProc->rotationSpeed.z = 0.0f;
			}
		}
		else if (interactionPackageRunning)
		{
			// Ensure the player can move first.
			SetDontMove(false);
			float xyDistToInteractionPos = Util::GetXYDistance
			(
				coopActor->data.location, interactionPackageEntryPos
			);
			// Stop when close enough for the package 
			// to kick in and start the activation animation.
			if (xyDistToInteractionPos < 0.5f * p->tm->GetMaxActivationDist())
			{
				ClearKeepOffsetFromActor();

				// Execute interaction package again 
				// if the player does not have any occupied furniture 
				// after setting the interaction package target and position earlier.
				auto interactionFurniture = coopActor->GetOccupiedFurniture(); 
				if (!Util::HandleIsValid(interactionFurniture)) 
				{
					auto interactionPackage = 
					(
						glob.coopPackages
						[!PackageIndex::kTotal * controllerID + !PackageIndex::kSpecialInteraction]
					);
					p->pam->SetCurrentPackage(interactionPackage);
					p->pam->EvaluatePackage();
				}
			}
			else
			{
				// Turn to face the interaction entry position.
				rawYawOffset = std::lerp
				(
					0.0f,
					Util::NormalizeAngToPi
					(
						Util::GetYawBetweenPositions
						(
							movementActor->data.location, interactionPackageEntryPos
						) - 
						movementActor->data.angle.z
					),
					playerRotInterpFactor * max(1.0f, 60.0f * *g_deltaTimeRealTime)
				);
				float newYaw = Util::NormalizeAng0To2Pi
				(
					movementActor->data.angle.z + rawYawOffset
				);
				movementActor->SetHeading(newYaw);
				midHighProc->rotationSpeed.z = 0.0f;
				// Move to interaction package entry position which was set during activation.
				// Slow down when nearing the interaction position.
				KeepOffsetFromActor
				(
					coopActor->GetHandle(), 
					RE::NiPoint3(0.0f, 10.0f, 0.0f), 
					RE::NiPoint3(),
					0.0f, 
					0.0f
				);
			}
		}
		else if (isSynced || isMounting || coopActor->IsInKillMove())
		{
			// Make sure the player can move and is not directed by this manager.
			SetDontMove(false);
			ClearKeepOffsetFromActor();
		}
		else if (isAnimDriven && 
				 coopActor->currentProcess &&
				 coopActor->currentProcess->middleHigh && 
				 coopActor->currentProcess->middleHigh->furnitureIdle)
		{
			SPDLOG_DEBUG
			(
				"[MM] SetPlayerOrientation: {}: "
				"is anim driven: {}, current idle: {} (0x{:X}), last idle: {} (0x{:X}), "
				"furniture idle: {} (0x{:X}).",
				coopActor->GetName(),
				isAnimDriven,
				coopActor->currentProcess && 
				coopActor->currentProcess->high && 
				coopActor->currentProcess->high->currentProcessIdle ?
				coopActor->currentProcess->high->currentProcessIdle->GetName() :
				"NONE",
				coopActor->currentProcess && 
				coopActor->currentProcess->high && 
				coopActor->currentProcess->high->currentProcessIdle ?
				coopActor->currentProcess->high->currentProcessIdle->formID : 
				0xDEAD,
				coopActor->currentProcess && 
				coopActor->currentProcess->middleHigh && 
				coopActor->currentProcess->middleHigh->lastIdlePlayed ? 
				coopActor->currentProcess->middleHigh->lastIdlePlayed->GetName() : 
				"NONE",
				coopActor->currentProcess && 
				coopActor->currentProcess->middleHigh && 
				coopActor->currentProcess->middleHigh->lastIdlePlayed ? 
				coopActor->currentProcess->middleHigh->lastIdlePlayed->formID : 
				0xDEAD,
				coopActor->currentProcess && 
				coopActor->currentProcess->middleHigh &&
				coopActor->currentProcess->middleHigh->furnitureIdle ? 
				coopActor->currentProcess->middleHigh->furnitureIdle->GetName() : 
				"NONE",
				coopActor->currentProcess && 
				coopActor->currentProcess->middleHigh && 
				coopActor->currentProcess->middleHigh->furnitureIdle ? 
				coopActor->currentProcess->middleHigh->furnitureIdle->formID : 
				0xDEAD
			);

			// Make sure the player can move to interact with furniture
			// and is not directed by this manager.
			SetDontMove(false);
			ClearKeepOffsetFromActor();
		}
		else if (menuStopsMovement || 
				 coopActor->IsInRagdollState() ||
				 coopActor->actorState1.knockState != RE::KNOCK_STATE_ENUM::kNormal)
		{
			// Don't move when a menu is opened that stops movement or when ragdolled.
			SetDontMove(true);
			ClearKeepOffsetFromActor();
		}
		else if (isDashDodging)
		{
			// Ensure the player can move first.
			SetDontMove(false);
			if (isBackStepDodge)
			{
				// Move backward throughout.
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(0.0f, -10.0f, 0.0f), 
					RE::NiPoint3(), 0.0f, 0.0f
				);
			}
			else if (framesSinceRequestingDashDodge <= 1)
			{
				// Set offset to keep throughout the dodge during the first frame.
				float facingToHeadingAngDiff = 
				(
					Util::NormalizeAngToPi
					(
						movementOffsetParams[!MoveParams::kLSGameAng] -
						movementActor->data.angle.z
					)
				);
				xPosOffset = 100.0f * sinf(facingToHeadingAngDiff);
				yPosOffset = 100.0f * cosf(facingToHeadingAngDiff);
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(xPosOffset, yPosOffset, 0.0f), 
					RE::NiPoint3(), 0.0f, 0.0f
				);
				midHighProc->rotationSpeed.z = 0.0f;
			}
		}
		else if (shouldCurtailMomentum)
		{
			// Stop the player from moving until their reported movement speed reaches 0.
			// Do not want to allow movement while curtailing momentum
			// because the player's movement type speeds are set to a very large number
			// to expedite deceleration.
			ClearKeepOffsetFromActor();
			SetDontMove(true);
			// Can still rotate.
			float newYaw = Util::NormalizeAng0To2Pi
			(
				movementActor->data.angle.z + rawYawOffset
			);
			movementActor->SetHeading(newYaw);
			midHighProc->rotationSpeed.z = 0.0f;
		}
		else if (shouldStopMoving || lsMag == 0.0f)
		{
			// SetDontMove() freezes actors in midair, 
			// so only set the don't move flag when not paragliding, on the ground, 
			// and not trying to jump.
			RE::bhkCharacterController* charController
			{
				movementActor && movementActor.get() ? 
				movementActor->GetCharController() :
				nullptr 
			};
			bool canFreeze = 
			(
				shouldStopMoving && 
				!p->pam->isAttacking && 
				!isAirborneWhileJumping && 
				!reqStartJump
			);
			if (canFreeze && 
				charController && 
				charController->context.currentState == RE::hkpCharacterStateType::kInAir) 
			{
				canFreeze = false;
			}
			
			ClearKeepOffsetFromActor();
			if (canFreeze)
			{
				SetDontMove(true);
			}
			else if (faceTarget || turnToTarget) 
			{
				// Only Z rotation needed if stopped and turning to or facing a target.
				// No need to keep an offset when the player is already facing the target.
				// Player will walk in place very slowly if the offset isn't cleared.
				// Looks weird. Do not like.
				SetDontMove(false);
			}
			else
			{
				// Already stopped, but do not prevent rotation.
				SetDontMove(false);
			}

			// Manually rotate to avoid slow motion shifting when the Z rotation offset is small.
			movementActor->SetHeading
			(
				Util::NormalizeAng0To2Pi
				(
					movementActor->data.angle.z + rawYawOffset
				)
			);
			midHighProc->rotationSpeed.z = 0.0f;
		}
		else
		{
			// Ensure the player/mount can move first.
			SetDontMove(false);

			// NOTE:
			// KeepOffsetFromActor() called on P1's mount does not work, 
			// so while P1 is mounted, they must not be AI driven.
			if (movementActor->IsAMount())
			{
				// Mounts cannot move sideways, so move forward and rotate.
				float newYaw = Util::NormalizeAng0To2Pi
				(
					movementActor->data.angle.z + rawYawOffset
				);
				movementActor->SetHeading(newYaw);
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(0.0f, 1.0f, 0.0f), 
					RE::NiPoint3(), 
					0.0f, 
					0.0f
				);
			}
			else
			{
				// NOTE: 
				// Keeping a rotational offset with KeepOffsetFromActor() leads to jittering
				// when moving the character, so we just interp the player's data angle instead
				// to set the target rotation.
				//if (p->isPlayer1) 
				{
					Util::NativeFunctions::ClearKeepOffsetFromActor(movementActor.get());
				}

				const float baseMTMult = 
				(
					Settings::fBaseMTRotationMult * Settings::fBaseRotationMult * PI
				);
				const float maxAngDiffPerFrame = baseMTMult * *g_deltaTimeRealTime;
				// Clamp to max per-frame rotation angle.
				rawYawOffset = std::clamp(rawYawOffset, -maxAngDiffPerFrame, maxAngDiffPerFrame);
				
				// Movement offset doesn't rotate the player when bAllowRotation is set to true,
				// so modify the middle high process rotation speed directly instead.
				// Do this when attacking, paragliding, or shield charging.
				bool allowRotation = false;
				coopActor->GetGraphVariableBool("bAllowRotation", allowRotation);
				bool useMidHighProcRot =
				(
					(midHighProc) && 
					(
						(allowRotation && p->pam->isAttacking) ||
						(isParagliding) ||
						(p->pam->IsPerformingAllOf(InputAction::kSprint, InputAction::kBlock))
					)
				);
				if (useMidHighProcRot)
				{
					midHighProc->rotationSpeed.z = rawYawOffset * (1.0f / *g_deltaTimeRealTime);
				}
				else
				{
					// Tack on offset.
					movementActor->SetHeading
					(
						Util::NormalizeAng0To2Pi
						(
							movementActor->data.angle.z + rawYawOffset
						)
					);
					midHighProc->rotationSpeed.z = 0.0f;
				}

				// Angle diff between facing and desired movement directions.
				float facingToHeadingAngDiff = 
				(
					Util::NormalizeAngToPi
					(
						movementOffsetParams[!MoveParams::kLSGameAng] - 
						movementActor->data.angle.z
					)
				);

				xPosOffset = sinf(facingToHeadingAngDiff) * 100.0f;
				yPosOffset = cosf(facingToHeadingAngDiff) * 100.0f;
				// No slowdown or catch up radius.
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(xPosOffset, yPosOffset, 0.0f), 
					RE::NiPoint3(), 
					-FLT_MAX, 
					-FLT_MAX
				);
			}
		}

		// Store the set pitch and yaw.
		playerPitch = movementActor->data.angle.x;
		playerYaw = movementActor->data.angle.z;
	}

	void MovementManager::SetShouldPerformLocationDiscovery()
	{
		// Temporary hacky workaround to allow for "automatic" location discovery,
		// since P1 is prevented from discovering locations while AI driven.
		// NOTE: 
		// Still does not work at times when the map marker's radius is 0.
		// Toggle off the co-op camera briefly to discover the location if
		// moving and stopping P1 does not trigger the discovery event.
		// Can also flop to discover locations. Yep.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Only run with P1, if valid and parent cell is available,
		// and if the co-op camera is running.
		if (!p->isPlayer1 || !p1 || !glob.cam->IsRunning())
		{
			return;
		}

		attemptDiscovery = false;  
		auto cell = p1->parentCell;
		if (!cell)
		{
			return;
		}

		// Check for discovery to remove local map fog of war when P1 is inactive 
		// (not moving either analog stick and not attacking/dodging).
		float movementSpeed = coopActor->DoGetMovementSpeed();
		bool isTKDodging = false;
		bool isTDMDodging = false;
		coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
		coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
		attemptDiscovery = 
		{ 
			!p->pam->isAttacking &&
			!p->pam->isBashing &&
			!p->pam->isBlocking &&
			!p->pam->isInCastingAnim && 
			!reqFaceTarget && 
			!isDashDodging && 
			!isTKDodging && 
			!isTDMDodging &&
			!rsMoved &&
			!lsMoved && 
			movementSpeed == 0.0f
		};

		if (cell->IsExteriorCell() && !attemptDiscovery)
		{
			// Check for map marker discovery in exterior cells 
			// when the camera origin point draws near an undiscovered map marker.
			// All map markers in the current cell are cached in the camera manager.

			// Check if the player has just moved into range of a marker.
			bool prevInRange = inRangeOfUndiscoveredMarker;
			inRangeOfUndiscoveredMarker = false;

			const auto& origin = glob.cam->camOriginPoint;
			for (const auto& markerRefr : glob.cam->cellMapMarkers)
			{
				if (!markerRefr || !markerRefr.get())
				{
					continue;
				}

				// Check if this marker is already visible on the map or not.
				auto extraMarker = markerRefr->extraList.GetByType<RE::ExtraMapMarker>(); 
				if (!extraMarker || !extraMarker->mapData)
				{
					continue;
				}
				
				bool visible = extraMarker->mapData->flags.all
				(
					RE::MapMarkerData::Flag::kVisible
				);

				// If one is available, get the marker discovery radius.
				// Default radius is 1000 in the CK.
				float thisRadius = 1000.0f;
				auto extraRadius = markerRefr->extraList.GetByType<RE::ExtraRadius>(); 
				if (extraRadius)
				{
					thisRadius = extraRadius->radius;
				}
				
				// If not visible (discovered) and this map marker is within range,
				// we can attempt discovery.
				inRangeOfUndiscoveredMarker = 
				(
					(!visible) && 
					(markerRefr->data.location.GetDistance(origin) < thisRadius)
				);

				if (inRangeOfUndiscoveredMarker)
				{
					// No need to continue searching if in range of one undiscovered marker.
					break;
				}
			}

			// Check if undiscovered map marker is discoverable.
			if (inRangeOfUndiscoveredMarker)
			{
				// Just moved into range.
				if (!prevInRange)
				{
					// Do not attempt discovery yet.
					// We'll wait 10 frames, since the player may move
					// in and out of range quickly and we do not want to 
					// perform this check too frequently.
					attemptDiscovery = false;
					framesSinceAttemptingDiscovery = 0;
				}
				else if (framesSinceAttemptingDiscovery < 10)
				{
					// Wait 10 frames.
					framesSinceAttemptingDiscovery++;
				}
				else
				{
					// 10 frames have passed, so signal to attempt discovery
					// and reset the discovery attempt frame count.
					attemptDiscovery = true;
					framesSinceAttemptingDiscovery = 0;
				}
			}
			else
			{
				// Not in range, so set both to false.
				attemptDiscovery = inRangeOfUndiscoveredMarker = false;
			}
		}
	}

	void MovementManager::UpdateAimPitch()
	{
		// Adjust aim pitch (look at) position, which influences player spinal pitch.

		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
		bool crosshairRefrValidity = 
		(
			crosshairRefrPtr && 
			crosshairRefrPtr.get() &&
			Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
		);
		auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle
		(
			p->tm->aimCorrectionTargetHandle
		);
		bool aimCorrectionTargetValidity = 
		(
			aimCorrectionTargetPtr && 
			aimCorrectionTargetPtr.get() &&
			Util::IsValidRefrForTargeting(aimCorrectionTargetPtr.get())
		);
		bool isUsingWeapMag = 
		(
			p->pam->isAttacking ||
			p->pam->isBlocking ||
			p->pam->isBashing ||
			p->pam->isInCastingAnim
		);
		// Can adjust pitch to face either a crosshair-targeted refr or an aim correction target.
		bool turningToCrosshairTarget = isUsingWeapMag && crosshairRefrValidity;
		bool usingAimCorrectionTarget = 
		(
			aimCorrectionTargetValidity && 
			!turningToCrosshairTarget && 
			Settings::vbUseAimCorrection[playerID] && 
			!reqFaceTarget && 
			isUsingWeapMag
		);
		
		// Also haven't figured out how to properly account for 
		// different default spinal rotations when transformed,
		// so don't adjust aim pitch to face the target while transformed for now.
		// Can still manually adjust the transformed player's spinal rotation though.
		adjustAimPitchToFaceTarget = 
		{ 
			(!p->isTransformed) && 
			(!p->tm->rmm->isGrabbing) && 
			(reqFaceTarget || turningToCrosshairTarget || usingAimCorrectionTarget) 
		};

		// Default to pitching towards the current crosshair position,
		// but if an aim correction target is selected,
		// or if not facing the crosshair position and an actor is selected with the crosshair,
		// aim at the targeted actor's torso.
		auto targetPos = p->tm->crosshairWorldPos;
		if (usingAimCorrectionTarget)
		{
			targetPos = Util::GetTorsoPosition(aimCorrectionTargetPtr.get());
		}
		else if (!reqFaceTarget && crosshairRefrValidity && crosshairRefrPtr->As<RE::Actor>())
		{
			targetPos = Util::GetTorsoPosition(crosshairRefrPtr->As<RE::Actor>());
		}

		float defaultPitchToTarget = Util::GetPitchBetweenPositions
		(
			playerDefaultAttackSourcePos, targetPos
		);
		// Slow down spinal rotation when close to the target to minimize jitter.
		float distToTarget = playerAttackSourcePos.GetDistance(targetPos);
		auto rotMult = 1.0f;
		if (distToTarget > p->tm->GetMaxActivationDist() / 10.0f)
		{
			rotMult *= Util::InterpolateEaseInEaseOut
			(
				0.0f, 
				1.0f, 
				(
					min
					(
						distToTarget, 
						Settings::fTargetAttackSourceDistToSlowRotation
					) / 
					max
					(
						0.01f, 
						Settings::fTargetAttackSourceDistToSlowRotation
					)
				), 
				2.0f
			);
		}
		else
		{
			rotMult = 0.0f;
		}

		// Controller data.
		const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
		const auto& rsX = rsData.xComp;
		const auto& rsY = rsData.yComp;
		const auto& rsMag = rsData.normMag;
		
		// Reset to default pitch when not targeting anything 
		// and aim pitch was not manually adjusted.
		bool resetAimPitchIfNotAdjusted = false;
		// Manually adjusting aim pitch with the RS.
		bool shouldAdjustAimPitch = 
		(
			p->pam->IsPerforming(InputAction::kAdjustAimPitch) &&
			fabsf(rsY) > fabsf(rsX)
		);
		if (shouldAdjustAimPitch) 
		{
			if (coopActor->IsSwimming()) 
			{
				// NOTE: 
				// Player cannot swim laterally if their spine is pitched too far upward, 
				// so clamp to a smaller angle (30 degrees instead of 90).
				aimPitch = std::clamp
				(
					aimPitch - 
					(
						rsData.normMag * 
						rsY * 
						Settings::vfMaxAimPitchAdjustmentRate[playerID] * 
						*g_deltaTimeRealTime
					), 
					-PI / 6.0f, 
					PI / 2.0f
				);
			}
			else
			{
				aimPitch = std::clamp
				(
					aimPitch - 
					(
						rsData.normMag * 
						rsY * 
						Settings::vfMaxAimPitchAdjustmentRate[playerID] * 
						*g_deltaTimeRealTime
					), 
					-PI / 2.0f, 
					PI / 2.0f
				);
			}

			aimPitchManuallyAdjusted = aimPitchAdjusted = true;
		}
		else if (adjustAimPitchToFaceTarget)
		{
			// Pitch directly at target if aim pitch was not manually modified.
			if (!aimPitchManuallyAdjusted) 
			{
				auto pitchDiff = Util::NormalizeAngToPi(defaultPitchToTarget - aimPitch);
				if (coopActor->IsSwimming())
				{
					aimPitch = std::clamp(aimPitch + pitchDiff * rotMult, -PI / 6.0f, PI / 2.0f);
				}
				else
				{
					aimPitch = std::clamp(aimPitch + pitchDiff * rotMult, -PI / 2.0f, PI / 2.0f);
				}
			}

			aimPitchAdjusted = true;
		}
		else
		{
			// Reset to default if not manually adjusted.
			resetAimPitchIfNotAdjusted = !aimPitchManuallyAdjusted;
		}

		bool shouldResetAimPitch = reqResetAimAndBody || resetAimPitchIfNotAdjusted;
		if (shouldResetAimPitch)
		{
			// Clear out all previously set node target rotations.
			// Instantly resets all nodes' rotations to their defaults, without blending out.
			if (reqResetAimAndBody) 
			{
				std::unique_lock<std::mutex> lock(p->mm->nom->rotationDataMutex);
				nom->ClearCustomRotations();
			}

			// If there is a valid target, adjust the aim pitch to face them on reset.
			if (adjustAimPitchToFaceTarget)
			{
				auto pitchDiff = Util::NormalizeAngToPi(defaultPitchToTarget - aimPitch);
				if (coopActor->IsSwimming())
				{
					aimPitch = std::clamp(aimPitch + pitchDiff * rotMult, -PI / 6.0f, PI / 2.0f);
				}
				else
				{
					aimPitch = std::clamp(aimPitch + pitchDiff * rotMult, -PI / 2.0f, PI / 2.0f);
				}
			}
			else
			{
				// Tip upward slightly when swimming to prevent players 
				// from sinking while swimming with the reset aim pitch.
				aimPitch = coopActor->IsSwimming() ? -PI / 24.0f : 0.0f;
			}

			// Indicate that aim pitch was reset and is no longer adjusted.
			aimPitchManuallyAdjusted = aimPitchAdjusted = false;
			reqResetAimAndBody = false;
		}

		// Set the aim pitch position after modifying the aim pitch 
		// if the player is not transformed.
		if (!p->isTransforming) 
		{
			float radialDist = playerScaledHeight / 2.0f;
			auto eyePos = Util::GetEyePosition(coopActor.get());
			const float headingAng = Util::ConvertAngle(coopActor->GetHeading(false));
			aimPitchPos = RE::NiPoint3
			(
				(
					eyePos.x + 
					radialDist * 
					cosf(headingAng) *
					cosf(aimPitch)
				),
				(
					eyePos.y + 
					radialDist * 
					sinf(headingAng) * 
					cosf(aimPitch)
				),
				eyePos.z - radialDist * sinf(aimPitch)
			);
		}

		// Tilt player up/down when swimming, based on their aim pitch.
		auto charController = coopActor->GetCharController(); 
		if (charController && coopActor->IsSwimming())
		{
			if (isDashDodging)
			{
				coopActor->data.angle.x = charController->pitchAngle;
			}
			else
			{
				charController->pitchAngle = aimPitch;
				coopActor->data.angle.x = aimPitch;
			}
		}
		else
		{
			// Modifications to P1's X angle here while aiming with a bow/crossbow
			// double up the effects of our custom torso rotation, so set to zero here.
			coopActor->data.angle.x = 0.0f;

			if (p->isPlayer1 && p->pam->isInCastingAnim)
			{
				auto lhCaster = coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kLeftHand
				);
				auto rhCaster = coopActor->GetMagicCaster
				(
					RE::MagicSystem::CastingSource::kRightHand
				);
				auto lhSpell = lhCaster ? lhCaster->currentSpell : nullptr;
				auto rhSpell = rhCaster ? rhCaster->currentSpell : nullptr;
				bool lhTargetLocationCast = 
				(
					(p->pam->isCastingLH || p->pam->isInCastingAnimLH) && 
					lhSpell &&
					lhSpell->GetDelivery() == RE::MagicSystem::Delivery::kTargetLocation
				);
				bool rhTargetLocationCast = 
				(
					(p->pam->isCastingRH || p->pam->isInCastingAnimRH) && 
					rhSpell &&
					rhSpell->GetDelivery() == RE::MagicSystem::Delivery::kTargetLocation
				);
				bool handCastingTargetLocationSpell = lhTargetLocationCast || rhTargetLocationCast;
				if (handCastingTargetLocationSpell)
				{
					auto lhMagNode = lhCaster ? lhCaster->GetMagicNode() : nullptr;
					auto rhMagNode = rhCaster ? rhCaster->GetMagicNode() : nullptr;
					if (reqFaceTarget)
					{
						// Direct at crosshair position.
						if (lhTargetLocationCast)
						{
							coopActor->data.angle.x = Util::GetPitchBetweenPositions
							(
								lhMagNode ? 
								lhMagNode->world.translate :
								coopActor->GetLookingAtLocation(),
								p->tm->crosshairWorldPos
							);
						}
						else
						{
							coopActor->data.angle.x = Util::GetPitchBetweenPositions
							(
								rhMagNode ? 
								rhMagNode->world.translate :
								coopActor->GetLookingAtLocation(),
								p->tm->crosshairWorldPos
							);
						}
					}
					else
					{
						// Look down and slightly forward to place near P1's feet.
						coopActor->data.angle.x = PI / 3.0f - aimPitch;
					}
				}
			}
		}
	}

	void MovementManager::UpdateAttackSourceOrientationData(bool&& a_setDefaultDirAndPos)
	{
		// Get node from which an attack would originate,
		// based on the player's equipped gear,
		// and save its position and direction.
		// Can also set the default attack source position and direction
		// using our cached default node world rotation data.

		// Need valid 3D and fixed strings.
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings || !coopActor->loadedData || !coopActor->loadedData->data3D)
		{
			return;
		}

		const auto& data3D = coopActor->loadedData->data3D;
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		// Attack source position and direction to set via refr.
		// Either the default position/direction or the post-torso rotation modification one.
		auto& attackSourcePos = 
		(
			a_setDefaultDirAndPos ? playerDefaultAttackSourcePos : playerAttackSourcePos
		);
		auto& attackSourceDir = 
		(
			a_setDefaultDirAndPos ? playerDefaultAttackSourceDir : playerAttackSourceDir
		);

		// Default attack source position is at eye level 
		// and in the facing direction of the player.
		attackSourcePos = coopActor->GetLookingAtLocation();
		attackSourceDir = Util::RotationToDirectionVect
		(
			-coopActor->data.angle.x, 
			Util::ConvertAngle(coopActor->data.angle.z)
		);

		// Adjust attack source position and rotation 
		// based on what attack action the player is performing.
		// Casting source position and direction.
		if (p->pam->isCastingLH && p->pam->isCastingRH)
		{
			// Best approximation to be made here is to rotate 
			// about the point between the two casting nodes when casting with two hands,
			// as there is no single source point for the released projectiles.
			// Also approximate attack direction as the averaged direction of the two caster nodes.
			// Note that the projectiles fired will still be directed at the target regardless
			// of the magic node's orientation.
			auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>
			(
				data3D->GetObjectByName(strings->npcLMagicNode)
			);
			auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>
			(
				data3D->GetObjectByName(strings->npcRMagicNode)
			);
			if (a_setDefaultDirAndPos)
			{
				if (lMagNodePtr && 
					lMagNodePtr.get() &&
					rMagNodePtr && 
					rMagNodePtr.get() &&
					nom->defaultNodeWorldTransformsMap.contains(lMagNodePtr) && 
					nom->defaultNodeWorldTransformsMap.contains(rMagNodePtr))
				{
					const auto& lhPos = 
					(
						nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).translate
					);
					const auto& rhPos = 
					(
						nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).translate
					);
					attackSourcePos = (lhPos + rhPos) / 2.0f;

					const auto& lhRot = nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).rotate;
					const auto& rhRot = nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).rotate;
					attackSourceDir = (lhRot * forward + rhRot * forward) / 2.0f;
				}
			}
			else
			{
				if (lMagNodePtr && lMagNodePtr.get() && rMagNodePtr && rMagNodePtr.get()) 
				{
					attackSourcePos = 
					(
						(lMagNodePtr->world.translate + rMagNodePtr->world.translate) / 2.0f
					);
					attackSourceDir = 
					(
						(
							lMagNodePtr->world.rotate * 
							forward + 
							rMagNodePtr->world.rotate * 
							forward
						) / 2.0f
					);
				}
			}
		}
		else if (p->pam->isCastingLH)
		{
			// Get position of the LH magic node and its direction.
			auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>
			(
				data3D->GetObjectByName(strings->npcLMagicNode)
			);
			if (a_setDefaultDirAndPos)
			{
				if (lMagNodePtr && 
					lMagNodePtr.get() &&
					nom->defaultNodeWorldTransformsMap.contains(lMagNodePtr))
				{
					attackSourcePos = nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).translate;
					attackSourceDir = 
					(
						nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).rotate * forward
					);
				}
			}
			else
			{
				if (lMagNodePtr && lMagNodePtr.get())
				{
					attackSourcePos = lMagNodePtr->world.translate;
					attackSourceDir = lMagNodePtr->world.rotate * forward;
				}
			}
		}
		else if (p->pam->isCastingRH)
		{
			// Get position of the RH magic node and its direction.
			auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>
			(
				data3D->GetObjectByName(strings->npcRMagicNode)
			);
			if (a_setDefaultDirAndPos)
			{
				if (rMagNodePtr && 
					rMagNodePtr.get() && 
					nom->defaultNodeWorldTransformsMap.contains(rMagNodePtr))
				{
					attackSourcePos = nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).translate;
					attackSourceDir = 
					(
						nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).rotate * forward
					);
				}
			}
			else
			{
				if (rMagNodePtr && rMagNodePtr.get())
				{
					attackSourcePos = rMagNodePtr->world.translate;
					attackSourceDir = rMagNodePtr->world.rotate * forward;
				}
			}
		}
		else
		{
			// Handle ranged attack source position and rotation.
			auto weaponNodePtr = 
			(
				RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->weapon))
			); 
			if (weaponNodePtr && weaponNodePtr.get()) 
			{
				bool ammoDrawnOrLater = 
				{
					coopActor->actorState1.meleeAttackState == 
					RE::ATTACK_STATE_ENUM::kBowAttached ||
					coopActor->actorState1.meleeAttackState ==
					RE::ATTACK_STATE_ENUM::kBowDrawn ||
					coopActor->actorState1.meleeAttackState == 
					RE::ATTACK_STATE_ENUM::kBowReleasing
				};
				bool nockingAmmo = 
				{
					coopActor->actorState1.meleeAttackState == 
					RE::ATTACK_STATE_ENUM::kBowAttached ||
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw
				};
				if (nockingAmmo && p->em->HasBowEquipped())
				{
					// Since the arrow rotates away from the player
					// while it is being removed from the quiver and nocked,
					// using the direction from the draw hand to the bow hand is more stable
					// and less jittery than using the arrow/weapon node's rotation.
					// The arrow node rotation will not be stable until it is fully drawn.
					auto lhNodePtr = RE::NiPointer<RE::NiAVObject>
					(
						data3D->GetObjectByName("NPC L Hand [LHnd]"sv)
					);
					auto rhNodePtr = RE::NiPointer<RE::NiAVObject>
					(
						data3D->GetObjectByName("NPC R Hand [RHnd]"sv)
					);
					if (a_setDefaultDirAndPos)
					{
						if (weaponNodePtr && 
							weaponNodePtr.get() && 
							nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourcePos = 
							(
								nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).translate
							);
						}

						if (lhNodePtr && lhNodePtr.get() &&
							rhNodePtr && rhNodePtr.get() &&
							nom->defaultNodeWorldTransformsMap.contains(lhNodePtr) &&
							nom->defaultNodeWorldTransformsMap.contains(rhNodePtr) && 
							nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							const auto& lhPos = 
							(
								nom->defaultNodeWorldTransformsMap.at(lhNodePtr).translate
							);
							const auto& rhPos = 
							(
								nom->defaultNodeWorldTransformsMap.at(rhNodePtr).translate
							);
							const auto& weapRot = 
							(
								nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).rotate
							);
							attackSourceDir = Util::RotationToDirectionVect
							(
								-Util::DirectionToGameAngPitch(weapRot * forward),
								Util::ConvertAngle(Util::DirectionToGameAngYaw(lhPos - rhPos))
							);
						}
					}
					else
					{
						if (lhNodePtr && lhNodePtr.get() && rhNodePtr && rhNodePtr.get())
						{
							attackSourceDir = Util::RotationToDirectionVect
							(
								-Util::DirectionToGameAngPitch
								(
									weaponNodePtr->world.rotate * forward
								),
								Util::ConvertAngle
								(
									Util::DirectionToGameAngYaw
									(
										lhNodePtr->world.translate - rhNodePtr->world.translate
									)
								)
							);
						}

						attackSourcePos = weaponNodePtr->world.translate;
					}
				}
				else if (nockingAmmo || ammoDrawnOrLater)
				{
					// Set direction and position directly from the weapon node's orientation data
					// if nocking a crossbow or if the bow/crossbow is already fully drawn.
					if (a_setDefaultDirAndPos)
					{
						if (nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourcePos = 
							(
								nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).translate
							);
						}

						if (nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourceDir = 
							(
								nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).rotate * 
								forward
							);
						}
					}
					else
					{
						attackSourcePos = weaponNodePtr->world.translate;
						attackSourceDir = weaponNodePtr->world.rotate * forward;
					}
				}
			}
		}

		// Ensure direction is normalized.
		attackSourceDir.Unitize();
	}

	void MovementManager::UpdateEncumbranceFactor()
	{
		// Set the player's encumbrance factor, which is their inventory weight
		// divided by their carryweight. Any number greater or equal to 1
		// means that the player is over-encumbered.

		float inventoryWeight = coopActor->GetWeightInContainer();
		const auto invChanges = coopActor->GetInventoryChanges();
		if (invChanges)
		{
			inventoryWeight = invChanges->totalWeight;
		}

		encumbranceFactor = 
		(
			inventoryWeight / 
			max(1.0f, coopActor->GetActorValue(RE::ActorValue::kCarryWeight))
		);
	}

	void MovementManager::UpdateMovementParameters()
	{
		// Update player movement parameters derived from controller analog stick movement
		// in both in-game coordinates and absolute coordinates.

		const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
		const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
		// Analog stick components and normalized displacement magnitudes.
		const float& lsX = lsData.xComp;
		const float& lsY = lsData.yComp;
		const float& rsX = rsData.xComp;
		const float& rsY = rsData.yComp;
		const float& lsMag = lsData.normMag;
		const float& rsMag = rsData.normMag;
		// Orientation angle of controller thumbsticks. 
		// NOT relative to the camera.
		float lsAbsAng = 0.0f;
		float rsAbsAng = 0.0f;
		// Components of thumbstick displacement vectors.
		float lxComp = 0.0f;
		float lyComp = 0.0f;
		float rxComp = 0.0f;
		float ryComp = 0.0f;

		// Get camera yaw angle.
		auto playerCam = RE::PlayerCamera::GetSingleton();
		float playerCamYaw = Util::NormalizeAng0To2Pi(playerCam->yaw);
		float camYaw = playerCamYaw;
		// Use co-op cam-reported yaw when it's active; otherwise, use the NiCamera reported value.
		if (glob.cam->IsRunning() && 
			playerCam->currentState && 
			playerCam->currentState.get() &&
			playerCam->currentState->id == RE::CameraState::kThirdPerson) 
		{
			camYaw = glob.cam->GetCurrentYaw();
		}
		else if (auto niCamPtr = Util::GetNiCamera(); niCamPtr && niCamPtr.get()) 
		{
			// Player cam's yaw does not always correspond to 
			// the actual camera forward direction angle in certain camera states, 
			// such as the bleedout camera state, so get that info from the NiCamera.
			RE::NiPoint3 niEulerAngles = Util::GetEulerAnglesFromRotMatrix(niCamPtr->world.rotate);
			camYaw = niEulerAngles.z;
		}

		// Game yaw angle for the LS.
		float lsAng = 0.0f;
		// Game yaw angle for the RS.
		float rsAng = 0.0f;
		// Get movement speed multiplier based on what action is being performed.
		float attackMovMult = 1.0f;
		if (p->pam->isInCastingAnim)
		{
			attackMovMult *= Settings::fCastingMovMult;
		}
		else if (p->pam->isWeaponAttack && p->em->Has2HRangedWeapEquipped())
		{
			attackMovMult *= Settings::fRangedAttackMovMult;
		}
		else if (p->lastAnimEventTag == "preHitFrame" || p->lastAnimEventTag == "HitFrame")
		{
			attackMovMult *= Settings::fMeleeAttackMovMult;
		}

		// Obtain Cartesian angle for left stick orientation.
		if (lsX == 0.0f && lsY == 0.0f) 
		{
			// Previous, no change, since the LS is centered.
			lsAbsAng = movementOffsetParams[!MoveParams::kLSAbsoluteAng];
		}
		else
		{
			lsAbsAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(atan2f(lsY, lsX)));
		}

		if (rsX == 0.0f && rsY == 0.0f) 
		{
			// Previous, no change, since the RS is centered.
			rsAbsAng = movementOffsetParams[!MoveParams::kRSAbsoluteAng];
		}
		else
		{
			rsAbsAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(atan2f(rsY, rsX)));
		}

		// Yaw angles for both analog sticks in the world's coordinate space 
		// (relative to the camera).
		lsAng = Util::NormalizeAng0To2Pi(camYaw + lsAbsAng);
		rsAng = Util::NormalizeAng0To2Pi(camYaw + rsAbsAng);

		// Get the absolute change in LS angle since the last check.
		float deltaLSAngle = Util::NormalizeAngToPi(fabsf(oldLSAngle - lsAbsAng));
		oldLSAngle = lsAbsAng;

		// Get X, Y components for both analog sticks, with respect to the camera's yaw.
		if (rsMag != 0.0f)
		{
			rsAng = Util::ConvertAngle(rsAng);
			rxComp = cosf(rsAng);
			ryComp = sinf(rsAng);
			rsAng = Util::ConvertAngle(rsAng);
		}
		else
		{
			// Unchanged.
			rsAng = movementOffsetParams[!MoveParams::kRSGameAng];
			rxComp = 0.0f;
			ryComp = 0.0f;
		}

		if (lsMag != 0.0f)
		{
			lsAng = Util::ConvertAngle(lsAng);
			lxComp = cosf(lsAng);
			lyComp = sinf(lsAng);
			lsAng = Util::ConvertAngle(lsAng);
		}
		else
		{
			// Unchanged.
			lsAng = movementOffsetParams[!MoveParams::kLSGameAng];
			lxComp = 0.0f;
			lyComp = 0.0f;
		}

		// Speedmult to set this frame.
		float speedMult = baseSpeedMult;
		if (p->pam->isSprinting) 
		{
			// Co-op companion mounts accelerate more slowly for some reason. 
			// Scale up the speedmult to better match P1's mount speedmult.
			speedMult = attackMovMult * baseSpeedMult * Settings::fSprintingMovMult;
		}
		else if (isDashDodging || isRequestingDashDodge)
		{
			// Leave at default speedmult when dodging.
			speedMult = Settings::fBaseSpeed;
		}
		else
		{
			// Modify the base speed mult by the attack movement modifier and the LS magnitude.
			speedMult = attackMovMult * baseSpeedMult * lsMag;
		}

		// Over-encumbered.
		// Slow down companion players only, as the game already slows down P1 when encumbered.
		if (!p->isPlayer1 && !p->isInGodMode && encumbranceFactor >= 1.0f)
		{
			// Just an estimate, similar enough to P1's speed when encumbered.
			speedMult *= 0.45f;
		}

		// Set analog stick angles (relative to camera) at max displacement.
		prevLSAngAtMaxDisp = lsAngAtMaxDisp;
		prevRSAngAtMaxDisp = rsAngAtMaxDisp;
		lsAngAtMaxDisp = (lsMag == 1.0f) ? lsAng : lsAngAtMaxDisp;
		rsAngAtMaxDisp = (rsMag == 1.0f) ? rsAng : rsAngAtMaxDisp;

		// All angles are in game coordinates before adding to params list.
		movementOffsetParams[!MoveParams::kLSXComp] = lxComp;
		movementOffsetParams[!MoveParams::kLSYComp] = lyComp;
		movementOffsetParams[!MoveParams::kRSXComp] = rxComp;
		movementOffsetParams[!MoveParams::kRSYComp] = ryComp;
		movementOffsetParams[!MoveParams::kSpeedMult] = speedMult;
		movementOffsetParams[!MoveParams::kLSGameAng] = lsAng;
		movementOffsetParams[!MoveParams::kRSGameAng] = rsAng;
		movementOffsetParams[!MoveParams::kDeltaLSAbsoluteAng] = deltaLSAngle;
		movementOffsetParams[!MoveParams::kLSAbsoluteAng] = lsAbsAng;
		movementOffsetParams[!MoveParams::kRSAbsoluteAng] = lsAbsAng;
	}

	void MovementManager::UpdateMovementState()
	{
		// Update all things related to movement state and then some.
		// Mounted state, location discovery for P1, analog stick movement flags,
		// ragdoll state, sneak state, swimming state, start/stop movement state,
		// P1 AI driven toggling, and more.
		// Also perform jumps, dash dodges, and even paragliding, if requested.

		// Do not update movement state data if mounting.
		if (isMounting)
		{
			return;
		}

		// Check if P1 should have their AI driven flag cleared, 
		// which re-enables location discovery.
		SetShouldPerformLocationDiscovery();

		// Update analog stick state and menu movement flag.
		menuStopsMovement = Util::OpenMenuStopsMovement();
		bool prevLSMoved = lsMoved;
		// LS/RS stopped when centered for two frames (norm mag is 0 this frame and last frame).
		bool prevMoved = 
		(
			glob.cdh->GetAnalogStickState(controllerID, true).prevNormMag != 0.0f
		);
		lsMoved = 
		(
			(prevMoved) ||
			(
				movementOffsetParams[!MoveParams::kLSXComp] != 0.0f || 
				movementOffsetParams[!MoveParams::kLSYComp] != 0.0f
			)
		);
		prevMoved = 
		(
			glob.cdh->GetAnalogStickState(controllerID, false).prevNormMag != 0.0f
		);
		rsMoved =
		(
			(prevMoved) ||
			(
				movementOffsetParams[!MoveParams::kRSXComp] != 0.0f || 
				movementOffsetParams[!MoveParams::kRSYComp] != 0.0f
			)
		);

		if (prevLSMoved && !lsMoved) 
		{
			p->lastMovementStopReqTP = SteadyClock::now();
		}
		else if (!prevLSMoved && lsMoved)
		{
			p->lastMovementStartReqTP = SteadyClock::now();
		}
		
		// Save torso position this frame to use elsewhere.
		playerTorsoPosition = Util::GetTorsoPosition(coopActor.get());

		// Update current mount.
		if (!coopActor->IsOnMount() && Util::HandleIsValid(p->currentMountHandle))
		{
			// Clear mount if not mounted anymore.
			// Remove movement offset for the current mount 
			// so that it does not continue to move when the dismounted player moves.
			RE::ActorPtr mount = p->currentMountHandle.get();
			Util::NativeFunctions::ClearKeepOffsetFromActor(mount.get());
			p->currentMountHandle.reset();
		}
		else if (coopActor->IsOnMount() && !p->currentMountHandle.get())
		{
			RE::ActorPtr mount;
			coopActor->GetMount(mount);
			if (mount) 
			{
				// Clear any lingering movement offset before setting new mount.
				Util::NativeFunctions::ClearKeepOffsetFromActor(mount.get());
				p->currentMountHandle = mount->GetHandle();
			}
		}

		// Set movement actor (any mount if the player is mounted, player actor otherwise).
		auto mount = p->GetCurrentMount();
		movementActor = mount && mount.get() ? mount : coopActor;
		const float movementSpeed = movementActor->DoGetMovementSpeed();

		// Ensure animation sneak state syncs up with actor sneak state.
		if (!coopActor->IsOnMount() &&
			!coopActor->IsSwimming() && 
			!coopActor->IsFlying() && 
			!isRequestingDashDodge && 
			!isDashDodging)
		{
			if (coopActor->IsSneaking() && !p->pam->isSneaking)
			{
				coopActor->NotifyAnimationGraph("SneakStart");
			}
			else if (!coopActor->IsSneaking() && p->pam->isSneaking)
			{
				coopActor->NotifyAnimationGraph("SneakStop");
			}
		}

		// Having Precision's ragdolling system enabled while triggering a
		// knock explosion elsewhere in this plugin seems to contribute, in part,
		// to an improper ragdoll reset position glitch 
		// where the hit actor is teleported to their last ragdoll position
		// instead of staying at their current position.
		// Is a major issue if the last ragdoll position was far away or in another cell entirely.
		// Precision is re-enabled on the actor after they get up.

		bool isRagdolled = coopActor->IsInRagdollState();
		if (isRagdolled && !playerRagdollTriggered)
		{
			if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI3; api)
			{
				api->ToggleDisableActor(coopActor->GetHandle(), true);
			}

			coopActor->PotentiallyFixRagdollState();
			playerRagdollTriggered = true;
		}
		else if (!isRagdolled && 
				 playerRagdollTriggered && 
				 coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal)
		{
			if (auto api = ALYSLC::PrecisionCompat::g_precisionAPI3; api)
			{
				api->ToggleDisableActor(coopActor->GetHandle(), false);
			}

			coopActor->PotentiallyFixRagdollState();
			playerRagdollTriggered = false;
		}

		// Update getup TP and flag.
		bool wasGettingUp = isGettingUp;
		auto knockState = coopActor->GetKnockState(); 
		if (knockState == RE::KNOCK_STATE_ENUM::kGetUp)
		{
			isGettingUp = true;
		}
		else
		{
			// Curtail momentum:
			// Seems as if there is some momentum carryover from before the player ragdolled,
			// so if they were moving at a high rate of speed and then ragdolled,
			// they'd shoot forward in their movement direction after fully getting up.
			bool finishedGettingUp = wasGettingUp && knockState == RE::KNOCK_STATE_ENUM::kNormal;
			// Stop instantly to prevent the player from slowly coming to a halt 
			// from residual momentum when turning towards or away from a target 
			// while attacking/bashing/blocking/casting.
			bool turnToFaceTargetWhileStopped = 
			{
				(!lsMoved && movementSpeed != 0.0f) &&
				(
					p->pam->isAttacking || p->pam->isBlocking ||
					p->pam->isBashing || p->pam->isInCastingAnim
				)
			};
			// Also stop the player momentarily to dampen residual momentum 
			// when turning to face a target.
			bool stopWhenTurningToTarget = 
			(
				(movementYawTargetChanged) && (turnToTarget || faceTarget)
			);
			if (finishedGettingUp || turnToFaceTargetWhileStopped || stopWhenTurningToTarget)
			{
				if (finishedGettingUp)
				{
					p->lastGetupTP = SteadyClock::now();
				}

				if (!shouldCurtailMomentum)
				{
					SPDLOG_DEBUG
					(
						"[MM] UpdateMovementState: {}: "
						"finishedGettingUp: {}, turnToFaceTargetWhileStopped: {}, "
						"movementYawTargetChanged: {}, "
						"turnToTarget: {}, faceTarget: {}.",
						coopActor->GetName(),
						finishedGettingUp,
						turnToFaceTargetWhileStopped,
						movementYawTargetChanged,
						p->pam->isPowerAttacking,
						turnToTarget,
						faceTarget
					);
				}

				shouldCurtailMomentum = true;
			}

			isGettingUp = false;
		}
		
		bool allowRotation = false;
		coopActor->GetGraphVariableBool("bAllowRotation", allowRotation);
		// Do not curtail momentum when this flag is set and the player is not knocked down
		// because doing so will stop the player from moving when performing
		// directional power attacks.
		if (allowRotation)
		{
			shouldCurtailMomentum = false;
		}
		else
		{
			// Freeze the player in place and wait until their reported movement speed is 0.
			shouldCurtailMomentum &= movementSpeed > 0.0f;
		}

		auto charController = movementActor->GetCharController(); 
		// Everything below requires a valid character controller first.
		if (!charController)
		{
			return;
		}

		// Is the player.
		if (!movementActor->IsAMount())
		{
			const auto& currentCharacterState = charController->context.currentState;
			// Jump up and get down.
			if (reqStartJump || isAirborneWhileJumping)
			{
				PerformJump();
			}
			else if ((currentCharacterState == RE::hkpCharacterStateType::kOnGround) &&
					 (coopActor->actorState1.walking || coopActor->actorState1.running))
			{
				// Prevent P1 from mysteriously dying after moving down a slope 
				// and jumping at the bottom.
				// The proxy controller fails to reset the fall state 
				// and fall damage is applied to the jump as if the player had jumped
				// all the way down from the top of the slope.
				if (charController && charController->fallStartHeight != 0.0f)
				{
					// Reset fall state.
					charController->lock.Lock();
					{
						charController->fallStartHeight = 0.0f;
						charController->fallTime = 0.0f;
					}
					charController->lock.Unlock();
				}
			}
				
			// Check if the player has started/stopped swimming 
			// and play the appropriate animation.
			// Keep actor state and animation state in sync.
			if (!isSwimming && coopActor->IsSwimming())
			{
				if (p->isPlayer1)
				{
					coopActor->NotifyAnimationGraph("SwimStart");
				}

				reqResetAimAndBody = true;
				isSwimming = true;
			}
			else if (isSwimming && !coopActor->IsSwimming())
			{
				if (p->isPlayer1)
				{
					coopActor->NotifyAnimationGraph("swimStop");
				}

				isSwimming = false;
			}

			// Perform dash dodge.
			if (isRequestingDashDodge || isDashDodging) 
			{
				PerformDashDodge();
			}
				
			// Perform a magical-paraglide alternative which looks like trash.
			if (isParagliding || shouldParaglide || !isParaglidingTiltAngleReset) 
			{
				PerformMagicalParaglide();
			}
		}
		else
		{
			// Mounted jump is animation event driven.
			if (reqStartJump)
			{
				bool mountMoving = charController->speedPct > 0.0f;
				if (mountMoving)
				{
					mount->NotifyAnimationGraph("forwardJumpStart");
				}
				else
				{
					mount->NotifyAnimationGraph("StandingRearUp");
				}

				reqStartJump = false;
			}
		}

		// P1 only.
		// Toggle AI driven as needed.
		if (p->isPlayer1 && glob.player1Actor && glob.player1Actor.get())
		{
			bool isActivating = 
			{
				p->pam->IsPerformingOneOf
				(
					InputAction::kActivate, InputAction::kActivateAllOfType
				) ||
				p->pam->AllInputsPressedForAtLeastOneAction
				(
					InputAction::kActivate, InputAction::kActivateAllOfType
				)
			};
			bool isMounted = coopActor->IsOnMount();
			// Requesting to paraglide or is paragliding.
			bool reqOrIsParagliding =
			{ 
				(isParagliding) || 
				(
					ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider &&
					isActivating && 
					glob.player1Actor->GetCharController() && 
					glob.player1Actor->GetCharController()->context.currentState ==
					RE::hkpCharacterStateType::kInAir
				) 
			};

			// Credits to ersh1 for finding out the movement handler's motion driven flag:
			// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/DirectionalMovementHandler.cpp
			bool isAIDriven = 
			(
				glob.player1Actor->movementController && 
				!glob.player1Actor->movementController->controlsDriven
			);
			// NOTE: 
			// Also when an event causes P1 to ragdoll, 
			// they will not exit the ragdolling state and attempt to get up 
			// unless AI driven is unset.
			bool shouldRemoveAIDriven =	
			{
				p->pam->sendingP1MotionDrivenEvents ||
				isMounted ||
				isRagdolled ||
				reqOrIsParagliding ||
				menuStopsMovement ||
				attemptDiscovery
			};

			if (isAIDriven && shouldRemoveAIDriven)
			{
				SPDLOG_DEBUG
				(
					"[MM] UpdateMovementState: "
					"{} is anim driven: {}, mounted: {}, "
					"ragdolled: {}, synced: {}, paragliding: {}. "
					"Sending motion driven events: {}, "
					"menu stops movement: {}, attempt discovery: {}. "
					"REMOVE AI driven.",
					glob.player1Actor->GetName(), 
					isAnimDriven, 
					isMounted, 
					isRagdolled, 
					isSynced, 
					reqOrIsParagliding,
					p->pam->sendingP1MotionDrivenEvents,
					menuStopsMovement,
					attemptDiscovery
				);
				bool changed = Util::SetPlayerAIDriven(false);
				if (changed)
				{
					SPDLOG_DEBUG
					(
						"[MM] UpdateMovementState: {} AI driven state changed to false.",
						coopActor->GetName()
					);
				}
			}
			else if (!isAIDriven && !shouldRemoveAIDriven)
			{
				SPDLOG_DEBUG
				(
					"[MM] UpdateMovementState: "
					"{} is anim driven: {}, mounted: {}, "
					"ragdolled: {}, synced: {}, paragliding: {}. "
					"Sending motion driven events: {}, "
					"menu stops movement: {}, attempt discovery: {}. "
					"SET AI driven.",
					glob.player1Actor->GetName(),
					isAnimDriven,
					isMounted, 
					isRagdolled,
					isSynced, 
					reqOrIsParagliding, 
					p->pam->sendingP1MotionDrivenEvents,
					menuStopsMovement, 
					attemptDiscovery
				);
				bool changed = Util::SetPlayerAIDriven(true);
				if (changed)
				{
					SPDLOG_DEBUG
					(
						"[MM] UpdateMovementState: {} AI driven state changed to true.", 
						coopActor->GetName()
					);
				}
			}
		}

		// Set start or stop movement flags.
		bool isMoving = 
		{
			(movementSpeed > 0.0f) &&
			(
				movementActor->actorState1.movingBack | 
				movementActor->actorState1.movingForward |
				movementActor->actorState1.movingLeft | 
				movementActor->actorState1.movingRight |
				movementActor->actorState1.running |
				movementActor->actorState1.sprinting |
				movementActor->actorState1.swimming
			) != 0
		};
		bool isTKDodging = false;
		bool isTDMDodging = false;
		coopActor->GetGraphVariableBool("bAnimationDriven", isAnimDriven);
		coopActor->GetGraphVariableBool("bIsSynced", isSynced);
		coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
		coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
		auto interactionPackage = 
		(
			glob.coopPackages
			[!PackageIndex::kTotal * controllerID + !PackageIndex::kSpecialInteraction]
		);
		interactionPackageRunning = p->pam->GetCurrentPackage() == interactionPackage;

		// Stop moving if currently moving and not dodging, 
		// and if the LS is centered, a menu stops movement, 
		// the player is reviving a buddy, or if attempting discovery.
		shouldStopMoving = 
		{
			(
				isMoving && 
				!isDashDodging &&
				!isRequestingDashDodge && 
				!isTKDodging && 
				!isTDMDodging
			) && 
			(
				!lsMoved || 
				menuStopsMovement || 
				p->isRevivingPlayer || 
				attemptDiscovery
			)
		};

		// Start movement if the LS is displaced and if the player is not moving or dodging,
		// not running an interaction package, not prevented from moving by a menu, 
		// and not reviving another player.
		shouldStartMoving = 
		{
			lsMoved && 
			!isMoving && 
			!isDashDodging && 
			!isRequestingDashDodge && 
			!interactionPackageRunning && 
			!menuStopsMovement && 
			!p->isRevivingPlayer
		};
	}

	void NodeOrientationManager::ApplyCustomNodeRotation
	(
		const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_nodePtr
	)
	{
		// If the node's rotation is handled by our node orientation manager,
		// apply our custom rotation to the given node.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		// Check if a supported node first.
		bool isLeftArmNode = GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES.contains(a_nodePtr->name);
		bool isRightArmNode = GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODES.contains(a_nodePtr->name);
		bool isTorsoNode = GlobalCoopData::ADJUSTABLE_TORSO_NODES.contains(a_nodePtr->name);
		if (!isLeftArmNode && !isRightArmNode && !isTorsoNode)
		{
			return;
		}

		if (!nodeNameToRotationDataMap.contains(a_nodePtr->name))
		{
			return;
		}

		const auto& data = nodeNameToRotationDataMap[a_nodePtr->name];

		// Set default rotation.
		data->defaultRotation = a_nodePtr->local.rotate;

		// Set new local rotations before the UpdateDownwardPass() call,
		// so that it can use our modified local rotations to set the nodes' new world rotations.
		if (isTorsoNode || Settings::bEnableArmsRotation)
		{
			a_nodePtr->local.rotate = data->currentRotation;
		}
	}

	void NodeOrientationManager::CheckAndPerformArmCollisions
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Setup raycasting start and end positions
		// along the lengths of the player's arms (approximation).
		// Then perform said raycasts and check for collisions.
		// Finally, if certain conditions hold, apply impulses/knockdowns/damage to hit objects.

		if (!glob.coopSessionActive)
		{
			return;
		}

		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		// Not moving the right stick, so no need to check for collisions.
		const auto& rsLinSpeed = 
		(
			glob.cdh->GetAnalogStickState(a_p->controllerID, false).stickLinearSpeed
		);
		if (rsLinSpeed == 0.0f)
		{
			return;
		}

		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		{
			std::unique_lock<std::mutex> lock(rotationDataMutex);
			const auto& uiRGBA = Settings::vuOverlayRGBAValues[a_p->playerID];
			std::vector<RE::BSFixedString> nodeNamesToCheck{};
			bool checkLeftArm = a_p->pam->IsPerformingOneOf
			(
				InputAction::kRotateLeftShoulder,
				InputAction::kRotateLeftForearm, 
				InputAction::kRotateLeftHand
			);
			bool checkRightArm = a_p->pam->IsPerformingOneOf
			(
				InputAction::kRotateRightShoulder, 
				InputAction::kRotateRightForearm, 
				InputAction::kRotateRightHand
			);
			if (checkLeftArm)
			{
				nodeNamesToCheck.emplace_back("NPC L Hand [LHnd]");
				nodeNamesToCheck.emplace_back(strings->npcLForearm);
				nodeNamesToCheck.emplace_back(strings->npcLUpperArm);
			}

			if (checkRightArm)
			{
				nodeNamesToCheck.emplace_back("NPC R Hand [RHnd]");
				nodeNamesToCheck.emplace_back("NPC R Forearm [RLar]");
				nodeNamesToCheck.emplace_back(strings->npcRUpperArm);
			}

			const uint32_t raycastsPerNode = 5;
			bool noPreviousHit = true;
			float staminaCost = 0.0f;
			for (const auto& name : nodeNamesToCheck)
			{
				auto nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(name)); 
				if (!nodePtr || !nodePtr.get())
				{
					continue;
				}

				// Update position and velocity first.
				const auto& playerPos = a_p->coopActor->data.location;
				const auto& nodeWorldPos = nodePtr->world.translate;
				RE::NiPoint3 localPos = nodeWorldPos - playerPos;
				if (!nodeNameToRotationDataMap.contains(nodePtr->name))
				{
					// Reset rotation and position to current values.
					const auto& newData = 
					(
						*nodeNameToRotationDataMap.insert_or_assign
						(
							nodePtr->name, std::make_unique<NodeRotationData>()
						).first
					).second;
					newData->currentRotation =
					newData->defaultRotation =
					newData->startingRotation =
					newData->targetRotation = nodePtr->local.rotate;
					newData->localPosition = localPos;
				}

				// Set local velocity and position.
				const auto& nodeData = nodeNameToRotationDataMap[nodePtr->name];
				RE::NiPoint3 velocity = 
				(
					(localPos - nodeData->localPosition) / (*g_deltaTimeRealTime)
				);
				nodeData->localPosition = localPos;
				nodeData->localVelocity = velocity;

				// Set arm node type, which determines how raycasts and impacts are calculated.
				ArmNodeType armNodeType = ArmNodeType::kShoulder;
				if (nodePtr->name == "NPC L Hand [LHnd]" || nodePtr->name == "NPC R Hand [RHnd]")
				{
					armNodeType = ArmNodeType::kHand;
				}
				else if (nodePtr->name == strings->npcLForearm || 
						 nodePtr->name == "NPC R Forearm [RLar]")
				{
					armNodeType = ArmNodeType::kForearm;
				}

				// Need arm rigid body and capsule shape to determine raycast
				// start and end points.
				auto armHkpRigidBody = Util::GethkpRigidBody(nodePtr.get()); 
				if (!armHkpRigidBody || !armHkpRigidBody.get())
				{
					continue;
				}

				auto hkpShape = armHkpRigidBody->GetShape(); 
				if (hkpShape->type != RE::hkpShapeType::kCapsule)
				{
					continue;
				}

				auto hkpCapsuleShape = static_cast<const RE::hkpCapsuleShape*>(hkpShape);
				RE::NiPoint3 vertexA;
				RE::NiPoint3 vertexB;
				RE::NiPoint3 zAxisDir;
				const float& capsuleRadius = hkpCapsuleShape->radius;
				// Special raycast offset and rotation handling for hand nodes,
				// since rigidbody capsule axes don't line up with the hands' node orientations.
				if (armNodeType == ArmNodeType::kHand)
				{
					zAxisDir = RE::NiPoint3
					(
						nodePtr->world.rotate.entry[0][2],
						nodePtr->world.rotate.entry[1][2],
						nodePtr->world.rotate.entry[2][2]
					);
					zAxisDir.Unitize();

					RE::NiPoint3 handCenterPos = 
					(
						nodePtr->world.translate + zAxisDir * capsuleRadius * 1.5f
					);
					if (zAxisDir.z > 0.0f)
					{
						vertexA = handCenterPos + zAxisDir * capsuleRadius;
						vertexB = handCenterPos - zAxisDir * capsuleRadius;
					}
					else
					{
						vertexA = handCenterPos - zAxisDir * capsuleRadius;
						vertexB = handCenterPos + zAxisDir * capsuleRadius;
					}
				}
				else
				{
					const auto& hkTransform = armHkpRigidBody->motion.motionState.transform;
					RE::NiPoint3 vertAOffset = 
					(
						ToNiPoint3(hkpCapsuleShape->vertexA) * HAVOK_TO_GAME
					);
					RE::NiPoint3 vertBOffset = 
					(
						ToNiPoint3(hkpCapsuleShape->vertexB) * HAVOK_TO_GAME
					);
					RE::NiTransform niTransform;
					niTransform.scale = 1.0f;
					niTransform.translate = ToNiPoint3(hkTransform.translation) * HAVOK_TO_GAME;
					// Set each corresponding column from the havok rotation matrix.
					niTransform.rotate.entry[0][0] = hkTransform.rotation.col0.quad.m128_f32[0];
					niTransform.rotate.entry[1][0] = hkTransform.rotation.col0.quad.m128_f32[1];
					niTransform.rotate.entry[2][0] = hkTransform.rotation.col0.quad.m128_f32[2];

					niTransform.rotate.entry[0][1] = hkTransform.rotation.col1.quad.m128_f32[0];
					niTransform.rotate.entry[1][1] = hkTransform.rotation.col1.quad.m128_f32[1];
					niTransform.rotate.entry[2][1] = hkTransform.rotation.col1.quad.m128_f32[2];

					niTransform.rotate.entry[0][2] = hkTransform.rotation.col2.quad.m128_f32[0];
					niTransform.rotate.entry[1][2] = hkTransform.rotation.col2.quad.m128_f32[1];
					niTransform.rotate.entry[2][2] = hkTransform.rotation.col2.quad.m128_f32[2];
					vertexA = niTransform * vertAOffset;
					vertexB = niTransform * vertBOffset;
					RE::NiPoint3 temp = 
					(
						vertexA.z > vertexB.z ? 
						vertexA - vertexB : 
						vertexB - vertexA
					);
					temp.Unitize();
					zAxisDir = { temp.x, temp.y, temp.z };
				}

				// Extend a little past the capsule edges for some overlap between nodes.
				const glm::vec4 zAxisDirVec = ToVec4(zAxisDir);
				float capsuleLength = 
				(
					(vertexB - vertexA).Length() + 2.5f * hkpCapsuleShape->radius * HAVOK_TO_GAME
				);
				// Node position to offset casts from.
				// At one end of the capsule.
				glm::vec4 originPos =
				(
					vertexA.z > vertexB.z ?
					ToVec4(vertexB - zAxisDir * hkpCapsuleShape->radius) :
					ToVec4(vertexA - zAxisDir * hkpCapsuleShape->radius)
				);
				// Velocity direction 
				glm::vec4 velDir{};
				glm::vec4 startPos{};
				glm::vec4 endPos{};
				uint32_t castNum = 1;
				bool hit = false;
				float impulseApplied = 0.0f;
				uint32_t impulseHits = 0;
				std::vector<RE::TESObjectREFRPtr> hitRefrPtrs{ };
				while (castNum <= raycastsPerNode)
				{
					// Offset by a fraction of the capsule length
					// from one end (on the first cast) to the other end (on the last cast).
					startPos = 
					(
						originPos + 
						(zAxisDirVec * (capsuleLength / raycastsPerNode)) * 
						static_cast<float>(castNum)
					);
					velDir = ToVec4
					(
						armHkpRigidBody->motion.GetPointVelocity
						(
							TohkVector4(startPos) * GAME_TO_HAVOK
						)
					);
					float havokSpeed = glm::length(velDir);
					// Cast further out when the starting point is moving fast.
					float havokSpeedFactor = std::lerp
					(
						1.5f, 
						4.0f, 
						std::clamp(havokSpeed / 20.0f, 0.0f, 1.0f)
					);
					velDir = 
					(
						havokSpeed <= 1e-5f ? 
						glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) : 
						glm::normalize(velDir)
					);
					endPos = 
					(
						startPos + 
						(velDir * hkpCapsuleShape->radius * havokSpeedFactor * HAVOK_TO_GAME)
					);
					bool hit = PerformArmCollision
					(
						a_p,
						startPos,
						endPos,
						nodePtr.get(),
						armNodeType,
						noPreviousHit,
						staminaCost
					);

					// Don't play hit sound after the first raycast hit this frame.
					if (hit)
					{
						noPreviousHit = false;
					}

					// REMOVE when done debugging.
					/*DebugAPI::QueueArrow3D
					(
						startPos, 
						endPos, 
						Settings::vuOverlayRGBAValues[a_p->playerID],
						5.0f,
						2.0f, 
						0.0f
					);*/

					// Leaving commented out for now:
					// For better performance, if there is a hit,
					// break early and move on to the next node.
					/*if (hit)
					{
						break;
					}*/

					++castNum;
				}
			}

			// Do not expend stamina if in god mode or the cost is 0.
			if (a_p->isInGodMode || staminaCost == 0.0f)
			{
				return;
			}

			a_p->pam->ExpendStamina(staminaCost);
		}
	}

	void NodeOrientationManager::DisplayAllNodeRotations(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Draw X, Y, and Z world axes for all/supported player nodes.
		
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		auto obj3DPtr = Util::Get3DObjectByName(a_p->coopActor.get(), strings->npc);
		if (!obj3DPtr || !obj3DPtr.get())
		{
			return;
		}

		auto nodePtr = RE::NiPointer<RE::NiNode>(obj3DPtr->AsNode()); 
		if (!nodePtr || !nodePtr.get())
		{
			return;
		}
		
		DisplayAllNodeRotations(nodePtr);
	}

	void NodeOrientationManager::DisplayAllNodeRotations
	(
		const RE::NiPointer<RE::NiNode>& a_nodePtr
	)
	{
		// Recursively save adjustable nodes' world rotations 
		// by walking the player's node tree from the given node.
		// The parent world transform is modified to the current node's world transform
		// before traversing its children.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

		const auto& newWorldRot = a_nodePtr->world.rotate;
		auto worldXAxis = RE::NiPoint3(newWorldRot * right);
		auto worldYAxis = RE::NiPoint3(newWorldRot * forward);
		auto worldZAxis = RE::NiPoint3(newWorldRot * up);
		glm::vec3 start
		{
			a_nodePtr->world.translate.x, 
			a_nodePtr->world.translate.y,
			a_nodePtr->world.translate.z
		};
		glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 5.0f };
		glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 5.0f };
		glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 5.0f };
		DebugAPI::QueueArrow3D(start, endX, 0xFF000088, 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endY, 0x00FF0088, 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endZ, 0x0000FF88, 3.0f, 2.0f);

		for (const auto child : a_nodePtr->children)
		{
			if (!child || !child.get() || !child->AsNode())
			{
				continue;
			}

			auto childNode = RE::NiPointer<RE::NiNode>(child->AsNode());
			DisplayAllNodeRotations(childNode);
		}
	}

	void NodeOrientationManager::InstantlyResetAllNodeData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Reset all node data.

		defaultNodeLocalTransformsMap.clear();
		defaultNodeWorldTransformsMap.clear();
		nodeNameToRotationDataMap.clear();

		// NOTE:
		// Not used at the moment.
		// Remove all colliders.
		/*Util::ChangeNodeColliderState
		(
			a_p->coopActor.get(),
			nullptr, 
			PrecisionAnnotationReqType::kRemoveAll
		);*/

		// Return early if the player's loaded 3D data is invalid.
		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		// Return early if the player's 3D is invalid.
		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		// Left arm first.
		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES) 
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get()) 
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData =
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}

		// Right arm.
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODES)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}

		// Torso.
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_TORSO_NODES)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}
	}

	void NodeOrientationManager::InstantlyResetArmNodeData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Only reset arm node data.
		
		// NOTE:
		// Unused for now.
		// Remove all colliders.
		/*Util::ChangeNodeColliderState
		(
			a_p->coopActor.get(), 
			nullptr,
			PrecisionAnnotationReqType::kRemoveAll
		);*/

		// Return early if the player's loaded 3D data is invalid.
		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		// Return early if the player's 3D is invalid.
		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		// Left arm.
		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}

		// Right arm.
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODES)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}
	}

	void NodeOrientationManager::InstantlyResetTorsoNodeData
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Only reset torso node data.
		
		// NOTE:
		// Unused for now.
		// Remove all colliders.
		/*Util::ChangeNodeColliderState
		(
			a_p->coopActor.get(),
			nullptr, 
			PrecisionAnnotationReqType::kRemoveAll
		);*/

		// Return early if the player's loaded 3D data is invalid.
		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		// Return early if the player's 3D is invalid.
		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		// All torso nodes.
		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_TORSO_NODES)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			// Reset rotation and position to current values.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					nodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation =
			newData->defaultRotation =
			newData->startingRotation =
			newData->targetRotation = nodePtr->local.rotate;
			newData->localPosition = nodePtr->world.translate - a_p->coopActor->data.location;
		}
	}

	bool NodeOrientationManager::NodeWasAdjusted(const RE::NiPointer<RE::NiAVObject>& a_nodePtr)
	{
		// Returns true if a custom cached rotation was set for the given node.
		// Can check for the node name hash in either the set of adjustable arm or torso nodes.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return false;
		}

		if (!nodeNameToRotationDataMap.contains(a_nodePtr->name)) 
		{
			return false;
		}

		const auto& data = nodeNameToRotationDataMap.at(a_nodePtr->name); 
		if (!data || !data.get()) 
		{
			return false;
		}
		
		return data->rotationModified;
	}

	bool NodeOrientationManager::PerformArmCollision
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const glm::vec4& a_startPos, 
		const glm::vec4& a_endPos, 
		RE::NiAVObject* a_armNode,
		const ArmNodeType& a_armNodeType,
		const bool& a_noPreviousHit,
		float& a_staminaCostOut
	)
	{
		// Perform raycast and iterate through results,
		// applying impulses to hit actors and forces to inanimate objects
		// at the raycasts' hit positions.
		// If an actor is hit hard enough
		// (hit speed above certain thresholds, depending on the connecting node),
		// knock the actor down and apply damage.
		// Return true if a 'collision' was applied through a Havok impulse.

		if (!a_armNode)
		{
			return false;
		}

		auto armHkpRigidBody = Util::GethkpRigidBody(a_armNode);
		if (!armHkpRigidBody || !armHkpRigidBody.get())
		{
			return false;
		}
		
		// Ensure the arm node is accounted for in the rotation data map before continuing.
		if (!nodeNameToRotationDataMap.contains(a_armNode->name))
		{
			// Set all rotations to the node's current local rotation 
			// so we have valid data to start off with.
			const auto& newData = 
			(
				(
					*nodeNameToRotationDataMap.insert_or_assign
					(
						a_armNode->name, 
						std::make_unique<NodeRotationData>()
					).first
				).second
			);
			newData->currentRotation = 
			newData->defaultRotation = 
			newData->startingRotation = 
			newData->targetRotation = a_armNode->local.rotate;
		}
		
		// Rotation data for this node.
		const auto& data = nodeNameToRotationDataMap.at(a_armNode->name);
		// Raycast and get all hit results, so we can apply multiple impulses
		// along the arm swing trajectory and create more of a 'push'
		// instead of a one-off impulse.
		auto raycastResults = Raycast::GetAllHavokCastHitResults(a_startPos, a_endPos);
		for (uint32_t i = 0; i < raycastResults.size(); ++i)
		{
			const auto& result = raycastResults[i];
			if (!result.hit || !result.hitObjectPtr || !result.hitObjectPtr.get())
			{
				continue;
			}

			auto hitRefrPtr = RE::TESObjectREFRPtr
			(
				Util::GetRefrFrom3D(result.hitObjectPtr.get())
			);
			// Ignore hits to the player themselves.
			// Stop hitting yourself.
			if (!hitRefrPtr || !hitRefrPtr.get() || hitRefrPtr == a_p->coopActor)
			{
				continue;
			}

			// Need a valid rigid body to apply an impulse to.
			auto hitHkpRigidBodyPtr = Util::GethkpRigidBody(result.hitObjectPtr.get()); 
			if (!hitHkpRigidBodyPtr || !hitHkpRigidBodyPtr.get())
			{
				continue;
			}

			// Hit position and velocity in game coordinates.
			auto hitPosVec = TohkVector4(result.hitPos * GAME_TO_HAVOK);
			float hitSpeed =
			(
				armHkpRigidBody->motion.GetPointVelocity(hitPosVec) * HAVOK_TO_GAME
			).Length3();
			if (hitSpeed == 0.0f)
			{
				hitSpeed =
				(
					Util::GetParentNodeHavokPointVelocity(a_armNode, hitPosVec) * HAVOK_TO_GAME
				).Length3();
			}

			// Not enough speed to apply an impulse, so continue;
			if (hitSpeed == 0.0f)
			{
				continue;
			}

			// Activate the hit rigid body to allow for impulse/force application.
			Util::NativeFunctions::hkpEntity_Activate(hitHkpRigidBodyPtr.get());
			// Get handle for the refr.
			auto refHandle = hitRefrPtr->GetHandle();
			
			// Ensure we do not slap something another player is grabbing.
			bool grabbedByAnotherPlayer = false;
			for (const auto& otherP : glob.coopPlayers)
			{
				if (!otherP->isActive || otherP == a_p)
				{
					continue;
				}

				if (!grabbedByAnotherPlayer && 
					otherP->tm->rmm->IsManaged(refHandle, true))
				{
					grabbedByAnotherPlayer = true;
				}

				// If released by another player, 
				// remove this refr from that player's released refr manager,
				// since this player could grab it.
				if (otherP->tm->rmm->IsManaged(refHandle, false))
				{
					otherP->tm->rmm->ClearRefr(refHandle);
				}
			}

			// Use our local velocity relative to the player's root node for better hit reg.
			auto hitVelocity = data->localVelocity;
			hitVelocity.Unitize();
			hitVelocity *= hitSpeed;
			// Volume scaling factor to apply to the hit sound we will play.
			// Scales with the inverse square of the hit speed.
			float hitVolume = 0.0f;
			// Strong enough to floor someone?
			bool slapKnockdown = false;
			auto hitActor = hitRefrPtr->As<RE::Actor>();
			if (hitActor)
			{
				// Stop the hit actor from attacking when the slap connects.
				if ((Settings::bSlapsStopAttacksAndBlocking) && 
					(hitActor->IsAttacking() || hitActor->IsBlocking()))
				{
					hitActor->NotifyAnimationGraph("attackStop");
					hitActor->NotifyAnimationGraph("recoilStart");
				}

				auto precisionAPI4 = ALYSLC::PrecisionCompat::g_precisionAPI4; 
				// Require Precision's API to apply a havok impulse.
				if (!precisionAPI4)
				{
					continue;
				}

				// Only do not check if knockdowns are disabled.
				bool checkHitSpeed = 
				(
					Settings::uSlapKnockdownCriteria != !SlapKnockdownCriteria::kNoKnockdowns
				);
				float armForceFactor = a_p->mm->GetArmRotationFactor(false);
				float invArmForceFactor = 1.0f / armForceFactor;
				// Harder to generate enough arm velocity when facing a target,
				// so reduce the required hit speed to trigger a knockdown in this case.
				float knockdownMinSpeed = 2000.0f * invArmForceFactor;
				switch (a_armNodeType)
				{
				case ArmNodeType::kForearm:
				{
					knockdownMinSpeed = 
					(
						(a_p->mm->reqFaceTarget ? 1100.0f : 1600.0f) * invArmForceFactor
					);

					break;
				}
				case ArmNodeType::kHand:
				{
					knockdownMinSpeed = 
					(
						(a_p->mm->reqFaceTarget ? 1000.0f : 1500.0f) * invArmForceFactor
					);

					break;
				}
				case ArmNodeType::kShoulder:
				{
					knockdownMinSpeed = 
					(
						(a_p->mm->reqFaceTarget ? 700.0f : 1200.0f) * invArmForceFactor
					);

					break;
				}
				default:
					break;
				}
				
				// Set speed ratio and hit volume after setting the min knockdown speed.
				float hitToKnockdownSpeedRatio = min(1.0f, hitSpeed / knockdownMinSpeed);
				hitVolume = min(2.0f, 2.0f * hitToKnockdownSpeedRatio);
				auto handle = hitActor->GetHandle();
				bool isAlreadyGrabbed = a_p->tm->rmm->IsManaged(handle, true);
				if (isAlreadyGrabbed)
				{
					// If already grabbed, the actor is already ragdolled
					// and we can release it on hit.
					slapKnockdown = true;
				}
				else
				{
					if (checkHitSpeed)
					{
						slapKnockdown = hitSpeed > knockdownMinSpeed;
					}

					// If set, must hit the actor's head to trigger a knockdown,
					// in addition to meeting the requisite arm velocity.
					if (slapKnockdown && 
						Settings::uSlapKnockdownCriteria == 
						!SlapKnockdownCriteria::kOnlyStrongHeadshots)
					{
						slapKnockdown = false;
						// Get the actor's head node from their head body part,
						// if one exists.
						RE::BGSBodyPart* headBP = nullptr;
						if (hitActor->race &&
							hitActor->race->bodyPartData &&
							hitActor->race->bodyPartData->parts)
						{
							auto bpDataList = hitActor->race->bodyPartData->parts;
							headBP = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kHead];
							if (!headBP)
							{
								headBP = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kEye];
								if (!headBP)
								{
									headBP = bpDataList[RE::BGSBodyPartDefs::LIMB_ENUM::kLookAt];
								}
							}
						}

						bool hasHeadNode = false;
						if (headBP)
						{
							const auto actor3DPtr = Util::GetRefr3D(hitActor);
							if (actor3DPtr && actor3DPtr.get())
							{
								auto headNode = RE::NiPointer<RE::NiAVObject>
								(
									actor3DPtr->GetObjectByName(headBP->targetName)
								);

								if (headNode && headNode.get())
								{
									hasHeadNode = true;
									// Direct hits to the head node will trigger a knockdown.
									if (result.hitObjectPtr == headNode)
									{
										slapKnockdown = true;
									}
									else
									{
										// Secondary check, since sometimes the hit object
										// is the actor's skeleton, 
										// but the hit position is visually on the actor's head.
										// Will trigger a knockdown if the hits lands
										// within the actor's head node capsule,
										// which is approximated as a sphere here for simplicity.
										auto headRigidBody = Util::GethkpRigidBody
										(
											headNode.get()
										); 
										if (headRigidBody && headRigidBody.get())
										{
											auto hkpShape = headRigidBody->GetShape(); 
											if (hkpShape->type == RE::hkpShapeType::kCapsule)
											{
												auto hkpCapsuleShape = 
												(
													static_cast<const RE::hkpCapsuleShape*>
													(
														hkpShape
													)
												);
												RE::NiPoint3 vertexA;
												RE::NiPoint3 vertexB;
												RE::NiPoint3 zAxisDir;

												const auto& hkTransform = 
												(
													headRigidBody->motion.motionState.transform
												);
												RE::NiPoint3 vertAOffset = 
												(
													ToNiPoint3(hkpCapsuleShape->vertexA) * 
													HAVOK_TO_GAME
												);
												RE::NiPoint3 vertBOffset = 
												(
													ToNiPoint3(hkpCapsuleShape->vertexB) * 
													HAVOK_TO_GAME
												);
												RE::NiTransform niTransform;
												niTransform.scale = 1.0f;
												niTransform.translate = 
												(
													ToNiPoint3(hkTransform.translation) * 
													HAVOK_TO_GAME
												);
												niTransform.rotate.entry[0][0] = 
												hkTransform.rotation.col0.quad.m128_f32[0];
												niTransform.rotate.entry[1][0] = 
												hkTransform.rotation.col0.quad.m128_f32[1];
												niTransform.rotate.entry[2][0] = 
												hkTransform.rotation.col0.quad.m128_f32[2];

												niTransform.rotate.entry[0][1] = 
												hkTransform.rotation.col1.quad.m128_f32[0];
												niTransform.rotate.entry[1][1] = 
												hkTransform.rotation.col1.quad.m128_f32[1];
												niTransform.rotate.entry[2][1] = 
												hkTransform.rotation.col1.quad.m128_f32[2];

												niTransform.rotate.entry[0][2] = 
												hkTransform.rotation.col2.quad.m128_f32[0];
												niTransform.rotate.entry[1][2] = 
												hkTransform.rotation.col2.quad.m128_f32[1];
												niTransform.rotate.entry[2][2] = 
												hkTransform.rotation.col2.quad.m128_f32[2];
												vertexA = niTransform * vertAOffset;
												vertexB = niTransform * vertBOffset;
													
												// For a bit more leeway, scale up a bit.
												float radius = 
												(
													(vertexB - vertexA).Length() + 
													1.25f *
													hkpCapsuleShape->radius *
													HAVOK_TO_GAME
												);

												auto hitPos = ToNiPoint3(result.hitPos);
												// Within the radius of the sphere to knock down.
												slapKnockdown = 
												(
													hitPos.GetDistance
													(
														headNode->world.translate
													) < radius
												);
											}
										}
									}
								}
							}
						}
							
						// Extremely rough approximation if the hit actor has no head:
						// Hit position must be within an eighth of the actor's height
						// from their looking at (eye) position.
						if (!slapKnockdown && !hasHeadNode)
						{
							auto hitPos = ToNiPoint3(result.hitPos);
							slapKnockdown = 
							(
								hitPos.GetDistance(hitActor->GetLookingAtLocation()) <
								0.125f * hitActor->GetHeight()
							);
						}
					}
				}

				// Criteria for damageable actors:
				// Not a ghost or invulnerable and either a player 
				// or not essential/protected or hostile to the player.
				bool hittable = 
				{
					(!hitActor) || 
					(!hitActor->IsGhost() && !hitActor->IsInvulnerable())
				};
				bool isReleasedActor = a_p->tm->rmm->IsManaged(handle, false);
				RE::hkVector4 hitForce{ 0.0f };
				// Ensure the actor is not already managed as a released refr 
				// or grabbed by another player before releasing it.
				if (slapKnockdown && !isReleasedActor && !grabbedByAnotherPlayer)
				{
					// Stop momentum before knocking down.
					hitHkpRigidBodyPtr->motion.SetLinearVelocity(RE::hkVector4());
					// Slap knockdown damage scales with thrown object damage.
					a_p->tm->rmm->AddGrabbedRefr(a_p, handle);
					a_p->tm->rmm->ClearGrabbedRefr(handle);
					if (a_p->tm->rmm->GetNumGrabbedRefrs() == 0)
					{
						a_p->tm->SetIsGrabbing(false);
					}

					a_p->tm->rmm->AddReleasedRefr(a_p, handle);
					a_staminaCostOut = 
					(
						(4.0f * sqrtf(hitActor->GetWeight() + 100.0f)) *
						(Settings::vfStaminaCostMult[a_p->playerID])
					);
					// Sneaky mechanic that may or may not have been intentional:
					// Knocking down an actor while facing the crosshair will throw them,
					// double the stamina cost, since throwing the actor has the potential
					// for more damage dealt.
					if (a_p->mm->reqFaceTarget)
					{
						a_staminaCostOut *= 2.0f;

						// Handle magicka cost as well.
						// Must have been successfully released.
						bool insertedAsReleasedRefr = 
						(
							a_p->tm->rmm->releasedRefrInfoList.empty() ?
							false :
							a_p->tm->rmm->releasedRefrInfoList[0]->refrHandle == handle
						);
						// Expend magicka.
						// Actor is thrown as if the grab bind were held and released
						// after half the max thrown window.
						if (insertedAsReleasedRefr)
						{
							// Update magicka overflow slowdown factor to use 
							// when determining the trajectory of thrown refrs.
							float magickaCost = 
							(
								a_p->tm->GetThrownRefrMagickaCost
								(
									hitActor,
									0.5f * Settings::fSecsToReleaseObjectsAtMaxSpeed
								)
							);
							a_p->tm->magickaOverflowSlowdownFactor = 
							(
								a_p->tm->GetThrownRefrMagickaOverflowSlowdownFactor
								(
									magickaCost
								)
							);

							if (magickaCost > 0.0f)
							{
								a_p->pam->ModifyAV(RE::ActorValue::kMagicka, -magickaCost);
							}
						}
					}
					
					//================
					// [Apply Damage]:
					//================

					// Set power attack, slap, and potentially the sneak attack flag 
					// before sending a hit event.

					float sneakMult = 
					(
						a_p->coopActor->IsSneaking() && a_p->tm->detectionPct < 100.0f ? 
						max
						(
							2.0f, 
							a_p->coopActor->GetActorValue(RE::ActorValue::kAttackDamageMult)
						) : 
						1.0f
					);
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

					if (hittable) 
					{
						bool isLeftArmNode = GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODES.contains
						(
							a_armNode->name
						);
						// 3x damage at player level 100.
						float levelDamageFactor = 
						(
							1.0f + 3.0f * max(a_p->coopActor->GetLevel() - 1.0f, 0.0f) / 99.0f
						);
						// Subject to change, but this works for now.
						float havokHitSpeedFactor = sqrtf(hitSpeed * GAME_TO_HAVOK);
						// Higher armor rating -> less damage taken.
						// 1 / 2 the damage at an armor rating of 100.
						float armorRatingFactor = sqrtf
						(
							1.0f / 
							(max(hitActor->CalcArmorRating() / 25.0f, 1.0f))
						);
						// Scale up damage with the total weight of the armor 
						// making contact with the hit actor.
						float armWeightFactor = 1.0f;
						auto forearmArmor = 
						(
							a_p->coopActor->GetWornArmor
							(
								RE::BGSBipedObjectForm::BipedObjectSlot::kForearms
							)
						);
						if (forearmArmor)
						{
							armWeightFactor += forearmArmor->weight;
						}

						auto handArmor = 
						(
							a_p->coopActor->GetWornArmor
							(
								RE::BGSBipedObjectForm::BipedObjectSlot::kHands
							)
						);
						if (handArmor)
						{
							armWeightFactor += handArmor->weight;
						}

						auto torsoArmor = 
						(
							a_p->coopActor->GetWornArmor
							(
								RE::BGSBipedObjectForm::BipedObjectSlot::kBody
							)
						);
						// Use a fraction of the torso armor's weight for the shoulder.
						if (torsoArmor)
						{
							armWeightFactor += torsoArmor->weight / 5.0f;
						}

						if (auto shield = a_p->em->GetShield(); shield && isLeftArmNode)
						{
							armWeightFactor += shield->weight;
						}

						armWeightFactor = 1.0f + armWeightFactor / 30.0f;
						float damage = 
						(
							havokHitSpeedFactor * 
							levelDamageFactor *
							armorRatingFactor * 
							armWeightFactor * 
							sneakMult
						);

						// Handle health damage.
						// Ignore damage to friendly actors if friendly fire is off.
						if ((damage != 0.0f) && 
							(
								Settings::vbFriendlyFire[a_p->playerID] || 
								!Util::IsPartyFriendlyActor(hitActor)
							))
						{
							// Damage will not be modified in either HandleHealthDamage() hook 
							// because the damage will not be attributed to the player
							// (attacker param is nullptr) 
							// since we are directly modifying the health AV here.
							// Therefore, to get the same damage modifications here, 
							// we tack on the slap knockdown damage mult, 
							// and multiply the result by the damage received mult 
							// if the target is a player.
							damage *= Settings::vfSlapKnockdownDamageMult[a_p->playerID];
							// Apply non-zero damage.
							// If hitting another player or Enderal is installed,
							// apply the damage directly without dealing damage 
							// through a second hit event,
							// which is used to attribute blame for the hit to P1 
							// and trigger alarms/bounties.
							auto playerIndex = GlobalCoopData::GetCoopPlayerIndex(hitActor); 
							if (playerIndex != -1)
							{
								const auto& hitP = glob.coopPlayers[playerIndex];
								damage *= Settings::vfDamageReceivedMult[hitP->controllerID];
								// No requested damage to deal in second hit event.
								a_p->tm->rmm->reqSpecialHitDamageAmount = 0.0f;
								// Apply damage directly to the health AV.
								// No attacker source will be reported 
								// in the HandleHealthDamage() hook,
								// so our damage here is the final damage 
								// which will be applied on hit.
								hitActor->RestoreActorValue
								(
									RE::ACTOR_VALUE_MODIFIER::kDamage,
									RE::ActorValue::kHealth,
									-damage
								);
							}
							else
							{
								// Divide by P1's damage dealt multiplier 
								// to nullify its application
								// in the HandleHealthDamage() hook, 
								// which fires on the second hit event
								// that is sent from our Hit Event Handler.
								const auto& p1DamageDealtMult = 
								(
									Settings::vfDamageDealtMult
									[glob.coopPlayers[glob.player1CID]->playerID]	
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
								a_p->tm->rmm->reqSpecialHitDamageAmount = damage;
							}
						}

						// Send the hit event after caching/apply damage.
						Util::SendHitEvent
						(
							a_p->coopActor.get(), 
							hitActor, 
							a_p->coopActor->formID,
							a_p->coopActor->formID,
							hitFlags
						);

						// REMOVE when done debugging.
						/*SPDLOG_DEBUG
						(
							"[MM] PerformArmCollision: {}: Hit actor {}. "
							"Havok hit speed factor: {}, "
							"level damage factor: {}, "
							"armor rating factor: {}, "
							"arm weight factor: {}, "
							"sneak mult: {}. "
							"Final damage: {}.", 
							a_p->coopActor->GetName(), 
							hitActor->GetName(),
							havokHitSpeedFactor,
							levelDamageFactor,
							armorRatingFactor,
							armWeightFactor,
							sneakMult,
							damage
						);*/
					}
				}
				else
				{
					// Apply a hit impulse of 0 to enable Precision's ragdoll for this actor.
					// Then we'll apply a force to the hit rigid body instead to smooth out
					// deformation of the ragdoll.
					// Only send on the first hit of the frame.
					// Sending on every hit produces a huge framerate spike 
					// and is completely unnecessary.
					// Also, expend stamina on first hit.
					if (a_noPreviousHit)
					{
						precisionAPI4->ApplyHitImpulse2
						(
							hitActor->GetHandle(), 
							a_p->coopActor->GetHandle(), 
							hitHkpRigidBodyPtr.get(), 
							RE::NiPoint3(), 
							hitPosVec, 
							0.0f
						);

						a_staminaCostOut = 
						(
							sqrtf(hitActor->GetWeight() + 100.0f) *
							Util::InterpolateEaseIn(0.0f, 1.0f, hitToKnockdownSpeedRatio, 3.0f) *
							Settings::vfStaminaCostMult[a_p->playerID]
						);
						
						// No reason to send hit events for refrs or invulnerable NPCs.
						// Only sent on first hit as well.
						if (hittable) 
						{
							// Send a hit event.
							// No damage, power attack flag, or ragdoll for no-knockdown slaps.
							Util::SendHitEvent
							(
								a_p->coopActor.get(),
								hitRefrPtr.get(),
								a_p->coopActor->formID,
								a_p->coopActor->formID,
								static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kSlap)
							);
						}
					}

					// Do not apply as much force to an already-ragdolled actor,
					// or if the flailing player is tired.
					float knockedDownMult = isReleasedActor ? 0.15f : 1.0f;
					hitForce = 
					(
						TohkVector4(hitVelocity) * 
						GAME_TO_HAVOK * 
						knockedDownMult * 
						armForceFactor
					);
					hitHkpRigidBodyPtr->motion.ApplyForce
					(
						*g_deltaTimeRealTime * 
						1000.0f * 
						Util::InterpolateEaseOut
						(
							1.0f, 0.5f, hitToKnockdownSpeedRatio, 7.0f
						), 
						hitForce * Settings::fArmCollisionForceMultiplier
					);
				}

				// REMOVE when done debugging.
				/*DebugAPI::QueuePoint3D
				(
					result.hitPos, 
					Settings::vuOverlayRGBAValues[a_p->playerID], 
					hitVelocity.Length() / 400.0f,
					1.0f
				);*/

				/*SPDLOG_DEBUG
				(
					"[GLOB] PerformArmCollision: "
					"{} hit {} (0x{:X}, {}, {}) with {} node, "
					"Hit pos point vel: {}. Hit force: {} "
					"{} Hits: {}. Stamina cost: {}.",
					a_p->coopActor->GetName(),
					hitRefrPtr->GetName(),
					hitRefrPtr->formID,
					hitRefrPtr->GetBaseObject() ? 
					*hitRefrPtr->GetBaseObject()->formType : 
					RE::FormType::None,
					result.hitObjectPtr->name,
					a_armNodeType == ArmNodeType::kForearm ? 
					"forearm" : 
					a_armNodeType == ArmNodeType::kHand ? 
					"hand" : 
					"shoulder",
					hitVelocity.Length(),
					hitForce.Length3(),
					knockdown ? "KO!" : "SLAPPED!",
					raycastResults.size(),
					a_staminaCostOut
				);*/
			}
			else
			{
				/*SPDLOG_DEBUG
				(
					"[MM] PerformArmCollision: {}: {} has mass of {}, "
					"inv mass of {}, hit velocity: {}, force applied: {} over {}s.",
					a_p->coopActor->GetName(),
					hitRefrPtr->GetName(),
					hitHkpRigidBodyPtr->motion.GetMass(),
					hitHkpRigidBodyPtr->motion.inertiaAndMassInv.quad.m128_f32[3],
					hitVelocity.Length(),
					(hitVelocity * GAME_TO_HAVOK * hitHkpRigidBodyPtr->motion.GetMass()).Length(),
					*g_deltaTimeRealTime
				);*/

				hitHkpRigidBodyPtr->motion.ApplyForce
				(
					*g_deltaTimeRealTime,
					(hitVelocity * hitHkpRigidBodyPtr->motion.GetMass() * 0.05f) *
					Settings::fArmCollisionForceMultiplier
				);
				if (a_noPreviousHit)
				{
					// Send a hit event on first-contact.
					// No damage, power attack flag, or ragdoll for slapping objects.
					Util::SendHitEvent
					(
						a_p->coopActor.get(),
						hitRefrPtr.get(),
						a_p->coopActor->formID,
						a_p->coopActor->formID,
						static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kSlap)
					);

					if (a_p->mm->reqFaceTarget)
					{
						// Slap -> telekinetic throw at target if facing the crosshair.
						// No stamina cost.
						// Dunno how useful this is, but it is cool.
						bool isReleasedAlready = a_p->tm->rmm->IsManaged(refHandle, false);
						// Can only manipulate this object if it has not been released,
						// is not grabbed by another player,
						// and has a supported motion type.
						bool canGrabAndThrow = 
						{ 
							!isReleasedAlready &&
							!grabbedByAnotherPlayer &&
							hitHkpRigidBodyPtr->motion.type != RE::hkpMotion::MotionType::kFixed &&
							hitHkpRigidBodyPtr->motion.type != RE::hkpMotion::MotionType::kInvalid 
						};
						if (canGrabAndThrow)
						{
							a_p->tm->rmm->AddGrabbedRefr(a_p, refHandle);
							a_p->tm->rmm->ClearGrabbedRefr(refHandle);
							if (a_p->tm->rmm->GetNumGrabbedRefrs() == 0)
							{
								a_p->tm->SetIsGrabbing(false);
							}

							a_p->tm->rmm->AddReleasedRefr(a_p, refHandle);
						}

						// Handle magicka cost as well.
						// Must have been successfully released.
						bool insertedAsReleasedRefr = 
						(
							a_p->tm->rmm->releasedRefrInfoList.empty() ?
							false :
							a_p->tm->rmm->releasedRefrInfoList[0]->refrHandle == refHandle
						);
						// Expend magicka.
						// Actor is thrown as if the grab bind were held and released
						// after half the max thrown window.
						if (insertedAsReleasedRefr)
						{
							// Update magicka overflow slowdown factor to use 
							// when determining the trajectory of thrown refrs.
							float magickaCost = 
							(
								a_p->tm->GetThrownRefrMagickaCost
								(
									hitRefrPtr.get(),
									0.5f * Settings::fSecsToReleaseObjectsAtMaxSpeed
								)
							);
							a_p->tm->magickaOverflowSlowdownFactor = 
							(
								a_p->tm->GetThrownRefrMagickaOverflowSlowdownFactor
								(
									magickaCost
								)
							);

							if (magickaCost > 0.0f)
							{
								a_p->pam->ModifyAV(RE::ActorValue::kMagicka, -magickaCost);
							}
						}
					}
				}
			}
				
			// Do not need to play hit sounds for subsequent hits.
			if (!a_noPreviousHit)
			{
				return true;
			}
			
			// Wish I could play an old timey punch sound effect here.
			// Only play the hit sound when first applying an impulse.
			// Otherwise the sheer number of arm collision-triggeed SFX
			// will sound like hail falling on a tin roof.
			auto audioManager = RE::BSAudioManager::GetSingleton(); 
			if (!audioManager)
			{
				return true;
			}

			RE::BSSoundHandle handle{ };
			RE::BGSSoundDescriptorForm* slapSFX = nullptr;
			if (slapKnockdown)
			{
				slapSFX =
				(
					RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xC190D)
				);
			}
			else if (hitActor)
			{
				slapSFX =
				(
					RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xC1AD1)
				);
			}
			else
			{
				slapSFX =
				(
					RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xAF63E)
				);
			}

			if (!slapSFX)
			{	
				return true;
			}

			bool succ = audioManager->BuildSoundDataFromDescriptor(handle, slapSFX);
			if (succ)
			{
				handle.SetPosition(ToNiPoint3(result.hitPos));
				handle.SetObjectToFollow(result.hitObjectPtr.get());
				handle.SetVolume(hitVolume);
				handle.Play();
			}

			if (!hitActor)
			{
				// Send destructible object destruction event.
				if (auto taskInterface = RE::TaskQueueInterface::GetSingleton(); taskInterface)
				{
					taskInterface->QueueUpdateDestructibleObject
					(
						hitRefrPtr.get(), 
						0.06f * a_p->coopActor->GetWeight() * hitSpeed * GAME_TO_HAVOK,
						false,
						a_p->coopActor.get()
					);
				}
			}

			return true;
		}

		return false;
	}

	void NodeOrientationManager::RestoreOriginalNodeLocalTransforms
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Restore saved node local transforms previously set by the game before our modifications.
		
		// Return early if the player's loaded 3D data is invalid.
		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		// Return early if the player's 3D is invalid.
		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		for (const auto& [nodePtr, localTrans] : defaultNodeLocalTransformsMap)
		{
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			nodePtr->local = localTrans;
		}
	}

	void NodeOrientationManager::SavePlayerNodeWorldTransforms
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// NOTE: 
		// When calling this function, the game SHOULD have updated all nodes' local transforms
		// (unless a havok impulse was applied),
		// but has not modified the world transforms from the previous frame 
		// (which we may have modified).
		// Since there is a disconnect, recursively traverse the player's nodes,
		// compute the new world transforms using the updated local transforms,
		// and save the adjustable nodes' world transforms as the default transforms to use later.

		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		// Return early if the player's loaded 3D data is invalid.
		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		// Return early if the player's 3D is invalid.
		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		auto npc3DPtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npc)); 
		if (!npc3DPtr || !npc3DPtr.get())
		{
			return;
		}

		auto nodePtr = RE::NiPointer<RE::NiNode>(npc3DPtr->AsNode());
		if (!nodePtr || !nodePtr.get())
		{
			return;
		}

		auto parentWorldTransform = nodePtr->parent ? nodePtr->parent->world : RE::NiTransform();
		SavePlayerNodeWorldTransforms(a_p, nodePtr, parentWorldTransform);
	}

	void NodeOrientationManager::SavePlayerNodeWorldTransforms
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		const RE::NiPointer<RE::NiNode>& a_nodePtr, 
		const RE::NiTransform& a_parentWorldTransform
	)
	{
		// Recursively save adjustable nodes' world rotations 
		// by walking the player's node tree from the given node.
		// The parent world transform is modified to the current node's world transform
		// before traversing its children.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

		// Cannot set the saved world rotation directly
		// to this node's reported world rotation, 
		// since it still contains our possibly-modified world rotation from the previous frame.
		// However, if called at the right time (during the havok physics pre-step),
		// the node's local rotation will be updated for the next frame, 
		// and we can then calculate the game's intended world rotation for this node 
		// via the parent node's world transform.

		auto newParentTransform = (a_parentWorldTransform * a_nodePtr->local);
		// Save default world transform.
		defaultNodeWorldTransformsMap.insert_or_assign(a_nodePtr, newParentTransform);

		for (const auto child : a_nodePtr->children)
		{
			if (!child || !child.get() || !child->AsNode())
			{
				continue;
			}

			auto childNode = RE::NiPointer<RE::NiNode>(child->AsNode());
			SavePlayerNodeWorldTransforms(a_p, childNode, newParentTransform);
		}
	}

	void NodeOrientationManager::SetBlendStatus
	(
		const RE::NiPointer<RE::NiAVObject>& a_nodePtr, NodeRotationBlendStatus&& a_newStatus
	)
	{
		// Update the blend status for the given node.

		if (!a_nodePtr || !a_nodePtr.get()) 
		{
			return;
		}

		if (!nodeNameToRotationDataMap.contains(a_nodePtr->name)) 
		{
			return;
		}

		auto& data = nodeNameToRotationDataMap.at(a_nodePtr->name); 
		if (!data || !data.get())
		{
			return;
		}

		// Reset frame counts to 0 before blending in/out.
		if (a_newStatus == NodeRotationBlendStatus::kBlendIn)
		{
			data->blendInFrameCount = 0;
		}
		else if (a_newStatus == NodeRotationBlendStatus::kBlendOut)
		{
			data->blendOutFrameCount = 0;
		}

		data->blendStatus = a_newStatus;
	}

	void NodeOrientationManager::UpdateArmNodeRotationData
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::NiPointer<RE::NiAVObject>& a_forearmNodePtr,
		const RE::NiPointer<RE::NiAVObject>& a_handNodePtr,
		bool a_rightArm
	)
	{
		// Update arm nodes' blend states and target rotations.
		// Modification method: local rotation directly derived from pitch/roll/yaw
		// adjustments based on the player's RS movement.

		// Invalid nodes or not accounted for in the rotation data map.
		if (!a_forearmNodePtr || !a_forearmNodePtr.get() || !a_handNodePtr || !a_handNodePtr.get())
		{
			return;
		}

		// Ensure both nodes are accounted for in the rotation data map.
		if (!nodeNameToRotationDataMap.contains(a_forearmNodePtr->name))
		{
			// Set all rotations to the node's current local rotation
			// so we have valid data to start off with.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					a_forearmNodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation = 
			newData->defaultRotation = 
			newData->startingRotation = 
			newData->targetRotation = a_forearmNodePtr->local.rotate;
		}

		if (!nodeNameToRotationDataMap.contains(a_handNodePtr->name))
		{
			// Set all rotations to the node's current local rotation 
			// so we have valid data to start off with.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					a_handNodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation = 
			newData->defaultRotation = 
			newData->startingRotation = 
			newData->targetRotation = a_handNodePtr->local.rotate;
		}

		const auto& forearmData = nodeNameToRotationDataMap[a_forearmNodePtr->name];
		const auto& handData = nodeNameToRotationDataMap[a_handNodePtr->name];
		// Set node target rotations for forearm and hand.
		bool forearmRotHasBeenSet = forearmData->rotationModified;
		bool handRotHasBeenSet = handData->rotationModified;
		const float oldForearmYaw = forearmRotHasBeenSet ? forearmData->rotationInput[0] : 0.0f;
		const float oldForearmPitch = forearmRotHasBeenSet ? forearmData->rotationInput[1] : 0.0f;
		const float oldForearmRoll = forearmRotHasBeenSet ? forearmData->rotationInput[2] : 0.0f;
		const float oldHandYaw = 
		(
			handRotHasBeenSet ? 
			handData->rotationInput[0] : 
			a_rightArm ?
			-(0.5f * PI) : 
			(0.5f * PI)
		);
		const float oldHandPitch = handRotHasBeenSet ? handData->rotationInput[1] : 0.0f;
		const float oldHandRoll = handRotHasBeenSet ? handData->rotationInput[2] : 0.0f;
		float targetForearmPitch = oldForearmPitch;
		float targetForearmRoll = oldForearmRoll;
		float targetForearmYaw = oldForearmYaw;
		float targetHandPitch = oldHandPitch;
		float targetHandRoll = oldHandRoll;
		float targetHandYaw = oldHandYaw;

		const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
		float xDisp = std::clamp(rsData.xComp * rsData.normMag, -1.0f, 1.0f);
		float yDisp = std::clamp(rsData.yComp * rsData.normMag, -1.0f, 1.0f);

		// Check which node rotation actions are being performed.
		bool rotatingForearm = 
		{
			a_rightArm ?
			a_p->pam->IsPerforming(InputAction::kRotateRightForearm) :
			a_p->pam->IsPerforming(InputAction::kRotateLeftForearm)
		};
		bool rotatingHand = 
		{
			a_rightArm ?
			a_p->pam->IsPerforming(InputAction::kRotateRightHand) :
			a_p->pam->IsPerforming(InputAction::kRotateLeftHand)
		};
		bool rotatingShoulder = 
		{
			a_rightArm ?
			a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
			a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
		};

		// Set rotation state flags first.
		forearmData->prevInterrupted = forearmData->interrupted;
		forearmData->prevRotationModified = forearmData->rotationModified;
		handData->prevInterrupted = handData->interrupted;
		handData->prevRotationModified = handData->rotationModified;
		// Interrupted (should blend out) if ragdolled
		forearmData->interrupted = handData->interrupted = 
		{
			a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			a_p->IsAwaitingRefresh() ||
			a_p->coopActor->IsWeaponDrawn() ||
			a_p->coopActor->IsSwimming()
		};

		if (rotatingHand)
		{
			// When clicking in the RS, change forearm yaw and hand yaw together along the +X axis,
			// and hand roll along the -X axis.
			// Change hand pitch along the Y axis.
			// Flip angle sign for the other hand.
			if (xDisp > 0.0f)
			{
				targetForearmYaw = xDisp * (0.25f * PI) - (PI / 60.0f);
				targetHandYaw = (xDisp - 1.0f) * (0.5f * PI);
				if (!a_rightArm)
				{
					targetForearmYaw = -targetForearmYaw;
					targetHandYaw = -targetHandYaw;
				}
				
				// Set hand AND forearm target rotation as modified.
				handData->rotationModified = forearmData->rotationModified = true;
			}
			else
			{
				// Hand roll.
				if (xDisp >= -0.333333f)
				{
					targetHandRoll = (-xDisp * 4.0f) * (0.25f * PI);
				}
				else if (xDisp >= -0.666666f)
				{
					targetHandRoll = (2.666664f + xDisp * 4.0f) * (0.25f * PI);
				}
				else
				{
					targetHandRoll = ((0.666666f + xDisp) * 3.0f) * (0.25f * PI);
				}

				if (!a_rightArm)
				{
					targetHandRoll = -targetHandRoll;
				}

				// Only hand target rotation modified.
				handData->rotationModified = true;
			}

			// Hand pitch.
			targetHandPitch = yDisp * (0.5f * PI);
		}
		else if (rotatingForearm)
		{
			// When NOT clicking in the RS, change forearm pitch along the +Y axis and
			// forearm roll along the -Y axis.
			// Change forearm yaw along the X axis.
			// Flip angle sign for the other side.
			if (yDisp > 0.0f)
			{
				// Forearm pitch.
				targetForearmPitch = yDisp * (0.5f * PI);
			}
			else
			{
				// Forearm roll.
				targetForearmRoll = -yDisp * (0.25f * PI) + (PI / 60.0f);
				if (!a_rightArm)
				{
					targetForearmRoll = -targetForearmRoll;
				}
			}

			if (xDisp > 0.0f)
			{
				// Forearm yaw.
				targetForearmYaw = xDisp * (0.25f * PI) - (PI / 60.0f);
			}
			else
			{
				// Forearm yaw.
				targetForearmYaw = xDisp * (0.25f * PI) + (PI / 60.0f);
			}

			if (!a_rightArm)
			{
				targetForearmYaw = -targetForearmYaw;
			}
			
			// Only forearm target rotation modified.
			forearmData->rotationModified = true;
		}
		else if (rotatingShoulder)
		{
			handData->rotationModified = forearmData->rotationModified = true;
			// Set default rotations if the forearm/hand nodes' rotations
			// were not previously modified.
			if (!forearmData->prevRotationModified) 
			{
				targetForearmPitch =
				targetForearmRoll =
				targetForearmYaw = 0.0f;
			}

			if (!handData->prevRotationModified) 
			{
				targetHandPitch =
				targetHandRoll = 0.0f;
				targetHandYaw = a_rightArm ? -PI / 2.0f : PI / 2.0f;
			}
		}

		// Update blend states after potentially setting the rotation modified flag 
		// and before setting current and target rotations.
		UpdateNodeRotationBlendState(a_p, forearmData, a_forearmNodePtr, true);
		UpdateNodeRotationBlendState(a_p, handData, a_handNodePtr, true);

		// If modified, set hand and forearm node rotation inputs and target rotations.
		const RE::NiPoint3 locXAxis{ 1.0f, 0.0f, 0.0f };
		const RE::NiPoint3 locYAxis{ 0.0f, 1.0f, 0.0f };
		const RE::NiPoint3 locZAxis{ 0.0f, 0.0f, 1.0f };
		if (forearmData->rotationModified) 
		{
			if (oldForearmPitch != targetForearmPitch)
			{
				targetForearmPitch = Util::InterpolateSmootherStep
				(
					oldForearmPitch, targetForearmPitch, min(1.0f, interpFactor)
				);
			}

			if (oldForearmRoll != targetForearmRoll)
			{
				targetForearmRoll = Util::InterpolateSmootherStep
				(
					oldForearmRoll, targetForearmRoll, min(1.0f, interpFactor)
				);
			}

			if (oldForearmYaw != targetForearmYaw)
			{
				targetForearmYaw = Util::InterpolateSmootherStep
				(
					oldForearmYaw, targetForearmYaw, min(1.0f, interpFactor)
				);
			}

			forearmData->rotationInput = std::array<float, 3>
			(
				{ targetForearmYaw, targetForearmPitch, targetForearmRoll }
			);
			Util::SetRotationMatrixPYRAndAxes
			(
				forearmData->targetRotation, 
				locXAxis, 
				locYAxis, 
				locZAxis, 
				forearmData->rotationInput[1], 
				forearmData->rotationInput[0], 
				forearmData->rotationInput[2]
			);
		}
		else
		{
			// Set to default when not modified.
			forearmData->targetRotation = forearmData->defaultRotation;
		}
		
		if (handData->rotationModified) 
		{

			// Gives a slightly more weighted feel to arm movement.
			float stickSpeedFactor = 
			(
				Util::InterpolateEaseOut
				(
					0.85f,
					1.0f, 
					std::clamp
					(
						0.6f, 
						1.0f, 
						rsData.stickAngularSpeed / 20.0f
					),
					2.0f
				)
			);

			// Slow down if low on stamina -> less likely to knock down NPCs with a slap.
			const float staminaSlowdownFactor = a_p->mm->GetArmRotationFactor(true);
			if (oldHandPitch != targetHandPitch)
			{
				targetHandPitch = Util::InterpolateSmootherStep
				(
					oldHandPitch, 
					targetHandPitch, 
					min(1.0f, interpFactor * stickSpeedFactor * staminaSlowdownFactor)
				);
			}

			if (oldHandRoll != targetHandRoll)
			{
				targetHandRoll = Util::InterpolateSmootherStep
				(
					oldHandRoll, 
					targetHandRoll, 
					min(1.0f, interpFactor * stickSpeedFactor * staminaSlowdownFactor)
				);
			}

			if (oldHandYaw != targetHandYaw)
			{
				targetHandYaw = Util::InterpolateSmootherStep
				(
					oldHandYaw, 
					targetHandYaw, 
					min(1.0f, interpFactor * stickSpeedFactor * staminaSlowdownFactor)
				);
			}

			handData->rotationInput = std::array<float, 3>
			(
				{ targetHandYaw, targetHandPitch, targetHandRoll }
			);
			Util::SetRotationMatrixPYRAndAxes
			(
				handData->targetRotation, 
				locXAxis,
				locYAxis, 
				locZAxis,
				handData->rotationInput[1], 
				handData->rotationInput[0], 
				handData->rotationInput[2] 
			);
		}
		else
		{
			// Set to default when not modified.
			handData->targetRotation = handData->defaultRotation;
		}

		// TODO: Figure out a way to produce a collision
		// that doesn't trigger combat. 
		// Also need to set hit data flags,
		// such as power attack/sneak hit flags 
		// for subsequent collider hits. 
		// As of right now, all hits do damage,
		// start combat with the hit target,
		// and do not have applied effects from special hit type flags.
		

		// Add Precision colliders to forearm/hand nodes
		// if the player just started modifying node rotations.
		/*
		bool shouldStart = 
		(
			(!forearmData->precisionColliderAdded) &&
			(
				(
					a_rightArm ? 
					a_p->pam->JustStarted(InputAction::kRotateRightForearm) ||
					a_p->pam->JustStarted(InputAction::kRotateRightShoulder) :
					a_p->pam->JustStarted(InputAction::kRotateLeftForearm) ||
					a_p->pam->JustStarted(InputAction::kRotateLeftShoulder)
				) ||
				(
					(rsData.stickLinearSpeed > 1E-2f) &&
					(
						a_rightArm ? 
						a_p->pam->IsPerforming(InputAction::kRotateRightForearm) ||
						a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
						a_p->pam->IsPerforming(InputAction::kRotateLeftForearm) ||
						a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
					)
				)
			)
		);
		bool shouldStop = 
		(
			(forearmData->precisionColliderAdded) &&
			(
				(rsData.stickLinearSpeed <= 1E-2f) ||
				(
					a_rightArm ? 
					a_p->pam->IsNotPerforming(InputAction::kRotateRightForearm) &&
					a_p->pam->IsNotPerforming(InputAction::kRotateRightShoulder) :
					a_p->pam->IsNotPerforming(InputAction::kRotateLeftForearm) &&
					a_p->pam->IsNotPerforming(InputAction::kRotateLeftShoulder)
				)
			)
		);
		if (shouldStart)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_forearmNodePtr.get(), 
				PrecisionAnnotationReqType::kStart
			);
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_forearmNodePtr.get(), 
				PrecisionAnnotationReqType::kAdd,
				0.05f,
				1.0f
			);
			
			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: ADD collision for {}.",
				a_p->coopActor->GetName(),
				a_forearmNodePtr->name);
			forearmData->precisionColliderAdded = true;
		}
		else if (shouldStop)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_forearmNodePtr.get(), 
				PrecisionAnnotationReqType::kRemove
			);
			
			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: REMOVE collision for {}.",
				a_p->coopActor->GetName(),
				a_forearmNodePtr->name);
			forearmData->precisionColliderAdded = false;
		}
		
		// Clear previous hits to allow for multiple hits while rotating the node.
		if (rotatingShoulder || rotatingForearm)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_forearmNodePtr.get(), 
				PrecisionAnnotationReqType::kMultiHit
			);
		}

		shouldStart = 
		(
			(!handData->precisionColliderAdded) &&
			(
				(
					a_rightArm ? 
					a_p->pam->JustStarted(InputAction::kRotateRightHand) ||
					a_p->pam->JustStarted(InputAction::kRotateRightShoulder) :
					a_p->pam->JustStarted(InputAction::kRotateLeftHand) ||
					a_p->pam->JustStarted(InputAction::kRotateLeftShoulder)
				) ||
				(
					(rsData.stickLinearSpeed > 1E-2f) &&
					(
						a_rightArm ? 
						a_p->pam->IsPerforming(InputAction::kRotateRightHand) ||
						a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
						a_p->pam->IsPerforming(InputAction::kRotateLeftHand) ||
						a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
					)
				)
			)
		);
		shouldStop = 
		(
			(handData->precisionColliderAdded) &&
			(
				(rsData.stickLinearSpeed <= 1E-2f) ||
				(
					a_rightArm ? 
					a_p->pam->IsNotPerforming(InputAction::kRotateRightHand) &&
					a_p->pam->IsNotPerforming(InputAction::kRotateRightShoulder) :
					a_p->pam->IsNotPerforming(InputAction::kRotateLeftHand) &&
					a_p->pam->IsNotPerforming(InputAction::kRotateLeftShoulder)
				)
			)
		);
		if (shouldStart)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_handNodePtr.get(), 
				PrecisionAnnotationReqType::kStart
			);
			// Extend the collider a bit to fully encapsulate the fingers.
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_handNodePtr.get(), 
				PrecisionAnnotationReqType::kAdd,
				0.01f,
				1.75f
			);

			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: ADD collision for {}.",
				a_p->coopActor->GetName(),
				a_handNodePtr->name);
			handData->precisionColliderAdded = true;
		}
		else if (shouldStop)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_handNodePtr.get(), 
				PrecisionAnnotationReqType::kRemove
			);
			
			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: REMOVE collision for {}.",
				a_p->coopActor->GetName(),
				a_handNodePtr->name);
			handData->precisionColliderAdded = false;
		}
		
		// Clear previous hits to allow for multiple hits while rotating the node.
		if (rotatingShoulder || rotatingHand)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_handNodePtr.get(), 
				PrecisionAnnotationReqType::kMultiHit
			);
		}
		*/

		// Update forearm and hand node rotation to set (current) 
		// after potentially setting target rotations.
		UpdateNodeRotationToSet(a_p, forearmData, a_forearmNodePtr, true);
		UpdateNodeRotationToSet(a_p, handData, a_handNodePtr, true);

	}
	void NodeOrientationManager::UpdateNodeRotationBlendState
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const std::unique_ptr<NodeRotationData>& a_data,
		const RE::NiPointer<RE::NiAVObject>& a_nodePtr,
		bool a_isArmNode
	)
	{
		// Update the blend status and endpoints to blend to/from for the given node.

		if (!a_data || !a_data.get() || !a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		auto ui = RE::UI::GetSingleton();
		bool blendIn = false;
		bool blendOut = false;
		// If interpolating, interpolate towards the target rotation.
		bool towardsTargetRotation = a_data->rotationModified && !a_data->interrupted;
		// Blend in flag for individual nodes.
		// Continue blending in if the target rotation was modified already
		// and if previously interrupted, 
		// or auto-pitched to face a target or the node is an arm node 
		// and the target rotation wasn't reached,
		// or still blending in.
		blendIn = 
		{
			(towardsTargetRotation) &&
			(
				(a_data->prevInterrupted) ||
				(
					(!a_p->mm->aimPitchManuallyAdjusted || a_isArmNode) && 
					(a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
				) ||
				(a_data->blendStatus == NodeRotationBlendStatus::kBlendIn)
			)
		};
		// Blend out if rotation was not modified or interrupted,
		// and if the default rotation has not been reached yet.
		blendOut = 
		{
			(!towardsTargetRotation) &&
			(a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
		};
		
		// More blend frames at a higher framerate.
		uint32_t maxBlendFrames = max
		(
			Settings::uBlendPlayerNodeRotationsFrameCount,
			static_cast<uint32_t>
			(
				(1.0f / (60.0f * *g_deltaTimeRealTime)) * 
				static_cast<float>(Settings::uBlendPlayerNodeRotationsFrameCount)
			)
		);
		if (blendIn)
		{
			if (a_data->blendStatus != NodeRotationBlendStatus::kBlendIn && 
				a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
			{
				// Just started blending in.
				NodeRotationBlendStatus prevStatus = a_data->blendStatus;
				// Start blending in when the player first tries to rotate their arms.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kBlendIn);
				// Set starting rotation before blending in.
				if (prevStatus == NodeRotationBlendStatus::kDefaultReached)
				{
					// Starting from game's set rotation.
					a_data->startingRotation = a_data->defaultRotation;
				}
				else
				{
					// Starting from the current blended rotation.
					a_data->startingRotation = a_data->currentRotation;
				}
			}

			// Blend interval elapsed, so we'll now set the requested rotations to the target ones.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn &&
				a_data->blendInFrameCount >= maxBlendFrames)
			{
				// Target rotation reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kTargetReached);
			}
		}
		else if (blendOut)
		{
			if (a_data->blendStatus != NodeRotationBlendStatus::kBlendOut && 
				a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
			{
				NodeRotationBlendStatus prevStatus = a_data->blendStatus;
				// Start blending out if not already blending out 
				// and if the default rotation was not reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kBlendOut);
				// Set starting rotation before blending out.
				if (prevStatus == NodeRotationBlendStatus::kTargetReached && 
					a_data->rotationModified)
				{
					// Starting from our set target rotation.
					a_data->startingRotation = a_data->targetRotation;
				}
				else
				{
					// Starting from the current blended rotation.
					a_data->startingRotation = a_data->currentRotation;
				}
			}

			// Fully blended out.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn &&
				a_data->blendInFrameCount >= maxBlendFrames)
			{
				// Default rotation reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kDefaultReached);
			}
		}
		else
		{
			// Not blending in or out, so set status to target or default reached,
			// depending on the interpolation direction.
			if (towardsTargetRotation && 
				a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
			{
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kTargetReached);
			}
			else if (!towardsTargetRotation && 
					 a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
			{
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kDefaultReached);
			}
		}
	}
	void NodeOrientationManager::UpdateNodeRotationToSet
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const std::unique_ptr<NodeRotationData>& a_data, 
		const RE::NiPointer<RE::NiAVObject>& a_nodePtr, 
		bool a_isArmNode
	)
	{
		// Update the local node rotation to eventually set
		// by interpolating between the starting and target/default rotation endpoints.

		if (!a_data || !a_data.get() || !a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn || 
			a_data->blendStatus == NodeRotationBlendStatus::kTargetReached)
		{
			// Interpolate between starting and target local rotations when blending in.
			// Set directly to the target local rotation otherwise.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn)
			{
				// More blend frames at a higher framerate.
				uint32_t maxBlendFrames = max
				(
					Settings::uBlendPlayerNodeRotationsFrameCount,
					static_cast<uint32_t>
					(
						(1.0f / (60.0f * *g_deltaTimeRealTime)) * 
						static_cast<float>(Settings::uBlendPlayerNodeRotationsFrameCount)
					)
				);
				// Blend in to reach the target rotation from the starting rotation.
				float t = std::clamp
				(
					static_cast<float>(a_data->blendInFrameCount) / 
					static_cast<float>(maxBlendFrames - 1),
					0.0f,
					1.0f
				);

				a_data->currentRotation = Util::InterpolateRotMatrix
				(
					a_data->startingRotation, a_data->targetRotation, t
				);
				a_data->blendInFrameCount++;
			}
			else
			{
				if (a_isArmNode)
				{
					// Set directly to the target rotation.
					a_data->currentRotation = a_data->targetRotation;
				}
				else
				{
					// Want to interpolate towards the target rotation,
					// which may change if the player is targeting 
					// a moving object or changes targets.
					// Don't want to snap to the new rotation.
					a_data->currentRotation = Util::InterpolateRotMatrix
					(
						a_data->currentRotation,
						a_data->targetRotation, 
						interpFactor
					);
				}
			}
		}
		else
		{
			// Interpolate between starting and game-given default local rotations 
			// when blending out. Set directly to the default local rotation otherwise.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendOut)
			{
				// More blend frames at a higher framerate.
				uint32_t maxBlendFrames = max
				(
					Settings::uBlendPlayerNodeRotationsFrameCount,
					static_cast<uint32_t>
					(
						(1.0f / (60.0f * *g_deltaTimeRealTime)) * 
						static_cast<float>(Settings::uBlendPlayerNodeRotationsFrameCount)
					)
				);
				// Blend out to reach the game's assigned rotation from the starting rotation.
				float t = std::clamp
				(
					static_cast<float>(a_data->blendOutFrameCount) / 
					static_cast<float>(maxBlendFrames - 1),
					0.0f,
					1.0f
				);

				a_data->currentRotation = Util::InterpolateRotMatrix
				(
					a_data->startingRotation, a_data->defaultRotation, t
				);
				a_data->blendOutFrameCount++;
			}
			else
			{
				// Reset to default rotation.
				a_data->currentRotation = a_data->defaultRotation;
			}
		}
	}
	void NodeOrientationManager::UpdateShoulderNodeRotationData
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::NiPointer<RE::NiAVObject>& a_shoulderNodePtr, 
		bool a_rightShoulder
	)
	{
		// Update shoulder node blend statuses and starting, current, and target rotations.
		// Modification method: local rotation derived from a world rotation
		// computed from hardcoded, manually verified interpolation endpoints for pitch/roll/yaw.
		// The player's RS movement determines the orientation of their arms.

		if (!a_shoulderNodePtr || !a_shoulderNodePtr.get())
		{
			return;
		}

		// Ensure the shoulder node is accounted for in rotation data map.
		if (!nodeNameToRotationDataMap.contains(a_shoulderNodePtr->name))
		{
			// Set all rotations to the node's current local rotation 
			// so we have valid data to start off with.
			const auto& newData = 
			(
				*nodeNameToRotationDataMap.insert_or_assign
				(
					a_shoulderNodePtr->name, std::make_unique<NodeRotationData>()
				).first
			).second;
			newData->currentRotation = 
			newData->defaultRotation = 
			newData->startingRotation = 
			newData->targetRotation = a_shoulderNodePtr->local.rotate;
		}

		// Rotation data we will modify.
		const auto& shoulderData = nodeNameToRotationDataMap[a_shoulderNodePtr->name];
		shoulderData->prevInterrupted = shoulderData->interrupted;
		shoulderData->prevRotationModified = shoulderData->rotationModified;
		shoulderData->interrupted =
		{
			a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			a_p->IsAwaitingRefresh() ||
			a_p->coopActor->IsWeaponDrawn() ||
			a_p->coopActor->IsSwimming()
		};

		bool rotatingShoulder = 
		{
			a_rightShoulder ?
			a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
			a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
		};

		// New rotation inputs to construct target rotation matrix with (if adjusted below).
		std::array<float, 3> newRotationInput = shoulderData->rotationInput;
		const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
		if (rotatingShoulder) 
		{
			// Set shoulder target rotation as modified.
			shoulderData->rotationModified = true;
			// Pick RS quadrant/axis based on its displacement.
			auto rsLoc = AnalogStickLocation::kCenter;
			float xDisp = std::clamp(rsData.xComp * rsData.normMag, -1.0f, 1.0f);
			float yDisp = std::clamp(rsData.yComp * rsData.normMag, -1.0f, 1.0f);
			if (rsData.normMag > 0.0f)
			{
				if (xDisp > 0.0f && yDisp > 0.0f)
				{
					rsLoc = AnalogStickLocation::kTopRight;
				}
				else if (xDisp > 0.0f && yDisp < 0.0f)
				{
					rsLoc = AnalogStickLocation::kBottomRight;
				}
				else if (xDisp < 0.0f && yDisp < 0.0f)
				{
					rsLoc = AnalogStickLocation::kBottomLeft;
				}
				else if (xDisp < 0.0f && yDisp > 0.0f)
				{
					rsLoc = AnalogStickLocation::kTopLeft;
				}
				else if (xDisp > 0.0f && yDisp == 0.0f)
				{
					rsLoc = AnalogStickLocation::kPosXAxis;
				}
				else if (xDisp == 0.0f && yDisp < 0.0f)
				{
					rsLoc = AnalogStickLocation::kNegYAxis;
				}
				else if (xDisp < 0.0f && yDisp == 0.0f)
				{
					rsLoc = AnalogStickLocation::kNegXAxis;
				}
				else if (xDisp == 0.0f && yDisp > 0.0f)
				{
					rsLoc = AnalogStickLocation::kPosYAxis;
				}
			}

			// Yaw (determined by RS X displacement and heading angle).
			// Pitch (determined by RS Y displacement).
			// Roll (constant).
			if (rsLoc == AnalogStickLocation::kCenter)
			{
				// Forward when centered.
				if (a_rightShoulder)
				{
					newRotationInput = rightShoulderMatAngleInputs.at(ArmOrientation::kForward);
				}
				else
				{
					newRotationInput = leftShoulderMatAngleInputs.at(ArmOrientation::kForward);
				}
			}
			else
			{
				// "Outer points" represent arm orientation points (up, down, left, right)
				// to use when the RS is displaced from center,
				// outer points ratio = 1, forward ratio = 0 at max displacement.
				// When centered, the forward ratio is 1 and the outer points ratio is 0,
				// meaning the forward arm orientation point is used.
				// The yaw, pitch, and roll angles for the RS orientation are blended
				// between the hardcoded forward and outer angle endpoints.
				const auto& forwardMatAngleInputs = 
				(
					a_rightShoulder ? 
					rightShoulderMatAngleInputs.at(ArmOrientation::kForward) : 
					leftShoulderMatAngleInputs.at(ArmOrientation::kForward)
				);
				float forwardRatio = 1.0f - std::clamp(rsData.normMag, 0.0f, 1.0f);
				float outerPointsRatio = 1.0f - forwardRatio;
				float forwardAngleX = forwardMatAngleInputs[0] * forwardRatio;
				float forwardAngleY = forwardMatAngleInputs[1] * forwardRatio;
				float forwardAngleZ = forwardMatAngleInputs[2] * forwardRatio;
				if (rsLoc == AnalogStickLocation::kPosYAxis)
				{
					// Right and left shoulder blend:
					// Forward and Upward.
					const auto& upwardMatAngleInputs = 
					(
						a_rightShoulder ? 
						rightShoulderMatAngleInputs.at(ArmOrientation::kUpward) : 
						leftShoulderMatAngleInputs.at(ArmOrientation::kUpward)
					);

					float outerPointsAngleX = upwardMatAngleInputs[0] * outerPointsRatio;
					float outerPointsAngleY = upwardMatAngleInputs[1] * outerPointsRatio;
					float outerPointsAngleZ = upwardMatAngleInputs[2] * outerPointsRatio;

					newRotationInput = 
					{
						forwardAngleX + outerPointsAngleX,
						forwardAngleY + outerPointsAngleY,
						forwardAngleZ + outerPointsAngleZ
					};
				}
				else if (rsLoc == AnalogStickLocation::kTopRight)
				{
					// Right shoulder blend:
					// Forward, Outward, and Upward.
					if (a_rightShoulder)
					{
						const auto& outwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);
						const auto& upwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kUpward)
						);

						float outwardWeight = max
						(
							RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)),
							0.0f
						);
						float upwardWeight = max
						(
							RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)),
							0.0f
						);
						float totalOuterPointsWeight = outwardWeight + upwardWeight;

						float outwardAngleX = 
						(
							outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleY = 
						(
							outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleZ = 
						(
							outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleX = 
						(
							upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleY = 
						(
							upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleZ = 
						(
							upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(outwardAngleX + upwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(outwardAngleY + upwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(outwardAngleZ + upwardAngleZ) * outerPointsRatio
						);

						newRotationInput =
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward, Inward, and Upward.
					else
					{
						const auto& inwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);
						const auto& upwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kUpward)
						);

						float inwardWeight = max
						(
							RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float upwardWeight = max
						(
							RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = inwardWeight + upwardWeight;

						float inwardAngleX = 
						(
							inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleY = 
						(
							inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleZ = 
						(
							inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleX = 
						(
							upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleY = 
						(
							upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleZ = 
						(
							upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = (inwardAngleX + upwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (inwardAngleY + upwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (inwardAngleZ + upwardAngleZ) * outerPointsRatio;

						newRotationInput =
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
				else if (rsLoc == AnalogStickLocation::kPosXAxis)
				{
					// Right shoulder blend:
					// Forward and Outward
					if (a_rightShoulder)
					{
						const auto& outwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);

						float outerPointsAngleX = outwardMatAngleInputs[0] * outerPointsRatio;
						float outerPointsAngleY = outwardMatAngleInputs[1] * outerPointsRatio;
						float outerPointsAngleZ = outwardMatAngleInputs[2] * outerPointsRatio;

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward and Inward.
					else
					{
						const auto& inwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);

						float outerPointsAngleX = inwardMatAngleInputs[0] * outerPointsRatio;
						float outerPointsAngleY = inwardMatAngleInputs[1] * outerPointsRatio;
						float outerPointsAngleZ = inwardMatAngleInputs[2] * outerPointsRatio;

						newRotationInput =
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
				else if (rsLoc == AnalogStickLocation::kBottomRight)
				{
					// Right shoulder blend:
					// Forward, Outward, and Downward.
					if (a_rightShoulder)
					{
						const auto& outwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);
						const auto& downwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kDownward)
						);

						float outwardWeight = max
						(
							RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float downwardWeight = max
						(
							RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = outwardWeight + downwardWeight;

						float outwardAngleX = 
						(
							outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleY = 
						(
							outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleZ = 
						(
							outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleX = 
						(
							downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleY = 
						(
							downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleZ = 
						(
							downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(outwardAngleX + downwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(outwardAngleY + downwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(outwardAngleZ + downwardAngleZ) * outerPointsRatio
						);

						newRotationInput =
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward, Inward, and Downward.
					else
					{
						const auto& inwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);
						const auto& downwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kDownward)
						);

						float inwardWeight = max
						(
							RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float downwardWeight = max
						(
							RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = inwardWeight + downwardWeight;

						float inwardAngleX = 
						(
							inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleY = 
						(
							inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleZ = 
						(
							inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleX = 
						(
							downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleY = 
						(
							downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleZ = 
						(
							downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(inwardAngleX + downwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(inwardAngleY + downwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(inwardAngleZ + downwardAngleZ) * outerPointsRatio
						);

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
				else if (rsLoc == AnalogStickLocation::kNegYAxis)
				{
					// Right and left shoulder blend:
					// Forward and Downward.

					const auto& downwardMatAngleInputs = 
					(
						a_rightShoulder ? 
						rightShoulderMatAngleInputs.at(ArmOrientation::kDownward) : 
						leftShoulderMatAngleInputs.at(ArmOrientation::kDownward)
					);

					float outerPointsAngleX = downwardMatAngleInputs[0] * outerPointsRatio;
					float outerPointsAngleY = downwardMatAngleInputs[1] * outerPointsRatio;
					float outerPointsAngleZ = downwardMatAngleInputs[2] * outerPointsRatio;

					newRotationInput = 
					{
						forwardAngleX + outerPointsAngleX,
						forwardAngleY + outerPointsAngleY,
						forwardAngleZ + outerPointsAngleZ
					};
				}
				else if (rsLoc == AnalogStickLocation::kBottomLeft)
				{
					// Right shoulder blend:
					// Forward, Inward, and Downward.
					if (a_rightShoulder)
					{
						const auto& inwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);
						const auto& downwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kDownward)
						);

						float inwardWeight = max
						(
							RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float downwardWeight = max
						(
							RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = inwardWeight + downwardWeight;

						float inwardAngleX = 
						(
							inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleY = 
						(
							inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleZ = 
						(
							inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleX = 
						(
							downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleY = 
						(
							downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleZ = 
						(
							downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(inwardAngleX + downwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(inwardAngleY + downwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(inwardAngleZ + downwardAngleZ) * outerPointsRatio
						);

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward, Outward, and Downward.
					else
					{
						const auto& outwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);
						const auto& downwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kDownward)
						);

						float outwardWeight = max
						(
							RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float downwardWeight = max
						(
							RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = outwardWeight + downwardWeight;

						float outwardAngleX = 
						(
							outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleY = 
						(
							outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleZ = 
						(
							outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleX = 
						(
							downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleY = 
						(
							downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight)
						);
						float downwardAngleZ = 
						(
							downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(outwardAngleX + downwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(outwardAngleY + downwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(outwardAngleZ + downwardAngleZ) * outerPointsRatio
						);

						newRotationInput =
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
				else if (rsLoc == AnalogStickLocation::kNegXAxis)
				{
					// Right shoulder blend:
					// Forward and Inward
					if (a_rightShoulder)
					{
						const auto& inwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);

						float outerPointsAngleX = inwardMatAngleInputs[0] * outerPointsRatio;
						float outerPointsAngleY = inwardMatAngleInputs[1] * outerPointsRatio;
						float outerPointsAngleZ = inwardMatAngleInputs[2] * outerPointsRatio;

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward and Outward.
					else
					{
						const auto& outwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);
						float outerPointsAngleX = outwardMatAngleInputs[0] * outerPointsRatio;
						float outerPointsAngleY = outwardMatAngleInputs[1] * outerPointsRatio;
						float outerPointsAngleZ = outwardMatAngleInputs[2] * outerPointsRatio;

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
				else if (rsLoc == AnalogStickLocation::kTopLeft)
				{
					// Right shoulder blend:
					// Forward, Inward, and Upward.
					if (a_rightShoulder)
					{
						const auto& inwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kInward)
						);
						const auto& upwardMatAngleInputs = 
						(
							rightShoulderMatAngleInputs.at(ArmOrientation::kUpward)
						);

						float inwardWeight = max
						(
							RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float upwardWeight = max
						(
							RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = inwardWeight + upwardWeight;

						float inwardAngleX = 
						(
							inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleY = 
						(
							inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight)
						);
						float inwardAngleZ = 
						(
							inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleX = 
						(
							upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleY = 
						(
							upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleZ = 
						(
							upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = (inwardAngleX + upwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (inwardAngleY + upwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (inwardAngleZ + upwardAngleZ) * outerPointsRatio;

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
					// Left shoulder blend:
					// Forward, Outward, and Upward.
					else
					{
						const auto& outwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kOutward)
						);
						const auto& upwardMatAngleInputs = 
						(
							leftShoulderMatAngleInputs.at(ArmOrientation::kUpward)
						);

						float outwardWeight = max
						(
							RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float upwardWeight = max
						(
							RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f
						);
						float totalOuterPointsWeight = outwardWeight + upwardWeight;

						float outwardAngleX = 
						(
							outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleY = 
						(
							outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight)
						);
						float outwardAngleZ = 
						(
							outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleX = 
						(
							upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleY = 
						(
							upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight)
						);
						float upwardAngleZ = 
						(
							upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight)
						);

						float outerPointsAngleX = 
						(
							(outwardAngleX + upwardAngleX) * outerPointsRatio
						);
						float outerPointsAngleY = 
						(
							(outwardAngleY + upwardAngleY) * outerPointsRatio
						);
						float outerPointsAngleZ = 
						(
							(outwardAngleZ + upwardAngleZ) * outerPointsRatio
						);

						newRotationInput = 
						{
							forwardAngleX + outerPointsAngleX,
							forwardAngleY + outerPointsAngleY,
							forwardAngleZ + outerPointsAngleZ
						};
					}
				}
			}
		}

		// Update blend state after potentially setting the rotation modified flag 
		// and before setting current and target rotations.
		UpdateNodeRotationBlendState(a_p, shoulderData, a_shoulderNodePtr, true);

		// Init to default rotation if not adjusted.
		if (shoulderData->rotationModified) 
		{
			// Gives a slightly more weighted feel to arm movement.
			float stickSpeedFactor = 
			(
				Util::InterpolateEaseOut
				(
					0.85f,
					1.0f, 
					std::clamp
					(
						0.6f, 
						1.0f, 
						rsData.stickAngularSpeed / 20.0f
					),
					2.0f
				)
			);

			// Slow down if low on stamina -> less likely to knock down NPCs with a slap.
			const float staminaSlowdownFactor = a_p->mm->GetArmRotationFactor(true);
			// Set rotation angle inputs used to construct the world rotation matrix below.
			const float oldYaw = shoulderData->rotationInput[0];
			const float oldPitch = shoulderData->rotationInput[1];
			float yaw = Util::InterpolateSmootherStep
			(
				oldYaw, 
				newRotationInput[0], 
				min(1.0f, interpFactor * stickSpeedFactor * staminaSlowdownFactor)
			);
			float pitch = Util::InterpolateSmootherStep
			(
				oldPitch,
				newRotationInput[1], 
				min(1.0f, interpFactor * stickSpeedFactor * staminaSlowdownFactor)
			);
			shoulderData->rotationInput = std::array<float, 3>({ yaw, pitch, 0.0f });

			// Construct world rotation from our angle inputs.
			auto newWorldTransform = a_shoulderNodePtr->world;
			Util::SetRotationMatrixPYR
			(
				newWorldTransform.rotate, 
				shoulderData->rotationInput[1], 
				shoulderData->rotationInput[0] + a_p->coopActor->GetHeading(false), 
				0.0f
			);

			// Get and set corresponding local rotation from our desired world rotation.
			auto parent = RE::NiPointer<RE::NiAVObject>(a_shoulderNodePtr->parent); 
			if (parent && parent.get())
			{
				RE::NiTransform inverseParent
				{
					defaultNodeWorldTransformsMap.contains(parent) ?
					defaultNodeWorldTransformsMap.at(parent).Invert() : 
					parent->world.Invert()
				};

				shoulderData->targetRotation = (inverseParent * newWorldTransform).rotate;
			}
		}
		else
		{
			// Set to default when not modified.
			shoulderData->targetRotation = shoulderData->defaultRotation;
		}

		// TODO: Figure out a way to produce a collision
		// that doesn't trigger combat. 
		// Also need to set hit data flags,
		// such as power attack/sneak hit flags 
		// for subsequent collider hits. 
		// As of right now, all hits do damage,
		// start combat with the hit target,
		// and do not have applied effects from special hit type flags.

		// Add Precision collider to shoulder node
		// if the player just started modifying node rotations.
		/*
		bool shouldStart = 
		(
			(!shoulderData->precisionColliderAdded) &&
			(
				(
					a_rightShoulder ? 
					a_p->pam->JustStarted(InputAction::kRotateRightShoulder) :
					a_p->pam->JustStarted(InputAction::kRotateLeftShoulder)
				) ||
				(
					(rsData.stickLinearSpeed > 1E-2f) &&
					(
						a_rightShoulder ? 
						a_p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
						a_p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
					)
				)
			)
		);
		bool shouldStop = 
		(
			(shoulderData->precisionColliderAdded) &&
			(
				(rsData.stickLinearSpeed <= 1E-2f) ||
				(
					a_rightShoulder ? 
					a_p->pam->IsNotPerforming(InputAction::kRotateRightShoulder) :
					a_p->pam->IsNotPerforming(InputAction::kRotateLeftShoulder)
				)
			)
		);
		if (shouldStart)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_shoulderNodePtr.get(), 
				PrecisionAnnotationReqType::kStart
			);
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_shoulderNodePtr.get(), 
				PrecisionAnnotationReqType::kAdd,
				0.05f,
				1.0f
			);
			
			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: ADD collision for {}.",
				a_p->coopActor->GetName(),
				a_shoulderNodePtr->name);
			shoulderData->precisionColliderAdded = true;
		}
		else if (shouldStop)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_shoulderNodePtr.get(), 
				PrecisionAnnotationReqType::kRemove
			);
			
			SPDLOG_DEBUG("[MM] UpdateArmNodeRotationData: {}: REMOVE collision for {}.",
				a_p->coopActor->GetName(),
				a_shoulderNodePtr->name);
			shoulderData->precisionColliderAdded = false;
		}
		
		// Clear previous hits to allow for multiple hits while rotating the node.
		if (rotatingShoulder)
		{
			Util::ChangeNodeColliderState
			(
				a_p->coopActor.get(), 
				a_shoulderNodePtr.get(), 
				PrecisionAnnotationReqType::kMultiHit
			);
		}
		*/

		// Update shoulder node rotation to set (current) 
		// after potentially setting target rotations.
		UpdateNodeRotationToSet(a_p, shoulderData, a_shoulderNodePtr, true);
	}

	void NodeOrientationManager::UpdateTorsoNodeRotationData
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Update torso node blend status and starting, current, and target rotations.
		// Modification method: local rotation derived from
		// rotating the base world rotation's axes by a pitch/roll determined
		// by the player's RS movement.

		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		auto loadedData = a_p->coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);
		bool isMounted = a_p->coopActor->IsOnMount();
		bool isRangedWeaponPrimed = false;
		// Only rotate when an equipped ranged weapon is primed.
		if (a_p->em->HasBowEquipped())
		{
			const auto& meleeAttackState = a_p->coopActor->actorState1.meleeAttackState;
			isRangedWeaponPrimed = 
			{
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased
			};
		}
		else if (a_p->em->HasCrossbowEquipped())
		{
			const auto& meleeAttackState = a_p->coopActor->actorState1.meleeAttackState;
			isRangedWeaponPrimed = 
			{
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased 
			};
		}

		// Rotate the torso throughout the attack animation's duration 
		// when not attacking with a 2H ranged weapon.
		bool isAttackingWithoutRangedWeap = 
		(
			!a_p->em->Has2HRangedWeapEquipped() && a_p->pam->isAttacking
		);
		// Performing an action towards a target.
		bool isAiming =
		{
			isAttackingWithoutRangedWeap ||
			isRangedWeaponPrimed ||
			a_p->pam->isBlocking ||
			a_p->pam->isBashing ||
			a_p->pam->isInCastingAnim ||
			a_p->pam->usingLHStaff->value == 1.0f ||
			a_p->pam->usingRHStaff->value == 1.0f ||
			a_p->pam->isVoiceCasting ||
			a_p->pam->IsPerforming(InputAction::kQuickSlotCast)
		};

		// Skip head node adjustments while transformed for the time being.
		// Have to verify everything works while transformed into a werewolf, for example.
		const uint8_t numAdjustableNodes = 
		(
			a_p->isTransformed ? 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size() - 1 : 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size()
		);

		// Base axes/rotation set from the cached default world rotation for each node.
		RE::NiPoint3 baseXAxis{ };
		RE::NiPoint3 baseYAxis{ };
		RE::NiPoint3 baseZAxis{ };
		RE::NiMatrix3 baseWorldRot{ };
		// Torso node's parent world transform.
		RE::NiTransform parentWorld{ };

		// Set the axis of rotation to use when rotating the torso nodes.
		bool isAttackingWith2HRangedWeapon = 
		(
			a_p->pam->isAttacking && a_p->em->Has2HRangedWeapEquipped()
		);
		if (isAttackingWith2HRangedWeapon) 
		{
			// Use the player's default attack source direction when aiming with a ranged weapon.
			a_p->mm->playerTorsoAxisOfRotation = a_p->mm->playerDefaultAttackSourceDir.Cross(up);
		}
		else
		{
			// If not aiming with a ranged weapon, instead use the player's facing direction
			// in the XY plane as the forward vector along which to derive the axis.
			auto aimingXYDir = 
			(
				Util::RotationToDirectionVect
				(
					0.0f, Util::ConvertAngle(a_p->coopActor->data.angle.z)
				)
			);

			a_p->mm->playerTorsoAxisOfRotation = aimingXYDir.Cross(up);
		}

		// Adjust all torso nodes.
		for (uint8_t i = 0; i < numAdjustableNodes; ++i)
		{
			const auto& nodeName = GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES[i];
			auto torsoNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); 

			// Node is invalid, on to the next one.
			if (!torsoNodePtr || !torsoNodePtr.get())
			{
				continue;
			}
				
			// Ensure the torso node is accounted for in rotation data map.
			if (!nodeNameToRotationDataMap.contains(torsoNodePtr->name))
			{
				// Set all rotations to the node's current local rotation 
				// so we have valid data to start off with.
				const auto& newData = 
				(
					*nodeNameToRotationDataMap.insert_or_assign
					(
						torsoNodePtr->name, std::make_unique<NodeRotationData>()
					).first
				).second;
				newData->currentRotation = 
				newData->defaultRotation = 
				newData->startingRotation = 
				newData->targetRotation = torsoNodePtr->local.rotate;
			}

			// Rotation data we will modify.
			const auto& torsoData = nodeNameToRotationDataMap[torsoNodePtr->name];
			// Set the rotation state flags before updating the blend state.
			torsoData->prevInterrupted = torsoData->interrupted;
			torsoData->prevRotationModified = torsoData->rotationModified;
			// Do not set the custom rotation if not aiming with a drawn weapon,
			// or if the player is ragdolled or inactive.
			torsoData->interrupted = 
			{
				(
					!a_p->mm->isDashDodging && 
					!isAiming && 
					a_p->coopActor->IsWeaponDrawn()
				) ||
				a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
				a_p->IsAwaitingRefresh()
			};
			// Rotation was modified if the player has adjusted their aim pitch
			// or if they are dash dodging on the ground.
			torsoData->rotationModified = 
			{
				(a_p->mm->aimPitchAdjusted) || 
				(
					a_p->mm->isDashDodging && 
					!a_p->mm->isParagliding
				)
			};

			// Update blend state after modifying the rotation flags
			// and before setting current and target rotations.
			UpdateNodeRotationBlendState(a_p, torsoData, torsoNodePtr, false);

			// Get the game's default world rotation for this node and derive its axes.
			if (defaultNodeWorldTransformsMap.contains(torsoNodePtr))
			{
				const auto& defaultWorldRotation = 
				(
					defaultNodeWorldTransformsMap.at(torsoNodePtr).rotate
				);
				baseXAxis = defaultWorldRotation * right;
				baseYAxis = defaultWorldRotation * forward;
				baseZAxis = defaultWorldRotation * up;
				baseWorldRot = defaultWorldRotation;

				// Get parent world transform of the base spinal node.
				// All other nodes will have their world rotations set 
				// with respect to this node's default world rotation.
				if (i == 0) 
				{
					auto parentNodePtr = RE::NiPointer<RE::NiAVObject>(torsoNodePtr->parent);
					if (parentNodePtr && parentNodePtr.get()) 
					{
						if (defaultNodeWorldTransformsMap.contains(parentNodePtr))
						{
							parentWorld = defaultNodeWorldTransformsMap.at(parentNodePtr);
						}
						else
						{
							parentWorld = parentNodePtr->world;
						}
					}
				}
			}

			if (torsoData->rotationModified)
			{
				RE::NiPoint3 newXAxis = baseXAxis;
				RE::NiPoint3 newYAxis = baseYAxis;
				RE::NiPoint3 newZAxis = baseZAxis;

				// Fraction of the aim pitch angle to rotate this node by.
				// Additional pitch/roll relative to the parent for each node
				// tapers off when moving up the spinal column.
				float frac = 1.0f;
				if (i == 0)
				{
					// Rotated 50% of the way there.
					frac = 0.5f;
				}
				else if (i == 1)
				{
					// Rotated 83.333333% of the way there (66.666666% increase from previous).
					frac = 5.0f / 6.0f;
				}
				else if (i == 2)
				{
					// Rotated 100% of the way there (20% increase from previous).
					frac = 1.0f;
				}

				// For adjustment towards a target.
				// Pitched/rolled with the aim pitch angle.
				if (a_p->mm->aimPitchAdjusted) 
				{
					torsoData->rotationInput[1] = -a_p->mm->aimPitch;
					// Pitch all the axes first.
					Util::RotateVectorAboutAxis
					(
						newXAxis, 
						a_p->mm->playerTorsoAxisOfRotation, 
						torsoData->rotationInput[1] * frac
					);
					Util::RotateVectorAboutAxis
					(
						newYAxis, 
						a_p->mm->playerTorsoAxisOfRotation, 
						torsoData->rotationInput[1] * frac
					);
					Util::RotateVectorAboutAxis
					(
						newZAxis,
						a_p->mm->playerTorsoAxisOfRotation, 
						torsoData->rotationInput[1] * frac
					);

					// Special case.
					// Modify the yaw of the torso nodes if the player is aiming
					// with a ranged weapon while mounted,
					// since the game does not automatically rotate our players 
					// to face the crosshair target.
					if (isMounted && isAttackingWith2HRangedWeapon)
					{
						// Yaw offset endpoints.
						float prevYawOffset = torsoData->rotationInput[0];
						float yawOffset = 0.0f;
						// Must be targeting something with the crosshair.
						auto crosshairRefrPtr = Util::GetRefrPtrFromHandle
						(
							a_p->tm->crosshairRefrHandle
						);
						if (crosshairRefrPtr && 
							Util::IsValidRefrForTargeting(crosshairRefrPtr.get()))
						{
							// Yaw offset is determined relative to the default facing angle
							// and position.
							auto playerAimYaw = Util::DirectionToGameAngYaw
							(
								a_p->mm->playerDefaultAttackSourceDir
							);
							float playerToTargetYaw = Util::GetYawBetweenPositions
							(
								a_p->mm->playerDefaultAttackSourcePos, a_p->tm->crosshairWorldPos
							);
							yawOffset = Util::NormalizeAngToPi(playerAimYaw - playerToTargetYaw);
							// Prevent interpolation along the 'longer' path 
							// between the two yaw endpoints by shifting the target endpoint 
							// to an equivalent angle that is closer to the starting endpoint.
							if (fabsf(yawOffset - prevYawOffset) > PI) 
							{
								if (yawOffset <= 0.0f) 
								{
									yawOffset += 2.0f * PI;
								}
								else
								{
									yawOffset -= 2.0f * PI;
								}
							}
						}

						torsoData->rotationInput[0] = Util::InterpolateSmootherStep
						(
							prevYawOffset, 
							yawOffset, 
							min(1.0f, interpFactor)
						);

						// Rotate the axes about the world 'up' axis with our yaw offset.
						if (torsoData->rotationInput[0] != 0.0f) 
						{
							// Normalized to take the shortest path.
							torsoData->rotationInput[0] = Util::NormalizeAngToPi
							(
								torsoData->rotationInput[0]
							);
							Util::RotateVectorAboutAxis
							(
								newXAxis, 
								up, 
								torsoData->rotationInput[0] * frac
							);
							Util::RotateVectorAboutAxis
							(
								newYAxis, 
								up, 
								torsoData->rotationInput[0] * frac
							);
							Util::RotateVectorAboutAxis
							(
								newZAxis,
								up, 
								torsoData->rotationInput[0] * frac
							);
						}
					}
				}

				// When dash dodging on the ground, tilt the spine in the direction of the dodge.
				// The pitch and roll offsets are set in the movement manager, 
				// so we just apply them here.
				if (a_p->mm->isDashDodging && !a_p->mm->isParagliding) 
				{
					if (a_p->mm->dashDodgeTorsoPitchOffset != 0.0f)
					{
						Util::RotateVectorAboutAxis
						(
							newXAxis,
							baseXAxis,
							a_p->mm->dashDodgeTorsoPitchOffset * frac
						);
						Util::RotateVectorAboutAxis
						(
							newYAxis,
							baseXAxis,
							a_p->mm->dashDodgeTorsoPitchOffset * frac
						);
						Util::RotateVectorAboutAxis
						(
							newZAxis,
							baseXAxis,
							a_p->mm->dashDodgeTorsoPitchOffset * frac
						);
					}

					if (a_p->mm->dashDodgeTorsoRollOffset != 0.0f)
					{
						Util::RotateVectorAboutAxis
						(
							newXAxis,
							baseYAxis,
							a_p->mm->dashDodgeTorsoRollOffset * frac
						);
						Util::RotateVectorAboutAxis
						(
							newYAxis,
							baseYAxis,
							a_p->mm->dashDodgeTorsoRollOffset * frac
						);
						Util::RotateVectorAboutAxis
						(
							newZAxis,
							baseYAxis,
							a_p->mm->dashDodgeTorsoRollOffset * frac
						);
					}
				}
				
				RE::NiMatrix3 newWorldRot{ newXAxis, newYAxis, newZAxis };
				// NOTE: Must transpose to ensure that our modified axes 
				// are set as the new target local rotation matrix's axes.
				newWorldRot = newWorldRot.Transpose();
				RE::NiTransform newTrans
				{
					torsoNodePtr->world 
				};
				newTrans.rotate = newWorldRot;
				// Set local rotation corresponding to our world rotation.
				torsoData->targetRotation = (parentWorld.Invert() * newTrans).rotate;
				// Update the parent world transform for the next torso node 
				// by setting it to the current node's new world transform
				// so that we can emulate a 'downward' pass of our own 
				// when setting subsequent child torso nodes' rotations.
				parentWorld = newTrans;
			}
			else
			{
				// Set to default when not modified.
				torsoData->targetRotation = torsoData->defaultRotation;
			}

			// Update torso node rotation to set (current)
			// after potentially setting target rotations.
			UpdateNodeRotationToSet(a_p, torsoData, torsoNodePtr, false);
		}
	}
}

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
			ALYSLC::Log("[MM] Initialize: Constructor for {}, CID: {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p ? p->controllerID : -1,
				p.use_count());
			RefreshData();
		}
		else
		{
			logger::error("[MM] ERR: Initialize: Cannot construct Movement Manager for controller ID {}.", a_p ? a_p->controllerID : -1);
		}
	}

#pragma region MANAGER_FUNCS_IMPL
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
		ALYSLC::Log("[MM] PrePauseTask: P{}", playerID + 1);

		// Set player 1 as motion driven when the manager is not active
		// to restore normal movement.
		if (p->isPlayer1)
		{
			Util::SetPlayerAIDriven(false);
		}

		// Force the player to get up out of ragdoll state before pausing.
		if (coopActor->IsInRagdollState() && glob.coopSessionActive && !p->isDowned && !coopActor->IsDead())
		{
			coopActor->NotifyAnimationGraph("GetUpBegin");
			coopActor->PotentiallyFixRagdollState();
		}

		// Reset pitch angle, speedmult.
		coopActor->data.angle.x = 0.0f;
		coopActor->SetActorValue(RE::ActorValue::kSpeedMult, 100.0f);
		coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -0.001f);
		coopActor->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 0.001f);

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
		ALYSLC::Log("[MM] PreStartTask: P{}", playerID + 1);
		ResetTPs();

		// Set player 1 as AI driven to allow for movement manipulation with this manager.
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
		RE::NiPoint3(0.0f, 0.0f, 0.0f);
		// Atomic flags.
		shouldFaceTarget = shouldResetAimAndBody = startJump = false;
		// Movement parameters list.
		movementOffsetParams = std::vector<float>(!MoveParams::kTotal, 0.0f);
		// Node rotation data.
		nom = std::make_unique<NodeOrientationManager>();
		// Booleans.
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
		rsMoved = false;
		sentJumpFallEvent = false;
		shouldAdjustAimPitch = false;
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
		framesSinceStartingJump = 0;

		// Reset time points used by this manager.
		ResetTPs();
		ALYSLC::Log("[MM] RefreshData: {}.", coopActor ? coopActor->GetName() : "NONE");
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

#pragma endregion

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

	void MovementManager::KeepOffsetFromActor(const RE::ActorHandle& a_targetHandle, const RE::NiPoint3& a_posOffset, const RE::NiPoint3& a_angOffset, float a_catchUpRadius, float a_followRadius)
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
		// Won't trigger when flying, mounted, swimming, jumping, blocking, or sneaking.
		
		// Must have a character controller attached.
		auto charController = coopActor->GetCharController(); 
		if (!charController) 
		{
			isRequestingDashDodge = isDashDodging = isBackStepDodge = false;
			framesSinceStartingDashDodge = framesSinceRequestingDashDodge = 0;
			dashDodgeCompletionRatio = 0.0f;
			return;
		}

		if (isRequestingDashDodge && !isDashDodging)
		{
			framesSinceStartingDashDodge = framesSinceRequestingDashDodge = 0;
			dashDodgeCompletionRatio = 0.0f;
			if (shouldFaceTarget)
			{
				// CUSTOM BINDS NOTE: Facing the target while dash dodging with the Dodge bind set to the same inputs as Sprint
				// will stop the player from translating while dodging because of the movement type switch to NPCSprinting.
				coopActor->data.angle.z = Util::NormalizeAng0To2Pi
				(
					Util::GetYawBetweenPositions(coopActor->data.location, p->tm->crosshairWorldPos)
				);
			}

			// Player has to move clearly.
			SetDontMove(false);
			// Stop attacking/sprinting before dodging.
			coopActor->NotifyAnimationGraph("attackStop");
			coopActor->NotifyAnimationGraph("SprintStop");
			coopActor->NotifyAnimationGraph("staggerStop");
			coopActor->NotifyAnimationGraph("moveStart");
			// Can dash dodge only if the sneak start animation event is processed successfully.
			bool succ = isParagliding || coopActor->NotifyAnimationGraph("SneakStart");
			isDashDodging = succ;
			isRequestingDashDodge = !succ;
			// Back step if not moving the LS or if staggered.
			// Backstep dodge creates the most separation from followup attacks when the player is staggered.
			const auto& lsMag = glob.cdh->GetAnalogStickState(controllerID, true).normMag;
			isBackStepDodge = 
			(
				(isDashDodging) && 
				(
					!lsMoved || 
					coopActor->actorState2.staggered
				)
			);
		}

		if (isDashDodging)
		{
			if (framesSinceRequestingDashDodge == 0) 
			{
				p->lastDashDodgeTP = SteadyClock::now();
				// Cache LS displacement and equipped weight.
				dashDodgeLSDisplacement = isBackStepDodge ? 1.0f : glob.cdh->GetAnalogStickState(controllerID, true).normMag;
				dashDodgeEquippedWeight = coopActor->GetEquippedWeight();
			}

			const uint32_t totalFrameCount = (Settings::uDashDodgeSetupFrameCount + Settings::uDashDodgeBaseAnimFrameCount);
			float secsSinceStartingDodge = Util::GetElapsedSeconds(p->lastDashDodgeTP);
			// Frame progress towards completing the dodge.
			float framesCompletionRatio = std::clamp((float)framesSinceRequestingDashDodge / totalFrameCount, 0.0f, 1.0f);
			// Completion time given at 60 FPS.
			// How close the dodge is to ending based on the time elapsed.
			float timeCompletionRatio = std::clamp(secsSinceStartingDodge / ((1.0f / 60.0f) * totalFrameCount), 0.0f, 1.0f);
			// At low framerates, each frame takes longer to execute, so the player will dodge for 
			// a longer time and the dodge displacement will be much further than intended.
			// In this case, compare to the fixed 60 FPS dodge time interval instead.
			dashDodgeCompletionRatio = timeCompletionRatio > framesCompletionRatio ? timeCompletionRatio : framesCompletionRatio;

			// Ensure the player can rotate so that they keep their dodge momentum
			// even when interrupting a power attack, during the duration of which rotation is disabled.
			bool allowRotation = false;
			coopActor->GetGraphVariableBool("bAllowRotation", allowRotation);
			// Yes, I'm not sure why it's named 'bAllowRotation' when setting it to true does just the opposite.
			if (allowRotation) 
			{
				coopActor->SetGraphVariableBool("bAllowRotation", false);
			}

			coopActor->SetGraphVariableBool("bAnimationDriven", false);
			Util::NativeFunctions::SetDontMove(coopActor.get(), false);

			bool dodgeDurationExpired = dashDodgeCompletionRatio == 1.0f;
			// Can dodge with either the regular dodge bind, or the special action bind.
			// Check the time since the more recently-triggered action.
			float secsSinceLastSpecialAction = p->pam->GetSecondsSinceLastStop(InputAction::kSpecialAction);
			float secsSinceLastDodge = p->pam->GetSecondsSinceLastStop(InputAction::kDodge);
			bool startedAttackDuringDodge = Util::GetElapsedSeconds(p->lastAttackStartTP) < min(secsSinceLastDodge, secsSinceLastSpecialAction);
			// If an attack was started while dodging or if dodge frame duration is up, stop dodging.
			if (dodgeDurationExpired || startedAttackDuringDodge)
			{
				isDashDodging = isBackStepDodge = false;

				// Do not stop in mid-air.
				if (!isParagliding) 
				{
					// Stop moving once dodge stops.
					SetDontMove(true);

					// Stop sneak animation which played for the duration of the dodge.
					if (!p->isTransformed)
					{
						coopActor->NotifyAnimationGraph("SneakStop");
					}

					// Reset torso and character controller pitch.
					charController->pitchAngle = charController->rollAngle = 
					dashDodgeTorsoPitchOffset = dashDodgeTorsoRollOffset = 0.0f;
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
				// Set I-frames when inside window, reset otherwise.
				if (!isGhost)
				{
					baseFlags.set(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
				}

				// Set direction and XY speed on the first frame of the dodge.
				// Rotation is locked by the movement type and the only
				// change made to the XY velocity after the first frame is the XY speed.
				if (framesSinceStartingDashDodge == 0)
				{
					if (isBackStepDodge || !lsMoved)
					{
						// Dodge backward.
						dashDodgeDir = Util::RotationToDirectionVect
						(
							0.0f, 
							Util::ConvertAngle(Util::NormalizeAng0To2Pi(coopActor->GetHeading(false) - PI))
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
						maxXYSpeed *= (glob.paraglidingMT->movementTypeData.defaultData.speeds[RE::Movement::SPEED_DIRECTIONS::kForward][RE::Movement::MaxSpeeds::kRun]);
					}
					else
					{
						maxXYSpeed *= 700.0f;
					}

					// P1 dodges further than companion players, even if the dash dodge speeds are set to the same value.
					// Halve the dodge speed for P1 here.
					maxXYSpeed *= 0.5f;
				}
				else
				{
					maxXYSpeed = Settings::fMaxDashDodgeSpeedmult;
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
				// Restore original Z component if paragliding.
				if (isParagliding) 
				{
					havokVel.quad.m128_f32[2] = zComp;
				}
				else if (charController->surfaceInfo.supportedState != RE::hkpSurfaceInfo::SupportedState::kSupported)
				{
					// Apply gravity to get downward velocity (gravity not in effect if linear velocity was set directly while in the air).
					havokVel.quad.m128_f32[2] = -9.8f * secsSinceStartingDodge;
				}
				
				// Velocity only changes when the current state is not grounded.
				charController->wantState = charController->context.currentState = RE::hkpCharacterStateType::kInAir;

				// Set new dodge velocity.
				charController->SetLinearVelocityImpl(havokVel);
				// Check if the player should land when dash dodging on the ground.
				if (!isParagliding)
				{
					charController->flags.set(RE::CHARACTER_FLAGS::kCheckSupport, RE::CHARACTER_FLAGS::kFloatLand, RE::CHARACTER_FLAGS::kJumping);
				}

				// Update leaning.
				if (const float setupRatio = Settings::uDashDodgeSetupFrameCount / totalFrameCount; dashDodgeCompletionRatio >= setupRatio)
				{
					// Adjust player character tilt to lean in the direction of the dodge.
					const float animRatio = dashDodgeCompletionRatio - setupRatio;
					const float animCompletionRatio = 1.0f - setupRatio;
					const float halfAnimCompletionRatio = animCompletionRatio / 2.0f;
					// Lean less when moving fast (trip hazard).
					const float maxLeanAngle = 
					{
						(isParagliding) ?
						(p->isPlayer1 ? PI / 2.0f : PI / 2.0f) :
						(
							Util::InterpolateSmootherStep
							(
								7.0f * PI / 18.0f, 
								PI / 6.0f, 
								dashDodgeXYSpeed / Settings::fMaxDashDodgeSpeedmult
							)
						)
					};

					// Reference to the pitch/roll angle we want to modify.
					// We want to tilt the entire character controller when paragliding,
					// but only the torso nodes when on the ground.
					float& pitchToSet = isParagliding ? charController->pitchAngle : dashDodgeTorsoPitchOffset;
					float& rollToSet = isParagliding ? charController->rollAngle : dashDodgeTorsoRollOffset;
					// Endpoint to interp from in the first half of the interval 
					// and to interp to in the second half of the interval.
					// NOTE: Our changes to char controller angles are wiped by the game each frame.
					const float endpointPitch = charController->pitchAngle ? pitchToSet : 0.0f;
					const float endpointRoll = charController->rollAngle ? rollToSet : 0.0f;
					if (isBackStepDodge)
					{
						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean back.
							pitchToSet = Util::InterpolateEaseOut
							(
								endpointPitch, 
								isParagliding ? -maxLeanAngle : maxLeanAngle, 
								(animRatio / halfAnimCompletionRatio),
								2.0f
							);
						}
						else
						{
							// Straighten back out by the time the dodge ends.
							pitchToSet = Util::InterpolateEaseIn
							(
								isParagliding ? -maxLeanAngle : maxLeanAngle,
								endpointPitch,
								(animRatio / halfAnimCompletionRatio) - 1.0f,
								2.0f
							);
						}
					}
					else
					{
						// Lean in direction of movement (mix of pitch and roll angle modifications).
						float normSpeed = dashDodgeDir.Unitize();
						auto dashDodgeYaw = Util::DirectionToGameAngYaw(dashDodgeDir);
						// Backward (180 degree diff) if not moving.
						float movementToFacingYawDiff = 
						(
							normSpeed == 0.0f ? 
							PI : 
							Util::NormalizeAngToPi(dashDodgeYaw - coopActor->data.angle.z)
						);
						float absAngDiffMod = fmodf(fabsf(movementToFacingYawDiff), PI);
						float pitchRatio = 
						(
							absAngDiffMod <= PI / 2.0f ? 
							(1.0f - absAngDiffMod / (PI / 2.0f)) : 
							(absAngDiffMod / (PI / 2.0f) - 1.0f)
						);
						float rollRatio = 1.0f - pitchRatio;
						float pitchSign = fabsf(movementToFacingYawDiff) <= PI / 2.0f ? -1.0f : 1.0f;
						float rollSign = movementToFacingYawDiff <= 0.0f ? -1.0f : 1.0f;
						// Flip sign convention when paragliding.
						if (isParagliding) 
						{
							pitchSign *= -1.0f;
							rollSign *= -1.0f;
						}

						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean in movement direction.
							pitchToSet = Util::InterpolateEaseOut
							(
								endpointPitch, 
								maxLeanAngle * pitchRatio * pitchSign, (animRatio / halfAnimCompletionRatio), 
								2.0f
							);
							rollToSet = Util::InterpolateEaseOut
							(
								endpointRoll, 
								maxLeanAngle * rollRatio * rollSign, (animRatio / halfAnimCompletionRatio), 
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
								(animRatio / halfAnimCompletionRatio) - 1.0f, 2.0f
							);
							rollToSet = Util::InterpolateEaseIn
							(
								maxLeanAngle * rollRatio * rollSign, 
								endpointRoll, 
								(animRatio / halfAnimCompletionRatio) - 1.0f, 2.0f
							);
						}
					}

					++framesSinceStartingDashDodge;
				}
			}

			++framesSinceRequestingDashDodge;
		}
		else if (isRequestingDashDodge && 
				p->pam->GetSecondsSinceLastStop(InputAction::kDodge) > *g_deltaTimeRealTime * 
				(Settings::uDashDodgeBaseAnimFrameCount + Settings::uDashDodgeSetupFrameCount) * 2.0f)
		{
			// Failsafe to reset after twice the dodge duration and dodge request was not handled.
			isRequestingDashDodge = false;
			p->pam->avcam->RemoveRequestedAction(AVCostAction::kDodge);
			p->pam->avcam->RemoveStartedAction(AVCostAction::kDodge);
			if (auto actorBase = coopActor->GetActorBase(); actorBase)
			{
				actorBase->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kIsGhost);
			}
		}
	}

	void MovementManager::PerformJump()
	{
		// Jump. That's it.

		if (auto charController = coopActor->GetCharController())
		{
			auto& currentHKPState = charController->context.currentState;
			const uint32_t jumpAscentFramecount = max(1, static_cast<uint32_t>(Settings::fSecsAfterGatherToFall * (1.0f / *g_deltaTimeRealTime) + 0.5f));
			// Start jump: play gather animation(s) and invert gravity for the player.
			if (startJump)
			{
				SetDontMove(false);
				RE::hkVector4 velBeforeJump;
				charController->GetLinearVelocityImpl(velBeforeJump);
				if (velBeforeJump.Length3() == 0.0f)
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
					charController->flags.set(RE::CHARACTER_FLAGS::kJumping, RE::CHARACTER_FLAGS::kFloatLand);
					charController->context.currentState = RE::hkpCharacterStateType::kInAir;

					const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
					const bool& lsMoved = lsData.normMag != 0.0f;
					RE::NiPoint2 velBeforeJumpXY{ velBeforeJump.quad.m128_f32[0], velBeforeJump.quad.m128_f32[1] };
					RE::NiPoint2 lsVelXY
					{
						(
							GAME_TO_HAVOK *
							Settings::fJumpBaseLSDirSpeed *
							lsData.normMag *
							cosf(Util::ConvertAngle(movementOffsetParams[!MoveParams::kLSGameAng]))
						),
						(
							GAME_TO_HAVOK *
							Settings::fJumpBaseLSDirSpeed *
							lsData.normMag *
							sinf(Util::ConvertAngle(movementOffsetParams[!MoveParams::kLSGameAng]))
						)
					};

					// Use LS XY velocity or pre-jump XY velocity, whichever is larger.
					velBeforeJump =
					(
						velBeforeJumpXY.Length() >= lsVelXY.Length() ?
						RE::hkVector4(velBeforeJumpXY.x, velBeforeJumpXY.y, havokInitialJumpZVelocity, 0.0f) :
						RE::hkVector4(lsVelXY.x, lsVelXY.y, havokInitialJumpZVelocity, 0.0f)
					);

					// Invert gravity and set initial velocity.
					charController->gravity = -Settings::fJumpingGravityMult;
					charController->SetLinearVelocityImpl(velBeforeJump);
				}
				charController->lock.Unlock();
				// Jump has started.
				framesSinceStartingJump = 0;
				isAirborneWhileJumping = true;
				isFallingWhileJumping = false;
				sentJumpFallEvent = false;
				startJump = false;
				p->jumpStartTP = SteadyClock::now();
				framesSinceStartingJump++;
			}
			else if (isAirborneWhileJumping)
			{
				// Abort jump if ragdolling.
				if (coopActor->IsInRagdollState())
				{
					// Reset jump state variables.
					charController->lock.Lock();
					{
						charController->flags.reset(RE::CHARACTER_FLAGS::kJumping, RE::CHARACTER_FLAGS::kFloatLand);
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
					startJump = false;
					sentJumpFallEvent = false;

					p->jumpStartTP = SteadyClock::now();
					framesSinceStartingJump = 0;
					return;
				}

				// REMOVE when done debugging.
				//ALYSLC::Log("[MM] {}: PerformJump: Flags while falling: 0b{:B}.", coopActor->GetName(), *charController->flags);

				// Handle ascent to peak of the jump at which the player begins to fall.
				if (!isFallingWhileJumping)
				{
					isFallingWhileJumping = framesSinceStartingJump >= jumpAscentFramecount;
					charController->lock.Lock();
					{
						// Zero gravity at apex.
						charController->gravity = Util::InterpolateEaseIn
						(
							-Settings::fJumpingGravityMult, 0.0f,
							static_cast<float>(framesSinceStartingJump) / jumpAscentFramecount,
							2.0f
						);
					}
					charController->lock.Unlock();
					framesSinceStartingJump++;
				}
				else
				{
					// Only send fall animation, which cancels all melee/ranged attack animations,
					// if the player is not attacking or casting.
					if (!sentJumpFallEvent && !p->pam->isAttacking)
					{
						charController->lock.Lock();
						{
							charController->flags.reset(RE::CHARACTER_FLAGS::kJumping, RE::CHARACTER_FLAGS::kFloatLand);
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
					bool stateAllowsLanding = 
					(
						(
							charController->flags.all
							(
								RE::CHARACTER_FLAGS::kCanJump, 
								RE::CHARACTER_FLAGS::kSupport
							)
						) && 
						(
							charController->context.currentState == RE::hkpCharacterStateType::kOnGround &&
							charController->surfaceInfo.supportedState.get() == RE::hkpSurfaceInfo::SupportedState::kSupported
						) 
					);
					if (stateAllowsLanding)
					{
						// Reset jump state variables.
						charController->lock.Lock();
						{
							charController->flags.reset(RE::CHARACTER_FLAGS::kJumping, RE::CHARACTER_FLAGS::kFloatLand);
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
						startJump = false;

						// Have to manually trigger landing animation to minimize occurrences of the hovering bug.
						coopActor->NotifyAnimationGraph("JumpLand");
						charController->lock.Lock();
						{
							charController->surfaceInfo.surfaceNormal = RE::hkVector4(0.0f);
							charController->surfaceInfo.surfaceDistanceExcess = 0.0f;
							charController->surfaceInfo.supportedState = RE::hkpSurfaceInfo::SupportedState::kSupported;
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
							0.0f, Settings::fJumpingGravityMult,
							(
								max(framesSinceStartingJump - jumpAscentFramecount, 0.0f) /
								jumpAscentFramecount),
							2.0f
						);
					}
					charController->lock.Unlock();
					framesSinceStartingJump++;
				}
			}
		}
	}

	void MovementManager::PerformMagicalParaglide()
	{
		// Counterpart to P1's paraglider, if the 'Skyrim's Paraglider' mod is installed
		// and P1 has obtained a paraglider.
		// Now with 1000% more jank and less polish.
		// The paraglider's there in spirit, I promise.
		
		// Nothing else to do for P1.
		if (p->isPlayer1 || !ALYSLC::SkyrimsParagliderCompat::g_paragliderInstalled || 
			!ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider)
		{
			return;
		}

		// All credit goes to Loki:
		// https://github.com/LXIV-CXXVIII/Skyrims-Paraglider/blob/main/src/main.cpp
		auto charController = coopActor->GetCharController();
		if (!charController) 
		{
			// Stop art effects and reset data.
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect1);
			Util::StopHitArt(coopActor.get(), glob.paraglideIndicatorEffect2);
			shouldParaglide = false;
			isParagliding = false;
			magicParaglideVelInterpFactor = magicParaglideEndZVel = magicParaglideStartZVel = 0.0f;
			p->lastParaglidingStateChangeTP = SteadyClock::now();
			return;
		}

		bool isAirborne = charController->context.currentState == RE::hkpCharacterStateType::kInAir;
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
				Util::StartHitArt(coopActor.get(), glob.paraglideIndicatorEffect1, coopActor.get());
				Util::StartHitArt(coopActor.get(), glob.paraglideIndicatorEffect2, coopActor.get());

				// Set starting Z velocity.
				magicParaglideStartZVel = havokVel.quad.m128_f32[2];
				// Is now paragliding.
				isParagliding = true;
				p->lastParaglidingStateChangeTP = SteadyClock::now();
			}

			// Make sure the player is continuously falling while paragliding.
			// NOTE: Game attempts to land the player periodically, even when in the air,
			// and no related animation event to catch is sent via the NotifyAnimationGraph() hook.
			// Sending the fall animation each frame cancels the landing animation,
			// but still leads to a 'hiccup' whenever the game tries to land the player.
			if (coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal) 
			{
				coopActor->NotifyAnimationGraph("JumpFallDirectional");
			}

			// No fall damage while paragliding.
			charController->fallStartHeight = 0.0f;
			charController->fallTime = 0.0f;

			// Hardcoded defaults for now.
			// Will read from paraglide config file later.
			// Default fall speed.
			magicParaglideEndZVel = -2.3f;
			// Rise while gale is active.
			if (glob.tarhielsGaleEffect && coopActor->HasMagicEffect(glob.tarhielsGaleEffect))
			{
				// Gale speed.
				magicParaglideEndZVel = 15.0f;
			}

			auto newHavokZVel = std::lerp(magicParaglideStartZVel, magicParaglideEndZVel, magicParaglideVelInterpFactor);
			if (magicParaglideVelInterpFactor < 1.0f)
			{
				(
					glob.tarhielsGaleEffect && coopActor->HasMagicEffect(glob.tarhielsGaleEffect) ? 
					magicParaglideVelInterpFactor += 0.01f : 
					magicParaglideVelInterpFactor += 0.025f
				);
			}

			havokVel.quad.m128_f32[2] = newHavokZVel;
			charController->SetLinearVelocityImpl(havokVel);

			if (!isDashDodging) 
			{
				// Tilt character controller in the player's gliding direction.
				const float maxTiltAngle = PI / 2.0f;
				RE::NiPoint3 linVelXY = RE::NiPoint3
				(
					charController->outVelocity.quad.m128_f32[0], 
					charController->outVelocity.quad.m128_f32[1], 
					0.0f
				);
				float normXYSpeed = linVelXY.Unitize();
				float pitchRatio = 0.0f;
				float pitchSign = 1.0f;
				float rollRatio = 0.0f;
				float rollSign = 1.0f;
				// Remain upright if not moving.
				if (normXYSpeed != 0.0f)
				{
					auto linVelYaw = Util::DirectionToGameAngYaw(linVelXY);
					float movementToFacingYawDiff = Util::NormalizeAngToPi(linVelYaw - coopActor->data.angle.z);
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


			// Rotate char controller back to upright position.
			charController->pitchAngle = Util::InterpolateEaseInEaseOut(charController->pitchAngle, 0.0f, 0.25f, 2.0f);
			charController->rollAngle = Util::InterpolateEaseInEaseOut(charController->rollAngle, 0.0f, 0.25f, 2.0f);
			isParaglidingTiltAngleReset = charController->pitchAngle < 1e-5f && charController->rollAngle < 1e-5f;
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
		
		// Need to retrieve nodes from player's 3D.
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings || !coopActor->loadedData || !coopActor->loadedData->data3D)
		{
			return;
		}

		const auto& data3D = coopActor->loadedData->data3D;
		// Head and LH/RH magic nodes.
		auto headMagicNode = data3D->GetObjectByName(strings->npcHeadMagicNode);
		auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcLMagicNode));
		auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcRMagicNode));

		bool isCastingDual = false;
		bool isCastingLH = false;
		bool isCastingRH = false;
		bool isCastingQS = 
		{
			p->pam->IsPerforming(InputAction::kQuickSlotCast) &&
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) &&
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)->state.none(RE::MagicCaster::State::kNone)
		};
		coopActor->GetGraphVariableBool("IsCastingDual", isCastingDual);
		coopActor->GetGraphVariableBool("IsCastingLeft", isCastingLH);
		coopActor->GetGraphVariableBool("IsCastingRight", isCastingRH);

		// Make sure the magic casters are active, in addition to having the casting animation playing.
		isCastingLH &= 
		(
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand) &&
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand)->state.none(RE::MagicCaster::State::kNone)
		);
		isCastingLH |= isCastingDual || p->pam->usingLHStaff->value == 1.0f;
		isCastingRH &= 
		(
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand) &&
			coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand)->state.none(RE::MagicCaster::State::kNone)
		);
		isCastingRH |= isCastingDual || p->pam->usingRHStaff->value == 1.0f;

		// Set node's rotation to face the target position.
		auto directNodeAtTarget = 
		[this](const RE::MagicSystem::CastingSource&& a_source, const RE::TESObjectREFRPtr& a_targetPtr, const RE::NiPoint3& a_targetPos) 
		{
			// By default, choose aim pitch direction if there is no target.
			std::pair<float, float> pitchYawPair{ aimPitch, coopActor->data.angle.z };
			if (auto magicCaster = coopActor->GetMagicCaster(a_source); magicCaster)
			{
				auto magNodePtr = RE::NiPointer<RE::NiAVObject>(magicCaster->GetMagicNode());
				if (!magNodePtr) 
				{
					return;
				}

				RE::NiPoint3 forward{ 0.0f, 1.0f, 0.0f };	
				// No target and not facing crosshair position, so return default facing direction pitch/yaw.
				if (a_targetPtr || shouldFaceTarget)
				{
					float pitch = Util::GetPitchBetweenPositions(magNodePtr->world.translate, a_targetPos);
					float yaw = Util::GetYawBetweenPositions(magNodePtr->world.translate, a_targetPos);
					pitchYawPair.first = pitch;
					pitchYawPair.second = yaw;
				}

				Util::SetRotationMatrixPY(magNodePtr->world.rotate, pitchYawPair.first, pitchYawPair.second);
			}
		};

		// Not sure if changing the casting direction individually for each
		// casting node is possible, so for now, all that can be done is changing the player's aim angle
		// graph variables. Obvisously this means that dual casting will be less accurate
		// as both nodes will cast at the same angle, irrespective of their individual rotations to the targeted location.

		uint8_t activeNodesCount = 0;
		std::pair<float, float> avgPitchYawPair{ 0.0f, 0.0f };
		const RE::NiPoint3 forward{ 0.0f, 1.0f, 0.0f };

		// Initially target the crosshair world position.
		auto targetPos = p->tm->crosshairWorldPos;
		auto targetPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
		bool targetValidity = targetPtr && Util::IsValidRefrForTargeting(targetPtr.get());
		if (!targetValidity && Settings::vbUseAimCorrection[playerID])
		{
			// Set to aim correction target torso position.
			targetPtr = Util::GetRefrPtrFromHandle(p->tm->aimCorrectionTargetHandle); 
			targetValidity = targetPtr && Util::IsValidRefrForTargeting(targetPtr.get());
			if (targetValidity)
			{
				targetPos = Util::GetTorsoPosition(targetPtr->As<RE::Actor>());
			}
		}

		if (!targetValidity) 
		{
			targetPtr = nullptr;
		}

		// No target and not facing the crosshair position, so use default aim direction pitch/yaw.
		bool useAimPitchPos = !targetValidity && !shouldFaceTarget;
		if (useAimPitchPos)
		{
			avgPitchYawPair = { aimPitch, coopActor->data.angle.z };
			targetPos = aimPitchPos;
		}

		// Add up casting nodes' pitch and yaw angles to the target position.
		// Keep track of how many nodes are active.
		// Will average the angles afterward before setting the corresponding graph variables.
		auto accumulateAvgPitchYaw = 
		[this, &avgPitchYawPair, &activeNodesCount, &useAimPitchPos, &targetPos, strings](RE::MagicSystem::CastingSource&& a_source) 
		{
			if (auto actorMagicCaster = coopActor->magicCasters[!a_source]; actorMagicCaster && actorMagicCaster->magicNode)
			{
				if (!useAimPitchPos) 
				{
					avgPitchYawPair.first += Util::GetPitchBetweenPositions(actorMagicCaster->magicNode->world.translate, targetPos);
					avgPitchYawPair.second += Util::GetYawBetweenPositions(actorMagicCaster->magicNode->world.translate, targetPos);
				}

				glm::vec3 start = ToVec3(actorMagicCaster->magicNode->world.translate);
				glm::vec3 offset{ ToVec3(actorMagicCaster->magicNode->world.rotate * RE::NiPoint3(0.0f, 1.0f, 0.0f)) };
				DebugAPI::QueueArrow3D(start, start + offset * 15.0f, Settings::vuOverlayRGBAValues[p->playerID], 3.0f, 2.0f);

				++activeNodesCount;
				return;
			}
				
			bool valid3D = coopActor->loadedData && coopActor->loadedData->data3D;
			if (!valid3D)
			{
				return;
			}

			if (auto headMagNodePtr = RE::NiPointer<RE::NiAVObject>(coopActor->loadedData->data3D->GetObjectByName(strings->npcHeadMagicNode)); 
				headMagNodePtr && p->pam->IsPerforming(InputAction::kQuickSlotCast))
			{
				if (!useAimPitchPos)
				{
					// Just in case if the caster node is not available, even though the instant caster is casting our quick slot spell.
					avgPitchYawPair.first += Util::GetPitchBetweenPositions(headMagNodePtr->world.translate, targetPos);
					avgPitchYawPair.second += Util::GetYawBetweenPositions(headMagNodePtr->world.translate, targetPos);
				}

				++activeNodesCount;
				return;
			}
			else if (auto lookNodePtr = RE::NiPointer<RE::NiAVObject>(coopActor->loadedData->data3D->GetObjectByName(strings->npcLookNode)); 
				lookNodePtr && p->pam->IsPerforming(InputAction::kQuickSlotCast))
			{
				if (!useAimPitchPos)
				{
					// Just in case if the head magic node is not available, even though the instant caster is casting our quick slot spell.
					avgPitchYawPair.first += Util::GetPitchBetweenPositions(lookNodePtr->world.translate, targetPos);
					avgPitchYawPair.second += Util::GetYawBetweenPositions(lookNodePtr->world.translate, targetPos);
				}

				++activeNodesCount;
				return;
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
				directNodeAtTarget(RE::MagicSystem::CastingSource::kLeftHand, targetPtr, targetPos);
			}
		}

		hasSpell = p->em->HasRHSpellEquipped(); 
		hasStaff = p->em->HasRHStaffEquipped(); 
		if ((isCastingRH) && (hasSpell || hasStaff))
		{
			accumulateAvgPitchYaw(RE::MagicSystem::CastingSource::kRightHand);
			if (!p->isPlayer1 && hasStaff)
			{
				directNodeAtTarget(RE::MagicSystem::CastingSource::kRightHand, targetPtr, targetPos);
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
			// Divide by total active nodes to get average.
			if (!useAimPitchPos) 
			{
				avgPitchYawPair.first /= activeNodesCount;
				avgPitchYawPair.second /= activeNodesCount;
			}

			// Sign is opposite of player pitch/yaw angle.
			coopActor->SetGraphVariableFloat("AimPitchCurrent", -avgPitchYawPair.first);
			// Is delta angle from player's facing direction.
			coopActor->SetGraphVariableFloat
			(
				"AimHeadingCurrent",
				-Util::NormalizeAngToPi(avgPitchYawPair.second - coopActor->data.angle.z)
			);

			glm::vec3 start = ToVec3(headMagicNode->world.translate);
			glm::vec3 offset{ ToVec3(Util::RotationToDirectionVect(-avgPitchYawPair.first, Util::ConvertAngle(avgPitchYawPair.second))) };
			DebugAPI::QueueArrow3D(start, start + offset * 15.0f, 0xFFFFFFFF, 3.0f, 2.0f);
		}
		else
		{
			// No active nodes, so adjust aim pitch to our custom pitch.
			coopActor->SetGraphVariableFloat("AimPitchCurrent", -aimPitch);
		}
	}

	void MovementManager::SetDontMove(bool&& a_set)
	{
		// Prevent/allow mount or player (movement actor) from moving,
		// based on player movement speed and LS position (centered or not).

		auto mount = p->GetCurrentMount();
		if (a_set) 
		{
			// Set once to stop mount from moving.
			if (!dontMoveSet && mount && mount.get())
			{
				Util::NativeFunctions::SetDontMove(mount.get(), true);
			}

			// Send move stop animation when the player is not in a killmove
			// and has not been told to stop while still moving.
			if ((!coopActor->IsInKillMove()) && (!dontMoveSet || movementActor->DoGetMovementSpeed() != 0.0f))
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
			// Set once to allow mount to move.
			if (dontMoveSet && mount && mount.get()) 
			{
				Util::NativeFunctions::SetDontMove(mount.get(), false);
			}

			// Allow player to move if they were stopped.
			if (dontMoveSet)
			{
				Util::NativeFunctions::SetDontMove(coopActor.get(), false);
			}

			if ((!coopActor->IsInKillMove()) && ((shouldStartMoving) || (dontMoveSet && lsMoved)))
			{
				// If not in a killmove and signalled to start moving or stopped moving but the LS is moved,
				// send move start animation event.
				movementActor->NotifyAnimationGraph("IdleStopInstant");
				movementActor->NotifyAnimationGraph("moveStart");
			}
			else if (!lsMoved && !isDashDodging && !isRequestingDashDodge && 
				!isParagliding && !interactionPackageRunning && dontMoveSet)
			{
				// If the LS is centered and the player is not dash dodging and 
				// has been stopped, send the move stop animation event.
				// Player could still be rotating in place here.
				movementActor->NotifyAnimationGraph("moveStop");
			}

			dontMoveSet = false;
		}
	}

	void MovementManager::SetHeadTrackTarget()
	{
		// Set the context-dependent position which should be used as the player's headtracking target.

		bool onlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
		if (!onlyAlwaysOpen)
		{
			return;
		}

		// Don't set while in dialogue.
		if (auto currentProc = coopActor->currentProcess; currentProc && currentProc->high && !p->pam->isInDialogue)
		{
			if (coopActor->IsAttacking()) 
			{
				// Look at crosshair position/refr, if any, or aim pitch pos otherwise.
				if (shouldFaceTarget || Util::HandleIsValid(p->tm->crosshairRefrHandle)) 
				{
					currentProc->SetHeadtrackTarget(coopActor.get(), p->tm->crosshairWorldPos);
				}
			}
			else
			{
				if (p->pam->IsPerforming(InputAction::kActivate))
				{
					// Look at activation target.
					if (auto interactionTargetPtr = Util::GetRefrPtrFromHandle(p->tm->activationRefrHandle); 
						interactionTargetPtr && Util::IsValidRefrForTargeting(interactionTargetPtr.get()) && 
						interactionTargetPtr->parentCell->IsAttached())
					{
						if (interactionTargetPtr->Is(RE::FormType::ActorCharacter, RE::FormType::NPC))
						{
							// Look at NPC's eyes.
							auto targetEyePos = interactionTargetPtr->As<RE::Actor>()->GetLookingAtLocation();
							if (Util::PointIsOnScreen(targetEyePos))
							{
								currentProc->SetHeadtrackTarget(coopActor.get(), targetEyePos);
							}
						}
						else
						{
							// Look at object's center if selectable.
							// Looking at in-air projectiles sometimes causes a crash.
							if (Util::IsSelectableRefr(interactionTargetPtr.get()) &&
								interactionTargetPtr->GetBaseObject() && 
								!interactionTargetPtr->GetBaseObject()->Is(RE::FormType::Projectile))
							{
								auto targetCenter = Util::Get3DCenterPos(interactionTargetPtr.get());
								if (Util::PointIsOnScreen(targetCenter))
								{
									currentProc->SetHeadtrackTarget(coopActor.get(), targetCenter);
								}
							}
						}
					}
				}
				else
				{
					if (const auto targetActorPtr = Util::GetActorPtrFromHandle(p->tm->GetRangedTargetActor()); 
						targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get()))
					{
						// Look at targeted actor's eyes.
						auto targetEyePos = targetActorPtr->GetLookingAtLocation();
						if (Util::PointIsOnScreen(targetEyePos))
						{
							currentProc->SetHeadtrackTarget(coopActor.get(), targetEyePos);
						}
					}
					else if (const auto targetRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle); 
							 targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get()))
					{
						// Look at crosshair refr's center.
						auto targetCenter = Util::Get3DCenterPos(targetRefrPtr.get());
						if (Util::PointIsOnScreen(targetCenter))
						{
							currentProc->SetHeadtrackTarget(coopActor.get(), targetCenter);
						}
					}
				}
			}
		}
	}

	void MovementManager::SetPlayerOrientation()
	{
		// Set player's rotation and keep/clear movement offset.
		//===========
		// [Rotation]
		//===========
		// Normalize current facing angle first.
		movementActor->data.angle.z = Util::NormalizeAng0To2Pi(movementActor->data.angle.z);
		float rotMult = Settings::fBaseRotationMult * GetRotationMult();
		float playerTargetYaw = movementActor->data.angle.z;
		// Do not set rotation if not AI driven, a menu is open that stops movement,
		// the player is AI driven, in synced animation, mounting, in a killmove, or staggered.
		if (!menuStopsMovement && !isAnimDriven && !isSynced && !isMounting && 
			!coopActor->IsInKillMove() && coopActor->actorState1.knockState == RE::KNOCK_STATE_ENUM::kNormal)
		{
			// Target actor -- either crosshair-selected or downed player.
			RE::ActorPtr targetPtr = Util::GetActorPtrFromHandle(p->tm->GetRangedTargetActor());
			if (p->isRevivingPlayer && p->pam->downedPlayerTarget)
			{
				targetPtr = p->pam->downedPlayerTarget->coopActor;
			}

			auto targetLocation = aimPitchPos;
			if (!p->isRevivingPlayer)
			{
				if (targetPtr && targetPtr->GetHandle() == p->tm->aimCorrectionTargetHandle)
				{
					targetLocation = Util::GetTorsoPosition(targetPtr.get());
				}
				else
				{
					targetLocation = p->tm->crosshairWorldPos;
				}
			}
			else if (targetPtr)
			{
				targetLocation = Util::GetTorsoPosition(targetPtr.get());
			}

			bool isTKDodging = false;
			bool isTDMDodging = false;
			coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
			coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
			auto crosshairTargetPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
			bool crosshairTargetValidity = crosshairTargetPtr && Util::IsValidRefrForTargeting(crosshairTargetPtr.get());
			float yawToTarget = Util::DirectionToGameAngYaw(targetLocation - coopActor->data.location);

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
					shouldFaceTarget && !p->isRevivingPlayer && !isTKDodging && !
					isTDMDodging && !isDashDodging && !coopActor->IsOnMount() && !coopActor->IsSwimming() &&
					!p->pam->isSprinting && !coopActor->IsSprinting()
				) && 
				(
					!coopActor->IsSneaking() || !p->pam->AllInputsPressedForAction(InputAction::kSprint)
				) &&
				(
					(!crosshairTargetValidity) || 
					(!p->tm->rmm->IsGrabbed(p->tm->crosshairRefrHandle) && !p->tm->rmm->IsReleased(p->tm->crosshairRefrHandle))
				)
			};

			// Check if the player is using their weapons/magic.
			bool isUsingWeapMag = 
			{ 
				(p->pam->isAttacking || p->pam->isBlocking ||
				 p->pam->isBashing || p->pam->isInCastingAnim) 
			};

			if (!isUsingWeapMag && coopActor->IsWeaponDrawn())
			{
				const auto& combatGroup = glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at(ActionGroup::kCombat);
				for (auto actionIndex : combatGroup)
				{
					isUsingWeapMag |= p->pam->AllInputsPressedForAction(static_cast<InputAction>(actionIndex));
					if (isUsingWeapMag)
					{
						break;
					}
				}
			}

			// If not facing the target at all times,
			// the player will still temporarily face the selected target if performing certain actions.
			// 1. Not dodging.
			// 2. Not mounted.
			// 3. Reviving another player or using weapons/magic with a valid target while not sprinting.
			turnToTarget = 
			{ 
				(!faceTarget && !isTKDodging && !isTDMDodging && !isDashDodging && !coopActor->IsOnMount()) &&
				(
					(p->isRevivingPlayer) || 
					(
						isUsingWeapMag && !p->pam->IsPerforming(InputAction::kSprint) &&
						targetPtr && Util::IsValidRefrForTargeting(targetPtr.get())
					)
				) 
			};

			if (turnToTarget || faceTarget) 
			{
				// Slow down rotation quickly if too close to the target since the angle to the target
				// changes too rapidly and causes jittering when rotating to directly face the target.
				float xyDistToTarget = Util::GetXYDistance(movementActor->data.location, targetLocation);
				float minDistToSlowRotation = Util::GetXYDistance(coopActor->data.location, movementActor->data.location) * 0.1f;
				float radius = Settings::fTargetAttackSourceDistToSlowRotation;
				if (auto player3D = Util::GetRefr3D(coopActor.get()); player3D && player3D.get()) 
				{
					radius = player3D->worldBound.radius;
				}

				bool isAimingWithRangedWeapon = 
				{
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
				};
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

				// Turn so that weapon (notched arrow/loaded bolt) node's direction is pointed at the target.
				if (isAimingWithRangedWeapon && p->em->HasRangedWeapEquipped())
				{
					// NOTE: Interferes with the torso node rotation code,
					// leading to spinal curvature and an odd aiming orientation when nearing an aim pitch of += 90 degrees.
					// Commented out for now until solution is found.
					// Directly face the target position.
					yawToTarget = Util::DirectionToGameAngYaw(targetLocation - movementActor->data.location);
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

			// Tilt player up/down when swimming, based on their aim pitch.
			if (auto charController = coopActor->GetCharController(); charController && coopActor->IsSwimming())
			{
				charController->pitchAngle = aimPitch;
			}
		}

		//==================
		// [Movement Offset]
		//==================
		const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
		const auto& lsMag = lsData.normMag;

		// X axis is to the right of the player's facing direction.
		// Y axis is in the player's facing direction.
		float xPosOffset = 0.0f;
		float yPosOffset = 0.0f;

		// Interpolated yaw offset from the player's current yaw to the target yaw.
		// NOTE: Keeping a rotational offset with KeepOffsetFromActor() leads to jittering
		// when moving the character, so we just interp the player's data angle instead
		// to set the target rotation, and only use the movement offset for translational motion.
		float rawYawOffset = Util::InterpolateSmootherStep
		(
			0.0f,
			rotMult * Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z),
			playerRotInterpFactor
		);
		// Distance from the offset actor to start running to catch up.
		float catchUpRadius = 0.0f;
		// Distance from the offset actor within which to stop moving completely.
		float followRadius = 0.0f;

		// Player 1 is not AI driven if attempting discovery or motion driven flag is set.
		bool p1MotionDriven = 
		{
			(p->isPlayer1) && 
			(
				(attemptDiscovery) || 
				(glob.player1Actor->movementController && glob.player1Actor->movementController->unk1C5)
			)
		};
		if (p1MotionDriven) 
		{
			if (attemptDiscovery)
			{
				// Don't move while attempting discovery, 
				ClearKeepOffsetFromActor();
				SetDontMove(true);
			}
			else
			{
				// Clear the movement offset to allow the game to handle P1's movement while not AI driven.
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
				movementActor->data.angle.z += std::clamp
				(
					rawYawOffset,
					-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
					Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
				);
			}
		}
		else if (isAnimDriven || isSynced || isMounting || coopActor->IsInKillMove())
		{
			// Make sure the player can move and is not directed by this manager.
			SetDontMove(false);
			ClearKeepOffsetFromActor();
		}
		else if (menuStopsMovement || coopActor->IsInRagdollState() || coopActor->actorState1.knockState != RE::KNOCK_STATE_ENUM::kNormal)
		{
			SetDontMove(true);
			ClearKeepOffsetFromActor();
		}
		else if (interactionPackageRunning)
		{
			// Ensure the player can move first.
			SetDontMove(false);
			float xyDistToInteractionPos = Util::GetXYDistance(coopActor->data.location, interactionPackageEntryPos);
			// Stop when close enough for the package to kick in and start the activation animation.
			if (xyDistToInteractionPos < 0.5f * p->tm->GetMaxActivationDist())
			{
				ClearKeepOffsetFromActor();

				// Execute interaction package again if the player does not have any occupied furniture 
				// after setting the interaction package target and position earlier.
				if (auto interactionFurniture = coopActor->GetOccupiedFurniture(); !Util::HandleIsValid(interactionFurniture)) 
				{
					auto interactionPackage = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kSpecialInteraction];
					p->pam->SetCurrentPackage(interactionPackage);
					p->pam->EvaluatePackage();
				}
			}
			else
			{
				rawYawOffset = Util::InterpolateSmootherStep
				(
					0.0f,
					Util::NormalizeAngToPi
					(
						Util::GetYawBetweenPositions(movementActor->data.location, interactionPackageEntryPos) - 
						movementActor->data.angle.z
					),
					playerRotInterpFactor
				);
				movementActor->data.angle.z += std::clamp
				(
					rawYawOffset,
					-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
					Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
				);
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
		else if (isDashDodging)
		{
			// Ensure the player can move first.
			SetDontMove(false);
			if (isBackStepDodge)
			{
				// Move backward.
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(0.0f, -10.0f, 0.0f), 
					RE::NiPoint3(), 0.0f, 0.0f
				);
			}
			else if (framesSinceRequestingDashDodge <= 1)
			{
				// Set offset to keep throughout dodge during the first frame.
				float facingToHeadingAngDiff = 
				(
					Util::NormalizeAngToPi(movementOffsetParams[!MoveParams::kLSGameAng] - movementActor->data.angle.z)
				);
				xPosOffset = 10.0f * sinf(facingToHeadingAngDiff);
				yPosOffset = 10.0f * cosf(facingToHeadingAngDiff);
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(xPosOffset, yPosOffset, 0.0f), 
					RE::NiPoint3(), 0.0f, 0.0f
				);
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
			movementActor->data.angle.z += std::clamp
			(
				rawYawOffset,
				-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
				Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
			);
		}
		else if ((!isParagliding) && (shouldStopMoving || lsMag == 0.0f))
		{
			// SetDontMove() freezes actors in midair, 
			// so only set don't move flag when not paragliding, on the ground, 
			// and not trying to jump.
			RE::bhkCharacterController* charController
			{
				movementActor && movementActor.get() ? 
				movementActor->GetCharController() :
				nullptr 
			};
			bool canFreeze = shouldStopMoving && !p->pam->isAttacking && !isAirborneWhileJumping && !startJump;
			if (canFreeze && charController && charController->context.currentState == RE::hkpCharacterStateType::kInAir) 
			{
				canFreeze = false;
			}

			if (canFreeze)
			{
				ClearKeepOffsetFromActor();
				SetDontMove(true);
			}
			else if (faceTarget || turnToTarget) 
			{
				// Only Z rotation needed if stopped and turning to or facing a target.
				// No need to keep an offset when the player is already facing the target.
				// Player will walk in place very slowly if the offset isn't cleared.
				// Looks weird. Do not like.
				SetDontMove(false);
				ClearKeepOffsetFromActor();
			}
			else
			{
				// Already stopped and no need to rotate or handle movement.
				SetDontMove(false);
				ClearKeepOffsetFromActor();
			}

			// Manually rotate to avoid slow motion shifting when the Z rotation offset is small.
			movementActor->data.angle.z += std::clamp
			(
				rawYawOffset,
				-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
				Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
			);
		}
		else
		{
			float facingToHeadingAngDiff = 
			(
				Util::NormalizeAngToPi(movementOffsetParams[!MoveParams::kLSGameAng] - movementActor->data.angle.z)
			);
			xPosOffset = sinf(facingToHeadingAngDiff) * lsMag;
			yPosOffset = cosf(facingToHeadingAngDiff) * lsMag;
			// Ensure the player/mount can move first.
			SetDontMove(false);

			// NOTE: Keeping a rotational offset with KeepOffsetFromActor() leads to jittering
			// when moving the character, so we just interp the player's data angle instead
			// to set the target rotation.
			// NOTE: KeepOffsetFromActor() called on P1's mount does not work, so while P1 is mounted, they must not be AI driven.
			if (movementActor->IsAMount())
			{
				// Mounts cannot move sideways, so move forward and rotate.
				movementActor->data.angle.z += std::clamp
				(
					rawYawOffset,
					-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
					Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
				);
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
				if (p->isPlayer1) 
				{
					Util::NativeFunctions::ClearKeepOffsetFromActor(movementActor.get());
				}

				// Movement offset doesn't rotate the player when bAllowRotation is set to true,
				// so modify the middle high process rotation speed directly instead.
				// Do this when attacking or shield charging.
				bool allowRotation = false;
				coopActor->GetGraphVariableBool("bAllowRotation", allowRotation);
				if (auto midHighProc = movementActor->GetMiddleHighProcess(); 
					(allowRotation && p->pam->isAttacking) ||
					(midHighProc && p->pam->IsPerformingAllOf(InputAction::kSprint, InputAction::kBlock)))
				{
					float angMult = Settings::fBaseMTRotationMult;
					float zRot = 0.0f;
					float angDiff = Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z);
					if (angDiff < 0.0f)
					{
						zRot = max(angDiff * angMult, -0.5f * angMult * PI) * rotMult;
					}
					else if (angDiff > 0.0f)
					{
						zRot = max(angDiff * angMult, 0.5f * angMult * PI) * rotMult;
					}

					midHighProc->rotationSpeed.z = zRot;
				}
				else
				{
					movementActor->data.angle.z += std::clamp
					(
						rawYawOffset,
						-Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime,
						Settings::fBaseMTRotationMult * PI * *g_deltaTimeRealTime
					);
					KeepOffsetFromActor
					(
						movementActor->GetHandle(), 
						RE::NiPoint3(xPosOffset, yPosOffset, 0.0f), 
						RE::NiPoint3(), 
						0.0f, 
						0.0f
					);
				}
			}
		}

		playerPitch = movementActor->data.angle.x;
		playerYaw = movementActor->data.angle.z;
	}

	void MovementManager::SetShouldPerformLocationDiscovery()
	{
		// Temporary hacky workaround to allow for "automatic" location discovery,
		// since P1 is prevented from discovering locations while AI driven.
		// NOTE: Still does not work at times when the map marker's radius is 0.
		// Toggle off the co-op camera briefly to discover the location if
		// moving and stopping P1 does not trigger the discovery event.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		// Only run with P1, if valid and parent cell is available.
		if (!p->isPlayer1 || !p1 || !p1->parentCell)
		{
			return;
		}

		attemptDiscovery = false;  
		if (auto cell = p1->parentCell; cell)
		{
			// Check for discovery in exterior cells when the camera origin point nears the center point of the cell.
			if (auto extCellData = cell->GetCoordinates(); extCellData)
			{
				const auto& origin = glob.cam->camOriginPoint;
				std::optional<RE::NiPoint3> closestUndiscoveredMapMarkerPos = std::nullopt;
				std::optional<float> closestUndiscoveredMapMarkerRadius = std::nullopt;
				const float checkRadius = 4000.0f;
				Util::ForEachReferenceInRange(origin, checkRadius, true, [&, p1](RE::TESObjectREFR* a_refr) {
					if (!a_refr || !Util::HandleIsValid(a_refr->GetHandle()) || !a_refr->IsHandleValid())
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					// Base FID 0x10 is map marker.
					if (auto baseObj = a_refr->GetBaseObject(); baseObj && baseObj->formID == 0x10)
					{
						// If one is available, get marker discovery radius.
						float thisRadius = 0.0f;
						if (auto extraRadius = a_refr->extraList.GetByType<RE::ExtraRadius>(); extraRadius)
						{
							thisRadius = extraRadius->radius;
						}

						// Check if this marker is already visible on the map or not.
						bool visible = false;
						if (auto extraMarker = a_refr->extraList.GetByType<RE::ExtraMapMarker>(); extraMarker && extraMarker->mapData)
						{
							visible = extraMarker->mapData->flags.all(RE::MapMarkerData::Flag::kVisible);
						}

						// If not visible (discovered) and this map marker is within range and closer than another map marker, save its position and radius.
						if ((!visible) && 
							(!closestUndiscoveredMapMarkerPos.has_value() || 
							a_refr->data.location.GetDistance(origin) < closestUndiscoveredMapMarkerPos.value().GetDistance(origin)))
						{
							closestUndiscoveredMapMarkerPos = a_refr->data.location;
							if (thisRadius != 0.0f)
							{
								closestUndiscoveredMapMarkerRadius = thisRadius;
							}
						}
					}

					return RE::BSContainer::ForEachResult::kContinue;
				});

				// Check if undiscovered map marker is discoverable.
				if (closestUndiscoveredMapMarkerPos.has_value())
				{
					auto setDiscoveryAttempt = 
					[this, p1, cell, &closestUndiscoveredMapMarkerPos, &closestUndiscoveredMapMarkerRadius]() 
					{
						bool shouldAttemptDiscovery = false;
						// Must be within range.
						if (glob.cam->camOriginPoint.GetDistance(closestUndiscoveredMapMarkerPos.value()) <= closestUndiscoveredMapMarkerRadius)
						{
							shouldAttemptDiscovery = true;
						}

						return shouldAttemptDiscovery;
					};

					// Default marker radius check distance in the CK is 1000.
					if (!closestUndiscoveredMapMarkerRadius.has_value())
					{
						closestUndiscoveredMapMarkerRadius = 1000.0f;
					}

					bool prevInRange = inRangeOfUndiscoveredMarker;
					inRangeOfUndiscoveredMarker = setDiscoveryAttempt();
					// Just moved into range.
					if (!prevInRange && inRangeOfUndiscoveredMarker)
					{
						attemptDiscovery = true;
						framesSinceAttemptingDiscovery = 0;
					}
					else if (inRangeOfUndiscoveredMarker && framesSinceAttemptingDiscovery < 10)
					{
						// Discovery does not trigger instantly once P1 is no longer AI driven.
						// Better to wait more than 1 frame to increase the likelihood 
						// that the game flags the location as discovered.
						// 10 frame leeway per attempt.
						attemptDiscovery = true;
						framesSinceAttemptingDiscovery++;
					}
					else
					{
						attemptDiscovery = false;
					}
				}
				else
				{
					attemptDiscovery = inRangeOfUndiscoveredMarker = false;
					framesSinceAttemptingDiscovery = 0;
				}
			}
			else if (cell->IsInteriorCell())
			{
				// Check for discovery to remove local map fog of war when P1 is inactive 
				// (not moving either analog stick and not attacking/dodging).
				float movementSpeed = coopActor->DoGetMovementSpeed();
				bool isTKDodging = false;
				bool isTDMDodging = false;
				coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
				coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
				attemptDiscovery = 
				{ 
					!p->pam->isAttacking && !p->pam->isInCastingAnim && !shouldFaceTarget && 
					!isDashDodging && !isTKDodging && !isTDMDodging &&
					!rsMoved && !lsMoved && movementSpeed == 0.0f && 
					movementOffsetParams[!MoveParams::kDeltaLSAbsoluteAng] == 0.0f 
				};
			}
		}
	}

	void MovementManager::UpdateAimPitch()
	{
		// Adjust aim pitch (look at) position, which influences player spinal pitch.
		// NOTE: Not mathematically correct, since I haven't found a way to adjust torso nodes' world rotations
		// directly, and have to rely on modifications of the nodes' local rotation matrices.
		// Ideally at some point, I'd like to have the player face the target position,
		// while their spine naturally bends to allow their eye vector to point at the target.

		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle);
		bool crosshairRefrValidity = crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get());
		auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle(p->tm->aimCorrectionTargetHandle);
		bool aimCorrectionTargetValidity = aimCorrectionTargetPtr && Util::IsValidRefrForTargeting(aimCorrectionTargetPtr.get());
		bool isInAttackingAnim = (p->pam->isAttacking || p->pam->isInCastingAnim);
		// Can adjust pitch to face either a crosshair-targeted refr or an aim correction target.
		bool turningToCrosshairTarget = isInAttackingAnim && crosshairRefrValidity;
		bool usingAimCorrectionTarget = 
		(
			aimCorrectionTargetValidity && 
			!turningToCrosshairTarget && 
			Settings::vbUseAimCorrection[playerID] && 
			!shouldFaceTarget && 
			isInAttackingAnim
		);
		
		// Also haven't figured out how to properly account for different default spinal rotations when transformed,
		// so don't adjust aim pitch to face the target while transformed for now.
		// Can still manually adjust the transformed player's spinal rotation though.
		bool adjustAimPitchToFaceTarget = 
		{ 
			(!p->isTransformed) && 
			(!p->tm->rmm->isGrabbing) && 
			(shouldFaceTarget || turningToCrosshairTarget || usingAimCorrectionTarget) 
		};
		bool resetAimPitchIfNotAdjusted = false;

		auto targetPos = 
		(
			usingAimCorrectionTarget ? 
			Util::GetTorsoPosition(aimCorrectionTargetPtr.get()) : 
			p->tm->crosshairWorldPos
		);

		float defaultPitchToTarget = Util::GetPitchBetweenPositions(playerDefaultAttackSourcePos, targetPos);
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

		shouldAdjustAimPitch = p->pam->IsPerforming(InputAction::kAdjustAimPitch);
		// NOTE: Player cannot swim laterally if their spine is pitched too far upward, so clamp to a smaller angle (30 degrees instead of 90).
		if (shouldAdjustAimPitch) 
		{
			// Manually adjusting aim pitch with the RS.
			if (coopActor->IsSwimming()) 
			{
				aimPitch = std::clamp
				(
					aimPitch - rsData.normMag * rsY * Settings::vfMaxAimPitchAdjustmentRate[playerID] * *g_deltaTimeRealTime, 
					-PI / 6.0f, 
					PI / 2.0f
				);
			}
			else
			{
				aimPitch = std::clamp
				(
					aimPitch - rsData.normMag * rsY * Settings::vfMaxAimPitchAdjustmentRate[playerID] * *g_deltaTimeRealTime, 
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
			// Reset to default pitch when not targeting anything and aim pitch was not manually adjusted.
			resetAimPitchIfNotAdjusted = !aimPitchManuallyAdjusted;
		}

		// Prevent cached spinal node local rotations from being restored when aim pitch is reset
		// or when no manual corrections were made.
		bool shouldResetAimPitch = shouldResetAimAndBody || resetAimPitchIfNotAdjusted;
		if (shouldResetAimPitch)
		{
			// Clear out all previously set node target rotations, preventing blending in to the cleared values.
			if (shouldResetAimAndBody) 
			{
				std::unique_lock<std::mutex> lock(p->mm->nom->rotationDataMutex);
				nom->ClearCustomRotations();
			}

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

			// Indicate that aim pitch was reset and is no longer manually adjusted.
			aimPitchManuallyAdjusted = aimPitchAdjusted = false;
			shouldResetAimAndBody = false;
		}

		// Set the aim pitch position after modifying the aim pitch if the player is not transformed.
		if (!p->isTransforming) 
		{
			float radialDist = playerScaledHeight / 2.0f;
			auto eyePos = Util::GetEyePosition(coopActor.get());
			aimPitchPos = RE::NiPoint3
			(
				eyePos.x + radialDist * cosf(Util::ConvertAngle(coopActor->GetHeading(false))) * cosf(aimPitch),
				eyePos.y + radialDist * sinf(Util::ConvertAngle(coopActor->GetHeading(false))) * cosf(aimPitch),
				eyePos.z - radialDist * sinf(aimPitch)
			);

			// Modifications to P1's X angle here while aiming with a bow/crossbow
			// double up the effects of our custom torso rotation, so set to zero here.
			coopActor->data.angle.x = 0.0f;
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
		auto& attackSourcePos = a_setDefaultDirAndPos ? playerDefaultAttackSourcePos : playerAttackSourcePos;
		auto& attackSourceDir = a_setDefaultDirAndPos ? playerDefaultAttackSourceDir : playerAttackSourceDir;

		// Default attack source position is at eye level and in the facing direction of the player.
		attackSourcePos = coopActor->GetLookingAtLocation();
		attackSourceDir = Util::RotationToDirectionVect
		(
			-coopActor->data.angle.x, 
			Util::ConvertAngle(coopActor->data.angle.z)
		);

		// Adjust attack source position and rotation based on what attack action the player is performing.
		// Casting source position and direction.
		if ((p->pam->isCastingLH || p->pam->isCastingRH || p->pam->isInCastingAnim))
		{
			if (p->pam->isCastingLH && p->pam->isCastingRH)
			{
				// Best approximation to be made here is to rotate about the point between the two casting nodes
				// when casting with two hands, as there is no single source point for the released projectiles.
				// Also approximate attack direction as the averaged direction of the two caster nodes.
				auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcLMagicNode));
				auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcRMagicNode));
				if (a_setDefaultDirAndPos)
				{
					if (lMagNodePtr && lMagNodePtr.get() &&
						rMagNodePtr && rMagNodePtr.get() &&
						nom->defaultNodeWorldTransformsMap.contains(lMagNodePtr) && 
						nom->defaultNodeWorldTransformsMap.contains(rMagNodePtr))
					{
						const auto& lhPos = nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).translate;
						const auto& rhPos = nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).translate;
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
						attackSourcePos = (lMagNodePtr->world.translate + rMagNodePtr->world.translate) / 2.0f;
						attackSourceDir = (lMagNodePtr->world.rotate * forward + rMagNodePtr->world.rotate * forward) / 2.0f;
					}
				}
			}
			else if (p->pam->isCastingLH)
			{
				auto lMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcLMagicNode));
				if (a_setDefaultDirAndPos)
				{
					if (lMagNodePtr && lMagNodePtr.get() && nom->defaultNodeWorldTransformsMap.contains(lMagNodePtr))
					{
						attackSourcePos = nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).translate;
						attackSourceDir = nom->defaultNodeWorldTransformsMap.at(lMagNodePtr).rotate * forward;
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
				auto rMagNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npcRMagicNode));
				if (a_setDefaultDirAndPos)
				{
					if (rMagNodePtr && rMagNodePtr.get() && nom->defaultNodeWorldTransformsMap.contains(rMagNodePtr))
					{
						attackSourcePos = nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).translate;
						attackSourceDir = nom->defaultNodeWorldTransformsMap.at(rMagNodePtr).rotate * forward;
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
		}
		else
		{
			// Handle ranged attack source position and rotation.
			if (auto weaponNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->weapon)); 
				weaponNodePtr && weaponNodePtr.get()) 
			{
				bool ammoDrawnOrLater = 
				{
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing
				};
				bool nockingAmmo = 
				{
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
					coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw
				};
				if (nockingAmmo && p->em->HasBowEquipped())
				{
					// Since the arrow rotates away from the player
					// while it is being removed from the quiver and nocked,
					// using the direction from the draw hand to the bow hand is more stable
					// and less jittery than using the arrow/weapon node's rotation.
					// The arrow node rotation will not be stable until it is fully drawn.
					auto lhNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName("NPC L Hand [LHnd]"sv));
					auto rhNodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName("NPC R Hand [RHnd]"sv));
					if (a_setDefaultDirAndPos)
					{
						if (weaponNodePtr && weaponNodePtr.get() && nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourcePos = nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).translate;
						}

						if (lhNodePtr && lhNodePtr.get() &&
							rhNodePtr && rhNodePtr.get() &&
							nom->defaultNodeWorldTransformsMap.contains(lhNodePtr) &&
							nom->defaultNodeWorldTransformsMap.contains(rhNodePtr) && 
							nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							const auto& lhPos = nom->defaultNodeWorldTransformsMap.at(lhNodePtr).translate;
							const auto& rhPos = nom->defaultNodeWorldTransformsMap.at(rhNodePtr).translate;
							const auto& weapRot = nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).rotate;
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
								-Util::DirectionToGameAngPitch(weaponNodePtr->world.rotate * forward),
								Util::ConvertAngle(Util::DirectionToGameAngYaw(lhNodePtr->world.translate - rhNodePtr->world.translate))
							);
						}

						attackSourcePos = weaponNodePtr->world.translate;
					}
				}
				else if (nockingAmmo || ammoDrawnOrLater)
				{
					if (a_setDefaultDirAndPos)
					{
						if (nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourcePos = nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).translate;
						}

						if (nom->defaultNodeWorldTransformsMap.contains(weaponNodePtr))
						{
							attackSourceDir = nom->defaultNodeWorldTransformsMap.at(weaponNodePtr).rotate * forward;
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

		// REMOVE when done debugging node directions.
		/*glm::vec3 startVec = ToVec3(attackSourcePos);
		glm::vec3 endVec1 = startVec + ToVec3(attackSourceDir) * 20.0f;
		DebugAPI::QueuePoint3D(startVec, a_setDefaultDirAndPos ? 0xFFFFFFFF : Settings::vuOverlayRGBAValues[playerID], 5.0f);
		DebugAPI::QueueArrow3D(startVec, endVec1, a_setDefaultDirAndPos ? 0xFFFFFFFF : Settings::vuOverlayRGBAValues[playerID], 3.0f, 2.0f);*/
	}

	void MovementManager::UpdateMovementParameters()
	{
		// Update player movement parameters derived from controller analog stick movement
		// in both in-game coordinates and absolute coordinates.

		const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
		const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
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
		if (glob.cam->IsRunning() && playerCam->currentState && playerCam->currentState.get() &&
			playerCam->currentState->id == RE::CameraState::kThirdPerson) 
		{
			camYaw = glob.cam->GetCurrentYaw();
		}
		else if (auto niCam = Util::GetNiCamera(); niCam) 
		{
			// Player cam's yaw does not always correspond to the actual camera forward direction angle in
			// certain camera state, such as the bleedout camera state, so get that info from the NiCamera.
			RE::NiPoint3 niEulerAngles = Util::GetEulerAnglesFromRotMatrix(niCam->world.rotate);
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

		// Obtain unit circle angle for left stick orientation.
		if (lsX == 0.0f && lsY == 0.0f) 
		{
			// Previous, no change, since the LS is centered.
			lsAbsAng = movementOffsetParams[!MoveParams::kLSAbsoluteAng];
		}
		else
		{
			float realLSAng = atan2f(lsY, lsX);
			lsAbsAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(realLSAng));
		}

		if (rsX == 0.0f && rsY == 0.0f) 
		{
			// Previous, no change, since the RS is centered.
			rsAbsAng = movementOffsetParams[!MoveParams::kRSAbsoluteAng];
		}
		else
		{
			float realRSAng = atan2f(rsY, rsX);
			rsAbsAng = Util::ConvertAngle(Util::NormalizeAng0To2Pi(realRSAng));
		}

		// Yaw angles for both analog sticks in the world's coordinate space (relative to the camera).
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

		//if (lsX != 0.0f || lsY != 0.0f)
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
		if (coopActor->IsSprinting() || (coopActor->IsSwimming() && p->pam->IsPerforming(InputAction::kSprint))) 
		{
			// Co-op companion mounts accelerate more slowly for some reason. Scale up speedmult to better match player 1's mount speedmult.
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

		prevLSAngAtMaxDisp = lsAngAtMaxDisp;
		prevRSAngAtMaxDisp = rsAngAtMaxDisp;
		lsAngAtMaxDisp = (lsMag == 1.0f) ? lsAng : lsAngAtMaxDisp;
		rsAngAtMaxDisp = (rsMag == 1.0f) ? rsAng : rsAngAtMaxDisp;

		// All angles given by in-game coordinates before adding to params list.
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

		// Check if P1 should have their AI driven flag cleared, which re-enables location discovery.
		SetShouldPerformLocationDiscovery();

		// Update analog stick state and menu movement flag.
		menuStopsMovement = Util::OpenMenuStopsMovement();
		bool prevLSMoved = lsMoved;
		lsMoved = movementOffsetParams[!MoveParams::kLSXComp] != 0.0f || movementOffsetParams[!MoveParams::kLSYComp] != 0.0f;
		rsMoved = movementOffsetParams[!MoveParams::kRSXComp] != 0.0f || movementOffsetParams[!MoveParams::kRSYComp] != 0.0f;

		if (prevLSMoved && !lsMoved) 
		{
			p->lastMovementStopReqTP = SteadyClock::now();
		}
		else if (!prevLSMoved && lsMoved)
		{
			p->lastMovementStartReqTP = SteadyClock::now();
		}

		// Update current mount.
		if (!coopActor->IsOnMount() && Util::HandleIsValid(p->currentMountHandle))
		{
			// Clear mount if not mounted anymore.
			// Remove movement offset for the current mount so that it does not
			// continue to move when the player moves their character.
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

		// Set movement actor (any mount if player is mounted, player actor otherwise).
		auto mount = p->GetCurrentMount();
		movementActor = mount && mount.get() ? mount : coopActor;
		const float movementSpeed = movementActor->DoGetMovementSpeed();

		// Ensure animation sneak state syncs up with actor sneak state.
		if (!coopActor->IsOnMount() && !coopActor->IsSwimming() && !coopActor->IsFlying() && !isRequestingDashDodge && !isDashDodging)
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
		// to an improper ragdoll reset position glitch where the hit actor is teleported to their last ragdoll position
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
		else if (!isRagdolled && playerRagdollTriggered && coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal)
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
		if (auto knockState = coopActor->GetKnockState(); knockState == RE::KNOCK_STATE_ENUM::kGetUp)
		{
			isGettingUp = true;
		}
		else
		{
			// Prevent the player from shooting forward in their facing direction after ragdolling.
			bool finishedGettingUp = wasGettingUp && knockState == RE::KNOCK_STATE_ENUM::kNormal;
			// Stop instantly to prevent the player from slowly coming to a halt 
			// from residual momentum when turning to face a target while attacking/bashing/blocking/casting.
			bool turnToFaceTargetWhileStopped = 
			{
				(!lsMoved && movementSpeed != 0.0f) &&
				(
					p->pam->isAttacking || p->pam->isBlocking ||
					p->pam->isBashing || p->pam->isInCastingAnim
				)
			};
			if (finishedGettingUp || turnToFaceTargetWhileStopped)
			{
				p->lastGetupTP = SteadyClock::now();
				shouldCurtailMomentum = true;
			}

			isGettingUp = false;
		}

		// Freeze the player in place and wait until their reported movement speed is 0.
		shouldCurtailMomentum &= movementSpeed > 0.0f;

		if (auto charController = movementActor->GetCharController(); charController)
		{
			// Is the player.
			if (!movementActor->IsAMount())
			{
				// Jump up and get down.
				if (startJump || isAirborneWhileJumping)
				{
					PerformJump();
				}
				else if (charController->context.currentState == RE::hkpCharacterStateType::kOnGround &&
						 (coopActor->actorState1.walking || coopActor->actorState1.running))
				{
					// Prevent player 1 from mysteriously dying after moving down a slope and jumping at the bottom.
					// The proxy controller fails to reset the fall state and fall damage is applied to the jump
					// as if the player had jumped all the way down from the top of the slope.
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

				// Perform dash dodge.
				if (isRequestingDashDodge || isDashDodging) 
				{
					PerformDashDodge();
				}
				
				// Perform magical-paraglide alternative which looks like trash.
				if (isParagliding || shouldParaglide || !isParaglidingTiltAngleReset) 
				{
					PerformMagicalParaglide();
				}

				// Check if the player has started/stopped swimming and play the appropriate animation.
				// Keep actor state and animation state in sync.
				if (!isSwimming && coopActor->IsSwimming())
				{
					if (p->isPlayer1)
					{
						coopActor->NotifyAnimationGraph("SwimStart");
					}

					shouldResetAimAndBody = true;
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
			}
			else
			{
				// Make sure face target is unset when mounted since mounts cannot strafe and move sideways.
				// Facing the target means the mount can only move towards the target or back (moonwalk) away from it.
				if (shouldFaceTarget) 
				{
					shouldFaceTarget = false;
					// Update player crosshair face target state.
					p->tm->crosshairRotationData->SetTimeSinceUpdate(0.0f);
					p->tm->crosshairRotationData->ShiftEndpoints(0.0f);
				}

				// Mounted jump is animation event driven.
				if (startJump)
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

					startJump = false;
				}
			}

			// Player 1 only.
			// Toggle AI driven as needed.
			if (p->isPlayer1 && glob.player1Actor && glob.player1Actor.get())
			{
				bool isActivating = 
				{
					p->pam->IsPerformingOneOf(InputAction::kActivate, InputAction::kActivateAllOfType) ||
					p->pam->AllInputsPressedForAtLeastOneAction(InputAction::kActivate, InputAction::kActivateAllOfType)
				};
				bool isMounted = coopActor->IsOnMount();
				bool reqOrIsParagliding =
				{ 
					(isParagliding) || 
					(
						ALYSLC::SkyrimsParagliderCompat::g_p1HasParaglider && isActivating && 
						glob.player1Actor->GetCharController() && 
						glob.player1Actor->GetCharController()->context.currentState == RE::hkpCharacterStateType::kInAir
					) 
				};

				// NOTE: If P1 is AI driven when equipping certain items,
				// the game will sometimes override the equip and equip another 
				// item that was previously equipped instead.
				// So while P1 is equipping or unequipping, we have to toggle
				// AI driven off, and if TDM is not installed,
				// adjust thumbstick inputs to rebase them relative to the co-op camera 
				// and rotate the player to face their movement direction.
				bool isEquipping = false;
				bool isUnequipping = false;
				coopActor->GetGraphVariableBool("IsEquipping", isEquipping);
				coopActor->GetGraphVariableBool("IsUnequipping", isUnequipping);

				// Credits to ersh1 for finding out the movement handler's motion driven flag:
				// https://github.com/ersh1/TrueDirectionalMovement/blob/master/src/DirectionalMovementHandler.cpp#L307
				bool isAIDriven = glob.player1Actor->movementController && !glob.player1Actor->movementController->unk1C5;
				// NOTE: Also when an event causes player 1 to ragdoll, they will not exit the
				// ragdolling state and attempt to get up unless AI driven is unset.
				bool shouldRemoveAIDriven =	
				{
					p->pam->sendingP1MotionDrivenEvents ||
					isMounted ||
					isRagdolled ||
					reqOrIsParagliding ||
					/*isEquipping ||
					isUnequipping ||*/
					menuStopsMovement ||
					attemptDiscovery
				};

				if (isAIDriven && shouldRemoveAIDriven)
				{
					ALYSLC::Log("[MM] UpdateMovementState: {} is anim driven: {}, mounted: {}, ragdolled: {}, synced: {}, paragliding: {}. Sending motion driven events: {}, menu stops movement: {}, attempt discovery: {}. REMOVE AI driven.",
						glob.player1Actor->GetName(), isAnimDriven, isMounted, isRagdolled, isSynced, reqOrIsParagliding, p->pam->sendingP1MotionDrivenEvents, menuStopsMovement, attemptDiscovery);
					bool changed = Util::SetPlayerAIDriven(false);
					if (changed)
					{
						ALYSLC::Log("[MM] UpdateMovementState: {} AI driven state changed to false.", coopActor->GetName());
					}
				}
				else if (!isAIDriven && !shouldRemoveAIDriven)
				{
					ALYSLC::Log("[MM] UpdateMovementState: {} is anim driven: {}, mounted: {}, ragdolled: {}, synced: {}, paragliding: {}. Sending motion driven events: {}, menu stops movement: {}, attempt discovery: {}. SET AI driven.",
						glob.player1Actor->GetName(), isAnimDriven, isMounted, isRagdolled, isSynced, reqOrIsParagliding, p->pam->sendingP1MotionDrivenEvents, menuStopsMovement, attemptDiscovery);

					bool changed = Util::SetPlayerAIDriven(true);
					if (changed)
					{
						ALYSLC::Log("[MM] UpdateMovementState: {} AI driven state changed to true.", coopActor->GetName());
					}
				}
			}

			// Set start or stop movement flags.
			bool isMoving = 
			{
				(movementSpeed > 0.0f) &&
				(
					 movementActor->actorState1.movingBack | movementActor->actorState1.movingForward |
					 movementActor->actorState1.movingLeft | movementActor->actorState1.movingRight |
					 movementActor->actorState1.running | movementActor->actorState1.sprinting |
					 movementActor->actorState1.swimming
				) != 0
			};
			bool isTKDodging = false;
			bool isTDMDodging = false;
			coopActor->GetGraphVariableBool("bAnimationDriven", isAnimDriven);
			coopActor->GetGraphVariableBool("bIsSynced", isSynced);
			coopActor->GetGraphVariableBool("bIsDodging", isTKDodging);
			coopActor->GetGraphVariableBool("TDM_Dodge", isTDMDodging);
			auto interactionPackage = glob.coopPackages[!PackageIndex::kTotal * controllerID + !PackageIndex::kSpecialInteraction];
			interactionPackageRunning = p->pam->GetCurrentPackage() == interactionPackage;

			// Stop moving if currently moving and not dash dodging, 
			// and if the LS is centered, a menu stops movement, the player is reviving a buddy, or if attempting discovery.
			shouldStopMoving = 
			{
				((isMoving && !isDashDodging && !isRequestingDashDodge && !isTKDodging && !isTDMDodging) && 
				(!lsMoved || menuStopsMovement || p->isRevivingPlayer || attemptDiscovery))
			};

			// Start movement if the LS is displaced and if the player is not moving or dodging,
			// not running an interaction package, not prevented from moving by a menu, 
			// not reviving another player, and not animation driven.
			shouldStartMoving = 
			{
				lsMoved && !isMoving && !isDashDodging && !isRequestingDashDodge && 
				!interactionPackageRunning && !menuStopsMovement && !p->isRevivingPlayer
			};
		}
	}

	void NodeOrientationManager::ApplyCustomNodeRotation(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_nodePtr)
	{
		// If the node's rotation is handled by our node orientation manager,
		// apply our custom rotation to the given node.

		// Check if a supported node first.
		bool isLeftArmNode = GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODE.contains(a_nodePtr->name);
		bool isRightArmNode = GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODE.contains(a_nodePtr->name);
		bool isTorsoNode = GlobalCoopData::ADJUSTABLE_TORSO_NODE.contains(a_nodePtr->name);
		if (!isLeftArmNode && !isRightArmNode && !isTorsoNode)
		{
			return;
		}

		if (nodeRotationDataMap.contains(a_nodePtr))
		{
			const auto& data = nodeRotationDataMap[a_nodePtr];

			// Set default rotation.
			data->defaultRotation = a_nodePtr->local.rotate;

			// Set new local rotations before the UpdateDownwardPass() call,
			// so that it can use our modified local rotations to set the nodes' new world rotations.
			if (isTorsoNode || Settings::bRotateArmsWhenSheathed)
			{
				a_nodePtr->local.rotate = data->currentRotation;
			}
		}
	}

	void NodeOrientationManager::DisplayAllNodeRotations(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Draw X, Y, and Z world axes for all/supported player nodes.
		
		// REMOVE when done debugging.
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

		if (auto npc3DPtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npc)); npc3DPtr && npc3DPtr.get())
		{
			if (auto nodePtr = RE::NiPointer<RE::NiNode>(npc3DPtr->AsNode()); nodePtr && nodePtr.get())
			{
				DisplayAllNodeRotations(nodePtr);
			}
		}
	}

	void NodeOrientationManager::DisplayAllNodeRotations(const RE::NiPointer<RE::NiNode>& a_nodePtr)
	{
		// Recursively save adjustable nodes' world rotations by walking the player's node tree from the given node.
		// The parent world transform is modified to the current node's world transform before traversing its children.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

		// Cannot set the saved world rotation directly
		// to this node's reported world rotation, since it still
		// contains our possibly-modified world rotation from the previous frame.
		// However, if called at the right time (during the havok physics pre-step),
		// the node's local rotation will be updated, and we can then calculate
		// the game's intended world rotation for this node via the parent node's world transform.

		const auto& newWorldRot = a_nodePtr->world.rotate;
		auto worldXAxis = RE::NiPoint3(newWorldRot * right);
		auto worldYAxis = RE::NiPoint3(newWorldRot * forward);
		auto worldZAxis = RE::NiPoint3(newWorldRot * up);
		glm::vec3 start{ a_nodePtr->world.translate.x, a_nodePtr->world.translate.y, a_nodePtr->world.translate.z };
		glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 5.0f };
		glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 5.0f };
		glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 5.0f };
		DebugAPI::QueueArrow3D(start, endX, 0xFF000088, 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endY, 0x00FF0088, 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endZ, 0x0000FF88, 3.0f, 2.0f);

		for (const auto child : a_nodePtr->children)
		{
			if (child && child.get() && child->AsNode())
			{
				auto childNode = RE::NiPointer<RE::NiNode>(child->AsNode());
				DisplayAllNodeRotations(childNode);
			}
		}
	}

	void NodeOrientationManager::InstantlyResetAllNodeData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Reset all node data.

		defaultNodeLocalTransformsMap.clear();
		defaultNodeWorldTransformsMap.clear();
		nodeRotationDataMap.clear();

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

		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODE) 
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get()) 
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}

		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODE)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get())
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}

		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_TORSO_NODE)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get())
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}
	}

	void NodeOrientationManager::InstantlyResetArmNodeData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Only reset arm node data.
		
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

		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODE)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get())
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}

		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODE)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get())
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}
	}

	void NodeOrientationManager::InstantlyResetTorsoNodeData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Only reset torso node data.
		
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

		RE::NiPointer<RE::NiAVObject> nodePtr{ nullptr };
		for (const auto& nodeName : GlobalCoopData::ADJUSTABLE_TORSO_NODE)
		{
			nodePtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName));
			if (nodePtr && nodePtr.get())
			{
				nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
			}
		}
	}

	bool NodeOrientationManager::NodeWasAdjusted(const RE::NiPointer<RE::NiAVObject>& a_nodePtr)
	{
		// Returns true if a custom cached rotation was set for the given node.
		// Can check for the node name hash in either the set of adjustable arm nodes or torso nodes.

		if (nodeRotationDataMap.contains(a_nodePtr)) 
		{
			if (const auto& data = nodeRotationDataMap.at(a_nodePtr); data && data.get()) 
			{
				return data->rotationModified;
			}
		}

		return false;
	}

	void NodeOrientationManager::RestoreOriginalNodeLocalTransforms(const std::shared_ptr<CoopPlayer>& a_p)
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

		// Restore saved node local transforms previously set by the game before our modifications.
		for (const auto& [nodePtr, localTrans] : defaultNodeLocalTransformsMap)
		{
			if (!nodePtr || !nodePtr.get())
			{
				continue;
			}

			nodePtr->local = localTrans;
		}
	}

	void NodeOrientationManager::SavePlayerNodeWorldTransforms(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// NOTE: At this point, the game SHOULD have updated all nodes' local transforms (unless a havok impulse was applied),
		// but has not modified the world transforms from the previous frame (which we may have modified),
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

		if (auto npc3DPtr = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(strings->npc)); npc3DPtr && npc3DPtr.get())
		{
			if (auto nodePtr = RE::NiPointer<RE::NiNode>(npc3DPtr->AsNode()); nodePtr && nodePtr.get())
			{
				auto parentWorldTransform = nodePtr->parent ? nodePtr->parent->world : RE::NiTransform();
				SavePlayerNodeWorldTransforms(a_p, nodePtr, parentWorldTransform);
			}
		}
	}

	void NodeOrientationManager::SavePlayerNodeWorldTransforms(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiNode>& a_nodePtr, const RE::NiTransform& a_parentWorldTransform)
	{
		// Recursively save adjustable nodes' world rotations by walking the player's node tree from the given node.
		// The parent world transform is modified to the current node's world transform before traversing its children.

		if (!a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

		// Cannot set the saved world rotation directly
		// to this node's reported world rotation, since it still
		// contains our possibly-modified world rotation from the previous frame.
		// However, if called at the right time (during the havok physics pre-step),
		// the node's local rotation will be updated, and we can then calculate
		// the game's intended world rotation for this node via the parent node's world transform.

		auto newParentTransform = (a_parentWorldTransform * a_nodePtr->local);
		// Save default world transform.
		defaultNodeWorldTransformsMap.insert_or_assign(a_nodePtr, newParentTransform);

		for (const auto child : a_nodePtr->children)
		{
			if (child && child.get() && child->AsNode())
			{
				auto childNode = RE::NiPointer<RE::NiNode>(child->AsNode());
				SavePlayerNodeWorldTransforms(a_p, childNode, newParentTransform);
			}
		}
	}

	void NodeOrientationManager::SetBlendStatus(const RE::NiPointer<RE::NiAVObject>& a_nodePtr, NodeRotationBlendStatus&& a_newStatus)
	{
		// Update the blend status for the given node.

		if (nodeRotationDataMap.contains(a_nodePtr)) 
		{
			auto& data = nodeRotationDataMap.at(a_nodePtr); 
			if (!data || !data.get())
			{
				return;
			}

			// REMOVE when done debugging.
			//ALYSLC::Log("[MM] SetBlendStatus: {} -> {}.", data->blendStatus, a_newStatus);
			
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
	}

	void NodeOrientationManager::UpdateArmNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_forearmNodePtr, const RE::NiPointer<RE::NiAVObject>& a_handNodePtr, bool a_rightArm)
	{
		// Update arm nodes' blend states and target rotations.
		// Modification method: local rotation directly obtained from pitch/roll/yaw
		// inputs from the player's RS movement.

		// Invalid nodes or not accounted for in the rotation data map.
		if (!a_forearmNodePtr || !a_forearmNodePtr.get() || !a_handNodePtr || !a_handNodePtr.get())
		{
			return;
		}

		// Ensure both nodes are accounted for in the rotation data map.
		if (!nodeRotationDataMap.contains(a_forearmNodePtr))
		{
			nodeRotationDataMap.insert_or_assign(a_forearmNodePtr, std::make_unique<NodeRotationData>());
		}

		if (!nodeRotationDataMap.contains(a_handNodePtr))
		{
			nodeRotationDataMap.insert_or_assign(a_handNodePtr, std::make_unique<NodeRotationData>());
		}

		const auto& forearmData = nodeRotationDataMap[a_forearmNodePtr];
		const auto& handData = nodeRotationDataMap[a_handNodePtr];
		// Set node target rotations for forearm and hand.
		bool forearmRotHasBeenSet = forearmData->rotationModified;
		bool handRotHasBeenSet = handData->rotationModified;
		const float oldForearmYaw = forearmRotHasBeenSet ? forearmData->rotationInput[0] : 0.0f;
		const float oldForearmPitch = forearmRotHasBeenSet ? forearmData->rotationInput[1] : 0.0f;
		const float oldForearmRoll = forearmRotHasBeenSet ? forearmData->rotationInput[2] : 0.0f;
		const float oldHandYaw = handRotHasBeenSet ? handData->rotationInput[0] : a_rightArm ? -(0.5f * PI) : (0.5f * PI);
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
		bool onlyRotatingShoulder = rotatingShoulder && !rotatingForearm && !rotatingHand;

		// Set rotation state flags first.
		forearmData->prevInterrupted = forearmData->interrupted;
		forearmData->prevRotationModified = forearmData->rotationModified;
		handData->prevInterrupted = handData->interrupted;
		handData->prevRotationModified = handData->rotationModified;
		forearmData->interrupted = handData->interrupted = 
		{
			a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			a_p->IsAwaitingRefresh() ||
			a_p->coopActor->IsWeaponDrawn() ||
			a_p->coopActor->actorState2.recoil ||
			a_p->coopActor->actorState2.staggered ||
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
			// Set defaults if the forearm/hand nodes' rotations were not previously modified.
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
				targetForearmPitch = Util::InterpolateSmootherStep(oldForearmPitch, targetForearmPitch, min(1.0f, interpFactor));
			}

			if (oldForearmRoll != targetForearmRoll)
			{
				targetForearmRoll = Util::InterpolateSmootherStep(oldForearmRoll, targetForearmRoll, min(1.0f, interpFactor));
			}

			if (oldForearmYaw != targetForearmYaw)
			{
				targetForearmYaw = Util::InterpolateSmootherStep(oldForearmYaw, targetForearmYaw, min(1.0f, interpFactor));
			}

			forearmData->rotationInput = std::array<float, 3>({ targetForearmYaw, targetForearmPitch, targetForearmRoll });
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
			forearmData->targetRotation = forearmData->defaultRotation; //a_forearmNodePtr->local.rotate;
		}
		
		if (handData->rotationModified) 
		{
			if (oldHandPitch != targetHandPitch)
			{
				targetHandPitch = Util::InterpolateSmootherStep(oldHandPitch, targetHandPitch, min(1.0f, interpFactor));
			}

			if (oldHandRoll != targetHandRoll)
			{
				targetHandRoll = Util::InterpolateSmootherStep(oldHandRoll, targetHandRoll, min(1.0f, interpFactor));
			}

			if (oldHandYaw != targetHandYaw)
			{
				targetHandYaw = Util::InterpolateSmootherStep(oldHandYaw, targetHandYaw, min(1.0f, interpFactor));
			}

			handData->rotationInput = std::array<float, 3>({ targetHandYaw, targetHandPitch, targetHandRoll });
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
			handData->targetRotation = handData->defaultRotation;  //a_handNodePtr->local.rotate;
		}

		// Update forearm and hand node rotation to set (current) after potentially setting target rotations.
		UpdateNodeRotationToSet(a_p, forearmData, a_forearmNodePtr, true);
		UpdateNodeRotationToSet(a_p, handData, a_handNodePtr, true);

	}
	void NodeOrientationManager::UpdateNodeRotationBlendState(const std::shared_ptr<CoopPlayer>& a_p, const std::unique_ptr<NodeRotationData>& a_data, const RE::NiPointer<RE::NiAVObject>& a_nodePtr, bool a_isArmNode)
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
		// Continue blending in if target rotation was modified already
		// and if previously interrupteed, currently auto-pitched to face a target,
		// or still blending in.
		blendIn = 
		{
			(towardsTargetRotation) &&
			(
				(a_data->prevInterrupted) ||
				(!a_p->mm->aimPitchManuallyAdjusted && a_data->blendStatus != NodeRotationBlendStatus::kTargetReached) ||
				(a_data->blendStatus == NodeRotationBlendStatus::kBlendIn)
			)
		};
		// Blend out if rotation was not modified or interrupted,
		// and if the default rotation has not been reached.
		blendOut = 
		{
			(!towardsTargetRotation) &&
			(a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
		};

		if (blendIn)
		{
			if (a_data->blendStatus != NodeRotationBlendStatus::kBlendIn && a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
			{
				// Just started blending in.
				NodeRotationBlendStatus prevStatus = a_data->blendStatus;
				// Start blending in when the player first tries to rotate their arms.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kBlendIn);
				// Set starting rotation before blending in.
				if (prevStatus == NodeRotationBlendStatus::kDefaultReached)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend in from default.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from game's set rotation.
					a_data->startingRotation = a_data->defaultRotation;
				}
				else if (prevStatus == NodeRotationBlendStatus::kTargetReached)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend in from target.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from our set target rotation.
					a_data->startingRotation = a_data->targetRotation;
				}
				else
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend in from current.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from the current blended rotation.
					a_data->startingRotation = a_data->currentRotation;
				}
			}

			// Blend interval elapsed, so we'll now set the requested rotations to the target ones.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn &&
				a_data->blendInFrameCount >= Settings::uBlendPlayerNodeRotationsFrameCount)
			{
				// REMOVE when done debugging.
				/*ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Done blending in. {} frames elapsed.",
					a_p->coopActor->GetName(),
					a_nodePtr->name,
					a_data->blendInFrameCount);*/

				// Target rotation reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kTargetReached);
			}
		}
		else if (blendOut)
		{
			if (a_data->blendStatus != NodeRotationBlendStatus::kBlendOut && a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
			{
				NodeRotationBlendStatus prevStatus = a_data->blendStatus;
				// Start blending out if not already blending out and if the default rotation was not reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kBlendOut);
				// Set starting rotation before blending out.
				if (prevStatus == NodeRotationBlendStatus::kDefaultReached)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend out from default.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from game's set rotation.
					a_data->startingRotation = a_data->defaultRotation;
				}
				else if (prevStatus == NodeRotationBlendStatus::kTargetReached && a_data->rotationModified)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend out from target.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from our set target rotation.
					a_data->startingRotation = a_data->targetRotation;
				}
				else
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Start blend out from current.", a_p->coopActor->GetName(), a_nodePtr->name);

					// Starting from the current blended rotation.
					a_data->startingRotation = a_data->currentRotation;
				}
			}

			// Fully blended out.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendOut &&
				a_data->blendOutFrameCount >= Settings::uBlendPlayerNodeRotationsFrameCount)
			{
				// REMOVE when done debugging.
				/*ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Done blending out. {} frames elapsed.",
					a_p->coopActor->GetName(),
					a_nodePtr->name,
					a_data->blendOutFrameCount);*/

				// Default rotation reached.
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kDefaultReached);
			}
		}
		else
		{
			if (towardsTargetRotation && a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
			{
				//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Target rotation reached.", a_p->coopActor->GetName(), a_nodePtr->name);
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kTargetReached);
			}
			else if (!towardsTargetRotation && a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
			{
				//ALYSLC::Log("[MM] UpdateNodeRotationBlendState: {}: {}: Default rotation reached.", a_p->coopActor->GetName(), a_nodePtr->name);
				SetBlendStatus(a_nodePtr, NodeRotationBlendStatus::kDefaultReached);
			}
		}
	}
	void NodeOrientationManager::UpdateNodeRotationToSet(const std::shared_ptr<CoopPlayer>& a_p, const std::unique_ptr<NodeRotationData>& a_data, const RE::NiPointer<RE::NiAVObject>& a_nodePtr, bool a_isArmNode)
	{
		// Update the local node rotation to eventually set
		// by interpolating between the starting and target/default rotation endpoints.

		if (!a_data || !a_data.get() || !a_nodePtr || !a_nodePtr.get())
		{
			return;
		}

		// [TODO]:
		// Improve arm node interpolation to not rotate the arm through the torso
		// or in the wrong direction towards the target rotation, taking a longer path.
		// Also look into issues in interpolating between certain start/endpoint rotations,
		// where the starting endpoint jumps forward or the target/default rotation
		// is not reached by the time the blending interval elapses.

		// REMOVE when done debugging.
		/*glm::vec3 start
		{
			a_nodePtr->world.translate.x,
			a_nodePtr->world.translate.y,
			a_nodePtr->world.translate.z,
		};

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);*/

		if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn || a_data->blendStatus == NodeRotationBlendStatus::kTargetReached)
		{
			// Interpolate between starting and target local rotations when blending in.
			// Set directly to the target local rotation otherwise.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn)
			{
				// Blend in to reach the target rotation from the starting rotation.
				float t = std::clamp
				(
					static_cast<float>(a_data->blendInFrameCount) / 
					static_cast<float>(Settings::uBlendPlayerNodeRotationsFrameCount - 1),
					0.0f,
					1.0f
				);

				a_data->currentRotation = Util::InterpolateRotMatrix(a_data->startingRotation, a_data->targetRotation, t);

				// REMOVE when done debugging.
				/*glm::vec3 endSX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * right) * 10.0f;
				glm::vec3 endSY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * forward) * 10.0f;
				glm::vec3 endSZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * up) * 10.0f;
				glm::vec3 endCX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * right) * 10.0f;
				glm::vec3 endCY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * forward) * 10.0f;
				glm::vec3 endCZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * up) * 10.0f;
				glm::vec3 endTX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * right) * 10.0f;
				glm::vec3 endTY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * forward) * 10.0f;
				glm::vec3 endTZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * up) * 10.0f;

				DebugAPI::QueueArrow3D(start, endSX, 0xFFFF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endSY, 0x00FFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endSZ, 0xFF00FFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endTX, 0xFF0000FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endTY, 0x00FF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endTZ, 0x0000FFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCX, 0xFFFFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCY, 0xFFFFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCZ, 0xFFFFFFFF, 3.0f, 2.0f);*/

				a_data->blendInFrameCount++;
			}
			else
			{
				a_data->currentRotation = a_data->targetRotation;

				// REMOVE when done debugging.
				/*glm::vec3 endTX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * right) * 10.0f;
				glm::vec3 endTY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * forward) * 10.0f;
				glm::vec3 endTZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->targetRotation * up) * 10.0f;

				DebugAPI::QueueArrow3D(start, endTX, 0xFF0000FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endTY, 0x00FF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endTZ, 0x0000FFFF, 3.0f, 2.0f);*/
			}
		}
		else
		{
				
			// Interpolate between starting and game-given default local rotations when blending out.
			// Set directly to the default local rotation otherwise.
			if (a_data->blendStatus == NodeRotationBlendStatus::kBlendOut)
			{
				// Blend out to reach the game's assigned rotation from the starting rotation.
				float t = std::clamp
				(
					static_cast<float>(a_data->blendOutFrameCount) / 
					static_cast<float>(Settings::uBlendPlayerNodeRotationsFrameCount - 1),
					0.0f,
					1.0f
				);

				// NOTE: Unsure why as of now, but switching start and end interpolation endpoints and inverting the time ratio
				// correctly interpolates between the endpoints more consistently and does not modify the starting endpoint,
				// which happens if the regular interpolation direction is used instead.
				a_data->currentRotation = Util::InterpolateRotMatrix(a_data->defaultRotation, a_data->startingRotation, 1.0f - t);

				// REMOVE when done debugging.
				/*glm::vec3 endSX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * right) * 10.0f;
				glm::vec3 endSY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * forward) * 10.0f;
				glm::vec3 endSZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->startingRotation * up) * 10.0f;
				glm::vec3 endCX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * right) * 10.0f;
				glm::vec3 endCY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * forward) * 10.0f;
				glm::vec3 endCZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->currentRotation * up) * 10.0f;
				glm::vec3 endDX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * right) * 10.0f;
				glm::vec3 endDY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * forward) * 10.0f;
				glm::vec3 endDZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * up) * 10.0f;

				DebugAPI::QueueArrow3D(start, endSX, 0xFFFF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endSY, 0x00FFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endSZ, 0xFF00FFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endDX, 0xFF0000FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endDY, 0x00FF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endDZ, 0x0000FFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCX, 0xFFFFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCY, 0xFFFFFFFF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endCZ, 0xFFFFFFFF, 3.0f, 2.0f);*/

				a_data->blendOutFrameCount++;
			}
			else
			{
				a_data->currentRotation = a_data->defaultRotation;

				// REMOVE when done debugging.
				/*glm::vec3 endDX = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * right) * 10.0f;
				glm::vec3 endDY = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * forward) * 10.0f;
				glm::vec3 endDZ = start + ToVec3(a_nodePtr->parent->world.rotate * a_data->defaultRotation * up) * 10.0f;

				DebugAPI::QueueArrow3D(start, endDX, 0xFF0000FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endDY, 0x00FF00FF, 3.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endDZ, 0x0000FFFF, 3.0f, 2.0f);*/
			}
		}
	}
	void NodeOrientationManager::UpdateShoulderNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_shoulderNodePtr, bool a_rightShoulder)
	{
		// Update forearm and hand blend statuses and starting, current, and target rotations.
		// Modification method: local rotation derived from a world rotation
		// computed from hardcoded, manually verified interpolation endpoints for pitch/roll/yaw.
		// The player's RS movement determines the orientation of their arms.

		if (!a_shoulderNodePtr || !a_shoulderNodePtr.get())
		{
			return;
		}

		// Ensure the shoulder node is accounted for in rotation data map.
		if (!nodeRotationDataMap.contains(a_shoulderNodePtr))
		{
			nodeRotationDataMap.insert_or_assign(a_shoulderNodePtr, std::make_unique<NodeRotationData>());
		}

		// Rotation data we will modify.
		const auto& shoulderData = nodeRotationDataMap[a_shoulderNodePtr];
		shoulderData->prevInterrupted = shoulderData->interrupted;
		shoulderData->prevRotationModified = shoulderData->rotationModified;
		shoulderData->interrupted =
		{
			a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
			a_p->IsAwaitingRefresh() ||
			a_p->coopActor->IsWeaponDrawn() ||
			a_p->coopActor->actorState2.recoil ||
			a_p->coopActor->actorState2.staggered ||
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
		if (rotatingShoulder) 
		{
			// Set shoulder target rotation as modified.
			shoulderData->rotationModified = true;

			// Pick RS quadrant/axis based on its displacement.
			const auto& rsData = glob.cdh->GetAnalogStickState(a_p->controllerID, false);
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

			// Yaw (determined by RS X displacement and heading angle), pitch (determined by RS Y displacement), roll (constant).
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
				// "Outer point" represents arm orientation points to use when RS is displaced from center,
				// outer points ratio = 1, forward ratio = 0 at max displacement.
				// When centered, the forward ratio is 1 and the outer points ratio is 0,
				// meaning the forward arm orientation point is used.
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
					// Outer point: upward.
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
						// Outer points: outward and upward.
						const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
						const auto& upwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kUpward);
						float outwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = outwardWeight + upwardWeight;
						float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
						float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (outwardAngleX + upwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (outwardAngleY + upwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (outwardAngleZ + upwardAngleZ) * outerPointsRatio;

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
						// Outer points: inward and upward.
						const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
						const auto& upwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kUpward);
						float inwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = inwardWeight + upwardWeight;
						float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
						float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

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
						// Outer point: outward.
						const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
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
						// Outer points: inward.
						const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
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
						// Outer points: outward and downward.
						const auto& outwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kOutward);
						const auto& downwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kDownward);
						float outwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = outwardWeight + downwardWeight;
						float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
						float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (outwardAngleX + downwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (outwardAngleY + downwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (outwardAngleZ + downwardAngleZ) * outerPointsRatio;

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
						// Outer points: inward and downward.
						const auto& inwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kInward);
						const auto& downwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kDownward);
						float inwardWeight = max(RE::NiPoint2(1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = inwardWeight + downwardWeight;
						float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
						float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (inwardAngleX + downwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (inwardAngleY + downwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (inwardAngleZ + downwardAngleZ) * outerPointsRatio;

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

					// Outer point: downward.
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
						// Outer points: inward and downward.
						const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
						const auto& downwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kDownward);
						float inwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = inwardWeight + downwardWeight;
						float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
						float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (inwardAngleX + downwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (inwardAngleY + downwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (inwardAngleZ + downwardAngleZ) * outerPointsRatio;

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
						// Outer points: outward and downward.
						const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
						const auto& downwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kDownward);
						float outwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float downwardWeight = max(RE::NiPoint2(0.0f, -1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = outwardWeight + downwardWeight;
						float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
						float downwardAngleX = downwardMatAngleInputs[0] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleY = downwardMatAngleInputs[1] * (downwardWeight / totalOuterPointsWeight);
						float downwardAngleZ = downwardMatAngleInputs[2] * (downwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (outwardAngleX + downwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (outwardAngleY + downwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (outwardAngleZ + downwardAngleZ) * outerPointsRatio;

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
						// Outer point: inward.
						const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
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
						// Outer point: outward.
						const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
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
						// Outer points: inward and upward.
						const auto& inwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kInward);
						const auto& upwardMatAngleInputs = rightShoulderMatAngleInputs.at(ArmOrientation::kUpward);
						float inwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = inwardWeight + upwardWeight;
						float inwardAngleX = inwardMatAngleInputs[0] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleY = inwardMatAngleInputs[1] * (inwardWeight / totalOuterPointsWeight);
						float inwardAngleZ = inwardMatAngleInputs[2] * (inwardWeight / totalOuterPointsWeight);
						float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

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
						// Outer points: outward and upward.
						const auto& outwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kOutward);
						const auto& upwardMatAngleInputs = leftShoulderMatAngleInputs.at(ArmOrientation::kUpward);
						float outwardWeight = max(RE::NiPoint2(-1.0f, 0.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float upwardWeight = max(RE::NiPoint2(0.0f, 1.0f).Dot(RE::NiPoint2(xDisp, yDisp)), 0.0f);
						float totalOuterPointsWeight = outwardWeight + upwardWeight;
						float outwardAngleX = outwardMatAngleInputs[0] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleY = outwardMatAngleInputs[1] * (outwardWeight / totalOuterPointsWeight);
						float outwardAngleZ = outwardMatAngleInputs[2] * (outwardWeight / totalOuterPointsWeight);
						float upwardAngleX = upwardMatAngleInputs[0] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleY = upwardMatAngleInputs[1] * (upwardWeight / totalOuterPointsWeight);
						float upwardAngleZ = upwardMatAngleInputs[2] * (upwardWeight / totalOuterPointsWeight);

						float outerPointsAngleX = (outwardAngleX + upwardAngleX) * outerPointsRatio;
						float outerPointsAngleY = (outwardAngleY + upwardAngleY) * outerPointsRatio;
						float outerPointsAngleZ = (outwardAngleZ + upwardAngleZ) * outerPointsRatio;

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

		// REMOVE when done debugging.
		/*
		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);

		const auto& worldRot = a_shoulderNode->world.rotate;
		auto worldXAxis = RE::NiPoint3(worldRot * right);
		auto worldYAxis = RE::NiPoint3(worldRot * forward);
		auto worldZAxis = RE::NiPoint3(worldRot * up);
		glm::vec3 start{ a_shoulderNode->world.translate.x, a_shoulderNode->world.translate.y, a_shoulderNode->world.translate.z };
		glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 10.0f };
		glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 10.0f };
		glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 10.0f };
		DebugAPI::QueueArrow3D(start, endX, 0xFF0000FF, 5.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endY, 0x00FF00FF, 5.0f, 2.0f);
		DebugAPI::QueueArrow3D(start, endZ, 0x0000FFFF, 5.0f, 2.0f);
		*/

		// Init to default rotation if not adjusted.
		if (shoulderData->rotationModified) 
		{
			// Set rotation angle inputs used to construct world rotation matrix below.
			const float oldYaw = shoulderData->rotationInput[0];
			const float oldPitch = shoulderData->rotationInput[1];
			float yaw = Util::InterpolateSmootherStep(oldYaw, newRotationInput[0], min(1.0f, interpFactor));
			float pitch = Util::InterpolateSmootherStep(oldPitch, newRotationInput[1], min(1.0f, interpFactor));
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
			if (auto parent = RE::NiPointer<RE::NiAVObject>(a_shoulderNodePtr->parent); parent && parent.get())
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
			shoulderData->targetRotation = shoulderData->defaultRotation;
		}

		// Update shoulder node rotation to set (current) after potentially setting target rotations.
		UpdateNodeRotationToSet(a_p, shoulderData, a_shoulderNodePtr, true);
	}

	void NodeOrientationManager::UpdateTorsoNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p)
	{
		// Update torso node blend status and starting, current, and target rotations.
		// Modification method: local rotation derived from a world rotation offset 
		// from the default world rotation for each torso node.

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
			isRangedWeaponPrimed = 
			{
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased
			};
		}
		else if (a_p->em->HasCrossbowEquipped())
		{
			isRangedWeaponPrimed = 
			{
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				a_p->coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased 
			};
		}

		// Rotate the torso throughout the attack animation's duration when not attacking with a 2H ranged weapon.
		bool isAttackingWithoutRangedWeap = !a_p->em->Has2HRangedWeapEquipped() && a_p->pam->isAttacking;
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

		// Skip head node adjustments for the time being.
		const uint8_t numAdjustableNodes = 
		(
			a_p->isTransformed ? 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size() - 1 : 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size()
		);

		// Base axes/rotation set from the cached default world rotation for each node.
		RE::NiPoint3 baseXAxis{ };
		RE::NiPoint3 baseYAxis{ };
		RE::NiPoint3 baseZAxis{};
		RE::NiMatrix3 baseWorldRot{};
		// Torso node's parent world transform.
		RE::NiTransform parentWorld{ };

		// Set the axis of rotation to use when rotating the torso nodes.
		bool isAttackingWith2HRangedWeapon = a_p->pam->isAttacking && a_p->em->Has2HRangedWeapEquipped();
		if (isAttackingWith2HRangedWeapon) 
		{
			// Use the player's default attack source direction when aiming with a ranged weapon,
			// since the axis of rotation typically shifts away from the node's X axis.
			a_p->mm->playerTorsoAxisOfRotation = a_p->mm->playerDefaultAttackSourceDir.Cross(up);
		}
		else
		{
			// If not aiming with a ranged weapon, instead use the player's aiming direction
			// in the XY plane as the forward vector along which to derive the axis.
			auto aimingXYDir = 
			(
				Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(a_p->coopActor->data.angle.z))//a_p->coopActor->GetAimHeading()))
			);

			a_p->mm->playerTorsoAxisOfRotation = aimingXYDir.Cross(up);
		}

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
			if (!nodeRotationDataMap.contains(torsoNodePtr))
			{
				nodeRotationDataMap.insert_or_assign(torsoNodePtr, std::make_unique<NodeRotationData>());
			}

			// Rotation data we will modify.
			const auto& torsoData = nodeRotationDataMap[torsoNodePtr];
			// Set the rotation state flags before updating the blend state.
			torsoData->prevInterrupted = torsoData->interrupted;
			torsoData->prevRotationModified = torsoData->rotationModified;
			// Do not set the custom rotation if not aiming with a drawn weapon,
			// or if the player is staggered/ragdolled or inactive.
			torsoData->interrupted = 
			{
				(!a_p->mm->isDashDodging && !isAiming && a_p->coopActor->IsWeaponDrawn()) ||
				a_p->coopActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
				a_p->IsAwaitingRefresh() ||
				a_p->coopActor->actorState2.recoil ||
				a_p->coopActor->actorState2.staggered
			};
			// Rotation was modified if the player has adjusted their aim pitch
			// or if they are dash dodging on the ground.
			torsoData->rotationModified = 
			{
				(a_p->mm->aimPitchAdjusted) || 
				(a_p->mm->isDashDodging && !a_p->mm->isParagliding)
			};

			// Update blend state after modifying the rotation flags
			// and before setting current and target rotations.
			UpdateNodeRotationBlendState(a_p, torsoData, torsoNodePtr, false);

			// Get the game's default world rotation for this node and derive its axes.
			if (defaultNodeWorldTransformsMap.contains(torsoNodePtr))
			{
				const auto& defaultWorldRotation = defaultNodeWorldTransformsMap.at(torsoNodePtr).rotate;
				baseXAxis = defaultWorldRotation * right;
				baseYAxis = defaultWorldRotation * forward;
				baseZAxis = defaultWorldRotation * up;
				baseWorldRot = defaultWorldRotation;

				// Get parent world transform of the base spinal node.
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

			// REMOVE when done debugging.
			/*
			const auto& worldRot = torsoNodePtr->world.rotate;
			auto worldXAxis = RE::NiPoint3(worldRot * right);
			auto worldYAxis = RE::NiPoint3(worldRot * forward);
			auto worldZAxis = RE::NiPoint3(worldRot * up);

			glm::vec3 start{ torsoNode->world.translate.x, torsoNode->world.translate.y, torsoNode->world.translate.z };
			glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 10.0f };
			glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 10.0f };
			glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 10.0f };
			DebugAPI::QueueArrow3D(start, endX, 0xFF0000FF, 5.0f, 2.0f);
			DebugAPI::QueueArrow3D(start, endY, 0x00FF00FF, 5.0f, 2.0f);
			DebugAPI::QueueArrow3D(start, endZ, 0x0000FFFF, 5.0f, 2.0f);*/

			if (torsoData->rotationModified)
			{
				RE::NiPoint3 newXAxis = baseXAxis;
				RE::NiPoint3 newYAxis = baseYAxis;
				RE::NiPoint3 newZAxis = baseZAxis;

				float frac = 1.0f;
				if (i == 0)
				{
					frac = 0.5f;
				}
				else if (i == 1)
				{
					frac = 5.0f / 6.0f;
				}
				else if (i == 2)
				{
					frac = 1.0f;
				}

				/*float defaultPitch = Util::DirectionToGameAngPitch(baseYAxis);
				ALYSLC::Log("[MM] {}: {}.", a_p->coopActor->GetName(), defaultPitch * TO_DEGREES);
				float aimPitchProportion = (a_p->mm->aimPitch + PI / 2.0f) / PI;
				float remappedPitch = 0.0f;
				if (aimPitchProportion >= 0.5f) 
				{
					remappedPitch = -std::lerp(0.0f, PI / 2.0f + defaultPitch, 2.0f * (aimPitchProportion - 0.5f));
				}
				else
				{
					remappedPitch = -std::lerp(-PI / 2.0f + defaultPitch, 0.0f, 2.0f * aimPitchProportion);
				}

				Util::RotateVectorAboutAxis
				(
					newXAxis, 
					a_p->mm->playerTorsoAxisOfRotation, 
					remappedPitch * frac
				);
				Util::RotateVectorAboutAxis
				(
					newYAxis, 
					a_p->mm->playerTorsoAxisOfRotation, 
					remappedPitch * frac
				);
				Util::RotateVectorAboutAxis
				(
					newZAxis,
					a_p->mm->playerTorsoAxisOfRotation, 
					remappedPitch * frac
				);*/

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

					// Modify the yaw of the torso nodes if the player is aiming with a ranged weapon while mounted,
					// since the game does not automatically rotate our players to face the crosshair target.
					if (isMounted && isAttackingWith2HRangedWeapon)
					{
						float prevYawOffset = torsoData->rotationInput[0];
						float yawOffset = 0.0f;
						if (auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(a_p->tm->crosshairRefrHandle);
							crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get()))
						{
							auto playerAimYaw = Util::DirectionToGameAngYaw(a_p->mm->playerDefaultAttackSourceDir);
							float playerToTargetYaw = Util::GetYawBetweenPositions(a_p->mm->playerDefaultAttackSourcePos, a_p->tm->crosshairWorldPos);
							yawOffset = Util::NormalizeAngToPi(playerAimYaw - playerToTargetYaw);
							// Prevent interpolation along the 'longer' path between the two endpoints
							// by shifting the target endpoint to an equivalent angle that is closer
							// to the starting endpoint.
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
							torsoData->rotationInput[0] = Util::NormalizeAngToPi(torsoData->rotationInput[0]);
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
				// NOTE: Must transpose to ensure that our modified axes are set
				// as the new target local rotation matrix's axes.
				newWorldRot = newWorldRot.Transpose();
				RE::NiTransform newTrans
				{
					torsoNodePtr->world 
				};
				newTrans.rotate = newWorldRot;

				// REMOVE when done debugging.
				/*const auto& defaultPos = defaultNodeWorldTransformsMap.at(torsoNodePtr).translate;
				glm::vec3 start
				{
					defaultPos.x,
					defaultPos.y,
					defaultPos.z
				};
				glm::vec3 endX = start + glm::vec3(newXAxis.x, newXAxis.y, newXAxis.z) * 10.0f;
				glm::vec3 endY = start + glm::vec3(newYAxis.x, newYAxis.y, newYAxis.z) * 10.0f;
				glm::vec3 endZ = start + glm::vec3(newZAxis.x, newZAxis.y, newZAxis.z) * 10.0f;
				DebugAPI::QueueArrow3D(start, endX, 0xFF0000FF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endY, 0x00FF00FF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endZ, 0x0000FFFF, 5.0f, 2.0f);

				endX = start + glm::vec3(baseXAxis.x, baseXAxis.y, baseXAxis.z) * 10.0f;
				endY = start + glm::vec3(baseYAxis.x, baseYAxis.y, baseYAxis.z) * 10.0f;
				endZ = start + glm::vec3(baseZAxis.x, baseZAxis.y, baseZAxis.z) * 10.0f;
				DebugAPI::QueueArrow3D(start, endX, 0xFFFF00FF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endY, 0x00FFFFFF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, endZ, 0xFF00FFFF, 5.0f, 2.0f);
				DebugAPI::QueueArrow3D(start, start + ToVec3(a_p->mm->playerTorsoAxisOfRotation) * 10.0f, 0xFFFFF00FF, 5.0f, 2.0f);*/

				// Set local rotation corresponding to our world rotation.
				torsoData->targetRotation = (parentWorld.Invert() * newTrans).rotate;
				// Update the parent world transform for the next node by setting it to the current node's new transform
				// so that we can emulate a 'downward' pass of our own when setting subsequent child torso nodes' rotations.
				parentWorld = newTrans;
			}
			else
			{
				torsoData->targetRotation = torsoData->defaultRotation;
			}

			// Update torso node rotation to set (current) after potentially setting target rotations.
			UpdateNodeRotationToSet(a_p, torsoData, torsoNodePtr, false);
		}
	}
}

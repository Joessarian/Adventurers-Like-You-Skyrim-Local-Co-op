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
		SetAttackSourceOrientationData();
		SetAimRotation();
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
			nrm->InstantlyResetAllNodeData();
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

		// Reset node rotations.
		if (currentState == ManagerState::kAwaitingRefresh)
		{
			nrm->InstantlyResetAllNodeData();
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
		playerAttackSourcePos = coopActor->data.location + RE::NiPoint3(0.0f, 0.0f, 0.75f * coopActor->GetHeight());
		playerAttackSourceDir = RE::NiPoint3(0.0f, 0.0f, 0.0f);
		// Atomic flags.
		shouldFaceTarget = shouldResetAimPitch = startJump = false;
		// Movement parameters list.
		movementOffsetParams = std::vector<float>(!MoveParams::kTotal, 0.0f);
		// Node rotation data.
		nrm = std::make_unique<NodeRotationManager>();
		// Booleans.
		aimPitchAdjusted = false;
		aimPitchManuallyAdjusted = false;
		attemptDiscovery = false;
		dontMoveSet = true;
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
		// Floats.
		aimPitch = PI / 18.0f;
		baseHeightMult = max(0.001f, static_cast<float>(coopActor->refScale) / 100.0f);
		baseSpeedMult = Settings::fBaseSpeed * (1.0f / baseHeightMult);
		dashDodgeCompletionRatio = 0.0f;
		dashDodgeEquippedWeight = 0.0f;
		dashDodgeInitialSpeed = 0.0f;
		dashDodgeLSDisplacement = 0.0f;
		lsAngAtMaxDisp = rsAngAtMaxDisp = 0.0f;
		oldLSAngle = 0.0f;
		playerScaledHeight = coopActor->GetHeight();
		playerPitch = 0.0f;
		playerYaw = coopActor->GetHeading(false);
		prevLSAngAtMaxDisp = prevRSAngAtMaxDisp = 0.0f;
		magicParaglideEndZVel = magicParaglideStartZVel = magicParaglideVelInterpFactor = 0.0f;
		framesSinceAttemptingDiscovery = framesSinceRequestingDashDodge = framesSinceStartingDashDodge = 0;

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
			bool startedAttackDuringDodge = Util::GetElapsedSeconds(p->lastAttackStartTP) < p->pam->GetSecondsSinceLastStop(InputAction::kDodge);
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

					// Reset character controller pitch.
					charController->pitchAngle = charController->rollAngle = 0.0f;
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
				const float dodgeVelRatio = 
				(
					Settings::fMinDashDodgeSpeedmult == 0.0f ? 
					2.0f : 
					Settings::fMaxDashDodgeSpeedmult / Settings::fMinDashDodgeSpeedmult
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
				float dodgeXYSpeed = 0.0f;
				// Burst of speed that peaks at the dodge midpoint.
				if (dashDodgeCompletionRatio <= 0.5f) 
				{
					dodgeXYSpeed = Util::InterpolateEaseOut
					(
						dashDodgeInitialSpeed,
						maxXYSpeed,
						dashDodgeCompletionRatio * 2.0f,
						2.0f
					) * dashDodgeLSDisplacement;
				}
				else
				{
					dodgeXYSpeed = Util::InterpolateEaseIn(
						maxXYSpeed,
						dashDodgeInitialSpeed,
						(dashDodgeCompletionRatio - 0.5f) * 2.0f,
						2.0f
					) * dashDodgeLSDisplacement;
				}

				// Convert back to havok units and then to hkVector4.
				havokVel = TohkVector4(dashDodgeDir * dodgeXYSpeed * GAME_TO_HAVOK);
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
						(p->isPlayer1 ? PI / 4.0f : PI / 2.0f) :
						(Util::InterpolateSmootherStep(PI / 4.0f, PI / 12.0f, dodgeXYSpeed / Settings::fMaxDashDodgeSpeedmult))
					};
					float endpointPitch = isParagliding ? charController->pitchAngle : 0.0f;
					float endpointRoll = isParagliding ? charController->rollAngle : 0.0f;
					if (isBackStepDodge)
					{
						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean back.
							charController->pitchAngle = Util::InterpolateEaseOut
							(
								endpointPitch, 
								-maxLeanAngle, 
								(animRatio / halfAnimCompletionRatio),
								2.0f
							);
						}
						else
						{
							// Straighten back out by the time the dodge ends.
							charController->pitchAngle = Util::InterpolateEaseIn
							(
								-maxLeanAngle, 
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
						float pitchSign = fabsf(movementToFacingYawDiff) <= PI / 2.0f ? 1.0f : -1.0f;
						float rollSign = movementToFacingYawDiff <= 0.0f ? 1.0f : -1.0f;
						if (animRatio < halfAnimCompletionRatio)
						{
							// Lean in movement direction.
							charController->pitchAngle = Util::InterpolateEaseOut
							(
								endpointPitch, 
								maxLeanAngle * pitchRatio * pitchSign, (animRatio / halfAnimCompletionRatio), 
								2.0f
							);
							charController->rollAngle = Util::InterpolateEaseOut
							(
								endpointRoll, 
								maxLeanAngle * rollRatio * rollSign, (animRatio / halfAnimCompletionRatio), 
								2.0f
							);
						}
						else
						{
							// Straighten back out by the time the dodge ends.
							charController->pitchAngle = Util::InterpolateEaseIn
							(
								maxLeanAngle * pitchRatio * pitchSign, 
								endpointPitch, 
								(animRatio / halfAnimCompletionRatio) - 1.0f, 2.0f
							);
							charController->rollAngle = Util::InterpolateEaseIn
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
			// At lower framerates, decrease the gravity mult, since the gravity mult will be active for longer.
			float gravityMult = (1.0f / (*g_deltaTimeRealTime * 60.0f)) * Settings::fJumpingGravityMult;
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
				charController->flags.set(RE::CHARACTER_FLAGS::kJumping);
				charController->flags.set(RE::CHARACTER_FLAGS::kNoGravityOnGround);
				charController->context.currentState = RE::hkpCharacterStateType::kInAir;
				velBeforeJump.quad.m128_f32[2] = havokInitialJumpZVelocity;

				const auto& lsData = glob.cdh->GetAnalogStickState(controllerID, true);
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
					velBeforeJump : 
					RE::hkVector4(lsVelXY.x, lsVelXY.y, havokInitialJumpZVelocity, 0.0f)
				);
				// Invert gravity and set initial velocity.
				charController->gravity = -gravityMult;
				charController->SetLinearVelocityImpl(velBeforeJump);
				charController->lock.Unlock();

				// Jump has started.
				p->jumpStartTP = SteadyClock::now();
				isAirborneWhileJumping = true;
				isFallingWhileJumping = false;
				sentJumpFallEvent = false;
				startJump = false;
			}
			else if (isAirborneWhileJumping)
			{
				// Abort jump if ragdolling.
				if (coopActor->IsInRagdollState()) 
				{
					// Reset jump state variables.
					charController->lock.Lock();
					charController->flags.reset(RE::CHARACTER_FLAGS::kJumping);
					charController->flags.reset(RE::CHARACTER_FLAGS::kNoGravityOnGround);
					charController->gravity = 1.0f;
					charController->fallStartHeight = 0.0f;
					charController->fallTime = 0.0f;
					charController->lock.Unlock();

					isAirborneWhileJumping = false;
					isFallingWhileJumping = false;
					startJump = false;
					sentJumpFallEvent = false;

					p->jumpStartTP = SteadyClock::now();
					return;
				}

				float secsSinceGather = Util::GetElapsedSeconds(p->jumpStartTP);
				// Handle ascent to peak of the jump at which the player begins to fall.
				if (!isFallingWhileJumping)
				{
					isFallingWhileJumping = secsSinceGather >= Settings::fSecsAfterGatherToFall;
					charController->lock.Lock();
					// Zero gravity at apex.
					charController->gravity = min
					(
						Settings::fJumpingGravityMult,
						Settings::fJumpingGravityMult * (secsSinceGather / max(0.01f, Settings::fSecsAfterGatherToFall) - 1.0f)
					);
					charController->lock.Unlock();
				}
				else
				{
					// Only send fall animation, which cancels all melee/ranged attack animations,
					// if the player is not attacking or casting.
					if (!sentJumpFallEvent && !p->pam->isAttacking)
					{
						charController->lock.Lock();
						charController->flags.reset(RE::CHARACTER_FLAGS::kJumping);
						charController->flags.reset(RE::CHARACTER_FLAGS::kNoGravityOnGround);
						charController->lock.Unlock();
						coopActor->NotifyAnimationGraph("JumpFall");
						sentJumpFallEvent = true;
					}

					// Check if the player has landed.
					if (charController->surfaceInfo.supportedState.get() != RE::hkpSurfaceInfo::SupportedState::kUnsupported)
					{
						// Reset jump state variables.
						charController->lock.Lock();
						charController->gravity = 1.0f;
						charController->fallStartHeight = 0.0f;
						charController->fallTime = 0.0f;
						charController->lock.Unlock();

						isAirborneWhileJumping = false;
						isFallingWhileJumping = false;
						startJump = false;

						// Update jump start TP on landing.
						p->jumpStartTP = SteadyClock::now();

						// Have to manually trigger landing animation to minimize occurrences of the hovering bug.
						coopActor->NotifyAnimationGraph("JumpLand");
						charController->flags.set(RE::CHARACTER_FLAGS::kCheckSupport, RE::CHARACTER_FLAGS::kFloatLand, RE::CHARACTER_FLAGS::kJumping);
						charController->lock.Lock();
						charController->surfaceInfo.surfaceNormal = RE::hkVector4(0.0f);
						charController->surfaceInfo.surfaceDistanceExcess = 0.0f;
						charController->surfaceInfo.supportedState = RE::hkpSurfaceInfo::SupportedState::kSupported;
						charController->lock.Unlock();
					}
					else
					{
						charController->lock.Lock();
						charController->gravity = min
						(
							Settings::fJumpingGravityMult, 
							Settings::fJumpingGravityMult * (secsSinceGather / max(0.01f, Settings::fSecsAfterGatherToFall) - 1.0f)
						);
						charController->lock.Unlock();
					}
				}
			}

			// REMOVE when done debugging.
			/*ALYSLC::Log("[MM] PerformJump: {}: base/set grav mult: {}, {}. Start jump: {}, airborne: {}, seconds since airborne: {}",
				coopActor->GetName(), gravityMult, charController->gravity,
				startJump, isAirborneWhileJumping, startJump ? 0.0f : Util::GetElapsedSeconds(p->jumpStartTP));*/
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
		auto lMagNode = data3D->GetObjectByName(strings->npcLMagicNode);
		auto rMagNode = data3D->GetObjectByName(strings->npcRMagicNode);

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
				auto magNode = magicCaster->GetMagicNode();
				if (!magNode) 
				{
					return;
				}

				RE::NiPoint3 forward{ 0.0f, 1.0f, 0.0f };	
				// No target and not facing crosshair position, so return default facing direction pitch/yaw.
				if (a_targetPtr || shouldFaceTarget)
				{
					float pitch = Util::GetPitchBetweenPositions(magNode->world.translate, a_targetPos);
					float yaw = Util::GetYawBetweenPositions(magNode->world.translate, a_targetPos);
					pitchYawPair.first = pitch;
					pitchYawPair.second = yaw;
				}

				Util::SetRotationMatrix(magNode->world.rotate, pitchYawPair.first, pitchYawPair.second);
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

				++activeNodesCount;
				return;
			}
				
			bool valid3D = coopActor->loadedData && coopActor->loadedData->data3D;
			if (!valid3D)
			{
				return;
			}

			if (auto headMagNode = coopActor->loadedData->data3D->GetObjectByName(strings->npcHeadMagicNode); 
				headMagNode && p->pam->IsPerforming(InputAction::kQuickSlotCast))
			{
				if (!useAimPitchPos)
				{
					// Just in case if the caster node is not available, even though the instant caster is casting our quick slot spell.
					avgPitchYawPair.first += Util::GetPitchBetweenPositions(headMagNode->world.translate, targetPos);
					avgPitchYawPair.second += Util::GetYawBetweenPositions(headMagNode->world.translate, targetPos);
				}

				++activeNodesCount;
				return;
			}
			else if (auto lookNode = coopActor->loadedData->data3D->GetObjectByName(strings->npcLookNode); 
				lookNode && p->pam->IsPerforming(InputAction::kQuickSlotCast))
			{
				if (!useAimPitchPos)
				{
					// Just in case if the head magic node is not available, even though the instant caster is casting our quick slot spell.
					avgPitchYawPair.first += Util::GetPitchBetweenPositions(lookNode->world.translate, targetPos);
					avgPitchYawPair.second += Util::GetYawBetweenPositions(lookNode->world.translate, targetPos);
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
		}
		else
		{
			// No active nodes, so adjust aim pitch to our custom pitch.
			coopActor->SetGraphVariableFloat("AimPitchCurrent", -aimPitch);
		}
	}

	void MovementManager::SetAttackSourceOrientationData()
	{
		// Get node from which an attack would originate,
		// based on the player's equipped gear, 
		// and save its position and direction.
		
		// Need valid 3D and fixed strings.
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings || !coopActor->loadedData || !coopActor->loadedData->data3D)
		{
			return;
		}

		const auto& data3D = coopActor->loadedData->data3D;
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		auto arrowNode = data3D->GetObjectByName(strings->arrow0);
		auto headNode = data3D->GetObjectByName(strings->npcHead);
		auto leadingFootNode = data3D->GetObjectByName(strings->npcLFoot);
		auto leftHandNode = data3D->GetObjectByName("NPC L Hand [LHnd]"sv);
		auto lMagNode = data3D->GetObjectByName(strings->npcLMagicNode);
		auto rightHandNode = data3D->GetObjectByName("NPC R Hand [RHnd]"sv);
		auto rMagNode = data3D->GetObjectByName(strings->npcRMagicNode);
		auto weaponNode = data3D->GetObjectByName(strings->weapon);
		RE::NiAVObject* sourceNode = nullptr;
		bool ammoDrawnOrLater = 
		{
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing
		};
		bool nockingAmmo = coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
							coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw;
		bool isAimingWithRangedWeapon = ammoDrawnOrLater;
		if ((p->pam->isCastingLH || p->pam->isCastingRH || p->pam->isInCastingAnim) && lMagNode && rMagNode)
		{
			if (p->pam->isCastingLH && p->pam->isCastingRH)
			{
				// Best approximation to be made here is to rotate about the point between the two casting nodes
				// when casting with two hands, as there is no single source point for the released projectiles.
				playerAttackSourcePos = (lMagNode->world.translate + rMagNode->world.translate) / 2.0f;
			}
			else if (p->pam->isCastingLH && lMagNode)
			{
				playerAttackSourcePos = lMagNode->world.translate;
				sourceNode = lMagNode;
			}
			else if (p->pam->isCastingRH && rMagNode)
			{
				playerAttackSourcePos = rMagNode->world.translate;
				sourceNode = rMagNode;
			}
			else
			{
				playerAttackSourcePos = (lMagNode->world.translate + rMagNode->world.translate) / 2.0f;
			}
		}
		else if (weaponNode && isAimingWithRangedWeapon)
		{
			playerAttackSourcePos = weaponNode->world.translate;
			sourceNode = weaponNode;
		}
		else
		{
			playerAttackSourcePos = headNode ? headNode->world.translate : coopActor->GetLookingAtLocation();
			sourceNode = headNode;
		}

		// Since the arrow rotates away from the player 
		// while it is being removed from the quiver and nocked,
		// using the direction from the draw hand to the bow hand is more stable 
		// and less jittery than using the arrow/weapon node's rotation.
		// The arrow node rotation will not be stable until it is fully drawn.
		if (nockingAmmo && sourceNode && leftHandNode && rightHandNode && p->em->HasBowEquipped()) 
		{
			float playerAttackSourceYaw = Util::DirectionToGameAngYaw(leftHandNode->world.translate - rightHandNode->world.translate);
			float playerAttackSourcePitch = Util::DirectionToGameAngPitch(sourceNode->world.rotate * forward);
			playerAttackSourceDir = Util::RotationToDirectionVect(-playerAttackSourcePitch, Util::ConvertAngle(playerAttackSourceYaw));
		}
		else
		{
			// Don't follow the node once its pitch gets close to 90 degrees, use the player facing direction instead
			// to prevent the player from jittering and swapping facing directions when turning to
			// direct this node at the target.
			if (sourceNode && fabsf(Util::DirectionToGameAngPitch(sourceNode->world.rotate * forward)) < 85.0f * PI / 180.0f)
			{
				playerAttackSourceDir = sourceNode->world.rotate * forward;
			}
			else
			{
				playerAttackSourceDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(coopActor->data.angle.z));
			}
		}

		// REMOVE when done debugging node directions.
		/*glm::vec3 startVec = ToVec3(playerAttackSourcePos);
		glm::vec3 endVec1 = startVec + ToVec3(playerAttackSourceDir) * 20.0f;
		RE::NiPoint3 toCrosshairPos = p->tm->crosshairWorldPos - playerAttackSourcePos;
		toCrosshairPos.Unitize();
		glm::vec3 endVec2 = startVec + ToVec3(toCrosshairPos.x) * 20.0f;
		RE::NiPoint3 weaponNodeDir = weaponNode ? weaponNode->world.rotate * forward : toCrosshairPos;
		glm::vec3 endVec3 = startVec + ToVec3(weaponNodeDir) * 20.0f;
		auto lookNode = coopActor->loadedData->data3D->GetObjectByName(strings->npcLookNode);
		RE::NiPoint3 lookingDir = lookNode ? lookNode->world.rotate * forward : toCrosshairPos;
		glm::vec3 endVec4 = startVec + ToVec3(lookingDir) * 20.0f;
		DebugAPI::QueuePoint3D(startVec, Settings::vuOverlayRGBAValues[playerID], 5.0f);
		DebugAPI::QueueArrow3D(startVec, endVec1, Settings::vuOverlayRGBAValues[playerID], 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(startVec, endVec2, Settings::vuCrosshairOuterOutlineRGBAValues[playerID], 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(startVec, endVec3, Settings::vuCrosshairInnerOutlineRGBAValues[playerID], 3.0f, 2.0f);
		DebugAPI::QueueArrow3D(startVec, endVec4, 0x00FFFFFF, 3.0f, 2.0f);*/
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
			else if (!lsMoved && !isDashDodging && !isRequestingDashDodge && !isParagliding && dontMoveSet)
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
				else if (Util::PointIsOnScreen(aimPitchPos))
				{
					currentProc->SetHeadtrackTarget(coopActor.get(), aimPitchPos);
				}
			}
			else
			{
				/*coopActor->SetGraphVariableBool("bHeadTrackSpine", true);
				coopActor->SetGraphVariableInt("IsNPC", 1);*/
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
					else if (Util::PointIsOnScreen(aimPitchPos))
					{
						// Look at aim pitch pos when not interacting with a refr.
						currentProc->SetHeadtrackTarget(coopActor.get(), aimPitchPos);
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
					else if (Util::PointIsOnScreen(aimPitchPos))
					{
						// Look at aim pitch pos when there is no targeted actor/refr.
						currentProc->SetHeadtrackTarget(coopActor.get(), aimPitchPos);
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
		coopActor->data.angle.z = Util::NormalizeAng0To2Pi(coopActor->data.angle.z);
		float rotMult = GetRotationMult();
		float playerTargetYaw = coopActor->data.angle.z;
		// Face the target directly at all times after toggled on by FaceTarget bind.
		bool faceTarget = false;
		// Temporarily turn to face the target when certain actions trigger.
		bool turnToTarget = false;
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
				float xyDistToTarget = Util::GetXYDistance(playerAttackSourcePos, targetLocation);
				float minDistToSlowRotation = Util::GetXYDistance(coopActor->data.location, playerAttackSourcePos) * 0.1f;
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
							xyDistToTarget - minDistToSlowRotation, Settings::fTargetAttackSourceDistToSlowRotation
						) / 
						max
						(
							0.01f, Settings::fTargetAttackSourceDistToSlowRotation
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
					yawToTarget = Util::DirectionToGameAngYaw(targetLocation - playerAttackSourcePos);
					float aimHeading = 0.0f;
					coopActor->GetGraphVariableFloat("AimHeadingCurrent", aimHeading);
					playerTargetYaw = coopActor->data.angle.z + (Util::NormalizeAngToPi(yawToTarget - (coopActor->data.angle.z + aimHeading))) * rotMult;  //(aimHeading * rotMult);  //+ Util::NormalizeAngToPi(yawToTarget - playerAttackSourceYaw) * rotMult;
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

		// Scale the angle diff by the rotation multiplier before setting the target yaw.
		// No rotation when dash dodging.
		// Otherwise, set rotation offset to the nearest degree.
		float zRotOffset = 
		(
			isDashDodging ? 
			0.0f : 
			static_cast<long long>
			(
				rotMult * Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z) * TO_DEGREES
			) / TO_DEGREES
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
				movementActor->data.angle.z += Util::InterpolateSmootherStep
				(
					0.0f, 
					Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z), 
					0.25f
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
				float headingToPos =
				(
					Util::NormalizeAngToPi
					(
						Util::GetYawBetweenPositions(movementActor->data.location, interactionPackageEntryPos) -
						movementActor->GetHeading(false)
					)
				);
				// Move to interaction package entry position which was set during activation.
				// Slow down when nearing the interaction position.
				KeepOffsetFromActor
				(
					coopActor->GetHandle(), 
					RE::NiPoint3(0.0f, 10.0f, 0.0f), 
					RE::NiPoint3(0.0f, 0.0f, headingToPos),
					0.0f, 
					0.0f
				);

				// Rotate slowly to face the target position using the player's refr data angle
				// instead of the movement rotation offset.
				//movementActor->data.angle.z += Util::InterpolateSmootherStep(0.0f, headingToPos, 0.25f);
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
		}
		else if ((!isParagliding) && (shouldStopMoving || lsMag == 0.0f))
		{
			// Freezes co-op companion players in midair temporarily, 
			// so only set don't move flag when not paragliding, on the ground, 
			// and not trying to jump.
			if (shouldStopMoving && !p->pam->isAttacking && !isAirborneWhileJumping && !startJump)
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
				// Manually rotate to avoid slow motion shifting when the Z rotation offset is small.
				movementActor->data.angle.z += Util::InterpolateSmootherStep
				(
					0.0f,
					Util::NormalizeAngToPi(playerTargetYaw - movementActor->data.angle.z), 
					0.25f
				);
			}
			else
			{
				// Already stopped and no need to rotate or handle movement.
				SetDontMove(false);
				ClearKeepOffsetFromActor();
			}
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
			// Position and rotation.
			// NOTE: KeepOffsetFromActor() called on P1's mount does not work, so while P1 is mounted, they must not be AI driven.
			if (movementActor->IsAMount())
			{
				// Mounts cannot move sideways, so move forward and rotate.
				KeepOffsetFromActor
				(
					movementActor->GetHandle(), 
					RE::NiPoint3(0.0f, 1.0f, 0.0f), 
					RE::NiPoint3(0.0f, 0.0f, zRotOffset), 
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
					float angMult = Settings::fBaseRotationMult * Settings::fBaseMTRotationMult;
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
					KeepOffsetFromActor
					(
						movementActor->GetHandle(), 
						RE::NiPoint3(xPosOffset, yPosOffset, 0.0f), 
						RE::NiPoint3(0.0f, 0.0f, zRotOffset), 
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

		float pitchToTarget = Util::GetPitchBetweenPositions(playerAttackSourcePos, targetPos);
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
				auto pitchDiff = Util::NormalizeAngToPi(pitchToTarget - aimPitch);
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
		bool shouldResetNodeRotations = shouldResetAimPitch || resetAimPitchIfNotAdjusted;
		if (shouldResetNodeRotations)
		{
			// Clear out all previously set node target rotations, preventing blending in to the cleared values.
			if (shouldResetAimPitch) 
			{
				std::unique_lock<std::mutex> lock(p->mm->nrm->rotationDataMutex, std::try_to_lock);
				if (lock)
				{
					nrm->ClearCustomRotations();
				}
			}

			// Tip upward slightly when swimming.
			if (adjustAimPitchToFaceTarget)
			{
				auto pitchDiff = Util::NormalizeAngToPi(pitchToTarget - aimPitch);
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
				aimPitch = 0.0f; 
			}

			// Indicate that aim pitch was reset and is no longer manually adjusted.
			aimPitchManuallyAdjusted = aimPitchAdjusted = false;
			shouldResetAimPitch = false;
		}

		// Set the aim pitch position after modifying the aim pitch if the player is not transformed.
		if (!p->isTransforming) 
		{
			float radialDist = playerScaledHeight / 2.0f;
			auto eyePos = Util::GetEyePosition(coopActor.get());
			aimPitchPos = RE::NiPoint3(
				eyePos.x + radialDist * cosf(Util::ConvertAngle(coopActor->GetHeading(false))) * cosf(aimPitch),
				eyePos.y + radialDist * sinf(Util::ConvertAngle(coopActor->GetHeading(false))) * cosf(aimPitch),
				eyePos.z - radialDist * sinf(aimPitch));
			// Modifications to P1's X angle here while aiming with a bow/crossbow
			// double up the effects of TDM's spinal rotation, so halve the set pitch here.
			coopActor->data.angle.x = 
			(
				(
					p->isPlayer1 &&
					ALYSLC::TrueDirectionalMovementCompat::g_trueDirectionalMovementInstalled &&
					p->pam->isRangedWeaponAttack
				) ? 
				aimPitch / 2.0f :
				aimPitch
			);
		}
	}

	void MovementManager::UpdateCachedArmNodeRotationData(RE::NiAVObject* a_forearmNode, RE::NiAVObject* a_handNode, bool a_rightArm)
	{
		const auto forearmNameHash = Hash(a_forearmNode->name);
		const auto handNameHash = Hash(a_handNode->name);
		// Invalid nodes or not accounted for in the rotation data map.
		if (!a_forearmNode || !a_handNode)
		{
			return;
		}

		// Ensure both nodes are accounted for in rotation data map.
		if (!nrm->nodeRotationDataMap.contains(forearmNameHash))
		{
			nrm->nodeRotationDataMap.insert_or_assign(forearmNameHash, std::make_unique<NodeRotationData>());
		}

		if (!nrm->nodeRotationDataMap.contains(handNameHash))
		{
			nrm->nodeRotationDataMap.insert_or_assign(handNameHash, std::make_unique<NodeRotationData>());
		}

		auto& forearmData = nrm->nodeRotationDataMap[forearmNameHash];
		auto& handData = nrm->nodeRotationDataMap[handNameHash];
		// Set initial starting/current/target rotations if not set yet.
		const auto identityMat = RE::NiMatrix3();
		if (forearmData->startingRotation == identityMat ||
			forearmData->currentRotation == identityMat ||
			forearmData->defaultRotation == identityMat ||
			forearmData->targetRotation == identityMat)
		{
			forearmData->startingRotation =
			forearmData->currentRotation =
			forearmData->defaultRotation = 
			forearmData->targetRotation = a_forearmNode->local.rotate;
		}

		if (handData->startingRotation == identityMat ||
			handData->currentRotation == identityMat ||
			handData->currentRotation == identityMat ||
			handData->targetRotation == identityMat)
		{
			handData->startingRotation =
			handData->currentRotation =
			handData->defaultRotation = 
			handData->targetRotation = a_handNode->local.rotate;
		}

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

		const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
		float xDisp = std::clamp(rsData.xComp * rsData.normMag, -1.0f, 1.0f);
		float yDisp = std::clamp(rsData.yComp * rsData.normMag, -1.0f, 1.0f);

		// Check which node rotation actions are being performed.
		bool rotatingForearm = 
		{
			a_rightArm ?
			p->pam->IsPerforming(InputAction::kRotateRightForearm) :
			p->pam->IsPerforming(InputAction::kRotateLeftForearm)
		};
		bool rotatingHand = 
		{
			a_rightArm ?
			p->pam->IsPerforming(InputAction::kRotateRightHand) :
			p->pam->IsPerforming(InputAction::kRotateLeftHand)
		};
		bool rotatingShoulder = 
		{
			a_rightArm ?
			p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
			p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
		};
		bool onlyRotatingShoulder = rotatingShoulder && !rotatingForearm && !rotatingHand;

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

		// If modified, set hand and forearm node rotation inputs and target rotations.
		const RE::NiPoint3 locXAxis{ 1.0f, 0.0f, 0.0f };
		const RE::NiPoint3 locYAxis{ 0.0f, 1.0f, 0.0f };
		const RE::NiPoint3 locZAxis{ 0.0f, 0.0f, 1.0f };
		if (forearmData->rotationModified) 
		{
			if (oldForearmPitch != targetForearmPitch)
			{
				targetForearmPitch = Util::InterpolateSmootherStep(oldForearmPitch, targetForearmPitch, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			if (oldForearmRoll != targetForearmRoll)
			{
				targetForearmRoll = Util::InterpolateSmootherStep(oldForearmRoll, targetForearmRoll, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			if (oldForearmYaw != targetForearmYaw)
			{
				targetForearmYaw = Util::InterpolateSmootherStep(oldForearmYaw, targetForearmYaw, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			forearmData->rotationInput = std::array<float, 3>({ targetForearmYaw, targetForearmPitch, targetForearmRoll });
			Util::SetRotationMatrix3
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
			forearmData->targetRotation = a_forearmNode->local.rotate;
		}
		
		if (handData->rotationModified) 
		{
			if (oldHandPitch != targetHandPitch)
			{
				targetHandPitch = Util::InterpolateSmootherStep(oldHandPitch, targetHandPitch, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			if (oldHandRoll != targetHandRoll)
			{
				targetHandRoll = Util::InterpolateSmootherStep(oldHandRoll, targetHandRoll, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			if (oldHandYaw != targetHandYaw)
			{
				targetHandYaw = Util::InterpolateSmootherStep(oldHandYaw, targetHandYaw, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			handData->rotationInput = std::array<float, 3>({ targetHandYaw, targetHandPitch, targetHandRoll });
			Util::SetRotationMatrix3
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
			handData->targetRotation = a_handNode->local.rotate;
		}

		// Update forearm and hand node blend status and data after potentially setting target rotations.
		UpdateCachedNodeRotationBlendData(forearmData, forearmNameHash, a_forearmNode, true);
		UpdateCachedNodeRotationBlendData(handData, handNameHash, a_handNode, true);
	}

	void MovementManager::UpdateCachedNodeRotationBlendData(const std::unique_ptr<NodeRotationData>& a_data, const uint32_t& a_nodeNameHash, RE::NiAVObject* a_node, bool a_isArmNode)
	{
		if (!a_data || !a_data.get() || !a_node)
		{
			return;
		}

		auto ui = RE::UI::GetSingleton();
		// Blend in flag for individual nodes.
		// Continue blending in if target rotation was modified already
		// or if now rotating the node (which will set a new target rotation).
		bool blendIn = false;
		// Not in dialogue or not controlling menus.
		bool inDialogueOrNotControllingMenus = 
		{
			(glob.menuCID != p->controllerID) ||
			(ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
		};
		if (a_isArmNode) 
		{
			// General blend in flag:
			// Can adjust when in dialogue or not controlling menus,
			// if not staggered or knocked back or swimming,
			// if weapons are out,
			// and if the player manager is running.
			// Blend out if false.
			
			blendIn = 
			{
				//inDialogueOrNotControllingMenus &&
				a_data->rotationModified &&
				coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
				!p->IsAwaitingRefresh() &&
				!coopActor->IsWeaponDrawn() &&
				!coopActor->actorState2.staggered &&
				!coopActor->IsSwimming()
			};

			// [TODO]: 
			// Figure out some way to blend in/out for arm nodes.
			// Since the rotation angle endpoints are hardcoded 
			// and there appears to be constraints imposed on the range of possble Euler angles, 
			// using separate Euler or matrix interp methods do not slowly adjust the nodes' current rotations 
			// to the target rotations while also taking the shortest/natural path. 
			// The arm nodes will swing through the body or rotate awkwardly at the moment, so for now, we'll just set the current rotation
			// directly to the target/default rotation for all arm nodes.
			if (blendIn) 
			{
				if (a_data->blendStatus != NodeRotationBlendStatus::kTargetReached) 
				{
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kTargetReached);
				}

				a_data->currentRotation = a_data->targetRotation;
			}
			else
			{
				// Weird issue that I haven't figured out yet:
				// Have to clear custom-set rotation since having the rotation modified
				// flag set leads to arm node flickering if a previous rotation was set,
				// as the game appears to warp some other arm node(s) between the default
				// and previously set custom orientations.
				nrm->ClearCustomRotation(a_nodeNameHash);
				if (a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
				{
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kDefaultReached);
				}

				a_data->currentRotation = a_data->defaultRotation;
			}
		}
		else
		{
			// General blend in flag:
			// Can adjust when in dialogue or not controlling menus,
			// if not staggered or knocked back,
			// and if the player manager is running.
			// Blend out if false.

			// Let game handle torso rotation when aiming with a bow/crossbow/spells.
			bool p1AimActive = 
			{
				p->isPlayer1 &&
				!p->coopActor->IsOnMount() && 
				p->pam->isRangedAttack
			};

			// TODO: Add torso blending while mounted.
			// Having issues with Euler angle interpolation discontinuities.
			const bool isMounted = coopActor->IsOnMount();

			// Only rotate the torso when drawing/releasing the bow/crossbow.
			bool isRangedWeaponPrimed = 
			{
				coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn ||
				coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
				coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased
			};
			// Rotate the torso throughout the attack animation's duration when not attacking with a 2H ranged weapon.
			bool isAttackingWithoutRangedWeap = !p->em->Has2HRangedWeapEquipped() && p->pam->isAttacking;
			blendIn = 
			{
				(
					//inDialogueOrNotControllingMenus &&
					!p1AimActive &&
					a_data->rotationModified &&
					coopActor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
					!p->IsAwaitingRefresh() &&
					!coopActor->actorState2.staggered
				) &&
				(
					!coopActor->IsWeaponDrawn() ||
					isRangedWeaponPrimed || 
					isAttackingWithoutRangedWeap ||
					p->pam->isBashing ||
					p->pam->isBlocking ||
					p->pam->isInCastingAnim ||
					p->pam->usingLHStaff ||
					p->pam->usingRHStaff ||
					p->mm->shouldFaceTarget
				)
			};

			if (blendIn)
			{
				if (a_data->blendStatus != NodeRotationBlendStatus::kBlendIn && a_data->blendStatus != NodeRotationBlendStatus::kTargetReached)
				{
					// Just started blending in.
					NodeRotationBlendStatus prevStatus = a_data->blendStatus;
					// Start blending in when the player first tries to rotate their arms.
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kBlendIn);
					// Set starting rotation before blending in.
					if (prevStatus == NodeRotationBlendStatus::kDefaultReached)
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend in from default.", coopActor->GetName(), a_node->name);
						
						// Starting from game's set rotation.
						a_data->startingRotation = a_data->defaultRotation;
					}
					else if (prevStatus == NodeRotationBlendStatus::kTargetReached)
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend in from target.", coopActor->GetName(), a_node->name);
						
						// Starting from our set target rotation.
						a_data->startingRotation = a_data->targetRotation;
					}
					else
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend in from current.", coopActor->GetName(), a_node->name);
						
						// Starting from the current blended rotation.
						a_data->startingRotation = a_data->currentRotation;
					}
				}

				float secsSinceBlendingIn = Util::GetElapsedSeconds(a_data->blendInTP);
				// Blend interval elapsed, so we'll now set the requested rotations to the target ones.
				if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn &&
					secsSinceBlendingIn > Settings::fSecsBlendPlayerNodeRotations)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Done blending in.", coopActor->GetName(), a_node->name);
					
					// Target rotation reached.
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kTargetReached);
				}

				// Interpolate between starting and target local rotations when blending in.
				// Set directly to the target local rotation otherwise.
				if (a_data->blendStatus == NodeRotationBlendStatus::kBlendIn && !isMounted)
				{
					// Blend out to reach the game's assigned rotation from the starting rotation.
					float t = std::clamp
					(
						secsSinceBlendingIn / Settings::fSecsBlendPlayerNodeRotations,
						0.0f,
						1.0f
					);

					float xC = 0.0f;
					float yC = 0.0f;
					float zC = 0.0f;
					float xS = 0.0f;
					float yS = 0.0f;
					float zS = 0.0f;
					float xT = 0.0f;
					float yT = 0.0f;
					float zT = 0.0f;
					a_data->startingRotation.ToEulerAnglesXYZ(xS, yS, zS);
					a_data->targetRotation.ToEulerAnglesXYZ(xT, yT, zT);
					xC = -Util::InterpolateSmootherStep(xS, xT, t);
					yC = Util::InterpolateSmootherStep(yS, yT, t);
					zC = -Util::InterpolateSmootherStep(zS, zT, t);
					a_data->currentRotation.SetEulerAnglesXYZ(xC, yC, zC);

					// REMOVE when done debugging.
					/*ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: BLEND IN: ({}, {}, {}) -> ({}, {}, {}) -> ({}, {}, {}).", 
						coopActor->GetName(), a_node->name,
						xS * TO_DEGREES, yS * TO_DEGREES, zS * TO_DEGREES,
						xC * TO_DEGREES, yC * TO_DEGREES, zC * TO_DEGREES,
						xT * TO_DEGREES, yT * TO_DEGREES, zT * TO_DEGREES);*/
				}
				else
				{
					a_data->currentRotation = a_data->targetRotation;
				}
			}
			else
			{
				if (a_data->blendStatus != NodeRotationBlendStatus::kBlendOut && a_data->blendStatus != NodeRotationBlendStatus::kDefaultReached)
				{
					NodeRotationBlendStatus prevStatus = a_data->blendStatus;
					// Start blending out if not already blending out and if the default rotation was not reached.
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kBlendOut);
					// Set starting rotation before blending out.
					if (prevStatus == NodeRotationBlendStatus::kDefaultReached)
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend out from default.", coopActor->GetName(), a_node->name);
						
						// Starting from game's set rotation.
						a_data->startingRotation = a_data->defaultRotation;
					}
					else if (prevStatus == NodeRotationBlendStatus::kTargetReached && a_data->rotationModified)
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend out from target.", coopActor->GetName(), a_node->name);
						
						// Starting from our set target rotation.
						a_data->startingRotation = a_data->targetRotation;
					}
					else
					{
						// REMOVE when done debugging.
						//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Start blend out from current.", coopActor->GetName(), a_node->name);
						
						// Starting from the current blended rotation.
						a_data->startingRotation = a_data->currentRotation;
					}
				}

				float secsSinceBlendingOut = Util::GetElapsedSeconds(a_data->blendOutTP);
				// Fully blended out.
				if (a_data->blendStatus == NodeRotationBlendStatus::kBlendOut &&
					Util::GetElapsedSeconds(a_data->blendOutTP) > Settings::fSecsBlendPlayerNodeRotations)
				{
					// REMOVE when done debugging.
					//ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: Done blending out.", coopActor->GetName(), a_node->name);
					
					// Default rotation reached.
					nrm->SetBlendStatus(a_nodeNameHash, NodeRotationBlendStatus::kDefaultReached);
				}

				// Interpolate between starting and game-given default local rotations when blending out.
				// Set directly to the default local rotation otherwise.
				if (a_data->blendStatus == NodeRotationBlendStatus::kBlendOut && !isMounted)
				{
					// Blend out to reach the game's assigned rotation from the starting rotation.
					float t = std::clamp
					(
						secsSinceBlendingOut / Settings::fSecsBlendPlayerNodeRotations,
						0.0f,
						1.0f
					);

					float xC = 0.0f;
					float yC = 0.0f;
					float zC = 0.0f;
					float xS = 0.0f;
					float yS = 0.0f;
					float zS = 0.0f;
					float xD = 0.0f;
					float yD = 0.0f;
					float zD = 0.0f;
					a_data->startingRotation.ToEulerAnglesXYZ(xS, yS, zS);
					a_data->defaultRotation.ToEulerAnglesXYZ(xD, yD, zD);
					xC = -Util::InterpolateSmootherStep(xS, xD, t);
					yC = Util::InterpolateSmootherStep(yS, yD, t);
					zC = -Util::InterpolateSmootherStep(zS, zD, t);
					a_data->currentRotation.SetEulerAnglesXYZ(xC, yC, zC);

					// REMOVE when done debugging.
					/*ALYSLC::Log("[MM] UpdateCachedNodeRotationBlendData: {}: {}: BLEND OUT: ({}, {}, {}) -> ({}, {}, {}) -> ({}, {}, {}).",
						coopActor->GetName(), a_node->name,
						xS * TO_DEGREES, yS * TO_DEGREES, zS * TO_DEGREES,
						xC * TO_DEGREES, yC * TO_DEGREES, zC * TO_DEGREES,
						xD * TO_DEGREES, yD * TO_DEGREES, zD * TO_DEGREES);*/
				}
				else
				{
					a_data->currentRotation = a_data->defaultRotation;
				}
			}

		}
	}

	void MovementManager::UpdateCachedShoulderNodeRotationData(RE::NiAVObject* a_shoulderNode, bool a_rightShoulder)
	{
		if (!a_shoulderNode || !a_shoulderNode->collisionObject)
		{
			return;
		}

		/*auto collisionObject = static_cast<RE::bhkCollisionObject*>(a_shoulderNode->collisionObject.get());
		auto rigidBody = collisionObject->GetRigidBody();
		if (!rigidBody || !rigidBody->referencedObject)
		{
			return;
		}*/

		const auto shoulderNameHash = Hash(a_shoulderNode->name);
		// Ensure the shoulder node is accounted for in rotation data map.
		if (!nrm->nodeRotationDataMap.contains(shoulderNameHash))
		{
			nrm->nodeRotationDataMap.insert_or_assign(shoulderNameHash, std::make_unique<NodeRotationData>());
		}

		// Rotation data we will modify.
		auto& shoulderData = nrm->nodeRotationDataMap[shoulderNameHash];
		// Set initial starting/current/target rotations if not set yet.
		const auto identityMat = RE::NiMatrix3();
		if (shoulderData->startingRotation == identityMat ||
			shoulderData->currentRotation == identityMat ||
			shoulderData->defaultRotation == identityMat || 
			shoulderData->targetRotation == identityMat)
		{
			shoulderData->startingRotation =
			shoulderData->currentRotation =
			shoulderData->defaultRotation = 
			shoulderData->targetRotation = a_shoulderNode->local.rotate;
		}

		// New rotation inputs to construct target rotation matrix with (if adjusted below).
		std::array<float, 3> newRotationInput = shoulderData->rotationInput;
		bool rotatingShoulder = 
		{
			a_rightShoulder ?
			p->pam->IsPerforming(InputAction::kRotateRightShoulder) :
			p->pam->IsPerforming(InputAction::kRotateLeftShoulder)
		};
		if (rotatingShoulder) 
		{
			// Set shoulder target rotation as modified.
			shoulderData->rotationModified = true;

			// Pick RS quadrant/axis based on its displacement.
			const auto& rsData = glob.cdh->GetAnalogStickState(controllerID, false);
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

			const auto& leftShoulderMatAngleInputs = nrm->leftShoulderMatAngleInputs;
			const auto& rightShoulderMatAngleInputs = nrm->rightShoulderMatAngleInputs;
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

		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);
		// REMOVE when done debugging.
		/*
		const auto& worldRot = a_shoulderNode->world.rotate;
		auto worldXAxis = RE::NiPoint3(worldRot * right);
		auto worldYAxis = RE::NiPoint3(worldRot * forward);
		auto worldZAxis = RE::NiPoint3(worldRot * up);
		glm::vec3 start{ a_shoulderNode->world.translate.x, a_shoulderNode->world.translate.y, a_shoulderNode->world.translate.z };
		glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 15.0f };
		glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 15.0f };
		glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 15.0f };
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
			float yaw = Util::InterpolateSmootherStep(oldYaw, newRotationInput[0], min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			float pitch = Util::InterpolateSmootherStep(oldPitch, newRotationInput[1], min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			shoulderData->rotationInput = std::array<float, 3>({ yaw, pitch, 0.0f });
			// Set local rotation from new world transform and inverse of parent transform.
			auto newWorldTransform = a_shoulderNode->world;
			Util::SetRotationMatrix2
			(
				newWorldTransform.rotate, 
				shoulderData->rotationInput[1], 
				shoulderData->rotationInput[0] + coopActor->GetHeading(false), 
				0.0f
			);
			if (a_shoulderNode->parent)
			{
				RE::NiTransform inverseParent = a_shoulderNode->parent->world.Invert();
				shoulderData->targetRotation = (inverseParent * newWorldTransform).rotate;
				a_shoulderNode->world.rotate = newWorldTransform.rotate;
				// Update rotation inputs after successfully setting new target rotation.
			}
		}
		else
		{
			shoulderData->targetRotation = shoulderData->defaultRotation;
		}

		// Update shoulder node blend status and data after potentially setting target rotations.
		UpdateCachedNodeRotationBlendData(shoulderData, shoulderNameHash, a_shoulderNode, true);
	}

	void MovementManager::UpdateCachedTorsoNodeRotationData()
	{
		const auto strings = RE::FixedStrings::GetSingleton();
		if (!strings)
		{
			return;
		}

		auto loadedData = coopActor->loadedData;
		if (!loadedData)
		{
			return;
		}

		auto data3D = loadedData->data3D;
		if (!data3D || !data3D->parent)
		{
			return;
		}

		// KNOWN ISSUES:
		// As of now, I haven't figure out how to directly set the world rotation matrices for the player's spinal nodes.
		// Has led to indirectly adjusting rotation via the local rotation matrices instead.
		// Upper spinal nodes begin to twist when pitch/roll nears 90 degrees.
		// Probably related to headtracking and the above local rotation workaround.
		const RE::NiPoint3 up = RE::NiPoint3(0.0f, 0.0f, 1.0f);
		const RE::NiPoint3 forward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
		const RE::NiPoint3 right = RE::NiPoint3(1.0f, 0.0f, 0.0f);
		auto weaponNode = coopActor->loadedData->data3D->GetObjectByName(strings->weapon);
		auto rootNode = coopActor->loadedData->data3D->GetObjectByName(strings->npcRoot);
		bool isAimingWithRangedWeapon = 
		{
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn || 
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleasing ||
			coopActor->actorState1.meleeAttackState == RE::ATTACK_STATE_ENUM::kBowReleased
		};
		bool isMounted = coopActor->IsOnMount();
		// Rotation angle inputs to set if adjusted.
		float pitch = 0.0f;
		float roll = 0.0f;
		float yaw = 0.0f;
		float playerFacingAng = coopActor->data.angle.z;
		// Interpolation factor for pitch.
		// Roll factor is pitchFactor - 1.
		// Yaw handled separately.
		float pitchFactor = 1.0f;
		// Ratio of total aim pitch by which to rotate each node.
		float div = 3.0f;
		// Skip head node adjustments for the time being.
		const uint8_t numAdjustableNodes = 
		(
			p->isTransformed ? 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size() - 1 : 
			GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES.size()
		);
		for (uint8_t i = 0; i < numAdjustableNodes; ++i)
		{
			const auto& nodeName = GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES[i];
			auto torsoNode = RE::NiPointer<RE::NiAVObject>(data3D->GetObjectByName(nodeName)); 
			// Node is invalid, on to the next one.
			if (!torsoNode || !torsoNode.get())
			{
				continue;
			}
				
			const auto nodeNameHash = Hash(nodeName);
			// Ensure the torso node is accounted for in rotation data map.
			if (!nrm->nodeRotationDataMap.contains(nodeNameHash))
			{
				nrm->nodeRotationDataMap.insert_or_assign(nodeNameHash, std::make_unique<NodeRotationData>());
			}

			// Rotation data we will modify.
			auto& torsoData = nrm->nodeRotationDataMap[nodeNameHash];
			// Set initial starting/current/target rotations if not set yet.
			const auto identityMat = RE::NiMatrix3();
			if (torsoData->startingRotation == identityMat ||
				torsoData->currentRotation == identityMat ||
				torsoData->defaultRotation == identityMat ||
				torsoData->targetRotation == identityMat)
			{
				torsoData->startingRotation =
				torsoData->currentRotation =
				torsoData->defaultRotation = 
				torsoData->targetRotation = torsoNode->local.rotate;
			}

			const auto& worldRot = torsoNode->world.rotate;
			auto worldXAxis = RE::NiPoint3(worldRot * right);
			auto worldYAxis = RE::NiPoint3(worldRot * forward);
			auto worldZAxis = RE::NiPoint3(worldRot * up);

			if ((p->pam->IsPerforming(InputAction::kAdjustAimPitch)) || 
				(isMounted && p->pam->isRangedWeaponAttack))
			{
				// Set torso target rotation as modified.
				torsoData->rotationModified = true;
			}

			// Aim pitch is modified externally and drawing/sheathing weapons
			// requires recalculation of node pitch/yaw/roll angles based on the player's adjusted aim pitch,
			// even if none of the torso nodes are being actively adjusted.

			// Pitching nodes up/down when attacking with a ranged weapon also uses the roll angle to varying degrees (no pun intended).
			// If we only modify the node's pitch angle, the player will pitch forward or backward at up to 90 degrees,
			// but not necessarily in the direction in which they are attacking,
			// since facing angle and attacking angle do not match, depending on the attack animation.
			// Only adjusted when mounted.
			auto localForwardXY = RE::NiPoint3(torsoNode->local.rotate * forward);
			localForwardXY.z = 0.0f;
			localForwardXY.Unitize();
			yaw = Util::NormalizeAngToPi(Util::GetYawBetweenPositions(RE::NiPoint3(), localForwardXY));

			bool isSpinalRoot = nodeNameHash == Hash(GlobalCoopData::TORSO_ADJUSTMENT_NPC_NODES[0]);
			if (isSpinalRoot)
			{
				/*
				float yawDiff = 0.0f;
				float nodeHeading = Util::DirectionToGameAngYaw(worldYAxis);
				if (bool weaponDrawn = coopActor->IsWeaponDrawn(); weaponDrawn && weaponNode && isAimingWithRangedWeapon)
				{
					auto playerFacingDir = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(playerFacingAng));
					// Difference between facing and attacking angles.
					yawDiff = Util::NormalizeAngToPi(Util::DirectionToGameAngYaw(playerFacingDir) - nodeHeading);
					float aimHeading = 0.0f;
					coopActor->GetGraphVariableFloat("AimHeadingCurrent", aimHeading);
					yawDiff = Util::NormalizeAngToPi(Util::NormalizeAng0To2Pi(coopActor->data.angle.z + aimHeading) - nodeHeading);
					// Aim straight up with the arrow node when player heading and arrow node directions are oriented at > 90 degrees.
					// Prevents stuttering when adjusting pitch near this endpoint.
					// Stuttering reason: player heading direction flips 180 degrees when the aim pitch goes above PI / 2 in magnitude.
					// Since we rotate the player so that their heading direction faces their target,
					// they will turn 180 degrees to face the target.
					if (playerFacingDir.Dot(weaponNode->world.rotate * forward) < 0.0f)
					{
						// Identity matrix.
						weaponNode->world.rotate = RE::NiMatrix3();
					}

				}
				else if (coopActor->IsAttacking())
				{
					if (shouldFaceTarget || Util::HandleIsValid(p->tm->crosshairRefrHandle))
					{
						yawDiff = Util::NormalizeAngToPi(Util::GetYawBetweenPositions(playerAttackSourcePos, p->tm->crosshairWorldPos) - nodeHeading);
					}
					else
					{
						yawDiff = Util::NormalizeAngToPi(playerFacingAng - nodeHeading);
					}
				}
				else
				{
					yawDiff = 0.0f;
				}

				// The smaller the yaw difference between attacking angle and player facing angle,
				// the more we rotate along the pitch axis, as opposed to the roll axis.
				pitchFactor = 1.0f - (fabsf(std::clamp(yawDiff, -PI / 4.0f, PI / 4.0f)) / (PI / 4.0f));
				*/

				
			}

			if (i == 0) 
			{
				div = 2.0f;
			}
			else if (i == 1)
			{
				div = 3.0f;
			}
			else if (i == 2)
			{
				div = 6.0f;
			}

			// Temporarily clamped to prevent Euler angle jumps when near interpolating +- 90 degrees.
			float clampedAimPitch = std::clamp(aimPitch, -89.0f * PI / 180.0f, 89.0f * PI / 180.0f);
			pitchFactor = fabsf(aimPitch / (PI / 2.0f));

			// TODO: Find proper axis of rotation for each unique attack animation.
			// Hardcoded approximations for the vanilla ranged attack animations will have to do for now.
			if (p->pam->isRangedWeaponAttack)
			{
				if (p->em->HasBowEquipped())
				{
					pitch = 
					(
						coopActor->IsSneaking() ? 
						0.55f * (clampedAimPitch / div) :
						(PI / 8.0f) / div
					);
					roll = 
					(
						coopActor->IsSneaking() ?
						-0.45f * (clampedAimPitch / div) :
						-1.0f * (clampedAimPitch / div) 
					);
					yaw = 
					(
						coopActor->IsSneaking() ?
						(PI / 10.0f) / div :
						0.0f
					);
				}
				else
				{
					pitch = 0.6f * (clampedAimPitch / div);
					roll = -0.4f * (clampedAimPitch / div);
					yaw = 
					(
						coopActor->IsSneaking() ? 
						(PI / 15.0f) / div : 
						(PI / 7.0f) / div
					);
				}
			}
			else if (p->pam->isInCastingAnim)
			{
				pitch = (clampedAimPitch / div);
				roll = 0.0f;
				yaw = isSpinalRoot ? -PI / 6.0f : 0.0f;
			}
			else
			{
				pitch = (clampedAimPitch / div);
				roll = 0.0f;
				yaw = 0.0f;
			}


			float oldPitch = torsoData->rotationInput[1];
			float oldYaw = torsoData->rotationInput[0];
			float oldRoll = torsoData->rotationInput[2];
			pitch = Util::InterpolateSmootherStep(oldPitch, pitch, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			roll = Util::InterpolateSmootherStep(oldRoll, roll, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));

			/*pitch = Util::InterpolateSmootherStep(oldPitch, pitchFactor * (aimPitch / div), min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			roll = Util::InterpolateSmootherStep(oldRoll, (pitchFactor - 1.0f) * (aimPitch / div), min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			yaw = Util::InterpolateSmootherStep(oldYaw, yaw, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));*/

			// Sooo, this took way too long.
			// Definitely not polished and has issues when moving away from a target while aiming or when pitching
			// too high or low.
			// Also hourglass torso mode when rotated > 90 degrees in either direction.
			// Haven't figured out the math yet because there is no proper heading angle to base calculations off of while mounted.
			// The player's heading angle is the same as the horse's while mounted and ranged weapon aim yaw is at a variable angular
			// offset from the player's facing direction that I can't seem to pinpoint, so I've estimated this angular offset for bows and crossbows.
			// Good enough for now.
			if (isMounted)
			{
				float newYaw = oldYaw;
				float angDiffScalingInterpFactor = 1.0f;
				if (coopActor->IsWeaponDrawn() && weaponNode && rootNode && p->pam->isRangedWeaponAttack && nodeName == strings->npcSpine)
				{
					// Face the target.
					if (auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(p->tm->crosshairRefrHandle); crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get()))
					{
						auto playerHeadingDir = rootNode->world.rotate * forward;
						// Not the same as the game's returned actor heading angle.
						playerFacingAng = Util::DirectionToGameAngYaw(playerHeadingDir);
						// If there were a way to get the fully drawn arrow's heading angle before it's fully drawn, I would.
						float playerToTargetYaw = Util::DirectionToGameAngYaw(p->tm->crosshairWorldPos - weaponNode->world.translate);
						float headingToTargetDiff = Util::NormalizeAngToPi(playerToTargetYaw - playerFacingAng);

						// Scale down interpolation factor when the weapon node is close to the target or when the yaw change is large.
						angDiffScalingInterpFactor = (1.1f - fabsf(newYaw - oldYaw) / (2.0f * PI));
						float xyDistToTarget = Util::GetXYDistance(weaponNode->world.translate, p->tm->crosshairWorldPos);
						// Can't compute yaw offset from player facing angle (unknown) to aim node yaw angle.
						float distScalingDiffFactor = Util::InterpolateEaseInEaseOut(0.0f, 1.0f, min(xyDistToTarget, Settings::fTargetAttackSourceDistToSlowRotation) / (Settings::fTargetAttackSourceDistToSlowRotation + 0.01f), 5.0f);
						// Scale down yaw diff if close to target to prevent wild swings in yaw.
						// Ugly as heck, I know.
						newYaw = Util::NormalizeAngToPi(oldYaw + distScalingDiffFactor * (Util::NormalizeAngToPi(headingToTargetDiff + (p->em->HasBowEquipped() ? PI / 3.0f : PI / 4.0f) - oldYaw)));

						// Frequent sign changes lead to spasmodic oscillation.
						// To void this, don't interpolate on sign change and instead directly set to the new yaw.
						// Side effect: torso twists into an hourglass near 180 degrees.
						// Oh well.
						float sign = newYaw < 0.0f ? -1.0f : 1.0f;
						if ((oldYaw * sign) < 0.0f)
						{
							oldYaw = newYaw;
						}
					}
					else
					{
						// Face forward by adding estimated offset.
						newYaw = p->em->HasBowEquipped() ? PI / 3.0f : PI / 4.0f;
					}
				}
				else
				{
					newYaw = 0.0f;
				}

				yaw = Util::InterpolateSmootherStep(oldYaw, newYaw, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor * angDiffScalingInterpFactor));
			}
			else
			{
				yaw = Util::InterpolateSmootherStep(oldYaw, yaw, min(1.0f, *g_deltaTimeRealTime * nrm->interpFactor));
			}

			// Save the 3 rotation angle inputs for use in constructing the target rotation matrix.
			torsoData->rotationInput = std::array<float, 3>({ yaw, pitch, roll });

			// REMOVE when done debugging.
			/*glm::vec3 start{ torsoNode->world.translate.x, torsoNode->world.translate.y, torsoNode->world.translate.z };
			glm::vec3 endX{ start + glm::vec3(worldXAxis.x, worldXAxis.y, worldXAxis.z) * 15.0f };
			glm::vec3 endY{ start + glm::vec3(worldYAxis.x, worldYAxis.y, worldYAxis.z) * 15.0f };
			glm::vec3 endZ{ start + glm::vec3(worldZAxis.x, worldZAxis.y, worldZAxis.z) * 15.0f };
			DebugAPI::QueueArrow3D(start, endX, 0xFF0000FF, 5.0f, 2.0f);
			DebugAPI::QueueArrow3D(start, endY, 0x00FF00FF, 5.0f, 2.0f);
			DebugAPI::QueueArrow3D(start, endZ, 0x0000FFFF, 5.0f, 2.0f);*/

			if (torsoData->rotationModified) 
			{
				RE::NiPoint3 locXAxis{ right };
				RE::NiPoint3 locYAxis{ forward };
				RE::NiPoint3 locZAxis{ up };
				Util::SetRotationMatrix3
				(
					torsoData->targetRotation, 
					locXAxis, 
					locYAxis, 
					locZAxis, 
					torsoData->rotationInput[1],
					torsoData->rotationInput[0], 
					torsoData->rotationInput[2]
				);
			}
			else
			{
				torsoData->targetRotation = torsoNode->local.rotate;
			}

			// Update torso node blend status and data after potentially setting target rotations.
			UpdateCachedNodeRotationBlendData(torsoData, nodeNameHash, torsoNode.get(), false);
		}
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
			speedMult = attackMovMult * baseSpeedMult * Settings::fSprintSpeedMult;
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
			if (wasGettingUp && knockState == RE::KNOCK_STATE_ENUM::kNormal)
			{
				p->lastGetupTP = SteadyClock::now();
				shouldCurtailMomentum = true;
			}

			isGettingUp = false;
		}

		// Freeze the player in place and wait until their reported movement speed is 0.
		shouldCurtailMomentum &= coopActor->DoGetMovementSpeed() > 0.0f;

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
						charController->fallStartHeight = 0.0f;
						charController->fallTime = 0.0f;
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

					shouldResetAimPitch = true;
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
			const float actorMovementSpeed = movementActor->DoGetMovementSpeed();
			bool isMoving = 
			{
				(actorMovementSpeed > 0.0f) &&
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

	void NodeRotationManager::InstantlyResetAllNodeData()
	{
		nodeRotationDataMap.clear();
		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODE_HASHES) 
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}

		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODE_HASHES)
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}

		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_TORSO_NODE_HASHES)
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}
	}

	void NodeRotationManager::InstantlyResetArmNodeData()
	{
		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_LEFT_ARM_NODE_HASHES)
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}

		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_RIGHT_ARM_NODE_HASHES)
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}
	}

	void NodeRotationManager::InstantlyResetTorsoNodeData()
	{
		for (const auto& nodeHash : GlobalCoopData::ADJUSTABLE_TORSO_NODE_HASHES)
		{
			nodeRotationDataMap.insert_or_assign(nodeHash, std::make_unique<NodeRotationData>());
		}
	}

	bool NodeRotationManager::NodeWasAdjusted(const uint32_t& a_nodeNameHash)
	{
		if (nodeRotationDataMap.contains(a_nodeNameHash)) 
		{
			if (const auto& data = nodeRotationDataMap.at(a_nodeNameHash); data && data.get()) 
			{
				return data->rotationModified;
			}
		}

		return false;
	}

	void NodeRotationManager::SetBlendStatus(const uint32_t& a_nodeNameHash, NodeRotationBlendStatus&& a_newStatus)
	{
		if (nodeRotationDataMap.contains(a_nodeNameHash)) 
		{
			auto& data = nodeRotationDataMap.at(a_nodeNameHash); 
			if (!data || !data.get())
			{
				return;
			}

			// REMOVE when done debugging.
			//ALYSLC::Log("[MM] SetBlendStatus: {} -> {}.", data->blendStatus, a_newStatus);
			
			if (a_newStatus == NodeRotationBlendStatus::kBlendIn)
			{
				data->blendInTP = SteadyClock::now();
			}
			else if (a_newStatus == NodeRotationBlendStatus::kBlendOut)
			{
				data->blendOutTP = SteadyClock::now();
			}

			data->blendStatus = a_newStatus;
		}
	}
}

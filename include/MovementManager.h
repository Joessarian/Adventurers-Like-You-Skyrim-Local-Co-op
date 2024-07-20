#pragma once
#include <cstdint>
#include <vector>
#include <Player.h>

namespace ALYSLC 
{
	using SteadyClock = std::chrono::steady_clock;
	class CoopPlayer;

	// Handles player movement, actions related to movement, and node orientation.
	struct MovementManager : public Manager
	{
		// Stores player body node rotation info to restore in the NiNode::UpdateDownwardPass() hook.
		struct NodeRotationManager
		{
			NodeRotationManager() 
			{
				Reset();
			}

			NodeRotationManager& operator=(const NodeRotationManager& a_nrm)
			{
				lastRecordedArmLocalVelocities = a_nrm.lastRecordedArmLocalVelocities;
				lastSetArmLocalPositions = a_nrm.lastSetArmLocalPositions;
				lastSetArmLocalRotations = a_nrm.lastSetArmLocalRotations;
				lastSetArmRotationInputs = a_nrm.lastSetArmRotationInputs;
				lastSetTorsoLocalRotations = a_nrm.lastSetTorsoLocalRotations;
				lastSetTorsoRotationInputs = a_nrm.lastSetTorsoRotationInputs;
				restoreArmNodeRotations = a_nrm.restoreArmNodeRotations;
				restoreTorsoNodeRotations = a_nrm.restoreTorsoNodeRotations;
				shouldBlendIn = a_nrm.shouldBlendIn;
				shouldBlendOut = a_nrm.shouldBlendOut;
				blendInTP = a_nrm.blendInTP;
				blendOutTP = a_nrm.blendOutTP;

				return *this;
			}

			NodeRotationManager& operator=(NodeRotationManager&& a_nrm)
			{
				lastRecordedArmLocalVelocities = std::move(a_nrm.lastRecordedArmLocalVelocities);
				lastSetArmLocalPositions = std::move(a_nrm.lastSetArmLocalPositions);
				lastSetArmLocalRotations = std::move(a_nrm.lastSetArmLocalRotations);
				lastSetArmRotationInputs = std::move(a_nrm.lastSetArmRotationInputs);
				lastSetTorsoLocalRotations = std::move(a_nrm.lastSetTorsoLocalRotations);
				lastSetTorsoRotationInputs = std::move(a_nrm.lastSetTorsoRotationInputs);
				restoreArmNodeRotations = std::move(a_nrm.restoreArmNodeRotations);
				restoreTorsoNodeRotations = std::move(a_nrm.restoreTorsoNodeRotations);
				shouldBlendIn = std::move(a_nrm.shouldBlendIn);
				shouldBlendOut = std::move(a_nrm.shouldBlendOut);
				blendInTP = std::move(a_nrm.blendInTP);
				blendOutTP = std::move(a_nrm.blendOutTP);

				return *this;
			}

			// Reset all node data.
			inline void Reset() 
			{
				std::unique_lock<std::mutex> lock(rotationDataMutex, std::try_to_lock);
				if (lock)
				{
					lastRecordedArmLocalVelocities.clear();
					lastSetArmLocalPositions.clear();
					lastSetArmLocalRotations.clear();
					lastSetArmRotationInputs.clear();
					lastSetTorsoRotationInputs.clear();
					lastSetTorsoLocalRotations.clear();
					blendOutTP = blendInTP = SteadyClock::now();
					shouldBlendIn = shouldBlendOut = false;
				}
			}

			// Only reset limb node data.
			inline void ResetLimbNodeOrientationData() 
			{
				std::unique_lock<std::mutex> lock(rotationDataMutex, std::try_to_lock);
				if (lock)
				{
					lastRecordedArmLocalVelocities.clear();
					lastSetArmLocalPositions.clear();
					lastSetArmLocalRotations.clear();
					lastSetArmRotationInputs.clear();
				}
			}

			// Only reset torso node data.
			inline void ResetTorsoNodeRotationData()
			{
				std::unique_lock<std::mutex> lock(rotationDataMutex, std::try_to_lock);
				if (lock)
				{
					lastSetTorsoRotationInputs.clear();
					lastSetTorsoLocalRotations.clear();
				}
			}

			// TODO: Blend node rotations in/out to match player-altered/default rotations.
			inline void SetShouldBlend(const bool& a_blendIn) 
			{
				std::unique_lock<std::mutex> lock(rotationDataMutex, std::try_to_lock);
				if (lock)
				{
					shouldBlendIn = a_blendIn;
					shouldBlendOut = !a_blendIn;
					if (a_blendIn)
					{
						blendInTP = SteadyClock::now();
					}
					else
					{
						blendOutTP = SteadyClock::now();
					}
				}
			}

			// Not sure of the game's Euler convention and how to account for the changes made
			// to nodes' world rotation matrices by changing the corresponding local rotation matrices.
			// So after a dozen hours of getting nowhere mathematically, 
			// here are manually tested boundary rotation angles for blending.
			// (RS X * 2PI, RS Y * 2PI, fixed Z), both radians and (degrees).
			// LEFT SHOULDER (Flip X sign for RIGHT SHOULDER)
			// 
			// UPWARD [ 1.8315454 (104.93981), -0.23093452 (-13.231572), 0 (0)] // +BACK [3.3239334 (190.44734), 0.8360221 (47.900536), 0 (0)]
			// DOWNWARD [2.4250095 (138.9428), -3.1119285 (-178.30035), 0 (0)]
			// INWARD [3.8038778 (217.94614), -1.8690848 (-107.09067), 0 (0)]
			// OUTWARD [0.34484762 (19.758312), -1.7245883 (-98.81162), 0 (0)]
			// FORWARD [2.9678023 (170.04254), -1.7808087 (-102.032814), 0 (0)]
			// BACKWARD FROM BOTTOM [3.5985053 (206.17915), -4.5647597 (-261.54144), 0 (0)]
			// BACKWARD FROM TOP [0.3231819 (18.516958), -1.531915 (-87.772255), 0 (0)]
			const std::unordered_map<ArmOrientation, std::array<float, 3>> leftShoulderMatAngleInputs = {
				{ ArmOrientation::kUpward, { 1.8315454f, -0.23093452f, 0.0f } },
				{ ArmOrientation::kDownward, { 2.4250095f, -3.1119285f, 0.0f } },
				{ ArmOrientation::kInward, { 3.8038778f, -1.8690848f, 0.0f } },
				{ ArmOrientation::kOutward, { 0.34484762, -1.7245883, 0.0f } },
				{ ArmOrientation::kForward, { 2.9678023f, -1.7808087f, 0.0f } },
				{ ArmOrientation::kBackward, { 3.5985053f, -4.5647597f, 0.0f } }
			};

			const std::unordered_map<ArmOrientation, std::array<float, 3>> rightShoulderMatAngleInputs = {
				{ ArmOrientation::kUpward, { -1.8315454f, -0.23093452f, 0.0f } },
				{ ArmOrientation::kDownward, { -2.4250095f, -3.1119285f, 0.0f } },
				{ ArmOrientation::kInward, { -3.8038778f, -1.8690848f, 0.0f } },
				{ ArmOrientation::kOutward, { -0.34484762, -1.7245883, 0.0f } },
				{ ArmOrientation::kForward, { -2.9678023f, -1.7808087f, 0.0f } },
				{ ArmOrientation::kBackward, { -3.5985053f, -4.5647597f, 0.0f } }
			};

			// Node rotation interpolation factor.
			// Higher values inch closer to directly setting the current rotation to the target rotation.
			// Lower values create more sluggish, but smooother, movement.
			const float interpFactor = 20.0f;

			// Last blend request time points.
			SteadyClock::time_point blendInTP;
			SteadyClock::time_point blendOutTP;

			// Mutex for setting rotation data.
			std::mutex rotationDataMutex;
			// Last recorded shoulder/forearm/hand node velocities relative to the player's root position. Indexed by node name hash.
			std::unordered_map<uint32_t, RE::NiPoint3> lastRecordedArmLocalVelocities;
			// Last set shoulder/forearm/hand node local positions relative to the player's root position. Indexed by node name hash.
			std::unordered_map<uint32_t, RE::NiPoint3> lastSetArmLocalPositions;
			// Last set shoulder/forearm/hand node local rotation matrix. Indexed by node name hash.
			std::unordered_map<uint32_t, RE::NiMatrix3> lastSetArmLocalRotations;
			// Indexed by node name hash. List of three angles (yaw, pitch, roll)
			// Derived from right stick X, Y displacement (yaw, pitch), and const (roll).
			std::unordered_map<uint32_t, std::array<float, 3>> lastSetArmRotationInputs;
			// Last set torso local rotation matrix. Indexed by node name hash.
			std::unordered_map<uint32_t, RE::NiMatrix3> lastSetTorsoLocalRotations;
			// Indexed by node name hash. List of three angles (pitch, yaw, roll).
			std::unordered_map<uint32_t, std::array<float, 3>> lastSetTorsoRotationInputs;

			// Should cached arm/torso node rotations be restored on downward pass?
			// Updated in havok physics pre-step callback.
			bool restoreArmNodeRotations;
			bool restoreTorsoNodeRotations;
			// TODO: Blend rotations in/out.
			// Should blend in/out.
			bool shouldBlendIn;
			bool shouldBlendOut;

		};

		MovementManager();
		// Delayed construction after the player is default-constructed 
		// and the player shared pointer is added to the list of co-op players in the global data holder.
		void Initialize(std::shared_ptr<CoopPlayer> a_p);

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		//
		// Member funcs
		//

		// Clear movement offset for player actor/mount.
		void ClearKeepOffsetFromActor();
		
		// Get the context-based Z rotation speed multiplier for the player.
		float GetRotationMult();
		
		// Keep movement offset between player actor/mount and target.
		void KeepOffsetFromActor(const RE::ActorHandle& a_targetHandle, const RE::NiPoint3& a_posOffset, const RE::NiPoint3& a_angOffset, float a_catchUpRadius, float a_followRadius);
		
		// Perform a dash dodge, if requested and no dodge mods are installed.
		void PerformDashDodge();
		
		// Have the player perform a jump.
		void PerformJump();

		// Fear not companion players, we have dollar store-brand paragliding at home.
		// Requires Loki's mod:
		// https://www.nexusmods.com/skyrimspecialedition/mods/53256
		// Code adapted to from here:
		// https://github.com/LXIV-CXXVIII/Skyrims-Paraglider/blob/main/src/main.cpp
		// And P1 must have the paraglider already.
		void PerformMagicalParaglide();

		// Reset all time points to the current time.
		void ResetTPs();
		
		// Set casting magic nodes and aim pitch/heading to aim at any targeted object.
		void SetAimRotation();
		
		// Set worldspace attack source position and direction.
		void SetAttackSourceOrientationData();
		
		// Set/unset don't move flag for current movement actor.
		void SetDontMove(bool&& a_set);
		
		// Set head-tracking target position for the player.
		void SetHeadTrackTarget();
		
		// Set player target rotation and movement offset to begin/stall movement and rotation.
		void SetPlayerOrientation();
		
		// Check if P1 is entering a new undiscovered location 
		// and attempt to discover it by unsetting the AI driven flag briefly.
		void SetShouldPerformLocationDiscovery();
		
		// Update aim pitch (torso) angle and position.
		void UpdateAimPitch();
		
		// Update movement parameters based on controller input.
		void UpdateMovementParameters();
		
		// Check if the player should stop/start moving, 
		// set appropriate AI/animation driven flags for player 1,
		// update mounted/jump/swim/dodging states, or fix ragdoll state.
		void UpdateMovementState();

		//
		// Members
		//
		 
		// The player.
		std::shared_ptr<CoopPlayer> p;
		// The player's character.
		RE::ActorPtr coopActor;
		// Actor to keep a movement offset from.
		// Either the player actor themselves or the current mount.
		RE::ActorPtr movementActor;
		// Last aim pitch position.
		RE::NiPoint3 aimPitchPos;
		// World position at which to start running the player's interaction package.
		RE::NiPoint3 interactionPackageEntryPos;
		// Player attack source's direction.
		RE::NiPoint3 playerAttackSourceDir;
		// Player attack source world position.
		RE::NiPoint3 playerAttackSourcePos;
		// Manages saved rotation data for this player's nodes.
		std::unique_ptr<NodeRotationManager> nrm;
		// Ten floats: 
		// Left stick xOffset, 
		// Left stick yOffset, 
		// Right stick xOffset, 
		// Right stick yOffset, 
		// SpeedMult to set, 
		// Left stick in-game Z angle (left stick absolute game angle + cam angle), 
		// Right stick in-game Z angle (right stick absolute game angle + cam angle),
		// Change in left stick absolute game angle since the last iteration, 
		// Absolute left stick Z game angle (not factoring in cam angle).
		// Absolute right stick Z game angle (not factoring in cam angle).
		std::vector<float> movementOffsetParams;

		// If aim pitch was adjusted, either manually or to face a target.
		bool aimPitchAdjusted;
		// If aim pitch was manually adjusted with the 'AdjustAimPitch' bind.
		bool aimPitchManuallyAdjusted;
		// P1 should be set to motion driven in order to trigger a location discovery event.
		bool attemptDiscovery;
		// How close the player is to completing their last requested dash dodge.
		// [0.0, 1.0]
		float dashDodgeCompletionRatio;
		// Was DontMove() successfully called on this player?
		bool dontMoveSet;
		// Has a movement offset been set for this player's actor/mount?
		bool hasMovementOffset;
		// Nearby map marker is undiscovered and in range to discover.
		bool inRangeOfUndiscoveredMarker;
		// Is this player's interaction package running?
		bool interactionPackageRunning;
		// Is the player airborne after jumping?
		bool isAirborneWhileJumping;
		// Is the player animation driven?
		bool isAnimDriven;
		// Is the player performing a backward dash dodge?
		bool isBackStepDodge;
		// Is the player dash dodging?
		bool isDashDodging;
		// Is the player falling after jumping?
		bool isFallingWhileJumping;
		// Is the player getting up?
		bool isGettingUp;
		// Is the player attempting to mount?
		bool isMounting;
		// Is the player in the air paragliding?
		bool isParagliding;
		// Have the player's char controller pitch and roll angles been reset to 0?
		bool isParaglidingTiltAngleReset;
		// Is the player requesting to dash dodge?
		bool isRequestingDashDodge;
		// Is the player swimming? Have to manually play swim start animation
		// when in water, and swim stop animation when out of water.
		bool isSwimming;
		// Is the player in a synced animation?
		bool isSynced;
		// Player is moving the left stick.
		bool lsMoved;
		// Is an unpaused menu opened that stops movement?
		bool menuStopsMovement;
		// Player had their ragdoll triggered.
		bool playerRagdollTriggered;
		// Player is moving the right stick.
		bool rsMoved;
		// Has the JumpFall animation event been sent?
		bool sentJumpFallEvent;
		// Should the player's aim pitch be adjusted?
		bool shouldAdjustAimPitch;
		// Continue forcing the player to remain stationary until their reported movement speed is 0.
		bool shouldCurtailMomentum;
		// Should the player turn to directly face the targeted position?
		bool shouldFaceTarget;
		// Should the companion player start or stop paragliding?
		// True: start, False: stop.
		bool shouldParaglide;
		// Reset aim pitch?
		bool shouldResetAimPitch;
		// Should the stationary player start moving?
		bool shouldStartMoving;
		// Should the moving player stop moving and rotating?
		bool shouldStopMoving;
		// Indicates that the player has started to jump.
		bool startJump;
		// Aim pitch for torso rotation and projectile calculations.
		float aimPitch;
		// Height factor relative to base height.
		float baseHeightMult;
		// Base speed multiplier.
		float baseSpeedMult;
		// Current left stick angle at max displacement from center.
		float lsAngAtMaxDisp;
		// Pseudo-paraglide interp factor. [0.0, 1.0]
		float magicParaglideVelInterpFactor;
		// Starting and ending upward vertical speed while pseudo-paragliding.
		float magicParaglideEndZVel;
		float magicParaglideStartZVel;
		// Old left stick absolute game Z angle.
		float oldLSAngle;
		// TODO: Get from modding framework: pitch set for this player.
		float playerPitch;
		// Player's height adjusted by ref scale.
		float playerScaledHeight;
		// TODO: Get from modding framework: yaw set for this player.
		float playerYaw;
		// Previous left stick angle at max displacement from center.
		float prevLSAngAtMaxDisp;
		// Previous right stick angle at max displacement from center.
		float prevRSAngAtMaxDisp;
		// Current right stick angle at max displacement from center.
		float rsAngAtMaxDisp;

		// Initial jump vertical velocity when springing up after gather (havok units).
		// Takes into account player scale to prevent tall players from
		// having the vertical of a two year old reaching for an object on a shelf,
		// and short players from springing around like a frog on nose candy.
		const float havokInitialJumpZVelocity = 5.0f;

		// NOT FOR P1: 
		// Hardcoded yaw offsets for aiming with a bow/crossbow (vanilla animations).
		// NOTE: Very loose approximation done to avoid rotation jitter.
		const float bowSneakingYawOffset = 4.5f * PI / 180.0f;
		const float bowStandingYawOffset = 3.0f * PI / 180.0f;
		const float crossbowSneakingYawOffset = -1.0f * PI / 180.0f;
		const float crossbowStandingYawOffset = 0.0f * PI / 180.0f;

		// FOR P1:
		const float p1BowSneakingYawOffset = -4.0f * PI / 180.0f;
		const float p1BowStandingYawOffset = -3.0f * PI / 180.0f;
		const float p1CrossbowSneakingYawOffset = -3.0f * PI / 180.0f;
		const float p1CrossbowStandingYawOffset = -1.0f * PI / 180.0f;

		// Player ID for this player.
		int32_t playerID;
		// The player's assigned controller ID determined by XInput.
		int32_t controllerID;
		// Frames since attempting to discover a new location.
		uint8_t framesSinceAttemptingDiscovery;
		// Frames since requesting dash dodge/performing dash dodge animation.
		uint32_t framesSinceRequestingDashDodge;
		uint32_t framesSinceStartingDashDodge;
	};
}

#pragma once
#include <cstdint>
#include <vector>
#include <Player.h>

namespace ALYSLC 
{
	using SteadyClock = std::chrono::steady_clock;
	class CoopPlayer;

	// Keeps track of rotations to set and blending progress for handled torso and arm nodes.
	struct NodeRotationData
	{
		NodeRotationData()
		{
			InstantlyResetData();
		}

		NodeRotationData(const NodeRotationData& a_nrd)
		{
			blendInTP = a_nrd.blendInTP;
			blendOutTP = a_nrd.blendOutTP;
			localVelocity = a_nrd.localVelocity;
			localPosition = a_nrd.localPosition;
			rotationInput = a_nrd.rotationInput;
			currentRotation = a_nrd.currentRotation;
			defaultRotation = a_nrd.defaultRotation;
			startingRotation = a_nrd.startingRotation;
			targetRotation = a_nrd.targetRotation;
			rotationModified = a_nrd.rotationModified;
			blendStatus = a_nrd.blendStatus;
		}

		NodeRotationData(NodeRotationData&& a_nrd)
		{
			blendInTP = std::move(a_nrd.blendInTP);
			blendOutTP = std::move(a_nrd.blendOutTP);
			localVelocity = std::move(a_nrd.localVelocity);
			localPosition = std::move(a_nrd.localPosition);
			rotationInput = std::move(a_nrd.rotationInput);
			currentRotation = std::move(a_nrd.currentRotation);
			defaultRotation = std::move(a_nrd.defaultRotation);
			startingRotation = std::move(a_nrd.startingRotation);
			targetRotation = std::move(a_nrd.targetRotation);
			rotationModified = std::move(a_nrd.rotationModified);
			blendStatus = std::move(a_nrd.blendStatus);
		}

		NodeRotationData& operator=(const NodeRotationData& a_nrd)
		{
			blendInTP = a_nrd.blendInTP;
			blendOutTP = a_nrd.blendOutTP;
			localVelocity = a_nrd.localVelocity;
			localPosition = a_nrd.localPosition;
			rotationInput = a_nrd.rotationInput;
			currentRotation = a_nrd.currentRotation;
			defaultRotation = a_nrd.defaultRotation;
			startingRotation = a_nrd.startingRotation;
			targetRotation = a_nrd.targetRotation;
			rotationModified = a_nrd.rotationModified;
			blendStatus = a_nrd.blendStatus;

			return *this;
		}

		NodeRotationData& operator=(NodeRotationData&& a_nrd)
		{
			blendInTP = std::move(a_nrd.blendInTP);
			blendOutTP = std::move(a_nrd.blendOutTP);
			localVelocity = std::move(a_nrd.localVelocity);
			localPosition = std::move(a_nrd.localPosition);
			rotationInput = std::move(a_nrd.rotationInput);
			currentRotation = std::move(a_nrd.currentRotation);
			defaultRotation = std::move(a_nrd.defaultRotation);
			startingRotation = std::move(a_nrd.startingRotation);
			targetRotation = std::move(a_nrd.targetRotation);
			rotationModified = std::move(a_nrd.rotationModified);
			blendStatus = std::move(a_nrd.blendStatus);

			return *this;
		}

		// Reset all node data.
		inline void InstantlyResetData()
		{
			blendOutTP = blendInTP = SteadyClock::now();

			localVelocity =
			localPosition = RE::NiPoint3();
			currentRotation =
			startingRotation =
			targetRotation = RE::NiMatrix3();
			rotationModified = false;
			blendStatus = NodeRotationBlendStatus::kDefaultReached;
		}

		// Last blend request time points.
		SteadyClock::time_point blendInTP;
		SteadyClock::time_point blendOutTP;

		// List of three angles (yaw, pitch, roll)
		// Derived from right stick X, Y displacement (yaw, pitch), and const (roll).
		std::array<float, 3> rotationInput;
		// Blended node rotation matrix to restore in an NiNode hook.
		RE::NiMatrix3 currentRotation;
		// Last saved node rotation matrix set by the game before modification.
		RE::NiMatrix3 defaultRotation;
		// Last set node rotation matrix to interp from while blending.
		RE::NiMatrix3 startingRotation;
		// Last set node rotation matrix to interp to while blending.
		RE::NiMatrix3 targetRotation;
		// Last set position relative to the player's root position.
		RE::NiPoint3 localPosition;
		// Last recorded node velocity relative to the player's root position.
		RE::NiPoint3 localVelocity;

		// TODO: Blend rotations in/out.
		// Player has modified the target rotation of this node.
		bool rotationModified;
		NodeRotationBlendStatus blendStatus;
	};

	// Stores player body node rotation info to restore in the NiNode::UpdateDownwardPass() hook.
	struct NodeRotationManager
	{
		NodeRotationManager()
		{
			InstantlyResetAllNodeData();
		}

		NodeRotationManager& operator=(const NodeRotationManager& a_nrm)
		{
			nodeRotationDataMap.clear();
			for (const auto& [nameHash, data] : a_nrm.nodeRotationDataMap)
			{
				if (data && data.get()) 
				{
					nodeRotationDataMap.insert_or_assign(nameHash, std::make_unique<NodeRotationData>());
					nodeRotationDataMap[nameHash]->blendInTP = data->blendInTP;
					nodeRotationDataMap[nameHash]->blendOutTP = data->blendOutTP;
					nodeRotationDataMap[nameHash]->blendStatus = data->blendStatus;
					nodeRotationDataMap[nameHash]->currentRotation = data->currentRotation;
					nodeRotationDataMap[nameHash]->localPosition = data->localPosition;
					nodeRotationDataMap[nameHash]->localVelocity = data->localVelocity;
					nodeRotationDataMap[nameHash]->rotationInput = data->rotationInput;
					nodeRotationDataMap[nameHash]->startingRotation = data->startingRotation;
					nodeRotationDataMap[nameHash]->targetRotation = data->targetRotation;
					nodeRotationDataMap[nameHash]->rotationModified = data->rotationModified;
				}
			}

			return *this;
		}

		NodeRotationManager& operator=(NodeRotationManager&& a_nrm)
		{
			nodeRotationDataMap.swap(a_nrm.nodeRotationDataMap);
			return *this;
		}

		inline void ClearCustomRotation(const uint32_t& a_nodeNameHash)
		{
			if (nodeRotationDataMap.contains(a_nodeNameHash))
			{
				if (auto& data = nodeRotationDataMap.at(a_nodeNameHash); data && data.get())
				{
					data->rotationModified = false;
					data->rotationInput.fill(0.0f);
				}
			}
		}

		inline void ClearCustomRotations()
		{
			for (auto& [_, data] : nodeRotationDataMap)
			{
				data->rotationModified = false;
				data->rotationInput.fill(0.0f);
			}
		}

		// Reset all node data.
		void InstantlyResetAllNodeData();

		// Only reset only arm node data.
		void InstantlyResetArmNodeData();

		// Only reset only torso node data.
		void InstantlyResetTorsoNodeData();

		// Returns true if a custom cached rotation was set for the given node (indexed with hashed name).
		// Can check for the node name hash in either the set of adjustable arm nodes or torso nodes.
		bool NodeWasAdjusted(const uint32_t& a_nodeNameHash);

		// Update the blend status for the node with the given name hash.
		void SetBlendStatus(const uint32_t& a_nodeNameHash, NodeRotationBlendStatus&& a_newStatus);

		// So after a dozen hours of getting nowhere mathematically,
		// here are manually tested boundary rotation angles for blending.
		// (RS X * 2PI, RS Y * 2PI, fixed Z), both radians and (degrees).
		// LEFT SHOULDER (Flip X sign for RIGHT SHOULDER):
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
			{ ArmOrientation::kOutward, { -0.34484762f, -1.7245883f, 0.0f } },
			{ ArmOrientation::kForward, { -2.9678023f, -1.7808087f, 0.0f } },
			{ ArmOrientation::kBackward, { -3.5985053f, -4.5647597f, 0.0f } }
		};

		// Node rotation interpolation factor.
		// Higher values inch closer to directly setting the current rotation to the target rotation.
		// Lower values create more sluggish, but smooother, movement.
		const float interpFactor = 20.0f;

		// Mutex for setting rotation data.
		// IMPORTANT: Lock before reading/adjusting any nodes' rotations
		// managed by this manager.
		std::mutex rotationDataMutex;
		// Maps adjustable nodes by node name hash to their corresponding custom rotation data.
		std::unordered_map<uint32_t, std::unique_ptr<NodeRotationData>> nodeRotationDataMap;
	};

	// Handles player movement, actions related to movement, and node orientation.
	struct MovementManager : public Manager
	{
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

		// Update cached rotation data for arm nodes.
		void UpdateCachedArmNodeRotationData(RE::NiAVObject* a_forearmNode, RE::NiAVObject* a_handNode, bool a_rightArm);

		// Update rotation blend-related data.
		void UpdateCachedNodeRotationBlendData(const std::unique_ptr<NodeRotationData>& a_data, const uint32_t& a_nodeNameHash, RE::NiAVObject* a_node, bool a_isArmNode);

		// Update cached rotation data for shoulder nodes.
		void UpdateCachedShoulderNodeRotationData(RE::NiAVObject* a_shoulderNode, bool a_rightShoulder);
		
		// Update cached rotation data for torso nodes.
		void UpdateCachedTorsoNodeRotationData();

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
		// Dash dodge direction set on the first frame of the dodge.
		RE::NiPoint3 dashDodgeDir;
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
		// Prevents ragdolled players from getting up and shooting forward in their facing/movement direction
		// due to leftover momentum from prior to regdolling.
		bool shouldCurtailMomentum;
		// Should the player turn to directly face the targeted position?
		bool shouldFaceTarget;
		// Should the companion player start or stop paragliding?
		// True: start, False: stop.
		bool shouldParaglide;
		// Reset aim pitch and body node rotations?
		bool shouldResetAimAndBody;
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
		// How close the player is to completing their last requested dash dodge.
		// [0.0, 1.0]
		float dashDodgeCompletionRatio;
		// Equipped objects' total weight at the start of the dash dodge.
		float dashDodgeEquippedWeight;
		// Initial speed for dash dodge.
		float dashDodgeInitialSpeed;
		// Dash dodge LS displacement at the start of the dodge.
		float dashDodgeLSDisplacement;
		// Current left stick angle at max displacement from center.
		float lsAngAtMaxDisp;
		// Pseudo-paraglide interp factor. [0.0, 1.0]
		float magicParaglideVelInterpFactor;
		// Starting and ending upward vertical speed while pseudo-paragliding.
		float magicParaglideEndZVel;
		float magicParaglideStartZVel;
		// Old left stick absolute game Z angle.
		float oldLSAngle;
		// Pitch to set for this player.
		float playerPitch;
		// Player's height adjusted by ref scale.
		float playerScaledHeight;
		// Yaw to set for this player.
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

		// Player ID for this player.
		int32_t playerID;
		// The player's assigned controller ID determined by XInput.
		int32_t controllerID;
		// Frames since attempting to discover a new location.
		uint8_t framesSinceAttemptingDiscovery;
		// Frames since requesting dash dodge/performing dash dodge animation.
		uint32_t framesSinceRequestingDashDodge;
		uint32_t framesSinceStartingDashDodge;
		uint32_t framesSinceStartingJump;
	};
}

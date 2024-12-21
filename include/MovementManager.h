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
			blendStatus = a_nrd.blendStatus;
			blendInFrameCount = a_nrd.blendInFrameCount;
			blendOutFrameCount = a_nrd.blendOutFrameCount;
			localVelocity = a_nrd.localVelocity;
			localPosition = a_nrd.localPosition;
			rotationInput = a_nrd.rotationInput;
			currentRotation = a_nrd.currentRotation;
			defaultRotation = a_nrd.defaultRotation;
			startingRotation = a_nrd.startingRotation;
			targetRotation = a_nrd.targetRotation;
			interrupted = a_nrd.interrupted;
			prevInterrupted = a_nrd.prevInterrupted;
			prevRotationModified = a_nrd.prevRotationModified;
			rotationModified = a_nrd.rotationModified;
		}

		NodeRotationData(NodeRotationData&& a_nrd)
		{
			blendStatus = std::move(a_nrd.blendStatus);
			blendInFrameCount = std::move(a_nrd.blendInFrameCount);
			blendOutFrameCount = std::move(a_nrd.blendOutFrameCount);
			localVelocity = std::move(a_nrd.localVelocity);
			localPosition = std::move(a_nrd.localPosition);
			rotationInput = std::move(a_nrd.rotationInput);
			currentRotation = std::move(a_nrd.currentRotation);
			defaultRotation = std::move(a_nrd.defaultRotation);
			startingRotation = std::move(a_nrd.startingRotation);
			targetRotation = std::move(a_nrd.targetRotation);
			interrupted = std::move(a_nrd.interrupted);
			prevInterrupted = std::move(a_nrd.prevInterrupted);
			prevRotationModified = std::move(a_nrd.prevRotationModified);
			rotationModified = std::move(a_nrd.rotationModified);
		}

		NodeRotationData& operator=(const NodeRotationData& a_nrd)
		{
			blendStatus = a_nrd.blendStatus;
			blendInFrameCount = a_nrd.blendInFrameCount;
			blendOutFrameCount = a_nrd.blendOutFrameCount;
			localVelocity = a_nrd.localVelocity;
			localPosition = a_nrd.localPosition;
			rotationInput = a_nrd.rotationInput;
			currentRotation = a_nrd.currentRotation;
			defaultRotation = a_nrd.defaultRotation;
			startingRotation = a_nrd.startingRotation;
			targetRotation = a_nrd.targetRotation;
			interrupted = a_nrd.interrupted;
			prevInterrupted = a_nrd.prevInterrupted;
			prevRotationModified = a_nrd.prevRotationModified;
			rotationModified = a_nrd.rotationModified;

			return *this;
		}

		NodeRotationData& operator=(NodeRotationData&& a_nrd)
		{
			blendStatus = std::move(a_nrd.blendStatus);
			blendInFrameCount = std::move(a_nrd.blendInFrameCount);
			blendOutFrameCount = std::move(a_nrd.blendOutFrameCount);
			localVelocity = std::move(a_nrd.localVelocity);
			localPosition = std::move(a_nrd.localPosition);
			rotationInput = std::move(a_nrd.rotationInput);
			currentRotation = std::move(a_nrd.currentRotation);
			defaultRotation = std::move(a_nrd.defaultRotation);
			startingRotation = std::move(a_nrd.startingRotation);
			targetRotation = std::move(a_nrd.targetRotation);
			interrupted = std::move(a_nrd.interrupted);
			prevInterrupted = std::move(a_nrd.prevInterrupted);
			prevRotationModified = std::move(a_nrd.prevRotationModified);
			rotationModified = std::move(a_nrd.rotationModified);

			return *this;
		}

		// Reset all node data.
		inline void InstantlyResetData()
		{
			blendInFrameCount = blendOutFrameCount = 0;

			localVelocity =
			localPosition = RE::NiPoint3();
			currentRotation =
			startingRotation =
			targetRotation = RE::NiMatrix3();
			interrupted = false;
			prevInterrupted = false;
			prevRotationModified = false;
			rotationModified = false;
			blendStatus = NodeRotationBlendStatus::kDefaultReached;
		}

		// Blend status for this node.
		NodeRotationBlendStatus blendStatus;

		// IMPORTANT NOTE: Currently, torso node rotation data is based on nodes' world rotations
		// and arm node rotation data is based on nodes' local rotations.
		// TODO: Change all saved rotation data to world rotations.
		
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

		// Was setting the custom rotation for this node interrupted
		// by the player's state (eg. ragdolled, staggered, manager paused)?
		bool interrupted;
		// Interrupted flag from the previous frame.
		bool prevInterrupted;
		// Rotation modified flag from the previous frame.
		bool prevRotationModified;
		// Player has modified the target rotation of this node.
		bool rotationModified;

		// Frames since blending in.
		uint32_t blendInFrameCount;
		// Frames since blending out.
		uint32_t blendOutFrameCount;
	};

	// Stores player body node position/rotation/velocity info.
	struct NodeOrientationManager
	{
		NodeOrientationManager()
		{ }

		NodeOrientationManager(const std::shared_ptr<CoopPlayer>& a_p)
		{
			InstantlyResetAllNodeData(a_p);
		}

		NodeOrientationManager& operator=(const NodeOrientationManager& a_nom)
		{
			defaultNodeLocalTransformsMap.clear();
			defaultNodeWorldTransformsMap.clear();
			nodeRotationDataMap.clear();
			for (const auto& [nodePtr, data] : a_nom.nodeRotationDataMap)
			{
				if (data && data.get()) 
				{
					nodeRotationDataMap.insert_or_assign(nodePtr, std::make_unique<NodeRotationData>());
					auto& newData = nodeRotationDataMap[nodePtr];
					newData->blendInFrameCount = data->blendInFrameCount;
					newData->blendOutFrameCount = data->blendOutFrameCount;
					newData->blendStatus = data->blendStatus;
					newData->currentRotation = data->currentRotation;
					newData->localPosition = data->localPosition;
					newData->localVelocity = data->localVelocity;
					newData->rotationInput = data->rotationInput;
					newData->startingRotation = data->startingRotation;
					newData->targetRotation = data->targetRotation;
					newData->interrupted = data->interrupted;
					newData->prevInterrupted = data->prevInterrupted;
					newData->prevRotationModified = data->prevRotationModified;
					newData->rotationModified = data->rotationModified;
				}
			}

			return *this;
		}

		NodeOrientationManager& operator=(NodeOrientationManager&& a_nom)
		{
			nodeRotationDataMap.swap(a_nom.nodeRotationDataMap);
			return *this;
		}

		inline void ClearCustomRotation(const RE::NiPointer<RE::NiAVObject>& a_nodePtr)
		{
			if (nodeRotationDataMap.contains(a_nodePtr))
			{
				if (auto& data = nodeRotationDataMap.at(a_nodePtr); data && data.get())
				{
					data->prevInterrupted = data->interrupted;
					data->prevRotationModified = data->prevRotationModified;
					data->interrupted = false;
					data->rotationModified = false;
					data->rotationInput.fill(0.0f);
				}
			}
		}

		inline void ClearCustomRotations()
		{
			for (auto& [_, data] : nodeRotationDataMap)
			{
				data->prevInterrupted = data->interrupted;
				data->prevRotationModified = data->prevRotationModified;
				data->interrupted = false;
				data->rotationModified = false;
				data->rotationInput.fill(0.0f);
			}
		}

		// Apply our custom rotation to the given node.
		void ApplyCustomNodeRotation(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_nodePtr);

		// Debug function to display rotation axes for player nodes.
		void DisplayAllNodeRotations(const std::shared_ptr<CoopPlayer>& a_p);

		// Reset all node data.
		void InstantlyResetAllNodeData(const std::shared_ptr<CoopPlayer>& a_p);

		// Only reset arm node data.
		void InstantlyResetArmNodeData(const std::shared_ptr<CoopPlayer>& a_p);

		// Only reset torso node data.
		void InstantlyResetTorsoNodeData(const std::shared_ptr<CoopPlayer>& a_p);

		// Returns true if a custom cached rotation was set for the given node.
		// Can check for the node in either the set of adjustable arm nodes or torso nodes.
		bool NodeWasAdjusted(const RE::NiPointer<RE::NiAVObject>& a_nodePtr);

		// Restore saved node local transforms previously set by the game before our modifications.
		void RestoreOriginalNodeLocalTransforms(const std::shared_ptr<CoopPlayer>& a_p);

		// Recursively save all nodes' local rotations and world positions/rotations
		// by walking the player's node tree from the given node.
		// NOTE: Must be called each frame before any UpdateDownwardPass() calls fire
		// to properly save the game's intended local rotations and world positions/rotations for all player nodes.
		void SavePlayerNodeWorldTransforms(const std::shared_ptr<CoopPlayer>& a_p);

		// Update the blend status for the given node.
		void SetBlendStatus(const RE::NiPointer<RE::NiAVObject>& a_nodePtr, NodeRotationBlendStatus&& a_newStatus);

		// Update cached rotation data for arm nodes.
		void UpdateArmNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_forearmNodePtr, const RE::NiPointer<RE::NiAVObject>& a_handNodePtr, bool a_rightArm);

		// Update node rotation blend status and endpoints.
		void UpdateNodeRotationBlendState(const std::shared_ptr<CoopPlayer>& a_p, const std::unique_ptr<NodeRotationData>& a_data, const RE::NiPointer<RE::NiAVObject>& a_nodePtr, bool a_isArmNode);

		// Based on this node's blend state, update its rotation data to use when modifying the node's world position.
		void UpdateNodeRotationToSet(const std::shared_ptr<CoopPlayer>& a_p, const std::unique_ptr<NodeRotationData>& a_data, const RE::NiPointer<RE::NiAVObject>& a_nodePtr, bool a_isArmNode);

		// Update cached rotation data for shoulder nodes.
		void UpdateShoulderNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiAVObject>& a_shoulderNodePtr, bool a_rightShoulder);

		// Update cached rotation data for torso nodes.
		void UpdateTorsoNodeRotationData(const std::shared_ptr<CoopPlayer>& a_p);

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
		// Used at 60 FPS, since all node rotation updates are run in a havok callback.
		const float interpFactor = 0.333333f;

		// Mutex for setting rotation data.
		// IMPORTANT: Lock before reading/adjusting any nodes' rotations
		// managed by this manager.
		std::mutex rotationDataMutex;
		// Maps adjustable nodes by node name hash to their corresponding custom rotation data.
		std::unordered_map<RE::NiPointer<RE::NiAVObject>, std::unique_ptr<NodeRotationData>> nodeRotationDataMap;
		// Maps all player node name hashes to their default local transforms
		// set by the game before our modifications.
		std::unordered_map<RE::NiPointer<RE::NiAVObject>, RE::NiTransform> defaultNodeLocalTransformsMap;
		// Maps all player node name hashes to their default world transforms
		// set by the game before our modifications.
		std::unordered_map<RE::NiPointer<RE::NiAVObject>, RE::NiTransform> defaultNodeWorldTransformsMap;

	private:
		// Recursive helper functions which walk the player's node tree
		// to display or save rotations.
		void DisplayAllNodeRotations(const RE::NiPointer<RE::NiNode>& a_nodePtr);

		void SavePlayerNodeWorldTransforms(const std::shared_ptr<CoopPlayer>& a_p, const RE::NiPointer<RE::NiNode>& a_nodePtr, const RE::NiTransform& a_parentWorldTransform);
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

		// Update worldspace attack source position and direction.
		// Can also set the default position and direction using the cached default world rotation data.
		void UpdateAttackSourceOrientationData(bool&& a_setDefaultDirAndPos);

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
		// Player attack source's direction set by the game before our custom node modifications.
		RE::NiPoint3 playerDefaultAttackSourceDir;
		// Player attack source's position set by the game before our custom node modifications.
		RE::NiPoint3 playerDefaultAttackSourcePos;
		// World-space axis of rotation about which to rotate torso nodes.
		RE::NiPoint3 playerTorsoAxisOfRotation;
		// Manages saved orientation data for this player's nodes.
		std::unique_ptr<NodeOrientationManager> nom;
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
		// Face the target directly at all times after toggled on by FaceTarget bind.
		bool faceTarget;
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
		// Temporarily turn to face the target when certain actions trigger.
		bool turnToTarget;
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
		// Total pitch offset to apply to torso nodes while dash dodging.
		float dashDodgeTorsoPitchOffset;
		// Total pitch offset to apply to torso nodes while dash dodging.
		float dashDodgeTorsoRollOffset;
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
		// Interpolation factor for rotating the player.
		const float playerRotInterpFactor = 0.25f;

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

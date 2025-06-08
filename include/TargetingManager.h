#pragma once
//#include <PCH.cpp>
#include <unordered_set>
#include <Enums.h>

#include <Player.h>

namespace ALYSLC
{
	class CoopPlayer;

	// Player manager that handles crosshair targeting, interaction with nearby objects,
	// player crosshair notification messages, and the physics of projectiles, 
	// ragdolling actors, and grabbed objects.
	// All player-to-environment targeting and interactions.
	struct TargetingManager : public Manager
	{
		// Information pertaining to an on-screen player-specific message
		// that is displayable using the game HUD's crosshair text member.
		struct CrosshairMessage
		{
			CrosshairMessage(int32_t a_playerCID) :
				type(CrosshairMessageType::kNone),
				setTP(SteadyClock::now()),
				text(""sv),
				delayedMessageTypes({}),
				secsMaxDisplayTime(0.0f),
				hash(Hash(text))
			{
				Clear();
			}

			CrosshairMessage(const CrosshairMessage& a_other) :
				type(a_other.type),
				setTP(a_other.setTP),
				text(a_other.text),
				delayedMessageTypes(a_other.delayedMessageTypes),
				secsMaxDisplayTime(a_other.secsMaxDisplayTime),
				hash(a_other.hash)
			{ }

			CrosshairMessage(CrosshairMessage&& a_other) :
				type(std::move(a_other.type)),
				setTP(std::move(a_other.setTP)),
				text(std::move(a_other.text)),
				delayedMessageTypes(std::move(a_other.delayedMessageTypes)),
				secsMaxDisplayTime(std::move(a_other.secsMaxDisplayTime)),
				hash(std::move(a_other.hash))
			{
				a_other.Clear();
			}

			CrosshairMessage& operator=(const CrosshairMessage& a_other)
			{
				type = a_other.type;
				setTP = a_other.setTP;
				text = a_other.text;
				delayedMessageTypes = a_other.delayedMessageTypes;
				secsMaxDisplayTime = a_other.secsMaxDisplayTime;
				hash = a_other.hash;

				return *this;
			}

			CrosshairMessage& operator=(CrosshairMessage&& a_other)
			{
				type = std::move(a_other.type);
				setTP = std::move(a_other.setTP);
				text = std::move(a_other.text);
				delayedMessageTypes = std::move(a_other.delayedMessageTypes);
				secsMaxDisplayTime = std::move(a_other.secsMaxDisplayTime);
				hash = std::move(a_other.hash);

				a_other.Clear();
				return *this;
			}
			
			// Clear out this message's data.
			inline void Clear() noexcept
			{
				type = CrosshairMessageType::kNone;
				text = ""sv;

				if (!delayedMessageTypes.empty())
				{
					delayedMessageTypes.clear();
				}

				secsMaxDisplayTime = 0.0f;
				hash = Hash(text);
			}

			// Copy another message's data to this one without invalidating the other message.
			inline void CopyMessageData(const std::unique_ptr<CrosshairMessage>& a_other)
			{
				if (!a_other || !a_other.get())
				{
					return;
				}

				type = a_other->type;
				setTP = a_other->setTP;
				text = a_other->text;
				delayedMessageTypes = a_other->delayedMessageTypes;
				secsMaxDisplayTime = a_other->secsMaxDisplayTime;
				hash = a_other->hash;
			}

			// Update cached data based on the given message type, text, 
			// and other message types to delay.
			// Start time point is set to now.
			inline void Update
			(
				CrosshairMessageType&& a_type, 
				const RE::BSFixedString a_text, 
				std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, 
				float a_secsMaxDisplayTime = 0.0f,
				bool a_updateSetTP = true
			) noexcept
			{
				type = a_type;
				text = a_text;
				delayedMessageTypes = a_delayedMessageTypes;
				secsMaxDisplayTime = a_secsMaxDisplayTime;
				hash = Hash(a_text);
				if (a_updateSetTP)
				{
					setTP = SteadyClock::now();
				}
			}

			// This message's type.
			CrosshairMessageType type;
			// When the message data was set.
			SteadyClock::time_point setTP;
			// This message's text.
			RE::BSFixedString text;
			// Other types of messages to delay when this message
			// is set as the current crosshair message.
			std::set<CrosshairMessageType> delayedMessageTypes;
			// Maximum number of seconds to display this message for.
			float secsMaxDisplayTime;
			// Hash of this message's text.
			uint32_t hash;
		};

		// Base class for all manipulable refrs, grabbed or released.
		class ManipulableRefrInfo
		{
		public:
			ManipulableRefrInfo() : 
				refrHandle(RE::ObjectRefHandle()),
				isActiveProjectile(false)
			{ }
			virtual ~ManipulableRefrInfo() = default;

			inline bool IsValid()
			{
				return refrHandle && refrHandle.get() && refrHandle.get().get();
			}

			// Clear data for the manipulable refr.
			// Grabbed and released refrs have different sets of data to clear.
			virtual void Clear() = 0;

			// Handle for the managed refr.
			RE::ObjectRefHandle refrHandle;

			// Is the object an active projectile?
			// (eg. one that can do damage and is still airborne)
			bool isActiveProjectile;
		};

		// Stores handle, grab time point, and other information related to grabbed references.
		class GrabbedReferenceInfo : public ManipulableRefrInfo
		{
		public:

			GrabbedReferenceInfo() :
				savedCollisionLayer(RE::COL_LAYER::kUnidentified),
				lastSetVelocity(RE::NiPoint3()),
				grabTP(std::nullopt),
				hasCollision(true)
			{ }

			GrabbedReferenceInfo(const RE::ObjectRefHandle& a_handle) :
				savedCollisionLayer(RE::COL_LAYER::kUnidentified),
				lastSetVelocity(RE::NiPoint3()),
				grabTP(SteadyClock::now()),
				hasCollision(true)
			{ 
				refrHandle = a_handle;
			}

			GrabbedReferenceInfo(const GrabbedReferenceInfo& a_other)
			{
				refrHandle = a_other.refrHandle;
				savedCollisionLayer = a_other.savedCollisionLayer;
				lastSetVelocity = a_other.lastSetVelocity;
				grabTP = a_other.grabTP;
				hasCollision = a_other.hasCollision;
			}

			GrabbedReferenceInfo(GrabbedReferenceInfo&& a_other)
			{
				refrHandle = std::move(a_other.refrHandle);
				savedCollisionLayer = std::move(a_other.savedCollisionLayer);
				lastSetVelocity = std::move(lastSetVelocity);
				grabTP = std::move(a_other.grabTP);
				hasCollision = std::move(a_other.hasCollision);
			}

			GrabbedReferenceInfo& operator=(const GrabbedReferenceInfo& a_other)
			{
				refrHandle = a_other.refrHandle;
				savedCollisionLayer = a_other.savedCollisionLayer;
				lastSetVelocity = a_other.lastSetVelocity;
				grabTP = a_other.grabTP;
				hasCollision = a_other.hasCollision;
				return *this;
			}

			GrabbedReferenceInfo& operator=(GrabbedReferenceInfo&& a_other)
			{
				refrHandle = std::move(a_other.refrHandle);
				savedCollisionLayer = std::move(a_other.savedCollisionLayer);
				lastSetVelocity = std::move(a_other.lastSetVelocity);
				grabTP = std::move(a_other.grabTP);
				hasCollision = std::move(a_other.hasCollision);
				return *this;
			}
			
			// Implements ALYSLC::TargetingManager::ManipulableRefrInfo:
			// Clear out handle and grab time point.
			inline void Clear() override
			{
				refrHandle = RE::ObjectRefHandle();
				savedCollisionLayer = RE::COL_LAYER::kUnidentified;
				lastSetVelocity = RE::NiPoint3();
				grabTP = std::nullopt;
				hasCollision = true;
			}

			// Restore the saved collision layer.
			void RestoreSavedCollisionLayer();
			
			// Saved the refr's collision layer to restore later.
			void SaveCollisionLayer();

			// Toggle collision for this grabbed refr.
			void ToggleCollision();

			// Update velocity/positioning of the grabbed refr at the given index. 
			// Use the first grabbed refr's buffer distance to position subsequent refrs around it.
			void UpdateGrabbedReference
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				const uint8_t& a_index, 
				const float& a_firstGrabbedReferenceBufferDist
			);

			// Saved collision layer for the grabbed refr.
			RE::COL_LAYER savedCollisionLayer;

			// The last set velocity for this grabbed refr.
			RE::NiPoint3 lastSetVelocity;

			// Time point at which this refr was grabbed.
			std::optional<SteadyClock::time_point> grabTP;

			// True if the grabbed refr's collision is active.
			bool hasCollision;
		};

		// Holds information pertaining to a havok contact event.
		// Includes the colliding refrs' handles, colliding rigid bodies,
		// contact position, and contact speed.
		// NOTE:
		// Meant to save RE::hkpContactPointEvent info to process later in the player's
		// targeting manager main task. Want to minimize the amount of computation done
		// in the contact point callback and some of the contact point event's members
		// become invalid if the event itself is stored for processing later.
		// Eg. 
		// If saved in a queue for later, the contact event's 'contactPoint' pointer member 
		// causes a crash when accessing its 'position' member at times.
		struct HavokContactEventInfo
		{
			HavokContactEventInfo() :
				handleA(RE::ObjectRefHandle()),
				handleB(RE::ObjectRefHandle()),
				rigidBodyA(nullptr),
				rigidBodyB(nullptr),
				contactPosition(RE::hkVector4()),
				contactSpeed(0.0f)
			{ }

			HavokContactEventInfo
			(
				const RE::ObjectRefHandle& a_handleA, 
				const RE::ObjectRefHandle& a_handleB, 
				const RE::hkRefPtr<RE::hkpRigidBody>& a_rigidBodyA,
				const RE::hkRefPtr<RE::hkpRigidBody>& a_rigidBodyB,
				const RE::hkVector4& a_contactPosition,
				const float& a_contactSpeed
			)
			{ 
				handleA = a_handleA;
				handleB = a_handleB;
				rigidBodyA = a_rigidBodyA;
				rigidBodyB = a_rigidBodyB;
				contactPosition = a_contactPosition;
				contactSpeed = a_contactSpeed;
			}

			HavokContactEventInfo(const HavokContactEventInfo& a_other)
			{ 
				handleA = a_other.handleA;
				handleB = a_other.handleB;
				rigidBodyA = a_other.rigidBodyA;
				rigidBodyB = a_other.rigidBodyB;
				contactPosition = a_other.contactPosition;
				contactSpeed = a_other.contactSpeed;
			}

			HavokContactEventInfo(HavokContactEventInfo&& a_other)
			{ 
				handleA = std::move(a_other.handleA);
				handleB = std::move(a_other.handleB);
				rigidBodyA = std::move(a_other.rigidBodyA);
				rigidBodyB = std::move(a_other.rigidBodyB);
				contactPosition = std::move(a_other.contactPosition);
				contactSpeed = std::move(a_other.contactSpeed);
			}

			HavokContactEventInfo& operator=(const HavokContactEventInfo& a_other)
			{ 
				handleA = a_other.handleA;
				handleB = a_other.handleB;
				rigidBodyA = a_other.rigidBodyA;
				rigidBodyB = a_other.rigidBodyB;
				contactPosition = a_other.contactPosition;
				contactSpeed = a_other.contactSpeed;

				return *this;
			}

			HavokContactEventInfo& operator=(HavokContactEventInfo&& a_other)
			{ 
				handleA = std::move(a_other.handleA);
				handleB = std::move(a_other.handleB);
				rigidBodyA = std::move(a_other.rigidBodyA);
				rigidBodyB = std::move(a_other.rigidBodyB);
				contactPosition = std::move(a_other.contactPosition);
				contactSpeed = std::move(a_other.contactSpeed);

				return *this;
			}
		
			// Handles for the first and second colliding bodies' refrs.
			RE::ObjectRefHandle handleA;
			RE::ObjectRefHandle handleB;
			// Colliding rigid bodies.
			RE::hkRefPtr<RE::hkpRigidBody> rigidBodyA;
			RE::hkRefPtr<RE::hkpRigidBody> rigidBodyB;
			// Havok position at which the two bodies collided.
			RE::hkVector4 contactPosition;
			// Relative speed at which the two bodies collided.
			float contactSpeed;
		};

		// Stores targeting (release state, trajectory, target) 
		// info about a refr dropped/thrown by a player.
		class ReleasedReferenceInfo : public ManipulableRefrInfo
		{
		public:

			ReleasedReferenceInfo() :
				trajType(ProjectileTrajType::kAimDirection),
				canReachTarget(true),
				isHoming(false),
				isThrown(false),
				startedHomingIn(false),
				initialTimeToTarget(0.0f),
				launchPitch(0.0f),
				launchYaw(0.0f),
				magickaOverflowSlowdownFactor(1.0f),
				releaseSpeed(0.0),
				targetedActorNode(),
				targetRefrHandle(RE::ObjectRefHandle()),
				lastSetTargetPosition(RE::NiPoint3()),
				lastSetVelocity(RE::NiPoint3()),
				releasePos(RE::NiPoint3()),
				releaseVelocity(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3()),
				firstHitTP(std::nullopt),
				releaseTP(std::nullopt)
			{
				hitRefrFIDs.clear();
			}

			ReleasedReferenceInfo
			(
				const int32_t& a_controllerID, 
				const RE::ObjectRefHandle& a_handle
			) :
				trajType(ProjectileTrajType::kAimDirection),
				canReachTarget(true),
				isHoming(false),
				isThrown(false),
				startedHomingIn(false),
				initialTimeToTarget(0.0f),
				launchPitch(0.0f),
				launchYaw(0.0f),
				magickaOverflowSlowdownFactor(0.0f),
				releaseSpeed(0.0),
				targetedActorNode(),
				targetRefrHandle(RE::ObjectRefHandle()),
				lastSetTargetPosition(RE::NiPoint3()),
				lastSetVelocity(RE::NiPoint3()),
				releasePos(RE::NiPoint3()),
				releaseVelocity(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3()),
				firstHitTP(std::nullopt),
				releaseTP(SteadyClock::now())
			{
				refrHandle = a_handle;
				hitRefrFIDs.clear();
			}

			ReleasedReferenceInfo(const ReleasedReferenceInfo& a_other) 
			{
				trajType = a_other.trajType;
				canReachTarget = a_other.canReachTarget;
				isHoming = a_other.isHoming;
				isThrown = a_other.isThrown;
				startedHomingIn = a_other.startedHomingIn;
				initialTimeToTarget = a_other.initialTimeToTarget;
				launchPitch = a_other.launchPitch;
				launchYaw = a_other.launchYaw;
				magickaOverflowSlowdownFactor = a_other.magickaOverflowSlowdownFactor;
				releaseSpeed = a_other.releaseSpeed;
				targetedActorNode = a_other.targetedActorNode;
				targetRefrHandle = a_other.targetRefrHandle;
				lastSetTargetPosition = a_other.lastSetTargetPosition;
				lastSetVelocity = a_other.lastSetVelocity;
				releasePos = a_other.releasePos;
				releaseVelocity = a_other.releaseVelocity;
				targetLocalPosOffset = a_other.targetLocalPosOffset;
				trajectoryEndPos = a_other.trajectoryEndPos;
				refrHandle = a_other.refrHandle;
				firstHitTP = a_other.firstHitTP;
				releaseTP = a_other.releaseTP;
				hitRefrFIDs = a_other.hitRefrFIDs;
			}

			ReleasedReferenceInfo(ReleasedReferenceInfo&& a_other) 
			{
				trajType = std::move(a_other.trajType);
				canReachTarget = std::move(a_other.canReachTarget);
				isHoming = std::move(a_other.isHoming);
				isThrown = std::move(a_other.isThrown);
				startedHomingIn = std::move(a_other.startedHomingIn);
				initialTimeToTarget = std::move(a_other.initialTimeToTarget);
				launchPitch = std::move(a_other.launchPitch);
				launchYaw = std::move(a_other.launchYaw);
				magickaOverflowSlowdownFactor = std::move(a_other.magickaOverflowSlowdownFactor);
				releaseSpeed = std::move(a_other.releaseSpeed);
				targetedActorNode = std::move(a_other.targetedActorNode);
				targetRefrHandle = std::move(a_other.targetRefrHandle);
				lastSetTargetPosition = std::move(a_other.lastSetTargetPosition);
				lastSetVelocity = std::move(a_other.lastSetVelocity);
				releasePos = std::move(a_other.releasePos);
				releaseVelocity = std::move(a_other.releaseVelocity);
				targetLocalPosOffset = std::move(a_other.targetLocalPosOffset);
				trajectoryEndPos = std::move(a_other.trajectoryEndPos);
				refrHandle = std::move(a_other.refrHandle);
				firstHitTP = std::move(a_other.firstHitTP);
				releaseTP = std::move(a_other.releaseTP);
				hitRefrFIDs = std::move(a_other.hitRefrFIDs);
			}

			ReleasedReferenceInfo& operator=(const ReleasedReferenceInfo& a_other) 
			{
				trajType = a_other.trajType;
				canReachTarget = a_other.canReachTarget;
				isHoming = a_other.isHoming;
				isThrown = a_other.isThrown;
				startedHomingIn = a_other.startedHomingIn;
				initialTimeToTarget = a_other.initialTimeToTarget;
				launchPitch = a_other.launchPitch;
				launchYaw = a_other.launchYaw;
				magickaOverflowSlowdownFactor = a_other.magickaOverflowSlowdownFactor;
				releaseSpeed = a_other.releaseSpeed;
				targetedActorNode = a_other.targetedActorNode;
				targetRefrHandle = a_other.targetRefrHandle;
				lastSetTargetPosition = a_other.lastSetTargetPosition;
				lastSetVelocity = a_other.lastSetVelocity;
				releasePos = a_other.releasePos;
				releaseVelocity = a_other.releaseVelocity;
				targetLocalPosOffset = a_other.targetLocalPosOffset;
				trajectoryEndPos = a_other.trajectoryEndPos;
				refrHandle = a_other.refrHandle;
				firstHitTP = a_other.firstHitTP;
				releaseTP = a_other.releaseTP;
				hitRefrFIDs = a_other.hitRefrFIDs;

				return *this;
			}

			ReleasedReferenceInfo& operator=(ReleasedReferenceInfo&& a_other) 
			{
				trajType = std::move(a_other.trajType);
				canReachTarget = std::move(a_other.canReachTarget);
				isHoming = std::move(a_other.isHoming);
				isThrown = std::move(a_other.isThrown);
				startedHomingIn = std::move(a_other.startedHomingIn);
				initialTimeToTarget = std::move(a_other.initialTimeToTarget);
				launchPitch = std::move(a_other.launchPitch);
				launchYaw = std::move(a_other.launchYaw);
				magickaOverflowSlowdownFactor = std::move(a_other.magickaOverflowSlowdownFactor);
				releaseSpeed = std::move(a_other.releaseSpeed);
				targetedActorNode = std::move(a_other.targetedActorNode);
				targetRefrHandle = std::move(a_other.targetRefrHandle);
				lastSetTargetPosition = std::move(a_other.lastSetTargetPosition);
				lastSetVelocity = std::move(a_other.lastSetVelocity);
				releasePos = std::move(a_other.releasePos);
				releaseVelocity = std::move(a_other.releaseVelocity);
				targetLocalPosOffset = std::move(a_other.targetLocalPosOffset);
				trajectoryEndPos = std::move(a_other.trajectoryEndPos);
				refrHandle = std::move(a_other.refrHandle);
				firstHitTP = std::move(a_other.firstHitTP);
				releaseTP = std::move(a_other.releaseTP);
				hitRefrFIDs = std::move(a_other.hitRefrFIDs);

				return *this;
			}
			
			// Add the given refr as hit by this released refr.
			inline void AddHitRefr(RE::TESObjectREFR* a_refr)
			{
				// Set first hit TP if colliding with something for the first time.
				// Ignore hits within 30 frames of release to allow the released refrs
				// to get off the ground and start on its trajectory if it hasn't already.
				// This applies to heavy or not very aerodynamic objects/actors,
				// such as fish, king crabs, and dragons.
				if (!firstHitTP.has_value() && 
					releaseTP.has_value() &&
					Util::GetElapsedSeconds(releaseTP.value()) > *g_deltaTimeRealTime * 30)
				{
					firstHitTP = SteadyClock::now();
				}

				hitRefrFIDs.insert(a_refr->formID);
			}

			// Implements ALYSLC::TargetingManager::ManipulableRefrInfo:
			// Clear all data for this released refr.
			inline void Clear()
			{
				trajType = ProjectileTrajType::kAimDirection;
				canReachTarget = true;
				isHoming = false;
				isThrown = false;
				startedHomingIn = false;
				initialTimeToTarget = launchPitch = launchYaw = releaseSpeed = 0.0;
				magickaOverflowSlowdownFactor = 1.0f;
				targetedActorNode.reset();
				targetRefrHandle = RE::ObjectRefHandle();
				lastSetTargetPosition = 
				lastSetVelocity = 
				releasePos =
				releaseVelocity =
				targetLocalPosOffset =
				trajectoryEndPos = RE::NiPoint3();
				refrHandle = RE::ObjectRefHandle();
				firstHitTP = releaseTP = std::nullopt;
				hitRefrFIDs.clear();
			}

			// Returns true if this released refr has already collided with the given refr.
			inline bool HasAlreadyHitRefr(RE::TESObjectREFR* a_collidedWithRefr)
			{
				if (a_collidedWithRefr)
				{
					if (hitRefrFIDs.empty())
					{
						return false;
					}
					else
					{
						return hitRefrFIDs.contains(a_collidedWithRefr->formID);
					}
				}

				return false;
			}

			// Save the refr's current velocity, cap the given speed of this released refr 
			// to the release speed, and then apply the capped velocity.
			void ApplyVelocity(RE::NiPoint3& a_velocityToSet);

			// Calculate the target position to throw this refr at 
			// in order to hit the player's currently targeted refr.
			RE::NiPoint3 CalculatePredInterceptPos
			(
				const std::shared_ptr<CoopPlayer>& a_p
			);

			// Direct this released refr at either the initial target position 
			// along a fixed trajectory or continuously towards the target position/target refr.
			// Can elect to draw the thrown refr's trajectory while it is managed.
			// Return velocity to set.
			RE::NiPoint3 GuideRefrAlongTrajectory(const std::shared_ptr<CoopPlayer>& a_p);
			
			// Set initial release trajectory info for this refr prior to its release.
			// Determines the released refr's initial orientation, speed, time to target,
			// targeted trajectory end position, 
			// and whether or not the released refr was thrown or dropped.
			// The refr's speed is not modified, nor is magicka deducted from the player.
			// Return true if the player can successfully throw this refr.
			bool InitPreviewTrajectory(const std::shared_ptr<CoopPlayer>& a_p);

			// Set this released refr's initial trajectory information.
			void InitTrajectory(const std::shared_ptr<CoopPlayer>& a_p);

			// Trajectory type.
			ProjectileTrajType trajType;
			// Last recorded target position to direct at for this released refr.
			RE::NiPoint3 lastSetTargetPosition;
			// Last recorded velocity for this released refr.
			RE::NiPoint3 lastSetVelocity;
			// Position the refr is released from.
			RE::NiPoint3 releasePos;
			// Velocity set at release.
			RE::NiPoint3 releaseVelocity;
			// Local position offset for the crosshair world position 
			// relative to the target's world position.
			RE::NiPoint3 targetLocalPosOffset;
			// Initial end position for the thrown refr's trajectory.
			RE::NiPoint3 trajectoryEndPos;
			// Actor node targeted by this thrown refr.
			RE::NiPointer<RE::NiAVObject> targetedActorNode;
			// Refr handle for the refr's target.
			RE::ObjectRefHandle targetRefrHandle;
			// Time point indicating when this released refr first hit another object.
			std::optional<SteadyClock::time_point> firstHitTP;
			// Time point when this refr was released.
			std::optional<SteadyClock::time_point> releaseTP;
			// Set of FIDs for the refrs that this refr has hit since release.
			std::set<RE::FormID> hitRefrFIDs;
			// Can the released refr reach the target intercept position
			// if there are no collisions along the trajectory?
			// Always true for homing and aim direction projectiles.
			bool canReachTarget;
			// Is this refr currently homing in on the target?
			bool isHoming;
			// Was this refr thrown or just dropped?
			bool isThrown;
			// Has this refr started homing in on the target?
			bool startedHomingIn;
			// Factor to apply to this refr's speed on release.
			// Slows down the release speed of this refr if the releasing player is low on magicka.
			// [0.25, 1.0]
			float magickaOverflowSlowdownFactor;
			// Time in seconds to reach the target at the time of the refr's release.
			double initialTimeToTarget;
			// Refr's pitch at launch ('up' is positive, 'down' is negative 
			// (opposite of game's convention)).
			double launchPitch;
			// Refr's yaw at launch (Cartesian convention).
			double launchYaw;
			// Refr's speed at launch.
			double releaseSpeed;
		};

		// Handles manipulation of and bookkeeping for grabbed and released refrs.
		struct RefrManipulationManager
		{
			RefrManipulationManager()
			{
				ClearAll();
			}

			RefrManipulationManager(const RefrManipulationManager& a_other) 
			{
				collidedRefrFIDPairs = a_other.collidedRefrFIDPairs;
				grabbedRefrHandlesToInfoIndices = a_other.grabbedRefrHandlesToInfoIndices;
				releasedRefrHandlesToInfoIndices = a_other.releasedRefrHandlesToInfoIndices;
				isGrabbing = a_other.isGrabbing;
				isAutoGrabbing = a_other.isAutoGrabbing;
				reqSpecialHitDamageAmount = a_other.reqSpecialHitDamageAmount;
				totalThrownRefrMagickaCost = a_other.totalThrownRefrMagickaCost;

				queuedReleasedRefrContactEvents.clear();
				std::for_each
				(
					a_other.queuedReleasedRefrContactEvents.begin(),
					a_other.queuedReleasedRefrContactEvents.end(),
					[this](const std::unique_ptr<HavokContactEventInfo>& a_info) 
					{
						if (a_info && a_info.get()) 
						{
							queuedReleasedRefrContactEvents.emplace_back
							(
								std::make_unique<HavokContactEventInfo>(*a_info.get())
							);
						}
					}
				);
				
				grabbedRefrInfoList.clear();
				std::for_each
				(
					a_other.grabbedRefrInfoList.begin(), a_other.grabbedRefrInfoList.end(),
					[this](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
					{
						if (a_info && a_info.get()) 
						{
							grabbedRefrInfoList.emplace_back
							(
								std::make_unique<GrabbedReferenceInfo>(*a_info.get())
							);
						}
					}
				);
				
				releasedRefrInfoList.clear();
				std::for_each
				(
					a_other.releasedRefrInfoList.begin(), a_other.releasedRefrInfoList.end(),
					[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info) 
					{
						if (a_info && a_info.get())
						{
							releasedRefrInfoList.emplace_back
							(
								std::make_unique<ReleasedReferenceInfo>(*a_info.get())
							);
						}
					}
				);
			}

			RefrManipulationManager(RefrManipulationManager&& a_other) 
			{
				collidedRefrFIDPairs = std::move(a_other.collidedRefrFIDPairs);
				grabbedRefrHandlesToInfoIndices = std::move
				(
					a_other.grabbedRefrHandlesToInfoIndices
				);
				releasedRefrHandlesToInfoIndices = std::move
				(
					a_other.releasedRefrHandlesToInfoIndices
				);
				isGrabbing = std::move(a_other.isGrabbing);
				isAutoGrabbing = std::move(a_other.isAutoGrabbing);
				reqSpecialHitDamageAmount = std::move(a_other.reqSpecialHitDamageAmount);
				totalThrownRefrMagickaCost = std::move(a_other.totalThrownRefrMagickaCost);
				
				queuedReleasedRefrContactEvents.swap(a_other.queuedReleasedRefrContactEvents);
				grabbedRefrInfoList.swap(a_other.grabbedRefrInfoList);
				releasedRefrInfoList.swap(a_other.releasedRefrInfoList);
			}

			RefrManipulationManager& operator=(const RefrManipulationManager& a_other) 
			{
				collidedRefrFIDPairs = a_other.collidedRefrFIDPairs;
				grabbedRefrHandlesToInfoIndices = a_other.grabbedRefrHandlesToInfoIndices;
				releasedRefrHandlesToInfoIndices = a_other.releasedRefrHandlesToInfoIndices;
				isGrabbing = a_other.isGrabbing;
				reqSpecialHitDamageAmount = a_other.reqSpecialHitDamageAmount;
				totalThrownRefrMagickaCost = a_other.totalThrownRefrMagickaCost;

				queuedReleasedRefrContactEvents.clear();
				std::for_each
				(
					a_other.queuedReleasedRefrContactEvents.begin(),
					a_other.queuedReleasedRefrContactEvents.end(),
					[this](const std::unique_ptr<HavokContactEventInfo>& a_info) 
					{
						if (a_info && a_info.get()) 
						{
							queuedReleasedRefrContactEvents.emplace_back
							(
								std::make_unique<HavokContactEventInfo>(*a_info.get())
							);
						}
					}
				);
				
				grabbedRefrInfoList.clear();
				std::for_each
				(
					a_other.grabbedRefrInfoList.begin(), a_other.grabbedRefrInfoList.end(),
					[this](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
					{
						if (a_info && a_info.get())
						{
							grabbedRefrInfoList.emplace_back
							(
								std::make_unique<GrabbedReferenceInfo>(*a_info.get())
							);
						}
					}
				);
				
				releasedRefrInfoList.clear();
				std::for_each
				(
					a_other.releasedRefrInfoList.begin(), a_other.releasedRefrInfoList.end(),
					[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info) 
					{
						if (a_info && a_info.get())
						{
							releasedRefrInfoList.emplace_back
							(
								std::make_unique<ReleasedReferenceInfo>(*a_info.get())
							);
						}
					}
				);

				return *this;
			}

			RefrManipulationManager& operator=(RefrManipulationManager&& a_other) 
			{
				collidedRefrFIDPairs = std::move(a_other.collidedRefrFIDPairs);
				grabbedRefrHandlesToInfoIndices = std::move
				(
					a_other.grabbedRefrHandlesToInfoIndices
				);
				releasedRefrHandlesToInfoIndices = std::move
				(
					a_other.releasedRefrHandlesToInfoIndices
				);
				isGrabbing = std::move(a_other.isGrabbing);
				isAutoGrabbing = std::move(a_other.isAutoGrabbing);
				reqSpecialHitDamageAmount = std::move(a_other.reqSpecialHitDamageAmount);
				totalThrownRefrMagickaCost = std::move(a_other.totalThrownRefrMagickaCost);
				
				queuedReleasedRefrContactEvents.swap(a_other.queuedReleasedRefrContactEvents);
				grabbedRefrInfoList.swap(a_other.grabbedRefrInfoList);
				releasedRefrInfoList.swap(a_other.releasedRefrInfoList);
				return *this;
			}
			
			// Get the current number of grabbed refrs handled by this manager.
			inline const uint8_t GetNumGrabbedRefrs() const noexcept 
			{
				return grabbedRefrInfoList.size(); 
			}

			// Get the current number of released refrs handled by this manager.
			inline const uint8_t GetNumReleasedRefrs() const noexcept 
			{
				return releasedRefrInfoList.size();
			}

			// Is the given refr designated as grabbed?
			inline const bool IsGrabbed(const RE::ObjectRefHandle& a_handle) 
			{
				return IsManaged(a_handle, true); 
			}

			// Is the given refr designated as released?
			inline const bool IsReleased(const RE::ObjectRefHandle& a_handle) 
			{
				return IsManaged(a_handle, false); 
			}

			// Add the given refr to the managed grabbed refrs data set,
			// set its grab time point, and ragdoll the refr, if it is an actor,
			// to allow for positional manipulation.
			// Returns the next open index in the grabbed refr list
			// at which the requested refr was inserted, 
			// or -1 if the requested refr could not be grabbed.
			int32_t AddGrabbedRefr
			(
				const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle
			);

			// Add the given refr to the managed released refrs data set.
			// Also set its release trajectory information (dropped or thrown)
			// to use when guiding its motion upon release.
			// The given magicka cost will modify its release speed, 
			// depending on how much magicka the player has left.
			// Returns the next open index in the released refr list
			// at which the requested refr was inserted, 
			// or -1 if the requested refr could not be released.
			int32_t AddReleasedRefr
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::ObjectRefHandle& a_handle,
				const float& a_magickaCost
			);

			// Returns true if there is room for another grabbed refr.
			bool CanGrabAnotherRefr();
			
			// Returns true if the given refr is valid, not already managed, 
			// and the maximum number of grabbed refr has not been reached.
			bool CanGrabRefr(const RE::ObjectRefHandle& a_handle);

			// Returns true if this manager can manipulate the grabbed refr at the given index.
			bool CanManipulateGrabbedRefr
			(
				const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index
			);

			// Clear all grabbed/released refr data and queued contact events.
			// Manager is no longer set as grabbing objects or auto-grabbing. 
			void ClearAll();

			// Remove grabbed actors.
			void ClearGrabbedActors(const std::shared_ptr<CoopPlayer>& a_p);

			// Clear given grabbed refr, if managed.
			void ClearGrabbedRefr(const RE::ObjectRefHandle& a_handle);

			// Clear all managed grabbed refrs.
			void ClearGrabbedRefrs() noexcept;

			// Clear any inactive released refrs (no active collision)
			// and refresh handle-to-index mappings upon successful erasure.
			void ClearInactiveReleasedRefrs();

			// Clear out released refrs with handles that have become invalid
			// and refresh handle-to-index mappings upon successful erasure.
			void ClearInvalidRefrs(bool&& a_grabbed);

			// Release/drop the given player.
			void ClearPlayerIfGrabbed(const std::shared_ptr<CoopPlayer>& a_p);

			// Remove the given refr from the grabbed or released refrs list
			// and refresh handle-to-index mappings upon successful erasure.
			void ClearRefr(const RE::ObjectRefHandle& a_handle);

			// Clear given released refr, if managed.
			void ClearReleasedRefr(const RE::ObjectRefHandle& a_handle);

			// Clear all managed released refrs.
			void ClearReleasedRefrs() noexcept;
			
			// Normalize and return the grab bind hold time, 
			// which directly influences the speed at which released refrs are thrown.
			float GetReleasedRefrBindHoldTimeFactor(const std::shared_ptr<CoopPlayer>& a_p);

			// Return the magicka cost for throwing the given refr.
			float GetThrownRefrMagickaCost
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				RE::TESObjectREFR* a_refrToThrow
			);
			// Factor by which to slow down all thrown refrs' release speeds.
			// The more magicka used up below a magicka level of 0, the slower the release speed.
			float GetThrownRefrMagickaOverflowSlowdownFactor
			(
				const std::shared_ptr<CoopPlayer>& a_p, const float& a_totalMagickaCost
			);

			// For each queued contact event, get both colliding objects 
			// and check if the thrown object should damage the hit object
			// and if the thrown object itself should receive damage.
			// Handle the collision and apply damage as needed.
			void HandleQueuedContactEvents(const std::shared_ptr<CoopPlayer>& a_p);

			// Is the given refr handled either as a grabbed refr or a released refr?
			const bool IsManaged(const RE::ObjectRefHandle& a_handle, bool a_grabbed);

			// NOTE:
			// Not working as intended and unused for now.
			// Move unloaded or far away grabbed objects (not actors) to the player.
			void MoveUnloadedGrabbedObjectsToPlayer(const std::shared_ptr<CoopPlayer>& a_p);

			// Refresh the refr handle-to-list index mappings 
			// for either all managed grabbed or released refrs.
			void RefreshHandleToIndexMappings(const bool& a_grabbed);
			
			// Cache the total magicka cost for throwing all this player's grabbed refrs.
			// Compute the total from the grabbed refrs or released refrs list.
			void SetTotalThrownRefrMagickaCost
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				bool&& a_checkGrabbedRefrsList
			);

			// Toggle collision state for all grabbed refrs.
			void ToggleGrabbedRefrCollisions();

			// List of havok contact events between released refrs and other objects.
			// Enqueued by a havok contact listener, removed and handled by this manager.
			std::list<std::unique_ptr<HavokContactEventInfo>> queuedReleasedRefrContactEvents;

			// Mutex for queueing/handling havok contact events.
			std::mutex contactEventsQueueMutex;

			// Maps grabbed refr handles to their positions in the managed grabbed refrs list.
			std::unordered_map<RE::ObjectRefHandle, uint8_t> grabbedRefrHandlesToInfoIndices;

			// Maps released refr handles to their positions in the managed released refrs list.
			std::unordered_map<RE::ObjectRefHandle, uint8_t> releasedRefrHandlesToInfoIndices;

			// Set of pairs of FIDs for the two objects that collide during a havok contact event.
			// One of the FIDs is always a released refr.
			std::unordered_set<std::pair<RE::FormID, RE::FormID>> collidedRefrFIDPairs;

			// List of grabbed refr info for each managed refr.
			std::vector<std::unique_ptr<GrabbedReferenceInfo>> grabbedRefrInfoList;

			// List of released refr info for each managed refr.
			std::vector<std::unique_ptr<ReleasedReferenceInfo>> releasedRefrInfoList;

			// Is the manager handling auto-grabbed clutter?
			bool isAutoGrabbing;

			// Should the manager handle grabbed refrs? 
			// If false, all grabbed refrs have been released.
			bool isGrabbing;

			// Damage to apply in sent hit data when bonk, slap, or splat hit event
			// is handled in the Hit Event Handler. 
			// Only have to cache one damage amount since hit events 
			// are sent and fully handled one at a time.
			float reqSpecialHitDamageAmount;

			// Cached total magicka to expend when releasing all grabbed objects.
			float totalThrownRefrMagickaCost;

		};

		// Stores physical movement data for a targeted refr.
		struct RefrTargetMotionState
		{
			RefrTargetMotionState() :
				targetRefrHandle(RE::ObjectRefHandle()),
				apiVel(RE::NiPoint3()),
				apiAccel(RE::NiPoint3()),
				cPos(RE::NiPoint3()),
				cVel(RE::NiPoint3()),
				cAccelPerFrame(RE::NiPoint3()),
				pPos(RE::NiPoint3()),
				pVel(RE::NiPoint3()),
				pAccelPerFrame(RE::NiPoint3()),
				toiVel(RE::NiPoint3()),
				toiAccel(RE::NiPoint3()),
				firstUpdate(true),
				apiSpeedDelta(0.0f),
				apiYawAngDelta(0.0f),
				toiSpeedDelta(0.0f),
				toiYawAngDelta(0.0f),
				cYaw(0.0f),
				cYawAngDeltaPerFrame(0.0f),
				pYaw(0.0f),
				pYawAngDeltaPerFrame(0.0f),
				cSpeedDeltaPerFrame(0.0f),
				avgDataFrameCount(0),
				lastUpdateTP(SteadyClock::now())
			{ }

			// Refresh all stored data.
			inline void Refresh()
			{
				targetRefrHandle = RE::ObjectRefHandle();
				apiVel =
				apiAccel =
				cPos =
				cVel =
				cAccelPerFrame =
				pPos =
				pVel =
				pAccelPerFrame =
				toiVel =
				toiAccel = RE::NiPoint3();

				apiSpeedDelta =
				apiYawAngDelta =
				cSpeedDeltaPerFrame = 
				cYaw =
				cYawAngDeltaPerFrame =
				pYaw =
				pYawAngDeltaPerFrame =
				toiSpeedDelta =
				toiYawAngDelta = 0.0f;

				avgDataFrameCount = 0;
				firstUpdate = true;
				lastUpdateTP = SteadyClock::now();
			}

			// Compute new motion data for the target refr.
			void UpdateMotionState(RE::ObjectRefHandle a_targetRefrHandle);

			// Targeted refr.
			RE::ObjectRefHandle targetRefrHandle;

			// Average per interval (api) and total over an interval (toi).
			RE::NiPoint3 apiAccel;
			RE::NiPoint3 apiVel;
			RE::NiPoint3 toiAccel;
			RE::NiPoint3 toiVel;
			
			float apiSpeedDelta;
			float apiYawAngDelta;
			float toiSpeedDelta;
			float toiYawAngDelta;

			// Previous.
			RE::NiPoint3 pAccelPerFrame;
			RE::NiPoint3 pPos;
			RE::NiPoint3 pVel;
			float pYaw;
			float pYawAngDeltaPerFrame;

			// Current.
			RE::NiPoint3 cAccelPerFrame;
			RE::NiPoint3 cPos;
			RE::NiPoint3 cVel;
			float cYaw;
			float cYawAngDeltaPerFrame;
			float cSpeedDeltaPerFrame;

			// Number of frames to accumulate data points before updating
			// total-over-interval and average-per-interval motion members.
			// Also the number of frames before all data members are populated.
			const uint8_t FRAMES_BETWEEN_AVG_DATA_UPDATES = 15;
			// New target selected.
			bool firstUpdate;
			// Number of frames since the last average data update.
			uint8_t avgDataFrameCount;
			// Time point at which data was last updated.
			SteadyClock::time_point lastUpdateTP;
		};

		// NOTE: 
		// Much of the trajectory stuff is 'good enough for now'
		// and will receive more attention in the future
		// when I'm not completely spent.
		// 
		// Holds projectile trajectory information at release.
		struct ManagedProjTrajectoryInfo
		{
		public:
			ManagedProjTrajectoryInfo() :
				trajType(ProjectileTrajType::kAimDirection),
				canReachTarget(true),
				isPhysicalProj(false),
				startedHomingIn(false),
				startedHomingTP(std::nullopt),
				g(9.81 * HAVOK_TO_GAME),
				mu(0.099609375),
				launchPitch(0.0),
				launchYaw(0.0),
				maxReleaseSpeed(0.0),
				projGravFactor(1.0),
				releaseSpeed(0.0),
				initialTrajTimeToTarget(0.0),
				targetedActorNode(),
				targetRefrHandle(RE::ObjectRefHandle()),
				releasePos(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3())
			{ }

			// Set trajectory on construction.
			ManagedProjTrajectoryInfo
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::ObjectRefHandle& a_projectileHandle, 
				RE::NiPoint3& a_initialVelocityOut, 
				const ProjectileTrajType& a_trajType
			)
			{
				SetTrajectory(a_p, a_projectileHandle, a_initialVelocityOut, a_trajType);
			}

			ManagedProjTrajectoryInfo
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				RE::BGSProjectile* a_projectileBase, 
				RE::TESObjectWEAP* a_weaponSource,
				RE::EffectSetting* a_magicEffectSource,
				const RE::NiPoint3& a_releasePos,
				const ProjectileTrajType& a_trajType
			)
			{
				SetTrajectory
				(
					a_p, 
					a_projectileBase, 
					a_weaponSource,
					a_magicEffectSource,
					a_releasePos,
					a_trajType
				);
			}

			// Refresh all data.
			inline void Refresh()
			{
				trajType = ProjectileTrajType::kAimDirection;
				canReachTarget = true;
				isPhysicalProj = false;
				startedHomingIn = false;
				startedHomingTP = std::nullopt;
				g = 9.81 * HAVOK_TO_GAME;
				mu = 0.099609375;

				launchPitch =
				launchYaw =
				maxReleaseSpeed =
				projGravFactor =
				releaseSpeed =
				initialTrajTimeToTarget = 0.0;

				targetedActorNode.reset();
				targetRefrHandle = RE::ObjectRefHandle();

				releasePos = 
				targetLocalPosOffset =
				trajectoryEndPos = RE::NiPoint3();
			}

			// Calculate the position at which the projectile should be directed 
			// to hit the current target position.
			// Return the time taken to hit the target at the computed intercept position
			// through the outparam.
			// 'NaN' or 'inf' if the projectile cannot hit the target position.
			// NOTE:
			// a_adjustReleaseSpeed is currently unused: 
			// Can optionally adjust the projectile's release speed to account for air resistance 
			// and hit the target position.
			RE::NiPoint3 CalculatePredInterceptPos
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const bool& a_adjustReleaseSpeed,
				double& a_timeToTarget
			);

			// Accounts for linear air drag.
			// Get the release speed to set for this projectile,
			// given the target position expressed as a point in a 2D plane:
			// The plane can be either of the two that lie parallel to the line 
			// that connects the release position and the target position,
			// and has a normal axis that is perpendicular to the game's Z axis.
			// The XY coordinate can be thought of 
			// as the traditional 'x' coordinate in the Cartesian plane, 
			// and the Z coordinate can likewise be thought of as the 'y' coordinate.
			double GetReleaseSpeedToTarget
			(
				const double& a_xy, const double& a_z, const double& a_launchPitch
			);

			// Get a rough approximation of the minimum launch pitch 
			// ('+' when aiming up, '-' when aiming down) to hit the target position.
			float GetRoughMinLaunchPitch(const std::shared_ptr<CoopPlayer>& a_p);

			// Set the physical projectile (non-magical) flag, initial release speed, 
			// max release speed, release position, and physical constants for the trajectory.
			// The given release speed is set as a fallback default.
			void SetInitialBaseProjectileData
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				const RE::ObjectRefHandle& a_projectileHandle, 
				const float& a_releaseSpeed
			);

			// Set based on a base projectile form.
			// Used to obtain trajectory data when a projectile has not been fired yet.
			// If wishing to set a magic projectile's trajectory, specify the magic effect
			// associated with the projectile; nullptr if not a magical projectile.
			// If wishing to set a physical projectile's trajectory, specify the weapon
			// associated with the projectile; nullptr if not a physical projectile.
			void SetInitialBaseProjectileData
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				RE::BGSProjectile* a_projectileBase, 
				RE::TESObjectWEAP* a_weaponSource,
				RE::EffectSetting* a_magicEffectSource, 
				const RE::NiPoint3& a_releasePos
			);

			// Sets up the initial trajectory data for the given projectile
			// based on the given starting velocity (which is modified) and the trajectory type.			
			void SetTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::ObjectRefHandle& a_projectileHandle, 
				RE::NiPoint3& a_initialVelocityOut,
				const ProjectileTrajType& a_trajType
			);
			
			// Set based on a base projectile form.
			// Used to obtain trajectory data when a projectile has not been fired yet.
			// If wishing to set a magic projectile's trajectory, specify the magic effect
			// associated with the projectile; nullptr if not a magical projectile.
			// If wishing to set a physical projectile's trajectory, specify the weapon
			// associated with the projectile; nullptr if not a physical projectile.
			void SetTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				RE::BGSProjectile* a_projectileBase, 
				RE::TESObjectWEAP* a_weaponSource,
				RE::EffectSetting* a_magicEffectSource,
				const RE::NiPoint3& a_releasePos,
				const ProjectileTrajType& a_trajType
			);

			// The trajectory type to use for this projectile.
			ProjectileTrajType trajType;
			// Position at which the projectile is released.
			RE::NiPoint3 releasePos;
			// Local position offset for the crosshair world position 
			// relative to the target's world position.
			RE::NiPoint3 targetLocalPosOffset;
			// Initial end position for the projectile's trajectory.
			RE::NiPoint3 trajectoryEndPos;
			// Actor node targeted by this projectile.
			RE::NiPointer<RE::NiAVObject> targetedActorNode;
			// Targeted refr's handle.
			RE::ObjectRefHandle targetRefrHandle;
			// If it exists, time point when the projectile started homing in on the target.
			// Otherwise, the projectile has not started homing yet.
			std::optional<SteadyClock::time_point> startedHomingTP;
			// Can the released refr reach the target intercept position
			// if there are no collisions along the trajectory?
			// Always true for homing and aim direction projectiles.
			bool canReachTarget;
			// True if an arrow/bolt (physical) projectile, false if a magic projectile.
			bool isPhysicalProj;
			// Has the projectile started homing in on the target position yet?
			bool startedHomingIn;
			// Gravitational constant to use when guiding the projectile along the trajectory.
			double g;
			// Time to reach the target (in seconds) based on 
			// the projectile's initial trajectory set on release.
			double initialTrajTimeToTarget;
			// Projectile's pitch at launch.
			// IMPORTANT NOTE:
			// ('up' is positive, 'down' is negative (opposite of game's convention)).
			double launchPitch;
			// IMPORTANT NOTE:
			// Projectile's yaw at launch (Cartesian, NOT game's convention).
			double launchYaw;
			// Maximum speed this projectile can be released at.
			double maxReleaseSpeed;
			// Drag coefficient to use when guiding the projectile along the trajectory.
			double mu;
			// Multiplier from the base projectile's data 
			// to multiply the projectile's gravitiation constant by.
			double projGravFactor;
			// Speed at which to release the projectile.
			double releaseSpeed;

		private: 
			void SetTrajectory
			(
				const std::shared_ptr<CoopPlayer>& a_p, 
				const RE::NiPoint3& a_releasePos,
				const ProjectileTrajType& a_trajType,
				const float& a_initialYaw,
				const bool& a_setStraightTrajectory
			);
		};

		// Handles managed projectiles fired by the player.
		struct ManagedProjectileHandler
		{
			ManagedProjectileHandler()
			{
				Clear();
			}

			// Clear out managed projectile FIDs.
			inline void Clear()
			{
				managedProjHandleToTrajInfoMap.clear();
			}

			// Precondition: 
			// The projectile must be managed already.
			// Check if managed first before calling.
			inline std::unique_ptr<ManagedProjTrajectoryInfo>& GetInfo
			(
				const RE::ObjectRefHandle& a_projectileHandle
			)
			{
				return managedProjHandleToTrajInfoMap.at(a_projectileHandle);
			};

			// Returns true if the given projectile is managed by this handler.
			inline const bool IsManaged(const RE::ObjectRefHandle& a_projectileHandle) 
			{
				if (!Util::HandleIsValid(a_projectileHandle))
				{
					return false;
				}

				return managedProjHandleToTrajInfoMap.contains(a_projectileHandle); 
			}

			// Remove the given projectile from the handled projectiles list
			// and clear out its data.
			inline void Remove(const RE::ObjectRefHandle& a_projectileHandle) 
			{
				if (!Util::HandleIsValid(a_projectileHandle))
				{
					return;
				}

				const auto iter = managedProjHandleToTrajInfoMap.find(a_projectileHandle); 
				if (iter != managedProjHandleToTrajInfoMap.end())
				{
					managedProjHandleToTrajInfoMap.erase(iter);
				}
			}

			// Insert the given projectile to the handled list if there is room.
			// Clear out inactive/invalid projectiles to make room as necessary.
			// Set the projectile's trajectory info using the given initial velocity 
			// (which is modified), and the given trajectory type.
			void Insert
			(
				const std::shared_ptr<CoopPlayer>& a_p,
				const RE::ObjectRefHandle& a_projectileHandle, 
				RE::NiPoint3& a_initialVelocityOut,
				const ProjectileTrajType& a_trajType
			);

			// Holds trajectory info for managed projectiles, indexed by their handles.
			std::unordered_map<RE::ObjectRefHandle, std::unique_ptr<ManagedProjTrajectoryInfo>> 
			managedProjHandleToTrajInfoMap;
		};

		TargetingManager();
		// Delayed initialization after the player is default-constructed 
		// and the player shared pointer is added to the list of co-op players 
		// in the global data holder.
		void Initialize(std::shared_ptr<CoopPlayer> a_p);

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		// Clear out currently-targeted proximity refr for activation.
		inline void ClearProximityRefr() { proximityRefrHandle.reset(); }

		// Clear out all targeted actor/refr handles.
		inline void ClearTargetHandles() 
		{
			// Target actors.
			aimCorrectionTargetHandle = 
			closestHostileActorHandle = 
			selectedTargetActorHandle = RE::ActorHandle();
			// Interaction/target refrs.
			activationRefrHandle = 
			aimTargetLinkedRefrHandle = 
			crosshairPickRefrHandle = 
			crosshairRefrHandle = 
			prevCrosshairRefrHandle =
			proximityRefrHandle = RE::ObjectRefHandle();
		}

		// Get the farthest distance an object can be located from the player 
		// while activating an object.
		inline float GetMaxActivationDist() const
		{
			// Can activate objects that are further away when mounted.
			return 
			{
				(coopActor->IsOnMount()) ? 
				Settings::fMountedActivationReachMult * maxReachActivationDist :
				maxReachActivationDist
			};
		}

		// Reset the player's crosshair scaleform position to its default position straight away.
		inline void ResetCrosshairPosition()
		{
			// Offset left or right about the center of the screen based on player index.
			crosshairScaleformPos.x = 
			(
				DebugAPI::screenResX / 2.0f + 
				100.0f * (fmod(playerID, 2) * 2 - 1) * 
				ceil((playerID + 1) / 2.0f)
			);
			crosshairScaleformPos.y = DebugAPI::screenResY / 2.0f;
			crosshairScaleformPos.z = 0.0f;
			crosshairLocalPosOffset = 
			crosshairLastMovementHitPosOffset = 
			crosshairInitialMovementHitPosOffset = RE::NiPoint3();
			// No longer selecting a refr, so clear out pixel XY deltas.
			crosshairOnRefrPixelXYDeltas = glm::vec2(0.0f, 0.0f);
		}

		// Clear out aim correction, ranged package target, and selected crosshair target handles.
		inline void ResetTargeting()
		{
			ClearTarget(TargetActorType::kAimCorrection);
			ClearTarget(TargetActorType::kLinkedRefr);
			ClearTarget(TargetActorType::kSelected);
		}

		// Update external crosshair message request with the given type, message text,
		// list of other message types to delay, and max display time.
		inline void SetCrosshairMessageRequest
		(
			CrosshairMessageType&& a_type, 
			const RE::BSFixedString a_text, 
			std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, 
			float a_secsMaxDisplayTime = 0.0f,
			bool a_updateSetTP = true
		)
		{
			SetCurrentCrosshairMessage
			(
				true, 
				std::move(a_type), 
				a_text, 
				std::move(a_delayedMessageTypes), 
				a_secsMaxDisplayTime,
				a_updateSetTP
			);
		}

		// Update the external/current crosshair message request with the given type, message text,
		// list of other message types to delay, and max display time.
		// Only update if another message is not displayed,
		// the given message type is not delayed, or if the delay has passed.
		inline void SetCurrentCrosshairMessage
		(
			bool&& a_extRequest, 
			CrosshairMessageType&& a_type, 
			const RE::BSFixedString a_text, 
			std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, 
			float a_secsMaxDisplayTime = 0.0f,
			bool a_updateSetTP = true
		)
		{
			{
				std::unique_lock<std::mutex> lock(crosshairMessageMutex, std::try_to_lock);
				// Only set if another thread is not already doing so.
				if (lock)
				{
					const bool noDelay = lastCrosshairMessage->delayedMessageTypes.empty();
					const bool isDelayedType = 
					(
						!noDelay && lastCrosshairMessage->delayedMessageTypes.contains(a_type)
					);
					const bool delayPassed = 
					(
						Util::GetElapsedSeconds(lastCrosshairMessage->setTP) > 
						lastCrosshairMessage->secsMaxDisplayTime
					);
					if (noDelay || !isDelayedType || delayPassed)
					{
						if (a_extRequest) 
						{
							extCrosshairMessage->Update
							(
								std::move(a_type), 
								a_text, 
								std::move(a_delayedMessageTypes), 
								std::move(a_secsMaxDisplayTime),
								a_updateSetTP
							);
						}
						else
						{
							crosshairMessage->Update
							(
								std::move(a_type), 
								a_text, 
								std::move(a_delayedMessageTypes), 
								std::move(a_secsMaxDisplayTime),
								a_updateSetTP
							);
						}
					}
				}
			}
		}

		// Signal the reference manipulation manager to handle grabbed objects when set, 
		// and handle released refrs when unset.
		// Auto-grabbing is mutually exclusive with normal grabbing of targeted objects.
		inline void SetIsGrabbing(bool&& a_set) 
		{ 
			rmm->isGrabbing = a_set; 
			if (a_set) 
			{
				rmm->isAutoGrabbing = false;
			}
		}

		// Set the player's last position and facing angle when cycling nearby activation refrs.
		inline void SetLastActivationCyclingOrientation()
		{
			lastActivationReqPos = coopActor->data.location;
			lastActivationFacingAngle = coopActor->GetHeading(false);
		}

		//
		// Member funcs
		//

		// Clear the cached actor/refr handle for the given target type.
		void ClearTarget(const TargetActorType& a_targetType);

		// Draw an indicator on the player's aim correction target.
		void DrawAimCorrectionIndicator();

		// Draw the aim pitch indicator arrow for this player
		// if they are adjusting their aim pitch manually or if aim pitch was reset.
		void DrawAimPitchIndicator();

		// Draw this player's active crosshair (lines and outlines) if it hasn't fully faded out.
		void DrawCrosshair();

		// Draw the four main body 'lines'/'prongs' of the retro-style crosshair.
		void DrawCrosshairLines();

		// Draw an outline, with a thickness multiplier given by the outline index
		// and colored with the given RGBA value, 
		// around the four prongs of the retro-style crosshair.
		void DrawCrosshairOutline(float&& a_outlineIndex, const uint32_t& a_outlineRGBA);

		// Draw a colored 'quest marker' indicator above the player's head, 
		// dependent on the player's indicator visibility setting.
		void DrawPlayerIndicator();

		// Draw a crosshair similar in style to the Skyrim's default crosshair.
		void DrawSkyrimStyleCrosshair();

		// Draw trajectories for projectiles that the player is attempting to release.
		void DrawTrajectories();

		// Draw trajectory (no air drag) based on the given launch parameters.
		// Can handle weapon/magic projectiles, or thrown refrs.
		// Can choose to cap the velocity, and thus the displacement, per time slice.
		// Projected trajectory is most accurate when the target position
		// is close to the release position and when the framerate does not vary much 
		// from frame to frame.
		void DrawTrajectory
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
			RE::ObjectRefHandle a_projHandle = RE::ObjectRefHandle()
		);

		// Get the closest targetable actor to the source refr 
		// (or player if no source refr handle is given)
		// using the given FOV in radians centered at their aiming angle 
		// (LS or heading angle in world or screen space),
		// and the given maximum range to consider targets 
		// (screen pixel distance or world XY or XYZ distance).
		// If range is given as '-1', ignore the range check.
		// If combat-dependent selection is requested, only consider hostile actors, 
		// unless attempting to heal a target.
		// If screen position checks are requested,
		// all world positions are converted to screen positions before performing FOV checks,
		// the FOV window is centered about the player's center in screen space, 
		// and the given range should be given in pixels.
		RE::ActorHandle GetClosestTargetableActorInFOV
		(
			const float& a_fovRads,
			RE::ObjectRefHandle a_sourceRefrHandle = RE::ObjectRefHandle(),
			const bool& a_useXYDistance = false, 
			const float& a_range = -1.0f, 
			const bool& a_combatDependentSelection = true,
			const bool& a_useScreenPositions = false
		);
		
		// Get detection-level-modified gradient RGB value.
		// NOTE: 
		// The raw detection level ranges from -1000 to 1000,
		// whereas the modified detection level to pass in here should range from 0 to 100.
		uint32_t GetDetectionLvlRGB(const float& a_detectionLvl, bool&& a_fromRawLevel);

		// Get actor level-difference-modified RGB value
		// which represents the difference in level between the player and the given actor.
		uint32_t GetLevelDifferenceRGB(const RE::ActorHandle& a_actorHandle);
		
		// Get a list of all nearby refrs that are the same type as the given refr.
		// Can compare based on having the exact same base form, 
		// or on having the same base form type.
		// E.g. Consider two book refrs: "Lusty Argonian Maid" and "Cats of Skyrim"
		// Same base form (Not the same book) => 'false',
		// Same form type (TESObjectBOOK) => 'true'
		const std::vector<RE::ObjectRefHandle>& GetNearbyRefrsOfSameType
		(
			RE::ObjectRefHandle a_refrHandle, 
			RefrCompType&& a_compType = RefrCompType::kSameBaseForm
		);

		// If cycling nearby objects, cycle one time and return the resulting refr's handle.
		// Otherwise, return the selected crosshair refr's handle.
		RE::ObjectRefHandle GetNextObjectToActivate();

		// Get the ranged target to set for the player's ranged attack package
		// based on the given attack source form.
		RE::ObjectRefHandle GetRangedPackageTargetRefr(RE::TESForm* a_rangedAttackSource);

		// Get the player's current ranged target actor, 
		// which is the crosshair refr if an actor is selected, 
		// the aim correction target if the option is enabled 
		// while not selecting an actor with the crosshair,
		// or the ranged package's aim target linked refr if it is not the player.
		RE::ActorHandle GetRangedTargetActor();

		// Sounds like a lot of- hoopla! Sounds like a lot of- hoopla! 
		// Sounds like a lot of- hoopla! Hoooooplaaaa! *Bonk*
		void HandleBonk
		(
			RE::ActorHandle a_hitActorHandle, 
			RE::ObjectRefHandle a_releasedRefrHandle, 
			float a_collidingMass,
			const RE::NiPoint3& a_collidingVelocity,
			const RE::NiPoint3& a_contactPos
		);

		// If the QuickLoot mod is installed, handle opening/closing of the QuickLoot menu
		// when the player moves their crosshair on/off a container.
		void HandleQuickLootMenu();

		// Handle positioning and collisions for the player's grabbed and released refrs.
		void HandleReferenceManipulation();

		// Splat! Ooh, that's going to leave a mark!
		void HandleSplat(RE::ActorHandle a_thrownActorHandle, const uint32_t& a_hitCount);

		// Is the given refr valid for targeting with the player's crosshair:
		// - Handle valid, visible, and not disabled/deleted.
		// - Not self, or part of the selection blacklist, 
		// or another player, unless player selection is enabled.
		// - Player has LOS or has not lost LOS for too long.
		bool IsRefrValidForCrosshairSelection(RE::ObjectRefHandle a_refrHandle);

		// Choose a raycast hit result to use as the crosshair's selected refr.
		// The chosen result is influenced by whether or not the player is in combat
		// and whether or not their crosshair is active for target selection.
		// The crosshair is considered active for selection when moving or when over a target.
		Raycast::RayResult PickRaycastHitResult
		(
			const std::vector<Raycast::RayResult>& a_raycastResults,
			const bool& a_inCombat,
			const bool&& a_crosshairActiveForSelection,
			bool a_showDebugPrints = false
		);
		
		// Returns true if the given refr is within the player's activation range.
		bool RefrIsInActivationRange(RE::ObjectRefHandle a_refrHandle) const;

		// Reset all time points to the current time.
		void ResetTPs();

		// Cycle through nearby targetable refrs and choose one for activation.
		void SelectProximityRefr();

		// Update the player's crosshair text entry periodically for the given message type.
		// Used to maintain up-to-date info on the selected crosshair target
		// or stealth state while the player is sneaking.
		void SetPeriodicCrosshairMessage(const CrosshairMessageType& a_type);

		// NOTE: 
		// Only when aim correction is enabled for this player.
		// Either select a new aim correction target, clear the current invalid one,
		// or clear the current one when attacking or selecting a target with the crosshair.
		void UpdateAimCorrectionTarget();

		// Update the target refr used by the ranged attack package.
		// The given equip index should hold the form triggering the ranged attack.
		// E.g. Left hand slot when trying to cast a spell in the left hand.
		bool UpdateAimTargetLinkedRefr(const EquipIndex& a_attackSlot);

		// Update the interpolation data used to animate the crosshair's contraction/expansion.
		void UpdateAnimatedCrosshairInterpData();
		
		// Update the crosshair text entry for this player based on external and periodic requests.
		void UpdateCrosshairMessage();

		// Update the crosshair's Scaleform and world positions when the player moves their RS
		// and fade out/re-center the crosshair as needed when inactive.
		// Also update the refr selected by the crosshair.
		void UpdateCrosshairPosAndSelection();

		// Using the given raycast hit result,
		// update the 2D bounds of the current crosshair-selected object, if any,
		// and compute the new speedmult to apply the the crosshair's pixel displacement 
		// when moving the crosshair across the object.
		void UpdateCrosshairSpeedmult(const Raycast::RayResult& a_chosenResult);

		// Award Sneak XP for companion players as necessary after updating their detection state.
		void UpdateSneakState();

		// Update closest hostile actor, their distance to the player, 
		// and the player's detection percent.
		void UpdateStealthDetectionState();

		// Update motion state data for the targeted refr.
		void UpdateTargetedRefrMotionState();

		// Check if UI elements should be drawn 
		// and then update the player's crosshair position and selected refr. 
		// Then, also draw all targeting UI elements 
		// (crosshair, aim pitch indicator, and player indicator).
		void UpdateTargetingOverlay();
		
		//
		// Members
		//
		
		// The player.
		std::shared_ptr<CoopPlayer> p;
		// Pixels moved across crosshair refr target, X and Y.
		glm::vec2 crosshairOnRefrPixelXYDeltas;
		// Scaleform coordinates (x, y, z), [0, max view dimension] 
		// for the player crosshair's center.
		glm::vec3 crosshairScaleformPos;

		// The co-op player's character.
		RE::ActorPtr coopActor;
		// Current actor targeted by aim correction.
		// NOTE: 
		// The aim correction target is only set when performing a ranged attack.
		RE::ActorHandle aimCorrectionTargetHandle;
		// Closest hostile actor used for detection checks.
		RE::ActorHandle closestHostileActorHandle;
		// Current actor selected by the crosshair.
		RE::ActorHandle selectedTargetActorHandle;
		// Local positional offsets from crosshair refr's center 
		// to the crosshair raycast hit position.
		// Set while the crosshair was first moved or is being moved.
		RE::NiPoint3 crosshairLastMovementHitPosOffset;
		RE::NiPoint3 crosshairInitialMovementHitPosOffset;
		// Local positional offset relative to the crosshair refr's center 
		// and their facing direction.
		// Equal to the last hit local positions when moving the crosshair.
		// Keeps the crosshair attached to the last local hit position 
		// relative to the crosshair refr's facing direction
		// while not moving the crosshair.
		RE::NiPoint3 crosshairLocalPosOffset;
		// Crosshair-targeted world position.
		RE::NiPoint3 crosshairWorldPos;
		// Last world position at which activation was requested.
		RE::NiPoint3 lastActivationReqPos;
		// Last interactable reference either targeted by the crosshair or by proximity selection.
		RE::ObjectRefHandle activationRefrHandle;
		// The aim target linked refr used as the target for the player's ranged attack package.
		RE::ObjectRefHandle aimTargetLinkedRefrHandle;
		// Current refr picked by the game's own crosshair.
		RE::ObjectRefHandle crosshairPickRefrHandle;
		// Reference targeted by the player's crosshair.
		RE::ObjectRefHandle crosshairRefrHandle;
		// Reference targeted by the player's crosshair from the previous frame.
		RE::ObjectRefHandle prevCrosshairRefrHandle;
		// Closest interactable refr in the player's FOV.
		RE::ObjectRefHandle proximityRefrHandle;
		// Cached ordered map of nearby refrs that can be interacted with by the player.
		// Sorted in ascending order based on a normalized factor derived
		// from the refr's distance to the player 
		// and the angle the player has to turn to face the refr directly.
		std::multimap<float, RE::ObjectRefHandle> nearbyReferences;
		// Mutex for modifying the player's current crosshair message text.
		std::mutex crosshairMessageMutex;
		// Mutex for modifying selected/aim correction/aim target linked refrs.
		std::mutex targetingMutex;
		// This player's crosshair text entry to display when the full crosshair message updates.
		std::string crosshairTextEntry;
		// Selected target refr's motion info.
		std::unique_ptr<RefrTargetMotionState> targetMotionState;
		// Interpolation data for fading drawn UI elements and for crosshair size adjustments.
		std::unique_ptr<TwoWayInterpData> aimPitchIndicatorFadeInterpData;
		std::unique_ptr<TwoWayInterpData> crosshairFadeInterpData;
		std::unique_ptr<TwoWayInterpData> crosshairSizeRatioInterpData;
		std::unique_ptr<TwoWayInterpData> playerIndicatorFadeInterpData;
		// Current crosshair message entry to set.
		std::unique_ptr<CrosshairMessage> crosshairMessage;
		// Externally-prompted message entry to set (set by another thread or manager).
		std::unique_ptr<CrosshairMessage> extCrosshairMessage;
		// Last set crosshair message entry.
		std::unique_ptr<CrosshairMessage> lastCrosshairMessage;
		// Interpolation data for oscillating and rotating the crosshair prongs.
		std::unique_ptr<InterpolationData<float>> crosshairOscillationData;
		std::unique_ptr<InterpolationData<float>> crosshairRotationData;
		// Holds information on the player's managed projectiles.
		std::unique_ptr<ManagedProjectileHandler> mph;
		// Manages grabbed/released objects.
		std::unique_ptr<RefrManipulationManager> rmm;
		// For activation: Nearby objects of the same type as the requested refr.
		std::vector<RE::ObjectRefHandle> nearbyObjectsOfSameType;
		// Should this manager draw targeting overlay elements?
		bool baseCanDrawOverlayElements;
		// Can the player activate their chosen activation refr?
		bool canActivateRefr;
		// Is the crosshair refr raycast result the closest one to the camera?
		bool choseClosestResult;
		// Is the crosshair refr in range to open the QuickLoot menu?
		bool crosshairRefrInRangeForQuickLoot;
		// Is the crosshair target refr in sight of the player?
		bool crosshairRefrInSight;
		// M.A.R.F: Mutual Assured Ragdoll Flight.
		// You grab me, I grab you, into the sky we go.
		// It's not a bug, it's a feature.
		bool isMARFing;
		// Is the player trying to interact with cycled, nearby refrs?
		bool useProximityInteraction;
		// Is a valid object being targeted by the crosshair's raycast?
		bool validCrosshairRefrHit;
		// Closest hostile actor's distance from the player.
		float closestHostileActorDist;
		// Difference in pitch between the last hit local position and the crosshair refr's center.
		float crosshairLocalPosPitchDiff;
		// Difference in yaw between the last hit local position and the crosshair refr's center.
		float crosshairLocalPosYawDiff;
		// Crosshair speed multiplier.
		// Used to slow down the crosshair over selectable objects.
		float crosshairSpeedMult;
		// If in combat:
		// Player's total remaining stealth points after decrementing the max
		// stealth points associated with them from all groups in combat with the player.
		// Otherwise:
		// Average detection percent of the player for all actors in the high process.
		// (100% == fully detected, 0% == hidden (can perform sneak attacks, gain XP from evasion))
		float detectionPct;
		// Distance offset to tack on to the default suspension distance
		// from the player to each grabbed object's default position.
		float grabbedRefrDistanceOffset;
		// Last angle at which the player was facing when the activate bind was pressed.
		float lastActivationFacingAngle;
		// Maximum distance an object can be from the player's center
		// to be considered for activation.
		float maxReachActivationDist;
		// Seconds since the last stealth state check.
		float secsSinceLastStealthStateCheck;
		// Seconds since visibilty on the target was last lost.
		float secsSinceTargetVisibilityLost;
		// Seconds since last visibility check.
		float secsSinceVisibleOnScreenCheck;
		// Controller ID for this player.
		int32_t controllerID;
		// Player ID for this player.
		int32_t playerID;
		// Detection percent RGB value (RRGGBB in hex)
		uint32_t detectionPctRGB;
	};
}

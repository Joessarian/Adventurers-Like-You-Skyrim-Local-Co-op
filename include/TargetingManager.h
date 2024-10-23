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
	// All player -> environment targeting and interactions.
	struct TargetingManager : public Manager
	{
		// Stores physical movement data for a targeted actor.
		struct ActorTargetMotionState
		{
			ActorTargetMotionState() :
				targetActorHandle(),
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
				populateCount(0),
				lastUpdateTP(SteadyClock::now())
			{ }

			// Enough data points to populate all members.
			inline bool IsFullyPopulated() const noexcept { return populateCount == FRAMES_BETWEEN_AVG_DATA_UPDATES; }

			// Refresh all stored data.
			inline void Refresh()
			{
				targetActorHandle = RE::ActorHandle();
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
				populateCount = 0;
				firstUpdate = true;
				lastUpdateTP = SteadyClock::now();
			}

			// Update computed motion state for the target.
			void UpdateMotionState(RE::ActorHandle a_targetActorHandle);

			// Targeted actor.
			RE::ActorHandle targetActorHandle;

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
			// Number of frames since first update.
			// Fully populated once one average data update is performed.
			uint8_t populateCount;
			// Time point at which data was last updated.
			SteadyClock::time_point lastUpdateTP;
		};

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
			{
			}

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

			// Copy another message's data to this one without invalidating the other message.
			inline void CopyMessageData(const std::unique_ptr<CrosshairMessage>& a_other)
			{
				if (a_other && a_other.get())
				{
					type = a_other->type;
					setTP = a_other->setTP;
					text = a_other->text;
					delayedMessageTypes = a_other->delayedMessageTypes;
					secsMaxDisplayTime = a_other->secsMaxDisplayTime;
					hash = a_other->hash;
				}
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

			// Update cached data based on the given message type, text, and other message types to delay.
			inline void Update(CrosshairMessageType&& a_type, const RE::BSFixedString a_text, std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, float a_secsMaxDisplayTime = 0.0f) noexcept
			{
				type = a_type;
				text = a_text;
				delayedMessageTypes = a_delayedMessageTypes;
				secsMaxDisplayTime = a_secsMaxDisplayTime;
				hash = Hash(a_text);
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

		// Stores handle, grab time point, and other information related to grabbed references.
		struct GrabbedReferenceInfo
		{
			GrabbedReferenceInfo() :
				refrHandle(RE::ObjectRefHandle()),
				grabTP(std::nullopt)
			{ }

			GrabbedReferenceInfo(const RE::ObjectRefHandle& a_handle) :
				refrHandle(a_handle),
				grabTP(SteadyClock::now())
			{ }

			GrabbedReferenceInfo(const GrabbedReferenceInfo& a_other)
			{
				refrHandle = a_other.refrHandle;
				grabTP = a_other.grabTP;
			}

			GrabbedReferenceInfo(GrabbedReferenceInfo&& a_other)
			{
				refrHandle = std::move(a_other.refrHandle);
				grabTP = std::move(a_other.grabTP);
			}

			GrabbedReferenceInfo& operator=(const GrabbedReferenceInfo& a_other)
			{
				refrHandle = a_other.refrHandle;
				grabTP = a_other.grabTP;
				return *this;
			}

			GrabbedReferenceInfo& operator=(GrabbedReferenceInfo&& a_other)
			{
				refrHandle = std::move(a_other.refrHandle);
				grabTP = std::move(a_other.grabTP);
				return *this;
			}

			// Returns true if the grabbed refr handle is still valid.
			inline bool IsValid() const noexcept { return refrHandle && refrHandle.get() && refrHandle.get().get(); }

			// Clear out handle and grab time point.
			inline void Clear()
			{
				refrHandle = RE::ObjectRefHandle();
				grabTP = std::nullopt;
			}

			// Update velocity/positioning of the grabbed refr at the given index. 
			// Use the first grabbed refr's radius to position subsequent refrs around it.
			void UpdateGrabbedReference(const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index, const float& a_firstGrabbedReferenceRadius);

			// Handle for this grabbed refr.
			RE::ObjectRefHandle refrHandle;

			// Time point at which this refr was grabbed.
			std::optional<SteadyClock::time_point> grabTP;
		};

		// Stores targeting (release state, trajectory, target) info about a refr dropped/thrown by a player.
		struct ReleasedReferenceInfo
		{
			ReleasedReferenceInfo() :
				trajType(ProjectileTrajType::kAimDirection),
				collisionActive(true),
				isHoming(false),
				isThrown(false),
				startedHomingIn(false),
				initialTimeToTarget(0.0f),
				launchPitch(0.0f),
				launchYaw(0.0f),
				releaseSpeed(0.0),
				targetActorHandle(),
				targetedActorNode(),
				releasePos(RE::NiPoint3()),
				releaseVelocity(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3()),
				refrHandle(RE::ObjectRefHandle()),
				firstHitTP(std::nullopt),
				releaseTP(std::nullopt)
			{
				hitRefrFIDs.clear();
			}

			ReleasedReferenceInfo(const int32_t& a_controllerID, const RE::ObjectRefHandle& a_handle) :
				trajType(ProjectileTrajType::kAimDirection),
				collisionActive(true),
				isHoming(false),
				isThrown(false),
				startedHomingIn(false),
				initialTimeToTarget(0.0f),
				launchPitch(0.0f),
				launchYaw(0.0f),
				releaseSpeed(0.0),
				targetActorHandle(),
				targetedActorNode(),
				releasePos(RE::NiPoint3()),
				releaseVelocity(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3()),
				refrHandle(RE::ObjectRefHandle()),
				firstHitTP(std::nullopt),
				releaseTP(SteadyClock::now())
			{
				refrHandle = a_handle;
				hitRefrFIDs.clear();
			}

			ReleasedReferenceInfo(const ReleasedReferenceInfo& a_other) 
			{
				trajType = a_other.trajType;
				collisionActive = a_other.collisionActive;
				isHoming = a_other.isHoming;
				isThrown = a_other.isThrown;
				startedHomingIn = a_other.startedHomingIn;
				initialTimeToTarget = a_other.initialTimeToTarget;
				launchPitch = a_other.launchPitch;
				launchYaw = a_other.launchYaw;
				releaseSpeed = a_other.releaseSpeed;
				targetActorHandle = a_other.targetActorHandle;
				targetedActorNode = a_other.targetedActorNode;
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
				collisionActive = std::move(a_other.collisionActive);
				isHoming = std::move(a_other.isHoming);
				isThrown = std::move(a_other.isThrown);
				startedHomingIn = std::move(a_other.startedHomingIn);
				initialTimeToTarget = std::move(a_other.initialTimeToTarget);
				launchPitch = std::move(a_other.launchPitch);
				launchYaw = std::move(a_other.launchYaw);
				releaseSpeed = std::move(a_other.releaseSpeed);
				targetActorHandle = std::move(a_other.targetActorHandle);
				targetedActorNode = std::move(a_other.targetedActorNode);
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
				collisionActive = a_other.collisionActive;
				isHoming = a_other.isHoming;
				isThrown = a_other.isThrown;
				startedHomingIn = a_other.startedHomingIn;
				initialTimeToTarget = a_other.initialTimeToTarget;
				launchPitch = a_other.launchPitch;
				launchYaw = a_other.launchYaw;
				releaseSpeed = a_other.releaseSpeed;
				targetActorHandle = a_other.targetActorHandle;
				targetedActorNode = a_other.targetedActorNode;
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
				collisionActive = std::move(a_other.collisionActive);
				isHoming = std::move(a_other.isHoming);
				isThrown = std::move(a_other.isThrown);
				startedHomingIn = std::move(a_other.startedHomingIn);
				initialTimeToTarget = std::move(a_other.initialTimeToTarget);
				launchPitch = std::move(a_other.launchPitch);
				launchYaw = std::move(a_other.launchYaw);
				releaseSpeed = std::move(a_other.releaseSpeed);
				targetActorHandle = std::move(a_other.targetActorHandle);
				targetedActorNode = std::move(a_other.targetedActorNode);
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
				if (hitRefrFIDs.empty()) 
				{
					firstHitTP = SteadyClock::now();
				}

				hitRefrFIDs.insert(a_refr->formID);
			}

			// Clear all data for this released refr.
			inline void Clear()
			{
				trajType = ProjectileTrajType::kAimDirection;
				collisionActive = false;
				isHoming = false;
				isThrown = false;
				startedHomingIn = false;
				initialTimeToTarget = launchPitch = launchYaw = releaseSpeed = 0.0;
				targetActorHandle = RE::ActorHandle();
				targetedActorNode.reset();
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

			// Returns true if this refr handle is still valid.
			inline bool IsValid() const noexcept { return refrHandle && refrHandle.get() && refrHandle.get().get(); } 

			// Calculate the target position to throw this refr at in order to hit the player's currently targeted refr.
			RE::NiPoint3 CalculatePredInterceptPos(const std::shared_ptr<CoopPlayer>& a_p);

			// Check if this released refr should start homing in on the target if it was thrown.
			void PerformInitialHomingCheck(const std::shared_ptr<CoopPlayer>& a_p);

			// Direct this released refr at the target position.
			void SetHomingTrajectory(const std::shared_ptr<CoopPlayer>& a_p);

			// Set this released refr's initial trajectory information.
			void SetReleaseTrajectoryInfo(const std::shared_ptr<CoopPlayer>& a_p);

			// Trajectory type.
			ProjectileTrajType trajType;
			// Actor handle for the refr's target.
			RE::ActorHandle targetActorHandle;
			// Velocity set at release.
			RE::NiPoint3 releaseVelocity;
			// Position the refr is released from.
			RE::NiPoint3 releasePos;
			// Local position offset for the crosshair world position relative to the target's world position.
			RE::NiPoint3 targetLocalPosOffset;
			// Initial end position for the thrown refr's trajectory.
			RE::NiPoint3 trajectoryEndPos;
			// Actor node targeted by this thrown refr.
			RE::NiPointer<RE::NiAVObject> targetedActorNode;
			// Handle for this refr.
			RE::ObjectRefHandle refrHandle;
			// Time point indicating when this released refr first hit another object.
			std::optional<SteadyClock::time_point> firstHitTP;
			// Time point when this refr was released.
			std::optional<SteadyClock::time_point> releaseTP;
			// Set of FIDs for the refrs that this refr has hit since release.
			std::set<RE::FormID> hitRefrFIDs;
			// Is this refr being checked for collisions with other objects?
			bool collisionActive;
			// Is this refr currently homing in on the target?
			bool isHoming;
			// Was this refr thrown or just dropped?
			bool isThrown;
			// Has this refr started homing in on the target?
			bool startedHomingIn;
			// Time in seconds to reach the target at the time of the refr's release.
			double initialTimeToTarget;
			// Refr's pitch at launch ('up' is positive, 'down' is negative (opposite of game's convention)).
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
				queuedReleasedRefrContactEvents = a_other.queuedReleasedRefrContactEvents;
				grabbedRefrHandlesToInfoIndices = a_other.grabbedRefrHandlesToInfoIndices;
				releasedRefrHandlesToInfoIndices = a_other.releasedRefrHandlesToInfoIndices;
				isGrabbing = a_other.isGrabbing;

				grabbedRefrInfoList.clear();
				releasedRefrInfoList.clear();

				std::for_each(a_other.grabbedRefrInfoList.begin(), a_other.grabbedRefrInfoList.end(),
					[this](const std::unique_ptr<GrabbedReferenceInfo>& a_info) {
						if (a_info && a_info.get()) 
						{
							grabbedRefrInfoList.emplace_back(std::make_unique<GrabbedReferenceInfo>(*a_info.get()));
						}
					}
				);

				std::for_each(a_other.releasedRefrInfoList.begin(), a_other.releasedRefrInfoList.end(),
					[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info) {
						if (a_info && a_info.get())
						{
							releasedRefrInfoList.emplace_back(std::make_unique<ReleasedReferenceInfo>(*a_info.get()));
						}
					}
				);
			}

			RefrManipulationManager(RefrManipulationManager&& a_other) 
			{
				collidedRefrFIDPairs = std::move(a_other.collidedRefrFIDPairs);
				queuedReleasedRefrContactEvents = std::move(a_other.queuedReleasedRefrContactEvents);
				grabbedRefrHandlesToInfoIndices = std::move(a_other.grabbedRefrHandlesToInfoIndices);
				releasedRefrHandlesToInfoIndices = std::move(a_other.releasedRefrHandlesToInfoIndices);
				isGrabbing = std::move(a_other.isGrabbing);

				grabbedRefrInfoList.swap(a_other.grabbedRefrInfoList);
				releasedRefrInfoList.swap(a_other.releasedRefrInfoList);
			}

			RefrManipulationManager& operator=(const RefrManipulationManager& a_other) 
			{
				collidedRefrFIDPairs = a_other.collidedRefrFIDPairs;
				queuedReleasedRefrContactEvents = a_other.queuedReleasedRefrContactEvents;
				grabbedRefrHandlesToInfoIndices = a_other.grabbedRefrHandlesToInfoIndices;
				releasedRefrHandlesToInfoIndices = a_other.releasedRefrHandlesToInfoIndices;
				isGrabbing = a_other.isGrabbing;

				grabbedRefrInfoList.clear();
				releasedRefrInfoList.clear();

				std::for_each(a_other.grabbedRefrInfoList.begin(), a_other.grabbedRefrInfoList.end(),
					[this](const std::unique_ptr<GrabbedReferenceInfo>& a_info) {
						if (a_info && a_info.get())
						{
							grabbedRefrInfoList.emplace_back(std::make_unique<GrabbedReferenceInfo>(*a_info.get()));
						}
					}
				);

				std::for_each(a_other.releasedRefrInfoList.begin(), a_other.releasedRefrInfoList.end(),
					[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info) {
						if (a_info && a_info.get())
						{
							releasedRefrInfoList.emplace_back(std::make_unique<ReleasedReferenceInfo>(*a_info.get()));
						}
					}
				);

				return *this;
			}

			RefrManipulationManager& operator=(RefrManipulationManager&& a_other) 
			{
				collidedRefrFIDPairs = std::move(a_other.collidedRefrFIDPairs);
				queuedReleasedRefrContactEvents = std::move(a_other.queuedReleasedRefrContactEvents);
				grabbedRefrHandlesToInfoIndices = std::move(a_other.grabbedRefrHandlesToInfoIndices);
				releasedRefrHandlesToInfoIndices = std::move(a_other.releasedRefrHandlesToInfoIndices);
				isGrabbing = std::move(a_other.isGrabbing);

				grabbedRefrInfoList.swap(a_other.grabbedRefrInfoList);
				releasedRefrInfoList.swap(a_other.releasedRefrInfoList);
				return *this;
			}
			
			// Get the current number of grabbed refrs handled by this manager.
			inline const uint8_t GetNumGrabbedRefrs() const noexcept { return grabbedRefrInfoList.size(); }

			// Get the current number of released refrs handled by this manager.
			inline const uint8_t GetNumReleasedRefrs() const noexcept { return releasedRefrInfoList.size(); }

			// Is the given refr designated as grabbed?
			inline const bool IsGrabbed(const RE::ObjectRefHandle& a_handle) { return IsManaged(a_handle, true); }

			// Is the given refr designated as released?
			inline const bool IsReleased(const RE::ObjectRefHandle& a_handle) { return IsManaged(a_handle, false); }

			// Add the given refr to the grabbed list if the maximum number of grabbed refrs has not been reached.
			void AddGrabbedRefr(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle);

			// Add the given refr to the released list and set its release trajectory.
			void AddReleasedRefr(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle);

			// Returns true if there is room for another grabbed refr.
			bool CanGrabAnotherRefr();
			
			// Returns true if the given refr is valid, not already managed, 
			// and the maximum number of grabbed refr has not been reached.
			bool CanGrabRefr(const RE::ObjectRefHandle& a_handle);

			// Returns true if this manager can manipulate the grabbed refr at the given index.
			bool CanManipulateGrabbedRefr(const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index);

			// Clear all grabbed/released refr data and queued contact events.
			// Manager is no longer set as grabbing objects or auto-grabbing. 
			void ClearAll();

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

			// Remove the given refr from the grabbed or released refrs list
			// and refresh handle-to-index mappings upon successful erasure.
			void ClearRefr(const RE::ObjectRefHandle& a_handle);

			// Clear given released refr, if managed.
			void ClearReleasedRefr(const RE::ObjectRefHandle& a_handle);

			// Clear all managed released refrs.
			void ClearReleasedRefrs() noexcept;

			// For each queued contact event, get both colliding objects 
			// and check if the thrown object should damage the hit object
			// and if the thrown object itself should receive damage.
			// Handle the collision and apply damage as needed.
			void HandleQueuedContactEvents(const std::shared_ptr<CoopPlayer>& a_p);

			// Is the given refr handled either as a grabbed refr or a released refr?
			const bool IsManaged(const RE::ObjectRefHandle& a_handle, bool a_grabbed);

			// Refresh the refr handle-to-list index mappings for either all managed grabbed or released refrs.
			void RefreshHandleToIndexMappings(const bool& a_grabbed);

			// List of havok contact events between released refrs and other objects.
			// Enqueued by a havok contact listener, removed and handled by this manager.
			std::list<RE::hkpContactPointEvent> queuedReleasedRefrContactEvents;

			// Mutex for queueing/handling havok contact events.
			std::mutex contactEventsQueueMutex;

			// Maps grabbed refr handles to their positions in the managed grabbed refrs list.
			std::unordered_map<RE::ObjectRefHandle, uint8_t, std::hash<RE::ObjectRefHandle>> grabbedRefrHandlesToInfoIndices;

			// Maps released refr handles to their positions in the managed released refrs list.
			std::unordered_map<RE::ObjectRefHandle, uint8_t, std::hash<RE::ObjectRefHandle>> releasedRefrHandlesToInfoIndices;

			// Set of pairs of FIDs for the two objects that collide during a havok contact event.
			// One of the FIDs is always a released refr.
			std::unordered_set<std::pair<RE::FormID, RE::FormID>, std::hash<std::pair<RE::FormID, RE::FormID>>> collidedRefrFIDPairs;

			// List of grabbed refr info for each managed refr.
			std::vector<std::unique_ptr<GrabbedReferenceInfo>> grabbedRefrInfoList;

			// List of released refr info for each managed refr.
			std::vector<std::unique_ptr<ReleasedReferenceInfo>> releasedRefrInfoList;

			// Is the manager handling auto-grabbed nearby clutter?
			bool isAutoGrabbing;

			// Should the manager handling grabbed refrs? If false, all grabbed refrs have been released.
			bool isGrabbing;
		};

		// NOTE: Much of the trajectory stuff is 'good enough for now'
		// and will receive more attention in the future
		// when I'm not completely spent.
		// 
		// Holds projectile trajectory information at release.
		struct ManagedProjTrajectoryInfo
		{
			ManagedProjTrajectoryInfo() :
				trajType(ProjectileTrajType::kAimDirection),
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
				targetActorHandle(),
				targetedActorNode(),
				releasePos(RE::NiPoint3()),
				targetLocalPosOffset(RE::NiPoint3()),
				trajectoryEndPos(RE::NiPoint3())
			{ }

			// Set trajectory on construction.
			ManagedProjTrajectoryInfo(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, RE::NiPoint3& a_initialVelocityOut, const ProjectileTrajType& a_trajType)
			{
				SetTrajectory(a_p, a_projectileHandle, a_initialVelocityOut, a_trajType);
			}

			// Refresh all data.
			inline void Refresh()
			{
				trajType = ProjectileTrajType::kAimDirection;
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

				targetActorHandle = RE::ActorHandle();
				targetedActorNode.reset();

				releasePos = 
				targetLocalPosOffset =
				trajectoryEndPos = RE::NiPoint3();
			}

			// Calculate the position at which the projectile should be directed to hit the current target position.
			// Currently unused: 
			// Can optionally adjust the projectile's release speed to account for air resistance and hit the target position.
			RE::NiPoint3 CalculatePredInterceptPos(const std::shared_ptr<CoopPlayer>& a_p, const bool& a_adjustReleaseSpeed);

			// Accounts for linear air drag.
			// Get the release speed to set for this projectile given the target position
			// expressed as a point in a 2D plane:
			// The plane can be either of the two that lie parallel to the line that connects the release position and the target position,
			// and have a normal axis that is perpendicular to the game's Z axis.
			// The XY coordinate can be thought of as the traditional 'x' coordinate in the Cartesian plane,
			// and the Z coordinate can likewise be thought of as the 'y' coordinate.
			double GetReleaseSpeedToTarget(const double& a_xy, const double& a_z, const double& a_launchPitch);

			// Set the physical projectile (non-magical) flag, initial release speed, max release speed, release position, 
			// and physical constants for the trajectory.
			// The given release speed is set as a fallback default.
			void SetInitialBaseProjectileData(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, const float& a_releaseSpeed);

			// Sets up the initial trajectory data for the given projectile
			// based on the given starting velocity (which is modified) and the trajectory type.			
			void SetTrajectory(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, RE::NiPoint3& a_initialVelocityOut, const ProjectileTrajType& a_trajType);

			// Get a rough approximation of the minimum launch pitch ('+' when aiming up, '-' when aiming down)
			// to hit the target position.
			float GetRoughMinLaunchPitch(const std::shared_ptr<CoopPlayer>& a_p);

			// The trajectory type to use for this projectile.
			ProjectileTrajType trajType;
			// Targeted actor's handle.
			RE::ActorHandle targetActorHandle;
			// Position at which the projectile is released.
			RE::NiPoint3 releasePos;
			// Local position offset for the crosshair world position relative to the target's world position.
			RE::NiPoint3 targetLocalPosOffset;
			// Initial end position for the projectile's trajectory.
			RE::NiPoint3 trajectoryEndPos;
			// Actor node targeted by this projectile.
			RE::NiPointer<RE::NiAVObject> targetedActorNode;
			// If it exists, time point when the projectile started homing in on the target.
			// Otherwise, the projectile has not started homing yet.
			std::optional<SteadyClock::time_point> startedHomingTP;
			// True if an arrow/bolt (physical) projectile, false if a magic projectile.
			bool isPhysicalProj;
			// Has the projectile started homing in on the target position yet?
			bool startedHomingIn;
			// Gravitational constant to use when guiding the projectile along the trajectory.
			double g;
			// Drag coefficient to use when guiding the projectile along the trajectory.
			double mu;
			// Projectile's pitch at launch ('up' is positive, 'down' is negative (opposite of game's convention)).
			double launchPitch;
			// Projectile's yaw at launch (Cartesian convention).
			double launchYaw;
			// Maximum speed this projectile can be released at.
			double maxReleaseSpeed;
			// Multiplier from the base projectile's data to multiply the projectile's gravitiation constant by.
			double projGravFactor;
			// Speed at which to release the projectile.
			double releaseSpeed;
			// Time to reach the target (in seconds) based on the projectile's initial trajectory set on release.
			double initialTrajTimeToTarget;
		};

		// Handles managed projectiles fired by the player.
		struct ManagedProjectileHandler
		{
			ManagedProjectileHandler()
			{
				Clear();
			}

			// Clear out managed projectile FIDs and trajectory data.
			inline void Clear()
			{
				managedProjHandles.clear();
				managedProjHandleToTrajInfoMap.clear();
			}

			// Precondition: the projectile must be managed already.
			// Check if managed first before calling.
			inline std::unique_ptr<ManagedProjTrajectoryInfo>& GetInfo(const RE::ObjectRefHandle& a_projectileHandle)
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

				if (managedProjHandleToTrajInfoMap.contains(a_projectileHandle)) 
				{
					managedProjHandles.remove(a_projectileHandle);
					managedProjHandleToTrajInfoMap.erase(a_projectileHandle);
				}
			}

			// Insert the given projectile to the handled list if there is room.
			// Clear out inactive/invalid projectiles to make room as necessary.
			// Set the projectile's trajectory info using the given initial velocity (which is modified),
			// and the given trajectory type.
			void Insert(const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_projectileHandle, RE::NiPoint3& a_initialVelocityOut, const ProjectileTrajType& a_trajType);

			// Newly managed projectiles' handles are added to the back and popped from the front.
			std::list<RE::ObjectRefHandle> managedProjHandles;
			// Holds trajectory info for managed projectiles, indexed by their handles.
			std::unordered_map<RE::ObjectRefHandle, std::unique_ptr<ManagedProjTrajectoryInfo>> managedProjHandleToTrajInfoMap;
		};

		TargetingManager();
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

		// Clear out currently-targeted proximity refr for activation.
		inline void ClearProximityRefr() { proximityRefrHandle.reset(); }

		// Clear out all targeted actor/refr handles.
		inline void ClearTargetHandles() 
		{
			// Target actors.
			aimCorrectionTargetHandle = closestHostileActorHandle = selectedTargetActorHandle = RE::ActorHandle();
			// Interaction/target refrs.
			activationRefrHandle = aimTargetLinkedRefrHandle = crosshairPickRefrHandle = crosshairRefrHandle = prevCrosshairRefrHandle = proximityRefrHandle = RE::ObjectRefHandle();
		}

		// Get the farthest distance an object can be located from the player while activating an object.
		inline float GetMaxActivationDist()
		{
			// Can activate objects that are further away when mounted.
			return (coopActor->IsOnMount()) ? Settings::fMountedActivationReachMult * maxReachActivationDist : maxReachActivationDist;
		}

		// Reset the player's crosshair scaleform position to its default position straight away.
		inline void ResetCrosshairPosition()
		{
			// Offset left or right about the center of the screen based on player index.
			crosshairScaleformPos.x = DebugAPI::screenResX / 2.0f + 100.0f * (fmod(playerID, 2) * 2 - 1) * ceil((playerID + 1) / 2.0f);
			crosshairScaleformPos.y = DebugAPI::screenResY / 2.0f;
			crosshairScaleformPos.z = 0.0f;
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
		inline void SetCrosshairMessageRequest(CrosshairMessageType&& a_type, const RE::BSFixedString a_text, std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, float a_secsMaxDisplayTime = 0.0f)
		{
			SetCurrentCrosshairMessage(true, std::move(a_type), a_text, std::move(a_delayedMessageTypes), a_secsMaxDisplayTime);
		}

		// Update the external/current crosshair message request with the given type, message text,
		// list of other message types to delay, and max display time.
		// Only update if another message is not displayed,
		// the given message type is not delayed, or if the delay has passed.
		inline void SetCurrentCrosshairMessage(bool&& a_extRequest, CrosshairMessageType&& a_type, const RE::BSFixedString a_text, std::set<CrosshairMessageType>&& a_delayedMessageTypes = {}, float a_secsMaxDisplayTime = 0.0f)
		{
			const bool noDelay = lastCrosshairMessage->delayedMessageTypes.empty();
			const bool isDelayedType = !noDelay && lastCrosshairMessage->delayedMessageTypes.contains(a_type);
			const bool delayPassed = Util::GetElapsedSeconds(lastCrosshairMessage->setTP) > lastCrosshairMessage->secsMaxDisplayTime;

			if (noDelay || !isDelayedType || delayPassed)
			{
				if (a_extRequest) 
				{
					extCrosshairMessage->Update(std::move(a_type), a_text, std::move(a_delayedMessageTypes), std::move(a_secsMaxDisplayTime));
				}
				else
				{
					crosshairMessage->Update(std::move(a_type), a_text, std::move(a_delayedMessageTypes), std::move(a_secsMaxDisplayTime));
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
			lastActivationReqPos = Util::Get3DCenterPos(coopActor.get());
			lastActivationFacingAngle = coopActor->GetHeading(false);
		}

		//
		// Member funcs
		//

		// Clear the cached actor/refr handle for the given target type.
		void ClearTarget(const TargetActorType& a_targetType);

		// Draw the aim pitch indicator arrow for this player
		// if they are adjusting their aim pitch manually or if aim pitch was reset.
		void DrawAimPitchIndicator();

		// Draw this player's active crosshair (lines and outlines) if it hasn't fully faded out.
		void DrawCrosshair();

		// Draw the four main body 'lines'/'prongs' of the retro-style crosshair.
		void DrawCrosshairLines();

		// Draw an outline, with a thickness multiplier given by the outline index
		// and colored with the given RGBA value, around the four prongs of the retro-style crosshair.
		void DrawCrosshairOutline(float&& a_outlineIndex, const uint32_t& a_outlineRGBA);

		// Draw a crosshair similar in style to the Skyrim's default crosshair.
		void DrawSkyrimStyleCrosshair();

		// Draw a colored 'quest marker' indicator above the player's head, 
		// dependent on the player's indicator visibility setting.
		void DrawPlayerIndicator();

		// Check if UI elements should be drawn and then update the player's crosshair position and selected refr. 
		// Then, also draw all targeting UI elements (crosshair, aim pitch indicator, and player indicator).
		void DrawTargetingOverlay();
		
		// Get the closest targetable actor to the source refr using the given FOV in radians centered at their heading angle,
		// and the given maximum range (XY or XYZ distance) to consider targets.
		// If combat dependent selection is requested, only target hostile actors unless attempting to heal a target.
		RE::ActorHandle GetClosestTargetableActorInFOV(RE::ObjectRefHandle a_sourceRefrHandle, const float& a_fovRads, const bool& a_useXYDistance = false, const float& a_range = -1.0f, const bool& a_combatDependentSelection = true);
		
		// Get detection-level-modified gradient RGB value.
		// The raw detection level ranges from -1000 to 1000,
		// whereas the modified detection level ranges from 0 to 100.
		uint32_t GetDetectionLvlRGB(const float& a_detectionLvl, bool&& a_fromRawLevel);

		// Get actor level-difference-modified RGB value
		// which represents the difference in level between the player and the given actor.
		uint32_t GetLevelDifferenceRGB(const RE::ActorHandle& a_actorHandle);

		// Get a list of all nearby refrs that are the same type as the given refr.
		// Can compare based on having the exact same base form, or on having the same base form type.
		// E.g. Consider two book refrs: "Lusty Argonian Maid" and "Cats of Skyrim"
		// Same base form (Not the same book) => 'false',
		// Same form type (TESObjectBOOK) => 'true'
		const std::vector<RE::ObjectRefHandle>& GetNearbyRefrsOfSameType(RE::ObjectRefHandle a_refrHandle, RefrCompType&& a_compType = RefrCompType::kSameBaseForm);
		
		// If cycling nearby objects, cycle one time and return the resulting refr's handle.
		// Otherwise, return the selected crosshair refr's handle.
		RE::ObjectRefHandle GetNextObjectToActivate();

		// Get the ranged target to set for the player's ranged attack package
		// based on the given attack source form.
		RE::ObjectRefHandle GetRangedPackageTargetRefr(RE::TESForm* a_rangedAttackSource);

		// Get the player's current ranged target actor, 
		// which is the crosshair refr if an actor is selected, 
		// the aim correction target if the option is enabled while not facing the crosshair,
		// or the ranged package's aim target linked refr if it is not the player.
		RE::ActorHandle GetRangedTargetActor();

		// Sounds like a lot of- hoopla! Sounds like a lot of- hoopla! Sounds like a lot of- hoopla! Hoooooplaaaa! *Bonk*
		void HandleBonk(RE::ActorHandle a_hitActorHandle, RE::ObjectRefHandle a_releasedRefrHandle, RE::hkpRigidBody* a_releasedRefrRigidBody, const RE::NiPoint3& a_contactPos);

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
		// or another player unless player selection is enabled.
		// - Player has LOS or has not lost LOS for too long.
		bool IsRefrValidForCrosshairSelection(RE::ObjectRefHandle a_refrHandle);

		// Choose a raycast hit result to use as the crosshair's selected refr.
		// The chosen result is influenced by whether or not the player is in combat
		// and whether or not their crosshair is active for target selection.
		// The crosshair is considered active for selection when moving or when over a target.
		Raycast::RayResult PickRaycastHitResult(const std::vector<Raycast::RayResult>& a_raycastResults, const bool& a_inCombat, const bool&& a_crosshairActiveForSelection);
		
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

		// Returns true if the cached nearby refrs used previously when selecting
		// a proximity refr should be refreshed when the player's last activation
		// position is too far away from their current position, or when they
		// have turned away from their previous activation facing angle.
		bool ShouldRefreshNearbyReferences();

		// NOTE: Only when aim correction is enabled for this player.
		// Either select a new aim correction target, clear the current invalid one,
		// or clear the current one when attacking or selecting a target with the crosshair.
		void UpdateAimCorrectionTarget();

		// Update the target refr used by the ranged attack package.
		// The given equip slot should holds the form triggering the attack.
		// E.g. Left hand slot holding a spell.
		bool UpdateAimTargetLinkedRefr(const EquipIndex& a_attackSlot);

		// Update the interpolation data used to animate the crosshair's contraction/expansion.
		void UpdateAnimatedCrosshairInterpData();

		// Update the crosshair Scaleform and world positions when the player moves their RS
		// and fade out/re-center the crosshair as needed when inactive.
		// Also update the refr selected by the crosshair.
		void UpdateCrosshairPosAndSelection();

		// Update the crosshair text entry for this player based on external and periodic requests.
		void UpdateCrosshairMessage();

		// Award Sneak XP for companion players as necessary after updating their detection state.
		void UpdateSneakState();

		// Update closest hostile actor, their distance to the player, and detection percent.
		void UpdateStealthDetectionState();

		//
		// Members
		//
		
		// The player.
		std::shared_ptr<CoopPlayer> p;
		// Pixels moved across crosshair refr target, X and Y.
		glm::vec2 crosshairOnRefrPixelXYDeltas;
		// Scaleform coordinates (x, y, z), [0, max view dimension] for the player crosshair's center.
		glm::vec3 crosshairScaleformPos;

		// The co-op player.
		RE::ActorPtr coopActor;
		// Current actor targeted by aim correction.
		// NOTE: The aim correction target is only set when performing a ranged attack.
		RE::ActorHandle aimCorrectionTargetHandle;
		// Closest hostile actor used for detection checks.
		RE::ActorHandle closestHostileActorHandle;
		// Current actor selected by the crosshair.
		RE::ActorHandle selectedTargetActorHandle;
		// Local positional offset from crosshair refr's center to crosshair world position.
		RE::NiPoint3 crosshairLastHitLocalPos;
		// Crosshair-targeted world position.
		RE::NiPoint3 crosshairWorldPos;
		// Last position at which activation was requested.
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
		// Sorted in ascending order based on the angle the player has to turn to face the refr directly.
		std::multimap<float, RE::ObjectRefHandle> nearbyReferences;
		// Mutex for modifying selected/aim correction/aim target linked refrs.
		std::mutex targetingMutex;
		// Local positional offset from crosshair refr's center to crosshair hit pos.
		// Set when the crosshair refr is first targeted.
		// If invalid, no crosshair valid refr is selected.
		std::optional<RE::NiPoint3> crosshairInitialHitLocalPos;
		// This player's crosshair text entry to display when the full crosshair message updates.
		std::string crosshairTextEntry;
		// Selected target actor's motion info.
		std::unique_ptr<ActorTargetMotionState> targetMotionState;
		// Interpolation data for fading drawn UI elements.
		std::unique_ptr<TwoWayInterpData> aimPitchIndicatorFadeInterpData;
		std::unique_ptr<TwoWayInterpData> crosshairFadeInterpData;
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
		std::unique_ptr<ManagedProjectileHandler> managedProjHandler;
		// Manages grabbed/released objects.
		std::unique_ptr<RefrManipulationManager> rmm;
		// For activation: nearby objects of the same type as the requested refr.
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
		// Is the player trying to interact with cycled nearby refrs?
		bool useProximityInteraction;
		// Is a valid object being targeted by the crosshair's raycast?
		bool validCrosshairRefrHit;
		// Closest hostile actor's distance from the player.
		float closestHostileActorDist;
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

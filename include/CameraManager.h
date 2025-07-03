#pragma once
#include <Controller.h>
#include <Enums.h>
#include <Player.h>
#include <Util.h>

namespace ALYSLC
{
	using SteadyClock = std::chrono::steady_clock;
	// NOTE: 
	// Unused for now.
	// Can use to trace out camera origin point path for movement pitch calculations if necessary.
	struct CamCubicBezier3DData
	{
		using CurrentControlPoints = std::array<RE::NiPoint3, 4>;
		using NextControlPoints = std::array<RE::NiPoint3, 3>;
		CamCubicBezier3DData()
		{
			currentPoints.fill(RE::NiPoint3());
			nextPoints.fill(RE::NiPoint3());
			lastCalcCurvePoint = RE::NiPoint3();
			lastCalcDirection = RE::NiPoint3();
			frameCountSinceShift = 0;
			nextUpdateIndex = 0;
		}

		CamCubicBezier3DData(const RE::NiPoint3& a_initialPoint)
		{
			currentPoints.fill(a_initialPoint);
			nextPoints.fill(a_initialPoint);
			lastCalcCurvePoint = RE::NiPoint3();
			lastCalcDirection = RE::NiPoint3();
			frameCountSinceShift = 0;
			nextUpdateIndex = 0;
		}

		CamCubicBezier3DData
		(
			CurrentControlPoints a_currentPoints, NextControlPoints a_nextPoints
		) :
			currentPoints(a_currentPoints), nextPoints(a_nextPoints),
			lastCalcCurvePoint(RE::NiPoint3()),
			lastCalcDirection(RE::NiPoint3()),
			frameCountSinceShift(0),
			nextUpdateIndex(0)
		{ }


		CamCubicBezier3DData(const CamCubicBezier3DData& a_cb3Dd) = delete;
		CamCubicBezier3DData(CamCubicBezier3DData&& a_cb3Dd) = delete;

		CamCubicBezier3DData& operator=(const CamCubicBezier3DData& a_cb3Dd)
		{
			currentPoints = a_cb3Dd.currentPoints;
			nextPoints = a_cb3Dd.nextPoints;
			lastCalcCurvePoint = a_cb3Dd.lastCalcCurvePoint;
			lastCalcDirection = a_cb3Dd.lastCalcDirection;
			frameCountSinceShift = a_cb3Dd.frameCountSinceShift;
			nextUpdateIndex = a_cb3Dd.nextUpdateIndex;

			return *this;
		}

		CamCubicBezier3DData& operator=(CamCubicBezier3DData&& a_cb3Dd)
		{
			currentPoints = std::move(a_cb3Dd.currentPoints);
			nextPoints = std::move(a_cb3Dd.nextPoints);
			lastCalcCurvePoint = std::move(a_cb3Dd.lastCalcCurvePoint);
			lastCalcDirection = std::move(a_cb3Dd.lastCalcDirection);
			frameCountSinceShift = std::move(a_cb3Dd.frameCountSinceShift);
			nextUpdateIndex = std::move(a_cb3Dd.nextUpdateIndex);

			return *this;
		}

		inline RE::NiPoint3 GetCurrentDirection()
		{
			return lastCalcDirection;
		}

		// Credits to Freya Holmér for the crystal clear visualization
		// of Bezier curves:
		// https://www.youtube.com/watch?v=aVwxzDHniEw&t=740s
		// Polynomial forms from:
		// https://en.wikipedia.org/wiki/B%C3%A9zier_curve#Cubic_B%C3%A9zier_curves
		inline RE::NiPoint3 GetCurvePoint(const float& a_t)
		{
			auto t2 = a_t * a_t;
			auto t3 = t2 * a_t;
			auto omt = 1.0f - a_t;
			auto omt2 = omt * omt;
			auto omt3 = omt2 * omt;
			lastCalcCurvePoint = 
			(
				currentPoints[0] * omt3 +
				currentPoints[1] * 3.0f * omt2 * a_t +
				currentPoints[2] * 3.0f * omt * t2 +
				currentPoints[3] * t3
			);
			lastCalcDirection = 
			(
				(currentPoints[1] - currentPoints[0]) * 3.0f * omt2 +
				(currentPoints[2] - currentPoints[1]) * 6.0f * omt * a_t +
				(currentPoints[3] - currentPoints[2]) * 3.0f * t2
			);
			return lastCalcCurvePoint;
		}

		// Reset all data to defaults.
		inline void ResetData()
		{
			currentPoints.fill(RE::NiPoint3());
			nextPoints.fill(RE::NiPoint3());
			lastCalcCurvePoint = RE::NiPoint3();
			frameCountSinceShift = 0;
			nextUpdateIndex = 0;
		}

		// Update control points (not including the first point) in the next control point list.
		// Split the update interval into fourths and only set the control point at
		// a particular index if it has not been set before.
		inline void SetNextControlPoint(const RE::NiPoint3& a_nextControlPoint)
		{
			if (nextUpdateIndex >= 0 && nextUpdateIndex < nextPoints.size())
			{
				nextPoints[nextUpdateIndex] = a_nextControlPoint;
			}
		}

		// Copy the next list of control points into the current
		// control point list. These points will be used to define
		// the current Bezier curve. Lastly, connect the leading
		// control point in the new list to the trailing control point
		// in the old list to maintain continuity.
		inline void ShiftControlPoints()
		{
			currentPoints[0] = lastCalcCurvePoint;
			currentPoints[1] = nextPoints[0];
			currentPoints[2] = nextPoints[1];
			currentPoints[3] = nextPoints[2];
			float distStartToEnd = (currentPoints[3] - currentPoints[0]).Length();
			// Shift control points 1 and 2 so that all points are equidistant from each other.
			float newDistToCtrl1 = distStartToEnd * (1.0f / 3.0f);
			float newDistToCtrl2 = distStartToEnd * (2.0f / 3.0f);
			auto dirStartToCtrl = currentPoints[1] - currentPoints[0];
			dirStartToCtrl.Unitize();
			currentPoints[1] = currentPoints[0] + dirStartToCtrl * newDistToCtrl1;
			dirStartToCtrl = currentPoints[2] - currentPoints[0];
			dirStartToCtrl.Unitize();
			currentPoints[2] = currentPoints[0] + dirStartToCtrl * newDistToCtrl2;
			nextUpdateIndex = 0;
		}

		// Updates next control point list and shifts the next control points list
		// into the current list, as needed, before returning the point on the current
		// curve corresponding to the given time ratio.
		inline RE::NiPoint3 UpdateCurve
		(
			CamMaxUpdateFrameCount a_maxUpdateFrameCount, const RE::NiPoint3& a_nextControlPoint
		)
		{
			// Initialize camera path to start and end at the first-passed point.
			if (lastCalcCurvePoint.Length() == 0.0f) 
			{
				currentPoints.fill(a_nextControlPoint);
				nextPoints.fill(a_nextControlPoint);
				lastCalcCurvePoint = a_nextControlPoint;
				lastCalcDirection = RE::NiPoint3();
				frameCountSinceShift = 0;
				nextUpdateIndex = 0;

				return a_nextControlPoint;
			}

			frameCountSinceShift = (++frameCountSinceShift % (!a_maxUpdateFrameCount)) + 1;
			nextUpdateIndex = min
			(
				nextPoints.size() - 1, 
				(uint8_t)floorf
				(
					(float)(frameCountSinceShift - 1) / 
					(!a_maxUpdateFrameCount / (float)nextPoints.size())
				)
			);
			float tRatio = min
			(
				(float)frameCountSinceShift / (float)(!a_maxUpdateFrameCount), 
				1.0f
			);

			SetNextControlPoint(a_nextControlPoint);
			auto point = GetCurvePoint(tRatio);
			// An update interval has elapsed, so shift the control points lists.
			if (frameCountSinceShift == !a_maxUpdateFrameCount)
			{
				ShiftControlPoints();
			}

			return point;
		}

		// Set of four control points that define the
		// current 3D Bezier curve.
		// After each interpolation interval has passed,
		// the next control points list is copied over to the
		// current control points list.
		CurrentControlPoints currentPoints;
		// Set of four control points that define the
		// next 3D Bezier curve.
		// The first control point is always equal to the last
		// control point from the current control points list.
		// This list is populated as the current Bezier curve is being
		// constructed.
		NextControlPoints nextPoints;
		// Most recently returned point.
		RE::NiPoint3 lastCalcCurvePoint;
		// Most recently calculated direction along curve.
		RE::NiPoint3 lastCalcDirection;
		// Frame count since last curve shift.
		uint8_t frameCountSinceShift;
		// Next index in the next control points list to update.
		uint8_t nextUpdateIndex;
	};

	struct ObjectFadeData
	{
		ObjectFadeData() :
			objectPtr(RE::NiPointer<RE::NiAVObject>()),
			fadeStateChangeTP(SteadyClock::now()),
			shouldFadeOut(false),
			currentFadeAmount(1.0f),
			fadeAmountAtStateChange(1.0f),
			hitToCamDist(0.0f),
			interpIntervalProportion(1.0f),
			targetFadeAmount(1.0f),
			fadeIndex(-1)
		{ }

		ObjectFadeData
		(
			RE::NiAVObject* a_object, 
			int32_t a_fadeIndex, 
			float a_hitToCamDist,
			bool a_shouldFadeOut
		) :
			objectPtr(a_object),
			fadeStateChangeTP(SteadyClock::now()),
			shouldFadeOut(a_shouldFadeOut),
			currentFadeAmount(1.0f),
			fadeAmountAtStateChange(1.0f),
			hitToCamDist(a_hitToCamDist),
			interpIntervalProportion(1.0f),
			targetFadeAmount(1.0f),
			fadeIndex(a_fadeIndex)
		{ 
			// Begin fading in/out on construction.
			SignalFadeStateChange(a_shouldFadeOut, fadeIndex);
		}

		ObjectFadeData(const ObjectFadeData& a_other) :
			objectPtr(a_other.objectPtr),
			fadeStateChangeTP(a_other.fadeStateChangeTP),
			shouldFadeOut(a_other.shouldFadeOut),
			currentFadeAmount(a_other.currentFadeAmount),
			fadeAmountAtStateChange(a_other.fadeAmountAtStateChange),
			hitToCamDist(a_other.hitToCamDist),
			interpIntervalProportion(a_other.interpIntervalProportion),
			targetFadeAmount(a_other.targetFadeAmount),
			fadeIndex(a_other.fadeIndex)
		{ }

		ObjectFadeData(ObjectFadeData&& a_other) :
			objectPtr(std::move(a_other.objectPtr)),
			fadeStateChangeTP(std::move(a_other.fadeStateChangeTP)),
			shouldFadeOut(std::move(a_other.shouldFadeOut)),
			currentFadeAmount(std::move(a_other.currentFadeAmount)),
			fadeAmountAtStateChange(std::move(a_other.fadeAmountAtStateChange)),
			hitToCamDist(std::move(a_other.hitToCamDist)),
			interpIntervalProportion(std::move(a_other.interpIntervalProportion)),
			targetFadeAmount(std::move(a_other.targetFadeAmount)),
			fadeIndex(std::move(a_other.fadeIndex))
		{ }

		ObjectFadeData& operator=(const ObjectFadeData& a_other)
		{
			objectPtr = a_other.objectPtr;
			fadeStateChangeTP = a_other.fadeStateChangeTP;
			shouldFadeOut = a_other.shouldFadeOut;
			currentFadeAmount = a_other.currentFadeAmount;
			fadeAmountAtStateChange = a_other.fadeAmountAtStateChange;
			hitToCamDist = a_other.hitToCamDist;
			interpIntervalProportion = a_other.interpIntervalProportion;
			targetFadeAmount = a_other.targetFadeAmount;
			fadeIndex = a_other.fadeIndex;

			return *this;
		}

		ObjectFadeData& operator=(ObjectFadeData&& a_other)
		{
			objectPtr = std::move(a_other.objectPtr);
			fadeStateChangeTP = std::move(a_other.fadeStateChangeTP);
			shouldFadeOut = std::move(a_other.shouldFadeOut);
			currentFadeAmount = std::move(a_other.currentFadeAmount);
			fadeAmountAtStateChange = std::move(a_other.fadeAmountAtStateChange);
			hitToCamDist = std::move(a_other.hitToCamDist);
			interpIntervalProportion = std::move(a_other.interpIntervalProportion);
			targetFadeAmount = std::move(a_other.targetFadeAmount);
			fadeIndex = std::move(a_other.fadeIndex);

			return *this;
		}

		// Reset fade to 1.0 right away if the object is still valid.
		inline void InstantlyResetFade()
		{
			shouldFadeOut = false;
			fadeStateChangeTP = SteadyClock::now();
			currentFadeAmount = 
			fadeAmountAtStateChange = 
			interpIntervalProportion = 
			targetFadeAmount = 1.0f;
			fadeIndex = -1;
			if (objectPtr && objectPtr->GetRefCount() > 0)
			{
				if (auto fadeNode = objectPtr->AsFadeNode(); fadeNode)
				{
					fadeNode->currentFade = 1.0;
				}

				objectPtr->fadeAmount = 1.0f;
			}
		}

		// Start fading the object out/in. 
		// Amount depends on its fade index 
		// (increases as the index does, meaning the object is further from the camera).
		inline void SignalFadeStateChange
		(
			const bool& a_shouldFadeOut, const int32_t& a_objectFadeIndex
		) 
		{
			if (objectPtr && objectPtr->GetRefCount() > 0)
			{
				shouldFadeOut = a_shouldFadeOut;
				fadeStateChangeTP = SteadyClock::now();
				fadeAmountAtStateChange = objectPtr->fadeAmount;
				if (fadeIndex < a_objectFadeIndex) 
				{
					fadeIndex = a_objectFadeIndex;
				}

				float lowerBoundFadeAmount = max(0.2f, 0.75f / pow(1.2f, fadeIndex));
				// Fade for larger objects such as trees and interior structures.
				if (Settings::bFadeLargerObstructions)
				{
					if (auto fadeNode = objectPtr->AsFadeNode(); fadeNode)
					{
						// The NiAVObject's fade ranges from 0 to 1 for trees to make sure that
						// the low definition model fades along with the full definition one.
						// When fading in/out, we want to be as far from the fade endpoints 
						// as possible so that we don't effectively set the fade value 
						// directly to an endpoint value.
						// So we use the last set fade value or the current reported fade amount,
						// whichever is farther away from the endpoint.
						if (a_shouldFadeOut)
						{
							fadeAmountAtStateChange = max
							(
								currentFadeAmount, objectPtr->fadeAmount
							);
						}
						else
						{
							fadeAmountAtStateChange = min
							(
								currentFadeAmount, objectPtr->fadeAmount
							);
						}

						if (fadeNode->AsTreeNode() || fadeNode->AsLeafAnimNode())
						{
							// Leaves and branches shimmer when not faded out fully.
							lowerBoundFadeAmount = 0.0f;
						}
						else
						{
							lowerBoundFadeAmount = max(0.4, 0.75f / pow(1.15f, fadeIndex));
						}
					}
				}

				if (a_shouldFadeOut) 
				{
					targetFadeAmount = lowerBoundFadeAmount;
				}
				else
				{
					targetFadeAmount = 1.0f;
				}

				interpIntervalProportion = std::clamp
				(
					fabsf(targetFadeAmount - fadeAmountAtStateChange) / 
					(1.0f - lowerBoundFadeAmount), 
					0.0f, 
					1.0f
				);
			}
		}

		// Returns true if the object's fade level was updated successfully 
		// and has not been completely faded in/out.
		// Returns false if the object is invalid or if the object has been completely faded in 
		// and does not need to be handled anymore.
		// Fade index indicates the object's distance from the camera;
		// higher indices mean the object is further from the camera
		// when the fade raycast(s) were performed.
		inline bool UpdateFade() 
		{
			if (objectPtr && objectPtr->GetRefCount() > 0)
			{
				float secsSinceStateChange = Util::GetElapsedSeconds(fadeStateChangeTP);
				float newFadeAmount = targetFadeAmount;
				float tRatio = std::clamp
				(
					secsSinceStateChange / 
					(interpIntervalProportion * Settings::fSecsObjectFadeInterval), 
					0.0f, 
					1.0f
				);
				if (interpIntervalProportion != 0.0f) 
				{
					newFadeAmount = Util::InterpolateSmootherStep
					(
						fadeAmountAtStateChange, targetFadeAmount, tRatio
					);
				}

				currentFadeAmount = objectPtr->fadeAmount = newFadeAmount;
				auto p1 = RE::PlayerCharacter::GetSingleton();
				bool isInteriorCell = p1 && p1->parentCell && !p1->parentCell->IsExteriorCell();
				if (shouldFadeOut)
				{
					// Fade for larger objects such as trees and buildings.
					if (Settings::bFadeLargerObstructions)
					{
						if (auto fadeNode = objectPtr->AsFadeNode(); fadeNode)
						{
							// For now:
							// Decided having an ugly low detail model appear 
							// and properly fade without shadow flickering was better than
							// having flickering shadows and smoother fading 
							// of the high detail model with the low detail model
							// still fading in/out to a lesser degree.
							if (fadeNode->AsTreeNode() || fadeNode->AsLeafAnimNode())
							{
								// Fade node's current fade controls shadow fade, 
								// and the low detail model fades in/out as the high detail model 
								// fades out/in once below half alpha,
								// which is why we start reversing the fade direction 
								// for the low detail model at this point.
								currentFadeAmount = objectPtr->fadeAmount = newFadeAmount;
								fadeNode->currentFade = 
								(
									newFadeAmount >= 0.5f ? 
									newFadeAmount : 
									1.0f - newFadeAmount
								);
							}
							else
							{
								currentFadeAmount = 
								fadeNode->currentFade =
								objectPtr->fadeAmount = 
								newFadeAmount;
							}
						}
					}
				}
				else
				{
					// Fade for larger objects such as trees and buildings.
					if (Settings::bFadeLargerObstructions)
					{
						if (auto fadeNode = objectPtr->AsFadeNode(); fadeNode)
						{
							if (fadeNode->AsTreeNode() || fadeNode->AsLeafAnimNode())
							{
								// Same logic as when fading out.
								objectPtr->fadeAmount = newFadeAmount;
								fadeNode->currentFade = 
								(
									newFadeAmount >= 0.5f ? 
									newFadeAmount : 
									1.0f - newFadeAmount
								);
							}
							else
							{
								fadeNode->currentFade = objectPtr->fadeAmount = newFadeAmount;
							}
						}
					}
				}

				// Completely faded in. Can remove from handled list/set.
				if (tRatio == 1.0f && !shouldFadeOut) 
				{
					// Clear faded flag.
					return false;
				}
				else
				{
					// Set pad to indicate that the object is being faded in/out.
					return true;
				}
			}
			else
			{
				return false;
			}
		}

		// Object which should have its fade level modified.
		RE::NiPointer<RE::NiAVObject> objectPtr;
		// Time point at which the fade direction last changed.
		SteadyClock::time_point fadeStateChangeTP;
		// True if the object should fade out, false if the object should fade in.
		bool shouldFadeOut;
		// Fade level when the fade direction last changed.
		float fadeAmountAtStateChange;
		// Current fade level.
		float currentFadeAmount;
		// Hit position recorded on this object when raycasting from the camera to the player.
		float hitToCamDist;
		// Proportion of the fade interval that has elapsed since the last fade direction change.
		float interpIntervalProportion;
		// Target fade level.
		float targetFadeAmount;
		// Raycast collision result index used to taper off the object's fade amount. 
		// Larger indices mean that the object is further away from the camera.
		uint32_t fadeIndex;
	};

	class CameraManager : public Manager
	{
	public:
		CameraManager();

		// Implements ALYSLC::Manager:
		void MainTask() override;
		void PrePauseTask() override;
		void PreStartTask() override;
		void RefreshData() override;
		const ManagerState ShouldSelfPause() override;
		const ManagerState ShouldSelfResume() override;

		// Clamp Z coordinate to the given bounds, 
		// at the given lower/upper offsets from those bounds.
		inline void ClampToZCoordAboveLowerBound
		(
			float& a_zCoordOut, 
			const float& a_lowerOffset, 
			const float& a_upperOffset,
			const float& a_zUpperBound, 
			const float& a_zLowerBound
		) const
		{
			// If both bounds don't exist, uh oh. Do not modify point.
			if (a_zUpperBound == FLT_MAX && a_zLowerBound == -FLT_MAX) 
			{
				return;
			}
			else if (a_zUpperBound != FLT_MAX && a_zLowerBound != -FLT_MAX)
			{
				// Place above lower bound or below upper bound, 
				// whichever modified Z position is lower.
				float newLowerBound = min
				(
					a_zLowerBound + a_lowerOffset, a_zUpperBound - a_upperOffset
				); 
				if (a_zCoordOut < newLowerBound) 
				{
					a_zCoordOut = newLowerBound;
				}
			}
			else if (a_zUpperBound == FLT_MAX)
			{
				// Simply place above lower bound.
				float newLowerBound = a_zLowerBound + a_lowerOffset; 
				if (a_zCoordOut < newLowerBound)
				{
					a_zCoordOut = newLowerBound;
				}
			}
			else
			{
				// Place above upper bound since we might've fallen through the floor 
				// and the void is below.
				float newLowerBound = a_zUpperBound + a_lowerOffset; 
				if (a_zCoordOut < newLowerBound)
				{
					a_zCoordOut = newLowerBound;
				}
			}
		}
		
		// Clear lock on target-related data.
		inline void ClearLockOnData()
		{
			camLockOnTargetHandle.reset();
			camLockOnFocusPoint = camFocusPoint;
			lockOnTargetInSight = false;
		}

		// Get current camera pitch.
		inline float GetCurrentPitch() const
		{
			if (IsRunning() && playerCam && playerCam->currentState)
			{
				auto tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->currentState.get());
				if (tpState)
				{
					return camPitch;
				}
			}
			else if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
			{
				auto eulerAngles = Util::GetEulerAnglesFromRotMatrix(niCamPtr->world.rotate);
				return eulerAngles.x;
			}

			return RE::PlayerCharacter::GetSingleton()->data.angle.x;
		}
		
		// Return the current position of the camera.
		// If using the co-op camera, return the last set target position.
		// Otherwise, return the camera node position.
		inline RE::NiPoint3 GetCurrentPosition() const
		{
			if (IsRunning())
			{
				return camTargetPos;
			}
			else
			{
				// Return NiCamera position -> player camera position -> P1's position
				// -> nothing because everything has failed.
				if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
				{
					return niCamPtr->world.translate;
				}

				if (playerCam && playerCam->cameraRoot)
				{
					return playerCam->cameraRoot->world.translate;
				}
				
				auto p1 = RE::PlayerCharacter::GetSingleton();
				return (p1 ? p1->data.location : RE::NiPoint3());
			}
		}

		// Get current camera yaw.
		inline float GetCurrentYaw() const
		{
			if (IsRunning() && playerCam && playerCam->currentState)
			{
				auto tpState = skyrim_cast<RE::ThirdPersonState*>(playerCam->currentState.get());
				if (tpState)
				{
					return camYaw;
				}
			}
			else if (auto niCamPtr = Util::GetNiCamera(); niCamPtr)
			{
				auto eulerAngles = Util::GetEulerAnglesFromRotMatrix(niCamPtr->world.rotate);
				return eulerAngles.z;
			}

			return RE::PlayerCamera::GetSingleton()->yaw;
		}

		// Set camera interpolation factors.
		inline void SetCamInterpFactors()
		{
			if (camState == CamState::kManualPositioning) 
			{
				camInterpFactor = Settings::fCamManualPosInterpFactor;
			}
			else
			{
				camInterpFactor = Settings::fCamInterpFactor;
			}
		}

		// Update movement pitch running total and number of readings, or reset both.
		inline void SetMovementPitchRunningTotal(bool a_reset)
		{
			if (!a_reset)
			{
				movementPitchRunningTotal += GetAutoRotateAngle(true);
				++numMovementPitchReadings;
			}
			else
			{
				movementPitchRunningTotal = 0.0f;
				numMovementPitchReadings = 0;
			}
		}

		// Update movement yaw running total and number of readings, or reset both.
		inline void SetMovementYawToCamRunningTotal(bool a_reset) 
		{
			if (!a_reset) 
			{
				movementYawToCamRunningTotal += GetAutoRotateAngle(false);
				++numMovementYawToCamReadings;
			}
			else
			{
				movementYawToCamRunningTotal = 0.0f;
				numMovementYawToCamReadings = 0;
			}
		}

		// Used externally. Signal the camera manager to wait for toggle 
		// (co-op camera is only re-enabled by P1).
		inline void SetWaitForToggle(bool a_set)
		{
			waitForToggle = a_set;
		}

		// Return true if the current lock on target should be considered as a player
		// for camera origin point, target position, and auto-zoom calculations.
		// Since rotation assistance does not auto-zoom to keep the lock on target in frame, 
		// we do not have to treat the target as a player.
		inline bool ShouldConsiderLockOnTargetAsPlayer()
		{
			auto camLockOnTargetPtr = Util::GetActorPtrFromHandle(camLockOnTargetHandle);
			return 
			(
				isLockedOn && 
				camLockOnTargetPtr && 
				Settings::uLockOnAssistance != !CamLockOnAssistanceLevel::kRotation
			);
		}

		// Reset time point data.
		inline void ResetTPs()
		{
			autoRotateJustResumedTP = SteadyClock::now();
			autoRotateJustSuspendedTP = SteadyClock::now();
			lockOnLOSCheckTP = SteadyClock::now();
			lockOnLOSLostTP = SteadyClock::now();
			noPlayersUnderExteriorRoofTP = SteadyClock::now();
			underExteriorRoofZoomInTP = SteadyClock::now();
			secsSinceLockOnTargetLOSChecked = 0.0f;
			secsSinceLockOnTargetLOSLost = 0.0f;
		}

		// Check if at least one node (or the player's refr position) 
		// is on screen at the given camera orientation.
		bool AllPlayersOnScreenAtCamOrientation
		(
			const RE::NiPoint3& a_camPos,
			const RE::NiPoint2& a_rotation,
			bool&& a_usePlayerPos = false, 
			const std::vector<RE::BSFixedString>&& a_nodeNamesToCheck = { }
		);
		
		// Calculate the next camera focus points: current, base, and collision.
		void CalcNextFocusPoint();
		
		// Calculate the next camera origin points: current, base, and collision.
		void CalcNextOriginPoint();
		
		// Calculate the next camera target points: current, base, and collision.
		void CalcNextTargetPosition();
		
		// Unused for now. Serves as a more performance-friendly, 
		// less precise alternative to the spherical hull cast.
		// Cast from the given start point to the given end point,
		// and if a hit is recorded, return the result.
		// Otherwise, cast the requested number of times 
		// in concentric clusters of 4 about the initial raycast
		// and return the hit result whose hit position is closest to its starting position.
		Raycast::RayResult ClusterCast
		(
			const glm::vec4& a_start, 
			const glm::vec4& a_end, 
			const float& a_radius, 
			const uint32_t& a_additionalRingsOfCasts
		);

		// Handle lock on requests, check for target validity, and enable/disable cam lock on.
		void CheckLockOnTarget();
		
		// Draw a lock on indicator above the current lock on target.
		void DrawLockOnIndicator(const float& a_centerX, const float& a_centerY);
		
		// Fade objects that obscure players from the camera's LOS.
		void FadeObstructions();
		
		// Calculate and return the camera's current average movement pitch or yaw-to-cam angle.
		float GetAutoRotateAngle(bool&& a_computePitch);
		
		// Check if any player is visible via raycast
		// (hit one or all actor nodes) from the given point.
		bool NoPlayersVisibleAtPoint(const RE::NiPoint3& a_point, bool&& a_checkAllNodes = true);
		
		// Checks if the given point is within the camera's frustum 
		// at the given camera orientation (no raycasts for visiblity).
		// Can specify a pixel margin ratio (fraction of screen width/height [0, 1])
		// around the border of the screen as well.
		bool PointOnScreenAtCamOrientationScreenspaceMargin
		(
			const RE::NiPoint3& a_point,
			const RE::NiPoint3& a_camPos,
			const RE::NiPoint2& a_rotation, 
			const float& a_marginRatio
		);

		bool PointOnScreenAtCamOrientationWorldspaceMargin
		(
			const RE::NiPoint3& a_point,
			const RE::NiPoint3& a_camPos,
			const RE::NiPoint2& a_rotation, 
			const float& a_marginWorldDist
		);
		
		// Reset all camera related data.
		void ResetCamData();
		
		// Reset fade on all handled obstructions and then clear them.
		void ResetFadeAndClearObstructions();
		
		// Enable or disable collision between the camera and character controllers.
		void SetCamActorCollisions(bool&& a_set);
		
		// Set orientation (rotation and position) for the camera.
		void SetCamOrientation(bool&& a_overrideLocalRotation);

		// Prevent/enable fading of players.
		void SetPlayerFadePrevention(bool&& a_noFade);
		
		// Toggle the co-op camera on or off.
		void ToggleCoopCamera(bool a_enable);
		
		// Transition the camera back into third person.
		// Used when the game automatically switches to a different camera state.
		// Done with a small delay to avoid odd crashes 
		// and wait for the first person skeleton to disable.
		void ToThirdPersonState(bool&& a_fromFirstPerson);

		// Update the auto rotation angles' (pitch/yaw) multiplier.
		void UpdateAutoRotateAngleMult();
		
		// Update camera pitch and yaw data.
		void UpdateCamRotation();
		
		// Set camera base/current focus point Z coordinate offsets.
		void UpdateCamHeight();
		
		// Update camera zoom data.
		void UpdateCamZoom();
		
		// Update data related to the current cell (P1's parent cell).
		void UpdateParentCell();
		
		// Reset fade for players (set alpha to 1) 
		// or fade them out depending on how close they are to the camera.
		void UpdatePlayerFadeAmounts(bool&& a_reset = false);

		//
		// Members
		//

		// Currently set camera lock on target's handle.
		RE::ActorHandle camLockOnTargetHandle;
		// Base position (before collision calculations).
		RE::NiPoint3 camBaseTargetPos;
		// Position at which the cam collides with geometry (if there are obstructions).
		// Equal to the base position when there are no obstructions.
		RE::NiPoint3 camCollisionTargetPos;
		// Current focus point.
		RE::NiPoint3 camFocusPoint;
		// Current lock on target focus point.
		RE::NiPoint3 camLockOnFocusPoint;
		// Current origin point.
		RE::NiPoint3 camOriginPoint;
		// Current origin point movement direction.
		RE::NiPoint3 camOriginPointDirection;
		// Current focal player's focus point.
		RE::NiPoint3 camPlayerFocusPoint;
		// Current world position of the camera to set.
		RE::NiPoint3 camTargetPos;
		// Player camera.
		RE::PlayerCamera* playerCam;
		// Current cell.
		RE::TESObjectCELL* currentCell;
		// Third person camera state.
		RE::ThirdPersonState* tpState;
		// Current camera state.
		CamState camState;
		// Previous camera state.
		CamState prevCamState;
		// Current camera adjustment mode.
		CamAdjustmentMode camAdjMode;
		// Time points.
		// Last time the movement pitch/yaw running totals started re-accumulating
		// after suspension.
		SteadyClock::time_point autoRotateJustResumedTP;
		// Last time the movement pitch/yaw running totals started to approach 0 
		// outside of updating.
		SteadyClock::time_point autoRotateJustSuspendedTP;
		// Last time an LOS check for the cam lock on target was made.
		SteadyClock::time_point lockOnLOSCheckTP;
		// Last time at which LOS was lost on the cam lock on target.
		SteadyClock::time_point lockOnLOSLostTP;
		// Last time at which no players were under an exterior roof.
		SteadyClock::time_point noPlayersUnderExteriorRoofTP;
		// Last time at which the camera was signalled to zoom in under an exterior roof.
		SteadyClock::time_point underExteriorRoofZoomInTP;
		// Toggle camera POV mutex.
		std::mutex camTogglePOVMutex;
		// Requested lock on target.
		// Nullptr: clear,
		// Valid actor pointer: set to actor,
		// Nullopt: no request.
		std::optional<RE::ActorHandle> lockOnActorReq;
		// Fade data for objects to fade between cam and players.
		std::set<std::unique_ptr<ObjectFadeData>> obstructionFadeDataSet;
		// Linear interpolation data set for oscillating the lock on indicator.
		std::unique_ptr<InterpolationData<float>> lockOnIndicatorOscillationInterpData;
		// Linear interpolation data set for the party's aggregate movement pitch.
		std::unique_ptr<InterpolationData<float>> movementPitchInterpData;
		// Linear interpolation data set 
		// for the party's aggregate movement yaw relative to the camera.
		std::unique_ptr<InterpolationData<float>> movementYawInterpData;
		// Set of handled faded objects and their current fade indices.
		std::unordered_map<RE::NiPointer<RE::NiAVObject>, int32_t> obstructionsToFadeIndicesMap;
		// Interpolated multiplier for auto-rotation angles.
		std::unique_ptr<TwoWayInterpData> movementAngleMultInterpData;
		// List of all map marker refrs in the current cell.
		std::vector<RE::TESObjectREFRPtr> cellMapMarkers;
		// Is auto-rotation suspended (both pitch and yaw values are approaching 0)?
		bool autoRotateSuspended;
		// Should the camera zoom in temporarily if all players are under an exterior roof?
		bool delayedZoomInUnderExteriorRoof;
		// Should the camera zoom out after all players were under an exterior roof and
		// have now moved out from under the roof?
		bool delayedZoomOutUnderExteriorRoof;
		// Current cell is an exterior cell.
		bool exteriorCell;
		// Is the camera automatically trailing the party?
		bool isAutoTrailing;
		// Is the camera colliding with a surface or passing through geometry
		// to get to the next target position (if interpolated)?
		bool isColliding;
		// Is the camera being manually positioned by a player?
		bool isManuallyPositioned;
		// Is the camera locked on to a valid target?
		bool isLockedOn;
		// Is the camera's POV being toggled?
		bool isTogglingPOV;
		// If there is LOS on the current lock on target.
		bool lockOnTargetInSight;
		// Interior cells only:
		// Should lock the camera's orientation until the party is far enough away.
		// Set to true if the camera is enabled near a teleport door that sits between
		// the initial camera target position and the party while in an interior cell.
		// Set to false once the players are past a distance equal to the minimum trailing
		// distance from the door.
		bool lockInteriorOrientationOnInit;
		// Was the toggle bind pressed while the co-op camera was waiting to be toggled on?
		bool toggleBindPressedWhileWaiting;
		// Should wait to toggle the co-op camera on again.
		bool waitForToggle;
		// Max pitch angular magnitude when in the auto-trail camera state.
		// Capped for now to prevent jitter when blending pitch/yaw at high pitch angles.
		const float autoTrailPitchMax = 75.0f * PI / 180.0f;
		// Hull size for anchor points when checking for collisions.
		// Should always be larger than target pos hull size.
		const float camAnchorPointHullSize = 35.0f; //15.0f;
		// Maximum camera rotation rate in radians / second.
		const float camMaxAngRotRate = PI / 1.5f;
		// Max movement speed in units/second when in manual positioning mode.
		const float camManualPosMaxMovementSpeed = 2000.0f;
		// Max movement speed in units/second.
		const float camMaxMovementSpeed = 1000.0f;
		// Hull size for target point when checking for collisions.
		// Should always be smaller than anchor point hull size.
		const float camTargetPosHullSize = 30.0f; //10.0f;
		// Average height of all active players.
		float avgPlayerHeight;
		// Base focus point Z displacement from the origin point.
		float camBaseHeightOffset;
		// Base X rotation for the camera node.
		float camBaseTargetPosPitch;
		// Base Z rotation for the camera node.
		float camBaseTargetPosYaw;
		// Current pitch from the camera node to the focus point.
		float camCurrentPitchToFocus;
		// Current yaw from the camera node to the focus point.
		float camCurrentYawToFocus;
		// Focus point Z displacement from the origin point,
		// after collisions with world geometry are taken into account.
		float camHeightOffset;
		// Interpolation factors for positioning/rotation.
		float camInterpFactor;
		// Max and min Z positions bound by collidable surfaces above and below
		// the current origin point.
		float camMaxAnchorPointZCoord;
		float camMinAnchorPointZCoord;
		// Maximum pitch angle magnitude to clamp the current cam pitch to.
		float camMaxPitchAngMag;
		// Max (approximated) distance the camera can zoom out from the focus point before 
		// before on-screen checks and raycasts fail.
		float camMaxZoomOutDist;
		// Minimum trailing distance (cam node to focus point).
		float camMinTrailingDistance;
		// Radial distance offset to apply on top of the current base radial distance.
		// [0, camMaxZoomOutDist]
		float camRadialDistanceOffset;
		// Camera's current pitch to set.
		float camPitch;
		// Saved exterior radial distance offset
		// (before zooming in while under a roof).
		float camSavedRadialDistanceOffset;
		// Target X rotation for the camera node relative to the focus point.
		// Not necessarily the camera's current pitch to set (if collisions are enabled).
		float camTargetPosPitch;
		// Target Z rotation for the camera node relative to the focus point.
		// Not necessarily the camera's current yaw to set (if collisions are enabled).
		float camTargetPosYaw;
		// Camera's target/desired radial distance from the focus point
		// if there are no collisions from the focus point to the base target position.
		float camTargetRadialDistance;
		// Camera's true radial distance from the focus point
		// after collisions are accounted for.
		float camTrueRadialDistance;
		// Camera's current yaw to set.
		float camYaw;
		// Movement pitch angle total for all players since the last update.
		float movementPitchRunningTotal;
		// Movement yaw angle total for all players since the last update.
		float movementYawToCamRunningTotal;
		// Previous pitch/yaw interp ratio.
		float prevRotInterpRatio;
		// Seconds since LOS to lock on target was checked.
		float secsSinceLockOnTargetLOSChecked;
		// Seconds since LOS to lock on target was lost.
		float secsSinceLockOnTargetLOSLost;
		// ID of the player controller adjusting the camera.
		int32_t controlCamCID;
		// Controller ID for the player given direct focus of the camera.
		// -1 if none or if focal player mode is not enabled.
		int32_t focalPlayerCID;
		// Number of movement pitch angle readings made since the last update.
		uint32_t numMovementPitchReadings;
		// Number of movement yaw angle readings made since the last update.
		uint32_t numMovementYawToCamReadings;
		// P1 cam toggle bind bitmask.
		uint16_t camToggleXIMask;
	};
}

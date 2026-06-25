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

	// This is a pretty gnarly file. Oh well.
	TargetingManager::TargetingManager() : 
		Manager(ManagerType::kTM)
	{ }

	void TargetingManager::Initialize(std::shared_ptr<CoopPlayer> a_p) 
	{
		if (a_p && 
			a_p->deviceID > -1 && 
			a_p->playerID > -1 &&
			a_p->playerID < ALYSLC_MAX_PLAYER_COUNT)
		{
			p = a_p;
			DBG
			(
				"Constructor for {} (0x{:X}), PID, DID: {}, {}, shared ptr count: {}.",
				p && p->coopActor ? p->coopActor->GetName() : "NONE",
				p && p->coopActor ? p->coopActor->formID : 0xDEAD,
				p ? p->playerID : -1,
				p ? p->deviceID : -1,
				p.use_count()
			);
			// Init once.
			if (!glob.allPlayersInit)
			{
				canSMORF = false;
				aimMode = static_cast<AimMode>(Settings::vuDefaultAimMode[p->playerID]);
			}
			
			RefreshData();
		}
		else
		{
			ERR
			(
				"Cannot construct Targeting Manager for device ID {}, player ID {}.", 
				a_p ? a_p->deviceID : -1,
				a_p ? a_p->playerID : -1
			);
		}
	}

	void TargetingManager::MainTask()
	{
		// Update crosshair position and selection first, 
		// and draw the crosshair, player indicator, and aim pitch indicator
		// if no fullscreen menu is open or not controlling menus.
		UpdateTargetingOverlay();
		// Select or clear the aim correction target if aim correction is enabled.
		UpdateAimCorrectionTarget();
		// Update the lock on crosshair and activation targets.
		UpdateLockOnTargets();
		// Update target motion state next once a crosshair target or aim correction target
		// have been selected or cleared.
		UpdateTargetedRefrMotionState();
		// Open/close QuickLoot menu if the mod is installed and if targeting a valid refr.
		HandleQuickLootMenu();
		// Update the player's detection state and award Sneak skill XP as necessary.
		UpdateSneakState();
		// Update the player's crosshair text entry with externally-requested
		// or periodic information.
		UpdateCrosshairMessage();
		// Handle grabbed reference motion and positioning.
		// Done here because the crosshair target and motion state must be updated first
		// if throwing any grabbed references.
		HandleReferenceManipulation();
		// Draw projected trajectories for any projectile-launching attacks
		// last after all data relevant to computing trajectories is available.
		DrawTrajectories();
	}

	void TargetingManager::PrePauseTask()
	{
		DBG
		(
			"P{}: Grabbed/released refr info list sizes: {}, {}.",
			p->playerID + 1,
			rmm->grabbedRefrInfoList.size(),
			rmm->releasedRefrInfoList.size()
		);
		
		auto ui = RE::UI::GetSingleton();
		if (nextState == ManagerState::kAwaitingRefresh ||
			ui->IsMenuOpen(RE::FaderMenu::MENU_NAME))
		{
			// Clear all targets.
			ClearTargetHandles();
			// No longer selecting a crosshair target.
			validCrosshairRefrHit = false;
			rmm->ClearAll();
			// Move crosshair back to its default position and hide.
			DeactivateCrosshair();
		}
		else if (glob.cam->IsPaused())
		{
			// Drop all grabbed objects and stop handling released objects 
			// when toggling off the co-op camera.
			rmm->ClearAll();
		}
		else
		{
			rmm->ClearReleasedRefrs();
			if (p->isDowned)
			{
				rmm->ClearGrabbedRefrs();
			}
		}

		// TODO:
		// PR for TrueHUD to allow for continuous display of actor info/boss bars 
		// for players even when not in combat.
		// Commented out for now.
		/*if (auto trueHUDAPI3 = ALYSLC::TrueHUDCompat::g_trueHUDAPI3; trueHUDAPI3)
		{
			const auto handle = coopActor->GetHandle();
			if (trueHUDAPI3->HasInfoBar(handle))
			{
				trueHUDAPI3->RemoveActorInfoBar(handle, TRUEHUD_API::WidgetRemovalMode::Normal);
				trueHUDAPI3->RemoveBoss(handle, TRUEHUD_API::WidgetRemovalMode::Normal);
			}
		}*/
	}

	void TargetingManager::PreStartTask()
	{
		DBG("P{}", playerID + 1);

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
			// Temporary solution until I figure out what triggers the 'character controller 
			// and 3D desync warp glitch', which occurs ~0.5 seconds after unpausing 
			// with a player previously grabbed.
			// Ragdolling fixes the issue, but I need to find a way to detect 
			// if this desync is happening and correct it in the UpdateGrabbedReferences() call.
			// Solution: If grabbed by another player, release this player before resuming.
			rmm->ClearPlayerIfGrabbed(p);
			// Clear out any lingering released refrs.
			rmm->ClearReleasedRefrs();
		}

		// Clear out game crosshair pick refr too.
		if (p->isPlayer1)
		{
			Util::SendCrosshairEvent(nullptr);
		}
	}

	void TargetingManager::RefreshData()
	{
		// Player data.
		deviceID = p->deviceID;
		playerID = p->playerID;
		coopActor = p->coopActor;

		// Projectile manager.
		if (mph)
		{
			// TODO:
			//std::unique_lock<std::mutex> lock(mph->managedProjMapMutex);
			mph->Clear();
		}
		else
		{
			mph = std::make_unique<ManagedProjectileHandler>();
		}
		
		// Grabbed/released object manipulation manager.
		if (rmm)
		{
			// TODO:
			//std::unique_lock<std::mutex> lock(rmm->manipulatedRefrMutex);
			rmm->ClearAll();
		}
		else
		{
			rmm = std::make_unique<RefrManipulationManager>();
		}

		// Motion state.
		if (targetMotionState)
		{
			targetMotionState->Refresh();
		}
		else
		{
			targetMotionState = std::make_unique<RefrTargetMotionState>();
		}

		// Crosshair text messages.

		// Current.
		if (crosshairMessage)
		{
			crosshairMessage->Clear();
		}
		else
		{
			crosshairMessage = std::make_unique<CrosshairMessage>();
		}

		// Externally set.
		if (extCrosshairMessage)
		{
			extCrosshairMessage->Clear();
		}
		else
		{
			extCrosshairMessage = std::make_unique<CrosshairMessage>();
		}

		// Last set.
		if (lastCrosshairMessage)
		{
			lastCrosshairMessage->Clear();
		}
		else
		{
			lastCrosshairMessage = std::make_unique<CrosshairMessage>();
		}

		// UI element fade data.

		// Activation indicator oscillation.
		if (activationIndicatorOscillationData)
		{
			activationIndicatorOscillationData->Reset(true, true);
		}
		else
		{
			activationIndicatorOscillationData = std::make_unique<TwoWayInterpData>();
		}

		activationIndicatorOscillationData->SetInterpInterval
		(
			Settings::fSecsBetweenActivationChecks, true
		);
		activationIndicatorOscillationData->SetInterpInterval
		(
			Settings::fSecsBetweenActivationChecks, false
		);

		// Aim correction indicator oscillation.
		if (aimCorrectionIndicatorOscillationData)
		{
			aimCorrectionIndicatorOscillationData->Reset(true, true);
		}
		else
		{
			aimCorrectionIndicatorOscillationData = std::make_unique<TwoWayInterpData>();
		}
		
		aimCorrectionIndicatorOscillationData->SetInterpInterval
		(
			Settings::vfSecsToOscillateCrosshair[playerID], true
		);
		aimCorrectionIndicatorOscillationData->SetInterpInterval
		(
			Settings::vfSecsToOscillateCrosshair[playerID], false
		);

		// Aim correction indicator rotation.
		if (aimCorrectionIndicatorRotationData)
		{
			aimCorrectionIndicatorRotationData->Reset(true, true);
		}
		else
		{
			aimCorrectionIndicatorRotationData = std::make_unique<TwoWayInterpData>();
		}

		aimCorrectionIndicatorRotationData->SetInterpInterval
		(
			Settings::vfSecsToRotateCrosshair[playerID], true
		);
		aimCorrectionIndicatorRotationData->SetInterpInterval
		(
			Settings::vfSecsToRotateCrosshair[playerID], false
		);

		// Aim pitch indicator fade.
		if (aimPitchIndicatorFadeInterpData)
		{
			aimPitchIndicatorFadeInterpData->Reset(true, true);
		}
		else
		{
			aimPitchIndicatorFadeInterpData = std::make_unique<TwoWayInterpData>();
		}

		aimPitchIndicatorFadeInterpData->SetInterpInterval(0.25f, true);
		aimPitchIndicatorFadeInterpData->SetInterpInterval(0.5f, false);

		// Crosshair fade.
		if (crosshairFadeInterpData)
		{
			crosshairFadeInterpData->Reset(true, true);
		}
		else
		{
			crosshairFadeInterpData = std::make_unique<TwoWayInterpData>();
		}

		crosshairFadeInterpData->SetInterpInterval(0.5f, true);
		crosshairFadeInterpData->SetInterpInterval(1.0f, false);

		// Crosshair size.
		if (crosshairSizeRatioInterpData)
		{
			crosshairSizeRatioInterpData->Reset(true, true);
		}
		else
		{
			crosshairSizeRatioInterpData = std::make_unique<TwoWayInterpData>();
		}

		crosshairSizeRatioInterpData->SetInterpInterval(1.0f, true);
		crosshairSizeRatioInterpData->SetInterpInterval(1.0f, false);

		// Player indicator fade.
		if (playerIndicatorFadeInterpData)
		{
			playerIndicatorFadeInterpData->Reset(true, true);
		}
		else
		{
			playerIndicatorFadeInterpData = std::make_unique<TwoWayInterpData>();
		}

		playerIndicatorFadeInterpData->SetInterpInterval(1.0f, true);
		playerIndicatorFadeInterpData->SetInterpInterval(1.0f, false);

		// Crosshair oscillation.
		if (crosshairOscillationData)
		{
			crosshairOscillationData->ResetData();
			// Differing starting oscillation values for each player so the crosshairs 
			// do not completely overlap when over a target.
			crosshairOscillationData->prev = 
			crosshairOscillationData->current = static_cast<float>(playerID) * 0.25f;
			crosshairOscillationData->next = 1.0f;
			crosshairOscillationData->SetUpdateInterval
			(
				Settings::vfSecsToOscillateCrosshair[playerID]
			);
			crosshairOscillationData->secsSinceUpdate = 
			(
				crosshairOscillationData->secsUpdateInterval * crosshairOscillationData->current
			);
		}
		else
		{
			crosshairOscillationData = std::make_unique<InterpolationData<float>>
			(
				static_cast<float>(playerID) * 0.25f, 
				static_cast<float>(playerID) * 0.25f, 
				1.0f, 
				Settings::vfSecsToOscillateCrosshair[playerID]
			);
			crosshairOscillationData->secsSinceUpdate = 
			(
				crosshairOscillationData->secsUpdateInterval * crosshairOscillationData->current
			);
		}
		
		// Crosshair rotation.
		if (crosshairRotationData)
		{
			crosshairRotationData->ResetData();
			crosshairRotationData->SetUpdateInterval(Settings::vfSecsToRotateCrosshair[playerID]);
		}
		else
		{
			crosshairRotationData = std::make_unique<InterpolationData<float>>
			(
				0.0f, 0.0f, 0.0f, Settings::vfSecsToRotateCrosshair[playerID]
			);
		}

		// Target handles.
		// Clear all target handles, not just crosshair selection-related ones.
		ClearTargetHandles();

		// World positions.
		crosshairLastMovementHitPosOffset = 
		crosshairInitialMovementHitPosOffset = 
		crosshairLocalPosOffset = RE::NiPoint3();
		crosshairWorldPos = 
		lastActivationReqPos = Util::GetTorsoPosition(coopActor.get());

		// Crosshair scaleform position.
		ResetCrosshairPosition();
		// Player indicator position.
		playerIndicatorScaleformPos = glm::vec2(0.0f, 0.0f);

		// Nearby refrs.
		nearbyObjectsOfSameType.clear();
		nearbyReferences.clear();
		// Bools.
		baseCanDrawOverlayElements = true;
		canActivateRefr = false;
		choseClosestResult = false;
		choseProximityActivationTarget = false;
		choseLockOnAimTarget = false;
		choseQuickActivationTarget = false;
		crosshairActive = false;
		crosshairManuallyAdjusted = false;
		crosshairRefrInSight = false;
		isMARFing = false;
		isSMORFing = false;
		lockOnToAimCorrectionTarget = false;
		selectedRefrInRangeForQuickLoot = false;
		shouldFindLockOnTargetFromPlayer = false;
		shouldResetCrosshairPosition = false;
		startedActivationCycling = false;
		validCrosshairRefrHit = false;
		wantsToSMORF = false;
		// Floats.
		closestHostileActorDist = FLT_MAX;
		crosshairLocalPosPitchDiff = 0.0f;
		crosshairLocalPosYawDiff = 0.0f;
		crosshairSpeedMult = 1.0f;
		detectionPct = 100.0f;
		grabbedRefrDistanceOffset = 0.0f;
		lastActivationFacingAngle = coopActor->GetHeading(false);
		playerIndicatorHeight = 0.0f;
		// Reach set to twice the actor's height initially.
		maxReachActivationDist = coopActor->GetHeight() * 2.0f;
		// Reset durations.
		secsSinceLastStealthStateCheck = 
		secsSinceTargetVisibilityLost = 
		secsSinceVisibleOnScreenCheck = 0.0f;
		// Ints.
		// 100% percent detection corresponds to green.
		detectionPctRGB = 0x00FF00;

		// Lastly, set player's pitch angle to 0, so that their pitch
		// angle since the last session was active does not carry over.
		coopActor->data.angle.x = 0.0f;
		// Reset all target handles, related data, and time points.
		ResetTargeting();
		ResetTPs();

		// TEMPORARY. REMOVE after testing out new 'Move Crosshair' bind with LockOn aim mode.
		aimMode = static_cast<AimMode>(Settings::vuDefaultAimMode[playerID]);
		DBG("{}: Aim mode {}.", coopActor ? coopActor->GetName() : "NONE", !aimMode);
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

	bool TargetingManager::CanActivateRefr(RE::TESObjectREFR* a_refr, bool a_checkLOS)
	{
		// Check if the given activation target refr is valid for activation,
		// and return true if so.
			
		if (!a_refr)
		{
			return false;
		}

		auto targetIndex = GlobalCoopData::GetCoopPlayerIndex(a_refr);
		// Set revive message if there is a downed player target.
		if (targetIndex != -1)
		{
			if (p->pam->downedPlayerTarget == glob.coopPlayers[targetIndex])
			{
				return HelperFuncs::EnoughOfAVToPerformPA(p, InputAction::kActivate);
			}
			else
			{
				return true;
			}
		}
		else
		{
			// Set activation message if refr is valid.
			if (Util::IsValidRefrForTargeting(a_refr))
			{
				// Get base object; return early if invalid.
				auto baseObj = a_refr->GetObjectReference(); 
				if (!baseObj)
				{
					return false;
				}

				// Influences what objects this player can activate 
				// (nothing that will open a menu if another player is controlling menus).
				bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(playerID);
				// Activation will teleport P1.
				bool tryingToUseTeleportRefr = 
				(
					a_refr->extraList.HasType<RE::ExtraTeleport>()
				);
				// Ensure that players cannot activate any refr that will teleport the party, 
				// and consequently auto-save, while a player is downed.
				bool otherPlayerDowned = std::any_of
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(), 
					[](const auto& a_p) 
					{
						if (a_p->isActive && a_p->isDowned)
						{
							return true;
						}

						return false;
					}
				);
				// Other activation criteria.
				bool menusOnlyAlwaysOpen = true;
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					menusOnlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				}

				bool isLocked = a_refr->IsLocked();
				// Is locked and P1 has the key.
				bool canUnlockWithKey = false;
				if (isLocked)
				{
					auto lockData = a_refr->extraList.GetByType<RE::ExtraLock>(); 
					if (lockData && lockData->lock)
					{
						// Check if P1 has the key.
						auto inventoryCounts = glob.player1Actor->GetInventoryCounts();
						auto key = lockData->lock->key;
						if (inventoryCounts.contains(key))
						{
							canUnlockWithKey = true;
						}
					}
				}

				// P1 has at least 1 lockpick.
				bool hasLockpicks = 
				(
					Util::GetLockpicksCount(RE::PlayerCharacter::GetSingleton()) > 0
				);
				// A crime to activate.
				bool offLimits = Util::ActivationIsOffLimits(p->coopActor.get(), a_refr);
				// Object prevented from being activated (ex. door bars).
				bool activationBlocked = false;
				auto xFlags = a_refr->extraList.GetByType<RE::ExtraFlags>(); 
				if (xFlags)
				{
					activationBlocked = 
					(
						xFlags &&
						xFlags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate) && 
						!a_refr->extraList.GetByType<RE::ExtraAshPileRef>()
					);
				}

				const auto handle = a_refr->GetHandle();
				// In activation range.
				bool isInRange = p->tm->RefrIsInActivationRange(handle);
				// Is a lootable refr.
				bool isLootable = Util::IsLootableRefr(a_refr);
				// Player is sneaking.
				bool isSneaking = p->coopActor->IsSneaking();
				// Something to do with usability.
				bool isPlayable = a_refr->GetPlayable();
				// Player has LOS on the refr.
				// Use the game's P1 LOS check for crosshair refrs not selected via raycast,
				// since our raycasts do not hit such refrs right now.
				bool passesLOSCheck =
				(
					(!a_checkLOS) ||
					(
						Util::HasLOS
						(
							a_refr, 
							p->coopActor.get(), 
							false, 
							p->tm->crosshairRefrHandle == handle, 
							p->tm->crosshairWorldPos
						)
					)
				);
					\
				if (!isSneaking && offLimits)
				{
					return false;
				}
				else if (!isPlayable || activationBlocked)
				{
					return false;
				}
				else if (isLocked && !hasLockpicks && !canUnlockWithKey)
				{
					return false;
				}
				else if (otherPlayerDowned && tryingToUseTeleportRefr)
				{
					return false;
				}
				else if (!menusOnlyAlwaysOpen && anotherPlayerControllingMenus && !isLootable)
				{
					return false;
				}
				else if (!passesLOSCheck)
				{
					return false;
				}
				else
				{
					return !offLimits;
				}
			}
		}

		return false;
	}

	void TargetingManager::ClearActivationTargetData()
	{
		// Clear out currently-targeted activation/proximity refrs.
		// If the activation refr is valid, will stop any playing activation shader as well.
		auto activationRefrPtr = Util::GetRefrPtrFromHandle
		(
			activationRefrHandle
		); 
		if (activationRefrPtr)
		{
			Util::StopAllActivationEffectShaders(activationRefrPtr.get(), playerID);
		}
		
		DBG
		(
			"{}: {}. Chose quick target: {}", 
			coopActor->GetName(), 
			Util::HandleIsValid(activationRefrHandle) ?
			activationRefrHandle.get()->GetName() : 
			"NONE",
			choseQuickActivationTarget
		);
		choseProximityActivationTarget = false;
		choseQuickActivationTarget = false;
		activationRefrHandle = RE::ObjectRefHandle();
	}

	void TargetingManager::ClearTarget(const TargetActorType& a_targetType)
	{
		// Clear the actor target handle that corresponds to the given target type.

		{
			std::unique_lock<std::mutex> targetingLock(targetingMutex, std::try_to_lock);
			if (targetingLock)
			{
				DBG
				(
					"{}: Lock obtained. (0x{:X})",
					coopActor->GetName(), std::hash<std::jthread::id>()(std::this_thread::get_id())
				);

				if (a_targetType == TargetActorType::kAimCorrection)
				{
					aimCorrectionTargetHandle.reset();
					lockOnToAimCorrectionTarget = false;
				}
				else if (a_targetType == TargetActorType::kLinkedRefr)
				{
					// Update linked refr using the aim target keyword first.
					if (p->aimTargetKeyword) 
					{
						coopActor->extraList.SetLinkedRef(nullptr, p->aimTargetKeyword);
					}

					aimTargetLinkedRefrHandle.reset();
				}
				else if (a_targetType == TargetActorType::kSelected)
				{
					selectedTargetActorHandle.reset();
				}
			}
			else
			{
				DBG
				(
					"{}: Failed to obtain lock. (0x{:X})",
					coopActor->GetName(), std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}
	}

	void TargetingManager::ColorizeActivationShader
	(
		RE::TESEffectShader* a_shader, bool a_canActivateRefr
	)
	{
		// Change the color of this player's activation shaders to match 
		// their main UI Overlay color.
		// If indicating a failed activation, colorize grey.
		// If indicating use instead of take, colorize white edged with the player's overlay color.

		if (!a_shader)
		{
			return;
		}

		// Default to grey.
		uint8_t red = 0x10;
		uint8_t green = 0x10;
		uint8_t blue = 0x10;
		uint8_t alpha = 0xFF;

		if (a_shader == glob.activateUseShader)
		{
			if (a_canActivateRefr)
			{
				red = 
				(
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] &
						0xFF000000
					) >> 24
				);
				green = 
				(
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 
						0x00FF0000
					) >> 16
				);
				blue = 
				(
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 
						0x0000FF00
					) >> 8
				);
			}
		
			// Main fill portion of the shader is the color of the player's outer crosshair outline.
			a_shader->data.colorKey1.alpha = alpha;
			a_shader->data.colorKey1.red = red;
			a_shader->data.colorKey1.green = green;
			a_shader->data.colorKey1.blue = blue;
			a_shader->data.colorKey2.alpha = alpha;
			a_shader->data.colorKey2.red = red;
			a_shader->data.colorKey2.green = green;
			a_shader->data.colorKey2.blue = blue;
			a_shader->data.colorKey3.alpha = alpha;
			a_shader->data.colorKey3.red = red;
			a_shader->data.colorKey3.green = green;
			a_shader->data.colorKey3.blue = blue;
			a_shader->data.fillTextureEffectColorKey1.alpha = alpha;
			a_shader->data.fillTextureEffectColorKey1.red = red;
			a_shader->data.fillTextureEffectColorKey1.green = green;
			a_shader->data.fillTextureEffectColorKey1.blue = blue;
			a_shader->data.fillTextureEffectColorKey2.alpha = alpha;
			a_shader->data.fillTextureEffectColorKey2.red = red;
			a_shader->data.fillTextureEffectColorKey2.green = green;
			a_shader->data.fillTextureEffectColorKey2.blue = blue;
			a_shader->data.fillTextureEffectColorKey3.alpha = alpha;
			a_shader->data.fillTextureEffectColorKey3.red = red;
			a_shader->data.fillTextureEffectColorKey3.green = green;
			a_shader->data.fillTextureEffectColorKey3.blue = blue;

			if (a_canActivateRefr)
			{
				red = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] & 
						0xFF000000
					) >> 24
				);
				green = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] & 
						0x00FF0000
					) >> 16
				);
				blue = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] &
						0x0000FF00
					) >> 8
				);
			}
				
			// Edge is the player's main overlay color.
			a_shader->data.edgeEffectColor.alpha = alpha;
			a_shader->data.edgeEffectColor.red = red;
			a_shader->data.edgeEffectColor.green = green;
			a_shader->data.edgeEffectColor.blue = blue;
			a_shader->data.edgeColor.alpha = alpha;
			a_shader->data.edgeColor.red = red;
			a_shader->data.edgeColor.green = green;
			a_shader->data.edgeColor.blue = blue;
		}
		else
		{
			if (a_canActivateRefr)
			{
				red = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] & 
						0xFF000000
					) >> 24
				);
				green = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] & 
						0x00FF0000
					) >> 16
				);
				blue = 
				(
					(
						Settings::vuOverlayRGBAValues[playerID] &
						0x0000FF00
					) >> 8
				);
			}
		
			// Edge color here fills the entire shader, so we use the player's overlay color.		
			a_shader->data.edgeEffectColor.alpha = alpha;
			a_shader->data.edgeEffectColor.red = red;
			a_shader->data.edgeEffectColor.green = green;
			a_shader->data.edgeEffectColor.blue = blue;
			a_shader->data.edgeColor.alpha = alpha;
			a_shader->data.edgeColor.red = red;
			a_shader->data.edgeColor.green = green;
			a_shader->data.edgeColor.blue = blue;
		}
	}

	void TargetingManager::DeactivateCrosshair()
	{
		// Clear the current crosshair target, request to reset the crosshair's position, 
		// reset crosshair data + offsets, and set as inactive.
		
		selectedTargetActorHandle = RE::ActorHandle();
		crosshairRefrHandle = RE::ObjectRefHandle();
		crosshairLocalPosOffset = 
		crosshairLastMovementHitPosOffset = 
		crosshairInitialMovementHitPosOffset = RE::NiPoint3();
		crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };

		crosshairManuallyAdjusted = false;
		shouldResetCrosshairPosition = true;
		ClearAimTargetData();
	}

	void TargetingManager::DrawActivationTargetIndicator()
	{
		// Draw the lower portion of the player indicator 
		// to mark the player's chosen activation target if it is not the crosshair target.

		// Need to have a valid activation target that is not the crosshair refr
		// and be performing an activation player action.

		auto ui = RE::UI::GetSingleton();
		auto activationRefrPtr = Util::GetRefrPtrFromHandle(activationRefrHandle);
		bool isActivating = p->pam->IsPerformingOneOf
		(
			InputAction::kActivate, 
			InputAction::kActivateAllOfType, 
			InputAction::kActivateCancel
		);
		bool hasLockOnActivationTarget = Util::HandleIsValid(activationRefrHandle);
		bool shouldNotDraw = 
		(
			(!baseCanDrawOverlayElements) ||
			(!activationRefrPtr || !Util::IsValidRefrForTargeting(activationRefrPtr.get())) ||
			(!hasLockOnActivationTarget && !isActivating)
		);
		if (shouldNotDraw)
		{
			return;
		}

		if (Settings::bRingIndicatorForActivation)
		{
			const auto refr3DPtr = activationRefrPtr->GetCurrent3D();
			auto centerScreenPos = Util::WorldToScreenPoint3
			(
				activationRefrPtr->As<RE::Actor>() ? 
				Util::GetTorsoPosition(activationRefrPtr->As<RE::Actor>()) : 
				Util::Get3DCenterPos(activationRefrPtr.get())
			);
			auto topScreenPos = Util::WorldToScreenPoint3
			(
				activationRefrPtr->As<RE::Actor>() ? 
				Util::GetHeadPosition(activationRefrPtr->As<RE::Actor>()) : 
				Util::Get3DCenterPos(activationRefrPtr.get()) + 
				RE::NiPoint3(0.0f, 0.0f, activationRefrPtr->GetHeight())
			);
			float radius = min
			(
				Settings::vfCrosshairLength[playerID],
				0.5f * Util::GetBoundMaxOrMinEdgeDist
				(
					activationRefrPtr.get(), true, true
				)	
			);

			const float thickness = 0.125f * radius;
			const auto center = ToVec3(centerScreenPos);
			float gapDelta = 0.0f;
			// Animate for better visibility.
			if ((activationIndicatorOscillationData->interpToMax &&
				activationIndicatorOscillationData->value != 1.0f) ||
				(activationIndicatorOscillationData->interpToMin && 
				activationIndicatorOscillationData->value != 0.0f))
			{
				activationIndicatorOscillationData->UpdateInterpolatedValue
				(
					activationIndicatorOscillationData->directionChangeFlag
				);
			}
			else
			{
				activationIndicatorOscillationData->UpdateInterpolatedValue
				(
					!activationIndicatorOscillationData->directionChangeFlag
				);
			}

			gapDelta = (activationIndicatorOscillationData->value * radius);
			// Fewer segments to draw when the gap is small (no readily apparent loss in quality).
			uint32_t numSegments = std::clamp(static_cast<int>(gapDelta * 3), 8, 48);
			DebugAPI::QueueCircle2D
			(
				center,
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				numSegments,
				2.0f * thickness + gapDelta,
				thickness,
				0.0f
			);
			DebugAPI::QueueCircle2D
			(
				center,
				Settings::vuOverlayRGBAValues[p->playerID],
				numSegments,
				thickness + gapDelta,
				thickness,
				0.0f
			);
			DebugAPI::QueueCircle2D
			(
				center,
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				numSegments,
				gapDelta,
				thickness,
				0.0f
			);
		}
		else
		{
			// Also do not draw the activation indicator if the player indicator is visible.
			// Too cluttered with both visible at once.
			const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(activationRefrHandle);
			bool playerIndicatorVisible = 
			(
				pIndex != -1 && 
				glob.coopPlayers[pIndex]->tm->playerIndicatorFadeInterpData->value > 0.0f
			);
			// Offset each player's capping circle upward from the torso position
			// to the head position, based on their player ID, 
			// so the circles do not intersect with each other when the same aim correction target 
			// is selected by multiple players.
			const float& indicatorBaseThickness = 
			(
				Settings::vfPlayerIndicatorThickness[p->playerID]
			);
			const float pixelHeight = Util::GetBoundPixelDist(activationRefrPtr.get(), true);
			auto screenBasePos = RE::NiPoint3();
			if (playerIndicatorVisible)
			{
				glm::vec2 scaleformPos = 
				{
					glob.coopPlayers[pIndex]->tm->playerIndicatorScaleformPos.x, 
					glob.coopPlayers[pIndex]->tm->playerIndicatorScaleformPos.y - 
					glob.coopPlayers[pIndex]->tm->playerIndicatorHeight - 
					5.0f
				};
				DebugAPI::ClampPointToScreen(scaleformPos);
				screenBasePos = RE::NiPoint3(scaleformPos.x, scaleformPos.y, 0.0f);
			}
			else
			{
				const auto asActor = activationRefrPtr->As<RE::Actor>();
				const auto basePos = 
				(
					asActor ? 
					Util::GetHeadPosition(asActor) + 
					RE::NiPoint3(0.0f, 0.0f, Util::GetHeadRadius(asActor) + 5.0f) :
					Util::Get3DCenterPos(activationRefrPtr.get())
				);
				screenBasePos = Util::WorldToScreenPoint3(basePos);
			}

			auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;
			const float indicatorLength = std::clamp
			(
				pixelHeight,
				DebugAPI::screenResY * 0.01f, 
				DebugAPI::screenResY * 0.02f
			);
			const float scalingFactor = 
			(
				indicatorLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH
			);
			const float indicatorThickness = indicatorBaseThickness * scalingFactor;
			const float indicatorGap = max(2.0f, indicatorLength);
			if ((activationIndicatorOscillationData->interpToMax &&
				activationIndicatorOscillationData->value != 1.0f) ||
				(activationIndicatorOscillationData->interpToMin && 
				activationIndicatorOscillationData->value != 0.0f))
			{
				activationIndicatorOscillationData->UpdateInterpolatedValue
				(
					activationIndicatorOscillationData->directionChangeFlag
				);
			}
			else
			{
				activationIndicatorOscillationData->UpdateInterpolatedValue
				(
					!activationIndicatorOscillationData->directionChangeFlag
				);
			}

			// Points are offset downward from origin (+Y Scaleform axis).
			// Have to rebase from the bottom tip by subtracting the length for each segment,
			// multiplying with the base scaling offset, and then factoring in the gap.
			float gapDelta = activationIndicatorOscillationData->value * indicatorGap;
			for (auto& offset : lowerPortionOffsets)
			{
				offset *= scalingFactor;
				offset.y -= gapDelta;
			}

			const auto port = Util::GetPort();
			const float trueLength = 
			(
				indicatorLength + 2.0f * indicatorThickness + gapDelta
			);
			const float trueWidth = 
			(
				0.5f * 
				scalingFactor *
				(
					GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[4].x - 
					GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[0].x
				)
			);
			glm::vec2 posScreenCoords{ screenBasePos.x, screenBasePos.y };
			DebugAPI::QueueShape2D
			(
				posScreenCoords,
				lowerPortionOffsets,
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				false, 
				1.5f * indicatorThickness,
				0.0f
			);
			DebugAPI::QueueShape2D
			(
				posScreenCoords,
				lowerPortionOffsets,
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				false, 
				indicatorThickness,
				0.0f
			);
			DebugAPI::QueueShape2D
			(
				posScreenCoords,
				lowerPortionOffsets, 
				Settings::vuOverlayRGBAValues[p->playerID]
			);
		}
	}

	void TargetingManager::DrawAimCorrectionIndicator()
	{
		// Draw two concentric circles to mark the player's aim pitch indicator.
		// Draw 'X' prongs if in face target mode while the crosshair is disabled.
		
		//
		// Double twin arrows for twin sticks. Oh yeah.
		//
		
		// Unnecessary to draw if aim correction is disabled and the crosshair is enabled.
		if (!Settings::vbUseAimCorrection[playerID] && aimMode == AimMode::kCrosshair)
		{
			return;
		}

		// Need to have an aim correction target.
		auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle);
		if (!aimCorrectionTargetPtr || crosshairActive)
		{
			return;
		}

		/*
		// Also do not draw the activation indicator if the player indicator is visible.
		// Too cluttered with both visible at once.
		const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(activationRefrHandle);
		bool playerIndicatorVisible = 
		(
			pIndex != -1 && 
			glob.coopPlayers[pIndex]->tm->playerIndicatorFadeInterpData->value > 0.0f
		);
		if (playerIndicatorVisible)
		{
			return;
		}

		// Offset each player's capping circle upward from the torso position
		// to the head position, based on their player ID, 
		// so the circles do not intersect with each other when the same aim correction target 
		// is selected by multiple players.
		const float& indicatorBaseThickness = 
		(
			Settings::vfPlayerIndicatorThickness[p->playerID]
		);
		const auto basePos = 
		(
			Util::GetHeadPosition(aimCorrectionTargetPtr.get()) + 
			RE::NiPoint3(0.0f, 0.0f, Util::GetHeadRadius(aimCorrectionTargetPtr.get()) + 5.0f)
		);
		const float pixelHeight = Util::GetBoundPixelDist(aimCorrectionTargetPtr.get(), true);
		auto screenBasePos = Util::WorldToScreenPoint3(basePos);
		auto screenTopPos = screenBasePos + RE::NiPoint3(0.0f, 0.0f, pixelHeight * 0.25f);
		auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;
		const float indicatorLength = std::clamp
		(
			pixelHeight,
			DebugAPI::screenResY * 0.01f, 
			DebugAPI::screenResY * 0.02f
		);
		const float scalingFactor = indicatorLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
		const float indicatorThickness = indicatorBaseThickness * scalingFactor;
		const float indicatorGap = max(2.0f, indicatorLength);
		if ((aimCorrectionIndicatorOscillationData->interpToMax &&
			aimCorrectionIndicatorOscillationData->value != 1.0f) ||
			(aimCorrectionIndicatorOscillationData->interpToMin && 
			aimCorrectionIndicatorOscillationData->value != 0.0f))
		{
			aimCorrectionIndicatorOscillationData->UpdateInterpolatedValue
			(
				aimCorrectionIndicatorOscillationData->directionChangeFlag
			);
		}
		else
		{
			aimCorrectionIndicatorOscillationData->UpdateInterpolatedValue
			(
				!aimCorrectionIndicatorOscillationData->directionChangeFlag
			);
		}

		// Points are offset downward from origin (+Y Scaleform axis).
		// Have to rebase from the bottom tip by subtracting the length for each segment,
		// multiplying with the base scaling offset, and then factoring in the gap.
		float gapDelta = aimCorrectionIndicatorOscillationData->value * indicatorGap;
		for (auto& offset : lowerPortionOffsets)
		{
			offset *= scalingFactor;
			offset.y -= gapDelta;
		}

		const auto port = Util::GetPort();
		const float trueLength = 
		(
			indicatorLength + 2.0f * indicatorThickness + gapDelta
		);
		const float trueWidth = 
		(
			0.5f * 
			scalingFactor *
			(
				GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[4].x - 
				GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS[0].x
			)
		);
		glm::vec2 posScreenCoords{ screenBasePos.x, screenBasePos.y };
		DebugAPI::QueueShape2D
		(
			posScreenCoords,
			lowerPortionOffsets,
			Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
			false, 
			indicatorThickness,
			0.0f
		);
		DebugAPI::QueueShape2D
		(
			posScreenCoords,
			lowerPortionOffsets, 
			Settings::vuOverlayRGBAValues[p->playerID]
		);
		*/

		bool shouldFaceTarget = Settings::bRingIndicatorForActivation;
		auto screenTorsoPos = Util::WorldToScreenPoint3
		(
			Util::GetTorsoPosition(aimCorrectionTargetPtr.get())
		);
		auto screenHeadPos = Util::WorldToScreenPoint3
		(
			Util::GetHeadPosition(aimCorrectionTargetPtr.get())
		);
		auto diff = (screenHeadPos - screenTorsoPos);
		// Cap the radius and modify thickness based on distance from the camera.
		float radius = std::clamp
		(
			diff.Length(),
			1.0f * Settings::vfCrosshairGapRadius[playerID], 
			2.0f * Settings::vfCrosshairGapRadius[playerID]
		);
		const float thickness = 0.125f * radius;
		const auto center = ToVec3(screenTorsoPos);
		float gapDelta = 0.0f;
		float rotationRatio = shouldFaceTarget ? 1.0f : 0.0f;
		// Four prongs ('+' when not facing the target, 'X' otherwise).
		float rotAng1{ PI / 2.0f };
		float rotAng2{ 0.0f };
		float rotAng3{ -PI / 2.0f };
		float rotAng4{ PI };
		if (shouldFaceTarget)
		{
			rotAng1 = { 3.0f * PI / 4.0f };
			rotAng2 = { PI / 4.0f };
			rotAng3 = { -PI / 4.0f };
			rotAng4 = { 5.0f * PI / 4.0f };
		}
		
		// Animate for better visibility.
		if ((aimCorrectionIndicatorOscillationData->interpToMax &&
			aimCorrectionIndicatorOscillationData->value != 1.0f) ||
			(aimCorrectionIndicatorOscillationData->interpToMin && 
			aimCorrectionIndicatorOscillationData->value != 0.0f))
		{
			aimCorrectionIndicatorOscillationData->UpdateInterpolatedValue
			(
				aimCorrectionIndicatorOscillationData->directionChangeFlag
			);
		}
		else
		{
			aimCorrectionIndicatorOscillationData->UpdateInterpolatedValue
			(
				!aimCorrectionIndicatorOscillationData->directionChangeFlag
			);
		}

		gapDelta = (aimCorrectionIndicatorOscillationData->value * radius);
		aimCorrectionIndicatorRotationData->UpdateInterpolatedValue(shouldFaceTarget);

		rotationRatio = aimCorrectionIndicatorRotationData->value;
		rotAng1 = 
		{
			Util::InterpolateSmootherStep(PI / 2.0f, 3.0f * PI / 4.0f, rotationRatio)
		};
		rotAng2 = 
		{
			Util::InterpolateSmootherStep(0.0f, PI / 4.0f, rotationRatio)
		};
		rotAng3 = 
		{
			Util::InterpolateSmootherStep(-PI / 2.0f, -PI / 4.0f, rotationRatio)
		};
		rotAng4 = 
		{
			Util::InterpolateSmootherStep(PI, 5.0f * PI / 4.0f, rotationRatio)
		};

		// Retract arrows when not facing target.
		radius *= rotationRatio;
		if (radius != 0.0f)
		{
			// Outer.
			auto newCenter = center + gapDelta * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				thickness * 2.0f,
				thickness * 4.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				thickness * 2.0f,
				thickness * 4.0f,
				0.0f
			);
			newCenter = 
			(
				center + gapDelta * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f)
			);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				thickness * 2.0f,
				thickness * 4.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
				thickness * 2.0f,
				thickness * 4.0f,
				0.0f
			);

			// Middle.
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				Settings::vuOverlayRGBAValues[p->playerID],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				Settings::vuOverlayRGBAValues[p->playerID],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				Settings::vuOverlayRGBAValues[p->playerID],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				Settings::vuOverlayRGBAValues[p->playerID],
				thickness * 1.5f,
				thickness * 2.0f,
				0.0f
			);

			// Inner.
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 
				0.75f * radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				thickness,
				thickness,
				0.0f
			);
			newCenter = center + gapDelta * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
				thickness,
				thickness,
				0.0f
			);
		}

		// Fewer segments to draw when the gap is small (no readily apparent loss in quality).
		uint32_t numSegments = std::clamp(static_cast<int>(gapDelta * 3), 8, 48);
		DebugAPI::QueueCircle2D
		(
			center,
			Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID],
			numSegments,
			2.0f * thickness + gapDelta,
			thickness,
			0.0f
		);
		DebugAPI::QueueCircle2D
		(
			center,
			Settings::vuOverlayRGBAValues[p->playerID],
			numSegments,
			thickness + gapDelta,
			thickness,
			0.0f
		);
		DebugAPI::QueueCircle2D
		(
			center,
			Settings::vuCrosshairInnerOutlineRGBAValues[p->playerID],
			numSegments,
			gapDelta,
			thickness,
			0.0f
		);
	}

	void TargetingManager::DrawAimPitchIndicator()
	{
		// Draw the player's aim pitch adjustment indicator
		// when the player is adjusting their aim pitch 
		// or for a short time after the player's aim pitch resets.

		if (!Settings::vbEnableAimPitchIndicator[playerID])
		{
			return;
		}

		const auto& paParam =
		(
			p->pam->paParamsList[!InputAction::kResetAim - !InputAction::kFirstAction]
		);
		bool shouldShowIndicator = 
		(
			(p->pam->IsPerforming(InputAction::kAdjustAimPitch)) ||
			(
				(
					paParam.triggerFlags.all(TriggerFlag::kMinHoldTime) &&
					p->pam->GetSecondsSinceLastStart(InputAction::kResetAim) < 0.25f
				) ||
				(
					paParam.triggerFlags.none(TriggerFlag::kMinHoldTime) && 
					p->pam->GetSecondsSinceLastStop(InputAction::kResetAim) < 0.25f
				)
			)
		);
		aimPitchIndicatorFadeInterpData->UpdateInterpolatedValue
		(
			baseCanDrawOverlayElements && shouldShowIndicator
		);
		if (!shouldShowIndicator && 
			!aimPitchIndicatorFadeInterpData->interpToMax && 
			!aimPitchIndicatorFadeInterpData->interpToMin)
		{
			return;
		}

		// Max of:
		// 1/4 the height of the arrow head.
		// 2 * last point's Y coordinate - (origin) first point's Y coordinate.
		const float& thickness = max
		(
			2.0f,
			0.25f * 
			fabsf
			(
				GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS
				[GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS.size() - 1].y -
				GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS[0].y
			)
		);
		RE::NiPoint3 eyePos = Util::GetEyePosition(coopActor.get());
		RE::NiPoint3 eyePosScreenPoint = Util::WorldToScreenPoint3(eyePos);
		RE::NiPoint3 arrowHeadScreenPoint = Util::WorldToScreenPoint3(p->mm->aimPitchPos);
		if (rmm->isGrabbing)
		{
			RE::NiPoint3 deltaPos = (p->mm->aimPitchPos - eyePos);
			deltaPos.Unitize();
			// Add the grabbed refr offset to better show where grabbed objects will be suspended.
			float grabSuspensionOffset = max
			(
				0.0f, (p->mm->aimPitchPos - eyePos).Length() + grabbedRefrDistanceOffset
			);
			if (grabSuspensionOffset == 0.0f)
			{
				deltaPos = RE::NiPoint3();
			}

			arrowHeadScreenPoint = Util::WorldToScreenPoint3
			(
				eyePos + 
				deltaPos * 
				grabSuspensionOffset
			);
		}

		// Base of the arrow.
		const glm::vec2 eyeOrigin = glm::vec2(eyePosScreenPoint.x, eyePosScreenPoint.y);
		// Tip of the arrow.
		const glm::vec2 arrowHeadOrigin = glm::vec2
		(
			arrowHeadScreenPoint.x, arrowHeadScreenPoint.y
		);

		// All three shapes' offsets.
		auto outerShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_OUTER_PIXEL_OFFSETS;
		auto middleShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_MID_PIXEL_OFFSETS;
		auto innerShapeOffsets = GlobalCoopData::AIM_PITCH_INDICATOR_HEAD_INNER_PIXEL_OFFSETS;

		// Get the angle (in screenspace) by which to rotate the shape.
		// If length is 0, direct the arrow head straight up on screen.
		const glm::vec2 arrowHeadScreenDir = glm::normalize(arrowHeadOrigin - eyeOrigin);
		float angToRotate = 
		(
			glm::length(arrowHeadOrigin - eyeOrigin) == 0.0f ?
			PI / 2.0f:
			-atan2f(arrowHeadScreenDir.y, arrowHeadScreenDir.x)
		);

		// Rotate to point in the player's facing direction.
		DebugAPI::RotateOffsetPoints2D(outerShapeOffsets, angToRotate);
		DebugAPI::RotateOffsetPoints2D(middleShapeOffsets, angToRotate);
		DebugAPI::RotateOffsetPoints2D(innerShapeOffsets, angToRotate);

		// Draw each shape and their outlines.
		// Line portion of the arrow.
		uint8_t alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value *
			static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueLine2D
		(
			eyeOrigin, 
			arrowHeadOrigin, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			3.0f * thickness
		);
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueLine2D
		(
			eyeOrigin, 
			arrowHeadOrigin, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			thickness
		);
			
		// Outer shape.
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			outerShapeOffsets, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);

		// Middle shape.
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			middleShapeOffsets, 
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);

		// Inner shape.
		alpha = static_cast<uint8_t>
		(
			aimPitchIndicatorFadeInterpData->value * 
			static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
		);
		DebugAPI::QueueShape2D
		(
			arrowHeadOrigin, 
			innerShapeOffsets, 
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			true, 
			thickness
		);
	}

	void TargetingManager::DrawCrosshair()
	{
		// Draw crosshair lines and outlines based on 
		// if the crosshair is targeting a valid reference.

		// Update fade value if inactive crosshair fading is enabled.
		if (Settings::vbFadeInactiveCrosshair[playerID])
		{
			float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
			// Can fade in if on a target, moving, facing a target,
			// or if not in the process of being re-centered while inactive.
			// Allow 1x inactive interval while static + 0.5x an inactive interval
			// while auto-recentering to elapse before fading out.
			crosshairFadeInterpData->UpdateInterpolatedValue
			(
				baseCanDrawOverlayElements && crosshairActive
			);
		}
		else
		{
			// Fade in/out when in crosshair/twin-stick mode while the auto-fade setting is off.
			crosshairFadeInterpData->UpdateInterpolatedValue(aimMode == AimMode::kCrosshair);
		}

		if (Settings::vuCrosshairStyle[playerID] == !CrosshairStyle::kRing)
		{
			// Draw ring-shaped aim correction crosshair.
			DrawRingShapedCrosshair();
		}
		else if (Settings::vuCrosshairStyle[playerID] == !CrosshairStyle::kRetro)
		{
			// Draw retro-styled crosshair.
			DrawRetroStyleCrosshair();
		}
		else
		{
			// Draw a Skyrim-style pronged crosshair.
			DrawSkyrimStyleCrosshair
			(
				Settings::vuCrosshairStyle[playerID] == !CrosshairStyle::kSkyrimStyleInverted
			);
		}
	}

	void TargetingManager::DrawCrosshairLines()
	{
		// Draw the main four lines of the crosshair using the player's assigned crosshair color 
		// and size params.

		const bool shouldRotate = Util::HandleIsValid(crosshairRefrHandle);
		float angToRotate = shouldRotate ? PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate the mode change rotation and contraction/expansion if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}
		
		bool selectedCamLockOnTarget = 
		(
			Util::HandleIsValid(glob.cam->camLockOnTargetHandle) &&
			crosshairRefrHandle == glob.cam->camLockOnTargetHandle
		);
		// Scale the lengt and base gap but not the gap delta to allow for 
		// unmodified contraction/expansion.
		const float crosshairLength = 
		(
			crosshairSizeRatioInterpData->value * Settings::vfCrosshairLength[playerID]
		);
		const float crosshairGap = 
		(
			crosshairSizeRatioInterpData->value * Settings::vfCrosshairGapRadius[playerID] +
			gapDelta
		);
		// Thickness is not auto-scaled.
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];
		// Draw crosshair lines.
		// '+' shape when not facing a target, 'X' shape otherwise.
		// Pairs of 2D line start and end points.

		std::pair<glm::vec2, glm::vec2> crosshairUp = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x, crosshairScaleformPos.y + crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x, crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairDown = 
		{
			glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap),
			glm::vec2
			(
				crosshairScaleformPos.x, crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairLeft = 
		{
			glm::vec2(crosshairScaleformPos.x - crosshairGap, crosshairScaleformPos.y),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength, crosshairScaleformPos.y
			)
		};
		std::pair<glm::vec2, glm::vec2> crosshairRight = 
		{
			glm::vec2(crosshairScaleformPos.x + crosshairGap, crosshairScaleformPos.y),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength, crosshairScaleformPos.y
			)
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
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			)
		);
		// Up.
		DebugAPI::QueueLine2D
		(
			crosshairUp.first, 
			crosshairUp.second, 
			selectedCamLockOnTarget ? 
			0x000000FF + alpha :
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Down.
		DebugAPI::QueueLine2D
		(
			crosshairDown.first,
			crosshairDown.second, 
			selectedCamLockOnTarget ? 
			0x000000FF + alpha :
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Left.
		DebugAPI::QueueLine2D
		(
			crosshairLeft.first,
			crosshairLeft.second, 
			selectedCamLockOnTarget ? 
			0x000000FF + alpha :
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
			crosshairThickness
		);
		// Right.
		DebugAPI::QueueLine2D
		(
			crosshairRight.first,
			crosshairRight.second, 
			selectedCamLockOnTarget ? 
			0x000000FF + alpha :
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
			crosshairThickness
		);

		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 25.0f))
		{
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				selectedCamLockOnTarget ? 
				0xFFFFFF00 + alpha :
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				64, 
				2.0f * crosshairThickness + crosshairGap + crosshairLength, 
				2.0f * crosshairThickness
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				selectedCamLockOnTarget ? 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha :
				0xFFFFFF00 + alpha, 
				64,
				4.0f * crosshairThickness + crosshairGap + crosshairLength,
				2.0f * crosshairThickness
			);
		}
	}

	void TargetingManager::DrawCrosshairOutline
	(
		float&& a_outlineIndex, const uint32_t& a_outlineRGBA
	)
	{
		// Always outline the four main crosshair lines 
		// and also outline that inner outline with another outline
		// when a valid object is selected by the player's crosshair.
		// Outline index is a whole number value that indicates 
		// the multiple of crosshair thicknesses
		// from the center four lines at which to draw the outline.
		// The higher the index, the further from the crosshair base lines
		// the outline will be drawn.

		const bool shouldRotate = Util::HandleIsValid(crosshairRefrHandle);
		float angToRotate = shouldRotate ? PI / 4.0f : 0.0f;
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
		// Scale the length and base gap but not the gap delta to allow for 
		// unmodified contraction/expansion.
		float crosshairLength = 
		(
			crosshairSizeRatioInterpData->value * Settings::vfCrosshairLength[playerID]
		);
		// Thickness is not auto-scaled.
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];
		// Longer than crosshair body lines. 
		// Since the line thickness does not trace around the caps of the line,
		// we have to extend the line a bit to achieve the same 'thickness' trace effect.
		crosshairLength += crosshairThickness * a_outlineIndex;
		// Apply scale mult to base gap, subtract half a thickness to properly trace around 
		// the 'start' cap of the inner prong line, and then add the unscaled gap delta.
		const float crosshairGap = 
		(
			crosshairSizeRatioInterpData->value * 
			Settings::vfCrosshairGapRadius[playerID] -
			0.5f * crosshairThickness * a_outlineIndex + 
			gapDelta
		);

		// Pairs of 2D line start and end coordinates.

		// Up outline endpoints.
		std::pair<glm::vec2, glm::vec2> up = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x, 
				crosshairScaleformPos.y + crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x,
				crosshairScaleformPos.y + crosshairGap + crosshairLength
			)
		};

		// Down outline endpoints.
		std::pair<glm::vec2, glm::vec2> down = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x,
				crosshairScaleformPos.y - crosshairGap
			),
			glm::vec2
			(
				crosshairScaleformPos.x, 
				crosshairScaleformPos.y - crosshairGap - crosshairLength
			)
		};

		// Left outline endpoints.
		std::pair<glm::vec2, glm::vec2> left = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap,
				crosshairScaleformPos.y
			),
			glm::vec2
			(
				crosshairScaleformPos.x - crosshairGap - crosshairLength,
				crosshairScaleformPos.y
			)
		};

		// Right outline endpoints.
		std::pair<glm::vec2, glm::vec2> right = 
		{
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap, 
				crosshairScaleformPos.y
			),
			glm::vec2
			(
				crosshairScaleformPos.x + crosshairGap + crosshairLength, 
				crosshairScaleformPos.y
			)
		};

		// Rotate if facing crosshair target.
		if (angToRotate != 0.0f)
		{
			DebugAPI::RotateLine2D(up, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(down, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(left, crosshairScaleformPos, angToRotate);
			DebugAPI::RotateLine2D(right, crosshairScaleformPos, angToRotate);
		}

		// Use interped fade value if enabled; otherwise, use the player's static fade value.
		uint8_t alpha = 
		(
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * static_cast<float>(a_outlineRGBA & 0xFF)
			)
		);
		DebugAPI::QueueLine2D
		(
			up.first, 
			up.second,
			(a_outlineRGBA & 0xFFFFFF00) + alpha, 
			crosshairThickness * (a_outlineIndex + 1.0f)
		);
		DebugAPI::QueueLine2D
		(
			down.first, 
			down.second, 
			(a_outlineRGBA & 0xFFFFFF00) + alpha,
			crosshairThickness * (a_outlineIndex + 1.0f)
		);
		DebugAPI::QueueLine2D
		(
			left.first, 
			left.second,
			(a_outlineRGBA & 0xFFFFFF00) + alpha, 
			crosshairThickness * (a_outlineIndex + 1.0f)
		);
		DebugAPI::QueueLine2D
		(
			right.first,
			right.second, 
			(a_outlineRGBA & 0xFFFFFF00) + alpha, 
			crosshairThickness * (a_outlineIndex + 1.0f)
		);
	}
	
	void TargetingManager::DrawPlayerIndicator()
	{
		// Draw a quest marker above the player's head 
		// when player-specific visibility conditions are met.

		const auto& visibilityType = Settings::vuPlayerIndicatorVisibilityType[playerID];
		// If the indicator is disabled, no need to draw. Bye.
		if (visibilityType == !PlayerIndicatorVisibilityType::kDisabled) 
		{
			return;
		}
		
		// Do not draw when special dialogue camera state is active and zoomed in.
		if (glob.menuPID == playerID &&
			glob.cam->IsRunning() &&
			glob.cam->inDialogueCamState && 
			Settings::bDialogueCamEnabled &&
			!glob.cam->adjustedAfterReachingDialoguePos)
		{
			return;
		}

		// Draw the player indicator only if an actor info bar is not drawn for this player.
		bool hasTrueHUDInfoBar = false;
		// TODO:
		// PR for TrueHUD to allow for continuous display of actor info/boss bars 
		// for players even when not in combat.
		// Draw the TrueHUD actor info bar while in combat and draw a boss bar for the player 
		// when not in combat and if the player's HMS AVs have changed.
		// Commented out for now.
		// TrueHUD API to request addition/removal of actor info or boss bars for this player.
		/*
		auto trueHUDAPI3 = ALYSLC::TrueHUDCompat::g_trueHUDAPI3; 
		if (trueHUDAPI3)
		{
			const bool atFullHMS = 
			(
				p->pam->currentHealth == p->pam->fullHealth &&
				p->pam->currentMagicka == p->pam->fullMagicka &&
				p->pam->currentStamina == p->pam->fullStamina
			);
			float secsSinceHMSFullyRestored = Util::GetElapsedSeconds(p->lastHMSFullRestorationTP);
			const float secsBeforeHidingBar = 
			(
				Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
			);
			const auto handle = coopActor->GetHandle();
			const bool hasInfoBar = trueHUDAPI3->HasInfoBar(handle, true);
			const bool hasBossBar = !hasInfoBar && trueHUDAPI3->HasInfoBar(handle);
			// Show the info/boss bar when in/out of combat or if downed
			// and if the player's HMS AVs are not at their full values 
			// for a certain amount of time.
			if (glob.isInCoopCombat || p->isDowned)
			{
				if (hasBossBar)
				{
					trueHUDAPI3->RemoveBoss
					(
						handle, TRUEHUD_API::WidgetRemovalMode::Normal
					);
				}
					
				if ((!hasInfoBar) && 
					(!atFullHMS || secsSinceHMSFullyRestored < secsBeforeHidingBar))
				{
					trueHUDAPI3->AddActorInfoBar(handle);
				}
				else if ((hasInfoBar) && 
						 (atFullHMS && secsSinceHMSFullyRestored >= secsBeforeHidingBar))
				{
					trueHUDAPI3->RemoveActorInfoBar
					(
						handle, TRUEHUD_API::WidgetRemovalMode::Normal
					);
				}
			}
			else
			{
				if (hasInfoBar)
				{
					trueHUDAPI3->RemoveActorInfoBar
					(
						handle, TRUEHUD_API::WidgetRemovalMode::Normal
					);
				}

				if ((!hasBossBar) &&
					(!atFullHMS || secsSinceHMSFullyRestored < secsBeforeHidingBar))
				{
					trueHUDAPI3->AddBoss(handle);
				}
				else if ((hasBossBar) &&
						 (atFullHMS && secsSinceHMSFullyRestored >= secsBeforeHidingBar))
				{
					trueHUDAPI3->RemoveBoss
					(
						handle, TRUEHUD_API::WidgetRemovalMode::Normal
					);
				}
			}

			hasTrueHUDInfoBar = trueHUDAPI3->HasInfoBar(handle, true);
		}
		*/

		// If the player is not on screen, 
		// draw the player indicator pointed at the player's position.
		// If on screen, draw player indicator above their head when:
		// - 'Always draw' setting is set -OR-
		// - There is no LOS to them -OR-
		// - Their pixel height is below a certain threshold.

		// Get viewport dimensions for the overlay menu.
		const auto port = Util::GetPort();
		// Check that the player's center is visible.
		RE::NiPoint3 posScreenCoords{ };
		bool onScreen = Util::PointIsOnScreen(p->mm->playerTorsoPosition, posScreenCoords);
		// Ensure two outlines can fit inside outermost outline.
		// Scale with player's pixel height and bound above and below.
		float indicatorBaseLength = Settings::vfPlayerIndicatorLength[playerID];
		const float& indicatorBaseThickness = Settings::vfPlayerIndicatorThickness[playerID];
		float playerPixelHeight = Util::GetBoundPixelDist(coopActor.get(), true);
		playerPixelHeight = playerPixelHeight == 0.0f ? indicatorBaseLength : playerPixelHeight;
		// Lower/upper bound are the smaller/larger of:
		// 4 indicator thicknesses or player pixel height scaled down by a factor.
		indicatorBaseLength = std::clamp
		(
			indicatorBaseLength,
			min
			(
				max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), 
				playerPixelHeight / 4.0f
			),
			max
			(
				max(4.0f * indicatorBaseThickness, indicatorBaseLength / 2.0f), 
				playerPixelHeight / 4.0f
			)
		);
		// Scaling factor used to scale up/down hardcoded point offsets for the indicator's shape.
		float scalingFactor = indicatorBaseLength / GlobalCoopData::PLAYER_INDICATOR_DEF_LENGTH;
		float indicatorLength = indicatorBaseLength * scalingFactor;
		float indicatorThickness = indicatorBaseThickness * scalingFactor;
		if (onScreen)
		{
			auto playerCam = RE::PlayerCamera::GetSingleton();
			bool shouldDraw = 
			(
				(!hasTrueHUDInfoBar) &&
				(baseCanDrawOverlayElements && playerCam && playerCam->cameraRoot)
			);
			if (shouldDraw) 
			{
				if (shouldDraw)
				{
					bool falseRef = false;
					const auto& camPos = 
					(
						glob.cam->IsRunning() ? 
						glob.cam->camTargetPos : 
						playerCam->cameraRoot->world.translate
					);
					const auto& playerTorsoPos = p->mm->playerTorsoPosition;
					// Condition for raycasting to check for LOS from cam to player:
					// 1. Low visibility mode is set,
					// 2. Not downed.
					// 3. The player's height is more than 1/10 of the screen's height.
					// 4. The player's torso is on screen.
					// Otherwise, no raycast is needed and the indicator will be drawn.
					bool shouldRaycastForLOS = 
					{ 
						visibilityType == !PlayerIndicatorVisibilityType::kLowVisibility && 
						!p->isDowned &&						 
						Util::GetBoundPixelDist(coopActor.get(), true) >= 
						DebugAPI::screenResY / 10.0f &&
						Util::PointIsOnScreen(playerTorsoPos) 
					};
					// If checking raycast LOS, and the player is not visible, draw the indicator.
					bool hasLOS = true;
					if (shouldRaycastForLOS)
					{
						// From cam to player torso.
						// If there is LOS, the cast will either hit nothing or the player.
						auto result = Raycast::hkpCastRay
						(
							{ camPos.x, camPos.y, camPos.z, 0.0f }, 
							{ playerTorsoPos.x, playerTorsoPos.y, playerTorsoPos.z, 0.0f }, 
							std::vector<RE::NiAVObject*>({ playerCam->cameraRoot.get() }), 
							RE::COL_LAYER::kLOS
						);
						auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
						// No hit or if the player is hit means that there is LOS on the player.
						hasLOS = 
						(
							(!result.hitObjectPtr) || 
							(hitRefrPtr && hitRefrPtr.get() == coopActor.get())
						);
					}

					shouldDraw = !shouldRaycastForLOS || !hasLOS;
				}
			}

			// Update fade value before fading in/out.
			// Will continue fading in/out until done even if the draw condition is not met.
			playerIndicatorFadeInterpData->UpdateInterpolatedValue(shouldDraw);
			if (shouldDraw || 
				playerIndicatorFadeInterpData->interpToMax || 
				playerIndicatorFadeInterpData->interpToMin) 
			{
				// Point facing downward above the player's head.
				RE::NiPoint3 topOfTheHeadPos = Util::GetHeadPosition(coopActor.get());
				// Based on the head body part's radius.
				auto headRadius = Util::GetHeadRadius(coopActor.get());
				topOfTheHeadPos.z += headRadius + 15.0f;
				posScreenCoords = Util::WorldToScreenPoint3(topOfTheHeadPos);
				// Origin and lower/upper shape offsets from this origin.
				playerIndicatorScaleformPos = 
				{
					std::clamp
					(
						posScreenCoords.x, 
						port.left + indicatorThickness, 
						port.right - indicatorThickness
					),
					std::clamp
					(
						posScreenCoords.y, 
						port.top + indicatorThickness,
						port.bottom - indicatorThickness
					)
				};

				auto upperPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS;
				auto lowerPortionOffsets = GlobalCoopData::PLAYER_INDICATOR_LOWER_PIXEL_OFFSETS;
				playerIndicatorHeight = fabsf
				(
					scalingFactor * GlobalCoopData::PLAYER_INDICATOR_UPPER_PIXEL_OFFSETS[0].y
				);

				// After calculating the new position,
				// skip drawing the indicator if there is a focal player.
				if (glob.cam->IsRunning() && glob.cam->focalPlayerPID != -1)
				{
					return;
				}

				// Scale offsets again.
				for (auto& offset : upperPortionOffsets)
				{
					offset *= scalingFactor;
				}

				for (auto& offset : lowerPortionOffsets)
				{
					offset *= scalingFactor;
				}

				// Draw each shape and their outlines.
				uint8_t alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value *
					static_cast<float>
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
					)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos,
					upperPortionOffsets,
					(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false,
					1.5f * indicatorThickness
				);

				// Lower portion.
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
					)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos, 
					lowerPortionOffsets, 
					(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false, 
					1.5f * indicatorThickness
				);
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos, 
					lowerPortionOffsets, 
					(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false, 
					indicatorThickness
				);
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos, 
					lowerPortionOffsets, 
					(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
				);

				// Upper portion.
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value *
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos,
					upperPortionOffsets,
					(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
					false,
					indicatorThickness
				);
				alpha = static_cast<uint8_t>
				(
					playerIndicatorFadeInterpData->value *
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				);
				DebugAPI::QueueShape2D
				(
					playerIndicatorScaleformPos,
					upperPortionOffsets, 
					(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
				);
			}
		}
		else
		{
			// Skip drawing the indicator if there is a focal player.
			if (glob.cam->IsRunning() && glob.cam->focalPlayerPID != -1)
			{
				return;
			}

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

			// Origin with respect to which all the above offset points 
			// are traced out when drawing the shape.
			glm::vec2 origin
			{
				std::clamp
				(
					posScreenCoords.x,
					port.left + indicatorThickness, 
					port.right - indicatorThickness
				),
				std::clamp
				(
					posScreenCoords.y, 
					port.top + indicatorThickness, 
					port.bottom - indicatorThickness
				)
			};

			// Rotate and draw lower portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(lowerPortionOffsets, indicatorRotRads);
			// Rotate and draw upper portion of the indicator + its outline.
			DebugAPI::RotateOffsetPoints2D(upperPortionOffsets, indicatorRotRads);
			uint8_t alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				upperPortionOffsets, 
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				false, 
				1.5f * indicatorThickness
			);

			// Lower.
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				lowerPortionOffsets, 
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				false, 
				1.5f * indicatorThickness
			);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				lowerPortionOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha, 
				false, 
				indicatorThickness
			);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				lowerPortionOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);

			// Upper.
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				upperPortionOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				false, 
				indicatorThickness
			);
			alpha = static_cast<uint8_t>
			(
				playerIndicatorFadeInterpData->value * 
				static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				upperPortionOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);
		}
	}

	void TargetingManager::DrawRetroStyleCrosshair()
	{
		// Draw a crosshair with four basic rectangular prongs.
		
		// Draw a retro-style crosshair with four lines for prongs.
		// First, outer outline.
		DrawCrosshairOutline
		(
			2.0f, Settings::vuCrosshairOuterOutlineRGBAValues[playerID]
		);
		// Second, inner outline.
		DrawCrosshairOutline
		(
			1.0f, Settings::vuCrosshairInnerOutlineRGBAValues[playerID]
		);
		// Draw prong lines last.
		DrawCrosshairLines();
	}

	void TargetingManager::DrawRingShapedCrosshair()
	{
		// Draw a crosshair that consists of concentric rings with 4 protruding arrows.
		// Similar in appearance to the aim correction indicator.
		
		const bool shouldRotate = Util::HandleIsValid(crosshairRefrHandle);
		float angToRotate = shouldRotate ? PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate the rotation, contraction, and expansion, if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairSizeRatioInterpData->value * crosshairOscillationData->current;
		}

		// Center at the crosshair position.
		const auto center = glm::vec3(crosshairScaleformPos.x, crosshairScaleformPos.y, 0.0f);
		const float& crosshairLength = 
		(
			crosshairSizeRatioInterpData->value * Settings::vfCrosshairLength[playerID]
		);
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];

		float rotationRatio = (crosshairRotationData->current) / (PI / 4.0f);
		// Four prongs ('+' when not facing the target, 'X' otherwise).
		float rotAng1{ PI / 2.0f };
		float rotAng2{ 0.0f };
		float rotAng3{ -PI / 2.0f };
		float rotAng4{ PI };
		if (shouldRotate)
		{
			rotAng1 = { 3.0f * PI / 4.0f };
			rotAng2 = { PI / 4.0f };
			rotAng3 = { -PI / 4.0f };
			rotAng4 = { 5.0f * PI / 4.0f };
		}
		
		rotAng1 = 
		{
			Util::InterpolateSmootherStep(PI / 2.0f, 3.0f * PI / 4.0f, rotationRatio)
		};
		rotAng2 = 
		{
			Util::InterpolateSmootherStep(0.0f, PI / 4.0f, rotationRatio)
		};
		rotAng3 = 
		{
			Util::InterpolateSmootherStep(-PI / 2.0f, -PI / 4.0f, rotationRatio)
		};
		rotAng4 = 
		{
			Util::InterpolateSmootherStep(PI, 5.0f * PI / 4.0f, rotationRatio)
		};

		// Retract arrows when not on a target.
		float radius = crosshairLength * rotationRatio;
		if (radius != 0.0f)
		{
			auto arrowStartOffset = gapDelta + 3.0f * crosshairThickness;
			// Outer.
			uint8_t alpha = 
			(
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
					)
				)
			);
			auto newCenter = 
			(
				center + arrowStartOffset * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f)
			);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.5f,
				crosshairThickness * 3.0f,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.5f,
				crosshairThickness * 3.0f,
				0.0f
			);
			newCenter = 
			(
				center + arrowStartOffset * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f)
			);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.5f,
				crosshairThickness * 3.0f,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.5f,
				crosshairThickness * 3.0f,
				0.0f
			);

			// Middle.
			alpha = 
			(
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuOverlayRGBAValues[playerID] & 0xFF
					)
				)
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.25f,
				crosshairThickness * 2.0f,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.25f,
				crosshairThickness * 2.0f,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.25f,
				crosshairThickness * 2.0f,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness * 1.25f,
				crosshairThickness * 2.0f,
				0.0f
			);

			// Inner.
			alpha = 
			(
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				)
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 
				0.75f * radius * glm::vec3(cosf(rotAng1), sinf(rotAng1), 0.0f),
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness,
				crosshairThickness,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng2), sinf(rotAng2), 0.0f),
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness,
				crosshairThickness,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng3), sinf(rotAng3), 0.0f),
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness,
				crosshairThickness,
				0.0f
			);
			newCenter = center + arrowStartOffset * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f);
			DebugAPI::QueueArrow2D
			(
				newCenter,
				newCenter + 0.75f * radius * glm::vec3(cosf(rotAng4), sinf(rotAng4), 0.0f),
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				crosshairThickness,
				crosshairThickness,
				0.0f
			);
		}

		// Fewer segments to draw when the gap is small (no readily apparent loss in quality).
		uint32_t numSegments = std::clamp(static_cast<int>(gapDelta * 3), 8, 48);
		uint8_t alpha = 
		(
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * 
				static_cast<float>
				(
					Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
				)
			)
		);
		DebugAPI::QueueCircle2D
		(
			center,
			(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
			numSegments,
			2.0f * crosshairThickness + gapDelta,
			crosshairThickness,
			0.0f
		);
		alpha = 
		(
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * 
				static_cast<float>
				(
					Settings::vuOverlayRGBAValues[playerID] & 0xFF
				)
			)
		);
		DebugAPI::QueueCircle2D
		(
			center,
			(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
			numSegments,
			crosshairThickness + gapDelta,
			crosshairThickness,
			0.0f
		);
		alpha = 
		(
			static_cast<uint8_t>
			(
				crosshairFadeInterpData->value * 
				static_cast<float>
				(
					Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
				)
			)
		);
		DebugAPI::QueueCircle2D
		(
			center,
			(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
			numSegments,
			gapDelta,
			crosshairThickness,
			0.0f
		);

		alpha = 0xFF;
		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 25.0f))
		{
			alpha = 
			(
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				)
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				64, 
				crosshairSizeRatioInterpData->value * 
				(
					2.0f * 
					crosshairThickness + 
					crosshairLength + 
					Settings::vfCrosshairGapRadius[playerID]
				) + gapDelta,
				2.0f * crosshairThickness
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				0xFFFFFF00 + alpha,
				64, 
				crosshairSizeRatioInterpData->value * 
				(
					4.0f * 
					crosshairThickness + 
					crosshairLength + 
					Settings::vfCrosshairGapRadius[playerID]
				) + gapDelta,
				2.0f * crosshairThickness
			);
		}
	}

	void TargetingManager::DrawSkyrimStyleCrosshair(bool a_shouldInvert)
	{
		// Draw a Skyrim-style crosshair with a player-specific colorway.

		const bool shouldRotate = Util::HandleIsValid(crosshairRefrHandle);
		float angToRotate = shouldRotate ? PI / 4.0f : 0.0f;
		float gapDelta = 0.0f;
		// Animate rotation, contraction, and expansion, if enabled.
		if (Settings::vbAnimatedCrosshair[playerID])
		{
			UpdateAnimatedCrosshairInterpData();
			angToRotate = crosshairRotationData->current;
			gapDelta = crosshairOscillationData->current;
		}

		// Center at the crosshair position.
		const auto origin = glm::vec2(crosshairScaleformPos.x, crosshairScaleformPos.y);
		const float& crosshairLength = Settings::vfCrosshairLength[playerID];
		const float& crosshairThickness = Settings::vfCrosshairThickness[playerID];

		// Points are offset to the right of the origin (+X Scaleform axis).
		std::vector<glm::vec2> defProngOffsets = GlobalCoopData::CROSSHAIR_PRONG_PIXEL_OFFSETS;
		std::vector<glm::vec2> baseProngOffsets = defProngOffsets;
		std::vector<glm::vec2> prongOffsets = defProngOffsets;
		std::vector<glm::vec2> prongRotatedOffsets = defProngOffsets;
		// Factor with which to scale the shape offset points and shape dimensions
		// relative to the default prong length.
		// Match the player's chosen crosshair length.
		float scalingFactor = crosshairLength / GlobalCoopData::CROSSHAIR_PRONG_DEF_LENGTH;
		const float defaultLength = 
		(
			GlobalCoopData::CROSSHAIR_PRONG_PIXEL_OFFSETS[3].x -
			GlobalCoopData::CROSSHAIR_PRONG_PIXEL_OFFSETS[0].x
		);
		for (auto& coord : baseProngOffsets)
		{
			// Invert along long axis for a spicier look.
			if (Settings::vuCrosshairStyle[playerID] == !CrosshairStyle::kSkyrimStyleInverted)
			{
				coord.x = defaultLength - coord.x;
			}

			coord *= scalingFactor;
		}

		// Draw outer, then inner outline of the prong, then the prong itself.
		uint8_t alpha = 0xFF;
		// [Outer outline]
		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += Settings::vfCrosshairGapRadius[playerID];
			// Scale the base gap but not the gap delta to allow for 
			// unmodified contraction/expansion.
			coord *= crosshairSizeRatioInterpData->value;
			coord.x += gapDelta;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
			// Don't rotate the first prong.
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
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFF
					)
				)
			);
			DebugAPI::QueueShape2D
			(
				origin,
				prongRotatedOffsets, 
				(Settings::vuCrosshairOuterOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				false, 
				2.0f * crosshairThickness
			);
		}

		// [Inner outline]
		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += Settings::vfCrosshairGapRadius[playerID];
			// Scale the base gap but not the gap delta to allow for 
			// unmodified contraction/expansion.
			coord *= crosshairSizeRatioInterpData->value;
			coord.x += gapDelta;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
			// Don't rotate the first prong.
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
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>
					(
						Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFF
					)
				)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				prongRotatedOffsets, 
				(Settings::vuCrosshairInnerOutlineRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				false, 
				crosshairThickness
			);
		}

		prongOffsets = baseProngOffsets;
		for (auto& coord : prongOffsets)
		{
			coord.x += Settings::vfCrosshairGapRadius[playerID]; 
			// Scale the base gap but not the gap delta to allow for 
			// unmodified contraction/expansion.
			coord *= crosshairSizeRatioInterpData->value;
			coord.x += gapDelta;
		}

		// Four prongs.
		for (uint8_t i = 0; i < 4; ++i)
		{
			// 90 degrees between each prong.
			// Don't rotate the first prong.
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
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				)
			);
			DebugAPI::QueueShape2D
			(
				origin, 
				prongRotatedOffsets, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha
			);
		}

		// Outline with two circles if near the edge of the screen for better visibility.
		if (!Util::PointIsOnScreen(crosshairWorldPos, DebugAPI::screenResY / 25.0f))
		{
			alpha = 
			(
				static_cast<uint8_t>
				(
					crosshairFadeInterpData->value * 
					static_cast<float>(Settings::vuOverlayRGBAValues[playerID] & 0xFF)
				)
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				(Settings::vuOverlayRGBAValues[playerID] & 0xFFFFFF00) + alpha,
				64, 
				crosshairSizeRatioInterpData->value * 
				(
					2.0f * 
					crosshairThickness + 
					crosshairLength + 
					Settings::vfCrosshairGapRadius[playerID]
				) + gapDelta,
				2.0f * crosshairThickness
			);
			DebugAPI::QueueCircle2D
			(
				crosshairScaleformPos, 
				0xFFFFFF00 + alpha,
				64, 
				crosshairSizeRatioInterpData->value * 
				(
					4.0f * 
					crosshairThickness + 
					crosshairLength + 
					Settings::vfCrosshairGapRadius[playerID]
				) + gapDelta,
				2.0f * crosshairThickness
			);
		}
	}

	void TargetingManager::DrawTrajectories()
	{
		// Draw trajectories for projectiles that the player is attempting to release.

		if (!Settings::vbEnablePredictedProjectileTrajectoryCurves[p->playerID])
		{
			return;
		}

		//=======================
		// [Set Trajectory Type]:
		//=======================

		// Get potential trajectory type first.
		ProjectileTrajType trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[p->playerID]
		);
		auto targetActorHandle = GetRangedTargetActor();
		auto targetActorPtr = Util::GetRefrPtrFromHandle(targetActorHandle);
		bool targetActorValidity = 
		(
			targetActorPtr && Util::IsValidRefrForTargeting(targetActorPtr.get())
		);
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		bool crosshairRefrValidity = 
		(
			crosshairRefrPtr && Util::IsValidRefrForTargeting(crosshairRefrPtr.get())
		);
		// Actor targeted (aim correction or otherwise), 
		// should face crosshair position (never true while mounted), 
		// or mounted and targeting an object.
		bool adjustTowardsTarget = 
		{
			(targetActorPtr != coopActor) && (targetActorValidity || crosshairActive)
		};
		// Aim in the player's facing direction if there is no target.
		if (!adjustTowardsTarget)
		{
			trajType = ProjectileTrajType::kAimDirection;
		}

		//======================
		// [Bows and Crossbows]:
		//======================
		
		const auto& meleeAttackState = coopActor->actorState1.meleeAttackState;
		bool aimingWithRangedWeap = false;
		if (p->em->HasBowEquipped())
		{
			// Avoid the 'kBowDraw' portion of the bow drawing process, 
			// since the arrow is not fully pulled from its quiver.
			aimingWithRangedWeap = 
			(
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowAttached ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
			);

		}
		else if (p->em->HasCrossbowEquipped())
		{
			// Avoid the 'kBowAttached' portion of the crossbow firing process,
			// since this state is set after firing the crossbow and when loading another bolt.
			aimingWithRangedWeap = 
			(
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDraw ||
				meleeAttackState == RE::ATTACK_STATE_ENUM::kBowDrawn
			);
		}

		if (aimingWithRangedWeap)
		{
			auto ammoForm = p->em->equippedForms[!EquipIndex::kAmmo];
			if (ammoForm)
			{
				auto ammo = ammoForm->As<RE::TESAmmo>();
				if (ammo && ammo->data.projectile)
				{
					auto baseProj = ammo->data.projectile;
					// Special case for beam/flame projectiles:
					// aim in the player's facing direction.
					if (baseProj->data.types.any
						(
							RE::BGSProjectileData::Type::kBeam,
							RE::BGSProjectileData::Type::kFlamethrower
						))
					{
						trajType = ProjectileTrajType::kAimDirection;
					}
					const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
					(
						p,
						baseProj,
						p->em->GetRHWeapon(),
						nullptr, 
						p->mm->playerAttackSourcePos,
						trajType
					);
					
					DrawTrajectory
					(
						baseProj,
						RE::ObjectRefHandle(),
						trajInfo->releasePos,
						trajInfo->trajectoryEndPos,
						trajInfo->initialTrajTimeToTarget,
						trajInfo->releaseSpeed,
						trajInfo->launchPitch,
						trajInfo->launchYaw,
						trajInfo->g,
						trajInfo->mu,
						baseProj->data.range,
						trajInfo->trajType,
						trajInfo->canReachTarget,
						true,
						false
					);
				}
			}
		}

		//=========================
		// [Spellcast Projectiles]:
		//=========================

		// Left hand spell/staff spell.
		auto lhWeap = p->em->GetLHWeapon(); 
		RE::MagicItem* lhSpell = nullptr;
		bool usingStaff = false;
		if (lhWeap && lhWeap->IsStaff())
		{
			usingStaff = p->pam->usingLHStaff->value == 1.0f;
			lhSpell = usingStaff ? lhWeap->formEnchanting : nullptr;
		}
		else
		{
			lhSpell = p->em->GetLHSpell();
		}

		auto lhSpellDelivery = 
		(
			lhSpell ? lhSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto lhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kLeftHand);
		// Caster loaded up with a targeted spell and actively casting.
		bool canDrawLHSpellTraj = 
		(
			(
				lhSpell && 
				lhSpell->avEffectSetting && 
				lhSpell->avEffectSetting->data.projectileBase &&
				lhSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
				lhCaster
			) && 
			(
				(usingStaff) || 
				(
					!lhWeap &&
					*lhCaster->state > RE::MagicCaster::State::kUnk01 &&
					*lhCaster->state < RE::MagicCaster::State::kUnk07
				)
			)
		);
		if (canDrawLHSpellTraj)
		{
			auto baseProj = lhSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Release from left hand node, or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			auto magicNodePtr = RE::NiPointer<RE::NiAVObject>(lhCaster->GetMagicNode()); 
			if (magicNodePtr) 
			{
				releasePos = magicNodePtr->world.translate;
			}
			else
			{
				auto leftHandNodePtr = Util::Get3DObjectByName
				(
					coopActor.get(), "NPC L Hand [LHnd]"
				); 
				if (leftHandNodePtr)
				{
					releasePos = leftHandNodePtr->world.translate;
				}
			}

			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				lhSpell->avEffectSetting, 
				releasePos,
				trajType
			);
			DrawTrajectory
			(
				baseProj,
				RE::ObjectRefHandle(),
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		// Right hand spell/staff spell.
		RE::MagicItem* rhSpell = nullptr;
		auto rhWeap = p->em->GetRHWeapon();
		usingStaff = false;
		if (rhWeap && rhWeap->IsStaff())
		{
			usingStaff = p->pam->usingRHStaff->value == 1.0f;
			rhSpell = usingStaff ? rhWeap->formEnchanting : nullptr;
		}
		else
		{
			rhSpell = p->em->GetRHSpell();
		}
		
		auto rhSpellDelivery = 
		(
			rhSpell ? rhSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto rhCaster = coopActor->GetMagicCaster(RE::MagicSystem::CastingSource::kRightHand);
		// Caster loaded up with a targeted spell and actively casting.
		bool canDrawRHSpellTraj = 
		(
			(
				rhSpell && 
				rhSpell->avEffectSetting && 
				rhSpell->avEffectSetting->data.projectileBase &&
				rhSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
				rhCaster
			) && 
			(
				(usingStaff) || 
				(
					!rhWeap && 
					*rhCaster->state > RE::MagicCaster::State::kUnk01 &&
					*rhCaster->state < RE::MagicCaster::State::kUnk07
				)
			)
		);
		if (canDrawRHSpellTraj)
		{
			auto baseProj = rhSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}
			
			// Release from right hand node, or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			auto magicNodePtr = RE::NiPointer<RE::NiAVObject>(rhCaster->GetMagicNode()); 
			if (magicNodePtr) 
			{
				releasePos = magicNodePtr->world.translate;
			}
			else
			{
				auto rightHandNodePtr = Util::Get3DObjectByName
				(
					coopActor.get(), "NPC R Hand [RHnd]"
				); 
				if (rightHandNodePtr)
				{
					releasePos = rightHandNodePtr->world.translate;
				}
			}

			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				rhSpell->avEffectSetting, 
				releasePos,
				trajType
			);
			DrawTrajectory
			(
				baseProj,
				RE::ObjectRefHandle(),
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		// Quick slot spell.
		auto qsSpell = p->em->quickSlotSpell; 
		auto qsSpellDelivery = 
		(
			qsSpell ? qsSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		auto instantCaster = coopActor->magicCasters[RE::Actor::SlotTypes::kPowerOrShout];
		// Instant caster loaded up with a targeted spell and the player is trying to cast.
		bool canDrawQSSpellTraj = 
		(
			qsSpell && 
			qsSpell->avEffectSetting && 
			qsSpell->avEffectSetting->data.projectileBase &&
			qsSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			instantCaster &&
			p->pam->IsPerforming(InputAction::kQuickSlotCast)
		);
		if (canDrawQSSpellTraj)
		{
			auto baseProj = qsSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Release from instant caster position or looking at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (instantCaster->magicNode) 
			{
				releasePos = instantCaster->magicNode->world.translate;
			}
			else
			{
				const auto strings = RE::FixedStrings::GetSingleton();
				if (strings)
				{
					auto headMagicNodePtr = Util::Get3DObjectByName
					(
						coopActor.get(), strings->npcHeadMagicNode
					); 
					if (headMagicNodePtr)
					{
						releasePos = headMagicNodePtr->world.translate;
					}
				}
			}

			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				qsSpell->avEffectSetting, 
				releasePos,
				trajType
			);
			DrawTrajectory
			(
				baseProj,
				RE::ObjectRefHandle(),
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}


		//==========
		// [Shouts]:
		//==========

		// Shout variation or power spell.
		auto voiceSpell = p->em->voiceSpell; 
		auto voiceSpellDelivery = 
		(
			voiceSpell ? voiceSpell->GetDelivery() : RE::MagicSystem::Delivery::kTotal
		);
		// Instant caster loaded up with a targeted shout/power spell 
		// and the player is trying to cast.
		bool canShoutSpellTraj = 
		(
			voiceSpell && 
			voiceSpell->avEffectSetting && 
			voiceSpell->avEffectSetting->data.projectileBase &&
			voiceSpellDelivery != RE::MagicSystem::Delivery::kSelf &&
			instantCaster &&
			p->pam->IsPerforming(InputAction::kShout)
		);
		if (canShoutSpellTraj)
		{
			auto baseProj = voiceSpell->avEffectSetting->data.projectileBase;
			// Special case for beam/flame projectiles:
			// aim in the player's facing direction.
			if (baseProj->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower
				))
			{
				trajType = ProjectileTrajType::kAimDirection;
			}

			// Cast from instant caster node position or from the look at pos, if unavailable.
			RE::NiPoint3 releasePos = coopActor->GetLookingAtLocation();
			if (instantCaster->magicNode) 
			{
				releasePos = instantCaster->magicNode->world.translate;
			}
			else
			{
				const auto strings = RE::FixedStrings::GetSingleton();
				if (strings)
				{
					auto headMagicNodePtr = Util::Get3DObjectByName
					(
						coopActor.get(), strings->npcHeadMagicNode
					); 
					if (headMagicNodePtr)
					{
						releasePos = headMagicNodePtr->world.translate;
					}
				}
			}

			const auto trajInfo = std::make_unique<ManagedProjTrajectoryInfo>
			(
				p,
				baseProj,
				nullptr,
				voiceSpell->avEffectSetting, 
				releasePos,
				trajType
			);
			DrawTrajectory
			(
				baseProj,
				RE::ObjectRefHandle(),
				trajInfo->releasePos,
				trajInfo->trajectoryEndPos,
				trajInfo->initialTrajTimeToTarget,
				trajInfo->releaseSpeed,
				trajInfo->launchPitch,
				trajInfo->launchYaw,
				trajInfo->g,
				trajInfo->mu,
				baseProj->data.range,
				trajInfo->trajType,
				trajInfo->canReachTarget,
				true,
				false
			);
		}

		//===================
		// [Grabbed Objects]:
		//===================

		// Trying to throw a grabbed object at the crosshair target position.
		if (crosshairActive &&
			rmm->isGrabbing && 
			rmm->GetNumGrabbedRefrs() > 0 && 
			p->pam->IsPerforming(InputAction::kGrabObject))
		{
			// IMPORTANT NOTE:
			// Drawing all grabbed refrs' projected release trajectories 
			// would be WAY too performance intensive and unnecessary,
			// since all refrs have the same release speed + launch angles on release.
			// The only difference is their release position, but drawing one trajectory
			// starting from the position of the first grabbed refr should provide
			// a helpful picture of how all the refrs will behave when thrown.

			// Need to have at least 1 valid grabbed refr.
			auto firstRefrHandle = 
			(
				rmm->grabbedRefrInfoList[0] && rmm->grabbedRefrInfoList[0]->IsValid() ?
				rmm->grabbedRefrInfoList[0]->refrHandle :
				RE::ObjectRefHandle()
			);
			if (!Util::HandleIsValid(firstRefrHandle))
			{
				return;
			}

			std::unique_ptr<ReleasedReferenceInfo> firstRefrInfo = 
			(
				std::make_unique<ReleasedReferenceInfo>(firstRefrHandle)
			);
			if (!firstRefrInfo)
			{
				return;
			}

			// Set the total magicka cost to throw all grabbed refrs,
			// and then compute the magicka overflow factor before populating the trajectory data.
			// Modify by the player's magicka cost multiplier 
			// to reflect the true cost applied through the CheckClampDamageModifier() hook.
			rmm->SetTotalThrownRefrMagickaCost(p, true);
			firstRefrInfo->magickaOverflowSlowdownFactor = 
			(
				rmm->GetThrownRefrMagickaOverflowSlowdownFactor
				(
					p, rmm->totalThrownRefrMagickaCost * Settings::vfMagickaCostMult[playerID]
				)
			);
			firstRefrInfo->InitPreviewTrajectory(p);
			DrawTrajectory
			(
				nullptr,
				firstRefrInfo->refrHandle,
				firstRefrInfo->releasePos,
				firstRefrInfo->trajectoryEndPos,
				firstRefrInfo->initialTimeToTarget,
				firstRefrInfo->releaseSpeed,
				firstRefrInfo->launchPitch,
				firstRefrInfo->launchYaw,
				Util::GetGravitationalConstant(),
				Settings::fMu,
				FLT_MAX,
				firstRefrInfo->trajType,
				firstRefrInfo->canReachTarget,
				false,
				true
			);
		}
	}

	void TargetingManager::DrawTrajectory
	(
		RE::BGSProjectile* a_baseProj,
		RE::ObjectRefHandle a_projHandle,
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
		bool&& a_capVelocity
	)
	{
		// Draw trajectory based on the given launch parameters.
		// Can handle weapon/magic projectiles, or thrown refrs.
		// Can choose to cap the velocity, and thus the displacement, per time slice.
		// Projected trajectory is most accurate when the target position
		// is close to the release position.
		// 
		// IMPORTANT NOTES:
		// NOT perfectly accurate and mainly meant to give the player a general idea
		// of a projectile's projected path to the target before it is launched
		// and allow for guided aim adjustments.
		// 
		// 1. Framerate dependent -- the smoother the framerate, the more accurate the trajectory.
		// 2. The 'fixed' portions of the trajectory,
		// ex. the entirety of the 'Predictive' or 'Aim Direction' trajectories,
		// or the first portion of the 'Homing' trajectory before homing in,
		// will correspond almost perfectly to the actual trajectory 
		// that the released projectile takes.
		// 3. The more time slices, the smoother the curve, but the higher the performance hit
		// from drawing more line segments and performing more raycasts along the curve.
		// 4. For the homing-in part of the trajectory, the projected trajectory
		// will not perfectly conform to the released projectile's actual path 
		// and will tend to undershoot at close range
		// (start homing in earlier due to the time slice being smaller than the frame delta time),
		// and overshoot when aiming far away
		// (start homing in later due to the time slice being larger than the frame delta time).
		// 5. For a perfect 1-1 correspondence, the time slice would have to equal the frame time,
		// but this would lead to a HUGE hit in performance if the time-to-target
		// is large. For example, at 60 FPS and therefore 60 time slices a second, 
		// shooting at the sky will typically result in a time-of-flight of 15+ seconds 
		// and therefore 900+ line segments queued and raycasts performed PER trajectory drawn. 
		// Not feasible.
		// 6. Collision check raycasts are not done with a hull size equal to the radius 
		// of the projectile for performance reasons.
		// The havok pick data cast is much faster than the camera sphere cast
		// with a customizable hull size.
		// However, this will result in the predicted trajectory not correctly showing collisions
		// with geometry if the drawn trajectory segment is within a radius-length of an obstacle.
		
		// Don't draw if the time to target is 0 
		// or if the release position is the same as the target position.
		if (a_initialProjectedTimeToTarget == 0.0f || a_releasePos == a_targetPos)
		{
			return;
		}
		
		// Hit the crosshair/ranged actor target when walking the curve.
		bool hitTarget = false;
		// Did the projectile hit another, non-target refr that is selectable?
		bool hitSelectableNonTargetRefr = false;
		// Should set a homing trajectory.
		// Should begin or continue homing in on the target position.
		bool shouldHomeIn = false;
		// Hit an object en-route to the target position.
		bool trajCollision = false;
		// Projectile and current crosshair target refrs.
		auto projRefrPtr = Util::GetRefrPtrFromHandle(a_projHandle);
		auto crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
		auto rangedTargetActorHandle = GetRangedTargetActor();
		bool rangedTargetActorValidity = Util::HandleIsValid(rangedTargetActorHandle);
		// Hit result for casts between time slice start and end positions.
		Raycast::RayResult result{ };
		// Ignore the firing player, the crosshair target, if selected, 
		// and the projectile itself, if given, when filtering through raycast collision results 
		// for each time slice.
		std::vector<RE::TESObjectREFR*> raycastExcludedRefrs{ coopActor.get() };
		if (crosshairRefrPtr)
		{
			raycastExcludedRefrs.emplace_back(crosshairRefrPtr.get());
		}

		if (projRefrPtr)
		{
			raycastExcludedRefrs.emplace_back(projRefrPtr.get());
		}

		// Insert all grabbed/thrown refrs if this is a grabbed/thrown refr.
		// Want to ignore collisions between them.
		if (!a_isWeapMagProj)
		{
			if (rmm->isGrabbing)
			{
				for (const auto& info : rmm->grabbedRefrInfoList)
				{
					if (!info->IsValid() || info->refrHandle == a_projHandle)
					{
						continue;
					}

					raycastExcludedRefrs.emplace_back(info->refrHandle.get().get());
				}
			}
			else
			{
				for (const auto& info : rmm->releasedRefrInfoList)
				{
					if (!info->IsValid() || info->refrHandle == a_projHandle)
					{
						continue;
					}

					raycastExcludedRefrs.emplace_back(info->refrHandle.get().get());
				}
			}
		}

		// Don't handle position/angle deltas below this value.
		const float epsilon = 1E-3f;
		// Initial XY, X, Y, and Z components of velocity at launch.
		const float initVelXY = a_releaseSpeed * cosf(a_launchPitch);
		const float initVelX = initVelXY * cosf(a_launchYaw);
		const float initVelY = initVelXY * sinf(a_launchYaw);
		const float initVelZ = a_releaseSpeed * sinf(a_launchPitch);
		// Distance in the XY plane to the target position.
		const float xyDistToTargetPos = Util::GetXYDistance(a_releasePos, a_targetPos);

		// Alpha values for the first and last line segment to draw along the trajectory.
		const float startingAlphaRatio = 0.2f;
		const float endingAlphaRatio = 0.7f;
		// RBG values at the start and end.
		const float rStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0xFF000000) >> 24;
		const float gStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0x00FF0000) >> 16;
		const float bStart = (Settings::vuOverlayRGBAValues[p->playerID] & 0x0000FF00) >> 8;
		const float rEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0xFF000000) >> 24
		);
		const float gEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0x00FF0000) >> 16
		);
		const float bEnd = 
		(
			(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0x0000FF00) >> 8
		);
		// Current RGBA and alpha values to set when drawing a line segment.
		uint32_t rgba = Settings::vuOverlayRGBAValues[p->playerID];
		uint32_t alpha = static_cast<uint32_t>(0xFF * startingAlphaRatio);
		
		// Get the expected lifetime for the projectile before it potentially despawns.
		float expectedLifetime = FLT_MAX;
		if (a_baseProj)
		{
			if (a_baseProj->data.lifetime > 0.0f)
			{
				expectedLifetime = a_baseProj->data.lifetime;
			}
			else
			{
				expectedLifetime = a_baseProj->data.range / a_baseProj->data.speed;
			}
		}
		
		// Capped projectile trajectory flight time.
		const float maxFlightTime = 
		(
			a_isWeapMagProj ? 
			min(expectedLifetime, Settings::fMaxProjAirborneSecsToTarget) :
			Settings::fMaxSecsBeforeClearingReleasedRefr
		);
		// Will not reach the target in time.
		bool tooLongToReach = 
		(
			a_initialProjectedTimeToTarget == 0.0f ||
			a_initialProjectedTimeToTarget >= maxFlightTime
		);
		// Do not draw the entire trajectory beyond a certain interval to improve performance.
		float totalFlightTime = min(a_initialProjectedTimeToTarget, maxFlightTime);

		// Cannot split the trajectory into two parts 
		// if the projectile reaches the target in under two frames,
		// so we'll start homing in right away, if this is a homing projectile.
		bool lessThanTwoFramesToReachTarget = 
		(
			a_initialProjectedTimeToTarget <= *g_deltaTimeRealTime * 2.0f
		);
		DBG
		(
			"{}: {}, TTT: {}, too long to reach: {} ({}, {}), "
			"less than 2 frames: {}, max flight time: {}, projected: {}.",
			coopActor->GetName(),
			a_baseProj ? 
			Util::GetEditorID(a_baseProj) :
			"NONE",
			a_initialProjectedTimeToTarget,
			tooLongToReach,
			expectedLifetime,
			Settings::fMaxProjAirborneSecsToTarget,
			lessThanTwoFramesToReachTarget,
			maxFlightTime,
			totalFlightTime
		);

		// Total number of time slices to split up the trajectory into.
		const uint32_t totalTimeSlices = 50.0f;
		// Seconds elapsed between each line segment the trajectory is broken up into.
		float secsSlice = totalFlightTime / totalTimeSlices;
		// Current number of elapsed time slices when walking the curve.
		uint32_t sliceCount = 0;
		// Number of time slices that must elapse before drawing a line segment.
		// Produces a 'dotted' effect.
		// Set to 1 to trace the entire trajectory curve.
		uint32_t slicesBeforeDrawing = 1;
		// Incremented until reaching the number of slices before drawing, then wrap around.
		// Line segments are drawn when this equals 0.
		uint32_t currentSliceIndex = 0;
		// Total number of drawn line segments.
		uint32_t drawnSegments = 0;
		// Current time elapsed along the trajectory.
		// Used to get the starting endpoint along the curve for this time slice.
		float currentT = 0.0f;
		// The next iteration's elapsed time along the trajectory.
		// Used to get the next endpoint along the curve for this time slice.
		float nextT = secsSlice;
		// Current distance of the last set endpoint from the release position.
		float distanceFromReleasePos = 0.0f;
		// Distance traversed along the trajectory so far.
		float distanceTraversed = 0.0f;
		// Start and end world positions along the curve for the current time slice.
		RE::NiPoint3 start{ a_releasePos };
		RE::NiPoint3 end{ a_releasePos };
		// Tangent vector for drawing the endpoint cap.
		glm::vec3 tangent{ ToVec3(Util::RotationToDirectionVect(a_launchPitch, a_launchYaw)) };
		while (sliceCount < totalTimeSlices)
		{
			// Set color values first.
			alpha = static_cast<uint32_t>
			(
				0xFF * 
				Util::InterpolateEaseIn
				(
					startingAlphaRatio,
					endingAlphaRatio, 
					sliceCount / static_cast<float>(totalTimeSlices), 
					3.0f
				)
			);
			rgba = 
			(
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							rStart, 
							rEnd,
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 24
				) |
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							gStart, 
							gEnd, 
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 16
				) |
				(
					static_cast<uint32_t>
					(
						std::lerp
						(
							bStart, 
							bEnd, 
							sliceCount / static_cast<float>(totalTimeSlices)
						)	
					) << 8
				) |
				alpha
			);
			
			// Current velocity along the fixed part of this trajectory.
			auto fixedTrajVelocity = RE::NiPoint3
			(
				initVelX,
				initVelY,
				initVelZ - a_g * currentT
			);
			// Pitch and yaw along the fixed part of the trajectory.
			float fixedTrajPitch = Util::GetPitchBetweenPositions
			(
				RE::NiPoint3(), fixedTrajVelocity
			);
			float fixedTrajYaw = Util::GetYawBetweenPositions
			(
				RE::NiPoint3(), fixedTrajVelocity
			);
			// Pitch and yaw from the last computed endpoint to the target position.
			const float pitchToTarget = Util::GetPitchBetweenPositions
			(
				end, a_targetPos
			);
			const float yawToTarget = Util::GetYawBetweenPositions
			(
				end, a_targetPos
			);
			// Pitch/yaw set to launch values (in game coords) for the first time slice,
			// since start == end before the first iteration completes.
			// Last set pitch and yaw from the previous time slices.
			const float lastSetPitch = 
			(
				sliceCount == 0 ?
				-a_launchPitch : 
				Util::DirectionToGameAngPitch(end - start)
			);
			const float lastSetYaw = 
			(
				sliceCount == 0 ?
				Util::ConvertAngle(a_launchYaw) : 
				Util::DirectionToGameAngYaw(end - start)
			);
			if (a_trajType == ProjectileTrajType::kHoming)
			{
				bool wasHomingin = shouldHomeIn;
				if (!shouldHomeIn)
				{
					// Should home in on the target position if past the halfway point,
					// the target position is too far away to reach in time,
					// or if the projectile will reach the target position in under 2 frames.
					bool passedHalfwayPoint = 
					(
						currentT - 0.5f * totalFlightTime > -0.1f * secsSlice ||
						Util::GetXYDistance(a_releasePos, end) >=
						0.5f * Util::GetXYDistance(a_releasePos, a_targetPos)
					);
					shouldHomeIn = 
					(
						passedHalfwayPoint || 
						tooLongToReach ||
						lessThanTwoFramesToReachTarget
					);
				}
				
				if (shouldHomeIn)
				{
					//=================================
					// [Set Pitch/Yaw to Track Target]:
					//=================================

					float pitchToSet = fixedTrajPitch;
					float yawToSet = fixedTrajYaw;
					if (tooLongToReach || lessThanTwoFramesToReachTarget)
					{
						// Can't hit target with given launch pitch, 
						// so set yaw directly to target right away.
						pitchToSet = pitchToTarget;
						yawToSet = yawToTarget;
					}
					else
					{
						float timeToFullyHomeIn = totalFlightTime;
						//if (tooLongToReach)
						//{
						//	// Home in completely at most a quarter way along the trajectory,
						//	// in terms of time.
						//	timeToFullyHomeIn = 0.25f * totalFlightTime;
						//}

						// Slowly turn to face.
						float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - lastSetPitch);
						pitchToSet = Util::NormalizeAngToPi
						(
							lastSetPitch + 
							Util::InterpolateSmootherStep
							(
								0.0f, 
								pitchDiff,
								min(1.0f, currentT / timeToFullyHomeIn)
							)
						);
						float yawDiff = Util::NormalizeAngToPi(yawToTarget - lastSetYaw);
						yawToSet = Util::NormalizeAng0To2Pi
						(
							lastSetYaw + 
							Util::InterpolateSmootherStep
							(
								0.0f, 
								yawDiff,
								min(1.0f, currentT / timeToFullyHomeIn)
							)
						);
					}

					// Velocity and speed used to obtain the time slice endpoints.
					auto newVel = RE::NiPoint3();
					float speed = fixedTrajVelocity.Length();
					// Max distance the projectile will travel in 1 frame 
					// at its current velocity.
					float maxDistPerFrame = 
					(
						max(a_releaseSpeed, speed) * *g_deltaTimeRealTime
					);
					// Velocity mult which slows down the projectile when close to the target 
					// to minimize overshooting and jarring course correction.
					float distSlowdownFactor = std::clamp
					(
						powf(start.GetDistance(a_targetPos) / (maxDistPerFrame + 0.01f), 5.0f), 
						0.1f,
						1.0f
					);
					if (a_isWeapMagProj)
					{
						RE::NiPoint3 targetLinVel{ };
						if (rangedTargetActorValidity)
						{
							targetLinVel = Util::GetActorLinearVelocity
							(
								rangedTargetActorHandle.get().get()
							);
						}
						else if (crosshairRefrPtr)
						{
							crosshairRefrPtr->GetLinearVelocity(targetLinVel);
						}

						speed = max
						(
							speed * distSlowdownFactor, 
							min(a_releaseSpeed, 1000.0f) + targetLinVel.Length()
						);
					}

					newVel = RE::NiPoint3
					(
						Util::RotationToDirectionVect
						(
							-pitchToSet, Util::ConvertAngle(yawToSet)
						) * speed
					);
					// Once homing, because the projectile's speed is now dependent 
					// on how far away it is from the target, we have to update
					// the time slice interval once the projectile starts homing in.
					uint32_t remainingSlices = totalTimeSlices - sliceCount;
					if (remainingSlices <= 0)
					{
						break;
					}

					secsSlice = 
					(
						(end.GetDistance(a_targetPos) / speed) / remainingSlices
					);
					// Starting from the previous endpoint, 
					// add the velocity * the time slice length.
					start = end;
					end += newVel * secsSlice;
				}
				else
				{
					// Set directly to the kinematically-derived positions when not homing.
					start = RE::NiPoint3
					(
						a_releasePos.x + initVelX * currentT,
						a_releasePos.y + initVelY * currentT,
						a_releasePos.z + initVelZ * currentT - 
						0.5f * a_g * currentT * currentT
					);
					end = RE::NiPoint3
					(
						a_releasePos.x + initVelX * nextT,
						a_releasePos.y + initVelY * nextT,
						a_releasePos.z + initVelZ * nextT - 
						0.5f * a_g * nextT * nextT
					);
				}
			}
			else if (a_g == 0.0f)
			{
				// Straight line towards the target position.
				start = RE::NiPoint3
				(
					a_releasePos.x + initVelX * currentT,
					a_releasePos.y + initVelY * currentT,
					a_releasePos.z + initVelZ * currentT
				);
				end = RE::NiPoint3
				(
					a_releasePos.x + initVelX * nextT,
					a_releasePos.y + initVelY * nextT,
					a_releasePos.z + initVelZ * nextT
				);
			}
			else if (!a_isWeapMagProj)
			{
				// No air resistance considerations for released refrs.
				// Set directly to the kinematically-derived positions.
				start = RE::NiPoint3
				(
					a_releasePos.x + initVelX * currentT,
					a_releasePos.y + initVelY * currentT,
					a_releasePos.z + initVelZ * currentT - 
					0.5f * a_g * currentT * currentT
				);
				end = RE::NiPoint3
				(
					a_releasePos.x + initVelX * nextT,
					a_releasePos.y + initVelY * nextT,
					a_releasePos.z + initVelZ * nextT - 
					0.5f * a_g * nextT * nextT
				);
			}
			else
			{
				// NOTE: 
				// Since the frametime is discrete and certain projectiles move extremely fast,
				// we cannot use the true velocity at any particular time computed
				// from the trajectory's formulas. 
				// We have to instead "connect the dots" between the current trajectory position 
				// and the next expected trajectory position one frame later 
				// to ensure that it will arrive at the next endpoint.
				// If the frametimes vary greatly from frame to frame,
				// the position and velocity calculations will not conform as well
				// to the original trajectory, speeding up and slowing down along the path.
				// 
				// Factors in linear air resistance.
				// May remove eventually.
				//
				// Initial X, Y components of velocity.
				float vx0 = a_releaseSpeed * cosf(a_launchPitch);
				float vy0 = a_releaseSpeed * sinf(a_launchPitch);
				// https://www.whitman.edu/Documents/Academics/Mathematics/2016/Henelsmith.pdf
				// XY, and Z positions: 
				// In the 2D plane, the XY pos is the X coordinate,
				// and Z pos is the Y coordinate.
				float currXY = (vx0 / a_mu) * (1 - exp(-a_mu * currentT));
				float currZ = 
				(
					(-a_g * currentT / a_mu) + 
					(1.0f / a_mu) * 
					(vy0 + a_g / a_mu) * 
					(1.0f - exp(-a_mu * currentT))
				);
				// Next projected XY and Z offsets.
				float nextXY = (vx0 / a_mu) * (1 - exp(-a_mu * nextT));
				float nextZ = 
				(
					(-a_g * nextT / a_mu) + 
					(1.0f / a_mu) * 
					(vy0 + a_g / a_mu) * 
					(1.0f - exp(-a_mu * nextT))
				);
				// Here, '+' means up, and '-' means down, unlike the game's pitch convention.
				// Pitch to face the next frame's expected position.
				const float pitchOnTraj = atan2f((nextZ - currZ), (nextXY - currXY));
				// Get the estimated speed from dividing the distance 
				// between the two positions by time slice length.
				const float speedToSet = 
				(
					Util::GetXYDistance(currXY, currZ, nextXY, nextZ) / secsSlice
				);
				// Launch yaw maintained throughout.
				auto vel = Util::RotationToDirectionVect(pitchOnTraj, a_launchYaw) * speedToSet;
				
				// Starting from the previous endpoint, 
				// add the velocity * the time slice length.
				start = end;
				end = start + vel * secsSlice;
			}

			// Cap the velocity once setting both endpoints
			// and then add the capped velocity * time slice length.
			if (a_capVelocity && shouldHomeIn)
			{
				auto deltaPos = end - start;
				auto oldVel = deltaPos.Length() / secsSlice;
				if (oldVel > a_releaseSpeed)
				{
					deltaPos.Unitize();
					end = start + deltaPos * a_releaseSpeed * secsSlice;
				}
			}

			// If the time slice end position is past the target position 
			// on the projectile's way down, set the next time slice end position 
			// to the target position, and break to finish drawing the trajectory.
			RE::NiPoint3 deltaPos = end - start;
			RE::NiPoint3 deltaPosDir = deltaPos;
			RE::NiPoint3 endToTargetPosDir = a_targetPos - end;
			deltaPosDir.Unitize();
			endToTargetPosDir.Unitize();
			auto dot = std::clamp(deltaPosDir.Dot(endToTargetPosDir), -1.0f, 1.0f);
			if (dot + 1.0f <= epsilon && deltaPos.z < 0.0f)
			{
				distanceTraversed += end.GetDistance(start);
				end = a_targetPos;
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}

				break;
			}
			
			float deltaPosDist = deltaPos.Length();
			distanceTraversed += deltaPosDist;
			float distanceOvershoot = distanceTraversed - a_maxRange;
			// Stop walking the curve if the max range was reached.
			if (distanceOvershoot >= 0.0f)
			{
				auto deltaPosDir = end - start;
				deltaPosDir.Unitize();
				// Move end point to within the max range.
				end = start + deltaPosDir * (deltaPosDist - distanceOvershoot);
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}

				break;
			}

			// Raycast and potentially draw a line segment if there are no collisions.
			// At least one endpoint must be on screen.
			if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
			{	
				result = Raycast::hkpCastRay
				(
					ToVec4(start),
					ToVec4(end),
					raycastExcludedRefrs, 
					RE::COL_LAYER::kLOS
				);
				if (result.hit)
				{
					auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
					bool hitRefrValidity =
					(
						hitRefrPtr && Util::IsValidRefrForTargeting(hitRefrPtr.get())
					);
					// No need to continue walking the curve afterward.
					trajCollision = true;
					// Hit the crosshair refr or the aim correction target refr.
					hitTarget = 
					(
						(Util::HandleIsValid(result.hitRefrHandle)) &&
						(
							crosshairRefrPtr &&
							hitRefrValidity &&
							hitRefrPtr == crosshairRefrHandle.get()
						) ||
						(
							rangedTargetActorValidity && 
							hitRefrPtr == rangedTargetActorHandle.get()
						)
					);
					// Hit a selectable refr that wasn't targeted.
					hitSelectableNonTargetRefr = 
					(
						hitRefrValidity && !hitTarget && Util::IsSelectableRefr(hitRefrPtr.get())
					); 
					// Hit something, so set the time slice end point 
					// to the target or hit position,
					// and connect a line segment to it.
					if (hitTarget)
					{
						end = a_targetPos;
					}
					else
					{
						end = ToNiPoint3(result.hitPos);
					}

					if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
					{
						distanceFromReleasePos = end.GetDistance(a_releasePos);
						DebugAPI::QueueLine3D
						(
							ToVec3(start),
							ToVec3(end),
							rgba,
							Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
							(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
						);
						++drawnSegments;
					}

					break;
				}

				// Draw line segment if the the current slice index
				// divides into the number of slices before drawing,
				// or if this slice is the last one and will connect to the target position.
				// Also make sure either endpoint is on screen first.
				if ((currentSliceIndex == 0 || sliceCount == totalTimeSlices - 1) && 
					(Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end)))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}
			}

			// Before the next iteration, update the last tangent vector to the curve,
			// slice index, time points, and slice count.
			tangent = glm::normalize(ToVec3(end - start));
			currentSliceIndex = (currentSliceIndex + 1) % slicesBeforeDrawing;
			currentT += secsSlice;
			nextT += secsSlice;
			++sliceCount;
		}
		
		// Thickness for the 'cap' circle that marks the end of the trajectory.
		const float trajectoryCapThickness = 
		(
			0.5f * 
			max
			(
				Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID],
				0.125f * Settings::vfCrosshairGapRadius[p->playerID]
			) / 
			(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
		);
		bool inRange = distanceTraversed < a_maxRange;
		if (a_trajType == ProjectileTrajType::kAimDirection)
		{
			// If there was a hit on the frame before reaching the target position,
			// it is almost always the case that the trajectory hits 
			// the same object that the crosshair raycast hit, 
			// meaning it has effectively reached the target position.
			if ((!trajCollision || sliceCount >= totalTimeSlices - 1) &&
				(inRange && start != end && end != a_targetPos))
			{
				// Connect to the target position if in range.
				start = end;
				end = a_targetPos;
				if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
				{
					distanceFromReleasePos = end.GetDistance(a_releasePos);
					DebugAPI::QueueLine3D
					(
						ToVec3(start),
						ToVec3(end),
						rgba,
						Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
						(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
					);
					++drawnSegments;
				}
			}
			
			if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
			{
				// Also hit the target if one is selected and there was no collision 
				// along the trajectory to the crosshair world position.
				hitTarget |= 
				(
					(!trajCollision) &&
					(
						(rangedTargetActorValidity) || 
						(crosshairRefrPtr && p->tm->crosshairActive)
					)
				);
				if ((inRange) && (hitTarget || hitSelectableNonTargetRefr))
				{
					// Hit something selectable with an aim direction projectile,
					// so notify the player of great success!
					// Draw a crosshair outer outline-colored circle.
					DebugAPI::QueueCircle3D
					(
						ToVec3(end),
						glm::normalize(tangent),
						(Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 0xFFFFFF00) + 
						static_cast<uint32_t>(endingAlphaRatio * 0xFF),
						16,
						Settings::vfCrosshairGapRadius[p->playerID],
						trajectoryCapThickness,
						true,
						true,
						0.0f
					);
				}
				else
				{
					// Red circle otherwise.
					DebugAPI::QueueCircle3D
					(
						ToVec3(end),
						glm::normalize(tangent),
						0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
						16,
						Settings::vfCrosshairGapRadius[p->playerID],
						trajectoryCapThickness,
						true,
						true,
						0.0f
					);
				}
			}
		}
		else
		{
			// If there was a hit on the frame before reaching the target position,
			// it is almost always the case that the trajectory hits 
			// the same object that the crosshair raycast hit, 
			// meaning it has effectively reached the target position.
			if (!trajCollision || hitTarget || sliceCount >= totalTimeSlices - 1)
			{
				bool connectToTargetPos = 
				(
					(start != end && end != a_targetPos) &&
					(
						(a_isWeapMagProj && inRange) ||
						(!a_isWeapMagProj && a_canReachTarget)
					)
				);
				if (connectToTargetPos)
				{
					// Connect to the target position if in range.
					start = end;
					end = a_targetPos;
					if (Util::PointIsOnScreen(start) || Util::PointIsOnScreen(end))
					{
						distanceFromReleasePos = end.GetDistance(a_releasePos);
						DebugAPI::QueueLine3D
						(
							ToVec3(start),
							ToVec3(end),
							rgba,
							Settings::vfPredictedProjectileTrajectoryCurveThickness[p->playerID] /
							(1.0f + powf(distanceFromReleasePos / 1000.0f, 5.0f))
						);
						++drawnSegments;
					}
				}

				if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
				{
					if (a_canReachTarget && inRange)
					{
						// Cap off with a circle to show that the target position/max range 
						// was reached without an intervening collision.
						DebugAPI::QueueCircle3D
						(
							ToVec3(end),
							glm::normalize(tangent),
							(
								Settings::vuCrosshairOuterOutlineRGBAValues[p->playerID] & 
								0xFFFFFF00
							) + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
							16,
							Settings::vfCrosshairGapRadius[p->playerID],
							trajectoryCapThickness,
							true,
							true,
							0.0f
						);
					}
					else
					{
						// Red circle when it is not possible to reach the target position.
						DebugAPI::QueueCircle3D
						(
							ToVec3(end),
							glm::normalize(tangent),
							0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
							16,
							Settings::vfCrosshairGapRadius[p->playerID],
							trajectoryCapThickness,
							true,
							true,
							0.0f
						);
					}
				}
			}
			else if (Util::PointIsOnScreen(end, Settings::vfCrosshairGapRadius[p->playerID]))
			{
				// Red circle when beyond the projectile's range, 
				// or colliding and failing to reach the target position.
				DebugAPI::QueueCircle3D
				(
					ToVec3(end),
					glm::normalize(tangent),
					0xFF000000 + static_cast<uint32_t>(endingAlphaRatio * 0xFF),
					16,
					Settings::vfCrosshairGapRadius[p->playerID],
					trajectoryCapThickness,
					true,
					true,
					0.0f
				);
			}
		}
	}

	RE::ObjectRefHandle TargetingManager::GetClosestSelectableRefrToCrosshairRay()
	{
		// Iterate through nearby refrs and get the closest selectable refr to the crosshair ray 
		// (ray starting from the crosshair's screen position in the direction of the camera).

		auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr)
		{
			return RE::ObjectRefHandle();
		}

		RE::TESObjectREFRPtr closestSelectableRefr{ nullptr };
		float closest2DDist = FLT_MAX;
		RE::NiPoint3 crosshairScaleformPosition = ToNiPoint3(crosshairScaleformPos);

		// Constrain to the current cell and allow a slightly larger range,
		// since the check is for crosshair selection.
		// We don't need to perform bounds checks on all loaded refrs, 
		// which is way too intensive.
		Util::ForEachReferenceInCellWithinRange
		(
			glob.player1Actor->GetParentCell(),
			p->mm->playerTorsoPosition, 
			GetMaxActivationDist() * 2.0f, 
			true,
			[
				&closestSelectableRefr,
				&crosshairScaleformPosition,
				&closest2DDist, 
				&niCamPtr
			]
			(RE::TESObjectREFR* a_refr)
			{
				// No issues with selecting actors with raycasting, so skip them.
				if (!a_refr || a_refr->As<RE::Actor>())
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				if (!Util::IsSelectableRefr(a_refr))
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}
				
				auto refr3DPtr = Util::GetRefr3D(a_refr);
				if (!refr3DPtr)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				float dist2D = crosshairScaleformPosition.GetDistance
				(
					Util::WorldToScreenPoint3(refr3DPtr->worldBound.center)
				);
				if (dist2D < closest2DDist)
				{
					// Obstructions are hit on their 'outward-facing' surface 
					// by the crosshair raycast, which is not a surface 
					// visible to the players that are beyond the obstruction,
					// so exclude such objects from determining
					// the crosshair's world position and selected refr.
					bool isAnObstruction = 
					(
						glob.cam->obstructionFadeDataMap.contains
						(
							refr3DPtr
						)
					);
					if (isAnObstruction)
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					// Check three points on the hit refr to see
					// if any of them are in front of the camera.
					// Then if none of those points are in front, 
					// perform a more expensive refr bounds check.
					bool inFrontOfCam = 
					{
						Util::IsInFrontOfCam(a_refr->data.location) ||
						Util::IsInFrontOfCam(refr3DPtr->world.translate) ||
						Util::IsInFrontOfCam(refr3DPtr->worldBound.center) ||
						RE::NiCamera::BoundInFrustum
						(
							refr3DPtr->worldBound, niCamPtr.get()
						)
					};
					if (!inFrontOfCam)
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					const RE::NiPoint3 boundCenter = refr3DPtr->worldBound.center;
					const RE::NiMatrix3 rotMat = refr3DPtr->world.rotate;
					auto halfExtent = 
					(
						(a_refr->GetBoundMax() - a_refr->GetBoundMin()) / 2.0f
					);
					float maxX = -FLT_MAX;
					float maxY = -FLT_MAX;
					float minX = FLT_MAX;
					float minY = FLT_MAX;
					// Get the minimum and maximum X and Y screen coordinates
					// for the refr's bounding box.
					auto setNewMinMaxXY = 
					[&maxX, &maxY, &minX, &minY](const RE::NiPoint3& a_extentPos)
					{
						if (a_extentPos.x > maxX)
						{
							maxX = a_extentPos.x;
						}

						if (a_extentPos.x < minX)
						{
							minX = a_extentPos.x;
						}

						if (a_extentPos.y > maxY)
						{
							maxY = a_extentPos.y;
						}

						if (a_extentPos.y < minY)
						{
							minY = a_extentPos.y;
						}
					};

					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + rotMat * halfExtent
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + rotMat * -halfExtent
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								-halfExtent.x,
								halfExtent.y,
								halfExtent.z
							)
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								halfExtent.x,
								-halfExtent.y,
								halfExtent.z
							)
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								-halfExtent.x,
								-halfExtent.y,
								halfExtent.z
							)
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								-halfExtent.x,
								halfExtent.y,
								-halfExtent.z
							)
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								halfExtent.x,
								-halfExtent.y,
								-halfExtent.z
							)
						)
					);
					setNewMinMaxXY
					(
						Util::WorldToScreenPoint3
						(
							boundCenter + 
							rotMat * 
							RE::NiPoint3
							(
								-halfExtent.x,
								-halfExtent.y,
								-halfExtent.z
							)
						)
					);
					// Must be within the refr's 2D bounds.
					bool isInBounds = 
					(
						crosshairScaleformPosition.x <= maxX &&
						crosshairScaleformPosition.x >= minX &&
						crosshairScaleformPosition.y <= maxY &&
						crosshairScaleformPosition.y >= minY
					);
					if (!isInBounds)
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					closest2DDist = dist2D;
					closestSelectableRefr = 
					(
						RE::TESObjectREFRPtr(a_refr)
					);
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		// Need to do an LOS check to make sure the object is visible,
		// since it could be obscured by another object
		// that is farther from the first raycast hit position.
		if (closestSelectableRefr)
		{
			return closestSelectableRefr->GetHandle();
		}

		return RE::ObjectRefHandle();
	}

	RE::ActorHandle TargetingManager::GetClosestTargetableActorInFOV
	(
		RE::Actor* a_sourceActor,
		const bool a_useLeftStickAngle,
		const bool a_useXYDistance,
		const bool a_combatDependentSelection,
		const bool a_angularAccuracyOverDistance,
		const bool a_preferScreenspaceSelection,
		const float& a_fovRads,
		const float a_range
	)
	{
		// WALL OF TEXT. WALL OF TEXT. OOGA BOOGA. WALL OF TEXT.
		// Get the closest targetable actor from the source actor
		// using the given FOV in radians centered at their LS/RS aiming angle 
		// (LS/RS angle or facing angle (if LS/RS is not moved) in world or screen space),
		// and the given maximum range (worldspace distance( to consider targets.
		// 
		// If using the left stick angle for targeting, the FOV window is centered
		// at the left stick's worldspace/screenspace angle. 
		// Otherwise, the window is centered at the right stick's worldspace/screenspace angle.
		// 
		// If screen position checks are requested,
		// all world positions are converted to screen positions before performing FOV checks,
		// the FOV window is centered about the source actor's center in screen space, 
		// and the given range should be given worldspace distance units.
		// 
		// If using XY distance, the Z components for positions are ignored
		// when comparing distances.
		// 
		// If combat-dependent selection is requested, only consider hostile actors, 
		// unless attempting to heal a target.
		// 
		// If prioritizing angular accuracy, 
		// ensure the difference between the LS/RS angle and the angle
		// to each target considered is factored into calculations. 
		// Will then prioritze aiming directly towards the target instead of just ensuring 
		// the target is within the FOV window and retrieving the closest target within that window.
		// 
		// The FOV window is given in radians and is centered at the LS/RS targeting angle.
		// If the absolute angle difference between the targeting angle
		// and the angle from the source to the target is larger than half of this angular window, 
		// the target is not considered.
		// 
		// If range is given as '-1', ignore the range check.
		// Otherwise, if the target is further away from the source than this range, 
		// the target is not considered.

		auto procLists = RE::ProcessLists::GetSingleton();
		if (!procLists)
		{
			return RE::ActorHandle();
		}

		// Make sure the source actor is valid, so fall back to the player if not given.
		if (!a_sourceActor)
		{
			a_sourceActor = coopActor.get();
		}

		// Should only target allies if casting a non-hostile spell.
		// Don't want to go around healing enemies, do we?
		bool shouldOnlyTargetAllies = false;
		// Should only target corpses to resurrect if casting a reanimate spell.
		bool shouldOnlyTargetCorpses = false;
		// Attack source triggering the call.
		RE::TESForm* attackSource = nullptr;
		// Does the attack source chosen based on the player's current combat action
		// contain a spell.
		bool sourceHasSpell = false;
		// Choosing to select based on combat and attack state.
		if (a_combatDependentSelection) 
		{
			// Ranged options in right and left hand + quick cast + shout.
			if (((p->pam->AllInputsPressedForAction(InputAction::kCastRH) || 
				  p->pam->isInCastingAnimRH) && 
				  p->em->HasRHSpellEquipped()) ||
				((p->pam->AllInputsPressedForAction(InputAction::kAttackRH) ||
				  p->pam->isInCastingAnimRH || 
				  p->pam->isAttacking) && 
				  p->em->HasRHStaffEquipped()) ||
				((p->pam->AllInputsPressedForAction(InputAction::kAttackRH) || 
				  p->pam->isAttacking) &&
				  p->em->Has2HRangedWeapEquipped()))
			{
				auto asWeap = p->em->GetRHWeapon();
				if (asWeap && asWeap->IsStaff() && asWeap->formEnchanting)
				{
					// No need to select a target if casting at self.
					if (asWeap->formEnchanting->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
					{
						return RE::ActorHandle();
					}
				}
				
				attackSource = p->em->equippedForms[!EquipIndex::kRightHand];
			}
			else if (((p->pam->AllInputsPressedForAction(InputAction::kCastLH) ||
					   p->pam->isInCastingAnimLH) && 
					   p->em->HasLHSpellEquipped()) ||
					 ((p->pam->AllInputsPressedForAction(InputAction::kAttackLH) ||
					   p->pam->isInCastingAnimLH || p->pam->isAttacking) &&
					   p->em->HasLHStaffEquipped()))
			{
				auto asWeap = p->em->GetLHWeapon();
				if (asWeap && asWeap->IsStaff() && asWeap->formEnchanting)
				{
					// No need to select a target if casting at self.
					if (asWeap->formEnchanting->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
					{
						return RE::ActorHandle();
					}
				}
				
				attackSource = p->em->equippedForms[!EquipIndex::kLeftHand];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kQuickSlotCast) && 
					  p->em->quickSlotSpell) ||
					  (p->pam->reqSpecialAction == SpecialActionType::kQuickCast))
			{
				if (p->em->quickSlotSpell)
				{
					// No need to select a target if casting at self.
					if (p->em->quickSlotSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
					{
						return RE::ActorHandle();
					}
				}

				attackSource = p->em->equippedForms[!EquipIndex::kQuickSlotSpell];
			}
			else if ((p->pam->AllInputsPressedForAction(InputAction::kShout) || 
					  p->pam->isShouting) && 
					 (p->em->voiceSpell)) 
			{
				if (p->em->voiceSpell)
				{
					// No need to select a target if casting at self.
					if (p->em->voiceSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
					{
						return RE::ActorHandle();
					}
				}

				attackSource = p->em->equippedForms[!EquipIndex::kVoice];
			}

			if (attackSource)
			{
				auto asSpell = attackSource->As<RE::SpellItem>();
				if (asSpell)
				{
					// No need to select a target if casting at self.
					if (asSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf)
					{
						return RE::ActorHandle();
					}
				}

				// Single attack source, so check if it has a hostile spell.
				sourceHasSpell = asSpell;
				shouldOnlyTargetAllies = sourceHasSpell && !Util::HasHostileEffect(attackSource);
				// Check if the base effect has the reanimate archetype.
				shouldOnlyTargetCorpses = 
				(
					sourceHasSpell && 
					asSpell->avEffectSetting && 
					asSpell->avEffectSetting->HasArchetype
					(
						RE::EffectSetting::Archetype::kReanimate
					)
				);
			}
			else if (p->pam->reqSpecialAction == SpecialActionType::kCastBothHands ||
					 p->pam->reqSpecialAction == SpecialActionType::kDualCast)
			{
				RE::MagicItem* lhSpell = p->em->GetLHSpell();
				auto lhWeap = p->em->GetLHWeapon();
				lhSpell = !lhSpell && lhWeap ? lhWeap->formEnchanting : lhSpell;
				RE::MagicItem* rhSpell = p->em->GetRHSpell();
				auto rhWeap = p->em->GetRHWeapon();
				rhSpell = !rhSpell && rhWeap ? rhWeap->formEnchanting : rhSpell;
				bool castingAtSelfOnly = 
				(
					lhSpell && lhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf &&
					rhSpell && rhSpell->GetDelivery() == RE::MagicSystem::Delivery::kSelf
				);
				// No need to select a target if casting both spells at self.
				if (castingAtSelfOnly)
				{
					return RE::ActorHandle();
				}

				sourceHasSpell = lhSpell && rhSpell;
				// Must be casting healing spells in both hands.
				// Don't want a situation where the player is firing a destruction spell 
				// and a healing spell at a friendly target.
				shouldOnlyTargetAllies = 
				{
					sourceHasSpell && 
					!Util::HasHostileEffect(p->em->equippedForms[!EquipIndex::kLeftHand]) && 
					!Util::HasHostileEffect(p->em->equippedForms[!EquipIndex::kRightHand])
				};
				// Check if the base effect for both spells has the reanimate archetype.
				shouldOnlyTargetCorpses = 
				(
					sourceHasSpell && 
					lhSpell->avEffectSetting && 
					lhSpell->avEffectSetting->HasArchetype
					(
						RE::EffectSetting::Archetype::kReanimate
					) && 
					rhSpell->avEffectSetting && 
					rhSpell->avEffectSetting->HasArchetype
					(
						RE::EffectSetting::Archetype::kReanimate
					)
				);
			}

			/*DBG
			(
				"{}: Attack source: {}, has spell: {}, has hostile spell: {}. "
				"Should only target allies: {}, should only target corpses: {}.",
				coopActor->GetName(),
				attackSource ? attackSource->GetName() : "NONE",
				attackSource ? (bool)attackSource->As<RE::SpellItem>() : false,
				Util::HasHostileEffect(attackSource),
				shouldOnlyTargetAllies,
				shouldOnlyTargetCorpses
			);*/
		}

		// Angles around which the FOV window is centered.
		float worldTargetingAngle = 0.0f;
		float screenTargetingAngle = 0.0f;
		// Want to find actors in front of the player in their facing direction
		// if they are currently facing a target or the crosshair world position.
		bool usePlayerFacingAngle = crosshairActive;
		if ((a_useLeftStickAngle && p->lsMoved) || (!a_useLeftStickAngle && p->rsMoved))
		{
			if (usePlayerFacingAngle)
			{
				// First, remove the camera-relative portion of the player's facing angle.
				// Convert the angle to its unit circle equivalent.
				// Then, flip the Y comp for the angle on the unit circle
				// to retrieve the new angle that corresponds with the Scaleform convention.
				screenTargetingAngle = 
				(
					2.0f * PI - 
					Util::NormalizeAng0To2Pi
					(
						Util::ConvertAngle
						(
							Util::NormalizeAng0To2Pi
							(
								coopActor->data.angle.z - glob.cam->GetCurrentYaw()
							)
						)
					)
				);
			}
			else
			{
				// Flip LS Y comp sign to conform with Scaleform convention.
				const auto& stickData = glob.cdh->GetAnalogStickState
				(
					deviceID, a_useLeftStickAngle
				);
				screenTargetingAngle = atan2f(-stickData.yComp, stickData.xComp);
			}
		}
		else
		{
			RE::NiPoint3 aimOriginPos = p->mm->playerTorsoPosition;
			RE::NiPoint3 aimDirection = Util::RotationToDirectionVect
			(
				0.0f, 
				Util::ConvertAngle
				(
					usePlayerFacingAngle ? 
					coopActor->data.angle.z :
					a_useLeftStickAngle ? 
					p->analogStickParams[!AnalogStickParams::kLSCamRelAngMovingFromCenter] :
					p->analogStickParams[!AnalogStickParams::kRSCamRelAngMovingFromCenter]
				)
			);
			auto screenAimOriginPos = Util::WorldToScreenPoint3(aimOriginPos, false);
			screenAimOriginPos.z = 0.0f;
			auto screenAimPos = Util::WorldToScreenPoint3
			(
				aimOriginPos + 
				aimDirection * 100.0f,
				false
			);
			screenAimPos.z = 0.0f;
			auto screenAimDir = screenAimPos - screenAimOriginPos;
			if (screenAimDir.Length() == 0.0f)
			{
				float camYaw = glob.cam->GetCurrentYaw();
				float yawDiff = Util::NormalizeAngToPi
				(
					camYaw - Util::DirectionToGameAngYaw(aimDirection)
				);
				// Aim down on the screen if not facing the camera's direction;
				// otherwise, aim up the screen.
				// Sign flipped due to Scaleform convention
				// (origin top left instead of bottom left).
				if (fabsf(yawDiff) >= PI / 2.0f)
				{
					screenTargetingAngle = PI / 2.0f;
				}
				else
				{
					screenTargetingAngle = 3.0f * PI / 2.0f;
				}
			}
			else
			{
				screenAimDir.Unitize();
				screenTargetingAngle = atan2f(screenAimDir.y, screenAimDir.x);
			}
		}

		if (usePlayerFacingAngle)
		{
			worldTargetingAngle = coopActor->data.angle.z;
		}
		else if (a_useLeftStickAngle)
		{
			worldTargetingAngle =
			(
				p->lsMoved ? 
				p->analogStickParams[!AnalogStickParams::kLSCamRelAng] :
				p->analogStickParams[!AnalogStickParams::kLSCamRelAngMovingFromCenter]
			);
		}
		else
		{
			worldTargetingAngle = 
			(
				p->rsMoved ? 
				p->analogStickParams[!AnalogStickParams::kRSCamRelAng] :
				p->analogStickParams[!AnalogStickParams::kRSCamRelAngMovingFromCenter]
			);
		}

		screenTargetingAngle = Util::NormalizeAng0To2Pi(screenTargetingAngle);
		worldTargetingAngle = Util::NormalizeAng0To2Pi(worldTargetingAngle);
		
		// Lowest distance-angle weight. Starts at max possible value.
		float minAngDistWeight = FLT_MAX;
		// Computed for each valid refr (outparam).
		float computedAngDistWeight = FLT_MAX;
		// Does this actor have the smallest angle/distance weight?
		bool hasMinAngDistWeight = false;
		// Is the actor's torso on screen?
		bool isOnScreen = false;
		// Is the actor in range and within the targeting angle's FOV window?
		bool inRangeAndFOV = false;
		// Another actor is in combat with this player.
		bool inCombatWithPlayer = false;
		// Sorted by angle/distance factor from smallest to largest.
		// Cached to check LOS on chosen actors from closest to farthest 
		// until the first one in LOS is found.
		std::multimap<float, RE::ActorHandle> factorMap{ };
		// Check all high actors.
		for (const auto& closeActorHandle : procLists->highActorHandles)
		{
			auto actorPtr = Util::GetActorPtrFromHandle(closeActorHandle); 
			if (!actorPtr || !Util::IsValidRefrForTargeting(actorPtr.get()) || 
				shouldOnlyTargetCorpses != actorPtr->IsDead())
			{
				continue;
			}

			// Targetable actors:
			// 1. Not blacklisted -AND-
			// 2. When only targeting allies (when casting healing spells),
			// must be a normally friendly actor that is not hostile -OR-
			// 3. When not targeting allies, must be a hostile actor.
				
			// Blacklist set, current mount, and non-players in the co-op entity blacklist.
			bool filteredOut = 
			{
				(actorPtr == coopActor) ||
				(actorPtr.get() == a_sourceActor) ||
				(actorPtr == p->GetCurrentMount())
			};
			if (filteredOut)
			{
				continue;
			}

			// Is hostile to a player and is targeting a player or player-friendly actor
			// or is in combat and fully detects any active player.
			auto p1 = RE::PlayerCharacter::GetSingleton();
			bool isActivelyHostile = Util::IsActivelyHostileToPlayerOrAlly(actorPtr.get());
			// Cap the range if the target is not actively hostile 
			// so we don't select a carefree rabbit 3 holds over hiding behind a tree.
			// Want to keep their location a surprise, you see.
			// And fewer LOS checks to perform, especially unnecessary ones,
			// always means better performance.
			// At least one actor is angry at this player.
			if (!inCombatWithPlayer && isActivelyHostile)
			{
				inCombatWithPlayer = true;
			}

			// Skip non-actively-hostile, off-screen actors.
			isOnScreen = Util::PointIsOnScreen(Util::GetTorsoPosition(actorPtr.get()));
			if (!isOnScreen && !isActivelyHostile)
			{
				continue;
			}

			// Next, filter out targets based on spell target type, 
			// friendliness, and the player's current combat state.
			// Filter out living actors when attempting to reanimate, 
			// or hostile actors when selecting allies
			// or friendly/neutral actors when the crosshair is active or when in combat.
			filteredOut = 
			(
				(!shouldOnlyTargetCorpses) &&
				(a_combatDependentSelection) && 
				(
					(
						(shouldOnlyTargetAllies) && 
						(
							!Util::IsPartyFriendlyActor(actorPtr.get()) || isActivelyHostile
						)
					) || 
					(
						(!shouldOnlyTargetAllies && !isActivelyHostile) && 
						(a_useLeftStickAngle || glob.isInCoopCombat)
					)
				)
			);
			
			if (filteredOut)
			{
				//DBG("{}: Filtered out {}.", coopActor->GetName(),actorPtr->GetName());
				continue;
			}

			// Run close actor check to update the new closest actor within the FOV window.
			IsRefrInRangeAndInFOV
			(
				a_sourceActor,
				actorPtr.get(), 
				a_angularAccuracyOverDistance,
				a_useXYDistance,
				isActivelyHostile,
				a_preferScreenspaceSelection,
				screenTargetingAngle,
				worldTargetingAngle,
				a_fovRads,
				a_range,
				computedAngDistWeight,
				inRangeAndFOV
			);
			if (inRangeAndFOV)
			{
				/*
				DBG
				(
					"{}: {} (0x{:X}) is now closest to {}. "
					"Crosshair mode: {}, combat with player: {}, "
					"co-op combat active: {}, only corpses: {}, "
					"only allies: {}, party friendly: {}, actively hostile: {}. New factor: {}. "
					"Stick mag, prev: {}, {}.", 
					coopActor->GetName(),
					actorPtr->GetName(),
					actorPtr->formID,
					a_sourceActor->GetName(),
					aimMode,
					inCombatWithPlayer,
					glob.isInCoopCombat,
					shouldOnlyTargetCorpses,
					shouldOnlyTargetAllies,
					Util::IsPartyFriendlyActor(actorPtr.get()),
					isActivelyHostile,
					computedAngDistWeight,
					aimMode == AimMode::kTwinStick ? 
					glob.cdh->GetAnalogStickState(deviceID, false).normMag :
					glob.cdh->GetAnalogStickState(deviceID, true).normMag,
					aimMode == AimMode::kTwinStick ? 
					glob.cdh->GetAnalogStickState(deviceID, false).prevNormMag :
					glob.cdh->GetAnalogStickState(deviceID, true).prevNormMag
				);*/
				factorMap.insert({ computedAngDistWeight, closeActorHandle });
				minAngDistWeight = computedAngDistWeight;
			}
		}
		
		// If not in combat and either not casting or casting a hostile spell, 
		// do not pick a close actor target.
		// Only want to choose friendly actors to heal with spells when out of combat.
		if ((a_useLeftStickAngle) &&
			(!glob.isInCoopCombat) && 
			(!sourceHasSpell || !shouldOnlyTargetAllies)) 
		{
			return RE::ActorHandle();
		}

		// Also add P1 if the companion player is performing this check.
		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (p1 && !p->isPlayer1) 
		{
			// No combat-dependent filter or if trying to heal P1.
			bool canAddP1 =
			(
				(!shouldOnlyTargetCorpses) && 
				(!a_combatDependentSelection || shouldOnlyTargetAllies)
			);
			// Do not target P1 with hostile spells when in combat.
			if (!a_useLeftStickAngle)
			{
				canAddP1 |= !glob.isInCoopCombat;
			}
			
			if (canAddP1)
			{
				// Perform new closest actor in FOV check on P1.
				IsRefrInRangeAndInFOV
				(
					a_sourceActor,
					p1,
					a_angularAccuracyOverDistance,
					a_useXYDistance,
					false,
					a_preferScreenspaceSelection,
					screenTargetingAngle,
					worldTargetingAngle,
					a_fovRads,
					a_range,
					computedAngDistWeight,
					inRangeAndFOV
				);
				if (inRangeAndFOV) //&& computedAngDistWeight < minAngDistWeight) 
				{
					minAngDistWeight = computedAngDistWeight;
					factorMap.insert({ computedAngDistWeight, p1->GetHandle() });
				}
			}
		}
		
		// Handle for the closest actor in FOV window.
		RE::ActorHandle closestActorInFOVHandle = RE::ActorHandle();
		for (const auto& [factor, actorHandle] : factorMap)
		{
			const auto refrPtr = Util::GetRefrPtrFromHandle(actorHandle);
			if (!refrPtr)
			{
				continue;
			}

			const auto asActor = refrPtr->As<RE::Actor>();
			if (!asActor)
			{
				continue;
			}

			// No FOV checks necessary for players. Select and break.
			if (GlobalCoopData::IsCoopPlayer(asActor))
			{
				/*DBG
				(
					"{}: Selected a fellow adventurer, {}, factor: {}", 
					coopActor->GetName(), asActor->GetName(), factor
				);*/
				closestActorInFOVHandle = actorHandle;
				break;
			}

			bool hasLOS = Util::HasLOS
			(
				refrPtr.get(), coopActor.get(), true, false, crosshairWorldPos
			);
			/*DBG
			(
				"{}: Considering {}. Has LOS: {}, factor: {}", 
				coopActor->GetName(), refrPtr->GetName(), hasLOS, factor
			);*/
			if (hasLOS)
			{
				closestActorInFOVHandle = actorHandle;
				break;
			}
		}

		const auto actorPtr = Util::GetActorPtrFromHandle(closestActorInFOVHandle);
		DBG
		(
			"{}: CHOSEN: {} (0x{:X}) is now closest to {}. "
			"Crosshair mode: {}, combat with player: {}, "
			"co-op combat active: {}, only corpses: {}, "
			"only allies: {}, party friendly: {}. New factor: {}. "
			"Stick mag, prev: {}, {}.", 
			coopActor->GetName(),
			actorPtr ? actorPtr->GetName() : "NONE",
			actorPtr ? actorPtr->formID : 0xDEAD,
			a_sourceActor->GetName(),
			aimMode,
			inCombatWithPlayer,
			glob.isInCoopCombat,
			shouldOnlyTargetCorpses,
			shouldOnlyTargetAllies,
			actorPtr ? 
			Util::IsPartyFriendlyActor(actorPtr.get()) : 
			false,
			computedAngDistWeight,
			aimMode == AimMode::kTwinStick ? 
			glob.cdh->GetAnalogStickState(deviceID, false).normMag :
			glob.cdh->GetAnalogStickState(deviceID, true).normMag,
			aimMode == AimMode::kTwinStick ? 
			glob.cdh->GetAnalogStickState(deviceID, false).prevNormMag :
			glob.cdh->GetAnalogStickState(deviceID, true).prevNormMag
		);
		return closestActorInFOVHandle;
	}

	uint32_t TargetingManager::GetDetectionLvlRGB
	(
		const float& a_detectionLvl, bool&& a_fromRawLevel
	)
	{
		// Get gradient RGB value corresponding to the given detection level.
		// Raw detection level ranges from -1000 to 1000, 
		// but the range [-20, 0] holds the most relevant detection levels 
		// with a noticeable change in awareness,
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
		// Get gradient RGB value corresponding to the level difference 
		// between the player and the given actor.
		// 
		// Gradient RGB values generated using https://rgb.birdflop.com/

		uint32_t levelRGB = 0xFFFFFF;
		auto actorPtr = Util::GetActorPtrFromHandle(a_actorHandle); 
		if (!actorPtr)
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

	RE::ObjectRefHandle TargetingManager::GetLockOnTarget
	(
		RE::ObjectRefHandle a_currentTargetHandle,
		bool a_asAimTarget, 
		bool a_useLeftStickAngle, 
		bool a_fromCurrentTarget,
		bool a_selectOnHold
	)
	{
		// Choose a target to lock on to in the direction of the player's left or right stick.
		// Can choose either an living NPC, if requesting an aim target, 
		// or all selectable objects or NPCs for activation instead.
		// Can also select a new target relative to the current target, 
		// instead of the player's character themselves. 
		// This will cycle through targets in the direction of the analog stick,
		// instead of selecting a target radially from the player.
		// Can select when holding down a button or displacing the analog stick
		// at a regular interval. 
		// Otherwise, will look for a new target right away without a cooldown.
		// Return the computed target's handle.

		auto procLists = RE::ProcessLists::GetSingleton();
		if (!procLists)
		{
			return RE::ObjectRefHandle();
		}
		
		bool canSelect = 
		(
			!a_fromCurrentTarget ||
			!a_selectOnHold ||
			Util::GetElapsedSeconds(p->lastLockOnAimTargetChangeTP) > 
			Settings::fSecsBetweenSelectingLockOnTargets
		);
		// Closest object refr within FOV window.
		const auto& playerTorsoPos = p->mm->playerTorsoPosition;
		const float maxCheckDist = GetMaxActivationDist();
		if (!a_asAimTarget && !canSelect)
		{
			// Check for a new target if the current target
			// is not within activation distance anymore.
			auto currentTargetPtr = Util::GetRefrPtrFromHandle(a_currentTargetHandle);
			bool tooFarAway = 
			(
				currentTargetPtr &&
				Util::GetRefrPosition(currentTargetPtr.get()).GetDistance(playerTorsoPos) > 
				maxCheckDist
			);
			canSelect |= tooFarAway;
		}
		
		// Choose a new target if:
		// 1. For activation only: The current target is not within activation range.
		// 2. Selecting from the player as the source position.
		// 3. Selecting when an input is pressed and not held.
		// 4. The selection cooldown interval has elapsed
		// if starting from the currently selected target.
		if (!canSelect)
		{
			return a_currentTargetHandle;
		}

		// Angles around which the FOV window is centered.
		// Can use either the LS or RS game angle.
		float screenTargetingAngle = 0.0f;
		float worldTargetingAngle = 0.0f;
		if ((p->lsMoved && a_useLeftStickAngle) || (p->rsMoved && !a_useLeftStickAngle))
		{
			const auto& stickData = glob.cdh->GetAnalogStickState
			(
				deviceID, a_useLeftStickAngle
			);
			// Flip LS Y comp sign to conform with Scaleform convention.
			screenTargetingAngle = Util::NormalizeAng0To2Pi
			(
				atan2f(-stickData.yComp, stickData.xComp)
			);
		}
		else
		{
			RE::NiPoint3 aimOriginPos = p->mm->playerTorsoPosition;
			RE::NiPoint3 aimDirection = Util::RotationToDirectionVect
			(
				0.0f, 
				Util::ConvertAngle
				(
					a_useLeftStickAngle ? 
					p->analogStickParams[!AnalogStickParams::kLSCamRelAngMovingFromCenter] :
					p->analogStickParams[!AnalogStickParams::kRSCamRelAngMovingFromCenter]
				)
			);
			auto screenAimOriginPos = Util::WorldToScreenPoint3(aimOriginPos, false);
			screenAimOriginPos.z = 0.0f;
			auto screenAimPos = Util::WorldToScreenPoint3
			(
				aimOriginPos + 
				aimDirection * 100.0f,
				false
			);
			screenAimPos.z = 0.0f;
			auto screenAimDir = screenAimPos - screenAimOriginPos;
			if (screenAimDir.Length() == 0.0f)
			{
				float camYaw = glob.cam->GetCurrentYaw();
				float yawDiff = Util::NormalizeAngToPi
				(
					camYaw - Util::DirectionToGameAngYaw(aimDirection)
				);
				// Aim down on the screen if not facing the camera's direction;
				// otherwise, aim up the screen.
				// Sign flipped due to Scaleform convention
				// (origin top left instead of bottom left).
				if (fabsf(yawDiff) >= PI / 2.0f)
				{
					screenTargetingAngle = PI / 2.0f;
				}
				else
				{
					screenTargetingAngle = 3.0f * PI / 2.0f;
				}
			}
			else
			{
				screenAimDir.Unitize();
				screenTargetingAngle = Util::NormalizeAng0To2Pi
				(
					atan2f(screenAimDir.y, screenAimDir.x)
				);	
			}
		}

		if (a_useLeftStickAngle)
		{
			worldTargetingAngle = 
			(
				p->lsMoved ? 
				p->analogStickParams[!AnalogStickParams::kLSCamRelAng] :
				coopActor->data.angle.z
			);
		}
		else
		{
			worldTargetingAngle = 
			(
				p->rsMoved ? 
				p->analogStickParams[!AnalogStickParams::kRSCamRelAng] :
				coopActor->data.angle.z
			);
		}
		
		// If the current target is not valid, clear it for comparisons below.
		if (!Util::HandleIsValid(a_currentTargetHandle) || 
			!Util::IsValidRefrForTargeting(a_currentTargetHandle.get().get()))
		{
			a_currentTargetHandle = RE::ObjectRefHandle();
		}

		// Either from the player's character or from the currently selected target.
		// Start selection from the player if the current target is offscreen,
		// since players can still select a target 1 level deep and won't have difficulty
		// fishing out the crosshair when if it is multiple jumps away from returning to
		// an onscreen position.

		if (a_selectOnHold)
		{
			shouldFindLockOnTargetFromPlayer = 
			(
				!a_fromCurrentTarget || !Util::HandleIsValid(a_currentTargetHandle)
			);
		}
		else if (!a_fromCurrentTarget || !Util::HandleIsValid(a_currentTargetHandle))
		{
			shouldFindLockOnTargetFromPlayer = true;
		}
		
		RE::TESObjectREFR* sourceRefr =
		(
			shouldFindLockOnTargetFromPlayer ?
			coopActor.get() :
			a_currentTargetHandle.get().get()
		);
		DBG
		(
			"{}: Should start from player: {}, source: {}. On hold: {}.", 
			coopActor->GetName(), 
			shouldFindLockOnTargetFromPlayer,
			sourceRefr->GetName(),
			a_selectOnHold
		);
		/*RE::TESObjectREFR* sourceRefr =
		(
			a_fromCurrentTarget && 
			!a_useLeftStickAngle &&
			Util::HandleIsValid(a_currentTargetHandle) &&
			Util::PointIsOnScreen(Util::GetRefrPosition(a_currentTargetHandle.get().get())) ?
			a_currentTargetHandle.get().get() : 
			coopActor.get()
		);*/

		auto p1 = RE::PlayerCharacter::GetSingleton();
		// To avoid checking LOS each time a new closer refr is found while looping through,
		// especially if a bunch of far-away and likely out-of-sight refrs are checked first,
		// we'll gather all the angle/distance factors for the refrs and then check LOS afterward, 
		// from the closest to the farthest refrs. 
		// That is, unless a player is the closest actor within the FOV window, then we're good,
		// and no LOS checks are required.
		std::multimap<float, RE::ObjectRefHandle> factorMap{ };
		float angDistFactor = FLT_MAX;
		// Is the actor's torso on screen?
		bool isOnScreen = false;
		// Is the actor in range and within the targeting angle's FOV window?
		bool inRangeAndFOV = false;
		if (a_asAimTarget)
		{
			// Needs polishing; not terrible, but not as accurate as I'd like it to be. 
			// Hope I can figure out how to de-clunk that junk.
			for (const auto& closeActorHandle : procLists->highActorHandles)
			{
				// Ignore non-actors, actors that cannot be targeted, and dead actors.
				auto actorPtr = Util::GetActorPtrFromHandle(closeActorHandle); 
				if (!actorPtr || 
					!Util::IsValidRefrForTargeting(actorPtr.get()) || 
					actorPtr->IsDead())
				{
					continue;
				}

				// Skip the player themselves, the player's mount, 
				// or the current crosshair target, 
				// if locking on via button press in free aim targeting mode
				// (otherwise, the target selected will change each frame).
				if ((actorPtr == coopActor || actorPtr == p->GetCurrentMount()) ||
					(
						closeActorHandle == a_currentTargetHandle
					))
				{
					continue;
				}

				// Cap the range if the target is not actively hostile 
				// so we don't select a carefree rabbit 3 holds over hiding behind a tree.
				bool isActivelyHostile = Util::IsActivelyHostileToPlayerOrAlly
				(
					actorPtr.get()
				);
				// Skip non-actively-hostile, off-screen actors.
				isOnScreen = Util::PointIsOnScreen(Util::GetTorsoPosition(actorPtr.get()));
				if (!isOnScreen && !isActivelyHostile)
				{
					continue;
				}

				// NOTE:
				// For all 'IsRefrInRangeAndInFOV' calls in this function:
				// 1. Add in the angle difference between targeting angle and angle to target
				// only if holding to select and not selecting from the current target,
				// meaning we continuously trying to select a closer target from the player
				// as the origin and can prioritize angular accuracy 
				// over moving quickly through different targets.
				// 2. Prefer screenspace positions and angles (if they are on screen only)
				// when moving from the current  target to the next in a chain. 
				// Easier to quickly move through targets without accounting for depth
				// when the camera is pitched flat.
				IsRefrInRangeAndInFOV
				(
					sourceRefr,
					actorPtr.get(),
					a_selectOnHold && !a_fromCurrentTarget,
					false,
					isActivelyHostile,
					a_fromCurrentTarget,
					screenTargetingAngle,
					worldTargetingAngle,
					Settings::vfAimCorrectionFOV[playerID],
					Settings::fMaxRaycastAndZoomOutDistance,
					angDistFactor,
					inRangeAndFOV
				);
				if (inRangeAndFOV)
				{
					// Do not prioritize grabbed NPCs.
					if (rmm->IsManaged(closeActorHandle, true))
					{
						angDistFactor = FLT_MAX;
					}

					factorMap.insert
					(
						{ angDistFactor, closeActorHandle }
					);
				}
			}
		
			// Also add P1 if the companion player is performing this check.
			if (p1 && !p->isPlayer1)
			{	
				const auto p1Handle = p1->GetHandle();	
				if (p1Handle != a_currentTargetHandle)
				{
					// Perform new closest actor in FOV check on P1.
					IsRefrInRangeAndInFOV
					(
						sourceRefr,
						p1,
						a_selectOnHold && !a_fromCurrentTarget,
						false,
						false,
						a_fromCurrentTarget,
						screenTargetingAngle,
						worldTargetingAngle,
						Settings::vfAimCorrectionFOV[playerID],
						Settings::fMaxRaycastAndZoomOutDistance,
						angDistFactor,
						inRangeAndFOV
					);
					// Must be in range and within FOV window to insert.
					if (inRangeAndFOV)
					{
						if (rmm->IsManaged(p1Handle, true))
						{
							angDistFactor = FLT_MAX;
						}

						factorMap.insert({ angDistFactor, p1Handle });
					}
				}
			}
		}
		else
		{
			Util::ForEachReferenceInRange
			(
				playerTorsoPos, maxCheckDist, true,
				[
					this, 
					sourceRefr,
					&a_currentTargetHandle,
					&a_fromCurrentTarget,
					&a_selectOnHold,
					&playerTorsoPos,
					&screenTargetingAngle,
					&worldTargetingAngle,
					&maxCheckDist,
					&isOnScreen,
					&inRangeAndFOV,
					&angDistFactor,
					&factorMap
				]
				(RE::TESObjectREFR* a_refr) 
				{
					// On to the next one.
					if (!a_refr || 
						!Util::HandleIsValid(a_refr->GetHandle()) || 
						!a_refr->IsHandleValid())
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}
					
					const auto handle = a_refr->GetHandle();
					auto baseObj = a_refr->GetBaseObject();
					// On to the next one x2.
					if (!baseObj || 
						!a_refr->Is3DLoaded() || 
						!a_refr->GetCurrent3D() ||
						a_refr->IsDeleted() || 
						!Util::IsValidRefrForTargeting(a_refr) ||
						!Util::IsSelectableRefr(a_refr) ||
						handle.get() == p->GetCurrentMount() ||
						handle == a_currentTargetHandle ||
						handle == coopActor->GetHandle()) 
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}
					
					isOnScreen = Util::PointIsOnScreen(Util::GetRefrPosition(a_refr));
					if (!isOnScreen)
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					IsRefrInRangeAndInFOV
					(
						sourceRefr,
						a_refr,
						a_selectOnHold && !a_fromCurrentTarget,
						false,
						false,
						false,
						screenTargetingAngle,
						worldTargetingAngle,
						Settings::vfAimCorrectionFOV[playerID],
						maxCheckDist,
						angDistFactor,
						inRangeAndFOV
					);

					// Must be in range and within FOV window to insert.
					if (inRangeAndFOV)
					{
						// Do not prioritize grabbed objects/NPCs.
						if (rmm->IsManaged(handle, true))
						{
							angDistFactor = FLT_MAX;
						}

						factorMap.insert({ angDistFactor, handle });
					}
					
					return RE::BSContainer::ForEachResult::kContinue;
				}
			);

			// Also add P1 if the companion player is performing this check.
			if (p1 && !p->isPlayer1)
			{	
				const auto p1Handle = p1->GetHandle();	
				if (p1Handle != a_currentTargetHandle)
				{
					// Perform new closest actor in FOV check on P1.
					IsRefrInRangeAndInFOV
					(
						sourceRefr,
						p1,
						a_selectOnHold && !a_fromCurrentTarget,
						false,
						false,
						false,
						screenTargetingAngle,
						worldTargetingAngle,
						Settings::vfAimCorrectionFOV[playerID],
						maxCheckDist,
						angDistFactor,
						inRangeAndFOV
					);
					// Must be in range and within FOV window to insert.
					if (inRangeAndFOV)
					{
						// Do not prioritize grabbed objects/NPCs.
						if (rmm->IsManaged(p1Handle, true))
						{
							angDistFactor = FLT_MAX;
						}

						factorMap.insert({ angDistFactor, p1Handle });
					}
				}
			}
		}

		// If there are no other refrs to consider, 
		// return the current target to maintain it if it is still targetable.
		/*if (factorMap.empty() && 
			Util::HandleIsValid(a_currentTargetHandle) &&
			Util::IsValidRefrForTargeting(a_currentTargetHandle.get().get()) &&
			!rmm->IsManaged(a_currentTargetHandle, true))
		{
			return a_currentTargetHandle;
		}*/

		// FOV check(s) before settling on closest in-FOV refr.
		// Want to restart the selection chain if LOS checks fail and targeting with the left stick,
		// but maintain the current selection if LOS checks fail when using the right stick.
		RE::ObjectRefHandle closestRefrHandle = RE::ObjectRefHandle();
		bool choseLastOption = false;
		const float playerPixelHeight = Util::GetBoundMaxOrMinEdgeDist(coopActor.get(), true, true);
		for (auto iter = factorMap.begin(); iter != factorMap.end(); ++iter)
		{
			const auto& [factor, refrHandle] = *iter;
			const auto refrPtr = Util::GetRefrPtrFromHandle(refrHandle);
			if (!refrPtr)
			{
				continue;
			}

			// Skip non-actors if choosing an aim target.
			if (a_asAimTarget)
			{
				const auto asActor = refrPtr->As<RE::Actor>();
				if (!asActor)
				{
					continue;
				}

				// No FOV checks necessary for players. Select and break.
				if (GlobalCoopData::IsCoopPlayer(asActor))
				{
					DBG
					(
						"{}: Selected a fellow adventurer, {}, factor: {}", 
						coopActor->GetName(), asActor->GetName(), factor
					);
					closestRefrHandle = refrHandle;
					break;
				}
			}

			const float pixelHeight = Util::GetBoundMaxOrMinEdgeDist(refrPtr.get(), true, true);
			bool hasLOS = Util::HasLOS
			(
				refrPtr.get(), coopActor.get(), true, false, crosshairWorldPos
			);
			const float refrToPlayerPixelHeightRatio = pixelHeight / playerPixelHeight;
			const float refrToPlayerHeightRatio = 
			(
				refrPtr->GetHeight() == 0.0f ? 
				1.0f :
				coopActor->GetHeight() == 0.0f ?
				1.0f :
				refrPtr->GetHeight() / coopActor->GetHeight()
			);
			// Not beyond one cell diagonal beyond the camera's distance to the origin point,
			// or larger than a tenth of the player's pixel height.
			// First check allows for wider consideration as the camera zooms out.
			// Second check allows for selection of larger NPCs even beyond the above range,
			// think selecting a far away, and visible, dragon that is flying away from the player.
			// Third factor still allows for selection of NPCs that are discernible enough, 
			// pixel-wise, on the screen.
			const bool inRange = 
			(
				fabsf
				(
					glob.cam->camTargetPos.GetDistance(glob.cam->camOriginPoint) - 
					glob.cam->camOriginPoint.GetDistance(Util::GetRefrPosition(refrPtr.get()))
				) <= 4096.0f * sqrtf(2.0f) ||
				pixelHeight >= 0.2f * playerPixelHeight ||
				pixelHeight > DebugAPI::screenResY / 60.0f
			);
			DBG
			(
				"{}: Considering {} (0x{:X}). Has LOS: {}, factor: {}, pixel heights: {}, {} "
				"({}, screen height: {}, ratio: {}), "
				"world height ratio (player / refr): {}. Height factor: {}, "
				"cam dist to origin, target dist to origin: {}, {}, diff: {}, YEE: {}.", 
				coopActor->GetName(), 
				refrPtr->GetName(),
				refrPtr->formID,
				hasLOS,
				factor,
				pixelHeight,
				playerPixelHeight,
				refrToPlayerPixelHeightRatio,
				DebugAPI::screenResY,
				pixelHeight / DebugAPI::screenResY,
				refrToPlayerHeightRatio,
				refrToPlayerHeightRatio >= 0.1f ?
				refrToPlayerPixelHeightRatio > 0.1f :
				refrToPlayerPixelHeightRatio > refrToPlayerHeightRatio,
				glob.cam->camTargetPos.GetDistance(glob.cam->camOriginPoint),
				glob.cam->camOriginPoint.GetDistance(Util::GetRefrPosition(refrPtr.get())),
				glob.cam->camTargetPos.GetDistance(glob.cam->camOriginPoint) - 
				glob.cam->camOriginPoint.GetDistance(Util::GetRefrPosition(refrPtr.get())),
				inRange
			);
			if (hasLOS && inRange)
			{
				closestRefrHandle = refrHandle;
				choseLastOption = ++iter == factorMap.end();
				break;
			}
		}
		
		// If failing to select a new target or if the chosen target 
		// is the last selectable target in the chain, 
		// restart the selection chain from the player next time.
		// Only when not selected via holding a bind, as while holding a bind,
		// its easy to jump back to selecting a target.
		if (!a_selectOnHold)
		{
			shouldFindLockOnTargetFromPlayer = 
			(
				!a_fromCurrentTarget || 
				!Util::HandleIsValid(closestRefrHandle) ||
				choseLastOption
			);
		}
		
		DBG
		(
			"{}: Should start from player: {}, closest refr: {}, choices: {}, chose last: {}", 
			coopActor->GetName(), 
			shouldFindLockOnTargetFromPlayer,
			Util::HandleIsValid(closestRefrHandle) ? 
			closestRefrHandle.get()->GetName() : 
			"NONE",
			factorMap.size(),
			choseLastOption
		);
		// If there is no new target and the previous target is valid, return it.
		// Will restart selection from the player on the next selection attempt.
		if (!Util::HandleIsValid(closestRefrHandle) && 
			Util::HandleIsValid(a_currentTargetHandle) &&
			Util::IsValidRefrForTargeting(a_currentTargetHandle.get().get()) &&
			!rmm->IsManaged(a_currentTargetHandle, true))
		{
			DBG
			(
				"{}: Chose current: {}", 
				coopActor->GetName(), 
				a_currentTargetHandle.get()->GetName()
			);
			return a_currentTargetHandle;
		}
		
		return closestRefrHandle;
	}

	std::vector<RE::ObjectRefHandle> TargetingManager::GetLootableRefrsInRange
	(
		bool a_containersOnly, const uint32_t& a_maxLOSChecks
	)
	{
		// Get a list of reachable, lootable refrs' handles in range of the player.
		// Can return a list of loose refrs' handles 
		// or a list of lootable containers' handles.
		// Max number of LOS checks for can be specified, 
		// since LOS checks for each refr can get expensive.
	
		// Clear old nearby objects of the same type before refreshing.
		nearbyObjectsOfSameType.clear();
		// Player wants to steal objects when sneaking.
		bool canSteal = coopActor->IsSneaking();
		// Number of LOS checks performed.
		uint32_t losChecksPerformed = 0;
		// Check each refr in range.
		Util::ForEachReferenceInRange
		(
			p->mm->playerTorsoPosition, GetMaxActivationDist(), true,
			[&](RE::TESObjectREFR* a_refr) 
			{
				// Ensure that the object reference is an interactable object, 
				// not a crime to activate, and lootable or a container

				// Stop checking once the max number of LOS checks have been performed.
				if (losChecksPerformed > a_maxLOSChecks)
				{
					return RE::BSContainer::ForEachResult::kStop;
				}

				// Refrs without a loaded name are usually statics or other uninteractable objects.
				if (!a_refr || 
					!Util::HandleIsValid(a_refr->GetHandle()) || 
					!a_refr->IsHandleValid() || 
					!a_refr->Is3DLoaded() || 
					!a_refr->GetCurrent3D() || 
					a_refr->IsDeleted() ||
					strlen(a_refr->GetName()) == 0) 
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				auto baseObj = a_refr->GetBaseObject();
				// Lootable refr or unlocked container/corpse 
				// and either the player is choosing to steal the object 
				// or the object is not a crime to activate.
				bool canLoot = 
				(
					(canSteal || !Util::ActivationIsOffLimits(coopActor.get(), a_refr)) && 
					(
						(
							(a_containersOnly && a_refr->HasContainer() && !a_refr->IsLocked()) &&
							(!a_refr->As<RE::Actor>() || a_refr->IsDead())
						) ||
						(
							!a_containersOnly && 
							Util::IsLootableRefr(a_refr) && 
							!a_refr->HasContainer()
						) 
					)
				);
				if (canLoot)
				{
					++losChecksPerformed;
					if (Util::HasLOS(a_refr, coopActor.get(), false, false, crosshairWorldPos))
					{
						nearbyObjectsOfSameType.emplace_back(a_refr);
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		return nearbyObjectsOfSameType;
	}


	const std::vector<RE::ObjectRefHandle>& TargetingManager::GetNearbyRefrsOfSameType
	(
		RE::ObjectRefHandle a_refrHandle, 
		const uint32_t& a_maxLOSChecks,
		RefrCompType&& a_compType
	)
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
		
		auto refrBaseObject = refrPtr->GetBaseObject();
		// Player wants to steal objects when sneaking.
		bool canSteal = coopActor->IsSneaking();
		// Number of LOS checks performed.
		uint32_t losChecksPerformed = 0;
		// Check each refr in range.
		Util::ForEachReferenceInRange
		(
			p->mm->playerTorsoPosition, GetMaxActivationDist(), true,
			[&](RE::TESObjectREFR* a_refr) 
			{
				// Ensure that the object reference is an interactable object, 
				// not a crime to activate,
				// and of the same form type as requested. 

				// Stop checking once the max number of LOS checks have been performed.
				if (losChecksPerformed > a_maxLOSChecks)
				{
					return RE::BSContainer::ForEachResult::kStop;
				}

				// Refrs without a loaded name are usually statics or other uninteractable objects.
				if (!a_refr || 
					!Util::HandleIsValid(a_refr->GetHandle()) || 
					!a_refr->IsHandleValid() || 
					!a_refr->Is3DLoaded() || 
					!a_refr->GetCurrent3D() || 
					a_refr->IsDeleted() ||
					strlen(a_refr->GetName()) == 0) 
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				auto baseObj = a_refr->GetBaseObject();
				// Lootable and either the player is choosing to steal the object 
				// or the object is not a crime to activate.
				if ((canSteal || !Util::ActivationIsOffLimits(coopActor.get(), a_refr)) && 
					(Util::IsLootableRefr(a_refr)))
				{
					bool sameType = false;
					if (a_compType == RefrCompType::kSameBaseForm)
					{
						sameType = baseObj && refrBaseObject && baseObj == refrBaseObject;
					}
					else
					{
						sameType = 
						(
							baseObj && 
							refrBaseObject && 
							*baseObj->formType == *refrBaseObject->formType
						);
					}


					if (sameType)
					{
						++losChecksPerformed;
						if (Util::HasLOS(a_refr, coopActor.get(), false, false, crosshairWorldPos))
						{
							nearbyObjectsOfSameType.emplace_back(a_refr);
						}
					}
				}

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);

		return nearbyObjectsOfSameType;
	}

	RE::ObjectRefHandle TargetingManager::GetRangedPackageTargetRefr
	(
		RE::TESForm* a_rangedAttackSource
	)
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

		bool hasSelectedTargetActor = 
		{
			Util::HandleIsValid(selectedTargetActorHandle)
		};
		bool hasAimCorrectionTarget = 
		(
			Util::HandleIsValid(aimCorrectionTargetHandle)
		);
		auto spell = a_rangedAttackSource->As<RE::SpellItem>();
		// Rules:
		// - Target self if for self-targeting spells.
		// - If a spell targets an actor, choose the crosshair-selected actor, 
		// aim correction actor, or the closest actor in the player's FOV.
		// - For touch range spells, choose the closest actor in touch range
		// in front of the player.
		if (spell && spell->GetDelivery() == Delivery::kSelf)
		{
			targetHandle = coopActor->GetHandle();
		}
		else if (spell && spell->GetDelivery() == Delivery::kTouch)
		{
			// If in range, choose the already-selected or aim correction target actor.
			bool cachedTargetChosen = false;
			float distToTarget = FLT_MAX;
			if (hasSelectedTargetActor) 
			{
				distToTarget = p->mm->playerTorsoPosition.GetDistance
				(
					Util::GetTorsoPosition(selectedTargetActorHandle.get().get())
				);
				if (distToTarget <= maxReachActivationDist)
				{
					targetHandle = selectedTargetActorHandle;
					cachedTargetChosen = true;
				}
			}
			else if (hasAimCorrectionTarget)
			{
				distToTarget = p->mm->playerTorsoPosition.GetDistance
				(
					Util::GetTorsoPosition(aimCorrectionTargetHandle.get().get())
				);
				if (distToTarget <= maxReachActivationDist)
				{
					targetHandle = aimCorrectionTargetHandle;
					cachedTargetChosen = true;
				}
			}

			// Fallback:
			// If neither the selected target actor or aim correction target are valid, 
			// select a new target actor.
			if (!cachedTargetChosen)
			{
				// Get the closest, non-blacklisted actor in a 180 degree arc 
				// in front of the player and within reach distance.
				// Use XY distance to ignore vertical displacements.
				targetHandle = GetClosestTargetableActorInFOV
				(
					coopActor.get(), true, true, true, false, false, PI, maxReachActivationDist
				);
			}
		}
		else if (spell && spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetActor)
		{
			// If a cached target actor is already available, choose it.
			if (hasSelectedTargetActor) 
			{
				targetHandle = selectedTargetActorHandle;
			}
			else if (hasAimCorrectionTarget)
			{
				targetHandle = aimCorrectionTargetHandle;
			}
			else
			{
				// Otherwise, get the closest, non-blacklisted actor 
				// in a 180 degree arc in front of the player.
				// Use XY distance to ignore vertical displacements.
				targetHandle = GetClosestTargetableActorInFOV
				(
					coopActor.get(), true, true, true, false, false, PI, -1.0f
				);
			}
		}
		else if (hasSelectedTargetActor)
		{
			targetHandle = selectedTargetActorHandle;
		}
		else if (hasAimCorrectionTarget)
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

		if (Util::HandleIsValid(selectedTargetActorHandle))
		{
			return selectedTargetActorHandle;
		}
		else
		{
			bool hasAimCorrectionTarget = 
			(
				(
					aimMode == AimMode::kTwinStick &&
					Util::HandleIsValid(aimCorrectionTargetHandle)
				) ||
				(
					Settings::vbUseAimCorrection[playerID] &&
					Util::HandleIsValid(aimCorrectionTargetHandle) &&
					!p->tm->crosshairActive
				)
			);
			if (hasAimCorrectionTarget)
			{
				return aimCorrectionTargetHandle;
			}
			else if (!p->tm->crosshairActive && !Settings::vbUseAimCorrection[playerID])
			{
				// NOTE:
				// Will comment out if bugs occur.
				// If not facing the crosshair, not using aim correction,
				// and the player is running their ranged attack package, 
				// choose the aim target linked refr, if available.
				// Ignore the player if they are the linked refr, 
				// since this is the fall-through case that simply 
				// enables the ranged attack package to execute.
				const auto refrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle); 
				if (refrPtr && refrPtr->As<RE::Actor>() && refrPtr != coopActor)
				{
					if (p->pam->GetCurrentPackage() ==
						p->pam->GetCoopPackage(PackageIndex::kRangedAttack))
					{
						return refrPtr->As<RE::Actor>()->GetHandle();
					}
					else
					{
						// Clear out ranged target linked refr if not running the ranged package.
						ClearTarget(TargetActorType::kLinkedRefr);
					}
				}
			}
		}
		
		// No targeted actor or targeting self (not considered own ranged target).
		return RE::ActorHandle();
	}

	RE::ObjectRefHandle TargetingManager::GetSelectableProximityRefrHandle(bool a_quickSelection)
	{
		// Choose a valid nearby refr to use for activation.
		// Stricter conditions for what objects/NPCs are selectable 
		// when for quick selection/activation.
		// Done to prevent accidental or unnecessary activation.

		const auto& playerTorsoPos = p->mm->playerTorsoPosition;
		// Handle to return.
		RE::ObjectRefHandle selectedRefrHandle = RE::ObjectRefHandle();
		// Clear out crosshair pick handle, which will be updated below if valid.
		crosshairPickRefrHandle = RE::ObjectRefHandle();
		const auto currentMount = p->GetCurrentMount();
		const auto& lsAngle = p->analogStickParams[!AnalogStickParams::kLSCamRelAng];
		// Re-populate nearby references if needed.
		bool orientationChanged = 
		(
			p->lsMoved || 
			fabsf
			(
				Util::NormalizeAngToPi(lsAngle - lastActivationFacingAngle)
			) > 
			Settings::fMinTurnAngToRefreshRefrs	
		);
		if (nearbyReferences.empty() || orientationChanged)
		{
			// Clear out any cached objects.
			nearbyReferences.clear();
			// Player heading angle in Cartesian convention.
			const float convLSAngle = Util::ConvertAngle(lsAngle);
			// Player's facing direction in the XY plane (yaw direction).
			RE::NiPoint3 movingDirXY = Util::RotationToDirectionVect(0.0f, convLSAngle);
			movingDirXY.Unitize();
			// Max activation reach distance.
			const float maxCheckDist = GetMaxActivationDist();
			// Used to check if activation cycling has started.
			const float& secsSinceActivationStarted = 
			(
				p->pam->paStatesList
				[!InputAction::kActivate - !InputAction::kFirstAction].secsPerformed
			);
			// Get each reference within range and record the magnitude of
			// its angular distance from the player's facing angle.
			Util::ForEachReferenceInRange
			(
				playerTorsoPos, maxCheckDist, true,
				[
					this, 
					&currentMount, 
					&playerTorsoPos, 
					&movingDirXY, 
					&maxCheckDist,
					&secsSinceActivationStarted,
					&a_quickSelection
				]
				(RE::TESObjectREFR* a_refr) 
				{
					// On to the next one.
					// Either the refr does not exist, is the player themselves, is their mount,
					// or is the furniture they are interacting with already.
					if ((!a_refr ||
						!Util::HandleIsValid(a_refr->GetHandle()) || 
						a_refr == coopActor.get()) ||
						(currentMount && a_refr == currentMount.get()) ||
						(coopActor->GetOccupiedFurniture() == a_refr->GetHandle()))
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					const auto handle = a_refr->GetHandle();
					auto baseObj = a_refr->GetBaseObject();
					// On to the next one x2.
					if (!baseObj) 
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					const auto modFile = baseObj->GetFile();
					// REMOVE when done debugging.
					const auto modFile2 = baseObj->GetFile(0);
					DBG
					(
						"{}: Activation candidate {} ({}, 0x{:X}, 0x{:X}) from mod {} ({}).", 
						coopActor->GetName(),
						a_refr->GetName(),
						Util::GetEditorID(baseObj),
						a_refr->formID,
						baseObj->formID,
						modFile ? modFile->fileName : "NONE",
						modFile2 ? modFile2->fileName : "NONE"
					);
					// EW. Don't know how else to tell if a mod-placed activator is from EVGAT.
					// Ignore, since these activators should not be activated by P2 through P1,
					// and activation can cause weird alignment issues anyways.
					bool isEVGATActivator = 
					(
						baseObj &&
						baseObj->Is
						(
							RE::FormType::Activator, RE::FormType::TalkingActivator
						) && 
						modFile && 
						std::string(modFile->fileName).find("EVG") != std::string::npos
					);
					// Skip EVG activators and non-selectable/targetable refrs.
					if (isEVGATActivator ||
						!Util::IsValidRefrForTargeting(a_refr) ||
						!Util::IsSelectableRefr(a_refr))
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}

					if (a_quickSelection)
					{
						// If not choosing a lock on activation target,
						// skip the currently selected target when activation cycling.
						if (startedActivationCycling && handle == activationRefrHandle)
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}

						// Also skip other players, or activating hostile actors 
						// that are not pacifiable or interactable while hostile 
						// (not a guard, mount, or normally hostile).
						auto asActor = a_refr->As<RE::Actor>();
						const bool isFriendly = Util::IsPartyFriendlyActor(asActor);
						const bool hostileToP1 = 
						(
							asActor && asActor->IsHostileToActor(glob.player1Actor.get())
						);
						const bool hostileToThisPlayer = 
						(
							asActor && asActor->IsHostileToActor(coopActor.get())
						);
						// Useless to activate hostile actors in combat.
						const bool activateHostileActor = 
						{ 
							(hostileToP1 || hostileToThisPlayer) &&
							(
								asActor && 
								!asActor->IsDead() && 
								!Util::CanStopCombatWithActor(asActor)
							)
						};

						// Do not consider friendly actors that are not mad at a player or 
						// do not need help getting up, and are not selected as a target.
						const bool friendlyActorNotActivatable = 
						(
							isFriendly &&
							!asActor->IsBleedingOut() &&
							!asActor->IsInRagdollState() &&
							!hostileToP1 &&
							!hostileToThisPlayer &&
							handle != aimCorrectionTargetHandle &&
							handle != crosshairRefrHandle
						);
						if (friendlyActorNotActivatable ||
							activateHostileActor || 
							glob.coopEntityBlacklistFIDSet.contains(a_refr->formID))
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}
					}

					auto refr3DPtr = Util::GetRefr3D(a_refr); 
					const auto niCamPtr = Util::GetNiCamera();
					// Skip refrs that are not within a 180 degree FOV cone 
					// in the player's facing direction and not on screen.
					// Do not want to select a door or furniture, for example,
					// that is behind the player or the camera.
					bool allPositionsBehindPlayer = true;
					bool onePositionBehindCamera = 
					/*(
						!Util::PointIsOnScreen(Util::Get3DCenterPos(a_refr))
					);*/
					(
						refr3DPtr && 
						niCamPtr &&
						!RE::NiCamera::BoundInFrustum
						(
							refr3DPtr->worldBound, niCamPtr.get()
						)
					);
					float facingToRefrDot = 0.0f;

					// Check three points on the refr for the best measurement 
					// of where the refr is located relative to the player.
					// Start with the reported refr location.
					RE::NiPoint3 refrLoc1 = a_refr->data.location;
					//onePositionBehindCamera |= !Util::PointIsOnScreen(refrLoc1);
					RE::NiPoint3 toRefrDirXY = refrLoc1 - playerTorsoPos;
					toRefrDirXY.z = 0.0f;
					toRefrDirXY.Unitize();

					// Minimum selection factor [0, 2]. 
					// Get the minimum factor among the (potentially) three refr positions.
					// Negate the dot product, meaning the more the player has to turn 
					// to face the object, the larger the factor.
					// Then we add 1 to ensure all dot product results are > 0, 
					// and mult by 0.5 to set the range to [0, 1]
					// Lastly scale by the distance from the player to the object,
					// meaning objects that are further away have a larger factor.
					// Divide by max reach distance to set range to [0, 1]
					facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
					if (facingToRefrDot >= 0.0f)
					{
						allPositionsBehindPlayer = false;
					}

					float minSelectionFactor = 
					(
						(0.5f * (1.0f - facingToRefrDot)) +
						(playerTorsoPos.GetDistance(refrLoc1) / maxCheckDist)
					);

					// Next two positions only exist if the refr's 3D is available.
					std::optional<RE::NiPoint3> refrLoc2 = std::nullopt;
					std::optional<RE::NiPoint3> refrLoc3 = std::nullopt;
					if (refr3DPtr)
					{
						refrLoc2 = refr3DPtr->world.translate;
						refrLoc3 = refr3DPtr->worldBound.center;
					}

					// Refr 3D world position.
					if (refrLoc2.has_value())
					{
						toRefrDirXY = refrLoc2.value() - playerTorsoPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();
						facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
						if (allPositionsBehindPlayer && facingToRefrDot >= 0.0f)
						{
							allPositionsBehindPlayer = false;
						}

						float selectionFactor = 
						(
							(0.5f * (1.0f - facingToRefrDot)) +
							(playerTorsoPos.GetDistance(refrLoc2.value()) / maxCheckDist)
						);
						if (selectionFactor < minSelectionFactor) 
						{
							minSelectionFactor = selectionFactor;
						}
					}

					// Refr 3D bound center position.
					if (refrLoc3.has_value())
					{
						toRefrDirXY = refrLoc3.value() - playerTorsoPos;
						toRefrDirXY.z = 0.0f;
						toRefrDirXY.Unitize();
						facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
						if (allPositionsBehindPlayer && facingToRefrDot >= 0.0f)
						{
							allPositionsBehindPlayer = false;
						}

						float selectionFactor = 
						(
							(0.5f * (1.0f - facingToRefrDot)) + 
							(playerTorsoPos.GetDistance(refrLoc3.value()) / maxCheckDist)
						);
						if (selectionFactor < minSelectionFactor)
						{
							minSelectionFactor = selectionFactor;
						}
					}
					
					// Player is not turned towards the object if its refr data, 3D,
					// and 3D center positions are behind the player.
					if (allPositionsBehindPlayer || onePositionBehindCamera)
					{
						return RE::BSContainer::ForEachResult::kContinue;
					}
					
					// Increase factor to push grabbed refrs to the back of the references map.
					// Do not want to prioritize selecting grabbed refrs instead of other refrs 
					// in front of the player.
					if (rmm->IsManaged(handle, true))
					{
						minSelectionFactor *= 2.0f;
					}

					/*DBG("Add {} (0x{:X}), factor {}.", 
						a_refr->GetName(), a_refr->formID, minSelectionFactor);*/
					nearbyReferences.insert
					(
						std::pair<float, RE::ObjectRefHandle>
						(
							minSelectionFactor, a_refr->GetHandle()
						)
					);

					return RE::BSContainer::ForEachResult::kContinue;
				}
			);
				
			// Add the game's crosshair pick refr if any.
			if (auto pickData = RE::CrosshairPickData::GetSingleton(); pickData)
			{
				auto pickRefrPtr = Util::GetRefrPtrFromHandle(pickData->target); 
				// Must be valid for targeting.
				if (pickRefrPtr && Util::IsValidRefrForTargeting(pickRefrPtr.get()))
				{
					// Must be in range of this player still.
					float distToRefr = playerTorsoPos.GetDistance
					(
						Util::Get3DCenterPos(pickRefrPtr.get())
					);
					if (distToRefr < maxCheckDist)
					{
						const bool blacklisted =
						(
							(currentMount && pickRefrPtr == currentMount) ||
							(
								pickRefrPtr->As<RE::Actor>() && 
								pickRefrPtr->As<RE::Actor>()->IsPlayerTeammate()
							) ||
							(glob.coopEntityBlacklistFIDSet.contains(pickRefrPtr->formID))
						);
						if (!blacklisted) 
						{
							// Skip if not within a 180 degree FOV cone 
							// in the player's facing direction.
							// Do not want to select a door or furniture, for example,
							// that is behind the player.
							bool allPositionsBehindPlayer = true;
							float facingToRefrDot = 0.0f;
							// Same three tests as for the nearby refrs above.
							RE::NiPoint3 refrLoc1 = pickRefrPtr->data.location;
							RE::NiPoint3 toRefrDirXY = refrLoc1 - playerTorsoPos;
							toRefrDirXY.z = 0.0f;
							toRefrDirXY.Unitize();
							facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
							if (facingToRefrDot >= 0.0f)
							{
								allPositionsBehindPlayer = false;
							}

							float minSelectionFactor = 
							(
								(0.5f * (1.0f - facingToRefrDot)) +
								(playerTorsoPos.GetDistance(refrLoc1) / maxCheckDist)
							);
							std::optional<RE::NiPoint3> refrLoc2 = std::nullopt;
							std::optional<RE::NiPoint3> refrLoc3 = std::nullopt;
							auto refr3DPtr = Util::GetRefr3D(pickRefrPtr.get()); 
							if (refr3DPtr)
							{
								refrLoc2 = refr3DPtr->world.translate;
								refrLoc3 = refr3DPtr->worldBound.center;
							}

							if (refrLoc2.has_value())
							{
								toRefrDirXY = refrLoc2.value() - playerTorsoPos;
								toRefrDirXY.z = 0.0f;
								toRefrDirXY.Unitize();
								facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
								if (allPositionsBehindPlayer && facingToRefrDot >= 0.0f)
								{
									allPositionsBehindPlayer = false;
								}

								float selectionFactor = 
								(
									(0.5f * (1.0f - facingToRefrDot)) +
									(playerTorsoPos.GetDistance(refrLoc2.value()) / maxCheckDist)
								);
								if (selectionFactor < minSelectionFactor) 
								{
									minSelectionFactor = selectionFactor;
								}
							}

							if (refrLoc3.has_value())
							{
								toRefrDirXY = refrLoc3.value() - playerTorsoPos;
								toRefrDirXY.z = 0.0f;
								toRefrDirXY.Unitize();
								facingToRefrDot = movingDirXY.Dot(toRefrDirXY);
								if (allPositionsBehindPlayer && facingToRefrDot >= 0.0f)
								{
									allPositionsBehindPlayer = false;
								}

								float selectionFactor = 
								(
									(0.5f * (1.0f - facingToRefrDot)) + 
									(playerTorsoPos.GetDistance(refrLoc3.value()) / maxCheckDist)
								);
								if (selectionFactor < minSelectionFactor)
								{
									minSelectionFactor = selectionFactor;
								}
							}

							// Player is not turned towards the object if its refr data, 3D,
							// and 3D center positions are behind the player.
							if (!allPositionsBehindPlayer)
							{
								// Save pick data refr handle.
								crosshairPickRefrHandle = pickData->target;
								/*DBG
								(
									"Add pick refr {} (0x{:X}), factor {}.", 
									pickRefrPtr->GetName(),
									pickRefrPtr->formID,
									minSelectionFactor
								);*/
								nearbyReferences.insert
								(
									std::pair<float, RE::ObjectRefHandle>
									(
										minSelectionFactor, crosshairPickRefrHandle
									)
								);
							}
						}
					}
				}
			}
		}
		
		// Get next selectable refr in view of the camera and remove it from the map.
		while (!nearbyReferences.empty())
		{
			auto nextRefrNodeHandle = nearbyReferences.extract(nearbyReferences.begin());
			if (nextRefrNodeHandle.empty())
			{
				continue;
			}
			
			// Do not select invalid refrs or refrs not in view of the camera or any player.
			// NOTE: 
			// These refrs, aside from the crosshair pick refr, 
			// are not directly selected with a player's crosshair,
			// so LOS still has to be checked.
			const auto& nextRefrHandle = nextRefrNodeHandle.mapped();
			auto nextRefrPtr = Util::GetRefrPtrFromHandle(nextRefrHandle); 
			if (!nextRefrPtr) 
			{
				continue;
			}

			// Finally set the proximity refr if the player has LOS on the refr.
			if (nextRefrHandle == crosshairPickRefrHandle || 
				Util::HasLOS
				(
					nextRefrPtr.get(), coopActor.get(), false, false, crosshairWorldPos
				))
			{
				selectedRefrHandle = nextRefrHandle;
				break;
			}
		}

		// Update activation cycling orientation.
		lastActivationReqPos = coopActor->data.location;
		lastActivationFacingAngle = lsAngle;
		return selectedRefrHandle;
	}

	void TargetingManager::HandleBonk
	(
		RE::ActorHandle a_hitActorHandle, 
		RE::ObjectRefHandle a_releasedRefrHandle,
		float a_collidingMass,
		float a_fallHeight,
		const RE::NiPoint3& a_collidingVelocity,
		const RE::NiPoint3& a_contactPos,
		bool a_shouldRagdoll
	)
	{
		// Apply damage to the given hit actor based on the physical properties 
		// of both the given released refr and the hit actor.

		auto hitActorPtr = Util::GetActorPtrFromHandle(a_hitActorHandle);
		auto releasedRefrPtr = Util::GetRefrPtrFromHandle(a_releasedRefrHandle);
		// The hit actor and released refr must be valid.
		if (!hitActorPtr || !releasedRefrPtr)
		{
			return;
		}

		// Set minimum mass to 1.
		if (a_collidingMass == 0.0f)
		{
			a_collidingMass = 1.0f;
		}

		// Get havok impact speed from the given colliding velocity, 
		// with refr linear speed as a fallback.
		float havokImpactSpeed = a_collidingVelocity.Length() * GAME_TO_HAVOK;
		if (havokImpactSpeed == 0.0f) 
		{
			RE::NiPoint3 linVel{ };
			releasedRefrPtr->GetLinearVelocity(linVel);
			havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
		}

		auto asActor = releasedRefrPtr->As<RE::Actor>();
		float actorWeight = 0.0f;
		float releasedRefrWeight = max(0.1f, releasedRefrPtr->GetWeight());
		const auto releasedPIndex = GlobalCoopData::GetCoopPlayerIndex(releasedRefrPtr);
		float inventoryWeight = 
		(
			releasedPIndex <= 0 ?
			releasedRefrPtr->GetWeightInContainer() :
			glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetWeightInContainer()
		);
		// Set power attack, bonk, and potentially the sneak attack flags
		// before sending a hit event.
		RE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> hitFlags{ };
		hitFlags.set
		(
			RE::TESHitEvent::Flag::kPowerAttack, 
			static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kBonk)
		);
		// Multiplier to apply for a sneak bonk.
		float sneakMult = 
		(
			coopActor->IsSneaking() && detectionPct < 100.0f ? 
			max(2.0f, coopActor->GetActorValue(RE::ActorValue::kAttackDamageMult)) : 
			1.0f
		);
		if (sneakMult > 1.0f)
		{
			hitFlags.set(RE::TESHitEvent::Flag::kSneakAttack);
		}

		// Criteria for damageable actors:
		// Not a ghost or invulnerable.
		float damage = 0.0f;
		if (!hitActorPtr->IsGhost() && !hitActorPtr->IsInvulnerable())
		{
			// 4x damage at player level 100.
			float levelDamageFactor = 
			(
				1.0f + 3.0f * max(coopActor->GetLevel() - 1.0f, 0.0f) / 99.0f
			);
			// Higher armor rating -> less damage taken.
			// 1 / 10 the damage at an armor rating of 100.
			float armorRatingFactor = std::clamp
			(
				-0.009f * hitActorPtr->CalcArmorRating() + 1.0f,
				0.1f,
				1.0f
			);
			// Scale up damage if the flopping actor is close to 
			// or exceeding their base carryweight.
			float equipmentWeightFactor = 1.0f;
			// Scale up actor-actor collision damage based on the released actor's base, 
			// equipped, and potentially inventory weight.
			if (asActor) 
			{
				float equippedWeight = asActor->GetEquippedWeight();
				// Scale up damage based on the player's inventory weight
				// relative to their base carryweight. 
				// The more over-encumbered the merrier.
				// sqrtf(2)x damage at full encumberance.
				float baseCarryWeight = coopActor->GetBaseActorValue
				(
					RE::ActorValue::kCarryWeight
				);
				equipmentWeightFactor = 
				(
					1.0f +
					(
						(sqrtf(2.0f) - 1.0f) * 
						(inventoryWeight / max(baseCarryWeight, 1.0f))
					)
				);

				// 1x damage at 0 equip weight,
				// ~sqrtf(2) damage at 100 equip weight, 
				// approaching 2x at infinite weight.
				// Multiplying the intentory and equipped weight factors together 
				// gives 2 at full encumbrance and 100 equip weight.
				equipmentWeightFactor *= 
				(
					1.0f + 
					((actorWeight + 100.0f) / 50.0f) / 
					(1 + expf((103.465736f - equippedWeight) / 10.0f))
				);
			}
			
			// Gravity considerations. Needs balancing and is subject to change.
			const float weightFactor = 
			(
				asActor || releasedRefrPtr->As<RE::Projectile>() ?
				1.0f: 
				sqrtf(releasedRefrWeight / 100.0f)
			);
			const float fallHeightDiff = max
			(
				0.0f, a_fallHeight - Util::Get3DCenterPos(releasedRefrPtr.get()).z
			);
			float gravDamageMult = 
			(
				2.5f + 
				(
					(1.5f) *
					(
						expf(0.004f * fallHeightDiff - 4.0f) - 
						expf(-0.004f * fallHeightDiff + 4.0f)
					) / 
					(
						expf(0.004f * fallHeightDiff - 4.0f) + 
						expf(-0.004f * fallHeightDiff + 4.0f)
					)
				)
			);
			damage = 
			(
				gravDamageMult *
				weightFactor * 
				havokImpactSpeed *
				levelDamageFactor *
				armorRatingFactor *
				equipmentWeightFactor *
				sneakMult
			);

			// REMOVE when done debugging.
			DBG
			(
				"{}: Hit actor {}. Thrown object {}'s mass: {}, weight: {}, equipped weight: {}, "
				"impact speed: {}, equipped weight factor: {}, armor rating and factor: {}, {},"
				"level damage factor: {} (player level: {}). "
				"Sneak mult: {}. Base carryweight: {}. "
				"Fall height: {}, current: {}, diff: {}, weight factor: {}, grav damage mult: {}, "
				"FINAL base damage: {}.", 
				coopActor->GetName(), 
				hitActorPtr->GetName(),
				releasedRefrPtr->GetName(),
				a_collidingMass,
				releasedRefrWeight,
				asActor ? asActor->GetEquippedWeight() : -1.0f,
				havokImpactSpeed,
				equipmentWeightFactor,
				hitActorPtr->CalcArmorRating(),
				armorRatingFactor,
				levelDamageFactor, 
				coopActor->GetLevel(),
				sneakMult,
				coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight),
				a_fallHeight,
				releasedRefrPtr->data.location.z,
				fallHeightDiff,
				weightFactor,
				gravDamageMult,
				damage
			);
		}

		// Handle health damage.
		// Ignore damage to friendly actors if friendly fire is off.
		if (damage != 0.0f)
		{
			// Damage will not be modified in either HandleHealthDamage() hook 
			// because the damage will not be attributed to the player
			// (attacker param is nullptr) since we are directly modifying the health AV here.
			// Therefore, to get the same damage modifications here, 
			// we tack on the thrown object damage mult, 
			// or flop damage mult if the released refr is the player themselves,
			// and multiply the result by the damage received mult if the target is a player.
			if (releasedRefrPtr == coopActor)
			{
				damage *= Settings::vfFlopDamageMult[playerID];
			}
			else
			{
				damage *= Settings::vfThrownObjectDamageMult[playerID];
			}
		}

		const bool triggerCombat = 
		(
			(!Util::IsDialogueTarget(hitActorPtr.get())) &&
			(
				Settings::vbFriendlyFire[playerID] || 
				!Util::IsPartyFriendlyActor(hitActorPtr.get())
			)
		);
		// First, apply stagger to actors that do not ragdoll while alive.
		Util::ApplyHit
		(
			coopActor.get(),
			hitActorPtr.get(),
			damage,
			triggerCombat,
			true,
			damage, 
			damage,
			coopActor->GetHandle(),
			releasedRefrPtr->formID,
			hitFlags
		);

		// Ragdoll the hit actor with a force dependent on the colliding body's impact speed.
		if (a_shouldRagdoll)
		{
			auto hitActorRigidBodyPtr = Util::GethkpRigidBody(hitActorPtr.get()); 
			if (hitActorRigidBodyPtr)
			{
				// TODO: 
				// Add impulse without knockdown setting for less-impactful collisions.
				if (auto precisionAPI4 = ALYSLC::PrecisionCompat::g_precisionAPI4; precisionAPI4)
				{
					precisionAPI4->ApplyHitImpulse2
					(
						a_hitActorHandle, 
						coopActor->GetHandle(), 
						hitActorRigidBodyPtr.get(), 
						ToNiPoint3(a_collidingVelocity), 
						TohkVector4(a_contactPos) * GAME_TO_HAVOK, 
						1.0f
					);
				}

				// Knockout!
				Util::PushActorAway(hitActorPtr.get(), a_contactPos, -1.0f);

				// Cheeky message.
				auto ui = RE::UI::GetSingleton();
				if (ui && glob.menuPID > -1 && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
				{
					bool showCheekyMessage = 
					(
						hitActorPtr == glob.coopPlayers[glob.menuPID]->coopActor
					);
					if (!showCheekyMessage)
					{
						auto menuTopicManager = RE::MenuTopicManager::GetSingleton();
						showCheekyMessage = 
						(
							(menuTopicManager) && 
							(
								a_hitActorHandle == menuTopicManager->speaker ||
								a_hitActorHandle == menuTopicManager->lastSpeaker
							)
						);
					}
							
					if (showCheekyMessage)
					{
						RE::BSFixedString messageText =
						(
							fmt::format("{} disapproves", coopActor->GetName()).c_str()
						);
						std::mt19937 generator{ };
						generator.seed(SteadyClock::now().time_since_epoch().count());
						float rand = generator() / (float)((std::mt19937::max)());
						if (rand <= 0.5f)
						{
							auto index = static_cast<size_t>
							(
								GlobalCoopData::CHEEKY_DISAPPROVAL_MESSAGE_OPTIONS.size() * 
								(generator() / (float)((std::mt19937::max)()))
							);
							messageText = GlobalCoopData::CHEEKY_DISAPPROVAL_MESSAGE_OPTIONS[index];
						}

						RE::DebugNotification(messageText.c_str(), "UISneakAttack");
					}
				}
			}
		}

		if (canSMORF && wantsToSMORF && asActor == coopActor.get())
		{
			isSMORFing = true;
			wantsToSMORF = false;
			rmm->ClearReleasedRefr(coopActor->GetHandle());
			SetIsGrabbing(true);
			rmm->AddGrabbedRefr(p, coopActor->GetHandle());
			SetCrosshairMessageRequest
			(
				CrosshairMessageType::kActivationInfo,
				fmt::format
				(
					"P{}: <font color=\"#FFD766\">"
					"Cheese for everyone!</font>",
					playerID + 1
				),
				{
					CrosshairMessageType::kNone,
					CrosshairMessageType::kEquippedItem,
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			DeactivateCrosshair();
		}

		// Play sound.
		auto audioManager = RE::BSAudioManager::GetSingleton(); 
		if (!audioManager)
		{
			return;
		}
				
		RE::BSSoundHandle handle{ };
		RE::BGSSoundDescriptorForm* flopSFX =
		(
			RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xAF664)
		);
		if (!flopSFX)
		{	
			return;
		}

		bool succ = audioManager->BuildSoundDataFromDescriptor(handle, flopSFX);
		if (succ)
		{
			handle.SetPosition(a_contactPos);
			auto actor3DPtr = Util::GetRefr3D(hitActorPtr.get());
			if (actor3DPtr)
			{
				handle.SetObjectToFollow(actor3DPtr.get());
				handle.SetVolume(min(1.0f, havokImpactSpeed / 5.0f));
				handle.Play();
			}
		}

		// Send detection event for the aggressor player.
		Util::SetActorsDetectionEvent
		(
			coopActor.get(), 
			releasedRefrPtr.get(), 
			a_collidingMass,
			a_contactPos
		);
	}

	void TargetingManager::HandleQuickLootMenu()
	{
		// Open the LootMenu when the player moves their crosshair over a lootable container,
		// or close the LootMenu if the player moves their crosshair off the container 
		// or if the container becomes invalid.

		// Only run if QuickLoot is loaded,
		// no temporary menus are open, 
		// and the player is not transformed or transforming.
		if (!ALYSLC::QuickLootCompat::g_installed)
		{
			return;
		}

		if (!Util::MenusOnlyAlwaysUnpaused())
		{
			return;
		}

		// LootMenu opens but is not visible sometimes 
		// when targeting a container while transformed,
		// so don't attempt to open it until the player reverts their form.
		// Players can still loot by selecting the container with their crosshair 
		// and activating it as usual.
		if (p->isTransforming || p->isTransformed)
		{
			return;
		}
		
		auto ui = RE::UI::GetSingleton();
		// Check for changes to the player's lock on-selected refr.
		const auto& selectedRefrHandle = activationRefrHandle;
		auto selectedRefrPtr = Util::GetRefrPtrFromHandle(selectedRefrHandle);
		auto prevSelectedRefrPtr = Util::GetRefrPtrFromHandle(prevQuickLootRefrHandle);
		bool selectedRefrValidity = 
		(
			selectedRefrPtr && Util::IsValidRefrForTargeting(selectedRefrPtr.get())
		);
		// Has the player moved into/out of range of their targeted refr?
		bool wasInRange = selectedRefrInRangeForQuickLoot;
		selectedRefrInRangeForQuickLoot = 
		(
			selectedRefrValidity ?
			RefrIsInActivationRange(selectedRefrHandle) :
			false
		);
		// Before sending a crosshair event to change the state of the QuickLoot menu,
		// ensure no other menus are opening back up in quick succession,
		// which will cause flickering due to many requests triggering in at once.
		// Truly an icky solution below.
		float secsSinceAllSupportedMenusClosed = Util::GetElapsedSeconds
		(
			glob.lastSupportedMenusClosedTP
		);
		bool newSelectedRefr = prevSelectedRefrPtr != selectedRefrPtr;
		/*DBG("{}: Previous and current targets ({}): {}, {}",
			coopActor->GetName(), 
			!aimMode,
			prevSelectedRefrPtr ? prevSelectedRefrPtr->GetName() : "NONE",
			selectedRefrPtr ? selectedRefrPtr->GetName() : "NONE");*/
		// Check if this player was last in control of the LootMenu.
		bool wasInControl = glob.quickLootControlPID == playerID;
		// Grace period of 1/8 of a second first.
		// Then also make sure there is no active request from a player.
		bool anyPlayerCanSet =
		(
			!glob.supportedMenuOpen.load() &&
			secsSinceAllSupportedMenusClosed > 0.125f &&
			glob.quickLootReqPID == -1
		);
		
		// Is this player controlling menus?
		bool controllingMenus = 
		(
			glob.supportedMenuOpen.load() && GlobalCoopData::IsControllingMenus(playerID)
		);
		bool quickLootMenuOpen = ui && ui->IsMenuOpen(GlobalCoopData::LOOT_MENU);
		// Is this player trying to activate an object?
		bool isActivating = p->pam->IsPerformingOneOf
		(
			InputAction::kActivate, InputAction::kActivateAllOfType, InputAction::kActivateCancel
		);
		// Send a new crosshair event to open the QuickLoot menu 
		// if the player's selected refr is valid,
		// any player can open the menu, and the refr is now in range + 
		// if it was just selected, not previously in range, 
		// or the player did not send the last opening request.
		bool shouldSendNewSetCrosshairEvent = 
		{
			(!isActivating && selectedRefrValidity && selectedRefrInRangeForQuickLoot) &&
			((anyPlayerCanSet) && (newSelectedRefr || !wasInRange || !wasInControl))
		};
		// Validate sending a new crosshair event if the new crosshair refr is valid,
		// the player is controlling menus,
		// and the player just selected a new refr that is in range.
		bool shouldValidateNewCrosshairEvent = 
		{
			!isActivating && 
			selectedRefrValidity && 
			controllingMenus && 
			newSelectedRefr &&
			selectedRefrInRangeForQuickLoot
		};
		// Close the LootMenu menu if the player is controlling the menu 
		// and the selected refr is no longer valid, or is no longer in range.
		// Also can clear if no supported menus are open,
		// this player was previously controlling the menu 
		// and is not selecting anything or is out of range,
		// and no other player is requesting to open the LootMenu,
		// but the requested container to open was not cleared.
		bool shouldSendClearCrosshairEvent = 
		{
			(controllingMenus) && 
			(
				(
					(!selectedRefrPtr && prevSelectedRefrPtr) || 
					(wasInRange && !selectedRefrInRangeForQuickLoot)
				) || 
				(
					(
						(
							!glob.supportedMenuOpen &&
							playerID == glob.quickLootControlPID &&
							glob.reqQuickLootContainerHandle != RE::ObjectRefHandle()
						) &&
						(!selectedRefrPtr || !selectedRefrInRangeForQuickLoot)
					) &&
					(
						playerID == glob.quickLootReqPID ||
						glob.quickLootReqPID == -1
					)
				)
			)
		};
		/*DBG
		(
			"{}: {}, PIDs: control: {}, req: {}, menu open: {}. New: {}, in range, was: {}, {}, "
			"any player: {}, as in control: {}, controlling menus: {}, is activating: {}, "
			"should send new: {}, should validate: {}, should clear: {}.",
			coopActor->GetName(), selectedRefrPtr ? selectedRefrPtr->GetName() : "NONE",
			glob.quickLootControlPID, glob.quickLootReqPID, quickLootMenuOpen,
			newSelectedRefr, selectedRefrInRangeForQuickLoot, wasInRange, anyPlayerCanSet,
			wasInControl, controllingMenus, isActivating, shouldSendNewSetCrosshairEvent,
			shouldValidateNewCrosshairEvent, shouldSendClearCrosshairEvent
		);*/
		// Can potentially open the QuickLoot menu.
		if (shouldSendNewSetCrosshairEvent || shouldValidateNewCrosshairEvent)
		{
			// Selected refr must be have an inventory and not be a player.
			bool hasLoot = 
			(
				selectedRefrPtr->HasContainer() && 
				!GlobalCoopData::IsCoopPlayer(selectedRefrPtr.get())
			);
			if (hasLoot)
			{
				// Check inventory first.
				hasLoot = false;
				// REMOVE when crash during inventory access is fixed.
				/*DBG
				(
					"{}: Check inventory of {} to see if it contains lootable objects.",
					coopActor->GetName(), selectedRefrPtr->GetName()
				);*/
				auto inventory = selectedRefrPtr->GetInventory(Util::IsLootableObject);
				for (const auto& [boundObj, invEntryData] : inventory)
				{
					if (boundObj && invEntryData.second && invEntryData.first > 0)
					{
						hasLoot = true;
						break;
					}
				}

				// Then check the refr's dropped inventory.
				if (!hasLoot)
				{
					auto droppedInventory = selectedRefrPtr->GetDroppedInventory
					(
						Util::IsLootableObject
					);
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
				(!selectedRefrPtr->As<RE::Actor>() || selectedRefrPtr->IsDead()) &&
				(
					selectedRefrValidity &&
					hasLoot &&
					selectedRefrInRangeForQuickLoot &&
					!selectedRefrPtr->IsLocked() &&
					!selectedRefrPtr->IsActivationBlocked() &&
					!glob.isInCoopCombat
				)
			};
			bool firstTimeLootingKilledActor = false;
			// If looting a corpse, give the killing player first rights.
			if (canOpenLootMenu)
			{
				// Is corpse.
				if (auto corpse = selectedRefrPtr->As<RE::Actor>(); corpse)
				{
					// Saved killing player as the actor's owner
					// when the HandleHealthDamage() hook fired before this actor died.
					const auto owner = corpse->extraList.GetOwner();
					const auto ownerActor = owner ? owner->As<RE::Actor>() : nullptr;
					bool killedByAPlayer = GlobalCoopData::IsCoopPlayer(ownerActor);
					firstTimeLootingKilledActor = 
					{
						(killedByAPlayer && ownerActor) && (ownerActor == coopActor.get())
					};
					// Can loot now if this player is looting the actor 
					// they killed for the first time,
					// or if the actor was not killed by a player.
					/*DBG
					(
						"First time: {}, killed by player: {}. Killer: {}",
						firstTimeLootingKilledActor,
						killedByAPlayer, 
						ownerActor ? ownerActor->GetName() : "NONE"
					);*/
					canOpenLootMenu &= 
					(
						firstTimeLootingKilledActor || !killedByAPlayer
					);

					if (!canOpenLootMenu)
					{
						SetCrosshairMessageRequest
						(
							CrosshairMessageType::kGeneralNotification,
							fmt::format
							(
								"P{}: To the combat victor '{}' go the QuickLoot spoils!",
								playerID + 1, ownerActor->GetName()
							),
							{
								CrosshairMessageType::kNone,
								CrosshairMessageType::kEquippedItem,
								CrosshairMessageType::kStealthState,
								CrosshairMessageType::kTargetSelection 
							},
							Settings::fSecsBetweenDiffCrosshairMsgs
						);
					}
				}
			}

			// Can now open the QuickLoot menu if the final LOS check passes.
			// LOS check last, since it is the most expensive.
			if (canOpenLootMenu)
			{
				bool passesLOSCheck = 
				(
					selectedRefrValidity && 
					Util::HasLOS
					(
						selectedRefrPtr.get(), 
						coopActor.get(),
						false, 
						true,
						crosshairWorldPos
					)
				);
				if (passesLOSCheck)
				{
					glob.moarm->InsertRequest
					(
						playerID, 
						InputAction::kMoveCrosshair, 
						SteadyClock::now(),
						GlobalCoopData::LOOT_MENU,
						selectedRefrHandle
					);
					// Send SKSE crosshair event to allow QuickLoot menu to trigger.
					// Deselect current crosshair pick refr first if sending a new crosshair event.
					if (shouldSendNewSetCrosshairEvent)
					{
						DBG
						(
							"{} is closing LootMenu, if open.", coopActor->GetName()
						);
						Util::SendCrosshairEvent(nullptr);
					}
					
					DBG
					(
						"{} opening LootMenu -> {}.",
						coopActor->GetName(),
						selectedRefrPtr->GetName()
					);
					Util::SendCrosshairEvent(selectedRefrPtr.get(), playerID);

					// After sending a crosshair event to open the LootMenu for a corpse,
					// clear out the ownership exData so other players can freely loot the corpse.
					if (firstTimeLootingKilledActor)
					{
						auto selectedTargetActor = selectedRefrPtr->As<RE::Actor>();
						if (selectedTargetActor)
						{
							const auto owner = selectedTargetActor->extraList.GetOwner();
							const auto ownerActor = owner ? owner->As<RE::Actor>() : nullptr;
							bool killedByAPlayer = GlobalCoopData::IsCoopPlayer(ownerActor);
							if (killedByAPlayer)
							{
								// Remove owner.
								selectedTargetActor->SetOwner(nullptr);
							}
						}
					}
				}
				else
				{
					DBG("{}: No LOS on {}.", coopActor->GetName(),
						selectedRefrPtr->GetName());
				}
			}
			else if (shouldValidateNewCrosshairEvent)
			{
				// Clear crosshair pick refr if the player's new crosshair refr is not lootable.
				// Closes the menu.
				DBG
				(
					"{} is closing LootMenu after moving crosshair onto un-lootable refr.",
					coopActor->GetName()
				);
				Util::SendCrosshairEvent(nullptr);
			}

			/*DBG
			(
				"{}: {}. Can open: {}, has loot: {}, in range: {}, locked: {}, "
				"activation blocked: {}, in combat: {}. "
				"Should send new: {}, should validate new: {}, first time: {}",
				coopActor->GetName(),
				selectedRefrPtr->GetName(),
				canOpenLootMenu,
				hasLoot,
				selectedRefrInRangeForQuickLoot,
				selectedRefrPtr->IsLocked(),
				selectedRefrPtr->IsActivationBlocked(),
				glob.isInCoopCombat,
				shouldSendNewSetCrosshairEvent,
				shouldValidateNewCrosshairEvent, 
				firstTimeLootingKilledActor
			);*/
		}
		else if (shouldSendClearCrosshairEvent)
		{
			// Close the menu by clearing the crosshair pick refr on request.
			DBG
			(
				"{} is closing LootMenu after no longer selecting a refr: {}, "
				"moving too far away: {}.",
				coopActor->GetName(),
				!selectedRefrValidity,
				!selectedRefrInRangeForQuickLoot
			);
			Util::SendCrosshairEvent(nullptr);
		}

		// Update for the next frame.
		prevQuickLootRefrHandle = activationRefrHandle;
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
			if (!otherP->isActive || otherP == p)
			{
				continue;
			}

			// Two different players are grabbing each other.
			if (rmm->IsManaged(otherP->coopActor->GetHandle(), true) && 
				otherP->tm->rmm->IsManaged(coopActor->GetHandle(), true))
			{
				isMARFing = true;
				break;
			}
		}

		//================
		//[Grabbed Refrs]:
		//================

		// Player has grabbed at least one object.
		if (!rmm->grabbedRefrInfoList.empty()) 
		{
			// Clear all invalid grabbed refrs first.
			rmm->ClearInvalidRefrs(true);
			// Release grabbed refrs when no longer grabbing.
			if (!rmm->isGrabbing)
			{
				// Cache the total magicka required to throw all objects.
				// A single factor derived from this cost scales all thrown refrs' release speeds.
				// Must be facing the crosshair position to throw.
				if (p->tm->crosshairActive)
				{
					rmm->SetTotalThrownRefrMagickaCost(p, true);
				}

				for (uint8_t i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
				{
					auto& grabbedRefrInfo = rmm->grabbedRefrInfoList[i];
					const auto& handle = grabbedRefrInfo->refrHandle;
					// Now managed as a released refr.
					rmm->AddReleasedRefr(p, handle, rmm->totalThrownRefrMagickaCost);

					// Reset paralysis flag and max out ragdoll timer on actors 
					// to prevent the game from instantly signalling them 
					// to get up after being released.
					// Set even if the actor getup removal setting is enabled 
					// since we don't want NPCs getting up in midair.
					auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
					if (!refrPtr) 
					{
						continue;
					}

					auto asActor = refrPtr->As<RE::Actor>(); 
					if (!asActor)
					{
						continue;
					}

					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					if (asActor->currentProcess && asActor->currentProcess->middleHigh)
					{
						asActor->currentProcess->middleHigh->unk2B0 = FLT_MAX;
					}
				}

				// Clear all grabbed refrs on release.
				rmm->ClearGrabbedRefrs();
				
				// If one refr is thrown, all are thrown.
				bool isThrown = 
				(
					rmm->releasedRefrInfoList.empty() ?
					false :
					rmm->releasedRefrInfoList[0]->isThrown
				);
				// Expend magicka if throwing all previously grabbed refrs.
				if (isThrown && rmm->totalThrownRefrMagickaCost > 0.0f)
				{
					p->pam->ModifyAV
					(
						RE::ActorValue::kMagicka, -rmm->totalThrownRefrMagickaCost
					);
				}

				// REMOVE when separate throw bind is added.
				DeactivateCrosshair();
			}
			else
			{
				// Update grabbed refrs' positioning via their velocity.
				// If grabbing multiple objects, all other objects are suspended 
				// in a ring around the first one.
				// Radius of ring at which to suspend all subsequent grabbed refrs.
				float ringBufferDist = 0.0f;
				for (uint8_t i = 0; i < rmm->grabbedRefrInfoList.size(); ++i)
				{
					auto& grabbedRefrInfo = rmm->grabbedRefrInfoList[i];
					if (!grabbedRefrInfo)
					{
						continue;
					}

					const auto& handle = grabbedRefrInfo->refrHandle;
					auto refrPtr = Util::GetRefrPtrFromHandle(handle);
					if (!refrPtr)
					{
						continue;
					}

					// Get buffer distance if it has not been set yet.
					if (ringBufferDist == 0.0f)
					{
						// Must use the first valid refr's radius for spacing purposes.
						auto refr3DPtr = Util::GetRefr3D(refrPtr.get()); 
						if (refr3DPtr)
						{
							ringBufferDist = 
							(
								refr3DPtr->worldBound.radius * 0.5f
							);
						}
						else
						{
							ringBufferDist = refrPtr->GetHeight() * 0.25f;
						}
					}

					// Paralyze living actor to prevent the game from automatically
					// signalling the actor to get up once the ragdoll timer hits 0.
					// Only done if the actor getup removal setting is enabled,
					// or if the grabbed actor is a player.
					if (Settings::bRemoveGrabbedActorAutoGetUp || 
						GlobalCoopData::IsCoopPlayer(refrPtr))
					{
						auto asActor = refrPtr->As<RE::Actor>(); 
						if (asActor &&
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
					if (rmm->CanManipulateGrabbedRefr(p, i))
					{
						grabbedRefrInfo->UpdateGrabbedReference(p, i, ringBufferDist);
					}
				}
			}
		}
		
		//=================
		//[Released Refrs]:
		//=================
		
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
			auto targetRefrPtr = Util::GetRefrPtrFromHandle
			(
				aimMode == AimMode::kTwinStick ? 
				aimCorrectionTargetHandle :
				crosshairRefrHandle
			);
			// Flopped-on refrs to add as released to propagate the initial collision.
			// Pairs of (refr handle, released angle factor).
			std::vector<std::pair<RE::ObjectRefHandle, float>> flopRedirectedRefrs{ };
			for (uint8_t i = 0; i < rmm->releasedRefrInfoList.size(); ++i)
			{
				auto& releasedRefrInfo = rmm->releasedRefrInfoList[i];
				const auto& handle = releasedRefrInfo->refrHandle;
				// Must have been released.
				if (!releasedRefrInfo->releaseTP.has_value())
				{
					rmm->ClearRefr(handle);
					// Decrement index since the next element has shifted 
					// into this index upon erasure.
					--i;
					continue;
				}

				auto releasedRefrPtr = Util::GetRefrPtrFromHandle
				(
					releasedRefrInfo->refrHandle
				);

				// Get active projectile, if the relased refr was demarcated as one.
				// No collision handling, as the game already does it for us.
				auto asActiveProjectile = 
				(
					releasedRefrInfo->isActiveProjectile ? 
					releasedRefrPtr->As<RE::Projectile>() : 
					nullptr
				);
				if (asActiveProjectile)
				{
					continue;
				}

				// Clear invalid refr and move on to the next one.
				if (!releasedRefrPtr)
				{
					rmm->ClearRefr(handle);
					--i;
					continue;
				}

				auto releasedActor = releasedRefrPtr->As<RE::Actor>();
				float secsSinceRelease = Util::GetElapsedSeconds
				(
					releasedRefrInfo->releaseTP.value()
				);
				// Do perform released refr collision checks any longer if:
				// 1. The released refr is a live actor that has gotten up from ragdolling 
				// -OR-
				// 2. The released refr is not a flopping player and hit anything
				// (if an aim prediction refr) or hit the player's target refr (if a homing refr) 
				// -OR-
				// 3. The post-release handling period has ended.
				bool shouldNoLongerHandleCollisions =
				(
					(
						(releasedActor && !releasedActor->IsDead()) &&
						(
							releasedActor->actorState1.knockState == 
							RE::KNOCK_STATE_ENUM::kNormal ||
							releasedActor->actorState1.knockState == 
							RE::KNOCK_STATE_ENUM::kGetUp
						)
					) ||
					(
						(!releasedActor || releasedActor->IsDead()) &&
						(
							releasedRefrInfo->firstHitTP.has_value()
						) &&
						(
							(releasedRefrInfo->trajType == ProjectileTrajType::kPrediction) ||
							(
								targetRefrPtr && 
								targetRefrPtr.get() &&
								releasedRefrInfo->hitRefrFIDs.contains(targetRefrPtr->formID)
							)
						)
					) ||
					(
						secsSinceRelease > Settings::fMaxSecsBeforeClearingReleasedRefr
					)
				);

				// Clear out released refr once we don't need to handle collisions for it.
				if (shouldNoLongerHandleCollisions)
				{
					rmm->ClearRefr(handle);
					--i;
					continue;
				}

				// Add a new contact listener for the current cell, 
				// if there is no active one currently.
				if (auto cell = releasedRefrPtr->parentCell; cell)
				{
					if (auto bhkWorld = cell->GetbhkWorld(); bhkWorld)
					{
						auto ahkpWorld = bhkWorld->GetWorld2(); 
						if (ahkpWorld && glob.contactListener->world != ahkpWorld)
						{
							glob.contactListener->world = ahkpWorld;
							glob.contactListener->AddContactListener(ahkpWorld);
						}
					}
				}

				// Raycast collision checks.
				auto releasedRefr3DPtr = Util::GetRefr3D(releasedRefrPtr.get());
				auto releasedRefrRigidBodyPtr = Util::GethkpRigidBody(releasedRefrPtr.get()); 
				// Must have both valid 3D and rigid body for raycast collision checks.
				if (!releasedRefr3DPtr || !releasedRefrRigidBodyPtr)
				{
					continue;
				}

				// Adjust trajectory to reach the trajectory end position 
				// or home in on the target if necessary.
				if (releasedRefrInfo->isThrown)
				{
					auto velToSet = releasedRefrInfo->GuideRefrAlongTrajectory(p);
					// Cap speed to the release speed, 
					// which effectively caps bonk damage as well.
					releasedRefrInfo->ApplyVelocity(velToSet);
				} 
				else if (!Settings::bPreventFallDamage)
				{
					// Set fall height at the apex of the trajectory to properly apply fall damage 
					// once the dropped actor hits a surface.
					auto asActor = releasedRefrPtr->As<RE::Actor>(); 
					auto hkpRigidBodyPtr = Util::GethkpRigidBody(asActor);
					auto charController = asActor ? asActor->GetCharController() : nullptr;
					if (charController && hkpRigidBodyPtr)
					{
						auto currentVelocity = ToNiPoint3
						(
							hkpRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME
						);
						float previousVelPitch = Util::GetPitchBetweenPositions
						(
							RE::NiPoint3(), releasedRefrInfo->lastSetVelocity
						);
						float currentVelPitch = Util::GetPitchBetweenPositions
						(
							RE::NiPoint3(), currentVelocity
						);
						// No collision recorded and angled downward at release,
						// or now angled downward after previously angled upward 
						// (reached or past apex).
						bool firstHit = releasedRefrInfo->firstHitTP.has_value();
						bool isAtOrPastTrajApex = 
						(
							(!firstHit) && 
							(currentVelPitch >= 0.0f) &&
							(
								(previousVelPitch <= 0.0f) || 
								(	
										
									releasedRefrInfo->releaseTP.has_value() &&
									Util::GetElapsedSeconds
									(
										releasedRefrInfo->releaseTP.value()
									) <= *g_deltaTimeRealTime
								)
							)
						);
						if (isAtOrPastTrajApex)
						{
							charController->lock.Lock();
							Util::AdjustFallState(charController, true);
							charController->lock.Unlock();
						}

						releasedRefrInfo->lastSetVelocity = currentVelocity;
					}
				}

				// Raycast to check for potential collisions to handle
				// now that we've updated the velocity of the released refr.
				RE::ObjectRefHandle hitRefrHandle{ };
				RE::NiPoint3 hitPos{ };
				RE::NiPoint3 hitNormal{ };
				RE::NiPoint3 velDir{ };
				glm::vec4 start{ };
				glm::vec4 end{ };
				glm::vec4 velOffset{ };
						
				// Raycast once per major NPC skeleton node.
				// Only need a single raycast to hit before breaking.
				uint32_t numNodesCastFrom = 0;
				// A raycast from the released refr hit something.
				bool hit = false;
				if (!Settings::bSimpleThrownObjectCollisionCheck)
				{
					// Actor collisions -- multiple raycasts per actor.
					// Again, must have valid loaded 3D.
					if (!releasedRefrPtr->loadedData || 
						!releasedRefrPtr->loadedData->data3D)
					{
						continue;
					}

					auto loadedData = releasedRefrPtr->loadedData; 
					auto data3DPtr = loadedData->data3D;
					RE::BSVisit::TraverseScenegraphObjects
					(
						data3DPtr.get(),
						[
							this, 
							&releasedRefrPtr,
							&start, 
							&end, 
							&hit,
							&hitRefrHandle,
							&hitPos, 
							&hitNormal,
							&velDir,
							&velOffset,
							&numNodesCastFrom
						]
						(RE::NiAVObject* a_node)
						{
							auto nodePtr = RE::NiPointer<RE::NiAVObject>(a_node);
							// Invalid node.
							if (!a_node || !nodePtr || !nodePtr->AsNode())
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Need a collision object from the node's 3D.
							auto collisionObject = nodePtr->GetCollisionObject();
							if (!collisionObject)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}
							
							// Redundant nodes which usually match with a skeleton node,
							// so we won't cast from them.
							if (nodePtr->name.contains("CME"))
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Need a rigid body.
							auto hkpRigidBodyPtr = Util::GethkpRigidBody(nodePtr.get()); 
							if (!hkpRigidBodyPtr || !hkpRigidBodyPtr->GetCollidable())
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							// Havok shape for its radius.
							const RE::hkpShape* hkpShape = 
							(
								hkpRigidBodyPtr->collidable.shape
							);
							if (!hkpShape)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							auto shape = static_cast<const RE::hkpConvexShape*>(hkpShape);
							// Invalid shape or radius.
							// The radius is sometimes reported as negative
							// or impossibly large, and any one node should not
							// have a radius larger than the height of the object
							// it belongs to anyways. Ignore these nodes.
							if (!shape || 
								shape->radius <= 0.0f ||
								shape->radius > 
								releasedRefrPtr->GetHeight() * GAME_TO_HAVOK)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							velDir = ToNiPoint3
							(
								hkpRigidBodyPtr->motion.linearVelocity, true
							);
							// Zero velocity -> no collision.
							if (velDir.Length() == 0.0f)
							{
								return RE::BSVisit::BSVisitControl::kContinue;
							}

							float distPerFrame = 
							(
								hkpRigidBodyPtr->motion.linearVelocity.Length3() *
								*g_deltaTimeRealTime *
								HAVOK_TO_GAME
							);
							float radius = (shape->radius * HAVOK_TO_GAME) + distPerFrame;
							// Cast from node's world position outward a length 
							// equal to the node's radius in the direction 
							// of the node's velocity.
							velOffset = ToVec4(velDir * radius);
							start = ToVec4(nodePtr->world.translate);
							end = start + velOffset;
							auto result = Raycast::hkpCastRay
							(
								start, 
								end, 
								std::vector<RE::TESObjectREFR*>({ releasedRefrPtr.get() }),
								std::vector<RE::FormType>
								(
									{ RE::FormType::Activator } 
								)
							);
							// Increment cast count.
							numNodesCastFrom++;

							// Only need a single hit, so once there is a hit, 
							// we save the hit refr and position and then break.
							if (result.hit)
							{
								hit = true;
								hitRefrHandle = result.hitRefrHandle;
								hitPos = ToNiPoint3(result.hitPos);
								hitNormal = ToNiPoint3(result.rayNormal);
								return RE::BSVisit::BSVisitControl::kStop;
							}

							return RE::BSVisit::BSVisitControl::kContinue;
						}
					);
				}

				// If not using the more comprehensive raycast collision detection system,
				// or if the released refr had no valid nodes to raycast from, 
				// fall back to the simpler raycast collision check.
				if (Settings::bSimpleThrownObjectCollisionCheck || numNodesCastFrom == 0) 
				{
					// A single raycast starting from the released refr's reported location 
					// or center and in the direction of the node/refr's velocity.
					// Cast length equals the distance the node travels per frame 
					// at the current velocity plus a small increment 
					// equal to half the refr's height or equal to its radius.
							
					velDir = ToNiPoint3
					(
						releasedRefrRigidBodyPtr->motion.linearVelocity, true
					);
					float distPerFrame = 
					(
						releasedRefrRigidBodyPtr->motion.linearVelocity.Length3() *
						*g_deltaTimeRealTime *
						HAVOK_TO_GAME
					);
					float incThrownRefrRadius = 0.0f;
					if (releasedActor)
					{
						incThrownRefrRadius = 
						(
							releasedRefrPtr->GetHeight() / 2.0f + distPerFrame
						);
					}
					else
					{
						incThrownRefrRadius = 
						(
							releasedRefr3DPtr->worldBound.radius + distPerFrame
						);
					}

					if (releasedActor)
					{
						start = ToVec4(Util::GetTorsoPosition(releasedActor));
					}
					else
					{
						start = ToVec4(releasedRefr3DPtr->worldBound.center);
					}
					
					end = start + ToVec4(velDir * incThrownRefrRadius);
					auto result = Raycast::hkpCastRay
					(
						start, 
						end, 
						std::vector<RE::TESObjectREFR*>({ releasedRefrPtr.get() }),
						std::vector<RE::FormType>
						(
							{ RE::FormType::Activator } 
						)
					);
					hit = result.hit;
					hitRefrHandle = result.hitRefrHandle;
					hitPos = ToNiPoint3(result.hitPos);
					hitNormal = ToNiPoint3(result.rayNormal);
				}

				auto hitRefrPtr = 
				(
					hit ? Util::GetRefrPtrFromHandle(hitRefrHandle) : nullptr
				); 
				// Do not continue setting the released refr's trajectory,
				// or bonk or splat if the hit actor is a player that is dash dodging
				// or flailing their arms in a crazed and/or defensive manner.
				// Either method still requires timing using the dash dodge's I-Frame window.
				if (hitRefrPtr)
				{
					auto hitPlayerIndex = GlobalCoopData::GetCoopPlayerIndex
					(
						hitRefrPtr.get()
					);
					if (hitPlayerIndex != -1)
					{
						const auto& hitP = glob.coopPlayers[hitPlayerIndex];
						// Number of seconds independent of framerate.
						float secsEvadeWindow = 
						(
							(
								Settings::uDashDodgeBaseAnimFrameCount + 
								Settings::uDashDodgeSetupFrameCount
							) *
							(1.0f / (*g_deltaTimeRealTime * 60.0f))
						);
						bool canEvade = 
						(
							(hitP->mm->isDashDodging) || 
							(
								hitP->pam->IsPerforming(InputAction::kRotateLeftShoulder) &&
								hitP->pam->GetSecondsSinceLastStart
								(
									InputAction::kRotateLeftShoulder
								) <
								secsEvadeWindow 
							) ||
							(
								hitP->pam->IsPerforming(InputAction::kRotateRightShoulder) &&
								hitP->pam->GetSecondsSinceLastStart
								(
									InputAction::kRotateRightShoulder
								) < secsEvadeWindow 
							)
						);
						if (canEvade)
						{
							// Clear the released refr, so we don't continue 
							// setting its trajectory or listening for collisions.
							// Otherwise, if it is homing in on the target,
							// it'll go through the player, come back around,
							// and hit the player once their dodge I-frames end
							// (or once their arms grow heavy with fatigue and stop moving).
							rmm->ClearRefr(handle);
							continue;
						}
					}
					
					// Handle potential collisions.
					bool hasAlreadyHitRefr = 
					(
						releasedRefrInfo->HasAlreadyHitRefr(hitRefrPtr.get())
					);

					// Ignore collisions between managed released refrs.
					if (rmm->IsManaged(hitRefrHandle, false))
					{
						continue;
					}

					// Ignore refrs without collision, such as activators.
					auto hitRigidBodyPtr = Util::GethkpRigidBody(hitRefrPtr.get()); 
					bool hasCollidable =
					(
						hitRigidBodyPtr &&
						hitRigidBodyPtr.get() && 
						hitRigidBodyPtr->GetCollidable()
					);
					if (!hasCollidable)
					{
						continue;
					}

					// Add hit refr to cached hit form IDs set.
					releasedRefrInfo->AddHitRefr(hitRefrPtr.get());
					auto hitActor = hitRefrPtr->As<RE::Actor>(); 
					// Hit a new, valid actor that is not the released refr 
					// or the player that released the refr. Bonk.
					bool shouldBonk = 
					(
						hitActor && 
						hitActor->currentProcess && 
						hitRefrPtr != releasedRefrPtr && 
						hitRefrPtr != coopActor && 
						!hasAlreadyHitRefr
					);
					if (shouldBonk)
					{
						HandleBonk
						(
							hitActor->GetHandle(), 
							releasedRefrPtr->GetHandle(), 
							releasedRefrRigidBodyPtr->motion.GetMass(),
							releasedRefrInfo->fallHeight,
							ToNiPoint3
							(
								releasedRefrRigidBodyPtr->motion.linearVelocity *
								HAVOK_TO_GAME
							),
							hitPos
						);
					}

					// Heh.
					// Works the same way as slapping the object to redirect it.
					const auto targetRefrHandle = hitRefrPtr->GetHandle();
					bool shouldRedirectWithFlop = 
					(
						(
							releasedRefrPtr == coopActor &&
							hitRefrPtr != coopActor &&
							targetRefrHandle != crosshairRefrHandle &&
							targetRefrHandle != aimCorrectionTargetHandle
						) &&
						(
							hitActor || 
							Util::IsLootableRefr(hitRefrPtr.get())
						)
					);
					if (shouldRedirectWithFlop) 
					{
						flopRedirectedRefrs.emplace_back
						(
							hitRefrHandle,
							std::lerp
							(
								0.5f,
								1.0f,
								min
								(
									1.0f,
									releasedRefrRigidBodyPtr->motion.linearVelocity.Length3() / 
									15.0f
								)
							)
						);
					}

					// Released actor hit a new refr that isn't itself. Splat.
					// Ignore refrs we have already hit, 
					// since we want to prioritize handling hits recorded
					// through the havok contact listener and use the raycast collision check 
					// as a fallback to catch anything that the contact listener fails to record.
					bool canSplat = 
					(
						releasedActor && 
						releasedActor != hitRefrPtr.get() &&
						!hasAlreadyHitRefr
					);
					if (canSplat)
					{
						HandleSplat
						(
							releasedActor->GetHandle(), 
							hitNormal,
							max(1, releasedRefrInfo->totalHitsCount),
							releasedRefrInfo->fallHeight,
							releasedRefrInfo->isThrown
						);
					}

					if (canSplat || shouldBonk)
					{
						// Update fall height to the refr's position after handling the collision.
						releasedRefrInfo->fallHeight = 
						(
							Util::Get3DCenterPos
							(
								releasedRefrPtr.get()
							).z
						);
					}
				}
			}

			// Add any flop bonk'd refrs as released.
			for (const auto& [handle, factor] : flopRedirectedRefrs)
			{
				if (!Util::HandleIsValid(handle))
				{
					continue;
				}

				for (const auto& otherP : glob.coopPlayers)
				{
					if (!otherP->isActive || otherP == p)
					{
						continue;
					}

					// Remove grabbed/released refr from the other player's managed lists.
					if (otherP->tm->rmm->IsManaged(handle, true) || 
						otherP->tm->rmm->IsManaged(handle, false))
					{
						otherP->tm->rmm->ClearRefr(handle);
					}
				}

				rmm->AddGrabbedRefr(p, handle);
				rmm->ClearGrabbedRefr(handle);
				if (rmm->GetNumGrabbedRefrs() == 0)
				{
					SetIsGrabbing(false);
				}

				rmm->AddReleasedRefr(p, handle, 0.0f, factor);
			}
		}
		else
		{
			// No managed released refrs, so clear out cached collision refr pairs.
			if (!rmm->collidedRefrFIDPairs.empty())
			{
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set
					// once there are no remaining released refrs to handle.
					rmm->collidedRefrFIDPairs.clear();
				}
			}

			// Also clear any queued contact events, which do not need handling anymore.
			if (!rmm->queuedReleasedRefrContactEvents.empty())
			{
				std::unique_lock<std::mutex> lock(rmm->contactEventsQueueMutex, std::try_to_lock);
				if (lock)
				{
					// Clear out collided pairs set once there 
					// are no remaining released refrs to handle.
					rmm->queuedReleasedRefrContactEvents.clear();
				}
			}

			// Finally, clear out released refr map if not empty already.
			if (!rmm->releasedRefrHandlesToInfoIndices.empty()) 
			{
				rmm->releasedRefrHandlesToInfoIndices.clear();
			}
		}

		bool wasSMORFing = isSMORFing;
		isSMORFing = 
		(
			isSMORFing && canSMORF && coopActor->IsInRagdollState() && rmm->isGrabbing
		);
		if (!isSMORFing && wasSMORFing)
		{
			rmm->ClearGrabbedRefr(coopActor->GetHandle());
		}
	}

	void TargetingManager::HandleSplat
	(
		RE::ActorHandle a_releasedActorHandle, 
		const RE::NiPoint3& a_hitNormal,
		const uint32_t& a_hitCount,
		const double& a_fallHeight,
		bool a_wasThrown
	)
	{
		// Apply impact damage to thrown/flopping actor.

		auto releasedActorPtr = Util::GetActorPtrFromHandle(a_releasedActorHandle);
		// Invalid thrown actor.
		if (!releasedActorPtr)
		{
			return;
		}
		
		auto releasedRefrRigidBodyPtr = Util::GethkpRigidBody(releasedActorPtr.get()); 
		float havokImpactSpeed = 0.0f;
		float damage = 0.0f;
		// More damage when the hit surface is oriented perpendicular to the actor's motion.
		float normOpposingVelocityFactor = 1.0f;
		// Not a ghost or invulnerable.
		bool damageable = 
		(
			!releasedActorPtr->IsGhost() &&
			!releasedActorPtr->IsInvulnerable() && 
			!releasedActorPtr->IsInWater()
		);
		const bool isFlopping = releasedActorPtr == coopActor;
		if (damageable)
		{
			if (releasedRefrRigidBodyPtr) 
			{
				auto velDir = ToNiPoint3(releasedRefrRigidBodyPtr->motion.linearVelocity, true);
				normOpposingVelocityFactor = 0.5f * (1.0f - velDir.Dot(a_hitNormal));
				havokImpactSpeed = releasedRefrRigidBodyPtr->motion.linearVelocity.Length3();
				// Get refr linear speed if rigidbody speed is 0.
				if (havokImpactSpeed == 0.0f)
				{
					RE::NiPoint3 linVel{ };
					releasedActorPtr->GetLinearVelocity(linVel);
					havokImpactSpeed = linVel.Length() * GAME_TO_HAVOK;
				}

				// Higher armor rating -> less damage taken.
				// 1 / 10 the damage at an armor rating of 100.
				float armorRatingFactor = std::clamp
				(
					-0.009f * releasedActorPtr->CalcArmorRating() + 1.0f,
					0.1f,
					1.0f
				);
				// Take 1 / 2 the damage at level 100.
				float levelDamageFactor = 
				(
					1.0f / 
					(1.0f + max(releasedActorPtr->GetLevel() - 1.0f, 0.0f) / 99.0f)
				);
				const auto releasedPIndex = GlobalCoopData::GetCoopPlayerIndex(releasedActorPtr);
				float inventoryWeight = 
				(
					releasedPIndex <= 0 ?
					releasedActorPtr->GetWeightInContainer() :
					glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetWeightInContainer()
				);
				const auto invChanges = 
				(
					releasedPIndex <= 0 ?
					releasedActorPtr->GetInventoryChanges() :
					glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetInventoryChanges()
				);
				if (invChanges)
				{
					inventoryWeight = invChanges->totalWeight;
				}

				// Actors that are nearly or over-encumbered take more damage.
				float inventoryWeightFactor = 
				(
					1.0f + 
					(
						inventoryWeight / 
						max(coopActor->GetBaseActorValue(RE::ActorValue::kCarryWeight), 1.0f)
					)
				);
				
				// Gravity considerations. Needs balancing and is subject to change.
				const float fallHeightDiff = max
				(
					0.0f, a_fallHeight - releasedActorPtr->data.location.z
				);
				float gravDamageMult = 
				(
					2.5f + 
					(
						(1.5f) *
						(
							expf(0.004f * fallHeightDiff - 4.0f) - 
							expf(-0.004f * fallHeightDiff + 4.0f)
						) / 
						(
							expf(0.004f * fallHeightDiff - 4.0f) + 
							expf(-0.004f * fallHeightDiff + 4.0f)
						)
					)
				);
				float flopSelfDamageMult = 
				(
					releasedActorPtr == coopActor ? 
					Settings::vfFlopHealthCostMult[playerID] :
					1.0f
				);
				damage = 
				(
					(flopSelfDamageMult) * 
					(
						normOpposingVelocityFactor * 
						gravDamageMult * 
						havokImpactSpeed * 
						levelDamageFactor * 
						armorRatingFactor * 
						inventoryWeightFactor *
						(1.0f / static_cast<float>(max(1, a_hitCount)))
					) 
				);

				// REMOVE when done debugging.
				DBG
				(
					"{}: Thrown actor: {}. Mass: {}, impact speed: {}, actor linear speed: {}, "
					"armor rating and factor: {}, {}, inventory weight factor: {}, "
					"level damage factor: {}, flop self-damage mult: {}, "
					"fall height: {}, current: {}, diff: {}, grav damage mult: {}, "
					"normal opposing velocity factor: {}, knock state: {}. "
					"FINAL base damage: {}. Hit #{}",
					coopActor->GetName(),
					releasedActorPtr->GetName(),
					releasedRefrRigidBodyPtr->motion.GetMass(),
					havokImpactSpeed,
					Util::GetActorLinearVelocity(releasedActorPtr.get()).Length() * GAME_TO_HAVOK,
					releasedActorPtr->CalcArmorRating(),
					armorRatingFactor,
					inventoryWeightFactor,
					levelDamageFactor,
					flopSelfDamageMult,
					a_fallHeight,
					releasedActorPtr->data.location.z,
					fallHeightDiff,
					gravDamageMult,
					normOpposingVelocityFactor,
					releasedActorPtr->GetKnockState(),
					damage, 
					a_hitCount
				);
			}

			// Apply thrown object damage mult.
			if (damage != 0.0f)
			{
				damage *= Settings::vfThrownObjectDamageMult[playerID];
			}
		}

		
		// Inflict damage for each hit if thrown or if flopping,
		// but only on the first hit if dropped,
		// since we'll allow the game to apply fall damage for subsequent hits,
		// and we want to aggro the thrown actor, which is not possible with fall damage alone.
		if (a_wasThrown || isFlopping || a_hitCount == 1)
		{
			// IMPORTANT:
			// Calling DoDamage() will apply the given damage,
			// but if it is called as the first hit that begins combat 
			// between a companion player and an NPC, 
			// grabbed and redirected projectiles do not inflict damage 
			// when thrown back at the NPC.

			releasedActorPtr->DoDamage(damage, coopActor.get(), true);
		}

		// Only send a hit event and hit data on the first hit, since there can be hundreds
		// of collisions and there is no need to signal a 'splat' hit or draw aggro more than once.
		// Also, play sound and send detection event only on the first hit.
		// Will prevent a barrage of sounds effects from playing at once, 
		// overlapping and amplifying the sound to deep-fried audio levels.
		if (a_hitCount > 1)
		{
			return;
		}

		// Power attack flag to add compatibility with Maximum Carnage,
		// which triggers gore effects on power attack kills.
		// Set splat hit flag as well.
		SKSE::stl::enumeration<RE::TESHitEvent::Flag, std::uint8_t> hitFlags{ };
		hitFlags.set
		(
			RE::TESHitEvent::Flag::kPowerAttack, 
			static_cast<RE::TESHitEvent::Flag>(AdditionalHitEventFlags::kSplat)
		);
		
		// Send hit data last to draw aggro (0 damage) towards the throwing player.
		// Can apply the hit and do damage if non-zero or the target is hostile.
		bool isHostile = 
		(
			(!GlobalCoopData::IsCoopPlayer(releasedActorPtr.get())) &&
			(
				(releasedActorPtr->IsHostileToActor(coopActor.get())) || 
				(
					Util::HandleIsValid(releasedActorPtr->currentCombatTarget) &&
					Util::IsPartyFriendlyActor
					(
						releasedActorPtr->currentCombatTarget.get().get()
					)
				)
			)
		);
		const bool triggerCombat = 
		(
			(
				(isHostile) || 
				(!releasedActorPtr->IsGhost() && !releasedActorPtr->IsInvulnerable())
			) &&
			(!Util::IsDialogueTarget(releasedActorPtr.get())) &&
			(
				Settings::vbFriendlyFire[playerID] || 
				isFlopping || 
				!Util::IsPartyFriendlyActor(releasedActorPtr.get())
			) 
		);
		Util::ApplyHit
		(
			coopActor.get(),
			releasedActorPtr.get(),
			0.0f,
			triggerCombat,
			true,
			0.0f,
			0.0f,
			coopActor->GetHandle(),
			releasedActorPtr->formID,
			hitFlags
		);

		if (canSMORF && wantsToSMORF && releasedActorPtr == coopActor)
		{
			isSMORFing = true;
			wantsToSMORF = false;
			rmm->ClearReleasedRefr(coopActor->GetHandle());
			SetIsGrabbing(true);
			rmm->AddGrabbedRefr(p, coopActor->GetHandle());
			SetCrosshairMessageRequest
			(
				CrosshairMessageType::kActivationInfo,
				fmt::format
				(
					"P{}: <font color=\"#FFD766\">"
					"Cheese for everyone!</font>",
					playerID + 1
				),
				{
					CrosshairMessageType::kNone,
					CrosshairMessageType::kEquippedItem,
					CrosshairMessageType::kStealthState,
					CrosshairMessageType::kTargetSelection 
				},
				Settings::fSecsBetweenDiffCrosshairMsgs
			);

			DeactivateCrosshair();
		}

		auto audioManager = RE::BSAudioManager::GetSingleton(); 
		if (!audioManager)
		{
			return;
		}
				
		RE::BSSoundHandle handle{ };
		RE::BGSSoundDescriptorForm* flopSFX =
		(
			RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(0xAF664)
		);
		if (!flopSFX)
		{	
			return;
		}

		bool succ = audioManager->BuildSoundDataFromDescriptor(handle, flopSFX);
		if (succ)
		{
			handle.SetPosition(releasedActorPtr->data.location);
			auto actor3DPtr = Util::GetRefr3D(releasedActorPtr.get());
			if (actor3DPtr)
			{
				handle.SetObjectToFollow(actor3DPtr.get());
				handle.SetVolume(min(1.0f, havokImpactSpeed / 5.0f));
				handle.Play();
			}
		}
		
		// Send detection event for the aggressor player.
		Util::SetActorsDetectionEvent
		(
			coopActor.get(), 
			releasedActorPtr.get(), 
			releasedRefrRigidBodyPtr ? 
			releasedRefrRigidBodyPtr->motion.GetMass() : 
			0.0f,
			releasedActorPtr->data.location
		);
	}

	void TargetingManager::IsRefrInRangeAndInFOV
	(
		RE::TESObjectREFR* a_sourceRefr,
		RE::TESObjectREFR* a_targetRefr,
		const bool a_includeAngleWeight,
		const bool a_useXYDistance,
		const bool a_targetIsHostile,
		const bool a_preferScreenspaceSelection,
		const float a_screenTargetingAngle, 
		const float a_worldTargetingAngle,
		const float a_fovRads,
		const float a_range,
		float& a_angDistWeightOut,
		bool& a_isInRangeAndFOVOut
	)
	{
		// Using screenspace positions:
		// Top left of screen is origin, right is +X, left is -X, down is +Y, up is -Y.
		// 
		// The distance factor is comprised of the normalized distance between the source refr's pos 
		// and the target refr pos.
		// The angle factor, if requested, is comprised of the normalized angle difference 
		// between the targeting angle and the angle from the source refr to the target refr.
		
		// Set outparams as not in range/FOV and not having a valid angle/distance weight.
		// Can then return early if the refr is invalid or not in range/FOV.
		a_isInRangeAndFOVOut = false;
		a_angDistWeightOut = FLT_MAX;

		if (!a_targetRefr)
		{
			return;
		}

		if (!a_sourceRefr)
		{
			a_sourceRefr = coopActor.get();
		}

		auto sourcePos = RE::NiPoint3();
		if (a_sourceRefr == coopActor.get())
		{
			sourcePos = p->mm->playerTorsoPosition;
		}
		else if (auto asActor = a_sourceRefr->As<RE::Actor>(); asActor)
		{
			sourcePos = Util::GetTorsoPosition(asActor);
		}
		else
		{
			sourcePos = Util::GetRefrPosition(a_sourceRefr);
		}

		auto asActor = a_targetRefr->As<RE::Actor>();
		auto targetPos = RE::NiPoint3();
		if (auto pIndex = GlobalCoopData::GetCoopPlayerIndex(a_targetRefr); pIndex != -1)
		{
			targetPos = glob.coopPlayers[pIndex]->mm->playerTorsoPosition;
		}
		else if (auto asActor = a_targetRefr->As<RE::Actor>(); asActor)
		{
			targetPos = Util::GetTorsoPosition(asActor);
		}
		else
		{
			targetPos = Util::GetRefrPosition(a_targetRefr);
		}

		// Normalize to have the same range as the targeting angle.
		float targetingAngle = 0.0f;
		float angleToTarget = 0.0f;
		auto sourceScreenPos = RE::NiPoint3();
		bool isSourceOnScreen = Util::PointIsOnScreen(sourcePos, sourceScreenPos, 0.0f, false);
		auto targetScreenPos = RE::NiPoint3();
		bool isTargetOnScreen = Util::PointIsOnScreen(targetPos, targetScreenPos, 0.0f, false);
		// Use worldspace positions when either position is offscreen, 
		// since it's easier to conceptualize the stick angle required to point 
		// from the player to the target in this situation.
		// Otherwise, if both positions are on screen, only use the worldspace angle/positions
		// if the player has their preferred check type set to worldspace,
		// and the function caller did not prefer screenspace selection.
		if (!isSourceOnScreen || !isTargetOnScreen || !a_preferScreenspaceSelection)
		{
			targetingAngle = a_worldTargetingAngle;
			angleToTarget = Util::NormalizeAng0To2Pi
			(
				Util::GetYawBetweenPositions(sourcePos, targetPos)
			);
		}
		else
		{
			targetingAngle = a_screenTargetingAngle;
			sourceScreenPos.z = 0.0f;
			targetScreenPos.z = 0.0f;
			angleToTarget = Util::NormalizeAng0To2Pi
			(
				atan2f
				(
					targetScreenPos.y - sourceScreenPos.y, 
					targetScreenPos.x - sourceScreenPos.x
				)
			);
		}
		
		// Angle diff between the analog stick's angle 
		// and the angle between the source and the target.
		const float turnToFaceRefrAngMag = fabsf
		(
			Util::NormalizeAngToPi(angleToTarget - targetingAngle)
		);
		// Within FOV.
		const bool inFOV = turnToFaceRefrAngMag <= (a_fovRads / 2.0f);
		// Don't need to check range if not in FOV.
		if (!inFOV)
		{
			DBG
			(
				"{}: {} is not in FOV: targeting angle: {}, angle to target: {}, "
				"turn to target: {}, FOV: {}. {}",
				coopActor->GetName(),
				a_targetRefr->GetName(),
				targetingAngle * TO_DEGREES, 
				angleToTarget * TO_DEGREES,
				turnToFaceRefrAngMag * TO_DEGREES,
				a_fovRads * TO_DEGREES,
				!isSourceOnScreen || !isTargetOnScreen || !a_preferScreenspaceSelection ?
				"WORLDSPACE" :
				"SCREENSPACE"
			);
			return;
		}
		
		// Disregard range when set to -1.
		bool useRange = a_range != -1.0f;
		// Get distance between player (NOT source) and close refr position.
		float distanceFromPlayer = FLT_MAX;
		if (a_useXYDistance)
		{
			distanceFromPlayer = Util::GetXYDistance(targetPos, p->mm->playerTorsoPosition);
		}
		else
		{
			distanceFromPlayer = targetPos.GetDistance(p->mm->playerTorsoPosition);
		}
		
		// If the target is not flagged as hostile, the selection range is decreased
		// to prevent selection of a rabbit hiding in a bush 3 holds over.
		float considerationRange = 
		(
			a_targetIsHostile || GlobalCoopData::IsCoopPlayer(a_targetRefr) ?
			a_range : 
			min(a_range, Settings::fMaxNonHostileAimCorrectionTargetDistance)
		);
		// Return false if this actor is not in range.
		// No need to compare distance-angle weight.
		if (useRange && distanceFromPlayer > considerationRange)
		{
			DBG
			(
				"{}: {} is too far away: range: {}, distance from player (source: {}): {}.",
				coopActor->GetName(), 
				a_targetRefr->GetName(),
				considerationRange,
				a_sourceRefr->GetName(),
				distanceFromPlayer
			);
			return;
		}
		
		const float distanceFromSource = sourcePos.GetDistance(targetPos);
		if (a_range == -1.0f)
		{
			a_angDistWeightOut = min
			(
				1.0f, distanceFromSource / Settings::fMaxRaycastAndZoomOutDistance
			);
		}
		else
		{
			a_angDistWeightOut = min(1.0f, distanceFromSource / a_range);
		}

		// Include the ratio of the angle diff to the target over the FOV window angle.
		if (a_includeAngleWeight)
		{
			a_angDistWeightOut += turnToFaceRefrAngMag / (a_fovRads / 2.0f);
		}
		
		// Is in range and in FOV window.
		a_isInRangeAndFOVOut = true;
		DBG
		(
			"{}: {} -> {}: {}: targeting angle: {}, angle to target: {}, FOV: {}, "
			"turn to target: {}, distance to target: {} (reach: {}), "
			"selection factor computed: {}. Is in range and in FOV.",
			coopActor->GetName(),
			a_sourceRefr->GetName(),
			a_targetRefr->GetName(),
			!isSourceOnScreen || 
			!isTargetOnScreen || 
			!Settings::vbScreenspaceBasedAimCorrectionCheck[playerID] ? 
			"WORLDSPACE" :
			"SCREENSPACE",
			targetingAngle * TO_DEGREES, 
			angleToTarget * TO_DEGREES,
			a_fovRads * TO_DEGREES,
			turnToFaceRefrAngMag * TO_DEGREES,
			distanceFromSource,
			maxReachActivationDist,
			a_angDistWeightOut
		);
	
		// REMOVE when done debugging.
		/*if (a_isScreenspaceTargetingAngle)
		{
			glm::vec2 sourceScreenVec = glm::vec2(sourceScreenPos.x, sourceScreenPos.y);
			DebugAPI::ClampPointToScreen(sourceScreenVec);
			glm::vec2 targetScreenVec = glm::vec2(targetScreenPos.x, targetScreenPos.y);
			DebugAPI::ClampPointToScreen(targetScreenVec);
			DebugAPI::QueuePoint2D
			(
				glm::vec2(sourceScreenVec.x, sourceScreenVec.y),
				Settings::vuOverlayRGBAValues[playerID],
				2.0f,
				2.0f
			);
			DebugAPI::QueuePoint2D
			(
				targetScreenVec,
				Settings::vuCrosshairOuterOutlineRGBAValues[playerID],
				2.0f,
				2.0f
			);
			DebugAPI::QueueArrow2D
			(
				sourceScreenVec,
				targetScreenVec,
				Settings::vuOverlayRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
			glm::vec2 dir = glm::vec2
			(
				cosf(angleToTarget),
				sinf(angleToTarget)
			);
			DebugAPI::QueueArrow2D
			(
				sourceScreenVec,
				sourceScreenVec + dir * glm::distance(sourceScreenVec, targetScreenVec),
				Settings::vuCrosshairInnerOutlineRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
			dir = glm::vec2
			(
				cosf(a_targetingAngle),
				sinf(a_targetingAngle)
			);
			DebugAPI::QueueArrow2D
			(
				sourceScreenVec,
				sourceScreenVec + dir * 100.0f,
				Settings::vuCrosshairOuterOutlineRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
		}
		else
		{
			const auto sourcePosVec = ToVec3(sourcePos);
			const auto targetPosVec = ToVec3(targetPos);
			DebugAPI::QueuePoint3D
			(
				sourcePosVec,
				Settings::vuOverlayRGBAValues[playerID],
				2.0f,
				2.0f
			);
			DebugAPI::QueuePoint3D
			(
				targetPosVec,
				Settings::vuCrosshairOuterOutlineRGBAValues[playerID],
				2.0f,
				2.0f
			);
			DebugAPI::QueueArrow3D
			(
				sourcePosVec,
				targetPosVec,
				Settings::vuOverlayRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
			DebugAPI::QueueArrow3D
			(
				sourcePosVec,
				sourcePosVec + 
				100.0f * 
				ToVec3(Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(angleToTarget))),
				Settings::vuCrosshairInnerOutlineRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
			DebugAPI::QueueArrow3D
			(
				sourcePosVec,
				sourcePosVec + 
				100.0f * 
				ToVec3(Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(a_targetingAngle))),
				Settings::vuCrosshairOuterOutlineRGBAValues[playerID],
				2.0f,
				2.0f,
				2.0f
			);
		}*/
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
			(isSelf && !isSMORFing) || 
			(isCoopPlayer && !Settings::vbCanTargetOtherPlayers[playerID]) ||
			(!isCoopPlayer && glob.coopEntityBlacklistFIDSet.contains(refrPtr->formID))
		};
		if (isNotSelectableCoopEntity)
		{
			return false;
		}

		// Skip LOS checks if targeting another player.
		if (isCoopPlayer)
		{
			crosshairRefrInSight = true;
			return true;
		}

		// Check LOS if the chosen result is not the closest once,
		// meaning another refr is blocking at least part of the chosen refr.
		// Need to also check LOS if the player is not moving their crosshair,
		// since the target refr can become obscured by other objects
		// if it moves relative to the player.
		bool checkLOS = !choseClosestResult || !p->pam->IsPerforming(InputAction::kMoveCrosshair);
		// Check once when the chosen refr is first selected.
		bool newSelection = crosshairRefrHandle != a_refrHandle;
		// Initial LOS check.
		if (newSelection &&
			checkLOS && 
			!Util::HasLOS(refrPtr.get(), coopActor.get(), true, true, crosshairWorldPos))
		{
			// Can't select if there is no LOS.
			return false;
		}

		// New crosshair refr is on the screen, at least initially.
		if (newSelection)
		{
			crosshairRefrInSight = true;
			return true;
		}

		// Subsequent on-screen/LOS checks.
		secsSinceVisibleOnScreenCheck = Util::GetElapsedSeconds(p->crosshairRefrVisibilityCheckTP);
		if (secsSinceVisibleOnScreenCheck > Settings::fSecsBetweenTargetVisibilityChecks)
		{
			p->crosshairRefrVisibilityCheckTP = SteadyClock::now();
			bool wasVisible = crosshairRefrInSight;
			bool falseRef = false;
			auto refr3DPtr = Util::GetRefr3D(refrPtr.get());
			// No current 3D -> not on screen or visible.
			if (!refr3DPtr) 
			{
				return false;
			}

			// Check if target is on the screen first.
			// Use three positions on the refr.
			crosshairRefrInSight = 
			(
				Util::PointIsOnScreen(refrPtr->data.location) ||
				Util::PointIsOnScreen(refr3DPtr->worldBound.center) || 
				Util::PointIsOnScreen(refr3DPtr->world.translate)
			);
			// Then, if the target is on screen and an LOS check is warranted, perform LOS check.
			if (crosshairRefrInSight && checkLOS) 
			{
				crosshairRefrInSight = Util::HasLOS
				(
					refrPtr.get(), coopActor.get(), true, true, crosshairWorldPos
				);
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
				secsSinceTargetVisibilityLost = Util::GetElapsedSeconds
				(
					p->crosshairRefrVisibilityLostTP
				);
			}
		}

		// Give the player a grace period to regain sight of their targeted refr,
		// since the player or the target might move relative to one another
		// and the target may come back into view.
		bool invalidateAfterNotVisible = 
		(
			secsSinceTargetVisibilityLost > Settings::fSecsWithoutLOSToInvalidateTarget
		);
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

	Raycast::RayResult TargetingManager::PickCrosshairRefr
	(
		bool a_inCombat,
		bool a_crosshairActiveForSelection,
		bool a_showDebugPrints
	)
	{
		// EXPERIMENTAL. Unused for now.
		// Check if a selectable refr is highlighted by the crosshair
		// and pick it as the crosshair refr.

		Raycast::RayResult result{ }; 
		const auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr)
		{
			return result;
		}

		Util::ForEachReferenceInRange
		(
			glob.cam->GetCurrentPosition(),
			Settings::fMaxRaycastAndZoomOutDistance,
			true,
			[this, niCamPtr](RE::TESObjectREFR* a_refr)
			{
				if (!a_refr)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				auto baseObject = a_refr->GetBaseObject();
				if (!baseObject)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				auto handle = a_refr->GetHandle();
				const auto refr3DPtr = Util::GetRefr3D(a_refr);
				if (!refr3DPtr)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				bool valid = Util::IsValidRefrForTargeting(a_refr);
				bool selectable = Util::IsSelectableRefr(a_refr);
				if (!valid || !selectable)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				bool inFrontOfCam = 
				{
					(
						Util::IsInFrontOfCam(refr3DPtr->worldBound.center) ||
						RE::NiCamera::BoundInFrustum(refr3DPtr->worldBound, niCamPtr.get())
					) ||
					(
						refr3DPtr->worldBound.center.Length() == 0.0f &&
						Util::IsInFrontOfCam(refr3DPtr->world.translate)
					)
				};
				if (!inFrontOfCam)
				{
					DBG
					(
						"{} (0x{:X}, 0x{:X}) is not in front of cam.", 
						a_refr->GetName(),
						a_refr->formID,
						*baseObject->formType
					);
					return RE::BSContainer::ForEachResult::kContinue;
				}

				RE::NiPoint3 boundMax{ };
				RE::NiPoint3 boundMin{ };
				RE::NiPoint3 boundCenter{ };
				RE::NiMatrix3 rotMat{ }; 
				float radius = 0.0f;
				auto asActor = a_refr->As<RE::Actor>();
				boundMax = a_refr->GetBoundMax();
				boundMin = a_refr->GetBoundMin();
				boundCenter = a_refr->data.location;
				bool isDead = a_refr->IsDead();
				bool isKnocked = 
				(
					asActor && asActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal
				);
				bool isRagdolled = asActor && asActor->IsInRagdollState();
				bool isUprightActor = asActor && !isDead && !isKnocked && !isRagdolled;
				if (isUprightActor)
				{
					// Offset halfway up the actor if upright.
					boundCenter = 
					(
						asActor->data.location + 
						RE::NiPoint3(0.0f, 0.0f, 0.5f * asActor->GetHeight())
					);
				}
				else if (auto refrHkpRigidBodyPtr = Util::GethkpRigidBody(a_refr); 
						 refrHkpRigidBodyPtr)
				{
					if ((asActor) && (isDead || isKnocked || isRagdolled))
					{
						// Centered at the rigid body's position when ragdolled.
						// The 3D center position is still upright, so we can't use it.
						boundCenter = ToNiPoint3
						(
							refrHkpRigidBodyPtr->motion.motionState.transform.translation *
							HAVOK_TO_GAME
						);
					}
					else
					{
						// 3D center pos otherwise.
						boundCenter = Util::Get3DCenterPos(a_refr);
					}

					// Grab bounds from collidable shape.
					if (refrHkpRigidBodyPtr->collidable.GetShape() &&
						refrHkpRigidBodyPtr->collidable.GetShape()->type == 
						RE::hkpShapeType::kBox)
					{
						auto shape = refrHkpRigidBodyPtr->collidable.GetShape();
						RE::hkTransform hkTrans{ };
						hkTrans.rotation.col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
						hkTrans.rotation.col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
						hkTrans.rotation.col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
						RE::hkAabb aabb{ };
						shape->GetAabbImpl(hkTrans, 0.0f, aabb);
						boundMax = ToNiPoint3(aabb.max) * HAVOK_TO_GAME;
						boundMin = ToNiPoint3(aabb.min) * HAVOK_TO_GAME;
					}
				}
			
				// Rotation from the refr's 3D.
				rotMat = refr3DPtr->world.rotate;
				if (boundMin == boundMax && boundMax.Length() == 0.0f)
				{
					// Fall back to the radius for the bounds.
					boundMax = 
					(
						RE::NiPoint3(0.0f, 1.0f, 0.0f) * refr3DPtr->worldBound.radius
					);
					boundMin = -boundMax;
				}
					
				radius = refr3DPtr->worldBound.radius;
				if (radius == 0.0f)
				{
					radius = a_refr->GetHeight() * 0.5f;
					if (radius == 0.0f)
					{
						radius = (boundMax - boundMin).Length() * 0.5f;
						if (radius == 0.0f)
						{
							return RE::BSContainer::ForEachResult::kContinue;
						}
					}
				}

				// Next fallback: halfway up the refr as the center position.
				if (boundCenter.Length() == 0.0f)
				{
					boundCenter = 
					(
						a_refr->data.location + 
						RE::NiPoint3(0.0f, 0.0f, 0.5f * a_refr->GetHeight())
					);
				}

				// Last fallback: bounds determined by half the refr's height.
				if (boundMin == boundMax && boundMax.Length() == 0.0f)
				{
					boundMax = 
					(
						RE::NiPoint3(0.0f, 1.0f, 0.0f) * 0.5f * a_refr->GetHeight()
					);
					boundMin = -boundMax;
				}
		
				// Offset from the bounding box's center to one of the corners 
				// along the positive X and Y axes.
				auto halfExtent = (boundMax - boundMin) / 2.0f;

				//
				// Compute the minimum and maximum X or Y screen coordinates from all the edges.
				//
			
				float maxCoord = -FLT_MAX;
				float minCoord = FLT_MAX;

				RE::NiPoint3 center2DPos{ };
				bool onScreen = Util::PointIsOnScreen(boundCenter, center2DPos, 0.0f, false);
				if (!onScreen)
				{
					return RE::BSContainer::ForEachResult::kContinue;
				}

				RE::NiPoint3 camUp = Util::RotationToDirectionVect
				(
					-(glob.cam->GetCurrentPitch() - PI / 2.0f), 
					Util::ConvertAngle(glob.cam->GetCurrentYaw())
				);
				float radius2D = 
				(
					Util::WorldToScreenPoint3(boundCenter + camUp * radius, false) -
					center2DPos
				).Length();
				const float currentCrosshairGap = 
				(
					Settings::vfCrosshairGapRadius[playerID] + crosshairOscillationData->current
				);
				if (glm::distance(crosshairScaleformPos, ToVec3(center2DPos)) > 
					radius2D + currentCrosshairGap)
				{
					DBG
					(
						"{} (0x{:X}, 0x{:X}) is more than {} ({} + {}) pixels "
						"({} game units) from the crosshair ({}).", 
						a_refr->GetName(),
						a_refr->formID,
						*baseObject->formType,
						radius2D + currentCrosshairGap,
						radius2D,
						currentCrosshairGap,
						radius,
						glm::distance(crosshairScaleformPos, ToVec3(center2DPos))
					);
					return RE::BSContainer::ForEachResult::kContinue;
				}
				
				DebugAPI::QueueCircle2D
				(
					glm::vec2(center2DPos.x, center2DPos.y),
					Settings::vuOverlayRGBAValues[playerID],
					16,
					radius2D, 
					3.0f
				);

				//
				// Get the endpoints of the bounding box.
				//
		
				// Top face.
				RE::NiPoint3 start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
				);
				RE::NiPoint3 end = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				// Bottom face.
				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				end = 
				(
					boundCenter +
					rotMat * 
					RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + 
					rotMat * 
					RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				// Connecting the faces.
				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + 
					rotMat * 
					RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				start = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
				);
				end = 
				(
					boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
				);
				DebugAPI::QueueLine3D
				(
					ToVec3(start),
					ToVec3(end),
					Settings::vuOverlayRGBAValues[playerID],
					3.0f,
					0.0f
				);

				return RE::BSContainer::ForEachResult::kContinue;
			}
		);	

		return result;
	}

	Raycast::RayResult TargetingManager::PickRaycastHitResult
	(
		const std::vector<Raycast::RayResult>& a_raycastResults,
		const bool& a_inCombat,
		const bool&& a_crosshairActiveForSelection,
		bool a_showDebugPrints
	)
	{
		// WARNING: SPAGHETTI SUPREME BELOW.
		// In dire need of a refactor at some point, and definitely needs thorough testing.
		// 
		// Choose crosshair raycast result to use for setting the crosshair world position 
		// and for selecting the next refr targeted by the player's crosshair.
		// Can filter out players, player teammates, and non-hostile actors when in combat.
		// Also, if choosing a result while the player's crosshair is active for selection,
		// meaning either moving or over a previous valid selection,
		// only choose hits with selectable refrs and ignore activators,
		// which do not have visible surfaces and would result in the crosshair world position 
		// floating in midair if selected.

		// Some scenarios for what the following code (I hope) should accomplish:
		// H = hostile
		// N = neutral/friendly
		// R = non-actor refr
		// P = player
		// O = object without refr
		// A = activator
		// 
		// [Combat]
		// H1 -> ...												Choose H1.
		// N1/P1 -> H1 -> ...										Choose H1.
		// N1/P1 -> N2/P2 -> No H/P...								Choose N1/P1.
		// N1/P1 -> N2/P2 -> H1 -> ...								Choose H1
		// N1 -> O1 -> H1 -> ...									Choose N1
		// N1/R1/O1 -> ... -> Not H -> ... -> P1 -> ...				Choose P1
		// N1/R1/O1 -> ... -> H1 -> ... -> P1 -> ...				Choose H1

		// [No Combat]
		// H1/N1/R1/O1 -> ... -> Not P -> ...						Choose H1/N1/R1/O1
		// H1/N1/R1/O1 -> ... -> P1 -> ...								Choose P1
		// 
		// [Activators]
		// A1 -> R1/H1/N1 -> ...			Choose R1/H1/N1
		// A1 -> Nothing/O1 -> ...			Choose A1
		// A1 -> A2 -> Nothing/O1 -> ...	Choose A1

		// Keep track of activator hits.
		bool activatorHit = false;
		// Can select other players.
		// If so, continue looking for a raycast that hits another player.
		bool canSelectOtherPlayers = Settings::vbCanTargetOtherPlayers[playerID];
		// Is the chosen result's hit refr selectable?
		bool chosenResultSelectable = false;
		// Blacklisted.
		bool excluded = false;
		// Is the chosen hit refr an activator?
		bool isChosenHitRefrAnActivator = false;
		// The hit object is designated as an obstruction between the camera and the player.
		bool isAnObstruction = false;
		// Must be in front of the camera to select.
		bool inFrontOfCam = false;
		// Was an object with no refr hit?
		bool objectNoRefrHit = false;

		// The current hit result's hit object is an activator.
		bool isActivator = false;
		// The current hit result's hit object is an EVGAT activator (should ignore).
		bool isEVGATActivator = false;
		// The current hit results's hit object is a hostile actor.
		bool isHostile = false;
		// The current hit result's hit object has no associated refr.
		bool isObjNoRefr = false;
		// The current hit result's hit object is another player.
		bool isOtherPlayer = false;
		// The current hit result's hit object is a non-actor refr.
		bool isRefr = false;

		// Crosshair raycast result to use for the crosshair world position and hit refr.
		Raycast::RayResult chosenResult{ };
		// No hit to start.
		chosenResult.hit = false;
		chosenResult.hitObjectPtr = nullptr;

		// No NiCam means no bounds-in-frustum checks, so return result early.
		auto niCamPtr = Util::GetNiCamera();
		if (!niCamPtr)
		{
			return chosenResult;
		}

		// Save hit indices to (un)set the closest hit result after the loop.
		uint32_t i = 0;
		int32_t chosenHitResultIndex = -1;
		int32_t chosenHitPosIndex = -1;
		int32_t firstNonActivatorHitIndex = -1;
		for (; i < a_raycastResults.size(); ++i)
		{
			auto& result = a_raycastResults[i];

			// REMOVE after debugging.
			RE::NiPoint3 hitPoint = ToNiPoint3(result.hitPos);
			if (a_showDebugPrints)
			{
				auto p1 = RE::PlayerCharacter::GetSingleton();
				DBG
				(
					"{}: For target selection: {}. "
					"Pre-parent recurse result {}: hit: {}, {} (refr name: {}, 0x{:X}, "
					"base name: {}, 0x{:X}, type: {}). "
					"Distance to camera: {}, distance to player: {}. Model: {}, hostile: {}. "
					"P1 parent cell: {} (0x{:X}), refr parent cell: {} (0x{:X}).",
					coopActor->GetName(),
					a_crosshairActiveForSelection,
					i,
					result.hit,
					result.hitObjectPtr ? result.hitObjectPtr->name.c_str() : "NONE",
					Util::HandleIsValid(result.hitRefrHandle) ? 
					result.hitRefrHandle.get()->GetName() : 
					"NONE",
					Util::HandleIsValid(result.hitRefrHandle) ? 
					result.hitRefrHandle.get()->formID : 
					0xDEAD,
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetObjectReference() ? 
					result.hitRefrHandle.get()->GetObjectReference()->GetName() : 
					"NONE",
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetObjectReference() ? 
					result.hitRefrHandle.get()->GetObjectReference()->formID : 
					0xDEAD,
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetObjectReference() ? 
					RE::FormTypeToString
					(
						*result.hitRefrHandle.get()->GetObjectReference()->formType
					) : 
					RE::FormTypeToString(RE::FormType::None),
					hitPoint.GetDistance(niCamPtr->world.translate),
					hitPoint.GetDistance(coopActor->data.location),
					Util::HandleIsValid(result.hitRefrHandle) && 
					result.hitRefrHandle.get()->HasWorldModel() ? 
					result.hitRefrHandle.get()->As<RE::TESModel>()->model : 
					"GUH",
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->As<RE::Actor>() ?
					result.hitRefrHandle.get()->As<RE::Actor>()->IsHostileToActor
					(
						coopActor.get()
					) || 
					result.hitRefrHandle.get()->As<RE::Actor>()->IsHostileToActor
					(
						glob.player1Actor.get()
					) :
					false,
					p1 && p1->parentCell ? p1->parentCell->GetName() : "NONE",
					p1 && p1->parentCell ? p1->parentCell->formID : 0xDEAD,
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetParentCell() ? 
					result.hitRefrHandle.get()->GetParentCell()->GetName() : 
					"NONE",
					Util::HandleIsValid(result.hitRefrHandle) &&
					result.hitRefrHandle.get()->GetParentCell() ? 
					result.hitRefrHandle.get()->GetParentCell()->formID : 
					0xDEAD
				);
			}
			
			// Must have hit an NiAVObject.
			if (!result.hit || !result.hitObjectPtr || !result.hitObjectPtr.get())
			{
				continue;
			}

			// Reset all per-hit flags.
			excluded =
			isAnObstruction = 
			inFrontOfCam =
			isActivator = 
			isEVGATActivator = 
			isHostile = 
			isObjNoRefr = 
			isOtherPlayer = 
			isRefr = false;
			auto hitRefrPtr = Util::GetRefrPtrFromHandle(result.hitRefrHandle);
			// If the hit refr is valid and selectable and not the player,
			// set crosshair refr and positional data.
			isObjNoRefr = !hitRefrPtr;
			if (isObjNoRefr)
			{
				// Hit object is a navmesh block or terrain without an associated refr.
				// Still have to check if in front of the cam and not an obstruction.
				// Not valid for target selection if so.
				inFrontOfCam = 
				{
					Util::IsInFrontOfCam(result.hitObjectPtr->world.translate) ||
					Util::IsInFrontOfCam(result.hitObjectPtr->worldBound.center) ||
					RE::NiCamera::BoundInFrustum(result.hitObjectPtr->worldBound, niCamPtr.get())
				};
				isAnObstruction = glob.cam->obstructionFadeDataMap.contains
				(
					result.hitObjectPtr
				);
				if (!inFrontOfCam || isAnObstruction)
				{
					continue;
				}

				// Set first non-activator hit index.
				if (firstNonActivatorHitIndex == -1)
				{
					if (a_showDebugPrints)
					{
						DBG
						(
							"{}: Set object no refr with {}: {}.",
							coopActor->GetName(),
							result.hitObjectPtr->name,
							firstNonActivatorHitIndex
						);
					}

					firstNonActivatorHitIndex = i;
				}
				
				objectNoRefrHit = true;
				if (a_crosshairActiveForSelection) 
				{
					if (canSelectOtherPlayers && !chosenResult.hit)
					{
						// First hit.
						// Set as hit position target, even if not selectable, for aiming purposes.
						chosenResult.hit = true;
						chosenResult.hitPos = result.hitPos;
						chosenHitPosIndex = i;
						chosenResultSelectable = false;
						if (a_showDebugPrints)
						{
							DBG
							(
								"{}: Set hit pos with first hit object no refr {}, i: {}.",
								coopActor->GetName(),
								result.hitObjectPtr->name,
								i
							);
						}
					}
					else if (!canSelectOtherPlayers)
					{
						if (!activatorHit)
						{
							// If not searching for another player and no activator was hit,
							// set the hit result to this object.
							// Otherwise, we'll use the hit activator's hit result.
							chosenResult = result;
							chosenHitResultIndex = chosenHitPosIndex = i;
							chosenResultSelectable = false;
							if (a_showDebugPrints)
							{
								DBG
								(
									"{}: Cannot select other players, no activator hit. "
									"Choose {}, i: {}.",
									coopActor->GetName(),
									result.hitObjectPtr->name,
									i
								);
							}
						}

						if (a_showDebugPrints)
						{
							DBG
							(
								"{}: Cannot select other players, break.",
								coopActor->GetName(),
								result.hitObjectPtr->name
							);
						}

						// Break either way.
						break;
					}
				}
				else
				{
					// If crosshair is not active for target selection,
					// choose this hit result for the crosshair position and stop looking.
					chosenResult = result;
					chosenHitPosIndex = chosenHitResultIndex = i;
					break;
				}
			}
			else
			{
				// Filter out self (if not SMORFing), current mount, non-targetable players,
				// and blacklisted actors.
				bool isCoopPlayer = glob.IsCoopPlayer(hitRefrPtr->formID);
				isOtherPlayer = isCoopPlayer && hitRefrPtr != coopActor;
				excluded =
				(
					(hitRefrPtr == coopActor && !isSMORFing) ||
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
					Util::IsInFrontOfCam(result.hitObjectPtr->world.translate) ||
					Util::IsInFrontOfCam(result.hitObjectPtr->worldBound.center) ||
					RE::NiCamera::BoundInFrustum(result.hitObjectPtr->worldBound, niCamPtr.get())
				};
				// Obstructions are hit on their 'outward-facing' surface 
				// by the crosshair raycast, which is not a surface visible to the players
				// that are beyond the obstruction, so exclude such objects from determining
				// the crosshair's world position and selected refr.
				isAnObstruction = glob.cam->obstructionFadeDataMap.contains
				(
					result.hitObjectPtr
				);
				if (excluded || !inFrontOfCam || isAnObstruction)
				{
					if (a_showDebugPrints)
					{
						DBG
						(
							"{}: Skipping refr {}. Excluded: {}, "
							"in front of cam: {}, obstruction: {}.",
							coopActor->GetName(),
							hitRefrPtr->GetName(),
							excluded,
							inFrontOfCam,
							isAnObstruction
						);
					}

					continue;
				}
				
				auto baseObj = hitRefrPtr->GetObjectReference();
				if (!baseObj && result.hitObjectPtr->userData)
				{
					baseObj = result.hitObjectPtr->userData->GetObjectReference();
				}

				isActivator = 
				{
					baseObj && baseObj->Is(RE::FormType::Activator, RE::FormType::TalkingActivator)
				};
				// Skip EVGAT activators since they should not be targeted for activation.
				isEVGATActivator = 
				(
					isActivator && 
					baseObj->GetFile() &&
					std::string(baseObj->GetFile()->fileName).find("EVG") != std::string::npos
				);
				if (isEVGATActivator)
				{
					continue;
				}

				// Save previous activator hit state to check if this is the first activator hit.
				bool activatorWasHit = activatorHit;
				// An activator was hit in this group of hit results.
				if (isActivator && !activatorHit)
				{
					activatorHit = true;
				}

				// Moving the crosshair, can select a crosshair refr.
				if (a_crosshairActiveForSelection) 
				{
					// If not looking for a player,
					// we can stop checking once an object without a refr is hit.
					if (objectNoRefrHit && !canSelectOtherPlayers)
					{
						if (a_showDebugPrints)
						{
							DBG
							(
								"{}: Hit object with no refr and cannot select other players. "
								"Break.",
								coopActor->GetName()
							);
						}

						break;
					}
					
					// 
					// First, check if selectable and handle invalid objects.
					//
					
					auto asActor = hitRefrPtr->As<RE::Actor>();
					isRefr = !asActor;
					bool isSelectable = Util::IsSelectableRefr(hitRefrPtr.get());
					bool validType = isSelectable;
					if (a_inCombat)
					{
						// TODO:
						// Implement additional actor combat targeting options.

						// Check if a hostile actor was hit while in combat.
						isHostile = 
						(
							(asActor) && 
							(!asActor->IsDead()) &&
							(
								asActor->IsHostileToActor(coopActor.get()) || 
								asActor->IsHostileToActor(glob.player1Actor.get())
							)
						);
					}

					// Not a valid type to use for selection,
					// but still set the hit position to use as the crosshair world position
					// if no other result was chosen yet and if the hit object is not an activator.
					if (!validType)
					{
						// Set first non-activator hit index.
						if (firstNonActivatorHitIndex == -1 && !isActivator)
						{
							firstNonActivatorHitIndex = i;
						}

						// Do not set chosen hit pos to activator's hit pos, 
						// since activators have no collision.
						if (!chosenResult.hit && !isActivator) 
						{
							chosenResult.hit = true;
							chosenResult.hitPos = result.hitPos;
							chosenHitPosIndex = i;
							chosenResultSelectable = isSelectable;
						}

						if (a_showDebugPrints)
						{
							DBG
							(
								"{}: {} is not valid, selectable: {}. "
								"Continue. Set result: {}. Non activator index: {}",
								coopActor->GetName(),
								hitRefrPtr->GetName(),
								isSelectable, 
								!chosenResult.hit && !isActivator,
								firstNonActivatorHitIndex
							);
						}

						// Keep looking for a result to use for crosshair refr selection.
						continue;
					}
					
					if (a_showDebugPrints)
					{
						DBG
						(
							"{}: {} at index {}. Hostile: {}, object with no refr hit: {}.",
							coopActor->GetName(),
							hitRefrPtr->GetName(),
							i,
							isHostile,
							objectNoRefrHit
						);
					}

					//
					// Can break out of the loop early, if selecting a hostile actor or player.
					//

					// If in combat and hitting a hostile actor,
					// choose the hostile actor result straight away and exit.
					bool chooseHostileActorInCombat = 
					(
						a_inCombat && isHostile
					);
					// Stop iterating through the hit results if another player was hit 
					// while outside of combat.
					bool chooseOtherPlayerOutOfCombat = 
					(
						!a_inCombat && 
						canSelectOtherPlayers && 
						isOtherPlayer
					);
					// Choose the hit player if another one was not already hit.
					bool chooseOtherPlayerInCombat = 
					(
						a_inCombat &&
						isOtherPlayer && 
						!GlobalCoopData::IsCoopPlayer(chosenResult.hitRefrHandle)
					);
					if (chooseHostileActorInCombat || 
						chooseOtherPlayerOutOfCombat ||
						chooseOtherPlayerInCombat)
					{
						if (firstNonActivatorHitIndex == -1)
						{
							firstNonActivatorHitIndex = i;
						}

						chosenResult = result;
						chosenHitPosIndex = chosenHitResultIndex = i;
						isChosenHitRefrAnActivator = false;
						chosenResultSelectable = true;
						if (chooseOtherPlayerInCombat)
						{
							if (a_showDebugPrints)
							{
								DBG
								(
									"{}: Chose player {}. Continuing. i: {}.",
									coopActor->GetName(),
									hitRefrPtr->GetName(),
									i
								);
							}

							// Continue looking for a hostile actor hit.
							continue;
						}
						else
						{
							// Already hit an endpoint result: 
							// 1. Hostile actor in combat -OR-
							// 2. Player when out of combat
							// So we can exit.
							if (a_showDebugPrints)
							{
								DBG
								(
									"{}: Chose hit refr {}, hostile: {}, "
									"player out of combat: {}. Breaking. i: {}.",
									coopActor->GetName(),
									hitRefrPtr->GetName(),
									chooseHostileActorInCombat,
									chooseOtherPlayerOutOfCombat,
									i
								);
							}

							break;
						}
					}
					
					// 
					// Set a new hit result and potentially continue looking.
					//

					// If a non-activator has not been hit yet, 
					// or if the currently chosen result's refr is not selectable
					// we can potentially update the chosen result.
					// Also, if this is the first activator hit, set the result.
					// However, in the case of consecutive activators, 
					// we do not want to set a subsequent activator's hit result
					// as the chosen one, since it would be further away than the first one.
					if ((firstNonActivatorHitIndex == -1 || !chosenResultSelectable) && 
						((!isActivator || !activatorWasHit)))
					{
						chosenResult = result;
						chosenHitPosIndex = chosenHitResultIndex = i;
						isChosenHitRefrAnActivator = isActivator;
						chosenResultSelectable = true;
						if (a_showDebugPrints)
						{
							DBG
							(
								"{}: Hit and set result {}. i: {}.",
								coopActor->GetName(),
								hitRefrPtr->GetName(),
								i
							);
						}
							
						if (!isActivator)
						{
							// Set first non activator index if not hitting an activator.
							if (firstNonActivatorHitIndex == -1)
							{
								firstNonActivatorHitIndex = i;
							}

							// Stop searching if not looking for a player,
							// since we've now hit a non-activator
							// and do not need to continue searching for one 
							// behind already-hit activators.
							if (!canSelectOtherPlayers)
							{
								if (a_showDebugPrints)
								{
									DBG
									(
										"{}: Chose {}. Breaking. Cannot select other players.",
										coopActor->GetName(),
										hitRefrPtr->GetName()
									);
								}

								break;
							}

							if (a_showDebugPrints)
							{
								DBG
								(
									"{}: Non-activator index with {} is now: {}.",
									coopActor->GetName(),
									hitRefrPtr->GetName(),
									firstNonActivatorHitIndex
								);
							}
						}
					}
				}
				else
				{
					// Since we only need the raycast hit result in order to set 
					// the crosshair world position when not attempting to select a refr, 
					// ignore activator hits.
					// Their hit position should not be used for the crosshair world position,
					// since it will affect the target position for drawn projectile trajectories.
					if (isActivator) 
					{
						continue;
					}
					else
					{
						// Set first non-activator hit index.
						if (firstNonActivatorHitIndex == -1)
						{
							firstNonActivatorHitIndex = i;
						}

						chosenResult = result;
						chosenHitPosIndex = chosenHitResultIndex = i;
						isChosenHitRefrAnActivator = false;

						break;
					}
				}
			}
		}

		// Closest result is the first non-activator one.
		choseClosestResult = chosenResult.hit && chosenHitResultIndex == firstNonActivatorHitIndex;

		if (a_showDebugPrints)
		{
			auto hitRefrPtr = Util::GetRefrPtrFromHandle(chosenResult.hitRefrHandle);
			DBG
			(
				"{}: chose result {}, for hit: {}. {} (0x{:X}, type: {}). "
				"For target selection: {}, is closest result: {} (first non-activator hit: {}). "
				"Searching for player target: {}.",
				coopActor->GetName(),
				chosenHitResultIndex,
				chosenHitPosIndex,
				hitRefrPtr ?
				hitRefrPtr->GetName() : 
				chosenResult.hitObjectPtr ? 
				chosenResult.hitObjectPtr->name : 
				"NONE",
				hitRefrPtr ? hitRefrPtr->formID : 0xDEAD,
				hitRefrPtr && hitRefrPtr->GetObjectReference() ? 
				RE::FormTypeToString(*hitRefrPtr->GetObjectReference()->formType) :
				RE::FormTypeToString(RE::FormType::None),
				a_crosshairActiveForSelection, 
				choseClosestResult, 
				firstNonActivatorHitIndex,
				canSelectOtherPlayers
			);
		}
		
		crosshairRefrFromRaycast = true;

		// Failsafe that is commented out for now.
		// Not optimized, obviously, and results in a 7-10+% FPS loss 
		// with two players moving their crosshairs around objects.
		// Only necessary until I get around to figuring out why 
		// raycasts do not hit certain refrs, even when casting directly through them
		// and not filtering out any hits.
		// Examples of such objects include certain doors, such as the Mzinchaleft interior door,
		// and tankards.
		/*
		auto closestSelectableRefrPtr = Util::GetRefrPtrFromHandle
		(
			GetClosestSelectableRefrToCrosshairRay()
		);
		if (closestSelectableRefrPtr)
		{
			const auto refrPos = Util::Get3DCenterPos(closestSelectableRefrPtr.get());
			// Alternative choice if the raycast fails to find a selectable refr,
			// or skips over a closer selectable refr.
			bool doNotUseRaycastResult = 
			(
				(chosenHitResultIndex == -1) ||
				(
					chosenResult.hit && 
					refrPos.GetDistance(glob.cam->camTargetPos) < 
					ToNiPoint3(chosenResult.hitPos).GetDistance(glob.cam->camTargetPos)
				)
			);
			if (doNotUseRaycastResult)
			{
				const auto refr3DPtr = Util::GetRefr3D(closestSelectableRefrPtr.get());
				RE::NiPoint3 origin{ };
				RE::NiPoint3 dir{ };
				niCamPtr->WindowPointToRay
				(
					crosshairScaleformPos.x, 
					crosshairScaleformPos.y, 
					origin, 
					dir, 
					DebugAPI::screenResX, 
					DebugAPI::screenResY
				);
				RE::NiPoint3 toRefrDir = 
				(
					refr3DPtr ? 
					refr3DPtr->worldBound.center - origin :
					closestSelectableRefrPtr->data.location - origin
				);
				float distToRefr = toRefrDir.Length();
				toRefrDir.Unitize();
				float distAlongRay = distToRefr * toRefrDir.Dot(dir);

				chosenResult.hit = true;
				chosenResult.hitObjectPtr = refr3DPtr;
				// Not necessarily on or within the hit refr's bounds, 
				// but close enough for our needs.
				chosenResult.hitPos = ToVec4(origin + dir * distAlongRay);
				chosenResult.hitRefrHandle = closestSelectableRefrPtr->GetHandle();
				// Not using the raycast hit result.
				crosshairRefrFromRaycast = false;
			}
		}
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
		float maxCheckDist = GetMaxActivationDist();
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
			return 
			(
				coopActor->data.location.GetDistance(refrPtr->data.location) <= 
				Settings::fMaxDistToRevive
			);
		}
		else
		{
			// If the refr is crosshair-selected, 
			// compare the max check distance with the distance from the crosshair world position.
			// Otherwise, compare the max check distance
			// with the smaller of the distance to the refr position
			// and the distance to the refr's center position.
			// We pick the smaller of the two since for certain refrs, especially activators,
			// the center position might be very far from the refr's reported location,
			// and we only need one or the other to be in range for activation.
			if (a_refrHandle == crosshairRefrHandle)
			{
				return crosshairWorldPos.GetDistance(p->mm->playerTorsoPosition) <= maxCheckDist;
			}
			else
			{
				return 
				(
					min
					(
						p->mm->playerTorsoPosition.GetDistance(refrPtr->data.location),
						p->mm->playerTorsoPosition.GetDistance(Util::Get3DCenterPos(refrPtr.get()))
					) <= maxCheckDist
				);
			}
		}
	}

	void TargetingManager::ResetTPs()
	{
		// Reset all player timepoints handled by this manager to the current time.

		p->crosshairLastActiveTP				=
		p->lastActivationTargetChangeTP			=
		p->lastAimCorrectionTargetSetTP			=
		p->lastAutoGrabTP						=
		p->lastCrosshairTargetChangeTP			=
		p->lastCrosshairUpdateTP				=
		p->lastHiddenInStealthRadiusTP			=
		p->lastLockOnAimTargetChangeTP			=
		p->lastStealthStateCheckTP				=
		p->crosshairRefrVisibilityLostTP		= 
		p->crosshairRefrVisibilityCheckTP		= SteadyClock::now();
	}

	void TargetingManager::SetLockOnAimTarget
	(
		bool a_useLeftStickAngle, bool a_fromCurrentTarget, bool a_selectOnHold
	)
	{
		// Find and set a lock on aim target (NPC), if any.
		// Use the left/right stick's angle as the targeting angle.
		// Originate the check from the player's position or from the current target's position.
		// Select the target if a bind is held or on press. 
		// Selecting on hold will select at an interval, instead of right away.

		// Evaluate for a new target in the direction of the player's analog stick.
		const auto prevHandle = 
		(
			aimMode == AimMode::kCrosshair ? crosshairRefrHandle : aimCorrectionTargetHandle
		);
		auto newHandle = GetLockOnTarget
		(
			prevHandle, true, a_useLeftStickAngle, a_fromCurrentTarget, a_selectOnHold
		);

		// Update chose lock on target flag.
		choseLockOnAimTarget = Util::HandleIsValid(newHandle);
		if (choseLockOnAimTarget)
		{
			// Set crosshair refr handle and selected target actor handle to the target's handle
			// if the crosshair is active; otherwise, set the aim correction handle.
			if (aimMode == AimMode::kCrosshair)
			{
				crosshairRefrHandle = newHandle;
				if (newHandle.get() && newHandle.get()->As<RE::Actor>())
				{
					selectedTargetActorHandle = newHandle.get()->As<RE::Actor>()->GetHandle();
				}
				else
				{
					selectedTargetActorHandle = RE::ActorHandle();
				}
			}
			else
			{
				if (newHandle.get() && newHandle.get()->As<RE::Actor>())
				{
					aimCorrectionTargetHandle = newHandle.get()->As<RE::Actor>()->GetHandle();
				}
				else
				{
					aimCorrectionTargetHandle = RE::ActorHandle();
				}
			}

			// Switch to different-colored activation shader later.
			if (newHandle != prevHandle) 
			{
				// Update activation target handle.
				// Remove all previous activation shaders.
				if (Util::HandleIsValid(prevHandle))
				{
					Util::StopAllActivationEffectShaders(prevHandle.get().get(), playerID);
				}

				ColorizeActivationShader(glob.activateHighlightShaders[playerID], true);
				Util::StartEffectShader
				(
					newHandle.get().get(),
					glob.activateHighlightShaders[playerID],
					max(0.1f, Settings::fSecsBetweenActivationChecks)
				);
			}

			DBG
			(
				"{}: Has chosen NPC {} as lock on target.", 
				coopActor->GetName(), 
				newHandle.get()->GetName()
			);
		}
		else
		{
			ClearAimTargetData();
			DBG
			(
				"{}: No chosen NPC lock on target. Time since change: {}s. "
				"Previous handle: {}", 
				coopActor->GetName(), 
				Util::GetElapsedSeconds(p->lastLockOnAimTargetChangeTP),
				prevHandle.get() ?
				prevHandle.get()->GetName() :
				"NONE"
			);
		}

		if (newHandle != prevHandle)
		{
			p->lastLockOnAimTargetChangeTP = SteadyClock::now();
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
		auto selectedTargetActorPtr = Util::GetActorPtrFromHandle
		(
			aimMode == AimMode::kTwinStick ? 
			aimCorrectionTargetHandle : 
			selectedTargetActorHandle
		); 
		// Should update set TP, even if the crosshair message is the same.
		bool updateSetTP = false;
		if (a_type == CrosshairMessageType::kTargetSelection)
		{
			if (selectedTargetActorPtr && !selectedTargetActorPtr->IsDead())
			{
				// Alive actor. Show name and level.
				// Passive actors' names are displayed in white,
				// pacifiable actors' names are displayed in pink,
				// and enemy actors' names are displayed in red.
				auto levelRGB = GetLevelDifferenceRGB
				(
					aimMode == AimMode::kTwinStick ? 
					aimCorrectionTargetHandle : 
					selectedTargetActorHandle
				);
				text = fmt::format
				(
					"P{}: {} <font color=\"#{:X}\">L{}</font> <font color=\"#{:X}\">{}</font>",
					playerID + 1, crosshairActive ? "Facing" : "Targeting",
					levelRGB, selectedTargetActorPtr->GetLevel(),
					!selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ? 
					0xFFFFFF : 
					Util::CanStopCombatWithActor(selectedTargetActorPtr.get()) ?
					0xFFBBBB :
					0xFF0000,
					selectedTargetActorPtr->GetDisplayFullName()
				);
				type = a_type;
				// Refresh set TP to keep the crosshair text from fading when moving the crosshair.
				updateSetTP = p->pam->IsPerforming(InputAction::kMoveCrosshair);
			}
			else
			{
				auto selectedRefrPtr = 
				(
					Util::HandleIsValid(activationRefrHandle) ? 
					Util::GetRefrPtrFromHandle(activationRefrHandle) : 
					Util::HandleIsValid(crosshairRefrHandle) ? 
					Util::GetRefrPtrFromHandle(crosshairRefrHandle) : 
					RE::TESObjectREFRPtr()
				);
				if (selectedRefrPtr)
				{
					// Notify the player that they should sneak to activate.
					bool isOffLimits = Util::ActivationIsOffLimits
					(
						coopActor.get(), selectedRefrPtr.get()
					); 
					bool shouldSneakToActivate = isOffLimits && !coopActor->IsSneaking();
					// Get activation text for the crosshair refr.
					bool hasActivationText = false;
					auto baseObj = selectedRefrPtr->GetObjectReference();
					text = Util::GetActivationText
					(
						coopActor.get(),
						baseObj, 
						selectedRefrPtr.get(),
						hasActivationText
					);
					if (hasActivationText && baseObj)
					{
						bool isBook = baseObj->IsBook();
						bool isNote = baseObj->IsNote();
						bool wouldPickupBookNote = 
						(
							(isBook || isNote) &&
							(
								!GlobalCoopData::CanControlMenus(playerID)
							)
						);
						if (shouldSneakToActivate)
						{
							if (wouldPickupBookNote)
							{
								text = fmt::format
								(
									"P{}: Sneak to <font color=\"#FF0000\">Steal</font> {}", 
									playerID + 1, selectedRefrPtr->GetName()
								);
							}
							else
							{
								text = fmt::format
								(
									"P{}: Sneak to {}", p->playerID + 1, text
								);
							}
						}
						else
						{
							if (wouldPickupBookNote)
							{
								if (isOffLimits)
								{
									text = fmt::format
									(
										"P{}: <font color=\"#FF0000\">Steal</font> {}", 
										playerID + 1,
										selectedRefrPtr->GetName()
									);
								}
								else
								{
									text = fmt::format
									(
										"P{}: Take {}", playerID + 1, selectedRefrPtr->GetName()
									);
								}
							}
							else
							{
								text = fmt::format
								(
									"P{}: {}", p->playerID + 1, text
								);
							}
						}
					}
					else
					{
						if (shouldSneakToActivate)
						{
							text = fmt::format
							(
								"P{}: Sneak to "
								"<font color=\"#FF0000\">interact</font> with {}",
								p->playerID + 1, text
							);
						}
						else
						{
							if (isOffLimits)
							{
								text = fmt::format
								(
									"P{}: <font color=\"#FF0000\">Interact</font> with {}", 
									p->playerID + 1, text
								);
							}
							else
							{
								text = fmt::format
								(
									"P{}: Interact with {}", p->playerID + 1, text
								);
							}
						}
					}

					int32_t value = -1;
					float weight = 0.0f;
					auto asActor = selectedRefrPtr->As<RE::Actor>();
					if ((asActor && asActor->IsDead()) || 
						(!asActor && selectedRefrPtr->GetContainer()))
					{
						// Get total weight and value in the container.
						Util::GetWeightAndValueInRefr(selectedRefrPtr.get(), weight, value);
					}
					else if (baseObj)
					{
						// Get weight and value for this individual refr.
						value = baseObj->GetGoldValue();
						weight = selectedRefrPtr->GetWeight();
					}

					if (value >= 0)
					{
						float inventoryWeight = 
						(
							p->isPlayer1 ? 
							coopActor->GetWeightInContainer() :
							p->em->inventoryChest->GetWeightInContainer()
						);
						const auto invChanges = 
						(
							p->isPlayer1 ? 
							coopActor->GetInventoryChanges() :
							p->em->inventoryChest->GetInventoryChanges()
						);
						if (invChanges)
						{
							inventoryWeight = invChanges->totalWeight;
						}

						const float carryweight = coopActor->GetTotalCarryWeight();
						float remainingCarryweight = carryweight - inventoryWeight;
						std::string weightValue = fmt::format
						(
							", <font color=\"#{:X}\">Value: </font>"
							"<font face=\"$EverywhereBoldFont\">{}</font>, "
							"<font color=\"#{:X}\">Weight: </font>"
							"<font face=\"$EverywhereBoldFont\">{:.0f}</font>, "
							"<font color=\"#{:X}\">Space: </font>"
							"<font face=\"$EverywhereBoldFont\">"
							"<font color=\"#{:X}\">{:.0f}</font>"
							"</font>",
							0xBBA53D,
							value,
							0x999999,
							weight,
							0x804a00,
							remainingCarryweight - weight <= 0.0f ? 
							0xFF0000 : 
							0xFFFFFF,
							remainingCarryweight,
							carryweight
						);
						text = fmt::format
						(
							"{}", std::string(text) + weightValue
						);
					}

					type = a_type;
					// Refresh set TP to keep the crosshair text from fading 
					// when moving the crosshair.
					updateSetTP = p->pam->IsPerforming(InputAction::kMoveCrosshair);
				}
			}
		}
		else if (a_type == CrosshairMessageType::kStealthState)
		{
			if (coopActor->IsSneaking())
			{
				const bool checkSelectedTarget = 
				(
					selectedTargetActorPtr &&
					selectedTargetActorPtr.get() && 
					!selectedTargetActorPtr->IsDead()
				);
				// Set sneak info text to indicate the player's hidden percent,
				// which is determined by their remaining stealth points:
				// 
				// player's total stealth points - 
				// max(all aggro'd actors' stealth point decrements)
				
				// If a particular target actor is selected, 
				// show their individual detection level of the player as well.
				if (checkSelectedTarget)
				{
					float targetDetectionPct = static_cast<uint8_t>
					(
						Util::GetDetectionPercent(coopActor.get(), selectedTargetActorPtr.get())
					);
					uint32_t targetDetectionPctRGB = GetDetectionLvlRGB(targetDetectionPct, false);
					// Set sneak info text to indicate the currently selected/aim correction
					// target's  detection level of the player 
					// and the overall detection percentage of the player
					// for all relevant high process actors.
					// Passive actors' names are displayed in white,
					// pacifiable actors' names are displayed in pink,
					// and enemy actors' names are displayed in red.
					text = fmt::format
					(
						"P{}: Detected by <font color=\"#{:X}\">{}</font> "
						"(<font color=\"#{:X}\">{}%</font>), overall "
						"(<font color=\"#{:X}\">{}%</font>)",
						playerID + 1,
						!selectedTargetActorPtr->IsHostileToActor(coopActor.get()) ? 
						0xFFFFFF : 
						Util::CanStopCombatWithActor(selectedTargetActorPtr.get()) ?
						0xFFBBBB :
						0xFF0000,
						selectedTargetActorPtr->GetDisplayFullName(),
						targetDetectionPctRGB, 
						targetDetectionPct,
						detectionPctRGB,
						detectionPct
					);
				}
				else
				{
					// Detection percent reported accounts for all relevant actors 
					// in the high process.
					text = fmt::format
					(
						"P{}: Detected (<font color=\"#{:X}\">{}%</font>)",
						playerID + 1, detectionPctRGB, detectionPct
					);
				}

				type = a_type;
				// Always show when sneaking.
				updateSetTP = true;
			}
		}
		
		SetCurrentCrosshairMessage
		(
			false,
			std::move(type),
			text, 
			{ }, 
			3.0f,
			updateSetTP
		);
	}

	void TargetingManager::UpdateActivationTarget
	(
		bool a_setToAimTargetHandle, bool a_quickSelection, bool a_playActivationShader
	)
	{
		// Set the activation target refr handle directly to the crosshair/aim correction handle,
		// or check for a selectable refr nearby.
		// Play/stop any activation shaders if set/cleared, and update the quick activation flag
		// and activation target changed TP.

		auto prevHandle = activationRefrHandle;
		auto newHandle = RE::ObjectRefHandle();
		if (a_setToAimTargetHandle)
		{
			newHandle = 
			(
				aimMode == AimMode::kCrosshair ? crosshairRefrHandle : aimCorrectionTargetHandle
			);
		}
		else
		{
			newHandle = GetSelectableProximityRefrHandle(a_quickSelection);
		}

		// Update set via quick activation flag.
		bool newHandleIsValid = Util::HandleIsValid(newHandle);
		choseQuickActivationTarget = a_quickSelection && newHandleIsValid;
		choseProximityActivationTarget = !a_setToAimTargetHandle && newHandleIsValid;
		if (newHandleIsValid)
		{
			// Update activation target handle.
			// Remove all previous activation shaders.
			if (Util::HandleIsValid(prevHandle))
			{
				Util::StopAllActivationEffectShaders(prevHandle.get().get(), playerID);
			}
				
			activationRefrHandle = newHandle;
			// Already checked LOS before selection, so no need to do so again.
			ValidateActivationRefr(false);

			DBG
			(
				"{}: Has chosen REFR {} as their activation target. "
				"Time since change: {}. Can activate: {}. Was: {}", 
				coopActor->GetName(), 
				newHandle.get()->GetName(),
				Util::GetElapsedSeconds(p->lastActivationTargetChangeTP),
				canActivateRefr,
				Util::HandleIsValid(prevHandle) ? 
				prevHandle.get()->GetName() : 
				"NONE"
			);
				
			if (newHandle != prevHandle || newHandle == crosshairRefrHandle)
			{
				// Will not play shader for now if the player cannot activate.
				if (a_playActivationShader)
				{
					auto shader = 
					(
						canActivateRefr ? 
						glob.activateHighlightShaders[playerID] : 
						glob.activateFailureShader
					);
					ColorizeActivationShader
					(
						shader, 
						canActivateRefr || GlobalCoopData::IsCoopPlayer(activationRefrHandle)
					);
					Util::StartEffectShader(newHandle.get().get(), shader, 1.0f);
				}
			}
		}
		else
		{
			DBG
			(
				"{}: No chosen REFR activation target. Time since change: {}s. "
				"Previous handle: {}", 
				coopActor->GetName(), 
				Util::GetElapsedSeconds(p->lastActivationTargetChangeTP),
				activationRefrHandle.get() ?
				activationRefrHandle.get()->GetName() :
				"NONE"
			);
			// Stop shader before clearing the current target if it is still valid.
			ClearActivationTargetData();
		}

		if (newHandle != prevHandle)
		{
			p->lastActivationTargetChangeTP = SteadyClock::now();
		}
	}

	void TargetingManager::UpdateAimCorrectionTarget()
	{
		// Update aim correction target if the player is attempting
		// to perform or is performing a ranged attack.
		// Clear the target otherwise.
		// Can skip the analog stick commitment check, 
		// which checks if the stick was just displaced to max 
		// or if the stick is at max displacement and not moving towards its resting position.

		// If the player has aim correction disabled and is not using the right stick 
		// to select a target (the crosshair is enabled), so we can clear out the existing target.
		if (!Settings::vbUseAimCorrection[playerID] && aimMode == AimMode::kCrosshair)
		{
			if (Util::HandleIsValid(aimCorrectionTargetHandle))
			{
				ClearTarget(TargetActorType::kAimCorrection);
			}

			return;
		}

		// First, if the current target is no longer selectable, clear it.
		auto currentTargetPtr = Util::GetActorPtrFromHandle(aimCorrectionTargetHandle);
		auto playerTorsoPos = p->mm->playerTorsoPosition;
		auto targetTorsoPos = 
		(
			currentTargetPtr ? 
			Util::GetTorsoPosition(currentTargetPtr.get()) : 
			playerTorsoPos
		);
		bool isNoLongerTargetable = 
		(
			currentTargetPtr && !Util::IsValidRefrForTargeting(currentTargetPtr.get())
		);
		if (isNoLongerTargetable)
		{
			ClearTarget(TargetActorType::kAimCorrection);
			return;
		}
		
		// Clear non-hostile aim correction target 
		// when the target is beyond the range of consideration.
		if (currentTargetPtr)
		{
			bool isActivelyHostile = Util::IsActivelyHostileToPlayerOrAlly
			(
				currentTargetPtr.get()
			);
			isNoLongerTargetable = 
			(
				!isActivelyHostile && 
				!GlobalCoopData::IsCoopPlayer(currentTargetPtr) &&
				playerTorsoPos.GetDistance(targetTorsoPos) > 
				Settings::fMaxNonHostileAimCorrectionTargetDistance
			);
			if (isNoLongerTargetable)
			{
				ClearTarget(TargetActorType::kAimCorrection);
				return;
			}
		}
		
		// Player is trying to/is performing/just finished an action 
		// that requires having a target.
		bool combatActionBindPressed = false;
		bool combatActionJustStarted = false;
		bool performingRangedAction = false;
		bool attackOrBlockRequest = p->pam->TurnToTargetForCombatAction
		(
			combatActionBindPressed, combatActionJustStarted, performingRangedAction
		);
		// Can select a target when in twin stick mode and aiming at an NPC with the RS.
		// NOTE:
		// Before throwing/dropping, the RMM's isGrabbing flag is unset to signal this manager
		// to release the grabbed objects in HandleReferenceManipulation(),
		// -but- since that function runs after this one since it needs updated target info,
		// we cannot use the flag here, since the aim correction target would get cleared,
		// leading to all objects being dropped instead of thrown.
		// Instead, check if there are still grabbed objects primed for release,
		// since this list does not get cleared until HandleReferenceManipulation() runs
		// and we thus retain the chosen aim correction target to serve 
		// as the target for all the release objects.
		const bool twinStickPickTarget = 
		(
			aimMode == AimMode::kTwinStick &&
			p->pam->AllInputsPressedForAction(InputAction::kResetAim) &&
			p->pam->AllInputsPressedForAction(InputAction::kRotateCam)
		);
		if (twinStickPickTarget)
		{
			lockOnToAimCorrectionTarget = true;
		}
		
		bool twinStickThrowSelection = 
		(
			aimMode == AimMode::kTwinStick && !rmm->grabbedRefrInfoList.empty()
		);
		const auto& stickState = glob.cdh->GetAnalogStickState(deviceID, !twinStickPickTarget);
		const auto selectedTargetActorPtr = Util::GetActorPtrFromHandle(selectedTargetActorHandle);
		// Can select an aim correction target with the LS when attacking.
		bool lsSelectTempTarget = 
		(
			(aimMode == AimMode::kCrosshair) &&
			(!crosshairActive) &&
			(Settings::vbUseAimCorrection[playerID]) && 
			(attackOrBlockRequest && !selectedTargetActorPtr)
		);
		bool canValidateTarget =
		(
			twinStickPickTarget || lsSelectTempTarget
		);

		// Clear when not selecting a new target, choosing a ranged target actor in Crosshair mode,
		// or when not attacking/throwing in combat, unless facing a target in Twin-stick mode.
		// Yeah. Simple.
		bool canClear = 
		(
			(!canValidateTarget) &&
			(
				(aimMode == AimMode::kCrosshair && selectedTargetActorPtr && currentTargetPtr) ||
				(!lockOnToAimCorrectionTarget && !attackOrBlockRequest && !twinStickThrowSelection)
			)
		);
		if (canValidateTarget)
		{
			// Require left stick 'commitment', meaning the left stick is displaced to max
			// and moving away from center or staying the same distance from center.
			// Ignore partial displacement and recentering to prevent finicky target switching.
			bool stickMovingAwayFromCenter = stickState.MovingAwayFromCenter();
			bool stickMovingTowardsCenter = stickState.MovingTowardsCenter();
			bool stickCommitment = 
			(
				(stickState.prevNormMag < 1.0f - 1E-2f && stickState.normMag >= 1.0f - 1E-2f) ||
				(
					(stickState.normMag - stickState.prevNormMag > -1E-3f) &&
					(stickState.stickLinearSpeed > 0.5f)
				)
			);
			const float selectionInterval = 
			(
				Settings::fSecsBetweenSelectingAimCorrectionTargets * 
				(0.5f / (stickState.stickLinearSpeed))
			);
			bool canSelectNewTarget = 
			(
				(twinStickPickTarget) || 
				(
					(
						Util::GetElapsedSeconds(p->lastAimCorrectionTargetSetTP) > selectionInterval
					) &&
					(
						((stickCommitment) || (combatActionJustStarted && !currentTargetPtr))
					)
				)
			);
			// Should check if the current target is in the FOV window
			// when not attempting to select a new target 
			// or after checking for a new target but retaining the current one.
			// bool retainingCurrentTarget = true;
			if (canSelectNewTarget)
			{
				auto refrPtr = Util::GetRefrPtrFromHandle
				(
					GetClosestTargetableActorInFOV
					(
						coopActor.get(),
						lsSelectTempTarget,
						false,
						lsSelectTempTarget,
						true,
						Settings::vbScreenspaceBasedAimCorrectionCheck[playerID],
						Settings::vfAimCorrectionFOV[playerID],
						!combatActionBindPressed || 
						performingRangedAction ||
						twinStickThrowSelection ||
						twinStickPickTarget ? 
						Settings::fMaxRaycastAndZoomOutDistance :
						GetMaxActivationDist()
					)
				);

				auto nextTarget = refrPtr ? refrPtr.get()->As<RE::Actor>() : nullptr;
				DBG
				(
					"{}: Twin-stick RS selection: {}, combat action just started: {}, "
					"attack or block request: {}, is ranged: {}, "
					"move crosshair time performed: {}, lock on bind pressed: {}, "
					"stick commitment: {}, elapsed time since set: {}. Selection interval: {}. "
					"Can select: {}. RS norm mags: {}, {} (diff: {}). Speeds: {}, {}. "
					"Current target: {} (0x{:X}), next target: {} (0x{:X}). Lock on to target: {}",
					coopActor->GetName(), 
					twinStickPickTarget,
					combatActionJustStarted,
					attackOrBlockRequest,
					performingRangedAction,
					p->pam->GetPlayerActionInputHoldTime(InputAction::kMoveCrosshair),
					p->pam->AllInputsPressedForAction(InputAction::kResetAim) &&
					p->pam->AllInputsPressedForAction(InputAction::kRotateCam),
					stickCommitment, 
					Util::GetElapsedSeconds(p->lastAimCorrectionTargetSetTP),
					selectionInterval,
					canSelectNewTarget,
					stickState.prevNormMag,
					stickState.normMag,
					stickState.normMag - stickState.prevNormMag,
					stickState.stickLinearSpeed,
					stickState.stickAngularSpeed * TO_DEGREES,
					currentTargetPtr ? currentTargetPtr->GetName() : "NONE",
					currentTargetPtr ? currentTargetPtr->formID : 0xDEAD,
					nextTarget ? nextTarget->GetName() : "NONE",
					nextTarget ? nextTarget->formID : 0xDEAD,
					lockOnToAimCorrectionTarget
				);

				if (nextTarget && Util::IsValidRefrForTargeting(nextTarget))
				{
					aimCorrectionTargetHandle = nextTarget->GetHandle();
					// Set activation target to locked-on aim correction target since the target
					// will linger and allow for activation.
					if (currentTargetPtr != refrPtr)
					{
						if (RefrIsInActivationRange(aimCorrectionTargetHandle))
						{
							UpdateActivationTarget(true, false, lockOnToAimCorrectionTarget);
							DBG
							(
								"{}: Aim correction target {} is selected "
								"as the activation target.",
								coopActor->GetName(), aimCorrectionTargetHandle.get()->GetName()
							);
						}
						else
						{
							// Remove previous activation target if it was selected 
							// as the aim correction target.
							if (Util::HandleIsValid(activationRefrHandle) && 
								currentTargetPtr && 
								currentTargetPtr == activationRefrHandle.get())
							{
								ClearActivationTargetData();
							}

							// Stop the activation shader on the previous aim correction target.
							if (currentTargetPtr)
							{
								Util::StopAllActivationEffectShaders
								(
									currentTargetPtr.get(), playerID
								);
							}

							if (lockOnToAimCorrectionTarget)
							{
								// Play a highlight shader when locking on to a target 
								// outside of activation range.
								ColorizeActivationShader
								(
									glob.activateHighlightShaders[playerID], true
								);
								Util::StartEffectShader
								(
									aimCorrectionTargetHandle.get().get(),
									glob.activateHighlightShaders[playerID],
									1.0f
								);
							}
						}
					}

					p->lastAimCorrectionTargetSetTP = SteadyClock::now();
				}
				else if (currentTargetPtr && !Util::IsValidRefrForTargeting(currentTargetPtr.get()))
				{
					ClearTarget(TargetActorType::kAimCorrection);
				}
			}
		}
		else if (canClear)
		{
			/*DBG
			(
				"{}: Right stick selection: {}, combat action just started: {}, "
				"attack or block request: {}, is ranged: {}, select throw target: {}, "
				"lock on to target: {}, Selected actor target: {} (0x{:X}), "
				"current target: {} (0x{:X}).",
				coopActor->GetName(), 
				rightStickSelection,
				combatActionJustStarted,
				attackOrBlockRequest,
				performingRangedAction,
				canSelectThrowTarget,
				lockOnToAimCorrectionTarget,
				selectedTargetActorPtr ? 
				selectedTargetActorPtr->GetName() :
				"NONE",
				selectedTargetActorPtr ? 
				selectedTargetActorPtr->formID :
				0xDEAD,
				currentTargetPtr ? 
				currentTargetPtr->GetName() :
				"NONE",
				currentTargetPtr ? 
				currentTargetPtr->formID :
				0xDEAD
			);*/

			// Clear the aim correction target when not attacking or trying to attack, blocking,
			// or when a crosshair target actor is selected.
			const auto& combatGroup = glob.paInfoHolder->DEF_ACTION_GROUPS_TO_INDICES.at
			(
				ActionGroup::kCombat
			);
			bool combatActionBindsPressed = false;
			if (coopActor->IsWeaponDrawn()) 
			{
				for (auto actionIndex : combatGroup)
				{
					combatActionBindsPressed |= p->pam->AllInputsPressedForAction
					(
						static_cast<InputAction>(actionIndex)
					);
					if (combatActionBindsPressed)
					{
						break;
					}
				}
			}

			const bool canClear = 
			(
				(currentTargetPtr) && 
				(
					(selectedTargetActorPtr) || 
					(
						!combatActionBindsPressed &&
						!p->pam->isAttacking && 
						!p->pam->isBashing && 
						!p->pam->isBlocking &&
						!p->pam->isInCastingAnim
					)
				)
			);
			if (canClear)
			{
				// Stop the activation shaders on the current aim correction target
				// before clearing it.
				Util::StopAllActivationEffectShaders(currentTargetPtr.get(), playerID);
				ClearTarget(TargetActorType::kAimCorrection);
			}
		}
	}

	bool TargetingManager::UpdateAimTargetLinkedRefr
	(
		const EquipIndex& a_attackSlot, bool a_findTarget
	)
	{
		// Update the target refr used by the ranged attack package.
		// The given equip index should hold the form triggering the ranged attack.
		// E.g. Left hand slot when trying to cast a spell in the left hand.
		// Can set to self to ensure the ranged attack package has a valid target.
		// If requesting to find a target, 
		// a new target refr will be computed and set based on the given attack slot.
		// Otherwise, the aim target linked refr will be set to the player's character 
		// to ensure the ranged target package has a valid target.
		// Then return true if a new target was set or the old one was cleared.

		// Requires the aim target keyword to set the aim target linked refr.
		if (!p->aimTargetKeyword)
		{
			return false;
		}

		// Check the given attack slot for a ranged weapon/spell.
		auto weapMagObj = p->em->equippedForms[!a_attackSlot];
		bool isRanged = false;
		if (weapMagObj)
		{
			if (weapMagObj->IsWeapon())
			{
				auto weap = weapMagObj->As<RE::TESObjectWEAP>();
				isRanged = 
				(
					weap->IsBow() || weap->IsCrossbow() || weap->IsStaff()
				);
			}
			else 
			{
				isRanged = weapMagObj->IsMagicItem() || weapMagObj->As<RE::TESShout>();
			}
		}

		if (isRanged)
		{
			auto currentTargetRefrPtr = Util::GetRefrPtrFromHandle(aimTargetLinkedRefrHandle);
			auto newTargetRefrPtr = 
			(
				a_findTarget ? 
				Util::GetRefrPtrFromHandle
				(
					GetRangedPackageTargetRefr(weapMagObj)
				) :
				coopActor
			);
			bool newTargetIsValid = 
			(
				newTargetRefrPtr &&
				Util::IsValidRefrForTargeting(newTargetRefrPtr.get())
			);

			// REMOVE when done debugging.
			/*DBG
			(
				"{}: index {}, weapMagObj: {}. Current: {}, new: {}. Find target: {}.",
				coopActor->GetName(),
				a_attackSlot,
				weapMagObj ? weapMagObj->GetName() : "NONE",
				currentTargetRefrPtr ? currentTargetRefrPtr->GetName() : "NONE",
				newTargetRefrPtr ? newTargetRefrPtr->GetName() : "NONE",
				a_findTarget
			);*/

			//if ((newTargetIsValid && newTargetRefrPtr != currentTargetRefrPtr) &&
			//	(!currentTargetRefrPtr || newTargetRefrPtr != coopActor))
			//{
			//	// Set new valid linked refr.
			//	coopActor->extraList.SetLinkedRef(newTargetRefrPtr.get(), p->aimTargetKeyword);
			//	aimTargetLinkedRefrHandle = newTargetRefrPtr->GetHandle();
			//	return true;
			//}
			//else if (!newTargetIsValid && currentTargetRefrPtr)
			//{
			//	// Clear old linked refr if no new one was selected.
			//	coopActor->extraList.SetLinkedRef(nullptr, p->aimTargetKeyword);
			//	aimTargetLinkedRefrHandle.reset();
			//	return true;
			//}

			if (newTargetIsValid)
			{
				// Set new valid linked refr.
				coopActor->extraList.SetLinkedRef(newTargetRefrPtr.get(), p->aimTargetKeyword);
				aimTargetLinkedRefrHandle = newTargetRefrPtr->GetHandle();
			}
			else if (!newTargetIsValid && currentTargetRefrPtr)
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
		
		float endPointAng = Util::HandleIsValid(crosshairRefrHandle) ? PI / 4.0f : 0.0f;
		// Interpolation endpoint changed, signal state change.
		if (crosshairRotationData->next != endPointAng)
		{
			crosshairRotationData->SetTimeSinceUpdate(0.0f);
			crosshairRotationData->ShiftEndpoints(endPointAng);
		}

		crosshairRotationData->next = endPointAng;
		if (crosshairRotationData->current != endPointAng)
		{
			crosshairRotationData->InterpolateSmootherStep
			(
				min
				(
					crosshairRotationData->secsSinceUpdate / 
					crosshairRotationData->secsUpdateInterval, 
					1.0f
				)
			);
			crosshairRotationData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);
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
		const float currentCrosshairGap = 
		(
			Settings::vfCrosshairGapRadius[playerID] + crosshairOscillationData->current
		);
		// Crosshair gap at max expansion.
		const float maxCrosshairGap = max
		(
			crosshairLength,
			Settings::vfCrosshairGapRadius[playerID] * 2.0f
		);
		// Includes inner outline, prong itself, and current interpolated gap.
		float currentProngDistFromCenter = 
		(
			2.0f * crosshairThickness + currentCrosshairGap + crosshairLength
		);
		// Check if the expanding/contracting crosshair is near the edge of the screen.
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
			endPointGapDelta = 
			(
				Settings::vuCrosshairStyle[playerID] == !CrosshairStyle::kRing ? 
				maxCrosshairGap : 
				0.0f
			);
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
		// Update the current interpolated gap value 
		// if it hasn't reached the interpolation endpoint yet.
		if (crosshairOscillationData->current != endPointGapDelta)
		{
			crosshairOscillationData->InterpolateSmootherStep
			(
				min
				(
					crosshairOscillationData->secsSinceUpdate / 
					crosshairOscillationData->secsUpdateInterval, 
					1.0f
				)
			);
			crosshairOscillationData->IncrementTimeSinceUpdate(*g_deltaTimeRealTime);

			// Reached the interpolation endpoint now, so signal completion,
			// and set previous, current, and next interpolation values to the endpoint value.
			if (crosshairOscillationData->current == endPointGapDelta)
			{
				crosshairOscillationData->SetUpdateDurationAsComplete();
				crosshairOscillationData->SetData
				(
					endPointGapDelta, endPointGapDelta, endPointGapDelta
				);
			}
		}
	}
		
	void TargetingManager::UpdateCrosshairMessage()
	{
		// Update the player's crosshair text entry to set the next time
		// the crosshair text message is updated.
		
		// NOTE:
		// Only TargetSelection and StealthState message types are set periodically.
		// Other messages types are triggered externally.

		// Can set without delaying.
		bool noDelay = false;
		// This message's type is one of the delayed types listed by the currently set message.
		bool isDelayedType = false;
		// The current message's delay restriction interval has passed.
		bool delayPassed = false;
		// External message (from outside the code in this file) was set.
		bool extMessageSet = false;
		// Set to the last message so that no new message is set if there is no update below.
		crosshairMessage->CopyMessageData(lastCrosshairMessage);
		// If there is an externally-requested crosshair message, prioritize it.
		if (extCrosshairMessage->type != CrosshairMessageType::kNone) 
		{
			noDelay = lastCrosshairMessage->delayedMessageTypes.empty();
			isDelayedType = 
			(
				!noDelay && 
				lastCrosshairMessage->delayedMessageTypes.contains(extCrosshairMessage->type)
			);
			delayPassed = 
			(
				Util::GetElapsedSeconds(lastCrosshairMessage->setTP) > 
				lastCrosshairMessage->secsMaxDisplayTime
			);
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
			// Display selection text if not sneaking 
			// or if selecting a non-actor or corpse refr.
			// Display stealth state text otherwise.
			auto selectedRefrPtr = Util::GetRefrPtrFromHandle(activationRefrHandle);
			auto selectedTargetActorPtr = Util::GetActorPtrFromHandle
			(
				aimMode == AimMode::kTwinStick ? 
				aimCorrectionTargetHandle : 
				selectedTargetActorHandle
			);
			bool displayTargetSelectionMessage = false;
			if (aimMode == AimMode::kTwinStick)
			{
				displayTargetSelectionMessage =
				(
					(!coopActor->IsSneaking()) || 
					(
						(selectedRefrPtr && !selectedTargetActorPtr) &&
						(
							!selectedRefrPtr->As<RE::Actor>() || 
							selectedRefrPtr->As<RE::Actor>()->IsDead()
						)
					)	
				);
			}
			else
			{
				displayTargetSelectionMessage =
				(
					(!coopActor->IsSneaking()) || 
					(
						(selectedRefrPtr) && 
						(!selectedTargetActorPtr || selectedTargetActorPtr->IsDead())
					)	
				);
			}
			
			if (displayTargetSelectionMessage)
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

		// Only set the last message if current and last are different.
		if (lastCrosshairMessage->text != crosshairMessage->text) 
		{
			crosshairMessage->setTP = SteadyClock::now();
			lastCrosshairMessage->CopyMessageData(crosshairMessage);
		}
		else
		{
			// Otherwise, just update the last message's start TP,
			// just in case the current crosshair message was set
			// by a more recent request, despite its text and type remaining unchanged.
			lastCrosshairMessage->setTP = crosshairMessage->setTP;
		}

		// Clear external message each frame here,
		// since we've just handled it.
		if (extCrosshairMessage->type != CrosshairMessageType::kNone) 
		{
			extCrosshairMessage->Clear();
		}
	}

	void TargetingManager::UpdateCrosshairPosAndSelection()
	{
		// Update the player's crosshair position and selected refr data.

		auto niCamPtr = Util::GetNiCamera();
		auto ui = RE::UI::GetSingleton();
		// Overlay menu to draw the crosshair on.
		auto overlay = ui ? ui->GetMenu(DebugOverlayMenu::MENU_NAME) : nullptr;
		auto view = overlay ? overlay->uiMovie : nullptr;
		if (!niCamPtr || !ui || !overlay || !view)
		{
			// Set last update time point before returning early.
			p->lastCrosshairUpdateTP = SteadyClock::now();
			return;
		}

		// Get dimensions from the view's visible frame.
		auto gRect = view->GetVisibleFrameRect();
		const float rectWidth = fabsf(gRect.right - gRect.left);
		const float rectHeight = fabsf(gRect.bottom - gRect.top);

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
		// TEMPORARY until 'Face Aim Target' is implemented.

		const bool isAiming = 
		(
			aimMode == AimMode::kCrosshair && 
			p->pam->IsPerforming(InputAction::kMoveCrosshair) &&
			!p->pam->AllInputsPressedForAtLeastOneAction
			(
				InputAction::kResetAim
			)
		);
		// Crosshair is inactive when in 'Twin Stick' mode.
		if (aimMode == AimMode::kCrosshair)
		{
			if (isAiming)
			{
				// Not snapping to a lock on target if moving the crosshair.
				choseLockOnAimTarget = false;
				crosshairManuallyAdjusted = true;
				// Get RS data.
				const auto& rsData = glob.cdh->GetAnalogStickState(deviceID, false);
				const auto& rsX = rsData.xComp;
				// Scaleform Y is inverted with respect to the analog stick's Y axis.
				const auto& rsY = -rsData.yComp;
				const auto& rsMag = rsData.normMag * rsData.normMag;
				const float secsSinceCrosshairUpdated = Util::GetElapsedSeconds
				(
					p->lastCrosshairUpdateTP
				);

				// Max pixels per second that the crosshair can travel across 
				// along the X and Y screen axes.
				float crosshairMaxXSpeedPPS = 
				(
					Settings::vfCrosshairHorizontalSensitivity[playerID] * 
					Settings::fCrosshairMaxTraversablePixelsPerSec
				);
				float crosshairMaxYSpeedPPS = 
				(
					Settings::vfCrosshairVerticalSensitivity[playerID] *
					Settings::fCrosshairMaxTraversablePixelsPerSec
				);
				// Slow down the moving crosshair when an actor or refr is selected.
				// Number of pixels to move across in the X and Y directions this update.
				RE::NiPoint2 pixelDeltas
				{
					rsX * 
					rsMag * 
					secsSinceCrosshairUpdated * 
					crosshairMaxXSpeedPPS,
					rsY * 
					rsMag *
					secsSinceCrosshairUpdated * 
					crosshairMaxYSpeedPPS
				};
				pixelDeltas *= crosshairSpeedMult;

				// When moving over a refr, add the pixel deltas 
				// relative to the initial 'entry' position 
				// which was set when the crosshair first selected the refr. 
				// This will allow the crosshair to 'stick' to moving targets
				// while moving it across the target,
				// since the change in pixels is made relative to the target's movement.
				if (crosshairRefrPtr)
				{
					// Add to cumulative pixels deltas.
					crosshairOnRefrPixelXYDeltas.x += pixelDeltas.x;
					crosshairOnRefrPixelXYDeltas.y += pixelDeltas.y;
					auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
					// Set the hit position's local offset from the hit refr's base position.
					// Base position is the torso position for actors 
					// and the center position for all other refrs.
					const auto refrBasePos = 
					(
						hitActor ? 
						Util::GetTorsoPosition(hitActor) : 
						Util::GetRefrPosition(crosshairRefrPtr.get())
					);
					// Get updated world position by adding the stored initial
					// movement hit pos offset to the refr's reported base position.
					auto newBaseCrosshairWorldPos = 
					(
						refrBasePos + crosshairInitialMovementHitPosOffset
					);
					// Get corresponding screen position.
					auto screenPos = Util::WorldToScreenPoint3(newBaseCrosshairWorldPos);
					// Add deltas to this base screen position 
					// to allow the crosshair to move relative to the selected refr.
					crosshairScaleformPos.x = std::clamp
					(
						screenPos.x + crosshairOnRefrPixelXYDeltas.x, 0.0f, rectWidth
					);
					crosshairScaleformPos.y = std::clamp
					(
						screenPos.y + crosshairOnRefrPixelXYDeltas.y, 0.0f, rectHeight
					);
					crosshairScaleformPos.z = 0.0f;
				}
				else
				{
					// Update scaleform position directly with the pixel deltas.
					crosshairScaleformPos.x = std::clamp
					(
						crosshairScaleformPos.x + pixelDeltas.x, 0.0f, rectWidth
					);
					crosshairScaleformPos.y = std::clamp
					(
						crosshairScaleformPos.y + pixelDeltas.y, 0.0f, rectHeight
					);
					crosshairScaleformPos.z = 0.0f;
				}

				// Clear selected actor and crosshair refr
				// before checking for raycast/proximity refr hits below.
				selectedTargetActorHandle = RE::ActorHandle();
				crosshairRefrHandle = RE::ObjectRefHandle();

				// Calculate near and far plane world positions for the current scaleform position.
				glm::mat4 pvMat{ };
				// Transpose first.
				pvMat[0][0] = niCamPtr->worldToCam[0][0];
				pvMat[1][0] = niCamPtr->worldToCam[0][1];
				pvMat[2][0] = niCamPtr->worldToCam[0][2];
				pvMat[3][0] = niCamPtr->worldToCam[0][3];
				pvMat[0][1] = niCamPtr->worldToCam[1][0];
				pvMat[1][1] = niCamPtr->worldToCam[1][1];
				pvMat[2][1] = niCamPtr->worldToCam[1][2];
				pvMat[3][1] = niCamPtr->worldToCam[1][3];
				pvMat[0][2] = niCamPtr->worldToCam[2][0];
				pvMat[1][2] = niCamPtr->worldToCam[2][1];
				pvMat[2][2] = niCamPtr->worldToCam[2][2];
				pvMat[3][2] = niCamPtr->worldToCam[2][3];
				pvMat[0][3] = niCamPtr->worldToCam[3][0];
				pvMat[1][3] = niCamPtr->worldToCam[3][1];
				pvMat[2][3] = niCamPtr->worldToCam[3][2];
				pvMat[3][3] = niCamPtr->worldToCam[3][3];
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
				// Derive world positions using the inverted projection view matrix
				// and the clip space near/far vectors.
				glm::vec4 worldPosNear = (invPVMat * clipSpacePosNear);
				glm::vec4 worldPosFar = (invPVMat * clipSpacePosFar);
				worldPosNear /= worldPosNear.w;
				worldPosFar /= worldPosFar.w;

				// Set initial crosshair world position to the far plane point.
				crosshairWorldPos = ToNiPoint3(worldPosFar);
				// Raycast for selectable refrs. Get all hits from near to far plane points.
				auto results = Raycast::GetAllHavokCastHitResults(worldPosNear, worldPosFar);
				// Pick a hit result with a potentially-selectable refr.
				Raycast::RayResult centerResult = PickRaycastHitResult
				(
					results, glob.isInCoopCombat, true
				);
				// Clear valid flag since we'll be updating it below if the chosen hit was valid.
				validCrosshairRefrHit = false;
				// Only need to check the result if it has a hit.
				if (centerResult.hit)
				{
					// Update crosshair world pos, regardless of whether or not 
					// the raycast hits anything selectable.
					crosshairWorldPos = ToNiPoint3(centerResult.hitPos);
					if (Util::HandleIsValid(centerResult.hitRefrHandle))
					{
						// Must be valid for selection.
						validCrosshairRefrHit = IsRefrValidForCrosshairSelection
						(
							centerResult.hitRefrHandle
						); 
						if (validCrosshairRefrHit)
						{
							// Set crosshair refr handle.
							crosshairRefrHandle = centerResult.hitRefrHandle;
							crosshairRefrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle);
							auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
							// Set selected actor handle if the hit refr is an actor.
							if (hitActor)
							{
								selectedTargetActorHandle = hitActor->GetHandle();
							}
					
							const auto refrBasePos = 
							(
								hitActor ? 
								Util::GetTorsoPosition(hitActor) : 
								Util::GetRefrPosition(crosshairRefrPtr.get())
							);
							// The local position offset to apply is the same as 
							// the movement offset when the crosshair is moved.
							crosshairLocalPosOffset = 
							crosshairLastMovementHitPosOffset = crosshairWorldPos - refrBasePos;
							// Pitch and yaw angle diffs from the base pos to the crosshair pos,
							// based on the selected refr's pitch/facing angles.
							crosshairLocalPosPitchDiff = Util::NormalizeAngToPi
							(
								Util::GetPitchBetweenPositions(refrBasePos, crosshairWorldPos) - 
								crosshairRefrPtr->data.angle.x
							);
							crosshairLocalPosYawDiff = Util::NormalizeAng0To2Pi
							(
								Util::GetYawBetweenPositions(refrBasePos, crosshairWorldPos) - 
								crosshairRefrPtr->data.angle.z
							);
							// If no refr was selected or a new one is selected, 
							// set the initial movement pos offset to the local offset.
							if (!prevCrosshairRefrPtr || crosshairRefrPtr != prevCrosshairRefrPtr)
							{
								crosshairInitialMovementHitPosOffset = crosshairLocalPosOffset;
								// Has just selected the refr, 
								// so no crosshair movement across it yet.
								crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
							}
						}
					}
				}

				// Update the crosshair speedmult to use the next frame when moving the crosshair.
				UpdateCrosshairSpeedmult(centerResult);
			}
			else if (crosshairRefrPtr)
			{
				// Refr selected when not moving the crosshair.
				// While not moving the crosshair, 
				// stick the crosshair to the target until it becomes invalid.
				
				crosshairManuallyAdjusted = false;
				// Check if targeted refr is still selectable and valid.
				validCrosshairRefrHit = 
				(
					IsRefrValidForCrosshairSelection(crosshairRefrHandle) && 
					Util::IsSelectableRefr(crosshairRefrPtr.get())
				);
				if (validCrosshairRefrHit)
				{
					// Move to the center of the selected lock on target over half a second.
					if (choseLockOnAimTarget)
					{
						// Update the crosshair world position using
						// the initial local hit position and the refr's new position.
						auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
						const auto refrBasePos = 
						(
							hitActor ? 
							Util::GetTorsoPosition(hitActor) : 
							Util::GetRefrPosition(crosshairRefrPtr.get())
						);

						crosshairWorldPos = refrBasePos;
						crosshairLocalPosOffset = 
						crosshairLastMovementHitPosOffset = 
						crosshairInitialMovementHitPosOffset = RE::NiPoint3();
						crosshairLocalPosPitchDiff =
						crosshairLocalPosYawDiff = 0.0f;
						crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
						auto screenPos = Util::WorldToScreenPoint3(crosshairWorldPos);
						const float secsSinceTargetChange = Util::GetElapsedSeconds
						(
							p->lastLockOnAimTargetChangeTP
						);
						if (secsSinceTargetChange <= Settings::fSecsToSnapCrosshairToLockOnTarget)
						{
							crosshairScaleformPos.x = Util::InterpolateSmootherStep
							(
								crosshairScaleformPos.x,
								screenPos.x,
								secsSinceTargetChange / Settings::fSecsToSnapCrosshairToLockOnTarget
							);
							crosshairScaleformPos.y = Util::InterpolateSmootherStep
							(
								crosshairScaleformPos.y,
								screenPos.y,
								secsSinceTargetChange / Settings::fSecsToSnapCrosshairToLockOnTarget
							);
						}
						else
						{
							crosshairScaleformPos.x = screenPos.x;
							crosshairScaleformPos.y = screenPos.y;
						}
					}
					else
					{
						// Update the crosshair world position using
						// the initial local hit position and the refr's new position.
						auto hitActor = crosshairRefrPtr->As<RE::Actor>(); 
						const auto refrBasePos = 
						(
							hitActor ? 
							Util::GetTorsoPosition(hitActor) : 
							Util::GetRefrPosition(crosshairRefrPtr.get())
						);

						// Update local positional offset so that the crosshair stays attached
						// to the crosshair refr at the same position 
						// (originally set while moving the crosshair)
						// relative to the crosshair refr's facing angle.
						// Maintain the same last-set distance from the refr base position.
						crosshairLocalPosOffset =
						(
							Util::RotationToDirectionVect
							(
								-Util::NormalizeAngToPi
								(
									crosshairRefrPtr->data.angle.x + crosshairLocalPosPitchDiff
								),
								Util::ConvertAngle
								(
									Util::NormalizeAng0To2Pi
									(
										crosshairRefrPtr->data.angle.z + crosshairLocalPosYawDiff
									)
								)
							) * 
							crosshairLastMovementHitPosOffset.Length()
						);
						// Set to local pos offset, so that if the crosshair begins moving
						// over this refr again, it will be offset relative to 
						// the last set local position.
						crosshairInitialMovementHitPosOffset = crosshairLocalPosOffset;
						// Zero out the pixel deltas until moving the crosshair again.
						crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
						// Offset the base position by the new offset 
						// to get the next crosshair world position.
						crosshairWorldPos = refrBasePos + crosshairLocalPosOffset;
						// Update the crosshair's scaleform position
						// based on its new world position.
						auto screenPos = Util::WorldToScreenPoint3(crosshairWorldPos);
						crosshairScaleformPos.x = screenPos.x;
						crosshairScaleformPos.y = screenPos.y;
					}
				}
				else
				{
					// No longer valid, time to reset data.
					// Clear out selected actor, refr, and initial hit local position.
					// Then set pixel deltas to 0.
					selectedTargetActorHandle = RE::ActorHandle();
					crosshairRefrHandle = RE::ObjectRefHandle();
					crosshairLocalPosOffset = 
					crosshairLastMovementHitPosOffset = 
					crosshairInitialMovementHitPosOffset = RE::NiPoint3();
					crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
				}
			}
			else
			{
				// No chosen refr, so no valid refr hit and we only have to potentially update 
				// the crosshair world position.
				// Only update the target position if the player's crosshair 
				// isn't fully faded or re-centered.
				validCrosshairRefrHit = false;
				bool isActive = 
				(
					(
						!Settings::vbRecenterInactiveCrosshair[playerID] &&
						!Settings::vbFadeInactiveCrosshair[playerID]
					) ||
					(
						Util::GetElapsedSeconds(p->crosshairLastActiveTP) < 
						Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
					)
				);
				if (isActive)
				{
					// Calculate near and far plane world positions 
					// for the current scaleform position.
					glm::mat4 pvMat{ };
					// Transpose first.
					pvMat[0][0] = niCamPtr->worldToCam[0][0];
					pvMat[1][0] = niCamPtr->worldToCam[0][1];
					pvMat[2][0] = niCamPtr->worldToCam[0][2];
					pvMat[3][0] = niCamPtr->worldToCam[0][3];
					pvMat[0][1] = niCamPtr->worldToCam[1][0];
					pvMat[1][1] = niCamPtr->worldToCam[1][1];
					pvMat[2][1] = niCamPtr->worldToCam[1][2];
					pvMat[3][1] = niCamPtr->worldToCam[1][3];
					pvMat[0][2] = niCamPtr->worldToCam[2][0];
					pvMat[1][2] = niCamPtr->worldToCam[2][1];
					pvMat[2][2] = niCamPtr->worldToCam[2][2];
					pvMat[3][2] = niCamPtr->worldToCam[2][3];
					pvMat[0][3] = niCamPtr->worldToCam[3][0];
					pvMat[1][3] = niCamPtr->worldToCam[3][1];
					pvMat[2][3] = niCamPtr->worldToCam[3][2];
					pvMat[3][3] = niCamPtr->worldToCam[3][3];
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
					// Derive world positions using the inverted projection view matrix 
					// and the clip space vectors.
					glm::vec4 worldPosNear = (invPVMat * clipSpacePosNear);
					glm::vec4 worldPosFar = (invPVMat * clipSpacePosFar);
					worldPosNear /= worldPosNear.w;
					worldPosFar /= worldPosFar.w;

					// Set initial crosshair world position to the far plane point.
					crosshairWorldPos = ToNiPoint3(worldPosFar);
					// Raycast for selectable refrs. Get all hits from near to far plane points.
					auto results = Raycast::GetAllHavokCastHitResults(worldPosNear, worldPosFar);
					// Get a valid result that does not have to contain a selectable refr.
					Raycast::RayResult centerResult = PickRaycastHitResult
					(
						results, glob.isInCoopCombat, false
					);
					// Set crosshair world position on hit.
					if (centerResult.hit)
					{
						crosshairWorldPos = ToNiPoint3(centerResult.hitPos);
					}
				}
			}
		}
		else
		{
			crosshairManuallyAdjusted = false;
		}

		//=======================================
		// [Crosshair Activity and Re-centering]:
		//=======================================

		const auto defaultCrosshairPos = GetDefaultCrosshairPosition();
		// Check if the crosshair is being actively 
		// or passively adjusted by the player in some way.
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
		// Is a player rotating the co-op camera or is P1 rotating the default TP camera?
		bool isCamRotating = 
		{
			(glob.cam->IsRunning()) ?
			(
				glob.cam->camAdjMode == CamAdjustmentMode::kRotate && 
				glob.cam->controlCamPID != -1 && 
				glob.coopPlayers[glob.cam->controlCamPID]->pam->IsPerforming
				(
					InputAction::kRotateCam
				)
			) :
			(
				(playerCam) && 
				(playerCam->rotationInput.x != 0.0f || playerCam->rotationInput.y != 0.0f)
			)
		};
		
		// Fulfilled reset-position request if the player is moving their crosshair,
		// has selected a target with it, or the crosshair has faded out/re-centered
		// depending on which option(s) the player has enabled,
		// or if both fade and re-centering options are disabled.
		bool noLongerResettingPosition = isAiming || Util::HandleIsValid(crosshairRefrHandle); 
		if (!noLongerResettingPosition)
		{
			const auto& canFade = Settings::vbFadeInactiveCrosshair[playerID];
			const auto& canRecenter = Settings::vbRecenterInactiveCrosshair[playerID];
			if (canFade && canRecenter)
			{
				noLongerResettingPosition = 
				(
					crosshairFadeInterpData->value == 0.0f &&
					crosshairScaleformPos == defaultCrosshairPos
				);
			}
			else if (canFade)
			{
				noLongerResettingPosition = crosshairFadeInterpData->value == 0.0f;
			}
			else if (canRecenter)
			{
				noLongerResettingPosition = crosshairScaleformPos == defaultCrosshairPos;
			}
			else
			{
				noLongerResettingPosition = true;
			}
		}

		if (shouldResetCrosshairPosition && noLongerResettingPosition)
		{
			shouldResetCrosshairPosition = false;
		}
		
		// Re-center the crosshair after an interval passes if the crosshair is disabled,
		// if requested externally, or if there is no valid target,
		// the player is not moving their crosshair, 
		// and the player is not facing the crosshair world position.
		float secsSinceActive = Util::GetElapsedSeconds(p->crosshairLastActiveTP);
		bool shouldRecenter = aimMode == AimMode::kTwinStick || shouldResetCrosshairPosition;
		if (shouldRecenter)
		{
			// Continue interpolating the position back 
			// towards the default position until reached.
			if (crosshairScaleformPos.x != defaultCrosshairPos.x || 
				crosshairScaleformPos.y != defaultCrosshairPos.y)
			{
				// Re-centering completes after about 1.5 inactivity intervals elapse.
				float tRatio = 0.0f;
				if (shouldResetCrosshairPosition)
				{
					// Start centering right away if a request was made.
					// Centering speed must never be slower than the fade out speed.
					tRatio = std::clamp
					(
						(
							secsSinceActive / 
							max
							(
								0.1f, 
								min
								(
									Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID],
									crosshairFadeInterpData->secsInterpToMinInterval
								)
							)
						),
						0.0f, 
						1.0f
					);
				}
				else
				{
					tRatio = std::clamp
					(
						(
							secsSinceActive / 
							max
							(
								0.1f, Settings::vfSecsBeforeRemovingInactiveCrosshair[playerID]
							)
						) - 1.0f,
						0.0f, 
						1.0f
					);
				}

				crosshairScaleformPos.x = Util::InterpolateSmootherStep
				(
					crosshairScaleformPos.x, defaultCrosshairPos.x, tRatio
				);
				crosshairScaleformPos.y = Util::InterpolateSmootherStep
				(
					crosshairScaleformPos.y, defaultCrosshairPos.y, tRatio
				);
				crosshairScaleformPos.z = 0.0f;

				// Reset offsets and pixel deltas too.
				crosshairLocalPosOffset = 
				crosshairLastMovementHitPosOffset = 
				crosshairInitialMovementHitPosOffset = RE::NiPoint3();
				crosshairOnRefrPixelXYDeltas = { 0.0f, 0.0f };
			}
		}

		// Dynamic crosshair resizing based on the selected object.
		if (Settings::vbAutoScaleCrosshairSize[playerID])
		{
			// Update crosshair size mult.
			auto refrPtr = Util::GetRefrPtrFromHandle(crosshairRefrHandle); 
			// Maximum gap length when the crosshair fully extends when animated.
			const float maxCrosshairGapDelta = 
			(
				Settings::vbAnimatedCrosshair[playerID] ? 
				max
				(
					Settings::vfCrosshairLength[playerID],
					Settings::vfCrosshairGapRadius[playerID] * 2.0f
				) : 
				0.0f
			);
			// Crosshair size includes all dimensions passed through when drawing a line 
			// along the length of a prong from the crosshair screen position
			// to the tip of the prong:
			// 1. One prong length
			// 2. Two thickness lengths
			// 3. The base gap radius
			// 4. The max additional gap radius
			const float maxCrosshairSize = 
			(
				2.0f * 
				(
					Settings::vfCrosshairLength[playerID] +
					2.0f * Settings::vfCrosshairThickness[playerID] +
					Settings::vfCrosshairGapRadius[playerID] +
					maxCrosshairGapDelta
				)
			);
			// Prevent the crosshair from getting too small; otherwise, it will not be visible.
			const float minCrosshairSize = 
			(
				0.005f * static_cast<float>(RE::BSGraphics::State::GetSingleton()->screenHeight)
			);
			if (refrPtr)
			{
				float minPixelDimension = 0.0f;
				if (minCrosshairSize >= maxCrosshairSize)
				{
					// Shrink down to the smaller of the min bound pixel distance
					// and the max crosshair size.
					minPixelDimension = min
					(
						Util::GetBoundMaxOrMinEdgeDist(refrPtr.get(), false, true),
						maxCrosshairSize
					);
				}
				else
				{
					// Ensure the min bound pixel distance is bound above and below
					// by the minimum and maximum crosshair sizes.
					minPixelDimension = std::clamp
					(
						Util::GetBoundMaxOrMinEdgeDist(refrPtr.get(), false, true),
						minCrosshairSize,
						maxCrosshairSize
					);
				}
				
				// Shrink down.
				crosshairSizeRatioInterpData->SetEndpoint
				(
					Util::InterpolateSmootherStep
					(
						crosshairSizeRatioInterpData->minEndpoint,
						minPixelDimension / maxCrosshairSize,
						0.25f
					),
					false
				);
				crosshairSizeRatioInterpData->UpdateInterpolatedValue(false);
			}
			else
			{
				// Back to full size.
				crosshairSizeRatioInterpData->SetEndpoint
				(
					Util::InterpolateSmootherStep
					(
						crosshairSizeRatioInterpData->maxEndpoint,
						1.0f,
						0.25f
					),
					true
				);
				crosshairSizeRatioInterpData->UpdateInterpolatedValue(true);
			}
		}
		else
		{
			// Maintain default crosshair size.
			crosshairSizeRatioInterpData->value = 1.0f;
		}
		
		// Update selection TP if the crosshair refr handle changed.
		if ((crosshairRefrHandle != prevCrosshairRefrHandle) && (isAiming || choseLockOnAimTarget))
		{
			DBG
			(
				"{}: {} -> {}, chose lock on activation/aim target: {}, {}, is aiming: {}. "
				"Activation target: {}",
				coopActor->GetName(),
				Util::HandleIsValid(prevCrosshairRefrHandle) ?
				prevCrosshairRefrHandle.get()->GetName() :
				"NONE",
				Util::HandleIsValid(crosshairRefrHandle) ?
				crosshairRefrHandle.get()->GetName() :
				"NONE",
				choseProximityActivationTarget,
				choseLockOnAimTarget,
				isAiming,
				Util::HandleIsValid(activationRefrHandle) ? 
				activationRefrHandle.get()->GetName() : 
				"NONE"
			);
			
			// Remove previous activation target if it was selected 
			// as the crosshair target.
			if (Util::HandleIsValid(activationRefrHandle) && 
				prevCrosshairRefrHandle == activationRefrHandle)
			{
				ClearActivationTargetData();
			}
			
			// If in range and selectable, set the activation target handle 
			// to the crosshair refr handle.
			if (Util::HandleIsValid(crosshairRefrHandle) && 
				Util::IsSelectableRefr(crosshairRefrHandle.get().get()))
			{
				if (RefrIsInActivationRange(crosshairRefrHandle) ||
					GlobalCoopData::IsCoopPlayer(crosshairRefrHandle))
				{
					UpdateActivationTarget(true, false, true);
					DBG
					(
						"{}: Crosshair refr {} is now selected as the activation target.",
						coopActor->GetName(), crosshairRefrHandle.get()->GetName()
					);
				}
				else
				{
					DBG
					(
						"{}: Crosshair refr {} is now selected.",
						coopActor->GetName(), crosshairRefrHandle.get()->GetName()
					);

					// Play the activation shader anyways.
					bool canActivate = CanActivateRefr(crosshairRefrHandle.get().get(), false);
					auto shader = 
					(
						canActivate ? 
						glob.activateHighlightShaders[playerID] : 
						glob.activateFailureShader
					);
					ColorizeActivationShader(shader, canActivate);
					Util::StartEffectShader
					(
						crosshairRefrHandle.get().get(),
						shader,
						max(0.1f, Settings::fSecsBetweenActivationChecks)
					);	
				}
			}
			
			if (Util::HandleIsValid(prevCrosshairRefrHandle))
			{
				Util::StopAllActivationEffectShaders(prevCrosshairRefrHandle.get().get(), playerID);
			}

			p->lastCrosshairTargetChangeTP = SteadyClock::now();
		}
		
		// The crosshair is inactive when re-centering/fading 
		// or when it has finished reaching its default position and faded out.
		// It becomes active again externally or when player moves the crosshair manually 
		// or snaps it to a target.
		if (crosshairActive)
		{
			if (aimMode == AimMode::kTwinStick || shouldResetCrosshairPosition)
			{
				DBG
				(
					"{}: Crosshair is INACTIVE. Twin stick: {}, should reset position: {}.",
					coopActor->GetName(),
					aimMode == AimMode::kTwinStick, 
					shouldResetCrosshairPosition
				);
				crosshairActive = false;
			}
		}
		else
		{
			if ((aimMode == AimMode::kCrosshair) && 
				( 
					isAiming || Util::HandleIsValid(crosshairRefrHandle)
				))
			{
				DBG
				(
					"{}: Crosshair is ACTIVE. Is aiming: {}, crosshair refr chosen: {}.",
					coopActor->GetName(),
					isAiming,
					Util::HandleIsValid(crosshairRefrHandle)
				);
				crosshairActive = true;
			}
		}

		// The crosshair is considered active when on a target, when moving the crosshair, 
		// or when selecting a new target.
		if (crosshairActive)
		{
			p->crosshairLastActiveTP = SteadyClock::now();
		}

		// Update the previous crosshair refr to current for the next frame.
		prevCrosshairRefrHandle = crosshairRefrHandle;
		// Set last update time point.
		p->lastCrosshairUpdateTP = SteadyClock::now();
	}

	void TargetingManager::UpdateCrosshairSpeedmult
	(
		const Raycast::RayResult& a_chosenResult
	)
	{
		// Using the given raycast hit result,
		// update the 2D bounds of the current crosshair-selected object, if any,
		// and compute the new speedmult to apply to the the crosshair's pixel displacement 
		// when moving the crosshair across the object.

		// Save the previous mult and then reset the current one before we recompute it.
		float prevCrosshairSpeedmult = crosshairSpeedMult;
		crosshairSpeedMult = 1.0f;

		// Nothing hit, so no speed to adjust.
		if (!validCrosshairRefrHit)
		{
			return;
		}

		// If crosshair magnetism is not enabled at all we can return early.
		if (!Settings::vbCrosshairMagnetismForObjRefs[playerID] && 
			!Settings::vbCrosshairMagnetismForActors[playerID])
		{
			return;
		}

		auto hitRefrPtr = Util::GetRefrPtrFromHandle(a_chosenResult.hitRefrHandle);
		if (!hitRefrPtr)
		{
			return;
		}
		
		auto asActor = hitRefrPtr->As<RE::Actor>(); 
		// If crosshair magnetism is not enabled for the hit result's refr type,
		// we can return early.
		if ((asActor && !Settings::vbCrosshairMagnetismForActors[playerID]) ||
			(!asActor && !Settings::vbCrosshairMagnetismForObjRefs[playerID]))
		{
			return;
		}

		// Experimental (may add as MCM option eventually):
		// No crosshair magnetism for non-hostile actors and objects when in combat.
		if ((glob.isInCoopCombat) && (!asActor || Util::IsPartyFriendlyActor(asActor)))
		{
			return;
		}

		RE::NiPoint3 boundMax{ };
		RE::NiPoint3 boundMin{ };
		RE::NiPoint3 boundCenter{ };
		RE::NiMatrix3 rotMat{ }; 
		
		// NOTE:
		// Wanted to allow for more granular selection, so the idea was to slow down
		// the crosshair based on what node it is moving across, but this obviously slows down
		// the crosshair too much when moving from node to node, 
		// as the minimum traversal time applies to each node crossed instead of the entire refr.
		// Keeping commented out for now, in case I want to revisit the idea.
		
		/*bool validDimensionsFrom3D = false;
		if (a_chosenResult.hitObjectPtr)
		{
			boundMax = 
			(
				RE::NiPoint3(0.0f, 1.0f, 0.0f) * a_chosenResult.hitObjectPtr->worldBound.radius
			);
			boundMin = -boundMax;
			boundCenter = 
			(
				a_chosenResult.hitObjectPtr->worldBound.center.Length() != 0.0f ?
				a_chosenResult.hitObjectPtr->worldBound.center :
				a_chosenResult.hitObjectPtr->world.translate
			);
			rotMat = a_chosenResult.hitObjectPtr->world.rotate;

			auto hitHkpRigidBodyPtr = Util::GethkpRigidBody(a_chosenResult.hitObjectPtr.get());
			if (hitHkpRigidBodyPtr)
			{
				DBG
				(
					"{}: {}: has rigid body.",
					coopActor->GetName(),
					a_chosenResult.hitObjectPtr->name
				);
				if (auto shape = hitHkpRigidBodyPtr->collidable.GetShape(); shape)
				{
					RE::hkTransform hkTrans{ };
					hkTrans.rotation.col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
					hkTrans.rotation.col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
					hkTrans.rotation.col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
					RE::hkAabb aabb{ };
					shape->GetAabbImpl(hkTrans, 0.0f, aabb);
					boundMax = ToNiPoint3(aabb.max) * HAVOK_TO_GAME;
					boundMin = ToNiPoint3(aabb.min) * HAVOK_TO_GAME;
					DBG
					(
						"{}: {}: has shape type {}.",
						coopActor->GetName(),
						a_chosenResult.hitObjectPtr->name,
						shape->type
					);
				}
			}

			validDimensionsFrom3D = 
			(
				boundMax.Length() != 0.0f &&
				boundMin.Length() != 0.0f &&
				boundCenter.Length() != 0.0f
			);
		}*/

		boundMax = hitRefrPtr->GetBoundMax();
		boundMin = hitRefrPtr->GetBoundMin();
		boundCenter = hitRefrPtr->data.location;
		auto refrHkpRigidBodyPtr = Util::GethkpRigidBody(hitRefrPtr.get());
		bool isDead = hitRefrPtr->IsDead();
		bool isKnocked = asActor && asActor->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal;
		bool isRagdolled = asActor && asActor->IsInRagdollState();
		bool isUprightActor = asActor && !isDead && !isKnocked && !isRagdolled;
		if (isUprightActor)
		{
			// Half up upright actor.
			boundCenter = 
			(
				asActor->data.location + 
				RE::NiPoint3(0.0f, 0.0f, 0.5f * asActor->GetHeight())
			);
		}
		else if (refrHkpRigidBodyPtr)
		{
			// Use rigidbody translation when ragdolled.
			if ((asActor) && (isDead || isKnocked || isRagdolled))
			{
				boundCenter = ToNiPoint3
				(
					refrHkpRigidBodyPtr->motion.motionState.transform.translation *
					HAVOK_TO_GAME
				);
			}
			else
			{
				boundCenter = Util::Get3DCenterPos(hitRefrPtr.get());
			}

			// Get bounds from collidable.
			if (refrHkpRigidBodyPtr->collidable.GetShape() &&
				refrHkpRigidBodyPtr->collidable.GetShape()->type == 
				RE::hkpShapeType::kBox)
			{
				auto shape = refrHkpRigidBodyPtr->collidable.GetShape();
				RE::hkTransform hkTrans{ };
				hkTrans.rotation.col0 = { 1.0f, 0.0f, 0.0f, 0.0f };
				hkTrans.rotation.col1 = { 0.0f, 1.0f, 0.0f, 0.0f };
				hkTrans.rotation.col2 = { 0.0f, 0.0f, 1.0f, 0.0f };
				RE::hkAabb aabb{ };
				shape->GetAabbImpl(hkTrans, 0.0f, aabb);
				boundMax = ToNiPoint3(aabb.max) * HAVOK_TO_GAME;
				boundMin = ToNiPoint3(aabb.min) * HAVOK_TO_GAME;
			}
		}
			
		auto hit3DPtr = Util::GetRefr3D(hitRefrPtr.get()); 
		if (hit3DPtr)
		{
			// Rotation from hit refr's 3D object.
			rotMat = hit3DPtr->world.rotate;
			// Fall back to 3D object's radius for the bounds,
			// if not found from the collidable above.
			if (boundMin == boundMax && boundMax.Length() == 0.0f)
			{
				boundMax = 
				(
					RE::NiPoint3(0.0f, 1.0f, 0.0f) * hit3DPtr->worldBound.radius
				);
				boundMin = -boundMax;
			}
		}
		else
		{
			// Fall back to refr reported angles if the refr's 3D is unavailable.
			Util::SetRotationMatrixPYR
			(
				rotMat,
				hitRefrPtr->data.angle.x,
				hitRefrPtr->data.angle.z,
				hitRefrPtr->data.angle.y
			);
		}

		// Fall back to half up the refr if the bound center is unavailable.
		if (boundCenter.Length() == 0.0f)
		{
			boundCenter = 
			(
				hitRefrPtr->data.location + 
				RE::NiPoint3(0.0f, 0.0f, 0.5f * hitRefrPtr->GetHeight())
			);
		}

		// Finally, one more fallback to using half the object's height as the bounds.
		if (boundMin == boundMax && boundMax.Length() == 0.0f)
		{
			boundMax = 
			(
				RE::NiPoint3(0.0f, 1.0f, 0.0f) * 0.5f * hitRefrPtr->GetHeight()
			);
			boundMin = -boundMax;
		}
		
		//
		// Get the edges of the bounding box.
		//
	
		// Pairs of screenspace line segment endpoints that make up the edges 
		// of the current crosshair-selected object's bounding box.
		std::vector<std::pair<RE::NiPoint3, RE::NiPoint3>> crosshairRefrScreenspaceEdges{ };

		// Offset from the bounding box's center to one of the corners 
		// along the positive X and Y axes.
		auto halfExtent = (boundMax - boundMin) / 2.0f;

		// Top face.
		RE::NiPoint3 start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
		);
		RE::NiPoint3 end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
		);
		auto lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		// Bottom face.
		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		// Connecting the faces.
		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(-halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		start = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, halfExtent.z)
		);
		end = 
		(
			boundCenter + rotMat * RE::NiPoint3(halfExtent.x, -halfExtent.y, -halfExtent.z)
		);
		lineScreenEndPoints = std::pair<RE::NiPoint3, RE::NiPoint3>
		(
			{ Util::WorldToScreenPoint3(start, false), Util::WorldToScreenPoint3(end, false) }
		);
		crosshairRefrScreenspaceEdges.emplace_back(lineScreenEndPoints);

		//
		// Compute bounding box intercept points 
		// along the line of the crosshair's movement direction on screen.
		//

		const auto& rsData = glob.cdh->GetAnalogStickState(deviceID, false);
		const auto& rsX = rsData.xComp;
		// Scaleform Y is inverted with respect to the analog stick's Y axis.
		const auto& rsY = -rsData.yComp;
		float edgeSlope = 0.0f;
		float a1 = rsY / rsX;
		float b1 = -1.0f;
		float c1 = a1 * crosshairScaleformPos.x -crosshairScaleformPos.y;
		// Save all intercept positions.
		std::vector<RE::NiPoint3> interceptPositions{ };
		for (const auto& endpoints : crosshairRefrScreenspaceEdges)
		{
			edgeSlope = 
			(
				(endpoints.second.y - endpoints.first.y) /
				(endpoints.second.x - endpoints.first.x)
			);

			float a2 = edgeSlope;
			float b2 = -1.0f;
			float c2 = a2 * endpoints.first.x - endpoints.first.y;
			float coeffDet = a1 * b2 - b1 * a2;
			float xDet = c1 * b2 - b1 * c2;
			float yDet = a1 * c2 - c1 * a2;
			float x = xDet / coeffDet;
			float y = yDet / coeffDet;
			// Intercept point must be along this edge.
			bool intersectionIsOnEdge = 
			(
				(!isnan(x) && !isinf(x) && !isnan(y) && !isinf(y)) && 
				(
					x >= min(endpoints.first.x, endpoints.second.x) &&	
					x <= max(endpoints.first.x, endpoints.second.x) &&	
					y >= min(endpoints.first.y, endpoints.second.y) &&	
					y <= max(endpoints.first.y, endpoints.second.y)
				)
			);
			if (intersectionIsOnEdge)
			{
				// Translate to origin about the crosshair's scaleform position.
				// Makes it easier to obtain the distance between intercept points.
				interceptPositions.emplace_back
				(
					RE::NiPoint3(x - crosshairScaleformPos.x, y - crosshairScaleformPos.y, 0.0f)
				);
			}
		}

		// Get the two intercept points that surround the crosshair scaleform position 
		// and are furthest from the crosshair position.
		float furthestInterceptPosDist1 = -FLT_MAX;
		float furthestInterceptPosDist2 = -FLT_MAX;
		const auto crosshairScaleformPoint = ToNiPoint3(crosshairScaleformPos);
		for (const auto& interceptPos : interceptPositions)
		{
			// Moving along an edge, so do not modify the speed mult (slope infinite or undefined).
			if (interceptPos.x - crosshairScaleformPos.x == 0.0f)
			{
				continue;
			}

			float sign = Util::NormalizeAngToPi
			(
				Util::GetYawBetweenPositions(RE::NiPoint3(), interceptPos)
			);
			float crosshairDistToIntercept = Util::GetXYDistance
			(
				0.0f, 0.0f, interceptPos.x, interceptPos.y
			);
			if (sign > 0.0f && crosshairDistToIntercept > furthestInterceptPosDist1)
			{
				furthestInterceptPosDist1 = crosshairDistToIntercept;
			}

			if (sign < 0.0f && crosshairDistToIntercept > furthestInterceptPosDist2) 
			{
				furthestInterceptPosDist2 = crosshairDistToIntercept;
			}
		}
		
		// All intercept points are on a straight line, so if all intercept positions
		// are only in one direction from the crosshair position
		// or if there are no intercept positions, 
		// we know that the crosshair is not within the bounding box yet
		// and we won't slow down the crosshair yet.
		if (furthestInterceptPosDist1 == -FLT_MAX || furthestInterceptPosDist2 == -FLT_MAX)
		{
			crosshairSpeedMult = 1.0f;
		}
		else
		{
			// Get the distance between the two bounding endpoints.
			// This is an estimation of the number of pixels the crosshair will have to traverse
			// to move from one side of the object to the other 
			// along its current movement direciton.
			float maxTraversalPixels = max
			(
				1.0f, furthestInterceptPosDist1 + furthestInterceptPosDist2
			);
			float crosshairMaxXSpeedPPS = 
			(
				Settings::vfCrosshairHorizontalSensitivity[playerID] * 
				Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			float crosshairMaxYSpeedPPS = 
			(
				Settings::vfCrosshairVerticalSensitivity[playerID] *
				Settings::fCrosshairMaxTraversablePixelsPerSec
			);
			float crosshairMaxSpeedPPS = sqrtf
			(
				crosshairMaxXSpeedPPS * crosshairMaxXSpeedPPS + 
				crosshairMaxYSpeedPPS * crosshairMaxYSpeedPPS
			);
			// In the case that true the raycast-collidable distance across the target
			// is larger than the estimated bound distance,
			// the crosshair will move much too slow, 
			// so set a lower bound to the crosshair speed mults to keep things moving.
			crosshairSpeedMult = std::clamp
			(
				(
					maxTraversalPixels == 1.0f ||
					Settings::vfMinSecsCrosshairTargetTraversal[playerID] <= 0.0f
				) ?
				1.0f :
				min
				(
					maxTraversalPixels / 
					(
						crosshairMaxSpeedPPS * 
						Settings::vfMinSecsCrosshairTargetTraversal[playerID]
					), 
					1.0f
				),
				0.01f,
				1.0f
			);
		}

		// Just in case calculations go haywire.
		if (isnan(crosshairSpeedMult) || isnan(crosshairSpeedMult))
		{
			crosshairSpeedMult = 1.0f;
		}
	}

	void TargetingManager::UpdateLockOnTargets()
	{
		// ALL TEMPORARY UNTIL NEW BINDS ARE IMPLEMENTED
		// Update the lock on crosshair/activation target.
		// Select a new crosshair target if using the aim bind.
		// Also clear the current activation target  
		// when it is no longer within activation range of the player.
			
		// TEMPORARY
		// Selects an activation lock on target in the player's left stick/facing direction,
		// starting from the player.
		
		const auto& inputStateRB = glob.cdh->GetInputState(deviceID, InputAction::kRShoulder);
		const auto noAnalogStickMask =
		(
			p->pam->inputBitMask & ((1 << !InputAction::kButtonTotal) - 1)
		);
		if (inputStateRB.isPressed)
		{
			const auto inputMask = (1 << !InputAction::kRShoulder);
			// Lone action check.
			if ((inputMask | noAnalogStickMask) != inputMask)
			{
				tempInterruptedBind1 = true;
			}

			if (!tempInterruptedBind1)
			{
				for (const auto& action : p->pam->occurringPAs)
				{
					auto occurringActionParams = 
					(
						p->pam->paStatesList[!action - !InputAction::kFirstAction].paParams
					);
					if ((occurringActionParams.inputMask & inputMask) == inputMask)
					{
						tempInterruptedBind1 = true;
						break;
					}
				}
			}

			if (!tempInterruptedBind1 && p->rsMoved)
			{
				tempInterruptedBind1 = true;
			}
		}
		else if (!inputStateRB.justReleased)
		{
			tempInterruptedBind1 = false;
		}

		auto canSelect = 
		(
			!tempInterruptedBind1 &&
			inputStateRB.justReleased &&
			(p->pam->inputBitMask & ((1 << !InputAction::kButtonTotal) - 1)) == 0
		);
		if (canSelect)
		{
			if (inputStateRB.heldTimeSecs > Settings::fSecsDefMinHoldTime)
			{
				if (Util::HandleIsValid(activationRefrHandle))
				{
					DBG("{}: {} is no longer selected.", 
						coopActor->GetName(), activationRefrHandle.get()->GetName());
					SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format
						(
							"P{}: {} is no longer selected",
							playerID + 1, activationRefrHandle.get()->GetName()
						),
						{ 
							CrosshairMessageType::kNone,
							CrosshairMessageType::kStealthState, 
							CrosshairMessageType::kTargetSelection 
						},
						0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
					);

					// Also deactivate the crosshair if the crosshair refr 
					// is also the activation refr.
					if (activationRefrHandle == crosshairRefrHandle)
					{
						DeactivateCrosshair();
					}

					// Clear current activation target when held and released.
					ClearActivationTargetData();
				}
			}
			else
			{
				// Choose a new activation target when tapped.
				UpdateActivationTarget(false, false, true);
			}
		}

		// TEMPORARY
		// Toggle lock on target.
		// If the crosshair is inactive, 
		// select a new aim target if pressing and releasing the RS without moving it.
		// If the crosshair is active, hide it.

		const auto& inputStateRThumb = glob.cdh->GetInputState(deviceID, InputAction::kRThumb);
		if (inputStateRThumb.isPressed)
		{
			const auto inputMask = (1 << !InputAction::kRThumb);
			// Lone action check.
			if ((inputMask | noAnalogStickMask) != inputMask)
			{
				tempInterruptedBind2 = true;
			}

			if (!tempInterruptedBind2)
			{
				for (const auto& action : p->pam->occurringPAs)
				{
					auto occurringActionParams = 
					(
						p->pam->paStatesList[!action - !InputAction::kFirstAction].paParams
					);
					if ((occurringActionParams.inputMask & inputMask) == inputMask)
					{
						tempInterruptedBind2 = true;
						break;
					}
				}
			}

			if (!tempInterruptedBind2 && p->rsMoved)
			{
				tempInterruptedBind2 = true;
			}
		}
		else if (!inputStateRThumb.justReleased)
		{
			tempInterruptedBind2 = false;
		}

		canSelect = 
		(
			!tempInterruptedBind2 &&
			inputStateRThumb.justReleased &&
			(p->pam->inputBitMask & ((1 << !InputAction::kButtonTotal) - 1)) == 0 &&
			!glob.cdh->GetAnalogStickState(deviceID, false).Moved()
		);
		if (canSelect)
		{
			crosshairManuallyAdjusted = false;
			if (crosshairActive)
			{
				// Signal the targeting manager to re-center and fade or remove the crosshair,
				// or clear the aim correcion target.
				if (aimMode == AimMode::kCrosshair)
				{
					DeactivateCrosshair();
					SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format
						(
							"P{}: Crosshair is now inactive",
							playerID + 1
						),
						{ 
							CrosshairMessageType::kNone
						},
						0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
				else
				{
					ClearTarget(TargetActorType::kAimCorrection);
					SetCrosshairMessageRequest
					(
						CrosshairMessageType::kGeneralNotification,
						fmt::format
						(
							"P{}: Cleared aim target",
							playerID + 1
						),
						{ 
							CrosshairMessageType::kNone
						},
						0.5f * Settings::fSecsBetweenDiffCrosshairMsgs
					);
				}
			}
			else
			{
				// Select a new lock on target.
				SetLockOnAimTarget(true, false, false);
			}
		}
		
		// TEMPORARY
		// Select a new aim target when aiming while in the 'LockOn' crosshair targeting mode.
		if (aimMode == AimMode::kCrosshair)
		{
			// Ignore the left stick.
			auto inputMask = 
			(
				p->pam->inputBitMask & 
				(((1 << !InputAction::kInputTotal) - 1) & (~(1 << !InputAction::kLS)))
			);
			const auto& inputStateRS = glob.cdh->GetInputState(deviceID, InputAction::kRS);
			if (inputStateRB.isPressed && inputStateRS.isPressed)
			{
				const auto inputMask = (1 << !InputAction::kRS) | (1 << !InputAction::kRShoulder);
				for (const auto& action : p->pam->occurringPAs)
				{
					auto occurringActionParams = 
					(
						p->pam->paStatesList[!action - !InputAction::kFirstAction].paParams
					);
					if ((occurringActionParams.inputMask & inputMask) == inputMask)
					{
						tempInterruptedBind3 = true;
						break;
					}
				}
			}
			else if (inputStateRB.justReleased || inputStateRS.justReleased)
			{
				tempInterruptedBind3 = false;
			}
			
			canSelect = 
			(
				!tempInterruptedBind3 &&
				inputStateRB.isPressed && 
				inputStateRS.isPressed
			);
			if (canSelect)
			{
				// Update as long as the right stick is not moving towards its centered position.
				const auto& stickState = glob.cdh->GetAnalogStickState(deviceID, false);
				// Small bit of re-centering wiggle room due to analog stick precision issues.
				bool shouldUpdate = 
				(
					stickState.normMag - stickState.prevNormMag > -1E-2f
				);
				if (shouldUpdate)
				{
					SetLockOnAimTarget(false, true, true);
				}
			}
		}
		
		// Check on the activation target.
		// Nothing to clear if it is not set.
		if (Util::HandleIsValid(activationRefrHandle))
		{
			// Clear if now in 'Free Aim' mode, if the refr is too far away from the player.
			if (!RefrIsInActivationRange(activationRefrHandle) &&
				!GlobalCoopData::IsCoopPlayer(activationRefrHandle))
			{
				DBG
				(
					"{}: Activation refr {} is no longer selected as the activation target.",
					coopActor->GetName(), activationRefrHandle.get()->GetName()
				);
				ClearActivationTargetData();
			}
		}
		else
		{
			if (aimMode == AimMode::kCrosshair)
			{
				// If in range and selectable, set the activation target handle 
				// to the crosshair refr handle.
				bool canSetAsActivationTarget = 
				(
					(
						Util::HandleIsValid(crosshairRefrHandle) && 
						Util::IsSelectableRefr(crosshairRefrHandle.get().get())
					) &&
					(
						RefrIsInActivationRange(crosshairRefrHandle) ||
						GlobalCoopData::IsCoopPlayer(activationRefrHandle)
					)
				);
				if (canSetAsActivationTarget)
				{
					UpdateActivationTarget(true, false, false);
					DBG
					(
						"{}: Crosshair refr {} is in range and chosen as the activation target.",
						coopActor->GetName(), crosshairRefrHandle.get()->GetName()
					);
				}

				p->lastCrosshairTargetChangeTP = SteadyClock::now();
			}
			else
			{
				if (Util::HandleIsValid(aimCorrectionTargetHandle) &&
					RefrIsInActivationRange(aimCorrectionTargetHandle))
				{
					UpdateActivationTarget(true, false, false);
					DBG
					(
						"{}: Aim correction target {} is now selected as the activation target.",
						coopActor->GetName(), aimCorrectionTargetHandle.get()->GetName()
					);
				}
			}
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
				const auto prevClosestHostileActorPtr = Util::GetActorPtrFromHandle
				(
					closestHostileActorHandle
				);

				// Update stealth detection state after storing the previous frame's values.
				UpdateStealthDetectionState();
				// Save the previous closest hostile actor and their distance from the player
				// to use in Sneak XP calculations after updating the stealth state.
				// P1's Sneak skill progression is already handled by the game.
				if (!p->isPlayer1)
				{
					// Add Sneak XP as needed.
					// XP formulae from:
					// https://en.uesp.net/wiki/Skyrim:Leveling#Skill_XP
					auto closestHostileActorPtr = Util::GetActorPtrFromHandle
					(
						closestHostileActorHandle
					);
					// Not fully detected (< 100% detection) 
					// within stealth radius of a hostile actor.
					if (detectionPct < 100.0f && 
						closestHostileActorPtr &&
						closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						// Just got within stealth XP radius of hostile actor.
						if (!prevClosestHostileActorPtr || 
							prevClosestHostileActorDist > Settings::fHostileTargetStealthRadius)
						{
							// NOTE: 
							// May not need this, but keeping for now.
							// Set last hidden time point.
							p->lastHiddenInStealthRadiusTP = SteadyClock::now();
						}
						else
						{
							GlobalCoopData::AddSkillXP
							(
								playerID,
								RE::ActorValue::kSneak, 
								0.625f * secsSinceLastStealthStateCheck
							);
						}
					}
					else if (!closestHostileActorPtr)
					{
						// Keep last hidden TP updated when not near any hostile actor.
						p->lastHiddenInStealthRadiusTP = SteadyClock::now();
					}

					// Becoming hidden (0% detected) after being detected 
					// within stealth radius of hostile actor.
					if (prevDetectionPct > 0.0f &&
						detectionPct == 0.0f && 
						closestHostileActorPtr &&
						closestHostileActorDist <= Settings::fHostileTargetStealthRadius)
					{
						GlobalCoopData::AddSkillXP(playerID, RE::ActorValue::kSneak, 2.5f);
					}
				}
			}
		}
		else if (secsSinceLastStealthStateCheck != 0.0f)
		{
			// Reset stealth state check interval when not sneaking 
			// since no checks are being performed.
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

		// Invalid proc lists -> fully detected.
		auto procLists = RE::ProcessLists::GetSingleton(); 
		if (!procLists)
		{
			detectionPctRGB = GetDetectionLvlRGB(100.0f, false);
			return;
		}

		// Accumulate detection percent.
		detectionPct = 0.0f;
		// Check each high process actor.
		for (const auto& handle : procLists->highActorHandles)
		{
			// Must be valid, targetable, and not dead.
			// Dead men tell no tales, after all.
			auto actorPtr = Util::GetActorPtrFromHandle(handle);
			if (!actorPtr || !Util::IsValidRefrForTargeting(actorPtr.get()) || actorPtr->IsDead())
			{
				continue;
			}

			// Ignore actors that do not modify the player's stealth state,
			// the player themselves, and the player's current mount.
			const bool ignored = 
			{
				actorPtr->boolFlags.any(RE::Actor::BOOL_FLAGS::kDoNotShowOnStealthMeter) ||
				actorPtr == coopActor ||
				actorPtr == p->GetCurrentMount()
			};
			if (ignored)
			{
				continue;
			}

			// Update detection percent if this actor's detection level of the player
			// is higher than the previous value.
			detectionPct = max
			(
				detectionPct, 
				static_cast<uint8_t>
				(
					Util::GetDetectionPercent(coopActor.get(), actorPtr.get())
				)
			);

			// Update the closest hostile actor and their distance to the player.
			const float distToPlayer = actorPtr->data.location.GetDistance
			(
				coopActor->data.location
			);
			if (actorPtr->IsHostileToActor(coopActor.get()) && 
				distToPlayer < closestHostileActorDist)
			{
				closestHostileActorHandle = actorPtr->GetHandle();
				closestHostileActorDist = distToPlayer;
			}
		}

		// Get detection percent RGB value after updating the player's detection percent.
		detectionPctRGB = GetDetectionLvlRGB(detectionPct, false);
	}

	void TargetingManager::UpdateTargetedRefrMotionState()
	{
		// Update motion state data for the targeted refr.

		auto targetedActorHandle = GetRangedTargetActor();
		if (Util::HandleIsValid(targetedActorHandle))
		{
			// Prioritize targeting the selected/aim correction/linked target actor.
			targetMotionState->UpdateMotionState(targetedActorHandle);
		}
		else if (Util::HandleIsValid(crosshairRefrHandle))
		{
			// Target the crosshair refr, which is NOT an actor.
			targetMotionState->UpdateMotionState(crosshairRefrHandle);
		}
		else
		{
			// Clear current motion state data.
			targetMotionState->UpdateMotionState(RE::ObjectRefHandle());
		}
	}

	void TargetingManager::UpdateTargetingOverlay()
	{
		// Draw all targeting overlay elements if there are no fullscreen menus open,
		// no temporary menus open, or this player is not controlling menus.

		baseCanDrawOverlayElements = false;
		if (auto ui = RE::UI::GetSingleton(); ui) 
		{
			bool fullscreenMenuOpen = 
			(
				ui->IsMenuOpen(RE::BookMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::MapMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::StatsMenu::MENU_NAME) || 
				ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME)
			);
			bool onlyAlwaysUnpaused = Util::MenusOnlyAlwaysUnpaused();
			bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(playerID);
			bool inDialogue = 
			(
				ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) && glob.menuPID == playerID
			);
			baseCanDrawOverlayElements = 
			(
				(onlyAlwaysUnpaused || anotherPlayerControllingMenus) &&
				(!ui->GameIsPaused() && !fullscreenMenuOpen && !inDialogue)
			);
		}

		// Update crosshair position and selected refr/actor handles first.
		if (baseCanDrawOverlayElements) 
		{
			UpdateCrosshairPosAndSelection();
		}

		// Update and draw all UI elements.
		DrawCrosshair();
		DrawActivationTargetIndicator();
		DrawAimCorrectionIndicator();
		DrawAimPitchIndicator();
		DrawPlayerIndicator();
	}

	void TargetingManager::ValidateActivationRefr(bool a_checkLOS)
	{
		// Check if the selected activation target refr is valid for activation,
		// and update the player's crosshair text to reflect that determination.
			
		// Set revive message if there is a downed player target.
		if (p->pam->downedPlayerTarget)
		{
			if (HelperFuncs::EnoughOfAVToPerformPA(p, InputAction::kActivate))
			{
				// Set revive player message.
				SetCrosshairMessageRequest
				(
					CrosshairMessageType::kReviveAlert,
					fmt::format
					(
						"P{}: <font color=\"#1E88E5\">Reviving {}</font>", 
						playerID + 1, 
						p->pam->downedPlayerTarget->coopActor->GetName()
					),
					{ 
						CrosshairMessageType::kNone,
						CrosshairMessageType::kActivationInfo, 
						CrosshairMessageType::kStealthState, 
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
			else
			{
				// Not enough health.
				SetCrosshairMessageRequest
				(
					CrosshairMessageType::kReviveAlert,
					fmt::format
					(
						"P{}: <font color=\"#FF0000\">"
						"Not enough health to revive another player!</font>", 
						playerID + 1
					),
					{
						CrosshairMessageType::kNone,
						CrosshairMessageType::kActivationInfo,
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}
		else
		{
			// Clear activation flag. Only set to true if valid below.
			canActivateRefr = false;
			const auto activationRefrPtr = Util::GetRefrPtrFromHandle(activationRefrHandle);
			// Set activation message if activation refr is valid.
			if (activationRefrPtr && Util::IsValidRefrForTargeting(activationRefrPtr.get()))
			{
				// Get base object; return early if invalid.
				auto baseObj = activationRefrPtr->GetObjectReference(); 
				if (!baseObj)
				{
					return;
				}

				// Influences what objects this player can activate 
				// (nothing that will open a menu if another player is controlling menus).
				bool anotherPlayerControllingMenus = !GlobalCoopData::CanControlMenus(playerID);
				// Activation will teleport P1.
				bool tryingToUseTeleportRefr = 
				(
					activationRefrPtr->extraList.HasType<RE::ExtraTeleport>()
				);
				// Ensure that players cannot activate any refr that will teleport the party, 
				// and consequently auto-save, while a player is downed.
				bool otherPlayerDowned = std::any_of
				(
					glob.coopPlayers.begin(), glob.coopPlayers.end(), 
					[](const auto& a_p) 
					{
						if (a_p->isActive && a_p->isDowned)
						{
							return true;
						}

						return false;
					}
				);
				// Other activation criteria.
				bool menusOnlyAlwaysOpen = true;
				if (auto ui = RE::UI::GetSingleton(); ui)
				{
					menusOnlyAlwaysOpen = Util::MenusOnlyAlwaysOpen();
				}

				bool isFurniture = baseObj->As<RE::TESFurniture>();
				bool isContainer = baseObj->As<RE::TESObjectCONT>();
				bool isCorpse = activationRefrPtr->As<RE::Actor>() && activationRefrPtr->IsDead();
				bool isDoor = baseObj->As<RE::TESObjectDOOR>();
				bool mustHoldToActivate = 
				(
					(choseQuickActivationTarget) &&
					(isContainer || isCorpse || isDoor || isFurniture) &&
					(
						p->pam->IsPerforming(InputAction::kActivate) && 
						p->pam->GetPlayerActionInputHoldTime(InputAction::kActivate) < 
						Settings::fSecsBeforeAlternateActivation
					)
				);
				bool isLocked = activationRefrPtr->IsLocked();
				// Is locked and P1 has the key.
				bool canUnlockWithKey = false;
				if (isLocked)
				{
					auto lockData = activationRefrPtr->extraList.GetByType<RE::ExtraLock>(); 
					if (lockData && lockData->lock)
					{
						// Check if P1 has the key.
						auto inventoryCounts = glob.player1Actor->GetInventoryCounts();
						auto key = lockData->lock->key;
						if (inventoryCounts.contains(key))
						{
							canUnlockWithKey = true;
						}
					}
				}

				// P1 has at least 1 lockpick.
				bool hasLockpicks = 
				(
					Util::GetLockpicksCount(RE::PlayerCharacter::GetSingleton()) > 0
				);
				// A crime to activate.
				bool offLimits = Util::ActivationIsOffLimits
				(
					coopActor.get(), activationRefrPtr.get()
				);
				// Object prevented from being activated (ex. door bars).
				bool activationBlocked = false;
				auto xFlags = activationRefrPtr->extraList.GetByType<RE::ExtraFlags>(); 
				if (xFlags)
				{
					activationBlocked = 
					(
						xFlags &&
						xFlags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate) && 
						!activationRefrPtr->extraList.GetByType<RE::ExtraAshPileRef>()
					);
				}

				// In activation range.
				bool isInRange = RefrIsInActivationRange(activationRefrHandle);
				// Is a lootable refr.
				bool isLootable = Util::IsLootableRefr(activationRefrPtr.get());
				// Player is sneaking.
				bool isSneaking = coopActor->IsSneaking();
				// Something to do with usability.
				bool isPlayable = activationRefrPtr->GetPlayable();
				// Player has LOS on the refr.
				// Use the game's P1 LOS check for crosshair refrs not selected via raycast,
				// since our raycasts do not hit such refrs right now.
				bool passesLOSCheck =
				(
					(!a_checkLOS) ||
					(
						activationRefrPtr &&
						Util::HasLOS
						(
							activationRefrPtr.get(), 
							coopActor.get(), 
							crosshairRefrHandle == activationRefrHandle &&
							!crosshairRefrFromRaycast, 
							crosshairRefrHandle == activationRefrHandle, 
							crosshairWorldPos
						)
					)
				);
					
				// Crosshair message to display.
				RE::BSFixedString activationMessage = ""sv;
				RE::BSFixedString activationString = ""sv;
				bool hasActivationText = false;
				if (!isPlayable || activationBlocked)
				{
					// Blocked from activating.
					activationMessage = fmt::format
					(
						"P{}: {} cannot be activated", playerID + 1, activationRefrPtr->GetName()
					);
				}
				else if (isLocked && !hasLockpicks && !canUnlockWithKey)
				{
					// No lockpicks or key.
					activationMessage = fmt::format("P{}: Out of lockpicks", playerID + 1);
				}
				else if (otherPlayerDowned && tryingToUseTeleportRefr)
				{
					// Can't leave the current cell with a player downed.
					activationMessage = fmt::format
					(
						"P{}: Cannot leave downed teammates behind", playerID + 1
					);
				}
				else if (!menusOnlyAlwaysOpen && anotherPlayerControllingMenus && !isLootable)
				{
					// Another player is controlling menus and the target refr is not lootable.
					activationMessage = fmt::format
					(
						"P{}: Another player is controlling menus", playerID + 1
					);
				}
				else if (!passesLOSCheck)
				{
					// Player has no LOS.
					activationMessage = fmt::format
					(
						"P{}: {} is not accessible from this position",
						playerID + 1, activationRefrPtr->GetName()
					);
				}
				else if (mustHoldToActivate)
				{
					// Do not activate objects that can trigger a menu 
					// or force a player into an animation (containers, doors, furniture) 
					// until the activation bind is held for longer than 
					// the activation cycling interval.

					activationString = Util::GetActivationText
					(
						coopActor.get(),
						baseObj,
						activationRefrPtr.get(),
						hasActivationText
					);
					if (hasActivationText)
					{
						activationMessage = fmt::format
						(
							"P{}: Hold to {}", playerID + 1, activationString
						);
					}
					else
					{
						activationMessage = fmt::format
						(
							"P{}: Hold to Interact with {}",
							playerID + 1, activationRefrPtr->GetName()
						);
					}
				}
				else
				{
					// Is another player.
					if (GlobalCoopData::IsCoopPlayer(activationRefrPtr.get()))
					{
						// Open gift menu to give players to this player after holding the bind
						// for at least the minimum hold time.
						if (p->pam->IsPerforming(InputAction::kActivate))
						{
							if (p->pam->JustStarted(InputAction::kActivate))
							{
								activationMessage = fmt::format
								(
									"P{}: Continue holding to give items to {}", 
									playerID + 1, activationRefrPtr->GetDisplayFullName()
								);
							}
							else
							{
								activationMessage = fmt::format
								(
									"P{}: Release to give items to {}",
									playerID + 1, activationRefrPtr->GetDisplayFullName()
								);

								canActivateRefr = true;
							}
						}
						else
						{
							activationMessage = fmt::format
							(
								"P{}: Player {}",
								playerID + 1,
								activationRefrPtr->GetDisplayFullName()
							);
						}
					}
					else
					{
						bool mustSneak = !isSneaking && offLimits;
						if (isInRange)
						{
							auto p1 = RE::PlayerCharacter::GetSingleton();
							auto asActor = activationRefrPtr->As<RE::Actor>();
							// Selected a hostile actor with the crosshair
							// or as the aim correction target when the crosshair is disabled.
							bool targetedHostileActor = 
							(
								(
									(asActor) && 
									(
										aimMode == AimMode::kTwinStick ||
										asActor->GetHandle() == crosshairRefrHandle
									)
								) &&
								(
									asActor->IsHostileToActor(coopActor.get()) ||
									asActor->IsHostileToActor(p1)
								)
							);
							// Living guard with a bounty out on the player.
							bool showSurrenderMessage = 
							(
								targetedHostileActor &&
								!asActor->IsDead() &&
								Util::IsGuard(asActor) &&
								Util::HasBountyOnPlayer(asActor) &&
								!coopActor->IsSneaking()
							);
							// Living, normally passive actor with no bounty on the player,
							// or fleeing the player.
							bool showStopCombatMessage = 
							(
								(
									!showSurrenderMessage &&
									targetedHostileActor &&
									!asActor->IsDead() &&
									!coopActor->IsSneaking()
								) &&
								(!Util::IsGuard(asActor) && Util::CanStopCombatWithActor(asActor))
							);
							if (showSurrenderMessage)
							{
								activationMessage = fmt::format
								(
									"P{}: Surrender to {}",
									playerID + 1, activationRefrPtr->GetName()
								);
							}
							else if (showStopCombatMessage)
							{
								activationMessage = fmt::format
								(
									"P{}: Stop combat with {}",
									playerID + 1, activationRefrPtr->GetName()
								);
							}
							else
							{
								auto boundObj = activationRefrPtr->GetBaseObject();
								// Player can activate this refr.
								// Set activation text to the refr's name 
								// if no text is available.
								activationString = Util::GetActivationText
								(
									coopActor.get(),
									baseObj,
									activationRefrPtr.get(),
									hasActivationText
								);
									SI_Error err = SI_OK;
								if (hasActivationText)
								{
									// Show regular message if performing primary activation action,
									// or custom message for secondary activation action.
									// Full credits to po3 (must have 'Use Or Take' installed):
									// https://github.com/powerof3/UseOrTake

									RE::BSFixedString activationLabel = "Use"sv;
									bool hasSecondaryActivation = true;
									if (performSecondaryActivationAction && 
										ALYSLC::UseOrTakeCompat::g_installed)
									{
										CSimpleIniA ini{ };
										ini.SetUnicode();

										// Import defaults.
										const std::filesystem::path configPath = 
										(
											"Data/SKSE/Plugins/po3_UseOrTake.ini"
										);
										err = ini.LoadFile(configPath.c_str()); 
										if (err == SI_OK && boundObj)
										{
											switch (*boundObj->formType)
											{
											case RE::FormType::Book:
											case RE::FormType::Note:
											{
												activationLabel = "Read"sv;
												break;
											}
											case RE::FormType::Armor:
											{
												Settings::ReadStringSetting
												(
													ini,
													"Armors", 
													"Alternate action label", 
													activationLabel
												);

												break;
											}
											case RE::FormType::Weapon:
											{
												Settings::ReadStringSetting
												(
													ini,
													"Weapons", 
													"Alternate action label", 
													activationLabel
												);

												break;
											}
											case RE::FormType::AlchemyItem:
											{
												// Credits to po3:
												// https://github.com/powerof3/UseOrTake/blob/master/src/Action.cpp#L81
												auto alchemyItem = 
												(
													baseObj->As<RE::AlchemyItem>()
												);
												if (alchemyItem->IsFood()) 
												{
													const auto useSound = 
													(
														alchemyItem->data.consumptionSound
													); 
													if (useSound && 
														useSound->GetFormID() == 0xB6435) 
													{  
														Settings::ReadStringSetting
														(
															ini,
															"Potions", 
															"Alternate action label", 
															activationLabel
														);
													}
													else
													{
														Settings::ReadStringSetting
														(
															ini,
															"Potions", 
															"Alternate action label (Food)", 
															activationLabel
														);
													}
												}
												else if (alchemyItem->IsPoison()) 
												{
													Settings::ReadStringSetting
													(
														ini,
														"Potions", 
														"Alternate action label (Poison)", 
														activationLabel
													);
												}

												break;
											}
											case RE::FormType::Ingredient:
											{
												Settings::ReadStringSetting
												(
													ini,
													"Ingredients", 
													"Alternate action label", 
													activationLabel
												);

												break;
											}
											case RE::FormType::Scroll:
											{
												// TODO:
												// Equip scroll support.
												Settings::ReadStringSetting
												(
													ini,
													"Scrolls", 
													"Alternate action label", 
													activationLabel
												);

												// Use the scroll right away for now.
												Settings::ReadStringSetting
												(
													ini,
													"Scrolls", 
													"Alternate secondary action label", 
													activationLabel
												);

												break;
											}
											case RE::FormType::Light:
											{
												auto light = baseObj->As<RE::TESObjectLIGH>();
												if (light->CanBeCarried())
												{
													Settings::ReadStringSetting
													(
														ini,
														"Torches", 
														"Alternate action label", 
														activationLabel
													);
												}

												break;
											}
											case RE::FormType::Ammo:
											{
												Settings::ReadStringSetting
												(
													ini,
													"Ammo", 
													"Alternate action label", 
													activationLabel
												);

												break;
											}
											default:
											{
												hasSecondaryActivation = false;
												break;
											}
											}
										}
									}
									else
									{
										hasSecondaryActivation = false;
									}

									if (hasSecondaryActivation)
									{
										if (mustSneak)
										{
											activationMessage = fmt::format
											(
												"P{}: Sneak to "
												"<font color=\"#FF0000\">{}</font> {}", 
												playerID + 1, 
												activationLabel,
												activationRefrPtr->GetName()
											);
										}
										else
										{
											activationMessage = fmt::format
											(
												"P{}: {} {}", 
												playerID + 1, 
												activationLabel,
												activationRefrPtr->GetName()
											);
										}
									}
									else
									{
										// Take readable objects as primary activation method.
										if (boundObj->Is(RE::FormType::Book, RE::FormType::Note))
										{
											if (mustSneak)
											{
												activationMessage = fmt::format
												(
													"P{}: Sneak to "
													"<font color=\"#FF0000\">take</font> {}", 
													playerID + 1,
													activationRefrPtr->GetName()
												);
											}
											else
											{
												activationMessage = fmt::format
												(
													"P{}: Take {}", 
													playerID + 1,
													activationRefrPtr->GetName()
												);
											}
										}
										else
										{
											if (mustSneak)
											{
												activationMessage = fmt::format
												(
													"P{}: Sneak to {}", 
													playerID + 1, activationString
												);
											}
											else
											{
												activationMessage = fmt::format
												(
													"P{}: {}", playerID + 1, activationString
												);
											}
										}
									}
								}
								else
								{
									if (mustSneak)
									{
										activationMessage = fmt::format
										(
											"P{}: Sneak to <font color=\"#FF0000\">interact</font> "
											"with {}",
											playerID + 1, activationRefrPtr->GetName()
										);
									}
									else if (offLimits)
									{
										activationMessage = fmt::format
										(
											"P{}: <font color=\"#FF0000\">Interact</font> "
											"with {}",
											playerID + 1, activationRefrPtr->GetName()
										);
									}
									else
									{
										activationMessage = fmt::format
										(
											"P{}: Interact with {}",
											playerID + 1, activationRefrPtr->GetName()
										);
									}
								}
								
								int32_t value = -1;
								float weight = 0.0f;
								auto asActor = activationRefrPtr->As<RE::Actor>();
								if ((asActor && asActor->IsDead()) || 
									(!asActor && activationRefrPtr->GetContainer()))
								{
									// Get total weight and value in the container.
									Util::GetWeightAndValueInRefr
									(
										activationRefrPtr.get(), weight, value
									);
								}
								else if (baseObj)
								{
									// Get weight and value for this individual refr.
									value = baseObj->GetGoldValue();
									weight = activationRefrPtr->GetWeight();
								}

								if (value >= 0)
								{
									float inventoryWeight = 
									(
										p->isPlayer1 ? 
										coopActor->GetWeightInContainer() :
										p->em->inventoryChest->GetWeightInContainer()
									);
									const auto invChanges = 
									(
										p->isPlayer1 ? 
										coopActor->GetInventoryChanges() :
										p->em->inventoryChest->GetInventoryChanges()
									);
									if (invChanges)
									{
										inventoryWeight = invChanges->totalWeight;
									}

									const float carryweight = coopActor->GetTotalCarryWeight();
									float remainingCarryweight = carryweight - inventoryWeight;
									std::string weightValue = fmt::format
									(
										", <font color=\"#{:X}\">Value: </font>"
										"<font face=\"$EverywhereBoldFont\">{}</font>, "
										"<font color=\"#{:X}\">Weight: </font>"
										"<font face=\"$EverywhereBoldFont\">{:.0f}</font>, "
										"<font color=\"#{:X}\">Space: </font>"
										"<font face=\"$EverywhereBoldFont\">"
										"<font color=\"#{:X}\">{:.0f}</font>"
										"</font>",
										0xBBA53D,
										value,
										0x999999,
										weight,
										0x804a00,
										remainingCarryweight - weight <= 0.0f ? 
										0xFF0000 : 
										0xFFFFFF,
										remainingCarryweight,
										carryweight
									);
									activationMessage = fmt::format
									(
										"{}", std::string(activationMessage) + weightValue
									);
								}
							}

							canActivateRefr = !mustSneak;
						}
						else
						{
							// Not in range.
							activationMessage = fmt::format
							(
								"P{}: {} is too far away",
								playerID + 1, activationRefrPtr->GetName()
							);
						}
					}
				}

				// Set crosshair message.
				SetCrosshairMessageRequest
				(
					CrosshairMessageType::kActivationInfo,
					activationMessage,
					{ 
						CrosshairMessageType::kNone, 
						CrosshairMessageType::kStealthState,
						CrosshairMessageType::kTargetSelection 
					},
					Settings::fSecsBetweenDiffCrosshairMsgs
				);
			}
		}
	}

	void TargetingManager::GrabbedReferenceInfo::RestoreSavedCollisionLayer()
	{
		// Restore the previously saved collision layer for non-actor refrs.

		if (!IsValid())
		{
			return;
		}
		
		auto objectPtr = refrHandle.get();
		auto refr3DPtr = Util::GetRefr3D(objectPtr.get()); 
		if (!refr3DPtr)
		{
			return;
		}
		
		// Set collision layer to the biped layer for actors 
		// that currently have the no char controller layer set. 
		// Done to prevent actors that were animation driven or grabbed while using furniture
		// from clipping through the ground once released.
		if (objectPtr->As<RE::Actor>() && savedCollisionLayer == RE::COL_LAYER::kBipedNoCC)
		{
			refr3DPtr->SetCollisionLayer(RE::COL_LAYER::kBiped);	
		}
		else
		{
			refr3DPtr->SetCollisionLayer(savedCollisionLayer);
		}

		Util::StopEffectShader(objectPtr.get(), glob.ghostFXShader);
		refr3DPtr->fadeAmount = 1.0f;
		hasCollision = savedCollisionLayer != RE::COL_LAYER::kNonCollidable;
	}

	void TargetingManager::GrabbedReferenceInfo::SaveCollisionLayer()
	{
		// Saved the refr's collision layer to restore later,
		// and then set the refr's collision layer to the given layer.

		if (!IsValid())
		{
			return;
		}
		
		auto objectPtr = refrHandle.get();
		auto refr3DPtr = Util::GetRefr3D(objectPtr.get()); 
		if (!refr3DPtr)
		{
			return;
		}
		
		savedCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
		hasCollision = savedCollisionLayer != RE::COL_LAYER::kNonCollidable;
	}

	void TargetingManager::GrabbedReferenceInfo::ToggleCollision()
	{
		// Toggle collision on/off for this grabbed refr.
		// Also play a shader and adjust fade amount.

		if (!IsValid())
		{
			return;
		}

		auto objectPtr = Util::GetRefrPtrFromHandle(refrHandle);
		auto refr3DPtr = Util::GetRefr3D(objectPtr.get());
		if (!refr3DPtr)
		{
			return;
		}

		auto currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
		// Restore saved and do not toggle if still unidentified.
		if (currentCollisionLayer == RE::COL_LAYER::kUnidentified)
		{
			refr3DPtr->SetCollisionLayer(savedCollisionLayer);
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
			return;
		}

		// Turn off collision and play shader to indicate the change.
		if (currentCollisionLayer != RE::COL_LAYER::kNonCollidable)
		{	
			refr3DPtr->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
			if (!hasCollision)
			{
				Util::StartEffectShader(objectPtr.get(), glob.ghostFXShader);
			}
		}
		else
		{
			// Turn collision back on and stop the shader.
			refr3DPtr->SetCollisionLayer(savedCollisionLayer);
			Util::StopEffectShader(objectPtr.get(), glob.ghostFXShader);
			refr3DPtr->fadeAmount = 1.0f;
			if (auto fadeNode = refr3DPtr->AsFadeNode(); fadeNode)
			{
				fadeNode->currentFade = 1.0f;
			}
			
			currentCollisionLayer = Util::GetCollisionLayer(refr3DPtr.get());
			hasCollision = currentCollisionLayer != RE::COL_LAYER::kNonCollidable;
		}
	}

	void TargetingManager::GrabbedReferenceInfo::UpdateGrabbedReference
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const uint8_t& a_index, 
		const float& a_firstGrabbedReferenceBufferDist
	)
	{
		// Update the position of grabbed refrs by setting their velocity.
		// Arrange multiple grabbed refrs in a (very poorly formed) ring 
		// about the first grabbed refr.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		// Fade out the object slightly if its collision is toggled off.
		if (!hasCollision)
		{
			auto refr3DPtr = Util::GetRefr3D(objectPtr.get());
			if (refr3DPtr)
			{
				refr3DPtr->fadeAmount = 0.5f;
				if (auto fadeNode = refr3DPtr->AsFadeNode(); fadeNode)
				{
					fadeNode->currentFade = 0.5f;
				}
			}
		}

		// Update grabbed refr orientation.
		bool isRagdolled = a_p->coopActor->IsInRagdollState();
		float facingAng = a_p->coopActor->GetHeading(false);
		const auto aimCorrectionTargetPtr = Util::GetActorPtrFromHandle
		(
			a_p->tm->aimCorrectionTargetHandle
		);
		if (a_p->mm->faceCrosshairPos)
		{
			// For S.M.O.R.F/M.A.R.F-ing, attempt to place the other grabbed players
			// between the player and the crosshair world position/target torso position
			// when facing it.
			facingAng = Util::GetYawBetweenPositions
			(
				a_p->coopActor->data.location, a_p->tm->crosshairWorldPos
			);
		}
		else if (isRagdolled)
		{
			// The last recorded LS game angle.
			facingAng = a_p->analogStickParams[!AnalogStickParams::kLSCamRelAng];
		}
		
		// Suspend the grabbed objects in front of the player
		// at a distance dependent on their max reach and the object's height.
		float objectHeight = objectPtr->GetHeight();
		float baseSuspensionDist = a_p->tm->GetMaxActivationDist() / 3.0f;
		float xySuspensionDist = max(objectHeight, baseSuspensionDist);
		// Can move the grabbed object(s) closer or farther from the player
		// by displacing the RS right (farther) and left (closer).
		const auto& rsData = glob.cdh->GetAnalogStickState(a_p->deviceID, false);
		if (a_p->pam->IsPerforming(InputAction::kAdjustAimPitch) && 
			fabsf(rsData.xComp) > fabsf(rsData.yComp))
		{
			a_p->tm->grabbedRefrDistanceOffset = std::clamp
			(
				a_p->tm->grabbedRefrDistanceOffset + 
				(
					rsData.xComp * 
					rsData.normMag *
					a_p->coopActor->GetHeight() *
					*g_deltaTimeRealTime
				),
				-baseSuspensionDist,
				2.0f * baseSuspensionDist
			);
		}

		// Arranged in a circle about the index 0 grabbed object.
		// Sweeps out clockwise from directly above the index 0 object.
		// Positional offset from the first object.
		auto indexBasedOffset = RE::NiPoint3();
		// Spacing between subsequent grabbed objects is dependent on the objects' radii/heights.
		float indexOffsetScalar = a_firstGrabbedReferenceBufferDist;
		if (auto object3DPtr = Util::GetRefr3D(objectPtr.get()); object3DPtr)
		{
			// Add the central object's radius to a portion of the current grabbed object's radius.
			indexOffsetScalar = 
			(
				a_firstGrabbedReferenceBufferDist + 
				min(objectHeight * 0.25f, object3DPtr->worldBound.radius * 0.25f)
			);
		}

		auto forward = Util::RotationToDirectionVect(0.0f, Util::ConvertAngle(facingAng));
		// Set direction from the central object based on the object's index.
		if (a_index > 0)
		{
			indexBasedOffset = RE::NiPoint3(0.0f, 0.0f, 1.0f);
			Util::RotateVectorAboutAxis
			(
				indexBasedOffset,
				forward, 
				2.0f * PI * ((float)(a_index - 1) / (float)(Settings::uMaxGrabbedReferences - 1))
			);
		}
		
		// Full credits to ersh1 once again for the steps 
		// to access a refr's motion type and apply linear velocity:
		// https://github.com/ersh1/Precision/blob/702428bc065c75b3964a0324992658b1ab0a0821/src/Havok/ContactListener.cpp#L8

		// Additional distance and speed mults if paragliding or M.A.R.F-ing.
		float suspensionDistMult = 1.0f;
		float maxSpeedMult = 1.0f;
		// Absolute max speed the grabbed object can reach.
		float grabbedRefrMaxSpeed = Settings::fBaseGrabbedRefrMaxSpeed;
		// Speedmult and suspension distance adjustments specifically for grabbing players.
		if ((a_p->tm->isSMORFing) || 
			(Settings::bCanGrabOtherPlayers && GlobalCoopData::IsCoopPlayer(objectPtr.get())))
		{
			if (a_p->mm->isParagliding)
			{
				// Carry other players while paragliding.
				// Make sure they keep up.
				maxSpeedMult = 2.0f;
				suspensionDistMult = 2.0f;
			}
			else if (a_p->tm->isMARFing || a_p->tm->isSMORFing)
			{
				// Uhh, we have Skyrim's Paraglider at home, guys. Really!
				// M.A.R.F/S.P.O.R.F is on.
				if (ALYSLC::SkyrimsParagliderCompat::g_installed && 
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
		// and when players are attempting to move in opposite directions,
		// but a bit better when based from the player's torso instead of from their head.
		RE::NiPoint3 basePos =
		(
			a_p->tm->isMARFing || a_p->tm->isSMORFing ?
			a_p->mm->playerTorsoPosition :
			RE::NiPoint3
			(
				a_p->coopActor->data.location.x,
				a_p->coopActor->data.location.y,
				a_p->coopActor->data.location.z + a_p->coopActor->GetHeight()
			)
		);
		targetPosition = 
		{
			(
				basePos.x + 
				(
					(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
					suspensionDistMult * 
					cosf(Util::ConvertAngle(facingAng)) *
					cosf(a_p->mm->aimPitch)
				)
			),
			(
				basePos.y + 
				(
					(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
					suspensionDistMult * 
					sinf(Util::ConvertAngle(facingAng)) * 
					cosf(a_p->mm->aimPitch)
				)
			),
			(
				basePos.z
			)
		};

		// Can move the grabbed refr vertically in an arc around the player by adjusting aim pitch.
		if (a_p->tm->isMARFing || a_p->tm->isSMORFing)
		{
			targetPosition.z += 
			(
				(xySuspensionDist + a_p->tm->grabbedRefrDistanceOffset) * 
				suspensionDistMult * 
				-sinf(a_p->mm->aimPitch)
			);
		}
		else
		{
			targetPosition.z += 
			(
				(a_p->coopActor->GetHeight() + a_p->tm->grabbedRefrDistanceOffset) * 
				suspensionDistMult * 
				-sinf(a_p->mm->aimPitch)
			);
		}

		// Get the object's position.
		auto objectPos = Util::GetRefrPosition(objectPtr.get());
		// Apply positional offset scalar to the normalized offset after rotation.
		indexBasedOffset *= indexOffsetScalar;
		// Now finalize the target position by adding the positional offset based on grab index.
		targetPosition += indexBasedOffset;

		// Positional delta to reach the target position.
		auto posDelta = targetPosition - objectPos;
		const float distToTargetPos = posDelta.Length();
		// Normalized direction.
		auto dir = distToTargetPos == 0.0f ? RE::NiPoint3() : posDelta / distToTargetPos;

		// Apply max speed mult.
		grabbedRefrMaxSpeed *= maxSpeedMult;
		// Should cap out at the player's movement speed if higher than the pre-defined max speed.
		auto mountPtr = a_p->GetCurrentMount();
		float playerMovementSpeed = 
		(
			isRagdolled && a_p->coopActor->GetCharController() ? 
			a_p->coopActor->GetCharController()->outVelocity.Length3() :
			mountPtr ? 
			mountPtr->DoGetMovementSpeed() :
			a_p->coopActor->DoGetMovementSpeed()
		);
		grabbedRefrMaxSpeed = max(grabbedRefrMaxSpeed, playerMovementSpeed);
		// Slow down when nearing the target position. Reduces jitter.
		const float slowdownRadius = a_p->tm->GetMaxActivationDist();
		float slowdownFactor = Util::InterpolateEaseOut
		(
			0.0f, 
			1.0f, 
			std::clamp
			(
				distToTargetPos / slowdownRadius, 
				0.0f, 
				1.0f
			), 
			5.0f
		);

		// Don't move at all when too close.
		auto playerToTargetDir = targetPosition - a_p->coopActor->data.location;
		auto objectToPlayerDir = a_p->coopActor->data.location - objectPos;
		playerToTargetDir.Unitize();
		objectToPlayerDir.Unitize();
		float catchupFactor = 5.5f + playerToTargetDir.Dot(objectToPlayerDir) * 4.5f;
		// Velocity to apply to the refr.
		auto havokVelocity = RE::NiPoint3();
		if ((!isRagdolled) && (a_p->lsMoved || playerMovementSpeed > 0.0f))
		{
			havokVelocity = 
			(
				dir * 
				fmin
				(
					playerMovementSpeed * catchupFactor, 
					grabbedRefrMaxSpeed
				) 
			);
		}
		else
		{
			havokVelocity = posDelta * catchupFactor;
		}
		
		// Cap to prevent overshooting.
		if (float speed = havokVelocity.Length(); speed != 0.0f)
		{
			havokVelocity = 
			(
				(havokVelocity / speed) * min(speed, distToTargetPos / *g_deltaTimeRealTime)
			);
		}

		// Convert to havok units before setting below.
		havokVelocity *= GAME_TO_HAVOK;
		// Adjust havok velocity here or, if there is no valid rigid body,
		// as in the case of some active projectiles,
		// still save the velocity to apply regardless.
		// Our projectile UpdateImpl() hook will still update active projectiles' velocity 
		// even if they do not have a rigid body.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		if (isActiveProjectile)
		{
			RE::NiPoint3 oldVelocity = asProjectile->linearVelocity;
			havokVelocity.x = Util::InterpolateSmootherStep
			(
				oldVelocity.x, 
				havokVelocity.x, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.y = Util::InterpolateSmootherStep
			(
				oldVelocity.y, 
				havokVelocity.y, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.z = Util::InterpolateSmootherStep
			(
				oldVelocity.z,
				havokVelocity.z, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			// Save our new velocity to apply later in the UpdateImpl() hook.
			lastSetVelocity = havokVelocity * HAVOK_TO_GAME;
		}
		else
		{
			// Need a valid rigid body if not an active projectile.
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get()); 
			if (!hkpRigidBodyPtr)
			{
				return;
			}

			RE::NiPoint3 oldVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity);
			havokVelocity.x = Util::InterpolateSmootherStep
			(
				oldVelocity.x, 
				havokVelocity.x, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.y = Util::InterpolateSmootherStep
			(
				oldVelocity.y, 
				havokVelocity.y, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			havokVelocity.z = Util::InterpolateSmootherStep
			(
				oldVelocity.z,
				havokVelocity.z, 
				std::clamp(0.0f, 1.0f, 0.85f * (60.0f * *g_deltaTimeRealTime))
			);
			
			// Activate the refr and set the computed velocity.
			// Without activation, the object will not always move to our target position
			// and a discrepancy between the havok rigid body and node positions may develop.
			// A discrepancy between the refr data reported position and the refr's 3D position
			// can also stall the object in the air without activation first.
			Util::NativeFunctions::hkpEntity_Activate(hkpRigidBodyPtr.get());
			hkpRigidBodyPtr->motion.SetLinearVelocity(havokVelocity);
			// Save our new velocity.
			lastSetVelocity = havokVelocity * HAVOK_TO_GAME;

			// Adjust the grabbed object's rotation if performing the requisite action.
			// [Rotation controls]:
			// 1. Move the right stick up and down to rotate along the horizontal axis 
			// facing right relative to the player, 
			// 2. Move the right stick left and right
			// to rotate along the vertical axis facing upward.
			bool shouldAdjustRotation = a_p->pam->IsPerforming(InputAction::kGrabRotateYZ);
			if (shouldAdjustRotation)
			{
				const auto& headingAngle = a_p->coopActor->GetHeading(false);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[0] = 
				(
					-Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * cosf(headingAngle)
				);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[1] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.yComp * sinf(headingAngle)
				);
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[2] = 
				(
					Settings::fGrabbedRefrBaseRotSpeed * rsData.xComp
				);
			}
			else
			{
				// Zero out angular velocity when not rotating.
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[0] = 0.0f;
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[1] = 0.0f;
				hkpRigidBodyPtr->motion.angularVelocity.quad.m128_f32[2] = 0.0f;
			}
		}
	}

	void TargetingManager::ReleasedReferenceInfo::ApplyVelocity(RE::NiPoint3& a_velocityToSet)
	{
		// Save the given velocity, cap the given speed of this released refr 
		// to the release speed, and then apply the capped velocity.

		if (!IsValid())
		{
			Clear();
			return;
		}
		
		float currentSpeed = a_velocityToSet.Length();
		// No need to set a velocity of 0.
		if (currentSpeed == 0.0f)
		{
			return;
		}

		// Clamp the velocity.
		if (isHoming && currentSpeed > releaseSpeed)
		{
			a_velocityToSet = (a_velocityToSet / currentSpeed) * (releaseSpeed);
		}

		// Apply the velocity directly here to any refrs with a rigid body.
		// Active projectiles have their velocity saved here and set in the UpdateImpl() hook.
		lastSetVelocity = a_velocityToSet;
		auto objectPtr = refrHandle.get();
		if (!isActiveProjectile)
		{
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get()); 
			if (!hkpRigidBodyPtr || !isThrown)
			{
				return;
			}
			
			// Activate the refr and set the computed velocity.
			// Without activation, the object will not always move to our target position
			// and a discrepancy between the havok rigid body and node positions may develop.
			// A discrepancy between the refr data reported position and the refr's 3D position
			// can also stall the object in the air without activation first.
			Util::NativeFunctions::hkpEntity_Activate(hkpRigidBodyPtr.get());
			hkpRigidBodyPtr->motion.SetLinearVelocity(a_velocityToSet * GAME_TO_HAVOK);
		}
	}
	
	RE::NiPoint3 TargetingManager::ReleasedReferenceInfo::CalculatePredInterceptPos
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Calculate the position at which the released refr is likely to collide
		// with the target once the refr is released. Use the target's physical motion
		// data to perform this calculation.

		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		// No valid target refr, so aim at the crosshair position.
		if (!targetRefrPtr)
		{
			return a_p->tm->crosshairWorldPos;
		}

		// Get targeted actor, if targeting one.
		auto targetActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		// Invert pitch convention for use with trig functions.
		const double& aimPitch = -a_p->mm->aimPitch;
		// Set the initial predicted intercept/hit position to the initial end position.
		RE::NiPoint3 predHitPos = trajectoryEndPos;
		// Next predicted velocity for the target. Set to current velocity initially.
		RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
		// Axis to rotate velocity vector around.
		RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
		// XY and Z offsets to the predicted position from the release position.
		double xy = Util::GetXYDistance(predHitPos, releasePos);
		double z = (predHitPos - releasePos).z;
		// Current delta yaw and yaw rotation speed.
		const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
		float currentZRotSpeed = 0.0f;
		if (targetActorPtr)
		{
			currentZRotSpeed =
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ?
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z :
				0.0f
			);
		}

		// Average of current and average per interval yaw deltas.
		float avgYawDeltaPerFrame = 
		(
			(
				currentYawAngDelta / (*g_deltaTimeRealTime) + 
				a_p->tm->targetMotionState->apiYawAngDelta / (*g_deltaTimeRealTime)
			) / 2.0
		);
		// Average of current and average per interval change in speed.
		const float avgSpeedDelta = 
		(
			(
				a_p->tm->targetMotionState->apiSpeedDelta +
				a_p->tm->targetMotionState->cSpeedDeltaPerFrame
			) / 2.0f
		);

		// Time to target.
		double t = xy / releaseSpeed * cosf(aimPitch);
		// Previously calculated time to target.
		double tPrev = 0.0;
		// Difference in the calculated times to target.
		double tDiff = fabsf(t - tPrev);
		// Flight time deltas at which to bail out of the calculation loop.
		double timeBailDeltaMin = 1E-4;
		double timeBailDeltaMax = 1000.0;
		// Max number of iterations, current number of iterations.
		uint8_t steps = 50;
		uint8_t step = 0;
		// Attempt to accurately estimate the target intercept position
		// and continue looping until the reported time-to-target values converge
		// to below the minimum time diff (success),
		// or diverge above the maximum time diff (failure),
		// or until the maximum number of iterations is reached.
		while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
		{
			// SUPER NOTE: 
			// Everything below is obviously not mathematically correct,
			// since the target's velocity and acceleration are changing constantly,
			// which means that finding the best predicted hit position
			// would require integration over the time of flight.
			// However the recorded acceleration and velocity motion data
			// for targets is very noisy, which leads to huge overshoots
			// when using the proper formulas for calculating the predicted position at time t.
			// This temporary, manually-tested solution performs slightly better.

			// Rotate predicted velocity vector by the yaw diff 
			// which will occur over the time delta.
			double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
			double speed = nPredTargetVel.Length();
			// Rotate and re-apply original speed, since the vector is normalized upon rotation.
			Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
			nPredTargetVel.Unitize();
			nPredTargetVel *= speed;
			// Offset the current aimed at position by the delta position calculated
			// using the position delta over the elapsed time frame from the previous iteration.
			auto posDelta = nPredTargetVel * (t - tPrev);
			predHitPos += posDelta;

			// Update positional offsets based on the new predicted hit position.
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
			// Failed to find intercept position, so set to the initially-aimed-at position.
			return trajectoryEndPos;
		}
		else
		{
			// Either converged on a particular intercept position, 
			// with the change in time to target under the lower bail precision (success),
			// or didn't quite meet that required precision (failed).
			return predHitPos;
		}
	}

	RE::NiPoint3 TargetingManager::ReleasedReferenceInfo::GuideRefrAlongTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Direct this released refr at either the initial target position 
		// along a fixed trajectory or continuously towards the target position/target refr.

		RE::NiPoint3 velToSet = RE::NiPoint3();
		if (!IsValid()) 
		{
			Clear();
			return velToSet;
		}

		auto objectPtr = refrHandle.get();
		if (!releaseTP.has_value())
		{
			Clear();
			return velToSet;
		}
		
		// Stop setting velocity to force the released refr along the predicted fixed trajectory
		// if the released refr has passed its time of flight,
		// is submerged after reaching the apex of the initial trajectory, 
		// or hit something or its target.
		// Collisions are still active though.
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		const float t = Util::GetElapsedSeconds(releaseTP.value());
		bool stopSettingPredictedTraj = 
		(
			(trajType == ProjectileTrajType::kPrediction) && 
			(
				(
					(!isActiveProjectile) && 
					(
						(t > initialTimeToTarget) || 
						(
							t > 0.5f * initialTimeToTarget && 
							objectPtr->GetSubmergeLevel
							(
								objectPtr->data.location.z, objectPtr->parentCell
							) == 1.0f
						)
					)
				) ||
				(!objectPtr->As<RE::Actor>() && firstHitTP.has_value()) ||
				(
					(objectPtr->As<RE::Actor>()) && 
					(
						(
							targetRefrPtr && hitRefrFIDs.contains(targetRefrPtr->formID)
						) ||
						(
							!targetRefrPtr && firstHitTP.has_value()
						)
					)
				)
			)
		);
		if (stopSettingPredictedTraj)
		{
			return velToSet;
		}

		// Get gravitational constant.
		double g = Util::GetGravitationalConstant();
		bool shouldUseHomingTrajectory = trajType == ProjectileTrajType::kHoming;
		// Set the target position. Default to crosshair world position first.
		RE::NiPoint3 aimTargetPos = a_p->tm->crosshairWorldPos;
		auto asActorPtr = 
		(
			targetRefrPtr ? RE::ActorPtr(targetRefrPtr->As<RE::Actor>()) : nullptr
		);
		if (shouldUseHomingTrajectory)
		{
			targetLocalPosOffset = 
			(
				targetRefrPtr ? 
				a_p->tm->crosshairLocalPosOffset : 
				RE::NiPoint3()
			);
			bool targetActorValidity = 
			(
				asActorPtr && Util::IsValidRefrForTargeting(asActorPtr.get())
			);
			if (targetActorValidity) 
			{
				aimTargetPos = Util::GetTorsoPosition(asActorPtr.get()) + targetLocalPosOffset;
			}
			else if (targetRefrPtr)
			{
				aimTargetPos = Util::GetRefrPosition(targetRefrPtr.get()) + targetLocalPosOffset;
			}
		}
		else
		{
			aimTargetPos = trajectoryEndPos;
		}
		
		// Get the object's position.
		auto objectPos = Util::GetRefrPosition(objectPtr.get());
		// Pitch and yaw to the target position.
		float pitchToTarget = Util::GetPitchBetweenPositions(objectPos, aimTargetPos);
		float yawToTarget = Util::GetYawBetweenPositions(objectPos, aimTargetPos);
		// Get the released refr's current velocity.
		RE::NiPoint3 currentVelocity{ };
		// Use the havok velocity here or, if there is no valid rigid body,
		// still set the velocity to apply regardless.
		// Our projectile UpdateImpl() hook can still update active projectiles' velocity 
		// even if they do not have a rigid body.
		if (isActiveProjectile)
		{
			auto asProjectile = objectPtr->As<RE::Projectile>();
			if (asProjectile)
			{
				currentVelocity = asProjectile->linearVelocity;
			}
			else
			{
				objectPtr->GetLinearVelocity(currentVelocity);
			}
		}
		else
		{
			auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get());
			if (!hkpRigidBodyPtr)
			{
				return velToSet;
			}

			currentVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME);
		}

		// Set speed to the corresponding speed along the fixed trajectory.
		// Will cap speed later if necessary.
		const float velXY = releaseSpeed * cosf(launchPitch);
		const float velX = velXY * cosf(launchYaw);
		const float velY = velXY * sinf(launchYaw);
		const float velZ = releaseSpeed * sinf(launchPitch) - g * t;
		float speedToSet = RE::NiPoint3(velX, velY, velZ).Length();
		const bool tooLongToReach = 
		(
			initialTimeToTarget == 0.0f ||
			initialTimeToTarget >= Settings::fMaxSecsBeforeClearingReleasedRefr
		);
		// Cannot split the trajectory into two parts 
		// if the projectile reaches the target in under two frames,
		// so we'll start homing in right away, if this is a homing projectile.
		const bool lessThanTwoFramesToReachTarget = 
		(
			initialTimeToTarget <= *g_deltaTimeRealTime * 2.0f
		);
		// Direct the released refr along the fixed trajectory determined at launch,
		// but keep the fixed trajectory pitch until the refr starts homing in.
		// Also check if the released refr should start homing in.
		// Save previous homing state.
		if (!startedHomingIn || !shouldUseHomingTrajectory)
		{
			// Maintain launch yaw and current pitch along the fixed trajectory portion
			// of the flight.
			// Current XY and Z positions relative to the release position.
			const float xy = Util::GetXYDistance(releasePos, objectPos);
			const float z = objectPos.z - releasePos.z;
			float nextT = (t + *g_deltaTimeRealTime);
			RE::NiPoint3 targetPos = RE::NiPoint3
			(
				releasePos.x + releaseSpeed * cosf(launchPitch) * cosf(launchYaw) * nextT,
				releasePos.y + releaseSpeed * cosf(launchPitch) * sinf(launchYaw) * nextT,
				releasePos.z + 
				releaseSpeed * sinf(launchPitch) * nextT - 
				0.5f * g * nextT * nextT
			);
			lastSetTargetPosition = targetPos;
			
			// 'Connect the dots' -> set velocity so that the refr reaches the projected position
			// during the next frame.
			velToSet = (targetPos - objectPos) / *g_deltaTimeRealTime;

			// Update fall height if the thrown actor is still rising,
			// so that we can factor in this height to our bonk and splat damage calculations.
			const auto centerPos = Util::Get3DCenterPos(objectPtr.get());
			if (centerPos.z > fallHeight)
			{
				fallHeight = centerPos.z;
			}

			// Nothing to do now if not guiding a homing projectile.
			if (!shouldUseHomingTrajectory)
			{
				return velToSet;
			}

			//======================
			// [Start-homing Check]:
			//======================
			// Check if homing projectile should fully start homing in on the target 
			// instead of following its initial fixed trajectory.
			// Either:
			// 1. Too long to reach the target.
			// 2. Past half the distance/time of flight.
			// 3. Will reach the target in under 2 frames.
			// Set homing flags if not already homing.

			// Check if the projectile has passed the halfway point of its flight.
			const float epsilon = 1E-3f;
			bool passedHalfwayPoint = 
			(
				t - 0.5f * initialTimeToTarget >= -epsilon ||
				xy > (0.5f * Util::GetXYDistance(releasePos, aimTargetPos))
			);
			bool noTargetAndMovingCrosshair = 
			(
				!targetRefrPtr &&
				a_p->tm->crosshairActive &&
				a_p->pam->IsPerforming(InputAction::kMoveCrosshair)
			);
			if (noTargetAndMovingCrosshair ||
				tooLongToReach ||
				passedHalfwayPoint ||
				lessThanTwoFramesToReachTarget)
			{
				// Used to check if the projectile should switch to homing mode.
				startedHomingIn = isHoming = true;
			}
			else
			{
				// Nothing more to do until the projectile starts homing in.
				return velToSet;
			}
		}
		
		// Only check if the projectile should stop homing once it starts.
		if (isHoming)
		{
			// Home in until past the target, or the target is hit, 
			// or if there is no target, until a hit is recorded.
			// Direction from the current position to the target.
			auto dirToTarget = aimTargetPos - objectPos;
			dirToTarget.Unitize();
			// Last frame's velocity direction.
			auto velDirLastFrame = currentVelocity;
			velDirLastFrame.Unitize();
			// Angle between last frame's velocity and the target.
			float angBetweenVelAndToTarget = acosf
			(
				std::clamp(dirToTarget.Dot(velDirLastFrame), -1.0f, 1.0f)
			);
			// Went past the target if velocity direction and direction to target 
			// diverge by >= 90 degrees and the distance to the target 
			// is less than the max distance travelable per frame (will pass the target next frame).
			bool passingTarget = 
			(
				angBetweenVelAndToTarget >= PI / 2.0f &&
				objectPos.GetDistance(aimTargetPos) <= 
				currentVelocity.Length() * *g_deltaTimeRealTime
			);
			isHoming = 
			(
				(!passingTarget) && 
				(
					(
						targetRefrPtr && !hitRefrFIDs.contains(targetRefrPtr->formID)
					) ||
					(
						!targetRefrPtr && !firstHitTP.has_value()
					)
				)
			);

			// REMOVE when done debugging.
			/*DBG
			(
				"{}: {} is homing: {}, passing target: {}, ang between vel and target: {}, "
				"dist to target: {}, hit target: {}, ttt: {}, t: {}. Hit refrs count: {}. "
				"Dist from release pos: {}, z offset: {}.",
				a_p->coopActor->GetName(),
				objectPtr->GetName(),
				isHoming,
				passingTarget,
				angBetweenVelAndToTarget * TO_DEGREES,
				objectPos.GetDistance(aimTargetPos),
				(targetRefrPtr && hitRefrFIDs.contains(targetRefrPtr->formID)),
				initialTimeToTarget,
				t,
				hitRefrFIDs.size(),
				objectPos.GetDistance(releasePos),
				fabsf(objectPos.z - releasePos.z)
			);*/
		}

		// If still homing in after updating the flag, set the new homing velocity.
		if (isHoming)
		{
			//=================================
			// [Set Pitch/Yaw to Track Target]:
			//=================================

			float currentVelPitch = Util::GetPitchBetweenPositions
			(
				RE::NiPoint3(), currentVelocity
			);
			// Sometimes invalid at launch time for some reason (-nan).
			float currentVelYaw = Util::GetYawBetweenPositions(RE::NiPoint3(), currentVelocity);
			const float t = Util::GetElapsedSeconds(releaseTP.value());
			float pitchToSet = currentVelPitch;
			float yawToSet = currentVelYaw;
			// Just launched or invalid, so set pitch and yaw to saved launch values.
			if (t == 0.0f || 
				isnan(currentVelPitch) || 
				isinf(currentVelPitch))
			{
				currentVelPitch = -launchPitch;
			}

			if (t == 0.0f || isnan(currentVelYaw) || isinf(currentVelYaw))
			{
				currentVelYaw = Util::ConvertAngle(launchYaw);
			}

			// Lerp pitch/yaw to slowly rotate the projectile to face the target in the XY plane.
			// Fully face the target in the XY plane once the projectile's initial time 
			// to the target position has elapsed.
			// Velocity on fixed trajectory determined by projectile launch data.
			if (tooLongToReach || lessThanTwoFramesToReachTarget)
			{
				// Can't hit target with given launch pitch, 
				// so set yaw directly to target right away.
				pitchToSet = pitchToTarget;
				yawToSet = yawToTarget;
			}
			else
			{
				// Slowly turn to face.
				float pitchDiff = Util::NormalizeAngToPi(pitchToTarget - currentVelPitch);
				pitchToSet = Util::NormalizeAngToPi
				(
					currentVelPitch + 
					Util::InterpolateSmootherStep
					(
						0.0f, pitchDiff, min(1.0f, t / (initialTimeToTarget))
					)
				);
				float yawDiff = Util::NormalizeAngToPi(yawToTarget - currentVelYaw);
				yawToSet = Util::NormalizeAng0To2Pi
				(
					currentVelYaw + 
					Util::InterpolateSmootherStep
					(
						0.0f, yawDiff, min(1.0f, t / (initialTimeToTarget))
					)
				);
			}

			// Set velocity.
			velToSet = 
			(
				Util::RotationToDirectionVect(-pitchToSet, Util::ConvertAngle(yawToSet)) *
				speedToSet
			);

			auto targetPos = lastSetTargetPosition + velToSet * *g_deltaTimeRealTime;
			lastSetTargetPosition = targetPos;
		}

		return velToSet;
	}

	bool TargetingManager::ReleasedReferenceInfo::InitPreviewTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Set initial release trajectory info for this refr prior to its release.
		// Determines the released refr's initial orientation, speed, time to target,
		// targeted trajectory end position, 
		// and whether or not the released refr was thrown or dropped.
		// The refr's speed is not modified, nor is magicka deducted from the player.
		// Return true if the player can successfully throw this refr.

		if (!IsValid())
		{
			return false;
		}

		auto objectPtr = refrHandle.get();
		if (!objectPtr)
		{
			return false;
		}
		
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrPtrValidity = 
		(
			targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActor = targetRefrPtrValidity ? targetRefrPtr->As<RE::Actor>() : nullptr;
		// If not trying to throw this refr, return early.
		bool objectIsPlayer = GlobalCoopData::IsCoopPlayer(objectPtr.get());
		bool notTryingToThrow = 
		(
			(
				!a_p->pam->IsPerforming(InputAction::kGrabObject) ||
				!a_p->tm->crosshairActive || 
				objectPtr == a_p->coopActor ||
				refrHandle == a_p->tm->crosshairRefrHandle
			) ||
			(
				(a_p->tm->aimMode == AimMode::kTwinStick) && 
				(!targetActor || targetRefrHandle == refrHandle)
			) ||
			(
				(objectPtr->As<RE::Actor>()) && 
				(!objectPtr->IsDead()) &&
				(
					(!objectIsPlayer && !Settings::bCanThrowActors) ||
					(objectIsPlayer && !Settings::bCanThrowOtherPlayers)
				)
			)
		);
		if (notTryingToThrow)
		{
			return false;
		}

		canReachTarget = true;
		startedHomingIn = false;
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		if (targetActor)
		{
			trajectoryEndPos = Util::GetTorsoPosition(targetActor) + targetLocalPosOffset;
		}
		else if (targetRefrPtrValidity)
		{
			trajectoryEndPos = Util::GetRefrPosition(targetRefrPtr.get()) + targetLocalPosOffset;
		}
		
		// Released from suspended position.
		releasePos = Util::GetRefrPosition(objectPtr.get());
		// Release velocity to set.
		releaseVelocity = RE::NiPoint3();
		// Angle straight at the initial intercept position.
		launchPitch = -Util::NormalizeAngToPi
		(
			Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos)
		);
		launchYaw = Util::NormalizeAng0To2Pi
		(
			Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos))
		);
		trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[a_p->playerID]
		);
	
		// Adjust release angle based on how long the grab bind was held for,
		// as if the bind were just released this frame.
		a_p->tm->rmm->UpdateThrownRefrReleaseAngleFactor(a_p);
		// Get HMS AVs inc per level up.
		uint32_t iAVDhmsLevelUp = 10;
		auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
		if (valueOpt.has_value())
		{
			iAVDhmsLevelUp = valueOpt.value();
		}

		// Total increase to the player's magicka so far.
		// Default to serialized data.
		float magickaTotalInc = 
		(
			glob.serializablePlayerData.at
			(
				a_p->coopActor->formID
			)->hmsPointIncreasesList[1]
		);
		// Tack on any modifiers.
		magickaTotalInc += 
		(
			a_p->coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
			) +
			a_p->coopActor->GetActorValueModifier
			(
				RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
			)
		);

		// Required total number of level ups put into magicka 
		// to get the current magicka level.
		float magickaLevelInc = magickaTotalInc / iAVDhmsLevelUp;
		// Increase max throw speed by 10% of the base throw speed per level up.
		float releaseSpeedInc = 
		(
			Settings::fBaseMaxThrownObjectReleaseSpeed * magickaLevelInc * 0.1f
		);
		// g is taken to be positive for all calculations below.
		double g = Util::GetGravitationalConstant();
		// Additional release speed multiplier if moving.
		float releaseSpeedMult = 
		(
			glob.cdh->GetInputState(a_p->deviceID, InputAction::kLS).isPressed ? 
			1.5f * magickaOverflowSlowdownFactor : 
			magickaOverflowSlowdownFactor
		);
		// Interpolate between the base throw speed and the max throw speed
		// based on how long the bind was held for.
		// Cap velocity at max refr release speed.
		const float v = Util::InterpolateSmootherStep
		(
			releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed, 
			std::clamp
			(
				(
					releaseSpeedMult * 
					(Settings::fBaseMaxThrownObjectReleaseSpeed + releaseSpeedInc)
				),
				releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed,
				Settings::fAbsoluteMaxThrownRefrReleaseSpeed 
			), 
			a_p->tm->rmm->normReleaseAngleFactor
		);

		// Once the initial release speed is set, update the intercept position, 
		// if using aim prediction.
		releaseSpeed = v;
		if (trajType == ProjectileTrajType::kPrediction)
		{
			trajectoryEndPos = CalculatePredInterceptPos(a_p);
		}
		
		// Projectile motion equation used to get max
		// and min launch angles at max launch speed,
		// and launch angle with minimized launch speed:
		// https://en.wikipedia.org/wiki/Projectile_motion#Angle_%CE%B8_required_to_hit_coordinate_(x,_y)
		// NOTE:
		// Calcs do not account for air drag.
		// When the actor is aiming at a target, holding the grab bind
		// modifies the launch angle (flatter trajectory if held longer).

		const float xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
		const float z = (trajectoryEndPos.z - releasePos.z);
		auto dirToTarget = trajectoryEndPos - releasePos;
		dirToTarget.Unitize();
		// Angle straight at the new intercept position.
		launchYaw = Util::NormalizeAng0To2Pi(atan2f(dirToTarget.y, dirToTarget.x));
		// Bounds for launch pitch.
		float steepestLaunchAng = 0.0f;
		float flattestLaunchAng = 0.0f;
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
			canReachTarget = false;
		}
		else
		{
			// Two solutions from the discriminant.
			float plusSoln = atan2f(((v * v) + sqrtf(discriminant)), (g * xy));
			float minusSoln = atan2f(((v * v) - sqrtf(discriminant)), (g * xy));
			// NOTE: 
			// Pitch convention here is the opposite of the game's:
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

			launchPitch = std::lerp
			(
				steepestLaunchAng, flattestLaunchAng, a_p->tm->rmm->normReleaseAngleFactor
			);
		}

		// New squared velocity based on the new launch pitch.
		float root = 
		(
			(g * xy * xy) / 
			(2.0f * cosf(launchPitch) * cosf(launchPitch) * (xy * tanf(launchPitch) - z))
		);
		releaseSpeed = withinRange ? sqrtf(root) : min(sqrtf(root), v);
		if (isnan(releaseSpeed) || isinf(releaseSpeed))
		{
			releaseSpeed = v;
		}

		// Components of velocity.
		float velX = releaseSpeed * cosf(launchYaw) * cosf(launchPitch);
		float velY = releaseSpeed * sinf(launchYaw) * cosf(launchPitch);
		float velZ = releaseSpeed * sinf(launchPitch);
		// XY velocity remains constant throughout since air resistance 
		// is removed before setting the object's velocity below.
		const float velXY = releaseSpeed * cosf(launchPitch);
		// Set the time to reach the target position.
		initialTimeToTarget = 
		(
			velXY == 0.0f || !withinRange ? 
			Settings::fMaxSecsBeforeClearingReleasedRefr : 
			Util::GetXYDistance(releasePos, trajectoryEndPos) / velXY
		);
		releaseVelocity = RE::NiPoint3(velX, velY, velZ);
		releaseSpeed = releaseVelocity.Length();		
		// Set as thrown.
		isThrown = true;
		// Set as our release velocity.
		lastSetVelocity = releaseVelocity;
		lastSetTargetPosition = releasePos;
		// Released now.
		releaseTP = SteadyClock::now();

		return true;
	}

	void TargetingManager::ReleasedReferenceInfo::InitTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		const float& a_normReleaseAngleFactor
	)
	{
		// Set initial release trajectory info for this released refr.
		// Determines the released refr's initial orientation, speed, time to target,
		// targeted trajectory end position, and whether or not 
		// the released refr was thrown or dropped.
		// Magicka overflow factor adjusts the release speed of the projectile.

		if (!IsValid())
		{
			Clear();
			return;
		}

		auto objectPtr = refrHandle.get();
		canReachTarget = true;
		startedHomingIn = false;
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrPtrValidity = 
		(
			targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActor = targetRefrPtrValidity ? targetRefrPtr->As<RE::Actor>() : nullptr;
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		if (targetActor)
		{
			trajectoryEndPos = Util::GetTorsoPosition(targetActor) + targetLocalPosOffset;
		}
		else if (targetRefrPtrValidity)
		{
			trajectoryEndPos = Util::GetRefrPosition(targetRefrPtr.get()) + targetLocalPosOffset;
		}

		// Released from suspended position.
		releasePos = Util::GetRefrPosition(objectPtr.get());
		// Angle straight at the initial intercept position.
		launchPitch = -Util::NormalizeAngToPi
		(
			Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos)
		);
		launchYaw = Util::NormalizeAng0To2Pi
		(
			Util::ConvertAngle(Util::GetYawBetweenPositions(releasePos, trajectoryEndPos))
		);
		trajType = static_cast<ProjectileTrajType>
		(
			Settings::vuProjectileTrajectoryType[a_p->playerID]
		);

		auto hkpRigidBodyPtr = Util::GethkpRigidBody(objectPtr.get());
		// Release velocity to apply.
		// Set to the current velocity initially.
		if (hkpRigidBodyPtr)
		{
			releaseVelocity = ToNiPoint3(hkpRigidBodyPtr->motion.linearVelocity) * HAVOK_TO_GAME;
		}
		else if (auto asProjectile = objectPtr->As<RE::Projectile>(); 
				 asProjectile && isActiveProjectile)
		{
			releaseVelocity = asProjectile->linearVelocity;
		}
		else
		{
			objectPtr->GetLinearVelocity(releaseVelocity);
		}
		
		// Throw the refr if facing the crosshair position,
		// or targeting an actor in twin-stick mode,
		// and if the thrown object is not the target refr,
		// and if it is not the player themselves (flop), unless thrown while SMORFing.
		// Only can throw living actors if the 'Can Grab Actors' setting is enabled,
		// and only can throw players if 'Can Grab Other Players' setting is enabled.
		// Drop the refr otherwise.
		bool objectIsPlayer = GlobalCoopData::IsCoopPlayer(objectPtr.get());
		bool shouldThrow = 
		(
			(a_p->tm->crosshairActive) && 
			(
				(
					(
						a_p->tm->aimMode == AimMode::kCrosshair &&
						refrHandle != a_p->tm->crosshairRefrHandle
					) ||
					(
						a_p->tm->aimMode == AimMode::kTwinStick &&
						targetActor && targetRefrHandle != refrHandle
					)
				) &&
				(
					(objectPtr != a_p->coopActor) || 
					(
						a_p->tm->isSMORFing && 
						a_p->pam->GetPlayerActionInputJustReleased(InputAction::kGrabObject, false)
					)
				)
			) &&
			(
				(!objectPtr->As<RE::Actor>()) || 
				(objectPtr->IsDead()) ||
				(!objectIsPlayer && Settings::bCanThrowActors) ||
				(objectIsPlayer && Settings::bCanThrowOtherPlayers)
			)
		);

		// REMOVE when done debugging.
		DBG
		(
			"{}: {}: should throw: {}, crosshair active: {}, is crosshair refr: {}, "
			"is player: {}, is self: {}, is SMORFing: {}, grab just released: {}, "
			"can SMORF: {}, wants to SMORF: {}.",
			a_p->coopActor->GetName(),
			objectPtr->GetName(),
			shouldThrow,
			a_p->tm->crosshairActive,
			refrHandle == a_p->tm->crosshairRefrHandle,
			objectIsPlayer,
			objectPtr == a_p->coopActor,
			a_p->tm->isSMORFing,
			a_p->pam->GetPlayerActionInputJustReleased(InputAction::kGrabObject, false),
			a_p->tm->canSMORF,
			a_p->tm->wantsToSMORF
		);
		if (shouldThrow)
		{
			auto asActor = objectPtr->As<RE::Actor>();
			// Throw refr telekinetically.
			// Zero out velocity first.
			if (!isActiveProjectile && hkpRigidBodyPtr)
			{
				Util::NativeFunctions::hkpEntity_Activate(hkpRigidBodyPtr.get());
				hkpRigidBodyPtr->motion.SetPosition(releasePos * GAME_TO_HAVOK);
				hkpRigidBodyPtr->motion.SetLinearVelocity({ 0 });
				hkpRigidBodyPtr->motion.SetAngularVelocity({ 0 });
			}
			
			// Get HMS AVs inc per level up.
			uint32_t iAVDhmsLevelUp = 10;
			auto valueOpt = Util::GetGameSettingInt("iAVDhmsLevelUp");
			if (valueOpt.has_value())
			{
				iAVDhmsLevelUp = valueOpt.value();
			}

			// Total increase to the player's magicka so far.
			// Default to serialized data.
			float magickaTotalInc = 
			(
				glob.serializablePlayerData.at
				(
					a_p->coopActor->formID
				)->hmsPointIncreasesList[1]
			);
			// Tack on any modifiers.
			magickaTotalInc += 
			(
				a_p->coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kMagicka
				) +
				a_p->coopActor->GetActorValueModifier
				(
					RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMagicka
				)
			);
			// Required total number of level ups put into magicka 
			// to get the current magicka level.
			float magickaLevelInc = magickaTotalInc / iAVDhmsLevelUp;
			// Increase max throw speed by 10% of the base throw speed per level up.
			float releaseSpeedInc = 
			(
				Settings::fBaseMaxThrownObjectReleaseSpeed * magickaLevelInc * 0.1f
			);
			// g is taken to be positive for all calculations below.
			double g = Util::GetGravitationalConstant();
			// Additional release speed multiplier if moving.
			float releaseSpeedMult = 
			(
				glob.cdh->GetInputState(a_p->deviceID, InputAction::kLS).isPressed ? 
				1.5f * magickaOverflowSlowdownFactor : 
				magickaOverflowSlowdownFactor
			);
			// Update normalized release angle factor.
			a_p->tm->rmm->UpdateThrownRefrReleaseAngleFactor(a_p, a_normReleaseAngleFactor);
			// Interpolate between the base throw speed and the max throw speed
			// based on how long the bind was held for.
			// Cap velocity at max refr release speed.
			const float v = Util::InterpolateSmootherStep
			(
				releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed, 
				std::clamp
				(
					(
						releaseSpeedMult * 
						(Settings::fBaseMaxThrownObjectReleaseSpeed + releaseSpeedInc)
					),
					releaseSpeedMult * Settings::fBaseMaxThrownObjectReleaseSpeed,
					Settings::fAbsoluteMaxThrownRefrReleaseSpeed 
				), 
				a_p->tm->rmm->normReleaseAngleFactor
			);
			// Once release speed is set, update the intercept position, if using aim prediction.
			releaseSpeed = v;
			if (trajType == ProjectileTrajType::kPrediction)
			{
				trajectoryEndPos = CalculatePredInterceptPos(a_p);
			}
			
			// Projectile motion equation used to get max 
			// and min launch angles at max launch speed,
			// and launch angle with minimized launch speed:
			// https://en.wikipedia.org/wiki/Projectile_motion#Angle_%CE%B8_required_to_hit_coordinate_(x,_y)
			// NOTE: 
			// Calcs do not account for air drag.
			// When the actor is aiming at a target, holding the grab bind
			// modifies the launch angle (flatter trajectory if held longer).

			const float xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			const float z = (trajectoryEndPos.z - releasePos.z);
			auto dirToTarget = trajectoryEndPos - releasePos;
			dirToTarget.Unitize();
			// Angle straight at target.
			launchYaw = Util::NormalizeAng0To2Pi(atan2f(dirToTarget.y, dirToTarget.x));
			// Bounds for launch pitch.
			float steepestLaunchAng = 0.0f;
			float flattestLaunchAng = 0.0f;
			// Only can hit the target with the calculated velocity
			// if the discriminant is positive.
			float discriminant = (v * v * v * v) - ((g * g * xy * xy) + (2 * g * z * v * v));
			bool withinRange = discriminant >= 0;
			if (withinRange)
			{
				// Two solutions from the discriminant.
				float plusSoln = atan2f(((v * v) + sqrtf(discriminant)), (g * xy));
				float minusSoln = atan2f(((v * v) - sqrtf(discriminant)), (g * xy));

				// NOTE:
				// Pitch convention here is the opposite of the game's:
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

				launchPitch = std::lerp
				(
					steepestLaunchAng, flattestLaunchAng, a_p->tm->rmm->normReleaseAngleFactor
				);
			}
			else
			{
				// Get max range launch angle when launched at max speed.
				// Pitch from release position to end position.
				float alpha = atanf(z / xy);
				// Halfway between the pitch between release and end positions
				// and the fully vertical pitch of 90 degrees.
				launchPitch = (PI / 2.0f) - (0.5f * (PI / 2.0f - alpha));
				canReachTarget = false;
			}

			// New squared velocity based on the new launch pitch.
			float root = 
			(
				(g * xy * xy) / 
				(2.0f * cosf(launchPitch) * cosf(launchPitch) * (xy * tanf(launchPitch) - z))
			);
			if (root >= 0)
			{
				releaseSpeed = withinRange ? sqrtf(root) : min(sqrtf(root), v);
			}
			else
			{
				releaseSpeed = v;
			}
			
			if (isnan(releaseSpeed) || isinf(releaseSpeed))
			{
				releaseSpeed = v;
			}

			// Components of velocity.
			float velX = releaseSpeed * cosf(launchYaw) * cosf(launchPitch);
			float velY = releaseSpeed * sinf(launchYaw) * cosf(launchPitch);
			float velZ = releaseSpeed * sinf(launchPitch);
			// XY velocity remains constant throughout since air resistance 
			// is removed before setting the object's velocity below.
			const float velXY = releaseSpeed * cosf(launchPitch);
			// Set the time to reach the target position.
			initialTimeToTarget = 
			(
				velXY == 0.0f || !withinRange ? 
				Settings::fMaxSecsBeforeClearingReleasedRefr : 
				Util::GetXYDistance(releasePos, trajectoryEndPos) / velXY
			);
			releaseVelocity = RE::NiPoint3(velX, velY, velZ);
			releaseSpeed = releaseVelocity.Length();
			if (hkpRigidBodyPtr)
			{
				// Activate the refr and set the computed velocity.
				// Without activation, the object will not always move to our target position
				// and a discrepancy between the havok rigid body and node positions may develop.
				// A discrepancy between the refr data reported position and the refr's 3D position
				// can also stall the object in the air without activation first.
				Util::NativeFunctions::hkpEntity_Activate(hkpRigidBodyPtr.get());
				hkpRigidBodyPtr->motion.SetLinearVelocity(releaseVelocity * GAME_TO_HAVOK);
			}
			
			// Set as thrown.
			isThrown = true;
		}
		else
		{
			// Drop object.
			isThrown = false;
		}

		// Set as our release velocity.
		lastSetVelocity = releaseVelocity;
		lastSetTargetPosition = releasePos;
		// Set release height, which will change when the refr 
		// reaches the apex of the trajectory.
		fallHeight = Util::Get3DCenterPos(objectPtr.get()).z;
		// Released now.
		releaseTP = SteadyClock::now();
	}

	int32_t TargetingManager::RefrManipulationManager::AddGrabbedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, const RE::ObjectRefHandle& a_handle
	)
	{
		// Add the given refr to the managed grabbed refrs data set,
		// set its grab time point, and ragdoll the refr, if it is an actor,
		// to allow for positional manipulation.
		// Returns the next open index in the grabbed refr list
		// at which the requested refr was inserted, 
		// or -1 if the requested refr could not be grabbed.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr)
		{
			return -1;
		}

		// Must have space for another grabbed refr and not already grabbed.
		int32_t nextOpenIndex = grabbedRefrInfoList.size();
		if (nextOpenIndex >= Settings::uMaxGrabbedReferences ||
			grabbedRefrHandlesToInfoIndices.contains(a_handle))
		{
			return -1;
		}

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
		// Save the original collision layer right after grabbing.
		info->SaveCollisionLayer();
		// Ragdoll actor if necesssary to allow manipulation.
		if (auto asActor = objectPtr->As<RE::Actor>(); asActor)
		{
			// If the actor to ragdoll is this player, push upward slightly.
			// Otherwise, knock down.
			if (asActor == a_p->coopActor.get()) 
			{
				RE::NiPoint3 forceOrigin = a_p->mm->playerTorsoPosition;
				// Adjust the force application point to allow the player
				// to either gain a bit more air when flopping while looking up,
				// or body slam with mean intentions when flopping while looking down.
				if (Settings::bAimPitchAffectsFlopTrajectory)
				{
					float forceOriginHeightOffset = 
					(
						a_p->coopActor->GetHeight() * 
						std::lerp(-0.5f, 0.5f, (a_p->mm->aimPitch + PI / 2.0f) / PI)
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

		// Set active projectile flag.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		info->isActiveProjectile = 
		(
			asProjectile && !asProjectile->ShouldBeLimited()
		);

		// Match collision state with the first grabbed refr.
		if (nextOpenIndex != 0 && grabbedRefrInfoList[0] && !grabbedRefrInfoList[0]->hasCollision)
		{
			info->ToggleCollision();
		}

		return nextOpenIndex;
	}

	int32_t TargetingManager::RefrManipulationManager::AddReleasedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::ObjectRefHandle& a_handle,
		float a_magickaCost,
		float a_normReleaseAngleFactor
	) 
	{
		// Add the given refr to the managed released refrs data set.
		// Also set its release trajectory information (dropped or thrown)
		// to use when guiding its motion upon release.
		// The given magicka cost will modify its release speed, 
		// depending on how much magicka the player has left.
		// Returns the next open index in the released refr list
		// at which the requested refr was inserted, 
		// or -1 if the requested refr could not be released.

		auto objectPtr = Util::GetRefrPtrFromHandle(a_handle); 
		if (!objectPtr)
		{
			return -1;
		}

		// Must not have been released already.
		if (releasedRefrHandlesToInfoIndices.contains(a_handle))
		{
			return -1;
		}
		
		int32_t nextOpenIndex = releasedRefrInfoList.size();
		// Store mapped index and then add to list.
		releasedRefrHandlesToInfoIndices.insert({ a_handle, nextOpenIndex });
		releasedRefrInfoList.emplace_back
		(
			std::make_unique<ReleasedReferenceInfo>(a_handle)
		);
		const auto& info = releasedRefrInfoList[nextOpenIndex];
		// Set active projectile flag.
		auto asProjectile = objectPtr->As<RE::Projectile>();
		info->isActiveProjectile = 
		(
			asProjectile && !asProjectile->ShouldBeLimited()
		);
		// Set the magicka overflow factor using the magicka cost
		// before initializing the trajectory.
		// Modify by the player's magicka cost multiplier 
		// to reflect the true cost applied through the CheckClampDamageModifier() hook.
		info->magickaOverflowSlowdownFactor = GetThrownRefrMagickaOverflowSlowdownFactor
		(
			a_p, a_magickaCost * Settings::vfMagickaCostMult[a_p->playerID]
		);
		// Set initial homing/aim prediction trajectory info.
		info->InitTrajectory(a_p, a_normReleaseAngleFactor);

		return nextOpenIndex;
	}

	bool TargetingManager::RefrManipulationManager::CanGrabAnotherRefr()
	{
		// Return true if the number of managed grabbed refrs 
		// is less than the maximum allowable number of grabbed refrs.

		return grabbedRefrInfoList.size() < Settings::uMaxGrabbedReferences;
	}

	bool TargetingManager::RefrManipulationManager::CanGrabRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Return true if the given refr is valid, not already managed,
		// and there's room for another grabbed refr.

		return Util::HandleIsValid(a_handle) && !IsManaged(a_handle, true) && CanGrabAnotherRefr();
	}

	bool TargetingManager::RefrManipulationManager::CanManipulateGrabbedRefr
	(
		const std::shared_ptr<CoopPlayer>& a_p, const uint8_t& a_index
	)
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

		// Must still be limited if originally added as an active projectile.
		if (info->isActiveProjectile)
		{
			auto asProj = objectPtr->As<RE::Projectile>();
			if (asProj && asProj->ShouldBeLimited())
			{
				info->Clear();
				return false;
			}
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
		// a certain period of inactivity.
		// Check if the grabbed actor is no longer ragdolling 
		// and either reset this player's grab-related state (if grabbing a player),
		// or attempt to re-grab the non-player actor.
		// Haven't found a hook to prevent the ragdoll timer from being set to 0 yet,
		// so chalk this solution up to more jank.
		if (asActor && !asActor->IsDead() && !asActor->IsInRagdollState())
		{
			if (GlobalCoopData::IsCoopPlayer(asActor)) 
			{
				// Player is not grabbed anymore.
				info->Clear();
				asActor->PotentiallyFixRagdollState();
				return false;
			}
			else
			{
				if (Settings::bRemoveGrabbedActorAutoGetUp && 
					asActor->currentProcess &&
					asActor->currentProcess->middleHigh)
				{
					// Paralyze living actors to prevent them from getting up constantly.
					if (asActor->boolBits.none(RE::Actor::BOOL_BITS::kParalyzed))
					{
						asActor->boolBits.set(RE::Actor::BOOL_BITS::kParalyzed);
					}

					// Knock 'em down again.
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
		
		{
			std::unique_lock<std::mutex> lock(contactEventsQueueMutex, std::try_to_lock);
			if (lock)
			{
				DBG
				(
					"Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				collidedRefrFIDPairs.clear();
				queuedReleasedRefrContactEvents.clear();
			}
			else
			{
				DBG
				(
					"Failed to obtain lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
			}
		}

		ClearGrabbedRefrs();
		ClearReleasedRefrs();
		// No longer grabbing.
		initialGrabCheckPerformed = isAutoGrabbing = isGrabbing = false;
		reqSpecialHitDamageAmount = 0.0f;
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedActors
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Clear all grabbed actors and refresh mappings if any were cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList,
			[&](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{
				auto refrPtr = Util::GetRefrPtrFromHandle(a_info->refrHandle);
				// Invalid or not an actor so do not clear.
				if (!refrPtr || !refrPtr->As<RE::Actor>())
				{
					return false;
				}
				
				// Restore cached collision layer before clearing.
				a_info->RestoreSavedCollisionLayer();
				auto asActor = refrPtr->As<RE::Actor>();
				if (asActor)
				{
					// Ensure actors are no longer paralyzed, unless the actor is a downed player.
					const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
					if ((pIndex == -1) || 
						(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
					{
						// No longer paralyzed + signal to get up.
						asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
						if (!asActor->IsDead()) 
						{
							asActor->NotifyAnimationGraph("GetUpBegin");
						}
					}
				}

				return true;
			}
		);

		if (numErased != 0)
		{
			RefreshHandleToIndexMappings(true);
		}
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Clear the given refr from the grabbed list.
		// Refresh handle-to-index mappings if the refr was cleared.

		size_t numErased = std::erase_if
		(
			grabbedRefrInfoList,
			[&a_handle](const std::unique_ptr<GrabbedReferenceInfo>& a_info) 
			{
				if (a_info->refrHandle == a_handle)
				{
					// Restore cached collision layer before clearing.
					a_info->RestoreSavedCollisionLayer();
					return true;
				}

				return false;
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
				// Ensure actors are no longer paralyzed, unless the actor is a downed player.
				if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
				{
					const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
					if ((pIndex == -1) || 
						(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
					{
						asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					}
				}
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearGrabbedRefrs() noexcept
	{
		// Clear all managed grabbed refrs + their cached data.

		for (const auto& info : grabbedRefrInfoList)
		{
			// Reset collision layer, just in case collisions were disabled.
			info->RestoreSavedCollisionLayer();
			const auto& handle = info->refrHandle;
			if (auto refrPtr = Util::GetRefrPtrFromHandle(handle); refrPtr)
			{
				// Ensure actors are no longer paralyzed, unless the actor is a downed player.
				if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
				{
					const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
					if ((pIndex == -1) || 
						(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
					{
						asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					}
				}
			}
		}

		grabbedRefrInfoList.clear();
		grabbedRefrHandlesToInfoIndices.clear();
	}

	void TargetingManager::RefrManipulationManager::ClearInactiveReleasedRefrs()
	{
		// Clear all managed released refrs that have no recorded release time point.
		// Refresh handle-to-index mappings if at least one released refr was cleared.

		auto numErased = std::erase_if
		(
			releasedRefrInfoList, 
			[this](const std::unique_ptr<ReleasedReferenceInfo>& a_info)
			{
				if (!a_info->releaseTP.has_value()) 
				{
					const auto& handle = a_info->refrHandle;
					auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
					if (refrPtr)
					{
						// Ensure actors are no longer paralyzed, 
						// unless the actor is a downed player.
						if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
						{
							const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
							if ((pIndex == -1) || 
								(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
							{
								asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
							}
						}
					}
				}

				return !a_info->releaseTP.has_value();
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
					if (!a_info->IsValid())
					{
						// Reset collision layer before clearing.
						a_info->RestoreSavedCollisionLayer();
						return true;
					}

					return false;
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

	void TargetingManager::RefrManipulationManager::ClearPlayerIfGrabbed
	(
		const std::shared_ptr<CoopPlayer>& a_p)
	{
		// If the given player is grabbed by another active player,
		// have the grabbing player release this player.

		const auto handle = a_p->coopActor->GetHandle();
		for (const auto& otherP : glob.coopPlayers)
		{
			if (!otherP->isActive || otherP == a_p)
			{
				continue;
			}

			if (otherP->tm->rmm->IsManaged(handle, true))
			{
				otherP->tm->rmm->ClearRefr(handle);
				// Ensure the player is no longer paralyzed, unless they are downed.
				if (!a_p->isDowned && !glob.partyWiped)
				{
					a_p->coopActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
					a_p->coopActor->NotifyAnimationGraph("GetUpBegin");
				}

				break;
			}
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
				if (a_info->refrHandle == a_handle)
				{
					// Restore original collision layer before clearing.
					a_info->RestoreSavedCollisionLayer();
					return true;
				}

				return false;
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
			// Ensure actors are no longer paralyzed, unless the actor is a downed player.
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
				if ((pIndex == -1) || 
					(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
				{
					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearReleasedRefr
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Clear the given refr from the released list.
		// Refresh handle-to-index mappings if the refr was cleared.
		
		size_t numErased = std::erase_if
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
			// Ensure actors are no longer paralyzed, unless the actor is a downed player.
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
				if ((pIndex == -1) || 
					(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
				{
					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}
		}
	}

	void TargetingManager::RefrManipulationManager::ClearReleasedRefrs() noexcept
	{
		// Clear all managed released refrs + their cached data.

		for (const auto& info : releasedRefrInfoList)
		{
			const auto& handle = info->refrHandle;
			auto refrPtr = Util::GetRefrPtrFromHandle(handle); 
			if (!refrPtr)
			{
				continue;
			}

			// Ensure actors are no longer paralyzed, unless the actor is a downed player.
			if (auto asActor = refrPtr->As<RE::Actor>(); asActor)
			{
				const auto pIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
				if ((pIndex == -1) || 
					(!glob.coopPlayers[pIndex]->isDowned && !glob.partyWiped))
				{
					asActor->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
				}
			}
		}

		releasedRefrInfoList.clear();
		releasedRefrHandlesToInfoIndices.clear();
	}

	bool TargetingManager::RefrManipulationManager::GetRefrInfoIndex
	(
		const RE::ObjectRefHandle& a_handle, bool a_grabbed, uint8_t& a_indexOut
	)
	{
		// Get the given refr handle's manipulable refr info index 
		// that gives its position in the grabbed/released refrs list 
		// and return it through the outparam.
		// Return true if the refr corresponding to the given handle 
		// is a managed grabbed/released object.

		if (!Util::HandleIsValid(a_handle))
		{
			return false;
		}

		// Must have a mapped index that is within the bounds of the corresponding info list.
		if (a_grabbed)
		{
			const auto iter = grabbedRefrHandlesToInfoIndices.find(a_handle);
			if (iter != grabbedRefrHandlesToInfoIndices.end() && 
				iter->second < grabbedRefrInfoList.size())
			{
				a_indexOut = iter->second;
				return true;
			}
		}
		else
		{
			const auto iter = releasedRefrHandlesToInfoIndices.find(a_handle);
			if (iter != releasedRefrHandlesToInfoIndices.end() && 
				iter->second < releasedRefrInfoList.size())
			{
				a_indexOut = iter->second;
				return true;
			}
		}

		return false;
	}

	float TargetingManager::RefrManipulationManager::GetThrownRefrMagickaCost
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		RE::TESObjectREFR* a_refrToThrow,
		const float& a_normReleaseAngleFactor
	)
	{
		// Return the base magicka cost for throwing the given refr.
		// If not specifying a release angle factor (or set to -1),
		// calculate the release angle factor based on the 'Grab Object' bind's hold time.

		if (a_p->isInGodMode || !a_refrToThrow)
		{
			return 0.0f;
		}

		UpdateThrownRefrReleaseAngleFactor(a_p, a_normReleaseAngleFactor);
		float objectWeight = max(0.0f, a_refrToThrow->GetWeight()) + 0.1f;
		auto asActor = a_refrToThrow->As<RE::Actor>();
		if (asActor)
		{
			// Weights can sometimes be -1, so ensure the weight is at least 0.
			const auto releasedPIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
			float inventoryWeight = max
			( 
				0.0f,
				releasedPIndex <= 0 ?
				asActor->GetWeightInContainer() :
				glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetWeightInContainer()
			);
			const auto invChanges = 
			(
				releasedPIndex <= 0 ?
				asActor->GetInventoryChanges() :
				glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetInventoryChanges()
			);
			if (invChanges)
			{
				inventoryWeight = invChanges->totalWeight;
			}

			objectWeight = objectWeight + inventoryWeight;
			return
			(
				(4.0f * sqrtf(objectWeight)) * 
				(0.5f * normReleaseAngleFactor + 0.5f) *
				(Settings::vfMagickaCostMult[a_p->playerID]) *
				(Settings::vfObjectManipulationMagickaCostMult[a_p->playerID])
			);
		}
		else
		{
			objectWeight = objectWeight + max(0.0f, a_refrToThrow->GetWeightInContainer());
			return
			(
				(4.0f * sqrtf(objectWeight)) * 
				(0.5f * normReleaseAngleFactor + 0.5f) *
				(Settings::vfMagickaCostMult[a_p->playerID]) *
				(Settings::vfObjectManipulationMagickaCostMult[a_p->playerID])
			);
		}
	}

	float TargetingManager::RefrManipulationManager::GetThrownRefrMagickaOverflowSlowdownFactor
	(
		const std::shared_ptr<CoopPlayer>& a_p, const float& a_trueMagickaCost
	)
	{
		// Factor by which to slow down all thrown refrs' release speeds.
		// The more magicka used up below a magicka level of 0, 
		// the smaller the factor and the slower the release speed.

		if (a_p->isInGodMode)
		{
			return 1.0f;
		}

		const float maxMagicka = Util::GetFullAVAmount
		(
			a_p->coopActor.get(), RE::ActorValue::kMagicka
		);
		const float currentMagicka = a_p->coopActor->GetActorValue(RE::ActorValue::kMagicka);
		// Throw speed slowdown factor based on how much extra magicka
		// the player would need to throw their grabbed refrs.
		// If the cost is larger than the player's remaining magicka,
		// they will throw all refrs at a reduced speed, 
		// as low as 25 percent of the original speed.
		return
		(
			std::clamp
			(
				1.0f - max(0.0f, (a_trueMagickaCost - currentMagicka) / maxMagicka),
				0.25f,
				1.0f
			)
		);
	}

	void TargetingManager::RefrManipulationManager::HandleQueuedContactEvents
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Ragdoll and apply damage to any hit actors from the contact events queue.

		// No released refrs, so no contact events to handle.
		if (a_p->tm->rmm->releasedRefrInfoList.empty())
		{
			return;
		}

		{
			std::unique_lock<std::mutex> lock
			(
				a_p->tm->rmm->contactEventsQueueMutex, std::try_to_lock
			);
			if (!lock)
			{
				return;
			}
			
			const auto& releasedRefrIndicesMap = 
			(
				a_p->tm->rmm->releasedRefrHandlesToInfoIndices
			);
			// Must obtain the point of contact between two collidables,
			// then get their handles and the associated refrs.
			RE::NiPoint3 contactPoint{ };
			RE::TESObjectREFRPtr refrPtrA = nullptr;
			RE::TESObjectREFRPtr refrPtrB = nullptr;
			// Unmanaged refr that the other managed refr collided with/was hit by.
			RE::TESObjectREFRPtr collidedWithRefrPtr = nullptr;
			// Rigid bodies for the hit refr and released refr, if any.
			RE::hkRefPtr<RE::hkpRigidBody> hitRigidBodyPtr{ nullptr };
			RE::hkRefPtr<RE::hkpRigidBody> releasedRigidBodyPtr{ nullptr };
			// Flopped-on refrs to add as released to propagate the initial collision.
			// Pairs of (refr handle, released angle factor).
			std::vector<std::pair<RE::ObjectRefHandle, float>> flopRedirectedRefrs{ };
			// Movin' through the queue.
			for (auto iter = queuedReleasedRefrContactEvents.begin(); 
				 iter != queuedReleasedRefrContactEvents.end(); 
				 ++iter)
			{
				const auto& contactEvent = *iter;
				// Must have two colliding bodies.
				if (!contactEvent->rigidBodyA || !contactEvent->rigidBodyB)
				{
					continue;
				}

				refrPtrA = Util::GetRefrPtrFromHandle(contactEvent->handleA);
				refrPtrB = Util::GetRefrPtrFromHandle(contactEvent->handleB);
				// SPECIAL CASE:
				// If one refr is invalid, it means a thrown refr collided with an object
				// that has no associated refr, such as a terrain block,
				// so we have to record the hit and potentially handle the splat and cleanup.
				if (!refrPtrA || !refrPtrB) 
				{
					auto releasedRefrPtr =
					(
						refrPtrA ?
						refrPtrA :
						refrPtrB ?
						refrPtrB :
						nullptr
					);
					if (!releasedRefrPtr)
					{
						continue;
					}

					auto releasedRefrHandle = releasedRefrPtr->GetHandle();
					if (!Util::HandleIsValid(releasedRefrHandle))
					{
						continue;
					}

					const auto iter = releasedRefrIndicesMap.find(releasedRefrHandle);
					// Not released by this player, no need to handle.
					if (iter == releasedRefrIndicesMap.end())
					{
						continue;
					}

					const auto index = iter->second;
					const auto& releasedRefrInfo = a_p->tm->rmm->releasedRefrInfoList[index];

					// Set first hit, if necessary.
					// Ignore hits within 30 frames/0.5s of release to allow the released refr
					// to get off the ground and start on its trajectory if it hasn't already.
					// This applies to heavy or not very aerodynamic objects/actors,
					// such as fish, rabbits, crabs, and dragons.
					// Can't set through the AddHitRefr() func,
					// since terrain does not have an FID to store.
					if (releasedRefrInfo->SetupPeriodElapsed())
					{
						// Set first hit TP if not set already.
						if (!releasedRefrInfo->firstHitTP.has_value())
						{
							releasedRefrInfo->firstHitTP = SteadyClock::now();
						}

						// Increment the collision count, since we're past the setup period.
						releasedRefrInfo->postSetupHitsCount++;
					}
					
					// Increment total hits count.
					releasedRefrInfo->totalHitsCount++;
					// Do not apply splat damage if this refr is not an actor 
					// or is a not flopping player and is not a thrown.
					auto releasedActor = releasedRefrPtr->As<RE::Actor>();
					if (!releasedActor)
					{
						// Update fall height to the actor's position
						// even when skipping the collision.
						releasedRefrInfo->fallHeight = 
						(
							Util::Get3DCenterPos(releasedRefrPtr.get()).z
						);
						continue;
					}
						
					auto releasedActorHandle = releasedActor->GetHandle();
					// Hit 3D object without an associated refr.
					// eg. Navmesh or terrain block.
					a_p->tm->HandleSplat
					(
						releasedActorHandle, 
						ToNiPoint3(contactEvent->contactNormal),
						max(1, releasedRefrInfo->totalHitsCount),
						releasedRefrInfo->fallHeight,
						releasedRefrInfo->isThrown
					);
					// Update fall height to the actor's position after handling the collision.
					releasedRefrInfo->fallHeight = Util::Get3DCenterPos
					(
						releasedRefrPtr.get()
					).z;

					// We're done here.
					continue;
				}
				
				// Have to have two valid handles to handle collisions below.
				if (!Util::HandleIsValid(contactEvent->handleA) || 
					!Util::HandleIsValid(contactEvent->handleB))
				{
					continue;
				}

				// Check for instances where one of the two colliding refrs
				// is managed and the other is not.
				// Want to ignore collisions between non-managed refrs
				// and between two managed refrs.
				int32_t collidingReleasedRefrIndex = -1;
				const auto iterA = releasedRefrIndicesMap.find(contactEvent->handleA);
				const auto iterB = releasedRefrIndicesMap.find(contactEvent->handleB);
				if (iterA != releasedRefrIndicesMap.end() && iterB == releasedRefrIndicesMap.end())
				{
					collidedWithRefrPtr = refrPtrB;
					collidingReleasedRefrIndex = iterA->second;
					hitRigidBodyPtr = contactEvent->rigidBodyB;
					releasedRigidBodyPtr = contactEvent->rigidBodyA;
				}

				if (iterB != releasedRefrIndicesMap.end() && iterA == releasedRefrIndicesMap.end())
				{
					collidedWithRefrPtr = refrPtrA;
					collidingReleasedRefrIndex = iterB->second;
					hitRigidBodyPtr = contactEvent->rigidBodyA;
					releasedRigidBodyPtr = contactEvent->rigidBodyB;
				}
				
				// No released refr rigid body.
				if (!releasedRigidBodyPtr)
				{
					continue;
				}
				
				// No index for the managed refr.
				if (collidingReleasedRefrIndex == -1)
				{
					continue;
				}

				// Why are you hitting yourself? Eh, whatever. Next!
				if (!collidedWithRefrPtr || collidedWithRefrPtr == a_p->coopActor)
				{
					continue;
				}

				// Ignore refrs without collision, such as activators.
				bool hasCollidable = hitRigidBodyPtr && hitRigidBodyPtr->GetCollidable();
				if (!hasCollidable)
				{
					continue;
				}

				// Do not handle if the hit refr is a player that is dash dodging
				// or flailing their arms in a crazed and/or defensive manner.
				// Either method still requires timing using the dash dodge's I-Frame window.
				auto hitPlayerIndex = GlobalCoopData::GetCoopPlayerIndex(collidedWithRefrPtr);
				if (hitPlayerIndex != -1)
				{
					const auto& hitP = glob.coopPlayers[hitPlayerIndex];
					// Number of seconds independent of framerate.
					float secsEvadeWindow = 
					(
						(
							Settings::uDashDodgeBaseAnimFrameCount + 
							Settings::uDashDodgeSetupFrameCount
						) *
						(1.0f / (*g_deltaTimeRealTime * 60.0f))
					);
					bool canEvade = 
					(
						(hitP->mm->isDashDodging) || 
						(
							hitP->pam->IsPerforming(InputAction::kRotateLeftShoulder) &&
							hitP->pam->GetSecondsSinceLastStart(InputAction::kRotateLeftShoulder) <
							secsEvadeWindow 
						) ||
						(
							hitP->pam->IsPerforming(InputAction::kRotateRightShoulder) &&
							hitP->pam->GetSecondsSinceLastStart
							(
								InputAction::kRotateRightShoulder
							) < secsEvadeWindow 
						)
					);
					if (canEvade)
					{
						// Clear the released refr, so we don't continue 
						// setting its trajectory or listening for collisions.
						// Otherwise, if it is homing in on the target,
						// it'll go through the player, come back around,
						// and hit the player once their dodge I-frames end
						// (or once their arms grow heavy with fatigue and stop moving).
						ClearRefr(collidedWithRefrPtr->GetHandle());
						// NOTE:
						// Unfortunately, at this stage, the collision has already occurred,
						// so we can only prevent damage application to the dodging player
						// by ignoring this event.
						// TODO:
						// Figure out how to register a pre-collision callback 
						// or find a similar place to hook in order to
						// filter out certain collisions before they occur.
						continue;
					}
				}
				
				// Get released refr info now that the rigid body is valid.
				const auto& releasedRefrInfo =
				(
					a_p->tm->rmm->releasedRefrInfoList[collidingReleasedRefrIndex]
				);
				auto releasedRefrPtr = Util::GetRefrPtrFromHandle
				(
					releasedRefrInfo->refrHandle
				);
				// Don't want repeated hits.
				bool hasAlreadyHitRefr = releasedRefrInfo->HasAlreadyHitRefr
				(
					collidedWithRefrPtr.get()
				);
				// Ignore collisions between managed released refrs.
				if (a_p->tm->rmm->IsManaged(collidedWithRefrPtr->GetHandle(), false))
				{
					continue;
				}
				
				// Ignore active projectile collisions since the game already handles them for us.
				if (releasedRefrInfo->isActiveProjectile)
				{
					continue;
				}
				
				// Add hit.
				releasedRefrInfo->AddHitRefr(collidedWithRefrPtr.get());
				auto hitActor = collidedWithRefrPtr->As<RE::Actor>(); 
				// Managed refr hit a new actor that isn't itself. Bonk.
				bool shouldBonk = 
				(
					hitActor && 
					hitActor->currentProcess && 
					releasedRefrPtr != collidedWithRefrPtr && 
					!hasAlreadyHitRefr	
				);
				if (shouldBonk)
				{
					a_p->tm->HandleBonk
					(
						hitActor->GetHandle(), 
						releasedRefrPtr->GetHandle(),
						releasedRigidBodyPtr->motion.GetMass(),
						releasedRefrInfo->fallHeight,
						ToNiPoint3(releasedRigidBodyPtr->motion.linearVelocity * HAVOK_TO_GAME),
						ToNiPoint3(contactEvent->contactPosition * HAVOK_TO_GAME)
					);
				}

				// Heh.
				// Works the same way as slapping the object to redirect it.
				const auto collidedWithRefrHandle = collidedWithRefrPtr->GetHandle();
				bool shouldRedirectWithFlop = 
				(
					(
						releasedRefrPtr == a_p->coopActor &&
						collidedWithRefrPtr != a_p->coopActor &&
						collidedWithRefrHandle != a_p->tm->crosshairRefrHandle &&
						collidedWithRefrHandle != a_p->tm->aimCorrectionTargetHandle
					) &&
					(
						hitActor || 
						Util::IsLootableRefr(collidedWithRefrPtr.get())
					)
				);
				if (shouldRedirectWithFlop) 
				{
					flopRedirectedRefrs.emplace_back
					(
						collidedWithRefrPtr->GetHandle(),
						std::lerp
						(
							0.5f,
							0.85f,
							min
							(
								1.0f, releasedRigidBodyPtr->motion.linearVelocity.Length3() / 15.0f
							)
						)
					);
				}

				// Thrown actor hit a new refr that isn't itself. Splat.
				auto thrownActor = releasedRefrPtr->As<RE::Actor>(); 
				bool canSplat = 
				(
					thrownActor && 
					thrownActor != collidedWithRefrPtr.get()
				);
				if (canSplat)
				{
					a_p->tm->HandleSplat
					(
						thrownActor->GetHandle(), 
						ToNiPoint3(contactEvent->contactNormal),
						max(1, releasedRefrInfo->totalHitsCount),
						releasedRefrInfo->fallHeight,
						releasedRefrInfo->isThrown
					);
				}

				if (canSplat || shouldBonk)
				{
					// Update fall height to the refr's position after handling the collision.
					releasedRefrInfo->fallHeight = Util::Get3DCenterPos(releasedRefrPtr.get()).z;
				}

				// Only damage destructible objects on the first hit.
				if (!hasAlreadyHitRefr)
				{
					// Get havok collision speed.
					float havokHitSpeed = contactEvent->contactSpeed;
					if (havokHitSpeed == 0.0f)
					{
						auto refr3DPtr = Util::GetRefr3D(releasedRefrPtr.get());
						if (refr3DPtr)
						{
							havokHitSpeed = Util::GetParentNodeHavokPointVelocity
							(
								refr3DPtr.get(), contactEvent->contactPosition
							).Length3();
						}
						else
						{
							havokHitSpeed = 
							(
								releasedRigidBodyPtr->motion.linearVelocity.Length3()
							);
						}
					}

					// Too slow to cause destructible object damage.
					if (havokHitSpeed < 1E-5f)
					{
						continue;
					}
				
					// Damage destructible objects.
					auto taskInterface = RE::TaskQueueInterface::GetSingleton(); 
					if (!taskInterface)
					{
						continue;
					}

					if (!releasedRefrPtr->Is(RE::FormType::ActorCharacter))
					{
						taskInterface->QueueUpdateDestructibleObject
						(
							releasedRefrPtr.get(),
							max(releasedRefrPtr->GetWeight(), 0.0f) * havokHitSpeed, 
							false,
							a_p->coopActor.get()
						);
					}
						
					if (!collidedWithRefrPtr->Is(RE::FormType::ActorCharacter))
					{
						taskInterface->QueueUpdateDestructibleObject
						(
							collidedWithRefrPtr.get(),
							max(releasedRefrPtr->GetWeight(), 0.0f) * havokHitSpeed,
							false, 
							a_p->coopActor.get()
						);
					}
				}
			}

			// Add any flop bonk'd refrs as released.
			for (const auto& [handle, factor] : flopRedirectedRefrs)
			{
				if (!Util::HandleIsValid(handle))
				{
					continue;
				}

				for (const auto& otherP : glob.coopPlayers)
				{
					if (!otherP->isActive || otherP == a_p)
					{
						continue;
					}

					// Remove grabbed/released refr from the other player's managed lists.
					if (otherP->tm->rmm->IsManaged(handle, true) || 
						otherP->tm->rmm->IsManaged(handle, false))
					{
						otherP->tm->rmm->ClearRefr(handle);
					}
				}

				a_p->tm->rmm->AddGrabbedRefr(a_p, handle);
				a_p->tm->rmm->ClearGrabbedRefr(handle);
				if (a_p->tm->rmm->GetNumGrabbedRefrs() == 0)
				{
					a_p->tm->SetIsGrabbing(false);
				}

				a_p->tm->rmm->AddReleasedRefr(a_p, handle, 0.0f, factor);
			}
			// No more events to handle.
			queuedReleasedRefrContactEvents.clear();
		}
	}

	const bool TargetingManager::RefrManipulationManager::IsManaged
	(
		const RE::ObjectRefHandle& a_handle, bool a_grabbed
	)
	{
		// Check if the given refr is handled as either a grabbed or released refr, 
		// depending on the given grabbed flag.

		if (a_grabbed)
		{
			return grabbedRefrHandlesToInfoIndices.contains(a_handle);
		}
		else
		{
			return releasedRefrHandlesToInfoIndices.contains(a_handle);
		}
	}

	void TargetingManager::RefrManipulationManager::MoveUnloadedGrabbedObjectsToPlayer
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// NOTE:
		// Not working consistently enough and unused for now.
		// Move grabbed objects to the player. Should call when P1 has moved to a new location.
		// NOTE 2: 
		// Unfortunately, I could not get MoveTo() to work consistently with grabbed actors,
		// so only object teleportation between cells is supported.

		auto p1 = RE::PlayerCharacter::GetSingleton();
		if (!p1)
		{
			return;
		}

		bool unloaded = false;
		for (uint8_t i = 0; i < grabbedRefrInfoList.size(); ++i)
		{
			// Move unloaded or far away grabbed objects to the player.
			auto& grabbedRefrInfo = grabbedRefrInfoList[i];
			if (!grabbedRefrInfo)
			{
				continue;
			}

			const auto& handle = grabbedRefrInfo->refrHandle;
			auto refrPtr = Util::GetRefrPtrFromHandle(handle);
			if (!refrPtr || refrPtr->As<RE::Actor>())
			{
				continue;
			}
			
			unloaded = 
			{
				(!refrPtr->IsDeleted()) &&
				(
					!refrPtr->Is3DLoaded() ||
					!refrPtr->loadedData ||
					!refrPtr->parentCell ||
					!refrPtr->parentCell->IsAttached() ||
					refrPtr->parentCell != p1->parentCell
				)
			};
			if (auto taskInterface = SKSE::GetTaskInterface(); taskInterface) 
			{
				taskInterface->AddTask
				(
					[refrPtr, p1]() 
					{
						refrPtr->Disable();
						refrPtr->SetParentCell(p1->parentCell);
						refrPtr->MoveTo(p1);
						refrPtr->Enable(false);
					}
				);
			}
		}
	}

	void TargetingManager::RefrManipulationManager::RefreshHandleToIndexMappings
	(
		const bool& a_grabbed
	)
	{
		// Reconstruct the grabbed/released handle-to-list-index mappings
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
				releasedRefrHandlesToInfoIndices.insert
				(
					{ releasedRefrInfoList[i]->refrHandle, i }
				);
			}
		}
	}
	
	void TargetingManager::RefrManipulationManager::SetTotalThrownRefrMagickaCost
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		bool&& a_checkGrabbedRefrsList,
		const float& a_normReleaseAngleFactor
	)
	{
		// Cache the total base magicka cost for throwing all this player's grabbed refrs.
		// Either calculate the cost from the grabbed refrs or released refrs list.
		// If not specifying a release angle factor (or set to -1),
		// calculate the release angle factor based on the 'Grab Object' bind's hold time.

		// No cost when in god mode or if dropping refrs.
		if (a_p->isInGodMode)
		{
			totalThrownRefrMagickaCost = 0.0f;
			return;
		}

		UpdateThrownRefrReleaseAngleFactor(a_p, a_normReleaseAngleFactor);
		float totalMagickaCost = 0.0f;
		if (a_checkGrabbedRefrsList && !grabbedRefrInfoList.empty())
		{
			for (auto i = 0; i < grabbedRefrInfoList.size(); ++i)
			{
				const auto& info = grabbedRefrInfoList[i];
				if (!info->IsValid())
				{
					continue;
				}

				auto objectPtr = Util::GetRefrPtrFromHandle(info->refrHandle);
				if (!objectPtr)
				{
					continue;
				}

				float objectWeight = max(0.0f, objectPtr->GetWeight()) + 0.1f;
				auto asActor = objectPtr->As<RE::Actor>();
				if (asActor)
				{
					const auto grabbedPIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
					// Weights can sometimes be -1, so ensure the weight is at least 0.
					float inventoryWeight = max
					( 
						0.0f,
						grabbedPIndex <= 0 ?
						asActor->GetWeightInContainer() :
						glob.coopPlayers[grabbedPIndex]->em->inventoryChest->GetWeightInContainer()
					);
					const auto invChanges = 
					(
						grabbedPIndex <= 0 ?
						asActor->GetInventoryChanges() :
						glob.coopPlayers[grabbedPIndex]->em->inventoryChest->GetInventoryChanges()
					);
					if (invChanges)
					{
						inventoryWeight = invChanges->totalWeight;
					}

					objectWeight = objectWeight + inventoryWeight;
					totalMagickaCost += 
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normReleaseAngleFactor + 0.5f)
					);
				}
				else
				{
					objectWeight = objectWeight + max(0.0f, objectPtr->GetWeightInContainer());
					totalMagickaCost +=
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normReleaseAngleFactor + 0.5f)
					);
				}
			}
		}
		else if (!a_checkGrabbedRefrsList && !releasedRefrInfoList.empty())
		{
			for (auto i = 0; i < releasedRefrInfoList.size(); ++i)
			{
				const auto& info = releasedRefrInfoList[i];
				if (!info->IsValid())
				{
					continue;
				}

				auto objectPtr = Util::GetRefrPtrFromHandle(info->refrHandle);
				if (!objectPtr)
				{
					continue;
				}

				float objectWeight = max(0.0f, objectPtr->GetWeight()) + 0.1f;
				auto asActor = objectPtr->As<RE::Actor>();
				if (asActor)
				{
					const auto releasedPIndex = GlobalCoopData::GetCoopPlayerIndex(asActor);
					// Weights can sometimes be -1, so ensure the weight is at least 0.
					float inventoryWeight = max
					( 
						0.0f,
						releasedPIndex <= 0 ?
						asActor->GetWeightInContainer() :
						glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetWeightInContainer()
					);
					const auto invChanges = 
					(
						releasedPIndex <= 0 ?
						asActor->GetInventoryChanges() :
						glob.coopPlayers[releasedPIndex]->em->inventoryChest->GetInventoryChanges()
					);
					if (invChanges)
					{
						inventoryWeight = invChanges->totalWeight;
					}
					objectWeight = objectWeight + inventoryWeight;
					totalMagickaCost += 
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normReleaseAngleFactor + 0.5f)
					);
				}
				else
				{
					objectWeight = objectWeight + max(0.0f, objectPtr->GetWeightInContainer());
					totalMagickaCost +=
					(
						(4.0f * sqrtf(objectWeight)) * 
						(0.5f * normReleaseAngleFactor + 0.5f)
					);
				}
			}
		}

		// Apply the player-dependent object manipulation magicka mult last.
		// The player's magicka cost mult is applied in the CheckClampDamageModifier() hook.
		totalThrownRefrMagickaCost = 
		(
			totalMagickaCost *
			Settings::vfMagickaCostMult[a_p->playerID] *
			Settings::vfObjectManipulationMagickaCostMult[a_p->playerID]
		);
	}

	void TargetingManager::RefrManipulationManager::ToggleGrabbedRefrCollisions()
	{
		// For all grabbed refrs, toggle collision off for refrs 
		// without the non-collidable collision layer,
		// and toggle collision on for refrs with that layer.

		if (!isGrabbing)
		{
			return;
		}

		for (const auto& info : grabbedRefrInfoList)
		{
			if (!info->IsValid())
			{
				continue;
			}

			info->ToggleCollision();
		}
	}
	
	void TargetingManager::RefrManipulationManager::UpdateThrownRefrReleaseAngleFactor
	(
		const std::shared_ptr<CoopPlayer>& a_p, const float& a_factorToSet
	)
	{
		// If the given factor is not -1, clamp and return it.
		// Otherwise, calculate a new one by normalizing and returning the grab bind hold time, 
		// which directly influences the angle at which released refrs are thrown.

		if (a_factorToSet != -1.0f)
		{
			normReleaseAngleFactor = std::clamp(a_factorToSet, 0.0f, 1.0f);
			return;
		}

		// Adjust release speed based on how long the grab bind was held.
		const auto actionIndex = !InputAction::kGrabObject - !InputAction::kFirstAction;
		float cappedHoldTime = 
		(
			min
			(
				a_p->pam->paStatesList[actionIndex].secsPerformed,
				max(0.01f, Settings::fSecsToReleaseObjectsAtMaxSpeed)
			)
		);

		// Normalize and cache it.
		normReleaseAngleFactor = std::lerp
		(
			0.0f, 
			1.0f,
			cappedHoldTime / max(0.01f, Settings::fSecsToReleaseObjectsAtMaxSpeed)
		);
	}

	const bool TargetingManager::RefrManipulationManager::WasThrown
	(
		const RE::ObjectRefHandle& a_handle
	)
	{
		// Was the given refr released as thrown?

		// No valid handle, so couldn't have been thrown.
		if (!Util::HandleIsValid(a_handle))
		{
			return false;
		}

		// Does not map to an index in the released refr info list.
		const auto iter = releasedRefrHandlesToInfoIndices.find(a_handle);
		if (iter == releasedRefrHandlesToInfoIndices.end())
		{
			return false;
		}

		// Mapped index lies outside the bounds of the released refr info list.
		if (iter->second >= releasedRefrInfoList.size())
		{
			return false;
		}

		return releasedRefrInfoList[iter->second]->isThrown;
	}

	void TargetingManager::RefrTargetMotionState::UpdateMotionState
	(
		RE::ObjectRefHandle a_targetRefrHandle
	)
	{
		// Update physical motion-related data for given the target refr.
		// Used for predictive projectile trajectory calculations.

		// New target.
		if (a_targetRefrHandle != targetRefrHandle) 
		{
			Refresh();
		}

		targetRefrHandle = a_targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle); 
		bool targetRefrValidity = targetRefrPtr && targetRefrPtr->IsHandleValid();
		if (!targetRefrValidity)
		{
			return;
		}

		auto asActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		// Need a valid char controller for targeted actors.
		if (asActorPtr && !asActorPtr->GetCharController())
		{
			return;
		}

		lastUpdateTP = SteadyClock::now();
		if (firstUpdate)
		{
			// Need to set both current and previous positions/velocities/angles
			// to their corresponding initial values on the first update.
			if (asActorPtr)
			{
				RE::hkVector4 pos{ };
				asActorPtr->GetCharController()->GetPosition(pos, false);
				pPos = cPos = ToNiPoint3(pos) * HAVOK_TO_GAME;
				pVel = cVel = Util::GetActorLinearVelocity(asActorPtr.get());
				pYaw = cYaw = 
				(
					cVel.Length() == 0.0f ? 
					asActorPtr->GetHeading(false) : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}
			else
			{
				pPos = cPos = targetRefrPtr->data.location;
				RE::NiPoint3 linVel{ };
				targetRefrPtr->GetLinearVelocity(linVel);
				pVel = cVel = linVel;
				pYaw = cYaw = 
				(
					cVel.Length() == 0.0f ? 
					targetRefrPtr->data.angle.z : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}
			
			apiAccel =
			apiVel = RE::NiPoint3();
			apiSpeedDelta = 
			apiYawAngDelta = 0.0f;
		}
		else
		{
			++avgDataFrameCount;

			// Set previous data to current data before updating the current data.
			pPos = cPos;
			pVel = cVel;
			pAccelPerFrame = cAccelPerFrame;
			pYaw = cYaw;
			pYawAngDeltaPerFrame = cYawAngDeltaPerFrame;

			if (asActorPtr)
			{
				// NOTE: 
				// Both the reported actor movement speed (DoGetMovementSpeed())
				// and the havok char controller movement speed (GetLinearVelocity())
				// do not fully account for the actor bumping into a surface and slowing down, 
				// (havok controller speed only partially accounts for the slowdown),
				// so we'll make use of the pos delta-derived speed 
				// (accounts for collision slowdowns). 
				// Drawback: 
				// However, the character controller position delta-derived speed
				// is noisier and prone to sudden changes, 
				// which leads to uglier predicted trajectory arcs when drawing trajectories.
				RE::hkVector4 pos{ };
				asActorPtr->GetCharController()->GetPosition(pos, false);
				cPos = ToNiPoint3(pos) * HAVOK_TO_GAME;
				cPos = pPos + RE::NiPoint3
				(
					std::lerp(0.0f, (cPos - pPos).x, 0.25f),
					std::lerp(0.0f, (cPos - pPos).y, 0.25f),
					std::lerp(0.0f, (cPos - pPos).z, 0.25f)
				);
				cVel = (cPos - pPos) / *g_deltaTimeRealTime;
				cYaw = 
				(
					cVel.Length() == 0.0f ? 
					asActorPtr->GetHeading(false) : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}
			else
			{
				cPos = targetRefrPtr->data.location;
				cPos = pPos + RE::NiPoint3
				(
					std::lerp(0.0f, (cPos - pPos).x, 0.25f),
					std::lerp(0.0f, (cPos - pPos).y, 0.25f),
					std::lerp(0.0f, (cPos - pPos).z, 0.25f)
				);
				cVel = (cPos - pPos) / *g_deltaTimeRealTime;
				cYaw = 
				(
					cVel.Length() == 0.0f ? 
					targetRefrPtr->data.angle.z : 
					Util::GetYawBetweenPositions(RE::NiPoint3(), cVel)
				);
			}

			// Acceleration, change in speed and yaw per frame.
			cAccelPerFrame = (cVel - pVel);
			cSpeedDeltaPerFrame = (cVel.Length() - pVel.Length());
			cYawAngDeltaPerFrame = Util::NormalizeAngToPi(cYaw - pYaw);

			// Total-over-an-interval data used to get the average per interval.
			toiAccel += cAccelPerFrame;
			toiSpeedDelta += cSpeedDeltaPerFrame;
			toiVel += cVel;
			toiYawAngDelta += cYawAngDeltaPerFrame;

			// Reported velocity changes are drastic even over a short amount of time,
			// leading to large, accumulating errors in aim prediction.
			// Can't reliably use them alone, so the average-per-interval is also calculated.
			if (avgDataFrameCount == FRAMES_BETWEEN_AVG_DATA_UPDATES)
			{
				apiAccel = toiAccel / avgDataFrameCount;
				apiVel = toiVel / avgDataFrameCount;
				apiSpeedDelta = toiSpeedDelta / avgDataFrameCount;
				apiYawAngDelta = toiYawAngDelta / avgDataFrameCount;

				// Reset totals and frame count each time the averages are set.
				toiAccel = 
				toiVel = RE::NiPoint3();
				toiSpeedDelta =
				toiYawAngDelta =
				avgDataFrameCount = 0;
			}
		}

		firstUpdate = false;
	}

	RE::NiPoint3 TargetingManager::ManagedProjTrajectoryInfo::CalculatePredInterceptPos
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const bool& a_adjustReleaseSpeed,
		double& a_timeToTarget
	)
	{
		// Calculate the position at which the launched projectile is likely to collide
		// with the target. 
		// Use the target's physical motion data to perform this calculation.
		// Adjust the projectile's release speed, if requested,
		// to allow it to hit the predicted target position, despite the effects of air drag.
		// 
		// Return the time taken to hit the target at the computed intercept position
		// through the outparam.
		// 'NaN' or 'inf' if the projectile cannot hit the target position.
		
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		// No valid target refr, so aim at the crosshair position.
		if (!targetRefrPtr)
		{
			double xy = Util::GetXYDistance(trajectoryEndPos, releasePos);
			double z = (trajectoryEndPos - releasePos).z;
			if (a_adjustReleaseSpeed)
			{
				releaseSpeed = GetReleaseSpeedToTarget(xy, z, -a_p->mm->aimPitch);
			}
			
			a_timeToTarget = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
			return trajectoryEndPos;
		}

		auto targetActorPtr = RE::ActorPtr(targetRefrPtr->As<RE::Actor>());
		// Set the initial predicted intercept/hit position to the initial aimed-at position.
		RE::NiPoint3 predHitPos = trajectoryEndPos;
		// Next predicted velocity for the target. Set to current velocity initially.
		RE::NiPoint3 nPredTargetVel = a_p->tm->targetMotionState->cVel;
		// Axis to rotate velocity vector around.
		RE::NiPoint3 upAxis{ 0.0f, 0.0f, 1.0f };
		// XY and Z offsets to the predicted position from the release position.
		double xy = Util::GetXYDistance(predHitPos, releasePos);
		double z = (trajectoryEndPos - releasePos).z;
		// Initial release speed, adjust as needed.
		double firstReleaseSpeed = 
		releaseSpeed = 
		(
			a_adjustReleaseSpeed ? 
			GetReleaseSpeedToTarget(xy, z, launchPitch) : 
			releaseSpeed
		);
		// Current delta yaw and yaw rotation speed.
		const float& currentYawAngDelta = a_p->tm->targetMotionState->cYawAngDeltaPerFrame;
		float currentZRotSpeed = 0.0f;
		if (targetActorPtr)
		{
			currentZRotSpeed =
			(
				targetActorPtr->currentProcess && targetActorPtr->currentProcess->middleHigh ?
				targetActorPtr->currentProcess->middleHigh->rotationSpeed.z :
				0.0f
			);
		}
			
		float rotationSign = currentYawAngDelta < 0.0f ? -1.0f : 1.0f;
		// Average of current and average per interval yaw deltas.
		float avgYawDeltaPerFrame = 
		(
			(
				currentYawAngDelta +
				a_p->tm->targetMotionState->apiYawAngDelta
			) / 2.0f
		);
		// Average of current and average per interval change in speed.
		const float avgSpeedDelta = 
		(
			(
				a_p->tm->targetMotionState->cSpeedDeltaPerFrame +
				a_p->tm->targetMotionState->apiSpeedDelta
			) / 2.0f
		);
		
		// Time to target, accounting for air resistance.
		double t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
		// Previously calculated time to target.
		double tPrev = 0.0;
		// Difference in the calculated times to target.
		double tDiff = fabsf(t - tPrev);
		// Flight time deltas at which to bail out of the calculation loop.
		// Converging on a time-of-flight if below this value.
		double timeBailDeltaMin = 1E-4;
		// Diverging time-of-flight if above this value.
		double timeBailDeltaMax = 1000.0;
		// Max number of iterations, current number of iterations.
		uint8_t steps = 50;
		uint8_t step = 0;
		// Attempt to accurately estimate the target intercept position
		// and continue looping until the reported time-to-target values converge
		// to below the minimum time diff (success), 
		// or diverge above the maximum time diff (failure),
		// or until the maximum number of iterations is reached (could go either way).
		while (step < steps && tDiff > timeBailDeltaMin && tDiff < timeBailDeltaMax)
		{
			// SUPER NOTE: 
			// Everything below is obviously not mathematically correct, 
			// since the target's velocity and acceleration are changing constantly,
			// which means that finding the best predicted hit position
			// would require integration over the time of flight.
			// However the recorded acceleration and velocity motion data
			// for targets is very noisy, which leads to huge overshoots
			// when using the proper formulas for calculating the predicted position at time t.
			// This temporary, manually-tested solution performs slightly better.
			
			// Rotate predicted velocity vector by the yaw diff 
			// which will occur over the time delta.
			double angToRotate = -Util::NormalizeAngToPi(avgYawDeltaPerFrame * tDiff);
			double speed = nPredTargetVel.Length();
			// Rotate and re-apply original speed, since the vector is normalized upon rotation.
			Util::RotateVectorAboutAxis(nPredTargetVel, upAxis, angToRotate);
			nPredTargetVel.Unitize();
			nPredTargetVel *= speed;
			// Offset the current aimed at position by the delta position calculated
			// using the position delta over the elapsed time frame from the previous iteration.
			auto posDelta = nPredTargetVel * (t - tPrev);
			predHitPos += posDelta;

			// Update positional offsets based on the new predicted hit position.
			xy = Util::GetXYDistance(predHitPos - releasePos);
			z = (predHitPos - releasePos).z;
			// Adjust the release speed to account for air drag again.
			if (a_adjustReleaseSpeed)
			{
				releaseSpeed = GetReleaseSpeedToTarget(xy, z, launchPitch);
			}

			// Set previous time to target to current.
			tPrev = t;
			// Update current time to target using the new XY positional offset
			// and release speed.
			t = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
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
			// Failed to find intercept position, 
			// so set to the initially-aimed-at position as a fallback.
			predHitPos = trajectoryEndPos;
		}

		// Set the final time to target before returning the predicted intercept position.
		xy = Util::GetXYDistance(predHitPos - releasePos);
		z = (predHitPos - releasePos).z;
		a_timeToTarget = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;

		return predHitPos;
	}

	double TargetingManager::ManagedProjTrajectoryInfo::GetReleaseSpeedToTarget
	(
		const double& a_xy, const double& a_z, const double& a_launchPitch
	)
	{
		// Accounting for linear air resistance, 
		// get the release speed required to hit the target position
		// given by the XY and Z offsets and the launch pitch.
		
		// Get release speed first.
		double releaseSpeedNew = releaseSpeed;
		double w = -exp((a_z * mu * mu / g) - (a_xy * tanf(a_launchPitch) * mu * mu / g) - 1.0);
		const auto solnPair = Util::LambertWFunc::ApproxRealSolutionBothBranches(w, 1E-10);
		// Two potential solutions.
		double launchSpeed1 = 
		(
			solnPair.first.has_value() ? 
			(a_xy * mu) / (cosf(a_launchPitch) * (solnPair.first.value() + 1.0)) : 
			-1.0
		);
		double launchSpeed2 = 
		(
			solnPair.second.has_value() ?
			(a_xy * mu) / (cosf(a_launchPitch) * (solnPair.second.value() + 1.0)) : 
			-1.0
		);
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
		if (trajType == ProjectileTrajType::kAimDirection &&
			isPhysicalProj && 
			releaseSpeed < maxReleaseSpeed) 
		{
			// Smallest of the three.
			return min(releaseSpeed, min(maxReleaseSpeed, releaseSpeedNew));
		}

		// Can't be larger than the max release speed.
		return min(maxReleaseSpeed, releaseSpeedNew);
	}

	float TargetingManager::ManagedProjTrajectoryInfo::GetRoughMinLaunchPitch
	(
		const std::shared_ptr<CoopPlayer>& a_p
	)
	{
		// Get rough estimate of the minimum launch pitch required to hit the target, 
		// based on the given projectile's release speed.
		
		// NOTE: 
		// Cannot analytically find a solution for this minimum launch pitch
		// when applying air resistance, so calculate the drop when aiming in a straight line 
		// at the predicted intercept position and then add the drop
		// to the predicted intercept position to compensate.
		// Finally, recalculate the pitch to the new temporary intercept position.
		// Still will fail to hit the intercept position at range, 
		// but is more accurate than simply setting the
		// minimum launch pitch to the straight-line-to-target pitch.

		float straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
		// Time to target and positional offsets, as usual.
		double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
		double tAirDrag = 
		(
			-log(1.0 - ((xy * mu) / (releaseSpeed * cosf(straightLinePitch)))) / mu
		);
		double z = 
		(
			(-g * tAirDrag / mu) + 
			((releaseSpeed * sinf(straightLinePitch) + g / mu) / mu) * 
			(1.0 - exp(-mu * tAirDrag))
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
			return 
			(
				-Util::GetPitchBetweenPositions
				(
					releasePos, trajectoryEndPos + RE::NiPoint3(0.0f, 0.0f, straightLineTrajDrop)
				)
			);
		}
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetInitialBaseProjectileData
	(
		const std::shared_ptr<CoopPlayer>& a_p,
		const RE::ObjectRefHandle& a_projectileHandle, 
		const float& a_releaseSpeed
	)
	{
		// Set physical data and projectile data that depends on the base projectile type
		// for the given projectile and its given initial release speed.
		
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
				float fullDrawTime = 
				(
					0.4f + 
					(
						1.66f / 
						(
							weap->GetSpeed() * 
							(
								1.0f + (float)a_p->coopActor->HasPerk(glob.quickShotPerk)
							)
						)
					) +
					0.6f
				);
				float drawTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kAttackRH);
				float power = 
				(
					std::clamp(drawTime, fullDrawTime * 0.35f, fullDrawTime) / fullDrawTime
				);
				releaseSpeed = maxReleaseSpeed * power;
			}
			else
			{
				// Fixed initial release speed otherwise.
				releaseSpeed = ammo->data.projectile->data.speed;
			}
			
			// Set projectile base gravity factor.
			projGravFactor = ammo->data.projectile->data.gravity;
		}
		else if (auto avEffect = projectile->avEffect; avEffect && avEffect->data.projectileBase)
		{
			// Is a magic projectile.
			isPhysicalProj = false;
			// Max and initial release speeds are the same.
			maxReleaseSpeed = releaseSpeed = avEffect->data.projectileBase->data.speed;
			if (avEffect->data.projectileBase->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower,
					RE::BGSProjectileData::Type::kGrenade
				))
			{
				// Beams, flames, and grenades (lobber) are not affected by gravity,
				// at least according to the CK.
				projGravFactor = 0.0f;
			}
			else
			{
				// Set projectile base gravity factor.
				projGravFactor = avEffect->data.projectileBase->data.gravity;
			}
		}
		else
		{
			// Anything else.
			isPhysicalProj = true;
			maxReleaseSpeed = releaseSpeed = a_releaseSpeed;
		}

		// Physical constants.
		mu = Settings::fMu;
		g = Util::GetGravitationalConstant() * projGravFactor;
		// And lastly, the release position.
		releasePos = projectile->data.location;
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetInitialBaseProjectileData
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		RE::BGSProjectile* a_projectileBase, 
		RE::TESObjectWEAP* a_weaponSource,
		RE::EffectSetting* a_magicEffectSource, 
		const RE::NiPoint3& a_releasePos
	)
	{
		// Set based on a base projectile form.
		// Used to obtain trajectory data when a projectile has not been fired yet.
		// If wishing to set a magic projectile's trajectory, specify the magic effect
		// associated with the projectile; nullptr if not a magical projectile.
		
		if (!a_projectileBase)
		{
			return;
		}

		// Base projectile-dependent data.
		projGravFactor = 1.0;
		if (a_magicEffectSource && a_magicEffectSource->data.projectileBase)
		{
			// Is a magic projectile.
			isPhysicalProj = false;
			// Max and initial release speeds are the same.
			maxReleaseSpeed = releaseSpeed = a_magicEffectSource->data.projectileBase->data.speed;
			if (a_magicEffectSource->data.projectileBase->data.types.any
				(
					RE::BGSProjectileData::Type::kBeam,
					RE::BGSProjectileData::Type::kFlamethrower,
					RE::BGSProjectileData::Type::kGrenade
				))
			{
				// Beams, flames, and grenades (lobber) are not affected by gravity,
				// at least according to the CK.
				projGravFactor = 0.0f;
			}
			else
			{
				// Set projectile base gravity factor.
				projGravFactor = a_magicEffectSource->data.projectileBase->data.gravity;
			}
		}
		else if (a_weaponSource)
		{
			// Is a physical projectile.
			isPhysicalProj = true;
			maxReleaseSpeed = a_projectileBase->data.speed;
			if (a_weaponSource->IsBow())
			{
				// Set release speed based on draw time.
				float fullDrawTime = 
				(
					0.4f + 
					(
						1.66f / 
						(
							a_weaponSource->GetSpeed() * 
							(
								1.0f + (float)a_p->coopActor->HasPerk(glob.quickShotPerk)
							)
						)
					) + 
					0.6f
				);
				float drawTime = a_p->pam->GetPlayerActionInputHoldTime(InputAction::kAttackRH);
				float power = 
				(
					std::clamp(drawTime, fullDrawTime * 0.35f, fullDrawTime) / fullDrawTime
				);
				releaseSpeed = maxReleaseSpeed * power;
			}
			else
			{
				// Fixed initial release speed otherwise.
				releaseSpeed = a_projectileBase->data.speed;
			}
			
			// Set projectile base gravity factor.
			projGravFactor = a_projectileBase->data.gravity;
		}
		else
		{
			// Anything else.
			isPhysicalProj = true;
			maxReleaseSpeed = releaseSpeed = 2500.0f;
		}

		// Physical constants.
		mu = Settings::fMu;
		g = Util::GetGravitationalConstant() * projGravFactor;
		// And lastly, the release position.
		releasePos = a_releasePos;
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::ObjectRefHandle& a_projectileHandle, 
		RE::NiPoint3& a_initialVelocityOut, 
		const ProjectileTrajType& a_trajType
	)
	{
		// Sets up the initial trajectory data for the given projectile
		// based on the given starting velocity (which is modified) and the trajectory type.
		// NOTE: 
		// Should be run once when the projectile of interest is released.

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

		// Set initial base projectile data first.
		SetInitialBaseProjectileData(a_p, a_projectileHandle, a_initialVelocityOut.Length());
		// Targeting angle at which the projectile would be released.
		float targetingAngle = 
		(
			a_p->pam->isAttacking ? 
			Util::DirectionToGameAngYaw(a_p->mm->playerDefaultAttackSourceDir) :
			a_p->coopActor->data.angle.z	
		);
		// Set trajectory data common to both the predicted and at-launch trajectories.
		SetTrajectory
		(
			a_p, 
			projectile->data.location,
			a_trajType, 
			targetingAngle, 
			projectile->As<RE::BeamProjectile>() || projectile->As<RE::FlameProjectile>()
		);

		// Re-scale the initial velocity sent on launch to our computed release speed.
		a_initialVelocityOut.Unitize();
		a_initialVelocityOut *= releaseSpeed;
		// Perform ammo projectile damage scaling based on 
		// the ratio of the computed release speed over the max release speed.
		// Projectile power always defaults to 1 for companion players,
		// so we can scale it directly.
		// For P1, we have to compute the original weapon damage (before power scales it)
		// by dividing the current weapon damage by the power set on launch. 
		// Then we also scale the power/resultant weapon damage 
		// based on our own release speed factor.
		// This will directly adjust the output damage of the projectile on hit.
		if (const auto ammo = projectile->ammoSource; ammo && ammo->data.projectile)
		{
			// Scale arrow/bolt's damage based on the computed power.
			double releaseSpeedFactor = std::clamp(releaseSpeed / maxReleaseSpeed, 0.1, 1.0);
			if (a_p->isPlayer1) 
			{
				float originalWeaponDamage = 
				(
					projectile->weaponDamage / max(0.1f, projectile->power)
				);
				projectile->weaponDamage = originalWeaponDamage * releaseSpeedFactor;
				projectile->power = releaseSpeedFactor;
			}
			else
			{
				projectile->weaponDamage *= releaseSpeedFactor;
				projectile->power = releaseSpeedFactor;
			}
		}
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		RE::BGSProjectile* a_projectileBase, 
		RE::TESObjectWEAP* a_weaponSource,
		RE::EffectSetting* a_magicEffectSource, 
		const RE::NiPoint3& a_releasePos,
		const ProjectileTrajType& a_trajType
	)
	{
		// Sets up the initial trajectory data based on the given starting velocity 
		// (which is modified) and the trajectory type.
		// Used to obtain trajectory data when a projectile has not been fired yet.

		if (!a_projectileBase)
		{
			return;
		}

		// Set initial base projectile data first.
		SetInitialBaseProjectileData
		(
			a_p, a_projectileBase, a_weaponSource, a_magicEffectSource, a_releasePos
		);
		// Targeting angle at which the projectile would be released.
		float targetingAngle = 
		(
			a_p->pam->isAttacking ? 
			Util::DirectionToGameAngYaw(a_p->mm->playerDefaultAttackSourceDir) :
			a_p->coopActor->data.angle.z	
		);
		// Set trajectory data common to both the predicted and at-launch trajectories.
		SetTrajectory
		(
			a_p, 
			a_releasePos,
			a_trajType, 
			targetingAngle,
			a_projectileBase->data.types.any
			(
				RE::BGSProjectileData::Type::kBeam,
				RE::BGSProjectileData::Type::kFlamethrower
			)
		);
	}

	void TargetingManager::ManagedProjTrajectoryInfo::SetTrajectory
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::NiPoint3& a_releasePos,
		const ProjectileTrajType& a_trajType, 
		const float& a_initialYaw, 
		const bool& a_setStraightTrajectory
	)
	{
		// Sets up the initial trajectory data in common 
		// for both projected and launched trajectories.

		trajType = a_trajType;
		startedHomingIn = false;
		canReachTarget = true;
		// Set target refr regardless of projectile trajectory type.
		targetRefrHandle = a_p->tm->targetMotionState->targetRefrHandle;
		auto targetRefrPtr = Util::GetRefrPtrFromHandle(targetRefrHandle);
		bool targetRefrValidity = 
		(
			targetRefrPtr && Util::IsValidRefrForTargeting(targetRefrPtr.get())
		);
		auto targetActorPtr = 
		(
			targetRefrValidity ? RE::ActorPtr(targetRefrPtr->As<RE::Actor>()) : nullptr
		);
		targetLocalPosOffset = 
		(
			Util::HandleIsValid(targetRefrHandle) ? 
			a_p->tm->crosshairLocalPosOffset : 
			RE::NiPoint3()
		);
		targetedActorNode.reset();
		// Default to crosshair world position.
		trajectoryEndPos = a_p->tm->crosshairWorldPos;
		// When facing the crosshair, choose the exact crosshair position 
		// locally offset from the target refr; 
		// otherwise, target the selected refr's center position.
		// Done to maximize hit chance, especially for actors,
		// since an actor's center position is most likely 
		// to be within their character controller collider.
		if (targetRefrValidity) 
		{
			trajectoryEndPos =
			(
				targetActorPtr ? 
				Util::GetTorsoPosition(targetActorPtr.get()) :
				Util::GetRefrPosition(targetRefrPtr.get())
			);
			// Refr is selected by the crosshair and the player is facing it.
			if (a_p->tm->crosshairActive && targetRefrHandle == a_p->tm->crosshairRefrHandle) 
			{
				trajectoryEndPos += targetLocalPosOffset;
			}
		}

		// Firing an aim prediction or aim direction projectile 
		// while aiming at an actor or facing the target refr.
		bool predictInterceptPos = 
		(
			(a_trajType != ProjectileTrajType::kHoming) && 
			(!a_setStraightTrajectory) &&
			((targetActorPtr) || (a_p->tm->crosshairActive))
		);

		if (predictInterceptPos) 
		{
			// XY and Z offsets from the release position to the trajectory end position.
			double xy = 0.0;
			double z = 0.0;
			float minLaunchPitch = GetRoughMinLaunchPitch(a_p);
			launchPitch = std::clamp
			(
				a_p->mm->aimPitchManuallyAdjusted ?
				-a_p->mm->aimPitch :
				minLaunchPitch, 
				-89.9f * PI / 180.0f,
				89.9f * PI / 180.0f
			);
			// Add some arc to fast projectiles by decreasing their release speed
			// when released at a steeper angle.
			float straightLinePitch = -Util::GetPitchBetweenPositions
			(
				releasePos, trajectoryEndPos
			);
			if (releaseSpeed > 10000.0f)
			{
				maxReleaseSpeed = releaseSpeed;
				double ratio = std::clamp
				(
					1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
				);
				releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
			}

			// Calculate the position at which the projectile is predicted to hit the target actor.
			// As of now, no release speed modifications for projectiles.
			// Used to only modify the release speed of physical projectiles instead of modifying 
			// the gravitational constant to hit the target position,
			// but doing so would negate the bow draw-time mechanic, so I've decided against it.

			// Set both the intercept position and the initial time to reach that position.
			trajectoryEndPos = CalculatePredInterceptPos(a_p, false, initialTrajTimeToTarget);
			// Launch towards the computed end position.
			launchYaw = Util::ConvertAngle
			(
				Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
			);
			// XY offset from release pos to trajectory end pos.
			xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			// Z offset from release pos to trajectory end pos.
			z = (trajectoryEndPos - releasePos).z;

			// Since we are not modifying the projectile's release speed,
			// in order to still hit the intercept position, 
			// we modify the gravitational constant.
			// Better obviously for launching accurate, fast-arcing projectiles
			// instead of lowering the release speed while keeping g constant
			// in order to hit the same position.
			// But looks a bit odd for flat trajectory shots when g is low.
			// Tradeoffs, schmadeoffs.
			
			// Save base projectile-determined gravitational constant.
			float baseG = g;
			// New g to allow the projectile to hit the target.
			g = 
			(
				(
					(mu * mu * releaseSpeed * cosf(launchPitch)) * 
					(z - xy * tanf(launchPitch))
				) / 
				(
					(
						releaseSpeed * 
						cosf(launchPitch) * 
						log(1 - (xy * mu) / (releaseSpeed * cosf(launchPitch)))
					) + xy * mu
				)
			);
			
			// NOTE:
			// We do a little gravity inversion (maybe).
			// Compensates for having no analytical solution 
			// to the minimum aim pitch to hit the target.
			// Can reach if:
			// 1. Gravity does not have to be negated -OR- 
			// 2. If the player has not adjusted their aim -AND-
			// 3. The time to target is beyond the manageable interval -AND-
			// 4. The gravitational constant and time to target are valid.
			canReachTarget = 
			(
				(g >= 0.0f || !a_p->mm->aimPitchManuallyAdjusted) &&
				initialTrajTimeToTarget > 0.0f &&
				initialTrajTimeToTarget < Settings::fMaxProjAirborneSecsToTarget &&
				!isnan(g) &&
				!isinf(g) &&
				!isnan(initialTrajTimeToTarget) &&
				!isinf(initialTrajTimeToTarget)
			);
			// Reset g to default for a more natural path when not reaching the target.
			if (!canReachTarget)
			{
				g = baseG;
				if (isnan(initialTrajTimeToTarget) || isinf(initialTrajTimeToTarget))
				{
					// Shoot far away in the aiming direction.
					initialTrajTimeToTarget = Settings::fMaxProjAirborneSecsToTarget;
					xy = 
					(
						(releaseSpeed * cosf(launchPitch) / mu) * 
						(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
					);
					z = 
					(
						(-g * Settings::fMaxProjTrajectorySecsToTarget / mu) + 
						((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
						(1.0 - exp(-mu *Settings::fMaxProjTrajectorySecsToTarget))
					);
				}
				else
				{
					// Finite and manageable time to target, 
					// so maintain launch orientation and speed.
					xy = 
					(
						(releaseSpeed * cosf(launchPitch) / mu) * 
						(1.0 - exp(-mu * initialTrajTimeToTarget))
					);
					z = 
					(
						(-g * initialTrajTimeToTarget / mu) + 
						((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
						(1.0 - exp(-mu * initialTrajTimeToTarget))
					);
				}

				// If the projectile cannot reach the target, 
				// adjust the trajectory end position to a position 
				// far away in the player's aiming direction.
				trajectoryEndPos = RE::NiPoint3
				(
					releasePos.x + xy * cosf(launchYaw), 
					releasePos.y + xy * sinf(launchYaw),
					releasePos.z + z
				);
			}
		}
		else if (a_trajType == ProjectileTrajType::kHoming)
		{
			// Set straight-line pitch from release position to end position
			// after calculating the trajectory end position.
			// NOTE: 
			// Launch pitch/straight line pitch is sign-flipped
			// relative to the game's pitch sign convention.
			float straightLinePitch = -a_p->mm->aimPitch;
			if (targetActorPtr)
			{
				// If the target is an aim correction or linked target, target the torso.
				if (targetRefrHandle == a_p->tm->aimCorrectionTargetHandle)
				{
					trajectoryEndPos = Util::GetTorsoPosition(targetActorPtr.get());
				}

				straightLinePitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
				// Set launch angles, always above the straight line pitch.
				launchPitch = std::clamp
				(
					max(straightLinePitch, -a_p->mm->aimPitch), 
					-89.9f * PI / 180.0f, 
					89.9f * PI / 180.0f
				);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);

				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (releaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = releaseSpeed;
					double ratio = std::clamp
					(
						1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
					);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE:
				// No air resistance, so the XY component of velocity 
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max
				(
					0.0, 
					Util::GetXYDistance(releasePos, trajectoryEndPos) / 
					(releaseSpeed * cosf(launchPitch))
				);
			}
			else
			{
				// XY offset to trajectory end position.
				double xy = 0.0f;
				// Aim as far away as the max navmesh move distance 
				// or crosshair world position, whichever is farther away.
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = 
				(
					iniPrefSettings ? 
					iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : 
					nullptr
				); 
				if (projMaxDistSetting && 
					releasePos.GetDistance(a_p->tm->crosshairWorldPos) <
					projMaxDistSetting->data.f)
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
				launchPitch = std::clamp
				(
					max(straightLinePitch, -a_p->mm->aimPitch),
					-89.9f * PI / 180.0f,
					89.9f * PI / 180.0f
				);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);

				// Add some arc to fast projectiles by decreasing their release speed 
				// when released at a steeper angle.
				if (releaseSpeed > 10000.0f)
				{
					maxReleaseSpeed = releaseSpeed;
					double ratio = std::clamp
					(
						1.0 - (max(0.0, launchPitch - straightLinePitch) / (PI / 2.0)), 0.0, 1.0
					);
					releaseSpeed = Util::InterpolateEaseIn(10000.0f, maxReleaseSpeed, ratio, 7.0f);
				}

				// NOTE: 
				// No air resistance, so the XY component of velocity
				// is constant along the fixed trajectory portion of flight.
				initialTrajTimeToTarget = max(0.0, xy / (releaseSpeed * cosf(launchPitch)));
			}

			// The usual, except without air resistance.
			double xy = Util::GetXYDistance(releasePos, trajectoryEndPos);
			double z = (releasePos - trajectoryEndPos).z;
			g = 
			(
				(2.0 / xy) * 
				(
					(powf(releaseSpeed, 2.0f) * cosf(launchPitch) * sinf(launchPitch)) + 
					((z * powf(releaseSpeed * cosf(launchPitch), 2.0f)) / (xy))
				)
			);
			g = isnan(g) || isinf(g) ? g = 0.0 : g;
		}
		else if (a_setStraightTrajectory)
		{
			// If there is a valid target actor or if the facing the crosshair world position,
			// aim directly at the current crosshair position without modifying the release speed.
			// If not, aim far away in the projectile's initial facing direction, 

			// Set launch angles, end position, and time to target.
			if ((targetActorPtr) || (a_p->tm->crosshairActive))
			{
				launchPitch = -Util::GetPitchBetweenPositions(releasePos, trajectoryEndPos);
				launchYaw = Util::ConvertAngle
				(
					Util::GetYawBetweenPositions(releasePos, trajectoryEndPos)
				);
			}
			else
			{
				// Launch far away in aiming direction.
				launchPitch = -a_p->mm->aimPitch;
				launchYaw = Util::ConvertAngle(a_initialYaw);
				RE::NiPoint3 launchDir = Util::RotationToDirectionVect(launchPitch, launchYaw);
				// Choose endpoint that is far from the release point.
				double farDist = FLT_MAX;
				auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
				auto projMaxDistSetting = 
				(
					iniPrefSettings ?
					iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : 
					nullptr
				); 
				if (projMaxDistSetting && 
					releasePos.GetDistance(a_p->tm->crosshairWorldPos) < 
					projMaxDistSetting->data.f)
				{
					farDist = projMaxDistSetting->data.f;
				}
				else
				{
					farDist = max
					(
						Settings::fMaxRaycastAndZoomOutDistance, 
						releasePos.GetDistance(a_p->tm->crosshairWorldPos)
					);
				}

				trajectoryEndPos = releasePos + launchDir * farDist;
			}

			initialTrajTimeToTarget = releasePos.GetDistance(trajectoryEndPos) / releaseSpeed;
		}
		else
		{
			// Aim direction projectile and not facing the crosshair world position.
			// Aim far, far away in the direction that the player is facing.

			// Set launch angles.
			launchPitch = std::clamp
			(
				-a_p->mm->aimPitch, -89.9f * PI / 180.0f, 89.9f * PI / 180.0f
			);
			launchYaw = Util::ConvertAngle(a_initialYaw);

			// Choose endpoint that is far from the release point.
			// Default time of flight is arbitrary, but should be relatively large.
			// Accounting for air resistance.
			double xy = 0.0;
			double z = 0.0;
			double tAirDrag = 0.0;

			auto iniPrefSettings = RE::INIPrefSettingCollection::GetSingleton();
			auto projMaxDistSetting = 
			(
				iniPrefSettings ? iniPrefSettings->GetSetting("fVisibleNavmeshMoveDist") : nullptr
			); 
			if (projMaxDistSetting) 
			{
				xy = projMaxDistSetting->data.f;;
				tAirDrag = 
				(
					-log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu
				);
				z = 
				(
					(-g * tAirDrag / mu) + 
					((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
					(1.0 - exp(-mu * tAirDrag))
				);
			}
			else
			{
				xy = 
				(
					(releaseSpeed * cosf(launchPitch) / mu) * 
					(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
				);
				tAirDrag = -log(1.0 - ((xy * mu) / (releaseSpeed * cosf(launchPitch)))) / mu;
				z = 
				(
					(-g * Settings::fMaxProjTrajectorySecsToTarget / mu) + 
					((releaseSpeed * sinf(launchPitch) + g / mu) / mu) * 
					(1.0 - exp(-mu * Settings::fMaxProjTrajectorySecsToTarget))
				);
			}
			
			initialTrajTimeToTarget = 
			(
				isnan(tAirDrag) ? 
				static_cast<double>(Settings::fMaxProjAirborneSecsToTarget) :
				std::clamp
				(
					tAirDrag,
					0.0,
					static_cast<double>(Settings::fMaxProjAirborneSecsToTarget)
				)
			);
			trajectoryEndPos = RE::NiPoint3
			(
				releasePos.x + xy * cosf(launchYaw), 
				releasePos.y + xy * sinf(launchYaw),
				releasePos.z + z
			);
		}

		// REMOVE when done debugging.
		DBG
		(
			"Aim correction: {}, selected: {}, linked ref: {}, crosshair refr: {}, "
			"traj type: {}, set straight traj: {}, predict: {}, target actor: {}. TTT: {}.",
			Util::HandleIsValid(a_p->tm->aimCorrectionTargetHandle) ?
			a_p->tm->aimCorrectionTargetHandle.get()->GetName() :
			"NONE",
			Util::HandleIsValid(a_p->tm->selectedTargetActorHandle) ?
			a_p->tm->selectedTargetActorHandle.get()->GetName() :
			"NONE",
			Util::HandleIsValid(a_p->tm->aimTargetLinkedRefrHandle) ?
			a_p->tm->aimTargetLinkedRefrHandle.get()->GetName() :
			"NONE",
			Util::HandleIsValid(a_p->tm->crosshairRefrHandle) ?
			a_p->tm->crosshairRefrHandle.get()->GetName() :
			"NONE",
			!trajType,
			a_setStraightTrajectory,
			predictInterceptPos,
			Util::HandleIsValid(targetRefrHandle) ? 
			targetRefrHandle.get()->GetName() : 
			"NONE",
			initialTrajTimeToTarget
		);
	}

	void TargetingManager::ManagedProjectileHandler::Insert
	(
		const std::shared_ptr<CoopPlayer>& a_p, 
		const RE::ObjectRefHandle& a_projectileHandle, 
		RE::NiPoint3& a_initialVelocityOut, 
		const ProjectileTrajType& a_trajType
	)
	{
		// Insert the given projectile into the managed list.
		// Then set its trajectory information 
		// and update its initial velocity through the outparam.

		// Housekeeping first.
		// Keep the managed projectiles map at a modest size by removing expired projectiles
		// if the queue size is above a certain threshold.
		if (managedProjHandleToTrajInfoMap.size() >= 
			Settings::uManagedPlayerProjectilesBeforeRemoval)
		{
			RE::Projectile* projectile = nullptr;
			RE::TESObjectREFRPtr projectilePtr{ };
			for (const auto& [handle, _] : managedProjHandleToTrajInfoMap) 
			{
				projectilePtr = Util::GetRefrPtrFromHandle(handle);
				if (projectilePtr)
				{
					projectile = projectilePtr->As<RE::Projectile>();
				}

				// Remove if:
				// Invalid, not loaded, deleted, marked for deletion, 
				// has collided (if not a beam or flames), or limited.
				bool shouldRemove = 
				{
					(!projectile) ||
					(!projectile->Is3DLoaded()) ||
					(projectile->IsDeleted()) ||
					(projectile->IsMarkedForDeletion()) ||
					(
						!projectile->As<RE::BeamProjectile>() && 
						!projectile->As<RE::FlameProjectile>() && 
						!projectile->impacts.empty()
					) ||
					(projectile->ShouldBeLimited())
				};
				if (shouldRemove) 
				{
					managedProjHandleToTrajInfoMap.erase(handle);
				}
			}
		}

		auto projectilePtr = Util::GetRefrPtrFromHandle(a_projectileHandle);
		// Smart ptr was invalid, so its managed projectile is as well.
		if (!projectilePtr)
		{
			return;
		}

		// Insert constructed trajectory info for this projectile.
		// NOTE: 
		// Construction sets all the trajectory data automatically.
		managedProjHandleToTrajInfoMap.insert_or_assign
		(
			a_projectileHandle, 
			std::make_unique<ManagedProjTrajectoryInfo>
			(
				a_p, a_projectileHandle, a_initialVelocityOut, a_trajType
			)
		);
	}
}
